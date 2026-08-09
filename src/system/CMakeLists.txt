idf_component_register(
    SRCS "system.c" "system_web.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_server freertos driver esp_timer
)
