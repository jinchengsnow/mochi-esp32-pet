// SPDX-License-Identifier: Apache-2.0
// Observable host adapter: the real UI decides which cues to request.
#include "audio_stub.h"
static pet_audio_status_t state = {.enabled = true, .available = true, .saved = true};
static bool off;
static unsigned counts[PET_SOUND_COUNT];
bool pet_audio_init(void) { return state.available; }
pet_audio_status_t pet_audio_status(void) { return state; }
void pet_audio_play(pet_sound_t sound)
{
    if (sound > PET_SOUND_NONE && sound < PET_SOUND_COUNT && state.available && state.enabled && !off)
        ++counts[sound];
}
void pet_audio_toggle(void)
{
    state.enabled = !state.enabled; ++state.revision;
    if (state.enabled) pet_audio_play(PET_SOUND_ENABLED);
}
void pet_audio_set_screen_off(bool value) { off = value; }
bool pet_audio_wait_quiet(uint32_t timeout_ms) { (void)timeout_ms; return true; }
unsigned pet_audio_preview_count(pet_sound_t sound) { return counts[sound]; }
void pet_audio_preview_available(bool value) { state.available = value; }
