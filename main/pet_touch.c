// SPDX-License-Identifier: Apache-2.0
#include "pet_touch.h"
static uint32_t magnitude(int32_t value) { return value < 0 ? (uint32_t)-value : (uint32_t)value; }
static pet_gesture_t taps_gesture(uint8_t taps)
{
    return taps == 1 ? PET_TOUCH_HEAD : taps == 2 ? PET_TOUCH_PLAY : PET_TOUCH_NONE;
}
void pet_touch_cancel(pet_touch_t *t) { *t = (pet_touch_t){0}; }
pet_gesture_t pet_touch_down(pet_touch_t *t, int16_t x, int16_t y, uint32_t now)
{
    if (t->pressed) return PET_TOUCH_NONE;
    pet_gesture_t previous = PET_TOUCH_NONE;
    if (t->taps && now - t->last_up >= PET_TOUCH_GAP_MS) {
        previous = taps_gesture(t->taps);
        t->taps = 0;
    }
    if (t->blocked && now - t->last_up >= PET_TOUCH_GAP_MS) t->blocked = false;
    t->pressed = true;
    t->claimed = t->blocked;
    t->start_x = t->x = x; t->start_y = t->y = y;
    t->down_at = now; t->travel = 0;
    return previous;
}
void pet_touch_move(pet_touch_t *t, int16_t x, int16_t y)
{
    if (!t->pressed) return;
    t->x = x; t->y = y;
    uint32_t dx = magnitude((int32_t)x - t->start_x), dy = magnitude((int32_t)y - t->start_y);
    if (dx > t->travel) t->travel = dx;
    if (dy > t->travel) t->travel = dy;
}
pet_gesture_t pet_touch_tick(pet_touch_t *t, uint32_t now)
{
    if (t->pressed) {
        if (!t->claimed && t->travel <= PET_TOUCH_SLOP && now - t->down_at >= PET_TOUCH_HOLD_MS) {
            t->claimed = true; t->taps = 0;
            return PET_TOUCH_HUG;
        }
        return PET_TOUCH_NONE;
    }
    if (t->blocked && now - t->last_up >= PET_TOUCH_GAP_MS) t->blocked = false;
    if (t->taps && now - t->last_up >= PET_TOUCH_GAP_MS) {
        pet_gesture_t result = taps_gesture(t->taps);
        t->taps = 0;
        return result;
    }
    return PET_TOUCH_NONE;
}
pet_gesture_t pet_touch_up(pet_touch_t *t, int16_t x, int16_t y, uint32_t now)
{
    if (!t->pressed) return PET_TOUCH_NONE;
    pet_touch_move(t, x, y);
    t->pressed = false;
    t->last_up = now;
    if (t->claimed) { t->taps = 0; return PET_TOUCH_NONE; }
    if (t->travel > PET_TOUCH_SLOP) {
        t->taps = 0;
        const int32_t dx = (int32_t)x - t->start_x, dy = (int32_t)y - t->start_y;
        const uint32_t ax = magnitude(dx), ay = magnitude(dy);
        if (ax >= PET_TOUCH_SWIPE && ax * 4 >= ay * 5)
            return dx < 0 ? PET_TOUCH_TEASE_LEFT : PET_TOUCH_TEASE_RIGHT;
        if (dy <= -PET_TOUCH_SWIPE && ay * 4 >= ax * 5) return PET_TOUCH_HIGH_FIVE;
        return PET_TOUCH_NONE; // A drag, downward swipe or diagonal is never an accidental tap.
    }
    if (now - t->down_at >= PET_TOUCH_HOLD_MS) {
        t->taps = 0;
        return PET_TOUCH_HUG;
    }
    if (++t->taps >= 3) {
        t->taps = 0; t->blocked = true; // Ignore the tail of a 4+ tap burst until it stops.
        return PET_TOUCH_TICKLE;
    }
    return PET_TOUCH_NONE;
}
