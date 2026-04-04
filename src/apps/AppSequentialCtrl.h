#ifndef APP_SEQUENTIAL_CTRL_H
#define APP_SEQUENTIAL_CTRL_H

#include "apps/IApp.h"
#include "system/SystemContext.h"
#include <Arduino.h>
#include "drivers/ModbusMaster.h"

class PollManager;

/**
 * @class AppSequentialCtrl
 * @brief 序列化控制应用 (通用异步序列处理器)。
 * 负责执行需要多步、独占总线且可能需要步进延时的复杂指令序列。
 */
class AppSequentialCtrl : public IApp {
public:
    AppSequentialCtrl(SystemContext* ctx, ModbusMaster* rs485, PollManager* pollMgr, SemaphoreHandle_t mutex)
        : _ctx(ctx), _rs485(rs485), _pollMgr(pollMgr), _mutex(mutex) {}

    void onEnter() override;
    void onLoop() override;
    void onExit() override;

    OperationMode getMode() const override { return MODE_SEQUENTIAL_CTRL; }
    bool isFinished() const override;
    bool hasUIProgress() override;
    int getUIProgress() override;

    void triggerGlobalTare();
    void triggerGlobalServo(bool open);

private:
    struct SequenceConfig {
        uint16_t regAddr;
        uint16_t regVal;
        uint32_t delayMs;
        const char* label;
    };

    enum Action { ACT_NONE, ACT_GLOBAL_TARE, ACT_GLOBAL_OPEN, ACT_GLOBAL_CLOSE };
    enum State  { STATE_IDLE, STATE_SENDING, STATE_WAITING, STATE_NEXT, STATE_DELAY, STATE_FINISH };
    
    SystemContext*    _ctx;
    ModbusMaster*     _rs485;
    PollManager*      _pollMgr;
    SemaphoreHandle_t _mutex;

    Action             _pendingAction = ACT_NONE;
    SequenceConfig     _curConfig = {0, 0, 0, ""};
    State              _state = STATE_IDLE;
    Modbus::ResultCode _lastResult = Modbus::EX_SUCCESS;
    int                _step = 0;
    int                _progress = 0;
    unsigned long      _lastStepTime = 0;
    uint32_t           _stepDelayMs = 0;
    bool               _isFinished = false;

    void handleSequenceStateMachine(unsigned long now);
};

inline bool AppSequentialCtrl::isFinished() const { return _isFinished; }
inline bool AppSequentialCtrl::hasUIProgress() { return !_isFinished; }
inline int AppSequentialCtrl::getUIProgress() { return _progress; }

#endif // APP_SEQUENTIAL_CTRL_H
