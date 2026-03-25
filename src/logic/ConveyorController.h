#ifndef CONVEYOR_CONTROLLER_H
#define CONVEYOR_CONTROLLER_H

#include "system/ModbusMaster.h"

class ConveyorController {
public:
    ConveyorController(ModbusMaster* rs485, int motorId1, int motorId2);
    void begin();
    
    // 分阶段传输逻辑
    void collectFromUnits(); // 一级带：长位移收料并转运至二级带入料口
    void advanceOutput();    // 二级带：短位移步进步进
    
    // 获取电机当前是否在运行（可通过位置查询或状态寄存器）
    bool isMoving();

private:
    ModbusMaster* _rs485;
    int _id1, _id2;
    long _currentPos1, _currentPos2;
};

#endif
