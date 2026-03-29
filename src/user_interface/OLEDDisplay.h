#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <U8g2lib.h>
#include "Display.h"
#include "system/PinDefinition.h"

class OLEDDisplay : public Display {
private:
    U8G2& _oled;

public:
    OLEDDisplay(U8G2& oled) : _oled(oled) {}
    
    void begin() override;
    void clear() override;
    void display() override;
    
    void drawSplash(uint32_t elapsedMillis) override;
    void drawDashboard(const std::vector<float>& weights, float target, float tolerance, const String& status) override;
    void drawMenu(const String& title, const std::vector<String>& items, int cursorIndex, int scrollOffset) override;
    void drawNodeDetail(int id, float weight, bool online) override;
    void drawParamEdit(const String& label, float value, float refValue, bool isMin) override;
    void drawRs485Diag(uint32_t txPulse, uint8_t rxByte, uint32_t rxCount) override;
    void drawScan(int currentId, bool finished, const std::vector<ScanRow>& history, float scrollY, uint32_t scanCount) override;
    void drawAbout(const String& version, const String& buildDate) override;

private:
    void drawBarGraph(const std::vector<float>& weights, float target);
};

#endif
