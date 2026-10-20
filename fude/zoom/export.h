// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_EXPORT
#define FUDE_ZOOM_EXPORT

#include "rde.h"
#include "drawing/base/kfile.h"
#include "zoom/scene.h"
#include "drawing/base/theme.h"
#include "zoom/units.h"
#include "zoom/rich.h"

// ===========================================================================
// Export: what is on screen, taken out of the app as a file (research §6; Phase 1's export of the view).
//
//   PNG  the view drawn again off screen, larger: page_export.c draws it with the renderer into a render texture, reads
//        it back the frame after, and RDE encodes it. Nothing of it is here.
//   SVG  the view as a drawing, written here: strokes as paths (their width followed in runs where the pen's pressure
//        changes it), shapes as their outlines, filled or not, fills as paths with what the eraser took masked out,
//        pictures as themselves (their own JPEG or PNG inside), and the frames inside drawn in their places: the same
//        frames the renderer would draw, from three levels up down to half a point across.
//   PDF  the same, as a page the size of the screen (pdf.h); page_export.c drives it. Also several views a page each,
//        pages printed to scale, and a template tiled across sheets of paper at its true size.
//   DXF  for a CNC router or a laser (dxf.h): the view in millimetres, each stroke's centreline and each shape's
//        outline a path, each fill's outline and the eraser's cuts in it closed paths, a layer for each colour drawn
//        with, and what a pre-flight finds before cutting.
//   CSV  the cut list: every board in the document, counted by part.
//
// Where it sits: zoom, the canvas apps' layer. The page (page_export.c) calls the exports and sets the settings below;
// stl.h uses the same walk for its 3D model.
// Its main types and their lifecycle: fude_zoom_export_sink is a table of callbacks the caller fills in on the stack.
// The exports write into the caller's fude_bytes; apart from the settings below they keep nothing between calls.
// Threads: main thread only. The settings (fude_zoom_export_measure, _units, _formulas, _only) are module statics that
// every later export reads, and the walk reads the active theme.
// Files: none written here: each export appends its file's bytes to a fude_bytes, which the page saves or shares.
// Formats: SVG as UTF-8 XML; PDF through pdf.h; DXF R12 through dxf.h; CSV with CRLF line ends.
// Design: every export walks the scene the same way (fude_zoom_export_walk), each with a sink of its own that is
// handed what to draw as screen geometry, so all of them show what the renderer shows. Each shows the screen as the
// camera has it, _half each way (screen points), with its paper's colour behind (docs/infinite_canvas_design.md §15).
// ===========================================================================

// Flags added to a text's style (FUDE_ZOOM_TEXT_) when it is handed on: each of its lines centred in its box's width
// (a diagram symbol's text, as the screen has it).
#define FUDE_ZOOM_EXPORT_TEXT_CENTRED 0x80u
// Or each line against its box's right edge (a bracket's text, on its left).
#define FUDE_ZOOM_EXPORT_TEXT_RIGHT   0x40u
// Its letters bold, slanted, or both (a note's words: rich.h).
#define FUDE_ZOOM_EXPORT_TEXT_BOLD    0x20u
#define FUDE_ZOOM_EXPORT_TEXT_ITALIC  0x10u
#define FUDE_ZOOM_EXPORT_TEXT_FLAGS   0xF0u   // These four bits; the text's own style is in the bits under them.

// What the walk hands on, in the order the renderer draws it, as points of the screen (origin at its centre, Y up, in
// points). Any callback may be NULL when that kind of thing is not wanted.
typedef struct {
    // The sink's own state, handed back to each callback.
    any self;
    // A stroke: its centreline and each point's half-width (one point: a dot).
    void (*stroke)(any _self, const fude_zoom_v2* _p, const f64* _r, u32 _n, rde_color _color);
    // A shape: its outline (closed or not), its line's half-width, and its fill (alpha 0 when it has none).
    void (*shape)(any _self, const fude_zoom_v2* _p, u32 _n, b8 _closed, f64 _radius, rde_color _line, rde_color _fill);
    // A fill: its points with each one's ring (the first's ring is the outline; any other ring is cut out of it).
    void (*fill)(any _self, const fude_zoom_v2* _p, const u32* _rings, u32 _n, rde_color _color);
    // A picture: its file's bytes (JPEG or PNG; 4 of them at least, the walk hands over no fewer), its half-size, and
    // the way from its own units (round its middle) to the screen.
    void (*image)(any _self, const u8* _bytes, u32 _size, f64 _hw, f64 _hh, fude_zoom_sim _to);
    // A text: its box's top-left corner, its letters' height and its box's size (screen points), its style (a note or
    // not: FUDE_ZOOM_TEXT_, with the flags above), its ink and paper, and its lines (_count of them, each NUL-ended;
    // a note's already wrapped to its width, near enough).
    void (*text)(any _self, fude_zoom_v2 _top_left, f64 _px, f64 _w, f64 _h, u8 _style, rde_color _ink, rde_color _paper, const c8* const* _lines, u32 _count);
    // True for a blade or a beam (DXF, STL): what only shows how big things are (a sheet, which is the stock and not a
    // cut; a dimension) is left out.
    b8 cutting;
    // Told first which object the geometry that follows belongs to (NULL when not wanted). The 3D model (stl.h) reads a
    // board's thickness this way.
    void (*object)(any _self, const fude_zoom_scene* _s, u32 _object);
} fude_zoom_export_sink;

// Sets how a note's words are measured as it is laid out (rich.h): the app's fonts' widths (the page passes render.h's
// fude_zoom_render_measure with its renderer as _self). Until it is set, or with NULL, each letter is taken as half its
// height wide.
void fude_zoom_export_measure(fude_zoom_rich_measure_fn _fn, any _self);

// Sets how lengths are written (a dimension's number, a sheet's rulers): _mm_per_unit millimetres to a unit of the
// home frame, in the units style _units (copied). Until it is set, no lengths are written.
void fude_zoom_export_units(f64 _mm_per_unit, const fude_zoom_units_style* _units);

// The formulas' type (formula.h), named here without including formula.h.
struct fude_zoom_formulas;
// Sets how formulas (FUDE_ZOOM_TEXT_FORMULA) are written: as they are drawn, their glyphs' outlines, set with
// _formulas (the page's, formula.h; it must stay alive while it is set). Until it is set, or with NULL, a formula is
// written as its LaTeX.
void fude_zoom_export_formulas(struct fude_zoom_formulas* _formulas);

// Exports only what the lasso holds: from now on only the objects _keep marks are walked (a byte for each object
// index, _count of them; 1 means it is written; a frame's object marked 1 is walked into, and what is in it is
// filtered by _keep too), until it is set again with NULL. _keep is not copied: it must stay valid until then. The
// page marks what is held, everything inside a frame held, and the frames down to them.
void fude_zoom_export_only(const u8* _keep, u32 _count);

// Walks the view as the camera has it, _half each way (screen points), through _sink, starting from the highest frame
// that is still drawn exactly (at most three levels up), as the renderer does. "The theme's ink" is handed on as the
// active theme's ink, or with fude_zoom_export_walk_as as _theme's (a print's: the paper theme's, on white).
void fude_zoom_export_walk(const fude_zoom_scene* _s, fude_zoom_v2 _half, const fude_zoom_export_sink* _sink);
void fude_zoom_export_walk_as(const fude_zoom_scene* _s, fude_zoom_v2 _half, const fude_zoom_export_sink* _sink, const fude_theme* _theme);

// Appends the view as an SVG to _out, on a background of _paper. Returns false only when _out is still empty after.
b8 fude_zoom_export_svg(const fude_zoom_scene* _s, fude_zoom_v2 _half, rde_color _paper, fude_bytes* _out);

// Appends the view as a PDF to _out: one page the size of the screen, a screen point to a PDF point, its paper behind.
// Returns false when nothing was written.
b8 fude_zoom_export_pdf(const fude_zoom_scene* _s, fude_zoom_v2 _half, rde_color _paper, fude_bytes* _out);

// Appends a PDF of several views to _out, a page each (the areas): page _i is what camera _views[_i] sees on a
// screen _halves[_i] each way (points), its paper behind. The scene's camera is moved for each and put back after.
// Returns false when nothing was written (_n 0, or the PDF could not be made).
b8 fude_zoom_export_pdf_views(fude_zoom_scene* _s, rde_color _paper, const fude_zoom_camera* _views, const fude_zoom_v2* _halves, u32 _n, fude_bytes* _out);

// Appends a PDF of views printed to scale (the sheets) to _out: page _i is what camera _views[_i] sees on a
// page _halves[_i] each way (in page points: what it shows at its scale), on white, in the paper theme's ink. The
// printer's own error is undone as in fude_zoom_export_tiles (_printer). _papers is NULL, or _n arrays of paper.h's
// fude_zoom_paper_line (in each camera's frame's units): page _i's paper, drawn under its ink (a notebook's:
// fude_zoom_paper_sheet). The scene's camera is moved for each and put back after. Returns false when nothing was
// written.
b8 fude_zoom_export_pdf_pages(fude_zoom_scene* _s, const fude_zoom_camera* _views, const fude_zoom_v2* _halves, u32 _n, fude_zoom_v2 _printer,
                              const rde_arr* _papers, fude_bytes* _out);

// Appends a template printed at its true size (research §2.10, item 8) to _out as a PDF: the view in millimetres
// (_mm_per_point to a screen point) across pages _page_w_mm by _page_h_mm. Each page leaves 10 mm round it that a
// printer may not reach and shares 10 mm with the next (dashed), with registration crosses at its corners and its
// label (rows by letter, columns by number); the first also has a 100 mm square to check the printer's scale by, when
// it fits, with _square_note printed by it (the caller's words, in the app's language, for "1:1, check the square:
// 100 mm a side"; NULL or "" for none). The pages are exactly the paper's size, so nothing scales them to fit.
// _printer: how long a millimetre comes out when this printer prints it, across and down; it is undone round each
// page's middle so the print measures true (a value outside 0.5 to 2, such as 0, means the printer is taken as exact).
// *_pages (may be NULL) gets how many pages. Returns false when nothing was written, or when it would take more than 64
// pages or 26 rows: zoom in. The PDF's text is in the fonts pdf.h was given (fude_zoom_pdf_fonts), or Helvetica's.
b8 fude_zoom_export_tiles(const fude_zoom_scene* _s, fude_zoom_v2 _half, f64 _mm_per_point, f64 _page_w_mm, f64 _page_h_mm, fude_zoom_v2 _printer,
                          const c8* _square_note, fude_bytes* _out, u32* _pages);

// The printer's check, a page to print at 100% and measure: a line FUDE_ZOOM_CHECK_X_MM long across and one
// FUDE_ZOOM_CHECK_Y_MM long down, with a tick each centimetre. What they measure over what they should is the
// printer's factor (_printer above). A page of 210 x 279.4 mm prints whole on both A4 and Letter.
#define FUDE_ZOOM_CHECK_X_MM 150.0
#define FUDE_ZOOM_CHECK_Y_MM 200.0
// Appends the printer's check, one page _page_w_mm by _page_h_mm, to _out as a PDF: _title over it and _how under that
// (the caller's words, in the app's language: "Printer check", and how to print it and what to measure; NULL or "" for
// none). Returns false when nothing was written.
b8 fude_zoom_export_print_check(f64 _page_w_mm, f64 _page_h_mm, const c8* _title, const c8* _how, fude_bytes* _out);

// What a pre-flight found in a cutting file (dxf.h's fude_zoom_dxf_check; paths is its entities).
typedef struct {
    u32 paths, open_paths, duplicates, tiny_segments;
} fude_zoom_export_check;

// Appends the view as a DXF to _out, at _mm_per_point millimetres to a screen point, the theme's ink as black.
// *_check (may be NULL) gets what its pre-flight found (0.05 mm counts as meeting). Returns false when nothing was
// written. Pictures are left out.
b8 fude_zoom_export_dxf(const fude_zoom_scene* _s, fude_zoom_v2 _half, f64 _mm_per_point, fude_bytes* _out, fude_zoom_export_check* _check);

// Appends the cut list (research §2.10, item 9) to _out as a CSV a spreadsheet opens: every board shown in the
// document, at every depth, the same part counted once with its quantity, under _header's seven titles (part, quantity,
// length, width, thickness, material, grain). Lengths are in millimetres (_mm_per_unit to a unit of the home frame),
// rounded to a tenth, the longer side the length. The material is written with _material_words (shape.h's
// FUDE_ZOOM_MATERIAL_COUNT of them, the first, for none said, usually "") and the grain with _grain_words (along,
// across); the same part in two materials makes two rows. Returns how many boards there are (0 when there are none:
// only the header row is written).
u32 fude_zoom_export_cutlist(const fude_zoom_scene* _s, f64 _mm_per_unit, const c8* const* _header, const c8* const* _grain_words,
                             const c8* const* _material_words, fude_bytes* _out);

#endif
