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
    MODE_ABOUT          // 关于界面
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

#endif
