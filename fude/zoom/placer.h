// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PLACER_H
#define FUDE_ZOOM_PLACER_H

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/scene.h"

// ===========================================================================
// DRAWING AS A PERSON DOES, BY CODE: the library's parts put down at their
// sizes, wired pin to pin (routed as the Wire tool routes, or straight as on
// a breadboard), texts and pen lines — what the examples are drawn with
// (examples.h), and a custom part's inside drawn from its definition
// (logic.h). Points are screen points round a middle (Y up, ×1), taken into
// a frame of any scene; what is made is kept in order (_born).
// ===========================================================================

typedef struct fude_zoom_placer {
    fude_zoom_scene* s;
    u32              frame;
    fude_zoom_sim    to_frame;   // a point (screen points) → the frame's units
    f64              u;          // the frame's units a point
    rde_color        ink;
    rde_arr*         born;       // TYPE(u32): what it made
    fude_zoom_v2     o;          // the middle of what is being drawn (points): where (0, 0) is
} fude_zoom_placer;

fude_zoom_placer fude_zoom_placer_make(fude_zoom_scene* _s, u32 _frame, fude_zoom_sim _to_frame, rde_color _ink, rde_arr* _born);
// A point (points from the middle) in the frame.
fude_zoom_v2     fude_zoom_placer_at(const fude_zoom_placer* _p, f64 _x, f64 _y);
// A symbol _w × _h points (0: as the library has it) at _x, _y, turned _turn degrees, its text _text. Its object
// (FUDE_ZOOM_NONE: no such symbol).
u32              fude_zoom_placer_part(fude_zoom_placer* _p, const c8* _id, f64 _x, f64 _y, f64 _turn, f64 _w, f64 _h, const c8* _text);
// Where part _o's pin _pin is (points from the middle).
fude_zoom_v2     fude_zoom_placer_pin(const fude_zoom_placer* _p, u32 _o, u32 _pin);
// A wire from part _a's pin _pa to _b's _pb: routed, or straight.
void             fude_zoom_placer_wire(fude_zoom_placer* _p, u32 _a, u32 _pa, u32 _b, u32 _pb, b8 _straight);
// ...through corners _via (_n of them: points from the middle), as a person draws it round what is in the way.
void             fude_zoom_placer_wire_via(fude_zoom_placer* _p, u32 _a, u32 _pa, u32 _b, u32 _pb, const fude_zoom_v2* _via, u32 _n);
// A text _size points high from _x, _y (its top left), _w wide.
void             fude_zoom_placer_text(fude_zoom_placer* _p, f64 _x, f64 _y, f64 _size, f64 _w, const c8* _text);
// A pen line through _pts (points), closed or not, _width points wide (64 points at most). Its object.
u32              fude_zoom_placer_path(fude_zoom_placer* _p, const fude_zoom_v2* _pts, u32 _n, b8 _closed, f64 _width);

#endif
