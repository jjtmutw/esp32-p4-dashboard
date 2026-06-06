#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "lvgl.h"

esp_err_t dashboard_board_init(void);
bool dashboard_board_lock(uint32_t timeout_ms);
void dashboard_board_unlock(void);
lv_display_t *dashboard_board_display(void);
void dashboard_board_set_brightness(int percent);
