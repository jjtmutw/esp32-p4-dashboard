#pragma once

#include <stdbool.h>
#include "lvgl.h"

#define CAMERA_CARD_COUNT 4

typedef struct {
    const char *name;
    const char *url;
} camera_stream_config_t;

void camera_stream_attach_card(int index, lv_obj_t *preview, lv_obj_t *image_obj, lv_obj_t *status_label,
                               lv_obj_t *placeholder_label);
void camera_stream_start_all(void);
void camera_stream_set_url(int index, const char *url);
void camera_stream_set_device_id(int index, const char *device_id);
bool camera_stream_is_online(int index);
void camera_stream_handle_mqtt_image(const char *device_id, const char *mime, int width, int height,
                                     double motion_score, const char *image_base64);
