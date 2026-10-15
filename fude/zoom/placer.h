// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PLACER_H
#define FUDE_ZOOM_PLACER_H

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/scene.h"

// ===========================================================================
// Placer: drawing as a person does, by code. It puts the diagram library's parts down at their sizes, wires them pin
// to pin (routed as the Wire tool routes, or straight as on a breadboard), and writes texts and pen lines.
//
// Where it sits: zoom, the canvas apps' layer. The examples are drawn with it (examples.h), and so is a custom part's
// inside, from its definition (logic.h).
// Its main types and their lifecycle: fude_zoom_placer is a small value made by fude_zoom_placer_make and kept on the
// caller's stack while it draws; it owns nothing. Everything it makes is added to the scene and appended, in order,
// to the caller's _born array.
// Threads: main thread only (it changes the scene).
// Units: the caller's points are screen points round a middle (Y up, at ×1); the placer takes them into a frame of any
// scene through to_frame.
// ===========================================================================

// Where and how a placer draws (fude_zoom_placer_make).
typedef struct fude_zoom_placer {
    fude_zoom_scene* s;
    u32              frame;
    fude_zoom_sim    to_frame;   // Takes a point (screen points) into the frame's units.
    f64              u;          // The frame's units to a point.
    rde_color        ink;
    rde_arr*         born;       // TYPE(u32): the objects it made, in order. The caller's.
    fude_zoom_v2     o;          // Where (0, 0) is: the middle of what is being drawn, in points. Starts at (0, 0).
} fude_zoom_placer;

// Returns a placer drawing into frame _frame of _s, taking points into it with _to_frame, in _ink, appending what it
// makes to _born.
fude_zoom_placer fude_zoom_placer_make(fude_zoom_scene* _s, u32 _frame, fude_zoom_sim _to_frame, rde_color _ink, rde_arr* _born);
// Returns a point (points from the middle) in the frame's units.
fude_zoom_v2     fude_zoom_placer_at(const fude_zoom_placer* _p, f64 _x, f64 _y);
// Places the library's symbol _id, _w × _h points (0 for the size the library gives it), its middle at _x, _y, turned
// _turn degrees, with its text _text. Returns its object, or FUDE_ZOOM_NONE when there is no such symbol.
u32              fude_zoom_placer_part(fude_zoom_placer* _p, const c8* _id, f64 _x, f64 _y, f64 _turn, f64 _w, f64 _h, const c8* _text);
// Returns where part _o's pin _pin is, in points from the middle. With no such pin it gives the frame's origin.
fude_zoom_v2     fude_zoom_placer_pin(const fude_zoom_placer* _p, u32 _o, u32 _pin);
// Draws a wire from part _a's pin _pa to part _b's pin _pb: routed as the Wire tool routes, or straight when _straight.
// Nothing is drawn when a part is FUDE_ZOOM_NONE or a pin is missing.
void             fude_zoom_placer_wire(fude_zoom_placer* _p, u32 _a, u32 _pa, u32 _b, u32 _pb, b8 _straight);
// The same through the corners _via (_n of them, points from the middle), as a person draws a wire round what is in
// the way. Nothing is drawn when the corners do not fit in a wire (FUDE_ZOOM_WIRE_POINTS, its ends included).
void             fude_zoom_placer_wire_via(fude_zoom_placer* _p, u32 _a, u32 _pa, u32 _b, u32 _pb, const fude_zoom_v2* _via, u32 _n);
// Writes a plain text _size points high, its top-left corner at _x, _y, wrapped to _w points wide.
void             fude_zoom_placer_text(fude_zoom_placer* _p, f64 _x, f64 _y, f64 _size, f64 _w, const c8* _text);
// Draws a pen line through _pts (points from the middle), closed or not, _width points wide. Returns its object, or
// FUDE_ZOOM_NONE when it has no points or more than 64 (a closed line's first point counted again at its end).
u32              fude_zoom_placer_path(fude_zoom_placer* _p, const fude_zoom_v2* _pts, u32 _n, b8 _closed, f64 _width);

#endif
