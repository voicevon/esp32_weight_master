#include "EncoderHMI.h"
#include <Arduino.h>

EncoderHMI::EncoderHMI(Adafruit_SSD1306& oled, 
                       int clkPin, int dtPin, int swPin, 
                       float* targetWeight, float* tolerance)
    : _oled(oled), _encoder(clkPin, dtPin, swPin, -1, 4), // 4: Acceleration
      _targetWeight(targetWeight), _tolerance(tolerance) {}

void EncoderHMI::begin() {
    _encoder.begin();
    _encoder.setup([]() {
        // ISR Handle - can be refined in main.cpp if needed for static mapping
    });
    _encoder.setBoundaries(0, 10000, false); // Target Weight Range: 0 - 1000g (in 0.1g steps)
    _encoder.setEncoderValue((long)(*_targetWeight * 10)); // Scale float to int (300.0g -> 3000)
}

void EncoderHMI::update(const std::vector<float>& weights, const String& status) {
    if (millis() - _lastUpdate < 100) return; // Limit refresh rate to 10Hz
    _lastUpdate = millis();

    _oled.clearDisplay();
    _oled.setTextColor(SSD1306_WHITE);

    // 核心显示区域 (0, 0, 128, 40)
    drawDashboard(weights, status);
    
    // 柱状图区域 (0, 42, 128, 64)
    drawBarGraph(weights);
    
    _oled.display();
}

void EncoderHMI::drawDashboard(const std::vector<float>& weights, const String& status) {
    _oled.setTextSize(1);
    _oled.setCursor(0, 0);
    _oled.print("STATUS: ");
    _oled.print(status);

    _oled.setCursor(0, 15);
    _oled.setTextSize(2);
    _oled.printf("%.1fg", *_targetWeight);

    _oled.setTextSize(1);
    _oled.setCursor(85, 18);
    _oled.printf("+/-%.1fg", *_tolerance);
    
    _oled.drawLine(0, 38, 128, 38, SSD1306_WHITE);
}

void EncoderHMI::drawBarGraph(const std::vector<float>& weights) {
    // 20 个单元，每个宽约 5 像素，间距 1 像素，总宽 120 像素
    int marginX = 4;
    int bottomY = 63;
    int maxHeight = 20;

    for (int i = 0; i < weights.size(); i++) {
        // 将重量映射为 0 - 20 像素的高度
        float h = (weights[i] / (*_targetWeight)) * maxHeight;
        if (h > maxHeight) h = maxHeight;
        
        int x = marginX + (i * 6);
        int barHeight = (int)h;
        
        // 绘制柱子
        _oled.fillRect(x, bottomY - barHeight, 4, barHeight, SSD1306_WHITE);
    }
}

void EncoderHMI::handleEncoder() {
    // 菜单级联：如果按下按钮，切换进入参数设置
    if (_encoder.isEncoderButtonClicked()) {
        _inMenu = !_inMenu;
    }

    if (_inMenu) {
        long rawVal = _encoder.readEncoder();
        *_targetWeight = (float)rawVal / 10.0f;
    }
}
