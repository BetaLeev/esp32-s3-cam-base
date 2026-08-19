/**
 * @file ai_ws.h
 * @brief AI WebSocket 接口定义
 *
 * 提供 WebSocket 接口用于接收音频流
 */
#ifndef AI_WS_H
#define AI_WS_H

#include "esp_err.h"
#include "esp_http_server.h"
esp_err_t ai_ws_send_text_if_connected(const char *data);
/**
 * @brief 注册 AI WebSocket 路由
 */
esp_err_t ai_ws_register_routes(httpd_handle_t handle);

#endif /* AI_WS_H */
