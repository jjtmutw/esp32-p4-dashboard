#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

typedef struct {
    char wifi_ssid[33];
    char wifi_password[65];
    char mqtt_uri[128];
    char http_publish_url[160];
    bool has_wifi;
} dashboard_runtime_config_t;

esp_err_t nvs_config_init(void);
void nvs_config_load_device_id(char *device_id, size_t device_id_size);
void nvs_config_load_controls(void);
void nvs_config_save_controls(void);
void nvs_config_load_dashboard(void);
void nvs_config_save_dashboard(void);
void nvs_config_load_runtime(dashboard_runtime_config_t *config);
void nvs_config_save_runtime(const dashboard_runtime_config_t *config);
