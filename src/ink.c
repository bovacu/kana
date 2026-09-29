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

void kana_ink_init(kana_ink* _ink) {
    memset(_ink, 0, sizeof(*_ink));
    _ink->width_mode      = KANA_INK_WIDTH_MODE_CONSTANT;
    _ink->brush_scale     = KANA_INK_BRUSH_SCALE_PAGE;
    _ink->color           = KANA_INK_COLOR;
    _ink->constant_radius = KANA_INK_RADIUS_DEFAULT;
    _ink->zoom            = 1.0f;
}

// Screen units → the canvas units points are stored in.
RDE_INTERNAL f32 kana_ink_to_canvas_units(const kana_ink* _ink, f32 _screen_units) {
    return _ink->zoom > 0.0f ? _screen_units / _ink->zoom : _screen_units;
}

// A brush WIDTH → the canvas units it is stored in. PAGE widths already are;
// SCREEN widths are held constant on screen, so they shrink by the zoom.
RDE_INTERNAL f32 kana_ink_width_to_canvas(const kana_ink* _ink, f32 _radius) {
    return _ink->brush_scale == KANA_INK_BRUSH_SCALE_SCREEN ? kana_ink_to_canvas_units(_ink, _radius) : _radius;
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

RDE_INTERNAL f32 kana_ink_radius_from_pressure(f32 _pressure) {
    return KANA_INK_WIDTH_BASE + KANA_INK_WIDTH_PRESSURE * _pressure;
}

RDE_INTERNAL kana_ink_stroke* kana_ink_open_pen_stroke(kana_ink* _ink) {
    if(!_ink->drawing || _ink->stroke_count == 0) {
        return NULL;
    }

    kana_ink_stroke* _stroke = &_ink->strokes[_ink->stroke_count - 1];
    return _stroke->from_pen ? _stroke : NULL;
}

// Only readings taken WHILE TOUCHING count: SDL also reports the pressure axis
// just before the down and just after the up, and those would fake a spread.
RDE_INTERNAL void kana_ink_track_pressure_range(kana_ink* _ink, f32 _raw) {
    if(_ink->pressure_live || kana_ink_open_pen_stroke(_ink) == NULL) {
        return;
    }

    if(!_ink->_pressure_seen_any) {
        _ink->_pressure_seen_min = _raw;
        _ink->_pressure_seen_max = _raw;
        _ink->_pressure_seen_any = true;
    } else {
        if(_raw < _ink->_pressure_seen_min) { _ink->_pressure_seen_min = _raw; }
        if(_raw > _ink->_pressure_seen_max) { _ink->_pressure_seen_max = _raw; }
    }

    if(_ink->_pressure_seen_max - _ink->_pressure_seen_min > KANA_INK_PRESSURE_LIVE_RANGE) {
        _ink->pressure_live = true;

        // In pressure mode, the stroke that proved it started on simulated widths;
        // redraw it from the real pressure it recorded all along, so it has no seam.
        if(_ink->width_mode == KANA_INK_WIDTH_MODE_PRESSURE) {
            kana_ink_stroke* _stroke = kana_ink_open_pen_stroke(_ink);
            for(u32 _p = 0; _p < _stroke->point_count; _p++) {
                _stroke->points[_p].radius = kana_ink_width_to_canvas(_ink, kana_ink_radius_from_pressure(_stroke->points[_p].pressure));
            }
        }
    }
}

// Width from speed, for a pen with no pressure. Speed from the EVENT times,
// between accepted samples, eased so a single fast or slow sample can't twitch it.
RDE_INTERNAL f32 kana_ink_simulated_pressure(kana_ink* _ink, const kana_ink_stroke* _stroke, rde_vec_2F _position) {
    if(_stroke->point_count == 0) {
        _ink->_sim_pressure = KANA_INK_SIM_START;
    } else {
        const f64 _dt = _ink->sample_time - _ink->_sim_last_time;

        if(_dt > 1e-5) {
            const rde_vec_2F _last  = _stroke->points[_stroke->point_count - 1].position;
            const f32        _dx    = _position.x - _last.x;
            const f32        _dy    = _position.y - _last.y;
            // Canvas distance back to screen units: the feel is the hand's speed.
            const f32        _speed = (f32)((f64)(sqrtf(_dx * _dx + _dy * _dy) * _ink->zoom) / _dt);
            const f32        _slow  = 1.0f - rde_math_clamp_f32(_speed / KANA_INK_SIM_FAST_SPEED, 0.0f, 1.0f);
            const f32        _target = KANA_INK_SIM_MIN + (KANA_INK_SIM_MAX - KANA_INK_SIM_MIN) * _slow;

            _ink->_sim_pressure += (_target - _ink->_sim_pressure) * KANA_INK_SIM_SMOOTH;
        }
    }

    _ink->_sim_last_time = _ink->sample_time;
    return _ink->_sim_pressure;
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
        kana_ink_track_pressure_range(_ink, _pen->pressure);

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

    f32 _radius = _ink->constant_radius;

    if(_ink->width_mode == KANA_INK_WIDTH_MODE_PRESSURE) {
        // A pen without real pressure takes its width from speed. Mouse and touch
        // strokes keep the pressure begin() gave them (zero: the base width).
        const f32 _width_pressure = (_stroke->from_pen && !_ink->pressure_live)
                                  ? kana_ink_simulated_pressure(_ink, _stroke, _position)
                                  : _ink->pressure;
        _radius = kana_ink_radius_from_pressure(_width_pressure);
    }

    kana_ink_point* _point = &_stroke->points[_stroke->point_count++];
    _point->position = _position;
    _point->pressure = _ink->pressure;
    _point->radius   = kana_ink_width_to_canvas(_ink, _radius);
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
    _stroke->color       = _ink->color;

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
        const f32 _min_step = kana_ink_to_canvas_units(_ink, KANA_INK_MIN_STEP_PX);
        if((_dx * _dx + _dy * _dy) < (_min_step * _min_step)) {
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

// Squared distance from _p to the segment _a-_b.
RDE_INTERNAL f32 kana_ink_dist2_to_segment(rde_vec_2F _p, rde_vec_2F _a, rde_vec_2F _b) {
    const f32 _abx = _b.x - _a.x;
    const f32 _aby = _b.y - _a.y;
    const f32 _len2 = _abx * _abx + _aby * _aby;
    f32 _t = 0.0f;

    if(_len2 > 0.0f) {
        _t = rde_math_clamp_f32(((_p.x - _a.x) * _abx + (_p.y - _a.y) * _aby) / _len2, 0.0f, 1.0f);
    }

    const f32 _dx = _p.x - (_a.x + _abx * _t);
    const f32 _dy = _p.y - (_a.y + _aby * _t);
    return _dx * _dx + _dy * _dy;
}

RDE_INTERNAL b8 kana_ink_stroke_touches(const kana_ink_stroke* _stroke, rde_vec_2F _point, f32 _radius) {
    for(u32 _p = 0; _p < _stroke->point_count; _p++) {
        const kana_ink_point* _a = &_stroke->points[_p];
        const kana_ink_point* _b = &_stroke->points[_p + 1 < _stroke->point_count ? _p + 1 : _p];
        const f32 _reach = _radius + (_a->radius > _b->radius ? _a->radius : _b->radius);

        if(kana_ink_dist2_to_segment(_point, _a->position, _b->position) <= _reach * _reach) {
            return true;
        }
    }

    return false;
}

u32 kana_ink_erase_at(kana_ink* _ink, rde_vec_2F _point, f32 _radius) {
    if(_ink->drawing) {
        return 0;
    }

    u32 _erased = 0;
    u32 _kept   = 0;

    for(u32 _s = 0; _s < _ink->stroke_count; _s++) {
        if(kana_ink_stroke_touches(&_ink->strokes[_s], _point, _radius)) {
            _erased++;
            continue;
        }

        // Compact in place; order is preserved — it is the stroke ORDER scoring reads.
        if(_kept != _s) {
            memmove(&_ink->strokes[_kept], &_ink->strokes[_s], sizeof(kana_ink_stroke));
        }
        _kept++;
    }

    _ink->stroke_count = _kept;
    return _erased;
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
    return _point->radius;
}

// Scratch for one stroke in the shape rde_rendering_2d_draw_stroke takes.
RDE_INTERNAL rde_vec_2F kana_ink_scratch_positions[KANA_INK_MAX_POINTS];
RDE_INTERNAL f32        kana_ink_scratch_radii[KANA_INK_MAX_POINTS];

void kana_ink_render(kana_ink* _ink, rde_vec_2F _offset, f32 _zoom, f64 _now, b8 _show_samples) {
    f64 _newest = 0.0;

    for(u32 _s = 0; _s < _ink->stroke_count; _s++) {
        const kana_ink_stroke* _stroke = &_ink->strokes[_s];

        if(_stroke->point_count == 0) {
            continue;
        }

        const rde_color _color = _stroke->eraser ? KANA_INK_ERASER_COLOR : _stroke->color;

        // The whole stroke as ONE antialiased shape (round caps and joins, a
        // one-pixel fringe on the outline). It used to be a disc per sample plus a
        // quad per segment: hard-edged, and it can't simply be softened — every
        // piece would bring its own fringe and they'd stack into beads at each joint.
        for(u32 _p = 0; _p < _stroke->point_count; _p++) {
            const kana_ink_point* _point = &_stroke->points[_p];

            // Canvas → screen: points and widths scale with the zoom, like ink on
            // a page under a magnifier.
            kana_ink_scratch_positions[_p] = (rde_vec_2F){ _point->position.x * _zoom + _offset.x, _point->position.y * _zoom + _offset.y };
            kana_ink_scratch_radii[_p]     = rde_math_clamp_f32(kana_ink_width_at(_point) * _zoom, KANA_INK_MIN_SCREEN_RADIUS, 1e6f);

            if(_point->time > _newest) {
                _newest = _point->time;
            }
        }

        rde_rendering_2d_draw_stroke(kana_ink_scratch_positions, kana_ink_scratch_radii, _stroke->point_count, _color);

        // Every raw sample as a dot. THIS IS THE DIAGNOSTIC THAT MATTERS: draw a
        // fast stroke and look at the spacing. Evenly spaced dots mean the pen's
        // full rate is arriving; long gaps on the fast parts mean samples are
        // being coalesced to the frame rate and quick strokes will be polygons.
        if(_show_samples) {
            for(u32 _p = 0; _p < _stroke->point_count; _p++) {
                rde_rendering_2d_draw_circle(kana_ink_scratch_positions[_p], 1.5f, 6u,
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
