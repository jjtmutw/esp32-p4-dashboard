#pragma once
#include <stdbool.h>

void ui_main_create(void);
void ui_main_set_title(const char *title);
void ui_main_refresh_background(void);
void ui_main_set_network_online(bool online);
void ui_main_set_mqtt_online(bool online);
