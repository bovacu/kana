// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PLAN_H
#define FUDE_ZOOM_PLAN_H

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Building plans (the Floor plans and House wiring topics): the diagram library's Floor plan family (doors,
// windows, furniture, fittings), each a plan seen from above and put in at its TRUE SIZE (a bed 90 × 190 cm is that on
// the canvas, as a wall drawn 4 m long is); and its House wiring family, the symbols an electrician marks a plan with
// (sockets, switches, lights, the consumer unit...), at the size symbols are drawn.
//
// Where it sits: zoom, the canvas apps' layer, under the diagram library: symbol.c draws these parts with
// fude_zoom_plan_draw, and the page (page_pieces.c) asks fude_zoom_plan_of_kind for a part's true size when it is put
// in.
// Its main types and their lifecycle: fude_zoom_plan_part is an entry of a constant table in plan.c.
// Threads: main thread only (fude_zoom_plan_of_kind's answers are kept in the topics' one table, symbol.h's
// fude_zoom_symbol_topic_part).
//
// A door, a window or an opening comes with the wall's band at its foot (10 cm deep: as thick as the wall it goes in,
// once stretched), filled as the page is, so put over a wall (shape.h's WALL) it cuts its opening in it.
// Design: see docs/infinite_canvas_design.md §29.
// ===========================================================================

// A plan part: an entry of plan.c's table.
typedef struct {
    const c8* id;       // Its symbol's id (symbol.c).
    u8        look;     // How it is drawn (plan.c's FZP_ looks).
    f32       w, h;     // Its size: centimetres when true_size, else points as it is put in.
    b8        true_size;
    const c8* value;    // The text it starts with.
} fude_zoom_plan_part;

// fude_zoom_plan_find returns the part whose symbol id is _id, and fude_zoom_plan_of_kind the part drawn for the
// diagram library's symbol kind _kind (symbol.h). Both return NULL when it is not a plan part. The pointers are to a
// constant table and stay valid for the app's life.
const fude_zoom_plan_part* fude_zoom_plan_find(const c8* _id);
const fude_zoom_plan_part* fude_zoom_plan_of_kind(u32 _kind);
// Returns whether _part is an opening in a wall: a door (any), a window or a bare opening. False for NULL. A room's
// bounds take these in (page_pictures.c), never its furniture.
b8                         fude_zoom_plan_opening(const fude_zoom_plan_part* _part);
// Draws _part for half sizes _hw × _hh (its own units), stretched to that box, its curves cut into _segments to a whole
// turn (kept within 16 to 96). Its polylines go into _points and _parts (symbol.h's parts, the first its outline),
// both cleared first. Returns how many parts (0 for a NULL part).
u32  fude_zoom_plan_draw(const fude_zoom_plan_part* _part, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points, rde_arr* _parts);

#endif
