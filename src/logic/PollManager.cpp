#include "PollManager.h"
#include <Preferences.h>
#include "system/SystemConfig.h"

// Union for weight parsing
union FloatConverter {
    float f;
    uint16_t r[2];
};

// 静态成员定义
PollManager* PollManager::_instance = nullptr;

PollManager::PollManager(ModbusMaster* master) : _mb(master) {
    _instance = this;
}

void PollManager::begin() {
    loadWhitelist();
}

void PollManager::process() {
    // PollManager 现在仅负责基础的总线维护（如果有需要）
    // 目前 process() 可以保持空，业务由 App 驱动
}

bool PollManager::asyncUpdateNode(int id) {
    if (id < 1 || id > 20) return false;
    
    // 基础的频率控制 (10ms 帧间隙)
    if (millis() - _lastRequestTime < 10) return false;

    if (_mb->asyncRead(id, 0x0000, 8, onPollComplete, _nodes[id].registers)) {
        _lastRequestTime = millis();
        return true;
    }
    return false;
}

bool PollManager::onPollComplete(Modbus::ResultCode event, uint16_t transactionId, void* data) {
    if (!_instance) return false;
    PollManager& pollManager = *_instance;
    
    uint8_t id = (uint8_t)transactionId;
    if (id < 1 || id > 20) return false;

    if (event == Modbus::EX_SUCCESS) {
        pollManager._nodes[id].online = true;
        pollManager._nodes[id].failCounter = 0;

        uint16_t* regs = (uint16_t*)data; 
        if (regs != nullptr) {
            pollManager.updateNodeFromRegisters(id, regs);
        }
    } else {
        pollManager._nodes[id].online = false;
        pollManager._nodes[id].failCounter++;
    }

    return true; 
}

void PollManager::updateNodeFromRegisters(int id, uint16_t* regs) {
    FloatConverter conv;
    conv.r[1] = regs[0]; // High Word
    conv.r[0] = regs[1]; // Low Word
    _nodes[id].weight = conv.f;

    uint16_t statusReg = regs[2];
    _nodes[id].stable = (statusReg >> 8) & 0x01;
    _nodes[id].doorPhase = (statusReg & 0xFF);
    
    _nodes[id].lastDataId = regs[5];

    // 状态机精简：不再在这里判断 PRODUCTION 逻辑
    if (_nodes[id].status == NODE_DIRTY) {
        _nodes[id].status = NODE_REFRESHING;
    } else {
        if (_nodes[id].doorPhase == 0 && _nodes[id].stable) {
            _nodes[id].status = NODE_STABLE;
        } else {
            _nodes[id].status = NODE_REFRESHING;
        }
    }
}

float PollManager::getWeight(int id) const { return (id >= 1 && id <= 20) ? _nodes[id].weight : 0.0f; }
bool PollManager::isStable(int id) const { return (id >= 1 && id <= 20) ? _nodes[id].stable : false; }
uint8_t PollManager::getDoorPhase(int id) const { return (id >= 1 && id <= 20) ? _nodes[id].doorPhase : 0; }
bool PollManager::isOnline(int id) const { return (id >= 1 && id <= 20) ? _nodes[id].online : false; }
bool PollManager::isWhitelisted(int id) const { return (id >= 1 && id <= 20) ? _nodes[id].whitelisted : false; }
NodeStatus PollManager::getNodeStatus(int id) const { return (id >= 1 && id <= 20) ? _nodes[id].status : NODE_DIRTY; }
void PollManager::setNodeStatus(int id, NodeStatus s) { if (id >= 1 && id <= 20) _nodes[id].status = s; }
void PollManager::setWhitelisted(int id, bool w) { if (id >= 1 && id <= 20) _nodes[id].whitelisted = w; }
void PollManager::setServoState(int id, bool open) { if (id >= 1 && id <= 20) _nodes[id].servoOpen = open; }
void PollManager::invalidateNode(int id) {
    if (id >= 1 && id <= 20) {
        _nodes[id].weight = 0.0f;
        _nodes[id].stable = false;
        _nodes[id].status = NODE_DIRTY;
    }
}

int PollManager::getUnstableCount() const {
    int count = 0;
    for (int i = 1; i <= 20; i++) if (_nodes[i].online && !_nodes[i].stable) count++;
    return count;
}

void PollManager::fillUISnapshot(UISnapshot& snapshot) const {
    float sSum = 0, uSum = 0;
    for (int i = 1; i <= 20; i++) {
        snapshot.currentWeights[i]   = _nodes[i].weight;
        snapshot.stableNodes[i]      = _nodes[i].stable;
        snapshot.onlineNodes[i]      = _nodes[i].online;
        snapshot.whitelistedNodes[i] = _nodes[i].whitelisted;
        
        if (!_nodes[i].online) {
            snapshot.servoRealStates[i] = -1; 
        } else {
            snapshot.servoRealStates[i] = _nodes[i].servoOpen ? 1 : 0;
        }

        if (_nodes[i].online && _nodes[i].whitelisted) {
            if (_nodes[i].stable) sSum += _nodes[i].weight;
            else uSum += _nodes[i].weight;
        }
    }
    snapshot.stableWeightSum = sSum;
    snapshot.unstableWeightSum = uSum;

    // 扫描数据现在由 AppDispatcher 从 AppScan 获取，这里不再负责填充相关字段
}

uint32_t PollManager::getWhitelistMask() const {
    uint32_t mask = 0;
    for (int i = 1; i <= 20; i++) if (_nodes[i].whitelisted) mask |= (1 << (i - 1));
    return mask;
}

void PollManager::saveWhitelist() {
    Preferences prefs;
    prefs.begin("modbus", false);
    for (int i = 1; i <= 20; i++) {
        char key[8];
        snprintf(key, sizeof(key), "wl%d", i);
        prefs.putBool(key, _nodes[i].whitelisted);
    }
    prefs.end();
}

void PollManager::loadWhitelist() {
    Preferences prefs;
    prefs.begin("modbus", true);
    for (int i = 1; i <= 20; i++) {
        char key[8];
        snprintf(key, sizeof(key), "wl%d", i);
        _nodes[i].whitelisted = prefs.getBool(key, true);
    }
    prefs.end();
}
