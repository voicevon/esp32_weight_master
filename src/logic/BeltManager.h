#ifndef BELT_MANAGER_H
#define BELT_MANAGER_H

#include "drivers/ModbusMaster.h"

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
     * @brief 一级带：长位移收料并转运至二级带入料口
     * @param pulses 相对移动脉冲数（默认 30000）
     */
    void collectFromUnits(int32_t pulses = 30000);
    
    /**
     * @brief 二级带：短位移步进步进
     * @param pulses 相对移动脉冲数（默认 5000）
     */
    void advanceOutput(int32_t pulses = 5000);
    
    /**
     * @brief 获取电机当前是否在运行
     */
    bool isMoving();

    // 辅助测试接口：按毫米为单位控制特定皮带
    void moveDistanceMm(uint8_t id, int32_t mm);

    int getMotorId(int index) const { return index == 0 ? _id1 : _id2; }

private:
    ModbusMaster* _rs485;
    int _id1, _id2;

    // 底层相对位移发送逻辑
    void moveRelative(uint8_t id, int32_t pulses);
};

#endif // BELT_MANAGER_H
