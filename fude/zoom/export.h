// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_EXPORT
#define FUDE_ZOOM_EXPORT

#include "rde.h"
#include "drawing/base/kfile.h"
#include "zoom/scene.h"
#include "drawing/base/theme.h"
#include "zoom/units.h"

// ===========================================================================
// What is on screen, out of the app (research §6, Phase 1's: the view):
//
//   PNG  the view drawn again off screen, larger (page.c: the renderer into a
//        render texture, read back the frame after and encoded by RDE).
//   SVG  the view as drawing, written here: strokes as paths (their width
//        followed in runs where the pen's pressure moves it), shapes as their
//        outlines, filled or not, fills as paths with what the eraser took
//        masked out, pictures as themselves (their own JPEG or PNG inside),
//        and the frames inside drawn in their places — the same frames the
//        renderer would draw, from three levels up down to half a point.
//   PDF  the same, as a page the size of the screen (pdf.h; page.c drives it).
//   DXF  for a CNC router or a laser (dxf.h): the view in millimetres — each
//        stroke's centreline and each shape's outline a path, each fill's
//        outline and the eraser's cuts in it closed paths — a layer for each
//        colour drawn with, and what a pre-flight finds before cutting.
//
// All of them walk the scene the same way (fude_zoom_export_walk), each with
// a sink of its own that is handed what to draw as screen geometry.
//
// Each is of the screen as the camera has it, _half each way (screen points),
// its paper's colour behind.
// ===========================================================================

// A text's style handed on (FUDE_ZOOM_TEXT_), with this: each of its lines centred in its box's width (a
// diagram symbol's, as the screen has it).
#define FUDE_ZOOM_EXPORT_TEXT_CENTRED 0x80u

// What the walk hands on, in the order the renderer draws it: points of the
// screen (centre origin, Y up, points). Any may be NULL (not wanted).
typedef struct {
    void* self;
    // A stroke's centreline and each point's half-width (one point: a dot).
    void (*stroke)(void* _self, const fude_zoom_v2* _p, const f64* _r, u32 _n, rde_color _color);
    // A shape: its outline (closed or not), its line's half-width, and its fill (alpha 0: none).
    void (*shape)(void* _self, const fude_zoom_v2* _p, u32 _n, b8 _closed, f64 _radius, rde_color _line, rde_color _fill);
    // A fill: its points with each one's ring (the first's ring the outline; any other ring is cut out of it).
    void (*fill)(void* _self, const fude_zoom_v2* _p, const u32* _rings, u32 _n, rde_color _color);
    // A picture: its file's bytes (JPEG or PNG), its half-size, and the way from its own units (round its middle) to the screen.
    void (*image)(void* _self, const u8* _bytes, u32 _size, f64 _hw, f64 _hh, fude_zoom_sim _to);
    // A text: its box's top left, its letters' height and its box's size (screen
    // points), a note or not (FUDE_ZOOM_TEXT_), its ink and paper, and its lines
    // (_lines of them, NUL-ended: a note's already wrapped to its width, near enough).
    void (*text)(void* _self, fude_zoom_v2 _top_left, f64 _px, f64 _w, f64 _h, u8 _style, rde_color _ink, rde_color _paper, const c8* const* _lines, u32 _count);
    // For a blade or a beam (DXF): what only shows how big things are — a sheet (the stock, not a cut), a
    // dimension — left out.
    b8 cutting;
    // Told first which thing what follows is of (NULL: not wanted): a 3D model's (stl.h) board's thickness.
    void (*object)(void* _self, const fude_zoom_scene* _s, u32 _object);
} fude_zoom_export_sink;

// Lengths as the exports write them (a dimension's number, a sheet's rulers): _mm_per_unit millimetres a unit of
// the home frame, in _units (copied). Until set, none are written.
void fude_zoom_export_units(f64 _mm_per_unit, const fude_zoom_units_style* _units);

// Exporting what the lasso holds: from now on only the objects _keep says are
// walked (a byte an object, _count of them; 1: written — a frame's object kept:
// walked into, what is in it as _keep says too), until set again with NULL.
// (page.c: what is held, everything inside a frame held, the frames down to them.)
void fude_zoom_export_only(const u8* _keep, u32 _count);

// The view as the camera has it, _half each way, through _sink; "the theme's
// ink" handed on as the active theme's, or (_as) as _theme's — a print's, on white.
void fude_zoom_export_walk(const fude_zoom_scene* _s, fude_zoom_v2 _half, const fude_zoom_export_sink* _sink);
void fude_zoom_export_walk_as(const fude_zoom_scene* _s, fude_zoom_v2 _half, const fude_zoom_export_sink* _sink, const fude_theme* _theme);

// The SVG of the view into _out (appended). False: nothing could be written.
b8 fude_zoom_export_svg(const fude_zoom_scene* _s, fude_zoom_v2 _half, rde_color _paper, fude_bytes* _out);

// The PDF of the view into _out (appended): one page the size of the screen,
// a point a point, its paper behind. False: nothing written.
b8 fude_zoom_export_pdf(const fude_zoom_scene* _s, fude_zoom_v2 _half, rde_color _paper, fude_bytes* _out);

// ...of several views, a page each (Sketching's areas): page _i the camera _views[i] sees on a screen _halves[i]
// each way (points), its paper behind. The camera is put back after. False: nothing written.
b8 fude_zoom_export_pdf_views(fude_zoom_scene* _s, rde_color _paper, const fude_zoom_camera* _views, const fude_zoom_v2* _halves, u32 _n, fude_bytes* _out);

// ...printed to scale (Sketching's sheets): page _i the camera _views[i] sees on a page _halves[i] each way
// (points: what it shows at its scale), on white in the paper theme's ink, the printer's own error undone
// (_printer: as fude_zoom_export_tiles). The camera is put back after. False: nothing written.
b8 fude_zoom_export_pdf_pages(fude_zoom_scene* _s, const fude_zoom_camera* _views, const fude_zoom_v2* _halves, u32 _n, fude_zoom_v2 _printer, fude_bytes* _out);

// A template printed at its true size (research §2.10, item 8): the view in
// millimetres (_mm_per_point a screen point) across pages of _page_w_mm by
// _page_h_mm, each with 10 mm round it a printer may not reach and 10 mm shared
// with the next (dashed), registration crosses at its corners, its label (rows
// by letter, columns by number) and, on the first, a 100 mm square to check
// the printer's scale by. Pages exactly the paper's size, so nothing scales
// them to fit. False: nothing written (or it would take more than 64 pages:
// zoom in). *_pages: how many. _printer: how long a millimetre this printer
// prints comes out, across and down (0 or 1: as it should), undone round each
// page's middle so the print measures true.
b8 fude_zoom_export_tiles(const fude_zoom_scene* _s, fude_zoom_v2 _half, f64 _mm_per_point, f64 _page_w_mm, f64 _page_h_mm, fude_zoom_v2 _printer, fude_bytes* _out, u32* _pages);

// The printer's check: one page with a line FUDE_ZOOM_CHECK_X_MM long across
// and one FUDE_ZOOM_CHECK_Y_MM long down, a tick a centimetre, to print at
// 100% and measure (what they measure over what they should is the printer's
// factor). 210 x 279.4 mm prints whole on both A4 and Letter. False: nothing written.
#define FUDE_ZOOM_CHECK_X_MM 150.0
#define FUDE_ZOOM_CHECK_Y_MM 200.0
b8 fude_zoom_export_print_check(f64 _page_w_mm, f64 _page_h_mm, fude_bytes* _out);

// What a pre-flight found in a cutting file (dxf.h's).
typedef struct {
    u32 paths, open_paths, duplicates, tiny_segments;
} fude_zoom_export_check;

// The DXF of the view into _out (appended), _mm_per_point millimetres a screen
// point; what its pre-flight found into _check (may be NULL). False: nothing written.
b8 fude_zoom_export_dxf(const fude_zoom_scene* _s, fude_zoom_v2 _half, f64 _mm_per_point, fude_bytes* _out, fude_zoom_export_check* _check);

// The cut list (research §2.10, item 9): every board shown in the document,
// at every depth, as a CSV a spreadsheet opens — the same part counted once with
// its quantity — under _header's seven titles (part, quantity, length, width,
// thickness, material, grain); lengths in millimetres (_mm_per_unit a unit of the
// home frame), the longer side the length; the material in _material_words (shape.h'
// FUDE_ZOOM_MATERIAL_COUNT of them, the first for none said: ""), the grain in
// _grain_words (along, across); the same part in two materials two rows.
// How many boards (0: none, a header only).
u32 fude_zoom_export_cutlist(const fude_zoom_scene* _s, f64 _mm_per_unit, const c8* const* _header, const c8* const* _grain_words,
                             const c8* const* _material_words, fude_bytes* _out);

#endif
