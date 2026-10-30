// SPDX-License-Identifier: Apache-2.0
#include "pet_touch.h"
#include "pet_model.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static pet_gesture_t tap(pet_touch_t *t, uint32_t start)
{
    assert(pet_touch_down(t, 230, 235, start) == PET_TOUCH_NONE);
    return pet_touch_up(t, 230, 235, start + 40);
}
int main(void)
{
    pet_touch_t t = {0};
    assert(tap(&t, 0) == PET_TOUCH_NONE);
    assert(pet_touch_tick(&t, 359) == PET_TOUCH_NONE);
    assert(pet_touch_tick(&t, 360) == PET_TOUCH_HEAD);
    assert(pet_touch_tick(&t, 999) == PET_TOUCH_NONE);
    pet_touch_cancel(&t);
    tap(&t, 0); tap(&t, 120);
    assert(pet_touch_tick(&t, 479) == PET_TOUCH_NONE);
    assert(pet_touch_tick(&t, 480) == PET_TOUCH_PLAY);
    pet_touch_cancel(&t);
    tap(&t, 0); tap(&t, 120);
    assert(tap(&t, 240) == PET_TOUCH_TICKLE);
    assert(tap(&t, 360) == PET_TOUCH_NONE);
    assert(pet_touch_tick(&t, 900) == PET_TOUCH_NONE); // Four taps cannot leak another head pat.
    assert(tap(&t, 1000) == PET_TOUCH_NONE);
    assert(pet_touch_tick(&t, 1360) == PET_TOUCH_HEAD);
    pet_touch_cancel(&t);
    tap(&t, 0);
    assert(pet_touch_down(&t, 230, 235, 360) == PET_TOUCH_HEAD); // Exact gap boundary starts a new sequence.
    pet_touch_cancel(&t);
    assert(pet_touch_down(&t, 230, 235, 0) == PET_TOUCH_NONE);
    assert(pet_touch_tick(&t, PET_TOUCH_HOLD_MS - 1) == PET_TOUCH_NONE);
    assert(pet_touch_tick(&t, PET_TOUCH_HOLD_MS) == PET_TOUCH_HUG);
    assert(pet_touch_tick(&t, 1000) == PET_TOUCH_NONE);
    assert(pet_touch_up(&t, 230, 235, 1100) == PET_TOUCH_NONE);
    assert(pet_touch_tick(&t, 1600) == PET_TOUCH_NONE);
    pet_touch_cancel(&t);
    pet_touch_down(&t, 230, 235, 0);
    assert(pet_touch_up(&t, 230, 235, 700) == PET_TOUCH_HUG); // Release fallback without a periodic tick.
    pet_touch_cancel(&t);
    pet_touch_down(&t, 230, 235, 0); pet_touch_move(&t, 251, 235);
    assert(pet_touch_tick(&t, 700) == PET_TOUCH_NONE); // A moving finger is not a hug.
    assert(pet_touch_up(&t, 240, 235, 800) == PET_TOUCH_NONE); // Out-and-back drag is not a tap.
    const int endpoints[][2] = {{160,235}, {300,235}, {230,160}, {230,310}, {300,305}};
    const pet_gesture_t expected[] = {PET_TOUCH_TEASE_LEFT, PET_TOUCH_TEASE_RIGHT, PET_TOUCH_HIGH_FIVE, PET_TOUCH_NONE, PET_TOUCH_NONE};
    for (unsigned i = 0; i < 5; ++i) {
        pet_touch_cancel(&t); pet_touch_down(&t, 230, 235, 0);
        assert(pet_touch_up(&t, endpoints[i][0], endpoints[i][1], 240) == expected[i]);
        assert(pet_touch_tick(&t, 999) == PET_TOUCH_NONE);
    }
    pet_touch_cancel(&t); tap(&t, 0); pet_touch_cancel(&t);
    assert(pet_touch_tick(&t, 999) == PET_TOUCH_NONE);
    pet_touch_cancel(&t);
    assert(tap(&t, UINT32_MAX-50) == PET_TOUCH_NONE);
    assert(pet_touch_tick(&t, 309) == PET_TOUCH_HEAD); // Timer wraparound.

    pet_model_t pet;
    pet_model_init(&pet, 0);
    for (int emote = PET_EMOTE_HEAD; emote <= PET_EMOTE_HIGH_FIVE; ++emote) {
        assert(pet_model_interact(&pet, emote, emote*100) == PET_ACTION_OK);
        assert(pet.emote == (pet_emote_t)emote && pet.progress.growth_xp == 1);
        assert(pet.progress.quest_flags == PET_QUEST_STROKE);
        pet_pose_t p = pet_model_pose(&pet, emote*100+200);
        assert(isfinite(p.bob) && fabsf(p.tail) <= 16.01f && fabsf(p.sway) <= 7.01f);
        assert(p.eyes >= 0 && p.eyes <= 1 && p.paw >= 0 && p.paw <= 1 && p.cuddle >= 0 && p.cuddle <= 1);
    }
    assert(pet_model_interact(&pet, PET_EMOTE_NONE, 800) == PET_ACTION_IGNORED);
    pet_model_act(&pet, PET_FEED, 1000);
    assert(pet_model_interact(&pet, PET_EMOTE_HUG, 1100) == PET_ACTION_IGNORED);
    pet_model_tick(&pet, 1000+PET_EATING_MS+PET_HAPPY_MS);
    assert(pet.emote == PET_EMOTE_NONE);
    pet_model_set_screen_off(&pet, true, 8000);
    assert(pet_model_interact(&pet, PET_EMOTE_HEAD, 8001) == PET_ACTION_IGNORED);
    pet_model_set_screen_off(&pet, false, 9000);
    assert(pet_model_play_start(&pet, 9000));
    assert(pet_model_interact(&pet, PET_EMOTE_TICKLE, 9100) == PET_ACTION_IGNORED);
    puts("PASS: 1/2/3 taps, 4-tap suppression, exact gap/hold, swipe/drag, cancel/wrap, affection variants/shared reward/busy/off guards");
}
