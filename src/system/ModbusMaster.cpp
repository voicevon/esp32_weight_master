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

// 查表法 CRC16 减少 CPU 占用
static const uint16_t crcTable[] PROGMEM = {
    0X0000, 0XC0C1, 0XC181, 0X0140, 0X0301, 0XC3C1, 0XC281, 0X0240,
    0X0601, 0XC6C1, 0XC781, 0X0740, 0X0501, 0XC5C1, 0XC481, 0X0440,
    0X0C01, 0XCCC1, 0XCD81, 0X0D40, 0X0F01, 0XCFC1, 0XCE81, 0X0E40,
    0X0A01, 0XCAC1, 0XCB81, 0X0B40, 0X0901, 0XC9C1, 0XC881, 0X0840,
    0X1801, 0XD8C1, 0XD9C1, 0X1940, 0X1B01, 0XDBC1, 0XDA81, 0X1A40,
    0X1E01, 0XDEC1, 0XDFC1, 0X1F40, 0X1D01, 0XDDC1, 0XDC81, 0X1C40,
    0X1401, 0XD4C1, 0XD5C1, 0X1540, 0X1701, 0XD7C1, 0XD681, 0X1640,
    0X1201, 0XD2C1, 0XD3C1, 0X1340, 0X1101, 0XD1C1, 0XD081, 0X1040,
    0X3001, 0XF0C1, 0XF1C1, 0X3140, 0X3301, 0XF3C1, 0XF281, 0X3240,
    0X3601, 0XF6C1, 0XF781, 0X3740, 0X3501, 0XF5C1, 0XF481, 0X3440,
    0X3C01, 0XFCC1, 0XFDC1, 0X3D40, 0X3F01, 0XFFC1, 0XFE81, 0X3E40,
    0X3A01, 0XFAC1, 0XFB81, 0X3B40, 0X3901, 0XF9C1, 0XF881, 0X3840,
    0X2801, 0XE8C1, 0XE9C1, 0X2940, 0X2B01, 0XEBC1, 0XEA81, 0X2A40,
    0X2E01, 0XEEC1, 0XEFC1, 0X2F40, 0X2D01, 0XEDC1, 0XEC81, 0X2C40,
    0X2401, 0XE4C1, 0XE5C1, 0X2540, 0X2701, 0XE7C1, 0XE681, 0X2640,
    0X2201, 0XE2C1, 0XE3C1, 0X2340, 0X2101, 0XE1C1, 0XE081, 0X2040,
    0X6001, 0XA0C1, 0XA1C1, 0X6140, 0X6301, 0XA3C1, 0XA281, 0X6340, // 这里有一个修正
    0X6601, 0XA6C1, 0XA781, 0X6740, 0X6501, 0XA5C1, 0XA481, 0X6440,
    0X6C01, 0XACC1, 0XADC1, 0X6D40, 0X6F01, 0XAFC1, 0XAE81, 0X6E40,
    0X6A01, 0XAAC1, 0XAB81, 0X6B40, 0X6901, 0XA9C1, 0XA881, 0X6840,
    0X7801, 0XB8C1, 0XB9C1, 0X7940, 0X7B01, 0XBBC1, 0XBA81, 0X7A40,
    0X7E01, 0XBEC1, 0XBFC1, 0X7F40, 0X7D01, 0XBDC1, 0XBC81, 0X7C40,
    0X7401, 0XB4C1, 0XB5C1, 0X7540, 0X7701, 0XB7C1, 0XB681, 0X7640,
    0X7201, 0XB2C1, 0XB3C1, 0X7340, 0X7101, 0XB1C1, 0XB081, 0X7040,
    0X5001, 0X90C1, 0X91C1, 0X5140, 0X5301, 0X93C1, 0X9281, 0X5240,
    0X5601, 0X96C1, 0X9781, 0X5740, 0X5501, 0X95C1, 0X9481, 0X5440,
    0X5C01, 0X9CC1, 0X9DC1, 0X5D40, 0X5F01, 0X9FC1, 0X9E81, 0X5E40,
    0X5A01, 0X9AC1, 0X9B81, 0X5B40, 0X5901, 0X99C1, 0X9881, 0X5840,
    0X4801, 0X88C1, 0X89C1, 0X4940, 0X4B01, 0X8BC1, 0X8A81, 0X4A40,
    0X4E01, 0X8EC1, 0X8FC1, 0X4F40, 0X4D01, 0X8DC1, 0X8C81, 0X4C40,
    0X4401, 0X84C1, 0X85C1, 0X4540, 0X4701, 0X87C1, 0X8681, 0X4740,
    0X4201, 0X82C1, 0X83C1, 0X4340, 0X4101, 0X81C1, 0X8081, 0X4040
};

uint16_t ModbusMaster::calculateCRC(uint8_t* buf, int len) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; i++) {
        crc = (crc >> 8) ^ pgm_read_word(&crcTable[(crc ^ buf[i]) & 0xFF]);
    }
    return crc;
}

void ModbusMaster::sendPacket(uint8_t* buf, int len) {
    uint16_t crc = calculateCRC(buf, len);
    buf[len++] = crc & 0xFF;
    buf[len++] = (crc >> 8) & 0xFF;
    
    if (_enPin >= 0) digitalWrite(_enPin, HIGH); // 只有当引脚有效时才进行逻辑控制
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

    _lastTid++; 
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
                unsigned long lastByteTime = millis();
                int idx = 0;
                while (millis() - lastByteTime < 10) { // 3.5 字符时间判定 (9600 约 3.6ms, 10ms 足够)
                    if (Serial1.available()) {
                        instance->_rxBuf[idx++] = Serial1.read();
                        lastByteTime = millis();
                        if (idx >= 256) break;
                    }
                    vTaskDelay(1);
                }
                
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
                            instance->_status = ST_SUCCESS;
                            if (instance->_pendingCb) {
                                instance->_pendingCb(Modbus::EX_SUCCESS, instance->_lastTid, instance->_pendingData);
                                instance->_pendingCb = nullptr;
                            }
                        } else if (fn == 0x06) { // 写回复 (Echo)
                            instance->_status = ST_SUCCESS;
                        }
                    } else {
                        instance->_status = ST_ERROR;
                        Serial.println("[MB_MASTER] CRC Error");
                    }
                }
            } else if (millis() - instance->_lastPollTime > 1000) {
                instance->_status = ST_TIMEOUT;
                instance->_packetsDropped++;
                if (instance->_pendingCb) {
                    instance->_pendingCb(Modbus::EX_TIMEOUT, instance->_lastTid, instance->_pendingData);
                    instance->_pendingCb = nullptr;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// 原始字节诊断
void ModbusMaster::sendRawByte(uint8_t byte) { Serial1.write(byte); }
int ModbusMaster::availableRaw() { return Serial1.available(); }
uint8_t ModbusMaster::readRawByte() { return Serial1.read(); }
void ModbusMaster::clearRawBuffer() { while(Serial1.available()) Serial1.read(); }
