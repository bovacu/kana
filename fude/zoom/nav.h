// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_NAV
#define FUDE_ZOOM_NAV

#include "rde.h"
#include "zoom/scene.h"

// ===========================================================================
// Getting around a canvas with no edges and no bottom (research §2.1):
//
//   FLYING   the camera carried from one view to another, however many levels
//            apart: out until both ends are on screen, across, and in. The zoom
//            eases on a log scale and the pan happens while far out, so the
//            way stays readable. Undo and redo fly to the change; so do Back,
//            the marks and the depth's levels (page.h).
//   MARKS    with nothing on screen, arrows at its edge towards the nearest
//            things drawn in each direction, and rings round what is on screen
//            but too small to see. A tap flies to them.
//   DEPTH    how far in the camera is, said briefly: ×12, ×10⁶.
//
// A flight works the camera out from one of its ends, in that end's own units
// — the coarser while the camera is out past it, else the deeper — and puts it
// in the ancestor where that zoom belongs: so it is exact at any depth and
// makes no frames on the way.
// ===========================================================================

#define FUDE_ZOOM_FLY_STEPS  64u
#define FUDE_ZOOM_NAV_ARROWS 8u
#define FUDE_ZOOM_NAV_RINGS  8u
#define FUDE_ZOOM_NAV_MARKS  (FUDE_ZOOM_NAV_ARROWS + FUDE_ZOOM_NAV_RINGS)   // at most, for fude_zoom_nav_marks
#define FUDE_ZOOM_NAV_RING   2.0    // things drawn smaller than this on screen (points) get a ring
#define FUDE_ZOOM_NAV_DRAWN_DEPTH 16u   // levels looked into for what is drawn (deeper: a frame's own box)
#define FUDE_ZOOM_NAV_LEVEL_GROWS 1.5   // a level is one of the depth's when what is drawn there is this much wider

typedef struct {
    b8               active;
    fude_zoom_camera ends[2];       // from, to (where it lands, exactly)
    fude_zoom_v2     cross[2];      // each end's view of the other's point, in its own units
    f64              l[2];          // their zooms, log2, in their common frame's units
    f64              g[2];          // log2 of a unit of each end's frame, in the common frame's
    f64              l_dip;         // how far out (log2) the middle goes past the straight way
    f64              way[FUDE_ZOOM_FLY_STEPS + 1];   // the way along, by the eased time: most of the pan while far out
    f64              began, length; // seconds
} fude_zoom_flight;

// A flight from the camera to _to (_half: the screen's half-size). False: it
// is already there, or the ends are too many levels apart to fly between —
// then the camera is put there at once.
b8   fude_zoom_fly_begin(fude_zoom_flight* _f, fude_zoom_scene* _s, fude_zoom_camera _to, fude_zoom_v2 _half, f64 _now);
// The camera where the flight has it at _now. False once it has landed: the
// camera exactly at _to.
b8   fude_zoom_fly_step(fude_zoom_flight* _f, fude_zoom_scene* _s, fude_zoom_v2 _half, f64 _now);

// The camera that shows frame _frame's box _box (its units) filling about
// _fill of the screen, the box in the middle.
fude_zoom_camera fude_zoom_nav_framing(fude_zoom_v2 _half, u32 _frame, fude_zoom_box _box, f64 _fill);

// A mark: where it is drawn (screen, centre origin, Y up), which way it points
// (radians; a ring points nowhere), and what a tap shows (a frame's box).
typedef struct {
    rde_vec_2F    at;
    f32           angle;
    b8            ring;
    u32           frame;
    fude_zoom_box box;
} fude_zoom_nav_mark;

// The marks for the camera now: the nearest thing drawn in each eighth of a
// turn off screen, at the edge (inset _inset), and rings for what is on screen
// but too small to see — in the camera's frame and two above it. How many.
u32  fude_zoom_nav_marks(const fude_zoom_scene* _s, fude_zoom_v2 _half, f32 _inset, fude_zoom_nav_mark* _out);

// What is drawn in frame _frame and the frames in it, in its units, leaving out its object _skip
// (FUDE_ZOOM_NONE: none) — a frame in it for its drawing, not the view it was made for; no bookmarks,
// layers' records, guides, nor what hidden layers hide. Empty: nothing.
fude_zoom_box fude_zoom_nav_drawn(const fude_zoom_scene* _s, u32 _frame, u32 _skip);

// The depth's levels: what is drawn at each level from the camera's up, framed — the nearest first,
// the outermost last (kept when there are more than _max). A level with nothing drawn past what the
// one below it has (frames made on the way out beyond all there is) shows the same: said once. The
// view the camera is at is left out. Nothing drawn anywhere: the top at its zoom 1. How many.
u32  fude_zoom_nav_levels(const fude_zoom_scene* _s, fude_zoom_v2 _half, fude_zoom_camera* _out, u32 _max);

// How far in the camera is: log10 of its zoom in home's units (scene.h: the
// canvas's own frame, ×1 at its zoom 1) — and how far in a view _c would be.
f64  fude_zoom_nav_depth(const fude_zoom_scene* _s);
f64  fude_zoom_nav_depth_of(const fude_zoom_scene* _s, fude_zoom_camera _c);
// That said briefly into _out: "×12", "×0.5", "×10⁶", "×10⁻³".
void fude_zoom_nav_say(f64 _log10, c8* _out, usize _size);

#endif
