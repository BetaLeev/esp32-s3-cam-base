idf_component_register(
    SRCS "servo.c" "servo_web.c"
    INCLUDE_DIRS "."
    REQUIRES driver freertos config web_module esp_http_server
)
