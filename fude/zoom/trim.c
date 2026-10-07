// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/trim.h"
#include "zoom/shape.h"
#include <math.h>
#include <stdlib.h>

// Where segment p→q crosses the line a + t·d (t: along it), added to _out between _from and _to.
RDE_INTERNAL void fude_zoom_trim_piece(fude_zoom_v2 _a, fude_zoom_v2 _d, fude_zoom_v2 _p, fude_zoom_v2 _q, f64 _from, f64 _to, f64* _out, u32* _n, u32 _max) {
    const fude_zoom_v2 _e = { _q.x - _p.x, _q.y - _p.y };
    const f64 _den = _d.x * _e.y - _d.y * _e.x;
    if(fabs(_den) < 1e-18 || *_n >= _max) {
        return;   // (parallel: no one crossing)
    }
    const fude_zoom_v2 _w = { _p.x - _a.x, _p.y - _a.y };
    const f64 _t = (_w.x * _e.y - _w.y * _e.x) / _den, _u = (_w.x * _d.y - _w.y * _d.x) / _den;
    if(_u < -1e-12 || _u > 1.0 + 1e-12 || _t < _from || _t > _to) {
        return;
    }
    _out[(*_n)++] = _t;
}

RDE_INTERNAL int fude_zoom_trim_by_f64(const void* _a, const void* _b) {
    const f64 _x = *(const f64*)_a, _y = *(const f64*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

u32 fude_zoom_trim_crossings(const fude_zoom_scene* _s, u32 _frame, u32 _skip, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _from, f64 _to, f64* _out, u32 _max) {
    const fude_zoom_v2 _d = { _b.x - _a.x, _b.y - _a.y };
    if(!(hypot(_d.x, _d.y) > 0.0) || _max == 0u) {
        return 0;
    }
    const fude_zoom_v2 _p0 = { _a.x + _d.x * _from, _a.y + _d.y * _from }, _p1 = { _a.x + _d.x * _to, _a.y + _d.y * _to };
    const fude_zoom_box _box = { fmin(_p0.x, _p1.x), fmin(_p0.y, _p1.y), fmax(_p0.x, _p1.x), fmax(_p0.y, _p1.y) };
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _found = rde_arr_new(sizeof(u32), _heap), _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap), _q = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
    fude_zoom_scene_query(_s, _frame, _box, &_found);
    u32 _n = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_found) && _n < _max; _i++) {
        const u32 _object = ((const u32*)_found.memory)[_i];
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
        if(_object == _skip || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || fude_zoom_scene_hides(_s, _o)) {
            continue;
        }
        b8 _closed = false;
        rde_arr_clear(&_pts);
        if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
            if(_o->channels == FUDE_ZOOM_SHAPE_SHEET || _o->channels == FUDE_ZOOM_SHAPE_DIMENSION || _o->channels == FUDE_ZOOM_SHAPE_RADIAL ||
               _o->channels == FUDE_ZOOM_SHAPE_ANGLE) {
                continue;   // (a sheet's edge and a measurement's lines are not drawing)
            }
            fude_zoom_scene_shape_outline(_s, _object, 96u, &_pts, &_closed);
        } else if((_o->kind == FUDE_ZOOM_KIND_STROKE || _o->kind == FUDE_ZOOM_KIND_FILL) && _o->count > 0) {
            rde_arr_clear(&_q);
            rde_arr_add_n(&_q, _o->count);
            if(!fude_zoom_scene_points(_s, _object, (fude_zoom_qpoint*)_q.memory)) {
                continue;
            }
            for(u32 _k = 0; _k < _o->count; _k++) {
                const fude_zoom_v2 _p = fude_zoom_scene_point_at(_o, &((const fude_zoom_qpoint*)_q.memory)[_k]);
                rde_arr_add(&_pts, (any)&_p);
            }
            _closed = _o->kind == FUDE_ZOOM_KIND_FILL;
        } else {
            continue;
        }
        const u32 _m = (u32)rde_arr_length(&_pts);
        const fude_zoom_v2* _p = (const fude_zoom_v2*)_pts.memory;
        for(u32 _k = 0; _m >= 2u && _k < (_closed ? _m : _m - 1u); _k++) {
            fude_zoom_trim_piece(_a, _d, _p[_k], _p[(_k + 1u) % _m], _from, _to, _out, &_n, _max);
        }
    }
    rde_arr_free(&_found);
    rde_arr_free(&_pts);
    rde_arr_free(&_q);
    // Sorted, each once (an outline's corner on the line crossed by both its sides).
    qsort(_out, _n, sizeof(f64), fude_zoom_trim_by_f64);
    u32 _w = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        if(_w == 0u || _out[_i] - _out[_w - 1u] > 1e-9) {
            _out[_w++] = _out[_i];
        }
    }
    return _w;
}

u32 fude_zoom_trim_keep(const f64* _t, u32 _n, f64 _at, f64* _keep) {
    f64 _lo = 0.0, _hi = 1.0;
    for(u32 _i = 0; _i < _n; _i++) {
        if(_t[_i] > 1e-9 && _t[_i] < 1.0 - 1e-9) {
            if(_t[_i] <= _at) {
                _lo = fmax(_lo, _t[_i]);
            } else {
                _hi = fmin(_hi, _t[_i]);
            }
        }
    }
    u32 _k = 0;
    if(_lo > 0.0) {
        _keep[2u * _k] = 0.0;
        _keep[2u * _k + 1u] = _lo;
        _k++;
    }
    if(_hi < 1.0) {
        _keep[2u * _k] = _hi;
        _keep[2u * _k + 1u] = 1.0;
        _k++;
    }
    return _k;
}
