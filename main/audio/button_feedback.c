#include "button_feedback.h"

#include <stdint.h>
#include <string.h>

#include "bsp/esp32_p4_wifi6_touch_lcd_4b.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define FEEDBACK_SAMPLE_RATE 22050
#define FEEDBACK_CHUNK_SAMPLES 256
#define FEEDBACK_VOLUME 55
#define FEEDBACK_AMPLITUDE 7000

static const char *TAG = "button_feedback";
static TaskHandle_t s_feedback_task;
static esp_codec_dev_handle_t s_speaker;

static bool speaker_open(void)
{
    if (s_speaker == NULL) {
        s_speaker = bsp_audio_codec_speaker_init();
        if (s_speaker == NULL) {
            ESP_LOGW(TAG, "Speaker codec init failed");
            return false;
        }

        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 1,
            .channel_mask = 0,
            .sample_rate = FEEDBACK_SAMPLE_RATE,
            .mclk_multiple = 0,
        };
        if (esp_codec_dev_open(s_speaker, &fs) != ESP_CODEC_DEV_OK) {
            ESP_LOGW(TAG, "Speaker codec open failed");
            return false;
        }
        esp_codec_dev_set_out_vol(s_speaker, FEEDBACK_VOLUME);
    }
    return true;
}

static void write_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    int16_t samples[FEEDBACK_CHUNK_SAMPLES];
    uint32_t phase = 0;
    uint32_t phase_step = (freq_hz * 65536U) / FEEDBACK_SAMPLE_RATE;
    uint32_t remaining = (FEEDBACK_SAMPLE_RATE * duration_ms) / 1000U;

    while (remaining > 0) {
        uint32_t count = remaining > FEEDBACK_CHUNK_SAMPLES ? FEEDBACK_CHUNK_SAMPLES : remaining;
        for (uint32_t i = 0; i < count; i++) {
            phase += phase_step;
            samples[i] = (phase & 0x8000U) ? FEEDBACK_AMPLITUDE : -FEEDBACK_AMPLITUDE;
        }
        esp_codec_dev_write(s_speaker, samples, count * sizeof(samples[0]));
        remaining -= count;
    }
}

static void write_silence(uint32_t duration_ms)
{
    int16_t samples[FEEDBACK_CHUNK_SAMPLES];
    memset(samples, 0, sizeof(samples));
    uint32_t remaining = (FEEDBACK_SAMPLE_RATE * duration_ms) / 1000U;

    while (remaining > 0) {
        uint32_t count = remaining > FEEDBACK_CHUNK_SAMPLES ? FEEDBACK_CHUNK_SAMPLES : remaining;
        esp_codec_dev_write(s_speaker, samples, count * sizeof(samples[0]));
        remaining -= count;
    }
}

static void feedback_task(void *arg)
{
    (void)arg;
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        while (ulTaskNotifyTake(pdTRUE, 0) > 0) {
        }
        if (!speaker_open()) {
            continue;
        }
        write_tone(1200, 55);
        write_silence(25);
        write_tone(1600, 55);
    }
}

void button_feedback_start(void)
{
    if (s_feedback_task != NULL) {
        return;
    }
    xTaskCreate(feedback_task, "button_feedback", 4096, NULL, 4, &s_feedback_task);
}

void button_feedback_beep(void)
{
    if (s_feedback_task != NULL) {
        xTaskNotifyGive(s_feedback_task);
    }
}
