#ifndef FUDE_DOC
#define FUDE_DOC

#include "rde.h"
#include "drawing/ink/canvas.h"
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
} fude_doc_book;

// A picture of part of a page, on the GPU.
RDE_STRUCT {
    rde_memory_texture* texture;   // NULL: none
    rde_vec_2F          from;      // the part of the page it shows, in page points from its top-left
    rde_vec_2F          size;
    f32                 zoom;      // the zoom it was drawn for (a sharp piece's)
} fude_doc_tile;

RDE_STRUCT {
    rde_vec_2F    points;   // its size in the PDF (points)
    rde_vec_2F    size;     // on the canvas
    f32           top;      // its top edge, canvas y (Y up: pages go down)
    fude_doc_tile whole;
    fude_doc_tile sharp;    // what was on screen, at the screen's pixels
    u32           seen;     // the frame it was last on screen (or near), for letting go
} fude_doc_page;

struct fude_doc_job;

typedef struct fude_doc {
    c8                   path[RDE_MAX_PATH];   // the PDF open ("": none)
    b8                   failed;               // it could not be opened (not tried again)
    fude_pdf*            pdf;
    fude_doc_page*       pages;
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
    fude_pdf_match*      matches;              // FUDE_PDF_MATCHES
    u32                  match_count;
    u32                  match_at;             // the one shown (match_count: none yet)
    b8                   searching;            // asked, matches coming or kept
    b8                   search_done;
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

// Its pages (0: no document open), and the one in the middle of the view (from 0).
u32   fude_doc_page_count(const fude_doc* _doc);
u32   fude_doc_page_at(const fude_doc* _doc, fude_view _view);
// Page _page's top under the top of the screen, at the zoom there is (at least 100%).
void  fude_doc_go_to_page(fude_doc* _doc, fude_canvas* _canvas, u32 _page);
// The PDF's own text inside the canvas box _min-_max (a lasso's: the pages it
// covers, a line between them), UTF-8 into _out: its length (0: none there, or
// no text in the PDF — a scan).
usize fude_doc_text_in(fude_doc* _doc, rde_vec_2F _min, rde_vec_2F _max, c8* _out, usize _size);
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
