// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PDF
#define FUDE_ZOOM_PDF

#include "rde.h"
#include "drawing/base/kfile.h"

// ===========================================================================
// A PDF 1.4 writer, in memory, plain C: drawing commands in, a whole file out.
// What the view's PDF export, the tiled print (woodworking templates on A4 or
// Letter, at their true size) and the full-size print-shop file are made with.
//
//   Pages     each its own size, in points (1/72 inch; FUDE_ZOOM_PDF_MM to a
//             millimetre). Inside a page the coordinates are PDF's own: the
//             origin at the bottom-left corner, x to the right, y UP.
//   Drawing   paths (moves, lines, cubic curves, rectangles), filled (non-zero
//             or even-odd), stroked, or used to clip; colours with their alpha
//             (one shared ExtGState for each distinct alpha, fill and stroke
//             apart); line width, caps, joins and dashes; transforms (cm) and
//             save/restore (q/Q) around them.
//   Images    added once, drawn as often as wanted: a JPEG's bytes as they are
//             (DCTDecode), or RGBA pixels as an RGB image with its alpha as a
//             soft mask (none when every pixel is opaque).
//   Text      in the standard Helvetica (not embedded: every reader has it or
//             its metric twin), WinAnsiEncoding — Latin-1 and the few extras it
//             maps (€ – — ‘ ’ “ ” • …); anything else comes out as '?'. Its
//             width from Helvetica's own metrics, so labels can be centred.
//
// Content streams are written as they are, without compression (the project
// has no zlib); numbers are kept short instead: four decimals at most (six for
// a transform's scale and turn), no trailing zeros, never an exponent.
//
// The writer is opaque. Drawing outside a page is ignored; a page begun while
// another is open ends that one first, and each page starts from PDF's default
// state (black, opaque, line width 1), whatever the one before left — saves
// still open when it ends are restored there.
// ===========================================================================

#define FUDE_ZOOM_PDF_MM (72.0 / 25.4)   // points in a millimetre

typedef struct fude_zoom_pdf fude_zoom_pdf;

// An empty document (NULL: no memory). Free it with fude_zoom_pdf_free.
fude_zoom_pdf* fude_zoom_pdf_new(void);
void           fude_zoom_pdf_free(fude_zoom_pdf* _pdf);

// --- pages -----------------------------------------------------------------------------------------

// A new page, _width_pt by _height_pt (its MediaBox). Under 3 points, or not a
// number, a side becomes 3; readers such as Acrobat open up to 14400 (200 inches).
void fude_zoom_pdf_page_begin(fude_zoom_pdf* _pdf, f64 _width_pt, f64 _height_pt);
void fude_zoom_pdf_page_end(fude_zoom_pdf* _pdf);

// --- graphics state --------------------------------------------------------------------------------

// q and Q: everything below (colours, widths, dashes, transforms, clips) is put
// back by the matching restore. A restore with no save open is ignored.
void fude_zoom_pdf_save(fude_zoom_pdf* _pdf);
void fude_zoom_pdf_restore(fude_zoom_pdf* _pdf);
// The current transform times this matrix (cm): a point (x, y) is drawn at
// (_a x + _c y + _e, _b x + _d y + _f).
void fude_zoom_pdf_transform(fude_zoom_pdf* _pdf, f64 _a, f64 _b, f64 _c, f64 _d, f64 _e, f64 _f);
// The colour fills (and text) are painted in, and its alpha.
void fude_zoom_pdf_fill_color(fude_zoom_pdf* _pdf, rde_color _c);
// The colour lines are stroked in, and its alpha.
void fude_zoom_pdf_stroke_color(fude_zoom_pdf* _pdf, rde_color _c);
void fude_zoom_pdf_line_width(fude_zoom_pdf* _pdf, f64 _width);
// Line ends (_cap: 0 butt, 1 round, 2 square) and corners (_join: 0 miter,
// 1 round, 2 bevel); above 2 counts as 2.
void fude_zoom_pdf_line_style(fude_zoom_pdf* _pdf, u8 _cap, u8 _join);
// Dashes: _count lengths, on and off in turn, starting _phase into them. 0 (or
// a pattern with a negative length, or only zeros): solid. Past 16 lengths, the
// rest are left out.
void fude_zoom_pdf_dash(fude_zoom_pdf* _pdf, const f64* _pattern, u32 _count, f64 _phase);

// --- paths -----------------------------------------------------------------------------------------

void fude_zoom_pdf_move(fude_zoom_pdf* _pdf, f64 _x, f64 _y);
void fude_zoom_pdf_line(fude_zoom_pdf* _pdf, f64 _x, f64 _y);
// A cubic Bezier from the current point to (_x3, _y3), its controls 1 and 2.
void fude_zoom_pdf_curve(fude_zoom_pdf* _pdf, f64 _x1, f64 _y1, f64 _x2, f64 _y2, f64 _x3, f64 _y3);
// Closes the current subpath (a line back to where it began).
void fude_zoom_pdf_close(fude_zoom_pdf* _pdf);
// A rectangle as a closed subpath of its own, its corner (_x, _y).
void fude_zoom_pdf_rect(fude_zoom_pdf* _pdf, f64 _x, f64 _y, f64 _w, f64 _h);
// _count points (_xy: x, y, x, y...) as one subpath: a move to the first, lines
// to the rest, closed when _closed. The fast way for a stroke's thousands.
void fude_zoom_pdf_polyline(fude_zoom_pdf* _pdf, const f64* _xy, u32 _count, b8 _closed);

// Painting the path, which ends it: fill (_even_odd: f*, else non-zero f),
// stroke, or both (B*, B).
void fude_zoom_pdf_fill(fude_zoom_pdf* _pdf, b8 _even_odd);
void fude_zoom_pdf_stroke(fude_zoom_pdf* _pdf);
void fude_zoom_pdf_fill_stroke(fude_zoom_pdf* _pdf, b8 _even_odd);
// The path as the clip (W n, W* n), painting nothing. Only a restore widens a
// clip again, so it goes inside a save.
void fude_zoom_pdf_clip(fude_zoom_pdf* _pdf, b8 _even_odd);

// --- images ----------------------------------------------------------------------------------------

// A JPEG, its bytes kept as they are: its size and colours (1 gray, 3 RGB,
// 4 CMYK) read from its frame header. Baseline, extended or progressive, 8 bits
// (what DCTDecode reads); Adobe's inverted CMYK is drawn the right way round.
// Its EXIF orientation is not applied. Its id, 0 when it is none of those.
u32  fude_zoom_pdf_jpeg(fude_zoom_pdf* _pdf, const u8* _bytes, u32 _size);
// _width by _height RGBA pixels (straight alpha, top row first), copied. Its
// id, 0 when there is nothing to add.
u32  fude_zoom_pdf_rgba(fude_zoom_pdf* _pdf, const u8* _rgba, u32 _width, u32 _height);
// Image _id drawn on the unit square mapped by this matrix (as the transform):
// (_w, 0, 0, _h, _x, _y) puts it upright, _w by _h, its bottom-left corner at
// (_x, _y) and its top row at the top. It is painted through the fill's alpha
// (PDF's /ca covers images too): an opaque fill colour first for an opaque
// picture. An unknown id draws nothing.
void fude_zoom_pdf_image(fude_zoom_pdf* _pdf, u32 _id, f64 _a, f64 _b, f64 _c, f64 _d, f64 _e, f64 _f);

// --- text ------------------------------------------------------------------------------------------

// UTF-8 text in Helvetica, _size points, in _c: its baseline starts at (_x, _y).
// Leaves the fill colour as it found it.
void fude_zoom_pdf_text(fude_zoom_pdf* _pdf, f64 _x, f64 _y, f64 _size, rde_color _c, const c8* _utf8);
// How wide that text is at _size points (Helvetica's widths, no kerning).
f64  fude_zoom_pdf_text_width(f64 _size, const c8* _utf8);

// --- the file --------------------------------------------------------------------------------------

// The whole file appended to _out (ending a page still open). False, with
// nothing appended, when there are no pages or the document grew past 2 GB.
b8   fude_zoom_pdf_finish(fude_zoom_pdf* _pdf, fude_bytes* _out);

#endif
