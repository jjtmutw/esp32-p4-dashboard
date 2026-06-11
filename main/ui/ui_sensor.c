#include "ui_sensor.h"

#include <stdio.h>
#include <string.h>

#include "app_config/device_config.h"
#include "esp_log.h"
#include "lvgl.h"
#include "ui_text_image.h"

typedef struct {
    lv_obj_t *card;
    lv_obj_t *name_label;
    lv_obj_t *name_image;
    lv_obj_t *value_label;
    lv_obj_t *unit_label;
    lv_obj_t *unit_image;
    lv_obj_t *line;
} sensor_card_t;

static sensor_card_t s_cards[SENSOR_ITEM_COUNT];
static const char *TAG = "ui_sensor";
static lv_obj_t *s_sensor_parent;

#define SENSOR_CARD_W 218
#define SENSOR_CARD_H 188
#define SENSOR_CARD_GAP 12
#define SENSOR_CARD_TOP_Y 10
#define SENSOR_COLUMNS 3
#define SENSOR_CARDS_PER_PAGE 9
#define SENSOR_PAGE_W 704

static void layout_visible_cards(void);
static bool sensor_item_configured(const sensor_item_t *item);
static void sensor_card_set_blank(int i, bool blank);

static bool sensor_matches(const sensor_item_t *item, const char *topic, const char *device_id, const char *key)
{
    if (item == NULL || topic == NULL || key == NULL || item->topic[0] == '\0' || item->key[0] == '\0') {
        return false;
    }
    if (strcmp(item->topic, topic) != 0 || strcmp(item->key, key) != 0) {
        return false;
    }
    return item->device_id[0] == '\0' || (device_id != NULL && strcmp(item->device_id, device_id) == 0);
}

static bool sensor_item_configured(const sensor_item_t *item)
{
    return item != NULL &&
           (item->label[0] != '\0' || item->topic[0] != '\0' ||
            item->key[0] != '\0' || item->device_id[0] != '\0');
}

static void sensor_card_set_blank(int i, bool blank)
{
    if (i < 0 || i >= SENSOR_ITEM_COUNT || s_cards[i].card == NULL) {
        return;
    }

    lv_obj_clear_flag(s_cards[i].card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(s_cards[i].card, blank ? LV_OPA_TRANSP : LV_OPA_80, 0);
    lv_obj_set_style_border_opa(s_cards[i].card, blank ? LV_OPA_TRANSP : LV_OPA_COVER, 0);

    lv_obj_t *children[] = {
        s_cards[i].name_label,
        s_cards[i].name_image,
        s_cards[i].value_label,
        s_cards[i].unit_label,
        s_cards[i].unit_image,
        s_cards[i].line,
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

static void refresh_sensor_card(int i)
{
    if (i < 0 || i >= SENSOR_ITEM_COUNT || s_cards[i].card == NULL) {
        return;
    }

    sensor_item_t *item = &g_sensor_items[i];
    if (!sensor_item_configured(item)) {
        lv_obj_add_flag(s_cards[i].card, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (!item->visible) {
        sensor_card_set_blank(i, true);
        return;
    }
    sensor_card_set_blank(i, false);
    lv_label_set_text_fmt(s_cards[i].name_label, "%02d  %s", i + 1, item->label);
    lv_label_set_text(s_cards[i].value_label, item->value[0] != '\0' ? item->value : "--");
    lv_label_set_text(s_cards[i].unit_label, item->unit);
    ui_text_image_apply(s_cards[i].name_image, s_cards[i].name_label, &item->label_image, LV_ALIGN_TOP_LEFT, 6, 4);
    ui_text_image_apply(s_cards[i].unit_image, s_cards[i].unit_label, &item->unit_image, LV_ALIGN_CENTER, 70, 20);
}

void ui_sensor_update_value(const char *topic, const char *device_id, const char *key, const char *value)
{
    if (value == NULL) {
        return;
    }
    bool matched = false;
    for (int i = 0; i < SENSOR_ITEM_COUNT; i++) {
        if (sensor_matches(&g_sensor_items[i], topic, device_id, key)) {
            strlcpy(g_sensor_items[i].value, value, sizeof(g_sensor_items[i].value));
            refresh_sensor_card(i);
            matched = true;
            ESP_LOGI(TAG, "sensor update idx=%d topic=%s device_id=%s key=%s value=%s",
                     i, topic, device_id != NULL ? device_id : "", key, value);
        }
    }
    if (!matched) {
        ESP_LOGW(TAG, "sensor update no match topic=%s device_id=%s key=%s value=%s",
                 topic != NULL ? topic : "", device_id != NULL ? device_id : "",
                 key != NULL ? key : "", value);
    }
}

void ui_sensor_update_climate(const char *device_id, double temperature, double humidity)
{
    char value[16];
    snprintf(value, sizeof(value), "%.1f", temperature);
    ui_sensor_update_value("jj/sensor/state", device_id, "temperature", value);
    snprintf(value, sizeof(value), "%.1f", humidity);
    ui_sensor_update_value("jj/sensor/state", device_id, "humidity", value);
}

void ui_sensor_refresh_all(void)
{
    for (int i = 0; i < SENSOR_ITEM_COUNT; i++) {
        refresh_sensor_card(i);
    }
    layout_visible_cards();
}

void ui_sensor_create(lv_obj_t *parent)
{
    int total_w = (SENSOR_CARD_W * SENSOR_COLUMNS) + (SENSOR_CARD_GAP * (SENSOR_COLUMNS - 1));
    int start_x = (SENSOR_PAGE_W - total_w) / 2;
    s_sensor_parent = parent;

    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_pad_row(parent, 0, 0);
    lv_obj_set_style_pad_column(parent, 0, 0);
    lv_obj_set_scroll_dir(parent, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(parent, LV_SCROLL_SNAP_START);
    lv_obj_scroll_to_x(parent, 0, LV_ANIM_OFF);

    for (int i = 0; i < SENSOR_ITEM_COUNT; i++) {
        lv_obj_t *card = lv_obj_create(parent);
        lv_obj_set_size(card, SENSOR_CARD_W, SENSOR_CARD_H);
        lv_obj_set_pos(card, start_x + (i % SENSOR_COLUMNS) * (SENSOR_CARD_W + SENSOR_CARD_GAP),
                       SENSOR_CARD_TOP_Y + (i / SENSOR_COLUMNS) * (SENSOR_CARD_H + SENSOR_CARD_GAP));
        lv_obj_set_style_bg_color(card, lv_color_hex(0x0b2230), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_80, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x159faf), 0);
        lv_obj_set_style_radius(card, 8, 0);
        s_cards[i].card = card;

        lv_obj_t *name = lv_label_create(card);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_width(name, 204);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_18, 0);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, 6, 4);
        s_cards[i].name_label = name;

        lv_obj_t *name_image = lv_image_create(card);
        lv_obj_add_flag(name_image, LV_OBJ_FLAG_HIDDEN);
        s_cards[i].name_image = name_image;

        lv_obj_t *val = lv_label_create(card);
        lv_obj_set_width(val, 145);
        lv_label_set_long_mode(val, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(val, &lv_font_montserrat_40, 0);
        lv_obj_set_style_text_color(val, lv_color_hex(0xf4fbff), 0);
        lv_obj_align(val, LV_ALIGN_CENTER, -30, 12);
        s_cards[i].value_label = val;

        lv_obj_t *unit = lv_label_create(card);
        lv_obj_set_style_text_font(unit, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(unit, lv_color_hex(0x9eeff7), 0);
        lv_obj_align(unit, LV_ALIGN_CENTER, 70, 20);
        s_cards[i].unit_label = unit;

        lv_obj_t *unit_image = lv_image_create(card);
        lv_obj_add_flag(unit_image, LV_OBJ_FLAG_HIDDEN);
        s_cards[i].unit_image = unit_image;

        lv_obj_t *line = lv_line_create(card);
        static lv_point_precise_t pts[6] = {{0, 28}, {28, 18}, {56, 24}, {84, 10}, {112, 16}, {145, 6}};
        lv_line_set_points(line, pts, 6);
        lv_obj_set_style_line_color(line, lv_color_hex(0x21f6ff), 0);
        lv_obj_set_style_line_width(line, 2, 0);
        lv_obj_align(line, LV_ALIGN_BOTTOM_LEFT, 12, -8);
        s_cards[i].line = line;

        refresh_sensor_card(i);
    }
    layout_visible_cards();
}

static void layout_visible_cards(void)
{
    if (s_sensor_parent == NULL) {
        return;
    }

    int total_w = (SENSOR_CARD_W * SENSOR_COLUMNS) + (SENSOR_CARD_GAP * (SENSOR_COLUMNS - 1));
    int start_x = (SENSOR_PAGE_W - total_w) / 2;
    int slot_count = 0;
    for (int i = 0; i < SENSOR_ITEM_COUNT; i++) {
        if (s_cards[i].card == NULL) {
            continue;
        }
        if (!sensor_item_configured(&g_sensor_items[i])) {
            lv_obj_add_flag(s_cards[i].card, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        slot_count = i + 1;
        lv_obj_set_pos(s_cards[i].card,
                       start_x + (i % SENSOR_COLUMNS) * (SENSOR_CARD_W + SENSOR_CARD_GAP),
                       SENSOR_CARD_TOP_Y + (i / SENSOR_COLUMNS) * (SENSOR_CARD_H + SENSOR_CARD_GAP));
        if (!g_sensor_items[i].visible) {
            sensor_card_set_blank(i, true);
        }
    }

    if (slot_count <= SENSOR_CARDS_PER_PAGE) {
        lv_obj_set_scroll_snap_y(s_sensor_parent, LV_SCROLL_SNAP_NONE);
        lv_obj_set_scroll_dir(s_sensor_parent, LV_DIR_NONE);
        lv_obj_clear_flag(s_sensor_parent, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_scroll_to_y(s_sensor_parent, 0, LV_ANIM_OFF);
    } else {
        lv_obj_add_flag(s_sensor_parent, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(s_sensor_parent, LV_DIR_VER);
        lv_obj_set_scroll_snap_y(s_sensor_parent, LV_SCROLL_SNAP_START);
    }
}
