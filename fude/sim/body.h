// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SIM_BODY_H
#define FUDE_SIM_BODY_H

#include "rde.h"

// ===========================================================================
// Bodies' shapes: turns an outline as it was drawn into what rigid-body physics takes, and lists the materials a body
// can be made of.
//
// Where it sits: `fude/sim`, the mechanical domain (docs/simulation_architecture.md §7). zoom/mech.c, zoom/mechrun.c
// and zoom/bodyform.c use it to make drawings into bodies, and tests/sim tests it on its own.
// An outline is simplified, then cut into convex pieces of at most FUDE_SIM_BODY_CORNERS corners (a concave shape
// becomes several pieces of one body); its area, centre and moment of inertia are measured here too.
// Main types: plain values (fude_sim_v2, fude_sim_material); the materials are a constant table.
// Threads: pure functions over the caller's data and a constant table, so they are safe on any thread; today they
// are called on the app's main thread.
//
// Units are whatever the outline's are (millimetres on a canvas app's canvas). A material's density is in kg/m³, and a
// body is taken to be a plate FUDE_SIM_BODY_THICKNESS thick, because a drawing has no depth.
// ===========================================================================

#define FUDE_SIM_BODY_THICKNESS 0.01   // metres: a drawn body is taken as a plate 10 mm thick
#define FUDE_SIM_BODY_CORNERS   8u     // the most corners of a convex piece (the physics' polygons have at most 8)

// A point of an outline. It is RDE's f64 vector (x, y), so rde_math's f64 geometry takes outlines as they are.
typedef rde_vec_2D fude_sim_v2;

// A material a body can be made of.
typedef struct {
    const c8* id;          // "wood", "steel"...
    const c8* name;        // its name in English (the app shows its own translation)
    f64       density;     // kg/m³
    f64       friction;    // 0..1
    f64       bounce;      // 0..1 (its restitution)
    f64       strength;    // MPa (N/mm²) it holds pulled before it gives: its tensile strength; shear takes ⅗ of it
} fude_sim_material;

// The materials (wood, steel, aluminium, rubber, plastic, glass, ice, foam). fude_sim_material_at returns NULL past
// the end, and fude_sim_material_find returns NULL when no material has id _id. They are static and stay valid for
// ever.
u32                      fude_sim_material_count(void);
const fude_sim_material* fude_sim_material_at(u32 _i);
const fude_sim_material* fude_sim_material_find(const c8* _id);

// A polygon's measures. fude_sim_polygon_area returns its signed area (positive when it runs counter-clockwise; 0 for
// fewer than 3 corners). fude_sim_polygon_centroid returns the centre of its area (the mean of its corners when it has
// no area). Both are worked out about its first corner, so that a small shape far from the origin keeps its precision.
// fude_sim_polygon_inertia returns its polar second moment of area about _about, always positive: its moment of
// inertia with one unit of mass on each unit of area, so multiply it by the mass per unit of area.
f64         fude_sim_polygon_area(const fude_sim_v2* _p, u32 _n);
fude_sim_v2 fude_sim_polygon_centroid(const fude_sim_v2* _p, u32 _n);
f64         fude_sim_polygon_inertia(const fude_sim_v2* _p, u32 _n, fude_sim_v2 _about);
// Returns whether point _q is inside the polygon, by the even-odd rule (rde_math's: never for fewer than 3 corners).
// The area above is not rde_math's shoelace on purpose: it is summed about the first corner, for the precision.
b8          fude_sim_polygon_inside(const fude_sim_v2* _p, u32 _n, fude_sim_v2 _q);
// Returns whether the polygon is convex: all its turns go the same way. Corners in a straight line are allowed. False
// for fewer than 3 corners.
b8          fude_sim_polygon_convex(const fude_sim_v2* _p, u32 _n);

// Simplifies a closed outline. Repeated points, and a last point equal to the first, are dropped; then corners no
// further than _tolerance from the line their neighbours make are dropped (Douglas–Peucker, run round the loop: split
// at its first point and the point furthest from it). The result runs counter-clockwise and is written into _out, at
// most _max points. Returns how many; 0 when fewer than 3 are left.
u32 fude_sim_outline_simplify(const fude_sim_v2* _in, u32 _n, f64 _tolerance, fude_sim_v2* _out, u32 _max);

// Simplifies an open line the same way, keeping both its ends. Returns how many points; 0 when fewer than 2 are left.
u32 fude_sim_polyline_simplify(const fude_sim_v2* _in, u32 _n, f64 _tolerance, fude_sim_v2* _out, u32 _max);

// Cuts a simple polygon (counter-clockwise, not crossing itself) into convex pieces of at most _corners corners (3 to
// 8; more is taken as 8). It is triangulated by ear clipping, then neighbouring pieces are merged while the result
// stays convex (Hertel–Mehlhorn); corners in a straight line are dropped. Each piece's corners are appended to
// _points (fude_sim_v2, counter-clockwise) and its corner count to _counts (u32); neither array is cleared first.
// Returns how many pieces; 0 when it cannot be done (too few corners, a polygon that crosses itself or runs clockwise).
u32 fude_sim_convex_pieces(const fude_sim_v2* _p, u32 _n, u32 _corners, rde_arr* _points, rde_arr* _counts);

#endif
