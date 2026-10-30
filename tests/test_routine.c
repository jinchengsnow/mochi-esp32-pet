// SPDX-License-Identifier: Apache-2.0
#include "pet_model.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int64_t local_time(int64_t day, unsigned minute)
{
    return day * 86400 + minute * 60 - PET_LOCAL_OFFSET_S;
}

int main(void)
{
    pet_routine_plan_t plan, again, next;
    unsigned different = 0;
    for (int64_t day = 20756; day < 21156; ++day) {
        assert(pet_routine_at(local_time(day, 12 * 60), &plan) == PET_ROUTINE_DAY);
        assert(plan.day == day && plan.wake >= 420 && plan.wake <= 480);
        assert(plan.bedtime >= 1305 && plan.bedtime <= 1335);
        assert(plan.nap_start >= 780 && plan.nap_start <= 795);
        assert(plan.nap_end >= 825 && plan.nap_end <= 840);
        assert(plan.nap_end - plan.nap_start >= 30 && plan.nap_end - plan.nap_start <= 60);
        pet_routine_at(local_time(day, 12 * 60), &again);
        assert(plan.wake == again.wake && plan.bedtime == again.bedtime &&
               plan.nap_start == again.nap_start && plan.nap_end == again.nap_end);
        assert(pet_routine_at(local_time(day, plan.wake) - 1, &again) == PET_ROUTINE_NIGHT);
        assert(pet_routine_at(local_time(day, plan.wake), &again) == PET_ROUTINE_DAY);
        assert(pet_routine_at(local_time(day, plan.nap_start) - 1, &again) == PET_ROUTINE_DAY);
        assert(pet_routine_at(local_time(day, plan.nap_start), &again) == PET_ROUTINE_NAP);
        assert(pet_routine_at(local_time(day, plan.nap_end) - 1, &again) == PET_ROUTINE_NAP);
        assert(pet_routine_at(local_time(day, plan.nap_end), &again) == PET_ROUTINE_DAY);
        assert(pet_routine_at(local_time(day, plan.bedtime) - 1, &again) == PET_ROUTINE_DAY);
        assert(pet_routine_at(local_time(day, plan.bedtime), &again) == PET_ROUTINE_NIGHT);
        assert(pet_routine_at(local_time(day, 0), &again) == PET_ROUTINE_NIGHT);
        pet_routine_at(local_time(day + 1, 0), &next);
        different += plan.wake != next.wake || plan.bedtime != next.bedtime;
    }
    assert(different > 350);
    const int64_t day = 20756;
    pet_routine_at(local_time(day, 0), &plan);
    pet_model_t p;
    pet_model_init(&p, 0);
    pet_model_update_clock(&p, false, 0, 60001);
    assert(p.state == PET_IDLE && p.routine == PET_ROUTINE_UNSET);
    // Bedtime while eating waits for the meal and its happy animation.
    pet_model_init(&p, 0);
    pet_model_update_clock(&p, true, local_time(day, plan.bedtime) - 1, 0);
    assert(pet_model_act(&p, PET_FEED, 0) == PET_ACTION_OK);
    pet_model_update_clock(&p, true, local_time(day, plan.bedtime), 1000);
    assert(p.state == PET_EATING);
    pet_model_update_clock(&p, true, local_time(day, plan.bedtime) + 6, 7000);
    assert(p.state == PET_SLEEPING && p.routine == PET_ROUTINE_NIGHT);
    assert(pet_model_interact(&p, PET_EMOTE_HUG, 7000) == PET_ACTION_OK);
    const int64_t until = p.awake_until_s;
    pet_model_update_clock(&p, true, p.utc_s + 3, 10000);
    assert(p.state == PET_IDLE);
    // Regular clock samples preserve the ten-minute override.
    uint32_t now = 10000;
    while (p.utc_s < until - 1) {
        pet_model_update_clock(&p, true, p.utc_s + 1, ++now);
        assert(p.state == PET_IDLE);
    }
    pet_model_update_clock(&p, true, until, ++now);
    assert(p.state == PET_SLEEPING);
    // Midnight stays asleep; next day's wake is derived from that day's plan.
    pet_model_update_clock(&p, true, local_time(day + 1, 0), ++now);
    assert(p.state == PET_SLEEPING);
    pet_routine_at(local_time(day + 1, 0), &next);
    pet_model_update_clock(&p, true, local_time(day + 1, next.wake), ++now);
    assert(p.state == PET_IDLE && !p.progress.sleeping);
    // Manual daytime sleep survives ordinary samples, ends at nap's wake boundary.
    pet_model_act(&p, PET_TOGGLE_SLEEP, ++now);
    pet_model_update_clock(&p, true, p.utc_s + 1, ++now);
    assert(p.state == PET_SLEEPING && p.manual_sleep);
    pet_model_update_clock(&p, true, local_time(day + 1, next.nap_start), ++now);
    assert(p.state == PET_SLEEPING && p.routine == PET_ROUTINE_NAP);
    pet_model_update_clock(&p, true, local_time(day + 1, next.nap_end), ++now);
    assert(p.state == PET_IDLE);
    // Clock correction cannot leave an old override active for hours.
    pet_model_update_clock(&p, true, local_time(day, plan.bedtime), ++now);
    pet_model_act(&p, PET_TOGGLE_SLEEP, ++now);
    assert(p.state == PET_IDLE && p.awake_until_s > p.utc_s);
    pet_model_update_clock(&p, true, local_time(day, 5 * 60), ++now);
    assert(p.state == PET_SLEEPING && !p.awake_until_s);
    // Menu/low-power schedule updates never require or switch screen emission.
    pet_model_set_screen_off(&p, true, now);
    pet_model_advance_sleep(&p, 3 * 3600000ULL, now);
    pet_model_update_clock(&p, true, local_time(day, 8 * 60), now);
    assert(p.screen_off && p.state == PET_IDLE && p.progress.growth_xp >= 3);
    // A current game finishes once at bedtime before the role falls asleep.
    pet_model_init(&p, 0);
    pet_model_update_clock(&p, true, local_time(day, plan.bedtime) - 1, 0);
    assert(pet_model_play_start(&p, 0));
    pet_model_update_clock(&p, true, local_time(day, plan.bedtime), 1000);
    assert(p.game.active);
    pet_model_update_clock(&p, true, local_time(day, plan.bedtime) + 15, 16000);
    assert(!p.game.active && p.progress.games == 1 && p.state == PET_SLEEPING);
    // Each autonomous activity appears, animates, ends, and gives no quest/reward.
    pet_model_init(&p, UINT32_MAX - 1000);
    now = p.last_tick;
    pet_model_update_clock(&p, true, local_time(day, 10 * 60), now);
    unsigned seen = 0;
    pet_ambient_t previous_ambient = PET_AMBIENT_NONE;
    for (unsigned i = 0; i < 100 && seen != 15; ++i) {
        now += 26000;
        pet_model_ambient_tick(&p, now, true);
        assert(p.ambient != PET_AMBIENT_NONE && p.ambient != previous_ambient);
        const pet_ambient_t current = p.ambient;
        previous_ambient = current;
        seen |= 1u << (current - 1);
        pet_pose_t first = pet_model_pose(&p, now + 500), second = pet_model_pose(&p, now + 1800);
        assert(isfinite(first.bob) && isfinite(first.paw) && isfinite(second.gaze));
        assert(first.bob != second.bob || first.paw != second.paw || first.gaze != second.gaze);
        pet_model_ambient_tick(&p, now + PET_AMBIENT_MS, true);
        assert(p.ambient == PET_AMBIENT_NONE);
        now += PET_AMBIENT_MS;
    }
    assert(seen == 15 && p.progress.growth_xp == 0 && p.progress.stars == 0 && !p.progress.quest_flags);
    now += 26000; pet_model_ambient_tick(&p, now, true);
    pet_model_ambient_tick(&p, now, false);
    assert(p.ambient == PET_AMBIENT_NONE);
    pet_model_set_screen_off(&p, true, now);
    pet_model_ambient_tick(&p, now + 26000, true);
    assert(p.ambient == PET_AMBIENT_NONE);
    pet_model_set_screen_off(&p, false, now);
    pet_model_update_clock(&p, true, local_time(day, plan.nap_start), now);
    pet_model_ambient_tick(&p, now + 26000, true);
    assert(p.ambient == PET_AMBIENT_NONE && p.state == PET_SLEEPING);
    puts("PASS: 400-day UTC+8 schedule ranges/stability/boundaries, midnight/wake/nap, grace and calibration, manual sleep, busy-game/meal precedence, dark schedule progression, four ambient animations/no rewards/input/menu/off suppression/tick wrap");
}
