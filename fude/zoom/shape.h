// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SHAPE
#define FUDE_ZOOM_SHAPE

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Shapes: lines, rectangles, ellipses, polygons and the rest, kept as their NUMBERS rather than points (a rectangle is
// its half-width, half-height and corner radius), so they stay exact at any zoom and can be changed later. Also the
// geometry that turns a hand-drawn stroke into a shape, and the boards a cut list lists.
//
// Where it sits: zoom, the canvas apps' layer, under the scene. A shape is an object of kind SHAPE (scene.h): its type
// is in the object's `channels`, its numbers in its payload (f64s, in its frame's units from its translation, before
// its rotation and scale); it is drawn in the object's colour and half-width, and filled with the same colour when the
// object says FILLED. The renderer, the exports, the page's tools, snapping and the lasso all read shapes through here.
// Threads: none of its own and no shared state; in practice everything calls it on the main thread.
// Files: the numbers below are what the canvas's file keeps for each shape (zfile.h), so their layout is part of the
// file format.
//
//   LINE       x1 y1                       from its translation to there
//   RECT       half-width half-height      round its translation (radius: its corners')
//              radius
//   ELLIPSE    rx ry                       round its translation
//   POLYGON    x y for each corner         closed; round its translation
//   DIMENSION  x y offset                  a measure from its translation to there, its line offset to its left
//                                          (render_shape.c writes its length in real units on it)
//   ARROW      k, then x y for each of     a line through its points with heads (FUDE_ZOOM_ARROW_ bits) that
//              its k points (from its      joins two things: their ids, each in two halves (scene.h's
//              translation), heads,        fude_zoom_id_put; 0 for a loose end). A connector (connect.h). Older files
//              from_hi from_lo to_hi       kept each id as one number, its id's own bits (FUDE_ZOOM_ARROW_NUMBERS_BITS
//              to_lo                       in all): still read (connect.h's fude_zoom_connect_arrow_ends)
//   PATH       bits, then x y for each     a smooth line through its nodes (Catmull-Rom: it passes through every
//              node                        one), closed, and each node a corner, as its first number's bits say
//                                          (the Curve tool's; its nodes can be moved, added, deleted, made corners)
//   ARC        radius from sweep           an arc of a circle round its translation (its centre), from angle `from`
//                                          (radians, counter-clockwise from +x) on by `sweep` (either way, under a
//                                          whole turn): exact, as drawn along a compass or a round edge, or a
//                                          fillet's (fude_zoom_shape_fillet)
//   SYMBOL     kind hw hh px text...       one of the diagram library's (symbol.h: a process, a class, a database, a
//                                          server...) round its translation: its half sizes, its letters' height and
//                                          its text's bytes (NUL-ended). It is drawn as its parts, its text in its
//                                          boxes; its outline is what connects and snaps.
//   BOARD      half-length half-width      a board (research §2.10, item 9): a rectangle round its translation, its
//              thickness look name...      thickness in millimetres, its look (its grain, 0 along its length or 1
//                                          across, and its material: fude_zoom_board_look), and its name's bytes in
//                                          the numbers after (NUL-ended). What a cut list lists.
//   SHEET      half-width half-height      a measured sheet (sheet.h): a rectangle round its translation with rulers
//              scale flags                 along its sides in real units, a grid if it wants one, printed at 1:scale
//   RADIAL     radius angle diameter       a circle's or an arc's size, its translation the centre: a line out to it
//              id_hi id_lo                 at that angle (diameter 1: right across), with "R 20 mm" or "Ø 40 mm" on
//                                          it; the id is the circle's, which it follows, in two halves (scene.h's
//                                          fude_zoom_id_put). Older files kept it as one number, its id's own bits
//                                          (four numbers in all): still read (connect.h's fude_zoom_connect_radial_id)
//   ANGLE      radius from sweep           an angle's size: an arc round its translation (the corner) from angle
//                                          `from` on by `sweep`, with "45°" on it
//   WALL       x y thickness               a building's wall from its translation to x y, as thick as it says, its
//                                          ends half its thickness past the points (so two walls meeting at a point
//                                          close their corner)
//   WIRE       k, x y × k, id pin id pin   a circuit's wire (circuit.h) through its k points, then its ends' parts
//                                          and pins (0 and -1 for none)
//   CONSTRAINT kind id_a id_b              two lines kept so (connect.h): parallel, square to each other, or the
//                                          same length. Nothing is drawn; its place is a point between them.
//   PROPS      kind id_hi id_lo values…    properties of a drawn thing (props.h: a body's material, whether it is
//                                          fixed, its mass…), naming it by its id's two halves. Nothing is drawn;
//                                          its place is a point by the thing.
//   GUIDE      x y reach                   a construction line through its translation the way x y goes, reaching
//                                          that far each way (well past its sheet: across the screen wherever it is
//                                          drawn on). Never printed or cut; lines snap to it (to its crossings with
//                                          them, and along it).
//
// A DIMENSION may keep, after its three numbers, what its ends are on, so that it follows them (connect.h): 0, then
// each end's thing's id in two halves (scene.h's fude_zoom_id_put) and its point (FUDE_ZOOM_DIMENSION_REFS numbers in
// all). Older files kept each id as one number, its id's own bits (FUDE_ZOOM_DIMENSION_REFS_BITS numbers in all):
// still read (connect.h's fude_zoom_connect_dimension_ends), and written as they are now when the dimension is made
// again or copied.
//
// HOLD TO SNAP (research §2.4): a stroke drawn roughly, with the pen held still at its end, becomes the shape it looks
// like (a line, a circle or an ellipse, a rectangle, a triangle, or a polygon of up to eight corners), or stays a
// stroke when it looks like none of them (fude_zoom_shape_recognize).
// ===========================================================================

// A shape's type, kept in its object's channels.
typedef enum {
    FUDE_ZOOM_SHAPE_LINE = 1,
    FUDE_ZOOM_SHAPE_RECT,
    FUDE_ZOOM_SHAPE_ELLIPSE,
    FUDE_ZOOM_SHAPE_POLYGON,
    FUDE_ZOOM_SHAPE_DIMENSION,
    FUDE_ZOOM_SHAPE_ARROW,
    FUDE_ZOOM_SHAPE_BOARD,
    FUDE_ZOOM_SHAPE_PATH,
    FUDE_ZOOM_SHAPE_ARC,
    FUDE_ZOOM_SHAPE_SYMBOL,
    FUDE_ZOOM_SHAPE_SHEET,
    FUDE_ZOOM_SHAPE_RADIAL,
    FUDE_ZOOM_SHAPE_ANGLE,
    FUDE_ZOOM_SHAPE_GUIDE,
    FUDE_ZOOM_SHAPE_CONSTRAINT,
    FUDE_ZOOM_SHAPE_WIRE,
    FUDE_ZOOM_SHAPE_WALL,
    FUDE_ZOOM_SHAPE_PROPS
} FUDE_ZOOM_SHAPE_;

// Returns whether a shape of type _type is an attribute of other things (a CONSTRAINT, PROPS): nothing is drawn of
// it, and it is never lassoed, rubbed out, snapped to or exported; it follows what it names.
static inline b8 fude_zoom_shape_is_attribute(u8 _type) {
    return _type == FUDE_ZOOM_SHAPE_CONSTRAINT || _type == FUDE_ZOOM_SHAPE_PROPS;
}

// A CONSTRAINT's kind: its first number (connect.h).
typedef enum {
    FUDE_ZOOM_CONSTRAINT_PARALLEL = 1,
    FUDE_ZOOM_CONSTRAINT_SQUARE,       // Perpendicular.
    FUDE_ZOOM_CONSTRAINT_EQUAL,        // The same length.
    FUDE_ZOOM_CONSTRAINT_KINDS
} FUDE_ZOOM_CONSTRAINT_;

#define FUDE_ZOOM_GUIDE_REACH 1000.0   // A guide's reach each way when it has none of its own, in its own units.

// How many numbers a dimension has when it keeps what its ends are on (connect.h): now, and in an older file.
#define FUDE_ZOOM_DIMENSION_REFS      10u   // x y offset 0 from_hi from_lo from_key to_hi to_lo to_key
#define FUDE_ZOOM_DIMENSION_REFS_BITS 8u    // x y offset 0 from from_key to to_key (each id one number's bits)
// How many numbers a RADIAL has: radius angle diameter, then its circle's id in two halves (fude_zoom_id_put).
#define FUDE_ZOOM_RADIAL_NUMBERS 5u

// An arrow's heads and line: its heads number, the one after its points (number 1 + 2k).
typedef enum {
    FUDE_ZOOM_ARROW_END    = 1,    // A head where it ends.
    FUDE_ZOOM_ARROW_START  = 2,    // A head where it starts.
    FUDE_ZOOM_ARROW_DOTTED = 4,
    FUDE_ZOOM_ARROW_CIRCLE = 8,    // Its heads round, not pointed.
} FUDE_ZOOM_ARROW_;
#define FUDE_ZOOM_ARROW_POINTS 6u   // An arrow's points at most, so its numbers fit.
// How many numbers an arrow of _k points has with the ids it joins (connect.h): now (each id in two halves), and in an
// older file (each id one number's own bits).
#define FUDE_ZOOM_ARROW_NUMBERS(_k)      (2u * (_k) + 6u)
#define FUDE_ZOOM_ARROW_NUMBERS_BITS(_k) (2u * (_k) + 4u)
// Returns how many points an arrow's numbers (_n, _count of them) say it has, its first number: 0 when it has none, or
// when that number is not a count from 0 to FUDE_ZOOM_ARROW_POINTS (a damaged file's: NaN, negative or vast, which a
// cast to u32 would not hold).
static inline u32 fude_zoom_arrow_points(const f64* _n, u32 _count) {
    return _count >= 1u && _n[0] >= 0.0 && _n[0] <= (f64)FUDE_ZOOM_ARROW_POINTS ? (u32)_n[0] : 0u;
}
// Returns an arrow's heads (FUDE_ZOOM_ARROW_ bits and symbol.h's ends' styles), its number after its _k points: 0 when
// it has none, or when that number is not one a u32 holds.
static inline u32 fude_zoom_arrow_heads(const f64* _n, u32 _count, u32 _k) {
    const f64 _h = _count >= 2u * _k + 2u ? _n[1u + 2u * _k] : 0.0;
    return _h >= 0.0 && _h < 4294967296.0 ? (u32)_h : 0u;
}

#define FUDE_ZOOM_SHAPE_CORNERS 8u    // A recognised polygon's corners at most.
#define FUDE_ZOOM_PATH_NODES    32u   // A path's nodes at most.
#define FUDE_ZOOM_SHAPE_NUMBERS (2u + 4u * FUDE_ZOOM_PATH_NODES)   // A shape's numbers at most: a path's, with handles.
#define FUDE_ZOOM_PATH_CLOSED   1u    // A path's first number's bit 0: it is closed.
// Bit node + 1 of a path's first number: that node is a corner, where the line turns instead of passing smoothly.
#define FUDE_ZOOM_PATH_CORNER(_node) (2ull << (_node))   // Node _node's corner bit.
// Bit 40 of a path's first number: each node's handle is kept after the nodes (x y each).
#define FUDE_ZOOM_PATH_HANDLES  (1ull << 40)   // The bit that says its handles are kept.

#define FUDE_ZOOM_BOARD_NAME 96u   // A board's name's size at most in bytes, its NUL included (12 numbers).

// Simplifies a freehand line's _n points into a path's nodes: Douglas-Peucker with tolerance _tol (in the points'
// units), the tolerance let out until they fit in _most (at least 2). The ends are kept. The nodes go into _out, which
// must hold _most. Returns how many.
u32 fude_zoom_shape_fit_path(const fude_zoom_v2* _p, u32 _n, f64 _tol, u32 _most, fude_zoom_v2* _out);

// Sculpt (research §2.2): redraws part of a line _old (_m points) with a new stroke _new (_n points) begun within
// _reach of it. When _new also ends on it, the stretch between where _new begins and ends is replaced by _new (turned
// round when drawn against the line's way); when _new ends off it, all of the line the way _new went from where _new
// began is replaced. The result goes into _out (cleared first). Returns false when _new did not begin on the line.
b8  fude_zoom_shape_sculpt(const fude_zoom_v2* _old, u32 _m, const fude_zoom_v2* _new, u32 _n, f64 _reach, rde_arr* _out);

// Returns a point (_t from 0 to 1) of the Catmull-Rom segment from _b to _c; _a is the node before, _d the one after.
fude_zoom_v2 fude_zoom_shape_catmull(fude_zoom_v2 _a, fude_zoom_v2 _b, fude_zoom_v2 _c, fude_zoom_v2 _d, f64 _t);

// A path's BEZIER HANDLES: a node's handle (x y from the node) is the way the line leaves it (and, opposite, the way
// it comes in: smooth through it), dragged out by hand. (0, 0) means the curve's own: a sixth of the way from the node
// before to the node after (at a corner, along each stretch), which is what the Catmull-Rom line through the nodes is.
// The handles are kept after the nodes when any is set (FUDE_ZOOM_PATH_HANDLES in its first number).
//
// Unpacks a path's _count numbers: its nodes into _nodes and its handles into _handles (zeros when none are kept), each
// with room for FUDE_ZOOM_PATH_NODES, and its first number's bits into *_bits. Returns how many nodes.
u32  fude_zoom_path_unpack(const f64* _n, u32 _count, u64* _bits, fude_zoom_v2* _nodes, fude_zoom_v2* _handles);
// Packs a path's numbers again into _n (room for FUDE_ZOOM_SHAPE_NUMBERS), keeping the handles only when one is set
// (_handles may be NULL). Returns how many numbers.
u32  fude_zoom_path_pack(u64 _bits, const fude_zoom_v2* _nodes, const fude_zoom_v2* _handles, u32 _count, f64* _n);
// Returns, as a handle, the way the line leaves node _i (_out) or comes into it (!_out): its own handle, or the
// curve's own when it has none. (0, 0) for fewer than 2 nodes or an _i out of range.
fude_zoom_v2 fude_zoom_path_tangent(const fude_zoom_v2* _nodes, const fude_zoom_v2* _handles, u32 _count, u64 _bits, u32 _i, b8 _out);

// Fits a circle through stroke points _p (an arc drawn along a round edge): its centre and radius, the angle it starts
// at and how far it goes round (signed: counter-clockwise is positive). Returns false when there are too few points or
// they are not on one circle (any of them further from it than _tolerance).
b8   fude_zoom_shape_fit_arc(const fude_zoom_v2* _p, u32 _n, f64 _tolerance, fude_zoom_v2* _centre, f64* _radius, f64* _from, f64* _sweep);

// A FILLET: rounds the corner where lines a0→a1 and b0→b1 meet (or would meet, carried on) with an arc of radius
// _radius touching both. Each line is cut back to where the arc touches it, keeping its far end: *_a_keep and *_b_keep
// get the ends kept, *_a_end and *_b_end the ends at the corner's side, moved there. The arc's centre, start and sweep
// go into *_centre, *_from and *_sweep. Returns false when the lines are parallel or too short for that radius.
b8   fude_zoom_shape_fillet(fude_zoom_v2 _a0, fude_zoom_v2 _a1, fude_zoom_v2 _b0, fude_zoom_v2 _b1, f64 _radius,
                            fude_zoom_v2* _a_keep, fude_zoom_v2* _a_end, fude_zoom_v2* _b_keep, fude_zoom_v2* _b_end,
                            fude_zoom_v2* _centre, f64* _from, f64* _sweep);

// A CHAMFER: cuts the same corner straight, _back along each line from where they meet (with 0, the lines are carried
// on or cut back to meet there and the corner stays sharp). Each line's end at the corner's side is moved there and its
// far end kept, as for a fillet. Returns false when the lines are parallel, _back is negative, or a line is too short
// for it.
b8   fude_zoom_shape_chamfer(fude_zoom_v2 _a0, fude_zoom_v2 _a1, fude_zoom_v2 _b0, fude_zoom_v2 _b1, f64 _back,
                             fude_zoom_v2* _a_keep, fude_zoom_v2* _a_end, fude_zoom_v2* _b_keep, fude_zoom_v2* _b_end);

// What a board is made of: what the cut list says, and the wood it is drawn in. Kept in its fourth number, in the bits
// above its grain (fude_zoom_board_look).
typedef enum {
    FUDE_ZOOM_MATERIAL_NONE = 0,   // Not said: pale timber.
    FUDE_ZOOM_MATERIAL_PINE,
    FUDE_ZOOM_MATERIAL_OAK,
    FUDE_ZOOM_MATERIAL_WALNUT,
    FUDE_ZOOM_MATERIAL_MAPLE,
    FUDE_ZOOM_MATERIAL_BEECH,
    FUDE_ZOOM_MATERIAL_CHERRY,
    FUDE_ZOOM_MATERIAL_BIRCH_PLY,
    FUDE_ZOOM_MATERIAL_MDF,
    FUDE_ZOOM_MATERIAL_MELAMINE,
    FUDE_ZOOM_MATERIAL_COUNT
} FUDE_ZOOM_MATERIAL_;

// Writes a board's numbers into _n (room for FUDE_ZOOM_SHAPE_NUMBERS): its half sizes (its frame's units), its
// thickness (mm), its look (bit 0 its grain, 1 meaning across its length; the bits above it its material,
// FUDE_ZOOM_MATERIAL_) and its name (cut short at a whole character to fit). Returns how many numbers.
u32  fude_zoom_board_numbers(f64* _n, f64 _half_length, f64 _half_width, f64 _thickness_mm, u8 _look, const c8* _name);
// Read a board's look (grain | material << 1: what fude_zoom_board_numbers takes), its grain (0 along, 1 across) and
// its material from its _count numbers. A board from before materials has none said.
u8   fude_zoom_board_look(const f64* _n, u32 _count);
u8   fude_zoom_board_grain(const f64* _n, u32 _count);
u8   fude_zoom_board_material(const f64* _n, u32 _count);
// Returns the colour of the wood a material is drawn in (its fill); an unknown material is pale timber.
rde_color fude_zoom_material_wood(u8 _material);
// Writes a board's name from its numbers into _out (_size bytes, NUL-ended, cut at a whole character): "" when it has
// none.
void fude_zoom_board_name(const f64* _n, u32 _count, c8* _out, usize _size);
// A cut board (cut.h) keeps, past its name's twelve numbers (from number FUDE_ZOOM_BOARD_RINGS_AT), its rings: how
// many, then each one's point count and points (its own units, round its translation): ring 0 its outline, the others
// its holes. Its half sizes are its box's (what its label and the cut list say).
#define FUDE_ZOOM_BOARD_RINGS_AT 16u
// Writes a cut board's numbers into _out (f64s, cleared first): its head as fude_zoom_board_numbers writes it (_look
// its look: grain and material, as fude_zoom_board_look reads it), then its rings from _points and _rings (_n points,
// each one's ring). Returns how many numbers.
u32  fude_zoom_board_cut_numbers(rde_arr* _out, f64 _half_length, f64 _half_width, f64 _thickness_mm, u8 _look, const c8* _name,
                                 const fude_zoom_v2* _points, const u32* _rings, u32 _n);
// Writes a board's outline and holes (its own units) into _points and _rings (fill.h's rings; both cleared first): a
// cut board's own rings, else its rectangle. Returns how many points.
u32  fude_zoom_board_rings(const f64* _n, u32 _count, rde_arr* _points, rde_arr* _rings);
// Returns whether a board has been cut: its outline is its own, not its rectangle.
b8   fude_zoom_board_is_cut(const f64* _n, u32 _count);

// A shape as it is recognised or being placed, in whatever units its points came in.
typedef struct {
    u8           type;        // FUDE_ZOOM_SHAPE_ (0 when none).
    fude_zoom_v2 at;          // Its translation: a line's start; the others' middle.
    f64          rotation;
    f64          n[FUDE_ZOOM_SHAPE_NUMBERS];
    u32          count;       // How many numbers are in n.
} fude_zoom_shape_fit;

// Finds the shape a stroke's _count points look like, into *_out (cleared first). The points are in screen units, as
// the tolerances are a hand's on the screen. Returns false when it looks like none: it stays a stroke.
b8   fude_zoom_shape_recognize(const fude_zoom_v2* _points, u32 _count, fude_zoom_shape_fit* _out);

// Writes a shape's outline in its own units (before its translation, rotation and scale) into _out (an rde_arr of
// fude_zoom_v2, cleared first). _segments: how finely an ellipse or a round corner is cut. *_closed: whether the last
// point joins the first.
void fude_zoom_shape_outline(u8 _type, const f64* _n, u32 _count, u32 _segments, rde_arr* _out, b8* _closed);
// Returns enough segments for an outline _size_on_screen points across to look round: about one every 6 points,
// 24 to 360.
u32  fude_zoom_shape_segments(f64 _size_on_screen);

// A shape's line style (a technical drawing's, research §2.10): solid; dashed, for what is hidden; or long dash and
// dot, for a centre line. Kept in the shape's q (scene.h).
typedef enum {
    FUDE_ZOOM_LINE_SOLID = 0,
    FUDE_ZOOM_LINE_DASHED,
    FUDE_ZOOM_LINE_DASH_DOT,
    FUDE_ZOOM_LINE_STYLES
} FUDE_ZOOM_LINE_;

// A shape's q keeps its line style in its low 3 bits and, in bits 4 and 5, how a closed shape is HATCHED: lines across
// its inside, 12 of its line's widths apart (a section on a drawing; they print as its lines do, at any scale).
typedef enum {
    FUDE_ZOOM_HATCH_NONE = 0,
    FUDE_ZOOM_HATCH_DIAGONAL,   // At 45°.
    FUDE_ZOOM_HATCH_CROSS,      // At 45° and 135°.
    FUDE_ZOOM_HATCH_KINDS
} FUDE_ZOOM_HATCH_;
#define FUDE_ZOOM_HATCH_SHIFT 4u
#define FUDE_ZOOM_HATCH_APART 12.0   // Its lines are this many of its line's widths apart.
// Read a q's line style and hatch, and pack them into a q.
static inline u8 fude_zoom_line_style_of(i8 _q) { return (u8)((u8)_q & 0x07u); }
static inline u8 fude_zoom_hatch_of(i8 _q)      { return (u8)(((u8)_q >> FUDE_ZOOM_HATCH_SHIFT) & 0x03u); }
static inline i8 fude_zoom_q_of(u8 _style, u8 _hatch) { return (i8)((_style & 0x07u) | ((_hatch & 0x03u) << FUDE_ZOOM_HATCH_SHIFT)); }

// Appends a hatch's lines to _out (pairs of fude_zoom_v2, each line's ends): lines at _angle (radians), _apart apart,
// across the inside of the closed polygon _p (_n points; inside as even-odd says, so a hole is left out), only what
// falls in _view. Returns how many lines were added.
u32  fude_zoom_hatch_lines(const fude_zoom_v2* _p, u32 _n, f64 _angle, f64 _apart, fude_zoom_box _view, rde_arr* _out);

// Cuts the polyline _p (_n points; with _closed it goes back to its first) into the dashes of line style _style, for a
// line _w wide: the pattern grows with the width (a dash 7 widths long and its gap 3.5; a centre line's 12, 3, a dot,
// 3), counted along it from its first point. Only what falls in _view (a box round the screen: deep in, a line may run
// to millions of points) goes into _out (cleared first; pairs of fude_zoom_v2, each piece's ends). Returns how many
// pieces (0 for a solid line).
u32  fude_zoom_line_dashes(const fude_zoom_v2* _p, u32 _n, b8 _closed, u8 _style, f64 _w, fude_zoom_box _view, rde_arr* _out);

// Turns a shape's numbers over in place, its own y the other way (a mirror image round its own x axis): what a
// mirror's copy is drawn from (select.h). The numbers of a rectangle, an ellipse, a plain board, a sheet and a symbol
// stay as they are, being their own mirror images; so do a wire's, because the parts it joins are only turned by a
// mirror and the wire must still meet their pins.
void fude_zoom_shape_mirror(u8 _type, f64* _n, u32 _count);

#endif
