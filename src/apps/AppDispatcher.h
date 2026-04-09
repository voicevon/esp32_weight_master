#ifndef APP_DISPATCHER_H
#define APP_DISPATCHER_H

#include <Arduino.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "system/SystemContext.h"
#include "system/SystemTypes.h"
#include "apps/IApp.h"
#include "ui/UIManager.h"

class ModbusMaster;
class BeltManager;
class PollManager;

/**
 * @class AppDispatcher
 * @brief 系统调度器（核心内核）。
 * 负责管理所有 App 实例、FreeRTOS 任务、UI 同步以及原子模式切换。
 */
class AppDispatcher : public ICommandBus {
public:
    AppDispatcher(SystemContext* ctx, ModbusMaster* rs485, PollManager* pollMgr, UIManager* ui, BeltManager* conveyor);

    void registerApp(IApp* app);
    void begin(OperationMode initialMode = MODE_PRODUCTION);

    // ICommandBus 接口实现 (UI 交互桥梁)
    void cmdGlobalTare() override;
    void cmdStartScan() override;
    void cmdToggleDiagnosis(bool active) override;
    void cmdServoTest(int id, bool open) override;
    void cmdGlobalServo(bool open) override;
    void cmdBeltTest(int beltId, int distanceMm) override;
    void cmdTriggerBeltScan() override;
    void cmdUpdateTargets(float dMin, float dMax) override;
    
    void cmdSerialSendHex(const char* hexStr) override;
    void cmdSerialToggleAuto(bool enable) override;
    void cmdSetDiagSubMode(int mode) override;
    void cmdSetDiagTarget(int id) override;
    void cmdDiagAction(int actionId) override;

    // 模式控制
    void updateOperationMode(OperationMode newMode) override;

private:
    // 资源
    SystemContext*        _ctx;
    ModbusMaster*         _rs485;
    PollManager*          _pollMgr;
    UIManager*            _ui;
    BeltManager*          _conveyor;

    // App 管理
    std::vector<IApp*>    _apps;
    IApp*                 _currentApp = nullptr;
    OperationMode         _currentMode = MODE_IDLE;
    OperationMode         _pendingMode = MODE_IDLE;

    // 任务同步
    SemaphoreHandle_t     _mutexCtx;
    void controlLoop();
    void uiLoop();
    static void controlTaskEntry(void* self);
    static void uiTaskEntry(void* self);

    // 辅助
    bool canSwitchMode() const;
    void executeModeSwitch();
    IApp* findApp(OperationMode mode);
    const char* modeToStr(OperationMode m);
};

#endif // APP_DISPATCHER_H
