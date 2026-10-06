// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_BUCKET_H
#define FUDE_ZOOM_BUCKET_H

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// A paint bucket for line art (research §2.12): the area round a tap that
// strokes drawn one by one close — not one closed line, but the gaps between
// several — filled, with small gaps closed and islands left out.
//
//   1. The ink is laid on a grid round the tap: each cell its distance to the
//      nearest ink (each piece a segment and its half-width).
//   2. Cells nearer the ink than half the gap are walls, so a gap narrower
//      than the gap is closed.
//   3. The cells reached from the tap without crossing a wall are the area.
//      Reaching the grid's edge, it is not closed: nothing is filled (it never
//      floods to infinity).
//   4. The area grows back by half the gap and a little more (_under), so it
//      reaches under the lines that bound it and no seam shows between them;
//      its edge is traced (fill.h's marching squares) with its holes — the
//      islands drawn inside it.
//
// All in one plane's units (the page's screen points): the result is turned
// into a FILL in a frame by whoever asks.
// ===========================================================================

typedef struct {
    fude_zoom_v2 a, b;    // a segment of ink
    f64          r;       // its half-width
} fude_zoom_bucket_ink;

#define FUDE_ZOOM_BUCKET_CELLS 2000000u   // the grid's cells at most (the cell grows to keep within it)

typedef enum {
    FUDE_ZOOM_BUCKET_FILLED = 0,
    FUDE_ZOOM_BUCKET_OPEN,         // it ran out to the edge: not closed
    FUDE_ZOOM_BUCKET_ON_INK,       // the tap was on the ink itself
    FUDE_ZOOM_BUCKET_NOTHING
} FUDE_ZOOM_BUCKET_;

// The area round _tap bounded by _ink, gaps narrower than _gap closed, within
// _bounds, the grid _cell fine (or coarser, to keep within the cells): its
// outline then its islands into _out, each point's ring into _rings (fill.h's
// rings: 0 the outline, the others cut out of it).
FUDE_ZOOM_BUCKET_ fude_zoom_bucket_region(const fude_zoom_bucket_ink* _ink, u32 _n, fude_zoom_box _bounds, fude_zoom_v2 _tap,
                                          f64 _cell, f64 _gap, f64 _under, rde_arr* _out, rde_arr* _rings);

#endif
