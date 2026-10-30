// SPDX-License-Identifier: Apache-2.0
#ifndef PET_TOUCH_H
#define PET_TOUCH_H
#include <stdbool.h>
#include <stdint.h>
typedef enum {
    PET_TOUCH_NONE, PET_TOUCH_HEAD, PET_TOUCH_PLAY, PET_TOUCH_TICKLE,
    PET_TOUCH_HUG, PET_TOUCH_TEASE_LEFT, PET_TOUCH_TEASE_RIGHT, PET_TOUCH_HIGH_FIVE
} pet_gesture_t;
enum { PET_TOUCH_GAP_MS = 320, PET_TOUCH_HOLD_MS = 650,
       PET_TOUCH_SLOP = 16, PET_TOUCH_SWIPE = 60 };
typedef struct {
    int16_t start_x, start_y, x, y;
    uint32_t down_at, last_up;
    uint32_t travel;
    uint8_t taps;
    bool pressed, claimed, blocked;
} pet_touch_t;
void pet_touch_cancel(pet_touch_t *touch);
pet_gesture_t pet_touch_down(pet_touch_t *touch, int16_t x, int16_t y, uint32_t now);
void pet_touch_move(pet_touch_t *touch, int16_t x, int16_t y);
pet_gesture_t pet_touch_up(pet_touch_t *touch, int16_t x, int16_t y, uint32_t now);
pet_gesture_t pet_touch_tick(pet_touch_t *touch, uint32_t now);
#endif
