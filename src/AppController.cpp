#include "AppController.h"
#include <lvgl.h>
#include "system/ModbusMaster.h"
#include "system/PollManager.h"
#include "system/SystemConfig.h"
#include "system/PinDefinition.h"
#include "logic/CombinationEngine.h"
#include "logic/ConveyorController.h"
#include "UIManager.h"
#include "logic/ProductionHandler.h"
#include "system/ScanHandler.h"
#include "system/DiagPulseHandler.h"
#include "system/SequentialCtrlHandler.h"

// =============================================================================
// ICommandBus 实现
// =============================================================================

void AppController::cmdGlobalTare() {
    Serial.println("[CMD] UI Requested Global Tare. Entering Pending State.");
    
    // 找到 SequentialCtrlHandler 并触发指令
    for (auto h : _handlers) {
        if (h->getMode() == MODE_SEQUENTIAL_CTRL) {
            static_cast<SequentialCtrlHandler*>(h)->triggerGlobalTare();
            break;
        }
    }

    updateOperationMode(MODE_SEQUENTIAL_CTRL);
}

void AppController::cmdStartScan() {
    updateOperationMode(MODE_DIAG_SCAN);
}

void AppController::cmdToggleDiagnosis(bool active) {
    updateOperationMode(active ? MODE_DIAG_PULSE : MODE_PRODUCTION);
}

void AppController::cmdServoTest(int id, bool open) {
    if (id < 1 || id > 20) return;
    
    // 增加 Node 10 专项诊断日志
    if (id == 10) Serial.printf("[DIAG] Sending Servo %s to Node 10...\n", open ? "OPEN" : "CLOSE");
    
    bool ok = _rs485->syncWrite(id, 0x0100, open ? 1 : 2);
    if (!ok && id == 10) Serial.println("[DIAG] Node 10: syncWrite FAILED (Timeout or Error)");
    
    _pollMgr->setServoState(id, open);
}

void AppController::cmdClearAccumulated() {
    xSemaphoreTake(_mutexProduction, portMAX_DELAY);
    _ctx.config.accumulatedWeight = 0;
    _nvs.begin("production", false);
    _nvs.putFloat("accu", 0.0f);
    _nvs.end();
    xSemaphoreGive(_mutexProduction);
}

void AppController::cmdUpdateTargets(float dMin, float dMax) {
    xSemaphoreTake(_mutexProduction, portMAX_DELAY);
    _ctx.config.targetMin += dMin;
    _ctx.config.targetMax += dMax;
    if (_ctx.config.targetMin < 10) _ctx.config.targetMin = 10;
    if (_ctx.config.targetMax < _ctx.config.targetMin)
        _ctx.config.targetMax = _ctx.config.targetMin;
    _nvs.begin("production", false);
    _nvs.putFloat("tmin", _ctx.config.targetMin);
    _nvs.putFloat("tmax", _ctx.config.targetMax);
    _nvs.end();
    xSemaphoreGive(_mutexProduction);
}

// =============================================================================
// 模式管理实现
// =============================================================================

void AppController::updateOperationMode(OperationMode newMode) {
    if (_pendingMode == newMode && _currentMode == newMode) return;

    Serial.printf("[SYSTEM] Mode Switch REQUESTED: %s\n", modeToStr(newMode));
    _pendingMode = newMode;
}

void AppController::executeModeSwitch() {
    if (_pendingMode == _currentMode) return;

    Serial.printf("[SYSTEM] ATOMIC SWITCH: %s -> %s\n",
                  modeToStr(_currentMode),
                  modeToStr(_pendingMode));

    if (_currentHandler) {
        _currentHandler->onExit();
    }

    _currentMode = _pendingMode;
    
    // 更新上下文以通告 UI
    xSemaphoreTake(_mutexProduction, portMAX_DELAY);
    _ctx.ui.curMode = _currentMode;
    xSemaphoreGive(_mutexProduction);

    // 寻找新 Handler
    _currentHandler = nullptr;
    for (auto h : _handlers) {
        if (h->getMode() == _currentMode) {
            _currentHandler = h;
            break;
        }
    }

    if (_currentHandler) {
        _currentHandler->onEnter();
    }
}

bool AppController::canSwitchMode() const {
    // 只有当总线真正空闲或处于结果终态时，才允许切换
    ModbusMaster::TransactionStatus status = _rs485->getStatus();
    return (status != ModbusMaster::ST_WAITING);
}

// =============================================================================
// 构造 / begin
// =============================================================================

AppController::AppController(ModbusMaster* rs485, PollManager* pollMgr,
                             CombinationEngine* engine, ConveyorController* conveyor,
                             UIManager* ui)
    : _rs485(rs485), _pollMgr(pollMgr), _engine(engine), _conveyor(conveyor), _ui(ui)
{
}

void AppController::begin() {
    _ui->setCommandBus(this);
    _pollMgr->setCommandBus(this);

    // --- 初始化同步原语 (Phase 4: 整合锁) ---
    _mutexProduction = xSemaphoreCreateMutex();
    _mutexDiag       = xSemaphoreCreateMutex();

    // --- 加载持久化参数 ---
    _nvs.begin("production", true);
    _ctx.config.targetMin = _nvs.getFloat("tmin", 290.0f);
    _ctx.config.targetMax = _nvs.getFloat("tmax", 310.0f);
    _ctx.config.accumulatedWeight = _nvs.getFloat("accu", 0.0f);
    _nvs.end();

    // --- 初始化通讯层与外设 ---
    _rs485->begin();
    _pollMgr->begin();
    _conveyor->begin();
    _ctx.prog.sysStatus = SYS_READY;

    // --- 初始化 Handlers ---
    _handlers.push_back(new ProductionHandler(&_ctx, _pollMgr, _rs485, _engine, _conveyor, _mutexProduction));
    _handlers.push_back(new ScanHandler(&_ctx, _pollMgr, _mutexDiag));
    _handlers.push_back(new DiagPulseHandler(&_ctx, _rs485, _mutexDiag));
    _handlers.push_back(new SequentialCtrlHandler(&_ctx, _rs485, _pollMgr, _mutexDiag));

    // --- 启动双核 FreeRTOS 任务 ---
    xTaskCreatePinnedToCore(controlTaskEntry, "ControlTask", 8192, this, 10, NULL, 1);
    xTaskCreatePinnedToCore(uiTaskEntry,      "UITask",      8192, this,  5, NULL, 0);

    Serial.println("[SYSTEM] Phase 4 Core Ready (App Architecture)");
    delay(500); 
    updateOperationMode(MODE_PRODUCTION);
}

// =============================================================================
// controlLoop — 核心业务逻辑 (Core 1)
// =============================================================================

void AppController::controlTaskEntry(void* self) {
    static_cast<AppController*>(self)->controlLoop();
}

void AppController::controlLoop() {
    Serial.println("[TASK] Control Task Started on Core 1");

    while (true) {
        // 1. 原子切换检查
        if (_pendingMode != _currentMode && canSwitchMode()) {
            executeModeSwitch();
        }

        // 2. 调度当前 Handler
        if (_currentHandler) {
            _currentHandler->onLoop();
        }

        // 3. 处理 Sequential 模式的自动返回 (可选)
        if (_currentMode == MODE_SEQUENTIAL_CTRL && _currentHandler) {
            auto seq = static_cast<SequentialCtrlHandler*>(_currentHandler);
            if (seq->isFinished()) {
                // 暂时简单的回退到生产模式，或者记录之前的模式
                updateOperationMode(MODE_PRODUCTION);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// =============================================================================
// uiLoop — UI 同步与渲染 (Core 0)
// =============================================================================

void AppController::uiTaskEntry(void* self) {
    static_cast<AppController*>(self)->uiLoop();
}

void AppController::uiLoop() {
    Serial.println("[TASK] UI/LVGL Task Started on Core 0");
    static unsigned long frameCount = 0;
    static unsigned long totalLogicMs = 0;
    static unsigned long totalRenderMs = 0;

    while (true) {
        unsigned long start = millis();
        // Phase 4 优化后的数据同步
        _pollMgr->fillUISnapshot(_ctx.ui);
        
        // 同步序列控制状态
        if (_currentMode == MODE_SEQUENTIAL_CTRL && _currentHandler) {
            auto seq = static_cast<SequentialCtrlHandler*>(_currentHandler);
            _ctx.ui.isTareRunning = !seq->isFinished();
            _ctx.ui.tareProgress = seq->getProgress();
        } else {
            _ctx.ui.isTareRunning = false;
            _ctx.ui.tareProgress = 0;
        }

        xSemaphoreTake(_mutexProduction, portMAX_DELAY);
        xSemaphoreGive(_mutexProduction);

        xSemaphoreTake(_mutexDiag, portMAX_DELAY);
        xSemaphoreGive(_mutexDiag);

        unsigned long logicEnd = millis();
        _ui->updateDashboard(&_ctx);
        
        lv_tick_inc(33);
        lv_timer_handler();
        unsigned long renderEnd = millis();

        totalLogicMs += (logicEnd - start);
        totalRenderMs += (renderEnd - logicEnd);
        frameCount++;

        if (frameCount >= 100) {
            frameCount = 0;
            totalLogicMs = 0;
            totalRenderMs = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(33));
    }
}

const char* AppController::modeToStr(OperationMode m) {
    switch (m) {
        case MODE_IDLE:            return "IDLE";
        case MODE_PRODUCTION:      return "PRODUCTION";
        case MODE_DIAG_PULSE:      return "DIAG_PULSE";
        case MODE_DIAG_SCAN:       return "DIAG_SCAN";
        case MODE_DIAG_DETAIL:     return "DIAG_DETAIL";
        case MODE_CONFIGURATION:   return "CONFIGURATION";
        case MODE_SEQUENTIAL_CTRL: return "SEQUENTIAL_CTRL";
        case MODE_SERVO_TEST:      return "SERVO_TEST";
        case MODE_ABOUT:           return "ABOUT";
        default:                   return "UNKNOWN";
    }
}
