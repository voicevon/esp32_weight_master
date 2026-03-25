#include "OLEDDisplay.h"

void OLEDDisplay::begin() {
    _oled.begin();
    _oled.enableUTF8Print(); // 启用 UTF8 支持以显示中文
}

void OLEDDisplay::clear() {
    _oled.clearBuffer();
}

void OLEDDisplay::display() {
    _oled.sendBuffer();
}

void OLEDDisplay::drawSplash() {
    _oled.clearBuffer();
    _oled.setFont(u8g2_font_wqy12_t_gb2312b);
    _oled.drawStr(30, 25, "ANTIGRAVITY");
    _oled.setCursor(35, 45);
    _oled.print("重量大师"); // "Weight Master" in Chinese
    _oled.sendBuffer();
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

void OLEDDisplay::drawParamEdit(const String& label, float value) {
    _oled.setFont(u8g2_font_wqy12_t_gb2312b);
    _oled.setCursor(10, 12);
    _oled.print("正在编辑: ");
    _oled.print(label);
    
    _oled.setFont(u8g2_font_logisoso18_tn);
    _oled.setCursor(20, 45);
    _oled.printf("%d", (int)value);
    _oled.setFont(u8g2_font_wqy12_t_gb2312b);
    _oled.print("g");
}

void OLEDDisplay::drawAbout(const String& version, const String& buildDate) {
    _oled.setFont(u8g2_font_wqy12_t_gb2312b);
    _oled.setCursor(0, 10);
    _oled.print("关于系统");
    _oled.drawHLine(0, 12, 128);
    
    _oled.setCursor(5, 26);
    _oled.print("Fang's Asp-CombScale"); // Fang casing fixed
    _oled.setCursor(5, 40);
    _oled.print("电话: 13306400990");
    _oled.setCursor(5, 52);
    _oled.print("版本: "); _oled.print(version);
    
    _oled.setCursor(5, 64);
    _oled.print("Copyright 2026"); // Added copyright, removed "Click to back"
}

void OLEDDisplay::drawRs485Diag(uint32_t txPulse, uint8_t rxByte, uint32_t rxCount) {
    _oled.setFont(u8g2_font_wqy12_t_gb2312b);
    _oled.setCursor(0, 10);
    _oled.print("> 物理层总线测试");
    _oled.drawHLine(0, 12, 128);

    _oled.setCursor(10, 26);
    _oled.printf("发送脉冲:  0x%02X", (uint8_t)txPulse);
    
    _oled.drawHLine(0, 32, 128);

    _oled.setCursor(10, 46);
    _oled.printf("接收脉冲:  0x%02X", rxByte);
    
    _oled.setCursor(80, 52);
    _oled.printf("#%u", rxCount);

    _oled.drawHLine(0, 58, 128);
    _oled.setCursor(20, 64);
    _oled.print("按键返回菜单");
}

void OLEDDisplay::drawScan(int progress, bool finished, const bool* onlineStatus) {
    _oled.setFont(u8g2_font_wqy12_t_gb2312b);
    _oled.setCursor(0, 10);
    _oled.print("> 正在扫描节点");
    _oled.drawHLine(0, 12, 128);

    int startX = 4;
    int gridY = 15;
    for (int i = 0; i < 20; i++) {
        int id = i + 1;
        int x = startX + (i * 6);
        _oled.drawFrame(x, gridY, 5, 8);
        if (onlineStatus[id]) {
            _oled.drawBox(x + 1, gridY + 1, 3, 6);
        }
    }

    _oled.setCursor(10, 35);
    if (!finished) {
        _oled.printf("正在检查 ID: %d", progress);
        _oled.drawFrame(10, 42, 108, 6);
        _oled.drawBox(10, 42, (progress * 108 / 20), 6);
    } else {
        _oled.print("扫描完成!");
        _oled.setCursor(10, 55);
        _oled.print("点击编码器返回");
    }
}
