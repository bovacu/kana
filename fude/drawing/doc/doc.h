// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_DOC
#define FUDE_DOC

#include "rde.h"
#include "drawing/ink/canvas.h"
#include "drawing/ink/ink.h"
#include "drawing/doc/pdf.h"

// ===========================================================================
// A canvas's document: the PDF under its ink, to read and write on — a book of
// the app's library (Kana's lectures, in its assets) or the canvas's own (a
// PDF imported, or pictures and scans made into one: notes/<id>.pdf). Which,
// the canvas's note says (notes.h: fude_note.document).
//
// Its pages hang one under the other down from the canvas's origin, each
// FUDE_DOC_PAGE_W canvas units wide: ink written on one stays on it at any zoom,
// and the canvas pans and zooms over them as over anything (canvas.h). Room is
// left around them, to write in the margins.
//
// Drawn from textures: each page whole, sharp enough at a glance; and, once the
// view rests, the part of each page on screen again at the screen's own pixels
// — sharp at any zoom. They are drawn on a thread of their own, one piece at a
// time, nearest first; nothing waits for them (a page not drawn yet is blank).
// In a dark theme the pages are dark too: drawn in the theme's colours (white
// paper its page colour, black print light), so ink in the theme's colour shows
// on them.
//
// While the view moves, the page in the middle of the screen shows as "3 / 24"
// at the bottom.
//
// TEXT: a page's own (PDFKit's, pdf.h) — or, a page with none (a scan, a photo),
// what the app's picture reader (extension.h: Kana's ML Kit) reads in it: each
// such page read once, in the background, nearest the view first, its lines
// kept beside the PDF (<name>.lines) so it is never read again. The lasso's
// text and Search use both.
// ===========================================================================

struct fude_app;

#define FUDE_DOC_PAGE_W     1000.0f   // a page's width, canvas units
#define FUDE_DOC_PAGE_GAP   48.0f     // between pages
#define FUDE_DOC_WHOLE_PX   1100u     // a whole page's texture: its width in pixels
#define FUDE_DOC_SHARP_MAX  4096u     // the sharp piece's longer side, at most
#define FUDE_DOC_SCALE      2.0f      // pixels to a point on screen (iPad, Mac: all 2x)
#define FUDE_DOC_KEEP       6u        // whole pages kept drawn, at most (the nearest): about 12 MB each
#define FUDE_DOC_REST       0.15      // seconds the view keeps still before the sharp pieces

// A book of the app's library (extension.h: the app's list of them).
typedef struct fude_doc_book {
    u8        id;          // 2 and up, never reused: what a canvas keeps (fude_note.document)
    const c8* file;        // its PDF, among the app's assets ("assets/lectures/...")
    u32       title;       // FUDE_TEXT_
    u32       about;       // FUDE_TEXT_: what it is, for whom
    const c8* credit;      // who made it and its licence, as they word it ("文化庁 · CC BY 4.0")
    const c8* cover;       // its first page as a picture (a PNG in the assets), for the Library; NULL: none
    const c8* language;    // the language it is written in ("en", "es"...): of the books with its title the
                           // Library shows the one in the app's language, else the English one; NULL: shown always
} fude_doc_book;

// A picture of part of a page, on the GPU.
RDE_STRUCT {
    rde_memory_texture* texture;   // NULL: none
    rde_vec_2F          from;      // the part of the page it shows, in page points from its top-left
    rde_vec_2F          size;
    f32                 zoom;      // the zoom it was drawn for (a sharp piece's)
} fude_doc_tile;

#define FUDE_DOC_LINE_TEXT 256u    // bytes of a read line's text, at most
#define FUDE_DOC_READ_PX   1600u   // a page read for its text: its picture's width
#define FUDE_DOC_READ_MAX  256u    // lines kept from one page, at most

// A line of a page's text read from its picture: where, and what it says.
typedef struct fude_doc_line {
    u32        page;
    rde_vec_2F from, size;   // points from the page's top-left, as read (from the app's reader: its pixels)
    c8         text[FUDE_DOC_LINE_TEXT];
} fude_doc_line;

// A page's text.
typedef enum {
    FUDE_DOC_TEXT_UNKNOWN = 0,   // not asked yet
    FUDE_DOC_TEXT_OWN,           // the PDF's own
    FUDE_DOC_TEXT_TO_READ,       // none: its picture to be read
    FUDE_DOC_TEXT_READING,
    FUDE_DOC_TEXT_READ           // read (its lines, maybe none)
} FUDE_DOC_TEXT_;

RDE_STRUCT {
    rde_vec_2F    points;   // its size in the PDF (points)
    rde_vec_2F    size;     // on the canvas
    f32           top;      // its top edge, canvas y (Y up: pages go down)
    fude_doc_tile whole;
    fude_doc_tile sharp;    // what was on screen, at the screen's pixels
    u32           seen;     // the frame it was last on screen (or near), for letting go
    u8            text;     // FUDE_DOC_TEXT_
} fude_doc_page;

struct fude_doc_job;

typedef struct fude_doc {
    c8                   path[RDE_MAX_PATH];   // the PDF open ("": none)
    b8                   failed;               // it could not be opened (not tried again)
    fude_pdf*            pdf;
    rde_arr TYPE(fude_doc_page) pages;
    u32                  page_count;
    struct fude_doc_job* job;                  // the piece being drawn (NULL: none)
    b8                   dark;                 // drawn in a dark theme's colours
    rde_color            paper, print;         // ...these
    fude_view            seen_view;            // to tell the view resting
    f64                  moved_at;
    u32                  frame;
    u32                  page_shown;           // the page in the middle, last shown in the pill
    f64                  page_shown_at;
    // A search (the document bar's): its matches as they come, the one shown.
    rde_arr TYPE(fude_pdf_match) matches;      // FUDE_PDF_MATCHES: the PDF's own, then the read lines'
    u32                  match_count;
    u32                  match_at;             // the one shown (match_count: none yet)
    b8                   searching;            // asked, matches coming or kept
    b8                   search_done;
    c8                   query[128];
    // The pages' text read from their pictures (see the top).
    rde_arr TYPE(fude_doc_line) lines;
    c8                   lines_path[RDE_MAX_PATH];   // where they are kept ("": not kept — a book of the app's)
    rde_arr TYPE(fude_doc_line) read_lines;    // FUDE_DOC_READ_MAX: a page's, as the reader hands them over
    u32                  reading;              // the page being read (UINT32_MAX: none)
    u32                  read_w;               // ...its picture's width
    u32                  probe;                // the next page asked whether it has text of its own
    u32                  wanted;               // a page to read before the others (UINT32_MAX: none)
    b8                   reader_off;           // the reader said no: not asked again while this one is open
    b8                   reader_drain;         // a reading of a document let go: taken and dropped
    rde_arr TYPE(u8)     turned;               // each page's quarter turns, as laid out (the canvas's page says: canvas.h)
} fude_doc;

void fude_doc_init(fude_doc* _doc);
void fude_doc_destroy(fude_doc* _doc);

// The document of a canvas whose note says _document (fude_note.document; 0:
// none): where its PDF is, into _out. False: none (or no such book).
b8   fude_doc_path(const struct fude_app* _app, u32 _canvas, u8 _document, c8* _out, usize _size);

// Once a frame (the page's): the open canvas's document opened (another let
// go), finished pieces put on the GPU, the next one asked for.
void fude_doc_update(fude_doc* _doc, struct fude_app* _app);
// Its pages, under the ink (after the paper).
void fude_doc_render(fude_doc* _doc, const fude_canvas* _canvas, rde_window* _window);
// The "3 / 24" pill, over everything on the page.
void fude_doc_render_pill(fude_doc* _doc, const struct fude_app* _app, rde_window* _window);

// The view that fits the first page's width on a _window, its top near the top.
fude_view fude_doc_first_view(rde_window* _window);

// Page _page turned a quarter clockwise more (the canvas's: saved with its ink),
// what is written on it turned with it and what is on the pages after moved
// with them; a page read from its picture read again (it was likely sideways).
// False: no room to keep another turned page.
b8    fude_doc_turn_page(fude_doc* _doc, struct fude_app* _app, u32 _page);
// Its pages (0: no document open), and the one in the middle of the view (from 0).
u32   fude_doc_page_count(const fude_doc* _doc);
u32   fude_doc_page_at(const fude_doc* _doc, fude_view _view);
// Page _page's top under the top of the screen, at the zoom there is (at least 100%).
void  fude_doc_go_to_page(fude_doc* _doc, fude_canvas* _canvas, u32 _page);
// The text inside the canvas box _min-_max (a lasso's: the pages it covers, a
// line between them) — a page's own, else what was read in its picture —
// UTF-8 into _out: its length (0: none there). *_pending (may be NULL): a page
// under it is still to be read (it is read next: ask again).
usize fude_doc_text_in(fude_doc* _doc, rde_vec_2F _min, rde_vec_2F _max, c8* _out, usize _size, b8* _pending);
// The document as a new PDF at _out, what was written on it drawn over its
// pages (_ink: the canvas's; ink beside the pages is left out) — on white paper,
// the theme's ink as the light theme's — and the text read in its pictures put
// in unseen (any reader finds it). False when it could not be written.
b8    fude_doc_export(fude_doc* _doc, const fude_ink* _ink, const c8* _out);
// Search: _query's matches found as the PDF is gone through (the first shown as
// it comes), all of them marked on the pages; stepped through (_step: +1 the
// next, -1 the one before, round); stopped. The count so far, the one shown
// (from 0), and whether it has been through the whole document.
void  fude_doc_search(fude_doc* _doc, const c8* _query);
void  fude_doc_search_step(fude_doc* _doc, fude_canvas* _canvas, i32 _step);
void  fude_doc_search_stop(fude_doc* _doc);
u32   fude_doc_search_count(const fude_doc* _doc, u32* _at, b8* _done);

// A new canvas over a document, opened (its view at the first page, no paper
// under it), named _name, in folder _parent (0: the top level). _document: a
// library book's id, or FUDE_NOTE_DOCUMENT_OWN — then the PDF at _own is copied
// in as the canvas's own. Its id; 0 when it could not be made.
u32  fude_doc_new_canvas(struct fude_app* _app, u8 _document, const c8* _own, const c8* _name, u32 _parent);
// The canvas over library book _book: the one there is (opened), or a new one
// in the folder the others are in (made, named _folder_name, the first time).
u32  fude_doc_open_book(struct fude_app* _app, const fude_doc_book* _book, const c8* _folder_name);

#endif
