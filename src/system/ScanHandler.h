#ifndef SCAN_HANDLER_H
#define SCAN_HANDLER_H

#include "IModeHandler.h"
#include "PollManager.h"
#include "SystemContext.h"
#include <Arduino.h>

class ScanHandler : public IModeHandler {
public:
    ScanHandler(SystemContext* ctx, PollManager* pollMgr, SemaphoreHandle_t mutex)
        : _ctx(ctx), _pollMgr(pollMgr), _mutex(mutex) {}

    void onEnter() override {
        _pollMgr->setMode(MODE_DIAG_SCAN);
        Serial.println("[SCAN] Diagnostic Scan Mode Entered.");
    }

    void onLoop() override {
        _pollMgr->process();

        // 将扫描进度同步到上下文
        xSemaphoreTake(_mutex, portMAX_DELAY);
        _ctx->diag.scanProgress     = _pollMgr->getScanProgress();
        _ctx->diag.currentScanCycle = _pollMgr->getScanCycle();
        for(int c=0; c<5; c++) {
            for(int i=1; i<=20; i++) {
                _ctx->diag.scanResults[c][i] = _pollMgr->getScanHistory(c, i);
            }
        }
        xSemaphoreGive(_mutex);
    }

    void onExit() override {
        Serial.println("[SCAN] Diagnostic Scan Mode Exited.");
    }

    OperationMode getMode() const override { return MODE_DIAG_SCAN; }

private:
    SystemContext*    _ctx;
    PollManager*      _pollMgr;
    SemaphoreHandle_t _mutex;
};

#endif // SCAN_HANDLER_H
