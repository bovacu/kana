// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_BUCKET_H
#define FUDE_ZOOM_BUCKET_H

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Paint bucket: a fill for line art (research §2.12). It fills the area round a tap that strokes drawn one by one
// close: not one closed line, but the gaps between several. Small gaps are closed and islands are left out.
//
//   1. The ink is laid on a grid round the tap: each cell gets its distance to the nearest ink (each piece of ink a
//      segment with its half-width).
//   2. Cells nearer the ink than half the gap are walls, so a gap narrower than the gap is closed.
//   3. The cells reached from the tap without crossing a wall are the area. If they reach the grid's edge the area is
//      not closed and nothing is filled: it never floods out to infinity.
//   4. The area grows back by half the gap and a little more (_under), so it reaches under the lines that bound it
//      and no seam shows between them. Its edge is traced with fill.h's marching squares, with its holes: the islands
//      drawn inside it.
//
// Where it sits: zoom, the canvas apps' layer, over fill.h. The page's Fill tool (page_pictures.c) gathers the ink on
// the screen, calls it with the screen as its bounds, and turns the result into a FILL in the camera's frame; a
// building plan's rooms (page_diagrams.c) are found the same way.
// Threads: none of its own and no shared state; the page calls it on the main thread.
// Design: everything here is in one plane's units (the page's screen points), so the grid and the gap are sized to
// what the person sees, at any zoom (docs/infinite_canvas_design.md §15).
// ===========================================================================

// A piece of ink the bucket fills up to.
typedef struct {
    fude_zoom_v2 a, b;    // A segment of ink.
    f64          r;       // Its half-width.
} fude_zoom_bucket_ink;

#define FUDE_ZOOM_BUCKET_CELLS 2000000u   // The grid's cells at most: the cell is made bigger to keep within it.

// How a bucket fill went (fude_zoom_bucket_region).
typedef enum {
    FUDE_ZOOM_BUCKET_FILLED = 0,
    FUDE_ZOOM_BUCKET_OPEN,         // It ran out to the grid's edge: the area is not closed.
    FUDE_ZOOM_BUCKET_ON_INK,       // The tap was on the ink itself.
    FUDE_ZOOM_BUCKET_NOTHING
} FUDE_ZOOM_BUCKET_;

// Fills the area round _tap bounded by the _n pieces of _ink, gaps narrower than _gap closed, within _bounds, on a grid
// _cell fine (or coarser, to keep within FUDE_ZOOM_BUCKET_CELLS). The area is grown back under the lines round it by
// half the gap and _under more (step 4), so no seam shows between them. Its outline and then its islands go into _out,
// and each point's ring into _rings (fill.h's rings: 0 the outline, the others cut out of it; NULL to skip); both are
// cleared first. Returns:
//   FILLED   an area was found;
//   OPEN     it ran out to the grid's edge: what is round the tap does not close it (no ink at all, _n 0, gives this);
//   ON_INK   the tap was on the ink itself;
//   NOTHING  _cell or _bounds is empty (not above 0), the tap is outside the grid's border, it is within half a gap of
//            the ink without being on it, or the area is too small to trace.
FUDE_ZOOM_BUCKET_ fude_zoom_bucket_region(const fude_zoom_bucket_ink* _ink, u32 _n, fude_zoom_box _bounds, fude_zoom_v2 _tap,
                                          f64 _cell, f64 _gap, f64 _under, rde_arr* _out, rde_arr* _rings);

#endif
