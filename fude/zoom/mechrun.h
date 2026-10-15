// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_MECHRUN_H
#define FUDE_ZOOM_MECHRUN_H

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/mech.h"

// ===========================================================================
// Mechanism run: runs a mechanism on its own, as rigid bodies in RDE's 2D physics (mech.h's plan made real).
//
// The world is made from a plan and keeps its own copy of it: the canvas is never read again while it runs, nor
// written. It is stepped in 240ths of a second whatever the frame rate, and what it is now is handed out to whoever
// draws it (page_mech.c): where each part has moved to from where it was drawn, and where a rope's or a spring's end
// is. It is to its drawing as a game's world is to its view.
//
// Each joint is one of RDE's (rde.h): hinges and motors; slides (a slider on its rail, a rack); springs; ropes (never
// longer than drawn, and slack when they would be shorter); two ropes over a pulley (what one side lets down, the
// other takes up); and gears meshed with gears or racks (a rack is let go where its teeth end). A pulley is turned
// here by the rope over it. All are solved together, in 8 sub-steps a step, so that trains of light and heavy gears
// hold. A motor comes up to its speed in a quarter of a second, as a real one does.
//
// Units: home units are the home frame's (millimetres); world units are the physics' (k of them to a home unit);
// times are in seconds, angles in radians counter-clockwise, forces in newtons unless said otherwise.
//
// Where it sits: `fude/zoom`, the canvas layer. page_mech.c runs it in Play and draws from it, and coupling.h joins it
// to a circuit.
// Its main type, fude_zoom_mech_world, is the caller's struct: set up by fude_zoom_mech_world_init, released by
// fude_zoom_mech_world_destroy, made to run by fude_zoom_mech_world_start and stopped by fude_zoom_mech_world_stop.
// Threads: main thread only.
// Design: docs/infinite_canvas_design.md §40 (RDE's own gear and pulley joints, so gears hold), §49 (what a
// mechanism's parts take before they break) and §53 (parts moved by hand as it plays).
// ===========================================================================

#define FUDE_ZOOM_MECH_STEP (1.0 / 240.0)   // its step (seconds)

// A drive motor of the plan, as it runs.
typedef struct {
    rde_physics_2d_joint* joint;
    f64                   speed;         // its speed once up to it (RDE's sense: the ground's turn against its part's)
    u32                   pin;           // its joint's index in the world's joints
    f64                   was;           // its hinge's angle at the last step (radians, within half a turn either way)
    f64                   turned;        // how far it has turned in all (radians: its angle's changes added up)
    // Where it was as the current half second began, and how far into that half second it is (seconds): every half
    // second it checks how far it got, to tell when it is jammed.
    f64                   from, held;    // where it was as this half second began, and how far into it it is
    b8                    jammed;        // reported as jammed (it hardly turned in half a second while it drove)
} fude_zoom_mech_motor;

// The kinds of joint the world holds and watches: each pin, slide, spring, rope and gear mesh. Each is pulled with a
// force, against what it can take: its strength, in newtons (a pin's to shear, a slide's, a rope's to snap, a mesh's
// teeth's to break, as its parts' material and size give them); a spring's strength is how far past its length it
// can stretch before it gives (home units). Past its strength it breaks and is let go, together with what rests on it
// (a mesh, on its gear's pin). A pin whose parts cannot both be where it holds them (drawn so that they do not fit)
// pulls apart, and the world counts how long it has been apart.
typedef enum {
    FUDE_ZOOM_MECH_JOINT_PIN = 0,
    FUDE_ZOOM_MECH_JOINT_SLIDE,
    FUDE_ZOOM_MECH_JOINT_SPRING,
    FUDE_ZOOM_MECH_JOINT_ROPE,
    FUDE_ZOOM_MECH_JOINT_GEAR
} FUDE_ZOOM_MECH_JOINT_;

// A joint as the world holds it.
typedef struct {
    rde_physics_2d_joint* joint;     // NULL: broken
    u8                    kind;      // a FUDE_ZOOM_MECH_JOINT_
    u32                   item;      // what it is in the plan: the index of its hinge, slide, spring, rope or mesh
    u32                   a, b;      // the plan's bodies it joins (FUDE_ZOOM_NONE: the ground)
    fude_zoom_v2          at;        // where it is, as drawn (home units)
    // A pin's anchors, in world units.
    rde_vec_2F            la, lb;    // a pin's anchor on each body (in its own frame; the ground's in the world's)
    f64                   strength;
    f64                   force;     // what pulls it now (newtons; for a spring, its stretch past its length)
    f64                   load;      // force over strength (1: at its strength)
    u32                   on[2];     // the joints it rests on (a mesh: its gears' pins or slide; FUDE_ZOOM_NONE: none)
    f64                   apart;     // a pin's: seconds its parts have been apart (-1: reported already)
    u32                   over;      // steps in a row past its strength (a moment's knock does not break it)
    f64                   rest;      // a spring's length as drawn, or a mesh's pitch radius (home units)
} fude_zoom_mech_joint;

// What happened as it ran, for the page to tell and take away: a joint broke (`force` pulled it past `strength`); a
// motor jammed (held still while it drove: `force` is the speed it got, `strength` the speed it wanted, in radians a
// second); or a pin's parts cannot fit (`force` is how far apart they are, `strength` how far they may be, in home
// units).
typedef enum {
    FUDE_ZOOM_MECH_BROKE = 1,
    FUDE_ZOOM_MECH_JAMMED,
    FUDE_ZOOM_MECH_MISFIT
} FUDE_ZOOM_MECH_EVENT_;

// One thing that happened, of kind FUDE_ZOOM_MECH_EVENT_.
typedef struct {
    u8  kind;
    u32 joint;                       // its joint's index in the world's joints
    f64 force, strength;
} fude_zoom_mech_event;

// A hand crank (mech.h's): its axle's hinge, its body (the plan's), its angle at the last step and how far it has
// turned (radians, every whole turn counted: its angle's changes added up), and, while held, where the hand would have
// it.
typedef struct {
    rde_physics_2d_joint* joint;
    u32                   body;
    f64                   was, turned;
    b8                    held;
    f64                   target;
} fude_zoom_mech_crank;

// A circuit motor's shaft (mech.h's FUDE_ZOOM_MECH_SHAFT): the hinge of what is pinned on it, free or driven as the
// circuit says (coupling.h).
typedef struct {
    rde_physics_2d_joint* joint;
    u32                   body;          // what turns on it (the plan's body)
    u32                   object;        // the circuit's motor (its symbol)
} fude_zoom_mech_shaft;

// A circuit solenoid's plunger (mech.h's FUDE_ZOOM_MECH_PLUNGER): its rod (the plan's body), its solenoid (the
// circuit's symbol), the way out of its coil (a unit vector), where its rod started (world units), how far in it goes
// (home units), and how hard it is pulled in now (in units of torque_unit's force, its unit of mass's weight; less than
// 0 pushes it out), as coupling.h sets it.
typedef struct {
    u32          body;
    u32          object;
    fude_zoom_v2 axis;
    rde_vec_2F   start;
    f64          stroke;
    f64          pull;
} fude_zoom_mech_plunger;

// A running mechanism.
typedef struct {
    fude_zoom_mech_plan   plan;          // what it runs (its own copy)
    rde_physics_2d_world* world;         // NULL: not running
    rde_physics_2d_body*  ground;
    rde_arr TYPE(rde_physics_2d_body*) bodies;   // each plan body's (NULL: a pivot, motor, spring, rope or pulley)
    rde_arr TYPE(f64)     inv_mass;      // 1 / each body's mass (0: no body of its own, or held still)
    rde_arr TYPE(fude_zoom_sim) moves;   // each body's move from where it was drawn (home units), as last stepped
    rde_arr TYPE(f64)     ropes_were;    // each rope's length (end to end) when it started, to turn its pulley by
    rde_arr TYPE(fude_zoom_mech_motor) motors;
    rde_arr TYPE(fude_zoom_mech_shaft) shafts;
    rde_arr TYPE(fude_zoom_mech_crank) cranks;
    rde_arr TYPE(fude_zoom_mech_plunger) plungers;
    rde_arr TYPE(f64)     turned;        // each body's turn since it started (radians, counter-clockwise, all turns)
    rde_arr TYPE(f64)     turned_was;    // each body's angle as last read
    // A part taken hold of by hand (FUDE_ZOOM_NONE: none): where on it (its own frame, world units), and where it is
    // dragged to (home units).
    u32                   grab;
    rde_vec_2F            grab_on;
    fude_zoom_v2          grab_to;
    rde_arr TYPE(fude_zoom_mech_joint) joints;
    rde_arr TYPE(fude_zoom_mech_event) events;
    // The paths of its tracers (the plan's): for each tracer, an array of the points where it has been (home units, a
    // little apart), in order, so a tracer's path is read without going through the others'; how many points they keep
    // together (at most FUDE_ZOOM_MECH_TRAIL_MOST); and each tracer's last point kept (NaN: none yet). The arrays are
    // made as tracers come, emptied (their memory kept) when the world stops, freed with it.
    rde_arr TYPE(rde_arr)      trails;
    u32                        trail_points;
    rde_arr TYPE(fude_zoom_v2) trail_last;
    f64                   k;             // world units to a home unit (parts about 1 across suit the physics best)
    f64                   time;          // seconds run
    f64                   left;          // the time not stepped yet, less than a step (seconds)
} fude_zoom_mech_world;

// Sets up a world that is not running, and frees what one holds.
void fude_zoom_mech_world_init(fude_zoom_mech_world* _w);
void fude_zoom_mech_world_destroy(fude_zoom_mech_world* _w);
// Makes the world from _plan, which it copies; any world it ran before is let go first. Returns false when there is
// nothing in the plan (and then it is not running).
b8   fude_zoom_mech_world_start(fude_zoom_mech_world* _w, const fude_zoom_mech_plan* _plan);
// Lets the world go: nothing runs until it is started again.
void fude_zoom_mech_world_stop(fude_zoom_mech_world* _w);
// Returns whether it is running.
b8   fude_zoom_mech_world_on(const fude_zoom_mech_world* _w);
// Runs it on by _dt seconds, in steps of FUDE_ZOOM_MECH_STEP (at most a tenth of a second a call); the time left over
// is kept for the next call.
void fude_zoom_mech_world_step(fude_zoom_mech_world* _w, f64 _dt);
// Runs it one step on (FUDE_ZOOM_MECH_STEP), whatever time is owed: for a caller that keeps the time itself (a
// circuit played with it, coupling.h).
void fude_zoom_mech_world_tick(fude_zoom_mech_world* _w);

// Returns the shaft of the circuit's motor _object, or FUDE_ZOOM_NONE when there is none (nothing on it, or it is not
// in this world).
u32  fude_zoom_mech_world_shaft(const fude_zoom_mech_world* _w, u32 _object);
// Returns how fast what is on shaft _shaft turns (radians a second, counter-clockwise).
f64  fude_zoom_mech_world_shaft_spin(const fude_zoom_mech_world* _w, u32 _shaft);
// Returns how far round what is on shaft _shaft is (radians, counter-clockwise, as the world has it).
f64  fude_zoom_mech_world_shaft_angle(const fude_zoom_mech_world* _w, u32 _shaft);
// Drives shaft _shaft toward _spin (radians a second, counter-clockwise) with at most _torque (in torque_unit's); a
// torque of 0 or less leaves it free.
void fude_zoom_mech_world_shaft_drive(fude_zoom_mech_world* _w, u32 _shaft, f64 _spin, f64 _torque);
// Returns the scale of the world's torques: the torque of its unit of mass's weight at its unit of length.
f64  fude_zoom_mech_world_torque_unit(const fude_zoom_mech_world* _w);
// Returns how far body _body has turned since it started (radians, counter-clockwise, every whole turn counted), which
// a wheel's belt and a worm's threads follow.
f64  fude_zoom_mech_world_turned(const fude_zoom_mech_world* _w, u32 _body);
// Returns how far along its slide body _body has gone from where it started (home units; 0 when it has no slide), as
// a cam's follower has.
f64  fude_zoom_mech_world_slid(const fude_zoom_mech_world* _w, u32 _body);
// Returns the plunger of the circuit's solenoid _object, or FUDE_ZOOM_NONE when there is none.
u32  fude_zoom_mech_world_plunger(const fude_zoom_mech_world* _w, u32 _object);
// Pulls plunger _plunger in as hard as _pull (in units of torque_unit's force; less than 0 pushes it out), on every
// step from now on.
void fude_zoom_mech_world_plunger_pull(fude_zoom_mech_world* _w, u32 _plunger, f64 _pull);
// Returns how far in plunger _plunger is: 0 all the way out (as it started), 1 all the way in.
f64  fude_zoom_mech_world_plunger_in(const fude_zoom_mech_world* _w, u32 _plunger);
// Returns whether point _at (home units) is under a part that moves, as it is now (not the ground, a wall or a fixed
// drawing).
b8   fude_zoom_mech_world_covers(const fude_zoom_mech_world* _w, fude_zoom_v2 _at);

// Returns whether the plan's _item of kind _kind (a FUDE_ZOOM_MECH_JOINT_: its spring, its rope…) has broken.
b8   fude_zoom_mech_world_broken(const fude_zoom_mech_world* _w, u8 _kind, u32 _item);
// Returns body _b's move from where it was drawn (home units); the identity when it has not moved or there is no such
// body.
fude_zoom_sim fude_zoom_mech_world_move(const fude_zoom_mech_world* _w, u32 _b);
// Returns where point _p (home units, as drawn) is now, carried by body _b (FUDE_ZOOM_NONE: it stays still).
fude_zoom_v2 fude_zoom_mech_world_point(const fude_zoom_mech_world* _w, u32 _b, fude_zoom_v2 _p);
// --- by hand, as it plays ---
#define FUDE_ZOOM_MECH_HAND_TORQUE 20.0   // N·m: as hard as a hand turns a crank
#define FUDE_ZOOM_MECH_HAND_HZ     12.0   // how stiffly a held part follows the hand (a damped spring for its mass)
// Returns the hand crank whose disc holds point _at (home units), or FUDE_ZOOM_NONE when there is none.
u32  fude_zoom_mech_world_crank_at(const fude_zoom_mech_world* _w, fude_zoom_v2 _at);
// Holds crank _c and turns it toward _angle (radians, counter-clockwise, every whole turn counted), at most as hard as
// a hand turns.
void fude_zoom_mech_world_crank_hold(fude_zoom_mech_world* _w, u32 _c, f64 _angle);
// Lets crank _c go: it turns freely but for its axle's friction (its text's, else mech.h's
// FUDE_ZOOM_MECH_CRANK_FRICTION).
void fude_zoom_mech_world_crank_let_go(fude_zoom_mech_world* _w, u32 _c);
// fude_zoom_mech_world_crank_angle returns how far crank _c has turned since it started (radians, counter-clockwise,
// every whole turn counted); fude_zoom_mech_world_crank_middle returns its middle (home units).
f64  fude_zoom_mech_world_crank_angle(const fude_zoom_mech_world* _w, u32 _c);
fude_zoom_v2 fude_zoom_mech_world_crank_middle(const fude_zoom_mech_world* _w, u32 _c);
// fude_zoom_mech_world_grab takes hold of what moves under _at (home units), at that point; from then on it is pulled
// toward where it is dragged (fude_zoom_mech_world_drag) until it is let go. Returns false when nothing that moves is
// there.
b8   fude_zoom_mech_world_grab(fude_zoom_mech_world* _w, fude_zoom_v2 _at);
void fude_zoom_mech_world_drag(fude_zoom_mech_world* _w, fude_zoom_v2 _to);
void fude_zoom_mech_world_let_go(fude_zoom_mech_world* _w);
// Gives where the held point of the part held is now (home units). Returns false when nothing is held.
b8   fude_zoom_mech_world_grabbed(const fude_zoom_mech_world* _w, fude_zoom_v2* _at);

#define FUDE_ZOOM_MECH_TRAIL_MOST 40000u   // the most tracer points kept, all tracers together; later ones are not kept
// Returns tracer _t's path (the plan's tracer): its points in order, in home units, how many in *_count (0, and NULL
// returned, when it has none or there is no such tracer). The points are the world's own, read in place, not copied
// (the page draws them every frame): valid until the world next steps, stops or is destroyed.
const fude_zoom_v2* fude_zoom_mech_world_trail(const fude_zoom_mech_world* _w, u32 _t, u32* _count);
#define FUDE_ZOOM_MECH_TRAIL_STEP 0.6f   // screen points: a point closer than this to the last one kept adds nothing seen
// A tracer's path as it is drawn (page_mech.c draws each stretch as one stroke, as the pen's lines are drawn): the
// path's _count points (home units) taken to the screen by _to_screen into _points (rde_vec_2F, cleared first), but a
// point closer than FUDE_ZOOM_MECH_TRAIL_STEP to the last one kept (the first and the last are always kept); then into
// _runs (u32 pairs, cleared first) each stretch of those that comes within _radius of the screen (half size _half, its
// middle at 0): its first point in _points and how many points it has, at least 2. Returns how many stretches. Far
// out of sight, a stroke's joins would still be made, so only what comes near the screen is drawn.
u32  fude_zoom_mech_trail_runs(const fude_zoom_v2* _path, u32 _count, fude_zoom_sim _to_screen, fude_zoom_v2 _half, f32 _radius, rde_arr* _points, rde_arr* _runs);

#endif
