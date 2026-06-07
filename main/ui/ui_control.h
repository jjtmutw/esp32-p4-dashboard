#pragma once
#include "lvgl.h"
#include "app_config/device_config.h"

void ui_control_create(lv_obj_t *parent);
void ui_control_refresh_item(control_item_t *item);
void ui_control_refresh_all(void);
void ui_control_mark_sent(control_item_t *item);
