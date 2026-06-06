#pragma once

#include "app_config/device_config.h"
#include "lvgl.h"

void ui_text_image_apply(lv_obj_t *image_obj, lv_obj_t *fallback_label,
                         const dashboard_text_image_t *image,
                         lv_align_t align, int x, int y);
