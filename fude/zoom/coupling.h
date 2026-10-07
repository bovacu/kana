// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_COUPLING_H
#define FUDE_ZOOM_COUPLING_H

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/scene.h"
#include "zoom/circuit.h"
#include "zoom/mechrun.h"

// ===========================================================================
// A CIRCUIT AND A MECHANISM PLAYED TOGETHER (Sketching's Play, what the lasso
// holds of both): stepped together, a mechanism's step (FUDE_ZOOM_MECH_STEP)
// at a time, each told between what the other did.
//
// A MOTOR of the circuit with a part of the mechanism pinned on its middle
// (a gear's hole, a link's, a wheel's over it: its shaft) turns that part, and
// is turned by it — a DC motor: its winding (FUDE_ZOOM_MOTOR_R) behind a back-
// EMF as fast as its shaft turns (its text: its rated volts and its speed at
// them, "6V 60rpm"), its torque as the current through it. Driven, it comes
// up to the speed its volts make, slower under a load, drawing more as it is
// held back; turned by the mechanism, it is a dynamo: its volts as fast as it
// turns, what it lights holding it back, nothing joined to it nothing. At its
// rated volts, held still, it turns as hard as FUDE_ZOOM_COUPLING_STALL times
// the mechanism's unit of mass hung from its unit of length.
//
// A PUSH BUTTON of the circuit is pressed while a part of the mechanism that
// moves is over its middle (a rack at the end of its travel, a cam, an arm
// going by): a limit switch, a counter's sensor.
//
// Solved so the two stay steady: over a step the circuit, as it loads the
// motor (how much more current a volt less of back-EMF makes, and the back-
// EMF at which none flows), is a straight line, and the motor's torque is
// that line's at the speed the shaft has — the shaft's motor brought toward
// where no current flows with no more torque than the current there makes.
// ===========================================================================

#define FUDE_ZOOM_COUPLING_STALL 50.0   // a motor's torque held still at its rated volts (the world's torque units)

typedef struct {
    u32 part;                 // the circuit's motor (its place in the circuit's parts)
    u32 shaft;                // the world's shaft (mechrun.h)
} fude_zoom_coupled_motor;

typedef struct {
    u32          part;        // the circuit's push button
    fude_zoom_v2 at;          // its middle (home units)
} fude_zoom_coupled_button;

typedef struct {
    rde_arr TYPE(fude_zoom_coupled_motor)  motors;
    rde_arr TYPE(fude_zoom_coupled_button) buttons;
    f64     left;             // time not stepped yet (less than a step)
    f64     owed;             // the circuit's time not stepped yet (less than its step)
} fude_zoom_coupling;

void fude_zoom_coupling_init(fude_zoom_coupling* _k);
void fude_zoom_coupling_destroy(fude_zoom_coupling* _k);
// Nothing coupled (the circuit's motors and buttons as they are by themselves).
void fude_zoom_coupling_clear(fude_zoom_coupling* _k, fude_zoom_circuit* _c);
// What of circuit _c world _w works with (_s: where its buttons are): its motors with something on their shafts, its push
// buttons. How many.
u32  fude_zoom_coupling_build(fude_zoom_coupling* _k, fude_zoom_circuit* _c, const fude_zoom_mech_world* _w, const fude_zoom_scene* _s);
// _dt seconds on, both together (a tenth of a second at most). False: the circuit was not solved.
b8   fude_zoom_coupling_step(fude_zoom_coupling* _k, fude_zoom_circuit* _c, fude_zoom_mech_world* _w, f64 _dt);

#endif
