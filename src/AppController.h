#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

#include <Arduino.h>
#include <vector>
#include <Preferences.h>
#include "system/SystemContext.h"
#include "system/SystemTypes.h"
#include "logic/ConveyorController.h"
#include "UIManager.h"
#include "I_Command_Bus.h"

// 前向声明
class ModbusMaster;
class PollManager;
class CombinationEngine;

/**
 * @class AppController
 * @brief 应用层总协调器。
 * Phase 4: 优化锁粒度，消除 redundant 拷贝。
 */
class AppController : public ICommandBus {
public:
    AppController(ModbusMaster* rs485, PollManager* pollMgr,
                  CombinationEngine* engine, ConveyorController* conveyor,
                  UIManager* ui);

    void begin();

    // --- ICommandBus 实现 ---
    void cmdGlobalTare() override;
    void cmdStartScan() override;
    void cmdClearAccumulated() override;
    void cmdUpdateTargets(float dMin, float dMax) override;
    void cmdToggleDiagnosis(bool active) override;
    void cmdServoTest(int id, bool open) override;
    void updateOperationMode(OperationMode newMode) override;

private:
    // --- 管理器指针 ---
    ModbusMaster*       _rs485;
    PollManager*        _pollMgr;
    CombinationEngine*  _engine;
    ConveyorController* _conveyor;
    UIManager*          _ui;

    // --- 业务状态 (Phase 4: 整合上下文) ---
    SystemContext      _ctx;
    float              _accumulatedWeight   = 0.0f;
    bool               _isProductionActive  = true;
    
    // 全局置零进度跟踪
    bool               _isTareRunning       = false;
    int                _tareProgress        = 0;
    
    Preferences        _nvs;

    // --- FreeRTOS 同步原语 (Phase 4: 优化锁) ---
    SemaphoreHandle_t _mutexProduction = nullptr; // 保护 config 与 prog
    SemaphoreHandle_t _mutexDiag       = nullptr; // 保护 diag 日志

    // --- 内部辅助与任务 ---
    static const char* modeToStr(OperationMode m);
    static void controlTaskEntry(void* self);
    static void uiTaskEntry(void* self);
    void controlLoop();
    void uiLoop();
};

#endif // APP_CONTROLLER_H
