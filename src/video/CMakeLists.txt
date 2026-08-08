# 视频模块 CMakeLists.txt

idf_component_register(
    SRCS "video.c" "video_web.c"
    INCLUDE_DIRS "."
    REQUIRES esp32-camera esp_http_server freertos driver
)
