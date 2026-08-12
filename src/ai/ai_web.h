/**
 * @file ai_web.h
 * @brief AI Web API 接口定义
 *
 * 提供 HTTP API 用于查询状态、命令列表等
 */
#ifndef AI_WEB_H
#define AI_WEB_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief 注册 AI 模块的 HTTP 路由
 */
esp_err_t ai_web_register_routes(httpd_handle_t handle);

#endif /* AI_WEB_H */
