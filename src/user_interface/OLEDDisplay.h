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
    void drawDashboard(const std::vector<float>& weights, float stableSum, float unstableSum, int unstableCount, float totalSum, float accumulatedWeight, uint32_t whitelistMask, float target, float tolerance, SystemStatus status, uint32_t selectionMask = 0) override;
    void drawMenu(const String& title, const std::vector<String>& items, int cursorIndex, int scrollOffset) override;
    void drawNodeDetail(int id, float weight, bool online) override;
    void drawParamEdit(const String& label, float value, float refValue, bool isMin) override;
    void drawRs485Diag(uint32_t txPulse, uint8_t rxByte, uint32_t rxCount) override;
    void drawScan(int currentId, bool finished, const std::vector<ScanRow>& history, float scrollY, uint32_t scanCount) override;
    void drawAbout(const String& version, const String& buildDate) override;
    void drawMessage(const String& msg) override;
    void drawSequentialProgress(const String& label, int current, int total) override;

private:
    void drawBarGraph(const std::vector<float>& weights, float target, uint32_t selectionMask, uint32_t whitelistMask);
    
    // UI 组件化辅助方法
    void drawStatusBar(SystemStatus status, float totalSum, float accumulatedWeight);
    void drawWeightDisplay(float stableSum, float unstableSum, int unstableCount);
    void drawProductionSummary(float accumulatedWeight);
};

#endif
