/**
 * @file video_web.h
 * @brief 视频监控 Web API 接口
 */

#ifndef VIDEO_WEB_H
#define VIDEO_WEB_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief 注册视频模块的所有 URI 路由
 * @param server HTTP 服务器句柄
 */
void video_web_register_routes(httpd_handle_t server);

esp_err_t video_web_info_handler(httpd_req_t *req);
esp_err_t video_web_status_handler(httpd_req_t *req);
esp_err_t video_web_framesizes_handler(httpd_req_t *req);
esp_err_t video_web_stream_handler(httpd_req_t *req);
esp_err_t video_web_snapshot_handler(httpd_req_t *req);
esp_err_t video_web_config_handler(httpd_req_t *req);
esp_err_t video_web_start_stream_handler(httpd_req_t *req);
esp_err_t video_web_stop_stream_handler(httpd_req_t *req);

#endif /* VIDEO_WEB_H */