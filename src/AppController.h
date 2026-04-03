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

// 前向声明，避免头文件间循环依赖
class ModbusMaster;
class PollManager;
class CombinationEngine;

/**
 * @class AppController
 * @brief 应用层总协调器。
 * 实现 ICommandBus 接口，接收来自 UI 的指令。
 * 持有所有业务状态、FreeRTOS 双核任务和生产调度逻辑。
 */
class AppController : public ICommandBus {
public:
    AppController(ModbusMaster* rs485, PollManager* pollMgr,
                  CombinationEngine* engine, ConveyorController* conveyor,
                  UIManager* ui);

    /**
     * @brief 初始化同步原语、注入 UI 依赖、启动双核任务。
     */
    void begin();

    // --- ICommandBus 接口实现 ---
    void cmdGlobalTare() override;
    void cmdStartScan() override;
    void cmdClearAccumulated() override;
    void cmdUpdateTargets(float dMin, float dMax) override;
    void cmdToggleDiagnosis(bool active) override;
    void updateOperationMode(OperationMode newMode) override;

private:
    // --- 管理器指针 ---
    ModbusMaster*       _rs485;
    PollManager*        _pollMgr;
    CombinationEngine*  _engine;
    ConveyorController* _conveyor;
    UIManager*          _ui;

    // --- 业务状态 ---
    SystemContext      _ctx;
    std::vector<float> _slaveWeights;
    std::vector<bool>  _slaveStable;
    SystemStatus       _systemStatus        = SYS_INIT;
    float              _lastCombinedWeight  = 0.0f;
    uint32_t           _currentSelectedMask = 0;
    float              _accumulatedWeight   = 0.0f;
    bool               _isProductionActive  = true;
    Preferences        _nvs;

    // --- FreeRTOS 同步原语 ---
    SemaphoreHandle_t _mutexParams  = nullptr;
    SemaphoreHandle_t _mutexWeights = nullptr;
    SemaphoreHandle_t _mutexStatus  = nullptr;

    // --- 辅助与任务 ---
    static const char* modeToStr(OperationMode m);
    static void controlTaskEntry(void* self);
    static void uiTaskEntry(void* self);
    void controlLoop();
    void uiLoop();
};

#endif // APP_CONTROLLER_H
