#ifndef APP_PRODUCTION_H
#define APP_PRODUCTION_H

#include "apps/IApp.h"
#include "system/SystemContext.h"
#include <vector>
#include <Preferences.h>

class PollManager;
class ModbusMaster;
class CombinationEngine;
class BeltManager;

/**
 * @class AppProduction
 * @brief 生产称重应用。
 * 负责核心组合秤逻辑、下料同步、输送带控制以及生产参数持久化。
 */
class AppProduction : public IApp {
public:
    AppProduction(SystemContext* ctx, PollManager* pollMgr, ModbusMaster* rs485,
                  CombinationEngine* engine, BeltManager* conveyor,
                  SemaphoreHandle_t mutex);

    // IApp 接口实现
    void onEnter() override;
    void onLoop() override;
    void onExit() override;
    OperationMode getMode() const override { return MODE_PRODUCTION; }

    // 生产参数管理 (从 AppController 迁移)
    void updateTargets(float dMin, float dMax);
    void clearAccumulated();

private:
    SystemContext*      _ctx;
    PollManager*        _pollMgr;
    ModbusMaster*       _rs485;
    CombinationEngine*  _engine;
    BeltManager*        _conveyor;
    SemaphoreHandle_t   _mutex;

    Preferences         _nvs;
    unsigned long       _lastCalcTime = 0;
    uint8_t             _currentPollId = 1;

    void handlePolling();
    void loadParams();
    void saveParams();
};

#endif // APP_PRODUCTION_H
