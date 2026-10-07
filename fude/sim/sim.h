// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SIM_H
#define FUDE_SIM_H

#include "rde.h"

// ===========================================================================
// SIMULATION: parts and how they behave, joined in nets, run — circuits (analog
// and digital), mechanisms, and what joins them — as one system (the design:
// docs/simulation_architecture.md). Pure C: no canvas, no UI; what is drawn is
// compiled into it (zoom/), and what it finds is read back out.
//
//   DEFINITIONS   what a kind of part is: its ports (each in a domain: an
//                 electric pin, a logic pin or bus, a shaft…), its parameters,
//                 and its behaviour — a MODEL (a primitive: C code, by id) or the
//                 parts inside it (a COMPOSITE: a custom part, made of others,
//                 nested to any depth). Kept in a LIBRARY.
//   INSTANCES     a definition used: its reference (U1), its parameters' values,
//                 each port's NET.
//   ELABORATION   a composite flattened into PRIMITIVES with their nets (its
//                 parts' names prefixed by their paths: "U2/U1"), checked.
//   RUN           the engines on the flat circuit: digital (events) today;
//                 analog (MNA) and mechanical next, the same way.
//
// A definition is also a small text file (fude_sim_def_write / _read), readable
// and shareable: a custom part saved, a canvas's copy of it.
// ===========================================================================

#define FUDE_SIM_ID      48u    // a definition's id, at most (with its 0)
#define FUDE_SIM_NAME    32u    // a port's, a net's, a reference's name
#define FUDE_SIM_PATH    128u   // a primitive's path in the flat circuit ("U2/U1/U3")
#define FUDE_SIM_PARAMS  8u     // a primitive's parameters, at most
#define FUDE_SIM_DEPTH   32u    // composites inside composites, at most (deeper: a part made of itself)
#define FUDE_SIM_NONE    0xFFFFFFFFu

// --- domains, ports, parameters -------------------------------------------------------------------

typedef enum {
    FUDE_SIM_ELECTRIC = 0,   // a voltage across, a current through (the analog engine)
    FUDE_SIM_LOGIC,          // 0, 1, Z, X (the digital engine)
    FUDE_SIM_ROTATION,       // a shaft: its angle and speed across, its torque through (the mechanical engine)
    FUDE_SIM_TRANSLATION,    // a point along a line: its place and speed, its force
    FUDE_SIM_SIGNAL,         // a number (a sensor's reading, a PWM's duty)
    FUDE_SIM_DOMAINS
} FUDE_SIM_DOMAIN_;

typedef enum {
    FUDE_SIM_IN = 0,
    FUDE_SIM_OUT,
    FUDE_SIM_INOUT,
    FUDE_SIM_PASSIVE         // an electric pin: neither in nor out
} FUDE_SIM_DIR_;

typedef enum {
    FUDE_SIM_LEFT = 0,       // where a port sits on its part's symbol
    FUDE_SIM_RIGHT,
    FUDE_SIM_TOP,
    FUDE_SIM_BOTTOM
} FUDE_SIM_SIDE_;

typedef struct {
    c8 name[FUDE_SIM_NAME];
    u8 domain;               // FUDE_SIM_DOMAIN_
    u8 dir;                  // FUDE_SIM_DIR_
    u8 width;                // bits (a logic bus: 1..64); 1 for the rest
    u8 side;                 // FUDE_SIM_SIDE_
} fude_sim_port;

typedef struct {
    c8  name[FUDE_SIM_NAME];
    c8  unit[8];
    f64 value;               // its default
    f64 lo, hi;              // its limits (lo == hi: none)
} fude_sim_param;

// The logic values (a net's, a port's drive).
typedef enum {
    FUDE_SIM_0 = 0,
    FUDE_SIM_1,
    FUDE_SIM_Z,              // released: nothing drives it
    FUDE_SIM_X               // unknown, or drivers that disagree
} FUDE_SIM_LOGIC_;

// --- models ---------------------------------------------------------------------------------------

struct fude_sim_run;

// How a primitive behaves: what it is, and a function each for what it does (each may be NULL).
typedef struct fude_sim_model {
    const c8*             id;
    const c8*             name;          // in English (its symbol's name, the app's in its language)
    const fude_sim_param* params;        // its parameters (their defaults)
    u32                   param_count;
    u32                   state;         // numbers it keeps, each
    // Its ports for its parameters (an AND's inputs); into _out (_max at most). How many.
    u32  (*ports)(const f64* _params, fude_sim_port* _out, u32 _max);
    // LOGIC: its outputs as it starts; an input changed (its outputs, after their delays); a time it asked to wake at.
    void (*start)(struct fude_sim_run* _run, u32 _prim);
    void (*logic)(struct fude_sim_run* _run, u32 _prim);
    void (*wake)(struct fude_sim_run* _run, u32 _prim);
    // The numbers it keeps for its parameters (a memory's words), when they depend on them (NULL: `state`).
    u32  (*states)(const f64* _params);
} fude_sim_model;

// The numbers a primitive of _model keeps with parameters _params.
u32 fude_sim_model_states(const fude_sim_model* _model, const f64* _params);

// Registered models (every built-in one: the logic family's here; more as they come).
u32                   fude_sim_model_count(void);
const fude_sim_model* fude_sim_model_at(u32 _i);
const fude_sim_model* fude_sim_model_find(const c8* _id);

// --- definitions and the library -----------------------------------------------------------------

typedef enum {
    FUDE_SIM_PRIMITIVE = 0,  // its behaviour: its model
    FUDE_SIM_COMPOSITE       // ...the parts inside it
} FUDE_SIM_KIND_;

// A part inside a composite.
typedef struct {
    c8  def[FUDE_SIM_ID];         // what it is (a definition's id)
    c8  ref[FUDE_SIM_NAME];       // its reference ("U1")
    c8  params[96];               // its parameters as typed ("inputs=3 delay=5n"; "": its definition's)
    u32 first, count;             // its ports' nets: count of them, from first in its composite's inst_nets
    u64 tag;                      // what it is to whoever made it (a canvas object's id)
} fude_sim_inst;

typedef struct fude_sim_def {
    c8                    id[FUDE_SIM_ID];
    c8                    name[64];
    c8                    desc[160];
    u32                   version;
    u8                    kind;          // FUDE_SIM_KIND_
    const fude_sim_model* model;         // a primitive's
    rde_arr TYPE(fude_sim_port)  ports;
    rde_arr TYPE(fude_sim_param) params;
    // A composite's: its nets (their names), its parts, each part's ports' nets, each of its ports' net inside.
    rde_arr TYPE(c8[FUDE_SIM_NAME]) nets;
    rde_arr TYPE(fude_sim_inst)  insts;
    rde_arr TYPE(u32)            inst_nets;
    rde_arr TYPE(u32)            port_nets;
} fude_sim_def;

// A new composite (its id, its name). Free with fude_sim_def_free (or give it to a library).
fude_sim_def* fude_sim_def_new(const c8* _id, const c8* _name);
void          fude_sim_def_free(fude_sim_def* _def);
// A port of a composite (and its net inside, named as it is). Its index.
u32 fude_sim_def_port(fude_sim_def* _def, const c8* _name, u8 _domain, u8 _dir, u8 _width);
// A net of a composite by its name (made if it is not there). Its index.
u32 fude_sim_def_net(fude_sim_def* _def, const c8* _name);
// A part in a composite: definition _what, its reference, its parameters as typed, its ports' nets (by name, in
// its definition's port order; a name not there yet makes a net). Its index.
u32 fude_sim_def_inst(fude_sim_def* _def, const c8* _what, const c8* _ref, const c8* _params, const c8* const* _nets, u32 _count);

typedef struct {
    rde_arr TYPE(fude_sim_def*) defs;
} fude_sim_lib;

void          fude_sim_lib_init(fude_sim_lib* _lib);          // every registered model in it, as a primitive
void          fude_sim_lib_destroy(fude_sim_lib* _lib);
const fude_sim_def* fude_sim_lib_find(const fude_sim_lib* _lib, const c8* _id);
// _def given to it (another with its id let go: its version one more). Its index.
u32           fude_sim_lib_add(fude_sim_lib* _lib, fude_sim_def* _def);

// A definition as text (appended to _out: c8s) — and read back (NULL: it could not be; _error says why).
void          fude_sim_def_write(const fude_sim_def* _def, rde_arr* _out);
fude_sim_def* fude_sim_def_read(const c8* _text, usize _size, c8* _error, usize _error_size);

// A parameter's number as it is typed: "4.7k", "10n", "1e-3", "2.2 M" (p n u µ m k M G); false: none there.
b8 fude_sim_number(const c8* _text, f64* _out);

// --- the chips: logic chips (74HC…) as custom parts, their pins in order (chips.c) ------------------

u32       fude_sim_chip_count(void);
const c8* fude_sim_chip_text(u32 _i);
// Every chip added to _lib. How many could not be read (0, unless one's text is wrong).
u32       fude_sim_chips_add(fude_sim_lib* _lib);

// --- elaboration: the flat circuit ---------------------------------------------------------------

typedef struct {
    const fude_sim_model* model;
    c8   path[FUDE_SIM_PATH];     // "U2/U1"
    u32  first, count;            // its ports: count of them, from first in the flat circuit's ports and port_nets
    f64  params[FUDE_SIM_PARAMS]; // its parameters' values (its model's order)
    u32  state;                   // its first number in a run's state
    u64  tag;                     // its top composite's part's tag (the canvas object it is in)
} fude_sim_prim;

typedef struct {
    rde_arr TYPE(fude_sim_prim) prims;
    rde_arr TYPE(fude_sim_port) ports;      // each primitive's, in turn
    rde_arr TYPE(u32)           port_nets;  // ...their nets
    rde_arr TYPE(u8)            net_domain; // each net's
    u32                         nets;
    u32                         state;      // numbers its primitives keep, together
    rde_str                     errors;     // what is wrong, a line each ("" when nothing)
} fude_sim_flat;

void fude_sim_flat_init(fude_sim_flat* _flat);
void fude_sim_flat_destroy(fude_sim_flat* _flat);
// _top flattened (its parts' composites into their primitives, its nets joined through their ports), checked. False:
// it could not be (a definition missing, a part made of itself: _flat's errors say).
b8   fude_sim_elaborate(const fude_sim_lib* _lib, const fude_sim_def* _top, fude_sim_flat* _flat);
// What is wrong, a line each ("" when nothing).
const c8* fude_sim_flat_errors(const fude_sim_flat* _flat);
// The primitive at _path ("U2/U1"; FUDE_SIM_NONE: none).
u32  fude_sim_flat_find(const fude_sim_flat* _flat, const c8* _path);

// --- the run --------------------------------------------------------------------------------------

typedef struct {
    f64 time;
    u64 seq;
    u32 port;                     // a flat port (FUDE_SIM_NONE: a primitive's wake, below)
    u32 prim;
    u8  value;
} fude_sim_event;

typedef struct fude_sim_run {
    const fude_sim_flat* flat;
    f64  now;
    u64  seq;
    // LOGIC: each net's value; each port's drive and its last scheduled change (a newer one cancels it: inertial).
    rde_arr TYPE(u8)  net_value;
    rde_arr TYPE(u8)  drive;
    rde_arr TYPE(u64) pending;
    // Each net's readers (its primitives with an IN or INOUT logic port on it) and drivers (OUT, INOUT ports): net n's
    // from first[n] to first[n + 1].
    rde_arr TYPE(u32) read_first,  read_prims;
    rde_arr TYPE(u32) drive_first, drive_ports;
    rde_arr TYPE(f64) state;
    rde_arr TYPE(fude_sim_event) events;   // a heap: the soonest first
    u32  settle_limit;                     // events in one instant at most (more: it oscillates)
    b8   oscillating;
} fude_sim_run;

// A run of _flat at time 0: each primitive started, what follows settled.
void fude_sim_run_init(fude_sim_run* _run, const fude_sim_flat* _flat);
void fude_sim_run_destroy(fude_sim_run* _run);
// On to time _t (seconds): every event before it.
void fude_sim_run_until(fude_sim_run* _run, f64 _t);
// What settles now (every event at or before now, and those they bring at the same time and later, within _horizon).
void fude_sim_run_settle(fude_sim_run* _run, f64 _horizon);
u8   fude_sim_run_net(const fude_sim_run* _run, u32 _net);
// An input part's value set (a switch: "input"'s): what follows, from now.
void fude_sim_run_set(fude_sim_run* _run, u32 _prim, u8 _value);
// A net's value from its drivers but primitive _prim's (what the rest drive it to: a bridge's own left out).
u8   fude_sim_run_net_without(const fude_sim_run* _run, u32 _net, u32 _prim);
// What a run before kept carried over (a circuit made again): each primitive's state where one of the same path and
// model was in _was, then every primitive worked out again from it and settled.
void fude_sim_run_keep(fude_sim_run* _run, const fude_sim_run* _was);

// For models: their port's net's value now, what they drive, a change to drive after _delay, a wake after _delay,
// their state, their parameters, the time.
u8   fude_sim_in(const fude_sim_run* _run, u32 _prim, u32 _port);
void fude_sim_out(fude_sim_run* _run, u32 _prim, u32 _port, u8 _value, f64 _delay);
void fude_sim_wake(fude_sim_run* _run, u32 _prim, f64 _delay);
f64* fude_sim_state(fude_sim_run* _run, u32 _prim);
f64  fude_sim_arg(const fude_sim_run* _run, u32 _prim, u32 _k);
u32  fude_sim_port_count(const fude_sim_run* _run, u32 _prim);

#endif
