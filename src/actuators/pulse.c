/**
 * @file pulse.c
 * @brief 脉冲控制模块核心实现
 *
 * 技术方案：
 * - 使用 LEDC 实现 PWM 输出（强度控制）
 * - 单次脉冲：设置定时器一次性输出
 * - 连续脉冲：周期性软件定时器控制
 */

#include "pulse.h"
#include "../config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "PULSE";
#define LOG_TAG TAG

/* ========================================
 * LEDC 通道定义 - 使用独立通道避免冲突
 * ======================================== */
/**
 * 脉冲 PWM 使用 LEDC_TIMER_3 和 LEDC_CHANNEL_5
 * - TIMER_0: 电机 PWM
 * - TIMER_1: 舵机 PWM
 * - TIMER_2: RGB LED
 * - TIMER_3: 脉冲控制（可用）
 */
#define PULSE_LEDC_TIMER        LEDC_TIMER_3
#define PULSE_LEDC_MODE         LEDC_LOW_SPEED_MODE
#define PULSE_LEDC_CHANNEL      LEDC_CHANNEL_5
#define PULSE_LEDC_DUTY_RES     LEDC_TIMER_10_BIT  /**< 10位分辨率 0-1023 */
#define PULSE_LEDC_DUTY_MAX     1023

/* ========================================
 * 软件定时器配置
 * ======================================== */
#define PULSE_TIMER_PERIOD_MS   10   /**< 定时器周期 10ms */

/* ========================================
 * 模块状态
 * ======================================== */
static pulse_config_t s_config = {
    .pin = 2,
    .mode = PULSE_MODE_SINGLE,
    .intensity = 50,
    .frequency = 10,
    .pulse_width = 100,
    .enabled = false
};

static pulse_status_t s_status = {
    .current_intensity = 0,
    .pulse_count = 0,
    .elapsed_time = 0.0f,
    .pin_level = 0
};

static SemaphoreHandle_t s_mutex = NULL;
static StaticSemaphore_t s_mutex_buffer;
static TimerHandle_t s_pulse_timer = NULL;
static StaticTimer_t s_pulse_timer_buffer;
static StaticTask_t s_pulse_task_buffer;
static StackType_t s_pulse_task_stack[2048 / sizeof(StackType_t)];

static volatile bool s_timer_running = false;
static bool s_single_pulse_done = false;    /**< 单次脉冲是否已完成 */
static uint32_t s_pulse_period_ms = 0;      /**< 脉冲周期（连续模式） */
static uint32_t s_pulse_on_time_ms = 0;     /**< 脉冲高电平时间（单次模式） */
static uint32_t s_pulse_off_time_ms = 0;    /**< 脉冲低电平时间 */
static uint32_t s_pulse_timer_ticks = 0;    /**< 定时器计数 */
static TaskHandle_t s_pulse_task_handle = NULL;

/* ========================================
 * LEDC 配置
 * ======================================== */

/**
 * @brief 配置 LEDC 定时器
 */
static esp_err_t configure_ledc_timer(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode = PULSE_LEDC_MODE,
        .timer_num = PULSE_LEDC_TIMER,
        .freq_hz = 1000,  // 基础频率，定时器控制实际频率
        .duty_resolution = PULSE_LEDC_DUTY_RES,
        .clk_cfg = LEDC_AUTO_CLK
    };

    esp_err_t ret = ledc_timer_config(&timer_cfg);
    if (ret != ESP_OK) {
        PULSE_LOGE(TAG, "LEDC定时器配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

/**
 * @brief 配置 LEDC 通道
 */
static esp_err_t configure_ledc_channel(int pin)
{
    // 验证引脚范围
    if (pin < 0 || pin > 48) {
        PULSE_LOGE(TAG, "无效的引脚号: %d", pin);
        return ESP_ERR_INVALID_ARG;
    }

    gpio_num_t gpio_num = (gpio_num_t)pin;

    // 配置 GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        PULSE_LOGE(TAG, "GPIO%d配置失败: %s", pin, esp_err_to_name(ret));
        return ret;
    }

    // 先停止当前通道
    ledc_stop(PULSE_LEDC_MODE, PULSE_LEDC_CHANNEL, 0);

    // 配置 LEDC 通道
    ledc_channel_config_t channel_cfg = {
        .speed_mode = PULSE_LEDC_MODE,
        .channel = PULSE_LEDC_CHANNEL,
        .timer_sel = PULSE_LEDC_TIMER,
        .gpio_num = gpio_num,
        .duty = 0,
        .hpoint = 0,
        .flags = {
            .output_invert = false
        }
    };

    ret = ledc_channel_config(&channel_cfg);
    if (ret != ESP_OK) {
        PULSE_LOGE(TAG, "GPIO%d LEDC通道配置失败: %s", pin, esp_err_to_name(ret));
        return ret;
    }

    PULSE_LOGI(TAG, "LEDC通道配置完成: GPIO%d", pin);
    return ESP_OK;
}

/**
 * @brief 设置 PWM 占空比
 */
static esp_err_t set_pwm_duty(uint8_t intensity)
{
    // intensity: 0-100%, duty: 0-1023
    uint32_t duty = (intensity * PULSE_LEDC_DUTY_MAX) / 100;

    esp_err_t ret = ledc_set_duty(PULSE_LEDC_MODE, PULSE_LEDC_CHANNEL, duty);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = ledc_update_duty(PULSE_LEDC_MODE, PULSE_LEDC_CHANNEL);
    return ret;
}

/**
 * @brief 停止 PWM 输出并重置 GPIO
 */
static void stop_pwm(void)
{
    // 停止 LEDC 通道
    ledc_stop(PULSE_LEDC_MODE, PULSE_LEDC_CHANNEL, 0);

    // 重置 GPIO 为默认状态（输入模式，无上下拉）
    if (s_config.pin >= 0 && s_config.pin <= 48) {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << s_config.pin),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io_conf);
    }
}

/**
 * @brief 清理旧引脚（切换引脚前调用）
 */
static void cleanup_old_pin(int old_pin)
{
    if (old_pin < 0 || old_pin > 48) {
        return;
    }

    // 停止 LEDC 通道
    ledc_stop(PULSE_LEDC_MODE, PULSE_LEDC_CHANNEL, 0);

    // 重置 GPIO 为默认状态
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << old_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    PULSE_LOGI(TAG, "GPIO%d 已清理", old_pin);
}

/* ========================================
 * 脉冲定时器回调
 * ======================================== */

/**
 * @brief 连续脉冲定时器回调
 */
static void pulse_timer_callback(TimerHandle_t xTimer)
{
    (void)xTimer;

    if (!s_config.enabled) {
        return;
    }

    BaseType_t higher_priority_task_woken = pdFALSE;

    // 发送信号给脉冲任务
    xTaskNotifyFromISR(s_pulse_task_handle, 0x01, eSetBits, &higher_priority_task_woken);

    portYIELD_FROM_ISR(higher_priority_task_woken);
}

/* ========================================
 * 脉冲任务
 * ======================================== */

/**
 * @brief 脉冲任务 - 处理定时器事件
 */
static void pulse_task(void *arg)
{
    (void)arg;

    PULSE_LOGI(TAG, "脉冲任务启动");

    while (1) {
        uint32_t notification = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (notification & 0x01) {
            // 获取互斥锁
            if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                s_pulse_timer_ticks++;

                if (s_config.mode == PULSE_MODE_CONTINUOUS) {
                    // 连续脉冲模式 - 使用软件定时器方式
                    // 定时器周期 10ms，累加计数
                    // 每个脉冲周期的tick数 = period_ms / 10
                    // 高电平tick数 = on_time / 10

                    // 防止除零
                    uint32_t period_ticks = s_pulse_period_ms / PULSE_TIMER_PERIOD_MS;
                    uint32_t on_ticks = s_pulse_on_time_ms / PULSE_TIMER_PERIOD_MS;

                    if (period_ticks == 0) {
                        period_ticks = 1;
                    }
                    if (on_ticks == 0) {
                        on_ticks = 1;
                    }

                    // 在周期内控制占空比
                    uint32_t current_tick_in_period = s_pulse_timer_ticks % period_ticks;

                    if (current_tick_in_period < on_ticks) {
                        // 高电平
                        set_pwm_duty(s_config.intensity);
                        s_status.pin_level = 1;
                        s_status.pulse_count++;
                    } else {
                        // 低电平
                        stop_pwm();
                        s_status.pin_level = 0;
                    }

                    // 更新已用时间
                    s_status.elapsed_time += PULSE_TIMER_PERIOD_MS / 1000.0f;
                    s_status.current_intensity = s_config.intensity;

                    // 同步到全局状态
                    g_system_status.pulse_pin = s_config.pin;
                    g_system_status.pulse_enabled = s_config.enabled;
                    g_system_status.pulse_current_intensity = s_status.current_intensity;
                    g_system_status.pulse_count = s_status.pulse_count;
                    g_system_status.pulse_elapsed_time = s_status.elapsed_time;
                    g_system_status.pulse_pin_level = s_status.pin_level;

                } else if (s_config.mode == PULSE_MODE_SINGLE) {
                    // 单次脉冲模式
                    // 防止除零，确保至少1个tick
                    uint32_t on_time_ms = s_pulse_on_time_ms > 0 ? s_pulse_on_time_ms : PULSE_TIMER_PERIOD_MS;
                    uint32_t off_time_ms = s_pulse_off_time_ms > 0 ? s_pulse_off_time_ms : PULSE_TIMER_PERIOD_MS;

                    uint32_t total_ticks = (on_time_ms + off_time_ms) / PULSE_TIMER_PERIOD_MS;
                    uint32_t on_ticks = on_time_ms / PULSE_TIMER_PERIOD_MS;

                    if (total_ticks == 0) total_ticks = 1;
                    if (on_ticks == 0) on_ticks = 1;

                    if (s_single_pulse_done) {
                        // 脉冲已完成，不做任何操作
                    } else if (s_pulse_timer_ticks <= on_ticks) {
                        // 高电平阶段
                        set_pwm_duty(s_config.intensity);
                        s_status.pin_level = 1;
                        s_status.pulse_count = 1;
                        s_status.current_intensity = s_config.intensity;
                    } else {
                        // 低电平阶段或结束
                        stop_pwm();
                        s_status.pin_level = 0;

                        if (s_pulse_timer_ticks >= total_ticks) {
                            // 脉冲完成，标记完成但不停止定时器
                            s_single_pulse_done = true;
                            s_status.current_intensity = 0;
                            PULSE_LOGI(TAG, "单次脉冲完成");
                        }
                    }

                    s_status.elapsed_time += PULSE_TIMER_PERIOD_MS / 1000.0f;

                    // 同步到全局状态
                    g_system_status.pulse_pin = s_config.pin;
                    g_system_status.pulse_enabled = s_config.enabled;
                    g_system_status.pulse_current_intensity = s_status.current_intensity;
                    g_system_status.pulse_count = s_status.pulse_count;
                    g_system_status.pulse_elapsed_time = s_status.elapsed_time;
                    g_system_status.pulse_pin_level = s_status.pin_level;
                }

                xSemaphoreGive(s_mutex);
            }
        }
    }
}

/* ========================================
 * 公共接口实现
 * ======================================== */

esp_err_t pulse_init(void)
{
    PULSE_LOGI(TAG, "初始化脉冲控制模块...");

    // 创建互斥锁
    s_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buffer);
    if (s_mutex == NULL) {
        PULSE_LOGE(TAG, "创建互斥锁失败");
        return ESP_FAIL;
    }

    // 配置 LEDC 定时器
    esp_err_t ret = configure_ledc_timer();
    if (ret != ESP_OK) {
        return ret;
    }

    // 初始化状态
    memset(&s_status, 0, sizeof(s_status));

    PULSE_LOGI(TAG, "脉冲控制模块初始化完成");
    return ESP_OK;
}

esp_err_t pulse_deinit(void)
{
    pulse_stop();

    if (s_pulse_timer != NULL) {
        xTimerDelete(s_pulse_timer, pdMS_TO_TICKS(100));
        s_pulse_timer = NULL;
    }

    if (s_pulse_task_handle != NULL) {
        vTaskDelete(s_pulse_task_handle);
        s_pulse_task_handle = NULL;
    }

    if (s_mutex != NULL) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }

    PULSE_LOGI(TAG, "脉冲控制模块已停止");
    return ESP_OK;
}

esp_err_t pulse_start(int pin, pulse_mode_t mode, uint8_t intensity,
                      uint32_t frequency, uint32_t pulse_width)
{
    if (pin < 0 || pin > 48) {
        PULSE_LOGE(TAG, "无效的引脚号: %d", pin);
        return ESP_ERR_INVALID_ARG;
    }

    if (intensity > 100) {
        intensity = 100;
    }

    if (frequency < 1) {
        frequency = 1;
    } else if (frequency > 1000) {
        frequency = 1000;
    }

    if (pulse_width < 1) {
        pulse_width = 1;
    } else if (pulse_width > 1000) {
        pulse_width = 1000;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        PULSE_LOGE(TAG, "无法获取互斥锁");
        return ESP_ERR_TIMEOUT;
    }

    // 保存旧引脚
    int old_pin = s_config.pin;

    // 如果引脚改变，需要先清理旧引脚，再配置新引脚
    if (s_config.pin != pin || !s_config.enabled) {
        // 清理旧引脚
        if (old_pin != pin && old_pin >= 0) {
            cleanup_old_pin(old_pin);
        }
        // 配置新引脚
        esp_err_t ret = configure_ledc_channel(pin);
        if (ret != ESP_OK) {
            xSemaphoreGive(s_mutex);
            return ret;
        }
    }

    // 更新配置
    s_config.pin = pin;
    s_config.mode = mode;
    s_config.intensity = intensity;
    s_config.frequency = frequency;
    s_config.pulse_width = pulse_width;
    s_config.enabled = true;

    // 计算脉冲时间参数
    if (mode == PULSE_MODE_CONTINUOUS) {
        // 连续模式：频率控制周期，占空比由 intensity 控制
        // period_ms = 1000 / frequency (每个脉冲周期)
        // 高电平时间 = period_ms * intensity / 100 (由强度决定)
        // pulse_width 参数在连续模式下被忽略（或者理解为占空比参考值）
        uint32_t period_ms = 1000 / frequency;
        if (period_ms == 0) {
            period_ms = 1;  // 防止除零
        }

        // 高电平时间由强度和周期决定
        uint32_t on_time = (period_ms * intensity + 99) / 100;  // 四舍五入
        if (on_time == 0 && intensity > 0) {
            on_time = 1;  // 至少1ms
        }
        if (on_time >= period_ms) {
            on_time = period_ms - 1;  // 确保有低电平时间
        }

        s_pulse_on_time_ms = on_time;
        s_pulse_off_time_ms = period_ms - on_time;
        s_pulse_period_ms = period_ms;

        PULSE_LOGI(TAG, "连续脉冲: GPIO%d, 频率=%lu Hz, 脉冲宽度=%lu ms, 强度=%d%%",
                   pin, frequency, pulse_width, intensity);
    } else {
        // 单次模式
        s_pulse_on_time_ms = pulse_width;
        s_pulse_off_time_ms = 100;  // 低电平持续时间
        s_pulse_period_ms = pulse_width + s_pulse_off_time_ms;

        PULSE_LOGI(TAG, "单次脉冲: GPIO%d, 脉冲宽度=%lu ms, 强度=%d%%",
                   pin, pulse_width, intensity);
    }

    // 重置状态
    s_status.pulse_count = 0;
    s_status.elapsed_time = 0.0f;
    s_pulse_timer_ticks = 0;
    s_single_pulse_done = false;

    // 创建脉冲任务（如果尚未创建）
    if (s_pulse_task_handle == NULL) {
        s_pulse_task_handle = xTaskCreateStatic(
            pulse_task,
            "pulse_task",
            sizeof(s_pulse_task_stack),
            NULL,
            configMAX_PRIORITIES - 1,
            s_pulse_task_stack,
            &s_pulse_task_buffer
        );

        if (s_pulse_task_handle == NULL) {
            PULSE_LOGE(TAG, "创建脉冲任务失败");
            xSemaphoreGive(s_mutex);
            return ESP_ERR_NO_MEM;
        }
    }

    // 创建/启动定时器
    if (s_pulse_timer == NULL) {
        s_pulse_timer = xTimerCreateStatic(
            "pulse_timer",
            pdMS_TO_TICKS(PULSE_TIMER_PERIOD_MS),
            pdTRUE,  // 自动重载
            NULL,
            pulse_timer_callback,
            &s_pulse_timer_buffer
        );

        if (s_pulse_timer == NULL) {
            PULSE_LOGE(TAG, "创建定时器失败");
            xSemaphoreGive(s_mutex);
            return ESP_ERR_NO_MEM;
        }
    }

    // 启动定时器
    if (!xTimerStart(s_pulse_timer, pdMS_TO_TICKS(100))) {
        PULSE_LOGE(TAG, "启动定时器失败");
        xSemaphoreGive(s_mutex);
        return ESP_FAIL;
    }

    PULSE_LOGI(TAG, "脉冲控制已启动");
    xSemaphoreGive(s_mutex);

    return ESP_OK;
}

esp_err_t pulse_stop(void)
{
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (!s_config.enabled) {
        xSemaphoreGive(s_mutex);
        return ESP_OK;
    }

    s_config.enabled = false;
    s_single_pulse_done = false;
    s_pulse_timer_ticks = 0;

    // 停止定时器
    if (s_pulse_timer != NULL) {
        xTimerStop(s_pulse_timer, pdMS_TO_TICKS(100));
    }

    // 停止 PWM 输出
    stop_pwm();

    // 重置状态
    s_status.current_intensity = 0;
    s_status.pin_level = 0;

    // 同步到全局状态
    g_system_status.pulse_enabled = false;
    g_system_status.pulse_current_intensity = 0;
    g_system_status.pulse_pin_level = 0;

    PULSE_LOGI(TAG, "脉冲控制已停止");
    xSemaphoreGive(s_mutex);

    return ESP_OK;
}

const pulse_config_t* pulse_get_config(void)
{
    return &s_config;
}

void pulse_get_status(pulse_status_t *status)
{
    if (status == NULL) {
        return;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        memcpy(status, &s_status, sizeof(pulse_status_t));
        xSemaphoreGive(s_mutex);
    } else {
        memset(status, 0, sizeof(pulse_status_t));
    }
}
