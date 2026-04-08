#ifndef PIN_DEFINITION_H
#define PIN_DEFINITION_H

/* 
 * ESP32-S3 Weight Master - 硬件引脚与寄存器统一定义
 * 本文件管理业务层的引脚常量。LCD 和 Touch 的引脚归 TouchScreen 管。
 */

// --- RS485 通信 (Serial 1/2) ---
// 根据 esp32_s3_lcd 的通信验证基础，RX:15，TX:16
#define PIN_RS485_RX     15
#define PIN_RS485_TX     16
#define PIN_RS485_TX_EN  -1    // 硬件自动收发切换，无需使能引脚
#define RS485_TX_ENABLE  HIGH 
#define RS485_RX_ENABLE  LOW  
#define RS485_BAUD       9600

// --- 旋转编码器 (HMI 交互 - 在新大屏中可能不再需要，暂时保留防止编译报错) ---
#define PIN_ENCODER_A       -1
#define PIN_ENCODER_B       -1  
#define PIN_ENCODER_BUTTON  -1  

// --- 从机与电机 ID 分配 ---
#define NUM_SLAVES      20  // 称重单元总数 (1-10，或者之前的 20)
#define MOTOR_ID_BELT1  21  // 收集带 (一级)
#define MOTOR_ID_BELT2  22  // 输出带 (二级)

// --- Modbus 寄存器地址 (必须与从机保持一致) ---
#define REG_WEIGHT_H    0x0000  // 称重值高位 (Float)
#define REG_STATUS      0x0002  // 运行状态

#endif // PIN_DEFINITION_H
