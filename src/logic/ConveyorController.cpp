#include "ConveyorController.h"
#include <Arduino.h>

ConveyorController::ConveyorController(ModbusMaster* rs485, int motorId1, int motorId2)
    : _rs485(rs485), _id1(motorId1), _id2(motorId2), _currentPos1(0), _currentPos2(0) {}

void ConveyorController::begin() {
    // 初始伺服处于静止状态，位置置零 (根据实际硬件情况可增加 Homing 逻辑)
}

void ConveyorController::collectFromUnits() {
    _currentPos1 += 30000;
    _rs485->setPosition(_id1, _currentPos1, 200); 
}

void ConveyorController::advanceOutput() {
    _currentPos2 += 5000;
    _rs485->setPosition(_id2, _currentPos2, 400); 
}

bool ConveyorController::isMoving() {
    return false;
}
