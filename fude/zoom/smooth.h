// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SMOOTH
#define FUDE_ZOOM_SMOOTH

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// What the pen's points become as a stroke is drawn (research §2.2), the
// toolbar's Smoothing:
//
//   LOW, MEDIUM, HIGH  a Gaussian along the stroke, a few screen points wide
//            (2, 5, 10): the hand's tremor goes, the shape stays. It looks both
//            ways, so the line never lags behind the pen the way an average of
//            the last points does (StreamLine). It is a line fitted to the
//            points round each one (local linear regression), wider on the side
//            that has points near an end, so a stroke starts and ends where the
//            pen did, less its shake — no hook. The last stretch settles as the
//            pen moves on: a point is final once the pen is past its window
//            (fude_zoom_smooth_points).
//   ROPE     the pulled string (Lazy Nezumi, Krita's stabilizer): the line is
//            drawn by a tip on a string behind the pen, which moves only when
//            the string is taut — slow, sure lines and clean corners, without a
//            wobble. When the pen lifts the line catches up with it (Krita's
//            finish line). Low's smoothing over it.
//
// Widths and the string's length are screen points as the stroke is drawn
// (research §1: tools sized to the screen), in the drawing frame's units here.
// What is kept is the smoothed line — what was seen — so an erase or a lasso
// acts on exactly what is on the screen.
// ===========================================================================

typedef enum {
    FUDE_ZOOM_SMOOTH_OFF = 0,
    FUDE_ZOOM_SMOOTH_LOW,
    FUDE_ZOOM_SMOOTH_MEDIUM,
    FUDE_ZOOM_SMOOTH_HIGH,
    FUDE_ZOOM_SMOOTH_ROPE,
    FUDE_ZOOM_SMOOTH_COUNT
} FUDE_ZOOM_SMOOTH_;

#define FUDE_ZOOM_SMOOTH_DEFAULT     FUDE_ZOOM_SMOOTH_LOW
#define FUDE_ZOOM_SMOOTH_ROPE_LENGTH 22.0    // screen points
#define FUDE_ZOOM_SMOOTH_CATCH_STEP  2.0     // screen points between the finish line's points

// A level's Gaussian width (its sigma), screen points (0: none).
f64  fude_zoom_smooth_sigma(u8 _level);

// _raw's _n points smoothed with width _sigma (their units) into _out, from
// point _from on — those before it are as they were (a stroke being drawn
// re-smooths only its unsettled end). The first point that may still change
// when more come: pass it back as _from next time.
u32  fude_zoom_smooth_points(const fude_zoom_v2* _raw, u32 _n, f64 _sigma, fude_zoom_v2* _out, u32 _from);

// The rope: its tip pulled towards _pen until the string, _length long, is
// slack again. True: the tip moved.
b8   fude_zoom_smooth_rope(fude_zoom_v2* _tip, fude_zoom_v2 _pen, f64 _length);

#endif
