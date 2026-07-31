/**
 * @file spiffs_web.h
 * @brief SPIFFS Web 文件系统支持
 * 
 * 功能：
 * - 初始化 SPIFFS 文件系统
 * - 提供静态文件读取接口
 * - 自动挂载 web 目录到 /spiffs
 * 
 * 使用方法：
 * 1. 在 main.c 中调用 spiffs_web_init() 初始化
 * 2. 前端文件放在 data/web/ 目录（CMakeLists.txt 会打包）
 * 3. http_server.c 通过 spiffs_web_read_file() 读取文件
 */

#ifndef SPIFFS_WEB_H
#define SPIFFS_WEB_H

#include "esp_err.h"
#include "esp_vfs.h"

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

#endif /* SPIFFS_WEB_H */
