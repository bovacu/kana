// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/chart.h"
#include "zoom/plot.h"
#include "zoom/symbol.h"
#include "zoom/calc.h"
#include "drawing/base/utf8.h"

// ===========================================================================
// See chart.h.
//
// Its text is read into categories and their values first (fzch_read, into an fzch_data); then the chart is drawn in
// the box less the room round it for its numbers, its categories' names, its title and its legend, all of it measured
// in its labels' height (fude_zoom_plot_label_height), so it scales with the symbol. Its helpers are fzch_.
// ===========================================================================

#define FZCH_CATS    64u    // categories at most
#define FZCH_SERIES  6u     // series at most (the palette's colours)
#define FZCH_NAME    48u    // a name's bytes, its 0 included (as many as plot.h's label holds)

// A chart's text, read: its title, its series' names (`named` of them), and its categories (`ncat`), each with its
// name and up to FZCH_SERIES values (nv[c] of them). nser is the most values any category has.
typedef struct {
    c8  title[FZCH_NAME];
    c8  series[FZCH_SERIES][FZCH_NAME];
    u32 named;                          // series named
    c8  cat[FZCH_CATS][FZCH_NAME];
    f64 v[FZCH_CATS][FZCH_SERIES];
    u32 nv[FZCH_CATS];                   // values on each one's line
    u32 ncat, nser;
} fzch_data;

// See chart.h.
b8 fude_zoom_chart_is(const c8* _id) {
    return _id != NULL && (rde_strcmp(_id, "bar chart") == 0 || rde_strcmp(_id, "line chart") == 0 || rde_strcmp(_id, "pie chart") == 0);
}

// See chart.h.
const c8* fude_zoom_chart_example(const c8* _id) {
    if(_id != NULL && rde_strcmp(_id, "pie chart") == 0) {
        return "Time\nWork 8\nSleep 8\nFamily 5\nOther 3";
    }
    return "Sales\nJan 120 80\nFeb 95 110\nMar 140 120\nseries: Shop, Online";
}

// Copies the text from _a up to _b into _out as a name: cut to FZCH_NAME - 1 bytes, at a whole character, when longer.
RDE_INTERNAL void fzch_name(c8* _out, const c8* _a, const c8* _b) {
    fude_utf8_copy(_out, FZCH_NAME, _a, (usize)(_b - _a));
}

// True for what may separate a line's name and values: a space, a tab, a comma, a colon, a semicolon, or a carriage
// return (a line ending in \r\n).
RDE_INTERNAL b8 fzch_sep(c8 _c) {
    return _c == ' ' || _c == '\t' || _c == ',' || _c == ':' || _c == ';' || _c == '\r';
}

// True when the token from _a up to _b is a whole, finite decimal number (calc.h's fude_zoom_calc_decimal: no hex),
// with or without a "%" at its end; *_v gets it. The token is read in place, so a number of any length is one: what
// follows _b is a separator or the line's end, which no number runs into.
RDE_INTERNAL b8 fzch_number(const c8* _a, const c8* _b, f64* _v) {
    const c8* _end = _b > _a && _b[-1] == '%' ? _b - 1 : _b;
    f64       _x   = 0.0;
    const usize _len = _end > _a ? fude_zoom_calc_decimal(_a, true, &_x) : 0u;
    if(_len == 0u || _a + _len != _end || !rde_math_is_finite_f64(_x)) {
        return false;
    }
    *_v = _x;
    return true;
}

// Reads a chart's text into _d, one line at a time, separators trimmed from each end. A line starting "series" names
// the series. Any other line's values are the numbers at its end (up to 16 are read back from it, and the first
// FZCH_SERIES of those kept, in the line's order), and its name is what is before them. A line with no number at its
// end is the title when it comes before any category and there is no title yet; otherwise it is skipped. Categories
// past FZCH_CATS are dropped.
RDE_INTERNAL void fzch_read(const c8* _text, fzch_data* _d) {
    rde_memset(_d, 0, sizeof(*_d));
    const c8* _p = _text != NULL ? _text : "";
    while(*_p != 0) {
        const c8* _e = _p;
        while(*_e != 0 && *_e != '\n') {
            _e++;
        }
        const c8* _a = _p;
        const c8* _b = _e;
        _p = *_e != 0 ? _e + 1 : _e;
        while(_a < _b && fzch_sep(*_a)) {
            _a++;
        }
        while(_b > _a && fzch_sep(_b[-1])) {
            _b--;
        }
        if(_a == _b) {
            continue;
        }
        // "series: A, B": the series' names.
        if(_b - _a > 7 && rde_strncasecmp(_a, "series", 6u) == 0 && (_a[6] == ':' || _a[6] == ' ')) {
            const c8* _s = _a + 7;
            _d->named = 0;
            while(_s < _b && _d->named < FZCH_SERIES) {
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
                    fzch_name(_d->series[_d->named++], _s, _te);
                }
                _s = _t;
            }
            continue;
        }
        // Its values: the numbers at its end (as many as are, read back from it); its name what is before them.
        f64 _vals[FZCH_SERIES];
        u32 _nv = 0;
        const c8* _name_end = _b;
        f64 _back[16];
        u32 _nb = 0;
        while(_name_end > _a && _nb < 16u) {
            const c8* _t = _name_end;
            while(_t > _a && !fzch_sep(_t[-1])) {
                _t--;
            }
            f64 _x;
            if(!fzch_number(_t, _name_end, &_x)) {
                break;
            }
            _back[_nb++] = _x;
            _name_end = _t;
            while(_name_end > _a && fzch_sep(_name_end[-1])) {
                _name_end--;
            }
        }
        for(u32 _k = 0; _k < _nb && _nv < FZCH_SERIES; _k++) {
            _vals[_nv++] = _back[_nb - 1u - _k];   // (in the line's order)
        }
        if(_nv == 0u) {
            if(_d->ncat == 0u && _d->title[0] == 0) {
                fzch_name(_d->title, _a, _b);   // (a first line with no number: its title)
            }
            continue;
        }
        if(_d->ncat >= FZCH_CATS) {
            continue;
        }
        const u32 _c = _d->ncat++;
        fzch_name(_d->cat[_c], _a, _name_end);
        for(u32 _k = 0; _k < _nv; _k++) {
            _d->v[_c][_k] = _vals[_k];
        }
        _d->nv[_c] = _nv;
        _d->nser   = _nv > _d->nser ? _nv : _d->nser;
    }
}

// --- drawing ----------------------------------------------------------------------------------------

// Appends a part of _n points flagged _flags (chart.h says what the flags mean).
RDE_INTERNAL void fzch_part(rde_arr* _points, rde_arr* _parts, const fude_zoom_v2* _p, u32 _n, u8 _flags) {
    const fude_zoom_symbol_part _q = { (u32)rde_arr_length(_points), _n, _flags, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, 0.0f };
    for(u32 _i = 0; _i < _n; _i++) {
        rde_arr_add(_points, (any)&_p[_i]);
    }
    rde_arr_add(_parts, (any)&_q);
}

// Appends a part that is one straight line.
RDE_INTERNAL void fzch_line(rde_arr* _points, rde_arr* _parts, f64 _x0, f64 _y0, f64 _x1, f64 _y1, u8 _flags) {
    const fude_zoom_v2 _p[2] = { { _x0, _y0 }, { _x1, _y1 } };
    fzch_part(_points, _parts, _p, 2u, _flags);
}

// Appends a label at (_x, _y), placed by _align and coloured by _curve as plot.h's labels are; nothing when there is no
// list of labels or no text.
RDE_INTERNAL void fzch_label(rde_arr* _labels, f64 _x, f64 _y, const c8* _text, u8 _align, u8 _curve) {
    if(_labels == NULL || _text == NULL || _text[0] == 0) {
        return;
    }
    fude_zoom_plot_label _l;
    rde_memset(&_l, 0, sizeof(_l));
    _l.at = (fude_zoom_v2){ _x, _y };
    rde_snprintf(_l.text, sizeof(_l.text), "%s", _text);
    _l.align = _align;
    _l.curve = _curve;
    rde_arr_add(_labels, (any)&_l);
}

// A tick's step: 1, 2 or 5 of a power of ten, about _n of them over _range.
RDE_INTERNAL f64 fzch_step(f64 _range, f64 _n) {
    const f64 _raw = _range / _n;
    const f64 _p = rde_math_pow_f64(10.0, rde_math_floor_f64(rde_math_log10_f64(_raw)));
    const f64 _f = _raw / _p;
    return (_f <= 1.0 ? 1.0 : _f <= 2.0 ? 2.0 : _f <= 5.0 ? 5.0 : 10.0) * _p;
}

// Writes a tick's number as text, in at most 6 significant digits; a value within 1e-12 of 0 is written as 0, not as
// a tiny number left by adding the steps up.
RDE_INTERNAL void fzch_number_text(f64 _v, c8* _out, usize _size) {
    if(rde_math_abs_f64(_v) < 1e-12) {
        _v = 0.0;
    }
    rde_snprintf(_out, _size, "%.6g", _v);
}

// A dot: a small round region in series _k's colour.
RDE_INTERNAL void fzch_dot(rde_arr* _points, rde_arr* _parts, f64 _x, f64 _y, f64 _r, u32 _k) {
    fude_zoom_v2 _p[12];
    for(u32 _i = 0; _i < 12u; _i++) {
        const f64 _a = 2.0 * RDE_PI * (f64)_i / 12.0;
        _p[_i] = (fude_zoom_v2){ _x + rde_math_cos_f64(_a) * _r, _y + rde_math_sin_f64(_a) * _r };
    }
    fzch_part(_points, _parts, _p, 12u, (u8)(FUDE_ZOOM_CHART_SOLID + _k % FZCH_SERIES));
}

// See chart.h. The title, when there is one, takes the top 2.4 labels' heights. A pie chart's slices go clockwise from
// the top, each in its own colour, with its outline drawn again as a part of its own so the slices show apart. Bars
// and lines are drawn on axes from the lowest value or 0, whichever is lower, to the highest or 0, rounded out to
// whole ticks; the legend's lines take room over the plot, so nothing is drawn under them.
u32 fude_zoom_chart_lines(const c8* _id, const c8* _text, f64 _hw, f64 _hh, rde_arr* _points, rde_arr* _parts, rde_arr* _labels) {
    if(!fude_zoom_chart_is(_id) || !(_hw > 0.0) || !(_hh > 0.0)) {
        return 0u;
    }
    fzch_data* _d = (fzch_data*)rde_malloc(sizeof(fzch_data));
    if(_d == NULL) {
        return 0u;
    }
    fzch_read(_text, _d);
    const f64 _lh  = fude_zoom_plot_label_height(_id, _hw, _hh);   // (its labels' height)
    const f64 _top = _hh - (_d->title[0] != 0 ? _lh * 2.4 : _lh * 0.8);
    if(_d->title[0] != 0) {
        fzch_label(_labels, 0.0, _hh - _lh * 1.1, _d->title, 2u, 0u);
    }
    if(rde_strcmp(_id, "pie chart") == 0) {
        // Its slices: each category's first value's share, from the top round clockwise, each its own colour.
        f64 _sum = 0.0;
        for(u32 _c = 0; _c < _d->ncat; _c++) {
            _sum += _d->nv[_c] > 0u && _d->v[_c][0] > 0.0 ? _d->v[_c][0] : 0.0;
        }
        const f64 _r = rde_math_max_f64(rde_math_min_f64(_hw * 0.62, (_top + _hh) * 0.5 - _lh * 2.2), _lh);
        const fude_zoom_v2 _mid = { 0.0, (_top - _hh) * 0.5 };
        f64 _a0 = RDE_PI * 0.5;
        for(u32 _c = 0; _c < _d->ncat && _sum > 0.0; _c++) {
            const f64 _v = _d->nv[_c] > 0u && _d->v[_c][0] > 0.0 ? _d->v[_c][0] : 0.0;
            if(_v <= 0.0) {
                continue;
            }
            const f64 _sweep = 2.0 * RDE_PI * _v / _sum, _a1 = _a0 - _sweep;
            fude_zoom_v2 _p[200];
            u32 _n = 0;
            _p[_n++] = _mid;
            const u32 _steps = (u32)rde_math_min_f64(rde_math_max_f64(rde_math_ceil_f64(_sweep / (RDE_PI / 90.0)), 2.0), 180.0);
            for(u32 _i = 0; _i <= _steps; _i++) {
                const f64 _a = _a0 + (_a1 - _a0) * (f64)_i / (f64)_steps;
                _p[_n++] = (fude_zoom_v2){ _mid.x + rde_math_cos_f64(_a) * _r, _mid.y + rde_math_sin_f64(_a) * _r };
            }
            fzch_part(_points, _parts, _p, _n, (u8)(FUDE_ZOOM_CHART_SOLID + _c % FZCH_SERIES));
            _p[_n++] = _mid;   // (its edge closed: the slices apart)
            fzch_part(_points, _parts, _p, _n, 0u);
            // Its name and its share, out beyond its middle: from there outward (a slice on the right: from it on;
            // on the left: ending by it; at the top or the bottom: centred over or under it).
            const f64 _am = (_a0 + _a1) * 0.5, _ca = rde_math_cos_f64(_am), _sa = rde_math_sin_f64(_am);
            // (its share always whole: the name cut before a whole character to leave room for it)
            c8 _share[16];
            const usize _sl = (usize)rde_snprintf(_share, sizeof(_share), " %.0f%%", 100.0 * _v / _sum);
            c8 _say[FZCH_NAME];
            fude_utf8_copy(_say, sizeof(_say) - _sl, _d->cat[_c], rde_strlen(_d->cat[_c]));
            rde_snprintf(_say + rde_strlen(_say), _sl + 1u, "%s", _share);
            const u8 _side = _ca > 0.3 ? 3u : _ca < -0.3 ? 1u : 2u;
            fzch_label(_labels, _mid.x + _ca * (_r + _lh * 0.5), _mid.y + _sa * (_r + _lh * (_side == 2u ? 0.9 : 0.4)), _say, _side, 0u);
            _a0 = _a1;
        }
        const u32 _drawn = _d->ncat > 0u ? 1u : 0u;
        rde_free(_d);
        return _drawn;
    }
    // Bars and lines: on axes, the values up, the categories along.
    const f64 _x0 = -_hw + _lh * 4.6, _x1 = _hw - _lh * 0.8, _y0 = -_hh + _lh * 2.2;
    const f64 _y1 = _top - (f64)_d->named * _lh * 1.3;   // (its legend's lines over the values: nothing under them)
    f64 _vmin = 0.0, _vmax = 0.0;
    for(u32 _c = 0; _c < _d->ncat; _c++) {
        for(u32 _k = 0; _k < _d->nv[_c]; _k++) {
            _vmin = rde_math_min_f64(_vmin, _d->v[_c][_k]);
            _vmax = rde_math_max_f64(_vmax, _d->v[_c][_k]);
        }
    }
    if(!(_vmax > _vmin)) {
        _vmax = _vmin + 1.0;
    }
    const f64 _step = fzch_step(_vmax - _vmin, 5.0);
    const f64 _lo = rde_math_floor_f64(_vmin / _step + 1e-9) * _step, _hi = rde_math_ceil_f64(_vmax / _step - 1e-9) * _step;   // (0 itself where all are above it)
    #define FZCH_Y(_v) (_y0 + ((_v) - _lo) / (_hi - _lo) * (_y1 - _y0))
    // Its grid and its numbers, its axes.
    for(f64 _t = _lo; _t <= _hi + _step * 0.5 && _y1 > _y0; _t += _step) {
        const f64 _y = FZCH_Y(_t);
        fzch_line(_points, _parts, _x0, _y, _x1, _y, FUDE_ZOOM_SYMBOL_DASHED);
        c8 _num[24];
        fzch_number_text(_t, _num, sizeof(_num));
        fzch_label(_labels, _x0 - _lh * 0.4, _y, _num, 1u, 0u);
    }
    fzch_line(_points, _parts, _x0, _y0, _x0, _y1, 0u);
    fzch_line(_points, _parts, _x0, FZCH_Y(0.0), _x1, FZCH_Y(0.0), 0u);
    const f64 _slot = _d->ncat > 0u ? (_x1 - _x0) / (f64)_d->ncat : 0.0;
    const u32 _ns = _d->nser > 0u ? _d->nser : 1u;
    const b8 _bars = rde_strcmp(_id, "bar chart") == 0;
    for(u32 _c = 0; _c < _d->ncat; _c++) {
        const f64 _left = _x0 + _slot * (f64)_c, _mid = _left + _slot * 0.5;
        fzch_label(_labels, _mid, _y0 - _lh * 0.4, _d->cat[_c], 0u, 0u);
        for(u32 _k = 0; _bars && _k < _d->nv[_c]; _k++) {
            const f64 _bw = _slot * 0.7 / (f64)_ns, _bx = _left + _slot * 0.15 + _bw * (f64)_k;
            const f64 _ya = FZCH_Y(0.0), _yb = FZCH_Y(_d->v[_c][_k]);
            const fude_zoom_v2 _p[4] = { { _bx, _ya }, { _bx + _bw, _ya }, { _bx + _bw, _yb }, { _bx, _yb } };
            fzch_part(_points, _parts, _p, 4u, (u8)(FUDE_ZOOM_CHART_SOLID + _k % FZCH_SERIES));
        }
    }
    if(!_bars) {
        // A series a line through its values at its categories' middles, a dot at each.
        for(u32 _k = 0; _k < _ns; _k++) {
            fude_zoom_v2 _p[FZCH_CATS];
            u32 _n = 0;
            for(u32 _c = 0; _c < _d->ncat; _c++) {
                if(_k < _d->nv[_c]) {
                    _p[_n++] = (fude_zoom_v2){ _x0 + _slot * ((f64)_c + 0.5), FZCH_Y(_d->v[_c][_k]) };
                }
            }
            if(_n >= 2u) {
                fzch_part(_points, _parts, _p, _n, (u8)(32u + _k % FZCH_SERIES));
            }
            for(u32 _i = 0; _i < _n; _i++) {
                fzch_dot(_points, _parts, _p[_i].x, _p[_i].y, _lh * 0.28, _k);
            }
        }
    }
    #undef FZCH_Y
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
        fzch_label(_labels, rde_math_max_f64(_x1 - _lh * (0.6 * (f64)_longest + 0.8), _x0 + _lh * 0.6), _top - _lh * 0.2, _d->series[_k], 4u, (u8)(1u + _k % FZCH_SERIES));
    }
    const u32 _series = _d->ncat > 0u ? _ns : 0u;
    rde_free(_d);
    return _series;
}
