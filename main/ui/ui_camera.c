#include "ui_camera.h"

#include <stdint.h>
#include <stdio.h>
#include "app_config/device_config.h"
#include "audio/button_feedback.h"
#include "camera/camera_stream.h"
#include "lvgl.h"
#include "mqtt/mqtt_client_app.h"
#include "ui_text_image.h"

typedef struct {
    lv_obj_t *card;
    lv_obj_t *title;
    lv_obj_t *title_image;
    lv_obj_t *live;
    lv_obj_t *video;
    lv_obj_t *image;
    lv_obj_t *placeholder;
    lv_obj_t *ops;
    lv_obj_t *snap;
} camera_card_t;

static camera_card_t s_cards[CAMERA_ITEM_COUNT];
static lv_obj_t *s_camera_parent;

#define CAMERA_CARD_W 672
#define CAMERA_CARD_H 588
#define CAMERA_CARD_GAP 18
#define CAMERA_CARD_TOP_Y 18
#define CAMERA_PAGE_W 704

static void layout_visible_cards(void);
static bool camera_item_configured(const camera_item_t *item);
static void camera_card_set_blank(int i, bool blank);

static void snap_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    int index = (int)(intptr_t)lv_event_get_user_data(e);
    if (index < 0 || index >= CAMERA_ITEM_COUNT) {
        return;
    }

    camera_item_t *item = &g_camera_items[index];
    if (!item->visible || item->device_id[0] == '\0') {
        return;
    }

    char topic[64];
    snprintf(topic, sizeof(topic), "jj/camera/cmd/%s", item->device_id);
    button_feedback_beep();
    mqtt_publish_camera_command(topic, "snap");
}

static bool camera_item_configured(const camera_item_t *item)
{
    return item != NULL &&
           (item->label[0] != '\0' || item->topic[0] != '\0' ||
            item->device_id[0] != '\0' || item->url[0] != '\0');
}

static void camera_card_set_blank(int i, bool blank)
{
    if (i < 0 || i >= CAMERA_ITEM_COUNT || s_cards[i].card == NULL) {
        return;
    }

    lv_obj_clear_flag(s_cards[i].card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(s_cards[i].card, blank ? LV_OPA_TRANSP : LV_OPA_80, 0);
    lv_obj_set_style_border_opa(s_cards[i].card, blank ? LV_OPA_TRANSP : LV_OPA_COVER, 0);

    lv_obj_t *children[] = {
        s_cards[i].title,
        s_cards[i].title_image,
        s_cards[i].live,
        s_cards[i].video,
        s_cards[i].ops,
        s_cards[i].snap,
    };
    for (size_t n = 0; n < sizeof(children) / sizeof(children[0]); n++) {
        if (children[n] == NULL) {
            continue;
        }
        if (blank) {
            lv_obj_add_flag(children[n], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(children[n], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void refresh_camera_card(int i)
{
    if (i < 0 || i >= CAMERA_ITEM_COUNT || s_cards[i].card == NULL) {
        return;
    }
    camera_item_t *item = &g_camera_items[i];
    if (!camera_item_configured(item)) {
        lv_obj_add_flag(s_cards[i].card, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (!item->visible) {
        camera_card_set_blank(i, true);
        camera_stream_set_device_id(i, "");
        camera_stream_set_url(i, "");
        return;
    }
    camera_card_set_blank(i, false);
    lv_label_set_text_fmt(s_cards[i].title, "%02d  %s", i + 1, item->label);
    ui_text_image_apply(s_cards[i].title_image, s_cards[i].title, &item->label_image, LV_ALIGN_TOP_LEFT, 12, 6);
    camera_stream_set_device_id(i, item->device_id);
    camera_stream_set_url(i, item->url);
}

void ui_camera_refresh_all(void)
{
    for (int i = 0; i < CAMERA_ITEM_COUNT; i++) {
        refresh_camera_card(i);
    }
    layout_visible_cards();
}

void ui_camera_create(lv_obj_t *parent)
{
    int start_x = (CAMERA_PAGE_W - CAMERA_CARD_W) / 2;
    s_camera_parent = parent;

    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_pad_row(parent, 0, 0);
    lv_obj_set_style_pad_column(parent, 0, 0);
    lv_obj_set_scroll_dir(parent, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(parent, LV_SCROLL_SNAP_CENTER);
    lv_obj_scroll_to_x(parent, 0, LV_ANIM_OFF);

    for (int i = 0; i < CAMERA_ITEM_COUNT; i++) {
        lv_obj_t *card = lv_obj_create(parent);
        lv_obj_set_size(card, CAMERA_CARD_W, CAMERA_CARD_H);
        lv_obj_set_pos(card, start_x, CAMERA_CARD_TOP_Y + i * (CAMERA_CARD_H + CAMERA_CARD_GAP));
        lv_obj_set_style_bg_color(card, lv_color_hex(0x0b2230), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_80, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x159faf), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        s_cards[i].card = card;

        lv_obj_t *title = lv_label_create(card);
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
        lv_obj_set_width(title, 430);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 12, 6);
        s_cards[i].title = title;

        lv_obj_t *title_image = lv_image_create(card);
        lv_obj_add_flag(title_image, LV_OBJ_FLAG_HIDDEN);
        s_cards[i].title_image = title_image;

        lv_obj_t *live = lv_label_create(card);
        lv_label_set_text(live, "WAIT");
        lv_obj_set_width(live, 150);
        lv_label_set_long_mode(live, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(live, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_font(live, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(live, lv_color_hex(0xffd166), 0);
        lv_obj_align(live, LV_ALIGN_TOP_RIGHT, -48, 6);
        s_cards[i].live = live;

        lv_obj_t *video = lv_obj_create(card);
        lv_obj_set_size(video, 640, 480);
        lv_obj_align(video, LV_ALIGN_TOP_MID, 0, 44);
        lv_obj_set_style_bg_color(video, lv_color_hex(0x071c35), 0);
        lv_obj_set_style_radius(video, 8, 0);
        lv_obj_set_style_border_width(video, 0, 0);
        s_cards[i].video = video;

        lv_obj_t *placeholder = lv_label_create(video);
        lv_label_set_text(placeholder, "MQTT IMAGE");
        lv_obj_set_style_text_font(placeholder, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(placeholder, lv_color_hex(0x8cb9c8), 0);
        lv_obj_center(placeholder);

        lv_obj_t *image = lv_image_create(video);
        lv_obj_set_style_transform_pivot_x(image, 0, 0);
        lv_obj_set_style_transform_pivot_y(image, 0, 0);
        lv_obj_center(image);
        s_cards[i].image = image;
        s_cards[i].placeholder = placeholder;

        lv_obj_t *ops = lv_label_create(card);
        lv_label_set_text(ops, "device image via MQTT");
        lv_obj_set_style_text_font(ops, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(ops, lv_color_hex(0x8cb9c8), 0);
        lv_obj_align(ops, LV_ALIGN_BOTTOM_LEFT, 18, -14);
        s_cards[i].ops = ops;

        lv_obj_t *snap = lv_btn_create(card);
        lv_obj_set_size(snap, 110, 44);
        lv_obj_align(snap, LV_ALIGN_BOTTOM_RIGHT, -18, -8);
        lv_obj_set_style_bg_color(snap, lv_color_hex(0x159faf), 0);
        lv_obj_set_style_radius(snap, 8, 0);
        lv_obj_add_event_cb(snap, snap_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        s_cards[i].snap = snap;
        lv_obj_t *snap_label = lv_label_create(snap);
        lv_label_set_text(snap_label, "SNAP");
        lv_obj_set_style_text_font(snap_label, &lv_font_montserrat_18, 0);
        lv_obj_center(snap_label);

        lv_obj_move_foreground(title);
        lv_obj_move_foreground(title_image);
        lv_obj_move_foreground(live);
        camera_stream_attach_card(i, video, image, live, placeholder);
        refresh_camera_card(i);
    }
    layout_visible_cards();
}

static void layout_visible_cards(void)
{
    if (s_camera_parent == NULL) {
        return;
    }

    int start_x = (CAMERA_PAGE_W - CAMERA_CARD_W) / 2;
    int slot_count = 0;
    for (int i = 0; i < CAMERA_ITEM_COUNT; i++) {
        if (s_cards[i].card == NULL) {
            continue;
        }
        if (!camera_item_configured(&g_camera_items[i])) {
            lv_obj_add_flag(s_cards[i].card, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        slot_count = i + 1;
        lv_obj_set_pos(s_cards[i].card, start_x,
                       CAMERA_CARD_TOP_Y + i * (CAMERA_CARD_H + CAMERA_CARD_GAP));
        if (!g_camera_items[i].visible) {
            camera_card_set_blank(i, true);
        }
    }

    if (slot_count <= 1) {
        lv_obj_set_scroll_snap_y(s_camera_parent, LV_SCROLL_SNAP_NONE);
        lv_obj_set_scroll_dir(s_camera_parent, LV_DIR_NONE);
        lv_obj_clear_flag(s_camera_parent, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_scroll_to_y(s_camera_parent, 0, LV_ANIM_OFF);
    } else {
        lv_obj_add_flag(s_camera_parent, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(s_camera_parent, LV_DIR_VER);
        lv_obj_set_scroll_snap_y(s_camera_parent, LV_SCROLL_SNAP_CENTER);
    }
}
