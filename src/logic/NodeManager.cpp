#include "NodeManager.h"
#include <Preferences.h>

NodeManager::NodeManager(ModbusMaster* master) : _mb(master) {
}

void NodeManager::begin() {
    for (int i = 1; i <= 20; i++) {
        _nodes[i].init(i, _mb);
    }
    loadWhitelist();
}

bool NodeManager::asyncUpdateNode(int id) {
    if (id < 1 || id > 20) return false;
    return _nodes[id].asyncUpdate();
}

float NodeManager::getWeight(int id) const {
    return (id >= 1 && id <= 20) ? _nodes[id].getWeight() : 0.0f;
}

bool NodeManager::isStable(int id) const {
    return (id >= 1 && id <= 20) ? _nodes[id].isStable() : false;
}

bool NodeManager::isOnline(int id) const {
    return (id >= 1 && id <= 20) ? _nodes[id].isOnline() : false;
}

bool NodeManager::isWhitelisted(int id) const {
    return (id >= 1 && id <= 20) ? _nodes[id].isWhitelisted() : false;
}

NodeStatus NodeManager::getNodeStatus(int id) const {
    return (id >= 1 && id <= 20) ? _nodes[id].getStatus() : NODE_DIRTY;
}

void NodeManager::setNodeStatus(int id, NodeStatus s) {
    if (id >= 1 && id <= 20) _nodes[id].setStatus(s);
}

void NodeManager::setWhitelisted(int id, bool w) {
    if (id >= 1 && id <= 20) _nodes[id].setWhitelisted(w);
}

void NodeManager::setServoState(int id, bool open) {
    // 逻辑层模拟状态，实际反馈由异步读取决定
    // 暂时保持空，因为 WeightNode 内部有缓存
}

void NodeManager::invalidateNode(int id) {
    if (id >= 1 && id <= 20) _nodes[id].invalidate();
}

int NodeManager::getUnstableCount() const {
    int count = 0;
    for (int i = 1; i <= 20; i++) {
        if (_nodes[i].isOnline() && !_nodes[i].isStable()) count++;
    }
    return count;
}

uint32_t NodeManager::getWhitelistMask() const {
    uint32_t mask = 0;
    for (int i = 1; i <= 20; i++) {
        if (_nodes[i].isWhitelisted()) mask |= (1 << (i - 1));
    }
    return mask;
}

void NodeManager::fillUISnapshot(UISnapshot& snapshot) const {
    float sSum = 0, uSum = 0;
    for (int i = 1; i <= 20; i++) {
        snapshot.currentWeights[i]   = _nodes[i].getWeight();
        snapshot.stableNodes[i]      = _nodes[i].isStable();
        snapshot.onlineNodes[i]      = _nodes[i].isOnline();
        snapshot.whitelistedNodes[i] = _nodes[i].isWhitelisted();
        
        if (snapshot.curMode != 5) { // MODE_SERVO_TEST = 5
            if (!_nodes[i].isOnline()) {
                snapshot.servoRealStates[i] = -1; 
            } else {
                snapshot.servoRealStates[i] = _nodes[i].isServoOpen() ? 1 : 0;
            }
        }

        if (_nodes[i].isOnline() && _nodes[i].isWhitelisted()) {
            if (_nodes[i].isStable()) sSum += _nodes[i].getWeight();
            else uSum += _nodes[i].getWeight();
        }
    }
    snapshot.stableWeightSum = sSum;
    snapshot.unstableWeightSum = uSum;
}

void NodeManager::saveWhitelist() {
    Preferences prefs;
    prefs.begin("modbus", false);
    for (int i = 1; i <= 20; i++) {
        char key[8];
        snprintf(key, sizeof(key), "wl%d", i);
        prefs.putBool(key, _nodes[i].isWhitelisted());
    }
    prefs.end();
}

void NodeManager::loadWhitelist() {
    Preferences prefs;
    prefs.begin("modbus", true);
    for (int i = 1; i <= 20; i++) {
        char key[8];
        snprintf(key, sizeof(key), "wl%d", i);
        _nodes[i].setWhitelisted(prefs.getBool(key, true));
    }
    prefs.end();
}
