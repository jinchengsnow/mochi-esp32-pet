// SPDX-License-Identifier: Apache-2.0
#include <stdlib.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_app_desc.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "pet_ui.h"
#include "pet_store.h"
#include "pet_device.h"
#include "pet_audio.h"

void app_main(void)
{
    pet_progress_t progress;
    bool restored;
    const bool storage_ok = pet_store_init(&progress, &restored);
    if (bsp_display_start() == NULL) {
        ESP_LOGE("pet", "Display initialization failed");
        abort();
    }
    const bool audio_ok = pet_audio_init();
    const bool battery_ok = pet_device_init();
    ESP_ERROR_CHECK(bsp_display_lock(3000));
    // Brightness is a CO5300 SPI command, not an independent backlight GPIO.
    // Serialize it with LVGL flushes using the same display lock.
    ESP_ERROR_CHECK(bsp_display_brightness_set(65));
    pet_ui_bind_input(bsp_display_get_input_dev());
    pet_ui_create(lv_screen_active(), &progress);
    bsp_display_unlock();
    ESP_LOGI("pet", "MOCHI game %s ready: satiety=%u xp=%u restored=%d storage=%s battery=%s audio=%s",
             esp_app_get_description()->version, progress.satiety, progress.growth_xp,
             restored, storage_ok ? "ready" : "unavailable", battery_ok ? "polling" : "unavailable",
             audio_ok ? "ready" : "unavailable");
}
