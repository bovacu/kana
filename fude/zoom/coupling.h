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
// A SERVO of the circuit with a part of the mechanism on its middle turns it
// to the angle its pulses say, as fast as it can and with no more than its
// stall torque — held back, it strains and draws its stall current; with no
// pulses or no supply it lets it be. Its horn is where that part is.
//
// A PUSH BUTTON of the circuit is pressed while a part of the mechanism that
// moves is over its middle (a rack at the end of its travel, a cam, an arm
// going by): a limit switch, a counter's sensor. A SLOTTED SENSOR's slot is
// blocked while one is in it, across its beam.
//
// A SOLENOID's plunger is the mechanism's: pulled in as hard as its coil's
// current pulls it (FUDE_ZOOM_COUPLING_PULL its rated pull, halfway in, at
// its rated current), pushed out by its spring; what is pinned on its end it
// pulls, what is in its way it pushes. A STEPPER turns what is on its shaft
// toward where its coils' field is, as hard as its holding torque
// (FUDE_ZOOM_COUPLING_HOLD), and holds it there — pushed harder, it slips. A
// ROTARY ENCODER's contacts and a POT's wiper go as what is on their shafts
// turns (a pot's 270°, end to end, clockwise up).
//
// Solved so the two stay steady: over a step the circuit, as it loads the
// motor (how much more current a volt less of back-EMF makes, and the back-
// EMF at which none flows), is a straight line, and the motor's torque is
// that line's at the speed the shaft has — the shaft's motor brought toward
// where no current flows with no more torque than the current there makes.
// ===========================================================================

#define FUDE_ZOOM_COUPLING_STALL 50.0   // a motor's torque held still at its rated volts (the world's torque units)
#define FUDE_ZOOM_COUPLING_PULL  40.0   // a solenoid's rated pull (the world's forces: its unit of mass's weight)
#define FUDE_ZOOM_COUPLING_HOLD  30.0   // a stepper's holding torque, its coils at 5 V (the world's torque units)

typedef struct {
    u32 part;                 // the circuit's motor (its place in the circuit's parts)
    u32 shaft;                // the world's shaft (mechrun.h)
} fude_zoom_coupled_motor;

typedef struct {
    u32 part;                 // the circuit's servo
    u32 shaft;                // the world's shaft
    f64 offset;               // its horn's angle less what is on its shaft's, as they began (radians)
} fude_zoom_coupled_servo;

typedef struct {
    u32          part;        // the circuit's push button
    fude_zoom_v2 at;          // its middle (home units)
} fude_zoom_coupled_button;

// A stepper, an encoder or a pot on a shaft: its own reading as it began (a stepper's rotor's angle, an encoder's
// detents, a pot's wiper), the shaft's angle as last read, and how far the shaft has turned since it began (radians,
// counter-clockwise, every turn counted).
typedef struct {
    u32 part;
    u32 shaft;
    f64 from;
    f64 was;
    f64 turned;
} fude_zoom_coupled_shaft;

typedef struct {
    u32 part;                 // the circuit's solenoid
    u32 plunger;              // the world's plunger (mechrun.h)
} fude_zoom_coupled_solenoid;

typedef struct {
    rde_arr TYPE(fude_zoom_coupled_motor)  motors;
    rde_arr TYPE(fude_zoom_coupled_servo)  servos;
    rde_arr TYPE(fude_zoom_coupled_button) buttons;
    rde_arr TYPE(fude_zoom_coupled_button) slots;      // (a slotted sensor's: its beam's middle)
    rde_arr TYPE(fude_zoom_coupled_shaft)  steppers;
    rde_arr TYPE(fude_zoom_coupled_shaft)  encoders;
    rde_arr TYPE(fude_zoom_coupled_shaft)  pots;
    rde_arr TYPE(fude_zoom_coupled_solenoid) solenoids;
    f64     left;             // time not stepped yet (less than a step)
} fude_zoom_coupling;

void fude_zoom_coupling_init(fude_zoom_coupling* _k);
void fude_zoom_coupling_destroy(fude_zoom_coupling* _k);
// Nothing coupled (the circuit's motors and buttons as they are by themselves).
void fude_zoom_coupling_clear(fude_zoom_coupling* _k, fude_zoom_circuit* _c);
// What of circuit _c world _w works with (_s: where its buttons are): its motors, servos, steppers, encoders and pots
// with something on their shafts, its solenoids' plungers, its push buttons and slotted sensors. How many.
u32  fude_zoom_coupling_build(fude_zoom_coupling* _k, fude_zoom_circuit* _c, const fude_zoom_mech_world* _w, const fude_zoom_scene* _s);
// _dt seconds on, both together (a tenth of a second at most). False: the circuit was not solved.
b8   fude_zoom_coupling_step(fude_zoom_coupling* _k, fude_zoom_circuit* _c, fude_zoom_mech_world* _w, f64 _dt);
// Whether the mechanism works circuit part _part (its place in the circuit's parts) — an encoder or a pot its shaft
// turns, a slotted sensor whose beam it blocks —: not a hand's to tap (what is on its shaft is: a crank).
b8   fude_zoom_coupling_works(const fude_zoom_coupling* _k, u32 _part);

#endif
