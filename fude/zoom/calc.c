// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/calc.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    FZK_NUM = 1, FZK_X, FZK_ADD, FZK_SUB, FZK_MUL, FZK_DIV, FZK_POW, FZK_MOD, FZK_NEG,
    FZK_SIN, FZK_COS, FZK_TAN, FZK_ASIN, FZK_ACOS, FZK_ATAN, FZK_SINH, FZK_COSH, FZK_TANH, FZK_SQRT, FZK_CBRT, FZK_ABS, FZK_EXP,
    FZK_LN, FZK_LOG, FZK_LOG2, FZK_FLOOR, FZK_CEIL, FZK_ROUND, FZK_SIGN, FZK_VAR
} FZK_;

typedef struct {
    const c8*       p;
    fude_zoom_calc* c;
    b8              ok;
} fzk;

RDE_INTERNAL void fzk_emit(fzk* _k, u8 _op, f64 _v) {
    if(_k->c->count >= FUDE_ZOOM_CALC_STEPS) {
        _k->ok = false;
        return;
    }
    _k->c->op[_k->c->count]    = _op;
    _k->c->value[_k->c->count] = _v;
    _k->c->count++;
}

RDE_INTERNAL void fzk_space(fzk* _k) {
    while(*_k->p == ' ' || *_k->p == '\t') {
        _k->p++;
    }
}

// Is what comes next _s (taken if so)?
RDE_INTERNAL b8 fzk_take(fzk* _k, const c8* _s) {
    fzk_space(_k);
    const usize _n = strlen(_s);
    if(strncmp(_k->p, _s, _n) == 0) {
        _k->p += _n;
        return true;
    }
    return false;
}

RDE_INTERNAL void fzk_expr(fzk* _k);
RDE_INTERNAL void fzk_unary(fzk* _k);

// A function's name (the longest first) → its step; 0: none.
RDE_INTERNAL u8 fzk_function(fzk* _k) {
    static const struct { const c8* name; u8 op; } _f[] = {
        { "asin", FZK_ASIN }, { "acos", FZK_ACOS }, { "atan", FZK_ATAN }, { "sinh", FZK_SINH }, { "cosh", FZK_COSH }, { "tanh", FZK_TANH },
        { "sqrt", FZK_SQRT }, { "cbrt", FZK_CBRT }, { "log2", FZK_LOG2 }, { "floor", FZK_FLOOR }, { "ceil", FZK_CEIL }, { "round", FZK_ROUND },
        { "sign", FZK_SIGN }, { "sin", FZK_SIN }, { "cos", FZK_COS }, { "tan", FZK_TAN }, { "abs", FZK_ABS }, { "exp", FZK_EXP }, { "ln", FZK_LN },
        { "log", FZK_LOG }, { "sen", FZK_SIN }, { "tg", FZK_TAN }, { "\xE2\x88\x9A", FZK_SQRT },
    };
    fzk_space(_k);
    for(u32 _i = 0; _i < sizeof(_f) / sizeof(_f[0]); _i++) {
        const usize _n = strlen(_f[_i].name);
        if(strncmp(_k->p, _f[_i].name, _n) == 0) {
            _k->p += _n;
            return _f[_i].op;
        }
    }
    return 0u;
}

// Does a factor start here (a number before it multiplies it: 2x, 3(x+1), 2sin(x))?
RDE_INTERNAL b8 fzk_starts(fzk* _k) {
    fzk_space(_k);
    const c8 _c = *_k->p;
    if(_c == 'x' || _c == 't' || _c == '(' || _c == 'e' || (_c >= 'a' && _c <= 'z')) {
        return true;
    }
    return strncmp(_k->p, "\xCF\x80", 2) == 0 || strncmp(_k->p, "\xCE\xB8", 2) == 0 || strncmp(_k->p, "\xE2\x88\x9A", 3) == 0;   // π θ √
}

RDE_INTERNAL void fzk_primary(fzk* _k) {
    fzk_space(_k);
    const c8* _p = _k->p;
    if((*_p >= '0' && *_p <= '9') || (*_p == '.' && _p[1] >= '0' && _p[1] <= '9')) {
        c8* _end = NULL;
        const f64 _v = strtod(_p, &_end);
        _k->p = _end;
        fzk_emit(_k, FZK_NUM, _v);
        return;
    }
    if(fzk_take(_k, "(")) {
        fzk_expr(_k);
        if(!fzk_take(_k, ")")) {
            _k->ok = false;
        }
        return;
    }
    if(fzk_take(_k, "|")) {
        fzk_expr(_k);
        if(!fzk_take(_k, "|")) {
            _k->ok = false;
        }
        fzk_emit(_k, FZK_ABS, 0.0);
        return;
    }
    if(fzk_take(_k, "pi") || fzk_take(_k, "\xCF\x80")) {
        fzk_emit(_k, FZK_NUM, 3.14159265358979323846);
        return;
    }
    const u8 _f = fzk_function(_k);
    if(_f != 0u) {
        fzk_unary(_k);   // (sin(x), sin x, √x)
        fzk_emit(_k, _f, 0.0);
        return;
    }
    if(fzk_take(_k, "x") || fzk_take(_k, "t") || fzk_take(_k, "\xCE\xB8")) {
        fzk_emit(_k, FZK_X, 0.0);
        return;
    }
    if(fzk_take(_k, "e")) {
        fzk_emit(_k, FZK_NUM, 2.71828182845904523536);
        return;
    }
    // Any other letter: a variable (its value given when it is worked out).
    if(*_k->p >= 'a' && *_k->p <= 'z') {
        const u32 _letter = (u32)(*_k->p - 'a');
        _k->p++;
        _k->c->vars |= 1u << _letter;
        fzk_emit(_k, FZK_VAR, (f64)_letter);
        return;
    }
    _k->ok = false;
}

RDE_INTERNAL void fzk_power(fzk* _k) {
    fzk_primary(_k);
    if(fzk_take(_k, "^") || fzk_take(_k, "**")) {
        fzk_unary(_k);   // (right to left: 2^3^2 is 2^9; e^-x)
        fzk_emit(_k, FZK_POW, 0.0);
    }
}

RDE_INTERNAL void fzk_unary(fzk* _k) {
    if(fzk_take(_k, "-") || fzk_take(_k, "\xE2\x88\x92")) {
        fzk_unary(_k);
        fzk_emit(_k, FZK_NEG, 0.0);
        return;
    }
    if(fzk_take(_k, "+")) {
        fzk_unary(_k);
        return;
    }
    fzk_power(_k);
}

RDE_INTERNAL void fzk_term(fzk* _k) {
    fzk_unary(_k);
    for(u32 _guard = 0; _k->ok && _guard < 256u; _guard++) {
        if(fzk_take(_k, "*") || fzk_take(_k, "\xC3\x97") || fzk_take(_k, "\xC2\xB7")) {
            fzk_unary(_k);
            fzk_emit(_k, FZK_MUL, 0.0);
        } else if(fzk_take(_k, "/") || fzk_take(_k, "\xC3\xB7")) {
            fzk_unary(_k);
            fzk_emit(_k, FZK_DIV, 0.0);
        } else if(fzk_take(_k, "%")) {
            fzk_unary(_k);
            fzk_emit(_k, FZK_MOD, 0.0);
        } else if(fzk_starts(_k)) {
            fzk_power(_k);   // (side by side: multiplied)
            fzk_emit(_k, FZK_MUL, 0.0);
        } else {
            break;
        }
    }
}

RDE_INTERNAL void fzk_expr(fzk* _k) {
    fzk_term(_k);
    for(u32 _guard = 0; _k->ok && _guard < 256u; _guard++) {
        if(fzk_take(_k, "+")) {
            fzk_term(_k);
            fzk_emit(_k, FZK_ADD, 0.0);
        } else if(fzk_take(_k, "-") || fzk_take(_k, "\xE2\x88\x92")) {
            fzk_term(_k);
            fzk_emit(_k, FZK_SUB, 0.0);
        } else {
            break;
        }
    }
}

b8 fude_zoom_calc_parse(fude_zoom_calc* _c, const c8* _text) {
    memset(_c, 0, sizeof(*_c));
    if(_text == NULL) {
        return false;
    }
    // Its first line; what is after an "=" (y = …, f(x) = …).
    c8 _line[256];
    usize _n = strcspn(_text, "\n");
    _n = _n < sizeof(_line) - 1u ? _n : sizeof(_line) - 1u;
    memcpy(_line, _text, _n);
    _line[_n] = 0;
    const c8* _eq = strchr(_line, '=');
    c8 _low[256];
    const c8* _from = _eq != NULL ? _eq + 1 : _line;
    usize _m = 0;
    for(; _from[_m] != 0 && _m + 1u < sizeof(_low); _m++) {
        const c8 _ch = _from[_m];
        _low[_m] = (_ch >= 'A' && _ch <= 'Z') ? (c8)(_ch + ('a' - 'A')) : _ch;   // (Sin, X: as sin, x)
    }
    _low[_m] = 0;
    fzk _k = { _low, _c, true };
    fzk_expr(&_k);
    fzk_space(&_k);
    if(!_k.ok || *_k.p != 0 || _c->count == 0u) {
        memset(_c, 0, sizeof(*_c));
        return false;
    }
    return true;
}

f64 fude_zoom_calc_at(const fude_zoom_calc* _c, f64 _x) {
    return fude_zoom_calc_at_with(_c, _x, NULL);
}

f64 fude_zoom_calc_at_with(const fude_zoom_calc* _c, f64 _x, const f64* _vars) {
    f64 _s[FUDE_ZOOM_CALC_STEPS];
    u32 _n = 0;
    for(u32 _i = 0; _i < _c->count; _i++) {
        const u8 _op = _c->op[_i];
        if(_op == FZK_NUM || _op == FZK_X || _op == FZK_VAR) {
            _s[_n++] = _op == FZK_NUM ? _c->value[_i] : (_op == FZK_X ? _x : (_vars != NULL ? _vars[(u32)_c->value[_i] % 26u] : NAN));
            continue;
        }
        if(_op >= FZK_ADD && _op <= FZK_MOD) {
            if(_n < 2u) {
                return NAN;
            }
            const f64 _b = _s[--_n], _a = _s[_n - 1u];
            f64 _r = 0.0;
            switch(_op) {
            case FZK_ADD: _r = _a + _b; break;
            case FZK_SUB: _r = _a - _b; break;
            case FZK_MUL: _r = _a * _b; break;
            case FZK_DIV: _r = _b != 0.0 ? _a / _b : NAN; break;
            case FZK_POW: _r = pow(_a, _b); break;
            default:      _r = _b != 0.0 ? fmod(_a, _b) : NAN; break;
            }
            _s[_n - 1u] = _r;
            continue;
        }
        if(_n < 1u) {
            return NAN;
        }
        const f64 _a = _s[_n - 1u];
        f64 _r;
        switch(_op) {
        case FZK_NEG:   _r = -_a; break;
        case FZK_SIN:   _r = sin(_a); break;
        case FZK_COS:   _r = cos(_a); break;
        case FZK_TAN:   _r = tan(_a); break;
        case FZK_ASIN:  _r = asin(_a); break;
        case FZK_ACOS:  _r = acos(_a); break;
        case FZK_ATAN:  _r = atan(_a); break;
        case FZK_SINH:  _r = sinh(_a); break;
        case FZK_COSH:  _r = cosh(_a); break;
        case FZK_TANH:  _r = tanh(_a); break;
        case FZK_SQRT:  _r = _a >= 0.0 ? sqrt(_a) : NAN; break;
        case FZK_CBRT:  _r = cbrt(_a); break;
        case FZK_ABS:   _r = fabs(_a); break;
        case FZK_EXP:   _r = exp(_a); break;
        case FZK_LN:    _r = _a > 0.0 ? log(_a) : NAN; break;
        case FZK_LOG:   _r = _a > 0.0 ? log10(_a) : NAN; break;
        case FZK_LOG2:  _r = _a > 0.0 ? log2(_a) : NAN; break;
        case FZK_FLOOR: _r = floor(_a); break;
        case FZK_CEIL:  _r = ceil(_a); break;
        case FZK_ROUND: _r = round(_a); break;
        default:        _r = _a > 0.0 ? 1.0 : (_a < 0.0 ? -1.0 : 0.0); break;
        }
        _s[_n - 1u] = _r;
    }
    return _n == 1u ? _s[0] : NAN;
}
