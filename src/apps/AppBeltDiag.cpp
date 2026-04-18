#include "apps/AppBeltDiag.h"
#include "system/SystemConfig.h"

AppBeltDiag::AppBeltDiag(SystemContext* ctx, ModbusMaster* rs485, Belt* b1, Belt* b2, SemaphoreHandle_t mutexCtx)
    : _ctx(ctx), _rs485(rs485), _b1(b1), _b2(b2), _mutexCtx(mutexCtx), _state(DIAG_IDLE), _stateTimer(0) {
}

void AppBeltDiag::onEnter() {
    _state = DIAG_IDLE;
    if (_mutexCtx) xSemaphoreTake(_mutexCtx, portMAX_DELAY);
    _ctx->prog.sysStatus = SYS_READY;
    strncpy(_ctx->prog.statusText, "皮带诊断模式", 32);
    _ctx->ui.beltDiagScanning = false;
    _ctx->ui.beltStatus[0] = 0;
    _ctx->ui.beltStatus[1] = 0;
    _ctx->ui.beltIsMoving[0] = false;
    _ctx->ui.beltIsMoving[1] = false;
    if (_mutexCtx) xSemaphoreGive(_mutexCtx);
}

void AppBeltDiag::onLoop() {
    switch (_state) {
        case DIAG_IDLE:
            break;

        case DIAG_SCANNING:
            // 扫描由各 Belt 对象异步执行，这里仅需轮询状态并更新 UI
            if (_mutexCtx) xSemaphoreTake(_mutexCtx, portMAX_DELAY);
            _ctx->ui.beltStatus[0] = (int8_t)_b1->getStatus();
            _ctx->ui.beltStatus[1] = (int8_t)_b2->getStatus();
            _ctx->prog.dirtyFlags |= DF_PROGRESS; // 必须立起标志，UI 才会刷新
            if (_mutexCtx) xSemaphoreGive(_mutexCtx);
            break;

        case DIAG_RUNNING:
            if (millis() - _stateTimer > 1500) {
                _state = DIAG_IDLE;
                if (_mutexCtx) xSemaphoreTake(_mutexCtx, portMAX_DELAY);
                _ctx->ui.beltIsMoving[0] = false;
                _ctx->ui.beltIsMoving[1] = false;
                if (_mutexCtx) xSemaphoreGive(_mutexCtx);
            }
            break;
    }
}

void AppBeltDiag::onExit() {
    _state = DIAG_IDLE;
    if (_mutexCtx) xSemaphoreTake(_mutexCtx, portMAX_DELAY);
    _ctx->ui.beltDiagScanning = false;
    _ctx->ui.beltIsMoving[0] = false;
    _ctx->ui.beltIsMoving[1] = false;
    if (_mutexCtx) xSemaphoreGive(_mutexCtx);
}

void AppBeltDiag::triggerScan() {
    if (_state != DIAG_IDLE) return;
    
    _state = DIAG_SCANNING;
    _stateTimer = millis();

    if (_mutexCtx) xSemaphoreTake(_mutexCtx, portMAX_DELAY);
    _ctx->ui.beltStatus[0] = 0; // 等待
    _ctx->ui.beltStatus[1] = 0; 
    _ctx->prog.dirtyFlags |= DF_PROGRESS; // 开始扫描，立旗
    if (_mutexCtx) xSemaphoreGive(_mutexCtx);

    // 顺序触发两个电机的异步扫描
    _b1->scan([this](bool ok1) {
        this->_b2->scan([this](bool ok2) {
            if (this->_state == DIAG_SCANNING) {
                this->_state = DIAG_IDLE;
                if (this->_mutexCtx) xSemaphoreTake(this->_mutexCtx, portMAX_DELAY);
                this->_ctx->ui.beltDiagScanning = false;
                this->_ctx->ui.beltStatus[0] = (int8_t)this->_b1->getStatus();
                this->_ctx->ui.beltStatus[1] = (int8_t)this->_b2->getStatus();
                this->_ctx->prog.dirtyFlags |= DF_PROGRESS; // 扫描结束，最终立旗
                if (this->_mutexCtx) xSemaphoreGive(this->_mutexCtx);
                Serial.println("[AppBeltDiag] Sequential Scan Finished.");
            }
        });
    });
}

void AppBeltDiag::triggerRun(int beltIndex, int distanceMm) {
    if (_state != DIAG_IDLE) return;

    _state = DIAG_RUNNING;
    _stateTimer = millis();
    
    if (_mutexCtx) xSemaphoreTake(_mutexCtx, portMAX_DELAY);
    if (beltIndex == 0 || beltIndex == 1) {
        _ctx->ui.beltIsMoving[beltIndex] = true;
    }
    if (_mutexCtx) xSemaphoreGive(_mutexCtx);

    if (beltIndex == 0) _b1->moveDistanceMm(distanceMm);
    else if (beltIndex == 1) _b2->moveDistanceMm(distanceMm);
}

void AppBeltDiag::triggerRunToggle(int beltIndex, bool run) {
    if (beltIndex != 0 && beltIndex != 1) return;

    if (_mutexCtx) xSemaphoreTake(_mutexCtx, portMAX_DELAY);
    _ctx->ui.beltIsMoving[beltIndex] = run;
    if (_mutexCtx) xSemaphoreGive(_mutexCtx);

    if (beltIndex == 0) {
        if (run) _b1->moveRelative(PULSES_PER_REV * 10); // 位置模式下 run 默认为转 10 圈
        else _b1->positionPause();
    } else {
        if (run) _b2->speedRun();
        else _b2->speedStop();
    }
    
    // 如果是启动，更新状态和计时器（防止 UI 立即切回 IDLE）
    if (run) {
        _state = DIAG_RUNNING;
        _stateTimer = millis();
    } else {
        _state = DIAG_IDLE;
    }
}
