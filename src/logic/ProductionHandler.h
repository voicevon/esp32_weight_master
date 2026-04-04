#ifndef PRODUCTION_HANDLER_H
#define PRODUCTION_HANDLER_H

#include "system/IModeHandler.h"
#include "system/SystemContext.h"
#include "system/PollManager.h"
#include "system/ModbusMaster.h"
#include "CombinationEngine.h"
#include "ConveyorController.h"

class ProductionHandler : public IModeHandler {
public:
    ProductionHandler(SystemContext* ctx, PollManager* pollMgr, ModbusMaster* rs485,
                      CombinationEngine* engine, ConveyorController* conveyor,
                      SemaphoreHandle_t mutex)
        : _ctx(ctx), _pollMgr(pollMgr), _rs485(rs485), _engine(engine), 
          _conveyor(conveyor), _mutex(mutex) {}

    void onEnter() override {
        _pollMgr->setMode(MODE_PRODUCTION);
        xSemaphoreTake(_mutex, portMAX_DELAY);
        _ctx->prog.sysStatus = SYS_READY;
        xSemaphoreGive(_mutex);
    }

    void onLoop() override;

    void onExit() override {
        // 可以在此处做一些清理工作
    }

    OperationMode getMode() const override { return MODE_PRODUCTION; }

private:
    SystemContext*      _ctx;
    PollManager*        _pollMgr;
    ModbusMaster*       _rs485;
    CombinationEngine*  _engine;
    ConveyorController* _conveyor;
    SemaphoreHandle_t   _mutex;
    
    unsigned long _lastCalcTime = 0;
};

#endif // PRODUCTION_HANDLER_H
