/**
 * @file system_web.c
 * @brief 系统管理 Web API 实现
 */
#include "system_web.h"
#include "system.h"
#include "../config.h"
#include "../web/web_module.h"
#include "../device/led/led.h"
#include "../wifi/wifi.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "driver/gpio.h"

/**
 * @brief 注册系统模块的 URI 路由
 */
void system_web_register_routes(httpd_handle_t server)
{
    httpd_uri_t routes[] = {
        // 统一状态接口
        {.uri = "/api/status",             .method = HTTP_GET,  .handler = status_web_handler},

        // 网络状态
        {.uri = "/api/network",            .method = HTTP_GET,  .handler = system_web_network_handler},

        // 板子信息
        {.uri = "/api/system/info",        .method = HTTP_GET,  .handler = system_web_info_handler},

        // 温度
        {.uri = "/api/system/temp",        .method = HTTP_GET,  .handler = system_web_temp_handler},

        // 系统控制
        {.uri = "/api/system/reboot",      .method = HTTP_POST, .handler = system_web_reboot_handler},
        {.uri = "/api/system/shutdown",    .method = HTTP_POST, .handler = system_web_shutdown_handler},

        // 其他
        {.uri = "/api/system/status",      .method = HTTP_GET,  .handler = system_web_status_handler},
        {.uri = "/api/system/resources",   .method = HTTP_GET,  .handler = system_web_resources_handler},
    };

    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        esp_err_t ret = httpd_register_uri_handler(server, &routes[i]);
        if (ret != ESP_OK) {
            ESP_LOGE("SYSTEM_WEB", "注册路由失败: %s", routes[i].uri);
        }
    }

    ESP_LOGI("SYSTEM_WEB", "已注册 %d 个系统路由", (int)(sizeof(routes) / sizeof(routes[0])));
}

/* ========================================
 * 工具函数
 * ======================================== */

/**
 * @brief 构建硬件资源 JSON 对象
 */
static void build_resources_object(cJSON *parent)
{
    cJSON *dram = cJSON_CreateObject();
    cJSON_AddNumberToObject(dram, "total", system_get_dram_total());
    cJSON_AddNumberToObject(dram, "free", system_get_dram_free());
    cJSON_AddItemToObject(parent, "dram", dram);

    cJSON *psram = cJSON_CreateObject();
    cJSON_AddNumberToObject(psram, "total", system_get_psram_total());
    cJSON_AddNumberToObject(psram, "free", system_get_psram_free());
    cJSON_AddItemToObject(parent, "psram", psram);

    cJSON *flash = cJSON_CreateObject();
    cJSON_AddNumberToObject(flash, "total", system_get_flash_total());
    cJSON_AddNumberToObject(flash, "free", system_get_flash_free());
    cJSON_AddItemToObject(parent, "flash", flash);

    cJSON *spiffs = cJSON_CreateObject();
    cJSON_AddNumberToObject(spiffs, "total", system_get_spiffs_total());
    cJSON_AddNumberToObject(spiffs, "free", system_get_spiffs_free());
    cJSON_AddItemToObject(parent, "spiffs", spiffs);

    cJSON *sdcard = cJSON_CreateObject();
    cJSON_AddNumberToObject(sdcard, "total", g_system_status.sdcard_total);
    cJSON_AddNumberToObject(sdcard, "free", g_system_status.sdcard_free);
    cJSON_AddBoolToObject(sdcard, "mounted", g_system_status.sdcard_mounted);
    cJSON_AddItemToObject(parent, "sdcard", sdcard);

    cJSON *cpu = cJSON_CreateObject();
    cJSON_AddNumberToObject(cpu, "freq_mhz", system_get_cpu_freq_mhz());
    cJSON_AddItemToObject(parent, "cpu", cpu);
}

/**
 * @brief 格式化运行时间
 */
static void format_uptime(char *buf, size_t len, uint32_t seconds)
{
    uint32_t hours = seconds / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    uint32_t secs = seconds % 60;

    if (hours > 0) {
        snprintf(buf, len, "%lu小时%lu分%lu秒", (unsigned long)hours, (unsigned long)minutes, (unsigned long)secs);
    } else if (minutes > 0) {
        snprintf(buf, len, "%lu分%lu秒", (unsigned long)minutes, (unsigned long)secs);
    } else {
        snprintf(buf, len, "%lu秒", (unsigned long)secs);
    }
}

/* ========================================
 * API 实现
 * ======================================== */

/**
 * @brief API: 获取完整系统状态（统一接口，供前端轮询使用）
 * GET /api/status
 */
esp_err_t status_web_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return send_internal_error(req, "创建响应数据失败");
    }

    // 执行器状态
    cJSON_AddNumberToObject(data, "pump_state", g_system_status.pump_state);
    cJSON_AddNumberToObject(data, "pump_gear", g_system_status.pump_state ? 2 : 0);
    cJSON_AddStringToObject(data, "pump_gear_name", g_system_status.pump_state ? "中速" : "关闭");
    cJSON_AddNumberToObject(data, "pump_speed", g_system_status.pump_speed);
    cJSON_AddNumberToObject(data, "servo_angle", g_system_status.servo_angle);

    // 硬件资源
    cJSON_AddNumberToObject(data, "dram_free", system_get_dram_free());
    cJSON_AddNumberToObject(data, "dram_total", system_get_dram_total());
    cJSON_AddNumberToObject(data, "psram_free", system_get_psram_free());
    cJSON_AddNumberToObject(data, "psram_total", system_get_psram_total());
    cJSON_AddNumberToObject(data, "flash_total", system_get_flash_total());
    cJSON_AddNumberToObject(data, "spiffs_free", system_get_spiffs_free());
    cJSON_AddNumberToObject(data, "spiffs_total", system_get_spiffs_total());
    cJSON_AddBoolToObject(data, "sdcard_mounted", g_system_status.sdcard_mounted);
    cJSON_AddNumberToObject(data, "sdcard_free", g_system_status.sdcard_free);
    cJSON_AddNumberToObject(data, "uptime_seconds", system_get_uptime_seconds());

    // LED状态
    led_status_t led_status;
    led_config_t led_config;
    led_get_status(&led_status);
    led_get_config(&led_config);

    // LED引脚（-1 表示未设置）
    if (led_config.pin >= 0) {
        cJSON_AddNumberToObject(data, "led_pin", led_config.pin);
    } else {
        cJSON_AddNullToObject(data, "led_pin");
    }

    cJSON_AddBoolToObject(data, "led_enabled", led_status.enabled);
    cJSON_AddNumberToObject(data, "led_current_level", led_status.current_level);
    cJSON_AddNumberToObject(data, "led_executed_count", led_status.executed_count);
    cJSON_AddNumberToObject(data, "led_total_count", led_status.total_count);
    cJSON_AddNumberToObject(data, "led_elapsed_time", (int)led_status.elapsed_time);
    cJSON_AddNumberToObject(data, "led_remaining_time", (int)led_status.remaining_time);

    // 温度
    cJSON_AddNumberToObject(data, "chip_temp", system_get_chip_temp());

    // 脉冲控制状态
    if (g_system_status.pulse_pin >= 0) {
        cJSON_AddNumberToObject(data, "pulse_pin", g_system_status.pulse_pin);
    } else {
        cJSON_AddNullToObject(data, "pulse_pin");
    }
    cJSON_AddBoolToObject(data, "pulse_enabled", g_system_status.pulse_enabled);
    cJSON_AddNumberToObject(data, "pulse_current_intensity", g_system_status.pulse_current_intensity);
    cJSON_AddNumberToObject(data, "pulse_count", g_system_status.pulse_count);
    cJSON_AddNumberToObject(data, "pulse_elapsed_time", g_system_status.pulse_elapsed_time);
    cJSON_AddNumberToObject(data, "pulse_pin_level", g_system_status.pulse_pin_level);

    return send_success(req, data, "获取状态成功");
}

/**
 * @brief API: 获取板子基本信息
 * GET /api/system/info
 */
esp_err_t system_web_info_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return send_internal_error(req, "创建响应数据失败");
    }

    // 板子信息
    cJSON_AddStringToObject(data, "chip_model", system_get_chip_model());
    cJSON_AddStringToObject(data, "firmware_version", FIRMWARE_VERSION);
    cJSON_AddStringToObject(data, "build_time", system_get_build_time());
    cJSON_AddStringToObject(data, "board_name", "ESP32-S3 智能控制器");

    // 运行信息
    uint32_t uptime = system_get_uptime_seconds();
    char uptime_str[64];
    format_uptime(uptime_str, sizeof(uptime_str), uptime);
    cJSON_AddStringToObject(data, "uptime", uptime_str);
    cJSON_AddNumberToObject(data, "uptime_seconds", uptime);

    // 内存信息
    uint32_t free_heap = system_get_dram_free();
    if (free_heap >= 1024 * 1024) {
        char heap_str[32];
        snprintf(heap_str, sizeof(heap_str), "%.1f MB", free_heap / (1024.0 * 1024.0));
        cJSON_AddStringToObject(data, "free_heap", heap_str);
    } else {
        char heap_str[32];
        snprintf(heap_str, sizeof(heap_str), "%lu KB", free_heap / 1024);
        cJSON_AddStringToObject(data, "free_heap", heap_str);
    }

    return send_success(req, data, "获取板子信息成功");
}

/**
 * @brief API: 获取温度数据
 * GET /api/system/temp
 */
esp_err_t system_web_temp_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return send_internal_error(req, "创建响应数据失败");
    }

    // 芯片温度
    float chip_temp = system_get_chip_temp();
    cJSON_AddNumberToObject(data, "chip_temp", chip_temp);

    // 温度传感器状态
    cJSON_AddBoolToObject(data, "sensor_ok", system_temp_sensor_initialized());

    // 注意：环境温度和CPU温度需要额外传感器或计算
    // 这里暂时返回0，后续可扩展
    cJSON_AddNumberToObject(data, "ambient_temp", 0);
    cJSON_AddNumberToObject(data, "cpu_temp", 0);

    return send_success(req, data, "获取温度数据成功");
}

/**
 * @brief API: 系统重启
 * POST /api/system/reboot
 */
esp_err_t system_web_reboot_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_bad_request(req, "仅支持 POST 请求");
    }

    ESP_LOGW("SYSTEM_WEB", "收到重启请求");

    // 返回成功响应后再执行重启
    send_success(req, NULL, "系统正在重启...");

    // 延迟一点执行重启，确保响应发送完成
    vTaskDelay(pdMS_TO_TICKS(100));

    system_reboot();

    return ESP_OK;  // 不会执行到这里
}

/**
 * @brief API: 系统关机（深度睡眠）
 * POST /api/system/shutdown
 * Body: {"wakeup_pin": -1, "wakeup_level": 0, "sleep_mode": "light" | "deep"}
 */
esp_err_t system_web_shutdown_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_bad_request(req, "仅支持 POST 请求");
    }

    int wakeup_pin = -1;
    int wakeup_level = 0;

    // 尝试解析请求体
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = '\0';

        cJSON *root = cJSON_Parse(buf);
        if (root) {
            cJSON *pin_item = cJSON_GetObjectItem(root, "wakeup_pin");
            cJSON *level_item = cJSON_GetObjectItem(root, "wakeup_level");
            cJSON *mode_item = cJSON_GetObjectItem(root, "sleep_mode");

            if (pin_item && cJSON_IsNumber(pin_item)) {
                wakeup_pin = pin_item->valueint;
            }
            if (level_item && cJSON_IsNumber(level_item)) {
                wakeup_level = level_item->valueint;
            }
            if (mode_item && cJSON_IsString(mode_item)) {
                // 设置全局睡眠模式
                extern char g_sleep_mode[16];
                snprintf(g_sleep_mode, sizeof(g_sleep_mode), "%s", mode_item->valuestring);
            }

            cJSON_Delete(root);
        }
    }

    // 获取睡眠模式信息
    extern char g_sleep_mode[16];
    ESP_LOGW("SYSTEM_WEB", "收到关机请求，唤醒引脚: GPIO%d, 睡眠模式: %s", wakeup_pin, g_sleep_mode);

    // 返回成功响应后再执行关机
    send_success(req, NULL, "系统正在关机...");

    // 延迟一点执行关机，确保响应发送完成
    vTaskDelay(pdMS_TO_TICKS(100));

    system_shutdown(wakeup_pin, wakeup_level);

    return ESP_OK;  // 不会执行到这里
}

/**
 * @brief API: 获取网络状态
 * GET /api/network
 */
esp_err_t system_web_network_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return send_internal_error(req, "创建响应数据失败");
    }

    // WiFi 信号强度
    cJSON_AddNumberToObject(data, "rssi", wifi_get_sta_rssi());
    cJSON_AddBoolToObject(data, "connected", g_system_status.sta_connected);
    cJSON_AddStringToObject(data, "bssid", g_system_status.sta_bssid);

    return send_success(req, data, "获取网络状态成功");
}

/**
 * @brief API: 获取系统状态
 * GET /api/system/status
 */
esp_err_t system_web_status_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return send_internal_error(req, "创建响应数据失败");
    }

    // 系统基本信息
    cJSON_AddNumberToObject(data, "version", system_get_version());
    cJSON_AddNumberToObject(data, "uptime_seconds", system_get_uptime_seconds());

    // 传感器数据
    cJSON *sensors = cJSON_CreateObject();
    cJSON_AddNumberToObject(sensors, "thermistor_raw", g_system_status.thermistor_raw);
    cJSON_AddNumberToObject(sensors, "thermistor_temp", g_system_status.thermistor_temp);
    cJSON_AddNumberToObject(sensors, "photosensor_raw", g_system_status.photosensor_raw);
    cJSON_AddNumberToObject(sensors, "light", g_system_status.light);
    cJSON_AddNumberToObject(sensors, "dht11_temp", g_system_status.dht11_temp);
    cJSON_AddNumberToObject(sensors, "dht11_humidity", g_system_status.dht11_humidity);
    cJSON_AddBoolToObject(sensors, "dht11_valid", g_system_status.dht11_valid);
    cJSON_AddNumberToObject(sensors, "chip_temp", system_get_chip_temp());
    cJSON_AddItemToObject(data, "sensors", sensors);

    // 执行器状态
    cJSON *actuators = cJSON_CreateObject();
    cJSON_AddNumberToObject(actuators, "pump_state", g_system_status.pump_state);
    cJSON_AddNumberToObject(actuators, "pump_speed", g_system_status.pump_speed);
    cJSON_AddNumberToObject(actuators, "servo_angle", g_system_status.servo_angle);
    cJSON_AddItemToObject(data, "actuators", actuators);

    // 硬件资源
    cJSON *resources = cJSON_CreateObject();
    build_resources_object(resources);
    cJSON_AddItemToObject(data, "resources", resources);

    return send_success(req, data, "获取系统状态成功");
}

/**
 * @brief API: 获取硬件资源信息
 * GET /api/system/resources
 */
esp_err_t system_web_resources_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return send_internal_error(req, "创建响应数据失败");
    }

    build_resources_object(data);

    return send_success(req, data, "获取硬件资源信息成功");
}
