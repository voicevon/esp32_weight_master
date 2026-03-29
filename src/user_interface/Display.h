#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <vector>

struct ScanRow {
    bool online[21]; // Indices 1-20
};

/**
 * @brief 显示输出抽象基类
 */
class Display {
public:
    virtual ~Display() {}
    
    virtual void begin() = 0;
    virtual void clear() = 0;
    virtual void display() = 0;
    
    // 高层显示方法
    virtual void drawSplash(uint32_t elapsedMillis) = 0;
    virtual void drawDashboard(const std::vector<float>& weights, float target, float tolerance, const String& status) = 0;
    virtual void drawMenu(const String& title, const std::vector<String>& items, int cursorIndex, int scrollOffset) = 0;
    virtual void drawNodeDetail(int id, float weight, bool online) = 0;
    virtual void drawParamEdit(const String& label, float value, float refValue, bool isMin) = 0;
    virtual void drawRs485Diag(uint32_t txPulse, uint8_t rxByte, uint32_t rxCount) = 0;
    virtual void drawScan(int currentId, bool finished, const std::vector<ScanRow>& history, float scrollY, uint32_t scanCount) = 0;
    virtual void drawAbout(const String& version, const String& buildDate) = 0;
};

#endif
