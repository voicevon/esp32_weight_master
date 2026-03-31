#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <vector>
#include "system/ModbusMaster.h"
#include "logic/CombinationEngine.h"
#include "logic/ConveyorController.h"
#include "user_interface/UserInterface.h"
#include "user_interface/OLEDDisplay.h"
#include "system/PinDefinition.h"
#include "system/SystemTypes.h"
#include "system/SystemConfig.h"
#include "system/SystemContext.h"
#include <Preferences.h>

// ---------------------------
// 共享资源与状态上下文
// ---------------------------
SystemContext globalCtx;
std::vector<float> slaveWeights(NUM_SLAVES, 0.0f);
std::vector<bool> slaveStable(NUM_SLAVES, false);
SystemStatus systemStatus = SYS_INIT;
float lastCombinedWeight = 0.0f;
uint32_t currentSelectedMask = 0; 
float accumulatedTotalWeight = 0.0f;
bool isProductionActive = false;

SemaphoreHandle_t mutexParams; 
SemaphoreHandle_t mutexWeights; 
SemaphoreHandle_t mutexStatus;  

Preferences nvs;

// ---------------------------
// 全局对象
// ---------------------------
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
ModbusMaster rs485(PIN_RS485_RX, PIN_RS485_TX, PIN_RS485_TX_EN, RS485_BAUD);
CombinationEngine engine(290.0f, 310.0f); // 初始值将很快被 setup 中的 NVS 覆盖
ConveyorController conveyor(&rs485, MOTOR_ID_BELT1, MOTOR_ID_BELT2);

// ---------------------------
// 任务函数定义
// ---------------------------

// 核心逻辑任务 (Core 1 - 专业控制)
void controlTask(void* pvParameters) {
    static unsigned long lastCalcTime = 0;
    
    while (true) {
        OperationMode mode = UserInterface::getInstance()->getMode();

        // 1. 根据当前模式决定是否推动 Modbus 轮询 (生产/扫描模式可见总线)
        if (mode == MODE_PRODUCTION || mode == MODE_DIAG_SCAN) {
            rs485.update();
        }

        // 2. 更新本地重量缓存 (待机/生产/查看详情时均更新，确保实时性)
        if (mode == MODE_PRODUCTION || mode == MODE_IDLE || mode == MODE_DIAG_DETAIL) {
            xSemaphoreTake(mutexWeights, portMAX_DELAY);
            for (int i = 0; i < NUM_SLAVES; i++) {
                slaveWeights[i] = rs485.getWeight(i + 1);
                slaveStable[i] = rs485.isStable(i + 1);
            }
            xSemaphoreGive(mutexWeights);
        }

        // 3. 生产逻辑处理
        bool canCalculate = false;
        float currentMin, currentMax;

        xSemaphoreTake(mutexParams, portMAX_DELAY);
        canCalculate = isProductionActive;
        currentMin = globalCtx.config.targetMin;
        currentMax = globalCtx.config.targetMax;
        xSemaphoreGive(mutexParams);

        xSemaphoreTake(mutexStatus, portMAX_DELAY);
        SystemStatus currentStatus = systemStatus;
        xSemaphoreGive(mutexStatus);

        if (canCalculate && currentStatus == SYS_READY) {
            if (millis() - lastCalcTime > CALC_ENGINE_INTERVAL_MS) { 
                lastCalcTime = millis();
                
                engine.setTargetRange(currentMin, currentMax);
                
                // --- 动态映射方案：仅将活跃稳定的节点送入引擎 ---
                std::vector<float> activeWeights;
                std::vector<int> activeIds;
                
                for (int i = 0; i < NUM_SLAVES; i++) {
                    int id = i + 1;
                    if (slaveStable[i] && rs485.getNodeStatus(id) == NODE_STABLE) {
                        activeWeights.push_back(slaveWeights[i]);
                        activeIds.push_back(id);
                    }
                }
                
                int availableCnt = activeWeights.size();
                CombinationResult res = {false, 0.0f, {}};

                // 只要有可用节点就尝试计算 (取消最低限制)
                if (availableCnt > 0) {
                    res = engine.findBestCombination(activeWeights);
                }
                
                // 生产环境诊断日志
                if (res.success) {
                    // 进入落料流程，立即锁定相关节点 (通过映射还原物理 ID)
                    xSemaphoreTake(mutexStatus, portMAX_DELAY);
                    systemStatus = SYS_DISCHARGING;
                    lastCombinedWeight = res.totalWeight;
                    currentSelectedMask = 0;
                    
                    std::vector<int> mappedIds;
                    for (int idx_in_active : res.selectedIndices) {
                        int physicalId = activeIds[idx_in_active - 1];
                        mappedIds.push_back(physicalId);
                        currentSelectedMask |= (1 << (physicalId - 1));
                        rs485.setNodeStatus(physicalId, NODE_LOCKED);
                    }
                    xSemaphoreGive(mutexStatus);

                    Serial.printf("[AUTO] Combination Found: %.1f g, Mask: 0x%08X (Avail: %d)\n", 
                                  res.totalWeight, currentSelectedMask, availableCnt);
                    
                    // 1. 发起脉冲式开门指令
                    for (int id : mappedIds) {
                        rs485.openDischarge1S(id); // 内部会自动设为 NODE_DISCHARGING
                    }
                    
                    // 2. 预留等待物料完全排空并门页复位 (从配置加载延时)
                    vTaskDelay(pdMS_TO_TICKS(DISCHARGE_SETTLE_MS));

                    // 3. 标记数据已过时，强制等待下一次扫描刷新
                    for (int id : mappedIds) {
                        rs485.setNodeStatus(id, NODE_DIRTY);
                    }

                    // 4. 更新累计重量并持久化
                    xSemaphoreTake(mutexParams, portMAX_DELAY);
                    accumulatedTotalWeight += res.totalWeight;
                    globalCtx.config.accumulatedWeight = accumulatedTotalWeight;
                    nvs.begin("production", false);
                    nvs.putFloat("accu", accumulatedTotalWeight);
                    nvs.end();
                    xSemaphoreGive(mutexParams);

                    // 落料结束，清除高亮
                    xSemaphoreTake(mutexStatus, portMAX_DELAY);
                    currentSelectedMask = 0;
                    xSemaphoreGive(mutexStatus);

                    // 级联传输
                    xSemaphoreTake(mutexStatus, portMAX_DELAY);
                    systemStatus = SYS_TRANSFER_B1;
                    xSemaphoreGive(mutexStatus);
                    conveyor.collectFromUnits();
                    vTaskDelay(pdMS_TO_TICKS(BELT_COLLECT_PERIOD_MS));

                    xSemaphoreTake(mutexStatus, portMAX_DELAY);
                    systemStatus = SYS_STEPPING_B2;
                    xSemaphoreGive(mutexStatus);
                    conveyor.advanceOutput();
                    vTaskDelay(pdMS_TO_TICKS(BELT_STEP_PERIOD_MS));

                    xSemaphoreTake(mutexStatus, portMAX_DELAY);
                    systemStatus = SYS_READY;
                    xSemaphoreGive(mutexStatus);
                } else {
                    // 每 2 秒打印一次为何没组合成功 (避免刷屏)
                    static uint32_t lastEngineLog = 0;
                    if (millis() - lastEngineLog > 2000) {
                        Serial.printf("[ENGINE] Standby... Avail Nodes: %d, Target: [%.1f-%.1f]\n", 
                                      availableCnt, currentMin, currentMax);
                        lastEngineLog = millis();
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1)); // 给协议栈留出微量 breathing room
    }
}

// HMI 界面任务 (Core 0 - 渲染与交互)
void uiTask(void* pvParameters) {
    while (true) {
        // 1. 获取并上报参数修改
        xSemaphoreTake(mutexParams, portMAX_DELAY);
        isProductionActive = (UserInterface::getInstance()->getMode() == MODE_PRODUCTION);
        // 如果 UI 修改了 targetMin/Max，这里会被同步（UserInterface 持有指针）
        xSemaphoreGive(mutexParams);

        // 2. 刷新显示 (核心 0 专属数据汇总)
        std::vector<float> localWeights(NUM_SLAVES);
        SystemStatus localStatus;
        float stableSum = 0.0f;
        float unstableSum = 0.0f;
        
        xSemaphoreTake(mutexWeights, portMAX_DELAY);
        localWeights = slaveWeights;
        for (int i = 0; i < NUM_SLAVES; i++) {
            if (slaveStable[i]) stableSum += slaveWeights[i];
            else unstableSum += slaveWeights[i];
        }
        xSemaphoreGive(mutexWeights);

        xSemaphoreTake(mutexStatus, portMAX_DELAY);
        localStatus = systemStatus;
        // 待机或就绪：显示实时稳定总重；下料动作中：显示该批次目标重量
        float displayStable = (localStatus == SYS_READY || localStatus == SYS_INIT) ? stableSum : lastCombinedWeight;
        float totalSum = stableSum + unstableSum;
        float localAccumulatedWeight = accumulatedTotalWeight;
        uint32_t localSelectionMask = currentSelectedMask;
        xSemaphoreGive(mutexStatus);

        UserInterface::getInstance()->update(localWeights, displayStable, unstableSum, totalSum, localAccumulatedWeight, localStatus, localSelectionMask);

        vTaskDelay(pdMS_TO_TICKS(33)); // 约 30FPS 刷新率
    }
}

// ---------------------------
// 主入口
// ---------------------------
void setup() {
    Serial.begin(115200);
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    // 初始化锁
    mutexParams = xSemaphoreCreateMutex();
    mutexWeights = xSemaphoreCreateMutex();
    mutexStatus = xSemaphoreCreateMutex();

    // 1. 持久化数据加载
    nvs.begin("production", true); // 只读模式
    globalCtx.config.targetMin = nvs.getFloat("tmin", 290.0f);
    globalCtx.config.targetMax = nvs.getFloat("tmax", 310.0f);
    globalCtx.config.accumulatedWeight = nvs.getFloat("accu", 0.0f);
    nvs.end();

    accumulatedTotalWeight = globalCtx.config.accumulatedWeight;
    Serial.printf("[SYSTEM] Persistence loaded: Min=%.1f, Max=%.1f, Accu=%.1f\n", 
                   globalCtx.config.targetMin, globalCtx.config.targetMax, accumulatedTotalWeight);

    // 2. 初始化 HMI (Core 0 操作)
    display.setI2CAddress(0x3C * 2);
    if(!display.begin()) {
        Serial.println(F("U8g2 initialization failed"));
    }
    display.enableUTF8Print(); 
    UserInterface::getInstance()->initialize(&globalCtx, &rs485);
    UserInterface::getInstance()->addDisplay(new OLEDDisplay(display));

    // 初始化硬件
    rs485.begin();
    conveyor.begin();
    systemStatus = SYS_READY;

    // 创建多任务 (关键分配)
    xTaskCreatePinnedToCore(
        controlTask,    // 任务函数
        "ControlTask",  // 名称
        8192,           // 栈空间
        NULL,           // 参数
        10,             // 优先级 (高)
        NULL,           // 句柄
        1               // 绑定到核心 1 (主控/通讯)
    );

    xTaskCreatePinnedToCore(
        uiTask,
        "UITask",
        4096,
        NULL,
        5,              // 优先级 (中)
        NULL,
        0               // 绑定到核心 0 (HMI/Rendering)
    );

    Serial.println("Dual-Core Multitasking Started.");
}

void loop() {
    // Arduino Loop 留空或处理最低优先级心跳
    vTaskDelay(pdMS_TO_TICKS(1000));
}
