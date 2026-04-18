#include "apps/AppSequentialCtrl.h"
#include "logic/NodeManager.h"
#include "system/SystemConfig.h"
#include <Arduino.h>

void AppSequentialCtrl::onEnter() {
    Serial.println("[AppSequentialCtrl] Sequential Control Mode Entered.");
    _isFinished = false;
    _step = 0;
    _progress = 0;
    _state = STATE_SENDING;

    // 根据 Action 载入轻量级配置 (Data-Driven mapping)
    switch (_pendingAction) {
        case ACT_GLOBAL_TARE:
            _curConfig = { REG_CMD_CONTROL, CMD_TARE, 0, "TARE" };
            break;
        case ACT_GLOBAL_OPEN:
            _curConfig = { REG_CMD_CONTROL, CMD_SERVO_OPEN, 150, "SERVO_OPEN" };
            break;
        case ACT_GLOBAL_CLOSE:
            _curConfig = { REG_CMD_CONTROL, CMD_SERVO_CLOSE, 150, "SERVO_CLOSE" };
            break;
        default:
            Serial.println("[AppSequentialCtrl] Unknown Action, finishing immediately.");
            _isFinished = true;
            return;
    }

    Serial.printf("[AppSequentialCtrl] ACTION: %s (Reg: 0x%04X, Val: %d, Delay: %dms)\n", 
                  _curConfig.label, _curConfig.regAddr, _curConfig.regVal, _curConfig.delayMs);
}

void AppSequentialCtrl::onLoop() {
    if (_isFinished) return;
    handleSequenceStateMachine(millis());
}

void AppSequentialCtrl::onExit() {
    Serial.println("[AppSequentialCtrl] Sequential Control Mode Exited.");
    _pendingAction = ACT_NONE;
    _state = STATE_IDLE;
}

void AppSequentialCtrl::triggerGlobalTare() {
    _pendingAction = ACT_GLOBAL_TARE;
}

void AppSequentialCtrl::triggerGlobalServo(bool open) {
    _pendingAction = open ? ACT_GLOBAL_OPEN : ACT_GLOBAL_CLOSE;
}

/**
 * @brief 通用异步序列状态机 (彻底移除具体业务逻辑，完全由数据驱动)
 */
void AppSequentialCtrl::handleSequenceStateMachine(unsigned long now) {
    switch (_state) {
        case STATE_SENDING: {
            int nodeId = _step + 1;
            if (nodeId <= 20) {
                if (_nodeMgr->isWhitelisted(nodeId)) {
                    // 彻底移除 switch(_pendingAction)，统一读取 _curConfig
                    bool sent = _rs485->asyncWrite(nodeId, _curConfig.regAddr, _curConfig.regVal, 
                        [this](Modbus::ResultCode res, uint16_t tid, void* data) {
                            this->_lastResult = res;
                            this->_state = STATE_NEXT;
                            return true;
                        }
                    );

                    if (sent) {
                        _state = STATE_WAITING;
                    }
                } else {
                    // 跳过非白名单节点
                    _step++;
                    _progress = (_step * 100) / 20;
                }
            } else {
                _state = STATE_FINISH;
            }
            break;
        }

        case STATE_WAITING:
            break;

        case STATE_NEXT:
            if (_curConfig.delayMs > 0) {
                _lastStepTime = now;
                _state = STATE_DELAY;
            } else {
                _step++;
                _progress = (_step * 100) / 20;
                _state = STATE_SENDING;
            }
            break;

        case STATE_DELAY:
            if (now - _lastStepTime >= _curConfig.delayMs) {
                _step++;
                _progress = (_step * 100) / 20;
                _state = STATE_SENDING;
            }
            break;

        case STATE_FINISH:
            _isFinished = true;
            _progress = 100;
            _state = STATE_IDLE;
            Serial.printf("[AppSequentialCtrl] Sequence %s Completed.\n", _curConfig.label);
            break;
        
        default:
            break;
    }
}
