// SPDX-License-Identifier: Apache-2.0
#include "pet_model.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void hit_n(pet_model_t *p, unsigned n, uint32_t start)
{
    for (unsigned i = 0; i < n; ++i)
        assert(pet_model_play_hit(p, p->game.target, start + i * PET_PLAY_HIT_MS));
}

static void test_play(void)
{
    pet_model_t p;
    pet_model_init(&p, 0);
    assert(pet_model_play_start(&p, 0));
    assert(p.progress.satiety == 65 && p.game.active && p.game.target == 0);
    assert(p.progress.play_wait_ms == PET_PLAY_MS + PET_PLAY_WAIT_MS);
    assert(!pet_model_play_start(&p, 0));
    assert(p.progress.satiety == 65);
    assert(pet_model_act(&p, PET_FEED, 0) == PET_ACTION_IGNORED);
    assert(pet_model_act(&p, PET_TOGGLE_SLEEP, 0) == PET_ACTION_IGNORED);
    assert(pet_model_play_hit(&p, 0, 0)); // First hit can be immediate.
    assert(p.game.target == 2 && p.game.hits == 1 && p.game.combo == 1);
    assert(!pet_model_play_hit(&p, 2, PET_PLAY_HIT_MS - 1));
    assert(p.game.hits == 1 && p.game.combo == 1);
    assert(pet_model_play_hit(&p, 2, PET_PLAY_HIT_MS));
    assert(!pet_model_play_hit(&p, 255, PET_PLAY_HIT_MS));
    assert(p.game.hits == 2 && p.game.combo == 0 && p.game.best_combo == 2);
    hit_n(&p, 3, 2 * PET_PLAY_HIT_MS);
    pet_model_tick(&p, PET_PLAY_MS - 1);
    assert(p.game.active && p.progress.growth_xp == 0 && p.progress.stars == 0);
    assert(!pet_model_play_hit(&p, p.game.target, PET_PLAY_MS)); // Late tap cannot score.
    assert(!p.game.active && p.game.hits == 5 && p.game.best_combo == 3);
    assert(p.progress.growth_xp == 3 && p.progress.stars == 2 && p.progress.games == 1);
    assert(p.game.reward_xp == 3 && p.game.reward_stars == 2);
    assert(p.progress.quest_flags == PET_QUEST_PLAY && p.progress.play_wait_ms == PET_PLAY_WAIT_MS);
    pet_model_tick(&p, PET_PLAY_MS);
    pet_model_play_cancel(&p, PET_PLAY_MS);
    assert(p.progress.games == 1 && p.progress.growth_xp == 3);
    assert(!pet_model_play_start(&p, PET_PLAY_MS + PET_PLAY_WAIT_MS - 1));
    assert(pet_model_play_start(&p, PET_PLAY_MS + PET_PLAY_WAIT_MS));
    const uint32_t second = p.game.started;
    hit_n(&p, 10, second);
    pet_model_tick(&p, second + PET_PLAY_MS + 1000);
    assert(p.progress.games == 2 && p.progress.growth_xp == 9 && p.progress.stars == 6);
    assert(p.game.reward_xp == 6 && p.game.reward_stars == 4);
    assert(p.progress.play_wait_ms == PET_PLAY_WAIT_MS - 1000);

    // Cancel consumes food and starts cooldown, but cannot award or finish again.
    pet_model_init(&p, 0);
    assert(pet_model_play_start(&p, 0));
    hit_n(&p, 10, 0);
    pet_model_play_cancel(&p, 2000);
    assert(!p.game.active && p.progress.games == 0 && p.progress.play_wait_ms == PET_PLAY_WAIT_MS);
    assert(p.progress.growth_xp == 0 && p.progress.stars == 0 && p.progress.satiety == 65);
    pet_model_tick(&p, PET_PLAY_MS);
    assert(p.progress.games == 0);

    // Reboot mid-game aborts the session and preserves the cooldown reservation.
    pet_model_init(&p, 0);
    assert(pet_model_play_start(&p, 0));
    hit_n(&p, 10, 0);
    pet_model_tick(&p, 2000);
    const pet_progress_t saved = p.progress;
    assert(pet_model_restore(&p, &saved, 0));
    assert(!p.game.active && p.game.hits == 0 && p.progress.play_wait_ms == 73000);
    assert(!pet_model_play_start(&p, 72999));
    assert(pet_model_play_start(&p, 73000));

    // Failed rounds and starvation gates never award.
    pet_model_init(&p, 0);
    p.progress.satiety = 9;
    assert(!pet_model_play_start(&p, 0));
    assert(p.progress.satiety == 9);
    p.progress.satiety = 10;
    pet_model_act(&p, PET_TOGGLE_SLEEP, 0);
    assert(pet_model_play_start(&p, 0));
    assert(!p.progress.sleeping && p.progress.satiety == 5);
    hit_n(&p, 4, 0);
    pet_model_tick(&p, PET_PLAY_MS);
    assert(p.progress.games == 1 && p.progress.growth_xp == 0 && p.progress.stars == 0);
    assert(p.progress.quest_flags == 0);
    pet_model_init(&p, 0);
    pet_model_act(&p, PET_FEED, 0);
    assert(!pet_model_play_start(&p, 1));

    const uint32_t wrap_start = UINT32_MAX - 1000;
    pet_model_init(&p, wrap_start);
    assert(pet_model_play_start(&p, wrap_start));
    hit_n(&p, 10, wrap_start);
    pet_model_tick(&p, wrap_start + PET_PLAY_MS);
    assert(!p.game.active && p.progress.growth_xp == 6);
    assert(p.progress.play_wait_ms == PET_PLAY_WAIT_MS && p.progress.games == 1);
    pet_model_tick(&p, wrap_start + PET_PLAY_MS + PET_PLAY_WAIT_MS);
    assert(p.progress.play_wait_ms == 0);
}

static void test_quest_shop_and_caps(void)
{
    pet_model_t p;
    pet_model_init(&p, 0);
    pet_model_act(&p, PET_STROKE, 0);
    pet_model_act(&p, PET_FEED, 0);
    assert(p.progress.quest_flags == (PET_QUEST_STROKE | PET_QUEST_FEED));
    assert(pet_model_play_start(&p, 6000));
    hit_n(&p, 5, 6000);
    pet_model_tick(&p, 6000 + PET_PLAY_MS);
    assert(p.progress.growth_xp == 14 && p.progress.stars == 7);
    assert(p.progress.quest_flags == 0 && p.game.reward_xp == 9 && p.game.reward_stars == 7);
    pet_model_tick(&p, 6000 + PET_PLAY_MS);
    pet_model_act(&p, PET_STROKE, 6001 + PET_PLAY_MS);
    assert(p.progress.quest_flags == 0 && p.progress.growth_xp == 14);

    pet_model_init(&p, 0);
    p.progress.stars = 24;
    assert(!pet_model_accessory(&p, 1, 0)); // Need youth stage.
    p.progress.growth_xp = 20;
    assert(!pet_model_accessory(&p, 2, 0));
    assert(pet_model_accessory(&p, 1, 0));
    assert(p.progress.stars == 16 && p.progress.accessory == 1 && p.progress.unlocked == 1);
    assert(pet_model_accessory(&p, 1, 0) && p.progress.stars == 16);
    assert(pet_model_accessory(&p, 0, 0) && p.progress.accessory == 0);
    assert(pet_model_accessory(&p, 1, 0) && p.progress.stars == 16);
    p.progress.growth_xp = 60;
    assert(pet_model_accessory(&p, 2, 0));
    assert(p.progress.stars == 0 && p.progress.accessory == 2 && p.progress.unlocked == 3);
    assert(pet_model_accessory(&p, 1, 0) && p.progress.stars == 0);
    assert(!pet_model_accessory(&p, 3, 0));
    assert(pet_progress_valid(&p.progress));
    p.progress.unlocked = 0; p.progress.accessory = 0;
    assert(!pet_model_accessory(&p, 2, 0)); // Cost cannot underflow.

    p.progress.growth_xp = PET_MAX_XP - 1;
    p.progress.stars = PET_MAX_STARS - 1;
    p.progress.quest_flags = PET_QUEST_STROKE | PET_QUEST_FEED;
    p.progress.games = UINT32_MAX;
    assert(pet_model_play_start(&p, 0));
    hit_n(&p, 10, 0);
    pet_model_tick(&p, PET_PLAY_MS);
    assert(p.progress.growth_xp == PET_MAX_XP && p.progress.stars == PET_MAX_STARS);
    assert(p.game.reward_xp == 1 && p.game.reward_stars == 1 && p.progress.quest_flags == 0);
    assert(p.progress.games == UINT32_MAX && pet_progress_valid(&p.progress));
    assert(pet_model_level(&p) == 10);
    p.progress.growth_xp = 0; assert(pet_model_level(&p) == 1);
    p.progress.growth_xp = 59; assert(pet_model_level(&p) == 1);
    p.progress.growth_xp = 60; assert(pet_model_level(&p) == 2);
    p.progress.growth_xp = 539; assert(pet_model_level(&p) == 9);
    p.progress.growth_xp = 540; assert(pet_model_level(&p) == 10);
}

int main(void)
{
    test_play();
    test_quest_shop_and_caps();
    pet_model_t p;
    pet_model_init(&p, 100);
    assert(p.state == PET_IDLE && p.progress.satiety == 70);
    assert(pet_model_act(&p, PET_STROKE, 200) == PET_ACTION_OK);
    assert(p.state == PET_HAPPY && p.progress.growth_xp == 1);
    pet_model_tick(&p, 200 + PET_HAPPY_MS - 1);
    assert(p.state == PET_HAPPY);
    pet_model_tick(&p, 200 + PET_HAPPY_MS);
    assert(p.state == PET_IDLE);
    pet_model_act(&p, PET_STROKE, 2700);
    assert(p.progress.growth_xp == 1);
    assert(pet_model_act(&p, PET_FEED, 3000) == PET_ACTION_OK);
    assert(p.state == PET_EATING && p.progress.satiety == 95);
    assert(p.progress.growth_xp == 5 && p.progress.meals == 1);
    for (uint32_t t = 3100; t < 6500; t += 10) {
        assert(pet_model_act(&p, PET_FEED, t) == PET_ACTION_IGNORED);
        assert(pet_model_act(&p, PET_STROKE, t) == PET_ACTION_IGNORED);
        assert(p.state_since == 3000 && p.progress.growth_xp == 5);
    }
    pet_model_tick(&p, 6600);
    assert(p.state == PET_HAPPY);
    pet_model_tick(&p, 9000);
    assert(p.state == PET_IDLE);
    assert(pet_model_act(&p, PET_FEED, 10000) == PET_ACTION_FULL);
    assert(p.progress.meals == 1 && p.progress.satiety == 95 && p.progress.growth_xp == 5);
    pet_model_tick(&p, 70000);
    assert(p.state == PET_IDLE && !p.progress.sleeping); // Bright idle no longer sleeps after a minute.
    pet_model_act(&p, PET_STROKE, 71000);
    assert(p.state == PET_HAPPY && !p.progress.sleeping);
    pet_model_act(&p, PET_TOGGLE_SLEEP, 71100);
    assert(p.state == PET_SLEEPING);
    pet_model_act(&p, PET_TOGGLE_SLEEP, 71200);
    assert(p.state == PET_IDLE);

    pet_model_init(&p, 0);
    pet_model_tick(&p, PET_HUNGER_STEP_MS - 1);
    assert(p.progress.satiety == 70);
    pet_model_tick(&p, PET_HUNGER_STEP_MS);
    assert(p.progress.satiety == 69 && p.progress.hunger_ms == 0);
    assert(p.progress.active_ms == PET_HUNGER_STEP_MS);
    pet_model_tick(&p, PET_HUNGER_STEP_MS * 80u);
    assert(p.progress.satiety == 0);
    pet_model_act(&p, PET_FEED, PET_HUNGER_STEP_MS * 80u);
    assert(p.progress.satiety == 25 && p.progress.growth_xp == 44);

    pet_progress_t checkpoint = p.progress;
    checkpoint.growth_xp = 19;
    checkpoint.stroke_wait_ms = 0;
    checkpoint.hunger_ms = PET_REST_HUNGER_MS - 60;
    checkpoint.active_ms = UINT64_MAX - 1;
    assert(pet_model_restore(&p, &checkpoint, 500));
    assert(pet_model_growth(&p) == PET_BABY);
    pet_model_act(&p, PET_STROKE, 510);
    assert(p.progress.growth_xp == 20 && pet_model_growth(&p) == PET_YOUNG);
    assert(p.progress.satiety == 24 && p.progress.active_ms == UINT64_MAX);
    checkpoint = p.progress;
    checkpoint.growth_xp = 59;
    checkpoint.stroke_wait_ms = 0;
    checkpoint.sleeping = true;
    assert(pet_model_restore(&p, &checkpoint, 1000));
    assert(p.state == PET_SLEEPING);
    pet_model_act(&p, PET_STROKE, 1001);
    assert(p.progress.growth_xp == 60 && pet_model_growth(&p) == PET_ADULT);
    pet_model_tick(&p, 1001 + PET_STROKE_REWARD_MS);
    pet_model_act(&p, PET_STROKE, 1002 + PET_STROKE_REWARD_MS);
    assert(p.progress.growth_xp == PET_ADULT_XP + 1);

    checkpoint = p.progress;
    assert(pet_model_restore(&p, &checkpoint, 0));
    assert(p.progress.satiety == checkpoint.satiety && p.progress.active_ms == checkpoint.active_ms);
    assert(p.progress.stroke_wait_ms == checkpoint.stroke_wait_ms);
    pet_model_act(&p, PET_STROKE, 1);
    assert(p.progress.growth_xp == checkpoint.growth_xp);
    checkpoint.satiety = 101;
    assert(!pet_model_restore(&p, &checkpoint, 123));
    assert(p.progress.satiety == 70);

    const uint32_t start = UINT32_MAX - 1000;
    pet_model_init(&p, start);
    pet_model_act(&p, PET_FEED, start);
    pet_model_tick(&p, start + PET_EATING_MS + PET_HAPPY_MS);
    assert(p.state == PET_IDLE && p.progress.active_ms == PET_EATING_MS + PET_HAPPY_MS);
    pet_model_tick(&p, start + PET_HUNGER_STEP_MS);
    assert(p.progress.satiety == 94 && p.state == PET_IDLE);
    assert(pet_model_act(&p, (pet_action_t)99, 800000) == PET_ACTION_IGNORED);
    for (int state = PET_IDLE; state <= PET_SLEEPING; ++state) {
        p.state = (pet_state_t)state;
        p.state_since = 0;
        for (uint32_t t = 0; t < 120000; t += 17) {
            pet_pose_t pose = pet_model_pose(&p, t);
            assert(isfinite(pose.bob) && fabsf(pose.bob) <= 7.01f);
            assert(isfinite(pose.tail) && fabsf(pose.tail) <= 12.01f);
            assert(pose.eyes >= 0 && pose.eyes <= 1 && pose.mouth >= 0 && pose.mouth <= 1);
        }
    }
    puts("PASS: hunger/meal limits, growth/cooldown, game scoring/timeout/cancel/reboot, quests/shop/caps, wraparound, poses");
}
