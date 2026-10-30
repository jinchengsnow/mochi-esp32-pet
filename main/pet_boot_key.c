// SPDX-License-Identifier: Apache-2.0
#include "pet_boot_key.h"
void pet_boot_key_init(pet_boot_key_t *key, bool down, uint32_t now)
{
    *key = (pet_boot_key_t){.raw_down = down, .stable_down = down,
                           .ready = !down, .changed_at = now};
}
bool pet_boot_key_sample(pet_boot_key_t *key, bool down, uint32_t now)
{
    if (down != key->raw_down) { key->raw_down = down; key->changed_at = now; }
    if (down == key->stable_down || now - key->changed_at < PET_BOOT_DEBOUNCE_MS) return false;
    key->stable_down = down;
    if (down) { key->pressed = key->ready; return false; }
    const bool event = key->ready && key->pressed;
    key->pressed = false;
    key->ready = true;
    return event;
}
bool pet_boot_key_busy(const pet_boot_key_t *key)
{
    return key->raw_down || key->stable_down || key->pressed;
}
