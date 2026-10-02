// wordexam.h: words written whole, a box a character (each box's reference
// strokes written with the pen), marked by the matcher; a given character (T of
// Tシャツ) needs no writing; a too-long word is not asked; Undo takes the last
// stroke whatever box it was in; the reviews (review.h, the words' keys) move;
// Retry the wrong ones. charnote.h's store too. argv[1]: the data.
#include "screens/wordexam.h"
#include "study/charnote.h"
#include "study/review.h"
#include "base/theme.h"
#include <stdio.h>
#include <string.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)
static f64 g_now = 1000.0;
void rde_rendering_2d_draw_stroke(const rde_vec_2F* p, const f32* r, u32 n, rde_color c) { (void)c; for(u32 i = 0; i < n; i++) { volatile f32 x = p[i].x + r[i]; (void)x; } }
void rde_rendering_2d_draw_circle(const rde_vec_2F p, f32 r, u32 s, const rde_color c, rde_shader* sh) { (void)p; (void)r; (void)s; (void)c; (void)sh; }
void rde_rendering_2d_draw_circle_border(const rde_vec_2F p, f32 r, f32 t, u32 s, const rde_color c, rde_shader* sh) { (void)p; (void)r; (void)t; (void)s; (void)c; (void)sh; }
void rde_rendering_2d_draw_rectangle(const rde_vec_2F a, const rde_vec_2F b, const rde_color c) { (void)a; (void)b; (void)c; }
void rde_rendering_2d_draw_rounded_rectangle(const rde_vec_2F a, const rde_vec_2F b, f32 r, u32 p, const rde_color c, rde_shader* s) { (void)a; (void)b; (void)r; (void)p; (void)c; (void)s; }
void rde_rendering_2d_draw_text_2(rde_font* f, const c8* t, rde_vec_3F p, rde_vec_2F s, f32 r, rde_color c) { (void)f; (void)t; (void)p; (void)s; (void)r; (void)c; }
void rde_rendering_begin_clipping_rect(const rde_window* w, rde_vec_2I p, rde_vec_2UI s) { (void)w; (void)p; (void)s; }
void rde_rendering_end_clipping_rect(void) {}
rde_vec_2I rde_window_get_size(const rde_window* w) { (void)w; return (rde_vec_2I){ 1133, 744 }; }
rde_vec_4I rde_window_get_safe_area_insets(const rde_window* w) { (void)w; return (rde_vec_4I){ 0, 24, 0, 20 }; }
f64 rde_engine_get_time_now(void) { return g_now; }
void rde_memcpy(any d, const any s, usize n) { memcpy(d, s, n); }

static kana_kanji_db db;
static const f32 TOP = 372.0f - 32.0f, BOTTOM = -372.0f + 120.0f;
static void frame(kana_wordexam* e) { g_now += 0.016; kana_wordexam_update(e, 0.016f); kana_wordexam_render(e, NULL, NULL, 32.0f, TOP, BOTTOM); }

// Writes code point _cp's reference strokes in box _b of the word on screen.
static void write_in(kana_wordexam* e, u32 b, u32 cp) {
    kana_kanji_info info; if(!kana_kanji_find(&db, cp, &info)) { printf("no %X\n", cp); return; }
    static rde_vec_2F pts[KANA_GLYPH_MAX_POINTS];
    const rde_vec_2F tl = { e->boxes_tl.x + (f32)b * (e->box + e->box_gap), e->boxes_tl.y };
    const f32 k = e->box / KANA_KANJI_BOX;
    for(u32 s = 0; s < info.strokes; s++) {
        kana_kanji_stroke st; if(!kana_kanji_stroke_at(&db, &info, s, &st)) continue;
        const u32 n = kana_glyph_stroke_points(&st, pts, NULL);
        for(u32 i = 0; i < n; i++) {
            const rde_vec_2F at = { tl.x + pts[i].x * k, tl.y - pts[i].y * k };
            g_now += 0.008;
            if(i == 0) kana_wordexam_pointer_down(e, at, true, g_now); else kana_wordexam_pointer_moved(e, at, g_now);
        }
        kana_wordexam_pointer_up(e, g_now);
    }
}

int main(int argc, char** argv) {
    CHECK(argc > 1 && kana_kanji_load(&db, argv[1]));
    kana_catalog cat; kana_catalog_init(&cat, &db);
    kana_vocab_open(NULL);
    kana_reviews_open(NULL);
    const u32 nihon  = kana_vocab_add("日本", "にほん", "Japan", 0);
    const u32 gakusei = kana_vocab_add("学生", "がくせい", "student", 0);
    const u32 taberu = kana_vocab_add("食べる", "たべる", "to eat", 0);
    const u32 shirt  = kana_vocab_add("Tシャツ", "ティーシャツ", "T-shirt", 0);
    const u32 longw  = kana_vocab_add("一二三四五六七八九", "", "too long", 0);
    const u32 ids[5] = { nihon, gakusei, taberu, shirt, longw };

    kana_wordexam e; kana_wordexam_init(&e, &db, &cat);
    kana_wordexam_open(&e, ids, 5, "Lesson 1");
    frame(&e);
    CHECK(e.open && e.stage == KANA_WORDEXAM_SETUP && kana_wordexam_planned(&e) == 4u);   // the long one is not asked
    e.length = 3u;   // all
    kana_wordexam_start(&e); frame(&e);
    CHECK(e.stage == KANA_WORDEXAM_WRITING && e.count == 4u && e.box > 100.0f);

    for(u32 i = 0; i < e.count; i++) {
        kana_wordexam_item* it = &e.items[e.current];
        // Undo: a stroke in box 0, one in box 1; Undo takes box 1's.
        if(i == 0) {
            write_in(&e, 0, it->chars[0]);
            const u32 before0 = kana_ink_alive_strokes(&it->ink[0]);
            if(it->count > 1u && it->records[1] != UINT32_MAX) {
                write_in(&e, 1, it->chars[1]);
                const u32 before1 = kana_ink_alive_strokes(&it->ink[1]);
                kana_wordexam_undo(&e);
                CHECK(before1 > 0u && kana_ink_alive_strokes(&it->ink[1]) == before1 - 1u);
                CHECK(kana_ink_alive_strokes(&it->ink[0]) == before0);
            }
            kana_wordexam_clear(&e);
            CHECK(kana_ink_alive_strokes(&it->ink[0]) == 0u);
        }
        // Each box as itself — 学生's second as 木 (wrong; 主, a look-alike, would
        // pass: its first three candidates have 生).
        for(u32 b = 0; b < it->count; b++) {
            if(it->records[b] == UINT32_MAX) continue;   // given: T
            const b8 wrong = it->word == gakusei && b == 1u;
            write_in(&e, b, wrong ? 0x6728u : it->chars[b]);
        }
        CHECK(kana_wordexam_at_last(&e) == (i + 1u == e.count));
        kana_wordexam_next(&e); frame(&e);
    }
    CHECK(e.stage == KANA_WORDEXAM_RESULTS);
    for(int f = 0; f < 600 && !e.kept; f++) frame(&e);
    CHECK(kana_wordexam_graded(&e) && e.kept);
    u32 right = 0;
    for(u32 i = 0; i < e.count; i++) {
        const kana_wordexam_item* it = &e.items[i];
        right += it->correct;
        if(it->word == gakusei) { CHECK(!it->correct && it->box_right[0] && !it->box_right[1]); }
        else                    { CHECK(it->correct && it->points > 50.0f); }
        if(it->word == shirt)   { CHECK(it->records[0] == UINT32_MAX && it->box_right[0]); }
        CHECK(kana_reviews_due_day(KANA_VOCAB_KEY(it->word)) == kana_reviews_today() + 1u);   // reviewed: tomorrow (right: a day; wrong: again tomorrow)
    }
    CHECK(right == 3u && kana_wordexam_wrong(&e) == 1u);
    printf("word exam: %u of %u right\n", right, e.count);

    // A tapped result: its word.
    e.tapped = 1; u32 w = 0; CHECK(kana_wordexam_take_tap(&e, &w) && w == e.items[1].word && !kana_wordexam_take_tap(&e, &w));

    // Retry: the wrong one alone, straight to writing; nothing written: wrong.
    kana_wordexam_retry_wrong(&e); frame(&e);
    CHECK(e.stage == KANA_WORDEXAM_WRITING && e.count == 1u && e.items[0].word == gakusei);
    kana_wordexam_next(&e); for(int f = 0; f < 100 && !e.kept; f++) frame(&e);
    CHECK(kana_wordexam_graded(&e) && !e.items[0].correct);

    // Reviews: straight to writing, in the order given.
    const u32 due[2] = { taberu, nihon };
    kana_wordexam_open_review(&e, due, 2); frame(&e);
    CHECK(e.review && e.stage == KANA_WORDEXAM_WRITING && e.count == 2u && e.items[0].word == taberu);
    kana_wordexam_close(&e);
    CHECK(!e.open);

    // Character notes.
    kana_charnotes_open(NULL);
    CHECK(kana_charnote_get(0x672A)[0] == 0 && kana_charnotes_count() == 0u);
    const u32 r0 = kana_charnotes_revision();
    kana_charnote_set(0x672A, "  short top line: not yet  \n");
    CHECK(strcmp(kana_charnote_get(0x672A), "short top line: not yet") == 0 && kana_charnotes_revision() != r0);
    kana_charnote_set(0x672B, "long top line: the end");
    kana_charnote_set(0x5F85, "wait");
    CHECK(kana_charnotes_count() == 3u && strcmp(kana_charnote_get(0x672B), "long top line: the end") == 0);
    kana_charnote_set(0x672A, "");
    CHECK(kana_charnotes_count() == 2u && kana_charnote_get(0x672A)[0] == 0);
    char longn[1000]; memset(longn, 0, sizeof longn); for(int i = 0; i < 200; i++) strcat(longn, "語");
    kana_charnote_set(0x8A9E, longn);
    CHECK(strlen(kana_charnote_get(0x8A9E)) <= KANA_CHARNOTE_TEXT - 1u && strlen(kana_charnote_get(0x8A9E)) % 3u == 0u);
    system("mkdir -p saves");
    kana_charnotes_close(); kana_charnotes_open("saves/charnotes.kana");
    kana_charnote_set(0x5F85, "wait (彳 on the left)");
    kana_charnotes_close(); kana_charnotes_open("saves/charnotes.kana");
    CHECK(strcmp(kana_charnote_get(0x5F85), "wait (彳 on the left)") == 0 && kana_charnotes_count() == 1u);
    kana_charnotes_close();

    kana_wordexam_destroy(&e); kana_catalog_destroy(&cat); kana_kanji_unload(&db);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
