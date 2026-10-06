// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_NEST_H
#define FUDE_ZOOM_NEST_H

#include "rde.h"

// ===========================================================================
// Fitting parts into a board (research §2.10, item 9: the cut list's other
// half): rectangles placed on a rectangle with the saw's kerf between them,
// as many and as much of them as fit.
//
// MaxRects (Jylänki's): the board's free room kept as the largest rectangles
// it holds; each part goes into the one it leaves least over on its shorter
// side (Best Short Side Fit), and every free rectangle it overlaps is split
// round it. Each part is tried turned too when it may be (a part whose grain
// must run along the board's may not). The parts are tried in several orders
// (by area, by their longer side, their shorter, their perimeter, their width,
// their height) and the order that puts down the most wood wins. The kerf is
// made room for by growing each part and the board by it: neighbours a kerf
// apart, the board's edges none.
// ===========================================================================

typedef struct {
    f64 w, h;       // its size across and up the board (a turn swaps them)
    b8  can_turn;   // may it be turned a quarter (its grain not minded)
} fude_zoom_nest_part;

typedef struct {
    f64 x, y;       // its lower left corner on the board (the board from 0, 0)
    f64 w, h;       // its size as placed
    b8  turned;     // a quarter turn from how it was given
    b8  placed;     // false: it did not fit
} fude_zoom_nest_place;

// _n parts onto a board _board_w by _board_h, _kerf between them: each one's
// place into _out (_n of them). How many fit.
u32 fude_zoom_nest(f64 _board_w, f64 _board_h, f64 _kerf, const fude_zoom_nest_part* _parts, u32 _n, fude_zoom_nest_place* _out);

#endif
