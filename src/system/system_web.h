/**
 * @file system_web.h
 * @brief 系统管理 Web API 接口
 */
#ifndef SYSTEM_WEB_H
#define SYSTEM_WEB_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief 注册系统模块的所有 URI 路由
 * @param server HTTP 服务器句柄
 */
void system_web_register_routes(httpd_handle_t server);

/* ========================================
 * 统一状态接口
 * ======================================== */

/**
 * @brief API: 获取完整系统状态（统一接口）
 * GET /api/status
 */
esp_err_t status_web_handler(httpd_req_t *req);

/* ========================================
 * 板子信息接口
 * ======================================== */

/**
 * @brief API: 获取板子基本信息
 * GET /api/system/info
 */
esp_err_t system_web_info_handler(httpd_req_t *req);

/* ========================================
 * 温度接口
 * ======================================== */

/**
 * @brief API: 获取温度数据
 * GET /api/system/temp
 */
esp_err_t system_web_temp_handler(httpd_req_t *req);

/* ========================================
 * 系统控制接口
 * ======================================== */

/**
 * @brief API: 系统重启
 * POST /api/system/reboot
 */
esp_err_t system_web_reboot_handler(httpd_req_t *req);

/**
 * @brief API: 系统关机（深度睡眠）
 * POST /api/system/shutdown
 */
esp_err_t system_web_shutdown_handler(httpd_req_t *req);

/* ========================================
 * 其他接口
 * ======================================== */

/**
 * @brief API: 获取系统状态
 * GET /api/system/status
 */
esp_err_t system_web_status_handler(httpd_req_t *req);

/**
 * @brief API: 获取硬件资源信息
 * GET /api/system/resources
 */
esp_err_t system_web_resources_handler(httpd_req_t *req);

#endif /* SYSTEM_WEB_H */
