#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <vector>
#include "system/ModbusMaster.h"
#include "logic/CombinationEngine.h"
#include "logic/ConveyorController.h"
#include "user_interface/UserInterface.h"
#include "user_interface/OLEDDisplay.h"
#include "system/PinDefinition.h"
#include "system/SystemTypes.h"

// ---------------------------
// 共享资源与同步锁
// ---------------------------
float targetMin = 290.0f;
float targetMax = 310.0f;
std::vector<float> slaveWeights(NUM_SLAVES, 0.0f);
std::vector<bool> slaveStable(NUM_SLAVES, false);
String systemStatus = "INIT";
float lastCombinedWeight = 0.0f;
float accumulatedTotalWeight = 0.0f;
bool isProductionActive = false;

SemaphoreHandle_t mutexParams; // 保护 targetMin/Max 和生产开关
SemaphoreHandle_t mutexWeights; // 保护 slaveWeights
SemaphoreHandle_t mutexStatus;  // 保护 systemStatus

// ---------------------------
// 全局对象
// ---------------------------
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
ModbusMaster rs485(PIN_RS485_RX, PIN_RS485_TX, PIN_RS485_TX_EN, RS485_BAUD);
CombinationEngine engine(targetMin, targetMax);
ConveyorController conveyor(&rs485, MOTOR_ID_BELT1, MOTOR_ID_BELT2);

// ---------------------------
// 任务函数定义
// ---------------------------

// 核心逻辑任务 (Core 1 - 专业控制)
void controlTask(void* pvParameters) {
    static unsigned long lastCalcTime = 0;
    
    while (true) {
        OperationMode mode = UserInterface::getInstance()->getMode();

        // 1. 根据当前模式决定是否推动 Modbus 轮询 (生产/扫描模式可见总线)
        if (mode == MODE_PRODUCTION || mode == MODE_DIAG_SCAN) {
            rs485.update();
        }

        // 2. 更新本地重量缓存 (仅在生产或详情查看时有意义)
        if (mode == MODE_PRODUCTION || mode == MODE_DIAG_DETAIL) {
            xSemaphoreTake(mutexWeights, portMAX_DELAY);
            for (int i = 0; i < NUM_SLAVES; i++) {
                slaveWeights[i] = rs485.getWeight(i + 1);
                slaveStable[i] = rs485.isStable(i + 1);
            }
            xSemaphoreGive(mutexWeights);
        }

        // 3. 生产逻辑处理
        bool canCalculate = false;
        String currentStatus;
        float currentMin, currentMax;

        xSemaphoreTake(mutexParams, portMAX_DELAY);
        canCalculate = isProductionActive;
        currentMin = targetMin;
        currentMax = targetMax;
        xSemaphoreGive(mutexParams);

        xSemaphoreTake(mutexStatus, portMAX_DELAY);
        currentStatus = systemStatus;
        xSemaphoreGive(mutexStatus);

        if (canCalculate && currentStatus == "READY") {
            if (millis() - lastCalcTime > 150) { // 控制计算频率
                lastCalcTime = millis();
                
                engine.setTargetRange(currentMin, currentMax);
                
                // 仅将稳定的重量参与组合计算
                std::vector<float> stableWeightsForEngine(NUM_SLAVES, 0.0f);
                for (int i = 0; i < NUM_SLAVES; i++) {
                    if (slaveStable[i]) {
                        stableWeightsForEngine[i] = slaveWeights[i];
                    } else {
                        stableWeightsForEngine[i] = -10000.0f; // 标记为极其巨大的负数，确保不被选中
                    }
                }
                
                CombinationResult res = engine.findBestCombination(stableWeightsForEngine);
                
                // Live preview: always update lastCombinedWeight if in READY state
                xSemaphoreTake(mutexStatus, portMAX_DELAY);
                lastCombinedWeight = res.success ? res.totalWeight : 0.0f; 
                xSemaphoreGive(mutexStatus);

                if (res.success) {
                    // 进入落料流程
                    xSemaphoreTake(mutexStatus, portMAX_DELAY);
                    systemStatus = "DISCHARGING";
                    accumulatedTotalWeight += res.totalWeight;
                    xSemaphoreGive(mutexStatus);

                    // 1. 发起开门指令
                    for (int id : res.selectedIndices) rs485.openDischarge(id);
                    
                    // 2. 等待所有选中的从机反馈“开门结束” (Wait for DoorPhase == 3)
                    unsigned long dischargeStart = millis();
                    bool allDone = false;
                    while (!allDone && (millis() - dischargeStart < 3000)) { // 3s 超时保护
                        allDone = true;
                        rs485.update(); // 持续推动 Modbus 轮询最新状态
                        for (int id : res.selectedIndices) {
                            if (rs485.getDoorPhase(id) != 3) {
                                allDone = false;
                                break;
                            }
                        }
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }

                    // 3. 执行关门与去皮
                    for (int id : res.selectedIndices) {
                        rs485.closeDischarge(id);
                        rs485.tare(id);
                    }

                    // 级联传输
                    xSemaphoreTake(mutexStatus, portMAX_DELAY);
                    systemStatus = "TRANSFER-B1";
                    xSemaphoreGive(mutexStatus);
                    conveyor.collectFromUnits();
                    vTaskDelay(pdMS_TO_TICKS(2500));

                    xSemaphoreTake(mutexStatus, portMAX_DELAY);
                    systemStatus = "STEPPING-B2";
                    xSemaphoreGive(mutexStatus);
                    conveyor.advanceOutput();
                    vTaskDelay(pdMS_TO_TICKS(1200));

                    xSemaphoreTake(mutexStatus, portMAX_DELAY);
                    systemStatus = "READY";
                    xSemaphoreGive(mutexStatus);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1)); // 给协议栈留出微量 breathing room
    }
}

// HMI 界面任务 (Core 0 - 渲染与交互)
void uiTask(void* pvParameters) {
    while (true) {
        // 1. 获取并上报参数修改
        xSemaphoreTake(mutexParams, portMAX_DELAY);
        isProductionActive = (UserInterface::getInstance()->getMode() == MODE_PRODUCTION);
        // 如果 UI 修改了 targetMin/Max，这里会被同步（UserInterface 持有指针）
        xSemaphoreGive(mutexParams);

        // 2. 刷新显示 (核心 0 专属 IO 操作)
        std::vector<float> localWeights(NUM_SLAVES);
        String localStatus;
        float stableSum = 0.0f;
        float unstableSum = 0.0f;
        
        xSemaphoreTake(mutexWeights, portMAX_DELAY);
        localWeights = slaveWeights;
        for (int i = 0; i < NUM_SLAVES; i++) {
            if (slaveStable[i]) {
                stableSum += slaveWeights[i];
            } else {
                unstableSum += slaveWeights[i];
            }
        }
        xSemaphoreGive(mutexWeights);

        xSemaphoreTake(mutexStatus, portMAX_DELAY);
        localStatus = systemStatus;
        // 如果处于 READY 状态，显示所有稳定斗的实时总和；否则显示上一批次的组合重量
        float displayStable = (localStatus == "READY") ? stableSum : lastCombinedWeight;
        float totalSum = stableSum + unstableSum;
        float localAccumulatedWeight = accumulatedTotalWeight;
        xSemaphoreGive(mutexStatus);

        UserInterface::getInstance()->update(localWeights, displayStable, unstableSum, totalSum, localAccumulatedWeight, localStatus);

        vTaskDelay(pdMS_TO_TICKS(33)); // 约 30FPS 刷新率
    }
}

// ---------------------------
// 主入口
// ---------------------------
void setup() {
    Serial.begin(115200);
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    // 初始化锁
    mutexParams = xSemaphoreCreateMutex();
    mutexWeights = xSemaphoreCreateMutex();
    mutexStatus = xSemaphoreCreateMutex();

    // 初始化 HMI (Core 0 操作)
    display.setI2CAddress(0x3C * 2); // U8g2 expects 8-bit address or use setI2CAddress
    // Alternatively, just call begin() if 0x3C is default. 
    // Actually U8g2 begin() handles it.
    if(!display.begin()) {
        Serial.println(F("U8g2 initialization failed"));
    }
    display.enableUTF8Print(); // Enable UTF8 for Chinese support
    UserInterface::getInstance()->initialize(&targetMin, &targetMax, &rs485);
    UserInterface::getInstance()->addDisplay(new OLEDDisplay(display));

    // 初始化硬件
    rs485.begin();
    conveyor.begin();
    systemStatus = "READY";

    // 创建多任务 (关键分配)
    xTaskCreatePinnedToCore(
        controlTask,    // 任务函数
        "ControlTask",  // 名称
        8192,           // 栈空间
        NULL,           // 参数
        10,             // 优先级 (高)
        NULL,           // 句柄
        1               // 绑定到核心 1 (主控/通讯)
    );

    xTaskCreatePinnedToCore(
        uiTask,
        "UITask",
        4096,
        NULL,
        5,              // 优先级 (中)
        NULL,
        0               // 绑定到核心 0 (HMI/Rendering)
    );

    Serial.println("Dual-Core Multitasking Started.");
}

void loop() {
    // Arduino Loop 留空或处理最低优先级心跳
    vTaskDelay(pdMS_TO_TICKS(1000));
}
