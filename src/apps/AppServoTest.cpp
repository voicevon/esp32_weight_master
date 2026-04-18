#include "AppServoTest.h"
#include <Arduino.h>

AppServoTest::AppServoTest(SystemContext* ctx, ModbusMaster* rs485, NodeManager* nodeMgr)
    : _ctx(ctx), _rs485(rs485), _nodeMgr(nodeMgr), _sequencer(ctx, rs485, nodeMgr) {}

void AppServoTest::onEnter() {
    Serial.println("[AppServoTest] Entering Maintenance Mode...");
    // 清理界面同步状态
    memset(_ctx->ui.servoRealStates, 0, sizeof(_ctx->ui.servoRealStates));
}

void AppServoTest::onLoop() {
    _sequencer.update(millis());
}

void AppServoTest::onExit() {
    Serial.println("[AppServoTest] Exiting Maintenance Mode.");
    _sequencer.stop();
}

void AppServoTest::triggerServo(int id, bool open) {
    if (id < 1 || id > 20) return;
    if (_sequencer.isBusy()) return; // 序列执行期间禁止手动单点

    Serial.printf("[AppServoTest] Node %d: %s\n", id, open ? "OPEN" : "CLOSE");
    _rs485->asyncWrite(id, 0x011F, open ? 0x01 : 0x00, nullptr);
    
    // 预更新 UI 状态 (避免等待轮询产生的延迟感)
    _ctx->ui.servoRealStates[id] = open ? 1 : 0;
}

void AppServoTest::triggerGlobalServo(bool open) {
    // 动作码：1=开启, 2=关闭
    _sequencer.start(0x011F, open ? 0x01 : 0x00, 150, open ? 1 : 2);
    
    const char* taskName = open ? "正在开启白名单舵机..." : "正在关闭白名单舵机...";
    strncpy(_ctx->prog.statusText, taskName, 32);
    _ctx->prog.dirtyFlags |= DF_SYS_STATUS;
}

