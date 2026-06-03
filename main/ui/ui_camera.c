#include "lvgl.h"
#include "ui_camera.h"

static const char *cams[4] = {"辦公室相機", "實驗室相機", "機房相機", "植物室相機"};

void ui_camera_create(lv_obj_t *parent) {
    int w = 312, h = 238, gap = 14;
    for (int i = 0; i < 4; i++) {
        lv_obj_t *card = lv_obj_create(parent);
        lv_obj_set_size(card, w, h);
        lv_obj_set_pos(card, 10 + (i % 2) * (w + gap), 12 + (i / 2) * (h + gap));
        lv_obj_set_style_bg_color(card, lv_color_hex(0x0b2230), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x159faf), 0);
        lv_obj_set_style_radius(card, 12, 0);

        lv_obj_t *title = lv_label_create(card);
        lv_label_set_text_fmt(title, "%02d  %s", i + 1, cams[i]);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 4);
        lv_obj_t *live = lv_label_create(card);
        lv_label_set_text(live, "● LIVE");
        lv_obj_set_style_text_color(live, lv_color_hex(0x46ff8a), 0);
        lv_obj_align(live, LV_ALIGN_TOP_RIGHT, -8, 4);

        lv_obj_t *video = lv_obj_create(card);
        lv_obj_set_size(video, 286, 155);
        lv_obj_align(video, LV_ALIGN_CENTER, 0, -10);
        lv_obj_set_style_bg_color(video, i == 2 ? lv_color_hex(0x071c35) : lv_color_hex(0x323b3f), 0);
        lv_obj_set_style_radius(video, 8, 0);
        lv_obj_t *placeholder = lv_label_create(video);
        lv_label_set_text(placeholder, "CAMERA STREAM\nHTTP JPEG / MJPEG");
        lv_obj_center(placeholder);

        lv_obj_t *ops = lv_label_create(card);
        lv_label_set_text(ops, "📷 擷取    ▣ 錄影    🔊 聲音    ⚙ 設定");
        lv_obj_align(ops, LV_ALIGN_BOTTOM_MID, 0, -4);
    }
}
