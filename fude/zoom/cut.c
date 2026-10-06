// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/cut.h"
#include "zoom/fill.h"
#include "zoom/shape.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FUDE_ZOOM_CUT_PI 3.14159265358979323846

typedef struct {
    fude_zoom_v2 a, b;
} fude_zoom_cut_edge;

// A point on an edge where it is split: how far along (0..1) and the point itself (shared with the other edge).
typedef struct {
    f64          t;
    fude_zoom_v2 p;
} fude_zoom_cut_split;

// A kept piece of edge, the material on its left: its ends' vertex numbers.
typedef struct {
    u32 from, to;
    b8  used;
} fude_zoom_cut_side;

f64 fude_zoom_cut_area(const fude_zoom_v2* _p, u32 _n) {
    f64 _a = 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_v2 _q = _p[_i], _r = _p[(_i + 1u) % _n];
        _a += _q.x * _r.y - _r.x * _q.y;
    }
    return _a * 0.5;
}

// Edges of rings given as runs (points, each one's ring): each run closed.
RDE_INTERNAL void fude_zoom_cut_edges(const fude_zoom_v2* _p, const u32* _rings, u32 _n, rde_arr* _edges) {
    for(u32 _from = 0; _from < _n;) {
        u32 _to = _from;
        while(_to < _n && (_rings == NULL ? 0u : _rings[_to]) == (_rings == NULL ? 0u : _rings[_from])) {
            _to++;
        }
        if(_to - _from >= 3u) {
            for(u32 _i = _from; _i < _to; _i++) {
                const u32 _j = _i + 1u < _to ? _i + 1u : _from;
                if(_p[_i].x != _p[_j].x || _p[_i].y != _p[_j].y) {
                    const fude_zoom_cut_edge _e = { _p[_i], _p[_j] };
                    rde_arr_add(_edges, (any)&_e);
                }
            }
        }
        _from = _to;
    }
}

RDE_INTERNAL int fude_zoom_cut_by_t(const void* _a, const void* _b) {
    const f64 _x = ((const fude_zoom_cut_split*)_a)->t, _y = ((const fude_zoom_cut_split*)_b)->t;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// A vertex's number by its exact point (the same point, bit for bit, the same number).
typedef struct {
    u64* keys;      // x's bits, y's bits, a pair a slot (0, 0: empty — the point (0, 0) is held apart)
    u32* values;
    u32  capacity;
} fude_zoom_cut_map;

RDE_INTERNAL u64 fude_zoom_cut_hash(u64 _x, u64 _y) {
    u64 _h = _x * 0x9E3779B97F4A7C15ull ^ (_y + 0x632BE59BD9B4E019ull + (_x << 6) + (_x >> 2));
    _h ^= _h >> 31;
    return _h * 0xBF58476D1CE4E5B9ull;
}

RDE_INTERNAL u32 fude_zoom_cut_vertex(fude_zoom_cut_map* _m, rde_arr* _vertices, fude_zoom_v2 _p) {
    u64 _x, _y;
    // (-0 and 0 the same point)
    const f64 _px = _p.x == 0.0 ? 0.0 : _p.x, _py = _p.y == 0.0 ? 0.0 : _p.y;
    memcpy(&_x, &_px, sizeof(u64));
    memcpy(&_y, &_py, sizeof(u64));
    u32 _slot = (u32)(fude_zoom_cut_hash(_x, _y) & (u64)(_m->capacity - 1u));
    for(;;) {
        const u64 _kx = _m->keys[_slot * 2u], _ky = _m->keys[_slot * 2u + 1u];
        if(_m->values[_slot] == 0xFFFFFFFFu) {
            _m->keys[_slot * 2u]      = _x;
            _m->keys[_slot * 2u + 1u] = _y;
            _m->values[_slot]         = (u32)rde_arr_length(_vertices);
            const fude_zoom_v2 _v = { _px, _py };
            rde_arr_add(_vertices, (any)&_v);
            return _m->values[_slot];
        }
        if(_kx == _x && _ky == _y) {
            return _m->values[_slot];
        }
        _slot = (_slot + 1u) & (_m->capacity - 1u);
    }
}

// Inside what is left: the region less every cut (fill.h: the outline's
// inside, less every other ring — its holes and the cuts — however they overlap).
RDE_INTERNAL b8 fude_zoom_cut_material(const fude_zoom_v2* _all, const u32* _all_rings, u32 _all_n, fude_zoom_v2 _p) {
    return fude_zoom_fill_inside_rings(_all, _all_rings, _all_n, _p);
}

u32 fude_zoom_cut_region(const fude_zoom_v2* _points, const u32* _rings, u32 _n, const fude_zoom_v2* _cut, const u32* _cut_rings, u32 _cut_n,
                         rde_arr* _out, rde_arr* _out_rings, rde_arr* _out_piece) {
    rde_arr_clear(_out);
    rde_arr_clear(_out_rings);
    rde_arr_clear(_out_piece);
    if(_n < 3u) {
        return 0;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    // How big it all is: what "touching" and "the same point" mean.
    fude_zoom_box _all_box = fude_zoom_box_empty();
    for(u32 _i = 0; _i < _n + _cut_n; _i++) {
        const fude_zoom_v2 _q = _i < _n ? _points[_i] : _cut[_i - _n];
        _all_box = fude_zoom_box_union(_all_box, (fude_zoom_box){ _q.x, _q.y, _q.x, _q.y });
    }
    const f64 _size = fmax(hypot(_all_box.max_x - _all_box.min_x, _all_box.max_y - _all_box.min_y), 1e-300);
    const f64 _eps  = _size * 1e-9;
    // Everything as one set of rings (the region's, then each cut a ring of its
    // own): what the material test reads. A point next to nothing from the one
    // before it is that one (a loop drawn by hand ends a hair off where it began).
    fude_zoom_v2* _all       = _heap->malloc(_heap->allocator, (usize)(_n + _cut_n) * sizeof(fude_zoom_v2));
    u32*          _all_rings = _heap->malloc(_heap->allocator, (usize)(_n + _cut_n) * sizeof(u32));
    u32 _all_n = 0, _ring_id = 0;
    for(u32 _part = 0; _part < 2u; _part++) {
        const fude_zoom_v2* _src   = _part == 0 ? _points : _cut;
        const u32*          _src_r = _part == 0 ? _rings : _cut_rings;
        const u32           _src_n = _part == 0 ? _n : _cut_n;
        for(u32 _from = 0; _from < _src_n;) {
            u32 _to = _from;
            while(_to < _src_n && (_src_r != NULL ? _src_r[_to] : 0u) == (_src_r != NULL ? _src_r[_from] : 0u)) {
                _to++;
            }
            // Ring 0 the region's outline; every other ring (its holes, the cuts) an id of its own.
            const u32 _id    = _part == 0 && _from == 0 ? 0u : ++_ring_id;
            const u32 _start = _all_n;
            for(u32 _i = _from; _i < _to; _i++) {
                if(_all_n > _start && hypot(_src[_i].x - _all[_all_n - 1u].x, _src[_i].y - _all[_all_n - 1u].y) <= _eps * 100.0) {
                    continue;
                }
                _all[_all_n]       = _src[_i];
                _all_rings[_all_n] = _id;
                _all_n++;
            }
            while(_all_n - _start >= 2u && hypot(_all[_all_n - 1u].x - _all[_start].x, _all[_all_n - 1u].y - _all[_start].y) <= _eps * 100.0) {
                _all_n--;
            }
            if(_all_n - _start < 3u) {
                _all_n = _start;   // (not a ring)
            }
            _from = _to;
        }
    }
    rde_arr _edges = rde_arr_new(sizeof(fude_zoom_cut_edge), _heap);
    fude_zoom_cut_edges(_all, _all_rings, _all_n, &_edges);
    const u32 _e = (u32)rde_arr_length(&_edges);
    const fude_zoom_cut_edge* _ed = (const fude_zoom_cut_edge*)_edges.memory;
    // Where each edge is split: its ends, every crossing, every other edge's end lying on it.
    rde_arr* _splits = _heap->malloc(_heap->allocator, (usize)(_e > 0 ? _e : 1u) * sizeof(rde_arr));
    for(u32 _i = 0; _i < _e; _i++) {
        _splits[_i] = rde_arr_new(sizeof(fude_zoom_cut_split), _heap);
        const fude_zoom_cut_split _s0 = { 0.0, _ed[_i].a }, _s1 = { 1.0, _ed[_i].b };
        rde_arr_add(&_splits[_i], (any)&_s0);
        rde_arr_add(&_splits[_i], (any)&_s1);
    }
    for(u32 _i = 0; _i < _e; _i++) {
        const fude_zoom_v2 _a = _ed[_i].a, _b = _ed[_i].b;
        const fude_zoom_v2 _d = { _b.x - _a.x, _b.y - _a.y };
        const f64 _l2 = _d.x * _d.x + _d.y * _d.y;
        const f64 _ix0 = fmin(_a.x, _b.x) - _eps, _ix1 = fmax(_a.x, _b.x) + _eps, _iy0 = fmin(_a.y, _b.y) - _eps, _iy1 = fmax(_a.y, _b.y) + _eps;
        for(u32 _j = _i + 1u; _j < _e; _j++) {
            const fude_zoom_v2 _c = _ed[_j].a, _f = _ed[_j].b;
            if(fmax(_c.x, _f.x) < _ix0 || fmin(_c.x, _f.x) > _ix1 || fmax(_c.y, _f.y) < _iy0 || fmin(_c.y, _f.y) > _iy1) {
                continue;
            }
            const fude_zoom_v2 _g = { _f.x - _c.x, _f.y - _c.y };
            const f64 _den = _d.x * _g.y - _d.y * _g.x;
            const f64 _m2  = _g.x * _g.x + _g.y * _g.y;
            if(fabs(_den) <= 1e-12 * sqrt(_l2 * _m2)) {
                // Along each other: each split where the other ends on it.
                const f64 _off = fabs((_c.x - _a.x) * _d.y - (_c.y - _a.y) * _d.x) / sqrt(_l2);
                if(_off > _eps) {
                    continue;
                }
                const fude_zoom_v2 _ends_j[2] = { _c, _f }, _ends_i[2] = { _a, _b };
                for(u32 _k = 0; _k < 2u; _k++) {
                    const f64 _t = ((_ends_j[_k].x - _a.x) * _d.x + (_ends_j[_k].y - _a.y) * _d.y) / _l2;
                    if(_t > 1e-12 && _t < 1.0 - 1e-12) {
                        const fude_zoom_cut_split _sp = { _t, _ends_j[_k] };
                        rde_arr_add(&_splits[_i], (any)&_sp);
                    }
                    const f64 _u = ((_ends_i[_k].x - _c.x) * _g.x + (_ends_i[_k].y - _c.y) * _g.y) / _m2;
                    if(_u > 1e-12 && _u < 1.0 - 1e-12) {
                        const fude_zoom_cut_split _sp = { _u, _ends_i[_k] };
                        rde_arr_add(&_splits[_j], (any)&_sp);
                    }
                }
                continue;
            }
            const f64 _t = ((_c.x - _a.x) * _g.y - (_c.y - _a.y) * _g.x) / _den;
            const f64 _u = ((_c.x - _a.x) * _d.y - (_c.y - _a.y) * _d.x) / _den;
            const f64 _tt = 1e-12;
            if(_t < -_tt || _t > 1.0 + _tt || _u < -_tt || _u > 1.0 + _tt) {
                continue;
            }
            // The point: an end itself where it is one (so it is the very same point), else worked out.
            const b8 _i_end = _t <= _tt || _t >= 1.0 - _tt, _j_end = _u <= _tt || _u >= 1.0 - _tt;
            fude_zoom_v2 _p;
            if(_j_end) {
                _p = _u <= _tt ? _c : _f;
            } else if(_i_end) {
                _p = _t <= _tt ? _a : _b;
            } else {
                _p = (fude_zoom_v2){ _a.x + _d.x * _t, _a.y + _d.y * _t };
            }
            if(!_i_end) {
                const fude_zoom_cut_split _sp = { _t, _p };
                rde_arr_add(&_splits[_i], (any)&_sp);
            }
            if(!_j_end) {
                const fude_zoom_cut_split _sp = { _u, _p };
                rde_arr_add(&_splits[_j], (any)&_sp);
            }
        }
    }
    // Each edge's pieces: kept where the material is on one side only, turned to have it on the left.
    const u32 _cap = 1u << (u32)ceil(log2(fmax(64.0, 4.0 * (f64)(_e * 4u + 16u))));
    fude_zoom_cut_map _map = { _heap->malloc(_heap->allocator, (usize)_cap * 2u * sizeof(u64)), _heap->malloc(_heap->allocator, (usize)_cap * sizeof(u32)), _cap };
    memset(_map.values, 0xFF, (usize)_cap * sizeof(u32));
    rde_arr _vertices = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr _sides    = rde_arr_new(sizeof(fude_zoom_cut_side), _heap);
    const f64 _side_eps = _size * 1e-7;
    for(u32 _i = 0; _i < _e; _i++) {
        fude_zoom_cut_split* _sp = (fude_zoom_cut_split*)_splits[_i].memory;
        const u32 _k = (u32)rde_arr_length(&_splits[_i]);
        qsort(_sp, _k, sizeof(fude_zoom_cut_split), fude_zoom_cut_by_t);
        for(u32 _q = 0; _q + 1u < _k; _q++) {
            const fude_zoom_v2 _p0 = _sp[_q].p, _p1 = _sp[_q + 1u].p;
            const f64 _len = hypot(_p1.x - _p0.x, _p1.y - _p0.y);
            if(_len <= _eps) {
                continue;
            }
            const fude_zoom_v2 _m = { (_p0.x + _p1.x) * 0.5, (_p0.y + _p1.y) * 0.5 };
            const fude_zoom_v2 _nl = { -(_p1.y - _p0.y) / _len, (_p1.x - _p0.x) / _len };
            const f64 _o = fmin(_side_eps, _len * 0.25);
            const b8 _left  = fude_zoom_cut_material(_all, _all_rings, _all_n, (fude_zoom_v2){ _m.x + _nl.x * _o, _m.y + _nl.y * _o });
            const b8 _right = fude_zoom_cut_material(_all, _all_rings, _all_n, (fude_zoom_v2){ _m.x - _nl.x * _o, _m.y - _nl.y * _o });
            if(_left == _right) {
                continue;
            }
            const u32 _va = fude_zoom_cut_vertex(&_map, &_vertices, _p0), _vb = fude_zoom_cut_vertex(&_map, &_vertices, _p1);
            if(_va == _vb) {
                continue;
            }
            const fude_zoom_cut_side _side = { _left ? _va : _vb, _left ? _vb : _va, false };
            rde_arr_add(&_sides, (any)&_side);
        }
    }
    for(u32 _i = 0; _i < _e; _i++) {
        rde_arr_free(&_splits[_i]);
    }
    _heap->free(_heap->allocator, _splits);
    // The same piece of edge kept twice (edges along each other): once.
    fude_zoom_cut_side* _sd = (fude_zoom_cut_side*)_sides.memory;
    const u32 _ns = (u32)rde_arr_length(&_sides);
    for(u32 _i = 0; _i < _ns; _i++) {
        for(u32 _j = _i + 1u; _j < _ns && !_sd[_i].used; _j++) {
            if(!_sd[_j].used && _sd[_j].from == _sd[_i].from && _sd[_j].to == _sd[_i].to) {
                _sd[_j].used = true;
            }
        }
    }
    // Outgoing sides of each vertex.
    const u32 _nv = (u32)rde_arr_length(&_vertices);
    const fude_zoom_v2* _vx = (const fude_zoom_v2*)_vertices.memory;
    u32* _first_out = _heap->malloc(_heap->allocator, (usize)(_nv + 1u) * sizeof(u32));
    u32* _order     = _heap->malloc(_heap->allocator, (usize)(_ns > 0 ? _ns : 1u) * sizeof(u32));
    memset(_first_out, 0, (usize)(_nv + 1u) * sizeof(u32));
    for(u32 _i = 0; _i < _ns; _i++) {
        _first_out[_sd[_i].from + 1u]++;
    }
    for(u32 _v = 0; _v < _nv; _v++) {
        _first_out[_v + 1u] += _first_out[_v];
    }
    u32* _fill_at = _heap->malloc(_heap->allocator, (usize)(_nv > 0 ? _nv : 1u) * sizeof(u32));
    memcpy(_fill_at, _first_out, (usize)(_nv > 0 ? _nv : 1u) * sizeof(u32));
    for(u32 _i = 0; _i < _ns; _i++) {
        _order[_fill_at[_sd[_i].from]++] = _i;
    }
    _heap->free(_heap->allocator, _fill_at);
    // Loops: from a side, on and on, at each vertex the side going on that keeps
    // the same material on the left (the first met turning clockwise from back).
    rde_arr _loops = rde_arr_new(sizeof(u32), _heap);     // vertex numbers, loop after loop
    rde_arr _loop_at = rde_arr_new(sizeof(u32), _heap);   // where each loop begins in _loops
    for(u32 _start = 0; _start < _ns; _start++) {
        if(_sd[_start].used) {
            continue;
        }
        const u32 _begin = (u32)rde_arr_length(&_loops);
        u32 _cur = _start;
        b8  _closed = false;
        for(u32 _guard = 0; _guard <= _ns; _guard++) {
            _sd[_cur].used = true;
            rde_arr_add(&_loops, (any)&_sd[_cur].from);
            const u32 _at = _sd[_cur].to;
            if(_at == _sd[_start].from) {
                _closed = true;
                break;
            }
            const fude_zoom_v2 _back = { _vx[_sd[_cur].from].x - _vx[_at].x, _vx[_sd[_cur].from].y - _vx[_at].y };
            const f64 _back_a = atan2(_back.y, _back.x);
            u32 _next = 0xFFFFFFFFu;
            f64 _best = 1e300;
            for(u32 _o = _first_out[_at]; _o < _first_out[_at + 1u]; _o++) {
                const u32 _c = _order[_o];
                if(_sd[_c].used && _c != _start) {
                    continue;
                }
                const fude_zoom_v2 _dir = { _vx[_sd[_c].to].x - _vx[_at].x, _vx[_sd[_c].to].y - _vx[_at].y };
                f64 _cw = fmod(_back_a - atan2(_dir.y, _dir.x) + 4.0 * FUDE_ZOOM_CUT_PI, 2.0 * FUDE_ZOOM_CUT_PI);
                if(_cw < 1e-12) {
                    _cw = 2.0 * FUDE_ZOOM_CUT_PI;
                }
                if(_cw < _best) {
                    _best = _cw;
                    _next = _c;
                }
            }
            if(_next == 0xFFFFFFFFu) {
                // Nothing goes on from here: a side beginning a hair away (numbers' rounding), or the start itself, does.
                const f64 _gap = _size * 1e-6;
                if(hypot(_vx[_at].x - _vx[_sd[_start].from].x, _vx[_at].y - _vx[_sd[_start].from].y) <= _gap) {
                    _closed = true;
                    break;
                }
                f64 _near = _gap;
                for(u32 _c = 0; _c < _ns; _c++) {
                    if(_sd[_c].used) {
                        continue;
                    }
                    const f64 _d = hypot(_vx[_sd[_c].from].x - _vx[_at].x, _vx[_sd[_c].from].y - _vx[_at].y);
                    if(_d <= _near) {
                        _near = _d;
                        _next = _c;
                    }
                }
            }
            if(_next == 0xFFFFFFFFu || _next == _start) {
                _closed = _next == _start;
                break;
            }
            _cur = _next;
        }
            if(_closed && (u32)rde_arr_length(&_loops) - _begin >= 3u) {
            rde_arr_add(&_loop_at, (any)&_begin);
        } else {
            _loops.count = _begin;   // (an open chain: numbers not good enough to close it — left out)
        }
    }
    const u32 _nl = (u32)rde_arr_length(&_loop_at);
    // Each loop's points (straight runs made one), its area.
    rde_arr _lp = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    u32* _lp_at  = _heap->malloc(_heap->allocator, (usize)(_nl + 1u) * sizeof(u32));
    f64* _area   = _heap->malloc(_heap->allocator, (usize)(_nl > 0 ? _nl : 1u) * sizeof(f64));
    for(u32 _l = 0; _l < _nl; _l++) {
        const u32 _from = ((const u32*)_loop_at.memory)[_l];
        const u32 _to   = _l + 1u < _nl ? ((const u32*)_loop_at.memory)[_l + 1u] : (u32)rde_arr_length(&_loops);
        const u32* _ids = (const u32*)_loops.memory;
        _lp_at[_l] = (u32)rde_arr_length(&_lp);
        const u32 _k = _to - _from;
        for(u32 _i = 0; _i < _k; _i++) {
            const fude_zoom_v2 _a = _vx[_ids[_from + (_i + _k - 1u) % _k]], _b = _vx[_ids[_from + _i]], _c = _vx[_ids[_from + (_i + 1u) % _k]];
            const f64 _cross = (_b.x - _a.x) * (_c.y - _b.y) - (_b.y - _a.y) * (_c.x - _b.x);
            const f64 _dot   = (_b.x - _a.x) * (_c.x - _b.x) + (_b.y - _a.y) * (_c.y - _b.y);
            if(fabs(_cross) <= 1e-12 * hypot(_b.x - _a.x, _b.y - _a.y) * hypot(_c.x - _b.x, _c.y - _b.y) && _dot > 0.0) {
                continue;   // a point along a straight run
            }
            rde_arr_add(&_lp, (any)&_b);
        }
        _area[_l] = fude_zoom_cut_area(&((const fude_zoom_v2*)_lp.memory)[_lp_at[_l]], (u32)rde_arr_length(&_lp) - _lp_at[_l]);
    }
    _lp_at[_nl] = (u32)rde_arr_length(&_lp);
    // Outlines (counter-clockwise) a piece each; each hole to the smallest outline round it.
    u32 _pieces = 0;
    u32* _piece_of = _heap->malloc(_heap->allocator, (usize)(_nl > 0 ? _nl : 1u) * sizeof(u32));
    const fude_zoom_v2* _lpp = (const fude_zoom_v2*)_lp.memory;
    for(u32 _l = 0; _l < _nl; _l++) {
        _piece_of[_l] = 0xFFFFFFFFu;
        if(_area[_l] > 0.0 && _lp_at[_l + 1u] - _lp_at[_l] >= 3u) {
            _piece_of[_l] = _pieces++;
        }
    }
    for(u32 _l = 0; _l < _nl; _l++) {
        if(_area[_l] >= 0.0 || _lp_at[_l + 1u] - _lp_at[_l] < 3u) {
            continue;
        }
        // A point just off its first edge on the material's side (its left).
        const fude_zoom_v2 _a = _lpp[_lp_at[_l]], _b = _lpp[_lp_at[_l] + 1u];
        const f64 _len = hypot(_b.x - _a.x, _b.y - _a.y);
        const f64 _o = fmin(_side_eps, _len * 0.25);
        const fude_zoom_v2 _q = { (_a.x + _b.x) * 0.5 - (_b.y - _a.y) / _len * _o, (_a.y + _b.y) * 0.5 + (_b.x - _a.x) / _len * _o };
        u32 _best = 0xFFFFFFFFu;
        f64 _best_area = 1e300;
        for(u32 _o2 = 0; _o2 < _nl; _o2++) {
            if(_piece_of[_o2] == 0xFFFFFFFFu || _area[_o2] >= _best_area) {
                continue;
            }
            if(fude_zoom_fill_inside(&_lpp[_lp_at[_o2]], _lp_at[_o2 + 1u] - _lp_at[_o2], _q)) {
                _best = _o2;
                _best_area = _area[_o2];
            }
        }
        _piece_of[_l] = _best != 0xFFFFFFFFu ? 0x80000000u | _piece_of[_best] : 0xFFFFFFFFu;
    }
    // Out: each piece, its outline then its holes.
    for(u32 _p = 0; _p < _pieces; _p++) {
        u32 _ring = 0;
        for(u32 _pass = 0; _pass < 2u; _pass++) {
            for(u32 _l = 0; _l < _nl; _l++) {
                const b8 _mine = _pass == 0 ? _piece_of[_l] == _p : _piece_of[_l] == (0x80000000u | _p);
                if(!_mine) {
                    continue;
                }
                for(u32 _i = _lp_at[_l]; _i < _lp_at[_l + 1u]; _i++) {
                    rde_arr_add(_out, (any)&_lpp[_i]);
                    rde_arr_add(_out_rings, (any)&_ring);
                    rde_arr_add(_out_piece, (any)&_p);
                }
                _ring++;
            }
        }
    }
    _heap->free(_heap->allocator, _piece_of);
    _heap->free(_heap->allocator, _lp_at);
    _heap->free(_heap->allocator, _area);
    rde_arr_free(&_lp);
    rde_arr_free(&_loops);
    rde_arr_free(&_loop_at);
    _heap->free(_heap->allocator, _first_out);
    _heap->free(_heap->allocator, _order);
    rde_arr_free(&_sides);
    rde_arr_free(&_vertices);
    _heap->free(_heap->allocator, _map.keys);
    _heap->free(_heap->allocator, _map.values);
    rde_arr_free(&_edges);
    _heap->free(_heap->allocator, _all);
    _heap->free(_heap->allocator, _all_rings);
    return _pieces;
}

u32 fude_zoom_cut_band(const fude_zoom_v2* _path, u32 _n, f64 _width, rde_arr* _out) {
    // The line without repeated points.
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_v2* _p = _heap->malloc(_heap->allocator, (usize)(_n > 0 ? _n : 1u) * sizeof(fude_zoom_v2));
    u32 _m = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        if(_m == 0 || hypot(_path[_i].x - _p[_m - 1u].x, _path[_i].y - _p[_m - 1u].y) > _width * 1e-6) {
            _p[_m++] = _path[_i];
        }
    }
    if(_m < 2u || !(_width > 0.0)) {
        _heap->free(_heap->allocator, _p);
        return 0;
    }
    const f64 _h = _width * 0.5;
    const u32 _start = (u32)rde_arr_length(_out);
    // Its left side forwards, its right side back: at each point the sides' offset, mitred (at most four times out).
    for(u32 _side = 0; _side < 2u; _side++) {
        const f64 _s = _side == 0 ? 1.0 : -1.0;
        for(u32 _k = 0; _k < _m; _k++) {
            const u32 _i = _side == 0 ? _k : _m - 1u - _k;
            const fude_zoom_v2 _d0 = _i > 0 ? (fude_zoom_v2){ _p[_i].x - _p[_i - 1u].x, _p[_i].y - _p[_i - 1u].y } : (fude_zoom_v2){ _p[1].x - _p[0].x, _p[1].y - _p[0].y };
            const fude_zoom_v2 _d1 = _i + 1u < _m ? (fude_zoom_v2){ _p[_i + 1u].x - _p[_i].x, _p[_i + 1u].y - _p[_i].y } : _d0;
            const f64 _l0 = hypot(_d0.x, _d0.y), _l1 = hypot(_d1.x, _d1.y);
            const fude_zoom_v2 _n0 = { -_d0.y / _l0, _d0.x / _l0 }, _n1 = { -_d1.y / _l1, _d1.x / _l1 };
            fude_zoom_v2 _mt = { _n0.x + _n1.x, _n0.y + _n1.y };
            const f64 _ml = hypot(_mt.x, _mt.y);
            f64 _len = _h;
            if(_ml > 1e-9) {
                _mt = (fude_zoom_v2){ _mt.x / _ml, _mt.y / _ml };
                const f64 _c = _mt.x * _n0.x + _mt.y * _n0.y;
                _len = _c > 0.25 ? _h / _c : _h * 4.0;
            } else {
                _mt = _n0;   // turned right back: straight out
            }
            const fude_zoom_v2 _q = { _p[_i].x + _mt.x * _len * _s, _p[_i].y + _mt.y * _len * _s };
            rde_arr_add(_out, (any)&_q);
        }
    }
    _heap->free(_heap->allocator, _p);
    return (u32)rde_arr_length(_out) - _start;
}

u32 fude_zoom_cut_board(fude_zoom_scene* _s, u32 _board, const fude_zoom_v2* _cut, const u32* _cut_rings, u32 _n, rde_arr* _born) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _board);
    if(_o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_BOARD || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _n < 3u) {
        return FUDE_ZOOM_NONE;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _num = rde_arr_new(sizeof(f64), _heap), _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap), _rings = rde_arr_new(sizeof(u32), _heap);
    const u32 _count = fude_zoom_scene_shape_numbers_all(_s, _board, &_num);
    const f64* _nn = (const f64*)_num.memory;
    const u32 _np = _count >= 4u ? fude_zoom_board_rings(_nn, _count, &_pts, &_rings) : 0u;
    u32 _made = FUDE_ZOOM_NONE;
    if(_np >= 3u) {
        // The cuts in the board's own units.
        const fude_zoom_object _look = *_o;
        const fude_zoom_sim _back = fude_zoom_sim_inverse(fude_zoom_object_sim(&_look));
        fude_zoom_v2* _local = _heap->malloc(_heap->allocator, (usize)_n * sizeof(fude_zoom_v2));
        fude_zoom_box _cb = fude_zoom_box_empty(), _bb = fude_zoom_box_empty();
        for(u32 _i = 0; _i < _n; _i++) {
            _local[_i] = fude_zoom_sim_apply(_back, _cut[_i]);
            _cb = fude_zoom_box_union(_cb, (fude_zoom_box){ _local[_i].x, _local[_i].y, _local[_i].x, _local[_i].y });
        }
        const fude_zoom_v2* _bp = (const fude_zoom_v2*)_pts.memory;
        f64 _before = 0.0;
        for(u32 _from = 0; _from < _np;) {
            u32 _to = _from;
            while(_to < _np && ((const u32*)_rings.memory)[_to] == ((const u32*)_rings.memory)[_from]) {
                _to++;
            }
            _before += fude_zoom_cut_area(&_bp[_from], _to - _from);
            _from = _to;
        }
        for(u32 _i = 0; _i < _np; _i++) {
            _bb = fude_zoom_box_union(_bb, (fude_zoom_box){ _bp[_i].x, _bp[_i].y, _bp[_i].x, _bp[_i].y });
        }
        if(_cb.max_x >= _bb.min_x && _cb.min_x <= _bb.max_x && _cb.max_y >= _bb.min_y && _cb.min_y <= _bb.max_y) {
            rde_arr _out = rde_arr_new(sizeof(fude_zoom_v2), _heap), _out_rings = rde_arr_new(sizeof(u32), _heap), _out_piece = rde_arr_new(sizeof(u32), _heap);
            const u32 _pieces = fude_zoom_cut_region(_bp, (const u32*)_rings.memory, _np, _local, _cut_rings, _n, &_out, &_out_rings, &_out_piece);
            const fude_zoom_v2* _op = (const fude_zoom_v2*)_out.memory;
            const u32* _or = (const u32*)_out_rings.memory;
            const u32* _oc = (const u32*)_out_piece.memory;
            const u32  _on = (u32)rde_arr_length(&_out);
            f64 _after = 0.0;
            for(u32 _from = 0; _from < _on;) {
                u32 _to = _from;
                while(_to < _on && _or[_to] == _or[_from] && _oc[_to] == _oc[_from]) {
                    _to++;
                }
                _after += fude_zoom_cut_area(&_op[_from], _to - _from);
                _from = _to;
            }
            // What the cuts could take at most: their areas together. Less left than that would be numbers gone wrong (a loop lost): no change then.
            f64 _cuts = 0.0;
            for(u32 _from = 0; _from < _n;) {
                u32 _to = _from;
                while(_to < _n && (_cut_rings != NULL ? _cut_rings[_to] : 0u) == (_cut_rings != NULL ? _cut_rings[_from] : 0u)) {
                    _to++;
                }
                _cuts += fabs(fude_zoom_cut_area(&_local[_from], _to - _from));
                _from = _to;
            }
            const b8 _sane = _after <= _before + fabs(_before) * 1e-6 && _after >= _before - _cuts - fabs(_before) * 1e-6;
            if(_sane && !(_pieces == 1u && fabs(_after - _before) <= fabs(_before) * 1e-9)) {
                // Each piece a board where it lies: its box's middle its translation.
                c8 _name[FUDE_ZOOM_BOARD_NAME];
                fude_zoom_board_name(_nn, _count, _name, sizeof(_name));
                const fude_zoom_sim _to_frame = fude_zoom_object_sim(&_look);
                rde_arr _numbers = rde_arr_new(sizeof(f64), _heap), _piece = rde_arr_new(sizeof(fude_zoom_v2), _heap), _piece_rings = rde_arr_new(sizeof(u32), _heap);
                const u16 _was = _s->layer;
                _s->layer = _look.layer;
                _made = 0;
                for(u32 _p = 0; _p < _pieces; _p++) {
                    rde_arr_clear(&_piece);
                    rde_arr_clear(&_piece_rings);
                    fude_zoom_box _box = fude_zoom_box_empty();
                    f64 _area = 0.0;
                    u32 _ring_count = 0, _last = 0xFFFFFFFFu;
                    for(u32 _i = 0; _i < _on; _i++) {
                        if(_oc[_i] != _p) {
                            continue;
                        }
                        rde_arr_add(&_piece, (any)&_op[_i]);
                        rde_arr_add(&_piece_rings, (any)&_or[_i]);
                        _ring_count += _or[_i] != _last ? 1u : 0u;
                        _last = _or[_i];
                        if(_or[_i] == 0u) {
                            _box = fude_zoom_box_union(_box, (fude_zoom_box){ _op[_i].x, _op[_i].y, _op[_i].x, _op[_i].y });
                        }
                    }
                    const u32 _k = (u32)rde_arr_length(&_piece);
                    fude_zoom_v2* _pp = (fude_zoom_v2*)_piece.memory;
                    for(u32 _from = 0; _from < _k;) {
                        u32 _to = _from;
                        while(_to < _k && ((const u32*)_piece_rings.memory)[_to] == ((const u32*)_piece_rings.memory)[_from]) {
                            _to++;
                        }
                        _area += fude_zoom_cut_area(&_pp[_from], _to - _from);
                        _from = _to;
                    }
                    if(_k < 3u || !(_area > fabs(_before) * 1e-9)) {
                        continue;   // (a sliver of nothing)
                    }
                    const fude_zoom_v2 _mid = { (_box.min_x + _box.max_x) * 0.5, (_box.min_y + _box.max_y) * 0.5 };
                    for(u32 _i = 0; _i < _k; _i++) {
                        _pp[_i].x -= _mid.x;
                        _pp[_i].y -= _mid.y;
                    }
                    const f64 _hl = (_box.max_x - _box.min_x) * 0.5, _hw = (_box.max_y - _box.min_y) * 0.5;
                    // Still a plain rectangle (four corners, its box's area): a plain board.
                    const b8 _plain = _ring_count == 1u && _k == 4u && fabs(_area - 4.0 * _hl * _hw) <= _area * 1e-9;
                    f64 _small[FUDE_ZOOM_SHAPE_NUMBERS];
                    const f64* _write;
                    u32 _wn;
                    if(_plain) {
                        _wn    = fude_zoom_board_numbers(_small, _hl, _hw, _nn[2], fude_zoom_board_look(_nn, _count), _name);
                        _write = _small;
                    } else {
                        _wn    = fude_zoom_board_cut_numbers(&_numbers, _hl, _hw, _nn[2], fude_zoom_board_look(_nn, _count), _name, _pp, (const u32*)_piece_rings.memory, _k);
                        _write = (const f64*)_numbers.memory;
                    }
                    const fude_zoom_place _place = { fude_zoom_sim_apply(_to_frame, _mid), _look.rotation, _look.scale };
                    const u32 _new = fude_zoom_scene_add_shape_fill(_s, _look.frame, _place, FUDE_ZOOM_SHAPE_BOARD, _write, _wn, _look.color, _look.radius,
                                                                    _look.flags, _look.fill, _look.z);
                    rde_arr_add(_born, (any)&_new);
                    _made++;
                }
                _s->layer = _was;
                rde_arr_free(&_numbers);
                rde_arr_free(&_piece);
                rde_arr_free(&_piece_rings);
            }
            rde_arr_free(&_out);
            rde_arr_free(&_out_rings);
            rde_arr_free(&_out_piece);
        }
        _heap->free(_heap->allocator, _local);
    }
    rde_arr_free(&_num);
    rde_arr_free(&_pts);
    rde_arr_free(&_rings);
    return _made;
}
