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
    FUDE_ZOOM_EXAMPLE_LED_RESISTOR,    // an LED behind 470 Ω lit, one straight on 9 V burnt (limits.h)
    FUDE_ZOOM_EXAMPLE_RESISTOR_WATTS,  // 47 Ω on 5 V: a ¼ W resistor burnt, a 1 W one warm
    FUDE_ZOOM_EXAMPLE_SHORT_FUSE,      // a lamp a switch shorts: behind a fuse it blows; with none, the battery says so
    FUDE_ZOOM_EXAMPLE_CAP_POLARITY,    // electrolytics on 12 V: a 16 V one well, one backwards popped, a 6.3 V one popped
    FUDE_ZOOM_EXAMPLE_TRANSISTOR_SIZE, // a 12 V 5 W lamp: a BC547 switching it burnt, a TIP120 well
    FUDE_ZOOM_EXAMPLE_RINGING,         // 10 H and 1000 µF ringing at 1.6 Hz on a voltmeter
    FUDE_ZOOM_EXAMPLE_DIGIT_COUNTER,   // a 74HC161 counting into a CD4511 lighting a 7-segment digit (10–15 blank)
    FUDE_ZOOM_EXAMPLE_MATRIX_SCAN,     // a 74HC161 and a 74HC138 scanning an LED matrix row by row: each row its number
    FUDE_ZOOM_EXAMPLE_BAR_METER,       // an LM3914 and a bar graph: a pot's voltage as a bar
    FUDE_ZOOM_EXAMPLE_PANEL_METERS,    // a panel voltmeter on a pot's wiper, a panel ammeter in a row with 100 Ω
    FUDE_ZOOM_EXAMPLE_LCD_BY_HAND,     // a character LCD written by hand: logic inputs on its pins, a button on E
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
    FUDE_ZOOM_EXAMPLE_MATERIALS,       // blocks of ice, wood, rubber, steel down ramps; balls of rubber, glass, foam bouncing
    FUDE_ZOOM_EXAMPLE_TOO_HEAVY,       // weights on links and ropes: 500 kg shears a pin, 300 kg snaps a rope (mechrun.h)
    FUDE_ZOOM_EXAMPLE_MOTOR_TORQUE,    // 10 kg on an arm: a 2 N·m motor jammed, a 30 N·m one turning it round
    FUDE_ZOOM_EXAMPLE_MOTOR_GEARS,     // a battery, a switch, a meter, a motor turning a gear train (coupling.h)
    FUDE_ZOOM_EXAMPLE_DYNAMO,          // a drive motor turning a dynamo through gears: it lights an LED, a meter reads it
    FUDE_ZOOM_EXAMPLE_FORWARD_BACK,    // two logic inputs into an L293D: its motor's pinion moves a rack one way or back
    FUDE_ZOOM_EXAMPLE_TURN_COUNTER,    // a motor's arm presses a button each turn: a 74HC161 counts them on LEDs
    FUDE_ZOOM_EXAMPLE_SHUTTLE,         // a rack pressing a button at each end: a latch of NORs turns its L293D's motor back
    FUDE_ZOOM_EXAMPLE_SERVO_TESTER,    // a 555's short pulses (a pot their length) turning a servo's arm
    FUDE_ZOOM_EXAMPLE_SERVO_ANGLES,    // 1, 1.5, 2 ms pulses: arms at 45°, 90°, 135°; a servo on 12 V burnt
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
// Is example _example made to show what goes wrong (a part burnt, a short, a pin sheared, a motor jammed): its parts'
// events what it is for?
b8 fude_zoom_example_goes_wrong(u32 _example);

// Words an example writes by what it draws, asked of its title's callback instead of an example: a material's name
// (FUDE_ZOOM_EXAMPLE_WORD_MATERIAL + its index, sim/body.h's), and these.
#define FUDE_ZOOM_EXAMPLE_WORD_MATERIAL 1000u
typedef enum {
    FUDE_ZOOM_EXAMPLE_WORD_TAP_SHORT = 1100u,   // "Tap the switch: a short"
    FUDE_ZOOM_EXAMPLE_WORD_NO_FUSE,             // "No fuse"
    FUDE_ZOOM_EXAMPLE_WORD_BACKWARDS,           // "backwards"
    FUDE_ZOOM_EXAMPLE_WORD_RESTART,             // "Restart: new parts"
    FUDE_ZOOM_EXAMPLE_WORD_LCD_STEPS            // "Tap E to send D7–D0 as set: …"
} FUDE_ZOOM_EXAMPLE_WORD_;

// An example's title (its name in the person's language: the app's), or one of the words above; NULL: none written.
typedef const c8* (*fude_zoom_example_title)(u32 _example);

// Example _example drawn into frame _frame of _s: _to_frame takes its points (screen points round the view's middle,
// Y up, ×1) into the frame's units; in _ink, its title (and those of what it is made of) _title's. What it made
// appended to _born (objects, in order). How many.
u32 fude_zoom_example_build(fude_zoom_scene* _s, u32 _frame, fude_zoom_sim _to_frame, rde_color _ink, u32 _example, fude_zoom_example_title _title,
                            rde_arr* _born);

#endif
