// SPDX-License-Identifier: Apache-2.0
#include "pet_model.h"
#include <assert.h>
#include <stdio.h>
static void same_progress(const pet_model_t *a, const pet_model_t *b)
{
    assert(a->progress.satiety == b->progress.satiety && a->progress.growth_xp == b->progress.growth_xp);
    assert(a->progress.hunger_ms == b->progress.hunger_ms && a->progress.growth_ms == b->progress.growth_ms);
    assert(a->progress.active_ms == b->progress.active_ms);
    assert(a->progress.play_wait_ms == b->progress.play_wait_ms && a->progress.stroke_wait_ms == b->progress.stroke_wait_ms);
}
int main(void)
{
    pet_model_t bright, dark, split;
    pet_model_init(&bright, 0);
    pet_model_tick(&bright, 3600000);
    assert(bright.progress.satiety == 58 && bright.progress.growth_xp == 6);
    assert(bright.state == PET_IDLE); // Daytime bright idle stays awake.
    pet_model_init(&dark, 0);
    pet_model_set_screen_off(&dark, true, 0);
    pet_model_advance_sleep(&dark, 3600000, 123);
    assert(dark.progress.satiety == 68 && dark.progress.growth_xp == 1);
    assert(dark.progress.active_ms == 3600000 && dark.last_tick == 123);
    pet_model_set_screen_off(&dark, false, 123);
    pet_model_tick(&dark, 3600123);
    assert(dark.progress.satiety == 56 && dark.progress.growth_xp == 7);

    pet_model_init(&dark, 0);
    pet_model_set_screen_off(&dark, true, 0);
    pet_model_init(&split, 0);
    pet_model_set_screen_off(&split, true, 0);
    for (unsigned i = 0; i < 7200; ++i) pet_model_advance_sleep(&split, 500, 0);
    pet_model_advance_sleep(&dark, 3600000, 0);
    same_progress(&split, &dark); // Periodic wakeups and one bulk calculation are identical.

    pet_model_init(&dark, 0);
    pet_model_tick(&dark, 240000); // Four bright minutes = 80% hunger, 40% growth.
    pet_model_set_screen_off(&dark, true, 240000);
    pet_model_advance_sleep(&dark, 360000, 240000); // Six rest minutes complete hunger.
    assert(dark.progress.satiety == 69 && dark.progress.growth_xp == 0);
    pet_model_set_screen_off(&dark, false, 240000);
    pet_model_tick(&dark, 540000); // Five bright minutes complete growth.
    assert(dark.progress.satiety == 68 && dark.progress.growth_xp == 1);

    pet_model_init(&dark, 0);
    for (unsigned i = 0; i < 100; ++i) {
        pet_model_set_screen_off(&dark, false, i * 200);
        pet_model_set_screen_off(&dark, true, i * 200 + 100);
        pet_model_tick(&dark, i * 200 + 200);
    }
    assert(dark.progress.hunger_ms == 70000 && dark.progress.growth_ms == 70000);

    pet_model_init(&dark, UINT32_MAX - 100);
    pet_model_set_screen_off(&dark, true, UINT32_MAX - 100);
    pet_model_advance_sleep(&dark, 3600000, 50);
    assert(dark.progress.growth_xp == 1 && dark.progress.satiety == 68);
    pet_model_advance_sleep(&dark, UINT64_MAX, 50);
    assert(dark.progress.growth_xp == 600 && dark.progress.satiety == 0);
    assert(dark.progress.active_ms == UINT64_MAX && pet_progress_valid(&dark.progress));

    pet_model_init(&dark, 0);
    assert(pet_model_play_start(&dark, 0));
    for (unsigned i = 0; i < 10; ++i) assert(pet_model_play_hit(&dark, dark.game.target, i*180));
    pet_model_set_screen_off(&dark, true, 3000);
    pet_model_advance_sleep(&dark, 3600000, 3000);
    assert(!dark.game.active && dark.progress.games == 1 && dark.progress.play_wait_ms == 0);
    assert(dark.progress.growth_xp == 7 && dark.game.reward_xp == 6); // Rest growth isn't a game reward.
    assert(dark.progress.stars == 4 && dark.progress.satiety == 63);
    pet_model_advance_sleep(&dark, 0, 3000);
    assert(dark.progress.games == 1 && dark.progress.growth_xp == 7);
    pet_model_init(&dark, 0);
    dark.progress.growth_xp = 599;
    assert(pet_model_play_start(&dark, 0));
    for (unsigned i = 0; i < 5; ++i) assert(pet_model_play_hit(&dark, dark.game.target, i*180));
    pet_model_set_screen_off(&dark, true, 3000);
    pet_model_advance_sleep(&dark, 3600000, 3000);
    assert(dark.progress.growth_xp == 600 && dark.game.reward_xp == 1);
    puts("PASS: bright/rest 6x rates, fractional mode switches, 500ms vs bulk, clock rebase/wrap, long sleep/caps, one game settlement");
}
