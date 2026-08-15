/**
 * @file audio_i2s.cpp
 * @brief 音频后台播放封装层
 *
 * 将音频播放移至后台 Task（Core 1），主程序只需调用 audio_i2s_play_file()
 * 不阻塞主线程
 *
 * 支持格式: MP3, WAV
 */
extern "C" {
#include "audio_i2s.h"
}

#include "audio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "AUDIO_I2S";

// ============================================================================
// 命令类型
// ============================================================================

typedef enum {
    CMD_NONE = 0,
    CMD_PLAY_FILE,
    CMD_STOP,
} audio_i2s_cmd_t;

typedef struct {
    audio_i2s_cmd_t cmd;
    char path[256];
} audio_i2s_message_t;

// ============================================================================
// 全局变量
// ============================================================================

static TaskHandle_t s_task_handle = NULL;
static QueueHandle_t s_cmd_queue = NULL;
static SemaphoreHandle_t s_state_mutex = NULL;
static StaticSemaphore_t s_mutex_buffer;

static audio_i2s_state_t s_state = AUDIO_I2S_STATE_IDLE;
static audio_i2s_info_t s_info = {
    .state = AUDIO_I2S_STATE_IDLE,
    .duration_ms = 0,
    .position_ms = 0,
    .sample_rate = 0,
    .channels = 0,
    .bitrate = 0,
    .current_file = {0}
};
static uint8_t s_volume = 80;  // 0-100
static bool s_initialized = false;
static bool s_stop_requested = false;

// ============================================================================
// 后台播放任务
// ============================================================================

static void audio_play_task(void *param)
{
    (void)param;

    ESP_LOGI(TAG, "音频播放任务启动 (Core %d)", xPortGetCoreID());

    while (true) {
        // 处理命令
        audio_i2s_message_t msg;
        if (xQueueReceive(s_cmd_queue, &msg, portMAX_DELAY) == pdTRUE) {
            switch (msg.cmd) {
                case CMD_PLAY_FILE: {
                    ESP_LOGI(TAG, "开始后台播放: %s", msg.path);

                    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        strncpy(s_info.current_file, msg.path, sizeof(s_info.current_file) - 1);
                        s_state = AUDIO_I2S_STATE_PLAYING;
                        s_info.position_ms = 0;
                        xSemaphoreGive(s_state_mutex);
                    }

                    // 调用现有的 audio_play_file（同步播放）
                    // 这会在后台任务中执行，不阻塞主线程
                    int32_t duration = audio_play_file(msg.path);

                    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        if (s_state == AUDIO_I2S_STATE_PLAYING) {
                            s_state = AUDIO_I2S_STATE_STOPPED;
                            s_info.duration_ms = (duration > 0) ? duration : 0;
                            s_info.position_ms = s_info.duration_ms;
                            ESP_LOGI(TAG, "播放完成: %s", msg.path);
                        }
                        xSemaphoreGive(s_state_mutex);
                    }
                    break;
                }

                case CMD_STOP:
                    ESP_LOGI(TAG, "停止播放请求");
                    audio_stop();
                    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        s_state = AUDIO_I2S_STATE_STOPPED;
                        xSemaphoreGive(s_state_mutex);
                    }
                    break;

                default:
                    break;
            }
        }

        vTaskDelay(1);
    }
}

// ============================================================================
// C API 实现
// ============================================================================

extern "C" {

esp_err_t audio_i2s_init(gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout)
{
    (void)bclk;
    (void)ws;
    (void)dout;

    if (s_initialized) {
        ESP_LOGW(TAG, "audioI2S 已经初始化");
        return ESP_OK;
    }

    // 初始化底层音频模块
    esp_err_t ret = audio_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "audio 模块初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 设置音量
    audio_set_volume_percent(s_volume);

    // 初始化互斥锁
    s_state_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buffer);
    if (s_state_mutex == NULL) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_FAIL;
    }

    // 创建命令队列
    s_cmd_queue = xQueueCreate(4, sizeof(audio_i2s_message_t));
    if (s_cmd_queue == NULL) {
        ESP_LOGE(TAG, "创建命令队列失败");
        vSemaphoreDelete(s_state_mutex);
        return ESP_FAIL;
    }

    // 创建后台播放任务（Core 1）
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        audio_play_task,
        "audio_play",
        8192,  // 栈大小
        NULL,
        configMAX_PRIORITIES - 3,  // 中等优先级
        &s_task_handle,
        1  // Core 1
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "创建音频任务失败");
        vQueueDelete(s_cmd_queue);
        vSemaphoreDelete(s_state_mutex);
        return ESP_FAIL;
    }

    s_initialized = true;
    s_state = AUDIO_I2S_STATE_IDLE;
    memset(&s_info, 0, sizeof(s_info));

    ESP_LOGI(TAG, "audioI2S 初始化完成 (后台任务运行在 Core 1)");

    return ESP_OK;
}

esp_err_t audio_i2s_init_default(void)
{
    return audio_i2s_init(
        (gpio_num_t)GPIO_AUDIO_BCLK,
        (gpio_num_t)GPIO_AUDIO_WS,
        (gpio_num_t)GPIO_AUDIO_DIN
    );
}

esp_err_t audio_i2s_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    // 发送停止命令
    audio_i2s_message_t msg = {.cmd = CMD_STOP};
    xQueueSend(s_cmd_queue, &msg, 0);

    // 等待任务结束
    vTaskDelay(100);

    if (s_task_handle != NULL) {
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
    }

    // 清理资源
    if (s_cmd_queue != NULL) {
        vQueueDelete(s_cmd_queue);
        s_cmd_queue = NULL;
    }

    if (s_state_mutex != NULL) {
        vSemaphoreDelete(s_state_mutex);
        s_state_mutex = NULL;
    }

    // 反初始化音频模块
    audio_deinit();

    s_initialized = false;
    ESP_LOGI(TAG, "audioI2S 已反初始化");

    return ESP_OK;
}

esp_err_t audio_i2s_play_file(const char *file_path)
{
    if (!s_initialized || file_path == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    audio_i2s_message_t msg = {.cmd = CMD_PLAY_FILE};
    snprintf(msg.path, sizeof(msg.path), "%s", file_path);

    // 非阻塞发送
    if (xQueueSend(s_cmd_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "发送播放命令失败（队列满）");
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "排队播放: %s", file_path);
    return ESP_OK;
}

esp_err_t audio_i2s_play_sd(const char *path)
{
    return audio_i2s_play_file(path);
}

esp_err_t audio_i2s_play_url(const char *url)
{
    (void)url;
    // 网络流播放暂未实现
    ESP_LOGW(TAG, "URL 播放暂未支持");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t audio_i2s_stop(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    audio_i2s_message_t msg = {.cmd = CMD_STOP};
    xQueueSend(s_cmd_queue, &msg, 0);

    return ESP_OK;
}

bool audio_i2s_pause_resume(void)
{
    if (!s_initialized) {
        return false;
    }

    // 简单实现：停止当前播放
    audio_stop();
    return true;
}

void audio_i2s_set_volume(uint8_t vol)
{
    if (!s_initialized) {
        return;
    }

    // audio_i2s 使用 0-21，转换为 0-100
    uint8_t vol_100 = (vol * 100) / 21;
    if (vol_100 > 100) vol_100 = 100;

    s_volume = vol_100;
    audio_set_volume_percent(vol_100);
}

void audio_i2s_get_info(audio_i2s_info_t *info)
{
    if (info == NULL || !s_initialized) {
        return;
    }

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        memcpy(info, &s_info, sizeof(audio_i2s_info_t));
        xSemaphoreGive(s_state_mutex);
    }
}

bool audio_i2s_is_playing(void)
{
    if (!s_initialized) {
        return false;
    }

    bool playing = false;
    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        playing = (s_state == AUDIO_I2S_STATE_PLAYING);
        xSemaphoreGive(s_state_mutex);
    }
    return playing;
}

bool audio_i2s_set_position(uint16_t sec)
{
    (void)sec;
    // 定位功能暂未实现
    return false;
}

void audio_i2s_skip(int16_t sec)
{
    (void)sec;
    // 跳过功能暂未实现
}

uint8_t audio_i2s_get_progress(void)
{
    if (!s_initialized) {
        return 0;
    }

    uint8_t progress = 0;
    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (s_info.duration_ms > 0) {
            progress = (s_info.position_ms * 100) / s_info.duration_ms;
            if (progress > 100) progress = 100;
        }
        xSemaphoreGive(s_state_mutex);
    }
    return progress;
}

}  // extern "C"
