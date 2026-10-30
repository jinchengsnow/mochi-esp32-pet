// SPDX-License-Identifier: Apache-2.0
#include "pet_routine.h"

static uint32_t mix(uint32_t n)
{
    n ^= n >> 16; n *= 0x7feb352du;
    n ^= n >> 15; n *= 0x846ca68bu;
    return n ^ (n >> 16);
}

pet_routine_t pet_routine_at(int64_t utc_s, pet_routine_plan_t *plan)
{
    const int64_t local = utc_s + PET_LOCAL_OFFSET_S;
    const int64_t day = local / 86400;
    const unsigned minute = (unsigned)(local % 86400) / 60;
    const uint32_t seed = mix((uint32_t)day ^ 0x4d4f4348u);
    *plan = (pet_routine_plan_t){.day = day,
        .wake = 7 * 60 + seed % 61,
        .bedtime = 21 * 60 + 45 + mix(seed + 1) % 31,
        .nap_start = 13 * 60 + mix(seed + 2) % 16,
        .nap_end = 13 * 60 + 45 + mix(seed + 3) % 16};
    if (minute < plan->wake || minute >= plan->bedtime) return PET_ROUTINE_NIGHT;
    if (minute >= plan->nap_start && minute < plan->nap_end) return PET_ROUTINE_NAP;
    return PET_ROUTINE_DAY;
}
