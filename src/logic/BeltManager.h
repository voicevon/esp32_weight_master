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
     */
    void collectFromUnits();
    
    /**
     * @brief 二级带：短位移步进步进
     */
    void advanceOutput();
    
    /**
     * @brief 获取电机当前是否在运行
     */
    bool isMoving();

private:
    ModbusMaster* _rs485;
    int _id1, _id2;
    uint32_t _currentPos1, _currentPos2;
};

#endif // BELT_MANAGER_H
