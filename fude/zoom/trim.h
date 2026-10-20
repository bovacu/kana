// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_TRIM_H
#define FUDE_ZOOM_TRIM_H

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/scene.h"

// ===========================================================================
// Trim and Extend (the precision tools): where a straight line is crossed by what else is drawn in its own
// frame (shapes' outlines, guides among them; strokes' centrelines; fills' edges). Trim cuts away the piece of a line
// between the crossings either side of where it is tapped; Extend carries a line's ends on to the first thing each
// would meet.
//
// Where it sits: zoom, the canvas apps' layer. The page (page_edit.c) uses it for Instruments → Trim and for Extend,
// and makes the new lines itself.
// Threads: main thread only (it reads the scene). It keeps no state.
// Design: see docs/infinite_canvas_design.md §26.
// ===========================================================================

// Finds where the line _a → _b (its frame's units) is crossed by what else is in _frame: everything alive and not on a
// hidden layer except _skip, and except sheets and measurements (DIMENSION, RADIAL, ANGLE), whose lines are not
// drawing. Each crossing is a parameter along the line (0 at _a, 1 at _b) from _from to _to. They go into _out sorted,
// each once, at most _max of them. Returns how many.
u32 fude_zoom_trim_crossings(const fude_zoom_scene* _s, u32 _frame, u32 _skip, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _from, f64 _to, f64* _out, u32 _max);

// Works out what of a line is kept when the piece round _at (a parameter along it) is cut away between the crossings
// either side of it (_t: sorted parameters, _n of them; those at the line's very ends do not count). Up to two pieces
// are kept, as parameters: _keep[2k] to _keep[2k + 1], so _keep needs room for 4. Returns how many pieces (0 when the
// whole line goes).
u32 fude_zoom_trim_keep(const f64* _t, u32 _n, f64 _at, f64* _keep);

#endif
