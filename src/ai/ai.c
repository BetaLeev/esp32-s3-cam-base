/**
 * @file ai.c
 * @brief AI 语音交互核心模块实现
 */
#include "ai.h"
#include "ai_ws.h"
#include "audio/mic.h"
#include "config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

#define LOG_TAG "AI"

static ai_service_status_t s_ai_status = {
    .state = AI_STATE_UNINIT,
    .initialized = false,
    .ws_connected = false,
    .recognize_count = 0,
    .success_count = 0,
    .fail_count = 0
};

static int64_t s_last_update_time = 0;

static void update_state(ai_state_t new_state) {
    s_ai_status.state = new_state;
    s_last_update_time = esp_timer_get_time() / 1000;
    AI_LOGD(LOG_TAG, "AI 状态变更: %d", new_state);
}

/**
 * @brief 内部实现的文本命令处理分发逻辑（复用自 ws 和 ASR）
 */
void ai_process_recognized_text(const char *text) {
    AI_LOGI(LOG_TAG, "【语音识别转文字成功】: \"%s\"", text);
    
    char reply[256];
    const char *command_type = "unknown";
    const char *response_text = "未识别的语音命令";

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
        response_text = "系统运行正常，麦克风与音频模块工作良好";
    }

    // 如果是通过 WebSocket 连接的，顺便发给前端界面
    snprintf(reply, sizeof(reply), "{\"type\":\"recognition_result\",\"command\":\"%s\",\"text\":\"%s\",\"action\":\"%s\"}", command_type, text, response_text);
    ai_ws_send_text_if_connected(reply);
    
    ai_inc_recognize(true);
}

/**
 * @brief 麦克风底层音频流回调（在此处实现简易 ASR 触发）
 */
static void ai_mic_data_callback(const uint8_t *data, size_t size) {
    // 简单的能量累加检测：如果检测到连续有声音输入，模拟触发一次语音转文字
    static int voice_packet_count = 0;
    int16_t *samples = (int16_t *)data;
    size_t count = size / sizeof(int16_t);
    
    long energy = 0;
    for (size_t i = 0; i < count; i++) {
        energy += abs(samples[i]);
    }
    long avg_energy = count > 0 ? (energy / count) : 0;

    // 当检测到说话声音（能量阈值）时，累积计数
    if (avg_energy > 800) { 
        voice_packet_count++;
        // 持续约 1 秒的有效声音输入后，触发一次语音指令识别模拟
        if (voice_packet_count > 25) { 
            AI_LOGI(LOG_TAG, "检测到语音输入，正在转换文字...");
            // 示例：实际项目中可替换为离线命令词识别 (MultiNet) 或云端 ASR 识别结果
            ai_process_recognized_text("打开灯"); 
            voice_packet_count = 0;
        }
    } else {
        if (voice_packet_count > 0) voice_packet_count--;
    }
}

esp_err_t ai_init(void) {
    if (s_ai_status.initialized) {
        return ESP_OK;
    }

    AI_LOGI(LOG_TAG, "初始化 AI 模块并绑定麦克风回调...");
    update_state(AI_STATE_INITING);

    // 注册麦克风音频流监听回调
    mic_register_callback(ai_mic_data_callback);

    update_state(AI_STATE_IDLE);
    s_ai_status.initialized = true;

    AI_LOGI(LOG_TAG, "AI 模块初始化完成");
    return ESP_OK;
}

esp_err_t ai_deinit(void) {
    if (!s_ai_status.initialized) {
        return ESP_OK;
    }
    mic_register_callback(NULL);
    update_state(AI_STATE_UNINIT);
    s_ai_status.initialized = false;
    return ESP_OK;
}

ai_state_t ai_get_state(void) { return s_ai_status.state; }
const ai_service_status_t *ai_get_service_status(void) { return &s_ai_status; }
bool ai_is_initialized(void) { return s_ai_status.initialized; }

void ai_reset_stats(void) {
    s_ai_status.recognize_count = 0;
    s_ai_status.success_count = 0;
    s_ai_status.fail_count = 0;
}

void ai_inc_recognize(bool success) {
    s_ai_status.recognize_count++;
    if (success) s_ai_status.success_count++;
    else s_ai_status.fail_count++;
}

void ai_set_ws_connected(bool connected) { s_ai_status.ws_connected = connected; }

const char *ai_state_to_string(ai_state_t state) {
    switch (state) {
        case AI_STATE_IDLE: return "idle";
        case AI_STATE_LISTENING: return "listening";
        case AI_STATE_RECOGNIZING: return "recognizing";
        default: return "unknown";
    }
}