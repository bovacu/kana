// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_MECH_H
#define FUDE_ZOOM_MECH_H

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/scene.h"

// ===========================================================================
// MECHANISMS (Sketching's Mechanisms topic): linkages, gears, springs and
// weights drawn as the diagram library's symbols (symbol.h's Mechanisms family)
// and run as rigid bodies (RDE's 2D physics: mechsim, page.c).
//
// A part's HOLES are where it is pinned: a hole over another part's hole is a
// hinge between them; over a fixed pivot's, the part turns about the ground
// there; over a motor's, it is turned (the motor's text: its speed, "30 rpm").
// A spring's two ends pull the parts whose holes they are on toward its length
// as it was drawn. Gears whose pitch circles touch turn each other (their teeth:
// each gear's count, by its id — all of one tooth size, so any two mesh), the
// smaller faster; a gear (or a pulley) nothing holds turns on an axle of its
// own where it is. A gear on a rack moves it as far as the gear is over its
// teeth: there the rack stops and lets the gear go on (turned back, it takes
// the rack back). Meshed gears start with their teeth in each other's gaps (a
// gear only on its axle turned the little it needs, a rack slid along itself
// the little it needs). A weight or a crate falls; walls stay.
//
// A ROPE holds what its ends are on — a body (anywhere on it), or still: a
// pivot, a wall, a pulley's rim, or where it was drawn — and never stretches
// past its length as drawn (it goes slack). Two ropes down either side of one
// PULLEY are one rope over it: what one lets down, the other takes up (their
// lengths together kept), the pulley turning with them.
//
// What is worked out here is the plan (which bodies, which hinges, springs,
// ropes, gear meshes: pure, tested) and the ropes' sums (pure, tested); running
// it is mechrun.h's.
// ===========================================================================

typedef enum {
    FUDE_ZOOM_MECH_LINK = 0,   // a bar: a hole each end
    FUDE_ZOOM_MECH_PLATE,      // a triangle: three holes
    FUDE_ZOOM_MECH_GEAR,       // a spur gear: its hole in its middle, its teeth its part's
    FUDE_ZOOM_MECH_PIVOT,      // a fixed pivot (the ground)
    FUDE_ZOOM_MECH_MOTOR,      // a fixed pivot that turns what is pinned to it
    FUDE_ZOOM_MECH_SPRING,     // between its two ends
    FUDE_ZOOM_MECH_WEIGHT,     // a ball: it falls, it rolls
    FUDE_ZOOM_MECH_WHEEL,      // a wheel: its hole in its middle, it rolls
    FUDE_ZOOM_MECH_WALL,       // fixed, solid
    FUDE_ZOOM_MECH_PULLEY,     // a wheel ropes run over: on its own axle where it is
    FUDE_ZOOM_MECH_ROPE,       // between its two ends: never longer than drawn
    FUDE_ZOOM_MECH_CRATE,      // a box: it falls, it hangs (its mass its text)
    FUDE_ZOOM_MECH_RAIL,       // a fixed guide: what slides along it, between its ends
    FUDE_ZOOM_MECH_SLIDER,     // a block that slides along the rail it is on: its hole in its middle (a piston)
    FUDE_ZOOM_MECH_RACK,       // a toothed bar sliding along its length: a gear on it turns as it goes
    FUDE_ZOOM_MECH_PIN,        // a pin: whatever bodies are under it hinged there (one alone: to the ground, a nail)
    FUDE_ZOOM_MECH_DRAWN,      // a drawing made a body (props.h): its own shape, its material
    FUDE_ZOOM_MECH_SHAFT       // a circuit's motor (circuit.h): its shaft in its middle — what is pinned there it turns, as the
                               // circuit drives it, and is turned by (coupling.h)
} FUDE_ZOOM_MECH_;

typedef struct {
    const c8* id;            // its symbol's id (symbol.c)
    u8        kind;          // FUDE_ZOOM_MECH_
    u8        teeth;         // a gear's
    u8        hole_count;
    f32       holes[3][2];   // u, v from -1 to 1 of its half sizes
    const c8* value;         // its text to begin with
} fude_zoom_mech_part;

const fude_zoom_mech_part* fude_zoom_mech_find(const c8* _id);
const fude_zoom_mech_part* fude_zoom_mech_of_kind(u32 _kind);
const fude_zoom_mech_part* fude_zoom_mech_of(const fude_zoom_scene* _s, u32 _object);
// Its polylines (symbol.h's parts, the first its outline) for half sizes _hw × _hh. How many parts.
u32  fude_zoom_mech_draw(const fude_zoom_mech_part* _part, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points, rde_arr* _parts);
// A gear's pitch radius (own units) for its half width: where its teeth meet another's.
f64  fude_zoom_mech_pitch(const fude_zoom_mech_part* _part, f64 _hw);

// --- the plan ----------------------------------------------------------------------------------------

typedef struct {
    u32                        object;
    const fude_zoom_mech_part* part;
    fude_zoom_v2               at;        // its middle (the home frame's units)
    f64                        angle;     // its turn (radians)
    f64                        hw, hh;    // its half sizes (home units)
    f64                        value;     // a motor's speed (turns a second), a weight's or a crate's mass (kg), a spring's stiffness
    f64                        scale;     // home units its own unit (as its symbol is drawn)
    f64                        phase;     // a gear's: turned this much more as it starts (radians): its teeth in the gaps they mesh with
    f64                        shift;     // a rack's: moved this far along itself as it starts (home units): its teeth in its gear's gaps
    b8                         fixed;     // stays where it is (a pivot, a motor, a wall, a pulley, a fixed drawing)
    // A drawn body's: its convex pieces (the plan's pieces: from piece, pieces of them; their corners about at, as it
    // was drawn), its mass (kg), friction, bounce; an open line (fixed ground: thin pieces along it).
    u32                        piece, pieces;
    f64                        mass, friction, bounce;
} fude_zoom_mech_body;

typedef struct {
    u32          a, b;       // bodies (b FUDE_ZOOM_NONE: the ground)
    fude_zoom_v2 at;         // where (home units)
    b8           motor;      // turned by a motor
    f64          speed;      // ...this fast (radians a second)
    u32          shaft;      // on a circuit's motor's shaft: that body (FUDE_ZOOM_NONE: none)
} fude_zoom_mech_hinge;

typedef struct {
    u32          a, b;       // bodies (FUDE_ZOOM_NONE: the ground)
    fude_zoom_v2 pa, pb;     // where it holds each (home units)
    f64          length;     // as it was drawn
    f64          stiffness;
} fude_zoom_mech_spring;

typedef struct {
    u32 a, b;                // gears (b: a rack, when rack)
    f64 ratio;               // b turns -ratio times as fast as a (a's teeth over b's); a rack's: a's turn + ratio·b's travel kept
    f64 lower, upper;        // a rack's: its travel (home units, its slide's) while a is over its teeth
    f64 period;              // a's tooth (radians): meshing again, a whole one from where it was
    b8  rack;
} fude_zoom_mech_mesh;

// What slides: a slider along its rail, a rack along its length.
typedef struct {
    u32          body;
    fude_zoom_v2 at;         // where it is on its line (home units)
    fude_zoom_v2 axis;       // the line's way (unit)
    f64          lower, upper;   // how far it may go back and on from there (home units)
} fude_zoom_mech_slide;

typedef struct {
    u32          body;       // its own symbol (in the bodies: what is drawn)
    u32          a, b;       // the bodies its ends hold (FUDE_ZOOM_NONE: that end held still where it is)
    fude_zoom_v2 pa, pb;     // where (home units)
    f64          length;     // as it was drawn: never longer
    u32          over;       // the pulley its b end comes off (FUDE_ZOOM_NONE: none)
    u32          pair;       // the rope down that pulley's other side (FUDE_ZOOM_NONE: none): their lengths together kept
} fude_zoom_mech_rope;

typedef struct {
    rde_arr TYPE(fude_zoom_mech_body)   bodies;
    rde_arr TYPE(fude_zoom_mech_hinge)  hinges;
    rde_arr TYPE(fude_zoom_mech_spring) springs;
    rde_arr TYPE(fude_zoom_mech_mesh)   meshes;
    rde_arr TYPE(fude_zoom_mech_rope)   ropes;
    rde_arr TYPE(fude_zoom_mech_slide)  slides;
    rde_arr TYPE(fude_zoom_v2)          piece_points;   // drawn bodies' convex pieces' corners, in turn
    rde_arr TYPE(u32)                   piece_counts;   // ...how many each
    f64                                 unit;   // the parts' size (home units: a link's thickness, the smallest)
} fude_zoom_mech_plan;

void fude_zoom_mech_plan_init(fude_zoom_mech_plan* _p);
void fude_zoom_mech_plan_destroy(fude_zoom_mech_plan* _p);
// _to as _from is (its own copy of everything).
void fude_zoom_mech_plan_copy(fude_zoom_mech_plan* _to, const fude_zoom_mech_plan* _from);
// Worked out from what is on the canvas (every part alive and shown; _scope: only the objects it marks — one byte an
// object, NULL: all) — a circuit's motor among them a shaft (its part FUDE_ZOOM_MECH_SHAFT's, fixed). How many bodies.
u32  fude_zoom_mech_plan_build(fude_zoom_mech_plan* _p, const fude_zoom_scene* _s, const u8* _scope);
// A pulley's radius where ropes run (home units).
f64  fude_zoom_mech_pulley_radius(const fude_zoom_mech_body* _b);
// A rack's pitch line: how far above its middle (its own units, for half height _hh), and its teeth's pitch for a
// gear of module _module (its teeth across its pitch circle: 2 × pitch radius / teeth).
f64  fude_zoom_mech_rack_pitch_line(f64 _hh);
// A rack's teeth's module (its own units, for half height _hh): a third of it (the library's rack: 5 points, as its gears').
f64  fude_zoom_mech_rack_module(f64 _hh);
// A rack's teeth (home units, along it from its middle, as drawn): its first one's middle (its right end's), the next
// ones _pitch apart toward its left, how many; its pitch line over its middle (*_line).
u32  fude_zoom_mech_rack_teeth(const fude_zoom_mech_body* _rack, f64* _first, f64* _pitch, f64* _line);
// Where gear _gear's teeth are toward world angle _toward, as it stands (its angle, its phase): 0 a tooth's middle, ½ a
// gap's (a fraction of a tooth, 0 to 1).
f64  fude_zoom_mech_tooth_at(const fude_zoom_mech_body* _gear, f64 _toward);

// --- ropes: kept from stretching ---------------------------------------------------------------------

// A rope's end that a body holds: how fast it moves, the way the rope pulls it back along (unit: from what it hangs
// from toward it), 1 / its body's mass (0: held still); what keeping the rope does to it.
typedef struct {
    fude_zoom_v2 v, u;
    f64          w;
    fude_zoom_v2 dv, dp;     // its change of speed, of place
} fude_zoom_mech_pull;

// A rope (its ends: _n of them — a rope between two bodies, two; a pair over a pulley, its two loose ends) _stretch past
// its length (or their lengths together past theirs; at most 0: slack): its ends' speeds outward taken away, shared by
// their masses, and their places brought back to its length (_t's dv, dp). An inextensible rope, as an impulse.
void fude_zoom_mech_rope_keep(fude_zoom_mech_pull* _t, u32 _n, f64 _stretch);
// The gears' turning speeds kept to each other's (_omega: each body's, radians a second; those driven — a motor's,
// or the first of each train — set the rest, through their meshes).
void fude_zoom_mech_gears(const fude_zoom_mech_plan* _p, f64* _omega, const b8* _driven);

#endif
