// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_FILL
#define FUDE_ZOOM_FILL

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Fill: filling regions and the geometry under it (research §5.2; Phase 1's tap to fill). Closing gaps between
// several strokes came later, with the paint bucket (bucket.h).
//
//   A closed SHAPE tapped with the Fill tool takes the brush's colour as its fill (scene.h: FUDE_ZOOM_FLAG_FILLED and
//   the object's fill colour); tapped again in the same colour, it lets it go. No new geometry is made.
//   A closed STROKE (its gesture's ends near enough each other) gets a FILL object: the loop of its centreline round
//   the tap, drawn just under it, so the line stays on top and no seam shows between them.
//   The ERASER rubs a fill out where it passes: each step's capsule becomes a cut ring of the fill, taken out of its
//   outline.
//
// Where it sits: zoom, the canvas apps' layer. The page fills on a tap (page_pictures.c) and makes offsets
// (page_text.c); the erasers (erase.c) cut fills and strokes; the renderer (render_stroke.c), the exports (export.c,
// pdfview.c, stl.c), the bucket (bucket.c), the saw (cut.c), formulas and instruments use its triangles, rings and
// contours.
// Threads: none of its own and no shared state; it may be called on any thread with the caller's own arrays. In
// practice everything calls it on the main thread.
// Rings: many functions here take a polygon in rings: its points, and each point's ring number in _rings, the points
// of one ring in a run. Ring 0 is the outline (inside as even-odd says); every other ring is a cut, taken out of it.
// Design: a polygon's inside is even-odd (a figure 8 fills both its lobes; where a hand's loop runs over its start, the
// sliver there does not). It is drawn as triangles from a slab decomposition: the plane cut at every corner's height
// and every crossing's, and each slab's edges paired off left to right. That is exact for any shape with any crossings
// (docs/infinite_canvas_design.md §14, and §15 for the bucket).
// ===========================================================================

// Appends _n points (a closed polygon: the last joins the first) to _out as triangles (an rde_arr of fude_zoom_v2,
// three to a triangle). Returns how many triangles were added.
u32  fude_zoom_fill_triangulate(const fude_zoom_v2* _points, u32 _n, rde_arr* _out);
// Returns whether _p is inside the polygon (even-odd): rde_math's test (rde_math_point_in_polygon_2d_f64), never inside
// one of fewer than 3 points.
b8   fude_zoom_fill_inside(const fude_zoom_v2* _points, u32 _n, fude_zoom_v2 _p);

// A fill the eraser has been at: its points in rings, each point's ring in _rings (runs of one ring; NULL when all are
// ring 0). Ring 0 is the outline (even-odd); every other ring is a cut the eraser made (one step's capsule), taken out
// of it: all the cuts together, however they overlap. fude_zoom_fill_triangulate_rings appends its triangles to _out
// and returns how many; fude_zoom_fill_inside_rings returns whether _p is inside what is left.
u32  fude_zoom_fill_triangulate_rings(const fude_zoom_v2* _points, const u32* _rings, u32 _n, rde_arr* _out);
b8   fude_zoom_fill_inside_rings(const fude_zoom_v2* _points, const u32* _rings, u32 _n, fude_zoom_v2 _p);
// Its edge as it shows: only where inside meets outside, so the stretch of an outline a cut took, and the part of a
// cut beyond the outline, are left out. The edge comes as open lines: each point is appended to _out and its line's
// number (0, 1, ...) to _lines; a line all the way round ends where it began. Returns how many points were added.
u32  fude_zoom_fill_edges_rings(const fude_zoom_v2* _points, const u32* _rings, u32 _n, rde_arr* _out, rde_arr* _lines);
#define FUDE_ZOOM_FILL_ROUND 8u   // Segments to each half turn of a capsule's round ends.
// Appends the eraser's reach over one step to _out: a capsule (convex) from _a to _b of radius _r. Returns how many
// points were added (2 × (FUDE_ZOOM_FILL_ROUND + 1)).
u32  fude_zoom_fill_capsule(fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _r, rde_arr* _out);
// The eraser's reach over a whole sweep, everywhere within _r of the path _path (_n points), as one outline: the
// distance to the path is sampled on a grid a third of _r fine, its _r line traced (marching squares, the edges placed
// between the samples), the outermost loop kept and simplified to a sixth of a cell. Into _out (cleared first).
// Returns how many points.
u32  fude_zoom_fill_sweep(const fude_zoom_v2* _path, u32 _n, f64 _r, rde_arr* _out);
// The same with a radius at each point (_radii, eased along each segment): a stroke's own outline, which is what the
// partial eraser cuts (erase.h). fude_zoom_fill_widths_cell returns the grid's cell: a third of the smallest radius,
// but never finer than a twelfth of the largest. With _rings (else NULL), the holes the stroke closes round (an "o"'s
// middle) follow the outline, and each point's ring goes into _rings (0 the outline, 1... the holes, as fill's cuts).
// fude_zoom_fill_sweep_widths returns how many points.
f64  fude_zoom_fill_widths_cell(const f64* _radii, u32 _n);
u32  fude_zoom_fill_sweep_widths(const fude_zoom_v2* _path, const f64* _radii, u32 _n, rde_arr* _out, rde_arr* _rings);
// The marching squares those use, on a field of the caller's own: traces its 0 line (below 0 is inside) sampled on a
// grid _w by _h, _cell apart from (_x0, _y0). It keeps the outermost loop, simplified to a sixth of a cell, and with
// _rings its holes after it (each point's ring into _rings). Into _out (both cleared first). Returns how many points.
u32  fude_zoom_fill_contour(const f32* _field, u32 _w, u32 _h, f64 _x0, f64 _y0, f64 _cell, rde_arr* _out, rde_arr* _rings);
// Offsets a closed outline by _d (outwards; inwards when negative), as one loop (its outermost): what a router's bit
// or a saw's kerf leaves round a part. The signed distance to the outline is sampled finely (about a quarter of _d,
// never coarser than a two-hundredth of its size nor finer than a 1500th) and its _d line traced. Into _out (cleared
// first). Returns how many points (0 for fewer than 3 points, _d 0, an outline with no extent, or a grid that would
// take more than 4 million samples).
u32  fude_zoom_fill_offset(const fude_zoom_v2* _poly, u32 _n, f64 _d, rde_arr* _out);
// Finds the part of a closed line round _tap. The line is split at its crossings into simple loops (walking it, each
// time it runs into itself the loop it just closed is taken out), and of the loops round _tap the smallest is kept: a
// figure 8's one lobe, or a hand's loop without the bit that ran past its start. With _close, its end is joined back
// to its start too (its ends are near each other); otherwise only the loops it draws itself count (an "a": its bowl,
// not its tail). Into _out (cleared first). Returns how many points (0 when no loop is round _tap).
u32  fude_zoom_fill_region(const fude_zoom_v2* _points, u32 _n, fude_zoom_v2 _tap, b8 _close, rde_arr* _out);
// Clips the polygon to the box _box (Sutherland-Hodgman, one side at a time: a concave polygon may come out with edges
// along the box, which even-odd fills right). Into _out (an rde_arr of fude_zoom_v2, cleared first). Returns how many
// points.
u32  fude_zoom_fill_clip(const fude_zoom_v2* _points, u32 _n, fude_zoom_box _box, rde_arr* _out);

#endif
