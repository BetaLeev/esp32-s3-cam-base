/**
 * @file spiffs_web.c
 * @brief SPIFFS Web 文件系统实现
 */

#include "spiffs_web.h"
#include "config.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include <sys/stat.h>

static const char *TAG = "SPIFFS_WEB";
#define LOG_TAG TAG

/**
 * @brief 初始化 Web 文件系统
 */
esp_err_t spiffs_web_init(void)
{
    SPIFFS_LOGI(TAG, "初始化 SPIFFS 文件系统...");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = WEBFS_BASE_PATH,
        .partition_label = WEBFS_PARTITION_LABEL,
        .max_files = 10,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS挂载失败: %s", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(WEBFS_PARTITION_LABEL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取SPIFFS信息失败");
        return ret;
    }

    ESP_LOGI(TAG, "SPIFFS挂载成功");
    ESP_LOGI(TAG, "总空间: %d bytes, 已使用: %d bytes", total, used);

    return ESP_OK;
}

/**
 * @brief 读取文件内容
 */
esp_err_t spiffs_web_read_file(const char *path, char **out_buffer, size_t *out_len)
{
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s/%s", WEBFS_BASE_PATH, path);

    // 使用文本模式读取
    FILE *f = fopen(full_path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法打开文件: %s", full_path);
        return ESP_FAIL;
    }

    // 获取文件大小
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(f);
        ESP_LOGE(TAG, "文件大小无效: %s", full_path);
        return ESP_FAIL;
    }

    // 分配内存
    char *buffer = malloc(file_size + 1);
    if (buffer == NULL) {
        fclose(f);
        ESP_LOGE(TAG, "内存分配失败");
        return ESP_ERR_NO_MEM;
    }

    // 读取文件
    size_t read_len = fread(buffer, 1, file_size, f);
    buffer[read_len] = '\0';
    fclose(f);

    *out_buffer = buffer;
    *out_len = read_len;

    ESP_LOGI(TAG, "读取文件: %s (%d bytes)", path, read_len);

    return ESP_OK;
}

/**
 * @brief 获取SPIFFS文件系统信息
 */
esp_err_t spiffs_web_get_info(size_t *out_total, size_t *out_free)
{
    if (out_total == NULL || out_free == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info(WEBFS_PARTITION_LABEL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取SPIFFS信息失败");
        return ret;
    }

    *out_total = total;
    *out_free = total - used;

    return ESP_OK;
}
