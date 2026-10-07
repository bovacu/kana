// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/nest.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    f64 x, y, w, h;
} fude_zoom_nest_rect;

#define FUDE_ZOOM_NEST_ORDERS 6u
#define FUDE_ZOOM_NEST_EPS    1e-9

// The orders the parts are tried in: each a key, larger first.
RDE_INTERNAL f64 fude_zoom_nest_key(const fude_zoom_nest_part* _p, u32 _order) {
    switch(_order) {
        case 0:  return _p->w * _p->h;
        case 1:  return fmax(_p->w, _p->h);
        case 2:  return fmin(_p->w, _p->h);
        case 3:  return _p->w + _p->h;
        case 4:  return _p->w;
        default: return _p->h;
    }
}

typedef struct {
    f64 key, second;
    u32 index;
} fude_zoom_nest_sort;

RDE_INTERNAL int fude_zoom_nest_by_key(const void* _a, const void* _b) {
    const fude_zoom_nest_sort* _x = (const fude_zoom_nest_sort*)_a;
    const fude_zoom_nest_sort* _y = (const fude_zoom_nest_sort*)_b;
    if(_x->key != _y->key) {
        return _x->key > _y->key ? -1 : 1;
    }
    if(_x->second != _y->second) {
        return _x->second > _y->second ? -1 : 1;
    }
    return _x->index < _y->index ? -1 : (_x->index > _y->index ? 1 : 0);
}

RDE_INTERNAL b8 fude_zoom_nest_inside(const fude_zoom_nest_rect* _a, const fude_zoom_nest_rect* _b) {
    return _a->x >= _b->x - FUDE_ZOOM_NEST_EPS && _a->y >= _b->y - FUDE_ZOOM_NEST_EPS &&
           _a->x + _a->w <= _b->x + _b->w + FUDE_ZOOM_NEST_EPS && _a->y + _a->h <= _b->y + _b->h + FUDE_ZOOM_NEST_EPS;
}

// The free rectangles once _used is taken from them: each one it overlaps split
// into what is left of it on each side, then those inside another dropped.
RDE_INTERNAL void fude_zoom_nest_take(rde_arr* _free, fude_zoom_nest_rect _used) {
    rde_arr _next = rde_arr_new(sizeof(fude_zoom_nest_rect), rde_memory_allocator_get_default_std());
    const fude_zoom_nest_rect* _f = (const fude_zoom_nest_rect*)_free->memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(_free); _i++) {
        const fude_zoom_nest_rect _r = _f[_i];
        if(_used.x >= _r.x + _r.w - FUDE_ZOOM_NEST_EPS || _used.x + _used.w <= _r.x + FUDE_ZOOM_NEST_EPS ||
           _used.y >= _r.y + _r.h - FUDE_ZOOM_NEST_EPS || _used.y + _used.h <= _r.y + FUDE_ZOOM_NEST_EPS) {
            rde_arr_add(&_next, (any)&_r);
            continue;
        }
        if(_used.x > _r.x + FUDE_ZOOM_NEST_EPS) {
            const fude_zoom_nest_rect _left = { _r.x, _r.y, _used.x - _r.x, _r.h };
            rde_arr_add(&_next, (any)&_left);
        }
        if(_used.x + _used.w < _r.x + _r.w - FUDE_ZOOM_NEST_EPS) {
            const fude_zoom_nest_rect _right = { _used.x + _used.w, _r.y, _r.x + _r.w - (_used.x + _used.w), _r.h };
            rde_arr_add(&_next, (any)&_right);
        }
        if(_used.y > _r.y + FUDE_ZOOM_NEST_EPS) {
            const fude_zoom_nest_rect _below = { _r.x, _r.y, _r.w, _used.y - _r.y };
            rde_arr_add(&_next, (any)&_below);
        }
        if(_used.y + _used.h < _r.y + _r.h - FUDE_ZOOM_NEST_EPS) {
            const fude_zoom_nest_rect _above = { _r.x, _used.y + _used.h, _r.w, _r.y + _r.h - (_used.y + _used.h) };
            rde_arr_add(&_next, (any)&_above);
        }
    }
    // Those inside another: gone (the same one twice: once).
    fude_zoom_nest_rect* _n = (fude_zoom_nest_rect*)_next.memory;
    u32 _count = (u32)rde_arr_length(&_next);
    rde_arr_clear(_free);
    for(u32 _i = 0; _i < _count; _i++) {
        b8 _inside = false;
        for(u32 _j = 0; _j < _count && !_inside; _j++) {
            if(_i != _j && fude_zoom_nest_inside(&_n[_i], &_n[_j]) && (!fude_zoom_nest_inside(&_n[_j], &_n[_i]) || _j < _i)) {
                _inside = true;
            }
        }
        if(!_inside) {
            rde_arr_add(_free, (any)&_n[_i]);
        }
    }
    rde_arr_free(&_next);
}

// One order's go: how much wood it put down, its places into _out.
RDE_INTERNAL f64 fude_zoom_nest_try(f64 _bw, f64 _bh, f64 _kerf, const fude_zoom_nest_part* _parts, const u32* _order, u32 _n, fude_zoom_nest_place* _out) {
    rde_arr _free = rde_arr_new(sizeof(fude_zoom_nest_rect), rde_memory_allocator_get_default_std());
    // The board and each part grown by the kerf: neighbours a kerf apart, the board's edges none.
    const fude_zoom_nest_rect _all = { 0.0, 0.0, _bw + _kerf, _bh + _kerf };
    rde_arr_add(&_free, (any)&_all);
    f64 _area = 0.0;
    for(u32 _k = 0; _k < _n; _k++) {
        const u32 _i = _order[_k];
        const fude_zoom_nest_part* _p = &_parts[_i];
        _out[_i].placed = false;
        if(!(_p->w > 0.0) || !(_p->h > 0.0)) {
            continue;
        }
        f64 _best_short = 1e300, _best_long = 1e300;
        fude_zoom_nest_rect _best = { 0.0, 0.0, 0.0, 0.0 };
        b8 _turned = false, _found = false;
        const fude_zoom_nest_rect* _f = (const fude_zoom_nest_rect*)_free.memory;
        for(u32 _r = 0; _r < (u32)rde_arr_length(&_free); _r++) {
            for(u32 _turn = 0; _turn < (_p->can_turn ? 2u : 1u); _turn++) {
                const f64 _w = (_turn ? _p->h : _p->w) + _kerf, _h = (_turn ? _p->w : _p->h) + _kerf;
                if(_w > _f[_r].w + FUDE_ZOOM_NEST_EPS || _h > _f[_r].h + FUDE_ZOOM_NEST_EPS) {
                    continue;
                }
                const f64 _lw = _f[_r].w - _w, _lh = _f[_r].h - _h;
                const f64 _short = fmin(_lw, _lh), _long = fmax(_lw, _lh);
                if(_short < _best_short - FUDE_ZOOM_NEST_EPS || (fabs(_short - _best_short) <= FUDE_ZOOM_NEST_EPS && _long < _best_long)) {
                    _best_short = _short;
                    _best_long  = _long;
                    _best       = (fude_zoom_nest_rect){ _f[_r].x, _f[_r].y, _w, _h };
                    _turned     = _turn != 0u;
                    _found      = true;
                }
            }
        }
        if(!_found) {
            continue;
        }
        fude_zoom_nest_take(&_free, _best);
        _out[_i] = (fude_zoom_nest_place){ _best.x, _best.y, _turned ? _p->h : _p->w, _turned ? _p->w : _p->h, _turned, true };
        _area += _p->w * _p->h;
    }
    rde_arr_free(&_free);
    return _area;
}

u32 fude_zoom_nest(f64 _board_w, f64 _board_h, f64 _kerf, const fude_zoom_nest_part* _parts, u32 _n, fude_zoom_nest_place* _out) {
    if(_n == 0) {
        return 0;
    }
    _kerf = _kerf > 0.0 ? _kerf : 0.0;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _keys_arr = rde_arr_new(sizeof(fude_zoom_nest_sort), _heap), _order_arr = rde_arr_new(sizeof(u32), _heap), _try_arr = rde_arr_new(sizeof(fude_zoom_nest_place), _heap);
    rde_arr_resize(&_keys_arr, _n);
    rde_arr_resize(&_order_arr, _n);
    rde_arr_resize(&_try_arr, _n);
    fude_zoom_nest_sort*  _keys  = (fude_zoom_nest_sort*)_keys_arr.memory;   // (sized once: they stay put)
    u32*                  _order = (u32*)_order_arr.memory;
    fude_zoom_nest_place* _try   = (fude_zoom_nest_place*)_try_arr.memory;
    f64 _best = -1.0;
    for(u32 _o = 0; _o < FUDE_ZOOM_NEST_ORDERS; _o++) {
        for(u32 _i = 0; _i < _n; _i++) {
            _keys[_i] = (fude_zoom_nest_sort){ fude_zoom_nest_key(&_parts[_i], _o), _parts[_i].w * _parts[_i].h, _i };
        }
        qsort(_keys, _n, sizeof(fude_zoom_nest_sort), fude_zoom_nest_by_key);
        for(u32 _i = 0; _i < _n; _i++) {
            _order[_i] = _keys[_i].index;
        }
        const f64 _area = fude_zoom_nest_try(_board_w, _board_h, _kerf, _parts, _order, _n, _try);
        if(_area > _best + FUDE_ZOOM_NEST_EPS) {
            _best = _area;
            memcpy(_out, _try, (usize)_n * sizeof(fude_zoom_nest_place));
        }
    }
    u32 _placed = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        _placed += _out[_i].placed ? 1u : 0u;
    }
    rde_arr_free(&_keys_arr);
    rde_arr_free(&_order_arr);
    rde_arr_free(&_try_arr);
    return _placed;
}
