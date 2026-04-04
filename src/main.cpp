/**
 * @file main.cpp
 * @brief 系统启动入口（精简版）
 *
 * 职责边界：
 *   - 实例化所有顶层对象（驱动层 → 业务层 → 应用层）
 *   - 初始化硬件与 UI
 *   - 调用 AppController::begin() 启动双核任务
 *
 * 全部业务逻辑已迁移至 AppController.cpp（Phase 1 重构）
 */

#include <Arduino.h>
#include "HardwareManager.h"
#include "UIManager.h"
#include "system/ModbusMaster.h"
#include "system/PinDefinition.h"
#include "logic/CombinationEngine.h"
#include "logic/ConveyorController.h"
#include "system/PollManager.h"
#include "AppController.h"

// --- 驱动层 ---
HardwareManager    hw;
UIManager          ui;
ModbusMaster       rs485(PIN_RS485_RX, PIN_RS485_TX, PIN_RS485_TX_EN, RS485_BAUD);

// --- 业务层 ---
PollManager        pollManager(&rs485);
CombinationEngine  engine(290.0f, 310.0f);
ConveyorController conveyor(&rs485, MOTOR_ID_BELT1, MOTOR_ID_BELT2);

// --- 应用层协调器 ---
AppController      appCtrl(&rs485, &pollManager, &engine, &conveyor, &ui);

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[SYSTEM] Starting Waveshare ESP32-S3 Weight Master...");

    if (hw.begin()) {
        hw.lvglInit();
        ui.init();
        Serial.println("[SYSTEM] Hardware & UI Init OK.");
    } else {
        Serial.println("[SYSTEM] CRITICAL: HW Init Failed");
        while (1) delay(100);
    }

    appCtrl.begin(); // 启动双核任务并进入生产模式
}

void loop() {
    // 业务已全部交由 FreeRTOS 双核任务处理，loop 保持空闲
    vTaskDelay(pdMS_TO_TICKS(1000));
}
