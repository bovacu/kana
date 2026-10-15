// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_CIRCUIT_PARTS
#define FUDE_ZOOM_CIRCUIT_PARTS

#include "zoom/circuit.h"
#include "zoom/symbol.h"
#include "zoom/shape.h"
#include "zoom/logic.h"
#include "zoom/props.h"
#include "zoom/limits.h"
#include "zoom/display.h"

// ===========================================================================
// Circuit parts: the circuit's private interface. The circuit (circuit.h) is written as circuit.c and the
// circuit_*.c modules; this is what they share with each other: the functions one module calls in another, and the
// constants and types more than one needs. Included by circuit.c and circuit_*.c only, never anywhere else: everything
// outside the circuit uses circuit.h.
//
// Where it sits: fude/zoom, under circuit.h (which it includes, with every header the circuit uses). The modules
// include this header and nothing else.
// Its main types and their lifecycle: the circuit (fude_zoom_circuit, circuit.h), set up by fude_zoom_circuit_init and
// freed by fude_zoom_circuit_destroy (circuit.c); a solve's context (fzc_ctx), made on the stack for each solve.
// Threads: the main thread only, as the circuit itself.
// Files: none.
// Design: circuit.c was one file of some 6,000 lines; it was split by what each part does, its code moved unchanged,
// and then its five functions of over 150 lines were broken into steps by extraction alone (fude_zoom_part_draw into
// fzc_draw_*, fzc_stamp into fzc_stamp_*, fzc_read into fzc_read_*, fzc_blocks into fzc_blocks_*,
// fude_zoom_circuit_build_in into fzc_build_*). A function one module calls in another is declared here (not
// RDE_INTERNAL); what one module alone uses stays RDE_INTERNAL in it. Every internal name keeps circuit.c's prefix,
// fzc_ and FZC_.
//
// Which module holds what:
//   circuit.c            the coordinator: a circuit set up and freed, built, reset, run on in time; its store, its
//                        volts
//   circuit_catalogue.c  the parts: the catalogue, a breadboard's holes, the part an object is, its pins where they are
//   circuit_draw.c       each part drawn (fude_zoom_part_draw)
//   circuit_read.c       a part's text read: values, words, LED colours, the sensors' curves, each model's numbers
//   circuit_wires.c      wires: their numbers, their routes, following their parts, those left joined to nothing; a
//                        grid of segments (fzc_seg_grid)
//   circuit_build.c      the circuit from the canvas (fude_zoom_circuit_build_in): terminals, nodes, vertices
//   circuit_blocks.c     the unknowns: rails held, pins that sense, blocks and their references
//   circuit_stamp.c      each part's elements stamped into its block's system; the non-linear devices
//   circuit_solve.c      the solve: systems shaped, Newton's method, the wires' currents, quiet blocks, a solve at a
//                        time
//   circuit_step.c       what is stepped, and after a step: what parts show and keep, their limits, the step's size,
//                        sound
//   circuit_play.c       what Play asks: an oscilloscope's knob, a motor's load, the tags' size, a tap
// ===========================================================================

// --- the constants and types the modules share (as circuit.c had them) -------------------------------------------

// How a part is drawn.
typedef enum {
    FZC_RESISTOR = 0, FZC_POT, FZC_CAP, FZC_CAP_POL, FZC_INDUCTOR, FZC_DIODE, FZC_LED, FZC_ZENER,
    FZC_NPN, FZC_PNP, FZC_NMOS, FZC_PMOS, FZC_VSOURCE, FZC_BATTERY, FZC_ACSOURCE, FZC_CLOCK, FZC_ISOURCE,
    FZC_GROUND, FZC_RAIL, FZC_SWITCH, FZC_BUTTON, FZC_LAMP, FZC_MOTOR, FZC_BUZZER, FZC_FUSE, FZC_OPAMP,
    FZC_VOLTMETER, FZC_AMMETER, FZC_SEVEN_SEG, FZC_GATE, FZC_FLIPFLOP, FZC_LOGIC_IN, FZC_LOGIC_OUT,
    FZC_CHIP, FZC_MODULE, FZC_REG3, FZC_BOARD, FZC_BREADBOARD, FZC_BLOCK,
    FZC_SEG_PANEL, FZC_MATRIX, FZC_BAR_GRAPH, FZC_METER, FZC_CHAR_LCD, FZC_SCOPE, FZC_SPDT, FZC_DPDT, FZC_LDR, FZC_NTC,
    FZC_SOLENOID, FZC_STEPPER, FZC_ENCODER, FZC_SLOT,
    FZC_SCR, FZC_TRIAC, FZC_XFMR, FZC_BRIDGE, FZC_OPTO, FZC_PHOTODIODE, FZC_PHOTOTRANS, FZC_SPEAKER
} FZC_LOOK_;

// A breadboard's size, in holes (circuit_catalogue.c's FZC_BB_ROW has its rows' heights).
#define FZC_BB_H 18.5f   // A breadboard's height, in holes.
#define FZC_BB_W 33.0f   // Its width, in holes.

// The solve's constants, and a standalone logic part's levels.
#define FZC_GMIN      1e-12       // siemens added to each unknown's diagonal, so a node joined to nothing still solves
#define FZC_VT        0.025852    // the thermal voltage at about 27 °C (volts): a junction's kT/q
#define FZC_LOGIC_V   5.0         // a standalone gate's, flip-flop's, logic input's high
#define FZC_LOGIC_R   25.0        // ...and its output's resistance

// A part's pins. Every pin of a part is worked with, however many it has (circuit.h: its pin_room places in the
// circuit's pin_nodes and pin_amps). FZC_PIN_ROOM is the room every part has at least, so that a model reading a pin by
// its number past its part's own (a catalogue part's of fewer pins than its model's) reads one joined to nothing.
// FZC_PIN_BITS is a u64's bits: a board's driven pins (a board is the catalogue's, of fewer pins) and a part's pins
// that only sense are a bit each, so only a part's first 64 pins can be either (fzc_senses).
#define FZC_PIN_ROOM  64u
#define FZC_PIN_BITS  64u

// Returns whether part _p's pin _k only senses (circuit.h: its sense); a pin past FZC_PIN_BITS never does.
static inline b8 fzc_senses(const fude_zoom_circuit_part* _p, u32 _k) {
    return _k < FZC_PIN_BITS && ((_p->sense >> _k) & 1ull) != 0u;
}

// A part's key's fixed numbers (fzc_key): its numbers (8), its state (8), spin, pushed, switches and burnt; its pins'
// volts follow them.
#define FZC_KEY_FIXED 20u

// One solve's context: the circuit, and the step being taken.
typedef struct {
    fude_zoom_circuit* c;
    u32                n;      // unknowns (nodes less ground)
    b8                 tran;   // stepping in time (capacitors and inductors by their companions)
    f64                dt;     // the step (seconds)
    f64                t;      // the time at the step's end (seconds)
    b8                 be;     // this step by backward Euler (after a jump), not the trapezoidal rule
} fzc_ctx;

// A diode: is·(e^(v/nvt) − 1) — its exponent counted from _shift volts (an LED's: its forward voltage, its is 10 mA
// there), so that it is as steep as it is at what it conducts at however high that is (fzc_exp straightens past e^40:
// from 0 V, that is 2 V for an LED's nvt — a blue one's 3 V would barely conduct).
typedef struct { f64 is, nvt, zener, shift; } fzc_diode_k;

// A contact between pins _a and _b: closed, _g; open, nothing — until the voltage over it reaches FUDE_ZOOM_ARC (a
// coil's current stopped with no diode for it): then it arcs, carrying that current on.
#define FZC_ARC FUDE_ZOOM_ARC   // The voltage an open contact arcs over at (circuit.h).

// The steps' sizes. A step's change at most: how far a capacitor's curve bends (FZC_STEP_DV volts, and 2% of them),
// how far it goes (FZC_STEP_DV_MOST volts, and 10% of them), an inductor's amperes (FZC_STEP_DI, and 2% of them).
// The first step is FZC_STEP_FIRST (and the next after a jump at least that), growing at most FZC_STEP_GROW times at
// a time.
#define FZC_STEP_DV    0.05    // volts
#define FZC_STEP_DV_MOST 0.5   // volts
#define FZC_STEP_DI    1e-3    // amperes
#define FZC_STEP_LEAST 1e-7    // a step's least (seconds)
#define FZC_STEP_FIRST 1e-6    // seconds
#define FZC_STEP_GROW  4.0

// --- circuit_read.c: a part's text read ------------------------------------------------------------------------------

// Returns whether two names are the same in any case (ASCII).
b8 fzc_same_name(const c8* _a, const c8* _b);
// Returns what a part holds beyond its state, in bytes (eight at a time): a display's lights as seen,
// an LCD's controller, an oscilloscope's sweeps.
u32 fzc_store_size(const fude_zoom_part* _part);
// Reads a part's numbers (value[]) from its text, with each kind's defaults; _fresh: its state's starting values,
// its switches and drives too (it is new to the circuit, or reset).
void fzc_read(fude_zoom_circuit_part* _p, const c8* _text, b8 _fresh);
// Writes a symbol's text (read from all its numbers, not only the first FUDE_ZOOM_SHAPE_NUMBERS) into _out.
void fzc_text_of(const fude_zoom_scene* _s, u32 _object, c8* _out, usize _size);

// --- circuit_wires.c: a grid of segments -----------------------------------------------------------------------------

// A segment (home units), what it belongs to (its owner: the caller's, such as a wire's index or a pin's terminal), and
// its reach: how near a point lies on it.
typedef struct {
    u32          owner;
    fude_zoom_v2 a, b;
    f64          tol;
} fzc_seg;

// A segment in a grid cell (a cell's key: its two coordinates' 32 bits side by side).
typedef struct {
    u64 cell;
    u32 seg;
} fzc_cell_seg;

// A grid of segments, so that what lies within reach of a point is found by looking in that point's cell alone, not
// at every segment (circuit_wires.c says how it is laid out): its cells' size; each cell's segments, sorted by cell;
// each cell's first among them; the segments too long for cells, tried for every point.
typedef struct {
    f64                        cell;
    rde_arr TYPE(fzc_cell_seg) in;
    rde_hash_map TYPE(u64, u32) first;
    rde_arr TYPE(u32)          wide;
} fzc_seg_grid;

// Makes grid _g of the _n segments _segs (it keeps their indices: the array may change after, its segments not);
// fzc_grid_free lets it go.
void fzc_grid_make(fzc_seg_grid* _g, const fzc_seg* _segs, u32 _n);
void fzc_grid_free(fzc_seg_grid* _g);
// Writes into _out (an rde_arr of u32, cleared first) the indices of the segments that may lie within reach of point
// _at, each once, in ascending order; every segment within its reach of _at is among them. The caller measures.
void fzc_grid_near(const fzc_seg_grid* _g, fude_zoom_v2 _at, rde_arr* _out);

// --- circuit_build.c: the circuit from the canvas --------------------------------------------------------------------

// Works out the circuit's longest step (step) from its parts: a millisecond, finer for what changes faster.
void fzc_step_most(fude_zoom_circuit* _c);

// --- circuit_blocks.c: the unknowns, rails held, blocks --------------------------------------------------------------

// Lets its blocks' sparse systems go, and empties their list.
void fzc_systems_free(fude_zoom_circuit* _c);
// The rails' nodes held (known, at their volts), each other pin on one, and each part's pins that sense.
void fzc_held(fude_zoom_circuit* _c);
// Its unknowns in blocks: each node's block and slot, each part's block, each block's system (to be shaped) and
// reference, what is live, and where each part's key is.
void fzc_blocks(fude_zoom_circuit* _c);

// --- circuit_stamp.c: the stamps, the non-linear devices -------------------------------------------------------------

// Returns part _p's pin _pin's node (0: ground; FUDE_ZOOM_NONE: past its pin_room, or joined to nothing).
u32 fzc_node(const fude_zoom_circuit_part* _p, u32 _pin);
// Returns a pin's voltage now.
f64 fzc_pv(const fzc_ctx* _x, const fude_zoom_circuit_part* _p, u32 _pin);
// Returns the place of the system's entry at node _row's row, node _col's column; a place no one reads for a
// node a rail holds or ground, across two blocks, or where its block is not shaped yet (each noted).
f64* fzc_entry(fude_zoom_circuit* _c, u32 _row, u32 _col);
// A diode's currents (A, K) at voltages _v into _i; _user: its fzc_diode_k.
void fzc_diode_fn(const f64* _v, f64* _i, const any _user);
// Returns a pin's voltage over another's.
f64 fzc_over(const fzc_ctx* _x, const fude_zoom_circuit_part* _p, u32 _a, u32 _b);
// Returns the volts of a board's supply pin named _name (0: not one): 5V, VBUS and VSYS 5 V, 3V3 3.3 V, and IOREF the
// board's logic volts (_logic: the part's a; 5 V when it has none), as on an Uno, whose IOREF says its pins' level.
f64 fzc_supply(const c8* _name, f64 _logic);
// Returns a clock's part of each period high (its pulse over its period; none given: half).
f64 fzc_duty(const fude_zoom_circuit_part* _p);
// Returns where in its period a clock is at _t (0 .. 1), a whisker on.
f64 fzc_clock_phase(const fude_zoom_circuit_part* _p, f64 _t);
// Returns whether both of a two-pin part's ends are known (ground, or held).
b8 fzc_pinned(const fude_zoom_circuit* _c, const fude_zoom_circuit_part* _p);
// Stamps part _p into its block's system for the step: each model's elements (a burnt part open but for an arc).
void fzc_stamp(fzc_ctx* _x, fude_zoom_circuit_part* _p);

// --- circuit_solve.c: the solve, quiet blocks, a solve at a time -----------------------------------------------------

// Solves until it settles (Newton's method), its blocks made again whenever a stamp joins two; _only: that block
// alone (FUDE_ZOOM_NONE: all). False: not solved.
b8 fzc_newton(fzc_ctx* _x, u32 _only);
// Sizes the system's scratch for _n unknowns and the circuit's blocks.
void fzc_room(fude_zoom_circuit* _c, u32 _n);
// Works out each wire piece's current from the parts' pins' currents, along a tree of the wires.
void fzc_wire_currents(fude_zoom_circuit* _c);
// Solves the circuit at its time plus _dt (_tran: a step of time; else DC, as it stands), the logic settled within
// it, what is stepped worked out, the quiet blocks left. False: not solved (time moves on only when it was).
b8 fzc_at(fude_zoom_circuit* _c, b8 _tran, f64 _dt);

// --- circuit_step.c: what is stepped, and after a step ---------------------------------------------------------------

// Works out what is stepped (logic, a 555, a fuse) from the voltages now. True: something changed.
b8 fzc_digital(fzc_ctx* _x, fude_zoom_circuit_part* _p);
// Returns how long until something is about to happen (seconds): a clock's next edge, a 555's threshold, an AC
// source's fortieth of a period.
f64 fzc_breakpoint(const fude_zoom_circuit* _c);
// After a step: what the parts show and remember, their limits (stepping), the next step's size, the sound.
void fzc_after(fzc_ctx* _x);

#endif
