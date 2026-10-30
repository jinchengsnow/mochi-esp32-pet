// SPDX-License-Identifier: Apache-2.0
#include "pet_save.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_legacy(void)
{
    // v1 fixture encoded independently with Python struct/zlib, not the v2 writer.
    const uint8_t v1[48] = {
        0x4d, 0x43, 0x48, 0x33, 0x01, 0x00, 0x30, 0x00, 0x2a, 0x00, 0x00, 0x00,
        0x15, 0xcd, 0x5b, 0x07, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
        0x3c, 0x00, 0x5f, 0x01, 0xd2, 0x04, 0x00, 0x00, 0xf8, 0x8f, 0x04, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x52, 0x6c, 0xd5, 0x95
    };
    pet_progress_t p, restored;
    uint32_t gen;
    assert(pet_save_decode(v1, sizeof(v1), &p, &gen));
    assert(gen == 42 && p.active_ms == 123456789 && p.meals == 7);
    assert(p.growth_xp == 60 && p.satiety == 95 && p.sleeping);
    assert(p.hunger_ms == 1234 * PET_TIME_SCALE && p.stroke_wait_ms == 299000);
    assert(p.stars == 0 && p.games == 0 && p.quest_flags == 0);
    assert(p.accessory == 0 && p.unlocked == 0 && p.play_wait_ms == 0);
    uint8_t bytes[PET_SAVE_BYTES];
    pet_save_encode(&p, gen + 1, bytes);
    assert(pet_save_decode(bytes, sizeof(bytes), &restored, &gen));
    assert(gen == 43 && restored.growth_xp == 60 && restored.meals == 7);
    for (unsigned bit = 0; bit < sizeof(v1) * 8; ++bit) {
        uint8_t bad[sizeof(v1)];
        memcpy(bad, v1, sizeof(bad));
        bad[bit / 8] ^= (uint8_t)(1u << (bit % 8));
        assert(!pet_save_decode(bad, sizeof(bad), &p, &gen));
    }
}

static void test_ranges(void)
{
    pet_progress_t valid, bad, restored;
    uint8_t bytes[PET_SAVE_BYTES];
    uint32_t gen;
    pet_progress_default(&valid);
    // Invalid values carry a valid CRC, so this tests semantic validation.
    for (unsigned i = 0; i < 11; ++i) {
        bad = valid;
        switch (i) {
        case 0: bad.play_wait_ms = PET_PLAY_MS + PET_PLAY_WAIT_MS + 1; break;
        case 1: bad.stars = PET_MAX_STARS + 1; break;
        case 2: bad.quest_flags = 7; break;
        case 3: bad.quest_flags = 128; break;
        case 4: bad.accessory = 3; break;
        case 5: bad.unlocked = 4; break;
        case 6: bad.accessory = 1; break; // Wearing an unpurchased item.
        case 7: bad.unlocked = 1; break; // Purchased before stage unlock.
        case 8: bad.unlocked = 2; bad.growth_xp = 59; break;
        case 10: bad.growth_ms = PET_REST_GROWTH_MS; break;
        case 9: bad.stroke_wait_ms = PET_STROKE_REWARD_MS + 1; break;
        }
        pet_save_encode(&bad, 1, bytes);
        restored = valid;
        gen = 99;
        assert(!pet_save_decode(bytes, sizeof(bytes), &restored, &gen));
        assert(gen == 99 && restored.satiety == valid.satiety); // Failure leaves output intact.
    }
}

static void test_v2_migration(void)
{
    const uint8_t v2[80] = {
        0x4d, 0x43, 0x48, 0x33, 0x02, 0x00, 0x50, 0x00, 0x4d, 0x00, 0x00, 0x00,
        0xe8, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x7b, 0x00, 0x2a, 0x00, 0xdf, 0x93, 0x04, 0x00, 0xa8, 0x61, 0x00, 0x00,
        0x09, 0x00, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00,
        0xd2, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x44, 0x12, 0xc0, 0x74,
    };
    pet_progress_t progress;
    uint32_t generation;
    assert(pet_save_decode(v2, sizeof(v2), &progress, &generation));
    assert(generation == 77 && progress.growth_xp == 123 && progress.satiety == 42);
    assert(progress.hunger_ms == 1799994 && progress.growth_ms == 0);
    assert(progress.stars == 9 && progress.quest_flags == 3 && progress.accessory == 1);
    assert(progress.games == 12 && progress.play_wait_ms == 1234);
    pet_model_t pet;
    assert(pet_model_restore(&pet, &progress, 0));
    pet_model_tick(&pet, 1);
    assert(pet.progress.satiety == 41); // The old almost-complete hunger interval stays almost complete.
    for (unsigned bit = 0; bit < sizeof(v2) * 8; ++bit) {
        uint8_t broken[80]; memcpy(broken, v2, sizeof(broken));
        broken[bit/8] ^= (uint8_t)(1u << (bit%8));
        assert(!pet_save_decode(broken, sizeof(broken), &progress, &generation));
    }
}

int main(void)
{
    test_legacy();
    test_v2_migration();
    test_ranges();
    pet_progress_t before, after;
    pet_progress_default(&before);
    before.active_ms = UINT64_C(0x1122334455667788);
    before.meals = 12345; before.growth_xp = 39;
    before.satiety = 23; before.sleeping = true;
    before.hunger_ms = 123456; before.stroke_wait_ms = 299999;
    before.growth_ms = 987654;
    before.stars = 321; before.quest_flags = PET_QUEST_STROKE | PET_QUEST_PLAY;
    before.accessory = 1; before.unlocked = 1;
    before.games = UINT32_MAX; before.play_wait_ms = 74000;
    uint8_t bytes[PET_SAVE_BYTES], broken[PET_SAVE_BYTES];
    uint32_t gen;
    pet_save_encode(&before, UINT32_MAX, bytes);
    assert(pet_save_decode(bytes, sizeof(bytes), &after, &gen));
    assert(gen == UINT32_MAX && after.active_ms == before.active_ms);
    assert(after.meals == before.meals && after.growth_xp == 39 && after.satiety == 23);
    assert(after.sleeping && after.hunger_ms == before.hunger_ms && after.stroke_wait_ms == before.stroke_wait_ms);
    assert(after.growth_ms == 987654);
    assert(after.stars == 321 && after.quest_flags == before.quest_flags);
    assert(after.accessory == 1 && after.unlocked == 1 && after.games == UINT32_MAX && after.play_wait_ms == 74000);
    pet_model_t pet;
    assert(pet_model_restore(&pet, &after, 0));
    assert(!pet.game.active && !pet_model_play_start(&pet, 1));
    assert(!pet_save_decode(NULL, sizeof(bytes), &after, &gen));
    assert(!pet_save_decode(bytes, sizeof(bytes), NULL, &gen));
    assert(!pet_save_decode(bytes, sizeof(bytes), &after, NULL));
    for (size_t size = 0; size < sizeof(bytes); ++size)
        assert(!pet_save_decode(bytes, size, &after, &gen));
    assert(!pet_save_decode(bytes, sizeof(bytes) + 1, &after, &gen));
    for (unsigned bit = 0; bit < sizeof(bytes) * 8; ++bit) {
        memcpy(broken, bytes, sizeof(bytes));
        broken[bit / 8] ^= (uint8_t)(1u << (bit % 8));
        assert(!pet_save_decode(broken, sizeof(broken), &after, &gen));
    }
    before.satiety = 101;
    pet_save_encode(&before, 1, bytes);
    assert(!pet_save_decode(bytes, sizeof(bytes), &after, &gen));
    before.satiety = 50; before.growth_xp = PET_MAX_XP + 1;
    pet_save_encode(&before, 1, bytes);
    assert(!pet_save_decode(bytes, sizeof(bytes), &after, &gen));
    before.growth_xp = 0; before.hunger_ms = PET_REST_HUNGER_MS;
    pet_save_encode(&before, 1, bytes);
    assert(!pet_save_decode(bytes, sizeof(bytes), &after, &gen));
    assert(pet_save_newer(2, 1) && pet_save_newer(0, UINT32_MAX));
    assert(!pet_save_newer(UINT32_MAX, 0) && !pet_save_newer(5, 5));
    pet_progress_default(&before);
    before.growth_xp = 20;
    pet_save_encode(&before, 40, bytes);
    before.growth_xp = 60;
    pet_save_encode(&before, 41, broken);
    broken[12] ^= 1;
    assert(!pet_save_decode(broken, sizeof(broken), &after, &gen));
    assert(pet_save_decode(bytes, sizeof(bytes), &after, &gen));
    assert(gen == 40 && after.growth_xp == 20);
    puts("PASS: v3 round trip, v1/v2 fixture migration, every truncation/bit flip, semantic ranges, cooldown restore, fallback");
}
