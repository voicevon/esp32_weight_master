#ifndef MODBUS_MASTER_H
#define MODBUS_MASTER_H

#include <Arduino.h>
#include <ModbusRTU.h>
#include <functional>
#include "PinDefinition.h"

class ModbusMaster {
public:
    // 状态机管理
    enum TransactionStatus {
        ST_IDLE,      // 空闲
        ST_PENDING,   // 指令已排队，即将发送
        ST_WAITING,   // 正在等待 ACK (原子状态)
        ST_SUCCESS,   // 上一次命令成功
        ST_TIMEOUT,   // 上一次命令超时
        ST_ERROR      // 上一次命令出错 (CRC/Exceptions)
    };

    ModbusMaster(int rxPin, int txPin, int enPin, long baud);
    void begin();
    
    // 异步控制核心
    void update(); 
    TransactionStatus getStatus() const { return _status; }
    void startScan(); // 启动全量扫描
    void stopScan();  // 中止全量扫描
    bool isScanning() const { return _isScanning; }
    int getScanProgress() const { return _scanProgress; }

    // 获取缓存数据（非阻塞）
    float getWeight(int id);
    bool isStable(int id);
    uint8_t getDoorPhase(int id);
    bool isNodeOnline(int id);
    const bool* getOnlineStatusArray() const { return _onlineStatus; }

    // 控制指令 (保持同步/阻塞直到完成，因为这些是偶发低频操作)
    bool openDischarge(int id);
    bool closeDischarge(int id);
    bool tare(int id);
    bool broadcastTare(); // 广播去皮 (ID 0)
    bool setPosition(int id, long position, int speed);

    // 诊断接口
    bool performLoopbackTest(); // 执行环回测试
    void sendRawByte(uint8_t byte); // 发送单字节脉冲 (1Hz 诊断用)
    int availableRaw(); // 获取原始串口可用字节数
    uint8_t readRawByte(); // 读取原始串口单字节
    uint32_t getPacketsSent() const { return _packetsSent; }
    uint32_t getPacketsDropped() const { return _packetsDropped; }
    void resetStats() { _packetsSent = 0; _packetsDropped = 0; }

    // 白名单管理
    void savePollWhitelist();
    void loadPollWhitelist();
    bool isWhitelisted(int id) const { return (id >= 1 && id <= 20) ? _pollWhitelist[id] : false; }

private:
    int _rxPin, _txPin, _enPin;
    long _baud;
    ModbusRTU _mb;

    // 轮询与事务状态机
    uint8_t _currentPollId = 1;
    TransactionStatus _status = ST_IDLE;
    unsigned long _lastPollTime = 0;
    
    // 节点状态缓存
    float _cachedWeights[21]; 
    bool _isStable[21];
    uint8_t _doorPhases[21];
    bool _onlineStatus[21];
    bool _pollWhitelist[21]; // 通讯白名单：仅名单内节点参与普通轮询
    uint16_t _tempRegs[3];     
    
    // 统计项
    uint32_t _packetsSent = 0;
    uint32_t _packetsDropped = 0;
    uint8_t _failCounters[21];       // Consecutive failures per node
    uint32_t _nodeErrorStats[21];    // Cumulative errors per node
    Modbus::ResultCode _nodeLastResult[21]; // Latest error code per node

    bool _isScanning = false;
    int _scanProgress = 0;

    bool waitTransaction(uint8_t id);
    static ModbusMaster* _instance;
    static bool cbPoll(Modbus::ResultCode event, uint16_t transactionId, void* data);
    static bool cbSync(Modbus::ResultCode event, uint16_t transactionId, void* data);
    
    // 内部事务执行器
    bool execCmd(uint8_t id, std::function<bool()> startFunc);

    SemaphoreHandle_t _mutexBus; // 保护底层 Modbus 对象和状态机
};

#endif
