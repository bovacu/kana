#include "screens/practice.h"
#include "base/theme.h"
#include <stdio.h>
#include <string.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)
static f64 g_now = 1000.0;
void rde_rendering_2d_draw_stroke(const rde_vec_2F* p, const f32* r, u32 n, rde_color c) { (void)c; for(u32 i = 0; i < n; i++) { volatile f32 x = p[i].x + r[i]; (void)x; } }
void rde_rendering_2d_draw_circle(const rde_vec_2F p, f32 r, u32 s, const rde_color c, rde_shader* sh) { (void)p; (void)r; (void)s; (void)c; (void)sh; }
void rde_rendering_2d_draw_rectangle(const rde_vec_2F a, const rde_vec_2F b, const rde_color c) { (void)a; (void)b; (void)c; }
void rde_rendering_2d_draw_text_2(rde_font* f, const c8* t, rde_vec_3F p, rde_vec_2F s, f32 r, rde_color c) { (void)f; (void)t; (void)p; (void)s; (void)r; (void)c; }
rde_vec_2I rde_window_get_size(const rde_window* w) { (void)w; return (rde_vec_2I){ 744, 1133 }; }
rde_vec_4I rde_window_get_safe_area_insets(const rde_window* w) { (void)w; return (rde_vec_4I){ 0, 24, 0, 20 }; }
f64 rde_engine_get_time_now(void) { return g_now; }
void rde_memcpy(any d, const any s, usize n) { memcpy(d, s, n); }

static const f32 TOP = 566.5f - 24.0f - 8.0f, BOTTOM = -566.5f + 20.0f + 76.0f + 24.0f;
static void frame(kana_practice* p) { g_now += 0.016; kana_practice_render(p, NULL, NULL, 32.0f, TOP, BOTTOM); }
// A stroke across square _sq, left to right at mid height.
static void stroke_in(kana_practice* p, u32 sq) {
    const rde_vec_2F tl = p->square_tl[sq]; const f32 s = p->square_size;
    kana_practice_pen_down(p, (rde_vec_2F){ tl.x + s * 0.2f, tl.y - s * 0.5f });
    for(int i = 1; i <= 10; i++) { g_now += 0.01; p->inks[sq].sample_time = g_now; kana_practice_pen_moved(p, (rde_vec_2F){ tl.x + s * (0.2f + 0.06f * (f32)i), tl.y - s * 0.5f }); }
    kana_practice_pen_up(p);
}

int main(int argc, char** argv) {
    kana_kanji_db db; CHECK(kana_kanji_load(&db, argc > 1 ? argv[1] : "characters.kana"));
    u32 rec[3]; const u32 cps[3] = { 0x4E00, 0x3042, 0x4E8C };   // 一 (one stroke), あ, 二
    for(int i = 0; i < 3; i++) CHECK(kana_kanji_find_index(&db, cps[i], &rec[i]));

    kana_practice p; kana_practice_init(&p, &db);
    kana_practice_open(&p, rec[0]);
    CHECK(p.open && !kana_practice_in_set(&p));

    kana_practice_open_set(&p, rec, 3);
    CHECK(p.open && kana_practice_in_set(&p) && p.set_position == 0 && !kana_practice_at_last(&p) && !p.summary_open);
    frame(&p); CHECK(p.square_size > 0.0f);

    // 一: a good straight stroke in two squares; Next scores and saves it.
    const u32 rev0 = kana_history_revision();
    stroke_in(&p, 0); stroke_in(&p, 1);
    kana_practice_next(&p);
    const f32* res = (const f32*)p.set_results.memory;
    printf("一 scored %.1f; now at %u (U+%04X)\n", res[0], p.set_position, p.info.codepoint);
    CHECK(res[0] >= 0.0f && kana_history_revision() == rev0 + 1 && p.set_position == 1 && p.info.codepoint == 0x3042);

    // あ: nothing written; Next skips it (no score, no save).
    frame(&p); kana_practice_next(&p);
    res = (const f32*)p.set_results.memory;
    CHECK(res[1] < 0.0f && kana_history_revision() == rev0 + 1 && p.set_position == 2 && kana_practice_at_last(&p));

    // 二 (last): one stroke where two are wanted — weak; Score, then Finish.
    frame(&p); stroke_in(&p, 0);
    kana_practice_score(&p);
    const u32 rev1 = kana_history_revision();
    kana_practice_next(&p);                        // already scored and saved: no second save
    res = (const f32*)p.set_results.memory;
    printf("二 scored %.1f; summary %d; weak %u\n", res[2], p.summary_open, kana_practice_weak_count(&p));
    CHECK(p.summary_open && kana_history_revision() == rev1 && res[2] >= 0.0f && res[2] < KANA_THEME_GRADE_GOOD);
    frame(&p); CHECK(p.square_size == 0.0f);     // the summary: no squares
    kana_practice_pen_down(&p, (rde_vec_2F){ 0, 0 }); CHECK(p.writing < 0);
    kana_practice_next(&p); CHECK(p.summary_open);  // Next does nothing on the summary

    // Weakest again: only the ones under good, weakest first.
    const u32 weak = kana_practice_weak_count(&p);
    CHECK(weak >= 1);
    kana_practice_weakest_again(&p);
    CHECK(!p.summary_open && p.open && (u32)rde_arr_length(&p.set) == weak && p.set_position == 0);
    CHECK(((const u32*)p.set.memory)[weak - 1] == rec[2] || weak == 1);
    frame(&p);

    // A long list is cut to KANA_PRACTICE_SET_MAX.
    u32 many[150]; for(u32 i = 0; i < 150; i++) many[i] = rec[i % 3];
    kana_practice_open_set(&p, many, 150);
    CHECK((u32)rde_arr_length(&p.set) == KANA_PRACTICE_SET_MAX);

    kana_practice_close(&p); CHECK(!p.open && !p.summary_open);
    kana_practice_destroy(&p); kana_kanji_unload(&db);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
