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
//   PARALLEL the line from there along a straight thing drawn near (a line's,
//            a rectangle's or a polygon's side, a board's, a sheet's), or one
//            of the ways asked for (a sheet's sides: across and up it)
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
    FUDE_ZOOM_SNAP_PARALLEL,
    FUDE_ZOOM_SNAP_GRID
} FUDE_ZOOM_SNAP_;

#define FUDE_ZOOM_SNAP_RADIUS 14.0   // screen points
#define FUDE_ZOOM_SNAP_ALONG  320.0  // straight things this near the pen give a PARALLEL (screen points)
#define FUDE_ZOOM_SNAP_WAYS   8u     // ways asked for, at most

#define FUDE_ZOOM_SNAP_NO_KEY  INT32_MIN   // a point that is no point of a thing's own (a middle, a crossing)
#define FUDE_ZOOM_SNAP_CENTRE_KEY (-1)     // ...a thing's centre

typedef struct {
    u8           kind;   // FUDE_ZOOM_SNAP_
    fude_zoom_v2 at;     // on the screen (centre origin, Y up)
    fude_zoom_v2 a, b;   // PERP, PARALLEL: the piece it is square to or along (a way asked for: a == b)
    // The thing it is a point of, and which (connect.h's fude_zoom_connect_point: a shape's outline corner, a
    // stroke's first or last point, a centre): what a dimension drawn to it follows. FUDE_ZOOM_NONE: none.
    u32          object;
    i32          key;
} fude_zoom_snap;

// The point _screen snaps to (kind NONE: nothing near; at is _screen then).
// _frames: what the tools reach (render.h: fude_zoom_render_editable).
fude_zoom_snap fude_zoom_snap_find(const fude_zoom_scene* _s, const fude_zoom_visible* _frames, u32 _frame_count, fude_zoom_v2 _screen, f64 _radius);
// ...reached from _from (screen): PERP and PARALLEL too, along straight things near or _ways (unit ways on the
// screen, _way_count of them).
// The ways straight things go from _vertex (screen): each line, side or edge drawn from it or through it (within _tol
// points), a unit way each (two for one through it), alike ways once, into _out (at most _max). How many.
u32 fude_zoom_snap_rays(const fude_zoom_scene* _s, const fude_zoom_visible* _frames, u32 _frame_count, fude_zoom_v2 _vertex, f64 _tol, fude_zoom_v2* _out, u32 _max);
fude_zoom_snap fude_zoom_snap_find_from(const fude_zoom_scene* _s, const fude_zoom_visible* _frames, u32 _frame_count, fude_zoom_v2 _screen, f64 _radius,
                                        fude_zoom_v2 _from, const fude_zoom_v2* _ways, u32 _way_count);

#endif
