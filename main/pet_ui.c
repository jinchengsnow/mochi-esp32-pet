// SPDX-License-Identifier: Apache-2.0
#include "pet_ui.h"
#include "pet_font.h"
#include "pet_store.h"
#include "pet_device.h"
#include "pet_touch.h"
#include "pet_clock_build.h"
#include "pet_audio.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

enum { BG = 0x12121e, INK = 0x59413e, CREAM = 0xffe8ca,
       FUR = 0xf5cf9d, PINK = 0xf7a7ae, MINT = 0xb7e4ce,
       MUTED = 0x9d97ae, LILAC = 0xcbb4f6 };

typedef struct { lv_layer_t *layer; int x, y; float sx, sy; } brush_t;

// Every level has its own palette and anatomy. Derived from existing XP; no save migration.
typedef struct {
    const char *name;
    uint32_t coat, face, markings, iris, accent, ruff;
    uint8_t fluff, forehead, tail_bands, aura;
} cat_look_t;
static const cat_look_t cat_looks[10] = {
    {"奶团幼猫", 0xf5d7b7, 0xffefd8, 0xe6bf99, 0x78584c, 0xf5bdc2, 0xfff5e7, 0, 0, 0, 0},
    {"蜜桃小猫", 0xf0bc98, 0xffe5ce, 0xdc9c78, 0x805c51, 0xf0a8b3, 0xfff0df, 0, 0, 1, 0},
    {"杏糖花猫", 0xeebc84, 0xffe6c8, 0xd89958, 0x83604e, 0xe6b977, 0xffefdc, 1, 0, 2, 0},
    {"蜜金橘猫", 0xe5ab61, 0xffe4b4, 0xc88845, 0x826344, 0xf0cc85, 0xffeac5, 1, 0, 3, 0},
    {"奶霜长毛", 0xe7bb87, 0xfff0d6, 0xcf995f, 0x7f6651, 0xdec5a7, 0xfff7e7, 2, 0, 3, 0},
    {"月光银猫", 0xc9c7e4, 0xefefff, 0x9c99c3, 0x68678c, 0xc4b4ec, 0xf8f4ff, 2, 1, 2, 1},
    {"星纹灵猫", 0xcbb8e7, 0xf2ecff, 0x9a80c6, 0x765795, 0xdfb9ee, 0xf4e6ff, 2, 2, 3, 1},
    {"星云长毛", 0xbb9bdf, 0xf1e4ff, 0x9877c9, 0x78548d, 0xcfacf7, 0xe1cef6, 3, 2, 4, 2},
    {"极光灵猫", 0x9ecfc8, 0xe4f4ef, 0x73b6b3, 0x4a7b80, 0xb5a2e4, 0xe0cdf3, 3, 1, 4, 2},
    {"星耀守护猫", 0xf0c77c, 0xffedc9, 0xc89250, 0x916a48, 0xf6d88c, 0xfff5dc, 3, 3, 4, 3},
};

static struct {
    pet_model_t pet;
    pet_touch_t touch;
    lv_obj_t *stage, *status, *hint, *sleep_label, *battery_label, *battery_row, *battery_icon, *blanker;
    lv_obj_t *clock_label, *sound_icon;
    pet_audio_status_t sound;
    lv_obj_t *save_label, *food_label, *growth_label, *food_fill, *growth_fill;
    lv_obj_t *buttons[4], *name, *overlay, *panel_title, *panel_info, *panel_note, *arena, *shop[2];
    int page; /* 0 home, 1 growth, 2 game, 3 results, 4 clock */
    int64_t clock_edit;
    uint16_t before_xp, before_stars;
    lv_timer_t *timer;
    pet_battery_t battery;
    uint32_t shown_battery;
    pet_state_t shown;
    int shown_food, shown_xp, shown_store;
    uint32_t queued_revision, last_save, toast_until, evolution_since;
    unsigned shown_level;
    bool evolution_pending, evolving, screen_off;
    const char *toast;
    char feedback[96], evolution_text[64];
} ui;
static lv_indev_t *touch_input;

static const cat_look_t *cat_look(void) { return &cat_looks[pet_model_level(&ui.pet) - 1]; }

static void sync_panel(void);
static void open_growth(lv_event_t *e);
static void start_play(lv_event_t *e);
static void apply_gesture(pet_gesture_t gesture);
static void open_clock(lv_event_t *e);
static void sync_sound(void);
static lv_obj_t *label(lv_obj_t *parent, const char *value, int y, int width,
                       const lv_font_t *font, uint32_t color);

static float scale_x(brush_t b) { return b.sx ? b.sx : 1.0f; }
static float scale_y(brush_t b) { return b.sy ? b.sy : 1.0f; }
static int px(brush_t b, int x) { return b.x + lroundf(x * scale_x(b)); }
static int py(brush_t b, int y) { return b.y + lroundf(y * scale_y(b)); }

static void box(brush_t b, int x, int y, int w, int h, int radius, uint32_t color, uint8_t opa)
{
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_hex(color);
    d.bg_opa = opa;
    d.radius = radius == LV_RADIUS_CIRCLE ? radius : lroundf(radius * fminf(scale_x(b), scale_y(b)));
    lv_area_t a = {px(b, x), py(b, y), px(b, x + w) - 1, py(b, y + h) - 1};
    lv_draw_rect(b.layer, &d, &a);
}

static void dot(brush_t b, int x, int y, int r, uint32_t color, uint8_t opa)
{
    box(b, x - r, y - r, r * 2, r * 2, LV_RADIUS_CIRCLE, color, opa);
}

static void line(brush_t b, int x1, int y1, int x2, int y2, int w, uint32_t color, uint8_t opa)
{
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.p1 = (lv_point_precise_t){px(b, x1), py(b, y1)};
    d.p2 = (lv_point_precise_t){px(b, x2), py(b, y2)};
    d.width = fmaxf(1, roundf(w * fminf(scale_x(b), scale_y(b))));
    d.color = lv_color_hex(color);
    d.opa = opa;
    d.round_start = d.round_end = true;
    lv_draw_line(b.layer, &d);
}

static void arc(brush_t b, int x, int y, int r, int start, int end, int w, uint32_t color)
{
    lv_draw_arc_dsc_t d;
    lv_draw_arc_dsc_init(&d);
    d.center = (lv_point_t){px(b, x), py(b, y)};
    d.radius = lroundf(r * fminf(scale_x(b), scale_y(b)));
    d.start_angle = start;
    d.end_angle = end;
    d.width = fmaxf(1, roundf(w * fminf(scale_x(b), scale_y(b))));
    d.color = lv_color_hex(color);
    d.rounded = true;
    lv_draw_arc(b.layer, &d);
}

static void tri(brush_t b, int x1, int y1, int x2, int y2, int x3, int y3,
                uint32_t color, uint8_t opa)
{
    lv_draw_triangle_dsc_t d;
    lv_draw_triangle_dsc_init(&d);
    d.p[0] = (lv_point_precise_t){px(b, x1), py(b, y1)};
    d.p[1] = (lv_point_precise_t){px(b, x2), py(b, y2)};
    d.p[2] = (lv_point_precise_t){px(b, x3), py(b, y3)};
    d.color = lv_color_hex(color);
    d.opa = opa;
    lv_draw_triangle(b.layer, &d);
}

static void heart(brush_t b, int x, int y, int s, uint8_t opa)
{
    dot(b, x - s / 2, y, s / 2 + 1, PINK, opa);
    dot(b, x + s / 2, y, s / 2 + 1, PINK, opa);
    tri(b, x - s, y, x + s, y, x, y + s + 1, PINK, opa);
}

static void star(brush_t b, int x, int y, int r, uint32_t color, uint8_t opa)
{
    tri(b, x, y - r, x + r / 3, y, x - r / 3, y, color, opa);
    tri(b, x, y + r, x + r / 3, y, x - r / 3, y, color, opa);
    tri(b, x - r, y, x, y - r / 3, x, y + r / 3, color, opa);
    tri(b, x + r, y, x, y - r / 3, x, y + r / 3, color, opa);
}

static void text(brush_t b, int x, int y, int w, const char *value, uint32_t color, uint8_t opa)
{
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.text = value;
    d.font = &lv_font_montserrat_20;
    d.color = lv_color_hex(color);
    d.opa = opa;
    d.align = LV_TEXT_ALIGN_CENTER;
    lv_area_t a = {b.x + x, b.y + y, b.x + x + w - 1, b.y + y + 27};
    lv_draw_label(b.layer, &d, &a);
}

static void bowl(brush_t b, int x, int y, bool food)
{
    if (food) {
        dot(b, x - 11, y - 1, 5, 0xd99c6c, 255);
        dot(b, x + 1, y - 3, 6, 0xe9b17d, 255);
        dot(b, x + 12, y, 5, 0xd99c6c, 255);
    }
    box(b, x - 27, y + 1, 54, 22, 11, MINT, 255);
    box(b, x - 29, y, 58, 7, 3, 0xd2f4e5, 255);
    dot(b, x, y + 14, 4, 0x699b85, 255);
}

static brush_t event_brush(lv_event_t *e)
{
    lv_area_t a;
    lv_obj_get_coords(lv_event_get_target_obj(e), &a);
    return (brush_t){.layer = lv_event_get_layer(e), .x = a.x1, .y = a.y1};
}

static void draw_pet(lv_event_t *e)
{
    brush_t b = event_brush(e);
    const uint32_t now = lv_tick_get();
    const pet_state_t state = ui.pet.state;
    const pet_pose_t p = pet_model_pose(&ui.pet, now);
    const cat_look_t *look = cat_look();
    const unsigned level = pet_model_level(&ui.pet);
    const int bob = lroundf(p.bob), sway = lroundf(p.sway);
    const bool sleeping = state == PET_SLEEPING;
    const float body_scale = 0.72f + 0.042f * (level - 1);
    const float head_scale = 0.88f + 0.014f * (level - 1);
    // Feet stay anchored while torso, head ratio, ear tufts and coat grow with level.
    brush_t body = {.layer = b.layer, .x = b.x + lroundf(178 * (1 - body_scale)),
        .y = b.y + lroundf(201 * (1 - body_scale)) + bob / 2, .sx = body_scale, .sy = body_scale};
    brush_t h = {.layer = b.layer, .x = b.x + lroundf(178 * (1 - head_scale)) + sway,
        .y = b.y + lroundf(175 * (1 - head_scale) + 5 - 1.4f * (level - 1)) + bob + (sleeping ? 5 : 0),
        .sx = head_scale, .sy = head_scale};
    if (ui.pet.ambient == PET_AMBIENT_STRETCH && state == PET_IDLE)
        h.y += lroundf(p.paw * 7);

    // All effects and moving art stay inside the 356 x 214 stage.
    box(b, 99 - level, 193, 158 + level * 2, 11, 6, 0x090910, 190);
    if (look->aura) {
        arc(b, 178, 115, 98, 208, 285, 2, look->accent);
        arc(b, 178, 115, 98, 24, 70, 2, look->accent);
        if (look->aura >= 2) {
            arc(b, 178, 115, 105, 120, 162, 2, level == 9 ? MINT : LILAC);
            arc(b, 178, 115, 105, 310, 350, 2, level == 9 ? LILAC : PINK);
        }
        if (look->aura == 3) {
            star(b, 77, 81, 9, look->accent, 230);
            star(b, 287, 136, 8, LILAC, 230);
        }
    }
    if (ui.evolving && now - ui.evolution_since < 2800) {
        const float phase = (now - ui.evolution_since) / 2800.0f;
        for (int i = 0; i < 6; ++i) {
            const float angle = i * 1.047198f + phase * 0.9f;
            star(b, 178 + lroundf(cosf(angle) * 113), 110 + lroundf(sinf(angle) * 81),
                 4 + (int)(3 * sinf(phase * 3.141593f)), look->accent, (uint8_t)(255 * (1 - phase)));
        }
    }
    star(b, 48, 97, 5, look->accent, 100);
    star(b, 304, 126, 4, look->accent, 100);
    dot(b, 66, 143, 2, LILAC, 130);
    dot(b, 290, 65, 2, PINK, 130);

    // Tail thickens, gains bands, then a pale fluffy tip.
    arc(body, 246, 165, 30, 275 + (int)p.tail, 105, look->fluff >= 2 ? 22 : 18, look->coat);
    for (unsigned i = 0; i < look->tail_bands; ++i)
        arc(body, 246, 165, 30, (300 + i * 30) % 360, (309 + i * 30) % 360, look->fluff >= 2 ? 23 : 19, look->markings);
    if (look->fluff >= 2) dot(body, 249, 137, 11, look->ruff, 255);
    box(body, 121, 125, 114, 69, 32, look->coat, 255);
    box(body, 139, 144, 77, 49, 23, look->ruff, 255);
    if (look->fluff >= 2) {
        tri(body, 122, 157, 117, 177, 145, 166, look->ruff, 255);
        tri(body, 232, 157, 239, 177, 210, 166, look->ruff, 255);
        tri(body, 159, 172, 177, 196, 194, 172, look->ruff, 255);
    }
    if (look->fluff == 3) {
        for (int i = 0; i < 5; ++i) dot(body, 140 + i * 19, 164, 14, look->ruff, 255);
        star(body, 177, 185, 7, look->accent, 255);
    }
    box(body, 115, 181, 46, 21, 10, look->face, 255);
    box(body, 195, 181, 46, 21, 10, look->face, 255);
    line(body, 131, 190, 131, 195, 2, look->markings, 160);
    line(body, 142, 190, 142, 195, 2, look->markings, 160);
    line(body, 212, 190, 212, 195, 2, look->markings, 160);
    line(body, 223, 190, 223, 195, 2, look->markings, 160);

    const int ear_tip = 35 - (int)(level < 5 ? level : 5);
    tri(h, 99, ear_tip, 100, 102, 160, 68, look->coat, 255);
    tri(h, 256, ear_tip, 256, 102, 197, 68, look->coat, 255);
    dot(h, 103, ear_tip + 3, 5, look->coat, 255);
    dot(h, 252, ear_tip + 3, 5, look->coat, 255);
    tri(h, 110, ear_tip + 16, 112, 81, 142, 67, PINK, 210);
    tri(h, 246, ear_tip + 16, 243, 81, 214, 67, PINK, 210);
    if (look->fluff >= 2) {
        tri(h, 99, ear_tip + 7, 91, ear_tip - 4, 113, ear_tip + 12, look->ruff, 255);
        tri(h, 244, ear_tip + 10, 264, ear_tip - 4, 256, ear_tip + 17, look->ruff, 255);
    }
    if (look->fluff) {
        tri(h, 101, 118, 79, 128, 104, 141, look->face, 255);
        tri(h, 255, 118, 277, 128, 252, 141, look->face, 255);
    }
    if (look->fluff >= 2) {
        tri(h, 100, 139, 81, 154, 117, 164, look->face, 255);
        tri(h, 254, 139, 275, 154, 238, 164, look->face, 255);
    }
    box(h, 90, 58, 176, 119, 59, look->face, 255);
    // The three soft forehead stripes keep MOCHI recognizable across all palettes.
    box(h, 154, 58, 13, 22, 6, look->markings, 180);
    box(h, 175, 58, 12, 28, 6, look->markings, 180);
    box(h, 195, 59, 11, 20, 5, look->markings, 180);
    if (level >= 3) {
        box(h, 100, 118, 20, 9, 4, look->markings, 140);
        box(h, 101, 133, 16, 7, 3, look->markings, 140);
        box(h, 235, 118, 20, 9, 4, look->markings, 140);
        box(h, 239, 133, 15, 7, 3, look->markings, 140);
    }
    if (look->forehead == 1) {
        dot(h, 178, 95, 10, look->accent, 255);
        dot(h, 183, 91, 8, look->face, 255);
    } else if (look->forehead >= 2) {
        star(h, 178, 95, look->forehead == 3 ? 14 : 10, look->forehead == 3 ? 0xe6b956 : look->accent, 255);
        if (look->forehead == 3) dot(h, 178, 95, 3, LILAC, 255);
    }

    if (p.eyes < 0.15f) {
        if (state == PET_HAPPY) {
            arc(h, 140, 123, 13, 205, 335, 4, INK);
            arc(h, 216, 123, 13, 205, 335, 4, INK);
        } else {
            arc(h, 140, 113, 13, 20, 160, 4, INK);
            arc(h, 216, 113, 13, 20, 160, 4, INK);
        }
    } else {
        const int height = (int)((23 - level / 3) * p.eyes) + 2;
        box(h, 132 + (int)p.gaze, 121 - height / 2, 16, height, 8, INK, 255);
        box(h, 208 + (int)p.gaze, 121 - height / 2, 16, height, 8, INK, 255);
        if (height >= 9) {
            box(h, 135 + (int)p.gaze, 123 - height / 2, 10, height - 4, 5, look->iris, 255);
            box(h, 211 + (int)p.gaze, 123 - height / 2, 10, height - 4, 5, look->iris, 255);
        }
        if (p.eyes > 0.65f) {
            dot(h, 136 + (int)p.gaze, 116, 3, 0xfffaf5, 255);
            dot(h, 212 + (int)p.gaze, 116, 3, 0xfffaf5, 255);
        }
    }
    box(h, 117, 137, 20, 8, 4, PINK, sleeping ? 90 : 170);
    box(h, 219, 137, 20, 8, 4, PINK, sleeping ? 90 : 170);
    tri(h, 173, 129, 184, 129, 178, 135, 0xca8a82, 255);
    if (state == PET_EATING || (state == PET_HAPPY && ui.pet.emote == PET_EMOTE_TICKLE)) {
        box(h, 171, 138, 15, 5 + (int)(p.mouth * 9), 7, INK, 255);
        dot(h, 178, 144, 3, PINK, 220);
    } else {
        arc(h, 171, 136, 9, 15, 100, 2, INK);
        arc(h, 185, 136, 9, 80, 165, 2, INK);
    }
    line(h, 109, 126, 92, 123, 2, look->markings, 210);
    line(h, 108, 136, 91, 139, 2, look->markings, 210);
    line(h, 247, 126, 264, 123, 2, look->markings, 210);
    line(h, 248, 136, 265, 139, 2, look->markings, 210);

    const pet_growth_t growth = pet_model_growth(&ui.pet);
    if (growth != PET_BABY) {
        const uint32_t scarf = level >= 5 ? look->accent : growth == PET_YOUNG ? MINT : LILAC;
        box(body, 143, 172, 70, 10, 5, scarf, 255);
        tri(body, 194, 180, 211, 180, 211, 195, scarf, 255);
        if (growth == PET_ADULT) star(body, 176, 188, 7, level >= 6 ? look->face : 0xf9d9a4, 255);
    }
    // Purchased accessories follow the transformed head, never replace the level's anatomy.
    if (ui.pet.progress.accessory == 1) {
        tri(h, 215, 64, 239, 51, 239, 77, PINK, 255);
        tri(h, 215, 64, 191, 51, 191, 77, PINK, 255);
        dot(h, 215, 64, 6, LILAC, 255);
    } else if (ui.pet.progress.accessory == 2) {
        box(h, 151, 45, 54, 16, 4, 0xf9d16b, 255);
        tri(h, 151, 49, 147, 30, 169, 46, 0xf9d16b, 255);
        tri(h, 164, 49, 178, 26, 192, 49, 0xf9d16b, 255);
        tri(h, 187, 49, 209, 30, 205, 49, 0xf9d16b, 255);
        dot(h, 178, 49, 4, PINK, 255);
    }
    // Autonomous daytime animation: independent from rewarding affection gestures.
    if (state == PET_IDLE && ui.pet.ambient != PET_AMBIENT_NONE) {
        const float a = (now - ui.pet.ambient_since) / 1000.0f;
        if (ui.pet.ambient == PET_AMBIENT_STRETCH) {
            const int reach = lroundf(p.paw * 18);
            line(body, 134, 170, 108 - reach, 191, 15, look->coat, 255);
            line(body, 222, 170, 248 + reach, 191, 15, look->coat, 255);
            dot(body, 108 - reach, 193, 12, look->face, 255);
            dot(body, 248 + reach, 193, 12, look->face, 255);
        } else if (ui.pet.ambient == PET_AMBIENT_GROOM) {
            const int hand_y = 125 + lroundf(p.paw * 19);
            line(h, 120, 171, 123, hand_y + 10, 16, look->coat, 255);
            dot(h, 123, hand_y, 13, look->face, 255);
            dot(h, 123, hand_y + 3, 5, PINK, 210);
            line(b, 99, 132, 92, 127, 2, MINT, 180);
        } else if (ui.pet.ambient == PET_AMBIENT_BALL) {
            const int ball_x = 281 + lroundf(22 * sinf(a * 2.5f));
            dot(b, ball_x, 187, 16, LILAC, 255);
            arc(b, ball_x, 187, 11, 30 + (int)(a * 130) % 360, 170 + (int)(a * 130) % 360, 2, 0x8068a8);
            line(b, ball_x - 10, 177, ball_x + 9, 194, 2, 0x8068a8, 210);
            line(b, ball_x + 12, 196, ball_x + 23, 199, 2, LILAC, 210);
            line(body, 221, 168, 250, 176 + lroundf(p.paw * 12), 15, look->coat, 255);
            dot(body, 251, 176 + lroundf(p.paw * 12), 12, look->face, 255);
        } else if (ui.pet.ambient == PET_AMBIENT_WATCH) {
            const int bx = 178 + lroundf(102 * sinf(a * 1.7f));
            const int by = 40 + lroundf(7 * sinf(a * 3));
            const int wing = 5 + lroundf(4 * fabsf(sinf(a * 12)));
            dot(b, bx - wing, by, wing, PINK, 240);
            dot(b, bx + wing, by, wing, LILAC, 240);
            line(b, bx, by - 5, bx, by + 8, 2, MINT, 240);
        }
    }
    // Affection changes body posture as well as the face.
    if (state == PET_HAPPY && ui.pet.emote == PET_EMOTE_HUG) {
        line(body, 137, 160, 172, 182, 15, look->coat, 255);
        line(body, 219, 160, 184, 182, 15, look->coat, 255);
        dot(body, 172, 180, 9, look->face, 255);
        dot(body, 184, 180, 9, look->face, 255);
        heart(b, 80, 116, 9 + lroundf(p.cuddle * 3), 220);
        heart(b, 285, 142, 7 + lroundf(p.cuddle * 3), 210);
    }
    if (state == PET_HAPPY && (ui.pet.emote == PET_EMOTE_HIGH_FIVE ||
        ui.pet.emote == PET_EMOTE_TEASE_LEFT || ui.pet.emote == PET_EMOTE_TEASE_RIGHT)) {
        const bool left = ui.pet.emote == PET_EMOTE_TEASE_LEFT;
        const int hand_x = left ? 102 : 254;
        const int hand_y = ui.pet.emote == PET_EMOTE_HIGH_FIVE ? 128 : 149;
        line(body, left ? 135 : 221, 174, hand_x, hand_y + 10, 16, look->coat, 255);
        dot(body, hand_x, hand_y, 15, look->face, 255);
        dot(body, hand_x, hand_y + 4, 6, PINK, 230);
        for (int i = -1; i <= 1; ++i) dot(body, hand_x + i * 8, hand_y - 7, 4, PINK, 230);
        if (ui.pet.emote == PET_EMOTE_HIGH_FIVE) {
            star(b, 282, 86, 10, look->accent, 255);
            line(b, 278, 108, 288, 105, 3, PINK, 230);
        } else {
            const int toy_x = left ? 58 : 298;
            line(b, toy_x, 55, toy_x + (left ? 12 : -12), 111, 2, LILAC, 230);
            dot(b, toy_x + (left ? 12 : -12), 118, 10, look->accent, 255);
            tri(b, toy_x - 2, 116, toy_x + 17, 128, toy_x + 18, 108, PINK, 230);
        }
    }
    if (state == PET_HAPPY && ui.pet.emote == PET_EMOTE_TICKLE) {
        const int paw_y = 156 - lroundf(p.paw * 12);
        line(body, 136, 165, 116, paw_y, 12, look->face, 255);
        line(body, 220, 165, 242, paw_y, 12, look->face, 255);
        dot(body, 116, paw_y, 9, look->face, 255);
        dot(body, 242, paw_y, 9, look->face, 255);
        dot(body, 116, paw_y + 1, 4, PINK, 240);
        dot(body, 242, paw_y + 1, 4, PINK, 240);
        star(b, 78, 151, 8, PINK, 220);
        star(b, 285, 165, 7, look->accent, 230);
    }
    if ((ui.touch.pressed && !ui.touch.claimed) || ui.touch.taps) {
        int tx = ui.touch.x - b.x, ty = ui.touch.y - b.y;
        if (tx >= 20 && tx <= 336 && ty >= 20 && ty <= 194)
            arc(b, tx, ty, 18 + ui.touch.taps * 3, 205, 335, 2, PINK);
    }
    if (state == PET_HAPPY) {
        const int lift = (int)(p.sparkle * 13);
        heart(b, 69, 60 - lift, 11, 230);
        heart(b, 282, 41 + lift, 8, 190);
        star(b, 300, 93, 7, look->accent, 255);
    } else if (state == PET_EATING) {
        bowl(b, 178, 183, true);
        const int crumb = (int)((now - ui.pet.state_since) % 420) / 30;
        dot(b, 168, 158 + crumb, 3, look->coat, 220);
        dot(b, 190, 166 + crumb, 2, look->coat, 200);
    } else if (sleeping) {
        const int drift = (int)((now - ui.pet.state_since) % 2000) / 140;
        text(b, 265, 70 - drift, 25, "z", LILAC, 230);
        text(b, 287, 50 - drift, 28, "Z", LILAC, 180);
        text(b, 310, 31 - drift, 28, "Z", LILAC, 120);
    } else if (ui.pet.progress.satiety <= 25) {
        bowl(b, 291, 65, false);
    }
}

static void draw_icon(lv_event_t *e)
{
    brush_t b = event_brush(e);
    const pet_action_t action = (pet_action_t)(uintptr_t)lv_event_get_user_data(e);
    if ((unsigned)action == 3) {
        star(b, 38, 33, 17, 0xf9d9a4, 255);
        dot(b, 60, 20, 3, LILAC, 255);
    } else if (action == PET_STROKE) {
        heart(b, 38, 29, 10, 255);
        line(b, 25, 45, 50, 45, 3, PINK, 150);
    } else if (action == PET_FEED) {
        // The same bowl at half size, kept inside the button.
        dot(b, 30, 28, 4, 0xe9b17d, 255);
        dot(b, 39, 25, 4, FUR, 255);
        dot(b, 46, 29, 4, 0xe9b17d, 255);
        box(b, 19, 31, 38, 17, 8, MINT, 255);
        box(b, 18, 30, 40, 5, 2, 0xd2f4e5, 255);
    } else if (ui.pet.state == PET_SLEEPING) {
        dot(b, 38, 33, 9, 0xf9d9a4, 255);
        for (int i = 0; i < 8; ++i) {
            const float a = i * 0.785398f;
            line(b, 38 + (int)(14 * cosf(a)), 33 + (int)(14 * sinf(a)),
                 38 + (int)(18 * cosf(a)), 33 + (int)(18 * sinf(a)), 2, 0xf9d9a4, 255);
        }
    } else {
        dot(b, 36, 32, 14, LILAC, 255);
        dot(b, 43, 25, 12, 0x272334, 255);
        star(b, 53, 40, 5, LILAC, 255);
    }
}

static void sync_labels(void)
{
    const pet_progress_t *p = &ui.pet.progress;
    const pet_store_status_t store = pet_store_status();
    if (ui.shown_store != (int)store) {
        ui.shown_store = store;
        const char *save = store == PET_STORE_SAVED ? "已保存" :
                           store == PET_STORE_SAVING ? "保存中" : "存档异常";
        lv_label_set_text(ui.save_label, save);
        lv_obj_set_style_text_color(ui.save_label, lv_color_hex(store == PET_STORE_ERROR ? PINK : MUTED), 0);
    }
    if (ui.shown_food != p->satiety) {
        ui.shown_food = p->satiety;
        lv_label_set_text_fmt(ui.food_label, "饱食 %u/100", p->satiety);
        lv_obj_set_width(ui.food_fill, 124 * p->satiety / 100);
        lv_obj_set_style_bg_color(ui.food_fill, lv_color_hex(p->satiety <= 25 ? PINK : MINT), 0);
    }
    if (ui.shown_xp != p->growth_xp) {
        ui.shown_xp = p->growth_xp;
        const unsigned level = pet_model_level(&ui.pet);
        if (ui.shown_level && level > ui.shown_level) ui.evolution_pending = true;
        ui.shown_level = level;
        lv_label_set_text_fmt(ui.name, "MOCHI  Lv.%u", level);
        const pet_growth_t growth = pet_model_growth(&ui.pet);
        const char *name = growth == PET_BABY ? "幼崽" : growth == PET_YOUNG ? "少年" : "成年";
        if (growth == PET_ADULT) lv_label_set_text_fmt(ui.growth_label, "%s %u/600", name, p->growth_xp);
        else lv_label_set_text_fmt(ui.growth_label, "%s %u/%u", name,
                                  growth == PET_BABY ? p->growth_xp : p->growth_xp - PET_YOUNG_XP,
                                  growth == PET_BABY ? PET_YOUNG_XP : PET_ADULT_XP - PET_YOUNG_XP);
        unsigned amount = growth == PET_BABY ? p->growth_xp * 124 / PET_YOUNG_XP :
                          growth == PET_YOUNG ? (p->growth_xp - PET_YOUNG_XP) * 124 /
                                               (PET_ADULT_XP - PET_YOUNG_XP) :
                          p->growth_xp * 124 / PET_MAX_XP;
        lv_obj_set_width(ui.growth_fill, amount);
    }
    if (ui.evolution_pending && ui.page == 0) {
        ui.evolution_pending = false;
        ui.evolving = true;
        ui.evolution_since = lv_tick_get();
        snprintf(ui.evolution_text, sizeof(ui.evolution_text), "进化啦！%s", cat_look()->name);
        ui.toast = ui.evolution_text;
        ui.toast_until = lv_tick_get() + 2800;
        pet_audio_play(PET_SOUND_LEVEL);
    }
    if (ui.evolving && lv_tick_get() - ui.evolution_since >= 2800) ui.evolving = false;
    const char *status, *hint;
    uint32_t color;
    switch (ui.pet.state) {
    case PET_HAPPY:
        color = PINK;
        switch (ui.pet.emote) {
        case PET_EMOTE_HEAD: status = "摸摸头"; hint = "喜欢你摸摸我"; break;
        case PET_EMOTE_TICKLE: status = "挠挠肚皮"; hint = "哈哈哈，肚皮好痒"; break;
        case PET_EMOTE_HUG: status = "抱抱你"; hint = "靠着你，暖暖的"; break;
        case PET_EMOTE_TEASE_LEFT:
        case PET_EMOTE_TEASE_RIGHT: status = "追逗猫棒"; hint = "抓到啦，再来一次"; break;
        case PET_EMOTE_HIGH_FIVE: status = "击掌成功"; hint = "小爪爪，和你碰一下"; break;
        default: status = "超开心"; hint = "一起玩真开心"; break;
        }
        break;
    case PET_EATING:
        status = "开饭啦"; hint = "啊呜，好好吃"; color = MINT; break;
    case PET_SLEEPING:
        status = ui.pet.routine == PET_ROUTINE_NIGHT ? "夜间好梦" :
                 ui.pet.routine == PET_ROUTINE_NAP ? "午休一会" : "做个好梦";
        hint = ui.pet.routine == PET_ROUTINE_NIGHT || ui.pet.routine == PET_ROUTINE_NAP ?
               "轻轻摸醒，陪伴10分钟再休息" : "轻轻碰我，就会醒来";
        color = LILAC; break;
    default:
        status = p->satiety <= 25 ? "肚子饿啦" : cat_look()->name;
        hint = p->satiety <= 25 ? "想吃一口，点点喂食" : (lv_tick_get() / 4000 % 2 ? "长按抱抱 · 左右逗猫 · 上滑击掌" : "单击摸头 · 双击玩耍 · 三击挠肚皮");
        color = p->satiety <= 25 ? PINK : MINT; break;
    }
    if (ui.pet.state == PET_IDLE && p->satiety > 25) {
        switch (ui.pet.ambient) {
        case PET_AMBIENT_STRETCH: status = "伸个懒腰"; hint = "爪爪伸长，舒服一下"; break;
        case PET_AMBIENT_GROOM: status = "洗洗脸"; hint = "把自己打理得干干净净"; break;
        case PET_AMBIENT_BALL: status = "自己玩毛线球"; hint = "滚过来，扑一下"; break;
        case PET_AMBIENT_WATCH: status = "看看四周"; hint = "有只蝴蝶飞过去啦"; break;
        default: if (!ui.pet.clock_valid) hint = "点顶部校时，开启规律作息"; break;
        }
    }
    if (strcmp(lv_label_get_text(ui.status), status)) lv_label_set_text(ui.status, status);
    lv_obj_set_style_text_color(ui.status, lv_color_hex(color), 0);
    if (ui.toast && (int32_t)(lv_tick_get() - ui.toast_until) < 0) hint = ui.toast;
    else ui.toast = NULL;
    // LVGL avoids invalidation for unchanged label text.
    if (strcmp(lv_label_get_text(ui.hint), hint)) lv_label_set_text(ui.hint, hint);
    const char *sleep = ui.pet.state == PET_SLEEPING ? "叫醒" : "睡觉";
    if (strcmp(lv_label_get_text(ui.sleep_label), sleep)) lv_label_set_text(ui.sleep_label, sleep);
    if (ui.shown != ui.pet.state) {
        ui.shown = ui.pet.state;
        lv_obj_invalidate(ui.buttons[PET_TOGGLE_SLEEP]);
        printf("PET state=%s satiety=%u xp=%u\n", pet_state_name(ui.pet.state), p->satiety, p->growth_xp);
    }
}

static void request_save(uint32_t now, bool force)
{
    if (force || ui.queued_revision != ui.pet.revision || now - ui.last_save >= 30000) {
        pet_store_request(&ui.pet.progress);
        ui.queued_revision = ui.pet.revision;
        ui.last_save = now;
    }
}

static void draw_battery_icon(lv_event_t *e)
{
    brush_t b = event_brush(e);
    const pet_battery_t battery = ui.battery;
    const uint32_t color = pet_battery_low(battery) ? PINK : battery.charging ? MINT : MUTED;
    box(b, 0, 4, 29, 17, 4, color, 255);
    box(b, 2, 6, 25, 13, 2, BG, 255);
    box(b, 29, 9, 3, 7, 1, color, 255);
    if (battery.available && battery.present && battery.level_valid) {
        unsigned width = (23u * battery.percent + 50) / 100;
        if (battery.percent && !width) width = 1;
        if (width) box(b, 3, 7, width, 11, 1, pet_battery_low(battery) ? PINK : MINT, 255);
    } else if (battery.available && !battery.present) {
        line(b, 8, 9, 20, 17, 2, MUTED, 255);
        line(b, 8, 17, 20, 9, 2, MUTED, 255);
    } else {
        line(b, 10, 13, 18, 13, 2, MUTED, 255);
    }
    if (battery.charging) {
        tri(b, 17, 3, 10, 13, 16, 13, 0xf9d16b, 255);
        tri(b, 12, 21, 20, 11, 14, 11, 0xf9d16b, 255);
    }
}

static void draw_sound_icon(lv_event_t *e)
{
    brush_t b = event_brush(e);
    const bool on = ui.sound.available && ui.sound.enabled;
    const uint32_t color = on ? MINT : MUTED;
    box(b, 2, 10, 6, 8, 1, color, 240);
    tri(b, 7, 10, 13, 5, 13, 23, color, 240);
    if (on) {
        arc(b, 12, 14, 7, 312, 48, 2, color);
        arc(b, 12, 14, 11, 312, 48, 2, color);
    } else {
        line(b, 17, 10, 23, 18, 2, PINK, 240);
        line(b, 23, 10, 17, 18, 2, PINK, 240);
    }
}

static void sync_sound(void)
{
    const pet_audio_status_t state = pet_audio_status();
    if (state.revision != ui.sound.revision) {
        ui.toast = !state.available ? "音效暂不可用" : state.enabled ? "声音已开启" : "声音已关闭";
        ui.toast_until = lv_tick_get() + 2000;
    } else if (ui.sound.available && !state.available) {
        ui.toast = "音效暂不可用";
        ui.toast_until = lv_tick_get() + 2000;
    }
    if (state.enabled != ui.sound.enabled || state.available != ui.sound.available)
        lv_obj_invalidate(ui.sound_icon);
    ui.sound = state;
}

static void layout_header(int caption_width)
{
    // Use measured text, not cached coordinates before LVGL's next layout pass.
    const int clock_x = 40 + caption_width + 12;
    lv_obj_align(ui.clock_label, LV_ALIGN_TOP_LEFT, clock_x, 0);
    lv_obj_set_pos(ui.sound_icon, clock_x + 72, 0);
    lv_obj_set_width(ui.battery_row, clock_x + 98);
    lv_obj_align(ui.battery_row, LV_ALIGN_TOP_MID, 0, 28);
}

static void update_clock(void)
{
    const pet_clock_t clock = pet_device_clock();
    pet_model_update_clock(&ui.pet, clock.valid, clock.utc_s, lv_tick_get());
}

static void sync_clock(void)
{
    char caption[16];
    if (ui.pet.clock_valid) {
        const unsigned minute = (unsigned)((ui.pet.utc_s + PET_LOCAL_OFFSET_S) % 86400) / 60;
        snprintf(caption, sizeof(caption), "%02u:%02u", minute / 60, minute % 60);
    } else snprintf(caption, sizeof(caption), "校时");
    if (strcmp(lv_label_get_text(ui.clock_label), caption)) lv_label_set_text(ui.clock_label, caption);
}

static void sync_battery(void)
{
    const pet_battery_t b = pet_device_battery();
    const uint32_t signature = b.available | (b.present << 1) | (b.charging << 2) |
                              (b.usb << 3) | (b.level_valid << 4) | ((uint32_t)b.percent << 8);
    if (signature == ui.shown_battery) return;
    ui.shown_battery = signature;
    ui.battery = b;
    char caption[48];
    if (!b.available) snprintf(caption, sizeof(caption), "电量 --");
    else if (!b.present) snprintf(caption, sizeof(caption), "未装电池");
    else if (!b.level_valid) snprintf(caption, sizeof(caption), b.charging ? "充电中 --" : "电量 --");
    else if (b.charging) snprintf(caption, sizeof(caption), "%u%% 充电中", b.percent);
    else if (pet_battery_low(b)) snprintf(caption, sizeof(caption), "%u%% 请充电", b.percent);
    else snprintf(caption, sizeof(caption), "%u%%", b.percent);
    lv_label_set_text(ui.battery_label, caption);
    const uint32_t color = pet_battery_low(b) ? PINK : b.charging ? MINT : MUTED;
    lv_obj_set_style_text_color(ui.battery_label, lv_color_hex(color), 0);
    lv_point_t size;
    lv_text_get_size(&size, caption, &pet_font_18, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_obj_set_width(ui.battery_label, size.x + 2);
    layout_header(size.x + 2);
    lv_obj_invalidate(ui.battery_icon);
}

static void toggle_screen(void)
{
    pet_touch_cancel(&ui.touch);
    if (ui.screen_off) {
        if (!pet_device_screen(true)) return;
        lv_obj_delete(ui.blanker);
        ui.blanker = NULL;
        ui.screen_off = false;
        pet_audio_set_screen_off(false);
        pet_model_set_screen_off(&ui.pet, false, lv_tick_get());
        pet_device_low_power(false);
        if (touch_input) {
            lv_indev_reset(touch_input, NULL);
            lv_indev_wait_release(touch_input);
            lv_indev_enable(touch_input, true);
        }
        sync_battery();
        sync_labels();
        lv_obj_invalidate(lv_screen_active());
        return;
    }
    request_save(lv_tick_get(), true);
    if (!pet_device_screen(false)) {
        ui.toast = "熄屏失败，请重试";
        ui.toast_until = lv_tick_get() + 2800;
        sync_labels();
        return;
    }
    if (touch_input) {
        lv_indev_reset(touch_input, NULL);
        lv_indev_enable(touch_input, false);
    }
    ui.toast = NULL;
    ui.blanker = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(ui.blanker);
    lv_obj_set_pos(ui.blanker, 0, 0);
    lv_obj_set_size(ui.blanker, 466, 466);
    lv_obj_set_style_bg_color(ui.blanker, lv_color_hex(0), 0);
    lv_obj_set_style_bg_opa(ui.blanker, LV_OPA_COVER, 0);
    lv_obj_remove_flag(ui.blanker, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui.blanker, LV_OBJ_FLAG_CLICKABLE);
    // No touch callback. Only a physical PWR event can remove this overlay.
    ui.screen_off = true;
    pet_audio_set_screen_off(true);
    pet_model_set_screen_off(&ui.pet, true, lv_tick_get());
    request_save(lv_tick_get(), true);
    pet_device_low_power(true);
}

static void clicked(lv_event_t *e)
{
    pet_touch_cancel(&ui.touch);
    const pet_action_t action = (pet_action_t)(uintptr_t)lv_event_get_user_data(e);
    const uint16_t old_xp = ui.pet.progress.growth_xp;
    const uint16_t old_stars = ui.pet.progress.stars;
    const pet_action_result_t result = pet_model_act(&ui.pet, action, lv_tick_get());
    if (result != PET_ACTION_IGNORED) {
        pet_audio_play(result == PET_ACTION_FULL ? PET_SOUND_FULL : action == PET_FEED ? PET_SOUND_FEED :
                       action == PET_TOGGLE_SLEEP ? (ui.pet.state == PET_SLEEPING ? PET_SOUND_SLEEP : PET_SOUND_WAKE) : PET_SOUND_HEAD);
        if (action == PET_FEED && result == PET_ACTION_OK) {
            if (ui.pet.progress.growth_xp > old_xp)
                snprintf(ui.feedback, sizeof(ui.feedback), "饱食 +25，成长 +%u", ui.pet.progress.growth_xp - old_xp);
            else snprintf(ui.feedback, sizeof(ui.feedback), "饱食 +25，谢谢你的照顾");
        }
        ui.toast = result == PET_ACTION_FULL ? "还不饿，先陪我玩吧" :
                   action == PET_FEED ? ui.feedback :
                   action == PET_STROKE && ui.pet.progress.growth_xp > old_xp ? "成长 +1，谢谢你的陪伴" : NULL;
        if (ui.pet.progress.stars > old_stars) {
            pet_audio_play(PET_SOUND_REWARD);
            snprintf(ui.feedback, sizeof(ui.feedback), "心愿达成！成长 +%u / 星星 +%u",
                     ui.pet.progress.growth_xp - old_xp, ui.pet.progress.stars - old_stars);
            ui.toast = ui.feedback;
        }
        ui.toast_until = lv_tick_get() + 2800;
        request_save(lv_tick_get(), false);
        sync_labels();
        lv_obj_invalidate(ui.stage);
    }
}

static void animate(lv_timer_t *timer)
{
    (void)timer;
    pet_model_tick(&ui.pet, lv_tick_get());
    update_clock();
    request_save(lv_tick_get(), false);
    if (pet_device_take_screen_key()) toggle_screen();
    if (ui.screen_off) return;
    sync_sound();
    if (ui.page == 0) apply_gesture(pet_touch_tick(&ui.touch, lv_tick_get()));
    pet_model_ambient_tick(&ui.pet, lv_tick_get(), ui.page == 0 && !ui.touch.pressed && !ui.touch.taps);
    sync_battery();
    sync_clock();
    sync_labels();
    sync_panel();
    if (!ui.page) lv_obj_invalidate(ui.stage);
}

static lv_obj_t *label(lv_obj_t *parent, const char *value, int y, int width,
                       const lv_font_t *font, uint32_t color)
{
    lv_obj_t *o = lv_label_create(parent);
    lv_obj_set_width(o, width);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(o, value);
    lv_obj_align(o, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}


static void close_panel(lv_event_t *e)
{
    (void)e;
    if (ui.pet.game.active) pet_model_play_cancel(&ui.pet, lv_tick_get());
    if (ui.overlay) lv_obj_delete(ui.overlay);
    ui.overlay = NULL;
    ui.page = 0;
    request_save(lv_tick_get(), false);
    sync_labels();
    lv_obj_invalidate(ui.stage);
}

static lv_obj_t *panel_button(const char *value, int x, int y, int w, lv_event_cb_t cb, uintptr_t id)
{
    lv_obj_t *button = lv_obj_create(ui.overlay);
    lv_obj_remove_style_all(button);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, 48);
    lv_obj_set_style_radius(button, 18, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x302b40), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x514360), LV_STATE_PRESSED);
    lv_obj_remove_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, (void *)id);
    label(button, value, 12, w, &pet_font_18, CREAM);
    return button;
}

static void new_panel(int page)
{
    pet_touch_cancel(&ui.touch);
    pet_model_ambient_cancel(&ui.pet, lv_tick_get());
    if (ui.overlay) lv_obj_delete(ui.overlay);
    ui.page = page;
    ui.overlay = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(ui.overlay);
    lv_obj_set_size(ui.overlay, 466, 466);
    lv_obj_set_style_bg_color(ui.overlay, lv_color_hex(BG), 0);
    lv_obj_set_style_bg_opa(ui.overlay, LV_OPA_COVER, 0);
    lv_obj_remove_flag(ui.overlay, LV_OBJ_FLAG_SCROLLABLE);
}

static void sync_clock_editor(void)
{
    const time_t local = (time_t)(ui.clock_edit + PET_LOCAL_OFFSET_S);
    struct tm date;
    gmtime_r(&local, &date);
    lv_label_set_text_fmt(ui.panel_title, "%04d-%02d-%02d", date.tm_year + 1900, date.tm_mon + 1, date.tm_mday);
    lv_label_set_text_fmt(ui.panel_info, "%02d : %02d", date.tm_hour, date.tm_min);
}

static void change_clock(lv_event_t *e)
{
    const unsigned id = (unsigned)(uintptr_t)lv_event_get_user_data(e);
    const int direction = id < 3 ? 1 : -1;
    const int unit = id % 3;
    int64_t next = ui.clock_edit;
    const int64_t local = next + PET_LOCAL_OFFSET_S;
    if (unit == 0) next += direction * 86400;
    else if (unit == 1) next += (((local / 3600 % 24 + direction + 24) % 24) - local / 3600 % 24) * 3600;
    else next += (((local / 60 % 60 + direction + 60) % 60) - local / 60 % 60) * 60;
    if (next >= INT64_C(1704067200) && next < INT64_C(4102444800)) ui.clock_edit = next;
    sync_clock_editor();
}

static void save_clock(lv_event_t *e)
{
    if (!pet_device_set_clock(ui.clock_edit)) {
        lv_label_set_text(ui.panel_note, "校时失败，请重试");
        return;
    }
    update_clock();
    sync_clock();
    close_panel(e);
    ui.toast = "北京时间已设置，作息开启";
    ui.toast_until = lv_tick_get() + 2800;
    sync_labels();
}

static void open_clock(lv_event_t *e)
{
    (void)e;
    const pet_clock_t clock = pet_device_clock();
    ui.clock_edit = clock.valid ? clock.utc_s : PET_BUILD_EPOCH;
    ui.clock_edit -= ui.clock_edit % 60;
    new_panel(4);
    panel_button("返回", 183, 32, 100, close_panel, 0);
    ui.panel_title = label(ui.overlay, "", 102, 300, &pet_font_18, CREAM);
    ui.panel_info = label(ui.overlay, "", 145, 280, &lv_font_montserrat_28, MINT);
    label(ui.overlay, "北京时间 UTC+8", 192, 280, &pet_font_18, MUTED);
    const char *names[] = {"日期 +", "小时 +", "分钟 +", "日期 -", "小时 -", "分钟 -"};
    for (unsigned i = 0; i < 6; ++i)
        panel_button(names[i], 83 + (i % 3) * 102, i < 3 ? 232 : 290, 96, change_clock, i);
    panel_button("保存时间", 155, 357, 156, save_clock, 0);
    ui.panel_note = label(ui.overlay, "设置后按作息生活", 416, 310, &pet_font_18, MUTED);
    sync_clock_editor();
}

static void select_accessory(lv_event_t *e)
{
    uint8_t accessory = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    const bool ok = pet_model_accessory(&ui.pet, accessory, lv_tick_get());
    if (ok) pet_audio_play(PET_SOUND_HIGH_FIVE);
    lv_label_set_text(ui.panel_note, ok ? "换装成功，回家看看我" : "成长或星星不足，再陪我玩玩");
    request_save(lv_tick_get(), false);
    sync_panel();
}

static void open_growth(lv_event_t *e)
{
    (void)e;
    new_panel(1);
    panel_button("返回小窝", 155, 30, 156, close_panel, 0);
    ui.panel_title = label(ui.overlay, "", 91, 330, &pet_font_18, CREAM);
    label(ui.overlay, "陪伴心愿 · 集齐自动领奖", 126, 330, &pet_font_18, LILAC);
    ui.panel_info = label(ui.overlay, "", 165, 350, &pet_font_18, MINT);
    lv_obj_set_style_text_line_space(ui.panel_info, 8, 0);
    label(ui.overlay, "集齐：成长 +6 / 星星 +5", 263, 330, &pet_font_18, LILAC);
    ui.panel_note = label(ui.overlay, "装扮永久解锁，可免费换装", 297, 330, &pet_font_18, MUTED);
    ui.shop[0] = panel_button("蝴蝶结 8星", 89, 335, 140, select_accessory, 1);
    ui.shop[1] = panel_button("皇冠 16星", 237, 335, 140, select_accessory, 2);
    panel_button("摘下装扮", 159, 398, 148, select_accessory, 0);
    sync_panel();
}

static const lv_point_t targets[5] = {{145,195}, {320,195}, {233,260}, {150,325}, {315,325}};

static void draw_game(lv_event_t *e)
{
    brush_t b = event_brush(e);
    for (unsigned i = 0; i < 5; ++i) {
        const int x = targets[i].x, y = targets[i].y - 155;
        const bool lit = i == ui.pet.game.target;
        dot(b, x, y, lit ? 34 : 29, lit ? 0x51404f : 0x242334, 255);
        star(b, x, y, lit ? 21 : 10, lit ? 0xf9d16b : 0x51465c, 255);
        if (lit) arc(b, x, y, 38, 0, 359, 2, PINK);
    }
}

static void hit_target(lv_event_t *e)
{
    lv_indev_t *indev = lv_event_get_indev(e);
    if (!indev || !ui.pet.game.active) return;
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    uint8_t target = 255;
    for (unsigned i = 0; i < 5; ++i) {
        int dx = point.x - targets[i].x, dy = point.y - targets[i].y;
        if (dx * dx + dy * dy <= 40 * 40) { target = i; break; }
    }
    if (pet_model_play_hit(&ui.pet, target, lv_tick_get())) pet_audio_play(PET_SOUND_HIT);
    sync_panel();
}

static void start_play(lv_event_t *e)
{
    (void)e;
    pet_touch_cancel(&ui.touch);
    if (!pet_model_play_start(&ui.pet, lv_tick_get())) {
        if (ui.pet.state == PET_EATING) ui.toast = "等我吃完，一起追星星";
        else if (ui.pet.progress.satiety < 10) ui.toast = "肚子空空，先喂我吃饭吧";
        else {
            snprintf(ui.feedback, sizeof(ui.feedback), "休息 %lu 秒，再来追星",
                     (unsigned long)((ui.pet.progress.play_wait_ms + 999) / 1000));
            ui.toast = ui.feedback;
        }
        ui.toast_until = lv_tick_get() + 2800;
        sync_labels();
        return;
    }
    ui.before_xp = ui.pet.progress.growth_xp;
    pet_audio_play(PET_SOUND_PLAY);
    ui.before_stars = ui.pet.progress.stars;
    new_panel(2);
    ui.panel_title = label(ui.overlay, "", 49, 270, &pet_font_18, CREAM);
    label(ui.overlay, "点亮星星 · 点错中断连击", 87, 330, &pet_font_18, MUTED);
    ui.panel_info = label(ui.overlay, "", 120, 330, &pet_font_18, MINT);
    ui.arena = lv_obj_create(ui.overlay);
    lv_obj_remove_style_all(ui.arena);
    lv_obj_set_pos(ui.arena, 0, 155);
    lv_obj_set_size(ui.arena, 466, 215);
    lv_obj_remove_flag(ui.arena, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ui.arena, draw_game, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(ui.arena, hit_target, LV_EVENT_PRESSED, NULL);
    panel_button("结束陪玩", 157, 388, 152, close_panel, 0);
    request_save(lv_tick_get(), false);
    sync_panel();
}

static void sync_panel(void)
{
    const pet_progress_t *p = &ui.pet.progress;
    if (ui.page == 1) {
        lv_label_set_text(lv_obj_get_child(ui.shop[0], 0), p->accessory == 1 ? "已戴蝴蝶结" :
                          p->unlocked & 1 ? "换蝴蝶结" : p->growth_xp < 20 ? "少年 · 8星" : "蝴蝶结 8星");
        lv_label_set_text(lv_obj_get_child(ui.shop[1], 0), p->accessory == 2 ? "已戴皇冠" :
                          p->unlocked & 2 ? "换皇冠" : p->growth_xp < 60 ? "成年 · 16星" : "皇冠 16星");
        lv_label_set_text_fmt(ui.panel_title, "羁绊 Lv.%u  /  星星 %u", pet_model_level(&ui.pet), p->stars);
        lv_label_set_text_fmt(ui.panel_info, "%s 摸摸我 · 每5分钟记一次\n%s 吃一餐 · 饿了才能喂\n%s 追星星 · 命中至少5次",
                             p->quest_flags & 1 ? "[OK]" : "[  ]",
                             p->quest_flags & 2 ? "[OK]" : "[  ]",
                             p->quest_flags & 4 ? "[OK]" : "[  ]");
    } else if (ui.page == 2 && ui.pet.game.active) {
        unsigned left = PET_PLAY_MS - (uint32_t)(lv_tick_get() - ui.pet.game.started);
        lv_label_set_text_fmt(ui.panel_title, "追星星   %u 秒", (left + 999) / 1000);
        lv_label_set_text_fmt(ui.panel_info, "命中 %u  /  连击 %u", ui.pet.game.hits, ui.pet.game.combo);
        lv_obj_invalidate(ui.arena);
    } else if (ui.page == 2) {
        pet_audio_play(ui.pet.game.hits >= 5 ? PET_SOUND_REWARD : PET_SOUND_FULL);
        new_panel(3);
        label(ui.overlay, "陪玩完成", 70, 280, &pet_font_18, CREAM);
        lv_obj_t *score = label(ui.overlay, "", 125, 330, &pet_font_18, MINT);
        lv_label_set_text_fmt(score, "命中 %u 次 · 最佳连击 %u", ui.pet.game.hits, ui.pet.game.best_combo);
        lv_obj_t *reward = label(ui.overlay, "", 184, 330, &pet_font_18, LILAC);
        lv_label_set_text_fmt(reward, "成长 +%u  /  星星 +%u", ui.pet.game.reward_xp, ui.pet.game.reward_stars);
        label(ui.overlay, ui.pet.game.hits >= 10 ? "好默契！下次也一起玩" :
                          ui.pet.game.hits >= 5 ? "接住啦！10次命中奖励更多" : "再试试，命中5次就有奖励", 240, 340, &pet_font_18, MUTED);
        label(ui.overlay, "陪玩后休息60秒", 283, 280, &pet_font_18, MUTED);
        panel_button("回家摸摸", 155, 345, 156, close_panel, 0);
        request_save(lv_tick_get(), false);
    }
}

static void cancel_pet_touch(lv_event_t *e)
{
    (void)e;
    pet_touch_cancel(&ui.touch);
    pet_model_ambient_cancel(&ui.pet, lv_tick_get());
}

static void apply_gesture(pet_gesture_t gesture)
{
    if (gesture == PET_TOUCH_NONE || ui.screen_off || ui.page != 0) return;
    if (gesture == PET_TOUCH_PLAY) { start_play(NULL); return; }
    pet_emote_t emote = PET_EMOTE_HEAD;
    if (gesture == PET_TOUCH_TICKLE) emote = PET_EMOTE_TICKLE;
    else if (gesture == PET_TOUCH_HUG) emote = PET_EMOTE_HUG;
    else if (gesture == PET_TOUCH_TEASE_LEFT) emote = PET_EMOTE_TEASE_LEFT;
    else if (gesture == PET_TOUCH_TEASE_RIGHT) emote = PET_EMOTE_TEASE_RIGHT;
    else if (gesture == PET_TOUCH_HIGH_FIVE) emote = PET_EMOTE_HIGH_FIVE;
    const uint16_t old_xp = ui.pet.progress.growth_xp, old_stars = ui.pet.progress.stars;
    if (pet_model_interact(&ui.pet, emote, lv_tick_get()) == PET_ACTION_IGNORED) {
        ui.toast = "等我吃完，再陪你玩";
        ui.toast_until = lv_tick_get() + 1800;
        return;
    }
    const pet_sound_t cue = emote == PET_EMOTE_HEAD ? PET_SOUND_HEAD : emote == PET_EMOTE_TICKLE ? PET_SOUND_TICKLE :
                            emote == PET_EMOTE_HUG ? PET_SOUND_HUG : emote == PET_EMOTE_HIGH_FIVE ? PET_SOUND_HIGH_FIVE : PET_SOUND_TEASE;
    pet_audio_play(ui.pet.progress.stars > old_stars ? PET_SOUND_REWARD : cue);
    const char *name = emote == PET_EMOTE_HEAD ? "摸摸头" : emote == PET_EMOTE_TICKLE ? "挠挠肚皮" :
                       emote == PET_EMOTE_HUG ? "抱抱你" : emote == PET_EMOTE_HIGH_FIVE ? "击掌成功" : "逗猫时间";
    const unsigned xp = ui.pet.progress.growth_xp - old_xp, stars = ui.pet.progress.stars - old_stars;
    if (stars) snprintf(ui.feedback, sizeof(ui.feedback), "心愿达成！成长 +%u / 星星 +%u", xp, stars);
    else if (xp) snprintf(ui.feedback, sizeof(ui.feedback), "%s · 成长 +%u", name, xp);
    else snprintf(ui.feedback, sizeof(ui.feedback), "%s · 喜欢和你玩", name);
    ui.toast = ui.feedback;
    ui.toast_until = lv_tick_get() + 2600;
    request_save(lv_tick_get(), false);
    sync_labels();
    lv_obj_invalidate(ui.stage);
}

static void touch_pet(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESS_LOST || code == LV_EVENT_INDEV_RESET) {
        pet_touch_cancel(&ui.touch);
        return;
    }
    if (ui.screen_off || ui.page != 0) { pet_touch_cancel(&ui.touch); return; }
    lv_indev_t *input = lv_event_get_indev(e);
    if (!input) return;
    lv_point_t point;
    lv_indev_get_point(input, &point);
    pet_gesture_t gesture = PET_TOUCH_NONE;
    if (code == LV_EVENT_PRESSED) {
        pet_model_ambient_cancel(&ui.pet, lv_tick_get());
        gesture = pet_touch_down(&ui.touch, point.x, point.y, lv_tick_get());
    }
    else if (code == LV_EVENT_PRESSING) pet_touch_move(&ui.touch, point.x, point.y);
    else if (code == LV_EVENT_RELEASED) gesture = pet_touch_up(&ui.touch, point.x, point.y, lv_tick_get());
    apply_gesture(gesture);
    lv_obj_invalidate(ui.stage);
}

static void make_button(lv_obj_t *screen, pet_action_t action, int x, const char *name)
{
    lv_obj_t *o = lv_obj_create(screen);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, 357);
    lv_obj_set_size(o, 76, 86);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(o, cancel_pet_touch, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(o, (unsigned)action == 3 ? start_play : clicked,
                        LV_EVENT_CLICKED, (void *)(uintptr_t)action);
    lv_obj_add_event_cb(o, draw_icon, LV_EVENT_DRAW_MAIN, (void *)(uintptr_t)action);
    // A separate, non-interactive disc leaves the entire 76 x 86 touch target.
    lv_obj_set_style_radius(o, 20, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(0x332d44), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(o, LV_OPA_50, LV_STATE_PRESSED);
    lv_obj_t *caption = label(o, name, 62, 76, &pet_font_18, MUTED);
    if (action == PET_TOGGLE_SLEEP) ui.sleep_label = caption;
    ui.buttons[action] = o;
}

static void draw_button_discs(lv_event_t *e)
{
    brush_t b = event_brush(e);
    for (int i = 0; i < 3; ++i) dot(b, 139 + 94 * i, 390, 29, 0x272334, 255);
}

void pet_ui_create(lv_obj_t *screen, const pet_progress_t *restored)
{
    pet_ui_destroy();
    lv_obj_clean(screen);
    pet_model_restore(&ui.pet, restored, lv_tick_get());
    ui.shown = (pet_state_t)-1;
    ui.shown_food = ui.shown_xp = ui.shown_store = -1;
    ui.toast = NULL;
    ui.page = 0; ui.overlay = NULL;
    ui.blanker = NULL; ui.screen_off = false;
    pet_audio_set_screen_off(false);
    ui.sound = pet_audio_status();
    pet_touch_cancel(&ui.touch);
    ui.shown_level = 0; ui.evolution_pending = ui.evolving = false;
    lv_obj_set_style_bg_color(screen, lv_color_hex(BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_t *back = lv_obj_create(screen);
    lv_obj_remove_style_all(back);
    lv_obj_set_size(back, 466, 466);
    lv_obj_remove_flag(back, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back, draw_button_discs, LV_EVENT_DRAW_MAIN, NULL);
    ui.save_label = label(screen, "", 441, 110, &pet_font_18, MUTED);
    ui.battery_row = lv_obj_create(screen);
    lv_obj_remove_style_all(ui.battery_row);
    lv_obj_set_size(ui.battery_row, 150, 24);
    lv_obj_remove_flag(ui.battery_row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    ui.battery_icon = lv_obj_create(ui.battery_row);
    lv_obj_remove_style_all(ui.battery_icon);
    lv_obj_set_size(ui.battery_icon, 32, 24);
    lv_obj_remove_flag(ui.battery_icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.battery_icon, draw_battery_icon, LV_EVENT_DRAW_MAIN, NULL);
    ui.battery_label = label(ui.battery_row, "", 0, 140, &pet_font_18, MUTED);
    lv_obj_align(ui.battery_label, LV_ALIGN_TOP_LEFT, 40, 0);
    ui.clock_label = label(ui.battery_row, "校时", 0, 64, &pet_font_18, LILAC);
    lv_obj_add_flag(ui.clock_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.clock_label, cancel_pet_touch, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui.clock_label, open_clock, LV_EVENT_CLICKED, NULL);
    ui.sound_icon = lv_obj_create(ui.battery_row);
    lv_obj_remove_style_all(ui.sound_icon);
    lv_obj_set_size(ui.sound_icon, 26, 26);
    lv_obj_remove_flag(ui.sound_icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.sound_icon, draw_sound_icon, LV_EVENT_DRAW_MAIN, NULL);
    ui.shown_battery = UINT32_MAX;
    ui.name = label(screen, "MOCHI", 48, 280, &lv_font_montserrat_28, CREAM);
    lv_obj_add_flag(ui.name, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_ver(ui.name, 5, 0);
    lv_obj_add_event_cb(ui.name, cancel_pet_touch, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui.name, open_growth, LV_EVENT_CLICKED, NULL);
    ui.status = label(screen, "", 86, 200, &pet_font_18, MINT);
    ui.food_label = label(screen, "", 111, 148, &pet_font_18, MINT);
    lv_obj_align(ui.food_label, LV_ALIGN_TOP_LEFT, 83, 111);
    ui.growth_label = label(screen, "", 111, 156, &pet_font_18, LILAC);
    lv_obj_align(ui.growth_label, LV_ALIGN_TOP_LEFT, 236, 111);
    lv_obj_t **fills[] = {&ui.food_fill, &ui.growth_fill};
    for (int i = 0; i < 2; ++i) {
        lv_obj_t *track = lv_obj_create(screen);
        lv_obj_remove_style_all(track);
        lv_obj_set_pos(track, 94 + i * 153, 139);
        lv_obj_set_size(track, 124, 4);
        lv_obj_set_style_bg_color(track, lv_color_hex(0x302b40), 0);
        lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(track, 2, 0);
        lv_obj_remove_flag(track, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        *fills[i] = lv_obj_create(track);
        lv_obj_remove_style_all(*fills[i]);
        lv_obj_set_size(*fills[i], 0, 4);
        lv_obj_set_style_bg_color(*fills[i], lv_color_hex(i ? LILAC : MINT), 0);
        lv_obj_set_style_bg_opa(*fills[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(*fills[i], 2, 0);
        lv_obj_remove_flag(*fills[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }
    ui.stage = lv_obj_create(screen);
    lv_obj_remove_style_all(ui.stage);
    lv_obj_set_pos(ui.stage, 55, 135);
    lv_obj_set_size(ui.stage, 356, 214);
    lv_obj_remove_flag(ui.stage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui.stage, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(ui.stage, draw_pet, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(ui.stage, touch_pet, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui.stage, touch_pet, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(ui.stage, touch_pet, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(ui.stage, touch_pet, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(ui.stage, touch_pet, LV_EVENT_INDEV_RESET, NULL);
    ui.hint = label(screen, "", 346, 334, &pet_font_18, MUTED);
    make_button(screen, (pet_action_t)3, 101, "追星");
    make_button(screen, PET_FEED, 195, "喂食");
    make_button(screen, PET_TOGGLE_SLEEP, 289, "睡觉");
    update_clock();
    request_save(lv_tick_get(), true);
    sync_battery();
    sync_clock();
    sync_labels();
    ui.timer = lv_timer_create(animate, 40, NULL);
}

void pet_ui_bind_input(lv_indev_t *input) { touch_input = input; }

bool pet_ui_sleep_begin(void)
{
    if (!ui.screen_off) return false;
    pet_model_tick(&ui.pet, lv_tick_get());
    update_clock();
    request_save(lv_tick_get(), true);
    return true;
}

void pet_ui_sleep_advance(uint64_t elapsed_ms, bool save)
{
    pet_model_advance_sleep(&ui.pet, elapsed_ms, lv_tick_get());
    update_clock();
    if (save) request_save(lv_tick_get(), true);
}

pet_state_t pet_ui_state(void) { return ui.pet.state; }
const pet_progress_t *pet_ui_progress(void) { return &ui.pet.progress; }

void pet_ui_destroy(void)
{
    pet_touch_cancel(&ui.touch);
    if (ui.screen_off) {
        pet_audio_set_screen_off(false);
        pet_device_low_power(false);
        pet_device_screen(true);
        ui.screen_off = false;
        if (touch_input) {
            lv_indev_reset(touch_input, NULL);
            lv_indev_wait_release(touch_input);
            lv_indev_enable(touch_input, true);
        }
    }
    if (ui.timer) {
        lv_timer_delete(ui.timer);
        ui.timer = NULL;
    }
    // Parent screen owns all objects. Destroy is called before that screen.
}
