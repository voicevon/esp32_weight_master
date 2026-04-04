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
            Serial.println("[AppSequentialCtrl] Starting Global Tare Sequence...");
        }
    }

    void onLoop() override {
        if (_isFinished) return;

        unsigned long now = millis();
        if (_pendingAction == ACT_GLOBAL_TARE) {
            handleGlobalTare(now);
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
    
    SystemContext*    _ctx;
    ModbusMaster*     _rs485;
    PollManager*      _pollMgr;
    SemaphoreHandle_t _mutex;

    Action        _pendingAction = ACT_NONE;
    int           _step = 0;
    int           _progress = 0;
    unsigned long _startTime = 0;
    unsigned long _lastStepTime = 0;
    bool          _isFinished = false;

    void handleGlobalTare(unsigned long now) {
        // 每 100ms 执行一个节点的置零，确保总线不冲突
        if (now - _lastStepTime < 100) return;
        _lastStepTime = now;

        int nodeId = _step + 1;
        if (nodeId <= 20) {
            if (_pollMgr->isWhitelisted(nodeId)) {
                _rs485->syncWrite(nodeId, 0x0100, 3); // 3 = Tare command
            }
            _step++;
            _progress = (_step * 100) / 20;
        } else {
            _isFinished = true;
            _progress = 100;
            Serial.println("[AppSequentialCtrl] Global Tare Sequence Completed.");
        }
    }
};

#endif // APP_SEQUENTIAL_CTRL_H
