#pragma once
#include "lvgl.h"

void ui_sensor_create(lv_obj_t *parent);
void ui_sensor_update_climate(const char *device_id, double temperature, double humidity);
void ui_sensor_update_value(const char *topic, const char *device_id, const char *key, const char *value);
void ui_sensor_refresh_all(void);
