// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM
#define FUDE_ZOOM

#include "rde.h"

// ===========================================================================
// Zoom: the types every part of the deep-zoom canvas shares (ids, points, boxes, transforms), and the f64 maths on
// them.
//
// Where it sits: the bottom of fude/zoom, the canvas layer of the canvas apps (one per product, product.h). Every other
// zoom module includes it.
// Its main types and their lifecycle: plain values (fude_zoom_v2, fude_zoom_box, fude_zoom_sim, fude_zoom_xform),
// passed and returned by copy; nothing here allocates.
// Threads: every function is pure and keeps no state, so any thread may call them.
// Design: see docs/infinite_canvas_design.md §2. In short:
//   - FRAMES are the precision mechanism, and the user never sees them. Each frame has its own units and a transform
//     into its parent (one unit is nominally 1/1024 of a parent unit), so no stored number is ever huge or tiny, at
//     any depth. Inside a frame, absolute positions (an object's translation) are f64 and relative ones (a stroke's
//     points) are small integers, so a frame is unbounded sideways and frames exist only for depth.
//   - Everything that turns frame units into screen positions is done here in f64, on the CPU, relative to the
//     camera, so the GPU only ever sees screen-sized f32 numbers.
//   - Screen space is the app's, as everywhere in fude: the origin at the centre, Y up, in points.
// ===========================================================================

// An object's or a frame's id, unique within a canvas and kept in its file.
typedef u64 fude_zoom_id;

// A point or a vector, in whatever units the caller is working in (a frame's units or screen points). It is RDE's f64
// vector (x, y), so rde_math's f64 geometry (a point in a polygon, the distance to a segment, a polygon's area) takes
// the canvas's points and arrays of them as they are.
typedef rde_vec_2D fude_zoom_v2;

// An axis-aligned box. A box with min > max on either axis is empty (fude_zoom_box_empty makes one).
typedef struct {
    f64 min_x, min_y, max_x, max_y;
} fude_zoom_box;

// A similarity transform (scale, rotation and translation):
//   x' = a·x − b·y + tx
//   y' = b·x + a·y + ty
// where a = s·cos θ and b = s·sin θ. Composing two in f64 is exact enough for the levels the renderer draws (see the
// error table in docs/infinite_canvas_design.md §2.3).
typedef struct {
    f64 a, b, tx, ty;
} fude_zoom_sim;

// A frame's place in its parent: p_parent = origin + R(rotation)·(scale·p).
typedef struct {
    f64 ox, oy;
    f64 scale;        // the size of one of the frame's units, in parent units (nominally FUDE_ZOOM_CHILD_SCALE)
    f64 rotation;     // in radians
} fude_zoom_xform;

// Box helpers. fude_zoom_box_empty returns an empty box, the starting point for a union. fude_zoom_box_union returns
// the box round both (an empty one adds nothing). fude_zoom_box_grow widens a box by _by on every side (an empty box
// stays empty).
fude_zoom_box fude_zoom_box_empty(void);
b8            fude_zoom_box_is_empty(fude_zoom_box _b);
fude_zoom_box fude_zoom_box_union(fude_zoom_box _a, fude_zoom_box _b);
fude_zoom_box fude_zoom_box_grow(fude_zoom_box _b, f64 _by);
// Cuts the segment from *_a to *_b to the part inside _box (Liang–Barsky), moving its ends. Returns false when none of
// the segment is inside the box; the points are then left as they were. Only an end that was cut moves: one inside
// the box is left exactly as it was, so a caller can tell which ends were cut by comparing them.
b8 fude_zoom_clip_line(fude_zoom_v2* _a, fude_zoom_v2* _b, fude_zoom_box _box);
// The order rde_sort and rde_arr_sort take for u32 values (indices, slots): the smaller first. Any thread.
i32 fude_zoom_by_u32(const any _a, const any _b);
// More box helpers. Overlaps is false when either box is empty; touching edges count as overlapping. Contains counts
// points on the edge as inside. The area of an empty box is 0.
b8            fude_zoom_box_overlaps(fude_zoom_box _a, fude_zoom_box _b);
b8            fude_zoom_box_contains(fude_zoom_box _b, fude_zoom_v2 _p);
f64           fude_zoom_box_area(fude_zoom_box _b);

// Transform helpers. fude_zoom_sim_from_xform turns a frame's place into the transform from the frame to its parent.
// fude_zoom_sim_compose(outer, inner) applies inner first, then outer. fude_zoom_sim_inverse expects a non-zero scale.
// fude_zoom_sim_scale returns the transform's scale factor, |s|.
fude_zoom_sim fude_zoom_sim_identity(void);
fude_zoom_sim fude_zoom_sim_from_xform(fude_zoom_xform _x);          // from the frame to its parent
fude_zoom_sim fude_zoom_sim_compose(fude_zoom_sim _outer, fude_zoom_sim _inner);   // outer ∘ inner: inner first
fude_zoom_sim fude_zoom_sim_inverse(fude_zoom_sim _s);
fude_zoom_v2  fude_zoom_sim_apply(fude_zoom_sim _s, fude_zoom_v2 _p);
f64           fude_zoom_sim_scale(fude_zoom_sim _s);                  // |s|
// Returns the axis-aligned box round the four corners of _b mapped by _s (an empty box stays empty).
fude_zoom_box fude_zoom_sim_box(fude_zoom_sim _s, fude_zoom_box _b);

#endif
