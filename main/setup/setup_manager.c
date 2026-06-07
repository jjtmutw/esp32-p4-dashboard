#include "setup_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_event.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "app_config/device_config.h"
#include "storage/nvs_config.h"
#include "board/dashboard_board.h"
#include "ui_main.h"

#ifndef CONFIG_WIFI_RMT_STATIC_RX_BUFFER_NUM
#define CONFIG_WIFI_RMT_STATIC_RX_BUFFER_NUM 10
#endif
#ifndef CONFIG_WIFI_RMT_DYNAMIC_RX_BUFFER_NUM
#define CONFIG_WIFI_RMT_DYNAMIC_RX_BUFFER_NUM 32
#endif
#ifndef CONFIG_WIFI_RMT_TX_BUFFER_TYPE
#define CONFIG_WIFI_RMT_TX_BUFFER_TYPE 1
#endif
#ifndef CONFIG_WIFI_RMT_DYNAMIC_RX_MGMT_BUF
#define CONFIG_WIFI_RMT_DYNAMIC_RX_MGMT_BUF 0
#endif
#ifndef CONFIG_WIFI_RMT_ESPNOW_MAX_ENCRYPT_NUM
#define CONFIG_WIFI_RMT_ESPNOW_MAX_ENCRYPT_NUM 7
#endif

#define SETUP_AP_SSID_PREFIX "JJ-Dashboard-Setup"
#define SETUP_AP_PASSWORD "12345678"
#define SETUP_AP_CHANNEL 6
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_MAX_RETRY 8
#define WIFI_START_STAGGER_MAX_MS 2500

static const char *TAG = "setup_manager";
static dashboard_runtime_config_t s_runtime_config;
static EventGroupHandle_t s_wifi_events;
static esp_netif_t *s_sta_netif;
static esp_netif_t *s_ap_netif;
static httpd_handle_t s_httpd;
static bool s_wifi_started;
static bool s_online;
static int s_retry_count;
static char s_setup_ap_ssid[33];

static const esp_netif_ip_info_t s_setup_ap_ip = {
    .ip = { .addr = ESP_IP4TOADDR(192, 168, 4, 1) },
    .gw = { .addr = ESP_IP4TOADDR(192, 168, 4, 1) },
    .netmask = { .addr = ESP_IP4TOADDR(255, 255, 255, 0) },
};

static bool mac_is_zero(const uint8_t mac[6])
{
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0) {
            return false;
        }
    }
    return true;
}

static uint32_t identity_hash(void)
{
    uint32_t hash = 2166136261U;
    uint8_t base_mac[6] = {0};

    if (esp_efuse_mac_get_default(base_mac) == ESP_OK && !mac_is_zero(base_mac)) {
        for (int i = 0; i < 6; i++) {
            hash ^= base_mac[i];
            hash *= 16777619U;
        }
    }

    for (int i = 0; g_dashboard_device_id[i] != '\0'; i++) {
        hash ^= (uint8_t)g_dashboard_device_id[i];
        hash *= 16777619U;
    }

    return hash;
}

static void build_fallback_mac(uint8_t mac[6], uint8_t suffix)
{
    uint8_t base_mac[6] = {0};
    uint32_t hash = identity_hash();

    if (esp_efuse_mac_get_default(base_mac) == ESP_OK && !mac_is_zero(base_mac)) {
        mac[0] = 0x02;
        mac[1] = base_mac[1];
        mac[2] = base_mac[2];
        mac[3] = base_mac[3];
        mac[4] = base_mac[4] ^ (uint8_t)(hash >> 8);
        mac[5] = base_mac[5] ^ suffix ^ (uint8_t)hash;
        return;
    }

    mac[0] = 0x02;
    mac[1] = 0x4a;
    mac[2] = 0x4a;
    mac[3] = (uint8_t)(hash >> 16);
    mac[4] = (uint8_t)(hash >> 8);
    mac[5] = (uint8_t)hash ^ suffix;
}

static void build_setup_ap_ssid(char *ssid, size_t ssid_size)
{
    strlcpy(ssid, SETUP_AP_SSID_PREFIX, ssid_size);
}

static void stagger_wifi_start(const char *reason)
{
    uint32_t delay_ms = identity_hash() % WIFI_START_STAGGER_MAX_MS;
    if (delay_ms < 100) {
        delay_ms += 100;
    }

    ESP_LOGI(TAG, "stagger %s Wi-Fi start by %lu ms", reason, (unsigned long)delay_ms);
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

static void apply_wifi_mac(wifi_interface_t ifx, esp_mac_type_t type, const char *name, uint8_t fallback_suffix)
{
    uint8_t mac[6] = {0};
    esp_err_t err = esp_read_mac(mac, type);
    if (err != ESP_OK || mac_is_zero(mac)) {
        ESP_LOGW(TAG, "%s MAC unavailable from eFuse (%s), using local fallback", name, esp_err_to_name(err));
        build_fallback_mac(mac, fallback_suffix);
    }

    mac[0] &= 0xfe;
    if ((mac[0] & 0x02) == 0 && fallback_suffix >= 0x80) {
        mac[0] |= 0x02;
    }

    err = esp_wifi_set_mac(ifx, mac);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "%s MAC set to " MACSTR, name, MAC2STR(mac));
    } else {
        ESP_LOGW(TAG, "%s MAC set failed: %s", name, esp_err_to_name(err));
    }
}

static void ui_set_online(bool online)
{
    if (dashboard_board_lock(50)) {
        ui_main_set_network_online(online);
        dashboard_board_unlock();
    }
}

static void url_decode(char *text)
{
    char *src = text;
    char *dst = text;
    while (*src) {
        if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else if (src[0] == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], 0};
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static void form_get_value(const char *body, const char *key, char *dest, size_t dest_size)
{
    if (body == NULL || key == NULL || dest == NULL || dest_size == 0) {
        return;
    }

    dest[0] = '\0';
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "%s=", key);
    const char *start = strstr(body, pattern);
    if (start == NULL) {
        return;
    }

    start += strlen(pattern);
    const char *end = strchr(start, '&');
    size_t len = end == NULL ? strlen(start) : (size_t)(end - start);
    if (len >= dest_size) {
        len = dest_size - 1;
    }
    memcpy(dest, start, len);
    dest[len] = '\0';
    url_decode(dest);
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    char page[2048];
    snprintf(page, sizeof(page),
             "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
             "<title>JJ Dashboard Setup</title><style>"
             "body{font-family:Arial,sans-serif;background:#06121d;color:#e9fbff;margin:24px}"
             "label{display:block;margin:14px 0 6px}input{width:100%%;box-sizing:border-box;padding:12px;border-radius:8px;border:1px solid #159faf;background:#0b2230;color:white;font-size:16px}"
             "button{margin-top:20px;width:100%%;padding:14px;border:0;border-radius:8px;background:#0ab9b9;color:white;font-size:18px}"
             ".hint{color:#8cb9c8;font-size:14px;line-height:1.4}</style></head><body>"
             "<h2>JJ Dashboard Setup</h2><p class='hint'>Enter Wi-Fi and dashboard connection settings. The device will restart after saving.</p>"
             "<form method='post' action='/save'>"
             "<label>Wi-Fi SSID</label><input name='ssid' value='%s' required>"
             "<label>Wi-Fi Password</label><input name='pass' type='password' value='%s'>"
             "<label>MQTT Broker URI</label><input name='mqtt' value='%s'>"
             "<label>HTTP Publish URL</label><input name='http' value='%s'>"
             "<button type='submit'>Save and Restart</button></form></body></html>",
             s_runtime_config.wifi_ssid, s_runtime_config.wifi_password,
             s_runtime_config.mqtt_uri, s_runtime_config.http_publish_url);

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t save_post_handler(httpd_req_t *req)
{
    int total = req->content_len;
    char *body = calloc(1, total + 1);
    if (body == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
    }

    int received = 0;
    while (received < total) {
        int ret = httpd_req_recv(req, body + received, total - received);
        if (ret <= 0) {
            free(body);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Read failed");
        }
        received += ret;
    }

    dashboard_runtime_config_t config = s_runtime_config;
    form_get_value(body, "ssid", config.wifi_ssid, sizeof(config.wifi_ssid));
    form_get_value(body, "pass", config.wifi_password, sizeof(config.wifi_password));
    form_get_value(body, "mqtt", config.mqtt_uri, sizeof(config.mqtt_uri));
    form_get_value(body, "http", config.http_publish_url, sizeof(config.http_publish_url));
    config.has_wifi = config.wifi_ssid[0] != '\0';
    nvs_config_save_runtime(&config);
    free(body);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, "<html><body><h2>Saved. Restarting...</h2></body></html>");
    vTaskDelay(pdMS_TO_TICKS(600));
    esp_restart();
    return ESP_OK;
}

static esp_err_t start_http_server(void)
{
    if (s_httpd != NULL) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    ESP_RETURN_ON_ERROR(httpd_start(&s_httpd, &config), TAG, "start setup web server failed");

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
    };
    httpd_uri_t save = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = save_post_handler,
    };
    httpd_register_uri_handler(s_httpd, &root);
    httpd_register_uri_handler(s_httpd, &save);
    ESP_LOGI(TAG, "setup portal ready: http://192.168.4.1");
    return ESP_OK;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_online = false;
        ui_set_online(false);
        if (s_retry_count++ < WIFI_MAX_RETRY) {
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_wifi_events, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_retry_count = 0;
        s_online = true;
        ui_set_online(true);
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}

static esp_netif_t *create_setup_ap_netif(void)
{
    esp_netif_inherent_config_t netif_config = {
        .flags = ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP,
        .ip_info = &s_setup_ap_ip,
        .get_ip_event = 0,
        .lost_ip_event = 0,
        .if_key = "WIFI_AP_DEF",
        .if_desc = "setup_ap",
        .route_prio = 10,
    };

    esp_netif_t *netif = esp_netif_create_wifi(WIFI_IF_AP, &netif_config);
    if (netif != NULL) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_default_wifi_ap_handlers());
    }
    return netif;
}

static esp_err_t wifi_init_once(void)
{
    if (s_wifi_events == NULL) {
        s_wifi_events = xEventGroupCreate();
    }
    if (s_sta_netif == NULL) {
        s_sta_netif = esp_netif_create_default_wifi_sta();
    }
    if (s_ap_netif == NULL) {
        s_ap_netif = create_setup_ap_netif();
    }

    if (!s_wifi_started) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init failed");
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));
        s_wifi_started = true;
    }
    return ESP_OK;
}

static esp_err_t start_portal(void)
{
    ESP_RETURN_ON_ERROR(wifi_init_once(), TAG, "wifi init for AP failed");
    s_online = false;
    ui_set_online(false);
    build_setup_ap_ssid(s_setup_ap_ssid, sizeof(s_setup_ap_ssid));

    wifi_config_t ap_config = {
        .ap = {
            .channel = SETUP_AP_CHANNEL,
            .password = SETUP_AP_PASSWORD,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .ssid_hidden = 0,
            .beacon_interval = 100,
        },
    };

    esp_err_t stop_err = esp_wifi_stop();
    if (stop_err != ESP_OK && stop_err != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGW(TAG, "wifi stop before setup AP failed: %s", esp_err_to_name(stop_err));
    }

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "set setup AP mode failed");
    apply_wifi_mac(WIFI_IF_AP, ESP_MAC_WIFI_SOFTAP, "setup AP", 0x81);
    strlcpy((char *)ap_config.ap.ssid, s_setup_ap_ssid, sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid_len = strlen(s_setup_ap_ssid);
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap_config), TAG, "set setup AP config failed");
    stagger_wifi_start("setup AP");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start setup AP failed");

    uint8_t ap_mac[6] = {0};
    if (esp_wifi_get_mac(WIFI_IF_AP, ap_mac) == ESP_OK) {
        ESP_LOGI(TAG, "setup AP active MAC: " MACSTR, MAC2STR(ap_mac));
        if (mac_is_zero(ap_mac)) {
            ESP_LOGE(TAG, "setup AP MAC is zero; external Wi-Fi radio is probably not initialized");
        }
    }
    ESP_LOGI(TAG, "setup AP started: ssid=%s password=%s", s_setup_ap_ssid, SETUP_AP_PASSWORD);
    return start_http_server();
}

static esp_err_t try_sta_connect(void)
{
    if (!s_runtime_config.has_wifi) {
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(wifi_init_once(), TAG, "wifi init for STA failed");
    xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    s_retry_count = 0;

    wifi_config_t sta_config = {0};
    strlcpy((char *)sta_config.sta.ssid, s_runtime_config.wifi_ssid, sizeof(sta_config.sta.ssid));
    strlcpy((char *)sta_config.sta.password, s_runtime_config.wifi_password, sizeof(sta_config.sta.password));
    sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;

    esp_err_t stop_err = esp_wifi_stop();
    if (stop_err != ESP_OK && stop_err != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGW(TAG, "wifi stop before STA connect failed: %s", esp_err_to_name(stop_err));
    }

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set STA mode failed");
    apply_wifi_mac(WIFI_IF_STA, ESP_MAC_WIFI_STA, "STA", 0x80);
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &sta_config), TAG, "set STA config failed");
    stagger_wifi_start("STA");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start STA failed");
    ESP_RETURN_ON_ERROR(esp_wifi_connect(), TAG, "STA connect failed");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_events, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, pdMS_TO_TICKS(12000));
    return (bits & WIFI_CONNECTED_BIT) ? ESP_OK : ESP_FAIL;
}

esp_err_t setup_manager_start(void)
{
    nvs_config_load_runtime(&s_runtime_config);
    if (try_sta_connect() == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi connected to %s", s_runtime_config.wifi_ssid);
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Wi-Fi unavailable, entering setup portal");
    s_online = false;
    ui_set_online(false);
    return start_portal();
}

static void force_portal_task(void *arg)
{
    (void)arg;
    nvs_config_load_runtime(&s_runtime_config);
    start_portal();
    vTaskDelete(NULL);
}

void setup_manager_force_portal(void)
{
    xTaskCreate(force_portal_task, "force_setup", 4096, NULL, 4, NULL);
}

bool setup_manager_is_online(void)
{
    return s_online;
}

const char *setup_manager_mqtt_uri(void)
{
    return s_runtime_config.mqtt_uri[0] ? s_runtime_config.mqtt_uri : CONFIG_DASHBOARD_MQTT_BROKER_URI;
}

const char *setup_manager_http_publish_url(void)
{
    return s_runtime_config.http_publish_url[0] ? s_runtime_config.http_publish_url : CONFIG_DASHBOARD_HTTP_PUBLISH_URL;
}

const char *setup_manager_setup_ap_ssid(void)
{
    return s_setup_ap_ssid[0] ? s_setup_ap_ssid : SETUP_AP_SSID_PREFIX;
}
