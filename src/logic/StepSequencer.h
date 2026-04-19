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

    StepSequencer(SystemContext* ctx, ModbusMaster* rs485, NodeManager* nodeMgr);

    /**
     * @brief 启动一个新序列
     */
    void start(uint16_t reg, uint16_t val, uint32_t delayMs, int uiActionCode = 0);

    /**
     * @brief 在 App 的 onLoop 中持续调用
     */
    void update(unsigned long now);

    void stop();

    bool isBusy() const { return _isBusy; }
    int getProgress() const { return _progress; }

private:
    void nextStep();

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
