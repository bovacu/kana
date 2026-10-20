// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SHEET_H
#define FUDE_ZOOM_SHEET_H

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/scene.h"

// ===========================================================================
// Sheet: measured sheets (shape.h's SHEET; Insert, Measured sheet). A sheet is a rectangle of a size typed
// exactly, such as a cutting mat, a sheet of plywood, a printer's bed or a drawing's paper, with a ruler along each
// of its four sides in real units and, if wanted, a grid that the pen snaps to.
//
// Where it sits: zoom, the canvas apps' layer. The renderer (render_shape.c) and the exports (export.c) draw sheets
// with it, snapping (snap.h) uses its grid, and the sheet's card (sheetform.h) chooses its paper and scale.
// Its main types and their lifecycle: plain values (fude_zoom_sheet, fude_zoom_sheet_step) and arrays of lines and
// labels the caller creates and frees.
// Threads: called on the main thread. It keeps no state between calls.
// Files: a sheet is kept in a SHEET shape's numbers (fude_zoom_sheet_numbers): half-width, half-height (its own
//   units), scale (N of 1:N), flags (FUDE_ZOOM_SHEET_ bits). Numbers with only the two half sizes still read, as a
//   sheet at 1:1 with no flags.
// Design:
//   The rulers are numbered from the sheet's bottom left corner, x along and y up, because that is a CAD drawing's
//   origin and a 3D printer bed's. Stretched by a side, a sheet says its new length as it goes; tapped, it opens its
//   card.
//   What is drawn on a sheet is at its TRUE SIZE, as everything on the canvas is. Its scale (1:N) is only how it
//   prints: Export's Sheets puts each on a page N times smaller (A3 at 1:50 is a sheet 21 × 14.85 m, printed on A3).
//   Its marks follow the zoom as a ruler's do: ticks at least 7 points apart (a millimetre's, or a finer or coarser
//   step of 1, 2 or 5 times a power of ten, or an inch's halvings), every so many of them longer and numbered. The
//   grid's lines are at least FUDE_ZOOM_SHEET_GRID_PT apart, the numbered ones stronger.
//   See docs/infinite_canvas_design.md §24.
// ===========================================================================

#define FUDE_ZOOM_SHEET_GRID    1u      // flag: its grid is shown and snapped to
#define FUDE_ZOOM_SHEET_TICK_PT 7.0     // the least distance between ticks on the screen (points)
#define FUDE_ZOOM_SHEET_GRID_PT 14.0    // the least distance between the grid's lines on the screen (points)
#define FUDE_ZOOM_SHEET_RULERS  56.0    // with its smaller side shorter than this on screen, no rulers (unreadable)
#define FUDE_ZOOM_SHEET_NUMBERS 150.0   // shorter than this, ticks but no numbers (the sides' numbers would meet)
#define FUDE_ZOOM_SHEET_BAND_PT 36.0    // the width of the band under a ruler stuck to the screen's edge (points)

// A sheet, as read from its numbers.
typedef struct {
    f64 hw, hh;      // half sizes, in its own units
    f64 scale;       // N of 1:N (1: printed at its true size)
    u32 flags;       // FUDE_ZOOM_SHEET_ bits
} fude_zoom_sheet;

// Writes a sheet's numbers into _n, which needs room for 4, and returns 4. Half sizes are made positive, and a scale
// under 1 or not finite is written as 1.
u32  fude_zoom_sheet_numbers(f64* _n, f64 _hw, f64 _hh, f64 _scale, u32 _flags);
// Reads a sheet back from _count numbers into _out. A missing scale, or one under 1 or not finite, reads as 1; missing
// flags read as 0. Returns false when there are fewer than 2 numbers, so they are not a sheet's.
b8   fude_zoom_sheet_of(const f64* _n, u32 _count, fude_zoom_sheet* _out);

// A ruler's step at one zoom.
typedef struct {
    // The distance between ticks (millimetres).
    f64       step_mm;
    // Every major-th tick is numbered, and every mid-th is middling (mid 0: none).
    u32       major;
    u32       mid;
    // True for an inch's halvings, each halving's tick shorter than the last.
    b8        halves;
    // How many millimetres make one of the unit the numbers are written in, and that unit's name ("mm", "in"...).
    f64       per_mm;
    const c8* unit;
} fude_zoom_sheet_step;

// Finds the step for marks at least _min_points apart when a screen point is _mm_per_point millimetres, in inches and
// feet (_inch) or metric, and writes it to _out. Metric steps are 1, 2 or 5 times a power of ten millimetres, numbered
// in mm, cm or m. Inch steps are halvings down to a sixteenth with the inches numbered, and zoomed out, inches with
// each foot numbered, then three and six inches, then feet in 1, 2 or 5 times a power of ten. Returns false when
// _mm_per_point is not a finite number above 0.
b8   fude_zoom_sheet_step_for(f64 _mm_per_point, f64 _min_points, b8 _inch, fude_zoom_sheet_step* _out);
// Returns tick _i's length in points: 14 for a numbered one, shorter for each lesser one (10 for a middling one, 6 for
// the rest; an inch's halvings 2.5 shorter each level, 4.5 at least).
f64  fude_zoom_sheet_tick(const fude_zoom_sheet_step* _s, u64 _i);
// Writes _v as a ruler writes it, to four decimals at most with no trailing zeros: "12", "2.5", never "-0".
void fude_zoom_sheet_number(f64 _v, c8* _out, usize _size);

// A line of a sheet's marks on the screen, from a to b (screen points, centre origin, y up).
typedef struct {
    fude_zoom_v2 a, b;
    u8           weight;   // 0 a tick, 1 a grid's line, 2 a numbered grid line
} fude_zoom_sheet_line;

// A ruler's number on the screen: at is where its tick ends (a little further in), in is the way into the sheet, so
// the number is written from at in that direction.
typedef struct {
    fude_zoom_v2 at, in;
    c8           text[24];
} fude_zoom_sheet_label;

// Makes a sheet's marks on the screen: its ticks and grid lines into _lines (fude_zoom_sheet_line) and its rulers'
// numbers into _labels (fude_zoom_sheet_label), both cleared first, and its unit's name by its bottom left corner
// into *_unit (zeroed first; set only when the rulers are numbered; may be NULL). _all maps its own units to the
// screen (centre origin, y up, points), _mm_per_unit is how many millimetres one of its units is, and _view is the
// screen, outside which marks are left out. Makes nothing for a sheet with no size.
void fude_zoom_sheet_marks(const fude_zoom_sheet* _sheet, fude_zoom_sim _all, f64 _mm_per_unit, b8 _inch, fude_zoom_box _view,
                           rde_arr* _lines, rde_arr* _labels, fude_zoom_sheet_label* _unit);
// As fude_zoom_sheet_marks, but with _bands given it makes only the rulers STUCK to the screen's edges, drawn over
// everything: zoomed in, with the sheet's own top or left ruler off the screen, its top ruler runs along the screen's
// top and its left one down the screen's left, still counting from its corner, the number nearest the screen's middle
// saying its unit. Each sits on a band: _bands gets up to two screen boxes to draw under them, and *_band_count how
// many. Only a sheet square to the screen (not turned) gets stuck rulers.
void fude_zoom_sheet_marks_stuck(const fude_zoom_sheet* _sheet, fude_zoom_sim _all, f64 _mm_per_unit, b8 _inch, fude_zoom_box _view,
                                 rde_arr* _lines, rde_arr* _labels, fude_zoom_sheet_label* _unit, fude_zoom_box* _bands, u32* _band_count);

// Snaps _p (the sheet's own units) to the nearest crossing of its grid as the grid shows at this zoom (counted from
// its bottom left corner) and writes it to *_out. Returns false when no grid shows there: the sheet has no grid, or
// _p is off the sheet.
b8   fude_zoom_sheet_grid_snap(const fude_zoom_sheet* _sheet, f64 _mm_per_unit, f64 _mm_per_point, b8 _inch, fude_zoom_v2 _p, fude_zoom_v2* _out);
// Returns _mm rounded to the ruler's step at this zoom, one step at least, so a side stretched by hand stops on a tick.
// Returns _mm unchanged when there is no zoom to measure at.
f64  fude_zoom_sheet_round(f64 _mm, f64 _mm_per_point, b8 _inch);

// The papers the sheet's card offers (their sizes in millimetres, upright).
typedef enum {
    FUDE_ZOOM_PAPER_A4 = 0,
    FUDE_ZOOM_PAPER_A3,
    FUDE_ZOOM_PAPER_A2,
    FUDE_ZOOM_PAPER_A1,
    FUDE_ZOOM_PAPER_A0,
    FUDE_ZOOM_PAPER_LETTER,
    FUDE_ZOOM_PAPER_TABLOID,
    // How many papers there are.
    FUDE_ZOOM_PAPER_COUNT
} FUDE_ZOOM_PAPER_;
// Writes paper _paper's size in millimetres, upright; an unknown paper gives A4's.
void fude_zoom_paper_size(u32 _paper, f64* _w_mm, f64* _h_mm);
// Returns the paper that is _w_mm × _h_mm, either way up, to half a millimetre, or FUDE_ZOOM_NONE when none is.
u32  fude_zoom_paper_find(f64 _w_mm, f64 _h_mm);

#endif
