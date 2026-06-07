#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t setup_manager_start(void);
void setup_manager_force_portal(void);
bool setup_manager_is_online(void);
const char *setup_manager_mqtt_uri(void);
const char *setup_manager_http_publish_url(void);
const char *setup_manager_setup_ap_ssid(void);
