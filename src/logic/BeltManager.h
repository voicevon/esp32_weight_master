#ifndef BELT_MANAGER_H
#define BELT_MANAGER_H

#include <functional>
#include "drivers/ModbusMaster.h"

enum BeltStatus {
    BELT_OFFLINE = 0, // 初始/离线
    BELT_READY   = 1, // 在线就绪
    BELT_MOVING  = 2, // 正在运转
    BELT_FAULT   = 3  // 电机报错/超时
};

/**
 * @class BeltManager
 * @brief 皮带输送协作管理类。
 * 负责控制一级收料皮带和二级输出皮带的运动逻辑。
 */
class BeltManager {
public:
    BeltManager(ModbusMaster* rs485, int motorId1, int motorId2);
    void begin();
    
    /**
     * @brief 异步触发所有皮带的在线扫描
     * @param onComplete 扫描完成后调用的回调
     */
    void scanAll(std::function<void()> onComplete = nullptr);

    /**
     * @brief 获取皮带状态
     * @param index 0: 一级带, 1: 二级带
     */
    BeltStatus getStatus(int index) const;

    /**
     * @brief 一级带：长位移收料并转运至二级带入料口
     */
    void collectFromUnits(int32_t pulses = 30000);
    
    /**
     * @brief 二级带：短位移步进步进
     */
    void advanceOutput(int32_t pulses = 5000);
    
    /**
     * @brief 获取电机当前是否在运行
     */
    bool isMoving() const;

    // 辅助测试接口：按毫米为单位控制特定皮带
    void moveDistanceMm(uint8_t id, int32_t mm);

    int getMotorId(int index) const { return index == 0 ? _id1 : _id2; }

private:
    ModbusMaster* _rs485;
    int _id1, _id2;

    struct BeltNode {
        uint8_t id;
        BeltStatus status;
        uint16_t scanBuffer;
        bool finished;
    } _nodes[2];

    bool _isScanning = false;
    std::function<void()> _onScanComplete = nullptr;

    // 底层相对位移发送逻辑
    void moveRelative(uint8_t id, int32_t pulses);
    
    // 内部扫描回调
    void handleInternalScanResult(int index, Modbus::ResultCode result);
};

#endif // BELT_MANAGER_H
