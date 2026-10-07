// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_EXAMPLES_H
#define FUDE_ZOOM_EXAMPLES_H

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/scene.h"

// ===========================================================================
// EXAMPLES (Sketching's Insert → Examples): canvases to learn from, and to
// try the simulations hard — circuits from a torch to a chaser of chips,
// mechanisms from a crank to a machine of many, and both together at once,
// each a canvas of its own in order from the simplest. Drawn as a person
// would draw them (the library's parts at their sizes, wired pin to pin, a
// title over each), so what they do is what Play does with anything drawn.
//
// Each is drawn round the view's middle in screen points at ×1 (Y up), into
// any frame of any scene: the tests run every one of them as Play would.
// ===========================================================================

typedef enum {
    FUDE_ZOOM_EXAMPLE_TORCH = 0,       // a battery, a switch, a resistor, an LED
    FUDE_ZOOM_EXAMPLE_GATES,           // two inputs into every gate, a probe on each (A 1, B 0)
    FUDE_ZOOM_EXAMPLE_FLASHER,         // a 555 flashing an LED
    FUDE_ZOOM_EXAMPLE_FULL_ADDER,      // a full adder of gates (1 + 1 + 0)
    FUDE_ZOOM_EXAMPLE_ADDER_4,         // four full adders of gates, ripple carry (5 + 3)
    FUDE_ZOOM_EXAMPLE_COUNTER,         // a 74HC161 on a breadboard counting a clock into four LEDs
    FUDE_ZOOM_EXAMPLE_ADDER_CHIP,      // a 74HC283 adding two switched nibbles (6 + 7)
    FUDE_ZOOM_EXAMPLE_CHASER,          // a clock, a 74HC161 into a 74HC138: eight LEDs in turn
    FUDE_ZOOM_EXAMPLE_CRANK,           // a crank and rocker on a motor
    FUDE_ZOOM_EXAMPLE_GEAR_TRAIN,      // three gears on a motor
    FUDE_ZOOM_EXAMPLE_PENDULUM,        // a pendulum, a weight on a spring
    FUDE_ZOOM_EXAMPLE_PULLEYS,         // a pulley, a weight and a crate; a rope pendulum
    FUDE_ZOOM_EXAMPLE_PISTON,          // a slider-crank
    FUDE_ZOOM_EXAMPLE_RACK,            // a rack and its pinion
    FUDE_ZOOM_EXAMPLE_GEARS_RACK,      // a gear train turning a rack over it
    FUDE_ZOOM_EXAMPLE_BODIES,          // drawings made bodies: a ramp, a ball, a triangle, an L, a pendulum
    FUDE_ZOOM_EXAMPLE_GEARBOX,         // nine gears of four sizes, two racks
    FUDE_ZOOM_EXAMPLE_MACHINE,         // six mechanisms on one canvas
    FUDE_ZOOM_EXAMPLE_MOTOR_GEARS,     // a battery, a switch, a meter, a motor turning a gear train (coupling.h)
    FUDE_ZOOM_EXAMPLE_DYNAMO,          // a drive motor turning a dynamo through gears: it lights an LED, a meter reads it
    FUDE_ZOOM_EXAMPLE_FORWARD_BACK,    // two logic inputs into an L293D: its motor's pinion moves a rack one way or back
    FUDE_ZOOM_EXAMPLE_TURN_COUNTER,    // a motor's arm presses a button each turn: a 74HC161 counts them on LEDs
    FUDE_ZOOM_EXAMPLE_SHUTTLE,         // a rack pressing a button at each end: a latch of NORs turns its L293D's motor back
    FUDE_ZOOM_EXAMPLE_WORKBENCH,       // a counter and gates beside gears and pulleys
    FUDE_ZOOM_EXAMPLE_EVERYTHING,      // the chaser, the 4-bit adder, the gearbox, the machine, the shuttle, the counter
    FUDE_ZOOM_EXAMPLE_COUNT
} FUDE_ZOOM_EXAMPLE_;

typedef enum {
    FUDE_ZOOM_EXAMPLES_ELECTRONICS = 0,
    FUDE_ZOOM_EXAMPLES_MECHANISMS,
    FUDE_ZOOM_EXAMPLES_BOTH
} FUDE_ZOOM_EXAMPLES_;

// Which of them example _example is (FUDE_ZOOM_EXAMPLES_; the mechanisms' and both: Play's mechanisms in them).
u8 fude_zoom_example_group(u32 _example);

// An example's title (its name in the person's language: the app's); NULL: none written.
typedef const c8* (*fude_zoom_example_title)(u32 _example);

// Example _example drawn into frame _frame of _s: _to_frame takes its points (screen points round the view's middle,
// Y up, ×1) into the frame's units; in _ink, its title (and those of what it is made of) _title's. What it made
// appended to _born (objects, in order). How many.
u32 fude_zoom_example_build(fude_zoom_scene* _s, u32 _frame, fude_zoom_sim _to_frame, rde_color _ink, u32 _example, fude_zoom_example_title _title,
                            rde_arr* _born);

#endif
