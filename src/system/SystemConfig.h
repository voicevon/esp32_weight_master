#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

/**
 * @file SystemConfig.h
 * @brief 系统全局业务配置常量 (全英文大写规范)
 */

// --- 生产核心逻辑 ---
#define CALC_ENGINE_INTERVAL_MS     150   // 称重组合计算频率
// --- 机械动作延迟 (MS) ---
#define DISCHARGE_PULSE_MS          1000  // 下料斗脉冲开启时间 (1s 自动关闭)
#define DISCHARGE_SETTLE_MS         1200  // 下料后等待排空并复位的安全余量
#define BELT_COLLECT_PERIOD_MS      2500  // 收集输送带 (Belt 1) 动作时长
#define BELT_STEP_PERIOD_MS         1200  // 输出输送带 (Belt 2) 步进时长

// --- 通讯超时与重试 ---
#define MODBUS_POLL_TIMEOUT_MS      100   // 单个节点轮询超时上限 (从 1500ms 降低以防死等)
#define MODBUS_SERIAL_BAUD          9600  // RS485 总线波特率

// --- 界面交互限制 ---
#define MESSAGE_BOX_DURATION_MS     2000  // 提示框停留时间
#define SPLASH_SCREEN_DURATION_MS   4000  // 开机动画时长
#define UI_REFRESH_RATE_FPS         30    // 界面渲染帧率

#endif // SYSTEM_CONFIG_H
