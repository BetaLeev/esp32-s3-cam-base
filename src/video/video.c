/**
 * @file video.c
 * @brief 摄像头视频模块实现
 *
 * 基于 ESP32-CAM 官方 esp32-camera 组件
 * 参考官方示例: $IDF_PATH/examples/peripherals/camera/pic_server
 */

#include "video.h"
#include "../config.h"
#include "esp_camera.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "VIDEO";
#define LOG_TAG TAG

/* ========================================
 * 摄像头引脚配置 - ESP32-S3-CAM (果云板载)
 *
 * 引脚阵列与官方 CAMERA_MODEL_ESP32S3_EYE 完全一致
 * 参考来源: esp32-camera/examples/camera_pins.h
 *
 * DVP 并行总线引脚映射（Y2~Y9 对应 D0~D7）:
 *   Y9(GPIO16) → D7,  Y8(GPIO17) → D6
 *   Y7(GPIO18) → D5,  Y6(GPIO12) → D4
 *   Y5(GPIO10) → D3,  Y4(GPIO8)  → D2
 *   Y3(GPIO9)  → D1,  Y2(GPIO11) → D0
 *
 * 同步信号: VSYNC=GPIO6, HREF=GPIO7, PCLK=GPIO13
 * 时钟:     XCLK=GPIO15 (由 LEDC 生成 20MHz)
 * SCCB:     SDA=GPIO4, SCL=GPIO5 (I2C 端口1, 100kHz)
 * 电源:     PWDN=-1(未使用), RESET=-1(硬件复位)
 * ======================================== */

#define CAM_PIN_PWDN    -1  /* 电源down引脚, -1=不使用(板载硬件控制) */
#define CAM_PIN_RESET   -1  /* 复位引脚, -1=不使用(板载硬件复位) */
#define CAM_PIN_XCLK    15  /* 主时钟输出引脚, LEDC生成20MHz方波 */
#define CAM_PIN_SIOD    4   /* SCCB(I2C)数据线 SDA */
#define CAM_PIN_SIOC    5   /* SCCB(I2C)时钟线 SCL */

#define CAM_PIN_D7      16  /* 数据位7 (Y9) - MSB */
#define CAM_PIN_D6      17  /* 数据位6 (Y8) */
#define CAM_PIN_D5      18  /* 数据位5 (Y7) */
#define CAM_PIN_D4      12  /* 数据位4 (Y6) */
#define CAM_PIN_D3      10  /* 数据位3 (Y5) */
#define CAM_PIN_D2      8   /* 数据位2 (Y4) */
#define CAM_PIN_D1      9   /* 数据位1 (Y3) */
#define CAM_PIN_D0      11  /* 数据位0 (Y2) - LSB */

#define CAM_PIN_VSYNC   6   /* 垂直同步信号(帧开始) */
#define CAM_PIN_HREF    7   /* 行有效信号(行数据有效) */
#define CAM_PIN_PCLK    13  /* 像素时钟(每个像素一个上升沿) */

/* ========================================
 * 模块状态
 * ======================================== */

static bool s_video_initialized = false;
static bool s_video_streaming = false;
static bool s_init_in_progress = false;   /* 异步初始化进行中 */
static TaskHandle_t s_init_task = NULL;   /* 初始化任务句柄 */

/* 互斥锁 - 使用静态初始化避免竞态条件 */
static SemaphoreHandle_t s_video_mutex = NULL;
static StaticSemaphore_t s_video_mutex_buffer;

/* 最新帧缓存（单帧） */
static camera_fb_t *s_latest_frame = NULL;

/* 当前帧大小（缓存，避免每次查询传感器） */
static framesize_t s_current_framesize = FRAMESIZE_VGA;

/* ========================================
 * 摄像头配置参数
 *
 * 关键依赖: 需要在 sdkconfig 中启用 PSRAM
 *   CONFIG_SPIRAM=y (Octal模式, 40MHz)
 *
 * PSRAM 用于帧缓冲区(fb_location=CAMERA_FB_IN_PSRAM)
 * SVGA(800x600) JPEG 双缓冲约需 300KB+ PSRAM
 * ======================================== */

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
    .xclk_freq_hz = 20000000,  /* 20MHz XCLK - 官方推荐值 */
    .ledc_timer = LEDC_TIMER_0,   /* LEDC定时器0 生成XCLK */
    .ledc_channel = LEDC_CHANNEL_0, /* LEDC通道0 输出XCLK */
    .pixel_format = PIXFORMAT_JPEG,  /* JPEG格式(OV2640硬件编码) */
    .frame_size = FRAMESIZE_VGA,      /* 640x480 默认分辨率 (与Arduino可工作代码一致) */
    .jpeg_quality = 12,               /* 0-63, 越小质量越高(12=较好) */
    .fb_count = 2,                    /* 双缓冲提高采集效率 */
    .fb_location = CAMERA_FB_IN_PSRAM, /* 帧缓冲存放在PSRAM */
    .grab_mode = CAMERA_GRAB_LATEST,   /* 默认值 (与Arduino一致) */
};

/* ========================================
 * 支持的帧大小列表
 * ======================================== */

static const video_framesize_info_t s_framesize_list[] = {
    {FRAMESIZE_96X96,   96,    96,    "96x96"},
    {FRAMESIZE_QQVGA,   160,   120,   "160x120 (QQVGA)"},
    {FRAMESIZE_QCIF,    176,   144,   "176x144 (QCIF)"},
    {FRAMESIZE_HQVGA,   240,   176,   "240x176 (HQVGA)"},
    {FRAMESIZE_240X240, 240,   240,   "240x240"},
    {FRAMESIZE_QVGA,    320,   240,   "320x240 (QVGA)"},
    {FRAMESIZE_CIF,     400,   296,   "400x296 (CIF)"},
    {FRAMESIZE_HVGA,    480,   320,   "480x320 (HVGA)"},
    {FRAMESIZE_VGA,     640,   480,   "640x480 (VGA)"},
    {FRAMESIZE_SVGA,    800,   600,   "800x600 (SVGA)"},
    {FRAMESIZE_XGA,     1024,  768,   "1024x768 (XGA)"},
    {FRAMESIZE_HD,      1280,  720,   "1280x720 (HD)"},
    {FRAMESIZE_SXGA,    1280,  1024,  "1280x1024 (SXGA)"},
    {FRAMESIZE_UXGA,    1600,  1200,  "1600x1200 (UXGA)"},
};

#define FRAMESIZE_LIST_COUNT (sizeof(s_framesize_list) / sizeof(s_framesize_list[0]))

/* ========================================
 * 内部辅助函数
 * ======================================== */

/**
 * @brief 初始化互斥锁（线程安全）
 */
static void init_mutex_safe(void)
{
    static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&spinlock);
    if (s_video_mutex == NULL) {
        s_video_mutex = xSemaphoreCreateMutexStatic(&s_video_mutex_buffer);
    }
    portEXIT_CRITICAL(&spinlock);
}

/**
 * @brief 获取锁（带超时）
 */
static bool lock_video(uint32_t timeout_ms)
{
    init_mutex_safe();
    if (s_video_mutex == NULL) {
        return false;
    }
    return xSemaphoreTake(s_video_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

/**
 * @brief 释放锁
 */
static void unlock_video(void)
{
    if (s_video_mutex != NULL) {
        xSemaphoreGive(s_video_mutex);
    }
}

/**
 * @brief 根据PID获取传感器名称
 */
static const char* get_sensor_name(uint16_t pid)
{
    switch (pid) {
        case OV2640_PID:  return "OV2640";
        case OV3660_PID:  return "OV3660";
        case OV5640_PID:  return "OV5640";
        case OV7670_PID:  return "OV7670";
        case OV7725_PID:  return "OV7725";
        case GC032A_PID:  return "GC032A";
        case GC2145_PID:  return "GC2145";
        case BF3005_PID:  return "BF3005";
        case BF20A6_PID:  return "BF20A6";
        case SC101IOT_PID:return "SC101IOT";
        default:          return "Unknown";
    }
}

/**
 * @brief 从帧大小枚举获取宽高
 */
static bool framesize_to_wh(framesize_t fs, uint16_t *out_w, uint16_t *out_h)
{
    for (uint32_t i = 0; i < FRAMESIZE_LIST_COUNT; i++) {
        if (s_framesize_list[i].framesize == fs) {
            if (out_w) *out_w = s_framesize_list[i].width;
            if (out_h) *out_h = s_framesize_list[i].height;
            return true;
        }
    }
    return false;
}

/* ========================================
 * 公共接口：生命周期
 * ======================================== */

esp_err_t video_init(void)
{
    if (s_video_initialized) {
        VIDEO_LOGW(TAG, "视频模块已初始化");
        return ESP_OK;
    }

    VIDEO_LOGI(TAG, "========== 摄像头初始化开始 ==========");

    /* 注意: BROWN_OUT 已在 sdkconfig 中禁用 (CONFIG_ESP_BROWNOUT_DET=n)
     * 无需运行时关闭，避免在 actuators 之后才关闭的时序问题 */

    /* 初始化互斥锁 */
    init_mutex_safe();
    if (s_video_mutex == NULL) {
        VIDEO_LOGE(TAG, "创建互斥锁失败");
        return ESP_FAIL;
    }

    /* ----------------------------------------------------------------
     * 【关键】构建本地 camera_config_t 副本并零初始化
     *   - 完全对齐可工作的 Arduino 代码配置
     *   - 不设置 fb_location / grab_mode，使用默认值（与 Arduino 一致）
     * ---------------------------------------------------------------- */
    VIDEO_LOGI(TAG, "[1/3] 构建配置并调用 esp_camera_init()...");

    /* I2C 总线恢复：如果 SDA 被从设备拉低，主机无法产生 START 信号
     * 方法：手动产生 9 个 SCL 时钟脉冲释放总线 */
    VIDEO_LOGI(TAG, "  执行 I2C 总线恢复 + 内部上拉 (SDA=GPIO%d, SCL=GPIO%d)...",
             CAM_PIN_SIOD, CAM_PIN_SIOC);

    /* 先配置 SDA/SCL 为开漏输出 + 内部上拉 */
    gpio_config_t sda_conf = {
        .pin_bit_mask = (1ULL << CAM_PIN_SIOD),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&sda_conf);
    gpio_set_level((gpio_num_t)CAM_PIN_SIOD, 1);

    gpio_config_t scl_conf = {
        .pin_bit_mask = (1ULL << CAM_PIN_SIOC),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&scl_conf);
    gpio_set_level((gpio_num_t)CAM_PIN_SIOC, 1);

    /* 9 个 SCL 脉冲释放总线 */
    for (int i = 0; i < 9; i++) {
        gpio_set_level((gpio_num_t)CAM_PIN_SIOC, 0);
        esp_rom_delay_us(5);
        gpio_set_level((gpio_num_t)CAM_PIN_SIOC, 1);
        esp_rom_delay_us(5);
    }
    /* STOP 条件: SCL=1 时 SDA 低→高 */
    gpio_set_level((gpio_num_t)CAM_PIN_SIOD, 0);
    esp_rom_delay_us(5);
    gpio_set_level((gpio_num_t)CAM_PIN_SIOC, 1);
    esp_rom_delay_us(5);
    gpio_set_level((gpio_num_t)CAM_PIN_SIOD, 1);
    esp_rom_delay_us(5);

    /* 保持内部上拉，交给 esp_camera_init() 重新配置 */
    VIDEO_LOGI(TAG, "  I2C 总线恢复完成 (已设内部上拉)");

    /* XCLK 由 esp_camera_init 内部的 cam_init → ll_cam_config 自动配置
     * ESP32-S3 上 CAMERA_ENABLE_OUT_CLOCK 是空操作，但 cam_init 会配置 LCD_CAM
     * 不需要手动配置 XCLK */

    /* 等待系统稳定 */
    vTaskDelay(pdMS_TO_TICKS(100));

    camera_config_t local_cfg;
    memset(&local_cfg, 0, sizeof(local_cfg));
    local_cfg.pin_pwdn     = CAM_PIN_PWDN;
    local_cfg.pin_reset    = CAM_PIN_RESET;
    local_cfg.pin_xclk     = CAM_PIN_XCLK;
    local_cfg.pin_sccb_sda = CAM_PIN_SIOD;
    local_cfg.pin_sccb_scl = CAM_PIN_SIOC;
    local_cfg.pin_d7       = CAM_PIN_D7;
    local_cfg.pin_d6       = CAM_PIN_D6;
    local_cfg.pin_d5       = CAM_PIN_D5;
    local_cfg.pin_d4       = CAM_PIN_D4;
    local_cfg.pin_d3       = CAM_PIN_D3;
    local_cfg.pin_d2       = CAM_PIN_D2;
    local_cfg.pin_d1       = CAM_PIN_D1;
    local_cfg.pin_d0       = CAM_PIN_D0;
    local_cfg.pin_vsync    = CAM_PIN_VSYNC;
    local_cfg.pin_href     = CAM_PIN_HREF;
    local_cfg.pin_pclk     = CAM_PIN_PCLK;
    local_cfg.xclk_freq_hz = 15000000;            /* 15MHz XCLK (参考Arduino可工作代码) */
    local_cfg.ledc_timer   = LEDC_TIMER_0;
    local_cfg.ledc_channel = LEDC_CHANNEL_0;
    local_cfg.pixel_format = PIXFORMAT_JPEG;
    local_cfg.frame_size   = FRAMESIZE_VGA;        /* 640x480 (与Arduino代码一致) */
    local_cfg.jpeg_quality = 12;                   /* 0-63 越小越清晰 */
    local_cfg.fb_count     = 2;                    /* 双缓冲 */

    VIDEO_LOGI(TAG, "  XCLK=%dHz  FB=PSRAM  JPEG_Q=%d  SIZE=VGA(640x480)",
             local_cfg.xclk_freq_hz, local_cfg.jpeg_quality);
    VIDEO_LOGI(TAG, "  SCCB: SDA=GPIO%d  SCL=GPIO%d  PORT1 100kHz (legacy bit-bang)",
             CAM_PIN_SIOD, CAM_PIN_SIOC);

    VIDEO_LOGI(TAG, "  >>> 即将调用 esp_camera_init()...");
    VIDEO_LOGI(TAG, "  >>> 如果在此日志后卡住, 说明 SCCB probe 阻塞");
    esp_err_t err = esp_camera_init(&local_cfg);
    VIDEO_LOGI(TAG, "  <<< esp_camera_init() 返回: 0x%x (%s)",
             err, esp_err_to_name(err));

    if (err != ESP_OK) {
        VIDEO_LOGE(TAG, "摄像头初始化失败: %s", esp_err_to_name(err));
        switch (err) {
            case ESP_ERR_NOT_FOUND:
                VIDEO_LOGE(TAG, "  原因: 未找到摄像头驱动 (检查SCCB连接)");
                break;
            case ESP_ERR_NO_MEM:
                VIDEO_LOGE(TAG, "  原因: 内存不足 (PSRAM是否启用?)");
                break;
            case ESP_ERR_INVALID_STATE:
                VIDEO_LOGE(TAG, "  原因: 摄像头已初始化");
                break;
            case ESP_ERR_INVALID_ARG:
                VIDEO_LOGE(TAG, "  原因: 配置参数错误 (引脚?)");
                break;
            default:
                VIDEO_LOGE(TAG, "  原因: 未知错误 0x%x", err);
                break;
        }
        return err;
    }

    /* 获取传感器句柄并设置默认参数 */
    VIDEO_LOGI(TAG, "[2/3] 配置传感器默认参数...");
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        VIDEO_LOGI(TAG, "  传感器型号: %s (PID: 0x%04X)",
                 get_sensor_name(s->id.PID), s->id.PID);

        /* 参考官方示例默认配置 */
        s->set_brightness(s, 0);
        s->set_contrast(s, 0);
        s->set_saturation(s, 0);
        s->set_special_effect(s, 0);
        s->set_whitebal(s, 1);          /* 自动白平衡 */
        s->set_awb_gain(s, 1);          /* 自动白平衡增益 */
        s->set_wb_mode(s, 0);           /* 白平衡模式自动 */
        s->set_exposure_ctrl(s, 1);     /* 自动曝光 */
        s->set_aec2(s, 0);
        s->set_ae_level(s, 0);
        s->set_aec_value(s, 300);
        s->set_gain_ctrl(s, 1);         /* 自动增益 */
        s->set_agc_gain(s, 0);
        s->set_gainceiling(s, (gainceiling_t)0);
        s->set_bpc(s, 0);               /* 坏点校正 */
        s->set_wpc(s, 1);
        s->set_raw_gma(s, 1);           /* Gamma校正 */
        s->set_lenc(s, 1);              /* 镜头校正 */
        s->set_hmirror(s, 0);
        s->set_vflip(s, 0);
        s->set_dcw(s, 1);
        s->set_colorbar(s, 0);

        VIDEO_LOGI(TAG, "  传感器参数配置完成");
    } else {
        VIDEO_LOGW(TAG, "  无法获取传感器句柄");
    }

    /* 缓存当前帧大小 */
    s_current_framesize = camera_config.frame_size;

    s_video_initialized = true;
    s_video_streaming = false;

    VIDEO_LOGI(TAG, "[3/3] 初始化完成");
    uint16_t w = 0, h = 0;
    framesize_to_wh(s_current_framesize, &w, &h);
    VIDEO_LOGI(TAG, "  分辨率: %ux%u, JPEG质量: %d, 缓冲帧数: %d",
             w, h, camera_config.jpeg_quality, camera_config.fb_count);
    VIDEO_LOGI(TAG, "========== 摄像头初始化成功 ==========");

    return ESP_OK;
}

/* ========================================
 * 异步初始化任务
 * ======================================== */

static void video_init_task(void *pvParameters)
{
    /* 延迟 2 秒，让主任务先完成 WiFi/HTTP server 初始化 */
    vTaskDelay(pdMS_TO_TICKS(2000));

    VIDEO_LOGI(TAG, "异步初始化任务开始执行...");
    s_init_in_progress = true;

    esp_err_t err = video_init();
    if (err != ESP_OK) {
        VIDEO_LOGE(TAG, "异步初始化失败，摄像头不可用 (0x%x)，继续运行其他模块", err);
    }

    s_init_in_progress = false;
    s_init_task = NULL;
    vTaskDelete(NULL);  /* 自删除 */
}

esp_err_t video_init_async(void)
{
    if (s_video_initialized || s_init_in_progress) {
        return ESP_OK;
    }

    BaseType_t ret = xTaskCreate(
        video_init_task,
        "video_init",
        8192,          /* 8KB 栈 (esp_camera_init 需要较大栈) */
        NULL,
        1,             /* 优先级 1 (与主任务相同，避免抢占主流程) */
        &s_init_task
    );

    if (ret != pdPASS) {
        VIDEO_LOGE(TAG, "创建异步初始化任务失败");
        s_init_task = NULL;
        return ESP_ERR_NO_MEM;
    }

    VIDEO_LOGI(TAG, "异步初始化任务已创建，将在后台执行 (不阻塞app_main)");
    return ESP_OK;
}

esp_err_t video_deinit(void)
{
    if (!s_video_initialized) {
        return ESP_OK;
    }

    VIDEO_LOGI(TAG, "反初始化视频模块...");

    /* 停止视频流 */
    if (s_video_streaming) {
        video_stop_streaming();
    }

    /* 释放帧缓存 */
    if (lock_video(1000)) {
        if (s_latest_frame != NULL) {
            esp_camera_fb_return(s_latest_frame);
            s_latest_frame = NULL;
        }
        unlock_video();
    }

    /* 关闭摄像头 */
    esp_err_t ret = esp_camera_deinit();
    if (ret != ESP_OK) {
        VIDEO_LOGE(TAG, "摄像头反初始化失败: %s", esp_err_to_name(ret));
    }

    s_video_initialized = false;
    VIDEO_LOGI(TAG, "视频模块已反初始化");

    return ret;
}

/* ========================================
 * 公共接口：状态
 * ======================================== */

bool video_is_initialized(void)
{
    return s_video_initialized;
}

bool video_is_init_in_progress(void)
{
    return s_init_in_progress;
}

bool video_is_streaming(void)
{
    return s_video_streaming;
}

esp_err_t video_start_streaming(void)
{
    if (!s_video_initialized) {
        VIDEO_LOGE(TAG, "视频模块未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_video_streaming) {
        VIDEO_LOGW(TAG, "视频流已在运行");
        return ESP_OK;
    }

    s_video_streaming = true;
    VIDEO_LOGI(TAG, "视频流已启动");
    return ESP_OK;
}

esp_err_t video_stop_streaming(void)
{
    if (!s_video_streaming) {
        VIDEO_LOGW(TAG, "视频流未在运行");
        return ESP_OK;
    }

    s_video_streaming = false;
    VIDEO_LOGI(TAG, "视频流已停止");
    return ESP_OK;
}

/* ========================================
 * 公共接口：帧捕获
 * ======================================== */

camera_fb_t* video_capture_frame(void)
{
    if (!s_video_initialized) {
        return NULL;
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb == NULL) {
        VIDEO_LOGW(TAG, "获取摄像头帧失败");
        return NULL;
    }

    if (fb->len == 0) {
        VIDEO_LOGW(TAG, "摄像头帧数据为空");
        esp_camera_fb_return(fb);
        return NULL;
    }

    return fb;
}

esp_err_t video_get_jpeg(uint8_t **buf, size_t *len)
{
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

    /* 释放之前缓存的帧 */
    if (s_latest_frame != NULL) {
        esp_camera_fb_return(s_latest_frame);
    }
    s_latest_frame = fb;

    *buf = fb->buf;
    *len = fb->len;

    unlock_video();
    return ESP_OK;
}

void video_release_jpeg(void)
{
    if (!lock_video(200)) {
        return;
    }
    /* 注意：不在这里释放帧，而是在下一次 get_jpeg 时替换 */
    unlock_video();
}

/* ========================================
 * 公共接口：参数配置
 * ======================================== */

esp_err_t video_set_parameter(video_param_type_t type, int value)
{
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
            if (value < -2 || value > 2) return ESP_ERR_INVALID_ARG;
            ret = s->set_brightness(s, value);
            VIDEO_LOGI(TAG, "设置亮度: %d", value);
            break;

        case VIDEO_PARAM_CONTRAST:
            if (value < -2 || value > 2) return ESP_ERR_INVALID_ARG;
            ret = s->set_contrast(s, value);
            VIDEO_LOGI(TAG, "设置对比度: %d", value);
            break;

        case VIDEO_PARAM_SATURATION:
            if (value < -2 || value > 2) return ESP_ERR_INVALID_ARG;
            ret = s->set_saturation(s, value);
            VIDEO_LOGI(TAG, "设置饱和度: %d", value);
            break;

        case VIDEO_PARAM_GAIN:
            if (value < 0 || value > 30) return ESP_ERR_INVALID_ARG;
            /* 先关闭自动增益再设置手动增益 */
            s->set_gain_ctrl(s, 0);
            vTaskDelay(pdMS_TO_TICKS(10));
            ret = s->set_agc_gain(s, value);
            VIDEO_LOGI(TAG, "设置手动增益: %d", value);
            break;

        case VIDEO_PARAM_EXPOSURE:
            if (value < 0 || value > 1200) return ESP_ERR_INVALID_ARG;
            /* 先关闭自动曝光再设置手动曝光 */
            s->set_exposure_ctrl(s, 0);
            vTaskDelay(pdMS_TO_TICKS(10));
            ret = s->set_aec_value(s, value);
            VIDEO_LOGI(TAG, "设置手动曝光: %d", value);
            break;

        case VIDEO_PARAM_HMIRROR:
            ret = s->set_hmirror(s, value ? 1 : 0);
            VIDEO_LOGI(TAG, "设置水平镜像: %s", value ? "开启" : "关闭");
            break;

        case VIDEO_PARAM_VFLIP:
            ret = s->set_vflip(s, value ? 1 : 0);
            VIDEO_LOGI(TAG, "设置垂直翻转: %s", value ? "开启" : "关闭");
            break;

        case VIDEO_PARAM_QUALITY:
            if (value < 0 || value > 63) return ESP_ERR_INVALID_ARG;
            /* JPEG质量需要重新初始化摄像头 */
            VIDEO_LOGW(TAG, "JPEG质量设置将在下次初始化后生效 (当前: %d, 请求: %d)",
                     camera_config.jpeg_quality, value);
            camera_config.jpeg_quality = value;
            break;

        case VIDEO_PARAM_FRAMESIZE:
            ret = video_set_framesize((framesize_t)value);
            break;

        default:
            ret = ESP_ERR_INVALID_ARG;
            break;
    }

    return ret;
}

int video_get_parameter(video_param_type_t type)
{
    if (!s_video_initialized) {
        return -1;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) {
        return -1;
    }

    switch (type) {
        case VIDEO_PARAM_BRIGHTNESS: return s->status.brightness;
        case VIDEO_PARAM_CONTRAST:   return s->status.contrast;
        case VIDEO_PARAM_SATURATION: return s->status.saturation;
        case VIDEO_PARAM_GAIN:       return s->status.agc_gain;
        case VIDEO_PARAM_EXPOSURE:   return s->status.aec_value;
        case VIDEO_PARAM_HMIRROR:    return s->status.hmirror ? 1 : 0;
        case VIDEO_PARAM_VFLIP:      return s->status.vflip ? 1 : 0;
        case VIDEO_PARAM_QUALITY:    return (int)camera_config.jpeg_quality;
        case VIDEO_PARAM_FRAMESIZE:  return (int)s_current_framesize;
        default:                     return -1;
    }
}

esp_err_t video_set_framesize(framesize_t framesize)
{
    if (!s_video_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) {
        return ESP_FAIL;
    }

    /* 验证帧大小是否在支持列表中 */
    bool valid = false;
    for (uint32_t i = 0; i < FRAMESIZE_LIST_COUNT; i++) {
        if (s_framesize_list[i].framesize == framesize) {
            valid = true;
            break;
        }
    }
    if (!valid) {
        VIDEO_LOGE(TAG, "不支持的帧大小: %d", framesize);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = s->set_framesize(s, framesize);
    if (ret == ESP_OK) {
        s_current_framesize = framesize;
        uint16_t w = 0, h = 0;
        framesize_to_wh(framesize, &w, &h);
        VIDEO_LOGI(TAG, "帧大小已设置: %ux%u", w, h);
    } else {
        VIDEO_LOGE(TAG, "设置帧大小失败: %s", esp_err_to_name(ret));
    }

    return ret;
}

/* ========================================
 * 公共接口：信息查询
 * ======================================== */

esp_err_t video_get_info_struct(video_info_t *info)
{
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(info, 0, sizeof(video_info_t));

    /* 初始化中也返回基本状态，供前端显示 */
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
        strncpy(info->sensor_name, get_sensor_name(s->id.PID),
                sizeof(info->sensor_name) - 1);
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

    info->framesize = s_current_framesize;
    framesize_to_wh(s_current_framesize, &info->width, &info->height);
    info->jpeg_quality = camera_config.jpeg_quality;

    return ESP_OK;
}

const video_framesize_info_t* video_get_framesize_list(uint32_t *out_count)
{
    if (out_count) {
        *out_count = FRAMESIZE_LIST_COUNT;
    }
    return s_framesize_list;
}
