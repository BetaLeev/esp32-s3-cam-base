/**
 * @file web_module.h
 * @brief Web模块头文件 - 静态文件服务
 */
#ifndef WEB_MODULE_H
#define WEB_MODULE_H

#include <stddef.h>
#include "esp_err.h"

/**
 * @brief 初始化Web模块
 */
esp_err_t web_module_init(void);

/**
 * @brief 获取HTML内容
 */
const char* web_module_get_html(size_t* len);

/**
 * @brief 获取CSS内容
 */
const char* web_module_get_css(size_t* len);

/**
 * @brief 获取JS内容
 */
const char* web_module_get_js(size_t* len);

#endif /* WEB_MODULE_H */
