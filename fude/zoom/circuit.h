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
    f64                   value[4];     // its numbers as its text says (a resistance, a voltage, a frequency...)
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
    u32                   block;        // its pins' block of unknowns (below; FUDE_ZOOM_NONE: on ground or nothing only)
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
    FUDE_ZOOM_CIRCUIT_OVER         // a source past its current (a short circuit, or near one)
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
    rde_arr TYPE(u32) block_reference;     // a block ground reaches nowhere: its node held at 0 V (else FUDE_ZOOM_NONE)
    rde_arr TYPE(u64) joins;               // each a row's node << 32 | a column's
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
// A part's state toggled by a tap (a switch, a button pressed or let go, a logic input, a pot's wiper on a step, a
// board's pin through input, high, low, blink): what it says now into _say (its text, as the part keeps it). False:
// nothing to toggle there.
b8   fude_zoom_circuit_tap(fude_zoom_circuit* _c, u32 _part, i32 _pin, c8* _say, usize _size);
// How high Play's tags' letters are (a node's volts, a meter's reading, a logic input's 0 or 1: screen points), for
// parts whose smaller half-sizes on the screen are _sizes (sorted here): as big as the parts look — half the typical
// one's (their median; a part not seen, no size or none finite, not counted) —, at most _most; 0 (none drawn) under
// _least, as a part's pin names are not.
f64  fude_zoom_circuit_tag_px(f64* _sizes, u32 _count, f64 _most, f64 _least);

#endif
