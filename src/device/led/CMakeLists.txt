idf_component_register(
    SRCS "led.c" "led_web.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_server driver freertos config web_module
)
