// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PAGES
#define FUDE_ZOOM_PAGES

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/scene.h"

// ===========================================================================
// PAGES: a canvas made a notebook (docs/product_split.md §4.1). Pages of one
// size, as many as it needs, one under the other in its home frame at their
// real size — a unit a millimetre, as a PDF's (pdfview.h) —, their middles on
// x 0, the first's top on y 0 (Y up: they go down), FUDE_ZOOM_PAGES_GAP apart.
// Each is an endless canvas inside its edges: zoomed into as deep as any.
//
// There is always one page more than the last one written on (one is added
// as the last one is used: never one to ask for), and the view is kept over
// them (page.c, as a PDF's). Each page's paper (the canvas's dots, squares or
// lines) at its true spacing — finer lines coming in between as the view goes
// in, so there is always something to write along. Kept beside the canvas
// (<id>.pages).
// ===========================================================================

#define FUDE_ZOOM_PAGES_GAP    8.0      // between pages, millimetres
#define FUDE_ZOOM_PAGES_MOST   100000u  // pages, at most
#define FUDE_ZOOM_PAGES_LINES  8.0      // ruled paper's lines apart, millimetres (a notebook's)
#define FUDE_ZOOM_PAGES_GRID   5.0      // squared and dotted paper's, millimetres
#define FUDE_ZOOM_PAGES_MARGIN 20.0     // ruled paper's top margin and left margin line, millimetres

// The sizes offered (a page's width by its height, upright).
typedef enum {
    FUDE_ZOOM_PAGES_A4 = 0,    // 210 × 297
    FUDE_ZOOM_PAGES_A5,        // 148 × 210
    FUDE_ZOOM_PAGES_LETTER,    // 215.9 × 279.4
    FUDE_ZOOM_PAGES_SQUARE,    // 210 × 210
    FUDE_ZOOM_PAGES_SIZES
} FUDE_ZOOM_PAGES_;

typedef struct {
    b8  on;        // a notebook (else: the endless canvas)
    f64 w, h;      // a page's size, millimetres
    u32 count;     // its pages
} fude_zoom_pages;

void fude_zoom_pages_init(fude_zoom_pages* _p);
// Made a notebook of pages of size _size (FUDE_ZOOM_PAGES_), or of _w by _h millimetres (both over 0); its pages as many as
// they were (one at least).
void fude_zoom_pages_size(fude_zoom_pages* _p, u8 _size);
void fude_zoom_pages_custom(fude_zoom_pages* _p, f64 _w, f64 _h);

// Page _i's box (home frame units); every page's; the page _at is on, or the nearest.
fude_zoom_box fude_zoom_pages_box(const fude_zoom_pages* _p, u32 _i);
fude_zoom_box fude_zoom_pages_bounds(const fude_zoom_pages* _p);
u32           fude_zoom_pages_at(const fude_zoom_pages* _p, fude_zoom_v2 _at);

// Is anything drawn on page _i (in the home frame, or in a frame in it: what was drawn zoomed in)?
b8 fude_zoom_pages_used(const fude_zoom_pages* _p, const fude_zoom_scene* _s, u32 _i);
// As many pages as it needs: one past the last one used. True: it changed.
b8 fude_zoom_pages_grow(fude_zoom_pages* _p, const fude_zoom_scene* _s);

// Paper's lines apart for a view: _true the paper's own (millimetres), _z screen points to a millimetre. The spacing to
// draw: its own as a notebook's page shows it — halved again and again once the view goes in so far that its lines are
// _in points apart twice over, so they are never further apart than that (*_fade: how far the finest have come in,
// 0 .. 1) —; further out, closer than _out points: its own, faded (*_fade under 1), none at all at a quarter of _out.
f64 fude_zoom_pages_spacing(f64 _true, f64 _z, f64 _out, f64 _in, f64* _fade);

// <id>.pages: written (false: it could not be), read (false: none, or not one — the endless canvas).
b8 fude_zoom_pages_save(const fude_zoom_pages* _p, const c8* _path);
b8 fude_zoom_pages_load(fude_zoom_pages* _p, const c8* _path);

#endif
