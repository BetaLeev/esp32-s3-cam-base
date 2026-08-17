/**
 * @file display.c
 * @brief 基于 U8g2 官方图形库的 ESP32-S3 OLED 0.96寸 (SSD1306/I2C) 驱动
 * @note 支持任意 GB2312 UTF-8 中文字符串渲染，无需手动算位图
 */

#include "display.h"
#include "config.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "u8g2.h"
#include <string.h>

static const char *TAG = "OLED_DISPLAY";

#define OLED_I2C_NUM I2C_NUM_0

/* 全局 U8g2 句柄 */
static u8g2_t u8g2;

/**
 * @brief U8g2 I2C 字节传输回调函数 (适配 ESP-IDF 硬件 I2C)
 */
static uint8_t u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    static uint8_t buffer[128];
    static uint8_t buf_idx = 0;

    switch (msg) {
    case U8X8_MSG_BYTE_SEND: {
        uint8_t *data = (uint8_t *)arg_ptr;
        while (arg_int > 0) {
            if (buf_idx < sizeof(buffer)) {
                buffer[buf_idx++] = *data;
            }
            data++;
            arg_int--;
        }
        break;
    }
    case U8X8_MSG_BYTE_INIT:
        break;
    case U8X8_MSG_BYTE_SET_DC:
        break;
    case U8X8_MSG_BYTE_START_TRANSFER:
        buf_idx = 0;
        break;
    case U8X8_MSG_BYTE_END_TRANSFER: {
        if (buf_idx == 0)
            break;

        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (u8x8_GetI2CAddress(u8x8)) | I2C_MASTER_WRITE, true);
        i2c_master_write(cmd, buffer, buf_idx, true);
        i2c_master_stop(cmd);

        esp_err_t ret = i2c_master_cmd_begin(OLED_I2C_NUM, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2C 传输失败: %s", esp_err_to_name(ret));
        }
        break;
    }
    default:
        return 0;
    }
    return 1;
}

/**
 * @brief U8g2 GPIO 与延时回调函数
 */
static uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int,
                                            void *arg_ptr) {
    switch (msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        break;
    case U8X8_MSG_DELAY_MILLI:
        vTaskDelay(pdMS_TO_TICKS(arg_int));
        break;
    default:
        return 0;
    }
    return 1;
}

esp_err_t display_init(void) {
    ESP_LOGI(TAG, "初始化 0.96寸 SSD1306 OLED (U8g2 官方驱动模式)");

    /* 配置硬件 I2C 总线 */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_OLED_SDA, // GPIO 21
        .scl_io_num = GPIO_OLED_SCL, // GPIO 47
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };

    i2c_param_config(OLED_I2C_NUM, &conf);
    esp_err_t ret = i2c_driver_install(OLED_I2C_NUM, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C 驱动安装失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 初始化 U8g2 句柄 (SSD1306 128x64 全显存模式) */
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb,
                                           u8g2_esp32_gpio_and_delay_cb);

    /* 设置 I2C 地址 (0x3C << 1 = 0x78) */
    u8g2_SetI2CAddress(&u8g2, OLED_I2C_ADDRESS << 1);

    /* 初始化屏幕并开启显示 (内部自动发送开启电荷泵指令) */
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);

    /* 加载 U8g2 官方 GB2312 常用中文字库 (文泉驿 16x16 点阵) */
    u8g2_SetFont(&u8g2, u8g2_font_wqy16_t_gb2312);

    /* 显示开机提示画面 */
    u8g2_ClearBuffer(&u8g2);
    u8g2_DrawFrame(&u8g2, 0, 0, 128, 64);
    u8g2_DrawUTF8(&u8g2, 16, 26, "智能水泵系统");
    u8g2_DrawUTF8(&u8g2, 24, 48, "U8g2 驱动成功");
    u8g2_SendBuffer(&u8g2);

    ESP_LOGI(TAG, "OLED 显示屏 U8g2 引擎点亮成功!");
    return ESP_OK;
}

void display_show_chinese_demo(const char *text) {
    if (!text || strlen(text) == 0) {
        text = "MAC问题解决专家";
    }

    ESP_LOGI(TAG, "请求显示内容: %s", text);

    u8g2_ClearBuffer(&u8g2);

    /* 绘制外边框与装饰线条 */
    u8g2_DrawFrame(&u8g2, 0, 0, 128, 64);
    u8g2_DrawHLine(&u8g2, 0, 14, 128);

    /* 顶部标题栏 */
    u8g2_SetFont(&u8g2, u8g2_font_wqy12_t_gb2312);
    u8g2_DrawUTF8(&u8g2, 4, 11, "esp32-s3");

    /* 主内容区（选用 16x16 文泉驿中文字体，自动计算 X 轴居中） */
    u8g2_SetFont(&u8g2, u8g2_font_wqy15_t_gb2312);
    int16_t str_width = u8g2_GetUTF8Width(&u8g2, text);
    int16_t x_pos = (128 - str_width) / 2;
    if (x_pos < 2)
        x_pos = 2;

    u8g2_DrawUTF8(&u8g2, x_pos, 44, text);

    /* 刷新到物理屏幕 */
    u8g2_SendBuffer(&u8g2);
}