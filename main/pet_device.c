// SPDX-License-Identifier: Apache-2.0
#include "pet_device.h"
#include "pet_power_key.h"
#include "pet_boot_key.h"
#include "pet_audio.h"
#include "pet_ui.h"
#include "pet_store.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include <inttypes.h>
#include <sys/time.h>
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "pet_device";
static i2c_master_dev_handle_t pmic;
static portMUX_TYPE battery_lock = portMUX_INITIALIZER_UNLOCKED;
static pet_battery_t battery;
static uint32_t pending_keys;
static bool key_ready;
static bool rest_requested;
static bool boot_ready;
static pet_boot_key_t boot_key;
enum { PET_BOOT_GPIO = GPIO_NUM_0 };

static void poll_boot(void)
{
    if (boot_ready && pet_boot_key_sample(&boot_key, gpio_get_level(PET_BOOT_GPIO) == 0,
                                          (uint32_t)(esp_timer_get_time() / 1000)))
        pet_audio_toggle();
}

pet_clock_t pet_device_clock(void)
{
    struct timeval now;
    if (gettimeofday(&now, NULL) != 0) return (pet_clock_t){0};
    return (pet_clock_t){.valid = now.tv_sec >= INT64_C(1704067200) &&
                        now.tv_sec < INT64_C(4102444800), .utc_s = now.tv_sec};
}

bool pet_device_set_clock(int64_t utc_s)
{
    if (utc_s < INT64_C(1704067200) || utc_s >= INT64_C(4102444800)) return false;
    const struct timeval now = {.tv_sec = utc_s};
    return settimeofday(&now, NULL) == 0;
}

static esp_err_t read_reg(uint8_t reg, uint8_t *out, size_t size)
{
    return i2c_master_transmit_receive(pmic, &reg, 1, out, size, 100);
}

static void sample_battery(void)
{
    uint8_t status[2], modules = 0, percent = 255;
    pet_battery_t next = {0};
    esp_err_t err = read_reg(0x00, status, sizeof(status));
    if (err == ESP_OK && (status[0] & 0x08)) {
        err = read_reg(0x18, &modules, 1);
        if (err == ESP_OK && (modules & 0x08)) err = read_reg(0xa4, &percent, 1);
    }
    if (err == ESP_OK) next = pet_battery_decode(status[0], status[1], modules, percent);
    else ESP_LOGW(TAG, "Battery read failed: %s", esp_err_to_name(err));
    portENTER_CRITICAL(&battery_lock);
    battery = next;
    portEXIT_CRITICAL(&battery_lock);
}

static bool key_read(void *context, uint8_t reg, uint8_t *value)
{
    (void)context;
    return read_reg(reg, value, 1) == ESP_OK;
}
static bool key_write(void *context, uint8_t reg, uint8_t value)
{
    (void)context;
    const uint8_t bytes[] = {reg, value};
    return i2c_master_transmit(pmic, bytes, sizeof(bytes), 100) == ESP_OK;
}
static const pet_power_key_io_t key_io = {.read = key_read, .write = key_write};

static bool wait_for_save(void)
{
    // The writer coalesces for one second. Never freeze another core during a pending commit.
    for (unsigned tries = 0; tries < 150; ++tries) {
        const pet_store_status_t status = pet_store_status();
        if (status == PET_STORE_SAVED) return true;
        if (status == PET_STORE_ERROR) return false;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return false;
}

static void run_low_power(void)
{
    bool paused = false, began = false;
    int64_t last_us = 0, began_us = 0;
    uint32_t sleep_cycles = 0;
    uint64_t checkpoint_ms = 0, remainder_us = 0;
    esp_err_t err = esp_lv_adapter_pause(2000);
    if (err != ESP_OK) goto done;
    paused = true;
    err = bsp_display_lock(2000);
    if (err != ESP_OK) goto done;
    began = pet_ui_sleep_begin();
    bsp_display_unlock();
    if (!began) goto done;
    err = gpio_wakeup_enable(PET_BOOT_GPIO, GPIO_INTR_LOW_LEVEL);
    if (err == ESP_OK) err = esp_sleep_enable_gpio_wakeup();
    if (err != ESP_OK) goto done;
    last_us = began_us = esp_timer_get_time();
    ESP_LOGI(TAG, "Low power preparing: LVGL/touch paused, timer poll 500ms");
    bool flush = true;
    while (__atomic_load_n(&rest_requested, __ATOMIC_ACQUIRE)) {
        if (flush && !wait_for_save()) { err = ESP_FAIL; break; }
        flush = false;
        poll_boot(); // A release may enqueue a settings commit; settle it before sleeping.
        if (!pet_audio_wait_quiet(2000)) { err = ESP_ERR_TIMEOUT; break; }
        // AXP_IRQ is not routed to an ESP GPIO on this board. PWR is latched in the PMIC.
        // Timer wake checks it without keeping either CPU or the LVGL tick timer running.
        if (pet_boot_key_busy(&boot_key)) {
            // Remain dark; debounce a held BOOT at 20ms without a low-level wake spin.
            vTaskDelay(pdMS_TO_TICKS(20));
        } else {
            err = esp_sleep_enable_timer_wakeup(500000);
            if (err == ESP_OK) err = esp_light_sleep_start();
            if (err == ESP_OK) ++sleep_cycles;
            else if (err == ESP_ERR_SLEEP_REJECT) {
                // A BOOT edge between polling and sleep entry is a normal wake,
                // not a display fault. Yield before debouncing and retry dark.
                err = ESP_OK;
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        }
        const int64_t now_us = esp_timer_get_time();
        const uint64_t elapsed_us = (uint64_t)(now_us - last_us) + remainder_us;
        const uint64_t elapsed_ms = elapsed_us / 1000;
        const int key = pet_power_key_poll(&key_io);
        poll_boot(); // BOOT only changes audio; it never queues a display/PWR event.
        if (key < 0) err = ESP_FAIL; // Restore the screen if the only physical wake path is unavailable.
        checkpoint_ms += elapsed_ms;
        const bool save = key == 1 || err != ESP_OK || checkpoint_ms >= 300000;
        if (bsp_display_lock(2000) != ESP_OK) { err = ESP_ERR_TIMEOUT; break; }
        pet_ui_sleep_advance(elapsed_ms, save);
        remainder_us = elapsed_us % 1000;
        last_us = now_us;
        bsp_display_unlock();
        if (save) { checkpoint_ms = 0; flush = true; }
        if (key == 1 || err != ESP_OK) break;
    }
    (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    // Only the LVGL thread sends CO5300 commands, under its display lock (0.4.4 fix).
    // Queue one PWR event so normal UI recovery restores the display and gates held touches.
done:
    (void)gpio_wakeup_disable(PET_BOOT_GPIO);
    (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    if (began && last_us && bsp_display_lock(2000) == ESP_OK) {
        const uint64_t tail_us = (uint64_t)(esp_timer_get_time() - last_us) + remainder_us;
        pet_ui_sleep_advance(tail_us / 1000, true);
        bsp_display_unlock();
    }
    if (began_us) ESP_LOGI(TAG, "Low power exit: sleep_cycles=%" PRIu32 " rest_ms=%" PRIu64,
                          sleep_cycles, (uint64_t)(esp_timer_get_time() - began_us) / 1000);
    __atomic_store_n(&rest_requested, false, __ATOMIC_RELEASE);
    if (err != ESP_OK) ESP_LOGW(TAG, "Low-power entry/loop failed: %s; restoring display", esp_err_to_name(err));
    if (began || err != ESP_OK) __atomic_add_fetch(&pending_keys, 1, __ATOMIC_RELEASE);
    if (paused) (void)esp_lv_adapter_resume();
}

static void power_task(void *context)
{
    (void)context;
    unsigned ticks = 0;
    for (;;) {
        if (__atomic_load_n(&rest_requested, __ATOMIC_ACQUIRE)) run_low_power();
        poll_boot();
        if (!key_ready && ticks % 50 == 0) key_ready = pet_power_key_init(&key_io);
        if (key_ready && ticks % 5 == 0) {
            const int key = pet_power_key_poll(&key_io);
            if (key == 1) __atomic_add_fetch(&pending_keys, 1, __ATOMIC_RELEASE);
        }
        if (ticks % 250 == 0) sample_battery();
        ++ticks;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

bool pet_device_init(void)
{
    gpio_config_t boot_config = {.pin_bit_mask = 1ULL << PET_BOOT_GPIO,
        .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE, .intr_type = GPIO_INTR_DISABLE};
    boot_ready = gpio_config(&boot_config) == ESP_OK;
    pet_boot_key_init(&boot_key, gpio_get_level(PET_BOOT_GPIO) == 0,
                     (uint32_t)(esp_timer_get_time() / 1000));
    if (!boot_ready) ESP_LOGW(TAG, "BOOT input unavailable");
    // Reuse the touch/audio bus. Retain factory charger and power-rail settings.
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) return false;
    i2c_device_config_t cfg = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x34, .scl_speed_hz = 100000};
    esp_err_t err = i2c_master_bus_add_device(bus, &cfg, &pmic);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Cannot attach battery reader: %s", esp_err_to_name(err));
        return false;
    }
    sample_battery();
    key_ready = pet_power_key_init(&key_io);
    if (!key_ready) ESP_LOGW(TAG, "PWR short-press setup failed; retrying in background");
    if (xTaskCreate(power_task, "pet_power", 6144, NULL, 2, NULL) != pdPASS) {
        i2c_master_bus_rm_device(pmic);
        pmic = NULL;
        portENTER_CRITICAL(&battery_lock);
        battery = (pet_battery_t){0};
        portEXIT_CRITICAL(&battery_lock);
        return false;
    }
    return true;
}

pet_battery_t pet_device_battery(void)
{
    portENTER_CRITICAL(&battery_lock);
    const pet_battery_t result = battery;
    portEXIT_CRITICAL(&battery_lock);
    return result;
}

bool pet_device_screen(bool on)
{
    const esp_err_t err = bsp_display_brightness_set(on ? 65 : 0);
    if (err != ESP_OK) ESP_LOGW(TAG, "Display brightness failed: %s", esp_err_to_name(err));
    return err == ESP_OK;
}

bool pet_device_take_screen_key(void)
{
    uint32_t pending = __atomic_load_n(&pending_keys, __ATOMIC_ACQUIRE);
    while (pending && !__atomic_compare_exchange_n(&pending_keys, &pending, pending - 1, false,
                                                   __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {}
    return pending != 0;
}

void pet_device_low_power(bool requested)
{
    __atomic_store_n(&rest_requested, requested, __ATOMIC_RELEASE);
}
