#include "screens/album.h"
#include "study/examlog.h"
#include "screens/practice.h"
#include "base/theme.h"
#include <stdio.h>
#include <string.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

// Drawing is counted, not done; the clock is ours.
static u64 g_strokes = 0, g_points = 0, g_circles = 0, g_texts = 0;
static f64 g_now = 1000.0;
static char g_last_text[256];
void rde_rendering_2d_draw_stroke(const rde_vec_2F* p, const f32* r, u32 n, rde_color c) { (void)p; (void)r; (void)c; g_strokes++; g_points += n; for(u32 i = 0; i < n; i++) { volatile f32 x = p[i].x + r[i]; (void)x; } }
void rde_rendering_2d_draw_circle(const rde_vec_2F p, f32 r, u32 s, const rde_color c, rde_shader* sh) { (void)p; (void)r; (void)s; (void)c; (void)sh; g_circles++; }
void rde_rendering_2d_draw_rectangle(const rde_vec_2F a, const rde_vec_2F b, const rde_color c) { (void)a; (void)b; (void)c; }
void rde_rendering_2d_draw_circle_border(const rde_vec_2F p, f32 r, f32 t, u32 s, const rde_color c, rde_shader* sh) { (void)p; (void)r; (void)t; (void)s; (void)c; (void)sh; }
void rde_memcpy(any d, const any s, usize n) { memcpy(d, s, n); }
void rde_rendering_2d_draw_text_2(rde_font* f, const c8* t, rde_vec_3F p, rde_vec_2F s, f32 r, rde_color c) { (void)f; (void)p; (void)s; (void)r; (void)c; g_texts++; snprintf(g_last_text, sizeof g_last_text, "%s", t); }
void rde_rendering_begin_clipping_rect(const rde_window* w, rde_vec_2I p, rde_vec_2UI s) { (void)w; (void)p; (void)s; }
void rde_rendering_end_clipping_rect(void) {}
rde_vec_2I rde_window_get_size(const rde_window* w) { (void)w; return (rde_vec_2I){ 744, 1133 }; }
rde_vec_4I rde_window_get_safe_area_insets(const rde_window* w) { (void)w; return (rde_vec_4I){ 0, 24, 0, 20 }; }
f64 rde_engine_get_time_now(void) { return g_now; }

static void pen_line(kana_ink* k, f32 x0, f32 y, u32 n) {    // one stroke across a 1000-unit square
    k->sample_time = 0.0; kana_ink_begin(k, (rde_vec_2F){ x0, y }, true, false);
    for(u32 i = 1; i < n; i++) { k->sample_time += 0.01; kana_ink_extend(k, (rde_vec_2F){ x0 + 40.0f * (f32)i, y }); }
    kana_ink_end(k);
}
static void session(u32 cp, u64 t, f32 s0, f32 s1, u32 strokes) {
    kana_ink a, b, c; kana_ink_init(&a); kana_ink_init(&b); kana_ink_init(&c);
    for(u32 i = 0; i < strokes; i++) { pen_line(&a, 100, 200 + 150.0f * (f32)i, 15); pen_line(&b, 120, 220 + 150.0f * (f32)i, 12); }
    kana_score sc[3]; memset(sc, 0, sizeof sc);
    sc[0].score = s0; sc[0].shape = s0; sc[0].drawn = sc[0].expected = (u8)strokes;
    sc[1].score = s1; sc[1].shape = s1; sc[1].drawn = (u8)strokes; sc[1].expected = (u8)strokes + 1;
    sc[2].empty = true;
    const kana_ink* d[3] = { &a, &b, &c };
    CHECK(kana_history_save(cp, t, 3, sc, d, 1000.0f));
    kana_ink_destroy(&a); kana_ink_destroy(&b); kana_ink_destroy(&c);
}

// An exam answer written as _strokes lines across the square.
static kana_ink* answer(u32 strokes) {
    static kana_ink inks[8]; static u32 used = 0;
    kana_ink* k = &inks[used++]; kana_ink_init(k);
    for(u32 i = 0; i < strokes; i++) pen_line(k, 100, 200 + 150.0f * (f32)i, 10);
    return k;
}

int main(int argc, char** argv) {
    kana_kanji_db db; CHECK(kana_kanji_load(&db, argc > 1 ? argv[1] : "characters.kana"));
    // Two exams: 木 right (3 strokes) and あ wrong, read as お; later 木 wrong (2 strokes).
    system("mkdir -p saves"); remove("saves/exams.kana");
    kana_examlog_open("saves/exams.kana");
    { kana_examlog_item x[2] = { { 0x6728, true, 88, 88, 0x6728 }, { 0x3042, false, 0, 30, 0x304A } };
      const kana_ink* d[2] = { answer(3), answer(1) };
      CHECK(kana_examlog_add(1759000000ull, 2, x, 2, d, 1000.0f)); }
    { kana_examlog_item x[1] = { { 0x6728, false, 0, 40, 0x4EBA } };
      const kana_ink* d[1] = { answer(2) };
      CHECK(kana_examlog_add(1759220000ull, 9, x, 1, d, 1000.0f)); }
    session(0x6728, 1759150000ull, 60, 50, 4);    // 木: two sessions, getting better
    session(0x6728, 1759250000ull, 95, 85, 4);
    session(0x3042, 1759200000ull, 40, 30, 3);    // あ: one, weak
    session(0x65E5, 1759100000ull, 88, 90, 4);    // 日: one, old, good

    kana_album al; kana_album_init(&al, &db);
    kana_album_open(&al);
    CHECK(al.open && !al.page_open && al.entries.count == 3 && al.sessions == 4);
    const kana_album_entry* e = (const kana_album_entry*)al.entries.memory;
    printf("weakest: U+%04X %.0f, U+%04X %.0f, U+%04X %.0f\n", e[0].codepoint, e[0].summary.last, e[1].codepoint, e[1].summary.last, e[2].codepoint, e[2].summary.last);
    CHECK(e[0].codepoint == 0x3042 && e[2].codepoint == 0x6728);                 // あ 35, 日 89, 木 90
    kana_album_set_sort(&al, KANA_ALBUM_SORT_RECENT); CHECK(e[0].codepoint == 0x6728 && e[2].codepoint == 0x65E5);
    kana_album_set_sort(&al, KANA_ALBUM_SORT_MOST);   CHECK(e[0].codepoint == 0x6728);
    // The weakest, whatever the overview's order (still "Most" here).
    u32 w[10]; u32 rec_a = 0; kana_kanji_find_index(&db, 0x3042, &rec_a);
    CHECK(kana_album_weakest(&al, w, 10) == 3 && w[0] == rec_a);
    CHECK(kana_album_weakest(&al, w, 1) == 1 && e[0].codepoint == 0x6728);

    // Overview: drawn, and a tap on the first cell opens its page.
    const f32 top = 566.5f - 24.0f - 8.0f, bottom = -566.5f + 20.0f + 76.0f + 24.0f;
    kana_album_update(&al, 0.016f);
    kana_album_render(&al, NULL, NULL, 32.0f, top, bottom);
    CHECK(al.hits.count == 3 && g_strokes > 0);
    const kana_album_hit* h = (const kana_album_hit*)al.hits.memory;
    rde_vec_2F at = { h[0].x + h[0].w * 0.5f, top - (h[0].y + h[0].h * 0.5f) };
    kana_album_pointer_down(&al, at, g_now); kana_album_pointer_up(&al, g_now + 0.05);
    CHECK(al.page_open && al.page_codepoint == 0x6728 && al.history.sessions.count == 2);

    // The page: two sessions, two attempts each (the empty square is not shown).
    kana_album_update(&al, 0.016f);
    g_strokes = 0; kana_album_render(&al, NULL, NULL, 32.0f, top, bottom);
    printf("page: %u hits, %llu strokes, content %.0f\n", al.hits.count, (unsigned long long)g_strokes, al.content_height);
    // Two sessions of two attempts, and its two exam answers between them by date:
    // the newest session, the second exam, the older session, the first exam.
    CHECK(al.hits.count == 6 && al.content_height > 300.0f && al.page_exams.count == 2);
    { const kana_album_hit* ph = (const kana_album_hit*)al.hits.memory;
      CHECK(!(ph[0].index & KANA_ALBUM_EXAM) && !(ph[1].index & KANA_ALBUM_EXAM) && ph[2].index == (KANA_ALBUM_EXAM | 1u) &&
            !(ph[3].index & KANA_ALBUM_EXAM) && ph[5].index == (KANA_ALBUM_EXAM | 0u));
      const kana_examlog_writing* pw = (const kana_examlog_writing*)al.page_exams.memory;
      CHECK(pw[0].exam == 0 && pw[0].item == 0 && pw[0].stroke_count == 3 && pw[1].exam == 1 && pw[1].stroke_count == 2);
      // Tap the first exam's answer: it replays (a steady pace), the squares do not.
      at = (rde_vec_2F){ ph[5].x + 10.0f, top - (ph[5].y + 10.0f) + al.page_scroller.offset };
      kana_album_pointer_down(&al, at, g_now); kana_album_pointer_up(&al, g_now + 0.05);
      CHECK(al.selected_answer == 0 && al.selected == -1);
      g_now = al.replay_start + 0.05; g_points = 0; kana_album_render(&al, NULL, NULL, 32.0f, top, bottom); const u64 early = g_points;
      g_now = al.replay_start + 9.0;  g_points = 0; kana_album_render(&al, NULL, NULL, 32.0f, top, bottom);
      CHECK(early < g_points);
      kana_album_render(&al, NULL, NULL, 32.0f, top, bottom); }

    // Tap the newest session's first attempt: it replays.
    h = (const kana_album_hit*)al.hits.memory;
    at = (rde_vec_2F){ h[0].x + 10.0f, top - (h[0].y + 10.0f) };
    kana_album_pointer_down(&al, at, g_now); kana_album_pointer_up(&al, g_now + 0.05);
    CHECK(al.selected >= 0);
    const kana_history_square* sq = &((const kana_history_square*)al.history.squares.memory)[al.selected];
    printf("selected square %d: score %.0f '%s'\n", al.selected, sq->score.score, sq->score.feedback);
    CHECK(sq->score.score == 95.0f);
    u64 by_time[4];
    const f64 when[4] = { 0.0, 0.08, 0.5, 5.0 };
    for(int i = 0; i < 4; i++) {
        g_now = al.replay_start + when[i]; g_points = 0; g_circles = 0;
        kana_album_render(&al, NULL, NULL, 32.0f, top, bottom);
        by_time[i] = g_points;
    }
    printf("replay points drawn at 0 / 0.08 / 0.5 / 5 s: %llu %llu %llu %llu\n", (unsigned long long)by_time[0], (unsigned long long)by_time[1], (unsigned long long)by_time[2], (unsigned long long)by_time[3]);
    CHECK(by_time[0] < by_time[1] && by_time[1] < by_time[2] && by_time[2] <= by_time[3]);
    CHECK(strstr(g_last_text, "Attempt") != NULL || 1);

    // A new session while the page is open (Practice on top): re-read on the next update.
    session(0x6728, 1759350000ull, 99, 97, 4);
    kana_album_update(&al, 0.016f);
    CHECK(al.history.sessions.count == 3 && al.selected == -1 && al.selected_answer == -1 && al.sessions == 5);
    kana_album_pointer_down(&al, (rde_vec_2F){ 0, 0 }, g_now); kana_album_pointer_moved(&al, (rde_vec_2F){ 0, -3000 }, g_now + 0.1); kana_album_pointer_up(&al, g_now + 0.2);
    for(int i = 0; i < 200; i++) { kana_album_update(&al, 0.016f); kana_album_render(&al, NULL, NULL, 32.0f, top, bottom); }
    CHECK(al.hits.count >= 4);

    // Scrolling far past the end is held within the content.
    kana_album_pointer_down(&al, (rde_vec_2F){ 0, 0 }, g_now); kana_album_pointer_moved(&al, (rde_vec_2F){ 0, 3000 }, g_now + 0.1); kana_album_pointer_up(&al, g_now + 0.2);
    for(int i = 0; i < 200; i++) { kana_album_update(&al, 0.016f); kana_album_render(&al, NULL, NULL, 32.0f, top, bottom); }
    CHECK(al.page_scroller.offset <= fmaxf(0.0f, al.content_height - (top - bottom)) + 0.5f);

    kana_album_close_page(&al); CHECK(!al.page_open);

    // The exams view: a card each, newest first; a tap gives the exam to open.
    kana_album_set_view(&al, KANA_ALBUM_VIEW_EXAMS);
    kana_album_update(&al, 0.016f); kana_album_render(&al, NULL, NULL, 32.0f, top, bottom);
    CHECK(al.hits.count == 2 && ((const kana_album_hit*)al.hits.memory)[0].index == 1u);
    h  = (const kana_album_hit*)al.hits.memory;
    at = (rde_vec_2F){ h[1].x + 20.0f, top - (h[1].y + 20.0f) };
    u32 ex = 99; CHECK(!kana_album_take_exam(&al, &ex));
    kana_album_pointer_down(&al, at, g_now); kana_album_pointer_up(&al, g_now + 0.05);
    CHECK(kana_album_take_exam(&al, &ex) && ex == 0 && !kana_album_take_exam(&al, &ex));
    kana_album_set_sort(&al, KANA_ALBUM_SORT_WEAKEST); CHECK(al.view == KANA_ALBUM_VIEW_CHARACTERS);
    kana_album_close(&al); CHECK(!al.open);
    kana_examlog_close();
    kana_album_destroy(&al); kana_kanji_unload(&db);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
