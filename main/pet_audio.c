// SPDX-License-Identifier: Apache-2.0
#include "pet_audio.h"
#include "bsp/esp-bsp.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"

typedef struct { pet_sound_t sound; uint32_t epoch; } cue_t;
static const char *TAG = "pet_audio";
static QueueHandle_t cues;
static esp_codec_dev_handle_t speaker;
static i2s_chan_handle_t tx;
static bool tx_running;
static nvs_handle_t settings;
static bool settings_open, enabled = true, available, screen_off, active, saved = true;
static uint32_t epoch, requested, completed;

static void enqueue(pet_sound_t sound)
{
    const cue_t cue = {.sound = sound, .epoch = __atomic_add_fetch(&epoch, 1, __ATOMIC_ACQ_REL)};
    if (cues) xQueueOverwrite(cues, &cue);
}
pet_audio_status_t pet_audio_status(void)
{
    return (pet_audio_status_t){.enabled = __atomic_load_n(&enabled, __ATOMIC_ACQUIRE),
        .available = __atomic_load_n(&available, __ATOMIC_ACQUIRE),
        .saved = __atomic_load_n(&saved, __ATOMIC_ACQUIRE),
        .revision = __atomic_load_n(&requested, __ATOMIC_ACQUIRE)};
}
void pet_audio_play(pet_sound_t sound)
{
    if (sound <= PET_SOUND_NONE || sound >= PET_SOUND_COUNT ||
        !__atomic_load_n(&enabled, __ATOMIC_ACQUIRE) ||
        !__atomic_load_n(&available, __ATOMIC_ACQUIRE) ||
        __atomic_load_n(&screen_off, __ATOMIC_ACQUIRE)) return;
    enqueue(sound);
}
void pet_audio_toggle(void)
{
    // Only the power worker toggles; playback and status may run on other tasks.
    const bool on = !__atomic_load_n(&enabled, __ATOMIC_ACQUIRE);
    __atomic_store_n(&enabled, on, __ATOMIC_RELEASE);
    __atomic_store_n(&saved, false, __ATOMIC_RELEASE);
    __atomic_add_fetch(&requested, 1, __ATOMIC_ACQ_REL);
    enqueue(on && !__atomic_load_n(&screen_off, __ATOMIC_ACQUIRE) ? PET_SOUND_ENABLED : PET_SOUND_NONE);
    ESP_LOGI(TAG, "BOOT: sound=%s", on ? "on" : "off");
}
void pet_audio_set_screen_off(bool off)
{
    __atomic_store_n(&screen_off, off, __ATOMIC_RELEASE);
    enqueue(PET_SOUND_NONE); // Cancels both a current cue and any stale queued cue.
}
bool pet_audio_wait_quiet(uint32_t timeout_ms)
{
    if (!cues) return true;
    const int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    do {
        if (!__atomic_load_n(&active, __ATOMIC_ACQUIRE) && uxQueueMessagesWaiting(cues) == 0 &&
            __atomic_load_n(&completed, __ATOMIC_ACQUIRE) == __atomic_load_n(&requested, __ATOMIC_ACQUIRE))
            return true;
        vTaskDelay(pdMS_TO_TICKS(10));
    } while (esp_timer_get_time() < deadline);
    return false;
}
static void save_setting(void)
{
    const uint32_t revision = __atomic_load_n(&requested, __ATOMIC_ACQUIRE);
    if (revision == __atomic_load_n(&completed, __ATOMIC_ACQUIRE)) return;
    const bool on = __atomic_load_n(&enabled, __ATOMIC_ACQUIRE);
    esp_err_t err = settings_open ? nvs_set_u8(settings, "sound_on", on) : ESP_ERR_INVALID_STATE;
    if (err == ESP_OK) err = nvs_commit(settings);
    // Finish even on failure so a settings fault cannot keep the CPUs awake forever.
    __atomic_store_n(&saved, err == ESP_OK, __ATOMIC_RELEASE);
    __atomic_store_n(&completed, revision, __ATOMIC_RELEASE);
    if (err != ESP_OK) ESP_LOGW(TAG, "Sound setting not saved: %s", esp_err_to_name(err));
    else ESP_LOGI(TAG, "Sound setting saved: %s", on ? "on" : "off");
}
static bool allowed(cue_t cue)
{
    return cue.epoch == __atomic_load_n(&epoch, __ATOMIC_ACQUIRE) &&
           __atomic_load_n(&enabled, __ATOMIC_ACQUIRE) &&
           __atomic_load_n(&available, __ATOMIC_ACQUIRE) &&
           !__atomic_load_n(&screen_off, __ATOMIC_ACQUIRE);
}
static void worker(void *context)
{
    (void)context;
    int16_t pcm[PET_SOUND_CHUNK];
    int16_t stereo[PET_SOUND_CHUNK * 2];
    cue_t cue;
    for (;;) {
        if (xQueueReceive(cues, &cue, portMAX_DELAY) != pdTRUE) continue;
        __atomic_store_n(&active, true, __ATOMIC_RELEASE);
        save_setting();
        if (cue.sound != PET_SOUND_NONE && allowed(cue)) {
            esp_codec_dev_sample_info_t format = {.sample_rate = PET_SOUND_RATE, .channel = 2, .bits_per_sample = 16};
            int err = esp_codec_dev_open(speaker, &format);
            if (err == ESP_CODEC_DEV_OK && allowed(cue)) {
                ESP_LOGI(TAG, "cue=%s", pet_sound_name(cue.sound));
                uint32_t offset = 0;
                size_t count;
                while (allowed(cue) && (count = pet_sound_render(cue.sound, offset, pcm, PET_SOUND_CHUNK))) {
                    // Feed identical L/R slots so either ES8311 DAC channel selection hears the mono cue.
                    for (size_t i = 0; i < count; ++i) stereo[i * 2] = stereo[i * 2 + 1] = pcm[i];
                    err = esp_codec_dev_write(speaker, stereo, count * 2 * sizeof(*stereo));
                    if (err != ESP_CODEC_DEV_OK) break;
                    offset += count;
                }
                // Let the short DMA tail drain naturally; mute/off still interrupts promptly.
                if (allowed(cue)) vTaskDelay(pdMS_TO_TICKS(25));
            }
            esp_codec_dev_close(speaker); // Gates I2S TX and disables PA; no microphone/RX channel exists.
            if (err != ESP_CODEC_DEV_OK) {
                __atomic_store_n(&available, false, __ATOMIC_RELEASE);
                ESP_LOGW(TAG, "Playback failed (%d); sound disabled, gameplay continues", err);
            }
        }
        __atomic_store_n(&active, false, __ATOMIC_RELEASE);
    }
}
// A fixed-format, TX-only data interface. The generic codec I2S adapter attempts
// to disable/reconfigure a stopped channel on every open, producing false errors.
static bool output_open(const audio_codec_data_if_t *data) { (void)data; return tx != NULL; }
static int output_enable(const audio_codec_data_if_t *data, esp_codec_dev_type_t type, bool on)
{
    (void)data;
    if (type != ESP_CODEC_DEV_TYPE_OUT || !tx) return ESP_CODEC_DEV_NOT_SUPPORT;
    if (tx_running == on) return ESP_CODEC_DEV_OK;
    const esp_err_t err = on ? i2s_channel_enable(tx) : i2s_channel_disable(tx);
    if (err == ESP_OK) tx_running = on;
    return err == ESP_OK ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_DRV_ERR;
}
static int output_format(const audio_codec_data_if_t *data, esp_codec_dev_type_t type, esp_codec_dev_sample_info_t *format)
{
    (void)data;
    return type == ESP_CODEC_DEV_TYPE_OUT && format->sample_rate == PET_SOUND_RATE &&
           format->bits_per_sample == 16 && format->channel == 2 ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_NOT_SUPPORT;
}
static int output_write(const audio_codec_data_if_t *data, uint8_t *pcm, int bytes)
{
    (void)data;
    size_t written = 0;
    const esp_err_t err = i2s_channel_write(tx, pcm, bytes, &written, 100);
    return err == ESP_OK && written == (size_t)bytes ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_DRV_ERR;
}
static const audio_codec_data_if_t output = {.is_open = output_open, .enable = output_enable,
    .set_fmt = output_format, .write = output_write};

static esp_codec_dev_handle_t speaker_init(void)
{
    // TX only: the BSP duplex convenience helper would also keep an unused RX active.
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(CONFIG_BSP_I2S_NUM, I2S_ROLE_MASTER);
    channel.auto_clear = true;
    channel.dma_desc_num = 4; channel.dma_frame_num = 128;
    if (i2s_new_channel(&channel, &tx, NULL) != ESP_OK) return NULL;
    i2s_std_config_t config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(PET_SOUND_RATE),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {.mclk = BSP_I2S_MCLK, .bclk = BSP_I2S_SCLK, .ws = BSP_I2S_LCLK,
                     .dout = BSP_I2S_DOUT, .din = GPIO_NUM_NC}};
    if (i2s_channel_init_std_mode(tx, &config) != ESP_OK) { i2s_del_channel(tx); tx = NULL; return NULL; }
    audio_codec_i2c_cfg_t control_config = {.port = BSP_I2C_NUM, .addr = ES8311_CODEC_DEFAULT_ADDR,
                                           .bus_handle = bsp_i2c_get_handle()};
    const audio_codec_ctrl_if_t *control = audio_codec_new_i2c_ctrl(&control_config);
    const audio_codec_gpio_if_t *gpio = audio_codec_new_gpio();
    if (!control || !gpio) return NULL;
    es8311_codec_cfg_t codec_config = {.ctrl_if = control, .gpio_if = gpio,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC, .pa_pin = BSP_POWER_AMP_IO,
        .master_mode = false, .use_mclk = true, .hw_gain = {.pa_voltage = 5.0, .codec_dac_voltage = 3.3}};
    const audio_codec_if_t *codec = es8311_codec_new(&codec_config);
    if (!codec) return NULL;
    esp_codec_dev_cfg_t device = {.dev_type = ESP_CODEC_DEV_TYPE_OUT, .codec_if = codec, .data_if = &output};
    return esp_codec_dev_new(&device);
}
bool pet_audio_init(void)
{
    const gpio_config_t pa = {.pin_bit_mask = 1ULL << BSP_POWER_AMP_IO, .mode = GPIO_MODE_OUTPUT};
    if (gpio_config(&pa) != ESP_OK) return false;
    gpio_set_level(BSP_POWER_AMP_IO, 0);
    settings_open = nvs_open_from_partition("pet_nvs", "audio", NVS_READWRITE, &settings) == ESP_OK;
    uint8_t stored;
    if (settings_open && nvs_get_u8(settings, "sound_on", &stored) == ESP_OK && stored <= 1) enabled = stored;
    cues = xQueueCreate(1, sizeof(cue_t));
    if (!cues) return false;
    speaker = speaker_init();
    if (speaker) {
        esp_codec_dev_set_out_vol(speaker, PET_SOUND_VOLUME);
        available = true; // Keep DAC/PA and I2S stopped until the first accepted cue.
    }
    if (xTaskCreate(worker, "pet_audio", 6144, NULL, 3, NULL) != pdPASS) {
        vQueueDelete(cues); cues = NULL; available = false;
    }
    ESP_LOGI(TAG, "ES8311 TX-only ready=%d sound=%s volume=%d", available, enabled ? "on" : "off", PET_SOUND_VOLUME);
    return available;
}
