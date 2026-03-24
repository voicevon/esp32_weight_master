#include "Rs485Master.h"

// 静态转换 union，用于跨函数解析 Float
union FloatConverter {
    float f;
    uint16_t r[2];
};

Rs485Master::Rs485Master(int rxPin, int txPin, int enPin, long baud)
    : _rxPin(rxPin), _txPin(txPin), _enPin(enPin), _baud(baud) {
    for (int i = 0; i < 21; i++) {
        _cachedWeights[i] = 0.0f;
        _onlineStatus[i] = false;
    }
}

void Rs485Master::begin() {
    Serial2.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    _mb.begin(&Serial2, _enPin);
    _mb.master();
}

// ---------------------------
// 异步指令轮询逻辑
// ---------------------------
void Rs485Master::update() {
    _mb.task(); // 维持底层协议栈

    // 如果当前正在等待响应，且未超时，则直接返回
    if (_isWaiting) {
        if (millis() - _lastPollTime > 200) { // 200ms 防卡死超时
            _onlineStatus[_currentPollId] = false;
            _isWaiting = false;
            _currentPollId = (_currentPollId % 20) + 1; // 尝试下一个
        }
        return;
    }

    // 空闲状态：发起下一个 ID 的查询
    _isWaiting = true;
    _lastPollTime = millis();
    _mb.readHreg(_currentPollId, REG_WEIGHT_H, _tempRegs, 2, cbPoll, this);
}

// 静态回调：数据接收到位后在此处更新缓存
bool Rs485Master::cbPoll(Modbus::ResultCode event, uint16_t transactionId, void* data) {
    Rs485Master* instance = (Rs485Master*)data;
    
    if (event == Modbus::EX_SUCCESS) {
        FloatConverter conv;
        conv.r[0] = instance->_tempRegs[0];
        conv.r[1] = instance->_tempRegs[1];
        
        uint8_t id = instance->_currentPollId;
        instance->_cachedWeights[id] = conv.f;
        instance->_onlineStatus[id] = true;
    } else {
        instance->_onlineStatus[instance->_currentPollId] = false;
    }

    // 释放标志，轮换 ID
    instance->_isWaiting = false;
    instance->_currentPollId = (instance->_currentPollId % 20) + 1;
    return true;
}

float Rs485Master::getWeight(int id) {
    if (id < 1 || id > 20) return -1.0f;
    return _cachedWeights[id]; // 立即返回缓存
}

bool Rs485Master::isNodeOnline(int id) {
    if (id < 1 || id > 20) return false;
    return _onlineStatus[id];
}

// ---------------------------
// 阻塞指令 (兼容同步场景)
// ---------------------------
bool transactionDone = false;
bool cbSync(Modbus::ResultCode event, uint16_t transactionId, void* data) {
    transactionDone = true;
    return true;
}

bool Rs485Master::waitTransaction(uint8_t id) {
    transactionDone = false;
    unsigned long start = millis();
    while (!transactionDone && (millis() - start < 150)) {
        _mb.task();
        delay(1);
    }
    return transactionDone;
}

bool Rs485Master::openDischarge(int id) {
    if (_mb.writeHreg(id, REG_CTRL_CMD, 1, cbSync)) {
        return waitTransaction(id);
    }
    return false;
}

bool Rs485Master::closeDischarge(int id) {
    if (_mb.writeHreg(id, REG_CTRL_CMD, 2, cbSync)) {
        return waitTransaction(id);
    }
    return false;
}

bool Rs485Master::tare(int id) {
    if (_mb.writeHreg(id, REG_CTRL_CMD, 3, cbSync)) {
        return waitTransaction(id);
    }
    return false;
}

bool Rs485Master::setPosition(int id, long position, int speed) {
    uint16_t posHigh = (uint16_t)((position >> 16) & 0xFFFF);
    uint16_t posLow = (uint16_t)(position & 0xFFFF);
    uint16_t spd = (uint16_t)speed;
    uint16_t servoData[3] = {posHigh, posLow, spd};
    
    if (_mb.writeHreg(id, 0x0028, servoData, 3, cbSync)) {
        return waitTransaction(id);
    }
    return false;
}
