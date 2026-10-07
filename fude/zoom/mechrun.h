// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_MECHRUN_H
#define FUDE_ZOOM_MECHRUN_H

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/mech.h"

// ===========================================================================
// A MECHANISM RUNNING, on its own (mech.h's plan as rigid bodies: RDE's 2D
// physics). Made from a plan — its own copy: the canvas is never read again
// while it runs, nor written —, stepped in 240ths of a second whatever the
// frames are, and what it is now handed out for whoever draws it (page.c):
// where each part has moved to from where it was drawn, where a rope's or a
// spring's end is. As a game's world is to its view.
//
// Each is one of RDE's joints (rde.h): hinges and motors, slides (a slider on
// its rail, a rack), springs, ropes (never longer than drawn, slack when they
// would be shorter), two ropes over a pulley (what one side lets down the
// other takes up), gears meshed with gears or racks; a pulley turns here with
// the rope over it.
// ===========================================================================

typedef struct {
    fude_zoom_mech_plan   plan;          // what it runs (its own copy)
    rde_physics_2d_world* world;         // NULL: not running
    rde_physics_2d_body*  ground;
    rde_arr TYPE(rde_physics_2d_body*) bodies;   // each plan body's (NULL: none of its own: a pivot, a motor, a spring, a rope, a pulley)
    rde_arr TYPE(f64)     inv_mass;      // 1 / each body's mass (0: none of its own, or held still)
    rde_arr TYPE(fude_zoom_sim) moves;   // each body's move from where it was drawn (home units), as last stepped
    rde_arr TYPE(f64)     ropes_were;    // each rope's length (end to end) when it started: a pulley's turn
    f64                   k;             // world units a home unit (its parts about one: what the physics is best at)
    f64                   time;          // seconds run
    f64                   left;          // time not stepped yet (less than a step)
} fude_zoom_mech_world;

void fude_zoom_mech_world_init(fude_zoom_mech_world* _w);
void fude_zoom_mech_world_destroy(fude_zoom_mech_world* _w);
// Made from _plan (copied; any world before let go). False: nothing in it (it is not running).
b8   fude_zoom_mech_world_start(fude_zoom_mech_world* _w, const fude_zoom_mech_plan* _plan);
// Let go: nothing running.
void fude_zoom_mech_world_stop(fude_zoom_mech_world* _w);
b8   fude_zoom_mech_world_on(const fude_zoom_mech_world* _w);
// _dt seconds on (in 240ths of a second; a tenth of a second at most a call).
void fude_zoom_mech_world_step(fude_zoom_mech_world* _w, f64 _dt);
// Body _b's move from where it was drawn (home units; the identity: it has not moved, or it is none).
fude_zoom_sim fude_zoom_mech_world_move(const fude_zoom_mech_world* _w, u32 _b);
// Point _p (home units, as drawn) held by body _b (FUDE_ZOOM_NONE: still) where it is now.
fude_zoom_v2 fude_zoom_mech_world_point(const fude_zoom_mech_world* _w, u32 _b, fude_zoom_v2 _p);

#endif
