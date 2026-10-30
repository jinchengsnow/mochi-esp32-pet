// SPDX-License-Identifier: Apache-2.0
#ifndef PET_DEVICE_STUB_H
#define PET_DEVICE_STUB_H
#include "pet_device.h"
void pet_device_preview_battery(pet_battery_t battery);
void pet_device_preview_fail_screen(void);
void pet_device_preview_press_key(void);
bool pet_device_preview_screen_on(void);
bool pet_device_preview_rest_requested(void);
#endif
