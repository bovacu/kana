// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "sim/body.h"

// ===========================================================================
// See body.h.
//
// Internal names start fsb_ (FSB_ for the table). The file follows the header's order: the materials, a polygon's
// measures, simplifying (fsb_dp, Douglas–Peucker on an open chain), then the cut into convex pieces. Each function
// takes its scratch arrays from the std allocator and frees them before it returns. While a polygon is cut, a piece
// is held as its corners' indices into the polygon (fsb_piece); the points are copied out only at the end.
// ===========================================================================

// Returns a new array of _n zeroed items of _size bytes, on the std allocator; the caller frees it.
RDE_INTERNAL rde_arr fsb_scratch(usize _size, u32 _n) {
    rde_arr _a = rde_arr_new(_size, rde_memory_allocator_get_default_std());
    rde_arr_resize(&_a, _n);
    return _a;
}

// Appends _in's points to _out (an rde_arr of fude_sim_v2), skipping each point equal to the one before it. Returns
// _out's length.
RDE_INTERNAL u32 fsb_distinct(const fude_sim_v2* _in, u32 _n, rde_arr* _out) {
    for(u32 _i = 0; _i < _n; _i++) {
        const u32 _m = (u32)rde_arr_length(_out);
        const fude_sim_v2* _last = _m > 0u ? &((const fude_sim_v2*)_out->memory)[_m - 1u] : NULL;
        if(_last == NULL || rde_math_hypot_f64(_in[_i].x - _last->x, _in[_i].y - _last->y) > 0.0) {
            rde_arr_add(_out, (any)&_in[_i]);
        }
    }
    return (u32)rde_arr_length(_out);
}

// The materials: id, English name, density (kg/m³), friction, bounce, tensile strength (MPa), as fude_sim_material
// has them.
RDE_INTERNAL const fude_sim_material FSB_MATERIALS[] = {
    { "wood",      "Wood",      600.0,  0.50, 0.30, 40.0 },
    { "steel",     "Steel",     7850.0, 0.40, 0.20, 400.0 },
    { "aluminium", "Aluminium", 2700.0, 0.40, 0.20, 200.0 },
    { "rubber",    "Rubber",    1100.0, 0.90, 0.80, 15.0 },
    { "plastic",   "Plastic",   950.0,  0.30, 0.40, 40.0 },
    { "glass",     "Glass",     2500.0, 0.20, 0.10, 30.0 },
    { "ice",       "Ice",       917.0,  0.03, 0.05, 1.0 },
    { "foam",      "Foam",      50.0,   0.60, 0.30, 0.3 },
};

// See the header.
u32 fude_sim_material_count(void) {
    return (u32)(sizeof(FSB_MATERIALS) / sizeof(FSB_MATERIALS[0]));
}

// See the header.
const fude_sim_material* fude_sim_material_at(u32 _i) {
    return _i < fude_sim_material_count() ? &FSB_MATERIALS[_i] : NULL;
}

// See the header. A linear search: there are eight.
const fude_sim_material* fude_sim_material_find(const c8* _id) {
    for(u32 _i = 0; _id != NULL && _i < fude_sim_material_count(); _i++) {
        if(rde_strcmp(FSB_MATERIALS[_i].id, _id) == 0) {
            return &FSB_MATERIALS[_i];
        }
    }
    return NULL;
}

// --- a polygon's measures ------------------------------------------------------------------------------

// Returns the cross product of _o→_a and _o→_b: positive when _o, _a, _b turn counter-clockwise, 0 when in a line.
RDE_INTERNAL f64 fsb_cross(fude_sim_v2 _o, fude_sim_v2 _a, fude_sim_v2 _b) {
    return (_a.x - _o.x) * (_b.y - _o.y) - (_a.y - _o.y) * (_b.x - _o.x);
}

// See the header: the shoelace formula, as a fan of triangles from the first corner.
f64 fude_sim_polygon_area(const fude_sim_v2* _p, u32 _n) {
    if(_n < 3u) {
        return 0.0;
    }
    // (about its first corner: a small shape far from the origin keeps its digits)
    f64 _a = 0.0;
    for(u32 _i = 1; _i + 1u < _n; _i++) {
        _a += (_p[_i].x - _p[0].x) * (_p[_i + 1u].y - _p[0].y) - (_p[_i + 1u].x - _p[0].x) * (_p[_i].y - _p[0].y);
    }
    return 0.5 * _a;
}

// See the header: the centre of the shoelace's triangles, weighted by their signed areas, about the first corner.
fude_sim_v2 fude_sim_polygon_centroid(const fude_sim_v2* _p, u32 _n) {
    // (about its first corner: no precision lost far from the origin)
    const fude_sim_v2 _o = _n > 0u ? _p[0] : (fude_sim_v2){ 0.0, 0.0 };
    f64 _a = 0.0, _cx = 0.0, _cy = 0.0;
    for(u32 _i = 0, _j = _n - 1u; _i < _n && _n >= 3u; _j = _i++) {
        const f64 _xj = _p[_j].x - _o.x, _yj = _p[_j].y - _o.y, _xi = _p[_i].x - _o.x, _yi = _p[_i].y - _o.y;
        const f64 _c = _xj * _yi - _xi * _yj;
        _a  += _c;
        _cx += (_xj + _xi) * _c;
        _cy += (_yj + _yi) * _c;
    }
    if(rde_math_abs_f64(_a) < 1e-300) {
        // (no area: its corners' mean)
        fude_sim_v2 _m = { 0.0, 0.0 };
        for(u32 _i = 0; _i < _n; _i++) {
            _m.x += _p[_i].x / (f64)_n;
            _m.y += _p[_i].y / (f64)_n;
        }
        return _m;
    }
    return (fude_sim_v2){ _o.x + _cx / (3.0 * _a), _o.y + _cy / (3.0 * _a) };
}

// See the header: each edge's term of the polygon's polar second moment, summed and divided by 12; the absolute value
// makes a clockwise polygon give the same answer.
f64 fude_sim_polygon_inertia(const fude_sim_v2* _p, u32 _n, fude_sim_v2 _about) {
    f64 _j = 0.0;
    for(u32 _i = 0, _k = _n - 1u; _i < _n && _n >= 3u; _k = _i++) {
        const f64 _x0 = _p[_k].x - _about.x, _y0 = _p[_k].y - _about.y, _x1 = _p[_i].x - _about.x, _y1 = _p[_i].y - _about.y;
        const f64 _c = _x0 * _y1 - _x1 * _y0;
        _j += _c * (_x0 * _x0 + _x0 * _x1 + _x1 * _x1 + _y0 * _y0 + _y0 * _y1 + _y1 * _y1);
    }
    return rde_math_abs_f64(_j) / 12.0;
}

// See the header: rde_math's even-odd test (a ray cast towards +x, counting the edges it crosses).
b8 fude_sim_polygon_inside(const fude_sim_v2* _p, u32 _n, fude_sim_v2 _q) {
    return rde_math_point_in_polygon_2d_f64(_q, _p, _n);
}

// Returns a small length for comparisons at the polygon's scale: its extent from the first corner times 1e-9, so a
// test means the same for a shape in millimetres as for one in metres.
RDE_INTERNAL f64 fsb_eps(const fude_sim_v2* _p, u32 _n) {
    f64 _s = 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        _s = rde_math_max_f64(_s, rde_math_max_f64(rde_math_abs_f64(_p[_i].x - _p[0].x), rde_math_abs_f64(_p[_i].y - _p[0].y)));
    }
    return rde_math_max_f64(_s, 1e-300) * 1e-9;
}

// See the header. A turn whose cross product is at most fsb_eps times the two edges' length counts as straight and is
// skipped, so a corner in a line does not make a polygon concave.
b8 fude_sim_polygon_convex(const fude_sim_v2* _p, u32 _n) {
    if(_n < 3u) {
        return false;
    }
    const f64 _e = fsb_eps(_p, _n);
    i32 _way = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _c = fsb_cross(_p[_i], _p[(_i + 1u) % _n], _p[(_i + 2u) % _n]);
        const f64 _l = rde_math_hypot_f64(_p[(_i + 1u) % _n].x - _p[_i].x, _p[(_i + 1u) % _n].y - _p[_i].y) + rde_math_hypot_f64(_p[(_i + 2u) % _n].x - _p[(_i + 1u) % _n].x, _p[(_i + 2u) % _n].y - _p[(_i + 1u) % _n].y);
        if(rde_math_abs_f64(_c) <= _e * rde_math_max_f64(_l, 1e-300)) {
            continue;   // (straight on)
        }
        const i32 _s = _c > 0.0 ? 1 : -1;
        if(_way != 0 && _s != _way) {
            return false;
        }
        _way = _s;
    }
    return true;
}

// --- simplified ------------------------------------------------------------------------------------------

// Douglas–Peucker on the open chain _p[_a.._b]: marks in _keep (1) the point furthest from the chord _a–_b when it is
// further than _tol, then does the same on each side of it. _keep[_a] and _keep[_b] are the caller's to set.
RDE_INTERNAL void fsb_dp(const fude_sim_v2* _p, u32 _a, u32 _b, f64 _tol, u8* _keep) {
    if(_b <= _a + 1u) {
        return;
    }
    const f64 _dx = _p[_b].x - _p[_a].x, _dy = _p[_b].y - _p[_a].y, _l = rde_math_hypot_f64(_dx, _dy);
    f64 _far = -1.0;
    u32 _at = _a;
    for(u32 _i = _a + 1u; _i < _b; _i++) {
        const f64 _d = _l > 0.0 ? rde_math_abs_f64((_p[_i].x - _p[_a].x) * _dy - (_p[_i].y - _p[_a].y) * _dx) / _l : rde_math_hypot_f64(_p[_i].x - _p[_a].x, _p[_i].y - _p[_a].y);
        if(_d > _far) {
            _far = _d;
            _at  = _i;
        }
    }
    if(_far > _tol) {
        _keep[_at] = 1u;
        fsb_dp(_p, _a, _at, _tol, _keep);
        fsb_dp(_p, _at, _b, _tol, _keep);
    }
}

// See the header. The loop is made an array of _m + 1 points, its first again at the end, so both halves are open
// chains fsb_dp can take; the copy at the end is left out of the result.
u32 fude_sim_outline_simplify(const fude_sim_v2* _in, u32 _n, f64 _tolerance, fude_sim_v2* _out, u32 _max) {
    // (its points without repeats, its last its first again left off — then its first again, closing the loop)
    rde_arr _loop_arr = rde_arr_new_with_capacity(sizeof(fude_sim_v2), (usize)_n + 2u, rde_memory_allocator_get_default_std());
    u32 _m = fsb_distinct(_in, _n, &_loop_arr);
    const fude_sim_v2* _p = (const fude_sim_v2*)_loop_arr.memory;
    while(_m > 1u && _p[_m - 1u].x == _p[0].x && _p[_m - 1u].y == _p[0].y) {
        _m--;
    }
    if(_m < 3u) {
        rde_arr_free(&_loop_arr);
        return 0;
    }
    const fude_sim_v2 _start = _p[0];
    rde_arr_resize(&_loop_arr, _m);
    rde_arr_add(&_loop_arr, (any)&_start);   // (copied first: an add may move the array)
    const fude_sim_v2* _loop = (const fude_sim_v2*)_loop_arr.memory;
    // Round the loop: split at its first point and the one furthest from it, each half simplified as a chain.
    u32 _far = 0;
    f64 _fd = -1.0;
    for(u32 _i = 1; _i < _m; _i++) {
        const f64 _d = rde_math_hypot_f64(_loop[_i].x - _start.x, _loop[_i].y - _start.y);
        if(_d > _fd) {
            _fd = _d;
            _far = _i;
        }
    }
    rde_arr _keep_arr = fsb_scratch(sizeof(u8), _m + 1u);
    u8* _keep = (u8*)_keep_arr.memory;
    _keep[0] = _keep[_far] = _keep[_m] = 1u;
    fsb_dp(_loop, 0u, _far, _tolerance, _keep);
    fsb_dp(_loop, _far, _m, _tolerance, _keep);
    u32 _k = 0;
    for(u32 _i = 0; _i < _m; _i++) {
        if(_keep[_i] && _k < _max) {
            _out[_k++] = _loop[_i];
        }
    }
    rde_arr_free(&_keep_arr);
    rde_arr_free(&_loop_arr);
    if(_k < 3u) {
        return 0;
    }
    if(fude_sim_polygon_area(_out, _k) < 0.0) {
        for(u32 _i = 0; _i < _k / 2u; _i++) {
            const fude_sim_v2 _t = _out[_i];
            _out[_i] = _out[_k - 1u - _i];
            _out[_k - 1u - _i] = _t;
        }
    }
    return _k;
}

// See the header.
u32 fude_sim_polyline_simplify(const fude_sim_v2* _in, u32 _n, f64 _tolerance, fude_sim_v2* _out, u32 _max) {
    rde_arr _line = rde_arr_new_with_capacity(sizeof(fude_sim_v2), (usize)_n + 1u, rde_memory_allocator_get_default_std());
    const u32 _m = fsb_distinct(_in, _n, &_line);
    const fude_sim_v2* _p = (const fude_sim_v2*)_line.memory;
    if(_m < 2u) {
        rde_arr_free(&_line);
        return 0;
    }
    rde_arr _keep_arr = fsb_scratch(sizeof(u8), _m);
    u8* _keep = (u8*)_keep_arr.memory;
    _keep[0] = _keep[_m - 1u] = 1u;
    fsb_dp(_p, 0u, _m - 1u, _tolerance, _keep);
    u32 _k = 0;
    for(u32 _i = 0; _i < _m; _i++) {
        if(_keep[_i] && _k < _max) {
            _out[_k++] = _p[_i];
        }
    }
    rde_arr_free(&_keep_arr);
    rde_arr_free(&_line);
    return _k >= 2u ? _k : 0u;
}

// --- cut into convex pieces ---------------------------------------------------------------------------------

// Returns whether _q is in the counter-clockwise triangle _a _b _c, within _e (a point on an edge counts as in).
RDE_INTERNAL b8 fsb_in_triangle(fude_sim_v2 _a, fude_sim_v2 _b, fude_sim_v2 _c, fude_sim_v2 _q, f64 _e) {
    return fsb_cross(_a, _b, _q) >= -_e && fsb_cross(_b, _c, _q) >= -_e && fsb_cross(_c, _a, _q) >= -_e;
}

// A convex piece while the polygon is cut: its corners, counter-clockwise, as indices into the polygon.
typedef struct {
    u32 idx[64];   // Room to spare: a piece never has more than FUDE_SIM_BODY_CORNERS.
    u32 n;         // How many corners.
    b8  gone;      // Merged into another piece; skipped from then on.
} fsb_piece;

// Returns whether a piece is convex and runs counter-clockwise: what a merge must keep.
RDE_INTERNAL b8 fsb_piece_convex(const fude_sim_v2* _p, const fsb_piece* _q) {
    fude_sim_v2 _c[64];
    for(u32 _i = 0; _i < _q->n; _i++) {
        _c[_i] = _p[_q->idx[_i]];
    }
    return fude_sim_polygon_convex(_c, _q->n) && fude_sim_polygon_area(_c, _q->n) > 0.0;
}

// See the header. Three steps: ear clipping into triangles; merging neighbours (Hertel–Mehlhorn) while each piece
// stays convex and within _corners; then the pieces left are appended to _points and _counts. The ear search gives up
// after _n² + 8 passes, and when a pass finds no ear, which is how a polygon that crosses itself is refused.
u32 fude_sim_convex_pieces(const fude_sim_v2* _p, u32 _n, u32 _corners, rde_arr* _points, rde_arr* _counts) {
    if(_n < 3u || _corners < 3u) {
        return 0;
    }
    _corners = _corners > FUDE_SIM_BODY_CORNERS ? FUDE_SIM_BODY_CORNERS : _corners;
    const f64 _e = fsb_eps(_p, _n);
    // Ear clipping: the triangles, as the polygon's indices.
    rde_arr _left_arr = fsb_scratch(sizeof(u32), _n);
    u32* _left = (u32*)_left_arr.memory;   // (only removed from: it stays where it is)
    u32 _m = _n;
    for(u32 _i = 0; _i < _n; _i++) {
        _left[_i] = _i;
    }
    rde_arr _pieces_arr = fsb_scratch(sizeof(fsb_piece), _n);
    fsb_piece* _pieces = (fsb_piece*)_pieces_arr.memory;
    u32 _np = 0;
    u32 _guard = 0;
    while(_m > 3u && _guard < _n * _n + 8u) {
        _guard++;
        b8 _cut = false;
        for(u32 _k = 0; _k < _m && !_cut; _k++) {
            const u32 _a = _left[(_k + _m - 1u) % _m], _b = _left[_k], _c = _left[(_k + 1u) % _m];
            const f64 _turn = fsb_cross(_p[_a], _p[_b], _p[_c]);
            if(rde_math_abs_f64(_turn) <= _e * rde_math_max_f64(rde_math_hypot_f64(_p[_c].x - _p[_a].x, _p[_c].y - _p[_a].y), 1e-300)) {
                // (a corner straight on: dropped, no triangle)
                rde_arr_remove(&_left_arr, _k);
                _m--;
                _cut = true;
                break;
            }
            if(_turn < 0.0) {
                continue;   // (it turns in: not an ear)
            }
            b8 _empty = true;
            for(u32 _j = 0; _j < _m && _empty; _j++) {
                const u32 _q = _left[_j];
                if(_q == _a || _q == _b || _q == _c) {
                    continue;
                }
                // (a corner on the same place as one of the ear's: only if it is inside past its edges)
                const b8 _same = (_p[_q].x == _p[_a].x && _p[_q].y == _p[_a].y) || (_p[_q].x == _p[_b].x && _p[_q].y == _p[_b].y) ||
                                 (_p[_q].x == _p[_c].x && _p[_q].y == _p[_c].y);
                if(!_same && fsb_in_triangle(_p[_a], _p[_b], _p[_c], _p[_q], _e)) {
                    _empty = false;
                }
            }
            if(!_empty) {
                continue;
            }
            fsb_piece* _t = &_pieces[_np++];
            _t->idx[0] = _a;
            _t->idx[1] = _b;
            _t->idx[2] = _c;
            _t->n = 3u;
            rde_arr_remove(&_left_arr, _k);
            _m--;
            _cut = true;
        }
        if(!_cut) {
            break;   // (no ear anywhere: it crosses itself)
        }
    }
    if(_m != 3u || _np + 1u > _n) {
        rde_arr_free(&_left_arr);
        rde_arr_free(&_pieces_arr);
        return 0;
    }
    if(fsb_cross(_p[_left[0]], _p[_left[1]], _p[_left[2]]) > 0.0) {
        fsb_piece* _t = &_pieces[_np++];
        _t->idx[0] = _left[0];
        _t->idx[1] = _left[1];
        _t->idx[2] = _left[2];
        _t->n = 3u;
    }
    rde_arr_free(&_left_arr);
    // Merged: two pieces sharing an edge (one has it u → v, the other v → u) into one, while it stays convex and
    // small enough. Every merge starts the search again, until a pass finds none.
    for(b8 _merged = true; _merged;) {
        _merged = false;
        for(u32 _i = 0; _i < _np && !_merged; _i++) {
            if(_pieces[_i].gone) {
                continue;
            }
            for(u32 _j = _i + 1u; _j < _np && !_merged; _j++) {
                if(_pieces[_j].gone || _pieces[_i].n + _pieces[_j].n - 2u > _corners) {
                    continue;
                }
                const fsb_piece* _a = &_pieces[_i], *_b = &_pieces[_j];
                for(u32 _ea = 0; _ea < _a->n && !_merged; _ea++) {
                    const u32 _u = _a->idx[_ea], _v = _a->idx[(_ea + 1u) % _a->n];
                    for(u32 _eb = 0; _eb < _b->n && !_merged; _eb++) {
                        if(_b->idx[_eb] != _v || _b->idx[(_eb + 1u) % _b->n] != _u) {
                            continue;
                        }
                        // (a from v round to u, then b from u's next round to v's previous)
                        fsb_piece _c;
                        rde_memset(&_c, 0, sizeof(_c));
                        for(u32 _k = 0; _k < _a->n; _k++) {
                            _c.idx[_c.n++] = _a->idx[(_ea + 1u + _k) % _a->n];
                        }
                        for(u32 _k = 2; _k < _b->n; _k++) {
                            _c.idx[_c.n++] = _b->idx[(_eb + _k) % _b->n];
                        }
                        if(_c.n <= _corners && fsb_piece_convex(_p, &_c)) {
                            _pieces[_i] = _c;
                            _pieces[_j].gone = true;
                            _merged = true;
                        }
                    }
                }
            }
        }
    }
    u32 _count = 0;
    for(u32 _i = 0; _i < _np; _i++) {
        if(_pieces[_i].gone) {
            continue;
        }
        for(u32 _k = 0; _k < _pieces[_i].n; _k++) {
            rde_arr_add(_points, (any)&_p[_pieces[_i].idx[_k]]);
        }
        rde_arr_add(_counts, (any)&_pieces[_i].n);
        _count++;
    }
    rde_arr_free(&_pieces_arr);
    return _count;
}
