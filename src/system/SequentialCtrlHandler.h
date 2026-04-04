#ifndef SEQUENTIAL_CTRL_HANDLER_H
#define SEQUENTIAL_CTRL_HANDLER_H

#include "IModeHandler.h"
#include "ModbusMaster.h"
#include "PollManager.h"
#include "SystemContext.h"
#include <Arduino.h>

enum SeqTaskType {
    SEQ_TASK_NONE,
    SEQ_TASK_GLOBAL_TARE,
    SEQ_TASK_CUSTOM
};

class SequentialCtrlHandler : public IModeHandler {
public:
    SequentialCtrlHandler(SystemContext* ctx, ModbusMaster* rs485, PollManager* pollMgr, SemaphoreHandle_t mutex)
        : _ctx(ctx), _rs485(rs485), _pollMgr(pollMgr), _mutex(mutex) {}

    void onEnter() override {
        Serial.println("[SEQ] Sequential Control Mode Entered.");
        _isFinished = false;
        _progress = 0;
    }

    void onLoop() override {
        if (_isFinished) return;

        switch (_taskType) {
            case SEQ_TASK_GLOBAL_TARE:
                runGlobalTare();
                break;
            default:
                _isFinished = true;
                break;
        }
    }

    void onExit() override {
        Serial.println("[SEQ] Sequential Control Mode Exited.");
    }

    OperationMode getMode() const override { return MODE_SEQUENTIAL_CTRL; }

    // 指令触发接口
    void triggerGlobalTare() {
        _taskType = SEQ_TASK_GLOBAL_TARE;
        _isFinished = false;
        _progress = 0;
    }

    bool isFinished() const { return _isFinished; }
    int getProgress() const { return _progress; }

private:
    SystemContext*      _ctx;
    ModbusMaster*       _rs485;
    PollManager*        _pollMgr;
    SemaphoreHandle_t   _mutex;

    SeqTaskType _taskType = SEQ_TASK_NONE;
    bool        _isFinished = true;
    int         _progress = 0;

    void runGlobalTare() {
        Serial.println("[SEQ] Executing Global Tare Sequence...");
        
        int onlineDevices[21];
        int count = 0;
        for (int i = 1; i <= 20; i++) {
            if (_pollMgr->isOnline(i)) {
                onlineDevices[count++] = i;
            }
        }

        if (count == 0) {
            _isFinished = true;
            _progress = 100;
            return;
        }

        for (int i = 0; i < count; i++) {
            int id = onlineDevices[i];
            Serial.printf("[SEQ] Taring Node %d (%d/%d)...\n", id, i + 1, count);
            
            bool ok = _rs485->syncWrite(id, 0x0100, 3); // 3 = CMD_TARE
            if (ok) Serial.printf("[SEQ] Node %d: SUCCESS\n", id);
            else   Serial.printf("[SEQ] Node %d: FAILED\n", id);

            _progress = ((i + 1) * 100) / count;
            
            // 同步进度到 UI 上下文 (如果需要实时更新)
            // _ctx->ui.tareProgress = _progress; 
            
            vTaskDelay(pdMS_TO_TICKS(50)); // 必要的总线间歇
        }

        _isFinished = true;
        _progress = 100;
        Serial.println("[SEQ] Global Tare Sequence FINISHED.");
    }
};

#endif // SEQUENTIAL_CTRL_HANDLER_H
