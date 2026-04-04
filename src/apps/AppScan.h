#ifndef APP_SCAN_H
#define APP_SCAN_H

#include "apps/IApp.h"
#include "system/SystemContext.h"
#include "logic/PollManager.h"
#include <Arduino.h>

/**
 * @class AppScan
 * @brief 节点扫描应用。
 * 负责在诊断模式下遍历所有节点并更新白名单。
 * 业务逻辑已完全从 PollManager 迁移至此。
 */
class AppScan : public IApp {
public:
    AppScan(SystemContext* ctx, PollManager* pollMgr, ModbusMaster* rs485, SemaphoreHandle_t mutex)
        : _ctx(ctx), _pollMgr(pollMgr), _rs485(rs485), _mutex(mutex) {}

    void onEnter() override;
    void onLoop() override;
    void onExit() override;

    OperationMode getMode() const override { return MODE_DIAG_SCAN; }
    bool isFinished() const override { return _isFinished; }
    
    // UI 支持
    bool hasUIProgress() override { return true; }
    int getUIProgress() override { return (_scanCycle * 20 + _scanProgress) * 100 / 100; }
    
    int getScanProgress() const { return _scanProgress; }
    int getScanCycle() const { return _scanCycle; }
    bool getScanResult(int cycle, int id) const { 
        return (cycle >= 0 && cycle < 5 && id >= 1 && id <= 20) ? _scanHistory[cycle][id] : false; 
    }

private:
    SystemContext*    _ctx;
    PollManager*      _pollMgr;
    ModbusMaster*     _rs485;
    SemaphoreHandle_t _mutex;

    int  _scanProgress = 1;
    int  _scanCycle = 0;
    bool _scanHistory[5][21]; 
    bool _isFinished = false;
    unsigned long _lastRequestTime = 0;

    void handleScanStep();
};

#endif // APP_SCAN_H
