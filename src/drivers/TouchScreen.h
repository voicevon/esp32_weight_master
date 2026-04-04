#ifndef TOUCH_SCREEN_H
#define TOUCH_SCREEN_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

/**
 * @class TouchScreen
 * @brief Waveshare 7" ESP32-S3-Touch 屏显与触摸驱动管理类。
 * 封装了 RGB LCD 驱动 (Arduino_GFX) 和 GT911 触摸控制逻辑。
 */
class TouchScreen {
public:
    TouchScreen();
    bool begin();
    void lvglInit();

    // LVGL 回调 (Static)
    static void disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
    static void touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);

    // 硬件分辨率常量
    static constexpr uint16_t screenWidth  = 800;
    static constexpr uint16_t screenHeight = 480;

private:
    void initI2C();
    void resetTouch();
    void writeCH422G(byte addr, byte val);
    void writeReg8(uint16_t reg, uint8_t val);
    uint8_t readReg8(uint16_t reg);

    static Arduino_ESP32RGBPanel *rgbpanel;
    static Arduino_RGB_Display *gfx;
    static uint8_t current_touch_addr;
};

#endif // TOUCH_SCREEN_H
