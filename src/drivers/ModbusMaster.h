#ifndef MODBUS_MASTER_H
#define MODBUS_MASTER_H

#include <Arduino.h>
#include <functional>
#include "PinDefinition.h"
#include "system/SystemTypes.h"

// 自定义 Modbus 结果枚举，解耦第三方库
namespace Modbus {
    enum ResultCode {
        EX_SUCCESS = 0x00,
        EX_TIMEOUT = 0xE4,
        EX_ERROR   = 0xFF
    };
}

typedef std::function<bool(Modbus::ResultCode, uint16_t, void*)> cbTransaction;

/**
 * @class ModbusMaster
 * @brief 自研轻量级 Modbus RTU 驱动：处理协议组包、CRC16 校验及硬件时序。
 * 彻底移除对 modbus-esp8266 的依赖，解决 TID 匹配及阻塞问题。
 */
class ModbusMaster {
public:
    enum TransactionStatus {
        ST_IDLE,      // 空闲
        ST_WAITING,   // 正在等待 ACK (指令已发出)
        ST_SUCCESS,   // 上一次事务成功
        ST_TIMEOUT,   // 上一次事务超时
        ST_ERROR      // 上一次事务出错
    };

    ModbusMaster(int rxPin, int txPin, int enPin, long baud);
    void begin();
    
    TransactionStatus getStatus() const { return _status; }

    /**
     * @brief 异步读取寄存器 (0x03)
     */
    bool asyncRead(uint8_t id, uint16_t addr, uint16_t count, cbTransaction cb, uint16_t* destBuffer);
    
    /**
     * @brief 异步写入单个寄存器 (0x06)
     */
    bool asyncWrite(uint8_t id, uint16_t addr, uint16_t value, cbTransaction cb);
    
    /**
     * @brief 同步写入单个寄存器 (0x06)
     */
    bool syncWrite(uint8_t id, uint16_t addr, uint16_t value);

    /**
     * @brief 广播写入 (ID=0, 无回复)
     */
    bool broadcastWrite(uint16_t addr, uint16_t value);

    // 原始字节诊断接口
    void sendRawBuffer(const uint8_t* buf, int len);
    void sendRawByte(uint8_t byte);
    int availableRaw();
    uint8_t readRawByte();
    void clearRawBuffer();

    uint32_t getPacketsSent() const { return _packetsSent; }
    uint32_t getPacketsDropped() const { return _packetsDropped; }

private:
    int _rxPin, _txPin, _enPin;
    long _baud;
    
    volatile TransactionStatus _status = ST_IDLE;
    unsigned long _lastPollTime = 0;
    
    // 异步回调处理
    cbTransaction     _pendingCb   = nullptr;
    void*             _pendingData = nullptr;
    volatile uint16_t _lastTid     = 0;

    // 通讯缓冲区
    uint8_t _txBuf[32];
    uint8_t _rxBuf[256];
    int     _rxLen = 0;

    uint32_t _packetsSent = 0;
    uint32_t _packetsDropped = 0;
    uint32_t _charTimeUs = 0; 

    // 私有辅助函数
    uint16_t calculateCRC(uint8_t* buf, int len);
    void sendPacket(uint8_t* buf, int len);
    bool validateResponse(uint8_t* buf, int len, uint8_t expectedId, uint8_t expectedFn);

    static ModbusMaster* _instance;
    static void modbusTask(void* param);
    
    TaskHandle_t _taskHandle = NULL;
    SemaphoreHandle_t _mutexBus;
};

#endif
