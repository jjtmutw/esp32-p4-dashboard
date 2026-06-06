#include "nvs_config.h"
#include <stdlib.h>
#include <string.h>
#include "app_config/device_config.h"
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "nvs.h"

#define DASHBOARD_NVS_NAMESPACE "dashboard"
#define DASHBOARD_CONTROLS_KEY "controls_json"
#define DASHBOARD_CONFIG_KEY "config_json"
#define DASHBOARD_DEVICE_ID_KEY "device_id"
#define DASHBOARD_WIFI_SSID_KEY "wifi_ssid"
#define DASHBOARD_WIFI_PASSWORD_KEY "wifi_pass"
#define DASHBOARD_MQTT_URI_KEY "mqtt_uri"
#define DASHBOARD_HTTP_URL_KEY "http_url"
#define DASHBOARD_OLD_DEFAULT_MQTT_URI "mqtt://192.168.1.10:1883"

static const char *TAG = "nvs_config";

static void *dashboard_large_calloc(size_t size)
{
    void *ptr = heap_caps_calloc(1, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr == NULL) {
        ptr = calloc(1, size);
    }
    return ptr;
}

esp_err_t nvs_config_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

static bool is_valid_device_id(const char *device_id)
{
    if (device_id == NULL || strlen(device_id) != DASHBOARD_DEVICE_ID_LEN) {
        return false;
    }
    for (int i = 0; i < DASHBOARD_DEVICE_ID_LEN; i++) {
        char c = device_id[i];
        bool ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        if (!ok) {
            return false;
        }
    }
    return true;
}

static void make_device_id(char *device_id, size_t device_id_size)
{
    static const char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    if (device_id == NULL || device_id_size < DASHBOARD_DEVICE_ID_SIZE) {
        return;
    }
    for (int i = 0; i < DASHBOARD_DEVICE_ID_LEN; i++) {
        device_id[i] = alphabet[esp_random() % (sizeof(alphabet) - 1)];
    }
    device_id[DASHBOARD_DEVICE_ID_LEN] = '\0';
}

void nvs_config_load_device_id(char *device_id, size_t device_id_size)
{
    if (device_id == NULL || device_id_size < DASHBOARD_DEVICE_ID_SIZE) {
        return;
    }

    device_id[0] = '\0';
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(DASHBOARD_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        size_t len = device_id_size;
        err = nvs_get_str(handle, DASHBOARD_DEVICE_ID_KEY, device_id, &len);
        if (err != ESP_OK || !is_valid_device_id(device_id)) {
            make_device_id(device_id, device_id_size);
            err = nvs_set_str(handle, DASHBOARD_DEVICE_ID_KEY, device_id);
            if (err == ESP_OK) {
                err = nvs_commit(handle);
            }
            ESP_LOGI(TAG, "generated dashboard device id: %s", device_id);
        } else {
            ESP_LOGI(TAG, "loaded dashboard device id: %s", device_id);
        }
        nvs_close(handle);
    }

    if (!is_valid_device_id(device_id)) {
        make_device_id(device_id, device_id_size);
    }
}

void nvs_config_load_controls(void) {
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(DASHBOARD_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "using built-in control defaults");
        return;
    }

    size_t json_len = 0;
    bool stored_as_blob = true;
    err = nvs_get_blob(handle, DASHBOARD_CONTROLS_KEY, NULL, &json_len);
    if (err == ESP_ERR_NVS_NOT_FOUND || err == ESP_ERR_NVS_TYPE_MISMATCH) {
        stored_as_blob = false;
        err = nvs_get_str(handle, DASHBOARD_CONTROLS_KEY, NULL, &json_len);
    }
    if (err != ESP_OK || json_len == 0) {
        nvs_close(handle);
        return;
    }

    char *json = dashboard_large_calloc(json_len + 1);
    if (json == NULL) {
        nvs_close(handle);
        return;
    }

    if (stored_as_blob) {
        err = nvs_get_blob(handle, DASHBOARD_CONTROLS_KEY, json, &json_len);
        json[json_len] = '\0';
    } else {
        err = nvs_get_str(handle, DASHBOARD_CONTROLS_KEY, json, &json_len);
    }
    if (err == ESP_OK && control_items_apply_json(json)) {
        ESP_LOGI(TAG, "loaded control JSON from NVS");
    } else {
        ESP_LOGW(TAG, "stored control JSON is invalid");
    }

    free(json);
    nvs_close(handle);
}

void nvs_config_save_controls(void) {
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(DASHBOARD_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_erase_key(handle, DASHBOARD_CONTROLS_KEY);
        err = nvs_commit(handle);
        nvs_close(handle);
    }

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to erase legacy control JSON: %s", esp_err_to_name(err));
    }
    nvs_config_save_dashboard();
}

void nvs_config_load_dashboard(void)
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(DASHBOARD_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "using built-in dashboard config defaults");
        return;
    }

    size_t json_len = 0;
    bool stored_as_blob = true;
    err = nvs_get_blob(handle, DASHBOARD_CONFIG_KEY, NULL, &json_len);
    if (err == ESP_ERR_NVS_NOT_FOUND || err == ESP_ERR_NVS_TYPE_MISMATCH) {
        stored_as_blob = false;
        err = nvs_get_str(handle, DASHBOARD_CONFIG_KEY, NULL, &json_len);
    }
    if (err != ESP_OK || json_len == 0) {
        nvs_close(handle);
        nvs_config_load_controls();
        return;
    }

    char *json = dashboard_large_calloc(json_len + 1);
    if (json == NULL) {
        nvs_close(handle);
        return;
    }

    if (stored_as_blob) {
        err = nvs_get_blob(handle, DASHBOARD_CONFIG_KEY, json, &json_len);
        json[json_len] = '\0';
    } else {
        err = nvs_get_str(handle, DASHBOARD_CONFIG_KEY, json, &json_len);
    }
    if (err == ESP_OK && dashboard_config_apply_json(json, false)) {
        ESP_LOGI(TAG, "loaded dashboard config JSON from NVS");
    } else {
        ESP_LOGW(TAG, "stored dashboard config JSON is invalid");
    }

    free(json);
    nvs_close(handle);
}

void nvs_config_save_dashboard(void)
{
    char *json = dashboard_config_to_json_string();
    if (json == NULL) {
        return;
    }

    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(DASHBOARD_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_erase_key(handle, DASHBOARD_CONTROLS_KEY);
        nvs_erase_key(handle, DASHBOARD_CONFIG_KEY);
        err = nvs_set_blob(handle, DASHBOARD_CONFIG_KEY, json, strlen(json) + 1);
        if (err == ESP_OK) {
            err = nvs_commit(handle);
        }
        nvs_close(handle);
    }

    if (err == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
        dashboard_runtime_config_t runtime;
        nvs_config_load_runtime(&runtime);
        err = nvs_open(DASHBOARD_NVS_NAMESPACE, NVS_READWRITE, &handle);
        if (err == ESP_OK) {
            nvs_erase_all(handle);
            if (g_dashboard_device_id[0] != '\0') {
                err = nvs_set_str(handle, DASHBOARD_DEVICE_ID_KEY, g_dashboard_device_id);
            }
            if (err == ESP_OK && runtime.wifi_ssid[0] != '\0') {
                err = nvs_set_str(handle, DASHBOARD_WIFI_SSID_KEY, runtime.wifi_ssid);
            }
            if (err == ESP_OK && runtime.wifi_password[0] != '\0') {
                err = nvs_set_str(handle, DASHBOARD_WIFI_PASSWORD_KEY, runtime.wifi_password);
            }
            if (err == ESP_OK && runtime.mqtt_uri[0] != '\0') {
                err = nvs_set_str(handle, DASHBOARD_MQTT_URI_KEY, runtime.mqtt_uri);
            }
            if (err == ESP_OK && runtime.http_publish_url[0] != '\0') {
                err = nvs_set_str(handle, DASHBOARD_HTTP_URL_KEY, runtime.http_publish_url);
            }
            if (err == ESP_OK) {
                err = nvs_set_blob(handle, DASHBOARD_CONFIG_KEY, json, strlen(json) + 1);
            }
            if (err == ESP_OK) {
                err = nvs_commit(handle);
                ESP_LOGI(TAG, "compacted dashboard NVS namespace");
            }
            nvs_close(handle);
        }
    }

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to save dashboard config JSON: %s", esp_err_to_name(err));
    }
    cJSON_free(json);
}

static void nvs_get_str_or_default(nvs_handle_t handle, const char *key, char *dest, size_t dest_size, const char *fallback)
{
    if (dest == NULL || dest_size == 0) {
        return;
    }

    dest[0] = '\0';
    size_t len = dest_size;
    esp_err_t err = nvs_get_str(handle, key, dest, &len);
    if (err != ESP_OK && fallback != NULL) {
        strlcpy(dest, fallback, dest_size);
    }
}

void nvs_config_load_runtime(dashboard_runtime_config_t *config)
{
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));
    strlcpy(config->mqtt_uri, CONFIG_DASHBOARD_MQTT_BROKER_URI, sizeof(config->mqtt_uri));
    strlcpy(config->http_publish_url, CONFIG_DASHBOARD_HTTP_PUBLISH_URL, sizeof(config->http_publish_url));

    nvs_handle_t handle;
    esp_err_t err = nvs_open(DASHBOARD_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return;
    }

    nvs_get_str_or_default(handle, DASHBOARD_WIFI_SSID_KEY, config->wifi_ssid, sizeof(config->wifi_ssid), "");
    nvs_get_str_or_default(handle, DASHBOARD_WIFI_PASSWORD_KEY, config->wifi_password, sizeof(config->wifi_password), "");
    nvs_get_str_or_default(handle, DASHBOARD_MQTT_URI_KEY, config->mqtt_uri, sizeof(config->mqtt_uri), CONFIG_DASHBOARD_MQTT_BROKER_URI);
    nvs_get_str_or_default(handle, DASHBOARD_HTTP_URL_KEY, config->http_publish_url, sizeof(config->http_publish_url), CONFIG_DASHBOARD_HTTP_PUBLISH_URL);
    if (strcmp(config->mqtt_uri, DASHBOARD_OLD_DEFAULT_MQTT_URI) == 0) {
        strlcpy(config->mqtt_uri, CONFIG_DASHBOARD_MQTT_BROKER_URI, sizeof(config->mqtt_uri));
    }
    config->has_wifi = config->wifi_ssid[0] != '\0';
    nvs_close(handle);
}

void nvs_config_save_runtime(const dashboard_runtime_config_t *config)
{
    if (config == NULL) {
        return;
    }

    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(DASHBOARD_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, DASHBOARD_WIFI_SSID_KEY, config->wifi_ssid);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(handle, DASHBOARD_WIFI_PASSWORD_KEY, config->wifi_password);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(handle, DASHBOARD_MQTT_URI_KEY, config->mqtt_uri);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(handle, DASHBOARD_HTTP_URL_KEY, config->http_publish_url);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    if (handle) {
        nvs_close(handle);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to save runtime config: %s", esp_err_to_name(err));
    }
}
