#include "study/handwriting/match.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static int fails = 0;
static f64 fake_now = 0;
f64 rde_engine_get_time_now(void) { return fake_now; }
void rde_rendering_2d_draw_stroke(const rde_vec_2F* p, const f32* r, u32 n, rde_color c) { (void)p; (void)r; (void)n; (void)c; }
void rde_rendering_2d_draw_circle(const rde_vec_2F p, f32 r, u32 s, const rde_color c, rde_shader* sh) { (void)p; (void)r; (void)s; (void)c; (void)sh; }

static u32 rnd = 12345; static f32 jitter(f32 a) { rnd = rnd * 1103515245u + 12345u; return ((f32)((rnd >> 8) & 0xFFFF) / 65535.0f - 0.5f) * 2.0f * a; }

// Draws a character's strokes into _ink as a hand would: Y up, scaled 3x, offset,
// jittered; optional order swap of strokes a/b, a dropped stroke, a reversed stroke.
static void draw(fude_ink* ink, const fude_kanji_db* db, u32 cp, int swap_a, int swap_b, int drop, int reverse) {
    fude_kanji_info k; fude_kanji_find(db, cp, &k);
    u32 order[64]; for(u32 i = 0; i < k.strokes; i++) order[i] = i;
    if(swap_a >= 0) { u32 t = order[swap_a]; order[swap_a] = order[swap_b]; order[swap_b] = t; }
    for(u32 o = 0; o < k.strokes; o++) {
        u32 s = order[o]; if((int)s == drop) continue;
        fude_kanji_stroke st; fude_kanji_stroke_at(db, &k, s, &st);
        rde_vec_2F pts[1024]; u32 n = fude_kanji_stroke_points(&st, 0.1f, pts, 1024);
        // dense like a pen: add midpoints
        rde_vec_2F dense[4096]; u32 m = 0;
        for(u32 i = 0; i < n; i++) { if(i > 0) for(int t = 1; t < 4; t++) { f32 f = t / 4.0f; dense[m++] = (rde_vec_2F){ pts[i-1].x + (pts[i].x - pts[i-1].x) * f, pts[i-1].y + (pts[i].y - pts[i-1].y) * f }; } dense[m++] = pts[i]; }
        for(u32 i = 0; i < m; i++) {
            u32 j = ((int)s == reverse) ? m - 1 - i : i;
            rde_vec_2F p = { 200.0f + dense[j].x * 3.0f + jitter(1.5f), 300.0f - dense[j].y * 3.0f + jitter(1.5f) };
            if(i == 0) fude_ink_begin(ink, p, true, false); else { ink->sample_time += 0.004; fude_ink_extend(ink, p); }
        }
        fude_ink_end(ink);
    }
}

static int rank(const fude_kanji_db* db, const fude_catalog* c, u32 cp, int sa, int sb, int drop, int rev, double* ms, char* top) {
    fude_ink ink; fude_ink_init(&ink); draw(&ink, db, cp, sa, sb, drop, rev);
    fude_match_result r[10];
    clock_t t0 = clock(); u32 n = fude_match_rank(db, c, FUDE_FILTER_ALL, &ink, r, 10); *ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
    int at = -1; top[0] = 0;
    for(u32 i = 0; i < n; i++) { fude_kanji_info k; fude_kanji_at(db, r[i].record, &k); char u[5]; fude_utf8_put(k.codepoint, u); if(i < 5) { strcat(top, u); strcat(top, " "); } if(k.codepoint == cp && at < 0) at = (int)i; }
    fude_ink_destroy(&ink);
    return at;
}

int main(int argc, char** argv) {
    (void)argc;
    fude_kanji_db db; fude_kanji_load(&db, argv[1]);
    fude_catalog c; fude_catalog_init(&c, &db);
    const u32 cps[] = { 0x6728, 0x65E5, 0x672C, 0x8A9E, 0x5B66, 0x66F8, 0x3042, 0x30AB, 0x6C34, 0x706B, 0x9053, 0x611B, 0x9B31, 0x4E00, 0x53E3 };
    int top1 = 0, top3 = 0, total = 0; double worst = 0;
    for(u32 i = 0; i < sizeof cps / sizeof cps[0]; i++) {
        double ms; char top[64]; int at = rank(&db, &c, cps[i], -1, -1, -1, -1, &ms, top);
        char u[5]; fude_utf8_put(cps[i], u);
        printf("  %s  rank %d  %.1f ms   top: %s\n", u, at + 1, ms, top);
        total++; top1 += at == 0; top3 += at >= 0 && at < 3; if(ms > worst) worst = ms;
    }
    printf("clean: top-1 %d/%d, top-3 %d/%d, worst %.1f ms\n", top1, total, top3, total, worst);
    if(top3 < total) fails++;

    double ms; char top[64]; int at;
    at = rank(&db, &c, 0x8A9E, 0, 1, -1, -1, &ms, top); printf("語 strokes 1<->2 swapped: rank %d  top: %s\n", at + 1, top); if(at < 0 || at > 2) fails++;
    at = rank(&db, &c, 0x5B66, -1, -1, 3, -1, &ms, top); printf("学 one stroke missing:     rank %d  top: %s\n", at + 1, top); if(at < 0 || at > 4) fails++;
    at = rank(&db, &c, 0x6728, -1, -1, -1, 0, &ms, top); printf("木 stroke 1 backwards:     rank %d  top: %s\n", at + 1, top); if(at < 0 || at > 2) fails++;

    fude_catalog_destroy(&c); fude_kanji_unload(&db);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
