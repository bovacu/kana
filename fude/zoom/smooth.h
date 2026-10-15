// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SMOOTH
#define FUDE_ZOOM_SMOOTH

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Smoothing: what the pen's points become as a stroke is drawn (research §2.2; the toolbar's Smoothing).
//
//   LOW, MEDIUM, HIGH  a Gaussian along the stroke, a few screen points wide (2, 5 and 10): the hand's tremor goes and
//                      the shape stays. It looks both ways along the stroke, so the line never lags behind the pen the
//                      way an average of the last points does (StreamLine). It is a line fitted to the points round
//                      each one (local linear regression), wider on the side that has points near an end, so a stroke
//                      starts and ends where the pen did, less its shake, with no hook. The last stretch settles as
//                      the pen moves on: a point is final once the pen is past its window (fude_zoom_smooth_points).
//   ROPE               the pulled string (Lazy Nezumi, Krita's stabiliser): the line is drawn by a tip on a string
//                      behind the pen, which moves only when the string is taut, for slow, sure lines and clean
//                      corners without a wobble. When the pen lifts, the line catches up with it (Krita's finish
//                      line). Low's smoothing is applied over it.
//
// Where it sits: zoom, the canvas apps' layer. The page (page_pen.c) smooths each stroke as it is drawn.
// Threads: none of its own and no shared state; the page calls it on the main thread.
// Design: widths and the string's length are in screen points as the stroke is drawn (research §1: tools are sized to
// the screen); the page passes them in the drawing frame's units. What is kept is the smoothed line, which is what was
// seen, so an eraser or a lasso acts on exactly what is on the screen (docs/infinite_canvas_design.md §14).
// ===========================================================================

// The smoothing levels (the toolbar's Smoothing: Off, Low, Medium, High, Rope).
typedef enum {
    FUDE_ZOOM_SMOOTH_OFF = 0,
    FUDE_ZOOM_SMOOTH_LOW,
    FUDE_ZOOM_SMOOTH_MEDIUM,
    FUDE_ZOOM_SMOOTH_HIGH,
    FUDE_ZOOM_SMOOTH_ROPE,
    FUDE_ZOOM_SMOOTH_COUNT
} FUDE_ZOOM_SMOOTH_;

// New strokes are drawn as the hand drew them until the person chooses a smoothing level.
#define FUDE_ZOOM_SMOOTH_DEFAULT     FUDE_ZOOM_SMOOTH_OFF   // Borja's choice, 2026-10-11.
#define FUDE_ZOOM_SMOOTH_ROPE_LENGTH 22.0    // The rope's length, in screen points.
#define FUDE_ZOOM_SMOOTH_CATCH_STEP  2.0     // Screen points between the finish line's points.

// Returns a level's Gaussian width (its sigma) in screen points: 2, 5 or 10, Rope's 2 (Low's), and 0 for none.
f64  fude_zoom_smooth_sigma(u8 _level);

// Smooths _raw's _n points with width _sigma (in their units) into _out, from point _from on; the points before it
// are left as they were, so a stroke being drawn smooths again only its unsettled end. Returns the first point that
// may still change when more points come: pass it back as _from next time.
u32  fude_zoom_smooth_points(const fude_zoom_v2* _raw, u32 _n, f64 _sigma, fude_zoom_v2* _out, u32 _from);

// The rope: pulls its tip towards _pen until the string, _length long, is slack again. Returns true when the tip moved.
b8   fude_zoom_smooth_rope(fude_zoom_v2* _tip, fude_zoom_v2 _pen, f64 _length);

#endif
