// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PDFVIEW
#define FUDE_ZOOM_PDFVIEW

#include "rde.h"
#include "zoom/zoom.h"
#include "drawing/doc/pdf.h"
#include "drawing/doc/doc.h"
#include "zoom/scene.h"

// ===========================================================================
// PDF view: a canvas over a PDF (Insert → PDF). The PDF's pages lie under the ink and are written on like
// anything else drawn; the PDF is the canvas's own (notes.h: <id>.pdf).
//
// Where it sits: zoom, the canvas apps' layer, over drawing/doc's pdf.h (which opens, draws, searches and writes PDFs)
// and doc.h's tiles. The page owns one fude_zoom_pdfview (page.h), updates it and keeps the view over its
// pages (page_frame.c), draws it each frame (page_render.c), and runs Find (page_find.c) and Export (page_export.c)
// through it.
// Its main types and their lifecycle: the page makes its fude_zoom_pdfview with fude_zoom_pdfview_init, opens a PDF in
// it with fude_zoom_pdfview_open, and lets it go with fude_zoom_pdfview_close (opening another closes the one before).
// Threads: every function is called on the main thread. The pages' pictures are drawn by a PDF worker of the view's own
// (drawing/doc/worker.h, the module doc.h's documents draw with too: RDE's detached threads, one started for each
// piece), one piece at a time, nearest the view first; the view looks each frame whether the piece is done. Nothing
// waits for a piece (a page not drawn yet shows as blank paper) but fude_zoom_pdfview_export, which draws the PDF's
// pages itself and so waits for the piece being drawn, if any. A PDF let go while a piece is being drawn leaves that
// piece to the worker to finish and free, closing the PDF with it.
// Files: reads the PDF; fude_zoom_pdfview_export writes a new PDF.
//
// Layout: the pages hang one under the other in the canvas's home frame, at their own size (a unit is a millimetre:
// page.h's FUDE_ZOOM_PAGE_MM_PER_UNIT), their middles on x = 0 and the first page's top on y = 0 (Y points up, so the
// pages go down). The canvas is as big as the pages: page_frame.c keeps the view over them and never out past them
// (fude_zoom_pdfview_bounds), and it zooms in as far as any canvas goes.
// Drawing: from textures, as doc.h's documents are drawn. Each page has a whole picture, sharp enough at a
// glance; once the view rests, the part of each page on screen is drawn again at the screen's own pixels, so it is
// sharp at any zoom however deep (the PDF is drawn again for what is on screen). In a dark theme the pages are dark
// too: the paper in the page's colour, the print in the text's.
// Search: Find searches the PDF's own text (pdf.h; a scan has none). The matches are marked on the pages and the one
// gone to is outlined.
// Design: see docs/infinite_canvas_design.md §19.
// ===========================================================================

#define FUDE_ZOOM_PDFVIEW_GAP        8.0      // The gap between pages, in millimetres.
// A whole page's picture's width, a sharp piece's most, the pixels to a point, how much of the whole pages is kept and
// how long the view rests before the sharp pieces are drawn are the PDF worker's (worker.h: FUDE_DOC_WHOLE_PX,
// FUDE_DOC_SHARP_MAX, FUDE_DOC_SCALE, FUDE_DOC_KEEP_BYTES, FUDE_DOC_REST), as doc.h's documents have them. So is the
// choice of the whole pages to keep and draw (fude_doc_wholes).

// One of the PDF's pages on the canvas, with its pictures.
typedef struct {
    rde_vec_2F    points;   // Its size in the PDF, in points, as read.
    fude_zoom_box box;      // Where it lies on the canvas, in home frame units.
    fude_doc_tile whole;
    // Its tile's zoom says how many screen points to a page point it was drawn for.
    fude_doc_tile sharp;    // What was on screen, drawn at the screen's pixels.
} fude_zoom_pdfview_page;

// The PDF under a canvas, its pages' pictures and a search in it.
typedef struct {
    fude_pdf*                 pdf;          // NULL when none is open.
    c8                        path[RDE_MAX_PATH];
    rde_arr TYPE(fude_zoom_pdfview_page) pages;
    u32                       page_count;
    fude_zoom_box             bounds;       // Every page together, in home frame units.
    // Draws the pages' pieces, one at a time (as pdf.h asks), in the theme's colours (its paper is a blank page's).
    fude_doc_worker           worker;
    // Each page as seen from the view, filled each frame for the worker to choose the whole pages to keep and draw
    // (worker.h: fude_doc_wholes, which reorders it). Made with pages, page_count of them.
    rde_arr TYPE(fude_doc_seen) pages_seen;
    fude_zoom_sim             seen;         // The home frame on screen last frame, to tell when the view rests.
    f64                       moved_at;
    // A search (Find): its matches as they come in, and the one gone to.
    rde_arr TYPE(fude_pdf_match) matches;   // Room for FUDE_PDF_MATCHES.
    u32                       match_count;
    u32                       match_at;     // The match gone to (0 when a search starts: the first one).
    b8                        searching;
    b8                        search_done;
    b8                        search_go;    // Fly to the first match as soon as it comes in.
    c8                        query[128];
} fude_zoom_pdfview;

// Returns its pages (page_count of them while a PDF is open). The pointer is valid until the PDF is closed.
static inline fude_zoom_pdfview_page* fude_zoom_pdfview_pages(const fude_zoom_pdfview* _v) { return (fude_zoom_pdfview_page*)_v->pages.memory; }

// Clears _v, with no PDF open, and sets up its worker (worker.h: the first worker makes the mutex they all share).
void fude_zoom_pdfview_init(fude_zoom_pdfview* _v);
// Opens the PDF at _path over the canvas, closing the one before. Returns false when it does not open or has no pages.
b8   fude_zoom_pdfview_open(fude_zoom_pdfview* _v, const c8* _path);
// Closes the PDF (or leaves it to the worker to close, while a piece is being drawn), frees the pictures and clears _v.
void fude_zoom_pdfview_close(fude_zoom_pdfview* _v);
// Returns whether a PDF with pages is open.
b8   fude_zoom_pdfview_is_open(const fude_zoom_pdfview* _v);

// Call once a frame: puts a finished piece on the GPU, asks for the next one, lets go of pictures far from the view,
// redraws everything when the theme's colours change, and collects a search's new matches. _home: the home frame on
// the screen (origin at the screen's centre, Y up); _half: half the screen's size, in points.
void fude_zoom_pdfview_update(fude_zoom_pdfview* _v, fude_zoom_sim _home, fude_zoom_v2 _half);
// Draws the pages (and a search's marks), under the ink. Arguments as for fude_zoom_pdfview_update.
void fude_zoom_pdfview_render(const fude_zoom_pdfview* _v, fude_zoom_sim _home, fude_zoom_v2 _half);

// Returns the canvas the pages make, in home frame units: what the page keeps the view over.
fude_zoom_box fude_zoom_pdfview_bounds(const fude_zoom_pdfview* _v);
// Returns the page whose box holds _at (home frame units), or else the nearest one, counting from 0.
u32  fude_zoom_pdfview_page_at(const fude_zoom_pdfview* _v, fude_zoom_v2 _at);
// Turn a place on page _page (in points from its top-left corner) into canvas units (home frame), and back. A page
// out of range is taken as the first.
fude_zoom_v2 fude_zoom_pdfview_to_canvas(const fude_zoom_pdfview* _v, u32 _page, rde_vec_2F _points);
rde_vec_2F   fude_zoom_pdfview_to_page(const fude_zoom_pdfview* _v, u32 _page, fude_zoom_v2 _at);

// fude_zoom_pdfview_search starts a search for _query, stopping the one before; its matches come in as the PDF is gone
// through (fude_zoom_pdfview_update), and the first is gone to as it comes (fude_zoom_pdfview_search_wanted). Nothing
// is searched when no PDF is open, _query is empty, or this platform cannot read a PDF's text.
// fude_zoom_pdfview_search_stop stops it and drops its matches.
void fude_zoom_pdfview_search(fude_zoom_pdfview* _v, const c8* _query);
void fude_zoom_pdfview_search_stop(fude_zoom_pdfview* _v);
// Returns the match to fly to now, when one is wanted (the first match, once it has come in): its box on the canvas
// (home frame units) into *_box, and it becomes the one gone to. Returns false when none is wanted; it answers true
// only once for each search.
b8   fude_zoom_pdfview_search_wanted(fude_zoom_pdfview* _v, fude_zoom_box* _box);
// Puts match _i's box on the canvas (home frame units) into *_box and makes it the one gone to. Returns false when
// there is no such match.
b8   fude_zoom_pdfview_match_box(fude_zoom_pdfview* _v, u32 _i, fude_zoom_box* _box);

// The formulas' type (formula.h), named here without including formula.h.
struct fude_zoom_formulas;
// Writes the PDF again at _out with what was drawn on it (scene _s's things at any depth). Each page is copied as it
// is, so its text stays text, and over it go, in the order they were drawn (the markers first, under the rest): lines,
// the fills the eraser and the bucket left, the shapes, the pictures, and the texts and notes, their lines broken as
// _fonts (each loaded at _font_px) break them on the canvas: the app's font in its four faces, in pdf.h's
// FUDE_PDF_FACE_ order (regular, bold, italic, bold italic), a note's words measured and set in their own face (a NULL
// face as the regular one); with no regular one, no text is written. It is on white paper, with the theme's ink as
// the light theme's. What lies beside the pages is left out. Formulas are set with _formulas (with NULL, as their
// LaTeX). It first waits for the piece the worker is drawing, if any (pdf.h: a PDF is drawn on one thread at a time).
// Returns false when no PDF is open or it could not be written (pdf.h: this platform has no writer).
b8   fude_zoom_pdfview_export(const fude_zoom_pdfview* _v, const fude_zoom_scene* _s, rde_font* const _fonts[FUDE_PDF_FACES], f32 _font_px,
                              struct fude_zoom_formulas* _formulas, const c8* _out);

#endif
