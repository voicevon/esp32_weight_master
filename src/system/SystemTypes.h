#ifndef SYSTEM_TYPES_H
#define SYSTEM_TYPES_H

/**
 * @brief 系统全局运行模式
 * 不同模式对应不同的硬件资源占用策略，确保诊断与生产互斥
 */
enum OperationMode {
    MODE_IDLE,          // 待载状态 (不轮询)
    MODE_PRODUCTION,    // 生产模式 (全速轮询)
    MODE_DIAG_PULSE,    // 诊断：1Hz 脉冲测试 (独占总线)
    MODE_DIAG_SCAN,     // 诊断：全量扫描 (独占总线)
    MODE_DIAG_DETAIL,   // 诊断：节点详情查看
    MODE_CONFIGURATION, // 配置模式
    MODE_SEQUENTIAL_CTRL, // 序列化控制模式 (开/关/置零)
    MODE_ABOUT           // 关于界面
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
    SYS_DISCHARGING,   // 下料中
    SYS_TRANSFER_B1,   // 级联传送中 (B1)
    SYS_STEPPING_B2    // 步进传送中 (B2)
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
struct ProductionState {
    SystemStatus status;          // READY, DISCHARGING...
    float        lastBatchWeight; // 最近成功的组合重量
    uint32_t     selectionMask;   // 下料掩码
};

/**
 * @brief 诊断与全系统扫描数据
 */
struct DiagContext {
    int     scanProgress;      // 0-20
    int     currentScanCycle;  // 0-4
    bool    scanResults[5][21]; // 1-based id index -> size 21
    uint8_t diagLastSent;
    char    diagRxHex[128];
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
    float         stableWeightSum;    // 已稳总重 (白名单内在线稳节点)
    float         unstableWeightSum;  // 未稳总值 (白名单内在线非稳节点)
};

#endif
