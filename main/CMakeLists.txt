idf_component_register(
    SRCS
        "app_main.c"
        "audio/button_feedback.c"
        "board/dashboard_board.c"
        "camera/camera_stream.c"
        "ui/ui_main.c"
        "ui/ui_text_image.c"
        "ui/ui_control.c"
        "ui/ui_sensor.c"
        "ui/ui_camera.c"
        "ui/ui_settings.c"
        "app_config/device_config.c"
        "mqtt/mqtt_client_app.c"
        "setup/setup_manager.c"
        "storage/nvs_config.c"
    INCLUDE_DIRS "." "audio" "board" "camera" "ui" "app_config" "mqtt" "setup" "storage"
    REQUIRES esp_driver_jpeg esp_http_client esp_http_server esp_event esp_mm esp_netif esp_wifi mbedtls mqtt nvs_flash json
)
