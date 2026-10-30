// SPDX-License-Identifier: Apache-2.0
#ifndef PET_DEVICE_H
#define PET_DEVICE_H
#include "pet_battery.h"
// Init after BSP display/I2C setup. Polls telemetry off the LVGL thread.
bool pet_device_init(void);
pet_battery_t pet_device_battery(void);
typedef struct { bool valid; int64_t utc_s; } pet_clock_t;
// ESP system time uses its RTC through light sleep; cold power loss needs calibration.
pet_clock_t pet_device_clock(void);
bool pet_device_set_clock(int64_t utc_s);
// One PWR short-press event, consumed on the LVGL thread.
bool pet_device_take_screen_key(void);
// Called by LVGL after the display/input transition; power worker owns sleep entry.
void pet_device_low_power(bool requested);
// Display emission only; the separate low-power worker suspends LVGL/touch and CPUs.
bool pet_device_screen(bool on);
#endif
