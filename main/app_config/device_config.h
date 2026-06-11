#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"
#include "lvgl.h"

#define DASHBOARD_DEVICE_ID_LEN 8
#define DASHBOARD_DEVICE_ID_SIZE (DASHBOARD_DEVICE_ID_LEN + 1)
#define DASHBOARD_TITLE_SIZE 64
#define CONTROL_URL_SIZE 256
#define CONTROL_ITEM_COUNT 12
#define SENSOR_ITEM_COUNT 12
#define CAMERA_ITEM_COUNT 4
#define TEXT_IMAGE_FORMAT_SIZE 12
#define DASHBOARD_BACKGROUND_FORMAT_SIZE 12
#define DASHBOARD_BACKGROUND_MIME_SIZE 24

typedef struct {
    bool valid;
    uint16_t width;
    uint16_t height;
    uint32_t color;
    char format[TEXT_IMAGE_FORMAT_SIZE];
    char *base64;
    uint8_t *data;
    lv_image_dsc_t dsc;
} dashboard_text_image_t;

typedef struct {
    bool valid;
    uint16_t width;
    uint16_t height;
    char format[DASHBOARD_BACKGROUND_FORMAT_SIZE];
    char mime[DASHBOARD_BACKGROUND_MIME_SIZE];
    char *base64;
    uint8_t *data;
    size_t data_size;
    lv_image_dsc_t dsc;
} dashboard_background_image_t;

typedef struct {
    char id[32];
    char name[32];
    char subtitle[32];
    char icon[16];
    char action[16];
    char mqtt_topic[64];
    char device[32];
    char sw[16];
    char cmd_on[16];
    char cmd_off[16];
    char cmd_toggle[16];
    char url[CONTROL_URL_SIZE];
    char url_on[CONTROL_URL_SIZE];
    char url_off[CONTROL_URL_SIZE];
    bool confirm;
    int auto_off_sec;
    bool visible;
    bool state;
    dashboard_text_image_t name_image;
    dashboard_text_image_t subtitle_image;
} control_item_t;

typedef struct {
    char label[32];
    char unit[16];
    char topic[64];
    char key[32];
    char device_id[16];
    char value[16];
    bool visible;
    dashboard_text_image_t label_image;
    dashboard_text_image_t unit_image;
} sensor_item_t;

typedef struct {
    char label[32];
    char device_id[16];
    char topic[64];
    char url[128];
    bool visible;
    dashboard_text_image_t label_image;
} camera_item_t;

extern control_item_t g_control_items[CONTROL_ITEM_COUNT];
extern sensor_item_t g_sensor_items[SENSOR_ITEM_COUNT];
extern camera_item_t g_camera_items[CAMERA_ITEM_COUNT];
extern char g_dashboard_title[DASHBOARD_TITLE_SIZE];
extern dashboard_text_image_t g_dashboard_title_image;
extern dashboard_background_image_t g_dashboard_background_image;
extern char g_dashboard_device_id[DASHBOARD_DEVICE_ID_SIZE];
extern uint32_t g_dashboard_config_version;

void dashboard_config_set_device_id(const char *device_id);
control_item_t *control_item_find_by_id(const char *id);
control_item_t *control_item_find_by_device_switch(const char *device, const char *sw);
char *control_item_to_json_string(const control_item_t *item);
bool control_item_apply_json(control_item_t *item, const char *json);
char *control_items_to_json_string(void);
bool control_items_apply_json(const char *json);
char *dashboard_config_to_json_string(void);
bool dashboard_config_apply_json(const char *json, bool require_matching_device);
void dashboard_text_image_free(dashboard_text_image_t *image);
void dashboard_background_image_free(dashboard_background_image_t *image);
