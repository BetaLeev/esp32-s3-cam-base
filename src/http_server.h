/**
 * @file http_server.h
 * @brief HTTP服务器模块头文件 - 嵌入式Web页面与API接口
 */
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_err.h"

/**
 * @brief HTTP服务器初始化
 * @note 创建HTTP服务器，注册URI处理器
 * @return 初始化成功返回ESP_OK
 */
esp_err_t http_server_init(void);

/**
 * @brief 获取系统状态JSON字符串
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 成功返回JSON字符串长度
 */
int http_server_get_status_json(char* buffer, size_t buffer_size);

#endif /* HTTP_SERVER_H */
