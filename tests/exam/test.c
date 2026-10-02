#include "study/screens/exam.h"
#include "study/models/examlog.h"
#include "drawing/base/kfile.h"
#include "study/models/marks.h"
#include "study/models/vocab.h"
#include "drawing/base/theme.h"
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
rde_vec_2I rde_window_get_size(const rde_window* w) { (void)w; return (rde_vec_2I){ 744, 1133 }; }
rde_vec_4I rde_window_get_safe_area_insets(const rde_window* w) { (void)w; return (rde_vec_4I){ 0, 24, 0, 20 }; }
f64 rde_engine_get_time_now(void) { return g_now; }
void rde_memcpy(any d, const any s, usize n) { memcpy(d, s, n); }

static const f32 TOP = 566.5f - 24.0f - 8.0f, BOTTOM = -566.5f + 20.0f + 76.0f + 24.0f;
static void frame(fude_exam* e) { g_now += 0.016; fude_exam_update(e, 0.016f); fude_exam_render(e, NULL, NULL, 32.0f, TOP, BOTTOM); }

// Writes record _as (its reference strokes) in the exam's square, with the pen.
static void write_as(fude_exam* e, const fude_kanji_db* db, u32 as) {
    fude_kanji_info info; fude_kanji_at(db, as, &info);
    static rde_vec_2F pts[FUDE_GLYPH_MAX_POINTS];
    const rde_vec_2F tl = e->square_tl; const f32 k = e->square_size / FUDE_KANJI_BOX;
    for(u32 s = 0; s < info.strokes; s++) {
        fude_kanji_stroke st; if(!fude_kanji_stroke_at(db, &info, s, &st)) continue;
        const u32 n = fude_glyph_stroke_points(&st, pts, NULL);
        for(u32 i = 0; i < n; i++) {
            const rde_vec_2F at = { tl.x + pts[i].x * k, tl.y - pts[i].y * k };
            g_now += 0.008;
            if(i == 0) fude_exam_pointer_down(e, at, true, g_now); else fude_exam_pointer_moved(e, at, g_now);
        }
        fude_exam_pointer_up(e, g_now);
    }
}

// An exam of what the preview holds, each written as itself (or, for _wrong_at,
// as another character); returns how many were right.
static u32 take(fude_exam* e, const fude_kanji_db* db, i32 wrong_at, u32 wrong_as) {
    fude_exam_start(e); frame(e);
    CHECK(e->stage == FUDE_EXAM_WRITING && e->square_size > 200.0f);
    for(u32 i = 0; i < e->asked; i++) {
        write_as(e, db, (i32)i == wrong_at ? wrong_as : e->items[e->order[i]].record);
        CHECK(fude_exam_at_last(e) == (i + 1 == e->asked));
        fude_exam_next(e); frame(e);
    }
    CHECK(e->stage == FUDE_EXAM_RESULTS);
    for(int f = 0; f < 400 && !e->saved; f++) frame(e);
    CHECK(fude_exam_graded(e) && e->saved);
    u32 right = 0; for(u32 i = 0; i < e->asked; i++) right += e->items[e->order[i]].correct;
    return right;
}

int main(int argc, char** argv) {
    fude_kanji_db db; CHECK(fude_kanji_load(&db, argc > 1 ? argv[1] : "characters.kana"));
    fude_catalog cat; fude_catalog_init(&cat, &db);
    system("rm -rf saves && mkdir -p saves");

    // --- marks: set, list, NONE removes, a file round trip ---------------------------
    fude_marks_open("saves/marks.kana");
    CHECK(fude_marks_count(FUDE_MARK_STUDYING) == 0);
    const u32 r0 = fude_marks_revision();
    CHECK(fude_marks_set(0x6728, FUDE_MARK_STUDYING) && fude_marks_get(0x6728) == FUDE_MARK_STUDYING && fude_marks_revision() != r0);
    const u32 many[3] = { 0x65E5, 0x3042, 0x4EBA };
    CHECK(fude_marks_set_many(many, 3, FUDE_MARK_KNOWN));
    CHECK(fude_marks_count(FUDE_MARK_KNOWN) == 3 && fude_marks_count(FUDE_MARK_STUDYING) == 1);
    u32 list[8]; CHECK(fude_marks_list(FUDE_MARK_KNOWN, list, 8) == 3 && list[0] == 0x3042 && list[2] == 0x65E5);   // code point order
    fude_marks_set(0x4EBA, FUDE_MARK_NONE);
    CHECK(fude_marks_get(0x4EBA) == FUDE_MARK_NONE && fude_marks_count(FUDE_MARK_KNOWN) == 2);
    // Every change kept, in order: 木 Studying, three Known, 人 unmarked (setting a mark it has is no change).
    CHECK(fude_marks_set(0x6728, FUDE_MARK_STUDYING));
    CHECK(fude_marks_history_count() == 5);
    { const fude_marks_change* c = fude_marks_history();
      CHECK(c[0].codepoint == 0x6728 && c[0].mark == FUDE_MARK_STUDYING && c[1].mark == FUDE_MARK_KNOWN && c[4].codepoint == 0x4EBA && c[4].mark == FUDE_MARK_NONE);
      CHECK(c[0].time <= c[4].time && c[0].time > 1700000000ull); }
    fude_marks_close(); CHECK(fude_marks_get(0x6728) == FUDE_MARK_NONE && fude_marks_history_count() == 0);
    fude_marks_open("saves/marks.kana");
    CHECK(fude_marks_get(0x6728) == FUDE_MARK_STUDYING && fude_marks_get(0x65E5) == FUDE_MARK_KNOWN && fude_marks_count(FUDE_MARK_KNOWN) == 2);
    CHECK(fude_marks_history_count() == 5 && fude_marks_history()[4].codepoint == 0x4EBA && fude_marks_history()[4].mark == FUDE_MARK_NONE);
    // A file from before changes were kept: its marks start the history, oldest first.
    { fude_bytes b = fude_bytes_new(128);
      fude_put_header(&b, 1u, FUDE_TAG('M', 'A', 'R', 'K'));
      const u32 ch = fude_chunk_begin(&b, FUDE_TAG('M', 'R', 'K', 'S'));
      fude_put_u32(&b, 2u); fude_put_u32(&b, 16u);
      fude_put_u32(&b, 0x3042); fude_put_u8(&b, FUDE_MARK_KNOWN);    fude_put_u8(&b, 0); fude_put_u8(&b, 0); fude_put_u8(&b, 0); fude_put_u32(&b, 2000u); fude_put_u32(&b, 0u);
      fude_put_u32(&b, 0x6728); fude_put_u8(&b, FUDE_MARK_STUDYING); fude_put_u8(&b, 0); fude_put_u8(&b, 0); fude_put_u8(&b, 0); fude_put_u32(&b, 1000u); fude_put_u32(&b, 0u);
      fude_chunk_end(&b, ch);
      CHECK(fude_bytes_write_and_free(&b, "saves/old_marks.kana", NULL));
      fude_marks_close(); fude_marks_open("saves/old_marks.kana");
      CHECK(fude_marks_count(FUDE_MARK_KNOWN) == 1 && fude_marks_history_count() == 2);
      CHECK(fude_marks_history()[0].codepoint == 0x6728 && fude_marks_history()[0].time == 1000u && fude_marks_history()[1].codepoint == 0x3042);
      fude_marks_close(); fude_marks_open("saves/marks.kana"); }
    // The catalog's filters follow them.
    u32 ki; CHECK(fude_kanji_find_index(&db, 0x6728, &ki) && fude_catalog_passes(&cat, ki, FUDE_FILTER_STUDYING) && !fude_catalog_passes(&cat, ki, FUDE_FILTER_KNOWN));
    fude_marks_set_many(many, 3, FUDE_MARK_NONE); fude_marks_set(0x6728, FUDE_MARK_NONE);

    // --- the learner's vocabulary (vocab.h, vocabtest has the rest): a kanji's words --
    fude_vocab_open("saves/words.kana");
    CHECK(fude_vocab_kanji_count(0x76EE) == 0 && fude_vocab_count() == 0);
    CHECK(fude_vocab_add("目薬", "めぐすり", "eye drops", 0x76EE) != 0 && fude_vocab_add("目覚まし", "めざまし", "alarm clock", 0x76EE) != 0);
    CHECK(fude_vocab_kanji_count(0x76EE) == 2 && fude_vocab_count() == 2 && fude_vocab_find("目薬", "めぐすり") != 0);
    fude_kanji_word uw; CHECK(fude_vocab_kanji_at(0x76EE, 1, &uw) && strcmp(uw.written, "目覚まし") == 0 && strcmp(uw.meaning, "alarm clock") == 0);
    fude_vocab_close(); fude_vocab_open("saves/words.kana");
    CHECK(fude_vocab_kanji_count(0x76EE) == 2);
    fude_vocab_remove(fude_vocab_find("目薬", "めぐすり"));
    CHECK(fude_vocab_kanji_count(0x76EE) == 1 && fude_vocab_find("目薬", "めぐすり") == 0);
    fude_vocab_remove(fude_vocab_find("目覚まし", "めざまし"));

    // --- the exam log ---------------------------------------------------------------
    fude_examlog_open("saves/exams.kana");
    CHECK(fude_examlog_count() == 0 && fude_examlog_streak(0x6728) == 0);

    // --- setup: the sources and lengths --------------------------------------------
    fude_exam e; fude_exam_init(&e, &db, &cat);
    fude_exam_open(&e); frame(&e);
    CHECK(e.open && e.stage == FUDE_EXAM_SETUP && e.chip_count >= 9);
    printf("sources: N5 %u, hiragana %u, katakana %u, studying %u\n", fude_exam_source_size(&e, FUDE_EXAM_SOURCE_N5),
           fude_exam_source_size(&e, FUDE_EXAM_SOURCE_HIRAGANA), fude_exam_source_size(&e, FUDE_EXAM_SOURCE_KATAKANA), fude_exam_source_size(&e, FUDE_EXAM_SOURCE_STUDYING));
    CHECK(fude_exam_source_size(&e, FUDE_EXAM_SOURCE_N5) > 50 && fude_exam_source_size(&e, FUDE_EXAM_SOURCE_HIRAGANA) > 40);
    CHECK(fude_exam_source_size(&e, FUDE_EXAM_SOURCE_STUDYING) == 0);
    // The kana sources: the gojūon and its voiced forms, 71 each — no small or old ones.
    CHECK(fude_exam_source_size(&e, FUDE_EXAM_SOURCE_HIRAGANA) == 71 && fude_exam_source_size(&e, FUDE_EXAM_SOURCE_KATAKANA) == 71);
    { const u32 out[] = { 0x3041, 0x3063, 0x3090, 0x30F4 };   // ぁ っ ゐ ヴ
      for(u32 i = 0; i < 4; i++) { u32 r; CHECK(fude_kanji_find_index(&db, out[i], &r));
          fude_exam_open_with(&e, &r, 1); e.source = i < 3 ? FUDE_EXAM_SOURCE_HIRAGANA : FUDE_EXAM_SOURCE_KATAKANA; rde_arr_clear(&e.selection);
          e.length = 3; fude_exam_preview(&e); for(u32 j = 0; j < e.count; j++) CHECK(e.items[j].record != r); }
      fude_exam_open(&e); }
    e.source = FUDE_EXAM_SOURCE_N5; e.length = 0;   // 10
    CHECK(fude_exam_planned(&e) == 10);
    fude_exam_preview(&e); frame(&e);
    CHECK(e.stage == FUDE_EXAM_PREVIEW && e.count == 10 && fude_exam_included(&e) == 10);
    fude_exam_tick_all(&e, false); CHECK(fude_exam_included(&e) == 0);
    fude_exam_start(&e); CHECK(e.stage == FUDE_EXAM_PREVIEW);   // nothing to ask: stays
    fude_exam_tick_all(&e, true);
    e.items[3].included = false;   // left out
    CHECK(fude_exam_included(&e) == 9);

    // --- an exam, all written right but one ------------------------------------------
    u32 other; CHECK(fude_kanji_find_index(&db, 0x3042, &other));   // あ, not in N5's kanji
    const u32 right = take(&e, &db, 2, other);
    printf("exam 1: %u of %u right; item 0 %.0f points\n", right, e.asked, (f64)e.items[e.order[0]].score);
    CHECK(e.asked == 9 && right == 8);
    CHECK(!e.items[e.order[2]].correct && e.items[e.order[2]].score == 0.0f && e.items[e.order[2]].read_as == other);
    CHECK(e.items[e.order[0]].correct && e.items[e.order[0]].score > 80.0f);
    CHECK(fude_examlog_count() == 1 && fude_examlog_exams()[0].item_count == 9 && fude_examlog_exams()[0].correct == 8);
    u32 wrong[8]; CHECK(fude_exam_wrong(&e, wrong, 8) == 1);

    // Three in a row: Known. Wrong once Known: Studying again.
    fude_kanji_info first; fude_kanji_at(&db, e.items[e.order[0]].record, &first);
    CHECK(fude_examlog_streak(first.codepoint) == 1 && fude_marks_get(first.codepoint) == FUDE_MARK_NONE);
    const u32 keep[9]; u32 n9 = fude_exam_asked(&e, (u32*)keep, 9); CHECK(n9 == 9);
    for(int t = 0; t < 2; t++) { fude_exam_open_with(&e, keep, n9); CHECK(e.stage == FUDE_EXAM_PREVIEW && e.count == 9); take(&e, &db, -1, 0); }
    CHECK(fude_examlog_count() == 3 && fude_examlog_streak(first.codepoint) == 3 && fude_marks_get(first.codepoint) == FUDE_MARK_KNOWN);
    fude_exam_open_with(&e, &keep[0], 1); take(&e, &db, 0, other);
    CHECK(fude_marks_get(first.codepoint) == FUDE_MARK_STUDYING && fude_examlog_streak(first.codepoint) == 0);

    // Retry wrong: straight to writing, only the wrong one.
    fude_exam_retry_wrong(&e);
    CHECK(e.stage == FUDE_EXAM_WRITING && e.asked == 1 && e.items[e.order[0]].record == keep[0]);
    fude_exam_close(&e);

    // The log survives a reload (results; drawings skipped).
    fude_examlog_close(); fude_examlog_open("saves/exams.kana");
    CHECK(fude_examlog_count() == 4 && fude_examlog_exams()[0].correct == 8 && fude_examlog_exams()[3].item_count == 1);
    CHECK(fude_examlog_streak(first.codepoint) == 0);
    const fude_examlog_item* it = &fude_examlog_items()[fude_examlog_exams()[0].first_item + 2];
    fude_kanji_info oi; fude_kanji_at(&db, other, &oi);
    CHECK(!it->correct && it->read_as == oi.codepoint);

    // The writing, read back: the first exam's nine answers, and every answer to
    // the first character (in all four exams).
    { rde_memory_allocator* hp = rde_memory_allocator_get_default_std();
      rde_arr w = rde_arr_new(sizeof(fude_examlog_writing), hp), st = rde_arr_new(sizeof(fude_history_stroke), hp), pt = rde_arr_new(sizeof(fude_history_point), hp);
      CHECK(fude_examlog_read_writing(0, 0, &w, &st, &pt) == 9);
      const fude_examlog_writing* ws = (const fude_examlog_writing*)w.memory;
      fude_kanji_info i0; u32 r0i; CHECK(fude_kanji_find_index(&db, fude_examlog_items()[0].codepoint, &r0i)); fude_kanji_at(&db, r0i, &i0);
      CHECK(ws[0].exam == 0 && ws[0].item == 0 && ws[0].stroke_count == i0.strokes && ws[8].item == 8);
      const fude_history_point* p0 = &((const fude_history_point*)pt.memory)[((const fude_history_stroke*)st.memory)[0].first_point];
      CHECK(((const fude_history_stroke*)st.memory)[0].point_count > 2 && p0[1].time > p0[0].time);
      rde_arr_clear(&w); rde_arr_clear(&st); rde_arr_clear(&pt);
      CHECK(fude_examlog_read_writing(UINT32_MAX, first.codepoint, &w, &st, &pt) == 4);
      ws = (const fude_examlog_writing*)w.memory;
      CHECK(ws[0].exam == 0 && ws[3].exam == 3 && ws[3].item == 0);
      CHECK(fude_examlog_read_writing(7, 0, &w, &st, &pt) == 0);
      rde_arr_free(&w); rde_arr_free(&st); rde_arr_free(&pt); }

    // A kept exam, again: its results as they were, the writing back, nothing kept twice.
    CHECK(!fude_exam_open_kept(&e, 99));
    CHECK(fude_exam_open_kept(&e, 0));
    CHECK(e.open && e.stage == FUDE_EXAM_RESULTS && e.saved && e.kept == 0 && e.asked == 9 && fude_exam_graded(&e));
    { u32 right0 = 0; for(u32 i = 0; i < e.asked; i++) right0 += e.items[e.order[i]].correct;
      CHECK(right0 == 8 && !e.items[2].correct && e.items[2].read_as == other && e.items[0].score > 80.0f);
      fude_kanji_info i0; fude_kanji_at(&db, e.items[0].record, &i0);
      CHECK(fude_ink_alive_strokes(&e.items[0].ink) == i0.strokes); }
    for(int f = 0; f < 5; f++) frame(&e);
    CHECK(fude_examlog_count() == 4);
    fude_exam_retry_wrong(&e);
    CHECK(e.stage == FUDE_EXAM_WRITING && e.asked == 1 && e.kept == UINT32_MAX && !e.saved);
    fude_exam_close(&e);

    // Reviews (the side panel's): what is due, straight to writing — it opened
    // empty once, the list it was given never read. And the Exam opened after one
    // is back on a source of its own, not the review's list.
    fude_exam_open_review(&e, keep, 3);
    CHECK(e.source == FUDE_EXAM_SOURCE_REVIEW && e.stage == FUDE_EXAM_WRITING && e.count == 3 && e.asked == 3);
    { b8 all_kept = true; for(u32 i = 0; i < e.asked; i++) { const u32 rec = e.items[e.order[i]].record; all_kept &= rec == keep[0] || rec == keep[1] || rec == keep[2]; }
      CHECK(all_kept); }
    fude_exam_close(&e);
    fude_exam_open(&e);
    CHECK(e.source != FUDE_EXAM_SOURCE_REVIEW && e.stage == FUDE_EXAM_SETUP);
    fude_exam_close(&e);

    // A kana exam: hiragana, prompt romaji; written right.
    fude_exam_open(&e); e.source = FUDE_EXAM_SOURCE_HIRAGANA; e.length = 0; fude_exam_preview(&e);
    CHECK(e.count == 10);
    printf("hiragana exam: %u of 10 right\n", take(&e, &db, -1, 0));

    fude_exam_destroy(&e); fude_catalog_destroy(&cat); fude_kanji_unload(&db);
    fude_marks_close(); fude_examlog_close();
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
