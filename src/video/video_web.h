/**
 * @file video_web.h
 * @brief 视频监控 Web API 接口
 *
 * RESTFUL API 规范：
 * - GET  /api/video/info       - 获取摄像头详细信息
 * - GET  /api/video/status     - 获取摄像头状态（简化）
 * - GET  /api/video/framesizes - 获取支持的分辨率列表
 * - GET  /api/video/stream     - MJPEG 实时视频流
 * - GET  /api/video/snapshot   - 获取单帧 JPEG 快照
 * - POST /api/video/config     - 设置摄像头参数
 * - POST /api/video/stream/start  - 启动视频流
 * - POST /api/video/stream/stop   - 停止视频流
 *
 * 统一响应格式：
 * @code
 * {
 *     "status": "success" | "error" | "warning",
 *     "code": 200,
 *     "message": "描述信息",
 *     "data": {...} | null
 * }
 * @endcode
 */

#ifndef VIDEO_WEB_H
#define VIDEO_WEB_H

#include "esp_err.h"
#include "esp_http_server.h"

/* ========================================
 * 信息查询 API
 * ======================================== */

/**
 * @brief API: 获取摄像头详细信息
 * GET /api/video/info
 */
esp_err_t video_web_info_handler(httpd_req_t *req);

/**
 * @brief API: 获取摄像头状态（简化版，用于前端轮询）
 * GET /api/video/status
 */
esp_err_t video_web_status_handler(httpd_req_t *req);

/**
 * @brief API: 获取支持的分辨率列表
 * GET /api/video/framesizes
 */
esp_err_t video_web_framesizes_handler(httpd_req_t *req);

/* ========================================
 * 视频流 / 快照 API
 * ======================================== */

/**
 * @brief API: MJPEG 实时视频流
 * GET /api/video/stream
 * 返回 multipart/x-mixed-replace 流
 */
esp_err_t video_web_stream_handler(httpd_req_t *req);

/**
 * @brief API: 获取单帧 JPEG 快照
 * GET /api/video/snapshot
 * 返回 image/jpeg 图片
 */
esp_err_t video_web_snapshot_handler(httpd_req_t *req);

/* ========================================
 * 配置 / 控制 API
 * ======================================== */

/**
 * @brief API: 设置摄像头参数（JSON Body）
 * POST /api/video/config
 *
 * 请求体示例:
 * @code
 * {
 *     "brightness": 0,
 *     "contrast": 0,
 *     "saturation": 0,
 *     "hmirror": false,
 *     "vflip": false,
 *     "framesize": 8
 * }
 * @endcode
 */
esp_err_t video_web_config_handler(httpd_req_t *req);

/**
 * @brief API: 启动视频流
 * POST /api/video/stream/start
 */
esp_err_t video_web_start_stream_handler(httpd_req_t *req);

/**
 * @brief API: 停止视频流
 * POST /api/video/stream/stop
 */
esp_err_t video_web_stop_stream_handler(httpd_req_t *req);

#endif /* VIDEO_WEB_H */
