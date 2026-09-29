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

// Typed views of the arrays, read the way rde_arr_foreach reads them. An add may
// move an array, so take these again after adding to it.
RDE_INTERNAL kana_ink_stroke* kana_ink_strokes(const kana_ink* _ink) { return (kana_ink_stroke*)_ink->strokes.memory; }
RDE_INTERNAL kana_ink_point*  kana_ink_points(const kana_ink* _ink)  { return (kana_ink_point*)_ink->points.memory; }
RDE_INTERNAL kana_ink_action* kana_ink_actions(const kana_ink* _ink) { return (kana_ink_action*)_ink->actions.memory; }
RDE_INTERNAL u32*             kana_ink_targets(const kana_ink* _ink) { return (u32*)_ink->targets.memory; }

RDE_INTERNAL u32 kana_ink_len(const rde_arr* _arr) {
    return (u32)rde_arr_length(_arr);
}

// Drops the tail past _length. rde_arr has clear but no truncate; this is what
// rde_arr_clear does, to a length other than zero.
RDE_INTERNAL void kana_ink_truncate(rde_arr* _arr, u32 _length) {
    if(_length < _arr->count) {
        _arr->count = _length;
    }
}

void kana_ink_init(kana_ink* _ink) {
    memset(_ink, 0, sizeof(*_ink));
    _ink->width_mode      = KANA_INK_WIDTH_MODE_CONSTANT;
    _ink->brush_scale     = KANA_INK_BRUSH_SCALE_PAGE;
    _ink->color           = KANA_INK_COLOR;
    _ink->constant_radius = KANA_INK_RADIUS_DEFAULT;
    _ink->zoom            = 1.0f;

    // The page has no natural size limit, so its arrays live on the standard heap:
    // the engine's default allocator is a fixed-budget free list, and running that
    // out is a fatal assert, not an error the app could handle.
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _ink->strokes            = rde_arr_new(sizeof(kana_ink_stroke), _heap);
    _ink->points             = rde_arr_new(sizeof(kana_ink_point),  _heap);
    _ink->actions            = rde_arr_new(sizeof(kana_ink_action), _heap);
    _ink->targets            = rde_arr_new(sizeof(u32),             _heap);
    _ink->_scratch_positions = rde_arr_new(sizeof(rde_vec_2F),      _heap);
    _ink->_scratch_radii     = rde_arr_new(sizeof(f32),             _heap);
}

void kana_ink_destroy(kana_ink* _ink) {
    rde_arr* _arrays[] = { &_ink->strokes, &_ink->points, &_ink->actions, &_ink->targets, &_ink->_scratch_positions, &_ink->_scratch_radii };

    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }

    memset(_ink, 0, sizeof(*_ink));
}

// The stroke's bounds from all its points (points ± radius).
RDE_INTERNAL void kana_ink_recompute_bounds(const kana_ink* _ink, kana_ink_stroke* _stroke) {
    const kana_ink_point* _p = &kana_ink_points(_ink)[_stroke->first_point];

    for(u32 _i = 0; _i < _stroke->point_count; _i++) {
        const rde_vec_2F _lo = { _p[_i].position.x - _p[_i].radius, _p[_i].position.y - _p[_i].radius };
        const rde_vec_2F _hi = { _p[_i].position.x + _p[_i].radius, _p[_i].position.y + _p[_i].radius };

        if(_i == 0) {
            _stroke->bounds_min = _lo;
            _stroke->bounds_max = _hi;
        } else {
            _stroke->bounds_min = (rde_vec_2F){ _lo.x < _stroke->bounds_min.x ? _lo.x : _stroke->bounds_min.x, _lo.y < _stroke->bounds_min.y ? _lo.y : _stroke->bounds_min.y };
            _stroke->bounds_max = (rde_vec_2F){ _hi.x > _stroke->bounds_max.x ? _hi.x : _stroke->bounds_max.x, _hi.y > _stroke->bounds_max.y ? _hi.y : _stroke->bounds_max.y };
        }
    }
}

void kana_ink_add_loaded_stroke(kana_ink* _ink, const kana_ink_point* _points, u32 _count, rde_color _color, b8 _from_pen) {
    if(_count == 0 || _ink->drawing || kana_ink_len(&_ink->actions) > 0) {
        return;
    }

    kana_ink_stroke _stroke = {
        .first_point = kana_ink_len(&_ink->points),
        .point_count = _count,
        .color       = _color,
        .from_pen    = _from_pen,
        .alive       = true,
    };
    rde_arr_add(&_ink->strokes, &_stroke);
    memcpy(rde_arr_add_n(&_ink->points, _count), _points, (usize)_count * sizeof(kana_ink_point));
    kana_ink_recompute_bounds(_ink, &kana_ink_strokes(_ink)[kana_ink_len(&_ink->strokes) - 1]);
}

const kana_ink_point* kana_ink_stroke_points(const kana_ink* _ink, const kana_ink_stroke* _stroke) {
    return &kana_ink_points(_ink)[_stroke->first_point];
}

u32 kana_ink_stroke_count(const kana_ink* _ink) {
    return kana_ink_len(&_ink->strokes);
}

const kana_ink_stroke* kana_ink_stroke_at(const kana_ink* _ink, u32 _index) {
    return &kana_ink_strokes(_ink)[_index];
}

u32 kana_ink_undo_steps(const kana_ink* _ink) {
    return _ink->action_count;
}

u32 kana_ink_redo_steps(const kana_ink* _ink) {
    return kana_ink_len(&_ink->actions) - _ink->action_count;
}

// --- history -------------------------------------------------------------------

// A new edit after some undos: the undone actions can no longer be redone, and
// what they wrote is dropped for good. Everything they reference sits at the TAIL
// of strokes, points and targets — all three only grow at the end, in history
// order — so dropping it is a truncation. (The first discarded WRITE's stroke is
// the oldest one to go; everything after it went with it.)
RDE_INTERNAL void kana_ink_discard_redo(kana_ink* _ink) {
    const kana_ink_action* _actions = kana_ink_actions(_ink);

    for(u32 _a = _ink->action_count; _a < kana_ink_len(&_ink->actions); _a++) {
        const kana_ink_action* _action = &_actions[_a];

        if(_action->type == KANA_INK_ACTION_WRITE) {
            if(_action->first < kana_ink_len(&_ink->strokes)) {
                kana_ink_truncate(&_ink->points, kana_ink_strokes(_ink)[_action->first].first_point);
                kana_ink_truncate(&_ink->strokes, _action->first);
            }
        } else if(_action->first < kana_ink_len(&_ink->targets)) {
            kana_ink_truncate(&_ink->targets, _action->first);
        }
    }

    kana_ink_truncate(&_ink->actions, _ink->action_count);
}

// The caller has already discarded the redo branch.
RDE_INTERNAL void kana_ink_push_action(kana_ink* _ink, KANA_INK_ACTION_ _type, u32 _first, u32 _count, rde_vec_2F _delta) {
    kana_ink_action _action = { .type = _type, .first = _first, .count = _count, .delta = _delta };
    rde_arr_add(&_ink->actions, &_action);
    _ink->action_count = kana_ink_len(&_ink->actions);
}

// Moves the strokes in targets[_first .. _first + _count): points and bounds.
RDE_INTERNAL void kana_ink_translate(kana_ink* _ink, u32 _first, u32 _count, rde_vec_2F _delta) {
    kana_ink_stroke* _strokes = kana_ink_strokes(_ink);
    kana_ink_point*  _points  = kana_ink_points(_ink);
    const u32*       _targets = kana_ink_targets(_ink);

    for(u32 _i = 0; _i < _count; _i++) {
        kana_ink_stroke* _stroke = &_strokes[_targets[_first + _i]];
        kana_ink_point*  _p      = &_points[_stroke->first_point];

        for(u32 _j = 0; _j < _stroke->point_count; _j++) {
            _p[_j].position.x += _delta.x;
            _p[_j].position.y += _delta.y;
        }

        _stroke->bounds_min.x += _delta.x;
        _stroke->bounds_min.y += _delta.y;
        _stroke->bounds_max.x += _delta.x;
        _stroke->bounds_max.y += _delta.y;
    }

    _ink->revision++;
}

// Applies an action (_forward) or reverts it.
RDE_INTERNAL void kana_ink_apply(kana_ink* _ink, const kana_ink_action* _action, b8 _forward) {
    _ink->revision++;

    kana_ink_stroke* _strokes = kana_ink_strokes(_ink);

    if(_action->type == KANA_INK_ACTION_WRITE) {
        for(u32 _i = 0; _i < _action->count; _i++) {
            _strokes[_action->first + _i].alive = _forward;
        }
        return;
    }

    if(_action->type == KANA_INK_ACTION_MOVE) {
        const rde_vec_2F _d = _forward ? _action->delta : (rde_vec_2F){ -_action->delta.x, -_action->delta.y };
        kana_ink_translate(_ink, _action->first, _action->count, _d);
        return;
    }

    const u32* _targets = kana_ink_targets(_ink);
    for(u32 _i = 0; _i < _action->count; _i++) {
        _strokes[_targets[_action->first + _i]].alive = !_forward;
    }
}

// Closes whatever gesture group is open (an erase swipe, a move), making it
// history. Anything that reads or rewrites history calls this first.
RDE_INTERNAL void kana_ink_close_groups(kana_ink* _ink) {
    kana_ink_erase_end(_ink);
    kana_ink_move_end(_ink);
}

// From the history alone, not whether a stroke is open: a button greying out for
// the length of every stroke would flicker. (The open stroke is already an action.)
b8 kana_ink_can_undo(const kana_ink* _ink) {
    return _ink->action_count > 0 || _ink->_erase_open || _ink->_move_open;
}

b8 kana_ink_can_redo(const kana_ink* _ink) {
    return !_ink->_erase_open && !_ink->_move_open && _ink->action_count < kana_ink_len(&_ink->actions);
}

b8 kana_ink_undo(kana_ink* _ink) {
    if(_ink->drawing) {
        return false;
    }

    kana_ink_close_groups(_ink);

    if(_ink->action_count == 0) {
        return false;
    }

    _ink->action_count--;
    kana_ink_apply(_ink, &kana_ink_actions(_ink)[_ink->action_count], false);
    return true;
}

b8 kana_ink_redo(kana_ink* _ink) {
    if(_ink->drawing) {
        return false;
    }

    kana_ink_close_groups(_ink);

    if(_ink->action_count >= kana_ink_len(&_ink->actions)) {
        return false;
    }

    kana_ink_apply(_ink, &kana_ink_actions(_ink)[_ink->action_count], true);
    _ink->action_count++;
    return true;
}

// Erases one stroke as part of the open erase gesture, opening it on the first.
// Lazily, so a swipe that erases nothing doesn't throw away the redo branch.
RDE_INTERNAL void kana_ink_erase_stroke(kana_ink* _ink, u32 _stroke) {
    if(!_ink->_erase_open) {
        kana_ink_move_end(_ink);
        // Only DEAD strokes can be truncated away here, and the one being erased
        // is alive, so its index stays valid.
        kana_ink_discard_redo(_ink);
        _ink->_erase_open  = true;
        _ink->_erase_first = kana_ink_len(&_ink->targets);
    }

    rde_arr_add(&_ink->targets, &_stroke);
    kana_ink_strokes(_ink)[_stroke].alive = false;
    _ink->revision++;
}

void kana_ink_erase_strokes(kana_ink* _ink, const u32* _ids, u32 _count) {
    if(_ink->drawing) {
        return;
    }

    kana_ink_close_groups(_ink);

    for(u32 _i = 0; _i < _count; _i++) {
        if(_ids[_i] < kana_ink_len(&_ink->strokes) && kana_ink_strokes(_ink)[_ids[_i]].alive) {
            kana_ink_erase_stroke(_ink, _ids[_i]);
        }
    }

    kana_ink_erase_end(_ink);
}

u32 kana_ink_add_strokes(kana_ink* _ink, const kana_ink_stroke* _strokes, u32 _count, const kana_ink_point* _points, rde_vec_2F _offset) {
    if(_ink->drawing || _count == 0) {
        return UINT32_MAX;
    }

    kana_ink_close_groups(_ink);
    kana_ink_discard_redo(_ink);

    const u32 _first = kana_ink_len(&_ink->strokes);

    for(u32 _i = 0; _i < _count; _i++) {
        const kana_ink_stroke* _src = &_strokes[_i];
        if(_src->point_count == 0) {
            continue;
        }

        kana_ink_stroke _stroke = {
            .first_point = kana_ink_len(&_ink->points),
            .point_count = _src->point_count,
            .color       = _src->color,
            .from_pen    = _src->from_pen,
            .eraser      = _src->eraser,
            .alive       = true,
        };

        kana_ink_point* _dst = rde_arr_add_n(&_ink->points, _src->point_count);
        memcpy(_dst, &_points[_src->first_point], (usize)_src->point_count * sizeof(kana_ink_point));
        for(u32 _p = 0; _p < _src->point_count; _p++) {
            _dst[_p].position.x += _offset.x;
            _dst[_p].position.y += _offset.y;
        }

        rde_arr_add(&_ink->strokes, &_stroke);
        kana_ink_recompute_bounds(_ink, &kana_ink_strokes(_ink)[kana_ink_len(&_ink->strokes) - 1]);
    }

    const u32 _added = kana_ink_len(&_ink->strokes) - _first;
    if(_added == 0) {
        return UINT32_MAX;
    }

    kana_ink_push_action(_ink, KANA_INK_ACTION_WRITE, _first, _added, (rde_vec_2F){ 0.0f, 0.0f });
    _ink->revision++;
    return _first;
}

void kana_ink_move_begin(kana_ink* _ink, const u32* _ids, u32 _count) {
    if(_ink->drawing) {
        return;
    }

    kana_ink_close_groups(_ink);
    // Only DEAD strokes can be truncated here; the ones moving are alive.
    kana_ink_discard_redo(_ink);

    _ink->_move_open  = true;
    _ink->_move_first = kana_ink_len(&_ink->targets);
    _ink->_move_delta = (rde_vec_2F){ 0.0f, 0.0f };

    for(u32 _i = 0; _i < _count; _i++) {
        u32 _id = _ids[_i];
        if(_id < kana_ink_len(&_ink->strokes) && kana_ink_strokes(_ink)[_id].alive) {
            rde_arr_add(&_ink->targets, &_id);
        }
    }
}

void kana_ink_move_by(kana_ink* _ink, rde_vec_2F _delta) {
    if(!_ink->_move_open) {
        return;
    }

    kana_ink_translate(_ink, _ink->_move_first, kana_ink_len(&_ink->targets) - _ink->_move_first, _delta);
    _ink->_move_delta.x += _delta.x;
    _ink->_move_delta.y += _delta.y;
}

void kana_ink_move_end(kana_ink* _ink) {
    if(!_ink->_move_open) {
        return;
    }

    _ink->_move_open = false;
    const u32 _count = kana_ink_len(&_ink->targets) - _ink->_move_first;

    // Put down where it was picked up: nothing happened.
    if(_count == 0 || !(fabsf(_ink->_move_delta.x) > 0.0f || fabsf(_ink->_move_delta.y) > 0.0f)) {
        kana_ink_truncate(&_ink->targets, _ink->_move_first);
        return;
    }

    kana_ink_push_action(_ink, KANA_INK_ACTION_MOVE, _ink->_move_first, _count, _ink->_move_delta);
}

void kana_ink_erase_end(kana_ink* _ink) {
    if(!_ink->_erase_open) {
        return;
    }

    _ink->_erase_open = false;
    kana_ink_push_action(_ink, KANA_INK_ACTION_ERASE, _ink->_erase_first, kana_ink_len(&_ink->targets) - _ink->_erase_first, (rde_vec_2F){ 0.0f, 0.0f });
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
    kana_ink_end(_ink);
    kana_ink_close_groups(_ink);

    for(u32 _s = 0; _s < kana_ink_len(&_ink->strokes); _s++) {
        if(kana_ink_strokes(_ink)[_s].alive) {
            kana_ink_erase_stroke(_ink, _s);
        }
    }

    kana_ink_erase_end(_ink);   // one action: one undo brings the whole page back
    _ink->stale_ms_max = 0.0f;
}

RDE_INTERNAL f32 kana_ink_radius_from_pressure(f32 _pressure) {
    return KANA_INK_WIDTH_BASE + KANA_INK_WIDTH_PRESSURE * _pressure;
}

RDE_INTERNAL kana_ink_stroke* kana_ink_open_pen_stroke(kana_ink* _ink) {
    if(!_ink->drawing || kana_ink_len(&_ink->strokes) == 0) {
        return NULL;
    }

    kana_ink_stroke* _stroke = &kana_ink_strokes(_ink)[kana_ink_len(&_ink->strokes) - 1];
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
            const kana_ink_stroke* _stroke = kana_ink_open_pen_stroke(_ink);
            kana_ink_point*        _points = &kana_ink_points(_ink)[_stroke->first_point];
            for(u32 _p = 0; _p < _stroke->point_count; _p++) {
                _points[_p].radius = kana_ink_width_to_canvas(_ink, kana_ink_radius_from_pressure(_points[_p].pressure));
            }
            kana_ink_recompute_bounds(_ink, kana_ink_open_pen_stroke(_ink));
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
            const rde_vec_2F _last  = kana_ink_stroke_points(_ink, _stroke)[_stroke->point_count - 1].position;
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
    if(kana_ink_len(&_ink->strokes) == 0) {
        return;
    }

    // The open stroke is always the LAST, so its points stay contiguous at the
    // tail. The slot is added first (the array may move); the stroke's count only
    // grows once it is filled, so the reads below still see the previous point.
    kana_ink_point*  _point  = rde_arr_add(&_ink->points, NULL);
    kana_ink_stroke* _stroke = &kana_ink_strokes(_ink)[kana_ink_len(&_ink->strokes) - 1];

    f32 _radius = _ink->constant_radius;

    if(_ink->width_mode == KANA_INK_WIDTH_MODE_PRESSURE) {
        // A pen without real pressure takes its width from speed. Mouse and touch
        // strokes keep the pressure begin() gave them (zero: the base width).
        const f32 _width_pressure = (_stroke->from_pen && !_ink->pressure_live)
                                  ? kana_ink_simulated_pressure(_ink, _stroke, _position)
                                  : _ink->pressure;
        _radius = kana_ink_radius_from_pressure(_width_pressure);
    }

    // Pen samples carry their event time (several arrive per frame). The desktop
    // mouse is polled once a frame and has none: frame clock, and the HUD's rate
    // reads the frame rate.
    const f64 _now = rde_engine_get_time_now();
    const f64 _t   = _stroke->from_pen ? _ink->sample_time : _now;
    if(_stroke->point_count == 0) {
        _ink->_stroke_t0 = _t;
    }

    _stroke->point_count++;
    _point->position = _position;
    _point->pressure = _ink->pressure;
    _point->radius   = kana_ink_width_to_canvas(_ink, _radius);
    _point->time     = (f32)(_t - _ink->_stroke_t0);

    {
        const rde_vec_2F _lo = { _position.x - _point->radius, _position.y - _point->radius };
        const rde_vec_2F _hi = { _position.x + _point->radius, _position.y + _point->radius };
        if(_stroke->point_count == 1) {
            _stroke->bounds_min = _lo;
            _stroke->bounds_max = _hi;
        } else {
            _stroke->bounds_min = (rde_vec_2F){ _lo.x < _stroke->bounds_min.x ? _lo.x : _stroke->bounds_min.x, _lo.y < _stroke->bounds_min.y ? _lo.y : _stroke->bounds_min.y };
            _stroke->bounds_max = (rde_vec_2F){ _hi.x > _stroke->bounds_max.x ? _hi.x : _stroke->bounds_max.x, _hi.y > _stroke->bounds_max.y ? _hi.y : _stroke->bounds_max.y };
        }
    }

    _ink->_newest_handled = _now;
    _ink->samples_this_frame++;
    _ink->revision++;

    if(_stroke->point_count == 1) {
        _ink->_hz_first_time = _t;
        _ink->_hz_samples    = 1;
    } else {
        _ink->_hz_samples++;
        if(_t > _ink->_hz_first_time) {
            _ink->sample_hz = (f32)((f64)(_ink->_hz_samples - 1) / (_t - _ink->_hz_first_time));
        }
    }
}

void kana_ink_begin(kana_ink* _ink, rde_vec_2F _position, b8 _from_pen, b8 _eraser) {
    if(_ink->drawing) {
        // A second down without an up. Close the old one rather than merging:
        // two marks the user made separately must stay two strokes, or the
        // stroke COUNT — the first thing scoring checks — is wrong.
        kana_ink_end(_ink);
    }

    kana_ink_close_groups(_ink);
    kana_ink_discard_redo(_ink);

    kana_ink_stroke _stroke = {
        .first_point = kana_ink_len(&_ink->points),
        .point_count = 0,
        .color       = _ink->color,
        .from_pen    = _from_pen,
        .eraser      = _eraser,
        .alive       = true,
    };
    rde_arr_add(&_ink->strokes, &_stroke);
    const u32 _index = kana_ink_len(&_ink->strokes) - 1;
    kana_ink_push_action(_ink, KANA_INK_ACTION_WRITE, _index, 1u, (rde_vec_2F){ 0.0f, 0.0f });
    _ink->revision++;

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
    if(!_ink->drawing || kana_ink_len(&_ink->strokes) == 0) {
        return;
    }

    const kana_ink_stroke* _stroke = &kana_ink_strokes(_ink)[kana_ink_len(&_ink->strokes) - 1];

    if(_stroke->point_count > 0) {
        const rde_vec_2F _last = kana_ink_stroke_points(_ink, _stroke)[_stroke->point_count - 1].position;
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

RDE_INTERNAL b8 kana_ink_stroke_touches(const kana_ink* _ink, const kana_ink_stroke* _stroke, rde_vec_2F _point, f32 _radius) {
    const kana_ink_point* _points = kana_ink_stroke_points(_ink, _stroke);

    for(u32 _p = 0; _p < _stroke->point_count; _p++) {
        const kana_ink_point* _a = &_points[_p];
        const kana_ink_point* _b = &_points[_p + 1 < _stroke->point_count ? _p + 1 : _p];
        const f32 _reach = _radius + (_a->radius > _b->radius ? _a->radius : _b->radius);

        if(kana_ink_dist2_to_segment(_point, _a->position, _b->position) <= _reach * _reach) {
            return true;
        }
    }

    return false;
}

// Cheap reject before the per-segment test.
RDE_INTERNAL b8 kana_ink_near_bounds(const kana_ink_stroke* _stroke, rde_vec_2F _point, f32 _radius) {
    return _point.x >= _stroke->bounds_min.x - _radius && _point.x <= _stroke->bounds_max.x + _radius &&
           _point.y >= _stroke->bounds_min.y - _radius && _point.y <= _stroke->bounds_max.y + _radius;
}

// How far _point is from the stroke's INK — its edge, not its centre line.
// Negative when it is on the ink.
RDE_INTERNAL f32 kana_ink_stroke_gap(const kana_ink* _ink, const kana_ink_stroke* _stroke, rde_vec_2F _point) {
    const kana_ink_point* _points = kana_ink_stroke_points(_ink, _stroke);
    f32                   _gap    = 1e30f;

    for(u32 _p = 0; _p < _stroke->point_count; _p++) {
        const kana_ink_point* _a = &_points[_p];
        const kana_ink_point* _b = &_points[_p + 1 < _stroke->point_count ? _p + 1 : _p];
        const f32 _g = sqrtf(kana_ink_dist2_to_segment(_point, _a->position, _b->position)) - (_a->radius > _b->radius ? _a->radius : _b->radius);
        _gap = _g < _gap ? _g : _gap;
    }

    return _gap;
}

u32 kana_ink_pick(const kana_ink* _ink, rde_vec_2F _point, f32 _radius) {
    const kana_ink_stroke* _strokes = kana_ink_strokes(_ink);
    u32                    _best    = UINT32_MAX;
    f32                    _best_gap = _radius;

    // The NEAREST ink within reach — a tap between two strokes means the one it
    // is closer to. Newest first, so on a tie (overlapping ink) the one drawn on
    // top wins.
    for(u32 _s = kana_ink_len(&_ink->strokes); _s-- > 0;) {
        if(!_strokes[_s].alive || !kana_ink_near_bounds(&_strokes[_s], _point, _radius)) {
            continue;
        }

        const f32 _gap = kana_ink_stroke_gap(_ink, &_strokes[_s], _point);
        if(_gap <= _best_gap && (_best == UINT32_MAX || _gap < _best_gap)) {
            _best     = _s;
            _best_gap = _gap;
        }
    }

    return _best;
}

u32 kana_ink_erase_at(kana_ink* _ink, rde_vec_2F _point, f32 _radius) {
    if(_ink->drawing) {
        return 0;
    }

    u32 _erased = 0;

    // Marked dead, never moved: order is preserved (it is the stroke ORDER scoring
    // reads) and the erase stays undoable.
    for(u32 _s = 0; _s < kana_ink_len(&_ink->strokes); _s++) {
        const kana_ink_stroke* _stroke = &kana_ink_strokes(_ink)[_s];
        if(_stroke->alive && kana_ink_near_bounds(_stroke, _point, _radius) && kana_ink_stroke_touches(_ink, _stroke, _point, _radius)) {
            kana_ink_erase_stroke(_ink, _s);
            _erased++;
        }
    }

    return _erased;
}

void kana_ink_frame_begin(kana_ink* _ink) {
    _ink->samples_this_frame = 0;
}

u32 kana_ink_alive_strokes(const kana_ink* _ink) {
    const kana_ink_stroke* _strokes = kana_ink_strokes(_ink);
    u32                    _alive   = 0;

    for(u32 _i = 0; _i < kana_ink_len(&_ink->strokes); _i++) {
        _alive += _strokes[_i].alive ? 1u : 0u;
    }

    return _alive;
}

u32 kana_ink_total_points(const kana_ink* _ink) {
    const kana_ink_stroke* _strokes = kana_ink_strokes(_ink);
    u32                    _total   = 0;

    for(u32 _i = 0; _i < kana_ink_len(&_ink->strokes); _i++) {
        _total += _strokes[_i].alive ? _strokes[_i].point_count : 0u;
    }

    return _total;
}

RDE_INTERNAL f32 kana_ink_width_at(const kana_ink_point* _point) {
    return _point->radius;
}

// Fills the scratch with one stroke in screen space (widths + _extra_px) and
// draws it as ONE antialiased shape: round caps and joins, a one-pixel fringe on
// the outline. It used to be a disc per sample plus a quad per segment:
// hard-edged, and it can't simply be softened — every piece would bring its own
// fringe and they'd stack into beads at each joint. Returns the positions.
RDE_INTERNAL const rde_vec_2F* kana_ink_emit_stroke(kana_ink* _ink, const kana_ink_stroke* _stroke, rde_vec_2F _offset, f32 _zoom, f32 _extra_px, rde_color _color) {
    rde_arr_clear(&_ink->_scratch_positions);
    rde_arr_clear(&_ink->_scratch_radii);
    rde_vec_2F* _positions = rde_arr_add_n(&_ink->_scratch_positions, _stroke->point_count);
    f32*        _radii     = rde_arr_add_n(&_ink->_scratch_radii,     _stroke->point_count);

    const kana_ink_point* _points = kana_ink_stroke_points(_ink, _stroke);

    for(u32 _p = 0; _p < _stroke->point_count; _p++) {
        const kana_ink_point* _point = &_points[_p];

        // Canvas → screen: points and widths scale with the zoom, like ink on
        // a page under a magnifier.
        _positions[_p] = (rde_vec_2F){ _point->position.x * _zoom + _offset.x, _point->position.y * _zoom + _offset.y };
        _radii[_p]     = rde_math_clamp_f32(kana_ink_width_at(_point) * _zoom, KANA_INK_MIN_SCREEN_RADIUS, 1e6f) + _extra_px;
    }

    rde_rendering_2d_draw_stroke(_positions, _radii, _stroke->point_count, _color);
    return _positions;
}

void kana_ink_draw_stroke(kana_ink* _ink, u32 _index, rde_vec_2F _offset, f32 _zoom, f32 _extra_px, rde_color _color) {
    if(_index >= kana_ink_len(&_ink->strokes)) {
        return;
    }

    const kana_ink_stroke* _stroke = &kana_ink_strokes(_ink)[_index];
    if(_stroke->alive && _stroke->point_count > 0) {
        kana_ink_emit_stroke(_ink, _stroke, _offset, _zoom, _extra_px, _color);
    }
}

void kana_ink_render(kana_ink* _ink, rde_vec_2F _offset, f32 _zoom, rde_vec_2F _screen_half, f64 _now, b8 _show_samples) {
    // The canvas rect on screen, padded by the thinnest drawn ink: a stroke whose
    // bounds miss it is not drawn at all, so a big page costs what is visible.
    const f32        _pad     = (KANA_INK_MIN_SCREEN_RADIUS + 2.0f) / _zoom;
    const rde_vec_2F _vis_min = { (-_screen_half.x - _offset.x) / _zoom - _pad, (-_screen_half.y - _offset.y) / _zoom - _pad };
    const rde_vec_2F _vis_max = { ( _screen_half.x - _offset.x) / _zoom + _pad, ( _screen_half.y - _offset.y) / _zoom + _pad };

    const kana_ink_stroke* _strokes = kana_ink_strokes(_ink);

    for(u32 _s = 0; _s < kana_ink_len(&_ink->strokes); _s++) {
        const kana_ink_stroke* _stroke = &_strokes[_s];

        if(!_stroke->alive || _stroke->point_count == 0 ||
           _stroke->bounds_max.x < _vis_min.x || _stroke->bounds_min.x > _vis_max.x ||
           _stroke->bounds_max.y < _vis_min.y || _stroke->bounds_min.y > _vis_max.y) {
            continue;
        }

        const rde_color   _color     = _stroke->eraser ? KANA_INK_ERASER_COLOR : _stroke->color;
        const rde_vec_2F* _positions = kana_ink_emit_stroke(_ink, _stroke, _offset, _zoom, 0.0f, _color);

        // Every raw sample as a dot. THIS IS THE DIAGNOSTIC THAT MATTERS: draw a
        // fast stroke and look at the spacing. Evenly spaced dots mean the pen's
        // full rate is arriving; long gaps on the fast parts mean samples are
        // being coalesced to the frame rate and quick strokes will be polygons.
        if(_show_samples) {
            for(u32 _p = 0; _p < _stroke->point_count; _p++) {
                rde_rendering_2d_draw_circle(_positions[_p], 1.5f, 6u,
                                             KANA_INK_SAMPLE_COLOR, NULL);
            }
        }
    }

    // Measured HERE rather than at event time, because this is the moment the
    // ink actually reaches the frame. It is the app-side half of the latency
    // only — everything between the nib touching glass and the event arriving is
    // invisible from in here, and needs a camera to see.
    //
    // Only while a stroke is open (the last one): after the pen lifts, "now minus
    // the newest sample" is just the time since the last stroke, not a lag.
    if(_ink->drawing && _ink->_newest_handled > 0.0) {
        _ink->stale_ms = (f32)((_now - _ink->_newest_handled) * 1000.0);

        if(_ink->stale_ms > _ink->stale_ms_max) {
            _ink->stale_ms_max = _ink->stale_ms;
        }
    }
}
