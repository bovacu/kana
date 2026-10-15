// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PROPS_H
#define FUDE_ZOOM_PROPS_H

#include "rde.h"
#include "zoom/scene.h"

// ===========================================================================
// Properties: what a drawn thing is to a simulation, kept as an attribute object (a PROPS shape, shape.h) that names
// the thing by its id and holds a kind and some numbers. A drawing stays a drawing and gains what it is to the
// simulation.
//
// Where it sits: fude/zoom, over scene.h. The page (page_mech.c, page_circuit.c), the selection (select.c, which copies
// an attribute with its thing), the examples, and the circuit and mechanism compilers (circuit_build.c, mech.c) use
// it.
// Its main types and their lifecycle: an attribute is an ordinary object of the scene, which owns it; this module
// only reads and makes them. Making one is not an undo step by itself: the caller records it in the history.
// fude_zoom_body_props is a plain value.
// Threads: main thread only, like the scene.
// Files: attributes are saved with the canvas as shapes (zfile.h); their numbers are laid out as below.
// Design:
//   - Each attribute is a separate object rather than a field of the thing, so any thing (a stroke, a shape) can carry
//     one without changing how things are stored, and older files simply have none.
//   - It follows the thing when the thing is made again (fude_zoom_props_remap), and means nothing once the thing is
//     gone.
//   - Kinds so far: a BODY (Mechanisms: docs/simulation_architecture.md §7) holds its material (one of sim/body.h's),
//     whether it is fixed, its mass (0 means worked out from its area) and its friction and bounce (negative means
//     its material's). A LIGHT (Electronics) holds the colour a logic probe lights in. LIMITS (Electronics: limits.h)
//     hold what a part takes at most, as set on its card: W, A, V and V the wrong way round (0 means no limit), and
//     the real part they come from (-1 means the part's own). More kinds will come as needed (a board's outline, a
//     sensor's zone...), each with its own numbers.
// ===========================================================================

// An attribute's kind, its first number.
typedef enum {
    FUDE_ZOOM_PROPS_BODY = 1,
    FUDE_ZOOM_PROPS_LIGHT,
    FUDE_ZOOM_PROPS_LIMITS
} FUDE_ZOOM_PROPS_;

// What a body is made of and how it moves.
typedef struct {
    u32 material;   // an index into sim/body.h's materials
    b8  fixed;      // it stays where it is, as the ground does (a ramp, a wall, a hook)
    f64 mass;       // in kg; 0 means worked out from its area and material, 10 mm thick
    f64 friction;   // from 0 to 1; negative means its material's
    f64 bounce;     // from 0 to 1; negative means its material's
} fude_zoom_body_props;

// The most numbers an attribute holds. A body uses all 8: its kind, the two halves of its thing's id, its material,
// fixed, mass, friction and bounce.
#define FUDE_ZOOM_PROPS_NUMBERS 8u   // a body uses all 8 (see above)

// Returns the index of the first attribute of kind _kind on object _target, or FUDE_ZOOM_NONE when it has none.
u32 fude_zoom_props_find(const fude_zoom_scene* _s, u32 _target, u8 _kind);
// Adds to _out (u32) the index of every attribute object (a PROPS shape, alive or not: one deleted may be brought
// back) among the scene's objects from *_looked on, in order, and sets *_looked to how many objects the scene has.
// Called again as objects are made, it looks at each object once: a scene's objects are only ever added, and an
// object's kind never changes. A *_looked past the scene's objects (another scene's) starts again, _out cleared.
void fude_zoom_props_gather(const fude_zoom_scene* _s, u32* _looked, rde_arr* _out);
// fude_zoom_props_kind returns an attribute's kind, or 0 when _props is not a live attribute. fude_zoom_props_target
// returns the index of the object it is on, or FUDE_ZOOM_NONE when that object is gone or dead.
u8  fude_zoom_props_kind(const fude_zoom_scene* _s, u32 _props);
u32 fude_zoom_props_target(const fude_zoom_scene* _s, u32 _props);
// Reads a body's properties from its attribute into _out. Returns false when _props is not a body's attribute.
b8  fude_zoom_props_body(const fude_zoom_scene* _s, u32 _props, fude_zoom_body_props* _out);
// Makes a new body attribute on _target, in its frame and on its layer, and returns its index. When _target already
// had one, the caller erases the old one.
u32 fude_zoom_props_add_body(fude_zoom_scene* _s, u32 _target, const fude_zoom_body_props* _body);
// Reads a light's colour from its attribute into _out. Returns false when _props is not a light's attribute.
b8  fude_zoom_props_light(const fude_zoom_scene* _s, u32 _props, rde_color* _out);
// Makes a new light attribute of colour _color on _target and returns its index. When _target already had one, the
// caller erases the old one.
u32 fude_zoom_props_add_light(fude_zoom_scene* _s, u32 _target, rde_color _color);
// Reads a part's limits from its attribute: 4 numbers into _most (W, A, V, reverse V; 0 means none) and the real part
// they come from into _preset (-1 means its own). Returns false when _props is not a limits attribute.
b8  fude_zoom_props_limits(const fude_zoom_scene* _s, u32 _props, f64* _most, i32* _preset);
// Makes a new limits attribute on _target from 4 numbers in _most, and returns its index. When _target already had
// one, the caller erases the old one.
u32 fude_zoom_props_add_limits(fude_zoom_scene* _s, u32 _target, const f64* _most, i32 _preset);
// Puts attribute _props again on its thing after the thing was made again: when its thing's id is _old[i], a copy is
// made on the object whose id is _new[i]. Returns the new attribute's index, or FUDE_ZOOM_NONE when its thing is not
// among _old or its new object is not found. The caller erases the old attribute.
u32 fude_zoom_props_remap(fude_zoom_scene* _s, u32 _props, const fude_zoom_id* _old, const fude_zoom_id* _new, u32 _n);

#endif
