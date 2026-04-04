#include "TouchScreen.h"
#include <Wire.h>

/* --- Hardware Pins (Waveshare ESP32-S3-Touch-LCD-7) --- */
#define CH422G_ADDR_SYSTEM  0x24
#define CH422G_ADDR_IO_LOW  0x38
#define LCD_VSYNC  3
#define LCD_HSYNC  46
#define LCD_DE     5
#define LCD_PCLK   7
#define LCD_B0 14
#define LCD_B1 38
#define LCD_B2 18
#define LCD_B3 17
#define LCD_B4 10
#define LCD_G0 39
#define LCD_G1 0
#define LCD_G2 45
#define LCD_G3 48
#define LCD_G4 47
#define LCD_G5 21
#define LCD_R0 1
#define LCD_R1 2
#define LCD_R2 42
#define LCD_R3 41
#define LCD_R4 40

#define TOUCH_INT  4
#define TOUCH_SDA  8
#define TOUCH_SCL  9

/* --- 静态成员存储 --- */
Arduino_ESP32RGBPanel* TouchScreen::rgbpanel = new Arduino_ESP32RGBPanel(
    LCD_DE, LCD_VSYNC, LCD_HSYNC, LCD_PCLK,
    LCD_R0, LCD_R1, LCD_R2, LCD_R3, LCD_R4,
    LCD_G0, LCD_G1, LCD_G2, LCD_G3, LCD_G4, LCD_G5,
    LCD_B0, LCD_B1, LCD_B2, LCD_B3, LCD_B4,
    1, 10, 8, 10, 1, 10, 8, 10, 1, 16000000L
);

Arduino_RGB_Display* TouchScreen::gfx = new Arduino_RGB_Display(
    TouchScreen::screenWidth, TouchScreen::screenHeight, TouchScreen::rgbpanel
);

uint8_t TouchScreen::current_touch_addr = 0x5D;

TouchScreen::TouchScreen() {}

bool TouchScreen::begin() {
    Serial.println("[HW] Starting TouchScreen Initialization...");
    
    // I2C 100kHz for stability (符合 waveshare-s3-touch-expert 最佳实践)
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setClock(100000); 

    // CH422G Expander Init
    writeCH422G(CH422G_ADDR_SYSTEM, 0x01); 

    // GT911 Reset Sequence
    resetTouch();

    // Address Detection
    Wire.beginTransmission(0x5D);
    if (Wire.endTransmission() == 0) {
        current_touch_addr = 0x5D;
    } else {
        current_touch_addr = 0x14;
    }
    Serial.printf("[HW] Touch Controller at 0x%02X\n", current_touch_addr);

    // GT911 Activation
    writeReg8(0x8100, 0x01); 
    writeReg8(0x8040, 0x00);

    // LCD Setup
    if (gfx->begin()) {
        gfx->fillScreen(BLACK);
        Serial.println("[HW] LCD Ready.");
        return true;
    } else {
        Serial.println("[HW] LCD ERR: Init failed.");
        return false;
    }
}

void TouchScreen::lvglInit() {
    lv_init();

    // Draw buffer
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf[screenWidth * 40];
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 40);

    // Display Driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Input Driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);
}

void TouchScreen::resetTouch() {
    pinMode(TOUCH_INT, OUTPUT);
    digitalWrite(TOUCH_INT, LOW); 
    delay(10);
    writeCH422G(CH422G_ADDR_IO_LOW, 0x0C); // RST LOW
    delay(100); 
    writeCH422G(CH422G_ADDR_IO_LOW, 0x0E); // RST HIGH
    delay(300); 
    pinMode(TOUCH_INT, INPUT); 
}

void TouchScreen::writeCH422G(byte addr, byte val) {
    Wire.beginTransmission(addr);
    Wire.write(val);
    Wire.endTransmission();
}

void TouchScreen::writeReg8(uint16_t reg, uint8_t val) {
    Wire.beginTransmission(current_touch_addr);
    Wire.write(reg >> 8);
    Wire.write(reg & 0xFF);
    Wire.write(val);
    Wire.endTransmission();
}

uint8_t TouchScreen::readReg8(uint16_t reg) {
    Wire.beginTransmission(current_touch_addr);
    Wire.write(reg >> 8);
    Wire.write(reg & 0xFF);
    if (Wire.endTransmission() != 0) return 0xFF;
    Wire.requestFrom(current_touch_addr, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0xFF;
}

void TouchScreen::disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
    lv_disp_flush_ready(disp);
}

void TouchScreen::touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    // Read status
    Wire.beginTransmission(current_touch_addr);
    Wire.write(0x81); Wire.write(0x4E);
    if (Wire.endTransmission() != 0) return;
    Wire.requestFrom(current_touch_addr, (uint8_t)1);
    uint8_t touch_status = Wire.read();

    bool is_ready = (touch_status & 0x80);
    uint8_t points = (touch_status & 0x0F);

    if (is_ready && points > 0 && points <= 5) {
        uint8_t buf[4];
        Wire.beginTransmission(current_touch_addr);
        Wire.write(0x81); Wire.write(0x50);
        Wire.endTransmission();
        Wire.requestFrom(current_touch_addr, (uint8_t)4);
        for(int i=0; i<4; i++) buf[i] = Wire.read();
        
        data->point.x = buf[0] | (buf[1] << 8);
        data->point.y = buf[2] | (buf[3] << 8);
        data->state = LV_INDEV_STATE_PR;
        
        // Clear flag
        Wire.beginTransmission(current_touch_addr);
        Wire.write(0x81); Wire.write(0x4E);
        Wire.write(0x00);
        Wire.endTransmission();
    } else {
        data->state = LV_INDEV_STATE_REL;
        if (is_ready) {
            Wire.beginTransmission(current_touch_addr);
            Wire.write(0x81); Wire.write(0x4E);
            Wire.write(0x00);
            Wire.endTransmission();
        }
    }
}
