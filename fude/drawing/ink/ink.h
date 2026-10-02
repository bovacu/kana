#ifndef FUDE_INK
#define FUDE_INK

#include "rde.h"

// ===========================================================================
// The ink: capturing what the pen did, and drawing it.
//
// THIS IS THE ONE MODULE THAT SURVIVES THE SPIKE. Everything else in milestone 1
// is a harness for measuring it, but the stroke representation here is what the
// scoring in milestone 3 will consume, so it is built for that from the start:
// an ORDERED list of strokes, each an ORDERED list of timestamped points.
//
// That ordering is the whole reason this is hand-rolled rather than handed to
// PencilKit on iOS. PencilKit returns a PKDrawing — a picture — and scoring
// stroke order against a reference needs the strokes themselves, not an image of
// them. Owning the ink is a requirement here, not a preference, and it is what
// makes one implementation serve both platforms.
//
// NOTHING IS EVER DELETED BY AN EDIT. Erasing marks strokes dead instead of
// removing them, so every edit — a stroke, an erase swipe, Clear — is one entry in
// an undo history that just flips flags back and forth. The strokes that count
// (for drawing now, for scoring later) are the ALIVE ones, in written order.
//
// PRESSURE DOES NOT ARRIVE WITH POSITION. SDL delivers it as its own PEN_AXIS
// event, so the live value has to be kept here and sampled when a move arrives —
// reading pressure off a move event gets zero. Same for tilt. See fude_ink_pen_axis.
// ===========================================================================

// Below this, consecutive samples are treated as the same point. Stops a
// stationary pen from stacking hundreds of coincident samples, which would make
// the segment quads degenerate (zero-length direction vector -> NaN normal).
#define FUDE_INK_MIN_STEP_PX     1.0f

// Stroke width in pixels at zero and full pressure. A pen that reports no
// pressure at all draws at FUDE_INK_WIDTH_BASE, which is why that is a usable
// width on its own rather than hairline.
#define FUDE_INK_WIDTH_BASE      2.0f
#define FUDE_INK_WIDTH_PRESSURE  7.0f

// Raw pressure is noisy enough to make a stroke visibly lumpy. Smoothed with an
// exponential average; this is the weight of each new sample.
#define FUDE_INK_PRESSURE_SMOOTH 0.35f

// How a stroke's width is chosen. CONSTANT is the default: one width for every
// stroke, `constant_radius` (a setting a UI will expose later). PRESSURE follows
// the pen's pressure, or its speed on a pen that has none (see below).
typedef enum {
    FUDE_INK_WIDTH_MODE_CONSTANT = 0,
    FUDE_INK_WIDTH_MODE_PRESSURE
} FUDE_INK_WIDTH_MODE_;

// What the width is constant IN, on a zoomable canvas:
//   PAGE   — fixed on the page, like a pen on paper: zoom is a magnifier, so ink
//            written at any zoom looks the same once you zoom back.
//   SCREEN — fixed on the screen while writing: the stroke always looks the same
//            thickness as it goes down, so ink written zoomed out is thick on the
//            page (and huge when zoomed back in).
// Only affects NEW strokes: each point keeps the width it was written with.
typedef enum {
    FUDE_INK_BRUSH_SCALE_PAGE = 0,
    FUDE_INK_BRUSH_SCALE_SCREEN
} FUDE_INK_BRUSH_SCALE_;

// Default half-width for FUDE_INK_WIDTH_MODE_CONSTANT: page units in PAGE mode
// (screen units at 100% zoom), screen units in SCREEN mode.
#define FUDE_INK_RADIUS_DEFAULT      3.0f

// Ink is never DRAWN thinner than this half-width in screen units, so a stroke
// seen very zoomed out doesn't vanish. Display only — the stored width is kept.
#define FUDE_INK_MIN_SCREEN_RADIUS   0.5f

// PENS WITHOUT PRESSURE. Most "Apple Pencil compatible" styluses deliver position
// and tilt but no pressure: UIKit gives them a fixed nominal force, which reads as
// a flat ~0.05-0.07 here. A pen counts as having REAL pressure once its readings
// while touching spread wider than this; until then its width comes from speed —
// slow and deliberate is thick, fast is thin, the way a brush behaves.
#define FUDE_INK_PRESSURE_LIVE_RANGE 0.1f
// The simulated pressure a stroke starts at, and the range speed maps into: at
// rest it tends to _MAX, at _FAST_SPEED (world units per second) and above, _MIN.
#define FUDE_INK_SIM_START           0.45f
#define FUDE_INK_SIM_MIN             0.15f
#define FUDE_INK_SIM_MAX             0.70f
#define FUDE_INK_SIM_FAST_SPEED      1200.0f
// Weight of each new sample in the simulated pressure's exponential average, so
// the width eases instead of twitching with every sample's speed.
#define FUDE_INK_SIM_SMOOTH          0.2f

// @struct fude_ink_point
// @desc One sample. `time` is seconds since the stroke's FIRST sample — the
// rhythm of the stroke, which is what survives a save and what scoring can use.
// Pen samples are timed by their EVENT timestamps (several arrive per frame); the
// desktop mouse, polled once a frame, by the frame clock.
RDE_STRUCT {
    rde_vec_2F position;   // CANVAS space (see canvas.h), not the screen
    f32        pressure;   // 0..1, already smoothed — what the PEN reported, even a flat one
    f32        radius;     // drawn half-width, canvas units (see fude_ink.width_mode)
    f32        time;       // seconds since the stroke's first sample
} fude_ink_point;

// @struct fude_ink_stroke
// @desc One continuous mark: pen down to pen up. Its points are a range of
// fude_ink.points — see fude_ink_stroke_points.
//
// @field from_pen THE GO/NO-GO FLAG for this milestone. False means the stroke
//  came from a finger/touch fallback because no pen events arrived — which on an
//  iPad with an Apple Pencil is the answer to open question 1.
// @field alive False once erased. Kept, not removed, so the erase can be undone.
RDE_STRUCT {
    u32            first_point;
    u32            point_count;
    rde_vec_2F     bounds_min;  // the ink's extent (points ± radius), canvas units
    rde_vec_2F     bounds_max;
    rde_color      color;       // the brush colour when the stroke was written; FUDE_THEME_INK: the theme's ink
    b8             from_pen;
    b8             eraser;
    b8             alive;
} fude_ink_stroke;

// One undoable edit.
typedef enum {
    FUDE_INK_ACTION_WRITE = 0,   // strokes first .. first + count were written (count > 1: a paste)
    FUDE_INK_ACTION_ERASE,       // the strokes in targets[first .. first + count) were erased
    FUDE_INK_ACTION_MOVE         // the strokes in targets[first .. first + count) moved by delta
} FUDE_INK_ACTION_;

RDE_STRUCT {
    FUDE_INK_ACTION_ type;
    u32              first;
    u32              count;
    rde_vec_2F       delta;      // MOVE only, canvas units
} fude_ink_action;

RDE_STRUCT {
    // EVERY stroke written, in order, dead ones included (see the header). They
    // only ever grow at the END, as do points and targets — which is what lets a
    // discarded redo branch be dropped by truncation (fude_ink_discard_redo).
    rde_arr TYPE(fude_ink_stroke) strokes;
    rde_arr TYPE(fude_ink_point)  points;    // every stroke's points, back to back

    // UNDO HISTORY. actions[0 .. action_count) are applied; the rest, to the end
    // of the array, were undone and can be redone — until a new edit drops them.
    rde_arr TYPE(fude_ink_action) actions;
    u32                           action_count;
    rde_arr TYPE(u32)             targets;   // stroke indices the ERASE and MOVE actions act on, back to back
    // An erase gesture in progress. Everything one swipe takes is ONE action, so
    // one undo brings all of it back; fude_ink_erase_end closes it.
    b8               _erase_open;
    u32              _erase_first;
    // A move in progress (the lasso dragging a selection): one action at the end.
    b8               _move_open;
    u32              _move_first;
    rde_vec_2F       _move_delta;

    b8              drawing;          // a stroke is open

    // Bumped by every change to what is on the page (a point, a stroke, an erase,
    // undo, redo). Whoever saves compares it against the revision it last saved.
    u32             revision;
    f64             _stroke_t0;        // clock of the open stroke's first sample
    f64             _newest_handled;   // engine clock when the newest sample was handled

    // LIVE PEN STATE, folded from PEN_AXIS events. Sampled when a move arrives.
    f32             pressure;
    f32             pressure_raw;
    rde_vec_2F      tilt;
    u32             pen_id;
    b8              eraser;

    // Scratch for rde_rendering_2d_draw_stroke, one stroke at a time.
    rde_arr TYPE(rde_vec_2F) _scratch_positions;
    rde_arr TYPE(f32)        _scratch_radii;

    // Has a pen event EVER arrived? The headline readout of the spike.
    b8              pen_seen;

    // Event time (seconds, OS clock) of the sample being handled. Set by the caller
    // before begin/extend, like pressure is kept live: several pen samples arrive
    // per frame, so the engine clock would call them all simultaneous.
    f64             sample_time;

    // Width choice (settings; fude_ink_init sets the defaults).
    FUDE_INK_WIDTH_MODE_  width_mode;
    FUDE_INK_BRUSH_SCALE_ brush_scale;
    rde_color             color;             // brush colour for NEW strokes (FUDE_THEME_INK by default)
    f32                   constant_radius;   // for FUDE_INK_WIDTH_MODE_CONSTANT; units per brush_scale
    // The canvas zoom at the moment of capture, set by the caller like
    // sample_time. Turns screen units into the canvas units points are stored in:
    // always for the min step (a sampling density), and for the width in SCREEN
    // brush scale.
    f32                   zoom;

    // Has this pen shown real pressure? Latched once its readings while touching
    // spread past FUDE_INK_PRESSURE_LIVE_RANGE; never un-latched.
    b8              pressure_live;
    f32             _pressure_seen_min;
    f32             _pressure_seen_max;
    b8              _pressure_seen_any;
    // Speed-derived pressure for the open stroke (pens without real pressure).
    f32             _sim_pressure;
    f64             _sim_last_time;

    // --- measurement -------------------------------------------------------
    //
    // What can honestly be measured in software. Absolute pen-to-photon latency
    // cannot be: it needs a high-speed camera pointed at the glass. These cover
    // the half that is ours.

    // Samples accepted since the last frame, and the rate while drawing. The
    // rate is the one that matters: a pen reporting at 120-240 Hz that arrives
    // as ~60 Hz means samples are being coalesced or dropped, and fast strokes
    // will come out as polygons.
    //
    // The rate is the current (or last) stroke's, over its own span of EVENT
    // timestamps — not a wall-clock window, which counted the pen-up gaps between
    // strokes and read low. It counts ACCEPTED samples, so a slow stroke (samples
    // under the min step) reads low too: draw fast to read the input rate.
    u32             samples_this_frame;
    f32             sample_hz;
    f64             _hz_first_time;
    u32             _hz_samples;

    // Age of the newest sample at the moment it is drawn, in ms. This is the
    // app-side half of the latency — event handled to frame presented — and
    // excludes everything before the event reached us.
    f32             stale_ms;
    f32             stale_ms_max;
} fude_ink;

// @func fude_ink_init / destroy
void fude_ink_init(fude_ink* _ink);
void fude_ink_destroy(fude_ink* _ink);

// @func fude_ink_add_loaded_stroke
// @desc Appends a stroke read from a save: alive, and NOT in the undo history
// (a loaded page is where history starts). Only before any editing.
void fude_ink_add_loaded_stroke(fude_ink* _ink, const fude_ink_point* _points, u32 _count, rde_color _color, b8 _from_pen);

// @func fude_ink_stroke_count / stroke_at
// @desc Every stroke written, in order, erased ones included (check .alive).
u32                    fude_ink_stroke_count(const fude_ink* _ink);
const fude_ink_stroke* fude_ink_stroke_at(const fude_ink* _ink, u32 _index);

// @func fude_ink_stroke_points
// @desc A stroke's points (stroke->point_count of them). Only valid until the
// next point is added: the array grows.
const fude_ink_point* fude_ink_stroke_points(const fude_ink* _ink, const fude_ink_stroke* _stroke);

// @func fude_ink_clear
// @desc Erases every stroke, as ONE undoable edit. Keeps the live pen state —
// the pen has not moved.
void fude_ink_clear(fude_ink* _ink);

// @func fude_ink_pen_axis
// @desc Folds a PEN_AXIS event into the live pen state. Must be called for axis
// events or pressure stays at zero and every stroke draws hairline.
void fude_ink_pen_axis(fude_ink* _ink, const rde_event_pen* _pen);

// @func fude_ink_begin / extend / end
// @desc One stroke's lifetime. _position is in 2D world space; the caller owns
// the window-pixel conversion, because only it knows the camera.
void fude_ink_begin(fude_ink* _ink, rde_vec_2F _position, b8 _from_pen, b8 _eraser);
void fude_ink_extend(fude_ink* _ink, rde_vec_2F _position);
void fude_ink_end(fude_ink* _ink);

// @func fude_ink_erase_at / erase_end
// @desc Stroke eraser: erases every stroke whose edge passes within _radius of
// _point (both CANVAS units), anywhere along it — not just at its samples.
// Whole strokes go, as in most note apps: scoring works on strokes, so a stroke
// is the unit of writing. Returns how many were erased. Not while one is open.
// Every erase_at until erase_end is one gesture, undone as one: call erase_end
// when the pen (or mouse) lifts.
u32 fude_ink_erase_at(fude_ink* _ink, rde_vec_2F _point, f32 _radius);
void fude_ink_erase_end(fude_ink* _ink);

// @func fude_ink_erase_strokes
// @desc Erases the listed strokes as ONE undoable edit (the lasso's Delete). Dead
// or out-of-range indices are skipped.
void fude_ink_erase_strokes(fude_ink* _ink, const u32* _ids, u32 _count);

// @func fude_ink_pen_radius
// @desc The half-width (canvas units) a stroke written now at _pressure (0..1)
// gets: the brush's constant width or its pressure width, held to the page or
// the screen as the brush scale says. For strokes the app writes (pasted text).
f32 fude_ink_pen_radius(const fude_ink* _ink, f32 _pressure);

// @func fude_ink_add_strokes
// @desc Appends strokes as ONE undoable edit (a paste, a duplicate). _strokes
// supplies each stroke's point_count, colour and flags, and its first_point
// indexes _points; every position gets _offset added. Returns the first new
// stroke's index (they are consecutive), or UINT32_MAX when nothing was added.
u32 fude_ink_add_strokes(fude_ink* _ink, const fude_ink_stroke* _strokes, u32 _count, const fude_ink_point* _points, rde_vec_2F _offset);

// @func fude_ink_move_begin / move_by / move_end
// @desc Moves the listed strokes, live, as ONE undoable edit: begin with the
// strokes, move_by as the pen drags (canvas units), move_end when it lifts. A
// move that ends where it began leaves no history.
void fude_ink_move_begin(fude_ink* _ink, const u32* _ids, u32 _count);
void fude_ink_move_by(fude_ink* _ink, rde_vec_2F _delta);
void fude_ink_move_end(fude_ink* _ink);

// @func fude_ink_pick
// @desc The topmost alive stroke passing within _radius of _point (canvas
// units), or UINT32_MAX.
u32 fude_ink_pick(const fude_ink* _ink, rde_vec_2F _point, f32 _radius);

// @func fude_ink_undo / redo
// @desc Steps the history back / forward one edit. False when there is nothing
// to step to, or a stroke is open. An erase gesture in progress is closed first.
b8 fude_ink_undo(fude_ink* _ink);
b8 fude_ink_redo(fude_ink* _ink);
b8 fude_ink_can_undo(const fude_ink* _ink);
b8 fude_ink_can_redo(const fude_ink* _ink);
u32 fude_ink_undo_steps(const fude_ink* _ink);
u32 fude_ink_redo_steps(const fude_ink* _ink);

// @func fude_ink_frame_begin
// @desc Resets the per-frame counters. Call once per frame before handling events.
void fude_ink_frame_begin(fude_ink* _ink);

// @func fude_ink_render
// @desc Draws every stroke through the canvas view (screen = canvas * _zoom +
// _offset). Only strokes whose bounds reach the screen are drawn; _screen_half
// is half the window, in screen units. Call inside a 2D drawing block. Also
// updates stale_ms while a stroke is open, since "now" at draw time is exactly
// what that measures.
void fude_ink_render(fude_ink* _ink, rde_vec_2F _offset, f32 _zoom, rde_vec_2F _screen_half, f64 _now, b8 _show_samples);

// @func fude_ink_draw_stroke
// @desc Draws one stroke through the view, _extra_px wider on each side, in
// _color — for highlights (the lasso's glow). Inside a 2D drawing block.
void fude_ink_draw_stroke(fude_ink* _ink, u32 _index, rde_vec_2F _offset, f32 _zoom, f32 _extra_px, rde_color _color);

// @func fude_ink_alive_strokes / total_points
// @desc The strokes that count (not erased), and their points.
u32 fude_ink_alive_strokes(const fude_ink* _ink);
u32 fude_ink_total_points(const fude_ink* _ink);

#endif
