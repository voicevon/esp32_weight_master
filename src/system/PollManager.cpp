#include "PollManager.h"
#include <Preferences.h>
#include "SystemConfig.h"

// Union for weight parsing
union FloatConverter {
    float f;
    uint16_t r[2];
};

// 静态成员定义
PollManager* PollManager::_instance = nullptr;

PollManager::PollManager(ModbusMaster* master) : _mb(master), _curMode(MODE_IDLE) {
    _instance = this;
}

void PollManager::begin() {
    loadWhitelist();
}

void PollManager::setMode(OperationMode mode) {
    if (_curMode == mode) return;
    _curMode = mode;
    
    if (mode == MODE_DIAG_SCAN) {
        _scanProgress = 1;
        _scanCycle = 0;
        memset(_scanHistory, 0, sizeof(_scanHistory));
        Serial.println("[POLL] Scan Strategy Started (Cycles 0-4)");
    }
}

void PollManager::process() {
    // 核心解耦点：只有当驱动层真正空闲时，业务调度层才下发任务 (Phase 4 修正：允许所有终态状态)
    ModbusMaster::TransactionStatus status = _mb->getStatus();
    if (status != ModbusMaster::ST_IDLE && status != ModbusMaster::ST_SUCCESS && 
        status != ModbusMaster::ST_TIMEOUT && status != ModbusMaster::ST_ERROR) {
        return;
    }

    switch (_curMode) {
        case MODE_PRODUCTION:
            handleProductionPoll();
            break;
        case MODE_DIAG_SCAN:
            handleScanPoll();
            break;
        default:
            break;
    }
}

void PollManager::handleProductionPoll() {
    // 白名单过滤策略：寻找下一个在白名单的 ID
    uint8_t nextId = _currentPollId;
    for (int i = 0; i < 20; i++) {
        nextId = (nextId % 20) + 1;
        if (_nodes[nextId].whitelisted) break;
    }
    _currentPollId = nextId;

    // 下发原子读取指令 (地址 0x0000, 6 个寄存器)
    // 注意：ModbusMaster 将提供异步接口，回调通过 static helper 路由给实例
    // 显式传递所属节点的缓冲区物理地址
    // 库会将此地址返回给 onPollComplete 的 data 参数
    _mb->asyncRead(_currentPollId, 0x0000, 8, onPollComplete, _nodes[_currentPollId].registers);
}

void PollManager::handleScanPoll() {
    // 全量遍历策略：不看白名单，强制遍历 1-20
    _mb->asyncRead(_scanProgress, 0x0000, 8, onPollComplete, _nodes[_scanProgress].registers);
}

bool PollManager::onPollComplete(Modbus::ResultCode event, uint16_t transactionId, void* data) {
    // 使用 static _instance 指针访问实例，不再依赖全局变量名
    if (!_instance) return false;
    PollManager& pollManager = *_instance;
    
    uint8_t id = pollManager._curMode == MODE_DIAG_SCAN ? pollManager._scanProgress : pollManager._currentPollId;

    if (event == Modbus::EX_SUCCESS) {
        pollManager._nodes[id].online = true;
        pollManager._nodes[id].failCounter = 0;

        // 核心重构：data 指针现在由业务层(PollManager)显式提供，指向 NodeData.registers
        uint16_t* regs = (uint16_t*)data; 
        if (regs == nullptr) return false;

        // 根据寄存器回传地址反推 ID (更严谨的做法)
        // 但此处我们依然可以信任 transactionId 或全局单例，
        // 直到我们将 PollManager 改为非单例架构。
        pollManager.updateNodeFromRegisters(id, regs);

        if (pollManager._curMode == MODE_DIAG_SCAN) {
            pollManager._scanHistory[pollManager._scanCycle][id] = true;
        }
    } else {
        pollManager._nodes[id].online = false;
        pollManager._nodes[id].failCounter++;
        if (pollManager._curMode == MODE_DIAG_SCAN) {
            pollManager._scanHistory[pollManager._scanCycle][id] = false;
        }
    }

    // 扫描模式下的进度推进
    if (pollManager._curMode == MODE_DIAG_SCAN) {
        pollManager._scanProgress++;
        if (pollManager._scanProgress > 20) {
            pollManager._scanProgress = 1;
            pollManager._scanCycle++;
            if (pollManager._scanCycle >= 5) {
                // 5 轮扫完，生成最终白名单并退出扫描模式
                pollManager.saveWhitelist(); // 内部自动从 scanHistory 生成
                if (pollManager._bus) pollManager._bus->updateOperationMode(MODE_CONFIGURATION);
            }
        }
    }

    return true; 
}

void PollManager::updateNodeFromRegisters(int id, uint16_t* regs) {
    FloatConverter conv;
    conv.r[0] = regs[0];
    conv.r[1] = regs[1];
    _nodes[id].weight = conv.f;

    uint16_t statusReg = regs[2];
    _nodes[id].stable = (statusReg >> 8) & 0x01;
    _nodes[id].doorPhase = (statusReg & 0xFF);
    
    uint16_t currentDataId = regs[5];
    _nodes[id].lastDataId = currentDataId;

    // 生产业务 FSM 迁移至此
    if (_curMode == MODE_PRODUCTION) {
        if (_nodes[id].status == NODE_DIRTY) {
            _nodes[id].status = NODE_REFRESHING;
        } else {
            bool isDoorClosed = (_nodes[id].doorPhase == 0);
            if (isDoorClosed && _nodes[id].stable) {
                _nodes[id].status = NODE_STABLE;
            } else {
                _nodes[id].status = NODE_REFRESHING;
            }
        }
    }
}

// 数据访问接口 (Read-only)
float PollManager::getWeight(int id) const { return (id >= 1 && id <= 20) ? _nodes[id].weight : 0.0f; }
bool PollManager::isStable(int id) const { return (id >= 1 && id <= 20) ? _nodes[id].stable : false; }
uint8_t PollManager::getDoorPhase(int id) const { return (id >= 1 && id <= 20) ? _nodes[id].doorPhase : 0; }
bool PollManager::isOnline(int id) const { return (id >= 1 && id <= 20) ? _nodes[id].online : false; }
bool PollManager::isWhitelisted(int id) const { return (id >= 1 && id <= 20) ? _nodes[id].whitelisted : false; }
NodeStatus PollManager::getNodeStatus(int id) const { return (id >= 1 && id <= 20) ? _nodes[id].status : NODE_DIRTY; }
void PollManager::setNodeStatus(int id, NodeStatus s) { if (id >= 1 && id <= 20) _nodes[id].status = s; }

int PollManager::getUnstableCount() const {
    int count = 0;
    for (int i = 1; i <= 20; i++) if (_nodes[i].online && !_nodes[i].stable) count++;
    return count;
}

uint32_t PollManager::getWhitelistMask() const {
    uint32_t mask = 0;
    for (int i = 1; i <= 20; i++) if (_nodes[i].whitelisted) mask |= (1 << (i - 1));
    return mask;
}

void PollManager::saveWhitelist() {
    Preferences prefs;
    prefs.begin("modbus", false);
    int foundCount = 0;
    for (int i = 1; i <= 20; i++) {
        bool anyPass = false;
        for (int c = 0; c < 5; c++) if (_scanHistory[c][i]) { anyPass = true; break; }
        _nodes[i].whitelisted = anyPass;
        if (anyPass) foundCount++;

        char key[8];
        snprintf(key, sizeof(key), "wl%d", i);
        prefs.putBool(key, anyPass);
    }
    prefs.end();
    Serial.printf("[POLL] Whitelist Saved. Found %d nodes.\n", foundCount);
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
