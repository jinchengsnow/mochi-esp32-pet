// SPDX-License-Identifier: Apache-2.0
#include "pet_store.h"
#include "pet_save.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <inttypes.h>

typedef struct { pet_progress_t progress; uint32_t request; } save_request_t;
static QueueHandle_t queue;
static nvs_handle_t handle;
static uint32_t generation, requested, completed;
static bool failed = true;

static void writer(void *context)
{
    (void)context;
    save_request_t pending;
    bool have = false;
    for (;;) {
        if (xQueueReceive(queue, &pending, have ? pdMS_TO_TICKS(5000) : portMAX_DELAY) == pdTRUE)
            have = true;
        if (!have) continue;
        // Coalesce bursts and cap flash commits to approximately once a second.
        vTaskDelay(pdMS_TO_TICKS(1000));
        save_request_t latest;
        if (xQueueReceive(queue, &latest, 0) == pdTRUE) pending = latest;
        uint8_t bytes[PET_SAVE_BYTES];
        const uint32_t next = generation + 1;
        pet_save_encode(&pending.progress, next, bytes);
        esp_err_t err = nvs_set_blob(handle, (next & 1u) ? "pet_a" : "pet_b", bytes, sizeof(bytes));
        if (err == ESP_OK) err = nvs_commit(handle);
        __atomic_store_n(&failed, err != ESP_OK, __ATOMIC_RELEASE);
        if (err == ESP_OK) {
            generation = next;
            __atomic_store_n(&completed, pending.request, __ATOMIC_RELEASE);
            ESP_LOGI("pet_store", "saved gen=%" PRIu32 " satiety=%u xp=%u meals=%" PRIu32,
                     generation, pending.progress.satiety, pending.progress.growth_xp,
                     pending.progress.meals);
            have = false;
        } else {
            ESP_LOGE("pet_store", "Save failed (%s); retaining last committed slot, will retry", esp_err_to_name(err));
        }
    }
}

bool pet_store_init(pet_progress_t *restored, bool *had_save)
{
    pet_progress_default(restored);
    *had_save = false;
    // This partition is separate from the factory/Bluetooth NVS. Never erase on errors.
    esp_err_t err = nvs_flash_init_partition("pet_nvs");
    if (err == ESP_OK) err = nvs_open_from_partition("pet_nvs", "mochi", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE("pet_store", "Storage unavailable: %s; no erase performed", esp_err_to_name(err));
        return false;
    }
    bool invalid = false;
    for (unsigned i = 0; i < 2; ++i) {
        uint8_t bytes[PET_SAVE_BYTES];
        size_t size = sizeof(bytes);
        err = nvs_get_blob(handle, i ? "pet_b" : "pet_a", bytes, &size);
        if (err == ESP_ERR_NVS_NOT_FOUND) continue;
        pet_progress_t candidate;
        uint32_t gen;
        if (err != ESP_OK || !pet_save_decode(bytes, size, &candidate, &gen)) {
            invalid = true;
            continue;
        }
        if (!*had_save || pet_save_newer(gen, generation)) {
            generation = gen;
            *restored = candidate;
            *had_save = true;
        }
    }
    if (invalid && !*had_save) {
        ESP_LOGE("pet_store", "Existing saves unreadable/unsupported; writes disabled, data retained");
        nvs_close(handle);
        return false;
    }
    queue = xQueueCreate(1, sizeof(save_request_t));
    if (!queue || xTaskCreate(writer, "pet_save", 4096, NULL, 3, NULL) != pdPASS) {
        if (queue) vQueueDelete(queue);
        queue = NULL;
        nvs_close(handle);
        ESP_LOGE("pet_store", "Cannot start save worker");
        return false;
    }
    __atomic_store_n(&failed, false, __ATOMIC_RELEASE);
    ESP_LOGI("pet_store", "%s gen=%" PRIu32 " satiety=%u xp=%u meals=%" PRIu32 "%s",
             *had_save ? "restored" : "new pet", generation, restored->satiety,
             restored->growth_xp, restored->meals, invalid ? " (recovered from alternate slot)" : "");
    return true;
}

bool pet_store_request(const pet_progress_t *progress)
{
    if (!queue || !pet_progress_valid(progress)) return false;
    save_request_t request = {.progress = *progress,
        .request = __atomic_add_fetch(&requested, 1, __ATOMIC_ACQ_REL)};
    return xQueueOverwrite(queue, &request) == pdPASS;
}

pet_store_status_t pet_store_status(void)
{
    if (__atomic_load_n(&failed, __ATOMIC_ACQUIRE)) return PET_STORE_ERROR;
    return __atomic_load_n(&requested, __ATOMIC_ACQUIRE) ==
           __atomic_load_n(&completed, __ATOMIC_ACQUIRE) ? PET_STORE_SAVED : PET_STORE_SAVING;
}
