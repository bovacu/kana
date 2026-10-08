// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SIM_BODY_H
#define FUDE_SIM_BODY_H

#include "rde.h"

// ===========================================================================
// BODIES' SHAPES (the mechanical domain: docs/simulation_architecture.md §7):
// an outline as drawn made into what rigid-body physics takes — simplified,
// cut into convex pieces of at most eight corners (a concave shape is several
// pieces of one body) — its area, its centre, its turn's inertia; and the
// materials a body can be of. Pure: tested on its own.
//
// Units are whatever the outline's are (millimetres on Sketching's canvas);
// a material's density is kg/m³, a body taken as FUDE_SIM_BODY_THICKNESS
// thick (a drawing has no depth).
// ===========================================================================

#define FUDE_SIM_BODY_THICKNESS 0.01   // metres: a drawn body as a plate 10 mm thick
#define FUDE_SIM_BODY_CORNERS   8u     // a convex piece's corners, at most (the physics' polygons)

typedef struct {
    f64 x, y;
} fude_sim_v2;

typedef struct {
    const c8* id;          // "wood", "steel"...
    const c8* name;        // in English (the app's in its language)
    f64       density;     // kg/m³
    f64       friction;    // 0..1
    f64       bounce;      // 0..1 (restitution)
    f64       strength;    // MPa: what a square millimetre of it holds pulled before it gives (its tensile strength; shear ⅗ of it)
} fude_sim_material;

u32                      fude_sim_material_count(void);
const fude_sim_material* fude_sim_material_at(u32 _i);
const fude_sim_material* fude_sim_material_find(const c8* _id);

// A polygon's area (positive: counter-clockwise), its centre (of its area), and its second moment of area about _about
// (its inertia a unit of mass a unit of area: multiply by its density).
f64         fude_sim_polygon_area(const fude_sim_v2* _p, u32 _n);
fude_sim_v2 fude_sim_polygon_centroid(const fude_sim_v2* _p, u32 _n);
f64         fude_sim_polygon_inertia(const fude_sim_v2* _p, u32 _n, fude_sim_v2 _about);
// Is _q inside it (even-odd)?
b8          fude_sim_polygon_inside(const fude_sim_v2* _p, u32 _n, fude_sim_v2 _q);
// Is it convex (its turns all one way; collinear corners allowed)?
b8          fude_sim_polygon_convex(const fude_sim_v2* _p, u32 _n);

// A closed outline simplified: corners no further than _tolerance from the line their neighbours make dropped
// (Douglas–Peucker, round the loop), repeated points and its last point equal to its first dropped, counter-clockwise.
// Into _out (_max at most). How many (fewer than 3: nothing left of it).
u32 fude_sim_outline_simplify(const fude_sim_v2* _in, u32 _n, f64 _tolerance, fude_sim_v2* _out, u32 _max);

// An open line simplified as a closed outline is (its ends kept). How many points (fewer than 2: nothing left).
u32 fude_sim_polyline_simplify(const fude_sim_v2* _in, u32 _n, f64 _tolerance, fude_sim_v2* _out, u32 _max);

// A simple polygon (counter-clockwise, no crossings) cut into convex pieces of at most _corners corners (3..8):
// triangulated (ear clipping), then neighbours merged while what they make stays convex (Hertel–Mehlhorn). Each
// piece's corners into _points (fude_sim_v2: its own, counter-clockwise), how many each into _counts (u32). How many
// pieces (0: it could not be: too few corners, or it crosses itself).
u32 fude_sim_convex_pieces(const fude_sim_v2* _p, u32 _n, u32 _corners, rde_arr* _points, rde_arr* _counts);

#endif
