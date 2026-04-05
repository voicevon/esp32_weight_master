#ifndef APP_DIAG_PULSE_H
#define APP_DIAG_PULSE_H

#include "apps/IApp.h"
#include "system/SystemContext.h"
#include <Arduino.h>
#include "drivers/ModbusMaster.h"

/**
 * @class AppDiagPulse
 * @brief 诊断脉冲测试应用。
 * 以 1Hz 频率向所有从机发送简单的读取指令，用于目测总线负载与稳定性。
 */
class AppDiagPulse : public IApp {
public:
    AppDiagPulse(SystemContext* ctx, ModbusMaster* rs485, SemaphoreHandle_t mutex)
        : _ctx(ctx), _rs485(rs485), _mutex(mutex) {}

    void onEnter() override {
        Serial.println("[AppDiagPulse] Diagnostic Pulse Mode Entered.");
        _lastSent = 0;
        _counter = 0;
        xSemaphoreTake(_mutex, portMAX_DELAY);
        _ctx->ui.diagTxCount = 0;
        _ctx->ui.diagRxCount = 0;
        memset(_ctx->ui.diagRxHex, 0, sizeof(_ctx->ui.diagRxHex));
        xSemaphoreGive(_mutex);
    }

    void onLoop() override {
        unsigned long now = millis();

        // 1. 发送逻辑 (1Hz)
        if (now - _lastSent >= 1000) {
            _lastSent = now;
            uint8_t val = _counter++;
            _rs485->sendRawByte(val);
            
            xSemaphoreTake(_mutex, portMAX_DELAY);
            _ctx->ui.diagTxCount++;
            _ctx->ui.diagTxValue = val;
            xSemaphoreGive(_mutex);
        }

        // 2. 接收逻辑 (非阻塞)
        while (_rs485->availableRaw() > 0) {
            uint8_t b = _rs485->readRawByte();
            xSemaphoreTake(_mutex, portMAX_DELAY);
            _ctx->ui.diagRxCount++;
            
            // 简单追加到 HEX 显示字符串 (最近 16 字节)
            char tmp[4];
            snprintf(tmp, sizeof(tmp), "%02X ", b);
            
            size_t len = strlen(_ctx->ui.diagRxHex);
            if (len > 40) { // 超过 15 字节左右就截断滚动
                memmove(_ctx->ui.diagRxHex, _ctx->ui.diagRxHex + 3, len - 2);
                strcat(_ctx->ui.diagRxHex, tmp);
            } else {
                strcat(_ctx->ui.diagRxHex, tmp);
            }
            xSemaphoreGive(_mutex);
        }
    }

    void onExit() override {
        Serial.println("[AppDiagPulse] Diagnostic Pulse Mode Exited.");
    }

    OperationMode getMode() const override { return MODE_DIAG_PULSE; }

private:
    SystemContext*    _ctx;
    ModbusMaster*     _rs485;
    SemaphoreHandle_t _mutex;
    unsigned long     _lastSent = 0;
    uint8_t           _counter = 0;
};

#endif // APP_DIAG_PULSE_H
