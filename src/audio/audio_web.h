/**
 * @file audio_web.h
 * @brief 音频模块Web接口定义
 *
 * 提供HTTP API接口用于Web控制
 */
#ifndef AUDIO_WEB_H
#define AUDIO_WEB_H

#include <stdint.h>
#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief 初始化音频Web模块
 */
esp_err_t audio_web_init(void);

/**
 * @brief 反初始化音频Web模块
 */
esp_err_t audio_web_deinit(void);

/**
 * @brief 注册音频Web路由
 * @note 需要在HTTP服务器初始化后调用
 */
esp_err_t audio_web_register_routes(void);

/**
 * @brief 获取音频模块状态JSON
 * @param json_str 输出JSON字符串
 * @param max_len 缓冲区最大长度
 */
esp_err_t audio_web_get_status_json(char *json_str, size_t max_len);

/* ========================================
 * Web API 处理函数声明
 * ======================================== */

/**
 * @brief 获取音频状态API
 * @note URI: /api/audio/status
 * @note Method: GET
 */
esp_err_t audio_web_api_get_status(httpd_req_t *req);

/**
 * @brief 播放WAV文件API
 * @note URI: /api/audio/play
 * @note Method: POST
 * @note Body: {"file": "/web/test.wav"}
 */
esp_err_t audio_web_api_play(httpd_req_t *req);

/**
 * @brief 停止播放API
 * @note URI: /api/audio/stop
 * @note Method: POST
 */
esp_err_t audio_web_api_stop(httpd_req_t *req);

/**
 * @brief 配置引脚API
 * @note URI: /api/audio/pins
 * @note Method: POST
 * @note Body: {"bclk": 14, "ws": 15, "dout": 16, "gain": 21, "sd": 22}
 */
esp_err_t audio_web_api_set_pins(httpd_req_t *req);

/**
 * @brief 获取引脚配置API
 * @note URI: /api/audio/pins
 * @note Method: GET
 */
esp_err_t audio_web_api_get_pins(httpd_req_t *req);

/**
 * @brief 设置增益API
 * @note URI: /api/audio/gain
 * @note Method: POST
 * @note Body: {"gain": 9} (3, 6, 9, 12)
 */
esp_err_t audio_web_api_set_gain(httpd_req_t *req);

/**
 * @brief 设置音量API
 * @note URI: /api/audio/volume
 * @note Method: POST
 * @note Body: {"volume": 75} (0-100)
 */
esp_err_t audio_web_api_set_volume(httpd_req_t *req);

/**
 * @brief 测试音频播放API
 * @note URI: /api/audio/test
 * @note Method: POST
 * @note 播放内置测试音调
 */
esp_err_t audio_web_api_test(httpd_req_t *req);

#endif /* AUDIO_WEB_H */
