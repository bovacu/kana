// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PDFVIEW
#define FUDE_ZOOM_PDFVIEW

#include "rde.h"
#include "zoom/zoom.h"
#include "drawing/doc/pdf.h"
#include "drawing/doc/doc.h"
#include "zoom/scene.h"

// ===========================================================================
// A canvas over a PDF (Sketching): its pages under the ink, written on as
// anything is drawn — a canvas made from one (Insert → PDF: the PDF the
// canvas's own, notes.h' <id>.pdf).
//
// The pages hang one under the other in the canvas's home frame, as big as
// they are (a unit a millimetre: page.h' FUDE_ZOOM_PAGE_MM_PER_UNIT), their
// middles on x 0, the first's top on y 0 (Y up: they go down). The canvas is
// as big as they are: the view kept over them, never out past them (the page
// keeps it so: fude_zoom_pdfview_bounds), and zoomed in as far as the canvas goes.
//
// Drawn from textures, as Kana's documents are (doc.h): each page whole, sharp
// enough at a glance; and, the view resting, the part of each page on screen
// again at the screen's own pixels — sharp at any zoom (the PDF is drawn
// again for what is on screen, however deep). Drawn on a thread of their own,
// one piece at a time, nearest first; a page not drawn yet is blank paper.
// In a dark theme the pages are dark too (paper the page's colour, print the
// text's).
//
// Its text (pdf.h: the PDF's own; a scan has none) is searched by Find: the
// matches marked on the pages, the one gone to outlined.
// ===========================================================================

#define FUDE_ZOOM_PDFVIEW_GAP        8.0      // between pages, millimetres
#define FUDE_ZOOM_PDFVIEW_WHOLE_PX   1100u    // a whole page's texture: its width in pixels
#define FUDE_ZOOM_PDFVIEW_SHARP_MAX  4096u    // a sharp piece's longer side, at most
#define FUDE_ZOOM_PDFVIEW_SCALE      2.0      // pixels to a screen point (iPad, Mac: 2x)
#define FUDE_ZOOM_PDFVIEW_KEEP       6u       // whole pages kept drawn, at most (the nearest)
#define FUDE_ZOOM_PDFVIEW_REST       0.15     // seconds the view keeps still before the sharp pieces

typedef struct {
    rde_vec_2F    points;   // its size in the PDF (points), as read
    fude_zoom_box box;      // on the canvas (home frame units)
    fude_doc_tile whole;
    fude_doc_tile sharp;    // what was on screen, at the screen's pixels (its zoom: screen points to a page point)
} fude_zoom_pdfview_page;

struct fude_zoom_pdfview_job;

typedef struct {
    fude_pdf*                 pdf;          // NULL: none open
    c8                        path[RDE_MAX_PATH];
    rde_arr TYPE(fude_zoom_pdfview_page) pages;
    u32                       page_count;
    fude_zoom_box             bounds;       // every page, home frame units
    struct fude_zoom_pdfview_job* job;          // the piece being drawn (NULL: none)
    b8                        dark;
    rde_color                 paper, print;
    fude_zoom_sim             seen;         // the home frame on screen, to tell the view resting
    f64                       moved_at;
    // A search (Find): its matches as they come, the one gone to.
    rde_arr TYPE(fude_pdf_match) matches;   // FUDE_PDF_MATCHES
    u32                       match_count;
    u32                       match_at;     // the one gone to (match_count: none yet)
    b8                        searching;
    b8                        search_done;
    b8                        search_go;    // go to the first as it comes
    c8                        query[128];
} fude_zoom_pdfview;

// Its pages (page_count of them while it is open).
static inline fude_zoom_pdfview_page* fude_zoom_pdfview_pages(const fude_zoom_pdfview* _v) { return (fude_zoom_pdfview_page*)_v->pages.memory; }

void fude_zoom_pdfview_init(fude_zoom_pdfview* _v);
// The PDF at _path open over the canvas (the one before let go). False: it does not open.
b8   fude_zoom_pdfview_open(fude_zoom_pdfview* _v, const c8* _path);
void fude_zoom_pdfview_close(fude_zoom_pdfview* _v);
b8   fude_zoom_pdfview_is_open(const fude_zoom_pdfview* _v);

// Once a frame: finished pieces put on the GPU, the next asked for. _home: the
// home frame on the screen (centre origin, Y up); _half: half the screen.
void fude_zoom_pdfview_update(fude_zoom_pdfview* _v, fude_zoom_sim _home, fude_zoom_v2 _half);
// The pages (and a search's marks), under the ink.
void fude_zoom_pdfview_render(const fude_zoom_pdfview* _v, fude_zoom_sim _home, fude_zoom_v2 _half);

// The canvas the pages make (home frame units): what the view is kept over.
fude_zoom_box fude_zoom_pdfview_bounds(const fude_zoom_pdfview* _v);
// The page whose box holds _at (home units), or the nearest (from 0).
u32  fude_zoom_pdfview_page_at(const fude_zoom_pdfview* _v, fude_zoom_v2 _at);
// A place on page _page (points from its top-left) on the canvas, and back.
fude_zoom_v2 fude_zoom_pdfview_to_canvas(const fude_zoom_pdfview* _v, u32 _page, rde_vec_2F _points);
rde_vec_2F   fude_zoom_pdfview_to_page(const fude_zoom_pdfview* _v, u32 _page, fude_zoom_v2 _at);

// Search: _query's matches found as the PDF is gone through (the first gone to
// as it comes: fude_zoom_pdfview_search_wanted); stopped.
void fude_zoom_pdfview_search(fude_zoom_pdfview* _v, const c8* _query);
void fude_zoom_pdfview_search_stop(fude_zoom_pdfview* _v);
// The match to go to now, when one is wanted (the first come in): its box on the
// canvas (home units). False: none wanted.
b8   fude_zoom_pdfview_search_wanted(fude_zoom_pdfview* _v, fude_zoom_box* _box);
// Match _i's box on the canvas (home units); made the one gone to.
b8   fude_zoom_pdfview_match_box(fude_zoom_pdfview* _v, u32 _i, fude_zoom_box* _box);

// The PDF again at _out with what was drawn on it (scene _s's, at any depth): each
// page copied as it is (its text stays text), and over it, in the order they were
// drawn (the markers' first, under the rest), the lines, the fills the eraser and the
// bucket left, the shapes, the texts and notes (their lines broken as _font breaks them
// on the canvas) — on white paper, the theme's ink as the light theme's. What is beside
// the pages is left out; pictures put on the canvas are not written yet. False when it
// could not be (pdf.h: no writer here).
b8   fude_zoom_pdfview_export(const fude_zoom_pdfview* _v, const fude_zoom_scene* _s, rde_font* _font, f32 _font_px, const c8* _out);

#endif
