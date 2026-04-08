/**
 * @file main.cpp
 * @brief 系统启动入口（App-based Peer Architecture）
 */

#include <Arduino.h>
#include "drivers/TouchScreen.h"
#include "drivers/PinDefinition.h"
#include "logic/CombinationEngine.h"
#include "logic/BeltManager.h"

// Apps
#include "apps/AppDispatcher.h"
#include "apps/AppProduction.h"
#include "apps/AppScan.h"
#include "apps/AppDiagPulse.h"
#include "apps/AppSequentialCtrl.h"
#include "apps/AppBeltDiag.h"

// --- 全局共享状态 ---
SystemContext      sysCtx;

// --- 驱动层 ---
TouchScreen        hw;
UIManager          ui;
ModbusMaster       rs485(PIN_RS485_RX, PIN_RS485_TX, PIN_RS485_TX_EN, RS485_BAUD);

// --- 业务层 ---
PollManager        pollManager(&rs485);
CombinationEngine  engine(290.0f, 310.0f);
BeltManager        conveyor(&rs485, MOTOR_ID_BELT1, MOTOR_ID_BELT2);

// --- 核心调度器 ---
AppDispatcher      dispatcher(&sysCtx, &rs485, &pollManager, &ui, &conveyor);

// --- 互斥锁 (共享状态同步) ---
SemaphoreHandle_t  mutexCtx;

// --- 具体应用实例 ---
AppProduction      appProduction(&sysCtx, &pollManager, &rs485, &engine, &conveyor, nullptr);
AppScan            appScan(&sysCtx, &pollManager, &rs485, nullptr);
AppDiagPulse       appDiagPulse(&sysCtx, &rs485, nullptr);
AppSequentialCtrl  appSeqCtrl(&sysCtx, &rs485, &pollManager, nullptr);
AppBeltDiag        appBeltDiag(&sysCtx, &rs485, &conveyor, nullptr);

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[SYSTEM] Starting App-based Weight Master...");

    mutexCtx = xSemaphoreCreateMutex();

    if (hw.begin()) {
        hw.lvglInit();
        ui.init();
        Serial.println("[SYSTEM] Hardware & UI Init OK.");
    } else {
        Serial.println("[SYSTEM] CRITICAL: HW Init Failed");
        while (1) delay(100);
    }

    // 重新注入互斥锁与状态上下文
    appProduction = AppProduction(&sysCtx, &pollManager, &rs485, &engine, &conveyor, mutexCtx);
    appScan       = AppScan(&sysCtx, &pollManager, &rs485, mutexCtx);
    appDiagPulse  = AppDiagPulse(&sysCtx, &rs485, mutexCtx);
    appSeqCtrl    = AppSequentialCtrl(&sysCtx, &rs485, &pollManager, mutexCtx);
    appBeltDiag   = AppBeltDiag(&sysCtx, &rs485, &conveyor, mutexCtx);

    // 注册应用
    dispatcher.registerApp(&appProduction);
    dispatcher.registerApp(&appScan);
    dispatcher.registerApp(&appDiagPulse);
    dispatcher.registerApp(&appSeqCtrl);
    dispatcher.registerApp(&appBeltDiag);

    // 启动调度器 (内部会启动双核任务)
    dispatcher.begin(MODE_PRODUCTION); 
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
