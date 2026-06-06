#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "board/dashboard_board.h"
#include "audio/button_feedback.h"
#include "ui_main.h"
#include "storage/nvs_config.h"
#include "app_config/device_config.h"
#include "setup/setup_manager.h"
#include "mqtt/mqtt_client_app.h"
#include "camera/camera_stream.h"

static const char *TAG = "app_main";

static void log_heap_checkpoint(const char *label)
{
    ESP_LOGI(TAG, "heap %s: internal=%u psram=%u",
             label,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_config_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    log_heap_checkpoint("after system init");
    nvs_config_load_device_id(g_dashboard_device_id, sizeof(g_dashboard_device_id));
    nvs_config_load_dashboard();
    dashboard_config_set_device_id(g_dashboard_device_id);
    log_heap_checkpoint("after config load");

    ESP_ERROR_CHECK(dashboard_board_init());
    button_feedback_start();
    log_heap_checkpoint("after board init");

    if (dashboard_board_lock(0)) {
        ui_main_create();
        ESP_LOGI(TAG, "Dashboard UI created");
        dashboard_board_unlock();
    } else {
        ESP_LOGE(TAG, "LVGL lock failed during UI creation");
    }

    log_heap_checkpoint("before network start");
    esp_err_t network_err = setup_manager_start();
    if (network_err == ESP_OK && setup_manager_is_online()) {
        camera_stream_start_all();
        mqtt_app_start();
    } else {
        ESP_LOGW(TAG, "Network setup portal active; dashboard data services are paused");
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
