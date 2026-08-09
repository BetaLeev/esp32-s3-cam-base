idf_component_register(
    SRCS "wifi.c" "wifi_web.c" "wifi_config.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_netif lwip nvs_flash esp_http_server freertos cjson
)
