// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/smooth.h"

#include <math.h>

f64 fude_zoom_smooth_sigma(u8 _level) {
    switch(_level) {
        case FUDE_ZOOM_SMOOTH_LOW:    return 2.0;
        case FUDE_ZOOM_SMOOTH_MEDIUM: return 5.0;
        case FUDE_ZOOM_SMOOTH_HIGH:   return 10.0;
        case FUDE_ZOOM_SMOOTH_ROPE:   return 2.0;
        default:                      return 0.0;
    }
}

// How much wider the Gaussian grows, at most, on the side of a point that has
// points, when the other side runs out (at an end): the fit there still sees
// as many points as in the middle, so an end is about as steady.
#define FUDE_ZOOM_SMOOTH_END_GROW 3.0

// Each point i is where a line fitted to its neighbours along the stroke (arc
// length s) puts it: least squares, weighted by a Gaussian of the distance
// times each point's share of the length (half its two segments, so a slow
// stretch, sampled densely, does not pull harder than a quick one). In the
// middle that is the Gaussian's mean, three widths each way. Near an end the
// neighbours are all on one side: the line still runs true to the end (no
// shrinking onto the pen's last shaky point — a hook), and the Gaussian on the
// side that has points widens as the other side runs out (up to END_GROW
// times), so the end is about as steady as the middle. A point is final once
// the end is past its window: nothing that comes later falls in it.
u32 fude_zoom_smooth_points(const fude_zoom_v2* _raw, u32 _n, f64 _sigma, fude_zoom_v2* _out, u32 _from) {
    if(_from >= _n) {
        return _n;
    }
    if(!(_sigma > 0.0) || _n < 3u) {
        for(u32 _i = _from; _i < _n; _i++) {
            _out[_i] = _raw[_i];
        }
        return _sigma > 0.0 ? (_n > 0u ? _n - 1u : 0u) : _n;
    }
    const f64 _reach = 3.0 * _sigma;
    const f64 _most  = _reach * FUDE_ZOOM_SMOOTH_END_GROW;
    // From where the windows begin: the start, or the widest window before _from.
    u32 _lo   = _from;
    f64 _back = 0.0;
    while(_lo > 0u && _back < _most) {
        _back += hypot(_raw[_lo].x - _raw[_lo - 1u].x, _raw[_lo].y - _raw[_lo - 1u].y);
        _lo--;
    }
    rde_arr TYPE(f64) _s_arr = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std());
    f64* _s = (f64*)rde_arr_add_n(&_s_arr, _n - _lo);   // (sized once: it stays put)
    _s[0] = 0.0;
    for(u32 _i = _lo + 1u; _i < _n; _i++) {
        _s[_i - _lo] = _s[_i - _lo - 1u] + hypot(_raw[_i].x - _raw[_i - 1u].x, _raw[_i].y - _raw[_i - 1u].y);
    }
    const f64 _end     = _s[_n - 1u - _lo];
    u32       _settled = _n;
    for(u32 _i = _from; _i < _n; _i++) {
        const f64 _si = _s[_i - _lo];
        // What each side is short of three widths (the start is far when _lo is not it).
        const f64 _short_back = _lo == 0u ? fmax(0.0, _reach - _si) : 0.0;
        const f64 _short_on   = fmax(0.0, _reach - (_end - _si));
        const f64 _sg_back = _sigma * (1.0 + (FUDE_ZOOM_SMOOTH_END_GROW - 1.0) * _short_on / _reach);
        const f64 _sg_on   = _sigma * (1.0 + (FUDE_ZOOM_SMOOTH_END_GROW - 1.0) * _short_back / _reach);
        if(_settled == _n && _end - _si < fmax(_reach, 3.0 * _sg_on)) {
            _settled = _i;
        }
        // The window: back to three of its widths (or the start), on to three (or the end).
        u32 _a = _i, _b = _i;
        while(_a > _lo && _si - _s[_a - 1u - _lo] < 3.0 * _sg_back) {
            _a--;
        }
        while(_b + 1u < _n && _s[_b + 1u - _lo] - _si < 3.0 * _sg_on) {
            _b++;
        }
        const f64 _inv_back = 1.0 / (2.0 * _sg_back * _sg_back);
        const f64 _inv_on   = 1.0 / (2.0 * _sg_on * _sg_on);
        f64 _w0 = 0.0, _w1 = 0.0, _w2 = 0.0, _x0 = 0.0, _x1 = 0.0, _y0 = 0.0, _y1 = 0.0;
        for(u32 _j = _a; _j <= _b; _j++) {
            const f64 _d    = _s[_j - _lo] - _si;
            const f64 _prev = _j > _lo ? _s[_j - _lo - 1u] : _s[_j - _lo];
            const f64 _next = _j + 1u < _n ? _s[_j - _lo + 1u] : _s[_j - _lo];
            const f64 _k    = exp(-_d * _d * (_d < 0.0 ? _inv_back : _inv_on)) * 0.5 * (_next - _prev);
            _w0 += _k;
            _w1 += _k * _d;
            _w2 += _k * _d * _d;
            _x0 += _k * _raw[_j].x;
            _x1 += _k * _d * _raw[_j].x;
            _y0 += _k * _raw[_j].y;
            _y1 += _k * _d * _raw[_j].y;
        }
        // The fitted line at d = 0; a mean when the points give no line (all at one place).
        const f64 _det = _w0 * _w2 - _w1 * _w1;
        if(_det > 1e-12 * _w0 * _w2 && _w0 > 0.0) {
            _out[_i] = (fude_zoom_v2){ (_w2 * _x0 - _w1 * _x1) / _det, (_w2 * _y0 - _w1 * _y1) / _det };
        } else if(_w0 > 0.0) {
            _out[_i] = (fude_zoom_v2){ _x0 / _w0, _y0 / _w0 };
        } else {
            _out[_i] = _raw[_i];
        }
    }
    rde_arr_free(&_s_arr);
    return _settled;
}

b8 fude_zoom_smooth_rope(fude_zoom_v2* _tip, fude_zoom_v2 _pen, f64 _length) {
    const f64 _dx = _pen.x - _tip->x, _dy = _pen.y - _tip->y;
    const f64 _d  = hypot(_dx, _dy);
    if(_d <= _length) {
        return false;
    }
    const f64 _k = (_d - _length) / _d;
    _tip->x += _dx * _k;
    _tip->y += _dy * _k;
    return true;
}
