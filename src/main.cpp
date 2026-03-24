#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <vector>
#include "system/Rs485Master.h"
#include "logic/CombinationEngine.h"
#include "logic/ConveyorController.h"
#include "user_interface/UserInterface.h"
#include "user_interface/OLEDDisplay.h"
#include "system/PinDefinition.h"

// 初始参数 (可通过编码器调节)
float targetWeight = 300.0f;
float tolerance = 5.0f;

// 全局对象
Adafruit_SSD1306 display(128, 64, &Wire, -1);
Rs485Master rs485(RS485_RX, RS485_TX, RS485_EN, RS485_BAUD);
CombinationEngine engine(targetWeight, tolerance);
ConveyorController conveyor(&rs485, MOTOR_ID_BELT1, MOTOR_ID_BELT2);

std::vector<float> slaveWeights(NUM_SLAVES, 0.0f);
String systemStatus = "READY";

void setup() {
    Serial.begin(115200);
    Wire.begin(I2C_SDA, I2C_SCL);
    
    // 初始化 HMI (模块化架构)
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
    }
    UserInterface::getInstance()->initialize(&targetWeight, &tolerance, &rs485);
    UserInterface::getInstance()->addDisplay(new OLEDDisplay(display));

    rs485.begin();
    conveyor.begin();
    
    systemStatus = "READY";
}

void loop() {
    // 1. 推动异步 Modbus 协议栈和交互任务
    rs485.update();
    
    // 2. 定时同步缓存数据到本地变量 (用于计算和显示)
    // 这里的 getWeight 现在是零延迟的内存读取
    for (int i = 0; i < NUM_SLAVES; i++) {
        slaveWeights[i] = rs485.getWeight(i + 1);
    }
    
    // 3. 始终保持 UI 逻辑更新 (包含输入处理与显示刷新)
    UserInterface::getInstance()->update(slaveWeights, systemStatus);
    
    // 4. 定义分拣引擎调度 (每 100ms 尝试一次新的组合寻找)
    static unsigned long lastCalcTime = 0;
    bool isProduction = (UserInterface::getInstance()->getMode() == MODE_PRODUCTION);
    if (millis() - lastCalcTime > 100 && systemStatus == "READY" && isProduction) {
        lastCalcTime = millis();
        
        engine.setTargetWeight(targetWeight); 
        CombinationResult res = engine.findBestCombination(slaveWeights);
        
        if (res.success) {
            systemStatus = "DISCHARGING";
            UserInterface::getInstance()->update(slaveWeights, systemStatus);
            
            // 下发落料指令 (同步指令会暂时占用总线，这是合理的)
            for (int id : res.selectedIndices) {
                rs485.openDischarge(id);
            }
            
            delay(800); 
            
            for (int id : res.selectedIndices) {
                rs485.closeDischarge(id);
                rs485.tare(id);
            }
            
            // 级联传输
            systemStatus = "TRANSFER-B1";
            UserInterface::getInstance()->update(slaveWeights, systemStatus);
            conveyor.collectFromUnits();
            delay(2500); 
            
            systemStatus = "STEPPING-B2";
            UserInterface::getInstance()->update(slaveWeights, systemStatus);
            conveyor.advanceOutput();
            delay(1200); 
            
            systemStatus = "READY";
        }
    }
}
