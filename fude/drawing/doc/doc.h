// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_DOC
#define FUDE_DOC

#include "rde.h"
#include "drawing/ink/canvas.h"
#include "drawing/ink/ink.h"
#include "drawing/doc/pdf.h"
#include "drawing/doc/worker.h"

// ===========================================================================
// Document: the PDF under a canvas's ink, to read and to write on. It is either a book from the app's library (a study
// app's lectures, in its assets) or the canvas's own PDF (one imported, or pictures and scans made into one:
// notes/<id>.pdf). The canvas's note says which (notes.h: fude_note.document).
//
// Where it sits: drawing/doc, above drawing/ink (the canvas and ink it lies under) and drawing/base. The app's page
// (page.c) owns one fude_doc and updates and draws it each frame; the document bar (docbar.h), the study layer's
// page text (study/widgets/pagetext.c) and the look harness also call it.
//
// Its main types and their lifecycle: the page makes one fude_doc with fude_doc_init and keeps it for the app's life.
// Nobody opens or closes a PDF by hand: each frame fude_doc_update opens the PDF that fude_doc_place names and lets
// go of the one before. fude_doc_destroy lets everything go. A fude_doc_book is one entry of the app's own constant
// table (extension.h: fude_extension.library).
//
// Layout: the pages hang one under the other, down from the canvas's origin, each FUDE_DOC_PAGE_W canvas units
// wide. Ink written on a page stays on it at any zoom, and the canvas pans and zooms over the pages as over anything
// else (canvas.h). Room is left around them, so the learner can write in the margins.
//
// Drawing: each page is drawn into textures. One shows the whole page, sharp enough at a glance; once the view
// rests, the part of each page on screen is drawn again at the screen's own pixels, so it is sharp at any zoom. In a
// dark theme the pages are dark too: drawn in the theme's colours (white paper becomes its page colour and black
// print becomes light), so ink in the theme's colour shows on them. While the view moves, the page in the middle of
// the screen shows as "3 / 24" at the bottom.
//
// Text: a page's own text comes from the PDF (PDFKit's, pdf.h). A page with none (a scan, a photo) is read by the
// app's picture reader instead (extension.h: ML Kit, say): each such page is read once, in the background, nearest
// the view first, and its lines are kept beside the PDF (<name>.lines) so it is never read again. The lasso's text
// and Search use both kinds.
//
// Threads: everything here is called on the main thread, and the picture reader's hooks are called and polled there
// too. The textures are drawn by the document's PDF worker (worker.h: RDE's detached threads), one piece at a time,
// nearest the view first. Nothing waits for them (a page not drawn yet shows blank paper) but fude_doc_export, which
// draws the PDF's pages itself and so waits for the piece being drawn, if any. A document let go while a piece is being
// drawn leaves that piece to the worker to finish and free, closing the PDF with it.
//
// Files: reads the PDF; reads and writes the read lines in <name>.lines beside a canvas's own PDF (a kfile.h file of
// kind 'LINS', described at the top of doc.c); fude_doc_export writes a new PDF. The lines of a book of the app's
// are never kept, since its books have text of their own.
//
// Design: pages are drawn off the main thread, one piece at a time, so a slow page never stalls a frame. The whole
// pages kept drawn are those on screen and the nearest others within FUDE_DOC_KEEP_BYTES (worker.h: fude_doc_wholes),
// so a long book does not fill memory, and a page is drawn only when it is to be kept, so the worker rests once they
// are drawn and the pictures have their turn to be read.
// ===========================================================================

#define FUDE_DOC_PAGE_W     1000.0f   // Every page's width, in canvas units.
#define FUDE_DOC_PAGE_GAP   48.0f     // The gap between one page and the next, in canvas units.
// The pieces' sizes, how much of the whole pages is kept and how long the view must rest are the worker's (worker.h:
// FUDE_DOC_WHOLE_PX, FUDE_DOC_SHARP_MAX, FUDE_DOC_SCALE, FUDE_DOC_KEEP_BYTES, FUDE_DOC_REST), the same for a canvas
// app's PDF view.

// A book of the app's library (extension.h: the app's list of them).
typedef struct fude_doc_book {
    // 2 and up, and never reused, because a canvas stores it (fude_note.document).
    u8        id;
    const c8* file;        // Its PDF, among the app's assets ("assets/lectures/...").
    u32       title;       // A FUDE_TEXT_ id.
    u32       about;       // A FUDE_TEXT_ id: what the book is, and who it is for.
    const c8* credit;      // Who made it and its licence, worded as they word it ("文化庁 · CC BY 4.0").
    // Its first page as a picture (a PNG in the assets), shown in the Library; NULL when it has none.
    const c8* cover;
    // The language it is written in ("en", "es"...). Of the books with the same title, the Library shows the one in
    // the app's language, else the English one; a book with NULL here is always shown.
    const c8* language;
} fude_doc_book;

#define FUDE_DOC_LINE_TEXT 256u    // The longest text of a read line, in bytes.
#define FUDE_DOC_READ_PX   1600u   // The width of the picture of a page handed to the reader.
// Its height at most, in pixels: a page longer than one and a half times its width (a receipt, a long scroll) is drawn
// squeezed rather than handed over as a picture the reader takes too long on; its lines are then turned back into
// points by the picture's own height (fude_doc.read_h).
#define FUDE_DOC_READ_MAX_H 2400u

// A line of a page's text, read from its picture: where it is, and what it says.
typedef struct fude_doc_line {
    u32        page;
    // Its box in points from the page's top-left. The app's reader hands it over in the picture's pixels, and doc.c
    // converts it to points.
    rde_vec_2F from, size;
    c8         text[FUDE_DOC_LINE_TEXT];
} fude_doc_line;

// Where a page's text comes from, and how far reading it has gone.
typedef enum {
    FUDE_DOC_TEXT_UNKNOWN = 0,   // The PDF has not been asked yet.
    FUDE_DOC_TEXT_OWN,           // The PDF has text of its own on this page.
    FUDE_DOC_TEXT_TO_READ,       // It has none: its picture is waiting to be read.
    FUDE_DOC_TEXT_READING,       // The reader has its picture now.
    FUDE_DOC_TEXT_READ,          // Read; its lines are kept (there may be none).
    FUDE_DOC_TEXT_FAILED         // Its picture could not be drawn, so nothing was read: not asked again while the
                                 // document is open, and not saved as read, so the next opening tries it again.
} FUDE_DOC_TEXT_;

// One page of the open document, as laid out on the canvas.
RDE_STRUCT {
    rde_vec_2F    points;   // Its size in the PDF, in points.
    rde_vec_2F    size;     // Its size on the canvas.
    f32           top;      // Its top edge, as a canvas y (Y points up, so the pages go down).
    fude_doc_tile whole;
    fude_doc_tile sharp;    // The part that was on screen, at the screen's own pixels.
    u8            text;     // FUDE_DOC_TEXT_
} fude_doc_page;

// The document under the open canvas. Its fields are doc.c's; the document bar only reads `searching`.
typedef struct fude_doc {
    c8                   path[RDE_MAX_PATH];   // The open PDF ("" when none is open).
    b8                   failed;               // It could not be opened; it is not tried again.
    fude_pdf*            pdf;
    rde_arr TYPE(fude_doc_page) pages;
    u32                  page_count;
    // Draws the pages' pieces, one at a time, in the theme's colours (its paper is the blank page's colour).
    fude_doc_worker      worker;
    fude_view            seen_view;            // The view last seen, to tell when it has come to rest.
    f64                  moved_at;
    // A search (the document bar's): its matches as they come in, and the one shown.
    // Room for FUDE_PDF_MATCHES: the PDF's own matches first, then those in the read lines.
    rde_arr TYPE(fude_pdf_match) matches;
    u32                  match_count;
    u32                  match_at;             // The match shown (match_count when none is shown yet).
    b8                   searching;            // A search was asked for; its matches are coming in or kept.
    b8                   search_done;
    c8                   query[128];
    // The pages' text read from their pictures (see the top of this file).
    rde_arr TYPE(fude_doc_line) lines;
    c8                   lines_path[RDE_MAX_PATH];   // Where they are kept ("" when not kept: a book of the app's).
    // One page's lines, as the reader hands them over (every one it read), before they join lines.
    rde_arr TYPE(fude_doc_line) read_lines;
    u32                  reading;              // The page the reader has now (UINT32_MAX when none)...
    u32                  read_w;               // ...and the width of its picture,
    u32                  read_h;               // ...and its height.
    u32                  probe;                // The next page to ask whether it has text of its own.
    u32                  wanted;               // A page to read before the others (UINT32_MAX when none).
    b8                   reader_off;           // The reader refused; it is not asked again while this PDF is open.
    // A reading no longer wanted (its document was let go, or its page turned) is still due: its answer is taken and
    // dropped when it comes.
    b8                   reader_drain;
    // Each page's quarter turns, as laid out. The canvas's page holds the real value (canvas.h), and its revision when
    // the document last took them (turns_seen; turns_taken is false until it has, once it opens).
    rde_arr TYPE(u8)     turned;
    u32                  turns_seen;
    b8                   turns_taken;
    // Each page as seen from the view, filled each frame for the worker to choose the whole pages to keep and draw
    // (worker.h: fude_doc_wholes, which reorders it). Room for page_count, kept from frame to frame.
    rde_arr TYPE(fude_doc_seen) seen;
} fude_doc;

// fude_doc_init sets up an empty document with nothing open. fude_doc_destroy lets go of everything it holds (the PDF,
// its textures, and a job still on the worker, which the worker then frees).
void fude_doc_init(fude_doc* _doc);
void fude_doc_destroy(fude_doc* _doc);

// Where a document is shown, filled in each frame by the app's page (page.c). path is the file under its canvas's
// ink ("" for none; documents.h finds it); then the canvas and the window. The two hooks are the reader for pages
// with no text of their own (the app's extension, extension.h: NULL when the app reads no pictures).
typedef struct {
    const c8*    path;
    fude_canvas* canvas;
    rde_window*  window;
    b8         (*picture_read)(const u8* _rgba, u32 _w, u32 _h);
    b8         (*picture_lines)(rde_arr* _out);
} fude_doc_place;

// Called once a frame by the page. Opens the document _at names (letting go of another one), puts finished pieces
// on the GPU, asks the worker for the next one, and moves the reading of pictures and a search along.
void fude_doc_update(fude_doc* _doc, const fude_doc_place* _at);
// Draws the pages under the ink, after the paper.
void fude_doc_render(fude_doc* _doc, const fude_canvas* _canvas, rde_window* _window);
// Draws the "3 / 24" pill (the page in the middle of _view, out of how many) in _font, over everything on the page.
// It shows only for a moment after the view moves, and never for a document of one page.
void fude_doc_render_pill(fude_doc* _doc, fude_view _view, rde_font* _font, f32 _font_px, rde_window* _window);

// The view for a new document canvas on _window: the first page's width fits across, its top near the top.
fude_view fude_doc_first_view(rde_window* _window);

// Turns page _page a quarter more clockwise. The turn belongs to the canvas and is saved with its ink. What is
// written on the page turns with it, and the ink on the pages after it moves with them. A page whose text was read
// from its picture is read again, since it was likely sideways. Returns false when no document is open, or _page is
// out of range (or past 65,535, the most the page's file can name).
b8    fude_doc_turn_page(fude_doc* _doc, fude_canvas* _canvas, fude_ink* _ink, u32 _page);
// The number of pages in the open document (0 when none is open), and the page in the middle of _view, counting
// from 0.
u32   fude_doc_page_count(const fude_doc* _doc);
u32   fude_doc_page_at(const fude_doc* _doc, fude_view _view);
// Scrolls so that page _page's top sits under the top of the screen (a _page past the end goes to the last page),
// keeping the zoom as it is: the reader's frame keeps it at 100% or more.
void  fude_doc_go_to_page(fude_doc* _doc, fude_canvas* _canvas, u32 _page);
// Writes the text inside the canvas box _min-_max (a lasso's) into _out as UTF-8, always ending it with a 0, and
// returns its length (0 when there is none there). The box may cover several pages; their texts are joined with a
// line break. A page's own text is used, else what was read in its picture. *_pending (_pending may be NULL) is set
// when a page under the box is still to be read: it is read next, so ask again.
usize fude_doc_text_in(fude_doc* _doc, rde_vec_2F _min, rde_vec_2F _max, c8* _out, usize _size, b8* _pending);
// Writes the document as a new PDF at _out, with what was written on it (_ink: the canvas's) drawn over its pages.
// Ink beside the pages is left out. The pages are on white paper, so ink in the theme's colour is drawn in the light
// theme's ink colour. The text read in its pictures is put in unseen, so any PDF reader finds it. It first waits for
// the piece the worker is drawing, if any (pdf.h: a PDF is drawn on one thread at a time). Returns false when no
// document is open or the file could not be written.
b8    fude_doc_export(fude_doc* _doc, const fude_ink* _ink, const c8* _out);
// Search. fude_doc_search starts a search for _query (stopping the one before): its matches are found a little each
// frame as the PDF is gone through, the first is shown as soon as it comes, and all are marked on the pages. The text
// read in its pictures is searched too, compared as the PDF's own is (utf8.h: fude_utf8_find).
// fude_doc_search_step shows another match (_step +1 the next, -1 the one before, wrapping round), and
// fude_doc_search_stop ends the search. fude_doc_search_count returns the number of matches so far (0 when not
// searching), and sets *_at to the one shown (from 0) and *_done to whether the whole document has been searched;
// neither pointer may be NULL.
void  fude_doc_search(fude_doc* _doc, const c8* _query);
void  fude_doc_search_step(fude_doc* _doc, fude_canvas* _canvas, i32 _step);
void  fude_doc_search_stop(fude_doc* _doc);
u32   fude_doc_search_count(const fude_doc* _doc, u32* _at, b8* _done);

#endif
