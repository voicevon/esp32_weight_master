#include "OLEDDisplay.h"

void OLEDDisplay::begin() {
    // Already initialized in main.cpp for now, or just ensure display is ready
}

void OLEDDisplay::clear() {
    _oled.clearDisplay();
}

void OLEDDisplay::display() {
    _oled.display();
}

void OLEDDisplay::drawSplash() {
    _oled.clearDisplay();
    _oled.setTextColor(SSD1306_WHITE);
    _oled.setTextSize(1);
    _oled.setCursor(30, 20);
    _oled.print("ANTIGRAVITY");
    _oled.setCursor(25, 40);
    _oled.print("Weight Master");
}

void OLEDDisplay::drawDashboard(const std::vector<float>& weights, float target, float tolerance, const String& status) {
    _oled.setTextColor(SSD1306_WHITE);
    _oled.setTextSize(1);
    _oled.setCursor(0, 0);
    _oled.print("STATUS: ");
    _oled.print(status);

    _oled.setCursor(0, 15);
    _oled.setTextSize(2);
    _oled.printf("%.1fg", target);

    _oled.setTextSize(1);
    _oled.setCursor(85, 18);
    _oled.printf("+/-%.1f", tolerance);
    
    _oled.drawLine(0, 38, 128, 38, SSD1306_WHITE);
    drawBarGraph(weights, target);
}

void OLEDDisplay::drawBarGraph(const std::vector<float>& weights, float target) {
    int marginX = 4;
    int bottomY = 63;
    int maxHeight = 20;
    for (int i = 0; i < (int)weights.size(); i++) {
        float ratio = target > 0 ? (weights[i] / target) : 0;
        float h = ratio * maxHeight;
        if (h > maxHeight) h = maxHeight;
        int x = marginX + (i * 6);
        _oled.fillRect(x, bottomY - (int)h, 4, (int)h, SSD1306_WHITE);
    }
}

void OLEDDisplay::drawMenu(const String& title, const std::vector<String>& items, int cursorIndex, int scrollOffset) {
    _oled.setTextColor(SSD1306_WHITE);
    _oled.setTextSize(1);
    _oled.setCursor(0, 0);
    _oled.print("> ");
    _oled.print(title);
    _oled.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    int maxVisible = 4;
    for (int i = 0; i < maxVisible && (i + scrollOffset) < (int)items.size(); i++) {
        int idx = i + scrollOffset;
        _oled.setCursor(10, 15 + (i * 12));
        if (idx == cursorIndex) _oled.print("> ");
        else _oled.print("  ");
        _oled.print(items[idx]);
    }
}

void OLEDDisplay::drawNodeDetail(int id, float weight, bool online) {
    _oled.setTextColor(SSD1306_WHITE);
    _oled.setTextSize(1);
    _oled.setCursor(0, 0);
    _oled.printf("NODE %d STATUS", id);
    _oled.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    
    _oled.setCursor(10, 25);
    _oled.printf("Weight: %.1fg", weight);
    _oled.setCursor(10, 40);
    _oled.printf("Status: %s", online ? "ONLINE" : "OFFLINE");
    _oled.setCursor(10, 55);
    _oled.print("Press to return");
}

void OLEDDisplay::drawParamEdit(const String& label, float value) {
    _oled.setTextColor(SSD1306_WHITE);
    _oled.setTextSize(1);
    _oled.setCursor(10, 10);
    _oled.print("EDITING: ");
    _oled.print(label);
    _oled.setTextSize(2);
    _oled.setCursor(20, 30);
    _oled.printf("%.1fg", value);
}
