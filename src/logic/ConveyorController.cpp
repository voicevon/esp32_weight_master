#include "ConveyorController.h"
#include <Arduino.h>

ConveyorController::ConveyorController(ModbusMaster* rs485, int motorId1, int motorId2)
    : _rs485(rs485), _id1(motorId1), _id2(motorId2), _currentPos1(0), _currentPos2(0) {}

void ConveyorController::begin() {
    // 初始伺服处于静止状态，位置置零
}

void ConveyorController::collectFromUnits() {
    _currentPos1 += 30000;
    // 使用通用同步写入接口 (地址 0x0200 为伺服位置寄存器)
    _rs485->syncWrite(_id1, 0x0200, (uint16_t)(_currentPos1 % 65536)); 
}

void ConveyorController::advanceOutput() {
    _currentPos2 += 5000;
    // 使用通用同步写入接口 (地址 0x0200 为伺服位置寄存器)
    _rs485->syncWrite(_id2, 0x0200, (uint16_t)(_currentPos2 % 65536)); 
}

bool ConveyorController::isMoving() {
    return false;
}
