#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <Adafruit_SSD1306.h>
#include "Display.h"
#include "system/PinDefinition.h"

class OLEDDisplay : public Display {
private:
    Adafruit_SSD1306& _oled;

public:
    OLEDDisplay(Adafruit_SSD1306& oled) : _oled(oled) {}
    
    void begin() override;
    void clear() override;
    void display() override;
    
    void drawSplash() override;
    void drawDashboard(const std::vector<float>& weights, float target, float tolerance, const String& status) override;
    void drawMenu(const String& title, const std::vector<String>& items, int cursorIndex, int scrollOffset) override;
    void drawNodeDetail(int id, float weight, bool online) override;
    void drawParamEdit(const String& label, float value) override;

private:
    void drawBarGraph(const std::vector<float>& weights, float target);
};

#endif
