// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/shape.h"

#include <math.h>
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
    *_closed = _type != FUDE_ZOOM_SHAPE_LINE;
    if(_type == FUDE_ZOOM_SHAPE_LINE && _count >= 2u) {
        const fude_zoom_v2 _p[2] = { { 0.0, 0.0 }, { _n[0], _n[1] } };
        rde_arr_add(_out, (any)&_p[0]);
        rde_arr_add(_out, (any)&_p[1]);
    } else if(_type == FUDE_ZOOM_SHAPE_ELLIPSE && _count >= 2u) {
        for(u32 _i = 0; _i < _segments; _i++) {
            const f64          _a = 2.0 * FUDE_ZOOM_SHAPE_PI * (f64)_i / (f64)_segments;
            const fude_zoom_v2 _p = { cos(_a) * _n[0], sin(_a) * _n[1] };
            rde_arr_add(_out, (any)&_p);
        }
    } else if(_type == FUDE_ZOOM_SHAPE_RECT && _count >= 2u) {
        const f64 _hw = _n[0], _hh = _n[1];
        f64 _r = _count >= 3u ? _n[2] : 0.0;
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
    }
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
