/**
 * @file led.h
 * @brief LED控制模块接口
 */

#ifndef LED_H
#define LED_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

/* ========================================
 * LED配置
 * ======================================== */

/**
 * @brief LED触发模式
 */
typedef enum {
    LED_MODE_STATIC = 0,   /**< 静态模式 - 保持固定电平 */
    LED_MODE_BLINK = 1     /**< 闪烁模式 - 周期切换 */
} led_mode_t;

/**
 * @brief LED配置结构
 */
typedef struct {
    gpio_num_t pin;            /**< GPIO引脚 */
    led_mode_t mode;           /**< 触发模式 */
    uint8_t initial_level;     /**< 初始电平 0=低/1=高 */
    float high_duration;       /**< 高电平时长(秒) */
    float low_duration;        /**< 低电平时长(秒) */
    int repeat_count;          /**< 重复次数，-1=无限 */
} led_config_t;

/**
 * @brief LED状态结构
 */
typedef struct {
    uint8_t enabled;           /**< 是否启用 */
    uint8_t current_level;     /**< 当前电平 */
    uint32_t executed_count;   /**< 已执行次数 */
    uint32_t total_count;      /**< 总次数 */
    float elapsed_time;        /**< 已用时间(秒) */
    float remaining_time;      /**< 剩余时间(秒) */
} led_status_t;

/* ========================================
 * LED接口
 * ======================================== */

/**
 * @brief 初始化LED模块
 */
esp_err_t led_init(void);

/**
 * @brief 配置LED
 * @param config LED配置
 */
esp_err_t led_configure(const led_config_t *config);

/**
 * @brief 启动LED控制
 * @param config LED配置（可为NULL，使用当前配置）
 */
esp_err_t led_start(const led_config_t *config);

/**
 * @brief 停止LED控制
 */
esp_err_t led_stop(void);

/**
 * @brief 获取LED状态
 * @param status 输出状态
 */
void led_get_status(led_status_t *status);

/**
 * @brief 获取LED当前配置
 * @param config 输出配置
 */
void led_get_config(led_config_t *config);

/**
 * @brief 获取已使用的GPIO引脚列表
 * @param pins 输出引脚数组
 * @param max_count 最大数量
 * @return 实际引脚数量
 */
int led_get_used_pins(int *pins, int max_count);

/* ========================================
 * LED日志宏
 * ======================================== */
#include "../config.h"

#ifndef LED_LOGD
#define LED_LOGD(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)
#endif

#ifndef LED_LOGI
#define LED_LOGI(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
#endif

#ifndef LED_LOGW
#define LED_LOGW(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)
#endif

#ifndef LED_LOGE
#define LED_LOGE(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__)
#endif

#endif // LED_H
