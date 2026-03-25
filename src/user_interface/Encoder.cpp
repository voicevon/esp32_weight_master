#include "Encoder.h"

Encoder* Encoder::instance = nullptr;

Encoder::Encoder() {}

Encoder* Encoder::getInstance() {
    if (instance == nullptr) {
        instance = new Encoder();
    }
    return instance;
}

void Encoder::initialize() {
    // 配置引脚为上拉输入
    pinMode(PIN_ENCODER_A, INPUT_PULLUP);
    pinMode(PIN_ENCODER_B, INPUT_PULLUP);
    pinMode(PIN_ENCODER_BUTTON, INPUT_PULLUP);

    // 初始化初始状态
    encoderState = (digitalRead(PIN_ENCODER_A) << 1) | digitalRead(PIN_ENCODER_B);
    buttonDownState = (digitalRead(PIN_ENCODER_BUTTON) == LOW);

    // 注册外部中断
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_A), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_B), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_BUTTON), buttonISR, CHANGE);
}

void IRAM_ATTR Encoder::encoderISR() {
    Encoder* enc = Encoder::instance;
    if (!enc) return;

    // 快速读取 GPIO 电平 (ESP32 gpio_get_level 的 Arduino 封装通常足够快)
    int s = (digitalRead(PIN_ENCODER_A) << 1) | digitalRead(PIN_ENCODER_B);
    
    if (s != enc->encoderState) {
        // 经典正交解码状态表 (-1, 0, 1)
        // 索引格式: [old_state_2bit | new_state_2bit]
        static const int8_t trans[] = {
            0, -1,  1,  2, 
            1,  0,  2, -1, 
           -1,  2,  0,  1, 
            2,  1, -1,  0  
        };
        
        int full_state = (enc->encoderState << 2) | (s & 0x03);
        int8_t step = trans[full_state & 0x0F];
        
        // 过滤掉非法跳转 (2) 和无位移 (0)
        if (step != 2 && step != 0) {
            enc->encoderTotalSteps += step;
        }
        
        enc->encoderState = s;
    }
}

void IRAM_ATTR Encoder::buttonISR() {
    Encoder* enc = Encoder::instance;
    if (!enc) return;

    unsigned long currentTime = millis();
    bool currentState = (digitalRead(PIN_ENCODER_BUTTON) == LOW);

    // 简单的软件消抖逻辑
    if (currentTime - enc->lastButtonDebounceTime > DEBOUNCE_DELAY) {
        if (currentState != enc->buttonDownState) {
            bool wasPressed = enc->buttonDownState;
            enc->buttonDownState = currentState;
            enc->lastButtonDebounceTime = currentTime;

            if (currentState) {
                // 记录按下时刻
                enc->buttonPressStartTime = currentTime;
            } else if (wasPressed) {
                // 在抬起时计算持续时间
                unsigned long duration = currentTime - enc->buttonPressStartTime;
                if (duration >= LONG_PRESS_DELAY) {
                    enc->buttonLongPressFlag = true;
                } else {
                    enc->buttonClickFlag = true;
                }
            }
        }
    }
}

int Encoder::getDelta() {
    // 读取当前总步数 (ISR 实时更新)
    int current = encoderTotalSteps;
    int rawDiff = current - lastConsumedTotalSteps;
    int delta = 0;
    
    // 应用 4:1 分频。绝大多数 HMI 旋转编码器一个物理“咔哒”点对应 4 个脉冲。
    if (abs(rawDiff) >= 4) {
        delta = rawDiff / 4;
        // 更新最近消耗的总步数（按 4 的倍数增加）
        lastConsumedTotalSteps += delta * 4;
    }
    
    return delta;
}

bool Encoder::isClicked() {
    bool res = buttonClickFlag;
    if (res) buttonClickFlag = false;
    return res;
}

bool Encoder::isLongPressed() {
    bool res = buttonLongPressFlag;
    if (res) buttonLongPressFlag = false;
    return res;
}
