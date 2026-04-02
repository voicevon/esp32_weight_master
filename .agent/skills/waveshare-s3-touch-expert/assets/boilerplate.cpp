/**
 * @file   waveshare_s3_touch_boilerplate.cpp
 * @brief  生产级 GT911 初始化与 LVGL 同步模板
 * @author Waveshare-S3-Touch-Expert (Local Skill)
 */

#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>

/* --- 核心定义：不要修改引脚，除非你有官方手册 --- */
#define TOUCH_SDA  8
#define TOUCH_SCL  9
#define TOUCH_INT  4
#define CH422G_ADDR_IO_LOW  0x38

uint8_t touch_addr = 0x5D; // 默认 0x5D，备选 0x14

/* --- 强制：总线降速保证 --- */
void init_i2c_bus() {
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setClock(100000); // 必须 100kHz，400kHz 有电磁干扰
}

/* --- 强制：黄金握手时序 (Reset Dance) --- */
void reset_gt911() {
    pinMode(TOUCH_INT, OUTPUT);
    digitalWrite(TOUCH_INT, LOW); 
    delay(10); // Hold INT LOW before RST

    // 使用 CH422G 复位 RST 引脚 (IO 1)
    Wire.beginTransmission(CH422G_ADDR_IO_LOW);
    Wire.write(0x0C); // RST LOW (0b00001100)
    Wire.endTransmission();
    delay(100); 

    Wire.beginTransmission(CH422G_ADDR_IO_LOW);
    Wire.write(0x0E); // RST HIGH (0b00001110)
    Wire.endTransmission();
    delay(200); // Wait for boot

    pinMode(TOUCH_INT, INPUT); // Release INT
}

/* --- 强制：Fresh Config 激活 --- */
void activate_touch_sensor() {
    Wire.beginTransmission(touch_addr);
    Wire.write(0x81); Wire.write(0x00); // 寄存器 0x8100
    Wire.write(0x01); // 写入 0x01 激活 Fresh Flag
    Wire.endTransmission();

    Wire.beginTransmission(touch_addr);
    Wire.write(0x80); Wire.write(0x40); // 激活 Command Reg
    Wire.write(0x00); 
    Wire.endTransmission();
}

/* --- 强制：LVGL 调度心脏 --- */
void loop() {
    lv_tick_inc(5);      // ！！如果没有这一行，触摸永远不会被调度！！
    lv_timer_handler();  // 处理 LVGL 任务
    delay(5);            // 控制 CPU 占用
}
