// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_STL_H
#define FUDE_ZOOM_STL_H

#include "rde.h"
#include "drawing/base/kfile.h"
#include "zoom/scene.h"

// ===========================================================================
// A 3D model for a printer (Sketching's Export → 3D model): the view — or what
// the lasso holds — as solids, in millimetres, a binary STL a slicer opens.
//
// Every closed outline drawn (a rectangle, an ellipse, a polygon, a closed
// path, a fill, a board — a cut one with its holes) is a ring; rings inside
// rings take turns (as a laser's file does, even-odd): a ring in nothing is a
// solid's outside, one in that its hole, one in the hole a solid again... Each
// solid stands on the plate (z 0) as tall as its board is thick, or _thickness_mm
// for anything else. Open lines, text, pictures, what only says how big things
// are (dimensions, sheets) and diagrams' symbols are left out.
// ===========================================================================

typedef struct {
    u32 solids;      // how many
    u32 triangles;
} fude_zoom_stl_said;

// The view as the camera has it, _half each way (screen points), _mm_per_point millimetres a screen point, into _out
// (appended). False: nothing closed to make a solid of.
b8 fude_zoom_export_stl(const fude_zoom_scene* _s, fude_zoom_v2 _half, f64 _mm_per_point, f64 _thickness_mm, fude_bytes* _out, fude_zoom_stl_said* _said);

// Solids from rings (millimetres; _rings: each point's ring; _heights: each ring's height when it is a solid's
// outside), as above, into _out (f32 triangles: 9 numbers each, the three corners; facing out). How many triangles.
u32 fude_zoom_stl_solids(const fude_zoom_v2* _points, const u32* _rings, u32 _n, const f64* _heights, rde_arr* _out, u32* _solids);

#endif
