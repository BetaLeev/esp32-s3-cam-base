/**
 * @file sdcard_web.h
 * @brief TF卡Web文件管理HTTP处理模块接口
 *
 * RESTFUL API 规范：
 * - GET /api/sdcard/files - 获取文件列表
 * - GET /api/sdcard/info - 获取TF卡信息
 * - GET /api/sdcard/dirs - 获取目录列表
 * - GET /api/sdcard/dirsize - 计算目录大小
 * - GET /fs - 下载/预览文件
 * - POST /api/sdcard/upload - 上传文件
 * - POST /api/sdcard/mkdir - 创建目录
 * - POST /api/sdcard/delete - 删除文件或目录
 *
 * 统一响应格式：
 * @code
 * {
 *     "status": "success" | "error" | "warning",
 *     "code": 200,
 *     "message": "描述信息",
 *     "data": {...} | null
 * }
 * @endcode
 */

#ifndef SDCARD_WEB_H
#define SDCARD_WEB_H

#include "esp_err.h"
#include "esp_http_server.h"

/* ========================================
 * 文件列表 API
 * GET /api/sdcard/files?path=xxx
 * ======================================== */
esp_err_t sdcard_web_files_handler(httpd_req_t *req);

/* ========================================
 * TF卡信息 API
 * GET /api/sdcard/info
 * ======================================== */
esp_err_t sdcard_web_info_handler(httpd_req_t *req);

/* ========================================
 * 下载/预览文件
 * GET /fs?path=xxx
 * ======================================== */
esp_err_t sdcard_web_download_handler(httpd_req_t *req);

/* ========================================
 * 上传文件 API
 * POST /api/sdcard/upload
 * 请求参数: path=目录路径, filename=文件名
 * 请求体: 文件二进制数据
 * ======================================== */
esp_err_t sdcard_web_upload_handler(httpd_req_t *req);

/* ========================================
 * 创建目录 API
 * POST /api/sdcard/mkdir
 * 请求体: {"path": "目录路径"}
 * ======================================== */
esp_err_t sdcard_web_mkdir_handler(httpd_req_t *req);

/* ========================================
 * 删除文件或目录 API
 * POST /api/sdcard/delete
 * 请求体: {"path": "文件或目录路径"}
 * ======================================== */
esp_err_t sdcard_web_delete_handler(httpd_req_t *req);

/* ========================================
 * 获取目录列表 API (懒加载优化)
 * GET /api/sdcard/dirs?path=xxx
 * ======================================== */
esp_err_t sdcard_web_dirs_handler(httpd_req_t *req);

/* ========================================
 * 计算目录大小 API
 * GET /api/sdcard/dirsize?path=xxx
 * ======================================== */
esp_err_t sdcard_web_dirsize_handler(httpd_req_t *req);

/* ========================================
 * 调试: 打印文件列表到串口
 * GET /api/sdcard/debug
 * ======================================== */
esp_err_t sdcard_web_debug_handler(httpd_req_t *req);

#endif // SDCARD_WEB_H
