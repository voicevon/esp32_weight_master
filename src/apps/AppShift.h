#ifndef APP_SHIFT_H
#define APP_SHIFT_H

#include "apps/IApp.h"
#include "system/SystemContext.h"
#include "logic/StepSequencer.h"
#include <Preferences.h>

class ModbusMaster;
class NodeManager;
class Belt;

/**
 * @class AppShift
 * @brief 上下班管理应用。
 * 负责一键上班/下班的硬件序列执行及报表显示。
 */
class AppShift : public IApp {
public:
    AppShift(SystemContext* ctx, ModbusMaster* rs485, NodeManager* nodeMgr, Belt* b1, Belt* b2);

    void onEnter() override;
    void onLoop() override;
    void onExit() override;
    void requestCancel() override { _cancelRequested = true; }

    OperationMode getMode() const override { return MODE_SHIFT_MANAGEMENT; }
    bool isFinished() const override { return _isFinished; }
    
    // UI 支持
    bool hasUIProgress() override { return _sequencer.isBusy() || _isBeltRunning; }
    int getUIProgress() override;

    // 业务触发
    void triggerStartShift();
    void triggerEndShift();

private:
    SystemContext* _ctx;
    ModbusMaster*  _rs485;
    NodeManager*   _nodeMgr;
    Belt*          _b1;
    Belt*          _b2;

    StepSequencer _sequencer;
    Preferences   _nvs;

    enum ShiftState {
        IDLE,
        SV_OPEN_SWEEP,
        SV_CLOSE_SWEEP,
        BELT1_MOVE,
        BELT2_RUN,
        DONE
    };

    ShiftState _state = IDLE;
    unsigned long _stateStartTime = 0;
    bool _isFinished = false;
    bool _cancelRequested = false;
    bool _isBeltRunning = false;
    bool _isEndShift = false; // 标记是否为下班模式（完成后需清零）

    void nextState();
    void handleState(unsigned long now);
};

#endif // APP_SHIFT_H
