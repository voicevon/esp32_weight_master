#ifndef MODBUS_MASTER_H
#define MODBUS_MASTER_H

#include <Arduino.h>
#include <ModbusRTU.h>
#include "PinDefinition.h"

class ModbusMaster {
public:
    ModbusMaster(int rxPin, int txPin, int enPin, long baud);
    void begin();
    
    // 异步控制核心
    void update(); 
    void startScan(); // 启动全量扫描
    bool isScanning() const { return _isScanning; }
    int getScanProgress() const { return _scanProgress; }

    // 获取缓存数据（非阻塞）
    float getWeight(int id);
    bool isNodeOnline(int id);
    const bool* getOnlineStatusArray() const { return _onlineStatus; }

    // 控制指令 (保持同步/阻塞直到完成，因为这些是偶发低频操作)
    bool openDischarge(int id);
    bool closeDischarge(int id);
    bool tare(int id);
    bool setPosition(int id, long position, int speed);

    // 诊断接口
    bool performLoopbackTest(); // 执行环回测试
    void sendRawByte(uint8_t byte); // 发送单字节脉冲 (1Hz 诊断用)
    int availableRaw(); // 获取原始串口可用字节数
    uint8_t readRawByte(); // 读取原始串口单字节
    uint32_t getPacketsSent() const { return _packetsSent; }
    uint32_t getPacketsDropped() const { return _packetsDropped; }
    void resetStats() { _packetsSent = 0; _packetsDropped = 0; }

private:
    int _rxPin, _txPin, _enPin;
    long _baud;
    ModbusRTU _mb;

    // 轮询状态机
    uint8_t _currentPollId = 1;
    bool _isWaiting = false;
    unsigned long _lastPollTime = 0;
    
    float _cachedWeights[21]; // 索引 1-20
    bool _onlineStatus[21];
    uint16_t _tempRegs[2];     // 用于接收 32bit Float 的临时缓冲区
    
    // 统计项
    uint32_t _packetsSent = 0;
    uint32_t _packetsDropped = 0;

    bool _isScanning = false;
    int _scanProgress = 0;

    bool waitTransaction(uint8_t id);
    static ModbusMaster* _instance;
    static bool cbPoll(Modbus::ResultCode event, uint16_t transactionId, void* data);
};

#endif
