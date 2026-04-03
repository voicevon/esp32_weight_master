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
    _mb.begin(&Serial1, _enPin);
    _mb.master();
    
    // 启动高优先级心跳任务 (核心 1, 优先级 15)
    xTaskCreatePinnedToCore(modbusTask, "MB_Task", 4096, this, 15, &_taskHandle, 1);
}

void ModbusMaster::modbusTask(void* param) {
    ModbusMaster* instance = (ModbusMaster*)param;
    while (true) {
        if (xSemaphoreTake(instance->_mutexBus, pdMS_TO_TICKS(10)) == pdTRUE) {
            instance->_mb.task(); // 驱动协议栈
            
            // 内部事务超时监控
            if (instance->_status == ST_WAITING) {
                if (millis() - instance->_lastPollTime > 1000) {
                    Serial.println("[MB_MASTER] Transaction Timeout Detected");
                    instance->_status = ST_TIMEOUT;
                    instance->_packetsDropped++;
                    
                    // --- 超时强制通知业务层 (TID 匹配机制) ---
                    if (instance->_pendingCb) {
                        Serial.println("[MB_MASTER] Software Timeout Triggered. Clearing UART Buffer.");
                        instance->clearRawBuffer(); // 软件超时后清理 UART 缓冲区，防止脏数据干扰后续包
                        instance->_pendingCb(Modbus::EX_TIMEOUT, instance->_lastTid, instance->_pendingData);
                        instance->_pendingCb = nullptr;
                    }
                }
            }
            xSemaphoreGive(instance->_mutexBus);
        }
        vTaskDelay(pdMS_TO_TICKS(5)); // 通讯步进周期 5ms
    }
}

bool ModbusMaster::asyncRead(uint8_t id, uint16_t addr, uint16_t count, cbTransaction cb, uint16_t* destBuffer) {
    if (xSemaphoreTake(_mutexBus, pdMS_TO_TICKS(10)) != pdTRUE) return false;
    
    // 只有在空置或上次已结束时才接受新事务
    if (_status != ST_IDLE && _status != ST_SUCCESS && _status != ST_TIMEOUT && _status != ST_ERROR) {
        xSemaphoreGive(_mutexBus);
        return false;
    }

    _status       = ST_WAITING;
    _lastPollTime = millis();
    _packetsSent++;

    _pendingCb   = cb;
    _pendingData = (void*)destBuffer;
    
    // 包装原始回调以自动更新状态 (加入 TID 匹配监测)
    _lastTid = _mb.readHreg(id, addr, destBuffer, count, [](Modbus::ResultCode event, uint16_t tid, void* d) {
        if (_instance) {
            // 只有当 TID 匹配或为 0 (ModbusRTU 库硬编码 RTU 回调 TID 为 0) 时才更新
            if (tid == _instance->_lastTid || tid == 0) {
                _instance->_status = (event == Modbus::EX_SUCCESS) ? ST_SUCCESS : ST_ERROR;
                if (_instance->_pendingCb) {
                    _instance->_pendingCb(event, tid, d);
                    _instance->_pendingCb = nullptr;
                }
            } else {
                Serial.printf("[MB_MASTER] Ignored Ghost Response: Expect TID %d, Got %d\n", _instance->_lastTid, tid);
            }
        }
        return true;
    });

    xSemaphoreGive(_mutexBus);
    return true;
}

bool ModbusMaster::syncWrite(uint8_t id, uint16_t addr, uint16_t value) {
    if (xSemaphoreTake(_mutexBus, pdMS_TO_TICKS(1000)) != pdTRUE) return false;
    
    _status = ST_WAITING;
    _lastPollTime = millis();
    _mb.writeHreg(id, addr, value, [](Modbus::ResultCode event, uint16_t tid, void* d) {
        if (_instance) _instance->_status = (event == Modbus::EX_SUCCESS) ? ST_SUCCESS : ST_ERROR;
        return true;
    });

    // 等待事务完成 (带超时机制的阻塞)
    bool result = false;
    unsigned long start = millis();
    while (millis() - start < 1000) {
        _mb.task(); // 强制手动步进协议栈
        if (_status == ST_SUCCESS) { result = true; break; }
        if (_status == ST_ERROR || _status == ST_TIMEOUT) break;
        vTaskDelay(1);
    }
    
    xSemaphoreGive(_mutexBus);
    return result;
}

bool ModbusMaster::broadcastWrite(uint16_t addr, uint16_t value) {
    if (xSemaphoreTake(_mutexBus, pdMS_TO_TICKS(1000)) != pdTRUE) return false;
    // 广播 ID 为 0，不等待 ACK
    bool ok = _mb.writeHreg(0, addr, value);
    xSemaphoreGive(_mutexBus);
    return ok;
}

// 原始脉冲诊断接口
void ModbusMaster::sendRawByte(uint8_t byte) { Serial1.write(byte); }
int ModbusMaster::availableRaw() { return Serial1.available(); }
uint8_t ModbusMaster::readRawByte() { return Serial1.read(); }
void ModbusMaster::clearRawBuffer() { while(Serial1.available()) Serial1.read(); }
