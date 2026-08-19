/**
 * @file motor.c
 * @brief 电机控制模块核心实现
 */

#include "motor.h"
#include "../../config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "MOTOR";

/* ========== 电机变量与句柄 ========== */
static pump_gear_t s_pump_gear = PUMP_OFF;
static uint8_t s_motor_speed = 0;
static SemaphoreHandle_t s_motor_mutex = NULL;
static StaticSemaphore_t s_motor_mutex_buffer;

static uint8_t get_speed_from_gear(pump_gear_t gear)
{
    switch (gear) {
        case PUMP_OFF:    return 0;
        case PUMP_LOW:    return PUMP_SPEED_LOW;
        case PUMP_MEDIUM: return PUMP_SPEED_MEDIUM;
        case PUMP_HIGH:   return PUMP_SPEED_HIGH;
        default:          return 0;
    }
}

static void soft_start_motor(uint8_t target_speed)
{
    if (target_speed == 0) return;

    if (MOTOR_AIN1_PIN != GPIO_NUM_NC && MOTOR_AIN1_PIN >= 0) {
        gpio_set_level(MOTOR_AIN1_PIN, 1);
    }
    if (MOTOR_AIN2_PIN != GPIO_NUM_NC && MOTOR_AIN2_PIN >= 0) {
        gpio_set_level(MOTOR_AIN2_PIN, 0);
    }

    // 全速启动 150ms 克服静摩擦
    uint32_t duty_full = LEDC_MOTOR_DUTY_MAX;
    ledc_set_duty(LEDC_MOTOR_MODE, LEDC_MOTOR_CHANNEL, duty_full);
    ledc_update_duty(LEDC_MOTOR_MODE, LEDC_MOTOR_CHANNEL);

    vTaskDelay(pdMS_TO_TICKS(150));

    // 回落到目标速度
    uint32_t duty_target = (target_speed * LEDC_MOTOR_DUTY_MAX) / 100;
    ledc_set_duty(LEDC_MOTOR_MODE, LEDC_MOTOR_CHANNEL, duty_target);
    ledc_update_duty(LEDC_MOTOR_MODE, LEDC_MOTOR_CHANNEL);
}

const char* motor_pump_get_gear_name(pump_gear_t gear)
{
    switch (gear) {
        case PUMP_OFF:    return "关闭";
        case PUMP_LOW:    return "低档";
        case PUMP_MEDIUM: return "中档";
        case PUMP_HIGH:   return "高档";
        default:          return "未知";
    }
}

esp_err_t motor_pump_set_gear(pump_gear_t gear)
{
    if (xSemaphoreTake(s_motor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "无法获取电机互斥锁");
        return ESP_ERR_TIMEOUT;
    }

    s_pump_gear = gear;
    uint8_t speed = get_speed_from_gear(gear);

    if (speed > 0) {
        if (speed < PUMP_SPEED_MIN) speed = PUMP_SPEED_MIN;
        soft_start_motor(speed);

        s_motor_speed = speed;
        g_system_status.pump_state = 1;
        g_system_status.pump_speed = speed;
        ESP_LOGI(TAG, "水泵档位: %s, 速度: %d%%", motor_pump_get_gear_name(gear), speed);
    } else {
        ledc_stop(LEDC_MOTOR_MODE, LEDC_MOTOR_CHANNEL, 0);
        if (MOTOR_AIN1_PIN != GPIO_NUM_NC && MOTOR_AIN1_PIN >= 0) {
            gpio_set_level(MOTOR_AIN1_PIN, 0);
        }
        if (MOTOR_AIN2_PIN != GPIO_NUM_NC && MOTOR_AIN2_PIN >= 0) {
            gpio_set_level(MOTOR_AIN2_PIN, 0);
        }

        s_motor_speed = 0;
        g_system_status.pump_state = 0;
        g_system_status.pump_speed = 0;
        ESP_LOGI(TAG, "水泵关闭");
    }

    g_system_status.version++;
    xSemaphoreGive(s_motor_mutex);
    return ESP_OK;
}

pump_gear_t motor_pump_get_gear(void)
{
    return s_pump_gear;
}

esp_err_t motor_set_speed(uint8_t speed)
{
    if (speed > 0 && speed < PUMP_SPEED_MIN) speed = PUMP_SPEED_MIN;
    if (speed > 100) speed = 100;

    if (xSemaphoreTake(s_motor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return ESP_ERR_TIMEOUT;

    s_motor_speed = speed;
    uint32_t duty = (speed * LEDC_MOTOR_DUTY_MAX) / 100;

    esp_err_t ret = ledc_set_duty(LEDC_MOTOR_MODE, LEDC_MOTOR_CHANNEL, duty);
    if (ret == ESP_OK) {
        ret = ledc_update_duty(LEDC_MOTOR_MODE, LEDC_MOTOR_CHANNEL);
    }

    g_system_status.pump_speed = speed;
    g_system_status.version++;

    xSemaphoreGive(s_motor_mutex);
    return ret;
}

uint8_t motor_get_speed(void)
{
    return s_motor_speed;
}

esp_err_t motor_start(void)
{
    if (xSemaphoreTake(s_motor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return ESP_ERR_TIMEOUT;
    s_pump_gear = PUMP_MEDIUM;
    if (s_motor_speed < PUMP_SPEED_MIN) s_motor_speed = PUMP_SPEED_MIN;
    soft_start_motor(s_motor_speed);

    g_system_status.pump_state = 1;
    g_system_status.pump_speed = s_motor_speed;
    g_system_status.version++;

    xSemaphoreGive(s_motor_mutex);
    return ESP_OK;
}

esp_err_t motor_stop(void)
{
    if (xSemaphoreTake(s_motor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return ESP_ERR_TIMEOUT;
    s_pump_gear = PUMP_OFF;
    ledc_stop(LEDC_MOTOR_MODE, LEDC_MOTOR_CHANNEL, 0);
    if (MOTOR_AIN1_PIN != GPIO_NUM_NC && MOTOR_AIN1_PIN >= 0) {
        gpio_set_level(MOTOR_AIN1_PIN, 0);
    }
    if (MOTOR_AIN2_PIN != GPIO_NUM_NC && MOTOR_AIN2_PIN >= 0) {
        gpio_set_level(MOTOR_AIN2_PIN, 0);
    }

    g_system_status.pump_state = 0;
    g_system_status.pump_speed = 0;
    g_system_status.version++;

    xSemaphoreGive(s_motor_mutex);
    return ESP_OK;
}

esp_err_t motor_set_state(bool state)
{
    return state ? motor_start() : motor_stop();
}

bool motor_get_state(void)
{
    return s_pump_gear != PUMP_OFF;
}

esp_err_t motor_init(void)
{
    ESP_LOGI(TAG, "初始化电机模块...");

    if (s_motor_mutex == NULL) {
        s_motor_mutex = xSemaphoreCreateMutexStatic(&s_motor_mutex_buffer);
    }
    if (s_motor_mutex == NULL) {
        ESP_LOGE(TAG, "创建电机互斥锁失败");
        return ESP_FAIL;
    }

    if (MOTOR_AIN1_PIN != GPIO_NUM_NC && MOTOR_AIN1_PIN >= 0) {
        gpio_config_t io_conf_ain1 = {
            .pin_bit_mask = (MOTOR_AIN1_PIN >= 0) ? (1ULL << MOTOR_AIN1_PIN) : 0ULL,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io_conf_ain1);
        gpio_set_level(MOTOR_AIN1_PIN, 0);
    } else {
        ESP_LOGW(TAG, "MOTOR_AIN1_PIN 未配置，跳过 GPIO 配置");
    }

    if (MOTOR_AIN2_PIN != GPIO_NUM_NC && MOTOR_AIN2_PIN >= 0) {
        gpio_config_t io_conf_ain2 = {
            .pin_bit_mask = (MOTOR_AIN2_PIN >= 0) ? (1ULL << MOTOR_AIN2_PIN) : 0ULL,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io_conf_ain2);
        gpio_set_level(MOTOR_AIN2_PIN, 0);
    } else {
        ESP_LOGW(TAG, "MOTOR_AIN2_PIN 未配置，跳过 GPIO 配置");
    }

    if (MOTOR_PWMA_PIN != GPIO_NUM_NC && MOTOR_PWMA_PIN >= 0) {
        ledc_timer_config_t ledc_timer = {
            .speed_mode = LEDC_MOTOR_MODE,
            .timer_num = LEDC_MOTOR_TIMER,
            .freq_hz = LEDC_MOTOR_FREQUENCY,
            .duty_resolution = LEDC_MOTOR_DUTY_RES,
            .clk_cfg = LEDC_AUTO_CLK
        };
        ledc_timer_config(&ledc_timer);

        ledc_channel_config_t ledc_channel = {
            .speed_mode = LEDC_MOTOR_MODE,
            .channel = LEDC_MOTOR_CHANNEL,
            .timer_sel = LEDC_MOTOR_TIMER,
            .gpio_num = MOTOR_PWMA_PIN,
            .duty = 0,
            .hpoint = 0
        };
        ledc_channel_config(&ledc_channel);
        ledc_stop(LEDC_MOTOR_MODE, LEDC_MOTOR_CHANNEL, 0);
    } else {
        ESP_LOGW(TAG, "MOTOR_PWMA_PIN 未配置，跳过 PWM 初始化");
    }

    ESP_LOGI(TAG, "电机模块初始化完成");
    return ESP_OK;
}