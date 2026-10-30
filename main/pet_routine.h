// SPDX-License-Identifier: Apache-2.0
#ifndef PET_ROUTINE_H
#define PET_ROUTINE_H
#include <stdint.h>
typedef enum { PET_ROUTINE_UNSET, PET_ROUTINE_DAY, PET_ROUTINE_NAP, PET_ROUTINE_NIGHT } pet_routine_t;
typedef struct {
    int64_t day; // Local calendar day, UTC+8. Never uptime.
    uint16_t wake, bedtime, nap_start, nap_end; // Minutes after local midnight.
} pet_routine_plan_t;
enum { PET_LOCAL_OFFSET_S = 8 * 3600, PET_WAKE_GRACE_S = 10 * 60 };
pet_routine_t pet_routine_at(int64_t utc_s, pet_routine_plan_t *plan);
#endif
