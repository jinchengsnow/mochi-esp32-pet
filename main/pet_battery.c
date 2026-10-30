// SPDX-License-Identifier: Apache-2.0
#include "pet_battery.h"
pet_battery_t pet_battery_decode(uint8_t status0, uint8_t status1, uint8_t modules, uint8_t percent)
{
    pet_battery_t b = {.available = true, .present = (status0 & 0x08) != 0,
                       .usb = (status0 & 0x20) != 0};
    b.charging = b.present && ((status1 >> 5) & 3) == 1;
    b.level_valid = b.present && (modules & 0x08) != 0 && percent <= 100;
    if (b.level_valid) b.percent = percent;
    return b;
}
bool pet_battery_low(pet_battery_t b)
{
    return b.available && b.present && b.level_valid && b.percent < 10;
}
