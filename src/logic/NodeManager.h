#ifndef NODE_MANAGER_H
#define NODE_MANAGER_H

#include <Arduino.h>
#include "drivers/ModbusMaster.h"
#include "logic/WeightNode.h"
#include "system/SystemTypes.h"
#include "ui/I_Command_Bus.h"

/**
 * @class NodeManager
 * @brief 节点管理器：持有并管理所有 WeightNode 实例。
 * 替代原有的 PollManager，符合对象化架构。
 */
class NodeManager {
public:
    NodeManager(ModbusMaster* master);
    void begin();
    void setCommandBus(ICommandBus* bus) { _bus = bus; }

    // --- 核心接口 ---
    bool asyncUpdateNode(int id);
    
    // --- 节点访问接口 (代理到 WeightNode) ---
    WeightNode* getNode(int id) { return (id >= 1 && id <= 20) ? &_nodes[id] : nullptr; }
    float getWeight(int id) const;
    bool  isStable(int id) const;
    bool  isOnline(int id) const;
    bool  isWhitelisted(int id) const;
    NodeStatus getNodeStatus(int id) const;

    void setNodeStatus(int id, NodeStatus s);
    void setWhitelisted(int id, bool w);
    void setServoState(int id, bool open);
    void invalidateNode(int id);
    
    // --- 聚合统计与同步 ---
    int getUnstableCount() const;
    uint32_t getWhitelistMask() const;
    void fillUISnapshot(UISnapshot& snapshot) const;

    // --- 持久化 ---
    void saveWhitelist();
    void loadWhitelist();

private:
    ModbusMaster* _mb;
    WeightNode    _nodes[21]; // 索引 1-20
    ICommandBus*  _bus = nullptr;
};

#endif // NODE_MANAGER_H
