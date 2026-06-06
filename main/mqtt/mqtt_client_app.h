#pragma once
#include "app_config/device_config.h"

void mqtt_app_start(void);
bool mqtt_is_connected(void);
bool mqtt_request_config_update(bool force);
void mqtt_publish_control(control_item_t *item, const char *cmd);
void mqtt_publish_camera_command(const char *topic, const char *payload);
void mqtt_handle_control_state_json(const char *json, int len);
