// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SHEET_H
#define FUDE_ZOOM_SHEET_H

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/scene.h"

// ===========================================================================
// MEASURED SHEETS (shape.h's SHEET; Sketching's Insert → Sheet): a rectangle of
// a size typed exactly — a cutting mat, a sheet of plywood, a printer's bed, a
// drawing's paper — with a ruler along each of its four sides in real units,
// numbered from its bottom left corner (x along, y up: a CAD drawing's origin
// and a 3D printer's bed's), and, if it likes, a grid (what the pen snaps to on
// it). Stretched by a side, it says its new length as it goes; tapped, its card.
//
// What is drawn on it is at its TRUE SIZE, as everything on the canvas is: its
// scale (1:N) is how it prints — Export's Sheets puts each on a page N times
// smaller (A3 at 1:50 is a sheet 21 × 14.85 m, printed on A3).
//
// Its numbers: half-width half-height scale flags (its own units; scale N of
// 1:N; FUDE_ZOOM_SHEET_ bits).
//
// Its marks follow the zoom as a ruler's do: ticks at least 7 points apart (a
// millimetre's, or a finer or coarser step: 1, 2 or 5 of a power of ten, or an
// inch's halvings), every so many longer and numbered; the grid's lines at least
// FUDE_ZOOM_SHEET_GRID_PT apart, the numbered ones stronger.
// ===========================================================================

#define FUDE_ZOOM_SHEET_GRID    1u      // its grid shown (and snapped to)
#define FUDE_ZOOM_SHEET_TICK_PT 7.0     // ticks this far apart on the screen at least
#define FUDE_ZOOM_SHEET_GRID_PT 14.0    // ...the grid's lines
#define FUDE_ZOOM_SHEET_RULERS  56.0    // its smaller side on the screen under this: no rulers (too small to read)
#define FUDE_ZOOM_SHEET_NUMBERS 150.0   // ...under this: their ticks, not numbered (the sides' numbers would meet)

typedef struct {
    f64 hw, hh;      // half sizes, its own units
    f64 scale;       // N of 1:N (1: printed at its true size)
    u32 flags;       // FUDE_ZOOM_SHEET_
} fude_zoom_sheet;

// Its numbers into _n (4 of room). How many.
u32  fude_zoom_sheet_numbers(f64* _n, f64 _hw, f64 _hh, f64 _scale, u32 _flags);
// ...and back (a scale under 1 or not a number: 1). False: not a sheet's.
b8   fude_zoom_sheet_of(const f64* _n, u32 _count, fude_zoom_sheet* _out);

// A ruler's step at a zoom: ticks _step_mm apart, every _major-th numbered (in _unit, _per_mm a number),
// every _mid-th middling (0: none); _halves: an inch's, each halving shorter.
typedef struct {
    f64       step_mm;
    u32       major;
    u32       mid;
    b8        halves;
    f64       per_mm;
    const c8* unit;
} fude_zoom_sheet_step;

// The step for marks at least _min_points apart, _mm_per_point millimetres a screen point, in inches and
// feet (_inch) or metric. False: no zoom to measure at.
b8   fude_zoom_sheet_step_for(f64 _mm_per_point, f64 _min_points, b8 _inch, fude_zoom_sheet_step* _out);
// Tick _i's length (points): a numbered one's 14, shorter for each lesser one.
f64  fude_zoom_sheet_tick(const fude_zoom_sheet_step* _s, u64 _i);
// A number as a ruler writes it: "12", "2.5" (no trailing zeros).
void fude_zoom_sheet_number(f64 _v, c8* _out, usize _size);

// A sheet's marks on the screen (centre origin, Y up, points): its ticks and the grid's lines, and its
// rulers' numbers — each where its tick ends (_at) and the way into the sheet (_in: a number goes that way
// from there) — and its unit's name by its bottom left corner (_unit_at: the corner's, _unit_in: inwards
// both ways). _all its own units → the screen, _mm_per_unit millimetres one of its own, _view the screen
// (what is outside it left out).
typedef struct {
    fude_zoom_v2 a, b;
    u8           weight;   // 0 a tick, 1 a grid's line, 2 a numbered one
} fude_zoom_sheet_line;

typedef struct {
    fude_zoom_v2 at, in;
    c8           text[24];
} fude_zoom_sheet_label;

void fude_zoom_sheet_marks(const fude_zoom_sheet* _sheet, fude_zoom_sim _all, f64 _mm_per_unit, b8 _inch, fude_zoom_box _view,
                           rde_arr* _lines, rde_arr* _labels, fude_zoom_sheet_label* _unit);

// Where a point (its own units) lands on its grid at a zoom (the grid as it shows, from its bottom left
// corner): the nearest crossing. False: no grid shown there (not a grid sheet's, or the point off it).
b8   fude_zoom_sheet_grid_snap(const fude_zoom_sheet* _sheet, f64 _mm_per_unit, f64 _mm_per_point, b8 _inch, fude_zoom_v2 _p, fude_zoom_v2* _out);
// A length (mm) to its ruler's step at a zoom (stretching a side by hand: the nearest tick).
f64  fude_zoom_sheet_round(f64 _mm, f64 _mm_per_point, b8 _inch);

// Papers (their sizes in millimetres, upright): the card's choices.
typedef enum {
    FUDE_ZOOM_PAPER_A4 = 0,
    FUDE_ZOOM_PAPER_A3,
    FUDE_ZOOM_PAPER_A2,
    FUDE_ZOOM_PAPER_A1,
    FUDE_ZOOM_PAPER_A0,
    FUDE_ZOOM_PAPER_LETTER,
    FUDE_ZOOM_PAPER_TABLOID,
    FUDE_ZOOM_PAPER_COUNT
} FUDE_ZOOM_PAPER_;
void fude_zoom_paper_size(u32 _paper, f64* _w_mm, f64* _h_mm);
// The paper _w_mm × _h_mm is (either way up, to half a millimetre); FUDE_ZOOM_NONE: none of them.
u32  fude_zoom_paper_find(f64 _w_mm, f64 _h_mm);

#endif
