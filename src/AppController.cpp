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
    xSemaphoreTake(_mutexParams, portMAX_DELAY);
    _accumulatedWeight            = 0;
    _ctx.config.accumulatedWeight = 0;
    _nvs.begin("production", false);
    _nvs.putFloat("accu", 0.0f);
    _nvs.end();
    xSemaphoreGive(_mutexParams);
}

void AppController::cmdUpdateTargets(float dMin, float dMax) {
    xSemaphoreTake(_mutexParams, portMAX_DELAY);
    _ctx.config.targetMin += dMin;
    _ctx.config.targetMax += dMax;
    if (_ctx.config.targetMin < 10) _ctx.config.targetMin = 10;
    if (_ctx.config.targetMax < _ctx.config.targetMin)
        _ctx.config.targetMax = _ctx.config.targetMin;
    _nvs.begin("production", false);
    _nvs.putFloat("tmin", _ctx.config.targetMin);
    _nvs.putFloat("tmax", _ctx.config.targetMax);
    _nvs.end();
    xSemaphoreGive(_mutexParams);
}

// =============================================================================
// 模式管理实现
// =============================================================================

void AppController::updateOperationMode(OperationMode newMode) {
    if (_ctx.state.curMode == newMode) return;

    Serial.printf("[SYSTEM] Mode Changing: %s -> %s\n",
                  modeToStr(_ctx.state.curMode),
                  modeToStr(newMode));

    if (_ctx.state.curMode == MODE_DIAG_SCAN || _ctx.state.curMode == MODE_DIAG_PULSE)
        _rs485->clearRawBuffer();

    xSemaphoreTake(_mutexStatus, portMAX_DELAY);
    _ctx.state.curMode = newMode;
    xSemaphoreGive(_mutexStatus);

    _pollMgr->setMode(newMode);
}

// =============================================================================
// 构造 / begin
// =============================================================================

AppController::AppController(ModbusMaster* rs485, PollManager* pollMgr,
                             CombinationEngine* engine, ConveyorController* conveyor,
                             UIManager* ui)
    : _rs485(rs485), _pollMgr(pollMgr), _engine(engine), _conveyor(conveyor), _ui(ui),
      _slaveWeights(NUM_SLAVES, 0.0f), _slaveStable(NUM_SLAVES, false)
{
}

void AppController::begin() {
    // --- 注入 ICommandBus ---
    _ui->setCommandBus(this);
    _pollMgr->setCommandBus(this);

    // --- 初始化同步原语 ---
    _mutexParams  = xSemaphoreCreateMutex();
    _mutexWeights = xSemaphoreCreateMutex();
    _mutexStatus  = xSemaphoreCreateMutex();

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
    _systemStatus = SYS_READY;

    // --- 启动双核 FreeRTOS 任务 ---
    xTaskCreatePinnedToCore(controlTaskEntry, "ControlTask", 8192, this, 10, NULL, 1);
    xTaskCreatePinnedToCore(uiTaskEntry,      "UITask",      8192, this,  5, NULL, 0);

    Serial.println("[SYSTEM] Dual-Core Multitasking Started");
    delay(500); 
    updateOperationMode(MODE_PRODUCTION);
}

// =============================================================================
// FreeRTOS 任务逻辑 (保持不变)
// =============================================================================

void AppController::controlTaskEntry(void* self) {
    static_cast<AppController*>(self)->controlLoop();
}

void AppController::uiTaskEntry(void* self) {
    static_cast<AppController*>(self)->uiLoop();
}

void AppController::controlLoop() {
    Serial.println("[TASK] Control Task Started on Core 1");
    static unsigned long lastDiagPulseTime = 0;
    static unsigned long lastCalcTime      = 0;

    while (true) {
        OperationMode mode;
        xSemaphoreTake(_mutexStatus, portMAX_DELAY);
        mode = _ctx.state.curMode;
        xSemaphoreGive(_mutexStatus);

        if (mode == MODE_DIAG_PULSE) {
            if (millis() - lastDiagPulseTime >= 1000) {
                lastDiagPulseTime = millis();
                xSemaphoreTake(_mutexStatus, portMAX_DELAY);
                _ctx.state.diagLastSent++;
                uint8_t toSend = _ctx.state.diagLastSent;
                xSemaphoreGive(_mutexStatus);
                _rs485->sendRawByte(toSend);
            }
            if (_rs485->availableRaw() > 0) {
                xSemaphoreTake(_mutexStatus, portMAX_DELAY);
                while (_rs485->availableRaw() > 0) {
                    uint8_t b = _rs485->readRawByte();
                    char hexBuf[8];
                    snprintf(hexBuf, sizeof(hexBuf), "%02X ", b);
                    if (strlen(_ctx.state.diagRxHex) > 100)
                        memset(_ctx.state.diagRxHex, 0, sizeof(_ctx.state.diagRxHex));
                    strncat(_ctx.state.diagRxHex, hexBuf,
                            sizeof(_ctx.state.diagRxHex) - strlen(_ctx.state.diagRxHex) - 1);
                }
                xSemaphoreGive(_mutexStatus);
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        _pollMgr->process();

        if (mode == MODE_PRODUCTION) {
            xSemaphoreTake(_mutexWeights, portMAX_DELAY);
            for (int i = 0; i < NUM_SLAVES; i++) {
                _slaveWeights[i] = _pollMgr->getWeight(i + 1);
                _slaveStable[i]  = _pollMgr->isStable(i + 1);
            }
            xSemaphoreGive(_mutexWeights);

            bool  canCalculate;
            float currentMin, currentMax;
            xSemaphoreTake(_mutexParams, portMAX_DELAY);
            canCalculate = _isProductionActive;
            currentMin   = _ctx.config.targetMin;
            currentMax   = _ctx.config.targetMax;
            xSemaphoreGive(_mutexParams);

            SystemStatus currentStatus;
            xSemaphoreTake(_mutexStatus, portMAX_DELAY);
            currentStatus = _systemStatus;
            xSemaphoreGive(_mutexStatus);

            if (canCalculate && currentStatus == SYS_READY) {
                if (millis() - lastCalcTime > CALC_ENGINE_INTERVAL_MS) {
                    lastCalcTime = millis();
                    _engine->setTargetRange(currentMin, currentMax);

                    std::vector<float> activeWeights;
                    std::vector<int>   activeIds;
                    for (int i = 0; i < NUM_SLAVES; i++) {
                        int id = i + 1;
                        if (_slaveStable[i] &&
                            _pollMgr->getNodeStatus(id) == NODE_STABLE &&
                            _pollMgr->isWhitelisted(id)) {
                            activeWeights.push_back(_slaveWeights[i]);
                            activeIds.push_back(id);
                        }
                    }

                    CombinationResult res = {false, 0.0f, {}};
                    if (!activeWeights.empty())
                        res = _engine->findBestCombination(activeWeights);

                    if (res.success) {
                        xSemaphoreTake(_mutexStatus, portMAX_DELAY);
                        _systemStatus        = SYS_DISCHARGING;
                        _lastCombinedWeight  = res.totalWeight;
                        _currentSelectedMask = 0;
                        std::vector<int> mappedIds;
                        for (int idx : res.selectedIndices) {
                            int physicalId = activeIds[idx];
                            mappedIds.push_back(physicalId);
                            _currentSelectedMask |= (1 << (physicalId - 1));
                            _pollMgr->setNodeStatus(physicalId, NODE_LOCKED);
                        }
                        xSemaphoreGive(_mutexStatus);

                        Serial.printf("[AUTO] Combination Found: %.1f g, Mask: 0x%08X\n",
                                      res.totalWeight, _currentSelectedMask);

                        for (int id : mappedIds) _rs485->syncWrite(id, 0x0100, 5);
                        vTaskDelay(pdMS_TO_TICKS(DISCHARGE_SETTLE_MS));
                        for (int id : mappedIds) _pollMgr->setNodeStatus(id, NODE_DIRTY);

                        xSemaphoreTake(_mutexParams, portMAX_DELAY);
                        _accumulatedWeight            += res.totalWeight;
                        _ctx.config.accumulatedWeight  = _accumulatedWeight;
                        xSemaphoreGive(_mutexParams);

                        xSemaphoreTake(_mutexStatus, portMAX_DELAY);
                        _systemStatus        = SYS_TRANSFER_B1;
                        _currentSelectedMask = 0;
                        xSemaphoreGive(_mutexStatus);
                        _conveyor->collectFromUnits();
                        vTaskDelay(pdMS_TO_TICKS(BELT_COLLECT_PERIOD_MS));

                        xSemaphoreTake(_mutexStatus, portMAX_DELAY);
                        _systemStatus = SYS_STEPPING_B2;
                        xSemaphoreGive(_mutexStatus);
                        _conveyor->advanceOutput();
                        vTaskDelay(pdMS_TO_TICKS(BELT_STEP_PERIOD_MS));

                        xSemaphoreTake(_mutexStatus, portMAX_DELAY);
                        _systemStatus = SYS_READY;
                        xSemaphoreGive(_mutexStatus);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void AppController::uiLoop() {
    Serial.println("[TASK] UI/LVGL Task Started on Core 0");
    while (true) {
        xSemaphoreTake(_mutexWeights, portMAX_DELAY);
        for (int i = 0; i < NUM_SLAVES; i++) {
            _ctx.state.currentWeights[i] = _slaveWeights[i];
            _ctx.state.stableNodes[i]    = _slaveStable[i];
        }
        xSemaphoreGive(_mutexWeights);

        xSemaphoreTake(_mutexStatus, portMAX_DELAY);
        _ctx.state.lastBatchWeight   = _lastCombinedWeight;
        _ctx.state.selectionMask     = _currentSelectedMask;
        _ctx.state.status            = _systemStatus;
        _ctx.state.scanProgress      = _pollMgr->getScanProgress();
        _ctx.state.currentScanCycle  = _pollMgr->getScanCycle();
        for (int c = 0; c < 5; c++)
            for (int i = 0; i < 20; i++)
                _ctx.state.scanResults[c][i] = _pollMgr->getScanHistory(c, i + 1);
        for (int i = 0; i < 20; i++) {
            _ctx.state.onlineNodes[i]      = _pollMgr->isOnline(i + 1);
            _ctx.state.whitelistedNodes[i] = _pollMgr->isWhitelisted(i + 1);
        }
        xSemaphoreGive(_mutexStatus);

        _ui->updateDashboard(&_ctx);
        lv_tick_inc(33);
        lv_timer_handler();
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
