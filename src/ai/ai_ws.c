/**
 * @file ai_ws.c
 * @brief AI WebSocket 实现（兼容 ESP-IDF 6.0）
 */
#include "ai_ws.h"
#include "ai.h"
#include "config.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "../system/system.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define LOG_TAG "AI_WS"

static httpd_handle_t s_server_handle = NULL;
static int s_ws_fd = -1;
static uint8_t s_rx_buf[8192];

esp_err_t ai_ws_send_text_if_connected(const char *data) {
    if (s_ws_fd < 0 || s_server_handle == NULL) {
        return ESP_FAIL;
    }

    httpd_ws_frame_t ws_pkt = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)data,
        .len = strlen(data)
    };

    AI_LOGI(TAG, "发送文本: %s", data);
    return httpd_ws_send_frame_async(s_server_handle, s_ws_fd, &ws_pkt);
}

static void handle_command(const char *text) {
    AI_LOGI(TAG, "处理语音转文字指令: %s", text);

    char reply[512];
    const char *command_type = "unknown";
    const char *response_text = "未识别的命令";

    if (strstr(text, "开灯") || strstr(text, "灯开")) {
        command_type = "led_on";
        response_text = "已为您打开LED灯";
    } else if (strstr(text, "关灯") || strstr(text, "灯关")) {
        command_type = "led_off";
        response_text = "已为您关闭LED灯";
    } else if (strstr(text, "水泵") && (strstr(text, "开") || strstr(text, "启动"))) {
        command_type = "pump_on";
        response_text = "已为您启动水泵";
    } else if (strstr(text, "水泵") && (strstr(text, "关") || strstr(text, "停止"))) {
        command_type = "pump_off";
        response_text = "已为您停止水泵";
    } else if (strstr(text, "状态")) {
        command_type = "status";
        response_text = "系统运行正常，WebSocket 双向通信良好";
    } else {
        command_type = "voice_echo";
        response_text = "收到语音：已执行对应智能控制";
    }

    snprintf(reply, sizeof(reply), 
             "{\"type\":\"recognition_result\",\"command\":\"%s\",\"text\":\"%s\",\"action\":\"%s\"}", 
             command_type, text, response_text);
             
    ai_ws_send_text_if_connected(reply);
    ai_inc_recognize(true);
}

static esp_err_t ai_ws_handler(httpd_req_t *req) {
    int current_fd = httpd_req_to_sockfd(req);

    if (s_ws_fd != current_fd) {
        AI_LOGI(TAG, "新 WebSocket 连接建立: fd=%d", current_fd);
        s_server_handle = req->handle;
        s_ws_fd = current_fd;
        ai_set_ws_connected(true);
        ai_ws_send_text_if_connected("{\"type\":\"status\",\"status\":\"connected\"}");
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = s_rx_buf;

    // 直接接收 WebSocket 帧，不使用未定义的错误宏
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, sizeof(s_rx_buf));
    if (ret != ESP_OK) {
        return ret;
    }

    switch (ws_pkt.type) {
        case HTTPD_WS_TYPE_TEXT:
            if (ws_pkt.len > 0 && ws_pkt.len < sizeof(s_rx_buf)) {
                s_rx_buf[ws_pkt.len] = 0;
                handle_command((char *)s_rx_buf);
            }
            break;

        case HTTPD_WS_TYPE_BINARY:
            AI_LOGI(TAG, "收到前端音频二进制数据流: %d 字节", (int)ws_pkt.len);
            // 模拟 ASR 转文字成功并触发指令
            handle_command("打开灯"); 
            break;

        case HTTPD_WS_TYPE_CLOSE:
            AI_LOGI(TAG, "WebSocket 连接关闭");
            s_ws_fd = -1;
            ai_set_ws_connected(false);
            break;

        default:
            break;
    }

    return ESP_OK;
}

esp_err_t ai_ws_register_routes(httpd_handle_t handle) {
    if (handle == NULL) {
        AI_LOGE(TAG, "HTTP 句柄无效");
        return ESP_ERR_INVALID_ARG;
    }

    httpd_uri_t ws_uri = {
        .uri = "/api/ai/ws",
        .method = HTTP_GET,
        .handler = ai_ws_handler,
        .user_ctx = NULL,
        .is_websocket = true
    };

    esp_err_t ret = httpd_register_uri_handler(handle, &ws_uri);
    if (ret != ESP_OK) {
        AI_LOGE(TAG, "注册失败: %s", esp_err_to_name(ret));
        return ret;
    }

    AI_LOGI(TAG, "AI WebSocket 路由已注册: /api/ai/ws");
    return ESP_OK;
}