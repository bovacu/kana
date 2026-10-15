// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_INSTRUMENT_PARTS
#define FUDE_ZOOM_INSTRUMENT_PARTS

#include "zoom/instrument.h"
#include "zoom/fill.h"
#include "drawing/base/draw.h"

// ===========================================================================
// Instrument parts: the instruments' private interface. The instruments (instrument.h) are written as
// instrument.c and the instrument_*.c modules; this is what they share with each other: the functions one module calls
// in another, and the constants more than one needs. Included by instrument.c and instrument_*.c only, never anywhere
// else: everything outside the instruments uses instrument.h.
//
// Where it sits: fude/zoom, under instrument.h (which it includes, with every header the instruments use). The modules
// include this header and nothing else.
// Its main types and their lifecycle: the instruments (fude_zoom_instruments, instrument.h), the caller's struct, set
// up by fude_zoom_instruments_init (instrument.c); everything is in its fixed arrays, so nothing is freed.
// Threads: the main thread only, as the instruments themselves.
// Files: none.
// Design: instrument.c was one file of some 2,600 lines; it was split by what each part does, its code moved unchanged.
// A function one module calls in another is declared here (not RDE_INTERNAL); what one module alone uses stays
// RDE_INTERNAL in it. Every name keeps instrument.c's: fude_zoom_instrument_ and fude_zoom_instruments_.
//
// Which module holds what:
//   instrument.c        where things are (edges, keys, outlines, curves, knobs, × and size tabs), and the pile
//   instrument_hands.c  hands on one: moved, turned, grown, docked and sliding against the others; the fingers
//   instrument_pen.c    the pen along an edge, chained from edge to edge at their crossings; the line drawn, told
//   instrument_draw.c   the instruments drawn (fude_zoom_instruments_render)
// ===========================================================================

// --- the constants and types the modules share (as instrument.c had them) --------------------------------------------

// Their sizes on the screen.
#define FUDE_ZOOM_RULER_LENGTH   560.0
#define FUDE_ZOOM_RULER_WIDTH    FUDE_ZOOM_INSTRUMENT_RULER_WIDTH
#define FUDE_ZOOM_RULER_ZERO     12.0    // its graduations start this far in from its end
#define FUDE_ZOOM_SQUARE_45_LEG  280.0
#define FUDE_ZOOM_SQUARE_30_LEG  340.0   // the long leg (the short one: this over √3)
#define FUDE_ZOOM_SQUARE_HOLLOW  36.0    // the band round a set square's hollow middle
#define FUDE_ZOOM_PROTRACTOR_R   160.0
#define FUDE_ZOOM_ARC_SIDES      72u     // an arc's sides in an outline (the protractor's round, a corner gauge's)
#define FUDE_ZOOM_CIRCLE_RIM     34.0    // a circle template's plate round its hole (an ellipse template's too)
#define FUDE_ZOOM_CORNER_BAND    48.0    // a corner radius gauge's plate past its round

// --- instrument.c: where things are, the pile ------------------------------------------------------------------------

// A point of the instrument's own units (its middle the origin, turned with it) on the screen.
fude_zoom_v2 fude_zoom_instrument_place(const fude_zoom_instrument* _t, fude_zoom_v2 _local);
// A point of the screen in the instrument's own units: fude_zoom_instrument_place undone.
fude_zoom_v2 fude_zoom_instrument_unplace(const fude_zoom_instrument* _t, fude_zoom_v2 _screen);
// A corner radius gauge's round, and its plate's half side (the round at its lower left).
f64 fude_zoom_corner_r(const fude_zoom_instrument* _t);
// Its knob's, ×'s and tabs' scale: as much bigger as it is on the screen (instrument.h's look), not too small to take
// nor huge.
f64 fude_zoom_instrument_icons(const fude_zoom_instrument* _t);
// A circle template's rim (real: its band), an ellipse template's (as much bigger as it is).
f64 fude_zoom_instrument_rim(const fude_zoom_instrument* _t);
// How much bigger than when it came out (a ruler: longer; a set square, a protractor: bigger); 1 when not set.
f64 fude_zoom_instrument_size(const fude_zoom_instrument* _t);
// A set square's corners (its own units, the right angle first, then along
// its long leg, then the short), round its middle.
void fude_zoom_square_corners(u32 _kind, f64 _size, fude_zoom_v2 _out[3]);
// An ellipse template's hole's half sizes, its plate's.
void fude_zoom_instrument_ellipse(const fude_zoom_instrument* _t, f64* _rx, f64* _ry, f64* _hx, f64* _hy);
// An edge's curve on the screen — a stencil's line, else the instrument's one (an ellipse template's hole, a
// French curve's edge): how many points.
u32 fude_zoom_instrument_curve_of(const fude_zoom_instruments* _ins, u32 _slot, const fude_zoom_instrument_edge* _e, fude_zoom_v2* _out, u32 _max);
// How long a curve is (a closed one, round; _open: from its first point to its last).
f64 fude_zoom_instrument_curve_length(const fude_zoom_v2* _c, u32 _n, b8 _open);
// The point of a curve (_n points: closed, or _open with two ends) nearest _p: how far round it is from
// its first point (along it), how far _p is from it. _window > 0: only within that far round either way of
// _near (the pen going on along it, never jumping across).
fude_zoom_v2 fude_zoom_instrument_on_curve(const fude_zoom_v2* _c, u32 _n, fude_zoom_v2 _p, f64 _near, f64 _window, f64* _along, f64* _off, f64* _round, b8 _open);
// A curve's point _along round it (_open: along it, held at its ends), and the way it goes there.
fude_zoom_v2 fude_zoom_instrument_curve_at(const fude_zoom_v2* _c, u32 _n, f64 _along, fude_zoom_v2* _dir, b8 _open);
// Its size tab (a compass has none: its hinge opens it), just past the end that
// grows, and the point that stays where it is as it grows (a ruler's zero end, a
// set square's right angle, a protractor's centre). Its own units.
b8 fude_zoom_instrument_grow_local(const fude_zoom_instrument* _t, fude_zoom_v2* _tab, fude_zoom_v2* _fixed);
// The shown one whose size tab is at _p, the one on top (-1: none).
i32 fude_zoom_instruments_grow_under(const fude_zoom_instruments* _ins, fude_zoom_v2 _p);
// A hand takes slot _k by its size tab.
void fude_zoom_instruments_take_grow(fude_zoom_instruments* _ins, u32 _k, u64 _id, fude_zoom_v2 _at);
// The shown one whose × is at _p, the one on top (-1: none).
i32 fude_zoom_instruments_close_under(const fude_zoom_instruments* _ins, fude_zoom_v2 _p);
// Returns whether _p is on instrument _slot's body (what a hand takes it by): a stencil's plate away from its lines,
// a template's plate and not its hole, a French curve's inside its curve, else inside its outline (even-odd).
b8 fude_zoom_instrument_inside(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2 _p);
// Brought to the top of the pile.
void fude_zoom_instrument_raise(fude_zoom_instruments* _ins, u32 _slot);

// --- instrument_hands.c: hands on one --------------------------------------------------------------------------------

// Returns an angle brought into (-π, π].
f64 fude_zoom_instrument_wrap(f64 _a);
// The hands as they are now become where the next moves are measured from.
void fude_zoom_instruments_regrab(fude_zoom_instruments* _ins);
// Returns _v made one long ((1, 0) for no length).
fude_zoom_v2 fude_zoom_v2_norm(fude_zoom_v2 _v);
// Returns the dot product of _a and _b.
f64 fude_zoom_v2_dot(fude_zoom_v2 _a, fude_zoom_v2 _b);
// The held instrument after its hands moved: its size tab makes it longer or shorter (its other end staying); a
// compass's hinge sets its radius and its way; its knob turns it (resting at each 15°); one hand drags it (docking
// and sliding); two turn and move it as the line between them does.
void fude_zoom_instruments_follow(fude_zoom_instruments* _ins);
// One hand (_id at _at) takes instrument _k by its body: raised to the top, the hands measured from here.
void fude_zoom_instruments_hold(fude_zoom_instruments* _ins, u64 _id, fude_zoom_v2 _at, u32 _k);
// Half an instrument's narrowest width on the screen (its outline's box, its own way round): a
// ruler's, half its band.
f64 fude_zoom_instrument_thin(const fude_zoom_instruments* _ins, u32 _slot);
// How near _at is to an instrument's nearest edge (screen points).
f64 fude_zoom_instrument_edge_off(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2 _at);
// A hand on a × going off it: it is not put away (the hand stays the instruments', doing nothing).
b8 fude_zoom_instruments_closing_moved(fude_zoom_instruments* _ins, u64 _id, fude_zoom_v2 _at);
// That hand lifted: on the × still, put away.
b8 fude_zoom_instruments_closing_up(fude_zoom_instruments* _ins, u64 _id);

// --- instrument_pen.c: the pen ---------------------------------------------------------------------------------------

// Where on edge _e the point nearest _p is: its distance along the edge (an
// angle on an arc), and how far _p is from it.
fude_zoom_v2 fude_zoom_instrument_on_edge(const fude_zoom_instrument_edge* _e, fude_zoom_v2 _p, f64* _along, f64* _off);

#endif
