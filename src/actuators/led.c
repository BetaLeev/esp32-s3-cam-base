/**
 * @file led.c
 * @brief LED控制模块核心实现
 *
 * 使用FreeRTOS软件定时器实现LED闪烁控制
 */

#include "led.h"
#include "../config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "LED";

// LED配置和状态 - 使用GPIO0（strapping引脚，启动后可用，避免与电机GPIO2冲突）
static led_config_t s_led_config = {
    .pin = GPIO_NUM_0,
    .mode = LED_MODE_BLINK,
    .initial_level = 1,
    .high_duration = 1.0f,
    .low_duration = 1.0f,
    .repeat_count = 3
};

static led_status_t s_led_status = {
    .enabled = false,
    .current_level = 0,
    .executed_count = 0,
    .total_count = 3,
    .elapsed_time = 0,
    .remaining_time = 0
};

static SemaphoreHandle_t s_led_mutex = NULL;
static StaticSemaphore_t s_led_mutex_buffer;

// 定时器相关
static TimerHandle_t s_led_timer = NULL;
static StaticTimer_t s_led_timer_buffer;
static TaskHandle_t s_led_task_handle = NULL;
static StaticTask_t s_led_task_buffer;
static StackType_t s_led_task_stack[1024];

// 内部状态
static uint8_t s_is_high_phase = 1;
static int32_t s_repeat_remaining = 0;
static float s_phase_remaining_ms = 0;

/* ========================================
 * 内部函数
 * ======================================== */

/**
 * @brief 设置GPIO电平
 */
static void set_gpio_level(uint8_t level)
{
    gpio_set_level(s_led_config.pin, level);
    s_led_status.current_level = level;
}

/**
 * @brief LED定时器回调
 */
static void led_timer_callback(TimerHandle_t xTimer)
{
    (void)xTimer;
    BaseType_t higher_priority_task_woken = pdFALSE;

    // 通知LED任务处理
    if (s_led_task_handle != NULL) {
        vTaskNotifyGiveFromISR(s_led_task_handle, &higher_priority_task_woken);
    }

    portYIELD_FROM_ISR(higher_priority_task_woken);
}

/**
 * @brief LED处理任务
 */
static void led_task(void *arg)
{
    (void)arg;

    while (1) {
        // 等待定时器通知
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (xSemaphoreTake(s_led_mutex, portMAX_DELAY) == pdTRUE) {
            if (!s_led_status.enabled) {
                xSemaphoreGive(s_led_mutex);
                continue;
            }

            // 更新已用时间
            s_led_status.elapsed_time += 0.1f; // 100ms周期

            // 减少阶段剩余时间
            s_phase_remaining_ms -= 100.0f;

            // 检查是否需要切换电平
            if (s_phase_remaining_ms <= 0) {
                if (s_is_high_phase) {
                    // 切换到低电平
                    set_gpio_level(0);
                    s_is_high_phase = 0;
                    s_phase_remaining_ms = s_led_config.low_duration * 1000.0f;

                    LED_LOGD(TAG, "LED 低电平, 时长: %.1fs", s_led_config.low_duration);
                } else {
                    // 切换到高电平
                    set_gpio_level(1);
                    s_is_high_phase = 1;
                    s_phase_remaining_ms = s_led_config.high_duration * 1000.0f;

                    LED_LOGD(TAG, "LED 高电平, 时长: %.1fs", s_led_config.high_duration);

                    // 完成一次完整循环
                    s_led_status.executed_count++;

                    // 检查是否到达重复次数
                    if (s_repeat_remaining > 0) {
                        s_repeat_remaining--;
                        if (s_repeat_remaining == 0) {
                            // 停止LED
                            led_stop();
                            LED_LOGI(TAG, "LED控制完成, 执行次数: %lu", s_led_status.executed_count);
                        }
                    }
                }
            }

            // 更新剩余时间
            if (s_repeat_remaining > 0) {
                float total_cycle_time = (s_led_config.high_duration + s_led_config.low_duration);
                s_led_status.remaining_time = s_repeat_remaining * total_cycle_time;
            } else {
                s_led_status.remaining_time = 0;
            }

            xSemaphoreGive(s_led_mutex);
        }
    }
}

/* ========================================
 * 公共接口实现
 * ======================================== */

esp_err_t led_init(void)
{
    ESP_LOGI(TAG, "初始化LED模块...");

    // 创建互斥锁
    s_led_mutex = xSemaphoreCreateMutexStatic(&s_led_mutex_buffer);
    if (s_led_mutex == NULL) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_FAIL;
    }

    // 配置GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << s_led_config.pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        LED_LOGE(TAG, "配置GPIO失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 初始电平为低
    gpio_set_level(s_led_config.pin, 0);

    // 创建软件定时器 (100ms周期)
    s_led_timer = xTimerCreateStatic(
        "led_timer",
        pdMS_TO_TICKS(100),
        pdTRUE,  // 自动重载
        NULL,
        led_timer_callback,
        &s_led_timer_buffer
    );

    if (s_led_timer == NULL) {
        LED_LOGE(TAG, "创建定时器失败");
        return ESP_FAIL;
    }

    // 创建LED处理任务
    s_led_task_handle = xTaskCreateStatic(
        led_task,
        "led_task",
        1024,
        NULL,
        2,  // 优先级
        s_led_task_stack,
        &s_led_task_buffer
    );

    if (s_led_task_handle == NULL) {
        LED_LOGE(TAG, "创建LED任务失败");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LED模块初始化完成, 默认引脚: GPIO%d", s_led_config.pin);
    return ESP_OK;
}

esp_err_t led_configure(const led_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_led_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        LED_LOGW(TAG, "获取互斥锁超时");
        return ESP_ERR_TIMEOUT;
    }

    // 停止当前LED控制
    if (s_led_status.enabled) {
        led_stop();
    }

    // 更新配置
    s_led_config.pin = config->pin;
    s_led_config.mode = config->mode;
    s_led_config.initial_level = config->initial_level;
    s_led_config.high_duration = config->high_duration;
    s_led_config.low_duration = config->low_duration;
    s_led_config.repeat_count = config->repeat_count;

    // 重新配置GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << s_led_config.pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        LED_LOGE(TAG, "配置GPIO失败: %s", esp_err_to_name(ret));
    }

    // 设置初始电平
    gpio_set_level(s_led_config.pin, s_led_config.initial_level);
    s_led_status.current_level = s_led_config.initial_level;

    LED_LOGI(TAG, "LED配置: pin=%d, mode=%s, high=%.1fs, low=%.1fs, repeat=%d",
             s_led_config.pin,
             s_led_config.mode == LED_MODE_STATIC ? "static" : "blink",
             s_led_config.high_duration,
             s_led_config.low_duration,
             s_led_config.repeat_count);

    xSemaphoreGive(s_led_mutex);
    return ESP_OK;
}

esp_err_t led_start(const led_config_t *config)
{
    if (config != NULL) {
        // 先配置
        esp_err_t ret = led_configure(config);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    if (xSemaphoreTake(s_led_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        LED_LOGW(TAG, "获取互斥锁超时");
        return ESP_ERR_TIMEOUT;
    }

    // 初始化状态
    s_led_status.enabled = true;
    s_led_status.executed_count = 0;
    s_led_status.elapsed_time = 0;
    s_led_status.total_count = (s_led_config.repeat_count < 0) ? 0 : s_led_config.repeat_count;
    s_repeat_remaining = s_led_config.repeat_count;

    // 静态模式
    if (s_led_config.mode == LED_MODE_STATIC) {
        gpio_set_level(s_led_config.pin, s_led_config.initial_level);
        s_led_status.current_level = s_led_config.initial_level;
        s_led_status.remaining_time = 0;
        LED_LOGI(TAG, "LED静态模式启动, 电平=%d", s_led_config.initial_level);
    }
    // 闪烁模式
    else {
        s_is_high_phase = 1;
        s_phase_remaining_ms = s_led_config.high_duration * 1000.0f;

        gpio_set_level(s_led_config.pin, 1);
        s_led_status.current_level = 1;

        float total_time = (s_led_config.high_duration + s_led_config.low_duration) *
                          (s_led_config.repeat_count < 0 ? 1 : s_led_config.repeat_count);
        s_led_status.remaining_time = total_time;

        LED_LOGI(TAG, "LED闪烁模式启动, 高=%.1fs, 低=%.1fs, 重复=%d",
                 s_led_config.high_duration,
                 s_led_config.low_duration,
                 s_led_config.repeat_count);

        // 启动定时器
        xTimerStart(s_led_timer, 0);
    }

    xSemaphoreGive(s_led_mutex);
    return ESP_OK;
}

esp_err_t led_stop(void)
{
    if (xSemaphoreTake(s_led_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        LED_LOGW(TAG, "获取互斥锁超时");
        return ESP_ERR_TIMEOUT;
    }

    // 停止定时器
    xTimerStop(s_led_timer, 0);

    // 关闭LED
    gpio_set_level(s_led_config.pin, 0);
    s_led_status.enabled = false;
    s_led_status.current_level = 0;
    s_led_status.remaining_time = 0;

    LED_LOGI(TAG, "LED已停止");

    xSemaphoreGive(s_led_mutex);
    return ESP_OK;
}

void led_get_status(led_status_t *status)
{
    if (status == NULL) return;

    if (xSemaphoreTake(s_led_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        memcpy(status, &s_led_status, sizeof(led_status_t));
        xSemaphoreGive(s_led_mutex);
    } else {
        memset(status, 0, sizeof(led_status_t));
    }
}

void led_get_config(led_config_t *config)
{
    if (config == NULL) return;

    if (xSemaphoreTake(s_led_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        memcpy(config, &s_led_config, sizeof(led_config_t));
        xSemaphoreGive(s_led_mutex);
    } else {
        memset(config, 0, sizeof(led_config_t));
    }
}

int led_get_used_pins(int *pins, int max_count)
{
    if (pins == NULL || max_count <= 0) return 0;

    // 返回当前LED使用的引脚
    if (max_count >= 1) {
        pins[0] = (int)s_led_config.pin;
        return 1;
    }

    return 0;
}
