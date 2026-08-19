#include "audio/mic.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

static const char *TAG = "MIC_OFFICIAL";

static i2s_chan_handle_t s_rx_handle = NULL;
static bool g_mic_initialized = false;
static bool g_mic_testing = false;
static volatile int g_last_sound_level = 0;
static mic_data_callback_t g_mic_cb = NULL;

static int g_config_sample_rate = 44100;
static int g_config_shift_bits = 8;
static float g_config_vol_scale = 0.2f;

static int32_t s_pcm32_buf[256];
static int16_t s_pcm16_buf[128];

// -----------------------------------------------------------------------------
// 参数设置
// -----------------------------------------------------------------------------
void mic_set_params(int sample_rate, int shift_bits, float volume_scale) {
    if (sample_rate > 8000 && sample_rate <= 48000)
        g_config_sample_rate = sample_rate;
    if (shift_bits >= 0 && shift_bits <= 24)
        g_config_shift_bits = shift_bits;
    if (volume_scale >= 0.0f && volume_scale <= 2.0f)
        g_config_vol_scale = volume_scale;
    ESP_LOGI(TAG, "更新麦克风参数: 采样率=%d, 移位=%d, 缩放=%.2f",
             g_config_sample_rate, g_config_shift_bits, g_config_vol_scale);
}

void mic_get_params(int *sample_rate, int *shift_bits, float *volume_scale) {
    if (sample_rate) *sample_rate = g_config_sample_rate;
    if (shift_bits) *shift_bits = g_config_shift_bits;
    if (volume_scale) *volume_scale = g_config_vol_scale;
}

// -----------------------------------------------------------------------------
// 麦克风读取任务
// -----------------------------------------------------------------------------
static void mic_read_task(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "原生 I2S 麦克风采集 Task 启动 (Core %d)", xPortGetCoreID());

    uint32_t log_tick_counter = 0;

    while (1) {
        if (g_mic_testing && s_rx_handle != NULL) {
            size_t bytes_read = 0;
            int ret = i2s_channel_read(s_rx_handle, s_pcm32_buf, sizeof(s_pcm32_buf),
                                       &bytes_read, pdMS_TO_TICKS(100));

            if (ret == ESP_OK && bytes_read > 0) {
                size_t out_frames = bytes_read / sizeof(int32_t) / 2;
                double sum = 0.0;
                int shift = g_config_shift_bits;
                float vol = g_config_vol_scale;

                for (size_t i = 0; i < out_frames; i++) {
                    int32_t raw_sample = s_pcm32_buf[i * 2];
                    int32_t shifted = raw_sample >> shift;
                    int32_t scaled = (int32_t)(shifted * vol);

                    // 软限幅，防止大声破音
                    if (scaled > 32767) scaled = 32767;
                    if (scaled < -32768) scaled = -32768;

                    s_pcm16_buf[i] = (int16_t)scaled;
                    sum += (double)s_pcm16_buf[i] * s_pcm16_buf[i];
                }

                double rms = sqrt(sum / (out_frames > 0 ? out_frames : 1));
                int level = (int)(rms / 1000.0);
                if (level > 100) level = 100;
                g_last_sound_level = level;

                if (++log_tick_counter >= 170) {
                    ESP_LOGI(TAG, "当前麦克风响度: [%d%%] | RMS: %.2f", g_last_sound_level, rms);
                    log_tick_counter = 0;
                }

                // 实时音频数据回调
                if (g_mic_cb != NULL) {
                    g_mic_cb((const uint8_t *)s_pcm16_buf, out_frames * sizeof(int16_t));
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        } else {
            g_last_sound_level = 0;
            log_tick_counter = 0;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// -----------------------------------------------------------------------------
// 初始化
// -----------------------------------------------------------------------------
esp_err_t mic_init(void) {
    if (g_mic_initialized)
        return ESP_OK;

    ESP_LOGI(TAG, "正在初始化原生高性能麦克风 (44100Hz 同步模式)...");

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 6;
    chan_cfg.dma_frame_num = 240;

    if (i2s_new_channel(&chan_cfg, NULL, &s_rx_handle) != ESP_OK) {
        ESP_LOGE(TAG, "创建 I2S 接收通道失败");
        return ESP_FAIL;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_41,
            .ws   = GPIO_NUM_45,
            .dout = I2S_GPIO_UNUSED,
            .din  = GPIO_NUM_14,
        },
    };

    if (i2s_channel_init_std_mode(s_rx_handle, &std_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "初始化 I2S STD 模式失败");
        return ESP_FAIL;
    }

    if (i2s_channel_enable(s_rx_handle) != ESP_OK) {
        ESP_LOGE(TAG, "使能 I2S 接收通道失败");
        return ESP_FAIL;
    }

    g_mic_initialized = true;
    xTaskCreatePinnedToCore(mic_read_task, "mic_read_task", 4096, NULL, 2, NULL, 1);
    return ESP_OK;
}

// -----------------------------------------------------------------------------
// 对外接口
// -----------------------------------------------------------------------------
esp_err_t mic_read(void *dest, size_t size, size_t *bytes_read) {
    if (!s_rx_handle)
        return ESP_ERR_INVALID_STATE;
    return i2s_channel_read(s_rx_handle, dest, size, bytes_read, pdMS_TO_TICKS(100));
}

int mic_get_sound_level(void) {
    return (g_mic_initialized && g_mic_testing) ? g_last_sound_level : 0;
}

void mic_set_testing(bool enable) {
    g_mic_testing = enable;

    if (enable) {
        if (s_rx_handle) {
            i2s_channel_enable(s_rx_handle);
        }
        ESP_LOGI(TAG, "已开始麦克风读取测试...");
    } else {
        g_last_sound_level = 0;
        if (s_rx_handle) {
            i2s_channel_disable(s_rx_handle);
        }
    }
}

bool mic_is_testing(void) {
    return g_mic_testing;
}

void mic_register_callback(mic_data_callback_t cb) {
    g_mic_cb = cb;
}