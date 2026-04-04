#ifndef APP_SCAN_H
#define APP_SCAN_H

#include "apps/IApp.h"
#include "system/SystemContext.h"
#include <Arduino.h>

class PollManager;

/**
 * @class AppScan
 * @brief 节点扫描应用。
 * 负责在诊断模式下遍历所有节点并更新白名单。
 */
class AppScan : public IApp {
public:
    AppScan(SystemContext* ctx, PollManager* pollMgr, SemaphoreHandle_t mutex)
        : _ctx(ctx), _pollMgr(pollMgr), _mutex(mutex) {}

    void onEnter() override {
        Serial.println("[AppScan] Diagnostic Scan Mode Entered.");
        _pollMgr->setMode(MODE_DIAG_SCAN);
    }

    void onLoop() override {
        // 核心逻辑已由 PollManager 内部 handleScanPoll 处理
        // 此处仅作为模式生命周期的锚点
    }

    void onExit() override {
        Serial.println("[AppScan] Diagnostic Scan Mode Exited.");
    }

    OperationMode getMode() const override { return MODE_DIAG_SCAN; }

    bool isFinished() const override {
        // 当 PollManager 自动切回 CONFIGURATION 时判定为完成
        // 此处逻辑可根据需求进一步精化
        return false; 
    }

private:
    SystemContext*    _ctx;
    PollManager*      _pollMgr;
    SemaphoreHandle_t _mutex;
};

#endif // APP_SCAN_H
