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
    u32                   pin;           // its joint (joints': below)
    f64                   was;           // its hinge's angle last step (radians, as RDE has it: within a half turn either way)
    f64                   turned;        // how far it has turned, all told (radians: its angle's steps added up)
    f64                   from, held;    // where it was as this half second began, and how far into it it is
    b8                    jammed;        // said: hardly on in a half second while it drives (until it turns again)
} fude_zoom_mech_motor;

// A joint as the world holds it — each pin, slide, spring, rope, gear mesh —: how hard it is pulled, against what it takes
// (its strength: newtons — a pin's to shear, a slide's, a rope's to snap, a mesh's teeth's to break, as its parts'
// material and size have them; a spring's: how far past its length it stretches before it gives, home units). Past it:
// broken, let go, with what rests on it (a mesh on its gear's pin). A pin whose parts cannot both be where it holds them
// (drawn so that they do not fit) pulls apart: how long it has.
typedef enum {
    FUDE_ZOOM_MECH_JOINT_PIN = 0,
    FUDE_ZOOM_MECH_JOINT_SLIDE,
    FUDE_ZOOM_MECH_JOINT_SPRING,
    FUDE_ZOOM_MECH_JOINT_ROPE,
    FUDE_ZOOM_MECH_JOINT_GEAR
} FUDE_ZOOM_MECH_JOINT_;

typedef struct {
    rde_physics_2d_joint* joint;     // NULL: broken
    u8                    kind;      // FUDE_ZOOM_MECH_JOINT_
    u32                   item;      // what in the plan it is (its hinge's, slide's, spring's, rope's, mesh's place)
    u32                   a, b;      // the plan's bodies (FUDE_ZOOM_NONE: the ground)
    fude_zoom_v2          at;        // where, as drawn (home units)
    rde_vec_2F            la, lb;    // a pin's: where it holds each (each one's own, world units; the ground's: the world's)
    f64                   strength;
    f64                   force;     // what it is pulled with now (newtons; a spring's: its stretch past its length)
    f64                   load;      // force over strength (1: at it)
    u32                   on[2];     // the joints it rests on (a mesh: its gears' pins or slide; FUDE_ZOOM_NONE: none)
    f64                   apart;     // seconds its parts have been apart (a pin's; -1: said)
    u32                   over;      // steps it has been past its strength in a row (a moment's knock is not a load)
    f64                   rest;      // a spring's length as drawn (home units)
} fude_zoom_mech_joint;

// What happened as it ran, for the page to tell (taken by it): a joint broken (what pulled it, its strength), a motor
// jammed (held still while it drives), a pin whose parts cannot fit.
typedef enum {
    FUDE_ZOOM_MECH_BROKE = 1,
    FUDE_ZOOM_MECH_JAMMED,
    FUDE_ZOOM_MECH_MISFIT
} FUDE_ZOOM_MECH_EVENT_;

typedef struct {
    u8  kind;
    u32 joint;                       // its joint (joints')
    f64 force, strength;
} fude_zoom_mech_event;

// A hand crank (mech.h's): its axle's hinge, its body (the plan's), how far it has turned (radians, every turn counted:
// its angle's steps added up), and, held, where the hand would have it.
typedef struct {
    rde_physics_2d_joint* joint;
    u32                   body;
    f64                   was, turned;
    b8                    held;
    f64                   target;
} fude_zoom_mech_crank;

// A circuit's motor's shaft (mech.h' FUDE_ZOOM_MECH_SHAFT): the hinge of what is pinned on it, free, or driven as the
// circuit says (coupling.h).
typedef struct {
    rde_physics_2d_joint* joint;
    u32                   body;          // what turns on it (the plan's)
    u32                   object;        // the circuit's motor (its symbol)
} fude_zoom_mech_shaft;

// A circuit's solenoid's plunger (mech.h' FUDE_ZOOM_MECH_PLUNGER): its rod (the plan's body), its solenoid (the circuit's
// symbol), the way out of its coil (unit), where its rod started (world units), how far in it goes (home units), and how
// hard it is pulled in now (in torque_unit's force — its unit of mass's weight —; less than 0: pushed out) — coupling's.
typedef struct {
    u32          body;
    u32          object;
    fude_zoom_v2 axis;
    rde_vec_2F   start;
    f64          stroke;
    f64          pull;
} fude_zoom_mech_plunger;

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
    rde_arr TYPE(fude_zoom_mech_crank) cranks;
    rde_arr TYPE(fude_zoom_mech_plunger) plungers;
    rde_arr TYPE(f64)     turned;        // each body's turn since it started (radians, counter-clockwise, every turn counted)
    rde_arr TYPE(f64)     turned_was;    // (its angle as last read)
    // A part taken hold of by hand (FUDE_ZOOM_NONE: none): where on it (its own frame, world units), where it is dragged to
    // (home units).
    u32                   grab;
    rde_vec_2F            grab_on;
    fude_zoom_v2          grab_to;
    rde_arr TYPE(fude_zoom_mech_joint) joints;
    rde_arr TYPE(fude_zoom_mech_event) events;
    // Its tracers' paths (the plan's tracers): each point where one has been (home units, a little apart), whose it is.
    rde_arr TYPE(fude_zoom_v2) trail;
    rde_arr TYPE(u32)          trail_of;
    rde_arr TYPE(fude_zoom_v2) trail_last;   // each tracer's last point (NAN: none yet)
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
// How far round what is on shaft _shaft is (radians, counter-clockwise: as the world has it).
f64  fude_zoom_mech_world_shaft_angle(const fude_zoom_mech_world* _w, u32 _shaft);
// Shaft _shaft driven toward _spin (radians a second, counter-clockwise) with at most _torque (in torque_unit's); at
// most 0: free.
void fude_zoom_mech_world_shaft_drive(fude_zoom_mech_world* _w, u32 _shaft, f64 _spin, f64 _torque);
// The world's torques' scale: what its unit of mass weighs at its unit of length (a turn's worth of what its parts are).
f64  fude_zoom_mech_world_torque_unit(const fude_zoom_mech_world* _w);
// Body _body's turn since it started (radians, counter-clockwise, every turn counted: its wheel's belt, its worm's threads).
f64  fude_zoom_mech_world_turned(const fude_zoom_mech_world* _w, u32 _body);
// How far along its slide body _body has gone from where it started (home units; 0: it has none) — a follower's.
f64  fude_zoom_mech_world_slid(const fude_zoom_mech_world* _w, u32 _body);
// The plunger of the circuit's solenoid _object (FUDE_ZOOM_NONE: none).
u32  fude_zoom_mech_world_plunger(const fude_zoom_mech_world* _w, u32 _object);
// Plunger _plunger pulled in as hard as _pull (in torque_unit's force; less than 0: pushed out), each step from now.
void fude_zoom_mech_world_plunger_pull(fude_zoom_mech_world* _w, u32 _plunger, f64 _pull);
// How far in plunger _plunger is (0: all the way out, as it started; 1: all the way in).
f64  fude_zoom_mech_world_plunger_in(const fude_zoom_mech_world* _w, u32 _plunger);
// Is point _at (home units) under a part that moves (as it is now)?
b8   fude_zoom_mech_world_covers(const fude_zoom_mech_world* _w, fude_zoom_v2 _at);

// Has the plan's _item of kind _kind (FUDE_ZOOM_MECH_JOINT_: its spring, its rope…) broken?
b8   fude_zoom_mech_world_broken(const fude_zoom_mech_world* _w, u8 _kind, u32 _item);
// Body _b's move from where it was drawn (home units; the identity: it has not moved, or it is none).
fude_zoom_sim fude_zoom_mech_world_move(const fude_zoom_mech_world* _w, u32 _b);
// Point _p (home units, as drawn) held by body _b (FUDE_ZOOM_NONE: still) where it is now.
fude_zoom_v2 fude_zoom_mech_world_point(const fude_zoom_mech_world* _w, u32 _b, fude_zoom_v2 _p);
// --- by hand, as it plays ---
#define FUDE_ZOOM_MECH_HAND_TORQUE 20.0   // N·m: as hard as a hand turns a crank
#define FUDE_ZOOM_MECH_HAND_HZ     12.0   // how stiffly a part held follows the hand (a spring as its mass makes this, damped)
// The hand crank whose disc holds point _at (home units; FUDE_ZOOM_NONE: none).
u32  fude_zoom_mech_world_crank_at(const fude_zoom_mech_world* _w, fude_zoom_v2 _at);
// Crank _c held: turned toward _angle (radians, counter-clockwise, every turn counted) as hard as a hand turns, at most.
void fude_zoom_mech_world_crank_hold(fude_zoom_mech_world* _w, u32 _c, f64 _angle);
// ...let go: it turns free but for its axle's friction (its text's, else FUDE_ZOOM_MECH_CRANK_FRICTION: mech.h).
void fude_zoom_mech_world_crank_let_go(fude_zoom_mech_world* _w, u32 _c);
// How far crank _c has turned since it started (radians, counter-clockwise, every turn counted); its middle (home units).
f64  fude_zoom_mech_world_crank_angle(const fude_zoom_mech_world* _w, u32 _c);
fude_zoom_v2 fude_zoom_mech_world_crank_middle(const fude_zoom_mech_world* _w, u32 _c);
// What moves under _at (home units) taken hold of there: pulled toward where it is dragged. False: nothing that moves.
b8   fude_zoom_mech_world_grab(fude_zoom_mech_world* _w, fude_zoom_v2 _at);
void fude_zoom_mech_world_drag(fude_zoom_mech_world* _w, fude_zoom_v2 _to);
void fude_zoom_mech_world_let_go(fude_zoom_mech_world* _w);
// Where on it the part held is now (home units; false: none held).
b8   fude_zoom_mech_world_grabbed(const fude_zoom_mech_world* _w, fude_zoom_v2* _at);

#define FUDE_ZOOM_MECH_TRAIL_MOST 40000u   // its tracers' points at most (all of theirs together: the rest not kept)
// Tracer _t's path (the plan's tracer): its points, in turn, into _out (home units). How many.
u32  fude_zoom_mech_world_trail(const fude_zoom_mech_world* _w, u32 _t, rde_arr* _out);

#endif
