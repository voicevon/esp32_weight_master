#include "Belt.h"
#include <Arduino.h>

Belt::Belt(ModbusMaster* rs485, uint8_t motorId, BeltMode mode, uint16_t defaultSpeed)
    : _rs485(rs485), _id(motorId), _status(BELT_OFFLINE), _mode(mode), _speed(defaultSpeed) {
}

void Belt::begin() {
    _taskQueue.clear();
    _status = BELT_OFFLINE;
}

void Belt::pushTask(uint16_t reg, uint16_t val, bool isRead, std::function<void(bool)> onDone) {
    _taskQueue.push_back({reg, val, isRead, onDone});
}

void Belt::setSpeed(uint16_t speed) {
    _speed = speed;
    if (_mode == MODE_SPEED) {
        pushTask(REG_SPEED_SET, speed, false);
    }
}

void Belt::moveRelative(int32_t pulses) {
    _status = BELT_MOVING;

    // 拆分圈数和圈内脉冲数
    int16_t revs = pulses / PULSES_PER_REV;
    int16_t pls  = pulses % PULSES_PER_REV;
    
    // 顺序压入 5 个指令任务
    pushTask(REG_POS1_REV, (uint16_t)revs, false);
    pushTask(REG_POS1_PULSE, (uint16_t)pls, false);
    pushTask(REG_POS1_SPEED, _speed, false); // 使用成员变量中存储的速度
    pushTask(REG_VIRTUAL_IO, 0, false);
    pushTask(REG_VIRTUAL_IO, 1, false);
}

void Belt::moveDistanceMm(int32_t mm) {
    int32_t pulses = mm * PULSES_PER_MM;
    moveRelative(pulses);
}

void Belt::positionPause() {
    if (_mode != MODE_POSITION) return;
    pushTask(REG_VIRTUAL_IO, 0, false);
    _status = BELT_READY;
}

void Belt::positionResume() {
    if (_mode != MODE_POSITION) return;
    pushTask(REG_VIRTUAL_IO, 1, false);
    _status = BELT_MOVING;
}

void Belt::speedRun(bool forward) {
    if (_mode != MODE_SPEED) return;
    _status = BELT_MOVING;
    pushTask(0x0015, _speed, false);
    pushTask(REG_VIRTUAL_IO, forward ? 1 : 2, false);
}

void Belt::speedStop() {
    if (_mode != MODE_SPEED) return;
    pushTask(REG_VIRTUAL_IO, 0, false);
    _status = BELT_READY;
}

void Belt::stop() {
    _taskQueue.clear();
    
    if (_mode == MODE_SPEED) {
        speedStop();
    } else {
        positionPause();
    }
}

void Belt::scan(std::function<void(bool)> onComplete) {
    // 扫描任务逻辑
    auto wrapCb = [this, onComplete](bool success) {
        if (success) {
            this->_status = BELT_READY;
        } else {
            this->_status = BELT_OFFLINE;
        }
        if (onComplete) onComplete(success);
    };

    pushTask(REG_POS1_REV, 1, true, wrapCb);
}

void Belt::update() {
    if (_taskQueue.empty()) {
        // 如果原本是运行中且指令发完，这里可以做一个初步的状态回退（或维持运行由业务确认）
        if (_status == BELT_MOVING) {
            // 注意：这里只是指令“发完”，并不代表电机“停稳”。
            // 工业逻辑中通常需要查询 0x011F 或位置偏差，这里暂维持原样。
        }
        return;
    }

    // 只有总线物理空闲/成功/报错时，才发起队列中的下一条
    auto mbStatus = _rs485->getStatus();
    if (mbStatus == ModbusMaster::ST_WAITING) return;

    // 获取任务副本（暂不弹出）
    BeltTask& task = _taskQueue.front();

    bool sent = false;
    if (task.isRead) {
        sent = _rs485->asyncRead(_id, task.reg, task.value, [task](Modbus::ResultCode res, uint16_t tid, void* data) {
            if (task.onDone) task.onDone(res == Modbus::EX_SUCCESS);
            return true;
        }, &_scanBuffer);
    } else {
        sent = _rs485->asyncWrite(_id, task.reg, task.value, [task](Modbus::ResultCode res, uint16_t tid, void* data) {
            if (task.onDone) task.onDone(res == Modbus::EX_SUCCESS);
            return true;
        });
    }

    // 只有在指令成功夺取总线并发出后，才将其从队列中移除
    if (sent) {
        if (_id == 21 || _id == 22) { // 仅跟踪皮带电机 (ID 21, 22)
            Serial.printf("[Belt] ID:%d CMD:0x%02X REG:0x%04X Sent.\n", _id, task.isRead ? 0x03 : 0x06, task.reg);
        }
        _taskQueue.pop_front();
    }
}
