// SPDX-License-Identifier: Apache-2.0
#include "pet_sound.h"
#include <math.h>

static const uint16_t duration_ms[PET_SOUND_COUNT] = {0, 360, 420, 650, 260, 180, 480, 220, 330, 65, 520, 700, 300, 250, 180};
static const char *names[PET_SOUND_COUNT] = {"none", "head", "tickle", "hug", "tease", "high-five", "feed", "full", "play", "hit", "reward", "level", "sleep", "wake", "enabled"};
static const float tau = 6.28318530718f;

uint32_t pet_sound_samples(pet_sound_t sound)
{
    return sound > PET_SOUND_NONE && sound < PET_SOUND_COUNT ?
           (uint32_t)duration_ms[sound] * PET_SOUND_RATE / 1000 : 0;
}
const char *pet_sound_name(pet_sound_t sound)
{
    return (unsigned)sound < PET_SOUND_COUNT ? names[sound] : "invalid";
}
static float noise(uint32_t n)
{
    n ^= n >> 16; n *= 0x7feb352du; n ^= n >> 15; n *= 0x846ca68bu; n ^= n >> 16;
    return (n & 65535) / 32767.5f - 1;
}
static float meow(float t, float duration, float base, float glide)
{
    const float phase = tau * (base * t + glide * duration / 3.141592654f * (1 - cosf(3.141592654f * t / duration)));
    return 0.62f * sinf(phase) + 0.24f * sinf(phase * 2) + 0.10f * sinf(phase * 3);
}
static float notes(float t, float duration, const float *frequencies, unsigned count)
{
    const float slot = duration / count;
    const unsigned note = fminf(count - 1, floorf(t / slot));
    const float age = t - note * slot;
    const float envelope = fminf(1, age / 0.006f) * fmaxf(0, 1 - age / slot);
    return envelope * (0.80f * sinf(tau * frequencies[note] * age) + 0.15f * sinf(tau * frequencies[note] * 2 * age));
}
size_t pet_sound_render(pet_sound_t sound, uint32_t offset, int16_t *out, size_t capacity)
{
    const uint32_t total = pet_sound_samples(sound);
    if (!out || offset >= total) return 0;
    const size_t count = capacity < total - offset ? capacity : total - offset;
    const float duration = total / (float)PET_SOUND_RATE;
    static const float happy[] = {784, 1047}, play[] = {523, 659, 784};
    static const float reward[] = {523, 659, 784, 1047}, level[] = {523, 659, 784, 1047, 1319};
    static const float sleep[] = {523, 392, 330}, wake[] = {659, 880};
    for (size_t i = 0; i < count; ++i) {
        const uint32_t sample = offset + i;
        const float t = sample / (float)PET_SOUND_RATE;
        const float envelope = fminf(1, t / 0.012f) * fminf(1, (total - 1 - sample) / (PET_SOUND_RATE * 0.025f));
        float value = 0;
        switch (sound) {
        case PET_SOUND_HEAD: value = meow(t, duration, 380, 260); break;
        case PET_SOUND_TICKLE:
            value = meow(t, duration, 630, 180) * fabsf(sinf(tau * 4.8f * t)); break;
        case PET_SOUND_HUG:
            value = (0.65f * sinf(tau * 125 * t) + 0.10f * noise(sample)) * (0.6f + 0.4f * sinf(tau * 26 * t)); break;
        case PET_SOUND_TEASE: value = meow(t, duration, 520, 380); break;
        case PET_SOUND_FEED:
            value = (0.35f * sinf(tau * 270 * t) + 0.40f * noise(sample)) * powf(fabsf(sinf(tau * 4.2f * t)), 4); break;
        case PET_SOUND_FULL: value = meow(t, duration, 310, 100) * 0.7f; break;
        case PET_SOUND_HIT: value = sinf(tau * 1047 * t) * (1 - t / duration); break;
        case PET_SOUND_HIGH_FIVE:
        case PET_SOUND_ENABLED: value = notes(t, duration, happy, 2); break;
        case PET_SOUND_PLAY: value = notes(t, duration, play, 3); break;
        case PET_SOUND_REWARD: value = notes(t, duration, reward, 4); break;
        case PET_SOUND_LEVEL: value = notes(t, duration, level, 5); break;
        case PET_SOUND_SLEEP: value = notes(t, duration, sleep, 3) * 0.6f; break;
        case PET_SOUND_WAKE: value = notes(t, duration, wake, 2) * 0.7f; break;
        default: break;
        }
        out[i] = (int16_t)lroundf(value * envelope * 6500);
    }
    return count;
}
