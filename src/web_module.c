/**
 * @file web_module.c
 * @brief Web模块 - 提供前端静态文件的嵌入支持
 * 
 * 前端文件位于 src/web/ 目录，编译时通过 CMakeLists.txt 的 EMBED_FILES 嵌入
 * 修改前端代码请直接编辑 src/web/ 目录下的文件
 */

#include "web_module.h"
#include "esp_log.h"

static const char *TAG = "WEB_MODULE";

/**
 * @brief 初始化Web模块
 */
esp_err_t web_module_init(void)
{
    ESP_LOGI(TAG, "Web模块初始化完成");
    ESP_LOGI(TAG, "前端文件位于: src/web/");
    return ESP_OK;
}
