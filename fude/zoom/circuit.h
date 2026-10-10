// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_CIRCUIT_H
#define FUDE_ZOOM_CIRCUIT_H

#include "rde.h"
#include "sim/sparse.h"
#include "zoom/zoom.h"
#include "zoom/scene.h"

struct fude_zoom_logic;

// ===========================================================================
// ELECTRONICS (Sketching's Electronics topic): circuits drawn as schematics and
// simulated as they are drawn.
//
// PARTS are the diagram library's symbols (symbol.h: the Electronics, Logic,
// Chips and modules, and Boards families), each with its PINS — where a wire
// meets it (its own u, v from -1 to 1, as its drawing) — and what it does in the
// simulation (its MODEL). A part's text is its value, read as it is typed: "10k",
// "4.7 kΩ", "100uF", "9V", "5V 50Hz", "1Hz", "red", "on"; a board's or a chip's
// lines after its name say how its pins are driven ("D13 blink", "D9 high").
//
// WIRES (shape.h's WIRE) are lines whose ends keep the pins they were drawn to
// (a part's id and its pin), following the part when it moves; where a wire's
// end lies on another wire, they are joined there.
//
// THE CIRCUIT is worked out from what is on the canvas — the nodes (pins and
// wire ends where they meet, wires and what they join), each part's elements on
// them — and SIMULATED by modified nodal analysis: the nodes' voltages and the
// sources' currents solved together, Newton's method for what is not linear
// (diodes, LEDs, transistors, op-amps), in steps of time (capacitors and
// inductors by backward Euler; logic, timers and boards stepped with it). What
// it finds is drawn over the schematic (page.c): each wire's voltage, current
// flowing along it as dots, LEDs and lamps lit, meters' readings, logic levels.
// ===========================================================================

// A pin: where it is on its part (u, v from -1 to 1 of its half sizes), its name ("" for a plain lead), and the
// side its wire leaves by (FUDE_ZOOM_PIN_SIDE_).
typedef enum {
    FUDE_ZOOM_PIN_LEFT = 0,
    FUDE_ZOOM_PIN_RIGHT,
    FUDE_ZOOM_PIN_UP,
    FUDE_ZOOM_PIN_DOWN,
    FUDE_ZOOM_PIN_ANY     // a hole in the middle of it (a breadboard's): a wire comes into it any way, straight
} FUDE_ZOOM_PIN_SIDE_;

typedef struct {
    f32       u, v;
    const c8* name;
    u8        side;
} fude_zoom_pin;

// What a part does (circuit.c's models).
typedef enum {
    FUDE_ZOOM_MODEL_NONE = 0,   // drawn only: its pins joined to nothing inside (a module, a microcontroller)
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
    FUDE_ZOOM_MODEL_GATE,        // a logic gate (its kind: the part's gate)
    FUDE_ZOOM_MODEL_DFF,
    FUDE_ZOOM_MODEL_TFF,
    FUDE_ZOOM_MODEL_LOGIC_IN,
    FUDE_ZOOM_MODEL_LOGIC_OUT,
    FUDE_ZOOM_MODEL_TIMER555,
    FUDE_ZOOM_MODEL_REGULATOR,   // IN, GND, OUT: its volts and dropout the part's
    FUDE_ZOOM_MODEL_SHIFT595,
    FUDE_ZOOM_MODEL_DRIVER293,
    FUDE_ZOOM_MODEL_ULN2003,
    FUDE_ZOOM_MODEL_RELAY,
    FUDE_ZOOM_MODEL_BOARD,       // its power pins sources; its pins driven as its text says
    FUDE_ZOOM_MODEL_SIM,         // a definition of the simulation's (a chip, a custom part): logic.h
    FUDE_ZOOM_MODEL_SERVO,       // GND, VCC, SIG: its horn's angle as its signal's pulses say (circuit.c's fzc_servo)
    // Displays (display.h): a panel of 7-segment digits, an LED matrix, a 10-LED bar graph, an LM3914 (a bar graph's
    // driver), a panel meter, a character LCD (an HD44780's).
    FUDE_ZOOM_MODEL_SEG_PANEL,
    FUDE_ZOOM_MODEL_LED_MATRIX,
    FUDE_ZOOM_MODEL_BAR_GRAPH,
    FUDE_ZOOM_MODEL_LM3914,
    FUDE_ZOOM_MODEL_PANEL_METER,
    FUDE_ZOOM_MODEL_CHAR_LCD,
    FUDE_ZOOM_MODEL_SCOPE,       // an oscilloscope: CH1, CH2, GND — their voltages over time on its screen (display.h)
    FUDE_ZOOM_MODEL_SPDT,        // a changeover switch: COM to A, or (tapped) to B
    FUDE_ZOOM_MODEL_DPDT,        // two, worked together: COM1 to A1 or B1, COM2 to A2 or B2
    FUDE_ZOOM_MODEL_LDR,         // a light-dependent resistor: as bright as its light (state[0]: lux)
    FUDE_ZOOM_MODEL_THERMISTOR,  // an NTC thermistor: as warm as its temperature (state[0]: °C)
    // Worked by, or working, a mechanism (coupling.h): a solenoid (its coil, its plunger), a unipolar stepper motor, a
    // rotary encoder, a slotted optical sensor (below).
    FUDE_ZOOM_MODEL_SOLENOID,
    FUDE_ZOOM_MODEL_STEPPER,
    FUDE_ZOOM_MODEL_ENCODER,
    FUDE_ZOOM_MODEL_SLOT,
    // (0.1.75) Flip-flops of the logic's own (J, CLK, K; S, R), an LM393's two open-collector comparators, an LM358's two
    // op-amps, thyristors (an SCR, a TRIAC), a transformer and a bridge rectifier, light (an optocoupler, a photodiode,
    // a phototransistor), a speaker that sounds (below).
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

typedef struct {
    const c8*            id;          // its symbol's id (symbol.c's catalogue)
    u8                   model;       // FUDE_ZOOM_MODEL_
    u8                   look;        // how it is drawn (circuit.c)
    u8                   gate;        // a gate's kind; a 74HC's (its gates')
    const fude_zoom_pin* pins;
    u16                  pin_count;   // (a breadboard's holes: worked out, fude_zoom_part_pin)
    const c8*            value;       // its text to begin with
    f32                  a, b;        // the model's own: a regulator's volts and dropout, a board's supply volts...
} fude_zoom_part;

// A BREADBOARD (half size): 30 columns of holes, rows a–e and f–j (each column's five joined), and a pair of rails
// along its top and its bottom (each rail joined along). Parts' pins and wires' ends in its holes are joined as its
// strips are.
#define FUDE_ZOOM_BREADBOARD_COLS 30u
#define FUDE_ZOOM_BREADBOARD_ROWS 14u   // + − rails, a–e, f–j, + − rails
// Its pin _i's strip (holes with the same are joined).
u32  fude_zoom_breadboard_strip(u32 _i);

// The part whose symbol's id is _id (NULL: not a part).
const fude_zoom_part* fude_zoom_part_find(const c8* _id);
// ...and by a symbol's kind (symbol.h; NULL: not a part).
const fude_zoom_part* fude_zoom_part_of_kind(u32 _kind);
// How many parts there are, and each.
u32                   fude_zoom_part_count(void);
const fude_zoom_part* fude_zoom_part_at(u32 _i);

// Where a part's body starts in from its pins (its u: its pins' names go inside it from there); 0: its pins are not named
// on it (a resistor's, a breadboard's).
f32  fude_zoom_part_inset(const fude_zoom_part* _part);
// ...and where its pins' names go, of its half sizes: a left or right pin's (_u), a top or bottom one's (_v) — a sized
// display's as big as it is (display.h).
void fude_zoom_part_insets(const fude_zoom_part* _part, f32* _u, f32* _v);
// Words on a part's face by what they name (an oscilloscope's knobs: CH1, CH2, VOLTS/DIV, POS, TIME/DIV): each its middle
// at (u, v) of its half sizes. How many (at most _most).
typedef struct {
    f32       u, v;
    const c8* text;
} fude_zoom_part_label;
u32  fude_zoom_part_labels(const fude_zoom_part* _part, fude_zoom_part_label* _out, u32 _most);
// A part's height as it comes (the catalogue's units): its symbol's (_catalogue_h), a sized display's its size's.
f64  fude_zoom_part_room_h(const fude_zoom_part* _part, f64 _catalogue_h);
// A part's pin _i (a breadboard's holes worked out). False: none.
b8   fude_zoom_part_pin(const fude_zoom_part* _part, u32 _i, fude_zoom_pin* _out);

// A part drawn (symbol.c's symbols call it): its polylines for half sizes _hw × _hh into _points (fude_zoom_v2) and
// _parts (fude_zoom_symbol_part), as symbol.h's parts (the first its outline). How many parts.
u32  fude_zoom_part_draw(const fude_zoom_part* _part, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points, rde_arr* _parts);

// A part's pin _pin where it is now (its symbol _object's frame's units). False: no such pin.
b8   fude_zoom_part_pin_at(const fude_zoom_scene* _s, u32 _object, u32 _pin, fude_zoom_v2* _out);
// A part's grid: a tenth of its pins' room as it came, in its frame's units (0: not a part). Its wires' stubs.
f64  fude_zoom_part_unit(const fude_zoom_scene* _s, u32 _object);
// The part an object is (a symbol whose kind is a part's), or NULL.
const fude_zoom_part* fude_zoom_part_of(const fude_zoom_scene* _s, u32 _object);
// A custom part (a definition of a person's own: its pins round a block, logic.h).
b8   fude_zoom_part_custom(const fude_zoom_part* _part);
// The part a symbol's numbers (its kind, its half sizes, its letters' height, its text) are: a custom part's as its
// text names it.
const fude_zoom_part* fude_zoom_part_of_numbers(const f64* _n, u32 _count);
// ...a symbol of kind _kind's with text _text (a custom part, a sized display: as it says).
const fude_zoom_part* fude_zoom_part_of_text(u32 _kind, const c8* _text);
// A part made for a symbol from its text (a custom part's, a sized display's: its own pins).
b8   fude_zoom_part_made(const fude_zoom_part* _part);

// --- wires ----------------------------------------------------------------------------------------

#define FUDE_ZOOM_WIRE_POINTS 32u   // a wire's points, at most

// A wire's numbers (shape.h's WIRE): its points (from its translation), then the part and pin at each end
// (an object id — its two halves, scene.h's fude_zoom_id_put — and a pin; 0 / -1: none). How many numbers. (Before
// 0.1.49 an id was one number, rounded once a device's half was large: read as it was, its part found by its pin.)
u32  fude_zoom_wire_numbers(f64* _n, const fude_zoom_v2* _points, u32 _count, fude_zoom_id _from, i32 _from_pin, fude_zoom_id _to, i32 _to_pin);
// ...and back: its points (from its translation), its ends' parts and pins. How many points.
u32  fude_zoom_wire_of(const f64* _n, u32 _count, fude_zoom_v2* _points, fude_zoom_id* _from, i32* _from_pin, fude_zoom_id* _to, i32* _to_pin);
// The part a wire's end keeps (its id, its pin _pin; an old save's id, rounded: the part with such an id whose pin is
// where that end is — _end, in frame _frame's units). FUDE_ZOOM_NONE: none.
u32  fude_zoom_wire_part(const fude_zoom_scene* _s, fude_zoom_id _id, i32 _pin, u32 _frame, fude_zoom_v2 _end);
// A wire's way from _a to _b (frame units), leaving _a by _side_a and coming into _b by _side_b (FUDE_ZOOM_PIN_SIDE_;
// 255: either): square — out of each pin its way _stub first, then across and up between them, never back over
// itself. How many points (into _out, at most 6).
u32  fude_zoom_wire_route(fude_zoom_v2 _a, u8 _side_a, fude_zoom_v2 _b, u8 _side_b, f64 _stub, fude_zoom_v2* _out);
// The side a part's pin faces now (as the part is turned: FUDE_ZOOM_PIN_SIDE_), 255: not a pin, or any way (a hole).
u8   fude_zoom_part_side_at(const fude_zoom_scene* _s, u32 _object, u32 _pin);
// A part's pin _pin in frame _frame's units (wherever the part is: another frame's, deeper or shallower), the side
// it faces there (255: any), and the parts' grid there (a wire's stub). False: no such pin.
b8   fude_zoom_part_pin_in(const fude_zoom_scene* _s, u32 _object, u32 _pin, u32 _frame, fude_zoom_v2* _at, u8* _side, f64* _stub);
// A new wire through _points (frame units) in _frame, its ends' parts and pins. Its index.
u32  fude_zoom_wire_add(fude_zoom_scene* _s, u32 _frame, const fude_zoom_v2* _points, u32 _count, u32 _from, i32 _from_pin, u32 _to, i32 _to_pin,
                        rde_color _color, f32 _radius);
// The wires joined to any of _moved (object indexes, just moved; themselves not among them): each made again with
// its ends where their pins are now (routed square again), both into _died and _born. How many.
u32  fude_zoom_wire_follow(fude_zoom_scene* _s, const u32* _moved, u32 _count, rde_arr* _died, rde_arr* _born);
// A wire joins two things or is not: the wires left joined to nothing at an end by what just died (_died: objects,
// TYPE(u32)) — an end at a dead part's pin, or on a dead wire and on nothing else — killed too and added to _died, and
// those they leave so, in turn (one undo step with what died first). How many.
u32  fude_zoom_wire_orphans(fude_zoom_scene* _s, rde_arr* _died);
// _wire again with the ids it is joined to _old[i] now _new[i], its ends where those pins are: the new wire, or
// FUDE_ZOOM_NONE (joined to none of them). The caller lets the old one go.
u32  fude_zoom_wire_remap(fude_zoom_scene* _s, u32 _wire, const fude_zoom_id* _old, const fude_zoom_id* _new, u32 _n);

// --- values -----------------------------------------------------------------------------------------

// A value as a part's text says it ("4.7k", "4k7", "100 uF", "10mH", "5V 50Hz": the _index-th number of it, its
// metric prefix applied: p n u µ m k M G). False: none there.
b8   fude_zoom_circuit_value(const c8* _text, u32 _index, f64* _out);
// A light-dependent resistor's resistance in _lux of light (a GL5528's: 20 kΩ at 10 lux, as its light to the -0.7th,
// 100 Ω to 1 MΩ); an NTC thermistor's at _celsius (_r25 at 25 °C, its B constant _beta).
f64  fude_zoom_circuit_ldr_ohms(f64 _lux);
f64  fude_zoom_circuit_ntc_ohms(f64 _r25, f64 _beta, f64 _celsius);
// What a sensor's slider spans, and where _value is along it (0–1), and back: an LDR's light (1 to 100 000 lux, as its
// logarithm), a thermistor's temperature (-20 to 120 °C).
f64  fude_zoom_circuit_sensor_along(u8 _model, f64 _value);
f64  fude_zoom_circuit_sensor_at(u8 _model, f64 _along);

// --- the circuit and its simulation -----------------------------------------------------------------


// LIMITS: what a part takes at most, as its datasheet says (limits.h) — the power it can turn to heat (W), the current
// through it (A), the voltage across it (V), and the voltage the wrong way round (V: an LED's, a diode's, an
// electrolytic's; a MOSFET's gate, either way). 0: none. A part past one heats up and, hot enough, burns: open from then
// on, until the circuit starts again.
typedef enum {
    FUDE_ZOOM_LIMIT_POWER = 0,
    FUDE_ZOOM_LIMIT_CURRENT,
    FUDE_ZOOM_LIMIT_VOLTAGE,
    FUDE_ZOOM_LIMIT_REVERSE,
    FUDE_ZOOM_LIMIT_COUNT
} FUDE_ZOOM_LIMIT_;

typedef struct {
    f64 most[FUDE_ZOOM_LIMIT_COUNT];
} fude_zoom_limits;

// A part in the circuit: its object, what it is, its pins' nodes, its value(s), its state.
typedef struct {
    u32                   object;
    const fude_zoom_part* part;
    u32                   node[64];     // each pin's node (0: ground; FUDE_ZOOM_NONE: joined to nothing)
    f64                   value[8];     // its numbers as its text says (a resistance, a voltage, a frequency...)
    f64                   state[8];     // what it remembers (a capacitor's voltage, a flip-flop's Q, a 555's...)
    f64                   pin_i[64];    // each pin's current, out of the part into its node (amperes), as last solved
    f64                   shown;        // what it shows (an LED's brightness 0..1, a meter's reading, a motor's turn)
    u32                   branch;       // its first voltage source's row in the system (FUDE_ZOOM_NONE: none)
    u32                   internal;     // its first internal node (FUDE_ZOOM_NONE: none)
    u64                   drive;        // a board's pins driven (bit: pin), and how (below: drive_high, drive_blink)
    u64                   drive_high, drive_blink;
    u64                   switch_on;    // a switch's or a button's, a logic input's: closed / high
    c8                    color[12];    // an LED's colour ("red")
    rde_color             lit;          // a logic probe's colour lit (its LIGHT: props.h; else FUDE_ZOOM_PROBE_LIT)
    // Played with a mechanism (coupling.h): a motor with a part of it on its shaft — its winding behind its back-EMF,
    // the shaft as fast as that part turns (radians a second, the way its voltage turns it) —, a push button a moving
    // part presses (closed as if held).
    b8                    shafted;
    f64                   spin;
    b8                    pushed;
    u32                   block;        // its pins' block of unknowns (below; FUDE_ZOOM_NONE: on ground, held or nothing only)
    u64                   sense;        // its pins that only sense (bit: pin) — what is on them read once solved, a leak stamped
    b8                    spans;        // ...one of them in another block than its own
    // Its limits (above: its own, a real part's its text names, or a typical one's), and how it stands to them as last
    // stepped: each one's measure (W, A, V), the worst measure over its limit (1: at it), its heat (1: it burns), burnt
    // (open until the circuit starts again); its worst limit, and that limit's measure at its worst since it started.
    fude_zoom_limits      limits;
    f64                   measure[FUDE_ZOOM_LIMIT_COUNT];
    f64                   stress;
    f64                   heat;
    b8                    burnt;
    u8                    worst;
    f64                   worst_seen;
    b8                    told;         // (a source past its current: said so, until it is back within it)
    b8                    suspect;      // the circuit not solved: this part where it would not settle (a reason to look)
    // What it holds beyond its state (the circuit's store: a display's lights as seen, an LCD's controller), where.
    u32                   store_at, store_size;
} fude_zoom_circuit_part;

// What happened to a part as the circuit stepped, for the page to tell (fude_zoom_circuit's events, taken by it).
typedef enum {
    FUDE_ZOOM_CIRCUIT_BURNT = 1,   // past a limit long enough: burnt
    FUDE_ZOOM_CIRCUIT_OVER,        // a source past its current (a short circuit, or near one)
    FUDE_ZOOM_CIRCUIT_ARC          // a contact opened on a coil's current and arced (no diode for it: measure, the volts over it)
} FUDE_ZOOM_CIRCUIT_EVENT_;

typedef struct {
    u32 part;                      // its place in the circuit's parts
    u8  kind;                      // FUDE_ZOOM_CIRCUIT_EVENT_
    u8  limit;                     // FUDE_ZOOM_LIMIT_ that it was past
    f64 measure, most;             // how much, and its limit
} fude_zoom_circuit_event;

#define FUDE_ZOOM_PROBE_LIT ((rde_color){ 255, 50, 50, 255 })   // a logic probe's, lit, unless chosen

// A wire in the circuit: its object, its points (the home frame's units), its node, its current (amperes,
// from its first point to its last) as last solved.
#define FUDE_ZOOM_WIRE_PIECES 8u   // a wire's pieces between what joins it (its ends, other wires' ends on it), at most

typedef struct {
    u32           object;
    fude_zoom_v2  points[FUDE_ZOOM_WIRE_POINTS];
    u32           count;
    u32           node;
    f64           length;                           // along it (home units)
    u32           pieces;
    f64           cut[FUDE_ZOOM_WIRE_PIECES + 1u];   // its pieces' ends, as lengths along it (0 … length)
    f64           current[FUDE_ZOOM_WIRE_PIECES];    // each piece's current (amperes) along it, as last solved
    f64           phase[FUDE_ZOOM_WIRE_PIECES];      // its dots' way along, as they have moved (page.c)
    b8            loose[2];                         // its ends joined to nothing (a pin, a lead, a wire): shown as such
} fude_zoom_circuit_wire;

typedef struct {
    rde_arr TYPE(fude_zoom_circuit_part) parts;
    rde_arr TYPE(fude_zoom_circuit_wire) wires;
    u32     nodes;                         // nodes (0: ground), internal ones included
    f64     unit;                          // the parts' grid (a tenth of a resistor's length: home units), the smallest
    u32     branches;                      // voltage sources' currents solved for
    rde_arr TYPE(f64) v;                   // each node's voltage, as last solved (nodes of them; read: fude_zoom_circuit_volts)
    f64     time;                          // seconds simulated
    f64     step;                          // its longest step of time (seconds): a millisecond, finer for an AC source
    // Its steps of time as it goes (fude_zoom_circuit_advance): finer while what it stores changes fast (a capacitor's
    // voltage, an inductor's current — each step's change at most so much), and landing on what is about to happen (a
    // clock's edge, a 555's threshold as its capacitor heads for it); growing again, twice at a time, up to step.
    f64     step_now;
    f64     step_ratio;                    // (the last step's worst change over what a step should change: 1 at most)
    // Capacitors and inductors stepped by the trapezoidal rule (their currents and voltages as the mean of a step's ends:
    // second order, an oscillation kept as it is); by backward Euler on the step after a jump (a switch, a logic edge, the
    // start), as SPICE does — the trapezoidal rule rings on one. The last step's length and way (a motor's load probe).
    b8      be_next;
    f64     step_last;
    b8      be_last;
    b8      ok;                            // solved (false: it did not converge, or there is nothing to solve)
    b8      grounded;                      // a ground part is in it (else the first source's minus is 0 V)
    u32     solves;                        // Newton iterations, last step
    // How its wires' currents are found (circuit.c): each wire's pieces' ends' and each part's pins' places where
    // things touch (vertices; FUDE_ZOOM_NONE: none), FUDE_ZOOM_WIRE_PIECES + 1 a wire, 64 a part.
    rde_arr TYPE(u32) cut_vertex;
    rde_arr TYPE(u32) pin_vertex;
    u32     vertices;
    // Its unknowns in BLOCKS: the nodes a part's pins join (ground not counted) are one — the independent circuits on a
    // canvas, each solved on its own. Each node's block and slot (the unknowns in block order; ground: none), each
    // block's first slot and how many; each block's system (sim/sparse.h: shaped by where its parts' stamps write —
    // the joins, as nodes — ordered so that it fills in little, and solved sparse).
    rde_arr TYPE(u32) node_block;
    rde_arr TYPE(u32) node_slot;
    rde_arr TYPE(u32) block_first;
    rde_arr TYPE(u32) block_size;
    u32     blocks;
    rde_arr TYPE(fude_sim_sparse) block_system;
    rde_arr TYPE(u32) block_reference;     // the nodes held at 0 V: one an island ground reaches nowhere (a block, a side of one)
    rde_arr TYPE(u64) joins;               // each a row's node << 32 | a column's
    // RAILS HELD: a node a supply rail is on (and no rail at another voltage) is known, as ground is — at the rail's volts,
    // outside every block; the rail's current what the rest on its node draw (fzc_held_currents). Each node's rail
    // (FUDE_ZOOM_NONE: none); each other pin on a held node (node << 32 | part << 6 | pin).
    rde_arr TYPE(u32) node_held;
    rde_arr TYPE(u64) held_pins;
    // A block is what a part's pins join — but through a pin that only senses (part.sense: a 555's trigger, a logic
    // input): its stamp a leak, what it reads read once solved —, and what a stamp writes across (each pair of nodes
    // once, smaller << 32 | larger: found as it steps, the blocks then made again, reblock).
    rde_arr TYPE(u64) couplings;
    b8      reblock;
    // QUIET blocks, left as they were for a step: one nothing in moved last step (its capacitors' currents and its
    // inductors' voltages next to none, still), nothing in changing by itself (a clock, an AC source: live), each of its
    // parts as it was when it was last solved (its key: what its stamps read — its numbers, the state they use, its pins'
    // volts). A big circuit whose fast corner alone moves (a 555's tone) is stepped as fast as that corner.
    rde_arr TYPE(u8)  block_still;
    rde_arr TYPE(u8)  block_live;
    rde_arr TYPE(u8)  block_quiet;
    rde_arr TYPE(f64) v_step;              // the voltages as the step began
    rde_arr TYPE(f64) part_key;            // each part's key as last solved, from key_at's
    rde_arr TYPE(u32) key_at;
    b8      wake;                          // every block solved (what the logic drives changed within the step; a DC solve)
    u32     skipped;                       // blocks left quiet, last step
    b8      reshape;                       // a stamp wrote where its block's shape had no place: shaped again
    b8      recording;                     // (the stamps noting where they write, not writing)
    b8      suspected;                     // a step not solved: parts marked where it would not settle (their suspect)
    // Scratch.
    rde_arr TYPE(f64) a;                   // a block's matrix, dense (row-major), when it cannot be solved sparse
    rde_arr TYPE(f64) rhs;                 // by slot
    rde_arr TYPE(f64) x;
    rde_arr TYPE(u8)  block_done;          // a block settled in this Newton solve
    rde_arr TYPE(f64) block_most;          // ...its largest change, this step
    rde_arr TYPE(u32) cols;                // the pivot row's columns not zero (solving a block)
    rde_arr TYPE(f64) v_keep;              // the voltages kept while a motor's load is worked out
    rde_arr TYPE(u32) limits_of;           // (building: each object's limits attribute, FUDE_ZOOM_NONE: none)
    // What happened as it stepped (parts burnt, sources past their current), the page's to tell and clear.
    rde_arr TYPE(fude_zoom_circuit_event) events;
    // What parts hold beyond their state (each part's store_at, store_size: kept as the circuit is built again).
    rde_arr TYPE(u8) store;
    // Its speakers' sound (below): samples at audio_rate a second (0: none made), the next one's time, the slow average
    // taken out of them (a speaker moves no air with what does not change).
    rde_arr TYPE(f32) audio;
    f64     audio_rate;
    f64     audio_next;
    f64     audio_dc;
    // Its logic: gates, flip-flops, chips, custom parts run by the simulation's digital engine (logic.h).
    struct fude_zoom_logic* logic;
} fude_zoom_circuit;

void fude_zoom_circuit_init(fude_zoom_circuit* _c);
void fude_zoom_circuit_destroy(fude_zoom_circuit* _c);
// Worked out again from what is on the canvas (every part and wire alive and shown; their places in the home
// frame), keeping each part's state where it is still there (a capacitor's charge, a flip-flop's Q). How many parts.
u32  fude_zoom_circuit_build(fude_zoom_circuit* _c, const fude_zoom_scene* _s);
// ...only what _scope marks (a byte an object: its parts; the wires it marks, those joined to its parts, and those
// joined to such wires). NULL: all.
u32  fude_zoom_circuit_build_in(fude_zoom_circuit* _c, const fude_zoom_scene* _s, const u8* _scope);
// Back to the start: no time, nothing charged.
void fude_zoom_circuit_reset(fude_zoom_circuit* _c);
// Solved now as it stands (DC: capacitors open, inductors shorted; what is stepped as it is). False: not solved.
b8   fude_zoom_circuit_dc(fude_zoom_circuit* _c);
// Something changed at once (a switch, a button pressed, a part added): the next step backward Euler, the steps short.
void fude_zoom_circuit_jump(fude_zoom_circuit* _c);
// What part _p holds beyond its state (NULL: nothing): a display's lights (f32 each, 0–1.5 as seen), an LCD's controller.
u8*       fude_zoom_circuit_store(fude_zoom_circuit* _c, const fude_zoom_circuit_part* _p);
const u8* fude_zoom_circuit_store_of(const fude_zoom_circuit* _c, const fude_zoom_circuit_part* _p);
// Node _node's voltage as last solved (ground, none, or no such node: 0).
f64  fude_zoom_circuit_volts(const fude_zoom_circuit* _c, u32 _node);
// _dt seconds on (in steps of at most its step). False: a step not solved.
b8   fude_zoom_circuit_run(fude_zoom_circuit* _c, f64 _dt, u32 _max_steps);
// _span seconds on in steps as fine as what happens needs (above: step_now), _max_steps of them at most (short of
// them: less far — slow motion), its wires' currents worked out after or not. False: a step not solved.
b8   fude_zoom_circuit_advance(fude_zoom_circuit* _c, f64 _span, u32 _max_steps, b8 _wires);
// _steps of its step on, its wires' currents worked out after them or not (_wires: as run does). False: a step not solved.
b8   fude_zoom_circuit_steps(fude_zoom_circuit* _c, u32 _steps, b8 _wires);

// A MOTOR (its text: its rated volts, and its speed at them — "6V 60rpm"; 60 rpm unless it says). Its winding's
// resistance (ohms); its back-EMF's volts a radian a second.
#define FUDE_ZOOM_MOTOR_R 8.0
f64  fude_zoom_motor_k(const fude_zoom_circuit_part* _motor);
// A shafted motor (coupling.h) as the circuit loads it now, near where it is: how much more current through it (amperes,
// pin 0 to pin 1) a volt less of its back-EMF makes (≥ 0: 0, nothing joined to it), and the back-EMF at which none would
// flow (*_still, volts). As the circuit last stepped; the circuit left as it was. False: not a shafted motor, or not
// solved.
b8   fude_zoom_circuit_motor_load(fude_zoom_circuit* _c, u32 _part, f64* _conductance, f64* _still);

// A SOLENOID (its text: its rated volts, "12V"): a coil — its resistance its rated volts over half an ampere, its
// inductance its resistance's 2 ms — and a plunger out of it, pulled in as hard as the square of its current (its rated
// pull at its rated current halfway in: more as it is further in, less further out) against a light spring. Alone it
// goes in when its pull beats the spring and out when it does not — in from all the way out from 87% of its rated
// current, out again below 57% of it (held in, it holds on with less): a solenoid's, a relay's. On a mechanism (coupling.h) its plunger
// is the mechanism's: it pulls what is pinned on its end (its hole), pushes what is in its way, presses a button.
// state: 0 its current (pin 0 to pin 1), 1 the voltage over it, 2 how far in its plunger is (0 out, 1 in), 3 on a
// mechanism (1) or not.
#define FUDE_ZOOM_SOLENOID_STROKE 0.3    // its plunger's travel, of its half width
#define FUDE_ZOOM_SOLENOID_TIP    0.85   // its plunger's end out (its hole: of its half width from its middle)
#define FUDE_ZOOM_SOLENOID_SPRING 0.45   // its spring's push, of its rated pull
f64  fude_zoom_circuit_solenoid_pull(const fude_zoom_circuit_part* _p);
// A UNIPOLAR STEPPER MOTOR (a 28BYJ-48: its text "28BYJ-48" — 2048 steps a turn of its geared shaft —, or how many,
// "200 steps"): four coils from COM to A, B, C, D (50 Ω each), each a quarter of its rotor's electrical turn on from the
// one before; its rotor drawn toward where their currents together point, as far as the nearest way there is (it
// follows coils energised in turn — A, B, C, D: a step each on —, two at a time, half steps; too fast, or the
// opposite coil, and it is lost), as fast as its rotor can (500 steps a second). Its shaft four steps a turn of its
// rotor's field. On a mechanism, what is on its shaft turned there as hard as its holding torque, and holding it there
// (pushed past half a turn of its field, it slips a step). state: 0 its rotor's electrical angle (radians), 1 its
// shaft's (degrees), 2 its steps (full ones, from where it began), 3–6 its coils' currents (into each from COM), 7 on a
// mechanism's shaft (≥ 0) or not (−1).
#define FUDE_ZOOM_STEPPER_COIL  50.0    // ohms
#define FUDE_ZOOM_STEPPER_RATE  500.0   // full steps a second, at most
// Where its coils' currents point (radians: A's way 0, B's a quarter turn on…), and how strongly, of one coil's at 5 V.
f64  fude_zoom_circuit_stepper_field(const fude_zoom_circuit_part* _p, f64* _strength);
// A ROTARY ENCODER (its text: its detents a turn, "20"): A and B each a contact to C, closed in turn as its shaft turns
// (quadrature) — a detent all the way through each's closing and opening, A a quarter of it before B turned clockwise,
// both open at a detent. Alone a tap turns it a detent clockwise; on a mechanism, as what is on its shaft turns.
// state: 0 where it is (detents, clockwise), 1 where a tap sends it, 6 where its contacts were last stepped, 7 on a
// mechanism's shaft (≥ 0) or not (−1).
void fude_zoom_circuit_encoder_contacts(f64 _detents, b8* _a, b8* _b);
// A SLOTTED OPTICAL SENSOR (a photo-interrupter): A and K its infrared LED's (1.2 V), C and E its phototransistor's —
// conducting a tenth of the LED's current while its slot is clear, nothing while something is in it (a mechanism's
// part going through its slot; alone, a tap: a hand in it). state: 0 blocked (1) or clear (0).
#define FUDE_ZOOM_SLOT_CTR 0.1   // its phototransistor's current over its LED's, the slot clear
// THYRISTORS. An SCR (A, K, G): off — nothing through it but a leak, up to its breakover (its voltage limit) — until
// its gate takes its trigger current with its anode over its cathode; on — a diode from anode to cathode (a volt) —
// until what goes through it falls under its holding current. A TRIAC (MT2, MT1, G): the same either way round,
// triggered by its gate's current either way — on AC, off at each crossing, on again when the gate fires it.
// state: 0 on (1) or off (0). Their trigger and holding currents: value 0 and 1.
// A TRANSFORMER (P1, P2, S1, S2; its text its turns' ratio, "2:1"): two windings coupled (0.995) — its primary 10 H, its
// secondary as many turns fewer squared; 1 Ω in its primary, as much less in its secondary — so on AC its secondary has
// its primary's volts over its ratio, a load's current back in its primary over it; on DC, only its windings'
// resistance. state: 0, 1 its windings' currents, 2, 3 their voltages (the last step's).
// A BRIDGE RECTIFIER (AC, +, AC, −): four diodes — whichever AC pin is higher into +, − out of whichever is lower.
// An OPTOCOUPLER (A, K, C, E: a PC817): its LED's light on its phototransistor — C to E a current its CTR times its
// LED's (1), less as C nears E. A PHOTODIODE (A, K): a diode, and against it a current as its light (50 nA a lux).
// A PHOTOTRANSISTOR (C, E): as much current as its light (2 µA a lux), less as C nears E. Their light: state 0, lux
// (as an LDR's: its text's, a tap's, Play's slider).
#define FUDE_ZOOM_PHOTODIODE_A_LUX       50e-9
#define FUDE_ZOOM_PHOTOTRANSISTOR_A_LUX  2e-6
// Whether a part's model is lit (an LDR, a photodiode, a phototransistor: state 0 its light, lux).
b8   fude_zoom_circuit_lit(u8 _model);
// A SPEAKER (8 Ω, or its text's): the current through it its cone's push. Its sound, while the circuit makes it
// (audio_rate above 0): the speakers' currents together, a sample every 1/audio_rate seconds of the circuit's time,
// 0.1 A full scale, the slow average (under 20 Hz) taken out — the page plays them.
// Sound made at _rate samples a second from now (0: none; the samples made so far let go).
void fude_zoom_circuit_sound(fude_zoom_circuit* _c, f64 _rate);
// A part's state toggled by a tap (a switch, a button pressed or let go, a logic input, a pot's wiper on a step, a
// board's pin through input, high, low, blink; an encoder a detent on, a slotted sensor's slot blocked or clear): what
// it says now into _say (its text, as the part keeps it). False: nothing to toggle there.
// A contact (a switch's, a button's, a changeover's, a relay's) arcs when it opens on a coil's current with no diode to
// carry it: FUDE_ZOOM_ARC volts over it. When one last did (circuit seconds; at most 0: never) — its state[7].
#define FUDE_ZOOM_ARC 50.0
f64  fude_zoom_circuit_sparked(const fude_zoom_circuit_part* _p);
b8   fude_zoom_circuit_tap(fude_zoom_circuit* _c, u32 _part, i32 _pin, c8* _say, usize _size);
// An oscilloscope's knob _knob (display.h's) turned a step _way (+1: clockwise, -1) as it plays: its sweep started again
// when its time a division changes (and the circuit's steps no longer than its samples are apart); what it is set to now
// into _say ("CH1 VOLTS/DIV 0.5 V"). False: not an oscilloscope's knob.
b8   fude_zoom_circuit_scope_turn(fude_zoom_circuit* _c, u32 _part, u32 _knob, i32 _way, c8* _say, usize _size);
// How high Play's tags' letters are (a node's volts, a meter's reading, a logic input's 0 or 1: screen points), for
// parts whose smaller half-sizes on the screen are _sizes (sorted here): as big as the parts look — half the typical
// one's (their median; a part not seen, no size or none finite, not counted) —, at most _most; 0 (none drawn) under
// _least, as a part's pin names are not.
f64  fude_zoom_circuit_tag_px(f64* _sizes, u32 _count, f64 _most, f64 _least);

#endif
