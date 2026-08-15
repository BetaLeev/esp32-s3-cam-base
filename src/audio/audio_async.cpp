/**
 * @file audio_async.cpp
 * @brief 异步音频播放实现
 *
 * 使用现有 audio.c 模块，在后台任务中播放
 * Core 1 运行解码和 I2S，主程序不阻塞
 */
extern "C" {
#include "audio_async.h"
}

#include "audio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "AUDIO_ASYNC";

// ============================================================================
// 命令类型
// ============================================================================

typedef enum {
    CMD_PLAY = 1,
    CMD_STOP,
    CMD_PAUSE,
    CMD_RESUME,
    CMD_SET_VOLUME,
} async_cmd_t;

typedef struct {
    async_cmd_t cmd;
    union {
        struct {
            char path[256];
        } play;
        struct {
            uint8_t volume;
        } volume;
    } data;
} async_message_t;

// ============================================================================
// 全局状态
// ============================================================================

static TaskHandle_t s_task_handle = NULL;
static QueueHandle_t s_cmd_queue = NULL;
static SemaphoreHandle_t s_state_mutex = NULL;
static StaticSemaphore_t s_mutex_buffer;

static bool s_initialized = false;
static audio_async_state_t s_state = AUDIO_ASYNC_STATE_IDLE;
static audio_async_info_t s_info = {
    .state = AUDIO_ASYNC_STATE_IDLE,
    .duration_ms = 0,
    .position_ms = 0,
    .sample_rate = 0,
    .channels = 0,
    .bitrate = 0,
    .current_file = {0}
};
static uint8_t s_volume = 80;

// ============================================================================
// 后台播放任务
// ============================================================================

static void audio_async_task(void *param)
{
    (void)param;

    ESP_LOGI(TAG, "异步音频任务启动 (Core %d)", xPortGetCoreID());

    while (true) {
        async_message_t msg;

        // 等待命令（阻塞）
        if (xQueueReceive(s_cmd_queue, &msg, portMAX_DELAY) == pdTRUE) {
            switch (msg.cmd) {
                case CMD_PLAY: {
                    const char *path = msg.data.play.path;
                    ESP_LOGI(TAG, "后台播放: %s", path);

                    // 更新状态
                    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        strncpy(s_info.current_file, path, sizeof(s_info.current_file) - 1);
                        s_state = AUDIO_ASYNC_STATE_PLAYING;
                        s_info.position_ms = 0;
                        xSemaphoreGive(s_state_mutex);
                    }

                    // 调用现有 audio 模块播放（同步，但运行在后台任务）
                    int32_t duration = audio_play_file(path);

                    // 播放完成
                    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        if (s_state == AUDIO_ASYNC_STATE_PLAYING) {
                            s_state = AUDIO_ASYNC_STATE_STOPPED;
                            s_info.duration_ms = (duration > 0) ? duration : s_info.position_ms;
                            s_info.position_ms = s_info.duration_ms;
                            ESP_LOGI(TAG, "播放完成: %s (%d ms)", path, s_info.duration_ms);
                        }
                        xSemaphoreGive(s_state_mutex);
                    }
                    break;
                }

                case CMD_STOP: {
                    ESP_LOGI(TAG, "停止播放");
                    audio_stop();

                    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        s_state = AUDIO_ASYNC_STATE_STOPPED;
                        xSemaphoreGive(s_state_mutex);
                    }
                    break;
                }

                case CMD_PAUSE: {
                    ESP_LOGI(TAG, "暂停播放");
                    audio_pause();

                    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        s_state = AUDIO_ASYNC_STATE_PAUSED;
                        xSemaphoreGive(s_state_mutex);
                    }
                    break;
                }

                case CMD_RESUME: {
                    ESP_LOGI(TAG, "恢复播放");
                    audio_resume();

                    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        s_state = AUDIO_ASYNC_STATE_PLAYING;
                        xSemaphoreGive(s_state_mutex);
                    }
                    break;
                }

                case CMD_SET_VOLUME: {
                    uint8_t vol = msg.data.volume.volume;
                    ESP_LOGI(TAG, "设置音量: %d%%", vol);
                    s_volume = vol;
                    audio_set_volume_percent(vol);
                    break;
                }

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

esp_err_t audio_async_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "异步音频已初始化");
        return ESP_OK;
    }

    // 初始化底层 audio 模块
    esp_err_t ret = audio_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "audio 模块初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 设置默认音量
    audio_set_volume_percent(s_volume);

    // 创建互斥锁
    s_state_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buffer);
    if (s_state_mutex == NULL) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        audio_deinit();
        return ESP_FAIL;
    }

    // 创建命令队列
    s_cmd_queue = xQueueCreate(4, sizeof(async_message_t));
    if (s_cmd_queue == NULL) {
        ESP_LOGE(TAG, "创建命令队列失败");
        vSemaphoreDelete(s_state_mutex);
        audio_deinit();
        return ESP_FAIL;
    }

    // 创建后台任务（Core 1）
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        audio_async_task,
        "audio_async",
        8192,  // 栈大小
        NULL,
        configMAX_PRIORITIES - 3,
        &s_task_handle,
        1  // Core 1
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "创建音频任务失败");
        vQueueDelete(s_cmd_queue);
        vSemaphoreDelete(s_state_mutex);
        audio_deinit();
        return ESP_FAIL;
    }

    s_initialized = true;
    s_state = AUDIO_ASYNC_STATE_IDLE;
    memset(&s_info, 0, sizeof(s_info));

    ESP_LOGI(TAG, "异步音频初始化完成 (后台任务在 Core 1)");
    return ESP_OK;
}

esp_err_t audio_async_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    // 发送停止命令
    async_message_t msg = {.cmd = CMD_STOP};
    xQueueSend(s_cmd_queue, &msg, 0);
    vTaskDelay(100);

    // 删除任务
    if (s_task_handle) {
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
    }

    // 清理资源
    if (s_cmd_queue) {
        vQueueDelete(s_cmd_queue);
        s_cmd_queue = NULL;
    }

    if (s_state_mutex) {
        vSemaphoreDelete(s_state_mutex);
        s_state_mutex = NULL;
    }

    // 反初始化 audio 模块
    audio_deinit();

    s_initialized = false;
    ESP_LOGI(TAG, "异步音频已反初始化");

    return ESP_OK;
}

esp_err_t audio_async_play(const char *file_path)
{
    if (!s_initialized || !file_path) {
        return ESP_ERR_INVALID_STATE;
    }

    async_message_t msg = {.cmd = CMD_PLAY};
    snprintf(msg.data.play.path, sizeof(msg.data.play.path), "%s", file_path);

    if (xQueueSend(s_cmd_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "发送播放命令失败");
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "排队播放: %s", file_path);
    return ESP_OK;
}

esp_err_t audio_async_play_sd(const char *path)
{
    return audio_async_play(path);
}

esp_err_t audio_async_stop(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    async_message_t msg = {.cmd = CMD_STOP};
    xQueueSend(s_cmd_queue, &msg, 0);
    return ESP_OK;
}

esp_err_t audio_async_pause(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    async_message_t msg = {.cmd = CMD_PAUSE};
    xQueueSend(s_cmd_queue, &msg, 0);
    return ESP_OK;
}

esp_err_t audio_async_resume(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    async_message_t msg = {.cmd = CMD_RESUME};
    xQueueSend(s_cmd_queue, &msg, 0);
    return ESP_OK;
}

void audio_async_set_volume(uint8_t volume)
{
    if (!s_initialized) return;

    if (volume > 100) volume = 100;
    s_volume = volume;

    async_message_t msg = {.cmd = CMD_SET_VOLUME};
    msg.data.volume.volume = volume;
    xQueueSend(s_cmd_queue, &msg, 0);
}

uint8_t audio_async_get_volume(void)
{
    return s_volume;
}

bool audio_async_is_playing(void)
{
    if (!s_initialized) return false;

    bool playing = false;
    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        playing = (s_state == AUDIO_ASYNC_STATE_PLAYING);
        xSemaphoreGive(s_state_mutex);
    }
    return playing;
}

void audio_async_get_info(audio_async_info_t *info)
{
    if (!info || !s_initialized) return;

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        memcpy(info, &s_info, sizeof(audio_async_info_t));
        xSemaphoreGive(s_state_mutex);
    }
}

uint8_t audio_async_get_progress(void)
{
    if (!s_initialized) return 0;

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
