// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_NAV
#define FUDE_ZOOM_NAV

#include "rde.h"
#include "zoom/scene.h"

// ===========================================================================
// Navigation: getting around a canvas with no edges and no bottom (docs/research/infinite_canvas_research.md §2.1).
//
//   FLYING   carries the camera from one view to another, however many levels apart: out until both ends are on
//            screen, across, and in. The zoom eases on a log scale and the pan happens while far out, so the way
//            stays readable. Undo and redo fly to the change; so do Back, the marks and the depth's levels (page.h).
//   MARKS    when nothing is on screen: arrows at the screen's edge pointing to the nearest things drawn in each
//            direction, and rings round what is on screen but too small to see. A tap flies to them.
//   DEPTH    how far in the camera is, said briefly: ×12, ×10⁶.
//
// Where it sits: fude/zoom, over scene.h. The page (page_nav.c, and the page_*.c modules that move the view), the map
// (map.c) and notebooks' pages (pages.c) use it.
// Its main types and their lifecycle: fude_zoom_flight and fude_zoom_nav_mark are plain values the caller keeps; the
// page holds one flight.
// Threads: main thread only, like the scene.
// Design: a flight works the camera out from one of its ends, in that end's own units (from the coarser end while the
// camera is further out than it, otherwise from the deeper one), and puts it in the ancestor frame where that zoom
// belongs. So a flight is exact at any depth, and it makes no frames on the way.
// ===========================================================================

// How many steps the table of a flight's way along has.
#define FUDE_ZOOM_FLY_STEPS  64u
// The most arrows (one for each eighth of a turn) and the most rings fude_zoom_nav_marks gives.
#define FUDE_ZOOM_NAV_ARROWS 8u
#define FUDE_ZOOM_NAV_RINGS  8u
#define FUDE_ZOOM_NAV_MARKS  (FUDE_ZOOM_NAV_ARROWS + FUDE_ZOOM_NAV_RINGS)   // the most marks fude_zoom_nav_marks gives
#define FUDE_ZOOM_NAV_RING   2.0    // things drawn smaller than this on screen (in points) get a ring
#define FUDE_ZOOM_NAV_DRAWN_DEPTH 16u   // how many levels down to look for what is drawn (deeper, a frame's own box)
#define FUDE_ZOOM_NAV_LEVEL_GROWS 1.5   // a level is given when its drawing is this much wider than the last one given

// A flight under way.
typedef struct {
    b8               active;
    fude_zoom_camera ends[2];       // where it starts and where it lands (exactly)
    fude_zoom_v2     cross[2];      // where each end sees the other's point, in its own units
    f64              l[2];          // the ends' zooms, as log2, in their common frame's units
    f64              g[2];          // log2 of the size of a unit of each end's frame, in the common frame's units
    f64              l_dip;         // how much further out (log2) the middle goes than the straight way would
    f64              way[FUDE_ZOOM_FLY_STEPS + 1];   // how far along the pan is, by eased time (mostly while far out)
    f64              began, length; // in seconds
} fude_zoom_flight;

// Starts a flight from the camera to _to (_half is half the screen's size). Returns false, and puts the camera at _to
// at once, when it is already there or the ends are too many levels apart to fly between.
b8   fude_zoom_fly_begin(fude_zoom_flight* _f, fude_zoom_scene* _s, fude_zoom_camera _to, fude_zoom_v2 _half, f64 _now);
// Moves the camera to where the flight has it at time _now. Returns false once it has landed, with the camera
// exactly at _to (or when no flight is under way).
b8   fude_zoom_fly_step(fude_zoom_flight* _f, fude_zoom_scene* _s, fude_zoom_v2 _half, f64 _now);

// Returns the camera that shows frame _frame's box _box (in its units) in the middle of the screen, filling about
// _fill of it. A box with no size is framed as a thousandth of a unit across, so the zoom never runs away.
fude_zoom_camera fude_zoom_nav_framing(fude_zoom_v2 _half, u32 _frame, fude_zoom_box _box, f64 _fill);

// A mark: where it is drawn (on screen, origin at the centre, Y up), which way it points (in radians; a ring points
// nowhere), and what a tap shows (a frame and a box in it).
typedef struct {
    rde_vec_2F    at;
    f32           angle;
    b8            ring;
    u32           frame;
    fude_zoom_box box;
} fude_zoom_nav_mark;

// Writes into _out (room for FUDE_ZOOM_NAV_MARKS) the marks for the camera as it is now: an arrow for the nearest
// thing drawn off screen in each eighth of a turn, at the screen's edge inset by _inset, and rings for what is on
// screen but too small to see. It looks in the camera's frame and the two above it. Returns how many marks.
u32  fude_zoom_nav_marks(const fude_zoom_scene* _s, fude_zoom_v2 _half, f32 _inset, fude_zoom_nav_mark* _out);

// Returns the box round what is drawn in frame _frame and the frames inside it, in its units, leaving out its object
// _skip (FUDE_ZOOM_NONE for none). A frame inside counts for what is drawn in it, not for its own box (the view it was
// made for, often far wider). Bookmarks, layers' records, guides and what hidden layers hide are left out. Returns an
// empty box when nothing is drawn.
fude_zoom_box fude_zoom_nav_drawn(const fude_zoom_scene* _s, u32 _frame, u32 _skip);

// Writes into _out the depth's levels: a camera framing what is drawn at each level from the camera's frame upwards,
// the nearest first and the outermost last (when there are more than _max, the outermost takes the last place). A level
// whose drawing is not much wider than the last one given (frames made on the way out, beyond all there is) would show
// the same thing, so it is left out. The view the camera is at already is left out too. When that leaves nothing, it
// gives the view of everything drawn, or the top frame at zoom 1 when nothing is drawn anywhere. Returns how many
// cameras it wrote (0 only when _max is 0).
u32  fude_zoom_nav_levels(const fude_zoom_scene* _s, fude_zoom_v2 _half, fude_zoom_camera* _out, u32 _max);

// Returns how far in the camera is, as log10 of its zoom in home's units (scene.h: home is the canvas's own frame,
// ×1 at its zoom 1); fude_zoom_nav_depth_of returns the same for a view _c.
f64  fude_zoom_nav_depth(const fude_zoom_scene* _s);
f64  fude_zoom_nav_depth_of(const fude_zoom_scene* _s, fude_zoom_camera _c);
// Writes a depth (log10 of the zoom) briefly into _out as UTF-8: "×12", "×0.5", "×10⁶", "×3·10³", "×10⁻³".
void fude_zoom_nav_say(f64 _log10, c8* _out, usize _size);

#endif
