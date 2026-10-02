#include "study/models/history.h"
#include "drawing/base/kfile.h"
#include <stdio.h>
#include <string.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)
static void stroke(fude_ink* k, f32 x0, f32 y, u32 n) { fude_ink_begin(k, (rde_vec_2F){ x0, y }, true, false); for(u32 i = 1; i < n; i++) { k->sample_time += 0.01; fude_ink_extend(k, (rde_vec_2F){ x0 + 5.0f * (f32)i, y }); } fude_ink_end(k); }
int main(void) {
    fude_ink a, b; fude_ink_init(&a); fude_ink_init(&b);
    stroke(&a, 10, 100, 20); stroke(&a, 10, 50, 10);           // square 0: 2 strokes, square size 200
    fude_score sc[2]; memset(sc, 0, sizeof sc);
    sc[0].score = 80; sc[0].shape = 90; sc[0].drawn = 2; sc[0].expected = 2; snprintf(sc[0].feedback, 96, "Good");
    sc[1].empty = true;                                          // square 1: left empty
    const fude_ink* d[2] = { &a, &b };
    CHECK(fude_history_save(0x6728, 1759150000ull, 2, sc, d, 200.0f));
    sc[0].score = 90; CHECK(fude_history_save(0x6728, 1759150100ull, 2, sc, d, 200.0f));
    fude_history_summary s; CHECK(fude_history_summarize(0x6728, &s));
    printf("sessions %u first %.0f last %.0f best %.0f time %llu\n", s.sessions, s.first, s.last, s.best, (unsigned long long)s.last_time);
    CHECK(s.sessions == 2 && s.first == 80.0f && s.last == 90.0f && s.best == 90.0f && s.last_time == 1759150100ull);
    CHECK(!fude_history_summarize(0x65E5, &s));

    // Read the first session's first square's drawing back.
    char path[512]; snprintf(path, sizeof path, "%s6728.kana", fude_history_dir());
    u32 size = 0; u8* data = fude_file_read(path, &size); fude_reader r = fude_reader_make(data, size);
    CHECK(fude_read_header(&r, 1, FUDE_TAG('P','R','A','C')));
    u32 tag; fude_reader c; CHECK(fude_next_chunk(&r, &tag, &c));
    fude_get_u32(&c); fude_get_u32(&c); fude_get_u32(&c); fude_get_f32(&c);
    u32 squares = fude_get_u32(&c), rec = fude_get_u32(&c); CHECK(squares == 2 && rec == 20);
    u32 start = c.pos; u8 scored = fude_get_u8(&c); f32 score = fude_get_f32(&c); c.pos = start + rec;
    u32 strokes = fude_get_u32(&c), pts = fude_get_u32(&c); u16 x = fude_get_u16(&c), y = fude_get_u16(&c);
    printf("square 0: scored %u score %.0f strokes %u first stroke %u points, first point %u,%u\n", scored, score, strokes, pts, x, y);
    CHECK(scored == 1 && score == 80.0f && strokes == 2 && pts == 20 && x == (u16)(10.0f / 200.0f * 65535.0f + 0.5f) && y == (u16)(0.5f * 65535.0f + 0.5f));
    fude_file_free(data);

    // The album's side: the list, the revision, the whole history back.
    CHECK(fude_history_revision() == 2);
    rde_arr list = rde_arr_new(sizeof(u32), NULL); fude_history_list(&list);
    printf("listed %u character(s)\n", list.count);
    CHECK(list.count == 1 && *(u32*)list.memory == 0x6728);            // not the .bak beside it
    rde_arr_free(&list);

    fude_history h; fude_history_init(&h);
    CHECK(fude_history_load(&h, 0x6728));
    CHECK(h.sessions.count == 2 && h.squares.count == 4 && h.strokes.count == 4 && h.points.count == 60);
    const fude_history_session* s0 = (const fude_history_session*)h.sessions.memory;
    const fude_history_square*  q  = (const fude_history_square*)h.squares.memory;
    const fude_history_stroke*  k  = (const fude_history_stroke*)h.strokes.memory;
    const fude_history_point*   pt = (const fude_history_point*)h.points.memory;
    printf("session 0: time %llu average %.0f squares %u; square 0 score %.0f '%s' strokes %u; square 1 '%s'\n", (unsigned long long)s0[0].time, s0[0].average, s0[0].square_count,
           q[0].score.score, q[0].score.feedback, q[0].stroke_count, q[1].score.feedback);
    CHECK(s0[0].time == 1759150000ull && s0[0].average == 80.0f && s0[1].average == 90.0f && s0[1].first_square == 2);
    CHECK(!q[0].score.empty && q[0].stroke_count == 2 && k[0].point_count == 20 && k[1].point_count == 10 && q[1].score.empty && q[1].stroke_count == 0);
    CHECK(strcmp(q[0].score.feedback, "Good") == 0 && strcmp(q[1].score.feedback, "Empty") == 0);
    CHECK(pt[0].x == x && pt[0].y == y && pt[1].time > 0.0f);
    CHECK(!fude_history_load(&h, 0x65E5) && h.sessions.count == 0);

    // A damaged tail (a session cut short) leaves the sessions before it.
    { FILE* f = fopen(path, "r+b"); fseek(f, 0, SEEK_END); long n = ftell(f); ftruncate(fileno(f), n - 7); fclose(f); }
    CHECK(fude_history_load(&h, 0x6728) && h.sessions.count == 1 && h.squares.count == 2 && h.points.count == 30);
    fude_history_destroy(&h);

    fude_ink_destroy(&a); fude_ink_destroy(&b);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
