#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>
#include "system/PinDefinition.h"

/**
 * @brief 编码器驱动类 (Sorter 模式)
 * 采用原生 ISR 中断解码，彻底消除 HMI 迟滞
 */
class Encoder {
private:
    static Encoder* instance;
    Encoder();

    // 核心状态变量
    volatile int encoderTotalSteps = 0;
    volatile int encoderState = 0;
    int lastConsumedTotalSteps = 0;

    // 按钮状态变量
    volatile bool buttonClickFlag = false;
    volatile bool buttonLongPressFlag = false;
    volatile bool buttonDownState = false;
    volatile unsigned long lastButtonDebounceTime = 0;
    volatile unsigned long buttonPressStartTime = 0;

    static const unsigned long DEBOUNCE_DELAY = 50;   // 50ms 消抖
    static const unsigned long LONG_PRESS_DELAY = 1000; // 1s 长按

public:
    static Encoder* getInstance();
    
    /**
     * @brief 初始化 GPIO 与外部中断
     */
    void initialize();
    
    /**
     * @brief 获取逻辑步数增量 (4:1 分频，对应物理咔哒声)
     * @return 增量值 (-1, 0, 1)
     */
    int getDelta();
    
    /**
     * @brief 检查是否发生短按
     */
    bool isClicked();
    
    /**
     * @brief 检查是否发生长按
     */
    bool isLongPressed();

    /**
     * @brief 是否正在按下
     */
    bool isPressed() const { return buttonDownState; }

    // 中断处理函数 (静态)
    static void IRAM_ATTR encoderISR();
    static void IRAM_ATTR buttonISR();
};

#endif
