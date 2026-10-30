// SPDX-License-Identifier: Apache-2.0
// Hardware fixture for host rendering; never linked into firmware.
#include "device_stub.h"
#include "lvgl.h"
static pet_battery_t battery = {.available = true, .present = true, .level_valid = true, .percent = 82};
static unsigned pending_keys;
static bool screen_on = true, fail_next;
static pet_clock_t clock_value;
static uint32_t clock_tick;
pet_clock_t pet_device_clock(void)
{
    pet_clock_t result = clock_value;
    if (result.valid) result.utc_s += (uint32_t)(lv_tick_get() - clock_tick) / 1000;
    return result;
}
bool pet_device_set_clock(int64_t utc_s)
{
    if (utc_s < INT64_C(1704067200) || utc_s >= INT64_C(4102444800)) return false;
    clock_value = (pet_clock_t){.valid = true, .utc_s = utc_s};
    clock_tick = lv_tick_get();
    return true;
}
bool pet_device_init(void) { return true; }
pet_battery_t pet_device_battery(void) { return battery; }
bool pet_device_screen(bool on)
{
    if (fail_next) { fail_next = false; return false; }
    screen_on = on;
    return true;
}
void pet_device_preview_battery(pet_battery_t value) { battery = value; }
void pet_device_preview_fail_screen(void) { fail_next = true; }
bool pet_device_preview_screen_on(void) { return screen_on; }

void pet_device_preview_press_key(void) { ++pending_keys; }
bool pet_device_take_screen_key(void)
{
    if (!pending_keys) return false;
    --pending_keys;
    return true;
}

static bool rest_requested;
void pet_device_low_power(bool requested) { rest_requested = requested; }
bool pet_device_preview_rest_requested(void) { return rest_requested; }
