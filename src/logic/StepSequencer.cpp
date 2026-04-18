#include "logic/StepSequencer.h"
#include <Arduino.h>

StepSequencer::StepSequencer(SystemContext* ctx, ModbusMaster* rs485, NodeManager* nodeMgr)
    : _ctx(ctx), _rs485(rs485), _nodeMgr(nodeMgr) {}

void StepSequencer::start(uint16_t reg, uint16_t val, uint32_t delayMs, int uiActionCode) {
    _regAddr = reg;
    _regVal = val;
    _delayBetweenSteps = delayMs;
    _uiActionCode = uiActionCode;
    
    _step = 0;
    _progress = 0;
    _state = SENDING;
    _isBusy = true;
    
    // 更新 UI 状态
    _ctx->ui.activeSeqAction = _uiActionCode;
    _ctx->ui.isTareRunning = true; 
    _ctx->ui.tareProgress = 0;
}

void StepSequencer::update(unsigned long now) {
    if (!_isBusy) return;

    switch (_state) {
        case SENDING: {
            int nodeId = _step + 1;
            if (nodeId <= 20) {
                if (_nodeMgr->isWhitelisted(nodeId)) {
                    Serial.printf("[StepSequencer] SENDING Node %d: Reg 0x%04X, Val %d\n", nodeId, _regAddr, _regVal);
                    bool sent = _rs485->asyncWrite(nodeId, _regAddr, _regVal, 
                        [this, nodeId](Modbus::ResultCode res, uint16_t tid, void* data) {
                            if (res == Modbus::EX_SUCCESS) {
                                _ctx->ui.servoRealStates[nodeId] = (this->_regVal == CMD_SERVO_OPEN) ? 1 : 0;
                            } else {
                                Serial.printf("[StepSequencer] ERROR Node %d: Result %d\n", nodeId, (int)res);
                                _ctx->ui.servoRealStates[nodeId] = -1;
                            }
                            this->_ctx->prog.dirtyFlags |= DF_NODE_DATA; // 唤醒UI重绘
                            this->_state = DELAY;
                            this->_lastStepTime = millis();
                            return true;
                        }
                    );

                    if (sent) {
                        _state = WAITING;
                        _ctx->ui.activeSeqNode = nodeId;
                    } else {
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }
                } else {
                    nextStep();
                }
            } else {
                _state = FINISH;
            }
            break;
        }

        case WAITING:
            break;

        case DELAY:
            if (now - _lastStepTime >= _delayBetweenSteps) {
                nextStep();
            }
            break;

        case FINISH:
            stop();
            break;
        
        default: break;
    }
}

void StepSequencer::stop() {
    _isBusy = false;
    _state = IDLE;
    _ctx->ui.activeSeqNode = 0;
    _ctx->ui.activeSeqAction = 0;
    _ctx->ui.isTareRunning = false;
    _ctx->ui.tareProgress = 100;
}

void StepSequencer::nextStep() {
    _step++;
    _progress = (_step * 100) / 20;
    _ctx->ui.tareProgress = _progress;
    _ctx->prog.dirtyFlags |= DF_PROGRESS;
    _state = SENDING;
}
