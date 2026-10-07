// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SNAP
#define FUDE_ZOOM_SNAP

#include "rde.h"
#include "zoom/scene.h"
#include "zoom/render.h"

// ===========================================================================
// Snapping you can predict (research §2.10, item 6): where the pen is about to
// put a point — a shape's corner, a measure's end — it goes onto the thing
// drawn there when one is near enough on the screen:
//
//   END      a stroke's ends, a line's ends, a dimension's
//   CORNER   a rectangle's or a polygon's corners
//   CROSS    where two things drawn cross (lines, strokes, arcs, outlines:
//            what a compass's constructions are made from)
//   MID      the middle of a line, of a rectangle's or a polygon's side
//   CENTRE   a rectangle's, an ellipse's or a polygon's middle
//
// and, for a point reached from another (a line's end, a measure's: _from) —
//
//   PERP     where the line from there meets something drawn square (the
//            foot of the perpendicular on the piece of it near the pen)
//   TANGENT  where the line from there touches a circle or an arc near the pen
//   PARALLEL the line from there along a straight thing drawn near (a line's,
//            a rectangle's or a polygon's side, a board's, a sheet's), or one
//            of the ways asked for (a sheet's sides: across and up it), or a
//            turn of those every so many degrees (15: an angle's steps)
//   EXTENSION a straight thing near carried on past its end, onto the pen
//   NEAREST  the nearest point of a line or an outline right under the pen
//   GRID     a measured sheet's grid's crossing (sheet.h: the page offers it
//            when nothing else is near)
//
// in that order: a corner beats a middle a little nearer, so what it lands on
// is the most telling thing there (never a length rounded off: that would be
// silent). Only what the renderer drew at the camera's depth (and the frames
// above it that tools reach) is looked at, within the radius, in screen points.
// The target is shown before the pen lifts (page.c), so it is never a surprise.
// ===========================================================================

typedef enum {
    FUDE_ZOOM_SNAP_NONE = 0,
    FUDE_ZOOM_SNAP_END,
    FUDE_ZOOM_SNAP_CORNER,
    FUDE_ZOOM_SNAP_CROSS,
    FUDE_ZOOM_SNAP_MID,
    FUDE_ZOOM_SNAP_CENTRE,
    FUDE_ZOOM_SNAP_PERP,
    FUDE_ZOOM_SNAP_TANGENT,
    FUDE_ZOOM_SNAP_PARALLEL,
    FUDE_ZOOM_SNAP_EXTENSION,
    FUDE_ZOOM_SNAP_NEAREST,
    FUDE_ZOOM_SNAP_GRID
} FUDE_ZOOM_SNAP_;

#define FUDE_ZOOM_SNAP_RADIUS 14.0   // screen points
#define FUDE_ZOOM_SNAP_ALONG  320.0  // straight things this near the pen give a PARALLEL (screen points)
#define FUDE_ZOOM_SNAP_WAYS   8u     // ways asked for, at most
#define FUDE_ZOOM_SNAP_REACH_ON 8.0  // NEAREST: this near a line (screen points: less than the rest, as it is everywhere)
#define FUDE_ZOOM_SNAP_FAR_ON 400.0  // EXTENSION: carried on this far past its end at most

#define FUDE_ZOOM_SNAP_NO_KEY  INT32_MIN   // a point that is no point of a thing's own (a middle, a crossing)
#define FUDE_ZOOM_SNAP_CENTRE_KEY (-1)     // ...a thing's centre

typedef struct {
    u8           kind;   // FUDE_ZOOM_SNAP_
    fude_zoom_v2 at;     // on the screen (centre origin, Y up)
    fude_zoom_v2 a, b;   // PERP, PARALLEL, EXTENSION, NEAREST: the piece it is of (a way asked for: a == b); TANGENT: the
                         // circle's centre (a) and its radius (b.x)
    // The thing it is a point of, and which (connect.h's fude_zoom_connect_point: a shape's outline corner, a
    // stroke's first or last point, a centre): what a dimension drawn to it follows. FUDE_ZOOM_NONE: none.
    u32          object;
    i32          key;
} fude_zoom_snap;

// The point _screen snaps to (kind NONE: nothing near; at is _screen then).
// _frames: what the tools reach (render.h: fude_zoom_render_editable).
fude_zoom_snap fude_zoom_snap_find(const fude_zoom_scene* _s, const fude_zoom_visible* _frames, u32 _frame_count, fude_zoom_v2 _screen, f64 _radius);
// The ways straight things go from _vertex (screen): each line, side or edge drawn from it or through it (within _tol
// points), a unit way each (two for one through it), alike ways once, into _out (at most _max). How many.
u32 fude_zoom_snap_rays(const fude_zoom_scene* _s, const fude_zoom_visible* _frames, u32 _frame_count, fude_zoom_v2 _vertex, f64 _tol, fude_zoom_v2* _out, u32 _max);
// ...reached from _from (screen): PERP, TANGENT, PARALLEL and EXTENSION too, along straight things near or _ways (unit ways
// on the screen, _way_count of them) and, _step_deg not 0, the first way turned by every _step_deg degrees (more
// finely: the pen nearer them).
fude_zoom_snap fude_zoom_snap_find_from(const fude_zoom_scene* _s, const fude_zoom_visible* _frames, u32 _frame_count, fude_zoom_v2 _screen, f64 _radius,
                                        fude_zoom_v2 _from, const fude_zoom_v2* _ways, u32 _way_count, f64 _step_deg);

// LEVEL AND PLUMB: points being moved by (_dx, _dy) — their places as the move began, _movers — pulled so one of them
// lies level with one of _targets (the same y) and one plumb with one (the same x), each way the nearest within
// _reach, independently: the move as pulled into *_out_dx, *_out_dy; what each way lines up with into *_line_x (a plumb
// line's x) and *_line_y (a level line's y), NAN where nothing does. (A part's pins with another's: the wires between
// them straight.)
// A line each way it already lines up with (*_line_x, *_line_y as they are passed in; NAN: none) holds within
// _keep (wider than _reach: it does not flicker off at its edge), unless another target is nearer.
void fude_zoom_snap_align(const fude_zoom_v2* _movers, u32 _nm, const fude_zoom_v2* _targets, u32 _nt, f64 _dx, f64 _dy, f64 _reach,
                          f64 _keep, f64* _out_dx, f64* _out_dy, f64* _line_x, f64* _line_y);
// _v moved to the nearest of the lines _origin + k·_step (a grid's, one way). _step not over 0: _v as it is.
f64  fude_zoom_snap_grid(f64 _v, f64 _origin, f64 _step);

#endif
