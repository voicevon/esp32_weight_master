#ifndef APP_MODBUS_DIAG_H
#define APP_MODBUS_DIAG_H

#include "apps/IApp.h"
#include "system/SystemContext.h"
#include "drivers/ModbusMaster.h"

class AppModbusDiag : public IApp {
public:
    AppModbusDiag(SystemContext* ctx, ModbusMaster* rs485);

    void onEnter() override;
    void onLoop() override;
    void onExit() override;
    bool isFinished() const override { return false; }
    OperationMode getMode() const override { return MODE_MODBUS_DIAG; }

    void triggerSendHex(const char* hexStr);
    void toggleAutoSend(bool enable);

    // 新增控制接口
    void setSubMode(DiagSubMode mode);
    DiagSubMode getSubMode() const { return _subMode; }
    
    void setTargetId(int id) { _targetId = id; }
    int  getTargetId() const { return _targetId; }
    
    void triggerAction(int actionId);

private:
    SystemContext* _ctx;
    ModbusMaster*  _rs485;

    DiagSubMode _subMode = DIAG_SUB_PULSE;
    int         _targetId = 21; // 默认目标：皮带 1

    bool     _autoSend = false;
    uint32_t _lastSendTime = 0;
    uint8_t  _autoByte = 0x00;

    uint8_t  _rxBuf[256];
    int      _rxIdx = 0;
    uint32_t _lastRxTime = 0;
    bool     _waitingResponse = false;

    void pushLog(const char* tag, const char* hex);
    void updateRxHex();
    
    // 指令生成辅助
    void sendMotorMove(int distance);
    void sendMotorStop();
    void sendMotorIO(uint16_t value);
};

#endif
