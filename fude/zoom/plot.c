// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/plot.h"
#include "zoom/calc.h"
#include "zoom/symbol.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FZL_FUNCTIONS 6u
#define FZL_SAMPLES   480u
#define FZL_POINTS    8u
#define FZL_LEG       3.0    // seconds a variable takes from one end of its range to the other, playing
#define FZL_PI        3.14159265358979323846

// What a graph's text says: its functions, its view, its variables (a letter's value, or its range), the x its
// points are marked at, the points it puts.
typedef struct {
    fude_zoom_calc     f[FZL_FUNCTIONS];
    c8                 names[FZL_FUNCTIONS][48];   // (each as written: its legend's line)
    u32                nf;
    f64                x0, x1, y0, y1;
    b8                 y_set;
    fude_zoom_plot_var vars[26];                   // by letter (x: where its points are marked)
    u32                set;                        // the letters set (a bit each)
    fude_zoom_v2       points[FZL_POINTS];
    u32                np;
} fzl_read;

// "2", "1..5", "1 .. 5", "-3 to 3": its number, or its two (a range). False: not that.
RDE_INTERNAL b8 fzl_numbers(const c8* _s, f64* _a, f64* _b, b8* _range) {
    c8 _t[64];
    snprintf(_t, sizeof(_t), "%s", _s);
    c8* _second = NULL;
    c8* _dots = strstr(_t, "..");
    c8* _to   = strstr(_t, " to ");
    if(_dots != NULL) {
        *_dots  = 0;
        _second = _dots + 2;
    } else if(_to != NULL) {
        *_to    = 0;
        _second = _to + 4;
    }
    c8* _e = NULL;
    *_a = strtod(_t, &_e);
    if(_e == _t) {
        return false;
    }
    while(*_e == ' ' || *_e == '\t' || *_e == '\r') {
        _e++;
    }
    if(*_e != 0) {
        return false;
    }
    *_range = false;
    *_b = *_a;
    if(_second != NULL) {
        *_b = strtod(_second, &_e);
        if(_e == _second) {
            return false;
        }
        while(*_e == ' ' || *_e == '\t' || *_e == '\r') {
            _e++;
        }
        if(*_e != 0) {
            return false;
        }
        *_range = *_b != *_a;
    }
    return true;
}

RDE_INTERNAL void fzl_parse(const c8* _id, const c8* _text, fzl_read* _r) {
    memset(_r, 0, sizeof(*_r));
    const b8 _line_only = strcmp(_id, "number line") == 0;
    const b8 _functions = strcmp(_id, "graph") == 0;
    _r->x0 = -10.0;
    _r->x1 = 10.0;
    for(const c8* _l = _text; _l != NULL && *_l != 0;) {
        const c8* _end = strchr(_l, '\n');
        const usize _len = _end != NULL ? (usize)(_end - _l) : strlen(_l);
        c8 _line[200];
        snprintf(_line, sizeof(_line), "%.*s", (int)(_len < sizeof(_line) - 1u ? _len : sizeof(_line) - 1u), _l);
        _l = _end != NULL ? _end + 1 : NULL;
        f64 _a, _b;
        c8 _axis = 0;
        b8 _range = false;
        const c8* _w = _line;
        while(*_w == ' ' || *_w == '\t') {
            _w++;
        }
        // A letter = a number (or a range): a variable's (x: where its points are marked; y = … is a function's).
        const c8* _eq = _w + 1;
        while(*_eq == ' ' || *_eq == '\t') {
            _eq++;
        }
        const c8 _letter = (c8)(*_w >= 'A' && *_w <= 'Z' ? *_w + ('a' - 'A') : *_w);
        if(_functions && _letter >= 'a' && _letter <= 'z' && _letter != 'y' && _letter != 'e' && *_eq == '=' && fzl_numbers(_eq + 1, &_a, &_b, &_range)) {
            fude_zoom_plot_var* _v = &_r->vars[_letter - 'a'];
            _v->letter = _letter;
            _v->from   = _a;
            _v->to     = _b;
            _v->range  = _range;
            _r->set   |= 1u << (u32)(_letter - 'a');
            continue;
        }
        if((sscanf(_line, " %c %lf %lf", &_axis, &_a, &_b) == 3 || sscanf(_line, " %c: %lf .. %lf", &_axis, &_a, &_b) == 3) &&
           (_axis == 'x' || _axis == 'X' || _axis == 'y' || _axis == 'Y') && _b != _a) {
            if(_axis == 'x' || _axis == 'X') {
                _r->x0 = fmin(_a, _b);
                _r->x1 = fmax(_a, _b);
            } else {
                _r->y0    = fmin(_a, _b);
                _r->y1    = fmax(_a, _b);
                _r->y_set = true;
            }
        } else if(_line_only && sscanf(_line, " %lf %lf", &_a, &_b) == 2 && _b != _a) {
            _r->x0 = fmin(_a, _b);
            _r->x1 = fmax(_a, _b);
        } else if(_functions && _r->np < FZL_POINTS && sscanf(_line, " ( %lf , %lf )", &_a, &_b) == 2) {
            _r->points[_r->np++] = (fude_zoom_v2){ _a, _b };
        } else if(_functions && _r->nf < FZL_FUNCTIONS && fude_zoom_calc_parse(&_r->f[_r->nf], _line)) {
            snprintf(_r->names[_r->nf], sizeof(_r->names[_r->nf]), "%s%s", strchr(_w, '=') != NULL ? "" : "y = ", _w);
            _r->nf++;
        }
    }
}

// Variable _v's value now: where a slider holds it, else as its line says (a range: there and back, playing; its
// first number, not).
RDE_INTERNAL f64 fzl_value(const fude_zoom_plot_var* _v, const fude_zoom_plot_play* _play) {
    const u32 _k = (u32)(_v->letter - 'a') % 26u;
    if(_play != NULL && !isnan(_play->held[_k])) {
        return _play->held[_k];
    }
    if(!_v->range || _play == NULL || isnan(_play->time)) {
        return _v->from;
    }
    return _v->from + (_v->to - _v->from) * (0.5 - 0.5 * cos(FZL_PI * _play->time / FZL_LEG));
}

u32 fude_zoom_plot_vars(const c8* _id, const c8* _text, fude_zoom_plot_var* _out, u32 _max) {
    if(_id == NULL || strcmp(_id, "graph") != 0) {
        return 0;
    }
    fzl_read* _r = (fzl_read*)malloc(sizeof(fzl_read));
    if(_r == NULL) {
        return 0;
    }
    fzl_parse(_id, _text, _r);
    u32 _n = 0;
    for(u32 _k = 0; _k < 26u && _n < _max; _k++) {
        if(_r->set & (1u << _k)) {
            _out[_n++] = _r->vars[_k];
        }
    }
    free(_r);
    return _n;
}

f64 fude_zoom_plot_var_value(const fude_zoom_plot_var* _v, const fude_zoom_plot_play* _play) {
    return fzl_value(_v, _play);
}

// A number short (its legend's, a point's): 3 figures, no trailing noughts.
RDE_INTERNAL void fzl_short(f64 _v, c8* _out, usize _size) {
    if(fabs(_v) < 1e-12) {
        _v = 0.0;
    }
    snprintf(_out, _size, "%.3g", _v);
}

b8 fude_zoom_plot_is(const c8* _id) {
    return _id != NULL && (strcmp(_id, "graph") == 0 || strcmp(_id, "axes") == 0 || strcmp(_id, "number line") == 0);
}

f64 fude_zoom_plot_label_height(const c8* _id, f64 _hw, f64 _hh) {
    return _id != NULL && strcmp(_id, "number line") == 0 ? fmin(_hh * 0.5, _hw * 0.08) : fmin(_hw, _hh) * 0.1;
}

RDE_INTERNAL void fzl_line(rde_arr* _points, rde_arr* _parts, fude_zoom_v2 _a, fude_zoom_v2 _b, u8 _flags) {
    const fude_zoom_symbol_part _p = { (u32)rde_arr_length(_points), 2u, _flags };
    rde_arr_add(_points, (any)&_a);
    rde_arr_add(_points, (any)&_b);
    rde_arr_add(_parts, (any)&_p);
}

// A dot (a closed ring, its first point again last): a curve's (flags 32 + its), or the ink's (SOLID).
RDE_INTERNAL void fzl_dot(rde_arr* _points, rde_arr* _parts, fude_zoom_v2 _c, f64 _r, u8 _flags) {
    const fude_zoom_symbol_part _p = { (u32)rde_arr_length(_points), 13u, _flags };
    for(u32 _i = 0; _i <= 12u; _i++) {
        const f64 _a = 2.0 * FZL_PI * (f64)(_i % 12u) / 12.0;
        const fude_zoom_v2 _q = { _c.x + cos(_a) * _r, _c.y + sin(_a) * _r };
        rde_arr_add(_points, (any)&_q);
    }
    rde_arr_add(_parts, (any)&_p);
}

// A tick's step for a span: 1, 2 or 5 of a power of ten, about _want of them across it.
RDE_INTERNAL f64 fzl_step(f64 _span, f64 _want) {
    const f64 _raw = _span / fmax(_want, 1.0);
    const f64 _p = pow(10.0, floor(log10(fmax(_raw, 1e-300))));
    const f64 _r = _raw / _p;
    return (_r <= 1.0 ? 1.0 : (_r <= 2.0 ? 2.0 : (_r <= 5.0 ? 5.0 : 10.0))) * _p;
}

RDE_INTERNAL void fzl_number(f64 _v, f64 _step, c8* _out, usize _size) {
    if(fabs(_v) < _step * 1e-6) {
        _v = 0.0;
    }
    const i32 _dec = _step >= 1.0 ? 0 : (i32)ceil(-log10(_step) - 1e-9);
    snprintf(_out, _size, "%.*f", _dec < 6 ? _dec : 6, _v);
}

RDE_INTERNAL int fzl_by_f64(const void* _a, const void* _b) {
    const f64 _x = *(const f64*)_a, _y = *(const f64*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

u32 fude_zoom_plot_lines(const c8* _id, const c8* _text, f64 _hw, f64 _hh, rde_arr* _points, rde_arr* _parts, rde_arr* _labels) {
    return fude_zoom_plot_lines_play(_id, _text, _hw, _hh, NULL, _points, _parts, _labels);
}

u32 fude_zoom_plot_lines_play(const c8* _id, const c8* _text, f64 _hw, f64 _hh, const fude_zoom_plot_play* _play, rde_arr* _points, rde_arr* _parts,
                              rde_arr* _labels) {
    if(!fude_zoom_plot_is(_id)) {
        return 0;
    }
    const b8 _line_only = strcmp(_id, "number line") == 0;
    // Its text: a function a line, a view's ("x -5 5", "y -2 2"; a number line's: its two numbers), a variable's
    // ("a = 2", "a = 0..5"), where points are marked ("x = 1.5"), a point ("(2, 3)").
    fzl_read* _r = (fzl_read*)malloc(sizeof(fzl_read));
    if(_r == NULL) {
        return 0;
    }
    fzl_parse(_id, _text, _r);
    fude_zoom_calc* _f = _r->f;
    const u32 _nf = _r->nf;
    f64 _x0 = _r->x0, _x1 = _r->x1, _y0 = _r->y0, _y1 = _r->y1;
    const b8 _y_set = _r->y_set;
    // Each variable's value now (NaN: not set).
    f64 _vars[26];
    for(u32 _k = 0; _k < 26u; _k++) {
        _vars[_k] = (_r->set & (1u << _k)) ? fzl_value(&_r->vars[_k], _play) : NAN;
    }
    // Its y as its functions go (the middle 96 % of what they reach: an asymptote's spike left out), else as x is.
    if(!_y_set) {
        // (a variable over a range: as it is anywhere in it, so the axes stay as it moves)
        const u32 _passes = 5u;
        const u32 _most = FZL_SAMPLES * (_nf > 0u ? _nf : 1u) * _passes;
        rde_arr TYPE(f64) _v_arr = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std());
        f64* _v = (f64*)rde_arr_add_n(&_v_arr, _most);   // (sized once: it stays put)
        u32 _nv = 0;
        for(u32 _pass = 0; _pass < _passes; _pass++) {
            f64 _at[26];
            for(u32 _k = 0; _k < 26u; _k++) {
                const fude_zoom_plot_var* _q = &_r->vars[_k];
                const b8 _held = _play != NULL && !isnan(_play->held[_k]);
                _at[_k] = (_r->set & (1u << _k)) && _q->range && !_held ? _q->from + (_q->to - _q->from) * (f64)_pass / (f64)(_passes - 1u) : _vars[_k];
            }
            for(u32 _k = 0; _k < _nf; _k++) {
                for(u32 _i = 0; _i < FZL_SAMPLES; _i += (_pass == 0u ? 1u : 4u)) {
                    const f64 _y = fude_zoom_calc_at_with(&_f[_k], _x0 + (_x1 - _x0) * (f64)_i / (f64)(FZL_SAMPLES - 1u), _at);
                    if(isfinite(_y) && fabs(_y) < 1e9) {
                        _v[_nv++] = _y;
                    }
                }
            }
        }
        if(_nv >= 2u) {
            qsort(_v, _nv, sizeof(f64), fzl_by_f64);
            _y0 = _v[(u32)((f64)_nv * 0.02)];
            _y1 = _v[(u32)((f64)(_nv - 1u) * 0.98)];
            if(_y1 - _y0 < 1e-9) {
                _y0 -= 1.0;
                _y1 += 1.0;
            }
            const f64 _pad = (_y1 - _y0) * 0.08;
            _y0 -= _pad;
            _y1 += _pad;
            // (the x axis kept in sight when it is near)
            if(_y0 > 0.0 && _y0 < (_y1 - _y0) * 0.35) { _y0 = -(_y1 - _y0) * 0.05; }
            if(_y1 < 0.0 && -_y1 < (_y1 - _y0) * 0.35) { _y1 = (_y1 - _y0) * 0.05; }
        } else {
            const f64 _r = (_x1 - _x0) * 0.5 * (_hh / fmax(_hw, 1e-300));
            _y0 = -_r;
            _y1 = _r;
        }
        rde_arr_free(&_v_arr);
    }
    // The view in its box (a little in from its edges).
    const f64 _bx = _hw * 0.92, _by = _hh * 0.88;
    #define FZL_X(_v) (-_bx + ((_v) - _x0) / (_x1 - _x0) * 2.0 * _bx)
    #define FZL_Y(_v) (-_by + ((_v) - _y0) / (_y1 - _y0) * 2.0 * _by)
    const f64 _sx = fzl_step(_x1 - _x0, _line_only ? 10.0 : _hw / fmax(_hh, 1e-300) * 7.0), _sy = fzl_step(_y1 - _y0, 7.0);
    const f64 _ax = _line_only ? 0.0 : (_y0 <= 0.0 && _y1 >= 0.0 ? FZL_Y(0.0) : -_by);   // (where the x axis is)
    const f64 _ay = _x0 <= 0.0 && _x1 >= 0.0 ? FZL_X(0.0) : -_bx;                       // (...the y axis)
    const f64 _tick = fmin(_hw, _hh) * (_line_only ? 0.25 : 0.03);
    // Grid and ticks.
    for(f64 _v = ceil(_x0 / _sx) * _sx; _v <= _x1 + _sx * 1e-9; _v += _sx) {
        const f64 _x = FZL_X(_v);
        if(!_line_only) {
            fzl_line(_points, _parts, (fude_zoom_v2){ _x, -_by }, (fude_zoom_v2){ _x, _by }, FUDE_ZOOM_SYMBOL_DASHED);
        }
        fzl_line(_points, _parts, (fude_zoom_v2){ _x, _ax - _tick }, (fude_zoom_v2){ _x, _ax + _tick }, 0u);
        if(_labels != NULL && (_line_only || fabs(_v) > _sx * 1e-6)) {   // (a graph's 0: where the axes cross, below)
            fude_zoom_plot_label _lb = { { _x, _ax - _tick * (_line_only ? 1.6 : 2.5) }, { 0 }, 0u, 0u };
            fzl_number(_v, _sx, _lb.text, sizeof(_lb.text));
            rde_arr_add(_labels, (any)&_lb);
        }
    }
    if(!_line_only) {
        for(f64 _v = ceil(_y0 / _sy) * _sy; _v <= _y1 + _sy * 1e-9; _v += _sy) {
            const f64 _y = FZL_Y(_v);
            fzl_line(_points, _parts, (fude_zoom_v2){ -_bx, _y }, (fude_zoom_v2){ _bx, _y }, FUDE_ZOOM_SYMBOL_DASHED);
            fzl_line(_points, _parts, (fude_zoom_v2){ _ay - _tick, _y }, (fude_zoom_v2){ _ay + _tick, _y }, 0u);
            if(_labels != NULL && fabs(_v) > _sy * 1e-6) {
                fude_zoom_plot_label _lb = { { _ay - _tick * 2.0, _y }, { 0 }, 1u, 0u };
                fzl_number(_v, _sy, _lb.text, sizeof(_lb.text));
                rde_arr_add(_labels, (any)&_lb);
            }
        }
        fzl_line(_points, _parts, (fude_zoom_v2){ _ay, -_by }, (fude_zoom_v2){ _ay, _by }, 0u);
    }
    fzl_line(_points, _parts, (fude_zoom_v2){ -_bx, _ax }, (fude_zoom_v2){ _bx, _ax }, 0u);
    if(_labels != NULL && !_line_only && _x0 <= 0.0 && _x1 >= 0.0) {   // (where the axes cross)
        const fude_zoom_plot_label _o = { { _ay - _tick * 2.0, _ax - _tick * 2.5 }, "0", 1u, 0u };
        rde_arr_add(_labels, (any)&_o);
    }
    // Each function's curve (its own colour: its part's flags 32 + which): broken where it is not, or leaves the view.
    for(u32 _k = 0; _k < _nf; _k++) {
        u32 _first = (u32)rde_arr_length(_points);
        f64 _py = NAN;
        for(u32 _i = 0; _i <= FZL_SAMPLES; _i++) {
            const f64 _xv = _x0 + (_x1 - _x0) * (f64)_i / (f64)FZL_SAMPLES;
            const f64 _yv = fude_zoom_calc_at_with(&_f[_k], _xv, _vars);
            const b8 _in = isfinite(_yv) && _yv >= _y0 - (_y1 - _y0) * 0.02 && _yv <= _y1 + (_y1 - _y0) * 0.02;
            const b8 _jump = isfinite(_py) && isfinite(_yv) && fabs(_yv - _py) > (_y1 - _y0) * 0.9;
            if(!_in || _jump) {
                // (the piece so far ended — at the view's edge, if it went past it)
                if(_in == false && isfinite(_yv) && isfinite(_py) && !_jump && (u32)rde_arr_length(_points) > _first) {
                    const f64 _edge = _yv > _y1 ? _y1 : _y0;
                    const f64 _t = (_edge - _py) / (_yv - _py);
                    const fude_zoom_v2 _q = { FZL_X(_xv - (_x1 - _x0) / (f64)FZL_SAMPLES * (1.0 - _t)), FZL_Y(_edge) };
                    rde_arr_add(_points, (any)&_q);
                }
                const u32 _n = (u32)rde_arr_length(_points) - _first;
                if(_n >= 2u) {
                    const fude_zoom_symbol_part _p = { _first, _n, (u8)(32u + _k) };
                    rde_arr_add(_parts, (any)&_p);
                } else {
                    _points->count = _first;
                }
                _first = (u32)rde_arr_length(_points);
                if(!_jump && isfinite(_yv) && isfinite(_py) && _in == false) {
                    _py = _yv;
                    continue;
                }
            }
            if(_in) {
                if((u32)rde_arr_length(_points) == _first && isfinite(_py) && !_jump && (_py > _y1 || _py < _y0)) {
                    // (coming back into the view: from its edge)
                    const f64 _edge = _py > _y1 ? _y1 : _y0;
                    const f64 _t = (_edge - _py) / (_yv - _py);
                    const fude_zoom_v2 _q = { FZL_X(_xv - (_x1 - _x0) / (f64)FZL_SAMPLES * (1.0 - _t)), FZL_Y(_edge) };
                    rde_arr_add(_points, (any)&_q);
                }
                const fude_zoom_v2 _q = { FZL_X(_xv), FZL_Y(fmin(fmax(_yv, _y0), _y1)) };
                rde_arr_add(_points, (any)&_q);
            }
            _py = _yv;
        }
        const u32 _n = (u32)rde_arr_length(_points) - _first;
        if(_n >= 2u) {
            const fude_zoom_symbol_part _p = { _first, _n, (u8)(32u + _k) };
            rde_arr_add(_parts, (any)&_p);
        } else {
            _points->count = _first;
        }
    }
    const f64 _lh = fude_zoom_plot_label_height(_id, _hw, _hh);
    // Where its points are marked (x = …): a line up through it, a dot on each curve there, its numbers by it.
    const f64 _xm = _vars['x' - 'a'];
    if(isfinite(_xm) && _xm >= _x0 && _xm <= _x1) {
        fzl_line(_points, _parts, (fude_zoom_v2){ FZL_X(_xm), -_by }, (fude_zoom_v2){ FZL_X(_xm), _by }, FUDE_ZOOM_SYMBOL_DASHED);
        for(u32 _k = 0; _k < _nf; _k++) {
            const f64 _ym = fude_zoom_calc_at_with(&_f[_k], _xm, _vars);
            if(!isfinite(_ym) || _ym < _y0 || _ym > _y1) {
                continue;
            }
            const fude_zoom_v2 _c = { FZL_X(_xm), FZL_Y(_ym) };
            fzl_dot(_points, _parts, _c, _lh * 0.32, (u8)(32u + _k));
            if(_labels != NULL) {
                fude_zoom_plot_label _lb = { { _c.x + _lh * 0.5, _c.y + _lh * 0.6 }, { 0 }, 3u, (u8)(1u + _k) };
                c8 _a[24], _b[24];
                fzl_short(_xm, _a, sizeof(_a));
                fzl_short(_ym, _b, sizeof(_b));
                snprintf(_lb.text, sizeof(_lb.text), "(%s, %s)", _a, _b);
                rde_arr_add(_labels, (any)&_lb);
            }
        }
    }
    // Its points: a dot each, its numbers by it.
    for(u32 _i = 0; _i < _r->np; _i++) {
        const fude_zoom_v2 _q = _r->points[_i];
        if(_q.x < _x0 || _q.x > _x1 || _q.y < _y0 || _q.y > _y1) {
            continue;
        }
        const fude_zoom_v2 _c = { FZL_X(_q.x), FZL_Y(_q.y) };
        fzl_dot(_points, _parts, _c, _lh * 0.32, FUDE_ZOOM_SYMBOL_SOLID);
        if(_labels != NULL) {
            fude_zoom_plot_label _lb = { { _c.x + _lh * 0.5, _c.y + _lh * 0.6 }, { 0 }, 3u, 0u };
            c8 _a[24], _b[24];
            fzl_short(_q.x, _a, sizeof(_a));
            fzl_short(_q.y, _b, sizeof(_b));
            snprintf(_lb.text, sizeof(_lb.text), "(%s, %s)", _a, _b);
            rde_arr_add(_labels, (any)&_lb);
        }
    }
    // The legend: each function as written, top left, a line each; then each variable's value now.
    u32 _row = 0;
    for(u32 _k = 0; _k < _nf && _labels != NULL; _k++, _row++) {
        fude_zoom_plot_label _lb = { { -_bx + _lh * 0.6, _by - _lh * (0.9 + 1.3 * (f64)_row) }, { 0 }, 4u, (u8)(1u + _k) };
        snprintf(_lb.text, sizeof(_lb.text), "%s", _r->names[_k]);
        rde_arr_add(_labels, (any)&_lb);
    }
    for(u32 _k = 0; _k < 26u && _labels != NULL; _k++) {
        if(!(_r->set & (1u << _k)) || _k == (u32)('x' - 'a')) {
            continue;
        }
        fude_zoom_plot_label _lb = { { -_bx + _lh * 0.6, _by - _lh * (0.9 + 1.3 * (f64)_row) }, { 0 }, 4u, 0u };
        c8 _a[24];
        fzl_short(_vars[_k], _a, sizeof(_a));
        snprintf(_lb.text, sizeof(_lb.text), "%c = %s", (c8)('a' + _k), _a);
        rde_arr_add(_labels, (any)&_lb);
        _row++;
    }
    free(_r);
    #undef FZL_X
    #undef FZL_Y
    return _nf;
}
