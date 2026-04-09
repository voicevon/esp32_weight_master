#include "AppServoTest.h"
#include <Arduino.h>

AppServoTest::AppServoTest(SystemContext* ctx, ModbusMaster* rs485)
    : _ctx(ctx), _rs485(rs485) {}

void AppServoTest::onEnter() {
    Serial.println("[AppServoTest] Entering Maintenance Mode...");
    // 清理界面同步状态
    memset(_ctx->ui.servoRealStates, 0, sizeof(_ctx->ui.servoRealStates));
}

void AppServoTest::onLoop() {
    // 逻辑层目前通过 PollManager 的 fillUISnapshot 间接更新 UI 状态
    // 此处无需复杂逻辑
}

void AppServoTest::onExit() {
    Serial.println("[AppServoTest] Exiting Maintenance Mode.");
}

void AppServoTest::triggerServo(int id, bool open) {
    if (id < 1 || id > 20) return;

    // 根据协议文档：
    // 地址 0x011F 控制门机动作 (虚拟数字输出)
    // 0x01: 开启, 0x00: 关闭
    Serial.printf("[AppServoTest] Node %d: %s\n", id, open ? "OPEN" : "CLOSE");
    _rs485->asyncWrite(id, 0x011F, open ? 0x01 : 0x00, nullptr);
    
    // 预更新 UI 状态 (避免等待轮询产生的延迟感)
    _ctx->ui.servoRealStates[id] = open ? 1 : 0;
}
