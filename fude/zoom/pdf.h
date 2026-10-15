// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PDF
#define FUDE_ZOOM_PDF

#include "rde.h"
#include "drawing/base/kfile.h"

// ===========================================================================
// PDF writer: a PDF 1.4 writer in plain C, in memory: drawing commands go in and a whole file comes out. The view's
// PDF export, the tiled print (woodworking templates on A4 or Letter, at their true size) and the full-size file for a
// print shop are made with it.
//
// Where it sits: zoom, the canvas apps' layer. export.c is its user (export.h), and page_export.c sets the fonts its
// text is written in. It is not drawing/doc's pdf.h, which writes over an existing PDF with the platform's own
// libraries; this one needs none. It reads the fonts it embeds through mathfont.h.
// Its main types and their lifecycle: fude_zoom_pdf_new makes a fude_zoom_pdf (opaque) and fude_zoom_pdf_free frees it.
// The fonts set with fude_zoom_pdf_fonts are kept here, shared by the documents made after.
// Threads: none of its own. The fonts set are this module's state, so it is used on the main thread only (as it is);
// one document must be used on one thread at a time.
// Files: it writes no file itself: fude_zoom_pdf_finish appends the PDF's bytes to a fude_bytes for the caller to save.
// It reads the font files it is told to (fude_file_read), when a character is first looked for in them.
//
//   Pages     each its own size, in points (1/72 inch; FUDE_ZOOM_PDF_MM points to a millimetre). Inside a page the
//             coordinates are PDF's own: the origin at the bottom-left corner, x to the right, y UP.
//   Drawing   paths (moves, lines, cubic curves, rectangles), filled (non-zero or even-odd), stroked, or used to clip;
//             colours with their alpha (one shared ExtGState for each distinct alpha, fill and stroke apart); line
//             width, caps, joins and dashes; transforms (cm) and save/restore (q/Q) around them.
//   Images    added once and drawn as often as wanted: a JPEG's bytes as they are (DCTDecode), or RGBA pixels as an
//             RGB image with its alpha as a soft mask (no mask when every pixel is opaque).
//   Text      in the app's own fonts, as the screen draws it, once they are set (fude_zoom_pdf_fonts): for each
//             character the face's font, or the first of the fonts after it that has the character, as the screen's
//             fallbacks go; a character none of them has is the first font's missing-glyph box, as on the screen. Each
//             font used is embedded as a Type 0 font (Identity-H): TrueType outlines as a CIDFontType2 (FontFile2) cut
//             down to the glyphs used, CFF outlines as a CIDFontType0 (FontFile3, CIDFontType0C) whole. The text is
//             written as glyph ids, with each font's widths (W) and a ToUnicode map back to the characters, so a
//             reader can select, copy and search it. With no fonts set, or the face's and the regular face's first
//             font unreadable, it is the standard Helvetica instead (not embedded: every reader has it or its metric
//             twin) in WinAnsiEncoding: Latin-1 and the few extras it maps (€ – — ‘ ’ “ ” • …), any other character a
//             '?'. Widths come from the same fonts' metrics, so labels can be centred.
//
// Design: content streams are written as they are, without compression, because the project has no zlib; numbers are
// kept short instead: four decimals at most (six for a transform's scale and turn), no trailing zeros, never an
// exponent. Drawing outside a page is ignored. A page begun while another is open ends that one first, and each page
// starts from PDF's default state (black, opaque, line width 1), whatever the one before left; saves still open when
// a page ends are restored there, because some readers refuse a page with a q left open.
// Fonts: a TrueType font is cut down by keeping every glyph id and emptying the glyphs not used (a used composite's
// parts are kept), so only glyf and loca are rebuilt and nothing is renumbered: a CID is the glyph id (CIDToGIDMap
// /Identity), and the tables that list every glyph (hmtx) are copied as they are, with the hinting (cvt, fpgm, prep)
// and the tables some readers look for (OS/2, cmap, name, post). Roboto cut down so is about 18 KB and 100 bytes a
// glyph used, against 146 KB whole: a page of Latin, Greek and Cyrillic in one face carries 25 to 30 KB of it, and in
// all four faces about 100 KB. A CFF font is not cut down (that would need a CFF writer): it is embedded whole, its
// glyphs named by their CIDs when it is CID-keyed, so one bigger than FUDE_ZOOM_PDF_CFF_MOST is left out of its chain
// (logged): the canvas apps' one, Noto Sans JP cut down to the language picker's names (日本語), is 0.8 KB. Shaping is
// not done: each character is its own glyph at its own advance, without the kerning (GPOS) and ligatures (GSUB) the
// screen's HarfBuzz applies, which Latin, Greek and Cyrillic read well without; a character the screen draws as
// nothing (a zero-width space or joiner, a variation selector: Unicode's default-ignorable ones, which HarfBuzz hides)
// is left out here too. A glyph's ToUnicode entry is the first character it was written for, and the missing-glyph
// box's is U+FFFD.
// ===========================================================================

#define FUDE_ZOOM_PDF_MM (72.0 / 25.4)   // Points in a millimetre.

typedef struct fude_zoom_pdf fude_zoom_pdf;

// Makes an empty document; NULL when there is no memory. Free it with fude_zoom_pdf_free, which accepts NULL.
fude_zoom_pdf* fude_zoom_pdf_new(void);
void           fude_zoom_pdf_free(fude_zoom_pdf* _pdf);

// --- pages -----------------------------------------------------------------------------------------

// fude_zoom_pdf_page_begin starts a new page _width_pt by _height_pt points (its MediaBox), ending an open one first.
// A side under 3 points, or not a number, becomes 3; readers such as Acrobat open sides up to 14400 points (200
// inches). fude_zoom_pdf_page_end ends the open page, restoring any saves still open.
void fude_zoom_pdf_page_begin(fude_zoom_pdf* _pdf, f64 _width_pt, f64 _height_pt);
void fude_zoom_pdf_page_end(fude_zoom_pdf* _pdf);

// --- graphics state --------------------------------------------------------------------------------

// Save and restore (q and Q): everything below (colours, widths, dashes, transforms, clips) set after a save is put
// back by the matching restore. A restore with no save open is ignored.
void fude_zoom_pdf_save(fude_zoom_pdf* _pdf);
void fude_zoom_pdf_restore(fude_zoom_pdf* _pdf);
// Multiplies the current transform by this matrix (cm): a point (x, y) is then drawn at
// (_a x + _c y + _e, _b x + _d y + _f).
void fude_zoom_pdf_transform(fude_zoom_pdf* _pdf, f64 _a, f64 _b, f64 _c, f64 _d, f64 _e, f64 _f);
// Sets the colour fills (and text) are painted in, and its alpha.
void fude_zoom_pdf_fill_color(fude_zoom_pdf* _pdf, rde_color _c);
// Sets the colour lines are stroked in, and its alpha.
void fude_zoom_pdf_stroke_color(fude_zoom_pdf* _pdf, rde_color _c);
// Sets the width lines are stroked at, in the current units (points unless transformed).
void fude_zoom_pdf_line_width(fude_zoom_pdf* _pdf, f64 _width);
// Sets the line ends (_cap: 0 butt, 1 round, 2 square) and corners (_join: 0 mitre, 1 round, 2 bevel). A value above
// 2 counts as 2.
void fude_zoom_pdf_line_style(fude_zoom_pdf* _pdf, u8 _cap, u8 _join);
// Sets the dashes: _count lengths at _pattern, on and off in turn, starting _phase into them. A _count of 0, a NULL
// pattern, a negative length or only zeros means solid. Past 16 lengths the rest are left out.
void fude_zoom_pdf_dash(fude_zoom_pdf* _pdf, const f64* _pattern, u32 _count, f64 _phase);

// --- paths -----------------------------------------------------------------------------------------

// Begin a new subpath at (_x, _y), and add a straight line from the current point to (_x, _y).
void fude_zoom_pdf_move(fude_zoom_pdf* _pdf, f64 _x, f64 _y);
void fude_zoom_pdf_line(fude_zoom_pdf* _pdf, f64 _x, f64 _y);
// Adds a cubic Bezier curve from the current point to (_x3, _y3), with control points 1 and 2.
void fude_zoom_pdf_curve(fude_zoom_pdf* _pdf, f64 _x1, f64 _y1, f64 _x2, f64 _y2, f64 _x3, f64 _y3);
// Closes the current subpath with a line back to where it began.
void fude_zoom_pdf_close(fude_zoom_pdf* _pdf);
// Adds a rectangle as a closed subpath of its own, its corner at (_x, _y), _w wide and _h high.
void fude_zoom_pdf_rect(fude_zoom_pdf* _pdf, f64 _x, f64 _y, f64 _w, f64 _h);
// Adds _count points (_xy: x, y, x, y...) as one subpath: a move to the first and lines to the rest, closed when
// _closed. It is the fast way to add a stroke's thousands of points.
void fude_zoom_pdf_polyline(fude_zoom_pdf* _pdf, const f64* _xy, u32 _count, b8 _closed);

// Paint the path, which ends it: fill it (with _even_odd the even-odd rule, f*; else non-zero, f), stroke it, or both
// (B*, B).
void fude_zoom_pdf_fill(fude_zoom_pdf* _pdf, b8 _even_odd);
void fude_zoom_pdf_stroke(fude_zoom_pdf* _pdf);
void fude_zoom_pdf_fill_stroke(fude_zoom_pdf* _pdf, b8 _even_odd);
// Uses the path as the clip (W n, or W* n with _even_odd), painting nothing. Only a restore widens a clip again, so
// clip inside a save.
void fude_zoom_pdf_clip(fude_zoom_pdf* _pdf, b8 _even_odd);

// --- images ----------------------------------------------------------------------------------------

// Adds a JPEG, its bytes kept as they are (copied); its size and colours (1 grey, 3 RGB, 4 CMYK) are read from its
// frame header. Baseline, extended or progressive JPEGs of 8 bits are taken (what DCTDecode reads); Adobe's inverted
// CMYK is drawn the right way round. Its EXIF orientation is not applied. Returns the image's id, or 0 when it is none
// of those or the document would grow past 2 GB.
u32  fude_zoom_pdf_jpeg(fude_zoom_pdf* _pdf, const u8* _bytes, u32 _size);
// Adds _width by _height RGBA pixels (straight alpha, top row first), copied. Returns the image's id, or 0 when there
// is nothing to add or the document would grow past 2 GB.
u32  fude_zoom_pdf_rgba(fude_zoom_pdf* _pdf, const u8* _rgba, u32 _width, u32 _height);
// Draws image _id on the unit square mapped by this matrix (as fude_zoom_pdf_transform takes it): (_w, 0, 0, _h, _x,
// _y) puts it upright, _w by _h, its bottom-left corner at (_x, _y) and its top row at the top. It is painted through
// the fill's alpha (PDF's /ca covers images too), so set an opaque fill colour first for an opaque picture. An unknown
// id draws nothing.
void fude_zoom_pdf_image(fude_zoom_pdf* _pdf, u32 _id, f64 _a, f64 _b, f64 _c, f64 _d, f64 _e, f64 _f);

// --- fonts -----------------------------------------------------------------------------------------

#define FUDE_ZOOM_PDF_FACES    4u           // a text's faces: 0 regular, 1 bold, 2 italic, 3 bold italic
#define FUDE_ZOOM_PDF_CHAIN    6u           // the font files a face tries for a character, at most
#define FUDE_ZOOM_PDF_CFF_MOST (2u << 20)   // a CFF font's CFF table, at most (bytes): it is embedded whole

// The font files text is written in. chains[f] is face f's, in the order the screen tries them for a character (the
// face's own font, then its fallbacks), as paths fude_file_read takes ("assets/fonts/Roboto-Regular.ttf"); the first
// NULL or "" ends a face's list. A file in several lists is read once.
typedef struct {
    const c8* chains[FUDE_ZOOM_PDF_FACES][FUDE_ZOOM_PDF_CHAIN];
} fude_zoom_pdf_font_files;

// Sets the fonts text is written in from now on (the paths are copied): for the documents made after (each keeps the
// fonts set when it was made) and for fude_zoom_pdf_text_width and _writes. NULL goes back to Helvetica. A file is
// read the first time a character is looked for in it, and let go again whenever no document made with these fonts is
// left (and once they are set again and their last document is freed), so the fonts take memory only while PDFs are
// made. A file that cannot be read, is not OpenType with TrueType or CFF outlines, or is a CFF over
// FUDE_ZOOM_PDF_CFF_MOST, is left out of its chains (logged). A face whose first font is left out takes the regular
// face's fonts, as the screen draws a face it could not load in the regular font; with the regular face's first left
// out too, text is Helvetica. Main thread only.
void fude_zoom_pdf_fonts(const fude_zoom_pdf_font_files* _files);

// --- text ------------------------------------------------------------------------------------------

// Writes UTF-8 text in the regular face, _size points high, in colour _c, its baseline starting at (_x, _y). The fill
// colour is left as it was.
void fude_zoom_pdf_text(fude_zoom_pdf* _pdf, f64 _x, f64 _y, f64 _size, rde_color _c, const c8* _utf8);
// The same in face _face: 0 regular, 1 bold, 2 italic, 3 bold italic (any other value is regular). Each font used is
// written into the file once, at the end (in Helvetica: Helvetica, Helvetica-Bold, -Oblique and -BoldOblique).
void fude_zoom_pdf_text_face(fude_zoom_pdf* _pdf, f64 _x, f64 _y, f64 _size, rde_color _c, const c8* _utf8, u8 _face);
// Returns how wide that text is at _size points in the fonts set now (each character its glyph's advance, no kerning),
// or in Helvetica's widths when it is written in Helvetica (any face: its regular widths). fude_zoom_pdf_text_width is
// the regular face's.
f64  fude_zoom_pdf_text_width(f64 _size, const c8* _utf8);
f64  fude_zoom_pdf_text_width_face(f64 _size, const c8* _utf8, u8 _face);
// Returns whether every character of _utf8 prints as written in the regular face: one of its fonts set now has it (or
// it is one the screen draws as nothing), or, in Helvetica, WinAnsiEncoding has it (Latin-1 and the extras above).
// False when one would come out as the missing-glyph box or a '?' (Japanese past the fonts' few, say), and
// for NULL. "" is true.
b8   fude_zoom_pdf_text_writes(const c8* _utf8);

// --- the file --------------------------------------------------------------------------------------

// Appends the whole file to _out, ending a page still open. Returns false, with nothing appended, when there are no
// pages or the document grew past 2 GB.
b8   fude_zoom_pdf_finish(fude_zoom_pdf* _pdf, fude_bytes* _out);

#endif
