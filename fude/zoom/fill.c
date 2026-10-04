// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/fill.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct { f64 x0, y0, x1, y1; u32 ring; } fude_zoom_fill_edge;   // y0 < y1
typedef struct { f64 mid, top, bottom; u32 ring; } fude_zoom_fill_cut;   // an edge's x through a slab

RDE_INTERNAL int fude_zoom_fill_by_f64(const void* _a, const void* _b) {
    const f64 _x = *(const f64*)_a, _y = *(const f64*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

RDE_INTERNAL int fude_zoom_fill_by_low(const void* _a, const void* _b) {
    const f64 _x = ((const fude_zoom_fill_edge*)_a)->y0, _y = ((const fude_zoom_fill_edge*)_b)->y0;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

RDE_INTERNAL int fude_zoom_fill_by_mid(const void* _a, const void* _b) {
    const f64 _x = ((const fude_zoom_fill_cut*)_a)->mid, _y = ((const fude_zoom_fill_cut*)_b)->mid;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

RDE_INTERNAL f64 fude_zoom_fill_x_at(const fude_zoom_fill_edge* _e, f64 _y) {
    return _e->x0 + (_e->x1 - _e->x0) * (_y - _e->y0) / (_e->y1 - _e->y0);
}

u32 fude_zoom_fill_triangulate(const fude_zoom_v2* _p, u32 _n, rde_arr* _out) {
    return fude_zoom_fill_triangulate_rings(_p, NULL, _n, _out);
}

u32 fude_zoom_fill_triangulate_rings(const fude_zoom_v2* _p, const u32* _rings, u32 _n, rde_arr* _out) {
    if(_n < 3u) {
        return 0;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    // The edges, low end first, each run of a ring closed on itself (level ones
    // add nothing: they are slab edges anyway).
    fude_zoom_fill_edge* _e = (fude_zoom_fill_edge*)_heap->malloc(_heap->allocator, (usize)_n * sizeof(fude_zoom_fill_edge));
    u32 _ne = 0, _rings_most = 0;
    for(u32 _start = 0; _start < _n;) {
        const u32 _ring = _rings != NULL ? _rings[_start] : 0u;
        u32 _end = _start + 1u;
        while(_end < _n && (_rings != NULL ? _rings[_end] : 0u) == _ring) {
            _end++;
        }
        _rings_most = _ring > _rings_most ? _ring : _rings_most;
        for(u32 _i = _start; _i < _end; _i++) {
            fude_zoom_v2 _a = _p[_i], _b = _p[_i + 1u < _end ? _i + 1u : _start];
            if(_a.y == _b.y) {
                continue;
            }
            if(_a.y > _b.y) {
                const fude_zoom_v2 _t = _a;
                _a = _b;
                _b = _t;
            }
            _e[_ne++] = (fude_zoom_fill_edge){ _a.x, _a.y, _b.x, _b.y, _ring };
        }
        _start = _end;
    }
    qsort(_e, _ne, sizeof(fude_zoom_fill_edge), fude_zoom_fill_by_low);
    // The slabs' edges: every corner's height, and every crossing's.
    u32  _cap = _n * 2u + 16u, _ny = 0;
    f64* _ys  = (f64*)_heap->malloc(_heap->allocator, (usize)_cap * sizeof(f64));
    for(u32 _i = 0; _i < _n; _i++) {
        _ys[_ny++] = _p[_i].y;
    }
    for(u32 _i = 0; _i < _ne; _i++) {
        for(u32 _j = _i + 1u; _j < _ne && _e[_j].y0 < _e[_i].y1; _j++) {
            const f64 _lo = fmax(_e[_i].y0, _e[_j].y0), _hi = fmin(_e[_i].y1, _e[_j].y1);
            if(_hi <= _lo) {
                continue;
            }
            const f64 _da = fude_zoom_fill_x_at(&_e[_i], _lo) - fude_zoom_fill_x_at(&_e[_j], _lo);
            const f64 _db = fude_zoom_fill_x_at(&_e[_i], _hi) - fude_zoom_fill_x_at(&_e[_j], _hi);
            if((_da < 0.0 && _db > 0.0) || (_da > 0.0 && _db < 0.0)) {
                if(_ny == _cap) {
                    _cap *= 2u;
                    f64* _more = (f64*)_heap->malloc(_heap->allocator, (usize)_cap * sizeof(f64));
                    memcpy(_more, _ys, (usize)_ny * sizeof(f64));
                    _heap->free(_heap->allocator, _ys);
                    _ys = _more;
                }
                _ys[_ny++] = _lo + (_hi - _lo) * _da / (_da - _db);
            }
        }
    }
    qsort(_ys, _ny, sizeof(f64), fude_zoom_fill_by_f64);
    // Each slab: the edges through it, left to right. Inside is the outline's
    // (ring 0, even-odd) where no cut is open (each cut even-odd in itself,
    // any of them open takes it out): a trapezoid, two triangles, between each
    // two edges with inside between them.
    fude_zoom_fill_cut* _c   = (fude_zoom_fill_cut*)_heap->malloc(_heap->allocator, (usize)(_ne + 1u) * sizeof(fude_zoom_fill_cut));
    u8*                 _par = (u8*)_heap->malloc(_heap->allocator, (usize)_rings_most + 1u);
    memset(_par, 0, (usize)_rings_most + 1u);
    // The edges spanning the slab: an active list, edges joining as the slabs
    // climb past their low ends and leaving past their high ones.
    u32* _active = (u32*)_heap->malloc(_heap->allocator, (usize)(_ne + 1u) * sizeof(u32));
    u32  _na = 0, _next = 0;
    u32 _made = 0;
    for(u32 _k = 0; _k + 1u < _ny; _k++) {
        const f64 _y0 = _ys[_k], _y1 = _ys[_k + 1u];
        if(!(_y1 - _y0 > 1e-12 * (fabs(_y0) + fabs(_y1)))) {
            continue;
        }
        while(_next < _ne && _e[_next].y0 <= _y0) {
            _active[_na++] = _next++;
        }
        u32 _kept = 0;
        for(u32 _i = 0; _i < _na; _i++) {
            if(_e[_active[_i]].y1 > _y0) {
                _active[_kept++] = _active[_i];
            }
        }
        _na = _kept;
        const f64 _ym = 0.5 * (_y0 + _y1);
        u32 _nc = 0;
        for(u32 _a = 0; _a < _na; _a++) {
            const u32 _i = _active[_a];
            if(_e[_i].y1 >= _y1) {
                _c[_nc++] = (fude_zoom_fill_cut){ fude_zoom_fill_x_at(&_e[_i], _ym), fude_zoom_fill_x_at(&_e[_i], _y0), fude_zoom_fill_x_at(&_e[_i], _y1), _e[_i].ring };
            }
        }
        qsort(_c, _nc, sizeof(fude_zoom_fill_cut), fude_zoom_fill_by_mid);
        u8  _outline = 0;
        u32 _open    = 0;
        for(u32 _i = 0; _i < _nc; _i++) {
            if(_i > 0 && _outline && _open == 0u) {
                const fude_zoom_v2 _six[6] = {
                    { _c[_i - 1u].top, _y0 }, { _c[_i].top, _y0 }, { _c[_i].bottom, _y1 },
                    { _c[_i - 1u].top, _y0 }, { _c[_i].bottom, _y1 }, { _c[_i - 1u].bottom, _y1 },
                };
                for(u32 _v = 0; _v < 6u; _v++) {
                    rde_arr_add(_out, (any)&_six[_v]);
                }
                _made += 2u;
            }
            if(_c[_i].ring == 0u) {
                _outline ^= 1u;
            } else {
                _par[_c[_i].ring] ^= 1u;
                _open = _par[_c[_i].ring] ? _open + 1u : _open - 1u;
            }
        }
        for(u32 _i = 0; _i < _nc; _i++) {
            _par[_c[_i].ring] = 0;   // (each ring crosses a slab an even number of times: already 0)
        }
    }
    _heap->free(_heap->allocator, _active);
    _heap->free(_heap->allocator, _par);
    _heap->free(_heap->allocator, _c);
    _heap->free(_heap->allocator, _ys);
    _heap->free(_heap->allocator, _e);
    return _made;
}

b8 fude_zoom_fill_inside_rings(const fude_zoom_v2* _p, const u32* _rings, u32 _n, fude_zoom_v2 _q) {
    b8 _in = false;
    for(u32 _start = 0; _start < _n;) {
        const u32 _ring = _rings != NULL ? _rings[_start] : 0u;
        u32 _end = _start + 1u;
        while(_end < _n && (_rings != NULL ? _rings[_end] : 0u) == _ring) {
            _end++;
        }
        const b8 _here = fude_zoom_fill_inside(&_p[_start], _end - _start, _q);
        if(_ring == 0u) {
            _in = _in != _here;
        } else if(_here) {
            return false;   // the eraser took it
        }
        _start = _end;
    }
    return _in;
}

u32 fude_zoom_fill_capsule(fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _r, rde_arr* _out) {
    const f64 _dx = _b.x - _a.x, _dy = _b.y - _a.y, _len = hypot(_dx, _dy);
    const f64 _base = _len > 0.0 ? atan2(_dy, _dx) : 0.0;
    const u32 _half = FUDE_ZOOM_FILL_ROUND;
    // Round b's end, then round a's: a half turn each, from one side to the other.
    for(u32 _end = 0; _end < 2u; _end++) {
        const fude_zoom_v2 _c = _end == 0u ? _b : _a;
        const f64 _from = _base - 1.5707963267948966 + (_end == 0u ? 0.0 : 3.141592653589793);
        for(u32 _i = 0; _i <= _half; _i++) {
            const f64 _t = _from + 3.141592653589793 * (f64)_i / (f64)_half;
            const fude_zoom_v2 _q = { _c.x + cos(_t) * _r, _c.y + sin(_t) * _r };
            rde_arr_add(_out, (any)&_q);
        }
    }
    return 2u * (_half + 1u);
}

b8 fude_zoom_fill_inside(const fude_zoom_v2* _p, u32 _n, fude_zoom_v2 _q) {
    b8 _in = false;
    for(u32 _i = 0, _j = _n - 1u; _i < _n; _j = _i++) {
        if((_p[_i].y > _q.y) != (_p[_j].y > _q.y) && _q.x < (_p[_j].x - _p[_i].x) * (_q.y - _p[_i].y) / (_p[_j].y - _p[_i].y) + _p[_i].x) {
            _in = !_in;
        }
    }
    return _in;
}

u32 fude_zoom_fill_clip(const fude_zoom_v2* _p, u32 _n, fude_zoom_box _box, rde_arr* _out) {
    rde_arr_clear(_out);
    if(_n < 3u) {
        return 0;
    }
    rde_arr _a = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    fude_zoom_v2* _first = rde_arr_add_n(&_a, _n);
    memcpy(_first, _p, (usize)_n * sizeof(fude_zoom_v2));
    // Each side of the box: x ≥ min_x, x ≤ max_x, y ≥ min_y, y ≤ max_y.
    for(u32 _side = 0; _side < 4u && rde_arr_length(&_a) > 0; _side++) {
        rde_arr_clear(_out);
        const fude_zoom_v2* _in = (const fude_zoom_v2*)_a.memory;
        const u32           _k  = (u32)rde_arr_length(&_a);
        for(u32 _i = 0; _i < _k; _i++) {
            const fude_zoom_v2 _u = _in[_i], _v = _in[(_i + 1u) % _k];
            const f64 _du = _side == 0u ? _u.x - _box.min_x : _side == 1u ? _box.max_x - _u.x : _side == 2u ? _u.y - _box.min_y : _box.max_y - _u.y;
            const f64 _dv = _side == 0u ? _v.x - _box.min_x : _side == 1u ? _box.max_x - _v.x : _side == 2u ? _v.y - _box.min_y : _box.max_y - _v.y;
            if(_du >= 0.0) {
                rde_arr_add(_out, (any)&_u);
            }
            if((_du >= 0.0) != (_dv >= 0.0)) {
                const f64          _t = _du / (_du - _dv);
                const fude_zoom_v2 _c = { _u.x + (_v.x - _u.x) * _t, _u.y + (_v.y - _u.y) * _t };
                rde_arr_add(_out, (any)&_c);
            }
        }
        rde_arr_clear(&_a);
        if(rde_arr_length(_out) > 0) {
            memcpy(rde_arr_add_n(&_a, rde_arr_length(_out)), _out->memory, rde_arr_length(_out) * sizeof(fude_zoom_v2));
        }
    }
    rde_arr_free(&_a);
    return (u32)rde_arr_length(_out);
}

// Where segment a-b crosses c-d (t along a-b, u along c-d, both in [0, 1]).
RDE_INTERNAL b8 fude_zoom_fill_cross(fude_zoom_v2 _a, fude_zoom_v2 _b, fude_zoom_v2 _c, fude_zoom_v2 _d, f64* _t, f64* _u) {
    const f64 _den = (_b.x - _a.x) * (_d.y - _c.y) - (_b.y - _a.y) * (_d.x - _c.x);
    if(_den == 0.0) {
        return false;
    }
    *_t = ((_c.x - _a.x) * (_d.y - _c.y) - (_c.y - _a.y) * (_d.x - _c.x)) / _den;
    *_u = ((_c.x - _a.x) * (_b.y - _a.y) - (_c.y - _a.y) * (_b.x - _a.x)) / _den;
    return *_t > 1e-12 && *_t <= 1.0 && *_u >= 0.0 && *_u <= 1.0;
}

RDE_INTERNAL f64 fude_zoom_fill_signed_area(const fude_zoom_v2* _p, u32 _n) {
    f64 _a = 0.0;
    for(u32 _i = 0, _j = _n - 1u; _i < _n; _j = _i++) {
        _a += _p[_j].x * _p[_i].y - _p[_i].x * _p[_j].y;
    }
    return _a * 0.5;
}

RDE_INTERNAL f64 fude_zoom_fill_area(const fude_zoom_v2* _p, u32 _n) {
    f64 _a = 0.0;
    for(u32 _i = 0, _j = _n - 1u; _i < _n; _j = _i++) {
        _a += _p[_j].x * _p[_i].y - _p[_i].x * _p[_j].y;
    }
    return fabs(_a) * 0.5;
}

// A loop taken out: kept as the best when it is round _tap and smaller.
RDE_INTERNAL void fude_zoom_fill_offer(const fude_zoom_v2* _loop, u32 _n, fude_zoom_v2 _tap, rde_arr* _out, f64* _best) {
    if(_n < 3u || !fude_zoom_fill_inside(_loop, _n, _tap)) {
        return;
    }
    const f64 _area = fude_zoom_fill_area(_loop, _n);
    if(_area < *_best) {
        *_best = _area;
        rde_arr_clear(_out);
        memcpy(rde_arr_add_n(_out, _n), _loop, (usize)_n * sizeof(fude_zoom_v2));
    }
}

u32 fude_zoom_fill_region(const fude_zoom_v2* _p, u32 _n, fude_zoom_v2 _tap, b8 _close, rde_arr* _out) {
    rde_arr_clear(_out);
    if(_n < 3u) {
        return 0;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _path = rde_arr_new(sizeof(fude_zoom_v2), _heap);   // the walk so far, loops taken out
    rde_arr _loop = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    f64     _best = 1e300;
    rde_arr_add(&_path, (any)&_p[0]);
    // Every point (then back to the first, when its ends join): each segment from the walk's end.
    for(u32 _k = 1; _k <= (_close ? _n : _n - 1u); _k++) {
        fude_zoom_v2 _to = _p[_k % _n];
        for(u32 _guard = 0; _guard < 4096u; _guard++) {
            fude_zoom_v2*      _w    = (fude_zoom_v2*)_path.memory;
            const u32          _m    = (u32)rde_arr_length(&_path);
            const fude_zoom_v2 _from = _w[_m - 1u];
            // The earliest segment of the walk this one runs into (not the one it starts from).
            i32 _hit = -1;
            f64 _ht = 2.0, _hu = 0.0;
            for(u32 _j = 0; _j + 2u < _m; _j++) {
                f64 _t, _u;
                if(fude_zoom_fill_cross(_from, _to, _w[_j], _w[_j + 1u], &_t, &_u) && (_hit < 0 || _t < _ht)) {
                    _hit = (i32)_j; _ht = _t; _hu = _u;
                }
            }
            if(_hit < 0) {
                break;
            }
            // The loop it closed: from the crossing round the walk back to it.
            const fude_zoom_v2 _x = { _from.x + (_to.x - _from.x) * _ht, _from.y + (_to.y - _from.y) * _ht };
            rde_arr_clear(&_loop);
            rde_arr_add(&_loop, (any)&_x);
            for(u32 _j = (u32)_hit + 1u; _j < _m; _j++) {
                rde_arr_add(&_loop, (any)&_w[_j]);
            }
            fude_zoom_fill_offer((const fude_zoom_v2*)_loop.memory, (u32)rde_arr_length(&_loop), _tap, _out, &_best);
            // The walk goes on from the crossing.
            _path.count = (usize)_hit + 1u;
            rde_arr_add(&_path, (any)&_x);
            (void)_hu;
        }
        if(_k < _n) {
            rde_arr_add(&_path, (any)&_to);
        }
    }
    // What is left closes back to its start: the last loop (when its ends join).
    if(_close) {
        fude_zoom_fill_offer((const fude_zoom_v2*)_path.memory, (u32)rde_arr_length(&_path), _tap, _out, &_best);
    }
    rde_arr_free(&_path);
    rde_arr_free(&_loop);
    return (u32)rde_arr_length(_out);
}

// --- a sweep's reach as one outline ---------------------------------------------------------------

RDE_INTERNAL f64 fude_zoom_fill_seg_dist2(fude_zoom_v2 _p, fude_zoom_v2 _a, fude_zoom_v2 _b) {
    const f64 _dx = _b.x - _a.x, _dy = _b.y - _a.y, _l2 = _dx * _dx + _dy * _dy;
    f64 _t = _l2 > 0.0 ? ((_p.x - _a.x) * _dx + (_p.y - _a.y) * _dy) / _l2 : 0.0;
    _t = _t < 0.0 ? 0.0 : (_t > 1.0 ? 1.0 : _t);
    const f64 _ex = _a.x + _dx * _t - _p.x, _ey = _a.y + _dy * _t - _p.y;
    return _ex * _ex + _ey * _ey;
}

// Douglas-Peucker on a closed loop, in place: the points kept, how many.
RDE_INTERNAL u32 fude_zoom_fill_simplify(fude_zoom_v2* _p, u32 _n, f64 _tol) {
    if(_n < 8u) {
        return _n;
    }
    u8*  _keep  = (u8*)calloc(_n, 1u);
    u32* _stack = (u32*)malloc((usize)_n * 2u * sizeof(u32));
    if(_keep == NULL || _stack == NULL) {
        free(_keep);
        free(_stack);
        return _n;
    }
    // Split at the point furthest from the first: two open runs.
    u32 _far = 0;
    f64 _best = -1.0;
    for(u32 _i = 1; _i < _n; _i++) {
        const f64 _d = (_p[_i].x - _p[0].x) * (_p[_i].x - _p[0].x) + (_p[_i].y - _p[0].y) * (_p[_i].y - _p[0].y);
        if(_d > _best) { _best = _d; _far = _i; }
    }
    _keep[0] = _keep[_far] = 1u;
    u32 _top = 0;
    _stack[_top++] = 0; _stack[_top++] = _far;
    _stack[_top++] = _far; _stack[_top++] = _n;   // _n: back round to 0
    while(_top > 0) {
        const u32 _b = _stack[--_top], _a = _stack[--_top];
        const fude_zoom_v2 _pa = _p[_a], _pb = _p[_b % _n];
        u32 _worst = 0;
        f64 _wd = -1.0;
        for(u32 _i = _a + 1u; _i < _b; _i++) {
            const f64 _d = fude_zoom_fill_seg_dist2(_p[_i], _pa, _pb);
            if(_d > _wd) { _wd = _d; _worst = _i; }
        }
        if(_wd > _tol * _tol) {
            _keep[_worst] = 1u;
            _stack[_top++] = _a; _stack[_top++] = _worst;
            _stack[_top++] = _worst; _stack[_top++] = _b;
        }
    }
    u32 _m = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        if(_keep[_i]) {
            _p[_m++] = _p[_i];
        }
    }
    free(_keep);
    free(_stack);
    return _m;
}

// The distance from _p to the segment a-b less the width there (eased from _ra to _rb).
RDE_INTERNAL f64 fude_zoom_fill_reach(fude_zoom_v2 _p, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _ra, f64 _rb) {
    const f64 _dx = _b.x - _a.x, _dy = _b.y - _a.y, _l2 = _dx * _dx + _dy * _dy;
    f64 _t = _l2 > 0.0 ? ((_p.x - _a.x) * _dx + (_p.y - _a.y) * _dy) / _l2 : 0.0;
    _t = _t < 0.0 ? 0.0 : (_t > 1.0 ? 1.0 : _t);
    const f64 _ex = _a.x + _dx * _t - _p.x, _ey = _a.y + _dy * _t - _p.y;
    return sqrt(_ex * _ex + _ey * _ey) - (_ra + (_rb - _ra) * _t);
}

RDE_INTERNAL u32 fude_zoom_fill_trace(const fude_zoom_v2* _path, const f64* _radii, u32 _n, f64 _r, rde_arr* _out, rde_arr* _rings);

u32 fude_zoom_fill_sweep(const fude_zoom_v2* _path, u32 _n, f64 _r, rde_arr* _out) {
    return fude_zoom_fill_trace(_path, NULL, _n, _r, _out, NULL);
}

f64 fude_zoom_fill_widths_cell(const f64* _radii, u32 _n) {
    f64 _thin = 1e300, _wide = 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        _thin = fmin(_thin, _radii[_i]);
        _wide = fmax(_wide, _radii[_i]);
    }
    return fmax(_thin, _wide / 4.0) / 3.0;
}

u32 fude_zoom_fill_sweep_widths(const fude_zoom_v2* _path, const f64* _radii, u32 _n, rde_arr* _out, rde_arr* _rings) {
    return fude_zoom_fill_trace(_path, _radii, _n, fude_zoom_fill_widths_cell(_radii, _n) * 3.0, _out, _rings);
}

// The reach of a path (each point's width in _radii, or _r all along) traced:
// the grid _r / 3 fine. Its outermost loop; with _rings, the holes in it too,
// after it, each point's ring (0 the outline, 1... the holes) into _rings.
RDE_INTERNAL u32 fude_zoom_fill_trace(const fude_zoom_v2* _path, const f64* _radii, u32 _n, f64 _r, rde_arr* _out, rde_arr* _rings) {
    rde_arr_clear(_out);
    if(_rings != NULL) {
        rde_arr_clear(_rings);
    }
    if(_n == 0 || !(_r > 0.0)) {
        return 0;
    }
    f64 _wide = _r;
    for(u32 _i = 0; _radii != NULL && _i < _n; _i++) {
        _wide = fmax(_wide, _radii[_i]);
    }
    const f64 _cell = _r / 3.0;
    fude_zoom_box _b = fude_zoom_box_empty();
    for(u32 _i = 0; _i < _n; _i++) {
        _b = fude_zoom_box_union(_b, (fude_zoom_box){ _path[_i].x, _path[_i].y, _path[_i].x, _path[_i].y });
    }
    // The grid's corners: two cells past the reach all round (its edge is outside).
    const f64 _x0 = _b.min_x - _wide - 2.0 * _cell, _y0 = _b.min_y - _wide - 2.0 * _cell;
    const u32 _w = (u32)ceil((_b.max_x - _b.min_x + 2.0 * _wide + 4.0 * _cell) / _cell) + 1u;
    const u32 _h = (u32)ceil((_b.max_y - _b.min_y + 2.0 * _wide + 4.0 * _cell) / _cell) + 1u;
    if((u64)_w * (u64)_h > 4000000u) {
        return 0;   // a sweep too long to trace: left uncut (it is cut in its parts as the pen goes on)
    }
    // The distance to the path less the reach at each corner (stamped segment by segment).
    f32* _f = (f32*)malloc((usize)_w * _h * sizeof(f32));
    if(_f == NULL) {
        return 0;
    }
    for(u32 _i = 0; _i < _w * _h; _i++) {
        _f[_i] = 1e30f;
    }
    for(u32 _s = 0; _s < (_n > 1u ? _n - 1u : 1u); _s++) {
        const u32          _s1 = _n > 1u ? _s + 1u : _s;
        const fude_zoom_v2 _a = _path[_s], _c = _path[_s1];
        const f64 _ra = _radii != NULL ? _radii[_s] : _r, _rc = _radii != NULL ? _radii[_s1] : _r, _rm = fmax(_ra, _rc);
        const i32 _i0 = (i32)floor((fmin(_a.x, _c.x) - _rm - _cell - _x0) / _cell), _i1 = (i32)ceil((fmax(_a.x, _c.x) + _rm + _cell - _x0) / _cell);
        const i32 _j0 = (i32)floor((fmin(_a.y, _c.y) - _rm - _cell - _y0) / _cell), _j1 = (i32)ceil((fmax(_a.y, _c.y) + _rm + _cell - _y0) / _cell);
        for(i32 _j = _j0 < 0 ? 0 : _j0; _j <= _j1 && _j < (i32)_h; _j++) {
            for(i32 _i = _i0 < 0 ? 0 : _i0; _i <= _i1 && _i < (i32)_w; _i++) {
                const fude_zoom_v2 _q = { _x0 + _i * _cell, _y0 + _j * _cell };
                const f32 _d = (f32)fude_zoom_fill_reach(_q, _a, _c, _ra, _rc);
                f32* _at = &_f[(u32)_j * _w + (u32)_i];
                *_at = _d < *_at ? _d : *_at;
            }
        }
    }
    // Marching squares: each cell's crossings of 0 (inside: below), the edges
    // numbered so neighbours share them; then joined into loops.
    // An edge's id: (j * w + i) * 2 for the bottom (i→i+1 at row j), + 1 for the left (j→j+1 at column i).
    rde_arr _segs = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());   // pairs of edge ids, from → to (inside on the left)
    for(u32 _j = 0; _j + 1u < _h; _j++) {
        for(u32 _i = 0; _i + 1u < _w; _i++) {
            const f32 _v[4] = { _f[_j * _w + _i], _f[_j * _w + _i + 1u], _f[(_j + 1u) * _w + _i + 1u], _f[(_j + 1u) * _w + _i] };   // bl br tr tl
            const u32 _code = (_v[0] < 0.0f ? 1u : 0u) | (_v[1] < 0.0f ? 2u : 0u) | (_v[2] < 0.0f ? 4u : 0u) | (_v[3] < 0.0f ? 8u : 0u);
            if(_code == 0u || _code == 15u) {
                continue;
            }
            const u32 _bottom = (_j * _w + _i) * 2u, _left = (_j * _w + _i) * 2u + 1u;
            const u32 _top    = ((_j + 1u) * _w + _i) * 2u, _right = (_j * _w + _i + 1u) * 2u + 1u;
            // Segments with the inside on their left (counter-clockwise round the inside).
            static const i8 TABLE[16][4] = {
                { -1, -1, -1, -1 }, { 0, 3, -1, -1 }, { 1, 0, -1, -1 }, { 1, 3, -1, -1 },
                { 2, 1, -1, -1 },   { 0, 1, 2, 3 },   { 2, 0, -1, -1 }, { 2, 3, -1, -1 },
                { 3, 2, -1, -1 },   { 0, 2, -1, -1 }, { 1, 2, 3, 0 },   { 1, 2, -1, -1 },
                { 3, 1, -1, -1 },   { 0, 1, -1, -1 }, { 3, 0, -1, -1 }, { -1, -1, -1, -1 },
            };
            const u32 _edges[4] = { _bottom, _right, _top, _left };
            for(u32 _k = 0; _k < 4u && TABLE[_code][_k] >= 0; _k += 2u) {
                const u32 _pair[2] = { _edges[TABLE[_code][_k]], _edges[TABLE[_code][_k + 1u]] };
                rde_arr_add(&_segs, (any)&_pair[0]);
                rde_arr_add(&_segs, (any)&_pair[1]);
            }
        }
    }
    // Where each edge's crossing is (between its two corners, by the field), and
    // each segment's successor (the one starting where it ends).
    const u32  _ns   = (u32)rde_arr_length(&_segs) / 2u;
    const u32* _sg   = (const u32*)_segs.memory;
    const u32  _ids  = _w * _h * 2u;
    i32*       _from = (i32*)malloc((usize)_ids * sizeof(i32));
    u8*        _used = (u8*)calloc(_ns > 0 ? _ns : 1u, 1u);
    if(_from == NULL || _used == NULL) {
        free(_from); free(_used); free(_f); rde_arr_free(&_segs);
        return 0;
    }
    for(u32 _i = 0; _i < _ids; _i++) {
        _from[_i] = -1;
    }
    for(u32 _k = 0; _k < _ns; _k++) {
        _from[_sg[_k * 2u]] = (i32)_k;
    }
    rde_arr _loop  = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    rde_arr _holes = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());   // each hole's points, after its count (as x)
    f64 _best = 0.0;
    for(u32 _k = 0; _k < _ns; _k++) {
        if(_used[_k]) {
            continue;
        }
        rde_arr_clear(&_loop);
        u32 _s = _k;
        while(!_used[_s]) {
            _used[_s] = 1u;
            // The crossing on the edge this segment ends at.
            const u32 _e  = _sg[_s * 2u + 1u];
            const u32 _c  = _e / 2u, _i = _c % _w, _j = _c / _w;
            const b8  _up = (_e & 1u) != 0u;
            const f32 _va = _f[_j * _w + _i], _vb = _up ? _f[(_j + 1u) * _w + _i] : _f[_j * _w + _i + 1u];
            const f64 _t  = _va != _vb ? (f64)_va / (f64)(_va - _vb) : 0.5;
            const fude_zoom_v2 _q = { _x0 + ((f64)_i + (_up ? 0.0 : _t)) * _cell, _y0 + ((f64)_j + (_up ? _t : 0.0)) * _cell };
            rde_arr_add(&_loop, (any)&_q);
            const i32 _next = _from[_e];
            if(_next < 0) {
                break;
            }
            _s = (u32)_next;
        }
        const u32 _m = (u32)rde_arr_length(&_loop);
        if(_m < 3u) {
            continue;
        }
        const f64 _area = fude_zoom_fill_area((const fude_zoom_v2*)_loop.memory, _m);
        if(_area > _best) {   // the outermost: the largest (a sweep's reach is one piece)
            _best = _area;
            rde_arr_clear(_out);
            memcpy(rde_arr_add_n(_out, _m), _loop.memory, (usize)_m * sizeof(fude_zoom_v2));
        }
        if(_rings != NULL && _area > _cell * _cell) {   // every loop but specks: the holes are told after
            const fude_zoom_v2 _head = { (f64)_m, fude_zoom_fill_signed_area((const fude_zoom_v2*)_loop.memory, _m) };
            rde_arr_add(&_holes, (any)&_head);
            memcpy(rde_arr_add_n(&_holes, _m), _loop.memory, (usize)_m * sizeof(fude_zoom_v2));
        }
    }
    // The holes: the loops turning the other way from the outline (the inside
    // on the left of each, so round a hole they turn back).
    const f64 _turn = _rings != NULL && rde_arr_length(_out) >= 3u ? fude_zoom_fill_signed_area((const fude_zoom_v2*)_out->memory, (u32)rde_arr_length(_out)) : 0.0;
    rde_arr_free(&_loop);
    free(_from);
    free(_used);
    free(_f);
    rde_arr_free(&_segs);
    u32 _kept = fude_zoom_fill_simplify((fude_zoom_v2*)_out->memory, (u32)rde_arr_length(_out), _cell / 6.0);
    _out->count = _kept;
    if(_rings != NULL && _kept >= 3u) {
        memset(rde_arr_add_n(_rings, _kept), 0, (usize)_kept * sizeof(u32));
        u32 _ring = 1u;
        for(u32 _at = 0; _at < (u32)rde_arr_length(&_holes);) {
            const u32 _m = (u32)((const fude_zoom_v2*)_holes.memory)[_at].x;
            if(((const fude_zoom_v2*)_holes.memory)[_at].y * _turn >= 0.0) {
                _at += _m + 1u;   // the outline itself, or an island
                continue;
            }
            fude_zoom_v2* _h = rde_arr_add_n(_out, _m);
            memcpy(_h, &((const fude_zoom_v2*)_holes.memory)[_at + 1u], (usize)_m * sizeof(fude_zoom_v2));
            const u32 _hk = fude_zoom_fill_simplify(_h, _m, _cell / 6.0);
            _out->count = _kept + _hk;
            if(_hk >= 3u) {
                u32* _r = rde_arr_add_n(_rings, _hk);
                for(u32 _i = 0; _i < _hk; _i++) {
                    _r[_i] = _ring;
                }
                _kept += _hk;
                _ring++;
            } else {
                _out->count = _kept;
            }
            _at += _m + 1u;
        }
    }
    rde_arr_free(&_holes);
    return _kept;
}
