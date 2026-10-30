// SPDX-License-Identifier: Apache-2.0
#include "pet_save.h"
#include <string.h>

static void put(uint8_t *dst, uint64_t v, unsigned n)
{
    for (unsigned i = 0; i < n; ++i) dst[i] = (uint8_t)(v >> (i * 8));
}
static uint64_t get(const uint8_t *src, unsigned n)
{
    uint64_t v = 0;
    for (unsigned i = 0; i < n; ++i) v |= (uint64_t)src[i] << (i * 8);
    return v;
}
static uint32_t crc32(const uint8_t *src, size_t n)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < n; ++i) {
        crc ^= src[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
void pet_save_encode(const pet_progress_t *p, uint32_t gen, uint8_t out[PET_SAVE_BYTES])
{
    memset(out, 0, PET_SAVE_BYTES);
    memcpy(out, "MCH3", 4);
    put(out + 4, 3, 2); put(out + 6, PET_SAVE_BYTES, 2);
    put(out + 8, gen, 4); put(out + 12, p->active_ms, 8);
    put(out + 20, p->meals, 4); put(out + 24, p->growth_xp, 2);
    out[26] = p->satiety; out[27] = p->sleeping;
    put(out + 28, p->hunger_ms, 4); put(out + 32, p->stroke_wait_ms, 4);
    put(out + 36, p->stars, 2);
    out[38] = p->quest_flags; out[39] = p->accessory; out[40] = p->unlocked;
    put(out + 52, p->growth_ms, 4);
    put(out + 44, p->games, 4); put(out + 48, p->play_wait_ms, 4);
    put(out + PET_SAVE_BYTES - 4, crc32(out, PET_SAVE_BYTES - 4), 4);
}
bool pet_save_decode(const uint8_t *b, size_t n, pet_progress_t *p, uint32_t *gen)
{
    if (!b || !p || !gen || (n != PET_SAVE_BYTES && n != PET_SAVE_V1_BYTES)) return false;
    const bool legacy = n == PET_SAVE_V1_BYTES;
    const unsigned version = (unsigned)get(b + 4, 2);
    if (memcmp(b, "MCH3", 4) || (legacy ? version != 1 : (version != 2 && version != 3)) ||
        get(b + 6, 2) != n || get(b + n - 4, 4) != crc32(b, n - 4) || b[27] > 1)
        return false;
    if (legacy) {
        for (unsigned i = 36; i < 44; ++i) if (b[i]) return false;
        if (get(b + 24, 2) > PET_ADULT_XP) return false;
    } else {
        for (unsigned i = 41; i < 44; ++i) if (b[i]) return false;
        for (unsigned i = version == 2 ? 52 : 56; i < PET_SAVE_BYTES - 4; ++i) if (b[i]) return false;
    }
    pet_progress_t candidate = {.active_ms = get(b + 12, 8), .meals = (uint32_t)get(b + 20, 4),
        .growth_xp = (uint16_t)get(b + 24, 2), .satiety = b[26], .sleeping = b[27] != 0,
        .hunger_ms = (uint32_t)get(b + 28, 4), .stroke_wait_ms = (uint32_t)get(b + 32, 4)};
    if (!legacy) {
        candidate.stars = (uint16_t)get(b + 36, 2);
        candidate.quest_flags = b[38]; candidate.accessory = b[39]; candidate.unlocked = b[40];
        candidate.games = (uint32_t)get(b + 44, 4); candidate.play_wait_ms = (uint32_t)get(b + 48, 4);
    }
    if (version < 3) {
        if (candidate.hunger_ms >= PET_HUNGER_STEP_MS) return false;
        candidate.hunger_ms *= PET_TIME_SCALE; // Preserve the old fractional hunger meter.
    } else candidate.growth_ms = (uint32_t)get(b + 52, 4);
    if (!pet_progress_valid(&candidate)) return false;
    *p = candidate;
    *gen = (uint32_t)get(b + 8, 4);
    return true;
}
bool pet_save_newer(uint32_t candidate, uint32_t previous)
{
    const uint32_t distance = candidate - previous;
    return distance != 0 && distance < 0x80000000u;
}
