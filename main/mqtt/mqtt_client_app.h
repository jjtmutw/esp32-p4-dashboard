#pragma once
#include "app_config/device_config.h"
void mqtt_app_start(void);
void mqtt_publish_control(control_item_t *item, bool on);
