#include "apps/AppProduction.h"
#include <Arduino.h>
#include "system/SystemConfig.h"
#include "system/SystemContext.h"
#include "logic/NodeManager.h"
#include "logic/WeightNode.h"
#include "drivers/ModbusMaster.h"
#include "logic/CombinationEngine.h"
#include "drivers/Belt.h"

#define BELT2_AUTO_STOP_MS            5000  // 二级带自主停机超时 (5秒)

AppProduction::AppProduction(SystemContext* ctx, NodeManager* nodeMgr, ModbusMaster* rs485,
                             CombinationEngine* engine, Belt* b1, Belt* b2,
                             SemaphoreHandle_t mutex)
    : _ctx(ctx), _nodeMgr(nodeMgr), _rs485(rs485), _engine(engine), _b1(b1), _b2(b2), _mutex(mutex),
      _sequencer(ctx, rs485, nodeMgr)
{
}

void AppProduction::onEnter() {
    loadParams();
    _dischargeIndex = 0;
    _selectedNodes.clear();
    _stateStartTime = millis();
    _belt2StartTime = 0;
    _belt2Running = false;
    updateUIState(SYS_READY, 0, 0, true);
    Serial.println("[AppProduction] Production Mode Entered.");
}

void AppProduction::onLoop() {
    unsigned long now = millis();
    SystemStatus currentStatus;

    // 驱动指令序列 (置零等)
    _sequencer.update(now);

    // 同步获取当前业务状态
    xSemaphoreTake(_mutex, portMAX_DELAY);
    currentStatus = _ctx->prog.sysStatus;
    xSemaphoreGive(_mutex);

    // 1. 动态轮询控制：就绪或皮带运转期间执行轮询。
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

        case SYS_SEQ_CLOSE:
            handleCloseState(now);
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

    // 3. 皮带 2 自主停机监测 (Watchdog)
    if (_belt2Running && (now - _belt2StartTime >= BELT2_AUTO_STOP_MS)) {
        _b2->speedStop();
        _belt2Running = false;
        Serial.println("[AppProduction] Belt2 Autonomous Stop (Timeout).");
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
    std::vector<WeightNode*> stableNodes;

    for (int id = 1; id <= 20; id++) {
        WeightNode* node = _nodeMgr->getNode(id);
        if (node && node->isOnline() && node->isStable() && node->isWhitelisted()) {
            float w = node->getWeight();
            stableSum += w;
            stableNodes.push_back(node);
        }
    }

    if (stableSum < currentMin) return; 

    // 2. 调用算法引擎
    _engine->setTargetRange(currentMin, currentMax);
    CombinationResult res = _engine->findBestCombination(stableNodes);
    
    if (!res.success) {
        // 寻解失败时不清除上一次的 idMask 和快照，实现“锁定显示”
        updateUIState(SYS_READY, _ctx->prog.idMask, 0, false); 
        return;
    }

    // 3. 准备下料数据并切入下料状态
    _selectedNodes = res.selectedNodes;

    // 清理并更新重量快照块
    memset(_ctx->prog.lastBatchWeights, 0, sizeof(_ctx->prog.lastBatchWeights));
    
    uint32_t mask = 0;
    for (auto* node : _selectedNodes) {
        int id = node->getId();
        mask |= (1 << (id - 1));
        _ctx->prog.lastBatchWeights[id] = node->getWeight(); // 捕获瞬间重量用于锁定显示
        node->invalidate(); // 立即作废数据，防止重复拾取
    }
    
    _dischargeIndex = 0;
    _lastCombinedWeight = res.totalWeight;
    _stateStartTime = now;
    _ctx->prog.dirtyFlags |= (DF_PROD_RES | DF_WEIGHT_LIST); // 重大结果变化
    updateUIState(SYS_SEQ_DROP, mask, res.totalWeight);
    Serial.printf("[AppProduction] Combined: %.1f g, Mask: 0x%08X\n", res.totalWeight, mask);
}

void AppProduction::handleDropState(unsigned long now) {
    if (_dischargeIndex < (int)_selectedNodes.size()) {
        WeightNode* node = _selectedNodes[_dischargeIndex];
        
        // 如果已经开启，则看下一个
        if (node->isServoOpen()) {
            _dischargeIndex++;
            return;
        }

        // 尝试开启
        if (node->asyncOpenServo()) {
            Serial.printf("[AppProduction] Sending OPEN to Node %d...\n", node->getId());
        }
        
        // 挂起状态检查：如果重试多次仍失败，通知用户并在运行时拉黑该节点
        if (node->getRetryCount() >= 3) {
            char failMsg[32];
            snprintf(failMsg, sizeof(failMsg), "节点 %d 开启失败", node->getId());
            strncpy(_ctx->prog.statusText, failMsg, 32);
            Serial.printf("[AppProduction] CRITICAL: Node %d failed to open after 3 retries. Blacklisting.\n", node->getId());
            node->setHealthy(false); // 运行时拉黑
            _dischargeIndex++; // 跳过该错误节点
        }
    } 
    else if (now - _stateStartTime >= DISCHARGE_MIN_DURATION_MS) {
        _dischargeIndex = 0;
        updateUIState(SYS_SEQ_CLOSE);
    }
}

void AppProduction::handleCloseState(unsigned long now) {
    if (_dischargeIndex < (int)_selectedNodes.size()) {
        WeightNode* node = _selectedNodes[_dischargeIndex];
        
        if (!node->isServoOpen()) {
            _dischargeIndex++;
            return;
        }

        if (node->asyncCloseServo()) {
            Serial.printf("[AppProduction] Sending CLOSE to Node %d...\n", node->getId());
        }

        if (node->getRetryCount() >= 3) {
            Serial.printf("[AppProduction] CRITICAL: Node %d failed to close. Blacklisting.\n", node->getId());
            node->setHealthy(false);
            _dischargeIndex++;
        }
    } else {
        // 所有舵机关闭完成后，启动皮带运行
        _b2->stop(); 
        _belt2Running = false;

        _b1->moveDistanceMm(2000);
        _stateStartTime = now;
        updateUIState(SYS_BELT_A);
    }
}

void AppProduction::handleSettleState(unsigned long now) {
    if (now - _stateStartTime >= DISCHARGE_SETTLE_MS) {
        _b2->stop(); // 重要：高优先级物理互锁
        _belt2Running = false;

        _b1->moveDistanceMm(2000);
        _stateStartTime = now;
        updateUIState(SYS_BELT_A);
    }
}
void AppProduction::handleBeltAState(unsigned long now) {
    if (now - _stateStartTime >= BELT_COLLECT_PERIOD_MS) {
        // 模式切换：由点动改为持续运行 (Speed Mode)
        _b2->speedRun(true); 
        _belt2StartTime = now;
        _belt2Running = true;
        
        updateUIState(SYS_READY); // 直接进入就绪，开始下一轮计算
        Serial.println("[AppProduction] Belt2 Started (Continuous Mode).");
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
void AppProduction::updateUIState(SystemStatus status, uint32_t mask, float weight, bool success) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _ctx->prog.sysStatus = status;
    _ctx->prog.lastCalcSuccess = success;
    
    // 逻辑层决定文案，解耦 UI 与内部状态映射
    if (status == SYS_READY && !success) {
        strncpy(_ctx->prog.statusText, "寻组合失败 (无解)", 32);
    } else {
        switch (status) {
            case SYS_READY:         strncpy(_ctx->prog.statusText, "就绪", 32); break;
            case SYS_SEQ_DROP: {
                // 组合反馈：显示选中的 ID 列表和重量
                char idList[24] = "";
                for (int i = 0; i < (int)_selectedNodes.size(); i++) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "%d%s", _selectedNodes[i]->getId(), (i == (int)_selectedNodes.size() - 1) ? "" : ",");
                    strncat(idList, buf, sizeof(idList) - strlen(idList) - 1);
                }
                snprintf(_ctx->prog.statusText, 32, "下料:%s (%.1fg)", idList, weight);
                break;
            }
            case SYS_SEQ_CLOSE:     strncpy(_ctx->prog.statusText, "逐个关闭中", 32); break;
            case SYS_SETTLE_STABLE: strncpy(_ctx->prog.statusText, "沉降稳定中", 32); break;
            case SYS_BELT_A:        strncpy(_ctx->prog.statusText, "收集皮带运行", 32); break;
            case SYS_BELT_B:        strncpy(_ctx->prog.statusText, "步进输出运行", 32); break;
            default:                strncpy(_ctx->prog.statusText, "初始化", 32); break;
        }
    }

    if (mask != 0 || status == SYS_BELT_A || status == SYS_READY) {
        _ctx->prog.idMask = mask;
    }
    
    if (weight > 0.0f) {
        _ctx->prog.batchWeight = weight;
        _ctx->config.accumulatedWeight += weight;
        _ctx->prog.dirtyFlags |= DF_CONFIG; // 累计重量变化
        saveParams(); // 生产数据落盘
    }
    _ctx->prog.dirtyFlags |= DF_SYS_STATUS; // 状态文案或状态枚举变化
    xSemaphoreGive(_mutex);
}


void AppProduction::handlePolling() {
    if (_sequencer.isBusy()) return; // 序列执行期间强制挂起常规轮询

    uint8_t nextId = _currentPollId;
    for (int i = 0; i < 20; i++) {
        nextId = (nextId % 20) + 1;
        if (_nodeMgr->isWhitelisted(nextId)) break;
    }
    
    if (_nodeMgr->asyncUpdateNode(nextId)) {
        _currentPollId = nextId;
    }
}

void AppProduction::onExit() {
    Serial.println("[AppProduction] Production Mode Exited.");
    _sequencer.stop();
}

void AppProduction::updateTargets(float deltaBase, float deltaOffset) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    
    float currentBase = _ctx->config.targetMin;
    float currentOffset = _ctx->config.targetMax - _ctx->config.targetMin;

    float newBase = currentBase + deltaBase;
    float newOffset = currentOffset + deltaOffset;

    if (newBase < 0) newBase = 0;
    if (newOffset < 0) newOffset = 0;

    _ctx->config.targetMin = newBase;
    _ctx->config.targetMax = newBase + newOffset;
    _ctx->prog.dirtyFlags |= DF_CONFIG; // 目标值设置变化

    saveParams();
    xSemaphoreGive(_mutex);
}

void AppProduction::loadParams() {
    _nvs.begin("production", true);
    _ctx->config.targetMin = _nvs.getFloat("tmin", 170.0f); // 默认基准改为 170
    _ctx->config.targetMax = _nvs.getFloat("tmax", 180.0f); // 默认最大改为 180 (170+10)
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

void AppProduction::triggerGlobalTare() {
    // 启动 1-20 节点的置零序列 (CMD_TARE = 0x01)
    _sequencer.start(REG_CMD_CONTROL, CMD_TARE, 0, 0); // 0 为置零 UI 码
    strncpy(_ctx->prog.statusText, "正在执行全局置零...", 32);
    _ctx->prog.dirtyFlags |= DF_SYS_STATUS;
}

