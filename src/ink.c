#include "ink.h"

#include <math.h>
#include <string.h>

// ===========================================================================
// See ink.h. Two halves: capture (cheap, exact, keeps everything) and render
// (throwaway quality for now — the spike is about feel and timing, not beauty).
// ===========================================================================

#define KANA_INK_COLOR        (rde_color){  30,  30,  36, 255 }
#define KANA_INK_ERASER_COLOR (rde_color){ 200,  90,  90, 255 }
#define KANA_INK_SAMPLE_COLOR (rde_color){  40, 160, 220, 255 }

// Round joins and caps come free from stamping a disc at every sample; this is
// how many triangles each gets. Eight is invisible at these widths and keeps the
// draw count sane.
#define KANA_INK_CIRCLE_SEGS  8u

void kana_ink_init(kana_ink* _ink) {
    memset(_ink, 0, sizeof(*_ink));
}

void kana_ink_clear(kana_ink* _ink) {
    // The STROKES go; the pen state stays. Clearing the canvas is not the pen
    // lifting, and zeroing pressure here would make the next stroke start
    // hairline until the first axis event caught up.
    _ink->stroke_count    = 0;
    _ink->drawing         = false;
    _ink->dropped_points  = false;
    _ink->dropped_strokes = false;
    _ink->stale_ms_max    = 0.0f;
}

void kana_ink_pen_axis(kana_ink* _ink, const rde_event_pen* _pen) {
    _ink->pen_seen = true;
    _ink->pen_id   = _pen->pen_id;

    // SDL reports ONE axis per event, and leaves the others at whatever the
    // struct was initialised to — so each has to be taken only when it is the
    // one that changed. Reading all three off every axis event zeroes two of
    // them at random.
    //
    // The axis numbers are SDL_PenAxis: 0 = pressure, 1 = xtilt, 2 = ytilt. The
    // engine already breaks those three out into named fields, so the check is
    // on which field this event is actually carrying.
    if(_pen->axis == 0) {
        _ink->pressure_raw = _pen->pressure;

        // Smoothed, because raw pressure jitters by a few percent sample to
        // sample and an unsmoothed width makes the stroke visibly lumpy.
        _ink->pressure += (_pen->pressure - _ink->pressure) * KANA_INK_PRESSURE_SMOOTH;
    } else if(_pen->axis == 1) {
        _ink->tilt.x = _pen->tilt.x;
    } else if(_pen->axis == 2) {
        _ink->tilt.y = _pen->tilt.y;
    }
}

RDE_INTERNAL void kana_ink_push_point(kana_ink* _ink, rde_vec_2F _position) {
    if(_ink->stroke_count == 0) {
        return;
    }

    kana_ink_stroke* _stroke = &_ink->strokes[_ink->stroke_count - 1];

    if(_stroke->point_count >= KANA_INK_MAX_POINTS) {
        // Recorded rather than silently ignored: a stroke that hit the cap is a
        // stroke the scoring would later see truncated, and that must not look
        // like the user lifting the pen early.
        _ink->dropped_points = true;
        return;
    }

    kana_ink_point* _point = &_stroke->points[_stroke->point_count++];
    _point->position = _position;
    _point->pressure = _ink->pressure;
    _point->time     = rde_engine_get_time_now();

    _ink->samples_this_frame++;
    _ink->_hz_window_samples++;
}

void kana_ink_begin(kana_ink* _ink, rde_vec_2F _position, b8 _from_pen, b8 _eraser) {
    if(_ink->drawing) {
        // A second down without an up. Close the old one rather than merging:
        // two marks the user made separately must stay two strokes, or the
        // stroke COUNT — the first thing scoring checks — is wrong.
        kana_ink_end(_ink);
    }

    if(_ink->stroke_count >= KANA_INK_MAX_STROKES) {
        _ink->dropped_strokes = true;
        return;
    }

    kana_ink_stroke* _stroke = &_ink->strokes[_ink->stroke_count++];
    _stroke->point_count = 0;
    _stroke->from_pen    = _from_pen;
    _stroke->eraser      = _eraser;

    _ink->drawing = true;

    if(_from_pen) {
        _ink->pen_seen = true;
    } else {
        // A touch carries no pressure worth having (SDL reports 1.0 on most
        // panels), so the stroke draws at the base width instead of maximum.
        _ink->pressure = 0.0f;
    }

    kana_ink_push_point(_ink, _position);
}

void kana_ink_extend(kana_ink* _ink, rde_vec_2F _position) {
    if(!_ink->drawing || _ink->stroke_count == 0) {
        return;
    }

    const kana_ink_stroke* _stroke = &_ink->strokes[_ink->stroke_count - 1];

    if(_stroke->point_count > 0) {
        const rde_vec_2F _last = _stroke->points[_stroke->point_count - 1].position;
        const f32 _dx = _position.x - _last.x;
        const f32 _dy = _position.y - _last.y;

        // Coincident samples make the segment direction undefined, and
        // normalising a zero vector is a NaN that spreads through the whole
        // quad. Dropping them costs nothing: they carry no new shape.
        if((_dx * _dx + _dy * _dy) < (KANA_INK_MIN_STEP_PX * KANA_INK_MIN_STEP_PX)) {
            return;
        }
    }

    kana_ink_push_point(_ink, _position);
}

void kana_ink_end(kana_ink* _ink) {
    if(!_ink->drawing) {
        return;
    }

    _ink->drawing = false;

    // A stroke with a single point is a tap, not a mark. Kept anyway — in
    // Japanese it may well be a legitimate short stroke, and the renderer stamps
    // a dot for it.
}

void kana_ink_frame_begin(kana_ink* _ink, f64 _now) {
    _ink->samples_this_frame = 0;

    if(_ink->_hz_window_start <= 0.0) {
        _ink->_hz_window_start = _now;
        return;
    }

    // A rolling half-second window. Long enough that the number is readable
    // rather than flickering, short enough to respond within one stroke.
    const f64 _elapsed = _now - _ink->_hz_window_start;

    if(_elapsed >= 0.5) {
        _ink->sample_hz          = (f32)((f64)_ink->_hz_window_samples / _elapsed);
        _ink->_hz_window_samples = 0;
        _ink->_hz_window_start   = _now;
    }
}

u32 kana_ink_total_points(const kana_ink* _ink) {
    u32 _total = 0;

    for(u32 _i = 0; _i < _ink->stroke_count; _i++) {
        _total += _ink->strokes[_i].point_count;
    }

    return _total;
}

RDE_INTERNAL f32 kana_ink_width_at(const kana_ink_point* _point) {
    return KANA_INK_WIDTH_BASE + KANA_INK_WIDTH_PRESSURE * _point->pressure;
}

// One segment as a quad, with the two ends at their own widths so the stroke
// tapers with pressure rather than stepping.
RDE_INTERNAL void kana_ink_draw_segment(rde_vec_2F _a, f32 _wa, rde_vec_2F _b, f32 _wb, rde_color _color) {
    const f32 _dx  = _b.x - _a.x;
    const f32 _dy  = _b.y - _a.y;
    const f32 _len = sqrtf(_dx * _dx + _dy * _dy);

    if(_len < 0.0001f) {
        return;
    }

    // Left-hand normal of the segment direction.
    const f32 _nx = -_dy / _len;
    const f32 _ny =  _dx / _len;

    const rde_vec_2F _quad[4] = {
        { _a.x + _nx * _wa, _a.y + _ny * _wa },
        { _b.x + _nx * _wb, _b.y + _ny * _wb },
        { _b.x - _nx * _wb, _b.y - _ny * _wb },
        { _a.x - _nx * _wa, _a.y - _ny * _wa }
    };

    rde_rendering_2d_draw_polygon(_quad, 4, _color, NULL);
}

void kana_ink_render(kana_ink* _ink, f64 _now, b8 _show_samples) {
    f64 _newest = 0.0;

    for(u32 _s = 0; _s < _ink->stroke_count; _s++) {
        const kana_ink_stroke* _stroke = &_ink->strokes[_s];

        if(_stroke->point_count == 0) {
            continue;
        }

        const rde_color _color = _stroke->eraser ? KANA_INK_ERASER_COLOR : KANA_INK_COLOR;

        // A disc at every sample gives round caps AND round joins for free. The
        // alternative — mitring the quads — is more code and looks worse at the
        // sharp reversals Japanese strokes are full of.
        for(u32 _p = 0; _p < _stroke->point_count; _p++) {
            const kana_ink_point* _point = &_stroke->points[_p];

            rde_rendering_2d_draw_circle(_point->position, kana_ink_width_at(_point),
                                         KANA_INK_CIRCLE_SEGS, _color, NULL);

            if(_p > 0) {
                const kana_ink_point* _prev = &_stroke->points[_p - 1];

                kana_ink_draw_segment(_prev->position, kana_ink_width_at(_prev),
                                      _point->position, kana_ink_width_at(_point), _color);
            }

            if(_point->time > _newest) {
                _newest = _point->time;
            }
        }

        // Every raw sample as a dot. THIS IS THE DIAGNOSTIC THAT MATTERS: draw a
        // fast stroke and look at the spacing. Evenly spaced dots mean the pen's
        // full rate is arriving; long gaps on the fast parts mean samples are
        // being coalesced to the frame rate and quick strokes will be polygons.
        if(_show_samples) {
            for(u32 _p = 0; _p < _stroke->point_count; _p++) {
                rde_rendering_2d_draw_circle(_stroke->points[_p].position, 1.5f, 6u,
                                             KANA_INK_SAMPLE_COLOR, NULL);
            }
        }
    }

    // Measured HERE rather than at event time, because this is the moment the
    // ink actually reaches the frame. It is the app-side half of the latency
    // only — everything between the nib touching glass and the event arriving is
    // invisible from in here, and needs a camera to see.
    if(_newest > 0.0) {
        _ink->stale_ms = (f32)((_now - _newest) * 1000.0);

        if(_ink->stale_ms > _ink->stale_ms_max) {
            _ink->stale_ms_max = _ink->stale_ms;
        }
    }
}
