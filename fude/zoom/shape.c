// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/shape.h"
#include "zoom/symbol.h"

// ===========================================================================
// See shape.h.
//
// No state: every function works on the numbers or points it is handed. In order: a shape's outline; a path's
// numbers and handles; a board's numbers; fitting an arc, the fillet, the hatch and the chamfer; a board's look, name
// and rings; recognising a stroke (with Douglas-Peucker, which fude_zoom_shape_fit_path shares); Sculpt; the mirror;
// the dashes. Names follow the header's: fude_zoom_shape_, fude_zoom_path_, fude_zoom_board_, fude_zoom_hatch_ and
// fude_zoom_line_.
// ===========================================================================

// How near a hand-drawn stroke must come to each shape to be taken for it (fude_zoom_shape_recognize). A "size" is
// the stroke's box's diagonal.
#define FUDE_ZOOM_SHAPE_LINE_BOW   0.06   // a line: no point further off it than this share of its length
#define FUDE_ZOOM_SHAPE_CLOSED     0.22   // closed: the ends nearer than this share of the drawing's size
#define FUDE_ZOOM_SHAPE_CORNER_TOL 0.07   // corners found where the path bends more than this share of its size
#define FUDE_ZOOM_SHAPE_POLY_FIT   0.045  // a polygon: the path this close to its sides (share of its size)
#define FUDE_ZOOM_SHAPE_ELLIPSE_FIT 0.16  // an ellipse: the path this close to it (in its own radii)
#define FUDE_ZOOM_SHAPE_ROUND      0.15   // an ellipse this near a circle is one
#define FUDE_ZOOM_SHAPE_SQUARE_ANG 0.30   // a quadrilateral's corners this near square (radians): a rectangle

// A dashed line's pieces, at most, in one call (fude_zoom_line_dashes): a screen holds a few thousand; more is a line
// far thinner than the view, which would look solid anyway.
#define FUDE_ZOOM_LINE_DASHES_MOST 200000u

// See the header: the circumference of a circle _size across, a segment every 6 points, held to 24..360.
u32 fude_zoom_shape_segments(f64 _size) {
    const f64 _n = _size * RDE_PI / 6.0;   // about 6 pt a segment
    return (u32)(_n < 24.0 ? 24.0 : (_n > 360.0 ? 360.0 : _n));
}

// A wall's outline (its numbers _n: its run and its thickness) into _out: the rectangle round its line, its ends
// squared off half its thickness past them.
RDE_INTERNAL void fude_zoom_shape_outline_wall(const f64* _n, rde_arr* _out) {
    const f64 _l = rde_math_hypot_f64(_n[0], _n[1]), _t = rde_math_abs_f64(_n[2]) * 0.5;
    const fude_zoom_v2 _u = _l > 0.0 ? (fude_zoom_v2){ _n[0] / _l, _n[1] / _l } : (fude_zoom_v2){ 1.0, 0.0 };
    const fude_zoom_v2 _v = { -_u.y * _t, _u.x * _t };
    const fude_zoom_v2 _a = { -_u.x * _t, -_u.y * _t }, _b = { _n[0] + _u.x * _t, _n[1] + _u.y * _t };
    const fude_zoom_v2 _c[4] = { { _a.x - _v.x, _a.y - _v.y }, { _b.x - _v.x, _b.y - _v.y }, { _b.x + _v.x, _b.y + _v.y }, { _a.x + _v.x, _a.y + _v.y } };
    for(u32 _i = 0; _i < 4u; _i++) {
        rde_arr_add(_out, (any)&_c[_i]);
    }
}

// A rectangle's, a board's or a sheet's outline (_type, its numbers _n, _count of them) into _out: its four corners,
// or, a rectangle with a corner radius, each corner a quarter circle of _segments / 4 pieces (2 at least).
RDE_INTERNAL void fude_zoom_shape_outline_rect(u8 _type, const f64* _n, u32 _count, u32 _segments, rde_arr* _out) {
    const f64 _hw = _n[0], _hh = _n[1];
    f64 _r = _count >= 3u && _type == FUDE_ZOOM_SHAPE_RECT ? _n[2] : 0.0;   // (a board's third number is its thickness, a sheet's its scale)
    _r = rde_math_min_f64(rde_math_max_f64(_r, 0.0), rde_math_min_f64(rde_math_abs_f64(_hw), rde_math_abs_f64(_hh)));
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
                const f64          _a = RDE_PI * 0.5 * ((f64)_k - 1.0 + (f64)_i / (f64)_per);
                const fude_zoom_v2 _p = { _c[_k].x + rde_math_cos_f64(_a) * _r, _c[_k].y + rde_math_sin_f64(_a) * _r };
                rde_arr_add(_out, (any)&_p);
            }
        }
    }
}

// A path's outline (its numbers _n, _count of them) into _out, and whether it is closed into *_closed: with _segments 1
// its nodes only; else each stretch a cubic from its node's handle to the next's, sampled alike, the whole held to
// 720 points.
RDE_INTERNAL void fude_zoom_shape_outline_path(const f64* _n, u32 _count, u32 _segments, rde_arr* _out, b8* _closed) {
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

// See the header. Each type is a branch of its own, and a type with too few numbers gives no points. With _segments
// 1 (what snapping asks for), an arc gives only its ends and middle, a path only its nodes and a symbol its quarters.
// A path's stretches are each a cubic from its node's handle to the next's, the whole held to 720 points.
void fude_zoom_shape_outline(u8 _type, const f64* _n, u32 _count, u32 _segments, rde_arr* _out, b8* _closed) {
    rde_arr_clear(_out);
    *_closed = _type != FUDE_ZOOM_SHAPE_LINE && _type != FUDE_ZOOM_SHAPE_DIMENSION && _type != FUDE_ZOOM_SHAPE_ARROW && _type != FUDE_ZOOM_SHAPE_ARC &&
               _type != FUDE_ZOOM_SHAPE_RADIAL && _type != FUDE_ZOOM_SHAPE_ANGLE && _type != FUDE_ZOOM_SHAPE_GUIDE && !fude_zoom_shape_is_attribute(_type) &&
               _type != FUDE_ZOOM_SHAPE_WIRE;
    if(_type == FUDE_ZOOM_SHAPE_WALL && _count >= 3u) {
        fude_zoom_shape_outline_wall(_n, _out);
        return;
    }
    if(_type == FUDE_ZOOM_SHAPE_WIRE) {
        const u32 _k = _count >= 1u && _n[0] >= 2.0 && _n[0] <= 1024.0 ? (u32)_n[0] : 0u;
        for(u32 _i = 0; _i < _k && 2u + 2u * _i < _count; _i++) {
            const fude_zoom_v2 _p = { _n[1u + 2u * _i], _n[2u + 2u * _i] };
            rde_arr_add(_out, (any)&_p);
        }
        return;
    }
    if(fude_zoom_shape_is_attribute(_type)) {
        const fude_zoom_v2 _p = { 0.0, 0.0 };   // (nothing to draw: a point, for its box)
        rde_arr_add(_out, (any)&_p);
        return;
    }
    if(_type == FUDE_ZOOM_SHAPE_GUIDE && _count >= 2u) {
        // A line as long as its reach each way (boxed, snapped and crossed by it).
        const f64 _l = rde_math_hypot_f64(_n[0], _n[1]);
        const f64 _r = _count >= 3u && _n[2] > 0.0 ? _n[2] : FUDE_ZOOM_GUIDE_REACH;
        const fude_zoom_v2 _u = _l > 0.0 ? (fude_zoom_v2){ _n[0] / _l * _r, _n[1] / _l * _r } : (fude_zoom_v2){ _r, 0.0 };
        const fude_zoom_v2 _p[2] = { { -_u.x, -_u.y }, _u };
        rde_arr_add(_out, (any)&_p[0]);
        rde_arr_add(_out, (any)&_p[1]);
        return;
    }
    if(_type == FUDE_ZOOM_SHAPE_RADIAL && _count >= 3u) {
        // Its line: from the centre (or right across) out to the circle.
        const fude_zoom_v2 _u = { rde_math_cos_f64(_n[1]) * _n[0], rde_math_sin_f64(_n[1]) * _n[0] };
        const fude_zoom_v2 _p[2] = { _n[2] >= 0.5 ? (fude_zoom_v2){ -_u.x, -_u.y } : (fude_zoom_v2){ 0.0, 0.0 }, _u };
        rde_arr_add(_out, (any)&_p[0]);
        rde_arr_add(_out, (any)&_p[1]);
        return;
    }
    if(_type == FUDE_ZOOM_SHAPE_SYMBOL && _count >= 3u) {
        // Its outline (its parts are fude_zoom_symbol_parts'); _segments 1: a round one's quarters only (snapping).
        fude_zoom_symbol_outline(fude_zoom_symbol_kind_of(_n, _count), _n[1], _n[2], _segments <= 1u ? 16u : _segments, _out);
        return;
    }
    if((_type == FUDE_ZOOM_SHAPE_ARC || _type == FUDE_ZOOM_SHAPE_ANGLE) && _count >= 3u) {
        // An arc, or an angle's arc (the same numbers: radius, from, sweep): its ends (and, _segments 1, only them and
        // its middle: what snapping looks at); else round as finely as a circle would be.
        const f64 _sweep = rde_math_max_f64(rde_math_min_f64(_n[2], 2.0 * RDE_PI), -2.0 * RDE_PI);
        u32 _k = _segments <= 1u ? 2u : (u32)rde_math_ceil_f64((f64)_segments * rde_math_abs_f64(_sweep) / (2.0 * RDE_PI));
        _k = _k < 2u ? 2u : _k;
        for(u32 _i = 0; _i <= _k; _i++) {
            const f64          _a = _n[1] + _sweep * (f64)_i / (f64)_k;
            const fude_zoom_v2 _p = { rde_math_cos_f64(_a) * _n[0], rde_math_sin_f64(_a) * _n[0] };
            rde_arr_add(_out, (any)&_p);
        }
        return;
    }
    if(_type == FUDE_ZOOM_SHAPE_ARROW && _count >= 1u) {
        const u32 _k = fude_zoom_arrow_points(_n, _count);
        for(u32 _i = 0; _i < _k && 2u * _i + 2u < _count; _i++) {
            const fude_zoom_v2 _p = { _n[1u + 2u * _i], _n[2u + 2u * _i] };
            rde_arr_add(_out, (any)&_p);
        }
        return;
    }
    if(_type == FUDE_ZOOM_SHAPE_DIMENSION && _count >= 3u) {
        // Where it measures from and to, and its line beside them (what it is
        // picked, boxed and snapped to by).
        const f64 _len = rde_math_hypot_f64(_n[0], _n[1]);
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
            const f64          _a = 2.0 * RDE_PI * (f64)_i / (f64)_segments;
            const fude_zoom_v2 _p = { rde_math_cos_f64(_a) * _n[0], rde_math_sin_f64(_a) * _n[1] };
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
        fude_zoom_shape_outline_rect(_type, _n, _count, _segments, _out);
    } else if(_type == FUDE_ZOOM_SHAPE_POLYGON) {
        for(u32 _i = 0; _i + 1u < _count; _i += 2u) {
            const fude_zoom_v2 _p = { _n[_i], _n[_i + 1u] };
            rde_arr_add(_out, (any)&_p);
        }
    } else if(_type == FUDE_ZOOM_SHAPE_PATH && _count >= 5u) {
        fude_zoom_shape_outline_path(_n, _count, _segments, _out, _closed);
    }
}

// See the header. The nodes come first (x y each), then, when FUDE_ZOOM_PATH_HANDLES is set, the handles in the same
// order; more than FUDE_ZOOM_PATH_NODES are left off.
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

// See the header. FUDE_ZOOM_PATH_HANDLES is set or cleared in the bits as handles are kept or not.
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

// See the header. The curve's own handle is Catmull-Rom's tangent over 3, as a cubic Bézier's handle is: a sixth of
// the way from the node before to the node after. At a corner, or an open path's end, the node itself stands in for
// the neighbour on that side, so the line runs straight along its stretch.
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

// See the header: the uniform Catmull-Rom cubic.
fude_zoom_v2 fude_zoom_shape_catmull(fude_zoom_v2 _a, fude_zoom_v2 _b, fude_zoom_v2 _c, fude_zoom_v2 _d, f64 _t) {
    const f64 _t2 = _t * _t, _t3 = _t2 * _t;
    return (fude_zoom_v2){
        0.5 * (2.0 * _b.x + (-_a.x + _c.x) * _t + (2.0 * _a.x - 5.0 * _b.x + 4.0 * _c.x - _d.x) * _t2 + (-_a.x + 3.0 * _b.x - 3.0 * _c.x + _d.x) * _t3),
        0.5 * (2.0 * _b.y + (-_a.y + _c.y) * _t + (2.0 * _a.y - 5.0 * _b.y + 4.0 * _c.y - _d.y) * _t2 + (-_a.y + 3.0 * _b.y - 3.0 * _c.y + _d.y) * _t3),
    };
}

// See the header. All FUDE_ZOOM_SHAPE_NUMBERS numbers are zeroed first. An unknown material is dropped from the look,
// keeping its grain. The name's bytes, its NUL included, are copied into the numbers from number 4 on, eight to a
// number, as their bits.
u32 fude_zoom_board_numbers(f64* _n, f64 _half_length, f64 _half_width, f64 _thickness_mm, u8 _look, const c8* _name) {
    rde_memset(_n, 0, FUDE_ZOOM_SHAPE_NUMBERS * sizeof(f64));
    _n[0] = _half_length;
    _n[1] = _half_width;
    _n[2] = _thickness_mm;
    _n[3] = (f64)((_look & 1u) | ((_look >> 1) < FUDE_ZOOM_MATERIAL_COUNT ? _look & ~1u : 0u));
    usize _len = _name != NULL ? rde_strlen(_name) : 0u;
    if(_len > FUDE_ZOOM_BOARD_NAME - 1u) {
        _len = FUDE_ZOOM_BOARD_NAME - 1u;
        while(_len > 0 && ((u8)_name[_len] & 0xC0u) == 0x80u) {
            _len--;
        }
    }
    u8 _bytes[FUDE_ZOOM_BOARD_NAME];
    rde_memset(_bytes, 0, sizeof(_bytes));
    if(_len > 0) {
        rde_memcpy(_bytes, _name, _len);
    }
    const u32 _words = (u32)((_len + 1u + 7u) / 8u);
    rde_memcpy(&_n[4], _bytes, (usize)_words * 8u);   // (its bits, as a connector keeps ids)
    return 4u + _words;
}

// See the header. The centre is Kåsa's least-squares circle; the radius is the points' mean distance from it, and
// the sweep is the points' turns round it added up, held to a whole turn either way.
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
    if(!(rde_math_abs_f64(_det) > 1e-300)) {
        return false;   // on a line
    }
    const f64 _d = -(_sxz * _syy - _syz * _sxy) / _det, _e = -(_sxx * _syz - _sxy * _sxz) / _det;
    const fude_zoom_v2 _c = { _m.x - _d * 0.5, _m.y - _e * 0.5 };
    f64 _r = 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        _r += rde_math_hypot_f64(_p[_i].x - _c.x, _p[_i].y - _c.y);
    }
    _r /= (f64)_n;
    if(!(_r > 0.0) || !rde_math_is_finite_f64(_r)) {
        return false;
    }
    for(u32 _i = 0; _i < _n; _i++) {
        if(rde_math_abs_f64(rde_math_hypot_f64(_p[_i].x - _c.x, _p[_i].y - _c.y) - _r) > _tolerance) {
            return false;
        }
    }
    // How far round, the way it went (each step the short way: the points are close).
    f64 _turn = 0.0, _was = rde_math_atan2_f64(_p[0].y - _c.y, _p[0].x - _c.x);
    const f64 _start = _was;
    for(u32 _i = 1; _i < _n; _i++) {
        const f64 _now = rde_math_atan2_f64(_p[_i].y - _c.y, _p[_i].x - _c.x);
        f64 _d_a = _now - _was;
        while(_d_a > RDE_PI)   { _d_a -= 2.0 * RDE_PI; }
        while(_d_a < -RDE_PI)  { _d_a += 2.0 * RDE_PI; }
        _turn += _d_a;
        _was   = _now;
    }
    *_centre = _c;
    *_radius = _r;
    *_from   = _start;
    *_sweep  = rde_math_max_f64(rde_math_min_f64(_turn, 2.0 * RDE_PI), -2.0 * RDE_PI);
    return true;
}

// See the header. The arc touches each line _radius / tan(half the angle) from the corner, its centre on the
// bisector _radius / sin(half the angle) from it, and its sweep is the short way round. Also false for a radius not
// above 0, a line of no length, or lines meeting straight or turned back on each other.
b8 fude_zoom_shape_fillet(fude_zoom_v2 _a0, fude_zoom_v2 _a1, fude_zoom_v2 _b0, fude_zoom_v2 _b1, f64 _radius,
                          fude_zoom_v2* _a_keep, fude_zoom_v2* _a_end, fude_zoom_v2* _b_keep, fude_zoom_v2* _b_end,
                          fude_zoom_v2* _centre, f64* _from, f64* _sweep) {
    // Where they meet, carried on.
    const fude_zoom_v2 _da = { _a1.x - _a0.x, _a1.y - _a0.y }, _db = { _b1.x - _b0.x, _b1.y - _b0.y };
    const f64 _cross = _da.x * _db.y - _da.y * _db.x;
    const f64 _la = rde_math_hypot_f64(_da.x, _da.y), _lb = rde_math_hypot_f64(_db.x, _db.y);
    if(!(_radius > 0.0) || !(_la > 0.0) || !(_lb > 0.0) || rde_math_abs_f64(_cross) <= 1e-12 * _la * _lb) {
        return false;
    }
    const f64 _t = ((_b0.x - _a0.x) * _db.y - (_b0.y - _a0.y) * _db.x) / _cross;
    const fude_zoom_v2 _x = { _a0.x + _da.x * _t, _a0.y + _da.y * _t };
    // Each line's end nearer the corner goes; its other end stays (the way from the corner along it: u).
    const b8 _a_near0 = rde_math_hypot_f64(_a0.x - _x.x, _a0.y - _x.y) < rde_math_hypot_f64(_a1.x - _x.x, _a1.y - _x.y);
    const b8 _b_near0 = rde_math_hypot_f64(_b0.x - _x.x, _b0.y - _x.y) < rde_math_hypot_f64(_b1.x - _x.x, _b1.y - _x.y);
    const fude_zoom_v2 _af = _a_near0 ? _a1 : _a0, _bf = _b_near0 ? _b1 : _b0;   // the far ends
    const f64 _fa = rde_math_hypot_f64(_af.x - _x.x, _af.y - _x.y), _fb = rde_math_hypot_f64(_bf.x - _x.x, _bf.y - _x.y);
    if(!(_fa > 0.0) || !(_fb > 0.0)) {
        return false;
    }
    const fude_zoom_v2 _ua = { (_af.x - _x.x) / _fa, (_af.y - _x.y) / _fa }, _ub = { (_bf.x - _x.x) / _fb, (_bf.y - _x.y) / _fb };
    const f64 _cos = rde_math_max_f64(rde_math_min_f64(_ua.x * _ub.x + _ua.y * _ub.y, 1.0), -1.0);
    const f64 _angle = rde_math_acos_f64(_cos);   // between them, at the corner
    if(!(_angle > 1e-9) || !(_angle < RDE_PI - 1e-9)) {
        return false;
    }
    const f64 _back = _radius / rde_math_tan_f64(_angle * 0.5);   // from the corner to where the arc touches each
    if(_back >= _fa || _back >= _fb) {
        return false;   // past a line's far end
    }
    const fude_zoom_v2 _ta = { _x.x + _ua.x * _back, _x.y + _ua.y * _back }, _tb = { _x.x + _ub.x * _back, _x.y + _ub.y * _back };
    const fude_zoom_v2 _bis = { _ua.x + _ub.x, _ua.y + _ub.y };
    const f64 _bl = rde_math_hypot_f64(_bis.x, _bis.y);
    const f64 _out = _radius / rde_math_sin_f64(_angle * 0.5);   // the corner to the centre, along the bisector
    const fude_zoom_v2 _c = { _x.x + _bis.x / _bl * _out, _x.y + _bis.y / _bl * _out };
    *_a_keep = _af; *_a_end = _ta;
    *_b_keep = _bf; *_b_end = _tb;
    *_centre = _c;
    const f64 _s = rde_math_atan2_f64(_ta.y - _c.y, _ta.x - _c.x), _e = rde_math_atan2_f64(_tb.y - _c.y, _tb.x - _c.x);
    f64 _sw = _e - _s;
    while(_sw > RDE_PI)  { _sw -= 2.0 * RDE_PI; }
    while(_sw < -RDE_PI) { _sw += 2.0 * RDE_PI; }   // (the short way: a fillet is under a half turn)
    *_from  = _s;
    *_sweep = _sw;
    return true;
}

// rde_sort's order for f64, ascending.
RDE_INTERNAL int fude_zoom_hatch_by_f64(const any _a, const any _b) {
    const f64 _x = *(const f64*)_a, _y = *(const f64*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// See the header. Each line is cut where it crosses the polygon's sides, and the stretches between the 1st and 2nd
// crossing, the 3rd and 4th and so on are inside. At most 256 crossings a line are counted, and nothing is drawn
// when the view would take more than 20 000 lines.
u32 fude_zoom_hatch_lines(const fude_zoom_v2* _p, u32 _n, f64 _angle, f64 _apart, fude_zoom_box _view, rde_arr* _out) {
    if(_n < 3u || !(_apart > 0.0)) {
        return 0;
    }
    // In turned axes: u along the lines, v across them; a line at each v = k·apart.
    const f64 _c = rde_math_cos_f64(_angle), _s = rde_math_sin_f64(_angle);
    f64 _v0 = 1e300, _v1 = -1e300;
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _v = -_p[_i].x * _s + _p[_i].y * _c;
        _v0 = rde_math_min_f64(_v0, _v);
        _v1 = rde_math_max_f64(_v1, _v);
    }
    // ...only where the view is (its corners' v).
    f64 _w0 = 1e300, _w1 = -1e300;
    const fude_zoom_v2 _corner[4] = { { _view.min_x, _view.min_y }, { _view.max_x, _view.min_y }, { _view.max_x, _view.max_y }, { _view.min_x, _view.max_y } };
    for(u32 _i = 0; _i < 4u; _i++) {
        const f64 _v = -_corner[_i].x * _s + _corner[_i].y * _c;
        _w0 = rde_math_min_f64(_w0, _v);
        _w1 = rde_math_max_f64(_w1, _v);
    }
    _v0 = rde_math_max_f64(_v0, _w0);
    _v1 = rde_math_min_f64(_v1, _w1);
    if(!(_v1 > _v0) || (_v1 - _v0) / _apart > 20000.0) {
        return 0;
    }
    const u32 _from = (u32)rde_arr_length(_out);
    f64 _cuts[256];
    // The lines counted by an integer index from the first whole step: an f64 counter would stop moving past 2^53
    // (a view far from 0 against its lines' spacing), and the loop would never end. At most 20 001 of them (above).
    const f64 _k0 = rde_math_ceil_f64(_v0 / _apart), _k1 = rde_math_floor_f64(_v1 / _apart);
    const u32 _lines = _k1 >= _k0 && _k1 - _k0 <= 20000.0 ? (u32)(_k1 - _k0) + 1u : 0u;
    for(u32 _j = 0; _j < _lines; _j++) {
        const f64 _v = (_k0 + (f64)_j) * _apart;
        u32 _m = 0;
        for(u32 _i = 0; _i < _n && _m < 256u; _i++) {
            const fude_zoom_v2 _a = _p[_i], _b = _p[(_i + 1u) % _n];
            const f64 _va = -_a.x * _s + _a.y * _c, _vb = -_b.x * _s + _b.y * _c;
            if((_va > _v) == (_vb > _v)) {
                continue;
            }
            const f64 _t = (_v - _va) / (_vb - _va);
            const fude_zoom_v2 _x = { _a.x + (_b.x - _a.x) * _t, _a.y + (_b.y - _a.y) * _t };
            _cuts[_m++] = _x.x * _c + _x.y * _s;   // (its u)
        }
        rde_sort(_cuts, _m, sizeof(f64), fude_zoom_hatch_by_f64);
        for(u32 _i = 0; _i + 1u < _m; _i += 2u) {
            const fude_zoom_v2 _a = { _cuts[_i] * _c - _v * _s, _cuts[_i] * _s + _v * _c };
            const fude_zoom_v2 _b = { _cuts[_i + 1u] * _c - _v * _s, _cuts[_i + 1u] * _s + _v * _c };
            rde_arr_add(_out, (any)&_a);
            rde_arr_add(_out, (any)&_b);
        }
    }
    return ((u32)rde_arr_length(_out) - _from) / 2u;
}

// See the header. Where the lines meet is found as for a fillet; each line's far end is the one further from it.
b8 fude_zoom_shape_chamfer(fude_zoom_v2 _a0, fude_zoom_v2 _a1, fude_zoom_v2 _b0, fude_zoom_v2 _b1, f64 _back,
                           fude_zoom_v2* _a_keep, fude_zoom_v2* _a_end, fude_zoom_v2* _b_keep, fude_zoom_v2* _b_end) {
    const fude_zoom_v2 _da = { _a1.x - _a0.x, _a1.y - _a0.y }, _db = { _b1.x - _b0.x, _b1.y - _b0.y };
    const f64 _cross = _da.x * _db.y - _da.y * _db.x;
    const f64 _la = rde_math_hypot_f64(_da.x, _da.y), _lb = rde_math_hypot_f64(_db.x, _db.y);
    if(!(_back >= 0.0) || !(_la > 0.0) || !(_lb > 0.0) || rde_math_abs_f64(_cross) <= 1e-12 * _la * _lb) {
        return false;
    }
    const f64 _t = ((_b0.x - _a0.x) * _db.y - (_b0.y - _a0.y) * _db.x) / _cross;
    const fude_zoom_v2 _x = { _a0.x + _da.x * _t, _a0.y + _da.y * _t };
    const b8 _a_near0 = rde_math_hypot_f64(_a0.x - _x.x, _a0.y - _x.y) < rde_math_hypot_f64(_a1.x - _x.x, _a1.y - _x.y);
    const b8 _b_near0 = rde_math_hypot_f64(_b0.x - _x.x, _b0.y - _x.y) < rde_math_hypot_f64(_b1.x - _x.x, _b1.y - _x.y);
    const fude_zoom_v2 _af = _a_near0 ? _a1 : _a0, _bf = _b_near0 ? _b1 : _b0;
    const f64 _fa = rde_math_hypot_f64(_af.x - _x.x, _af.y - _x.y), _fb = rde_math_hypot_f64(_bf.x - _x.x, _bf.y - _x.y);
    if(!(_fa > _back) || !(_fb > _back)) {
        return false;
    }
    *_a_keep = _af;
    *_b_keep = _bf;
    *_a_end  = (fude_zoom_v2){ _x.x + (_af.x - _x.x) / _fa * _back, _x.y + (_af.y - _x.y) / _fa * _back };
    *_b_end  = (fude_zoom_v2){ _x.x + (_bf.x - _x.x) / _fb * _back, _x.y + (_bf.y - _x.y) / _fb * _back };
    return true;
}

// See the header. A fourth number missing, out of 0..255, or with an unknown material gives no material.
u8 fude_zoom_board_look(const f64* _n, u32 _count) {
    if(_count < 4u || !(_n[3] >= 0.5) || !(_n[3] < 256.0)) {
        return 0;
    }
    const u32 _look = (u32)(_n[3] + 0.5);
    return (u8)((_look & 1u) | ((_look >> 1) < FUDE_ZOOM_MATERIAL_COUNT ? _look & ~1u : 0u));
}

// See the header: the look's bit 0.
u8 fude_zoom_board_grain(const f64* _n, u32 _count) {
    return fude_zoom_board_look(_n, _count) & 1u;
}

// See the header: the look's bits above its grain.
u8 fude_zoom_board_material(const f64* _n, u32 _count) {
    return fude_zoom_board_look(_n, _count) >> 1;
}

// See the header. The colours are in FUDE_ZOOM_MATERIAL_ order.
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

// See the header. At most FUDE_ZOOM_BOARD_NAME bytes are read, from number 4 on.
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
    rde_memcpy(_bytes, &_n[4], (usize)_words * 8u);
    _bytes[_words * 8u] = 0;
    usize _len = rde_strnlen((const c8*)_bytes, _words * 8u);
    if(_len >= _size) {
        _len = _size - 1u;
        while(_len > 0 && (_bytes[_len] & 0xC0u) == 0x80u) {
            _len--;
        }
    }
    rde_memcpy(_out, _bytes, _len);
    _out[_len] = 0;
}

// See the header: it has rings, at least one, past its name.
b8 fude_zoom_board_is_cut(const f64* _n, u32 _count) {
    return _count > FUDE_ZOOM_BOARD_RINGS_AT + 1u && _n[FUDE_ZOOM_BOARD_RINGS_AT] >= 1.0;
}

// See the header. The head is always FUDE_ZOOM_BOARD_RINGS_AT numbers (the name's twelve in full, zeros after it),
// so the rings start at the same number whatever the name's length.
u32 fude_zoom_board_cut_numbers(rde_arr* _out, f64 _half_length, f64 _half_width, f64 _thickness_mm, u8 _look, const c8* _name,
                                const fude_zoom_v2* _points, const u32* _rings, u32 _n) {
    rde_arr_clear(_out);
    f64 _head[FUDE_ZOOM_SHAPE_NUMBERS];
    fude_zoom_board_numbers(_head, _half_length, _half_width, _thickness_mm, _look, _name);
    rde_memcpy(rde_arr_add_n(_out, FUDE_ZOOM_BOARD_RINGS_AT), _head, FUDE_ZOOM_BOARD_RINGS_AT * sizeof(f64));   // (its name's twelve numbers, all of them)
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

// A board cut's count kept as a number (its rings, a ring's points): 0 when it is not one a u32 holds (a damaged
// file's: NaN, negative or vast, which a cast would not hold).
RDE_INTERNAL u32 fude_zoom_board_count_of(f64 _v) {
    return _v >= 0.0 && _v < 4294967296.0 ? (u32)_v : 0u;
}

// See the header. A cut board's rings are read no further than its numbers go; a ring's number is its place (0 the
// outline).
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
    const u32 _ring_count = fude_zoom_board_count_of(_n[FUDE_ZOOM_BOARD_RINGS_AT]);
    u32 _at = FUDE_ZOOM_BOARD_RINGS_AT + 1u;
    for(u32 _r = 0; _r < _ring_count && _at < _count; _r++) {
        const u32 _k = fude_zoom_board_count_of(_n[_at++]);
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
// The distance from a point to a segment is rde_math's (rde_math_point_segment_distance_2d_f64: to the segment's start
// when it has no length).

// Douglas-Peucker on _p[_from .. _to]: marks in _keep the points kept between the two ends (which the caller marks):
// the point furthest from the chord when it is further than _tol, then the same on each side of it. Stops at 64
// levels deep.
RDE_INTERNAL void fude_zoom_shape_simplify(const fude_zoom_v2* _p, u32 _from, u32 _to, f64 _tol, u8* _keep, u32 _depth) {
    if(_to <= _from + 1u || _depth > 64u) {
        return;
    }
    f64 _far = -1.0;
    u32 _at  = _from;
    for(u32 _i = _from + 1u; _i < _to; _i++) {
        const f64 _d = rde_math_point_segment_distance_2d_f64(_p[_i], _p[_from], _p[_to]);
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

// See the header. The tolerance grows by 1.4 a try, for at most 40 tries; if the nodes still do not fit, the first
// _most are kept, the last of them moved to the line's end.
u32 fude_zoom_shape_fit_path(const fude_zoom_v2* _p, u32 _n, f64 _tol, u32 _most, fude_zoom_v2* _out) {
    if(_n == 0 || _most < 2u) {
        return 0;
    }
    if(_n == 1u) {
        _out[0] = _p[0];
        return 1u;
    }
    rde_arr TYPE(u8) _keep_arr = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_keep_arr, _n);
    u8* _keep = (u8*)_keep_arr.memory;   // (sized once: it stays put)
    // Simplified until it fits in _most nodes (the tolerance let out each time).
    u32 _kept = 0;
    for(u32 _tries = 0; _tries < 40u; _tries++, _tol *= 1.4) {
        rde_memset(_keep, 0, _n);
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
    rde_arr_free(&_keep_arr);
    return _m;
}

// Returns how far, on average, the path's _n points are from the closed polygon _c (its _k corners): each point's
// distance to the nearest side.
RDE_INTERNAL f64 fude_zoom_shape_poly_error(const fude_zoom_v2* _p, u32 _n, const fude_zoom_v2* _c, u32 _k) {
    f64 _sum = 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        f64 _best = 1e300;
        for(u32 _j = 0; _j < _k; _j++) {
            const f64 _d = rde_math_point_segment_distance_2d_f64(_p[_i], _c[_j], _c[(_j + 1u) % _k]);
            _best = _d < _best ? _d : _best;
        }
        _sum += _best;
    }
    return _sum / (f64)_n;
}

// Writes the corners _c (_k of them) as a shape into *_out: four with every corner within FUDE_ZOOM_SHAPE_SQUARE_ANG of
// square a RECTANGLE, turned as its longest side (and level or upright when nearly so), its box in that turn; any
// other a POLYGON round their mean. Returns true.
RDE_INTERNAL b8 fude_zoom_shape_polygon(const fude_zoom_v2* _c, u32 _k, fude_zoom_shape_fit* _out) {
    if(_k == 4u) {
        // Four corners near square: a RECTANGLE, turned as its longest side.
        b8 _square = true;
        for(u32 _i = 0; _i < 4u && _square; _i++) {
            const fude_zoom_v2 _u = { _c[(_i + 1u) % 4u].x - _c[_i].x, _c[(_i + 1u) % 4u].y - _c[_i].y };
            const fude_zoom_v2 _v = { _c[(_i + 3u) % 4u].x - _c[_i].x, _c[(_i + 3u) % 4u].y - _c[_i].y };
            const f64 _ang = rde_math_acos_f64(rde_math_max_f64(-1.0, rde_math_min_f64(1.0, (_u.x * _v.x + _u.y * _v.y) / (rde_math_hypot_f64(_u.x, _u.y) * rde_math_hypot_f64(_v.x, _v.y) + 1e-300))));
            _square = rde_math_abs_f64(_ang - RDE_PI * 0.5) <= FUDE_ZOOM_SHAPE_SQUARE_ANG;
        }
        if(_square) {
            u32 _long = 0;
            f64 _best = -1.0;
            for(u32 _i = 0; _i < 4u; _i++) {
                const f64 _l = rde_math_hypot_f64(_c[(_i + 1u) % 4u].x - _c[_i].x, _c[(_i + 1u) % 4u].y - _c[_i].y);
                if(_l > _best) {
                    _best = _l;
                    _long = _i;
                }
            }
            f64 _rot = rde_math_atan2_f64(_c[(_long + 1u) % 4u].y - _c[_long].y, _c[(_long + 1u) % 4u].x - _c[_long].x);
            const f64 _q = RDE_PI * 0.5;
            // (turned half round it is the same rectangle: its long side's way within a quarter turn of level — drawn
            // from any corner, either way round, the same rectangle)
            if(_rot > _q) {
                _rot -= RDE_PI;
            } else if(_rot <= -_q) {
                _rot += RDE_PI;
            }
            // Nearly level (or upright) is level: hands are a few degrees off.
            const f64 _snap = rde_math_round_f64(_rot / _q) * _q;
            if(rde_math_abs_f64(_rot - _snap) < 0.12) {
                _rot = _snap;
            }
            const f64 _co = rde_math_cos_f64(_rot), _si = rde_math_sin_f64(_rot);
            f64 _minu = 1e300, _maxu = -1e300, _minv = 1e300, _maxv = -1e300;
            for(u32 _i = 0; _i < 4u; _i++) {
                const f64 _u = _c[_i].x * _co + _c[_i].y * _si, _v = -_c[_i].x * _si + _c[_i].y * _co;
                _minu = rde_math_min_f64(_minu, _u); _maxu = rde_math_max_f64(_maxu, _u);
                _minv = rde_math_min_f64(_minv, _v); _maxv = rde_math_max_f64(_maxv, _v);
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

// See the header. In order: a stroke under 12 points across is a dot (none); one close to the chord between its ends,
// and not much longer than it, is a LINE; any other must end near where it began. Then its corners are found
// (Douglas-Peucker, with those that come out twice where the ends meet, or on a straight run, dropped) and the
// polygon through them is weighed against the ellipse on the stroke's principal axes: whichever hugs the stroke closer
// wins, each within its own limit. Every point is looked at for corners (a shape drawn slowly at the pen's rate has
// thousands), and 64 corners are kept at most.
b8 fude_zoom_shape_recognize(const fude_zoom_v2* _p, u32 _n, fude_zoom_shape_fit* _out) {
    rde_memset(_out, 0, sizeof(*_out));
    if(_n < 3u) {
        return false;
    }
    fude_zoom_box _b = fude_zoom_box_empty();
    f64 _length = 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        _b = fude_zoom_box_union(_b, (fude_zoom_box){ _p[_i].x, _p[_i].y, _p[_i].x, _p[_i].y });
        if(_i > 0) {
            _length += rde_math_hypot_f64(_p[_i].x - _p[_i - 1u].x, _p[_i].y - _p[_i - 1u].y);
        }
    }
    const f64 _size = rde_math_hypot_f64(_b.max_x - _b.min_x, _b.max_y - _b.min_y);
    if(_size < 12.0) {
        return false;   // a dot
    }

    // A LINE: nothing far from the chord between the ends.
    const fude_zoom_v2 _a = _p[0], _z = _p[_n - 1u];
    const f64 _chord = rde_math_hypot_f64(_z.x - _a.x, _z.y - _a.y);
    f64 _bow = 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _d = rde_math_point_segment_distance_2d_f64(_p[_i], _a, _z);
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

    // Corners: the path simplified, closed on its first point. The marks are as many as its points (std allocator:
    // they grow with the stroke). At most 64 corners are kept: a polygon has at most FUDE_ZOOM_SHAPE_CORNERS, and a
    // list cut short at 64 is only ever weighed against the whole stroke below, which it cannot hug, so no shape is
    // made of part of one.
    rde_arr TYPE(u8) _keep_arr = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_keep_arr, _n);
    u8* _keep = (u8*)_keep_arr.memory;
    rde_memset(_keep, 0, _n);
    _keep[0] = _keep[_n - 1u] = 1u;
    fude_zoom_shape_simplify(_p, 0u, _n - 1u, FUDE_ZOOM_SHAPE_CORNER_TOL * _size, _keep, 0u);
    fude_zoom_v2 _c[64];
    u32 _k = 0;
    for(u32 _i = 0; _i < _n - 1u && _k < 64u; _i++) {   // the last point is the first again
        if(_keep[_i]) {
            _c[_k++] = _p[_i];
        }
    }
    rde_arr_free(&_keep_arr);
    // A corner where the ends meet often comes out twice, or on a straight run.
    for(u32 _pass = 0; _pass < 2u && _k > 3u; _pass++) {
        u32 _w = 0;
        for(u32 _i = 0; _i < _k; _i++) {
            const fude_zoom_v2 _prev = _c[(_i + _k - 1u) % _k], _next = _c[(_i + 1u) % _k];
            if(rde_math_point_segment_distance_2d_f64(_c[_i], _prev, _next) > FUDE_ZOOM_SHAPE_CORNER_TOL * _size * 0.6) {
                _c[_w++] = _c[_i];
            }
        }
        _k = _w;
    }

    // A POLYGON candidate when its sides hug the path...
    const b8  _poly_ok  = _k >= 3u && _k <= FUDE_ZOOM_SHAPE_CORNERS;
    const f64 _poly_err = _poly_ok ? fude_zoom_shape_poly_error(_p, _n, _c, _k) : 1e300;

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
    f64 _rot = 0.5 * rde_math_atan2_f64(2.0 * _sxy, _sxx - _syy);
    const f64 _co = rde_math_cos_f64(_rot), _si = rde_math_sin_f64(_rot);
    f64 _minu = 1e300, _maxu = -1e300, _minv = 1e300, _maxv = -1e300;
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _u = _p[_i].x * _co + _p[_i].y * _si, _v = -_p[_i].x * _si + _p[_i].y * _co;
        _minu = rde_math_min_f64(_minu, _u); _maxu = rde_math_max_f64(_maxu, _u);
        _minv = rde_math_min_f64(_minv, _v); _maxv = rde_math_max_f64(_maxv, _v);
    }
    f64 _rx = (_maxu - _minu) * 0.5, _ry = (_maxv - _minv) * 0.5;
    if(_rx <= 0.0 || _ry <= 0.0) {
        return false;
    }
    const f64 _cu = (_minu + _maxu) * 0.5, _cv = (_minv + _maxv) * 0.5;
    f64 _err = 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _u = (_p[_i].x * _co + _p[_i].y * _si - _cu) / _rx, _v = (-_p[_i].x * _si + _p[_i].y * _co - _cv) / _ry;
        _err += rde_math_abs_f64(rde_math_sqrt_f64(_u * _u + _v * _v) - 1.0);
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
    if(rde_math_abs_f64(_rx - _ry) <= FUDE_ZOOM_SHAPE_ROUND * rde_math_max_f64(_rx, _ry)) {
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

// See the header. Where _new begins and ends on the line is the line's point nearest each of its ends. The way _new
// went, when it ends off the line, is its first few points' way, against the line's way from where _new began.
b8 fude_zoom_shape_sculpt(const fude_zoom_v2* _old, u32 _m, const fude_zoom_v2* _new, u32 _n, f64 _reach, rde_arr* _out) {
    rde_arr_clear(_out);
    if(_m < 2u || _n < 2u) {
        return false;
    }
    // Where the new one begins and ends on it.
    u32 _i0 = 0, _i1 = 0;
    f64 _d0 = 1e300, _d1 = 1e300;
    for(u32 _j = 0; _j < _m; _j++) {
        const f64 _a = rde_math_hypot_f64(_old[_j].x - _new[0].x, _old[_j].y - _new[0].y), _b = rde_math_hypot_f64(_old[_j].x - _new[_n - 1u].x, _old[_j].y - _new[_n - 1u].y);
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

// See the header. Each number that is a y, an angle or a side is turned over; the rest are left.
void fude_zoom_shape_mirror(u8 _type, f64* _n, u32 _count) {
    // A WIRE is left as it is: the parts it joins are symbols, which a mirror only turns (they have no mirror image),
    // so a wire turned with them still meets their pins, where a flipped one would leave them.
    if((_type == FUDE_ZOOM_SHAPE_LINE || _type == FUDE_ZOOM_SHAPE_WALL || _type == FUDE_ZOOM_SHAPE_GUIDE) && _count >= 2u) {
        _n[1] = -_n[1];   // the way it goes: x y
    } else if(_type == FUDE_ZOOM_SHAPE_RADIAL && _count >= 2u) {
        _n[1] = -_n[1];   // its angle
    } else if(_type == FUDE_ZOOM_SHAPE_DIMENSION && _count >= 3u) {
        _n[1] = -_n[1];
        _n[2] = -_n[2];   // (its line on the same side of what it measures, seen in the mirror)
    } else if(_type == FUDE_ZOOM_SHAPE_POLYGON) {
        for(u32 _i = 1; _i < _count; _i += 2u) {
            _n[_i] = -_n[_i];
        }
    } else if((_type == FUDE_ZOOM_SHAPE_ARC || _type == FUDE_ZOOM_SHAPE_ANGLE) && _count >= 3u) {   // from, sweep
        _n[1] = -_n[1];
        _n[2] = -_n[2];
    } else if(_type == FUDE_ZOOM_SHAPE_ARROW && _count >= 1u) {
        const u32 _k = fude_zoom_arrow_points(_n, _count);
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
        const u32 _rings = _at < _count ? fude_zoom_board_count_of(_n[_at]) : 0u;
        _at++;
        for(u32 _r = 0; _r < _rings && _at < _count; _r++) {
            const u32 _k = fude_zoom_board_count_of(_n[_at++]);
            for(u32 _i = 0; _i < _k && _at + 1u < _count; _i++, _at += 2u) {
                _n[_at + 1u] = -_n[_at + 1u];
            }
        }
    }
}

// See the header. A width under 1.2 is taken as 1.2, and a centre line's dot is at least 0.8 long. Only the part of
// each segment in _view is walked (zoom.h's fude_zoom_clip_line); what lies before it is only counted, so the dashes
// stay in step along the line.
// The walk is measured from where the segment enters the view, not from its start: deep in, a segment may be
// millions of units long, and a position that far along it would not move when a dash's length is added to it (the
// loop would never end). The pattern's place is worked out once where it enters (from the length before it), then
// stepped a whole part of the pattern at a time. At most FUDE_ZOOM_LINE_DASHES_MOST pieces are made.
u32 fude_zoom_line_dashes(const fude_zoom_v2* _p, u32 _n, b8 _closed, u8 _style, f64 _w, fude_zoom_box _view, rde_arr* _out) {
    rde_arr_clear(_out);
    if(_n < 2u || _style == FUDE_ZOOM_LINE_SOLID || _style >= FUDE_ZOOM_LINE_STYLES) {
        return 0u;
    }
    const f64 _u = rde_math_max_f64(_w, 1.2);
    f64 _pattern[4];
    u32 _np;
    if(_style == FUDE_ZOOM_LINE_DASHED) {
        _pattern[0] = 7.0 * _u; _pattern[1] = 3.5 * _u;
        _np = 2u;
    } else {
        _pattern[0] = 12.0 * _u; _pattern[1] = 3.0 * _u; _pattern[2] = rde_math_max_f64(0.6 * _u, 0.8); _pattern[3] = 3.0 * _u;
        _np = 4u;
    }
    f64 _period = 0.0;
    for(u32 _i = 0; _i < _np; _i++) {
        _period += _pattern[_i];
    }
    f64 _along = 0.0;   // how far along the line the pattern is
    const u32 _m = _closed ? _n : _n - 1u;
    for(u32 _i = 0; _i < _m && rde_arr_length(_out) < 2u * FUDE_ZOOM_LINE_DASHES_MOST; _i++) {
        const fude_zoom_v2 _a = _p[_i], _b = _p[(_i + 1u) % _n];
        const f64 _len = rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y);
        fude_zoom_v2 _from = _a, _to = _b;   // (cut to the view: the part on the screen)
        if(!(_len > 0.0) || !rde_math_is_finite_f64(_len) || !fude_zoom_clip_line(&_from, &_to, _view)) {
            _along += rde_math_is_finite_f64(_len) ? _len : 0.0;   // (a length no number holds counts as none)
            continue;
        }
        // The part on the screen: from where it enters (_from) along _dir, _span long. Where the pattern is there:
        // in its part _k, with _left of that part still to go.
        const fude_zoom_v2 _dir  = { (_b.x - _a.x) / _len, (_b.y - _a.y) / _len };
        const f64 _span  = rde_math_hypot_f64(_to.x - _from.x, _to.y - _from.y);
        const f64 _phase = rde_math_fmod_f64(_along + rde_math_hypot_f64(_from.x - _a.x, _from.y - _a.y), _period);
        u32 _k = 0;
        f64 _acc = 0.0;
        while(_k + 1u < _np && _acc + _pattern[_k] <= _phase) {
            _acc += _pattern[_k];
            _k++;
        }
        f64 _left = rde_math_max_f64(_acc + _pattern[_k] - _phase, 0.0);
        // Then a whole part of the pattern a step (each at least 0.8 long, so each step moves on), a dash for each
        // even part, until the end of the part on the screen or the most pieces.
        f64 _o = 0.0;
        while(_o < _span && rde_arr_length(_out) < 2u * FUDE_ZOOM_LINE_DASHES_MOST) {
            const f64 _to = rde_math_min_f64(_o + _left, _span);
            if(_k % 2u == 0u && _to > _o) {
                const fude_zoom_v2 _q[2] = { { _from.x + _dir.x * _o, _from.y + _dir.y * _o }, { _from.x + _dir.x * _to, _from.y + _dir.y * _to } };
                rde_arr_add(_out, (any)&_q[0]);
                rde_arr_add(_out, (any)&_q[1]);
            }
            if(!(_to > _o) && _left > 0.0) {
                break;   // (a part of the pattern too short to move the place along: the screen is millions of them)
            }
            _o    = _to;
            _k    = (_k + 1u) % _np;
            _left = _pattern[_k];
        }
        _along += _len;
    }
    return (u32)rde_arr_length(_out) / 2u;
}
