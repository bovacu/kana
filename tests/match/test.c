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
            if(i == 0) fude_ink_begin(ink, p, true); else { ink->sample_time += 0.004; fude_ink_extend(ink, p); }
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

// --- The ranking as it was before Phase 4 (4.7, 2026-10-15): every record gone through in order, each rough cost
// worked out whole. The ranking now (records by stroke count, a candidate turned away by a floor under its rough cost
// once it could not be kept) must give exactly what it gave: the same candidates, the same costs to the bit, in the
// same order. Copied from match.c as it was; each reference is prepared once, as match.c's are, so the times compare.
#define REF_COARSE 4u
#define REF_STEP   ((FUDE_MATCH_POINTS - 1) / (REF_COARSE - 1))
#define REF_KEEP   64u
static const fude_match_stroke* ref_prepared(const fude_kanji_db* db, u32 r, const fude_kanji_info* k) {
    static fude_match_stroke* strokes; static u32* first; static u8* ready;
    if(first == NULL) {
        first = calloc(db->count + 1u, sizeof(u32)); ready = calloc(db->count + 1u, 1);
        for(u32 i = 0; i < db->count; i++) { fude_kanji_info in; first[i + 1u] = first[i] + (fude_kanji_at(db, i, &in) && in.strokes <= FUDE_MATCH_MAX_STROKES ? in.strokes : 0u); }
        strokes = malloc(sizeof(fude_match_stroke) * (first[db->count] + 1u));
    }
    if(ready[r] == 0) ready[r] = k->strokes > 0 && fude_match_reference(db, k, &strokes[first[r]], FUDE_MATCH_MAX_STROKES) == k->strokes ? 1 : 2;
    return ready[r] == 1 ? &strokes[first[r]] : NULL;
}
static f32 ref_rough(const rde_vec_2F user[][REF_COARSE], u32 n, const fude_match_stroke* ref, u32 m) {
    static f32 d[FUDE_MATCH_MAX_STROKES][FUDE_MATCH_MAX_STROKES], dp[FUDE_MATCH_MAX_STROKES + 1][FUDE_MATCH_MAX_STROKES + 1];
    f32 closest = 0.0f;
    for(u32 i = 0; i < n; i++) {
        f32 row = 1e30f;
        for(u32 j = 0; j < m; j++) {
            f32 fwd = 0.0f, rev = 0.0f;
            for(u32 k = 0; k < REF_COARSE; k++) {
                const rde_vec_2F p = user[i][k], q = ref[j].p[k * REF_STEP], r = ref[j].p[(REF_COARSE - 1u - k) * REF_STEP];
                fwd += rde_math_sqrt_f32((p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y));
                rev += rde_math_sqrt_f32((p.x - r.x) * (p.x - r.x) + (p.y - r.y) * (p.y - r.y));
            }
            d[i][j] = rde_math_min_f32(fwd / (f32)REF_COARSE, rev / (f32)REF_COARSE + FUDE_MATCH_REVERSED);
            row     = rde_math_min_f32(row, d[i][j]);
        }
        closest += row;
    }
    dp[0][0] = 0.0f;
    for(u32 i = 1; i <= n; i++) dp[i][0] = (f32)i * FUDE_MATCH_GAP;
    for(u32 j = 1; j <= m; j++) dp[0][j] = (f32)j * FUDE_MATCH_GAP;
    for(u32 i = 1; i <= n; i++) for(u32 j = 1; j <= m; j++)
        dp[i][j] = rde_math_min_f32(dp[i - 1][j - 1] + d[i - 1][j - 1], rde_math_min_f32(dp[i - 1][j], dp[i][j - 1]) + FUDE_MATCH_GAP);
    const f32 ordered = dp[n][m] / (f32)(n > m ? n : m);
    return n != m ? ordered : rde_math_min_f32(ordered, closest / (f32)n + FUDE_MATCH_UNORDERED);
}
static b8 ref_passes(const fude_catalog* c, u32 r, FUDE_FILTER_ f, const fude_kanji_info* k) {
    if(f != FUDE_MATCH_TEXT) return fude_catalog_passes(c, r, f);
    if(fude_catalog_passes(c, r, FUDE_FILTER_ALL)) return true;
    switch(k->codepoint) { case 0x3001: case 0x3002: case 0x3005: case 0x30FB: case 0x30FC: case 0xFF01: case 0xFF1F: case 0x0021: case 0x003F: return true;
                           default: return k->codepoint >= '0' && k->codepoint <= '9'; }
}
static void ref_keep(fude_match_result* list, u32* found, u32 max, u32 record, f32 cost) {
    if(*found < max || cost < list[*found - 1u].cost) {
        u32 at = *found < max ? *found : max - 1u;
        while(at > 0 && list[at - 1u].cost > cost) { if(at < max) list[at] = list[at - 1u]; at--; }
        list[at] = (fude_match_result){ .record = record, .cost = cost };
        if(*found < max) (*found)++;
    }
}
static u32 ref_rank(const fude_kanji_db* db, const fude_catalog* c, FUDE_FILTER_ f, const fude_match_stroke* user, u32 n, fude_match_result* out, u32 max) {
    static rde_vec_2F ru[FUDE_MATCH_MAX_STROKES][REF_COARSE]; static fude_match_result kept[REF_KEEP * 4u];
    if(max == 0 || n == 0 || n > FUDE_MATCH_MAX_STROKES) return 0;
    for(u32 i = 0; i < n; i++) for(u32 k = 0; k < REF_COARSE; k++) ru[i][k] = user[i].p[k * REF_STEP];
    const u32 keep = max * 3u > REF_KEEP ? (max * 3u < REF_KEEP * 4u ? max * 3u : REF_KEEP * 4u) : REF_KEEP;
    u32 kept_n = 0;
    for(u32 r = 0; r < db->count; r++) {
        fude_kanji_info k;
        if(!fude_kanji_at(db, r, &k) || k.strokes == 0 || k.strokes > FUDE_MATCH_MAX_STROKES) continue;
        if(k.strokes + FUDE_MATCH_STROKE_SLACK < n || k.strokes > n + FUDE_MATCH_STROKE_SLACK || !ref_passes(c, r, f, &k)) continue;
        const fude_match_stroke* ref = ref_prepared(db, r, &k);
        if(ref != NULL) ref_keep(kept, &kept_n, keep, r, ref_rough(ru, n, ref, k.strokes));
    }
    u32 found = 0;
    for(u32 i = 0; i < kept_n; i++) ref_keep(out, &found, max, kept[i].record, fude_match_cost_record(db, kept[i].record, user, n));
    return found;
}
// Ranks _user both ways; true when they give the same. The time each took is added to t_old and t_new (ms).
static double t_old, t_new;
static int same_rank(const fude_kanji_db* db, const fude_catalog* c, FUDE_FILTER_ f, const fude_match_stroke* user, u32 n, u32 max) {
    fude_match_result a[64], b[64];
    clock_t t0 = clock(); const u32 na = ref_rank(db, c, f, user, n, a, max);
    clock_t t1 = clock(); const u32 nb = fude_match_rank_strokes(db, c, f, user, n, b, max);
    clock_t t2 = clock();
    t_old += 1000.0 * (double)(t1 - t0) / CLOCKS_PER_SEC; t_new += 1000.0 * (double)(t2 - t1) / CLOCKS_PER_SEC;
    if(na != nb) return 0;
    for(u32 i = 0; i < na; i++) if(a[i].record != b[i].record || memcmp(&a[i].cost, &b[i].cost, sizeof(f32)) != 0) return 0;
    return 1;
}
// Writes a character's strokes into _ink as a hand would, at (ox, oy), _k times KanjiVG's size: Y up, jittered.
static void draw_at(fude_ink* ink, const fude_kanji_db* db, u32 cp, f32 ox, f32 oy, f32 k) {
    fude_kanji_info info; fude_kanji_find(db, cp, &info);
    for(u32 s = 0; s < info.strokes; s++) {
        fude_kanji_stroke st; fude_kanji_stroke_at(db, &info, s, &st);
        rde_vec_2F pts[1024]; const u32 n = fude_kanji_stroke_points(&st, 0.1f, pts, 1024);
        for(u32 i = 0; i < n; i++) {
            const rde_vec_2F p = { ox + pts[i].x * k + jitter(0.8f), oy - pts[i].y * k + jitter(0.8f) };
            if(i == 0) fude_ink_begin(ink, p, true); else { ink->sample_time += 0.004; fude_ink_extend(ink, p); }
        }
        fude_ink_end(ink);
    }
}
// A sentence, its characters side by side, and every run of up to 16 strokes in it ranked both ways as reading free
// writing ranks a run (segment.c: its strokes resampled, Y down, fitted together; FUDE_MATCH_TEXT, 16 kept). Returns
// how many runs differ; prints the time both ways.
static int sentence(const fude_kanji_db* db, const fude_catalog* c, const char* name, const u32* cps, u32 count) {
    fude_ink ink; fude_ink_init(&ink);
    for(u32 i = 0; i < count; i++) draw_at(&ink, db, cps[i], (f32)i * 330.0f, 0.0f, 3.0f);
    static fude_match_stroke raw[512], run[FUDE_MATCH_MAX_STROKES];
    u32 total = 0;
    for(u32 s = 0; s < fude_ink_stroke_count(&ink) && total < 512u; s++) {
        const fude_ink_stroke* st = fude_ink_stroke_at(&ink, s); const fude_ink_point* p = fude_ink_stroke_points(&ink, st);
        rde_vec_2F pts[1024]; const u32 n = st->point_count < 1024u ? st->point_count : 1024u;
        for(u32 i = 0; i < n; i++) pts[i] = (rde_vec_2F){ p[i].position.x, -p[i].position.y };
        fude_match_resample_points(pts, n, &raw[total++]);
    }
    int bad = 0, runs = 0; t_old = t_new = 0.0;
    for(u32 s = 0; s < total; s++) for(u32 len = 1; len <= 16u && s + len <= total; len++) {
        memcpy(run, &raw[s], sizeof(fude_match_stroke) * len); fude_match_fit(run, len);
        bad += !same_rank(db, c, FUDE_MATCH_TEXT, run, len, 16u); runs++;
    }
    printf("  %s: %u strokes, %d runs ranked: before %.0f ms, now %.0f ms (%.1fx)%s\n", name, total, runs, t_old, t_new, t_new > 0.0 ? t_old / t_new : 0.0, bad ? "  DIFFERENT" : "");
    fude_ink_destroy(&ink);
    return bad;
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

    // The ranking against the one before (above): every character of the data drawn as a hand would, ranked as
    // Browse's draw-search ranks (every character, 10 kept); every sixth as reading free writing ranks a run
    // (FUDE_MATCH_TEXT, 16 kept), every eleventh with a stroke left out, every thirteenth with two strokes swapped and
    // one backwards (30 kept: more than FUDE_MATCH_KEEP go to the exact cost). Then sentences, every run of strokes in
    // them as the reading ranks it. (About four minutes with the sanitizers, most of it the old ranking's.)
    {
        fude_match_stroke user[FUDE_MATCH_MAX_STROKES];
        int differ = 0, compared = 0; t_old = t_new = 0.0;
        for(u32 r = 0; r < db.count; r++) {
            fude_kanji_info k; fude_kanji_at(&db, r, &k);
            if(k.strokes == 0 || k.strokes > FUDE_MATCH_MAX_STROKES) continue;
            fude_ink ink; fude_ink_init(&ink); draw(&ink, &db, k.codepoint, -1, -1, -1, -1);
            u32 n = fude_match_drawing(&ink, user, FUDE_MATCH_MAX_STROKES); fude_ink_destroy(&ink);
            differ += !same_rank(&db, &c, FUDE_FILTER_ALL, user, n, 10u); compared++;
            if(r % 6u == 0) { differ += !same_rank(&db, &c, FUDE_MATCH_TEXT, user, n, 16u); compared++; }
            if(r % 11u == 0 && k.strokes > 1) {
                fude_ink_init(&ink); draw(&ink, &db, k.codepoint, -1, -1, (int)(k.strokes / 2u), -1);
                n = fude_match_drawing(&ink, user, FUDE_MATCH_MAX_STROKES); fude_ink_destroy(&ink);
                differ += !same_rank(&db, &c, FUDE_FILTER_ALL, user, n, 10u); compared++;
            }
            if(r % 13u == 0 && k.strokes > 2) {
                fude_ink_init(&ink); draw(&ink, &db, k.codepoint, 0, 1, -1, 2);
                n = fude_match_drawing(&ink, user, FUDE_MATCH_MAX_STROKES); fude_ink_destroy(&ink);
                differ += !same_rank(&db, &c, FUDE_FILTER_ALL, user, n, 30u); compared++;
            }
        }
        printf("every character drawn: %d rankings, %d different; before %.0f ms, now %.0f ms (%.1fx)\n", compared, differ, t_old, t_new, t_new > 0.0 ? t_old / t_new : 0.0);
        if(differ) fails++;
        const u32 nihongo[] = { 0x65E5, 0x672C, 0x8A9E }, kyouwa[] = { 0x4ECA, 0x65E5, 0x306F }, mizu[] = { 0x6C34, 0x3092, 0x98F2, 0x3080 };
        const u32 neko[] = { 0x306D, 0x3053 }, kanji[] = { 0x6F22, 0x5B57 }, kyou[] = { 0x304D, 0x3087, 0x3046 };
        int bad = sentence(&db, &c, "日本語", nihongo, 3) + sentence(&db, &c, "今日は", kyouwa, 3) + sentence(&db, &c, "水を飲む", mizu, 4);
        bad += sentence(&db, &c, "ねこ", neko, 2) + sentence(&db, &c, "漢字", kanji, 2) + sentence(&db, &c, "きょう", kyou, 3);
        if(bad) { printf("sentences: %d runs ranked differently\n", bad); fails++; }
    }

    fude_catalog_destroy(&c); fude_kanji_unload(&db);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
