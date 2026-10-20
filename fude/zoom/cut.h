// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_CUT_H
#define FUDE_ZOOM_CUT_H

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/scene.h"

// ===========================================================================
// Cut: cutting a board (research §2.10, item 9). It works out what is left of a region (an outline with holes) once
// cuts are taken out of it, as the separate pieces it falls into. A saw's cut is a band as wide as its kerf along the
// line cut; a hole or a notch is the closed shape taken out. The same arrangement also gives Combine's union and
// intersection.
//
// Where it sits: zoom, the canvas apps' layer, over fill.h's rings and shape.h's boards. The page uses it for the Saw
// (page_pen.c), Combine (page_edit.c) and cutting offcuts from stock (page_boards.c); the partial eraser (erase.c) rubs
// wood out of a board with it.
// Threads: main thread only, since fude_zoom_cut_board changes the scene; the other functions keep no state.
//
// How it works: the region's rings (as fill.h has them: ring 0 the outline, the others holes) and the cuts (closed
// polygons, each taken out) are laid together. Every edge is split where another crosses it or ends on it; a piece of
// edge is kept where the material lies on one side of it only, turned so the material is on its left; the kept pieces
// are joined end to end into loops, counter-clockwise ones outlines and clockwise ones holes, each hole given to the
// smallest outline round it.
// Design: it is exact. Where two edges cross is worked out once and shared by both, so the loops close on the very
// same points; edges lying along each other (a cut snapped onto an edge) come out right, with neither side of them
// material (docs/infinite_canvas_design.md §16).
// ===========================================================================

// Returns the pieces of the region less the cuts. The region: _n points, each one's ring in _rings (runs of one ring;
// ring 0 the outline; NULL when it is all one ring). The cuts: _cut_n points, each one's cut in _cut_rings (runs of
// one; each a closed polygon taken out; NULL when it is all one cut). The pieces' points go into _out, each point's
// ring within its piece into _out_rings (0 its outline, counter-clockwise, then its holes, clockwise) and each point's
// piece into _out_piece; all three are cleared first. Returns how many pieces (0 when nothing is left).
u32 fude_zoom_cut_region(const fude_zoom_v2* _points, const u32* _rings, u32 _n, const fude_zoom_v2* _cut, const u32* _cut_rings, u32 _cut_n,
                         rde_arr* _out, rde_arr* _out_rings, rde_arr* _out_piece);

// The other ways the same arrangement can be laid together (Combine, page_edit.c), of the region's rings
// (ring 0 its outline, the others its holes) and the others' (each a closed polygon). What is kept:
//   SUBTRACT   what is in the region and in none of the others (fude_zoom_cut_region);
//   UNION      what is in the region or in any of the others;
//   INTERSECT  what is in the region and in one of the others.
typedef enum {
    FUDE_ZOOM_CUT_SUBTRACT = 0,
    FUDE_ZOOM_CUT_UNION,
    FUDE_ZOOM_CUT_INTERSECT
} FUDE_ZOOM_CUT_;
// As fude_zoom_cut_region, keeping what _op (FUDE_ZOOM_CUT_) says. Returns how many pieces (0 when nothing is kept, or
// the region has fewer than 3 points and _op is not UNION).
u32 fude_zoom_cut_combine(const fude_zoom_v2* _points, const u32* _rings, u32 _n, const fude_zoom_v2* _cut, const u32* _cut_rings, u32 _cut_n,
                          u8 _op, rde_arr* _out, rde_arr* _out_rings, rde_arr* _out_piece);
// Appends a saw's cut along an open line (_n points) to _out as a closed polygon: the band _width wide round it, its
// sides _width / 2 either side, mitred at its bends (at most four times out) and square at its ends (not carried past
// them). Returns how many points were added (0 when the line has fewer than two distinct points or _width is not
// positive).
u32 fude_zoom_cut_band(const fude_zoom_v2* _path, u32 _n, f64 _width, rde_arr* _out);

// Returns a polygon's signed area: positive when it goes counter-clockwise. The canvas's one shoelace summed from the
// first edge round to the closing one (cut.c, stl.c's rings and render_stroke.c's clip use it). It is not rde_math's
// (rde_math_polygon_signed_area_2d_f64), which sums from the closing edge: the same terms in another order round a
// nearly flat loop's area, and so its sign, which decides what a cut keeps, differently.
f64 fude_zoom_cut_area(const fude_zoom_v2* _p, u32 _n);

// Cuts _board (a BOARD shape, shape.h) by _cut: polygons in its frame's units, each one a run of _cut_rings (NULL when
// it is all one polygon), _n points in all. The pieces are made as new boards where it was, in its order, on its layer,
// with its look (its line style and hatch too), name, thickness and grain; a piece still a plain rectangle becomes a
// plain board (its sizes can be typed again). Their object indexes are appended to _born. The board itself is left as
// it is: the caller lets it go, in its own undo step. Returns how many pieces were made (0 when nothing is left of it),
// or FUDE_ZOOM_NONE when nothing was made: the cuts miss it or change nothing, it is not a live board, or the result
// does not add up (a guard against a lost loop).
u32 fude_zoom_cut_board(fude_zoom_scene* _s, u32 _board, const fude_zoom_v2* _cut, const u32* _cut_rings, u32 _n, rde_arr* _born);

#endif
