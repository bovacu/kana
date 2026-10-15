// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SIM_H
#define FUDE_SIM_H

#include "rde.h"

// ===========================================================================
// Simulation: the definitions of parts, the netlists that join them and the engines that run them, for circuits
// (analog and digital), mechanisms and what joins them, built as one system rather than one per topic.
//
// Where it sits: `fude/sim`, the lowest layer of the canvas apps, under `fude/zoom`. It is pure C with no canvas and
// no UI: zoom/logic.c compiles what is drawn into its netlists and reads what it finds back out, and tests/sim tests
// it on its own. The overall design is docs/simulation_architecture.md; how it came to be is
// docs/infinite_canvas_design.md §35.
//
//   DEFINITIONS   What a kind of part is: its ports (each in a domain: an electric pin, a logic pin or bus, a shaft),
//                 its parameters, and its behaviour. A PRIMITIVE behaves as its MODEL says (C code, found by id); a
//                 COMPOSITE (a custom part) behaves as the parts inside it do, nested to any depth. Definitions are
//                 kept in a LIBRARY.
//   INSTANCES     A definition used inside a composite: its reference ("U1"), its parameters' values, and the NET
//                 each of its ports is on.
//   ELABORATION   A composite flattened into PRIMITIVES joined by nets, and checked. Each primitive is named by its
//                 path through the composites it is in ("U2/U1").
//   RUN           The engines run on the flat circuit. Today that is the digital engine (events); the analog (MNA)
//                 and mechanical engines are to follow the same way.
//
// Main types and their lifecycle: a fude_sim_def is made on the heap by fude_sim_def_new or fude_sim_def_read and
// freed by fude_sim_def_free, unless it is given to a library (fude_sim_lib_add), which then owns and frees it. A
// fude_sim_lib, fude_sim_flat and fude_sim_run are the caller's structs, set up by their _init and released by their
// _destroy. A run points at its flat circuit, which must outlive it.
//
// Threads: called on the app's main thread (zoom/logic.c, as the canvas plays). The module keeps no state of its own
// besides constant tables, so separate libraries, flat circuits and runs could be used on different threads, but one
// object must never be used from two threads at once.
//
// Files: none are opened here. A definition is turned into a small text, readable, diffable and shareable ("fude-part
// 1", then one line per field, port, parameter, net and part, then "end": see fude_sim_def_write), and read back by
// fude_sim_def_read. zoom/page_custom.c saves these texts as `<id>.part` files for My parts, the only place a custom
// part is kept: a canvas names the parts it uses, and finds them in My parts (a canvas keeping its own copies is
// planned: docs/simulation_architecture.md, step 3b).
//
// Design:
// - Logic runs on events, not equations: a net changes only when one of its drivers does, so thousands of gates run
//   at interactive speed, and each model's delay orders what follows.
// - Delays are inertial: a change undone before its delay has passed never happens, as a real gate does not pass a
//   pulse shorter than its delay.
// - A net that something drives starts unknown (X), so that nothing is assumed about it before its drivers start.
// - Recursion is found by depth: composites nested deeper than FUDE_SIM_DEPTH are taken as a part made of itself. It
//   is a cheap test, and no real part nests that deep.
// - Every primitive keeps its state as numbers in one array of the run, so that a circuit built again can carry its
//   state over by each primitive's path (fude_sim_run_keep).
// ===========================================================================

// Sizes, each counting the terminating 0 where it is a string's.
#define FUDE_SIM_ID      48u    // the longest definition id
#define FUDE_SIM_NAME    32u    // the longest name of a port, a net or a reference
#define FUDE_SIM_PATH    128u   // the longest path of a primitive in the flat circuit ("U2/U1/U3")
#define FUDE_SIM_PARAMS  8u     // the most parameters a primitive has
#define FUDE_SIM_DEPTH   32u    // the most composites nested in each other; deeper is taken as a part made of itself
// "No index": no net, no primitive, no port.
#define FUDE_SIM_NONE    0xFFFFFFFFu

// --- domains, ports, parameters -------------------------------------------------------------------

// What a connection carries, and so which engine solves it.
typedef enum {
    FUDE_SIM_ELECTRIC = 0,   // a voltage across it and a current through it (the analog engine)
    FUDE_SIM_LOGIC,          // the logic values 0, 1, Z and X (the digital engine)
    FUDE_SIM_ROTATION,       // a shaft: its angle and speed across, its torque through (the mechanical engine)
    FUDE_SIM_TRANSLATION,    // a point moving along a line: its place and speed across, its force through
    FUDE_SIM_SIGNAL,         // a plain number, such as a sensor's reading or a PWM's duty
    FUDE_SIM_DOMAINS
} FUDE_SIM_DOMAIN_;

// The direction of a port.
typedef enum {
    FUDE_SIM_IN = 0,
    FUDE_SIM_OUT,
    FUDE_SIM_INOUT,
    FUDE_SIM_PASSIVE         // an electric pin, which is neither an input nor an output
} FUDE_SIM_DIR_;

// The side of its part's symbol where a port is drawn.
typedef enum {
    FUDE_SIM_LEFT = 0,       // inputs go on the left, outputs on the right, unless a definition says otherwise
    FUDE_SIM_RIGHT,
    FUDE_SIM_TOP,
    FUDE_SIM_BOTTOM
} FUDE_SIM_SIDE_;

// A part's connection. Its width is the part's text's (fude_sim_def_write keeps it, fude_sim_def_read reads it back),
// so a part made for a bus keeps saying so; nothing runs on it yet, as the digital engine runs one bit a port: kept in
// the format now so that files made today read the same once buses run, rather than the format changing then.
typedef struct {
    c8 name[FUDE_SIM_NAME];
    u8 domain;               // a FUDE_SIM_DOMAIN_
    u8 dir;                  // a FUDE_SIM_DIR_
    u8 width;                // in bits: 1 to 64 for a logic bus, 1 for every other port (the engine runs one bit now)
    u8 side;                 // a FUDE_SIM_SIDE_
} fude_sim_port;

// A parameter a definition takes, such as a clock's period. Its limits are for an editor of parameters to keep a value
// within; none is made yet, and elaboration takes a value as it is typed (a clock of 0.1 ns runs as one). They are kept
// in the part's text, as a port's width is, so that a part says them once such an editor reads them.
typedef struct {
    c8  name[FUDE_SIM_NAME];
    c8  unit[8];
    f64 value;               // its default value
    f64 lo, hi;              // its limits, for whoever edits it (lo == hi: no limits); elaboration does not apply them
} fude_sim_param;

// The logic values: what a net holds, and what a port drives.
typedef enum {
    FUDE_SIM_0 = 0,
    FUDE_SIM_1,
    FUDE_SIM_Z,              // released: nothing drives it
    FUDE_SIM_X               // unknown, or driven by drivers that disagree
} FUDE_SIM_LOGIC_;

// --- models ---------------------------------------------------------------------------------------

struct fude_sim_run;

// How a primitive behaves: what it is, and one function for each thing it does. Any function may be NULL.
typedef struct fude_sim_model {
    const c8*             id;
    const c8*             name;          // its name in English, shown on its symbol (the app shows its own translation)
    const fude_sim_param* params;        // its parameters, with their default values
    u32                   param_count;
    u32                   state;         // how many numbers each primitive of it keeps in a run (unless `states` says)
    // Writes its ports for the parameter values _params into _out (at most _max of them) and returns how many. The
    // ports may depend on the parameters: an AND gate has as many inputs as its parameter says.
    u32  (*ports)(const f64* _params, fude_sim_port* _out, u32 _max);
    // LOGIC: `start` sets its outputs as a run starts; `logic` is called when one of its inputs changes and sets its
    // outputs after their delays; `wake` is called at a time it asked to be woken at (fude_sim_wake), such as a clock's
    // next edge.
    void (*start)(struct fude_sim_run* _run, u32 _prim);
    void (*logic)(struct fude_sim_run* _run, u32 _prim);
    void (*wake)(struct fude_sim_run* _run, u32 _prim);
    // How many numbers it keeps for the parameter values _params, for a model where that depends on them (a memory
    // keeps one pair per word). NULL means it always keeps `state` numbers.
    u32  (*states)(const f64* _params);
} fude_sim_model;

// Returns how many numbers a primitive of _model with parameter values _params keeps in a run (0 for a NULL model).
u32 fude_sim_model_states(const fude_sim_model* _model, const f64* _params);

// The registered models: every built-in one, which today is the logic family (inputs, constants, probes, clocks,
// gates, buffers, flip-flops, latches and a memory, in sim/logic.c). fude_sim_model_at returns NULL past the end, and
// fude_sim_model_find returns NULL when no model has that id. The models are static and stay valid for ever.
u32                   fude_sim_model_count(void);
const fude_sim_model* fude_sim_model_at(u32 _i);
const fude_sim_model* fude_sim_model_find(const c8* _id);

// --- definitions and the library -----------------------------------------------------------------

// What a definition's behaviour comes from.
typedef enum {
    FUDE_SIM_PRIMITIVE = 0,  // its model
    FUDE_SIM_COMPOSITE       // the parts inside it
} FUDE_SIM_KIND_;

// A part inside a composite: an instance of a definition.
typedef struct {
    c8  def[FUDE_SIM_ID];         // the id of the definition it is
    c8  ref[FUDE_SIM_NAME];       // its reference ("U1")
    c8  params[96];               // its parameters as typed ("inputs=3 delay=5n"); "" keeps its definition's defaults
    u32 first, count;             // its ports' nets: `count` of them, from index `first` in its composite's inst_nets
    u64 tag;                      // what it stands for to whoever made it (the id of a canvas object)
} fude_sim_inst;

// What a kind of part is. Ids, names of ports and nets, and references have their spaces, tabs, quotes and line breaks
// turned into underscores, because the text format keeps each of them as a single word.
typedef struct fude_sim_def {
    c8                    id[FUDE_SIM_ID];
    c8                    name[64];
    c8                    desc[160];
    // Its version: 1 when made, raised when it replaces one with the same id in a library.
    u32                   version;
    u8                    kind;          // a FUDE_SIM_KIND_
    const fude_sim_model* model;         // a primitive's model (NULL for a composite)
    rde_arr TYPE(fude_sim_port)  ports;
    rde_arr TYPE(fude_sim_param) params;
    // A composite's insides: the names of its nets; its parts; each part's ports' nets (indices into nets, or
    // FUDE_SIM_NONE for a port left unjoined); and, for each of its own ports, the net inside that the port is on.
    rde_arr TYPE(c8[FUDE_SIM_NAME]) nets;
    rde_arr TYPE(fude_sim_inst)  insts;
    rde_arr TYPE(u32)            inst_nets;
    rde_arr TYPE(u32)            port_nets;
    // Each net's index by its name, so that a part's nets are found as it is added without a search through every net
    // (a netlist of thousands of parts). fude_sim_def_net, the one function that adds a net, keeps it.
    rde_hash_map TYPE(c8[FUDE_SIM_NAME], u32) net_index;
} fude_sim_def;

// Makes a new, empty composite with id _id and name _name, at version 1. The caller owns it: free it with
// fude_sim_def_free, or give it to a library with fude_sim_lib_add. fude_sim_def_free does nothing for NULL.
fude_sim_def* fude_sim_def_new(const c8* _id, const c8* _name);
void          fude_sim_def_free(fude_sim_def* _def);
// Adds a port to a composite, together with the net inside it of the same name (an existing net of that name is
// reused). A domain or direction out of range becomes electric or passive, and a width of 0 becomes 1; outputs go on
// the right, the rest on the left. Returns the port's index.
u32 fude_sim_def_port(fude_sim_def* _def, const c8* _name, u8 _domain, u8 _dir, u8 _width);
// Returns the index of the composite's net called _name, adding the net first if there is none by that name.
u32 fude_sim_def_net(fude_sim_def* _def, const c8* _name);
// Adds a part to a composite: an instance of definition _what with reference _ref and parameters _params as typed.
// _nets gives the names of the nets its _count ports are on, in its definition's port order; a NULL or empty name
// leaves that port unjoined, and a name not used yet makes a new net. Quotes and line breaks in _params become
// spaces, because the text format keeps the parameters between quotes on one line. Returns the part's index.
u32 fude_sim_def_inst(fude_sim_def* _def, const c8* _what, const c8* _ref, const c8* _params, const c8* const* _nets, u32 _count);

// A set of definitions, found by id. It owns every definition in it.
typedef struct {
    rde_arr TYPE(fude_sim_def*) defs;
} fude_sim_lib;

// fude_sim_lib_init fills the library with one primitive definition for each registered model; fude_sim_lib_destroy
// frees every definition in it. fude_sim_lib_find returns the definition with id _id, or NULL when there is none; the
// pointer stays valid until a definition with the same id replaces it or the library is destroyed.
void          fude_sim_lib_init(fude_sim_lib* _lib);          // every registered model is put in as a primitive
void          fude_sim_lib_destroy(fude_sim_lib* _lib);
const fude_sim_def* fude_sim_lib_find(const fude_sim_lib* _lib, const c8* _id);
// Gives _def to the library, which owns it from then on. A definition already there with the same id is freed and
// replaced, and _def's version becomes one more than that one's, unless _def's own version is higher already.
// Returns its index.
u32           fude_sim_lib_add(fude_sim_lib* _lib, fude_sim_def* _def);

// fude_sim_def_write appends a definition's text to _out (an array of c8, with no terminating 0): "fude-part 1", then
// lines "id", "name", "version", "desc", "port <name> <domain> <dir> <width> <side>",
// "param <name> <default> <unit or -> <lo> <hi>", and either "net <name>" and
// "inst <definition> <reference or -> "<parameters>" <net or -> ..." for a composite or "model <id>" for a primitive,
// and finally "end"; a line break in the name or the description is written as a space, since each is the rest of its
// line. fude_sim_def_read reads such a text (_size bytes; blank lines and lines starting with '#' are
// skipped) into a new definition the caller owns, or returns NULL and writes why, with its line number, into _error.
void          fude_sim_def_write(const fude_sim_def* _def, rde_arr* _out);
fude_sim_def* fude_sim_def_read(const c8* _text, usize _size, c8* _error, usize _error_size);

// Reads a parameter's number as it is typed: "4.7k", "10n", "1e-3", "2.2 M". The SI prefixes p n u µ m k M G multiply
// it (K works as k); spaces may come before the prefix, and what follows it is ignored. Returns false when the text
// does not start with a number, or the number is not finite.
b8 fude_sim_number(const c8* _text, f64* _out);

// --- the chips: logic chips (74HC…) as custom parts, their pins in order (chips.c) ------------------

// The built-in chips' part texts (the 74HC series), in the format fude_sim_def_read reads. fude_sim_chip_text returns
// NULL past the end; the texts are static and stay valid for ever.
u32       fude_sim_chip_count(void);
const c8* fude_sim_chip_text(u32 _i);
// Reads every chip and adds it to _lib. Returns how many could not be read, which is 0 unless a chip's text is wrong.
u32       fude_sim_chips_add(fude_sim_lib* _lib);

// --- elaboration: the flat circuit ---------------------------------------------------------------

// A primitive in the flat circuit.
typedef struct {
    const fude_sim_model* model;
    c8   path[FUDE_SIM_PATH];     // its path through the composites it is in ("U2/U1")
    u32  first, count;            // its ports: `count` of them from `first` in the flat circuit's ports and port_nets
    f64  params[FUDE_SIM_PARAMS]; // its parameters' values, in its model's order
    u32  state;                   // the index of its first number in a run's state
    u64  tag;                     // the tag of the top composite's part it is inside (the canvas object it belongs to)
} fude_sim_prim;

// A composite flattened into primitives and the nets that join them.
typedef struct {
    rde_arr TYPE(fude_sim_prim) prims;
    rde_arr TYPE(fude_sim_port) ports;      // every primitive's ports, one primitive after another
    rde_arr TYPE(u32)           port_nets;  // the net each of those ports is on
    rde_arr TYPE(u8)            net_domain; // each net's domains, a bit (1 << FUDE_SIM_DOMAIN_) for each found on it
    u32                         nets;
    u32                         state;      // how many numbers its primitives keep, together
    rde_str                     errors;     // what is wrong, one line each ("" when nothing is)
    // Each primitive's index by its path (the first one's, where two share a path), made as it is elaborated, so that
    // fude_sim_flat_find is a look-up rather than a search through every primitive.
    rde_hash_map TYPE(c8[FUDE_SIM_PATH], u32) by_path;
} fude_sim_flat;

// Sets up an empty flat circuit, and frees what one holds.
void fude_sim_flat_init(fude_sim_flat* _flat);
void fude_sim_flat_destroy(fude_sim_flat* _flat);
// Flattens _top into _flat, which is cleared first: each composite part is replaced by its insides, and nets are
// joined through the ports between them. Then it checks the nets: logic and electric ports may share a net (to be
// bridged where they meet; elaboration itself does not add bridges yet), but any other mix of domains is an error.
// Returns false when it could not be done or the check failed (a definition missing, a part made of itself, domains
// mixed); _flat's errors then say why, and _flat may be partly filled.
b8   fude_sim_elaborate(const fude_sim_lib* _lib, const fude_sim_def* _top, fude_sim_flat* _flat);
// Returns what is wrong, one line each ("" when nothing is). The text belongs to _flat and stays valid until it is
// elaborated again or destroyed.
const c8* fude_sim_flat_errors(const fude_sim_flat* _flat);
// Returns the index of the primitive at _path ("U2/U1"; the first, where two share it), or FUDE_SIM_NONE when there is
// none (or _path is NULL).
u32  fude_sim_flat_find(const fude_sim_flat* _flat, const c8* _path);

// --- the run --------------------------------------------------------------------------------------

// Something to happen at a time: a port's drive changing to `value`, or a primitive's wake. `time` is in seconds, and
// `seq` is the order it was made in: of two events at the same time, the one made first goes first.
typedef struct {
    f64 time;
    u64 seq;
    u32 port;                     // the flat port whose drive changes (FUDE_SIM_NONE: a wake for `prim`)
    u32 prim;
    u8  value;
} fude_sim_event;

// The digital engine running a flat circuit. `flat` is the circuit it runs: it is not owned, and must outlive the run.
// `now` is the time in seconds, and `seq` the number given to the last event made.
typedef struct fude_sim_run {
    const fude_sim_flat* flat;
    f64  now;
    u64  seq;
    // LOGIC: each net's value; each port's drive; and the seq of each port's last scheduled change. A newer change
    // to a port cancels one still waiting, which makes the delays inertial.
    rde_arr TYPE(u8)  net_value;
    rde_arr TYPE(u8)  drive;
    rde_arr TYPE(u64) pending;
    // Each net's readers (the primitives with an IN or INOUT logic port on it, each once) and its drivers (its OUT and
    // INOUT ports): net n's are those from first[n] up to first[n + 1].
    rde_arr TYPE(u32) read_first,  read_prims;
    rde_arr TYPE(u32) drive_first, drive_ports;
    // The numbers the primitives keep, each primitive's from its `state` index on (fude_sim_state).
    rde_arr TYPE(f64) state;
    rde_arr TYPE(fude_sim_event) events;   // a heap with the soonest event first
    u32  settle_limit;                     // the most events one call may fire (200 000); more means it oscillates
    // Set when a call fired more than settle_limit events (and stays set).
    b8   oscillating;
} fude_sim_run;

// Starts a run of _flat at time 0. Every primitive is started and its starting outputs are put on their nets all at
// once; then each primitive is worked out once from what it reads, and what follows is settled (1 ms ahead). The
// outputs go on together because a flip-flop working out its reset while its set is still unknown would latch X for
// ever. fude_sim_run_destroy frees the run, and does nothing for one that is all zeros.
void fude_sim_run_init(fude_sim_run* _run, const fude_sim_flat* _flat);
void fude_sim_run_destroy(fude_sim_run* _run);
// Runs on to time _t (seconds), firing every event at or before it. When more than settle_limit events fire (a loop of
// gates that never rests), the run is marked as oscillating and its waiting changes of drive are dropped, which stops
// the loop where it is; its wakes are kept (those due before _t moved on to _t), so its clocks go on ticking.
void fude_sim_run_until(fude_sim_run* _run, f64 _t);
// Settles what follows from now: fires every change up to _horizon seconds ahead, and any wake due now. It stops at
// the first wake due later, so a clock's next tick is left for its time. Past settle_limit events it marks the run as
// oscillating and stops, every event left waiting (the next fude_sim_run_until deals with them).
void fude_sim_run_settle(fude_sim_run* _run, f64 _horizon);
// Returns a net's value (a FUDE_SIM_LOGIC_), or FUDE_SIM_Z for a net that does not exist.
u8   fude_sim_run_net(const fude_sim_run* _run, u32 _net);
// Sets the value of an input primitive (a switch: the "input" model) and drives its output to it from now. The change
// happens on the next fude_sim_run_until or fude_sim_run_settle. Returns false, changing nothing, when _prim is not
// an input (or not in the circuit).
b8   fude_sim_run_set(fude_sim_run* _run, u32 _prim, u8 _value);
// Returns a net's value as its drivers make it, leaving out primitive _prim's own ports: what the rest of the circuit
// drives it to. A bridge uses it so as not to read back its own drive. FUDE_SIM_Z for a net that does not exist.
u8   fude_sim_run_net_without(const fude_sim_run* _run, u32 _net, u32 _prim);
// Carries over what an earlier run kept, after the circuit was built again: _run is newly started, _was is the run
// before. Each primitive found in _was by the same path, model and port count gets its state and its ports' drives,
// then every primitive is worked out again from them and settled. Drives are carried too because a latch made of
// gates holds its value in its nets, not in any state. The run goes on from _was's time (now), and a primitive that
// took its state keeps the wakes it was waiting for, so a clock goes on with its phase; a new one starts at that time.
void fude_sim_run_keep(fude_sim_run* _run, const fude_sim_run* _was);

// For models, given their primitive _prim and a port _port numbered within it:
// fude_sim_in returns the value of the port's net (FUDE_SIM_Z when there is no such port); fude_sim_out sets the
// port's drive to _value after _delay seconds (a newer change to the port cancels this one if it is still waiting);
// fude_sim_wake asks for the model's wake after _delay seconds; fude_sim_state returns the primitive's numbers in the
// run, valid until the run is destroyed; fude_sim_arg returns its parameter _k (0 past FUDE_SIM_PARAMS);
// fude_sim_port_count returns how many ports it has.
u8   fude_sim_in(const fude_sim_run* _run, u32 _prim, u32 _port);
void fude_sim_out(fude_sim_run* _run, u32 _prim, u32 _port, u8 _value, f64 _delay);
void fude_sim_wake(fude_sim_run* _run, u32 _prim, f64 _delay);
f64* fude_sim_state(fude_sim_run* _run, u32 _prim);
f64  fude_sim_arg(const fude_sim_run* _run, u32 _prim, u32 _k);
u32  fude_sim_port_count(const fude_sim_run* _run, u32 _prim);

#endif
