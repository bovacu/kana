// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PROPS_H
#define FUDE_ZOOM_PROPS_H

#include "rde.h"
#include "zoom/scene.h"

// ===========================================================================
// PROPERTIES of a drawn thing: an attribute object (shape.h's PROPS) naming
// the thing by its id, its kind, its numbers — so a drawing stays a drawing
// and gains what it is to a simulation. It follows the thing when it is made
// again (fude_zoom_props_remap); it is nothing when the thing is gone.
//
// Kinds: a BODY (Mechanisms: docs/simulation_architecture.md §7) — its
// material (sim/body.h's), whether it is fixed, its mass (0: its area's), its
// friction and bounce (negative: its material's); a LIGHT (Electronics) — the
// colour a logic probe lights in; LIMITS (Electronics: limits.h) — what a
// part takes at most (W, A, V, the wrong way round V; 0: none), as set on
// its card, and the real part they are (-1: its own). More kinds as they come (a board's outline, a
// sensor's zone…), each its own numbers. Copied with their thing (select.c).
// ===========================================================================

typedef enum {
    FUDE_ZOOM_PROPS_BODY = 1,
    FUDE_ZOOM_PROPS_LIGHT,
    FUDE_ZOOM_PROPS_LIMITS
} FUDE_ZOOM_PROPS_;

typedef struct {
    u32 material;   // sim/body.h's materials' index
    b8  fixed;      // stays where it is (the ground: a ramp, a wall, a hook)
    f64 mass;       // kg (0: from its area, its material, 10 mm thick)
    f64 friction;   // 0..1 (negative: its material's)
    f64 bounce;     // 0..1 (negative: its material's)
} fude_zoom_body_props;

#define FUDE_ZOOM_PROPS_NUMBERS 8u   // a body's: kind, the id's two halves, material, fixed, mass, friction, bounce

// The attribute of kind _kind on object _target (FUDE_ZOOM_NONE: none).
u32 fude_zoom_props_find(const fude_zoom_scene* _s, u32 _target, u8 _kind);
// An attribute's kind (0: not one) and the object it is on (FUDE_ZOOM_NONE: gone).
u8  fude_zoom_props_kind(const fude_zoom_scene* _s, u32 _props);
u32 fude_zoom_props_target(const fude_zoom_scene* _s, u32 _props);
// A body's properties from its attribute. False: not a body's.
b8  fude_zoom_props_body(const fude_zoom_scene* _s, u32 _props, fude_zoom_body_props* _out);
// A new body attribute on _target (in its frame, on its layer). Its index. (Its old one, if any, the caller lets go.)
u32 fude_zoom_props_add_body(fude_zoom_scene* _s, u32 _target, const fude_zoom_body_props* _body);
// A light's colour from its attribute. False: not a light's.
b8  fude_zoom_props_light(const fude_zoom_scene* _s, u32 _props, rde_color* _out);
// A new light attribute on _target, of colour _color. Its index. (Its old one, if any, the caller lets go.)
u32 fude_zoom_props_add_light(fude_zoom_scene* _s, u32 _target, rde_color _color);
// A part's limits from its attribute (4 numbers into _most) and the real part they are (-1: its own). False: not limits'.
b8  fude_zoom_props_limits(const fude_zoom_scene* _s, u32 _props, f64* _most, i32* _preset);
// A new limits attribute on _target. Its index. (Its old one, if any, the caller lets go.)
u32 fude_zoom_props_add_limits(fude_zoom_scene* _s, u32 _target, const f64* _most, i32 _preset);
// _props again on what its thing is now (its id _old[i] now _new[i]): the new attribute, or FUDE_ZOOM_NONE (not on
// any of them). The caller lets the old one go.
u32 fude_zoom_props_remap(fude_zoom_scene* _s, u32 _props, const fude_zoom_id* _old, const fude_zoom_id* _new, u32 _n);

#endif
