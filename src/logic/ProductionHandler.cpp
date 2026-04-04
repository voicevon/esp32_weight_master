#include "ProductionHandler.h"
#include <Arduino.h>
#include <vector>
#include "system/SystemTypes.h"

void ProductionHandler::onLoop() {
    _pollMgr->process();

    bool  canCalculate;
    float currentMin, currentMax;
    SystemStatus currentStatus;

    xSemaphoreTake(_mutex, portMAX_DELAY);
    canCalculate  = true; // 简单化处理，Handler 只在开启时 loop
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

                Serial.printf("[AUTO] Combined: %.1f g, Mask: 0x%08X\n", res.totalWeight, _ctx->prog.idMask);

                // 执行下料指令
                for (int id : mappedIds) _rs485->syncWrite(id, 0x0100, 5);
                vTaskDelay(pdMS_TO_TICKS(DISCHARGE_SETTLE_MS));
                for (int id : mappedIds) _pollMgr->setNodeStatus(id, NODE_DIRTY);

                xSemaphoreTake(_mutex, portMAX_DELAY);
                _ctx->config.accumulatedWeight += res.totalWeight;
                
                _ctx->prog.sysStatus            = SYS_TRANSFER_B1;
                _ctx->prog.idMask               = 0;
                xSemaphoreGive(_mutex);

                // 输送带动作
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
