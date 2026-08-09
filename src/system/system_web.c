/**
 * @file system_web.c
 * @brief 系统管理 Web API 实现
 */
#include "system_web.h"
#include "system.h"
#include "../config.h"
#include "../web_module.h"
#include "esp_http_server.h"
#include "cJSON.h"

/**
 * @brief 注册系统模块的 URI 路由
 */
void system_web_register_routes(httpd_handle_t server) {
    httpd_uri_t routes[] = {
        {.uri = "/api/system/status",     .method = HTTP_GET, .handler = system_web_status_handler},
        {.uri = "/api/system/resources",  .method = HTTP_GET, .handler = system_web_resources_handler},
    };

    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(server, &routes[i]);
    }
}

/**
 * @brief 构建硬件资源 JSON 对象
 */
static void build_resources_object(cJSON *parent) {
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
 * @brief API: 获取系统状态
 * GET /api/system/status
 */
esp_err_t system_web_status_handler(httpd_req_t *req) {
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
esp_err_t system_web_resources_handler(httpd_req_t *req) {
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
