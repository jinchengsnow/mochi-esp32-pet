// SPDX-License-Identifier: Apache-2.0
#ifndef PET_UI_H
#define PET_UI_H
#include "lvgl.h"
#include "pet_model.h"

// Called once, with the BSP display lock held. LVGL owns callbacks thereafter.
void pet_ui_bind_input(lv_indev_t *input);
void pet_ui_create(lv_obj_t *screen, const pet_progress_t *restored);
// Called only with the display lock held while the LVGL adapter is paused.
bool pet_ui_sleep_begin(void);
void pet_ui_sleep_advance(uint64_t elapsed_ms, bool save);
pet_state_t pet_ui_state(void);
const pet_progress_t *pet_ui_progress(void);
void pet_ui_destroy(void);
#endif
