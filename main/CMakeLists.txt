idf_component_register(
    SRCS
        "app_main.c"
        "ui/ui_main.c"
        "ui/ui_control.c"
        "ui/ui_sensor.c"
        "ui/ui_camera.c"
        "ui/ui_settings.c"
        "app_config/device_config.c"
        "mqtt/mqtt_client_app.c"
        "storage/nvs_config.c"
    INCLUDE_DIRS "." "ui" "app_config" "mqtt" "storage"
)
