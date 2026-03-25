#ifndef PIN_DEFINITION_H
#define PIN_DEFINITION_H

/* 
 * ESP32 Weight Master - 硬件引脚与寄存器统一定义
 * 本文件由 Antigravity 自动生成，用于解耦机械布局文档与代码实现。
 */

// --- I2C 显示屏 (OLED 128x64) ---
#define PIN_I2C_SDA      23
#define PIN_I2C_SCL      22

// --- RS485 通信 (Serial 2) ---
#define PIN_RS485_RX     16
#define PIN_RS485_TX     4
#define PIN_RS485_TX_EN  5     // 逻辑：HIGH 为发送，LOW 为接收
#define RS485_TX_ENABLE  HIGH 
#define RS485_RX_ENABLE  LOW  
#define RS485_BAUD       9600

// --- 旋转编码器 (HMI 交互) ---
#define PIN_ENCODER_A       18  
#define PIN_ENCODER_B       19  
#define PIN_ENCODER_BUTTON  21  

// --- 从机与电机 ID 分配 ---
#define NUM_SLAVES      20  // 称重单元总数 (1-20)
#define MOTOR_ID_BELT1  21  // 收集带 (一级)
#define MOTOR_ID_BELT2  22  // 输出带 (二级)

// --- Modbus 寄存器地址 (必须与从机保持一致) ---
#define REG_WEIGHT_H    0x0000  // 称重值高位 (Float)
#define REG_STATUS      0x0002  // 运行状态
#define REG_CTRL_CMD    0x0100  // 控制指令 (1:开, 2:关, 3:去皮)

#endif // PIN_DEFINITION_H
