#include "apps/AppScan.h"
#include <Arduino.h>
#include "drivers/ModbusMaster.h"

void AppScan::onEnter() {
    Serial.println("[AppScan] Diagnostic Scan Mode Started.");
    _scanProgress = 1;
    _scanCycle = 0;
    _isFinished = false;
    memset(_scanHistory, 0, sizeof(_scanHistory));
    _lastRequestTime = 0;
}

void AppScan::onLoop() {
    if (_isFinished) return;

    // 检查总线状态
    if (_rs485 && _rs485->getStatus() == ModbusMaster::ST_WAITING) {
        return;
    }

    // 如果刚发完请求（通过 _lastRequestTime 判断或驱动层状态）
    // 且现在驱动层已经非 WAITING，说明前一个节点的请求已由回调处理完成
    if (_lastRequestTime > 0) {
        // 记录本轮结果
        _scanHistory[_scanCycle][_scanProgress] = _pollMgr->isOnline(_scanProgress);
        
        // 推进 ID
        _scanProgress++;
        _lastRequestTime = 0;

        if (_scanProgress > 20) {
            _scanProgress = 1;
            _scanCycle++;
            if (_scanCycle >= 5) {
                _isFinished = true;
                _pollMgr->saveWhitelist(); // 完成后保存白名单
                Serial.println("[AppScan] Scan Sequence Completed.");
            }
        }
        return;
    }

    // 下发下一个请求
    if (_pollMgr->asyncUpdateNode(_scanProgress)) {
        _lastRequestTime = millis();
    }
}

void AppScan::onExit() {
    Serial.println("[AppScan] Diagnostic Scan Mode Stopped.");
}
