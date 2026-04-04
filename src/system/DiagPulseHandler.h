#ifndef DIAG_PULSE_HANDLER_H
#define DIAG_PULSE_HANDLER_H

#include "IModeHandler.h"
#include "ModbusMaster.h"
#include "SystemContext.h"
#include <Arduino.h>

class DiagPulseHandler : public IModeHandler {
public:
    DiagPulseHandler(SystemContext* ctx, ModbusMaster* rs485, SemaphoreHandle_t mutex)
        : _ctx(ctx), _rs485(rs485), _mutex(mutex) {}

    void onEnter() override {
        _rs485->clearRawBuffer();
        Serial.println("[DIAG] Raw Pulse Mode Entered.");
    }

    void onLoop() override {
        unsigned long now = millis();
        if (now - _lastPulseTime >= 1000) {
            _lastPulseTime = now;
            xSemaphoreTake(_mutex, portMAX_DELAY);
            _ctx->diag.diagLastSent++;
            uint8_t toSend = _ctx->diag.diagLastSent;
            xSemaphoreGive(_mutex);
            _rs485->sendRawByte(toSend);
        }

        if (_rs485->availableRaw() > 0) {
            xSemaphoreTake(_mutex, portMAX_DELAY);
            while (_rs485->availableRaw() > 0) {
                uint8_t b = _rs485->readRawByte();
                char hexBuf[8];
                snprintf(hexBuf, sizeof(hexBuf), "%02X ", b);
                if (strlen(_ctx->diag.diagRxHex) > 100)
                    memset(_ctx->diag.diagRxHex, 0, sizeof(_ctx->diag.diagRxHex));
                strncat(_ctx->diag.diagRxHex, hexBuf, sizeof(_ctx->diag.diagRxHex) - strlen(_ctx->diag.diagRxHex) - 1);
            }
            xSemaphoreGive(_mutex);
        }
    }

    void onExit() override {
        Serial.println("[DIAG] Raw Pulse Mode Exited.");
    }

    OperationMode getMode() const override { return MODE_DIAG_PULSE; }

private:
    SystemContext*    _ctx;
    ModbusMaster*     _rs485;
    SemaphoreHandle_t _mutex;
    unsigned long     _lastPulseTime = 0;
};

#endif // DIAG_PULSE_HANDLER_H
