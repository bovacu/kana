// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/ink/ink.h"
#include "drawing/base/theme.h"
#include "drawing/widgets/draw.h"

#include <math.h>
#include <string.h>

// ===========================================================================
// See ink.h. Two halves: capture (cheap, exact, keeps everything) and render
// (throwaway quality for now — the spike is about feel and timing, not beauty).
// ===========================================================================

// Typed views of the arrays, read the way rde_arr_foreach reads them. An add may
// move an array, so take these again after adding to it.
RDE_INTERNAL fude_ink_stroke* fude_ink_strokes(const fude_ink* _ink) { return (fude_ink_stroke*)_ink->strokes.memory; }
RDE_INTERNAL fude_ink_point*  fude_ink_points(const fude_ink* _ink)  { return (fude_ink_point*)_ink->points.memory; }
RDE_INTERNAL fude_ink_action* fude_ink_actions(const fude_ink* _ink) { return (fude_ink_action*)_ink->actions.memory; }
RDE_INTERNAL u32*             fude_ink_targets(const fude_ink* _ink) { return (u32*)_ink->targets.memory; }

RDE_INTERNAL u32 fude_ink_len(const rde_arr* _arr) {
    return (u32)rde_arr_length(_arr);
}

// Drops the tail past _length. rde_arr has clear but no truncate; this is what
// rde_arr_clear does, to a length other than zero.
RDE_INTERNAL void fude_ink_truncate(rde_arr* _arr, u32 _length) {
    if(_length < _arr->count) {
        _arr->count = _length;
    }
}

void fude_ink_init(fude_ink* _ink) {
    memset(_ink, 0, sizeof(*_ink));
    _ink->width_mode      = FUDE_INK_WIDTH_MODE_CONSTANT;
    _ink->brush_scale     = FUDE_INK_BRUSH_SCALE_PAGE;
    _ink->color           = FUDE_THEME_INK;
    _ink->constant_radius = FUDE_INK_RADIUS_DEFAULT;
    _ink->marker_color    = (rde_color){ 255, 214, 0, FUDE_INK_MARKER_ALPHA };   // the classic yellow
    _ink->marker_radius   = FUDE_INK_MARKER_RADIUS;
    _ink->eraser_radius   = FUDE_INK_ERASER_RADIUS;
    _ink->zoom            = 1.0f;

    // The page has no natural size limit, so its arrays live on the standard heap:
    // the engine's default allocator is a fixed-budget free list, and running that
    // out is a fatal assert, not an error the app could handle.
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _ink->strokes            = rde_arr_new(sizeof(fude_ink_stroke), _heap);
    _ink->points             = rde_arr_new(sizeof(fude_ink_point),  _heap);
    _ink->actions            = rde_arr_new(sizeof(fude_ink_action), _heap);
    _ink->targets            = rde_arr_new(sizeof(u32),             _heap);
    _ink->_scratch_positions = rde_arr_new(sizeof(rde_vec_2F),      _heap);
    _ink->_scratch_radii     = rde_arr_new(sizeof(f32),             _heap);
}

void fude_ink_destroy(fude_ink* _ink) {
    rde_arr* _arrays[] = { &_ink->strokes, &_ink->points, &_ink->actions, &_ink->targets, &_ink->_scratch_positions, &_ink->_scratch_radii };

    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }

    memset(_ink, 0, sizeof(*_ink));
}

// The stroke's bounds from all its points (points ± radius).
RDE_INTERNAL void fude_ink_recompute_bounds(const fude_ink* _ink, fude_ink_stroke* _stroke) {
    const fude_ink_point* _p = &fude_ink_points(_ink)[_stroke->first_point];

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

void fude_ink_add_loaded_stroke(fude_ink* _ink, const fude_ink_point* _points, u32 _count, rde_color _color, b8 _from_pen) {
    fude_ink_add_loaded_stroke_2(_ink, _points, _count, _color, _from_pen, false);
}

void fude_ink_add_loaded_stroke_2(fude_ink* _ink, const fude_ink_point* _points, u32 _count, rde_color _color, b8 _from_pen, b8 _marker) {
    if(_count == 0 || _ink->drawing || fude_ink_len(&_ink->actions) > 0) {
        return;
    }

    fude_ink_stroke _stroke = {
        .first_point = fude_ink_len(&_ink->points),
        .point_count = _count,
        .color       = _color,
        .from_pen    = _from_pen,
        .alive       = true,
        .marker      = _marker,
    };
    rde_arr_add(&_ink->strokes, &_stroke);
    memcpy(rde_arr_add_n(&_ink->points, _count), _points, (usize)_count * sizeof(fude_ink_point));
    fude_ink_recompute_bounds(_ink, &fude_ink_strokes(_ink)[fude_ink_len(&_ink->strokes) - 1]);
}

void fude_ink_remap(fude_ink* _ink, void (*_map)(any _user, u32 _stroke, fude_ink_point* _point), any _user) {
    fude_ink_stroke* _strokes = (fude_ink_stroke*)_ink->strokes.memory;
    fude_ink_point*  _points  = (fude_ink_point*)_ink->points.memory;
    for(u32 _s = 0; _s < fude_ink_len(&_ink->strokes); _s++) {
        if(!_strokes[_s].alive) {
            continue;
        }
        for(u32 _p = 0; _p < _strokes[_s].point_count; _p++) {
            _map(_user, _s, &_points[_strokes[_s].first_point + _p]);
        }
        fude_ink_recompute_bounds(_ink, &_strokes[_s]);
    }
    _ink->revision++;
}

const fude_ink_point* fude_ink_stroke_points(const fude_ink* _ink, const fude_ink_stroke* _stroke) {
    return &fude_ink_points(_ink)[_stroke->first_point];
}

u32 fude_ink_stroke_count(const fude_ink* _ink) {
    return fude_ink_len(&_ink->strokes);
}

const fude_ink_stroke* fude_ink_stroke_at(const fude_ink* _ink, u32 _index) {
    return &fude_ink_strokes(_ink)[_index];
}

u32 fude_ink_undo_steps(const fude_ink* _ink) {
    return _ink->action_count;
}

u32 fude_ink_redo_steps(const fude_ink* _ink) {
    return fude_ink_len(&_ink->actions) - _ink->action_count;
}

// --- history -------------------------------------------------------------------

// A new edit after some undos: the undone actions can no longer be redone, and
// what they wrote is dropped for good. Everything they reference sits at the TAIL
// of strokes, points and targets — all three only grow at the end, in history
// order — so dropping it is a truncation. (The first discarded WRITE's stroke is
// the oldest one to go; everything after it went with it.)
RDE_INTERNAL void fude_ink_discard_redo(fude_ink* _ink) {
    const fude_ink_action* _actions = fude_ink_actions(_ink);

    for(u32 _a = _ink->action_count; _a < fude_ink_len(&_ink->actions); _a++) {
        const fude_ink_action* _action = &_actions[_a];

        if(_action->type == FUDE_INK_ACTION_WRITE) {
            if(_action->first < fude_ink_len(&_ink->strokes)) {
                fude_ink_truncate(&_ink->points, fude_ink_strokes(_ink)[_action->first].first_point);
                fude_ink_truncate(&_ink->strokes, _action->first);
            }
        } else if(_action->first < fude_ink_len(&_ink->targets)) {
            fude_ink_truncate(&_ink->targets, _action->first);
        }
    }

    fude_ink_truncate(&_ink->actions, _ink->action_count);
}

// The caller has already discarded the redo branch.
RDE_INTERNAL void fude_ink_push_action(fude_ink* _ink, FUDE_INK_ACTION_ _type, u32 _first, u32 _count, rde_vec_2F _delta) {
    fude_ink_action _action = { .type = _type, .first = _first, .count = _count, .delta = _delta };
    rde_arr_add(&_ink->actions, &_action);
    _ink->action_count = fude_ink_len(&_ink->actions);
}

// Moves the strokes in targets[_first .. _first + _count): points and bounds.
RDE_INTERNAL void fude_ink_translate(fude_ink* _ink, u32 _first, u32 _count, rde_vec_2F _delta) {
    fude_ink_stroke* _strokes = fude_ink_strokes(_ink);
    fude_ink_point*  _points  = fude_ink_points(_ink);
    const u32*       _targets = fude_ink_targets(_ink);

    for(u32 _i = 0; _i < _count; _i++) {
        fude_ink_stroke* _stroke = &_strokes[_targets[_first + _i]];
        fude_ink_point*  _p      = &_points[_stroke->first_point];

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
RDE_INTERNAL void fude_ink_apply(fude_ink* _ink, const fude_ink_action* _action, b8 _forward) {
    _ink->revision++;

    fude_ink_stroke* _strokes = fude_ink_strokes(_ink);

    if(_action->type == FUDE_INK_ACTION_WRITE) {
        for(u32 _i = 0; _i < _action->count; _i++) {
            _strokes[_action->first + _i].alive = _forward;
        }
        return;
    }

    if(_action->type == FUDE_INK_ACTION_MOVE) {
        const rde_vec_2F _d = _forward ? _action->delta : (rde_vec_2F){ -_action->delta.x, -_action->delta.y };
        fude_ink_translate(_ink, _action->first, _action->count, _d);
        return;
    }

    const u32* _targets = fude_ink_targets(_ink);
    for(u32 _i = 0; _i < _action->count; _i++) {
        _strokes[_targets[_action->first + _i]].alive = !_forward;
    }
}

// Closes whatever gesture group is open (an erase swipe, a move), making it
// history. Anything that reads or rewrites history calls this first.
RDE_INTERNAL void fude_ink_close_groups(fude_ink* _ink) {
    fude_ink_erase_end(_ink);
    fude_ink_move_end(_ink);
}

// From the history alone, not whether a stroke is open: a button greying out for
// the length of every stroke would flicker. (The open stroke is already an action.)
b8 fude_ink_can_undo(const fude_ink* _ink) {
    return _ink->action_count > 0 || _ink->_erase_open || _ink->_move_open;
}

b8 fude_ink_can_redo(const fude_ink* _ink) {
    return !_ink->_erase_open && !_ink->_move_open && _ink->action_count < fude_ink_len(&_ink->actions);
}

b8 fude_ink_undo(fude_ink* _ink) {
    if(_ink->drawing) {
        return false;
    }

    fude_ink_close_groups(_ink);

    if(_ink->action_count == 0) {
        return false;
    }

    _ink->action_count--;
    fude_ink_apply(_ink, &fude_ink_actions(_ink)[_ink->action_count], false);
    return true;
}

b8 fude_ink_redo(fude_ink* _ink) {
    if(_ink->drawing) {
        return false;
    }

    fude_ink_close_groups(_ink);

    if(_ink->action_count >= fude_ink_len(&_ink->actions)) {
        return false;
    }

    fude_ink_apply(_ink, &fude_ink_actions(_ink)[_ink->action_count], true);
    _ink->action_count++;
    return true;
}

// Erases one stroke as part of the open erase gesture, opening it on the first.
// Lazily, so a swipe that erases nothing doesn't throw away the redo branch.
RDE_INTERNAL void fude_ink_erase_stroke(fude_ink* _ink, u32 _stroke) {
    if(!_ink->_erase_open) {
        fude_ink_move_end(_ink);
        // Only DEAD strokes can be truncated away here, and the one being erased
        // is alive, so its index stays valid.
        fude_ink_discard_redo(_ink);
        _ink->_erase_open  = true;
        _ink->_erase_first = fude_ink_len(&_ink->targets);
    }

    rde_arr_add(&_ink->targets, &_stroke);
    fude_ink_strokes(_ink)[_stroke].alive = false;
    _ink->revision++;
}

void fude_ink_erase_strokes(fude_ink* _ink, const u32* _ids, u32 _count) {
    if(_ink->drawing) {
        return;
    }

    fude_ink_close_groups(_ink);

    for(u32 _i = 0; _i < _count; _i++) {
        if(_ids[_i] < fude_ink_len(&_ink->strokes) && fude_ink_strokes(_ink)[_ids[_i]].alive) {
            fude_ink_erase_stroke(_ink, _ids[_i]);
        }
    }

    fude_ink_erase_end(_ink);
}

u32 fude_ink_add_strokes(fude_ink* _ink, const fude_ink_stroke* _strokes, u32 _count, const fude_ink_point* _points, rde_vec_2F _offset) {
    if(_ink->drawing || _count == 0) {
        return UINT32_MAX;
    }

    fude_ink_close_groups(_ink);
    fude_ink_discard_redo(_ink);

    const u32 _first = fude_ink_len(&_ink->strokes);

    for(u32 _i = 0; _i < _count; _i++) {
        const fude_ink_stroke* _src = &_strokes[_i];
        if(_src->point_count == 0) {
            continue;
        }

        fude_ink_stroke _stroke = {
            .first_point = fude_ink_len(&_ink->points),
            .point_count = _src->point_count,
            .color       = _src->color,
            .from_pen    = _src->from_pen,
            .eraser      = _src->eraser,
            .alive       = true,
            .marker      = _src->marker,
        };

        fude_ink_point* _dst = rde_arr_add_n(&_ink->points, _src->point_count);
        memcpy(_dst, &_points[_src->first_point], (usize)_src->point_count * sizeof(fude_ink_point));
        for(u32 _p = 0; _p < _src->point_count; _p++) {
            _dst[_p].position.x += _offset.x;
            _dst[_p].position.y += _offset.y;
        }

        rde_arr_add(&_ink->strokes, &_stroke);
        fude_ink_recompute_bounds(_ink, &fude_ink_strokes(_ink)[fude_ink_len(&_ink->strokes) - 1]);
    }

    const u32 _added = fude_ink_len(&_ink->strokes) - _first;
    if(_added == 0) {
        return UINT32_MAX;
    }

    fude_ink_push_action(_ink, FUDE_INK_ACTION_WRITE, _first, _added, (rde_vec_2F){ 0.0f, 0.0f });
    _ink->revision++;
    return _first;
}

void fude_ink_move_begin(fude_ink* _ink, const u32* _ids, u32 _count) {
    if(_ink->drawing) {
        return;
    }

    fude_ink_close_groups(_ink);
    // Only DEAD strokes can be truncated here; the ones moving are alive.
    fude_ink_discard_redo(_ink);

    _ink->_move_open  = true;
    _ink->_move_first = fude_ink_len(&_ink->targets);
    _ink->_move_delta = (rde_vec_2F){ 0.0f, 0.0f };

    for(u32 _i = 0; _i < _count; _i++) {
        u32 _id = _ids[_i];
        if(_id < fude_ink_len(&_ink->strokes) && fude_ink_strokes(_ink)[_id].alive) {
            rde_arr_add(&_ink->targets, &_id);
        }
    }
}

void fude_ink_move_by(fude_ink* _ink, rde_vec_2F _delta) {
    if(!_ink->_move_open) {
        return;
    }

    fude_ink_translate(_ink, _ink->_move_first, fude_ink_len(&_ink->targets) - _ink->_move_first, _delta);
    _ink->_move_delta.x += _delta.x;
    _ink->_move_delta.y += _delta.y;
}

void fude_ink_move_end(fude_ink* _ink) {
    if(!_ink->_move_open) {
        return;
    }

    _ink->_move_open = false;
    const u32 _count = fude_ink_len(&_ink->targets) - _ink->_move_first;

    // Put down where it was picked up: nothing happened.
    if(_count == 0 || !(fabsf(_ink->_move_delta.x) > 0.0f || fabsf(_ink->_move_delta.y) > 0.0f)) {
        fude_ink_truncate(&_ink->targets, _ink->_move_first);
        return;
    }

    fude_ink_push_action(_ink, FUDE_INK_ACTION_MOVE, _ink->_move_first, _count, _ink->_move_delta);
}

void fude_ink_erase_end(fude_ink* _ink) {
    if(!_ink->_erase_open) {
        return;
    }

    _ink->_erase_open = false;
    fude_ink_push_action(_ink, FUDE_INK_ACTION_ERASE, _ink->_erase_first, fude_ink_len(&_ink->targets) - _ink->_erase_first, (rde_vec_2F){ 0.0f, 0.0f });
}

// Screen units → the canvas units points are stored in.
RDE_INTERNAL f32 fude_ink_to_canvas_units(const fude_ink* _ink, f32 _screen_units) {
    return _ink->zoom > 0.0f ? _screen_units / _ink->zoom : _screen_units;
}

// A brush WIDTH → the canvas units it is stored in. PAGE widths already are;
// SCREEN widths are held constant on screen, so they shrink by the zoom.
RDE_INTERNAL f32 fude_ink_width_to_canvas(const fude_ink* _ink, f32 _radius) {
    return _ink->brush_scale == FUDE_INK_BRUSH_SCALE_SCREEN ? fude_ink_to_canvas_units(_ink, _radius) : _radius;
}

void fude_ink_clear(fude_ink* _ink) {
    // The STROKES go; the pen state stays. Clearing the canvas is not the pen
    // lifting, and zeroing pressure here would make the next stroke start
    // hairline until the first axis event caught up.
    fude_ink_end(_ink);
    fude_ink_close_groups(_ink);

    for(u32 _s = 0; _s < fude_ink_len(&_ink->strokes); _s++) {
        if(fude_ink_strokes(_ink)[_s].alive) {
            fude_ink_erase_stroke(_ink, _s);
        }
    }

    fude_ink_erase_end(_ink);   // one action: one undo brings the whole page back
    _ink->stale_ms_max = 0.0f;
}

RDE_INTERNAL f32 fude_ink_radius_from_pressure(f32 _pressure) {
    return FUDE_INK_WIDTH_BASE + FUDE_INK_WIDTH_PRESSURE * _pressure;
}

f32 fude_ink_pen_radius(const fude_ink* _ink, f32 _pressure) {
    const f32 _radius = _ink->width_mode == FUDE_INK_WIDTH_MODE_PRESSURE ? fude_ink_radius_from_pressure(_pressure) : _ink->constant_radius;
    return fude_ink_width_to_canvas(_ink, _radius);
}

RDE_INTERNAL fude_ink_stroke* fude_ink_open_pen_stroke(fude_ink* _ink) {
    if(!_ink->drawing || fude_ink_len(&_ink->strokes) == 0) {
        return NULL;
    }

    fude_ink_stroke* _stroke = &fude_ink_strokes(_ink)[fude_ink_len(&_ink->strokes) - 1];
    return _stroke->from_pen ? _stroke : NULL;
}

// Only readings taken WHILE TOUCHING count: SDL also reports the pressure axis
// just before the down and just after the up, and those would fake a spread.
RDE_INTERNAL void fude_ink_track_pressure_range(fude_ink* _ink, f32 _raw) {
    if(_ink->pressure_live || fude_ink_open_pen_stroke(_ink) == NULL) {
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

    if(_ink->_pressure_seen_max - _ink->_pressure_seen_min > FUDE_INK_PRESSURE_LIVE_RANGE) {
        _ink->pressure_live = true;

        // In pressure mode, the stroke that proved it started on simulated widths;
        // redraw it from the real pressure it recorded all along, so it has no seam.
        if(_ink->width_mode == FUDE_INK_WIDTH_MODE_PRESSURE) {
            const fude_ink_stroke* _stroke = fude_ink_open_pen_stroke(_ink);
            fude_ink_point*        _points = &fude_ink_points(_ink)[_stroke->first_point];
            for(u32 _p = 0; _p < _stroke->point_count; _p++) {
                _points[_p].radius = fude_ink_width_to_canvas(_ink, fude_ink_radius_from_pressure(_points[_p].pressure));
            }
            fude_ink_recompute_bounds(_ink, fude_ink_open_pen_stroke(_ink));
        }
    }
}

// Width from speed, for a pen with no pressure. Speed from the EVENT times,
// between accepted samples, eased so a single fast or slow sample can't twitch it.
RDE_INTERNAL f32 fude_ink_simulated_pressure(fude_ink* _ink, const fude_ink_stroke* _stroke, rde_vec_2F _position) {
    if(_stroke->point_count == 0) {
        _ink->_sim_pressure = FUDE_INK_SIM_START;
    } else {
        const f64 _dt = _ink->sample_time - _ink->_sim_last_time;

        if(_dt > 1e-5) {
            const rde_vec_2F _last  = fude_ink_stroke_points(_ink, _stroke)[_stroke->point_count - 1].position;
            const f32        _dx    = _position.x - _last.x;
            const f32        _dy    = _position.y - _last.y;
            // Canvas distance back to screen units: the feel is the hand's speed.
            const f32        _speed = (f32)((f64)(sqrtf(_dx * _dx + _dy * _dy) * _ink->zoom) / _dt);
            const f32        _slow  = 1.0f - rde_math_clamp_f32(_speed / FUDE_INK_SIM_FAST_SPEED, 0.0f, 1.0f);
            const f32        _target = FUDE_INK_SIM_MIN + (FUDE_INK_SIM_MAX - FUDE_INK_SIM_MIN) * _slow;

            _ink->_sim_pressure += (_target - _ink->_sim_pressure) * FUDE_INK_SIM_SMOOTH;
        }
    }

    _ink->_sim_last_time = _ink->sample_time;
    return _ink->_sim_pressure;
}

void fude_ink_pen_axis(fude_ink* _ink, const rde_event_pen* _pen) {
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
        fude_ink_track_pressure_range(_ink, _pen->pressure);

        // Smoothed, because raw pressure jitters by a few percent sample to
        // sample and an unsmoothed width makes the stroke visibly lumpy.
        _ink->pressure += (_pen->pressure - _ink->pressure) * FUDE_INK_PRESSURE_SMOOTH;
    } else if(_pen->axis == 1) {
        _ink->tilt.x = _pen->tilt.x;
    } else if(_pen->axis == 2) {
        _ink->tilt.y = _pen->tilt.y;
    }
}

RDE_INTERNAL void fude_ink_push_point(fude_ink* _ink, rde_vec_2F _position) {
    if(fude_ink_len(&_ink->strokes) == 0) {
        return;
    }

    // The open stroke is always the LAST, so its points stay contiguous at the
    // tail. The slot is added first (the array may move); the stroke's count only
    // grows once it is filled, so the reads below still see the previous point.
    fude_ink_point*  _point  = rde_arr_add(&_ink->points, NULL);
    fude_ink_stroke* _stroke = &fude_ink_strokes(_ink)[fude_ink_len(&_ink->strokes) - 1];

    f32 _radius = _ink->constant_radius;

    if(_ink->width_mode == FUDE_INK_WIDTH_MODE_PRESSURE && !_stroke->marker) {
        // A pen without real pressure takes its width from speed. Mouse and touch
        // strokes keep the pressure begin() gave them (zero: the base width).
        const f32 _width_pressure = (_stroke->from_pen && !_ink->pressure_live)
                                  ? fude_ink_simulated_pressure(_ink, _stroke, _position)
                                  : _ink->pressure;
        _radius = fude_ink_radius_from_pressure(_width_pressure);
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
    _point->radius   = _stroke->marker ? _ink->marker_radius : fude_ink_width_to_canvas(_ink, _radius);   // the marker: one width on the page
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

void fude_ink_begin(fude_ink* _ink, rde_vec_2F _position, b8 _from_pen, b8 _eraser) {
    if(_ink->drawing) {
        // A second down without an up. Close the old one rather than merging:
        // two marks the user made separately must stay two strokes, or the
        // stroke COUNT — the first thing scoring checks — is wrong.
        fude_ink_end(_ink);
    }

    fude_ink_close_groups(_ink);
    fude_ink_discard_redo(_ink);

    fude_ink_stroke _stroke = {
        .first_point = fude_ink_len(&_ink->points),
        .point_count = 0,
        .color       = _ink->marking && !_eraser ? _ink->marker_color : _ink->color,
        .from_pen    = _from_pen,
        .eraser      = _eraser,
        .alive       = true,
        .marker      = _ink->marking && !_eraser,
    };
    rde_arr_add(&_ink->strokes, &_stroke);
    const u32 _index = fude_ink_len(&_ink->strokes) - 1;
    fude_ink_push_action(_ink, FUDE_INK_ACTION_WRITE, _index, 1u, (rde_vec_2F){ 0.0f, 0.0f });
    _ink->revision++;

    _ink->drawing = true;

    if(_from_pen) {
        _ink->pen_seen = true;
    } else {
        // A touch carries no pressure worth having (SDL reports 1.0 on most
        // panels), so the stroke draws at the base width instead of maximum.
        _ink->pressure = 0.0f;
    }

    fude_ink_push_point(_ink, _position);
}

void fude_ink_extend(fude_ink* _ink, rde_vec_2F _position) {
    if(!_ink->drawing || fude_ink_len(&_ink->strokes) == 0) {
        return;
    }

    const fude_ink_stroke* _stroke = &fude_ink_strokes(_ink)[fude_ink_len(&_ink->strokes) - 1];

    if(_stroke->point_count > 0) {
        const rde_vec_2F _last = fude_ink_stroke_points(_ink, _stroke)[_stroke->point_count - 1].position;
        const f32 _dx = _position.x - _last.x;
        const f32 _dy = _position.y - _last.y;

        // Coincident samples make the segment direction undefined, and
        // normalising a zero vector is a NaN that spreads through the whole
        // quad. Dropping them costs nothing: they carry no new shape.
        const f32 _min_step = fude_ink_to_canvas_units(_ink, FUDE_INK_MIN_STEP_PX);
        if((_dx * _dx + _dy * _dy) < (_min_step * _min_step)) {
            return;
        }
    }

    fude_ink_push_point(_ink, _position);
}

void fude_ink_end(fude_ink* _ink) {
    if(!_ink->drawing) {
        return;
    }

    _ink->drawing = false;

    // A stroke with a single point is a tap, not a mark. Kept anyway — in
    // Japanese it may well be a legitimate short stroke, and the renderer stamps
    // a dot for it.
}

// Squared distance from _p to the segment _a-_b.
RDE_INTERNAL f32 fude_ink_dist2_to_segment(rde_vec_2F _p, rde_vec_2F _a, rde_vec_2F _b) {
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

RDE_INTERNAL b8 fude_ink_stroke_touches(const fude_ink* _ink, const fude_ink_stroke* _stroke, rde_vec_2F _point, f32 _radius) {
    const fude_ink_point* _points = fude_ink_stroke_points(_ink, _stroke);

    for(u32 _p = 0; _p < _stroke->point_count; _p++) {
        const fude_ink_point* _a = &_points[_p];
        const fude_ink_point* _b = &_points[_p + 1 < _stroke->point_count ? _p + 1 : _p];
        const f32 _reach = _radius + (_a->radius > _b->radius ? _a->radius : _b->radius);

        if(fude_ink_dist2_to_segment(_point, _a->position, _b->position) <= _reach * _reach) {
            return true;
        }
    }

    return false;
}

// Cheap reject before the per-segment test.
RDE_INTERNAL b8 fude_ink_near_bounds(const fude_ink_stroke* _stroke, rde_vec_2F _point, f32 _radius) {
    return _point.x >= _stroke->bounds_min.x - _radius && _point.x <= _stroke->bounds_max.x + _radius &&
           _point.y >= _stroke->bounds_min.y - _radius && _point.y <= _stroke->bounds_max.y + _radius;
}

// How far _point is from the stroke's INK — its edge, not its centre line.
// Negative when it is on the ink.
RDE_INTERNAL f32 fude_ink_stroke_gap(const fude_ink* _ink, const fude_ink_stroke* _stroke, rde_vec_2F _point) {
    const fude_ink_point* _points = fude_ink_stroke_points(_ink, _stroke);
    f32                   _gap    = 1e30f;

    for(u32 _p = 0; _p < _stroke->point_count; _p++) {
        const fude_ink_point* _a = &_points[_p];
        const fude_ink_point* _b = &_points[_p + 1 < _stroke->point_count ? _p + 1 : _p];
        const f32 _g = sqrtf(fude_ink_dist2_to_segment(_point, _a->position, _b->position)) - (_a->radius > _b->radius ? _a->radius : _b->radius);
        _gap = _g < _gap ? _g : _gap;
    }

    return _gap;
}

u32 fude_ink_pick(const fude_ink* _ink, rde_vec_2F _point, f32 _radius) {
    const fude_ink_stroke* _strokes = fude_ink_strokes(_ink);
    u32                    _best    = UINT32_MAX;
    f32                    _best_gap = _radius;

    // The NEAREST ink within reach — a tap between two strokes means the one it
    // is closer to. Newest first, so on a tie (overlapping ink) the one drawn on
    // top wins.
    for(u32 _s = fude_ink_len(&_ink->strokes); _s-- > 0;) {
        if(!_strokes[_s].alive || !fude_ink_near_bounds(&_strokes[_s], _point, _radius)) {
            continue;
        }

        const f32 _gap = fude_ink_stroke_gap(_ink, &_strokes[_s], _point);
        if(_gap <= _best_gap && (_best == UINT32_MAX || _gap < _best_gap)) {
            _best     = _s;
            _best_gap = _gap;
        }
    }

    return _best;
}

u32 fude_ink_erase_at(fude_ink* _ink, rde_vec_2F _point, f32 _radius) {
    if(_ink->drawing) {
        return 0;
    }

    u32 _erased = 0;

    // Marked dead, never moved: order is preserved (it is the stroke ORDER scoring
    // reads) and the erase stays undoable.
    for(u32 _s = 0; _s < fude_ink_len(&_ink->strokes); _s++) {
        const fude_ink_stroke* _stroke = &fude_ink_strokes(_ink)[_s];
        if(_stroke->alive && fude_ink_near_bounds(_stroke, _point, _radius) && fude_ink_stroke_touches(_ink, _stroke, _point, _radius)) {
            fude_ink_erase_stroke(_ink, _s);
            _erased++;
        }
    }

    return _erased;
}

void fude_ink_frame_begin(fude_ink* _ink) {
    _ink->samples_this_frame = 0;
}

u32 fude_ink_alive_strokes(const fude_ink* _ink) {
    const fude_ink_stroke* _strokes = fude_ink_strokes(_ink);
    u32                    _alive   = 0;

    for(u32 _i = 0; _i < fude_ink_len(&_ink->strokes); _i++) {
        _alive += _strokes[_i].alive ? 1u : 0u;
    }

    return _alive;
}

u32 fude_ink_total_points(const fude_ink* _ink) {
    const fude_ink_stroke* _strokes = fude_ink_strokes(_ink);
    u32                    _total   = 0;

    for(u32 _i = 0; _i < fude_ink_len(&_ink->strokes); _i++) {
        _total += _strokes[_i].alive ? _strokes[_i].point_count : 0u;
    }

    return _total;
}

RDE_INTERNAL f32 fude_ink_width_at(const fude_ink_point* _point) {
    return _point->radius;
}

// Fills the scratch with one stroke in screen space (widths + _extra_px) and
// draws it as ONE antialiased shape: round caps and joins, a one-pixel fringe on
// the outline. It used to be a disc per sample plus a quad per segment:
// hard-edged, and it can't simply be softened — every piece would bring its own
// fringe and they'd stack into beads at each joint. Returns the positions.
RDE_INTERNAL const rde_vec_2F* fude_ink_emit_stroke(fude_ink* _ink, const fude_ink_stroke* _stroke, rde_vec_2F _offset, f32 _zoom, f32 _extra_px, rde_color _color) {
    rde_arr_clear(&_ink->_scratch_positions);
    rde_arr_clear(&_ink->_scratch_radii);
    rde_vec_2F* _positions = rde_arr_add_n(&_ink->_scratch_positions, _stroke->point_count);
    f32*        _radii     = rde_arr_add_n(&_ink->_scratch_radii,     _stroke->point_count);

    const fude_ink_point* _points = fude_ink_stroke_points(_ink, _stroke);
    u32                   _n      = 0;
    rde_vec_2F            _sum    = { 0.0f, 0.0f };   // the marker: the samples since the point drawn last...
    u32                   _summed = 0;               // ...how many

    for(u32 _p = 0; _p < _stroke->point_count; _p++) {
        const fude_ink_point* _point = &_points[_p];

        // Canvas → screen: points and widths scale with the zoom, like ink on
        // a page under a magnifier.
        // (As drawn: in a right-to-left screen, a kept box moves it — draw.h.)
        const rde_vec_2F _at = fude_draw_at((rde_vec_2F){ _point->position.x * _zoom + _offset.x, _point->position.y * _zoom + _offset.y });
        const f32        _r  = rde_math_clamp_f32(fude_ink_width_at(_point) * _zoom, FUDE_INK_MIN_SCREEN_RADIUS, 1e6f) + _extra_px;

        // The marker is see-through: where its outline folds over itself it
        // shows twice as dark — and a slow hand's samples, close together and
        // jittering, fold it at every one. So only points at least a good part
        // of its width apart are drawn, each the middle of the samples since the
        // one before (the hand's tremor evened out); the last as it is: the
        // stroke ends where the pen lifted.
        rde_vec_2F _draw = _at;
        if(_stroke->marker && _n > 0u) {
            const b8  _end  = _p + 1u == _stroke->point_count;
            const f32 _dx   = _at.x - _positions[_n - 1u].x;
            const f32 _dy   = _at.y - _positions[_n - 1u].y;
            const f32 _step = _r * FUDE_INK_MARKER_STEP;
            _sum.x += _at.x;
            _sum.y += _at.y;
            _summed++;
            if(_dx * _dx + _dy * _dy < _step * _step) {
                if(_end && _n > 1u) {
                    _positions[_n - 1u] = _at;   // the end, in place of the point just before it
                    _radii[_n - 1u]     = _r;
                }
                continue;
            }
            if(!_end) {
                _draw = (rde_vec_2F){ _sum.x / (f32)_summed, _sum.y / (f32)_summed };
            }
            _sum    = (rde_vec_2F){ 0.0f, 0.0f };
            _summed = 0;
        }
        _positions[_n] = _draw;
        _radii[_n]     = _r;
        _n++;
    }

    rde_rendering_2d_draw_stroke(_positions, _radii, _n, _color);
    return _positions;
}

void fude_ink_draw_stroke(fude_ink* _ink, u32 _index, rde_vec_2F _offset, f32 _zoom, f32 _extra_px, rde_color _color) {
    if(_index >= fude_ink_len(&_ink->strokes)) {
        return;
    }

    const fude_ink_stroke* _stroke = &fude_ink_strokes(_ink)[_index];
    if(_stroke->alive && _stroke->point_count > 0) {
        fude_ink_emit_stroke(_ink, _stroke, _offset, _zoom, _extra_px, _color);
    }
}

void fude_ink_render(fude_ink* _ink, rde_vec_2F _offset, f32 _zoom, rde_vec_2F _screen_half, f64 _now, b8 _show_samples) {
    // The canvas rect on screen, padded by the thinnest drawn ink: a stroke whose
    // bounds miss it is not drawn at all, so a big page costs what is visible.
    const f32        _pad     = (FUDE_INK_MIN_SCREEN_RADIUS + 2.0f) / _zoom;
    const rde_vec_2F _vis_min = { (-_screen_half.x - _offset.x) / _zoom - _pad, (-_screen_half.y - _offset.y) / _zoom - _pad };
    const rde_vec_2F _vis_max = { ( _screen_half.x - _offset.x) / _zoom + _pad, ( _screen_half.y - _offset.y) / _zoom + _pad };

    const fude_ink_stroke* _strokes = fude_ink_strokes(_ink);

    // The marker's strokes first, under the rest (a pen's ink stays crisp over them).
    for(u32 _pass = 0; _pass < 2u; _pass++)
    for(u32 _s = 0; _s < fude_ink_len(&_ink->strokes); _s++) {
        const fude_ink_stroke* _stroke = &_strokes[_s];

        if(_stroke->marker != (_pass == 0u) || !_stroke->alive || _stroke->point_count == 0 ||
           _stroke->bounds_max.x < _vis_min.x || _stroke->bounds_min.x > _vis_max.x ||
           _stroke->bounds_max.y < _vis_min.y || _stroke->bounds_min.y > _vis_max.y) {
            continue;
        }

        const rde_color   _color     = _stroke->eraser ? fude_theme_active()->eraser : fude_theme_resolve(_stroke->color);
        const rde_vec_2F* _positions = fude_ink_emit_stroke(_ink, _stroke, _offset, _zoom, 0.0f, _color);

        // Every raw sample as a dot. THIS IS THE DIAGNOSTIC THAT MATTERS: draw a
        // fast stroke and look at the spacing. Evenly spaced dots mean the pen's
        // full rate is arriving; long gaps on the fast parts mean samples are
        // being coalesced to the frame rate and quick strokes will be polygons.
        if(_show_samples) {
            for(u32 _p = 0; _p < _stroke->point_count; _p++) {
                rde_rendering_2d_draw_circle(_positions[_p], 1.5f, 6u, fude_theme_active()->samples, NULL);
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
