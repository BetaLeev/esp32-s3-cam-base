/**
 * @file servo.c
 * @brief 舵机控制模块核心实现
 */

#include "servo.h"
#include "../../config.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "SERVO";

static uint8_t s_servo_angle = 90;
static SemaphoreHandle_t s_servo_mutex = NULL;
static StaticSemaphore_t s_servo_mutex_buffer;
static bool s_servo_ledc_initialized = false;

/**
 * @brief 舵机 LEDC 初始化
 */
static esp_err_t servo_ledc_init(void)
{
    if (s_servo_ledc_initialized) {
        return ESP_OK;
    }

    /* 1. 配置 LEDC 定时器 */
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
        .duty = SERVO_DUTY_NEUTRAL, /* 默认初始 90 度 */
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

esp_err_t servo_init(void)
{
    if (s_servo_mutex == NULL) {
        s_servo_mutex = xSemaphoreCreateMutexStatic(&s_servo_mutex_buffer);
    }
    if (s_servo_mutex == NULL) {
        ESP_LOGE(TAG, "创建舵机互斥锁失败");
        return ESP_FAIL;
    }

    return servo_ledc_init();
}
esp_err_t servo_set_angle(uint8_t angle)
{
    // SG90 / 标准舵机安全范围 0-180°
    if (angle > 180) {
        angle = 180;
    }

    if (xSemaphoreTake(s_servo_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "无法获取舵机互斥锁");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;
    if (!s_servo_ledc_initialized) {
        ret = servo_ledc_init();
        if (ret != ESP_OK) {
            xSemaphoreGive(s_servo_mutex);
            return ret;
        }
    }

    /* 
     * 特别修正：
     * 如果 angle 等于 150，说明已经达到了前端的顶端。
     * 为了防止超出你手头这只舵机的物理机械极限（导致死区无响应），
     * 我们人为将 150° 的最大 Duty 限制在 1950 或 2000（对应约 135° 左右的脉宽），
     * 这样既能让它转到最右侧，又不会因为顶死而罢工。
     */
    uint32_t duty;
    if (angle >= 150) {
        duty = 2000; // 你可以尝试在 1950~2100 之间微调这个值
    } else {
        duty = SERVO_DUTY_MIN + ((uint32_t)angle * (SERVO_DUTY_MAX - SERVO_DUTY_MIN)) / 180;
    }

    ESP_LOGI(TAG, "当前角度: %d, 最终施加Duty: %lu", angle, duty);

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
uint8_t servo_get_angle(void)
{
    return s_servo_angle;
}