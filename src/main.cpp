#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <vector>
#include "Rs485Master.h"
#include "CombinationEngine.h"
#include "ConveyorController.h"
#include "EncoderHMI.h"

// 硬件引脚
#define I2C_SDA 21
#define I2C_SCL 22
#define RS485_RX 16
#define RS485_TX 17
#define RS485_EN 4

// 编码器引脚 (来自 hardware_layout.md)
#define ENC_CLK 32
#define ENC_DT  33
#define ENC_SW  25

// 电机 ID 分配
#define MOTOR_ID_BELT1 21
#define MOTOR_ID_BELT2 22

// 初始参数 (可通过编码器调节)
#define NUM_SLAVES 20
float targetWeight = 300.0f;
float tolerance = 5.0f;

// 全局对象
Adafruit_SSD1306 display(128, 64, &Wire, -1);
Rs485Master rs485(RS485_RX, RS485_TX, RS485_EN, 115200);
CombinationEngine engine(targetWeight, tolerance);
ConveyorController conveyor(&rs485, MOTOR_ID_BELT1, MOTOR_ID_BELT2);
EncoderHMI hmi(display, ENC_CLK, ENC_DT, ENC_SW, &targetWeight, &tolerance);

std::vector<float> slaveWeights(NUM_SLAVES, 0.0f);
String systemStatus = "READY";

void setup() {
    Serial.begin(115200);
    Wire.begin(I2C_SDA, I2C_SCL);
    
    // 初始化 HMI 和通信
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
    }
    
    hmi.begin();
    rs485.begin();
    conveyor.begin();
    
    systemStatus = "READY";
}

void loop() {
    // 1. 推动异步 Modbus 协议栈和交互任务
    rs485.update();
    hmi.handleEncoder();
    
    // 2. 定时同步缓存数据到本地变量 (用于计算和显示)
    // 这里的 getWeight 现在是零延迟的内存读取
    for (int i = 0; i < NUM_SLAVES; i++) {
        slaveWeights[i] = rs485.getWeight(i + 1);
    }
    
    // 3. 始终保持 HMI 刷新 (由于是非阻塞的，这里非常丝滑)
    hmi.update(slaveWeights, systemStatus);
    
    // 4. 定义分拣引擎调度 (每 100ms 尝试一次新的组合寻找)
    static unsigned long lastCalcTime = 0;
    if (millis() - lastCalcTime > 100 && systemStatus == "READY") {
        lastCalcTime = millis();
        
        engine.setTargetWeight(targetWeight); 
        CombinationResult res = engine.findBestCombination(slaveWeights);
        
        if (res.success) {
            systemStatus = "DISCHARGING";
            hmi.update(slaveWeights, systemStatus);
            
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
            hmi.update(slaveWeights, systemStatus);
            conveyor.collectFromUnits();
            delay(2500); 
            
            systemStatus = "STEPPING-B2";
            hmi.update(slaveWeights, systemStatus);
            conveyor.advanceOutput();
            delay(1200); 
            
            systemStatus = "READY";
        }
    }
}
