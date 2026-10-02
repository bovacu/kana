#include "handwriting/score.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fails = 0;
static f64 fake_now = 0;
f64 rde_engine_get_time_now(void) { return fake_now; }
void rde_rendering_2d_draw_stroke(const rde_vec_2F* p, const f32* r, u32 n, rde_color c) { (void)p; (void)r; (void)n; (void)c; }
void rde_rendering_2d_draw_circle(const rde_vec_2F p, f32 r, u32 s, const rde_color c, rde_shader* sh) { (void)p; (void)r; (void)s; (void)c; (void)sh; }
static u32 rnd = 12345; static f32 jitter(f32 a) { rnd = rnd * 1103515245u + 12345u; return ((f32)((rnd >> 8) & 0xFFFF) / 65535.0f - 0.5f) * 2.0f * a; }

// Draws a character's strokes into _ink as a hand would: Y up, scaled 3x, offset,
// jittered; optional order swap of strokes a/b, a dropped stroke, a reversed stroke.
static f32 noise = 1.5f; static void draw(kana_ink* ink, const kana_kanji_db* db, u32 cp, int swap_a, int swap_b, int drop, int reverse) {
    kana_kanji_info k; kana_kanji_find(db, cp, &k);
    u32 order[64]; for(u32 i = 0; i < k.strokes; i++) order[i] = i;
    if(swap_a >= 0) { u32 t = order[swap_a]; order[swap_a] = order[swap_b]; order[swap_b] = t; }
    for(u32 o = 0; o < k.strokes; o++) {
        u32 s = order[o]; if((int)s == drop) continue;
        kana_kanji_stroke st; kana_kanji_stroke_at(db, &k, s, &st);
        rde_vec_2F pts[1024]; u32 n = kana_kanji_stroke_points(&st, 0.1f, pts, 1024);
        // dense like a pen: add midpoints
        rde_vec_2F dense[4096]; u32 m = 0;
        for(u32 i = 0; i < n; i++) { if(i > 0) for(int t = 1; t < 4; t++) { f32 f = t / 4.0f; dense[m++] = (rde_vec_2F){ pts[i-1].x + (pts[i].x - pts[i-1].x) * f, pts[i-1].y + (pts[i].y - pts[i-1].y) * f }; } dense[m++] = pts[i]; }
        for(u32 i = 0; i < m; i++) {
            u32 j = ((int)s == reverse) ? m - 1 - i : i;
            rde_vec_2F p = { 200.0f + dense[j].x * 3.0f + jitter(noise), 300.0f - dense[j].y * 3.0f + jitter(noise) };
            if(i == 0) kana_ink_begin(ink, p, true, false); else { ink->sample_time += 0.004; kana_ink_extend(ink, p); }
        }
        kana_ink_end(ink);
    }
}


// Adds one stray stroke to an ink.
static void extra_stroke(kana_ink* ink) { kana_ink_begin(ink, (rde_vec_2F){ 200, 290 }, true, false); for(int i = 1; i < 20; i++) kana_ink_extend(ink, (rde_vec_2F){ 200.0f + i * 3.0f, 290.0f }); kana_ink_end(ink); }

static kana_score run(const kana_kanji_db* db, u32 target, u32 drawn_cp, int sa, int sb, int drop, int rev, int extra, const char* label) {
    kana_ink ink; kana_ink_init(&ink); draw(&ink, db, drawn_cp, sa, sb, drop, rev); if(extra) extra_stroke(&ink);
    kana_kanji_info k; kana_kanji_find(db, target, &k);
    kana_score s = kana_score_drawing(db, &k, &ink);
    printf("  %-34s score %5.1f  shape %5.1f  drawn %u/%u  miss %u extra %u  order %u (%u,%u)  rev %u (%u)  | %s\n",
           label, s.score, s.shape, s.drawn, s.expected, s.missing, s.extra, s.misplaced, s.swap_a, s.swap_b, s.reversed, s.first_reversed, s.feedback);
    kana_ink_destroy(&ink); return s;
}

int main(int argc, char** argv) {
    (void)argc;
    kana_kanji_db db; kana_kanji_load(&db, argv[1]);
    const u32 cps[] = { 0x6728, 0x65E5, 0x8A9E, 0x5B66, 0x3042, 0x30AB, 0x6C34, 0x9053 };
    for(u32 i = 0; i < sizeof cps / sizeof cps[0]; i++) {
        char u[5]; kana_kanji_utf8(cps[i], u); char l[32]; snprintf(l, sizeof l, "%s clean", u);
        kana_score s = run(&db, cps[i], cps[i], -1, -1, -1, -1, 0, l);
        if(s.score < 85 || s.misplaced || s.reversed || s.missing || s.extra) fails++;
    }
    kana_score s;
    s = run(&db, 0x8A9E, 0x8A9E, 0, 1, -1, -1, 0, "語 strokes 1,2 swapped");   if(s.misplaced == 0 || s.score >= 95) fails++;
    s = run(&db, 0x6728, 0x6728, 1, 2, -1, -1, 0, "木 strokes 2,3 swapped");   if(s.misplaced == 0 || s.swap_a != 2 || s.swap_b != 3) fails++;
    s = run(&db, 0x6728, 0x6728, -1, -1, -1, 1, 0, "木 stroke 2 backwards");   if(s.reversed != 1 || s.first_reversed != 2) fails++;
    s = run(&db, 0x6728, 0x6728, -1, -1, 2, -1, 0, "木 stroke 3 missing");     if(s.missing != 1 || s.drawn != 3) fails++;
    s = run(&db, 0x6728, 0x6728, -1, -1, -1, -1, 1, "木 + an extra stroke");    if(s.extra != 1) fails++;
    s = run(&db, 0x6728, 0x672C, -1, -1, -1, -1, 0, "本 written, scored as 木"); if(s.score > 60) fails++;
    s = run(&db, 0x65E5, 0x76EE, -1, -1, -1, -1, 0, "目 written, scored as 日"); if(s.score > 60) fails++;
    noise = 6.0f;  s = run(&db, 0x6728, 0x6728, -1, -1, -1, -1, 0, "木 sloppy (jitter 6)");
    noise = 12.0f; kana_score t = run(&db, 0x6728, 0x6728, -1, -1, -1, -1, 0, "木 very sloppy (jitter 12)"); if(t.score >= s.score) fails++;
    { kana_ink ink; kana_ink_init(&ink); kana_kanji_info k; kana_kanji_find(&db, 0x6728, &k); kana_score e = kana_score_drawing(&db, &k, &ink); printf("  empty: %d '%s'\n", e.empty, e.feedback); if(!e.empty) fails++; kana_ink_destroy(&ink); }
    kana_kanji_unload(&db);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
