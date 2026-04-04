---
name: waveshare-s3-touch-expert
description: (本项目专属) 专门处理 Waveshare ESP32-S3-Touch-LCD-7 触控调试。在项目中后期需要重置 I2C 配置或调试触摸无反应时触发。强行要求 I2C 速率降为 100kHz 并检查 LVGL Tick 注入。
---

# Waveshare-S3-Touch-Expert (Project Private)

本 Skill 是为本项目量身定制的，旨在通过“一键式”标准流程，稳固 GT911 触摸屏在 ESP32-S3 上的表现。

## 🚀 黄金初始化法则

### 1. 物理层：不要信任 400kHz！
本项目总线在大电流背光开启时，400kHz 会产生巨大的数据幽灵。
**强制指令**：`Wire.setClock(100000);`。

### 2. 时钟层：LVGL 心跳步进
如果没有 `lv_tick_inc(5)` 放在 `loop()`，任何触摸读取回调 (`indev_read`) 都不会执行。

### 3. 配置层：强制 Fresh Flag
GT911 有时会由于 I2C 读取错误将配置状态锁死。
**强制动作**：
- 向 `0x8100` 写入 `0x01`（Fresh Config）
- 向 `0x8040` 写入 `0x00`（退出休眠并重置状态）

## 📚 资产查阅
- **配置全量表**：[config_array.md](references/config_array.md)
- **代码存根**：[boilerplate.cpp](assets/boilerplate.cpp)
