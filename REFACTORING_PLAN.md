# ESP32 Weight Master — 固件重构计划书

> **版本**: v1.0
> **日期**: 2026-04-03
> **范围**: `d:/Software/antigravity/esp32_weight_master/src/`
> **原则**: 只改结构，不改功能；每个 Phase 独立可编译可验证。

---

## 一、现状架构问题总览

```
┌─────────────────────────────────────────────────────────┐
│                        main.cpp                         │
│  全局变量 × 13 │ mutex × 3 │ FreeRTOS Task × 2          │
│  生产业务逻辑  │ 命令函数  │ updateOperationMode()       │
└──────────┬───────────────────────┬──────────────────────┘
           │extern 访问             │直接调用
      PollManager              UIManager
      ┌──────────┐          ┌────────────────────┐
      │static cb │          │extern void cmdXxx()│
      │extern PM │          │→ 调 main.cpp 业务  │
      └──────────┘          └────────────────────┘
           │
      ModbusMaster ←── ConveyorController (直接指针)
```

---

## 二、问题清单（7 项）

### P1 — `main.cpp` 上帝文件

| 维度 | 现状 |
|------|------|
| 全局变量 | `slaveWeights`, `slaveStable`, `systemStatus`, `lastCombinedWeight`, `currentSelectedMask`, `accumulatedTotalWeight`, `isProductionActive` … 共 13+ 个裸变量 |
| 任务定义 | `controlTask` 内嵌完整的组合引擎调度、传送带控制、权重同步全部逻辑（~130 行） |
| 命令函数 | `cmdGlobalTare`, `cmdStartScan`, `cmdToggleDiagnosis`, `cmdClearAccumulated`, `cmdUpdateTargets` 5 个孤立函数散落文件末尾 |
| 模式管理 | `updateOperationMode()` 是裸全局函数，无法测试、无法 mock |

**危害**：任何新功能或 bug fix 都必须改 `main.cpp`，极易引入回归。

---

### P2 — `PollManager::onPollComplete` 的 `extern` 单例依赖

```cpp
// PollManager.cpp 第 71 行
extern PollManager pollManager;   // ← 硬编码依赖全局实例名
```

**危害**：
- `PollManager` 类无法实例化多个（例如从机调试场景）
- 单测无法构造独立实例
- 全局名必须与 `main.cpp` 的变量名严格匹配，重命名即崩溃

---

### P3 — 三把 mutex 裸同步，中间缓冲区冗余

```cpp
SemaphoreHandle_t mutexParams;   // 保护 targetMin/Max, accumulatedWeight
SemaphoreHandle_t mutexWeights;  // 保护 slaveWeights[], slaveStable[]
SemaphoreHandle_t mutexStatus;   // 保护 systemStatus, curMode, scanProgress...
```

`slaveWeights[]` / `slaveStable[]` 是 `PollManager._nodes[]` 的手工拷贝，`uiTask` 在做二次拷贝：

```
PollManager._nodes → [slaveWeights] → globalCtx.state → UI   (3 层副本)
```

**危害**：三份重量数据共存，任意一层更新滞后都会导致数据不一致；锁粒度过大。

---

### P4 — `SystemContext` 上帝对象

```cpp
struct RuntimeState {
    OperationMode curMode;      // 模式控制 (生产/诊断)
    SystemStatus status;        // 生产业务状态
    float currentWeights[20];   // 称重数据
    bool stableNodes[20];
    float lastBatchWeight;      // 生产结果
    int scanProgress;           // 扫描诊断状态
    bool scanResults[5][20];    // 5 轮扫描历史
    uint8_t diagLastSent;       // 链路诊断状态
    char diagRxHex[128];
};
```

**生产数据**、**扫描诊断**、**链路诊断** 三种职责混合，每次 uiTask 同步时整体加锁拷贝。

---

### P5 — `UIManager` 通过 `extern` 命令函数耦合 main scope

```cpp
// UIManager.h
extern void cmdGlobalTare();
extern void cmdClearAccumulated();
extern void cmdUpdateTargets(float, float);
extern void cmdStartScan();
extern void cmdGenerateWhitelist();   // ← 函数在 main.cpp 中根本不存在！
```

**危害**：`UIManager` 与 `main.cpp` 强耦合；`cmdGenerateWhitelist` 是悬空声明（链接期隐患）。

---

### P6 — `ConveyorController` 位置值截断溢出

```cpp
_currentPos1 += 30000;  // int 累加，无上限
_rs485->syncWrite(_id1, 0x0200, (uint16_t)_currentPos1);  // 超 65535 即截断，伺服突变
```

---

### P7 — `CombinationEngine` 返回索引约定歧义

`selectedIndices` 注释"1-based IDs"，但调用方当 0-based 下标使用并用 `-1` 修正：

```cpp
// main.cpp 第 160 行
int physicalId = activeIds[idx_in_active - 1];  // -1 是语义补丁，极端情况为 UB
```

---

## 三、重构目标架构

```
┌──────────────────────────────────────────────────────┐
│              main.cpp  (精简 ≤60 行)                  │
│  对象实例化 + setup() + loop()                       │
└─────────────────────┬────────────────────────────────┘
                       │
         ┌─────────────▼──────────────┐
         │       AppController        │  [NEW]
         │  拥有所有 FreeRTOS Task    │
         │  实现 ICommandBus          │
         └──┬──────────┬──────────────┘
            │          │
   ┌─────────▼──┐  ┌───▼──────────────────┐
   │ controlLoop │  │ UIManager            │
   │ (生产调度) │  │ ICommandBus 接口调用 │
   └────────────┘  └──────────────────────┘
            │
   ┌─────────▼──────────────────────────┐
   │         PollManager                │
   │  static _instance（消除 extern）   │
   └─────────────────────────────────────┘
            │
   ┌─────────▼──────────┐
   │    ModbusMaster     │  (纯驱动，不变)
   └─────────────────────┘
```

---

## 四、分阶段计划

---

### Phase 1 — 拆分 `main.cpp`：引入 `AppController`

**目标**：`main.cpp` 只做对象创建和启动，所有运行逻辑迁移至 `AppController`。

#### [NEW] `src/AppController.h` + `src/AppController.cpp`

```cpp
class AppController {
public:
    AppController(ModbusMaster*, PollManager*, CombinationEngine*,
                  ConveyorController*, UIManager*);
    void begin();

    // 命令接口（供 UIManager 回调，Phase 3 后改为纯虚继承）
    void cmdGlobalTare();
    void cmdStartScan();
    void cmdClearAccumulated();
    void cmdUpdateTargets(float dMin, float dMax);
    void cmdToggleDiagnosis(bool active);
    void updateOperationMode(OperationMode newMode);

private:
    static void controlTaskEntry(void* self);
    static void uiTaskEntry(void* self);
    void controlLoop();   // 原 controlTask 逻辑
    void uiLoop();        // 原 uiTask 逻辑
    // 所有 mutex + 业务变量迁移至此
};
```

#### [MODIFY] `src/main.cpp`

只保留对象实例化 + `setup()` + `loop()`，目标 ≤ 60 行。

**验收**：`main.cpp` ≤ 60 行，编译通过，功能零回归。

---

### Phase 2 — 消除 `PollManager` 的 `extern` 单例依赖

**目标**：`onPollComplete` 改用 `static _instance` 指针，不再依赖全局变量名。

```cpp
// 现状
static bool onPollComplete(...) {
    extern PollManager pollManager;   // 硬编码
}
// 目标
static bool onPollComplete(...) {
    if (!_instance) return false;
    PollManager* pm = _instance;      // static 指针
}
```

#### [MODIFY] `src/system/PollManager.h`
- 添加 `static PollManager* _instance;`

#### [MODIFY] `src/system/PollManager.cpp`
- 构造函数：`_instance = this`
- `onPollComplete`：改用 `_instance`

> **注意**：`_instance` 仍是软单例。多实例场景需将 `this` 通过 asyncRead context 传递，本 Phase 不涉及。

**验收**：`PollManager.cpp` 不含 `extern` 关键字。

---

### Phase 3 — 引入 `ICommandBus` 解耦 `UIManager` 与业务层

**目标**：`UIManager` 不再通过 `extern` 函数直接耦合 `main.cpp`。

#### [NEW] `src/ICommandBus.h`

```cpp
class ICommandBus {
public:
    virtual ~ICommandBus() = default;
    virtual void cmdGlobalTare() = 0;
    virtual void cmdStartScan() = 0;
    virtual void cmdClearAccumulated() = 0;
    virtual void cmdUpdateTargets(float dMin, float dMax) = 0;
    virtual void cmdToggleDiagnosis(bool active) = 0;
};
```

#### [MODIFY] `src/UIManager.h`

- 删除所有 `extern void cmdXxx()` 声明（含悬空的 `cmdGenerateWhitelist`）
- 新增 `void setCommandBus(ICommandBus* bus)`

#### [MODIFY] `src/UIManager.cpp`

- 所有按钮回调改为 `_bus->cmdXxx()`

#### [MODIFY] `src/AppController.h`

- 继承 `ICommandBus`，`begin()` 中调用 `ui->setCommandBus(this)`

**验收**：`UIManager.h` 无 `extern` 声明，编译无 "undefined reference"。

---

### Phase 4 — 分拆 `SystemContext`，削减 mutex 粒度

**目标**：三种数据域独立封装；消除 `slaveWeights[]` / `slaveStable[]` 冗余中间层。

#### [NEW] `src/system/ProductionContext.h`

```cpp
struct ProductionContext {
    // 锁：mutexProduction（合并原 mutexParams + mutexWeights）
    float    targetMin, targetMax;
    float    accumulatedWeight;
    bool     isProductionEnabled;
    float    lastBatchWeight;
    uint32_t selectionMask;
    SystemStatus status;
};
```

#### [NEW] `src/system/DiagContext.h`

```cpp
struct DiagContext {
    // 锁：mutexDiag（原 mutexStatus 中诊断部分）
    uint8_t diagLastSent;
    char    diagRxHex[128];
    int     scanProgress;
    int     currentScanCycle;
    bool    scanResults[5][20];
};
```

#### UISnapshot（uiTask 专用，无锁）

```cpp
struct UISnapshot {
    OperationMode curMode;
    float  currentWeights[20];   // 直接从 pollManager->getWeight(i) 读取
    bool   stableNodes[20];
    bool   onlineNodes[20];
    bool   whitelistedNodes[20];
};
```

**数据流对比**：

```
重构前：PollManager._nodes → slaveWeights[] → globalCtx.state → UI    (3 层拷贝)
重构后：PollManager ────────────────────────────── UISnapshot           (1 次查询)
```

#### [MODIFY] `src/AppController.cpp`

- mutex 从 3 个缩减为 2 个：`mutexProduction` + `mutexDiag`
- 删除 `slaveWeights[]`、`slaveStable[]`

**验收**：mutex 数量 ≤ 2，`AppController.cpp` 无 `slaveWeights`，UI 刷新无回归。

---

### Phase 5 — 修复 `CombinationEngine` 索引约定 + `ConveyorController` 溢出

#### Phase 5a — 统一 selectedIndices 为 0-based

| 文件 | 改动 |
|------|------|
| `src/logic/CombinationEngine.h` | 注释改为"0-based index into the input `weights` vector" |
| `src/logic/CombinationEngine.cpp` | `push_back(nodes[node_idx].index + 1)` → `push_back(nodes[node_idx].index)` |
| `src/AppController.cpp` | `activeIds[idx_in_active - 1]` → `activeIds[idx_in_active]` |

#### Phase 5b — ConveyorController 防溢出

| 文件 | 改动 |
|------|------|
| `src/logic/ConveyorController.h` | `_currentPos1/2` 改为 `uint32_t` |
| `src/logic/ConveyorController.cpp` | 写入时 `% 65536` 取模保护 |

---

## 五、执行优先级

| 优先级 | Phase | 改动量 | 风险 | 建议时机 |
|--------|-------|--------|------|---------|
| ★★★ | P2 消除 extern 单例 | 小 | 低 | 立即 |
| ★★★ | P5a 索引约定修复 | 小 | 低 | 立即 |
| ★★☆ | P1 引入 AppController | 大 | 中 | 功能冻结后 |
| ★★☆ | P3 引入 ICommandBus | 中 | 低 | P1 之后 |
| ★☆☆ | P4 分拆 SystemContext | 中 | 中 | P1+P3 之后 |
| ★☆☆ | P5b ConveyorController 溢出 | 小 | 低 | 按需 |

---

## 六、范围外事项

| 事项 | 理由 |
|------|------|
| `UIManager.cpp` LVGL 组件细节 | 不涉及架构耦合 |
| `ModbusMaster` 协议栈内部 | 驱动层已稳定 |
| `CombinationEngine` 算法优化 | 贪婪+置换策略功能正确 |
| 从机固件 | 超出项目范围 |

---

## 七、验收总 Checklist

- [x] `main.cpp` 行数 ≤ 60 行
- [x] `PollManager.cpp` 不含 `extern` 关键字
- [x] `UIManager.h` 不含 `extern void cmd` 声明
- [x] mutex 数量从 3 减至 2
- [x] `slaveWeights[]` 冗余中间层不存在
- [x] `CombinationEngine::selectedIndices` 统一为 0-based，调用方无 `-1` hack
- [x] `ConveyorController` 溢出保护已添加 (Phase 5b)
- [ ] 编译零警告（`-Wall`）
- [ ] 硬件验证：生产模式、扫描模式、诊断模式三场景功能正常
