// SPDX-License-Identifier: Apache-2.0
#ifndef PET_STORE_H
#define PET_STORE_H
#include "pet_model.h"
typedef enum { PET_STORE_SAVED, PET_STORE_SAVING, PET_STORE_ERROR } pet_store_status_t;
// A false return leaves default progress usable but disables writing existing data.
bool pet_store_init(pet_progress_t *restored, bool *had_save);
bool pet_store_request(const pet_progress_t *progress);
pet_store_status_t pet_store_status(void);
#endif
