// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_DOC_WORKER
#define FUDE_DOC_WORKER

#include "rde.h"
#include "drawing/doc/pdf.h"

// ===========================================================================
// The PDF worker: draws a PDF's pieces (a page whole, the part of a page on screen at the screen's pixels, or a page's
// picture for a reader) on a worker thread, one piece at a time, and hands them back to the main thread, which puts
// them on the GPU as textures (fude_doc_tile). Every PDF shown under ink is drawn through it: a study app's document
// (doc.c) and a canvas app's PDF view (zoom/pdfview.c), each with its own fude_doc_worker.
//
// Where it sits: drawing/doc, over pdf.h, which draws the pixels. Which piece is drawn next is its caller's to say
// (each lays its pages out its own way), but for the whole pages, which both choose the same way: from what the
// caller sees of each page (fude_doc_seen), fude_doc_wholes says which to keep, which to draw next and frees the rest.
// This module runs the piece and everything about the thread.
//
// Its main types and their lifecycle: the caller keeps a fude_doc_worker (inside its document) and sets it up with
// fude_doc_worker_init. fude_doc_worker_start hands the worker a fude_doc_piece, made here; fude_doc_worker_take hands
// it back once drawn, and the caller frees it with fude_doc_piece_free. fude_doc_worker_drop lets go of the piece as
// the PDF closes. A fude_doc_tile is a texture the caller keeps for each page, filled by fude_doc_tile_take.
//
// Threads: every function here is called on the main thread. The piece is drawn on a detached RDE thread started for
// it, which reads only the piece (filled before it starts) and the PDF, and writes only the piece's pixels and its ok;
// then it sets the piece's done, under one mutex shared by every worker (it guards only done and orphaned), and wakes
// whoever waits for it (fude_doc_worker_wait). The main thread looks at done once a frame (fude_doc_worker_take);
// nothing waits for a piece but a PDF being written out (below). See docs/architecture.md, "Threads": "The PDF
// worker".
//   - One piece at a time: a caller starts a piece only when fude_doc_worker_busy is false, so a PDF is only ever
//     drawn on one thread (pdf.h asks it).
//   - The colours changed (fude_doc_worker_colours: a theme changed), or the pages it draws did
//     (fude_doc_worker_forget: a page turned): the piece being drawn is marked stale. It stays the worker's piece, so
//     no second one is started on the same PDF, and fude_doc_worker_take throws it away.
//   - The PDF written out (fude_doc_worker_wait): a PDF writer draws the PDF's pages on the main thread, so it first
//     waits for the piece being drawn, at most one piece's drawing, and no new piece starts until it is through.
//   - The PDF closing (fude_doc_worker_drop): a finished piece is freed there; one still being drawn is marked
//     orphaned, and the thread frees it and closes the PDF itself once it has drawn with it, so the PDF is never closed
//     under it (pdf.h's Threads allows that close on the worker).
//
// Files: none.
//
// Design: a piece comes from the engine's allocator (rde_calloc), which is thread-safe, because the thread may be the
// one to free it; its pixels are on the std allocator, as anything as big as a page is, and so is a tile's copy of
// them (RDE's memory texture keeps its pixels after the upload: no call of RDE's lets them go). A piece to show is
// turned the way a texture wants (the bottom row first) and, in a dark theme, recoloured, both on the thread, so the
// main thread only copies it to the GPU. A picture for a reader is left as drawn: the top row first, in its own
// colours.
// Whole pages are bounded by bytes, not by a count, because their size varies: a page is drawn whole at about its
// size on screen (fude_doc_whole_size), so zoomed out over many pages each costs little. The pages on screen are
// always kept (each picture at most about twice its page's width on screen, so together a few screens' worth); the
// others are kept nearest the view first while everything kept fits FUDE_DOC_KEEP_BYTES. A page is drawn only when
// it is kept, so nothing drawn is let go at once and, once every kept page is drawn, the worker rests: the caller's
// other work (a document's reading of its scans) has its turn.
// ===========================================================================

// The pieces' measures, the same for every caller.
#define FUDE_DOC_WHOLE_PX    1100u     // The width in pixels of a whole page's texture at most (a page seen smaller...
#define FUDE_DOC_WHOLE_LEAST 17u       // ...is drawn smaller, down to this: FUDE_DOC_WHOLE_PX halved six times).
// A page narrower than this on screen, in pixels, is not drawn whole: it shows as its paper, which is all that a page
// so small shows, and a document zoomed out over thousands of pages does not draw thousands of dots.
#define FUDE_DOC_SEEN_LEAST  8.0f
// The longer side of any texture made here, in pixels, at most: a sharp piece's, and a whole page's (a long receipt is
// drawn narrower to fit). It is the least Vulkan promises (maxImageDimension2D), which every Android GPU the apps draw
// with gives; Apple's give 16384. RDE does not say the GPU's own limit.
#define FUDE_DOC_SHARP_MAX   4096u
#define FUDE_DOC_SCALE       2.0f      // Screen pixels to a point (iPad and Mac are all 2x).
// The whole pages' pictures kept, in bytes (4 a pixel) at most, but for those on screen, which are always kept: about
// seven A4 pages at FUDE_DOC_WHOLE_PX (1100 x 1556), or many more pages seen small. Each byte is kept twice: on the GPU
// and in the memory texture's copy (a third more on the GPU for the mipmaps).
#define FUDE_DOC_KEEP_BYTES  (48u * 1024u * 1024u)
#define FUDE_DOC_REST        0.15      // Seconds the view must keep still before the sharp pieces are drawn.

// What a piece is drawn for.
typedef enum {
    FUDE_DOC_PIECE_WHOLE = 0,   // A page whole, its texture mipmapped (it is seen small too).
    FUDE_DOC_PIECE_SHARP,       // The part of a page on screen, at the screen's pixels.
    FUDE_DOC_PIECE_READ         // A page's picture for a reader: left as drawn, never put on the GPU here.
} FUDE_DOC_PIECE_;

// A picture of part of a page, on the GPU.
RDE_STRUCT {
    rde_memory_texture* texture;   // NULL when nothing is drawn yet.
    rde_vec_2F          from;      // The part of the page it shows, in page points from the page's top-left...
    rde_vec_2F          size;      // ...and its size in points.
    // What it was drawn for: a sharp piece's zoom (its caller's measure); a whole page's width level (the *_level of
    // fude_doc_whole_size, which a tall page's picture is narrower than).
    f32                 zoom;
    u32                 w, h;      // Its size in pixels.
} fude_doc_tile;

// One piece of work handed to the worker, and its result. The main thread fills it (fude_doc_worker_start) and the
// thread draws it; once fude_doc_worker_take hands it back, every field is the main thread's to read.
typedef struct fude_doc_piece {
    fude_pdf*  pdf;
    b8         close_pdf;   // The PDF was let go while it was drawn: the thread closes it too.
    u32        page;
    u8         kind;        // FUDE_DOC_PIECE_
    rde_vec_2F from, size;  // The part of the page to draw, in its points.
    u32        w, h;        // The picture's size, in pixels.
    f32        zoom;        // What it is drawn for (fude_doc_tile's zoom: a sharp piece's zoom, a whole page's level).
    b8         dark;        // Drawn in a dark theme's colours: white paper in paper, black print in print.
    rde_color  paper, print;
    rde_arr TYPE(u8) pixels;   // w * h * 4 bytes, RGBA: the bottom row first once drawn, but for a reader's picture.
    b8         ok;          // fude_pdf_render drew it.
    b8         done;        // The thread has finished (read and written under the mutex).
    b8         orphaned;    // The main thread let it go, so the thread frees it (under the mutex).
} fude_doc_piece;

// A worker: the piece being drawn, and the colours pieces are drawn in. Its fields are worker.c's; a caller reads
// paper (the colour a page shows before it is drawn) and calls fude_doc_worker_busy.
typedef struct {
    fude_doc_piece* job;     // The piece being drawn (NULL when none).
    // The running piece was asked for before the colours or the pages changed: what it draws is thrown away when it is
    // taken. It stays the worker's piece meanwhile, so no second one is started on the same PDF.
    b8              stale;
    b8              dark;    // Pieces are drawn in a dark theme's colours...
    rde_color       paper, print;   // ...these two (paper is white in a light theme).
} fude_doc_worker;

// Clears _worker: no piece, and colours no theme has, so the first fude_doc_worker_colours takes the theme's. The first
// call makes the mutex and the wake-up every worker shares (never freed).
void fude_doc_worker_init(fude_doc_worker* _worker);
// Whether a piece is being drawn, or drawn and not taken yet: no other piece may be started meanwhile.
b8   fude_doc_worker_busy(const fude_doc_worker* _worker);
// Takes the active theme's colours for the pieces: in a dark theme (its page colour's luma under 128) the paper a shade
// off its page colour and the print in its text colour; else white paper. Returns true when they changed: the piece
// being drawn (drawn the old way) is then marked stale, and the caller drops its tiles so they are drawn again.
b8   fude_doc_worker_colours(fude_doc_worker* _worker);
// Marks the piece being drawn, if any, stale: what it shows is no longer the page (a page turned, so it is laid out
// and drawn again), and fude_doc_worker_take throws it away once it is through.
void fude_doc_worker_forget(fude_doc_worker* _worker);
// Starts drawing a piece of _pdf on a new detached thread: the part of page _page at _from of _size (page points) into
// _w x _h pixels, for _kind (FUDE_DOC_PIECE_), for _zoom (kept with it: fude_doc_tile's zoom), in the worker's colours.
// Call it only when the worker is not busy. (The piece is rde_calloc's, which stops the app rather than fail.)
void fude_doc_worker_start(fude_doc_worker* _worker, fude_pdf* _pdf, u32 _page, u8 _kind, rde_vec_2F _from, rde_vec_2F _size, u32 _w, u32 _h, f32 _zoom);
// Takes the piece once it is drawn. Returns NULL while it is still being drawn, when there is none, and for a stale one
// (freed here). Otherwise the piece is the caller's (its ok says whether it was drawn), to free with
// fude_doc_piece_free.
fude_doc_piece* fude_doc_worker_take(fude_doc_worker* _worker);
// Waits, on the calling (main) thread, until the piece being drawn, if any, is through, so the PDF is drawn on no other
// thread until the next piece is started: what a PDF writer needs before it draws the PDF's pages (pdf.h: one thread
// at a time). The piece stays the worker's, taken as ever by fude_doc_worker_take.
void fude_doc_worker_wait(const fude_doc_worker* _worker);
// Frees a piece taken with fude_doc_worker_take, and its pixels; its PDF is not closed.
void fude_doc_piece_free(fude_doc_piece* _piece);
// Lets go of the piece, as its PDF closes: the only call that may leave a piece to the thread. A finished piece is
// freed here. Returns true when one was still being drawn: it is orphaned, its thread closes the PDF once done with it,
// and the caller must forget its PDF (never close it). False: the caller closes its PDF itself.
b8   fude_doc_worker_drop(fude_doc_worker* _worker);

// Frees a tile's texture and empties the tile, so it reads as not drawn.
void fude_doc_tile_free(fude_doc_tile* _tile);
// Puts a drawn piece (taken, its ok true, not a reader's) on the GPU as _tile, in place of what the tile had: clamped
// at its edges, and mipmapped for a whole page, which is seen small too. The tile keeps the piece's part, zoom and
// size in pixels. Its memory texture, and the copy of the pixels it keeps, are on the std allocator.
void fude_doc_tile_take(fude_doc_tile* _tile, const fude_doc_piece* _piece);

// --- Whole pages: which to keep, and which to draw ------------------------------------------

// The size in pixels to draw page _points (its size in points) whole at, when it is _px pixels wide on screen. Its
// *_level is FUDE_DOC_WHOLE_PX halved for as long as the half is still as wide as the page on screen, down to
// FUDE_DOC_WHOLE_LEAST, so the picture is from once to twice the page's width on screen (at most FUDE_DOC_WHOLE_PX).
// *_w and *_h are what is drawn: the level wide, as tall as the page's shape makes it (a page narrower than a point
// taken as a point wide; 1 at least), and a page so tall that this passes FUDE_DOC_SHARP_MAX shrunk both ways to fit.
void fude_doc_whole_size(rde_vec_2F _points, f32 _px, u32* _level, u32* _w, u32* _h);

// Where a page is from the view (fude_doc_seen's where).
typedef enum {
    FUDE_DOC_AWAY = 0,     // Further than a screen from what is on screen: kept while there is room, never drawn.
    FUDE_DOC_NEAR,         // Within a screen of it: drawn ahead, so that scrolling finds it drawn.
    FUDE_DOC_ON_SCREEN     // On screen: always drawn, and always kept.
} FUDE_DOC_WHERE_;

// A page as its caller sees it this frame. The caller fills one for every page of its document, each frame, in an array
// of its own (fude_doc_wholes reorders it: it is scratch).
typedef struct {
    fude_doc_tile* whole;    // Its whole picture: the caller's own tile, which fude_doc_wholes may free.
    rde_vec_2F     points;   // Its size in points.
    f64            far;      // How far its middle is from the middle of the view, in any measure the same for each page.
    f32            px;       // Its width on screen, in pixels (FUDE_DOC_SCALE a point).
    u32            page;     // Which page it is.
    u8             where;    // FUDE_DOC_WHERE_
} fude_doc_seen;

// Chooses, once a frame, the whole pages to keep and the next to draw, from _seen (_n pages, filled by the caller; the
// array is reordered). A page on screen is always kept; the others, near ones first, then the rest, each nearest the
// middle of the view first, while their pictures and those on screen fit FUDE_DOC_KEEP_BYTES (a picture counted as it
// is, or as it will be once drawn again). Pages not kept have their whole pictures freed here. A kept page on screen or
// near whose picture is missing, or drawn for a size it no longer suits (the page zoomed in to over 15% wider on screen
// than the picture's level, or out until the level is over 2.3 times its width), is to be drawn: *_screen is the page
// on screen to draw next and *_near the near one, nearest the middle first (UINT32_MAX: none). A page is never asked
// for unless it is kept, so a still view asks for each page once and then nothing: the worker rests. A page under
// FUDE_DOC_SEEN_LEAST pixels wide on screen counts as away.
void fude_doc_wholes(fude_doc_seen* _seen, u32 _n, u32* _screen, u32* _near);

#endif
