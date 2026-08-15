/**
 * @file audio.c
 * @brief 音频模块实现（纯净无重复版）
 */
#include "audio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "sdcard/sdcard.h"
#include <string.h>
#include <strings.h>

static const char *TAG = "AUDIO";

static bool s_initialized = false;
static audio_state_t s_state = AUDIO_STATE_UNINIT;
static i2s_chan_handle_t s_i2s_handle = NULL;
static uint8_t s_volume_percent = 80;
static volatile bool s_stop_requested = false;

esp_err_t audio_init(void)
{
    if (s_initialized) return ESP_OK;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;

    if (i2s_new_channel(&chan_cfg, &s_i2s_handle, NULL) != ESP_OK) {
        return ESP_FAIL;
    }

    i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100);
    i2s_std_slot_config_t slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);

    i2s_std_config_t std_cfg = {
        .clk_cfg = clk_cfg,
        .slot_cfg = slot_cfg,
        .gpio_cfg = {
            .bclk = AUDIO_DEFAULT_BCLK,
            .ws = AUDIO_DEFAULT_WS,
            .dout = AUDIO_DEFAULT_DIN,
            .din = I2S_GPIO_UNUSED,
        },
    };

    if (i2s_channel_init_std_mode(s_i2s_handle, &std_cfg) != ESP_OK) {
        i2s_del_channel(s_i2s_handle);
        s_i2s_handle = NULL;
        return ESP_FAIL;
    }

    i2s_channel_enable(s_i2s_handle);

    // 初始化并启用音频输出
    gpio_set_direction(AUDIO_DEFAULT_SD, GPIO_MODE_OUTPUT);
    gpio_set_level(AUDIO_DEFAULT_SD, 1);  // SD拉高使能MAX98357

    // 初始化GAIN引脚并设置默认增益
    gpio_set_direction(AUDIO_DEFAULT_GAIN, GPIO_MODE_OUTPUT);
    audio_set_gain(AUDIO_GAIN_9DB);

    s_initialized = true;
    s_state = AUDIO_STATE_READY;
    ESP_LOGI(TAG, "音频模块初始化成功 (SD=ON, GAIN=9dB)");
    return ESP_OK;
}

esp_err_t audio_deinit(void)
{
    if (!s_initialized) return ESP_OK;
    if (s_i2s_handle) {
        i2s_channel_disable(s_i2s_handle);
        i2s_del_channel(s_i2s_handle);
        s_i2s_handle = NULL;
    }
    s_initialized = false;
    s_state = AUDIO_STATE_UNINIT;
    return ESP_OK;
}

audio_state_t audio_get_state(void) { return s_state; }
bool audio_is_initialized(void) { return s_initialized; }

uint8_t audio_get_volume_percent(void) { return s_volume_percent; }
esp_err_t audio_set_volume_percent(uint8_t volume)
{
    if (volume > 100) return ESP_ERR_INVALID_ARG;
    s_volume_percent = volume;
    return ESP_OK;
}

esp_err_t audio_stop(void)
{
    // 先设置停止标志，播放循环会检测到这个标志并退出
    s_stop_requested = true;

    // 立即静音：拉低 SD 引脚关闭 MAX98357 输出
    gpio_set_level(AUDIO_DEFAULT_SD, 0);

    // 禁用 I2S 通道停止 DMA
    if (s_i2s_handle != NULL) {
        i2s_channel_disable(s_i2s_handle);
    }

    // 短暂等待确保静音生效
    vTaskDelay(pdMS_TO_TICKS(5));

    // 重新拉高 SD 引脚使能输出（但不重启发送）
    gpio_set_level(AUDIO_DEFAULT_SD, 1);

    // 重新启用 I2S 通道
    if (s_i2s_handle != NULL) {
        i2s_channel_enable(s_i2s_handle);
    }

    // 最后更新状态（播放循环退出后再设置）
    s_state = AUDIO_STATE_READY;

    ESP_LOGI(TAG, "音频已停止 (SD静音)");

    return ESP_OK;
}

int32_t audio_play_mp3(const char *file_path)
{
    if (!file_path) return -1;
    ESP_LOGW(TAG, "当前版本暂未启用MP3解码: %s", file_path);
    return -1;
}

/* 唯一的 audio_play_wav 定义 */
int32_t audio_play_wav(const char *file_path)
{
    if (!s_initialized || file_path == NULL) return -1;

    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "无法打开 WAV 文件: %s", file_path);
        return -1;
    }

    uint8_t header[44];
    size_t read_len = fread(header, 1, 44, fp);
    if (read_len < 44) {
        fclose(fp);
        return -1;
    }

    // 重置停止标志
    s_stop_requested = false;
    ESP_LOGI(TAG, "开始播放 WAV 音频: %s", file_path);

    size_t buffer_size = 4096;
    uint8_t *buffer = (uint8_t*)malloc(buffer_size);
    if (buffer == NULL) {
        fclose(fp);
        return -1;
    }

    s_state = AUDIO_STATE_PLAYING;

    while (s_state == AUDIO_STATE_PLAYING) {
        // 每次循环都检查停止标志
        if (s_stop_requested) {
            ESP_LOGI(TAG, ">>> 检测到停止请求，退出播放循环");
            break;
        }

        size_t bytes_read = fread(buffer, 1, buffer_size, fp);
        if (bytes_read == 0) break;

        size_t bytes_written = 0;
        // 使用较短的超时时间，以便更快响应停止
        i2s_channel_write(s_i2s_handle, buffer, bytes_read, &bytes_written, pdMS_TO_TICKS(200));
    }

    ESP_LOGI(TAG, ">>> 退出循环, stop_requested=%d, state=%d", s_stop_requested, s_state);

    // 发送静音数据，清除 DMA 缓冲区中的残留音频
    if (s_stop_requested) {
        uint8_t *silence = (uint8_t*)calloc(1024, 1);
        if (silence) {
            for (int i = 0; i < 3; i++) {
                size_t dummy = 0;
                i2s_channel_write(s_i2s_handle, silence, 1024, &dummy, pdMS_TO_TICKS(50));
            }
            free(silence);
        }
    }

    free(buffer);
    fclose(fp);
    s_state = AUDIO_STATE_READY;

    if (s_stop_requested) {
        ESP_LOGI(TAG, "WAV 播放已停止");
        return -2;
    }

    ESP_LOGI(TAG, "WAV 播放完毕");
    return 0;
}

/* 播放内存中的音频数据（用于测试音调） */
esp_err_t audio_play_wav_data(const int16_t *data, size_t size)
{
    if (!s_initialized || data == NULL || size == 0) return ESP_ERR_INVALID_ARG;
    if (s_i2s_handle == NULL) return ESP_ERR_INVALID_STATE;

    s_state = AUDIO_STATE_PLAYING;

    // 计算播放时间（44.1kHz 16bit stereo = 176400 bytes/秒）
    uint32_t duration_ms = (size * 1000) / 176400;

    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(s_i2s_handle, data, size, &bytes_written, pdMS_TO_TICKS(5000));

    // 等待音频播放完成
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(duration_ms + 50));
    }

    s_state = AUDIO_STATE_READY;
    return ret;
}

int32_t audio_play_file(const char *file_path)
{
    if (!s_initialized || file_path == NULL) return -1;
    
    const char *ext = strrchr(file_path, '.');
    if (ext && strcasecmp(ext, ".wav") == 0) {
        return audio_play_wav(file_path);
    }
    
    return audio_play_mp3(file_path);
}

static audio_gain_t s_current_gain = AUDIO_GAIN_9DB;

esp_err_t audio_set_gain(audio_gain_t gain)
{
    if (gain > AUDIO_GAIN_12DB) return ESP_ERR_INVALID_ARG;

    // MAX98357 GAIN引脚控制：
    // GAIN=高电平 → 9dB 或 12dB (取决于SEL引脚)
    // GAIN=低电平 → 3dB 或 6dB (取决于SEL引脚)
    // 标准接法: SEL接地 → GAIN高=9dB, GAIN低=6dB
    gpio_set_level(AUDIO_DEFAULT_GAIN, gain == AUDIO_GAIN_3DB || gain == AUDIO_GAIN_9DB ? 1 : 0);

    s_current_gain = gain;
    ESP_LOGI(TAG, "增益设置为: %ddB", gain == AUDIO_GAIN_3DB ? 3 : 
                             gain == AUDIO_GAIN_6DB ? 6 :
                             gain == AUDIO_GAIN_9DB ? 9 : 12);
    return ESP_OK;
}

audio_gain_t audio_get_gain(void)
{
    return s_current_gain;
}

esp_err_t audio_enable_output(void)
{
    gpio_set_level(AUDIO_DEFAULT_SD, 1);  // SD拉高使能输出
    return ESP_OK;
}

esp_err_t audio_disable_output(void)
{
    gpio_set_level(AUDIO_DEFAULT_SD, 0);  // SD拉低关闭输出
    return ESP_OK;
}

esp_err_t audio_set_volume(uint8_t volume) { return audio_set_volume_percent(volume); }