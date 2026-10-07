// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_CONNECT
#define FUDE_ZOOM_CONNECT

#include "rde.h"
#include "zoom/scene.h"
#include "zoom/snap.h"

// ===========================================================================
// Connectors (research §2.9): an arrow (shape.h's ARROW) that keeps the ids of
// the two things it joins, and follows them when they move.
//
// It aims at each end's middle and stops on its outline — a shape's outline,
// a text's or a picture's box, anything else's box. When things it joins are
// moved, it is moved too, in the same step: turned, stretched and slid so its
// ends land where they should (its bends, if any, carried along), which is
// one undo with the move, and its width does not change with the stretch (the
// renderer draws an arrow's width in its frame's units).
// ===========================================================================

// Where the line from _object's middle towards _toward leaves its outline (frame units).
fude_zoom_v2 fude_zoom_connect_edge(const fude_zoom_scene* _s, u32 _object, fude_zoom_v2 _toward);
// The middle of _object (its box's), frame units.
fude_zoom_v2 fude_zoom_connect_middle(const fude_zoom_scene* _s, u32 _object);

// The arrows in _frame joined to any of _moved (object indexes, just moved,
// themselves not among them): each one's index, place before and place after
// appended (and the places set). How many.
u32 fude_zoom_connect_follow(fude_zoom_scene* _s, u32 _frame, const u32* _moved, u32 _count, rde_arr* _objects, rde_arr* _before, rde_arr* _after);

// A new connector from _a to _b in _frame (frame units), joining _from and _to
// (object indexes, FUDE_ZOOM_NONE: a loose end there), its heads and look.
u32 fude_zoom_connect_add(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _a, fude_zoom_v2 _b, u32 _from, u32 _to, u32 _heads, rde_color _color, f32 _radius);

// _arrow again with the ids _old[i] it joins changed to _new[i], on _layer:
// the new arrow (where it was, in its order), or FUDE_ZOOM_NONE when nothing
// changes (or it is not an arrow). The caller lets the old one go.
// Driving (research §2.10, item 5, constraint-lite): everything in _frame
// with an end or a corner at _from (within _tol, frame units) moved to _to —
// a line's or a dimension's end, a polygon's corner, a rectangle's corner (the
// one across from it staying). Each a new object in place of the old, both
// appended to _died and _born (the caller pushes the one step). _skip: left
// alone. How many.
u32 fude_zoom_connect_drive(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _from, fude_zoom_v2 _to, f64 _tol, u32 _skip, rde_arr* _died, rde_arr* _born);

u32 fude_zoom_connect_remap(fude_zoom_scene* _s, u32 _arrow, const fude_zoom_id* _old, const fude_zoom_id* _new, u32 _n, u16 _layer);

// DIMENSIONS THAT FOLLOW: a dimension drawn from a point of a thing to a point of a thing (snap.h's: an end, a
// corner, a centre) keeps which (shape.h: FUDE_ZOOM_DIMENSION_REFS), and a circle's size (RADIAL) its circle's id.
// When those things move, fude_zoom_connect_follow moves it with them (its ends where their points are now, in the
// same step); made again in place of them (stretched, sized), fude_zoom_connect_dim_remap measures it again.
//
// A point of a thing (snap.h's key): a shape's outline corner or end (fude_zoom_shape_outline's at one segment), a
// stroke's first (0) or last (1) point, or (FUDE_ZOOM_SNAP_CENTRE_KEY) its centre. Where it is now (frame units).
// False: gone, or no such point.
b8  fude_zoom_connect_point(const fude_zoom_scene* _s, u32 _object, i32 _key, fude_zoom_v2* _out);
// A circle's or an arc's centre and radius now (frame units). False: not one.
b8  fude_zoom_connect_round(const fude_zoom_scene* _s, u32 _object, fude_zoom_v2* _centre, f64* _radius);
// A new dimension from _a to _b in _frame (frame units), its line _offset to their left, joined to point _a_key of
// _a_obj and _b_key of _b_obj (FUDE_ZOOM_NONE: not joined there).
u32 fude_zoom_connect_dimension(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _offset, u32 _a_obj, i32 _a_key, u32 _b_obj, i32 _b_key,
                                rde_color _color, f32 _radius);
// _dim (a DIMENSION or a RADIAL) again, the ids _old[i] it is joined to now _new[i], measuring them where they are:
// the new one (where it was, in its order) or FUDE_ZOOM_NONE (not joined to any of them). The caller lets the old go.
u32 fude_zoom_connect_dim_remap(fude_zoom_scene* _s, u32 _dim, const fude_zoom_id* _old, const fude_zoom_id* _new, u32 _n);

// CONSTRAINTS THAT HOLD (shape.h's CONSTRAINT): two lines kept parallel, square to each other, or the same length.
// One turned or stretched (moved: fude_zoom_connect_follow), the other is turned or stretched round its own middle to
// keep it so, in the same step (and on along a chain of them); one made again (fude_zoom_connect_constraint_remap), the
// other made again to keep it so.
//
// A constraint's kind and its lines (FUDE_ZOOM_NONE: gone). False: not a constraint.
b8  fude_zoom_connect_constraint_of(const fude_zoom_scene* _s, u32 _object, u8* _kind, u32* _a, u32* _b);
// What line _b is moved by (round its middle, its frame's units) to keep constraint _kind with line _a as _a is now.
// False: nothing to do (it is so already, or they are not lines in one frame).
b8  fude_zoom_connect_constrain(const fude_zoom_scene* _s, u8 _kind, u32 _a, u32 _b, fude_zoom_sim* _out);
// A new constraint _kind between lines _a and _b (in their frame, between their middles). Its index.
u32 fude_zoom_connect_constraint_add(fude_zoom_scene* _s, u8 _kind, u32 _a, u32 _b);

#endif
