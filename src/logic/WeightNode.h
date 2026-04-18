#ifndef WEIGHT_NODE_H
#define WEIGHT_NODE_H

#include <Arduino.h>
#include "drivers/ModbusMaster.h"
#include "system/SystemTypes.h"
#include "system/SystemConfig.h"

/**
 * @class WeightNode
 * @brief 代表单个称重单元实体。
 * 封装了节点的通讯、状态管理、重试逻辑及数据解析。
 */
class WeightNode {
public:
    WeightNode();
    
    /**
     * @brief 初始化节点
     * @param id Modbus 物理 ID (1-20)
     * @param mb 共享的 ModbusMaster 指针
     */
    void init(int id, ModbusMaster* mb);

    // --- 异步操作接口 ---
    bool asyncUpdate();        // 触发重量/状态轮询
    bool asyncOpenServo();     // 触发开门动作 (含内部重试)
    bool asyncCloseServo();    // 触发关门动作
    bool asyncTare();          // 触发去皮动作

    // --- 数据获取接口 ---
    int   getId() const { return _id; }
    float getWeight() const { return _weight; }
    bool  isStable() const { return _stable; }
    bool  isOnline() const { return _online; }
    bool  isWhitelisted() const { return _whitelisted; }
    bool  isServoOpen() const { return _servoOpen; }
    int   getRetryCount() const { return _retryCount; }
    NodeStatus getStatus() const { return _status; }

    // --- 业务控制接口 ---
    void setWhitelisted(bool w) { _whitelisted = w; }
    void setStatus(NodeStatus s) { _status = s; }
    void invalidate();         // 作废当前数据 (下料后调用)

    // --- 内部回调适配器 ---
    static bool onPollStatic(Modbus::ResultCode event, uint16_t tid, void* data);
    static bool onActionStatic(Modbus::ResultCode event, uint16_t tid, void* data);

private:
    int            _id = 0;
    ModbusMaster*  _mb = nullptr;

    // 状态数据
    float      _weight = 0.0f;
    bool       _stable = false;
    bool       _online = false;
    bool       _whitelisted = true;
    bool       _servoOpen = false;
    uint8_t    _doorPhase = 0;
    NodeStatus _status = NODE_DIRTY;

    // 通讯管理
    uint16_t      _registers[8];
    int           _retryCount = 0;
    bool          _isAsyncActive = false;
    unsigned long _lastUpdateTick = 0;

    void parseRegisters();
    void handleActionCallback(Modbus::ResultCode event);
};

#endif // WEIGHT_NODE_H
