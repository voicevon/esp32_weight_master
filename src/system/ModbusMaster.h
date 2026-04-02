#ifndef MODBUS_MASTER_H
#define MODBUS_MASTER_H

#include <Arduino.h>
#include <ModbusRTU.h>
#include <functional>
#include "PinDefinition.h"
#include "SystemTypes.h"

/**
 * @class ModbusMaster
 * @brief 纯粹的通讯驱动层：负责协议栈心跳、UART 时序和原子事务执行。
 * [优化] 移除了内部共享缓冲区，强制显式内存管理。
 */
class ModbusMaster {
public:
    enum TransactionStatus {
        ST_IDLE,      // 空闲
        ST_PENDING,   // 指令已排队
        ST_WAITING,   // 正在等待 ACK
        ST_SUCCESS,   // 上一次事务成功
        ST_TIMEOUT,   // 上一次事务超时
        ST_ERROR      // 上一次事务出错
    };

    ModbusMaster(int rxPin, int txPin, int enPin, long baud);
    void begin();
    
    TransactionStatus getStatus() const { return _status; }

    /**
     * @brief 异步读取寄存器
     * @param destBuffer 必须由调用方保证在回调触发前生命周期有效
     */
    bool asyncRead(uint8_t id, uint16_t addr, uint16_t count, cbTransaction cb, uint16_t* destBuffer);
    
    // 控制指令 (保持同步)
    bool syncWrite(uint8_t id, uint16_t addr, uint16_t value);
    bool broadcastWrite(uint16_t addr, uint16_t value);

    // 原始字节诊断
    void sendRawByte(uint8_t byte);
    int availableRaw();
    uint8_t readRawByte();
    void clearRawBuffer();

    // 统计数据
    uint32_t getPacketsSent() const { return _packetsSent; }
    uint32_t getPacketsDropped() const { return _packetsDropped; }

private:
    int _rxPin, _txPin, _enPin;
    long _baud;
    ModbusRTU _mb;

    volatile TransactionStatus _status = ST_IDLE;
    unsigned long _lastPollTime = 0;

    uint32_t _packetsSent = 0;
    uint32_t _packetsDropped = 0;

    static ModbusMaster* _instance;
    static void modbusTask(void* param);
    
    TaskHandle_t _taskHandle = NULL;
    SemaphoreHandle_t _mutexBus;
};

#endif
