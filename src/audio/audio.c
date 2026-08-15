/**
 * @file audio.c
 * @brief MAX98357 音频模块核心实现（安全无崩溃版）
 */
#include "audio.h"
#include "audio_mp3.h"
#include "minimp3.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sdcard/sdcard.h"
#include <string.h>
#include <strings.h>

static const char *TAG = "AUDIO";

#define AUDIO_DEFAULT_BCLK    GPIO_AUDIO_BCLK
#define AUDIO_DEFAULT_WS      GPIO_AUDIO_WS
#define AUDIO_DEFAULT_DIN     GPIO_AUDIO_DIN

#define AUDIO_DEFAULT_GAIN    GPIO_NUM_45   
#define AUDIO_DEFAULT_SD      GPIO_AUDIO_SD 

#define AUDIO_WAV_HEADER_SIZE     44      
#define AUDIO_TIMEOUT_MS         5000     

static bool s_initialized = false;
static audio_state_t s_state = AUDIO_STATE_UNINIT;
static SemaphoreHandle_t s_mutex = NULL;

static audio_pin_config_t s_pin_config = {0};
static audio_play_config_t s_play_config = {0};
static audio_gain_t s_current_gain = AUDIO_GAIN_9DB;
static uint8_t s_volume_percent = 80;  

static i2s_chan_handle_t s_i2s_handle = NULL;
static StaticSemaphore_t s_mutex_buffer;

typedef struct PACKED {
    uint8_t  riff[4];         
    uint32_t file_size;       
    uint8_t  wave[4];         
    uint8_t  fmt[4];          
    uint32_t fmt_size;        
    uint16_t audio_format;     
    uint16_t num_channels;     
    uint32_t sample_rate;      
    uint32_t byte_rate;       
    uint16_t block_align;      
    uint16_t bits_per_sample;  
    uint8_t  data[4];         
    uint32_t data_size;        
} wav_header_t;

static audio_sample_rate_t parse_sample_rate(uint32_t rate)
{
    switch (rate) {
        case 8000:   return AUDIO_SAMPLE_RATE_8K;
        case 16000:  return AUDIO_SAMPLE_RATE_16K;
        case 22050:  return AUDIO_SAMPLE_RATE_22K;
        case 44100:  return AUDIO_SAMPLE_RATE_44K;
        case 48000:  return AUDIO_SAMPLE_RATE_48K;
        default:     return AUDIO_SAMPLE_RATE_44K;
    }
}

static esp_err_t set_gain_level(audio_gain_t gain)
{
    if (!s_pin_config.gain_enable) {
        return ESP_OK;
    }

    gpio_mode_t mode;
    gpio_num_t pin = s_pin_config.gain;
    uint32_t level;

    switch (gain) {
        case AUDIO_GAIN_3DB:   
            mode = GPIO_MODE_OUTPUT;
            level = 1;
            break;
        case AUDIO_GAIN_6DB:   
            mode = GPIO_MODE_OUTPUT;
            level = 0;
            break;
        case AUDIO_GAIN_9DB:   
            mode = GPIO_MODE_INPUT;
            level = 0;
            break;
        case AUDIO_GAIN_12DB:  
            mode = GPIO_MODE_OUTPUT;
            level = 1;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = mode,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) return ret;

    if (mode == GPIO_MODE_OUTPUT) {
        gpio_set_level(pin, level);
    }
    return ESP_OK;
}

static esp_err_t config_i2s_pins(void)
{
    gpio_config_t bclk_conf = {
        .pin_bit_mask = (1ULL << s_pin_config.bclk),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    if (gpio_config(&bclk_conf) != ESP_OK) return ESP_FAIL;

    gpio_config_t ws_conf = {
        .pin_bit_mask = (1ULL << s_pin_config.ws),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    if (gpio_config(&ws_conf) != ESP_OK) return ESP_FAIL;

    gpio_config_t din_conf = {
        .pin_bit_mask = (1ULL << s_pin_config.din),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    if (gpio_config(&din_conf) != ESP_OK) return ESP_FAIL;

    return ESP_OK;
}

static esp_err_t create_i2s_channel(void)
{
    esp_err_t ret;

    if (s_i2s_handle != NULL) {
        i2s_channel_disable(s_i2s_handle);
        i2s_del_channel(s_i2s_handle);
        s_i2s_handle = NULL;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;

    ret = i2s_new_channel(&chan_cfg, &s_i2s_handle, NULL);
    if (ret != ESP_OK) return ret;

    i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(s_play_config.sample_rate);
    i2s_std_slot_config_t slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                            (s_play_config.channel == 2) ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO);

    i2s_std_config_t std_cfg = {
        .clk_cfg = clk_cfg,
        .slot_cfg = slot_cfg,
        .gpio_cfg = {
            .bclk = s_pin_config.bclk,
            .ws = s_pin_config.ws,
            .dout = s_pin_config.din,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(s_i2s_handle, &std_cfg);
    if (ret != ESP_OK) {
        i2s_del_channel(s_i2s_handle);
        s_i2s_handle = NULL;
        return ret;
    }

    return ESP_OK;
}

esp_err_t audio_init_with_pins(const audio_pin_config_t *pin_config)
{
    if (pin_config == NULL) return ESP_ERR_INVALID_ARG;

    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buffer);
        if (s_mutex == NULL) return ESP_FAIL;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    memcpy(&s_pin_config, pin_config, sizeof(audio_pin_config_t));

    if (s_pin_config.sd_enable && s_pin_config.sd != GPIO_NUM_NC) {
        gpio_config_t sd_conf = {
            .pin_bit_mask = (1ULL << s_pin_config.sd),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&sd_conf);
        gpio_set_level(s_pin_config.sd, 1);
    }

    if (s_pin_config.gain_enable && s_pin_config.gain != GPIO_NUM_NC) {
        set_gain_level(s_current_gain);
    }

    config_i2s_pins();

    s_play_config.sample_rate = AUDIO_SAMPLE_RATE_44K;
    s_play_config.format = AUDIO_FMT_16BIT;
    s_play_config.channel = 2;
    s_play_config.gain = AUDIO_GAIN_9DB;

    esp_err_t ret = create_i2s_channel();
    if (ret == ESP_OK) {
        ret = i2s_channel_enable(s_i2s_handle);
    }

    if (ret == ESP_OK) {
        s_initialized = true;
        s_state = AUDIO_STATE_READY;
        ESP_LOGI(TAG, "音频模块初始化成功");
    }

    xSemaphoreGive(s_mutex);
    return ret;
}

esp_err_t audio_init(void)
{
    audio_pin_config_t default_pins = {
        .bclk = AUDIO_DEFAULT_BCLK,
        .ws = AUDIO_DEFAULT_WS,
        .din = AUDIO_DEFAULT_DIN,
        .gain = AUDIO_DEFAULT_GAIN,
        .sd = AUDIO_DEFAULT_SD,
        .gain_enable = true,
        .sd_enable = true
    };
    return audio_init_with_pins(&default_pins);
}

esp_err_t audio_deinit(void)
{
    if (!s_initialized) return ESP_OK;

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    audio_stop();

    if (s_i2s_handle != NULL) {
        i2s_channel_disable(s_i2s_handle);
        i2s_del_channel(s_i2s_handle);
        s_i2s_handle = NULL;
    }

    s_initialized = false;
    s_state = AUDIO_STATE_UNINIT;

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

audio_state_t audio_get_state(void)
{
    return s_state;
}

bool audio_is_initialized(void)
{
    return s_initialized;
}

esp_err_t audio_reconfig_pins(const audio_pin_config_t *pin_config)
{
    if (pin_config == NULL || !s_initialized) return ESP_ERR_INVALID_ARG;

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    audio_stop();
    memcpy(&s_pin_config, pin_config, sizeof(audio_pin_config_t));

    esp_err_t ret = create_i2s_channel();
    if (ret == ESP_OK) ret = i2s_channel_enable(s_i2s_handle);

    xSemaphoreGive(s_mutex);
    return ret;
}

esp_err_t audio_get_pin_config(audio_pin_config_t *pin_config)
{
    if (pin_config == NULL) return ESP_ERR_INVALID_ARG;
    memcpy(pin_config, &s_pin_config, sizeof(audio_pin_config_t));
    return ESP_OK;
}

esp_err_t audio_set_gain(audio_gain_t gain)
{
    s_current_gain = gain;
    return set_gain_level(gain);
}

audio_gain_t audio_get_gain(void)
{
    return s_current_gain;
}

uint8_t audio_get_volume_percent(void)
{
    return s_volume_percent;
}

esp_err_t audio_set_volume_percent(uint8_t volume)
{
    if (volume > 100) return ESP_ERR_INVALID_ARG;
    s_volume_percent = volume;
    ESP_LOGI(TAG, "软件音量设置为: %d%%", volume);
    return ESP_OK;
}

static void apply_volume_to_data(int16_t *data, size_t len, uint8_t channels)
{
    if (s_volume_percent >= 100) return;

    if (s_volume_percent == 0) {
        memset(data, 0, len);
        return;
    }

    uint16_t scale = (s_volume_percent * 256) / 100;
    size_t sample_count = len / sizeof(int16_t);

    for (size_t i = 0; i < sample_count; i++) {
        int32_t scaled = (int32_t)data[i] * scale / 256;
        data[i] = (int16_t)scaled;
    }
}

/* 流式 MP3 播放核心 */
typedef struct {
    uint8_t channels;
    uint32_t sample_rate;
} mp3_stream_ctx_t;

/* 使用堆内存动态申请，避开全局/栈内存边界越界风险 */
static void mp3_stream_pcm_callback(const uint8_t *pcm_data, size_t pcm_size, void *user_data)
{
    mp3_stream_ctx_t *ctx = (mp3_stream_ctx_t *)user_data;
    if (ctx == NULL || pcm_data == NULL || pcm_size == 0) return;

    if (s_state != AUDIO_STATE_PLAYING) return;

    int16_t *pcm_vol_buf = (int16_t *)malloc(pcm_size);
    if (pcm_vol_buf == NULL) return;

    memcpy(pcm_vol_buf, pcm_data, pcm_size);
    apply_volume_to_data(pcm_vol_buf, pcm_size, ctx->channels);

    size_t bytes_written = 0;
    i2s_channel_write(s_i2s_handle, pcm_vol_buf, pcm_size, &bytes_written, pdMS_TO_TICKS(1000));

    free(pcm_vol_buf);
}

int32_t audio_play_mp3(const char *file_path)
{
    if (!s_initialized || file_path == NULL) return -1;

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "获取音频锁超时");
        return -1;
    }

    char full_path[256];
    if (file_path[0] == '/') {
        snprintf(full_path, sizeof(full_path), "%s", file_path);
    } else {
        const char *mount_point = sdcard_get_mount_point();
        snprintf(full_path, sizeof(full_path), "%s/%s", mount_point, file_path);
    }

    ESP_LOGI(TAG, "开始流式播放 MP3: %s", full_path);

    mp3_stream_ctx_t stream_ctx = {
        .channels = 2,
        .sample_rate = 44100
    };

    audio_mp3_info_t mp3_info = {0};
    s_state = AUDIO_STATE_PLAYING;

    /* 解码并播放 */
    esp_err_t ret = audio_mp3_decode_file(full_path, mp3_stream_pcm_callback, &stream_ctx, &mp3_info);

    s_state = AUDIO_STATE_READY;

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MP3 解码或播放出错");
        xSemaphoreGive(s_mutex);
        return -1;
    }

    ESP_LOGI(TAG, "MP3 流式播放完成, 时长: %d ms", mp3_info.duration_ms);
    xSemaphoreGive(s_mutex);
    return mp3_info.duration_ms;
}

esp_err_t audio_play_data(const uint8_t *data, size_t len, const audio_play_config_t *config)
{
    if (!s_initialized || data == NULL || len == 0) return ESP_ERR_INVALID_ARG;

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (config != NULL) {
        memcpy(&s_play_config, config, sizeof(audio_play_config_t));
        create_i2s_channel();
        i2s_channel_enable(s_i2s_handle);
    }

    uint8_t *playback_data = (uint8_t*)malloc(len);
    if (playback_data == NULL) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NO_MEM;
    }

    memcpy(playback_data, data, len);
    apply_volume_to_data((int16_t*)playback_data, len, s_play_config.channel);

    s_state = AUDIO_STATE_PLAYING;

    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(s_i2s_handle, playback_data, len, &bytes_written, pdMS_TO_TICKS(5000));

    free(playback_data);
    s_state = AUDIO_STATE_READY;
    xSemaphoreGive(s_mutex);

    return ret;
}

esp_err_t audio_get_wav_info(const char *file_path, audio_file_info_t *info)
{
    if (file_path == NULL || info == NULL) return ESP_ERR_INVALID_ARG;

    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL) return ESP_FAIL;

    uint8_t header[AUDIO_WAV_HEADER_SIZE];
    size_t read_len = fread(header, 1, AUDIO_WAV_HEADER_SIZE, fp);
    fclose(fp);

    if (read_len < AUDIO_WAV_HEADER_SIZE) return ESP_FAIL;

    wav_header_t *wav = (wav_header_t*)header;
    if (memcmp(wav->riff, "RIFF", 4) != 0 || memcmp(wav->wave, "WAVE", 4) != 0) {
        return ESP_FAIL;
    }

    info->filename = strdup(file_path);
    info->size = wav->file_size + 8;
    info->sample_rate = parse_sample_rate(wav->sample_rate);
    info->channels = wav->num_channels;
    info->bits_per_sample = wav->bits_per_sample;
    info->byte_rate = wav->byte_rate;

    if (wav->byte_rate > 0) {
        info->duration_ms = (wav->data_size * 1000) / wav->byte_rate;
    }

    return ESP_OK;
}

void audio_free_wav_info(audio_file_info_t *info)
{
    if (info != NULL && info->filename != NULL) {
        free(info->filename);
        info->filename = NULL;
    }
}

int32_t audio_play_wav(const char *file_path)
{
    if (!s_initialized || file_path == NULL) return -1;

    audio_file_info_t file_info = {0};
    esp_err_t ret = audio_get_wav_info(file_path, &file_info);
    if (ret != ESP_OK) return -1;

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS)) != pdTRUE) {
        audio_free_wav_info(&file_info);
        return -1;
    }

    char full_path[256];
    if (file_path[0] == '/') {
        snprintf(full_path, sizeof(full_path), "%s", file_path);
    } else {
        const char *mount_point = sdcard_get_mount_point();
        snprintf(full_path, sizeof(full_path), "%s/%s", mount_point, file_path);
    }

    FILE *fp = fopen(full_path, "rb");
    if (fp == NULL) {
        xSemaphoreGive(s_mutex);
        audio_free_wav_info(&file_info);
        return -1;
    }

    fseek(fp, AUDIO_WAV_HEADER_SIZE, SEEK_SET);

    size_t buffer_size = 2048;
    uint8_t *playback_buffer = (uint8_t*)malloc(buffer_size);
    if (playback_buffer == NULL) {
        fclose(fp);
        xSemaphoreGive(s_mutex);
        audio_free_wav_info(&file_info);
        return -1;
    }

    s_state = AUDIO_STATE_PLAYING;

    while (s_state == AUDIO_STATE_PLAYING) {
        size_t bytes_read = fread(playback_buffer, 1, buffer_size, fp);
        if (bytes_read == 0) break;

        apply_volume_to_data((int16_t*)playback_buffer, bytes_read, file_info.channels);

        size_t bytes_written = 0;
        ret = i2s_channel_write(s_i2s_handle, playback_buffer, bytes_read, &bytes_written, pdMS_TO_TICKS(1000));
        if (ret != ESP_OK) break;
    }

    free(playback_buffer);
    fclose(fp);

    s_state = AUDIO_STATE_READY;
    xSemaphoreGive(s_mutex);
    audio_free_wav_info(&file_info);

    return file_info.duration_ms;
}

int32_t audio_play_file(const char *file_path)
{
    if (file_path == NULL) return -1;

    size_t len = strlen(file_path);
    if (len > 4) {
        const char *ext = file_path + len - 4;
        if (strcasecmp(ext, ".mp3") == 0) {
            return audio_play_mp3(file_path);
        } else if (strcasecmp(ext, ".wav") == 0) {
            return audio_play_wav(file_path);
        }
    }

    return audio_play_wav(file_path);
}

esp_err_t audio_stop(void)
{
    if (!s_initialized) return ESP_OK;

    s_state = AUDIO_STATE_READY;
    if (s_i2s_handle != NULL) {
        i2s_channel_disable(s_i2s_handle);
        i2s_channel_enable(s_i2s_handle);
    }

    ESP_LOGI(TAG, "播放已停止");
    return ESP_OK;
}

esp_err_t audio_pause(void)
{
    s_state = AUDIO_STATE_PAUSED;
    return ESP_OK;
}

esp_err_t audio_resume(void)
{
    s_state = AUDIO_STATE_PLAYING;
    return ESP_OK;
}

esp_err_t audio_enable_output(void)
{
    if (!s_initialized || !s_pin_config.sd_enable || s_pin_config.sd == GPIO_NUM_NC) {
        return ESP_ERR_INVALID_STATE;
    }
    return gpio_set_level(s_pin_config.sd, 1);
}

esp_err_t audio_disable_output(void)
{
    if (!s_initialized || !s_pin_config.sd_enable || s_pin_config.sd == GPIO_NUM_NC) {
        return ESP_ERR_INVALID_STATE;
    }
    return gpio_set_level(s_pin_config.sd, 0);
}

esp_err_t audio_set_volume(uint8_t volume)
{
    return audio_set_volume_percent(volume);
}