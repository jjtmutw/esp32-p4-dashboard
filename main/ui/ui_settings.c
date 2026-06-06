#include "lvgl.h"
#include "ui_settings.h"
#include "storage/nvs_config.h"
#include "ui_control.h"

typedef struct {
    control_item_t *item;
    lv_obj_t *modal;
    lv_obj_t *textarea;
    lv_obj_t *hint;
} settings_dialog_t;

static settings_dialog_t s_dialog;

static void close_dialog(void)
{
    if (s_dialog.modal != NULL) {
        lv_obj_del(s_dialog.modal);
    }
    s_dialog.modal = NULL;
    s_dialog.textarea = NULL;
    s_dialog.hint = NULL;
    s_dialog.item = NULL;
}

static void cancel_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    close_dialog();
}

static void save_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    const char *json = lv_textarea_get_text(s_dialog.textarea);
    if (!control_item_apply_json(s_dialog.item, json)) {
        lv_label_set_text(s_dialog.hint, "JSON format error");
        lv_obj_set_style_text_color(s_dialog.hint, lv_color_hex(0xff6b6b), 0);
        return;
    }

    nvs_config_save_controls();
    ui_control_refresh_all();
    close_dialog();
}

void ui_settings_open(control_item_t *item)
{
    if (item == NULL) {
        return;
    }

    close_dialog();
    s_dialog.item = item;

    lv_obj_t *modal = lv_obj_create(lv_layer_top());
    s_dialog.modal = modal;
    lv_obj_set_size(modal, 620, 520);
    lv_obj_center(modal);
    lv_obj_set_style_bg_color(modal, lv_color_hex(0x071824), 0);
    lv_obj_set_style_border_color(modal, lv_color_hex(0x21f6ff), 0);
    lv_obj_set_style_border_width(modal, 1, 0);
    lv_obj_set_style_radius(modal, 10, 0);
    lv_obj_set_style_pad_all(modal, 14, 0);

    lv_obj_t *title = lv_label_create(modal);
    lv_label_set_text_fmt(title, "Control JSON: %s", item->id);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *textarea = lv_textarea_create(modal);
    s_dialog.textarea = textarea;
    lv_obj_set_size(textarea, 590, 375);
    lv_obj_align(textarea, LV_ALIGN_TOP_MID, 0, 34);
    lv_textarea_set_one_line(textarea, false);
    lv_textarea_set_max_length(textarea, 1024);

    char *json = control_item_to_json_string(item);
    if (json != NULL) {
        lv_textarea_set_text(textarea, json);
        cJSON_free(json);
    }

    lv_obj_t *hint = lv_label_create(modal);
    s_dialog.hint = hint;
    lv_label_set_text(hint, "Edit fields, then Save");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x8cb9c8), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_LEFT, 4, -8);

    lv_obj_t *save = lv_btn_create(modal);
    lv_obj_set_size(save, 110, 42);
    lv_obj_align(save, LV_ALIGN_BOTTOM_RIGHT, -126, 0);
    lv_obj_add_event_cb(save, save_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *save_label = lv_label_create(save);
    lv_label_set_text(save_label, "Save");
    lv_obj_center(save_label);

    lv_obj_t *cancel = lv_btn_create(modal);
    lv_obj_set_size(cancel, 110, 42);
    lv_obj_align(cancel, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(cancel, cancel_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cancel_label = lv_label_create(cancel);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);
}
