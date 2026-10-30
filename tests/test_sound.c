// SPDX-License-Identifier: Apache-2.0
#include "pet_sound.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void)
{
    assert(!pet_sound_samples(PET_SOUND_NONE));
    assert(!pet_sound_samples((pet_sound_t)-1) && !pet_sound_samples(PET_SOUND_COUNT));
    int16_t guard[] = {123, 456};
    assert(!pet_sound_render(PET_SOUND_NONE, 0, guard, 2) && guard[0] == 123 && guard[1] == 456);
    for (pet_sound_t sound = PET_SOUND_HEAD; sound < PET_SOUND_COUNT; ++sound) {
        const uint32_t size = pet_sound_samples(sound);
        assert(size >= PET_SOUND_RATE / 20 && size <= PET_SOUND_RATE);
        int16_t *whole = malloc(size * sizeof(*whole)), *chunked = malloc(size * sizeof(*chunked));
        assert(whole && chunked);
        assert(pet_sound_render(sound, 0, whole, size) == size);
        uint32_t offset = 0;
        while (offset < size) {
            size_t n = pet_sound_render(sound, offset, chunked + offset, PET_SOUND_CHUNK);
            assert(n && n <= PET_SOUND_CHUNK && n <= size - offset);
            offset += n;
        }
        assert(!memcmp(whole, chunked, size * sizeof(*whole)));
        assert(!whole[0] && !whole[size - 1]); // Fade in/out, no edge step.
        int peak = 0; double energy = 0, sum = 0;
        for (uint32_t i = 0; i < size; ++i) {
            const int v = abs(whole[i]); if (v > peak) peak = v;
            energy += (double)whole[i] * whole[i]; sum += whole[i];
        }
        assert(peak > 500 && peak <= 6500 && sqrt(energy / size) > 100);
        assert(fabs(sum / size) < 100);
        assert(!pet_sound_render(sound, size, guard, 2));
        assert(!pet_sound_render(sound, UINT32_MAX, guard, 2));
        assert(!pet_sound_render(sound, 0, NULL, 2));
        assert(!pet_sound_render(sound, 0, guard, 0));
        free(whole); free(chunked);
    }
    puts("PASS: 14 bounded PCM cues, non-silent energy, peak/DC limits, zero-edge fades, chunk/whole identity, invalid/range guards");
}
