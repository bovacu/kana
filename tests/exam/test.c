#include "screens/exam.h"
#include "study/examlog.h"
#include "base/kfile.h"
#include "study/marks.h"
#include "study/vocab.h"
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
rde_vec_2I rde_window_get_size(const rde_window* w) { (void)w; return (rde_vec_2I){ 744, 1133 }; }
rde_vec_4I rde_window_get_safe_area_insets(const rde_window* w) { (void)w; return (rde_vec_4I){ 0, 24, 0, 20 }; }
f64 rde_engine_get_time_now(void) { return g_now; }
void rde_memcpy(any d, const any s, usize n) { memcpy(d, s, n); }

static const f32 TOP = 566.5f - 24.0f - 8.0f, BOTTOM = -566.5f + 20.0f + 76.0f + 24.0f;
static void frame(kana_exam* e) { g_now += 0.016; kana_exam_update(e, 0.016f); kana_exam_render(e, NULL, NULL, 32.0f, TOP, BOTTOM); }

// Writes record _as (its reference strokes) in the exam's square, with the pen.
static void write_as(kana_exam* e, const kana_kanji_db* db, u32 as) {
    kana_kanji_info info; kana_kanji_at(db, as, &info);
    static rde_vec_2F pts[KANA_GLYPH_MAX_POINTS];
    const rde_vec_2F tl = e->square_tl; const f32 k = e->square_size / KANA_KANJI_BOX;
    for(u32 s = 0; s < info.strokes; s++) {
        kana_kanji_stroke st; if(!kana_kanji_stroke_at(db, &info, s, &st)) continue;
        const u32 n = kana_glyph_stroke_points(&st, pts, NULL);
        for(u32 i = 0; i < n; i++) {
            const rde_vec_2F at = { tl.x + pts[i].x * k, tl.y - pts[i].y * k };
            g_now += 0.008;
            if(i == 0) kana_exam_pointer_down(e, at, true, g_now); else kana_exam_pointer_moved(e, at, g_now);
        }
        kana_exam_pointer_up(e, g_now);
    }
}

// An exam of what the preview holds, each written as itself (or, for _wrong_at,
// as another character); returns how many were right.
static u32 take(kana_exam* e, const kana_kanji_db* db, i32 wrong_at, u32 wrong_as) {
    kana_exam_start(e); frame(e);
    CHECK(e->stage == KANA_EXAM_WRITING && e->square_size > 200.0f);
    for(u32 i = 0; i < e->asked; i++) {
        write_as(e, db, (i32)i == wrong_at ? wrong_as : e->items[e->order[i]].record);
        CHECK(kana_exam_at_last(e) == (i + 1 == e->asked));
        kana_exam_next(e); frame(e);
    }
    CHECK(e->stage == KANA_EXAM_RESULTS);
    for(int f = 0; f < 400 && !e->saved; f++) frame(e);
    CHECK(kana_exam_graded(e) && e->saved);
    u32 right = 0; for(u32 i = 0; i < e->asked; i++) right += e->items[e->order[i]].correct;
    return right;
}

int main(int argc, char** argv) {
    kana_kanji_db db; CHECK(kana_kanji_load(&db, argc > 1 ? argv[1] : "characters.kana"));
    kana_catalog cat; kana_catalog_init(&cat, &db);
    system("rm -rf saves && mkdir -p saves");

    // --- marks: set, list, NONE removes, a file round trip ---------------------------
    kana_marks_open("saves/marks.kana");
    CHECK(kana_marks_count(KANA_MARK_STUDYING) == 0);
    const u32 r0 = kana_marks_revision();
    CHECK(kana_marks_set(0x6728, KANA_MARK_STUDYING) && kana_marks_get(0x6728) == KANA_MARK_STUDYING && kana_marks_revision() != r0);
    const u32 many[3] = { 0x65E5, 0x3042, 0x4EBA };
    CHECK(kana_marks_set_many(many, 3, KANA_MARK_KNOWN));
    CHECK(kana_marks_count(KANA_MARK_KNOWN) == 3 && kana_marks_count(KANA_MARK_STUDYING) == 1);
    u32 list[8]; CHECK(kana_marks_list(KANA_MARK_KNOWN, list, 8) == 3 && list[0] == 0x3042 && list[2] == 0x65E5);   // code point order
    kana_marks_set(0x4EBA, KANA_MARK_NONE);
    CHECK(kana_marks_get(0x4EBA) == KANA_MARK_NONE && kana_marks_count(KANA_MARK_KNOWN) == 2);
    // Every change kept, in order: 木 Studying, three Known, 人 unmarked (setting a mark it has is no change).
    CHECK(kana_marks_set(0x6728, KANA_MARK_STUDYING));
    CHECK(kana_marks_history_count() == 5);
    { const kana_marks_change* c = kana_marks_history();
      CHECK(c[0].codepoint == 0x6728 && c[0].mark == KANA_MARK_STUDYING && c[1].mark == KANA_MARK_KNOWN && c[4].codepoint == 0x4EBA && c[4].mark == KANA_MARK_NONE);
      CHECK(c[0].time <= c[4].time && c[0].time > 1700000000ull); }
    kana_marks_close(); CHECK(kana_marks_get(0x6728) == KANA_MARK_NONE && kana_marks_history_count() == 0);
    kana_marks_open("saves/marks.kana");
    CHECK(kana_marks_get(0x6728) == KANA_MARK_STUDYING && kana_marks_get(0x65E5) == KANA_MARK_KNOWN && kana_marks_count(KANA_MARK_KNOWN) == 2);
    CHECK(kana_marks_history_count() == 5 && kana_marks_history()[4].codepoint == 0x4EBA && kana_marks_history()[4].mark == KANA_MARK_NONE);
    // A file from before changes were kept: its marks start the history, oldest first.
    { kana_bytes b = kana_bytes_new(128);
      kana_put_header(&b, 1u, KANA_TAG('M', 'A', 'R', 'K'));
      const u32 ch = kana_chunk_begin(&b, KANA_TAG('M', 'R', 'K', 'S'));
      kana_put_u32(&b, 2u); kana_put_u32(&b, 16u);
      kana_put_u32(&b, 0x3042); kana_put_u8(&b, KANA_MARK_KNOWN);    kana_put_u8(&b, 0); kana_put_u8(&b, 0); kana_put_u8(&b, 0); kana_put_u32(&b, 2000u); kana_put_u32(&b, 0u);
      kana_put_u32(&b, 0x6728); kana_put_u8(&b, KANA_MARK_STUDYING); kana_put_u8(&b, 0); kana_put_u8(&b, 0); kana_put_u8(&b, 0); kana_put_u32(&b, 1000u); kana_put_u32(&b, 0u);
      kana_chunk_end(&b, ch);
      CHECK(kana_bytes_write_and_free(&b, "saves/old_marks.kana", NULL));
      kana_marks_close(); kana_marks_open("saves/old_marks.kana");
      CHECK(kana_marks_count(KANA_MARK_KNOWN) == 1 && kana_marks_history_count() == 2);
      CHECK(kana_marks_history()[0].codepoint == 0x6728 && kana_marks_history()[0].time == 1000u && kana_marks_history()[1].codepoint == 0x3042);
      kana_marks_close(); kana_marks_open("saves/marks.kana"); }
    // The catalog's filters follow them.
    u32 ki; CHECK(kana_kanji_find_index(&db, 0x6728, &ki) && kana_catalog_passes(&cat, ki, KANA_FILTER_STUDYING) && !kana_catalog_passes(&cat, ki, KANA_FILTER_KNOWN));
    kana_marks_set_many(many, 3, KANA_MARK_NONE); kana_marks_set(0x6728, KANA_MARK_NONE);

    // --- the learner's vocabulary (vocab.h, vocabtest has the rest): a kanji's words --
    kana_vocab_open("saves/words.kana");
    CHECK(kana_vocab_kanji_count(0x76EE) == 0 && kana_vocab_count() == 0);
    CHECK(kana_vocab_add("目薬", "めぐすり", "eye drops", 0x76EE) != 0 && kana_vocab_add("目覚まし", "めざまし", "alarm clock", 0x76EE) != 0);
    CHECK(kana_vocab_kanji_count(0x76EE) == 2 && kana_vocab_count() == 2 && kana_vocab_find("目薬", "めぐすり") != 0);
    kana_kanji_word uw; CHECK(kana_vocab_kanji_at(0x76EE, 1, &uw) && strcmp(uw.written, "目覚まし") == 0 && strcmp(uw.meaning, "alarm clock") == 0);
    kana_vocab_close(); kana_vocab_open("saves/words.kana");
    CHECK(kana_vocab_kanji_count(0x76EE) == 2);
    kana_vocab_remove(kana_vocab_find("目薬", "めぐすり"));
    CHECK(kana_vocab_kanji_count(0x76EE) == 1 && kana_vocab_find("目薬", "めぐすり") == 0);
    kana_vocab_remove(kana_vocab_find("目覚まし", "めざまし"));

    // --- the exam log ---------------------------------------------------------------
    kana_examlog_open("saves/exams.kana");
    CHECK(kana_examlog_count() == 0 && kana_examlog_streak(0x6728) == 0);

    // --- setup: the sources and lengths --------------------------------------------
    kana_exam e; kana_exam_init(&e, &db, &cat);
    kana_exam_open(&e); frame(&e);
    CHECK(e.open && e.stage == KANA_EXAM_SETUP && e.chip_count >= 9);
    printf("sources: N5 %u, hiragana %u, katakana %u, studying %u\n", kana_exam_source_size(&e, KANA_EXAM_SOURCE_N5),
           kana_exam_source_size(&e, KANA_EXAM_SOURCE_HIRAGANA), kana_exam_source_size(&e, KANA_EXAM_SOURCE_KATAKANA), kana_exam_source_size(&e, KANA_EXAM_SOURCE_STUDYING));
    CHECK(kana_exam_source_size(&e, KANA_EXAM_SOURCE_N5) > 50 && kana_exam_source_size(&e, KANA_EXAM_SOURCE_HIRAGANA) > 40);
    CHECK(kana_exam_source_size(&e, KANA_EXAM_SOURCE_STUDYING) == 0);
    // The kana sources: the gojūon and its voiced forms, 71 each — no small or old ones.
    CHECK(kana_exam_source_size(&e, KANA_EXAM_SOURCE_HIRAGANA) == 71 && kana_exam_source_size(&e, KANA_EXAM_SOURCE_KATAKANA) == 71);
    { const u32 out[] = { 0x3041, 0x3063, 0x3090, 0x30F4 };   // ぁ っ ゐ ヴ
      for(u32 i = 0; i < 4; i++) { u32 r; CHECK(kana_kanji_find_index(&db, out[i], &r));
          kana_exam_open_with(&e, &r, 1); e.source = i < 3 ? KANA_EXAM_SOURCE_HIRAGANA : KANA_EXAM_SOURCE_KATAKANA; rde_arr_clear(&e.selection);
          e.length = 3; kana_exam_preview(&e); for(u32 j = 0; j < e.count; j++) CHECK(e.items[j].record != r); }
      kana_exam_open(&e); }
    e.source = KANA_EXAM_SOURCE_N5; e.length = 0;   // 10
    CHECK(kana_exam_planned(&e) == 10);
    kana_exam_preview(&e); frame(&e);
    CHECK(e.stage == KANA_EXAM_PREVIEW && e.count == 10 && kana_exam_included(&e) == 10);
    kana_exam_tick_all(&e, false); CHECK(kana_exam_included(&e) == 0);
    kana_exam_start(&e); CHECK(e.stage == KANA_EXAM_PREVIEW);   // nothing to ask: stays
    kana_exam_tick_all(&e, true);
    e.items[3].included = false;   // left out
    CHECK(kana_exam_included(&e) == 9);

    // --- an exam, all written right but one ------------------------------------------
    u32 other; CHECK(kana_kanji_find_index(&db, 0x3042, &other));   // あ, not in N5's kanji
    const u32 right = take(&e, &db, 2, other);
    printf("exam 1: %u of %u right; item 0 %.0f points\n", right, e.asked, (f64)e.items[e.order[0]].score);
    CHECK(e.asked == 9 && right == 8);
    CHECK(!e.items[e.order[2]].correct && e.items[e.order[2]].score == 0.0f && e.items[e.order[2]].read_as == other);
    CHECK(e.items[e.order[0]].correct && e.items[e.order[0]].score > 80.0f);
    CHECK(kana_examlog_count() == 1 && kana_examlog_exams()[0].item_count == 9 && kana_examlog_exams()[0].correct == 8);
    u32 wrong[8]; CHECK(kana_exam_wrong(&e, wrong, 8) == 1);

    // Three in a row: Known. Wrong once Known: Studying again.
    kana_kanji_info first; kana_kanji_at(&db, e.items[e.order[0]].record, &first);
    CHECK(kana_examlog_streak(first.codepoint) == 1 && kana_marks_get(first.codepoint) == KANA_MARK_NONE);
    const u32 keep[9]; u32 n9 = kana_exam_asked(&e, (u32*)keep, 9); CHECK(n9 == 9);
    for(int t = 0; t < 2; t++) { kana_exam_open_with(&e, keep, n9); CHECK(e.stage == KANA_EXAM_PREVIEW && e.count == 9); take(&e, &db, -1, 0); }
    CHECK(kana_examlog_count() == 3 && kana_examlog_streak(first.codepoint) == 3 && kana_marks_get(first.codepoint) == KANA_MARK_KNOWN);
    kana_exam_open_with(&e, &keep[0], 1); take(&e, &db, 0, other);
    CHECK(kana_marks_get(first.codepoint) == KANA_MARK_STUDYING && kana_examlog_streak(first.codepoint) == 0);

    // Retry wrong: straight to writing, only the wrong one.
    kana_exam_retry_wrong(&e);
    CHECK(e.stage == KANA_EXAM_WRITING && e.asked == 1 && e.items[e.order[0]].record == keep[0]);
    kana_exam_close(&e);

    // The log survives a reload (results; drawings skipped).
    kana_examlog_close(); kana_examlog_open("saves/exams.kana");
    CHECK(kana_examlog_count() == 4 && kana_examlog_exams()[0].correct == 8 && kana_examlog_exams()[3].item_count == 1);
    CHECK(kana_examlog_streak(first.codepoint) == 0);
    const kana_examlog_item* it = &kana_examlog_items()[kana_examlog_exams()[0].first_item + 2];
    kana_kanji_info oi; kana_kanji_at(&db, other, &oi);
    CHECK(!it->correct && it->read_as == oi.codepoint);

    // The writing, read back: the first exam's nine answers, and every answer to
    // the first character (in all four exams).
    { rde_memory_allocator* hp = rde_memory_allocator_get_default_std();
      rde_arr w = rde_arr_new(sizeof(kana_examlog_writing), hp), st = rde_arr_new(sizeof(kana_history_stroke), hp), pt = rde_arr_new(sizeof(kana_history_point), hp);
      CHECK(kana_examlog_read_writing(0, 0, &w, &st, &pt) == 9);
      const kana_examlog_writing* ws = (const kana_examlog_writing*)w.memory;
      kana_kanji_info i0; u32 r0i; CHECK(kana_kanji_find_index(&db, kana_examlog_items()[0].codepoint, &r0i)); kana_kanji_at(&db, r0i, &i0);
      CHECK(ws[0].exam == 0 && ws[0].item == 0 && ws[0].stroke_count == i0.strokes && ws[8].item == 8);
      const kana_history_point* p0 = &((const kana_history_point*)pt.memory)[((const kana_history_stroke*)st.memory)[0].first_point];
      CHECK(((const kana_history_stroke*)st.memory)[0].point_count > 2 && p0[1].time > p0[0].time);
      rde_arr_clear(&w); rde_arr_clear(&st); rde_arr_clear(&pt);
      CHECK(kana_examlog_read_writing(UINT32_MAX, first.codepoint, &w, &st, &pt) == 4);
      ws = (const kana_examlog_writing*)w.memory;
      CHECK(ws[0].exam == 0 && ws[3].exam == 3 && ws[3].item == 0);
      CHECK(kana_examlog_read_writing(7, 0, &w, &st, &pt) == 0);
      rde_arr_free(&w); rde_arr_free(&st); rde_arr_free(&pt); }

    // A kept exam, again: its results as they were, the writing back, nothing kept twice.
    CHECK(!kana_exam_open_kept(&e, 99));
    CHECK(kana_exam_open_kept(&e, 0));
    CHECK(e.open && e.stage == KANA_EXAM_RESULTS && e.saved && e.kept == 0 && e.asked == 9 && kana_exam_graded(&e));
    { u32 right0 = 0; for(u32 i = 0; i < e.asked; i++) right0 += e.items[e.order[i]].correct;
      CHECK(right0 == 8 && !e.items[2].correct && e.items[2].read_as == other && e.items[0].score > 80.0f);
      kana_kanji_info i0; kana_kanji_at(&db, e.items[0].record, &i0);
      CHECK(kana_ink_alive_strokes(&e.items[0].ink) == i0.strokes); }
    for(int f = 0; f < 5; f++) frame(&e);
    CHECK(kana_examlog_count() == 4);
    kana_exam_retry_wrong(&e);
    CHECK(e.stage == KANA_EXAM_WRITING && e.asked == 1 && e.kept == UINT32_MAX && !e.saved);
    kana_exam_close(&e);

    // Reviews (the side panel's): what is due, straight to writing — it opened
    // empty once, the list it was given never read. And the Exam opened after one
    // is back on a source of its own, not the review's list.
    kana_exam_open_review(&e, keep, 3);
    CHECK(e.source == KANA_EXAM_SOURCE_REVIEW && e.stage == KANA_EXAM_WRITING && e.count == 3 && e.asked == 3);
    { b8 all_kept = true; for(u32 i = 0; i < e.asked; i++) { const u32 rec = e.items[e.order[i]].record; all_kept &= rec == keep[0] || rec == keep[1] || rec == keep[2]; }
      CHECK(all_kept); }
    kana_exam_close(&e);
    kana_exam_open(&e);
    CHECK(e.source != KANA_EXAM_SOURCE_REVIEW && e.stage == KANA_EXAM_SETUP);
    kana_exam_close(&e);

    // A kana exam: hiragana, prompt romaji; written right.
    kana_exam_open(&e); e.source = KANA_EXAM_SOURCE_HIRAGANA; e.length = 0; kana_exam_preview(&e);
    CHECK(e.count == 10);
    printf("hiragana exam: %u of 10 right\n", take(&e, &db, -1, 0));

    kana_exam_destroy(&e); kana_catalog_destroy(&cat); kana_kanji_unload(&db);
    kana_marks_close(); kana_examlog_close();
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
