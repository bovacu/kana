#ifndef KANA_INK
#define KANA_INK

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
// PRESSURE DOES NOT ARRIVE WITH POSITION. SDL delivers it as its own PEN_AXIS
// event, so the live value has to be kept here and sampled when a move arrives —
// reading pressure off a move event gets zero. Same for tilt. See kana_ink_pen_axis.
// ===========================================================================

// A kanji is at most ~30 strokes; 64 leaves room for free scribbling in the
// spike without making the struct enormous.
#define KANA_INK_MAX_STROKES     64
#define KANA_INK_MAX_POINTS      2048

// Below this, consecutive samples are treated as the same point. Stops a
// stationary pen from stacking hundreds of coincident samples, which would make
// the segment quads degenerate (zero-length direction vector -> NaN normal).
#define KANA_INK_MIN_STEP_PX     1.0f

// Stroke width in pixels at zero and full pressure. A pen that reports no
// pressure at all draws at KANA_INK_WIDTH_BASE, which is why that is a usable
// width on its own rather than hairline.
#define KANA_INK_WIDTH_BASE      2.0f
#define KANA_INK_WIDTH_PRESSURE  7.0f

// Raw pressure is noisy enough to make a stroke visibly lumpy. Smoothed with an
// exponential average; this is the weight of each new sample.
#define KANA_INK_PRESSURE_SMOOTH 0.35f

// @struct kana_ink_point
// @desc One sample. `time` is the engine clock at the moment the event was
// HANDLED, not the OS timestamp — see kana_ink.stale_ms for what that is for.
RDE_STRUCT {
    rde_vec_2F position;   // 2D world space (centre-origin, Y up), not window pixels
    f32        pressure;   // 0..1, already smoothed
    f64        time;       // seconds, engine monotonic clock
} kana_ink_point;

// @struct kana_ink_stroke
// @desc One continuous mark: pen down to pen up.
//
// @field from_pen THE GO/NO-GO FLAG for this milestone. False means the stroke
//  came from a finger/touch fallback because no pen events arrived — which on an
//  iPad with an Apple Pencil is the answer to open question 1.
RDE_STRUCT {
    kana_ink_point points[KANA_INK_MAX_POINTS];
    u32            point_count;
    b8             from_pen;
    b8             eraser;
} kana_ink_stroke;

RDE_STRUCT {
    kana_ink_stroke strokes[KANA_INK_MAX_STROKES];
    u32             stroke_count;

    b8              drawing;          // a stroke is open
    b8              dropped_points;   // a stroke ran past KANA_INK_MAX_POINTS
    b8              dropped_strokes;  // the canvas ran past KANA_INK_MAX_STROKES

    // LIVE PEN STATE, folded from PEN_AXIS events. Sampled when a move arrives.
    f32             pressure;
    f32             pressure_raw;
    rde_vec_2F      tilt;
    u32             pen_id;
    b8              eraser;

    // Has a pen event EVER arrived? The headline readout of the spike.
    b8              pen_seen;

    // --- measurement -------------------------------------------------------
    //
    // What can honestly be measured in software. Absolute pen-to-photon latency
    // cannot be: it needs a high-speed camera pointed at the glass. These cover
    // the half that is ours.

    // Samples accepted since the last frame, and the rate while drawing. The
    // rate is the one that matters: a pen reporting at 120-240 Hz that arrives
    // as ~60 Hz means samples are being coalesced or dropped, and fast strokes
    // will come out as polygons.
    u32             samples_this_frame;
    f32             sample_hz;
    f64             _hz_window_start;
    u32             _hz_window_samples;

    // Age of the newest sample at the moment it is drawn, in ms. This is the
    // app-side half of the latency — event handled to frame presented — and
    // excludes everything before the event reached us.
    f32             stale_ms;
    f32             stale_ms_max;
} kana_ink;

// @func kana_ink_init
void kana_ink_init(kana_ink* _ink);

// @func kana_ink_clear
// @desc Drops every stroke. Keeps the live pen state — the pen has not moved.
void kana_ink_clear(kana_ink* _ink);

// @func kana_ink_pen_axis
// @desc Folds a PEN_AXIS event into the live pen state. Must be called for axis
// events or pressure stays at zero and every stroke draws hairline.
void kana_ink_pen_axis(kana_ink* _ink, const rde_event_pen* _pen);

// @func kana_ink_begin / extend / end
// @desc One stroke's lifetime. _position is in 2D world space; the caller owns
// the window-pixel conversion, because only it knows the camera.
void kana_ink_begin(kana_ink* _ink, rde_vec_2F _position, b8 _from_pen, b8 _eraser);
void kana_ink_extend(kana_ink* _ink, rde_vec_2F _position);
void kana_ink_end(kana_ink* _ink);

// @func kana_ink_frame_begin
// @desc Resets the per-frame counters and advances the sample-rate window. Call
// once per frame before handling events.
void kana_ink_frame_begin(kana_ink* _ink, f64 _now);

// @func kana_ink_render
// @desc Draws every stroke. Call inside a 2D drawing block. Also updates
// stale_ms, since "now" at draw time is exactly what that measures.
void kana_ink_render(kana_ink* _ink, f64 _now, b8 _show_samples);

// @func kana_ink_total_points
u32 kana_ink_total_points(const kana_ink* _ink);

#endif
