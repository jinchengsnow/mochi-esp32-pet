// SPDX-License-Identifier: Apache-2.0
#ifndef PET_SAVE_H
#define PET_SAVE_H
#include "pet_model.h"
#include <stddef.h>
// Version 3 records are 80 bytes; decode also accepts v2 and 48-byte v1 records.
enum { PET_SAVE_BYTES = 80, PET_SAVE_V1_BYTES = 48 };
void pet_save_encode(const pet_progress_t *progress, uint32_t generation, uint8_t out[PET_SAVE_BYTES]);
bool pet_save_decode(const uint8_t *bytes, size_t size, pet_progress_t *progress, uint32_t *generation);
bool pet_save_newer(uint32_t candidate, uint32_t previous);
#endif
