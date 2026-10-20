// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PAGES
#define FUDE_ZOOM_PAGES

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/scene.h"

// ===========================================================================
// Pages: a canvas made a notebook (docs/product_split.md §4.1). Its pages are all one size, as many as it needs, laid
// one under the other in its home frame at their real size (one unit is a millimetre, as in a PDF: pdfview.h), their
// middles on x = 0, the first page's top on y = 0 (Y is up, so they go down), FUDE_ZOOM_PAGES_GAP apart. Each page is
// an endless canvas inside its edges, and can be zoomed into as deep as any.
//
// Where it sits: fude/zoom, over scene.h. The page keeps one fude_zoom_pages for the open canvas (page_file.c), keeps
// the view over the pages (as it does over a PDF's) and draws each page's paper with paper.h (page_render.c); the home
// list (homelist.c) reads the file to show which canvases are notebooks.
// Its main types and their lifecycle: fude_zoom_pages is a plain value with nothing to free.
// Threads: main thread only, like the scene.
// Files: <id>.pages beside the canvas: the KANA header ("KANA", u32 version, u32 kind 'PAGE'; kfile.h), then u8 on,
// f64 width, f64 height (millimetres) and u32 page count.
// Design: there is always one page more than the last one written on, so a new page is never something to ask for:
// one is added as the last one is used. Each page shows its paper (the canvas's dots, squares or lines) at its true
// spacing, with finer lines coming in between as the view goes in, so there is always something to write along.
// ===========================================================================

#define FUDE_ZOOM_PAGES_GAP    8.0      // the gap between pages, in millimetres
#define FUDE_ZOOM_PAGES_MOST   100000u  // the most pages a notebook has
#define FUDE_ZOOM_PAGES_LINES  8.0      // how far apart a notebook's ruled lines are, in millimetres
#define FUDE_ZOOM_PAGES_GRID   5.0      // how far apart squared and dotted paper's lines are, in millimetres
#define FUDE_ZOOM_PAGES_MARGIN 20.0     // ruled paper's top margin and left margin line, in millimetres

// The page sizes offered, as a page's width by its height in millimetres, upright.
typedef enum {
    FUDE_ZOOM_PAGES_A4 = 0,    // 210 × 297
    FUDE_ZOOM_PAGES_A5,        // 148 × 210
    FUDE_ZOOM_PAGES_LETTER,    // 215.9 × 279.4
    FUDE_ZOOM_PAGES_SQUARE,    // 210 × 210
    FUDE_ZOOM_PAGES_SIZES
} FUDE_ZOOM_PAGES_;

// A canvas's pages.
typedef struct {
    b8  on;        // the canvas is a notebook (otherwise it is the endless canvas)
    f64 w, h;      // a page's size, in millimetres
    u32 count;     // how many pages it has
} fude_zoom_pages;

// Sets up the endless canvas: no notebook, one A4 page ready should it become one.
void fude_zoom_pages_init(fude_zoom_pages* _p);
// Makes the canvas a notebook of pages of size _size (FUDE_ZOOM_PAGES_; an unknown one is A4), or of _w by _h
// millimetres (both must be over 0, otherwise nothing changes). It keeps as many pages as it had (one at least).
void fude_zoom_pages_size(fude_zoom_pages* _p, u8 _size);
void fude_zoom_pages_custom(fude_zoom_pages* _p, f64 _w, f64 _h);

// fude_zoom_pages_box returns page _i's box, in the home frame's units; fude_zoom_pages_bounds returns the box round
// every page; fude_zoom_pages_at returns the page _at is on, or the nearest one (counting each page with the gap
// under it).
fude_zoom_box fude_zoom_pages_box(const fude_zoom_pages* _p, u32 _i);
fude_zoom_box fude_zoom_pages_bounds(const fude_zoom_pages* _p);
u32           fude_zoom_pages_at(const fude_zoom_pages* _p, fude_zoom_v2 _at);

// Returns whether anything is drawn on page _i, in the home frame or in a frame inside it (what was drawn zoomed in).
b8 fude_zoom_pages_used(const fude_zoom_pages* _p, const fude_zoom_scene* _s, u32 _i);
// Adds pages until there is one past the last one used. Returns true when the count changed (never for the endless
// canvas).
b8 fude_zoom_pages_grow(fude_zoom_pages* _p, const fude_zoom_scene* _s);

// Returns the page object _object is on: the one the middle of its box is over (for a frame, the middle of what is
// drawn in it). Returns FUDE_ZOOM_NONE when it is on no page: not alive, not in the home frame, or a layer's record.
u32 fude_zoom_pages_of(const fude_zoom_pages* _p, const fude_zoom_scene* _s, u32 _object);

// How fude_zoom_pages_rearrange changes the pages.
typedef enum {
    FUDE_ZOOM_PAGES_INSERT = 0,
    FUDE_ZOOM_PAGES_DELETE,
    FUDE_ZOOM_PAGES_MOVE,
} FUDE_ZOOM_PAGES_DO_;
// Rearranges the pages (0.1.92). INSERT puts a blank page in before page _i (_i equal to the count puts it at the
// end); DELETE takes page _i out, letting go of what is on it (the last page left is emptied instead); MOVE makes page
// _i page _j. What is on the pages after them goes with them, and the count follows. What it changes goes into _died
// (u32: objects let go, already marked dead), _moved (u32) and _before and _after (fude_zoom_place: where they were
// and where they are now), for the caller to record as one undo step (fude_zoom_history_push_all). Returns false when
// there is no such page, or the canvas is not a notebook.
b8 fude_zoom_pages_rearrange(fude_zoom_pages* _p, fude_zoom_scene* _s, u8 _do, u32 _i, u32 _j, rde_arr* _died, rde_arr* _moved, rde_arr* _before, rde_arr* _after);

// Returns how far apart to draw paper's lines in a view. _true is the paper's own spacing (in millimetres) and _z how
// many screen points a millimetre covers. Normally the spacing is the paper's own, as a notebook's page shows it.
// Zoomed in so far that its lines would be twice _in points apart or more, it is halved again and again so they never
// are, and *_fade (0 to 1) says how far the finest have come in. Zoomed out so far that its lines are closer than
// _out points, it stays its own but fades (*_fade under 1), to nothing at a quarter of _out.
f64 fude_zoom_pages_spacing(f64 _true, f64 _z, f64 _out, f64 _in, f64* _fade);

// fude_zoom_pages_save writes <id>.pages, returning false when it could not. fude_zoom_pages_load reads it, returning
// false when there is none, it cannot be read, or the canvas is not a notebook (then _p is the endless canvas).
b8 fude_zoom_pages_save(const fude_zoom_pages* _p, const c8* _path);
b8 fude_zoom_pages_load(fude_zoom_pages* _p, const c8* _path);

#endif
