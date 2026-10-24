// The PDF worker (fude/drawing/doc/worker.h), shared by Kana's documents (doc.c) and the canvas apps' PDF view
// (zoom/pdfview.c): a piece drawn on its thread and taken back, turned the way a texture wants and darkened for a dark
// theme (a reader's picture left as drawn), a piece drawn in colours since changed thrown away (stale), or for pages
// since turned (forgotten), a piece waited for (as a PDF written out waits), a piece let go while drawing left to its
// thread to free with its PDF (orphaned), one that failed to draw handed back as failed, and the tiles put on the GPU
// (on the std allocator). Then the whole pages' plan (fude_doc_wholes): a document of many small pages in view is
// drawn once each and the worker rests (as it did not with a count of six), zoomed out over hundreds each page is
// drawn small and every one shows, the pages kept fit their budget, a still view draws nothing again, and a long page's
// picture fits a texture. The PDF is a stand-in whose drawing waits until the test lets it go on, so every state a
// piece passes through is seen; the GPU is a stand-in too.
#include "drawing/doc/worker.h"
#include "drawing/base/theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

// --- the PDF stand-in: it draws a page when the test lets it -------------------------------------------------------

struct fude_pdf { int closed; };

static rde_mutex gate      = NULL;   // guards the counts below, which the worker's threads touch too
static rde_cond  gate_cond = NULL;
static int       started   = 0;      // drawings begun
static int       allowed   = 0;      // drawings let go on
static int       finished  = 0;      // drawings through
static int       closes    = 0;      // fude_pdf_close calls

// Draws the page: its top row (10, 20, 30), the rest white; page 99 cannot be drawn. It waits for the test first.
b8 fude_pdf_render(fude_pdf* _pdf, u32 _page, rde_vec_2F _from, rde_vec_2F _size, u32 _w, u32 _h, u8* _rgba) {
    (void)_pdf; (void)_from; (void)_size;
    rde_mutex_lock(gate);
    const int _me = ++started;
    rde_cond_broadcast(gate_cond);
    while(allowed < _me) {
        rde_cond_wait(gate_cond, gate);
    }
    rde_mutex_unlock(gate);
    b8 _ok = _page != 99u;
    for(u32 _y = 0; _ok && _y < _h; _y++) {
        for(u32 _x = 0; _x < _w; _x++) {
            u8* _p = &_rgba[((usize)_y * _w + _x) * 4u];
            _p[0] = _y == 0u ? 10u : 255u; _p[1] = _y == 0u ? 20u : 255u; _p[2] = _y == 0u ? 30u : 255u; _p[3] = 255u;
        }
    }
    rde_mutex_lock(gate);
    finished++;
    rde_cond_broadcast(gate_cond);
    rde_mutex_unlock(gate);
    return _ok;
}

void fude_pdf_close(fude_pdf* _pdf) {
    rde_mutex_lock(gate);
    _pdf->closed++;
    closes++;
    rde_mutex_unlock(gate);
}

// Lets the next drawing go on.
static void let_go(void) {
    rde_mutex_lock(gate);
    allowed++;
    rde_cond_broadcast(gate_cond);
    rde_mutex_unlock(gate);
}

// Waits (5 s at most) until *_count reaches _want; false when it never does.
static b8 wait_for(const int* _count, int _want) {
    b8 _ok = true;
    rde_mutex_lock(gate);
    while(*_count < _want && _ok) {
        _ok = rde_cond_wait_timeout(gate_cond, gate, 5000u);
    }
    const b8 _there = *_count >= _want;
    rde_mutex_unlock(gate);
    return _there;
}

// Reads a count under the gate.
static int read_count(const int* _count) {
    rde_mutex_lock(gate);
    const int _n = *_count;
    rde_mutex_unlock(gate);
    return _n;
}

// A thread of the test's: lets the drawing go on after a moment, while the main thread waits for it.
static any let_go_later(rde_thread* _thread, any _data) {
    (void)_thread; (void)_data;
    rde_thread_sleep_ms(30);
    let_go();
    return NULL;
}

// Takes the worker's piece once its drawing is through (it is marked done just after): polls, 5 s at most.
static fude_doc_piece* take_when_done(fude_doc_worker* _w) {
    for(int _i = 0; _i < 5000; _i++) {
        fude_doc_piece* _p = fude_doc_worker_take(_w);
        if(_p != NULL || !fude_doc_worker_busy(_w)) {
            return _p;
        }
        rde_thread_sleep_ms(1);
    }
    return NULL;
}

// --- the GPU stand-in ----------------------------------------------------------------------------------------------

static int uploads = 0, mipmapped = 0, clamped = 0, textures = 0;
static rde_memory_allocator* texture_allocator = NULL;   // the allocator the last texture was made with
struct rde_memory_texture { u32 w, h; u8* pixels; };
rde_memory_texture* rde_memory_texture_create(u32 w, u32 h, u32 c, rde_memory_allocator* a) {
    texture_allocator = a;
    rde_memory_texture* m = calloc(1, sizeof *m); m->w = w; m->h = h; m->pixels = calloc((usize)w * h * c, 1); textures++; return m;
}
void rde_memory_texture_destroy(rde_memory_texture* m) { free(m->pixels); free(m); textures--; }
u8* rde_memory_texture_get_pixels(const rde_memory_texture* m) { return m->pixels; }
void rde_memory_texture_gpu_upload(rde_memory_texture* m, const rde_texture_parameters* p) {
    (void)m; uploads++; mipmapped += p->generate_mipmap ? 1 : 0;
    clamped += p->wrap_s == RDE_TEXTURE_PARAMETER_TYPE_WRAP_CLAMP_TO_EDGE && p->wrap_t == RDE_TEXTURE_PARAMETER_TYPE_WRAP_CLAMP_TO_EDGE ? 1 : 0;
}

// The colour of pixel (_x, _y) of a piece's pixels, as they lie in memory.
static const u8* pixel(const fude_doc_piece* _p, u32 _x, u32 _y) {
    return &((const u8*)_p->pixels.memory)[((usize)_y * _p->w + _x) * 4u];
}

// --- the whole pages' plan ----------------------------------------------------------------------------------------

// A document laid out as doc.c lays one out, in screen pixels: its pages one under the other from y 0 down, each as
// wide on screen as the others, GAP_PX apart; and each page's whole picture.
#define DOC_MOST 400u
#define GAP_PX   4.0f
static rde_vec_2F    doc_points[DOC_MOST];
static fude_doc_tile doc_whole[DOC_MOST];
static fude_doc_seen doc_seen[DOC_MOST];
static int           doc_drawn[DOC_MOST];   // each page's whole pieces drawn
static rde_arr       doc_pixels;             // a whole piece's pixels, as big as one can be

// Lets every page's picture go and forgets what was drawn.
static void doc_clear(u32 _n) {
    for(u32 _i = 0; _i < _n; _i++) {
        fude_doc_tile_free(&doc_whole[_i]);
        doc_drawn[_i] = 0;
    }
}

// Page _i's top and height on screen, in pixels from the column's top, its pages _px wide.
static void doc_place(u32 _i, f32 _px, f32* _top, f32* _h) {
    f32 _y = 0.0f;
    for(u32 _k = 0; _k < _i; _k++) {
        _y += _px * doc_points[_k].y / doc_points[_k].x + GAP_PX;
    }
    *_top = _y;
    *_h   = _px * doc_points[_i].y / doc_points[_i].x;
}

// Fills what the caller sees of each of the _n pages, as doc.c does: on screen when it reaches into the screen (_scroll
// to _scroll + _screen_h, down the column), near within a screen above or below, how far its middle is from the
// screen's, and its width on screen.
static void doc_see(u32 _n, f32 _px, f32 _scroll, f32 _screen_h) {
    const f32 _mid = _scroll + _screen_h * 0.5f;
    for(u32 _i = 0; _i < _n; _i++) {
        f32 _top, _h;
        doc_place(_i, _px, &_top, &_h);
        const b8 _on   = _top <= _scroll + _screen_h && _top + _h >= _scroll;
        const b8 _near = _top <= _scroll + 2.0f * _screen_h && _top + _h >= _scroll - _screen_h;
        doc_seen[_i] = (fude_doc_seen){ &doc_whole[_i], doc_points[_i], rde_math_abs_f64((f64)(_top + _h * 0.5f - _mid)), _px, _i,
                                        (u8)(_on ? FUDE_DOC_ON_SCREEN : _near ? FUDE_DOC_NEAR : FUDE_DOC_AWAY) };
    }
}

// Draws page _i whole at its size on screen (_px wide) straight away, as the worker would, and puts it in its tile.
static void doc_draw(u32 _i, f32 _px) {
    u32 _level, _w, _h;
    fude_doc_whole_size(doc_points[_i], _px, &_level, &_w, &_h);
    fude_doc_piece _piece = { .page = _i, .kind = FUDE_DOC_PIECE_WHOLE, .size = doc_points[_i], .w = _w, .h = _h, .zoom = (f32)_level, .pixels = doc_pixels, .ok = true };
    fude_doc_tile_take(&doc_whole[_i], &_piece);
    doc_drawn[_i]++;
}

// The bytes of the pictures kept, those of pages on screen apart (*_screen).
static u64 doc_kept(u32 _n, f32 _px, f32 _scroll, f32 _screen_h, u64* _screen) {
    u64 _all = 0;
    *_screen = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        f32 _top, _h;
        doc_place(_i, _px, &_top, &_h);
        const u64 _b = doc_whole[_i].texture != NULL ? (u64)doc_whole[_i].w * doc_whole[_i].h * 4u : 0u;
        _all += _b;
        *_screen += _top <= _scroll + _screen_h && _top + _h >= _scroll ? _b : 0u;
    }
    return _all;
}

// Runs _frames frames of a still view, as doc.c's update does: the plan (fude_doc_wholes, which lets go of what is not
// kept), then a page on screen drawn, else a near one, else the reader's turn (fude_doc_read_next), counted in
// *_reading. Checks every frame that no page drawn in this view has been let go. Returns the frame the worker first
// rested (nothing to draw), or -1.
static int doc_run(u32 _n, f32 _px, f32 _scroll, f32 _screen_h, int _frames, int* _reading) {
    int _rested = -1;
    *_reading = 0;
    b8 _drawn_here[DOC_MOST] = { 0 };
    for(int _f = 0; _f < _frames; _f++) {
        doc_see(_n, _px, _scroll, _screen_h);
        u32 _screen, _near;
        fude_doc_wholes(doc_seen, _n, &_screen, &_near);
        for(u32 _i = 0; _i < _n; _i++) {
            CHECK(!_drawn_here[_i] || doc_whole[_i].texture != NULL);   // (never drawn and then let go at once)
        }
        const u32 _next = _screen != UINT32_MAX ? _screen : _near;
        if(_next != UINT32_MAX) {
            doc_draw(_next, _px);
            _drawn_here[_next] = true;
        } else {
            _rested = _rested < 0 ? _f : _rested;
            (*_reading)++;
        }
    }
    return _rested;
}

// The old way, kept here as the reference it is measured against: the pages on screen drawn whole nearest the middle
// first, then the first near page not drawn, and the whole pages beyond the six nearest the middle let go each frame.
// Returns how many frames, of _frames, left the worker nothing to draw (the reader's turns).
static int doc_run_old(u32 _n, f32 _px, f32 _scroll, f32 _screen_h, int _frames) {
    int _reading = 0;
    for(int _f = 0; _f < _frames; _f++) {
        doc_see(_n, _px, _scroll, _screen_h);
        i32 _best = -1;
        for(u32 _i = 0; _i < _n; _i++) {
            if(doc_whole[_i].texture == NULL && doc_seen[_i].where == FUDE_DOC_ON_SCREEN && (_best < 0 || doc_seen[_i].far < doc_seen[_best].far)) {
                _best = (i32)_i;
            }
        }
        for(u32 _i = 0; _best < 0 && _i < _n; _i++) {
            _best = doc_whole[_i].texture == NULL && doc_seen[_i].where == FUDE_DOC_NEAR ? (i32)_i : -1;
        }
        if(_best >= 0) {
            doc_draw((u32)_best, _px);
        } else {
            _reading++;
        }
        for(u32 _kept = 7; _kept > 6u;) {
            _kept = 0;
            i32 _farthest = -1;
            for(u32 _i = 0; _i < _n; _i++) {
                if(doc_whole[_i].texture != NULL) {
                    _kept++;
                    _farthest = _farthest < 0 || doc_seen[_i].far > doc_seen[_farthest].far ? (i32)_i : _farthest;
                }
            }
            if(_kept > 6u) {
                fude_doc_tile_free(&doc_whole[_farthest]);
            }
        }
    }
    return _reading;
}

// A whole page's size: at about its size on screen (a level of FUDE_DOC_WHOLE_PX halved, from once to twice as wide),
// as tall as its shape (what the old fude_doc_worker_whole_h gave, at full width), and a long page shrunk to fit a
// texture.
static void test_whole_size(void) {
    u32 level, w, h;
    fude_doc_whole_size((rde_vec_2F){ 595.0f, 842.0f }, 1600.0f, &level, &w, &h);
    CHECK(level == 1100u && w == 1100u && h == (u32)(1100.0f * 842.0f / 595.0f));   // (seen bigger: the most)
    fude_doc_whole_size((rde_vec_2F){ 595.0f, 842.0f }, 780.0f, &level, &w, &h);
    CHECK(level == 1100u && w == 1100u);   // (a phone's reader: the page as it always was)
    fude_doc_whole_size((rde_vec_2F){ 595.0f, 842.0f }, 550.0f, &level, &w, &h);
    CHECK(level == 550u && w == 550u && h == (u32)(550.0f * 842.0f / 595.0f));
    fude_doc_whole_size((rde_vec_2F){ 595.0f, 842.0f }, 40.0f, &level, &w, &h);
    CHECK(level == 68u && w == 68u);
    fude_doc_whole_size((rde_vec_2F){ 595.0f, 842.0f }, 0.0f, &level, &w, &h);
    CHECK(level == FUDE_DOC_WHOLE_LEAST && w == FUDE_DOC_WHOLE_LEAST);
    fude_doc_whole_size((rde_vec_2F){ 0.25f, 2.0f }, 2000.0f, &level, &w, &h);
    CHECK(w == 1100u && h == 2200u);   // (narrower than a point: a point wide)
    fude_doc_whole_size((rde_vec_2F){ 1000.0f, 0.0f }, 2000.0f, &level, &w, &h);
    CHECK(w == 1100u && h == 1u);
    // A receipt eight times as long as it is wide (1100 x 8800 at full width): drawn whole, the GPU's largest texture
    // (FUDE_DOC_SHARP_MAX, rde_texture_get_max_size: 16384 here, with no GPU) taking it (before: shrunk to a fixed 4096).
    fude_doc_whole_size((rde_vec_2F){ 200.0f, 1600.0f }, 1600.0f, &level, &w, &h);
    CHECK(FUDE_DOC_SHARP_MAX == rde_texture_get_max_size());
    CHECK(level == 1100u && w == 1100u && h == 8800u);
    // One twenty times as long (1100 x 22000, past any GPU's limit): shrunk to FUDE_DOC_SHARP_MAX tall, its level kept,
    // and in a still view never drawn again for being narrower than it.
    fude_doc_whole_size((rde_vec_2F){ 100.0f, 2000.0f }, 2000.0f, &level, &w, &h);
    CHECK(level == 1100u && h == FUDE_DOC_SHARP_MAX && w == (u32)(1100.0f * (f32)FUDE_DOC_SHARP_MAX / 22000.0f));
    doc_points[0] = (rde_vec_2F){ 100.0f, 2000.0f };
    int reading = 0;
    CHECK(doc_run(1u, 2000.0f, 0.0f, 2000.0f, 10, &reading) == 1 && doc_drawn[0] == 1 && doc_whole[0].h == FUDE_DOC_SHARP_MAX);
    doc_clear(1u);
}

// Many small pages in the band (slides on a phone held upright: about 11 in the screen and a screen above and below,
// so more than six): the old way drew the farthest, let it go, drew it again, and never rested, so the reader never had
// its turn. Now each is drawn once, the worker rests, and reading runs.
static void test_wholes_small_pages(void) {
    const u32 n = 40u;
    for(u32 i = 0; i < n; i++) {
        doc_points[i] = (rde_vec_2F){ 595.0f, 335.0f };
    }
    const f32 px = 780.0f, scroll = 3000.0f, screen_h = 1688.0f;
    int reading = 0;
    const int rested = doc_run(n, px, scroll, screen_h, 300, &reading);
    int band = 0, once = 0, more = 0;
    doc_see(n, px, scroll, screen_h);
    for(u32 i = 0; i < n; i++) {
        band += doc_seen[i].where != FUDE_DOC_AWAY ? 1 : 0;
        once += doc_seen[i].where != FUDE_DOC_AWAY && doc_drawn[i] == 1 && doc_whole[i].texture != NULL ? 1 : 0;
        more += doc_drawn[i] > 1 ? 1 : 0;
    }
    CHECK(band > 6 && once == band && more == 0);   // (every page in the band drawn, once)
    CHECK(rested == band && reading == 300 - band);  // (then the worker rests, and the reader has every frame)
    u64 on_screen = 0;
    CHECK(doc_kept(n, px, scroll, screen_h, &on_screen) <= FUDE_DOC_KEEP_BYTES);
    printf("small pages: %d in the band, each drawn once, rested after %d frames\n", band, rested);
    doc_clear(n);
    const int old_reading = doc_run_old(n, px, scroll, screen_h, 300);
    int old_draws = 0;
    for(u32 i = 0; i < n; i++) {
        old_draws += doc_drawn[i];
    }
    CHECK(old_reading == 0 && old_draws == 300);   // (the old way: a piece every frame, the reader never)
    printf("small pages, the old way: %d pieces drawn in 300 frames, the reader's turns %d\n", old_draws, old_reading);
    doc_clear(n);
}

// Zoomed out over hundreds of pages (a canvas's PDF, each A4 page 40 pixels wide on a tall screen): every page on
// screen is drawn, small (68 pixels wide: a few KB), once; the old way showed six and left the rest blank. A small
// zoom draws nothing again; zoomed in past 15% of their level, the pages on screen are drawn again, sharper; zoomed out
// until the level is over 2.3 times their width, smaller. A page under FUDE_DOC_SEEN_LEAST pixels is left as paper.
static void test_wholes_zoomed_out(void) {
    const u32 n = 300u;
    for(u32 i = 0; i < n; i++) {
        doc_points[i] = (rde_vec_2F){ 595.0f, 842.0f };
    }
    const f32 scroll = 2000.0f, screen_h = 2732.0f;
    int reading = 0;
    const int rested = doc_run(n, 40.0f, scroll, screen_h, 400, &reading);
    int on = 0, shown = 0, more = 0;
    doc_see(n, 40.0f, scroll, screen_h);
    for(u32 i = 0; i < n; i++) {
        on    += doc_seen[i].where == FUDE_DOC_ON_SCREEN ? 1 : 0;
        shown += doc_seen[i].where == FUDE_DOC_ON_SCREEN && doc_whole[i].texture != NULL && doc_whole[i].w == 68u ? 1 : 0;
        more  += doc_drawn[i] > 1 ? 1 : 0;
    }
    u64 on_screen = 0;
    const u64 kept = doc_kept(n, 40.0f, scroll, screen_h, &on_screen);
    CHECK(on > 40 && shown == on && more == 0 && rested > 0 && kept <= FUDE_DOC_KEEP_BYTES);
    printf("zoomed out: %d pages on screen, all shown, %.1f MB kept, rested after %d frames\n", on, (f64)kept / 1048576.0, rested);
    // A little closer (10%): nothing drawn again.
    int before = 0, after = 0;
    for(u32 i = 0; i < n; i++) {
        before += doc_drawn[i];
    }
    CHECK(doc_run(n, 44.0f, scroll, screen_h, 50, &reading) == 0);
    // Closer still, past 15% of the level (80 pixels on 68): drawn again, at 137, each once.
    doc_run(n, 80.0f, scroll, screen_h, 400, &reading);
    doc_see(n, 80.0f, scroll, screen_h);
    int sharper = 0, on80 = 0;
    for(u32 i = 0; i < n; i++) {
        after   += doc_drawn[i];
        on80    += doc_seen[i].where == FUDE_DOC_ON_SCREEN ? 1 : 0;
        sharper += doc_seen[i].where == FUDE_DOC_ON_SCREEN && doc_whole[i].w == 137u ? 1 : 0;
    }
    CHECK(after > before && sharper == on80);
    // Out to 30 pixels, under 1/2.3 of 137: those drawn at 137 are drawn again, smaller (34); one still kept at 68 from
    // before suits 30 too (68 is under 2.3 times 30), so it stays.
    doc_run(n, 30.0f, scroll, screen_h, 600, &reading);
    doc_see(n, 30.0f, scroll, screen_h);
    int smaller = 0, on30 = 0, redrawn = 0;
    for(u32 i = 0; i < n; i++) {
        on30    += doc_seen[i].where == FUDE_DOC_ON_SCREEN ? 1 : 0;
        smaller += doc_seen[i].where == FUDE_DOC_ON_SCREEN && (doc_whole[i].w == 34u || doc_whole[i].w == 68u) ? 1 : 0;
        redrawn += doc_seen[i].where == FUDE_DOC_ON_SCREEN && doc_whole[i].w == 34u ? 1 : 0;
    }
    CHECK(smaller == on30 && redrawn > 0);
    doc_clear(n);
    // The old way, at 40 pixels: six pages kept, so most of those on screen stayed blank.
    doc_run_old(n, 40.0f, scroll, screen_h, 400);
    doc_see(n, 40.0f, scroll, screen_h);
    int old_shown = 0;
    for(u32 i = 0; i < n; i++) {
        old_shown += doc_seen[i].where == FUDE_DOC_ON_SCREEN && doc_whole[i].texture != NULL ? 1 : 0;
    }
    CHECK(old_shown <= 6);
    printf("zoomed out, the old way: %d of %d pages on screen shown\n", old_shown, on);
    doc_clear(n);
    // Pages too small to tell apart (5 pixels wide): nothing drawn, and the worker rests at once.
    CHECK(doc_run(n, 5.0f, 0.0f, screen_h, 5, &reading) == 0 && reading == 5);
    for(u32 i = 0; i < n; i++) {
        CHECK(doc_drawn[i] == 0);
    }
}

// The budget: A4 pages at full width (6.8 MB each) on a tall screen, ten in the band, more than FUDE_DOC_KEEP_BYTES
// holds. Those on screen are all drawn; the near ones nearest first while they fit; none past it is drawn (so none is
// drawn and let go), and the worker rests. Scrolled on, a page gone out of the band is kept while there is room, the
// farthest let go first, and the pages kept never pass the budget.
static void test_wholes_budget(void) {
    const u32 n = 30u;
    for(u32 i = 0; i < n; i++) {
        doc_points[i] = (rde_vec_2F){ 595.0f, 842.0f };
    }
    const f32 px = 1300.0f, screen_h = 6000.0f;
    f32 scroll = 9000.0f;
    int reading = 0;
    const int rested = doc_run(n, px, scroll, screen_h, 100, &reading);
    doc_see(n, px, scroll, screen_h);
    int on = 0, on_drawn = 0, near = 0, near_drawn = 0, more = 0;
    for(u32 i = 0; i < n; i++) {
        on         += doc_seen[i].where == FUDE_DOC_ON_SCREEN ? 1 : 0;
        on_drawn   += doc_seen[i].where == FUDE_DOC_ON_SCREEN && doc_whole[i].texture != NULL ? 1 : 0;
        near       += doc_seen[i].where == FUDE_DOC_NEAR ? 1 : 0;
        near_drawn += doc_seen[i].where == FUDE_DOC_NEAR && doc_whole[i].texture != NULL ? 1 : 0;
        more       += doc_drawn[i] > 1 ? 1 : 0;
    }
    const u64 page = (u64)1100u * (u64)(u32)(1100.0f * 842.0f / 595.0f) * 4u;
    u64 on_screen = 0;
    const u64 kept = doc_kept(n, px, scroll, screen_h, &on_screen);
    CHECK(on_drawn == on && near_drawn < near && near_drawn > 0 && more == 0 && rested > 0);
    CHECK(kept <= FUDE_DOC_KEEP_BYTES && kept + page > FUDE_DOC_KEEP_BYTES);   // (as many as fit, and no more)
    printf("budget: %d on screen, %d of %d near drawn, %.1f MB kept\n", on, near_drawn, near, (f64)kept / 1048576.0);
    // Scrolled down a screen: the pages drawn there stay within the budget, the farthest above let go first.
    scroll += screen_h;
    doc_run(n, px, scroll, screen_h, 100, &reading);
    CHECK(doc_kept(n, px, scroll, screen_h, &on_screen) <= FUDE_DOC_KEEP_BYTES);
    doc_see(n, px, scroll, screen_h);
    for(u32 i = 0; i < n; i++) {
        CHECK(doc_seen[i].where != FUDE_DOC_ON_SCREEN || doc_whole[i].texture != NULL);
    }
    doc_clear(n);
}

int main(void) {
    gate      = rde_mutex_create();
    gate_cond = rde_cond_create();
    doc_pixels = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&doc_pixels, (usize)FUDE_DOC_WHOLE_PX * FUDE_DOC_SHARP_MAX * 4u);
    struct fude_pdf pdf = { 0 };
    const rde_vec_2F a4 = { 595.0f, 842.0f };

    // A light theme: white paper; the first look at the colours takes them.
    fude_theme_set(FUDE_THEME_PAPER);
    fude_doc_worker w;
    fude_doc_worker_init(&w);
    CHECK(!fude_doc_worker_busy(&w) && fude_doc_worker_take(&w) == NULL);
    CHECK(fude_doc_worker_colours(&w) && !w.dark && w.paper.r == 255u && w.paper.g == 255u && w.paper.b == 255u);
    CHECK(!fude_doc_worker_colours(&w));   // (nothing changed)

    // A whole page: drawn on its thread, busy until taken; as drawn, the top row first (as RDE's textures hold rows).
    fude_doc_worker_start(&w, &pdf, 0u, FUDE_DOC_PIECE_WHOLE, (rde_vec_2F){ 0, 0 }, a4, 4u, 3u, 0.0f);
    CHECK(fude_doc_worker_busy(&w));
    CHECK(wait_for(&started, 1));
    CHECK(fude_doc_worker_take(&w) == NULL && fude_doc_worker_busy(&w));   // (still drawing: nothing to take)
    let_go();
    fude_doc_piece* p = take_when_done(&w);
    CHECK(p != NULL && !fude_doc_worker_busy(&w));
    if(p != NULL) {
        CHECK(p->ok && p->page == 0u && p->kind == FUDE_DOC_PIECE_WHOLE && p->w == 4u && p->h == 3u);
        CHECK(pixel(p, 0, 0)[0] == 10u && pixel(p, 3, 0)[2] == 30u && pixel(p, 0, 2)[0] == 255u);   // top row first
        // Onto the GPU as a whole page's tile: clamped, mipmapped, its part kept.
        fude_doc_tile tile = { 0 };
        fude_doc_tile_take(&tile, p);
        CHECK(tile.texture != NULL && uploads == 1 && mipmapped == 1 && clamped == 1 && tile.size.x == 595.0f && tile.size.y == 842.0f);
        CHECK(tile.w == 4u && tile.h == 3u && texture_allocator == rde_memory_allocator_get_default_std());   // (not the engine's pool)
        CHECK(memcmp(rde_memory_texture_get_pixels(tile.texture), p->pixels.memory, 4u * 3u * 4u) == 0);
        fude_doc_tile_take(&tile, p);   // (taken again: the old texture let go)
        CHECK(textures == 1);
        fude_doc_tile_free(&tile);
        CHECK(textures == 0 && tile.texture == NULL);
        fude_doc_piece_free(p);
    }
    CHECK(read_count(&closes) == 0);   // (a piece handed back never closes its PDF)

    // A sharp piece: a tile not mipmapped, its zoom kept.
    fude_doc_worker_start(&w, &pdf, 1u, FUDE_DOC_PIECE_SHARP, (rde_vec_2F){ 10, 20 }, (rde_vec_2F){ 30, 40 }, 2u, 2u, 3.5f);
    let_go();
    p = take_when_done(&w);
    CHECK(p != NULL);
    if(p != NULL) {
        fude_doc_tile tile = { 0 };
        const int uploads_before = uploads, mipmapped_before = mipmapped;   // (the whole page above was taken twice)
        fude_doc_tile_take(&tile, p);
        CHECK(uploads == uploads_before + 1 && mipmapped == mipmapped_before && tile.zoom == 3.5f && tile.from.x == 10.0f && tile.from.y == 20.0f);
        fude_doc_tile_free(&tile);
        fude_doc_piece_free(p);
    }

    // A reader's picture stays as drawn: its top row first, its own colours, whatever the theme.
    fude_theme_set(FUDE_THEME_NIGHT);
    CHECK(fude_doc_worker_colours(&w) && w.dark);
    fude_doc_worker_start(&w, &pdf, 2u, FUDE_DOC_PIECE_READ, (rde_vec_2F){ 0, 0 }, a4, 3u, 2u, 0.0f);
    let_go();
    p = take_when_done(&w);
    CHECK(p != NULL);
    if(p != NULL) {
        CHECK(p->ok && pixel(p, 0, 0)[0] == 10u && pixel(p, 2, 1)[0] == 255u && pixel(p, 2, 1)[1] == 255u);
        fude_doc_piece_free(p);
    }

    // A piece to show in a dark theme: white paper becomes the theme's paper, and the pieces carry its colours.
    const rde_color paper = w.paper;
    CHECK(paper.r < 128u);
    fude_doc_worker_start(&w, &pdf, 3u, FUDE_DOC_PIECE_WHOLE, (rde_vec_2F){ 0, 0 }, a4, 2u, 2u, 0.0f);
    let_go();
    p = take_when_done(&w);
    CHECK(p != NULL);
    if(p != NULL) {
        CHECK(p->dark && pixel(p, 0, 1)[0] == paper.r && pixel(p, 0, 1)[1] == paper.g && pixel(p, 0, 1)[2] == paper.b && pixel(p, 0, 1)[3] == 255u);
        fude_doc_piece_free(p);
    }

    // Forgotten: a page turns while a reader's picture is drawn. The picture shows the page the old way round, so it is
    // thrown away when through (doc.c then reads the page again, turned), and the piece stays the worker's meanwhile.
    fude_doc_worker_forget(&w);
    CHECK(!w.stale);   // (nothing being drawn: nothing to forget)
    fude_doc_worker_start(&w, &pdf, 7u, FUDE_DOC_PIECE_READ, (rde_vec_2F){ 0, 0 }, a4, 3u, 2u, 0.0f);
    fude_doc_worker_forget(&w);
    CHECK(w.stale && fude_doc_worker_busy(&w));
    let_go();
    CHECK(take_when_done(&w) == NULL && !fude_doc_worker_busy(&w) && !w.stale);

    // Waited for: a PDF written out waits until the piece being drawn is through (here let go a moment later, by a
    // thread of the test's), and the piece is then there to take at once.
    fude_doc_worker_wait(&w);   // (nothing being drawn: back at once)
    const int through = read_count(&finished);
    fude_doc_worker_start(&w, &pdf, 8u, FUDE_DOC_PIECE_WHOLE, (rde_vec_2F){ 0, 0 }, a4, 2u, 2u, 0.0f);
    rde_thread_run_detached(let_go_later, NULL, NULL);
    fude_doc_worker_wait(&w);
    CHECK(read_count(&finished) == through + 1);
    p = fude_doc_worker_take(&w);
    CHECK(p != NULL && p->ok && p->page == 8u && !fude_doc_worker_busy(&w));
    if(p != NULL) {
        fude_doc_piece_free(p);
    }

    // Stale: the colours change while a piece is drawn. It stays the worker's (busy: no second piece on the PDF), and
    // what it drew is thrown away when it is through.
    fude_doc_worker_start(&w, &pdf, 4u, FUDE_DOC_PIECE_WHOLE, (rde_vec_2F){ 0, 0 }, a4, 2u, 2u, 0.0f);
    fude_theme_set(FUDE_THEME_PAPER);
    CHECK(fude_doc_worker_colours(&w) && w.stale && fude_doc_worker_busy(&w));
    let_go();
    CHECK(take_when_done(&w) == NULL && !fude_doc_worker_busy(&w) && !w.stale);
    CHECK(read_count(&closes) == 0);

    // Failed: a page that cannot be drawn comes back with ok false (the caller drops it, and doc.c marks a picture to
    // read failed), never turned or darkened.
    fude_doc_worker_start(&w, &pdf, 99u, FUDE_DOC_PIECE_WHOLE, (rde_vec_2F){ 0, 0 }, a4, 2u, 2u, 0.0f);
    let_go();
    p = take_when_done(&w);
    CHECK(p != NULL && !p->ok);
    if(p != NULL) {
        fude_doc_piece_free(p);
    }

    // Orphaned: the PDF let go while a piece is drawn. The worker keeps the piece and the PDF: it frees the one and
    // closes the other once through, so the PDF is never closed under the drawing; the caller forgets the PDF.
    const int began = read_count(&started);
    fude_doc_worker_start(&w, &pdf, 5u, FUDE_DOC_PIECE_WHOLE, (rde_vec_2F){ 0, 0 }, a4, 2u, 2u, 0.0f);
    CHECK(wait_for(&started, began + 1));   // (drawing now, held there)
    CHECK(fude_doc_worker_drop(&w));
    CHECK(!fude_doc_worker_busy(&w) && read_count(&closes) == 0);   // (not closed while it draws)
    let_go();
    CHECK(wait_for(&closes, 1) && pdf.closed == 1);
    CHECK(fude_doc_worker_take(&w) == NULL);

    // Let go once drawn: freed there, and the PDF left to the caller. Whichever the worker had reached, the PDF is
    // closed once, by one of them.
    struct fude_pdf pdf2 = { 0 };
    fude_doc_worker_start(&w, &pdf2, 6u, FUDE_DOC_PIECE_WHOLE, (rde_vec_2F){ 0, 0 }, a4, 2u, 2u, 0.0f);
    const int done_before = read_count(&finished);
    let_go();
    CHECK(wait_for(&finished, done_before + 1));
    rde_thread_sleep_ms(50);   // (done is marked just after the drawing returns)
    const b8 orphaned = fude_doc_worker_drop(&w);
    if(!orphaned) {
        fude_pdf_close(&pdf2);   // (the caller's to close)
    }
    CHECK(wait_for(&closes, 2) && pdf2.closed == 1);
    rde_thread_sleep_ms(50);
    CHECK(pdf2.closed == 1 && read_count(&closes) == 2);

    // Nothing to let go: the caller closes its PDF.
    CHECK(!fude_doc_worker_drop(&w));

    test_whole_size();
    test_wholes_small_pages();
    test_wholes_zoomed_out();
    test_wholes_budget();

    if(fails == 0) {
        printf("ALL PASSED\n");
        return 0;
    }
    printf("%d FAILED\n", fails);
    return 1;
}
