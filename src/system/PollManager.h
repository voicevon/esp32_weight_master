#ifndef POLL_MANAGER_H
#define POLL_MANAGER_H

#include <Arduino.h>
#include "ModbusMaster.h"
#include "SystemTypes.h"
#include "I_Command_Bus.h"

/**
 * @class PollManager
 * @brief 业务调度逻辑层：负责轮询策略（扫描/生产）、数据缓存和白名单管理。
 * 符合 SRP 原则，将通讯逻辑以外的所有业务决策集中于此。
 */
class PollManager {
public:
    PollManager(ModbusMaster* master);
    void begin();
    void setCommandBus(ICommandBus* bus) { _bus = bus; }

    // 核心调度接口：在业务循环中非阻塞调用
    void process(); 
    void setMode(OperationMode mode);
    OperationMode getMode() const { return _curMode; }
    
    // 数据查询接口 (UI/业务逻辑调用)
    float getWeight(int id) const;
    bool isStable(int id) const;
    uint8_t getDoorPhase(int id) const;
    bool isOnline(int id) const;
    bool isWhitelisted(int id) const;
    NodeStatus getNodeStatus(int id) const;
    void setNodeStatus(int id, NodeStatus s);
    
    // 聚合统计
    int getUnstableCount() const;
    uint32_t getWhitelistMask() const;
    int getScanProgress() const { return _scanProgress; }
    int getScanCycle() const { return _scanCycle; }
    bool getScanHistory(int cycle, int id) const { 
        return (cycle >= 0 && cycle < 5 && id >= 1 && id <= 20) ? _scanHistory[cycle][id] : false; 
    }

    // 批量同步接口 (Phase 4 性能优化: 减少 API 调用开销)
    void fillUISnapshot(UISnapshot& snapshot) const;

    // 白名单持久化
    void saveWhitelist();
    void loadWhitelist();

private:
    ModbusMaster* _mb;
    OperationMode _curMode = MODE_IDLE;

    // 节点数据缓存 (1-20)
    struct NodeData {
        uint16_t registers[8];   // [新] 显式的寄存器落地缓冲区，确保异步安全
        float weight = 0.0f;
        bool stable = false;
        uint8_t doorPhase = 0;
        bool online = false;
        bool whitelisted = true;
        uint8_t failCounter = 0;
        uint16_t lastDataId = 0;
        NodeStatus status = NODE_DIRTY;
    } _nodes[21];
    unsigned long _lastRequestTime = 0;

    // 扫描状态机变量
    int _scanProgress = 1;
    int _scanCycle = 0;
    bool _scanHistory[5][21]; // 5 轮扫描的生还记录

    // 轮询 ID 管理
    uint8_t _currentPollId = 1;
    unsigned long _lastCycleStartTime = 0;
    int _whitelistedInCycle = 0;
    int _processedInCycle = 0;

    // static 回调实例指针（替代 extern 全局单例依赖）
    static PollManager* _instance;

    // 内部帮助函数
    ICommandBus* _bus = nullptr;
    void handleProductionPoll();
    void handleScanPoll();
    void updateNodeFromRegisters(int id, uint16_t* regs);
    static bool onPollComplete(Modbus::ResultCode event, uint16_t transactionId, void* data);
};

#endif
