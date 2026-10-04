// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SHAPE
#define FUDE_ZOOM_SHAPE

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Shapes: lines, rectangles, ellipses and polygons kept as their NUMBERS, not
// points — a rectangle is its half-width, half-height and corners — so they
// stay exact at any zoom and can be changed later. A shape is an object of
// kind SHAPE (scene.h): its type in the object's `channels`, its numbers in its
// payload (f64s, in its frame's units from its translation, before its
// rotation and scale), drawn in the object's colour and half-width, filled
// with the same colour when the object says FILLED.
//
//   LINE     x1 y1                          from its translation to there
//   RECT     half-width half-height radius  round its translation (radius: its corners')
//   ELLIPSE  rx ry                          round its translation
//   POLYGON  x y, a corner each             closed; round its translation
//
// HOLD TO SNAP (research §2.4): a stroke drawn roughly, the pen held still at
// its end, becomes the shape it looks like — a line, a circle or an ellipse, a
// rectangle, a triangle or a polygon of up to eight corners — or stays a
// stroke when it looks like none of them (fude_zoom_shape_recognize).
// ===========================================================================

typedef enum {
    FUDE_ZOOM_SHAPE_LINE = 1,
    FUDE_ZOOM_SHAPE_RECT,
    FUDE_ZOOM_SHAPE_ELLIPSE,
    FUDE_ZOOM_SHAPE_POLYGON
} FUDE_ZOOM_SHAPE_;

#define FUDE_ZOOM_SHAPE_CORNERS 8u
#define FUDE_ZOOM_SHAPE_NUMBERS (FUDE_ZOOM_SHAPE_CORNERS * 2u)

// A shape as recognized or being placed, in whatever units its points came in.
typedef struct {
    u8           type;        // FUDE_ZOOM_SHAPE_ (0: none)
    fude_zoom_v2 at;          // its translation (a line's start; the others' middle)
    f64          rotation;
    f64          n[FUDE_ZOOM_SHAPE_NUMBERS];
    u32          count;       // numbers in n
} fude_zoom_shape_fit;

// The shape a stroke's points look like (_count of them, screen units: the
// tolerances are a hand's on screen). False: none — it stays a stroke.
b8   fude_zoom_shape_recognize(const fude_zoom_v2* _points, u32 _count, fude_zoom_shape_fit* _out);

// A shape's outline, its own units (before its translation, rotation, scale),
// into _out (rde_arr of fude_zoom_v2). _segments: how finely an ellipse or a
// round corner is cut. *_closed: whether the last point joins the first.
void fude_zoom_shape_outline(u8 _type, const f64* _n, u32 _count, u32 _segments, rde_arr* _out, b8* _closed);
// Segments enough for an outline _size across on screen to look round.
u32  fude_zoom_shape_segments(f64 _size_on_screen);

#endif
