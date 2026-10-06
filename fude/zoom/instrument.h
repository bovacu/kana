// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_INSTRUMENT
#define FUDE_ZOOM_INSTRUMENT

#include "rde.h"
#include "zoom/scene.h"
#include "zoom/units.h"

// ===========================================================================
// The drawing board's instruments (research §2.10): a ruler, the 45° set
// square (escuadra), the 30°/60° one (cartabón), a protractor and a compass,
// laid over the page like the real ones over paper.
//
//   THE COMPASS  its needle where the page puts it (stuck in the drawing: the
//            page keeps it on its point and its radius a real length as the
//            page zooms), its pencil out from it; drawing near its circle draws
//            along it (an arc; all the way round, a circle); its hinge, held,
//            moves the pencil (its radius and where it points); its legs
//            dragged move it, needle and all
//
//   THE CIRCLE TEMPLATE  a plate with a round hole in it, its diameter a real
//            length (the page keeps it so, as a compass's radius), typed on the
//            numpad from the Ø under it; the pen in the hole near its edge draws
//            round inside it (the line inside the hole, its width and all)
//
//   THE ELLIPSE TEMPLATE  the same with an elliptical hole, its width and
//            its height typed, one after the other
//
//   THE FRENCH CURVE  a plate whose whole edge is a curve, tight at one end
//            and opening out along it, for drawing smooth curves through
//            points: the pen near its edge draws along it, beside it
//
//   SHOWN    from the toolbar's Instruments, as many of each as wanted (a
//            dozen in all), each put away by the × on it; each lies on the
//            drawing where it is put (the page keeps it there as it moves and
//            zooms, its body the same size on the screen) — never saved in it
//   MOVED    a finger on one drags it; two turn it as well (the angle shown
//            while it turns, held a moment at each 15°); the pen or the mouse
//            drags it by its body and turns it by its knob
//   DOCKED   dragged against another's edge, nearly parallel, it lies along
//            it and then only slides along it — the set squares on a drawing
//            board, drawing parallels and perpendiculars; pulled away from
//            it, it lets go
//   DRAWING  the pen going down near an edge draws along it, the line exactly
//            on the edge (on the protractor's round edge, an arc), never past
//            its ends; how far along it has gone is shown as it draws
//
// The graduations are in real units, by how many millimetres a screen point
// is at the camera's zoom (the canvas's scale: page.h), finer as the page is
// zoomed in.
//
// Everything here is in screen points, the page's screen (centre origin, Y up).
// ===========================================================================

typedef enum {
    FUDE_ZOOM_INSTRUMENT_RULER = 0,
    FUDE_ZOOM_INSTRUMENT_SQUARE_45,
    FUDE_ZOOM_INSTRUMENT_SQUARE_30,
    FUDE_ZOOM_INSTRUMENT_PROTRACTOR,
    FUDE_ZOOM_INSTRUMENT_COMPASS,      // its needle at `at`, its pencil `radius` out along `angle`
    FUDE_ZOOM_INSTRUMENT_CIRCLE,       // a circle template: a plate with a round hole `radius` across from `at` (a real length, typed)
    FUDE_ZOOM_INSTRUMENT_ELLIPSE,      // an ellipse template: its hole `radius` wide and `radius2` tall from `at` (real lengths, typed)
    FUDE_ZOOM_INSTRUMENT_CURVE,        // a French curve: a curved plate whose edge bends tighter and looser along it
    FUDE_ZOOM_INSTRUMENT_CORNER,       // a corner radius gauge: a square plate, one corner a quarter circle `radius` round (a real
                                       // length, typed) — a corner's two lines along its sides, the round drawn along its arc
    FUDE_ZOOM_INSTRUMENT_STENCIL,      // a stencil: a piece of My pieces laid as an instrument (its lines: stencils[slot]) — a
                                       // see-through plate round them, the pen near one drawing along it, on it
    FUDE_ZOOM_INSTRUMENT_COUNT
} FUDE_ZOOM_INSTRUMENT_;

#define FUDE_ZOOM_INSTRUMENT_SNAP       18.0    // the pen this near an edge (outside it) draws along it
#define FUDE_ZOOM_INSTRUMENT_GRIP       14.0    // a thin one is held from this far out of its middle at least (half of what there is to take)
#define FUDE_ZOOM_INSTRUMENT_DOCK       14.0    // an edge this near another's, and nearly parallel, docks to it
#define FUDE_ZOOM_INSTRUMENT_DOCK_TURN  0.10    // ...within this many radians (about 6°)
#define FUDE_ZOOM_INSTRUMENT_UNDOCK     40.0    // pulled this far off the edge it lies on, it lets go
#define FUDE_ZOOM_INSTRUMENT_DETENT     0.026   // radians (1.5°) round each 15° where a turn rests
#define FUDE_ZOOM_INSTRUMENT_KNOB       14.0    // the knob's radius
#define FUDE_ZOOM_INSTRUMENT_EDGES      8u      // (a stencil's lines; the others have five at most)
#define FUDE_ZOOM_INSTRUMENT_MOST       12u     // out at once, of any kinds
#define FUDE_ZOOM_INSTRUMENT_CLOSE      11.0    // the radius of the × that puts one away
#define FUDE_ZOOM_INSTRUMENT_GROW       11.0    // ...and of the tab that makes it longer or bigger,
#define FUDE_ZOOM_INSTRUMENT_GROW_OUT   30.0    // ...this far past its end (clear of where the pen draws along it)

// An edge: from a to b, counter-clockwise round the instrument (its outside
// on the right). The protractor's round edge is an arc instead.
typedef struct {
    fude_zoom_v2 a, b;
    b8           arc;
    fude_zoom_v2 centre;        // the arc's
    f64          radius, from;  // ...its radius, and where it starts (radians, counter-clockwise for π)
    b8           graduated;     // ticks along it
    b8           zero_at_b;     // its graduations count from b (a set square's leg, from the right angle)
    f64          span;          // an arc's sweep (radians): the protractor's half turn, the compass's whole one
    b8           curve;         // a closed curve instead (fude_zoom_instrument_curve: an ellipse template's hole, a French curve's edge)
    b8           hole;          // ...the instrument round it, not inside it (the line is drawn inside)
    f64          room;          // how deep its plate is behind it, for its marks (0: deep enough)
    u8           line;          // a curve's: which of the instrument's (a stencil's line; else 0)
    b8           open;          // ...a line with two ends, not a loop (a stencil's stroke)
    b8           trace;         // the line drawn ON it, not beside it (a stencil's: traced)
} fude_zoom_instrument_edge;

#define FUDE_ZOOM_INSTRUMENT_CURVE_POINTS 192u
#define FUDE_ZOOM_STENCIL_W               300.0   // a stencil's larger side at size 1 (its own units)

// A stencil's lines (page.c lays a piece: piece.h), its own units — its larger side FUDE_ZOOM_STENCIL_W at size 1,
// round its middle — each FUDE_ZOOM_INSTRUMENT_CURVE_POINTS points at most; and its plate's half sizes (its lines'
// box and a margin).
typedef struct {
    fude_zoom_v2 points[FUDE_ZOOM_INSTRUMENT_EDGES * FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
    u16          first[FUDE_ZOOM_INSTRUMENT_EDGES], count[FUDE_ZOOM_INSTRUMENT_EDGES];
    b8           closed[FUDE_ZOOM_INSTRUMENT_EDGES];
    u32          lines;
    f64          hw, hh;
    u32          piece;     // the piece it is (piece.h's id)
} fude_zoom_stencil;
#define FUDE_ZOOM_INSTRUMENT_RULER_WIDTH  64.0    // a ruler's band as it comes out (points; thick: how much wider now)
#define FUDE_ZOOM_INSTRUMENT_CROSSES      32u     // crossings of the edge drawn along, kept
#define FUDE_ZOOM_INSTRUMENT_CONTACTS     12u     // the others one lies against at once, at most (besides the one it slides along)

// One out. Everything below takes one by its place in tools (a "slot").
typedef struct {
    b8           shown;
    u8           kind;      // FUDE_ZOOM_INSTRUMENT_
    u32          serial;    // which it is (a slot used again holds another)
    fude_zoom_v2 at;        // where it is: the middle of its body (the protractor's: the centre of its arc)
    f64          angle;     // its turn, radians
    f64          size;      // how much longer (a ruler) or bigger than when it came out (0: as it came)
    f64          thick;     // a ruler's band: how much wider than when it came out (0: as it came)
    // A compass's radius on the screen (a circle template's hole's; an ellipse template's
    // hole half as wide, and radius2 half as tall).
    f64          radius;
    f64          radius2;   // (a circle template's: its plate's rim round the hole; a corner gauge's: its plate past its round — both real lengths too)
    // How much bigger on the screen than when it came out (the zoom since: the page's; 0: as it came) —
    // its knob, its × and its tabs, and an ellipse template's rim, with it (never too small to take).
    f64          look;
    f64          look_mpp;  // (the page's: millimetres a point when it came out)
    // The page's: where it lies on the drawing and how big it is there — the
    // point of it pinned (pin: its key, fude_zoom_instrument_keys, it was snapped
    // by: a corner, a zero mark; -1: its middle — a compass's needle, a hole's
    // centre) and that point in the drawing (a frame, a point in its units), and
    // its size a real length (stuck_mm: a ruler's length, a set square's long
    // leg, a protractor's radius, a French curve's width; a compass's or a
    // template's radius), so it stays on the drawing, as big as it is on it,
    // as the page moves and zooms — a ruler's band too (stuck_mm2: its width),
    // so a line along either edge stays on it (its marks the same on the screen).
    b8           stuck;
    i32          pin;
    u32          stuck_frame;
    fude_zoom_v2 stuck_point;
    f64          stuck_mm;
    f64          stuck_mm2;   // (an ellipse template's height's half; a ruler's width)
} fude_zoom_instrument;

typedef struct {
    fude_zoom_instrument tools[FUDE_ZOOM_INSTRUMENT_MOST];
    u8                   order[FUDE_ZOOM_INSTRUMENT_MOST];   // drawn in this order, the last on top
    fude_zoom_stencil    stencils[FUDE_ZOOM_INSTRUMENT_MOST];   // a stencil's lines, by its slot
    u32                  serials;
    // A hand on one: which (-1: none), its fingers (the pen and the mouse are
    // a finger of their own), and where things were when they took hold.
    i32                  held;
    u32                  hands;
    u64                  hand_id[2];
    fude_zoom_v2         hand_at[2];
    fude_zoom_v2         grab_at;        // the instrument's place...
    f64                  grab_angle;     // ...and turn when the hands last changed
    fude_zoom_v2         grab_hand[2];   // the hands then
    b8                   turning;        // the knob in hand (the pen's or the mouse's)
    f64                  turned_at;      // the angle shown until a moment after this
    // Lying along another's edge: which, and its edge and our own (-1: free).
    i32                  docked;
    u32                  dock_edge, own_edge;
    fude_zoom_v2         dock_from;      // where it docked (the pull away is measured from its line)
    // ...and resting against others besides, as many as it touches (slid along the first, it stops
    // flush against the next in its way): each one's slot, its edge, and our own edge or corner there.
    u32                  contacts;
    i32                  contact_slot[FUDE_ZOOM_INSTRUMENT_CONTACTS];
    u32                  contact_edge[FUDE_ZOOM_INSTRUMENT_CONTACTS];
    u32                  contact_own[FUDE_ZOOM_INSTRUMENT_CONTACTS];
    // The pen along an edge: which instrument (-1: none), which edge, and
    // where along it the line began (its distance from a, or its angle).
    i32                  ruled;
    u32                  ruled_edge;
    f64                  ruled_from, ruled_now;
    // Chained: where this edge crosses the others shown (worked out when the pen takes
    // it), what each crosses it with, and how far along this one; where the line began
    // on it (no turn there) and where it is; held at a crossing while it is not yet
    // clear which way the pen goes; and, once, the corner it turned at, and which way
    // was out of the edge it left.
    u32                  cross_n;
    fude_zoom_v2         cross_at[FUDE_ZOOM_INSTRUMENT_CROSSES];
    f64                  cross_along[FUDE_ZOOM_INSTRUMENT_CROSSES];
    u32                  cross_slot[FUDE_ZOOM_INSTRUMENT_CROSSES], cross_edge[FUDE_ZOOM_INSTRUMENT_CROSSES];
    fude_zoom_v2         ruled_start;
    b8                   ruled_cornered; // it began on this edge at a corner it turned at (the line cannot go back into the one it left)
    i32                  ruled_held;     // the crossing held at (-1: none)...
    f64                  ruled_hold_way; // ...and the way the line came to it along the edge (+1, -1)
    b8                   ruled_turned;
    fude_zoom_v2         ruled_corner, ruled_out_before;
    // A compass's radius, and the size, when the hands last changed; its size tab in hand.
    f64                  grab_r;
    f64                  grab_size;
    b8                   growing;
    // A × pressed: whose (-1: none, or the press went off it) and by which hand
    // (0: none), and where; let go there, that one is put away.
    i32                  closing;
    u64                  closing_hand;
    fude_zoom_v2         closing_at;
} fude_zoom_instruments;

#define FUDE_ZOOM_COMPASS_R 120.0   // the compass's radius when first shown (screen points)

// What of instrument _slot goes onto things drawn when it is dragged near them
// (the page snaps it: snap.h): its corners, a ruler's zero marks, a
// protractor's centre and the ends of its straight edge, a compass's needle.
// How many (at most FUDE_ZOOM_INSTRUMENT_KEYS).
#define FUDE_ZOOM_INSTRUMENT_KEYS 8u
u32  fude_zoom_instrument_keys(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2* _out);

// A compass's hinge (the knob that opens it) and where its radius is written
// (tapped: typed) — a circle template's diameter, under it.
fude_zoom_v2 fude_zoom_instrument_compass_hinge(const fude_zoom_instruments* _ins, u32 _slot);
fude_zoom_v2 fude_zoom_instrument_compass_label(const fude_zoom_instruments* _ins, u32 _slot);
// Its radius a real length the page keeps (a compass, a circle or an ellipse template)?
b8   fude_zoom_instrument_round(u8 _kind);
// A circle template's rim, a corner gauge's band past its round (screen points): its radius2, or as it came.
f64  fude_zoom_instrument_band(const fude_zoom_instrument* _t);
// A curved edge's points on the screen, round it counter-clockwise (an ellipse
// template's hole; a French curve's edge), at most _max. How many (0: none).
u32  fude_zoom_instrument_curve(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2* _out, u32 _max);
// A point on the screen in instrument _slot's own units (its middle the origin, turned with it), and back.
fude_zoom_v2 fude_zoom_instrument_local(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2 _screen);
fude_zoom_v2 fude_zoom_instrument_screen(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2 _local);
// Its size tab (a ruler's past its far end, a set square's past its sharp corner,
// a protractor's past its straight edge's end; a compass has none: false):
// dragged, it grows or shrinks it, the other end kept where it is.
b8   fude_zoom_instrument_grow_at(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2* _at);
// What its size tab sets, on the screen: a ruler's length, a set square's long leg, a protractor's radius (a compass's radius).
f64  fude_zoom_instrument_extent(const fude_zoom_instruments* _ins, u32 _slot);
// ...the same at its first size (what `size` is a multiple of).
f64  fude_zoom_instrument_extent_unit(u8 _kind);
// Its × (tapped: put away).
fude_zoom_v2 fude_zoom_instrument_close_at(const fude_zoom_instruments* _ins, u32 _slot);

#define FUDE_ZOOM_INSTRUMENT_PEN_HAND 0xFFFFFFFFFFFFFFF0ull   // the pen's or the mouse's hand

void fude_zoom_instruments_init(fude_zoom_instruments* _ins);
// Any shown? How many of _kind?
b8   fude_zoom_instruments_any(const fude_zoom_instruments* _ins);
u32  fude_zoom_instruments_count(const fude_zoom_instruments* _ins, FUDE_ZOOM_INSTRUMENT_ _kind);
// Another of _kind brought out (_half: the screen's half-size, to put it near
// its middle; each after the first a little further on). Its slot (-1: a dozen out already).
i32  fude_zoom_instrument_add(fude_zoom_instruments* _ins, FUDE_ZOOM_INSTRUMENT_ _kind, fude_zoom_v2 _half);
// Put away.
void fude_zoom_instrument_remove(fude_zoom_instruments* _ins, u32 _slot);

// Its edges on the screen now (at most FUDE_ZOOM_INSTRUMENT_EDGES). How many.
u32  fude_zoom_instrument_edges(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_instrument_edge* _out);
// Its outline on the screen (counter-clockwise; an arc in many short sides). How many corners.
u32  fude_zoom_instrument_outline(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2* _out, u32 _max);
// Its knob (where the pen or the mouse turns it).
fude_zoom_v2 fude_zoom_instrument_knob(const fude_zoom_instruments* _ins, u32 _slot);
// The shown instrument at _screen, the one on top (-1: none).
i32  fude_zoom_instruments_at(const fude_zoom_instruments* _ins, fude_zoom_v2 _screen);

// Fingers: one going down on an instrument (or a second near the one held)
// is taken — true: the page leaves it alone; moves and lifts of a taken one
// are taken too. Down and up on one's ×, it is put away.
b8   fude_zoom_instruments_finger_down(fude_zoom_instruments* _ins, u64 _id, fude_zoom_v2 _at);
b8   fude_zoom_instruments_finger_moved(fude_zoom_instruments* _ins, u64 _id, fude_zoom_v2 _at);
b8   fude_zoom_instruments_finger_up(fude_zoom_instruments* _ins, u64 _id);

// The pen (or the mouse) going down: what it does there.
typedef enum {
    FUDE_ZOOM_INSTRUMENT_PEN_NONE = 0,   // not at an instrument: it draws as ever
    FUDE_ZOOM_INSTRUMENT_PEN_RULED,      // near an edge: it draws along it (_snapped: where it starts)
    FUDE_ZOOM_INSTRUMENT_PEN_HELD        // on a body or a knob: it moves or turns it until it lifts (on a ×: lifted there, puts it away)
} FUDE_ZOOM_INSTRUMENT_PEN_;
FUDE_ZOOM_INSTRUMENT_PEN_ fude_zoom_instruments_pen_down(fude_zoom_instruments* _ins, fude_zoom_v2 _at, fude_zoom_v2* _snapped);
// The pen moved: ruled, where on the edge it draws now; held, the instrument follows (and _at comes back).
fude_zoom_v2 fude_zoom_instruments_pen_moved(fude_zoom_instruments* _ins, fude_zoom_v2 _at);
void fude_zoom_instruments_pen_up(fude_zoom_instruments* _ins);

// Drawing along an edge: how far it has gone (screen points, or degrees on
// the protractor's arc: _degrees). False: not drawing along one.
b8   fude_zoom_instruments_ruled(const fude_zoom_instruments* _ins, f64* _along, b8* _degrees, fude_zoom_v2* _label_at);
// Drawing along an edge: the way out of the instrument there (a unit; the line
// is drawn beside the edge, as a pencil against a ruler). A compass's circle has
// none (0, 0): its pencil draws on it.
fude_zoom_v2 fude_zoom_instruments_ruled_outward(const fude_zoom_instruments* _ins);
// Drawing along an edge that is straight (a line kept by its ends: not an arc, not a curve)?
b8   fude_zoom_instruments_ruled_straight(const fude_zoom_instruments* _ins);
// The edge drawn along is traced (a stencil's: the line on it, not beside it).
b8   fude_zoom_instruments_ruled_traced(const fude_zoom_instruments* _ins);
// ...or round (an arc of a circle: a compass's, the protractor's, a corner gauge's round, a
// circle template's hole — a line along it kept as an exact arc)?
b8   fude_zoom_instruments_ruled_round(const fude_zoom_instruments* _ins);
// CHAINED INSTRUMENTS: drawing along an edge, where it crosses another's (another
// instrument's, or this one's next edge — a set square's corner) the line turns
// onto that one once the pen has gone past the crossing nearer it than this one,
// the corner exactly at the crossing; going on past it near this edge, the line
// cannot go on into the instrument it meets (as with real ones): it stays at the
// crossing till the pen turns (or goes back) — where nothing is in the way (a
// compass's circle, leaving one it began under) it goes on. A turn since last
// asked: true, once, with the crossing and which way was out of the edge it left
// (for the corner beside both).
b8   fude_zoom_instruments_ruled_turn(fude_zoom_instruments* _ins, fude_zoom_v2* _corner, fude_zoom_v2* _out_before);

// How they look: the colours, the font for the numbers, and how many
// millimetres a screen point is (the graduations).
typedef struct {
    rde_font* font;
    f32       font_px;
    f64       mm_per_point;
    rde_color body, line, tick, text, accent;
    const fude_zoom_units_style* units;   // a compass's radius written in these ("R 25 mm"; NULL: not written)
} fude_zoom_instruments_look;
void fude_zoom_instruments_render(const fude_zoom_instruments* _ins, const fude_zoom_instruments_look* _look, f64 _now);

#endif
