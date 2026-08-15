idf_component_register(
    SRCS "motor.c" "motor_web.c"
    INCLUDE_DIRS "."
    REQUIRES driver freertos config web_module esp_http_server
)
