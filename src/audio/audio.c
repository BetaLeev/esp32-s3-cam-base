/**
 * @file audio.c
 * @brief MAX98357 音频模块核心实现
 *
 * 使用ESP-IDF标准I2S STD接口，支持动态引脚配置
 * 遵循规则：禁止超过512字节的局部数组，大缓冲区使用malloc
 */
#include "audio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "AUDIO";

/* ========================================
 * 默认引脚配置（使用 hw_audio.h 中的定义）
 * ======================================== */
/**
 * @brief MAX98357 I2S 接口引脚
 * - BCLK: 位时钟
 * - WS: 字选择/LRC
 * - DIN: 串行数据输入
 */
#define AUDIO_DEFAULT_BCLK    GPIO_AUDIO_BCLK
#define AUDIO_DEFAULT_WS      GPIO_AUDIO_WS
#define AUDIO_DEFAULT_DIN     GPIO_AUDIO_DIN

/**
 * @brief MAX98357 控制引脚
 */
#define AUDIO_DEFAULT_GAIN    GPIO_AUDIO_GAIN
#define AUDIO_DEFAULT_SD     GPIO_AUDIO_SD

/* ========================================
 * 常量定义
 * ======================================== */
#define AUDIO_WAV_HEADER_SIZE     44      /**< WAV文件头大小 */
#define AUDIO_MAX_RETRY          3       /**< 最大重试次数 */
#define AUDIO_TIMEOUT_MS         100     /**< 操作超时时间 */

/* ========================================
 * 静态变量
 * ======================================== */
static bool s_initialized = false;
static audio_state_t s_state = AUDIO_STATE_UNINIT;
static SemaphoreHandle_t s_mutex = NULL;

static audio_pin_config_t s_pin_config = {0};
static audio_play_config_t s_play_config = {0};
static audio_gain_t s_current_gain = AUDIO_GAIN_9DB;

static i2s_chan_handle_t s_i2s_handle = NULL;
static StaticSemaphore_t s_mutex_buffer;

/* ========================================
 * WAV文件头解析结构体
 * ======================================== */
typedef struct PACKED {
    uint8_t  riff[4];         /* "RIFF" */
    uint32_t file_size;       /* 文件总长度-8 */
    uint8_t  wave[4];         /* "WAVE" */
    uint8_t  fmt[4];          /* "fmt " */
    uint32_t fmt_size;        /* 格式数据长度(通常16) */
    uint16_t audio_format;     /* 音频格式(1=PCM) */
    uint16_t num_channels;     /* 通道数 */
    uint32_t sample_rate;      /* 采样率 */
    uint32_t byte_rate;       /* 字节速率 */
    uint16_t block_align;      /* 数据块对齐 */
    uint16_t bits_per_sample;  /* 位深度 */
    uint8_t  data[4];         /* "data" */
    uint32_t data_size;        /* 数据大小 */
} wav_header_t;

/* ========================================
 * 辅助函数
 * ======================================== */

/**
 * @brief 线程安全的状态更新
 */
static void set_state(audio_state_t new_state)
{
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS)) == pdTRUE) {
        s_state = new_state;
        xSemaphoreGive(s_mutex);
    }
}

/**
 * @brief 解析采样率字符串
 */
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

/**
 * @brief 根据增益等级设置GAIN引脚电平
 */
static esp_err_t set_gain_level(audio_gain_t gain)
{
    if (!s_pin_config.gain_enable) {
        return ESP_OK;
    }

    gpio_mode_t mode;
    gpio_num_t pin = s_pin_config.gain;
    uint32_t level;

    switch (gain) {
        case AUDIO_GAIN_3DB:   /* GAIN接SD */
            mode = GPIO_MODE_OUTPUT;
            level = 1;  /* SD输出高电平 */
            break;
        case AUDIO_GAIN_6DB:   /* GAIN接GND */
            mode = GPIO_MODE_OUTPUT;
            level = 0;
            break;
        case AUDIO_GAIN_9DB:   /* GAIN浮空 */
            mode = GPIO_MODE_INPUT;
            level = 0;
            break;
        case AUDIO_GAIN_12DB:  /* GAIN接VDD */
            mode = GPIO_MODE_OUTPUT;
            level = 1;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }

    /* 配置引脚 */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = mode,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GAIN引脚配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    if (mode == GPIO_MODE_OUTPUT) {
        gpio_set_level(pin, level);
    }

    ESP_LOGI(TAG, "增益设置为: %ddB", gain == AUDIO_GAIN_3DB ? 3 :
                               gain == AUDIO_GAIN_6DB ? 6 :
                               gain == AUDIO_GAIN_9DB ? 9 : 12);
    return ESP_OK;
}

/**
 * @brief 配置I2S引脚
 */
static esp_err_t config_i2s_pins(void)
{
    esp_err_t ret;

    gpio_config_t bclk_conf = {
        .pin_bit_mask = (1ULL << s_pin_config.bclk),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&bclk_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BCLK引脚配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    gpio_config_t ws_conf = {
        .pin_bit_mask = (1ULL << s_pin_config.ws),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&ws_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WS引脚配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    gpio_config_t din_conf = {
        .pin_bit_mask = (1ULL << s_pin_config.din),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&din_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "DIN引脚配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2S引脚配置完成: BCLK=GPIO%d, WS=GPIO%d, DIN=GPIO%d",
             s_pin_config.bclk, s_pin_config.ws, s_pin_config.din);
    return ESP_OK;
}

/**
 * @brief 创建I2S通道
 */
static esp_err_t create_i2s_channel(void)
{
    esp_err_t ret;

    /* 如果已存在通道，先删除 */
    if (s_i2s_handle != NULL) {
        ESP_LOGW(TAG, "I2S通道已存在，先删除");
        i2s_del_channel(s_i2s_handle);
        s_i2s_handle = NULL;
    }

    /* 配置I2S通道 */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;  /* 自动清空缓冲区 */

    ret = i2s_new_channel(&chan_cfg, &s_i2s_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建I2S通道失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 配置I2S标准模式 */
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(s_play_config.sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, 
                                                         (s_play_config.channel == 2) ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO),
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
        ESP_LOGE(TAG, "初始化I2S标准模式失败: %s", esp_err_to_name(ret));
        i2s_del_channel(s_i2s_handle);
        s_i2s_handle = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "I2S通道创建成功");
    return ESP_OK;
}

/* ========================================
 * 核心接口实现
 * ======================================== */

esp_err_t audio_init(void)
{
    /* 使用默认引脚配置 */
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

esp_err_t audio_init_with_pins(const audio_pin_config_t *pin_config)
{
    if (pin_config == NULL) {
        ESP_LOGE(TAG, "引脚配置为空");
        return ESP_ERR_INVALID_ARG;
    }

    /* 检查必要引脚 */
    if (pin_config->bclk == GPIO_NUM_NC || 
        pin_config->ws == GPIO_NUM_NC || 
        pin_config->din == GPIO_NUM_NC) {
        ESP_LOGE(TAG, "BCLK、WS、DIN引脚必须配置");
        return ESP_ERR_INVALID_ARG;
    }

    /* 创建互斥锁 */
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buffer);
        if (s_mutex == NULL) {
            ESP_LOGE(TAG, "创建互斥锁失败");
            return ESP_FAIL;
        }
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "获取互斥锁超时");
        return ESP_ERR_TIMEOUT;
    }

    /* 保存引脚配置 */
    memcpy(&s_pin_config, pin_config, sizeof(audio_pin_config_t));

    /* 配置SD引脚（如果启用） */
    if (s_pin_config.sd_enable && s_pin_config.sd != GPIO_NUM_NC) {
        gpio_config_t sd_conf = {
            .pin_bit_mask = (1ULL << s_pin_config.sd),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        esp_err_t ret = gpio_config(&sd_conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SD引脚配置失败");
            xSemaphoreGive(s_mutex);
            return ret;
        }
        /* 默认使能输出 */
        gpio_set_level(s_pin_config.sd, 1);
        ESP_LOGI(TAG, "SD引脚配置完成: GPIO%d", s_pin_config.sd);
    }

    /* 配置GAIN引脚 */
    if (s_pin_config.gain_enable && s_pin_config.gain != GPIO_NUM_NC) {
        esp_err_t ret = set_gain_level(s_current_gain);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "GAIN引脚配置失败，继续使用默认增益");
        }
    }

    /* 配置I2S引脚 */
    esp_err_t ret = config_i2s_pins();
    if (ret != ESP_OK) {
        xSemaphoreGive(s_mutex);
        return ret;
    }

    /* 默认播放配置 */
    s_play_config.sample_rate = AUDIO_SAMPLE_RATE_44K;
    s_play_config.format = AUDIO_FMT_16BIT;
    s_play_config.channel = 1;
    s_play_config.gain = AUDIO_GAIN_9DB;

    /* 创建I2S通道 */
    ret = create_i2s_channel();
    if (ret != ESP_OK) {
        xSemaphoreGive(s_mutex);
        return ret;
    }

    /* 启动I2S通道 */
    ret = i2s_channel_enable(s_i2s_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动I2S通道失败");
        i2s_del_channel(s_i2s_handle);
        s_i2s_handle = NULL;
        xSemaphoreGive(s_mutex);
        return ret;
    }

    s_initialized = true;
    s_state = AUDIO_STATE_READY;

    ESP_LOGI(TAG, "音频模块初始化成功");
    ESP_LOGI(TAG, "引脚配置: BCLK=GPIO%d, WS=GPIO%d, DIN=GPIO%d, GAIN=GPIO%d, SD=GPIO%d",
             s_pin_config.bclk, s_pin_config.ws, s_pin_config.din,
             s_pin_config.gain, s_pin_config.sd);

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t audio_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    /* 停止播放 */
    audio_stop();

    /* 禁用I2S通道 */
    if (s_i2s_handle != NULL) {
        i2s_channel_disable(s_i2s_handle);
        i2s_del_channel(s_i2s_handle);
        s_i2s_handle = NULL;
    }

    s_initialized = false;
    s_state = AUDIO_STATE_UNINIT;

    xSemaphoreGive(s_mutex);

    /* 删除互斥锁 */
    if (s_mutex != NULL) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }

    ESP_LOGI(TAG, "音频模块已反初始化");
    return ESP_OK;
}

audio_state_t audio_get_state(void)
{
    audio_state_t state;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS)) == pdTRUE) {
        state = s_state;
        xSemaphoreGive(s_mutex);
    } else {
        state = AUDIO_STATE_ERROR;
    }
    return state;
}

bool audio_is_initialized(void)
{
    return s_initialized;
}

/* ========================================
 * 引脚配置接口实现
 * ======================================== */

esp_err_t audio_reconfig_pins(const audio_pin_config_t *pin_config)
{
    if (pin_config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        ESP_LOGE(TAG, "音频模块未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    /* 如果正在播放，先停止 */
    if (s_state == AUDIO_STATE_PLAYING) {
        audio_stop();
    }

    /* 保存新配置 */
    audio_pin_config_t old_config;
    memcpy(&old_config, &s_pin_config, sizeof(audio_pin_config_t));
    memcpy(&s_pin_config, pin_config, sizeof(audio_pin_config_t));

    /* 重新配置 */
    esp_err_t ret = ESP_OK;

    /* 配置SD引脚 */
    if (s_pin_config.sd_enable && s_pin_config.sd != GPIO_NUM_NC) {
        gpio_config_t sd_conf = {
            .pin_bit_mask = (1ULL << s_pin_config.sd),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        ret = gpio_config(&sd_conf);
        if (ret == ESP_OK) {
            gpio_set_level(s_pin_config.sd, 1);
        }
    }

    /* 重新创建I2S通道 */
    if (ret == ESP_OK) {
        ret = create_i2s_channel();
    }

    if (ret == ESP_OK) {
        ret = i2s_channel_enable(s_i2s_handle);
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "引脚配置已更新: BCLK=GPIO%d, WS=GPIO%d, DIN=GPIO%d",
                 s_pin_config.bclk, s_pin_config.ws, s_pin_config.din);
        s_state = AUDIO_STATE_READY;
    } else {
        /* 恢复原配置 */
        memcpy(&s_pin_config, &old_config, sizeof(audio_pin_config_t));
        ESP_LOGE(TAG, "引脚重新配置失败: %s", esp_err_to_name(ret));
    }

    xSemaphoreGive(s_mutex);
    return ret;
}

esp_err_t audio_get_pin_config(audio_pin_config_t *pin_config)
{
    if (pin_config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS)) == pdTRUE) {
        memcpy(pin_config, &s_pin_config, sizeof(audio_pin_config_t));
        xSemaphoreGive(s_mutex);
        return ESP_OK;
    }

    return ESP_ERR_TIMEOUT;
}

/* ========================================
 * 增益控制接口实现
 * ======================================== */

esp_err_t audio_set_gain(audio_gain_t gain)
{
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = set_gain_level(gain);
    if (ret == ESP_OK) {
        s_current_gain = gain;
        s_play_config.gain = gain;
    }

    xSemaphoreGive(s_mutex);
    return ret;
}

audio_gain_t audio_get_gain(void)
{
    audio_gain_t gain;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS)) == pdTRUE) {
        gain = s_current_gain;
        xSemaphoreGive(s_mutex);
    } else {
        gain = AUDIO_GAIN_9DB;
    }
    return gain;
}

esp_err_t audio_set_gain_pin_level(uint8_t level)
{
    /* 根据电平选择增益 */
    audio_gain_t gain;
    switch (level) {
        case 0:  gain = AUDIO_GAIN_6DB; break;  /* GND */
        case 1:  gain = AUDIO_GAIN_12DB; break; /* VDD */
        default: gain = AUDIO_GAIN_9DB; break;  /* 浮空 */
    }
    return audio_set_gain(gain);
}

/* ========================================
 * 播放控制接口实现
 * ======================================== */

esp_err_t audio_configure(const audio_play_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    /* 更新配置 */
    memcpy(&s_play_config, config, sizeof(audio_play_config_t));

    /* 重新创建I2S通道以应用新配置 */
    esp_err_t ret = create_i2s_channel();
    if (ret == ESP_OK) {
        ret = i2s_channel_enable(s_i2s_handle);
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "播放配置已更新: 采样率=%d, 通道=%d",
                 s_play_config.sample_rate, s_play_config.channel);
    }

    xSemaphoreGive(s_mutex);
    return ret;
}

/**
 * @brief 播放音频数据（内部函数）
 */
static esp_err_t play_audio_data_internal(const uint8_t *data, size_t len, 
                                         audio_play_config_t *config)
{
    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_i2s_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(s_i2s_handle, data, len, &bytes_written, 
                                       pdMS_TO_TICKS(5000));  /* 5秒超时 */

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S写入失败: %s", esp_err_to_name(ret));
        return ret;
    }

    if (bytes_written < len) {
        ESP_LOGW(TAG, "只写入了 %d/%d 字节", bytes_written, len);
    }

    return ESP_OK;
}

esp_err_t audio_play_data(const uint8_t *data, size_t len, const audio_play_config_t *config)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret;

    /* 如果提供了新配置，先应用 */
    if (config != NULL) {
        memcpy(&s_play_config, config, sizeof(audio_play_config_t));
        ret = create_i2s_channel();
        if (ret != ESP_OK) {
            xSemaphoreGive(s_mutex);
            return ret;
        }
        i2s_channel_enable(s_i2s_handle);
    }

    s_state = AUDIO_STATE_PLAYING;

    ret = play_audio_data_internal(data, len, (audio_play_config_t*)&s_play_config);

    s_state = AUDIO_STATE_READY;

    xSemaphoreGive(s_mutex);
    return ret;
}

esp_err_t audio_stop(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (s_i2s_handle != NULL) {
        i2s_channel_disable(s_i2s_handle);
    }

    s_state = AUDIO_STATE_READY;
    ESP_LOGI(TAG, "播放已停止");

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t audio_pause(void)
{
    if (!s_initialized || s_state != AUDIO_STATE_PLAYING) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    s_state = AUDIO_STATE_PAUSED;
    ESP_LOGI(TAG, "播放已暂停");

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t audio_resume(void)
{
    if (!s_initialized || s_state != AUDIO_STATE_PAUSED) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    s_state = AUDIO_STATE_PLAYING;
    ESP_LOGI(TAG, "播放已恢复");

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

/* ========================================
 * 音频文件操作接口实现（使用POSIX VFS）
 * ======================================== */

esp_err_t audio_get_wav_info(const char *file_path, audio_file_info_t *info)
{
    if (file_path == NULL || info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 使用POSIX VFS接口读取文件头 */
    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "无法打开文件: %s", file_path);
        return ESP_FAIL;
    }

    /* 分配内存读取WAV头 - 使用malloc，不使用栈上数组 */
    uint8_t *header = (uint8_t*)malloc(AUDIO_WAV_HEADER_SIZE);
    if (header == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        fclose(fp);
        return ESP_ERR_NO_MEM;
    }

    /* 读取WAV头 */
    size_t read_len = fread(header, 1, AUDIO_WAV_HEADER_SIZE, fp);
    fclose(fp);  /* 及时关闭文件 */

    if (read_len < AUDIO_WAV_HEADER_SIZE) {
        ESP_LOGE(TAG, "WAV文件头不完整");
        free(header);
        return ESP_FAIL;
    }

    /* 解析WAV头 */
    wav_header_t *wav = (wav_header_t*)header;

    /* 验证RIFF和WAVE标识 */
    if (memcmp(wav->riff, "RIFF", 4) != 0 || memcmp(wav->wave, "WAVE", 4) != 0) {
        ESP_LOGE(TAG, "无效的WAV文件格式");
        free(header);
        return ESP_FAIL;
    }

    /* 填充文件信息 - filename使用malloc分配 */
    info->filename = (char*)malloc(strlen(file_path) + 1);
    if (info->filename != NULL) {
        strcpy(info->filename, file_path);
    }

    info->size = wav->file_size + 8;
    info->sample_rate = parse_sample_rate(wav->sample_rate);
    info->channels = wav->num_channels;
    info->bits_per_sample = wav->bits_per_sample;
    info->byte_rate = wav->byte_rate;

    /* 计算时长 */
    uint32_t bytes_per_sec = wav->byte_rate;
    if (bytes_per_sec > 0) {
        info->duration_ms = (wav->data_size * 1000) / bytes_per_sec;
    } else {
        info->duration_ms = 0;
    }

    /* 先打印日志再释放内存 */
    ESP_LOGI(TAG, "WAV文件信息: 采样率=%dHz, 通道=%d, 位深=%d, 时长=%ums",
              wav->sample_rate, info->channels, info->bits_per_sample, info->duration_ms);

    free(header);

    return ESP_OK;
}

void audio_free_wav_info(audio_file_info_t *info)
{
    if (info == NULL) {
        return;
    }

    if (info->filename != NULL) {
        free(info->filename);
        info->filename = NULL;
    }
}

int32_t audio_play_wav(const char *file_path)
{
    if (!s_initialized || file_path == NULL) {
        return -1;
    }

    /* 获取文件信息 */
    audio_file_info_t file_info = {0};
    esp_err_t ret = audio_get_wav_info(file_path, &file_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取WAV文件信息失败");
        return -1;
    }

    /* 检查是否支持此格式 */
    if (file_info.bits_per_sample != 16) {
        ESP_LOGE(TAG, "只支持16位WAV文件");
        audio_free_wav_info(&file_info);
        return -1;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "获取互斥锁超时");
        audio_free_wav_info(&file_info);
        return -1;
    }

    /* 配置播放参数 */
    audio_play_config_t play_cfg = {
        .sample_rate = file_info.sample_rate,
        .format = file_info.channels == 2 ? AUDIO_FMT_16BIT_STEREO : AUDIO_FMT_16BIT,
        .channel = file_info.channels,
        .gain = s_current_gain
    };

    memcpy(&s_play_config, &play_cfg, sizeof(audio_play_config_t));

    /* 重新创建I2S通道 */
    ret = create_i2s_channel();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建I2S通道失败");
        xSemaphoreGive(s_mutex);
        audio_free_wav_info(&file_info);
        return -1;
    }

    ret = i2s_channel_enable(s_i2s_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启用I2S通道失败");
        xSemaphoreGive(s_mutex);
        audio_free_wav_info(&file_info);
        return -1;
    }

    /* 打开文件 */
    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "无法打开文件: %s", file_path);
        xSemaphoreGive(s_mutex);
        audio_free_wav_info(&file_info);
        return -1;
    }

    /* 跳过WAV头 */
    fseek(fp, AUDIO_WAV_HEADER_SIZE, SEEK_SET);

    /* 分配缓冲区 - 使用堆内存，不使用栈数组 */
    /* 根据采样率计算缓冲区大小：44.1KHz * 16bit * 1通道 = 88200 bytes/sec */
    /* 使用约50ms的缓冲区 */
    size_t buffer_size = (file_info.sample_rate * file_info.channels * 2) / 20;  /* 50ms */
    if (buffer_size > 8192) buffer_size = 8192;  /* 限制最大缓冲区 */
    if (buffer_size < 512) buffer_size = 512;   /* 确保最小缓冲区 */

    uint8_t *playback_buffer = (uint8_t*)malloc(buffer_size);
    if (playback_buffer == NULL) {
        ESP_LOGE(TAG, "播放缓冲区分配失败");
        fclose(fp);
        xSemaphoreGive(s_mutex);
        audio_free_wav_info(&file_info);
        return -1;
    }

    s_state = AUDIO_STATE_PLAYING;
    int32_t total_bytes = 0;
    int32_t played_duration = 0;

    ESP_LOGI(TAG, "开始播放WAV文件...");
    ESP_LOGI(TAG, "缓冲区大小: %d字节", buffer_size);

    /* 分块播放 */
    while (1) {
        size_t bytes_read = fread(playback_buffer, 1, buffer_size, fp);
        if (bytes_read == 0) {
            break;  /* 文件结束 */
        }

        size_t bytes_written = 0;
        ret = i2s_channel_write(s_i2s_handle, playback_buffer, bytes_read, 
                                &bytes_written, pdMS_TO_TICKS(1000));

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S写入失败: %s", esp_err_to_name(ret));
            break;
        }

        total_bytes += bytes_written;

        /* 更新播放进度 */
        if (file_info.byte_rate > 0) {
            played_duration = (total_bytes * 1000) / file_info.byte_rate;
        }

        /* 播放进度日志（每500ms打印一次） */
        static int32_t last_log_time = 0;
        if (played_duration - last_log_time >= 500 || played_duration == 0) {
            ESP_LOGI(TAG, "播放进度: %d/%d ms", played_duration, file_info.duration_ms);
            last_log_time = played_duration;
        }

        /* 检查是否停止 */
        if (s_state != AUDIO_STATE_PLAYING) {
            ESP_LOGI(TAG, "播放被中断");
            break;
        }
    }

    /* 清理 */
    free(playback_buffer);
    fclose(fp);

    s_state = AUDIO_STATE_READY;

    ESP_LOGI(TAG, "WAV播放完成，总播放: %d字节, 时长: %dms", total_bytes, played_duration);

    xSemaphoreGive(s_mutex);
    audio_free_wav_info(&file_info);

    return played_duration;
}

/* ========================================
 * 硬件控制接口实现
 * ======================================== */

esp_err_t audio_enable_output(void)
{
    if (!s_initialized || !s_pin_config.sd_enable || s_pin_config.sd == GPIO_NUM_NC) {
        return ESP_ERR_INVALID_STATE;
    }

    return gpio_set_level(s_pin_config.sd, 1) == ESP_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t audio_disable_output(void)
{
    if (!s_initialized || !s_pin_config.sd_enable || s_pin_config.sd == GPIO_NUM_NC) {
        return ESP_ERR_INVALID_STATE;
    }

    return gpio_set_level(s_pin_config.sd, 0) == ESP_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t audio_set_volume(uint8_t volume)
{
    /* 音量通过增益实现：0-25=3dB, 26-50=6dB, 51-75=9dB, 76-100=12dB */
    audio_gain_t gain;

    if (volume <= 25) {
        gain = AUDIO_GAIN_3DB;
    } else if (volume <= 50) {
        gain = AUDIO_GAIN_6DB;
    } else if (volume <= 75) {
        gain = AUDIO_GAIN_9DB;
    } else {
        gain = AUDIO_GAIN_12DB;
    }

    return audio_set_gain(gain);
}
