#ifndef STEP_SEQUENCER_H
#define STEP_SEQUENCER_H

#include <Arduino.h>
#include <functional>
#include "drivers/ModbusMaster.h"
#include "logic/NodeManager.h"
#include "system/SystemContext.h"

/**
 * @class StepSequencer
 * @brief 节点驱动型异步序列执行器。
 * 
 * 专门处理针对 1~20 号白名单节点的逐个 Modbus 写入任务。
 * 允许 App 在不切换全局模式的情况下执行批量动作。
 */
class StepSequencer {
public:
    enum State { IDLE, SENDING, WAITING, DELAY, FINISH };

    StepSequencer(SystemContext* ctx, ModbusMaster* rs485, NodeManager* nodeMgr)
        : _ctx(ctx), _rs485(rs485), _nodeMgr(nodeMgr) {}

    /**
     * @brief 启动一个新序列
     */
    void start(uint16_t reg, uint16_t val, uint32_t delayMs, int uiActionCode = 0) {
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
        _ctx->ui.isTareRunning = true; // 保持复用原有进度条字段
        _ctx->ui.tareProgress = 0;
    }

    /**
     * @brief 在 App 的 onLoop 中持续调用
     */
    void update(unsigned long now) {
        if (!_isBusy) return;

        switch (_state) {
            case SENDING: {
                int nodeId = _step + 1;
                if (nodeId <= 20) {
                    if (_nodeMgr->isWhitelisted(nodeId)) {
                        bool sent = _rs485->asyncWrite(nodeId, _regAddr, _regVal, 
                            [this](Modbus::ResultCode res, uint16_t tid, void* data) {
                                this->_state = DELAY;
                                this->_lastStepTime = millis();
                                return true;
                            }
                        );

                        if (sent) {
                            _state = WAITING;
                            _ctx->ui.activeSeqNode = nodeId;
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
                // 等待 Modbus 回调切换至 DELAY
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

    void stop() {
        _isBusy = false;
        _state = IDLE;
        _ctx->ui.activeSeqNode = 0;
        _ctx->ui.activeSeqAction = 0;
        _ctx->ui.isTareRunning = false;
        _ctx->ui.tareProgress = 100;
    }

    bool isBusy() const { return _isBusy; }
    int getProgress() const { return _progress; }

private:
    void nextStep() {
        _step++;
        _progress = (_step * 100) / 20;
        _ctx->ui.tareProgress = _progress;
        _ctx->prog.dirtyFlags |= DF_PROGRESS;
        _state = SENDING;
    }

    SystemContext* _ctx;
    ModbusMaster*  _rs485;
    NodeManager*   _nodeMgr;

    uint16_t _regAddr;
    uint16_t _regVal;
    uint32_t _delayBetweenSteps;
    int      _uiActionCode;

    State         _state = IDLE;
    int           _step = 0;
    int           _progress = 0;
    unsigned long _lastStepTime = 0;
    bool          _isBusy = false;
};

#endif
