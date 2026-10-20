// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_MECH_H
#define FUDE_ZOOM_MECH_H

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/scene.h"

// ===========================================================================
// Mechanisms: the parts of the Mechanisms topic (linkages, gears, springs, weights and more, drawn as the
// diagram library's symbols: symbol.h's Mechanisms family), and the plan of how they are joined, which mechrun.h
// runs as rigid bodies in RDE's 2D physics.
//
// A part's HOLES are where it is pinned. A hole over another part's hole is a hinge between them; over a fixed
// pivot's, the part turns about the ground there; over a motor's, the motor turns it (the motor's text gives its
// speed, "30 rpm"). A spring's two ends pull the parts whose holes they are on toward its length as it was drawn.
// Gears whose pitch circles touch turn each other, the smaller faster: each gear's tooth count comes from its id, and
// all have one tooth size, so any two mesh. A gear (or a pulley) that nothing holds turns on an axle of its own where
// it is. A gear on a rack moves the rack as long as the gear is over its teeth; past them the rack stops and lets the
// gear turn on (turned back, the gear takes the rack back). Meshed gears start with their teeth in each other's gaps:
// a gear held only by its axle is turned the little it needs, and a rack is slid along itself the little it needs. A
// weight or a crate falls; walls stay.
//
// A ROPE holds what its ends are on: a body (anywhere on it), or a still point (a pivot, a wall, a pulley's rim, or
// where it was drawn). It never stretches past its length as drawn, and goes slack when shorter. Two ropes down either
// side of one PULLEY are one rope over it: what one lets down, the other takes up (their lengths together are kept),
// and the pulley turns with them.
//
// Where it sits: `fude/zoom`, the canvas layer. symbol.c draws the parts with fude_zoom_mech_draw; page_mech.c builds
// the plan from the canvas when Play starts, and mechrun.h runs it.
// What is worked out here, the plan (which bodies, hinges, springs, ropes and gear meshes), is pure and tested on its
// own; running it is mechrun.h's, on RDE's own joints (its gear, rope and pulley joints keep the meshes and the ropes,
// so nothing here works out speeds or a rope's pull).
// Main types: fude_zoom_mech_part is a static catalogue entry; fude_zoom_mech_plan is the caller's struct, set up by
// fude_zoom_mech_plan_init, filled by fude_zoom_mech_plan_build and released by fude_zoom_mech_plan_destroy.
// Threads: main thread only (fude_zoom_mech_of_kind's answers are kept in the topics' one table, symbol.h's
// fude_zoom_symbol_topic_part, and building reads the scene).
// Design: docs/infinite_canvas_design.md §28 (the first mechanisms), §33 (sliders and racks) and §55 (belts, cams,
// ratchets, dampers and worms).
// ===========================================================================

// The kinds of mechanism part.
typedef enum {
    FUDE_ZOOM_MECH_LINK = 0,   // a bar with a hole at each end
    FUDE_ZOOM_MECH_PLATE,      // a triangle with three holes
    FUDE_ZOOM_MECH_GEAR,       // a spur gear: its hole in its middle, its teeth its part's
    FUDE_ZOOM_MECH_PIVOT,      // a fixed pivot (the ground)
    FUDE_ZOOM_MECH_MOTOR,      // a fixed pivot that turns what is pinned to it
    FUDE_ZOOM_MECH_SPRING,     // a spring between its two ends
    FUDE_ZOOM_MECH_WEIGHT,     // a ball: it falls, it rolls
    FUDE_ZOOM_MECH_WHEEL,      // a wheel: its hole in its middle, it rolls
    FUDE_ZOOM_MECH_WALL,       // fixed and solid
    FUDE_ZOOM_MECH_PULLEY,     // a wheel ropes run over, on its own axle where it is
    FUDE_ZOOM_MECH_ROPE,       // a rope between its two ends, never longer than drawn
    FUDE_ZOOM_MECH_CRATE,      // a box: it falls, it hangs (its text is its mass)
    FUDE_ZOOM_MECH_RAIL,       // a fixed guide: what slides along it, between its ends
    FUDE_ZOOM_MECH_SLIDER,     // a block that slides along the rail it is on, its hole in its middle (a piston)
    FUDE_ZOOM_MECH_RACK,       // a toothed bar sliding along its length: a gear on it turns as it goes
    FUDE_ZOOM_MECH_PIN,        // a pin hinging whatever bodies are under it (one alone: to the ground, as a nail)
    FUDE_ZOOM_MECH_DRAWN,      // a drawing made into a body (props.h): its own shape, its material
    // A circuit's motor (circuit.h), with its shaft in its middle: it turns what is pinned there as the circuit
    // drives it, and is turned by it (coupling.h).
    FUDE_ZOOM_MECH_SHAFT,      // a circuit motor's shaft
    FUDE_ZOOM_MECH_TRACER,     // a pen on what moves under it, drawing its point's path as it plays (mechrun.h)
    // A hand crank: a disc on an axle of its own, with a handle hole at its rim, turned by hand as it plays.
    FUDE_ZOOM_MECH_CRANK,      // a hand crank
    // A circuit's solenoid (circuit.h): its plunger slides in and out of its coil, with a hole at its end, and is
    // pulled in as the circuit drives it (coupling.h).
    FUDE_ZOOM_MECH_PLUNGER,    // a circuit solenoid's plunger
    // (Added in 0.1.74: belts and chains, cams, ratchets, dampers and worms.)
    // A belt, or a chain (by its symbol's id), round the two turning parts its ends' middles are on (a sprocket, a
    // gear, a crank, a wheel on its axle). It turns one as far along its rim as the other, the same way; crossed (its
    // text says "crossed"), the other way.
    FUDE_ZOOM_MECH_BELT,       // a belt or a chain
    FUDE_ZOOM_MECH_SPROCKET,   // a toothed wheel a belt or chain runs round, on its own axle, its hole in its middle
    FUDE_ZOOM_MECH_CAM,        // an eccentric cam: a disc turning about a hole off its middle; what rests on it rises
                               // and falls
    // A cam's follower: a rod in a guide, its roller pressed along it (by its spring) onto what is in front of it.
    FUDE_ZOOM_MECH_FOLLOWER,   // a cam's follower
    FUDE_ZOOM_MECH_RATCHET,    // a ratchet wheel: sawtooth teeth, free counter-clockwise past a pawl, held clockwise
    FUDE_ZOOM_MECH_PAWL,       // a pawl: a lever on its own pivot, pressed onto the ratchet its tip is at
    // A dashpot between its two ends: it resists them moving apart or together, the harder the faster they go.
    FUDE_ZOOM_MECH_DAMPER,     // a dashpot
    // A worm turning at its text's speed ("60 rpm"), which turns a gear on it one tooth a turn and holds it: a gear
    // cannot turn a worm.
    FUDE_ZOOM_MECH_WORM        // a worm
} FUDE_ZOOM_MECH_;

// A hand crank's axle's friction (N·m) unless its text gives one: let go, it stops within a few degrees, as a pot's
// or an encoder's shaft does. A text of "0.05" makes it a flywheel's, nearly free.
#define FUDE_ZOOM_MECH_CRANK_FRICTION 2.0

// A part of the catalogue.
typedef struct {
    const c8* id;            // its symbol's id (symbol.c)
    u8        kind;          // a FUDE_ZOOM_MECH_
    u8        teeth;         // a gear's tooth count
    u8        hole_count;
    f32       holes[3][2];   // each hole's u, v, from -1 to 1 of its half sizes
    const c8* value;         // its text when placed
} fude_zoom_mech_part;

// Find a catalogue part: by its symbol's id ("gear 20T"), by a symbol kind, or as scene _s's object _object is (a live
// mechanism symbol). Each returns NULL when it is not a mechanism part. The parts are static and stay valid for ever.
const fude_zoom_mech_part* fude_zoom_mech_find(const c8* _id);
const fude_zoom_mech_part* fude_zoom_mech_of_kind(u32 _kind);
const fude_zoom_mech_part* fude_zoom_mech_of(const fude_zoom_scene* _s, u32 _object);
// Draws a part for half sizes _hw × _hh into _points and _parts (both cleared first): its polylines as symbol.h's
// parts, the first being its outline; _segments (16 to 128) is how finely curves are drawn. Returns how many parts.
u32  fude_zoom_mech_draw(const fude_zoom_mech_part* _part, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points, rde_arr* _parts);
// Returns a gear's pitch radius (in its own units) for its half width: where its teeth meet another gear's. For a part
// with no teeth, its half width.
f64  fude_zoom_mech_pitch(const fude_zoom_mech_part* _part, f64 _hw);
// Returns the speed a motor's or a worm's text says, in rpm, counter-clockwise when over 0 (_rpm when it gives no
// number): its first number with its sign (fude_zoom_circuit_value: "-30 rpm" turns clockwise, a hyphen inside a word
// is no minus), turned the other way when the text says clockwise. The way is a whole word (fude_zoom_circuit_word)
// in any of the apps' languages: clockwise "cw", "clockwise", "horario", "horário", "horaire", "時計回り", "右回り";
// counter-clockwise (the way it turns anyway) "ccw", "acw", "anticlockwise", "counterclockwise", "antihorario",
// "anti-horário", "antihoraire", "trigonométrique", "反時計回り", "左回り". So "30 rpm ccw" turns counter-clockwise, and
// "-30 rpm cw" too (a speed below 0, clockwise). The page's Value card reads a motor's way with it, and writes " cw".
f64  fude_zoom_mech_rpm(const c8* _text, f64 _rpm);

// --- the plan ----------------------------------------------------------------------------------------

// A body of the plan: one part as it was drawn. Home units are the home frame's.
typedef struct {
    u32                        object;
    const fude_zoom_mech_part* part;
    fude_zoom_v2               at;        // its middle (home units)
    f64                        angle;     // its turn (radians)
    f64                        hw, hh;    // its half sizes (home units)
    // What its text sets: a motor's or a worm's speed (turns a second; less than 0: clockwise), a weight's or a
    // crate's mass (kg), a spring's stiffness, a damper's strength, a belt's crossing (1: crossed), or a hand crank's
    // axle's friction (N·m).
    f64                        value;     // what its text sets, as above
    f64                        scale;     // home units to one of its own units (as its symbol is drawn)
    // A gear is turned this much more as it starts (radians) so that its teeth sit in the gaps they mesh with; a rack
    // is moved this far along itself (home units) so that its teeth sit in its gear's gaps.
    f64                        phase;     // a gear's extra turn as it starts (radians)
    f64                        shift;     // a rack's move along itself as it starts (home units)
    b8                         fixed;     // stays where it is (a pivot, a motor, a wall, a pulley, a fixed drawing)
    // A drawn body's: its convex pieces (`pieces` of the plan's pieces, from `piece`; their corners about `at`, as it
    // was drawn), its mass (kg), friction and bounce. An open line is fixed ground, made of thin pieces along it.
    u32                        piece, pieces;
    f64                        mass, friction, bounce;
    f64                        strength;  // its material's (MPa, sim/body.h; 0: the library parts', plastic)
    // A drive motor's torque when held still (N·m): its text's second number ("30 rpm 5Nm"), 10 when it gives none.
    f64                        torque;    // a drive motor's torque held still (N·m)
} fude_zoom_mech_body;

// A hinge between two bodies, or between a body and the ground.
typedef struct {
    u32          a, b;       // its bodies (b FUDE_ZOOM_NONE: the ground)
    fude_zoom_v2 at;         // where it is (home units)
    b8           motor;      // turned by a motor
    f64          speed;      // the motor's speed (radians a second)
    f64          torque;     // the motor's most torque (N·m)
    u32          shaft;      // the shaft body when it is on a circuit motor's shaft (FUDE_ZOOM_NONE: not)
    b8           crank;      // a hand crank's axle: turned by hand (mechrun.h), free while it is not held
} fude_zoom_mech_hinge;

// A spring.
typedef struct {
    u32          body;       // its own symbol (in the bodies: what is drawn, its colour and its width)
    u32          a, b;       // its bodies (FUDE_ZOOM_NONE: the ground)
    fude_zoom_v2 pa, pb;     // where it holds each (home units)
    f64          length;     // its length as it was drawn
    f64          stiffness;
} fude_zoom_mech_spring;

// Two gears meshing, a gear on a rack, or two turning parts joined by a belt.
typedef struct {
    u32 a, b;                // the gears (b is a rack when `rack`)
    // b turns -ratio times as fast as a (ratio: a's teeth over b's). For a rack, a's turn plus ratio times b's travel
    // is kept.
    f64 ratio;               // the ratio between a's and b's turns, as above
    f64 lower, upper;        // a rack's travel along its slide (home units) while a is over its teeth
    f64 period;              // one of a's teeth (radians): meshing again, it meshes a whole tooth from where it was
    b8  rack;
    b8  belt;                // a belt's or a chain's: no teeth meet (ratio: −a's radius over b's; crossed, +)
} fude_zoom_mech_mesh;

// A belt or a chain as drawn (its own symbol is the plan's body `body`) round bodies a and b, ra and rb its radii round
// each (home units), crossed or not; a chain is drawn with links, a belt smooth.
typedef struct {
    u32 body;
    u32 a, b;
    f64 ra, rb;
    b8  crossed, chain;
} fude_zoom_mech_belt;

// A damper (its own symbol is body `body`) between what its ends are on (FUDE_ZOOM_NONE: the ground there), at pa and
// pb. It pushes against them moving apart or together along it, as hard as `strength` times their mass ten times a
// second.
typedef struct {
    u32          body;
    u32          a, b;
    fude_zoom_v2 pa, pb;
    f64          strength;
} fude_zoom_mech_damper;

// A worm (`body`) turning a gear (`gear`) on its own axle (the plan's hinge `hinge`, driven): the gear turns a tooth
// for each turn of the worm, the way its side of the worm makes it (`side`: 1 over it, −1 under it).
typedef struct {
    u32 body;
    u32 gear;
    u32 hinge;
    f64 side;
    f64 speed;               // the worm's speed (turns a second; less than 0: the other way)
} fude_zoom_mech_worm;

// A pawl (`body`) pressed onto ratchet `ratchet` (FUDE_ZOOM_NONE: there is none at its tip).
typedef struct {
    u32 body;
    u32 ratchet;
} fude_zoom_mech_pawl;

// Sizes of some parts: how far a follower may slide each way, and where a cam's hole is.
#define FUDE_ZOOM_MECH_FOLLOWER_STROKE 0.4   // as a fraction of its half width
#define FUDE_ZOOM_MECH_CAM_ECCENTRIC   0.3   // a cam's hole off its middle, as a fraction of its radius
#define FUDE_ZOOM_MECH_RATCHET_TEETH   12u
// A worm's thread's lead, in its own units: one tooth of a 5-point module (π × 5).
#define FUDE_ZOOM_MECH_WORM_LEAD       (RDE_PI * 5.0)   // a worm thread's lead

// What slides: a slider along its rail, a rack along its length, a follower in its guide, a solenoid's plunger.
typedef struct {
    u32          body;
    fude_zoom_v2 at;         // where it is on its line (home units)
    fude_zoom_v2 axis;       // the line's direction (a unit vector)
    f64          lower, upper;   // how far it may go back and on from there (home units)
} fude_zoom_mech_slide;

// A rope.
typedef struct {
    u32          body;       // its own symbol (in the bodies: what is drawn)
    u32          a, b;       // the bodies its ends hold (FUDE_ZOOM_NONE: that end held still where it is)
    fude_zoom_v2 pa, pb;     // where (home units)
    f64          length;     // its length as drawn, which it never passes
    u32          over;       // the pulley its b end comes off (FUDE_ZOOM_NONE: none)
    u32          pair;       // the rope down that pulley's other side (FUDE_ZOOM_NONE: none); their total length kept
} fude_zoom_mech_rope;

// A tracer: on body `on` (the plan's; FUDE_ZOOM_NONE: on nothing that moves) at `at` (home units, as drawn); its symbol
// is the plan's body `body`.
typedef struct {
    u32          body;
    u32          on;
    fude_zoom_v2 at;
} fude_zoom_mech_tracer;

// A mechanism's plan: its bodies and everything that joins them, as worked out from the canvas.
typedef struct {
    rde_arr TYPE(fude_zoom_mech_body)   bodies;
    rde_arr TYPE(fude_zoom_mech_hinge)  hinges;
    rde_arr TYPE(fude_zoom_mech_spring) springs;
    rde_arr TYPE(fude_zoom_mech_mesh)   meshes;
    rde_arr TYPE(fude_zoom_mech_rope)   ropes;
    rde_arr TYPE(fude_zoom_mech_slide)  slides;
    rde_arr TYPE(fude_zoom_mech_tracer) tracers;
    rde_arr TYPE(fude_zoom_mech_belt)   belts;
    rde_arr TYPE(fude_zoom_mech_damper) dampers;
    rde_arr TYPE(fude_zoom_mech_worm)   worms;
    rde_arr TYPE(fude_zoom_mech_pawl)   pawls;
    rde_arr TYPE(fude_zoom_v2)          piece_points;   // the corners of drawn bodies' convex pieces, one after another
    rde_arr TYPE(u32)                   piece_counts;   // how many corners each piece has
    f64                                 unit;   // the smallest part's size (home units, such as a link's thickness)
} fude_zoom_mech_plan;

// Sets up an empty plan, and frees what one holds.
void fude_zoom_mech_plan_init(fude_zoom_mech_plan* _p);
void fude_zoom_mech_plan_destroy(fude_zoom_mech_plan* _p);
// Makes _to a copy of _from, with its own copy of everything.
void fude_zoom_mech_plan_copy(fude_zoom_mech_plan* _to, const fude_zoom_mech_plan* _from);
// Works out the plan from what is on the canvas: every part that is alive and shown, or only the objects _scope marks
// (one byte for each object; NULL for all). A circuit's motor, servo, stepper, encoder or pot among them becomes a
// shaft (FUDE_ZOOM_MECH_SHAFT's part, fixed), and a solenoid becomes its plunger (FUDE_ZOOM_MECH_PLUNGER's, sliding
// along it as far in as its stroke). The plan is cleared first. Returns how many bodies it has.
u32  fude_zoom_mech_plan_build(fude_zoom_mech_plan* _p, const fude_zoom_scene* _s, const u8* _scope);
// Returns a library part's size (home units), the plan's unit being the smallest part's: a long part's (a link, a
// rope, a rail, a rack, a belt) its half height, or 8% of its half length when that is more; any other's a quarter of
// its smaller side. A sliding part's strength is worked out from its own (mechrun.c).
f64  fude_zoom_mech_body_size(const fude_zoom_mech_body* _b);
// Returns a pulley's radius where ropes run (home units).
f64  fude_zoom_mech_pulley_radius(const fude_zoom_mech_body* _b);
// Returns the radius a belt or a chain runs round on a turning part (a sprocket's, a gear's pitch circle, a crank's,
// a wheel's); 0 for a part no belt goes round.
f64  fude_zoom_mech_belt_radius(const fude_zoom_mech_body* _b);
// Gives the radii of a ratchet's teeth's tips and roots (home units).
void fude_zoom_mech_ratchet_radii(const fude_zoom_mech_body* _b, f64* _tip, f64* _root);
// Returns how far above its middle a rack's pitch line is, in its own units, for half height _hh: two modules below
// its top, so its teeth's tips are a module above the line.
f64  fude_zoom_mech_rack_pitch_line(f64 _hh);
// Returns a rack's teeth's module (in its own units, for half height _hh): a third of it, so that the library's rack
// has the 5-point module of its gears.
f64  fude_zoom_mech_rack_module(f64 _hh);
// Gives a rack's teeth as drawn (home units, along it from its middle): its first tooth's middle (the one at its right
// end) in *_first, the gap between teeth toward its left in *_pitch, and its pitch line's height over its middle in
// *_line. Returns how many teeth.
u32  fude_zoom_mech_rack_teeth(const fude_zoom_mech_body* _rack, f64* _first, f64* _pitch, f64* _line);
// Returns where gear _gear's teeth are toward world angle _toward, as it stands (its angle and phase), as a fraction of
// a tooth from 0 to 1: 0 is a tooth's middle, ½ a gap's.
f64  fude_zoom_mech_tooth_at(const fude_zoom_mech_body* _gear, f64 _toward);

#endif
