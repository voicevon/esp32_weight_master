#include "apps/AppBeltDiag.h"
#include "system/SystemConfig.h"

AppBeltDiag::AppBeltDiag(SystemContext* ctx, ModbusMaster* rs485, BeltManager* conveyor, SemaphoreHandle_t mutexCtx)
    : _ctx(ctx), _rs485(rs485), _conveyor(conveyor), _mutexCtx(mutexCtx), _state(DIAG_IDLE), _stateTimer(0) {
}

void AppBeltDiag::onEnter() {
    _state = DIAG_IDLE;
    if (_mutexCtx) xSemaphoreTake(_mutexCtx, portMAX_DELAY);
    _ctx->prog.sysStatus = SYS_READY;
    strncpy(_ctx->prog.statusText, "皮带诊断模式", 32);
    _ctx->ui.beltDiagScanning = false;
    _ctx->ui.beltIsMoving[0] = false;
    _ctx->ui.beltIsMoving[1] = false;
    if (_mutexCtx) xSemaphoreGive(_mutexCtx);
}

void AppBeltDiag::onLoop() {
    // 简单的超时恢复机制防止卡死
    switch (_state) {
        case DIAG_IDLE:
            break;
        case DIAG_SCANNING_BELT1:
        case DIAG_SCANNING_BELT2:
            if (millis() - _stateTimer > 2000) { // 读超时2秒
                _state = DIAG_IDLE;
                if (_mutexCtx) xSemaphoreTake(_mutexCtx, portMAX_DELAY);
                _ctx->ui.beltDiagScanning = false;
                if (_mutexCtx) xSemaphoreGive(_mutexCtx);
            }
            break;
        case DIAG_RUNNING:
            if (millis() - _stateTimer > 1500) { // 发送指令后UI态保持1.5秒
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
    
    _state = DIAG_SCANNING_BELT1;
    _stateTimer = millis();

    if (_mutexCtx) xSemaphoreTake(_mutexCtx, portMAX_DELAY);
    _ctx->ui.beltDiagScanning = true;
    _ctx->ui.beltOnline[0] = false;
    _ctx->ui.beltOnline[1] = false;
    if (_mutexCtx) xSemaphoreGive(_mutexCtx);

    int id1 = _conveyor->getMotorId(0);
    // 异步读某寄存器验证连通性
    _rs485->asyncRead(id1, REG_CMD_CONTROL, 1, 
        [this](Modbus::ResultCode event, uint16_t transactionId, void* data) -> bool {
            this->handleScanResult((uint8_t)transactionId, (event == Modbus::EX_SUCCESS));
            return true;
        }, 
        _scanBuffer);
}

void AppBeltDiag::triggerRun(int beltIndex, int distanceMm) {
    if (_state == DIAG_SCANNING_BELT1 || _state == DIAG_SCANNING_BELT2) return;

    _state = DIAG_RUNNING;
    _stateTimer = millis();
    int motorId = _conveyor->getMotorId(beltIndex);
    
    if (_mutexCtx) xSemaphoreTake(_mutexCtx, portMAX_DELAY);
    if (beltIndex == 0 || beltIndex == 1) {
        _ctx->ui.beltIsMoving[beltIndex] = true;
    }
    if (_mutexCtx) xSemaphoreGive(_mutexCtx);

    if (_conveyor) {
        _conveyor->moveDistanceMm(motorId, distanceMm);
    }
}

void AppBeltDiag::handleScanResult(uint8_t id, bool success) {
    int id1 = _conveyor->getMotorId(0);
    int id2 = _conveyor->getMotorId(1);

    if (id == id1) {
        if (_mutexCtx) xSemaphoreTake(_mutexCtx, portMAX_DELAY);
        _ctx->ui.beltOnline[0] = success;
        if (_mutexCtx) xSemaphoreGive(_mutexCtx);
        
        // 接着扫描皮带2
        _state = DIAG_SCANNING_BELT2;
        _stateTimer = millis();
        _rs485->asyncRead(id2, REG_CMD_CONTROL, 1, 
            [this](Modbus::ResultCode event, uint16_t transactionId, void* data) -> bool {
                this->handleScanResult((uint8_t)transactionId, (event == Modbus::EX_SUCCESS));
                return true;
            }, 
            _scanBuffer);
    } 
    else if (id == id2) {
        if (_mutexCtx) xSemaphoreTake(_mutexCtx, portMAX_DELAY);
        _ctx->ui.beltOnline[1] = success;
        _ctx->ui.beltDiagScanning = false;
        if (_mutexCtx) xSemaphoreGive(_mutexCtx);
        
        _state = DIAG_IDLE;
    }
}
