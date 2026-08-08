/**
 * @file video_web.c
 * @brief 视频监控 Web API 实现
 *
 * 使用 web_module.h 提供的统一响应格式
 */

#include "video_web.h"
#include "video.h"
#include "../config.h"
#include "../web_module.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const char *TAG = "VIDEO_WEB";
#define LOG_TAG TAG

/* ========================================
 * 常量配置
 * ======================================== */

#define BOUNDARY            "esp32s3_frame"   /* MJPEG 边界标识 */
#define FRAME_INTERVAL_US   100000            /* 10fps (100ms/帧) */
#define STREAM_TIMEOUT_MS   5000              /* 单次发送超时 */

/* ========================================
 * VIDEO_WEB 日志宏 - 复用 VIDEO 日志级别
 * ======================================== */

#if LOG_LEVEL_VIDEO >= LOG_LEVEL_ERROR
    #define VIDEO_WEB_LOGE(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
#else
    #define VIDEO_WEB_LOGE(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_VIDEO >= LOG_LEVEL_WARN
    #define VIDEO_WEB_LOGW(tag, ...) ESP_LOGW(tag, __VA_ARGS__)
#else
    #define VIDEO_WEB_LOGW(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_VIDEO >= LOG_LEVEL_INFO
    #define VIDEO_WEB_LOGI(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
#else
    #define VIDEO_WEB_LOGI(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_VIDEO >= LOG_LEVEL_DEBUG
    #define VIDEO_WEB_LOGD(tag, ...) ESP_LOGD(tag, __VA_ARGS__)
#else
    #define VIDEO_WEB_LOGD(tag, ...) do {} while(0)
#endif

/* ========================================
 * 内部辅助：MJPEG 流发送
 * ======================================== */

/**
 * @brief 发送 MJPEG 流头部
 */
static esp_err_t stream_send_header(httpd_req_t *req)
{
    char *hdr = malloc(512);
    if (!hdr) return ESP_ERR_NO_MEM;

    int len = snprintf(hdr, 512,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=%s\r\n"
        "Cache-Control: no-cache, no-store, must-revalidate\r\n"
        "Pragma: no-cache\r\n"
        "Expires: 0\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n",
        BOUNDARY);

    esp_err_t ret = httpd_resp_sendstr(req, hdr);
    free(hdr);
    return ret;
}

/**
 * @brief 发送单个 JPEG 帧
 */
static esp_err_t stream_send_frame(httpd_req_t *req, const uint8_t *jpg, size_t jpg_len)
{
    char *part_hdr = malloc(256);
    if (!part_hdr) return ESP_ERR_NO_MEM;

    int hdr_len = snprintf(part_hdr, 256,
        "--%s\r\n"
        "Content-Type: image/jpeg\r\n"
        "Content-Length: %u\r\n"
        "\r\n",
        BOUNDARY, (unsigned)jpg_len);

    esp_err_t ret = httpd_resp_send_chunk(req, part_hdr, hdr_len);
    free(part_hdr);
    if (ret != ESP_OK) return ret;

    ret = httpd_resp_send_chunk(req, (const char *)jpg, jpg_len);
    if (ret != ESP_OK) return ret;

    return httpd_resp_send_chunk(req, "\r\n", 2);
}

/**
 * @brief 发送 MJPEG 流结束标记
 */
static void stream_send_end(httpd_req_t *req)
{
    char *end = malloc(64);
    if (end) {
        snprintf(end, 64, "--%s--\r\n", BOUNDARY);
        httpd_resp_send_chunk(req, end, strlen(end));
        free(end);
    }
    httpd_resp_send_chunk(req, NULL, 0);  /* 结束 chunked */
}

/* ========================================
 * API: 获取摄像头详细信息
 * GET /api/video/info
 * ======================================== */

esp_err_t video_web_info_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    video_info_t info;
    memset(&info, 0, sizeof(info));
    esp_err_t ret = video_get_info_struct(&info);
    if (ret != ESP_OK) {
        /* 即使失败也返回基本状态 */
        memset(&info, 0, sizeof(info));
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "initialized", info.initialized);
    cJSON_AddBoolToObject(data, "init_in_progress", video_is_init_in_progress());
    cJSON_AddBoolToObject(data, "streaming", info.streaming);
    cJSON_AddStringToObject(data, "sensor",
        info.sensor_name[0] ? info.sensor_name : "-");

    cJSON *resolution = cJSON_CreateObject();
    cJSON_AddNumberToObject(resolution, "width", info.width);
    cJSON_AddNumberToObject(resolution, "height", info.height);
    cJSON_AddNumberToObject(resolution, "framesize", (int)info.framesize);
    cJSON_AddItemToObject(data, "resolution", resolution);

    cJSON *params = cJSON_CreateObject();
    cJSON_AddNumberToObject(params, "brightness", info.brightness);
    cJSON_AddNumberToObject(params, "contrast", info.contrast);
    cJSON_AddNumberToObject(params, "saturation", info.saturation);
    cJSON_AddNumberToObject(params, "gain", info.gain);
    cJSON_AddNumberToObject(params, "exposure", info.exposure);
    cJSON_AddBoolToObject(params, "hmirror", info.hmirror);
    cJSON_AddBoolToObject(params, "vflip", info.vflip);
    cJSON_AddNumberToObject(params, "quality", info.jpeg_quality);
    cJSON_AddItemToObject(data, "params", params);

    const char *msg = "摄像头未初始化";
    if (info.initialized)      msg = "获取摄像头信息成功";
    else if (video_is_init_in_progress()) msg = "摄像头正在探测中...";

    return send_success(req, data, msg);
}

/* ========================================
 * API: 获取摄像头状态（简化版）
 * GET /api/video/status
 * ======================================== */

esp_err_t video_web_status_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    cJSON *data = cJSON_CreateObject();

    bool initialized = video_is_initialized();
    bool in_progress = video_is_init_in_progress();
    cJSON_AddBoolToObject(data, "initialized", initialized);
    cJSON_AddBoolToObject(data, "init_in_progress", in_progress);
    cJSON_AddBoolToObject(data, "streaming", video_is_streaming());

    if (initialized) {
        cJSON_AddNumberToObject(data, "brightness",
            video_get_parameter(VIDEO_PARAM_BRIGHTNESS));
        cJSON_AddNumberToObject(data, "contrast",
            video_get_parameter(VIDEO_PARAM_CONTRAST));
        cJSON_AddNumberToObject(data, "saturation",
            video_get_parameter(VIDEO_PARAM_SATURATION));
        cJSON_AddNumberToObject(data, "gain",
            video_get_parameter(VIDEO_PARAM_GAIN));
        cJSON_AddNumberToObject(data, "exposure",
            video_get_parameter(VIDEO_PARAM_EXPOSURE));
        cJSON_AddBoolToObject(data, "hmirror",
            video_get_parameter(VIDEO_PARAM_HMIRROR) ? true : false);
        cJSON_AddBoolToObject(data, "vflip",
            video_get_parameter(VIDEO_PARAM_VFLIP) ? true : false);
        cJSON_AddNumberToObject(data, "framesize",
            video_get_parameter(VIDEO_PARAM_FRAMESIZE));
        cJSON_AddNumberToObject(data, "quality",
            video_get_parameter(VIDEO_PARAM_QUALITY));
    }

    const char *msg = "摄像头未初始化";
    if (initialized) msg = "摄像头已就绪";
    else if (in_progress) msg = "摄像头正在探测中...";
    return send_success(req, data, msg);
}

/* ========================================
 * API: 获取支持的分辨率列表
 * GET /api/video/framesizes
 * ======================================== */

esp_err_t video_web_framesizes_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    uint32_t count = 0;
    const video_framesize_info_t *list = video_get_framesize_list(&count);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "current",
        video_get_parameter(VIDEO_PARAM_FRAMESIZE));

    cJSON *sizes = cJSON_CreateArray();
    for (uint32_t i = 0; i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", (int)list[i].framesize);
        cJSON_AddNumberToObject(item, "width", list[i].width);
        cJSON_AddNumberToObject(item, "height", list[i].height);
        cJSON_AddStringToObject(item, "name", list[i].name);
        cJSON_AddItemToArray(sizes, item);
    }
    cJSON_AddItemToObject(data, "list", sizes);

    return send_success(req, data, "获取分辨率列表成功");
}

/* ========================================
 * API: MJPEG 实时视频流
 * GET /api/video/stream
 * ======================================== */

esp_err_t video_web_stream_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    if (!video_is_initialized()) {
        return send_error(req, "摄像头未初始化", HTTP_SERVICE_UNAVAILABLE);
    }

    /* 启动流 */
    video_start_streaming();
    VIDEO_WEB_LOGI(TAG, "MJPEG 视频流已连接");

    /* 发送 MJPEG 头部 */
    if (stream_send_header(req) != ESP_OK) {
        VIDEO_WEB_LOGW(TAG, "发送 MJPEG 头部失败，客户端可能已断开");
        video_stop_streaming();
        return ESP_FAIL;
    }

    uint8_t *jpg_buf = NULL;
    size_t jpg_len = 0;
    int64_t last_frame_time = 0;
    uint32_t frames_sent = 0;
    bool client_connected = true;

    while (client_connected) {
        int64_t now = esp_timer_get_time();
        int64_t elapsed = now - last_frame_time;

        /* 控制帧率 */
        if (elapsed < FRAME_INTERVAL_US) {
            uint32_t sleep_ms = (FRAME_INTERVAL_US - elapsed) / 1000;
            if (sleep_ms > 0) {
                vTaskDelay(pdMS_TO_TICKS(sleep_ms));
            }
            continue;
        }

        /* 获取 JPEG 帧 */
        if (video_get_jpeg(&jpg_buf, &jpg_len) != ESP_OK) {
            VIDEO_WEB_LOGW(TAG, "获取帧失败");
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (jpg_buf != NULL && jpg_len > 0) {
            if (stream_send_frame(req, jpg_buf, jpg_len) != ESP_OK) {
                VIDEO_WEB_LOGW(TAG, "发送帧失败，客户端已断开");
                client_connected = false;
            } else {
                last_frame_time = now;
                frames_sent++;
            }
        }

        video_release_jpeg();
    }

    /* 结束流 */
    stream_send_end(req);
    video_stop_streaming();

    VIDEO_WEB_LOGI(TAG, "MJPEG 视频流结束，共发送 %lu 帧", (unsigned long)frames_sent);
    return ESP_OK;
}

/* ========================================
 * API: 单帧 JPEG 快照
 * GET /api/video/snapshot
 * ======================================== */

esp_err_t video_web_snapshot_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    if (!video_is_initialized()) {
        return send_error(req, "摄像头未初始化", HTTP_SERVICE_UNAVAILABLE);
    }

    /* 确保流已启动（获取第一帧可能需要一点时间） */
    bool started = false;
    if (!video_is_streaming()) {
        video_start_streaming();
        started = true;
        vTaskDelay(pdMS_TO_TICKS(50));  /* 等待摄像头稳定 */
    }

    uint8_t *jpg_buf = NULL;
    size_t jpg_len = 0;

    esp_err_t ret = video_get_jpeg(&jpg_buf, &jpg_len);
    if (ret != ESP_OK || jpg_buf == NULL || jpg_len == 0) {
        if (started) video_stop_streaming();
        video_release_jpeg();
        return send_error(req, "获取快照失败", HTTP_INTERNAL_ERROR);
    }

    /* 设置响应头 */
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=snapshot.jpg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    /* 发送图片 */
    ret = httpd_resp_send(req, (const char *)jpg_buf, jpg_len);

    if (ret != ESP_OK) {
        VIDEO_WEB_LOGW(TAG, "发送快照失败: %s", esp_err_to_name(ret));
    }

    video_release_jpeg();

    if (started) {
        video_stop_streaming();
    }

    return ret;
}

/* ========================================
 * API: 设置摄像头参数（JSON Body）
 * POST /api/video/config
 * ======================================== */

esp_err_t video_web_config_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_bad_request(req, "仅支持 POST 请求");
    }

    if (!video_is_initialized()) {
        return send_error(req, "摄像头未初始化", HTTP_SERVICE_UNAVAILABLE);
    }

    cJSON *json = parse_request_json(req);
    if (!json) {
        return send_bad_request(req, "无效的 JSON 格式");
    }

    bool any_changed = false;
    char msg_buf[256] = {0};

    /* 处理 brightness */
    cJSON *item = cJSON_GetObjectItem(json, "brightness");
    if (cJSON_IsNumber(item)) {
        esp_err_t r = video_set_parameter(VIDEO_PARAM_BRIGHTNESS, item->valueint);
        if (r == ESP_OK) any_changed = true;
    }

    /* 处理 contrast */
    item = cJSON_GetObjectItem(json, "contrast");
    if (cJSON_IsNumber(item)) {
        esp_err_t r = video_set_parameter(VIDEO_PARAM_CONTRAST, item->valueint);
        if (r == ESP_OK) any_changed = true;
    }

    /* 处理 saturation */
    item = cJSON_GetObjectItem(json, "saturation");
    if (cJSON_IsNumber(item)) {
        esp_err_t r = video_set_parameter(VIDEO_PARAM_SATURATION, item->valueint);
        if (r == ESP_OK) any_changed = true;
    }

    /* 处理 gain */
    item = cJSON_GetObjectItem(json, "gain");
    if (cJSON_IsNumber(item)) {
        esp_err_t r = video_set_parameter(VIDEO_PARAM_GAIN, item->valueint);
        if (r == ESP_OK) any_changed = true;
    }

    /* 处理 exposure */
    item = cJSON_GetObjectItem(json, "exposure");
    if (cJSON_IsNumber(item)) {
        esp_err_t r = video_set_parameter(VIDEO_PARAM_EXPOSURE, item->valueint);
        if (r == ESP_OK) any_changed = true;
    }

    /* 处理 hmirror */
    item = cJSON_GetObjectItem(json, "hmirror");
    if (cJSON_IsBool(item)) {
        esp_err_t r = video_set_parameter(VIDEO_PARAM_HMIRROR,
            cJSON_IsTrue(item) ? 1 : 0);
        if (r == ESP_OK) any_changed = true;
    }

    /* 处理 vflip */
    item = cJSON_GetObjectItem(json, "vflip");
    if (cJSON_IsBool(item)) {
        esp_err_t r = video_set_parameter(VIDEO_PARAM_VFLIP,
            cJSON_IsTrue(item) ? 1 : 0);
        if (r == ESP_OK) any_changed = true;
    }

    /* 处理 quality */
    item = cJSON_GetObjectItem(json, "quality");
    if (cJSON_IsNumber(item)) {
        esp_err_t r = video_set_parameter(VIDEO_PARAM_QUALITY, item->valueint);
        if (r == ESP_OK) any_changed = true;
    }

    /* 处理 framesize */
    item = cJSON_GetObjectItem(json, "framesize");
    if (cJSON_IsNumber(item)) {
        esp_err_t r = video_set_parameter(VIDEO_PARAM_FRAMESIZE, item->valueint);
        if (r == ESP_OK) any_changed = true;
    }

    cJSON_Delete(json);

    /* 构造响应 - 返回当前参数 */
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "brightness",
        video_get_parameter(VIDEO_PARAM_BRIGHTNESS));
    cJSON_AddNumberToObject(data, "contrast",
        video_get_parameter(VIDEO_PARAM_CONTRAST));
    cJSON_AddNumberToObject(data, "saturation",
        video_get_parameter(VIDEO_PARAM_SATURATION));
    cJSON_AddNumberToObject(data, "gain",
        video_get_parameter(VIDEO_PARAM_GAIN));
    cJSON_AddNumberToObject(data, "exposure",
        video_get_parameter(VIDEO_PARAM_EXPOSURE));
    cJSON_AddBoolToObject(data, "hmirror",
        video_get_parameter(VIDEO_PARAM_HMIRROR) ? true : false);
    cJSON_AddBoolToObject(data, "vflip",
        video_get_parameter(VIDEO_PARAM_VFLIP) ? true : false);
    cJSON_AddNumberToObject(data, "quality",
        video_get_parameter(VIDEO_PARAM_QUALITY));
    cJSON_AddNumberToObject(data, "framesize",
        video_get_parameter(VIDEO_PARAM_FRAMESIZE));

    snprintf(msg_buf, sizeof(msg_buf), any_changed
        ? "摄像头参数已更新"
        : "未提供有效的参数更新");

    return send_success(req, data, msg_buf);
}

/* ========================================
 * API: 启动视频流
 * POST /api/video/stream/start
 * ======================================== */

esp_err_t video_web_start_stream_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_bad_request(req, "仅支持 POST 请求");
    }

    if (!video_is_initialized()) {
        return send_error(req, "摄像头未初始化", HTTP_SERVICE_UNAVAILABLE);
    }

    esp_err_t ret = video_start_streaming();
    if (ret != ESP_OK) {
        return send_error(req, "启动视频流失败", HTTP_INTERNAL_ERROR);
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "streaming", true);

    return send_success(req, data, "视频流已启动");
}

/* ========================================
 * API: 停止视频流
 * POST /api/video/stream/stop
 * ======================================== */

esp_err_t video_web_stop_stream_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_bad_request(req, "仅支持 POST 请求");
    }

    esp_err_t ret = video_stop_streaming();
    if (ret != ESP_OK) {
        return send_error(req, "停止视频流失败", HTTP_INTERNAL_ERROR);
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "streaming", false);

    return send_success(req, data, "视频流已停止");
}
