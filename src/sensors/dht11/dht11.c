/**
 * @file dht11.c
 * @brief DHT11 温湿度传感器实现
 *
 * DHT11 单总线协议：
 * 1. 主机发送起始信号（拉低 >= 18ms）
 * 2. DHT11 响应：拉低 80us，再拉高 80us
 * 3. DHT11 发送 40 位数据
 * 4. 每位数据：先 50us 低电平，然后高电平表示 0 或 1
 *
 * DHT11 数据格式：
 *   byte[0] = 湿度整数 (例如 50 表示 50%)
 *   byte[1] = 湿度小数 (DHT11 固定为 0)
 *   byte[2] = 温度整数 (例如 25 表示 25°C)
 *   byte[3] = 温度小数 (DHT11 固定为 0)
 *   byte[4] = 校验和 = byte[0] + byte[1] + byte[2] + byte[3]
 */

#include "dht11.h"
#include "../../config.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "rom/ets_sys.h"

static const char *TAG = "DHT11";
#define LOG_TAG TAG

/* ========================================
 * 内部变量
 * ======================================== */

static gpio_num_t s_gpio = DHT11_DEFAULT_GPIO;
static SemaphoreHandle_t s_mutex = NULL;
static float s_temperature = 0.0f;
static float s_humidity = 0.0f;
static bool s_valid = false;
static uint32_t s_fail_count = 0;

/* ========================================
 * 内部函数
 * ======================================== */

static void delay_us(uint32_t us)
{
    ets_delay_us(us);
}

static esp_err_t read_raw(uint8_t *raw_data)
{
    if (raw_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    // 发送起始信号
    gpio_set_direction(s_gpio, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(s_gpio, 0);
    delay_us(20000);  // 拉低 >= 18ms
    gpio_set_level(s_gpio, 1);
    delay_us(30);    // 主机释放总线

    // 切换到输入模式
    gpio_set_direction(s_gpio, GPIO_MODE_INPUT);

    // 等待 DHT11 响应
    int timeout = 200;
    while (gpio_get_level(s_gpio) == 1) {
        if (timeout-- == 0) {
            xSemaphoreGive(s_mutex);
            return ESP_ERR_TIMEOUT;
        }
        delay_us(1);
    }

    timeout = 200;
    while (gpio_get_level(s_gpio) == 0) {
        if (timeout-- == 0) {
            xSemaphoreGive(s_mutex);
            return ESP_ERR_TIMEOUT;
        }
        delay_us(1);
    }

    timeout = 200;
    while (gpio_get_level(s_gpio) == 1) {
        if (timeout-- == 0) {
            xSemaphoreGive(s_mutex);
            return ESP_ERR_TIMEOUT;
        }
        delay_us(1);
    }

    // 读取 40 位数据
    for (int i = 0; i < 5; i++) {
        raw_data[i] = 0;
        for (int j = 7; j >= 0; j--) {
            // 等待位开始（低电平）
            timeout = 100;
            while (gpio_get_level(s_gpio) == 0) {
                if (timeout-- == 0) {
                    xSemaphoreGive(s_mutex);
                    return ESP_ERR_TIMEOUT;
                }
                delay_us(1);
            }

            // 等待 50us 后读取电平
            delay_us(50);

            if (gpio_get_level(s_gpio) == 1) {
                raw_data[i] |= (1 << j);
            }

            // 等待位结束
            timeout = 100;
            while (gpio_get_level(s_gpio) == 1) {
                if (timeout-- == 0) {
                    xSemaphoreGive(s_mutex);
                    return ESP_ERR_TIMEOUT;
                }
                delay_us(1);
            }
        }
    }

    xSemaphoreGive(s_mutex);

    // 校验
    uint8_t sum = raw_data[0] + raw_data[1] + raw_data[2] + raw_data[3];
    if (sum != raw_data[4]) {
        return ESP_ERR_INVALID_CRC;
    }

    return ESP_OK;
}

/* ========================================
 * 接口实现
 * ======================================== */

esp_err_t dht11_init(gpio_num_t gpio)
{
    s_gpio = gpio;

    // 创建互斥锁
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
        if (s_mutex == NULL) {
            SENSORS_LOGE(TAG, "创建互斥锁失败");
            return ESP_FAIL;
        }
    }

    // 配置 GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << s_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    SENSORS_LOGI(TAG, "DHT11 初始化完成, GPIO%d", s_gpio);
    return ESP_OK;
}

void dht11_set_gpio(gpio_num_t gpio)
{
    dht11_init(gpio);
}

gpio_num_t dht11_get_gpio(void)
{
    return s_gpio;
}

esp_err_t dht11_read(dht11_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t raw_data[5] = {0};
    esp_err_t ret = read_raw(raw_data);

    if (ret == ESP_OK) {
        // DHT11 数据格式：整数部分
        data->humidity = (float)raw_data[0];
        data->temperature = (float)raw_data[2];
        data->checksum = raw_data[4];
        data->valid = true;

        // 更新内部状态
        s_humidity = data->humidity;
        s_temperature = data->temperature;
        s_valid = true;
        s_fail_count = 0;
    } else {
        data->valid = false;
        s_valid = false;
        s_fail_count++;
    }

    return ret;
}

float dht11_get_temperature(void)
{
    return s_temperature;
}

float dht11_get_humidity(void)
{
    return s_humidity;
}

bool dht11_is_valid(void)
{
    return s_valid;
}
