#include "Belt.h"

Belt::Belt(ModbusMaster* rs485, uint8_t motorId)
    : _rs485(rs485), _id(motorId), _status(BELT_OFFLINE) {
}

void Belt::begin() {
    // 初始化逻辑：目前伺服重启后已根据物理参数配置为增量式位置指令模式 (P4-0=1)
}

void Belt::moveRelative(int32_t pulses) {
    if (!_rs485) return;
    
    _status = BELT_MOVING;

    // 拆分圈数和圈内脉冲数
    int16_t revs = pulses / PULSES_PER_REV;
    int16_t pls  = pulses % PULSES_PER_REV;
    
    // 指令发送回调 (目前仅用于确认发送)
    auto logCb = [this](Modbus::ResultCode res, uint16_t tid, void* data) { 
        return true; 
    };

    // 1. 设置指令 1 的相对圈数和脉冲数
    _rs485->asyncWrite(_id, REG_POS1_REV, (uint16_t)revs, logCb);
    _rs485->asyncWrite(_id, REG_POS1_PULSE, (uint16_t)pls, logCb);
    
    // 2. 触发虚拟端子运行 (0x011F 先写 0 复位沿信号，再写 1 触发位置 1)
    _rs485->asyncWrite(_id, REG_VIRTUAL_IO, 0, logCb);
    _rs485->asyncWrite(_id, REG_VIRTUAL_IO, 1, logCb);
}

void Belt::moveDistanceMm(int32_t mm) {
    int32_t pulses = mm * PULSES_PER_MM;
    moveRelative(pulses);
}

void Belt::scan(std::function<void(bool)> onComplete) {
    if (!_rs485) return;

    _status = BELT_OFFLINE; // 扫描前先重置
    _rs485->asyncRead(_id, REG_POS1_REV, 1, [this, onComplete](Modbus::ResultCode res, uint16_t tid, void* data) {
        bool success = (res == Modbus::EX_SUCCESS);
        if (success) {
            this->_status = BELT_READY;
        } else {
            this->_status = (res == Modbus::EX_TIMEOUT) ? BELT_OFFLINE : BELT_FAULT;
        }
        if (onComplete) onComplete(success);
        return true;
    }, &_scanBuffer);
}
