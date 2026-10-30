// SPDX-License-Identifier: Apache-2.0
// Export the exact production synthesizer for offline listening/review.
#include "pet_sound.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static void u16(FILE *f, unsigned n) { fputc(n & 255, f); fputc((n >> 8) & 255, f); }
static void u32(FILE *f, unsigned n) { u16(f,n); u16(f,n >> 16); }
int main(int argc, char **argv)
{
    assert(argc == 2);
    for (pet_sound_t cue = PET_SOUND_HEAD; cue < PET_SOUND_COUNT; ++cue) {
        char path[1024];
        snprintf(path,sizeof(path),"%s/%s.wav",argv[1],pet_sound_name(cue));
        FILE *f = fopen(path,"wb"); assert(f);
        const uint32_t samples = pet_sound_samples(cue), bytes = samples * 2;
        fwrite("RIFF",1,4,f); u32(f,36+bytes); fwrite("WAVEfmt ",1,8,f);
        u32(f,16); u16(f,1); u16(f,1); u32(f,PET_SOUND_RATE); u32(f,PET_SOUND_RATE*2);
        u16(f,2); u16(f,16); fwrite("data",1,4,f); u32(f,bytes);
        int16_t pcm[PET_SOUND_CHUNK]; uint32_t offset = 0; size_t count;
        while ((count = pet_sound_render(cue,offset,pcm,PET_SOUND_CHUNK))) {
            for (size_t i = 0; i < count; ++i) u16(f,(uint16_t)pcm[i]);
            offset += count;
        }
        assert(offset == samples); assert(fclose(f) == 0);
    }
    puts("Exported 14 production PCM cues: 22050 Hz mono WAV");
}
