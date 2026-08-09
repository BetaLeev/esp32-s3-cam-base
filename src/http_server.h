#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"

// 初始化 HTTP 服务器
esp_err_t http_server_init(void);

// 获取 HTTP 服务器句柄
httpd_handle_t get_httpd_handle(void);

#endif /* HTTP_SERVER_H */