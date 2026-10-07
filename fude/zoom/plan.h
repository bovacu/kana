// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PLAN_H
#define FUDE_ZOOM_PLAN_H

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// BUILDING PLANS (Sketching's Floor plans and House wiring topics): the diagram
// library's Floor plan family — doors, windows, furniture, fittings — put in at
// their TRUE SIZE (a bed 90 × 190 cm is that on the canvas, as a wall drawn 4 m
// long is), each a plan seen from above; and its House wiring family — the
// symbols an electrician marks a plan with (sockets, switches, lights, the
// board…), at the size symbols are.
//
// A door, a window or an opening comes with the wall's band at its foot (10 cm
// deep: as thick as the wall it goes in once stretched), filled as the page is,
// so put over a wall (shape.h's WALL) it cuts its opening in it.
// ===========================================================================

typedef struct {
    const c8* id;       // its symbol's id (symbol.c)
    u8        look;     // how it is drawn (plan.c)
    f32       w, h;     // its size: centimetres (true_size) or points as it comes
    b8        true_size;
    const c8* value;    // its text to begin with
} fude_zoom_plan_part;

const fude_zoom_plan_part* fude_zoom_plan_find(const c8* _id);
const fude_zoom_plan_part* fude_zoom_plan_of_kind(u32 _kind);
// Its polylines (symbol.h's parts, the first its outline) for half sizes _hw × _hh. How many parts.
u32  fude_zoom_plan_draw(const fude_zoom_plan_part* _part, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points, rde_arr* _parts);

#endif
