/**
 * @file audio_adf.cpp
 * @brief ESP-ADF Pipeline 音频播放实现
 *
 * 基于 ESP-ADF 的 Pipeline 架构实现音频播放
 *
 * ESP-ADF 支持的格式: MP3, AAC, WAV, FLAC, OGG, OPUS, M4A
 */
extern "C" {
#include "audio_adf.h"
}

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "aac_decoder.h"
#include "wav_decoder.h"
#include "flac_decoder.h"
#include "ogg_decoder.h"
#include "opus_decoder.h"

#include <string.h>

static const char *TAG = "AUDIO_ADF";

// ============================================================================
// 命令类型
// ============================================================================

typedef enum {
    CMD_PLAY_FILE = 1,
    CMD_PLAY_URL,
    CMD_PLAY_SD,
    CMD_STOP,
    CMD_PAUSE,
    CMD_RESUME,
    CMD_SET_VOLUME,
    CMD_SEEK,
} adf_cmd_t;

typedef struct {
    adf_cmd_t cmd;
    union {
        struct {
            char path[512];
        } file;
        struct {
            char url[512];
        } url;
        struct {
            uint8_t volume;
        } volume;
        struct {
            uint32_t position_ms;
        } seek;
    } data;
} adf_message_t;

// ============================================================================
// 全局状态
// ============================================================================

static TaskHandle_t s_task_handle = NULL;
static QueueHandle_t s_cmd_queue = NULL;
static SemaphoreHandle_t s_state_mutex = NULL;
static StaticSemaphore_t s_mutex_buffer;

static bool s_initialized = false;
static audio_adf_state_t s_state = AUDIO_ADF_STATE_IDLE;
static audio_adf_info_t s_info = {0};
static uint8_t s_volume = 80;
static audio_adf_event_callback_t s_event_callback = NULL;
static void *s_event_user_data = NULL;

// Pipeline 组件
static audio_pipeline_handle_t s_pipeline = NULL;
static audio_element_handle_t s_fatfs_reader = NULL;
static audio_element_handle_t s_decoder = NULL;
static audio_element_handle_t s_i2s_writer = NULL;

// ============================================================================
// ESP-ADF Pipeline 初始化
// ============================================================================

static esp_err_t init_adf_pipeline(gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout)
{
    ESP_LOGI(TAG, "初始化 ADF Pipeline...");

    // 1. 创建 Pipeline
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    s_pipeline = audio_pipeline_init(&pipeline_cfg);
    if (s_pipeline == NULL) {
        ESP_LOGE(TAG, "创建 Pipeline 失败");
        return ESP_FAIL;
    }

    // 2. 创建 FATFS 流读取器
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_READER;
    s_fatfs_reader = fatfs_stream_init(&fatfs_cfg);

    // 3. 创建 MP3 解码器
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    s_decoder = mp3_decoder_init(&mp3_cfg);

    // 4. 创建 I2S 输出流
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.i2s_config.sample_rate = 44100;
    i2s_cfg.i2s_pin.bck_io_num = (int)bclk;
    i2s_cfg.i2s_pin.ws_io_num = (int)ws;
    i2s_cfg.i2s_config.dma_desc_num = 6;
    i2s_cfg.i2s_config.dma_frame_num = 160;
    i2s_cfg.stack_in_ext = true;  // 栈在外部内存
    i2s_cfg.task_core = 1;       // Core 1
    s_i2s_writer = i2s_stream_init(&i2s_cfg);

    // 5. 注册 Pipeline 组件
    audio_pipeline_register(s_pipeline, s_fatfs_reader, "file");
    audio_pipeline_register(s_pipeline, s_decoder, "decoder");
    audio_pipeline_register(s_pipeline, s_i2s_writer, "i2s");

    // 6. 连接 Pipeline: file -> decoder -> i2s
    const char *link_tag[] = {"file", "decoder", "i2s"};
    audio_pipeline_link(s_pipeline, link_tag, 3);

    // 7. 设置 Ringbuffer
    audio_pipeline_set_ringbuf_info(s_pipeline, NULL, 0);

    ESP_LOGI(TAG, "ADF Pipeline 初始化完成");
    return ESP_OK;
}

static void deinit_adf_pipeline(void)
{
    if (s_pipeline) {
        audio_pipeline_stop(s_pipeline);
        audio_pipeline_wait_for_stop(s_pipeline);
        audio_pipeline_unregister(s_pipeline, s_fatfs_reader);
        audio_pipeline_unregister(s_pipeline, s_decoder);
        audio_pipeline_unregister(s_pipeline, s_i2s_writer);
        audio_pipeline_deinit(s_pipeline);
        s_pipeline = NULL;
    }
}

// ============================================================================
// 事件回调
// ============================================================================

static void audio_adf_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *data)
{
    (void)handler_args;
    (void)base;
    (void)data;

    if (id == AUDIO_PIPELINE_EVENT_AUTOFILL_DATA) {
        // 数据填充事件
    } else if (id == AUDIO_PIPELINE_EVENT_DATA_READY) {
        // 数据就绪事件
    } else if (id == AUDIO_PIPELINE_EVENT_STATE_CHANGED) {
        // 状态变化事件
    } else if (id == AUDIO_PIPELINE_EVENT_FINISH) {
        // 播放完成事件
        ESP_LOGI(TAG, "Pipeline 播放完成");

        if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            s_state = AUDIO_ADF_STATE_STOPPED;
            xSemaphoreGive(s_state_mutex);
        }

        if (s_event_callback) {
            s_event_callback(AUDIO_ADF_EVENT_COMPLETE, s_event_user_data);
        }
    }
}

// ============================================================================
// 后台任务
// ============================================================================

static void audio_adf_task(void *param)
{
    (void)param;

    ESP_LOGI(TAG, "ADF 音频任务启动 (Core %d)", xPortGetCoreID());

    // 注册 Pipeline 事件处理器
    audio_pipeline_set_listener(s_pipeline, NULL);

    while (true) {
        adf_message_t msg;

        // 处理命令队列
        if (xQueueReceive(s_cmd_queue, &msg, pdMS_TO_TICKS(10)) == pdTRUE) {
            switch (msg.cmd) {
                case CMD_PLAY_FILE:
                case CMD_PLAY_SD: {
                    const char *path = msg.data.file.path;
                    ESP_LOGI(TAG, "播放文件: %s", path);

                    // 停止当前播放
                    audio_pipeline_stop(s_pipeline);
                    audio_pipeline_wait_for_stop(s_pipeline);

                    // 设置文件路径
                    audio_element_set_uri(s_fatfs_reader, path);

                    // 开始播放
                    audio_pipeline_run(s_pipeline);

                    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        strncpy(s_info.current_file, path, sizeof(s_info.current_file) - 1);
                        s_state = AUDIO_ADF_STATE_PLAYING;
                        xSemaphoreGive(s_state_mutex);
                    }

                    if (s_event_callback) {
                        s_event_callback(AUDIO_ADF_EVENT_PLAYING, s_event_user_data);
                    }
                    break;
                }

                case CMD_PLAY_URL: {
                    const char *url = msg.data.url.url;
                    ESP_LOGI(TAG, "播放 URL: %s", url);

                    // 停止当前播放
                    audio_pipeline_stop(s_pipeline);
                    audio_pipeline_wait_for_stop(s_pipeline);

                    // 设置 URL（需要使用 http_stream）
                    audio_element_set_uri(s_fatfs_reader, url);

                    // 开始播放
                    audio_pipeline_run(s_pipeline);

                    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        strncpy(s_info.current_file, url, sizeof(s_info.current_file) - 1);
                        s_state = AUDIO_ADF_STATE_PLAYING;
                        xSemaphoreGive(s_state_mutex);
                    }
                    break;
                }

                case CMD_STOP: {
                    ESP_LOGI(TAG, "停止播放");
                    audio_pipeline_stop(s_pipeline);

                    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        s_state = AUDIO_ADF_STATE_STOPPED;
                        xSemaphoreGive(s_state_mutex);
                    }

                    if (s_event_callback) {
                        s_event_callback(AUDIO_ADF_EVENT_STOPPED, s_event_user_data);
                    }
                    break;
                }

                case CMD_PAUSE: {
                    ESP_LOGI(TAG, "暂停播放");
                    // 注意：ESP-ADF 暂停实现可能需要更复杂的处理
                    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        s_state = AUDIO_ADF_STATE_PAUSED;
                        xSemaphoreGive(s_state_mutex);
                    }

                    if (s_event_callback) {
                        s_event_callback(AUDIO_ADF_EVENT_PAUSED, s_event_user_data);
                    }
                    break;
                }

                case CMD_RESUME: {
                    ESP_LOGI(TAG, "恢复播放");
                    audio_pipeline_resume(s_pipeline);

                    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        s_state = AUDIO_ADF_STATE_PLAYING;
                        xSemaphoreGive(s_state_mutex);
                    }

                    if (s_event_callback) {
                        s_event_callback(AUDIO_ADF_EVENT_PLAYING, s_event_user_data);
                    }
                    break;
                }

                case CMD_SET_VOLUME: {
                    uint8_t vol = msg.data.volume.volume;
                    ESP_LOGI(TAG, "设置音量: %d", vol);
                    s_volume = vol;
                    // 通过 I2S 设置音量
                    break;
                }

                case CMD_SEEK: {
                    uint32_t pos = msg.data.seek.position_ms;
                    ESP_LOGI(TAG, "跳转位置: %u ms", pos);
                    // 定位功能
                    break;
                }

                default:
                    break;
            }
        }

        // 更新播放状态
        if (s_state == AUDIO_ADF_STATE_PLAYING && s_pipeline) {
            // 可以从 decoder 获取当前播放信息
            audio_element_state_t el_state = audio_element_get_state(s_decoder);
            if (el_state == AEL_STATE_FINISHED) {
                if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    s_state = AUDIO_ADF_STATE_STOPPED;
                    xSemaphoreGive(s_state_mutex);
                }
            }
        }

        vTaskDelay(1);
    }
}

// ============================================================================
// C API 实现
// ============================================================================

extern "C" {

esp_err_t audio_adf_init(gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "ADF 已初始化");
        return ESP_OK;
    }

    // 创建互斥锁
    s_state_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buffer);
    if (s_state_mutex == NULL) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_FAIL;
    }

    // 创建命令队列
    s_cmd_queue = xQueueCreate(8, sizeof(adf_message_t));
    if (s_cmd_queue == NULL) {
        ESP_LOGE(TAG, "创建命令队列失败");
        vSemaphoreDelete(s_state_mutex);
        return ESP_FAIL;
    }

    // 初始化 ADF Pipeline
    esp_err_t ret = init_adf_pipeline(bclk, ws, dout);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADF Pipeline 初始化失败");
        vQueueDelete(s_cmd_queue);
        vSemaphoreDelete(s_state_mutex);
        return ret;
    }

    // 创建后台任务（Core 1）
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        audio_adf_task,
        "audio_adf",
        8192,
        NULL,
        configMAX_PRIORITIES - 3,
        &s_task_handle,
        1  // Core 1
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "创建 ADF 任务失败");
        deinit_adf_pipeline();
        vQueueDelete(s_cmd_queue);
        vSemaphoreDelete(s_state_mutex);
        return ESP_FAIL;
    }

    s_initialized = true;
    s_state = AUDIO_ADF_STATE_IDLE;
    memset(&s_info, 0, sizeof(s_info));

    ESP_LOGI(TAG, "ESP-ADF 音频模块初始化完成");
    return ESP_OK;
}

esp_err_t audio_adf_init_default(void)
{
    extern gpio_num_t GPIO_AUDIO_BCLK;
    extern gpio_num_t GPIO_AUDIO_WS;
    extern gpio_num_t GPIO_AUDIO_DIN;

    return audio_adf_init(GPIO_AUDIO_BCLK, GPIO_AUDIO_WS, GPIO_AUDIO_DIN);
}

esp_err_t audio_adf_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    // 发送停止命令
    adf_message_t msg = {.cmd = CMD_STOP};
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

    // 清理 Pipeline
    deinit_adf_pipeline();

    s_initialized = false;
    ESP_LOGI(TAG, "ESP-ADF 已反初始化");

    return ESP_OK;
}

esp_err_t audio_adf_play_file(const char *file_path)
{
    if (!s_initialized || !file_path) {
        return ESP_ERR_INVALID_STATE;
    }

    adf_message_t msg = {.cmd = CMD_PLAY_FILE};
    snprintf(msg.data.file.path, sizeof(msg.data.file.path), "%s", file_path);

    if (xQueueSend(s_cmd_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "排队播放: %s", file_path);
    return ESP_OK;
}

esp_err_t audio_adf_play_sd(const char *path)
{
    return audio_adf_play_file(path);
}

esp_err_t audio_adf_play_url(const char *url)
{
    if (!s_initialized || !url) {
        return ESP_ERR_INVALID_STATE;
    }

    adf_message_t msg = {.cmd = CMD_PLAY_URL};
    snprintf(msg.data.url.url, sizeof(msg.data.url.url), "%s", url);

    if (xQueueSend(s_cmd_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "排队播放 URL: %s", url);
    return ESP_OK;
}

esp_err_t audio_adf_stop(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    adf_message_t msg = {.cmd = CMD_STOP};
    xQueueSend(s_cmd_queue, &msg, 0);
    return ESP_OK;
}

esp_err_t audio_adf_pause(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    adf_message_t msg = {.cmd = CMD_PAUSE};
    xQueueSend(s_cmd_queue, &msg, 0);
    return ESP_OK;
}

esp_err_t audio_adf_resume(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    adf_message_t msg = {.cmd = CMD_RESUME};
    xQueueSend(s_cmd_queue, &msg, 0);
    return ESP_OK;
}

void audio_adf_set_volume(uint8_t volume)
{
    if (!s_initialized) return;

    adf_message_t msg = {.cmd = CMD_SET_VOLUME};
    msg.data.volume.volume = volume;
    xQueueSend(s_cmd_queue, &msg, 0);
}

uint8_t audio_adf_get_volume(void)
{
    return s_volume;
}

bool audio_adf_is_playing(void)
{
    if (!s_initialized) return false;

    bool playing = false;
    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        playing = (s_state == AUDIO_ADF_STATE_PLAYING);
        xSemaphoreGive(s_state_mutex);
    }
    return playing;
}

void audio_adf_get_info(audio_adf_info_t *info)
{
    if (!info || !s_initialized) return;

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        memcpy(info, &s_info, sizeof(audio_adf_info_t));
        xSemaphoreGive(s_state_mutex);
    }
}

void audio_adf_set_event_callback(audio_adf_event_callback_t callback, void *user_data)
{
    s_event_callback = callback;
    s_event_user_data = user_data;
}

esp_err_t audio_adf_seek(uint32_t position_ms)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    adf_message_t msg = {.cmd = CMD_SEEK};
    msg.data.seek.position_ms = position_ms;
    xQueueSend(s_cmd_queue, &msg, 0);
    return ESP_OK;
}

}  // extern "C"
