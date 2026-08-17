/**
 * @file display.h
 * @brief ESP32-S3 OLED 0.96寸屏幕 (SSD1306/I2C/LVGL) 驱动模块
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 OLED 显示屏、I2C 总线及 LVGL 驱动框架
 * @return esp_err_t ESP_OK 表示成功，其他表示失败
 */
esp_err_t display_init(void);

/**
 * @brief 在 OLED 屏幕中央显示中文/英文字符串 (LVGL 线程安全)
 * @param text 要显示的中文或英文字符串 (需为 UTF-8 编码)
 */
void display_show_chinese_demo(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* DISPLAY_H */