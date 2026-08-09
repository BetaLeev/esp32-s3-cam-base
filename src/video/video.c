/**
 * @file video.c
 * @brief 摄像头视频模块实现
 */

#include "video.h"
#include "../config.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "VIDEO";
#define LOG_TAG TAG

#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 15
#define CAM_PIN_SIOD 4
#define CAM_PIN_SIOC 5

#define CAM_PIN_D7 16
#define CAM_PIN_D6 17
#define CAM_PIN_D5 18
#define CAM_PIN_D4 12
#define CAM_PIN_D3 10
#define CAM_PIN_D2 8
#define CAM_PIN_D1 9
#define CAM_PIN_D0 11

#define CAM_PIN_VSYNC 6
#define CAM_PIN_HREF 7
#define CAM_PIN_PCLK 13

static bool s_video_initialized = false;
static bool s_video_streaming = false;
static bool s_init_in_progress = false;
static TaskHandle_t s_init_task = NULL;

static SemaphoreHandle_t s_video_mutex = NULL;
static StaticSemaphore_t s_video_mutex_buffer;

static camera_fb_t *s_latest_frame = NULL;
static framesize_t s_current_framesize = FRAMESIZE_VGA;

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,
    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_VGA,
    .jpeg_quality = 12,
    .fb_count = 2,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_LATEST,
};

static const video_framesize_info_t s_framesize_list[] = {
    {FRAMESIZE_96X96, 96, 96, "96x96"},
    {FRAMESIZE_QQVGA, 160, 120, "160x120 (QQVGA)"},
    {FRAMESIZE_QCIF, 176, 144, "176x144 (QCIF)"},
    {FRAMESIZE_HQVGA, 240, 176, "240x176 (HQVGA)"},
    {FRAMESIZE_240X240, 240, 240, "240x240"},
    {FRAMESIZE_QVGA, 320, 240, "320x240 (QVGA)"},
    {FRAMESIZE_CIF, 400, 296, "400x296 (CIF)"},
    {FRAMESIZE_HVGA, 480, 320, "480x320 (HVGA)"},
    {FRAMESIZE_VGA, 640, 480, "640x480 (VGA)"},
    {FRAMESIZE_SVGA, 800, 600, "800x600 (SVGA)"},
    {FRAMESIZE_XGA, 1024, 768, "1024x768 (XGA)"},
    {FRAMESIZE_HD, 1280, 720, "1280x720 (HD)"},
    {FRAMESIZE_SXGA, 1280, 1024, "1280x1024 (SXGA)"},
    {FRAMESIZE_UXGA, 1600, 1200, "1600x1200 (UXGA)"},
};

#define FRAMESIZE_LIST_COUNT (sizeof(s_framesize_list) / sizeof(s_framesize_list[0]))

static void init_mutex_safe(void) {
    static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&spinlock);
    if (s_video_mutex == NULL) {
        s_video_mutex = xSemaphoreCreateMutexStatic(&s_video_mutex_buffer);
    }
    portEXIT_CRITICAL(&spinlock);
}

static bool lock_video(uint32_t timeout_ms) {
    init_mutex_safe();
    if (s_video_mutex == NULL) {
        return false;
    }
    return xSemaphoreTake(s_video_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

static void unlock_video(void) {
    if (s_video_mutex != NULL) {
        xSemaphoreGive(s_video_mutex);
    }
}

static const char *get_sensor_name(uint16_t pid) {
    switch (pid) {
    case OV2640_PID:
        return "OV2640";
    case OV3660_PID:
        return "OV3660";
    case OV5640_PID:
        return "OV5640";
    case OV7670_PID:
        return "OV7670";
    case OV7725_PID:
        return "OV7725";
    case GC032A_PID:
        return "GC032A";
    case GC2145_PID:
        return "GC2145";
    case BF3005_PID:
        return "BF3005";
    case BF20A6_PID:
        return "BF20A6";
    case SC101IOT_PID:
        return "SC101IOT";
    default:
        return "Unknown";
    }
}

static bool framesize_to_wh(framesize_t fs, uint16_t *out_w, uint16_t *out_h) {
    for (uint32_t i = 0; i < FRAMESIZE_LIST_COUNT; i++) {
        if (s_framesize_list[i].framesize == fs) {
            if (out_w)
                *out_w = s_framesize_list[i].width;
            if (out_h)
                *out_h = s_framesize_list[i].height;
            return true;
        }
    }
    return false;
}

esp_err_t video_init(void) {

    if (s_video_initialized) {
        VIDEO_LOGW(TAG, "视频模块已初始化");
        return ESP_OK;
    }

    VIDEO_LOGI(TAG, "========== 13摄像头初始化开始 ==========");

    init_mutex_safe();
    if (s_video_mutex == NULL) {
        VIDEO_LOGE(TAG, "创建互斥锁失败");
        return ESP_FAIL;
    }

    VIDEO_LOGI(TAG, "[1/3] 构建配置并调用 esp_camera_init()...");

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CAM_PIN_SIOD) | (1ULL << CAM_PIN_SIOC),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    vTaskDelay(pdMS_TO_TICKS(10));

    camera_config_t local_cfg;
    memset(&local_cfg, 0, sizeof(local_cfg));
    local_cfg.pin_pwdn = CAM_PIN_PWDN;
    local_cfg.pin_reset = CAM_PIN_RESET;
    local_cfg.pin_xclk = CAM_PIN_XCLK;
    local_cfg.pin_sccb_sda = CAM_PIN_SIOD;
    local_cfg.pin_sccb_scl = CAM_PIN_SIOC;
    local_cfg.pin_d7 = CAM_PIN_D7;
    local_cfg.pin_d6 = CAM_PIN_D6;
    local_cfg.pin_d5 = CAM_PIN_D5;
    local_cfg.pin_d4 = CAM_PIN_D4;
    local_cfg.pin_d3 = CAM_PIN_D3;
    local_cfg.pin_d2 = CAM_PIN_D2;
    local_cfg.pin_d1 = CAM_PIN_D1;
    local_cfg.pin_d0 = CAM_PIN_D0;
    local_cfg.pin_vsync = CAM_PIN_VSYNC;
    local_cfg.pin_href = CAM_PIN_HREF;
    local_cfg.pin_pclk = CAM_PIN_PCLK;
    local_cfg.ledc_timer = LEDC_TIMER_1;
    local_cfg.ledc_channel = LEDC_CHANNEL_1;
    local_cfg.xclk_freq_hz = 15000000;
    local_cfg.pixel_format = PIXFORMAT_JPEG;
    local_cfg.frame_size = FRAMESIZE_VGA;
    local_cfg.jpeg_quality = 12;
    local_cfg.fb_count = 2;
    local_cfg.fb_location = CAMERA_FB_IN_PSRAM;
    local_cfg.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    local_cfg.sccb_i2c_port = 0;

    esp_err_t err = esp_camera_init(&local_cfg);
    if (err != ESP_OK) {
        VIDEO_LOGE(TAG, "摄像头初始化失败: %s", esp_err_to_name(err));
        return err;
    }
    VIDEO_LOGI(TAG, "[2/3] 摄像头驱动初始化成功");

    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        s->set_brightness(s, 0);
        s->set_contrast(s, 0);
        s->set_saturation(s, 0);
        s->set_special_effect(s, 0);
        s->set_whitebal(s, 1);
        s->set_awb_gain(s, 1);
        s->set_wb_mode(s, 0);
        s->set_exposure_ctrl(s, 1);
        s->set_aec2(s, 0);
        s->set_ae_level(s, 0);
        s->set_aec_value(s, 300);
        s->set_gain_ctrl(s, 1);
        s->set_agc_gain(s, 0);
        s->set_gainceiling(s, (gainceiling_t)0);
        s->set_bpc(s, 0);
        s->set_wpc(s, 1);
        s->set_raw_gma(s, 1);
        s->set_lenc(s, 1);
        s->set_hmirror(s, 0);
        s->set_vflip(s, 0);
        s->set_dcw(s, 1);
        s->set_colorbar(s, 0);
    }

    s_current_framesize = camera_config.frame_size;
    s_video_initialized = true;
    s_video_streaming = false;

    VIDEO_LOGI(TAG, "[3/3] 摄像头初始化完成");

    return ESP_OK;
}

static void video_init_task(void *pvParameters) {
    vTaskDelay(pdMS_TO_TICKS(2000));
    s_init_in_progress = true;

    esp_err_t err = video_init();
    if (err != ESP_OK) {
        VIDEO_LOGE(TAG, "异步初始化失败，摄像头不可用 (0x%x)", err);
    }

    s_init_in_progress = false;
    s_init_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t video_init_async(void) {
    if (s_video_initialized || s_init_in_progress) {
        return ESP_OK;
    }

    BaseType_t ret = xTaskCreate(video_init_task, "video_init", 8192, NULL, 1, &s_init_task);
    if (ret != pdPASS) {
        s_init_task = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t video_deinit(void) {
    if (!s_video_initialized) {
        return ESP_OK;
    }

    if (s_video_streaming) {
        video_stop_streaming();
    }

    if (lock_video(1000)) {
        if (s_latest_frame != NULL) {
            esp_camera_fb_return(s_latest_frame);
            s_latest_frame = NULL;
        }
        unlock_video();
    }

    esp_err_t ret = esp_camera_deinit();
    s_video_initialized = false;
    return ret;
}

bool video_is_initialized(void) {
    return s_video_initialized;
}

bool video_is_init_in_progress(void) {
    return s_init_in_progress;
}

bool video_is_streaming(void) {
    return s_video_streaming;
}

esp_err_t video_start_streaming(void) {
    if (!s_video_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    s_video_streaming = true;

    // 立即获取并释放一帧，确保传感器退出待机状态
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
        VIDEO_LOGI(TAG, "触发传感器输出，首帧大小=%u 字节", fb->len);
        esp_camera_fb_return(fb);
    } else {
        VIDEO_LOGW(TAG, "触发传感器失败，稍后可能恢复");
    }
    return ESP_OK;
}

esp_err_t video_stop_streaming(void) {
    s_video_streaming = false;
    return ESP_OK;
}

camera_fb_t *video_capture_frame(void) {
    if (!s_video_initialized) {
        return NULL;
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb == NULL) {
        return NULL;
    }

    if (fb->len == 0) {
        esp_camera_fb_return(fb);
        return NULL;
    }

    return fb;
}

esp_err_t video_get_jpeg(uint8_t **buf, size_t *len) {
    if (!s_video_initialized || buf == NULL || len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    camera_fb_t *fb = video_capture_frame();
    if (fb == NULL) {
        return ESP_FAIL;
    }

    if (!lock_video(200)) {
        esp_camera_fb_return(fb);
        return ESP_ERR_TIMEOUT;
    }

    /* 如果有上一次未释放的旧帧，在此先清理 */
    if (s_latest_frame != NULL) {
        esp_camera_fb_return(s_latest_frame);
    }

    s_latest_frame = fb;
    *buf = fb->buf;
    *len = fb->len;

    unlock_video();
    return ESP_OK;
}

/* 核心修复：用完显式清理归还给驱动 */
void video_release_jpeg(void) {
    if (!lock_video(200)) {
        return;
    }
    if (s_latest_frame != NULL) {
        esp_camera_fb_return(s_latest_frame);
        s_latest_frame = NULL;
    }
    unlock_video();
}

esp_err_t video_set_parameter(video_param_type_t type, int value) {
    if (!s_video_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) {
        return ESP_FAIL;
    }

    esp_err_t ret = ESP_OK;

    switch (type) {
    case VIDEO_PARAM_BRIGHTNESS:
        if (value < -2 || value > 2)
            return ESP_ERR_INVALID_ARG;
        ret = s->set_brightness(s, value);
        break;
    case VIDEO_PARAM_CONTRAST:
        if (value < -2 || value > 2)
            return ESP_ERR_INVALID_ARG;
        ret = s->set_contrast(s, value);
        break;
    case VIDEO_PARAM_SATURATION:
        if (value < -2 || value > 2)
            return ESP_ERR_INVALID_ARG;
        ret = s->set_saturation(s, value);
        break;
    case VIDEO_PARAM_GAIN:
        if (value < 0 || value > 30)
            return ESP_ERR_INVALID_ARG;
        s->set_gain_ctrl(s, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        ret = s->set_agc_gain(s, value);
        break;
    case VIDEO_PARAM_EXPOSURE:
        if (value < 0 || value > 1200)
            return ESP_ERR_INVALID_ARG;
        s->set_exposure_ctrl(s, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        ret = s->set_aec_value(s, value);
        break;
    case VIDEO_PARAM_HMIRROR:
        ret = s->set_hmirror(s, value ? 1 : 0);
        break;
    case VIDEO_PARAM_VFLIP:
        ret = s->set_vflip(s, value ? 1 : 0);
        break;
    case VIDEO_PARAM_QUALITY:
        if (value < 0 || value > 63)
            return ESP_ERR_INVALID_ARG;
        camera_config.jpeg_quality = value;
        break;
    case VIDEO_PARAM_FRAMESIZE:
        ret = video_set_framesize((framesize_t)value);
        break;
    case VIDEO_PARAM_AWB:
        ret = s->set_whitebal(s, value ? 1 : 0);
        break;
    case VIDEO_PARAM_WB_MODE:
        if (value < 0 || value > 4)
            return ESP_ERR_INVALID_ARG;
        ret = s->set_wb_mode(s, value);
        break;
    case VIDEO_PARAM_AEC:
        ret = s->set_exposure_ctrl(s, value ? 1 : 0);
        break;
    case VIDEO_PARAM_AGC:
        ret = s->set_gain_ctrl(s, value ? 1 : 0);
        break;
    default:
        ret = ESP_ERR_INVALID_ARG;
        break;
    }

    return ret;
}

int video_get_parameter(video_param_type_t type) {
    if (!s_video_initialized) {
        return -1;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) {
        return -1;
    }

    switch (type) {
    case VIDEO_PARAM_BRIGHTNESS:
        return s->status.brightness;
    case VIDEO_PARAM_CONTRAST:
        return s->status.contrast;
    case VIDEO_PARAM_SATURATION:
        return s->status.saturation;
    case VIDEO_PARAM_GAIN:
        return s->status.agc_gain;
    case VIDEO_PARAM_EXPOSURE:
        return s->status.aec_value;
    case VIDEO_PARAM_HMIRROR:
        return s->status.hmirror ? 1 : 0;
    case VIDEO_PARAM_VFLIP:
        return s->status.vflip ? 1 : 0;
    case VIDEO_PARAM_QUALITY:
        return (int)camera_config.jpeg_quality;
    case VIDEO_PARAM_FRAMESIZE:
        return (int)s_current_framesize;
    case VIDEO_PARAM_AWB:
        return s->status.awb ? 1 : 0;
    case VIDEO_PARAM_WB_MODE:
        return s->status.wb_mode;
    case VIDEO_PARAM_AEC:
        return s->status.aec ? 1 : 0;
    case VIDEO_PARAM_AGC:
        return s->status.agc ? 1 : 0;
    default:
        return -1;
    }
}

esp_err_t video_set_framesize(framesize_t framesize) {
    if (!s_video_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) {
        return ESP_FAIL;
    }

    bool valid = false;
    for (uint32_t i = 0; i < FRAMESIZE_LIST_COUNT; i++) {
        if (s_framesize_list[i].framesize == framesize) {
            valid = true;
            break;
        }
    }
    if (!valid) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = s->set_framesize(s, framesize);
    if (ret == ESP_OK) {
        s_current_framesize = framesize;
    }

    return ret;
}

esp_err_t video_get_info_struct(video_info_t *info) {
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(info, 0, sizeof(video_info_t));

    if (s_init_in_progress) {
        info->initialized = false;
        info->streaming = false;
        strncpy(info->sensor_name, "Probing...", sizeof(info->sensor_name) - 1);
        info->jpeg_quality = camera_config.jpeg_quality;
        info->framesize = s_current_framesize;
        framesize_to_wh(s_current_framesize, &info->width, &info->height);
        return ESP_OK;
    }

    info->initialized = s_video_initialized;
    info->streaming = s_video_streaming;
    info->jpeg_quality = camera_config.jpeg_quality;
    info->framesize = s_current_framesize;
    framesize_to_wh(s_current_framesize, &info->width, &info->height);

    if (!s_video_initialized) {
        return ESP_OK;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        strncpy(info->sensor_name, get_sensor_name(s->id.PID), sizeof(info->sensor_name) - 1);
        info->brightness = s->status.brightness;
        info->contrast = s->status.contrast;
        info->saturation = s->status.saturation;
        info->gain = s->status.agc_gain;
        info->exposure = s->status.aec_value;
        info->hmirror = s->status.hmirror ? true : false;
        info->vflip = s->status.vflip ? true : false;
    } else {
        strncpy(info->sensor_name, "Unknown", sizeof(info->sensor_name) - 1);
    }

    return ESP_OK;
}

const video_framesize_info_t *video_get_framesize_list(uint32_t *out_count) {
    if (out_count) {
        *out_count = FRAMESIZE_LIST_COUNT;
    }
    return s_framesize_list;
}