/**
 * @file spiffs_web.h
 * @brief SPIFFS Web 文件系统支持
 */

#ifndef SPIFFS_WEB_H
#define SPIFFS_WEB_H

#include "esp_err.h"
#include "esp_vfs.h"
#include "esp_http_server.h"

/**
 * @brief Web 文件系统配置
 */
#define WEBFS_BASE_PATH "/spiffs"
#define WEBFS_PARTITION_LABEL "webfs"

/**
 * @brief 初始化 Web 文件系统
 * @return ESP_OK 成功，其他失败
 */
esp_err_t spiffs_web_init(void);

/**
 * @brief 读取文件内容
 * @param path 文件路径（相对于 /spiffs）
 * @param out_buffer 输出缓冲区
 * @param out_len 输出长度
 * @return ESP_OK 成功，其他失败
 */
esp_err_t spiffs_web_read_file(const char *path, char **out_buffer, size_t *out_len);

/**
 * @brief 获取SPIFFS文件系统信息
 * @param out_total 总容量
 * @param out_free 可用空间
 * @return ESP_OK 成功，其他失败
 */
esp_err_t spiffs_web_get_info(size_t *out_total, size_t *out_free);

/**
 * @brief 读取并向 HTTP 客户端发送静态文件（通配符处理 Handler）
 * @param req HTTP 请求句柄
 * @param filepath 文件路径（例如 "web/index.html" 或 "web/app.js"）
 * @return ESP_OK 成功，其他失败
 */
esp_err_t spiffs_web_file_handler(httpd_req_t *req, const char *filepath);

#endif /* SPIFFS_WEB_H */