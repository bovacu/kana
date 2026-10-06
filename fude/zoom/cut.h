// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_CUT_H
#define FUDE_ZOOM_CUT_H

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/scene.h"

// ===========================================================================
// Cutting a board (research §2.10, item 9): what is left of a region — an
// outline with holes — once cuts are taken out of it, as the separate pieces
// it falls into. A saw's cut is a band as wide as its kerf along the line
// cut; a hole or a notch is the closed shape taken out.
//
// The region's rings (fill.h's: ring 0 the outline, the others holes) and the
// cuts (closed polygons, each taken out) are laid together; every edge is
// split where another crosses it or ends on it; a piece of edge is kept where
// the material lies on one side of it only, turned so the material is on its
// left; the kept pieces are joined end to end into loops — counter-clockwise
// ones outlines, clockwise ones holes, each hole given to the smallest outline
// round it. Exact: where edges cross is worked out once and shared by both, so
// the loops close on the very same points; edges lying along each other (a
// cut snapped onto an edge) come out right, neither side of them material.
// ===========================================================================

// The pieces of the region (_n points, each one's ring in _rings: runs of one
// ring, ring 0 the outline) less the cuts (_cut_n points, each one's cut in
// _cut_rings: runs of one, each a closed polygon taken out). Into _out (the
// pieces' points), _out_rings (each point's ring within its piece: 0 its
// outline, counter-clockwise, then its holes, clockwise) and _out_piece (each
// point's piece). How many pieces (0: nothing left).
u32 fude_zoom_cut_region(const fude_zoom_v2* _points, const u32* _rings, u32 _n, const fude_zoom_v2* _cut, const u32* _cut_rings, u32 _cut_n,
                         rde_arr* _out, rde_arr* _out_rings, rde_arr* _out_piece);

// A saw's cut along an open line (_n points): the band _width wide round it
// (its sides _width / 2 either side, mitred at its bends, square at its ends),
// a closed polygon appended to _out. How many points.
u32 fude_zoom_cut_band(const fude_zoom_v2* _path, u32 _n, f64 _width, rde_arr* _out);

// A polygon's signed area (counter-clockwise: positive).
f64 fude_zoom_cut_area(const fude_zoom_v2* _p, u32 _n);

// _board (a BOARD, shape.h) cut by _cut (polygons in its frame's units, each
// one a run of _cut_rings; NULL: one): its pieces made — new boards where it
// was, in its order, on its layer, with its look, name, thickness and grain; a
// piece still a plain rectangle a plain board (its sizes still to type) —
// their indexes appended to _born. The board itself is left as it is (the
// caller lets it go, in its own step). How many pieces (0: nothing left of
// it); FUDE_ZOOM_NONE: the cuts miss it (nothing made).
u32 fude_zoom_cut_board(fude_zoom_scene* _s, u32 _board, const fude_zoom_v2* _cut, const u32* _cut_rings, u32 _n, rde_arr* _born);

#endif
