#ifndef RS485_MASTER_H
#define RS485_MASTER_H

#include <Arduino.h>
#include <ModbusRTU.h>

// 寄存器地址定义（必须与从机保持一致）
#define REG_WEIGHT_H    0x0000
#define REG_STATUS      0x0002
#define REG_CTRL_CMD    0x0100

class Rs485Master {
public:
    Rs485Master(int rxPin, int txPin, int enPin, long baud);
    void begin();
    
    // 异步控制核心
    void update(); 

    // 获取缓存数据（非阻塞）
    float getWeight(int id);
    bool isNodeOnline(int id);

    // 控制指令 (保持同步/阻塞直到完成，因为这些是偶发低频操作)
    bool openDischarge(int id);
    bool closeDischarge(int id);
    bool tare(int id);
    bool setPosition(int id, long position, int speed);

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

    bool waitTransaction(uint8_t id);
    static bool cbPoll(Modbus::ResultCode event, uint16_t transactionId, void* data);
};

#endif
