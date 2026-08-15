/**
 * @file video_web.c
 * @brief 视频监控 Web API 实现（带心跳与预热）
 */

#include "video_web.h"
#include "../config.h"
#include "../web_module.h"
#include "cJSON.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "video.h"
#include "video_ws.h"
#include <stdlib.h>
#include <string.h>

#define TAG "VIDEO_WEB"

#define BOUNDARY "esp32s3_frame"
#define FRAME_INTERVAL_US 80000       // 正常帧间隔 80ms (约12.5fps)
#define HEARTBEAT_INTERVAL_US 3000000 // 3秒无帧则发送心跳

// ---------- 辅助函数 ----------

static esp_err_t stream_send_header(httpd_req_t *req) {
    char hdr[512];
    snprintf(hdr, sizeof(hdr),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: multipart/x-mixed-replace; boundary=%s\r\n"
             "Cache-Control: no-cache, no-store, must-revalidate\r\n"
             "Pragma: no-cache\r\n"
             "Expires: 0\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "\r\n",
             BOUNDARY);
    return httpd_resp_sendstr(req, hdr);
}

static esp_err_t stream_send_frame(httpd_req_t *req, const uint8_t *jpg, size_t jpg_len) {
    char part_hdr[128];
    int hdr_len = snprintf(part_hdr, sizeof(part_hdr),
                           "--%s\r\n"
                           "Content-Type: image/jpeg\r\n"
                           "Content-Length: %u\r\n"
                           "\r\n",
                           BOUNDARY, (unsigned)jpg_len);

    esp_err_t ret = httpd_resp_send_chunk(req, part_hdr, hdr_len);
    if (ret != ESP_OK)
        return ret;

    ret = httpd_resp_send_chunk(req, (const char *)jpg, jpg_len);
    if (ret != ESP_OK)
        return ret;

    return httpd_resp_send_chunk(req, "\r\n", 2);
}

static esp_err_t stream_send_heartbeat(httpd_req_t *req) {
    char heartbeat[128];
    int len = snprintf(heartbeat, sizeof(heartbeat),
                       "--%s\r\n"
                       "Content-Type: text/plain\r\n"
                       "\r\n"
                       "keepalive\r\n",
                       BOUNDARY);
    return httpd_resp_send_chunk(req, heartbeat, len);
}

static esp_err_t video_web_options_handler(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// ---------- 路由注册 ----------

void video_web_register_routes(httpd_handle_t server) {
    VIDEO_LOGI(TAG, "开始注册视频模块路由...");

    httpd_uri_t routes[] = {
        {.uri = "/api/video/info", .method = HTTP_GET, .handler = video_web_info_handler},
        {.uri = "/api/video/status", .method = HTTP_GET, .handler = video_web_status_handler},
        {.uri = "/api/video/framesizes",
         .method = HTTP_GET,
         .handler = video_web_framesizes_handler},
        {.uri = "/api/video/snapshot", .method = HTTP_GET, .handler = video_web_snapshot_handler},
        {.uri = "/api/video/stream", .method = HTTP_GET, .handler = video_web_stream_handler},
        {.uri = "/api/video/config", .method = HTTP_POST, .handler = video_web_config_handler},
        {.uri = "/api/video/config", .method = HTTP_OPTIONS, .handler = video_web_options_handler},
        {.uri = "/api/video/stream/start",
         .method = HTTP_POST,
         .handler = video_web_start_stream_handler},
        {.uri = "/api/video/stream/stop",
         .method = HTTP_POST,
         .handler = video_web_stop_stream_handler}};

    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        esp_err_t ret __attribute__((unused)) = httpd_register_uri_handler(server, &routes[i]);
        const char *method __attribute__((unused)) = (routes[i].method == HTTP_GET)    ? "GET"
                             : (routes[i].method == HTTP_POST) ? "POST"
                                                               : "OPTIONS";
        VIDEO_LOGI(TAG, "注册路由[%d]: %s %s -> %s", (int)i, method, routes[i].uri,
                   ret == ESP_OK ? "OK" : "FAIL");
    }

    // 注册 WebSocket 路由
    esp_err_t ws_ret __attribute__((unused)) = video_ws_register(server);
    VIDEO_LOGI(TAG, "WebSocket 路由注册: %s", ws_ret == ESP_OK ? "OK" : "FAIL");

    VIDEO_LOGI(TAG, "视频模块路由注册完成");
}

// ---------- 流处理核心 ----------
esp_err_t video_web_stream_handler(httpd_req_t *req) {
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }
    if (!video_is_initialized()) {
        return send_error(req, "摄像头未初始化", HTTP_SERVICE_UNAVAILABLE);
    }

    video_start_streaming();

    // 1. 发送 HTTP 头
    if (stream_send_header(req) != ESP_OK) {
        video_stop_streaming();
        return ESP_FAIL;
    }

    // 2. 发送初始 boundary
    char init_boundary[64];
    int init_len = snprintf(init_boundary, sizeof(init_boundary), "--%s\r\n", BOUNDARY);
    if (httpd_resp_send_chunk(req, init_boundary, init_len) != ESP_OK) {
        video_stop_streaming();
        return ESP_FAIL;
    }

    // 3. 预热阶段：丢弃初期不稳定帧，直至传感器稳定输出
    uint8_t *jpg_buf = NULL;
    size_t jpg_len = 0;
    int warmup_attempts = 0;
    const int max_warmup = 50; // 最多尝试 50 次（约 1.5 秒）
    bool got_first_frame = false;

    VIDEO_LOGI(TAG, "开始预热摄像头，丢弃初期不稳定的帧...");
    while (warmup_attempts < max_warmup) {
        if (video_get_jpeg(&jpg_buf, &jpg_len) == ESP_OK && jpg_buf && jpg_len > 0) {
            video_release_jpeg();
            got_first_frame = true;
            VIDEO_LOGI(TAG, "预热成功：已获得第一帧，大小=%u 字节", (unsigned)jpg_len);
            break;
        }
        video_release_jpeg();
        vTaskDelay(pdMS_TO_TICKS(30));
        warmup_attempts++;
    }

    if (!got_first_frame) {
        VIDEO_LOGW(TAG, "预热未能获得帧，发送心跳维持连接");
        if (stream_send_heartbeat(req) != ESP_OK) {
            video_stop_streaming();
            return ESP_FAIL;
        }
    }

    // 4. 主循环：发送帧或心跳
    int64_t last_frame_time = esp_timer_get_time();
    esp_err_t res = ESP_OK;

    while (1) {
        int64_t now = esp_timer_get_time();
        int64_t elapsed = now - last_frame_time;

        bool frame_ok = false;
        if (video_get_jpeg(&jpg_buf, &jpg_len) == ESP_OK && jpg_buf && jpg_len > 0) {
            frame_ok = true;
        }

        if (frame_ok) {
            res = stream_send_frame(req, jpg_buf, jpg_len);
            video_release_jpeg();
            if (res != ESP_OK) {
                VIDEO_LOGW(TAG, "发送帧失败，客户端可能已断开");
                break;
            }
            last_frame_time = now;
        } else {
            video_release_jpeg();

            if (elapsed >= HEARTBEAT_INTERVAL_US) {
                res = stream_send_heartbeat(req);
                if (res != ESP_OK)
                    break;
                last_frame_time = now;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    video_stop_streaming();
    return res;
}

// ---------- 其他 handler（与你原版一致，仅保留必要修改） ----------

esp_err_t video_web_info_handler(httpd_req_t *req) {
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    video_info_t info;
    memset(&info, 0, sizeof(info));
    video_get_info_struct(&info);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "initialized", info.initialized);
    cJSON_AddBoolToObject(data, "init_in_progress", video_is_init_in_progress());
    cJSON_AddBoolToObject(data, "streaming", info.streaming);
    cJSON_AddStringToObject(data, "sensor", info.sensor_name[0] ? info.sensor_name : "-");

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
    if (info.initialized)
        msg = "获取摄像头信息成功";
    else if (video_is_init_in_progress())
        msg = "摄像头正在探测中...";

    return send_success(req, data, msg);
}

esp_err_t video_web_status_handler(httpd_req_t *req) {
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
        cJSON_AddNumberToObject(data, "brightness", video_get_parameter(VIDEO_PARAM_BRIGHTNESS));
        cJSON_AddNumberToObject(data, "contrast", video_get_parameter(VIDEO_PARAM_CONTRAST));
        cJSON_AddNumberToObject(data, "saturation", video_get_parameter(VIDEO_PARAM_SATURATION));
        cJSON_AddNumberToObject(data, "gain", video_get_parameter(VIDEO_PARAM_GAIN));
        cJSON_AddNumberToObject(data, "exposure", video_get_parameter(VIDEO_PARAM_EXPOSURE));
        cJSON_AddBoolToObject(data, "hmirror",
                              video_get_parameter(VIDEO_PARAM_HMIRROR) ? true : false);
        cJSON_AddBoolToObject(data, "vflip", video_get_parameter(VIDEO_PARAM_VFLIP) ? true : false);
        cJSON_AddNumberToObject(data, "framesize", video_get_parameter(VIDEO_PARAM_FRAMESIZE));
        cJSON_AddNumberToObject(data, "quality", video_get_parameter(VIDEO_PARAM_QUALITY));
    }

    const char *msg =
        initialized ? "摄像头已就绪" : (in_progress ? "摄像头正在探测中..." : "摄像头未初始化");
    return send_success(req, data, msg);
}

esp_err_t video_web_framesizes_handler(httpd_req_t *req) {
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    uint32_t count = 0;
    const video_framesize_info_t *list = video_get_framesize_list(&count);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "current", video_get_parameter(VIDEO_PARAM_FRAMESIZE));

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

esp_err_t video_web_snapshot_handler(httpd_req_t *req) {
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }
    if (!video_is_initialized()) {
        return send_error(req, "摄像头未初始化", HTTP_SERVICE_UNAVAILABLE);
    }

    bool started = false;
    if (!video_is_streaming()) {
        video_start_streaming();
        started = true;
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    uint8_t *jpg_buf = NULL;
    size_t jpg_len = 0;
    esp_err_t ret = video_get_jpeg(&jpg_buf, &jpg_len);
    if (ret != ESP_OK || jpg_buf == NULL || jpg_len == 0) {
        if (started)
            video_stop_streaming();
        video_release_jpeg();
        return send_error(req, "获取快照失败", HTTP_INTERNAL_ERROR);
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=snapshot.jpg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    ret = httpd_resp_send(req, (const char *)jpg_buf, jpg_len);
    video_release_jpeg();

    if (started)
        video_stop_streaming();
    return ret;
}

esp_err_t video_web_config_handler(httpd_req_t *req) {
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
    cJSON *item;

#define UPDATE_PARAM(field_name, enum_val)                                                         \
    item = cJSON_GetObjectItem(json, #field_name);                                                 \
    if (cJSON_IsNumber(item)) {                                                                    \
        if (video_set_parameter(enum_val, item->valueint) == ESP_OK)                               \
            any_changed = true;                                                                    \
    }

    UPDATE_PARAM(brightness, VIDEO_PARAM_BRIGHTNESS);
    UPDATE_PARAM(contrast, VIDEO_PARAM_CONTRAST);
    UPDATE_PARAM(saturation, VIDEO_PARAM_SATURATION);
    UPDATE_PARAM(gain, VIDEO_PARAM_GAIN);
    UPDATE_PARAM(exposure, VIDEO_PARAM_EXPOSURE);
    UPDATE_PARAM(awb, VIDEO_PARAM_AWB);
    UPDATE_PARAM(wb_mode, VIDEO_PARAM_WB_MODE);
    UPDATE_PARAM(aec, VIDEO_PARAM_AEC);
    UPDATE_PARAM(agc, VIDEO_PARAM_AGC);

    item = cJSON_GetObjectItem(json, "hmirror");
    if (cJSON_IsBool(item)) {
        if (video_set_parameter(VIDEO_PARAM_HMIRROR, cJSON_IsTrue(item) ? 1 : 0) == ESP_OK)
            any_changed = true;
    }
    item = cJSON_GetObjectItem(json, "vflip");
    if (cJSON_IsBool(item)) {
        if (video_set_parameter(VIDEO_PARAM_VFLIP, cJSON_IsTrue(item) ? 1 : 0) == ESP_OK)
            any_changed = true;
    }
    UPDATE_PARAM(quality, VIDEO_PARAM_QUALITY);
    UPDATE_PARAM(framesize, VIDEO_PARAM_FRAMESIZE);

    cJSON_Delete(json);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "brightness", video_get_parameter(VIDEO_PARAM_BRIGHTNESS));
    cJSON_AddNumberToObject(data, "contrast", video_get_parameter(VIDEO_PARAM_CONTRAST));
    cJSON_AddNumberToObject(data, "saturation", video_get_parameter(VIDEO_PARAM_SATURATION));
    cJSON_AddNumberToObject(data, "gain", video_get_parameter(VIDEO_PARAM_GAIN));
    cJSON_AddNumberToObject(data, "exposure", video_get_parameter(VIDEO_PARAM_EXPOSURE));
    cJSON_AddBoolToObject(data, "hmirror", video_get_parameter(VIDEO_PARAM_HMIRROR) ? true : false);
    cJSON_AddBoolToObject(data, "vflip", video_get_parameter(VIDEO_PARAM_VFLIP) ? true : false);
    cJSON_AddNumberToObject(data, "quality", video_get_parameter(VIDEO_PARAM_QUALITY));
    cJSON_AddNumberToObject(data, "framesize", video_get_parameter(VIDEO_PARAM_FRAMESIZE));
    cJSON_AddBoolToObject(data, "awb", video_get_parameter(VIDEO_PARAM_AWB) ? true : false);
    cJSON_AddNumberToObject(data, "wb_mode", video_get_parameter(VIDEO_PARAM_WB_MODE));
    cJSON_AddBoolToObject(data, "aec", video_get_parameter(VIDEO_PARAM_AEC) ? true : false);
    cJSON_AddBoolToObject(data, "agc", video_get_parameter(VIDEO_PARAM_AGC) ? true : false);
    return send_success(req, data, any_changed ? "摄像头参数已更新" : "未提供有效的参数更新");
}

esp_err_t video_web_start_stream_handler(httpd_req_t *req) {
    if (req->method != HTTP_POST) {
        return send_bad_request(req, "仅支持 POST 请求");
    }
    if (!video_is_initialized()) {
        return send_error(req, "摄像头未初始化", HTTP_SERVICE_UNAVAILABLE);
    }
    video_start_streaming();
    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "streaming", true);
    return send_success(req, data, "视频流已启动");
}

esp_err_t video_web_stop_stream_handler(httpd_req_t *req) {
    if (req->method != HTTP_POST) {
        return send_bad_request(req, "仅支持 POST 请求");
    }
    video_stop_streaming();
    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "streaming", false);
    return send_success(req, data, "视频流已停止");
}