#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "ui_main.h"
#include "storage/nvs_config.h"
#include "mqtt/mqtt_client_app.h"

void app_main(void)
{
    nvs_config_init();
    nvs_config_load_controls();

    // TODO: initialize Waveshare BSP display/touch here.
    // TODO: call lv_init() and register LCD + touch drivers via BSP example.

    ui_main_create();
    mqtt_app_start();

    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
