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

// 初始化 ModbusMaster, RS485_RX, TX, TX_EN(-1), BAUD
ModbusMaster rs485(PIN_RS485_RX, PIN_RS485_TX, PIN_RS485_TX_EN, RS485_BAUD);

// 初始化其他业务对象
CombinationEngine engine(290.0f, 310.0f); 
ConveyorController conveyor(&rs485, MOTOR_ID_BELT1, MOTOR_ID_BELT2);

// --- Task Definitions ---
void controlTask(void* pvParameters) {
    Serial.println("[TASK] Control Task Started on Core 1");
    static unsigned long lastDiagPulseTime = 0;
    static unsigned long lastCalcTime = 0;
    
    while (true) {
        // --- 485 诊断模式处理 (优先级最高，且与标准业务互斥) ---
        bool isDiagActive = false;
        xSemaphoreTake(mutexStatus, portMAX_DELAY);
        isDiagActive = globalCtx.state.isDiagPulseActive;
        xSemaphoreGive(mutexStatus);

        if (isDiagActive) {
            // 1. 定时发送递增脉冲 (1Hz)
            if (millis() - lastDiagPulseTime >= 1000) {
                lastDiagPulseTime = millis();
                
                xSemaphoreTake(mutexStatus, portMAX_DELAY);
                globalCtx.state.diagLastSent++;
                uint8_t toSend = globalCtx.state.diagLastSent;
                xSemaphoreGive(mutexStatus);
                
                rs485.sendRawByte(toSend);
            }

            // 2. 实时读取并格式化接收数据 (HEX 格式)
            if (rs485.availableRaw() > 0) {
                xSemaphoreTake(mutexStatus, portMAX_DELAY);
                
                // 简单的循环缓冲区逻辑：保持显示最近的数据
                while (rs485.availableRaw() > 0) {
                    uint8_t b = rs485.readRawByte();
                    char hexBuf[8];
                    snprintf(hexBuf, sizeof(hexBuf), "%02X ", b);
                    
                    // 如果缓冲区快满了，先清空一部分或全部 (此处简单处理：满则清空)
                    if (strlen(globalCtx.state.diagRxHex) > 100) {
                        memset(globalCtx.state.diagRxHex, 0, sizeof(globalCtx.state.diagRxHex));
                    }
                    strncat(globalCtx.state.diagRxHex, hexBuf, sizeof(globalCtx.state.diagRxHex) - strlen(globalCtx.state.diagRxHex) - 1);
                }
                
                xSemaphoreGive(mutexStatus);
            }

            vTaskDelay(pdMS_TO_TICKS(10)); // 诊断模式下的轻量等待
            continue; // 跳过下方正常业务逻辑
        }

        // --- 正常生产逻辑 ---
        // 核心心跳：推动 ModbusMaster 轮询过程
        rs485.update();
        
        // 更新本地重量缓存
        xSemaphoreTake(mutexWeights, portMAX_DELAY);
        for (int i = 0; i < NUM_SLAVES; i++) {
            slaveWeights[i] = rs485.getWeight(i + 1);
            slaveStable[i] = rs485.isStable(i + 1);
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

        xSemaphoreTake(mutexStatus, portMAX_DELAY);
        SystemStatus currentStatus = systemStatus;
        xSemaphoreGive(mutexStatus);

        if (canCalculate && currentStatus == SYS_READY) {
            if (millis() - lastCalcTime > CALC_ENGINE_INTERVAL_MS) { 
                lastCalcTime = millis();
                
                engine.setTargetRange(currentMin, currentMax);
                
                // --- 动态映射方案：仅将活跃稳定的节点送入引擎 ---
                std::vector<float> activeWeights;
                std::vector<int> activeIds;
                
                for (int i = 0; i < NUM_SLAVES; i++) {
                    int id = i + 1;
                    if (slaveStable[i] && rs485.getNodeStatus(id) == NODE_STABLE && rs485.isWhitelisted(id)) {
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
                        int physicalId = activeIds[idx_in_active - 1];
                        mappedIds.push_back(physicalId);
                        currentSelectedMask |= (1 << (physicalId - 1));
                        rs485.setNodeStatus(physicalId, NODE_LOCKED);
                    }
                    xSemaphoreGive(mutexStatus);

                    Serial.printf("[AUTO] Combination Found: %.1f g, Mask: 0x%08X (Avail: %d)\n", 
                                  res.totalWeight, currentSelectedMask, availableCnt);
                    
                    for (int id : mappedIds) {
                        rs485.openDischarge1S(id); 
                    }
                    
                    vTaskDelay(pdMS_TO_TICKS(DISCHARGE_SETTLE_MS));

                    for (int id : mappedIds) {
                        rs485.setNodeStatus(id, NODE_DIRTY);
                    }

                    xSemaphoreTake(mutexParams, portMAX_DELAY);
                    accumulatedTotalWeight += res.totalWeight;
                    globalCtx.config.accumulatedWeight = accumulatedTotalWeight;
                    xSemaphoreGive(mutexParams);

                    xSemaphoreTake(mutexStatus, portMAX_DELAY);
                    currentSelectedMask = 0;
                    xSemaphoreGive(mutexStatus);

                    // 级联传输
                    xSemaphoreTake(mutexStatus, portMAX_DELAY);
                    systemStatus = SYS_TRANSFER_B1;
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
                } else {
                    static uint32_t lastEngineLog = 0;
                    if (millis() - lastEngineLog > 2000) {
                        Serial.printf("[ENGINE] Standby... Avail Nodes: %d, Target: [%.1f-%.1f]\n", 
                                      availableCnt, currentMin, currentMax);
                        lastEngineLog = millis();
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
        // 1. 同步数据字典到全局 UI Context
        xSemaphoreTake(mutexWeights, portMAX_DELAY);
        for(int i=0; i<NUM_SLAVES; i++) {
            globalCtx.state.currentWeights[i] = slaveWeights[i];
            globalCtx.state.stableNodes[i] = slaveStable[i];
        }
        xSemaphoreGive(mutexWeights);

        xSemaphoreTake(mutexStatus, portMAX_DELAY);
        globalCtx.state.status = systemStatus;
        globalCtx.state.lastBatchWeight = lastCombinedWeight;
        globalCtx.state.selectionMask = currentSelectedMask;
        globalCtx.state.isScanning = rs485.isScanning();
        globalCtx.state.scanProgress = rs485.getScanProgress();
        globalCtx.state.currentScanCycle = rs485.getCurrentScanCycle();
        
        const bool (*history)[21] = rs485.getScanHistory();
        for(int c=0; c<5; c++) {
            for(int i=0; i<20; i++) {
                globalCtx.state.scanResults[c][i] = history[c][i+1];
            }
        }

        const bool* online = rs485.getOnlineStatusArray();
        for(int i=0; i<20; i++) {
            globalCtx.state.onlineNodes[i] = online[i+1]; 
        }
        xSemaphoreGive(mutexStatus);

        ui.updateDashboard(&globalCtx);

        // 2. LVGL 必须定时唤醒
        lv_tick_inc(33);
        lv_timer_handler();

        vTaskDelay(pdMS_TO_TICKS(33));
    }
}

void cmdGlobalTare() {
    rs485.broadcastTare();
}

void cmdStartScan() {
    rs485.startScan();
}

void cmdGenerateWhitelist() {
    rs485.generateWhitelistFromScan();
}

void cmdToggleDiagnosis(bool active) {
    xSemaphoreTake(mutexStatus, portMAX_DELAY);
    globalCtx.state.isDiagPulseActive = active;
    if (active) {
        // 开启诊断时，清空旧数据
        globalCtx.state.diagLastSent = 0;
        memset(globalCtx.state.diagRxHex, 0, sizeof(globalCtx.state.diagRxHex));
        rs485.clearRawBuffer();
    }
    xSemaphoreGive(mutexStatus);
    
    if (active) {
        Serial.println("[CMD] 485 Diagnosis Mode ACTIVATED.");
    } else {
        Serial.println("[CMD] 485 Diagnosis Mode DEACTIVATED.");
    }
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

    // 2. 初始化 RS485 总线和外设
    rs485.begin();
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
}

void loop() {
    // Arduino 默认 loop 留空，业务已交给 FreeRTOS Thread
    vTaskDelay(pdMS_TO_TICKS(1000));
}
