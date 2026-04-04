#include "apps/AppProduction.h"
#include <Arduino.h>
#include "system/SystemConfig.h"
#include "system/SystemContext.h"
#include "logic/PollManager.h"
#include "drivers/ModbusMaster.h"
#include "logic/CombinationEngine.h"
#include "logic/ConveyorController.h"

AppProduction::AppProduction(SystemContext* ctx, PollManager* pollMgr, ModbusMaster* rs485,
                             CombinationEngine* engine, ConveyorController* conveyor,
                             SemaphoreHandle_t mutex)
    : _ctx(ctx), _pollMgr(pollMgr), _rs485(rs485), _engine(engine), _conveyor(conveyor), _mutex(mutex)
{
}

void AppProduction::onEnter() {
    loadParams();
    Serial.println("[AppProduction] Production Mode Entered.");
}

void AppProduction::onLoop() {
    handlePolling();

    // 核心组合引擎逻辑由独立节拍控制
    bool  canCalculate = true; 
    float currentMin, currentMax;
    SystemStatus currentStatus;

    xSemaphoreTake(_mutex, portMAX_DELAY);
    currentMin    = _ctx->config.targetMin;
    currentMax    = _ctx->config.targetMax;
    currentStatus = _ctx->prog.sysStatus;
    xSemaphoreGive(_mutex);

    if (canCalculate && currentStatus == SYS_READY) {
        if (millis() - _lastCalcTime > CALC_ENGINE_INTERVAL_MS) {
            _lastCalcTime = millis();
            _engine->setTargetRange(currentMin, currentMax);

            std::vector<float> activeWeights;
            std::vector<int>   activeIds;
            for (int id = 1; id <= 20; id++) {
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
                xSemaphoreTake(_mutex, portMAX_DELAY);
                _ctx->prog.sysStatus        = SYS_DISCHARGING;
                _ctx->prog.batchWeight      = res.totalWeight;
                _ctx->prog.idMask           = 0;
                std::vector<int> mappedIds;
                for (int idx : res.selectedIndices) {
                    int physId = activeIds[idx];
                    mappedIds.push_back(physId);
                    _ctx->prog.idMask |= (1 << (physId - 1));
                    _pollMgr->setNodeStatus(physId, NODE_LOCKED);
                }
                xSemaphoreGive(_mutex);

                Serial.printf("[AppProduction] Combined: %.1f g, Mask: 0x%08X\n", res.totalWeight, _ctx->prog.idMask);

                for (int id : mappedIds) _rs485->syncWrite(id, 0x0100, 5);
                vTaskDelay(pdMS_TO_TICKS(DISCHARGE_SETTLE_MS));
                for (int id : mappedIds) _pollMgr->setNodeStatus(id, NODE_DIRTY);

                xSemaphoreTake(_mutex, portMAX_DELAY);
                _ctx->config.accumulatedWeight += res.totalWeight;
                saveParams(); 
                
                _ctx->prog.sysStatus            = SYS_TRANSFER_B1;
                _ctx->prog.idMask               = 0;
                xSemaphoreGive(_mutex);

                _conveyor->collectFromUnits();
                vTaskDelay(pdMS_TO_TICKS(BELT_COLLECT_PERIOD_MS));
                
                xSemaphoreTake(_mutex, portMAX_DELAY);
                _ctx->prog.sysStatus = SYS_STEPPING_B2;
                xSemaphoreGive(_mutex);
                
                _conveyor->advanceOutput();
                vTaskDelay(pdMS_TO_TICKS(BELT_STEP_PERIOD_MS));

                xSemaphoreTake(_mutex, portMAX_DELAY);
                _ctx->prog.sysStatus = SYS_READY;
                xSemaphoreGive(_mutex);
            }
        }
    }
}

void AppProduction::handlePolling() {
    uint8_t nextId = _currentPollId;
    for (int i = 0; i < 20; i++) {
        nextId = (nextId % 20) + 1;
        if (_pollMgr->isWhitelisted(nextId)) break;
    }
    
    if (_pollMgr->asyncUpdateNode(nextId)) {
        _currentPollId = nextId;
    }
}

void AppProduction::onExit() {
    Serial.println("[AppProduction] Production Mode Exited.");
}

void AppProduction::updateTargets(float dMin, float dMax) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _ctx->config.targetMin += dMin;
    _ctx->config.targetMax += dMax;
    if (_ctx->config.targetMin < 10) _ctx->config.targetMin = 10;
    if (_ctx->config.targetMax < _ctx->config.targetMin)
        _ctx->config.targetMax = _ctx->config.targetMin;
    saveParams();
    xSemaphoreGive(_mutex);
}

void AppProduction::clearAccumulated() {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _ctx->config.accumulatedWeight = 0;
    saveParams();
    xSemaphoreGive(_mutex);
}

void AppProduction::loadParams() {
    _nvs.begin("production", true);
    _ctx->config.targetMin = _nvs.getFloat("tmin", 290.0f);
    _ctx->config.targetMax = _nvs.getFloat("tmax", 310.0f);
    _ctx->config.accumulatedWeight = _nvs.getFloat("accu", 0.0f);
    _nvs.end();
}

void AppProduction::saveParams() {
    _nvs.begin("production", false);
    _nvs.putFloat("tmin", _ctx->config.targetMin);
    _nvs.putFloat("tmax", _ctx->config.targetMax);
    _nvs.putFloat("accu", _ctx->config.accumulatedWeight);
    _nvs.end();
}
