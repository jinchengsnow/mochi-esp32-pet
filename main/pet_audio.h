// SPDX-License-Identifier: Apache-2.0
#ifndef PET_AUDIO_H
#define PET_AUDIO_H
#include <stdbool.h>
#include <stdint.h>
#include "pet_sound.h"
typedef struct { bool enabled, available, saved; uint32_t revision; } pet_audio_status_t;
bool pet_audio_init(void);
pet_audio_status_t pet_audio_status(void);
// Thread-safe, nonblocking. One pending cue; newest interaction replaces older audio.
void pet_audio_play(pet_sound_t sound);
void pet_audio_toggle(void);
void pet_audio_set_screen_off(bool off);
// Power worker only: wait for codec/I2S shutdown and any sound-setting commit.
bool pet_audio_wait_quiet(uint32_t timeout_ms);
#endif
