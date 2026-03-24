#ifndef ENCODER_HMI_H
#define ENCODER_HMI_H

#include <AiEsp32RotaryEncoder.h>
#include <Adafruit_SSD1306.h>
#include <vector>

class EncoderHMI {
public:
    EncoderHMI(Adafruit_SSD1306& oled, 
               int clkPin, int dtPin, int swPin, 
               float* targetWeight, float* tolerance);

    void begin();
    void update(const std::vector<float>& weights, const String& status);
    
    // 菜单回调
    bool isButtonPressed();
    void handleEncoder();

private:
    Adafruit_SSD1306& _oled;
    AiEsp32RotaryEncoder _encoder;
    float* _targetWeight;
    float* _tolerance;

    bool _inMenu = false;
    unsigned long _lastUpdate = 0;

    void drawDashboard(const std::vector<float>& weights, const String& status);
    void drawMenu();
    void drawBarGraph(const std::vector<float>& weights);
};

#endif
