// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/chart.h"
#include "zoom/plot.h"
#include "zoom/symbol.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See chart.h. Its text read into categories and their values; then drawn in
// the box less room round it for its numbers, its names and its title.
// ===========================================================================

#define FZC_PI      3.14159265358979323846
#define FZC_CATS    64u    // categories at most
#define FZC_SERIES  6u     // series at most (the palette's colours)
#define FZC_NAME    48u    // a name's bytes (plot.h' label's)

typedef struct {
    c8  title[FZC_NAME];
    c8  series[FZC_SERIES][FZC_NAME];
    u32 named;                          // series named
    c8  cat[FZC_CATS][FZC_NAME];
    f64 v[FZC_CATS][FZC_SERIES];
    u32 nv[FZC_CATS];                   // values on each one's line
    u32 ncat, nser;
} fzc_data;

b8 fude_zoom_chart_is(const c8* _id) {
    return _id != NULL && (strcmp(_id, "bar chart") == 0 || strcmp(_id, "line chart") == 0 || strcmp(_id, "pie chart") == 0);
}

const c8* fude_zoom_chart_example(const c8* _id) {
    if(_id != NULL && strcmp(_id, "pie chart") == 0) {
        return "Time\nWork 8\nSleep 8\nFamily 5\nOther 3";
    }
    return "Sales\nJan 120 80\nFeb 95 110\nMar 140 120\nseries: Shop, Online";
}

// A name copied in (cut at a whole character to fit).
RDE_INTERNAL void fzc_name(c8* _out, const c8* _a, const c8* _b) {
    usize _n = (usize)(_b - _a);
    if(_n > FZC_NAME - 1u) {
        _n = FZC_NAME - 1u;
        while(_n > 0u && ((u8)_a[_n] & 0xC0u) == 0x80u) {
            _n--;
        }
    }
    memcpy(_out, _a, _n);
    _out[_n] = 0;
}

RDE_INTERNAL b8 fzc_sep(c8 _c) {
    return _c == ' ' || _c == '\t' || _c == ',' || _c == ':' || _c == ';' || _c == '\r';
}

// A token a number (its "%" off)?
RDE_INTERNAL b8 fzc_number(const c8* _a, const c8* _b, f64* _v) {
    c8 _t[48];
    usize _n = (usize)(_b - _a);
    if(_n == 0u || _n >= sizeof(_t)) {
        return false;
    }
    memcpy(_t, _a, _n);
    _t[_n] = 0;
    if(_t[_n - 1u] == '%') {
        _t[--_n] = 0;
    }
    c8* _end = NULL;
    const f64 _x = strtod(_t, &_end);
    if(_end == _t || *_end != 0 || !isfinite(_x)) {
        return false;
    }
    *_v = _x;
    return true;
}

RDE_INTERNAL void fzc_read(const c8* _text, fzc_data* _d) {
    memset(_d, 0, sizeof(*_d));
    const c8* _p = _text != NULL ? _text : "";
    while(*_p != 0) {
        const c8* _e = _p;
        while(*_e != 0 && *_e != '\n') {
            _e++;
        }
        const c8* _a = _p;
        const c8* _b = _e;
        _p = *_e != 0 ? _e + 1 : _e;
        while(_a < _b && fzc_sep(*_a)) {
            _a++;
        }
        while(_b > _a && fzc_sep(_b[-1])) {
            _b--;
        }
        if(_a == _b) {
            continue;
        }
        // "series: A, B": the series' names.
        if(_b - _a > 7 && strncasecmp(_a, "series", 6u) == 0 && (_a[6] == ':' || _a[6] == ' ')) {
            const c8* _s = _a + 7;
            _d->named = 0;
            while(_s < _b && _d->named < FZC_SERIES) {
                while(_s < _b && (*_s == ' ' || *_s == ',' || *_s == ':')) {
                    _s++;
                }
                const c8* _t = _s;
                while(_t < _b && *_t != ',') {
                    _t++;
                }
                const c8* _te = _t;
                while(_te > _s && _te[-1] == ' ') {
                    _te--;
                }
                if(_te > _s) {
                    fzc_name(_d->series[_d->named++], _s, _te);
                }
                _s = _t;
            }
            continue;
        }
        // Its values: the numbers at its end (as many as are, read back from it); its name what is before them.
        f64 _vals[FZC_SERIES];
        u32 _nv = 0;
        const c8* _name_end = _b;
        f64 _back[16];
        u32 _nb = 0;
        while(_name_end > _a && _nb < 16u) {
            const c8* _t = _name_end;
            while(_t > _a && !fzc_sep(_t[-1])) {
                _t--;
            }
            f64 _x;
            if(!fzc_number(_t, _name_end, &_x)) {
                break;
            }
            _back[_nb++] = _x;
            _name_end = _t;
            while(_name_end > _a && fzc_sep(_name_end[-1])) {
                _name_end--;
            }
        }
        for(u32 _k = 0; _k < _nb && _nv < FZC_SERIES; _k++) {
            _vals[_nv++] = _back[_nb - 1u - _k];   // (in the line's order)
        }
        if(_nv == 0u) {
            if(_d->ncat == 0u && _d->title[0] == 0) {
                fzc_name(_d->title, _a, _b);   // (a first line with no number: its title)
            }
            continue;
        }
        if(_d->ncat >= FZC_CATS) {
            continue;
        }
        const u32 _c = _d->ncat++;
        fzc_name(_d->cat[_c], _a, _name_end);
        for(u32 _k = 0; _k < _nv; _k++) {
            _d->v[_c][_k] = _vals[_k];
        }
        _d->nv[_c] = _nv;
        _d->nser   = _nv > _d->nser ? _nv : _d->nser;
    }
}

// --- drawing ----------------------------------------------------------------------------------------

RDE_INTERNAL void fzc_part(rde_arr* _points, rde_arr* _parts, const fude_zoom_v2* _p, u32 _n, u8 _flags) {
    const fude_zoom_symbol_part _q = { (u32)rde_arr_length(_points), _n, _flags, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, 0.0f };
    for(u32 _i = 0; _i < _n; _i++) {
        rde_arr_add(_points, (any)&_p[_i]);
    }
    rde_arr_add(_parts, (any)&_q);
}

RDE_INTERNAL void fzc_line(rde_arr* _points, rde_arr* _parts, f64 _x0, f64 _y0, f64 _x1, f64 _y1, u8 _flags) {
    const fude_zoom_v2 _p[2] = { { _x0, _y0 }, { _x1, _y1 } };
    fzc_part(_points, _parts, _p, 2u, _flags);
}

RDE_INTERNAL void fzc_label(rde_arr* _labels, f64 _x, f64 _y, const c8* _text, u8 _align, u8 _curve) {
    if(_labels == NULL || _text == NULL || _text[0] == 0) {
        return;
    }
    fude_zoom_plot_label _l;
    memset(&_l, 0, sizeof(_l));
    _l.at = (fude_zoom_v2){ _x, _y };
    snprintf(_l.text, sizeof(_l.text), "%s", _text);
    _l.align = _align;
    _l.curve = _curve;
    rde_arr_add(_labels, (any)&_l);
}

// A tick's step: 1, 2 or 5 of a power of ten, about _n of them over _range.
RDE_INTERNAL f64 fzc_step(f64 _range, f64 _n) {
    const f64 _raw = _range / _n;
    const f64 _p = pow(10.0, floor(log10(_raw)));
    const f64 _f = _raw / _p;
    return (_f <= 1.0 ? 1.0 : _f <= 2.0 ? 2.0 : _f <= 5.0 ? 5.0 : 10.0) * _p;
}

RDE_INTERNAL void fzc_number_text(f64 _v, c8* _out, usize _size) {
    if(fabs(_v) < 1e-12) {
        _v = 0.0;
    }
    snprintf(_out, _size, "%.6g", _v);
}

// A dot: a small round region in series _k's colour.
RDE_INTERNAL void fzc_dot(rde_arr* _points, rde_arr* _parts, f64 _x, f64 _y, f64 _r, u32 _k) {
    fude_zoom_v2 _p[12];
    for(u32 _i = 0; _i < 12u; _i++) {
        const f64 _a = 2.0 * FZC_PI * (f64)_i / 12.0;
        _p[_i] = (fude_zoom_v2){ _x + cos(_a) * _r, _y + sin(_a) * _r };
    }
    fzc_part(_points, _parts, _p, 12u, (u8)(FUDE_ZOOM_CHART_SOLID + _k % FZC_SERIES));
}

u32 fude_zoom_chart_lines(const c8* _id, const c8* _text, f64 _hw, f64 _hh, rde_arr* _points, rde_arr* _parts, rde_arr* _labels) {
    if(!fude_zoom_chart_is(_id) || !(_hw > 0.0) || !(_hh > 0.0)) {
        return 0u;
    }
    fzc_data* _d = (fzc_data*)malloc(sizeof(fzc_data));
    if(_d == NULL) {
        return 0u;
    }
    fzc_read(_text, _d);
    const f64 _lh  = fude_zoom_plot_label_height(_id, _hw, _hh);   // (its labels' height)
    const f64 _top = _hh - (_d->title[0] != 0 ? _lh * 2.4 : _lh * 0.8);
    if(_d->title[0] != 0) {
        fzc_label(_labels, 0.0, _hh - _lh * 1.1, _d->title, 2u, 0u);
    }
    if(strcmp(_id, "pie chart") == 0) {
        // Its slices: each category's first value's share, from the top round clockwise, each its own colour.
        f64 _sum = 0.0;
        for(u32 _c = 0; _c < _d->ncat; _c++) {
            _sum += _d->nv[_c] > 0u && _d->v[_c][0] > 0.0 ? _d->v[_c][0] : 0.0;
        }
        const f64 _r = fmax(fmin(_hw * 0.62, (_top + _hh) * 0.5 - _lh * 2.2), _lh);
        const fude_zoom_v2 _mid = { 0.0, (_top - _hh) * 0.5 };
        f64 _a0 = FZC_PI * 0.5;
        for(u32 _c = 0; _c < _d->ncat && _sum > 0.0; _c++) {
            const f64 _v = _d->nv[_c] > 0u && _d->v[_c][0] > 0.0 ? _d->v[_c][0] : 0.0;
            if(_v <= 0.0) {
                continue;
            }
            const f64 _sweep = 2.0 * FZC_PI * _v / _sum, _a1 = _a0 - _sweep;
            fude_zoom_v2 _p[200];
            u32 _n = 0;
            _p[_n++] = _mid;
            const u32 _steps = (u32)fmin(fmax(ceil(_sweep / (FZC_PI / 90.0)), 2.0), 180.0);
            for(u32 _i = 0; _i <= _steps; _i++) {
                const f64 _a = _a0 + (_a1 - _a0) * (f64)_i / (f64)_steps;
                _p[_n++] = (fude_zoom_v2){ _mid.x + cos(_a) * _r, _mid.y + sin(_a) * _r };
            }
            fzc_part(_points, _parts, _p, _n, (u8)(FUDE_ZOOM_CHART_SOLID + _c % FZC_SERIES));
            _p[_n++] = _mid;   // (its edge closed: the slices apart)
            fzc_part(_points, _parts, _p, _n, 0u);
            // Its name and its share, out beyond its middle: from there outward (a slice on the right: from it on;
            // on the left: ending by it; at the top or the bottom: centred over or under it).
            const f64 _am = (_a0 + _a1) * 0.5, _ca = cos(_am), _sa = sin(_am);
            c8 _say[FZC_NAME];
            snprintf(_say, sizeof(_say), "%s %.0f%%", _d->cat[_c], 100.0 * _v / _sum);
            const u8 _side = _ca > 0.3 ? 3u : _ca < -0.3 ? 1u : 2u;
            fzc_label(_labels, _mid.x + _ca * (_r + _lh * 0.5), _mid.y + _sa * (_r + _lh * (_side == 2u ? 0.9 : 0.4)), _say, _side, 0u);
            _a0 = _a1;
        }
        const u32 _drawn = _d->ncat > 0u ? 1u : 0u;
        free(_d);
        return _drawn;
    }
    // Bars and lines: on axes, the values up, the categories along.
    const f64 _x0 = -_hw + _lh * 4.6, _x1 = _hw - _lh * 0.8, _y0 = -_hh + _lh * 2.2;
    const f64 _y1 = _top - (f64)_d->named * _lh * 1.3;   // (its legend's lines over the values: nothing under them)
    f64 _vmin = 0.0, _vmax = 0.0;
    for(u32 _c = 0; _c < _d->ncat; _c++) {
        for(u32 _k = 0; _k < _d->nv[_c]; _k++) {
            _vmin = fmin(_vmin, _d->v[_c][_k]);
            _vmax = fmax(_vmax, _d->v[_c][_k]);
        }
    }
    if(!(_vmax > _vmin)) {
        _vmax = _vmin + 1.0;
    }
    const f64 _step = fzc_step(_vmax - _vmin, 5.0);
    const f64 _lo = floor(_vmin / _step + 1e-9) * _step, _hi = ceil(_vmax / _step - 1e-9) * _step;   // (0 itself where all are above it)
    #define FZC_Y(_v) (_y0 + ((_v) - _lo) / (_hi - _lo) * (_y1 - _y0))
    // Its grid and its numbers, its axes.
    for(f64 _t = _lo; _t <= _hi + _step * 0.5 && _y1 > _y0; _t += _step) {
        const f64 _y = FZC_Y(_t);
        fzc_line(_points, _parts, _x0, _y, _x1, _y, FUDE_ZOOM_SYMBOL_DASHED);
        c8 _num[24];
        fzc_number_text(_t, _num, sizeof(_num));
        fzc_label(_labels, _x0 - _lh * 0.4, _y, _num, 1u, 0u);
    }
    fzc_line(_points, _parts, _x0, _y0, _x0, _y1, 0u);
    fzc_line(_points, _parts, _x0, FZC_Y(0.0), _x1, FZC_Y(0.0), 0u);
    const f64 _slot = _d->ncat > 0u ? (_x1 - _x0) / (f64)_d->ncat : 0.0;
    const u32 _ns = _d->nser > 0u ? _d->nser : 1u;
    const b8 _bars = strcmp(_id, "bar chart") == 0;
    for(u32 _c = 0; _c < _d->ncat; _c++) {
        const f64 _left = _x0 + _slot * (f64)_c, _mid = _left + _slot * 0.5;
        fzc_label(_labels, _mid, _y0 - _lh * 0.4, _d->cat[_c], 0u, 0u);
        for(u32 _k = 0; _bars && _k < _d->nv[_c]; _k++) {
            const f64 _bw = _slot * 0.7 / (f64)_ns, _bx = _left + _slot * 0.15 + _bw * (f64)_k;
            const f64 _ya = FZC_Y(0.0), _yb = FZC_Y(_d->v[_c][_k]);
            const fude_zoom_v2 _p[4] = { { _bx, _ya }, { _bx + _bw, _ya }, { _bx + _bw, _yb }, { _bx, _yb } };
            fzc_part(_points, _parts, _p, 4u, (u8)(FUDE_ZOOM_CHART_SOLID + _k % FZC_SERIES));
        }
    }
    if(!_bars) {
        // A series a line through its values at its categories' middles, a dot at each.
        for(u32 _k = 0; _k < _ns; _k++) {
            fude_zoom_v2 _p[FZC_CATS];
            u32 _n = 0;
            for(u32 _c = 0; _c < _d->ncat; _c++) {
                if(_k < _d->nv[_c]) {
                    _p[_n++] = (fude_zoom_v2){ _x0 + _slot * ((f64)_c + 0.5), FZC_Y(_d->v[_c][_k]) };
                }
            }
            if(_n >= 2u) {
                fzc_part(_points, _parts, _p, _n, (u8)(32u + _k % FZC_SERIES));
            }
            for(u32 _i = 0; _i < _n; _i++) {
                fzc_dot(_points, _parts, _p[_i].x, _p[_i].y, _lh * 0.28, _k);
            }
        }
    }
    #undef FZC_Y
    // Its legend: the series' names, each in its colour, in its top right corner (as wide as its longest name).
    u32 _longest = 0;
    for(u32 _k = 0; _k < _d->named; _k++) {
        u32 _chars = 0;
        for(const c8* _q = _d->series[_k]; *_q != 0; _q++) {
            _chars += ((u8)*_q & 0xC0u) != 0x80u ? 1u : 0u;
        }
        _longest = _chars > _longest ? _chars : _longest;
    }
    for(u32 _k = 0; _k < _d->named; _k++) {
        fzc_label(_labels, fmax(_x1 - _lh * (0.6 * (f64)_longest + 0.8), _x0 + _lh * 0.6), _top - _lh * 0.2, _d->series[_k], 4u, (u8)(1u + _k % FZC_SERIES));
    }
    const u32 _series = _d->ncat > 0u ? _ns : 0u;
    free(_d);
    return _series;
}
