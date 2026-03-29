#include "ModbusMaster.h"

// 静态转换 union，用于跨函数解析 Float
union FloatConverter {
    float f;
    uint16_t r[2];
};

ModbusMaster* ModbusMaster::_instance = nullptr;

ModbusMaster::ModbusMaster(int rxPin, int txPin, int enPin, long baud)
    : _rxPin(rxPin), _txPin(txPin), _enPin(enPin), _baud(baud) {
    _instance = this;
    for (int i = 0; i < 21; i++) {
        _cachedWeights[i] = 0.0f;
        _isStable[i] = false;
        _doorPhases[i] = 0;
        _onlineStatus[i] = false;
        _failCounters[i] = 0;
        _nodeErrorStats[i] = 0;
        _nodeLastResult[i] = Modbus::EX_SUCCESS;
    }
}

void ModbusMaster::begin() {
    pinMode(_enPin, OUTPUT);
    digitalWrite(_enPin, RS485_RX_ENABLE);
    Serial2.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    _mb.begin(&Serial2, _enPin);
    _mb.master();
}

// ---------------------------
// 异步指令轮询逻辑
// ---------------------------
void ModbusMaster::update() {
    _mb.task(); // 维持底层协议栈

    // 如果当前正在等待响应，且未超时，则直接返回
    if (_isWaiting) {
        if (millis() - _lastPollTime > 200) { // 200ms 防卡死超时
            uint8_t id = _isScanning ? _scanProgress : _currentPollId;
            Serial.printf("[MODBUS] !! Timeout for ID: %d (Strike: 1/1)\n", id);
            
            _failCounters[id]++;
            _nodeErrorStats[id]++;
            _nodeLastResult[id] = Modbus::EX_TIMEOUT;

            if (_onlineStatus[id]) Serial.printf("[MODBUS] ID:%02d Status -> OFFLINE\n", id);
            _onlineStatus[id] = false;

            if (_isScanning) {
                if (_scanProgress >= 20) _isScanning = false;
                else _scanProgress++;
            } else {
                _currentPollId = (_currentPollId % 20) + 1;
            }
            _isWaiting = false;
        }
        return;
    }

    // 空闲状态：发起下一个 ID 的查询
    _isWaiting = true;
    _lastPollTime = millis();
    _packetsSent++; // 记录尝试发送
    
    if (_isScanning) {
        // 扫描模式：尝试读取权重寄存器，只为确认存在
        Serial.printf("[MODBUS] ++ Scanning ID: %d\n", _scanProgress);
        _mb.readHreg(_scanProgress, REG_WEIGHT_H, _tempRegs, 2, cbPoll);
    } else {
        Serial.printf("[MODBUS] >> Polling ID: %d\n", _currentPollId);
        // 读取 3 个寄存器：Weight (2) + Status (1)
        _mb.readHreg(_currentPollId, REG_WEIGHT_H, _tempRegs, 3, cbPoll);
    }
}

void ModbusMaster::startScan() {
    _isScanning = true;
    _scanProgress = 1;
    _isWaiting = false; // 立即开始
    // 重置所有在线状态，扫描时重新发现
    for (int i = 0; i < 21; i++) _onlineStatus[i] = false;
}

// 静态回调：数据接收到位后在此处更新缓存
bool ModbusMaster::cbPoll(Modbus::ResultCode event, uint16_t transactionId, void* data) {
    ModbusMaster* instance = _instance;
    uint8_t id = instance->_isScanning ? instance->_scanProgress : instance->_currentPollId;

    if (event == Modbus::EX_SUCCESS) {
        if (!instance->_onlineStatus[id]) {
            Serial.printf("[MODBUS] ID:%02d RECOVERED.\n", id);
        }
        instance->_onlineStatus[id] = true;
        instance->_failCounters[id] = 0;
        instance->_nodeLastResult[id] = event;

        FloatConverter conv;
        conv.r[0] = instance->_tempRegs[0];
        conv.r[1] = instance->_tempRegs[1];
        
        instance->_cachedWeights[id] = conv.f;

        if (!instance->_isScanning) {
            // 解析状态寄存器 (REG_STATUS)
            uint16_t status = instance->_tempRegs[2];
            instance->_isStable[id] = (status >> 8) & 0x01;
            instance->_doorPhases[id] = (status >> 9) & 0x07;
        }
    } else {
        instance->_packetsDropped++;
        instance->_failCounters[id]++;
        instance->_nodeErrorStats[id]++;
        instance->_nodeLastResult[id] = event;

        // Log error with codes (E4=Timeout, E7=CRC, etc)
        Serial.printf("[MODBUS] ID:%02d Error: 0x%02X (Strike: 1/1)\n", id, (int)event);

        if (instance->_onlineStatus[id]) {
            Serial.printf("[MODBUS] ID:%02d Status CHANGED -> OFFLINE\n", id);
        }
        instance->_onlineStatus[id] = false;
    }

    if (instance->_isScanning) {
        if (instance->_scanProgress >= 20) {
            instance->_isScanning = false; 
            Serial.println("[DIAG] Scan Complete.");
        } else {
            instance->_scanProgress++;
        }
    } else {
        instance->_currentPollId = (instance->_currentPollId % 20) + 1;
    }
    
    instance->_isWaiting = false;
    return true;
}

float ModbusMaster::getWeight(int id) {
    if (id < 1 || id > 20) return -1.0f;
    return _cachedWeights[id]; // 立即返回缓存
}

bool ModbusMaster::isNodeOnline(int id) {
    if (id < 1 || id > 20) return false;
    return _onlineStatus[id];
}

bool ModbusMaster::isStable(int id) {
    if (id < 1 || id > 20) return false;
    return _isStable[id];
}

uint8_t ModbusMaster::getDoorPhase(int id) {
    if (id < 1 || id > 20) return 0;
    return _doorPhases[id];
}

// ---------------------------
// 阻塞指令 (兼容同步场景)
// ---------------------------
static bool transactionDone = false;
static bool cbSync(Modbus::ResultCode event, uint16_t transactionId, void* data) {
    transactionDone = true;
    return true;
}

bool ModbusMaster::waitTransaction(uint8_t id) {
    transactionDone = false;
    unsigned long start = millis();
    while (!transactionDone && (millis() - start < 150)) {
        _mb.task();
        delay(1);
    }
    return transactionDone;
}

bool ModbusMaster::openDischarge(int id) {
    if (_mb.writeHreg(id, REG_CTRL_CMD, 1, cbSync)) {
        _packetsSent++;
        bool ok = waitTransaction(id);
        if (!ok) _packetsDropped++;
        return ok;
    }
    return false;
}

bool ModbusMaster::closeDischarge(int id) {
    if (_mb.writeHreg(id, REG_CTRL_CMD, 2, cbSync)) {
        _packetsSent++;
        bool ok = waitTransaction(id);
        if (!ok) _packetsDropped++;
        return ok;
    }
    return false;
}

bool ModbusMaster::tare(int id) {
    if (_mb.writeHreg(id, REG_CTRL_CMD, 3, cbSync)) {
        _packetsSent++;
        bool ok = waitTransaction(id);
        if (!ok) _packetsDropped++;
        return ok;
    }
    return false;
}

bool ModbusMaster::setPosition(int id, long position, int speed) {
    uint16_t posHigh = (uint16_t)((position >> 16) & 0xFFFF);
    uint16_t posLow = (uint16_t)(position & 0xFFFF);
    uint16_t spd = (uint16_t)speed;
    uint16_t servoData[3] = {posHigh, posLow, spd};
    
    if (_mb.writeHreg(id, 0x0028, servoData, 3, cbSync)) {
        _packetsSent++;
        bool ok = waitTransaction(id);
        if (!ok) _packetsDropped++;
        return ok;
    }
    return false;
}

bool ModbusMaster::performLoopbackTest() {
    const char* testMsg = "RS485_TEST";
    
    // 手动开启发送模式
    digitalWrite(_enPin, RS485_TX_ENABLE);
    delayMicroseconds(50);
    
    Serial2.flush();
    while(Serial2.available()) Serial2.read();

    Serial2.print(testMsg);
    Serial2.flush();
    
    // 切回接收模式
    delayMicroseconds(50);
    digitalWrite(_enPin, RS485_RX_ENABLE);

    unsigned long start = millis();
    String response = "";
    while (millis() - start < 100) {
        if (Serial2.available()) {
            response += (char)Serial2.read();
        }
    }

    Serial.printf("[DIAG] Loopback Sent: %s, Received: %s\n", testMsg, response.c_str());
    return response.indexOf(testMsg) != -1;
}

void ModbusMaster::sendRawByte(uint8_t byte) {
    digitalWrite(_enPin, RS485_TX_ENABLE);
    delayMicroseconds(50);
    Serial2.write(byte);
    Serial2.flush();
    delayMicroseconds(50);
    digitalWrite(_enPin, RS485_RX_ENABLE);
}

int ModbusMaster::availableRaw() {
    return Serial2.available();
}

uint8_t ModbusMaster::readRawByte() {
    return (uint8_t)Serial2.read();
}
