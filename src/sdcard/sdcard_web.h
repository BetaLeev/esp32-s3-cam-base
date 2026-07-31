/**
 * @file sdcard_web.h
 * @brief TF卡Web文件管理HTTP处理模块接口
 */

#ifndef SDCARD_WEB_H
#define SDCARD_WEB_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief API: 获取 TF 卡文件列表
 */
esp_err_t sdcard_web_files_handler(httpd_req_t *req);

/**
 * @brief API: 获取 TF 卡信息
 */
esp_err_t sdcard_web_info_handler(httpd_req_t *req);

/**
 * @brief 下载 TF 卡文件
 */
esp_err_t sdcard_web_download_handler(httpd_req_t *req);

/**
 * @brief 上传文件到 TF 卡
 */
esp_err_t sdcard_web_upload_handler(httpd_req_t *req);

/**
 * @brief 调试: 打印文件列表到串口
 */
esp_err_t sdcard_web_debug_handler(httpd_req_t *req);

#endif // SDCARD_WEB_H
