// SPDX-License-Identifier: Apache-2.0
#ifndef PET_MODEL_H
#define PET_MODEL_H
#include <stdbool.h>
#include <stdint.h>
#include "pet_routine.h"

typedef enum { PET_IDLE, PET_HAPPY, PET_EATING, PET_SLEEPING } pet_state_t;
typedef enum { PET_STROKE, PET_FEED, PET_TOGGLE_SLEEP } pet_action_t;
typedef enum { PET_EMOTE_NONE, PET_EMOTE_HEAD, PET_EMOTE_TICKLE, PET_EMOTE_HUG,
               PET_EMOTE_TEASE_LEFT, PET_EMOTE_TEASE_RIGHT, PET_EMOTE_HIGH_FIVE } pet_emote_t;
typedef enum { PET_AMBIENT_NONE, PET_AMBIENT_STRETCH, PET_AMBIENT_GROOM,
               PET_AMBIENT_BALL, PET_AMBIENT_WATCH } pet_ambient_t;
enum { PET_AMBIENT_MS = 5000 };

enum { PET_HAPPY_MS = 2400, PET_EATING_MS = 3600, PET_IDLE_SLEEP_MS = 60000 };
enum { PET_HUNGER_STEP_MS = 300000, PET_STROKE_REWARD_MS = 300000,
       PET_FEED_THRESHOLD = 75, PET_FEED_GAIN = 25, PET_YOUNG_XP = 20, PET_ADULT_XP = 60,
       PET_MAX_XP = 600, PET_MAX_STARS = 9999 };
// Fractional meters use rest-time milliseconds; bright time accrues six times faster.
enum { PET_TIME_SCALE = 6, PET_REST_HUNGER_MS = 1800000,
       PET_GROWTH_STEP_MS = 600000, PET_REST_GROWTH_MS = 3600000 };
enum { PET_PLAY_MS = 15000, PET_PLAY_WAIT_MS = 60000, PET_PLAY_HIT_MS = 180,
       PET_QUEST_STROKE = 1, PET_QUEST_FEED = 2, PET_QUEST_PLAY = 4 };
typedef enum { PET_BABY, PET_YOUNG, PET_ADULT } pet_growth_t;
typedef enum { PET_ACTION_IGNORED, PET_ACTION_OK, PET_ACTION_FULL } pet_action_result_t;

typedef struct {
    uint64_t active_ms;
    uint32_t meals, hunger_ms, stroke_wait_ms;
    uint32_t games, play_wait_ms, growth_ms;
    uint16_t growth_xp, stars;
    uint8_t satiety, quest_flags, accessory, unlocked;
    bool sleeping;
} pet_progress_t;

typedef struct {
    bool active;
    uint32_t started, last_hit;
    uint16_t hits, combo, best_combo;
    uint8_t target;
    // Actual credited totals, including a completed companion quest and caps.
    uint16_t reward_xp, reward_stars;
} pet_game_t;

typedef struct {
    pet_state_t state;
    pet_emote_t emote;
    uint32_t state_since;
    uint32_t last_interaction;
    uint32_t last_tick, revision;
    pet_progress_t progress;
    pet_game_t game;
    bool screen_off;
    bool clock_valid, manual_sleep;
    int64_t utc_s, awake_until_s;
    pet_routine_t routine;
    pet_routine_plan_t plan;
    pet_ambient_t ambient, last_ambient;
    uint32_t ambient_since, next_ambient, ambient_sequence;
} pet_model_t;

typedef struct {
    float bob, sway, tail, eyes, gaze, mouth, sparkle, paw, cuddle;
} pet_pose_t;

void pet_model_init(pet_model_t *pet, uint32_t now);
void pet_progress_default(pet_progress_t *progress);
bool pet_progress_valid(const pet_progress_t *progress);
bool pet_model_restore(pet_model_t *pet, const pet_progress_t *progress, uint32_t now);
void pet_model_tick(pet_model_t *pet, uint32_t now);
void pet_model_update_clock(pet_model_t *pet, bool valid, int64_t utc_s, uint32_t now);
// Autonomous poses give no interaction rewards. Input/menu/screen-off cancels them.
void pet_model_ambient_tick(pet_model_t *pet, uint32_t now, bool allow);
void pet_model_ambient_cancel(pet_model_t *pet, uint32_t now);
void pet_model_set_screen_off(pet_model_t *pet, bool off, uint32_t now);
// Account real elapsed rest time while LVGL ticks were paused, then re-anchor those ticks.
void pet_model_advance_sleep(pet_model_t *pet, uint64_t elapsed_ms, uint32_t now);
pet_action_result_t pet_model_act(pet_model_t *pet, pet_action_t action, uint32_t now);
// Affection variants share the existing stroke reward/cooldown and quest credit.
pet_action_result_t pet_model_interact(pet_model_t *pet, pet_emote_t emote, uint32_t now);
pet_growth_t pet_model_growth(const pet_model_t *pet);
unsigned pet_model_level(const pet_model_t *pet);
// 15-second, five-target game. Hit only the current target, >=180ms apart.
// >=5 hits earns 3 XP/2 stars; >=10 earns 6 XP/4 stars. The round costs 5 food.
// Persisted wait reserves round + cooldown on start, making reboot abort safe.
bool pet_model_play_start(pet_model_t *pet, uint32_t now);
bool pet_model_play_hit(pet_model_t *pet, uint8_t target, uint32_t now);
void pet_model_play_cancel(pet_model_t *pet, uint32_t now);
// 0 removes; 1 bow (8 stars, 20 XP); 2 crown (16 stars, 60 XP).
// Purchased accessories are free to equip again; unlocked bits are 1 and 2.
bool pet_model_accessory(pet_model_t *pet, uint8_t accessory, uint32_t now);
pet_pose_t pet_model_pose(const pet_model_t *pet, uint32_t now);
const char *pet_state_name(pet_state_t state);
#endif
