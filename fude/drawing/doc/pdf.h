#ifndef FUDE_PDF
#define FUDE_PDF

#include "rde.h"

// ===========================================================================
// PDFs, by the platform's own renderer: opened, measured, and drawn into
// pixels — the pages a document canvas shows under its ink (doc.h). Core
// Graphics on Apple's (iOS, macOS: plain C, no Objective-C). Elsewhere none
// yet (Android has PdfRenderer, Windows to choose): fude_pdf_available is false
// and nothing opens.
//
// A document is drawn on ONE thread at a time (doc.c keeps a single worker), and
// only drawn there: what the main thread needs — how many pages, how big — is
// read as it opens.
// ===========================================================================

typedef struct fude_pdf fude_pdf;

// Can this platform show PDFs at all?
b8         fude_pdf_available(void);

// The PDF at _path (relative: to the working folder — the app's assets on a
// device); NULL when it is not there, not a PDF, or locked with a password.
fude_pdf*  fude_pdf_open(const c8* _path);
void       fude_pdf_close(fude_pdf* _pdf);

u32        fude_pdf_page_count(const fude_pdf* _pdf);
// Page _page's size (from 0) in points (1/72 inch), turned as it is meant to be read.
rde_vec_2F fude_pdf_page_size(const fude_pdf* _pdf, u32 _page);

// Part of page _page into _w x _h pixels at _rgba (RGBA, 4 bytes each, the top
// row first): the rectangle at _from (points, from its top-left corner) of _size
// points, stretched to fill them, on white. False when it could not be.
b8         fude_pdf_render(fude_pdf* _pdf, u32 _page, rde_vec_2F _from, rde_vec_2F _size, u32 _w, u32 _h, u8* _rgba);

// Images (files: JPEG, PNG, HEIC...) as a new PDF at _out: a page each, turned
// as they were taken, as big as the image at 150 dots an inch (at most
// FUDE_PDF_IMAGE_PX pixels on its longer side), kept as JPEG. False when it
// could not be written (or none read).
#define FUDE_PDF_IMAGE_PX 2400u
b8         fude_pdf_from_images(const c8* const* _images, u32 _count, const c8* _out);

// --- its text: the PDF's own, where it has some (a scan has none) ---------------------
// Apple's PDFKit (pdf_kit.m), on the main thread. Positions as above: points
// from the page's top-left, the page as read.

// A place the text was found: its page and its rectangle there.
RDE_STRUCT {
    u32        page;
    rde_vec_2F from;
    rde_vec_2F size;
} fude_pdf_match;

#define FUDE_PDF_MATCHES 512u   // a search's matches kept, at most

// Can this platform read a PDF's text?
b8    fude_pdf_text_available(void);
// The text inside the rectangle at _from (_size) on page _page, UTF-8 into _out
// (_out_size bytes): its length; 0 when there is none there.
usize fude_pdf_text_in(fude_pdf* _pdf, u32 _page, rde_vec_2F _from, rde_vec_2F _size, c8* _out, usize _out_size);
// A search for _query (case, accents and full- or half-width alike): started (the
// one before stopped); its matches come in as it goes, in page order — it goes
// on a few milliseconds each time fude_pdf_find_matches is asked (once a frame).
void  fude_pdf_find_start(fude_pdf* _pdf, const c8* _query);
void  fude_pdf_find_stop(fude_pdf* _pdf);
// The matches so far, at most _max into _out (NULL: just count): how many; *_done
// once it has been through the whole document.
u32   fude_pdf_find_matches(fude_pdf* _pdf, fude_pdf_match* _out, u32 _max, b8* _done);

// The platform's side of the text (pdf_kit.m on Apple's; nothing elsewhere):
// a document of its own for the PDF at _path, kept by pdf.c in the fude_pdf.
void* fude_pdf_kit_open(const c8* _path);
void  fude_pdf_kit_close(void* _kit);
usize fude_pdf_kit_text_in(void* _kit, u32 _page, rde_vec_2F _from, rde_vec_2F _size, c8* _out, usize _out_size);
void  fude_pdf_kit_find_start(void* _kit, const c8* _query);
void  fude_pdf_kit_find_stop(void* _kit);
u32   fude_pdf_kit_find_matches(void* _kit, fude_pdf_match* _out, u32 _max, b8* _done);

#endif
