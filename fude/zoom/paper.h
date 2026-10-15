// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PAPER
#define FUDE_ZOOM_PAPER

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Paper: the canvas apps' own papers (drawing/ink/canvas.h's FUDE_PAPER_ values after the core's four, added in
// 0.1.86), worked out as lines in a view. The page (page_render.c) draws them, each line as strongly as it says, and
// exports print them (export.c).
//
//   isometric    lines three ways, 60° apart (vertical, and 30° either side of level): equilateral triangles, for
//                drawing things in 3D;
//   hexagons     a honeycomb, pointed at the top;
//   polar        rings round the middle and rays every 15°;
//   engineering  graph paper: a line every step, a stronger one every fifth, a strong and heavy one every tenth. Its
//                steps are powers of ten of each other (1 mm, 1 cm...), whatever the zoom;
//   music        staves of five lines a step apart, with six steps between one staff and the next;
//
// and two layouts of a notebook page: Cornell's (a cue column down its left, its notes ruled, a summary at its foot)
// and a planner's (a title, and a box to tick at the start of each ruled line).
//
// Where it sits: fude/zoom, pure geometry with no scene. The page draws the endless canvas's paper and a notebook's
// pages with it; export.c prints them.
// Its main types and their lifecycle: fude_zoom_paper_line is a plain value, added to an rde_arr the caller owns.
// Threads: it keeps no state, so any thread may call it.
// Design: the endless canvas's papers become twice as fine as you zoom in: their own lines stay a step apart, and
// finer ones fade in between them, as the core's dots and squares do. Lines are made whole and cut to the view
// (Liang–Barsky); rings are drawn as chords, only over the angles the view takes in.
// ===========================================================================

// What a line is, so the page draws it right.
typedef enum {
    FUDE_ZOOM_PAPER_PLAIN = 0,   // a line in the paper's ink
    FUDE_ZOOM_PAPER_MARGIN,      // the margin line of ruled paper, drawn a faint red
    FUDE_ZOOM_PAPER_DOT,         // a dot at a (b is the same point)
} FUDE_ZOOM_PAPER_LINE_;

// One line of paper.
typedef struct {
    fude_zoom_v2 a, b;
    f32          strength;   // how strongly, 0 to 1: the paper's own lines 1, finer ones as far as they have faded in
    b8           heavy;      // drawn thicker (engineering's tenth lines, a page layout's own lines)
    u8           kind;       // FUDE_ZOOM_PAPER_LINE_
} fude_zoom_paper_line;

// The most lines the paper functions leave in their output array: they stop adding once it holds this many.
#define FUDE_ZOOM_PAPER_MOST 20000u
// Adds to _out (an rde_arr of fude_zoom_paper_line) the lines of paper _paper inside _view, given in the paper's own
// units with Y up and the paper's middle at (0, 0). _step is how far apart its own lattice's lines are. _fade (0 to 1)
// is how strong the finer lattice is (half as far apart; for hexagons, half as big). _px is how many points one unit
// covers on screen, which sets how smooth a ring is and how fine engineering's lines go (engineering ignores _step).
// Music puts its staves' lines _step apart, the first staff's top line on y = 0. Returns how many lines it added;
// 0 for a paper that is not one of these, or a bad view or step.
u32 fude_zoom_paper_lines(u8 _paper, fude_zoom_box _view, f64 _step, f64 _fade, f64 _px, rde_arr* _out);

// Adds to _out the layout of a notebook page (Cornell or planner) on the page _page (Y up): its lines ruled _step
// apart from _top down (_top is the top margin's line, under its title), its own layout lines heavy. Returns how
// many lines it added; 0 when _paper is not a page layout.
u32 fude_zoom_paper_page(u8 _paper, fude_zoom_box _page, f64 _top, f64 _step, rde_arr* _out);

// Adds to _out the lines of a notebook's page _page (Y up) as it is printed: the whole of its paper _paper (any paper,
// the core's four too) at its true spacing, with no finer lines between. Ruled lines are _ruled apart from _margin
// under its top, and its margin line is _margin in from its left; squares and dots are _grid apart from its top left;
// the patterns are _grid apart round its middle; music's staves' lines are _staff apart from under its margin.
// Returns how many lines it added (0 for no paper).
u32 fude_zoom_paper_sheet(u8 _paper, fude_zoom_box _page, f64 _margin, f64 _ruled, f64 _grid, f64 _staff, rde_arr* _out);

// Returns whether _paper is a page layout (Cornell or planner). On the endless canvas, which has no pages, they show
// as ruled lines.
b8  fude_zoom_paper_is_page(u8 _paper);

// Returns how many whole numbers run from _k0 to _k1 (whole numbers held as f64: a view's edges in a lattice's steps,
// rounded in): 0 when none do, when more than _most + 1 would, or when either is 2^53 or more from 0 (a view that far
// out against its step, whose lines would all fall on each other). Whoever draws a lattice counts its lines with it
// and walks them by a u32 index from _k0, never with an f64 counter: past 2^53 adding 1 to one does not move it, and
// its loop would never end. Any thread.
u32 fude_zoom_paper_count(f64 _k0, f64 _k1, f64 _most);

#endif
