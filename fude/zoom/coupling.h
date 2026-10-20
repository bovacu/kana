// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_COUPLING_H
#define FUDE_ZOOM_COUPLING_H

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/scene.h"
#include "zoom/circuit.h"
#include "zoom/mechrun.h"

// ===========================================================================
// Coupling: plays a circuit and a mechanism together (Play, with what the lasso holds of both). They are
// stepped together, one mechanism step (FUDE_ZOOM_MECH_STEP) at a time, and between steps each is told what the
// other did.
//
// - A MOTOR of the circuit with a part of the mechanism pinned on its middle (a gear's hole, a link's, a wheel over
//   it: its shaft) turns that part, and is turned by it. It is a DC motor: its winding (FUDE_ZOOM_MOTOR_R) behind a
//   back-EMF that grows with its shaft's speed (its text gives its rated volts and its speed at them, "6V 60rpm"),
//   and its torque grows with the current through it. Driven, it comes up to the speed its volts make, turning slower
//   under a load and drawing more current as it is held back. Turned by the mechanism it is a dynamo: its volts grow
//   with its speed, whatever it lights holds it back, and with nothing joined to it nothing does. At its rated volts,
//   held still, it turns as hard as FUDE_ZOOM_COUPLING_STALL times the mechanism's unit of mass hung from its unit of
//   length.
// - A SERVO of the circuit with a part of the mechanism on its middle turns that part to the angle its pulses say, as
//   fast as it can and with no more than its stall torque. Held back, it strains and draws its stall current; with
//   no pulses or no supply it leaves the part free. Its horn is where that part is.
// - A PUSH BUTTON of the circuit is pressed while a moving part of the mechanism is over its middle (a rack at the end
//   of its travel, a cam, an arm going by), as a limit switch or a counter's sensor is. A SLOTTED SENSOR's beam is
//   blocked while a moving part is across its slot.
// - A SOLENOID's plunger is a part of the mechanism. It is pulled in as hard as its coil's current pulls it
//   (FUDE_ZOOM_COUPLING_PULL is its rated pull, halfway in, at its rated current) and pushed out by its spring; it
//   pulls what is pinned on its end, and pushes what is in its way.
// - A STEPPER turns what is on its shaft toward where its coils' field points, as hard as its holding torque
//   (FUDE_ZOOM_COUPLING_HOLD), and holds it there; pushed harder, it slips.
// - A ROTARY ENCODER's contacts and a POT's wiper move as what is on their shafts turns (a pot turns 270° from end to
//   end, its value rising clockwise).
//
// Where it sits: `fude/zoom`, the canvas layer, between circuit.h and mechrun.h; page_mech.c builds and steps it in
// Play.
// Its main type, fude_zoom_coupling, is the caller's struct (the page keeps one: page.h), set up by
// fude_zoom_coupling_init and released by fude_zoom_coupling_destroy. It holds indices into the circuit's parts and the
// world's shafts and plungers, so it must be built again whenever either is.
// Threads: main thread only, like the circuit and the world it joins.
//
// Design: a motor is solved so that the two stay steady together. Over one step, the circuit as it loads the motor is
// taken as a straight line: how much more current one volt less of back-EMF makes, and the back-EMF at which no
// current flows. The motor's torque is that line's at the speed the shaft has: the shaft is driven toward the speed
// at which no current would flow, with no more torque than the current at its present speed makes. More in
// docs/infinite_canvas_design.md §42 and §54.
// ===========================================================================

#define FUDE_ZOOM_COUPLING_STALL 50.0   // a motor's torque held still at its rated volts (in the world's torque units)
#define FUDE_ZOOM_COUPLING_PULL  40.0   // a solenoid's rated pull (in the world's forces: its unit of mass's weight)
#define FUDE_ZOOM_COUPLING_HOLD  30.0   // a stepper's holding torque with its coils at 5 V (the world's torque units)

// A circuit's motor with something on its shaft.
typedef struct {
    u32 part;                 // the circuit's motor (its index in the circuit's parts)
    u32 shaft;                // the world's shaft (mechrun.h)
} fude_zoom_coupled_motor;

// A circuit's servo with something on its shaft.
typedef struct {
    u32 part;                 // the circuit's servo
    u32 shaft;                // the world's shaft
    f64 offset;               // its horn's angle less the angle of what is on its shaft, as they began (radians)
} fude_zoom_coupled_servo;

// A circuit's push button, or a slotted sensor, that the mechanism's moving parts can press or block.
typedef struct {
    u32          part;        // the circuit's push button
    fude_zoom_v2 at;          // its middle (home units)
} fude_zoom_coupled_button;

// A stepper, an encoder or a pot on a shaft: its own reading as it began (a stepper's rotor's angle, an encoder's
// detents, a pot's wiper), the shaft's angle as last read, and how far the shaft has turned since it began (radians,
// counter-clockwise, every whole turn counted).
typedef struct {
    u32 part;
    u32 shaft;
    f64 from;
    f64 was;
    f64 turned;
} fude_zoom_coupled_shaft;

// A circuit's solenoid whose plunger is in the mechanism.
typedef struct {
    u32 part;                 // the circuit's solenoid
    u32 plunger;              // the world's plunger (mechrun.h)
} fude_zoom_coupled_solenoid;

// What of a circuit and a mechanism works together.
typedef struct {
    rde_arr TYPE(fude_zoom_coupled_motor)  motors;
    rde_arr TYPE(fude_zoom_coupled_servo)  servos;
    rde_arr TYPE(fude_zoom_coupled_button) buttons;
    rde_arr TYPE(fude_zoom_coupled_button) slots;      // slotted sensors (`at` is the middle of the beam)
    rde_arr TYPE(fude_zoom_coupled_shaft)  steppers;
    rde_arr TYPE(fude_zoom_coupled_shaft)  encoders;
    rde_arr TYPE(fude_zoom_coupled_shaft)  pots;
    rde_arr TYPE(fude_zoom_coupled_solenoid) solenoids;
    f64     left;             // the time not stepped yet, less than a step (seconds)
} fude_zoom_coupling;

// Sets up an empty coupling, and frees what one holds.
void fude_zoom_coupling_init(fude_zoom_coupling* _k);
void fude_zoom_coupling_destroy(fude_zoom_coupling* _k);
// Couples nothing, and sets circuit _c's motors, servos, steppers, encoders, buttons and solenoids back to working by
// themselves (_c may be NULL).
void fude_zoom_coupling_clear(fude_zoom_coupling* _k, fude_zoom_circuit* _c);
// Finds what of circuit _c works with world _w (scene _s says where the buttons are): its motors, servos, steppers,
// encoders and pots with something on their shafts, its solenoids' plungers, and its push buttons and slotted sensors.
// It is cleared first. Returns how many it found; 0 when the world is not running.
u32  fude_zoom_coupling_build(fude_zoom_coupling* _k, fude_zoom_circuit* _c, const fude_zoom_mech_world* _w, const fude_zoom_scene* _s);
// Runs both on by _dt seconds (at most a tenth of a second a call), one mechanism step at a time; in each, the circuit
// takes at most 200 of its own steps, and runs in slow motion when it needs more. Returns false when the circuit could
// not be solved (it stops there); true when the world is not running.
b8   fude_zoom_coupling_step(fude_zoom_coupling* _k, fude_zoom_circuit* _c, fude_zoom_mech_world* _w, f64 _dt);
// Returns whether the mechanism works circuit part _part (its index in the circuit's parts): an encoder or a pot whose
// shaft it turns, or a slotted sensor whose beam it blocks. Such a part is not the hand's to tap (what is on its shaft
// is, a crank for instance).
b8   fude_zoom_coupling_works(const fude_zoom_coupling* _k, u32 _part);

#endif
