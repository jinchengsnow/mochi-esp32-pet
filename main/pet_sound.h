// SPDX-License-Identifier: Apache-2.0
#ifndef PET_SOUND_H
#define PET_SOUND_H
#include <stddef.h>
#include <stdint.h>
typedef enum {
    PET_SOUND_NONE, PET_SOUND_HEAD, PET_SOUND_TICKLE, PET_SOUND_HUG, PET_SOUND_TEASE,
    PET_SOUND_HIGH_FIVE, PET_SOUND_FEED, PET_SOUND_FULL, PET_SOUND_PLAY,
    PET_SOUND_HIT, PET_SOUND_REWARD, PET_SOUND_LEVEL, PET_SOUND_SLEEP,
    PET_SOUND_WAKE, PET_SOUND_ENABLED, PET_SOUND_COUNT
} pet_sound_t;
enum { PET_SOUND_RATE = 22050, PET_SOUND_CHUNK = 220, PET_SOUND_VOLUME = 80 };
uint32_t pet_sound_samples(pet_sound_t sound);
// Original synthesized PCM, mono signed 16-bit. Stateless for chunk/whole identity.
size_t pet_sound_render(pet_sound_t sound, uint32_t offset, int16_t *out, size_t capacity);
const char *pet_sound_name(pet_sound_t sound);
#endif
