/**
 * @file audio_simple.cpp
 * @brief 简化音频播放实现
 *
 * 使用现有 audio.c 模块，在后台 Task 中播放
 * Core 1 运行解码，主程序不阻塞
 */
extern "C" {
#include "audio_simple.h"
}

#include "audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

static const char *TAG = "AUDIO_SIMPLE";

// ============================================================================
// 命令队列
// ============================================================================

typedef enum {
    CMD_PLAY = 1,
    CMD_STOP,
    CMD_SET_VOLUME,
    CMD_TONE,
} simple_cmd_t;

typedef struct {
    simple_cmd_t cmd;
    char path[256];
    uint8_t volume;
    uint32_t freq;
    uint32_t duration;
} simple_message_t;

static QueueHandle_t s_queue = NULL;
static TaskHandle_t s_task = NULL;
static bool s_initialized = false;
static audio_simple_state_t s_state = AUDIO_SIMPLE_STATE_IDLE;
static audio_simple_info_t s_info = {
    .state = AUDIO_SIMPLE_STATE_IDLE, .duration_ms = 0, .position_ms = 0, .current_file = {0}};
static uint8_t s_volume = 80;

// ============================================================================
// 后台任务
// ============================================================================

static void audio_simple_task(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "音频任务启动 (Core %d)", xPortGetCoreID());

    while (true) {
        simple_message_t msg;

        if (xQueueReceive(s_queue, &msg, portMAX_DELAY) == pdTRUE) {
            switch (msg.cmd) {
            case CMD_PLAY: {
                ESP_LOGI(TAG, "播放: %s", msg.path);
                strncpy(s_info.current_file, msg.path, sizeof(s_info.current_file) - 1);
                s_state = AUDIO_SIMPLE_STATE_PLAYING;

                int32_t duration = audio_play_file(msg.path);

                if (s_state == AUDIO_SIMPLE_STATE_PLAYING) {
                    s_state = AUDIO_SIMPLE_STATE_STOPPED;
                    s_info.duration_ms = (duration > 0) ? duration : 0;
                    s_info.position_ms = s_info.duration_ms;
                    ESP_LOGI(TAG, "播放完成");
                }
                break;
            }

            case CMD_STOP:
                ESP_LOGI(TAG, ">>> CMD_STOP 被处理，调用 audio_stop()");
                audio_stop();
                s_state = AUDIO_SIMPLE_STATE_STOPPED;
                ESP_LOGI(TAG, ">>> 停止完成");
                break;

            case CMD_SET_VOLUME:
                s_volume = msg.volume;
                audio_set_volume_percent(msg.volume);
                break;

            case CMD_TONE: {
                ESP_LOGI(TAG, "播放测试音调: %luHz %lums", msg.freq, msg.duration);
                s_state = AUDIO_SIMPLE_STATE_PLAYING;

                // 生成并播放正弦波
                const uint32_t sample_rate = 44100;
                const uint32_t num_samples = (sample_rate * msg.duration) / 1000;
                int16_t *samples = (int16_t *)malloc(num_samples * 2 * sizeof(int16_t));
                if (samples) {
                    for (uint32_t i = 0; i < num_samples; i++) {
                        double t = (double)i / sample_rate;
                        double sine = sin(2.0 * M_PI * msg.freq * t);
                        // 应用音量
                        int16_t sample = (int16_t)(sine * 16000 * s_volume / 100);
                        samples[i * 2] = sample;     // 左声道
                        samples[i * 2 + 1] = sample; // 右声道
                    }
                    // 播放生成的音频
                    audio_play_wav_data(samples, num_samples * 2 * sizeof(int16_t));
                    free(samples);
                }

                s_state = AUDIO_SIMPLE_STATE_STOPPED;
                s_info.duration_ms = msg.duration;
                s_info.position_ms = msg.duration;
                ESP_LOGI(TAG, "测试音调播放完成");
                break;
            }
            }
        }
    }
}

// ============================================================================
// C API
// ============================================================================

extern "C" {

esp_err_t audio_simple_init(void) {
    if (s_initialized)
        return ESP_OK;

    esp_err_t ret = audio_init();
    if (ret != ESP_OK)
        return ret;

    s_queue = xQueueCreate(4, sizeof(simple_message_t));
    if (!s_queue) {
        audio_deinit();
        return ESP_FAIL;
    }

    audio_set_volume_percent(s_volume);

    BaseType_t res = xTaskCreatePinnedToCore(audio_simple_task, "audio_s", 8192, NULL,
                                             configMAX_PRIORITIES - 3, &s_task, 1);

    if (res != pdPASS) {
        vQueueDelete(s_queue);
        audio_deinit();
        return ESP_FAIL;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "初始化完成 (Core 1)");
    return ESP_OK;
}

esp_err_t audio_simple_deinit(void) {
    if (!s_initialized)
        return ESP_OK;

    simple_message_t m = {};
    m.cmd = CMD_STOP;
    xQueueSend(s_queue, &m, 0);
    vTaskDelay(100);

    if (s_task) {
        vTaskDelete(s_task);
        s_task = NULL;
    }
    if (s_queue) {
        vQueueDelete(s_queue);
        s_queue = NULL;
    }

    audio_deinit();
    s_initialized = false;
    return ESP_OK;
}

esp_err_t audio_simple_play(const char *file_path) {
    if (!s_initialized || !file_path)
        return ESP_ERR_INVALID_STATE;

    simple_message_t m = {};
    m.cmd = CMD_PLAY;
    
    snprintf(m.path, sizeof(m.path), "%s", file_path);

    if (xQueueSend(s_queue, &m, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

void audio_simple_stop(void) {
    if (!s_initialized)
        return;

    ESP_LOGI(TAG, ">>> audio_simple_stop() 被调用 - 直接停止");

    // 直接调用 audio_stop()，不通过队列
    // 因为播放是同步阻塞的，队列消息无法及时处理
    audio_stop();
}

void audio_simple_set_volume(uint8_t volume) {
    if (!s_initialized)
        return;
    if (volume > 100)
        volume = 100;
    s_volume = volume;
    simple_message_t m = {};
    m.cmd = CMD_SET_VOLUME;
    m.volume = volume;
    xQueueSend(s_queue, &m, 0);
}

bool audio_simple_is_playing(void) {
    return s_initialized && s_state == AUDIO_SIMPLE_STATE_PLAYING;
}

void audio_simple_get_info(audio_simple_info_t *info) {
    if (!info || !s_initialized)
        return;
    memcpy(info, &s_info, sizeof(audio_simple_info_t));
}

uint8_t audio_simple_get_progress(void) {
    if (!s_initialized || s_info.duration_ms == 0)
        return 0;
    return (s_info.position_ms * 100) / s_info.duration_ms;
}

void audio_simple_play_tone(uint32_t freq_hz, uint32_t duration_ms) {
    if (!s_initialized)
        return;
    simple_message_t m = {};
    m.cmd = CMD_TONE;
    m.freq = freq_hz;
    m.duration = duration_ms;
    xQueueSend(s_queue, &m, pdMS_TO_TICKS(100));
}

} // extern "C"
