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
    
    bool isScanning() const { return _state >= DIAG_SCAN_START_B1 && _state <= DIAG_SCAN_WAIT_B2; }
    int8_t getBeltStatus(int idx) const { return (idx >= 0 && idx < 2) ? _ctx->ui.beltStatus[idx] : 0; }

    void triggerScan();
    void triggerRun(int beltIndex, int distanceMm);

private:
    SystemContext* _ctx;
    ModbusMaster*  _rs485;
    BeltManager*   _conveyor;
    SemaphoreHandle_t _mutexCtx;

    enum DiagState {
        DIAG_IDLE,
        DIAG_SCAN_START_B1,
        DIAG_SCAN_WAIT_B1,
        DIAG_SCAN_COOLDOWN, // 扫描间隙冷却，防止总线竞争
        DIAG_SCAN_START_B2,
        DIAG_SCAN_WAIT_B2,
        DIAG_RUNNING
    };

    DiagState _state;
    uint32_t  _stateTimer;
    uint16_t  _scanBuffer[8];
    
    bool _b1Finished = false;
    bool _b2Finished = false;
    Modbus::ResultCode _b1Result = Modbus::EX_SUCCESS;
    Modbus::ResultCode _b2Result = Modbus::EX_SUCCESS;

    void handleScanResult(uint8_t id, Modbus::ResultCode result);
};

#endif // APP_BELT_DIAG_H
