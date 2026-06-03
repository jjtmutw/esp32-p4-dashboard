#include "lvgl.h"
#include "ui_main.h"
#include "ui_control.h"
#include "ui_sensor.h"
#include "ui_camera.h"

static lv_style_t style_bg, style_panel, style_tab_active;

static void init_styles(void) {
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, lv_color_hex(0x06121d));
    lv_style_set_text_color(&style_bg, lv_color_hex(0xe9fbff));

    lv_style_init(&style_panel);
    lv_style_set_bg_color(&style_panel, lv_color_hex(0x0b2230));
    lv_style_set_bg_opa(&style_panel, LV_OPA_90);
    lv_style_set_border_color(&style_panel, lv_color_hex(0x18d6d6));
    lv_style_set_border_width(&style_panel, 1);
    lv_style_set_radius(&style_panel, 12);
    lv_style_set_pad_all(&style_panel, 10);

    lv_style_init(&style_tab_active);
    lv_style_set_bg_color(&style_tab_active, lv_color_hex(0x0bb7b7));
    lv_style_set_bg_grad_color(&style_tab_active, lv_color_hex(0x0b3348));
    lv_style_set_bg_grad_dir(&style_tab_active, LV_GRAD_DIR_HOR);
    lv_style_set_text_color(&style_tab_active, lv_color_white());
}

void ui_main_create(void) {
    init_styles();
    lv_obj_t *scr = lv_scr_act();
    lv_obj_add_style(scr, &style_bg, 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "JJ辦公室控制器");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_30, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    lv_obj_t *status = lv_label_create(scr);
    lv_label_set_text(status, "● ONLINE  |  2026/06/03 10:21:47");
    lv_obj_set_style_text_color(status, lv_color_hex(0x46ff8a), 0);
    lv_obj_align(status, LV_ALIGN_TOP_RIGHT, -28, 28);

    lv_obj_t *tabs = lv_tabview_create(scr, LV_DIR_TOP, 58);
    lv_obj_set_size(tabs, 684, 610);
    lv_obj_align(tabs, LV_ALIGN_BOTTOM_MID, 0, -18);

    lv_obj_t *tab1 = lv_tabview_add_tab(tabs, "  控制  ");
    lv_obj_t *tab2 = lv_tabview_add_tab(tabs, "  感測器  ");
    lv_obj_t *tab3 = lv_tabview_add_tab(tabs, "  影像監控  ");

    ui_control_create(tab1);
    ui_sensor_create(tab2);
    ui_camera_create(tab3);
}
