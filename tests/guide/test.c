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
static void frame(kana_practice* p) { g_now += 0.016; kana_practice_update(p); kana_practice_render(p, NULL, NULL, 32.0f, TOP, BOTTOM); }

// Reference stroke _idx written in the guided square, as KanjiVG has it (or
// backwards, or moved by _dy of the square down).
static void write_ref(kana_practice* p, u32 idx, int backwards, f32 dy) {
    kana_kanji_stroke st; static rde_vec_2F pts[KANA_GLYPH_MAX_POINTS];
    if(!kana_kanji_stroke_at(p->db, &p->info, idx, &st)) { CHECK(0); return; }
    const u32 n = kana_glyph_stroke_points(&st, pts, NULL);
    const rde_vec_2F tl = p->square_tl[0]; const f32 s = p->square_size, k = s / KANA_KANJI_BOX;
    for(u32 i = 0; i < n; i++) {
        const rde_vec_2F q = pts[backwards ? n - 1 - i : i];
        const rde_vec_2F at = { tl.x + q.x * k, tl.y - q.y * k - dy * s };
        g_now += 0.008; p->inks[0].sample_time = g_now;
        if(i == 0) kana_practice_pen_down(p, at); else kana_practice_pen_moved(p, at);
    }
    kana_practice_pen_up(p);
}

int main(int argc, char** argv) {
    kana_kanji_db db; CHECK(kana_kanji_load(&db, argc > 1 ? argv[1] : "characters.kana"));
    u32 ki; CHECK(kana_kanji_find_index(&db, 0x6728, &ki));   // 木: 一 丨 丿 ㇏

    kana_practice p; kana_practice_init(&p, &db);
    kana_practice_open(&p, ki);
    kana_practice_set_guided(&p, true);
    frame(&p);
    CHECK(p.guided && p.guide.stage == KANA_GUIDE_TRACE && p.guide.next == 0 && kana_practice_guiding(&p));
    CHECK(p.square_size > 300.0f);   // one big square
    printf("guided square %.0f\n", p.square_size);

    // Step 1: the second stroke first: taken back, "that is stroke 2".
    write_ref(&p, 1, 0, 0.0f);
    printf("wrong order: \"%s\"\n", p.guide.message);
    CHECK(p.guide.next == 0 && kana_ink_alive_strokes(&p.inks[0]) == 0 && rde_arr_length(&p.strokes_in) == 0 && strstr(p.guide.message, "stroke 2") != NULL);
    frame(&p);
    // Backwards.
    write_ref(&p, 0, 1, 0.0f);
    printf("backwards: \"%s\"\n", p.guide.message);
    CHECK(p.guide.next == 0 && strstr(p.guide.message, "Backwards") != NULL && kana_ink_alive_strokes(&p.inks[0]) == 0);
    // Far from it.
    write_ref(&p, 0, 0, 0.25f);
    printf("off: \"%s\"\n", p.guide.message);
    CHECK(p.guide.next == 0 && strstr(p.guide.message, "Not quite") != NULL);
    // Scoring does nothing in steps 1 and 2.
    const u32 rev0 = kana_history_revision();
    kana_practice_score(&p); CHECK(!p.scored && kana_history_revision() == rev0);

    // Right, a little off (a real hand): kept. Undo takes it back.
    write_ref(&p, 0, 0, 0.03f);
    CHECK(p.guide.next == 1 && p.guide.message[0] == 0 && kana_ink_alive_strokes(&p.inks[0]) == 1);
    kana_practice_undo(&p);
    CHECK(p.guide.next == 0 && kana_ink_alive_strokes(&p.inks[0]) == 0);
    for(u32 i = 0; i < 4; i++) { write_ref(&p, i, 0, 0.0f); frame(&p); }
    CHECK(p.guide.next == 4 && p.guide.stage_done_at > 0.0 && kana_ink_alive_strokes(&p.inks[0]) == 4);

    // After a moment: step 2, a clean square.
    g_now += 1.3; frame(&p);
    CHECK(p.guide.stage == KANA_GUIDE_HINT && p.guide.next == 0 && kana_ink_alive_strokes(&p.inks[0]) == 0);
    write_ref(&p, 0, 0, 0.06f);   // a looser hand passes here
    CHECK(p.guide.next == 1);
    for(u32 i = 1; i < 4; i++) { write_ref(&p, i, 0, 0.0f); frame(&p); }
    CHECK(p.guide.stage_done_at > 0.0);

    // The pen, during the pause: step 3 at once, and that stroke is its first.
    write_ref(&p, 0, 0, 0.0f);
    CHECK(p.guide.stage == KANA_GUIDE_RECALL && !kana_practice_guiding(&p) && kana_ink_alive_strokes(&p.inks[0]) == 1);
    for(u32 i = 1; i < 4; i++) { write_ref(&p, i, 0, 0.0f); frame(&p); }
    printf("from memory: %.1f, \"%s\", status \"%s\"\n", p.scores[0].score, p.feedback, p.status);
    CHECK(p.scored && !p.scores[0].empty && p.scores[0].score >= 80.0f && kana_history_revision() == rev0 + 1 && strstr(p.status, "saved") != NULL);

    // Writing again: a new attempt, in reverse order this time — scored, not checked.
    write_ref(&p, 3, 0, 0.0f);
    CHECK(!p.scored && kana_ink_alive_strokes(&p.inks[0]) == 1);
    for(int i = 2; i >= 0; i--) { write_ref(&p, (u32)i, 0, 0.0f); frame(&p); }
    printf("reverse order from memory: %.1f \"%s\"\n", p.scores[0].score, p.scores[0].feedback);
    CHECK(p.scored && p.scores[0].misplaced > 0 && kana_history_revision() == rev0 + 2);

    // Clear in step 3: a blank square again. Guided off: the squares back.
    kana_practice_clear(&p); CHECK(kana_ink_alive_strokes(&p.inks[0]) == 0 && p.guide.stage == KANA_GUIDE_RECALL);
    kana_practice_set_guided(&p, false); frame(&p);
    CHECK(!p.guided && p.square_size < 300.0f);

    // A set: guided stays on for the next character, from step 1.
    u32 set[2]; set[0] = ki; CHECK(kana_kanji_find_index(&db, 0x3042, &set[1]));
    kana_practice_open_set(&p, set, 2); kana_practice_set_guided(&p, true); frame(&p);
    write_ref(&p, 0, 0, 0.0f);
    kana_practice_next(&p);   // steps 1-2 are not saved
    CHECK(p.guided && p.info.codepoint == 0x3042 && p.guide.stage == KANA_GUIDE_TRACE && p.guide.next == 0 && kana_history_revision() == rev0 + 2);
    for(u32 i = 0; i < p.info.strokes; i++) { write_ref(&p, i, 0, 0.0f); frame(&p); }
    CHECK(p.guide.next == p.info.strokes);

    kana_practice_destroy(&p); kana_kanji_unload(&db);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
