// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_MATHREAD
#define FUDE_ZOOM_MATHREAD

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// HANDWRITTEN MATHS READ (docs/product_split.md §4.3, §9.5: our own reader —
// a first one): pen strokes as LaTeX, on the device, with no service.
//
//   1. The strokes in the order they were written are cut into symbols — one,
//      two or three strokes each (an = is two, a π two or three) — the way that
//      reads best: each run of strokes close together tried as a symbol, the
//      best sum of them kept (dynamic programming over the order).
//   2. Each symbol known by its shape: matched as a cloud of points ($P:
//      Vatavu, Anthony & Wobbrock 2012 — the order and the way of its strokes
//      not minded) against the shapes handwriting writes formulas with
//      (handmath.h): digits, letters, Greek, operators, relations, brackets,
//      ∑ ∫ √ ∞ ∂ ∇. A dot by its size; a capital or a small letter by its height.
//   3. Its layout read: fraction bars (what is over them and under them), roots
//      (what is under them), a sum's and an integral's limits, then a line's
//      superscripts and subscripts (smaller, and higher or lower than what they
//      follow); sin, cos, log… written as letters made functions.
//
// Points: any units, y up; strokes as handmath.h writes them (ends: past each
// one's last point).
// ===========================================================================

// What _strokes strokes (their points _points, their ends _ends) say, as LaTeX into _out (_size bytes). False: nothing
// read (no strokes, or none a symbol). *_symbols (NULL: not wanted) how many symbols it read.
b8  fude_zoom_mathread(const fude_zoom_v2* _points, const u32* _ends, u32 _strokes, c8* _out, usize _size, u32* _symbols);

// One symbol's strokes: the character it is most like (0: none), *_distance (NULL: not wanted) how far off it is
// ($P's distance: under about 2 a good match).
u32 fude_zoom_mathread_symbol(const fude_zoom_v2* _points, const u32* _ends, u32 _strokes, f64* _distance);

#endif
