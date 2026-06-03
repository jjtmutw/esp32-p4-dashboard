#include "lvgl.h"
#include "ui_sensor.h"

typedef struct { const char *name; const char *value; const char *unit; } sensor_demo_t;
static sensor_demo_t sensors[12] = {
    {"辦公室溫度", "28.3", "°C"}, {"辦公室濕度", "43.3", "%"}, {"辦公室 CO2", "1912", "ppm"},
    {"辦公室 TVOC", "5628", "ppb"}, {"電腦機房溫度", "22.8", "°C"}, {"電腦機房濕度", "49.3", "%"},
    {"實驗室溫度", "24.3", "°C"}, {"實驗室濕度", "65.0", "%"}, {"溫室溫度", "24.1", "°C"},
    {"溫室濕度", "59.7", "%"}, {"光照強度", "1280", "lux"}, {"土壤濕度", "32.6", "%"}
};

void ui_sensor_create(lv_obj_t *parent) {
    int w = 205, h = 125, gap = 12;
    for (int i = 0; i < 12; i++) {
        lv_obj_t *card = lv_obj_create(parent);
        lv_obj_set_size(card, w, h);
        lv_obj_set_pos(card, 10 + (i % 3) * (w + gap), 12 + (i / 3) * (h + gap));
        lv_obj_set_style_bg_color(card, lv_color_hex(0x0b2230), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x159faf), 0);
        lv_obj_set_style_radius(card, 12, 0);

        lv_obj_t *name = lv_label_create(card);
        lv_label_set_text_fmt(name, "%02d  %s", i + 1, sensors[i].name);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, 6, 4);

        lv_obj_t *val = lv_label_create(card);
        lv_label_set_text_fmt(val, "%s", sensors[i].value);
        lv_obj_set_style_text_font(val, &lv_font_montserrat_30, 0);
        lv_obj_align(val, LV_ALIGN_LEFT_MID, 8, 6);

        lv_obj_t *unit = lv_label_create(card);
        lv_label_set_text(unit, sensors[i].unit);
        lv_obj_align(unit, LV_ALIGN_CENTER, 54, 8);

        lv_obj_t *line = lv_line_create(card);
        static lv_point_precise_t pts[6] = {{0,28},{28,18},{56,24},{84,10},{112,16},{145,6}};
        lv_line_set_points(line, pts, 6);
        lv_obj_set_style_line_color(line, lv_color_hex(0x21f6ff), 0);
        lv_obj_set_style_line_width(line, 2, 0);
        lv_obj_align(line, LV_ALIGN_BOTTOM_LEFT, 12, -8);
    }
}
