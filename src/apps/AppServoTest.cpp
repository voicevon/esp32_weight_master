#include "AppServoTest.h"
#include <Arduino.h>

AppServoTest::AppServoTest(SystemContext* ctx, ModbusMaster* rs485, NodeManager* nodeMgr)
    : _ctx(ctx), _rs485(rs485), _nodeMgr(nodeMgr), _sequencer(ctx, rs485, nodeMgr) {}

void AppServoTest::onEnter() {
    Serial.println("[AppServoTest] Entering Maintenance Mode...");
    // 清理界面同步状态, 并标记所有离线节点为-1避免界面误导
    for(int i=1; i<=20; i++) {
        _ctx->ui.servoRealStates[i] = _nodeMgr->isOnline(i) ? 0 : -1;
    }
    _ctx->prog.dirtyFlags |= DF_NODE_DATA; // 触发UI刷新
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

    Serial.printf("[AppServoTest] Single Request Node %d -> %s\n", id, open ? "OPEN" : "CLOSE");
    _rs485->asyncWrite(id, 0x011F, open ? 0x01 : 0x00, 
        [this, id, open](Modbus::ResultCode res, uint16_t tid, void* data) {
            if (res == Modbus::EX_SUCCESS) {
                this->_ctx->ui.servoRealStates[id] = open ? 1 : 0;
            } else {
                Serial.printf("[AppServoTest] Single ERR Node %d: Result %d\n", id, (int)res);
                this->_ctx->ui.servoRealStates[id] = -1; // 报红
            }
            this->_ctx->prog.dirtyFlags |= DF_NODE_DATA; // 唤醒UI重绘舵机颜色
            return true;
        }
    );

}

void AppServoTest::triggerGlobalServo(bool open) {
    // 动作码：1=开启, 2=关闭
    _sequencer.start(REG_CMD_CONTROL, open ? CMD_SERVO_OPEN : CMD_SERVO_CLOSE, 150, open ? 1 : 2);
    
    const char* taskName = open ? "正在开启舵机..." : "正在关闭舵机...";
    // 新的提示文字更短（"正在开启舵机..." UTF-8 约 21 字节），
    // 已经可以安全地放在 32 字节的 statusText 中，不再有越界未自动补零的隐患。
    strncpy(_ctx->prog.statusText, taskName, 32);
    _ctx->prog.dirtyFlags |= DF_SYS_STATUS;
}

