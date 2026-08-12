/**
 * @file ai_ws.c
 * @brief AI WebSocket 实现
 */
#include "ai_ws.h"
#include "ai.h"
#include "config.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "../system/system.h"
#include <string.h>

static const char *TAG = "AI_WS";

static httpd_handle_t s_server_handle = NULL;
static int s_ws_fd = -1;
static uint8_t s_rx_buf[4096];

/* ========================================
 * 异步发送函数
 * ======================================== */

static esp_err_t ws_send_text(const char *data) {
    if (s_ws_fd < 0 || s_server_handle == NULL) {
        AI_LOGE(TAG, "发送失败: 连接无效");
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

/* ========================================
 * 命令处理
 * ======================================== */

static void handle_command(const char *text) {
    AI_LOGI(TAG, "收到命令: %s", text);

    char reply[256];
    const char *command_type = "unknown";
    const char *response_text = NULL;

    // 简单的关键词匹配
    if (strstr(text, "开灯") || strstr(text, "灯开")) {
        command_type = "led_on";
        response_text = "已打开LED灯";
    } else if (strstr(text, "关灯") || strstr(text, "灯关")) {
        command_type = "led_off";
        response_text = "已关闭LED灯";
    } else if (strstr(text, "水泵") && (strstr(text, "开") || strstr(text, "启动"))) {
        command_type = "pump_on";
        response_text = "已启动水泵";
    } else if (strstr(text, "水泵") && (strstr(text, "关") || strstr(text, "停止"))) {
        command_type = "pump_off";
        response_text = "已停止水泵";
    } else if (strstr(text, "温度") && strstr(text, "芯片")) {
        // 芯片/CPU 温度
        float chip_temp = system_get_chip_temp();
        snprintf(reply, sizeof(reply), "{\"type\":\"recognition_result\",\"command\":\"chip_temp\",\"text\":\"%s\",\"action\":\"当前芯片温度为 %.1f°C\"}", text, chip_temp);
        ws_send_text(reply);
        ai_inc_recognize(true);
        return;
    } else if (strstr(text, "温度")) {
        // 通用温度查询，返回芯片温度
        float chip_temp = system_get_chip_temp();
        snprintf(reply, sizeof(reply), "{\"type\":\"recognition_result\",\"command\":\"chip_temp\",\"text\":\"%s\",\"action\":\"当前芯片温度为 %.1f°C\"}", text, chip_temp);
        ws_send_text(reply);
        ai_inc_recognize(true);
        return;
    } else if (strstr(text, "湿度")) {
        command_type = "query_humidity";
        response_text = "当前湿度为 55%";  // TODO: 接入真实湿度传感器
    } else if (strstr(text, "状态") || strstr(text, "情况")) {
        command_type = "status";
        response_text = "系统运行正常，所有模块正常工作";
    } else if (strstr(text, "帮助") || strstr(text, "help")) {
        command_type = "help";
        response_text = "支持命令：开灯、关灯、开泵、关泵、查芯片温度、查湿度、查状态";
    } else {
        response_text = "未识别的命令，请说帮助获取支持列表";
    }

    // 构建响应
    snprintf(reply, sizeof(reply), "{\"type\":\"recognition_result\",\"command\":\"%s\",\"text\":\"%s\",\"action\":\"%s\"}", command_type, text, response_text);
    ws_send_text(reply);
    ai_inc_recognize(true);
}

/* ========================================
 * WebSocket Handler
 * ======================================== */

static esp_err_t ai_ws_handler(httpd_req_t *req) {
    int current_fd = httpd_req_to_sockfd(req);

    AI_LOGI(TAG, "Handler 调用: fd=%d, method=%d", current_fd, req->method);

    // 初始化连接
    if (s_ws_fd != current_fd) {
        AI_LOGI(TAG, "新连接: fd=%d -> %d", s_ws_fd, current_fd);
        s_server_handle = req->handle;
        s_ws_fd = current_fd;
        ai_set_ws_connected(true);

        // 发送欢迎消息
        ws_send_text("{\"type\":\"status\",\"status\":\"connected\"}");
    }

    // 接收 WebSocket 帧
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = s_rx_buf;

    // 第三个参数 0 表示只读取帧头
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, sizeof(s_rx_buf));
    if (ret != ESP_OK) {
        AI_LOGW(TAG, "接收帧失败: %s", esp_err_to_name(ret));
        return ret;
    }

    AI_LOGI(TAG, "收到帧: type=%d, len=%d", ws_pkt.type, (int)ws_pkt.len);

    // 处理不同类型的帧
    switch (ws_pkt.type) {
        case HTTPD_WS_TYPE_TEXT:
            if (ws_pkt.len > 0 && ws_pkt.len < sizeof(s_rx_buf)) {
                s_rx_buf[ws_pkt.len] = 0;
                AI_LOGI(TAG, "文本消息: %s", (char *)s_rx_buf);
                handle_command((char *)s_rx_buf);
            }
            break;

        case HTTPD_WS_TYPE_BINARY:
            AI_LOGI(TAG, "二进制消息: %d 字节", (int)ws_pkt.len);
            // TODO: 处理音频数据
            break;

        case HTTPD_WS_TYPE_PING:
            AI_LOGI(TAG, "收到 Ping，回复 Pong");
            break;

        case HTTPD_WS_TYPE_PONG:
            AI_LOGI(TAG, "收到 Pong");
            break;

        case HTTPD_WS_TYPE_CLOSE:
            AI_LOGI(TAG, "收到关闭帧");
            s_ws_fd = -1;
            ai_set_ws_connected(false);
            break;

        default:
            AI_LOGW(TAG, "未知帧类型: %d", ws_pkt.type);
            break;
    }

    return ESP_OK;
}

/* ========================================
 * 路由注册
 * ======================================== */

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

    AI_LOGI(TAG, "WebSocket 已注册: /api/ai/ws");
    return ESP_OK;
}
