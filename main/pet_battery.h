// SPDX-License-Identifier: Apache-2.0
#ifndef PET_BATTERY_H
#define PET_BATTERY_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    bool available, present, charging, usb, level_valid;
    uint8_t percent;
} pet_battery_t;
// AXP2101 status0/status1/module-enable/SOC. See docs/power.md for source registers.
pet_battery_t pet_battery_decode(uint8_t status0, uint8_t status1, uint8_t modules, uint8_t percent);
bool pet_battery_low(pet_battery_t battery);
#endif
