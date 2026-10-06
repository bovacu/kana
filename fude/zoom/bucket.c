// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/bucket.h"
#include "zoom/fill.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// The distance from _q to the segment _a–_b, less its half-width.
RDE_INTERNAL f64 fude_zoom_bucket_reach(fude_zoom_v2 _q, const fude_zoom_bucket_ink* _k) {
    const f64 _dx = _k->b.x - _k->a.x, _dy = _k->b.y - _k->a.y, _l2 = _dx * _dx + _dy * _dy;
    f64 _t = _l2 > 0.0 ? ((_q.x - _k->a.x) * _dx + (_q.y - _k->a.y) * _dy) / _l2 : 0.0;
    _t = _t < 0.0 ? 0.0 : (_t > 1.0 ? 1.0 : _t);
    return hypot(_k->a.x + _dx * _t - _q.x, _k->a.y + _dy * _t - _q.y) - _k->r;
}

// A chamfer distance transform (3-4) of _mask's cells to the nearest set one,
// in cells (an eighth or so off Euclidean at worst).
RDE_INTERNAL void fude_zoom_bucket_chamfer(const u8* _mask, u32 _w, u32 _h, f32* _d) {
    const f32 _far = 1e9f;
    for(u32 _i = 0; _i < _w * _h; _i++) {
        _d[_i] = _mask[_i] ? 0.0f : _far;
    }
    for(u32 _j = 0; _j < _h; _j++) {
        for(u32 _i = 0; _i < _w; _i++) {
            f32 _v = _d[_j * _w + _i];
            if(_i > 0)                    { _v = fminf(_v, _d[_j * _w + _i - 1u] + 3.0f); }
            if(_j > 0)                    { _v = fminf(_v, _d[(_j - 1u) * _w + _i] + 3.0f); }
            if(_i > 0 && _j > 0)          { _v = fminf(_v, _d[(_j - 1u) * _w + _i - 1u] + 4.0f); }
            if(_i + 1u < _w && _j > 0)    { _v = fminf(_v, _d[(_j - 1u) * _w + _i + 1u] + 4.0f); }
            _d[_j * _w + _i] = _v;
        }
    }
    for(u32 _j = _h; _j-- > 0;) {
        for(u32 _i = _w; _i-- > 0;) {
            f32 _v = _d[_j * _w + _i];
            if(_i + 1u < _w)               { _v = fminf(_v, _d[_j * _w + _i + 1u] + 3.0f); }
            if(_j + 1u < _h)               { _v = fminf(_v, _d[(_j + 1u) * _w + _i] + 3.0f); }
            if(_i + 1u < _w && _j + 1u < _h) { _v = fminf(_v, _d[(_j + 1u) * _w + _i + 1u] + 4.0f); }
            if(_i > 0 && _j + 1u < _h)     { _v = fminf(_v, _d[(_j + 1u) * _w + _i - 1u] + 4.0f); }
            _d[_j * _w + _i] = _v;
        }
    }
    for(u32 _i = 0; _i < _w * _h; _i++) {
        _d[_i] = _d[_i] >= _far ? _far : _d[_i] / 3.0f;
    }
}

FUDE_ZOOM_BUCKET_ fude_zoom_bucket_region(const fude_zoom_bucket_ink* _ink, u32 _n, fude_zoom_box _bounds, fude_zoom_v2 _tap,
                                          f64 _cell, f64 _gap, f64 _under, rde_arr* _out, rde_arr* _rings) {
    rde_arr_clear(_out);
    if(_rings != NULL) {
        rde_arr_clear(_rings);
    }
    const f64 _bw = _bounds.max_x - _bounds.min_x, _bh = _bounds.max_y - _bounds.min_y;
    if(!(_cell > 0.0) || !(_bw > 0.0) || !(_bh > 0.0)) {
        return FUDE_ZOOM_BUCKET_NOTHING;
    }
    while((_bw / _cell + 2.0) * (_bh / _cell + 2.0) > (f64)FUDE_ZOOM_BUCKET_CELLS) {
        _cell *= 1.25;
    }
    const u32 _w = (u32)ceil(_bw / _cell) + 1u, _h = (u32)ceil(_bh / _cell) + 1u;
    const f64 _x0 = _bounds.min_x, _y0 = _bounds.min_y;
    const i32 _ti = (i32)llround((_tap.x - _x0) / _cell), _tj = (i32)llround((_tap.y - _y0) / _cell);
    if(_ti <= 0 || _tj <= 0 || _ti >= (i32)_w - 1 || _tj >= (i32)_h - 1) {
        return FUDE_ZOOM_BUCKET_NOTHING;
    }
    const usize _cells = (usize)_w * _h;
    f32* _d    = (f32*)malloc(_cells * sizeof(f32));
    u8*  _area = (u8*)calloc(_cells, 1u);
    u32* _todo = (u32*)malloc(_cells * sizeof(u32));
    if(_d == NULL || _area == NULL || _todo == NULL) {
        free(_d); free(_area); free(_todo);
        return FUDE_ZOOM_BUCKET_NOTHING;
    }
    // 1. Each cell's distance to the ink (stamped piece by piece, as far as the walls reach).
    for(usize _i = 0; _i < _cells; _i++) {
        _d[_i] = 1e30f;
    }
    const f64 _wall = _gap * 0.5;
    for(u32 _k = 0; _k < _n; _k++) {
        const fude_zoom_bucket_ink* _p = &_ink[_k];
        const f64 _reach = _p->r + _wall + _cell;
        const i32 _i0 = (i32)floor((fmin(_p->a.x, _p->b.x) - _reach - _x0) / _cell), _i1 = (i32)ceil((fmax(_p->a.x, _p->b.x) + _reach - _x0) / _cell);
        const i32 _j0 = (i32)floor((fmin(_p->a.y, _p->b.y) - _reach - _y0) / _cell), _j1 = (i32)ceil((fmax(_p->a.y, _p->b.y) + _reach - _y0) / _cell);
        for(i32 _j = _j0 < 0 ? 0 : _j0; _j <= _j1 && _j < (i32)_h; _j++) {
            for(i32 _i = _i0 < 0 ? 0 : _i0; _i <= _i1 && _i < (i32)_w; _i++) {
                const f32 _v = (f32)fude_zoom_bucket_reach((fude_zoom_v2){ _x0 + _i * _cell, _y0 + _j * _cell }, _p);
                f32* _at = &_d[(usize)_j * _w + (usize)_i];
                *_at = _v < *_at ? _v : *_at;
            }
        }
    }
    const usize _start = (usize)_tj * _w + (usize)_ti;
    if(_d[_start] < (f32)_wall) {
        const b8 _on = _d[_start] < 0.0f;
        free(_d); free(_area); free(_todo);
        return _on ? FUDE_ZOOM_BUCKET_ON_INK : FUDE_ZOOM_BUCKET_NOTHING;
    }
    // 2–3. The cells reached from the tap, walls not crossed; the edge reached: not closed.
    u32 _head = 0, _tail = 0;
    _todo[_tail++] = (u32)_start;
    _area[_start]  = 1u;
    b8 _open = false;
    while(_head < _tail && !_open) {
        const u32 _c = _todo[_head++];
        const u32 _i = _c % _w, _j = _c / _w;
        if(_i == 0 || _j == 0 || _i + 1u == _w || _j + 1u == _h) {
            _open = true;
            break;
        }
        const u32 _next[4] = { _c - 1u, _c + 1u, _c - _w, _c + _w };
        for(u32 _k = 0; _k < 4u; _k++) {
            if(!_area[_next[_k]] && _d[_next[_k]] >= (f32)_wall) {
                _area[_next[_k]] = 1u;
                _todo[_tail++]   = _next[_k];
            }
        }
    }
    free(_todo);
    if(_open) {
        free(_d); free(_area);
        return FUDE_ZOOM_BUCKET_OPEN;
    }
    // 4. Grown back under the ink: the field is the distance to the area (out)
    //    or to its outside (in), less the growth; its 0 line is the fill's edge.
    f32* _out_d = (f32*)malloc(_cells * sizeof(f32));
    u8*  _not   = (u8*)malloc(_cells);
    if(_out_d == NULL || _not == NULL) {
        free(_d); free(_area); free(_out_d); free(_not);
        return FUDE_ZOOM_BUCKET_NOTHING;
    }
    fude_zoom_bucket_chamfer(_area, _w, _h, _out_d);
    for(usize _i = 0; _i < _cells; _i++) {
        _not[_i] = _area[_i] ? 0u : 1u;
    }
    fude_zoom_bucket_chamfer(_not, _w, _h, _d);   // (the ink distances are done with: the inside's distance to the outside)
    const f32 _grow = (f32)((_wall + _under) / _cell);
    for(usize _i = 0; _i < _cells; _i++) {
        _d[_i] = (_area[_i] ? -_d[_i] : _out_d[_i]) - _grow;
        _d[_i] *= (f32)_cell;
    }
    // Its border cells kept outside, so every loop closes inside the grid.
    for(u32 _i = 0; _i < _w; _i++) {
        _d[_i] = fmaxf(_d[_i], 1.0f);
        _d[(usize)(_h - 1u) * _w + _i] = fmaxf(_d[(usize)(_h - 1u) * _w + _i], 1.0f);
    }
    for(u32 _j = 0; _j < _h; _j++) {
        _d[(usize)_j * _w] = fmaxf(_d[(usize)_j * _w], 1.0f);
        _d[(usize)_j * _w + _w - 1u] = fmaxf(_d[(usize)_j * _w + _w - 1u], 1.0f);
    }
    const u32 _k = fude_zoom_fill_contour(_d, _w, _h, _x0, _y0, _cell, _out, _rings);
    free(_d); free(_area); free(_out_d); free(_not);
    return _k >= 3u ? FUDE_ZOOM_BUCKET_FILLED : FUDE_ZOOM_BUCKET_NOTHING;
}
