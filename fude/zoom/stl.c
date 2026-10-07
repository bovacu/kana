// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/stl.h"
#include "zoom/export.h"
#include "zoom/fill.h"
#include "zoom/shape.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See stl.h.
// ===========================================================================

// A ring of the rings handed in: where its points start, how many, its area (signed: counter-clockwise +).
typedef struct {
    u32 first, count;
    f64 area;
    u32 depth;     // rings it is inside
    u32 parent;    // the smallest of them (FUDE_ZOOM_NONE: none)
} fude_zoom_stl_ring;

RDE_INTERNAL f64 fude_zoom_stl_area(const fude_zoom_v2* _p, u32 _n) {
    f64 _a = 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_v2 _u = _p[_i], _v = _p[(_i + 1u) % _n];
        _a += _u.x * _v.y - _v.x * _u.y;
    }
    return _a * 0.5;
}

// A triangle, its corners (x, y, z), into _out (nine f32s).
RDE_INTERNAL void fude_zoom_stl_tri(rde_arr* _out, fude_zoom_v2 _a, f64 _za, fude_zoom_v2 _b, f64 _zb, fude_zoom_v2 _c, f64 _zc) {
    f32* _t = rde_arr_add_n(_out, 9u);
    _t[0] = (f32)_a.x; _t[1] = (f32)_a.y; _t[2] = (f32)_za;
    _t[3] = (f32)_b.x; _t[4] = (f32)_b.y; _t[5] = (f32)_zb;
    _t[6] = (f32)_c.x; _t[7] = (f32)_c.y; _t[8] = (f32)_zc;
}

// --- the top's triangles: ears clipped, the holes joined to the outside first ---------------------------

RDE_INTERNAL f64 fude_zoom_stl_cross(fude_zoom_v2 _a, fude_zoom_v2 _b, fude_zoom_v2 _c) {
    return (_b.x - _a.x) * (_c.y - _a.y) - (_b.y - _a.y) * (_c.x - _a.x);
}

RDE_INTERNAL b8 fude_zoom_stl_in_tri(fude_zoom_v2 _a, fude_zoom_v2 _b, fude_zoom_v2 _c, fude_zoom_v2 _p) {
    return fude_zoom_stl_cross(_a, _b, _p) >= 0.0 && fude_zoom_stl_cross(_b, _c, _p) >= 0.0 && fude_zoom_stl_cross(_c, _a, _p) >= 0.0;
}

// The outside (counter-clockwise, _n points) with each hole (clockwise) joined in by a cut there and back: one ring
// round all of it, into _out (cleared first). Each hole by its rightmost point to a point of the ring it can see,
// the holes rightmost first (as the ears' way of it goes: Eberly's).
RDE_INTERNAL void fude_zoom_stl_join(const fude_zoom_v2* _outside, u32 _n, const fude_zoom_v2* const* _holes, const u32* _counts, u32 _h, rde_arr* _out) {
    rde_arr_clear(_out);
    memcpy(rde_arr_add_n(_out, _n), _outside, (usize)_n * sizeof(fude_zoom_v2));
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _order_arr = rde_arr_new(sizeof(u32), _heap), _right_arr = rde_arr_new(sizeof(u32), _heap), _ins_arr = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr_resize(&_order_arr, _h);
    rde_arr_resize(&_right_arr, _h);
    u32* _order = (u32*)_order_arr.memory;   // (sized once: they stay put)
    u32* _right = (u32*)_right_arr.memory;
    for(u32 _i = 0; _i < _h; _i++) {
        _order[_i] = _i;
        _right[_i] = 0;
        for(u32 _k = 1; _k < _counts[_i]; _k++) {
            _right[_i] = _holes[_i][_k].x > _holes[_i][_right[_i]].x ? _k : _right[_i];
        }
    }
    for(u32 _i = 1; _i < _h; _i++) {
        for(u32 _j = _i; _j > 0u && _holes[_order[_j]][_right[_order[_j]]].x > _holes[_order[_j - 1u]][_right[_order[_j - 1u]]].x; _j--) {
            const u32 _t = _order[_j]; _order[_j] = _order[_j - 1u]; _order[_j - 1u] = _t;
        }
    }
    for(u32 _o = 0; _o < _h; _o++) {
        const u32 _hi = _order[_o];
        const fude_zoom_v2 _m = _holes[_hi][_right[_hi]];
        fude_zoom_v2* _r = (fude_zoom_v2*)_out->memory;
        const u32 _rn = (u32)rde_arr_length(_out);
        // The ring's side nearest to the right of M along its level.
        f64 _best_x = 1e300;
        u32 _edge = FUDE_ZOOM_NONE;
        fude_zoom_v2 _hit = { 0.0, 0.0 };
        for(u32 _i = 0; _i < _rn; _i++) {
            const fude_zoom_v2 _a = _r[_i], _b = _r[(_i + 1u) % _rn];
            if((_a.y > _m.y) == (_b.y > _m.y) && !(_a.y == _m.y && _b.y == _m.y)) {
                if(!(_a.y == _m.y || _b.y == _m.y)) {
                    continue;
                }
            }
            if(_a.y == _b.y) {
                continue;
            }
            const f64 _t = (_m.y - _a.y) / (_b.y - _a.y);
            if(_t < 0.0 || _t > 1.0) {
                continue;
            }
            const f64 _x = _a.x + (_b.x - _a.x) * _t;
            if(_x >= _m.x && _x < _best_x) {
                _best_x = _x;
                _edge   = _i;
                _hit    = (fude_zoom_v2){ _x, _m.y };
            }
        }
        if(_edge == FUDE_ZOOM_NONE) {
            continue;   // (not inside: left out)
        }
        // P: the side's end furthest right; if a corner of the ring lies in the triangle M, hit, P, the one of them
        // making the least angle with the level instead.
        u32 _p = _r[_edge].x > _r[(_edge + 1u) % _rn].x ? _edge : (_edge + 1u) % _rn;
        f64 _least = 1e300;
        for(u32 _i = 0; _i < _rn; _i++) {
            if(_i == _p) {
                continue;
            }
            const fude_zoom_v2 _q = _r[_i];
            const b8 _in = fude_zoom_stl_in_tri(_m, _hit, _r[_p], _q) || fude_zoom_stl_in_tri(_m, _r[_p], _hit, _q);
            if(!_in || (_q.x == _m.x && _q.y == _m.y)) {
                continue;
            }
            const f64 _ang = fabs(atan2(_q.y - _m.y, _q.x - _m.x));
            if(_ang < _least) {
                _least = _ang;
                _p     = _i;
            }
        }
        // The ring: up to P, P, then the hole from M round to M, then back to P.
        const u32 _hn = _counts[_hi];
        rde_arr_resize(&_ins_arr, (usize)_hn + 2u);
        fude_zoom_v2* _ins = (fude_zoom_v2*)_ins_arr.memory;
        for(u32 _k = 0; _k <= _hn; _k++) {
            _ins[_k] = _holes[_hi][(_right[_hi] + _k) % _hn];
        }
        _ins[_hn + 1u] = _r[_p];
        for(u32 _k = 0; _k < _hn + 2u; _k++) {
            rde_arr_insert(_out, _p + 1u + _k, (any)&_ins[_k]);
        }
    }
    rde_arr_free(&_ins_arr);
    rde_arr_free(&_order_arr);
    rde_arr_free(&_right_arr);
}

// A ring (counter-clockwise, simple but for its cuts there and back) as triangles, ears clipped one by one, into _out
// (three points each; appended). How many.
RDE_INTERNAL u32 fude_zoom_stl_ears(const fude_zoom_v2* _p, u32 _n, rde_arr* _out) {
    if(_n < 3u) {
        return 0u;
    }
    rde_arr _links = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_links, (usize)_n * 2u);
    u32* _next = (u32*)_links.memory;
    u32* _prev = _next + _n;
    for(u32 _i = 0; _i < _n; _i++) {
        _next[_i] = (_i + 1u) % _n;
        _prev[_i] = (_i + _n - 1u) % _n;
    }
    u32 _left = _n, _at = 0, _since = 0, _made = 0;
    while(_left > 3u && _since < 2u * _left + 4u) {
        const u32 _a = _prev[_at], _b = _at, _c = _next[_at];
        const f64 _cr = fude_zoom_stl_cross(_p[_a], _p[_b], _p[_c]);
        b8 _ear = _cr > 0.0;
        for(u32 _k = _next[_c]; _ear && _k != _a; _k = _next[_k]) {
            // (a point of the ring in it — the same point as a corner, a cut's other side, is not)
            const fude_zoom_v2 _q = _p[_k];
            const b8 _corner = (_q.x == _p[_a].x && _q.y == _p[_a].y) || (_q.x == _p[_b].x && _q.y == _p[_b].y) || (_q.x == _p[_c].x && _q.y == _p[_c].y);
            _ear = _corner || !fude_zoom_stl_in_tri(_p[_a], _p[_b], _p[_c], _q);
        }
        if(_ear || (_since >= _left && _cr >= 0.0) || _since >= 2u * _left) {
            if(_cr > 0.0) {
                fude_zoom_v2* _t = rde_arr_add_n(_out, 3u);
                _t[0] = _p[_a]; _t[1] = _p[_b]; _t[2] = _p[_c];
                _made++;
            }
            _next[_a] = _c;
            _prev[_c] = _a;
            _left--;
            _at    = _a;
            _since = 0;
            continue;
        }
        _at = _c;
        _since++;
    }
    if(_left == 3u) {
        const u32 _a = _prev[_at], _b = _at, _c = _next[_at];
        if(fude_zoom_stl_cross(_p[_a], _p[_b], _p[_c]) > 0.0) {
            fude_zoom_v2* _t = rde_arr_add_n(_out, 3u);
            _t[0] = _p[_a]; _t[1] = _p[_b]; _t[2] = _p[_c];
            _made++;
        }
    }
    rde_arr_free(&_links);
    return _made;
}

// An edge of the top's (its corners kept to a millionth of a millimetre, the lower first), and the way it goes.
typedef struct {
    i64 ax, ay, bx, by;
    u32 tri, side;
} fude_zoom_stl_edge;

RDE_INTERNAL int fude_zoom_stl_edge_cmp(const void* _a, const void* _b) {
    const fude_zoom_stl_edge* _x = (const fude_zoom_stl_edge*)_a;
    const fude_zoom_stl_edge* _y = (const fude_zoom_stl_edge*)_b;
    const i64 _d[4] = { _x->ax - _y->ax, _x->ay - _y->ay, _x->bx - _y->bx, _x->by - _y->by };
    for(u32 _i = 0; _i < 4u; _i++) {
        if(_d[_i] != 0) {
            return _d[_i] < 0 ? -1 : 1;
        }
    }
    return 0;
}

RDE_INTERNAL void fude_zoom_stl_walls(rde_arr* _out, const fude_zoom_v2* _tp, u32 _t, f64 _h) {
    if(_t == 0u) {
        return;
    }
    rde_arr _e_arr = rde_arr_new(sizeof(fude_zoom_stl_edge), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_e_arr, (usize)_t * 3u);
    fude_zoom_stl_edge* _e = (fude_zoom_stl_edge*)_e_arr.memory;
    for(u32 _k = 0; _k < _t; _k++) {
        for(u32 _s = 0; _s < 3u; _s++) {
            const fude_zoom_v2 _a = _tp[3u * _k + _s], _b = _tp[3u * _k + (_s + 1u) % 3u];
            i64 _q[4] = { (i64)llround(_a.x * 1e6), (i64)llround(_a.y * 1e6), (i64)llround(_b.x * 1e6), (i64)llround(_b.y * 1e6) };
            if(_q[2] < _q[0] || (_q[2] == _q[0] && _q[3] < _q[1])) {
                const i64 _x = _q[0], _y = _q[1];
                _q[0] = _q[2]; _q[1] = _q[3]; _q[2] = _x; _q[3] = _y;
            }
            _e[3u * _k + _s] = (fude_zoom_stl_edge){ _q[0], _q[1], _q[2], _q[3], _k, _s };
        }
    }
    qsort(_e, (usize)_t * 3u, sizeof(fude_zoom_stl_edge), fude_zoom_stl_edge_cmp);
    for(u32 _i = 0; _i < _t * 3u;) {
        u32 _j = _i + 1u;
        while(_j < _t * 3u && fude_zoom_stl_edge_cmp(&_e[_i], &_e[_j]) == 0) {
            _j++;
        }
        if(_j - _i == 1u) {
            // A side only one triangle has: the wall below it, facing out (the top's way round it, turned back).
            const fude_zoom_v2 _a = _tp[3u * _e[_i].tri + _e[_i].side], _b = _tp[3u * _e[_i].tri + (_e[_i].side + 1u) % 3u];
            if(fabs(_b.x - _a.x) + fabs(_b.y - _a.y) > 0.0) {
                fude_zoom_stl_tri(_out, _a, 0.0, _b, 0.0, _b, _h);
                fude_zoom_stl_tri(_out, _a, 0.0, _b, _h, _a, _h);
            }
        }
        _i = _j;
    }
    rde_arr_free(&_e_arr);
}

u32 fude_zoom_stl_solids(const fude_zoom_v2* _points, const u32* _rings, u32 _n, const f64* _heights, rde_arr* _out, u32* _solids) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _list = rde_arr_new(sizeof(fude_zoom_stl_ring), _heap);
    for(u32 _from = 0; _from < _n;) {
        u32 _to = _from;
        while(_to < _n && _rings[_to] == _rings[_from]) {
            _to++;
        }
        if(_to - _from >= 3u) {
            const fude_zoom_stl_ring _r = { _from, _to - _from, fude_zoom_stl_area(&_points[_from], _to - _from), 0u, FUDE_ZOOM_NONE };
            if(fabs(_r.area) > 1e-9) {
                rde_arr_add(&_list, (any)&_r);
            }
        }
        _from = _to;
    }
    fude_zoom_stl_ring* _r = (fude_zoom_stl_ring*)_list.memory;
    const u32 _nr = (u32)rde_arr_length(&_list);
    // How deep each is: the larger rings round its first point; the smallest of them its parent.
    for(u32 _i = 0; _i < _nr; _i++) {
        f64 _least = 1e300;
        for(u32 _j = 0; _j < _nr; _j++) {
            if(_j == _i || fabs(_r[_j].area) <= fabs(_r[_i].area) || !fude_zoom_fill_inside(&_points[_r[_j].first], _r[_j].count, _points[_r[_i].first])) {
                continue;
            }
            _r[_i].depth++;
            if(fabs(_r[_j].area) < _least) {
                _least = fabs(_r[_j].area);
                _r[_i].parent = _j;
            }
        }
    }
    // Each outside (an even depth) with its holes (the odd ones it is the parent of): its top and bottom (the outside
    // counter-clockwise, its holes clockwise, together as the fill's rings) and its walls.
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap), _ids = rde_arr_new(sizeof(u32), _heap), _tris = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr _holes_arr = rde_arr_new(sizeof(const fude_zoom_v2*), _heap), _counts_arr = rde_arr_new(sizeof(u32), _heap);
    const u32 _before = (u32)rde_arr_length(_out) / 9u;
    u32 _made = 0;
    for(u32 _i = 0; _i < _nr; _i++) {
        if(_r[_i].depth % 2u != 0u) {
            continue;
        }
        const f64 _h = _heights[_rings[_r[_i].first]];
        if(!(_h > 0.0)) {
            continue;
        }
        rde_arr_clear(&_pts);
        rde_arr_clear(&_ids);
        // Its outside (counter-clockwise) and its holes (clockwise), each copied the right way round.
        u32 _nh = 0;
        for(u32 _j = 0; _j < _nr; _j++) {
            if(_j != _i && !(_r[_j].parent == _i && _r[_j].depth % 2u == 1u)) {
                continue;
            }
            const b8 _outside = _j == _i;
            const b8 _turn = _outside ? _r[_j].area < 0.0 : _r[_j].area > 0.0;
            fude_zoom_v2* _q = rde_arr_add_n(&_pts, _r[_j].count);
            for(u32 _k = 0; _k < _r[_j].count; _k++) {
                _q[_k] = _points[_r[_j].first + (_turn ? _r[_j].count - 1u - _k : _k)];
            }
            const u32 _rec[2] = { _outside ? 0u : 1u + _nh, _r[_j].count };
            rde_arr_add(&_ids, (any)&_rec[0]);
            rde_arr_add(&_ids, (any)&_rec[1]);
            _nh += _outside ? 0u : 1u;
        }
        // (the outside first, then each hole: their starts in what was copied)
        const fude_zoom_v2* _all = (const fude_zoom_v2*)_pts.memory;
        const u32* _rec = (const u32*)_ids.memory;
        const fude_zoom_v2* _outside_p = NULL;
        u32 _outside_n = 0;
        rde_arr_resize(&_holes_arr, _nh);
        rde_arr_resize(&_counts_arr, _nh);
        const fude_zoom_v2** _holes = (const fude_zoom_v2**)_holes_arr.memory;
        u32* _counts = (u32*)_counts_arr.memory;
        u32 _off = 0, _hk = 0;
        for(u32 _j = 0; _j < (u32)rde_arr_length(&_ids) / 2u; _j++) {
            if(_rec[2u * _j] == 0u) {
                _outside_p = &_all[_off];
                _outside_n = _rec[2u * _j + 1u];
            } else {
                _holes[_hk]  = &_all[_off];
                _counts[_hk] = _rec[2u * _j + 1u];
                _hk++;
            }
            _off += _rec[2u * _j + 1u];
        }
        rde_arr _ring = rde_arr_new(sizeof(fude_zoom_v2), _heap);
        fude_zoom_stl_join(_outside_p, _outside_n, _holes, _counts, _hk, &_ring);
        rde_arr_clear(&_tris);
        const u32 _t = fude_zoom_stl_ears((const fude_zoom_v2*)_ring.memory, (u32)rde_arr_length(&_ring), &_tris);
        rde_arr_free(&_ring);
        fude_zoom_v2* _tp = (fude_zoom_v2*)_tris.memory;
        for(u32 _k = 0; _k < _t; _k++) {
            fude_zoom_v2* _c3 = &_tp[3u * _k];
            if((_c3[1].x - _c3[0].x) * (_c3[2].y - _c3[0].y) - (_c3[1].y - _c3[0].y) * (_c3[2].x - _c3[0].x) < 0.0) {
                const fude_zoom_v2 _s = _c3[1]; _c3[1] = _c3[2]; _c3[2] = _s;   // (counter-clockwise from above)
            }
            fude_zoom_stl_tri(_out, _c3[0], _h, _c3[1], _h, _c3[2], _h);     // its top, facing up
            fude_zoom_stl_tri(_out, _c3[0], 0.0, _c3[2], 0.0, _c3[1], 0.0);  // its bottom, facing down
        }
        // Its walls on its top's own edges (the triangles' sides only one triangle has: its rings as the triangles cut
        // them, so the walls meet the top and the bottom edge to edge — a closed surface a slicer takes whole).
        fude_zoom_stl_walls(_out, _tp, _t, _h);
        _made++;
    }
    rde_arr_free(&_pts);
    rde_arr_free(&_ids);
    rde_arr_free(&_tris);
    rde_arr_free(&_holes_arr);
    rde_arr_free(&_counts_arr);
    rde_arr_free(&_list);
    if(_solids != NULL) {
        *_solids = _made;
    }
    return (u32)rde_arr_length(_out) / 9u - _before;
}

// --- from the view (the export walk's sink) -----------------------------------------------------

typedef struct {
    rde_arr TYPE(fude_zoom_v2) pts;
    rde_arr TYPE(u32)          rings;
    rde_arr TYPE(f64)          heights;   // each ring's
    u32 next;          // the next ring's id
    f64 mm;            // millimetres a screen point
    f64 thickness;     // anything but a board's
    f64 height;        // this thing's
    b8  skip;          // this thing makes no solid
    b8  filled;        // this thing's fill came (a cut board's): its outlines after it are the same rings
} fude_zoom_stl_w;

RDE_INTERNAL void fude_zoom_stl_on_object(void* _self, const fude_zoom_scene* _s, u32 _object) {
    fude_zoom_stl_w* _w = (fude_zoom_stl_w*)_self;
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    _w->filled = false;
    _w->height = _w->thickness;
    _w->skip   = _o->kind != FUDE_ZOOM_KIND_SHAPE && _o->kind != FUDE_ZOOM_KIND_FILL;
    if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
        const u8 _t = _o->channels;
        _w->skip = _t == FUDE_ZOOM_SHAPE_SYMBOL || _t == FUDE_ZOOM_SHAPE_SHEET || _t == FUDE_ZOOM_SHAPE_GUIDE || fude_zoom_shape_is_attribute(_t) || _t == FUDE_ZOOM_SHAPE_DIMENSION || _t == FUDE_ZOOM_SHAPE_RADIAL ||
                   _t == FUDE_ZOOM_SHAPE_ANGLE || _t == FUDE_ZOOM_SHAPE_ARROW || _t == FUDE_ZOOM_SHAPE_LINE || _t == FUDE_ZOOM_SHAPE_ARC;
        if(_t == FUDE_ZOOM_SHAPE_BOARD) {
            f64 _n[3];
            if(fude_zoom_scene_shape_numbers(_s, _object, _n, 3u) >= 3u && _n[2] > 0.0) {
                _w->height = _n[2];   // (a board's thickness: millimetres)
            }
        }
    }
}

// One ring of points (screen) as millimetres, its repeated points dropped.
RDE_INTERNAL void fude_zoom_stl_ring_add(fude_zoom_stl_w* _w, const fude_zoom_v2* _p, u32 _n) {
    const u32 _id = _w->next++;
    u32 _kept = 0;
    fude_zoom_v2 _last = { 1e300, 1e300 }, _first = { 0.0, 0.0 };
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_v2 _q = { _p[_i].x * _w->mm, _p[_i].y * _w->mm };
        if(fabs(_q.x - _last.x) < 1e-6 && fabs(_q.y - _last.y) < 1e-6) {
            continue;
        }
        if(_kept > 0u && _i + 1u == _n && fabs(_q.x - _first.x) < 1e-6 && fabs(_q.y - _first.y) < 1e-6) {
            continue;   // (its first again: it closes on its own)
        }
        _first = _kept == 0u ? _q : _first;
        rde_arr_add(&_w->pts, (any)&_q);
        rde_arr_add(&_w->rings, (any)&_id);
        _last = _q;
        _kept++;
    }
    rde_arr_add(&_w->heights, (any)&_w->height);
}

RDE_INTERNAL void fude_zoom_stl_shape(void* _self, const fude_zoom_v2* _p, u32 _n, b8 _closed, f64 _radius, rde_color _line, rde_color _fill) {
    RDE_UNUSED(_radius); RDE_UNUSED(_line); RDE_UNUSED(_fill);
    fude_zoom_stl_w* _w = (fude_zoom_stl_w*)_self;
    if(_w->skip || _w->filled || !_closed || _n < 3u) {
        return;
    }
    fude_zoom_stl_ring_add(_w, _p, _n);
    _w->skip = true;   // (one outline a thing: a connector's heads, a symbol's parts are not its)
}

RDE_INTERNAL void fude_zoom_stl_fill(void* _self, const fude_zoom_v2* _p, const u32* _rings, u32 _n, rde_color _c) {
    RDE_UNUSED(_c);
    fude_zoom_stl_w* _w = (fude_zoom_stl_w*)_self;
    if(_w->skip) {
        return;
    }
    for(u32 _from = 0; _from < _n;) {
        u32 _to = _from;
        while(_to < _n && _rings[_to] == _rings[_from]) {
            _to++;
        }
        fude_zoom_stl_ring_add(_w, &_p[_from], _to - _from);
        _from = _to;
    }
    _w->filled = true;
}

b8 fude_zoom_export_stl(const fude_zoom_scene* _s, fude_zoom_v2 _half, f64 _mm_per_point, f64 _thickness_mm, fude_bytes* _out, fude_zoom_stl_said* _said) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_stl_w _w = {
        .pts = rde_arr_new(sizeof(fude_zoom_v2), _heap), .rings = rde_arr_new(sizeof(u32), _heap), .heights = rde_arr_new(sizeof(f64), _heap),
        .mm = _mm_per_point, .thickness = _thickness_mm, .height = _thickness_mm,
    };
    const fude_zoom_export_sink _sink = { &_w, NULL, fude_zoom_stl_shape, fude_zoom_stl_fill, NULL, NULL, true, fude_zoom_stl_on_object };
    fude_zoom_export_walk(_s, _half, &_sink);
    rde_arr _tris = rde_arr_new(sizeof(f32), _heap);
    u32 _solids = 0;
    const u32 _n = fude_zoom_stl_solids((const fude_zoom_v2*)_w.pts.memory, (const u32*)_w.rings.memory, (u32)rde_arr_length(&_w.pts),
                                        (const f64*)_w.heights.memory, &_tris, &_solids);
    if(_said != NULL) {
        _said->solids    = _solids;
        _said->triangles = _n;
    }
    if(_n > 0u) {
        // Binary STL: 80 bytes of header, how many, then each its facing and its corners (millimetres).
        c8 _head[80];
        memset(_head, 0, sizeof(_head));
        snprintf(_head, sizeof(_head), "Sketching: a 3D model, in millimetres");
        fude_put_data(_out, _head, 80u);
        fude_put_u32(_out, _n);
        const f32* _t = (const f32*)_tris.memory;
        for(u32 _i = 0; _i < _n; _i++, _t += 9) {
            const f32 _ux = _t[3] - _t[0], _uy = _t[4] - _t[1], _uz = _t[5] - _t[2];
            const f32 _vx = _t[6] - _t[0], _vy = _t[7] - _t[1], _vz = _t[8] - _t[2];
            f32 _nx = _uy * _vz - _uz * _vy, _ny = _uz * _vx - _ux * _vz, _nz = _ux * _vy - _uy * _vx;
            const f32 _l = sqrtf(_nx * _nx + _ny * _ny + _nz * _nz);
            if(_l > 0.0f) {
                _nx /= _l; _ny /= _l; _nz /= _l;
            }
            fude_put_f32(_out, _nx);
            fude_put_f32(_out, _ny);
            fude_put_f32(_out, _nz);
            for(u32 _k = 0; _k < 9u; _k++) {
                fude_put_f32(_out, _t[_k]);
            }
            fude_put_u16(_out, 0u);
        }
    }
    rde_arr_free(&_tris);
    rde_arr_free(&_w.pts);
    rde_arr_free(&_w.rings);
    rde_arr_free(&_w.heights);
    return _n > 0u;
}
