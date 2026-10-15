// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_PDF
#define FUDE_PDF

#include "rde.h"

// ===========================================================================
// PDF: opens, measures and draws PDFs into pixels with the platform's own renderer, writes new PDFs, and finds a
// PDF's own text. These are the pages a document canvas shows under its ink (doc.h).
//
// Where it sits: drawing/doc, the lowest part of it. doc.c uses it for a document canvas; the zoom app's PDF view
// (zoom/pdfview.c) and page (pictures), the import (import.c: pictures made into a PDF) and the document bar
// (docbar.c) use it too.
//
// Platforms: on Apple's (iOS, macOS) drawing and writing are Core Graphics (pdf.c, plain C, no Objective-C) and the
// text is PDFKit (pdf_kit.m). On Android drawing is the system's PdfRenderer and the text and writing are PDFBox
// (pdf_android.c). Elsewhere there is nothing yet (Windows is still to choose): fude_pdf_available is false and
// nothing opens.
//
// Its main types and their lifecycle: fude_pdf_open makes a fude_pdf and fude_pdf_close frees it. A fude_pdf_writer
// is made by fude_pdf_write_begin and freed by fude_pdf_write_end.
//
// Threads: everything is called on the main thread, except fude_pdf_render, which a worker thread may call (doc.c's
// and zoom/pdfview.c's), and fude_pdf_close, which that worker may call after its last render when the main thread
// let the PDF go while it was drawing (an orphaned job: the worker then owns the PDF alone). A document is drawn on
// ONE thread at a time, and only drawn there: what the main thread needs (how many pages, how big) is read as the PDF
// opens, and a PDF writer, which draws its pages on the main thread, first waits for the worker's piece (worker.h:
// fude_doc_worker_wait). On Android a search, and the opening and closing of PDFBox's document for the text
// (fude_pdf_text_prepare, fude_pdf_close), run on a thread of FudePdfBox's own ("FudePdfFind").
// Closing on the worker is safe on every platform: Apple's releases Core Graphics' document and PDFKit's (nothing
// else holds them by then, and the release has an autorelease pool of its own); Android's calls FudePdf.close through
// the worker's own JNI env, as its render does, and FudePdf's and FudePdfBox's lists are locked; elsewhere there is
// nothing to close. A worker's memory goes back to thread-safe allocators (SDL's and the std one).
//
// Positions: every position here is in points (1/72 inch) from the page's top-left corner, on the page as it is
// meant to be read: turned as the PDF says, and as the learner turned it (fude_pdf_set_turn).
//
// Design: a search goes on a few milliseconds at a time, each time its matches are asked for (once a frame), because
// PDFKit's own background search answers through a run loop that the app's frame does not turn.
// ===========================================================================

typedef struct fude_pdf fude_pdf;

// Whether this platform can show PDFs at all.
b8         fude_pdf_available(void);

// Opens the PDF at _path; a relative path is from the working folder, which on a device is the app's assets.
// Returns NULL when it is not there, is not a PDF, has no pages, or is locked with a password (an empty password is
// tried first). Free it with fude_pdf_close, which accepts NULL: on the main thread, or on the worker that drew with
// it last once the main thread has let it go (see Threads).
fude_pdf*  fude_pdf_open(const c8* _path);
void       fude_pdf_close(fude_pdf* _pdf);

// The number of pages (0 for NULL).
u32        fude_pdf_page_count(const fude_pdf* _pdf);
// The size of page _page (from 0) in points (1/72 inch), turned as it is meant to be read and as the learner turned
// it (fude_pdf_set_turn). A page out of range is A4 (595 x 842).
rde_vec_2F fude_pdf_page_size(const fude_pdf* _pdf, u32 _page);
// Turns page _page _quarters quarter turns clockwise more than the PDF says. It is then drawn, written, and its text
// read and found that way: every position here is on the page as turned. Call it on the main thread while nothing of
// the page is being drawn.
void       fude_pdf_set_turn(fude_pdf* _pdf, u32 _page, u8 _quarters);

// Draws part of page _page into _w x _h pixels at _rgba (RGBA, 4 bytes each, the top row first): the rectangle at
// _from (points from the page's top-left corner) of _size points, stretched to fill them, on white. Returns false
// when it could not be drawn. It may be called on a worker thread, one thread at a time for a document.
b8         fude_pdf_render(fude_pdf* _pdf, u32 _page, rde_vec_2F _from, rde_vec_2F _size, u32 _w, u32 _h, u8* _rgba);

#define FUDE_PDF_IMAGE_PX 2400u   // The longer side of a picture made into a PDF page, in pixels, at most.
// Makes image files (JPEG, PNG, HEIC...) into a new PDF at _out, one page each. Each is turned as it was taken, as
// big as the image at 150 dots an inch (at most FUDE_PDF_IMAGE_PX pixels on its longer side), and kept as JPEG.
// Returns false when the PDF could not be written, or when no image could be read (then no file is left).
b8         fude_pdf_from_images(const c8* const* _images, u32 _count, const c8* _out);

// Reads a picture file (JPEG, PNG, HEIC...) as bytes a texture can be made from: JPEG, or PNG when it has
// see-through parts (a logo, a diagram), turned as it was taken and no larger than _max_px on its longer side. It is
// done by ImageIO on Apple's devices and Android's ImageDecoder on Android (pdf_android.c); elsewhere it is the
// file's own bytes. The bytes go into _out (an rde_arr of u8 the caller owns, cleared first). Returns false when the
// file cannot be read.
b8         fude_picture_bytes(const c8* _path, u32 _max_px, rde_arr* _out);

// --- Writing a PDF: a document with what was written on it ----------------------------------
// Pages are copied from a PDF as they are, so their text stays text. Strokes are drawn over them and, where a picture
// shows text (a scan's, once read), that text is put in unseen, so any PDF reader finds and copies it. Positions are
// as above: points from the page's top-left, on the page as read. Apple's is Core Graphics and Core Text; Android's
// is PDFBox (pdf_android.c); elsewhere fude_pdf_write_begin returns NULL.
// The calls go: fude_pdf_write_begin, then for each page fude_pdf_write_page, what goes on it, and
// fude_pdf_write_page_end, then fude_pdf_write_end. Drawing calls made with no page begun are ignored.

typedef struct fude_pdf_writer fude_pdf_writer;

// Whether this platform can write a PDF (Apple's can; Android's can when PDFBox is there).
b8               fude_pdf_write_available(void);
// Starts writing a new PDF at _out. Returns NULL when it cannot be made or this platform cannot write one.
fude_pdf_writer* fude_pdf_write_begin(const c8* _out);
// Begins a page: page _page of _pdf, the same size, drawn as it is read.
void             fude_pdf_write_page(fude_pdf_writer* _w, fude_pdf* _pdf, u32 _page);
// Draws a stroke over the page through _n _points, with half-widths _radii, in _color. _even means one width and
// see-through (a marker's): it is drawn as one shape, so it is never darker where it crosses itself.
void             fude_pdf_write_stroke(fude_pdf_writer* _w, const rde_vec_2F* _points, const f32* _radii, u32 _n, rde_color _color, b8 _even);
// Fills a shape in _color made of _count triangles (three points each, positioned as above), painted as one (the
// nonzero rule, so where they meet does not show): a fill the eraser left, or a filled shape.
void             fude_pdf_write_fill(fude_pdf_writer* _w, const rde_vec_2F* _triangles, u32 _count, rde_color _color);
// Draws a picture (a JPEG's or a PNG's bytes) over the page, with its bottom-left, bottom-right and top-left corners
// at _corners (positioned as above), so it can be turned and stretched.
void             fude_pdf_write_image(fude_pdf_writer* _w, const u8* _bytes, u32 _size, const rde_vec_2F _corners[3]);
// Writes a line of visible text in _color, _size points tall, with the left end of its baseline at _at, in the app's
// UI font (Roboto, draw.h's FUDE_DRAW_FONT_*) and the face fude_pdf_write_face last set, as the screen draws it. A
// character Roboto lacks is set in a font of the system's on Apple's, and left out on Android's.
void             fude_pdf_write_text(fude_pdf_writer* _w, const c8* _text, rde_vec_2F _at, f32 _size, rde_color _color);
// The faces the visible text can be set in (fude_pdf_write_face): Roboto's four.
typedef enum {
    FUDE_PDF_FACE_REGULAR = 0,
    FUDE_PDF_FACE_BOLD,
    FUDE_PDF_FACE_ITALIC,
    FUDE_PDF_FACE_BOLD_ITALIC,
    FUDE_PDF_FACES
} FUDE_PDF_FACE_;
// Sets the face the visible text written next is set in (a FUDE_PDF_FACE_; any other is the regular one). A writer
// starts in the regular face and keeps the one set, page after page. Each face's font goes into the file the first
// time text is written in it.
void             fude_pdf_write_face(fude_pdf_writer* _w, u8 _face);
// Puts text in unseen, stretched to fill the box at _from of _size, so a PDF reader finds it where the picture shows
// it.
void             fude_pdf_write_hidden_text(fude_pdf_writer* _w, const c8* _text, rde_vec_2F _from, rde_vec_2F _size);
// Ends the page begun by fude_pdf_write_page.
void             fude_pdf_write_page_end(fude_pdf_writer* _w);
// Ends a page still open, finishes the file and frees _w. Returns false when it could not be written.
b8               fude_pdf_write_end(fude_pdf_writer* _w);

// --- A PDF's own text, where it has some (a scan has none) ---------------------------------
// Apple's is PDFKit (pdf_kit.m) and Android's is PDFBox (pdf_android.c), called on the main thread. Positions are as
// above: points from the page's top-left, on the page as read.

// A place where the text was found: its page and its rectangle on that page.
RDE_STRUCT {
    u32        page;
    rde_vec_2F from;
    rde_vec_2F size;
} fude_pdf_match;

// The most matches a search keeps; it counts as done once it has this many. A bound by design, not a limit on the
// document: the matches so far are copied out of the platform's search and marked on the pages every frame
// (fude_pdf_find_matches), so the bound keeps that frame's work small however common the word, and a word found more
// often than this is one to search with more of its words.
#define FUDE_PDF_MATCHES 512u

// Whether this platform can read a PDF's text.
b8    fude_pdf_text_available(void);
// Readies the PDF's text where that is slow, off the main thread: on Android it begins opening PDFBox's document for
// it on FudePdfBox's own thread, the first time it is called. Returns whether the text is ready: until it is, the calls
// below wait for the rest of the opening (a moment, or seconds for a big PDF on a phone), so a caller that asks
// often (a document asking each page whether it has text) asks only once this says so. Always true on Apple's
// (PDFKit opens its document at once, the first time it is asked) and elsewhere (there is none).
b8    fude_pdf_text_prepare(fude_pdf* _pdf);
// Whether page _page has text of its own, rather than a picture of some (as a scan has).
b8    fude_pdf_page_has_text(fude_pdf* _pdf, u32 _page);
// Writes the text inside the rectangle at _from of _size on page _page into _out (_out_size bytes) as UTF-8, ending
// it with a 0 and never cutting a character in half. Returns its length; 0 when there is none there.
usize fude_pdf_text_in(fude_pdf* _pdf, u32 _page, rde_vec_2F _from, rde_vec_2F _size, c8* _out, usize _out_size);
// Starts a search for _query, stopping the one before; case, accents and full- or half-width forms all match alike.
// Its matches come in as it goes, in page order: on Apple's it goes on a few milliseconds each time
// fude_pdf_find_matches is called (once a frame); Android's runs in a thread of its own. fude_pdf_find_stop ends it.
void  fude_pdf_find_start(fude_pdf* _pdf, const c8* _query);
void  fude_pdf_find_stop(fude_pdf* _pdf);
// Copies the matches so far, at most _max, into _out (NULL to just count them) and returns how many. Sets *_done
// (never NULL) once the search has been through the whole document.
u32   fude_pdf_find_matches(fude_pdf* _pdf, fude_pdf_match* _out, u32 _max, b8* _done);

// The platform's side of the text, for pdf.c only: pdf_kit.m on Apple's, and nothing elsewhere. fude_pdf_kit_open
// opens a document of its own for the PDF at _path (NULL when PDFKit cannot), which pdf.c keeps in the fude_pdf and
// frees with fude_pdf_kit_close. The rest work as the fude_pdf_ functions above, but take positions on the page as
// the PDF reads it (not turned by the learner); pdf.c converts them.
any fude_pdf_kit_open(const c8* _path);
b8    fude_pdf_kit_page_has_text(any _kit, u32 _page);
void  fude_pdf_kit_close(any _kit);
usize fude_pdf_kit_text_in(any _kit, u32 _page, rde_vec_2F _from, rde_vec_2F _size, c8* _out, usize _out_size);
void  fude_pdf_kit_find_start(any _kit, const c8* _query);
void  fude_pdf_kit_find_stop(any _kit);
u32   fude_pdf_kit_find_matches(any _kit, fude_pdf_match* _out, u32 _max, b8* _done);

#endif
