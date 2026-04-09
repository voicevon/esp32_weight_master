#ifndef APP_BELT_DIAG_H
#define APP_BELT_DIAG_H

#include "apps/IApp.h"
#include "system/SystemContext.h"
#include "drivers/ModbusMaster.h"
#include "logic/Belt.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

enum BeltDiagState {
    DIAG_IDLE,
    DIAG_SCANNING_BELT1,
    DIAG_SCANNING_BELT2,
    DIAG_RUNNING
};

class AppBeltDiag : public IApp {
public:
    AppBeltDiag(SystemContext* ctx, ModbusMaster* rs485, Belt* b1, Belt* b2, SemaphoreHandle_t mutexCtx);

    OperationMode getMode() const override { return MODE_BELT_DIAG; }
    void onEnter() override;
    void onLoop() override;
    void onExit() override;
    bool isFinished() const override { return false; }
    
    bool isScanning() const { return _state == DIAG_SCANNING; }
    int8_t getBeltStatus(int idx) const { return (idx >= 0 && idx < 2) ? (int8_t)(idx == 0 ? _b1->getStatus() : _b2->getStatus()) : 0; }

    void triggerScan();
    void triggerRun(int beltIndex, int distanceMm);

private:
    SystemContext* _ctx;
    ModbusMaster*  _rs485;
    Belt*          _b1;
    Belt*          _b2;
    SemaphoreHandle_t _mutexCtx;

    enum DiagState {
        DIAG_IDLE,
        DIAG_SCANNING,
        DIAG_RUNNING
    };

    DiagState _state;
    uint32_t  _stateTimer;
};

#endif // APP_BELT_DIAG_H
