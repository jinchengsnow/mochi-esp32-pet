// SPDX-License-Identifier: Apache-2.0
// Headless LVGL: the firmware's exact pet_ui.c renders into a real RGB565 buffer.
#include "pet_ui.h"
#include "pet_font.h"
#include "pet_save.h"
#include "device_stub.h"
#include "pet_touch.h"
#include "audio_stub.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef NDEBUG
#error "The preview and integration checks require assertions enabled"
#endif

enum { W = 466, H = 466 };
static uint16_t pixels[W * H], draw_buffer[W * 50];
static lv_indev_state_t pointer_state;
static lv_point_t pointer;
static unsigned frame, pointer_reads;
static const char *output_dir;

static void flush(lv_display_t *display, const lv_area_t *a, uint8_t *data)
{
    const uint16_t *src = (const uint16_t *)data;
    for (int y = a->y1; y <= a->y2; ++y) {
        assert(y >= 0 && y < H && a->x1 >= 0 && a->x2 < W);
        const size_t n = (size_t)(a->x2 - a->x1 + 1);
        memcpy(pixels + y * W + a->x1, src, n * sizeof(uint16_t));
        src += n;
    }
    lv_display_flush_ready(display);
}

static void read_pointer(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    ++pointer_reads;
    data->point = pointer;
    data->state = pointer_state;
}

static void step(unsigned ms)
{
    lv_tick_inc(ms);
    lv_timer_handler();
}

static void tap(int x, int y)
{
    pointer = (lv_point_t){x, y};
    pointer_state = LV_INDEV_STATE_PRESSED;
    step(40);
    pointer_state = LV_INDEV_STATE_RELEASED;
    step(40);
}

static void pet_tap(void)
{
    tap(230, 235);
    step(PET_TOUCH_GAP_MS + 40);
}

static void swipe(int x1, int y1, int x2, int y2)
{
    pointer = (lv_point_t){x1, y1}; pointer_state = LV_INDEV_STATE_PRESSED; step(40);
    for (int i = 1; i <= 6; ++i) {
        pointer = (lv_point_t){x1+(x2-x1)*i/6, y1+(y2-y1)*i/6}; step(40);
    }
    pointer_state = LV_INDEV_STATE_RELEASED; step(40);
}

static void save_frame(void)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%03u.ppm", output_dir, frame++);
    FILE *f = fopen(path, "wb");
    assert(f);
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            uint16_t c = pixels[y * W + x];
            unsigned char rgb[] = {(unsigned char)(((c >> 11) * 255 + 15) / 31),
                                   (unsigned char)((((c >> 5) & 63) * 255 + 31) / 63),
                                   (unsigned char)(((c & 31) * 255 + 15) / 31)};
            // Mask for the physical round face; no art or controls are in corners.
            const int dx = x * 2 - 465, dy = y * 2 - 465;
            if (dx * dx + dy * dy > 466 * 466) memset(rgb, 0, sizeof(rgb));
            assert(fwrite(rgb, 1, 3, f) == 3);
        }
    }
    fclose(f);
}

static lv_obj_t *find_label(lv_obj_t *obj, const char *value)
{
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), value)) return obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i) {
        lv_obj_t *found = find_label(lv_obj_get_child(obj, i), value);
        if (found) return found;
    }
    return NULL;
}

static bool contains_label(lv_obj_t *obj, const char *value)
{
    return find_label(obj, value) != NULL;
}

static void check_header(const char *battery_text, const char *clock_text)
{
    lv_obj_t *b = find_label(lv_screen_active(), battery_text);
    lv_obj_t *c = find_label(lv_screen_active(), clock_text);
    assert(b && c);
    lv_area_t battery_area, clock_area, parent_area;
    lv_obj_get_coords(b, &battery_area);
    lv_obj_get_coords(c, &clock_area);
    lv_obj_get_coords(lv_obj_get_parent(c), &parent_area);
    assert(clock_area.x1 > battery_area.x2 + 8 && clock_area.x2 <= parent_area.x2);
}

static void capture(pet_state_t state)
{
    for (int i = 0; i < 40; ++i) {
        step(40);
        assert(pet_ui_state() == state);
        save_frame();
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    output_dir = argv[1];
    assert(mkdir(output_dir, 0755) == 0);
    lv_init();
    for (unsigned i = 0; i < pet_font_codepoint_count; ++i) {
        lv_font_glyph_dsc_t glyph;
        assert(lv_font_get_glyph_dsc(&pet_font_18, &glyph, pet_font_codepoints[i], 0));
        assert(!glyph.is_placeholder);
    }
    lv_display_t *display = lv_display_create(W, H);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(indev, display);
    lv_indev_set_read_cb(indev, read_pointer);
    pet_ui_bind_input(indev);
    pet_progress_t fresh;
    pet_progress_default(&fresh);
    pet_ui_create(lv_screen_active(), &fresh);
    capture(PET_IDLE);
    pet_tap(); // Actual pointer events on the cat.
    capture(PET_HAPPY);
    tap(233, 389);
    capture(PET_EATING);
    assert(pet_ui_progress()->satiety == 95);
    assert(pet_ui_progress()->growth_xp == 5);
    tap(233, 389); // Repeated feed must leave the current meal running.
    assert(pet_ui_state() == PET_EATING);
    tap(327, 389);
    capture(PET_SLEEPING);
    pet_tap();
    assert(pet_ui_state() == PET_HAPPY);
    tap(327, 389);
    assert(pet_ui_state() == PET_SLEEPING);
    tap(327, 389);
    assert(pet_ui_state() == PET_IDLE);
    step(60001);
    assert(pet_ui_state() == PET_IDLE);
    tap(233, 389);
    assert(pet_ui_state() == PET_HAPPY); // Already full: wake, without a second meal.
    assert(pet_ui_progress()->satiety == 95 && pet_ui_progress()->meals == 1);
    fresh.satiety = 20;
    fresh.growth_xp = 19;
    pet_ui_create(lv_screen_active(), &fresh);
    step(40);
    save_frame(); // Hungry baby.
    pet_tap();
    assert(pet_ui_progress()->growth_xp == 20);
    step(PET_HAPPY_MS);
    save_frame(); // Young with scarf.
    fresh = *pet_ui_progress();
    fresh.growth_xp = 59;
    fresh.stroke_wait_ms = 0;
    pet_ui_create(lv_screen_active(), &fresh);
    step(40); // Let LVGL lay out the recreated touch targets before input.
    pet_tap();
    assert(pet_ui_progress()->growth_xp == 60);
    step(PET_HAPPY_MS);
    save_frame(); // Adult with medal.
    uint8_t saved[PET_SAVE_BYTES];
    uint32_t generation;
    pet_progress_t restored;
    pet_save_encode(pet_ui_progress(), 42, saved);
    assert(pet_save_decode(saved, sizeof(saved), &restored, &generation));
    assert(generation == 42);
    pet_ui_create(lv_screen_active(), &restored);
    step(40);
    assert(pet_ui_progress()->growth_xp == 60 && pet_ui_progress()->satiety == 20);
    tap(233, 389);
    assert(pet_ui_state() == PET_EATING && pet_ui_progress()->satiety == 45);
    step(PET_EATING_MS);
    assert(pet_ui_state() == PET_HAPPY);
    step(PET_HAPPY_MS);
    assert(pet_ui_state() == PET_IDLE);
    pet_progress_default(&fresh);
    fresh.growth_xp = 19;
    pet_ui_create(lv_screen_active(), &fresh);
    step(40);
    pet_tap();
    tap(233, 389);
    step(PET_EATING_MS + PET_HAPPY_MS);
    assert(pet_ui_progress()->quest_flags == (PET_QUEST_STROKE | PET_QUEST_FEED));
    tap(233, 65);
    step(40); save_frame(); // Companion wishes, two completed.
    tap(233, 52);
    tap(139, 389);
    step(40); save_frame(); // Same-code minigame field.
    assert(pet_ui_progress()->satiety == 90);
    const lv_point_t targets[5] = {{145,195}, {320,195}, {233,260}, {150,325}, {315,325}};
    for (unsigned i = 0; i < 10; ++i) {
        const lv_point_t pos = targets[(i * 2) % 5];
        tap(pos.x, pos.y);
        step(160);
    }
    step(PET_PLAY_MS);
    assert(pet_ui_progress()->games == 1);
    assert(pet_ui_progress()->growth_xp == 36);
    assert(pet_ui_progress()->stars == 9);
    assert(pet_ui_progress()->quest_flags == 0);
    step(40); save_frame(); // Result includes automatic wish reward.
    tap(233, 365);
    tap(139, 389); // Cooldown does not consume more food or start another game.
    assert(pet_ui_progress()->satiety == 90);
    tap(233, 65);
    step(40); save_frame(); // Growth and shop page.
    tap(300, 355); // Crown locked before adulthood, no charge.
    assert(pet_ui_progress()->accessory == 0 && pet_ui_progress()->stars == 9);
    tap(150, 355); // Buy bow.
    assert(pet_ui_progress()->accessory == 1 && pet_ui_progress()->stars == 1);
    tap(233, 52);
    step(40); save_frame(); // Bow actually worn on home pet.
    tap(233, 65);
    tap(233, 420); // Remove, then re-equip without charging.
    assert(pet_ui_progress()->accessory == 0);
    tap(150, 355);
    assert(pet_ui_progress()->accessory == 1 && pet_ui_progress()->stars == 1);
    tap(233, 52);
    fresh = *pet_ui_progress();
    fresh.growth_xp = 60; fresh.stars = 16;
    pet_ui_create(lv_screen_active(), &fresh);
    step(40);
    tap(233, 65);
    tap(300, 355);
    assert(pet_ui_progress()->accessory == 2 && pet_ui_progress()->stars == 0);
    tap(233, 52);
    step(40); save_frame(); // Adult crown.
    pet_progress_default(&fresh);
    fresh.growth_xp = 60;
    fresh.quest_flags = PET_QUEST_FEED | PET_QUEST_PLAY;
    pet_ui_create(lv_screen_active(), &fresh);
    step(40);
    pet_tap();
    assert(pet_ui_progress()->growth_xp == 67 && pet_ui_progress()->stars == 5);
    assert(contains_label(lv_screen_active(), "心愿达成！成长 +7 / 星星 +5"));
    // Exercise the real UI at all level thresholds in every animated state.
    const char *forms[10] = {"奶团幼猫", "蜜桃小猫", "杏糖花猫", "蜜金橘猫", "奶霜长毛",
                             "月光银猫", "星纹灵猫", "星云长毛", "极光灵猫", "星耀守护猫"};
    for (unsigned level = 1; level <= 10; ++level) {
        pet_progress_default(&fresh);
        fresh.growth_xp = (level - 1) * 60;
        pet_ui_create(lv_screen_active(), &fresh);
        step(40);
        assert(contains_label(lv_screen_active(), forms[level - 1]));
        save_frame();
        pet_tap();
        assert(pet_ui_state() == PET_HAPPY);
        save_frame();
        tap(233, 389);
        assert(pet_ui_state() == PET_EATING);
        save_frame();
        tap(327, 389);
        assert(pet_ui_state() == PET_SLEEPING);
        save_frame();
    }
    // Owned bows and crowns stay attached to the scaled head at each eligible level.
    for (unsigned level = 2; level <= 10; ++level) {
        for (unsigned accessory = 1; accessory <= 2; ++accessory) {
            pet_progress_default(&fresh);
            fresh.growth_xp = (level - 1) * 60;
            fresh.unlocked = 3;
            fresh.accessory = accessory;
            pet_ui_create(lv_screen_active(), &fresh);
            step(40);
            save_frame();
        }
    }
    pet_progress_default(&fresh);
    fresh.growth_xp = 20; fresh.unlocked = 1; fresh.accessory = 1;
    pet_ui_create(lv_screen_active(), &fresh);
    step(40); save_frame();
    // Highest-level crown must also fit at the peak of the happy bounce and in meals/sleep.
    pet_progress_default(&fresh);
    fresh.growth_xp = 540; fresh.unlocked = 3; fresh.accessory = 2;
    pet_ui_create(lv_screen_active(), &fresh);
    step(40);
    pet_tap();
    step(180); // The happy pose reaches its largest upward hop near 262ms.
    save_frame();
    tap(233, 389);
    save_frame();
    tap(327, 389);
    save_frame();
    // Live upgrade and save restore use the existing XP, not a separately persisted skin.
    pet_progress_default(&fresh);
    fresh.growth_xp = 59;
    pet_ui_create(lv_screen_active(), &fresh);
    step(40);
    assert(contains_label(lv_screen_active(), forms[0]));
    pet_tap();
    step(PET_HAPPY_MS);
    assert(contains_label(lv_screen_active(), forms[1]));
    assert(contains_label(lv_screen_active(), "进化啦！蜜桃小猫"));
    save_frame();
    pet_save_encode(pet_ui_progress(), 43, saved);
    assert(pet_save_decode(saved, sizeof(saved), &restored, &generation));
    pet_ui_create(lv_screen_active(), &restored);
    step(40);
    assert(contains_label(lv_screen_active(), forms[1]));
    pet_progress_default(&fresh);
    fresh.growth_xp = 60;
    pet_ui_create(lv_screen_active(), &fresh);
    step(40); save_frame(); // 232: battery icon, with no on-screen power button.
    assert(contains_label(lv_screen_active(), "82%"));
    pet_device_preview_battery(pet_battery_decode(0x08, 0x40, 0x08, 9));
    step(40); save_frame();
    assert(contains_label(lv_screen_active(), "9% 请充电"));
    check_header("9% 请充电", "校时");
    pet_device_preview_battery(pet_battery_decode(0x08, 0x40, 0x08, 10));
    step(40); save_frame();
    assert(contains_label(lv_screen_active(), "10%"));
    assert(!contains_label(lv_screen_active(), "10% 请充电"));
    pet_device_preview_battery(pet_battery_decode(0x28, 0x22, 0x08, 9));
    step(40); save_frame();
    assert(contains_label(lv_screen_active(), "9% 充电中"));
    pet_device_preview_battery((pet_battery_t){0});
    step(40); save_frame();
    assert(contains_label(lv_screen_active(), "电量 --"));
    pet_device_preview_battery(pet_battery_decode(0x20, 0x00, 0x08, 0));
    step(40); save_frame();
    assert(contains_label(lv_screen_active(), "未装电池"));
    pet_device_preview_battery(pet_battery_decode(0x08, 0x40, 0x08, 82));
    step(40);
    assert(!contains_label(lv_screen_active(), "熄屏"));
    tap(350, 35); // Blank header area cannot change screen power.
    assert(pet_device_preview_screen_on());
    pet_device_preview_fail_screen();
    pet_device_preview_press_key();
    step(80);
    assert(pet_device_preview_screen_on());
    assert(contains_label(lv_screen_active(), "熄屏失败，请重试"));
    pet_device_preview_press_key();
    step(80);
    assert(!pet_device_preview_screen_on());
    save_frame(); // 238: physical key switched off, touch device disabled.
    for (unsigned i = 0; i < W * H; ++i) assert(pixels[i] == 0);
    const unsigned food_before_wake = pet_ui_progress()->satiety;
    const unsigned xp_before_wake = pet_ui_progress()->growth_xp;
    const unsigned reads_before = pointer_reads;
    tap(233, 389);
    pet_tap();
    tap(303, 35);
    assert(!pet_device_preview_screen_on());
    assert(pointer_reads == reads_before); // No touch reads or widget events while off.
    assert(pet_ui_progress()->satiety == food_before_wake && pet_ui_progress()->growth_xp == xp_before_wake);
    pet_device_preview_fail_screen();
    pet_device_preview_press_key();
    step(80);
    assert(!pet_device_preview_screen_on()); // A failed physical wake remains off.
    // Finger already on the feed button must not turn into a tap when PWR wakes the display.
    pointer = (lv_point_t){233, 389}; pointer_state = LV_INDEV_STATE_PRESSED;
    pet_device_preview_press_key();
    step(80);
    assert(pet_device_preview_screen_on());
    step(160);
    assert(pet_ui_progress()->satiety == food_before_wake && pet_ui_state() == PET_IDLE);
    pointer_state = LV_INDEV_STATE_RELEASED;
    step(80);
    assert(pet_ui_progress()->satiety == food_before_wake);
    save_frame(); // 239: PWR woke screen; held/released touch caused no game action.
    tap(233, 389);
    assert(pet_ui_state() == PET_EATING && pet_ui_progress()->satiety == food_before_wake + 25);
    // The physical key also works with modal pages, which must be restored as-is.
    tap(233, 65);
    assert(contains_label(lv_screen_active(), "返回小窝"));
    pet_device_preview_press_key(); step(80);
    tap(233, 52); // Can't close the menu while the display is off.
    assert(!pet_device_preview_screen_on());
    pet_device_preview_press_key(); step(80);
    assert(contains_label(lv_screen_active(), "返回小窝"));
    tap(233, 52);
    step(PET_EATING_MS + PET_HAPPY_MS);
    tap(139, 389);
    pet_device_preview_press_key(); step(80);
    tap(145, 195); // No star hits while off.
    assert(!pet_device_preview_screen_on());
    pet_device_preview_press_key(); step(80);
    assert(contains_label(lv_screen_active(), "命中 0  /  连击 0"));
    tap(233, 410); // Cancel the round normally after waking.
    pet_progress_default(&fresh);
    pet_ui_create(lv_screen_active(), &fresh);
    step(40); save_frame(); // 240: identical baseline for both modes.
    step(3600000);
    assert(pet_ui_progress()->satiety == 58 && pet_ui_progress()->growth_xp == 6);
    save_frame(); // 241: one hour with the display on.
    pet_ui_create(lv_screen_active(), &fresh);
    step(40);
    pet_device_preview_press_key(); step(80);
    assert(!pet_device_preview_screen_on() && pet_device_preview_rest_requested());
    assert(pet_ui_sleep_begin());
    pet_ui_sleep_advance(3600000, true); // The UI clock does not advance during sleep.
    assert(pet_ui_progress()->satiety == 68 && pet_ui_progress()->growth_xp == 1);
    pet_device_preview_press_key(); step(80);
    assert(pet_device_preview_screen_on() && !pet_device_preview_rest_requested());
    assert(pet_ui_progress()->satiety == 68 && pet_ui_progress()->growth_xp == 1);
    save_frame(); // 242: one hour of rest, restored through PWR.
    // New gesture scenes use actual pointer events; no direct gesture/model injection.
    pet_progress_default(&fresh); fresh.growth_xp = 120;
    pet_ui_create(lv_screen_active(), &fresh); step(40);
    tap(230, 235);
    assert(pet_ui_state() == PET_IDLE && pet_ui_progress()->growth_xp == 120);
    step(PET_TOUCH_GAP_MS + 40);
    assert(contains_label(lv_screen_active(), "摸摸头"));
    assert(pet_ui_progress()->growth_xp == 121);
    save_frame(); // 243: single tap, head pat.
    pet_ui_create(lv_screen_active(), &fresh); step(40);
    tap(230, 235); tap(230, 235); step(PET_TOUCH_GAP_MS + 40);
    assert(pet_ui_progress()->growth_xp == 120 && pet_ui_progress()->satiety == 65);
    assert(pet_ui_progress()->quest_flags == 0); // Double tap never gives stroke credit.
    assert(contains_label(lv_screen_active(), "命中 0  /  连击 0"));
    save_frame(); // 244: double tap opens play.
    tap(233, 410);
    pet_ui_create(lv_screen_active(), &fresh); step(40);
    tap(230, 235); tap(230, 235); tap(230, 235); step(120);
    assert(contains_label(lv_screen_active(), "挠挠肚皮"));
    assert(pet_ui_progress()->growth_xp == 121 && pet_ui_progress()->satiety == 70);
    save_frame(); // 245: triple tap tickles; no double-tap game underneath.
    tap(230, 235); step(PET_TOUCH_GAP_MS + 40); // A fourth tap in the burst is suppressed.
    assert(pet_ui_progress()->growth_xp == 121 && pet_ui_progress()->satiety == 70);
    assert(contains_label(lv_screen_active(), "挠挠肚皮"));
    pet_ui_create(lv_screen_active(), &fresh); step(40);
    pointer = (lv_point_t){230,235}; pointer_state = LV_INDEV_STATE_PRESSED;
    step(40); step(PET_TOUCH_HOLD_MS + 40);
    assert(contains_label(lv_screen_active(), "抱抱你"));
    save_frame(); // 246: long press hug.
    pointer_state = LV_INDEV_STATE_RELEASED; step(40); step(PET_TOUCH_GAP_MS + 40);
    assert(pet_ui_progress()->growth_xp == 121); // Release doesn't add a head pat.
    pet_ui_create(lv_screen_active(), &fresh); step(40);
    swipe(290, 250, 180, 250);
    assert(contains_label(lv_screen_active(), "追逗猫棒"));
    save_frame(); // 247: tease left.
    pet_ui_create(lv_screen_active(), &fresh); step(40);
    swipe(180, 250, 290, 250);
    assert(contains_label(lv_screen_active(), "追逗猫棒"));
    save_frame(); // 248: tease right.
    pet_ui_create(lv_screen_active(), &fresh); step(40);
    swipe(230, 300, 230, 210);
    assert(contains_label(lv_screen_active(), "击掌成功"));
    save_frame(); // 249: upward high five.
    // All affection shares one cooldown, even when the gesture changes.
    swipe(230, 300, 230, 210);
    pet_tap();
    assert(pet_ui_progress()->growth_xp == 121);
    // Meals, menus and PWR cancel an unfinished tap sequence.
    pet_ui_create(lv_screen_active(), &fresh); step(40);
    tap(230, 235); tap(233, 389); step(PET_TOUCH_GAP_MS + 40);
    assert(pet_ui_progress()->growth_xp == 124 && pet_ui_state() == PET_EATING);
    pet_ui_create(lv_screen_active(), &fresh); step(40);
    tap(230, 235); tap(233, 65); step(PET_TOUCH_GAP_MS + 40);
    assert(pet_ui_progress()->growth_xp == 120);
    pet_ui_create(lv_screen_active(), &fresh); step(40);
    tap(230, 235); pet_device_preview_press_key(); step(80);
    step(PET_TOUCH_GAP_MS + 80);
    pet_device_preview_press_key(); step(80); step(PET_TOUCH_GAP_MS + 80);
    assert(pet_ui_progress()->growth_xp == 120);
    // Real wall-clock fixtures exercise the production schedule and display.
    const int64_t local_midnight = INT64_C(20756) * 86400 - PET_LOCAL_OFFSET_S;
    pet_routine_plan_t plan;
    pet_routine_at(local_midnight, &plan);
    assert(pet_device_set_clock(local_midnight + 22 * 3600 + 40 * 60));
    pet_ui_create(lv_screen_active(), &fresh); step(80);
    assert(pet_ui_state() == PET_SLEEPING && contains_label(lv_screen_active(), "夜间好梦"));
    check_header("82%", "22:40");
    save_frame(); // 250: scheduled night sleep; screen remains lit.
    // A pet gesture gives ten minutes of company during scheduled sleep.
    pet_tap(); assert(pet_ui_state() == PET_HAPPY);
    assert(pet_device_preview_screen_on());
    assert(pet_device_set_clock(local_midnight + plan.wake * 60));
    pet_ui_create(lv_screen_active(), &fresh); step(80);
    assert(pet_ui_state() == PET_IDLE); save_frame(); // 251: today's variable morning wake.
    assert(pet_device_set_clock(local_midnight + 13 * 3600 + 20 * 60));
    step(80);
    assert(pet_ui_state() == PET_SLEEPING && contains_label(lv_screen_active(), "午休一会"));
    save_frame(); // 252: scheduled nap.
    pet_device_preview_press_key(); step(80);
    assert(!pet_device_preview_screen_on() && pet_ui_sleep_begin());
    const unsigned reads_at_nap = pointer_reads;
    assert(pet_device_set_clock(local_midnight + 14 * 3600 + 1 * 60));
    pet_ui_sleep_advance(41 * 60000ULL, true);
    assert(pet_ui_state() == PET_IDLE && !pet_device_preview_screen_on());
    assert(pointer_reads == reads_at_nap);
    pet_device_preview_press_key(); step(80); step(80);
    assert(pet_device_preview_screen_on() && pet_ui_state() == PET_IDLE);
    save_frame(); // 253: nap ended while in low power, PWR restores display.
    // Let the production idle service select each activity; do not inject poses.
    const char *activities[] = {"伸个懒腰", "洗洗脸", "自己玩毛线球", "看看四周"};
    for (unsigned activity = 0; activity < 4; ++activity) {
        assert(pet_device_set_clock(local_midnight + 10 * 3600));
        pet_ui_create(lv_screen_active(), &fresh); step(80);
        unsigned tries = 0;
        while (!contains_label(lv_screen_active(), activities[activity]) && tries++ < 10000) step(40);
        assert(tries < 10000 && pet_ui_state() == PET_IDLE);
        const pet_progress_t before_ambient = *pet_ui_progress();
        step(400);
        for (unsigned i = 0; i < 40; ++i) {
            step(100);
            assert(contains_label(lv_screen_active(), activities[activity]));
            save_frame(); // 254..413: four real 4-second ambient animations.
        }
        assert(pet_ui_progress()->growth_xp == before_ambient.growth_xp);
        assert(pet_ui_progress()->stars == before_ambient.stars && pet_ui_progress()->quest_flags == before_ambient.quest_flags);
        // An input immediately clears the autonomous pose before disambiguation.
        tap(230, 235);
        assert(!contains_label(lv_screen_active(), activities[activity]));
        step(PET_TOUCH_GAP_MS + 40);
        assert(pet_ui_state() == PET_HAPPY && contains_label(lv_screen_active(), "摸摸头"));
    }
    // Clock editor: actual pointer targets change date, hour and minute, then apply.
    const int64_t edit_start = local_midnight + 14 * 3600 + 20 * 60;
    assert(pet_device_set_clock(edit_start));
    pet_ui_create(lv_screen_active(), &fresh); step(80);
    tap(278, 35);
    assert(contains_label(lv_screen_active(), "北京时间 UTC+8"));
    tap(233, 314); // Hour minus.
    tap(335, 256); // Minute plus.
    tap(131, 256); // Date plus.
    assert(contains_label(lv_screen_active(), "13 : 21"));
    step(80); save_frame(); // 414: calibration panel.
    tap(233, 380); step(80);
    const pet_clock_t calibrated = pet_device_clock();
    const int64_t expected = edit_start + 86400 - 3600 + 60;
    assert(calibrated.valid && calibrated.utc_s >= expected && calibrated.utc_s < expected + 2);
    assert(pet_ui_state() == PET_SLEEPING && contains_label(lv_screen_active(), "午休一会"));
    assert(pet_ui_progress()->growth_xp == fresh.growth_xp);
    // UI sound routing follows accepted interactions, independent of XP cooldown.
    assert(pet_device_set_clock(local_midnight + 10 * 3600));
    pet_ui_create(lv_screen_active(), &fresh); step(80);
    unsigned head_cues = pet_audio_preview_count(PET_SOUND_HEAD);
    tap(230, 235);
    assert(pet_audio_preview_count(PET_SOUND_HEAD) == head_cues);
    step(PET_TOUCH_GAP_MS + 40);
    assert(pet_audio_preview_count(PET_SOUND_HEAD) == head_cues + 1);
    pet_tap();
    assert(pet_audio_preview_count(PET_SOUND_HEAD) == head_cues + 2);
    assert(pet_ui_progress()->growth_xp == fresh.growth_xp + 1); // Sound doesn't bypass cooldown.
    pet_ui_create(lv_screen_active(), &fresh); step(80);
    unsigned tickle_cues = pet_audio_preview_count(PET_SOUND_TICKLE);
    unsigned play_cues = pet_audio_preview_count(PET_SOUND_PLAY);
    head_cues = pet_audio_preview_count(PET_SOUND_HEAD);
    tap(230, 235); tap(230, 235); tap(230, 235); step(120);
    assert(pet_audio_preview_count(PET_SOUND_TICKLE) == tickle_cues + 1);
    assert(pet_audio_preview_count(PET_SOUND_PLAY) == play_cues && pet_audio_preview_count(PET_SOUND_HEAD) == head_cues);
    pet_ui_create(lv_screen_active(), &fresh); step(80);
    unsigned hug_cues = pet_audio_preview_count(PET_SOUND_HUG);
    pointer = (lv_point_t){230,235}; pointer_state = LV_INDEV_STATE_PRESSED;
    step(40); step(PET_TOUCH_HOLD_MS + 40);
    pointer_state = LV_INDEV_STATE_RELEASED; step(40); step(PET_TOUCH_GAP_MS + 40);
    assert(pet_audio_preview_count(PET_SOUND_HUG) == hug_cues + 1);
    pet_ui_create(lv_screen_active(), &fresh); step(80);
    unsigned tease_cues = pet_audio_preview_count(PET_SOUND_TEASE);
    swipe(290,250,180,250); swipe(180,250,290,250);
    assert(pet_audio_preview_count(PET_SOUND_TEASE) == tease_cues + 2);
    unsigned high_cues = pet_audio_preview_count(PET_SOUND_HIGH_FIVE);
    swipe(230,300,230,210);
    assert(pet_audio_preview_count(PET_SOUND_HIGH_FIVE) == high_cues + 1);
    pet_ui_create(lv_screen_active(), &fresh); step(80);
    unsigned feed_cues = pet_audio_preview_count(PET_SOUND_FEED);
    tap(233,389);
    assert(pet_audio_preview_count(PET_SOUND_FEED) == feed_cues + 1);
    head_cues = pet_audio_preview_count(PET_SOUND_HEAD);
    pet_tap(); tap(233,389);
    assert(pet_audio_preview_count(PET_SOUND_HEAD) == head_cues && pet_audio_preview_count(PET_SOUND_FEED) == feed_cues + 1);
    pet_ui_create(lv_screen_active(), &fresh); step(80);
    tap(230,235); tap(230,235); step(PET_TOUCH_GAP_MS + 40);
    assert(pet_audio_preview_count(PET_SOUND_PLAY) == play_cues + 1);
    unsigned hit_cues = pet_audio_preview_count(PET_SOUND_HIT);
    tap(145,195); step(160); tap(320,195); // The second target is wrong; no hit cue.
    assert(pet_audio_preview_count(PET_SOUND_HIT) == hit_cues + 1);
    unsigned reward_cues = pet_audio_preview_count(PET_SOUND_REWARD);
    for (unsigned i = 1; i < 5; ++i) {
        const lv_point_t pos = targets[(i * 2) % 5];
        tap(pos.x,pos.y); step(160);
    }
    step(PET_PLAY_MS);
    assert(pet_audio_preview_count(PET_SOUND_REWARD) == reward_cues + 1);
    // BOOT fixture represents a debounced physical release, not a screen control.
    pet_ui_create(lv_screen_active(), &fresh); step(80); save_frame(); // 415: sound on.
    const unsigned food_before_boot = pet_ui_progress()->satiety;
    pet_audio_toggle(); step(80);
    assert(!pet_audio_status().enabled && contains_label(lv_screen_active(), "声音已关闭"));
    assert(pet_device_preview_screen_on() && pet_ui_progress()->satiety == food_before_boot);
    assert(pet_ui_progress()->growth_xp == fresh.growth_xp);
    save_frame(); // 416: muted indicator + toast.
    head_cues = pet_audio_preview_count(PET_SOUND_HEAD);
    pet_tap();
    assert(pet_audio_preview_count(PET_SOUND_HEAD) == head_cues);
    assert(pet_ui_state() == PET_HAPPY && pet_ui_progress()->growth_xp == fresh.growth_xp + 1);
    unsigned enabled_cues = pet_audio_preview_count(PET_SOUND_ENABLED);
    pet_audio_toggle(); step(80);
    assert(pet_audio_preview_count(PET_SOUND_ENABLED) == enabled_cues + 1);
    pet_device_preview_press_key(); step(80);
    assert(!pet_device_preview_screen_on());
    pet_audio_toggle(); pet_audio_toggle(); step(80);
    pet_audio_play(PET_SOUND_HEAD); pet_tap();
    assert(pet_audio_preview_count(PET_SOUND_HEAD) == head_cues);
    assert(pet_audio_preview_count(PET_SOUND_ENABLED) == enabled_cues + 1);
    assert(!pet_device_preview_screen_on()); // BOOT toggles silently in the dark.
    pet_device_preview_press_key(); step(80); step(80);
    assert(pet_device_preview_screen_on() && pet_audio_status().enabled);
    pet_ui_create(lv_screen_active(), &fresh); step(80);
    pet_audio_preview_available(false); step(80);
    assert(contains_label(lv_screen_active(), "音效暂不可用")); save_frame(); // 417: audio fault, gameplay still usable.
    pet_tap();
    assert(pet_ui_state() == PET_HAPPY && pet_audio_preview_count(PET_SOUND_HEAD) == head_cues);
    pet_audio_preview_available(true);
    pet_ui_destroy();
    lv_deinit();
    puts("PASS: same-code rendering, touch targets, feeding/full feedback, all growth stages, save restore, game scoring, wish rewards, cooldown, shop/equip, 10 level forms x 4 states, 19 accessory fits, live evolution/restore, battery/low/charging/absent/error, physical PWR off/wake/failure, no touch reads/actions off, held-touch release guard, modal/game power control, one-hour bright/rest 6x progression and paused-clock recovery, 1/2/3 taps/hold/swipes, no gesture fallthrough, shared rewards and cancellation");
    puts("PASS: scheduled night/morning/nap/afternoon, touch wake, nap ends during low power without display/touch wake, four autonomous animations and input precedence, no ambient rewards, UTC+8 date/hour/minute calibration with real pointers");
    puts("PASS: accepted gestures/feeding/game hit/reward route cues once, no tap fallthrough or busy cues, XP cooldown independent, physical mute feedback and icon, muted gameplay, BOOT on/off while dark cannot light display or play confirmation, unavailable-audio fallback");
}
