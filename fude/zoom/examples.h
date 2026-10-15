// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_EXAMPLES_H
#define FUDE_ZOOM_EXAMPLES_H

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/scene.h"

// ===========================================================================
// Examples (Sketching's Insert → Examples): canvases to learn from, which also try the simulations hard. Circuits go
// from a torch to a chaser of chips, mechanisms from a crank to a machine of many, and some have both working
// together; each is a canvas of its own, in order from the simplest. They are drawn as a person would draw them (the
// library's parts at their sizes, wired pin to pin, a title over each), so what they do is what Play does with
// anything drawn.
//
// Where it sits: zoom, the canvas apps' layer, over placer.h, which draws them. The page (page_custom.c) builds one
// into a new canvas when it is chosen; the tests (tests/zoom) build and run every one of them as Play would.
// Threads: main thread only (it changes the scene).
// Units: each is drawn round the view's middle in screen points at ×1 (Y up), into any frame of any scene.
// Design: see docs/infinite_canvas_design.md §41, §50 and §58.
// ===========================================================================

// The examples, in the order the list shows them: the electronics first, then the mechanisms, then both together.
typedef enum {
    FUDE_ZOOM_EXAMPLE_TORCH = 0,       // A battery, a switch, a resistor and an LED.
    FUDE_ZOOM_EXAMPLE_GATES,           // Two inputs into every gate, with a probe on each (A 1, B 0).
    FUDE_ZOOM_EXAMPLE_FLASHER,         // A 555 flashing an LED.
    FUDE_ZOOM_EXAMPLE_FULL_ADDER,      // A full adder made of gates (1 + 1 + 0).
    FUDE_ZOOM_EXAMPLE_ADDER_4,         // Four full adders of gates with ripple carry (5 + 3).
    FUDE_ZOOM_EXAMPLE_COUNTER,         // A 74HC161 on a breadboard counting a clock into four LEDs.
    FUDE_ZOOM_EXAMPLE_ADDER_CHIP,      // A 74HC283 adding two nibbles set with switches (6 + 7).
    FUDE_ZOOM_EXAMPLE_CHASER,          // A clock and a 74HC161 into a 74HC138: eight LEDs lit in turn.
    FUDE_ZOOM_EXAMPLE_LED_RESISTOR,    // An LED behind 470 Ω lit; one straight on 9 V burnt out (limits.h).
    FUDE_ZOOM_EXAMPLE_RESISTOR_WATTS,  // 47 Ω on 5 V: a ¼ W resistor burnt out, a 1 W one warm.
    FUDE_ZOOM_EXAMPLE_SHORT_FUSE,      // A lamp a switch shorts: a fuse blows; with no fuse, the battery says so.
    FUDE_ZOOM_EXAMPLE_CAP_POLARITY,    // Electrolytics on 12 V: a 16 V one fine; one reversed and a 6.3 V one popped.
    FUDE_ZOOM_EXAMPLE_TRANSISTOR_SIZE, // A 12 V 5 W lamp: a BC547 switching it burns out, a TIP120 copes.
    FUDE_ZOOM_EXAMPLE_RINGING,         // 10 H and 1000 µF ringing at 1.6 Hz on a voltmeter.
    FUDE_ZOOM_EXAMPLE_DIGIT_COUNTER,   // A 74HC161 counting into a CD4511 lighting a 7-segment digit (10–15 blank).
    // A 74HC161 and a 74HC138 scanning an LED matrix row by row, each row showing its own number.
    FUDE_ZOOM_EXAMPLE_MATRIX_SCAN,     // An LED matrix, scanned.
    FUDE_ZOOM_EXAMPLE_BAR_METER,       // An LM3914 and a bar graph: a pot's voltage shown as a bar.
    FUDE_ZOOM_EXAMPLE_PANEL_METERS,    // A panel voltmeter on a pot's wiper; a panel ammeter in series with 100 Ω.
    FUDE_ZOOM_EXAMPLE_LCD_BY_HAND,     // A character LCD written by hand: logic inputs on its pins, a button on E.
    FUDE_ZOOM_EXAMPLE_SCOPE_RC,        // A 100 Hz square wave into 10 kΩ and 100 nF, both on an oscilloscope.
    // A night light (an LDR under 47 kΩ into a transistor) and a thermistor's thermometer.
    FUDE_ZOOM_EXAMPLE_LIGHT_TEMP,      // Light and temperature sensors.
    FUDE_ZOOM_EXAMPLE_DECADE,          // A 4 Hz clock into a CD4017: ten LEDs lit one at a time.
    FUDE_ZOOM_EXAMPLE_FLIPFLOPS,       // An SR latch on two logic inputs; a JK flip-flop toggling on a clock.
    // An LM393 night light: an LDR's divider against a pot, and an LED on its open collector.
    FUDE_ZOOM_EXAMPLE_COMPARATOR,      // A comparator.
    // An SCR latching a lamp on (with a switch to stop it); a TRIAC switching a lamp on AC.
    FUDE_ZOOM_EXAMPLE_THYRISTORS,      // Thyristors.
    FUDE_ZOOM_EXAMPLE_POWER_SUPPLY,    // 24 V AC, a 2:1 transformer, a bridge, 1000 µF and a 7805: an LED on 5 V.
    // An optocoupler between two circuits; a photodiode and a phototransistor on meters.
    FUDE_ZOOM_EXAMPLE_LIGHT_LINK,      // Light between circuits.
    FUDE_ZOOM_EXAMPLE_SPEAKER_TONE,    // A 555 at about 460 Hz into a speaker, heard as it plays.
    // A crossroads' traffic lights: a 555, a CD4017, a ROM of 39 diodes, a digit and a beeper.
    FUDE_ZOOM_EXAMPLE_TRAFFIC,         // Traffic lights.
    // A 13-key piano: a 555 that each key tunes, a volume pot, a speaker and an oscilloscope.
    FUDE_ZOOM_EXAMPLE_PIANO,           // A piano.
    FUDE_ZOOM_EXAMPLE_CRANK,           // A crank and rocker driven by a motor.
    FUDE_ZOOM_EXAMPLE_GEAR_TRAIN,      // Three gears driven by a motor.
    FUDE_ZOOM_EXAMPLE_PENDULUM,        // A pendulum, and a weight on a spring.
    FUDE_ZOOM_EXAMPLE_PULLEYS,         // A pulley, a weight and a crate; a rope pendulum.
    FUDE_ZOOM_EXAMPLE_PISTON,          // A slider-crank.
    FUDE_ZOOM_EXAMPLE_RACK,            // A rack and its pinion.
    FUDE_ZOOM_EXAMPLE_GEARS_RACK,      // A gear train turning a rack above it.
    FUDE_ZOOM_EXAMPLE_BODIES,          // Drawings made into bodies: a ramp, a ball, a triangle, an L, a pendulum.
    FUDE_ZOOM_EXAMPLE_GEARBOX,         // Nine gears of four sizes and two racks.
    FUDE_ZOOM_EXAMPLE_MACHINE,         // Six mechanisms on one canvas.
    // Blocks of ice, wood, rubber and steel sliding down ramps; balls of rubber, glass and foam bouncing.
    FUDE_ZOOM_EXAMPLE_MATERIALS,       // Materials.
    // Weights on links and ropes: 500 kg shears a pin, 300 kg snaps a rope (mechrun.h).
    FUDE_ZOOM_EXAMPLE_TOO_HEAVY,       // Too heavy.
    FUDE_ZOOM_EXAMPLE_MOTOR_TORQUE,    // 10 kg on an arm: a 2 N·m motor stalls, a 30 N·m one turns it round.
    // The crank and rocker with tracers: its crank's circle, and its coupler's curve.
    FUDE_ZOOM_EXAMPLE_COUPLER_CURVE,   // A coupler curve.
    FUDE_ZOOM_EXAMPLE_BY_HAND,         // A crank and rocker on a hand crank; a pendulum to take hold of.
    // A chain from a small sprocket to one twice as big (turning half as fast); a crossed belt (turning backwards).
    FUDE_ZOOM_EXAMPLE_BELTS,           // Belts and chains.
    FUDE_ZOOM_EXAMPLE_CAM,             // An eccentric cam on a motor, a follower riding up and down on it.
    // Two wheels holding weights on ropes: the one with a pawl holds, the one without runs back.
    FUDE_ZOOM_EXAMPLE_RATCHET,         // A ratchet and pawl.
    FUDE_ZOOM_EXAMPLE_DAMPER,          // Two weights on springs: the one with a damper beside it settles at once.
    // A worm turning a 30-tooth gear one tooth a turn, lowering a weight it never lets go.
    FUDE_ZOOM_EXAMPLE_WORM,            // A worm drive.
    // Theo Jansen's walking legs: two of his eleven-bar legs on one crank, their feet traced.
    FUDE_ZOOM_EXAMPLE_STRANDBEEST,     // A Strandbeest's legs.
    // A four-stroke engine: piston, rod and crank, and a timing belt at half speed to two camshafts and the valves.
    FUDE_ZOOM_EXAMPLE_ENGINE,          // An engine.
    FUDE_ZOOM_EXAMPLE_MOTOR_GEARS,     // A battery, a switch, a meter and a motor turning a gear train (coupling.h).
    // A drive motor turning a dynamo through gears: the dynamo lights an LED, and a meter reads it.
    FUDE_ZOOM_EXAMPLE_DYNAMO,          // A dynamo.
    // Two logic inputs into an L293D: its motor's pinion moves a rack one way or back.
    FUDE_ZOOM_EXAMPLE_FORWARD_BACK,    // A motor driven forwards and back.
    FUDE_ZOOM_EXAMPLE_TURN_COUNTER,    // A motor's arm presses a button each turn; a 74HC161 counts them on LEDs.
    // A rack pressing a button at each end: a latch of NOR gates turns its L293D's motor back.
    FUDE_ZOOM_EXAMPLE_SHUTTLE,         // A shuttle.
    FUDE_ZOOM_EXAMPLE_SERVO_TESTER,    // A 555's short pulses (a pot sets their length) turning a servo's arm.
    // Pulses of 1, 1.5 and 2 ms: arms at 45°, 90° and 135°; a servo on 12 V burnt out.
    FUDE_ZOOM_EXAMPLE_SERVO_ANGLES,    // Servo angles.
    // A button on a 12 V solenoid pulling a latch's bolt, with a diode across it; one with no diode sparks.
    FUDE_ZOOM_EXAMPLE_SOLENOID,        // A solenoid.
    // A clock into two D flip-flops (a ring of four) and a ULN2003 stepping a 28BYJ-48's arm round.
    FUDE_ZOOM_EXAMPLE_STEPPER,         // A stepper motor.
    // Cranks on an encoder (its LEDs lit in turn) and on a pot (read by a meter); an arm through a slotted sensor.
    FUDE_ZOOM_EXAMPLE_SHAFT_SENSORS,   // Sensors on shafts.
    FUDE_ZOOM_EXAMPLE_WORKBENCH,       // A counter and gates beside gears and pulleys.
    // The chaser, the 4-bit adder, the gearbox, the machine, the shuttle and the counter, all on one canvas.
    FUDE_ZOOM_EXAMPLE_EVERYTHING,      // Everything at once.
    // A solenoid engine: a slotted sensor on its crank fires its coil; a counter, an oscilloscope and a meter on it.
    FUDE_ZOOM_EXAMPLE_SOLENOID_ENGINE, // A solenoid engine.
    FUDE_ZOOM_EXAMPLE_COUNT
} FUDE_ZOOM_EXAMPLE_;

// The groups the examples are in: Insert → Examples' first level.
typedef enum {
    FUDE_ZOOM_EXAMPLES_ELECTRONICS = 0,
    FUDE_ZOOM_EXAMPLES_MECHANISMS,
    FUDE_ZOOM_EXAMPLES_BOTH
} FUDE_ZOOM_EXAMPLES_;

// Returns the group example _example is in (FUDE_ZOOM_EXAMPLES_). Play runs mechanisms in the MECHANISMS and BOTH
// groups' examples.
u8 fude_zoom_example_group(u32 _example);
// The examples' folders, under their group (Insert → Examples: Examples / Electronics / Logic and counting / ...):
// what each one's examples are about, in the order they are listed.
typedef enum {
    FUDE_ZOOM_EXAMPLE_SET_FIRST = 0,      // Electronics: first circuits.
    FUDE_ZOOM_EXAMPLE_SET_LOGIC,          // Logic and counting.
    FUDE_ZOOM_EXAMPLE_SET_DISPLAYS,       // Displays.
    FUDE_ZOOM_EXAMPLE_SET_SENSING,        // Sensors and instruments.
    FUDE_ZOOM_EXAMPLE_SET_POWER,          // Power and switching.
    FUDE_ZOOM_EXAMPLE_SET_E_WRONG,        // What goes wrong.
    FUDE_ZOOM_EXAMPLE_SET_E_BIG,          // Big projects.
    FUDE_ZOOM_EXAMPLE_SET_LINKAGES,       // Mechanisms: linkages.
    FUDE_ZOOM_EXAMPLE_SET_GEARS,          // Gears and drives.
    FUDE_ZOOM_EXAMPLE_SET_MOTION,         // Motion and forces.
    FUDE_ZOOM_EXAMPLE_SET_M_WRONG,        // What goes wrong.
    FUDE_ZOOM_EXAMPLE_SET_M_BIG,          // Big projects.
    FUDE_ZOOM_EXAMPLE_SET_MOTORS,         // Both together: motors.
    FUDE_ZOOM_EXAMPLE_SET_ACTUATORS,      // Servos, steppers and solenoids.
    FUDE_ZOOM_EXAMPLE_SET_MACHINE_SENSORS,// Sensors on machines.
    FUDE_ZOOM_EXAMPLE_SET_B_BIG,          // Big projects.
    FUDE_ZOOM_EXAMPLE_SETS
} FUDE_ZOOM_EXAMPLE_SET_;

// fude_zoom_example_set returns the folder example _example goes in (FUDE_ZOOM_EXAMPLE_SET_), and
// fude_zoom_example_set_group the group a folder is under (FUDE_ZOOM_EXAMPLES_).
u8 fude_zoom_example_set(u32 _example);
u8 fude_zoom_example_set_group(u8 _set);
// Returns whether example _example is made to show what goes wrong (a part burnt out, a short, a pin sheared, a motor
// stalled), so that its parts' events are what it is for.
b8 fude_zoom_example_goes_wrong(u32 _example);

// Words an example writes beside what it draws. They are asked of the title callback (fude_zoom_example_title) with
// one of these numbers instead of an example's: a material's name (FUDE_ZOOM_EXAMPLE_WORD_MATERIAL plus its index in
// sim/body.h's materials), or one of the words below.
#define FUDE_ZOOM_EXAMPLE_WORD_MATERIAL 1000u
typedef enum {
    FUDE_ZOOM_EXAMPLE_WORD_TAP_SHORT = 1100u,   // "Tap the switch: a short"
    FUDE_ZOOM_EXAMPLE_WORD_NO_FUSE,             // "No fuse"
    FUDE_ZOOM_EXAMPLE_WORD_BACKWARDS,           // "backwards"
    FUDE_ZOOM_EXAMPLE_WORD_RESTART,             // "Restart: new parts"
    FUDE_ZOOM_EXAMPLE_WORD_LCD_STEPS,           // "Tap E to send D7–D0 as set: …"
    FUDE_ZOOM_EXAMPLE_WORD_TURN_CRANK,          // "Turn the crank: drag round it"
    FUDE_ZOOM_EXAMPLE_WORD_DRAG_WEIGHT,         // "Drag the weight"
    FUDE_ZOOM_EXAMPLE_WORD_SLIDERS,             // "Change the light and the temperature with Play's sliders, …"
    FUDE_ZOOM_EXAMPLE_WORD_NO_DIODE,            // "No diode: let go of its button"
    FUDE_ZOOM_EXAMPLE_WORD_RATCHET_DRAG,        // "Drag the wheels round: the one with a pawl turns one way only"
    FUDE_ZOOM_EXAMPLE_WORD_FLIPFLOPS,           // "Tap S and R …"
    FUDE_ZOOM_EXAMPLE_WORD_COMPARATOR,          // "Turn the light down with its slider …"
    FUDE_ZOOM_EXAMPLE_WORD_THYRISTORS,          // "Tap G to fire the SCR …"
    FUDE_ZOOM_EXAMPLE_WORD_LIGHT,               // "Change the light with the sliders …"
    FUDE_ZOOM_EXAMPLE_WORD_SCOPE,               // "Tap the knobs while it plays …"
    FUDE_ZOOM_EXAMPLE_WORD_TRAFFIC,             // "Each output of the CD4017 lights, through its diodes, …"
    FUDE_ZOOM_EXAMPLE_WORD_PIANO,               // "Press the keys while it plays …"
    FUDE_ZOOM_EXAMPLE_WORD_STRANDBEEST,         // "Theo Jansen's leg …"
    FUDE_ZOOM_EXAMPLE_WORD_ENGINE,              // "A starter motor turns the crank …"
    FUDE_ZOOM_EXAMPLE_WORD_SOLENOID_ENGINE      // "It starts by itself …"
} FUDE_ZOOM_EXAMPLE_WORD_;

// The caller's words: returns example _example's title (its name in the app's language), or the word _example names
// (FUDE_ZOOM_EXAMPLE_WORD_). NULL, or "", writes nothing. The text is only read while the example is built.
typedef const c8* (*fude_zoom_example_title)(u32 _example);

// Builds example _example into frame _frame of _s. _to_frame takes its points (screen points round the view's middle,
// Y up, at ×1) into the frame's units. It is drawn in _ink, its title (and the titles of what it is made of) and its
// words asked of _title (NULL to write no words at all). What it makes is appended to _born (object indexes, in order).
// Returns how many objects it made (0 for an unknown example, or a NULL scene or _born).
u32 fude_zoom_example_build(fude_zoom_scene* _s, u32 _frame, fude_zoom_sim _to_frame, rde_color _ink, u32 _example, fude_zoom_example_title _title,
                            rde_arr* _born);

#endif
