#ifndef BELT_H
#define BELT_H

#include "drivers/ModbusMaster.h"
#include <functional>
#include <deque>

/**
 * @enum BeltStatus
 * @brief 皮带电机的运行状态
 */
enum BeltStatus {
    BELT_OFFLINE = 0, // 初始/离线
    BELT_READY   = 1, // 在线就绪
    BELT_MOVING  = 2, // 正在运转
    BELT_FAULT   = 3  // 电机报错/超时
};

// 伺服电机寄存器宏定义 (十六进制地址)
#define REG_POS1_REV     0x0202  // 内部位置指令 1 的位置圈数
#define REG_POS1_PULSE   0x0203  // 内部位置指令 1 的位置圈内脉冲数
#define REG_POS1_SPEED   0x0204  // 内部位置指令控制 1 的移动速度
#define REG_VIRTUAL_IO   0x011F  // 虚拟输入端子状态值 (P3-31)

// 假设默认的编码器单圈脉冲数为 10000
#define PULSES_PER_REV   10000

// 皮带诊断专用的机械转换率宏定议：假设 1 毫米 = 100 脉冲 (可根据需要调整)
#define PULSES_PER_MM    400

/**
 * @class Belt
 * @brief 单个皮带电机驱动类，内置指令队列与状态机，支持平滑的异步指令序列。
 */
class Belt {
public:
    Belt(ModbusMaster* rs485, uint8_t motorId);
    
    void begin();
    
    // 核心更新接口：需在主循环中高频调用
    void update();

    // 基础指令 (推入队列)
    void moveRelative(int32_t pulses);
    void moveDistanceMm(int32_t mm);

    // 在线探测 (推入队列)
    void scan(std::function<void(bool)> onComplete = nullptr);

    uint8_t getId() const { return _id; }
    BeltStatus getStatus() const { return _status; }
    bool isMoving() const { return _status == BELT_MOVING; }
    bool isQueueEmpty() const { return _taskQueue.empty(); }

private:
    struct BeltTask {
        uint16_t reg;
        uint16_t value;
        bool isRead;
        std::function<void(bool)> onDone;
    };

    ModbusMaster* _rs485;
    uint8_t       _id;
    BeltStatus    _status;
    uint16_t      _scanBuffer;
    
    std::deque<BeltTask> _taskQueue;

    void pushTask(uint16_t reg, uint16_t val, bool isRead, std::function<void(bool)> onDone = nullptr);
};

#endif // BELT_H
