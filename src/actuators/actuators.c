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
#include "rom/ets_sys.h"

static const char *TAG = "ACTUATORS";
#define LOG_TAG TAG

/* ========== 舵机 ========== */

static uint8_t s_servo_angle = 90;
static SemaphoreHandle_t s_servo_mutex;

/* ========== 水泵档位 ========== */

static pump_gear_t s_pump_gear = PUMP_OFF;
static uint8_t s_motor_speed = 0;
static SemaphoreHandle_t s_motor_mutex;

/**
 * @brief 根据档位获取速度值
 */
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

/**
 * @brief 软启动 - 先高速启动再降到目标速度
 */
static void soft_start_motor(uint8_t target_speed)
{
    if (target_speed == 0) return;

    // 软启动：先以100%速度启动，让电机转起来
    gpio_set_level(MOTOR_AIN1_PIN, 1);
    gpio_set_level(MOTOR_AIN2_PIN, 0);

    // 全速启动
    uint32_t duty_full = (100 * 1023) / 100;
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty_full);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

    // 等待启动（150ms足够让电机转起来）
    vTaskDelay(pdMS_TO_TICKS(150));

    // 然后降到目标速度
    uint32_t duty_target = (target_speed * 1023) / 100;
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty_target);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

/**
 * @brief 获取档位名称
 */
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

/* ========================================
 * 舵机实现
 * ======================================== */

static void servo_pulse(uint32_t high_us)
{
    gpio_set_level(SERVO_GPIO, 1);
    ets_delay_us(high_us);
    gpio_set_level(SERVO_GPIO, 0);
}

esp_err_t actuators_servo_set_angle(uint8_t angle)
{
    if (angle > 180) angle = 180;

    if (xSemaphoreTake(s_servo_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "无法获取舵机互斥锁");
        return ESP_ERR_TIMEOUT;
    }

    s_servo_angle = angle;

    // SG90控制: 0度=0.5ms, 90度=1.5ms, 180度=2.5ms
    uint32_t high_us = 500 + (angle * 2000) / 180;

    // 发送50个脉冲
    for (int i = 0; i < 50; i++) {
        servo_pulse(high_us);
        ets_delay_us(20000 - high_us);
    }

    g_system_status.servo_angle = angle;
    g_system_status.version++;

    ESP_LOGI(TAG, "舵机角度: %d", angle);

    xSemaphoreGive(s_servo_mutex);

    return ESP_OK;
}

uint8_t actuators_servo_get_angle(void)
{
    return s_servo_angle;
}

static void servo_init(void)
{
    gpio_set_direction(SERVO_GPIO, GPIO_MODE_OUTPUT);
    ESP_LOGI(TAG, "舵机初始化完成, GPIO: %d", SERVO_GPIO);
}

/* ========================================
 * 水泵档位实现
 * ======================================== */

esp_err_t actuators_pump_set_gear(pump_gear_t gear)
{
    if (xSemaphoreTake(s_motor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "无法获取电机互斥锁");
        return ESP_ERR_TIMEOUT;
    }

    s_pump_gear = gear;
    uint8_t speed = get_speed_from_gear(gear);

    if (speed > 0) {
        // 速度限制：确保最小速度，防止电机不动
        if (speed < PUMP_SPEED_MIN) {
            speed = PUMP_SPEED_MIN;
        }

        // 使用软启动：先高速启动再降到目标速度
        soft_start_motor(speed);

        s_motor_speed = speed;
        g_system_status.pump_state = 1;
        g_system_status.pump_speed = speed;

        ESP_LOGI(TAG, "水泵档位: %s, 速度: %d%%", actuators_pump_get_gear_name(gear), speed);
    } else {
        // 关闭水泵
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

pump_gear_t actuators_pump_get_gear(void)
{
    return s_pump_gear;
}

/* ========================================
 * 电机实现（保留兼容）
 * ======================================== */

esp_err_t actuators_motor_set_speed(uint8_t speed)
{
    // 速度限制：确保最小速度，防止电机不动
    if (speed > 0 && speed < PUMP_SPEED_MIN) {
        speed = PUMP_SPEED_MIN;
    }
    if (speed > 100) speed = 100;

    if (xSemaphoreTake(s_motor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "无法获取电机互斥锁");
        return ESP_ERR_TIMEOUT;
    }

    s_motor_speed = speed;

    // 10位分辨率: 0-1023
    uint32_t duty = (speed * 1023) / 100;

    esp_err_t ret = ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    if (ret != ESP_OK) {
        ACTUATORS_LOGE(TAG, "设置LEDC占空比失败: %s", esp_err_to_name(ret));
        xSemaphoreGive(s_motor_mutex);
        return ret;
    }

    ret = ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "更新LEDC占空比失败: %s", esp_err_to_name(ret));
        xSemaphoreGive(s_motor_mutex);
        return ret;
    }

    ESP_LOGI(TAG, "设置电机速度: %d%% (duty=%lu)", speed, duty);

    g_system_status.pump_speed = speed;
    g_system_status.version++;

    xSemaphoreGive(s_motor_mutex);

    return ESP_OK;
}

uint8_t actuators_motor_get_speed(void)
{
    return s_motor_speed;
}

esp_err_t actuators_motor_start(void)
{
    if (xSemaphoreTake(s_motor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "无法获取电机互斥锁");
        return ESP_ERR_TIMEOUT;
    }

    s_pump_gear = PUMP_MEDIUM;

    // 如果速度为0，设置为默认速度50%（但不低于最小速度）
    if (s_motor_speed < PUMP_SPEED_MIN) {
        s_motor_speed = PUMP_SPEED_MIN;
    }

    // 使用软启动
    soft_start_motor(s_motor_speed);

    g_system_status.pump_state = 1;
    g_system_status.pump_speed = s_motor_speed;
    g_system_status.version++;

    ESP_LOGI(TAG, "电机开启");

    xSemaphoreGive(s_motor_mutex);

    return ESP_OK;
}

esp_err_t actuators_motor_stop(void)
{
    if (xSemaphoreTake(s_motor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "无法获取电机互斥锁");
        return ESP_ERR_TIMEOUT;
    }

    s_pump_gear = PUMP_OFF;

    esp_err_t ret = ledc_stop(LEDC_MODE, LEDC_CHANNEL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC停止失败: %s", esp_err_to_name(ret));
    }

    gpio_set_level(MOTOR_AIN1_PIN, 0);
    gpio_set_level(MOTOR_AIN2_PIN, 0);

    g_system_status.pump_state = 0;
    g_system_status.pump_speed = 0;
    g_system_status.version++;

    ESP_LOGI(TAG, "电机关闭");

    xSemaphoreGive(s_motor_mutex);

    return ESP_OK;
}

esp_err_t actuators_motor_set_state(bool state)
{
    return state ? actuators_motor_start() : actuators_motor_stop();
}

bool actuators_motor_get_state(void)
{
    return s_pump_gear != PUMP_OFF;
}

static void motor_init(void)
{
    esp_err_t ret;

    // 配置AIN1引脚
    gpio_config_t io_conf_ain1 = {
        .pin_bit_mask = (1ULL << MOTOR_AIN1_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ret = gpio_config(&io_conf_ain1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "配置AIN1 GPIO失败: %s", esp_err_to_name(ret));
    }

    // 配置AIN2引脚
    gpio_config_t io_conf_ain2 = {
        .pin_bit_mask = (1ULL << MOTOR_AIN2_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLDOWN_DISABLE,
        .pull_down_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ret = gpio_config(&io_conf_ain2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "配置AIN2 GPIO失败: %s", esp_err_to_name(ret));
    }

    // 初始状态：停止
    gpio_set_level(MOTOR_AIN1_PIN, 0);
    gpio_set_level(MOTOR_AIN2_PIN, 0);

    // 配置LEDC定时器 - ESP-IDF 5.x API
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY,
        .duty_resolution = LEDC_DUTY_RES,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC定时器配置失败: %s", esp_err_to_name(ret));
    }

    // 配置LEDC通道 - ESP-IDF 5.x API
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .gpio_num = MOTOR_PWMA_PIN,
        .duty = 0,
        .hpoint = 0,
        .flags = {
            .output_invert = false
        }
    };

    ret = ledc_channel_config(&ledc_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC通道配置失败: %s", esp_err_to_name(ret));
    }

    // 停止LEDC通道输出
    ret = ledc_stop(LEDC_MODE, LEDC_CHANNEL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC停止失败: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "电机控制初始化完成");
    ESP_LOGI(TAG, "PWMA=GPIO%d, AIN1=GPIO%d, AIN2=GPIO%d", MOTOR_PWMA_PIN, MOTOR_AIN1_PIN, MOTOR_AIN2_PIN);
}

/* ========================================
 * 公共接口实现
 * ======================================== */

esp_err_t actuators_init(void)
{
    ESP_LOGI(TAG, "初始化执行器模块...");

    // 创建舵机互斥锁
    s_servo_mutex = xSemaphoreCreateMutex();
    if (s_servo_mutex == NULL) {
        ESP_LOGE(TAG, "创建舵机互斥锁失败");
        return ESP_FAIL;
    }

    // 创建电机互斥锁
    s_motor_mutex = xSemaphoreCreateMutex();
    if (s_motor_mutex == NULL) {
        ESP_LOGE(TAG, "创建电机互斥锁失败");
        return ESP_FAIL;
    }

    // 初始化舵机
    servo_init();

    // 初始化电机
    motor_init();

    // 舵机初始角度设置暂时跳过 - 启动瞬间电流过大导致 USB 掉电重启
    // actuators_servo_set_angle(90);

    ESP_LOGI(TAG, "执行器模块初始化完成 (舵机初始角度已跳过)");

    return ESP_OK;
}
