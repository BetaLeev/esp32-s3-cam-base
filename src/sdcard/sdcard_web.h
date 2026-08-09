/**
 * @file sdcard_web.h
 * @brief TF卡Web文件管理HTTP处理模块接口
 */

#ifndef SDCARD_WEB_H
#define SDCARD_WEB_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief 注册当前模块的所有 URI 路由
 * @param server HTTP 服务器句柄
 */
void sdcard_web_register_routes(httpd_handle_t server);

/* ========================================
 * API 处理器声明
 * ======================================== */
esp_err_t sdcard_web_files_handler(httpd_req_t *req);
esp_err_t sdcard_web_info_handler(httpd_req_t *req);
esp_err_t sdcard_web_download_handler(httpd_req_t *req);
esp_err_t sdcard_web_upload_handler(httpd_req_t *req);
esp_err_t sdcard_web_mkdir_handler(httpd_req_t *req);
esp_err_t sdcard_web_delete_handler(httpd_req_t *req);
esp_err_t sdcard_web_dirs_handler(httpd_req_t *req);
esp_err_t sdcard_web_dirsize_handler(httpd_req_t *req);
esp_err_t sdcard_web_debug_handler(httpd_req_t *req);

#endif // SDCARD_WEB_H