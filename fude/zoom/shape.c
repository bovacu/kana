// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/shape.h"
#include "zoom/symbol.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See shape.h.
// ===========================================================================

#define FUDE_ZOOM_SHAPE_PI 3.141592653589793

#define FUDE_ZOOM_SHAPE_LINE_BOW   0.06   // a line: no point further off it than this share of its length
#define FUDE_ZOOM_SHAPE_CLOSED     0.22   // closed: the ends nearer than this share of the drawing's size
#define FUDE_ZOOM_SHAPE_CORNER_TOL 0.07   // corners found where the path bends more than this share of its size
#define FUDE_ZOOM_SHAPE_POLY_FIT   0.045  // a polygon: the path this close to its sides (share of its size)
#define FUDE_ZOOM_SHAPE_ELLIPSE_FIT 0.16  // an ellipse: the path this close to it (in its own radii)
#define FUDE_ZOOM_SHAPE_ROUND      0.15   // an ellipse this near a circle is one
#define FUDE_ZOOM_SHAPE_SQUARE_ANG 0.30   // a quadrilateral's corners this near square (radians): a rectangle

u32 fude_zoom_shape_segments(f64 _size) {
    const f64 _n = _size * FUDE_ZOOM_SHAPE_PI / 6.0;   // about 6 pt a segment
    return (u32)(_n < 24.0 ? 24.0 : (_n > 360.0 ? 360.0 : _n));
}

void fude_zoom_shape_outline(u8 _type, const f64* _n, u32 _count, u32 _segments, rde_arr* _out, b8* _closed) {
    rde_arr_clear(_out);
    *_closed = _type != FUDE_ZOOM_SHAPE_LINE && _type != FUDE_ZOOM_SHAPE_DIMENSION && _type != FUDE_ZOOM_SHAPE_ARROW && _type != FUDE_ZOOM_SHAPE_ARC &&
               _type != FUDE_ZOOM_SHAPE_RADIAL && _type != FUDE_ZOOM_SHAPE_ANGLE;
    if(_type == FUDE_ZOOM_SHAPE_RADIAL && _count >= 3u) {
        // Its line: from the centre (or right across) out to the circle.
        const fude_zoom_v2 _u = { cos(_n[1]) * _n[0], sin(_n[1]) * _n[0] };
        const fude_zoom_v2 _p[2] = { _n[2] >= 0.5 ? (fude_zoom_v2){ -_u.x, -_u.y } : (fude_zoom_v2){ 0.0, 0.0 }, _u };
        rde_arr_add(_out, (any)&_p[0]);
        rde_arr_add(_out, (any)&_p[1]);
        return;
    }
    if(_type == FUDE_ZOOM_SHAPE_ANGLE && _count >= 3u) {
        // Its arc (as an arc's: its ends, and between them round as finely as asked).
        const f64 _sweep = fmax(fmin(_n[2], 2.0 * FUDE_ZOOM_SHAPE_PI), -2.0 * FUDE_ZOOM_SHAPE_PI);
        u32 _k = _segments <= 1u ? 2u : (u32)ceil((f64)_segments * fabs(_sweep) / (2.0 * FUDE_ZOOM_SHAPE_PI));
        _k = _k < 2u ? 2u : _k;
        for(u32 _i = 0; _i <= _k; _i++) {
            const f64          _a = _n[1] + _sweep * (f64)_i / (f64)_k;
            const fude_zoom_v2 _p = { cos(_a) * _n[0], sin(_a) * _n[0] };
            rde_arr_add(_out, (any)&_p);
        }
        return;
    }
    if(_type == FUDE_ZOOM_SHAPE_SYMBOL && _count >= 3u) {
        // Its outline (its parts are fude_zoom_symbol_parts'); _segments 1: a round one's quarters only (snapping).
        fude_zoom_symbol_outline((u32)_n[0], _n[1], _n[2], _segments <= 1u ? 16u : _segments, _out);
        return;
    }
    if(_type == FUDE_ZOOM_SHAPE_ARC && _count >= 3u) {
        // Its ends (and, _segments 1, only them and its middle: what snapping looks at); else round as finely as a circle would be.
        const f64 _sweep = fmax(fmin(_n[2], 2.0 * FUDE_ZOOM_SHAPE_PI), -2.0 * FUDE_ZOOM_SHAPE_PI);
        u32 _k = _segments <= 1u ? 2u : (u32)ceil((f64)_segments * fabs(_sweep) / (2.0 * FUDE_ZOOM_SHAPE_PI));
        _k = _k < 2u ? 2u : _k;
        for(u32 _i = 0; _i <= _k; _i++) {
            const f64          _a = _n[1] + _sweep * (f64)_i / (f64)_k;
            const fude_zoom_v2 _p = { cos(_a) * _n[0], sin(_a) * _n[0] };
            rde_arr_add(_out, (any)&_p);
        }
        return;
    }
    if(_type == FUDE_ZOOM_SHAPE_ARROW && _count >= 1u) {
        const u32 _k = (u32)_n[0];
        for(u32 _i = 0; _i < _k && 2u * _i + 2u < _count && _i < FUDE_ZOOM_ARROW_POINTS; _i++) {
            const fude_zoom_v2 _p = { _n[1u + 2u * _i], _n[2u + 2u * _i] };
            rde_arr_add(_out, (any)&_p);
        }
        return;
    }
    if(_type == FUDE_ZOOM_SHAPE_DIMENSION && _count >= 3u) {
        // Where it measures from and to, and its line beside them (what it is
        // picked, boxed and snapped to by).
        const f64 _len = hypot(_n[0], _n[1]);
        const fude_zoom_v2 _left = _len > 0.0 ? (fude_zoom_v2){ -_n[1] / _len * _n[2], _n[0] / _len * _n[2] } : (fude_zoom_v2){ 0.0, 0.0 };
        const fude_zoom_v2 _p[4] = { { 0.0, 0.0 }, _left, { _n[0] + _left.x, _n[1] + _left.y }, { _n[0], _n[1] } };
        for(u32 _i = 0; _i < 4u; _i++) {
            rde_arr_add(_out, (any)&_p[_i]);
        }
    } else if(_type == FUDE_ZOOM_SHAPE_LINE && _count >= 2u) {
        const fude_zoom_v2 _p[2] = { { 0.0, 0.0 }, { _n[0], _n[1] } };
        rde_arr_add(_out, (any)&_p[0]);
        rde_arr_add(_out, (any)&_p[1]);
    } else if(_type == FUDE_ZOOM_SHAPE_ELLIPSE && _count >= 2u) {
        for(u32 _i = 0; _i < _segments; _i++) {
            const f64          _a = 2.0 * FUDE_ZOOM_SHAPE_PI * (f64)_i / (f64)_segments;
            const fude_zoom_v2 _p = { cos(_a) * _n[0], sin(_a) * _n[1] };
            rde_arr_add(_out, (any)&_p);
        }
    } else if(_type == FUDE_ZOOM_SHAPE_BOARD && fude_zoom_board_is_cut(_n, _count)) {
        // A cut board: its own outline (ring 0; its holes are fude_zoom_board_rings').
        const u32 _k = (u32)_n[FUDE_ZOOM_BOARD_RINGS_AT + 1u];
        for(u32 _i = 0; _i < _k && FUDE_ZOOM_BOARD_RINGS_AT + 3u + 2u * _i < _count; _i++) {
            const fude_zoom_v2 _p = { _n[FUDE_ZOOM_BOARD_RINGS_AT + 2u + 2u * _i], _n[FUDE_ZOOM_BOARD_RINGS_AT + 3u + 2u * _i] };
            rde_arr_add(_out, (any)&_p);
        }
    } else if((_type == FUDE_ZOOM_SHAPE_RECT || _type == FUDE_ZOOM_SHAPE_BOARD || _type == FUDE_ZOOM_SHAPE_SHEET) && _count >= 2u) {
        const f64 _hw = _n[0], _hh = _n[1];
        f64 _r = _count >= 3u && _type == FUDE_ZOOM_SHAPE_RECT ? _n[2] : 0.0;   // (a board's third number is its thickness, a sheet's its scale)
        _r = fmin(fmax(_r, 0.0), fmin(fabs(_hw), fabs(_hh)));
        if(_r <= 0.0) {
            const fude_zoom_v2 _p[4] = { { -_hw, -_hh }, { _hw, -_hh }, { _hw, _hh }, { -_hw, _hh } };
            for(u32 _i = 0; _i < 4u; _i++) {
                rde_arr_add(_out, (any)&_p[_i]);
            }
        } else {
            // Each corner a quarter circle.
            const fude_zoom_v2 _c[4] = { { _hw - _r, -_hh + _r }, { _hw - _r, _hh - _r }, { -_hw + _r, _hh - _r }, { -_hw + _r, -_hh + _r } };
            const u32 _per = _segments / 4u > 2u ? _segments / 4u : 2u;
            for(u32 _k = 0; _k < 4u; _k++) {
                for(u32 _i = 0; _i <= _per; _i++) {
                    const f64          _a = FUDE_ZOOM_SHAPE_PI * 0.5 * ((f64)_k - 1.0 + (f64)_i / (f64)_per);
                    const fude_zoom_v2 _p = { _c[_k].x + cos(_a) * _r, _c[_k].y + sin(_a) * _r };
                    rde_arr_add(_out, (any)&_p);
                }
            }
        }
    } else if(_type == FUDE_ZOOM_SHAPE_POLYGON) {
        for(u32 _i = 0; _i + 1u < _count; _i += 2u) {
            const fude_zoom_v2 _p = { _n[_i], _n[_i + 1u] };
            rde_arr_add(_out, (any)&_p);
        }
    } else if(_type == FUDE_ZOOM_SHAPE_PATH && _count >= 5u) {
        u64          _bits;
        fude_zoom_v2 _p[FUDE_ZOOM_PATH_NODES], _h[FUDE_ZOOM_PATH_NODES];
        const u32    _nodes = fude_zoom_path_unpack(_n, _count, &_bits, _p, _h);
        const b8     _loop  = (_bits & FUDE_ZOOM_PATH_CLOSED) != 0u;
        *_closed = _loop && _nodes >= 3u;
        if(_nodes < 2u) {
            return;
        }
        if(_segments <= 1u) {
            // Its nodes only (what snapping and driving look at).
            for(u32 _i = 0; _i < _nodes; _i++) {
                rde_arr_add(_out, (any)&_p[_i]);
            }
            return;
        }
        // Each stretch between nodes sampled alike, the whole within what a renderer takes.
        const u32 _stretches = *_closed ? _nodes : _nodes - 1u;
        u32 _per = _stretches > 0 ? _segments / _stretches : 1u;
        _per = _per < 4u ? 4u : _per;
        _per = _per * _stretches > 720u ? 720u / _stretches : _per;
        _per = _per < 1u ? 1u : _per;
        const u64 _shape = *_closed ? _bits : (_bits & ~(u64)FUDE_ZOOM_PATH_CLOSED);   // (a closed one of two nodes: drawn open)
        for(u32 _i = 0; _i < _stretches; _i++) {
            // Each stretch a cubic: from its node along the way it leaves, into the next the way it comes in.
            const u32          _j  = (_i + 1u) % _nodes;
            const fude_zoom_v2 _b  = _p[_i], _c = _p[_j];
            const fude_zoom_v2 _to = fude_zoom_path_tangent(_p, _h, _nodes, _shape, _i, true);
            const fude_zoom_v2 _in = fude_zoom_path_tangent(_p, _h, _nodes, _shape, _j, false);
            const fude_zoom_v2 _c1 = { _b.x + _to.x, _b.y + _to.y }, _c2 = { _c.x - _in.x, _c.y - _in.y };
            for(u32 _k = 0; _k < _per; _k++) {
                const f64 _t = (f64)_k / (f64)_per, _u = 1.0 - _t;
                const f64 _w0 = _u * _u * _u, _w1 = 3.0 * _u * _u * _t, _w2 = 3.0 * _u * _t * _t, _w3 = _t * _t * _t;
                const fude_zoom_v2 _q = { _w0 * _b.x + _w1 * _c1.x + _w2 * _c2.x + _w3 * _c.x, _w0 * _b.y + _w1 * _c1.y + _w2 * _c2.y + _w3 * _c.y };
                rde_arr_add(_out, (any)&_q);
            }
        }
        if(!*_closed) {
            rde_arr_add(_out, (any)&_p[_nodes - 1u]);
        }
    }
}

u32 fude_zoom_path_unpack(const f64* _n, u32 _count, u64* _bits, fude_zoom_v2* _nodes, fude_zoom_v2* _handles) {
    const u64 _b = _count > 0u && _n[0] >= 0.0 && _n[0] < 18446744073709551616.0 ? (u64)_n[0] : 0u;
    *_bits = _b;
    const b8  _kept = (_b & FUDE_ZOOM_PATH_HANDLES) != 0u;
    u32       _m    = _count > 1u ? (_count - 1u) / (_kept ? 4u : 2u) : 0u;
    _m = _m < FUDE_ZOOM_PATH_NODES ? _m : FUDE_ZOOM_PATH_NODES;
    for(u32 _i = 0; _i < _m; _i++) {
        _nodes[_i]   = (fude_zoom_v2){ _n[1u + 2u * _i], _n[2u + 2u * _i] };
        _handles[_i] = _kept ? (fude_zoom_v2){ _n[1u + 2u * _m + 2u * _i], _n[2u + 2u * _m + 2u * _i] } : (fude_zoom_v2){ 0.0, 0.0 };
    }
    return _m;
}

u32 fude_zoom_path_pack(u64 _bits, const fude_zoom_v2* _nodes, const fude_zoom_v2* _handles, u32 _count, f64* _n) {
    _count = _count < FUDE_ZOOM_PATH_NODES ? _count : FUDE_ZOOM_PATH_NODES;
    b8 _any = false;
    for(u32 _i = 0; _handles != NULL && _i < _count; _i++) {
        _any = _any || _handles[_i].x != 0.0 || _handles[_i].y != 0.0;
    }
    _bits = _any ? (_bits | FUDE_ZOOM_PATH_HANDLES) : (_bits & ~FUDE_ZOOM_PATH_HANDLES);
    _n[0] = (f64)_bits;
    for(u32 _i = 0; _i < _count; _i++) {
        _n[1u + 2u * _i] = _nodes[_i].x;
        _n[2u + 2u * _i] = _nodes[_i].y;
        if(_any) {
            _n[1u + 2u * _count + 2u * _i] = _handles[_i].x;
            _n[2u + 2u * _count + 2u * _i] = _handles[_i].y;
        }
    }
    return 1u + (_any ? 4u : 2u) * _count;
}

fude_zoom_v2 fude_zoom_path_tangent(const fude_zoom_v2* _nodes, const fude_zoom_v2* _handles, u32 _count, u64 _bits, u32 _i, b8 _out) {
    if(_count < 2u || _i >= _count) {
        return (fude_zoom_v2){ 0.0, 0.0 };
    }
    if(_handles != NULL && (_handles[_i].x != 0.0 || _handles[_i].y != 0.0)) {
        return _handles[_i];   // its own: smooth through it
    }
    // The curve's own (Catmull-Rom's): from the one before to the one after; at a corner, or an
    // open end, from itself on that side (straight along its stretch).
    const b8  _closed = (_bits & FUDE_ZOOM_PATH_CLOSED) != 0u;
    const b8  _corner = (_bits & FUDE_ZOOM_PATH_CORNER(_i)) != 0u;
    const fude_zoom_v2 _here = _nodes[_i];
    fude_zoom_v2 _before = _i > 0u ? _nodes[_i - 1u] : (_closed ? _nodes[_count - 1u] : _here);
    fude_zoom_v2 _after  = _i + 1u < _count ? _nodes[_i + 1u] : (_closed ? _nodes[0] : _here);
    if(_corner) {
        if(_out) { _before = _here; } else { _after = _here; }
    }
    return (fude_zoom_v2){ (_after.x - _before.x) / 6.0, (_after.y - _before.y) / 6.0 };
}

fude_zoom_v2 fude_zoom_shape_catmull(fude_zoom_v2 _a, fude_zoom_v2 _b, fude_zoom_v2 _c, fude_zoom_v2 _d, f64 _t) {
    const f64 _t2 = _t * _t, _t3 = _t2 * _t;
    return (fude_zoom_v2){
        0.5 * (2.0 * _b.x + (-_a.x + _c.x) * _t + (2.0 * _a.x - 5.0 * _b.x + 4.0 * _c.x - _d.x) * _t2 + (-_a.x + 3.0 * _b.x - 3.0 * _c.x + _d.x) * _t3),
        0.5 * (2.0 * _b.y + (-_a.y + _c.y) * _t + (2.0 * _a.y - 5.0 * _b.y + 4.0 * _c.y - _d.y) * _t2 + (-_a.y + 3.0 * _b.y - 3.0 * _c.y + _d.y) * _t3),
    };
}

u32 fude_zoom_board_numbers(f64* _n, f64 _half_length, f64 _half_width, f64 _thickness_mm, u8 _look, const c8* _name) {
    memset(_n, 0, FUDE_ZOOM_SHAPE_NUMBERS * sizeof(f64));
    _n[0] = _half_length;
    _n[1] = _half_width;
    _n[2] = _thickness_mm;
    _n[3] = (f64)((_look & 1u) | ((_look >> 1) < FUDE_ZOOM_MATERIAL_COUNT ? _look & ~1u : 0u));
    usize _len = _name != NULL ? strlen(_name) : 0u;
    if(_len > FUDE_ZOOM_BOARD_NAME - 1u) {
        _len = FUDE_ZOOM_BOARD_NAME - 1u;
        while(_len > 0 && ((u8)_name[_len] & 0xC0u) == 0x80u) {
            _len--;
        }
    }
    u8 _bytes[FUDE_ZOOM_BOARD_NAME];
    memset(_bytes, 0, sizeof(_bytes));
    if(_len > 0) {
        memcpy(_bytes, _name, _len);
    }
    const u32 _words = (u32)((_len + 1u + 7u) / 8u);
    memcpy(&_n[4], _bytes, (usize)_words * 8u);   // (its bits, as a connector keeps ids)
    return 4u + _words;
}

b8 fude_zoom_shape_fit_arc(const fude_zoom_v2* _p, u32 _n, f64 _tolerance, fude_zoom_v2* _centre, f64* _radius, f64* _from, f64* _sweep) {
    if(_n < 3u) {
        return false;
    }
    // Kåsa's fit, round the points' mean (for its sums to stay small): x² + y² + D x + E y + F = 0, least squares.
    fude_zoom_v2 _m = { 0.0, 0.0 };
    for(u32 _i = 0; _i < _n; _i++) {
        _m.x += _p[_i].x; _m.y += _p[_i].y;
    }
    _m.x /= (f64)_n; _m.y /= (f64)_n;
    f64 _sxx = 0, _sxy = 0, _syy = 0, _sxz = 0, _syz = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _x = _p[_i].x - _m.x, _y = _p[_i].y - _m.y, _z = _x * _x + _y * _y;
        _sxx += _x * _x; _sxy += _x * _y; _syy += _y * _y; _sxz += _x * _z; _syz += _y * _z;
    }
    // (Round the mean the sums of x and y are 0: D, E from a 2 × 2, the centre at -D/2, -E/2.)
    const f64 _det = _sxx * _syy - _sxy * _sxy;
    if(!(fabs(_det) > 1e-300)) {
        return false;   // on a line
    }
    const f64 _d = -(_sxz * _syy - _syz * _sxy) / _det, _e = -(_sxx * _syz - _sxy * _sxz) / _det;
    const fude_zoom_v2 _c = { _m.x - _d * 0.5, _m.y - _e * 0.5 };
    f64 _r = 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        _r += hypot(_p[_i].x - _c.x, _p[_i].y - _c.y);
    }
    _r /= (f64)_n;
    if(!(_r > 0.0) || !isfinite(_r)) {
        return false;
    }
    for(u32 _i = 0; _i < _n; _i++) {
        if(fabs(hypot(_p[_i].x - _c.x, _p[_i].y - _c.y) - _r) > _tolerance) {
            return false;
        }
    }
    // How far round, the way it went (each step the short way: the points are close).
    f64 _turn = 0.0, _was = atan2(_p[0].y - _c.y, _p[0].x - _c.x);
    const f64 _start = _was;
    for(u32 _i = 1; _i < _n; _i++) {
        const f64 _now = atan2(_p[_i].y - _c.y, _p[_i].x - _c.x);
        f64 _d_a = _now - _was;
        while(_d_a > FUDE_ZOOM_SHAPE_PI)   { _d_a -= 2.0 * FUDE_ZOOM_SHAPE_PI; }
        while(_d_a < -FUDE_ZOOM_SHAPE_PI)  { _d_a += 2.0 * FUDE_ZOOM_SHAPE_PI; }
        _turn += _d_a;
        _was   = _now;
    }
    *_centre = _c;
    *_radius = _r;
    *_from   = _start;
    *_sweep  = fmax(fmin(_turn, 2.0 * FUDE_ZOOM_SHAPE_PI), -2.0 * FUDE_ZOOM_SHAPE_PI);
    return true;
}

b8 fude_zoom_shape_fillet(fude_zoom_v2 _a0, fude_zoom_v2 _a1, fude_zoom_v2 _b0, fude_zoom_v2 _b1, f64 _radius,
                          fude_zoom_v2* _a_keep, fude_zoom_v2* _a_end, fude_zoom_v2* _b_keep, fude_zoom_v2* _b_end,
                          fude_zoom_v2* _centre, f64* _from, f64* _sweep) {
    // Where they meet, carried on.
    const fude_zoom_v2 _da = { _a1.x - _a0.x, _a1.y - _a0.y }, _db = { _b1.x - _b0.x, _b1.y - _b0.y };
    const f64 _cross = _da.x * _db.y - _da.y * _db.x;
    const f64 _la = hypot(_da.x, _da.y), _lb = hypot(_db.x, _db.y);
    if(!(_radius > 0.0) || !(_la > 0.0) || !(_lb > 0.0) || fabs(_cross) <= 1e-12 * _la * _lb) {
        return false;
    }
    const f64 _t = ((_b0.x - _a0.x) * _db.y - (_b0.y - _a0.y) * _db.x) / _cross;
    const fude_zoom_v2 _x = { _a0.x + _da.x * _t, _a0.y + _da.y * _t };
    // Each line's end nearer the corner goes; its other end stays (the way from the corner along it: u).
    const b8 _a_near0 = hypot(_a0.x - _x.x, _a0.y - _x.y) < hypot(_a1.x - _x.x, _a1.y - _x.y);
    const b8 _b_near0 = hypot(_b0.x - _x.x, _b0.y - _x.y) < hypot(_b1.x - _x.x, _b1.y - _x.y);
    const fude_zoom_v2 _af = _a_near0 ? _a1 : _a0, _bf = _b_near0 ? _b1 : _b0;   // the far ends
    const f64 _fa = hypot(_af.x - _x.x, _af.y - _x.y), _fb = hypot(_bf.x - _x.x, _bf.y - _x.y);
    if(!(_fa > 0.0) || !(_fb > 0.0)) {
        return false;
    }
    const fude_zoom_v2 _ua = { (_af.x - _x.x) / _fa, (_af.y - _x.y) / _fa }, _ub = { (_bf.x - _x.x) / _fb, (_bf.y - _x.y) / _fb };
    const f64 _cos = fmax(fmin(_ua.x * _ub.x + _ua.y * _ub.y, 1.0), -1.0);
    const f64 _angle = acos(_cos);   // between them, at the corner
    if(!(_angle > 1e-9) || !(_angle < FUDE_ZOOM_SHAPE_PI - 1e-9)) {
        return false;
    }
    const f64 _back = _radius / tan(_angle * 0.5);   // from the corner to where the arc touches each
    if(_back >= _fa || _back >= _fb) {
        return false;   // past a line's far end
    }
    const fude_zoom_v2 _ta = { _x.x + _ua.x * _back, _x.y + _ua.y * _back }, _tb = { _x.x + _ub.x * _back, _x.y + _ub.y * _back };
    const fude_zoom_v2 _bis = { _ua.x + _ub.x, _ua.y + _ub.y };
    const f64 _bl = hypot(_bis.x, _bis.y);
    const f64 _out = _radius / sin(_angle * 0.5);   // the corner to the centre, along the bisector
    const fude_zoom_v2 _c = { _x.x + _bis.x / _bl * _out, _x.y + _bis.y / _bl * _out };
    *_a_keep = _af; *_a_end = _ta;
    *_b_keep = _bf; *_b_end = _tb;
    *_centre = _c;
    const f64 _s = atan2(_ta.y - _c.y, _ta.x - _c.x), _e = atan2(_tb.y - _c.y, _tb.x - _c.x);
    f64 _sw = _e - _s;
    while(_sw > FUDE_ZOOM_SHAPE_PI)  { _sw -= 2.0 * FUDE_ZOOM_SHAPE_PI; }
    while(_sw < -FUDE_ZOOM_SHAPE_PI) { _sw += 2.0 * FUDE_ZOOM_SHAPE_PI; }   // (the short way: a fillet is under a half turn)
    *_from  = _s;
    *_sweep = _sw;
    return true;
}

u8 fude_zoom_board_look(const f64* _n, u32 _count) {
    if(_count < 4u || !(_n[3] >= 0.5) || !(_n[3] < 256.0)) {
        return 0;
    }
    const u32 _look = (u32)(_n[3] + 0.5);
    return (u8)((_look & 1u) | ((_look >> 1) < FUDE_ZOOM_MATERIAL_COUNT ? _look & ~1u : 0u));
}

u8 fude_zoom_board_grain(const f64* _n, u32 _count) {
    return fude_zoom_board_look(_n, _count) & 1u;
}

u8 fude_zoom_board_material(const f64* _n, u32 _count) {
    return fude_zoom_board_look(_n, _count) >> 1;
}

rde_color fude_zoom_material_wood(u8 _material) {
    static const rde_color _wood[FUDE_ZOOM_MATERIAL_COUNT] = {
        { 226, 202, 160, 255 },   // not said: pale timber
        { 236, 206, 146, 255 },   // pine
        { 204, 164, 112, 255 },   // oak
        { 118,  80,  54, 255 },   // walnut
        { 240, 224, 192, 255 },   // maple
        { 228, 190, 148, 255 },   // beech
        { 184, 112,  80, 255 },   // cherry
        { 238, 218, 178, 255 },   // birch plywood
        { 180, 146, 104, 255 },   // MDF
        { 244, 243, 238, 255 },   // melamine
    };
    return _wood[_material < FUDE_ZOOM_MATERIAL_COUNT ? _material : 0u];
}

void fude_zoom_board_name(const f64* _n, u32 _count, c8* _out, usize _size) {
    if(_size == 0) {
        return;
    }
    _out[0] = 0;
    if(_count <= 4u) {
        return;
    }
    u8 _bytes[FUDE_ZOOM_BOARD_NAME + 1u];
    const u32 _most  = FUDE_ZOOM_BOARD_NAME / 8u;
    const u32 _words = _count - 4u > _most ? _most : _count - 4u;
    memcpy(_bytes, &_n[4], (usize)_words * 8u);
    _bytes[_words * 8u] = 0;
    usize _len = strnlen((const c8*)_bytes, _words * 8u);
    if(_len >= _size) {
        _len = _size - 1u;
        while(_len > 0 && (_bytes[_len] & 0xC0u) == 0x80u) {
            _len--;
        }
    }
    memcpy(_out, _bytes, _len);
    _out[_len] = 0;
}

b8 fude_zoom_board_is_cut(const f64* _n, u32 _count) {
    return _count > FUDE_ZOOM_BOARD_RINGS_AT + 1u && _n[FUDE_ZOOM_BOARD_RINGS_AT] >= 1.0;
}

u32 fude_zoom_board_cut_numbers(rde_arr* _out, f64 _half_length, f64 _half_width, f64 _thickness_mm, u8 _grain, const c8* _name,
                                const fude_zoom_v2* _points, const u32* _rings, u32 _n) {
    rde_arr_clear(_out);
    f64 _head[FUDE_ZOOM_SHAPE_NUMBERS];
    fude_zoom_board_numbers(_head, _half_length, _half_width, _thickness_mm, _grain, _name);
    memcpy(rde_arr_add_n(_out, FUDE_ZOOM_BOARD_RINGS_AT), _head, FUDE_ZOOM_BOARD_RINGS_AT * sizeof(f64));   // (its name's twelve numbers, all of them)
    // How many rings, then each one: its count, its points.
    u32 _count = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        _count += _i == 0 || _rings[_i] != _rings[_i - 1u] ? 1u : 0u;
    }
    const f64 _r = (f64)_count;
    rde_arr_add(_out, (any)&_r);
    for(u32 _from = 0; _from < _n;) {
        u32 _to = _from;
        while(_to < _n && _rings[_to] == _rings[_from]) {
            _to++;
        }
        const f64 _k = (f64)(_to - _from);
        rde_arr_add(_out, (any)&_k);
        f64* _xy = rde_arr_add_n(_out, (usize)(_to - _from) * 2u);
        for(u32 _i = _from; _i < _to; _i++) {
            _xy[(_i - _from) * 2u]      = _points[_i].x;
            _xy[(_i - _from) * 2u + 1u] = _points[_i].y;
        }
        _from = _to;
    }
    return (u32)rde_arr_length(_out);
}

u32 fude_zoom_board_rings(const f64* _n, u32 _count, rde_arr* _points, rde_arr* _rings) {
    rde_arr_clear(_points);
    rde_arr_clear(_rings);
    if(_count < 2u) {
        return 0;
    }
    if(!fude_zoom_board_is_cut(_n, _count)) {
        const fude_zoom_v2 _c[4] = { { -_n[0], -_n[1] }, { _n[0], -_n[1] }, { _n[0], _n[1] }, { -_n[0], _n[1] } };
        const u32 _zero = 0;
        for(u32 _i = 0; _i < 4u; _i++) {
            rde_arr_add(_points, (any)&_c[_i]);
            rde_arr_add(_rings, (any)&_zero);
        }
        return 4u;
    }
    const u32 _ring_count = (u32)_n[FUDE_ZOOM_BOARD_RINGS_AT];
    u32 _at = FUDE_ZOOM_BOARD_RINGS_AT + 1u;
    for(u32 _r = 0; _r < _ring_count && _at < _count; _r++) {
        const u32 _k = (u32)_n[_at++];
        for(u32 _i = 0; _i < _k && _at + 1u < _count; _i++) {
            const fude_zoom_v2 _p = { _n[_at], _n[_at + 1u] };
            _at += 2u;
            rde_arr_add(_points, (any)&_p);
            rde_arr_add(_rings, (any)&_r);
        }
    }
    return (u32)rde_arr_length(_points);
}

// --- recognizing --------------------------------------------------------------------------

RDE_INTERNAL f64 fude_zoom_shape_dist_segment(fude_zoom_v2 _p, fude_zoom_v2 _a, fude_zoom_v2 _b) {
    const f64 _dx = _b.x - _a.x, _dy = _b.y - _a.y, _l2 = _dx * _dx + _dy * _dy;
    f64 _t = _l2 > 0.0 ? ((_p.x - _a.x) * _dx + (_p.y - _a.y) * _dy) / _l2 : 0.0;
    _t = _t < 0.0 ? 0.0 : (_t > 1.0 ? 1.0 : _t);
    return hypot(_a.x + _dx * _t - _p.x, _a.y + _dy * _t - _p.y);
}

// Douglas–Peucker on _p[_from .. _to]: the points kept (marked in _keep).
RDE_INTERNAL void fude_zoom_shape_simplify(const fude_zoom_v2* _p, u32 _from, u32 _to, f64 _tol, u8* _keep, u32 _depth) {
    if(_to <= _from + 1u || _depth > 64u) {
        return;
    }
    f64 _far = -1.0;
    u32 _at  = _from;
    for(u32 _i = _from + 1u; _i < _to; _i++) {
        const f64 _d = fude_zoom_shape_dist_segment(_p[_i], _p[_from], _p[_to]);
        if(_d > _far) {
            _far = _d;
            _at  = _i;
        }
    }
    if(_far > _tol) {
        _keep[_at] = 1u;
        fude_zoom_shape_simplify(_p, _from, _at, _tol, _keep, _depth + 1u);
        fude_zoom_shape_simplify(_p, _at, _to, _tol, _keep, _depth + 1u);
    }
}

u32 fude_zoom_shape_fit_path(const fude_zoom_v2* _p, u32 _n, f64 _tol, u32 _most, fude_zoom_v2* _out) {
    if(_n == 0 || _most < 2u) {
        return 0;
    }
    if(_n == 1u) {
        _out[0] = _p[0];
        return 1u;
    }
    u8* _keep = (u8*)malloc(_n);
    if(_keep == NULL) {
        return 0;
    }
    // Simplified until it fits in _most nodes (the tolerance let out each time).
    u32 _kept = 0;
    for(u32 _tries = 0; _tries < 40u; _tries++, _tol *= 1.4) {
        memset(_keep, 0, _n);
        _keep[0] = _keep[_n - 1u] = 1u;
        fude_zoom_shape_simplify(_p, 0, _n - 1u, _tol, _keep, 0);
        _kept = 0;
        for(u32 _i = 0; _i < _n; _i++) {
            _kept += _keep[_i];
        }
        if(_kept <= _most) {
            break;
        }
    }
    u32 _m = 0;
    for(u32 _i = 0; _i < _n && _m < _most; _i++) {
        if(_keep[_i]) {
            _out[_m++] = _p[_i];
        }
    }
    if(_m > 0 && (_out[_m - 1u].x != _p[_n - 1u].x || _out[_m - 1u].y != _p[_n - 1u].y)) {
        _out[_m - 1u] = _p[_n - 1u];   // (cut short: its end kept all the same)
    }
    free(_keep);
    return _m;
}

// How far, on average, the path is from the closed polygon _c (its _k corners).
RDE_INTERNAL f64 fude_zoom_shape_poly_error(const fude_zoom_v2* _p, u32 _n, const fude_zoom_v2* _c, u32 _k) {
    f64 _sum = 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        f64 _best = 1e300;
        for(u32 _j = 0; _j < _k; _j++) {
            const f64 _d = fude_zoom_shape_dist_segment(_p[_i], _c[_j], _c[(_j + 1u) % _k]);
            _best = _d < _best ? _d : _best;
        }
        _sum += _best;
    }
    return _sum / (f64)_n;
}

// The corners _c (_k of them) as a shape: four near square a RECTANGLE,
// otherwise a POLYGON round their middle.
RDE_INTERNAL b8 fude_zoom_shape_polygon(const fude_zoom_v2* _c, u32 _k, fude_zoom_shape_fit* _out) {
    if(_k == 4u) {
        // Four corners near square: a RECTANGLE, turned as its longest side.
        b8 _square = true;
        for(u32 _i = 0; _i < 4u && _square; _i++) {
            const fude_zoom_v2 _u = { _c[(_i + 1u) % 4u].x - _c[_i].x, _c[(_i + 1u) % 4u].y - _c[_i].y };
            const fude_zoom_v2 _v = { _c[(_i + 3u) % 4u].x - _c[_i].x, _c[(_i + 3u) % 4u].y - _c[_i].y };
            const f64 _ang = acos(fmax(-1.0, fmin(1.0, (_u.x * _v.x + _u.y * _v.y) / (hypot(_u.x, _u.y) * hypot(_v.x, _v.y) + 1e-300))));
            _square = fabs(_ang - FUDE_ZOOM_SHAPE_PI * 0.5) <= FUDE_ZOOM_SHAPE_SQUARE_ANG;
        }
        if(_square) {
            u32 _long = 0;
            f64 _best = -1.0;
            for(u32 _i = 0; _i < 4u; _i++) {
                const f64 _l = hypot(_c[(_i + 1u) % 4u].x - _c[_i].x, _c[(_i + 1u) % 4u].y - _c[_i].y);
                if(_l > _best) {
                    _best = _l;
                    _long = _i;
                }
            }
            f64 _rot = atan2(_c[(_long + 1u) % 4u].y - _c[_long].y, _c[(_long + 1u) % 4u].x - _c[_long].x);
            // Nearly level (or upright) is level: hands are a few degrees off.
            const f64 _q = FUDE_ZOOM_SHAPE_PI * 0.5;
            const f64 _snap = round(_rot / _q) * _q;
            if(fabs(_rot - _snap) < 0.12) {
                _rot = _snap;
            }
            const f64 _co = cos(_rot), _si = sin(_rot);
            f64 _minu = 1e300, _maxu = -1e300, _minv = 1e300, _maxv = -1e300;
            for(u32 _i = 0; _i < 4u; _i++) {
                const f64 _u = _c[_i].x * _co + _c[_i].y * _si, _v = -_c[_i].x * _si + _c[_i].y * _co;
                _minu = fmin(_minu, _u); _maxu = fmax(_maxu, _u);
                _minv = fmin(_minv, _v); _maxv = fmax(_maxv, _v);
            }
            const f64 _cu = (_minu + _maxu) * 0.5, _cv = (_minv + _maxv) * 0.5;
            _out->type     = FUDE_ZOOM_SHAPE_RECT;
            _out->at       = (fude_zoom_v2){ _cu * _co - _cv * _si, _cu * _si + _cv * _co };
            _out->rotation = _rot;
            _out->n[0]     = (_maxu - _minu) * 0.5;
            _out->n[1]     = (_maxv - _minv) * 0.5;
            _out->n[2]     = 0.0;
            _out->count    = 3u;
            return true;
        }
    }
    fude_zoom_v2 _mid = { 0.0, 0.0 };
    for(u32 _i = 0; _i < _k; _i++) {
        _mid.x += _c[_i].x / (f64)_k;
        _mid.y += _c[_i].y / (f64)_k;
    }
    _out->type  = FUDE_ZOOM_SHAPE_POLYGON;
    _out->at    = _mid;
    _out->count = _k * 2u;
    for(u32 _i = 0; _i < _k; _i++) {
        _out->n[_i * 2u]      = _c[_i].x - _mid.x;
        _out->n[_i * 2u + 1u] = _c[_i].y - _mid.y;
    }
    return true;
}

b8 fude_zoom_shape_recognize(const fude_zoom_v2* _p, u32 _n, fude_zoom_shape_fit* _out) {
    memset(_out, 0, sizeof(*_out));
    if(_n < 3u) {
        return false;
    }
    fude_zoom_box _b = fude_zoom_box_empty();
    f64 _length = 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        _b = fude_zoom_box_union(_b, (fude_zoom_box){ _p[_i].x, _p[_i].y, _p[_i].x, _p[_i].y });
        if(_i > 0) {
            _length += hypot(_p[_i].x - _p[_i - 1u].x, _p[_i].y - _p[_i - 1u].y);
        }
    }
    const f64 _size = hypot(_b.max_x - _b.min_x, _b.max_y - _b.min_y);
    if(_size < 12.0) {
        return false;   // a dot
    }

    // A LINE: nothing far from the chord between the ends.
    const fude_zoom_v2 _a = _p[0], _z = _p[_n - 1u];
    const f64 _chord = hypot(_z.x - _a.x, _z.y - _a.y);
    f64 _bow = 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _d = fude_zoom_shape_dist_segment(_p[_i], _a, _z);
        _bow = _d > _bow ? _d : _bow;
    }
    if(_chord > 0.0 && _bow <= FUDE_ZOOM_SHAPE_LINE_BOW * _chord && _chord >= 0.8 * _length) {
        _out->type  = FUDE_ZOOM_SHAPE_LINE;
        _out->at    = _a;
        _out->n[0]  = _z.x - _a.x;
        _out->n[1]  = _z.y - _a.y;
        _out->count = 2u;
        return true;
    }

    // The rest are closed: the ends must come back near each other.
    if(_chord > FUDE_ZOOM_SHAPE_CLOSED * _size) {
        return false;
    }

    // Corners: the path simplified, closed on its first point.
    u8 _keep[2048];
    const u32 _m = _n < 2048u ? _n : 2048u;
    memset(_keep, 0, _m);
    _keep[0] = _keep[_m - 1u] = 1u;
    fude_zoom_shape_simplify(_p, 0u, _m - 1u, FUDE_ZOOM_SHAPE_CORNER_TOL * _size, _keep, 0u);
    fude_zoom_v2 _c[64];
    u32 _k = 0;
    for(u32 _i = 0; _i < _m - 1u && _k < 64u; _i++) {   // the last point is the first again
        if(_keep[_i]) {
            _c[_k++] = _p[_i];
        }
    }
    // A corner where the ends meet often comes out twice, or on a straight run.
    for(u32 _pass = 0; _pass < 2u && _k > 3u; _pass++) {
        u32 _w = 0;
        for(u32 _i = 0; _i < _k; _i++) {
            const fude_zoom_v2 _prev = _c[(_i + _k - 1u) % _k], _next = _c[(_i + 1u) % _k];
            if(fude_zoom_shape_dist_segment(_c[_i], _prev, _next) > FUDE_ZOOM_SHAPE_CORNER_TOL * _size * 0.6) {
                _c[_w++] = _c[_i];
            }
        }
        _k = _w;
    }

    // A POLYGON candidate when its sides hug the path...
    const b8  _poly_ok  = _k >= 3u && _k <= FUDE_ZOOM_SHAPE_CORNERS;
    const f64 _poly_err = _poly_ok ? fude_zoom_shape_poly_error(_p, _m, _c, _k) : 1e300;

    // An ELLIPSE: its axes the path's own (principal axes), its radii its reach along them.
    fude_zoom_v2 _mean = { 0.0, 0.0 };
    for(u32 _i = 0; _i < _n; _i++) {
        _mean.x += _p[_i].x / (f64)_n;
        _mean.y += _p[_i].y / (f64)_n;
    }
    f64 _sxx = 0.0, _syy = 0.0, _sxy = 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _dx = _p[_i].x - _mean.x, _dy = _p[_i].y - _mean.y;
        _sxx += _dx * _dx; _syy += _dy * _dy; _sxy += _dx * _dy;
    }
    f64 _rot = 0.5 * atan2(2.0 * _sxy, _sxx - _syy);
    const f64 _co = cos(_rot), _si = sin(_rot);
    f64 _minu = 1e300, _maxu = -1e300, _minv = 1e300, _maxv = -1e300;
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _u = _p[_i].x * _co + _p[_i].y * _si, _v = -_p[_i].x * _si + _p[_i].y * _co;
        _minu = fmin(_minu, _u); _maxu = fmax(_maxu, _u);
        _minv = fmin(_minv, _v); _maxv = fmax(_maxv, _v);
    }
    f64 _rx = (_maxu - _minu) * 0.5, _ry = (_maxv - _minv) * 0.5;
    if(_rx <= 0.0 || _ry <= 0.0) {
        return false;
    }
    const f64 _cu = (_minu + _maxu) * 0.5, _cv = (_minv + _maxv) * 0.5;
    f64 _err = 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _u = (_p[_i].x * _co + _p[_i].y * _si - _cu) / _rx, _v = (-_p[_i].x * _si + _p[_i].y * _co - _cv) / _ry;
        _err += fabs(sqrt(_u * _u + _v * _v) - 1.0);
    }
    // ...and an ELLIPSE one: whichever hugs the path closer (a circle is near a
    // pentagon too, a rectangle near an ellipse), each within its own limit.
    const f64 _ell_err  = _err / (f64)_n;                  // in its radii
    const f64 _ell_dist = _ell_err * (_rx + _ry) * 0.5;     // the same, as a distance
    const b8  _ell_ok   = _ell_err <= FUDE_ZOOM_SHAPE_ELLIPSE_FIT;
    const b8  _poly_fit = _poly_err <= FUDE_ZOOM_SHAPE_POLY_FIT * _size;
    if(_poly_fit && (!_ell_ok || _poly_err < _ell_dist)) {
        return fude_zoom_shape_polygon(_c, _k, _out);
    }
    if(!_ell_ok) {
        return false;
    }
    if(fabs(_rx - _ry) <= FUDE_ZOOM_SHAPE_ROUND * fmax(_rx, _ry)) {
        _rx = _ry = (_rx + _ry) * 0.5;   // a circle
        _rot = 0.0;
    }
    _out->type     = FUDE_ZOOM_SHAPE_ELLIPSE;
    _out->at       = (fude_zoom_v2){ _cu * _co - _cv * _si, _cu * _si + _cv * _co };
    _out->rotation = _rot;
    _out->n[0]     = _rx;
    _out->n[1]     = _ry;
    _out->count    = 2u;
    return true;
}

b8 fude_zoom_shape_sculpt(const fude_zoom_v2* _old, u32 _m, const fude_zoom_v2* _new, u32 _n, f64 _reach, rde_arr* _out) {
    rde_arr_clear(_out);
    if(_m < 2u || _n < 2u) {
        return false;
    }
    // Where the new one begins and ends on it.
    u32 _i0 = 0, _i1 = 0;
    f64 _d0 = 1e300, _d1 = 1e300;
    for(u32 _j = 0; _j < _m; _j++) {
        const f64 _a = hypot(_old[_j].x - _new[0].x, _old[_j].y - _new[0].y), _b = hypot(_old[_j].x - _new[_n - 1u].x, _old[_j].y - _new[_n - 1u].y);
        if(_a < _d0) { _d0 = _a; _i0 = _j; }
        if(_b < _d1) { _d1 = _b; _i1 = _j; }
    }
    if(_d0 > _reach) {
        return false;
    }
    const b8 _ends_on = _d1 <= _reach && _i1 != _i0;
    if(_ends_on && _i0 < _i1) {
        for(u32 _j = 0; _j < _i0; _j++) rde_arr_add(_out, (any)&_old[_j]);
        for(u32 _j = 0; _j < _n; _j++) rde_arr_add(_out, (any)&_new[_j]);
        for(u32 _j = _i1 + 1u; _j < _m; _j++) rde_arr_add(_out, (any)&_old[_j]);
    } else if(_ends_on) {
        // Drawn against its way: put in turned round.
        for(u32 _j = 0; _j < _i1; _j++) rde_arr_add(_out, (any)&_old[_j]);
        for(u32 _j = _n; _j-- > 0;) rde_arr_add(_out, (any)&_new[_j]);
        for(u32 _j = _i0 + 1u; _j < _m; _j++) rde_arr_add(_out, (any)&_old[_j]);
    } else {
        // Ending off it: what lies the way the new one went is replaced (its tail, or its head).
        const fude_zoom_v2 _go  = { _new[_n > 6u ? 6u : _n - 1u].x - _new[0].x, _new[_n > 6u ? 6u : _n - 1u].y - _new[0].y };
        const u32          _on  = _i0 + 4u < _m ? _i0 + 4u : _m - 1u;
        const fude_zoom_v2 _fwd = { _old[_on].x - _old[_i0].x, _old[_on].y - _old[_i0].y };
        if(_go.x * _fwd.x + _go.y * _fwd.y >= 0.0) {
            for(u32 _j = 0; _j < _i0; _j++) rde_arr_add(_out, (any)&_old[_j]);
            for(u32 _j = 0; _j < _n; _j++) rde_arr_add(_out, (any)&_new[_j]);
        } else {
            for(u32 _j = _n; _j-- > 0;) rde_arr_add(_out, (any)&_new[_j]);
            for(u32 _j = _i0 + 1u; _j < _m; _j++) rde_arr_add(_out, (any)&_old[_j]);
        }
    }
    return true;
}

void fude_zoom_shape_mirror(u8 _type, f64* _n, u32 _count) {
    if(_type == FUDE_ZOOM_SHAPE_LINE && _count >= 2u) {
        _n[1] = -_n[1];
    } else if(_type == FUDE_ZOOM_SHAPE_DIMENSION && _count >= 3u) {
        _n[1] = -_n[1];
        _n[2] = -_n[2];   // (its line on the same side of what it measures, seen in the mirror)
    } else if(_type == FUDE_ZOOM_SHAPE_POLYGON) {
        for(u32 _i = 1; _i < _count; _i += 2u) {
            _n[_i] = -_n[_i];
        }
    } else if(_type == FUDE_ZOOM_SHAPE_ARC && _count >= 3u) {
        _n[1] = -_n[1];
        _n[2] = -_n[2];
    } else if(_type == FUDE_ZOOM_SHAPE_ARROW && _count >= 1u) {
        const u32 _k = (u32)_n[0];
        for(u32 _i = 0; _i < _k && 2u * _i + 2u < _count; _i++) {
            _n[2u + 2u * _i] = -_n[2u + 2u * _i];
        }
    } else if(_type == FUDE_ZOOM_SHAPE_PATH && _count >= 5u) {
        u64          _bits;
        fude_zoom_v2 _p[FUDE_ZOOM_PATH_NODES], _h[FUDE_ZOOM_PATH_NODES];
        const u32    _nodes = fude_zoom_path_unpack(_n, _count, &_bits, _p, _h);
        for(u32 _i = 0; _i < _nodes; _i++) {
            _p[_i].y = -_p[_i].y;
            _h[_i].y = -_h[_i].y;
        }
        fude_zoom_path_pack(_bits, _p, _h, _nodes, _n);
    } else if(_type == FUDE_ZOOM_SHAPE_BOARD && fude_zoom_board_is_cut(_n, _count)) {
        // Its rings' points: past its name, how many rings, then each one's count and points.
        u32 _at = FUDE_ZOOM_BOARD_RINGS_AT;
        const u32 _rings = _at < _count ? (u32)_n[_at] : 0u;
        _at++;
        for(u32 _r = 0; _r < _rings && _at < _count; _r++) {
            const u32 _k = (u32)_n[_at++];
            for(u32 _i = 0; _i < _k && _at + 1u < _count; _i++, _at += 2u) {
                _n[_at + 1u] = -_n[_at + 1u];
            }
        }
    }
}

// The part of segment _a-_b within _box: its start and end along it (0..1). False: none of it.
RDE_INTERNAL b8 fude_zoom_line_clip(fude_zoom_v2 _a, fude_zoom_v2 _b, fude_zoom_box _box, f64* _t0, f64* _t1) {
    f64 _lo = 0.0, _hi = 1.0;
    const f64 _d[2] = { _b.x - _a.x, _b.y - _a.y }, _p0[2] = { _a.x, _a.y }, _min[2] = { _box.min_x, _box.min_y }, _max[2] = { _box.max_x, _box.max_y };
    for(u32 _k = 0; _k < 2u; _k++) {
        if(fabs(_d[_k]) < 1e-300) {
            if(_p0[_k] < _min[_k] || _p0[_k] > _max[_k]) {
                return false;
            }
            continue;
        }
        f64 _u0 = (_min[_k] - _p0[_k]) / _d[_k], _u1 = (_max[_k] - _p0[_k]) / _d[_k];
        if(_u0 > _u1) { const f64 _t = _u0; _u0 = _u1; _u1 = _t; }
        _lo = fmax(_lo, _u0);
        _hi = fmin(_hi, _u1);
        if(_lo > _hi) {
            return false;
        }
    }
    *_t0 = _lo;
    *_t1 = _hi;
    return true;
}

u32 fude_zoom_line_dashes(const fude_zoom_v2* _p, u32 _n, b8 _closed, u8 _style, f64 _w, fude_zoom_box _view, rde_arr* _out) {
    rde_arr_clear(_out);
    if(_n < 2u || _style == FUDE_ZOOM_LINE_SOLID || _style >= FUDE_ZOOM_LINE_STYLES) {
        return 0u;
    }
    const f64 _u = fmax(_w, 1.2);
    f64 _pattern[4];
    u32 _np;
    if(_style == FUDE_ZOOM_LINE_DASHED) {
        _pattern[0] = 7.0 * _u; _pattern[1] = 3.5 * _u;
        _np = 2u;
    } else {
        _pattern[0] = 12.0 * _u; _pattern[1] = 3.0 * _u; _pattern[2] = fmax(0.6 * _u, 0.8); _pattern[3] = 3.0 * _u;
        _np = 4u;
    }
    f64 _period = 0.0;
    for(u32 _i = 0; _i < _np; _i++) {
        _period += _pattern[_i];
    }
    f64 _along = 0.0;   // how far along the line the pattern is
    const u32 _m = _closed ? _n : _n - 1u;
    for(u32 _i = 0; _i < _m && rde_arr_length(_out) < 2u * 200000u; _i++) {
        const fude_zoom_v2 _a = _p[_i], _b = _p[(_i + 1u) % _n];
        const f64 _len = hypot(_b.x - _a.x, _b.y - _a.y);
        f64 _t0, _t1;
        if(!(_len > 0.0) || !fude_zoom_line_clip(_a, _b, _view, &_t0, &_t1)) {
            _along += _len;
            continue;
        }
        // Walk the pattern over the part on the screen (what comes before it only counted).
        f64 _s = _t0 * _len;
        const f64 _end = _t1 * _len;
        while(_s < _end) {
            const f64 _phase = fmod(_along + _s, _period);
            u32 _k = 0;
            f64 _acc = 0.0;
            while(_k + 1u < _np && _acc + _pattern[_k] <= _phase) {
                _acc += _pattern[_k];
                _k++;
            }
            const f64 _left = _acc + _pattern[_k] - _phase;   // of this part of the pattern
            const f64 _to   = fmin(_s + fmax(_left, 1e-9 * _period), _end);
            if(_k % 2u == 0u) {
                const fude_zoom_v2 _q[2] = { { _a.x + (_b.x - _a.x) * _s / _len, _a.y + (_b.y - _a.y) * _s / _len },
                                             { _a.x + (_b.x - _a.x) * _to / _len, _a.y + (_b.y - _a.y) * _to / _len } };
                rde_arr_add(_out, (any)&_q[0]);
                rde_arr_add(_out, (any)&_q[1]);
            }
            _s = _to;
        }
        _along += _len;
    }
    return (u32)rde_arr_length(_out) / 2u;
}
