#include "drawing/base/save.h"
#include "drawing/base/theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>

// --- engine stubs (stdio) ---------------------------------------------------------
struct rde_file { FILE* f; };
static b8 g_failed = false;
b8   rde_failed(void) { return g_failed; }
void rde_error_policy_push(RDE_ON_FAIL_ p) { (void)p; }
void rde_error_policy_pop(void) {}
void rde_log_level(RDE_LOG_LEVEL_ l, const c8* fmt, ...) { (void)l; va_list a; va_start(a, fmt); printf("  [log] "); vprintf(fmt, a); printf("\n"); va_end(a); }
void rde_log_color(RDE_LOG_COLOR_ c, const c8* fmt, ...) { (void)c; (void)fmt; }
rde_file* rde_file_open(const c8* p, RDE_FILE_MODE_ m) {
    FILE* f = fopen(p, m == RDE_FILE_MODE_WRITE_BYTES ? "wb" : "rb");
    g_failed = f == NULL; if(!f) return NULL;
    rde_file* r = malloc(sizeof(*r)); r->f = f; return r;
}
void rde_file_write_bytes(rde_file* f, const u8* d, usize n) { g_failed = fwrite(d, 1, n, f->f) != n; }
void rde_file_close(rde_file* f) { fclose(f->f); free(f); }
u8* rde_file_read_full_file_bytes(rde_file* f, usize* n, rde_memory_allocator* a) {
    fseek(f->f, 0, SEEK_END); long sz = ftell(f->f); rewind(f->f);
    u8* d = a->calloc(a->allocator, (usize)sz + 1, 1); *n = fread(d, 1, (usize)sz, f->f); g_failed = false; return d;
}
b8 rde_file_exists(const c8* p) { struct stat st; return stat(p, &st) == 0; }
b8 rde_file_move(const c8* a, const c8* b) { return rename(a, b) == 0; }
b8 rde_file_create_missing_dirs(const c8* p) { (void)p; mkdir("./saves", 0755); return true; }
static f64 fake_now = 1.0;
f64  rde_engine_get_time_now(void) { return fake_now; }
void rde_rendering_2d_draw_stroke(const rde_vec_2F* p, const f32* r, u32 n, rde_color c) { (void)p; (void)r; (void)n; (void)c; }
void rde_rendering_2d_draw_circle(const rde_vec_2F p, f32 r, u32 s, const rde_color c, rde_shader* sh) { (void)p; (void)r; (void)s; (void)c; (void)sh; }

static int fails = 0;
static fude_page PG;   // a page's options where the test is not about them
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

static void stroke(fude_ink* k, f32 y, u32 n, rde_color col) {
    k->color = col;
    fude_ink_begin(k, (rde_vec_2F){ 0.0f, y }, true, false);
    for(u32 i = 1; i < n; i++) { k->sample_time += 0.004; k->pressure = 0.002f * (f32)i; fude_ink_extend(k, (rde_vec_2F){ 3.5f * (f32)i, y + 0.25f * (f32)i }); }
    fude_ink_end(k);
}

// Alive strokes of a and b identical, field by field, in order.
static b8 same_page(const fude_ink* a, const fude_ink* b) {
    u32 i = 0, j = 0; const u32 na = fude_ink_stroke_count(a), nb = fude_ink_stroke_count(b);
    for(;;) {
        while(i < na && !fude_ink_stroke_at(a, i)->alive) i++;
        while(j < nb && !fude_ink_stroke_at(b, j)->alive) j++;
        if(i == na || j == nb) return i == na && j == nb;
        const fude_ink_stroke* sa = fude_ink_stroke_at(a, i); const fude_ink_stroke* sb = fude_ink_stroke_at(b, j);
        if(sa->point_count != sb->point_count || memcmp(&sa->color, &sb->color, sizeof(rde_color)) || sa->from_pen != sb->from_pen) return false;
        if(memcmp(fude_ink_stroke_points(a, sa), fude_ink_stroke_points(b, sb), sa->point_count * sizeof(fude_ink_point))) return false;
        i++; j++;
    }
}

static long file_size(const c8* p) { struct stat st; return stat(p, &st) == 0 ? (long)st.st_size : -1; }

int main(void) {
    const c8* dir = fude_save_dir();
    CHECK(strcmp(dir, "./saves/") == 0);
    c8 doc[256], set[256], bak[256], bad[256], doc2[256];
    snprintf(doc, sizeof doc, "%s%s", dir, FUDE_SAVE_DOCUMENT_FILE);
    snprintf(set, sizeof set, "%s%s", dir, FUDE_SAVE_SETTINGS_FILE);
    snprintf(bak, sizeof bak, "%s.bak", doc); snprintf(bad, sizeof bad, "%s.bad", doc);
    snprintf(doc2, sizeof doc2, "%spaper.kana", dir);

    // Nothing saved yet.
    { fude_ink k; fude_ink_init(&k); fude_view v = { {0,0}, 1 }; CHECK(fude_load_document(doc, &k, &v, &PG) == FUDE_LOAD_MISSING); fude_ink_destroy(&k); }

    // A page with erased strokes and history.
    fude_ink a; fude_ink_init(&a);
    stroke(&a, 0, 50, FUDE_THEME_INK);                                 // the theme's ink
    stroke(&a, 100, 7, (rde_color){ 230, 72, 72, 255 });
    stroke(&a, 200, 1, (rde_color){ 80, 140, 235, 255 });            // a single dot
    stroke(&a, 300, 400, (rde_color){ 90, 190, 110, 255 });
    fude_ink_erase_at(&a, (rde_vec_2F){ 0.0f, 100.0f }, 1.0f); fude_ink_erase_end(&a);   // erase the red one
    fude_view va = { { 123.5f, -45.25f }, 2.5f };

    u32 bytes = 0;
    CHECK(fude_save_document(doc, &a, va, (fude_page){ .paper = FUDE_PAPER_SQUARES }, &bytes));
    CHECK(bytes == 12 + (8 + 12) + (8 + 1) + (8 + 8 + 3 * 12) + (8 + 8 + (50 + 1 + 400) * 20));
    CHECK(file_size(doc) == (long)bytes);
    CHECK(!rde_file_exists(bak));   // first save: nothing to back up

    // Round trip: exactly the alive strokes, bit for bit, and the view.
    fude_ink b; fude_ink_init(&b); fude_view vb = { {0,0}, 1 }; fude_page pb = { 0 };
    CHECK(fude_load_document(doc, &b, &vb, &pb) == FUDE_LOAD_OK);
    CHECK(pb.paper == FUDE_PAPER_SQUARES);               // the page's paper, saved with it
    CHECK(fude_ink_alive_strokes(&b) == 3); CHECK(same_page(&a, &b));
    CHECK(memcmp(&va, &vb, sizeof va) == 0);
    { fude_ink k; fude_ink_init(&k); fude_view v = { {0,0}, 1 }; fude_page q;                      // every paper round trips
      for(u32 i = 0; i < FUDE_PAPER_COUNT; i++) { CHECK(fude_save_document(doc2, &k, v, (fude_page){ .paper = (FUDE_PAPER_)i }, NULL));
                                                 q.paper = (FUDE_PAPER_)((i + 1) % FUDE_PAPER_COUNT); CHECK(fude_load_document(doc2, &k, &v, &q) == FUDE_LOAD_OK && q.paper == (FUDE_PAPER_)i); }
      fude_ink_destroy(&k); }
    CHECK(!fude_ink_can_undo(&b));                       // history starts at the load
    // ...and the loaded page is fully editable: erase + undo + a new stroke.
    CHECK(fude_ink_erase_at(&b, (rde_vec_2F){ 0.0f, 0.0f }, 1.0f) == 1); fude_ink_erase_end(&b);
    CHECK(fude_ink_undo(&b)); CHECK(same_page(&a, &b));
    stroke(&b, 500, 5, (rde_color){ 0, 0, 0, 255 }); CHECK(fude_ink_alive_strokes(&b) == 4);

    // Second save keeps the first as .bak.
    CHECK(fude_save_document(doc, &b, vb, PG, NULL));
    CHECK(file_size(bak) == (long)bytes);

    // Damaged main file (truncated): recovered from the backup, damaged one kept.
    { FILE* f = fopen(doc, "r+b"); ftruncate(fileno(f), 100); fclose(f); }
    { fude_ink c; fude_ink_init(&c); fude_view vc = { {0,0}, 1 };
      CHECK(fude_load_document(doc, &c, &vc, &PG) == FUDE_LOAD_RECOVERED);
      CHECK(same_page(&a, &c)); CHECK(rde_file_exists(bad)); CHECK(file_size(bad) == 100); fude_ink_destroy(&c); }

    // Main missing (kill between the two renames): the backup.
    { remove(doc); fude_ink c; fude_ink_init(&c); fude_view vc = { {0,0}, 1 };
      CHECK(fude_load_document(doc, &c, &vc, &PG) == FUDE_LOAD_RECOVERED); CHECK(same_page(&a, &c)); fude_ink_destroy(&c); }

    // Garbage everywhere: CORRUPT, ink untouched.
    { FILE* f = fopen(doc, "wb"); fputs("not a kana file at all", f); fclose(f);
      f = fopen(bak, "wb"); fputs("KANA", f); fclose(f);
      fude_ink c; fude_ink_init(&c); fude_view vc = { {7,7}, 1 };
      CHECK(fude_load_document(doc, &c, &vc, &PG) == FUDE_LOAD_CORRUPT); CHECK(fude_ink_stroke_count(&c) == 0); CHECK(vc.offset.x == 7.0f);
      fude_ink_destroy(&c); }

    // Forward compatibility: a "newer" file with an unknown chunk and longer records.
    {
        u8 buf[4096]; u32 n = 0;
        #define P8(v)  (buf[n++] = (u8)(v))
        #define P32(v) do { u32 _v = (u32)(v); P8(_v); P8(_v >> 8); P8(_v >> 16); P8(_v >> 24); } while(0)
        #define PF(v)  do { f32 _f = (v); u32 _u; memcpy(&_u, &_f, 4); P32(_u); } while(0)
        P8('K'); P8('A'); P8('N'); P8('A'); P32(1); P8('D'); P8('O'); P8('C'); P8(' ');
        P8('Z'); P8('Z'); P8('Z'); P8('Z'); P32(3); P8(1); P8(2); P8(3);                           // unknown chunk
        P8('S'); P8('T'); P8('R'); P8('K'); P32(8 + 16); P32(1); P32(16);                        // record 16 > 12
        P32(2); P8(10); P8(20); P8(30); P8(255); P8(1); P8(0); P8(0); P8(0); P32(0xDEADBEEF);
        P8('P'); P8('N'); P8('T'); P8('S'); P32(8 + 2 * 24); P32(2); P32(24);                   // record 24 > 20
        PF(1); PF(2); PF(0.5f); PF(3); PF(0); PF(99);
        PF(4); PF(5); PF(0.6f); PF(3); PF(0.01f); PF(99);
        FILE* f = fopen(doc, "wb"); fwrite(buf, 1, n, f); fclose(f);
        fude_ink c; fude_ink_init(&c); fude_view vc = { {0,0}, 1 }; fude_page pc = { .paper = FUDE_PAPER_LINES };
        CHECK(fude_load_document(doc, &c, &vc, &pc) == FUDE_LOAD_OK);
        CHECK(pc.paper == FUDE_PAPER_DOTS);                                                         // from before 'PAGE': the dots
        CHECK(fude_ink_stroke_count(&c) == 1 && c.points.count == 2 && fude_ink_stroke_at(&c, 0)->from_pen && fude_ink_stroke_at(&c, 0)->color.g == 20);
        CHECK(fude_ink_stroke_points(&c, fude_ink_stroke_at(&c, 0))[1].position.x == 4.0f && fude_ink_stroke_points(&c, fude_ink_stroke_at(&c, 0))[1].radius == 3.0f);
        fude_ink_destroy(&c);
    }

    // Before themes, the default ink was saved as 30 30 36: it loads as the theme's ink.
    {
        fude_ink l; fude_ink_init(&l);
        stroke(&l, 0, 5, (rde_color){ 30, 30, 36, 255 }); stroke(&l, 50, 5, (rde_color){ 30, 30, 37, 255 });
        CHECK(fude_save_document(doc, &l, (fude_view){ {0,0}, 1 }, PG, NULL));
        fude_ink c; fude_ink_init(&c); fude_view vc = { {0,0}, 1 };
        CHECK(fude_load_document(doc, &c, &vc, &PG) == FUDE_LOAD_OK);
        CHECK(fude_theme_is_ink(fude_ink_stroke_at(&c, 0)->color));
        CHECK(!fude_theme_is_ink(fude_ink_stroke_at(&c, 1)->color) && fude_ink_stroke_at(&c, 1)->color.b == 37);
        fude_theme_set(FUDE_THEME_NIGHT);
        CHECK(fude_theme_resolve(fude_ink_stroke_at(&c, 0)->color).r == fude_theme_active()->ink.r && fude_theme_active()->ink.r > 200);
        fude_theme_set(FUDE_THEME_PAPER);
        fude_ink_destroy(&c); fude_ink_destroy(&l);
    }

    // Settings round trip, and an older (shorter) file keeps the missing fields.
    {
        fude_settings s = { .tool = 1, .vertical = false, .show_hud = true, .brush_scale = 1, .width_mode = 0,
                            .color = { 1, 2, 3, 255 }, .radius = 4.5f, .toolbar_center = { 300.0f, 222.5f }, .theme = FUDE_THEME_SAKURA, .mlkit = false };
        CHECK(fude_save_settings(set, &s));
        fude_settings t; memset(&t, 0, sizeof t);
        CHECK(fude_load_settings(set, &t) == FUDE_LOAD_OK); CHECK(fude_settings_equal(&s, &t)); CHECK(!t.mlkit);
        s.mlkit = true;                                                                              // ML Kit on: saved too
        CHECK(fude_save_settings(set, &s)); memset(&t, 0, sizeof t);
        CHECK(fude_load_settings(set, &t) == FUDE_LOAD_OK); CHECK(fude_settings_equal(&s, &t) && t.mlkit);
        s.mlkit = false;
        s.toolbar_minimized = true;                                                                  // the bar folded: saved too
        CHECK(fude_save_settings(set, &s)); memset(&t, 0, sizeof t);
        CHECK(fude_load_settings(set, &t) == FUDE_LOAD_OK); CHECK(fude_settings_equal(&s, &t) && t.toolbar_minimized);
        s.toolbar_minimized = false;
        s.paper_size = FUDE_PAPER_LARGE;                                                          // the practice squares' size: saved too
        CHECK(fude_save_settings(set, &s)); memset(&t, 0, sizeof t);
        CHECK(fude_load_settings(set, &t) == FUDE_LOAD_OK); CHECK(fude_settings_equal(&s, &t) && t.paper_size == FUDE_PAPER_LARGE);
        s.paper_size = FUDE_PAPER_MEDIUM;
        s.finger_writes = true; s.pen_ever = false;                                                   // the hand on, no pen yet: saved too
        CHECK(fude_save_settings(set, &s)); memset(&t, 0, sizeof t);
        CHECK(fude_load_settings(set, &t) == FUDE_LOAD_OK); CHECK(fude_settings_equal(&s, &t) && t.finger_writes && !t.pen_ever);
        s.finger_writes = false; s.pen_ever = true;
        CHECK(fude_save_settings(set, &s)); memset(&t, 0, sizeof t);
        CHECK(fude_load_settings(set, &t) == FUDE_LOAD_OK); CHECK(fude_settings_equal(&s, &t) && !t.finger_writes && t.pen_ever);
        s.pen_ever = false;

        u8 buf[64]; u32 n = 0;
        P8('K'); P8('A'); P8('N'); P8('A'); P32(1); P8('S'); P8('E'); P8('T'); P8('T');
        P8('P'); P8('R'); P8('E'); P8('F'); P32(2); P8(0); P8(1);                                // only tool + vertical
        FILE* f = fopen(set, "wb"); fwrite(buf, 1, n, f); fclose(f);
        fude_settings u = s;
        CHECK(fude_load_settings(set, &u) == FUDE_LOAD_OK);
        CHECK(u.tool == 0 && u.vertical == true && u.radius == 4.5f && u.color.r == 1 && u.theme == FUDE_THEME_SAKURA && !u.mlkit);
        fude_settings v = s; v.mlkit = true;                                                          // a file from before ML Kit: the switch as it was
        CHECK(fude_load_settings(set, &v) == FUDE_LOAD_OK && v.mlkit);
        fude_settings m = s; m.toolbar_minimized = true;                                              // and from before the fold: as it was
        CHECK(fude_load_settings(set, &m) == FUDE_LOAD_OK && m.toolbar_minimized);
        fude_settings q = s; q.paper_size = FUDE_PAPER_SMALL;                                     // from before the size: as it was
        CHECK(fude_load_settings(set, &q) == FUDE_LOAD_OK && q.paper_size == FUDE_PAPER_SMALL);
        fude_settings h = s; h.finger_writes = true; h.pen_ever = false;                              // from before the hand: a pen user's
        CHECK(fude_load_settings(set, &h) == FUDE_LOAD_OK && !h.finger_writes && h.pen_ever);

        // An out-of-range theme (a newer build's) keeps the current one.
        n = 0;
        P8('K'); P8('A'); P8('N'); P8('A'); P32(1); P8('S'); P8('E'); P8('T'); P8('T');
        P8('P'); P8('R'); P8('E'); P8('F'); P32(5 + 4 + 4 + 8 + 1);
        P8(0); P8(1); P8(0); P8(0); P8(0); P8(30); P8(30); P8(36); P8(255); PF(2.0f); PF(10.0f); PF(20.0f); P8(200);
        f = fopen(set, "wb"); fwrite(buf, 1, n, f); fclose(f);
        fude_settings w = s;
        CHECK(fude_load_settings(set, &w) == FUDE_LOAD_OK);
        CHECK(w.theme == FUDE_THEME_SAKURA && fude_theme_is_ink(w.color) && w.radius == 2.0f);
    }

    fude_ink_destroy(&a); fude_ink_destroy(&b);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
