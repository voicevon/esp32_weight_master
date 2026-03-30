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
        _pollWhitelist[i] = true; // 默认全部开启，直到加载配置
        _failCounters[i] = 0;
        _nodeErrorStats[i] = 0;
        _nodeLastResult[i] = Modbus::EX_SUCCESS;
    }
    _mutexBus = xSemaphoreCreateMutex();
}

void ModbusMaster::begin() {
    pinMode(_enPin, OUTPUT);
    digitalWrite(_enPin, RS485_RX_ENABLE);
    Serial2.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    _mb.begin(&Serial2, _enPin);
    _mb.master();
    loadPollWhitelist(); // 从 Flash 加载已锁定节点列表
}

// ---------------------------
// 异步指令轮询逻辑 (状态机驱动)
// ---------------------------
void ModbusMaster::update() {
    if (xSemaphoreTake(_mutexBus, pdMS_TO_TICKS(10)) != pdTRUE) return; 
    
    _mb.task(); // 维持底层协议栈

    switch (_status) {
        case ST_IDLE: {
            // 发起下一个 ID 的查询
            _status = ST_WAITING;
            _lastPollTime = millis();
            _packetsSent++;
            
            uint8_t id = _isScanning ? _scanProgress : _currentPollId;

            // 如果不处于扫描模式，确保 ID 在白名单内
            if (!_isScanning && !_pollWhitelist[_currentPollId]) {
                uint8_t nextId = _currentPollId;
                for (int i = 0; i < 20; i++) {
                    nextId = (nextId % 20) + 1;
                    if (_pollWhitelist[nextId]) break;
                }
                _currentPollId = nextId;
                id = _currentPollId;
            }

            _mb.readHreg(id, REG_WEIGHT_H, _tempRegs, 3, cbPoll);
            break;
        }

        case ST_WAITING: {
            if (millis() - _lastPollTime > 1500) { // 超时判定
                _status = ST_TIMEOUT;
                uint8_t id = _isScanning ? _scanProgress : _currentPollId;
                
                Serial.printf("[MODBUS] !! Timeout for ID: %d\n", id);
                _failCounters[id]++;
                _nodeErrorStats[id]++;
                _nodeLastResult[id] = Modbus::EX_TIMEOUT;
                _onlineStatus[id] = false;

                // 推进到下一个节点
                if (_isScanning) {
                    if (_scanProgress >= 20) _isScanning = false; 
                    else _scanProgress++;
                } else {
                    _currentPollId = (_currentPollId % 20) + 1;
                }
                _status = ST_IDLE; // 恢复空闲，等待下一轮轮询
            }
            break;
        }

        case ST_SUCCESS:
        case ST_ERROR:
        case ST_TIMEOUT:
            // 这些状态通常在回调或命令执行结束后由外部或下一次流程重置为 IDLE
            _status = ST_IDLE;
            break;

        default:
            break;
    }

    xSemaphoreGive(_mutexBus);
}

void ModbusMaster::startScan() {
    xSemaphoreTake(_mutexBus, portMAX_DELAY);
    _isScanning = true;
    _scanProgress = 1;
    _status = ST_IDLE; // 强制回到空闲以开始新轮询
    for (int i = 0; i < 21; i++) _onlineStatus[i] = false;
    xSemaphoreGive(_mutexBus);
}

void ModbusMaster::stopScan() {
    if (!_isScanning) return;
    xSemaphoreTake(_mutexBus, portMAX_DELAY);
    _isScanning = false;
    _status = ST_IDLE;
    xSemaphoreGive(_mutexBus);
    Serial.println("[MODBUS] Scan interrupted by user.");
}

// 静态回调：数据接收到位后在此处更新缓存
bool ModbusMaster::cbPoll(Modbus::ResultCode event, uint16_t transactionId, void* data) {
    ModbusMaster* instance = _instance;
    uint8_t id = instance->_isScanning ? instance->_scanProgress : instance->_currentPollId;

    if (event == Modbus::EX_SUCCESS) {
        if (!instance->_onlineStatus[id]) {
            Serial.printf("[MODBUS] ID:%02d RECOVERED.\n", id);
        }
        instance->_status = ST_SUCCESS;
        instance->_onlineStatus[id] = true;
        instance->_failCounters[id] = 0;
        instance->_nodeLastResult[id] = event;

        FloatConverter conv;
        conv.r[0] = instance->_tempRegs[0];
        conv.r[1] = instance->_tempRegs[1];
        instance->_cachedWeights[id] = conv.f;

        if (!instance->_isScanning) {
            uint16_t status = instance->_tempRegs[2];
            instance->_isStable[id] = (status >> 8) & 0x01;
            instance->_doorPhases[id] = (status >> 9) & 0x07;
        }
    } else {
        instance->_status = (event == Modbus::EX_TIMEOUT) ? ST_TIMEOUT : ST_ERROR;
        instance->_packetsDropped++;
        instance->_failCounters[id]++;
        instance->_nodeErrorStats[id]++;
        instance->_nodeLastResult[id] = event;

        Serial.printf("[MODBUS] ID:%02d Error: 0x%02X\n", id, (int)event);
        instance->_onlineStatus[id] = false;
    }

    // 无论成功失败，推进 ID
    if (instance->_isScanning) {
        if (instance->_scanProgress >= 20) {
            instance->_isScanning = false; 
            instance->savePollWhitelist(); // 自动持久化扫描结果
            Serial.println("[MODBUS] Scan Complete & Automatically Persisted to Whitelist.");
        } else {
            instance->_scanProgress++;
        }
    } else {
        instance->_currentPollId = (instance->_currentPollId % 20) + 1;
    }
    
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
bool ModbusMaster::cbSync(Modbus::ResultCode event, uint16_t transactionId, void* data) {
    ModbusMaster* instance = ModbusMaster::_instance;
    if (event == Modbus::EX_SUCCESS) {
        instance->_status = ModbusMaster::ST_SUCCESS;
    } else {
        instance->_status = (event == Modbus::EX_TIMEOUT) ? ModbusMaster::ST_TIMEOUT : ModbusMaster::ST_ERROR;
    }
    return true;
}

bool ModbusMaster::waitTransaction(uint8_t id) {
    unsigned long start = millis();
    // 等待进入 ST_SUCCESS, ST_TIMEOUT 或 ST_ERROR 状态
    while (_status == ST_WAITING && (millis() - start < 200)) {
        _mb.task();
        delay(1);
    }
    return (_status == ST_SUCCESS);
}

// 内部事务执行器：确保原子性
bool ModbusMaster::execCmd(uint8_t id, std::function<bool()> startFunc) {
    if (xSemaphoreTake(_mutexBus, pdMS_TO_TICKS(150)) != pdTRUE) return false;
    
    // 如果总线正忙于其他任务，等待其进入 IDLE 或强制重置
    if (_status != ST_IDLE) {
        unsigned long waitStart = millis();
        while (_status != ST_IDLE && (millis() - waitStart < 100)) {
            _mb.task();
            delay(1);
        }
    }

    _status = ST_WAITING;
    _lastPollTime = millis();
    bool started = startFunc();
    
    bool result = false;
    if (started) {
        _packetsSent++;
        result = waitTransaction(id);
        if (!result) _packetsDropped++;
    }
    
    _status = ST_IDLE; // 指令结束，释放状态
    xSemaphoreGive(_mutexBus);
    return result;
}

bool ModbusMaster::openDischarge(int id) {
    return execCmd(id, [this, id](){ 
        return _mb.writeHreg(id, REG_CTRL_CMD, 1, cbSync); 
    });
}

bool ModbusMaster::closeDischarge(int id) {
    return execCmd(id, [this, id](){ 
        return _mb.writeHreg(id, REG_CTRL_CMD, 2, cbSync); 
    });
}

bool ModbusMaster::tare(int id) {
    Serial.printf("[MODBUS] >> Requesting TARE for ID: %d\n", id);
    return execCmd(id, [this, id](){ 
        return _mb.writeHreg(id, REG_CTRL_CMD, 3, cbSync); 
    });
}

bool ModbusMaster::broadcastTare() {
    // 强制基于“白名单”执行：对白名单内的在线节点发起去皮
    Serial.println("[MODBUS] >> Starting WHITELIST-ONLY SEQUENTIAL TARE");
    
    int successCount = 0;
    int totalTarget = 0;
    
    for (int id = 1; id <= 20; id++) {
        if (_pollWhitelist[id]) { // 核心规则：仅对白名单节点操作
            totalTarget++;
            if (this->tare(id)) {
                successCount++;
            }
            vTaskDelay(pdMS_TO_TICKS(10)); 
        }
    }
    
    Serial.printf("[MODBUS] << Sequential Tare Finished. Success: %d/%d\n", successCount, totalTarget);
    return (successCount == totalTarget);
}

#include <Preferences.h>
void ModbusMaster::savePollWhitelist() {
    Preferences prefs;
    prefs.begin("modbus", false);
    
    uint32_t mask = 0;
    // 将当前的 _onlineStatus 同步到白名单，并保存位掩码 (Bitmask)
    for (int id = 1; id <= 20; id++) {
        _pollWhitelist[id] = _onlineStatus[id];
        if (_pollWhitelist[id]) mask |= (1 << (id - 1));
    }
    
    prefs.putUInt("whitelist", mask);
    prefs.end();
    Serial.printf("[MODBUS] Whitelist PERSISTED to Flash. Mask: 0x%08X\n", mask);
}

void ModbusMaster::loadPollWhitelist() {
    Preferences prefs;
    prefs.begin("modbus", true);
    
    uint32_t mask = prefs.getUInt("whitelist", 0);
    prefs.end();
    
    if (mask == 0) {
        // 默认为全部开启（初次运行或未扫描）
        for (int i = 1; i <= 20; i++) _pollWhitelist[i] = true;
        Serial.println("[MODBUS] No whitelist found. Defaulting to ALL nodes.");
    } else {
        for (int i = 1; i <= 20; i++) {
            _pollWhitelist[i] = (mask & (1 << (i - 1))) != 0;
        }
        Serial.printf("[MODBUS] Poll Whitelist LOADED. Mask: 0x%08X\n", mask);
    }
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
