#include "ModbusMaster.h"
#include <Arduino.h>
#include <Preferences.h>
#include "SystemConfig.h"

// Helper union for float to Modbus register conversion
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
        _lastDataIds[i] = 0;
        _targetDataIds[i] = 0;
        _onlineStatus[i] = false;
        _pollWhitelist[i] = true; // 默认所有节点都在白名单
        _nodeStatus[i] = NODE_DIRTY;
        _failCounters[i] = 0;
        _nodeErrorStats[i] = 0;
        _nodeLastResult[i] = Modbus::EX_SUCCESS;
    }
    _mutexBus = xSemaphoreCreateMutex();
}

void ModbusMaster::begin() {
    Serial1.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    _mb.begin(&Serial1, _enPin);
    _mb.master();
    loadPollWhitelist();
}

void ModbusMaster::update() {
    if (xSemaphoreTake(_mutexBus, pdMS_TO_TICKS(10)) != pdTRUE) return; 
    _mb.task(); 

    switch (_status) {
        case ST_IDLE: {
            _status = ST_WAITING;
            _lastPollTime = millis();
            _packetsSent++;
            
            uint8_t id = _isScanning ? _scanProgress : _currentPollId;

            // 如果不是扫描模式且 ID 不在白名单，寻找下一个有效 ID
            if (!_isScanning && !_pollWhitelist[_currentPollId]) {
                uint8_t nextId = _currentPollId;
                for (int i = 0; i < 20; i++) {
                    nextId = (nextId % 20) + 1;
                    if (_pollWhitelist[nextId]) break;
                }
                _currentPollId = nextId;
                id = _currentPollId;
            }

            // 读取保持寄存器：Weight(2), Status(1), ADC(2), DataID(1) = 6 registers
            _mb.readHreg(id, 0x0000, _tempRegs, 6, cbPoll);
            break;
        }

        case ST_WAITING:
            if (millis() - _lastPollTime > MODBUS_POLL_TIMEOUT_MS) {
                _status = ST_TIMEOUT;
                _packetsDropped++;
                uint8_t id = _isScanning ? _scanProgress : _currentPollId;
                _onlineStatus[id] = false;
                _failCounters[id]++;
                
                if (_isScanning) {
                    _scanHistory[_scanCycle][id] = false; // 记录当前轮次的在线情况
                    _scanProgress++;
                    if (_scanProgress > 20) {
                        _scanCycle++;
                        if (_scanCycle >= 5) {
                            _isScanning = false;
                            generateWhitelistFromScan();
                        } else {
                            _scanProgress = 1; // 进入下一轮
                        }
                    }
                } else {
                    _currentPollId = (_currentPollId % 20) + 1;
                }
                _status = ST_IDLE;
            }
            break;

        case ST_SUCCESS:
        case ST_ERROR:
        case ST_TIMEOUT:
            _status = ST_IDLE;
            break;

        default:
            break;
    }
    xSemaphoreGive(_mutexBus);
}

bool ModbusMaster::cbPoll(Modbus::ResultCode event, uint16_t transactionId, void* data) {
    ModbusMaster* instance = _instance;
    uint8_t id = instance->_isScanning ? instance->_scanProgress : instance->_currentPollId;

    if (event == Modbus::EX_SUCCESS) {
        instance->_status = ST_SUCCESS;
        instance->_onlineStatus[id] = true;
        instance->_failCounters[id] = 0;

        // 1. 重量解析 (Float)
        FloatConverter conv;
        conv.r[0] = instance->_tempRegs[0];
        conv.r[1] = instance->_tempRegs[1];
        instance->_cachedWeights[id] = conv.f;

        // 2. 状态与门相位解析
        uint16_t statusReg = instance->_tempRegs[2];
        instance->_isStable[id] = (statusReg >> 8) & 0x01;
        instance->_doorPhases[id] = (statusReg & 0xFF); 
        
        // 3. DataID 解析
        uint16_t currentDataId = instance->_tempRegs[5];
        instance->_lastDataIds[id] = currentDataId;

        // 4. 加强版新鲜度判定
        if (!instance->_isScanning) {
            NodeStatus currentStatus = instance->_nodeStatus[id];
            bool isHardwareStable = instance->_isStable[id];
            
            if (instance->_targetDataIds[id] == 0) {
                instance->_targetDataIds[id] = currentDataId;
            }

            if (currentStatus == NODE_DIRTY) {
                instance->_targetDataIds[id] = currentDataId;
                instance->_nodeStatus[id] = NODE_REFRESHING;
            } else {
                bool isFresh = (currentDataId == instance->_targetDataIds[id]);
                bool isDoorClosed = (instance->_doorPhases[id] == 0); 

                if (isFresh && isDoorClosed && isHardwareStable) {
                    instance->_nodeStatus[id] = NODE_STABLE;
                } else if (!isHardwareStable || !isFresh) {
                    instance->_nodeStatus[id] = NODE_REFRESHING;
                }
            }
        }
    } else {
        instance->_status = ST_ERROR;
        instance->_onlineStatus[id] = false;
        instance->_nodeLastResult[id] = event;
        instance->_nodeErrorStats[id]++;
    }

    // ID 步进并重新统计不稳定数量
    instance->_unstableCount = 0;
    for (int i = 1; i <= 20; i++) {
        if (instance->_onlineStatus[i] && !instance->_isStable[i]) {
            instance->_unstableCount++;
        }
    }

    if (instance->_isScanning) {
        instance->_scanHistory[instance->_scanCycle][id] = (event == Modbus::EX_SUCCESS);
        instance->_scanProgress++;
        if (instance->_scanProgress > 20) {
            instance->_scanCycle++;
            if (instance->_scanCycle >= 5) {
                instance->_isScanning = false;
                instance->generateWhitelistFromScan();
            } else {
                instance->_scanProgress = 1; // 进入下一轮
            }
        }
    } else {
        instance->_currentPollId = (instance->_currentPollId % 20) + 1;
    }

    return true;
}

float ModbusMaster::getWeight(int id) {
    if (id < 1 || id > 20) return 0.0f;
    return _cachedWeights[id];
}

bool ModbusMaster::isStable(int id) {
    if (id < 1 || id > 20) return false;
    return _isStable[id];
}

uint8_t ModbusMaster::getDoorPhase(int id) {
    if (id < 1 || id > 20) return 0;
    return _doorPhases[id];
}

bool ModbusMaster::isNodeOnline(int id) {
    if (id < 1 || id > 20) return false;
    return _onlineStatus[id];
}

void ModbusMaster::setNodeStatus(int id, NodeStatus s) {
    if (id >= 1 && id <= 20) _nodeStatus[id] = s;
}

bool ModbusMaster::execCmd(uint8_t id, std::function<bool()> startFunc) {
    if (xSemaphoreTake(_mutexBus, pdMS_TO_TICKS(1000)) != pdTRUE) return false;
    
    _status = ST_PENDING;
    if (!startFunc()) {
        xSemaphoreGive(_mutexBus);
        return false;
    }
    
    _status = ST_WAITING;
    bool result = waitTransaction(id);
    xSemaphoreGive(_mutexBus);
    return result;
}

bool ModbusMaster::waitTransaction(uint8_t id) {
    unsigned long start = millis();
    while (millis() - start < MODBUS_POLL_TIMEOUT_MS) {
        _mb.task();
        if (_status == ST_SUCCESS) return true;
        if (_status == ST_ERROR || _status == ST_TIMEOUT) return false;
        vTaskDelay(1);
    }
    return false;
}

bool ModbusMaster::cbSync(Modbus::ResultCode event, uint16_t transactionId, void* data) {
    ModbusMaster* instance = _instance;
    if (event == Modbus::EX_SUCCESS) instance->_status = ST_SUCCESS;
    else instance->_status = ST_ERROR;
    return true;
}

bool ModbusMaster::openDischarge(int id) {
    return execCmd(id, [this, id](){ return _mb.writeHreg(id, 0x0100, 1, cbSync); });
}

bool ModbusMaster::openDischarge1S(int id) {
    bool ok = execCmd(id, [this, id](){ return _mb.writeHreg(id, 0x0100, 5, cbSync); });
    if (ok) {
        _targetDataIds[id] = _lastDataIds[id] + 1;
        setNodeStatus(id, NODE_DISCHARGING);
        Serial.printf("[FSM] Node %d Discharging. Expected ID: %d\n", id, _targetDataIds[id]);
    }
    return ok;
}

bool ModbusMaster::closeDischarge(int id) {
    return execCmd(id, [this, id](){ return _mb.writeHreg(id, 0x0100, 2, cbSync); });
}

bool ModbusMaster::tare(int id) {
    return execCmd(id, [this, id](){ return _mb.writeHreg(id, 0x0100, 3, cbSync); });
}

bool ModbusMaster::broadcastTare() {
    // 广播不需要等待回复 (Modbus ID 0)
    if (xSemaphoreTake(_mutexBus, pdMS_TO_TICKS(1000)) != pdTRUE) return false;
    bool ok = _mb.writeHreg(0, 0x0100, 3); 
    xSemaphoreGive(_mutexBus);
    return ok;
}

bool ModbusMaster::setPosition(int id, long position, int speed) {
    // 假设从机有位置寄存器 (此处为示意)
    return execCmd(id, [this, id, position](){ 
        return _mb.writeHreg(id, 0x0200, (uint16_t)position, cbSync); 
    });
}

void ModbusMaster::startScan() {
    _isScanning = true;
    _scanProgress = 1;
    _scanCycle = 0;
    // 重置历史记录数组
    for (int c = 0; c < 5; c++) {
        for (int i = 0; i < 21; i++) {
            _scanHistory[c][i] = false;
        }
    }
}

void ModbusMaster::stopScan() {
    _isScanning = false;
}

void ModbusMaster::generateWhitelistFromScan() {
    // 逻辑：只有在 5 次扫描中全部在线的节点才加入白名单
    for (int i = 1; i <= 20; i++) {
        bool allPass = true;
        for (int c = 0; c < 5; c++) {
            if (!_scanHistory[c][i]) {
                allPass = false;
                break;
            }
        }
        _pollWhitelist[i] = allPass;
    }
    savePollWhitelist();
}

void ModbusMaster::savePollWhitelist() {
    Preferences prefs;
    prefs.begin("modbus", false);
    for (int i = 1; i <= 20; i++) {
        char key[8];
        snprintf(key, sizeof(key), "wl%d", i);
        prefs.putBool(key, _pollWhitelist[i]);
    }
    prefs.end();
}

void ModbusMaster::loadPollWhitelist() {
    Preferences prefs;
    prefs.begin("modbus", true);
    for (int i = 1; i <= 20; i++) {
        char key[8];
        snprintf(key, sizeof(key), "wl%d", i);
        _pollWhitelist[i] = prefs.getBool(key, true);
    }
    prefs.end();
}

bool ModbusMaster::performLoopbackTest() {
    // 基础环回测试逻辑：向自己发送数据（由于是半双工 RS485，通常需要硬件闭环或特定节点配合）
    return false;
}

void ModbusMaster::sendRawByte(uint8_t byte) {
    Serial1.write(byte);
}

void ModbusMaster::clearRawBuffer() {
    while(Serial1.available()) Serial1.read();
}

int ModbusMaster::availableRaw() {
    return Serial1.available();
}

uint8_t ModbusMaster::readRawByte() {
    return Serial1.read();
}

uint32_t ModbusMaster::getWhitelistMask() const {
    uint32_t mask = 0;
    for (int i = 1; i <= 20; i++) {
        if (_pollWhitelist[i]) mask |= (1 << (i - 1));
    }
    return mask;
}
