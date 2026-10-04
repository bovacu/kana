// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/erase.h"
#include "zoom/fill.h"

#include <math.h>
#include <string.h>

// ===========================================================================
// See erase.h. A stroke's centreline is walked by one parameter u, from 0 at
// its first point to count−1 at its last: point i at u = i, and between points
// i and i+1 the straight segment, at u = i + t.
// ===========================================================================

#define FUDE_ZOOM_ERASE_SAMPLES 1024u   // samples a segment, at most
#define FUDE_ZOOM_ERASE_BISECT  12u     // halvings to find where a cut is

void fude_zoom_eraser_init(fude_zoom_eraser* _e) {
    memset(_e, 0, sizeof(*_e));
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _e->died   = rde_arr_new(sizeof(u32), _heap);
    _e->born   = rde_arr_new(sizeof(u32), _heap);
    _e->found  = rde_arr_new(sizeof(u32), _heap);
    _e->points = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
    _e->fills  = rde_arr_new(sizeof(fude_zoom_erase_fill), _heap);
}

// The sweep's fills let go (their copies and paths).
RDE_INTERNAL void fude_zoom_erase_fills_clear(fude_zoom_eraser* _e) {
    if(!rde_arr_is_inited(&_e->fills)) {
        return;
    }
    fude_zoom_erase_fill* _f = (fude_zoom_erase_fill*)_e->fills.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_e->fills); _i++) {
        rde_arr_free(&_f[_i].points);
        rde_arr_free(&_f[_i].path);
    }
    rde_arr_clear(&_e->fills);
}

void fude_zoom_eraser_destroy(fude_zoom_eraser* _e) {
    fude_zoom_erase_fills_clear(_e);
    rde_arr* _arrays[] = { &_e->died, &_e->born, &_e->found, &_e->points, &_e->fills };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    memset(_e, 0, sizeof(*_e));
}

void fude_zoom_erase_begin(fude_zoom_eraser* _e, FUDE_ZOOM_ERASE_ _mode) {
    _e->mode = (u8)_mode;
    _e->open = true;
    rde_arr_clear(&_e->died);
    rde_arr_clear(&_e->born);
    fude_zoom_erase_fills_clear(_e);
}

// --- geometry ----------------------------------------------------------------------------

RDE_INTERNAL f64 fude_zoom_dist2_point_segment(fude_zoom_v2 _p, fude_zoom_v2 _a, fude_zoom_v2 _b) {
    const f64 _dx = _b.x - _a.x, _dy = _b.y - _a.y;
    const f64 _len2 = _dx * _dx + _dy * _dy;
    f64 _t = _len2 > 0.0 ? ((_p.x - _a.x) * _dx + (_p.y - _a.y) * _dy) / _len2 : 0.0;
    _t = _t < 0.0 ? 0.0 : (_t > 1.0 ? 1.0 : _t);
    const f64 _ex = _a.x + _dx * _t - _p.x, _ey = _a.y + _dy * _t - _p.y;
    return _ex * _ex + _ey * _ey;
}

RDE_INTERNAL f64 fude_zoom_cross(fude_zoom_v2 _o, fude_zoom_v2 _a, fude_zoom_v2 _b) {
    return (_a.x - _o.x) * (_b.y - _o.y) - (_a.y - _o.y) * (_b.x - _o.x);
}

// Where segment p0-p1 crosses q0-q1, as the share along p (false: they don't).
RDE_INTERNAL b8 fude_zoom_segments_cross(fude_zoom_v2 _p0, fude_zoom_v2 _p1, fude_zoom_v2 _q0, fude_zoom_v2 _q1, f64* _t) {
    const f64 _rx = _p1.x - _p0.x, _ry = _p1.y - _p0.y;
    const f64 _sx = _q1.x - _q0.x, _sy = _q1.y - _q0.y;
    const f64 _den = _rx * _sy - _ry * _sx;
    if(fabs(_den) <= 1e-300) {
        return false;
    }
    const f64 _qx = _q0.x - _p0.x, _qy = _q0.y - _p0.y;
    const f64 _tp = (_qx * _sy - _qy * _sx) / _den;
    const f64 _tq = (_qx * _ry - _qy * _rx) / _den;
    if(_tp < 0.0 || _tp > 1.0 || _tq < 0.0 || _tq > 1.0) {
        return false;
    }
    *_t = _tp;
    return true;
}

RDE_INTERNAL f64 fude_zoom_dist2_segments(fude_zoom_v2 _p0, fude_zoom_v2 _p1, fude_zoom_v2 _q0, fude_zoom_v2 _q1) {
    f64 _t;
    if(fude_zoom_segments_cross(_p0, _p1, _q0, _q1, &_t)) {
        return 0.0;
    }
    f64 _d = fude_zoom_dist2_point_segment(_p0, _q0, _q1);
    f64 _e = fude_zoom_dist2_point_segment(_p1, _q0, _q1);
    _d = _e < _d ? _e : _d;
    _e = fude_zoom_dist2_point_segment(_q0, _p0, _p1);
    _d = _e < _d ? _e : _d;
    _e = fude_zoom_dist2_point_segment(_q1, _p0, _p1);
    return _e < _d ? _e : _d;
}

// --- a stroke, walked -----------------------------------------------------------------------

typedef struct {
    const fude_zoom_object* o;
    const fude_zoom_qpoint* q;
    u32                     n;
} fude_zoom_walk;

RDE_INTERNAL fude_zoom_v2 fude_zoom_walk_at(const fude_zoom_walk* _w, f64 _u, f32* _radius, f32* _pressure, f32* _time) {
    if(_w->n == 1u) {
        if(_radius != NULL) { *_radius = fude_zoom_scene_radius_at(_w->o, &_w->q[0]); }
        if(_pressure != NULL) { *_pressure = (f32)_w->q[0].pressure; }
        if(_time != NULL) { *_time = (f32)_w->q[0].time; }
        return fude_zoom_scene_point_at(_w->o, &_w->q[0]);
    }
    u32 _i = (u32)floor(_u);
    if(_i > _w->n - 2u) {
        _i = _w->n - 2u;
    }
    const f64          _t = _u - (f64)_i;
    const fude_zoom_v2 _a = fude_zoom_scene_point_at(_w->o, &_w->q[_i]);
    const fude_zoom_v2 _b = fude_zoom_scene_point_at(_w->o, &_w->q[_i + 1u]);
    if(_radius != NULL) {
        const f32 _ra = fude_zoom_scene_radius_at(_w->o, &_w->q[_i]);
        const f32 _rb = fude_zoom_scene_radius_at(_w->o, &_w->q[_i + 1u]);
        *_radius = _ra + (_rb - _ra) * (f32)_t;
    }
    if(_pressure != NULL) {
        *_pressure = (f32)_w->q[_i].pressure + ((f32)_w->q[_i + 1u].pressure - (f32)_w->q[_i].pressure) * (f32)_t;
    }
    if(_time != NULL) {
        *_time = (f32)_w->q[_i].time + ((f32)_w->q[_i + 1u].time - (f32)_w->q[_i].time) * (f32)_t;
    }
    return (fude_zoom_v2){ _a.x + (_b.x - _a.x) * _t, _a.y + (_b.y - _a.y) * _t };
}

RDE_INTERNAL b8 fude_zoom_walk_erased(const fude_zoom_walk* _w, f64 _u, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _r) {
    f32 _radius;
    const fude_zoom_v2 _p     = fude_zoom_walk_at(_w, _u, &_radius, NULL, NULL);
    const f64          _reach = _r + (f64)_radius;
    return fude_zoom_dist2_point_segment(_p, _a, _b) < _reach * _reach;
}

// Does the eraser (a-b, radius _r) touch the stroke anywhere?
RDE_INTERNAL b8 fude_zoom_walk_touched(const fude_zoom_walk* _w, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _r) {
    for(u32 _i = 0; _i < _w->n; _i++) {
        const fude_zoom_v2 _p0 = fude_zoom_scene_point_at(_w->o, &_w->q[_i]);
        const fude_zoom_v2 _p1 = _i + 1u < _w->n ? fude_zoom_scene_point_at(_w->o, &_w->q[_i + 1u]) : _p0;
        f64 _reach = _r + (f64)fude_zoom_scene_radius_at(_w->o, &_w->q[_i]);
        if(_i + 1u < _w->n) {
            const f64 _r1 = _r + (f64)fude_zoom_scene_radius_at(_w->o, &_w->q[_i + 1u]);
            _reach = _r1 > _reach ? _r1 : _reach;
        }
        if(fude_zoom_dist2_segments(_p0, _p1, _a, _b) < _reach * _reach) {
            return true;
        }
    }
    return false;
}

RDE_INTERNAL f64 fude_zoom_walk_length(const fude_zoom_walk* _w, f64 _u0, f64 _u1) {
    f64          _len  = 0.0;
    fude_zoom_v2 _prev = fude_zoom_walk_at(_w, _u0, NULL, NULL, NULL);
    for(u32 _i = (u32)floor(_u0) + 1u; (f64)_i < _u1; _i++) {
        const fude_zoom_v2 _p = fude_zoom_scene_point_at(_w->o, &_w->q[_i]);
        _len += hypot(_p.x - _prev.x, _p.y - _prev.y);
        _prev = _p;
    }
    const fude_zoom_v2 _end = fude_zoom_walk_at(_w, _u1, NULL, NULL, NULL);
    return _len + hypot(_end.x - _prev.x, _end.y - _prev.y);
}

// --- pieces ----------------------------------------------------------------------------------

// A point of the stroke as drawn (before its rotation and scale), at u.
RDE_INTERNAL fude_zoom_v2 fude_zoom_walk_local_at(const fude_zoom_walk* _w, f64 _u, f32* _pressure, f32* _time) {
    u32 _i = (u32)floor(_u);
    if(_w->n == 1u || _i > _w->n - 2u) {
        _i = _w->n == 1u ? 0u : _w->n - 2u;
    }
    const f64 _t = _w->n == 1u ? 0.0 : _u - (f64)_i;
    const u32 _j = _w->n == 1u ? 0u : _i + 1u;
    const fude_zoom_v2 _a = fude_zoom_scene_local_at(_w->o, &_w->q[_i]);
    const fude_zoom_v2 _b = fude_zoom_scene_local_at(_w->o, &_w->q[_j]);
    *_pressure = (f32)_w->q[_i].pressure + ((f32)_w->q[_j].pressure - (f32)_w->q[_i].pressure) * (f32)_t;
    *_time     = (f32)_w->q[_i].time + ((f32)_w->q[_j].time - (f32)_w->q[_i].time) * (f32)_t;
    return (fude_zoom_v2){ _a.x + (_b.x - _a.x) * _t, _a.y + (_b.y - _a.y) * _t };
}

// The stretch _u0.._u1 of a stroke as a stroke of its own, in the same place
// in the order (right after it), turned and scaled as it is. Its index, or
// FUDE_ZOOM_NONE when it is too short to keep. Built in the stroke's own
// coordinates (as drawn), so its points stay exact integers on its grid; only
// the two cut ends are new (rounded to the grid).
RDE_INTERNAL u32 fude_zoom_erase_piece(fude_zoom_scene* _s, fude_zoom_eraser* _e, const fude_zoom_walk* _w, u32 _object, f64 _u0, f64 _u1, f64 _min_len, u32 _nth) {
    if(_u1 - _u0 <= 0.0 || fude_zoom_walk_length(_w, _u0, _u1) < _min_len) {
        return FUDE_ZOOM_NONE;
    }
    const fude_zoom_object _o    = *_w->o;   // a copy: adding a stroke may move the table
    // Its quanta: the stroke's, or finer where the eraser works finer than they
    // are (a stroke drawn far out, erased deep in: its cut ends where the eraser
    // was, an eighth of a pixel). A power of two finer, so its own points stay
    // whole; never so fine its numbers outgrow 32 bits.
    const f64 _extent = fmax(_o.box.max_x - _o.box.min_x, _o.box.max_y - _o.box.min_y) / (_o.scale > 0.0 ? _o.scale : 1.0);
    i32       _qp     = (i32)floor(log2(fmax(_min_len / (_o.scale > 0.0 ? _o.scale : 1.0), 1e-300) / 8.0));
    _qp = _qp > _o.q ? _o.q : _qp;
    const i32 _qmin = (i32)ceil(log2(fmax(_extent, 1e-300) / 1073741824.0));
    _qp = _qp < _qmin ? _qmin : _qp;
    _qp = _qp > _o.q ? _o.q : _qp;
    const f64 _grid = ldexp(1.0, _qp);

    f32 _p0p, _p0t;
    const fude_zoom_v2 _l0   = fude_zoom_walk_local_at(_w, _u0, &_p0p, &_p0t);
    const fude_zoom_v2 _from = { round(_l0.x / _grid) * _grid, round(_l0.y / _grid) * _grid };

    rde_arr* _pts = &_e->points;
    rde_arr_clear(_pts);
    const u32 _first = (u32)floor(_u0) + 1u;
    for(i32 _k = -1; ; _k++) {
        fude_zoom_v2 _p;
        f32          _pr, _tm;
        b8           _last = false;
        if(_k < 0) {
            _p  = _l0;
            _pr = _p0p;
            _tm = _p0t;
        } else {
            const u32 _i = _first + (u32)_k;
            if((f64)_i >= _u1) {
                _p    = fude_zoom_walk_local_at(_w, _u1, &_pr, &_tm);
                _last = true;
            } else {
                _p  = fude_zoom_scene_local_at(&_o, &_w->q[_i]);
                _pr = (f32)_w->q[_i].pressure;
                _tm = (f32)_w->q[_i].time;
            }
        }
        const fude_zoom_qpoint _q = {
            (i32)llround((_p.x - _from.x) / _grid), (i32)llround((_p.y - _from.y) / _grid),
            (u16)(_pr < 0.0f ? 0.0f : (_pr > 1023.0f ? 1023.0f : _pr + 0.5f)),
            (u32)(_tm - _p0t < 0.0f ? 0.0f : _tm - _p0t + 0.5f),
        };
        rde_arr_add(_pts, (any)&_q);
        if(_last) {
            break;
        }
    }
    fude_zoom_qpoint* _q = (fude_zoom_qpoint*)_pts->memory;
    _q[0].x = 0;
    _q[0].y = 0;
    const u8 _keep = (u8)(_o.flags & (FUDE_ZOOM_FLAG_FROM_PEN | FUDE_ZOOM_FLAG_MARKER | FUDE_ZOOM_FLAG_PRESSURE));
    const fude_zoom_place _at = { fude_zoom_sim_apply(fude_zoom_object_sim(&_o), _from), _o.rotation, _o.scale };
    RDE_UNUSED(_object);
    return fude_zoom_scene_add_stroke_at(_s, _o.frame, _at, (i8)_qp, _q, (u32)rde_arr_length(_pts), _o.channels, _o.color, _o.radius, _keep, 0, _o.z + 1u + _nth);
}

// Object _object goes: made earlier in this sweep, it never was; else it is
// one the sweep erased.
RDE_INTERNAL void fude_zoom_erase_take(fude_zoom_scene* _s, fude_zoom_eraser* _e, u32 _object) {
    u32* _born = (u32*)_e->born.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_e->born); _i++) {
        if(_born[_i] == _object) {
            _born[_i] = _born[rde_arr_length(&_e->born) - 1u];
            _e->born.count--;
            fude_zoom_scene_discard(_s, _object);
            return;
        }
    }
    fude_zoom_scene_set_alive(_s, _object, false);
    rde_arr_add(&_e->died, (any)&_object);
}

// The stretches of a stroke kept (u0, u1 pairs, in order) replace it.
RDE_INTERNAL void fude_zoom_erase_replace(fude_zoom_scene* _s, fude_zoom_eraser* _e, const fude_zoom_walk* _w, u32 _object, const f64* _keep, u32 _pairs, f64 _min_len) {
    u32 _pieces[64];
    u32 _made = 0;
    for(u32 _i = 0; _i < _pairs && _made < 64u; _i++) {
        const u32 _piece = fude_zoom_erase_piece(_s, _e, _w, _object, _keep[_i * 2u], _keep[_i * 2u + 1u], _min_len, _made);
        if(_piece != FUDE_ZOOM_NONE) {
            _pieces[_made++] = _piece;
        }
    }
    fude_zoom_erase_take(_s, _e, _object);
    for(u32 _i = 0; _i < _made; _i++) {
        rde_arr_add(&_e->born, (any)&_pieces[_i]);
    }
}

// --- the modes -------------------------------------------------------------------------------

// PARTIAL: the stretches the disc did not reach, found by sampling the
// centreline finely and halving in on each change between rubbed and kept.
RDE_INTERNAL void fude_zoom_erase_partial(fude_zoom_scene* _s, fude_zoom_eraser* _e, const fude_zoom_walk* _w, u32 _object, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _r, f64 _min_len) {
    if(_w->n == 1u) {
        fude_zoom_erase_take(_s, _e, _object);
        return;
    }
    f64 _keep[128];
    u32 _pairs     = 0;
    b8  _in_keep   = !fude_zoom_walk_erased(_w, 0.0, _a, _b, _r);
    f64 _keep_from = 0.0;
    f64 _prev_u    = 0.0;
    b8  _cut       = !_in_keep;
    for(u32 _i = 0; _i + 1u < _w->n; _i++) {
        const fude_zoom_v2 _p0 = fude_zoom_scene_point_at(_w->o, &_w->q[_i]);
        const fude_zoom_v2 _p1 = fude_zoom_scene_point_at(_w->o, &_w->q[_i + 1u]);
        const f64 _len   = hypot(_p1.x - _p0.x, _p1.y - _p0.y);
        f64       _steps = ceil(_len / (_r * 0.25));
        _steps = _steps < 1.0 ? 1.0 : (_steps > (f64)FUDE_ZOOM_ERASE_SAMPLES ? (f64)FUDE_ZOOM_ERASE_SAMPLES : _steps);
        for(u32 _k = 1; _k <= (u32)_steps; _k++) {
            const f64 _u      = (f64)_i + (f64)_k / _steps;
            const b8  _kept   = !fude_zoom_walk_erased(_w, _u, _a, _b, _r);
            if(_kept != _in_keep) {
                // The change is between _prev_u and _u: halve in on it.
                f64 _lo = _prev_u, _hi = _u;
                for(u32 _h = 0; _h < FUDE_ZOOM_ERASE_BISECT; _h++) {
                    const f64 _mid = (_lo + _hi) * 0.5;
                    if((!fude_zoom_walk_erased(_w, _mid, _a, _b, _r)) == _in_keep) {
                        _lo = _mid;
                    } else {
                        _hi = _mid;
                    }
                }
                if(_in_keep) {
                    if(_pairs < 64u) {
                        _keep[_pairs * 2u]      = _keep_from;
                        _keep[_pairs * 2u + 1u] = _lo;
                        _pairs++;
                    }
                } else {
                    _keep_from = _hi;
                }
                _in_keep = _kept;
                _cut     = true;
            }
            _prev_u = _u;
        }
    }
    if(!_cut) {
        return;   // the disc reached its box, not the stroke
    }
    if(_in_keep && _pairs < 64u) {
        _keep[_pairs * 2u]      = _keep_from;
        _keep[_pairs * 2u + 1u] = (f64)(_w->n - 1u);
        _pairs++;
    }
    fude_zoom_erase_replace(_s, _e, _w, _object, _keep, _pairs, _min_len);
}

// STROKE: the stroke and every alive piece of its gesture, next to it in the
// order they came.
RDE_INTERNAL void fude_zoom_erase_gesture(fude_zoom_scene* _s, fude_zoom_eraser* _e, u32 _object) {
    const fude_zoom_object* _o       = fude_zoom_scene_object(_s, _object);
    const fude_zoom_id      _gesture = _o->gesture;
    const fude_zoom_frame*  _f       = fude_zoom_scene_frame(_s, _o->frame);
    const u32*              _arrival = (const u32*)_f->arrival.memory;
    const u32               _n       = (u32)rde_arr_length(&_f->arrival);
    u32 _lo = _o->file_pos, _hi = _o->file_pos;
    while(_lo > 0 && fude_zoom_scene_object(_s, _arrival[_lo - 1u])->gesture == _gesture) {
        _lo--;
    }
    while(_hi + 1u < _n && fude_zoom_scene_object(_s, _arrival[_hi + 1u])->gesture == _gesture) {
        _hi++;
    }
    for(u32 _i = _lo; _i <= _hi; _i++) {
        const u32 _piece = ((const u32*)fude_zoom_scene_frame(_s, fude_zoom_scene_object(_s, _object)->frame)->arrival.memory)[_i];
        if(fude_zoom_scene_object(_s, _piece)->flags & FUDE_ZOOM_FLAG_ALIVE) {
            fude_zoom_erase_take(_s, _e, _piece);
        }
    }
}

typedef struct {
    f64 u;
} fude_zoom_cut;

// TRIM: the stretch between the crossings nearest the touch.
RDE_INTERNAL void fude_zoom_erase_trim(fude_zoom_scene* _s, fude_zoom_eraser* _e, const fude_zoom_walk* _w, u32 _object, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _r, f64 _min_len) {
    // Where the eraser touches it: the first sample that is rubbed.
    f64 _touch = -1.0;
    for(u32 _i = 0; _i < _w->n && _touch < 0.0; _i++) {
        const u32 _steps = 8u;
        for(u32 _k = 0; _k < (_i + 1u < _w->n ? _steps : 1u); _k++) {
            const f64 _u = (f64)_i + (f64)_k / (f64)_steps;
            if(fude_zoom_walk_erased(_w, _u, _a, _b, _r)) {
                _touch = _u;
                break;
            }
        }
    }
    if(_touch < 0.0) {
        return;
    }

    // Every crossing with another alive stroke of the frame, as u along this one.
    const fude_zoom_object _self   = *_w->o;
    f64                    _before = -1.0;
    f64                    _after  = (f64)_w->n;   // past the end: none
    rde_arr _others = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_scene_query(_s, _self.frame, _self.box, &_others);
    rde_arr _theirs = rde_arr_new(sizeof(fude_zoom_qpoint), rde_memory_allocator_get_default_std());
    for(u32 _k = 0; _k < (u32)rde_arr_length(&_others); _k++) {
        const u32 _other = ((const u32*)_others.memory)[_k];
        const fude_zoom_object* _oo = fude_zoom_scene_object(_s, _other);
        if(_other == _object || _oo->kind != FUDE_ZOOM_KIND_STROKE || _oo->count < 2u) {
            continue;
        }
        rde_arr_clear(&_theirs);
        rde_arr_add_n(&_theirs, _oo->count);
        if(!fude_zoom_scene_points(_s, _other, (fude_zoom_qpoint*)_theirs.memory)) {
            continue;
        }
        const fude_zoom_qpoint* _tq = (const fude_zoom_qpoint*)_theirs.memory;
        for(u32 _i = 0; _i + 1u < _w->n; _i++) {
            const fude_zoom_v2 _p0 = fude_zoom_scene_point_at(&_self, &_w->q[_i]);
            const fude_zoom_v2 _p1 = fude_zoom_scene_point_at(&_self, &_w->q[_i + 1u]);
            const fude_zoom_box _seg = { fmin(_p0.x, _p1.x), fmin(_p0.y, _p1.y), fmax(_p0.x, _p1.x), fmax(_p0.y, _p1.y) };
            if(!fude_zoom_box_overlaps(_seg, _oo->box)) {
                continue;
            }
            for(u32 _j = 0; _j + 1u < _oo->count; _j++) {
                const fude_zoom_v2 _q0 = fude_zoom_scene_point_at(_oo, &_tq[_j]);
                const fude_zoom_v2 _q1 = fude_zoom_scene_point_at(_oo, &_tq[_j + 1u]);
                f64 _t;
                if(!fude_zoom_segments_cross(_p0, _p1, _q0, _q1, &_t)) {
                    continue;
                }
                const f64 _u = (f64)_i + _t;
                if(_u < _touch && _u > _before) {
                    _before = _u;
                }
                if(_u > _touch && _u < _after) {
                    _after = _u;
                }
            }
        }
    }
    rde_arr_free(&_others);
    rde_arr_free(&_theirs);

    f64 _keep[4];
    u32 _pairs = 0;
    if(_before > 0.0) {
        _keep[_pairs * 2u]      = 0.0;
        _keep[_pairs * 2u + 1u] = _before;
        _pairs++;
    }
    if(_after < (f64)(_w->n - 1u)) {
        _keep[_pairs * 2u]      = _after;
        _keep[_pairs * 2u + 1u] = (f64)(_w->n - 1u);
        _pairs++;
    }
    // No crossing at all: _before and _after are past both ends, nothing is kept.
    fude_zoom_erase_replace(_s, _e, _w, _object, _keep, _pairs, _min_len);
}

// Does the eraser (a-b, radius _r) touch a shape — its line, or inside it when filled?
RDE_INTERNAL b8 fude_zoom_erase_shape_touched(const fude_zoom_scene* _s, u32 _object, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _r) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    b8      _closed;
    fude_zoom_scene_shape_outline(_s, _object, 96u, &_pts, &_closed);
    const fude_zoom_v2* _p = (const fude_zoom_v2*)_pts.memory;
    const u32 _k = (u32)rde_arr_length(&_pts);
    const f64 _reach = _r + (f64)_o->radius * _o->scale;
    b8 _hit = false;
    for(u32 _i = 0; _i + 1u < _k + (_closed ? 1u : 0u) && !_hit; _i++) {
        _hit = fude_zoom_dist2_segments(_p[_i], _p[(_i + 1u) % _k], _a, _b) < _reach * _reach;
    }
    if(!_hit && _closed && (_o->flags & FUDE_ZOOM_FLAG_FILLED)) {
        b8 _in = false;
        for(u32 _i = 0, _j = _k - 1u; _i < _k; _j = _i++) {
            if((_p[_i].y > _a.y) != (_p[_j].y > _a.y) && _a.x < (_p[_j].x - _p[_i].x) * (_a.y - _p[_i].y) / (_p[_j].y - _p[_i].y) + _p[_i].x) {
                _in = !_in;
            }
        }
        _hit = _in;
    }
    rde_arr_free(&_pts);
    return _hit;
}

// A FILL's points (each point's ring in _rings) in its frame's units, into _e->points' memory and _pts.
RDE_INTERNAL u32 fude_zoom_erase_fill_points(const fude_zoom_scene* _s, fude_zoom_eraser* _e, u32 _object, rde_arr* _pts, rde_arr* _rings) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    rde_arr_clear(&_e->points);
    rde_arr_add_n(&_e->points, _o->count);
    if(_o->count < 3u || !fude_zoom_scene_points(_s, _object, (fude_zoom_qpoint*)_e->points.memory)) {
        return 0;
    }
    const fude_zoom_qpoint* _q = (const fude_zoom_qpoint*)_e->points.memory;
    rde_arr_clear(_pts);
    rde_arr_clear(_rings);
    fude_zoom_v2* _p = rde_arr_add_n(_pts, _o->count);
    u32*          _r = rde_arr_add_n(_rings, _o->count);
    for(u32 _i = 0; _i < _o->count; _i++) {
        _p[_i] = fude_zoom_scene_point_at(_o, &_q[_i]);
        _r[_i] = _q[_i].time;
    }
    return _o->count;
}

// Does the eraser (a-b, radius _r) reach what is left of a FILL — inside it at
// either end, or near any of its rings' edges?
RDE_INTERNAL b8 fude_zoom_erase_fill_touched(const fude_zoom_scene* _s, fude_zoom_eraser* _e, u32 _object, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _r) {
    rde_arr _pts   = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    rde_arr _rings = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    const u32 _n = fude_zoom_erase_fill_points(_s, _e, _object, &_pts, &_rings);
    const fude_zoom_v2* _p  = (const fude_zoom_v2*)_pts.memory;
    const u32*          _rr = (const u32*)_rings.memory;
    b8 _hit = _n >= 3u && (fude_zoom_fill_inside_rings(_p, _rr, _n, _a) || fude_zoom_fill_inside_rings(_p, _rr, _n, _b));
    for(u32 _start = 0; _start < _n && !_hit;) {
        u32 _end = _start + 1u;
        while(_end < _n && _rr[_end] == _rr[_start]) {
            _end++;
        }
        for(u32 _i = _start; _i < _end && !_hit; _i++) {
            _hit = fude_zoom_dist2_segments(_p[_i], _p[_i + 1u < _end ? _i + 1u : _start], _a, _b) < _r * _r;
        }
        _start = _end;
    }
    rde_arr_free(&_pts);
    rde_arr_free(&_rings);
    return _hit;
}

// PARTIAL on a FILL: the sweep's reach over it — everywhere within the eraser's
// radius of its path so far — cut out of it as it was before the sweep, as one
// ring; the fill made so replaces the one standing for it. Its index
// (FUDE_ZOOM_NONE: it could not).
RDE_INTERNAL u32 fude_zoom_erase_cut_fill(fude_zoom_scene* _s, fude_zoom_eraser* _e, u32 _object, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _r) {
    // This sweep's entry for it (made the first time the sweep reaches it).
    fude_zoom_erase_fill* _f = NULL;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_e->fills); _i++) {
        if(((fude_zoom_erase_fill*)_e->fills.memory)[_i].now == _object) {
            _f = &((fude_zoom_erase_fill*)_e->fills.memory)[_i];
        }
    }
    if(_f == NULL) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
        fude_zoom_erase_fill _n = {
            .now = _object, .place = fude_zoom_scene_place_of(_s, _object), .q = _o->q, .frame = _o->frame, .color = _o->color, .z = _o->z,
            .flags = (u8)(_o->flags & FUDE_ZOOM_FLAG_MARKER),
            .points = rde_arr_new(sizeof(fude_zoom_qpoint), rde_memory_allocator_get_default_std()),
            .path   = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std()),
        };
        rde_arr_add_n(&_n.points, _o->count);
        if(!fude_zoom_scene_points(_s, _object, (fude_zoom_qpoint*)_n.points.memory)) {
            rde_arr_free(&_n.points);
            rde_arr_free(&_n.path);
            return FUDE_ZOOM_NONE;
        }
        for(u32 _i = 0; _i < _o->count; _i++) {
            const u32 _t = ((const fude_zoom_qpoint*)_n.points.memory)[_i].time;
            _n.ring = _t >= _n.ring ? _t + 1u : _n.ring;
        }
        const fude_zoom_sim _back = fude_zoom_sim_inverse(fude_zoom_object_sim(_o));
        _n.reach = _r * fude_zoom_sim_scale(_back);
        // Finer quanta where the eraser works finer than the fill's own (a fill
        // drawn far out, rubbed deep in): its points kept whole, a power of two
        // finer; never past 32-bit numbers.
        const f64 _extent = fmax(_o->box.max_x - _o->box.min_x, _o->box.max_y - _o->box.min_y) / (_o->scale > 0.0 ? _o->scale : 1.0);
        i32 _qn = (i32)floor(log2(fmax(_n.reach, 1e-300) / 24.0));
        const i32 _qmin = (i32)ceil(log2(fmax(_extent, 1e-300) / 1073741824.0));
        _qn = _qn < _qmin ? _qmin : _qn;
        if(_qn < _n.q && _n.q - _qn < 31) {
            const i64 _k = (i64)1 << (_n.q - _qn);
            fude_zoom_qpoint* _pq = (fude_zoom_qpoint*)_n.points.memory;
            for(u32 _i = 0; _i < _o->count; _i++) {
                _pq[_i].x = (i32)((i64)_pq[_i].x * _k);
                _pq[_i].y = (i32)((i64)_pq[_i].y * _k);
            }
            _n.q = (i8)_qn;
        }
        rde_arr_add(&_e->fills, (any)&_n);
        _f = &((fude_zoom_erase_fill*)_e->fills.memory)[rde_arr_length(&_e->fills) - 1u];
    }
    // The path, in its units (each step goes on from where the last ended).
    const fude_zoom_object _now  = *fude_zoom_scene_object(_s, _object);
    const fude_zoom_sim    _back = fude_zoom_sim_inverse(fude_zoom_object_sim(&_now));
    const fude_zoom_v2     _la   = fude_zoom_sim_apply(_back, _a), _lb = fude_zoom_sim_apply(_back, _b);
    const u32 _np = (u32)rde_arr_length(&_f->path);
    if(_np == 0 || hypot(((const fude_zoom_v2*)_f->path.memory)[_np - 1u].x - _la.x, ((const fude_zoom_v2*)_f->path.memory)[_np - 1u].y - _la.y) > 1e-9) {
        rde_arr_add(&_f->path, (any)&_la);
    }
    rde_arr_add(&_f->path, (any)&_lb);
    // The reach as one ring, as quanta.
    rde_arr _ring = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    const u32 _k = fude_zoom_fill_sweep((const fude_zoom_v2*)_f->path.memory, (u32)rde_arr_length(&_f->path), _f->reach, &_ring);
    if(_k < 3u) {
        rde_arr_free(&_ring);
        return FUDE_ZOOM_NONE;
    }
    const u32 _base = (u32)rde_arr_length(&_f->points);
    rde_arr_clear(&_e->points);
    memcpy(rde_arr_add_n(&_e->points, _base + _k), _f->points.memory, (usize)_base * sizeof(fude_zoom_qpoint));
    fude_zoom_qpoint* _q    = &((fude_zoom_qpoint*)_e->points.memory)[_base];
    const f64         _grid = ldexp(1.0, _f->q);
    for(u32 _i = 0; _i < _k; _i++) {
        const fude_zoom_v2 _c = ((const fude_zoom_v2*)_ring.memory)[_i];
        _q[_i] = (fude_zoom_qpoint){ (i32)llround(_c.x / _grid), (i32)llround(_c.y / _grid), 0u, _f->ring };
    }
    rde_arr_free(&_ring);
    const u32 _made = fude_zoom_scene_add_fill_flags(_s, _f->frame, _f->place, _f->q, (const fude_zoom_qpoint*)_e->points.memory, _base + _k, _f->color, _f->flags, _f->z);
    _f->now = _made;
    return _made;
}

// PARTIAL reaching a stroke: it becomes its own outline (fill.h's sweep of its
// points at their widths, the loops it closes round left open), FILLs in its
// colour, place and order — born this sweep and added to _list, so this very
// step cuts them as fills: what the eraser passed over goes, as with any fill,
// whatever the stroke's width on screen. A long stroke is traced a stretch at
// a time (each a fill, overlapping the next where they meet). False: it could
// not be traced (cut as a stroke).
#define FUDE_ZOOM_ERASE_TRACE_CELLS 1500000.0   // a stretch's grid at most (fill.c traces up to 4M)
RDE_INTERNAL b8 fude_zoom_erase_outline(fude_zoom_scene* _s, fude_zoom_eraser* _e, u32 _object, rde_arr* _list) {
    const fude_zoom_object _o = *fude_zoom_scene_object(_s, _object);
    rde_arr_clear(&_e->points);
    rde_arr_add_n(&_e->points, _o.count);
    if(_o.count == 0 || !fude_zoom_scene_points(_s, _object, (fude_zoom_qpoint*)_e->points.memory)) {
        return false;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _pa = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr _wa = rde_arr_new(sizeof(f64), _heap);
    f64     _thin = 1e300, _wide = 0.0;
    for(u32 _i = 0; _i < _o.count; _i++) {
        const fude_zoom_qpoint* _q = &((const fude_zoom_qpoint*)_e->points.memory)[_i];
        const f64 _w = (f64)fude_zoom_scene_local_radius_at(&_o, _q);
        _thin = fmin(_thin, _w);
        _wide = fmax(_wide, _w);
    }
    // Long segments split (so a stretch can always end within its grid).
    const f64 _cell = fmax(_thin, _wide / 4.0) / 3.0;
    if(!(_cell > 0.0)) {
        rde_arr_free(&_pa);
        rde_arr_free(&_wa);
        return false;
    }
    for(u32 _i = 0; _i < _o.count; _i++) {
        const fude_zoom_qpoint* _q = &((const fude_zoom_qpoint*)_e->points.memory)[_i];
        const fude_zoom_v2 _p = fude_zoom_scene_local_at(&_o, _q);
        const f64          _w = (f64)fude_zoom_scene_local_radius_at(&_o, _q);
        if(_i > 0) {
            const fude_zoom_v2 _l = ((const fude_zoom_v2*)_pa.memory)[rde_arr_length(&_pa) - 1u];
            const f64          _lw = ((const f64*)_wa.memory)[rde_arr_length(&_wa) - 1u];
            const u32 _parts = (u32)fmin(4096.0, ceil(hypot(_p.x - _l.x, _p.y - _l.y) / (_cell * 300.0)));
            for(u32 _k = 1; _k < _parts; _k++) {
                const f64 _t = (f64)_k / (f64)_parts;
                const fude_zoom_v2 _m = { _l.x + (_p.x - _l.x) * _t, _l.y + (_p.y - _l.y) * _t };
                const f64          _mw = _lw + (_w - _lw) * _t;
                rde_arr_add(&_pa, (any)&_m);
                rde_arr_add(&_wa, (any)&_mw);
            }
        }
        rde_arr_add(&_pa, (any)&_p);
        rde_arr_add(&_wa, (any)&_w);
    }
    const u32           _n = (u32)rde_arr_length(&_pa);
    const fude_zoom_v2* _p = (const fude_zoom_v2*)_pa.memory;
    const f64*          _w = (const f64*)_wa.memory;
    // Its quanta: a 24th of its thinnest width (whole numbers from its first corner).
    const f64 _extent = fmax(_o.box.max_x - _o.box.min_x, _o.box.max_y - _o.box.min_y) / (_o.scale > 0.0 ? _o.scale : 1.0);
    i32 _qe = (i32)floor(log2(fmax(_thin, 1e-300) / 24.0));
    _qe = _qe > _o.q ? _o.q : _qe;
    const i32 _qmin = (i32)ceil(log2(fmax(_extent + 2.0 * _wide, 1e-300) / 1073741824.0));
    _qe = _qe < _qmin ? _qmin : _qe;
    const f64             _grid  = ldexp(1.0, _qe);
    const fude_zoom_place _place = fude_zoom_scene_place_of(_s, _object);
    const u8              _keep  = (u8)(_o.flags & FUDE_ZOOM_FLAG_MARKER);
    rde_arr _ring  = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr _rings = rde_arr_new(sizeof(u32), _heap);
    rde_arr _made  = rde_arr_new(sizeof(u32), _heap);
    b8      _ok    = true;
    for(u32 _from = 0; _from < _n && _ok;) {
        // The stretch: as far as its grid allows (at least one segment).
        fude_zoom_box _b  = { _p[_from].x, _p[_from].y, _p[_from].x, _p[_from].y };
        u32           _to = _from;
        while(_to + 1u < _n) {
            const fude_zoom_box _nb = fude_zoom_box_union(_b, (fude_zoom_box){ _p[_to + 1u].x, _p[_to + 1u].y, _p[_to + 1u].x, _p[_to + 1u].y });
            const f64 _gw = (_nb.max_x - _nb.min_x + 2.0 * _wide) / _cell + 6.0, _gh = (_nb.max_y - _nb.min_y + 2.0 * _wide) / _cell + 6.0;
            if(_gw * _gh > FUDE_ZOOM_ERASE_TRACE_CELLS && _to > _from) {
                break;
            }
            _b = _nb;
            _to++;
        }
        const u32 _k = fude_zoom_fill_sweep_widths(&_p[_from], &_w[_from], _to - _from + 1u, &_ring, &_rings);
        if(_k < 3u) {
            _ok = false;
            break;
        }
        rde_arr_clear(&_e->points);
        fude_zoom_qpoint* _q = rde_arr_add_n(&_e->points, _k);
        for(u32 _i = 0; _i < _k; _i++) {
            const fude_zoom_v2 _c = ((const fude_zoom_v2*)_ring.memory)[_i];
            _q[_i] = (fude_zoom_qpoint){ (i32)llround(_c.x / _grid), (i32)llround(_c.y / _grid), 0u, ((const u32*)_rings.memory)[_i] };
        }
        const u32 _fill = fude_zoom_scene_add_fill_flags(_s, _o.frame, _place, (i8)_qe, _q, _k, _o.color, _keep, _o.z);
        rde_arr_add(&_made, (any)&_fill);
        if(_to + 1u >= _n) {
            break;
        }
        _from = _to;
    }
    rde_arr_free(&_ring);
    rde_arr_free(&_rings);
    rde_arr_free(&_pa);
    rde_arr_free(&_wa);
    if(!_ok) {
        // Not traced: what was made of it goes again, and it is cut as a stroke.
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_made); _i++) {
            fude_zoom_scene_discard(_s, ((const u32*)_made.memory)[_i]);
        }
        rde_arr_free(&_made);
        return false;
    }
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_made); _i++) {
        rde_arr_add(&_e->born, (any)&((const u32*)_made.memory)[_i]);
        rde_arr_add(_list, (any)&((const u32*)_made.memory)[_i]);
    }
    rde_arr_free(&_made);
    fude_zoom_erase_take(_s, _e, _object);
    return true;
}

// PARTIAL reaching a SHAPE: it becomes what it looks like — its line a closed
// stroke, its fill (if any) a FILL just under it — born this sweep and added
// to _list, so this very step cuts them as any line and fill.
RDE_INTERNAL void fude_zoom_erase_unshape(fude_zoom_scene* _s, fude_zoom_eraser* _e, u32 _object, rde_arr* _list) {
    const fude_zoom_object _o = *fude_zoom_scene_object(_s, _object);
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    b8      _closed;
    fude_zoom_scene_shape_outline(_s, _object, 96u, &_pts, &_closed);
    const u32 _n = (u32)rde_arr_length(&_pts);
    if(_n < 2u) {
        rde_arr_free(&_pts);
        fude_zoom_erase_take(_s, _e, _object);
        return;
    }
    const fude_zoom_v2* _p     = (const fude_zoom_v2*)_pts.memory;
    const f64           _width = (f64)_o.radius * _o.scale;
    // Its quanta: an eighth of its line's half-width (whole numbers from its first corner).
    const i8  _qe   = (i8)fmax(-60.0, fmin(60.0, floor(log2(fmax(_width, 1e-300) / 8.0))));
    const f64 _grid = ldexp(1.0, _qe);
    rde_arr   _q    = rde_arr_new(sizeof(fude_zoom_qpoint), rde_memory_allocator_get_default_std());
    const u32 _m    = _closed ? _n + 1u : _n;
    fude_zoom_qpoint* _qp = rde_arr_add_n(&_q, _m);
    for(u32 _i = 0; _i < _m; _i++) {
        const fude_zoom_v2 _at = _p[_i % _n];
        _qp[_i] = (fude_zoom_qpoint){ (i32)llround((_at.x - _p[0].x) / _grid), (i32)llround((_at.y - _p[0].y) / _grid), 0u, 0u };
    }
    const fude_zoom_place _place = { _p[0], 0.0, 1.0 };
    const u32 _line = fude_zoom_scene_add_stroke_at(_s, _o.frame, _place, _qe, _qp, _m, 0u, _o.color, (f32)_width, 0u, 0, _o.z);
    rde_arr_add(&_e->born, (any)&_line);
    rde_arr_add(_list, (any)&_line);
    if(_closed && (_o.flags & FUDE_ZOOM_FLAG_FILLED) && _n >= 3u) {
        const u32 _fill = fude_zoom_scene_add_fill(_s, _o.frame, _place, _qe, _qp, _n, (_o.flags & FUDE_ZOOM_FLAG_FILL_OWN) ? _o.fill : _o.color, _o.z - 1u);
        rde_arr_add(&_e->born, (any)&_fill);
        rde_arr_add(_list, (any)&_fill);
    }
    rde_arr_free(&_q);
    rde_arr_free(&_pts);
    fude_zoom_erase_take(_s, _e, _object);
}

u32 fude_zoom_erase_step(fude_zoom_scene* _s, fude_zoom_eraser* _e, u32 _frame, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _r, f64 _min_len) {
    if(!_e->open || !(_r > 0.0)) {
        return 0;
    }
    const fude_zoom_box _reach = { fmin(_a.x, _b.x) - _r, fmin(_a.y, _b.y) - _r, fmax(_a.x, _b.x) + _r, fmax(_a.y, _b.y) + _r };
    rde_arr_clear(&_e->found);
    fude_zoom_scene_query(_s, _frame, _reach, &_e->found);
    const u32 _found = (u32)rde_arr_length(&_e->found);
    // The candidates are copied out first: cutting adds objects to the frame.
    rde_arr _list = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    if(_found > 0) {
        memcpy(rde_arr_add_n(&_list, _found), _e->found.memory, (usize)_found * sizeof(u32));
    }
    rde_arr _qs = rde_arr_new(sizeof(fude_zoom_qpoint), rde_memory_allocator_get_default_std());
    u32 _taken = 0;
    for(u32 _k = 0; _k < (u32)rde_arr_length(&_list); _k++) {   // (a shape unmade adds its line and fill)
        const u32 _object = ((const u32*)_list.memory)[_k];
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
        if(_o->kind == FUDE_ZOOM_KIND_FILL && (_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
            // A fill: rubbed out where the eraser passes (PARTIAL), or taken whole.
            if(fude_zoom_erase_fill_touched(_s, _e, _object, _a, _b, _r)) {
                const u32 _cut = _e->mode == FUDE_ZOOM_ERASE_PARTIAL ? fude_zoom_erase_cut_fill(_s, _e, _object, _a, _b, _r) : FUDE_ZOOM_NONE;
                fude_zoom_erase_take(_s, _e, _object);
                if(_cut != FUDE_ZOOM_NONE) {
                    rde_arr_add(&_e->born, (any)&_cut);
                }
                _taken++;
            }
            continue;
        }
        if(_o->kind == FUDE_ZOOM_KIND_SHAPE && (_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
            // A shape: rubbed out where the eraser passes as its line and fill
            // (PARTIAL: unmade into them, cut just below), or taken whole.
            if(fude_zoom_erase_shape_touched(_s, _object, _a, _b, _r)) {
                if(_e->mode == FUDE_ZOOM_ERASE_PARTIAL) {
                    fude_zoom_erase_unshape(_s, _e, _object, &_list);
                } else {
                    fude_zoom_erase_take(_s, _e, _object);
                }
                _taken++;
            }
            continue;
        }
        if(_o->kind != FUDE_ZOOM_KIND_STROKE || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->count == 0) {
            continue;
        }
        rde_arr_clear(&_qs);
        rde_arr_add_n(&_qs, _o->count);
        if(!fude_zoom_scene_points(_s, _object, (fude_zoom_qpoint*)_qs.memory)) {
            continue;
        }
        const fude_zoom_object _copy = *_o;   // the table may move while it is cut
        const fude_zoom_walk   _w    = { &_copy, (const fude_zoom_qpoint*)_qs.memory, _copy.count };
        if(!fude_zoom_walk_touched(&_w, _a, _b, _r)) {
            continue;
        }
        _taken++;
        // Rubbed out as its outline: what the eraser passes over goes, as with a fill.
        if(_e->mode == FUDE_ZOOM_ERASE_PARTIAL && fude_zoom_erase_outline(_s, _e, _object, &_list)) {
            continue;
        }
        if(_e->mode == FUDE_ZOOM_ERASE_STROKE) {
            fude_zoom_erase_gesture(_s, _e, _object);
        } else if(_e->mode == FUDE_ZOOM_ERASE_TRIM) {
            fude_zoom_erase_trim(_s, _e, &_w, _object, _a, _b, _r, _min_len);
        } else {
            fude_zoom_erase_partial(_s, _e, &_w, _object, _a, _b, _r, _min_len);
        }
    }
    rde_arr_free(&_qs);
    rde_arr_free(&_list);
    return _taken;
}

void fude_zoom_erase_end(fude_zoom_scene* _s, fude_zoom_eraser* _e, u32 _frame, fude_zoom_box _box) {
    if(!_e->open) {
        return;
    }
    _e->open = false;
    // A fill rubbed out all over: nothing left of it to keep.
    rde_arr _pts   = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    rde_arr _rings = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr _tris  = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_e->born);) {
        const u32 _object = ((const u32*)_e->born.memory)[_i];
        if(fude_zoom_scene_object(_s, _object)->kind != FUDE_ZOOM_KIND_FILL) {
            _i++;
            continue;
        }
        const u32 _n = fude_zoom_erase_fill_points(_s, _e, _object, &_pts, &_rings);
        u32 _outline = 0;
        while(_outline < _n && ((const u32*)_rings.memory)[_outline] == 0u) {
            _outline++;
        }
        rde_arr_clear(&_tris);
        const u32 _whole = fude_zoom_fill_triangulate((const fude_zoom_v2*)_pts.memory, _outline, &_tris);
        f64 _area_whole = 0.0, _area_left = 0.0;
        for(u32 _pass = 0; _pass < 2u; _pass++) {
            if(_pass == 1u) {
                rde_arr_clear(&_tris);
                fude_zoom_fill_triangulate_rings((const fude_zoom_v2*)_pts.memory, (const u32*)_rings.memory, _n, &_tris);
            }
            const fude_zoom_v2* _t = (const fude_zoom_v2*)_tris.memory;
            f64 _area = 0.0;
            for(u32 _k = 0; _k + 2u < (u32)rde_arr_length(&_tris); _k += 3u) {
                _area += fabs((_t[_k + 1u].x - _t[_k].x) * (_t[_k + 2u].y - _t[_k].y) - (_t[_k + 2u].x - _t[_k].x) * (_t[_k + 1u].y - _t[_k].y)) * 0.5;
            }
            *(_pass == 0u ? &_area_whole : &_area_left) = _area;
        }
        (void)_whole;
        if(_area_left <= _area_whole * 1e-4) {
            fude_zoom_erase_take(_s, _e, _object);   // born this sweep: as if never made
            continue;
        }
        _i++;
    }
    rde_arr_free(&_pts);
    rde_arr_free(&_rings);
    rde_arr_free(&_tris);
    fude_zoom_erase_fills_clear(_e);
    fude_zoom_history_push(_s, _frame, _box, (const u32*)_e->died.memory, (u32)rde_arr_length(&_e->died), (const u32*)_e->born.memory, (u32)rde_arr_length(&_e->born));
    rde_arr_clear(&_e->died);
    rde_arr_clear(&_e->born);
}
