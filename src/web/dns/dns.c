/**
 * @file dns.c
 * @brief DNS 服务器实现 - Captive Portal DNS 劫持
 */
#include "dns.h"
#include "esp_log.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "WEB_DNS";

/* DNS 服务器任务句柄 */
static TaskHandle_t s_dns_task_handle = NULL;

/* DNS 响应头结构 */
typedef struct {
    uint16_t transaction_id;
    uint16_t flags;
    uint16_t questions;
    uint16_t answer_rrs;
    uint16_t authority_rrs;
    uint16_t additional_rrs;
} __attribute__((packed)) dns_header_t;

/* AP IP 地址 (通过参数传入或使用默认值) */
static uint32_t s_ap_ip = 0x0104A8C0;  // 默认 192.168.4.1

/**
 * @brief 设置 DNS 返回的 IP 地址
 */
void web_dns_set_ap_ip(uint32_t ip)
{
    s_ap_ip = ip;
}

/**
 * @brief 获取 IP 地址字符串
 */
static const char* ip_to_string(uint32_t ip)
{
    static char buf[16];
    snprintf(buf, sizeof(buf), "%d.%d.%d.%d", 
             ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
    return buf;
}

/**
 * @brief DNS 服务器任务
 */
static void dns_server_task(void *pvParameters)
{
    ESP_LOGI(TAG, "DNS 服务器任务启动");

    int sock;
    struct sockaddr_in server_addr, client_addr;
    char recv_buffer[512];
    char response_buffer[512];
    socklen_t addr_len = sizeof(client_addr);

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "创建 DNS socket 失败");
        vTaskDelete(NULL);
        return;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(53);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "绑定 DNS 端口失败");
        closesocket(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "DNS 服务器运行在端口 53");
    ESP_LOGI(TAG, "返回 IP: %s", ip_to_string(s_ap_ip));

    while (1) {
        memset(&client_addr, 0, sizeof(client_addr));
        int len = recvfrom(sock, recv_buffer, sizeof(recv_buffer) - 1, 0,
                           (struct sockaddr *)&client_addr, &addr_len);

        if (len > 0) {
            recv_buffer[len] = '\0';

            if (len >= (int)sizeof(dns_header_t) + 1) {
                dns_header_t *dns_header = (dns_header_t *)recv_buffer;

                // 构建响应
                dns_header->flags = htons(0x8180);  // 标准查询响应
                dns_header->answer_rrs = htons(1);   // 1 个答案

                memset(response_buffer, 0, sizeof(response_buffer));
                memcpy(response_buffer, recv_buffer, len);

                // Answer 部分
                int answer_pos = len;
                response_buffer[answer_pos++] = 0xC0;  // 指针标记
                response_buffer[answer_pos++] = sizeof(dns_header_t);  // 指向查询名称

                // Type: A 记录
                response_buffer[answer_pos++] = 0x00;
                response_buffer[answer_pos++] = 0x01;

                // Class: IN
                response_buffer[answer_pos++] = 0x00;
                response_buffer[answer_pos++] = 0x01;

                // TTL: 300 秒
                response_buffer[answer_pos++] = 0x00;
                response_buffer[answer_pos++] = 0x00;
                response_buffer[answer_pos++] = 0x01;
                response_buffer[answer_pos++] = 0x2C;

                // 数据长度: 4 字节
                response_buffer[answer_pos++] = 0x00;
                response_buffer[answer_pos++] = 0x04;

                // IP 地址
                memcpy(&response_buffer[answer_pos], &s_ap_ip, 4);
                answer_pos += 4;

                // 发送响应
                sendto(sock, response_buffer, answer_pos, 0,
                       (struct sockaddr *)&client_addr, addr_len);

                ESP_LOGD(TAG, "DNS 查询: 来自 %s, 返回 %s",
                         inet_ntoa(client_addr.sin_addr), ip_to_string(s_ap_ip));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    closesocket(sock);
    vTaskDelete(NULL);
}

/**
 * @brief 初始化 DNS 服务器
 */
esp_err_t web_dns_init(void)
{
    if (s_dns_task_handle != NULL) {
        ESP_LOGW(TAG, "DNS 服务器已经在运行");
        return ESP_OK;
    }

    BaseType_t ret = xTaskCreate(
        dns_server_task,
        "web_dns",
        4096,
        NULL,
        3,
        &s_dns_task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建 DNS 服务器任务失败");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "DNS 服务器初始化完成");
    return ESP_OK;
}

/**
 * @brief 反初始化 DNS 服务器
 */
esp_err_t web_dns_deinit(void)
{
    if (s_dns_task_handle != NULL) {
        vTaskDelete(s_dns_task_handle);
        s_dns_task_handle = NULL;
        ESP_LOGI(TAG, "DNS 服务器已停止");
    }
    return ESP_OK;
}
