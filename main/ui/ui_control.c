#include "lvgl.h"
#include "ui_control.h"
#include "ui_settings.h"
#include "app_config/device_config.h"
#include "mqtt/mqtt_client_app.h"

static void card_event_cb(lv_event_t *e) {
    control_item_t *item = (control_item_t *)lv_event_get_user_data(e);
    if (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED) ui_settings_open(item);
}

static void btn_event_cb(lv_event_t *e) {
    control_item_t *item = (control_item_t *)lv_event_get_user_data(e);
    const char *txt = lv_label_get_text(lv_obj_get_child(lv_event_get_target(e), 0));
    item->state = (txt[1] == 'N');
    mqtt_publish_control(item, item->state);
}

void ui_control_create(lv_obj_t *parent) {
    int w = 205, h = 125, gap = 12;
    for (int i = 0; i < CONTROL_ITEM_COUNT; i++) {
        control_item_t *it = &g_control_items[i];
        lv_obj_t *card = lv_obj_create(parent);
        lv_obj_set_size(card, w, h);
        lv_obj_set_pos(card, 10 + (i % 3) * (w + gap), 12 + (i / 3) * (h + gap));
        lv_obj_set_style_bg_color(card, lv_color_hex(0x0b2230), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x159faf), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, 12, 0);
        lv_obj_add_event_cb(card, card_event_cb, LV_EVENT_LONG_PRESSED, it);

        lv_obj_t *name = lv_label_create(card);
        lv_label_set_text_fmt(name, "%02d  %s", i + 1, it->name);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_18, 0);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, 6, 4);

        lv_obj_t *sub = lv_label_create(card);
        lv_label_set_text(sub, it->subtitle);
        lv_obj_set_style_text_color(sub, lv_color_hex(0x8cb9c8), 0);
        lv_obj_align(sub, LV_ALIGN_TOP_LEFT, 10, 34);

        lv_obj_t *on = lv_btn_create(card);
        lv_obj_set_size(on, 78, 36);
        lv_obj_align(on, LV_ALIGN_BOTTOM_LEFT, 8, -4);
        lv_obj_set_style_bg_color(on, lv_color_hex(0x0ab9b9), 0);
        lv_obj_add_event_cb(on, btn_event_cb, LV_EVENT_CLICKED, it);
        lv_obj_t *onl = lv_label_create(on); lv_label_set_text(onl, "ON"); lv_obj_center(onl);

        lv_obj_t *off = lv_btn_create(card);
        lv_obj_set_size(off, 78, 36);
        lv_obj_align(off, LV_ALIGN_BOTTOM_RIGHT, -8, -4);
        lv_obj_set_style_bg_color(off, lv_color_hex(0x182936), 0);
        lv_obj_add_event_cb(off, btn_event_cb, LV_EVENT_CLICKED, it);
        lv_obj_t *offl = lv_label_create(off); lv_label_set_text(offl, "OFF"); lv_obj_center(offl);
    }
}
