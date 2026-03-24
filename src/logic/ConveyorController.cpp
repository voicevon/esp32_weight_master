#include "ConveyorController.h"
#include <Arduino.h>

ConveyorController::ConveyorController(Rs485Master* rs485, int motorId1, int motorId2)
    : _rs485(rs485), _id1(motorId1), _id2(motorId2), _currentPos1(0), _currentPos2(0) {}

void ConveyorController::begin() {
    // 初始伺服处于静止状态，位置置零 (根据实际硬件情况可增加 Homing 逻辑)
}

void ConveyorController::collectFromUnits() {
    // 【Stage 1】 一级带：收料并转运至末端倾倒。
    // 假定转运全长需要 30000 脉冲 (约 120cm 转动距离)。
    _currentPos1 += 30000;
    _rs485->setPosition(_id1, _currentPos1, 200); // 较低速运行，防止震动掉出。
}

void ConveyorController::advanceOutput() {
    // 【Stage 2】 二级带：向前步进一个格位（20cm = 5000 脉冲）。
    _currentPos2 += 5000;
    _rs485->setPosition(_id2, _currentPos2, 400); // 快速步进位移。
}

bool ConveyorController::isMoving() {
    // 后期可扩展 Modbus 实时状态读取
    return false;
}
