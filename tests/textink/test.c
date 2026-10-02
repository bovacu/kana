#include "study/handwriting/textink.h"
#include "drawing/base/theme.h"
#include <stdio.h>
#include <string.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)
void rde_rendering_2d_draw_stroke(const rde_vec_2F* p, const f32* r, u32 n, rde_color c) { (void)p; (void)r; (void)n; (void)c; }
void rde_rendering_2d_draw_circle(const rde_vec_2F p, f32 r, u32 s, const rde_color c, rde_shader* sh) { (void)p; (void)r; (void)s; (void)c; (void)sh; }
void rde_rendering_2d_draw_rectangle(const rde_vec_2F a, const rde_vec_2F b, const rde_color c) { (void)a; (void)b; (void)c; }
void rde_rendering_2d_draw_text_2(rde_font* f, const c8* t, rde_vec_3F p, rde_vec_2F s, f32 r, rde_color c) { (void)f; (void)t; (void)p; (void)s; (void)r; (void)c; }
rde_vec_2I rde_window_get_size(const rde_window* w) { (void)w; return (rde_vec_2I){ 744, 1133 }; }
rde_vec_4I rde_window_get_safe_area_insets(const rde_window* w) { (void)w; return (rde_vec_4I){ 0, 24, 0, 20 }; }
f64 rde_engine_get_time_now(void) { return 1000.0; }
void rde_memcpy(any d, const any s, usize n) { memcpy(d, s, n); }

// _text written, put on a fresh page, read back.
static void round_trip(const fude_kanji_db* db, const fude_catalog* cat, const char* text, const char* expect, f32 cell) {
    fude_clip clip; memset(&clip, 0, sizeof clip);
    const fude_textink_result r = fude_textink_write(db, text, cell, cell * 8.0f, 2.0f, FUDE_THEME_INK, &clip);
    fude_ink ink; fude_ink_init(&ink);
    const u32 n = (u32)rde_arr_length(&clip.strokes);
    const u32 first = fude_ink_add_strokes(&ink, (const fude_ink_stroke*)clip.strokes.memory, n, (const fude_ink_point*)clip.points.memory, (rde_vec_2F){ 300.0f, -50.0f });
    u32 ids[2048]; for(u32 i = 0; i < n; i++) ids[n - 1 - i] = first + i;   // any order
    fude_textink_reader rd; fude_textink_reader_init(&rd, db, cat);
    CHECK(fude_textink_read(&rd, &ink, ids, n));
    int frames = 0; while(!fude_textink_update(&rd) && frames < 100) frames++;
    printf("'%s' -> %u written, %u left out, %u strokes -> read '%s'\n", text, r.drawn, r.skipped, n, rd.text);
    CHECK(strcmp(rd.text, expect) == 0);
    fude_textink_reader_destroy(&rd); fude_ink_destroy(&ink);
    rde_arr_free(&clip.strokes); rde_arr_free(&clip.points);
}

int main(int argc, char** argv) {
    fude_kanji_db db; CHECK(fude_kanji_load(&db, argc > 1 ? argv[1] : "characters.kana"));
    fude_catalog cat; fude_catalog_init(&cat, &db);

    // Writing: what the data has, what it does not, spaces and lines; centred.
    fude_clip clip; memset(&clip, 0, sizeof clip);
    fude_textink_result r = fude_textink_write(&db, "日本 語\n\xF0\x9F\x98\x80水", 100.0f, 1000.0f, 3.0f, FUDE_THEME_INK, &clip);
    CHECK(r.drawn == 4 && r.skipped == 1);                // the emoji is not in the data
    fude_kanji_info hi, mizu; fude_kanji_find(&db, 0x65E5, &hi); fude_kanji_find(&db, 0x6C34, &mizu);
    u32 strokes = (u32)rde_arr_length(&clip.strokes);
    { fude_kanji_info a, b; fude_kanji_find(&db, 0x672C, &a); fude_kanji_find(&db, 0x8A9E, &b);
      CHECK(strokes == hi.strokes + a.strokes + b.strokes + mizu.strokes); }
    rde_vec_2F mn = { 1e30f, 1e30f }, mx = { -1e30f, -1e30f };
    const fude_ink_point* p = (const fude_ink_point*)clip.points.memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&clip.points); i++) {
        mn.x = fminf(mn.x, p[i].position.x); mn.y = fminf(mn.y, p[i].position.y); mx.x = fmaxf(mx.x, p[i].position.x); mx.y = fmaxf(mx.y, p[i].position.y);
        CHECK(p[i].radius == 3.0f);
    }
    printf("block %.0f x %.0f, centre (%.1f, %.1f)\n", (f64)(mx.x - mn.x), (f64)(mx.y - mn.y), (f64)((mn.x + mx.x) * 0.5f), (f64)((mn.y + mx.y) * 0.5f));
    CHECK(fabsf((mn.x + mx.x) * 0.5f) < 4.0f && fabsf((mn.y + mx.y) * 0.5f) < 4.0f);   // centred (to the radius)
    CHECK(mx.x - mn.x > 300.0f && mx.x - mn.x < 400.0f);   // 日本 _ 語: four cells across
    CHECK(mx.y - mn.y > 180.0f && mx.y - mn.y < 240.0f);   // two lines
    // Wrapping: 10 characters, 4 a line → 3 lines.
    r = fude_textink_write(&db, "あいうえおかきくけこ", 50.0f, 200.0f, 1.0f, FUDE_THEME_INK, &clip);
    mn = (rde_vec_2F){ 1e30f, 1e30f }; mx = (rde_vec_2F){ -1e30f, -1e30f }; p = (const fude_ink_point*)clip.points.memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&clip.points); i++) { mn.y = fminf(mn.y, p[i].position.y); mx.y = fmaxf(mx.y, p[i].position.y); mn.x = fminf(mn.x, p[i].position.x); mx.x = fmaxf(mx.x, p[i].position.x); }
    CHECK(r.drawn == 10 && mx.x - mn.x <= 200.0f && mx.y - mn.y > 2.0f * 50.0f * 1.25f);
    r = fude_textink_write(&db, "", 50.0f, 200.0f, 1.0f, FUDE_THEME_INK, &clip);
    CHECK(r.drawn == 0 && rde_arr_length(&clip.strokes) == 0);
    rde_arr_free(&clip.strokes); rde_arr_free(&clip.points);

    // Reading back (no ML Kit here: Kana's own reading).
    round_trip(&db, &cat, "日本語", "日本語", 100.0f);
    round_trip(&db, &cat, "今日は", "今日は", 80.0f);
    round_trip(&db, &cat, "水を飲む\nねこ", "水を飲む\nねこ", 90.0f);
    round_trip(&db, &cat, "漢字", "漢字", 30.0f);

    // Nothing to read.
    fude_textink_reader rd; fude_textink_reader_init(&rd, &db, &cat);
    fude_ink empty; fude_ink_init(&empty);
    CHECK(!fude_textink_read(&rd, &empty, NULL, 0));
    CHECK(!fude_textink_update(&rd));
    fude_textink_reader_destroy(&rd); fude_ink_destroy(&empty);

    fude_catalog_destroy(&cat); fude_kanji_unload(&db);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
