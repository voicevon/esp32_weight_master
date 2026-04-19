#include "apps/AppShift.h"
#include <Arduino.h>
#include "system/SystemConfig.h"
#include "drivers/ModbusMaster.h"
#include "logic/NodeManager.h"
#include "drivers/Belt.h"

AppShift::AppShift(SystemContext* ctx, ModbusMaster* rs485, NodeManager* nodeMgr, Belt* b1, Belt* b2)
    : _ctx(ctx), _rs485(rs485), _nodeMgr(nodeMgr), _b1(b1), _b2(b2), _sequencer(ctx, rs485, nodeMgr)
{
}

void AppShift::onEnter() {
    Serial.println("[AppShift] Entered Shift Management Mode.");
    _isFinished = false;
    _cancelRequested = false;
    _state = IDLE;
    _isBeltRunning = false;
}

void AppShift::onLoop() {
    unsigned long now = millis();
    _sequencer.update(now);
    handleState(now);
}

void AppShift::onExit() {
    Serial.println("[AppShift] Exited Shift Management Mode.");
    _sequencer.stop();
}

int AppShift::getUIProgress() {
    if (_sequencer.isBusy()) return _sequencer.getProgress();
    if (_state == BELT1_MOVE) return 50; // 占位
    if (_state == BELT2_RUN) return 75;
    return 0;
}

void AppShift::triggerStartShift() {
    if (_state != IDLE) return;
    Serial.println("[AppShift] Starting Shift Sequence...");
    _isEndShift = false;
    _state = SV_OPEN_SWEEP;
    strncpy(_ctx->prog.statusText, "正在执行上班流程...", 32);
    _ctx->prog.dirtyFlags |= DF_SYS_STATUS;
    _sequencer.start(REG_CMD_CONTROL, CMD_SERVO_OPEN, 500, 1); // 0.5s 间隔
}

void AppShift::triggerEndShift() {
    if (_state != IDLE) return;
    Serial.println("[AppShift] Ending Shift Sequence...");
    _isEndShift = true;
    _state = SV_OPEN_SWEEP;
    strncpy(_ctx->prog.statusText, "请不要关机，正在下班...", 32);
    _ctx->prog.dirtyFlags |= DF_SYS_STATUS;
    _sequencer.start(REG_CMD_CONTROL, CMD_SERVO_OPEN, 500, 1);
}

void AppShift::handleState(unsigned long now) {
    if (_state == IDLE) return;

    switch (_state) {
        case SV_OPEN_SWEEP:
            if (!_sequencer.isBusy()) {
                Serial.println("[AppShift] Servo Open Sweep Finished. Starting Close Sweep...");
                _state = SV_CLOSE_SWEEP;
                _sequencer.start(REG_CMD_CONTROL, CMD_SERVO_CLOSE, 500, 2); 
            }
            break;

        case SV_CLOSE_SWEEP:
            if (!_sequencer.isBusy()) {
                Serial.println("[AppShift] Servo Close Sweep Finished. Moving Belt 1...");
                _state = BELT1_MOVE;
                _b1->moveDistanceMm(2000);
                _stateStartTime = now;
                _isBeltRunning = true;
            }
            break;

        case BELT1_MOVE:
            // 等待 Belt 1 完成 (根据配置时间 8s)
            if (now - _stateStartTime >= BELT_COLLECT_PERIOD_MS) {
                Serial.println("[AppShift] Belt 1 Move Finished. Running Belt 2...");
                _state = BELT2_RUN;
                _b2->speedRun(true);
                _stateStartTime = now;
            }
            break;

        case BELT2_RUN:
            // 运行 15 秒
            if (now - _stateStartTime >= 15000) {
                _b2->speedStop();
                Serial.println("[AppShift] Belt 2 Run Finished. Sequence Complete.");
                
                if (_isEndShift) {
                    // 下班逻辑：清零单班次重量
                    _ctx->config.shiftWeight = 0;
                    _ctx->config.accumulatedWeight = 0; // 单次工作也清零
                    _ctx->prog.dirtyFlags |= DF_CONFIG;
                    
                    // 持久化
                    _nvs.begin("production", false);
                    _nvs.putFloat("shift", 0.0f);
                    _nvs.putFloat("accu", 0.0f);
                    _nvs.end();
                    Serial.println("[AppShift] Data Reset for End of Shift.");
                    strncpy(_ctx->prog.statusText, "下班完成，可以关闭电源", 32);
                } else {
                    strncpy(_ctx->prog.statusText, "上班自检完成", 32);
                }
                _ctx->prog.dirtyFlags |= DF_SYS_STATUS;

                _state = DONE;
                _isFinished = true;
                _isBeltRunning = false;
            }
            break;

        default:
            break;
    }
}
