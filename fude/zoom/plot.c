// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/plot.h"
#include "zoom/chart.h"
#include "zoom/calc.h"
#include "zoom/symbol.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FZL_FUNCTIONS 6u
#define FZL_SAMPLES   400u     // a curve's first samples across its run (then finer where it bends or leaps)
#define FZL_DEPTH     10u      // ...halved at most so many times
#define FZL_BUDGET    24000u   // ...samples at most (a curve that wiggles faster than that: as far as they go)
#define FZL_GRID      144u     // an equation's grid: so many cells across the view's longer side
#define FZL_MARKS     12u      // marked roots, turning points, crossings: so many of each a curve at most
#define FZL_POINTS    8u
#define FZL_LEG       3.0      // seconds a variable takes from one end of its range to the other, playing
#define FZL_PI        3.14159265358979323846
#define FZL_NONE      0xFFFFFFFFu

// A curve's kind: y = f(x); (f(t), g(t)); r = f(θ); an equation or inequality of x and y, f − g against 0.
typedef enum { FZL_EXPLICIT = 0, FZL_PARAMETRIC, FZL_POLAR, FZL_IMPLICIT } FZL_KIND_;
typedef enum { FZL_EQ = 0, FZL_LT, FZL_LE, FZL_GT, FZL_GE } FZL_REL_;
// What a graph's text asks to be marked.
typedef enum { FZL_ROOTS = 1, FZL_EXTREMA = 2, FZL_CROSSINGS = 4 } FZL_MARK_;

typedef struct {
    fude_zoom_calc f, g;
    u8             kind, rel;
} fzl_curve;

// What a graph's text says: its curves, its view, its variables (a letter's value, or its range), the x its points are
// marked at, the points it puts, what it marks, the area it shades.
typedef struct {
    fzl_curve          c[FZL_FUNCTIONS];
    c8                 names[FZL_FUNCTIONS][48];   // (each as written: its legend's line)
    u32                nf;
    f64                x0, x1, y0, y1, t0, t1;
    b8                 x_set, y_set, t_set;
    fude_zoom_plot_var vars[26];                   // by letter (x: where its points are marked)
    u32                set;                        // the letters set (a bit each)
    fude_zoom_v2       points[FZL_POINTS];
    u32                np;
    u8                 marks;                      // FZL_MARK_
    f64                area0, area1;               // under its first function, shaded (area_set)
    b8                 area_set;
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

// _s trimmed into _out (_size bytes).
RDE_INTERNAL void fzl_trim(const c8* _s, usize _n, c8* _out, usize _size) {
    while(_n > 0 && (*_s == ' ' || *_s == '\t')) {
        _s++;
        _n--;
    }
    while(_n > 0 && (_s[_n - 1u] == ' ' || _s[_n - 1u] == '\t' || _s[_n - 1u] == '\r')) {
        _n--;
    }
    snprintf(_out, _size, "%.*s", (int)(_n < _size - 1u ? _n : _size - 1u), _s);
}

// A view's line: "x -5 5", "x: -5 .. 5", "y 0 2pi", "t 0 2pi" (θ too) — its letter and its two numbers (any number as
// maths writes it: calc.h). False: not one.
RDE_INTERNAL b8 fzl_view(const c8* _line, c8* _axis, f64* _a, f64* _b) {
    const c8* _p = _line;
    while(*_p == ' ' || *_p == '\t') {
        _p++;
    }
    if(strncmp(_p, "\xCE\xB8", 2) == 0) {   // θ
        *_axis = 't';
        _p += 2;
    } else {
        const c8 _c = (c8)(*_p >= 'A' && *_p <= 'Z' ? *_p + ('a' - 'A') : *_p);
        if(_c != 'x' && _c != 'y' && _c != 't') {
            return false;
        }
        *_axis = _c;
        _p++;
    }
    if(*_p != ' ' && *_p != '\t' && *_p != ':') {
        return false;   // (x^2…, xy: a function's)
    }
    while(*_p == ' ' || *_p == '\t' || *_p == ':') {
        _p++;
    }
    c8 _one[64], _two[64];
    const c8* _dots = strstr(_p, "..");
    if(_dots != NULL) {
        fzl_trim(_p, (usize)(_dots - _p), _one, sizeof(_one));
        fzl_trim(_dots + 2, strlen(_dots + 2), _two, sizeof(_two));
    } else {
        // (two numbers apart: split where a space is between them — not "- 2", a sign's)
        const c8* _gap = NULL;
        for(const c8* _q = _p; *_q != 0; _q++) {
            if((*_q == ' ' || *_q == '\t') && _q > _p && _q[-1] != '-' && _q[-1] != '+') {
                const c8* _n = _q;
                while(*_n == ' ' || *_n == '\t') {
                    _n++;
                }
                if(*_n != 0 && *_n != '+' && *_n != '*' && *_n != '/' && *_n != '^') {
                    _gap = _q;
                    break;
                }
            }
        }
        if(_gap == NULL) {
            return false;
        }
        fzl_trim(_p, (usize)(_gap - _p), _one, sizeof(_one));
        fzl_trim(_gap, strlen(_gap), _two, sizeof(_two));
    }
    return fude_zoom_calc_number(_one, _a) && fude_zoom_calc_number(_two, _b) && *_a != *_b;
}

// Where _s's first relation (= < > ≤ ≥ <= >=) is, outside brackets: its offset, its length, its kind. False: none.
RDE_INTERNAL b8 fzl_relation(const c8* _s, usize* _at, usize* _len, u8* _rel) {
    i32 _depth = 0;
    for(usize _i = 0; _s[_i] != 0; _i++) {
        const c8 _c = _s[_i];
        if(_c == '(') {
            _depth++;
        } else if(_c == ')') {
            _depth--;
        }
        if(_depth != 0) {
            continue;
        }
        if(strncmp(&_s[_i], "<=", 2) == 0 || strncmp(&_s[_i], ">=", 2) == 0) {
            *_at = _i; *_len = 2u; *_rel = _c == '<' ? FZL_LE : FZL_GE;
            return true;
        }
        if(strncmp(&_s[_i], "\xE2\x89\xA4", 3) == 0 || strncmp(&_s[_i], "\xE2\x89\xA5", 3) == 0) {   // ≤ ≥
            *_at = _i; *_len = 3u; *_rel = (u8)_s[_i + 2u] == 0xA4u ? FZL_LE : FZL_GE;
            return true;
        }
        if(_c == '<' || _c == '>' || _c == '=') {
            *_at = _i; *_len = 1u; *_rel = _c == '<' ? FZL_LT : (_c == '>' ? FZL_GT : FZL_EQ);
            return true;
        }
    }
    return false;
}

// Is _s (trimmed) "y", or a function's name of x ("f(x)", "g(x)")? ...or r's ("r", "r(θ)", "r(t)")?
RDE_INTERNAL b8 fzl_is_y(const c8* _s) {
    if((_s[0] == 'y' || _s[0] == 'Y') && _s[1] == 0) {
        return true;
    }
    // (a letter, then "(x)", spaces anywhere between)
    if(!((_s[0] >= 'a' && _s[0] <= 'z') || (_s[0] >= 'A' && _s[0] <= 'Z'))) {
        return false;
    }
    const c8* _want = "(x)";
    const c8* _p = _s + 1;
    for(; *_want != 0; _want++) {
        while(*_p == ' ') {
            _p++;
        }
        if((*_p | 0x20) != (*_want | 0x20)) {
            return false;
        }
        _p++;
    }
    while(*_p == ' ') {
        _p++;
    }
    return *_p == 0;
}

RDE_INTERNAL b8 fzl_is_r(const c8* _s) {
    return (_s[0] == 'r' || _s[0] == 'R') && (_s[1] == 0 || strcmp(_s + 1, "(\xCE\xB8)") == 0 || strcmp(_s + 1, "(t)") == 0);
}

// A curve from a line (its name as written into _name). False: not one.
RDE_INTERNAL b8 fzl_curve_of(const c8* _w, fzl_curve* _c, c8* _name, usize _size) {
    memset(_c, 0, sizeof(*_c));
    snprintf(_name, _size, "%s", _w);
    usize _at = 0, _len = 0;
    u8 _rel = FZL_EQ;
    if(!fzl_relation(_w, &_at, &_len, &_rel)) {
        // A function as it is ("x^2 - 1": y = it); with y in it, an equation of it ("x^2 + y^2 - 4": = 0).
        if(!fude_zoom_calc_parse_expr(&_c->f, _w)) {
            return false;
        }
        if(_c->f.vars & (1u << ('y' - 'a'))) {
            _c->kind = FZL_IMPLICIT;
            fude_zoom_calc_parse_expr(&_c->g, "0");
            snprintf(_name, _size, "%s = 0", _w);
        } else {
            _c->kind = FZL_EXPLICIT;
            snprintf(_name, _size, "y = %s", _w);
        }
        return true;
    }
    c8 _lhs[200], _rhs[200];
    fzl_trim(_w, _at, _lhs, sizeof(_lhs));
    fzl_trim(_w + _at + _len, strlen(_w + _at + _len), _rhs, sizeof(_rhs));
    if(_lhs[0] == 0 || _rhs[0] == 0) {
        return false;
    }
    if(_rel == FZL_EQ && fzl_is_y(_lhs) && fude_zoom_calc_parse_expr(&_c->f, _rhs) && !(_c->f.vars & (1u << ('y' - 'a')))) {
        _c->kind = FZL_EXPLICIT;
        return true;
    }
    if(_rel == FZL_EQ && fzl_is_r(_lhs) && fude_zoom_calc_parse_expr(&_c->f, _rhs)) {
        _c->kind = FZL_POLAR;
        return true;
    }
    // Any other: an equation (or inequality) of x and y, its two sides apart (f − g against 0).
    if(!fude_zoom_calc_parse_expr(&_c->f, _lhs) || !fude_zoom_calc_parse_expr(&_c->g, _rhs)) {
        return false;
    }
    _c->kind = FZL_IMPLICIT;
    _c->rel  = _rel;
    return true;
}

// "(2, 3)" a point (_point), "(cos(t), sin(t))" a curve of t. False: neither.
RDE_INTERNAL b8 fzl_pair(const c8* _w, fzl_curve* _c, fude_zoom_v2* _point, b8* _is_point) {
    const usize _n = strlen(_w);
    if(_n < 5u || _w[0] != '(' || _w[_n - 1u] != ')') {
        return false;
    }
    i32 _depth = 0;
    usize _comma = 0;
    for(usize _i = 1; _i + 1u < _n; _i++) {
        if(_w[_i] == '(') {
            _depth++;
        } else if(_w[_i] == ')') {
            if(--_depth < 0) {
                return false;   // ("(x)(y)": not a pair)
            }
        } else if(_w[_i] == ',' && _depth == 0) {
            if(_comma != 0) {
                return false;
            }
            _comma = _i;
        }
    }
    if(_comma == 0 || _depth != 0) {
        return false;
    }
    c8 _a[200], _b[200];
    fzl_trim(_w + 1, _comma - 1u, _a, sizeof(_a));
    fzl_trim(_w + _comma + 1u, _n - _comma - 2u, _b, sizeof(_b));
    f64 _x = 0.0, _y = 0.0;
    if(fude_zoom_calc_number(_a, &_x) && fude_zoom_calc_number(_b, &_y)) {
        *_point    = (fude_zoom_v2){ _x, _y };
        *_is_point = true;
        return true;
    }
    memset(_c, 0, sizeof(*_c));
    if(!fude_zoom_calc_parse_expr(&_c->f, _a) || !fude_zoom_calc_parse_expr(&_c->g, _b)) {
        return false;
    }
    _c->kind   = FZL_PARAMETRIC;
    *_is_point = false;
    return true;
}

// Is _s (trimmed, any case) one of _words (NUL-separated, ending in an empty one)?
RDE_INTERNAL b8 fzl_word(const c8* _s, const c8* _words) {
    for(const c8* _w = _words; *_w != 0; _w += strlen(_w) + 1u) {
        usize _i = 0;
        while(_w[_i] != 0 && _s[_i] != 0 && (_s[_i] | 0x20) == _w[_i]) {
            _i++;
        }
        if(_w[_i] == 0 && _s[_i] == 0) {
            return true;
        }
    }
    return false;
}

RDE_INTERNAL void fzl_short(f64 _v, c8* _out, usize _size);

RDE_INTERNAL void fzl_parse(const c8* _id, const c8* _text, fzl_read* _r) {
    memset(_r, 0, sizeof(*_r));
    const b8 _line_only = strcmp(_id, "number line") == 0;
    const b8 _functions = strcmp(_id, "graph") == 0;
    _r->x0 = -10.0;
    _r->x1 = 10.0;
    _r->t0 = 0.0;
    _r->t1 = 2.0 * FZL_PI;
    for(const c8* _l = _text; _l != NULL && *_l != 0;) {
        const c8* _end = strchr(_l, '\n');
        const usize _len = _end != NULL ? (usize)(_end - _l) : strlen(_l);
        c8 _line[200];
        fzl_trim(_l, _len, _line, sizeof(_line));
        _l = _end != NULL ? _end + 1 : NULL;
        if(_line[0] == 0) {
            continue;
        }
        f64 _a, _b;
        c8 _axis = 0;
        b8 _range = false;
        const c8* _w = _line;
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
        if(fzl_view(_line, &_axis, &_a, &_b)) {
            if(_axis == 'x') {
                _r->x0 = fmin(_a, _b);
                _r->x1 = fmax(_a, _b);
                _r->x_set = true;
            } else if(_axis == 'y') {
                _r->y0    = fmin(_a, _b);
                _r->y1    = fmax(_a, _b);
                _r->y_set = true;
            } else {
                _r->t0    = _a;   // (a curve of t runs from the first to the second, as written)
                _r->t1    = _b;
                _r->t_set = true;
            }
            continue;
        }
        if(_line_only) {
            if(sscanf(_line, " %lf %lf", &_a, &_b) == 2 && _b != _a) {
                _r->x0 = fmin(_a, _b);
                _r->x1 = fmax(_a, _b);
                _r->x_set = true;
            }
            continue;
        }
        if(!_functions) {
            continue;
        }
        // What to mark: its roots, its turning points, where its curves cross; the area under it from a to b.
        if(fzl_word(_line, "roots\0zeros\0zeroes\0")) {
            _r->marks |= FZL_ROOTS;
            continue;
        }
        if(fzl_word(_line, "extrema\0turning points\0max min\0maxima\0minima\0")) {
            _r->marks |= FZL_EXTREMA;
            continue;
        }
        if(fzl_word(_line, "intersections\0crossings\0")) {
            _r->marks |= FZL_CROSSINGS;
            continue;
        }
        if((strncmp(_line, "area ", 5) == 0 || strncmp(_line, "integral ", 9) == 0)) {
            c8 _span[200];
            snprintf(_span, sizeof(_span), "x %s", strchr(_line, ' ') + 1);
            if(fzl_view(_span, &_axis, &_a, &_b)) {
                _r->area0    = fmin(_a, _b);
                _r->area1    = fmax(_a, _b);
                _r->area_set = true;
                continue;
            }
        }
        fude_zoom_v2 _point;
        b8 _is_point = false;
        fzl_curve _c;
        c8 _name[48];
        if(fzl_pair(_w, &_c, &_point, &_is_point)) {
            if(_is_point) {
                if(_r->np < FZL_POINTS) {
                    _r->points[_r->np++] = _point;
                }
            } else if(_r->nf < FZL_FUNCTIONS) {
                _r->c[_r->nf] = _c;
                snprintf(_r->names[_r->nf], sizeof(_r->names[_r->nf]), "%s", _w);
                _r->nf++;
            }
            continue;
        }
        if(_r->nf < FZL_FUNCTIONS && fzl_curve_of(_w, &_c, _name, sizeof(_name))) {
            _r->c[_r->nf] = _c;
            memcpy(_r->names[_r->nf], _name, sizeof(_name));
            _r->nf++;
        }
    }
    // "r = 2" with nothing using an r: not a variable — a polar curve, a circle.
    const u32 _rb = 1u << ('r' - 'a');
    b8 _r_used = false;
    for(u32 _k = 0; _k < _r->nf; _k++) {
        _r_used = _r_used || ((_r->c[_k].f.vars | _r->c[_k].g.vars) & _rb) != 0u;
    }
    if((_r->set & _rb) && !_r_used && !_r->vars['r' - 'a'].range && _r->nf < FZL_FUNCTIONS) {
        c8 _number[48];
        snprintf(_number, sizeof(_number), "%.17g", _r->vars['r' - 'a'].from);
        fzl_curve* _c = &_r->c[_r->nf];
        memset(_c, 0, sizeof(*_c));
        if(fude_zoom_calc_parse_expr(&_c->f, _number)) {
            _c->kind = FZL_POLAR;
            fzl_short(_r->vars['r' - 'a'].from, _number, sizeof(_number));
            snprintf(_r->names[_r->nf], sizeof(_r->names[_r->nf]), "r = %s", _number);
            _r->nf++;
            _r->set &= ~_rb;
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
    return _id != NULL && (strcmp(_id, "graph") == 0 || strcmp(_id, "axes") == 0 || strcmp(_id, "number line") == 0 || fude_zoom_chart_is(_id));   // (charts: chart.h)
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

// --- a curve worked out -------------------------------------------------------------------------------

// An explicit curve's y at _x.
RDE_INTERNAL f64 fzl_y(const fzl_curve* _c, f64 _x, const f64* _vars) {
    return fude_zoom_calc_at_with(&_c->f, _x, _vars);
}

// A curve of one number (x, t or θ: _s) at it: its point (NaN: not there).
RDE_INTERNAL fude_zoom_v2 fzl_at(const fzl_curve* _c, f64 _s, const f64* _vars) {
    switch(_c->kind) {
    case FZL_PARAMETRIC:
        return (fude_zoom_v2){ fude_zoom_calc_at_with(&_c->f, _s, _vars), fude_zoom_calc_at_with(&_c->g, _s, _vars) };
    case FZL_POLAR: {
        const f64 _r = fude_zoom_calc_at_with(&_c->f, _s, _vars);
        return (fude_zoom_v2){ _r * cos(_s), _r * sin(_s) };
    }
    default:
        return (fude_zoom_v2){ _s, fude_zoom_calc_at_with(&_c->f, _s, _vars) };
    }
}

// An equation's f − g at (_x, _y) (y the variable it is: calc.h's letters).
RDE_INTERNAL f64 fzl_implicit_at(const fzl_curve* _c, f64 _x, f64 _y, f64* _vars) {
    _vars['y' - 'a'] = _y;
    return fude_zoom_calc_at_with(&_c->f, _x, _vars) - fude_zoom_calc_at_with(&_c->g, _x, _vars);
}

RDE_INTERNAL b8 fzl_finite(fude_zoom_v2 _p) {
    return isfinite(_p.x) && isfinite(_p.y) && fabs(_p.x) < 1e12 && fabs(_p.y) < 1e12;
}

// Drawing a curve: its view (its numbers) in its box (its own units), the piece being drawn.
typedef struct {
    rde_arr*         points;
    rde_arr*         parts;
    u8               flags;
    f64              x0, x1, y0, y1;
    f64              bx, by;
    f64              tol;       // how far a chord may be from the curve (its own units)
    u32              first;     // the piece's first point
    b8               open;      // a piece being drawn
    fude_zoom_v2     last;      // its last point (the view's numbers)
    u32              budget;
    const fzl_curve* c;
    const f64*       vars;
    f64              taken[48][4];   // where labels are (its own units: left, bottom, right, top) — the legend's first
    u32              ntaken;
} fzl_tracer;

RDE_INTERNAL fude_zoom_v2 fzl_to_box(const fzl_tracer* _t, fude_zoom_v2 _p) {
    return (fude_zoom_v2){ -_t->bx + (_p.x - _t->x0) / (_t->x1 - _t->x0) * 2.0 * _t->bx, -_t->by + (_p.y - _t->y0) / (_t->y1 - _t->y0) * 2.0 * _t->by };
}

// The piece being drawn ended (kept when it is a line).
RDE_INTERNAL void fzl_break(fzl_tracer* _t) {
    if(_t->open) {
        const u32 _n = (u32)rde_arr_length(_t->points) - _t->first;
        if(_n >= 2u) {
            const fude_zoom_symbol_part _p = { _t->first, _n, _t->flags };
            rde_arr_add(_t->parts, (any)&_p);
        } else {
            _t->points->count = _t->first;
        }
    }
    _t->open = false;
}

// A stretch from _a to _b (the view's numbers), as much of it as is in the view: added to the piece being drawn (one
// of its own when it does not go on from it).
RDE_INTERNAL void fzl_segment(fzl_tracer* _t, fude_zoom_v2 _a, fude_zoom_v2 _b) {
    // (Liang–Barsky: the part of it inside the view)
    f64 _ta = 0.0, _tb = 1.0;
    const f64 _dx = _b.x - _a.x, _dy = _b.y - _a.y;
    const f64 _p[4] = { -_dx, _dx, -_dy, _dy };
    const f64 _q[4] = { _a.x - _t->x0, _t->x1 - _a.x, _a.y - _t->y0, _t->y1 - _a.y };
    for(u32 _i = 0; _i < 4u; _i++) {
        if(_p[_i] == 0.0) {
            if(_q[_i] < 0.0) {
                fzl_break(_t);
                return;
            }
            continue;
        }
        const f64 _r = _q[_i] / _p[_i];
        if(_p[_i] < 0.0) {
            _ta = fmax(_ta, _r);
        } else {
            _tb = fmin(_tb, _r);
        }
    }
    if(_ta > _tb) {
        fzl_break(_t);
        return;
    }
    const fude_zoom_v2 _pa = _ta == 0.0 ? _a : (fude_zoom_v2){ _a.x + _dx * _ta, _a.y + _dy * _ta };
    const fude_zoom_v2 _pb = _tb == 1.0 ? _b : (fude_zoom_v2){ _a.x + _dx * _tb, _a.y + _dy * _tb };
    if(!_t->open || _ta > 0.0 || _pa.x != _t->last.x || _pa.y != _t->last.y) {
        fzl_break(_t);
        _t->first = (u32)rde_arr_length(_t->points);
        _t->open  = true;
        const fude_zoom_v2 _q0 = fzl_to_box(_t, _pa);
        rde_arr_add(_t->points, (any)&_q0);
    }
    const fude_zoom_v2 _q1 = fzl_to_box(_t, _pb);
    rde_arr_add(_t->points, (any)&_q1);
    _t->last = _pb;
    if(_tb < 1.0) {
        fzl_break(_t);
    }
}

// From _sa (at _a) to _sb (at _b): finer where the curve bends away from the chord, leaps, or stops — at the finest,
// a leap across the view an asymptote (not drawn across), a stop where it ends.
RDE_INTERNAL void fzl_refine(fzl_tracer* _t, f64 _sa, fude_zoom_v2 _a, f64 _sb, fude_zoom_v2 _b, u32 _depth) {
    const b8 _fa = fzl_finite(_a), _fb = fzl_finite(_b);
    if(!_fa && !_fb) {
        fzl_break(_t);
        return;
    }
    if(_depth < FZL_DEPTH && _t->budget > 0u) {
        const f64 _sm = 0.5 * (_sa + _sb);
        const fude_zoom_v2 _m = fzl_at(_t->c, _sm, _t->vars);
        _t->budget--;
        b8 _finer = _fa != _fb || !fzl_finite(_m);
        if(!_finer) {
            const fude_zoom_v2 _A = fzl_to_box(_t, _a), _B = fzl_to_box(_t, _b), _M = fzl_to_box(_t, _m);
            const f64 _lx = _B.x - _A.x, _ly = _B.y - _A.y;
            const f64 _len = sqrt(_lx * _lx + _ly * _ly);
            const f64 _off = _len > 1e-12 ? fabs((_M.x - _A.x) * _ly - (_M.y - _A.y) * _lx) / _len : hypot(_M.x - _A.x, _M.y - _A.y);
            // (off its chord; or the chord long — a bend both halves of it hide; or the middle not between its ends)
            const f64 _along = _len > 1e-12 ? ((_M.x - _A.x) * _lx + (_M.y - _A.y) * _ly) / (_len * _len) : 0.5;
            _finer = _off > _t->tol || _len > 0.08 * fmin(_t->bx, _t->by) || _along < -0.1 || _along > 1.1;
        }
        if(_finer) {
            fzl_refine(_t, _sa, _a, _sm, _m, _depth + 1u);
            fzl_refine(_t, _sm, _m, _sb, _b, _depth + 1u);
            return;
        }
        fzl_segment(_t, _a, _b);
        return;
    }
    if(_fa && _fb) {
        const fude_zoom_v2 _A = fzl_to_box(_t, _a), _B = fzl_to_box(_t, _b);
        if(fabs(_B.y - _A.y) > _t->by || fabs(_B.x - _A.x) > _t->bx) {
            fzl_break(_t);   // (a leap at the finest: an asymptote)
            return;
        }
        fzl_segment(_t, _a, _b);
        return;
    }
    fzl_break(_t);   // (where it stops)
}

// A curve of one number from _s0 to _s1, as fine as it needs.
RDE_INTERNAL void fzl_trace(fzl_tracer* _t, f64 _s0, f64 _s1) {
    _t->open   = false;
    _t->budget = FZL_BUDGET;
    f64 _ps = _s0;
    fude_zoom_v2 _p = fzl_at(_t->c, _s0, _t->vars);
    for(u32 _i = 1; _i <= FZL_SAMPLES; _i++) {
        const f64 _s = _s0 + (_s1 - _s0) * (f64)_i / (f64)FZL_SAMPLES;
        const fude_zoom_v2 _q = fzl_at(_t->c, _s, _t->vars);
        fzl_refine(_t, _ps, _p, _s, _q, 0u);
        _ps = _s;
        _p  = _q;
    }
    fzl_break(_t);
}

// --- an equation's curve: marching squares --------------------------------------------------------

typedef struct {
    u32          edge[2];   // the grid's edges it joins (2 · node + 0 across, + 1 up)
    fude_zoom_v2 at[2];     // where (the view's numbers)
} fzl_seg;

typedef struct {
    u32 edge, end;   // an edge, and which segment's which end is on it (segment · 2 + end)
} fzl_end;

RDE_INTERNAL int fzl_by_edge(const void* _a, const void* _b) {
    const fzl_end* _x = (const fzl_end*)_a;
    const fzl_end* _y = (const fzl_end*)_b;
    return _x->edge < _y->edge ? -1 : (_x->edge > _y->edge ? 1 : (_x->end < _y->end ? -1 : (_x->end > _y->end ? 1 : 0)));
}

// Where on the edge from _a (_fa) to _b (_fb, the other sign) f − g is nothing: a few steps of regula falsi (Illinois).
// False: what it crosses there is a pole (1/x's), not the curve.
RDE_INTERNAL b8 fzl_cross(const fzl_curve* _c, f64* _vars, fude_zoom_v2 _a, f64 _fa, fude_zoom_v2 _b, f64 _fb, fude_zoom_v2* _out) {
    f64 _lo = 0.0, _hi = 1.0, _flo = _fa, _fhi = _fb;
    f64 _t = _fa / (_fa - _fb);
    f64 _ft = 0.0;
    i32 _side = 0;
    for(u32 _i = 0; _i < 5u; _i++) {
        _ft = fzl_implicit_at(_c, _a.x + (_b.x - _a.x) * _t, _a.y + (_b.y - _a.y) * _t, _vars);
        if(!isfinite(_ft)) {
            break;
        }
        if((_ft > 0.0) == (_flo > 0.0)) {
            _lo = _t; _flo = _ft;
            if(_side == -1) { _fhi *= 0.5; }
            _side = -1;
        } else {
            _hi = _t; _fhi = _ft;
            if(_side == 1) { _flo *= 0.5; }
            _side = 1;
        }
        if(_flo == _fhi) {
            break;
        }
        _t = _lo + (_hi - _lo) * _flo / (_flo - _fhi);
    }
    *_out = (fude_zoom_v2){ _a.x + (_b.x - _a.x) * _t, _a.y + (_b.y - _a.y) * _t };
    return isfinite(_ft) && fabs(_ft) <= 0.2 * fmax(fabs(_fa), fabs(_fb));
}

// The region f − g _rel 0 (an inequality's: shaded) — is a value of it in it?
RDE_INTERNAL b8 fzl_in(u8 _rel, f64 _v) {
    return _rel == FZL_LT || _rel == FZL_LE ? _v < 0.0 : _v > 0.0;
}

// An equation's curve (its pieces joined into lines) in the view, on a grid, its edge dashed when it is not in it (< >)
// — or (_region_only) an inequality's region shaded (flags 64 + its: polygons, a run of whole cells one).
RDE_INTERNAL void fzl_implicit(fzl_tracer* _t, u32 _k, const f64* _vars_in, b8 _region_only) {
    const fzl_curve* _c = _t->c;
    f64 _vars[26];
    memcpy(_vars, _vars_in, sizeof(_vars));
    const f64 _wide = _t->bx >= _t->by ? 1.0 : _t->bx / _t->by, _high = _t->by >= _t->bx ? 1.0 : _t->by / _t->bx;
    const u32 _nx = (u32)fmax(8.0, round((f64)FZL_GRID * _wide)), _ny = (u32)fmax(8.0, round((f64)FZL_GRID * _high));
    const f64 _dx = (_t->x1 - _t->x0) / (f64)_nx, _dy = (_t->y1 - _t->y0) / (f64)_ny;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _v_arr = rde_arr_new(sizeof(f64), _heap);
    f64* _v = (f64*)rde_arr_add_n(&_v_arr, (usize)(_nx + 1u) * (_ny + 1u));
    for(u32 _j = 0; _j <= _ny; _j++) {
        for(u32 _i = 0; _i <= _nx; _i++) {
            _v[_j * (_nx + 1u) + _i] = fzl_implicit_at(_c, _t->x0 + _dx * (f64)_i, _t->y0 + _dy * (f64)_j, _vars);
        }
    }
    #define FZL_V(_i, _j) _v[(_j) * (_nx + 1u) + (_i)]
    #define FZL_P(_i, _j) ((fude_zoom_v2){ _t->x0 + _dx * (f64)(_i), _t->y0 + _dy * (f64)(_j) })
    const b8 _shade = _c->rel != FZL_EQ && _region_only;
    const u8 _region = (u8)(64u + _k);
    rde_arr _segs = rde_arr_new(sizeof(fzl_seg), _heap);
    for(u32 _j = 0; _j < _ny; _j++) {
        u32 _run = FZL_NONE;   // (whole cells in the region in a row: one box)
        for(u32 _i = 0; _i <= _nx; _i++) {
            f64 _c4[4] = { 0 };
            b8 _whole = false, _known = _i < _nx;
            if(_known) {
                _c4[0] = FZL_V(_i, _j); _c4[1] = FZL_V(_i + 1u, _j); _c4[2] = FZL_V(_i + 1u, _j + 1u); _c4[3] = FZL_V(_i, _j + 1u);
                _known = isfinite(_c4[0]) && isfinite(_c4[1]) && isfinite(_c4[2]) && isfinite(_c4[3]);
                _whole = _known && _shade && fzl_in(_c->rel, _c4[0]) && fzl_in(_c->rel, _c4[1]) && fzl_in(_c->rel, _c4[2]) && fzl_in(_c->rel, _c4[3]);
            }
            if(_shade && !_whole && _run != FZL_NONE) {
                const fude_zoom_v2 _box[4] = { fzl_to_box(_t, FZL_P(_run, _j)), fzl_to_box(_t, FZL_P(_i, _j)), fzl_to_box(_t, FZL_P(_i, _j + 1u)),
                                               fzl_to_box(_t, FZL_P(_run, _j + 1u)) };
                const fude_zoom_symbol_part _p = { (u32)rde_arr_length(_t->points), 4u, _region };
                for(u32 _q = 0; _q < 4u; _q++) {
                    rde_arr_add(_t->points, (any)&_box[_q]);
                }
                rde_arr_add(_t->parts, (any)&_p);
                _run = FZL_NONE;
            }
            if(_whole) {
                if(_run == FZL_NONE) {
                    _run = _i;
                }
                continue;
            }
            if(!_known) {
                continue;
            }
            const fude_zoom_v2 _corner[4] = { FZL_P(_i, _j), FZL_P(_i + 1u, _j), FZL_P(_i + 1u, _j + 1u), FZL_P(_i, _j + 1u) };
            // (its edges: bottom, right, top, left — each corner to the next; their ids on the grid)
            const u32 _node = _j * (_nx + 1u) + _i;
            const u32 _ids[4] = { 2u * _node, 2u * (_node + 1u) + 1u, 2u * (_node + _nx + 1u), 2u * _node + 1u };
            fude_zoom_v2 _x[4];
            b8 _crosses[4], _real[4];
            for(u32 _e = 0; _e < 4u; _e++) {
                const u32 _f = (_e + 1u) % 4u;
                _crosses[_e] = (_c4[_e] > 0.0) != (_c4[_f] > 0.0);
                _real[_e] = false;
                if(_crosses[_e]) {
                    // (always from the edge's lower end: the cells either side of it find the same point)
                    const b8 _flip = _e >= 2u;
                    const u32 _from = _flip ? _f : _e, _to = _flip ? _e : _f;
                    _real[_e] = fzl_cross(_c, _vars, _corner[_from], _c4[_from], _corner[_to], _c4[_to], &_x[_e]);
                }
            }
            // The curve through it: two crossed edges joined; four (a saddle), paired as its middle says.
            u32 _pairs[2][2], _np = 0;
            const u32 _n = (u32)_crosses[0] + (u32)_crosses[1] + (u32)_crosses[2] + (u32)_crosses[3];
            if(_n == 2u) {
                u32 _w = 0;
                for(u32 _e = 0; _e < 4u; _e++) {
                    if(_crosses[_e]) {
                        _pairs[0][_w++] = _e;
                    }
                }
                _np = 1u;
            } else if(_n == 4u) {
                const f64 _mid = fzl_implicit_at(_c, _corner[0].x + 0.5 * _dx, _corner[0].y + 0.5 * _dy, _vars);
                const b8 _same = (_mid > 0.0) == (_c4[0] > 0.0);   // (the middle as the bottom left corner: those two joined)
                if(_same) {
                    _pairs[0][0] = 0u; _pairs[0][1] = 1u;   // (round the bottom right corner)
                    _pairs[1][0] = 2u; _pairs[1][1] = 3u;   // (...the top left)
                } else {
                    _pairs[0][0] = 3u; _pairs[0][1] = 0u;   // (round the bottom left)
                    _pairs[1][0] = 1u; _pairs[1][1] = 2u;   // (...the top right)
                }
                _np = 2u;
            }
            for(u32 _q = 0; _q < _np && !_region_only; _q++) {
                const u32 _e0 = _pairs[_q][0], _e1 = _pairs[_q][1];
                if(_real[_e0] && _real[_e1]) {
                    const fzl_seg _s = { { _ids[_e0], _ids[_e1] }, { _x[_e0], _x[_e1] } };
                    rde_arr_add(&_segs, (any)&_s);
                }
            }
            // Its share of the region: its corners in it and where its edges cross, in turn round it.
            if(_shade) {
                fude_zoom_v2 _poly[8];
                u32 _m = 0;
                for(u32 _e = 0; _e < 4u; _e++) {
                    if(fzl_in(_c->rel, _c4[_e])) {
                        _poly[_m++] = fzl_to_box(_t, _corner[_e]);
                    }
                    if(_crosses[_e]) {
                        fude_zoom_v2 _at = _x[_e];
                        if(!_real[_e]) {
                            const u32 _f = (_e + 1u) % 4u;
                            const f64 _r = _c4[_e] / (_c4[_e] - _c4[_f]);
                            _at = (fude_zoom_v2){ _corner[_e].x + (_corner[_f].x - _corner[_e].x) * _r, _corner[_e].y + (_corner[_f].y - _corner[_e].y) * _r };
                        }
                        _poly[_m++] = fzl_to_box(_t, _at);
                    }
                }
                if(_m >= 3u) {
                    const fude_zoom_symbol_part _p = { (u32)rde_arr_length(_t->points), _m, _region };
                    for(u32 _q = 0; _q < _m; _q++) {
                        rde_arr_add(_t->points, (any)&_poly[_q]);
                    }
                    rde_arr_add(_t->parts, (any)&_p);
                }
            }
        }
    }
    #undef FZL_V
    #undef FZL_P
    // The pieces joined where they share an edge: lines through the cells, each its own part.
    const u32 _ns = (u32)rde_arr_length(&_segs);
    if(_ns == 0u) {
        rde_arr_free(&_v_arr);
        rde_arr_free(&_segs);
        return;
    }
    const fzl_seg* _s = (const fzl_seg*)_segs.memory;
    rde_arr _ends_arr = rde_arr_new(sizeof(fzl_end), _heap);
    fzl_end* _ends = (fzl_end*)rde_arr_add_n(&_ends_arr, 2u * (usize)_ns);
    for(u32 _i = 0; _i < _ns; _i++) {
        _ends[2u * _i]      = (fzl_end){ _s[_i].edge[0], 2u * _i };
        _ends[2u * _i + 1u] = (fzl_end){ _s[_i].edge[1], 2u * _i + 1u };
    }
    qsort(_ends, 2u * (usize)_ns, sizeof(fzl_end), fzl_by_edge);
    rde_arr _link_arr = rde_arr_new(sizeof(u32), _heap), _seen_arr = rde_arr_new(sizeof(u8), _heap);
    u32* _link = (u32*)rde_arr_add_n(&_link_arr, 2u * (usize)_ns);   // (each end: the end it meets, or NONE)
    u8*  _seen = (u8*)rde_arr_add_n(&_seen_arr, (usize)_ns);
    for(u32 _i = 0; _i < 2u * _ns; _i++) {
        _link[_i] = FZL_NONE;
    }
    memset(_seen, 0, _ns);
    for(u32 _i = 0; _i + 1u < 2u * _ns; _i++) {
        if(_ends[_i].edge == _ends[_i + 1u].edge && (_i + 2u >= 2u * _ns || _ends[_i + 2u].edge != _ends[_i].edge) &&
           (_i == 0u || _ends[_i - 1u].edge != _ends[_i].edge)) {
            _link[_ends[_i].end]      = _ends[_i + 1u].end;
            _link[_ends[_i + 1u].end] = _ends[_i].end;
        }
    }
    const u8 _flags = (u8)(32u + _k + (_c->rel == FZL_LT || _c->rel == FZL_GT ? 8u : 0u));
    for(u32 _i = 0; _i < _ns; _i++) {
        if(_seen[_i]) {
            continue;
        }
        // (back to where its line starts — or all the way round a loop)
        u32 _at = 2u * _i;   // (leaving segment _i by its end 0)
        for(u32 _guard = 0; _guard < _ns; _guard++) {
            const u32 _next = _link[_at];
            if(_next == FZL_NONE || _next / 2u == _i) {
                break;
            }
            _at = _next ^ 1u;   // (through that segment, out of its other end)
        }
        // (from there: in by _at's end, out of the other, on)
        const u32 _first = (u32)rde_arr_length(_t->points);
        u32 _in = _at;
        const fude_zoom_v2 _q0 = fzl_to_box(_t, _s[_in / 2u].at[_in & 1u]);
        rde_arr_add(_t->points, (any)&_q0);
        for(u32 _guard = 0; _guard <= _ns; _guard++) {
            const u32 _seg = _in / 2u;
            if(_seen[_seg]) {
                break;
            }
            _seen[_seg] = 1u;
            const u32 _out = _in ^ 1u;
            const fude_zoom_v2 _q = fzl_to_box(_t, _s[_seg].at[_out & 1u]);
            rde_arr_add(_t->points, (any)&_q);
            if(_link[_out] == FZL_NONE) {
                break;
            }
            _in = _link[_out];
        }
        const u32 _n = (u32)rde_arr_length(_t->points) - _first;
        if(_n >= 2u) {
            const fude_zoom_symbol_part _p = { _first, _n, _flags };
            rde_arr_add(_t->parts, (any)&_p);
        } else {
            _t->points->count = _first;
        }
    }
    rde_arr_free(&_v_arr);
    rde_arr_free(&_segs);
    rde_arr_free(&_ends_arr);
    rde_arr_free(&_link_arr);
    rde_arr_free(&_seen_arr);
}

// Where an equation's curve is, roughly (a coarse grid over _box: its cells it crosses): their bounds widened into _b
// (x0, y0, x1, y1). False: none there.
RDE_INTERNAL b8 fzl_implicit_bounds(const fzl_curve* _c, const f64* _vars_in, f64 _r, f64* _b) {
    f64 _vars[26];
    memcpy(_vars, _vars_in, sizeof(_vars));
    const u32 _n = 64u;
    const f64 _d = 2.0 * _r / (f64)_n;
    f64 _row[65], _prev[65];
    b8 _any = false;
    for(u32 _j = 0; _j <= _n; _j++) {
        for(u32 _i = 0; _i <= _n; _i++) {
            _row[_i] = fzl_implicit_at(_c, -_r + _d * (f64)_i, -_r + _d * (f64)_j, _vars);   // (an inequality: round its edge)
        }
        for(u32 _i = 0; _j > 0u && _i < _n; _i++) {
            const f64 _a = _prev[_i], _b2 = _prev[_i + 1u], _c2 = _row[_i + 1u], _d2 = _row[_i];
            if(!isfinite(_a) || !isfinite(_b2) || !isfinite(_c2) || !isfinite(_d2)) {
                continue;
            }
            const b8 _p = _a > 0.0;
            if((_b2 > 0.0) != _p || (_c2 > 0.0) != _p || (_d2 > 0.0) != _p) {
                const f64 _x = -_r + _d * (f64)_i, _y = -_r + _d * (f64)(_j - 1u);
                _b[0] = _any ? fmin(_b[0], _x) : _x;
                _b[1] = _any ? fmin(_b[1], _y) : _y;
                _b[2] = _any ? fmax(_b[2], _x + _d) : _x + _d;
                _b[3] = _any ? fmax(_b[3], _y + _d) : _y + _d;
                _any = true;
            }
        }
        memcpy(_prev, _row, sizeof(_row));
    }
    return _any;
}

// --- what is marked on it -----------------------------------------------------------------------------

// The root of _f − _g (explicit curves; _g NULL: nothing) between _a and _b, its sign changing: bisected.
RDE_INTERNAL f64 fzl_bisect(const fzl_curve* _f, const fzl_curve* _g, const f64* _vars, f64 _a, f64 _b) {
    f64 _fa = fzl_y(_f, _a, _vars) - (_g != NULL ? fzl_y(_g, _a, _vars) : 0.0);
    for(u32 _i = 0; _i < 60u; _i++) {
        const f64 _m = 0.5 * (_a + _b);
        const f64 _fm = fzl_y(_f, _m, _vars) - (_g != NULL ? fzl_y(_g, _m, _vars) : 0.0);
        if(!isfinite(_fm)) {
            break;
        }
        if((_fm > 0.0) == (_fa > 0.0)) {
            _a  = _m;
            _fa = _fm;
        } else {
            _b = _m;
        }
    }
    return 0.5 * (_a + _b);
}

// Where _f − _g (or _f alone) crosses nothing in [_x0, _x1] (a pole's sign change left out: it does not come near
// nothing): up to _most of them into _out. How many.
RDE_INTERNAL u32 fzl_roots(const fzl_curve* _f, const fzl_curve* _g, const f64* _vars, f64 _x0, f64 _x1, f64 _span, f64* _out, u32 _most) {
    const u32 _n = 800u;
    u32 _found = 0;
    f64 _px = _x0;
    f64 _pv = fzl_y(_f, _x0, _vars) - (_g != NULL ? fzl_y(_g, _x0, _vars) : 0.0);
    for(u32 _i = 1; _i <= _n && _found < _most; _i++) {
        const f64 _x = _x0 + (_x1 - _x0) * (f64)_i / (f64)_n;
        const f64 _v = fzl_y(_f, _x, _vars) - (_g != NULL ? fzl_y(_g, _x, _vars) : 0.0);
        if(isfinite(_v) && isfinite(_pv)) {
            if(_v == 0.0) {
                _out[_found++] = _x;
            } else if(_pv != 0.0 && (_v > 0.0) != (_pv > 0.0)) {
                const f64 _r = fzl_bisect(_f, _g, _vars, _px, _x);
                const f64 _at = fzl_y(_f, _r, _vars) - (_g != NULL ? fzl_y(_g, _r, _vars) : 0.0);
                if(isfinite(_at) && fabs(_at) < 1e-6 * fmax(_span, 1e-9)) {
                    _out[_found++] = _r;
                }
            }
        }
        _px = _x;
        _pv = _v;
    }
    return _found;
}

// Its turning points in [_x0, _x1]: where it stops rising and falls (or the other way), found to a fine point (golden
// section). Up to _most into _out. How many.
RDE_INTERNAL u32 fzl_extrema(const fzl_curve* _f, const f64* _vars, f64 _x0, f64 _x1, f64 _span, f64* _out, u32 _most) {
    const u32 _n = 800u;
    const f64 _h = (_x1 - _x0) / (f64)_n;
    u32 _found = 0;
    f64 _a = fzl_y(_f, _x0, _vars), _b = fzl_y(_f, _x0 + _h, _vars);
    for(u32 _i = 2; _i <= _n && _found < _most; _i++) {
        const f64 _x = _x0 + _h * (f64)_i;
        const f64 _c = fzl_y(_f, _x, _vars);
        if(isfinite(_a) && isfinite(_b) && isfinite(_c)) {
            const b8 _max = _b > _a && _b >= _c, _min = _b < _a && _b <= _c;
            if((_max || _min) && fmax(fabs(_b - _a), fabs(_b - _c)) > 1e-9 * fmax(_span, 1e-9)) {
                // (golden section between its neighbours)
                f64 _lo = _x - 2.0 * _h, _hi = _x;
                const f64 _g = 0.6180339887498949;
                for(u32 _k = 0; _k < 60u; _k++) {
                    const f64 _m1 = _hi - (_hi - _lo) * _g, _m2 = _lo + (_hi - _lo) * _g;
                    const f64 _v1 = fzl_y(_f, _m1, _vars), _v2 = fzl_y(_f, _m2, _vars);
                    if(_max ? _v1 > _v2 : _v1 < _v2) {
                        _hi = _m2;
                    } else {
                        _lo = _m1;
                    }
                }
                _out[_found++] = 0.5 * (_lo + _hi);
            }
        }
        _a = _b;
        _b = _c;
    }
    return _found;
}

// The area between _f and the x axis from _a to _b (Simpson's rule, fine; NaN: not all of it there).
RDE_INTERNAL f64 fzl_area(const fzl_curve* _f, const f64* _vars, f64 _a, f64 _b) {
    const u32 _n = 2000u;
    const f64 _h = (_b - _a) / (f64)_n;
    f64 _sum = fzl_y(_f, _a, _vars) + fzl_y(_f, _b, _vars);
    for(u32 _i = 1; _i < _n; _i++) {
        _sum += (_i % 2u == 1u ? 4.0 : 2.0) * fzl_y(_f, _a + _h * (f64)_i, _vars);
    }
    return _sum * _h / 3.0;
}

// --- its legend as maths ----------------------------------------------------------------------------

// Curve _c (written _name) as LaTeX: "y = \frac{x^{2}}{4} - 1", "r = 1 + \cos \theta", "\left(\cos t, \sin 2 t\right)",
// "x^{2} + y^{2} \le 4". Empty: not so written.
RDE_INTERNAL void fzl_latex(const fzl_curve* _c, const c8* _name, c8* _out, usize _size) {
    static const c8* const _rels[] = { "=", "<", "\\le", ">", "\\ge" };
    c8 _a[192], _b[192];
    _out[0] = 0;
    switch(_c->kind) {
    case FZL_EXPLICIT: {
        c8 _lhs[32] = "y";   // (its left side as written: y, f(x))
        const c8* _eq = strchr(_name, '=');
        if(_eq != NULL) {
            fzl_trim(_name, (usize)(_eq - _name), _lhs, sizeof(_lhs));
        }
        if(fude_zoom_calc_latex(&_c->f, "x", _a, sizeof(_a))) {
            snprintf(_out, _size, "%s = %s", _lhs, _a);
        }
        break;
    }
    case FZL_POLAR:
        if(fude_zoom_calc_latex(&_c->f, "\\theta", _a, sizeof(_a))) {
            snprintf(_out, _size, "r = %s", _a);
        }
        break;
    case FZL_PARAMETRIC:
        if(fude_zoom_calc_latex(&_c->f, "t", _a, sizeof(_a)) && fude_zoom_calc_latex(&_c->g, "t", _b, sizeof(_b))) {
            snprintf(_out, _size, "\\left(%s, %s\\right)", _a, _b);
        }
        break;
    default:
        if(fude_zoom_calc_latex(&_c->f, "x", _a, sizeof(_a)) && fude_zoom_calc_latex(&_c->g, "x", _b, sizeof(_b))) {
            snprintf(_out, _size, "%s %s %s", _a, _rels[_c->rel % 5u], _b);
        }
        break;
    }
    if(strlen(_out) + 1u >= _size) {
        _out[0] = 0;   // (cut short: not maths — its words then)
    }
}

// --- drawn -------------------------------------------------------------------------------------

u32 fude_zoom_plot_lines(const c8* _id, const c8* _text, f64 _hw, f64 _hh, rde_arr* _points, rde_arr* _parts, rde_arr* _labels) {
    return fude_zoom_plot_lines_play(_id, _text, _hw, _hh, NULL, _points, _parts, _labels);
}

// A text's width, roughly (its own units, letters _lh high): what a label's room is reckoned by.
RDE_INTERNAL f64 fzl_text_w(const c8* _s, f64 _lh) {
    u32 _n = 0;
    for(const c8* _p = _s; *_p != 0; _p++) {
        _n += ((u8)*_p & 0xC0u) != 0x80u ? 1u : 0u;
    }
    return 0.56 * _lh * (f64)_n;
}

// A point's numbers by its dot at _c (its own units): above it to the right, else below, else to its left — wherever
// no other label (nor the legend) is, inside the graph; nowhere free, left out (its dot says where it is).
RDE_INTERNAL void fzl_label_by(fzl_tracer* _t, rde_arr* _labels, fude_zoom_v2 _c, fude_zoom_v2 _p, u8 _curve, f64 _lh) {
    if(_labels == NULL) {
        return;
    }
    fude_zoom_plot_label _lb = { { 0.0, 0.0 }, { 0 }, 3u, _curve };
    c8 _a[24], _b[24];
    fzl_short(_p.x, _a, sizeof(_a));
    fzl_short(_p.y, _b, sizeof(_b));
    snprintf(_lb.text, sizeof(_lb.text), "(%s, %s)", _a, _b);
    const f64 _w = fzl_text_w(_lb.text, _lh);
    const fude_zoom_v2 _try[4] = { { _c.x + _lh * 0.5, _c.y + _lh * 0.6 }, { _c.x + _lh * 0.5, _c.y - _lh * 0.6 },
                                   { _c.x - _lh * 0.5 - _w, _c.y + _lh * 0.6 }, { _c.x - _lh * 0.5 - _w, _c.y - _lh * 0.6 } };
    for(u32 _i = 0; _i < 4u; _i++) {
        const f64 _r[4] = { _try[_i].x, _try[_i].y - _lh * 0.55, _try[_i].x + _w, _try[_i].y + _lh * 0.55 };
        b8 _free = _r[0] >= -_t->bx / 0.92 && _r[2] <= _t->bx / 0.92 && _r[1] >= -_t->by / 0.88 && _r[3] <= _t->by / 0.88;
        for(u32 _k = 0; _k < _t->ntaken && _free; _k++) {
            const f64* _o = _t->taken[_k];
            _free = _r[2] <= _o[0] || _r[0] >= _o[2] || _r[3] <= _o[1] || _r[1] >= _o[3];
        }
        if(_free) {
            _lb.at = _try[_i];
            rde_arr_add(_labels, (any)&_lb);
            if(_t->ntaken < 48u) {
                memcpy(_t->taken[_t->ntaken++], _r, sizeof(_r));
            }
            return;
        }
    }
}

// A dot marked on curve _k at _p (the view's numbers), its numbers by it.
RDE_INTERNAL void fzl_mark(fzl_tracer* _t, rde_arr* _labels, u32 _k, fude_zoom_v2 _p, f64 _lh) {
    const fude_zoom_v2 _c = fzl_to_box(_t, _p);
    fzl_dot(_t->points, _t->parts, _c, _lh * 0.32, (u8)(32u + _k));
    fzl_label_by(_t, _labels, _c, _p, (u8)(1u + _k), _lh);
}

u32 fude_zoom_plot_lines_play(const c8* _id, const c8* _text, f64 _hw, f64 _hh, const fude_zoom_plot_play* _play, rde_arr* _points, rde_arr* _parts,
                              rde_arr* _labels) {
    if(!fude_zoom_plot_is(_id)) {
        return 0;
    }
    if(fude_zoom_chart_is(_id)) {
        return fude_zoom_chart_lines(_id, _text, _hw, _hh, _points, _parts, _labels);   // (a chart from data: chart.h)
    }
    const b8 _line_only = strcmp(_id, "number line") == 0;
    // Its text: a curve a line, a view's ("x -5 5", "y -2 2", "t 0 2pi"; a number line's: its two numbers), a
    // variable's ("a = 2", "a = 0..5"), where points are marked ("x = 1.5"), a point ("(2, 3)"), what to mark.
    fzl_read* _r = (fzl_read*)malloc(sizeof(fzl_read));
    if(_r == NULL) {
        return 0;
    }
    fzl_parse(_id, _text, _r);
    const fzl_curve* _f = _r->c;
    const u32 _nf = _r->nf;
    f64 _x0 = _r->x0, _x1 = _r->x1, _y0 = _r->y0, _y1 = _r->y1;
    // Each variable's value now (NaN: not set).
    f64 _vars[26];
    for(u32 _k = 0; _k < 26u; _k++) {
        _vars[_k] = (_r->set & (1u << _k)) ? fzl_value(&_r->vars[_k], _play) : NAN;
    }
    u32 _explicit = 0;
    for(u32 _k = 0; _k < _nf; _k++) {
        _explicit += _f[_k].kind == FZL_EXPLICIT ? 1u : 0u;
    }
    const f64 _bx = _hw * 0.92, _by = _hh * 0.88;   // (the view in its box, a little in from its edges)
    if(_explicit == 0u && _nf > 0u && !_line_only && (!_r->x_set || !_r->y_set)) {
        // Only curves of t, θ, or equations: the view round them (their points, their curves' cells), as long one way as
        // the other — a circle round.
        f64 _b[4] = { 0 };
        b8 _any = false;
        for(u32 _k = 0; _k < _nf; _k++) {
            if(_f[_k].kind == FZL_IMPLICIT) {
                f64 _e[4];
                b8 _hit = false;
                const f64 _tries[3] = { 10.0, 100.0, 1.0 };
                for(u32 _q = 0; _q < 3u && !_hit; _q++) {
                    _hit = fzl_implicit_bounds(&_f[_k], _vars, _tries[_q], _e);
                }
                if(_hit) {
                    _b[0] = _any ? fmin(_b[0], _e[0]) : _e[0]; _b[1] = _any ? fmin(_b[1], _e[1]) : _e[1];
                    _b[2] = _any ? fmax(_b[2], _e[2]) : _e[2]; _b[3] = _any ? fmax(_b[3], _e[3]) : _e[3];
                    _any = true;
                }
                continue;
            }
            for(u32 _i = 0; _i <= 720u; _i++) {
                const fude_zoom_v2 _p = fzl_at(&_f[_k], _r->t0 + (_r->t1 - _r->t0) * (f64)_i / 720.0, _vars);
                if(fzl_finite(_p) && fabs(_p.x) < 1e6 && fabs(_p.y) < 1e6) {
                    _b[0] = _any ? fmin(_b[0], _p.x) : _p.x; _b[1] = _any ? fmin(_b[1], _p.y) : _p.y;
                    _b[2] = _any ? fmax(_b[2], _p.x) : _p.x; _b[3] = _any ? fmax(_b[3], _p.y) : _p.y;
                    _any = true;
                }
            }
        }
        for(u32 _i = 0; _i < _r->np; _i++) {
            const fude_zoom_v2 _p = _r->points[_i];
            _b[0] = _any ? fmin(_b[0], _p.x) : _p.x; _b[1] = _any ? fmin(_b[1], _p.y) : _p.y;
            _b[2] = _any ? fmax(_b[2], _p.x) : _p.x; _b[3] = _any ? fmax(_b[3], _p.y) : _p.y;
            _any = true;
        }
        if(!_any) {
            _b[0] = -10.0; _b[1] = -10.0; _b[2] = 10.0; _b[3] = 10.0;
        }
        if(_r->x_set) {
            _b[0] = _x0; _b[2] = _x1;
        }
        if(_r->y_set) {
            _b[1] = _y0; _b[3] = _y1;
        }
        // (a little round it, the axes' crossing in sight when it is near; then one unit as long across as up)
        f64 _cx = 0.5 * (_b[0] + _b[2]), _cy = 0.5 * (_b[1] + _b[3]);
        f64 _sx = fmax(_b[2] - _b[0], 1e-6), _sy = fmax(_b[3] - _b[1], 1e-6);
        if(!_r->x_set) { _sx *= 1.2; }
        if(!_r->y_set) { _sy *= 1.2; }
        const f64 _unit = fmax(_sx / (2.0 * _bx), _sy / (2.0 * _by));
        if(!_r->x_set) { _sx = _unit * 2.0 * _bx; }
        if(!_r->y_set) { _sy = _unit * 2.0 * _by; }
        if(!_r->x_set) { _x0 = _cx - 0.5 * _sx; _x1 = _cx + 0.5 * _sx; }
        if(!_r->y_set) { _y0 = _cy - 0.5 * _sy; _y1 = _cy + 0.5 * _sy; }
    } else if(!_r->y_set) {
        // Its y as its functions go (the middle 96 % of what they reach: an asymptote's spike left out), else as x is.
        // (a variable over a range: as it is anywhere in it, so the axes stay as it moves)
        const u32 _passes = 5u, _samples = 480u;
        const u32 _most = _samples * (_nf > 0u ? _nf : 1u) * _passes;
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
                if(_f[_k].kind != FZL_EXPLICIT) {
                    continue;
                }
                for(u32 _i = 0; _i < _samples; _i += (_pass == 0u ? 1u : 4u)) {
                    const f64 _y = fzl_y(&_f[_k], _x0 + (_x1 - _x0) * (f64)_i / (f64)(_samples - 1u), _at);
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
            const f64 _half = (_x1 - _x0) * 0.5 * (_hh / fmax(_hw, 1e-300));
            _y0 = -_half;
            _y1 = _half;
        }
        rde_arr_free(&_v_arr);
    }
    #define FZL_X(_v) (-_bx + ((_v) - _x0) / (_x1 - _x0) * 2.0 * _bx)
    #define FZL_Y(_v) (-_by + ((_v) - _y0) / (_y1 - _y0) * 2.0 * _by)
    fzl_tracer _t;
    memset(&_t, 0, sizeof(_t));
    _t.points = _points; _t.parts = _parts;
    _t.x0 = _x0; _t.x1 = _x1; _t.y0 = _y0; _t.y1 = _y1; _t.bx = _bx; _t.by = _by;
    _t.tol  = 0.0008 * fmin(_bx, _by);
    _t.vars = _vars;
    // Under everything: the regions its inequalities hold, and the area it is asked for under its first function.
    for(u32 _k = 0; _k < _nf; _k++) {
        if(_f[_k].kind == FZL_IMPLICIT && _f[_k].rel != FZL_EQ) {
            _t.c = &_f[_k];
            fzl_implicit(&_t, _k, _vars, true);
        }
    }
    u32 _first_fn = FZL_NONE;
    for(u32 _k = 0; _k < _nf && _first_fn == FZL_NONE; _k++) {
        if(_f[_k].kind == FZL_EXPLICIT) {
            _first_fn = _k;
        }
    }
    f64 _area = NAN;
    if(_r->area_set && _first_fn != FZL_NONE) {
        const fzl_curve* _af = &_f[_first_fn];
        _area = fzl_area(_af, _vars, _r->area0, _r->area1);
        // (between the curve and the axis, as much of it as is in the view: a polygon its edge over and back under)
        const f64 _a = fmax(_r->area0, _x0), _b = fmin(_r->area1, _x1);
        const f64 _base = fmin(fmax(0.0, _y0), _y1);
        if(_b > _a) {
            const u32 _n = 240u;
            u32 _start = (u32)rde_arr_length(_points);
            for(u32 _i = 0; _i <= _n + 1u; _i++) {
                const f64 _x = _a + (_b - _a) * (f64)(_i < _n ? _i : _n) / (f64)_n;
                const f64 _y = _i <= _n ? fzl_y(_af, _x, _vars) : NAN;
                const u32 _have = (u32)rde_arr_length(_points) - _start;
                if(!isfinite(_y)) {
                    if(_have >= 1u) {
                        // (the piece closed down to the axis)
                        const fude_zoom_v2 _q[2] = { { ((fude_zoom_v2*)_points->memory)[_points->count - 1u].x, FZL_Y(_base) },
                                                     { ((fude_zoom_v2*)_points->memory)[_start].x, FZL_Y(_base) } };
                        rde_arr_add(_points, (any)&_q[0]);
                        rde_arr_add(_points, (any)&_q[1]);
                        const fude_zoom_symbol_part _p = { _start, _have + 2u, (u8)(64u + _first_fn) };
                        rde_arr_add(_parts, (any)&_p);
                    }
                    _start = (u32)rde_arr_length(_points);
                    continue;
                }
                const fude_zoom_v2 _q = { FZL_X(_x), FZL_Y(fmin(fmax(_y, _y0), _y1)) };
                rde_arr_add(_points, (any)&_q);
            }
        }
    }
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
    // Each curve (its own colour: its parts' flags 32 + which): broken where it is not, or leaps; cut at the view's edge.
    for(u32 _k = 0; _k < _nf; _k++) {
        _t.c     = &_f[_k];
        _t.flags = (u8)(32u + _k);
        if(_f[_k].kind == FZL_IMPLICIT) {
            fzl_implicit(&_t, _k, _vars, false);   // (an inequality's edge: its region shaded already, under it)
        } else if(_f[_k].kind == FZL_EXPLICIT) {
            fzl_trace(&_t, _x0, _x1);
        } else {
            fzl_trace(&_t, _r->t0, _r->t1);
        }
    }
    const f64 _lh = fude_zoom_plot_label_height(_id, _hw, _hh);
    // (the legend's room, top left: no point's numbers over it)
    {
        u32 _rows = _nf + (_r->area_set && _first_fn != FZL_NONE ? 1u : 0u);
        f64 _wide = _r->area_set ? fzl_text_w("x[0.000, 0.000] = 0.000", _lh) : 0.0;
        for(u32 _k = 0; _k < _nf; _k++) {
            _wide = fmax(_wide, fzl_text_w(_r->names[_k], _lh));
        }
        for(u32 _k = 0; _k < 26u; _k++) {
            if((_r->set & (1u << _k)) && _k != (u32)('x' - 'a')) {
                _rows++;
                _wide = fmax(_wide, fzl_text_w("a = 0.000", _lh));
            }
        }
        if(_rows > 0u) {
            const f64 _legend[4] = { -_bx + _lh * 0.3, _by - _lh * (0.9 + 1.3 * (f64)(_rows - 1u)) - _lh * 0.7, -_bx + _lh * 0.9 + _wide, _by - _lh * 0.2 };
            memcpy(_t.taken[_t.ntaken++], _legend, sizeof(_legend));
        }
    }
    // What it is asked to mark on its functions: where each crosses the x axis, turns, crosses another.
    for(u32 _k = 0; _k < _nf && _r->marks != 0u; _k++) {
        if(_f[_k].kind != FZL_EXPLICIT) {
            continue;
        }
        f64 _at[FZL_MARKS];
        if(_r->marks & FZL_ROOTS) {
            const u32 _n = fzl_roots(&_f[_k], NULL, _vars, _x0, _x1, _y1 - _y0, _at, FZL_MARKS);
            for(u32 _i = 0; _i < _n; _i++) {
                if(0.0 >= _y0 && 0.0 <= _y1) {
                    fzl_mark(&_t, _labels, _k, (fude_zoom_v2){ _at[_i], 0.0 }, _lh);
                }
            }
        }
        if(_r->marks & FZL_EXTREMA) {
            const u32 _n = fzl_extrema(&_f[_k], _vars, _x0, _x1, _y1 - _y0, _at, FZL_MARKS);
            for(u32 _i = 0; _i < _n; _i++) {
                const f64 _y = fzl_y(&_f[_k], _at[_i], _vars);
                if(isfinite(_y) && _y >= _y0 && _y <= _y1) {
                    fzl_mark(&_t, _labels, _k, (fude_zoom_v2){ _at[_i], _y }, _lh);
                }
            }
        }
        for(u32 _l = _k + 1u; _l < _nf && (_r->marks & FZL_CROSSINGS); _l++) {
            if(_f[_l].kind != FZL_EXPLICIT) {
                continue;
            }
            const u32 _n = fzl_roots(&_f[_k], &_f[_l], _vars, _x0, _x1, _y1 - _y0, _at, FZL_MARKS);
            for(u32 _i = 0; _i < _n; _i++) {
                const f64 _y = fzl_y(&_f[_k], _at[_i], _vars);
                if(isfinite(_y) && _y >= _y0 && _y <= _y1) {
                    fzl_mark(&_t, _labels, _k, (fude_zoom_v2){ _at[_i], _y }, _lh);
                }
            }
        }
    }
    // Where its points are marked (x = …): a line up through it, a dot on each function there, its numbers by it.
    const f64 _xm = _vars['x' - 'a'];
    if(isfinite(_xm) && _xm >= _x0 && _xm <= _x1) {
        fzl_line(_points, _parts, (fude_zoom_v2){ FZL_X(_xm), -_by }, (fude_zoom_v2){ FZL_X(_xm), _by }, FUDE_ZOOM_SYMBOL_DASHED);
        for(u32 _k = 0; _k < _nf; _k++) {
            if(_f[_k].kind != FZL_EXPLICIT) {
                continue;
            }
            const f64 _ym = fzl_y(&_f[_k], _xm, _vars);
            if(!isfinite(_ym) || _ym < _y0 || _ym > _y1) {
                continue;
            }
            fzl_mark(&_t, _labels, _k, (fude_zoom_v2){ _xm, _ym }, _lh);
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
        fzl_label_by(&_t, _labels, _c, _q, 0u, _lh);
    }
    // The legend: each curve as written, top left, a line each; the area asked for; then each variable's value now.
    u32 _row = 0;
    for(u32 _k = 0; _k < _nf && _labels != NULL; _k++, _row++) {
        fude_zoom_plot_label _lb = { { -_bx + _lh * 0.6, _by - _lh * (0.9 + 1.3 * (f64)_row) }, { 0 }, 4u, (u8)(1u + _k), { 0 } };
        snprintf(_lb.text, sizeof(_lb.text), "%s", _r->names[_k]);
        fzl_latex(&_f[_k], _r->names[_k], _lb.latex, sizeof(_lb.latex));
        rde_arr_add(_labels, (any)&_lb);
    }
    if(_labels != NULL && _r->area_set && _first_fn != FZL_NONE) {
        fude_zoom_plot_label _lb = { { -_bx + _lh * 0.6, _by - _lh * (0.9 + 1.3 * (f64)_row) }, { 0 }, 4u, (u8)(1u + _first_fn) };
        c8 _a[24], _b[24], _v[24];
        fzl_short(_r->area0, _a, sizeof(_a));
        fzl_short(_r->area1, _b, sizeof(_b));
        if(isfinite(_area)) {
            snprintf(_v, sizeof(_v), "%.4g", fabs(_area) < 1e-12 ? 0.0 : _area);
        } else {
            snprintf(_v, sizeof(_v), "?");
        }
        snprintf(_lb.text, sizeof(_lb.text), "\xE2\x88\xAB[%s, %s] = %s", _a, _b, _v);   // ∫
        snprintf(_lb.latex, sizeof(_lb.latex), "\\int_{%s}^{%s} y \\, dx = %s", _a, _b, _v);
        rde_arr_add(_labels, (any)&_lb);
        _row++;
    }
    for(u32 _k = 0; _k < 26u && _labels != NULL; _k++) {
        if(!(_r->set & (1u << _k)) || _k == (u32)('x' - 'a')) {
            continue;
        }
        fude_zoom_plot_label _lb = { { -_bx + _lh * 0.6, _by - _lh * (0.9 + 1.3 * (f64)_row) }, { 0 }, 4u, 0u };
        c8 _a[24];
        fzl_short(_vars[_k], _a, sizeof(_a));
        snprintf(_lb.text, sizeof(_lb.text), "%c = %s", (c8)('a' + _k), _a);
        snprintf(_lb.latex, sizeof(_lb.latex), "%c = %s", (c8)('a' + _k), _a);
        rde_arr_add(_labels, (any)&_lb);
        _row++;
    }
    free(_r);
    #undef FZL_X
    #undef FZL_Y
    return _nf;
}
