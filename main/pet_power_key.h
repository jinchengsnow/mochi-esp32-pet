// SPDX-License-Identifier: Apache-2.0
#ifndef PET_POWER_KEY_H
#define PET_POWER_KEY_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    void *context;
    bool (*read)(void *context, uint8_t reg, uint8_t *value);
    bool (*write)(void *context, uint8_t reg, uint8_t value);
} pet_power_key_io_t;
// AXP2101 PWR short press. Only enable bit3; clear only its W1C status bit.
bool pet_power_key_init(const pet_power_key_io_t *io);
// Returns 1 for an acknowledged short press, 0 for none, -1 for an I/O error.
int pet_power_key_poll(const pet_power_key_io_t *io);
#endif
