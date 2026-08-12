/**
 * @file config.h
 * @brief 配置汇总头文件 - 包含所有硬件配置
 *
 * 此文件汇总了 src/config/ 目录下的所有配置
 * 使用方式：只需 include 此文件即可访问所有配置
 */
#ifndef CONFIG_H
#define CONFIG_H

/* 包含所有配置模块 */
#include "config/hw_wifi.h"
#include "config/hw_gpio.h"
#include "config/hw_pwm.h"
#include "config/hw_adc.h"
#include "config/hw_network.h"
#include "config/hw_storage.h"
#include "config/hw_tasks.h"
#include "config/hw_system.h"
#include "config/hw_audio.h"
#include "config/hw_log.h"

/* ========================================
 * 兼容性宏定义 (兼容旧代码)
 * ======================================== */
/**
 * @brief 以下宏用于兼容旧代码
 * 新代码应直接使用 config/ 目录下的定义
 */

/* GPIO 引脚兼容性 */
#define MOTOR_PWMA_PIN       GPIO_MOTOR_PWMA
#define MOTOR_AIN1_PIN       GPIO_MOTOR_AIN1
#define MOTOR_AIN2_PIN       GPIO_MOTOR_AIN2
#define DHT11_PIN            GPIO_DHT11
#define SERVO_GPIO           GPIO_SERVO
#define THERMISTOR_GPIO      GPIO_ADC_THERMISTOR
#define PHOTOSENSOR_GPIO     GPIO_ADC_PHOTOSENSOR

/* 音频引脚兼容性 */
#define AUDIO_BCLK_PIN       GPIO_AUDIO_BCLK
#define AUDIO_WS_PIN         GPIO_AUDIO_WS
#define AUDIO_DIN_PIN        GPIO_AUDIO_DIN
#define AUDIO_GAIN_PIN       GPIO_AUDIO_GAIN
#define AUDIO_SD_PIN         GPIO_AUDIO_SD

/* SD卡引脚兼容性 */
#define SD_MMC_CLK_PIN       GPIO_SD_CLK
#define SD_MMC_CMD_PIN       GPIO_SD_CMD
#define SD_MMC_D0_PIN        GPIO_SD_D0

/* Wi-Fi 兼容性 */
#define WIFI_SSID            WIFI_AP_SSID
#define WIFI_PASSWORD        WIFI_AP_PASSWORD
#define WIFI_CHANNEL         WIFI_AP_CHANNEL
#define WIFI_MAX_CONNECT     WIFI_AP_MAX_CONNECTIONS
#define WIFI_AP_IP_1         WIFI_AP_IP_ADDR_1
#define WIFI_AP_IP_2         WIFI_AP_IP_ADDR_2
#define WIFI_AP_IP_3         WIFI_AP_IP_ADDR_3
#define WIFI_AP_IP_4         WIFI_AP_IP_ADDR_4

/* LEDC 兼容性 */
#define LEDC_TIMER           LEDC_MOTOR_TIMER
#define LEDC_MODE            LEDC_MOTOR_MODE
#define LEDC_CHANNEL         LEDC_MOTOR_CHANNEL
#define LEDC_FREQUENCY       LEDC_MOTOR_FREQUENCY
#define LEDC_DUTY_RES        LEDC_MOTOR_DUTY_RES

/* HTTP 兼容性 */
#define HTTP_PORT            HTTP_SERVER_PORT

/* 任务配置兼容性 */
#define DHT11_TASK_STACK     TASK_SENSOR_STACK_SIZE
#define DHT11_TASK_PRIORITY  TASK_SENSOR_PRIORITY
#define DHT11_READ_INTERVAL  pdMS_TO_TICKS(TASK_SENSOR_READ_INTERVAL_MS)

/* 系统默认值兼容性 - 通过 hw_defaults.h 使用 */

/* ========================================
 * 全局状态结构体
 * ======================================== */
typedef struct {
    /* 热敏电阻 */
    uint32_t thermistor_raw;
    float thermistor_temp;

    /* 光敏电阻 */
    uint32_t photosensor_raw;
    float light;

    /* DHT11温湿度 */
    float dht11_temp;
    float dht11_humidity;
    uint8_t dht11_valid;

    /* 执行器 */
    uint8_t pump_state;
    uint16_t pump_speed;
    uint8_t servo_angle;

    /* 脉冲控制 */
    int pulse_pin;
    uint8_t pulse_enabled;
    uint8_t pulse_current_intensity;
    uint32_t pulse_count;
    float pulse_elapsed_time;
    uint8_t pulse_pin_level;

    /* 网络状态 */
    uint8_t sta_connected;
    int8_t sta_rssi;
    char sta_bssid[18];

    /* 硬件资源监控 */
    uint32_t dram_total;
    uint32_t dram_free;
    uint32_t psram_total;
    uint32_t psram_free;
    uint32_t flash_total;
    uint32_t flash_free;
    uint32_t spiffs_total;
    uint32_t spiffs_free;
    uint64_t sdcard_total;
    uint64_t sdcard_free;
    uint8_t sdcard_mounted;

    /* 系统信息 */
    uint32_t cpu_freq_mhz;
    uint32_t uptime_seconds;
    uint32_t version;
} system_status_t;

extern system_status_t g_system_status;

#endif /* CONFIG_H */
