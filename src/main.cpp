#include <Arduino.h>
#include <Wire.h>
#include <vector>
#include <Preferences.h>
#include "HardwareManager.h"
#include "UIManager.h"
#include "system/ModbusMaster.h"
#include "system/PinDefinition.h"
#include "system/SystemContext.h"
#include "logic/CombinationEngine.h"
#include "logic/ConveyorController.h"
#include "system/PollManager.h"

// --- Global Manager Instances ---
HardwareManager hw;
UIManager ui;
Preferences nvs;

// ---------------------------
// 共享资源与状态上下文
// ---------------------------
SystemContext globalCtx;
std::vector<float> slaveWeights(NUM_SLAVES, 0.0f);
std::vector<bool> slaveStable(NUM_SLAVES, false);
SystemStatus systemStatus = SYS_INIT;
float lastCombinedWeight = 0.0f;
uint32_t currentSelectedMask = 0; 
float accumulatedTotalWeight = 0.0f;
bool isProductionActive = true; // Default Active for Phase 3 

SemaphoreHandle_t mutexParams; 
SemaphoreHandle_t mutexWeights; 
SemaphoreHandle_t mutexStatus;  

// 初始化 ModbusMaster
ModbusMaster rs485(PIN_RS485_RX, PIN_RS485_TX, PIN_RS485_TX_EN, RS485_BAUD);

// 初始化业务调度中心 (PollManager)
PollManager pollManager(&rs485);

// 初始化其他业务对象
CombinationEngine engine(290.0f, 310.0f); 
ConveyorController conveyor(&rs485, MOTOR_ID_BELT1, MOTOR_ID_BELT2);

// --- Task Definitions ---
// --- 辅助：将 OperationMode 枚举转换为可读字符串 ---
static const char* operationModeToStr(OperationMode m) {
    switch (m) {
        case MODE_IDLE:              return "IDLE";
        case MODE_PRODUCTION:        return "PRODUCTION";
        case MODE_DIAG_PULSE:        return "DIAG_PULSE";
        case MODE_DIAG_SCAN:         return "DIAG_SCAN";
        case MODE_DIAG_DETAIL:       return "DIAG_DETAIL";
        case MODE_CONFIGURATION:     return "CONFIGURATION";
        case MODE_SEQUENTIAL_CTRL:   return "SEQUENTIAL_CTRL";
        case MODE_ABOUT:             return "ABOUT";
        default:                     return "UNKNOWN";
    }
}

// --- 全局管理：统一切换运行模式 ---
void updateOperationMode(OperationMode newMode) {
    if (globalCtx.state.curMode == newMode) return;
    
    Serial.printf("[SYSTEM] Mode Changing: %s -> %s\n",
                  operationModeToStr(globalCtx.state.curMode),
                  operationModeToStr(newMode));
    
    // 退出旧模式的清理
    if (globalCtx.state.curMode == MODE_DIAG_SCAN || globalCtx.state.curMode == MODE_DIAG_PULSE) {
        rs485.clearRawBuffer();
    }
    
    xSemaphoreTake(mutexStatus, portMAX_DELAY);
    globalCtx.state.curMode = newMode;
    xSemaphoreGive(mutexStatus);

    // 同步新模式到 PollManager (调度中心)
    pollManager.setMode(newMode);
}

void controlTask(void* pvParameters) {
    Serial.println("[TASK] Control Task Started on Core 1");
    static unsigned long lastDiagPulseTime = 0;
    static unsigned long lastCalcTime = 0;
    
    while (true) {
        OperationMode mode;
        xSemaphoreTake(mutexStatus, portMAX_DELAY);
        mode = globalCtx.state.curMode;
        xSemaphoreGive(mutexStatus);

        // 1. 链路诊断模式 (1Hz 原始字节测试)
        if (mode == MODE_DIAG_PULSE) {
            if (millis() - lastDiagPulseTime >= 1000) {
                lastDiagPulseTime = millis();
                xSemaphoreTake(mutexStatus, portMAX_DELAY);
                globalCtx.state.diagLastSent++;
                uint8_t toSend = globalCtx.state.diagLastSent;
                xSemaphoreGive(mutexStatus);
                rs485.sendRawByte(toSend);
            }

            if (rs485.availableRaw() > 0) {
                xSemaphoreTake(mutexStatus, portMAX_DELAY);
                while (rs485.availableRaw() > 0) {
                    uint8_t b = rs485.readRawByte();
                    char hexBuf[8];
                    snprintf(hexBuf, sizeof(hexBuf), "%02X ", b);
                    if (strlen(globalCtx.state.diagRxHex) > 100) {
                        memset(globalCtx.state.diagRxHex, 0, sizeof(globalCtx.state.diagRxHex));
                    }
                    strncat(globalCtx.state.diagRxHex, hexBuf, sizeof(globalCtx.state.diagRxHex) - strlen(globalCtx.state.diagRxHex) - 1);
                }
                xSemaphoreGive(mutexStatus);
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }


        // 2. Modbus 调度与业务处理
        pollManager.process(); 
        
        if (mode == MODE_PRODUCTION) {
            // 从 PollManager 获取最新数据
            xSemaphoreTake(mutexWeights, portMAX_DELAY);
            for (int i = 0; i < NUM_SLAVES; i++) {
                slaveWeights[i] = pollManager.getWeight(i + 1);
                slaveStable[i] = pollManager.isStable(i + 1);
            }
            xSemaphoreGive(mutexWeights);

            // 生产逻辑处理
            bool canCalculate = false;
            float currentMin, currentMax;

            xSemaphoreTake(mutexParams, portMAX_DELAY);
            canCalculate = isProductionActive;
            currentMin = globalCtx.config.targetMin;
            currentMax = globalCtx.config.targetMax;
            xSemaphoreGive(mutexParams);

            SystemStatus currentStatus;
            xSemaphoreTake(mutexStatus, portMAX_DELAY);
            currentStatus = systemStatus;
            xSemaphoreGive(mutexStatus);

            if (canCalculate && currentStatus == SYS_READY) {
                if (millis() - lastCalcTime > CALC_ENGINE_INTERVAL_MS) { 
                    lastCalcTime = millis();
                    engine.setTargetRange(currentMin, currentMax);
                    
                    std::vector<float> activeWeights;
                    std::vector<int> activeIds;
                    for (int i = 0; i < NUM_SLAVES; i++) {
                        int id = i + 1;
                        if (slaveStable[i] && pollManager.getNodeStatus(id) == NODE_STABLE && pollManager.isWhitelisted(id)) {
                            activeWeights.push_back(slaveWeights[i]);
                            activeIds.push_back(id);
                        }
                    }
                    
                    int availableCnt = activeWeights.size();
                    CombinationResult res = {false, 0.0f, {}};
                    if (availableCnt > 0) {
                        res = engine.findBestCombination(activeWeights);
                    }
                    
                    if (res.success) {
                        xSemaphoreTake(mutexStatus, portMAX_DELAY);
                        systemStatus = SYS_DISCHARGING;
                        lastCombinedWeight = res.totalWeight;
                        currentSelectedMask = 0;
                        std::vector<int> mappedIds;
                        for (int idx_in_active : res.selectedIndices) {
                            int physicalId = activeIds[idx_in_active];
                            mappedIds.push_back(physicalId);
                            currentSelectedMask |= (1 << (physicalId - 1));
                            pollManager.setNodeStatus(physicalId, NODE_LOCKED);
                        }
                        xSemaphoreGive(mutexStatus);

                        Serial.printf("[AUTO] Combination Found: %.1f g, Mask: 0x%08X\n", res.totalWeight, currentSelectedMask);
                        for (int id : mappedIds) rs485.syncWrite(id, 0x0100, 5); // 0x0100=5: 脉冲开门 1S
                        vTaskDelay(pdMS_TO_TICKS(DISCHARGE_SETTLE_MS));

                        for (int id : mappedIds) pollManager.setNodeStatus(id, NODE_DIRTY);

                        xSemaphoreTake(mutexParams, portMAX_DELAY);
                        accumulatedTotalWeight += res.totalWeight;
                        globalCtx.config.accumulatedWeight = accumulatedTotalWeight;
                        xSemaphoreGive(mutexParams);

                        xSemaphoreTake(mutexStatus, portMAX_DELAY);
                        systemStatus = SYS_TRANSFER_B1;
                        currentSelectedMask = 0;
                        xSemaphoreGive(mutexStatus);
                        conveyor.collectFromUnits();
                        vTaskDelay(pdMS_TO_TICKS(BELT_COLLECT_PERIOD_MS));

                        xSemaphoreTake(mutexStatus, portMAX_DELAY);
                        systemStatus = SYS_STEPPING_B2;
                        xSemaphoreGive(mutexStatus);
                        conveyor.advanceOutput();
                        vTaskDelay(pdMS_TO_TICKS(BELT_STEP_PERIOD_MS));

                        xSemaphoreTake(mutexStatus, portMAX_DELAY);
                        systemStatus = SYS_READY;
                        xSemaphoreGive(mutexStatus);
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}

void uiTask(void* pvParameters) {
    Serial.println("[TASK] UI/LVGL Task Started on Core 0");
    while (true) {
        // 同步数据到 UI Context
        xSemaphoreTake(mutexWeights, portMAX_DELAY);
        for(int i=0; i<NUM_SLAVES; i++) {
            globalCtx.state.currentWeights[i] = slaveWeights[i];
            globalCtx.state.stableNodes[i] = slaveStable[i];
        }
        xSemaphoreGive(mutexWeights);

        xSemaphoreTake(mutexStatus, portMAX_DELAY);
        globalCtx.state.lastBatchWeight = lastCombinedWeight;
        globalCtx.state.selectionMask = currentSelectedMask;
        globalCtx.state.status = systemStatus; // 同步内部业务状态
        
        // 扫描进度同步 (数据源切换为 pollManager)
        globalCtx.state.scanProgress = pollManager.getScanProgress();
        globalCtx.state.currentScanCycle = pollManager.getScanCycle();
        
        // 扫描历史记录转换
        for(int c=0; c<5; c++) {
            for(int i=0; i<20; i++) {
                globalCtx.state.scanResults[c][i] = pollManager.getScanHistory(c, i + 1);
            }
        }

        for(int i=0; i<20; i++) {
            globalCtx.state.onlineNodes[i] = pollManager.isOnline(i+1); 
            globalCtx.state.whitelistedNodes[i] = pollManager.isWhitelisted(i+1);
        }
        xSemaphoreGive(mutexStatus);

        ui.updateDashboard(&globalCtx);
        lv_tick_inc(33);
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(33));
    }
}

void cmdGlobalTare() {
    rs485.broadcastWrite(0x0100, 3); // 0x0100=3: 广播去皮指令
}

void cmdStartScan() {
    updateOperationMode(MODE_DIAG_SCAN);
}

void cmdToggleDiagnosis(bool active) {
    if (active) updateOperationMode(MODE_DIAG_PULSE);
    else updateOperationMode(MODE_PRODUCTION);
}

void cmdClearAccumulated() {
    xSemaphoreTake(mutexParams, portMAX_DELAY);
    accumulatedTotalWeight = 0;
    globalCtx.config.accumulatedWeight = 0;
    nvs.begin("production", false);
    nvs.putFloat("accu", 0.0f);
    nvs.end();
    xSemaphoreGive(mutexParams);
}

void cmdUpdateTargets(float d_min, float d_max) {
    xSemaphoreTake(mutexParams, portMAX_DELAY);
    globalCtx.config.targetMin += d_min;
    globalCtx.config.targetMax += d_max;
    if (globalCtx.config.targetMin < 10) globalCtx.config.targetMin = 10;
    if (globalCtx.config.targetMax < globalCtx.config.targetMin) globalCtx.config.targetMax = globalCtx.config.targetMin;
    nvs.begin("production", false);
    nvs.putFloat("tmin", globalCtx.config.targetMin);
    nvs.putFloat("tmax", globalCtx.config.targetMax);
    nvs.end();
    xSemaphoreGive(mutexParams);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[SYSTEM] Starting Waveshare ESP32-S3 Weight Master [Phase 3 Migration]...");

    mutexParams = xSemaphoreCreateMutex();
    mutexWeights = xSemaphoreCreateMutex();
    mutexStatus = xSemaphoreCreateMutex();

    // Default target init
    globalCtx.config.targetMin = 290.0f;
    globalCtx.config.targetMax = 310.0f;
    globalCtx.config.accumulatedWeight = 0.0f;

    // 1. 初始化底层屏幕触控硬件
    if (hw.begin()) {
        hw.lvglInit(); 
        ui.init();     
        Serial.println("[SYSTEM] Hardware & UI Init OK.");
    } else {
        Serial.println("[SYSTEM] CRITICAL: HW Init Failed");
        while(1) delay(100);
    }

    // 2. 初始化 RS485 总线、业务调度中心和外设
    rs485.begin();
    pollManager.begin();
    conveyor.begin();
    systemStatus = SYS_READY;
    
    // 3. 将任务分配到双核
    xTaskCreatePinnedToCore(
        controlTask,
        "ControlTask",
        8192,
        NULL,
        10,
        NULL,
        1  // 核心 1，负责硬件通信和算法业务
    );

    xTaskCreatePinnedToCore(
        uiTask,
        "UITask",
        8192,
        NULL,
        5,
        NULL,
        0  // 核心 0，负责专门刷图和处理点击回调
    );

    Serial.println("[SYSTEM] Dual-Core Mutlitasking Started");
    
    // [DIAGNOSTIC] 启动后直接进入生产模式（Dashboard 界面），不再进行全量自动扫描
    // 用户如需重新探测节点，可在“系统维护”菜单中手动发起
    delay(500); // 等待任务稳定
    updateOperationMode(MODE_PRODUCTION);
}

void loop() {
    // Arduino 默认 loop 留空，业务已交给 FreeRTOS Thread
    vTaskDelay(pdMS_TO_TICKS(1000));
}
