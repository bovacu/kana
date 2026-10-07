// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_LOGIC_H
#define FUDE_ZOOM_LOGIC_H

#include "rde.h"
#include "sim/sim.h"
#include "zoom/circuit.h"

// ===========================================================================
// LOGIC ON THE CANVAS (Electronics; docs/simulation_architecture.md §5–6):
// the canvas's logic parts — gates, flip-flops, logic inputs and probes,
// chips, custom parts — compiled into one netlist of the simulation's (sim/)
// and run by its digital engine, nothing of it worked out twice.
//
// A node joined only to logic pins is one of its nets: a latch of two gates
// drawn on the canvas holds as a latch does. Where a logic pin meets the
// analog circuit (an LED, a resistor, a breadboard's rail, a battery) there is
// a BRIDGE: an output's a voltage source (its supply's high or its ground,
// through its output's resistance; nothing while it is released or unknown),
// an input's the node's voltage against half its supply (a chip's supply: its
// VCC pin over its GND pin; a gate's or a custom part's: FUDE_ZOOM_LOGIC_V).
//
// CHIPS and CUSTOM PARTS are definitions of the simulation's (sim/chips.c, a
// person's own: My parts, the canvas's): their part on the canvas is made from
// the definition — its pins its ports, in order (a chip's in its package's
// order: pin 1 at its top left, down, then up its right; a custom part's round
// a block: inputs left, outputs right).
// ===========================================================================

#define FUDE_ZOOM_LOGIC_V 5.0    // a level's volts where a part has no supply pins
#define FUDE_ZOOM_LOGIC_R 25.0   // an output's resistance (ohms)

// --- the library ---------------------------------------------------------------------------------

// The library the canvas's parts come from (made on first use): the simulation's models, the chips, and custom parts
// as they are learnt.
fude_sim_lib*       fude_zoom_logic_library(void);
// A custom part's text learnt (a newer version in place of one with its id). Its definition; NULL: not read (_error).
const fude_sim_def* fude_zoom_logic_learn(const c8* _text, usize _size, c8* _error, usize _error_size);
// The definition a part's text names: its id, or its name (the text's first line). NULL: none.
const fude_sim_def* fude_zoom_logic_find(const c8* _text);

// The part definition _def is on the canvas, as _template says (its look, its model): its pins round a package
// (_package: a chip's) or a block. Made once for each version of it, kept. NULL: no definition.
const fude_zoom_part* fude_zoom_logic_part(const fude_zoom_part* _template, const fude_sim_def* _def, b8 _package);

// --- a circuit's logic ---------------------------------------------------------------------------

typedef enum {
    FUDE_ZOOM_BRIDGE_IN = 0,     // the node's voltage read: a level into the logic
    FUDE_ZOOM_BRIDGE_OUT,        // the logic's level out: a source on the node
    FUDE_ZOOM_BRIDGE_INOUT       // both (a bus transceiver's pin)
} FUDE_ZOOM_BRIDGE_;

typedef struct {
    u32 part, pin;               // the circuit's part, its pin
    u32 node;                    // the circuit's node it is on
    u32 ground;                  // the pin of its part its levels are from (its GND); FUDE_ZOOM_NONE: the circuit's ground
    u32 supply;                  // ...its high's (its VCC); FUDE_ZOOM_NONE: FUDE_ZOOM_LOGIC_V over its ground
    u8  way;                     // FUDE_ZOOM_BRIDGE_
    u8  read;                    // the level it last read (in)
    u8  drive;                   // the level it drives (out): 0, 1, Z (none), X (none)
    u32 input;                   // its input primitive in the flat circuit (in), FUDE_SIM_NONE
    u32 net;                     // its pin's net in the flat circuit
} fude_zoom_bridge;

typedef struct fude_zoom_logic {
    fude_sim_def*                  netlist;      // the circuit's logic (owned)
    fude_sim_flat                  flat;
    fude_sim_run                   run;
    b8                             on;           // there is some, and it was made
    rde_arr TYPE(fude_zoom_bridge) bridges;
    rde_arr TYPE(u32)              part_bridges; // each circuit part's first bridge, and how many (two a part)
    rde_arr TYPE(u32)              node_net;     // each circuit node's net, where only logic pins are on it; else NONE
    rde_arr TYPE(u32)              part_input;   // each circuit part's input primitive (a logic input's); else NONE
    rde_arr TYPE(u32)              part_probe;   // each circuit part's first pin's net (a probe's); else NONE
    rde_arr TYPE(u8)               part_level;   // each logic input's level, as last set
    rde_str                        errors;       // why it could not be made ("": it was)
} fude_zoom_logic;

void fude_zoom_logic_init(fude_zoom_logic* _l);
void fude_zoom_logic_destroy(fude_zoom_logic* _l);
// What it held let go: made next from the start (a circuit reset).
void fude_zoom_logic_forget(fude_zoom_logic* _l);
// A part the logic runs (a gate, a flip-flop, a logic input or probe, a chip, a custom part).
b8   fude_zoom_logic_is(const fude_zoom_part* _part);
// Made from circuit _c's parts as they are built (their pins' nodes): its netlist and bridges, what it held before
// carried over (a flip-flop's Q, a latch's), settled.
void fude_zoom_logic_build(fude_zoom_logic* _l, const fude_zoom_circuit* _c);
// After the circuit is solved (its nodes' voltages _c->v): each input bridge's level, each logic input's, the logic
// run on to time _t and settled; true: what an output bridge drives changed (solve again).
b8   fude_zoom_logic_step(fude_zoom_logic* _l, const fude_zoom_circuit* _c, f64 _t);
// A node's level where only logic is on it (0, 1, Z, X); 0xFF: not one.
u8   fude_zoom_logic_node(const fude_zoom_logic* _l, u32 _node);
// A logic probe's level (its pin's net's); 0xFF: not one.
u8   fude_zoom_logic_probe(const fude_zoom_logic* _l, u32 _part);
// The bridges of part _part: from *_first, how many.
u32  fude_zoom_logic_bridges_of(const fude_zoom_logic* _l, u32 _part, u32* _first);

// --- custom parts made of what is drawn ---------------------------------------------------------

typedef enum {
    FUDE_ZOOM_MAKE_OK = 0,
    FUDE_ZOOM_MAKE_NOTHING,      // no logic part to make one of
    FUDE_ZOOM_MAKE_NO_PORTS      // no logic input or probe to be its inputs and outputs
} FUDE_ZOOM_MAKE_;

// A custom part made of circuit _c's logic (built from what a lasso holds, scene _s's): its logic inputs its inputs
// and its probes its outputs — named by their texts (else IN1…, OUT1…), each side in order from the top (left to
// right where level) — the rest its parts, joined as they are drawn. NULL: none (_problem says why).
fude_sim_def* fude_zoom_logic_make(const fude_zoom_circuit* _c, const fude_zoom_scene* _s, const c8* _id, const c8* _name, u32* _problem);

#endif
