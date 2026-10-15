// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_INSTRUMENT
#define FUDE_ZOOM_INSTRUMENT

#include "rde.h"
#include "zoom/scene.h"
#include "zoom/units.h"

// ===========================================================================
// Instruments: the drawing board's instruments (docs/research/infinite_canvas_research.md §2.10) laid over the page as
// real ones lie over paper: a ruler, the 45° set square (escuadra), the 30°/60° one (cartabón), a protractor, a
// compass, a circle template, an ellipse template, a French curve, a corner radius gauge, and stencils made of My
// pieces.
//
//   THE COMPASS  Its needle is where the page puts it: stuck in the drawing, the page keeps it on its point, and its
//            radius a real length, as the page zooms. Its pencil stands out from the needle. Drawing near its circle
//            draws along it (an arc; all the way round, a circle). Its hinge, held, moves the pencil (its radius and
//            where it points); its legs, dragged, move the whole compass, needle and all.
//
//   THE CIRCLE TEMPLATE  A plate with a round hole in it, the hole's diameter a real length (the page keeps it so, as
//            it keeps a compass's radius), typed on the numpad from the Ø under it. The pen in the hole near its edge
//            draws round inside it (the whole line inside the hole, its width and all).
//
//   THE ELLIPSE TEMPLATE  The same with an elliptical hole, its width and its height typed one after the other.
//
//   THE FRENCH CURVE  A plate whose whole edge is a curve, tight at one end and opening out along it, for drawing
//            smooth curves through points: the pen near its edge draws along it, beside it.
//
//   SHOWN    From the toolbar's Instruments, as many of each as wanted (a dozen in all), each put away by the × on
//            it. Each lies on the drawing where it is put: the page keeps it there as the view moves and zooms, its
//            body the same size on the screen. Instruments are not saved in the drawing itself (see Files below).
//   MOVED    A finger on one drags it; two fingers turn it as well (the angle is shown while it turns, and it rests a
//            moment at each 15°). The pen or the mouse drags it by its body and turns it by its knob.
//   DOCKED   Dragged against another's edge, nearly parallel, it lies along that edge and then only slides along it,
//            as set squares do on a drawing board to draw parallels and perpendiculars. Pulled away, it lets go.
//   DRAWING  The pen going down near an edge draws along it, the line exactly on the edge (on the protractor's round
//            edge, an arc) and never past its ends; how far along it has gone is shown as it draws.
//
// The graduations are in real units, from how many millimetres a screen point is at the camera's zoom (the canvas's
// scale: page.h), and get finer as the page is zoomed in.
//
// Everything here is in screen points, on the page's screen (origin at the centre, Y up).
//
// Where it sits: `fude/zoom`, the canvas layer. The page shows the instruments and keeps them on the drawing as the
// view moves (page_instruments.c), hands them the fingers, the pen and the mouse (page_input.c), and draws them
// (page_render.c).
// Its main type, fude_zoom_instruments, is the caller's struct (the page keeps one), set up by
// fude_zoom_instruments_init; it holds everything in fixed arrays, so there is nothing to free.
// Threads: main thread only. Following a chain of crossing edges keeps a recursion count in a variable of the module.
// Files: none here. page_file.c keeps the instruments beside the canvas's file, as `<canvas>.tools` (written when the
// canvas is left, read when it is opened): each one's kind, turn, size, the point of it held and where that is.
// Design: docs/infinite_canvas_design.md §15, §17 and §20 (the instruments, how they snap, lie on the drawing, grow,
// and chain).
// ===========================================================================

// The kinds of instrument.
typedef enum {
    FUDE_ZOOM_INSTRUMENT_RULER = 0,
    FUDE_ZOOM_INSTRUMENT_SQUARE_45,
    FUDE_ZOOM_INSTRUMENT_SQUARE_30,
    FUDE_ZOOM_INSTRUMENT_PROTRACTOR,
    FUDE_ZOOM_INSTRUMENT_COMPASS,      // its needle at `at`, its pencil `radius` out along `angle`
    // A circle template: a plate with a round hole of radius `radius` round `at` (a real length, typed).
    FUDE_ZOOM_INSTRUMENT_CIRCLE,       // a circle template: a plate with a round hole
    // An ellipse template: its hole's half width is `radius` and its half height `radius2`, round `at` (real lengths,
    // typed).
    FUDE_ZOOM_INSTRUMENT_ELLIPSE,      // an ellipse template: a plate with an elliptical hole
    FUDE_ZOOM_INSTRUMENT_CURVE,        // a French curve: a curved plate whose edge bends tighter and looser along it
    // A corner radius gauge: a square plate with one corner rounded to a quarter circle of radius `radius` (a real
    // length, typed). Drawing along it gives a corner's two lines along its sides, and the round along its arc.
    FUDE_ZOOM_INSTRUMENT_CORNER,       // a corner radius gauge
    // A stencil: a piece of My pieces laid as an instrument (its lines are stencils[slot]), with a see-through plate
    // round them; the pen near one of its lines draws along it, on it.
    FUDE_ZOOM_INSTRUMENT_STENCIL,      // a stencil made of one of My pieces
    FUDE_ZOOM_INSTRUMENT_COUNT
} FUDE_ZOOM_INSTRUMENT_;

// Distances in screen points, and angles in radians, that set how the instruments are handled.
#define FUDE_ZOOM_INSTRUMENT_SNAP       18.0    // the pen this near an edge (outside it) draws along it
// A thin instrument can be taken anywhere within this distance either side of its middle, even just outside it: this
// is half the width there is to take hold of.
#define FUDE_ZOOM_INSTRUMENT_GRIP       14.0    // a thin one can be taken this near its middle at least
#define FUDE_ZOOM_INSTRUMENT_DOCK       14.0    // an edge this near another's, and nearly parallel, docks to it
#define FUDE_ZOOM_INSTRUMENT_DOCK_TURN  0.10    // nearly parallel: within this many radians (about 6°)
#define FUDE_ZOOM_INSTRUMENT_UNDOCK     40.0    // pulled this far off the edge it lies on, it lets go
#define FUDE_ZOOM_INSTRUMENT_DETENT     0.026   // radians (1.5°) round each 15° where a turn rests
#define FUDE_ZOOM_INSTRUMENT_KNOB       14.0    // the knob's radius
#define FUDE_ZOOM_INSTRUMENT_EDGES      8u      // the most edges one has (a stencil's lines; the others have 5 at most)
#define FUDE_ZOOM_INSTRUMENT_MOST       12u     // the most out at once, of any kinds
#define FUDE_ZOOM_INSTRUMENT_CLOSE      11.0    // the radius of the × that puts one away
#define FUDE_ZOOM_INSTRUMENT_GROW       11.0    // the radius of the tab that makes it longer or bigger,
#define FUDE_ZOOM_INSTRUMENT_GROW_OUT   30.0    // which is this far past its end (clear of where the pen draws)

// An edge, from a to b, going counter-clockwise round the instrument (its outside on the right). The protractor's
// round edge is an arc instead.
typedef struct {
    fude_zoom_v2 a, b;
    b8           arc;
    fude_zoom_v2 centre;        // the arc's centre
    f64          radius, from;  // the arc's radius, and where it starts (radians; it runs counter-clockwise)
    b8           graduated;     // it has ticks along it
    b8           zero_at_b;     // its graduations count from b (a set square's leg, from the right angle)
    f64          span;          // an arc's sweep (radians): the protractor's half turn, the compass's whole one
    // A curve instead, whose points fude_zoom_instrument_curve gives: an ellipse template's hole, a French curve's
    // edge (closed curves), or a stencil's line.
    b8           curve;         // a curve instead of a straight line or an arc (see above)
    b8           hole;          // the instrument is round it, not inside it (so the line is drawn inside)
    f64          room;          // how deep its plate is behind it, for its marks (0: deep enough)
    u8           line;          // a curve's: which of the instrument's lines it is (a stencil's line; else 0)
    b8           open;          // a line with two ends, not a loop (a stencil's stroke)
    b8           trace;         // the line is drawn on it, not beside it (a stencil's lines are traced)
} fude_zoom_instrument_edge;

// The most points of a curved edge (a stencil's line, an ellipse template's hole, a French curve).
#define FUDE_ZOOM_INSTRUMENT_CURVE_POINTS 192u
#define FUDE_ZOOM_STENCIL_W               300.0   // a stencil's larger side at size 1 (its own units)

// A stencil's lines (page_pieces.c lays a piece: piece.h), in its own units: its larger side FUDE_ZOOM_STENCIL_W at
// size 1, round its middle. Line i has count[i] points from points[first[i]] (at most
// FUDE_ZOOM_INSTRUMENT_CURVE_POINTS), and closed[i] says whether it is a loop. hw and hh are its plate's half sizes:
// its lines' box and a margin.
typedef struct {
    fude_zoom_v2 points[FUDE_ZOOM_INSTRUMENT_EDGES * FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
    u16          first[FUDE_ZOOM_INSTRUMENT_EDGES], count[FUDE_ZOOM_INSTRUMENT_EDGES];
    b8           closed[FUDE_ZOOM_INSTRUMENT_EDGES];
    u32          lines;
    f64          hw, hh;
    u32          piece;     // the piece it is (piece.h's id)
} fude_zoom_stencil;
#define FUDE_ZOOM_INSTRUMENT_RULER_WIDTH  64.0    // a ruler's band as it comes out (points; `thick` makes it wider)
#define FUDE_ZOOM_INSTRUMENT_CROSSES      32u     // the most crossings kept of the edge drawn along
#define FUDE_ZOOM_INSTRUMENT_CONTACTS     12u     // the most others one rests against (besides the one it slides on)

// One instrument out. Everything below takes one by its index in `tools` (its "slot").
typedef struct {
    b8           shown;
    u8           kind;      // a FUDE_ZOOM_INSTRUMENT_
    u32          serial;    // which one it is (a slot used again holds another)
    fude_zoom_v2 at;        // where it is: the middle of its body (the protractor's: the centre of its arc)
    f64          angle;     // its turn, in radians
    f64          size;      // how many times longer (a ruler) or bigger it is than when it came out (0: as it came)
    f64          thick;     // a ruler's band: how many times wider than when it came out (0: as it came)
    // A compass's radius on the screen. For a circle template, its hole's radius; for an ellipse template, half its
    // hole's width (and radius2 half its height); for a corner gauge, its round's radius; for a stencil, its plate's
    // half width (and radius2 its half height).
    f64          radius;
    // For a circle template, its plate's rim round the hole; for a corner gauge, its plate past its round (both real
    // lengths too, kept by the page).
    f64          radius2;   // a second size, as above
    // How many times bigger it is on the screen than when it came out (the page's zoom since; 0: as it came). Its
    // knob, its × and its tabs, and an ellipse template's rim, grow with it (but never get too small to take).
    f64          look;
    f64          look_mpp;  // the page's millimetres a point when it came out
    // The page's record of where it lies on the drawing and how big it is there, so that it stays on the drawing, as
    // big as it is on it, as the page moves and zooms. `pin` is the key (fude_zoom_instrument_keys) it was snapped by:
    // a corner, a zero mark; -1 for its middle (a compass's needle, a hole's centre). `stuck_frame` and `stuck_point`
    // are that point in the drawing (a frame, and a point in its units). `stuck_mm` is its size as a real length: a
    // ruler's length, a set square's long leg, a protractor's radius, a French curve's width, a compass's or a
    // template's radius. A ruler's band keeps its width too (stuck_mm2), so a line along either edge stays on it
    // (its marks the same on the screen).
    b8           stuck;
    i32          pin;
    u32          stuck_frame;
    fude_zoom_v2 stuck_point;
    f64          stuck_mm;
    // A second real length: an ellipse template's half height, a ruler's width. A stencil keeps its piece's id here
    // (piece.h's u32, which an f64 holds exactly): a stencil has no second length, and what sizes the instruments
    // writes this only for the round ones and rulers (page_instruments.c), so the id stays as put. Kept here rather
    // than in a field of its own so the canvas's <id>.tools record (page_file.c) needs no change of format.
    f64          stuck_mm2;   // a second length, as above (a stencil: its piece's id)
} fude_zoom_instrument;

// All the instruments, and what hands and the pen are doing with them.
typedef struct {
    fude_zoom_instrument tools[FUDE_ZOOM_INSTRUMENT_MOST];
    u8                   order[FUDE_ZOOM_INSTRUMENT_MOST];   // the slots in drawing order, the last on top
    fude_zoom_stencil    stencils[FUDE_ZOOM_INSTRUMENT_MOST];   // each stencil's lines, by its slot
    u32                  serials;
    // A hand on one: which (-1: none), its fingers (the pen and the mouse count as a finger of their own), and where
    // things were when they took hold.
    i32                  held;
    u32                  hands;
    u64                  hand_id[2];
    fude_zoom_v2         hand_at[2];
    fude_zoom_v2         grab_at;        // the instrument's place...
    f64                  grab_angle;     // ...and turn when the hands last changed
    fude_zoom_v2         grab_hand[2];   // the hands then
    b8                   turning;        // its knob is in hand (the pen's or the mouse's)
    f64                  turned_at;      // when it last turned (engine time): the angle is shown until a moment after
    // Lying along another's edge: which one, its edge, and our own edge (-1: free).
    i32                  docked;
    u32                  dock_edge, own_edge;
    fude_zoom_v2         dock_from;      // where it docked (pulling away is measured from that line)
    // Resting against others besides, as many as it touches (slid along the first, it stops flush against the next in
    // its way): each one's slot, its edge, and our own edge or corner there.
    u32                  contacts;
    i32                  contact_slot[FUDE_ZOOM_INSTRUMENT_CONTACTS];
    u32                  contact_edge[FUDE_ZOOM_INSTRUMENT_CONTACTS];
    u32                  contact_own[FUDE_ZOOM_INSTRUMENT_CONTACTS];
    // The pen along an edge: which instrument (-1: none), which edge, and where along it the line began and is now
    // (its distance from a, or its angle).
    i32                  ruled;
    u32                  ruled_edge;
    f64                  ruled_from, ruled_now;
    // Chained edges: where this edge crosses the others shown (worked out when the pen takes it), what crosses it
    // there and how far along this one; where the line began on it (no turn there); a crossing held while it is not
    // yet clear which way the pen goes; and, once, the corner it turned at and which way was out of the edge it left.
    u32                  cross_n;
    fude_zoom_v2         cross_at[FUDE_ZOOM_INSTRUMENT_CROSSES];
    f64                  cross_along[FUDE_ZOOM_INSTRUMENT_CROSSES];
    u32                  cross_slot[FUDE_ZOOM_INSTRUMENT_CROSSES], cross_edge[FUDE_ZOOM_INSTRUMENT_CROSSES];
    fude_zoom_v2         ruled_start;
    // Whether it began on this edge at a corner it turned at, so the line cannot go back into the edge it left.
    b8                   ruled_cornered; // it began on this edge at a corner it turned at
    i32                  ruled_held;     // the crossing held at (-1: none)...
    f64                  ruled_hold_way; // ...and the way the line came to it along the edge (+1, -1)
    b8                   ruled_turned;
    fude_zoom_v2         ruled_corner, ruled_out_before;
    // A compass's radius and the instrument's size when the hands last changed, and whether its size tab is in hand.
    f64                  grab_r;
    f64                  grab_size;
    b8                   growing;
    // A × pressed: whose (-1: none, or the press went off it), by which hand (0: none), and where. Let go there, that
    // instrument is put away.
    i32                  closing;
    u64                  closing_hand;
    fude_zoom_v2         closing_at;
} fude_zoom_instruments;

#define FUDE_ZOOM_COMPASS_R 120.0   // the compass's radius when first shown (screen points)

// The most snapping points (keys) an instrument has.
#define FUDE_ZOOM_INSTRUMENT_KEYS 8u
// Writes into _out the points of instrument _slot that snap onto things drawn when it is dragged near them (the page
// snaps it: snap.h): its corners, a ruler's zero marks, a protractor's centre and the ends of its straight edge, a
// compass's needle. _out must have room for FUDE_ZOOM_INSTRUMENT_KEYS. Returns how many.
u32  fude_zoom_instrument_keys(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2* _out);

// Return where a compass's hinge is (the knob that opens it), and where its radius is written (tapped, it is typed);
// for a circle template, an ellipse template or a corner gauge, where its size is written, under it.
fude_zoom_v2 fude_zoom_instrument_compass_hinge(const fude_zoom_instruments* _ins, u32 _slot);
fude_zoom_v2 fude_zoom_instrument_compass_label(const fude_zoom_instruments* _ins, u32 _slot);
// Returns whether a kind's radius is a real length the page keeps: a compass, a circle or an ellipse template, or a
// corner gauge.
b8   fude_zoom_instrument_round(u8 _kind);
// Returns a circle template's rim or a corner gauge's band past its round (screen points): its radius2, or as it came.
f64  fude_zoom_instrument_band(const fude_zoom_instrument* _t);
// Writes a curved edge's points on the screen into _out, going counter-clockwise round it (an ellipse template's hole,
// a French curve's edge), at most _max (and at most FUDE_ZOOM_INSTRUMENT_CURVE_POINTS). Returns how many; 0 for an
// instrument with no such curve.
u32  fude_zoom_instrument_curve(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2* _out, u32 _max);
// Turn a point on the screen into instrument _slot's own units (its middle the origin, turned with it), and back.
fude_zoom_v2 fude_zoom_instrument_local(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2 _screen);
fude_zoom_v2 fude_zoom_instrument_screen(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2 _local);
// Gives where its size tab is: a ruler's past its far end, a set square's past its sharp corner, a protractor's past
// its straight edge's end, a French curve's past its loose end, a stencil's past its plate. Dragged, the tab makes it
// longer or shorter, its other end staying where it is. Returns false for one with no tab (a compass, whose hinge
// opens it, and the templates and the corner gauge, whose sizes are typed).
b8   fude_zoom_instrument_grow_at(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2* _at);
// Returns what its size tab sets, on the screen: a ruler's length, a set square's long leg, a protractor's radius, a
// French curve's or a stencil's width; for the others, their radius.
f64  fude_zoom_instrument_extent(const fude_zoom_instruments* _ins, u32 _slot);
// Returns the same at a kind's first size, which `size` is a multiple of (1 for the kinds sized by their radius).
f64  fude_zoom_instrument_extent_unit(u8 _kind);
// Returns where its × is (tapped, it puts the instrument away).
fude_zoom_v2 fude_zoom_instrument_close_at(const fude_zoom_instruments* _ins, u32 _slot);

#define FUDE_ZOOM_INSTRUMENT_PEN_HAND 0xFFFFFFFFFFFFFFF0ull   // the id of the pen's or the mouse's hand

// Sets up the instruments with none shown.
void fude_zoom_instruments_init(fude_zoom_instruments* _ins);
// fude_zoom_instruments_any returns whether any is shown; fude_zoom_instruments_count returns how many of _kind are.
b8   fude_zoom_instruments_any(const fude_zoom_instruments* _ins);
u32  fude_zoom_instruments_count(const fude_zoom_instruments* _ins, FUDE_ZOOM_INSTRUMENT_ _kind);
// Brings out another of _kind, on top. _half is the screen's half size, to put it near the middle; each one after
// the first of its kind goes a little further down and to the right. Returns its slot, or -1 when a dozen are out
// already.
i32  fude_zoom_instrument_add(fude_zoom_instruments* _ins, FUDE_ZOOM_INSTRUMENT_ _kind, fude_zoom_v2 _half);
// Puts instrument _slot away, letting go of any hand or pen on it.
void fude_zoom_instrument_remove(fude_zoom_instruments* _ins, u32 _slot);

// Writes its edges on the screen as they are now into _out, which must have room for FUDE_ZOOM_INSTRUMENT_EDGES.
// Returns how many.
u32  fude_zoom_instrument_edges(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_instrument_edge* _out);
// Writes its outline on the screen into _out (counter-clockwise; an arc as many short sides), at most _max corners.
// Returns how many.
u32  fude_zoom_instrument_outline(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2* _out, u32 _max);
// Returns where its knob is (where the pen or the mouse turns it); far off the screen for a circle template, which
// has none.
fude_zoom_v2 fude_zoom_instrument_knob(const fude_zoom_instruments* _ins, u32 _slot);
// Returns the shown instrument at _screen, the one on top, or -1 when there is none.
i32  fude_zoom_instruments_at(const fude_zoom_instruments* _ins, fude_zoom_v2 _screen);

// Fingers. A finger going down on an instrument (or a second finger near the one held) is taken: true means the page
// leaves it alone, and the moves and the lift of a finger taken are taken too. A finger down and up on a ×
// puts that instrument away. _id tells the fingers apart.
b8   fude_zoom_instruments_finger_down(fude_zoom_instruments* _ins, u64 _id, fude_zoom_v2 _at);
b8   fude_zoom_instruments_finger_moved(fude_zoom_instruments* _ins, u64 _id, fude_zoom_v2 _at);
b8   fude_zoom_instruments_finger_up(fude_zoom_instruments* _ins, u64 _id);

// What the pen (or the mouse) does where it goes down.
typedef enum {
    FUDE_ZOOM_INSTRUMENT_PEN_NONE = 0,   // not at an instrument: it draws as it always does
    FUDE_ZOOM_INSTRUMENT_PEN_RULED,      // near an edge: it draws along it (_snapped: where it starts)
    // On a body or a knob it moves or turns the instrument until it lifts; on a ×, lifting there puts it away.
    FUDE_ZOOM_INSTRUMENT_PEN_HELD        // on a body, a knob, a tab or a ×: it handles the instrument
} FUDE_ZOOM_INSTRUMENT_PEN_;
// The pen going down at _at: returns what it does there, and for PEN_RULED sets *_snapped to where on the edge the
// line starts.
FUDE_ZOOM_INSTRUMENT_PEN_ fude_zoom_instruments_pen_down(fude_zoom_instruments* _ins, fude_zoom_v2 _at, fude_zoom_v2* _snapped);
// The pen moved to _at. When it draws along an edge, returns where on the edge it draws now; when it holds an
// instrument, the instrument follows and _at comes back as it is. fude_zoom_instruments_pen_up lifts it.
fude_zoom_v2 fude_zoom_instruments_pen_moved(fude_zoom_instruments* _ins, fude_zoom_v2 _at);
void fude_zoom_instruments_pen_up(fude_zoom_instruments* _ins);

// While drawing along an edge: gives how far it has gone (screen points, or degrees on an arc: *_degrees) and where to
// write that, beside the pen outside the edge. Returns false when it is not drawing along one.
b8   fude_zoom_instruments_ruled(const fude_zoom_instruments* _ins, f64* _along, b8* _degrees, fude_zoom_v2* _label_at);
// While drawing along an edge: returns the way out of the instrument there, a unit vector (the line is drawn beside
// the edge, as a pencil against a ruler). A compass's circle has none, and returns (0, 0): its pencil draws on it.
fude_zoom_v2 fude_zoom_instruments_ruled_outward(const fude_zoom_instruments* _ins);
// Returns whether the edge drawn along is straight (a line kept by its ends: not an arc, not a curve).
b8   fude_zoom_instruments_ruled_straight(const fude_zoom_instruments* _ins);
// Returns whether the edge drawn along is traced: the line is drawn on it, not beside it (a stencil's).
b8   fude_zoom_instruments_ruled_traced(const fude_zoom_instruments* _ins);
// Returns whether the edge drawn along is round: an arc of a circle (a compass's, the protractor's, a corner gauge's
// round, a circle template's hole), so a line along it is kept as an exact arc.
b8   fude_zoom_instruments_ruled_round(const fude_zoom_instruments* _ins);
// CHAINED INSTRUMENTS. Drawing along an edge, where it crosses another's (another instrument's, or this one's next
// edge: a set square's corner), the line turns onto the other edge once the pen has gone past the crossing nearer
// the other edge than this one, the corner falling exactly at the crossing. Going on past it near this edge, the line
// cannot go on into the instrument it meets (as with real ones): it stays at the crossing until the pen turns or
// goes back. Where nothing is in the way (a compass's circle, or leaving an instrument it began under) it goes on.
// Returns true, once, after a turn since it was last asked, with the crossing in *_corner and which way was out of the
// edge it left in *_out_before (for the corner beside both).
b8   fude_zoom_instruments_ruled_turn(fude_zoom_instruments* _ins, fude_zoom_v2* _corner, fude_zoom_v2* _out_before);

// How the instruments look: the colours, the font for the numbers, and how many millimetres a screen point is (for
// the graduations).
typedef struct {
    rde_font* font;
    f32       font_px;
    f64       mm_per_point;
    rde_color body, line, tick, text, accent;
    const fude_zoom_units_style* units;   // a compass's radius is written in these ("R 25 mm"; NULL: not written)
} fude_zoom_instruments_look;
// Draws every shown instrument, the last in `order` on top. _now is the engine's time, to show a turned one's angle
// for a moment. Called while the page is drawn.
void fude_zoom_instruments_render(const fude_zoom_instruments* _ins, const fude_zoom_instruments_look* _look, f64 _now);

#endif
