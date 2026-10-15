// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_MATHREAD
#define FUDE_ZOOM_MATHREAD

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Mathread: handwritten maths read as LaTeX, on the device and with no online service. This is the app's own reader,
// a first version (docs/product_split.md §4.3 and §9.5).
//
// Where it sits: zoom, the canvas apps' layer. The page (page_text.c) uses it for the lasso's Formula button on ink,
// which replaces the selected strokes with a typeset formula. It recognises symbols by the shapes handmath.h writes.
// Its main types and their lifecycle: none of its own; the caller owns the points, the ends and the output buffer.
// Threads: main thread only. The symbol templates are built into static memory on the first call, and the layout
//   step sorts through a static pointer, so two threads must not call it at once.
// Design: it works in three steps.
//   1. The strokes, in the order they were written, are cut into symbols of one, two or three strokes each (an = is
//      two, a π two or three), choosing the cut that reads best: each run of strokes close together is tried as a
//      symbol, and the cut with the best total is kept (dynamic programming over the writing order).
//   2. Each symbol is recognised by its shape: matched as a cloud of points against the shapes handmath.h writes
//      formulas with (digits, letters, Greek, operators, relations, brackets, ∑ ∫ √ ∞ ∂ ∇). The matcher is $P
//      (Vatavu, Anthony & Wobbrock 2012), chosen because it ignores the order and direction of the strokes. A dot is
//      recognised by its size, and a capital is told from a small letter by its height.
//   3. The layout is read: fraction bars (what is over and under them), roots (what is under them), the limits of a
//      sum and an integral, then each line's superscripts and subscripts (smaller, and higher or lower than what they
//      follow). Letters spelling sin, cos, log and so on become those functions' commands.
//   Points may be in any units, y up. Strokes are given as handmath.h writes them: _ends holds, for each stroke, the
//   index one past its last point. Only the first 160 strokes are read.
//   See docs/infinite_canvas_design.md §65.
// ===========================================================================

// Reads _strokes strokes (their points in _points, their ends in _ends) and writes what they say as LaTeX into _out,
// which holds _size bytes; longer LaTeX is cut short. Returns false when nothing was read: no strokes, or none of them
// a symbol. *_symbols, unless _symbols is NULL, gets how many symbols it read.
b8  fude_zoom_mathread(const fude_zoom_v2* _points, const u32* _ends, u32 _strokes, c8* _out, usize _size, u32* _symbols);

// Returns the character that one symbol's strokes are most like, or 0 when there is none (no strokes or no points).
// It may return U+E000 or U+E001, the reader's second shapes for ∫ (written small) and √ (written with its bar).
// *_distance, unless _distance is NULL, gets how far off the match is, as $P measures it: under about 2 is a good
// match.
u32 fude_zoom_mathread_symbol(const fude_zoom_v2* _points, const u32* _ends, u32 _strokes, f64* _distance);

#endif
