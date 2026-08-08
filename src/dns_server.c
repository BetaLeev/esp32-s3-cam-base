/**
 * @file dns_server.c
 * @brief DNS服务器实现 - Captive Portal DNS劫持
 * @note 使用lwIP的DNS功能实现所有域名的DNS劫持
 */
#include "dns_server.h"
#include "config.h"
#include "wifi_app.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include "sys/socket.h"
#include "netinet/in.h"
#include "arpa/inet.h"

static const char *TAG = "DNS_SERVER";
#define LOG_TAG TAG

/* DNS服务器句柄 */
static TaskHandle_t dns_server_task_handle = NULL;

/* DNS响应信息 */
typedef struct {
    uint16_t transaction_id;
    uint16_t flags;
    uint16_t questions;
    uint16_t answer_rrs;
    uint16_t authority_rrs;
    uint16_t additional_rrs;
} __attribute__((packed)) dns_header_t;

/**
 * @brief DNS服务器任务 - 处理DNS查询
 */
static void dns_server_task(void *pvParameters)
{
    ESP_LOGI(TAG, "DNS服务器任务启动");

    int sock;
    struct sockaddr_in server_addr, client_addr;
    char recv_buffer[512];
    char response_buffer[512];
    socklen_t addr_len = sizeof(client_addr);

    // 创建UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        DNS_LOGE(TAG, "创建DNS socket失败");
        vTaskDelete(NULL);
        return;
    }

    // 配置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(53);

    // 绑定端口
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "绑定DNS端口失败");
        closesocket(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "DNS服务器运行在端口 53");

    // 获取AP IP地址
    char ap_ip_str[16];
    wifi_app_get_ap_ip_string(ap_ip_str, sizeof(ap_ip_str));
    uint32_t ap_ip = ipaddr_addr(ap_ip_str);

    while (1) {
        // 接收DNS查询
        int len = recvfrom(sock, recv_buffer, sizeof(recv_buffer) - 1, 0,
                           (struct sockaddr *)&client_addr, &addr_len);

        if (len > 0) {
            // 解析DNS查询包
            if (len >= sizeof(dns_header_t) + 1) {
                dns_header_t *dns_header = (dns_header_t *)recv_buffer;

                // 设置响应标志
                dns_header->flags = htons(0x8180);  // 标准查询响应
                dns_header->answer_rrs = htons(1);  // 1个答案

                // 构建DNS响应
                memset(response_buffer, 0, sizeof(response_buffer));
                memcpy(response_buffer, recv_buffer, len);

                // 找到查询名称的开始位置
                int name_start = sizeof(dns_header_t);

                // Answer名称 (指向查询名称的指针)
                int answer_pos = len;
                response_buffer[answer_pos++] = 0xC0;  // 指针标记
                response_buffer[answer_pos++] = name_start;  // 指向查询名称

                // Type: A记录
                response_buffer[answer_pos++] = 0x00;
                response_buffer[answer_pos++] = 0x01;

                // Class: IN
                response_buffer[answer_pos++] = 0x00;
                response_buffer[answer_pos++] = 0x01;

                // TTL: 300秒
                response_buffer[answer_pos++] = 0x00;
                response_buffer[answer_pos++] = 0x00;
                response_buffer[answer_pos++] = 0x01;
                response_buffer[answer_pos++] = 0x2C;

                // 数据长度: 4字节 (IPv4)
                response_buffer[answer_pos++] = 0x00;
                response_buffer[answer_pos++] = 0x04;

                // IP地址 (4字节)
                memcpy(&response_buffer[answer_pos], &ap_ip, 4);
                answer_pos += 4;

                ESP_LOGI(TAG, "DNS查询: 来自 %s, 返回 %s",
                        inet_ntoa(client_addr.sin_addr), ap_ip_str);

                // 发送DNS响应
                sendto(sock, response_buffer, answer_pos, 0,
                       (struct sockaddr *)&client_addr, addr_len);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    closesocket(sock);
    vTaskDelete(NULL);
}

/**
 * @brief DNS服务器初始化
 */
esp_err_t dns_server_init(void)
{
    if (dns_server_task_handle != NULL) {
        DNS_LOGW(TAG, "DNS服务器已经在运行");
        return ESP_OK;
    }

    BaseType_t ret = xTaskCreate(
        dns_server_task,
        "dns_server",
        4096,
        NULL,
        3,
        &dns_server_task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建DNS服务器任务失败");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "DNS服务器初始化完成");
    return ESP_OK;
}

/**
 * @brief DNS服务器反初始化
 */
esp_err_t dns_server_deinit(void)
{
    if (dns_server_task_handle != NULL) {
        vTaskDelete(dns_server_task_handle);
        dns_server_task_handle = NULL;
        ESP_LOGI(TAG, "DNS服务器已停止");
    }
    return ESP_OK;
}
