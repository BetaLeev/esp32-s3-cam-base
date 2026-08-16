/**
 * @file filesystem.h
 * @brief LittleFS 文件系统统一管理
 */
#ifndef WEB_FILESYSTEM_H
#define WEB_FILESYSTEM_H

#include "esp_err.h"
#include "esp_vfs.h"
#include "esp_http_server.h"

/**
 * @brief 初始化 Web 文件系统
 */
esp_err_t web_filesystem_init(void);

/**
 * @brief 获取文件系统信息
 */
esp_err_t web_filesystem_get_info(size_t *total, size_t *used);

/**
 * @brief 读取文件内容
 */
esp_err_t web_filesystem_read_file(const char *path, char **out_buffer, size_t *out_len);

/**
 * @brief 发送静态文件到 HTTP 客户端
 */
esp_err_t web_filesystem_serve_file(httpd_req_t *req, const char *filepath);

#endif // WEB_FILESYSTEM_H