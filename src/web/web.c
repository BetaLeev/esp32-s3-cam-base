/**
 * @file web.c
 * @brief Web 模块统一初始化
 */
#include "web.h"
#include "esp_log.h"
#include "lwip/ip_addr.h"
#include "esp_netif.h"
#include "wifi/wifi.h"
#include "dns/dns.h"
#include "http/http.h"
#include "filesystem/filesystem.h"

static const char *TAG = "WEB";

/**
 * @brief 获取 AP 接口的 IP 地址
 */
static uint32_t get_ap_ip(void)
{
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap_netif == NULL) {
        ESP_LOGW(TAG, "无法获取 AP netif，使用默认 IP");
        return ipaddr_addr("192.168.4.1");
    }

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(ap_netif, &ip_info) != ESP_OK) {
        ESP_LOGW(TAG, "无法获取 AP IP，使用默认 IP");
        return ipaddr_addr("192.168.4.1");
    }

    return ip_info.ip.addr;
}

/**
 * @brief Web 模块初始化
 */
esp_err_t web_init(void)
{
    ESP_LOGI(TAG, "========== 初始化 Web 模块 ==========");

    /* 1. 初始化文件系统 */
    esp_err_t ret = web_filesystem_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "文件系统初始化失败: %s", esp_err_to_name(ret));
        ESP_LOGW(TAG, "HTTP 服务器仍会启动，但静态文件服务不可用");
    }

    /* 2. 获取 AP IP 并设置 DNS */
    uint32_t ap_ip = get_ap_ip();
    web_dns_set_ap_ip(ap_ip);
    ESP_LOGI(TAG, "AP IP: " IPSTR, IP2STR((ip4_addr_t *)&ap_ip));

    /* 3. 初始化 DNS 服务器 */
    ret = web_dns_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "DNS 服务器初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 4. 初始化 HTTP 服务器 */
    ret = web_http_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP 服务器初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "========== Web 模块初始化完成 ==========");
    return ESP_OK;
}

/**
 * @brief Web 模块反初始化
 */
esp_err_t web_deinit(void)
{
    ESP_LOGI(TAG, "反初始化 Web 模块...");

    web_http_deinit();
    web_dns_deinit();

    ESP_LOGI(TAG, "Web 模块已反初始化");
    return ESP_OK;
}
