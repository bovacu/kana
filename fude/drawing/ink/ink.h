// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_INK
#define FUDE_INK

#include "rde.h"

// ===========================================================================
// Ink: capturing what the pen did, and drawing it.
//
// Where it sits: drawing/ink. The app shell's page, the study screens (Practice, the exams, Browse) and the canvas
// apps each own a fude_ink. The lasso (lasso.h) selects in it, pagefile.h saves it, a document (doc.c) turns it with
// its pages, and scoring reads its strokes.
// Its main types and their lifecycle: fude_ink is the caller's struct. fude_ink_init gives it its arrays and
// fude_ink_destroy frees them. The arrays are on the standard heap, because a page has no natural size limit, and
// running the engine's fixed pool out is a fatal assert, not an error the app could handle.
// Threads: main thread only.
// Files: none of its own; a page's file (pagefile.h) holds its alive strokes.
// Design:
//   ORDERED STROKES. This module was the one kept from milestone 1's spike (everything else there was a harness for
//   measuring it), and its strokes were built from the start for scoring (milestone 3) to read: an ORDERED list of
//   strokes, each an ORDERED list of timestamped points. That order is the whole reason the ink is written here
//   rather than handed to PencilKit on iOS. PencilKit returns a PKDrawing, which is a picture, and scoring stroke
//   order against a reference needs the strokes themselves, not an image of them. Owning the ink is a requirement,
//   not a preference, and it is what lets one implementation serve both platforms.
//   NOTHING IS EVER DELETED BY AN EDIT. Erasing marks strokes dead instead of removing them, so every edit (a
//   stroke, an erase swipe, Clear) is one entry in an undo history that only flips flags back and forth. The strokes
//   that count, for drawing and for scoring, are the ALIVE ones, in the order they were written.
//   PRESSURE DOES NOT ARRIVE WITH POSITION. SDL delivers it as an event of its own (PEN_AXIS), so the live value is
//   kept here and sampled when a move arrives; reading pressure off a move event gets zero. The same goes for tilt.
//   See fude_ink_pen_axis.
// ===========================================================================

// Samples closer than this (screen units) to the previous one are treated as the same point and dropped. That stops
// a stationary pen from stacking hundreds of coincident samples, which would make the segment quads degenerate (a
// zero-length direction gives a NaN normal).
#define FUDE_INK_MIN_STEP_PX     1.0f

// PRESSURE mode's half-width at zero pressure, and what full pressure adds to it (so 2 to 9, in the brush's units).
// A pen that reports no pressure at all draws at FUDE_INK_WIDTH_BASE, which is why that is a usable width on its
// own rather than a hairline.
#define FUDE_INK_WIDTH_BASE      2.0f
#define FUDE_INK_WIDTH_PRESSURE  7.0f

// Raw pressure is noisy enough to make a stroke visibly lumpy, so it is smoothed with an exponential average; this is
// the weight of each new sample.
#define FUDE_INK_PRESSURE_SMOOTH 0.35f

// How a stroke's width is chosen. CONSTANT is the default: one width for every stroke, constant_radius (which the
// toolbar's size slider sets). PRESSURE follows the pen's pressure, or its speed on a pen that has none (see below).
typedef enum {
    FUDE_INK_WIDTH_MODE_CONSTANT = 0,
    FUDE_INK_WIDTH_MODE_PRESSURE
} FUDE_INK_WIDTH_MODE_;

// What the width stays constant IN, on a zoomable canvas:
//   PAGE     fixed on the page, like a pen on paper. The zoom is a magnifier, so ink written at any zoom looks the
//            same once you zoom back.
//   SCREEN   fixed on the screen while writing. The stroke always looks the same thickness as it goes down, so ink
//            written zoomed out is thick on the page (and huge when zoomed back in).
// It only affects NEW strokes: each point keeps the width it was written with.
typedef enum {
    FUDE_INK_BRUSH_SCALE_PAGE = 0,
    FUDE_INK_BRUSH_SCALE_SCREEN
} FUDE_INK_BRUSH_SCALE_;

// The default half-width for FUDE_INK_WIDTH_MODE_CONSTANT: page units in PAGE mode (screen units at 100% zoom),
// screen units in SCREEN mode.
#define FUDE_INK_RADIUS_DEFAULT      3.0f

// Ink is never DRAWN thinner than this half-width in screen units, so a stroke seen very zoomed out does not vanish.
// This only affects drawing: the stored width is kept.
#define FUDE_INK_MIN_SCREEN_RADIUS   0.5f

// PENS WITHOUT PRESSURE. Most "Apple Pencil compatible" styluses deliver position and tilt but no pressure: UIKit
// gives them a fixed nominal force, which reads here as a flat 0.05 to 0.07. A pen counts as having REAL pressure
// once its readings while touching spread wider than this; until then its width comes from its speed, the way a
// brush behaves: slow and deliberate is thick, fast is thin.
#define FUDE_INK_PRESSURE_LIVE_RANGE 0.1f
// The simulated pressure a stroke starts at, and the range speed maps into: at rest it tends to _MAX, and at
// _FAST_SPEED (screen units per second) and above, to _MIN.
#define FUDE_INK_SIM_START           0.45f
#define FUDE_INK_SIM_MIN             0.15f
#define FUDE_INK_SIM_MAX             0.70f
#define FUDE_INK_SIM_FAST_SPEED      1200.0f
// The weight of each new sample in the simulated pressure's exponential average, so the width eases instead of
// twitching with every sample's speed.
#define FUDE_INK_SIM_SMOOTH          0.2f

// The marker's half-width on the page (canvas units: a line of text's height at 100%), its range, and how
// see-through its colours are (their alpha).
#define FUDE_INK_MARKER_RADIUS 10.0f
#define FUDE_INK_MARKER_MIN    3.0f
#define FUDE_INK_MARKER_MAX    32.0f
#define FUDE_INK_MARKER_ALPHA  110u
#define FUDE_INK_MARKER_STEP   0.75f   // its points are drawn at least this many half-widths apart (fude_ink_render)

// The eraser's reach: a radius in SCREEN units (a fingertip's circle at any zoom), and the range of its own size on
// the toolbar's slider, kept apart from the brush's.
#define FUDE_INK_ERASER_RADIUS 10.0f
#define FUDE_INK_ERASER_MIN    4.0f
#define FUDE_INK_ERASER_MAX    60.0f

// One sample. Its time is seconds since the stroke's FIRST sample: the rhythm of the stroke, which survives a save
// and which scoring can use. Pen samples are timed by their EVENT timestamps (several arrive per frame); the desktop
// mouse, polled once a frame, by the frame clock.
RDE_STRUCT {
    rde_vec_2F position;   // CANVAS space (see canvas.h), not the screen
    f32        pressure;   // 0..1, already smoothed: what the PEN reported, even when that is a flat reading
    f32        radius;     // the drawn half-width, canvas units (see fude_ink.width_mode)
    f32        time;       // seconds since the stroke's first sample
} fude_ink_point;

// One continuous mark, from pen down to pen up. Its points are a range of fude_ink.points (fude_ink_stroke_points).
RDE_STRUCT {
    // Its points: fude_ink.points[first_point .. first_point + point_count).
    u32            first_point;
    u32            point_count;
    rde_vec_2F     bounds_min;  // the ink's extent (each point ± its radius), canvas units
    rde_vec_2F     bounds_max;
    rde_color      color;       // the brush colour when the stroke was written; FUDE_THEME_INK means the theme's ink
    // Written with a pen. False means a finger or the mouse wrote it, because no pen events arrived. In milestone 1
    // this was the go/no-go flag: on an iPad with an Apple Pencil, it answered open question 1.
    b8             from_pen;
    // False once erased. The stroke is kept, not removed, so the erase can be undone.
    b8             alive;
    b8             marker;      // the marker's: see-through, one width, under the other ink (never read as writing)
} fude_ink_stroke;

// What an undoable edit did.
typedef enum {
    FUDE_INK_ACTION_WRITE = 0,   // strokes first .. first + count were written (more than one: a paste)
    FUDE_INK_ACTION_ERASE,       // the strokes listed in targets[first .. first + count) were erased
    FUDE_INK_ACTION_MOVE         // the strokes listed in targets[first .. first + count) were moved by delta
} FUDE_INK_ACTION_;

// One undoable edit: an entry of the history.
RDE_STRUCT {
    FUDE_INK_ACTION_ type;
    u32              first;
    u32              count;
    rde_vec_2F       delta;      // MOVE only, canvas units
} fude_ink_action;

// A page's ink: its strokes, their undo history, the live pen state and the measurements.
RDE_STRUCT {
    // EVERY stroke written, in order, dead ones included (see the top). Strokes only ever grow at the END, as do
    // points and targets, which is what lets a discarded redo branch be dropped by cutting the arrays short
    // (fude_ink_discard_redo).
    rde_arr TYPE(fude_ink_stroke) strokes;
    rde_arr TYPE(fude_ink_point)  points;    // every stroke's points, back to back

    // UNDO HISTORY. actions[0 .. action_count) are applied; the rest, to the end of the array, were undone and can be
    // redone, until a new edit drops them.
    rde_arr TYPE(fude_ink_action) actions;
    u32                           action_count;
    rde_arr TYPE(u32)             targets;   // the stroke indices the ERASE and MOVE actions act on, back to back
    // An erase gesture in progress. Everything one swipe takes is ONE action, so one undo brings all of it back;
    // fude_ink_erase_end closes it.
    b8               _erase_open;
    u32              _erase_first;
    // A move in progress (the lasso dragging a selection), which becomes one action when it ends.
    b8               _move_open;
    u32              _move_first;
    rde_vec_2F       _move_delta;

    b8              drawing;          // a stroke is open

    // Goes up with every change to what is on the page (a point, a stroke, an erase, an undo, a redo). Whoever saves
    // compares it with the revision it last saved.
    u32             revision;
    f64             _stroke_t0;        // the clock at the open stroke's first sample
    f64             _newest_handled;   // the engine's clock when the newest sample was handled

    // LIVE PEN STATE, folded in from PEN_AXIS events and sampled when a move arrives: the smoothed pressure and the
    // last raw reading, the tilt, which pen it is, and whether its eraser end is in use.
    f32             pressure;
    f32             pressure_raw;
    rde_vec_2F      tilt;
    u32             pen_id;
    b8              eraser;

    // Scratch space for rde_rendering_2d_draw_stroke, one stroke at a time.
    rde_arr TYPE(rde_vec_2F) _scratch_positions;
    rde_arr TYPE(f32)        _scratch_radii;

    // Whether a pen event has EVER arrived: the headline readout of milestone 1's spike.
    b8              pen_seen;

    // The event time (seconds, the OS clock) of the sample being handled. The caller sets it before begin and
    // extend, just as pressure is kept live: several pen samples arrive per frame, so the engine's clock would make
    // them all simultaneous.
    f64             sample_time;

    // How widths are chosen (settings; fude_ink_init sets the defaults).
    FUDE_INK_WIDTH_MODE_  width_mode;
    FUDE_INK_BRUSH_SCALE_ brush_scale;
    rde_color             color;             // the brush colour for NEW strokes (FUDE_THEME_INK by default)
    f32                   constant_radius;   // the half-width in CONSTANT mode, in brush_scale's units
    f32                   page_scale;        // PAGE widths times this are canvas units (1 when they are the same)
    // The marker (the toolbar's): while marking, new strokes are the marker's. Its colour is see-through (its alpha is
    // kept) and its half-width is in canvas units (it marks the page, so it zooms with it); pressure and speed do not
    // change it.
    b8                    marking;
    rde_color             marker_color;
    f32                   marker_radius;
    // The eraser's reach, in screen units (the page uses it; the toolbar sets it).
    f32                   eraser_radius;
    // The canvas's zoom at the moment of capture, set by the caller like sample_time. It turns screen units into the
    // canvas units points are stored in: always for the minimum step (a sampling density), and for the width in
    // SCREEN brush scale.
    f32                   zoom;

    // Whether this pen has shown real pressure. Set once its readings while touching spread past
    // FUDE_INK_PRESSURE_LIVE_RANGE, and never cleared.
    b8              pressure_live;
    f32             _pressure_seen_min;
    f32             _pressure_seen_max;
    b8              _pressure_seen_any;
    // The speed-derived pressure of the open stroke (for pens without real pressure).
    f32             _sim_pressure;
    f64             _sim_last_time;

    // --- measurement -------------------------------------------------------
    //
    // What can honestly be measured in software. The latency from the pen to the light on screen cannot be: it
    // needs a high-speed camera pointed at the glass. These cover the half that is ours.

    // The samples accepted since the last frame, and the rate while drawing. The rate is the one that matters: a pen
    // reporting at 120-240 Hz that arrives at about 60 Hz means samples are being merged or dropped, and fast strokes
    // will come out as polygons.
    //
    // The rate is the current (or last) stroke's, over its own span of EVENT timestamps, not a wall-clock window,
    // which counted the gaps between strokes and read low. It counts ACCEPTED samples, so a slow stroke (samples
    // under the minimum step) reads low too: draw fast to read the input rate.
    u32             samples_this_frame;
    f32             sample_hz;
    f64             _hz_first_time;
    u32             _hz_samples;

    // The age of the newest sample at the moment it is drawn, in ms. This is the app's half of the latency (from the
    // event being handled to the frame being presented), and leaves out everything before the event reached us.
    f32             stale_ms;
    f32             stale_ms_max;
} fude_ink;

// fude_ink_init sets up an empty ink with the default settings; fude_ink_destroy frees its arrays and zeroes it.
void fude_ink_init(fude_ink* _ink);
void fude_ink_destroy(fude_ink* _ink);
// Empties the page (its strokes, points and history) and keeps what is the writer's rather than the page's: the
// brush, the marker and the eraser (settings), the page's scale and the zoom, and what is known of the pen. Another
// canvas opening uses it, so its page starts empty with every setting as it was.
void fude_ink_clear_page(fude_ink* _ink);

// Appends a stroke read from a save: alive, and NOT in the undo history (a loaded page is where the history starts).
// Ignored once there is any history or a stroke is open, so call it only before any editing.
void fude_ink_add_loaded_stroke(fude_ink* _ink, const fude_ink_point* _points, u32 _count, rde_color _color, b8 _from_pen);
// Moves every stroke's points through _map (a document's page turned: its ink turns with it), then works out their
// bounds again. The erased strokes move too, because undo can bring them back. It is not an edit that can be undone:
// the history stays, and what it refers to has moved with the page.
void fude_ink_remap(fude_ink* _ink, void (*_map)(any _user, u32 _stroke, fude_ink_point* _point), any _user);
// The same as fude_ink_add_loaded_stroke, saying whether the stroke is the marker's (_marker).
void fude_ink_add_loaded_stroke_2(fude_ink* _ink, const fude_ink_point* _points, u32 _count, rde_color _color, b8 _from_pen, b8 _marker);

// fude_ink_stroke_count returns how many strokes were written, erased ones included (check .alive), and
// fude_ink_stroke_at returns one by its place in the written order (_index must be below the count). The pointer
// stays valid until the next stroke is added.
u32                    fude_ink_stroke_count(const fude_ink* _ink);
const fude_ink_stroke* fude_ink_stroke_at(const fude_ink* _ink, u32 _index);

// Returns a stroke's points (stroke->point_count of them). The pointer is only valid until the next point is added,
// because the array grows.
const fude_ink_point* fude_ink_stroke_points(const fude_ink* _ink, const fude_ink_stroke* _stroke);

// Erases every stroke as ONE undoable edit. The live pen state is kept, since the pen has not moved: zeroing the
// pressure would make the next stroke start as a hairline until the next axis event.
void fude_ink_clear(fude_ink* _ink);

// Folds a PEN_AXIS event into the live pen state. It must be called for axis events, or the pressure stays at zero
// and every stroke draws at the base width.
void fude_ink_pen_axis(fude_ink* _ink, const rde_event_pen* _pen);

// One stroke's lifetime: begin at pen down, extend at each move, end at pen up. _position is in CANVAS units: the
// caller converts from the screen, because only it knows the view, and sets zoom and sample_time first. A second
// begin without an end closes the open stroke first, so two marks stay two strokes (the stroke count is the first
// thing scoring checks). _from_pen says whether a pen wrote it (rather than a finger or the mouse). There is no eraser
// stroke: erasing removes whole strokes (fude_ink_erase_at).
void fude_ink_begin(fude_ink* _ink, rde_vec_2F _position, b8 _from_pen);
void fude_ink_extend(fude_ink* _ink, rde_vec_2F _position);
void fude_ink_end(fude_ink* _ink);

// The stroke eraser: erases every stroke whose edge passes within _radius of _point (both CANVAS units), anywhere
// along it, not just at its samples. Whole strokes go, as in most note apps, because scoring works on strokes, so a
// stroke is the unit of writing. Returns how many were erased; nothing is erased while a stroke is open. Every
// erase_at until erase_end is one gesture, undone as one: call erase_end when the pen (or mouse) lifts.
u32 fude_ink_erase_at(fude_ink* _ink, rde_vec_2F _point, f32 _radius);
void fude_ink_erase_end(fude_ink* _ink);

// Erases the listed strokes as ONE undoable edit (the lasso's Delete). Dead or out-of-range indices are skipped.
void fude_ink_erase_strokes(fude_ink* _ink, const u32* _ids, u32 _count);

// Returns the half-width (canvas units) a stroke written now at _pressure (0..1) gets: the brush's constant width or
// its pressure width, held to the page or to the screen as the brush scale says. For strokes the app writes itself
// (pasted text).
f32 fude_ink_pen_radius(const fude_ink* _ink, f32 _pressure);

// Appends strokes as ONE undoable edit (a paste, a duplicate). _strokes gives each stroke's point_count, colour and
// flags, and its first_point indexes _points; every position gets _offset added. Returns the first new stroke's index
// (the new strokes are consecutive), or UINT32_MAX when nothing was added (or a stroke is open).
u32 fude_ink_add_strokes(fude_ink* _ink, const fude_ink_stroke* _strokes, u32 _count, const fude_ink_point* _points, rde_vec_2F _offset);

// Moves the listed strokes live, as ONE undoable edit: move_begin with the strokes, move_by as the pen drags (canvas
// units), move_end when it lifts. A move that ends where it began leaves no history, and the edits undone before it
// can still be redone.
void fude_ink_move_begin(fude_ink* _ink, const u32* _ids, u32 _count);
void fude_ink_move_by(fude_ink* _ink, rde_vec_2F _delta);
void fude_ink_move_end(fude_ink* _ink);

// Returns the alive stroke whose ink is nearest _point, within _radius (canvas units); on a tie, the one drawn on
// top. UINT32_MAX when there is none.
u32 fude_ink_pick(const fude_ink* _ink, rde_vec_2F _point, f32 _radius);

// fude_ink_undo and fude_ink_redo step the history back or forward one edit, closing an erase or move gesture in
// progress first. They return false when there is nothing to step to, or a stroke is open. can_undo and can_redo say
// whether there is a step to take (from the history alone, so a button does not flicker while a stroke is written),
// and undo_steps and redo_steps how many.
b8 fude_ink_undo(fude_ink* _ink);
b8 fude_ink_redo(fude_ink* _ink);
b8 fude_ink_can_undo(const fude_ink* _ink);
b8 fude_ink_can_redo(const fude_ink* _ink);
u32 fude_ink_undo_steps(const fude_ink* _ink);
u32 fude_ink_redo_steps(const fude_ink* _ink);

// Resets the per-frame counters. Call it once per frame, before handling events.
void fude_ink_frame_begin(fude_ink* _ink);

// Draws every stroke through the canvas view (screen = canvas * _zoom + _offset): the marker's strokes first, under
// the rest. Only strokes whose bounds reach the screen are drawn; _screen_half is half the window, in screen units.
// _show_samples also draws every raw sample as a dot (a diagnostic). Call inside a 2D drawing block. While a stroke
// is open it also updates stale_ms, since "now" at draw time is exactly what that measures.
void fude_ink_render(fude_ink* _ink, rde_vec_2F _offset, f32 _zoom, rde_vec_2F _screen_half, f64 _now, b8 _show_samples);

// Draws one alive stroke through the view, _extra_px wider on each side, in _color: for highlights (the lasso's
// glow). Call inside a 2D drawing block.
void fude_ink_draw_stroke(fude_ink* _ink, u32 _index, rde_vec_2F _offset, f32 _zoom, f32 _extra_px, rde_color _color);

// fude_ink_alive_strokes returns how many strokes count (are not erased), and fude_ink_total_points how many points
// those strokes have.
u32 fude_ink_alive_strokes(const fude_ink* _ink);
u32 fude_ink_total_points(const fude_ink* _ink);

#endif
