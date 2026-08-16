/**
 * @file web.h
 * @brief Web 模块统一头文件
 * 
 * 包含: DNS 服务器、HTTP 服务器、文件系统
 */
#ifndef WEB_H
#define WEB_H

#include "esp_err.h"

/**
 * @brief Web 模块初始化 (一次性初始化所有子模块)
 */
esp_err_t web_init(void);

/**
 * @brief Web 模块反初始化
 */
esp_err_t web_deinit(void);

#endif // WEB_H
