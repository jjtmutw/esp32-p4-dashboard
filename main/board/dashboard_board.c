#include "dashboard_board.h"

#include "bsp/esp32_p4_wifi6_touch_lcd_4b.h"
#include "esp_log.h"

#define DASHBOARD_LCD_H_RES 720
#define DASHBOARD_LVGL_BUFFER_LINES 80

static const char *TAG = "dashboard_board";
static lv_display_t *s_display;

esp_err_t dashboard_board_init(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = DASHBOARD_LCD_H_RES * DASHBOARD_LVGL_BUFFER_LINES,
        .double_buffer = true,
        .flags = {
            .buff_dma = true,
            .buff_spiram = true,
            .sw_rotate = false,
        },
    };
    cfg.lvgl_port_cfg.task_stack = 16384;

    s_display = bsp_display_start_with_config(&cfg);
    if (s_display == NULL) {
        ESP_LOGE(TAG, "Waveshare BSP display/LVGL/touch init failed");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(bsp_display_brightness_init());
    ESP_ERROR_CHECK(bsp_display_brightness_set(90));
    ESP_ERROR_CHECK(bsp_display_backlight_on());
    ESP_LOGI(TAG, "LCD, GT911 touch, LVGL port and backlight are ready");
    return ESP_OK;
}

bool dashboard_board_lock(uint32_t timeout_ms)
{
    return bsp_display_lock(timeout_ms);
}

void dashboard_board_unlock(void)
{
    bsp_display_unlock();
}

lv_display_t *dashboard_board_display(void)
{
    return s_display;
}

void dashboard_board_set_brightness(int percent)
{
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_display_brightness_set(percent));
}

void dashboard_board_set_screen_on(bool on)
{
    ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_display_brightness_set(on ? 90 : 0));
}
