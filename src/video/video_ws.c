/**
 * @file video_ws.c
 * @brief 视频流 WebSocket 异步处理实现
 */

#include "video_ws.h"
#include "video.h"
#include "../config.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "VIDEO_WS";

#define WS_FRAME_INTERVAL_MS 80

static int s_ws_fd = -1;
static httpd_handle_t s_server_handle = NULL;
static TaskHandle_t s_ws_tx_task_handle = NULL;

// 后台异步推流 Task，避免阻塞 HTTP 服务器主线程
static void ws_async_send_task(void *arg)
{
    ESP_LOGI(TAG, "后台 WebSocket 推流任务已启动");
    video_start_streaming();

    uint8_t *jpg_buf = NULL;
    size_t jpg_len = 0;

    while (s_ws_fd >= 0) {
        if (video_get_jpeg(&jpg_buf, &jpg_len) == ESP_OK && jpg_buf && jpg_len > 0) {
            httpd_ws_frame_t ws_pkt = {
                .type = HTTPD_WS_TYPE_BINARY,
                .payload = jpg_buf,
                .len = jpg_len
            };

            // 使用 ESP-IDF 提供的异步发送接口
            esp_err_t ret = httpd_ws_send_frame_async(s_server_handle, s_ws_fd, &ws_pkt);
            video_release_jpeg();

            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "帧发送失败(%s)，可能客户端已断开", esp_err_to_name(ret));
                break;
            }
        } else {
            video_release_jpeg();
        }

        vTaskDelay(pdMS_TO_TICKS(WS_FRAME_INTERVAL_MS));
    }

    video_stop_streaming();
    s_ws_fd = -1;
    s_ws_tx_task_handle = NULL;
    ESP_LOGI(TAG, "后台 WebSocket 推流任务已安全退出");
    vTaskDelete(NULL);
}

// WebSocket 路由 Handler
static esp_err_t video_ws_handler(httpd_req_t *req)
{
    int current_fd = httpd_req_to_sockfd(req);

    // 尝试接收 WebSocket 数据包
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        return ret;
    }

    s_server_handle = req->handle;
    s_ws_fd = current_fd;

    // 收到消息/心跳激活时，若推流任务未运行则创建新任务
    if (s_ws_tx_task_handle == NULL) {
        xTaskCreatePinnedToCore(ws_async_send_task, "ws_tx_task", 4096, NULL, 5, &s_ws_tx_task_handle, 1);
    }

    return ESP_OK;
}

// 注册 WebSocket 路由
esp_err_t video_ws_register(httpd_handle_t handle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "httpd handle 为 NULL");
        return ESP_ERR_INVALID_ARG;
    }

    s_server_handle = handle;

    httpd_uri_t ws_uri = {
        .uri = "/api/video/ws",
        .method = HTTP_GET,
        .handler = video_ws_handler,
        .user_ctx = NULL,
        .is_websocket = true
    };

    esp_err_t ret = httpd_register_uri_handler(handle, &ws_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册 WebSocket 路由失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "WebSocket 路由已注册: /api/video/ws");
    return ESP_OK;
}