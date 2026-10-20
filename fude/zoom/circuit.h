// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_CIRCUIT_H
#define FUDE_ZOOM_CIRCUIT_H

#include "rde.h"
#include "sim/sparse.h"
#include "zoom/zoom.h"
#include "zoom/scene.h"

struct fude_zoom_logic;

// ===========================================================================
// Circuits: the Electronics topic, circuits drawn as schematics and simulated as they are drawn.
//
// PARTS are the diagram library's symbols (symbol.h's Electronics, Logic, Chips and modules, and Boards families).
// Each has its PINS, where a wire meets it (in its own u, v from -1 to 1, as it is drawn), and what it does in the
// simulation (its MODEL). A part's text is its value, read as it is typed: "10k", "4.7 kΩ", "100uF", "9V",
// "5V 50Hz", "1Hz", "red", "on". A board's or a chip's lines after its name say how its pins are driven ("D13 blink",
// "D9 high").
//
// WIRES (shape.h's WIRE) are lines whose ends keep the pins they were drawn to (a part's id and its pin), and follow
// the part when it moves; where a wire's end lies on another wire, the two are joined there.
//
// THE CIRCUIT is worked out from what is on the canvas: the nodes (pins and wire ends where they meet, and wires and
// what they join), and each part's elements on them. It is SIMULATED by nodal analysis: every source is stamped as its
// Norton equivalent (a voltage behind a small resistance: a DC source's 1 mΩ, a battery's 0.5 Ω, a logic output's
// 25 Ω), so the unknowns are the nodes' voltages alone, solved by Newton's method for what is not linear (diodes,
// LEDs, transistors, op-amps), in steps of time. Capacitors and inductors are stepped by the trapezoidal rule, and by
// backward Euler on the step after a jump; logic, timers and boards are stepped with it. What it finds is drawn over
// the schematic (page_circuit.c): each wire's voltage, current flowing along it as dots, LEDs and lamps lit, meters'
// readings, logic levels.
//
// Where it sits: `fude/zoom`, the canvas layer, on `fude/sim` (sim/sparse.h solves its blocks; logic.h runs its logic
// on sim's digital engine). The page builds and steps it in Play (page_mech.c) and draws what it finds
// (page_circuit.c); symbol.c draws the parts; display.h, logic.h and coupling.h work with its parts.
// Main types and lifecycle: the catalogue of parts (fude_zoom_part) is static, and the parts made from texts (custom
// parts, chips, sized displays) are kept for as long as the app runs. A fude_zoom_circuit is the caller's struct (the
// page keeps one), set up by fude_zoom_circuit_init and released by fude_zoom_circuit_destroy; it owns its logic.
// Threads: main thread only. Finding a part by a symbol's kind fills the topics' one table (symbol.h:
// fude_zoom_symbol_topic_part), and the page takes the circuit's sound from the main thread to hand it to the audio
// stream.
//
// Design: docs/infinite_canvas_design.md §27 (the first simulation), §46 (circuits of any size solved sparse in
// independent blocks), §47 (parts' limits), §48 (steps as fine as what happens), §56 (the parts added in 0.1.75) and
// §59 (rails held, and blocks left quiet, so a big circuit runs as fast as its fast corner).
// ===========================================================================

// The side of its part a pin's wire leaves by.
typedef enum {
    FUDE_ZOOM_PIN_LEFT = 0,
    FUDE_ZOOM_PIN_RIGHT,
    FUDE_ZOOM_PIN_UP,
    FUDE_ZOOM_PIN_DOWN,
    FUDE_ZOOM_PIN_ANY     // a hole in the middle of it (a breadboard's): a wire comes into it straight, any way
} FUDE_ZOOM_PIN_SIDE_;

// A pin: where it is on its part (u, v from -1 to 1 of its half sizes), its name ("" for a plain lead), and the side
// its wire leaves by (a FUDE_ZOOM_PIN_SIDE_).
typedef struct {
    f32       u, v;
    const c8* name;
    u8        side;
} fude_zoom_pin;

// What a part does in the simulation (the models: circuit_stamp.c and circuit_step.c).
typedef enum {
    FUDE_ZOOM_MODEL_NONE = 0,   // drawn only: its pins are joined to nothing inside (a module, a microcontroller)
    FUDE_ZOOM_MODEL_RESISTOR,
    FUDE_ZOOM_MODEL_POT,
    FUDE_ZOOM_MODEL_CAPACITOR,
    FUDE_ZOOM_MODEL_INDUCTOR,
    FUDE_ZOOM_MODEL_DIODE,
    FUDE_ZOOM_MODEL_LED,
    FUDE_ZOOM_MODEL_ZENER,
    FUDE_ZOOM_MODEL_NPN,
    FUDE_ZOOM_MODEL_PNP,
    FUDE_ZOOM_MODEL_NMOS,
    FUDE_ZOOM_MODEL_PMOS,
    FUDE_ZOOM_MODEL_VSOURCE,
    FUDE_ZOOM_MODEL_BATTERY,
    FUDE_ZOOM_MODEL_ACSOURCE,
    FUDE_ZOOM_MODEL_CLOCK,
    FUDE_ZOOM_MODEL_ISOURCE,
    FUDE_ZOOM_MODEL_GROUND,
    FUDE_ZOOM_MODEL_RAIL,
    FUDE_ZOOM_MODEL_SWITCH,
    FUDE_ZOOM_MODEL_BUTTON,
    FUDE_ZOOM_MODEL_LAMP,
    FUDE_ZOOM_MODEL_MOTOR,
    FUDE_ZOOM_MODEL_BUZZER,
    FUDE_ZOOM_MODEL_FUSE,
    FUDE_ZOOM_MODEL_OPAMP,
    FUDE_ZOOM_MODEL_VOLTMETER,
    FUDE_ZOOM_MODEL_AMMETER,
    FUDE_ZOOM_MODEL_SEVEN_SEG,
    FUDE_ZOOM_MODEL_GATE,        // a logic gate (its kind is the part's `gate`)
    FUDE_ZOOM_MODEL_DFF,
    FUDE_ZOOM_MODEL_TFF,
    FUDE_ZOOM_MODEL_LOGIC_IN,
    FUDE_ZOOM_MODEL_LOGIC_OUT,
    FUDE_ZOOM_MODEL_TIMER555,
    FUDE_ZOOM_MODEL_REGULATOR,   // IN, GND, OUT: its volts and its dropout are the part's a and b
    FUDE_ZOOM_MODEL_SHIFT595,
    FUDE_ZOOM_MODEL_DRIVER293,
    FUDE_ZOOM_MODEL_ULN2003,
    FUDE_ZOOM_MODEL_RELAY,
    FUDE_ZOOM_MODEL_BOARD,       // its power pins are sources, and its pins are driven as its text says
    FUDE_ZOOM_MODEL_SIM,         // a definition of the simulation's (a chip, a custom part): logic.h
    FUDE_ZOOM_MODEL_SERVO,       // GND, VCC, SIG: its horn turns to the angle its signal's pulses say (circuit_step.c)
    // Displays (display.h): a panel of 7-segment digits, an LED matrix, a 10-LED bar graph, an LM3914 (a bar graph's
    // driver), a panel meter, a character LCD (an HD44780's).
    FUDE_ZOOM_MODEL_SEG_PANEL,
    FUDE_ZOOM_MODEL_LED_MATRIX,
    FUDE_ZOOM_MODEL_BAR_GRAPH,
    FUDE_ZOOM_MODEL_LM3914,
    FUDE_ZOOM_MODEL_PANEL_METER,
    FUDE_ZOOM_MODEL_CHAR_LCD,
    // An oscilloscope (display.h): CH1, CH2 and GND, their voltages over time on its screen.
    FUDE_ZOOM_MODEL_SCOPE,       // an oscilloscope (display.h)
    FUDE_ZOOM_MODEL_SPDT,        // a changeover switch: COM to A, or (tapped) to B
    FUDE_ZOOM_MODEL_DPDT,        // two worked together: COM1 to A1 or B1, COM2 to A2 or B2
    FUDE_ZOOM_MODEL_LDR,         // a light-dependent resistor, as bright as its light (state[0]: lux)
    FUDE_ZOOM_MODEL_THERMISTOR,  // an NTC thermistor, as warm as its temperature (state[0]: °C)
    // Worked by, or working, a mechanism (coupling.h): a solenoid (its coil and its plunger), a unipolar stepper motor,
    // a rotary encoder, and a slotted optical sensor (each described below).
    FUDE_ZOOM_MODEL_SOLENOID,
    FUDE_ZOOM_MODEL_STEPPER,
    FUDE_ZOOM_MODEL_ENCODER,
    FUDE_ZOOM_MODEL_SLOT,
    // (Added in 0.1.75.) Flip-flops of the logic's own (J, CLK, K; S, R), an LM393's two open-collector comparators,
    // an LM358's two op-amps, thyristors (an SCR, a TRIAC), a transformer and a bridge rectifier, light (an
    // optocoupler, a photodiode, a phototransistor), and a speaker that sounds (each described below).
    FUDE_ZOOM_MODEL_JKFF,
    FUDE_ZOOM_MODEL_SRLATCH,
    FUDE_ZOOM_MODEL_COMPARATOR,
    FUDE_ZOOM_MODEL_OPAMP2,
    FUDE_ZOOM_MODEL_SCR,
    FUDE_ZOOM_MODEL_TRIAC,
    FUDE_ZOOM_MODEL_TRANSFORMER,
    FUDE_ZOOM_MODEL_BRIDGE,
    FUDE_ZOOM_MODEL_OPTO,
    FUDE_ZOOM_MODEL_PHOTODIODE,
    FUDE_ZOOM_MODEL_PHOTOTRANSISTOR,
    FUDE_ZOOM_MODEL_SPEAKER,
    FUDE_ZOOM_MODEL_COUNT
} FUDE_ZOOM_MODEL_;

// A logic gate's kind.
typedef enum {
    FUDE_ZOOM_GATE_AND = 0,
    FUDE_ZOOM_GATE_OR,
    FUDE_ZOOM_GATE_NOT,
    FUDE_ZOOM_GATE_NAND,
    FUDE_ZOOM_GATE_NOR,
    FUDE_ZOOM_GATE_XOR,
    FUDE_ZOOM_GATE_XNOR,
    FUDE_ZOOM_GATE_BUFFER
} FUDE_ZOOM_GATE_;

// A kind of part, as the catalogue has it.
typedef struct {
    const c8*            id;          // its symbol's id (symbol.c's catalogue)
    u8                   model;       // a FUDE_ZOOM_MODEL_
    u8                   look;        // how it is drawn (circuit_draw.c's own)
    u8                   gate;        // a gate's kind; for a 74HC chip, its gates' kind
    const fude_zoom_pin* pins;
    u16                  pin_count;   // (a breadboard's holes are worked out: fude_zoom_part_pin)
    const c8*            value;       // its text when placed
    f32                  a, b;        // the model's own: a regulator's volts and dropout, a board's supply volts...
} fude_zoom_part;

// A BREADBOARD (half size): 30 columns of holes in rows a–e and f–j (each column's five holes joined), and a pair of
// rails along its top and along its bottom (each rail joined along its length). Parts' pins and wires' ends in its
// holes are joined as its strips are.
#define FUDE_ZOOM_BREADBOARD_COLS 30u
#define FUDE_ZOOM_BREADBOARD_ROWS 14u   // the + and − rails, a–e, f–j, the + and − rails
// Returns the strip of a breadboard's pin _i: holes with the same strip are joined.
u32  fude_zoom_breadboard_strip(u32 _i);

// Return the part whose symbol has id _id, or whose symbol is of kind _kind (symbol.h); NULL when it is not a part.
// A chip of the simulation's comes with its definition's pins, and a sized display at the size its kind comes in.
// fude_zoom_part_of_kind is the topics' one kind-to-part map's (symbol.h: fude_zoom_symbol_topic_part), which keeps
// its answers.
const fude_zoom_part* fude_zoom_part_find(const c8* _id);
const fude_zoom_part* fude_zoom_part_of_kind(u32 _kind);
// The catalogue: how many parts it has, and each (NULL past the end). They are static and stay valid for ever.
u32                   fude_zoom_part_count(void);
const fude_zoom_part* fude_zoom_part_at(u32 _i);

// Returns where a part's body starts in from its pins (its u: its pins' names go inside it from there); 0 when its
// pins are not named on it (a resistor's, a breadboard's).
f32  fude_zoom_part_inset(const fude_zoom_part* _part);
// Gives where its pins' names go, as fractions of its half sizes: a left or right pin's (*_u), a top or bottom one's
// (*_v). A sized display's are as big as it is (display.h).
void fude_zoom_part_insets(const fude_zoom_part* _part, f32* _u, f32* _v);
// Words on a part's face, by what they name (an oscilloscope's knobs: CH1, CH2, VOLTS/DIV, POS, TIME/DIV), each with
// its middle at (u, v) of its half sizes.
typedef struct {
    f32       u, v;
    const c8* text;
} fude_zoom_part_label;
// Writes a part's face words into _out, at most _most. Returns how many (0 for a part with none).
u32  fude_zoom_part_labels(const fude_zoom_part* _part, fude_zoom_part_label* _out, u32 _most);
// Returns a part's height as it comes (the catalogue's units): its symbol's (_catalogue_h), or a sized display's for
// its size.
f64  fude_zoom_part_room_h(const fude_zoom_part* _part, f64 _catalogue_h);
// Gives a part's pin _i (a breadboard's holes are worked out). Returns false when there is no such pin.
b8   fude_zoom_part_pin(const fude_zoom_part* _part, u32 _i, fude_zoom_pin* _out);

// Draws a part (symbol.c's symbols call it): its polylines for half sizes _hw × _hh into _points (fude_zoom_v2) and
// _parts (fude_zoom_symbol_part), as symbol.h's parts, the first being its outline. Returns how many parts.
u32  fude_zoom_part_draw(const fude_zoom_part* _part, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points, rde_arr* _parts);

// Gives part _object's pin _pin where it is now, in its symbol's frame's units. Returns false when there is no such
// pin.
b8   fude_zoom_part_pin_at(const fude_zoom_scene* _s, u32 _object, u32 _pin, fude_zoom_v2* _out);
// Returns the parts' grid at symbol _object's size: ten of the catalogue's units (a tenth of a resistor's length) in
// its frame's units, which is how long its wires' stubs are. 0 when the object has no symbol numbers.
f64  fude_zoom_part_unit(const fude_zoom_scene* _s, u32 _object);
// Returns the part an object is (a live symbol whose kind is a part's), or NULL.
const fude_zoom_part* fude_zoom_part_of(const fude_zoom_scene* _s, u32 _object);
// Returns whether a part is a custom part (a definition of a person's own, its pins round a block: logic.h).
b8   fude_zoom_part_custom(const fude_zoom_part* _part);
// Returns the part a symbol's numbers make (its kind, its half sizes, its letters' height, its text): for a custom
// part or a sized display, the one its text names.
const fude_zoom_part* fude_zoom_part_of_numbers(const f64* _n, u32 _count);
// Returns the part a symbol of kind _kind with text _text is: a custom part or a sized display as the text says; any
// other part, itself.
const fude_zoom_part* fude_zoom_part_of_text(u32 _kind, const c8* _text);
// Returns whether a part was made for a symbol from its text (a custom part's or a sized display's, with its own pins).
b8   fude_zoom_part_made(const fude_zoom_part* _part);

// --- wires ----------------------------------------------------------------------------------------

#define FUDE_ZOOM_WIRE_POINTS 32u   // the most points a wire has
// The most numbers a wire has (fude_zoom_wire_numbers): its point count, each point's x and y, and at each end an id
// (two numbers) and a pin. Every buffer a wire's numbers go into or come out of is this size.
#define FUDE_ZOOM_WIRE_NUMBERS (1u + 2u * FUDE_ZOOM_WIRE_POINTS + 2u * (2u + 1u))

// Writes a wire's numbers (shape.h's WIRE) into _n: how many points it has, its points (relative to the first, which
// is the shape's translation), then the part and the pin at each end (each part an object id in two numbers, scene.h's
// fude_zoom_id_put, then a pin; 0 and -1 mean none). At most FUDE_ZOOM_WIRE_POINTS points are kept, so _n needs room
// for FUDE_ZOOM_WIRE_NUMBERS. Returns how many numbers it wrote.
u32  fude_zoom_wire_numbers(f64* _n, const fude_zoom_v2* _points, u32 _count, fude_zoom_id _from, i32 _from_pin, fude_zoom_id _to, i32 _to_pin);
// Reads a wire's numbers back: its points (relative to its translation) and its ends' parts and pins. Any output may be
// NULL. Files saved before 0.1.49 wrote an id as one number, rounded once a device's half of it was large; those are
// read as they were, and their parts are found by their pins (fude_zoom_wire_part). Returns how many points; 0 when the
// numbers are not a wire's.
u32  fude_zoom_wire_of(const f64* _n, u32 _count, fude_zoom_v2* _points, fude_zoom_id* _from, i32* _from_pin, fude_zoom_id* _to, i32* _to_pin);
// Returns the part a wire's end keeps: the object with id _id, whose pin is _pin. For an id from an old file, rounded,
// it is the part with such an id whose pin is nearest that end (_end, in frame _frame's units). FUDE_ZOOM_NONE when
// there is none.
u32  fude_zoom_wire_part(const fude_zoom_scene* _s, fude_zoom_id _id, i32 _pin, u32 _frame, fude_zoom_v2 _end);
// Routes a wire from _a to _b (frame units) squarely, leaving _a by side _side_a and coming into _b by side _side_b
// (FUDE_ZOOM_PIN_SIDE_; 255 for either): straight out of each pin by _stub first, then across and up between them,
// never back over itself. Writes its points into _out (at most 6) and returns how many.
u32  fude_zoom_wire_route(fude_zoom_v2 _a, u8 _side_a, fude_zoom_v2 _b, u8 _side_b, f64 _stub, fude_zoom_v2* _out);
// Returns the side a part's pin faces now, as the part is turned (a FUDE_ZOOM_PIN_SIDE_); 255 when it is not a pin, or
// faces any way (a hole).
u8   fude_zoom_part_side_at(const fude_zoom_scene* _s, u32 _object, u32 _pin);
// Gives part _object's pin _pin in frame _frame's units, wherever the part is (in another frame, deeper or
// shallower), the side it faces there (255: any), and the parts' grid there (for a wire's stub). _side and _stub may be
// NULL. Returns false when there is no such pin.
b8   fude_zoom_part_pin_in(const fude_zoom_scene* _s, u32 _object, u32 _pin, u32 _frame, fude_zoom_v2* _at, u8* _side, f64* _stub);
// Adds a new wire to frame _frame through _points (frame units), its ends kept by parts _from and _to (object indices,
// FUDE_ZOOM_NONE for none) at their pins. Returns its object index.
u32  fude_zoom_wire_add(fude_zoom_scene* _s, u32 _frame, const fude_zoom_v2* _points, u32 _count, u32 _from, i32 _from_pin, u32 _to, i32 _to_pin,
                        rde_color _color, f32 _radius);
// Makes the wires joined to any of _moved (object indices just moved) again, with their ends where their pins are now
// (routed squarely again). A wire that moved with every part it is joined to is left as it is. Each old wire dies and
// is added to _died, and each new one to _born. Returns how many were made again.
u32  fude_zoom_wire_follow(fude_zoom_scene* _s, const u32* _moved, u32 _count, rde_arr* _died, rde_arr* _born);
// Keeps every wire joining two things. Kills the wires that what just died (_died: object indices, TYPE(u32)) leaves
// joined to nothing at an end (an end at a dead part's pin, or on a dead wire and on nothing else), adds them to
// _died, and goes on with those they leave so in turn; the caller makes it one undo step with what died first.
// Returns how many it killed.
u32  fude_zoom_wire_orphans(fude_zoom_scene* _s, rde_arr* _died);
// Makes _wire again with each part id _old[i] it is joined to replaced by _new[i], its ends where those pins are.
// Returns the new wire, or FUDE_ZOOM_NONE when it is joined to none of them. The caller lets the old one go.
u32  fude_zoom_wire_remap(fude_zoom_scene* _s, u32 _wire, const fude_zoom_id* _old, const fude_zoom_id* _new, u32 _n);

// --- values -----------------------------------------------------------------------------------------

// Reads a value as a part's text says it ("4.7k", "4k7", "4R7", "100 uF", "10mH", "5V 50Hz"): the _index-th number in
// it, its metric prefix applied (p n u µ m k M G; a prefix between digits stands for the point). A minus sign right
// before a number makes it negative ("-5V", "−5V"); a hyphen joining it to a word or a number ("4-20mA") does not.
// Returns false when there is no such number.
b8   fude_zoom_circuit_value(const c8* _text, u32 _index, f64* _out);

// A word a part's text may hold, and what it stands for (a number of the caller's own: a colour, a way round).
typedef struct {
    const c8* word;    // as it is written: its ASCII letters match in any case, any other byte exactly
    u8        means;   // what the caller makes of it
} fude_zoom_word;
// Returns what the first of _words (_count of them) that _text holds means, or _none when it holds none. The text is
// read from its start, and at each place the longest word that is there wins. A word of Latin letters (ASCII, or
// accented ones: "horário") is matched whole, never inside a longer one: "on" is not in "contact", "1" is not in
// "SW1", "red" is not in "infrared", "cw" is not in "ccw". A word in another script (Japanese: "赤", "反時計回り") is
// matched wherever it is, as Japanese puts no spaces between words; "反時計回り" wins over the "時計回り" inside it
// because the longest wins where it begins. A NULL text holds no word.
u8   fude_zoom_circuit_word(const c8* _text, const fude_zoom_word* _words, u32 _count, u8 _none);

// LEVELS: the words a switch's, a logic input's or a board pin's text says how it is with, each a whole word in any
// case and in any of the apps' languages (English, Spanish, Portuguese, French, Japanese), as an LED's colour is: on,
// closed, high and 1 ("encendido", "ligado", "fermé", "haut", "オン"...) say HIGH; off, open, low and 0 ("apagado",
// "desligado", "ouvert", "bas", "オフ"...) say LOW; and blink ("parpadeo", "pisca", "clignote", "点滅") says BLINK, which
// only a board's pin does. The first of them a text holds says it (fude_zoom_circuit_word reads it).
#define FUDE_ZOOM_LEVEL_LOW   0u
#define FUDE_ZOOM_LEVEL_HIGH  1u
#define FUDE_ZOOM_LEVEL_BLINK 2u
// Returns the level (a FUDE_ZOOM_LEVEL_) the first level word in _text says, or _none when it holds none.
u8   fude_zoom_circuit_level(const c8* _text, u8 _none);
// Returns whether _word is a word that says HIGH or LOW, whole and nothing more (in any case): a logic input's text
// that says its level, not its name (logic.h's custom parts name their inputs by their texts).
b8   fude_zoom_circuit_level_word(const c8* _word);

// LED COLOURS: the colours an LED's text (a 7-segment panel's, an LED matrix's, a bar graph's too) can name, each with
// its forward voltage at 10 mA and how its light is drawn. Its text names one in any of the apps' languages, as whole
// words in any case: "red", "rojo", "vermelho", "rouge", "赤"; "infrared", "IR", "infrarrojo", "赤外線".
typedef enum {
    FUDE_ZOOM_LED_RED = 0,
    FUDE_ZOOM_LED_ORANGE,
    FUDE_ZOOM_LED_YELLOW,
    FUDE_ZOOM_LED_GREEN,
    FUDE_ZOOM_LED_BLUE,
    FUDE_ZOOM_LED_WHITE,
    FUDE_ZOOM_LED_PINK,
    FUDE_ZOOM_LED_PURPLE,
    FUDE_ZOOM_LED_INFRARED,
    FUDE_ZOOM_LED_COLORS
} FUDE_ZOOM_LED_;

// An LED colour.
typedef struct {
    const c8* name;    // its English name, as a circuit's part keeps it (fude_zoom_circuit_part's color): "red"
    f64       volts;   // an LED of it's forward voltage at 10 mA
    rde_color light;   // its light, as it is drawn lit
} fude_zoom_led_color;

// Returns the colour that _text names first (a FUDE_ZOOM_LED_), or FUDE_ZOOM_LED_RED when it names none: an LED is
// red unless it says otherwise. Its English name is one of the words, so a part's kept name reads back as itself.
u8   fude_zoom_led_color_of(const c8* _text);
// Returns colour _color (a FUDE_ZOOM_LED_; red for any other). The table is static and valid for the program's life.
const fude_zoom_led_color* fude_zoom_led_color_get(u8 _color);
// Return a light-dependent resistor's resistance in _lux of light (a GL5528's: 20 kΩ at 10 lux, changing as the light
// to the power -0.7, between 100 Ω and 1 MΩ), and an NTC thermistor's at _celsius (_r25 at 25 °C, its B constant
// _beta).
f64  fude_zoom_circuit_ldr_ohms(f64 _lux);
f64  fude_zoom_circuit_ntc_ohms(f64 _r25, f64 _beta, f64 _celsius);
// Convert between a sensor's value and where it is along its slider (0–1): a lit sensor's light runs from 1 to
// 100 000 lux on a logarithmic scale; a thermistor's temperature from -20 to 120 °C.
f64  fude_zoom_circuit_sensor_along(u8 _model, f64 _value);
f64  fude_zoom_circuit_sensor_at(u8 _model, f64 _along);

// --- the circuit and its simulation -----------------------------------------------------------------


// LIMITS: what a part takes at most, as its datasheet says (limits.h): the power it can turn into heat (W), the
// current through it (A), the voltage across it (V), and the voltage the wrong way round (V: an LED's, a diode's, an
// electrolytic capacitor's; a MOSFET's gate's, either way). 0 means no limit. A part past one of them heats up and,
// hot enough, burns: it is open from then on, until the circuit starts again.
typedef enum {
    FUDE_ZOOM_LIMIT_POWER = 0,
    FUDE_ZOOM_LIMIT_CURRENT,
    FUDE_ZOOM_LIMIT_VOLTAGE,
    FUDE_ZOOM_LIMIT_REVERSE,
    FUDE_ZOOM_LIMIT_COUNT
} FUDE_ZOOM_LIMIT_;

// A part's limits, by FUDE_ZOOM_LIMIT_.
typedef struct {
    f64 most[FUDE_ZOOM_LIMIT_COUNT];
} fude_zoom_limits;

// A part in the circuit: its object, what it is, its pins' nodes, its values, its state.
//
// Its pins' nodes and currents are the circuit's (pin_nodes, pin_amps: pin_room of them from pin_at), however many pins
// it has (a custom part's are as many as its ports); `node` and `pin_i` point there, and stay valid until the circuit
// is built again. Each part has room for 64 pins at least, so that a model reading a pin by its number past the part's
// own reads one joined to nothing.
typedef struct {
    u32                   object;
    const fude_zoom_part* part;
    u32*                  node;         // each pin's node (0: ground; FUDE_ZOOM_NONE: joined to nothing), as above
    f64                   value[8];     // its numbers as its text says (a resistance, a voltage, a frequency...)
    f64                   state[8];     // what it remembers (a capacitor's voltage, a flip-flop's Q, a 555's...)
    f64*                  pin_i;        // each pin's current, out of the part into its node (amperes), as last solved
    u32                   pin_at;       // its first pin's place in the circuit's pin_nodes and pin_amps
    u32                   pin_room;     // how many places it has there: its part's pins, 64 at least
    f64                   shown;        // what it shows (an LED's brightness 0..1, a meter's reading, a motor's turn)
    // A board's pins: those driven (a bit for each pin), and of those, which are driven high and which blink. A board
    // is the catalogue's, with fewer than 64 pins, so a bit each is enough.
    u64                   drive;        // a board's pins driven (a bit for each pin); how: drive_high, drive_blink
    u64                   drive_high, drive_blink;
    u64                   switch_on;    // a switch or a button closed, a logic input high (bit 0)
    c8                    color[12];    // an LED's colour ("red")
    rde_color             lit;          // a logic probe's colour lit (its LIGHT: props.h; else FUDE_ZOOM_PROBE_LIT)
    // Played with a mechanism (coupling.h): a motor with a part of the mechanism on its shaft (`shafted`: its winding
    // behind its back-EMF, its shaft turning as fast as that part, `spin`, in radians a second, the way its voltage
    // turns it), and a push button that a moving part presses (`pushed`: closed as if held).
    b8                    shafted;
    f64                   spin;
    b8                    pushed;
    // Its pins' block of unknowns (below). FUDE_ZOOM_NONE when its pins are only on ground, on held nodes, on nodes
    // only logic is on or on nothing: then its stamps read only known volts, and it is stamped once a solve.
    u32                   block;        // its pins' block of unknowns (FUDE_ZOOM_NONE: none, as above)
    // Its pins that only sense (a bit for each of its first 64 pins): what is on them is read once the circuit is
    // solved, and their stamp is a small leak, so they do not join their node to the part's block. A pin past 64 (a
    // custom part's) never only senses: its leak joins its node to the part's block, solved the same in a bigger one.
    u64                   sense;        // its pins that only sense (a bit for each of its first 64), as above
    b8                    spans;        // one of its sensing pins is in another block than its own
    // Its limits (above: its own, those of a real part its text names, or a typical part's), and how it stands to them
    // as last stepped: each limit's measure (W, A, V); its worst measure over its limit (1: at it); its heat (1: it
    // burns); whether it is burnt (open until the circuit starts again); its worst limit; and that limit's measure at
    // its worst since the circuit started.
    fude_zoom_limits      limits;
    f64                   measure[FUDE_ZOOM_LIMIT_COUNT];
    f64                   stress;
    f64                   heat;
    b8                    burnt;
    u8                    worst;
    f64                   worst_seen;
    b8                    told;         // a source past its current has been reported, until it is back within it
    // When the circuit could not be solved: this part is one where it would not settle (a place to look).
    b8                    suspect;      // the circuit was not solved, and this part would not settle
    // Where what it holds beyond its state is in the circuit's store (a display's lights as seen, an LCD's controller).
    u32                   store_at, store_size;
} fude_zoom_circuit_part;

// What happened to a part as the circuit stepped, for the page to tell (in the circuit's events, which the page takes).
typedef enum {
    FUDE_ZOOM_CIRCUIT_BURNT = 1,   // past a limit long enough: burnt
    FUDE_ZOOM_CIRCUIT_OVER,        // a source past its current (a short circuit, or near one)
    // A contact opened on a coil's current and arced, with no diode to carry the current; `measure` is the volts over
    // it.
    FUDE_ZOOM_CIRCUIT_ARC          // a contact opened on a coil's current and arced (no diode for it)
} FUDE_ZOOM_CIRCUIT_EVENT_;

// One thing that happened to a part.
typedef struct {
    u32 part;                      // its index in the circuit's parts
    u8  kind;                      // a FUDE_ZOOM_CIRCUIT_EVENT_
    u8  limit;                     // the FUDE_ZOOM_LIMIT_ it was past
    f64 measure, most;             // how much, and its limit
} fude_zoom_circuit_event;

#define FUDE_ZOOM_PROBE_LIT ((rde_color){ 255, 50, 50, 255 })   // a logic probe's colour when lit, unless chosen

// A wire in the circuit: its object, its points (the home frame's units), its node, and its currents (amperes) as
// last solved.
#define FUDE_ZOOM_WIRE_PIECES 8u   // the most pieces a wire has between what joins it (its ends, wires' ends on it)

typedef struct {
    u32           object;
    fude_zoom_v2  points[FUDE_ZOOM_WIRE_POINTS];
    u32           count;
    u32           node;
    f64           length;                           // its length (home units)
    u32           pieces;
    f64           cut[FUDE_ZOOM_WIRE_PIECES + 1u];   // its pieces' ends, as lengths along it (0 … length)
    f64           current[FUDE_ZOOM_WIRE_PIECES];    // each piece's current (amperes) along it, as last solved
    f64           phase[FUDE_ZOOM_WIRE_PIECES];      // how far its dots have moved along it (page_circuit.c)
    b8            loose[2];                         // its ends joined to nothing (a pin, a lead, a wire): shown as such
} fude_zoom_circuit_wire;

// A circuit, worked out from the canvas and simulated.
typedef struct {
    rde_arr TYPE(fude_zoom_circuit_part) parts;
    rde_arr TYPE(fude_zoom_circuit_wire) wires;
    u32     nodes;                         // nodes (0 is ground): its pins' and wires', none inside a part
    f64     unit;                          // the parts' grid: a tenth of a resistor's length (home units), the smallest
    rde_arr TYPE(f64) v;                   // each node's voltage as last solved (read: fude_zoom_circuit_volts)
    f64     time;                          // seconds simulated
    // Its longest step of time (seconds): a millisecond, shorter for what changes faster (a 40th of an AC source's or a
    // clock's period, a 50th of an oscilloscope's sweep).
    f64     step;                          // its longest step of time (seconds), as above
    // Its steps of time as it goes (fude_zoom_circuit_advance): shorter while what it stores changes fast (how far a
    // capacitor's curve bends in a step and how far its voltage goes, an inductor's current: each step's change is
    // kept to so much), an eighth of the last after a jump (not again on the step right after one), landing on what
    // is about to happen (a clock's edge, a 555's threshold as its capacitor heads for it), and growing again, to at
    // most four times the last step, up to `step`.
    f64     step_now;
    f64     step_ratio;                    // the last step's worst change over what a step should change (1 at most)
    // Capacitors and inductors are stepped by the trapezoidal rule (their currents and voltages taken as the mean of a
    // step's ends: second order, so an oscillation keeps its size), and by backward Euler on the step after a jump (a
    // switch, a logic edge, a thyristor firing, the start), as SPICE does, because the trapezoidal rule rings after
    // one. `be_next` asks for backward Euler next: a jump between steps (fude_zoom_circuit_jump) or within one sets it,
    // and the step that takes it clears it. `step_last` and `be_last` are the last step's length and rule (for a
    // motor's load probe).
    b8      be_next;
    f64     step_last;
    b8      be_last;
    b8      ok;                            // solved (false: it did not converge, or there is nothing to solve)
    b8      grounded;                      // a ground part is in it (else the first source's minus is 0 V)
    u32     solves;                        // Newton iterations in the last step
    // Its parts' pins' nodes and currents (each part's pin_room of them from its pin_at: fude_zoom_circuit_part).
    rde_arr TYPE(u32) pin_nodes;
    rde_arr TYPE(f64) pin_amps;
    // How its wires' currents are found (circuit_solve.c): the place where things touch (a vertex; FUDE_ZOOM_NONE:
    // none) of each wire's pieces' ends (FUDE_ZOOM_WIRE_PIECES + 1 for each wire) and of each part's pins (by their
    // places in pin_nodes).
    rde_arr TYPE(u32) cut_vertex;
    rde_arr TYPE(u32) pin_vertex;
    u32     vertices;
    // Its unknowns in BLOCKS, each solved on its own: the nodes a part's pins join are in one block (not counting
    // ground, a held node, a node only logic is on — the logic's own net, at its level's volts —, or a pin that only
    // senses: below). Blocks are the independent circuits on a canvas, and the corners of one circuit that a rail
    // feeds. For each node, its block and its slot (the unknowns in block order; ground and a node in no block have
    // none); for each block, its first slot and how many; and each block's system (sim/sparse.h), shaped by
    // where its parts' stamps write (the joins, as nodes), ordered so that it fills in little, and solved sparse.
    rde_arr TYPE(u32) node_block;
    rde_arr TYPE(u32) node_slot;
    rde_arr TYPE(u32) block_first;
    rde_arr TYPE(u32) block_size;
    u32     blocks;
    rde_arr TYPE(fude_sim_sparse) block_system;
    // For each block, the node held at 0 V: for an island that ground does not reach (a block, or a side of one).
    rde_arr TYPE(u32) block_reference;     // each block's node held at 0 V, as above
    rde_arr TYPE(u64) joins;               // each a row's node << 32 | a column's
    // RAILS HELD: a node that a supply rail is on (and no rail at another voltage) is known, as ground is: at the
    // rail's volts, outside every block, the rail's current being what the rest on its node draw (fzc_held_currents).
    // For each node, its rail (FUDE_ZOOM_NONE: none); and each other pin on a held node (node << 32 | the pin's place
    // in pin_nodes).
    rde_arr TYPE(u32) node_held;
    rde_arr TYPE(u64) held_pins;
    // A block is what a part's pins join, except through a pin that only senses (part.sense: a 555's trigger, a logic
    // input), whose stamp is a leak and whose node is read once solved. `couplings` are the pairs of nodes a stamp
    // writes across (each pair once, smaller << 32 | larger), found as it steps; `reblock` asks for the blocks to be
    // made again with them.
    rde_arr TYPE(u64) couplings;
    b8      reblock;
    // QUIET blocks are left as they were for a step: blocks in which nothing moved in the last step (their capacitors'
    // currents and inductors' voltages next to none), nothing changes by itself (a clock, an AC source: such a block is
    // live), and each part is as it was when last solved (its key: what its stamps read, which is its numbers, the
    // state they use and its pins' volts). So a big circuit whose fast corner alone moves (a 555's tone) is stepped as
    // fast as that corner.
    rde_arr TYPE(u8)  block_still;
    rde_arr TYPE(u8)  block_live;
    rde_arr TYPE(u8)  block_quiet;
    rde_arr TYPE(f64) v_step;              // the voltages as the step began
    rde_arr TYPE(f64) part_key;            // each part's key as last solved, from key_at's
    rde_arr TYPE(u32) key_at;
    // Solve every block (what the logic drives changed within the step; a DC solve).
    b8      wake;                          // solve every block, as above
    u32     skipped;                       // blocks left quiet in the last step
    b8      reshape;                       // a stamp wrote where its block's shape had no place: shaped again
    b8      recording;                     // the stamps are noting where they write, not writing
    b8      suspected;                     // a step not solved: parts marked where it would not settle (their suspect)
    // Scratch.
    rde_arr TYPE(f64) a;                   // a block's matrix, dense (row-major), when it cannot be solved sparse
    rde_arr TYPE(f64) rhs;                 // by slot
    rde_arr TYPE(f64) x;
    rde_arr TYPE(u8)  block_done;          // a block settled in this Newton solve
    rde_arr TYPE(f64) block_most;          // a block's largest change in this step
    rde_arr TYPE(u32) cols;                // the pivot row's columns that are not zero (solving a block)
    rde_arr TYPE(f64) v_keep;              // the voltages kept while a motor's load is worked out
    rde_arr TYPE(f64) amps_keep;           // ...and the pins' currents
    rde_arr TYPE(f64) part_scratch;        // one part's key, or its pins' volts and currents (the most pins a part has)
    // The wires' currents' (fzc_wire_currents, after every advance: each frame while it plays), kept so that a call
    // allocates nothing: each place's current pushed in, the piece up to its parent, the places in breadth-first
    // order, those seen, where each place's pieces start, the pieces place by place, and where the next one goes.
    // Cleared and sized again at each call (zeroed, as new ones were), freed with the circuit.
    rde_arr TYPE(f64) wire_inject;
    rde_arr TYPE(u32) wire_parent;
    rde_arr TYPE(u32) wire_order;
    rde_arr TYPE(u8)  wire_seen;
    rde_arr TYPE(u32) wire_count;
    rde_arr TYPE(u32) wire_adj;
    rde_arr TYPE(u32) wire_fill;
    rde_arr TYPE(u32) limits_of;           // while building: each object's limits attribute (FUDE_ZOOM_NONE: none)
    // What happened as it stepped (parts burnt, sources past their current), for the page to tell and clear.
    rde_arr TYPE(fude_zoom_circuit_event) events;
    // What parts hold beyond their state (each part's store_at and store_size), kept as the circuit is built again.
    rde_arr TYPE(u8) store;
    // Its speakers' sound (below): samples at audio_rate a second (0: none are made), the next one's time, and the slow
    // average taken out of them (a speaker moves no air with what does not change).
    rde_arr TYPE(f32) audio;
    f64     audio_rate;
    f64     audio_next;
    f64     audio_dc;
    // Its logic: gates, flip-flops, chips and custom parts, run by the simulation's digital engine (logic.h). Owned.
    struct fude_zoom_logic* logic;
} fude_zoom_circuit;

// Sets up an empty circuit (with its logic), and frees what one holds.
void fude_zoom_circuit_init(fude_zoom_circuit* _c);
void fude_zoom_circuit_destroy(fude_zoom_circuit* _c);
// Works the circuit out again from what is on the canvas (every part and wire alive and shown, at their places in
// the home frame), keeping each part's state where the part is still there (a capacitor's charge, a flip-flop's Q).
// Returns how many parts it has.
u32  fude_zoom_circuit_build(fude_zoom_circuit* _c, const fude_zoom_scene* _s);
// Builds it the same way from only what _scope marks (one byte for each object; NULL for all): the parts it marks, and
// the wires it marks, those joined to its parts, and those joined to such wires.
u32  fude_zoom_circuit_build_in(fude_zoom_circuit* _c, const fude_zoom_scene* _s, const u8* _scope);
// Takes it back to the start: no time run, nothing charged, and its parts' states read again from their texts at the
// next build (nothing of its logic carried over either).
void fude_zoom_circuit_reset(fude_zoom_circuit* _c);
// Solves it as it stands now, for DC (capacitors open, inductors shorted; what is stepped stays as it is). Returns
// false when it could not be solved.
b8   fude_zoom_circuit_dc(fude_zoom_circuit* _c);
// Says that something changed at once (a switch, a button pressed, a part added): the next step is taken by backward
// Euler, and the steps are cut short.
void fude_zoom_circuit_jump(fude_zoom_circuit* _c);
// Return what part _p holds beyond its state (a display's lights, an f32 each from 0 to 1.5 as seen; an LCD's
// controller), or NULL when it holds nothing. The memory is the circuit's, and stays valid until it is built again.
u8*       fude_zoom_circuit_store(fude_zoom_circuit* _c, const fude_zoom_circuit_part* _p);
const u8* fude_zoom_circuit_store_of(const fude_zoom_circuit* _c, const fude_zoom_circuit_part* _p);
// Returns node _node's voltage as last solved; 0 for ground, for no node, or for a node that does not exist.
f64  fude_zoom_circuit_volts(const fude_zoom_circuit* _c, u32 _node);
// Runs it on by _dt seconds, as fude_zoom_circuit_advance does, working out its wires' currents after. Returns false
// when a step could not be solved.
b8   fude_zoom_circuit_run(fude_zoom_circuit* _c, f64 _dt, u32 _max_steps);
// Runs it on by _span seconds, in steps as short as what happens needs (step_now, above), at most _max_steps of them:
// when that is not enough it goes less far, which plays it in slow motion. _wires says whether to work out its wires'
// currents after. Returns false when a step could not be solved.
b8   fude_zoom_circuit_advance(fude_zoom_circuit* _c, f64 _span, u32 _max_steps, b8 _wires);
// Runs it on by _steps of its longest step (`step`), working out its wires' currents after when _wires is true.
// Returns false when a step could not be solved.
b8   fude_zoom_circuit_steps(fude_zoom_circuit* _c, u32 _steps, b8 _wires);

// A MOTOR: its text gives its rated volts and its speed at them ("6V 60rpm"; 6 V and 60 rpm unless it says).
// FUDE_ZOOM_MOTOR_R is its winding's resistance (ohms).
#define FUDE_ZOOM_MOTOR_R 8.0
// Returns a motor's back-EMF in volts for each radian a second: its rated volts over its rated speed.
f64  fude_zoom_motor_k(const fude_zoom_circuit_part* _motor);
// Gives how the circuit loads a shafted motor (coupling.h) now, near where it is: *_conductance is how much more
// current (amperes, pin 0 to pin 1) one volt less of its back-EMF makes (0 or more; 0 when nothing is joined to it),
// and *_still is the back-EMF at which no current would flow (volts). It is worked out from the circuit as it last
// stepped, and leaves the circuit as it was. Returns false when part _part is not a shafted motor, or the circuit is
// not solved.
b8   fude_zoom_circuit_motor_load(fude_zoom_circuit* _c, u32 _part, f64* _conductance, f64* _still);

// A SOLENOID (its text: its rated volts, "12V"): a coil, whose resistance is its rated volts over half an ampere and
// whose inductance is 2 ms of its resistance, and a plunger out of it, pulled in as hard as the square of its current
// (its rated pull at its rated current when halfway in; more as it is further in, less further out) against a light
// spring. Alone it goes in when its pull beats the spring and out when it does not: in from all the way out from 87%
// of its rated current, and out again below 57% of it (held in, it holds on with less), as a solenoid's or a relay's
// does. On a mechanism (coupling.h) its plunger is the mechanism's: it pulls what is pinned on its end (its hole),
// pushes what is in its way, and presses a button. Its state: 0 its current (pin 0 to pin 1), 1 the voltage over it,
// 2 how far in its plunger is (0 out, 1 in), 3 whether it is on a mechanism (1) or not (0).
#define FUDE_ZOOM_SOLENOID_STROKE 0.3    // its plunger's travel, as a fraction of its half width
#define FUDE_ZOOM_SOLENOID_TIP    0.85   // its plunger's end (its hole), out from its middle, in half widths
#define FUDE_ZOOM_SOLENOID_SPRING 0.45   // its spring's push, as a fraction of its rated pull
// Returns a solenoid's pull now, as a fraction of its rated pull.
f64  fude_zoom_circuit_solenoid_pull(const fude_zoom_circuit_part* _p);
// A SERVO works only on a supply (VCC over GND) of at least this many volts: below it, it neither turns nor holds. Its
// step in the circuit (circuit_step.c) and its turning of a mechanism (coupling.c) both go by this one value.
#define FUDE_ZOOM_SERVO_LEAST 3.5
// A UNIPOLAR STEPPER MOTOR (a 28BYJ-48: its text "28BYJ-48", 2048 steps a turn of its geared shaft; or how many it has,
// "200 steps"): four coils from COM to A, B, C and D (50 Ω each), each a quarter of its rotor's electrical turn on from
// the one before. Its rotor is drawn toward where their currents together point, the nearest way there, as fast as it
// can (500 steps a second). So it follows coils energised in turn (A, B, C, D: a step each), two at a time, or in half
// steps; driven too fast, or with the opposite coil, it is lost. Its shaft turns four steps for each turn of its
// rotor's field. On a mechanism, it turns what is on its shaft there as hard as its holding torque, and holds it there
// (pushed past half a turn of its field, it slips a step). Its state: 0 its rotor's electrical angle (radians), 1 its
// shaft's angle (degrees), 2 its steps (full ones, from where it began), 3–6 its coils' currents (into each from COM),
// 7 whether it is on a mechanism's shaft (0 or more) or not (−1).
#define FUDE_ZOOM_STEPPER_COIL  50.0    // ohms
#define FUDE_ZOOM_STEPPER_RATE  500.0   // full steps a second, at most
// Returns where a stepper's coils' currents point (radians: A's way is 0, B's a quarter turn on…), and sets
// *_strength to how strongly, as a fraction of one coil's at 5 V.
f64  fude_zoom_circuit_stepper_field(const fude_zoom_circuit_part* _p, f64* _strength);
// A ROTARY ENCODER (its text: its detents a turn, "20"): A and B are each a contact to C, closed in turn as its shaft
// turns (quadrature). Each contact closes and opens within a detent, A a quarter of a detent before B when turned
// clockwise, and both are open at a detent. Alone, a tap turns it a detent clockwise; on a mechanism, it turns as what
// is on its shaft turns. Its state: 0 where it is (detents, clockwise), 1 where a tap sends it, 6 where its contacts
// were at the last step, 7 whether it is on a mechanism's shaft (0 or more) or not (−1).
// fude_zoom_circuit_encoder_contacts gives whether A and B are closed at position _detents.
void fude_zoom_circuit_encoder_contacts(f64 _detents, b8* _a, b8* _b);
// A SLOTTED OPTICAL SENSOR (a photo-interrupter): A and K are its infrared LED's (1.2 V), C and E its
// phototransistor's, which conducts a tenth of the LED's current while its slot is clear and nothing while something
// is in it (a mechanism's part going through its slot; alone, a tap, as a hand in it). Its state: 0 blocked (1) or
// clear (0).
#define FUDE_ZOOM_SLOT_CTR 0.1   // its phototransistor's current over its LED's, with the slot clear
// THYRISTORS. An SCR (A, K, G) is off, passing nothing but a leak up to its breakover (its voltage limit), until its
// gate takes its trigger current with its anode above its cathode; then it is on, a diode from anode to cathode (a
// volt), until what goes through it falls under its holding current. A TRIAC (MT2, MT1, G) does the same either way
// round, triggered by its gate's current either way: on AC it goes off at each crossing, and on again when the gate
// fires it. Their state: 0 on (1) or off (0). Their trigger and holding currents are values 0 and 1.
// A TRANSFORMER (P1, P2, S1, S2; its text is its turns' ratio, "2:1"): two windings coupled at 0.995, its primary
// 10 H and its secondary less by the square of the ratio; 1 Ω in its primary and less by as much in its secondary. So
// on AC its secondary has its primary's volts over its ratio, and a load's current comes back in its primary over it;
// on DC there is only its windings' resistance. Its state: 0 and 1 its windings' currents, 2 and 3 their voltages (at
// the last step).
// A BRIDGE RECTIFIER (AC, +, AC, −): four diodes: from whichever AC pin is higher into +, and out of − into whichever
// is lower.
// An OPTOCOUPLER (A, K, C, E: a PC817): its LED's light falls on its phototransistor, which passes from C to E its CTR
// (1) times its LED's current, less as C nears E. A PHOTODIODE (A, K): a diode, with a current against it as its light
// (50 nA a lux). A PHOTOTRANSISTOR (C, E): as much current as its light (2 µA a lux), less as C nears E. Their light is
// state 0, in lux, as an LDR's is (from its text, a tap, or Play's slider).
#define FUDE_ZOOM_PHOTODIODE_A_LUX       50e-9
#define FUDE_ZOOM_PHOTOTRANSISTOR_A_LUX  2e-6
// Returns whether a part's model is lit: an LDR, a photodiode or a phototransistor, whose state 0 is its light in lux.
b8   fude_zoom_circuit_lit(u8 _model);
// A SPEAKER (8 Ω, or as its text says): the current through it is its cone's push. While the circuit makes sound
// (audio_rate above 0), its sound is the speakers' currents together, a sample every 1/audio_rate seconds of the
// circuit's time, 0.1 A being full scale, with the slow average (under 20 Hz) taken out; the page plays the samples.
// fude_zoom_circuit_sound makes sound at _rate samples a second from now on (0: none), letting go of the samples made
// so far.
void fude_zoom_circuit_sound(fude_zoom_circuit* _c, f64 _rate);
// A contact (a switch's, a button's, a changeover's, a relay's) arcs when it opens on a coil's current with no diode
// to carry it: FUDE_ZOOM_ARC volts over it.
#define FUDE_ZOOM_ARC 50.0
// fude_zoom_circuit_sparked returns when a contact last arced (circuit seconds, its state[7]; 0 or less: never, or not
// a contact).
// fude_zoom_circuit_tap changes part _part's state as a tap does: a switch, a button pressed or let go, a logic input,
// a changeover switch, a pot's wiper a quarter on, a board's pin _pin through input, high, low and blink, a lit
// sensor's light or a thermistor's temperature to the next of a few, an encoder a detent on, a slotted sensor's slot
// blocked or clear. It writes what the part says now into _say in no language ("" when there is nothing to say): a
// logic level ("1"), a contact's or a pin's name ("B", "D13"), a number with its unit's symbol ("25 °C", "75%"), or
// a lit sensor's lux ("100"). The words that go with it (a switch's on, a pin's high, "lux") are the page's to say in
// the person's language, from fude_zoom_circuit_tap_word. It cuts the circuit's steps short. Returns false when there
// is nothing to change there.
f64  fude_zoom_circuit_sparked(const fude_zoom_circuit_part* _p);
b8   fude_zoom_circuit_tap(fude_zoom_circuit* _c, u32 _part, i32 _pin, c8* _say, usize _size);
// The word that goes with what a tap made a part say (fude_zoom_circuit_tap_word), which the page says in the
// person's language: PLAIN when _say says it all.
typedef enum {
    FUDE_ZOOM_TAP_PLAIN = 0,   // _say alone (a level, a contact's name, a number with its unit), or nothing
    FUDE_ZOOM_TAP_ON,          // a switch closed (_say "")
    FUDE_ZOOM_TAP_OFF,         // a switch open (_say "")
    FUDE_ZOOM_TAP_HIGH,        // a board's pin (_say: its name) driven high
    FUDE_ZOOM_TAP_LOW,         // ...driven low
    FUDE_ZOOM_TAP_BLINK,       // ...blinking
    FUDE_ZOOM_TAP_INPUT,       // ...an input again, driven by nothing
    FUDE_ZOOM_TAP_LUX          // a lit sensor's light (_say: how many lux)
} FUDE_ZOOM_TAP_;
// Returns the word (a FUDE_ZOOM_TAP_) that goes with what part _p says now, as a tap at its pin _pin (a board's; -1
// for any other part) left it.
u8   fude_zoom_circuit_tap_word(const fude_zoom_circuit_part* _p, i32 _pin);
// Turns oscilloscope _part's knob _knob (display.h's) a step _way (+1 clockwise, -1 back) as it plays. When its time a
// division changes, its sweep starts again and the circuit's steps are made no longer than a 50th of its sweep. Writes
// what the knob is set to now into _say ("CH1 VOLTS/DIV 0.5 V"), even when it was at its end already. Returns false
// when it is not an oscilloscope's knob.
b8   fude_zoom_circuit_scope_turn(fude_zoom_circuit* _c, u32 _part, u32 _knob, i32 _way, c8* _say, usize _size);
// Returns how high Play's tags' letters are (a node's volts, a meter's reading, a logic input's 0 or 1; screen points)
// for parts whose smaller half sizes on the screen are _sizes: half the typical part's (their median; sizes that are 0
// or not finite, parts not seen, are not counted), at most _most, and 0 (no tags drawn) below _least, as a part's pin
// names are not drawn then. _sizes is reordered in place.
f64  fude_zoom_circuit_tag_px(f64* _sizes, u32 _count, f64 _most, f64 _least);

#endif
