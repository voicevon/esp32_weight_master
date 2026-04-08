#ifndef APP_BELT_DIAG_H
#define APP_BELT_DIAG_H

#include "apps/IApp.h"
#include "system/SystemContext.h"
#include "drivers/ModbusMaster.h"
#include "logic/BeltManager.h"
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
    AppBeltDiag(SystemContext* ctx, ModbusMaster* rs485, BeltManager* conveyor, SemaphoreHandle_t mutexCtx);

    OperationMode getMode() const override { return MODE_BELT_DIAG; }
    void onEnter() override;
    void onLoop() override;
    void onExit() override;
    bool isFinished() const override { return false; }

    void triggerScan();
    void triggerRun(int beltIndex, int distanceMm);

private:
    SystemContext* _ctx;
    ModbusMaster*  _rs485;
    BeltManager*   _conveyor;
    SemaphoreHandle_t _mutexCtx;

    BeltDiagState _state;
    unsigned long _stateTimer;

    int _runBeltIndex;
    int _runDistance;

    uint16_t _scanBuffer[2];

    void handleScanResult(uint8_t id, bool success);
};

#endif // APP_BELT_DIAG_H
