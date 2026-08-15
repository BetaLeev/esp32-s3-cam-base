/**
 * @file actuators.c
 * @brief 执行器模块核心实现
 */

#include "actuators.h"
#include "../config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "ACTUATORS";

/* ========== 舵机变量与句柄 ========== */
static uint8_t s_servo_angle = 90;
static SemaphoreHandle_t s_servo_mutex = NULL;
static bool s_servo_ledc_initialized = false;

/* ========== 水泵/电机变量与句柄 ========== */
static pump_gear_t s_pump_gear = PUMP_OFF;
static uint8_t s_motor_speed = 0;
static SemaphoreHandle_t s_motor_mutex = NULL;

/**
 * @brief 舵机 LEDC 初始化
 */
static esp_err_t servo_init(void)
{
    if (s_servo_ledc_initialized) {
        return ESP_OK;
    }

    /* 1. 配置 LEDC 定时器 (使用 hw_pwm.h 中的宏定义) */
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_SERVO_MODE,
        .timer_num = LEDC_SERVO_TIMER,
        .freq_hz = LEDC_SERVO_FREQUENCY,
        .duty_resolution = LEDC_SERVO_DUTY_RES,
        .clk_cfg = LEDC_AUTO_CLK
    };

    esp_err_t ret = ledc_timer_config(&timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "舵机 LEDC 定时器配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 2. 配置 LEDC 通道 */
    ledc_channel_config_t channel = {
        .speed_mode = LEDC_SERVO_MODE,
        .channel = LEDC_SERVO_CHANNEL,
        .timer_sel = LEDC_SERVO_TIMER,
        .gpio_num = SERVO_GPIO,
        .duty = SERVO_DUTY_NEUTRAL, /* 默认初始 90 度 (1500 计数) */
        .hpoint = 0
    };

    ret = ledc_channel_config(&channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "舵机 LEDC 通道配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    s_servo_ledc_initialized = true;
    ESP_LOGI(TAG, "舵机 LEDC 初始化完成 | GPIO: %d | 频率: %dHz | 14位分辨率", 
             SERVO_GPIO, LEDC_SERVO_FREQUENCY);

    return ESP_OK;
}

/* ========================================
 * 舵机 API 实现
 * ======================================== */

esp_err_t actuators_servo_set_angle(uint8_t angle)
{
    if (angle > 180) angle = 180;

    if (xSemaphoreTake(s_servo_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "无法获取舵机互斥锁");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;
    if (!s_servo_ledc_initialized) {
        ret = servo_init();
        if (ret != ESP_OK) {
            xSemaphoreGive(s_servo_mutex);
            return ret;
        }
    }

    /* 计算占空比: 0~180度线性映射到 SERVO_DUTY_MIN(500) ~ SERVO_DUTY_MAX(2500) */
    uint32_t duty = SERVO_DUTY_MIN + ((uint32_t)angle * (SERVO_DUTY_MAX - SERVO_DUTY_MIN)) / 180;

    ret = ledc_set_duty(LEDC_SERVO_MODE, LEDC_SERVO_CHANNEL, duty);
    if (ret == ESP_OK) {
        ret = ledc_update_duty(LEDC_SERVO_MODE, LEDC_SERVO_CHANNEL);
    }

    if (ret == ESP_OK) {
        s_servo_angle = angle;
        
        // 更新系统全局状态
        g_system_status.servo_angle = angle;
        g_system_status.version++;

        ESP_LOGI(TAG, "舵机角度设置成功 -> 目标角度: %d°, 对应 Duty 计数值: %lu", angle, duty);
    } else {
        ESP_LOGE(TAG, "设置舵机占空比失败: %s", esp_err_to_name(ret));
    }

    xSemaphoreGive(s_servo_mutex);
    return ret;
}

uint8_t actuators_servo_get_angle(void)
{
    return s_servo_angle;
}

/* ========================================
 * 水泵与电机 API 实现
 * ======================================== */

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

    gpio_set_level(MOTOR_AIN1_PIN, 1);
    gpio_set_level(MOTOR_AIN2_PIN, 0);

    // 全速启动 150ms 克服静摩擦
    uint32_t duty_full = LEDC_MOTOR_DUTY_MAX;
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty_full);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

    vTaskDelay(pdMS_TO_TICKS(150));

    // 回落到目标速度
    uint32_t duty_target = (target_speed * LEDC_MOTOR_DUTY_MAX) / 100;
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty_target);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

const char* actuators_pump_get_gear_name(pump_gear_t gear)
{
    switch (gear) {
        case PUMP_OFF:    return "关闭";
        case PUMP_LOW:    return "低档";
        case PUMP_MEDIUM: return "中档";
        case PUMP_HIGH:   return "高档";
        default:          return "未知";
    }
}

esp_err_t actuators_pump_set_gear(pump_gear_t gear)
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
        ESP_LOGI(TAG, "水泵档位: %s, 速度: %d%%", actuators_pump_get_gear_name(gear), speed);
    } else {
        ledc_stop(LEDC_MODE, LEDC_CHANNEL, 0);
        gpio_set_level(MOTOR_AIN1_PIN, 0);
        gpio_set_level(MOTOR_AIN2_PIN, 0);

        s_motor_speed = 0;
        g_system_status.pump_state = 0;
        g_system_status.pump_speed = 0;
        ESP_LOGI(TAG, "水泵关闭");
    }

    g_system_status.version++;
    xSemaphoreGive(s_motor_mutex);
    return ESP_OK;
}

pump_gear_t actuators_pump_get_gear(void) { return s_pump_gear; }

esp_err_t actuators_motor_set_speed(uint8_t speed)
{
    if (speed > 0 && speed < PUMP_SPEED_MIN) speed = PUMP_SPEED_MIN;
    if (speed > 100) speed = 100;

    if (xSemaphoreTake(s_motor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return ESP_ERR_TIMEOUT;

    s_motor_speed = speed;
    uint32_t duty = (speed * LEDC_MOTOR_DUTY_MAX) / 100;

    esp_err_t ret = ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    if (ret == ESP_OK) {
        ret = ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    }

    g_system_status.pump_speed = speed;
    g_system_status.version++;

    xSemaphoreGive(s_motor_mutex);
    return ret;
}

uint8_t actuators_motor_get_speed(void) { return s_motor_speed; }

esp_err_t actuators_motor_start(void)
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

esp_err_t actuators_motor_stop(void)
{
    if (xSemaphoreTake(s_motor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return ESP_ERR_TIMEOUT;
    s_pump_gear = PUMP_OFF;
    ledc_stop(LEDC_MODE, LEDC_CHANNEL, 0);
    gpio_set_level(MOTOR_AIN1_PIN, 0);
    gpio_set_level(MOTOR_AIN2_PIN, 0);

    g_system_status.pump_state = 0;
    g_system_status.pump_speed = 0;
    g_system_status.version++;

    xSemaphoreGive(s_motor_mutex);
    return ESP_OK;
}

esp_err_t actuators_motor_set_state(bool state) { return state ? actuators_motor_start() : actuators_motor_stop(); }
bool actuators_motor_get_state(void) { return s_pump_gear != PUMP_OFF; }

static void motor_init(void)
{
    gpio_config_t io_conf_ain1 = {
        .pin_bit_mask = (1ULL << MOTOR_AIN1_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf_ain1);

    gpio_config_t io_conf_ain2 = {
        .pin_bit_mask = (1ULL << MOTOR_AIN2_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf_ain2);

    gpio_set_level(MOTOR_AIN1_PIN, 0);
    gpio_set_level(MOTOR_AIN2_PIN, 0);

    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY,
        .duty_resolution = LEDC_DUTY_RES,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .gpio_num = MOTOR_PWMA_PIN,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel);
    ledc_stop(LEDC_MODE, LEDC_CHANNEL, 0);
}

/* ========================================
 * 执行器模块初始化入口
 * ======================================== */

esp_err_t actuators_init(void)
{
    ESP_LOGI(TAG, "初始化执行器模块...");

    if (s_servo_mutex == NULL) s_servo_mutex = xSemaphoreCreateMutex();
    if (s_motor_mutex == NULL) s_motor_mutex = xSemaphoreCreateMutex();

    if (s_servo_mutex == NULL || s_motor_mutex == NULL) {
        ESP_LOGE(TAG, "创建执行器互斥锁失败");
        return ESP_FAIL;
    }

    // 初始化舵机 PWM
    esp_err_t ret = servo_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "舵机 LEDC 初始化失败");
    }

    // 初始化水泵/电机 GPIO 与 PWM
    motor_init();

    ESP_LOGI(TAG, "执行器模块初始化完成");
    return ESP_OK;
}