// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PAPER
#define FUDE_ZOOM_PAPER

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Sketching's own papers (canvas.h' FUDE_PAPER_ past the core's four, 0.1.86) as
// lines in a view, the page drawing them (page.c) as strongly as each says:
//
//   isometric    lines three ways, 60° apart (vertical, and 30° either side of
//                level): equilateral triangles, to draw things in 3D;
//   hexagons     a honeycomb, pointed at the top;
//   polar        rings round the middle and rays every 15°;
//   engineering  graph paper: a line every step, stronger every fifth, strong
//                and heavy every tenth — its steps tens of each other (1 mm,
//                1 cm…), whatever the zoom;
//   music        staves of five lines, a step apart, six steps between them;
//
// and a notebook page's own layouts: Cornell's (a cue column down its left, its
// notes ruled, a summary at its foot) and a planner's (a title, a box to tick at
// the start of each ruled line).
//
// The endless lattice's papers come twice as fine as they zoom in: their own
// lines a step apart, finer ones fading in between (as the dots and squares).
// ===========================================================================

typedef enum {
    FUDE_ZOOM_PAPER_PLAIN = 0,   // a line in the paper's ink
    FUDE_ZOOM_PAPER_MARGIN,      // ruled paper's margin line: a faint red
    FUDE_ZOOM_PAPER_DOT,         // a dot at a (b the same)
} FUDE_ZOOM_PAPER_LINE_;

typedef struct {
    fude_zoom_v2 a, b;
    f32          strength;   // how strongly (0 to 1): the paper's own lines 1, finer ones as far as they have come in
    b8           heavy;      // drawn thicker (engineering's tenth, a page layout's own lines)
    u8           kind;       // FUDE_ZOOM_PAPER_LINE_
} fude_zoom_paper_line;

// Paper _paper's lines in _view (its own units: Y up), round its middle (0, 0): its own lattice _step apart, a finer
// one (half as far apart; hexagons: half as big) _fade strong (0 to 1); a unit _px points on screen (how smooth a ring
// is, how fine engineering's lines go). Music: its staves' lines _step apart, its first staff's top line on y = 0.
// How many added to _out (fude_zoom_paper_line; FUDE_ZOOM_PAPER_MOST at most).
#define FUDE_ZOOM_PAPER_MOST 20000u
u32 fude_zoom_paper_lines(u8 _paper, fude_zoom_box _view, f64 _step, f64 _fade, f64 _px, rde_arr* _out);

// A notebook page's own layout (Cornell, planner) on the page _page (Y up): its lines ruled _step apart from _top down
// (its margin's line), its own lines heavy. How many added to _out; 0: not a page layout.
u32 fude_zoom_paper_page(u8 _paper, fude_zoom_box _page, f64 _top, f64 _step, rde_arr* _out);

// A notebook's page _page (Y up) as it is printed: the whole of its paper _paper (any, the core's four too) at its true
// spacing — ruled _ruled apart from _margin under its top (its margin line _margin in from its left), squares and dots
// _grid apart from its top left, the patterns' _grid apart round its middle, music's staves' lines _staff apart from
// under its margin — no finer lines between. How many added to _out.
u32 fude_zoom_paper_sheet(u8 _paper, fude_zoom_box _page, f64 _margin, f64 _ruled, f64 _grid, f64 _staff, rde_arr* _out);

// Is it a page layout (Cornell, planner: on the endless canvas, ruled lines)?
b8  fude_zoom_paper_is_page(u8 _paper);

#endif
