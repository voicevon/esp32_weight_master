#include "apps/AppProduction.h"
#include <Arduino.h>
#include "system/SystemConfig.h"
#include "system/SystemContext.h"
#include "logic/PollManager.h"
#include "drivers/ModbusMaster.h"
#include "logic/CombinationEngine.h"
#include "logic/BeltManager.h"

AppProduction::AppProduction(SystemContext* ctx, PollManager* pollMgr, ModbusMaster* rs485,
                             CombinationEngine* engine, BeltManager* conveyor,
                             SemaphoreHandle_t mutex)
    : _ctx(ctx), _pollMgr(pollMgr), _rs485(rs485), _engine(engine), _conveyor(conveyor), _mutex(mutex)
{
}

void AppProduction::onEnter() {
    loadParams();
    _dischargeIndex = 0;
    _selectedIds.clear();
    _stateStartTime = millis();
    updateUIState(SYS_READY);
    Serial.println("[AppProduction] Production Mode Entered.");
}

void AppProduction::onLoop() {
    unsigned long now = millis();
    SystemStatus currentStatus;

    // 同步获取当前业务状态
    xSemaphoreTake(_mutex, portMAX_DELAY);
    currentStatus = _ctx->prog.sysStatus;
    xSemaphoreGive(_mutex);

    // 1. 动态轮询控制：在就绪及皮带运转期间刷新数据，提高系统吞吐率
    if (currentStatus == SYS_READY || currentStatus == SYS_BELT_A || currentStatus == SYS_BELT_B) {
        handlePolling();
    }

    // 2. 状态机核心路由
    switch (currentStatus) {
        case SYS_READY:
            handleReadyState(now);
            break;

        case SYS_SEQ_DROP:
            handleDropState(now);
            break;

        case SYS_SETTLE_STABLE:
            handleSettleState(now);
            break;

        case SYS_BELT_A:
            handleBeltAState(now);
            break;

        case SYS_BELT_B:
            handleBeltBState(now);
            break;

        default:
            updateUIState(SYS_READY); // 异常状态恢复
            break;
    }
}

void AppProduction::handleReadyState(unsigned long now) {
    if (now - _lastCalcTime <= CALC_ENGINE_INTERVAL_MS) return;
    _lastCalcTime = now;

    float currentMin, currentMax;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    currentMin = _ctx->config.targetMin;
    currentMax = _ctx->config.targetMax;
    xSemaphoreGive(_mutex);

    // 1. 预检查：仅在稳定总重可能达标时才启动昂贵的引擎计算 (优化项)
    float stableSum = 0;
    std::vector<float> activeWeights;
    std::vector<int>   activeIds;

    for (int id = 1; id <= 20; id++) {
        if (_pollMgr->isOnline(id) && _pollMgr->isStable(id) && _pollMgr->isWhitelisted(id)) {
            float w = _pollMgr->getWeight(id);
            stableSum += w;
            activeWeights.push_back(w);
            activeIds.push_back(id);
        }
    }

    if (stableSum < currentMin) return; 

    // 2. 调用算法引擎
    _engine->setTargetRange(currentMin, currentMax);
    CombinationResult res = _engine->findBestCombination(activeWeights);
    if (!res.success) return;

    // 3. 准备下料数据并切入下料状态
    _selectedIds.clear();
    uint32_t mask = 0;
    for (int idx : res.selectedIndices) {
        int physId = activeIds[idx];
        _selectedIds.push_back(physId);
        mask |= (1 << (physId - 1));
    }
    
    _dischargeIndex = 0;
    _lastCombinedWeight = res.totalWeight;
    updateUIState(SYS_SEQ_DROP, mask, res.totalWeight);
    Serial.printf("[AppProduction] Combined: %.1f g, Mask: 0x%08X\n", res.totalWeight, mask);
}

void AppProduction::handleDropState(unsigned long now) {
    // 异步逐个分发下料指令
    if (_dischargeIndex < (int)_selectedIds.size()) {
        int nodeId = _selectedIds[_dischargeIndex];
        bool sent = _rs485->asyncWrite(nodeId, 0x0100, 5, [this](Modbus::ResultCode res, uint16_t tid, void* data) {
            this->_dischargeIndex++;
            return true;
        });
    } else {
        _stateStartTime = now;
        updateUIState(SYS_SETTLE_STABLE);
    }
}

void AppProduction::handleSettleState(unsigned long now) {
    if (now - _stateStartTime >= DISCHARGE_SETTLE_MS) {
        _conveyor->collectFromUnits();
        _stateStartTime = now;
        updateUIState(SYS_BELT_A);
    }
}

void AppProduction::handleBeltAState(unsigned long now) {
    if (now - _stateStartTime >= BELT_COLLECT_PERIOD_MS) {
        _conveyor->advanceOutput();
        _stateStartTime = now;
        updateUIState(SYS_BELT_B);
    }
}

void AppProduction::handleBeltBState(unsigned long now) {
    if (now - _stateStartTime >= BELT_STEP_PERIOD_MS) {
        updateUIState(SYS_READY);
    }
}

/**
 * @brief 界面反馈更新 (UI 与 逻辑解耦的桥梁)
 */
void AppProduction::updateUIState(SystemStatus status, uint32_t mask, float weight) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _ctx->prog.sysStatus = status;
    
    // 逻辑层决定文案，解耦 UI 与内部状态映射
    switch (status) {
        case SYS_READY:         strncpy(_ctx->prog.statusText, "就绪", 32); break;
        case SYS_SEQ_DROP:      strncpy(_ctx->prog.statusText, "逐个下料中", 32); break;
        case SYS_SETTLE_STABLE: strncpy(_ctx->prog.statusText, "沉降稳定中", 32); break;
        case SYS_BELT_A:        strncpy(_ctx->prog.statusText, "收集皮带运行", 32); break;
        case SYS_BELT_B:        strncpy(_ctx->prog.statusText, "步进输出运行", 32); break;
        default:                strncpy(_ctx->prog.statusText, "初始化", 32); break;
    }

    if (mask != 0 || status == SYS_BELT_A || status == SYS_READY) {
        _ctx->prog.idMask = mask;
    }
    
    if (weight > 0.0f) {
        _ctx->prog.batchWeight = weight;
        _ctx->config.accumulatedWeight += weight;
        saveParams(); // 生产数据落盘
    }
    xSemaphoreGive(_mutex);
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
