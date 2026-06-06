#include "ui_text_image.h"

#include "misc/cache/instance/lv_image_cache.h"

void ui_text_image_apply(lv_obj_t *image_obj, lv_obj_t *fallback_label,
                         const dashboard_text_image_t *image,
                         lv_align_t align, int x, int y)
{
    if (image_obj == NULL) {
        return;
    }

    if (image != NULL && image->valid && image->dsc.data != NULL) {
        const void *old_src = lv_image_get_src(image_obj);
        if (old_src != NULL) {
            lv_image_cache_drop(old_src);
        }
        lv_image_set_src(image_obj, &image->dsc);
        lv_obj_set_style_transform_pivot_x(image_obj, 0, 0);
        lv_obj_set_style_transform_pivot_y(image_obj, 0, 0);
        lv_image_set_scale(image_obj, LV_SCALE_NONE);
        lv_obj_set_style_image_recolor(image_obj, lv_color_hex(image->color), 0);
        lv_obj_set_style_image_recolor_opa(image_obj, LV_OPA_COVER, 0);
        lv_obj_align(image_obj, align, x, y);
        lv_obj_clear_flag(image_obj, LV_OBJ_FLAG_HIDDEN);
        if (fallback_label != NULL) {
            lv_obj_add_flag(fallback_label, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_obj_add_flag(image_obj, LV_OBJ_FLAG_HIDDEN);
        if (fallback_label != NULL) {
            lv_obj_clear_flag(fallback_label, LV_OBJ_FLAG_HIDDEN);
        }
    }
}
