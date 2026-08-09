/**
 * @file video.h
 * @brief 摄像头视频模块接口定义
 *
 * 参考 esp32-camera 官方组件实现
 */
#ifndef VIDEO_H
#define VIDEO_H

#include "esp_camera.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 视频参数类型
 */
typedef enum {
    VIDEO_PARAM_BRIGHTNESS = 0, /**< 亮度 -2 ~ 2 */
    VIDEO_PARAM_CONTRAST,       /**< 对比度 -2 ~ 2 */
    VIDEO_PARAM_SATURATION,     /**< 饱和度 -2 ~ 2 */
    VIDEO_PARAM_GAIN,           /**< 增益 0 ~ 30 */
    VIDEO_PARAM_EXPOSURE,       /**< 曝光 0 ~ 1200 */
    VIDEO_PARAM_HMIRROR,        /**< 水平镜像 0/1 */
    VIDEO_PARAM_VFLIP,          /**< 垂直翻转 0/1 */
    VIDEO_PARAM_QUALITY,        /**< JPEG质量 0~63 (越小越好) */
    VIDEO_PARAM_FRAMESIZE,      /**< 帧大小 framesize_t */
    VIDEO_PARAM_AWB,            /**< 自动白平衡开关 0/1 */
    VIDEO_PARAM_WB_MODE,        /**< 白平衡模式 0:Auto, 1:Sunny, 2:Cloudy, 3:Office, 4:Home */
    VIDEO_PARAM_AEC,            /**< 自动曝光开关 0/1 */
    VIDEO_PARAM_AGC,            /**< 自动增益开关 0/1 */
    VIDEO_PARAM_COUNT
} video_param_type_t;

/**
 * @brief 帧大小信息
 */
typedef struct {
    framesize_t framesize;
    uint16_t width;
    uint16_t height;
    const char *name;
} video_framesize_info_t;

/**
 * @brief 摄像头详细信息
 */
typedef struct {
    bool initialized;      /**< 是否已初始化 */
    bool streaming;        /**< 是否正在流传输 */
    char sensor_name[16];  /**< 传感器型号 */
    framesize_t framesize; /**< 当前帧大小 */
    uint16_t width;        /**< 当前宽度 */
    uint16_t height;       /**< 当前高度 */
    uint8_t jpeg_quality;  /**< JPEG质量 */
    int brightness;        /**< 亮度 */
    int contrast;          /**< 对比度 */
    int saturation;        /**< 饱和度 */
    int gain;              /**< 增益 */
    int exposure;          /**< 曝光 */
    bool hmirror;          /**< 水平镜像 */
    bool vflip;            /**< 垂直翻转 */
} video_info_t;

/* ========================================
 * 生命周期接口
 * ======================================== */

esp_err_t video_init(void);
esp_err_t video_init_async(void);
esp_err_t video_deinit(void);

/* ========================================
 * 状态接口
 * ======================================== */

bool video_is_initialized(void);
bool video_is_init_in_progress(void);
bool video_is_streaming(void);
esp_err_t video_start_streaming(void);
esp_err_t video_stop_streaming(void);

/* ========================================
 * 帧捕获接口
 * ======================================== */

camera_fb_t *video_capture_frame(void);
esp_err_t video_get_jpeg(uint8_t **buf, size_t *len);
void video_release_jpeg(void);

/* ========================================
 * 参数配置接口
 * ======================================== */

esp_err_t video_set_parameter(video_param_type_t type, int value);
int video_get_parameter(video_param_type_t type);
esp_err_t video_set_framesize(framesize_t framesize);

/* ========================================
 * 信息查询接口
 * ======================================== */

esp_err_t video_get_info_struct(video_info_t *info);
const video_framesize_info_t *video_get_framesize_list(uint32_t *out_count);

#endif /* VIDEO_H */