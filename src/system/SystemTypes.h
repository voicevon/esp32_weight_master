#ifndef SYSTEM_TYPES_H
#define SYSTEM_TYPES_H

#include <Arduino.h>

/**
 * @brief 系统全局运行模式
 * 不同模式对应不同的硬件资源占用策略，确保诊断与生产互斥
 */
enum OperationMode {
    MODE_IDLE,          // 待载状态 (不轮询)
    MODE_PRODUCTION,    // 生产模式 (全速轮询)
    MODE_DIAG_SCAN,     // 诊断：全量扫描 (独占总线)
    MODE_DIAG_DETAIL,   // 诊断：节点详情查看
    MODE_CONFIGURATION, // 配置模式
    MODE_SEQUENTIAL_CTRL, // 序列化控制模式 (开/关/置零)
    MODE_SERVO_TEST,     // 舵机维护测试模式 (独占总线)
    MODE_BELT_DIAG,      // 皮带诊断跑距测试模式 (独占总线)
    MODE_MODBUS_DIAG,    // Modbus 诊断器模式 (独占总线)
    MODE_ABOUT           // 关于界面
};
 
/**
 * @brief 日志输出级别
 */
enum LogLevel {
    LOG_NONE    = 0,
    LOG_ERROR   = 1,
    LOG_INFO    = 2,
    LOG_VERBOSE = 3
};

/**
 * @brief 诊断子模式 (用于总线助手)
 */
enum DiagSubMode {
    DIAG_SUB_PULSE,    // 原始脉冲模式 (1Hz 递增字节)
    DIAG_SUB_COMMAND   // 结构化指令模式 (电机控制报文)
};

/**
 * @brief UI 界面状态
 */
enum UIState {
    SCREEN_SPLASH,
    SCREEN_MAIN,
    SCREEN_MENU,
    SCREEN_DETAIL,
    SCREEN_EDIT,
    SCREEN_RS485_DIAG,
    SCREEN_SCAN,
    SCREEN_MESSAGE,     // 短信提示界面
    SCREEN_SEQUENTIAL_PROGRESS // 序列动作进度界面
};

/**
 * @brief 系统业务状态 (枚举规范)
 */
enum SystemStatus {
    SYS_INIT,          // 初始化中
    SYS_READY,         // 准备就绪
    SYS_SEQ_DROP,      // 逐个下料中
    SYS_SEQ_CLOSE,     // 逐个关门中 (新)
    SYS_SETTLE_STABLE, // 沉降稳定中
    SYS_BELT_A,        // 皮带 A 运行中
    SYS_BELT_B         // 皮带 B 步进中
};

/**
 * @brief 节点级独立状态机 (保证数据新鲜度)
 */
enum NodeStatus {
    NODE_STABLE,       // 就绪：重量已读回且稳定，可参与组合
    NODE_LOCKED,       // 锁定：已被组合引擎选中，正等待下料
    NODE_DISCHARGING,  // 下料中：出料指令已发送
    NODE_DIRTY,        // 脏数据：下料结束，缓存仍为旧重，严禁参与组合
    NODE_REFRESHING    // 刷新中：已读回第1个新重量，等待稳定
};

/**
 * @brief 系统生产设置 (持久化参数)
 */
struct ProductionParams {
    float targetMin;
    float targetMax;
    float accumulatedWeight;
    bool  isProductionEnabled;
};

/**
 * @brief 生产运行动态 (非持久化)
 */
struct WSProductionState {
    SystemStatus sysStatus;       // READY, DISCHARGING...
    char         statusText[32];  // 用于 UI 显示的动态文案
    float        batchWeight;     // 最近成功的组合重量
    uint32_t     idMask;          // 下料掩码
};


/**
 * @brief UI 渲染快照 (无锁副本，由 uiLoop 填充)
 */
struct UISnapshot {
    OperationMode curMode;
    float         currentWeights[21]; // 1-20
    bool          stableNodes[21];
    bool          onlineNodes[21];
    bool          whitelistedNodes[21];
    int8_t        servoRealStates[21]; // 0=紫色(关), 1=绿色(开), -1=红色(故障/离线)
    float         stableWeightSum;    // 已稳总重 (白名单内在线稳节点)
    float         unstableWeightSum;  // 未稳总值 (白名单内在线非稳节点)
    
    // 序列控制进度 (用于 UI 锁定与反馈)
    bool          isTareRunning;      // 是否正在执行全局置零
    int           tareProgress;       // 置零进度 (0-100)

    // 扫描与诊断同步 (由 PollManager 填充)
    int           scanProgress;
    int           scanCycle;
    bool          scanResults[5][21];
    int           diagTxCount;
    uint8_t       diagTxValue;
    int           diagRxCount;
    char          diagRxHex[128];

    // 皮带诊断同步 (由 AppBeltDiag 填充)
    bool          beltDiagScanning;   // 是否正在扫描皮带节点
    int8_t        beltStatus[2];      // 0=离线, 1=就绪, 2=运行, 3=故障
    bool          beltIsMoving[2];    // 皮带1和皮带2是否正在运行

    // 串口助手/Modbus 诊断同步 (由 AppModbusDiag 填充)
    DiagSubMode   diagSubMode;        // 当前激活的诊断子模式
    int           diagTargetNodeId;   // 指令模式的目标节点 ID
    bool          serialAutoSend;     // 是否自动循环发送 (脉冲模式)
    char          serialTxHex[64];    // 当前发送的报文 Hex
    char          serialRxHex[64];    // 最近收到的报文 Hex
    char          serialLogLine[128]; // 最近生成的一行日志 (用于 Append)
    uint32_t      serialLogTick;      // 日志更新序列号
};

#endif
