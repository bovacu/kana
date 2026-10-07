// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SHAPE
#define FUDE_ZOOM_SHAPE

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Shapes: lines, rectangles, ellipses and polygons kept as their NUMBERS, not
// points — a rectangle is its half-width, half-height and corners — so they
// stay exact at any zoom and can be changed later. A shape is an object of
// kind SHAPE (scene.h): its type in the object's `channels`, its numbers in its
// payload (f64s, in its frame's units from its translation, before its
// rotation and scale), drawn in the object's colour and half-width, filled
// with the same colour when the object says FILLED.
//
//   LINE     x1 y1                          from its translation to there
//   RECT     half-width half-height radius  round its translation (radius: its corners')
//   ELLIPSE  rx ry                          round its translation
//   POLYGON  x y, a corner each             closed; round its translation
//   DIMENSION x y offset                    a measure from its translation to there,
//                                           its line offset to the left (render.c
//                                           writes its length in real units on it)
//   ARROW    k, x y a point each (k, from   a line through its points, with heads:
//            its translation), heads,       FUDE_ZOOM_ARROW_ bits; the things it joins
//            from, to                       (their ids, their bits kept in the numbers;
//                                           0: loose) — a connector
//   PATH     bits, x y a node each          a smooth line through its nodes (Catmull-
//                                           Rom: it passes through every one), closed
//                                           and each node a corner as its first
//                                           number's bits say (the Curve tool's; its
//                                           nodes moved, added, deleted, turned corners)
//   ARC      radius from sweep              a circle's arc round its translation (its
//                                           centre), from angle `from` (radians,
//                                           counter-clockwise from +x) on by `sweep`
//                                           (either way, under a whole turn): exact —
//                                           drawn along a compass or a round edge, a
//                                           fillet's (fude_zoom_shape_fillet)
//   SYMBOL   kind hw hh px text...          one of the diagram library's (symbol.h: a
//                                           process, a class, a database, a server...)
//                                           round its translation, its half sizes, its
//                                           letters' height and its text's bytes (NUL-
//                                           ended): drawn as its parts, its text in its
//                                           boxes; its outline what connects and snaps
//   BOARD    half-length half-width         a board (research §2.10, item 9): a
//            thickness look name...         rectangle round its translation, its
//                                           thickness in millimetres, its grain
//                                           (0 along its length, 1 across) and
//                                           material (2 each one: fude_zoom_board_look),
//                                           its name's bytes kept in the numbers after
//                                           (NUL-ended) — what a cut list lists
//   SHEET    half-width half-height         a measured sheet (sheet.h): a rectangle round
//            scale flags                    its translation with rulers along its sides
//                                           in real units, a grid if it likes, printed
//                                           at 1:scale
//   RADIAL   radius angle diameter id      a circle's or an arc's size, its translation
//                                           the centre: a line out to it at that angle
//                                           (diameter 1: right across), "R 20 mm" or
//                                           "Ø 40 mm" on it; id: the circle's (it follows)
//   ANGLE    radius from sweep              an angle's size: an arc round its translation
//                                           (the corner) from angle `from` on by `sweep`,
//                                           "45°" on it
//   WALL     x y thickness                  a building's wall: from its translation to x y, as
//                                           thick as said, its ends half its thickness past the
//                                           points (so two meeting at a point close their corner)
//   WIRE     k, x y × k, id pin id pin      a circuit's wire (circuit.h): through its k points,
//                                           its ends' parts and pins (0 -1: none) after them
//   CONSTRAINT kind id_a id_b               two lines kept so (connect.h): parallel, square
//                                           to each other, the same length — nothing drawn
//                                           (a point where it was made, between them)
//   PROPS    kind id_hi id_lo values…       properties of a drawn thing (props.h: a body's —
//                                           its material, fixed or not, its mass…), naming it by
//                                           its id's halves — nothing drawn (a point by it)
//   GUIDE    x y reach                      a construction line: through its translation
//                                           the way x y goes, reach each way (well past its
//                                           sheet: across the screen wherever it is drawn
//                                           on) — never printed or cut, what lines snap to
//                                           (their crossings with it, along it)
//
// A DIMENSION may keep, after its three numbers, what its ends are on (it
// follows them: connect.h) — 0, then each end's thing's id and its point.
//
// HOLD TO SNAP (research §2.4): a stroke drawn roughly, the pen held still at
// its end, becomes the shape it looks like — a line, a circle or an ellipse, a
// rectangle, a triangle or a polygon of up to eight corners — or stays a
// stroke when it looks like none of them (fude_zoom_shape_recognize).
// ===========================================================================

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

// Is a shape of type _type an attribute of other things (a CONSTRAINT, PROPS): nothing drawn, never lassoed, rubbed
// out, snapped to or exported; it follows what it names.
static inline b8 fude_zoom_shape_is_attribute(u8 _type) {
    return _type == FUDE_ZOOM_SHAPE_CONSTRAINT || _type == FUDE_ZOOM_SHAPE_PROPS;
}

// A CONSTRAINT's kind (its first number; connect.h).
typedef enum {
    FUDE_ZOOM_CONSTRAINT_PARALLEL = 1,
    FUDE_ZOOM_CONSTRAINT_SQUARE,       // perpendicular
    FUDE_ZOOM_CONSTRAINT_EQUAL,        // the same length
    FUDE_ZOOM_CONSTRAINT_KINDS
} FUDE_ZOOM_CONSTRAINT_;

#define FUDE_ZOOM_GUIDE_REACH 1000.0   // a guide's reach each way when it has none of its own (its own units)

#define FUDE_ZOOM_DIMENSION_REFS 8u   // a dimension's numbers with what its ends are on: x y offset 0 from from_key to to_key

// An arrow's heads and line (its numbers' last but two).
typedef enum {
    FUDE_ZOOM_ARROW_END    = 1,    // a head where it ends
    FUDE_ZOOM_ARROW_START  = 2,    // ...where it starts
    FUDE_ZOOM_ARROW_DOTTED = 4,
    FUDE_ZOOM_ARROW_CIRCLE = 8,    // its heads round, not pointed
} FUDE_ZOOM_ARROW_;
#define FUDE_ZOOM_ARROW_POINTS 6u   // at most, so its numbers fit

#define FUDE_ZOOM_SHAPE_CORNERS 8u    // a recognized polygon's corners, at most
#define FUDE_ZOOM_PATH_NODES    32u   // a path's nodes, at most
#define FUDE_ZOOM_SHAPE_NUMBERS (2u + 4u * FUDE_ZOOM_PATH_NODES)   // a shape's numbers, at most (a path's with its handles)
#define FUDE_ZOOM_PATH_CLOSED   1u    // a path's first number's bit: closed
#define FUDE_ZOOM_PATH_CORNER(_node) (2ull << (_node))   // ...and a bit a node: a corner (the line turns there, not smooth)
#define FUDE_ZOOM_PATH_HANDLES  (1ull << 40)   // ...and: each node's handle kept after the nodes (x y each)

#define FUDE_ZOOM_BOARD_NAME 96u   // a board's name's bytes, its NUL included, at most (12 numbers)

// A freehand line's nodes for a path: its points (_n) simplified (Douglas-
// Peucker, _tol their units, let out until they fit) to at most _most, its
// ends kept, into _out. How many.
u32 fude_zoom_shape_fit_path(const fude_zoom_v2* _p, u32 _n, f64 _tol, u32 _most, fude_zoom_v2* _out);

// Sculpt (research §2.2): a line _old (_m points) redrawn in part by _new (_n),
// begun within _reach of it: the stretch between where _new begins and ends on
// it replaced by _new (turned round when drawn against its way), or, _new
// ending off it, all of it the way _new went from where _new began. Into _out.
// False: _new did not begin on it.
b8  fude_zoom_shape_sculpt(const fude_zoom_v2* _old, u32 _m, const fude_zoom_v2* _new, u32 _n, f64 _reach, rde_arr* _out);

// A point (_t from 0 to 1) of the Catmull-Rom segment from _b to _c (_a before it, _d after).
fude_zoom_v2 fude_zoom_shape_catmull(fude_zoom_v2 _a, fude_zoom_v2 _b, fude_zoom_v2 _c, fude_zoom_v2 _d, f64 _t);

// A path's BEZIER HANDLES: a node's handle (x y from it) is the way the line leaves it
// (and, opposite, comes into it: smooth through it), dragged out by hand; (0, 0) the
// curve's own — a sixth of the way from the node before to the node after (a corner's:
// along each stretch), which is what the Catmull-Rom line through the nodes is.
// Kept after the nodes when any is set (FUDE_ZOOM_PATH_HANDLES in its first number).
//
// Its nodes (into _nodes) and handles (into _handles, zeros when none are kept): how
// many nodes; its first number's bits into *_bits.
u32  fude_zoom_path_unpack(const f64* _n, u32 _count, u64* _bits, fude_zoom_v2* _nodes, fude_zoom_v2* _handles);
// ...and its numbers again (FUDE_ZOOM_SHAPE_NUMBERS of room), its handles kept only when
// one is set. How many numbers.
u32  fude_zoom_path_pack(u64 _bits, const fude_zoom_v2* _nodes, const fude_zoom_v2* _handles, u32 _count, f64* _n);
// The way the line leaves node _i (_out) or comes into it (!_out), as a handle: its own
// handle, or the curve's own when it has none.
fude_zoom_v2 fude_zoom_path_tangent(const fude_zoom_v2* _nodes, const fude_zoom_v2* _handles, u32 _count, u64 _bits, u32 _i, b8 _out);

// A circle through stroke points _p (an arc drawn along a round edge): its centre and radius,
// and the angle it starts at and how far it goes round (signed: counter-clockwise +). False when
// the points are not on one (any further from it than _tolerance) or too few.
b8   fude_zoom_shape_fit_arc(const fude_zoom_v2* _p, u32 _n, f64 _tolerance, fude_zoom_v2* _centre, f64* _radius, f64* _from, f64* _sweep);

// A FILLET: the corner where lines a0→a1 and b0→b1 meet (or would, carried on) rounded
// by an arc _radius round, touching both: each line cut back to where the arc touches it
// (its far end kept: _a_end, _b_end the ends left at the corner's side moved there), the
// arc's centre, start and sweep. False: parallel, or the lines too short for that radius.
b8   fude_zoom_shape_fillet(fude_zoom_v2 _a0, fude_zoom_v2 _a1, fude_zoom_v2 _b0, fude_zoom_v2 _b1, f64 _radius,
                            fude_zoom_v2* _a_keep, fude_zoom_v2* _a_end, fude_zoom_v2* _b_keep, fude_zoom_v2* _b_end,
                            fude_zoom_v2* _centre, f64* _from, f64* _sweep);

// A CHAMFER: the same corner cut straight _back along each line from where they meet (0: the lines carried on or cut
// back to meet there, its corner sharp): each line's end at the corner's side moved there (its far end kept). False:
// parallel, or a line too short for it.
b8   fude_zoom_shape_chamfer(fude_zoom_v2 _a0, fude_zoom_v2 _a1, fude_zoom_v2 _b0, fude_zoom_v2 _b1, f64 _back,
                             fude_zoom_v2* _a_keep, fude_zoom_v2* _a_end, fude_zoom_v2* _b_keep, fude_zoom_v2* _b_end);

// What a board is made of: what the cut list says, and the wood it is drawn in
// (kept in its fourth number, over its grain: fude_zoom_board_look).
typedef enum {
    FUDE_ZOOM_MATERIAL_NONE = 0,   // not said: pale timber
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

// A board's numbers into _n (FUDE_ZOOM_SHAPE_NUMBERS of room): its half sizes
// (its frame's units), thickness (mm), look — its grain (bit 0: 1 across its
// length) and its material (the bits over it: FUDE_ZOOM_MATERIAL_) — and name
// (cut short at a whole character to fit). How many numbers.
u32  fude_zoom_board_numbers(f64* _n, f64 _half_length, f64 _half_width, f64 _thickness_mm, u8 _look, const c8* _name);
// Its look (grain | material << 1: what the above takes), grain (0 along, 1 across)
// and material, from its numbers (a board from before materials: none said).
u8   fude_zoom_board_look(const f64* _n, u32 _count);
u8   fude_zoom_board_grain(const f64* _n, u32 _count);
u8   fude_zoom_board_material(const f64* _n, u32 _count);
// The wood a material is drawn in (its fill).
rde_color fude_zoom_material_wood(u8 _material);
// A board's name from its numbers ("" when it has none).
void fude_zoom_board_name(const f64* _n, u32 _count, c8* _out, usize _size);
// A board cut (cut.h): past its name's twelve numbers, its rings — how many,
// then each one's point count and points (its own units, round its
// translation): ring 0 its outline, the others its holes. Its half sizes
// are its box's (what its label and the cut list say). Into _out (f64s,
// cleared first). How many numbers.
#define FUDE_ZOOM_BOARD_RINGS_AT 16u
u32  fude_zoom_board_cut_numbers(rde_arr* _out, f64 _half_length, f64 _half_width, f64 _thickness_mm, u8 _grain, const c8* _name,
                                 const fude_zoom_v2* _points, const u32* _rings, u32 _n);
// A board's outline and holes (its own units) into _points and _rings (fill.h's
// rings; cleared first): a cut board's own, else its rectangle. How many points.
u32  fude_zoom_board_rings(const f64* _n, u32 _count, rde_arr* _points, rde_arr* _rings);
// Whether a board has been cut (its outline its own, not its rectangle).
b8   fude_zoom_board_is_cut(const f64* _n, u32 _count);

// A shape as recognized or being placed, in whatever units its points came in.
typedef struct {
    u8           type;        // FUDE_ZOOM_SHAPE_ (0: none)
    fude_zoom_v2 at;          // its translation (a line's start; the others' middle)
    f64          rotation;
    f64          n[FUDE_ZOOM_SHAPE_NUMBERS];
    u32          count;       // numbers in n
} fude_zoom_shape_fit;

// The shape a stroke's points look like (_count of them, screen units: the
// tolerances are a hand's on screen). False: none — it stays a stroke.
b8   fude_zoom_shape_recognize(const fude_zoom_v2* _points, u32 _count, fude_zoom_shape_fit* _out);

// A shape's outline, its own units (before its translation, rotation, scale),
// into _out (rde_arr of fude_zoom_v2). _segments: how finely an ellipse or a
// round corner is cut. *_closed: whether the last point joins the first.
void fude_zoom_shape_outline(u8 _type, const f64* _n, u32 _count, u32 _segments, rde_arr* _out, b8* _closed);
// Segments enough for an outline _size across on screen to look round.
u32  fude_zoom_shape_segments(f64 _size_on_screen);

// A shape's line (a technical drawing's: research §2.10): solid; dashed, what is hidden; or long dash and dot, a
// centre line. Kept in the shape's q (scene.h).
typedef enum {
    FUDE_ZOOM_LINE_SOLID = 0,
    FUDE_ZOOM_LINE_DASHED,
    FUDE_ZOOM_LINE_DASH_DOT,
    FUDE_ZOOM_LINE_STYLES
} FUDE_ZOOM_LINE_;

// A shape's q keeps its line's style in its low bits and, above them, how a closed one is HATCHED: lines across its
// inside, 12 of its line's widths apart (a section on a drawing: they print as its lines do, at any scale).
typedef enum {
    FUDE_ZOOM_HATCH_NONE = 0,
    FUDE_ZOOM_HATCH_DIAGONAL,   // at 45°
    FUDE_ZOOM_HATCH_CROSS,      // at 45° and 135°
    FUDE_ZOOM_HATCH_KINDS
} FUDE_ZOOM_HATCH_;
#define FUDE_ZOOM_HATCH_SHIFT 4u
#define FUDE_ZOOM_HATCH_APART 12.0   // its lines this many of its line's widths apart
static inline u8 fude_zoom_line_style_of(i8 _q) { return (u8)((u8)_q & 0x07u); }
static inline u8 fude_zoom_hatch_of(i8 _q)      { return (u8)(((u8)_q >> FUDE_ZOOM_HATCH_SHIFT) & 0x03u); }
static inline i8 fude_zoom_q_of(u8 _style, u8 _hatch) { return (i8)((_style & 0x07u) | ((_hatch & 0x03u) << FUDE_ZOOM_HATCH_SHIFT)); }

// A hatch's lines: lines at _angle (radians), _apart apart, across the inside of the closed polygon _p (_n points; its
// inside as even-odd says: a hole is left out), only what falls in _view — into _out (fude_zoom_v2 pairs). How many.
u32  fude_zoom_hatch_lines(const fude_zoom_v2* _p, u32 _n, f64 _angle, f64 _apart, fude_zoom_box _view, rde_arr* _out);

// A line's dashes in style _style: the polyline _p (_n points; _closed: back to its first) cut into its pieces, _w its
// width (the pattern grows with it: a dash 7 widths long, its gap 3.5; a centre line's 12, 3, a dot, 3), counted
// along it from its first point — only what falls in _view (a box round the screen: deep in, a line runs to millions
// of points) — into _out (fude_zoom_v2 pairs: each piece's ends). How many pieces.
u32  fude_zoom_line_dashes(const fude_zoom_v2* _p, u32 _n, b8 _closed, u8 _style, f64 _w, fude_zoom_box _view, rde_arr* _out);

// A shape's numbers turned over (its own y the other way: a mirror image of it round its own x axis), in place:
// what a mirror's copy is drawn from (select.h). A rectangle, an ellipse, a plain board, a sheet, a symbol: as they are.
void fude_zoom_shape_mirror(u8 _type, f64* _n, u32 _count);

#endif
