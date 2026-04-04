#ifndef APP_SEQUENTIAL_CTRL_H
#define APP_SEQUENTIAL_CTRL_H

#include "apps/IApp.h"
#include "system/SystemContext.h"
#include <Arduino.h>
#include "drivers/ModbusMaster.h"

class PollManager;

/**
 * @class AppSequentialCtrl
 * @brief 序列化控制应用（如全局置零）。
 * 负责执行需要多步、独动总线的复杂指令序列。
 */
class AppSequentialCtrl : public IApp {
public:
    AppSequentialCtrl(SystemContext* ctx, ModbusMaster* rs485, PollManager* pollMgr, SemaphoreHandle_t mutex)
        : _ctx(ctx), _rs485(rs485), _pollMgr(pollMgr), _mutex(mutex) {}

    void onEnter() override {
        Serial.println("[AppSequentialCtrl] Sequential Control Mode Entered.");
        _startTime = millis();
        _isFinished = false;
        
        if (_pendingAction == ACT_GLOBAL_TARE) {
            _step = 0;
            _progress = 0;
            _state = STATE_SENDING;
            Serial.println("[AppSequentialCtrl] Starting Global Tare Sequence...");
        }
    }

    void onLoop() override {
        if (_isFinished) return;

        unsigned long now = millis();
        if (_pendingAction == ACT_GLOBAL_TARE) {
            handleGlobalTareStateMachine(now);
        }
    }

    void onExit() override {
        Serial.println("[AppSequentialCtrl] Sequential Control Mode Exited.");
        _pendingAction = ACT_NONE;
    }

    OperationMode getMode() const override { return MODE_SEQUENTIAL_CTRL; }
    bool isFinished() const override { return _isFinished; }
    bool hasUIProgress() override { return !_isFinished; }
    int getUIProgress() override { return _progress; }

    void triggerGlobalTare() {
        _pendingAction = ACT_GLOBAL_TARE;
    }

private:
    enum Action { ACT_NONE, ACT_GLOBAL_TARE };
    enum State  { STATE_IDLE, STATE_SENDING, STATE_WAITING, STATE_NEXT, STATE_FINISH };
    
    SystemContext*    _ctx;
    ModbusMaster*     _rs485;
    PollManager*      _pollMgr;
    SemaphoreHandle_t _mutex;

    Action             _pendingAction = ACT_NONE;
    State              _state = STATE_IDLE;
    Modbus::ResultCode _lastResult = Modbus::EX_SUCCESS;
    int                _step = 0;
    int                _progress = 0;
    unsigned long      _startTime = 0;
    bool               _isFinished = false;

    void handleGlobalTareStateMachine(unsigned long now) {
        switch (_state) {
            case STATE_SENDING: {
                int nodeId = _step + 1;
                if (nodeId <= 20) {
                    if (_pollMgr->isWhitelisted(nodeId)) {
                        Serial.printf("[AppSequentialCtrl] Taring Node %d...\n", nodeId);
                        
                        // 使用 asyncWrite 代替阻塞的 syncWrite
                        bool sent = _rs485->asyncWrite(nodeId, 0x0100, 3, 
                            [this](Modbus::ResultCode res, uint16_t tid, void* data) {
                                this->_lastResult = res;
                                this->_state = STATE_NEXT;
                                return true;
                            }
                        );

                        if (sent) {
                            _state = STATE_WAITING;
                        } else {
                            // 总线忙（正在处理其他请求），本轮 Loop 略过，等待下一次循环重试
                        }
                    } else {
                        // 跳过非白名单节点，进度继续增加
                        _step++;
                        _progress = (_step * 100) / 20;
                    }
                } else {
                    _state = STATE_FINISH;
                }
                break;
            }

            case STATE_WAITING:
                // 底层驱动有自己的超时机制（2秒），此处无需额外超时
                // 回调函数接收到响应后，会将状态改为 STATE_NEXT
                break;

            case STATE_NEXT:
                if (_lastResult != Modbus::EX_SUCCESS) {
                    Serial.printf("[AppSequentialCtrl] Node %d Tare Failed: 0x%02X (Skipping)\n", _step + 1, _lastResult);
                }
                _step++;
                _progress = (_step * 100) / 20;
                _state = STATE_SENDING;
                break;

            case STATE_FINISH:
                _isFinished = true;
                _progress = 100;
                _state = STATE_IDLE;
                Serial.println("[AppSequentialCtrl] Global Tare Sequence Completed.");
                break;
            
            default:
                break;
        }
    }
};

#endif // APP_SEQUENTIAL_CTRL_H
