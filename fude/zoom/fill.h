// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_FILL
#define FUDE_ZOOM_FILL

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Filling (research §5.2, Phase 1's: a tap, no gap closing yet):
//
//   A closed SHAPE tapped with the Fill tool takes the brush's colour as its
//   fill (scene.h: FUDE_ZOOM_FLAG_FILLED and the object's fill colour) —
//   tapped again in the same colour, it lets it go. No new geometry.
//   A closed STROKE — its gesture's ends near enough each other — gets a FILL
//   object: the loop of its centreline round the tap, drawn just under it, so
//   the line stays on top and no seam shows between them.
//   The ERASER rubs a fill out where it passes: each step's capsule becomes a
//   cut ring of the fill, taken out of its outline.
//
// A polygon's inside is even-odd (a figure 8 fills both lobes; where a hand's
// loop runs over its start, the sliver there does not). It is drawn as
// triangles from a slab decomposition: cut at every corner's height and every
// crossing's, each slab's edges paired off left to right — exact, any shape,
// any crossings.
// ===========================================================================

// _n points (a closed polygon: the last joins the first) as triangles,
// appended to _out (rde_arr of fude_zoom_v2, three a triangle). How many.
u32  fude_zoom_fill_triangulate(const fude_zoom_v2* _points, u32 _n, rde_arr* _out);
// Is _p inside the polygon (even-odd)?
b8   fude_zoom_fill_inside(const fude_zoom_v2* _points, u32 _n, fude_zoom_v2 _p);

// A fill the eraser has been at: its points in rings, each point's ring in
// _rings (runs of one ring; NULL: all ring 0). Ring 0 is the outline (even-odd);
// every other ring is a cut the eraser made (one step's capsule), taken out of
// it — all the cuts together, however they overlap.
u32  fude_zoom_fill_triangulate_rings(const fude_zoom_v2* _points, const u32* _rings, u32 _n, rde_arr* _out);
b8   fude_zoom_fill_inside_rings(const fude_zoom_v2* _points, const u32* _rings, u32 _n, fude_zoom_v2 _p);
// Its edge as it shows: only where inside meets outside — an outline's stretch
// a cut took, and a cut's beyond the outline, left out — as open lines, each
// point appended to _out and its line's number (0, 1, ...) to _lines (a line
// all round ends where it began). How many points.
u32  fude_zoom_fill_edges_rings(const fude_zoom_v2* _points, const u32* _rings, u32 _n, rde_arr* _out, rde_arr* _lines);
// The eraser's reach over one step, a to b at radius _r: a capsule (convex),
// its points appended to _out. How many.
#define FUDE_ZOOM_FILL_ROUND 8u   // segments to each half turn of a capsule's ends
u32  fude_zoom_fill_capsule(fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _r, rde_arr* _out);
// The eraser's reach over a whole sweep — everywhere within _r of the path
// _path (_n points) — as one outline: the distance to the path sampled on a
// grid a third of _r fine, its _r line traced (marching squares, the edges
// placed between the samples) and the outermost loop kept, simplified to a
// sixth of a cell. Into _out (cleared first). How many points.
u32  fude_zoom_fill_sweep(const fude_zoom_v2* _path, u32 _n, f64 _r, rde_arr* _out);
// The same with a width at each point (_radii, eased along each segment): a
// stroke's own outline — what the partial eraser cuts (erase.h). The grid a
// third of the thinnest width fine (but never more than a twelfth of the
// widest): fude_zoom_fill_widths_cell. With _rings (else NULL), the holes the
// stroke closes round (an "o"'s middle) follow the outline, each point's ring
// into _rings (0 the outline, 1... the holes: fill.h's cuts).
f64  fude_zoom_fill_widths_cell(const f64* _radii, u32 _n);
u32  fude_zoom_fill_sweep_widths(const fude_zoom_v2* _path, const f64* _radii, u32 _n, rde_arr* _out, rde_arr* _rings);
// The marching squares those use, on a field of one's own: its 0 line (below
// 0 is inside) sampled on a grid _w by _h, _cell apart from (_x0, _y0) — the
// outermost loop, simplified to a sixth of a cell, and with _rings its holes
// after it (each point's ring into _rings). How many points.
u32  fude_zoom_fill_contour(const f32* _field, u32 _w, u32 _h, f64 _x0, f64 _y0, f64 _cell, rde_arr* _out, rde_arr* _rings);
// A closed outline offset by _d (out; inside when negative), as one loop (its
// outermost): what a router's bit or a saw's kerf leaves round a part. The
// signed distance to the outline sampled finely (about a quarter of _d, never
// coarser than a two-hundredth of its size), its _d line traced. How many points.
u32  fude_zoom_fill_offset(const fude_zoom_v2* _poly, u32 _n, f64 _d, rde_arr* _out);
// The part of a closed line round _tap: the line split at its crossings into
// simple loops (walking it, each time it runs into itself the loop it just
// closed is taken out), and of those round _tap, the smallest — a figure 8's
// one lobe, a hand's loop without the bit that ran past its start. _close: its
// end joined back to its start too (its ends are near each other); else only
// the loops it draws itself count (an "a": its bowl, not the tail). Into _out
// (cleared first). How many points (0: no loop is round _tap).
u32  fude_zoom_fill_region(const fude_zoom_v2* _points, u32 _n, fude_zoom_v2 _tap, b8 _close, rde_arr* _out);
// The polygon cut to the box _box (Sutherland-Hodgman, an edge at a time: a
// concave one may come out with edges along the box, which even-odd fills
// right), into _out (rde_arr of fude_zoom_v2, cleared first). How many points.
u32  fude_zoom_fill_clip(const fude_zoom_v2* _points, u32 _n, fude_zoom_box _box, rde_arr* _out);

#endif
