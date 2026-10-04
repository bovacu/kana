// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM
#define FUDE_ZOOM

#include "rde.h"

// ===========================================================================
// The deep-zoom canvas: what every part of it shares. The design, and why it is
// this way, is docs/infinite_canvas_design.md; in one breath:
//
// FRAMES are the precision mechanism, never shown. Each has its own units and a
// transform into its parent (a unit is 1/1024 of the parent's, nominally), so no
// stored number is ever huge or tiny, at any depth. Positions INSIDE a frame are
// f64 where they are absolute (an object's translation) and small integers where
// they are relative (a stroke's points), so a frame is unbounded sideways and
// frames only exist for depth.
//
// Everything that turns frame units into the screen is done here in f64, on the
// CPU, relative to the camera: the GPU only ever sees screen-sized f32.
//
// Screen space is the app's (centre origin, Y up, points), as everywhere in fude.
// ===========================================================================

typedef u64 fude_zoom_id;

typedef struct {
    f64 x, y;
} fude_zoom_v2;

// An axis-aligned box. Empty: min > max (fude_zoom_box_empty).
typedef struct {
    f64 min_x, min_y, max_x, max_y;
} fude_zoom_box;

// A similarity (scale, rotation, translation):
//   x' = a·x − b·y + tx
//   y' = b·x + a·y + ty
// a = s·cos θ, b = s·sin θ. Composing two is exact enough in f64 for the
// levels the renderer draws (the design's error table).
typedef struct {
    f64 a, b, tx, ty;
} fude_zoom_sim;

// A frame's place in its parent: p_parent = origin + R(rotation)·(scale·p).
typedef struct {
    f64 ox, oy;
    f64 scale;        // a unit of the frame, in parent units (FUDE_ZOOM_CHILD_SCALE nominally)
    f64 rotation;     // radians
} fude_zoom_xform;

fude_zoom_box fude_zoom_box_empty(void);
b8            fude_zoom_box_is_empty(fude_zoom_box _b);
fude_zoom_box fude_zoom_box_union(fude_zoom_box _a, fude_zoom_box _b);
fude_zoom_box fude_zoom_box_grow(fude_zoom_box _b, f64 _by);
b8            fude_zoom_box_overlaps(fude_zoom_box _a, fude_zoom_box _b);
b8            fude_zoom_box_contains(fude_zoom_box _b, fude_zoom_v2 _p);
f64           fude_zoom_box_area(fude_zoom_box _b);

fude_zoom_sim fude_zoom_sim_identity(void);
fude_zoom_sim fude_zoom_sim_from_xform(fude_zoom_xform _x);          // frame → parent
fude_zoom_sim fude_zoom_sim_compose(fude_zoom_sim _outer, fude_zoom_sim _inner);   // outer ∘ inner: inner first
fude_zoom_sim fude_zoom_sim_inverse(fude_zoom_sim _s);
fude_zoom_v2  fude_zoom_sim_apply(fude_zoom_sim _s, fude_zoom_v2 _p);
f64           fude_zoom_sim_scale(fude_zoom_sim _s);                  // |s|
// The box _b maps to under _s, as an axis-aligned box round it.
fude_zoom_box fude_zoom_sim_box(fude_zoom_sim _s, fude_zoom_box _b);

#endif
