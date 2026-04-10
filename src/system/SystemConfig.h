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
#define DISCHARGE_MIN_DURATION_MS   2000  // 开启到开始关闭之间的最小保持时间 (2s)
#define BELT_COLLECT_PERIOD_MS      8000  // 收集输送带 (Belt 1)动作时长
#define BELT_STEP_PERIOD_MS         8000  // 输出输送带 (Belt 2) 步进时长

// --- 通讯超时与重试 ---
#define MODBUS_POLL_TIMEOUT_MS      1000  // 单个节点轮询超时上限 (1s)
#define MODBUS_SUCCESS_COUNTER      1     // 启用从机请求成功计数器
#define MODBUS_SERIAL_BAUD          9600  // RS485 总线波特率

// --- 界面交互限制 ---
#define MESSAGE_BOX_DURATION_MS     2000  // 提示框停留时间
#define SPLASH_SCREEN_DURATION_MS   4000  // 开机动画时长
#define UI_REFRESH_RATE_FPS         30    // 界面渲染帧率

// --- Modbus 系统指令字 ---
#define REG_CMD_CONTROL             0x0100 // 总线控制寄存器
#define REG_BELT_REV                0x0000 // 皮带伺服寄存器 (临时改为 0x0100 测试连通性)
#define CMD_SERVO_OPEN              1      // 舵机开启
#define CMD_SERVO_CLOSE             2      // 舵机关闭
#define CMD_TARE                    3      // 节点去皮
#define CMD_PULSE_OPEN_1S           5      // 脉冲开启 (持续1s)

#endif // SYSTEM_CONFIG_H
