#include "device_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "mbedtls/base64.h"

char g_dashboard_title[DASHBOARD_TITLE_SIZE] = "\x4a\x4a\xe8\xbe\xa6\xe5\x85\xac\xe5\xae\xa4\xe6\x8e\xa7\xe5\x88\xb6\xe5\x99\xa8";
dashboard_text_image_t g_dashboard_title_image;
char g_dashboard_device_id[DASHBOARD_DEVICE_ID_SIZE] = "";
uint32_t g_dashboard_config_version = 1;
static const char *TAG = "device_config";

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
/* Default tables intentionally omit optional runtime text images. */
control_item_t g_control_items[CONTROL_ITEM_COUNT] = {
    {"room_light", "Greenhouse Light", "Lighting control", "light", "toggle", "JJ/iot/cmd", "JJ/LED001", "s1", "on", "off", "toggle", "", "", "", false, 0, true, true},
    {"sprinkler", "Greenhouse Sprinkler", "Irrigation system", "water", "toggle", "JJ/iot/cmd", "JJ/LED001", "s2", "on", "off", "toggle", "", "", "", false, 0, true, true},
    {"fan", "Greenhouse Fan", "Ventilation", "fan", "toggle", "JJ/iot/cmd", "JJ/LED001", "s3", "on", "off", "toggle", "", "", "", false, 0, true, true},
    {"eco_power", "Eco Tank Power", "Power control", "power", "toggle", "JJ/iot/cmd", "JJ/LED001", "s4", "on", "off", "toggle", "", "", "", true, 0, true, true},
    {"night_light", "Night Light 1", "Hallway lighting", "lamp", "toggle", "JJ/iot/cmd", "JJ/LED002", "s1", "on", "off", "toggle", "", "", "", false, 0, true, true},
    {"windmill", "Windmill Light", "Decor lighting", "lamp", "toggle", "JJ/iot/cmd", "JJ/LED002", "s2", "on", "off", "toggle", "", "", "", false, 0, true, true},
    {"machine_power", "Machine Room Power", "Equipment supply", "plug", "toggle", "JJ/iot/cmd", "JJ/LED002", "s3", "on", "off", "toggle", "", "", "", true, 0, true, false},
    {"alert_old", "Boss Alert", "Alert reminder", "bell", "url", "JJ/alert/cmd", "JJ/ALERT001", "s1", "send", "off", "toggle", "https://emr.prof-jj.com/mqtt/publish_66.php?topic=JJ/alert/cmd&value={\"device\":\"JJ/ALERT001\",\"switch\":\"s1\",\"cmd\":\"send\"}", "", "", false, 0, true, false},
    {"voice_remind", "Voice Reminder", "TTS broadcast", "speaker", "toggle", "JJ/tts/cmd", "JJ/TTS001", "s1", "on", "off", "toggle", "", "", "", false, 0, true, true},
    {"office_cam", "Office Camera", "Camera power", "camera", "toggle", "JJ/cam/cmd", "CAM001", "power", "on", "off", "toggle", "", "", "", false, 0, true, false},
    {"plant_light", "Plant Grow Light", "Plant lighting", "plant", "toggle", "JJ/iot/cmd", "JJ/LED003", "s1", "on", "off", "toggle", "", "", "", false, 0, true, true},
    {"reboot_all", "Reboot All Devices", "System reboot", "reboot", "url", "JJ/system/cmd", "all", "reboot", "reboot", "none", "toggle", "https://emr.prof-jj.com/mqtt/publish_66.php?topic=JJ/system/cmd&value={\"device\":\"all\",\"switch\":\"reboot\",\"cmd\":\"reboot\"}", "", "", true, 0, true, false},
};

sensor_item_t g_sensor_items[SENSOR_ITEM_COUNT] = {
    {"Office Temp", "C", "jj/sensor/state", "temperature", "XffcKwL9", "28.3", true},
    {"Office Humidity", "%", "jj/sensor/state", "humidity", "XffcKwL9", "43.3", true},
    {"Office CO2", "ppm", "jj/sensor/state", "co2", "XffcKwL9", "1912", true},
    {"Office TVOC", "ppb", "jj/sensor/state", "tvoc", "XffcKwL9", "5628", true},
    {"Server Temp", "C", "jj/sensor/state", "temperature", "server01", "22.8", true},
    {"Server Humidity", "%", "jj/sensor/state", "humidity", "server01", "49.3", true},
    {"Lab Temp", "C", "jj/sensor/state", "temperature", "lab00001", "24.3", true},
    {"Lab Humidity", "%", "jj/sensor/state", "humidity", "lab00001", "65.0", true},
    {"Greenhouse Temp", "C", "jj/sensor/state", "temperature", "green001", "24.1", true},
    {"Greenhouse Humidity", "%", "jj/sensor/state", "humidity", "green001", "59.7", true},
    {"Light Level", "lux", "jj/sensor/state", "light", "green001", "1280", true},
    {"Soil Moisture", "%", "jj/sensor/state", "soil", "green001", "32.6", true},
};

camera_item_t g_camera_items[CAMERA_ITEM_COUNT] = {
    {"Office Camera", "RIEtEXly", "jj/camera/image", "http://192.168.1.26/stream", true},
    {"Lab Camera", "camera_2", "jj/camera/image", "http://camera-2.local/stream", true},
    {"Server Room Camera", "camera_3", "jj/camera/image", "http://camera-3.local/stream", true},
    {"Plant Room Camera", "camera_4", "jj/camera/image", "http://camera-4.local/stream", true},
};
#pragma GCC diagnostic pop

static cJSON *get_item_any(cJSON *root, const char *key1, const char *key2)
{
    cJSON *value = cJSON_GetObjectItemCaseSensitive(root, key1);
    if (value == NULL && key2 != NULL) {
        value = cJSON_GetObjectItemCaseSensitive(root, key2);
    }
    return value;
}

static cJSON *get_item_any3(cJSON *root, const char *key1, const char *key2, const char *key3)
{
    cJSON *value = get_item_any(root, key1, key2);
    if (value == NULL && key3 != NULL) {
        value = cJSON_GetObjectItemCaseSensitive(root, key3);
    }
    return value;
}

static void copy_json_string_any(cJSON *root, const char *key1, const char *key2, char *dst, size_t dst_size)
{
    cJSON *value = get_item_any(root, key1, key2);
    if (cJSON_IsString(value) && value->valuestring != NULL) {
        strlcpy(dst, value->valuestring, dst_size);
    }
}

static void copy_json_string_any3(cJSON *root, const char *key1, const char *key2, const char *key3,
                                  char *dst, size_t dst_size)
{
    cJSON *value = get_item_any3(root, key1, key2, key3);
    if (cJSON_IsString(value) && value->valuestring != NULL) {
        strlcpy(dst, value->valuestring, dst_size);
    }
}

static void copy_json_bool(cJSON *root, const char *key, bool *dst)
{
    cJSON *value = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsBool(value)) {
        *dst = cJSON_IsTrue(value);
    }
}

static void copy_json_int(cJSON *root, const char *key, int *dst)
{
    cJSON *value = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsNumber(value)) {
        *dst = value->valueint;
    }
}

static char *copy_dynamic_string(const char *src)
{
    if (src == NULL) {
        return NULL;
    }
    size_t len = strlen(src) + 1;
    char *dst = heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (dst == NULL) {
        dst = malloc(len);
    }
    if (dst != NULL) {
        memcpy(dst, src, len);
    }
    return dst;
}

void dashboard_text_image_free(dashboard_text_image_t *image)
{
    if (image == NULL) {
        return;
    }
    free(image->base64);
    free(image->data);
    memset(image, 0, sizeof(*image));
}

static void dashboard_config_free_text_images(void)
{
    dashboard_text_image_free(&g_dashboard_title_image);
    for (int i = 0; i < CONTROL_ITEM_COUNT; i++) {
        dashboard_text_image_free(&g_control_items[i].name_image);
        dashboard_text_image_free(&g_control_items[i].subtitle_image);
    }
    for (int i = 0; i < SENSOR_ITEM_COUNT; i++) {
        dashboard_text_image_free(&g_sensor_items[i].label_image);
        dashboard_text_image_free(&g_sensor_items[i].unit_image);
    }
    for (int i = 0; i < CAMERA_ITEM_COUNT; i++) {
        dashboard_text_image_free(&g_camera_items[i].label_image);
    }
}

static uint32_t parse_color_hex(const char *text, uint32_t fallback)
{
    if (text == NULL) {
        return fallback;
    }

    if (text[0] == '#') {
        text++;
    } else if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        text += 2;
    }

    char *end = NULL;
    unsigned long value = strtoul(text, &end, 16);
    if (end == text) {
        return fallback;
    }
    return (uint32_t)(value & 0x00ffffffU);
}

static bool decode_base64_alloc(const char *base64, uint8_t **out, size_t *out_len)
{
    if (base64 == NULL || out == NULL || out_len == NULL) {
        return false;
    }

    size_t needed = 0;
    int ret = mbedtls_base64_decode(NULL, 0, &needed, (const unsigned char *)base64, strlen(base64));
    if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL || needed == 0) {
        return false;
    }

    uint8_t *decoded = heap_caps_malloc(needed, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (decoded == NULL) {
        decoded = malloc(needed);
    }
    if (decoded == NULL) {
        return false;
    }

    ret = mbedtls_base64_decode(decoded, needed, out_len, (const unsigned char *)base64, strlen(base64));
    if (ret != 0) {
        free(decoded);
        return false;
    }

    *out = decoded;
    return true;
}

static bool expand_a4_to_a8(const uint8_t *src, size_t src_len, uint16_t width, uint16_t height, uint8_t *dst)
{
    size_t stride = ((size_t)width + 1U) / 2U;
    if (src_len < stride * height) {
        return false;
    }

    for (uint16_t y = 0; y < height; y++) {
        const uint8_t *row = src + (size_t)y * stride;
        uint8_t *out = dst + (size_t)y * width;
        for (uint16_t x = 0; x < width; x++) {
            uint8_t packed = row[x / 2U];
            uint8_t nibble = (x & 1U) ? (packed & 0x0fU) : (packed >> 4U);
            out[x] = (uint8_t)(nibble * 17U);
        }
    }
    return true;
}

static bool expand_a4_rle_to_a8(const uint8_t *src, size_t src_len, uint16_t width, uint16_t height, uint8_t *dst)
{
    size_t expected = (size_t)width * height;
    size_t out = 0;
    for (size_t i = 0; i + 1 < src_len && out < expected; i += 2) {
        uint8_t run = src[i];
        uint8_t value = src[i + 1] & 0x0fU;
        uint8_t alpha = (uint8_t)(value * 17U);
        for (uint8_t j = 0; j < run && out < expected; j++) {
            dst[out++] = alpha;
        }
    }
    return out == expected;
}

static bool expand_a1_to_a8(const uint8_t *src, size_t src_len, uint16_t width, uint16_t height, uint8_t *dst)
{
    size_t stride = ((size_t)width + 7U) / 8U;
    if (src_len < stride * height) {
        return false;
    }

    for (uint16_t y = 0; y < height; y++) {
        const uint8_t *row = src + (size_t)y * stride;
        uint8_t *out = dst + (size_t)y * width;
        for (uint16_t x = 0; x < width; x++) {
            uint8_t packed = row[x / 8U];
            uint8_t bit = (packed >> (7U - (x & 7U))) & 1U;
            out[x] = bit ? 255U : 0U;
        }
    }
    return true;
}

static bool expand_a1_rle_to_a8(const uint8_t *src, size_t src_len, uint16_t width, uint16_t height, uint8_t *dst)
{
    size_t expected = (size_t)width * height;
    size_t out = 0;
    for (size_t i = 0; i + 1 < src_len && out < expected; i += 2) {
        uint8_t run = src[i];
        uint8_t alpha = (src[i + 1] & 1U) ? 255U : 0U;
        for (uint8_t j = 0; j < run && out < expected; j++) {
            dst[out++] = alpha;
        }
    }
    return out == expected;
}

static bool dashboard_text_image_apply_json(dashboard_text_image_t *image, cJSON *root)
{
    if (image == NULL || root == NULL || !cJSON_IsObject(root)) {
        return false;
    }

    cJSON *format_node = cJSON_GetObjectItemCaseSensitive(root, "format");
    cJSON *width_node = cJSON_GetObjectItemCaseSensitive(root, "width");
    cJSON *height_node = cJSON_GetObjectItemCaseSensitive(root, "height");
    cJSON *data_node = cJSON_GetObjectItemCaseSensitive(root, "data");
    cJSON *base64_node = cJSON_GetObjectItemCaseSensitive(root, "base64");
    cJSON *color_node = cJSON_GetObjectItemCaseSensitive(root, "color");
    const char *format = cJSON_IsString(format_node) ? format_node->valuestring : "";
    const char *base64 = cJSON_IsString(data_node) ? data_node->valuestring :
                         (cJSON_IsString(base64_node) ? base64_node->valuestring : NULL);
    int width = cJSON_IsNumber(width_node) ? width_node->valueint : 0;
    int height = cJSON_IsNumber(height_node) ? height_node->valueint : 0;

    if (width <= 0 || width > 520 || height <= 0 || height > 80 || base64 == NULL || base64[0] == '\0') {
        return false;
    }

    uint8_t *decoded = NULL;
    size_t decoded_len = 0;
    if (!decode_base64_alloc(base64, &decoded, &decoded_len)) {
        ESP_LOGW(TAG, "text image base64 decode failed");
        return false;
    }

    size_t out_len = (size_t)width * height;
    uint8_t *a8 = heap_caps_malloc(out_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (a8 == NULL) {
        a8 = malloc(out_len);
    }
    if (a8 == NULL) {
        free(decoded);
        return false;
    }

    bool ok = false;
    if (strcmp(format, "a1_rle") == 0) {
        ok = expand_a1_rle_to_a8(decoded, decoded_len, (uint16_t)width, (uint16_t)height, a8);
    } else if (strcmp(format, "a1") == 0) {
        ok = expand_a1_to_a8(decoded, decoded_len, (uint16_t)width, (uint16_t)height, a8);
    } else if (strcmp(format, "a4_rle") == 0) {
        ok = expand_a4_rle_to_a8(decoded, decoded_len, (uint16_t)width, (uint16_t)height, a8);
    } else if (strcmp(format, "a4") == 0) {
        ok = expand_a4_to_a8(decoded, decoded_len, (uint16_t)width, (uint16_t)height, a8);
    } else if (strcmp(format, "a8") == 0 && decoded_len >= out_len) {
        memcpy(a8, decoded, out_len);
        ok = true;
    }
    free(decoded);

    if (!ok) {
        ESP_LOGW(TAG, "unsupported or malformed text image format=%s size=%dx%d", format, width, height);
        free(a8);
        return false;
    }

    dashboard_text_image_free(image);
    image->base64 = copy_dynamic_string(base64);
    if (image->base64 == NULL) {
        free(a8);
        return false;
    }
    image->data = a8;
    image->valid = true;
    image->width = (uint16_t)width;
    image->height = (uint16_t)height;
    image->color = parse_color_hex(cJSON_IsString(color_node) ? color_node->valuestring : NULL, 0xffffff);
    strlcpy(image->format, format[0] != '\0' ? format : "a8", sizeof(image->format));

    memset(&image->dsc, 0, sizeof(image->dsc));
    image->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    image->dsc.header.w = image->width;
    image->dsc.header.h = image->height;
    image->dsc.header.cf = LV_COLOR_FORMAT_A8;
    image->dsc.header.stride = image->width;
    image->dsc.data_size = out_len;
    image->dsc.data = image->data;
    return true;
}

static void dashboard_text_image_add_json(cJSON *root, const char *key, const dashboard_text_image_t *image)
{
    if (root == NULL || key == NULL || image == NULL || !image->valid || image->base64 == NULL) {
        return;
    }

    cJSON *node = cJSON_CreateObject();
    if (node == NULL) {
        return;
    }

    char color[8];
    snprintf(color, sizeof(color), "#%06lx", (unsigned long)(image->color & 0x00ffffffU));
    cJSON_AddStringToObject(node, "format", image->format[0] != '\0' ? image->format : "a8");
    cJSON_AddNumberToObject(node, "width", image->width);
    cJSON_AddNumberToObject(node, "height", image->height);
    cJSON_AddStringToObject(node, "color", color);
    cJSON_AddStringToObject(node, "data", image->base64);
    cJSON_AddItemToObject(root, key, node);
}

static cJSON *control_item_to_json(const control_item_t *item)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    cJSON_AddStringToObject(root, "id", item->id);
    cJSON_AddStringToObject(root, "category", "button");
    cJSON_AddStringToObject(root, "label", item->name);
    cJSON_AddStringToObject(root, "name", item->name);
    cJSON_AddStringToObject(root, "subtitle", item->subtitle);
    dashboard_text_image_add_json(root, "name_image", &item->name_image);
    dashboard_text_image_add_json(root, "subtitle_image", &item->subtitle_image);
    cJSON_AddStringToObject(root, "icon", item->icon);
    cJSON_AddStringToObject(root, "action", item->action);
    cJSON_AddStringToObject(root, "type", item->action);
    cJSON_AddStringToObject(root, "mqtt_topic", item->mqtt_topic);
    cJSON_AddStringToObject(root, "topic", item->mqtt_topic);
    cJSON_AddStringToObject(root, "device", item->device);
    cJSON_AddStringToObject(root, "switch", item->sw);
    cJSON_AddStringToObject(root, "cmd_on", item->cmd_on);
    cJSON_AddStringToObject(root, "cmd_off", item->cmd_off);
    cJSON_AddStringToObject(root, "cmd_toggle", item->cmd_toggle);
    cJSON_AddStringToObject(root, "url", item->url);
    cJSON_AddStringToObject(root, "urlon", item->url_on);
    cJSON_AddStringToObject(root, "urloff", item->url_off);
    cJSON_AddBoolToObject(root, "confirm", item->confirm);
    cJSON_AddNumberToObject(root, "auto_off_sec", item->auto_off_sec);
    cJSON_AddBoolToObject(root, "visible", item->visible);
    cJSON_AddBoolToObject(root, "state", item->state);
    return root;
}

static cJSON *sensor_item_to_json(const sensor_item_t *item)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }
    cJSON_AddStringToObject(root, "category", "sensor");
    cJSON_AddStringToObject(root, "label", item->label);
    cJSON_AddStringToObject(root, "unit", item->unit);
    dashboard_text_image_add_json(root, "label_image", &item->label_image);
    dashboard_text_image_add_json(root, "unit_image", &item->unit_image);
    cJSON_AddStringToObject(root, "topic", item->topic);
    cJSON_AddStringToObject(root, "key", item->key);
    cJSON_AddStringToObject(root, "device_id", item->device_id);
    cJSON_AddStringToObject(root, "value", item->value);
    cJSON_AddBoolToObject(root, "visible", item->visible);
    return root;
}

static cJSON *camera_item_to_json(const camera_item_t *item)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }
    cJSON_AddStringToObject(root, "category", "camera");
    cJSON_AddStringToObject(root, "label", item->label);
    dashboard_text_image_add_json(root, "label_image", &item->label_image);
    cJSON_AddStringToObject(root, "device_id", item->device_id);
    cJSON_AddStringToObject(root, "topic", item->topic);
    cJSON_AddStringToObject(root, "url", item->url);
    cJSON_AddBoolToObject(root, "visible", item->visible);
    return root;
}

void dashboard_config_set_device_id(const char *device_id)
{
    if (device_id == NULL || strlen(device_id) != DASHBOARD_DEVICE_ID_LEN) {
        return;
    }
    strlcpy(g_dashboard_device_id, device_id, sizeof(g_dashboard_device_id));
}

control_item_t *control_item_find_by_id(const char *id)
{
    if (id == NULL) {
        return NULL;
    }
    for (int i = 0; i < CONTROL_ITEM_COUNT; i++) {
        if (strcmp(g_control_items[i].id, id) == 0) {
            return &g_control_items[i];
        }
    }
    return NULL;
}

control_item_t *control_item_find_by_device_switch(const char *device, const char *sw)
{
    if (device == NULL || sw == NULL) {
        return NULL;
    }
    for (int i = 0; i < CONTROL_ITEM_COUNT; i++) {
        if (strcmp(g_control_items[i].device, device) == 0 && strcmp(g_control_items[i].sw, sw) == 0) {
            return &g_control_items[i];
        }
    }
    return NULL;
}

char *control_item_to_json_string(const control_item_t *item)
{
    cJSON *root = control_item_to_json(item);
    if (root == NULL) {
        return NULL;
    }
    char *json = cJSON_Print(root);
    cJSON_Delete(root);
    return json;
}

bool control_item_apply_json(control_item_t *item, const char *json)
{
    if (item == NULL || json == NULL) {
        return false;
    }

    cJSON *root = cJSON_Parse(json);
    if (root == NULL || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return false;
    }

    copy_json_string_any(root, "id", NULL, item->id, sizeof(item->id));
    copy_json_string_any(root, "label", "name", item->name, sizeof(item->name));
    copy_json_string_any(root, "subtitle", NULL, item->subtitle, sizeof(item->subtitle));
    copy_json_string_any(root, "icon", NULL, item->icon, sizeof(item->icon));
    copy_json_string_any(root, "action", "control_type", item->action, sizeof(item->action));
    copy_json_string_any(root, "type", NULL, item->action, sizeof(item->action));
    copy_json_string_any(root, "mqtt_topic", "topic", item->mqtt_topic, sizeof(item->mqtt_topic));
    copy_json_string_any(root, "device", NULL, item->device, sizeof(item->device));
    copy_json_string_any(root, "switch", NULL, item->sw, sizeof(item->sw));
    copy_json_string_any(root, "cmd_on", NULL, item->cmd_on, sizeof(item->cmd_on));
    copy_json_string_any(root, "cmd_off", NULL, item->cmd_off, sizeof(item->cmd_off));
    copy_json_string_any(root, "cmd_toggle", "cmd", item->cmd_toggle, sizeof(item->cmd_toggle));
    copy_json_string_any(root, "url", NULL, item->url, sizeof(item->url));
    copy_json_string_any(root, "urlon", "url_on", item->url_on, sizeof(item->url_on));
    copy_json_string_any(root, "urloff", "url_off", item->url_off, sizeof(item->url_off));
    dashboard_text_image_free(&item->name_image);
    dashboard_text_image_free(&item->subtitle_image);
    dashboard_text_image_apply_json(&item->name_image, cJSON_GetObjectItemCaseSensitive(root, "name_image"));
    dashboard_text_image_apply_json(&item->subtitle_image, cJSON_GetObjectItemCaseSensitive(root, "subtitle_image"));
    copy_json_bool(root, "confirm", &item->confirm);
    copy_json_int(root, "auto_off_sec", &item->auto_off_sec);
    copy_json_bool(root, "visible", &item->visible);
    copy_json_bool(root, "state", &item->state);

    cJSON_Delete(root);
    return true;
}

static bool sensor_item_apply_json(sensor_item_t *item, cJSON *root)
{
    if (item == NULL || root == NULL || !cJSON_IsObject(root)) {
        return false;
    }
    copy_json_string_any3(root, "label", "Label", "name", item->label, sizeof(item->label));
    copy_json_string_any(root, "unit", "Unit", item->unit, sizeof(item->unit));
    copy_json_string_any(root, "topic", "Topic", item->topic, sizeof(item->topic));
    copy_json_string_any(root, "key", "Key", item->key, sizeof(item->key));
    copy_json_string_any(root, "device_id", "DEVICE_ID", item->device_id, sizeof(item->device_id));
    copy_json_string_any(root, "value", NULL, item->value, sizeof(item->value));
    dashboard_text_image_free(&item->label_image);
    dashboard_text_image_free(&item->unit_image);
    dashboard_text_image_apply_json(&item->label_image, cJSON_GetObjectItemCaseSensitive(root, "label_image"));
    dashboard_text_image_apply_json(&item->unit_image, cJSON_GetObjectItemCaseSensitive(root, "unit_image"));
    copy_json_bool(root, "visible", &item->visible);
    return true;
}

static bool camera_item_apply_json(camera_item_t *item, cJSON *root)
{
    if (item == NULL || root == NULL || !cJSON_IsObject(root)) {
        return false;
    }
    copy_json_string_any3(root, "label", "Label", "name", item->label, sizeof(item->label));
    copy_json_string_any(root, "device_id", "DEVICE_ID", item->device_id, sizeof(item->device_id));
    copy_json_string_any(root, "topic", "Topic", item->topic, sizeof(item->topic));
    copy_json_string_any(root, "url", NULL, item->url, sizeof(item->url));
    dashboard_text_image_free(&item->label_image);
    dashboard_text_image_apply_json(&item->label_image, cJSON_GetObjectItemCaseSensitive(root, "label_image"));
    copy_json_bool(root, "visible", &item->visible);
    return true;
}

static void control_item_reset(control_item_t *item)
{
    dashboard_text_image_free(&item->name_image);
    dashboard_text_image_free(&item->subtitle_image);
    memset(item, 0, sizeof(*item));
    strlcpy(item->action, "message", sizeof(item->action));
    strlcpy(item->cmd_on, "on", sizeof(item->cmd_on));
    strlcpy(item->cmd_off, "off", sizeof(item->cmd_off));
    strlcpy(item->cmd_toggle, "toggle", sizeof(item->cmd_toggle));
}

static void log_dashboard_config_summary(void)
{
    ESP_LOGI(TAG, "config version=%lu title=%s", (unsigned long)g_dashboard_config_version, g_dashboard_title);
    for (int i = 0; i < CONTROL_ITEM_COUNT; i++) {
        if (g_control_items[i].visible) {
            ESP_LOGI(TAG, "control[%d] name=%s action=%s url=%s urlon=%s urloff=%s",
                     i, g_control_items[i].name, g_control_items[i].action,
                     g_control_items[i].url, g_control_items[i].url_on, g_control_items[i].url_off);
        }
    }
    for (int i = 0; i < SENSOR_ITEM_COUNT; i++) {
        if (g_sensor_items[i].visible) {
            ESP_LOGI(TAG, "sensor[%d] label=%s topic=%s key=%s device_id=%s",
                     i, g_sensor_items[i].label, g_sensor_items[i].topic,
                     g_sensor_items[i].key, g_sensor_items[i].device_id);
        }
    }
    for (int i = 0; i < CAMERA_ITEM_COUNT; i++) {
        if (g_camera_items[i].visible) {
            ESP_LOGI(TAG, "camera[%d] label=%s topic=%s device_id=%s",
                     i, g_camera_items[i].label, g_camera_items[i].topic,
                     g_camera_items[i].device_id);
        }
    }
}

char *control_items_to_json_string(void)
{
    cJSON *array = cJSON_CreateArray();
    if (array == NULL) {
        return NULL;
    }

    for (int i = 0; i < CONTROL_ITEM_COUNT; i++) {
        if (!g_control_items[i].visible) {
            continue;
        }
        cJSON *item = control_item_to_json(&g_control_items[i]);
        if (item != NULL) {
            cJSON_AddItemToArray(array, item);
        }
    }

    char *json = cJSON_PrintUnformatted(array);
    cJSON_Delete(array);
    return json;
}

bool control_items_apply_json(const char *json)
{
    cJSON *array = cJSON_Parse(json);
    if (array == NULL || !cJSON_IsArray(array)) {
        cJSON_Delete(array);
        return false;
    }

    cJSON *node = NULL;
    int index = 0;
    cJSON_ArrayForEach(node, array) {
        if (!cJSON_IsObject(node)) {
            continue;
        }
        cJSON *id = cJSON_GetObjectItemCaseSensitive(node, "id");
        control_item_t *item = cJSON_IsString(id) ? control_item_find_by_id(id->valuestring) : NULL;
        if (item == NULL && index < CONTROL_ITEM_COUNT) {
            item = &g_control_items[index];
        }
        if (item != NULL) {
            char *item_json = cJSON_PrintUnformatted(node);
            if (item_json != NULL) {
                control_item_apply_json(item, item_json);
                cJSON_free(item_json);
            }
        }
        index++;
    }

    cJSON_Delete(array);
    return true;
}

static void add_array_item(cJSON *array, cJSON *item)
{
    if (array != NULL && item != NULL) {
        cJSON_AddItemToArray(array, item);
    } else {
        cJSON_Delete(item);
    }
}

char *dashboard_config_to_json_string(void)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    cJSON_AddStringToObject(root, "device_id", g_dashboard_device_id);
    cJSON_AddNumberToObject(root, "version", g_dashboard_config_version);
    cJSON_AddStringToObject(root, "title", g_dashboard_title);
    dashboard_text_image_add_json(root, "title_image", &g_dashboard_title_image);

    cJSON *controls = cJSON_AddArrayToObject(root, "controls");
    for (int i = 0; i < CONTROL_ITEM_COUNT; i++) {
        if (!g_control_items[i].visible) {
            continue;
        }
        add_array_item(controls, control_item_to_json(&g_control_items[i]));
    }

    cJSON *sensors = cJSON_AddArrayToObject(root, "sensors");
    for (int i = 0; i < SENSOR_ITEM_COUNT; i++) {
        if (!g_sensor_items[i].visible) {
            continue;
        }
        add_array_item(sensors, sensor_item_to_json(&g_sensor_items[i]));
    }

    cJSON *cameras = cJSON_AddArrayToObject(root, "cameras");
    for (int i = 0; i < CAMERA_ITEM_COUNT; i++) {
        if (!g_camera_items[i].visible) {
            continue;
        }
        add_array_item(cameras, camera_item_to_json(&g_camera_items[i]));
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

bool dashboard_config_apply_json(const char *json, bool require_matching_device)
{
    if (json == NULL) {
        return false;
    }

    cJSON *root = cJSON_Parse(json);
    if (root == NULL || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return false;
    }

    cJSON *device_id = cJSON_GetObjectItemCaseSensitive(root, "device_id");
    if (cJSON_IsString(device_id) && device_id->valuestring != NULL) {
        if (require_matching_device && g_dashboard_device_id[0] != '\0' &&
            strcmp(device_id->valuestring, g_dashboard_device_id) != 0) {
            cJSON_Delete(root);
            return false;
        }
    }

    cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
    if (cJSON_IsNumber(version)) {
        g_dashboard_config_version = (uint32_t)version->valueint;
    }
    dashboard_config_free_text_images();
    copy_json_string_any(root, "title", NULL, g_dashboard_title, sizeof(g_dashboard_title));
    dashboard_text_image_apply_json(&g_dashboard_title_image, cJSON_GetObjectItemCaseSensitive(root, "title_image"));

    cJSON *controls = cJSON_GetObjectItemCaseSensitive(root, "controls");
    if (cJSON_IsArray(controls)) {
        for (int i = 0; i < CONTROL_ITEM_COUNT; i++) {
            g_control_items[i].visible = false;
        }
        cJSON *node = NULL;
        int index = 0;
        cJSON_ArrayForEach(node, controls) {
            if (index >= CONTROL_ITEM_COUNT) {
                break;
            }
            control_item_reset(&g_control_items[index]);
            char *item_json = cJSON_PrintUnformatted(node);
            if (item_json != NULL) {
                control_item_apply_json(&g_control_items[index], item_json);
                cJSON_free(item_json);
            }
            cJSON *visible = cJSON_GetObjectItemCaseSensitive(node, "visible");
            if (!cJSON_IsBool(visible)) {
                g_control_items[index].visible = true;
            }
            index++;
        }
    }

    cJSON *sensors = cJSON_GetObjectItemCaseSensitive(root, "sensors");
    if (cJSON_IsArray(sensors)) {
        for (int i = 0; i < SENSOR_ITEM_COUNT; i++) {
            g_sensor_items[i].visible = false;
        }
        cJSON *node = NULL;
        int index = 0;
        cJSON_ArrayForEach(node, sensors) {
            if (index >= SENSOR_ITEM_COUNT) {
                break;
            }
            memset(&g_sensor_items[index], 0, sizeof(g_sensor_items[index]));
            sensor_item_apply_json(&g_sensor_items[index], node);
            cJSON *visible = cJSON_GetObjectItemCaseSensitive(node, "visible");
            if (!cJSON_IsBool(visible)) {
                g_sensor_items[index].visible = true;
            }
            index++;
        }
    }

    cJSON *cameras = cJSON_GetObjectItemCaseSensitive(root, "cameras");
    if (cJSON_IsArray(cameras)) {
        for (int i = 0; i < CAMERA_ITEM_COUNT; i++) {
            g_camera_items[i].visible = false;
        }
        cJSON *node = NULL;
        int index = 0;
        cJSON_ArrayForEach(node, cameras) {
            if (index >= CAMERA_ITEM_COUNT) {
                break;
            }
            memset(&g_camera_items[index], 0, sizeof(g_camera_items[index]));
            camera_item_apply_json(&g_camera_items[index], node);
            cJSON *visible = cJSON_GetObjectItemCaseSensitive(node, "visible");
            if (!cJSON_IsBool(visible)) {
                g_camera_items[index].visible = true;
            }
            index++;
        }
    }

    cJSON_Delete(root);
    log_dashboard_config_summary();
    return true;
}
