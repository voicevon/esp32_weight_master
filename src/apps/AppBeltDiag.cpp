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

        case DIAG_SCAN_START_B1: {
            int id1 = _conveyor->getMotorId(0);
            Serial.printf("[AppBeltDiag] >>> Scanning Belt 1 (ID: %d)\n", id1);
            _b1Finished = false;
            bool ok = _rs485->asyncRead(id1, REG_BELT_REV, 1, 
                [this, id1](Modbus::ResultCode event, uint16_t transactionId, void* data) -> bool {
                    this->handleScanResult(id1, event);
                    return true;
                }, _scanBuffer);

            if (ok) {
                _state = DIAG_SCAN_WAIT_B1;
                _stateTimer = millis();
            } else {
                Serial.println("[AppBeltDiag] ERR: Bus Busy for B1");
                _state = DIAG_IDLE; // 异常退出
            }
            break;
        }

        case DIAG_SCAN_WAIT_B1:
            if (_b1Finished) {
                _state = DIAG_SCAN_COOLDOWN;
                _stateTimer = millis();
            } else if (millis() - _stateTimer > 2500) { // 强行超时保护
                handleScanResult(_conveyor->getMotorId(0), Modbus::EX_TIMEOUT);
                _state = DIAG_SCAN_COOLDOWN;
                _stateTimer = millis();
            }
            break;

        case DIAG_SCAN_COOLDOWN:
            if (millis() - _stateTimer > 200) { // 强制冷却，确保总线空闲
                _state = DIAG_SCAN_START_B2;
            }
            break;

        case DIAG_SCAN_START_B2: {
            int id2 = _conveyor->getMotorId(1);
            Serial.printf("[AppBeltDiag] >>> Scanning Belt 2 (ID: %d)\n", id2);
            _b2Finished = false;
            bool ok = _rs485->asyncRead(id2, REG_BELT_REV, 1, 
                [this, id2](Modbus::ResultCode event, uint16_t transactionId, void* data) -> bool {
                    this->handleScanResult(id2, event);
                    return true;
                }, _scanBuffer);

            if (ok) {
                _state = DIAG_SCAN_WAIT_B2;
                _stateTimer = millis();
            } else {
                Serial.println("[AppBeltDiag] ERR: Bus Busy for B2");
                _state = DIAG_IDLE;
            }
            break;
        }

        case DIAG_SCAN_WAIT_B2:
            if (_b2Finished) {
                _state = DIAG_IDLE;
                if (_mutexCtx) xSemaphoreTake(_mutexCtx, portMAX_DELAY);
                _ctx->ui.beltDiagScanning = false;
                if (_mutexCtx) xSemaphoreGive(_mutexCtx);
            } else if (millis() - _stateTimer > 2500) {
                handleScanResult(_conveyor->getMotorId(1), Modbus::EX_TIMEOUT);
                _state = DIAG_IDLE;
                if (_mutexCtx) xSemaphoreTake(_mutexCtx, portMAX_DELAY);
                _ctx->ui.beltDiagScanning = false;
                if (_mutexCtx) xSemaphoreGive(_mutexCtx);
            }
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
    
    _state = DIAG_SCAN_START_B1;
    _stateTimer = millis();

    if (_mutexCtx) xSemaphoreTake(_mutexCtx, portMAX_DELAY);
    _ctx->ui.beltDiagScanning = true;
    _ctx->ui.beltStatus[0] = 0; // 等待
    _ctx->ui.beltStatus[1] = 0; 
    if (_mutexCtx) xSemaphoreGive(_mutexCtx);
}

void AppBeltDiag::triggerRun(int beltIndex, int distanceMm) {
    if (_state != DIAG_IDLE) return;

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

void AppBeltDiag::handleScanResult(uint8_t id, Modbus::ResultCode result) {
    int id1 = _conveyor->getMotorId(0);
    int id2 = _conveyor->getMotorId(1);

    int8_t status = 2; // 默认故障 (Offline) 
    if (result == Modbus::EX_SUCCESS) status = 1; // 在线
    else if (result == Modbus::EX_TIMEOUT) status = 3; // 超时

    if (id == id1) {
        if (_mutexCtx) xSemaphoreTake(_mutexCtx, portMAX_DELAY);
        _ctx->ui.beltStatus[0] = status;
        if (_mutexCtx) xSemaphoreGive(_mutexCtx);
        _b1Finished = true;
        _b1Result = result;
        Serial.printf("[AppBeltDiag] Belt 1 (ID:%d) Finished, Result:0x%02X\n", id, (uint8_t)result);
    } 
    else if (id == id2) {
        if (_mutexCtx) xSemaphoreTake(_mutexCtx, portMAX_DELAY);
        _ctx->ui.beltStatus[1] = status;
        if (_mutexCtx) xSemaphoreGive(_mutexCtx);
        _b2Finished = true;
        _b2Result = result;
        Serial.printf("[AppBeltDiag] Belt 2 (ID:%d) Finished, Result:0x%02X\n", id, (uint8_t)result);
    }
}
