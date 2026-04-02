#ifndef MODBUS_MASTER_H
#define MODBUS_MASTER_H

#include <Arduino.h>
#include <ModbusRTU.h>
#include <functional>
#include "PinDefinition.h"
#include "SystemTypes.h"

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
    
    // 异步控制核心 (传入当前全系统运行模式以决策资源策略)
    void update(OperationMode curMode); 
    TransactionStatus getStatus() const { return _status; }
    void startScan(); // 启动全量扫描
    void stopScan();  // 中止全量扫描
    OperationMode getMode() const { return _currentMode; }
    int getScanProgress() const { return _scanProgress; }
    int getCurrentScanCycle() const { return _scanCycle; }
    const bool (*getScanHistory())[21] { return _scanHistory; } // 返回 5x21 历史记录数组

    // 获取缓存数据（非阻塞）
    float getWeight(int id);
    bool isStable(int id);
    uint8_t getDoorPhase(int id);
    bool isNodeOnline(int id);
    int getUnstableCount() const { return _unstableCount; } // 新增：正在抖动的节点数量
    const bool* getOnlineStatusArray() const { return _onlineStatus; }
    
    // 状态机管理接口
    NodeStatus getNodeStatus(int id) const { return (id >= 1 && id <= 20) ? _nodeStatus[id] : NODE_DIRTY; }
    void setNodeStatus(int id, NodeStatus s);

    // 控制指令 (保持同步/阻塞直到完成，因为这些是偶发低频操作)
    bool openDischarge(int id);
    bool openDischarge1S(int id); // 脉冲式开门 (1秒后自动关闭)
    bool closeDischarge(int id);
    bool tare(int id);
    bool broadcastTare(); // 广播去皮 (ID 0)
    bool setPosition(int id, long position, int speed);

    // 诊断接口
    bool performLoopbackTest(); // 执行环回测试
    void sendRawByte(uint8_t byte); // 发送单字节脉冲 (1Hz 诊断用)
    void clearRawBuffer();         // 清除接收缓冲区
    int availableRaw();            // 获取原始串口可用字节数
    uint8_t readRawByte();         // 读取原始串口单字节
    uint32_t getPacketsSent() const { return _packetsSent; }
    uint32_t getPacketsDropped() const { return _packetsDropped; }
    void resetStats() { _packetsSent = 0; _packetsDropped = 0; }

    // 白名单管理
    void savePollWhitelist();
    void loadPollWhitelist();
    void generateWhitelistFromScan();
    bool isWhitelisted(int id) const { return (id >= 1 && id <= 20) ? _pollWhitelist[id] : false; }
    uint32_t getWhitelistMask() const; // 新增：获取所有节点的白名单状态位集

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
    uint16_t _lastDataIds[21];   // 最新读到的 ID
    uint16_t _targetDataIds[21]; // 期待达到的新鲜 ID
    
    bool _onlineStatus[21];
    bool _pollWhitelist[21]; // 通讯白名单：仅名单内节点参与普通轮询
    NodeStatus _nodeStatus[21]; // 节点级状态机缓存
    uint16_t _tempRegs[6];     
    
    // 统计项
    uint32_t _packetsSent = 0;
    uint32_t _packetsDropped = 0;
    uint8_t _failCounters[21];       // Consecutive failures per node
    uint32_t _nodeErrorStats[21];    // Cumulative errors per node
    Modbus::ResultCode _nodeLastResult[21]; // Latest error code per node
    int _unstableCount = 0; // 新增：正在抖动的节点数量

    void handlePollingCycle(OperationMode curMode);
    bool waitTransaction(uint8_t id);
    static ModbusMaster* _instance;
    static bool cbPoll(Modbus::ResultCode event, uint16_t transactionId, void* data);
    static bool cbSync(Modbus::ResultCode event, uint16_t transactionId, void* data);
    
    // 内部事务执行器
    bool execCmd(uint8_t id, std::function<bool()> startFunc);

    OperationMode _currentMode = MODE_IDLE; // 当前通讯执行模式
    int _scanProgress = 0;
    int _scanCycle = 0;       // 当前重试轮次 0-4
    bool _scanHistory[5][21]; // 记录 5 轮扫描中每一轮的在线状态

    static void modbusTask(void* param); // 内部高优先级通讯任务入口
    TaskHandle_t _taskHandle = NULL;
    SemaphoreHandle_t _mutexBus; // 保护底层 Modbus 对象和状态机
};

#endif
