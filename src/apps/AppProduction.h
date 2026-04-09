#ifndef APP_PRODUCTION_H
#define APP_PRODUCTION_H

#include "apps/IApp.h"
#include "system/SystemContext.h"
#include <vector>
#include <Preferences.h>

class PollManager;
class ModbusMaster;
class CombinationEngine;
class Belt;

/**
 * @class AppProduction
 * @brief 生产称重应用。
 * 负责核心组合秤逻辑、下料同步、输送带控制以及生产参数持久化。
 */
class AppProduction : public IApp {
public:
    AppProduction(SystemContext* ctx, PollManager* pollMgr, ModbusMaster* rs485,
                  CombinationEngine* engine, Belt* b1, Belt* b2,
                  SemaphoreHandle_t mutex);

    // IApp 接口实现
    void onEnter() override;
    void onLoop() override;
    void onExit() override;
    OperationMode getMode() const override { return MODE_PRODUCTION; }

    // 生产参数管理 (从 AppController 迁移)
    void updateTargets(float dMin, float dMax);

private:
    SystemContext*      _ctx;
    PollManager*        _pollMgr;
    ModbusMaster*       _rs485;
    CombinationEngine*  _engine;
    Belt*               _b1;
    Belt*               _b2;
    SemaphoreHandle_t   _mutex;

    Preferences         _nvs;
    unsigned long       _lastCalcTime = 0;
    uint8_t             _currentPollId = 1;
    
    // 异步状态机变量
    unsigned long       _stateStartTime = 0;
    std::vector<int>    _selectedIds;
    int                 _dischargeIndex = 0;
    float               _lastCombinedWeight = 0;

    void handlePolling();
    void handleReadyState(unsigned long now);
    void handleDropState(unsigned long now);
    void handleCloseState(unsigned long now);
    void handleSettleState(unsigned long now);
    void handleBeltAState(unsigned long now);
    void handleBeltBState(unsigned long now);

    void updateUIState(SystemStatus status, uint32_t mask = 0, float weight = 0.0f);
    void loadParams();
    void saveParams();
};

#endif // APP_PRODUCTION_H
