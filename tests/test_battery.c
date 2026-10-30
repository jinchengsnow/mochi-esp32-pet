// SPDX-License-Identifier: Apache-2.0
#include "pet_battery.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    // Independent register fixtures from AXP2101: battery present bit3, charge direction01.
    pet_battery_t b = pet_battery_decode(0x08, 0x40, 0x08, 9);
    assert(b.available && b.present && b.level_valid && b.percent == 9);
    assert(!b.usb && !b.charging && pet_battery_low(b));
    b = pet_battery_decode(0x08, 0x40, 0x08, 10);
    assert(!pet_battery_low(b));
    b = pet_battery_decode(0x08, 0x40, 0x08, 0);
    assert(b.level_valid && pet_battery_low(b));
    b = pet_battery_decode(0x28, 0x22, 0x08, 9);
    assert(b.usb && b.charging && b.percent == 9 && pet_battery_low(b));
    b = pet_battery_decode(0x28, 0x04, 0x08, 100);
    assert(b.usb && !b.charging && b.percent == 100 && b.level_valid);
    b = pet_battery_decode(0x20, 0x20, 0x08, 0);
    assert(!b.present && !b.charging && !b.level_valid && !pet_battery_low(b));
    b = pet_battery_decode(0x08, 0x40, 0x00, 0);
    assert(b.present && !b.level_valid && !pet_battery_low(b));
    b = pet_battery_decode(0x08, 0x40, 0x08, 101);
    assert(!b.level_valid && !pet_battery_low(b));
    b = pet_battery_decode(0x08, 0x40, 0x08, 255);
    assert(!b.level_valid && !pet_battery_low(b));
    assert(!pet_battery_low((pet_battery_t){0}));
    puts("PASS: battery telemetry, strict 10% boundary, zero/full, charge/USB, absent/disabled/invalid/unavailable");
}
