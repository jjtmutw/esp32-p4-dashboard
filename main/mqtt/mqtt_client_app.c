#include "mqtt_client_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board/dashboard_board.h"
#include "camera/camera_stream.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "storage/nvs_config.h"
#include "setup/setup_manager.h"
#include "ui_camera.h"
#include "ui_control.h"
#include "ui_main.h"
#include "ui_sensor.h"

#define MQTT_BROKER_URI CONFIG_DASHBOARD_MQTT_BROKER_URI
#define MQTT_STATE_TOPIC CONFIG_DASHBOARD_MQTT_STATE_TOPIC
#define MQTT_CAMERA_TOPIC CONFIG_DASHBOARD_MQTT_CAMERA_TOPIC
#define MQTT_SENSOR_TOPIC CONFIG_DASHBOARD_MQTT_SENSOR_TOPIC
#define MQTT_CONFIG_REQUEST_TOPIC CONFIG_DASHBOARD_CONFIG_REQUEST_TOPIC
#define MQTT_CONFIG_RESPONSE_PREFIX CONFIG_DASHBOARD_CONFIG_RESPONSE_PREFIX
#define MQTT_CONFIG_ALL_TOPIC CONFIG_DASHBOARD_CONFIG_ALL_TOPIC
#define HTTP_PUBLISH_URL CONFIG_DASHBOARD_HTTP_PUBLISH_URL

static const char *TAG = "mqtt_app";
static esp_mqtt_client_handle_t s_client;
static char *s_mqtt_rx_payload;
static int s_mqtt_rx_total;
static char s_mqtt_rx_topic[128];
static char s_mqtt_client_id[32];
static uint32_t s_config_request_seq;
static bool s_mqtt_connected;

typedef struct {
    char topic[64];
    char payload[128];
    char direct_url[384];
} http_publish_request_t;

static void http_publish_task(void *arg);

static void *mqtt_large_calloc(size_t size)
{
    void *ptr = heap_caps_calloc(1, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr == NULL) {
        ptr = calloc(1, size);
    }
    return ptr;
}

static void url_encode(const char *src, char *dst, size_t dst_size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t out = 0;

    for (size_t i = 0; src[i] != '\0' && out + 1 < dst_size; i++) {
        unsigned char c = (unsigned char)src[i];
        bool plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                     (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';

        if (plain) {
            dst[out++] = (char)c;
        } else if (out + 3 < dst_size) {
            dst[out++] = '%';
            dst[out++] = hex[c >> 4];
            dst[out++] = hex[c & 0x0f];
        } else {
            break;
        }
    }

    dst[out] = '\0';
}

static void sanitize_direct_url(const char *src, char *dst, size_t dst_size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t out = 0;

    for (size_t i = 0; src != NULL && src[i] != '\0' && out + 1 < dst_size; i++) {
        unsigned char c = (unsigned char)src[i];
        bool encode = c <= ' ' || c == '"' || c == '<' || c == '>' || c == '`' ||
                      c == '{' || c == '}' || c == '\\';

        if (!encode) {
            dst[out++] = (char)c;
        } else if (out + 3 < dst_size) {
            dst[out++] = '%';
            dst[out++] = hex[c >> 4];
            dst[out++] = hex[c & 0x0f];
        } else {
            break;
        }
    }

    dst[out] = '\0';
}

static bool topic_name_matches(const char *actual, const char *expected)
{
    return actual != NULL && expected != NULL && expected[0] != '\0' && strcmp(actual, expected) == 0;
}

static void make_config_response_topic(char *topic, size_t topic_size)
{
    snprintf(topic, topic_size, "%s/%s", MQTT_CONFIG_RESPONSE_PREFIX, g_dashboard_device_id);
}

static bool topic_matches_config_response(const char *topic)
{
    char response_topic[96];
    make_config_response_topic(response_topic, sizeof(response_topic));
    return topic_name_matches(topic, response_topic) || topic_name_matches(topic, MQTT_CONFIG_ALL_TOPIC);
}

static bool topic_matches_camera_config(const char *topic)
{
    if (topic_name_matches(topic, MQTT_CAMERA_TOPIC)) {
        return true;
    }
    for (int i = 0; i < CAMERA_ITEM_COUNT; i++) {
        if (topic_name_matches(topic, g_camera_items[i].topic)) {
            return true;
        }
    }
    return false;
}

static bool topic_matches_sensor_config(const char *topic)
{
    if (topic_name_matches(topic, MQTT_SENSOR_TOPIC)) {
        return true;
    }
    for (int i = 0; i < SENSOR_ITEM_COUNT; i++) {
        if (topic_name_matches(topic, g_sensor_items[i].topic)) {
            return true;
        }
    }
    return false;
}

static void mqtt_subscribe_if_set(const char *topic, int qos)
{
    if (s_client != NULL && topic != NULL && topic[0] != '\0') {
        esp_mqtt_client_subscribe(s_client, topic, qos);
    }
}

static void mqtt_subscribe_dashboard_topics(void)
{
    char response_topic[96];
    make_config_response_topic(response_topic, sizeof(response_topic));

    mqtt_subscribe_if_set(MQTT_STATE_TOPIC, 1);
    mqtt_subscribe_if_set(MQTT_CAMERA_TOPIC, 0);
    mqtt_subscribe_if_set(MQTT_SENSOR_TOPIC, 0);
    mqtt_subscribe_if_set(response_topic, 1);
    mqtt_subscribe_if_set(MQTT_CONFIG_ALL_TOPIC, 1);

    for (int i = 0; i < SENSOR_ITEM_COUNT; i++) {
        mqtt_subscribe_if_set(g_sensor_items[i].topic, 0);
    }
    for (int i = 0; i < CAMERA_ITEM_COUNT; i++) {
        mqtt_subscribe_if_set(g_camera_items[i].topic, 0);
    }
}

static const char *make_mqtt_client_id(void)
{
    const char *device_id = g_dashboard_device_id[0] != '\0' ? g_dashboard_device_id : "unknown";
    snprintf(s_mqtt_client_id, sizeof(s_mqtt_client_id), "jj-dashboard-%s", device_id);
    return s_mqtt_client_id;
}

bool mqtt_is_connected(void)
{
    return s_mqtt_connected;
}

bool mqtt_request_config_update(bool force)
{
    if (s_client == NULL || !s_mqtt_connected || g_dashboard_device_id[0] == '\0') {
        ESP_LOGW(TAG, "config request skipped: mqtt=%s connected=%s device_id=%s",
                 s_client != NULL ? "ready" : "not-ready",
                 s_mqtt_connected ? "yes" : "no", g_dashboard_device_id);
        return false;
    }

    char payload[128];
    uint32_t request_id = ++s_config_request_seq;
    snprintf(payload, sizeof(payload), "{\"device_id\":\"%s\",\"version\":%lu,\"force\":%s,\"request_id\":%lu}",
             g_dashboard_device_id, (unsigned long)g_dashboard_config_version,
             force ? "true" : "false", (unsigned long)request_id);
    int msg_id = esp_mqtt_client_publish(s_client, MQTT_CONFIG_REQUEST_TOPIC, payload, 0, 1, 0);
    ESP_LOGI(TAG, "requested config for device_id=%s version=%lu force=%s request_id=%lu msg_id=%d",
             g_dashboard_device_id, (unsigned long)g_dashboard_config_version,
             force ? "true" : "false", (unsigned long)request_id, msg_id);
    return msg_id >= 0;
}

static void apply_state_update(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        ESP_LOGW(TAG, "invalid MQTT JSON: %s", json);
        return;
    }

    cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "id");
    cJSON *device = cJSON_GetObjectItemCaseSensitive(root, "device");
    cJSON *sw = cJSON_GetObjectItemCaseSensitive(root, "switch");
    cJSON *state = cJSON_GetObjectItemCaseSensitive(root, "state");
    cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root, "cmd");

    control_item_t *item = NULL;
    if (cJSON_IsString(id)) {
        item = control_item_find_by_id(id->valuestring);
    }
    if (item == NULL && cJSON_IsString(device) && cJSON_IsString(sw)) {
        item = control_item_find_by_device_switch(device->valuestring, sw->valuestring);
    }

    if (item != NULL) {
        if (cJSON_IsBool(state)) {
            item->state = cJSON_IsTrue(state);
        } else if (cJSON_IsString(cmd)) {
            item->state = strcmp(cmd->valuestring, item->cmd_on) == 0;
        }

        if (dashboard_board_lock(100)) {
            ui_control_refresh_item(item);
            dashboard_board_unlock();
        }
        nvs_config_save_controls();
    }

    cJSON_Delete(root);
}

void mqtt_handle_control_state_json(const char *json, int len)
{
    if (json == NULL || len <= 0) {
        return;
    }

    char payload[256];
    int copy_len = len < (int)sizeof(payload) - 1 ? len : (int)sizeof(payload) - 1;
    memcpy(payload, json, copy_len);
    payload[copy_len] = '\0';
    apply_state_update(payload);
}

static void handle_camera_image_json(const char *json, int len)
{
    char *payload = calloc(1, len + 1);
    if (payload == NULL) {
        return;
    }
    memcpy(payload, json, len);

    cJSON *root = cJSON_Parse(payload);
    if (root != NULL) {
        cJSON *device_id = cJSON_GetObjectItemCaseSensitive(root, "device_id");
        cJSON *mime = cJSON_GetObjectItemCaseSensitive(root, "mime");
        cJSON *width = cJSON_GetObjectItemCaseSensitive(root, "width");
        cJSON *height = cJSON_GetObjectItemCaseSensitive(root, "height");
        cJSON *motion_score = cJSON_GetObjectItemCaseSensitive(root, "motion_score");
        cJSON *image_base64 = cJSON_GetObjectItemCaseSensitive(root, "image_base64");

        if (cJSON_IsString(device_id) && cJSON_IsString(image_base64)) {
            camera_stream_handle_mqtt_image(
                device_id->valuestring,
                cJSON_IsString(mime) ? mime->valuestring : "image/jpeg",
                cJSON_IsNumber(width) ? width->valueint : 0,
                cJSON_IsNumber(height) ? height->valueint : 0,
                cJSON_IsNumber(motion_score) ? motion_score->valuedouble : 0.0,
                image_base64->valuestring);
        }
        cJSON_Delete(root);
    }

    free(payload);
}

static void handle_dashboard_config_json(const char *json, int len)
{
    char *payload = mqtt_large_calloc((size_t)len + 1U);
    if (payload == NULL) {
        ESP_LOGW(TAG, "dashboard config alloc failed len=%d", len);
        return;
    }
    memcpy(payload, json, len);

    bool applied = false;
    if (dashboard_board_lock(2000)) {
        applied = dashboard_config_apply_json(payload, true);
        if (applied) {
            ui_main_set_title(g_dashboard_title);
            ui_main_refresh_background();
            ui_control_refresh_all();
            ui_sensor_refresh_all();
            ui_camera_refresh_all();
        }
        dashboard_board_unlock();
    } else {
        ESP_LOGW(TAG, "dashboard config update skipped: UI lock timeout");
    }

    if (applied) {
        nvs_config_save_dashboard();
        mqtt_subscribe_dashboard_topics();
        ESP_LOGI(TAG, "dashboard config applied: device_id=%s version=%lu",
                 g_dashboard_device_id, (unsigned long)g_dashboard_config_version);
    } else {
        ESP_LOGW(TAG, "dashboard config rejected");
    }

    free(payload);
}

static void update_sensor_from_json_value(const char *topic, const char *device_id, sensor_item_t *item, cJSON *root)
{
    cJSON *value = cJSON_GetObjectItemCaseSensitive(root, item->key);
    if (value == NULL) {
        ESP_LOGW(TAG, "sensor payload missing key=%s topic=%s device_id=%s",
                 item->key, topic, device_id != NULL ? device_id : "");
        return;
    }

    char text[16];
    if (cJSON_IsNumber(value)) {
        snprintf(text, sizeof(text), "%.1f", value->valuedouble);
    } else if (cJSON_IsString(value) && value->valuestring != NULL) {
        strlcpy(text, value->valuestring, sizeof(text));
    } else if (cJSON_IsBool(value)) {
        strlcpy(text, cJSON_IsTrue(value) ? "1" : "0", sizeof(text));
    } else {
        return;
    }
    ui_sensor_update_value(topic, device_id, item->key, text);
}

static bool sensor_item_accepts_device(const sensor_item_t *item, const char *device_id)
{
    return item->device_id[0] == '\0' || (device_id != NULL && strcmp(item->device_id, device_id) == 0);
}

static void handle_sensor_state_json(const char *topic, const char *json, int len)
{
    char *payload = calloc(1, len + 1);
    if (payload == NULL) {
        return;
    }
    memcpy(payload, json, len);

    cJSON *root = cJSON_Parse(payload);
    if (root != NULL) {
        cJSON *device_id = cJSON_GetObjectItemCaseSensitive(root, "device_id");
        const char *device = cJSON_IsString(device_id) ? device_id->valuestring : NULL;
        ESP_LOGI(TAG, "sensor payload topic=%s device_id=%s len=%d",
                 topic, device != NULL ? device : "", len);

        if (dashboard_board_lock(100)) {
            for (int i = 0; i < SENSOR_ITEM_COUNT; i++) {
                sensor_item_t *item = &g_sensor_items[i];
                if (!item->visible || !topic_name_matches(topic, item->topic) ||
                    !sensor_item_accepts_device(item, device)) {
                    continue;
                }
                ESP_LOGI(TAG, "sensor route idx=%d key=%s configured_device_id=%s",
                         i, item->key, item->device_id);
                update_sensor_from_json_value(topic, device, item, root);
            }
            dashboard_board_unlock();
        }
        cJSON_Delete(root);
    } else {
        ESP_LOGW(TAG, "invalid sensor JSON topic=%s len=%d", topic, len);
    }

    free(payload);
}

static void route_mqtt_payload(const char *topic, const char *data, int len)
{
    if (topic_matches_config_response(topic)) {
        handle_dashboard_config_json(data, len);
    } else if (topic_matches_camera_config(topic)) {
        handle_camera_image_json(data, len);
    } else if (topic_matches_sensor_config(topic)) {
        handle_sensor_state_json(topic, data, len);
    } else {
        mqtt_handle_control_state_json(data, len);
    }
}

static void handle_mqtt_data_event(esp_mqtt_event_handle_t event)
{
    if (event->total_data_len > event->data_len || event->current_data_offset > 0) {
        if (event->current_data_offset == 0) {
            free(s_mqtt_rx_payload);
            s_mqtt_rx_payload = mqtt_large_calloc((size_t)event->total_data_len + 1U);
            s_mqtt_rx_total = event->total_data_len;
            int topic_len = event->topic_len < (int)sizeof(s_mqtt_rx_topic) - 1
                                ? event->topic_len
                                : (int)sizeof(s_mqtt_rx_topic) - 1;
            memcpy(s_mqtt_rx_topic, event->topic, topic_len);
            s_mqtt_rx_topic[topic_len] = '\0';
            ESP_LOGI(TAG, "MQTT chunked payload topic=%s total=%d first=%d",
                     s_mqtt_rx_topic, event->total_data_len, event->data_len);
        }

        if (s_mqtt_rx_payload == NULL || event->current_data_offset + event->data_len > s_mqtt_rx_total) {
            ESP_LOGW(TAG, "dropping malformed MQTT chunk topic=%s total=%d offset=%d len=%d",
                     s_mqtt_rx_topic, s_mqtt_rx_total, event->current_data_offset, event->data_len);
            free(s_mqtt_rx_payload);
            s_mqtt_rx_payload = NULL;
            s_mqtt_rx_total = 0;
            return;
        }

        memcpy(s_mqtt_rx_payload + event->current_data_offset, event->data, event->data_len);
        if (event->current_data_offset + event->data_len == s_mqtt_rx_total) {
            route_mqtt_payload(s_mqtt_rx_topic, s_mqtt_rx_payload, s_mqtt_rx_total);
            free(s_mqtt_rx_payload);
            s_mqtt_rx_payload = NULL;
            s_mqtt_rx_total = 0;
        }
        return;
    }

    char topic[96];
    int topic_len = event->topic_len < (int)sizeof(topic) - 1 ? event->topic_len : (int)sizeof(topic) - 1;
    memcpy(topic, event->topic, topic_len);
    topic[topic_len] = '\0';
    route_mqtt_payload(topic, event->data, event->data_len);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;

    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_mqtt_connected = true;
        ESP_LOGI(TAG, "connected, subscribing dashboard topics");
        if (dashboard_board_lock(0)) {
            ui_main_set_mqtt_online(true);
            dashboard_board_unlock();
        }
        mqtt_subscribe_dashboard_topics();
        mqtt_request_config_update(false);
        break;
    case MQTT_EVENT_DATA:
        handle_mqtt_data_event(event);
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_mqtt_connected = false;
        ESP_LOGW(TAG, "disconnected");
        if (dashboard_board_lock(0)) {
            ui_main_set_mqtt_online(false);
            dashboard_board_unlock();
        }
        break;
    default:
        break;
    }
}

void mqtt_app_start(void) {
    const char *client_id = make_mqtt_client_id();
    esp_mqtt_client_config_t config = {
        .broker.address.uri = setup_manager_mqtt_uri(),
        .credentials.client_id = client_id,
        .buffer.size = 8192,
        .buffer.out_size = 2048,
    };
    ESP_LOGI(TAG, "starting MQTT uri=%s client_id=%s", setup_manager_mqtt_uri(), client_id);
    s_client = esp_mqtt_client_init(&config);
    if (s_client == NULL) {
        ESP_LOGW(TAG, "MQTT client init failed");
        return;
    }
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
}

void mqtt_publish_control(control_item_t *item, const char *cmd) {
    if (item == NULL) {
        return;
    }

    http_publish_request_t *request = calloc(1, sizeof(http_publish_request_t));
    if (request == NULL) {
        return;
    }

    if (cmd == NULL || cmd[0] == '\0') {
        cmd = item->cmd_toggle[0] ? item->cmd_toggle : "toggle";
    }

    bool is_onoff = strcmp(item->action, "onoff") == 0 || strcmp(item->action, "on_off") == 0;
    bool is_message = strcmp(item->action, "url") == 0 || strcmp(item->action, "message") == 0;
    if (is_onoff && strcmp(cmd, item->cmd_on) == 0 && item->url_on[0] != '\0') {
        strlcpy(request->direct_url, item->url_on, sizeof(request->direct_url));
    } else if (is_onoff && strcmp(cmd, item->cmd_off) == 0 && item->url_off[0] != '\0') {
        strlcpy(request->direct_url, item->url_off, sizeof(request->direct_url));
    } else if (is_message && item->url[0] != '\0') {
        strlcpy(request->direct_url, item->url, sizeof(request->direct_url));
    }

    strlcpy(request->topic, item->mqtt_topic, sizeof(request->topic));
    snprintf(request->payload, sizeof(request->payload), "{\"device\":\"%s\",\"switch\":\"%s\",\"cmd\":\"%s\"}",
             item->device, item->sw, cmd);
    if (xTaskCreate(http_publish_task, "http_publish", 4096, request, 4, NULL) != pdPASS) {
        free(request);
    }
}

void mqtt_publish_camera_command(const char *topic, const char *payload)
{
    if (s_client == NULL || topic == NULL || topic[0] == '\0') {
        return;
    }
    if (payload == NULL) {
        payload = "";
    }
    int msg_id = esp_mqtt_client_publish(s_client, topic, payload, 0, 0, 0);
    ESP_LOGI(TAG, "camera command topic=%s payload=%s msg_id=%d", topic, payload, msg_id);
}

static void http_publish_task(void *arg)
{
    http_publish_request_t *request = arg;
    if (request == NULL) {
        vTaskDelete(NULL);
        return;
    }
    char url[384];
    char encoded_topic[96];
    char encoded_payload[192];
    if (request->direct_url[0] != '\0') {
        sanitize_direct_url(request->direct_url, url, sizeof(url));
    } else {
        url_encode(request->topic, encoded_topic, sizeof(encoded_topic));
        url_encode(request->payload, encoded_payload, sizeof(encoded_payload));
        snprintf(url, sizeof(url), "%s?topic=%s&value=%s", setup_manager_http_publish_url(), encoded_topic, encoded_payload);
    }

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 3000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .skip_cert_common_name_check = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGW(TAG, "HTTP publish init failed");
        free(request);
        vTaskDelete(NULL);
        return;
    }
    esp_err_t err = esp_http_client_perform(client);
    ESP_LOGI(TAG, "HTTP publish %s status=%d", esp_err_to_name(err), esp_http_client_get_status_code(client));
    esp_http_client_cleanup(client);
    free(request);
    vTaskDelete(NULL);
}
