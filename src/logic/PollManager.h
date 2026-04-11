#ifndef POLL_MANAGER_H
#define POLL_MANAGER_H

#include <Arduino.h>
#include "drivers/ModbusMaster.h"
#include "system/SystemTypes.h"
#include "ui/I_Command_Bus.h"

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

    // 核心接口：由外部 App 调用
    void process(); 
    bool asyncUpdateNode(int id);
    
    // 数据查询接口 (UI/业务逻辑调用)
    float getWeight(int id) const;
    bool isStable(int id) const;
    uint8_t getDoorPhase(int id) const;
    bool isOnline(int id) const;
    bool isWhitelisted(int id) const;
    NodeStatus getNodeStatus(int id) const;
    void setNodeStatus(int id, NodeStatus s);
    void setWhitelisted(int id, bool w);
    void setServoState(int id, bool open);
    void invalidateNode(int id);
    
    // 聚合统计
    int getUnstableCount() const;
    uint32_t getWhitelistMask() const;

    // 批量同步接口 (Phase 4 性能优化)
    void fillUISnapshot(UISnapshot& snapshot) const;

    // 白名单持久化
    void saveWhitelist();
    void loadWhitelist();

private:
    ModbusMaster* _mb;

    // 节点数据缓存 (1-20)
    struct NodeData {
        uint16_t registers[8];   
        float weight = 0.0f;
        bool stable = false;
        uint8_t doorPhase = 0;
        bool online = false;
        bool whitelisted = true;
        bool servoOpen = false; 
        uint8_t failCounter = 0;
        uint16_t lastDataId = 0;
        NodeStatus status = NODE_DIRTY;
    } _nodes[21];
    unsigned long _lastRequestTime = 0;

    // static 回调实例指针
    static PollManager* _instance;

    ICommandBus* _bus = nullptr;
    void updateNodeFromRegisters(int id, uint16_t* regs);
    static bool onPollComplete(Modbus::ResultCode event, uint16_t transactionId, void* data);
};

#endif
