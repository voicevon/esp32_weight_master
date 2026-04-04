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
    }

    void onLoop() override {
        unsigned long now = millis();
        if (now - _lastSent >= 1000) {
            _lastSent = now;
            // 发送一个全局读取或特定心跳信号（此处简化为 Raw 诊断）
            _rs485->sendRawByte(0xFF); 
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
};

#endif // APP_DIAG_PULSE_H
