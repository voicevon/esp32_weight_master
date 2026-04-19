#include "system/SystemKernel.h"
#include <lvgl.h>
#include "system/SystemContext.h"
#include "drivers/ModbusMaster.h"
#include "logic/NodeManager.h"
#include "drivers/Belt.h"
#include "apps/AppProduction.h"
#include "apps/AppScan.h"
#include "apps/AppServoTest.h"
#include "apps/AppBeltDiag.h"
#include "apps/AppModbusDiag.h"
#include "apps/AppShift.h"

SystemKernel::SystemKernel(SystemContext* ctx, ModbusMaster* rs485, NodeManager* nodeMgr, UIManager* ui, Belt* b1, Belt* b2)
    : _ctx(ctx), _rs485(rs485), _nodeMgr(nodeMgr), _ui(ui), _b1(b1), _b2(b2) {
    _mutexCtx = xSemaphoreCreateMutex();
}

void SystemKernel::registerApp(IApp* app) {
    _apps.push_back(app);
}

void SystemKernel::begin(OperationMode initialMode) {
    _ui->setCommandBus(this);
    _nodeMgr->setCommandBus(this);

    _rs485->begin();
    _nodeMgr->begin();
    
    // 初始化系统上下文
    xSemaphoreTake(_mutexCtx, portMAX_DELAY);
    _ctx->prog.sysStatus = SYS_READY;
    strncpy(_ctx->prog.statusText, "库内核就绪", 32); 
    _ctx->ui.curMode = initialMode;
    xSemaphoreGive(_mutexCtx);

    // 启动 FreeRTOS 任务
    xTaskCreatePinnedToCore(controlTaskEntry, "ControlTask", 8192, this, 10, NULL, 1);
    xTaskCreatePinnedToCore(uiTaskEntry,      "UITask",      8192, this,  5, NULL, 0);

    delay(100);
    updateOperationMode(initialMode);
}

// =============================================================================
// ICommandBus 实现 (路由到具体 App)
// =============================================================================

void SystemKernel::cmdGlobalTare() {
    auto app = findApp(MODE_PRODUCTION);
    if (app) {
        static_cast<AppProduction*>(app)->triggerGlobalTare();
    }
}

void SystemKernel::cmdGlobalServo(bool open) {
    auto app = findApp(MODE_SERVO_TEST);
    if (app) {
        static_cast<AppServoTest*>(app)->triggerGlobalServo(open);
    }
}

void SystemKernel::cmdStartScan() {
    updateOperationMode(MODE_DIAG_SCAN);
}

void SystemKernel::cmdCancelScan() {
    if (_currentMode == MODE_DIAG_SCAN && _currentApp) {
        Serial.println("[Kernel] Cancel Scan REQUESTED.");
        _currentApp->requestCancel();
    }
}

void SystemKernel::cmdToggleDiagnosis(bool active) {
    // 仅透传业务逻辑状态，不再干预模式切换
}

void SystemKernel::cmdServoTest(int id, bool open) {
    bool success = _rs485->syncWrite(id, REG_CMD_CONTROL, open ? CMD_SERVO_OPEN : CMD_SERVO_CLOSE);
    _nodeMgr->setServoState(id, open);
    // 单点动作：更新本地的UI记录并触发颜色刷新
    _ctx->ui.servoRealStates[id] = success ? (open ? 1 : 0) : -1;
    _ctx->prog.dirtyFlags |= DF_NODE_DATA;
}

void SystemKernel::cmdBeltTest(int beltIndex, int distanceMm) {
    if (beltIndex != 0 && beltIndex != 1) return;

    auto app = findApp(MODE_BELT_DIAG);
    if (app && _currentMode == MODE_BELT_DIAG) {
        static_cast<AppBeltDiag*>(app)->triggerRun(beltIndex, distanceMm);
    } else {
        Belt* target = (beltIndex == 0) ? _b1 : _b2;
        if (target) target->moveDistanceMm(distanceMm);
    }
}

void SystemKernel::cmdBeltRun(int beltIndex, bool run) {
    if (beltIndex != 0 && beltIndex != 1) return;

    auto app = findApp(MODE_BELT_DIAG);
    if (app && _currentMode == MODE_BELT_DIAG) {
        static_cast<AppBeltDiag*>(app)->triggerRunToggle(beltIndex, run);
    } else {
        Belt* target = (beltIndex == 0) ? _b1 : _b2;
        if (target) {
            if (run) target->speedRun();
            else target->speedStop();
        }
    }
}

void SystemKernel::cmdTriggerBeltScan() {
    auto app = findApp(MODE_BELT_DIAG);
    if (app && _currentMode == MODE_BELT_DIAG) {
        static_cast<AppBeltDiag*>(app)->triggerScan();
    }
}

void SystemKernel::cmdSerialSendHex(const char* hexStr) {
    IApp* app = findApp(MODE_MODBUS_DIAG);
    if (app) ((AppModbusDiag*)app)->triggerSendHex(hexStr);
}

void SystemKernel::cmdSerialToggleAuto(bool enable) {
    IApp* app = findApp(MODE_MODBUS_DIAG);
    if (app) ((AppModbusDiag*)app)->toggleAutoSend(enable);
}

void SystemKernel::cmdSetDiagSubMode(int mode) {
    IApp* app = findApp(MODE_MODBUS_DIAG);
    if (app) ((AppModbusDiag*)app)->setSubMode((DiagSubMode)mode);
}

void SystemKernel::cmdSetDiagTarget(int id) {
    IApp* app = findApp(MODE_MODBUS_DIAG);
    if (app) ((AppModbusDiag*)app)->setTargetId(id);
}

void SystemKernel::cmdDiagAction(int actionId) {
    if (actionId == 10 || actionId == 11) {
        IApp* app = findApp(MODE_SHIFT_MANAGEMENT);
        if (app) {
            if (actionId == 10) static_cast<AppShift*>(app)->triggerStartShift();
            else static_cast<AppShift*>(app)->triggerEndShift();
        }
        return;
    }
    IApp* app = findApp(MODE_MODBUS_DIAG);
    if (app) ((AppModbusDiag*)app)->triggerAction(actionId);
}

void SystemKernel::cmdUpdateTargetBase(float delta) {
    auto app = findApp(MODE_PRODUCTION);
    if (app) static_cast<AppProduction*>(app)->updateTargets(delta, 0);
}

void SystemKernel::cmdUpdateTargetOffset(float delta) {
    auto app = findApp(MODE_PRODUCTION);
    if (app) static_cast<AppProduction*>(app)->updateTargets(0, delta);
}

void SystemKernel::cmdUpdateTargets(float dMin, float dMax) {
    auto app = findApp(MODE_PRODUCTION);
    if (app) static_cast<AppProduction*>(app)->updateTargets(dMin, dMax);
}

// =============================================================================
// 模式管理
// =============================================================================

void SystemKernel::updateOperationMode(OperationMode newMode) {
    if (_pendingMode == newMode && _currentMode == newMode) return;
    Serial.printf("[Kernel] Mode Switch REQUESTED: %s\n", modeToStr(newMode));
    _pendingMode = newMode;
    // 不在此处直接设置脏标记，而是在 executeModeSwitch 成功切换后设置
}

void SystemKernel::executeModeSwitch() {
    if (_pendingMode == _currentMode) return;

    Serial.printf("[Kernel] ATOMIC SWITCH: %s -> %s\n",
                  modeToStr(_currentMode),
                  modeToStr(_pendingMode));

    if (_currentApp) _currentApp->onExit();

    _currentMode = _pendingMode;
    
    xSemaphoreTake(_mutexCtx, portMAX_DELAY);
    _ctx->ui.curMode = _currentMode;
    _ctx->prog.dirtyFlags |= DF_OP_MODE; // 设置运行模式脏标记
    xSemaphoreGive(_mutexCtx);

    _currentApp = findApp(_currentMode);
    if (_currentApp) _currentApp->onEnter();
}

bool SystemKernel::canSwitchMode() const {
    return (_rs485->getStatus() != ModbusMaster::ST_WAITING);
}

IApp* SystemKernel::findApp(OperationMode mode) {
    for (auto app : _apps) {
        if (app->getMode() == mode) return app;
    }
    return nullptr;
}

// =============================================================================
// 任务循环
// =============================================================================

void SystemKernel::controlTaskEntry(void* self) {
    static_cast<SystemKernel*>(self)->controlLoop();
}

void SystemKernel::controlLoop() {
    Serial.println("[Kernel] Control Task Started on Core 1");
    while (true) {
        if (_pendingMode != _currentMode && canSwitchMode()) {
            executeModeSwitch();
        }

        // 驱动皮带异步任务队列 (提高到 App 逻辑之上，确保控制指令优先于轮询指令)
        if (_b1) _b1->update();
        if (_b2) _b2->update();

        if (_currentApp) {
            _currentApp->onLoop();
            if (_currentMode != MODE_PRODUCTION && _currentApp->isFinished()) {
                updateOperationMode(MODE_PRODUCTION);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void SystemKernel::uiTaskEntry(void* self) {
    static_cast<SystemKernel*>(self)->uiLoop();
}

void SystemKernel::uiLoop() {
    Serial.println("[Kernel] UI Task Started on Core 0");
    while (true) {
        // [脏标记同步逻辑]
        xSemaphoreTake(_mutexCtx, portMAX_DELAY);
        _nodeMgr->fillUISnapshot(_ctx->ui);
        
        // 捕获并在 UI 层累加脏标记
        _ctx->ui.dirtyFlags |= _ctx->prog.dirtyFlags;
        _ctx->prog.dirtyFlags = DF_NONE; // 重置逻辑层脏标记

        // 总是保持实时数据的脏标记 (Suggestion 3 探讨：高频模拟量保持刷新)
        _ctx->ui.dirtyFlags |= DF_LIVE_DATA; 

        if (_currentApp) {
            bool tareActive = _currentApp->hasUIProgress();
            if (tareActive != _ctx->ui.isTareRunning) {
                _ctx->ui.isTareRunning = tareActive;
                _ctx->ui.dirtyFlags |= DF_PROGRESS;
            }
            _ctx->ui.tareProgress = _currentApp->getUIProgress();

            if (_currentMode == MODE_BELT_DIAG) {
                _ctx->ui.beltDiagScanning = static_cast<AppBeltDiag*>(_currentApp)->isScanning();
                _ctx->ui.beltStatus[0] = static_cast<AppBeltDiag*>(_currentApp)->getBeltStatus(0);
                _ctx->ui.beltStatus[1] = static_cast<AppBeltDiag*>(_currentApp)->getBeltStatus(1);
            } else if (_currentMode == MODE_MODBUS_DIAG) {
                AppModbusDiag* diag = static_cast<AppModbusDiag*>(_currentApp);
                _ctx->ui.diagSubMode = diag->getSubMode();
                _ctx->ui.diagTargetNodeId = diag->getTargetId();
                
                _ctx->ui.serialAutoSend = _ctx->prog.diagAutoSend;
                _ctx->ui.serialLogTick  = _ctx->prog.diagLogTick;
                strncpy(_ctx->ui.serialLogLine, _ctx->prog.diagLogLine, sizeof(_ctx->ui.serialLogLine));
            }
        }

        // --- [核心修复] 全局扫描快照同步 (不受 currentMode 阻塞) ---
        // 即使模式切换了，只要 App 实例还在内存中，我们就最后一次同步数据，避免最后几个节点变红
        IApp* scanAppPtr = findApp(MODE_DIAG_SCAN);
        if (scanAppPtr) {
            AppScan* scanApp = static_cast<AppScan*>(scanAppPtr);
            _ctx->ui.scanCycle = scanApp->getScanCycle();
            _ctx->ui.scanProgress = scanApp->getScanProgress();
            for (int c = 0; c < 5; c++) {
                for (int i = 1; i <= 20; i++) {
                    _ctx->ui.scanResults[c][i] = scanApp->getScanResult(c, i);
                }
            }
        }

        
        _ctx->ui.lastCalcSuccess = _ctx->prog.lastCalcSuccess;
        memcpy(_ctx->ui.lastBatchWeights, _ctx->prog.lastBatchWeights, sizeof(_ctx->ui.lastBatchWeights));

        xSemaphoreGive(_mutexCtx);

        // UI 渲染 (传入带脏标记的上下文)
        _ui->updateDashboard(_ctx);
        
        // 完成本轮渲染后，由于 UISnapshot 是 Dispatcher 所有的临时副本（或 UI 任务持久化）
        // 我们需要确保 UIManager 内部能够处理完标志位。
        // 或者：直接在此处手动清除 UI Snap 脏标记（因为 UIManager 已经同步执行完）
        _ctx->ui.dirtyFlags = DF_NONE;

        lv_tick_inc(33);
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(33));
    }
}

const char* SystemKernel::modeToStr(OperationMode m) {
    switch (m) {
        case MODE_IDLE:            return "IDLE";
        case MODE_PRODUCTION:      return "PRODUCTION";
        case MODE_DIAG_SCAN:       return "DIAG_SCAN";
        case MODE_DIAG_DETAIL:     return "DIAG_DETAIL";
        case MODE_CONFIGURATION:   return "CONFIGURATION";
        case MODE_SERVO_TEST:      return "SERVO_TEST";
        case MODE_BELT_DIAG:       return "BELT_DIAG";
        case MODE_MODBUS_DIAG:     return "MODBUS_DIAG";
        case MODE_ABOUT:           return "ABOUT";
        case MODE_SHIFT_MANAGEMENT: return "SHIFT_MGMT";
        default:                   return "UNKNOWN";
    }
}
