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
// other takes up), gears meshed with gears or racks (a rack let go where its
// teeth end); a pulley turns here with the rope over it. All solved together,
// in 8 sub-steps a step (trains of gears light and heavy hold); a motor comes
// up to its speed in a quarter of a second, as one does.
// ===========================================================================

#define FUDE_ZOOM_MECH_STEP (1.0 / 240.0)   // its step (seconds)

typedef struct {
    rde_physics_2d_joint* joint;
    f64                   speed;         // its speed, once up to it (RDE's way round: the ground's turn against its part's)
} fude_zoom_mech_motor;

// A circuit's motor's shaft (mech.h' FUDE_ZOOM_MECH_SHAFT): the hinge of what is pinned on it, free, or driven as the
// circuit says (coupling.h).
typedef struct {
    rde_physics_2d_joint* joint;
    u32                   body;          // what turns on it (the plan's)
    u32                   object;        // the circuit's motor (its symbol)
} fude_zoom_mech_shaft;

typedef struct {
    fude_zoom_mech_plan   plan;          // what it runs (its own copy)
    rde_physics_2d_world* world;         // NULL: not running
    rde_physics_2d_body*  ground;
    rde_arr TYPE(rde_physics_2d_body*) bodies;   // each plan body's (NULL: none of its own: a pivot, a motor, a spring, a rope, a pulley)
    rde_arr TYPE(f64)     inv_mass;      // 1 / each body's mass (0: none of its own, or held still)
    rde_arr TYPE(fude_zoom_sim) moves;   // each body's move from where it was drawn (home units), as last stepped
    rde_arr TYPE(f64)     ropes_were;    // each rope's length (end to end) when it started: a pulley's turn
    rde_arr TYPE(fude_zoom_mech_motor) motors;
    rde_arr TYPE(fude_zoom_mech_shaft) shafts;
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
// One step on (FUDE_ZOOM_MECH_STEP), whatever is owed (another's: a circuit's played with it, coupling.h).
void fude_zoom_mech_world_tick(fude_zoom_mech_world* _w);

// The shaft of the circuit's motor _object (FUDE_ZOOM_NONE: none: nothing on it, or not this world's).
u32  fude_zoom_mech_world_shaft(const fude_zoom_mech_world* _w, u32 _object);
// How fast what is on shaft _shaft turns (radians a second, counter-clockwise).
f64  fude_zoom_mech_world_shaft_spin(const fude_zoom_mech_world* _w, u32 _shaft);
// Shaft _shaft driven toward _spin (radians a second, counter-clockwise) with at most _torque (in torque_unit's); at
// most 0: free.
void fude_zoom_mech_world_shaft_drive(fude_zoom_mech_world* _w, u32 _shaft, f64 _spin, f64 _torque);
// The world's torques' scale: what its unit of mass weighs at its unit of length (a turn's worth of what its parts are).
f64  fude_zoom_mech_world_torque_unit(const fude_zoom_mech_world* _w);
// Is point _at (home units) under a part that moves (as it is now)?
b8   fude_zoom_mech_world_covers(const fude_zoom_mech_world* _w, fude_zoom_v2 _at);

// Body _b's move from where it was drawn (home units; the identity: it has not moved, or it is none).
fude_zoom_sim fude_zoom_mech_world_move(const fude_zoom_mech_world* _w, u32 _b);
// Point _p (home units, as drawn) held by body _b (FUDE_ZOOM_NONE: still) where it is now.
fude_zoom_v2 fude_zoom_mech_world_point(const fude_zoom_mech_world* _w, u32 _b, fude_zoom_v2 _p);

#endif
