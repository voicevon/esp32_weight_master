#include "WeightNode.h"

union FloatConverter {
    float f;
    uint16_t r[2];
};

WeightNode::WeightNode() {}

void WeightNode::init(int id, ModbusMaster* mb) {
    _id = id;
    _mb = mb;
    _retryCount = 0;
    _isAsyncActive = false;
    _status = NODE_DIRTY;
}

bool WeightNode::asyncUpdate() {
    if (!_mb || _isAsyncActive) return false;
    if (millis() - _lastUpdateTick < 10) return false; // 基础频率限制

    _isAsyncActive = true;
    bool success = _mb->asyncRead(_id, 0x0000, 8, [this](Modbus::ResultCode event, uint16_t tid, void* data) {
        this->_isAsyncActive = false;
        if (event == Modbus::EX_SUCCESS) {
            this->_online = true;
            this->_retryCount = 0;
            this->parseRegisters();
        } else {
            this->_online = false;
            this->_retryCount++;
        }
        return true;
    }, _registers);

    if (success) _lastUpdateTick = millis();
    else _isAsyncActive = false;

    return success;
}

bool WeightNode::asyncOpenServo() {
    if (!_mb || _isAsyncActive) return false;

    _isAsyncActive = true;
    return _mb->asyncWrite(_id, REG_CMD_CONTROL, CMD_SERVO_OPEN, [this](Modbus::ResultCode event, uint16_t tid, void* data) {
        this->_isAsyncActive = false;
        if (event == Modbus::EX_SUCCESS) {
            this->_servoOpen = true;
            this->_retryCount = 0;
        } else {
            this->_retryCount++;
            // 注意：重试触发逻辑由外部循环驱动，此处仅累加计数
        }
        return true;
    });
}

bool WeightNode::asyncCloseServo() {
    if (!_mb || _isAsyncActive) return false;

    _isAsyncActive = true;
    return _mb->asyncWrite(_id, REG_CMD_CONTROL, CMD_SERVO_CLOSE, [this](Modbus::ResultCode event, uint16_t tid, void* data) {
        this->_isAsyncActive = false;
        if (event == Modbus::EX_SUCCESS) {
            this->_servoOpen = false;
            this->_retryCount = 0;
        } else {
            this->_retryCount++;
        }
        return true;
    });
}

bool WeightNode::asyncTare() {
    if (!_mb || _isAsyncActive) return false;

    _isAsyncActive = true;
    return _mb->asyncWrite(_id, REG_CMD_CONTROL, CMD_TARE, [this](Modbus::ResultCode event, uint16_t tid, void* data) {
        this->_isAsyncActive = false;
        return true;
    });
}

void WeightNode::invalidate() {
    _weight = 0.0f;
    _stable = false;
    _status = NODE_DIRTY;
}

void WeightNode::parseRegisters() {
    FloatConverter conv;
    conv.r[1] = _registers[0]; // High Word
    conv.r[0] = _registers[1]; // Low Word
    _weight = conv.f;

    uint16_t statusReg = _registers[2];
    _stable = (statusReg >> 8) & 0x01;
    _doorPhase = (statusReg & 0xFF);
    
    // 简单的内部状态机迁移
    if (_status == NODE_DIRTY) {
        _status = NODE_REFRESHING;
    } else {
        if (_doorPhase == 0 && _stable) {
            _status = NODE_STABLE;
        } else {
            _status = NODE_REFRESHING;
        }
    }
}
