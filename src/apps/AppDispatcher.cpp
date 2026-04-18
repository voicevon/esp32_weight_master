#include "apps/AppDispatcher.h"
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
#include "apps/AppSequentialCtrl.h"

AppDispatcher::AppDispatcher(SystemContext* ctx, ModbusMaster* rs485, NodeManager* pollMgr, UIManager* ui, Belt* b1, Belt* b2)
    : _ctx(ctx), _rs485(rs485), _pollMgr(pollMgr), _ui(ui), _b1(b1), _b2(b2) {
    _mutexCtx = xSemaphoreCreateMutex();
}

void AppDispatcher::registerApp(IApp* app) {
    _apps.push_back(app);
}

void AppDispatcher::begin(OperationMode initialMode) {
    _ui->setCommandBus(this);
    _pollMgr->setCommandBus(this);

    _rs485->begin();
    _pollMgr->begin();
    
    // 初始化系统上下文
    xSemaphoreTake(_mutexCtx, portMAX_DELAY);
    _ctx->prog.sysStatus = SYS_READY;
    strncpy(_ctx->prog.statusText, "就绪", 32); 
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

void AppDispatcher::cmdGlobalTare() {
    auto app = findApp(MODE_SEQUENTIAL_CTRL);
    if (app) {
        static_cast<AppSequentialCtrl*>(app)->triggerGlobalTare();
        updateOperationMode(MODE_SEQUENTIAL_CTRL);
    }
}

void AppDispatcher::cmdGlobalServo(bool open) {
    auto app = findApp(MODE_SEQUENTIAL_CTRL);
    if (app) {
        static_cast<AppSequentialCtrl*>(app)->triggerGlobalServo(open);
        updateOperationMode(MODE_SEQUENTIAL_CTRL);
    }
}

void AppDispatcher::cmdStartScan() {
    updateOperationMode(MODE_DIAG_SCAN);
}

void AppDispatcher::cmdToggleDiagnosis(bool active) {
    // 仅透传业务逻辑状态，不再干预模式切换
}

void AppDispatcher::cmdServoTest(int id, bool open) {
    _rs485->syncWrite(id, REG_CMD_CONTROL, open ? CMD_SERVO_OPEN : CMD_SERVO_CLOSE);
    _pollMgr->setServoState(id, open);
}

void AppDispatcher::cmdBeltTest(int beltIndex, int distanceMm) {
    if (beltIndex != 0 && beltIndex != 1) return;

    auto app = findApp(MODE_BELT_DIAG);
    if (app && _currentMode == MODE_BELT_DIAG) {
        static_cast<AppBeltDiag*>(app)->triggerRun(beltIndex, distanceMm);
    } else {
        Belt* target = (beltIndex == 0) ? _b1 : _b2;
        if (target) target->moveDistanceMm(distanceMm);
    }
}

void AppDispatcher::cmdBeltRun(int beltIndex, bool run) {
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

void AppDispatcher::cmdTriggerBeltScan() {
    auto app = findApp(MODE_BELT_DIAG);
    if (app && _currentMode == MODE_BELT_DIAG) {
        static_cast<AppBeltDiag*>(app)->triggerScan();
    }
}

void AppDispatcher::cmdSerialSendHex(const char* hexStr) {
    IApp* app = findApp(MODE_MODBUS_DIAG);
    if (app) ((AppModbusDiag*)app)->triggerSendHex(hexStr);
}

void AppDispatcher::cmdSerialToggleAuto(bool enable) {
    IApp* app = findApp(MODE_MODBUS_DIAG);
    if (app) ((AppModbusDiag*)app)->toggleAutoSend(enable);
}

void AppDispatcher::cmdSetDiagSubMode(int mode) {
    IApp* app = findApp(MODE_MODBUS_DIAG);
    if (app) ((AppModbusDiag*)app)->setSubMode((DiagSubMode)mode);
}

void AppDispatcher::cmdSetDiagTarget(int id) {
    IApp* app = findApp(MODE_MODBUS_DIAG);
    if (app) ((AppModbusDiag*)app)->setTargetId(id);
}

void AppDispatcher::cmdDiagAction(int actionId) {
    IApp* app = findApp(MODE_MODBUS_DIAG);
    if (app) ((AppModbusDiag*)app)->triggerAction(actionId);
}

void AppDispatcher::cmdUpdateTargetBase(float delta) {
    auto app = findApp(MODE_PRODUCTION);
    if (app) static_cast<AppProduction*>(app)->updateTargets(delta, 0);
}

void AppDispatcher::cmdUpdateTargetOffset(float delta) {
    auto app = findApp(MODE_PRODUCTION);
    if (app) static_cast<AppProduction*>(app)->updateTargets(0, delta);
}

void AppDispatcher::cmdUpdateTargets(float dMin, float dMax) {
    auto app = findApp(MODE_PRODUCTION);
    if (app) static_cast<AppProduction*>(app)->updateTargets(dMin, dMax);
}

// =============================================================================
// 模式管理
// =============================================================================

void AppDispatcher::updateOperationMode(OperationMode newMode) {
    if (_pendingMode == newMode && _currentMode == newMode) return;
    Serial.printf("[Dispatcher] Mode Switch REQUESTED: %s\n", modeToStr(newMode));
    _pendingMode = newMode;
}

void AppDispatcher::executeModeSwitch() {
    if (_pendingMode == _currentMode) return;

    Serial.printf("[Dispatcher] ATOMIC SWITCH: %s -> %s\n",
                  modeToStr(_currentMode),
                  modeToStr(_pendingMode));

    if (_currentApp) _currentApp->onExit();

    _currentMode = _pendingMode;
    
    xSemaphoreTake(_mutexCtx, portMAX_DELAY);
    _ctx->ui.curMode = _currentMode;
    xSemaphoreGive(_mutexCtx);

    _currentApp = findApp(_currentMode);
    if (_currentApp) _currentApp->onEnter();
}

bool AppDispatcher::canSwitchMode() const {
    return (_rs485->getStatus() != ModbusMaster::ST_WAITING);
}

IApp* AppDispatcher::findApp(OperationMode mode) {
    for (auto app : _apps) {
        if (app->getMode() == mode) return app;
    }
    return nullptr;
}

// =============================================================================
// 任务循环
// =============================================================================

void AppDispatcher::controlTaskEntry(void* self) {
    static_cast<AppDispatcher*>(self)->controlLoop();
}

void AppDispatcher::controlLoop() {
    Serial.println("[Dispatcher] Control Task Started on Core 1");
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

void AppDispatcher::uiTaskEntry(void* self) {
    static_cast<AppDispatcher*>(self)->uiLoop();
}

void AppDispatcher::uiLoop() {
    Serial.println("[Dispatcher] UI Task Started on Core 0");
    while (true) {
        _pollMgr->fillUISnapshot(_ctx->ui);
        
        if (_currentApp) {
            _ctx->ui.isTareRunning = _currentApp->hasUIProgress();
            _ctx->ui.tareProgress  = _currentApp->getUIProgress();

            // 专门处理扫描数据的同步 (从 App 层拉取到 UI 快照层)
            if (_currentMode == MODE_DIAG_SCAN) {
                AppScan* scanApp = static_cast<AppScan*>(_currentApp);
                _ctx->ui.scanCycle = scanApp->getScanCycle();
                _ctx->ui.scanProgress = scanApp->getScanProgress();
                for (int c = 0; c < 5; c++) {
                    for (int i = 1; i <= 20; i++) {
                        _ctx->ui.scanResults[c][i] = scanApp->getScanResult(c, i);
                    }
                }
            } else if (_currentMode == MODE_BELT_DIAG) {
                _ctx->ui.beltDiagScanning = static_cast<AppBeltDiag*>(_currentApp)->isScanning();
                _ctx->ui.beltStatus[0] = static_cast<AppBeltDiag*>(_currentApp)->getBeltStatus(0);
                _ctx->ui.beltStatus[1] = static_cast<AppBeltDiag*>(_currentApp)->getBeltStatus(1);
            } else if (_currentMode == MODE_MODBUS_DIAG) {
                AppModbusDiag* diag = static_cast<AppModbusDiag*>(_currentApp);
                _ctx->ui.diagSubMode = diag->getSubMode();
                _ctx->ui.diagTargetNodeId = diag->getTargetId();
            }
        }
        
        _ctx->ui.lastCalcSuccess = _ctx->prog.lastCalcSuccess;

        _ui->updateDashboard(_ctx);
        
        lv_tick_inc(33);
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(33));
    }
}

const char* AppDispatcher::modeToStr(OperationMode m) {
    switch (m) {
        case MODE_IDLE:            return "IDLE";
        case MODE_PRODUCTION:      return "PRODUCTION";
        case MODE_DIAG_SCAN:       return "DIAG_SCAN";
        case MODE_DIAG_DETAIL:     return "DIAG_DETAIL";
        case MODE_CONFIGURATION:   return "CONFIGURATION";
        case MODE_SEQUENTIAL_CTRL: return "SEQUENTIAL_CTRL";
        case MODE_SERVO_TEST:      return "SERVO_TEST";
        case MODE_BELT_DIAG:       return "BELT_DIAG";
        case MODE_MODBUS_DIAG:     return "MODBUS_DIAG";
        case MODE_ABOUT:           return "ABOUT";
        default:                   return "UNKNOWN";
    }
}
