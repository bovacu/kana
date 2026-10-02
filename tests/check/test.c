#include "screens/check.h"
#include "base/theme.h"
#include <stdio.h>
#include <string.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)
void rde_rendering_2d_draw_stroke(const rde_vec_2F* p, const f32* r, u32 n, rde_color c) { (void)c; for(u32 i = 0; i < n; i++) { volatile f32 x = p[i].x + r[i]; (void)x; } }
void rde_rendering_2d_draw_circle(const rde_vec_2F p, f32 r, u32 s, const rde_color c, rde_shader* sh) { (void)p; (void)r; (void)s; (void)c; (void)sh; }
void rde_rendering_2d_draw_rectangle(const rde_vec_2F a, const rde_vec_2F b, const rde_color c) { (void)a; (void)b; (void)c; }
void rde_rendering_2d_draw_text_2(rde_font* f, const c8* t, rde_vec_3F p, rde_vec_2F s, f32 r, rde_color c) { (void)f; (void)t; (void)p; (void)s; (void)r; (void)c; }
rde_vec_2I rde_window_get_size(const rde_window* w) { (void)w; return (rde_vec_2I){ 744, 1133 }; }
rde_vec_4I rde_window_get_safe_area_insets(const rde_window* w) { (void)w; return (rde_vec_4I){ 0, 24, 0, 20 }; }
f64 rde_engine_get_time_now(void) { return 1000.0; }
void rde_memcpy(any d, const any s, usize n) { memcpy(d, s, n); }

// Writes a character's reference strokes into an ink at canvas position (ox, oy), scale k, in _order.
static void write_char(const kana_kanji_db* db, kana_ink* ink, u32 cp, f32 ox, f32 oy, f32 k, const u32* order) {
    kana_kanji_info info; kana_kanji_find(db, cp, &info);
    for(u32 o = 0; o < info.strokes; o++) {
        kana_kanji_stroke st; kana_kanji_stroke_at(db, &info, order ? order[o] : o, &st);
        rde_vec_2F pts[1024]; const u32 n = kana_glyph_stroke_points(&st, pts, NULL);
        ink->sample_time = 0.0; ink->zoom = 1.0f;
        kana_ink_begin(ink, (rde_vec_2F){ ox + pts[0].x * k, oy - pts[0].y * k }, true, false);
        for(u32 i = 1; i < n; i++) { ink->sample_time += 0.01; kana_ink_extend(ink, (rde_vec_2F){ ox + pts[i].x * k, oy - pts[i].y * k }); }
        kana_ink_end(ink);
    }
}
static u32 cp_of(const kana_kanji_db* db, u32 record) { kana_kanji_info in; kana_kanji_at(db, record, &in); return in.codepoint; }

static const kana_check_char* ch(const kana_check* c, u32 i) { return &((const kana_check_char*)c->chars.memory)[i]; }
static u32 top_of(const kana_kanji_db* db, const kana_check* c, u32 i) { const kana_check_char* k = ch(c, i); return k->chosen >= 0 ? cp_of(db, k->candidates[k->chosen].record) : 0; }
// A frame: drawn ("Reading…" the first time), then updated — the reading happens.
static void frame(kana_check* c) { kana_check_render(c, NULL, NULL, 32.0f, 566.0f - 32.0f, -566.0f + 120.0f); kana_check_update(c); kana_check_render(c, NULL, NULL, 32.0f, 566.0f - 32.0f, -566.0f + 120.0f); }
static void write_text(const kana_kanji_db* db, kana_ink* ink, const u32* cps, u32 n, f32 k) {
    for(u32 i = 0; i < n; i++) write_char(db, ink, cps[i], (f32)i * 115.0f * k, 0.0f, k, NULL);
}

int main(int argc, char** argv) {
    (void)argc;
    kana_kanji_db db; CHECK(kana_kanji_load(&db, argv[1]));
    kana_catalog cat; kana_catalog_init(&cat, &db);
    kana_check c; kana_check_init(&c, &db, &cat);

    // A page: an unrelated stroke first, then 木 somewhere else, big.
    kana_ink ink; kana_ink_init(&ink);
    write_char(&db, &ink, 0x4E00, -900, 400, 2.0f, NULL);          // 一, not selected
    write_char(&db, &ink, 0x6728, 300, 200, 3.0f, NULL);           // 木: strokes 1..4
    u32 ids[4] = { 4, 2, 1, 3 };                                   // the lasso's order is any order
    CHECK(kana_check_open(&c, &ink, ids, 4));
    CHECK(c.open && c.pending && kana_check_count(&c) == 0);       // read on a later frame
    kana_check_update(&c);
    CHECK(c.pending);                                              // not before the screen has shown once
    frame(&c);
    CHECK(!c.pending && kana_check_count(&c) == 1);
    printf("best: U+%04X, then U+%04X, U+%04X; score %.0f '%s'\n", top_of(&db, &c, 0), cp_of(&db, ch(&c, 0)->candidates[1].record),
           cp_of(&db, ch(&c, 0)->candidates[2].record), ch(&c, 0)->score.score, ch(&c, 0)->score.feedback);
    CHECK(ch(&c, 0)->candidate_count > 3 && top_of(&db, &c, 0) == 0x6728 && ch(&c, 0)->chosen == 0 && ch(&c, 0)->score.score > 90.0f);
    u32 rec; CHECK(kana_check_chosen_record(&c, &rec) && cp_of(&db, rec) == 0x6728);

    // "I meant this other one": scored against it instead.
    kana_check_choose(&c, 1);
    printf("against U+%04X: %.0f '%s'\n", top_of(&db, &c, 0), ch(&c, 0)->score.score, ch(&c, 0)->score.feedback);
    CHECK(ch(&c, 0)->chosen == 1);

    // Drawn, and a tap on candidate 3 chooses it.
    kana_check_render(&c, NULL, NULL, 32.0f, 566.0f - 32.0f, -566.0f + 120.0f);
    const rde_vec_2F at = { c.cell_tl[2].x + 20.0f, c.cell_tl[2].y - 20.0f };
    kana_check_pointer_down(&c, at, 1.0); kana_check_pointer_moved(&c, (rde_vec_2F){ at.x + 3.0f, at.y }, 1.05); kana_check_pointer_up(&c, 1.1);
    CHECK(ch(&c, 0)->chosen == 2);
    kana_check_pointer_down(&c, at, 2.0); kana_check_pointer_moved(&c, (rde_vec_2F){ at.x + 80.0f, at.y }, 2.05); kana_check_pointer_up(&c, 2.1);   // a drag: nothing
    CHECK(ch(&c, 0)->chosen == 2);

    // 木 with strokes 2 and 3 swapped: still 木, and the order said.
    kana_ink ink2; kana_ink_init(&ink2);
    const u32 order[4] = { 0, 2, 1, 3 };
    write_char(&db, &ink2, 0x6728, 0, 0, 2.5f, order);
    u32 all[4] = { 0, 1, 2, 3 };
    CHECK(kana_check_open(&c, &ink2, all, 4)); frame(&c);
    printf("swapped: best U+%04X, %.0f '%s'\n", top_of(&db, &c, 0), ch(&c, 0)->score.score, ch(&c, 0)->score.feedback);
    CHECK(kana_check_count(&c) == 1 && top_of(&db, &c, 0) == 0x6728 && ch(&c, 0)->score.misplaced > 0);

    // A sentence: 日本語 across — three characters, each read and scored.
    kana_ink ink3; kana_ink_init(&ink3);
    const u32 nihongo[3] = { 0x65E5, 0x672C, 0x8A9E };
    write_text(&db, &ink3, nihongo, 3, 1.5f);
    u32 ids3[64]; const u32 n3 = kana_ink_stroke_count(&ink3); for(u32 i = 0; i < n3; i++) ids3[i] = i;
    CHECK(kana_check_open(&c, &ink3, ids3, n3)); frame(&c);
    printf("日本語: %u characters: U+%04X U+%04X U+%04X, scores %.0f %.0f %.0f\n", kana_check_count(&c), top_of(&db, &c, 0), top_of(&db, &c, 1),
           kana_check_count(&c) > 2 ? top_of(&db, &c, 2) : 0, ch(&c, 0)->score.score, ch(&c, 1)->score.score, kana_check_count(&c) > 2 ? ch(&c, 2)->score.score : 0.0f);
    CHECK(kana_check_count(&c) == 3 && top_of(&db, &c, 0) == 0x65E5 && top_of(&db, &c, 1) == 0x672C && top_of(&db, &c, 2) == 0x8A9E);
    CHECK(ch(&c, 0)->count == 4 && ch(&c, 1)->count == 5 && ch(&c, 2)->count == 14 && ch(&c, 2)->score.score > 90.0f);
    // A tap on the third character of the reading looks closer at it; its candidates are its own.
    const kana_check_hit* hits = (const kana_check_hit*)c.hits.memory; u32 tapped = 0;
    for(u32 i = 0; i < (u32)rde_arr_length(&c.hits); i++) if(hits[i].index == 2) { const rde_vec_2F p = { (hits[i].min.x + hits[i].max.x) * 0.5f, (hits[i].min.y + hits[i].max.y) * 0.5f };
        kana_check_pointer_down(&c, p, 3.0); kana_check_pointer_up(&c, 3.1); tapped++; }
    CHECK(tapped == 2 && c.selected == 2);                          // in the writing and in the reading
    CHECK(kana_check_chosen_record(&c, &rec) && cp_of(&db, rec) == 0x8A9E);
    u32 recs[8]; CHECK(kana_check_records(&c, recs, 8, true) == 3 && cp_of(&db, recs[1]) == 0x672C);

    // Read AS a text: Japanese, as typed; the strokes split to fit it.
    kana_check_read_as(&c, "日本語"); CHECK(c.pending && c.meant_count == 3); frame(&c);
    CHECK(!c.meant_failed && kana_check_count(&c) == 3 && ch(&c, 2)->count == 14 && top_of(&db, &c, 1) == 0x672C && ch(&c, 1)->candidate_count == 1);
    // As something else of three characters: read as it all the same, scored low.
    kana_check_read_as(&c, "目本話"); frame(&c);
    printf("as 目本話: U+%04X %.0f, U+%04X %.0f, U+%04X %.0f\n", top_of(&db, &c, 0), ch(&c, 0)->score.score, top_of(&db, &c, 1), ch(&c, 1)->score.score, top_of(&db, &c, 2), ch(&c, 2)->score.score);
    CHECK(kana_check_count(&c) == 3 && top_of(&db, &c, 0) == 0x76EE && top_of(&db, &c, 2) == 0x8A71 && ch(&c, 0)->score.score < ch(&c, 1)->score.score);
    // More characters than strokes: read freely instead, and said.
    kana_check_read_as(&c, "日本語日本語日本語日本語日本語日本語日本語日本語"); frame(&c);
    CHECK(c.meant_count == 24 && c.meant_failed && kana_check_count(&c) == 3 && top_of(&db, &c, 2) == 0x8A9E);
    // Empty: freely again.
    kana_check_read_as(&c, ""); frame(&c);
    CHECK(c.meant_count == 0 && !c.meant_failed && kana_check_count(&c) == 3);

    // Romaji: lower case to hiragana, UPPER to katakana, spaces skipped.
    kana_ink ink4; kana_ink_init(&ink4);
    const u32 kyou[3] = { 0x304D, 0x3087, 0x3046 };                 // きょう
    write_text(&db, &ink4, kyou, 3, 1.5f);
    u32 ids4[16]; const u32 n4 = kana_ink_stroke_count(&ink4); for(u32 i = 0; i < n4; i++) ids4[i] = i;
    CHECK(kana_check_open(&c, &ink4, ids4, n4)); frame(&c);
    printf("きょう: %u characters: U+%04X U+%04X U+%04X\n", kana_check_count(&c), top_of(&db, &c, 0), kana_check_count(&c) > 1 ? top_of(&db, &c, 1) : 0, kana_check_count(&c) > 2 ? top_of(&db, &c, 2) : 0);
    CHECK(kana_check_count(&c) == 3 && top_of(&db, &c, 0) == 0x304D && top_of(&db, &c, 1) == 0x3087 && top_of(&db, &c, 2) == 0x3046);
    kana_check_read_as(&c, "kyo u"); frame(&c);
    CHECK(c.meant_count == 3 && kana_check_count(&c) == 3 && top_of(&db, &c, 1) == 0x3087);
    kana_check_read_as(&c, "KYOU"); frame(&c);
    CHECK(c.meant_count == 3 && top_of(&db, &c, 0) == 0x30AD && top_of(&db, &c, 1) == 0x30E7 && top_of(&db, &c, 2) == 0x30A6);

    // Shared recognition (recognize.h): ML Kit's readings first — only those of the
    // reading's length, each character once, through the filter — then the matcher's.
    {
        kana_recognition r; memset(&r, 0, sizeof r); r.answered = true; r.line_count = 4;
        snprintf(r.lines[0], sizeof r.lines[0], "日本"); snprintf(r.lines[1], sizeof r.lines[1], "目本");
        snprintf(r.lines[2], sizeof r.lines[2], "日木"); snprintf(r.lines[3], sizeof r.lines[3], "日本語");   // another length: skipped
        u32 rec_hi, rec_ki, rec_ka; kana_kanji_find_index(&db, 0x3072, &rec_hi); kana_kanji_find_index(&db, 0x6728, &rec_ki); kana_kanji_find_index(&db, 0x304B, &rec_ka);
        const kana_match_result matched[3] = { { rec_ki, 5.0f }, { rec_hi, 6.0f }, { rec_ka, 7.0f } };
        kana_match_result out[8];
        u32 n = kana_recognize_candidates(&db, &r, 0, 2, NULL, KANA_FILTER_ALL, matched, 3, out, 8);
        CHECK(n == 5 && cp_of(&db, out[0].record) == 0x65E5 && cp_of(&db, out[1].record) == 0x76EE && out[0].cost == 0.0f && cp_of(&db, out[2].record) == 0x6728);
        n = kana_recognize_candidates(&db, &r, 1, 2, NULL, KANA_FILTER_ALL, matched, 3, out, 8);   // 本 once, then 木 (line 3 and the matcher's, once)
        CHECK(n == 4 && cp_of(&db, out[0].record) == 0x672C && cp_of(&db, out[1].record) == 0x6728 && cp_of(&db, out[2].record) == 0x3072);
        n = kana_recognize_candidates(&db, &r, 0, 2, &cat, KANA_FILTER_HIRAGANA, matched, 3, out, 8); // the filter: kana only
        CHECK(n == 2 && cp_of(&db, out[0].record) == 0x3072 && cp_of(&db, out[1].record) == 0x304B);
        u32 recs[8]; CHECK(kana_recognize_records(&db, "日 本　語", recs, 8) == 3);                 // spaces skipped
        CHECK(!kana_recognize_available() && !kana_recognize_start(&r, &ink) && !kana_recognize_poll(&r));   // no ML Kit here
    }

    // Nothing alive selected: not opened.
    kana_check_close(&c);
    kana_ink_erase_strokes(&ink2, all, 4);
    CHECK(!kana_check_open(&c, &ink2, all, 4) && !c.open);

    kana_ink_destroy(&ink); kana_ink_destroy(&ink2); kana_ink_destroy(&ink3); kana_ink_destroy(&ink4);
    kana_check_destroy(&c); kana_catalog_destroy(&cat); kana_match_release(); kana_kanji_unload(&db);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
