#include "ui_background.h"

#include <stdint.h>

#include "app_config/device_config.h"
#include "misc/cache/instance/lv_image_cache.h"

#define DASHBOARD_BACKGROUND_W 720
#define DASHBOARD_BACKGROUND_H 720
#define DASHBOARD_BACKGROUND_BYTES (DASHBOARD_BACKGROUND_W * DASHBOARD_BACKGROUND_H * 2)

extern const uint8_t dashboard_background_rgb565_start[] asm("_binary_dashboard_background_rgb565_start");
extern const uint8_t dashboard_background_rgb565_end[] asm("_binary_dashboard_background_rgb565_end");

static lv_obj_t *s_background;

static const lv_image_dsc_t dashboard_background_fallback_image = {
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.cf = LV_COLOR_FORMAT_RGB565,
    .header.w = DASHBOARD_BACKGROUND_W,
    .header.h = DASHBOARD_BACKGROUND_H,
    .header.stride = DASHBOARD_BACKGROUND_W * 2,
    .data_size = DASHBOARD_BACKGROUND_BYTES,
    .data = dashboard_background_rgb565_start,
};

void ui_background_create(lv_obj_t *parent)
{
    if (parent == NULL || s_background != NULL) {
        return;
    }

    s_background = lv_image_create(parent);
    lv_obj_set_size(s_background, DASHBOARD_BACKGROUND_W, DASHBOARD_BACKGROUND_H);
    lv_obj_align(s_background, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(s_background, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_background, LV_OBJ_FLAG_SCROLLABLE);
    ui_background_refresh();
    lv_obj_move_background(s_background);
}

void ui_background_refresh(void)
{
    if (s_background == NULL) {
        return;
    }

    const lv_image_dsc_t *src = g_dashboard_background_image.valid
        ? &g_dashboard_background_image.dsc
        : &dashboard_background_fallback_image;

    const void *old_src = lv_image_get_src(s_background);
    if (old_src != NULL) {
        lv_image_cache_drop(old_src);
    }
    lv_image_cache_drop(&dashboard_background_fallback_image);
    lv_image_cache_drop(&g_dashboard_background_image.dsc);
    lv_image_set_src(s_background, src);
    lv_image_set_scale(s_background, LV_SCALE_NONE);
    lv_obj_align(s_background, LV_ALIGN_CENTER, 0, 0);
    lv_obj_move_background(s_background);
}
