#include "AppController.h"
#include <lvgl.h>
#include "system/ModbusMaster.h"
#include "system/PollManager.h"
#include "system/SystemConfig.h"
#include "system/PinDefinition.h"
#include "logic/CombinationEngine.h"
#include "logic/ConveyorController.h"
#include "UIManager.h"

// =============================================================================
// ICommandBus 实现
// =============================================================================

void AppController::cmdGlobalTare() {
    _rs485->broadcastWrite(0x0100, CMD_TARE);
}

void AppController::cmdStartScan() {
    updateOperationMode(MODE_DIAG_SCAN);
}

void AppController::cmdToggleDiagnosis(bool active) {
    updateOperationMode(active ? MODE_DIAG_PULSE : MODE_PRODUCTION);
}

void AppController::cmdClearAccumulated() {
    xSemaphoreTake(_mutexProduction, portMAX_DELAY);
    _accumulatedWeight            = 0;
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
    if (_ctx.ui.curMode == newMode) return;

    Serial.printf("[SYSTEM] Mode Changing: %s -> %s\n",
                  modeToStr(_ctx.ui.curMode),
                  modeToStr(newMode));

    if (_ctx.ui.curMode == MODE_DIAG_SCAN || _ctx.ui.curMode == MODE_DIAG_PULSE)
        _rs485->clearRawBuffer();

    xSemaphoreTake(_mutexProduction, portMAX_DELAY);
    _ctx.ui.curMode = newMode;
    xSemaphoreGive(_mutexProduction);

    _pollMgr->setMode(newMode);
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
    _accumulatedWeight    = _nvs.getFloat("accu", 0.0f);
    _ctx.config.accumulatedWeight = _accumulatedWeight;
    _nvs.end();

    // --- 初始化通讯层与外设 ---
    _rs485->begin();
    _pollMgr->begin();
    _conveyor->begin();
    _ctx.prog.status = SYS_READY;

    // --- 启动双核 FreeRTOS 任务 ---
    xTaskCreatePinnedToCore(controlTaskEntry, "ControlTask", 8192, this, 10, NULL, 1);
    xTaskCreatePinnedToCore(uiTaskEntry,      "UITask",      8192, this,  5, NULL, 0);

    Serial.println("[SYSTEM] Phase 4 Core Ready");
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
    static unsigned long lastDiagPulseTime = 0;
    static unsigned long lastCalcTime      = 0;

    while (true) {
        OperationMode mode;
        xSemaphoreTake(_mutexProduction, portMAX_DELAY);
        mode = _ctx.ui.curMode;
        xSemaphoreGive(_mutexProduction);

        // --- 1. 链路诊断模式 (1Hz 原始字节脉冲) ---
        if (mode == MODE_DIAG_PULSE) {
            if (millis() - lastDiagPulseTime >= 1000) {
                lastDiagPulseTime = millis();
                xSemaphoreTake(_mutexDiag, portMAX_DELAY);
                _ctx.diag.diagLastSent++;
                uint8_t toSend = _ctx.diag.diagLastSent;
                xSemaphoreGive(_mutexDiag);
                _rs485->sendRawByte(toSend);
            }
            if (_rs485->availableRaw() > 0) {
                xSemaphoreTake(_mutexDiag, portMAX_DELAY);
                while (_rs485->availableRaw() > 0) {
                    uint8_t b = _rs485->readRawByte();
                    char hexBuf[8];
                    snprintf(hexBuf, sizeof(hexBuf), "%02X ", b);
                    if (strlen(_ctx.diag.diagRxHex) > 100)
                        memset(_ctx.diag.diagRxHex, 0, sizeof(_ctx.diag.diagRxHex));
                    strncat(_ctx.diag.diagRxHex, hexBuf, sizeof(_ctx.diag.diagRxHex) - strlen(_ctx.diag.diagRxHex) - 1);
                }
                xSemaphoreGive(_mutexDiag);
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // --- 2. 轮询器处理与自动扫描同步 ---
        _pollMgr->process();

        if (mode == MODE_DIAG_SCAN) {
             xSemaphoreTake(_mutexDiag, portMAX_DELAY);
             _ctx.diag.scanProgress     = _pollMgr->getScanProgress();
             _ctx.diag.currentScanCycle = _pollMgr->getScanCycle();
             for(int c=0; c<5; c++)
                 for(int i=1; i<=20; i++)
                     _ctx.diag.scanResults[c][i] = _pollMgr->getScanHistory(c, i);
             xSemaphoreGive(_mutexDiag);
        }

        // --- 3. 生产称重流程 ---
        if (mode == MODE_PRODUCTION) {
            bool  canCalculate;
            float currentMin, currentMax;
            SystemStatus currentStatus;

            xSemaphoreTake(_mutexProduction, portMAX_DELAY);
            canCalculate  = _isProductionActive;
            currentMin    = _ctx.config.targetMin;
            currentMax    = _ctx.config.targetMax;
            currentStatus = _ctx.prog.status;
            xSemaphoreGive(_mutexProduction);

            if (canCalculate && currentStatus == SYS_READY) {
                if (millis() - lastCalcTime > CALC_ENGINE_INTERVAL_MS) {
                    lastCalcTime = millis();
                    _engine->setTargetRange(currentMin, currentMax);

                    // Phase 4: [DIRECT ACCESS] 直接从 PollManager 获取数据，消除 redundant 拷贝层
                    std::vector<float> activeWeights;
                    std::vector<int>   activeIds;
                    for (int id = 1; id <= NUM_SLAVES; id++) {
                        if (_pollMgr->isStable(id) && 
                            _pollMgr->getNodeStatus(id) == NODE_STABLE && 
                            _pollMgr->isWhitelisted(id)) {
                            activeWeights.push_back(_pollMgr->getWeight(id));
                            activeIds.push_back(id);
                        }
                    }

                    CombinationResult res = {false, 0.0f, {}};
                    if (!activeWeights.empty()) res = _engine->findBestCombination(activeWeights);

                    if (res.success) {
                        xSemaphoreTake(_mutexProduction, portMAX_DELAY);
                        _ctx.prog.status           = SYS_DISCHARGING;
                        _ctx.prog.lastBatchWeight  = res.totalWeight;
                        _ctx.prog.selectionMask    = 0;
                        std::vector<int> mappedIds;
                        for (int idx : res.selectedIndices) {
                            int physId = activeIds[idx];
                            mappedIds.push_back(physId);
                            _ctx.prog.selectionMask |= (1 << (physId - 1));
                            _pollMgr->setNodeStatus(physId, NODE_LOCKED);
                        }
                        xSemaphoreGive(_mutexProduction);

                        Serial.printf("[AUTO] Combined: %.1f g, Mask: 0x%08X\n", res.totalWeight, _ctx.prog.selectionMask);

                        // 执行下料指令
                        for (int id : mappedIds) _rs485->syncWrite(id, 0x0100, 5);
                        vTaskDelay(pdMS_TO_TICKS(DISCHARGE_SETTLE_MS));
                        for (int id : mappedIds) _pollMgr->setNodeStatus(id, NODE_DIRTY);

                        xSemaphoreTake(_mutexProduction, portMAX_DELAY);
                        _accumulatedWeight            += res.totalWeight;
                        _ctx.config.accumulatedWeight  = _accumulatedWeight;
                        
                        // Persistence: Save to NVS immediately
                        _nvs.begin("production", false);
                        _nvs.putFloat("accu", _accumulatedWeight);
                        _nvs.end();

                        _ctx.prog.status               = SYS_TRANSFER_B1;
                        _ctx.prog.selectionMask        = 0;
                        xSemaphoreGive(_mutexProduction);

                        // 输送带动作
                        _conveyor->collectFromUnits();
                        vTaskDelay(pdMS_TO_TICKS(BELT_COLLECT_PERIOD_MS));
                        
                        xSemaphoreTake(_mutexProduction, portMAX_DELAY);
                        _ctx.prog.status = SYS_STEPPING_B2;
                        xSemaphoreGive(_mutexProduction);
                        
                        _conveyor->advanceOutput();
                        vTaskDelay(pdMS_TO_TICKS(BELT_STEP_PERIOD_MS));

                        xSemaphoreTake(_mutexProduction, portMAX_DELAY);
                        _ctx.prog.status = SYS_READY;
                        xSemaphoreGive(_mutexProduction);
                    }
                }
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
            Serial.printf("[PERF] UI 100-Frame Avg: Logic %.1f ms, Render %.1f ms, FPS %.1f\n",
                          (float)totalLogicMs / 100.0f,
                          (float)totalRenderMs / 100.0f,
                          1000.0f / ((float)(totalLogicMs + totalRenderMs) / 100.04f + 33.0f));
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
        case MODE_ABOUT:           return "ABOUT";
        default:                   return "UNKNOWN";
    }
}
