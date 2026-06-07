#include "lvgl.h"
#include "ui_main.h"
#include "ui_control.h"
#include "ui_sensor.h"
#include "ui_camera.h"
#include "ui_text_image.h"
#include "audio/button_feedback.h"
#include "app_config/device_config.h"
#include "setup/setup_manager.h"
#include "mqtt/mqtt_client_app.h"
#include "board/dashboard_board.h"
#include "esp_log.h"

#include <stdio.h>

static const char *TAG = "ui_main";
static lv_style_t style_bg;
static lv_obj_t *s_title_label;
static lv_obj_t *s_title_image;
static lv_obj_t *s_wifi_icon;
static lv_obj_t *s_mqtt_icon;
static lv_obj_t *s_setup_modal;
static lv_obj_t *s_settings_status;
static lv_obj_t *s_screen_power_icon;
static lv_obj_t *s_screen_wake_layer;
static uint32_t s_json_update_press_count;
static bool s_screen_on = true;

static void setup_close_event_cb(lv_event_t *e);
static void settings_request_config_event_cb(lv_event_t *e);
static void settings_start_ap_event_cb(lv_event_t *e);
static void screen_power_event_cb(lv_event_t *e);
static void screen_wake_event_cb(lv_event_t *e);

static void init_styles(void)
{
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, lv_color_hex(0x06121d));
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);
    lv_style_set_text_color(&style_bg, lv_color_hex(0xe9fbff));
}

static void settings_set_status(const char *text, uint32_t color)
{
    if (s_settings_status == NULL || text == NULL) {
        return;
    }
    lv_label_set_text(s_settings_status, text);
    lv_obj_set_style_text_color(s_settings_status, lv_color_hex(color), 0);
}

static void settings_set_setup_ap_status(void)
{
    char status[128];
    snprintf(status, sizeof(status), "Setup AP active: %s / 12345678 / http://192.168.4.1",
             setup_manager_setup_ap_ssid());
    settings_set_status(status, 0x46ff8a);
}

static lv_obj_t *settings_button_create(lv_obj_t *parent, const char *text, int width, lv_align_t align,
                                        int x, int y, uint32_t color, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, width, 44);
    lv_obj_align(btn, align, x, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_RELEASED, NULL);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    lv_obj_center(label);
    return btn;
}

static void setup_modal_open(bool ap_started)
{
    if (s_setup_modal != NULL) {
        lv_obj_del(s_setup_modal);
    }
    s_settings_status = NULL;

    s_setup_modal = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_setup_modal, 560, 350);
    lv_obj_center(s_setup_modal);
    lv_obj_set_style_bg_color(s_setup_modal, lv_color_hex(0x071824), 0);
    lv_obj_set_style_border_color(s_setup_modal, lv_color_hex(0x21f6ff), 0);
    lv_obj_set_style_border_width(s_setup_modal, 1, 0);
    lv_obj_set_style_radius(s_setup_modal, 10, 0);
    lv_obj_set_style_pad_all(s_setup_modal, 16, 0);
    lv_obj_clear_flag(s_setup_modal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_setup_modal);
    lv_label_set_text(title, "Device Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *summary = lv_label_create(s_setup_modal);
    lv_obj_set_width(summary, 520);
    lv_label_set_text_fmt(summary, "Device ID: %s\nConfig version: %lu\nMQTT: %s",
                          g_dashboard_device_id[0] != '\0' ? g_dashboard_device_id : "(not set)",
                          (unsigned long)g_dashboard_config_version,
                          setup_manager_mqtt_uri());
    lv_obj_set_style_text_font(summary, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(summary, lv_color_hex(0xe9fbff), 0);
    lv_obj_align(summary, LV_ALIGN_TOP_LEFT, 0, 48);

    lv_obj_t *hint = lv_label_create(s_setup_modal);
    lv_obj_set_width(hint, 520);
    lv_label_set_text(hint, "Update JSON asks MQTT config provider for the latest device config.");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x8cb9c8), 0);
    lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 0, 146);

    s_settings_status = lv_label_create(s_setup_modal);
    lv_obj_set_width(s_settings_status, 520);
    lv_obj_set_style_text_font(s_settings_status, &lv_font_montserrat_18, 0);
    lv_obj_align(s_settings_status, LV_ALIGN_TOP_LEFT, 0, 188);

    if (ap_started) {
        settings_set_setup_ap_status();
    } else if (mqtt_is_connected()) {
        settings_set_status("Ready. MQTT is online.", 0x46ff8a);
    } else {
        settings_set_status("MQTT is offline. Wait for MQTT before updating JSON.", 0xffcc66);
    }

    settings_button_create(s_setup_modal, "Update JSON", 164, LV_ALIGN_BOTTOM_LEFT, 0, 0, 0x0ab9b9,
                           settings_request_config_event_cb);
    settings_button_create(s_setup_modal, "Setup AP", 130, LV_ALIGN_BOTTOM_LEFT, 178, 0, 0x334a5f,
                           settings_start_ap_event_cb);
    settings_button_create(s_setup_modal, "Close", 110, LV_ALIGN_BOTTOM_RIGHT, 0, 0, 0x203344,
                           setup_close_event_cb);
}

static void setup_icon_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_RELEASED) {
        button_feedback_beep();
        setup_modal_open(false);
    }
}

static void screen_set_on(bool on)
{
    if (s_screen_on == on) {
        return;
    }

    s_screen_on = on;
    dashboard_board_set_screen_on(on);
    if (s_screen_power_icon != NULL) {
        lv_obj_set_style_text_color(s_screen_power_icon, lv_color_hex(on ? 0x46ff8a : 0x6f8794), 0);
    }
}

static void screen_wake_layer_create(void)
{
    if (s_screen_wake_layer != NULL) {
        return;
    }

    s_screen_wake_layer = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_screen_wake_layer);
    lv_obj_set_size(s_screen_wake_layer, LV_PCT(100), LV_PCT(100));
    lv_obj_center(s_screen_wake_layer);
    lv_obj_add_flag(s_screen_wake_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_screen_wake_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(s_screen_wake_layer, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(s_screen_wake_layer, screen_wake_event_cb, LV_EVENT_PRESSED, NULL);
}

static void screen_power_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    button_feedback_beep();
    screen_set_on(false);
    screen_wake_layer_create();
}

static void screen_wake_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    screen_set_on(true);
    if (s_screen_wake_layer != NULL) {
        lv_obj_del(s_screen_wake_layer);
        s_screen_wake_layer = NULL;
    }
}

static void setup_close_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (s_setup_modal != NULL) {
        lv_obj_del(s_setup_modal);
        s_setup_modal = NULL;
    }
    s_settings_status = NULL;
}

static void settings_request_config_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    button_feedback_beep();
    uint32_t press_count = ++s_json_update_press_count;
    bool sent = mqtt_request_config_update(true);
    if (sent) {
        char status[96];
        snprintf(status, sizeof(status), "Update JSON sent #%lu. Waiting for config.",
                 (unsigned long)press_count);
        settings_set_status(status, 0x46ff8a);
        ESP_LOGI(TAG, "Update JSON button sent request #%lu", (unsigned long)press_count);
    } else {
        settings_set_status("Cannot send request. MQTT or device ID is not ready.", 0xff6b6b);
        ESP_LOGW(TAG, "Update JSON button could not send request #%lu", (unsigned long)press_count);
    }
    lv_refr_now(NULL);
}

static void settings_start_ap_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    button_feedback_beep();
    setup_manager_force_portal();
    settings_set_setup_ap_status();
}

void ui_main_set_network_online(bool online)
{
    if (s_wifi_icon == NULL) {
        return;
    }

    lv_obj_set_style_text_color(s_wifi_icon, lv_color_hex(online ? 0x46ff8a : 0xff6b6b), 0);
    lv_obj_set_style_opa(s_wifi_icon, online ? LV_OPA_COVER : LV_OPA_50, 0);
}

void ui_main_set_mqtt_online(bool online)
{
    if (s_mqtt_icon == NULL) {
        return;
    }

    lv_obj_set_style_text_color(s_mqtt_icon, lv_color_hex(online ? 0x46ff8a : 0xff6b6b), 0);
    lv_obj_set_style_opa(s_mqtt_icon, online ? LV_OPA_COVER : LV_OPA_50, 0);
}

void ui_main_set_title(const char *title)
{
    if (s_title_label == NULL || title == NULL || title[0] == '\0') {
        return;
    }
    lv_label_set_text(s_title_label, title);
    lv_obj_align(s_title_label, LV_ALIGN_TOP_MID, 0, 10);
    ui_text_image_apply(s_title_image, s_title_label, &g_dashboard_title_image, LV_ALIGN_TOP_MID, 0, 10);
}

void ui_main_create(void)
{
    init_styles();
    lv_obj_t *scr = lv_scr_act();
    lv_obj_add_style(scr, &style_bg, 0);

    s_title_label = lv_label_create(scr);
    lv_label_set_text(s_title_label, g_dashboard_title);
    lv_obj_set_width(s_title_label, 430);
    lv_label_set_long_mode(s_title_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(s_title_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(s_title_label, lv_color_hex(0xffffff), 0);
    lv_obj_align(s_title_label, LV_ALIGN_TOP_MID, 0, 10);

    s_title_image = lv_image_create(scr);
    lv_obj_add_flag(s_title_image, LV_OBJ_FLAG_HIDDEN);
    ui_text_image_apply(s_title_image, s_title_label, &g_dashboard_title_image, LV_ALIGN_TOP_MID, 0, 10);

    s_screen_power_icon = lv_label_create(scr);
    lv_label_set_text(s_screen_power_icon, LV_SYMBOL_POWER);
    lv_obj_set_style_text_font(s_screen_power_icon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_screen_power_icon, lv_color_hex(0x46ff8a), 0);
    lv_obj_align(s_screen_power_icon, LV_ALIGN_TOP_LEFT, 18, 10);
    lv_obj_add_flag(s_screen_power_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_screen_power_icon, screen_power_event_cb, LV_EVENT_RELEASED, NULL);

    s_wifi_icon = lv_label_create(scr);
    lv_label_set_text(s_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(s_wifi_icon, &lv_font_montserrat_24, 0);
    lv_obj_align(s_wifi_icon, LV_ALIGN_TOP_RIGHT, -112, 10);
    ui_main_set_network_online(false);

    s_mqtt_icon = lv_label_create(scr);
    lv_label_set_text(s_mqtt_icon, "MQTT");
    lv_obj_set_style_text_font(s_mqtt_icon, &lv_font_montserrat_18, 0);
    lv_obj_align(s_mqtt_icon, LV_ALIGN_TOP_RIGHT, -56, 13);
    ui_main_set_mqtt_online(false);

    lv_obj_t *setup_icon = lv_label_create(scr);
    lv_label_set_text(setup_icon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(setup_icon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(setup_icon, lv_color_hex(0xe9fbff), 0);
    lv_obj_align(setup_icon, LV_ALIGN_TOP_RIGHT, -18, 10);
    lv_obj_add_flag(setup_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(setup_icon, setup_icon_event_cb, LV_EVENT_RELEASED, NULL);

    lv_obj_t *tabs = lv_tabview_create(scr);
    lv_tabview_set_tab_bar_position(tabs, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tabs, 44);
    lv_obj_set_size(tabs, 704, 660);
    lv_obj_align(tabs, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_text_font(tabs, &lv_font_montserrat_20, 0);

    lv_obj_t *tab1 = lv_tabview_add_tab(tabs, "  Controls  ");
    lv_obj_t *tab2 = lv_tabview_add_tab(tabs, "  Sensors  ");
    lv_obj_t *tab3 = lv_tabview_add_tab(tabs, "  Cameras  ");

    ui_control_create(tab1);
    ui_sensor_create(tab2);
    ui_camera_create(tab3);
}
