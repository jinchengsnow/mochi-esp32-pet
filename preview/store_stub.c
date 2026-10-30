// SPDX-License-Identifier: Apache-2.0
// Headless adapter: save serialization/restore is tested separately. No NVS on host.
#include "pet_store.h"
bool pet_store_init(pet_progress_t *p, bool *saved)
{
    pet_progress_default(p);
    *saved = false;
    return true;
}
bool pet_store_request(const pet_progress_t *p) { return pet_progress_valid(p); }
pet_store_status_t pet_store_status(void) { return PET_STORE_SAVED; }
