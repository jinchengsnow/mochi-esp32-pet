// SPDX-License-Identifier: Apache-2.0
#include "pet_model.h"
#include <math.h>

static void enter(pet_model_t *pet, pet_state_t state, uint32_t now)
{
    const bool sleeping = state == PET_SLEEPING;
    if (pet->progress.sleeping != sleeping) ++pet->revision;
    pet->progress.sleeping = sleeping;
    pet->state = state;
    pet->emote = PET_EMOTE_NONE;
    pet->ambient = PET_AMBIENT_NONE;
    pet->state_since = now;
    pet->next_ambient = now + 15000;
}

void pet_model_init(pet_model_t *pet, uint32_t now)
{
    *pet = (pet_model_t){.state = PET_IDLE, .state_since = now,
                         .last_interaction = now, .last_tick = now, .next_ambient = now + 15000};
    pet_progress_default(&pet->progress);
}

void pet_progress_default(pet_progress_t *p)
{
    *p = (pet_progress_t){.satiety = 70};
}

bool pet_progress_valid(const pet_progress_t *p)
{
    return p && p->satiety <= 100 && p->growth_xp <= PET_MAX_XP &&
           p->hunger_ms < PET_REST_HUNGER_MS && p->growth_ms < PET_REST_GROWTH_MS && p->stroke_wait_ms <= PET_STROKE_REWARD_MS &&
           p->stars <= PET_MAX_STARS && p->quest_flags < 7 && p->accessory <= 2 &&
           p->unlocked <= 3 && (!p->accessory || (p->unlocked & (1u << (p->accessory - 1)))) &&
           (!(p->unlocked & 1) || p->growth_xp >= PET_YOUNG_XP) &&
           (!(p->unlocked & 2) || p->growth_xp >= PET_ADULT_XP) &&
           p->play_wait_ms <= PET_PLAY_MS + PET_PLAY_WAIT_MS;
}

bool pet_model_restore(pet_model_t *pet, const pet_progress_t *p, uint32_t now)
{
    pet_model_init(pet, now);
    if (!pet_progress_valid(p)) return false;
    pet->progress = *p;
    pet->state = p->sleeping ? PET_SLEEPING : PET_IDLE;
    return true;
}

pet_growth_t pet_model_growth(const pet_model_t *pet)
{
    if (pet->progress.growth_xp >= PET_ADULT_XP) return PET_ADULT;
    return pet->progress.growth_xp >= PET_YOUNG_XP ? PET_YOUNG : PET_BABY;
}

unsigned pet_model_level(const pet_model_t *pet)
{
    const unsigned level = 1u + pet->progress.growth_xp / 60u;
    return level > 10 ? 10 : level;
}

static void reward(pet_model_t *pet, unsigned xp, unsigned stars)
{
    pet_progress_t *p = &pet->progress;
    p->growth_xp = p->growth_xp + xp > PET_MAX_XP ? PET_MAX_XP : p->growth_xp + xp;
    p->stars = p->stars + stars > PET_MAX_STARS ? PET_MAX_STARS : p->stars + stars;
    ++pet->revision;
}

static void quest(pet_model_t *pet, unsigned flag)
{
    pet->progress.quest_flags |= flag;
    if (pet->progress.quest_flags == 7) {
        pet->progress.quest_flags = 0;
        reward(pet, 6, 5);
    }
    ++pet->revision;
}

static void finish_play(pet_model_t *pet, uint32_t now)
{
    pet_game_t *g = &pet->game;
    const uint32_t finish = g->started + PET_PLAY_MS;
    const uint32_t late = now - finish;
    g->active = false;
    pet->progress.play_wait_ms = late >= PET_PLAY_WAIT_MS ? 0 : PET_PLAY_WAIT_MS - late;
    pet->last_interaction = finish;
    if (pet->progress.games < UINT32_MAX) ++pet->progress.games;
    const unsigned old_xp = pet->progress.growth_xp, old_stars = pet->progress.stars;
    if (g->hits >= 5) {
        reward(pet, g->hits >= 10 ? 6 : 3, g->hits >= 10 ? 4 : 2);
        quest(pet, PET_QUEST_PLAY);
    }
    g->reward_xp = pet->progress.growth_xp - old_xp;
    g->reward_stars = pet->progress.stars - old_stars;
    enter(pet, g->hits >= 5 ? PET_HAPPY : PET_IDLE, finish);
    ++pet->revision;
}

static uint64_t accrue(uint32_t *fraction, uint64_t ms, unsigned rate, uint32_t period)
{
    // Divide before multiplying so even a very large elapsed interval cannot overflow.
    const uint64_t rest = (ms % period) * rate + *fraction;
    const uint64_t units = (ms / period) * rate + rest / period;
    *fraction = (uint32_t)(rest % period);
    return units;
}

static void advance_progress(pet_model_t *pet, uint64_t dt, bool off)
{
    pet_progress_t *p = &pet->progress;
    p->active_ms = UINT64_MAX - p->active_ms < dt ? UINT64_MAX : p->active_ms + dt;
    p->stroke_wait_ms = dt >= p->stroke_wait_ms ? 0 : p->stroke_wait_ms - (uint32_t)dt;
    p->play_wait_ms = dt >= p->play_wait_ms ? 0 : p->play_wait_ms - (uint32_t)dt;
    const unsigned rate = off ? 1 : PET_TIME_SCALE;
    const uint64_t loss = accrue(&p->hunger_ms, dt, rate, PET_REST_HUNGER_MS);
    const uint64_t growth = accrue(&p->growth_ms, dt, rate, PET_REST_GROWTH_MS);
    const uint8_t food = p->satiety;
    const uint16_t xp = p->growth_xp;
    p->satiety = loss >= p->satiety ? 0 : p->satiety - (uint8_t)loss;
    p->growth_xp = growth >= (unsigned)(PET_MAX_XP - p->growth_xp) ? PET_MAX_XP : p->growth_xp + (uint16_t)growth;
    if (food != p->satiety || xp != p->growth_xp) ++pet->revision;
}

static void advance_runtime(pet_model_t *pet, uint32_t now)
{
    // Finish before transitions; late ticks cannot extend a round or its cooldown.
    if (pet->game.active && now - pet->game.started >= PET_PLAY_MS)
        finish_play(pet, now);
    // Unsigned subtraction also works across the 32-bit tick rollover.
    if (pet->state == PET_EATING && now - pet->state_since >= PET_EATING_MS)
        enter(pet, PET_HAPPY, pet->state_since + PET_EATING_MS);
    if (pet->state == PET_HAPPY && now - pet->state_since >= PET_HAPPY_MS)
        enter(pet, PET_IDLE, pet->state_since + PET_HAPPY_MS);
    if (pet->screen_off && !pet->clock_valid && !pet->game.active && pet->state == PET_IDLE &&
        now - pet->last_interaction >= PET_IDLE_SLEEP_MS)
        enter(pet, PET_SLEEPING, now);
}

static void advance_elapsed(pet_model_t *pet, uint64_t ms, bool off, uint32_t now)
{
    uint32_t virtual_now = pet->last_tick;
    // Settle a running game at its real deadline before any later passive XP reaches the cap.
    if (pet->game.active) {
        const uint32_t age = virtual_now - pet->game.started;
        const uint32_t remaining = age >= PET_PLAY_MS ? 0 : PET_PLAY_MS - age;
        if (ms >= remaining) {
            advance_progress(pet, remaining, off);
            virtual_now += remaining;
            advance_runtime(pet, virtual_now);
            ms -= remaining;
        }
    }
    advance_progress(pet, ms, off);
    // Every transient timeout is under 2 minutes; one day suffices to expire all of them.
    virtual_now += ms > 86400000 ? 86400000 : (uint32_t)ms;
    advance_runtime(pet, virtual_now);
    const uint32_t shift = now - virtual_now;
    pet->state_since += shift;
    pet->last_interaction += shift;
    pet->game.started += shift;
    pet->game.last_hit += shift;
    pet->last_tick = now;
}

void pet_model_tick(pet_model_t *pet, uint32_t now)
{
    advance_elapsed(pet, (uint32_t)(now - pet->last_tick), pet->screen_off, now);
}

void pet_model_ambient_cancel(pet_model_t *pet, uint32_t now)
{
    pet->ambient = PET_AMBIENT_NONE;
    pet->next_ambient = now + 12000 + pet->ambient_sequence % 13001;
}

void pet_model_ambient_tick(pet_model_t *pet, uint32_t now, bool allow)
{
    if (!allow || pet->screen_off || pet->state != PET_IDLE || pet->game.active ||
        (pet->clock_valid && pet->routine != PET_ROUTINE_DAY)) {
        pet_model_ambient_cancel(pet, now);
        return;
    }
    if (pet->ambient != PET_AMBIENT_NONE) {
        if (now - pet->ambient_since >= PET_AMBIENT_MS) pet_model_ambient_cancel(pet, now);
    } else if ((int32_t)(now - pet->next_ambient) >= 0) {
        pet->ambient_sequence = pet->ambient_sequence * 1664525u + 1013904223u;
        pet_ambient_t next = 1 + (pet->ambient_sequence >> 16) % 4;
        if (next == pet->last_ambient) next = 1 + next % 4;
        pet->ambient = pet->last_ambient = next;
        pet->ambient_since = now;
    }
}

void pet_model_update_clock(pet_model_t *pet, bool valid, int64_t utc_s, uint32_t now)
{
    pet_model_tick(pet, now);
    const pet_routine_t previous = pet->routine;
    // Calibration invalidates an override tied to the old wall clock.
    if (valid && pet->clock_valid && (utc_s < pet->utc_s || utc_s - pet->utc_s > 120)) {
        pet->awake_until_s = 0;
        pet->manual_sleep = false;
    }
    pet->clock_valid = valid;
    pet->utc_s = utc_s;
    pet->routine = valid ? pet_routine_at(utc_s, &pet->plan) : PET_ROUTINE_UNSET;
    if (!valid) return;
    if (pet->routine == PET_ROUTINE_DAY) {
        if (previous == PET_ROUTINE_NAP || previous == PET_ROUTINE_NIGHT) {
            pet->manual_sleep = false;
            pet->awake_until_s = 0;
        }
        if (pet->state == PET_SLEEPING && !pet->manual_sleep) enter(pet, PET_IDLE, now);
    } else if (!pet->game.active && pet->state == PET_IDLE && utc_s >= pet->awake_until_s) {
        enter(pet, PET_SLEEPING, now);
    }
}

static void user_awake(pet_model_t *pet, uint32_t now)
{
    pet_model_ambient_cancel(pet, now);
    pet->manual_sleep = false;
    if (pet->clock_valid && pet->routine != PET_ROUTINE_DAY)
        pet->awake_until_s = pet->utc_s + PET_WAKE_GRACE_S;
}

void pet_model_set_screen_off(pet_model_t *pet, bool off, uint32_t now)
{
    pet_model_tick(pet, now); // Settle the previous mode before switching its rate.
    pet->screen_off = off;
    pet_model_ambient_cancel(pet, now);
}

void pet_model_advance_sleep(pet_model_t *pet, uint64_t ms, uint32_t now)
{
    advance_elapsed(pet, ms, true, now);
}

pet_action_result_t pet_model_act(pet_model_t *pet, pet_action_t action, uint32_t now)
{
    pet_model_tick(pet, now);
    if (pet->game.active) return PET_ACTION_IGNORED;
    if (action != PET_STROKE && action != PET_FEED && action != PET_TOGGLE_SLEEP)
        return PET_ACTION_IGNORED;
    // A stream of taps cannot keep restarting a meal or replace its animation.
    if (pet->state == PET_EATING && action != PET_TOGGLE_SLEEP) return PET_ACTION_IGNORED;
    const bool was_sleeping = pet->state == PET_SLEEPING;
    user_awake(pet, now);
    pet->last_interaction = now;
    pet_progress_t *p = &pet->progress;
    if (action == PET_FEED && p->satiety > PET_FEED_THRESHOLD) {
        enter(pet, PET_HAPPY, now);
        return PET_ACTION_FULL;
    }
    unsigned xp = 0;
    if (action == PET_FEED) {
        // Award at meal start, so a save never leaves partially credited food.
        p->satiety = p->satiety + PET_FEED_GAIN > 100 ? 100 : p->satiety + PET_FEED_GAIN;
        if (p->meals < UINT32_MAX) ++p->meals;
        xp = 4;
        quest(pet, PET_QUEST_FEED);
        ++pet->revision;
    } else if (action == PET_STROKE && p->stroke_wait_ms == 0) {
        xp = 1;
        quest(pet, PET_QUEST_STROKE);
        p->stroke_wait_ms = PET_STROKE_REWARD_MS;
        ++pet->revision;
    }
    if (xp) reward(pet, xp, 0);
    if (action == PET_TOGGLE_SLEEP) {
        pet->manual_sleep = !was_sleeping;
        if (pet->manual_sleep) pet->awake_until_s = 0;
        enter(pet, was_sleeping ? PET_IDLE : PET_SLEEPING, now);
    } else
        enter(pet, action == PET_FEED ? PET_EATING : PET_HAPPY, now);
    if (action == PET_STROKE) pet->emote = PET_EMOTE_HEAD;
    return PET_ACTION_OK;
}

pet_action_result_t pet_model_interact(pet_model_t *pet, pet_emote_t emote, uint32_t now)
{
    if (emote <= PET_EMOTE_NONE || emote > PET_EMOTE_HIGH_FIVE || pet->screen_off)
        return PET_ACTION_IGNORED;
    const pet_action_result_t result = pet_model_act(pet, PET_STROKE, now);
    if (result == PET_ACTION_OK) pet->emote = emote;
    return result;
}

bool pet_model_play_start(pet_model_t *pet, uint32_t now)
{
    pet_model_tick(pet, now);
    if (pet->game.active || pet->state == PET_EATING || pet->progress.play_wait_ms ||
        pet->progress.satiety < 10) return false;
    user_awake(pet, now);
    pet->game = (pet_game_t){.active = true, .started = now, .last_hit = now - PET_PLAY_HIT_MS};
    pet->progress.satiety -= 5;
    // A save mid-round restores an aborted round with this remaining reservation.
    pet->progress.play_wait_ms = PET_PLAY_MS + PET_PLAY_WAIT_MS;
    pet->last_interaction = now;
    enter(pet, PET_IDLE, now);
    ++pet->revision;
    return true;
}

bool pet_model_play_hit(pet_model_t *pet, uint8_t target, uint32_t now)
{
    pet_model_tick(pet, now);
    pet_game_t *g = &pet->game;
    if (!g->active) return false;
    pet->last_interaction = now;
    if (target != g->target) {
        g->combo = 0;
        return false;
    }
    // Ignore debounced duplicate inputs; do not let a touch stream earn hits.
    if (now - g->last_hit < PET_PLAY_HIT_MS) return false;
    g->last_hit = now;
    ++g->hits;
    ++g->combo;
    if (g->combo > g->best_combo) g->best_combo = g->combo;
    g->target = (g->target + 2) % 5;
    return true;
}

void pet_model_play_cancel(pet_model_t *pet, uint32_t now)
{
    pet_model_tick(pet, now);
    if (!pet->game.active) return;
    pet->game.active = false;
    pet->progress.play_wait_ms = PET_PLAY_WAIT_MS;
    pet->last_interaction = now;
    enter(pet, PET_IDLE, now);
    ++pet->revision;
}

bool pet_model_accessory(pet_model_t *pet, uint8_t accessory, uint32_t now)
{
    pet_model_tick(pet, now);
    if (pet->game.active || accessory > 2) return false;
    pet_progress_t *p = &pet->progress;
    if (accessory) {
        const unsigned bit = 1u << (accessory - 1);
        const unsigned cost = accessory == 1 ? 8 : 16;
        const unsigned xp = accessory == 1 ? PET_YOUNG_XP : PET_ADULT_XP;
        if (!(p->unlocked & bit)) {
            if (p->growth_xp < xp || p->stars < cost) return false;
            p->stars -= cost;
            p->unlocked |= bit;
        }
    }
    if (p->accessory != accessory) {
        p->accessory = accessory;
        ++pet->revision;
    }
    pet->last_interaction = now;
    return true;
}

pet_pose_t pet_model_pose(const pet_model_t *pet, uint32_t now)
{
    // Reduce before converting to float so motion remains smooth after days on.
    const float t = (now % 60000) / 1000.0f;
    const float age = (now - pet->state_since) / 1000.0f;
    pet_pose_t p = {.bob = 2.0f * sinf(t * 2.2f), .sway = sinf(t * 1.5f),
                    .tail = 7.0f * sinf(t * 2.5f), .eyes = 1.0f,
                    .gaze = 2.0f * sinf(t * 0.7f)};
    const uint32_t blink = now % 4300;
    if (blink < 140) p.eyes = fabsf((float)blink - 70.0f) / 70.0f;
    if (pet->progress.satiety <= 25) {
        p.eyes *= 0.65f;
        p.tail *= 0.5f;
    }
    if (pet->state == PET_IDLE && pet->ambient != PET_AMBIENT_NONE) {
        const float a = (now - pet->ambient_since) / 1000.0f;
        switch (pet->ambient) {
        case PET_AMBIENT_STRETCH:
            p.bob = 5 * sinf(a * 0.628319f); p.sway = 0;
            p.eyes = 0; p.paw = sinf(a * 0.628319f); p.tail = -12; break;
        case PET_AMBIENT_GROOM:
            p.bob = sinf(a * 5); p.eyes = 0; p.paw = 0.5f + 0.5f * sinf(a * 6); break;
        case PET_AMBIENT_BALL:
            p.sway = 5 * sinf(a * 2.5f); p.gaze = 6 * sinf(a * 2.5f);
            p.paw = 0.5f + 0.5f * sinf(a * 5); p.tail = 12 * sinf(a * 5); break;
        case PET_AMBIENT_WATCH:
            p.gaze = 6 * sinf(a * 1.7f); p.sway = 3 * sinf(a * 1.7f); p.eyes = 1; break;
        default: break;
        }
    } else if (pet->state == PET_HAPPY) {
        p.bob = -7.0f * fabsf(sinf(age * 6.0f));
        p.tail = 12.0f * sinf(age * 10.0f);
        p.eyes = 0;
        p.sparkle = 0.5f + 0.5f * sinf(age * 4.0f);
        switch (pet->emote) {
        case PET_EMOTE_HEAD:
            p.bob = 2.0f * sinf(age * 5.0f); p.sway = 1.0f; break;
        case PET_EMOTE_TICKLE:
            p.sway = 7.0f * sinf(age * 12.0f);
            p.bob = -4.0f * fabsf(sinf(age * 10.0f));
            p.tail = 16.0f * sinf(age * 14.0f); p.mouth = 0.9f;
            p.paw = 0.7f + 0.3f * sinf(age * 12.0f); break;
        case PET_EMOTE_HUG:
            p.bob = sinf(age * 2.5f); p.sway = -4;
            p.tail = 2.0f * sinf(age * 2.0f); p.cuddle = 0.7f + 0.3f * sinf(age * 3.0f); break;
        case PET_EMOTE_TEASE_LEFT:
        case PET_EMOTE_TEASE_RIGHT:
            p.sway = pet->emote == PET_EMOTE_TEASE_LEFT ? -7 : 7;
            p.gaze = pet->emote == PET_EMOTE_TEASE_LEFT ? -6 : 6;
            p.eyes = 1; p.paw = 0.8f; p.tail = 15.0f * sinf(age * 9.0f); break;
        case PET_EMOTE_HIGH_FIVE:
            p.paw = 1; p.bob = -3.0f * fabsf(sinf(age * 4.0f)); break;
        default: break;
        }
    } else if (pet->state == PET_EATING) {
        p.bob = 3.0f * sinf(age * 13.0f);
        p.mouth = 0.5f + 0.5f * sinf(age * 13.0f);
        p.gaze = 0;
        p.eyes = 0.7f;
    } else if (pet->state == PET_SLEEPING) {
        p.bob = 2.5f * sinf(t * 1.5f);
        p.sway = 3;
        p.tail = 0;
        p.eyes = 0;
        p.gaze = 0;
    }
    return p;
}

const char *pet_state_name(pet_state_t state)
{
    switch (state) {
    case PET_IDLE: return "idle";
    case PET_HAPPY: return "happy";
    case PET_EATING: return "eating";
    case PET_SLEEPING: return "sleeping";
    default: return "unknown";
    }
}
