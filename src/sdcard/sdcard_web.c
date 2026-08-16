/**
 * @file sdcard_web.c
 * @brief TF卡Web文件管理HTTP处理模块
 */

#include "sdcard_web.h"
#include "../config.h"
#include "../utils/mime_utils.h"
#include "../utils/path_utils.h"
#include "../web/web_module.h"
#include "esp_http_server.h"
#include "esp_heap_caps.h" // 新增：用于支持 PSRAM 内存分配
#include "ff.h"
#include "sdcard.h"
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h> // rmdir and unlink declarations

#define LOG_TAG "SDCARD_WEB"

static esp_err_t sdcard_web_options_handler(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}
// ============================================================================
// 互斥锁管理
// ============================================================================

static SemaphoreHandle_t s_mutex = NULL;
static StaticSemaphore_t s_mutex_buffer;

static void init_mutex(void) {
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buffer);
    }
}

static void lock_sd(void) {
    init_mutex();
    if (s_mutex != NULL) {
        xSemaphoreTake(s_mutex, pdMS_TO_TICKS(5000));
    }
}

static void unlock_sd(void) {
    if (s_mutex != NULL) {
        xSemaphoreGive(s_mutex);
    }
}

void sdcard_web_register_routes(httpd_handle_t server) {
    httpd_uri_t routes[] = {
        // GET 精确路由（不带 * 号）
        {.uri = "/fs/files", .method = HTTP_GET, .handler = sdcard_web_download_handler},
        {.uri = "/fs/dirsize", .method = HTTP_GET, .handler = sdcard_web_dirsize_handler},
        {.uri = "/api/sdcard/files", .method = HTTP_GET, .handler = sdcard_web_files_handler},
        {.uri = "/api/sdcard/info", .method = HTTP_GET, .handler = sdcard_web_info_handler},
        {.uri = "/api/sdcard/dirs", .method = HTTP_GET, .handler = sdcard_web_dirs_handler},
        {.uri = "/api/sdcard/debug", .method = HTTP_GET, .handler = sdcard_web_debug_handler},

        // POST 路由及其 OPTIONS 预检响应
        {.uri = "/api/sdcard/upload", .method = HTTP_POST, .handler = sdcard_web_upload_handler},
        {.uri = "/api/sdcard/upload", .method = HTTP_OPTIONS, .handler = sdcard_web_options_handler},

        {.uri = "/api/sdcard/mkdir", .method = HTTP_POST, .handler = sdcard_web_mkdir_handler},
        {.uri = "/api/sdcard/mkdir", .method = HTTP_OPTIONS, .handler = sdcard_web_options_handler},

        {.uri = "/api/sdcard/delete", .method = HTTP_POST, .handler = sdcard_web_delete_handler},
        {.uri = "/api/sdcard/delete", .method = HTTP_OPTIONS, .handler = sdcard_web_options_handler},
    };

    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(server, &routes[i]);
    }
}
// ============================================================================
// 辅助函数
// ============================================================================

static void url_decode_inplace(char *str, size_t max_len) {
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

static esp_err_t remove_recursive(const char *path) {
    if (!path) return ESP_ERR_INVALID_ARG;

    struct stat st;
    if (stat(path, &st) != 0) return ESP_FAIL;

    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        if (!dir) return ESP_FAIL;

        char *child_path = malloc(2048);
        if (!child_path) {
            closedir(dir);
            return ESP_ERR_NO_MEM;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            snprintf(child_path, 2048, "%s/%s", path, entry->d_name);
            remove_recursive(child_path);
        }

        free(child_path);
        closedir(dir);

        if (rmdir(path) != 0) return ESP_FAIL;
    } else {
        if (unlink(path) != 0) return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t ensure_dirs_exist(const char *base_path, const char *relative_path) {
    if (relative_path == NULL || relative_path[0] == '\0') return ESP_OK;

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
        if (slash != NULL) *slash = '\0';

        if (token[0] != '\0') {
            snprintf(full_path, 2048, "%s/%s", base_path, token);
            mkdir(full_path, 0755);
        }
        token = slash ? slash + 1 : NULL;
    }

    free(path_copy);
    free(full_path);
    return ESP_OK;
}

// ============================================================================
// API 实现
// ============================================================================

esp_err_t sdcard_web_info_handler(httpd_req_t *req) {
    if (req->method != HTTP_GET) return send_bad_request(req, "仅支持 GET 请求");

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

/* 核心优化1：彻底重写，使用 FatFS 原生 API，避免巨量 f_stat 调用 */
esp_err_t sdcard_web_files_handler(httpd_req_t *req) {
    if (req->method != HTTP_GET) return send_bad_request(req, "仅支持 GET 请求");

    lock_sd();
    if (!sdcard_is_mounted()) {
        unlock_sd();
        return send_error(req, "TF卡未挂载", HTTP_SERVICE_UNAVAILABLE);
    }

    char *path_buf = malloc(256);
    char *query_buf = malloc(1024);
    if (!path_buf || !query_buf) {
        free(path_buf); free(query_buf); unlock_sd();
        return send_error(req, "内存分配失败", HTTP_INTERNAL_ERROR);
    }
    memset(path_buf, 0, 256);

    if (httpd_req_get_url_query_str(req, query_buf, 1024) == ESP_OK) {
        httpd_query_key_value(query_buf, "path", path_buf, 255);
    }
    url_decode_inplace(path_buf, 256);

    if (path_buf[0] != '\0' && !path_is_safe(path_buf)) {
        free(path_buf); free(query_buf); unlock_sd();
        return send_bad_request(req, "非法路径");
    }

    // 构建 FatFS 本地路径
    char fatfs_path[512];
    const char *safe_path = path_buf;
    if (safe_path[0] == '/') safe_path++; // 去除可能的前导斜杠
    if (safe_path[0] != '\0') snprintf(fatfs_path, sizeof(fatfs_path), "0:/%s", safe_path);
    else snprintf(fatfs_path, sizeof(fatfs_path), "0:/");

    FF_DIR dir;
    FRESULT res = f_opendir(&dir, fatfs_path);
    if (res != FR_OK) {
        free(path_buf); free(query_buf); unlock_sd();
        return send_error(req, "打开目录失败", HTTP_NOT_FOUND);
    }

    cJSON *files = cJSON_CreateArray();
    FILINFO fno;
    int count = 0;

    // 单次遍历带回全部信息(包含文件名，大小和属性)，速度提升 10 倍以上
    while (f_readdir(&dir, &fno) == FR_OK && count < 100) {
        if (fno.fname[0] == 0) break;
        if (fno.fname[0] == '.') continue;

        cJSON *file = cJSON_CreateObject();
        cJSON_AddStringToObject(file, "name", fno.fname);
        
        bool is_dir = (fno.fattrib & AM_DIR) != 0;
        cJSON_AddBoolToObject(file, "is_dir", is_dir);

        char rel_path[512];
        if (path_buf[0] != '\0') snprintf(rel_path, sizeof(rel_path), "%s/%s", path_buf, fno.fname);
        else snprintf(rel_path, sizeof(rel_path), "%s", fno.fname);
        
        cJSON_AddStringToObject(file, "url", rel_path);
        
        // 直接读取 FatFS 返回的文件大小信息，不要再用 f_stat 查寻
        cJSON_AddNumberToObject(file, "size", (double)fno.fsize);
        cJSON_AddItemToArray(files, file);
        count++;
    }
    f_closedir(&dir);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "mounted", true);
    cJSON_AddStringToObject(data, "path", path_buf);
    cJSON_AddItemToObject(data, "files", files);
    cJSON_AddNumberToObject(data, "count", count);

    free(path_buf); free(query_buf); unlock_sd();
    return send_success(req, data, "获取文件列表成功");
}

/* 核心优化2：与优化1逻辑相同，优化仅包含目录获取的 API */
esp_err_t sdcard_web_dirs_handler(httpd_req_t *req) {
    if (req->method != HTTP_GET) return send_bad_request(req, "仅支持 GET 请求");

    lock_sd();
    if (!sdcard_is_mounted()) {
        unlock_sd(); return send_error(req, "TF卡未挂载", HTTP_SERVICE_UNAVAILABLE);
    }

    char *path_buf = malloc(256);
    char *query_buf = malloc(1024);
    if (!path_buf || !query_buf) {
        free(path_buf); free(query_buf); unlock_sd();
        return send_error(req, "内存分配失败", HTTP_INTERNAL_ERROR);
    }
    memset(path_buf, 0, 256);

    if (httpd_req_get_url_query_str(req, query_buf, 1024) == ESP_OK) {
        httpd_query_key_value(query_buf, "path", path_buf, 255);
    }
    url_decode_inplace(path_buf, 256);

    if (path_buf[0] != '\0' && !path_is_safe(path_buf)) {
        free(path_buf); free(query_buf); unlock_sd();
        return send_bad_request(req, "非法路径");
    }

    char fatfs_path[512];
    const char *safe_path = path_buf;
    if (safe_path[0] == '/') safe_path++;
    if (safe_path[0] != '\0') snprintf(fatfs_path, sizeof(fatfs_path), "0:/%s", safe_path);
    else snprintf(fatfs_path, sizeof(fatfs_path), "0:/");

    FF_DIR dir;
    FRESULT res = f_opendir(&dir, fatfs_path);
    if (res != FR_OK) {
        free(path_buf); free(query_buf); unlock_sd();
        return send_error(req, "打开目录失败", HTTP_NOT_FOUND);
    }

    cJSON *dirs = cJSON_CreateArray();
    FILINFO fno;

    while (f_readdir(&dir, &fno) == FR_OK) {
        if (fno.fname[0] == 0) break;
        if (fno.fname[0] == '.') continue;
        if (!(fno.fattrib & AM_DIR)) continue;

        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", fno.fname);
        cJSON_AddItemToArray(dirs, item);
    }
    f_closedir(&dir);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "mounted", true);
    cJSON_AddStringToObject(data, "path", path_buf);
    cJSON_AddItemToObject(data, "dirs", dirs);

    free(path_buf); free(query_buf); unlock_sd();
    return send_success(req, data, "获取目录列表成功");
}

esp_err_t sdcard_web_mkdir_handler(httpd_req_t *req) {
    if (req->method != HTTP_POST) return send_bad_request(req, "仅支持 POST 请求");

    lock_sd();
    if (!sdcard_is_mounted()) {
        unlock_sd(); return send_error(req, "TF卡未挂载", HTTP_SERVICE_UNAVAILABLE);
    }

    cJSON *json = parse_request_json(req);
    if (!json) {
        unlock_sd(); return send_bad_request(req, "无效的 JSON 格式");
    }

    cJSON *path_item = cJSON_GetObjectItem(json, "path");
    if (!cJSON_IsString(path_item) || path_item->valuestring[0] == '\0') {
        cJSON_Delete(json); unlock_sd(); return send_bad_request(req, "缺少 path 参数");
    }

    if (!path_is_safe(path_item->valuestring)) {
        cJSON_Delete(json); unlock_sd(); return send_bad_request(req, "非法路径");
    }

    char *full_path = malloc(1024);
    if (!full_path) {
        cJSON_Delete(json); unlock_sd(); return send_error(req, "内存分配失败", HTTP_INTERNAL_ERROR);
    }

    const char *mount = sdcard_get_mount_point();
    if (!path_build_vfs(mount, path_item->valuestring, full_path, 1024)) {
        free(full_path); cJSON_Delete(json); unlock_sd(); return send_error(req, "路径构建失败", HTTP_INTERNAL_ERROR);
    }
    cJSON_Delete(json);

    if (mkdir(full_path, 0755) != 0) {
        free(full_path); unlock_sd(); return send_error(req, "创建目录失败", HTTP_INTERNAL_ERROR);
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "path", path_item->valuestring);

    free(full_path); unlock_sd();
    return send_success(req, data, "目录创建成功");
}

esp_err_t sdcard_web_delete_handler(httpd_req_t *req) {
    if (req->method != HTTP_POST) return send_bad_request(req, "仅支持 POST 请求");

    lock_sd();
    if (!sdcard_is_mounted()) {
        unlock_sd(); return send_error(req, "TF卡未挂载", HTTP_SERVICE_UNAVAILABLE);
    }

    cJSON *json = parse_request_json(req);
    if (!json) {
        unlock_sd(); return send_bad_request(req, "无效的 JSON 格式");
    }

    cJSON *path_item = cJSON_GetObjectItem(json, "path");
    if (!cJSON_IsString(path_item) || path_item->valuestring[0] == '\0') {
        cJSON_Delete(json); unlock_sd(); return send_bad_request(req, "缺少 path 参数");
    }

    if (!path_is_safe(path_item->valuestring)) {
        cJSON_Delete(json); unlock_sd(); return send_bad_request(req, "非法路径");
    }

    char *full_path = malloc(1024);
    if (!full_path) {
        cJSON_Delete(json); unlock_sd(); return send_error(req, "内存分配失败", HTTP_INTERNAL_ERROR);
    }

    const char *mount = sdcard_get_mount_point();
    if (!path_build_vfs(mount, path_item->valuestring, full_path, 1024)) {
        free(full_path); cJSON_Delete(json); unlock_sd(); return send_error(req, "路径构建失败", HTTP_INTERNAL_ERROR);
    }
    cJSON_Delete(json);

    esp_err_t del_ret = remove_recursive(full_path);
    if (del_ret != ESP_OK) {
        free(full_path); unlock_sd(); return send_error(req, "删除失败", HTTP_INTERNAL_ERROR);
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "path", path_item->valuestring);

    free(full_path); unlock_sd();
    return send_success(req, data, "删除成功");
}

esp_err_t sdcard_web_upload_handler(httpd_req_t *req) {
    if (req->method != HTTP_POST) return send_bad_request(req, "仅支持 POST 请求");

    lock_sd();
    if (!sdcard_is_mounted()) {
        unlock_sd(); return send_error(req, "TF卡未挂载", HTTP_SERVICE_UNAVAILABLE);
    }

    char *target_dir = malloc(256);
    char *filename = malloc(256);
    char *query_buf = malloc(1024);
    char *full_path = malloc(1024);
    char *content_disp = malloc(1024);
    if (!target_dir || !filename || !query_buf || !full_path || !content_disp) {
        free(target_dir); free(filename); free(query_buf); free(full_path); free(content_disp);
        unlock_sd(); return send_error(req, "内存分配失败", HTTP_INTERNAL_ERROR);
    }
    memset(target_dir, 0, 256);
    memset(filename, 0, 256);

    if (httpd_req_get_url_query_str(req, query_buf, 1024) == ESP_OK) {
        httpd_query_key_value(query_buf, "path", target_dir, 255);
        httpd_query_key_value(query_buf, "filename", filename, 255);
    }

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
        unlock_sd(); return send_bad_request(req, "未指定文件名");
    }

    if (target_dir[0] != '\0' && !path_is_safe(target_dir)) {
        free(target_dir); free(filename); free(query_buf); free(full_path); free(content_disp);
        unlock_sd(); return send_bad_request(req, "非法路径");
    }

    const char *mount = sdcard_get_mount_point();
    if (target_dir[0] != '\0') ensure_dirs_exist(mount, target_dir);

    char *rel_path = malloc(512);
    if (!rel_path) {
        free(target_dir); free(filename); free(query_buf); free(full_path); free(content_disp);
        unlock_sd(); return send_error(req, "内存分配失败", HTTP_INTERNAL_ERROR);
    }

    if (target_dir[0] != '\0') snprintf(rel_path, 512, "%s/%s", target_dir, filename);
    else snprintf(rel_path, 512, "%s", filename);

    if (!path_build_vfs(mount, rel_path, full_path, 1024)) {
        free(rel_path); free(target_dir); free(filename); free(query_buf); free(full_path); free(content_disp);
        unlock_sd(); return send_error(req, "路径构建失败", HTTP_INTERNAL_ERROR);
    }
    free(rel_path);

    FILE *f = fopen(full_path, "wb");
    if (f == NULL) {
        free(target_dir); free(filename); free(query_buf); free(full_path); free(content_disp);
        unlock_sd(); return send_error(req, "无法创建文件", HTTP_INTERNAL_ERROR);
    }

#define UPLOAD_CHUNK_SIZE 65536
    char *buf = malloc(UPLOAD_CHUNK_SIZE);
    if (buf == NULL) {
        fclose(f);
        free(target_dir); free(filename); free(query_buf); free(full_path); free(content_disp);
        unlock_sd(); return send_error(req, "内存分配失败", HTTP_INTERNAL_ERROR);
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

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "name", filename);
    cJSON_AddNumberToObject(data, "size", (double)received);

    free(target_dir); free(filename); free(query_buf); free(full_path); free(content_disp);
    unlock_sd();
    return send_success(req, data, "上传成功");
}

/* 核心优化3：PSRAM 大内存缓存提高网络下发效率 */
esp_err_t sdcard_web_download_handler(httpd_req_t *req) {
    if (req == NULL) return ESP_FAIL;

    lock_sd();

    char *file_path = malloc(2048);
    char *query_buf = malloc(1024);
    char *full_path = malloc(4096);
    if (!file_path || !query_buf || !full_path) {
        free(file_path); free(query_buf); free(full_path); unlock_sd();
        return send_error(req, "内存分配失败", HTTP_INTERNAL_ERROR);
    }
    memset(file_path, 0, 2048);

    if (httpd_req_get_url_query_str(req, query_buf, 1024) != ESP_OK ||
        httpd_query_key_value(query_buf, "path", file_path, 2047) != ESP_OK) {
        free(file_path); free(query_buf); free(full_path); unlock_sd();
        return send_bad_request(req, "缺少 path 参数");
    }

    url_decode_inplace(file_path, 2048);

    if (!path_is_safe(file_path)) {
        free(file_path); free(query_buf); free(full_path); unlock_sd();
        return send_bad_request(req, "非法路径");
    }

    const char *mount = sdcard_get_mount_point();
    size_t mount_len = strlen(mount);

    if (strncmp(file_path, mount, mount_len) == 0) {
        snprintf(full_path, 4096, "%s", file_path);
    } else if (file_path[0] == '/') {
        snprintf(full_path, 4096, "%s", file_path);
    } else {
        if (!path_build_vfs(mount, file_path, full_path, 4096)) {
            free(file_path); free(query_buf); free(full_path); unlock_sd();
            return send_error(req, "路径构建失败", HTTP_INTERNAL_ERROR);
        }
    }

    FILE *f = fopen(full_path, "rb");
    if (f == NULL) {
        free(file_path); free(query_buf); free(full_path); unlock_sd();
        return send_error(req, "文件不存在", HTTP_NOT_FOUND);
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(f);
        free(file_path); free(query_buf); free(full_path); unlock_sd();
        return send_error(req, "获取文件大小失败", HTTP_INTERNAL_ERROR);
    }

    const char *fname = strrchr(file_path, '/');
    fname = fname ? fname + 1 : file_path;
    const char *mime = mime_get_type(fname);

    httpd_resp_set_type(req, mime);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // 文件成功打开后立刻释放互斥锁
    unlock_sd();

    // 优化点：使用 PSRAM 分配 64KB 级的大容量缓冲区加速 Chunk 传输
    size_t alloc_size = 64 * 1024;
    char *chunk = heap_caps_malloc(alloc_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (chunk == NULL) {
        alloc_size = 32768;
        chunk = malloc(alloc_size); // 降级到内部内存
    }

    if (chunk == NULL) {
        fclose(f);
        free(file_path); free(query_buf); free(full_path);
        return send_error(req, "内存分配失败", HTTP_INTERNAL_ERROR);
    }

    size_t bytes;
    while ((bytes = fread(chunk, 1, alloc_size, f)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, bytes) != ESP_OK) {
            break;
        }
    }

    if(heap_caps_check_integrity_all(true)) {
        free(chunk); // PSRAM/DRAM 通用释放
    }
    
    fclose(f);
    free(file_path); free(query_buf); free(full_path);
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

esp_err_t sdcard_web_debug_handler(httpd_req_t *req) {
    if (req->method != HTTP_GET) return send_bad_request(req, "仅支持 GET 请求");

    lock_sd();
    if (!sdcard_is_mounted()) {
        unlock_sd(); return send_error(req, "TF卡未挂载", HTTP_SERVICE_UNAVAILABLE);
    }

    FF_DIR dir;
    if (f_opendir(&dir, "0:/") != FR_OK) {
        unlock_sd(); return send_error(req, "无法打开目录", HTTP_NOT_FOUND);
    }

    FILINFO fno;
    int count = 0;
    while (f_readdir(&dir, &fno) == FR_OK) {
        if (fno.fname[0] == 0) break;
        if (fno.fname[0] == '.') continue;
        count++;
    }
    f_closedir(&dir);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "count", count);

    unlock_sd();
    return send_success(req, data, "调试信息已打印");
}

/* 核心优化4：同样剔除文件大小读取的 f_stat 重复查询 */
esp_err_t sdcard_web_dirsize_handler(httpd_req_t *req) {
    if (req->method != HTTP_GET) return send_bad_request(req, "仅支持 GET 请求");

    lock_sd();
    if (!sdcard_is_mounted()) {
        unlock_sd(); return send_error(req, "TF卡未挂载", HTTP_SERVICE_UNAVAILABLE);
    }

    char *path_buf = malloc(256);
    char *query_buf = malloc(1024);
    if (!path_buf || !query_buf) {
        free(path_buf); free(query_buf); unlock_sd();
        return send_error(req, "内存分配失败", HTTP_INTERNAL_ERROR);
    }
    memset(path_buf, 0, 256);

    if (httpd_req_get_url_query_str(req, query_buf, 1024) == ESP_OK) {
        httpd_query_key_value(query_buf, "path", path_buf, 255);
    }

    if (path_buf[0] == '\0') {
        free(path_buf); free(query_buf); unlock_sd(); return send_bad_request(req, "缺少 path 参数");
    }

    url_decode_inplace(path_buf, 256);

    if (!path_is_safe(path_buf)) {
        free(path_buf); free(query_buf); unlock_sd(); return send_bad_request(req, "非法路径");
    }

    char fatfs_path[512];
    const char *safe_path = path_buf;
    if (safe_path[0] == '/') safe_path++;
    if (safe_path[0] != '\0') snprintf(fatfs_path, sizeof(fatfs_path), "0:/%s", safe_path);
    else snprintf(fatfs_path, sizeof(fatfs_path), "0:/");

    uint64_t total_size = 0;
    FF_DIR dir;
    if (f_opendir(&dir, fatfs_path) == FR_OK) {
        FILINFO fno;
        while (f_readdir(&dir, &fno) == FR_OK) {
            if (fno.fname[0] == 0) break;
            if (fno.fname[0] == '.') continue;
            
            // 如果不是目录，直接累加 fno.fsize 大小，不需要再调用底层耗时查询
            if (!(fno.fattrib & AM_DIR)) {
                total_size += fno.fsize;
            }
        }
        f_closedir(&dir);
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "size", (double)total_size);
    cJSON_AddStringToObject(data, "path", path_buf);

    free(path_buf); free(query_buf); unlock_sd();
    return send_success(req, data, "计算目录大小成功");
}