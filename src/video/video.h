/**
 * @file video.h
 * @brief 摄像头视频模块接口定义
 *
 * 参考 esp32-camera 官方组件实现
 */
#ifndef VIDEO_H
#define VIDEO_H

#include "esp_err.h"
#include "esp_camera.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 视频参数类型
 */
typedef enum {
    VIDEO_PARAM_BRIGHTNESS = 0,  /**< 亮度 -2 ~ 2 */
    VIDEO_PARAM_CONTRAST,        /**< 对比度 -2 ~ 2 */
    VIDEO_PARAM_SATURATION,      /**< 饱和度 -2 ~ 2 */
    VIDEO_PARAM_GAIN,             /**< 增益 0 ~ 30 */
    VIDEO_PARAM_EXPOSURE,         /**< 曝光 0 ~ 1200 */
    VIDEO_PARAM_HMIRROR,          /**< 水平镜像 0/1 */
    VIDEO_PARAM_VFLIP,            /**< 垂直翻转 0/1 */
    VIDEO_PARAM_QUALITY,          /**< JPEG质量 0~63 (越小越好) */
    VIDEO_PARAM_FRAMESIZE,        /**< 帧大小 framesize_t */
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
    bool initialized;             /**< 是否已初始化 */
    bool streaming;               /**< 是否正在流传输 */
    char sensor_name[16];         /**< 传感器型号 */
    framesize_t framesize;        /**< 当前帧大小 */
    uint16_t width;               /**< 当前宽度 */
    uint16_t height;              /**< 当前高度 */
    uint8_t jpeg_quality;         /**< JPEG质量 */
    int brightness;               /**< 亮度 */
    int contrast;                 /**< 对比度 */
    int saturation;               /**< 饱和度 */
    int gain;                     /**< 增益 */
    int exposure;                 /**< 曝光 */
    bool hmirror;                 /**< 水平镜像 */
    bool vflip;                   /**< 垂直翻转 */
} video_info_t;

/* ========================================
 * 生命周期接口
 * ======================================== */

/**
 * @brief 初始化视频模块（同步）- 会阻塞直到完成
 */
esp_err_t video_init(void);

/**
 * @brief 异步启动视频模块初始化（不阻塞）
 *        创建后台任务执行 esp_camera_init()
 */
esp_err_t video_init_async(void);

/**
 * @brief 反初始化视频模块
 */
esp_err_t video_deinit(void);

/* ========================================
 * 状态接口
 * ======================================== */

/**
 * @brief 获取摄像头是否已初始化
 */
bool video_is_initialized(void);

/**
 * @brief 是否正在异步初始化中
 */
bool video_is_init_in_progress(void);

/**
 * @brief 获取视频流状态
 */
bool video_is_streaming(void);

/**
 * @brief 启动视频流（内部启动采集）
 */
esp_err_t video_start_streaming(void);

/**
 * @brief 停止视频流
 */
esp_err_t video_stop_streaming(void);

/* ========================================
 * 帧捕获接口
 * ======================================== */

/**
 * @brief 捕获一帧图像（调用者需调用 esp_camera_fb_return 释放）
 */
camera_fb_t* video_capture_frame(void);

/**
 * @brief 获取最新JPEG图像（带互斥锁保护，单帧缓存）
 * @param buf 输出JPEG缓冲区指针
 * @param len 输出JPEG长度
 * @note 调用者必须调用 video_release_jpeg() 释放
 */
esp_err_t video_get_jpeg(uint8_t **buf, size_t *len);

/**
 * @brief 释放JPEG缓冲区引用
 */
void video_release_jpeg(void);

/* ========================================
 * 参数配置接口
 * ======================================== */

/**
 * @brief 设置摄像头参数
 */
esp_err_t video_set_parameter(video_param_type_t type, int value);

/**
 * @brief 获取摄像头参数
 */
int video_get_parameter(video_param_type_t type);

/**
 * @brief 设置帧大小
 */
esp_err_t video_set_framesize(framesize_t framesize);

/* ========================================
 * 信息查询接口
 * ======================================== */

/**
 * @brief 获取摄像头信息（填充结构体）
 */
esp_err_t video_get_info_struct(video_info_t *info);

/**
 * @brief 获取帧大小列表（支持的分辨率）
 * @param out_count 输出数量
 * @return 帧大小信息数组（只读，不需要释放）
 */
const video_framesize_info_t* video_get_framesize_list(uint32_t *out_count);

#endif /* VIDEO_H */
