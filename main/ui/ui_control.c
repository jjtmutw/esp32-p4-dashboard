#include "lvgl.h"
#include "ui_control.h"
#include "ui_settings.h"
#include "ui_text_image.h"
#include "app_config/device_config.h"
#include "audio/button_feedback.h"
#include "mqtt/mqtt_client_app.h"
#include "esp_timer.h"
#include <string.h>

typedef struct {
    lv_obj_t *card;
    lv_obj_t *name;
    lv_obj_t *name_image;
    lv_obj_t *subtitle;
    lv_obj_t *subtitle_image;
    lv_obj_t *state;
    lv_obj_t *left_btn;
    lv_obj_t *right_btn;
    lv_obj_t *wide_btn;
    lv_obj_t *wide_label;
    lv_timer_t *sent_timer;
    int64_t sent_until_ms;
} control_card_t;

typedef struct {
    control_item_t *item;
    const char *cmd;
} control_button_ctx_t;

#define CONTROL_CARD_W 218
#define CONTROL_CARD_H 188
#define CONTROL_CARD_GAP 12
#define CONTROL_CARD_TOP_Y 10
#define CONTROL_COLUMNS 3
#define CONTROL_CARDS_PER_PAGE 9
#define CONTROL_PAGE_W 704

static control_card_t s_cards[CONTROL_ITEM_COUNT];
static control_button_ctx_t s_button_ctx[CONTROL_ITEM_COUNT][3];
static lv_obj_t *s_control_parent;

static void btn_event_cb(lv_event_t *e);
static int control_item_index(control_item_t *item);
static void layout_visible_cards(void);
static bool control_item_is_message(const control_item_t *item);
static bool control_item_configured(const control_item_t *item);
static void control_card_set_blank(int i, bool blank);

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static void sent_timer_cb(lv_timer_t *timer)
{
    control_item_t *item = (control_item_t *)lv_timer_get_user_data(timer);
    int i = control_item_index(item);
    if (i >= 0) {
        s_cards[i].sent_timer = NULL;
        s_cards[i].sent_until_ms = 0;
        ui_control_refresh_item(item);
    }
    lv_timer_delete(timer);
}

static bool control_item_showing_sent(int i)
{
    return i >= 0 && s_cards[i].sent_until_ms > now_ms();
}

static lv_obj_t *create_command_button(lv_obj_t *card, int w, int h, lv_align_t align,
                                       int x, int y, uint32_t bg_color,
                                       const char *label_text, control_button_ctx_t *ctx)
{
    lv_obj_t *btn = lv_btn_create(card);
    lv_obj_set_size(btn, w, h);
    lv_obj_align(btn, align, x, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg_color), 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, ctx);

    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_label_set_text(label, label_text);
    lv_obj_center(label);
    return btn;
}

static void ensure_onoff_buttons(int i)
{
    if (s_cards[i].left_btn == NULL) {
        s_cards[i].left_btn = create_command_button(s_cards[i].card, 86, 34,
                                                    LV_ALIGN_BOTTOM_LEFT, 8, -4,
                                                    0x0ab9b9, "ON", &s_button_ctx[i][0]);
    }
    if (s_cards[i].right_btn == NULL) {
        s_cards[i].right_btn = create_command_button(s_cards[i].card, 86, 34,
                                                     LV_ALIGN_BOTTOM_RIGHT, -8, -4,
                                                     0x182936, "OFF", &s_button_ctx[i][1]);
    }
}

static void ensure_wide_button(int i, const char *label_text)
{
    if (s_cards[i].wide_btn == NULL) {
        s_cards[i].wide_btn = create_command_button(s_cards[i].card, 182, 34,
                                                    LV_ALIGN_BOTTOM_MID, 0, -4,
                                                    0x0ab9b9, label_text, &s_button_ctx[i][2]);
        s_cards[i].wide_label = lv_obj_get_child(s_cards[i].wide_btn, 0);
    } else if (s_cards[i].wide_label != NULL) {
        lv_label_set_text(s_cards[i].wide_label, label_text);
    }
}

static void card_event_cb(lv_event_t *e) {
    control_item_t *item = (control_item_t *)lv_event_get_user_data(e);
    if (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED) ui_settings_open(item);
}

static void btn_event_cb(lv_event_t *e) {
    control_button_ctx_t *ctx = (control_button_ctx_t *)lv_event_get_user_data(e);
    (void)lv_event_get_target(e);
    if (ctx == NULL || ctx->item == NULL) {
        return;
    }
    button_feedback_beep();
    mqtt_publish_control(ctx->item, ctx->cmd);
    if (control_item_is_message(ctx->item)) {
        ui_control_mark_sent(ctx->item);
    }
}

static int control_item_index(control_item_t *item)
{
    if (item == NULL) {
        return -1;
    }
    for (int i = 0; i < CONTROL_ITEM_COUNT; i++) {
        if (&g_control_items[i] == item) {
            return i;
        }
    }
    return -1;
}

static bool control_item_is_message(const control_item_t *item)
{
    return item != NULL && (strcmp(item->action, "url") == 0 || strcmp(item->action, "message") == 0);
}

static bool control_item_configured(const control_item_t *item)
{
    return item != NULL &&
           (item->id[0] != '\0' || item->name[0] != '\0' || item->subtitle[0] != '\0' ||
            item->url[0] != '\0' || item->url_on[0] != '\0' || item->url_off[0] != '\0' ||
            item->mqtt_topic[0] != '\0' || item->device[0] != '\0');
}

static void control_card_set_blank(int i, bool blank)
{
    if (i < 0 || i >= CONTROL_ITEM_COUNT || s_cards[i].card == NULL) {
        return;
    }

    lv_obj_clear_flag(s_cards[i].card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(s_cards[i].card, blank ? LV_OPA_TRANSP : LV_OPA_80, 0);
    lv_obj_set_style_border_opa(s_cards[i].card, blank ? LV_OPA_TRANSP : LV_OPA_COVER, 0);

    lv_obj_t *children[] = {
        s_cards[i].name,
        s_cards[i].name_image,
        s_cards[i].subtitle,
        s_cards[i].subtitle_image,
        s_cards[i].state,
        s_cards[i].left_btn,
        s_cards[i].right_btn,
        s_cards[i].wide_btn,
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

void ui_control_refresh_item(control_item_t *item)
{
    int i = control_item_index(item);
    if (i < 0 || s_cards[i].card == NULL) {
        return;
    }

    bool is_onoff = strcmp(item->action, "onoff") == 0 || strcmp(item->action, "on_off") == 0;
    bool is_url = control_item_is_message(item);

    if (!control_item_configured(item)) {
        lv_obj_add_flag(s_cards[i].card, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (!item->visible) {
        control_card_set_blank(i, true);
        return;
    }
    control_card_set_blank(i, false);

    lv_label_set_text_fmt(s_cards[i].name, "%02d  %s", i + 1, item->name);
    lv_label_set_text(s_cards[i].subtitle, item->subtitle);
    ui_text_image_apply(s_cards[i].name_image, s_cards[i].name, &item->name_image, LV_ALIGN_TOP_LEFT, 18, 12);
    ui_text_image_apply(s_cards[i].subtitle_image, s_cards[i].subtitle, &item->subtitle_image, LV_ALIGN_TOP_LEFT, 18, 46);
    if (control_item_showing_sent(i)) {
        lv_label_set_text(s_cards[i].state, "SENT");
        lv_obj_clear_flag(s_cards[i].state, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_border_color(s_cards[i].card, lv_color_hex(0x22f6b0), 0);
        lv_obj_set_style_text_color(s_cards[i].state, lv_color_hex(0x46ff8a), 0);
        lv_obj_set_style_opa(s_cards[i].state, LV_OPA_COVER, 0);
    } else if (is_url) {
        lv_label_set_text(s_cards[i].state, "");
        lv_obj_set_style_border_color(s_cards[i].card, lv_color_hex(0x159faf), 0);
        lv_obj_set_style_text_color(s_cards[i].state, lv_color_hex(0x6e8796), 0);
        lv_obj_set_style_opa(s_cards[i].state, LV_OPA_40, 0);
    } else {
        lv_label_set_text(s_cards[i].state, item->state ? "ON" : "OFF");
        lv_obj_clear_flag(s_cards[i].state, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_border_color(s_cards[i].card, lv_color_hex(item->state ? 0x22f6b0 : 0x159faf), 0);
        lv_obj_set_style_text_color(s_cards[i].state, lv_color_hex(item->state ? 0x46ff8a : 0x8cb9c8), 0);
        lv_obj_set_style_opa(s_cards[i].state, LV_OPA_COVER, 0);
    }

    if (is_onoff) {
        ensure_onoff_buttons(i);
        lv_obj_clear_flag(s_cards[i].left_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_cards[i].right_btn, LV_OBJ_FLAG_HIDDEN);
        if (s_cards[i].wide_btn != NULL) {
            lv_obj_add_flag(s_cards[i].wide_btn, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        const char *label = is_url ? "SEND" : "TOGGLE";
        ensure_wide_button(i, label);
        if (s_cards[i].left_btn != NULL) {
            lv_obj_add_flag(s_cards[i].left_btn, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_cards[i].right_btn != NULL) {
            lv_obj_add_flag(s_cards[i].right_btn, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_clear_flag(s_cards[i].wide_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_control_refresh_all(void)
{
    for (int i = 0; i < CONTROL_ITEM_COUNT; i++) {
        ui_control_refresh_item(&g_control_items[i]);
    }
    layout_visible_cards();
}

void ui_control_mark_sent(control_item_t *item)
{
    int i = control_item_index(item);
    if (i < 0 || s_cards[i].card == NULL) {
        return;
    }

    s_cards[i].sent_until_ms = now_ms() + 3000;
    if (s_cards[i].sent_timer == NULL) {
        s_cards[i].sent_timer = lv_timer_create(sent_timer_cb, 3000, item);
    } else {
        lv_timer_reset(s_cards[i].sent_timer);
    }
    ui_control_refresh_item(item);
}

void ui_control_create(lv_obj_t *parent) {
    int total_w = (CONTROL_CARD_W * CONTROL_COLUMNS) + (CONTROL_CARD_GAP * (CONTROL_COLUMNS - 1));
    int start_x = (CONTROL_PAGE_W - total_w) / 2;
    s_control_parent = parent;

    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_pad_row(parent, 0, 0);
    lv_obj_set_style_pad_column(parent, 0, 0);
    lv_obj_set_scroll_dir(parent, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(parent, LV_SCROLL_SNAP_START);
    lv_obj_scroll_to_x(parent, 0, LV_ANIM_OFF);

    for (int i = 0; i < CONTROL_ITEM_COUNT; i++) {
        control_item_t *it = &g_control_items[i];
        lv_obj_t *card = lv_obj_create(parent);
        lv_obj_set_size(card, CONTROL_CARD_W, CONTROL_CARD_H);
        lv_obj_set_pos(card, start_x + (i % CONTROL_COLUMNS) * (CONTROL_CARD_W + CONTROL_CARD_GAP),
                       CONTROL_CARD_TOP_Y + (i / CONTROL_COLUMNS) * (CONTROL_CARD_H + CONTROL_CARD_GAP));
        lv_obj_set_style_bg_color(card, lv_color_hex(0x0b2230), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_80, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x159faf), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_add_event_cb(card, card_event_cb, LV_EVENT_LONG_PRESSED, it);
        s_cards[i].card = card;

        lv_obj_t *name = lv_label_create(card);
        lv_label_set_text_fmt(name, "%02d  %s", i + 1, it->name);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_width(name, 192);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_18, 0);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, 18, 12);
        s_cards[i].name = name;

        lv_obj_t *name_image = lv_image_create(card);
        lv_obj_add_flag(name_image, LV_OBJ_FLAG_HIDDEN);
        s_cards[i].name_image = name_image;

        lv_obj_t *sub = lv_label_create(card);
        lv_label_set_text(sub, it->subtitle);
        lv_label_set_long_mode(sub, LV_LABEL_LONG_DOT);
        lv_obj_set_width(sub, 192);
        lv_obj_set_style_text_font(sub, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(sub, lv_color_hex(0x8cb9c8), 0);
        lv_obj_align(sub, LV_ALIGN_TOP_LEFT, 18, 46);
        s_cards[i].subtitle = sub;

        lv_obj_t *subtitle_image = lv_image_create(card);
        lv_obj_add_flag(subtitle_image, LV_OBJ_FLAG_HIDDEN);
        s_cards[i].subtitle_image = subtitle_image;

        lv_obj_t *state = lv_label_create(card);
        lv_label_set_text(state, it->state ? "ON" : "OFF");
        lv_obj_set_style_text_font(state, &lv_font_montserrat_24, 0);
        lv_obj_align(state, LV_ALIGN_BOTTOM_MID, 0, -42);
        s_cards[i].state = state;

        s_button_ctx[i][0].item = it;
        s_button_ctx[i][0].cmd = it->cmd_on;
        s_button_ctx[i][1].item = it;
        s_button_ctx[i][1].cmd = it->cmd_off;
        s_button_ctx[i][2].item = it;
        s_button_ctx[i][2].cmd = it->cmd_toggle;

        ui_control_refresh_item(it);
    }
    layout_visible_cards();
}

static void layout_visible_cards(void)
{
    if (s_control_parent == NULL) {
        return;
    }

    int total_w = (CONTROL_CARD_W * CONTROL_COLUMNS) + (CONTROL_CARD_GAP * (CONTROL_COLUMNS - 1));
    int start_x = (CONTROL_PAGE_W - total_w) / 2;
    int slot_count = 0;
    for (int i = 0; i < CONTROL_ITEM_COUNT; i++) {
        if (s_cards[i].card == NULL) {
            continue;
        }
        if (!control_item_configured(&g_control_items[i])) {
            lv_obj_add_flag(s_cards[i].card, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        slot_count = i + 1;
        lv_obj_set_pos(s_cards[i].card,
                       start_x + (i % CONTROL_COLUMNS) * (CONTROL_CARD_W + CONTROL_CARD_GAP),
                       CONTROL_CARD_TOP_Y + (i / CONTROL_COLUMNS) * (CONTROL_CARD_H + CONTROL_CARD_GAP));
        if (!g_control_items[i].visible) {
            control_card_set_blank(i, true);
        }
    }

    if (slot_count <= CONTROL_CARDS_PER_PAGE) {
        lv_obj_set_scroll_snap_y(s_control_parent, LV_SCROLL_SNAP_NONE);
        lv_obj_set_scroll_dir(s_control_parent, LV_DIR_NONE);
        lv_obj_clear_flag(s_control_parent, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_scroll_to_y(s_control_parent, 0, LV_ANIM_OFF);
    } else {
        lv_obj_add_flag(s_control_parent, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(s_control_parent, LV_DIR_VER);
        lv_obj_set_scroll_snap_y(s_control_parent, LV_SCROLL_SNAP_START);
    }
}
