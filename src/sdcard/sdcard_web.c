/**
 * @file sdcard_web.c
 * @brief TF卡Web文件管理HTTP处理模块
 *
 * 使用ESP-IDF官方VFS接口，简洁可靠
 */

#include "sdcard_web.h"
#include "../config.h"
#include "../web_module.h"
#include "../utils/path_utils.h"
#include "../utils/mime_utils.h"
#include "esp_http_server.h"
#include "ff.h"
#include "sdcard.h"
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "SDCARD_WEB";

// ============================================================================
// 互斥锁管理 - 使用静态初始化避免竞态条件
// ============================================================================

static SemaphoreHandle_t s_mutex = NULL;
static StaticSemaphore_t s_mutex_buffer;

static void init_mutex(void)
{
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buffer);
    }
}

static void lock_sd(void)
{
    init_mutex();
    if (s_mutex != NULL) {
        xSemaphoreTake(s_mutex, pdMS_TO_TICKS(5000));
    }
}

static void unlock_sd(void)
{
    if (s_mutex != NULL) {
        xSemaphoreGive(s_mutex);
    }
}

// ============================================================================
// 辅助函数：URL解码（httpd_query_key_value不解码）
// ============================================================================

static void url_decode_inplace(char *str, size_t max_len)
{
    if (!str) return;
    char *src = str;
    char *dst = str;
    
    while (*src && (size_t)(dst - str) < max_len - 1) {
        if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], '\0'};
            *dst = (char)strtol(hex, NULL, 16);
            dst++;
            src += 3;
        } else if (*src == '+') {
            *dst = ' ';
            dst++;
            src++;
        } else {
            *dst = *src;
            dst++;
            src++;
        }
    }
    *dst = '\0';
}

// ============================================================================
// 辅助函数：递归删除目录及其内容
// ============================================================================

static esp_err_t remove_recursive(const char *path)
{
    if (!path) return ESP_ERR_INVALID_ARG;

    struct stat st;
    if (stat(path, &st) != 0) {
        SDCARD_WEB_LOGE(TAG, "stat失败: %s (errno=%d)", path, errno);
        return ESP_FAIL;
    }

    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        if (!dir) {
            SDCARD_WEB_LOGE(TAG, "opendir失败: %s", path);
            return ESP_FAIL;
        }

        char *child_path = malloc(4096);
        if (!child_path) {
            closedir(dir);
            return ESP_ERR_NO_MEM;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            snprintf(child_path, 4096, "%s/%s", path, entry->d_name);
            remove_recursive(child_path);
        }

        free(child_path);
        closedir(dir);

        if (rmdir(path) != 0) {
            SDCARD_WEB_LOGE(TAG, "rmdir失败: %s (errno=%d)", path, errno);
            return ESP_FAIL;
        }
    } else {
        if (unlink(path) != 0) {
            SDCARD_WEB_LOGE(TAG, "unlink失败: %s (errno=%d)", path, errno);
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

// ============================================================================
// 辅助函数：递归创建目录 (使用堆内存，防止栈溢出)
// ============================================================================

static esp_err_t ensure_dirs_exist(const char *base_path, const char *relative_path)
{
    if (relative_path == NULL || relative_path[0] == '\0') {
        return ESP_OK;  // 空路径，无需创建
    }

    char *path_copy = malloc(1024);
    if (!path_copy) return ESP_ERR_NO_MEM;
    strncpy(path_copy, relative_path, 1023);
    path_copy[1023] = '\0';

    char *full_path = malloc(2048);
    if (!full_path) {
        free(path_copy);
        return ESP_ERR_NO_MEM;
    }

    char *token = path_copy;
    while (token != NULL && token[0] != '\0') {
        char *slash = strchr(token, '/');
        if (slash != NULL) {
            *slash = '\0';
        }

        if (token[0] != '\0') {
            snprintf(full_path, 2048, "%s/%s", base_path, token);
            mkdir(full_path, 0755);  // 忽略返回值，只确保目录存在
        }

        token = slash ? slash + 1 : NULL;
    }

    free(path_copy);
    free(full_path);
    return ESP_OK;
}

// ============================================================================
// API: 获取 TF 卡信息
// ============================================================================
esp_err_t sdcard_web_info_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    cJSON *data = cJSON_CreateObject();

    if (!sdcard_is_mounted()) {
        cJSON_AddBoolToObject(data, "mounted", false);
        return send_success(req, data, "TF卡未挂载");
    }

    uint64_t total = 0, free_space = 0;
    sdcard_get_info(&total, &free_space);

    cJSON_AddBoolToObject(data, "mounted", true);
    cJSON_AddNumberToObject(data, "total", (double)total);
    cJSON_AddNumberToObject(data, "free", (double)free_space);
    cJSON_AddNumberToObject(data, "used", (double)(total > free_space ? total - free_space : 0));
    cJSON_AddStringToObject(data, "mount_point", sdcard_get_mount_point());
    cJSON_AddStringToObject(data, "image_ext", sdcard_get_image_extensions());
    cJSON_AddStringToObject(data, "video_ext", sdcard_get_video_extensions());

    return send_success(req, data, "获取TF卡信息成功");
}

// ============================================================================
// API: 获取文件列表
// ============================================================================
esp_err_t sdcard_web_files_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    lock_sd();

    if (!sdcard_is_mounted()) {
        unlock_sd();
        return send_error(req, "TF卡未挂载", HTTP_SERVICE_UNAVAILABLE);
    }

    // 获取路径参数 (使用堆内存)
    char *path_buf = malloc(256);
    char *query_buf = malloc(1024);
    char *full_path = malloc(1024);
    char *file_path_buf = malloc(2048); // 用于 stat 的临时路径
    if (!path_buf || !query_buf || !full_path || !file_path_buf) {
        free(path_buf); free(query_buf); free(full_path); free(file_path_buf);
        unlock_sd();
        return send_error(req, "内存分配失败", HTTP_INTERNAL_ERROR);
    }
    memset(path_buf, 0, 256);

    if (httpd_req_get_url_query_str(req, query_buf, 1024) == ESP_OK) {
        httpd_query_key_value(query_buf, "path", path_buf, 255);
    }

    // URL解码（httpd_query_key_value不解码 %2F 等）
    url_decode_inplace(path_buf, 256);

    // 路径安全检查
    if (path_buf[0] != '\0' && !path_is_safe(path_buf)) {
        SDCARD_WEB_LOGE(TAG, "FILES: 非法路径: %s", path_buf);
        free(path_buf); free(query_buf); free(full_path); free(file_path_buf);
        unlock_sd();
        return send_bad_request(req, "非法路径");
    }

    // 构建完整路径
    const char *mount = sdcard_get_mount_point();
    if (!path_build_vfs(mount, path_buf[0] != '\0' ? path_buf : NULL, full_path, 1024)) {
        SDCARD_WEB_LOGE(TAG, "FILES: 路径构建失败");
        free(path_buf); free(query_buf); free(full_path); free(file_path_buf);
        unlock_sd();
        return send_error(req, "路径构建失败", HTTP_INTERNAL_ERROR);
    }
    SDCARD_WEB_LOGI(TAG, "FILES: mount=%s, path_buf=%s, full_path=%s", mount, path_buf, full_path);

    // 使用VFS接口打开目录
    DIR *dir = opendir(full_path);
    if (dir == NULL) {
        SDCARD_WEB_LOGE(TAG, "FILES: opendir failed for %s", full_path);
        free(path_buf); free(query_buf); free(full_path); free(file_path_buf);
        unlock_sd();
        return send_error(req, "打开目录失败", HTTP_NOT_FOUND);
    }

    cJSON *files = cJSON_CreateArray();
    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL && count < 100) {
        if (entry->d_name[0] == '.') continue;

        cJSON *file = cJSON_CreateObject();
        cJSON_AddStringToObject(file, "name", entry->d_name);
        cJSON_AddBoolToObject(file, "is_dir", entry->d_type == DT_DIR);

        // 构建文件相对路径（用于下载URL）
        char *rel_path = malloc(512);
        if (rel_path) {
            if (path_buf[0] != '\0') {
                snprintf(rel_path, 512, "%s/%s", path_buf, entry->d_name);
            } else {
                snprintf(rel_path, 512, "%s", entry->d_name);
            }
            cJSON_AddStringToObject(file, "url", rel_path);
            free(rel_path);
        }

        // 获取文件大小（使用FatFS，更可靠）
        snprintf(file_path_buf, 2048, "%s/%s", full_path, entry->d_name);
        uint32_t file_size = 0;
        bool is_dir = false;
        if (sdcard_get_file_size(file_path_buf, &file_size, &is_dir) == ESP_OK) {
            cJSON_AddNumberToObject(file, "size", (double)file_size);
        } else {
            SDCARD_WEB_LOGW(TAG, "获取文件大小失败: %s", file_path_buf);
            cJSON_AddNumberToObject(file, "size", 0);
        }

        cJSON_AddItemToArray(files, file);
        count++;
    }

    closedir(dir);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "mounted", true);
    cJSON_AddStringToObject(data, "path", path_buf);
    cJSON_AddItemToObject(data, "files", files);
    cJSON_AddNumberToObject(data, "count", count);

    free(path_buf); free(query_buf); free(full_path); free(file_path_buf);
    unlock_sd();
    return send_success(req, data, "获取文件列表成功");
}

// ============================================================================
// API: 获取目录列表
// ============================================================================
esp_err_t sdcard_web_dirs_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    lock_sd();

    if (!sdcard_is_mounted()) {
        unlock_sd();
        return send_error(req, "TF卡未挂载", HTTP_SERVICE_UNAVAILABLE);
    }

    char *path_buf = malloc(256);
    char *query_buf = malloc(1024);
    char *full_path = malloc(1024);
    if (!path_buf || !query_buf || !full_path) {
        free(path_buf); free(query_buf); free(full_path);
        unlock_sd();
        return send_error(req, "内存分配失败", HTTP_INTERNAL_ERROR);
    }
    memset(path_buf, 0, 256);

    if (httpd_req_get_url_query_str(req, query_buf, 1024) == ESP_OK) {
        httpd_query_key_value(query_buf, "path", path_buf, 255);
    }

    // URL解码
    url_decode_inplace(path_buf, 256);

    // 路径安全检查
    if (path_buf[0] != '\0' && !path_is_safe(path_buf)) {
        free(path_buf); free(query_buf); free(full_path);
        unlock_sd();
        return send_bad_request(req, "非法路径");
    }

    // 使用统一的路径构建
    const char *mount = sdcard_get_mount_point();
    if (!path_build_vfs(mount, path_buf[0] != '\0' ? path_buf : NULL, full_path, 1024)) {
        free(path_buf); free(query_buf); free(full_path);
        unlock_sd();
        return send_error(req, "路径构建失败", HTTP_INTERNAL_ERROR);
    }

    DIR *dir = opendir(full_path);
    if (dir == NULL) {
        free(path_buf); free(query_buf); free(full_path);
        unlock_sd();
        return send_error(req, "打开目录失败", HTTP_NOT_FOUND);
    }

    cJSON *dirs = cJSON_CreateArray();
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (entry->d_type != DT_DIR) continue;

        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", entry->d_name);
        cJSON_AddItemToArray(dirs, item);
    }

    closedir(dir);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "mounted", true);
    cJSON_AddStringToObject(data, "path", path_buf);
    cJSON_AddItemToObject(data, "dirs", dirs);

    free(path_buf); free(query_buf); free(full_path);
    unlock_sd();
    return send_success(req, data, "获取目录列表成功");
}

// ============================================================================
// API: 创建目录
// ============================================================================
esp_err_t sdcard_web_mkdir_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_bad_request(req, "仅支持 POST 请求");
    }

    lock_sd();

    if (!sdcard_is_mounted()) {
        unlock_sd();
        return send_error(req, "TF卡未挂载", HTTP_SERVICE_UNAVAILABLE);
    }

    cJSON *json = parse_request_json(req);
    if (!json) {
        unlock_sd();
        return send_bad_request(req, "无效的 JSON 格式");
    }

    cJSON *path_item = cJSON_GetObjectItem(json, "path");
    if (!cJSON_IsString(path_item) || path_item->valuestring[0] == '\0') {
        cJSON_Delete(json);
        unlock_sd();
        return send_bad_request(req, "缺少 path 参数");
    }

    // 路径安全检查
    if (!path_is_safe(path_item->valuestring)) {
        cJSON_Delete(json);
        unlock_sd();
        return send_bad_request(req, "非法路径");
    }

    char *full_path = malloc(1024);
    if (!full_path) {
        cJSON_Delete(json);
        unlock_sd();
        return send_error(req, "内存分配失败", HTTP_INTERNAL_ERROR);
    }

    // 使用统一的路径构建
    const char *mount = sdcard_get_mount_point();
    if (!path_build_vfs(mount, path_item->valuestring, full_path, 1024)) {
        free(full_path);
        cJSON_Delete(json);
        unlock_sd();
        return send_error(req, "路径构建失败", HTTP_INTERNAL_ERROR);
    }

    cJSON_Delete(json);

    if (mkdir(full_path, 0755) != 0) {
        SDCARD_WEB_LOGE(TAG, "创建目录失败: %s (errno=%d)", full_path, errno);
        free(full_path);
        unlock_sd();
        return send_error(req, "创建目录失败", HTTP_INTERNAL_ERROR);
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "path", path_item->valuestring);

    free(full_path);
    unlock_sd();
    return send_success(req, data, "目录创建成功");
}

// ============================================================================
// API: 删除文件或目录
// ============================================================================
esp_err_t sdcard_web_delete_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_bad_request(req, "仅支持 POST 请求");
    }

    lock_sd();

    if (!sdcard_is_mounted()) {
        unlock_sd();
        return send_error(req, "TF卡未挂载", HTTP_SERVICE_UNAVAILABLE);
    }

    cJSON *json = parse_request_json(req);
    if (!json) {
        unlock_sd();
        return send_bad_request(req, "无效的 JSON 格式");
    }

    cJSON *path_item = cJSON_GetObjectItem(json, "path");
    if (!cJSON_IsString(path_item) || path_item->valuestring[0] == '\0') {
        cJSON_Delete(json);
        unlock_sd();
        return send_bad_request(req, "缺少 path 参数");
    }

    // 路径安全检查
    if (!path_is_safe(path_item->valuestring)) {
        cJSON_Delete(json);
        unlock_sd();
        return send_bad_request(req, "非法路径");
    }

    char *full_path = malloc(1024);
    if (!full_path) {
        cJSON_Delete(json);
        unlock_sd();
        return send_error(req, "内存分配失败", HTTP_INTERNAL_ERROR);
    }

    // 使用统一的路径构建
    const char *mount = sdcard_get_mount_point();
    if (!path_build_vfs(mount, path_item->valuestring, full_path, 1024)) {
        free(full_path);
        cJSON_Delete(json);
        unlock_sd();
        return send_error(req, "路径构建失败", HTTP_INTERNAL_ERROR);
    }

    cJSON_Delete(json);

    // 使用递归删除（支持非空目录和特殊字符路径）
    esp_err_t del_ret = remove_recursive(full_path);
    if (del_ret != ESP_OK) {
        SDCARD_WEB_LOGE(TAG, "删除失败: %s (ret=%d)", full_path, del_ret);
        free(full_path);
        unlock_sd();
        return send_error(req, "删除失败", HTTP_INTERNAL_ERROR);
    }

    SDCARD_WEB_LOGI(TAG, "删除成功: %s", full_path);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "path", path_item->valuestring);

    free(full_path);
    unlock_sd();
    return send_success(req, data, "删除成功");
}

// ============================================================================
// API: 上传文件
// ============================================================================
esp_err_t sdcard_web_upload_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_bad_request(req, "仅支持 POST 请求");
    }

    lock_sd();

    if (!sdcard_is_mounted()) {
        unlock_sd();
        return send_error(req, "TF卡未挂载", HTTP_SERVICE_UNAVAILABLE);
    }

    // 获取参数 (使用堆内存)
    char *target_dir = malloc(256);
    char *filename = malloc(256);
    char *query_buf = malloc(1024);
    char *full_path = malloc(1024);
    char *content_disp = malloc(1024);
    if (!target_dir || !filename || !query_buf || !full_path || !content_disp) {
        free(target_dir); free(filename); free(query_buf); free(full_path); free(content_disp);
        unlock_sd();
        return send_error(req, "内存分配失败", HTTP_INTERNAL_ERROR);
    }
    memset(target_dir, 0, 256);
    memset(filename, 0, 256);

    if (httpd_req_get_url_query_str(req, query_buf, 1024) == ESP_OK) {
        httpd_query_key_value(query_buf, "path", target_dir, 255);
        httpd_query_key_value(query_buf, "filename", filename, 255);
    }

    // URL解码路径参数
    url_decode_inplace(target_dir, 256);
    url_decode_inplace(filename, 256);

    if (filename[0] == '\0') {
        if (httpd_req_get_hdr_value_str(req, "Content-Disposition", content_disp, 1024) == ESP_OK) {
            char *start = strstr(content_disp, "filename=\"");
            if (start) {
                start += 10;
                char *end = strchr(start, '"');
                if (end && end - start < 256) {
                    strncpy(filename, start, end - start);
                    filename[end - start] = '\0';
                }
            }
        }
    }

    if (filename[0] == '\0') {
        free(target_dir); free(filename); free(query_buf); free(full_path); free(content_disp);
        unlock_sd();
        return send_bad_request(req, "未指定文件名");
    }

    // 路径安全检查
    if (target_dir[0] != '\0' && !path_is_safe(target_dir)) {
        SDCARD_WEB_LOGE(TAG, "UPLOAD: 非法路径: %s", target_dir);
        free(target_dir); free(filename); free(query_buf); free(full_path); free(content_disp);
        unlock_sd();
        return send_bad_request(req, "非法路径");
    }

    // 构建完整路径
    const char *mount = sdcard_get_mount_point();
    SDCARD_WEB_LOGI(TAG, "UPLOAD: mount=%s, target_dir=%s, filename=%s", mount, target_dir, filename);
    
    // 先确保目标目录存在
    if (target_dir[0] != '\0') {
        ensure_dirs_exist(mount, target_dir);
    }
    
    // 构建完整VFS路径
    char *rel_path = malloc(512);
    if (!rel_path) {
        free(target_dir); free(filename); free(query_buf); free(full_path); free(content_disp);
        unlock_sd();
        return send_error(req, "内存分配失败", HTTP_INTERNAL_ERROR);
    }
    
    if (target_dir[0] != '\0') {
        snprintf(rel_path, 512, "%s/%s", target_dir, filename);
    } else {
        snprintf(rel_path, 512, "%s", filename);
    }
    
    if (!path_build_vfs(mount, rel_path, full_path, 1024)) {
        SDCARD_WEB_LOGE(TAG, "UPLOAD: 路径构建失败");
        free(rel_path); free(target_dir); free(filename); free(query_buf); free(full_path); free(content_disp);
        unlock_sd();
        return send_error(req, "路径构建失败", HTTP_INTERNAL_ERROR);
    }
    free(rel_path);
    
    SDCARD_WEB_LOGI(TAG, "UPLOAD: full_path=%s", full_path);

    FILE *f = fopen(full_path, "wb");
    if (f == NULL) {
        SDCARD_WEB_LOGE(TAG, "UPLOAD: fopen failed for %s", full_path);
        free(target_dir); free(filename); free(query_buf); free(full_path); free(content_disp);
        unlock_sd();
        return send_error(req, "无法创建文件", HTTP_INTERNAL_ERROR);
    }

    // 64KB缓冲区，减少系统调用次数（10MB文件只需~160次循环 vs 8KB的1280次）
    #define UPLOAD_CHUNK_SIZE 65536
    char *buf = malloc(UPLOAD_CHUNK_SIZE);
    if (buf == NULL) {
        fclose(f);
        free(target_dir); free(filename); free(query_buf); free(full_path); free(content_disp);
        unlock_sd();
        return send_error(req, "内存分配失败", HTTP_INTERNAL_ERROR);
    }

    size_t received = 0;
    int remaining = req->content_len;

    while (remaining > 0) {
        int to_read = remaining < UPLOAD_CHUNK_SIZE ? remaining : UPLOAD_CHUNK_SIZE;
        int ret = httpd_req_recv(req, buf, to_read);
        if (ret <= 0) break;

        fwrite(buf, 1, ret, f);
        received += ret;
        remaining -= ret;
    }

    free(buf);
    fclose(f);

    // 验证文件确实创建成功
    uint32_t verify_size = 0;
    bool verify_is_dir = false;
    esp_err_t verify_ret = sdcard_get_file_size(full_path, &verify_size, &verify_is_dir);
    SDCARD_WEB_LOGI(TAG, "UPLOAD: 验证文件 - path=%s, size=%lu, verify=%s", 
                    full_path, (unsigned long)verify_size, 
                    verify_ret == ESP_OK ? "OK" : "FAILED");

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "name", filename);
    cJSON_AddNumberToObject(data, "size", (double)received);

    free(target_dir); free(filename); free(query_buf); free(full_path); free(content_disp);
    unlock_sd();
    return send_success(req, data, "上传成功");
}

// ============================================================================
// API: 下载/预览文件
// ============================================================================
esp_err_t sdcard_web_download_handler(httpd_req_t *req)
{
    if (req == NULL) return ESP_FAIL;

    lock_sd();

    char *file_path = malloc(2048);
    char *query_buf = malloc(1024);
    char *full_path = malloc(4096);
    if (!file_path || !query_buf || !full_path) {
        free(file_path); free(query_buf); free(full_path);
        unlock_sd();
        return send_error(req, "内存分配失败", HTTP_INTERNAL_ERROR);
    }
    memset(file_path, 0, 2048);

    if (httpd_req_get_url_query_str(req, query_buf, 1024) != ESP_OK ||
        httpd_query_key_value(query_buf, "path", file_path, 2047) != ESP_OK) {
        free(file_path); free(query_buf); free(full_path);
        unlock_sd();
        return send_bad_request(req, "缺少 path 参数");
    }

    // URL解码路径
    url_decode_inplace(file_path, 2048);

    SDCARD_WEB_LOGI(TAG, "DOWNLOAD: decoded path=%s", file_path);

    // 路径安全检查
    if (!path_is_safe(file_path)) {
        SDCARD_WEB_LOGE(TAG, "DOWNLOAD: 非法路径: %s", file_path);
        free(file_path); free(query_buf); free(full_path);
        unlock_sd();
        return send_bad_request(req, "非法路径");
    }

    const char *mount = sdcard_get_mount_point();
    size_t mount_len = strlen(mount);
    
    // 检查file_path是否已经包含挂载点前缀
    if (strncmp(file_path, mount, mount_len) == 0) {
        // 已经是完整路径，直接使用
        snprintf(full_path, 4096, "%s", file_path);
    } else if (file_path[0] == '/') {
        // 以/开头但不是挂载点开头，保持原样
        snprintf(full_path, 4096, "%s", file_path);
    } else {
        // 相对路径，使用统一的路径构建
        if (!path_build_vfs(mount, file_path, full_path, 4096)) {
            free(file_path); free(query_buf); free(full_path);
            unlock_sd();
            return send_error(req, "路径构建失败", HTTP_INTERNAL_ERROR);
        }
    }

    SDCARD_WEB_LOGI(TAG, "DOWNLOAD: full_path=%s", full_path);

    FILE *f = fopen(full_path, "rb");
    if (f == NULL) {
        SDCARD_WEB_LOGE(TAG, "DOWNLOAD: 文件不存在: %s", full_path);
        free(file_path); free(query_buf); free(full_path);
        unlock_sd();
        return send_error(req, "文件不存在", HTTP_NOT_FOUND);
    }

    // 获取文件大小，设置Content-Length（避免chunked传输导致的问题）
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size < 0) {
        SDCARD_WEB_LOGE(TAG, "DOWNLOAD: 获取文件大小失败: %s", full_path);
        fclose(f);
        free(file_path); free(query_buf); free(full_path);
        unlock_sd();
        return send_error(req, "获取文件大小失败", HTTP_INTERNAL_ERROR);
    }

    SDCARD_WEB_LOGI(TAG, "DOWNLOAD: file=%s, size=%ld", full_path, file_size);

    const char *fname = strrchr(file_path, '/');
    fname = fname ? fname + 1 : file_path;

    // 使用统一的MIME类型工具（遵循项目规范：禁止复制粘贴）
    const char *mime = mime_get_type(fname);

    httpd_resp_set_type(req, mime);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // 注意：不要设置Content-Length，因为使用httpd_resp_send_chunk是chunked传输
    // 同时设置Content-Length和chunked会导致无效HTTP响应

    char *chunk = malloc(32768);
    if (chunk == NULL) {
        fclose(f);
        free(file_path); free(query_buf); free(full_path);
        unlock_sd();
        return send_error(req, "内存分配失败", HTTP_INTERNAL_ERROR);
    }

    size_t bytes;
    size_t total_sent = 0;
    while ((bytes = fread(chunk, 1, 32768, f)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, bytes) != ESP_OK) {
            SDCARD_WEB_LOGE(TAG, "DOWNLOAD: 发送失败, sent=%u/%ld", (unsigned)total_sent, file_size);
            break;
        }
        total_sent += bytes;
    }

    SDCARD_WEB_LOGI(TAG, "DOWNLOAD: 完成, sent=%u/%ld", (unsigned)total_sent, file_size);

    free(chunk);
    fclose(f);
    free(file_path); free(query_buf); free(full_path);
    unlock_sd();
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

// ============================================================================
// API: 调试 - 打印文件列表
// ============================================================================
esp_err_t sdcard_web_debug_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    lock_sd();

    if (!sdcard_is_mounted()) {
        unlock_sd();
        return send_error(req, "TF卡未挂载", HTTP_SERVICE_UNAVAILABLE);
    }

    const char *mount = sdcard_get_mount_point();
    DIR *dir = opendir(mount);
    if (dir == NULL) {
        unlock_sd();
        return send_error(req, "无法打开目录", HTTP_NOT_FOUND);
    }

    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        SDCARD_WEB_LOGI(TAG, "%s%s",
            entry->d_name,
            entry->d_type == DT_DIR ? "/" : "");
        count++;
    }

    closedir(dir);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "count", count);

    unlock_sd();
    return send_success(req, data, "调试信息已打印");
}

// ============================================================================
// API: 计算目录大小
// ============================================================================
esp_err_t sdcard_web_dirsize_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    lock_sd();

    if (!sdcard_is_mounted()) {
        unlock_sd();
        return send_error(req, "TF卡未挂载", HTTP_SERVICE_UNAVAILABLE);
    }

    char *path_buf = malloc(256);
    char *query_buf = malloc(1024);
    char *full_path = malloc(1024);
    char *file_path_buf = malloc(2048);
    if (!path_buf || !query_buf || !full_path || !file_path_buf) {
        free(path_buf); free(query_buf); free(full_path); free(file_path_buf);
        unlock_sd();
        return send_error(req, "内存分配失败", HTTP_INTERNAL_ERROR);
    }
    memset(path_buf, 0, 256);

    if (httpd_req_get_url_query_str(req, query_buf, 1024) == ESP_OK) {
        httpd_query_key_value(query_buf, "path", path_buf, 255);
    }

    if (path_buf[0] == '\0') {
        free(path_buf); free(query_buf); free(full_path); free(file_path_buf);
        unlock_sd();
        return send_bad_request(req, "缺少 path 参数");
    }

    // 路径安全检查
    if (!path_is_safe(path_buf)) {
        free(path_buf); free(query_buf); free(full_path); free(file_path_buf);
        unlock_sd();
        return send_bad_request(req, "非法路径");
    }

    // 使用统一的路径构建
    const char *mount = sdcard_get_mount_point();
    if (!path_build_vfs(mount, path_buf, full_path, 1024)) {
        free(path_buf); free(query_buf); free(full_path); free(file_path_buf);
        unlock_sd();
        return send_error(req, "路径构建失败", HTTP_INTERNAL_ERROR);
    }

    uint64_t total_size = 0;
    DIR *dir = opendir(full_path);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;

            snprintf(file_path_buf, 2048, "%s/%s", full_path, entry->d_name);

            uint32_t file_size = 0;
            bool is_dir_file = false;
            if (sdcard_get_file_size(file_path_buf, &file_size, &is_dir_file) == ESP_OK) {
                // 只计算文件大小，不包括目录
                if (!is_dir_file) {
                    total_size += file_size;
                }
            }
        }
        closedir(dir);
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "size", (double)total_size);
    cJSON_AddStringToObject(data, "path", path_buf);

    free(path_buf); free(query_buf); free(full_path); free(file_path_buf);
    unlock_sd();
    return send_success(req, data, "计算目录大小成功");
}
