#include "OLEDDisplay.h"
#include "splash_image.h"

void OLEDDisplay::begin() {
    // Note: _oled.begin() is already called in main.cpp
    _oled.enableUTF8Print(); // 启用 UTF8 支持以显示中文
}

void OLEDDisplay::clear() {
    _oled.clearBuffer();
}

void OLEDDisplay::display() {
    _oled.sendBuffer();
}

void OLEDDisplay::drawSplash(uint32_t elapsedMillis) {
    _oled.clearBuffer();
    
    // 1. Digital Rain / Matrix Effect (0-2000ms)
    // We use a simple pseudo-random generator based on elapsedMillis to draw "drops"
    if (elapsedMillis < 2000) {
        for (int i = 0; i < 15; i++) {
            // Deterministic "random" positions based on drop index
            int x = (i * 137) % 128;
            int speed = 2 + (i % 3);
            int yStart = (elapsedMillis * speed / 10) % 100 - 20;
            int length = 5 + (i % 5);
            
            _oled.drawVLine(x, yStart, length);
            // Add a "tail" pixel for a slightly cooler look
            if (yStart > 0) _oled.drawPixel(x, yStart - 1);
        }
    }

    // 2. Sequenced Text Animation
    int y1 = 64; // Starting position (off-screen bottom)
    int y2 = 64;
    
    const int targetY1 = 20;
    const int targetY2 = 45;
    
    // Line 1 sliding (400-1200ms)
    if (elapsedMillis < 400) {
        y1 = 64;
    } else if (elapsedMillis < 1200) {
        float progress = (float)(elapsedMillis - 400) / 800.0f;
        y1 = 64 - (int)(progress * (64 - targetY1));
    } else {
        y1 = targetY1;
    }
    
    // Line 2 sliding (1000-1800ms)
    if (elapsedMillis < 1000) {
        y2 = 64;
    } else if (elapsedMillis < 1800) {
        float progress = (float)(elapsedMillis - 1000) / 800.0f;
        y2 = 64 - (int)(progress * (64 - targetY2));
    } else {
        y2 = targetY2;
    }

    // Draw high-res bitmasks
    if (elapsedMillis > 400) {
        _oled.drawXBMP(0, y1 - 12, 128, 24, splash_line1);
    }
    if (elapsedMillis > 1000) {
        _oled.drawXBMP(0, y2 - 12, 128, 24, splash_line2);
    }
}

void OLEDDisplay::drawDashboard(const std::vector<float>& weights, float target, float tolerance, const String& status) {
    _oled.setFont(u8g2_font_wqy12_t_gb2312b);
    _oled.setCursor(0, 10);
    _oled.print("状态: ");
    _oled.print(status);

    _oled.setFont(u8g2_font_logisoso18_tn);
    _oled.setCursor(0, 34);
    _oled.printf("%.1f", target);
    _oled.setFont(u8g2_font_wqy12_t_gb2312b);
    _oled.print("g");

    _oled.setCursor(85, 30);
    _oled.printf("+/-%.1f", tolerance);
    
    _oled.drawFrame(0, 38, 128, 1); // 模拟原有的 line
    drawBarGraph(weights, target);
}

void OLEDDisplay::drawBarGraph(const std::vector<float>& weights, float target) {
    int marginX = 4;
    int bottomY = 63;
    int maxHeight = 20;
    for (int i = 0; i < (int)weights.size(); i++) {
        float ratio = target > 0 ? (weights[i] / target) : 0;
        int h = (int)(ratio * maxHeight);
        if (h > maxHeight) h = maxHeight;
        int x = marginX + (i * 6);
        _oled.drawBox(x, bottomY - h, 4, h);
    }
}

void OLEDDisplay::drawMenu(const String& title, const std::vector<String>& items, int cursorIndex, int scrollOffset) {
    _oled.setFont(u8g2_font_wqy12_t_gb2312b);
    _oled.setCursor(0, 10);
    _oled.print("> ");
    _oled.print(title);
    _oled.drawHLine(0, 12, 128);

    int maxVisible = 4;
    for (int i = 0; i < maxVisible && (i + scrollOffset) < (int)items.size(); i++) {
        int idx = i + scrollOffset;
        int y = 26 + (i * 12);
        _oled.setCursor(0, y);
        if (idx == cursorIndex) _oled.print("> ");
        else _oled.print("  ");
        _oled.print(items[idx]);
    }
}

void OLEDDisplay::drawNodeDetail(int id, float weight, bool online) {
    _oled.setFont(u8g2_font_wqy12_t_gb2312b);
    _oled.setCursor(0, 10);
    _oled.printf("节点 %d 状态", id);
    _oled.drawHLine(0, 12, 128);
    
    _oled.setCursor(10, 30);
    _oled.printf("重量: %.1fg", weight);
    _oled.setCursor(10, 45);
    _oled.printf("在线状态: %s", online ? "在线" : "离线");
    _oled.setCursor(10, 60);
    _oled.print("点击编码器返回");
}

void OLEDDisplay::drawParamEdit(const String& label, float value, float refValue, bool isMin) {
    _oled.setFont(u8g2_font_wqy12_t_gb2312b);
    _oled.setCursor(0, 10);
    if (isMin) {
        _oled.printf("参考最大值: %.1fg", refValue);
    } else {
        _oled.printf("参考最小值: %.1fg", refValue);
    }
    _oled.drawHLine(0, 12, 128);

    _oled.setCursor(10, 26);
    _oled.print("正在编辑: ");
    _oled.print(label);
    
    _oled.setFont(u8g2_font_logisoso24_tn); // Even larger font for the value
    _oled.setCursor(20, 56);
    _oled.printf("%.1f", value);
    _oled.setFont(u8g2_font_wqy12_t_gb2312b);
    _oled.print("g");
}

void OLEDDisplay::drawAbout(const String& version, const String& buildDate) {
    _oled.setFont(u8g2_font_wqy12_t_gb2312b);
    _oled.setCursor(0, 10);
    _oled.print("关于");
    _oled.drawHLine(0, 12, 128);
    
    _oled.setCursor(5, 26);
    _oled.print("冯氏芦笋组合称");
    _oled.setCursor(5, 42);
    _oled.print("电话：13306400990");
    _oled.setCursor(5, 58);
    _oled.print("2026年4月");
}

void OLEDDisplay::drawRs485Diag(uint32_t txPulse, uint8_t rxByte, uint32_t rxCount) {
    _oled.setFont(u8g2_font_wqy12_t_gb2312b);
    _oled.setCursor(0, 10);
    _oled.print("> 总线测试");
    _oled.drawHLine(0, 12, 128);

    _oled.setCursor(10, 28);
    _oled.printf("发送:  0x%02X", (uint8_t)txPulse);
    
    _oled.setCursor(10, 44);
    _oled.printf("接收:  0x%02X", rxByte);
    
    _oled.setCursor(10, 60);
    _oled.printf("统计:  %u 字节", rxCount);
}

void OLEDDisplay::drawScan(int currentId, bool finished, const std::vector<ScanRow>& history, float scrollY, uint32_t scanCount) {
    _oled.setFont(u8g2_font_wqy12_t_gb2312b);
    _oled.setCursor(0, 10);
    _oled.printf("> 扫描次数: %u", scanCount);
    _oled.drawHLine(0, 12, 128);

    int rowHeight = 12;
    int areaStartY = 14; 
    int startX = 0;

    for (int i = 0; i < (int)history.size(); i++) {
        const auto& row = history[i];
        // Baselines for 4 rows: 26, 38, 50, 62
        int y = areaStartY + rowHeight - (int)scrollY + (i * rowHeight);

        // Visibility Check: Top of box (y-9) must be strictly below header line (12)
        // y - 9 > 12  =>  y > 21
        if (y > 21 && y < 65) {
            for (int b = 0; b < 20; b++) {
                int groupIdx = b / 5;
                int intraIdx = b % 5;
                int x = startX + (groupIdx * 33) + (intraIdx * 6);
                int by = y - 9;
                
                if (row.online[b + 1]) {
                    // Online: Solid frame + solid inner box
                    _oled.drawFrame(x, by, 4, 9);
                    _oled.drawBox(x + 1, by + 1, 2, 7);
                } else {
                    // Offline: Dashed/Dotted frame
                    // Top & Bottom (alternate pixels)
                    _oled.drawPixel(x, by); _oled.drawPixel(x + 2, by);
                    _oled.drawPixel(x, by + 8); _oled.drawPixel(x + 2, by + 8);
                    // Left & Right (alternate pixels)
                    for (int dy = 0; dy < 9; dy += 2) {
                        _oled.drawPixel(x, by + dy);
                        _oled.drawPixel(x + 3, by + dy);
                    }
                }
            }
        }
    }

    // Status indicator at top right
    _oled.setFont(u8g2_font_6x10_tf);
    if (!finished) {
        _oled.setCursor(110, 10);
        _oled.printf("%02d", currentId);
    } else {
        _oled.setCursor(98, 10);
        _oled.print("DONE");
    }
}
