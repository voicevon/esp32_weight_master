#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
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
String systemStatus = "INIT";
bool isProductionActive = false;

SemaphoreHandle_t mutexParams; // 保护 targetMin/Max 和生产开关
SemaphoreHandle_t mutexWeights; // 保护 slaveWeights
SemaphoreHandle_t mutexStatus;  // 保护 systemStatus

// ---------------------------
// 全局对象
// ---------------------------
Adafruit_SSD1306 display(128, 64, &Wire, -1);
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
                CombinationResult res = engine.findBestCombination(slaveWeights);
                
                if (res.success) {
                    // 进入落料流程
                    xSemaphoreTake(mutexStatus, portMAX_DELAY);
                    systemStatus = "DISCHARGING";
                    xSemaphoreGive(mutexStatus);

                    // 同步执行硬件动作 (此时 Modbus 被主控逻辑独占)
                    for (int id : res.selectedIndices) rs485.openDischarge(id);
                    vTaskDelay(pdMS_TO_TICKS(800));
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
        
        xSemaphoreTake(mutexWeights, portMAX_DELAY);
        localWeights = slaveWeights;
        xSemaphoreGive(mutexWeights);

        xSemaphoreTake(mutexStatus, portMAX_DELAY);
        localStatus = systemStatus;
        xSemaphoreGive(mutexStatus);

        UserInterface::getInstance()->update(localWeights, localStatus);

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
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
    }
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
