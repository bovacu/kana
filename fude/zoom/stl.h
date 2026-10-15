// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_STL_H
#define FUDE_ZOOM_STL_H

#include "rde.h"
#include "drawing/base/kfile.h"
#include "zoom/scene.h"

// ===========================================================================
// STL export: a 3D model for a printer (Sketching's Export → 3D model). The view, or what the lasso holds, becomes
// solids in millimetres, written as a binary STL that a slicer opens.
//
// Where it sits: zoom, the canvas apps' layer. The page (page_export.c) calls fude_zoom_export_stl; it walks the view
// with export.h's walk, as the other exports do.
// Threads: main thread only (it reads the scene, and export.h's settings are shared statics).
// Files: writes no file itself: the STL's bytes are appended to a fude_bytes. Binary STL: an 80-byte header, a u32
// triangle count, then each triangle as its facing normal and three corners (f32 x, y, z each, in millimetres) and a
// u16 0.
//
// Every closed outline drawn (a rectangle, an ellipse, a polygon, a closed path, a fill, a board, a cut board with its
// holes) is a ring. Rings inside rings take turns, even-odd, as a laser's file does: a ring inside nothing is a
// solid's outside, a ring in that is its hole, a ring in the hole is a solid again, and so on. Each solid stands on
// the plate (z = 0), as tall as its board is thick, or _thickness_mm for anything else. Open lines, text, pictures,
// what only says how big things are (dimensions, sheets) and diagrams' symbols are left out.
// Design: see docs/infinite_canvas_design.md §24.
// ===========================================================================

// What an export made (fude_zoom_export_stl).
typedef struct {
    u32 solids;      // How many solids.
    u32 triangles;
} fude_zoom_stl_said;

// Writes the view as the camera has it, _half each way (screen points), into _out (appended) as a binary STL, at
// _mm_per_point millimetres a screen point; what is not a board is _thickness_mm tall. *_said (may be NULL) gets the
// counts. _creator, the app's name, opens the file's 80-byte header (NULL: none). Returns false, writing nothing, when
// nothing closed was found to make a solid of.
b8 fude_zoom_export_stl(const fude_zoom_scene* _s, fude_zoom_v2 _half, f64 _mm_per_point, f64 _thickness_mm, const c8* _creator, fude_bytes* _out,
                        fude_zoom_stl_said* _said);

// Builds solids from rings, as above. _points are in millimetres, _n of them; _rings gives each point's ring number
// (each ring a run of one number), and _heights[ring number] is each ring's height when it is a solid's outside (a
// height not above 0 makes no solid). The triangles are appended to _out as f32s, nine to a triangle (its three
// corners), facing out. *_solids (may be NULL) gets how many solids were made. Returns how many triangles were added.
u32 fude_zoom_stl_solids(const fude_zoom_v2* _points, const u32* _rings, u32 _n, const f64* _heights, rde_arr* _out, u32* _solids);

#endif
