#include "ModbusMaster.h"
#include <Arduino.h>

ModbusMaster* ModbusMaster::_instance = nullptr;

ModbusMaster::ModbusMaster(int rxPin, int txPin, int enPin, long baud)
    : _rxPin(rxPin), _txPin(txPin), _enPin(enPin), _baud(baud) {
    _instance = this;
    _mutexBus = xSemaphoreCreateMutex();
}

void ModbusMaster::begin() {
    Serial1.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    if (_enPin >= 0) {
        pinMode(_enPin, OUTPUT);
        digitalWrite(_enPin, LOW); // 默认接收模式
    }
    
    xTaskCreatePinnedToCore(modbusTask, "MB_Task", 4096, this, 15, &_taskHandle, 1);
}

// 查表法 CRC16 - 移除 PROGMEM 以避免 ESP32 内存访问对齐隐患
static const uint16_t crcTable[] = {
    0X0000, 0XC0C1, 0XC181, 0X0140, 0X0301, 0XC3C1, 0XC281, 0X0240, 0X0601, 0XC6C1, 0XC781, 0X0740, 0X0501, 0XC5C1, 0XC481, 0X0440,
    0X0C01, 0XCCC1, 0XCD81, 0X0D40, 0X0F01, 0XCFC1, 0XCE81, 0X0E40, 0X0A01, 0XCAC1, 0XCB81, 0X0B40, 0X0901, 0XC9C1, 0XC881, 0X0840,
    0X1801, 0XD8C1, 0XD9C1, 0X1940, 0X1B01, 0XDBC1, 0XDA81, 0X1A40, 0X1E01, 0XDEC1, 0XDFC1, 0X1F40, 0X1D01, 0XDDC1, 0XDC81, 0X1C40,
    0X1401, 0XD4C1, 0XD5C1, 0X1540, 0X1701, 0XD7C1, 0XD681, 0X1640, 0X1201, 0XD2C1, 0XD3C1, 0X1340, 0X1101, 0XD1C1, 0XD081, 0X1040,
    0X3001, 0XF0C1, 0XF1C1, 0X3140, 0X3301, 0XF3C1, 0XF281, 0X3240, 0X3601, 0XF6C1, 0XF781, 0X3740, 0X3501, 0XF5C1, 0XF481, 0X3440,
    0X3C01, 0XFCC1, 0XFDC1, 0X3D40, 0X3F01, 0XFFC1, 0XFE81, 0X3E40, 0X3A01, 0XFAC1, 0XFB81, 0X3B40, 0X3901, 0XF9C1, 0XF881, 0X3840,
    0X2801, 0XE8C1, 0XE9C1, 0X2940, 0X2B01, 0XEBC1, 0XEA81, 0X2A40, 0X2E01, 0XEEC1, 0XEFC1, 0X2F40, 0X2D01, 0XEDC1, 0XEC81, 0X2C40,
    0X2401, 0XE4C1, 0XE5C1, 0X2540, 0X2701, 0XE7C1, 0XE681, 0X2640, 0X2201, 0XE2C1, 0XE3C1, 0X2340, 0X2101, 0XE1C1, 0XE081, 0X2040,
    0X8140, 0X4101, 0X4001, 0X80C1, 0X4201, 0X82C1, 0X83C1, 0X4340, 0X4601, 0X86C1, 0X87C1, 0X4740, 0X4501, 0X85C1, 0X8481, 0X4440,
    0X4C01, 0X8CC1, 0X8DC1, 0X4D40, 0X4F01, 0X8FC1, 0X8E81, 0X4E40, 0X4A01, 0X8AC1, 0X8B81, 0X4B40, 0X4901, 0X89C1, 0X8881, 0X4840,
    0X9801, 0X58C1, 0X59C1, 0X9940, 0X5B01, 0X9BC1, 0X9A81, 0X5A40, 0X5E01, 0X9EC1, 0X9FC1, 0X5F40, 0X5D01, 0X9DC1, 0X9C81, 0X5C40,
    0X5401, 0X94C1, 0X95C1, 0X5540, 0X5701, 0X97C1, 0X9681, 0X5640, 0X5201, 0X92C1, 0X93C1, 0X5340, 0X5101, 0X91C1, 0X9081, 0X5040,
    0X7001, 0XB0C1, 0XB1C1, 0X7140, 0X7301, 0XB3C1, 0XB281, 0X7240, 0X7601, 0XB6C1, 0XB781, 0X7740, 0X7501, 0XB5C1, 0XB481, 0X7440,
    0X7C01, 0XBCC1, 0XBDC1, 0X7D40, 0X7F01, 0XBFC1, 0XBE81, 0X7E40, 0X7A01, 0XBAC1, 0XBB81, 0X7B40, 0X7901, 0XB9C1, 0XB881, 0X7840,
    0X6801, 0XA8C1, 0XA9C1, 0X6940, 0X6B01, 0XABC1, 0XAA81, 0X6A40, 0X6E01, 0XAEC1, 0XAFC1, 0X6F40, 0X6D01, 0XADC1, 0XAC81, 0X6C40,
    0X6401, 0XA4C1, 0XA5C1, 0X6540, 0X6701, 0XA7C1, 0XA681, 0X6640, 0X6201, 0XA2C1, 0XA3C1, 0X6340, 0X6101, 0XA1C1, 0XA081, 0X6040
};

uint16_t ModbusMaster::calculateCRC(uint8_t* buf, int len) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crcTable[(crc ^ buf[i]) & 0xFF];
    }
    return crc;
}

void ModbusMaster::sendPacket(uint8_t* buf, int len) {
    uint16_t crc = calculateCRC(buf, len);
    buf[len++] = crc & 0xFF;
    buf[len++] = (crc >> 8) & 0xFF;
    
    // 调试报告：打印原始发送字节
    Serial.print("[TX RAW >] ");
    for(int i=0; i<len; i++) Serial.printf("%02X ", buf[i]);
    Serial.println();

    if (_enPin >= 0) digitalWrite(_enPin, HIGH); 
    Serial1.write(buf, len);
    Serial1.flush();           // 等待 UART FIFO 发空
    if (_enPin >= 0) digitalWrite(_enPin, LOW);  
}

bool ModbusMaster::asyncRead(uint8_t id, uint16_t addr, uint16_t count, cbTransaction cb, uint16_t* destBuffer) {
    if (xSemaphoreTake(_mutexBus, pdMS_TO_TICKS(10)) != pdTRUE) return false;
    if (_status != ST_IDLE && _status != ST_SUCCESS && _status != ST_TIMEOUT && _status != ST_ERROR) {
        xSemaphoreGive(_mutexBus);
        return false;
    }

    _lastTid = id; 
    _pendingCb   = cb;
    _pendingData = (void*)destBuffer;
    _status      = ST_WAITING;
    _lastPollTime = millis();
    _packetsSent++;

    _txBuf[0] = id;
    _txBuf[1] = 0x03;
    _txBuf[2] = addr >> 8;
    _txBuf[3] = addr & 0xFF;
    _txBuf[4] = count >> 8;
    _txBuf[5] = count & 0xFF;
    
    while(Serial1.available()) Serial1.read(); // 清空缓冲区
    sendPacket(_txBuf, 6);
    
    Serial.printf("[ModbusMaster] QUEUE Read ID:%d Addr:0x%04X\n", id, addr);
    xSemaphoreGive(_mutexBus);
    return true;
}

bool ModbusMaster::asyncWrite(uint8_t id, uint16_t addr, uint16_t value, cbTransaction cb) {
    if (xSemaphoreTake(_mutexBus, pdMS_TO_TICKS(10)) != pdTRUE) return false;
    if (_status != ST_IDLE && _status != ST_SUCCESS && _status != ST_TIMEOUT && _status != ST_ERROR) {
        xSemaphoreGive(_mutexBus);
        return false;
    }

    _lastTid = id; 
    _pendingCb   = cb;
    _pendingData = nullptr; // 写指令不需要目标缓冲区
    _status      = ST_WAITING;
    _lastPollTime = millis();
    _packetsSent++;

    _txBuf[0] = id;
    _txBuf[1] = 0x06;
    _txBuf[2] = addr >> 8;
    _txBuf[3] = addr & 0xFF;
    _txBuf[4] = value >> 8;
    _txBuf[5] = value & 0xFF;
    
    while(Serial1.available()) Serial1.read();
    sendPacket(_txBuf, 6);
    
    Serial.printf("[ModbusMaster] QUEUE Write ID:%d Addr:0x%04X Val:%d\n", id, addr, value);
    xSemaphoreGive(_mutexBus);
    return true;
}

bool ModbusMaster::syncWrite(uint8_t id, uint16_t addr, uint16_t value) {
    if (xSemaphoreTake(_mutexBus, pdMS_TO_TICKS(500)) != pdTRUE) return false;
    
    _status = ST_WAITING;
    _lastPollTime = millis();
    _txBuf[0] = id;
    _txBuf[1] = 0x06;
    _txBuf[2] = addr >> 8;
    _txBuf[3] = addr & 0xFF;
    _txBuf[4] = value >> 8;
    _txBuf[5] = value & 0xFF;
    
    while(Serial1.available()) Serial1.read();
    sendPacket(_txBuf, 6);
    xSemaphoreGive(_mutexBus);

    unsigned long start = millis();
    while (millis() - start < 1000) {
        if (_status == ST_SUCCESS) return true;
        if (_status == ST_ERROR || _status == ST_TIMEOUT) return false;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return false;
}

bool ModbusMaster::broadcastWrite(uint16_t addr, uint16_t value) {
    if (xSemaphoreTake(_mutexBus, pdMS_TO_TICKS(500)) != pdTRUE) return false;
    _txBuf[0] = 0x00; // 广播 ID
    _txBuf[1] = 0x06;
    _txBuf[2] = addr >> 8;
    _txBuf[3] = addr & 0xFF;
    _txBuf[4] = value >> 8;
    _txBuf[5] = value & 0xFF;
    sendPacket(_txBuf, 6);
    xSemaphoreGive(_mutexBus);
    return true;
}

void ModbusMaster::modbusTask(void* param) {
    ModbusMaster* instance = (ModbusMaster*)param;
    while (true) {
        if (instance->_status == ST_WAITING) {
            if (Serial1.available()) {
                unsigned long firstByteTime = millis();
                unsigned long latency = firstByteTime - instance->_lastPollTime;
                
                unsigned long lastByteTime = millis();
                int idx = 0;
                while (millis() - lastByteTime < 15) { // 稍微放宽 3.5T 判定到 15ms
                    if (Serial1.available()) {
                        instance->_rxBuf[idx++] = Serial1.read();
                        lastByteTime = millis();
                        if (idx >= 256) break;
                    }
                    vTaskDelay(1);
                }
                
                unsigned long totalDuration = millis() - instance->_lastPollTime;

                // 解析报文
                if (idx >= 5) { // 最小报文长度: ID + FN + LEN + DATA + CRC16(2)
                    uint16_t calcCrc = instance->calculateCRC(instance->_rxBuf, idx - 2);
                    uint16_t rxCrc = instance->_rxBuf[idx-2] | (instance->_rxBuf[idx-1] << 8);
                    
                    if (calcCrc == rxCrc) {
                        uint8_t fn = instance->_rxBuf[1];
                        if (fn == 0x03) { // 读回复
                            int byteCount = instance->_rxBuf[2];
                            uint16_t* dest = (uint16_t*)instance->_pendingData;
                            for (int i = 0; i < byteCount / 2; i++) {
                                dest[i] = (instance->_rxBuf[3 + i * 2] << 8) | instance->_rxBuf[4 + i * 2];
                            }
                            if (instance->_pendingCb) {
                                instance->_pendingCb(Modbus::EX_SUCCESS, instance->_lastTid, instance->_pendingData);
                                instance->_pendingCb = nullptr;
                            }
                            instance->_status = ST_SUCCESS;
                        } else if (fn == 0x06) { // 写回复 (Echo)
                            if (instance->_pendingCb) {
                                instance->_pendingCb(Modbus::EX_SUCCESS, instance->_lastTid, nullptr);
                                instance->_pendingCb = nullptr;
                            }
                            instance->_status = ST_SUCCESS;
                        }
                    } else {
                        Serial.printf("[ModbusMaster] CRC ERROR from ID:%d (Calc:%04X, Rx:%04X)\n", 
                                      instance->_rxBuf[0], calcCrc, rxCrc);
                        if (instance->_pendingCb) {
                            instance->_pendingCb(Modbus::EX_ERROR, instance->_lastTid, nullptr);
                            instance->_pendingCb = nullptr;
                        }
                        instance->_status = ST_ERROR;
                    }
                } else {
                    // Short frame log removed
                }
            } else if (millis() - instance->_lastPollTime > 2000) { // 放宽到 2s
                Serial.printf("[ModbusMaster] TIMEOUT waiting for ID:%d\n", instance->_lastTid);
                if (instance->_pendingCb) {
                    instance->_pendingCb(Modbus::EX_TIMEOUT, instance->_lastTid, instance->_pendingData);
                    instance->_pendingCb = nullptr;
                }
                instance->_status = ST_TIMEOUT;
                instance->_packetsDropped++;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

}

// 原始字节诊断
void ModbusMaster::sendRawBuffer(const uint8_t* buf, int len) {
    if (xSemaphoreTake(_mutexBus, pdMS_TO_TICKS(100)) != pdTRUE) return;
    if (_enPin >= 0) digitalWrite(_enPin, HIGH); 
    Serial1.write(buf, len); 
    Serial1.flush();
    if (_enPin >= 0) digitalWrite(_enPin, LOW); 
    xSemaphoreGive(_mutexBus);
}

void ModbusMaster::sendRawByte(uint8_t byte) { 
    if (_enPin >= 0) digitalWrite(_enPin, HIGH); 
    Serial1.write(byte); 
    Serial1.flush();
    if (_enPin >= 0) digitalWrite(_enPin, LOW); 
}
int ModbusMaster::availableRaw() { return Serial1.available(); }
uint8_t ModbusMaster::readRawByte() { return Serial1.read(); }
void ModbusMaster::clearRawBuffer() { while(Serial1.available()) Serial1.read(); }
