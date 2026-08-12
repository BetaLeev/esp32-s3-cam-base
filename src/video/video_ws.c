/**
 * @file video_ws.c
 * @brief 视频流 WebSocket 实现
 */

#include "video_ws.h"
#include "video.h"
#include "../config.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "VIDEO_WS";

#define WS_FRAME_INTERVAL_MS 80

static httpd_handle_t s_server_handle = NULL;
static int s_ws_fd = -1;
static TaskHandle_t s_ws_tx_task_handle = NULL;

// 后台异步推流 Task
static void ws_async_send_task(void *arg)
{
    ESP_LOGI(TAG, "后台 WebSocket 推流任务已启动");

    uint8_t *jpg_buf = NULL;
    size_t jpg_len = 0;

    while (s_ws_fd >= 0) {
        if (video_get_jpeg(&jpg_buf, &jpg_len) == ESP_OK && jpg_buf && jpg_len > 0) {
            httpd_ws_frame_t ws_pkt = {
                .type = HTTPD_WS_TYPE_BINARY,
                .payload = jpg_buf,
                .len = jpg_len
            };

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

    ESP_LOGI(TAG, "后台 WebSocket 推流任务已安全退出");
    s_ws_fd = -1;
    s_ws_tx_task_handle = NULL;
    video_stop_streaming();
    vTaskDelete(NULL);
}

// WebSocket Handler
static esp_err_t video_ws_handler(httpd_req_t *req)
{
    int current_fd = httpd_req_to_sockfd(req);

    // 初始化连接
    if (s_ws_fd != current_fd) {
        ESP_LOGI(TAG, "新连接: fd=%d -> %d", s_ws_fd, current_fd);
        s_server_handle = req->handle;
        s_ws_fd = current_fd;
        video_stop_streaming();  // 先停止之前的推流
    }

    // 接收 WebSocket 帧
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = NULL;

    // 先获取帧头信息
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        // 非阻塞模式下没有数据是正常的
        if (ret == ESP_ERR_NOT_FOUND) {
            return ESP_OK;
        }
        ESP_LOGW(TAG, "接收帧失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "收到帧: type=%d, len=%d", ws_pkt.type, (int)ws_pkt.len);

    // 处理不同类型的帧
    switch (ws_pkt.type) {
        case HTTPD_WS_TYPE_TEXT: {
            // 分配缓冲区并接收文本数据
            char text_buf[256];
            if (ws_pkt.len > 0 && ws_pkt.len < sizeof(text_buf)) {
                ws_pkt.payload = (uint8_t *)text_buf;
                ret = httpd_ws_recv_frame(req, &ws_pkt, sizeof(text_buf));
                if (ret == ESP_OK) {
                    text_buf[ws_pkt.len] = 0;
                    ESP_LOGI(TAG, "文本消息: %s", text_buf);

                    // 收到 start 命令，开始推流
                    if (strncmp(text_buf, "start", 5) == 0) {
                        if (s_ws_tx_task_handle == NULL) {
                            ESP_LOGI(TAG, "收到 start，开始推流");
                            xTaskCreatePinnedToCore(ws_async_send_task, "ws_tx_task", 4096, NULL, 5, &s_ws_tx_task_handle, 1);
                        }
                    } else if (strncmp(text_buf, "stop", 4) == 0) {
                        ESP_LOGI(TAG, "收到 stop");
                        if (s_ws_tx_task_handle != NULL) {
                            s_ws_fd = -1;  // 通知任务退出
                        }
                    }
                }
            }
            break;
        }

        case HTTPD_WS_TYPE_PING:
            ESP_LOGD(TAG, "收到 Ping");
            break;

        case HTTPD_WS_TYPE_PONG:
            ESP_LOGD(TAG, "收到 Pong");
            break;

        case HTTPD_WS_TYPE_CLOSE:
            ESP_LOGI(TAG, "收到关闭帧");
            s_ws_fd = -1;
            if (s_ws_tx_task_handle != NULL) {
                vTaskDelay(pdMS_TO_TICKS(100));  // 等待任务退出
            }
            break;

        default:
            ESP_LOGW(TAG, "未知帧类型: %d", ws_pkt.type);
            break;
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
