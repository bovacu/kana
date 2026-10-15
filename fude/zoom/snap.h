// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SNAP
#define FUDE_ZOOM_SNAP

#include "rde.h"
#include "zoom/scene.h"
#include "zoom/render.h"

// ===========================================================================
// Snap: snapping you can predict (docs/research/infinite_canvas_research.md §2.10, item 6). Where the pen is about to
// put a point (a shape's corner, a measure's end), the point goes onto the thing drawn there when one is near enough
// on the screen:
//
//   END       a stroke's ends, a line's ends, a dimension's ends
//   CORNER    a rectangle's or a polygon's corners
//   CROSS     where two things drawn cross (lines, strokes, arcs, outlines: what a compass's constructions are made of)
//   MID       the middle of a line, or of a rectangle's or a polygon's side
//   CENTRE    the middle of a rectangle, an ellipse or a polygon
//
// and, for a point reached from another one (a line's or a measure's far end, reached from _from):
//
//   PERP      where the line from there meets something drawn at a right angle (the foot of the perpendicular on the
//             piece of it near the pen)
//   TANGENT   where the line from there touches a circle or an arc near the pen
//   PARALLEL  the line from there along a straight thing drawn nearby (a line, or a side of a rectangle, a polygon, a
//             board or a sheet), or along one of the ways asked for (a sheet's sides: across it and up it), or along
//             one of those turned by every so many degrees (15: an angle's steps)
//   EXTENSION a straight thing nearby carried on past its end, onto the pen
//   NEAREST   the nearest point of a line or an outline right under the pen
//   GRID      a crossing of a measured sheet's grid (sheet.h; the page offers it when nothing else is near)
//
// Where it sits: fude/zoom, over scene.h and render.h. The page (page_measure.c, page_circuit.c) uses it while shapes,
// measures and moves are drawn, and shows the target before the pen lifts, so it is never a surprise.
// Its main types and their lifecycle: fude_zoom_snap is a plain value returned by copy.
// Threads: main thread only, like the scene.
// Design: the kinds win in the order above, so a corner beats a middle that is a little nearer, and what the point
// lands on is the most telling thing there. A length is never rounded off, because that would happen silently. Only
// what the renderer drew at the camera's depth (and in the frames above it that tools reach) is looked at, within
// the radius, in screen points.
// ===========================================================================

// What a point snapped to. Of two kinds found, the lower value wins (NONE means nothing was found).
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

// Distances on screen, in points.
#define FUDE_ZOOM_SNAP_RADIUS 14.0   // how near a point must be to snap to it
#define FUDE_ZOOM_SNAP_ALONG  320.0  // straight things this near the pen give a PARALLEL
#define FUDE_ZOOM_SNAP_WAYS   8u     // the most ways that may be asked for
#define FUDE_ZOOM_SNAP_REACH_ON 8.0  // NEAREST: this near a line (less than the rest, since a line is everywhere)
#define FUDE_ZOOM_SNAP_FAR_ON 400.0  // EXTENSION: carried on at most this far past its end

// Keys of a snapped point (fude_zoom_snap.key) that are not a point of a thing's own outline.
#define FUDE_ZOOM_SNAP_NO_KEY  INT32_MIN   // not a point of a thing's own (a middle, a crossing)
#define FUDE_ZOOM_SNAP_CENTRE_KEY (-1)     // a thing's centre

// Where a point snapped to, and what to.
typedef struct {
    u8           kind;   // FUDE_ZOOM_SNAP_
    fude_zoom_v2 at;     // on the screen (origin at the centre, Y up)
    fude_zoom_v2 a, b;   // PERP, PARALLEL, EXTENSION, NEAREST: the piece it lies on (a == b for a way asked for);
                         // TANGENT: the circle's centre (a) and its radius (b.x)
    // The thing it is a point of (FUDE_ZOOM_NONE for none), and which point (key, as connect.h's
    // fude_zoom_connect_point takes it: a shape's outline corner, a stroke's first or last point, a centre). A
    // dimension drawn to it follows that point.
    u32          object;
    i32          key;
} fude_zoom_snap;

// Returns the point _screen snaps to within _radius (kind NONE when nothing is near enough). _frames are the frames
// the tools reach (render.h: fude_zoom_render_editable).
fude_zoom_snap fude_zoom_snap_find(const fude_zoom_scene* _s, const fude_zoom_visible* _frames, u32 _frame_count, fude_zoom_v2 _screen, f64 _radius);
// Writes into _out (at most _max) the ways straight things go from _vertex (on screen): one unit vector for each line,
// side or edge drawn from it, and two for one drawn through it (within _tol points). Ways within half a degree of one
// already found are given once. Returns how many.
u32 fude_zoom_snap_rays(const fude_zoom_scene* _s, const fude_zoom_visible* _frames, u32 _frame_count, fude_zoom_v2 _vertex, f64 _tol, fude_zoom_v2* _out, u32 _max);
// Like fude_zoom_snap_find, for a point reached from _from (on screen): PERP, TANGENT, PARALLEL and EXTENSION are
// offered too, along straight things nearby or along _ways (_way_count unit vectors on screen). When _step_deg is
// not 0, the first way turned by every _step_deg degrees is offered as well (more finely when the pen is nearer).
fude_zoom_snap fude_zoom_snap_find_from(const fude_zoom_scene* _s, const fude_zoom_visible* _frames, u32 _frame_count, fude_zoom_v2 _screen, f64 _radius,
                                        fude_zoom_v2 _from, const fude_zoom_v2* _ways, u32 _way_count, f64 _step_deg);

// LEVEL AND PLUMB: pulls points being moved by (_dx, _dy) so that one of them lies level with one of _targets (the
// same y) and one plumb with one (the same x), each way the nearest within _reach, independently of each other. This
// lines a part's pins up with another's, so the wires between them are straight. _movers are the points' places as
// the move began. Writes the pulled move into *_out_dx and *_out_dy, and what each way lines up with into *_line_x
// (a plumb line's x) and *_line_y (a level line's y), NAN where nothing does.
// A line it already lines up with (*_line_x and *_line_y as passed in; NAN for none) holds within _keep, which is
// wider than _reach so the line does not flicker off at its edge, unless another target is nearer.
void fude_zoom_snap_align(const fude_zoom_v2* _movers, u32 _nm, const fude_zoom_v2* _targets, u32 _nt, f64 _dx, f64 _dy, f64 _reach,
                          f64 _keep, f64* _out_dx, f64* _out_dy, f64* _line_x, f64* _line_y);
// Returns _v moved to the nearest of the lines _origin + k·_step (a grid's lines one way). When _step is not over 0,
// returns _v as it is.
f64  fude_zoom_snap_grid(f64 _v, f64 _origin, f64 _step);

#endif
