#include "camera_stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "driver/jpeg_decode.h"
#include "esp_cache.h"
#include "esp_err.h"
#include "esp_log.h"
#include "mbedtls/base64.h"
#include "libs/tjpgd/tjpgd.h"
#include "misc/cache/instance/lv_image_cache.h"
#include "board/dashboard_board.h"

#define CAMERA_PREVIEW_MAX_W 640
#define CAMERA_PREVIEW_MAX_H 480
#define CAMERA_JPEG_WORKBUF_SIZE 4096

typedef struct {
    const uint8_t *data;
    size_t len;
    size_t offset;
    uint8_t *rgb888;
    int width;
    int height;
} camera_jpeg_mem_t;

typedef struct {
    char device_id[16];
    char url[128];
    lv_obj_t *preview;
    lv_obj_t *image_obj;
    lv_obj_t *status_label;
    lv_obj_t *placeholder_label;
    uint8_t *frame[2];
    size_t frame_alloc_len[2];
    size_t frame_len;
    int active_frame;
    lv_image_dsc_t image_dsc[2];
    bool online;
} camera_slot_t;

static const char *TAG = "camera_stream";
static camera_slot_t s_slots[CAMERA_CARD_COUNT] = {
    {.device_id = "RIEtEXly", .url = "http://192.168.1.26/stream"},
    {.device_id = "camera_2", .url = "http://camera-2.local/stream"},
    {.device_id = "camera_3", .url = "http://camera-3.local/stream"},
    {.device_id = "camera_4", .url = "http://camera-4.local/stream"},
};

static int camera_slot_for_device(const char *device_id)
{
    if (device_id == NULL) {
        return -1;
    }
    for (int i = 0; i < CAMERA_CARD_COUNT; i++) {
        if (strcmp(s_slots[i].device_id, device_id) == 0) {
            return i;
        }
    }
    return -1;
}

static size_t jpeg_mem_input(JDEC *jd, uint8_t *buff, size_t ndata)
{
    camera_jpeg_mem_t *ctx = (camera_jpeg_mem_t *)jd->device;
    if (ctx == NULL || ctx->offset >= ctx->len) {
        return 0;
    }

    size_t remain = ctx->len - ctx->offset;
    size_t read_len = ndata < remain ? ndata : remain;
    if (buff != NULL) {
        memcpy(buff, ctx->data + ctx->offset, read_len);
    }
    ctx->offset += read_len;
    return read_len;
}

static int jpeg_rgb_output(JDEC *jd, void *bitmap, JRECT *rect)
{
    camera_jpeg_mem_t *ctx = (camera_jpeg_mem_t *)jd->device;
    if (ctx == NULL || ctx->rgb888 == NULL || rect == NULL || bitmap == NULL) {
        return 0;
    }

    int rect_w = (int)rect->right - (int)rect->left + 1;
    int rect_h = (int)rect->bottom - (int)rect->top + 1;
    if (rect_w <= 0 || rect_h <= 0 || rect->right >= ctx->width || rect->bottom >= ctx->height) {
        return 0;
    }

    const uint8_t *src = (const uint8_t *)bitmap;
    for (int y = 0; y < rect_h; y++) {
        uint8_t *dst = ctx->rgb888 + (((int)rect->top + y) * ctx->width + (int)rect->left) * 3;
        for (int x = 0; x < rect_w; x++) {
            dst[x * 3 + 0] = src[x * 3 + 2];
            dst[x * 3 + 1] = src[x * 3 + 1];
            dst[x * 3 + 2] = src[x * 3 + 0];
        }
        src += rect_w * 3;
    }
    return 1;
}

static uint8_t *decode_jpeg_rgb888(const uint8_t *jpeg, size_t jpeg_len, int *width, int *height, size_t *frame_len)
{
    camera_jpeg_mem_t ctx = {
        .data = jpeg,
        .len = jpeg_len,
    };
    uint8_t *work = malloc(CAMERA_JPEG_WORKBUF_SIZE);
    JDEC *jd = malloc(sizeof(JDEC));
    if (work == NULL || jd == NULL) {
        free(work);
        free(jd);
        return NULL;
    }

    JRESULT rc = jd_prepare(jd, jpeg_mem_input, work, CAMERA_JPEG_WORKBUF_SIZE, &ctx);
    if (rc != JDR_OK || jd->width == 0 || jd->height == 0) {
        ESP_LOGW(TAG, "software jpeg header failed: %d", rc);
        free(work);
        free(jd);
        return NULL;
    }

    if (jd->width > 800 || jd->height > 480) {
        ESP_LOGW(TAG, "jpeg image too large: %ux%u", (unsigned)jd->width, (unsigned)jd->height);
        free(work);
        free(jd);
        return NULL;
    }

    size_t expected_len = (size_t)jd->width * (size_t)jd->height * 3;
    ctx.rgb888 = malloc(expected_len);
    if (ctx.rgb888 == NULL) {
        free(work);
        free(jd);
        return NULL;
    }
    ctx.width = (int)jd->width;
    ctx.height = (int)jd->height;

    rc = jd_decomp(jd, jpeg_rgb_output, 0);
    free(work);
    free(jd);
    if (rc != JDR_OK) {
        ESP_LOGW(TAG, "software jpeg decode failed: %d", rc);
        free(ctx.rgb888);
        return NULL;
    }

    *width = ctx.width;
    *height = ctx.height;
    *frame_len = expected_len;
    return ctx.rgb888;
}

static uint8_t mix_u8(uint8_t a, uint8_t b, uint32_t t)
{
    return (uint8_t)((a * (65536 - t) + b * t + 32768) >> 16);
}

static uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) | (uint16_t)(b >> 3));
}

static bool calc_fit_size(int src_w, int src_h, int *dst_w, int *dst_h, size_t *dst_len)
{
    if (src_w <= 0 || src_h <= 0) {
        return false;
    }

    int out_w = CAMERA_PREVIEW_MAX_W;
    int out_h = (src_h * out_w + (src_w / 2)) / src_w;
    if (out_h > CAMERA_PREVIEW_MAX_H) {
        out_h = CAMERA_PREVIEW_MAX_H;
        out_w = (src_w * out_h + (src_h / 2)) / src_h;
    }
    if (out_w <= 0 || out_h <= 0) {
        return false;
    }

    *dst_w = out_w;
    *dst_h = out_h;
    *dst_len = (size_t)out_w * (size_t)out_h * 2;
    return true;
}

static bool ensure_frame_buffer(camera_slot_t *slot, int frame_index, size_t len)
{
    if (slot->frame[frame_index] != NULL && slot->frame_alloc_len[frame_index] >= len) {
        return true;
    }

    free(slot->frame[frame_index]);
    slot->frame[frame_index] = NULL;
    slot->frame_alloc_len[frame_index] = 0;

    jpeg_decode_memory_alloc_cfg_t out_mem_cfg = {
        .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,
    };
    size_t alloc_len = 0;
    uint8_t *frame = jpeg_alloc_decoder_mem(len, &out_mem_cfg, &alloc_len);
    if (frame == NULL) {
        ESP_LOGW(TAG, "camera resize alloc failed: %u", (unsigned)len);
        return false;
    }
    slot->frame[frame_index] = frame;
    slot->frame_alloc_len[frame_index] = alloc_len;
    return true;
}

static bool resize_rgb888_fit_to_rgb565(const uint8_t *src, int src_w, int src_h, uint8_t *dst_buf,
                                 int out_w, int out_h, size_t out_len, size_t alloc_len)
{
    if (src == NULL || dst_buf == NULL || src_w <= 0 || src_h <= 0 || out_w <= 0 || out_h <= 0) {
        return false;
    }

    uint16_t *dst = (uint16_t *)dst_buf;
    uint32_t step_x = out_w > 1 ? (uint32_t)(((uint64_t)(src_w - 1) << 16) / (uint32_t)(out_w - 1)) : 0;
    uint32_t step_y = out_h > 1 ? (uint32_t)(((uint64_t)(src_h - 1) << 16) / (uint32_t)(out_h - 1)) : 0;

    for (int y = 0; y < out_h; y++) {
        uint32_t src_y_fp = (uint32_t)y * step_y;
        int y0 = (int)(src_y_fp >> 16);
        int y1 = y0 + 1 < src_h ? y0 + 1 : y0;
        uint32_t fy = src_y_fp & 0xffff;

        for (int x = 0; x < out_w; x++) {
            uint32_t src_x_fp = (uint32_t)x * step_x;
            int x0 = (int)(src_x_fp >> 16);
            int x1 = x0 + 1 < src_w ? x0 + 1 : x0;
            uint32_t fx = src_x_fp & 0xffff;

            const uint8_t *p00 = src + ((y0 * src_w + x0) * 3);
            const uint8_t *p01 = src + ((y0 * src_w + x1) * 3);
            const uint8_t *p10 = src + ((y1 * src_w + x0) * 3);
            const uint8_t *p11 = src + ((y1 * src_w + x1) * 3);

            uint8_t rt = mix_u8(p00[0], p01[0], fx);
            uint8_t gt = mix_u8(p00[1], p01[1], fx);
            uint8_t bt = mix_u8(p00[2], p01[2], fx);
            uint8_t rb = mix_u8(p10[0], p11[0], fx);
            uint8_t gb = mix_u8(p10[1], p11[1], fx);
            uint8_t bb = mix_u8(p10[2], p11[2], fx);

            dst[y * out_w + x] = rgb888_to_rgb565(mix_u8(rt, rb, fy), mix_u8(gt, gb, fy), mix_u8(bt, bb, fy));
        }
    }
    if (alloc_len > out_len) {
        memset(dst_buf + out_len, 0, alloc_len - out_len);
    }
    esp_err_t err = esp_cache_msync(dst, alloc_len, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "camera resize cache sync failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

void camera_stream_attach_card(int index, lv_obj_t *preview, lv_obj_t *image_obj, lv_obj_t *status_label,
                               lv_obj_t *placeholder_label)
{
    if (index < 0 || index >= CAMERA_CARD_COUNT) {
        return;
    }
    s_slots[index].preview = preview;
    s_slots[index].image_obj = image_obj;
    s_slots[index].status_label = status_label;
    s_slots[index].placeholder_label = placeholder_label;
}

void camera_stream_start_all(void)
{
    ESP_LOGI(TAG, "MQTT image mode enabled; HTTP/MJPEG polling is disabled");
}

void camera_stream_set_url(int index, const char *url)
{
    if (index < 0 || index >= CAMERA_CARD_COUNT || url == NULL) {
        return;
    }
    strlcpy(s_slots[index].url, url, sizeof(s_slots[index].url));
}

void camera_stream_set_device_id(int index, const char *device_id)
{
    if (index < 0 || index >= CAMERA_CARD_COUNT || device_id == NULL) {
        return;
    }
    strlcpy(s_slots[index].device_id, device_id, sizeof(s_slots[index].device_id));
}

bool camera_stream_is_online(int index)
{
    return index >= 0 && index < CAMERA_CARD_COUNT && s_slots[index].online;
}

void camera_stream_handle_mqtt_image(const char *device_id, const char *mime, int width, int height,
                                     double motion_score, const char *image_base64)
{
    if (image_base64 == NULL || mime == NULL || strcmp(mime, "image/jpeg") != 0) {
        return;
    }
    if (width <= 0 || height <= 0) {
        ESP_LOGW(TAG, "camera %s invalid image size %dx%d", device_id != NULL ? device_id : "", width, height);
        return;
    }

    int index = camera_slot_for_device(device_id);
    if (index < 0) {
        ESP_LOGW(TAG, "camera image no card for device_id=%s", device_id != NULL ? device_id : "");
        return;
    }
    camera_slot_t *slot = &s_slots[index];

    size_t needed = 0;
    int ret = mbedtls_base64_decode(NULL, 0, &needed, (const unsigned char *)image_base64, strlen(image_base64));
    if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL || needed == 0) {
        ESP_LOGW(TAG, "camera %s base64 size probe failed", device_id);
        return;
    }

    uint8_t *jpeg = malloc(needed);
    if (jpeg == NULL) {
        return;
    }

    size_t decoded = 0;
    ret = mbedtls_base64_decode(jpeg, needed, &decoded, (const unsigned char *)image_base64, strlen(image_base64));
    if (ret != 0) {
        ESP_LOGW(TAG, "camera %s base64 decode failed: %d", device_id, ret);
        free(jpeg);
        return;
    }

    int image_w = width;
    int image_h = height;
    size_t frame_len = 0;
    uint8_t *frame = decode_jpeg_rgb888(jpeg, decoded, &image_w, &image_h, &frame_len);
    free(jpeg);
    if (frame == NULL) {
        return;
    }

    int display_w = image_w;
    int display_h = image_h;
    size_t display_len = 0;
    if (!calc_fit_size(image_w, image_h, &display_w, &display_h, &display_len)) {
        free(frame);
        return;
    }
    int next_frame = slot->active_frame ^ 1;
    if (!ensure_frame_buffer(slot, next_frame, display_len)) {
        free(frame);
        return;
    }
    if (!resize_rgb888_fit_to_rgb565(frame, image_w, image_h, slot->frame[next_frame], display_w, display_h,
                                     display_len, slot->frame_alloc_len[next_frame])) {
        free(frame);
        return;
    }
    free(frame);

    if (!dashboard_board_lock(100)) {
        return;
    }

    if (slot->image_obj != NULL) {
        const void *old_src = lv_image_get_src(slot->image_obj);
        if (old_src != NULL) {
            lv_image_cache_drop(old_src);
        }
    }
    lv_image_cache_drop(&slot->image_dsc[0]);
    lv_image_cache_drop(&slot->image_dsc[1]);

    slot->frame_len = display_len;
    slot->active_frame = next_frame;
    slot->online = true;
    lv_image_dsc_t *image_dsc = &slot->image_dsc[slot->active_frame];
    memset(image_dsc, 0, sizeof(*image_dsc));
    image_dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
    image_dsc->header.w = display_w;
    image_dsc->header.h = display_h;
    image_dsc->header.cf = LV_COLOR_FORMAT_RGB565;
    image_dsc->header.stride = display_w * 2;
    image_dsc->data_size = display_len;
    image_dsc->data = slot->frame[slot->active_frame];

    if (slot->image_obj != NULL) {
        lv_image_set_src(slot->image_obj, image_dsc);
        lv_image_set_scale(slot->image_obj, LV_SCALE_NONE);
        lv_obj_center(slot->image_obj);
        lv_obj_clear_flag(slot->image_obj, LV_OBJ_FLAG_HIDDEN);
    }
    if (slot->placeholder_label != NULL) {
        lv_obj_add_flag(slot->placeholder_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (slot->status_label != NULL) {
        lv_label_set_text_fmt(slot->status_label, "MQTT %.0f", motion_score);
        lv_obj_set_style_text_color(slot->status_label, lv_color_hex(0x46ff8a), 0);
    }
    ESP_LOGI(TAG, "camera frame idx=%d device_id=%s jpeg=%u rgb888=%u display_rgb565=%u src=%dx%d display=%dx%d",
             index, device_id != NULL ? device_id : "", (unsigned)decoded, (unsigned)frame_len, (unsigned)display_len,
             image_w, image_h, display_w, display_h);

    dashboard_board_unlock();
}
