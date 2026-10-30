// SPDX-License-Identifier: Apache-2.0
#ifndef PET_BOOT_KEY_H
#define PET_BOOT_KEY_H
#include <stdbool.h>
#include <stdint.h>
enum { PET_BOOT_DEBOUNCE_MS = 30 };
typedef struct {
    bool raw_down, stable_down, ready, pressed;
    uint32_t changed_at;
} pet_boot_key_t;
void pet_boot_key_init(pet_boot_key_t *key, bool down, uint32_t now);
// One event on debounced release. A key held at startup must first be released.
bool pet_boot_key_sample(pet_boot_key_t *key, bool down, uint32_t now);
bool pet_boot_key_busy(const pet_boot_key_t *key);
#endif
