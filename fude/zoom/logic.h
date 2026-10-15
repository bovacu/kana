// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_LOGIC_H
#define FUDE_ZOOM_LOGIC_H

#include "rde.h"
#include "sim/sim.h"
#include "zoom/circuit.h"

// ===========================================================================
// Logic on the canvas (Electronics; docs/simulation_architecture.md §5–6): the canvas's logic parts (gates,
// flip-flops, logic inputs and probes, chips, custom parts) compiled into one netlist of the simulation's (sim/) and
// run by its digital engine, so that nothing of them is worked out twice.
//
// A node joined only to logic pins becomes one of its nets, so a latch of two gates drawn on the canvas holds as a
// latch does. Where a logic pin meets the analog circuit (an LED, a resistor, a breadboard's rail, a battery) there is
// a BRIDGE. An output bridge is a voltage source: its supply's high or its ground, through the output's resistance
// (FUDE_ZOOM_LOGIC_R), and nothing while the output is released or unknown. An input bridge reads the node's voltage
// against half its supply: a chip's supply is its VCC pin over its GND pin; a gate's or a custom part's is
// FUDE_ZOOM_LOGIC_V. An unpowered input (less than 1 V of supply) reads low.
//
// CHIPS and CUSTOM PARTS are definitions of the simulation's (sim/chips.c; a person's own in My parts, which a canvas
// finds them in: it keeps no copies of its own). Their part on the canvas is made from the definition: its pins are
// its ports, in order. A chip's go round its package (pin 1 at its top left, down its left, then up its right); a
// custom part's go round a block (inputs on the left, outputs on the right).
//
// Where it sits: `fude/zoom`, the canvas layer, above `fude/sim`. The circuit builds a fude_zoom_logic with each
// circuit (circuit_build.c) and steps it with each solve (circuit_solve.c), and page_custom.c makes, learns, renames
// and draws custom parts with it.
// Main types and lifecycle: the library of definitions (fude_zoom_logic_library) and the parts made from them
// (fude_zoom_logic_part) belong to this module: made on first use and kept for as long as the app runs, because
// circuits hold pointers to them. A fude_zoom_logic is the caller's struct (the circuit keeps one), set up by
// fude_zoom_logic_init and released by fude_zoom_logic_destroy; it owns its netlist, flat circuit and run.
// Threads: main thread only. The library and the list of parts are shared and not locked.
// Design: docs/infinite_canvas_design.md §35.
// ===========================================================================

#define FUDE_ZOOM_LOGIC_V 5.0    // a level's volts where a part has no supply pins
#define FUDE_ZOOM_LOGIC_R 25.0   // an output's resistance (ohms)

// --- the library ---------------------------------------------------------------------------------

// Returns the library the canvas's parts come from: the simulation's models, the chips, and custom parts as they are
// learnt. It is made on first use and kept for as long as the app runs.
fude_sim_lib*       fude_zoom_logic_library(void);
// Reads a custom part's text and puts it in the library, in place of one with the same id (as a newer version; the
// one it replaces is freed, so pointers to it must not be kept). Returns its definition, owned by the library, or NULL
// when the text could not be read (_error says why).
const fude_sim_def* fude_zoom_logic_learn(const c8* _text, usize _size, c8* _error, usize _error_size);
// Returns the library's revision: a count that goes up whenever a custom part is learnt, renamed or forgotten, so
// what a cache worked out from which part a drawn symbol is (circuit.h's fude_zoom_part_of) knows it is stale. It
// starts at 0 and only goes up.
u32                 fude_zoom_logic_revision(void);
// Returns the definition a part's text names: by its id, by its name (the text's first line), or by a custom part's
// first name (for one renamed since, whose id was made from that name). NULL when there is none.
const fude_sim_def* fude_zoom_logic_find(const c8* _text);
// Writes a name as a custom part's id (and its files' names) are made of it: lowercase letters and digits, with a
// dash for each run of anything else ("part" when nothing is left).
void fude_zoom_logic_slug(const c8* _name, c8* _out, usize _size);
// Renames custom part _id. Its id is kept, so what is on canvases under its old name still finds it. Returns false when
// there is no such part (or _name is empty).
b8   fude_zoom_logic_rename(const c8* _id, const c8* _name);
// Forgets custom part _id ("user/…" only): it can no longer be found, but what was made of it before still holds it.
// Returns false when there is no such custom part.
b8   fude_zoom_logic_forget_part(const c8* _id);
// Puts _new's ports in _old's order, by their names (ports new to it go after them, as they were), so that when a part
// is made again, what is wired to its pins on canvases stays on the same pins.
void fude_zoom_logic_keep_order(fude_sim_def* _new, const fude_sim_def* _old);

// Returns the part that definition _def is on the canvas, looking as _template says (its look, its model), its pins
// (one for each of the definition's ports, however many) round a package (_package: a chip's) or round a block; a
// circuit joins every one of them (circuit.h's node). Each is made once for each version of the definition and kept
// for as long as the app runs. NULL when there is no template or no definition.
const fude_zoom_part* fude_zoom_logic_part(const fude_zoom_part* _template, const fude_sim_def* _def, b8 _package);

// --- a circuit's logic ---------------------------------------------------------------------------

// Which way a bridge works.
typedef enum {
    FUDE_ZOOM_BRIDGE_IN = 0,     // the node's voltage is read as a level into the logic
    FUDE_ZOOM_BRIDGE_OUT,        // the logic's level comes out as a source on the node
    FUDE_ZOOM_BRIDGE_INOUT       // both (a bus transceiver's pin)
} FUDE_ZOOM_BRIDGE_;

// Where a logic pin meets the analog circuit.
typedef struct {
    u32 part, pin;               // the circuit's part, and its pin
    u32 node;                    // the circuit's node it is on
    // The pin of its part that its levels are measured from (its GND); FUDE_ZOOM_NONE: the circuit's ground.
    u32 ground;                  // its part's GND pin; FUDE_ZOOM_NONE: the circuit's ground
    u32 supply;                  // its part's VCC pin; FUDE_ZOOM_NONE: FUDE_ZOOM_LOGIC_V over its ground
    u8  way;                     // a FUDE_ZOOM_BRIDGE_
    u8  read;                    // the level it last read (an input)
    u8  drive;                   // the level it drives (an output): 0 or 1; Z or X drive nothing
    u32 input;                   // its input primitive in the flat circuit (an input), or FUDE_SIM_NONE
    u32 net;                     // its pin's net in the flat circuit
} fude_zoom_bridge;

// A circuit's logic, compiled and running.
typedef struct fude_zoom_logic {
    fude_sim_def*                  netlist;      // the circuit's logic as one composite (owned)
    fude_sim_flat                  flat;
    fude_sim_run                   run;
    b8                             on;           // there is some logic, and it was made
    rde_arr TYPE(fude_zoom_bridge) bridges;
    rde_arr TYPE(u32)              part_bridges; // for each circuit part, two numbers: its first bridge, and how many
    rde_arr TYPE(u32)              node_net;     // each circuit node's net, where only logic pins are on it; else NONE
    rde_arr TYPE(u32)              part_input;   // each circuit part's input primitive (a logic input's); else NONE
    rde_arr TYPE(u32)              part_probe;   // each circuit part's first pin's net (a probe's); else NONE
    rde_arr TYPE(u8)               part_level;   // each logic input's level, as last set
    rde_str                        errors;       // why it could not be made ("" when it was)
} fude_zoom_logic;

// Sets up an empty logic, and frees what one holds.
void fude_zoom_logic_init(fude_zoom_logic* _l);
void fude_zoom_logic_destroy(fude_zoom_logic* _l);
// Lets go of what it held, so that it is made next from the start (a circuit reset) and nothing is carried over.
void fude_zoom_logic_forget(fude_zoom_logic* _l);
// Returns whether the logic runs a part: a gate, a flip-flop, a logic input or probe, a chip, a custom part.
b8   fude_zoom_logic_is(const fude_zoom_part* _part);
// Makes it from circuit _c's parts as they are built (their pins' nodes): its netlist and bridges. What it held before
// (a flip-flop's Q, a latch's state) is carried over, and it is settled. When it cannot be made, `on` stays false and
// `errors` says why.
void fude_zoom_logic_build(fude_zoom_logic* _l, const fude_zoom_circuit* _c);
// After the circuit is solved (its nodes' voltages in _c->v): runs the logic on to time _t, reads each logic input's
// switch and each input bridge's level, and settles it. Returns true when what an output bridge drives has changed,
// so the circuit must be solved again; false when nothing changed or there is no logic.
b8   fude_zoom_logic_step(fude_zoom_logic* _l, const fude_zoom_circuit* _c, f64 _t);
// Returns a node's level where only logic is on it (0, 1, Z or X); 0xFF when it is not such a node.
u8   fude_zoom_logic_node(const fude_zoom_logic* _l, u32 _node);
// Returns a logic probe's level (its pin's net's); 0xFF when the part is not a probe.
u8   fude_zoom_logic_probe(const fude_zoom_logic* _l, u32 _part);
// Returns how many bridges part _part has, and sets *_first to the first one's index in `bridges`.
u32  fude_zoom_logic_bridges_of(const fude_zoom_logic* _l, u32 _part, u32* _first);

// --- custom parts made of what is drawn ---------------------------------------------------------

// Why a custom part could not be made.
typedef enum {
    FUDE_ZOOM_MAKE_OK = 0,
    FUDE_ZOOM_MAKE_NOTHING,      // no logic part to make one of
    FUDE_ZOOM_MAKE_NO_PORTS      // no logic input or probe to be its inputs and outputs
} FUDE_ZOOM_MAKE_;

// Makes a custom part, with id _id and name _name, of circuit _c's logic (built from what a lasso holds of scene _s).
// Its logic inputs are its inputs and its probes its outputs, named by their texts (else IN1…, OUT1…), each side in
// order from the top (left to right where they are level); the rest are its parts, joined as they are drawn. Returns
// the new definition, which the caller owns, or NULL when there is nothing to make (*_problem says why).
fude_sim_def* fude_zoom_logic_make(const fude_zoom_circuit* _c, const fude_zoom_scene* _s, const c8* _id, const c8* _name, u32* _problem);

// --- a custom part's inside, drawn ----------------------------------------------------------------------

// The placer that fude_zoom_logic_draw takes (placer.h).
struct fude_zoom_placer;
// Draws custom part _def's inside from its definition, for when the drawing it was made of is not kept. Its inputs
// become logic inputs down the left and its outputs probes down the right (each named as its port); its parts go in
// columns as far along from the inputs as they are (gates, with a gate of more than two inputs as a chain of two-input
// ones; flip-flops; chips; custom parts); every net is wired from what drives it to what it drives; and its name goes
// over it. Everything is made with placer _p, whose middle is the drawing's. Made into a part again
// (fude_zoom_logic_make), the drawing does what _def does. Returns how many things it made.
u32 fude_zoom_logic_draw(const fude_sim_def* _def, struct fude_zoom_placer* _p);

#endif
