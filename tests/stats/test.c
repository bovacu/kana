#include "screens/stats.h"
#include "study/history.h"
#include "study/examlog.h"
#include "study/marks.h"
#include "base/theme.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)
void rde_rendering_2d_draw_rectangle(const rde_vec_2F a, const rde_vec_2F b, const rde_color c) { (void)a; (void)b; (void)c; }
void rde_rendering_2d_draw_rounded_rectangle(const rde_vec_2F a, const rde_vec_2F b, f32 r, u32 p, const rde_color c, rde_shader* s) { (void)a; (void)b; (void)r; (void)p; (void)c; (void)s; }
void rde_rendering_2d_draw_text_2(rde_font* f, const c8* t, rde_vec_3F p, rde_vec_2F s, f32 r, rde_color c) { (void)f; (void)t; (void)p; (void)s; (void)r; (void)c; }
void rde_rendering_begin_clipping_rect(const rde_window* w, rde_vec_2I p, rde_vec_2UI s) { (void)w; (void)p; (void)s; }
void rde_rendering_end_clipping_rect(void) {}
static rde_vec_2I g_window = { 744, 1133 };
rde_vec_2I rde_window_get_size(const rde_window* w) { (void)w; return g_window; }
rde_vec_4I rde_window_get_safe_area_insets(const rde_window* w) { (void)w; return (rde_vec_4I){ 0, 24, 0, 20 }; }
void rde_memcpy(any d, const any s, usize n) { memcpy(d, s, n); }
f64 rde_engine_get_time_now(void) { return 100.0; }
void rde_rendering_2d_draw_stroke(const rde_vec_2F* p, const f32* r, u32 n, rde_color c) { (void)p; (void)r; (void)n; (void)c; }
void rde_rendering_2d_draw_circle(const rde_vec_2F p, f32 r, u32 s, const rde_color c, rde_shader* sh) { (void)p; (void)r; (void)s; (void)c; (void)sh; }
void rde_rendering_2d_draw_circle_border(const rde_vec_2F p, f32 r, f32 t, u32 s, const rde_color c, rde_shader* sh) { (void)p; (void)r; (void)t; (void)s; (void)c; (void)sh; }

static void stroke(kana_ink* k, f32 x0, f32 y, u32 n) { kana_ink_begin(k, (rde_vec_2F){ x0, y }, true, false); for(u32 i = 1; i < n; i++) { k->sample_time += 0.05; kana_ink_extend(k, (rde_vec_2F){ x0 + 5.0f * (f32)i, y }); } kana_ink_end(k); }

// A session of _cp, _days_ago, two squares scored _a and _b (one with the mistakes in _m: order, direction, missing, extra).
static void session(u32 cp, i32 days_ago, f32 a, f32 b, const u8 m[4], f32 shape) {
    kana_ink ink; kana_ink_init(&ink); stroke(&ink, 10, 100, 11);   // one stroke, ~0.5 s
    kana_score sc[2]; memset(sc, 0, sizeof sc);
    sc[0].score = a; sc[0].shape = shape; sc[0].misplaced = m[0]; sc[0].reversed = m[1]; sc[0].missing = m[2]; sc[0].extra = m[3];
    sc[1].score = b; sc[1].shape = 95.0f;
    const kana_ink* d[2] = { &ink, &ink };
    CHECK(kana_history_save(cp, (u64)time(NULL) - (u64)days_ago * 86400u, 2, sc, d, 200.0f));
    kana_ink_destroy(&ink);
}

int main(int argc, char** argv) {
    system("rm -rf saves");
    kana_kanji_db db; CHECK(kana_kanji_load(&db, argc > 1 ? argv[1] : "characters.kana"));
    kana_catalog cat; kana_catalog_init(&cat, &db);
    kana_marks_open("saves/marks.kana"); kana_examlog_open("saves/exams.kana");

    // Nothing yet.
    kana_stats_data d; kana_stats_compute(&d, &db, &cat);
    CHECK(d.sessions == 0 && d.squares == 0 && d.average < 0.0f && d.exams == 0 && d.streak == 0 && d.weakest_count == 0 && d.first_time == 0);
    CHECK(d.mark_changes == 0 && d.week_known[KANA_STATS_WEEKS - 1] == 0);
    CHECK(d.coverage[KANA_STATS_GROUP_HIRAGANA].total == 71 && d.coverage[KANA_STATS_GROUP_N5].total > 50 && d.coverage[KANA_STATS_GROUP_N5].practised == 0);

    // 木: three days in a row up to today, getting better; 日: 40 days ago, then yesterday, worse;
    // 人: once, 10 days ago, backwards and poor.
    const u8 none[4] = { 0, 0, 0, 0 }, order[4] = { 1, 0, 0, 0 }, rev[4] = { 0, 1, 0, 0 }, miss[4] = { 0, 0, 1, 1 };
    session(0x6728, 2, 50, 50, miss, 55);
    session(0x6728, 1, 70, 70, order, 80);
    session(0x6728, 0, 90, 90, none, 95);
    session(0x65E5, 40, 80, 80, none, 90);
    session(0x65E5, 1, 60, 60, none, 70);
    session(0x4EBA, 10, 30, 30, rev, 40);
    // Exams: 5 days ago, 木 right and 日 wrong (not passed); today, 木 right (passed).
    kana_examlog_item e1[2] = { { 0x6728, true, 90, 90, 0x6728 }, { 0x65E5, false, 0, 40, 0x76EE } };
    CHECK(kana_examlog_add((u64)time(NULL) - 5u * 86400u, 2, e1, 2, NULL, 1000.0f));
    kana_examlog_item e2[1] = { { 0x6728, true, 100, 100, 0x6728 } };
    CHECK(kana_examlog_add((u64)time(NULL), 2, e2, 1, NULL, 1000.0f));
    // Marks over time: 日 Known 100 days ago, unmarked 50 days ago; 木 Studying 60
    // days ago, Known 5 days ago; 人 Studying 10 days ago.
    const u64 now = (u64)time(NULL);
    const u32 m_hi = 0x65E5, m_ki = 0x6728, m_hito = 0x4EBA;
    kana_marks_set_many_at(&m_hi, 1, KANA_MARK_KNOWN, now - 100u * 86400u);
    kana_marks_set_many_at(&m_ki, 1, KANA_MARK_STUDYING, now - 60u * 86400u);
    kana_marks_set_many_at(&m_hi, 1, KANA_MARK_NONE, now - 50u * 86400u);
    kana_marks_set_many_at(&m_hito, 1, KANA_MARK_STUDYING, now - 10u * 86400u);
    kana_marks_set_many_at(&m_ki, 1, KANA_MARK_KNOWN, now - 5u * 86400u);

    kana_stats_compute(&d, &db, &cat);
    printf("sessions %u squares %u chars %u avg %.1f recent %.1f before %.1f days %u streak %u best %u writing %.1fs\n", d.sessions, d.squares, d.characters,
           (f64)d.average, (f64)d.average_recent, (f64)d.average_before, d.days_active, d.streak, d.streak_best, (f64)d.writing_seconds);
    CHECK(d.sessions == 6 && d.squares == 12 && d.characters == 3);
    CHECK(d.average > 63.3f && d.average < 63.4f);                    // (50+70+90+80+60+30)*2 / 12
    CHECK(d.average_recent > 59.9f && d.average_recent < 60.1f);      // all but the 40 days ago: (50+70+90+60+30)/5
    CHECK(d.average_before > 79.9f && d.average_before < 80.1f);
    CHECK(d.days_active == 6 && d.streak == 3 && d.streak_best == 3);  // -40, -10, -5, -2, -1, 0
    CHECK(d.writing_seconds > 5.0f && d.writing_seconds < 7.0f);        // 12 squares x ~0.5 s
    CHECK(d.mistakes[KANA_STATS_MISTAKE_ORDER] == 1 && d.mistakes[KANA_STATS_MISTAKE_DIRECTION] == 1 && d.mistakes[KANA_STATS_MISTAKE_MISSING] == 1 &&
          d.mistakes[KANA_STATS_MISTAKE_EXTRA] == 1 && d.mistakes[KANA_STATS_MISTAKE_SHAPE] == 2);   // shapes 55 and 40
    CHECK(d.mistakes_recent[KANA_STATS_MISTAKE_DIRECTION] == 1 && d.squares_recent == 10);
    u32 hours = 0, days = 0; for(u32 i = 0; i < 24; i++) hours += d.hours[i]; for(u32 i = 0; i < 7; i++) days += d.weekdays[i];
    CHECK(hours == 12 && days == 12);
    u32 cal = 0; for(u32 i = 0; i < KANA_STATS_DAYS; i++) cal += d.day_squares[i] + d.day_exam_items[i];
    CHECK(cal == 12 + 3);   // every square and exam answer is inside the 26 weeks
    CHECK(d.exams == 2 && d.exams_passed == 1 && d.exam_items == 3 && d.exam_right == 2 && d.recent_exams == 2);
    CHECK(d.recent_accuracy[0] == 0.5f && d.recent_accuracy[1] == 1.0f);
    CHECK(d.known == 1 && d.studying == 1);
    CHECK(d.mark_changes == 5 && d.known_before == 0 && d.week_known[KANA_STATS_WEEKS - 1] == 1 && d.week_studying[KANA_STATS_WEEKS - 1] == 1);
    { const i64 start = d.today - (i64)(((d.today + 3) % 7 + 7) % 7) - 7 * (i64)(KANA_STATS_WEEKS - 1u);
      #define WEEK_OF(_days_ago) ((u32)((d.today - (_days_ago) - start) / 7))
      CHECK(d.week_known[WEEK_OF(150)] == 0 && d.week_studying[WEEK_OF(150)] == 0);   // nothing yet
      CHECK(d.week_known[WEEK_OF(70)] == 1 && d.week_studying[WEEK_OF(70)] == 0);     // 日
      CHECK(d.week_known[WEEK_OF(40)] == 0 && d.week_studying[WEEK_OF(40)] == 1);     // 木
      #undef WEEK_OF
    }
    const kana_stats_coverage* n5 = &d.coverage[KANA_STATS_GROUP_N5];
    CHECK(n5->practised == 3 && n5->known == 1 && n5->studying == 1);
    u32 r_ki, r_hi, r_hito; kana_kanji_find_index(&db, 0x6728, &r_ki); kana_kanji_find_index(&db, 0x65E5, &r_hi); kana_kanji_find_index(&db, 0x4EBA, &r_hito);
    CHECK(d.weakest_count == 3 && d.weakest[0] == r_hito && d.weakest[1] == r_hi && d.weakest[2] == r_ki);
    CHECK(d.improved_count == 1 && d.improved[0] == r_ki && d.improved_delta[0] == 40.0f);

    // The screen: renders in portrait and landscape, scrolls, and a tapped character is for the viewer.
    kana_stats s; kana_stats_init(&s, &db, &cat); kana_stats_open(&s);
    const f32 top = 566.5f - 24.0f - 8.0f, bottom = -566.5f + 20.0f + 76.0f + 24.0f;
    kana_stats_render(&s, NULL, NULL, 32.0f, top, bottom);
    printf("portrait content %.0f, view %.0f\n", (f64)s.content_height, (f64)(s.view_top - s.view_bottom));
    CHECK(s.content_height > s.view_top - s.view_bottom);
    for(int i = 0; i < 5; i++) { kana_stats_update(&s, 0.016f); }
    s.scroller.offset = s.content_height;   // to the end
    kana_stats_update(&s, 0.016f);
    kana_stats_render(&s, NULL, NULL, 32.0f, top, bottom);
    CHECK(rde_arr_length(&s.hits) == 4);   // three weakest, one most improved
    const kana_stats_hit* h = &((const kana_stats_hit*)s.hits.memory)[1];
    const rde_vec_2F at = { h->x + h->size * 0.5f, s.view_top - (h->y + h->size * 0.5f - s.scroller.offset) };
    kana_stats_pointer_down(&s, at, 10.0); kana_stats_pointer_up(&s, 10.05); kana_stats_update(&s, 0.016f);
    const u32* recs; u32 n, pos;
    CHECK(kana_stats_take_tap(&s, &recs, &n, &pos) && n == 3 && pos == 1 && recs[pos] == r_hi);
    CHECK(!kana_stats_take_tap(&s, &recs, &n, &pos));
    g_window = (rde_vec_2I){ 1133, 744 }; s.scroller.offset = 0.0f;
    kana_stats_render(&s, NULL, NULL, 32.0f, 372.0f - 32.0f, -372.0f + 120.0f);
    printf("landscape content %.0f\n", (f64)s.content_height);
    CHECK(s.content_height > 0.0f);
    kana_stats_destroy(&s);

    kana_catalog_destroy(&cat); kana_kanji_unload(&db); kana_marks_close(); kana_examlog_close();
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
