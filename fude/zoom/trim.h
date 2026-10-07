// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_TRIM_H
#define FUDE_ZOOM_TRIM_H

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/scene.h"

// ===========================================================================
// TRIM and EXTEND (Sketching's precision tools, page.c): where a straight line
// is crossed by what else is drawn — shapes' outlines (guides too), strokes,
// fills' edges — in its own frame. Trim cuts away the piece of a line between
// the crossings either side of where it is tapped; Extend carries a line's ends
// on to the first thing each would meet.
// ===========================================================================

// Where the line _a → _b (its frame's units) is crossed by what else is in _frame (not _skip; nothing hidden), as
// parameters along it (0 at _a, 1 at _b) from _from to _to, sorted, each once; how many (at most _max).
u32 fude_zoom_trim_crossings(const fude_zoom_scene* _s, u32 _frame, u32 _skip, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _from, f64 _to, f64* _out, u32 _max);

// What of a line is kept when the piece round _at (a parameter along it) is cut away, between the crossings _t
// (sorted, _n of them) either side of it: up to two pieces, as parameters (_keep[2k], _keep[2k+1]); how many.
u32 fude_zoom_trim_keep(const f64* _t, u32 _n, f64 _at, f64* _keep);

#endif
