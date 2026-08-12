/**
 * @file ai_web.c
 * @brief AI Web API 实现
 */
#include "ai_web.h"
#include "ai.h"
#include "web_module.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "AI_Web";

/* ========================================
 * 支持的命令定义
 * ======================================== */

/**
 * @brief 命令结构
 */
typedef struct {
    const char *command;    /**< 命令名称 */
    const char *action;     /**< 执行动作描述 */
    const char *category;   /**< 分类 */
} ai_command_t;

/**
 * @brief 支持的命令列表
 */
static const ai_command_t s_supported_commands[] = {
    // LED 控制
    {"led_on", "打开LED灯", "执行器"},
    {"led_off", "关闭LED灯", "执行器"},
    {"led_toggle", "切换LED状态", "执行器"},

    // 水泵控制
    {"pump_on", "启动水泵", "执行器"},
    {"pump_off", "停止水泵", "执行器"},
    {"pump_speed", "设置水泵速度", "执行器"},

    // 舵机控制
    {"servo_left", "舵机左转", "执行器"},
    {"servo_right", "舵机右转", "执行器"},
    {"servo_center", "舵机居中", "执行器"},

    // 传感器查询
    {"query_temp", "查询温度", "传感器"},
    {"query_humidity", "查询湿度", "传感器"},
    {"query_light", "查询光照", "传感器"},
    {"query_all", "查询所有传感器", "传感器"},

    // 系统
    {"status", "查询系统状态", "系统"},
    {"help", "显示帮助", "系统"},
};

static const int s_command_count = sizeof(s_supported_commands) / sizeof(s_supported_commands[0]);

/* ========================================
 * HTTP Handlers
 * ======================================== */

/**
 * @brief API: 获取 AI 服务状态
 * GET /api/ai/status
 */
static esp_err_t ai_status_handler(httpd_req_t *req) {
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    const ai_service_status_t *status = ai_get_service_status();

    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "initialized", status->initialized);
    cJSON_AddStringToObject(data, "state", ai_state_to_string(status->state));
    cJSON_AddBoolToObject(data, "online", status->initialized);
    cJSON_AddNumberToObject(data, "recognize_count", status->recognize_count);
    cJSON_AddNumberToObject(data, "success_count", status->success_count);
    cJSON_AddNumberToObject(data, "fail_count", status->fail_count);

    cJSON *result = cJSON_CreateObject();
    cJSON_AddItemToObject(result, "data", data);

    esp_err_t ret = send_success(req, result, "获取AI状态成功");
    cJSON_Delete(result);
    return ret;
}

/**
 * @brief API: 获取支持的命令列表
 * GET /api/ai/commands
 */
static esp_err_t ai_commands_handler(httpd_req_t *req) {
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    cJSON *list = cJSON_CreateArray();

    for (int i = 0; i < s_command_count; i++) {
        cJSON *cmd = cJSON_CreateObject();
        cJSON_AddStringToObject(cmd, "command", s_supported_commands[i].command);
        cJSON_AddStringToObject(cmd, "action", s_supported_commands[i].action);
        cJSON_AddStringToObject(cmd, "category", s_supported_commands[i].category);
        cJSON_AddItemToArray(list, cmd);
    }

    cJSON *result = cJSON_CreateObject();
    cJSON_AddItemToObject(result, "list", list);
    cJSON_AddNumberToObject(result, "count", s_command_count);

    esp_err_t ret = send_success(req, result, "获取命令列表成功");
    cJSON_Delete(result);
    return ret;
}

/**
 * @brief API: 发送文本消息（模拟对话）
 * POST /api/ai/chat
 */
static esp_err_t ai_chat_handler(httpd_req_t *req) {
    if (req->method != HTTP_POST) {
        return send_bad_request(req, "仅支持 POST 请求");
    }

    // 解析请求体
    cJSON *root = parse_request_json(req);
    if (root == NULL) {
        return send_bad_request(req, "无效的 JSON 请求体");
    }

    cJSON *message = cJSON_GetObjectItem(root, "message");
    if (!cJSON_IsString(message) || message->valuestring == NULL) {
        cJSON_Delete(root);
        return send_bad_request(req, "缺少 message 字段");
    }

    const char *user_message = message->valuestring;
    ESP_LOGI(TAG, "收到聊天消息: %s", user_message);

    // 简单的关键词匹配（后续可扩展为本地 AI 或云端 API）
    const char *reply = "收到消息，当前支持语音命令控制。如需帮助，请说\"帮助\"。";
    const char *command = NULL;
    const char *action = NULL;

    // 关键词匹配
    if (strstr(user_message, "打开灯") || strstr(user_message, "开灯")) {
        command = "led_on";
        action = "已打开LED灯";
        reply = action;
    } else if (strstr(user_message, "关闭灯") || strstr(user_message, "关灯")) {
        command = "led_off";
        action = "已关闭LED灯";
        reply = action;
    } else if (strstr(user_message, "水泵") && strstr(user_message, "开")) {
        command = "pump_on";
        action = "已启动水泵";
        reply = action;
    } else if (strstr(user_message, "水泵") && strstr(user_message, "关")) {
        command = "pump_off";
        action = "已停止水泵";
        reply = action;
    } else if (strstr(user_message, "温度")) {
        command = "query_temp";
        action = "当前温度为 25.5°C";
        reply = action;
    } else if (strstr(user_message, "湿度")) {
        command = "query_humidity";
        action = "当前湿度为 60%";
        reply = action;
    } else if (strstr(user_message, "状态")) {
        command = "status";
        action = "系统运行正常，所有模块正常工作";
        reply = action;
    } else if (strstr(user_message, "帮助") || strstr(user_message, "help")) {
        command = "help";
        action = "支持命令：开灯、关灯、开泵、关泵、查温度、查湿度";
        reply = action;
    }

    // 更新统计
    if (command) {
        ai_inc_recognize(true);
    }

    // 构建响应
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "reply", reply);
    if (command) {
        cJSON_AddStringToObject(result, "command", command);
    }
    if (action) {
        cJSON_AddStringToObject(result, "action", action);
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "reply", cJSON_Duplicate(result, true));

    cJSON_Delete(root);

    esp_err_t ret = send_success(req, data, "消息已处理");
    cJSON_Delete(result);
    return ret;
}

/* ========================================
 * 路由注册
 * ======================================== */

esp_err_t ai_web_register_routes(httpd_handle_t handle) {
    if (handle == NULL) {
        ESP_LOGE(TAG, "HTTP 服务器句柄无效");
        return ESP_ERR_INVALID_ARG;
    }

    // GET /api/ai/status
    httpd_uri_t status_uri = {
        .uri = "/api/ai/status",
        .method = HTTP_GET,
        .handler = ai_status_handler,
        .user_ctx = NULL
    };

    // GET /api/ai/commands
    httpd_uri_t commands_uri = {
        .uri = "/api/ai/commands",
        .method = HTTP_GET,
        .handler = ai_commands_handler,
        .user_ctx = NULL
    };

    // POST /api/ai/chat
    httpd_uri_t chat_uri = {
        .uri = "/api/ai/chat",
        .method = HTTP_POST,
        .handler = ai_chat_handler,
        .user_ctx = NULL
    };

    esp_err_t ret = ESP_OK;

    ret = httpd_register_uri_handler(handle, &status_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册 /api/ai/status 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = httpd_register_uri_handler(handle, &commands_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册 /api/ai/commands 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = httpd_register_uri_handler(handle, &chat_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册 /api/ai/chat 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "AI HTTP API 路由注册完成");
    return ESP_OK;
}
