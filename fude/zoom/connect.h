// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_CONNECT
#define FUDE_ZOOM_CONNECT

#include "rde.h"
#include "zoom/scene.h"
#include "zoom/snap.h"

// ===========================================================================
// Connect: things that stay joined to other things and follow them when they move. Three kinds:
//   connectors (research §2.9): an arrow (shape.h's ARROW) that keeps the ids of the two things it joins;
//   dimensions that follow: a dimension drawn between points of things, or a circle's size, that keeps which;
//   constraints that hold: two lines kept parallel, square to each other, or the same length.
// It also drives a dimension's ends (constraint-lite: moving every end or corner at a point to another).
//
// Where it sits: zoom, the canvas apps' layer, over the scene and shape.h's numbers. The page makes
// connectors (page_shapes.c), dimensions (page_measure.c) and constraints (page_edit.c); the lasso (select.c) calls
// fude_zoom_connect_follow after a move and remaps what was joined to things made again.
// Threads: main thread only (it reads and changes the scene).
//
// A connector aims at each end's middle and stops on its outline: a shape's outline, a text's or a picture's box,
// anything else's box. When the things it joins are moved, it is moved too, in the same step: turned, stretched and
// slid so its ends land where they should (its bends, if any, carried along). That makes it one undo with the move,
// and its width does not change with the stretch, because the renderer draws an arrow's width in its frame's units.
// Design: see docs/infinite_canvas_design.md §15 (connectors, driving), §21 (connectors' ends) and §26 (constraints).
// ===========================================================================

// Returns where the line from _object's middle towards _toward leaves its outline (frame units); its middle when
// _toward is inside it.
fude_zoom_v2 fude_zoom_connect_edge(const fude_zoom_scene* _s, u32 _object, fude_zoom_v2 _toward);
// Returns the middle of _object's box, in frame units.
fude_zoom_v2 fude_zoom_connect_middle(const fude_zoom_scene* _s, u32 _object);

// After the objects _moved (_count object indexes, just moved) have moved, moves what in _frame follows them and was
// not moved itself: connectors joined to them, dimensions joined to them, and lines constrained to them (and on along
// a chain of constraints). For each, its index, its place before and its place after are appended to _objects,
// _before and _after, and its new place is set, so the caller can push one undo step. Returns how many.
u32 fude_zoom_connect_follow(fude_zoom_scene* _s, u32 _frame, const u32* _moved, u32 _count, rde_arr* _objects, rde_arr* _before, rde_arr* _after);

// Adds a new connector from _a to _b in _frame (frame units), joining _from and _to (object indexes; FUDE_ZOOM_NONE
// for a loose end), with heads _heads (FUDE_ZOOM_ARROW_ bits), colour _color and half-width _radius. Returns its
// object index.
u32 fude_zoom_connect_add(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _a, fude_zoom_v2 _b, u32 _from, u32 _to, u32 _heads, rde_color _color, f32 _radius);

// Driving (research §2.10, item 5, constraint-lite): moves everything in _frame with an end or a corner at _from
// (within _tol, frame units) to _to: a line's or a dimension's end, a polygon's or a path's corner, a rectangle's
// corner (the one across from it staying). Each is made again as a new object in place of the old one; the old ones
// are appended to _died and the new ones to _born, for the caller to push as one step. _skip is left alone. Returns
// how many were moved.
u32 fude_zoom_connect_drive(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _from, fude_zoom_v2 _to, f64 _tol, u32 _skip, rde_arr* _died, rde_arr* _born);

// Makes _arrow again, on _layer, with the ids it joins changed from _old[i] to _new[i] (_n pairs). Returns the new
// arrow (where the old one was, in its order), or FUDE_ZOOM_NONE when nothing changes or it is not an arrow. The
// caller lets the old one go. An older arrow's ids (each one number's bits) are written in two halves in the new one.
u32 fude_zoom_connect_remap(fude_zoom_scene* _s, u32 _arrow, const fude_zoom_id* _old, const fude_zoom_id* _new, u32 _n, u16 _layer);
// Reads the ids an ARROW joins from its numbers (_n, _count of them, _k points: shape.h's layout) into *_from and *_to
// (0: that end is loose). With FUDE_ZOOM_ARROW_NUMBERS(_k) numbers or more each id is in two halves (scene.h's
// fude_zoom_id_put); with fewer but at least FUDE_ZOOM_ARROW_NUMBERS_BITS(_k), from a file made before it was kept so,
// each is one number's own bits. Returns false, both ids 0, when its numbers are too few to keep any. Any thread (it
// reads only _n).
b8   fude_zoom_connect_arrow_ends(const f64* _n, u32 _count, u32 _k, fude_zoom_id* _from, fude_zoom_id* _to);
// Writes the ids an ARROW of _k points joins into its numbers after its points and heads, as they are kept now: each
// in two halves (fude_zoom_id_put). _n has room for FUDE_ZOOM_ARROW_NUMBERS(_k) numbers, the count it then has. Any
// thread.
void fude_zoom_connect_arrow_put(f64* _n, u32 _k, fude_zoom_id _from, fude_zoom_id _to);

// DIMENSIONS THAT FOLLOW: a dimension drawn from a point of a thing to a point of a thing (snap.h's: an end, a corner,
// a centre) keeps which (shape.h: FUDE_ZOOM_DIMENSION_REFS), and a circle's size (RADIAL) keeps its circle's id. When
// those things move, fude_zoom_connect_follow moves the dimension with them (its ends where their points are now, in
// the same step); when they are made again in their place (stretched, resized), fude_zoom_connect_dim_remap measures
// it again.
//
// Finds where a point of a thing is now (frame units) into *_out. The point is snap.h's key: a shape's outline corner
// or end (fude_zoom_shape_outline's, at one segment), a stroke's first (0) or last (1) point, or its centre
// (FUDE_ZOOM_SNAP_CENTRE_KEY). Returns false when the thing is gone or has no such point.
b8  fude_zoom_connect_point(const fude_zoom_scene* _s, u32 _object, i32 _key, fude_zoom_v2* _out);
// Finds a circle's or an arc's centre and radius now (frame units). Returns false when _object is not a live one.
b8  fude_zoom_connect_round(const fude_zoom_scene* _s, u32 _object, fude_zoom_v2* _centre, f64* _radius);
// Adds a new dimension from _a to _b in _frame (frame units), its line _offset to their left, joined to point _a_key of
// _a_obj and point _b_key of _b_obj (FUDE_ZOOM_NONE when that end is not joined; a thing in another frame is not joined
// either). Returns its object index.
u32 fude_zoom_connect_dimension(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _offset, u32 _a_obj, i32 _a_key, u32 _b_obj, i32 _b_key,
                                rde_color _color, f32 _radius);
// Makes _dim (a DIMENSION or a RADIAL) again, the ids it is joined to changed from _old[i] to _new[i], measuring the
// things where they are now. Returns the new one (where the old one was, in its order), or FUDE_ZOOM_NONE when it is
// joined to none of them. The caller lets the old one go.
u32 fude_zoom_connect_dim_remap(fude_zoom_scene* _s, u32 _dim, const fude_zoom_id* _old, const fude_zoom_id* _new, u32 _n);
// Reads the id of the circle a RADIAL follows from its numbers (_n, _count of them, shape.h's layout): numbers 3 and 4
// as scene.h's fude_zoom_id_put writes an id (FUDE_ZOOM_RADIAL_NUMBERS in all); in a RADIAL of four numbers, from a
// file made before it was kept so, number 3's own bits. Returns 0 when it follows none. Any thread (it reads only _n).
fude_zoom_id fude_zoom_connect_radial_id(const f64* _n, u32 _count);
// Reads what a DIMENSION's two ends are on from its numbers (_n, _count of them, shape.h's layout): each end's thing's
// id into _id[0] and _id[1] (0: that end is on nothing) and its point (snap.h's key) into _key[0] and _key[1]
// (FUDE_ZOOM_SNAP_NO_KEY where the number is no key: not a whole number an i32 holds, as a damaged file's may be). With
// FUDE_ZOOM_DIMENSION_REFS numbers each id is in two halves (scene.h's fude_zoom_id_put); with
// FUDE_ZOOM_DIMENSION_REFS_BITS, from a file made before it was kept so, each is one number's own bits. Returns false,
// both ids 0, when it keeps neither (its three numbers alone: on nothing). Any thread (it reads only _n).
b8   fude_zoom_connect_dimension_ends(const f64* _n, u32 _count, fude_zoom_id _id[2], i32 _key[2]);
// Writes what a DIMENSION's ends are on into its numbers after its first three, as they are kept now: number 3 is 0,
// then each end's id in two halves (fude_zoom_id_put) and its key. _n has room for FUDE_ZOOM_DIMENSION_REFS numbers.
// Any thread.
void fude_zoom_connect_dimension_put(f64* _n, const fude_zoom_id _id[2], const i32 _key[2]);

// CONSTRAINTS THAT HOLD (shape.h's CONSTRAINT): two lines kept parallel, square to each other, or the same length. When
// one is turned or stretched (as it is moved, by fude_zoom_connect_follow), the other is turned or stretched round its
// own middle to keep it so, in the same step, and so on along a chain of them. When one is made again, the page makes
// the other again to keep it so (page_diagrams.c: fude_zoom_page_rebind_constraint).
//
// Reads a constraint: its kind (FUDE_ZOOM_CONSTRAINT_) into *_kind and its two lines' object indexes into *_a and *_b
// (FUDE_ZOOM_NONE for a line that is gone). Returns false when _object is not a live constraint.
b8  fude_zoom_connect_constraint_of(const fude_zoom_scene* _s, u32 _object, u8* _kind, u32* _a, u32* _b);
// Works out how line _b must move (round its middle, in its frame's units) to keep constraint _kind with line _a as _a
// is now, into *_out. Returns false when there is nothing to do: it holds already, or they are not two lines in one
// frame.
b8  fude_zoom_connect_constrain(const fude_zoom_scene* _s, u8 _kind, u32 _a, u32 _b, fude_zoom_sim* _out);
// Adds a new constraint _kind between lines _a and _b, in _a's frame and layer, placed between their middles (nothing
// is drawn of it). Returns its object index.
u32 fude_zoom_connect_constraint_add(fude_zoom_scene* _s, u8 _kind, u32 _a, u32 _b);

#endif
