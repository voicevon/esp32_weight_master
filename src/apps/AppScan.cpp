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
    _cancelRequested = false;
}

void AppScan::onLoop() {
    if (_isFinished) return;

    // 检查总线状态
    bool isWaiting = (_rs485 && _rs485->getStatus() == ModbusMaster::ST_WAITING);

    // [新增] 响应 UI 取消请求
    if (_cancelRequested) {
        if (!isWaiting) {
            Serial.println("[AppScan] CANCEL SAFE: Current Modbus transaction finished. Exiting.");
            _isFinished = true;
        }
        return; // 取消后不继续执行后续逻辑
    }

    if (isWaiting) return;

    // 如果刚发完请求（通过 _lastRequestTime 判断或驱动层状态）
    // 且现在驱动层已经非 WAITING，说明前一个节点的请求已由回调处理完成
    if (_lastRequestTime > 0) {
        // 记录本轮结果
        _scanHistory[_scanCycle][_scanProgress] = _nodeMgr->isOnline(_scanProgress);
        
        // 推进 ID
        _scanProgress++;
        _lastRequestTime = 0;

        if (_scanProgress > 20) {
            _scanProgress = 1;
            _scanCycle++;
            if (_scanCycle >= 5) {
                _isFinished = true;
                
                // [修复] 应用 3/5 多数表决原则生成白名单
                for (int i = 1; i <= 20; i++) {
                    int onlineCount = 0;
                    for (int c = 0; c < 5; c++) {
                        if (_scanHistory[c][i]) onlineCount++;
                    }
                    // 如果 5 次扫描中有 3 次及以上在线，则认为该节点有效
                    _nodeMgr->setWhitelisted(i, onlineCount >= 3);
                }

                _nodeMgr->saveWhitelist(); // 应用后立即持久化到 NVS
                _ctx->prog.dirtyFlags |= DF_NODE_DATA; // 节点白名单数据发生变更
                Serial.println("[AppScan] Scan results applied using 3/5 rule and saved.");
            }
        }
        _ctx->prog.dirtyFlags |= DF_PROGRESS; // 扫描进度变更
        return;
    }

    // 下发下一个请求
    if (_nodeMgr->asyncUpdateNode(_scanProgress)) {
        _lastRequestTime = millis();
    }
}

void AppScan::onExit() {
    Serial.println("[AppScan] Diagnostic Scan Mode Stopped.");
}
