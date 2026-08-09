#ifndef VIDEO_WS_H
#define VIDEO_WS_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief 注册视频 WebSocket 路由
 * @param handle httpd 服务器句柄
 * @return ESP_OK 成功
 */
esp_err_t video_ws_register(httpd_handle_t handle);

#endif // VIDEO_WS_H
