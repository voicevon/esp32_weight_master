#include "AppModbusDiag.h"
#include <Arduino.h>
#include "drivers/Belt.h"

// 寄存器地址由 Belt.h 提供定义

AppModbusDiag::AppModbusDiag(SystemContext* ctx, ModbusMaster* rs485)
    : _ctx(ctx), _rs485(rs485) {}

void AppModbusDiag::onEnter() {
    Serial.println("[AppModbusDiag] Terminal Started.");
    _subMode = DIAG_SUB_PULSE; 
    _autoSend = true;          
    _waitingResponse = false;
    _lastSendTime = 0; // [重要] 重置时间，立即使第一次脉冲生效
    
    _ctx->ui.diagSubMode = _subMode;
    _ctx->prog.diagAutoSend = _autoSend;
    _ctx->prog.diagLogTick = 0;
    memset(_ctx->prog.diagLogLine, 0, sizeof(_ctx->prog.diagLogLine));
    _rs485->clearRawBuffer();
    
    pushLog("SYS", "Diagnostic Terminal Ready (Pulse 1Hz Enabled)");
}

void AppModbusDiag::onLoop() {
    uint32_t now = millis();

    // 1. 每 5 秒通过串口发送一次心跳日志 (串口专属)
    static uint32_t lastHeartbeat = 0;
    if (now - lastHeartbeat > 5000) {
        lastHeartbeat = now;
        Serial.printf("[HEARTBEAT] ModbusDiag is running. SubMode: %d, AutoSend: %d\n", (int)_subMode, (int)_autoSend);
    }

    // 2. 处理脉冲模式 (1Hz)
    if (_subMode == DIAG_SUB_PULSE && _autoSend && (now - _lastSendTime > 1000 || _lastSendTime == 0)) {
        _lastSendTime = now;
        _rs485->sendRawByte(_autoByte);
        
        char hexBuffer[16];
        snprintf(hexBuffer, sizeof(hexBuffer), "0x%02X", _autoByte);
        pushLog("TX >", hexBuffer);
        
        _autoByte++;
        _rxIdx = 0; 
    }

    // 3. 原始字节捕获 (非阻塞)
    if (_rs485->availableRaw()) {
        while (_rs485->availableRaw() && _rxIdx < 250) {
            _rxBuf[_rxIdx++] = _rs485->readRawByte();
        }
        _lastRxTime = now; 
    }

    // 4. 50ms 寂静期判帧 (非阻塞)
    if (_rxIdx > 0 && (now - _lastRxTime > 50)) {
        updateRxHex();
        _rxIdx = 0; 
    }
}

void AppModbusDiag::onExit() {
    Serial.println("[AppModbusDiag] Terminal Stopped.");
    _autoSend = false;
}

void AppModbusDiag::setSubMode(DiagSubMode mode) {
    if (_subMode == mode) return;
    _subMode = mode;
    
    if (mode == DIAG_SUB_PULSE) {
        _autoSend = true; 
        pushLog("SYS", "Switch to PULSE mode");
    } else {
        _autoSend = false; 
        pushLog("SYS", "Switch to COMMAND mode");
    }
}

void AppModbusDiag::triggerAction(int actionId) {
    if (_subMode != DIAG_SUB_COMMAND) {
        pushLog("WAR", "Switch to COMMAND mode first.");
        return;
    }

    switch(actionId) {
        case 0: sendMotorStop(); break;
        case 1: sendMotorIO(1);  break; // 触发运行
        case 2: sendMotorMove(100); break;
        case 3: sendMotorMove(500); break;
        default: break;
    }
}

void AppModbusDiag::sendMotorMove(int distance) {
    int32_t pulses = (int32_t)distance * PULSES_PER_MM;
    int16_t revs = pulses / PULSES_PER_REV;
    int16_t pls  = pulses % PULSES_PER_REV;

    char msg[64];
    snprintf(msg, sizeof(msg), "MOVE %dmm ID:%d", distance, _targetId);
    pushLog("CMD", msg);

    _rs485->syncWrite(_targetId, REG_POS1_REV, (uint16_t)revs);
    _rs485->syncWrite(_targetId, REG_POS1_PULSE, (uint16_t)pls);
    _rs485->syncWrite(_targetId, REG_POS1_SPEED, 150); // 设定速度
    _rs485->syncWrite(_targetId, REG_VIRTUAL_IO, 0);
    _rs485->syncWrite(_targetId, REG_VIRTUAL_IO, 1);
}

void AppModbusDiag::sendMotorStop() {
    pushLog("CMD", "STOP MOTOR");
    _rs485->syncWrite(_targetId, REG_VIRTUAL_IO, 0x02); 
}

void AppModbusDiag::sendMotorIO(uint16_t value) {
    pushLog("CMD", "TRIGGER RUN");
    _rs485->syncWrite(_targetId, REG_VIRTUAL_IO, value);
}

void AppModbusDiag::triggerSendHex(const char* hexStr) {
    if (!hexStr || strlen(hexStr) < 2) return;

    uint8_t buf[64];
    int len = 0;
    const char* p = hexStr;
    while (*p && len < 64) {
        if (isxdigit(p[0]) && isxdigit(p[1])) {
            buf[len++] = (uint8_t)strtol(p, NULL, 16);
            p += 2;
        } else { p++; }
    }

    if (len > 0) {
        _rs485->sendRawBuffer(buf, len);
        pushLog("TX >", hexStr);
        _rxIdx = 0;
    }
}

void AppModbusDiag::toggleAutoSend(bool enable) {
    if (_subMode != DIAG_SUB_PULSE) return;
    _autoSend = enable;
    _ctx->prog.diagAutoSend = enable;
    _ctx->prog.dirtyFlags |= DF_DIAG;
    pushLog("SYS", enable ? "Pulse ON" : "Pulse OFF");
}

void AppModbusDiag::pushLog(const char* tag, const char* hex) {
    // 1. 更新逻辑层缓冲区
    snprintf(_ctx->prog.diagLogLine, sizeof(_ctx->prog.diagLogLine), "[%s] %s", tag, hex);
    _ctx->prog.diagLogTick++;
    _ctx->prog.dirtyFlags |= DF_DIAG;

    // 2. [核心改进] 串口镜像输出，方便排查

    // 2. [核心改进] 串口镜像输出，方便排查
    Serial.printf("[%s] %s\n", tag, hex);
}

void AppModbusDiag::updateRxHex() {
    if (_rxIdx == 0) return;
    char hexStr[256];
    int pos = 0;
    for (int i = 0; i < _rxIdx && pos < 250; i++) {
        pos += snprintf(hexStr + pos, 256 - pos, "%02X ", _rxBuf[i]);
    }
    pushLog("RX <", hexStr);
}
