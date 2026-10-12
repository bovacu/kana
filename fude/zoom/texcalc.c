// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/texcalc.h"
#include "zoom/calc.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See texcalc.h.
// ===========================================================================

typedef struct {
    const c8* p;
    const c8* end;
    c8*       out;
    usize     size;
    usize     n;
    b8        ok;
    u32       depth;   // (brackets within brackets: a guard)
} fztc;

RDE_INTERNAL void fztc_put(fztc* _t, const c8* _s) {
    const usize _l = strlen(_s);
    if(_t->n + _l + 1u > _t->size) {
        _t->ok = false;
        return;
    }
    memcpy(_t->out + _t->n, _s, _l);
    _t->n += _l;
    _t->out[_t->n] = 0;
}

RDE_INTERNAL void fztc_space(fztc* _t) {
    while(_t->p < _t->end && (*_t->p == ' ' || *_t->p == '\t' || *_t->p == '\n' || *_t->p == '\r')) {
        _t->p++;
    }
}

RDE_INTERNAL b8 fztc_letter(c8 _c) {
    return (_c >= 'a' && _c <= 'z') || (_c >= 'A' && _c <= 'Z');
}

RDE_INTERNAL void fztc_seq(fztc* _t, c8 _close);
RDE_INTERNAL void fztc_command(fztc* _t);

// An argument: {…} in brackets, or one character, or one command — always in brackets.
RDE_INTERNAL void fztc_arg(fztc* _t) {
    fztc_space(_t);
    if(_t->p >= _t->end) {
        _t->ok = false;
        return;
    }
    fztc_put(_t, "(");
    if(*_t->p == '{') {
        _t->p++;
        fztc_seq(_t, '}');
        if(_t->p >= _t->end || *_t->p != '}') {
            _t->ok = false;
            return;
        }
        _t->p++;
    } else if(*_t->p == '\\') {
        fztc_command(_t);
    } else {
        // (one character: its bytes, a UTF-8 one whole)
        c8 _one[8] = { 0 };
        usize _k = 0;
        do {
            _one[_k++] = *_t->p++;
        } while(_k < 4u && _t->p < _t->end && ((u8)*_t->p & 0xC0u) == 0x80u);
        fztc_put(_t, _one);
    }
    fztc_put(_t, ")");
}

// A command, its backslash next.
RDE_INTERNAL void fztc_command(fztc* _t) {
    _t->p++;   // (the backslash)
    if(_t->p >= _t->end) {
        _t->ok = false;
        return;
    }
    c8 _name[24] = { 0 };
    usize _k = 0;
    if(fztc_letter(*_t->p)) {
        while(_t->p < _t->end && fztc_letter(*_t->p)) {
            if(_k + 1u < sizeof(_name)) {
                _name[_k++] = *_t->p;
            }
            _t->p++;
        }
    } else {
        _name[_k++] = *_t->p++;
    }
    static const struct { const c8* tex; const c8* calc; } _same[] = {
        { "cdot", "*" }, { "times", "*" }, { "ast", "*" }, { "div", "/" }, { "pi", "pi" }, { "theta", "\xCE\xB8" },
        { "sin", "sin " }, { "cos", "cos " }, { "tan", "tan " }, { "sinh", "sinh " }, { "cosh", "cosh " }, { "tanh", "tanh " },
        { "arcsin", "asin " }, { "arccos", "acos " }, { "arctan", "atan " }, { "ln", "ln " }, { "log", "log " }, { "exp", "exp " },
        { ",", " " }, { ";", " " }, { ":", " " }, { "!", " " }, { " ", " " }, { "quad", " " }, { "qquad", " " },
        { "{", "(" }, { "}", ")" }, { "|", "|" }, { "vert", "|" }, { "lvert", "|" }, { "rvert", "|" },
        { "le", "<=" }, { "leq", "<=" }, { "ge", ">=" }, { "geq", ">=" }, { "lt", "<" }, { "gt", ">" },
        { "displaystyle", "" }, { "textstyle", "" },
    };
    if(strcmp(_name, "left") == 0 || strcmp(_name, "right") == 0 || strncmp(_name, "big", 3) == 0 || strncmp(_name, "Big", 3) == 0) {
        fztc_space(_t);
        if(_t->p < _t->end && *_t->p == '.') {
            _t->p++;   // (no delimiter; any other comes next, as it is)
        }
        return;
    }
    for(u32 _i = 0; _i < sizeof(_same) / sizeof(_same[0]); _i++) {
        if(strcmp(_name, _same[_i].tex) == 0) {
            fztc_put(_t, _same[_i].calc);
            return;
        }
    }
    if(strcmp(_name, "frac") == 0 || strcmp(_name, "dfrac") == 0 || strcmp(_name, "tfrac") == 0 || strcmp(_name, "cfrac") == 0) {
        fztc_put(_t, "(");
        fztc_arg(_t);
        fztc_put(_t, "/");
        fztc_arg(_t);
        fztc_put(_t, ")");
        return;
    }
    if(strcmp(_name, "sqrt") == 0) {
        fztc_space(_t);
        if(_t->p < _t->end && *_t->p == '[') {
            // (its degree first, written after: (a)^(1/(n)))
            c8 _degree[128] = { 0 };
            fztc _d = { _t->p + 1, _t->end, _degree, sizeof(_degree), 0u, true, _t->depth };
            fztc_seq(&_d, ']');
            if(!_d.ok || _d.p >= _t->end || *_d.p != ']') {
                _t->ok = false;
                return;
            }
            _t->p = _d.p + 1;
            fztc_put(_t, "(");
            fztc_arg(_t);
            fztc_put(_t, "^(1/(");
            fztc_put(_t, _degree);
            fztc_put(_t, ")))");
            return;
        }
        fztc_put(_t, "sqrt");
        fztc_arg(_t);
        return;
    }
    if(strcmp(_name, "operatorname") == 0 || strcmp(_name, "mathrm") == 0 || strcmp(_name, "text") == 0 || strcmp(_name, "mathit") == 0 ||
       strcmp(_name, "mathbf") == 0) {
        fztc_arg(_t);   // (abs, sign, floor…: calc's when it knows them)
        return;
    }
    _t->ok = false;   // (\infty, \sum, \alpha…: not calc's)
}

// Up to _close (0: the end), not taking it.
RDE_INTERNAL void fztc_seq(fztc* _t, c8 _close) {
    if(++_t->depth > 64u) {
        _t->ok = false;
        return;
    }
    while(_t->ok && _t->p < _t->end) {
        const c8 _c = *_t->p;
        if(_c == _close && _close != 0) {
            break;
        }
        switch(_c) {
        case '{':
            _t->p++;
            fztc_put(_t, "(");
            fztc_seq(_t, '}');
            if(_t->p >= _t->end || *_t->p != '}') {
                _t->ok = false;
                break;
            }
            _t->p++;
            fztc_put(_t, ")");
            break;
        case '}': case '_': case '&': case '!': case '#': case '$': case '\'':
            _t->ok = false;   // (subscripts, columns, factorials, primes: not calc's)
            break;
        case '\\':
            if(_t->p + 1 < _t->end && _t->p[1] == '\\') {
                _t->ok = false;   // (a line break: two formulas)
                break;
            }
            fztc_command(_t);
            break;
        case '^':
            _t->p++;
            fztc_put(_t, "^");
            fztc_arg(_t);
            break;
        case '[': _t->p++; fztc_put(_t, "("); break;
        case ']': _t->p++; fztc_put(_t, ")"); break;
        case '~': _t->p++; fztc_put(_t, " "); break;
        default: {
            const c8 _one[2] = { _c, 0 };
            _t->p++;
            fztc_put(_t, _one);
            break;
        }
        }
    }
    _t->depth--;
}

b8 fude_zoom_texcalc(const c8* _latex, u32 _len, c8* _out, usize _size) {
    if(_size == 0) {
        return false;
    }
    _out[0] = 0;
    fztc _t = { _latex, _latex + _len, _out, _size, 0u, true, 0u };
    fztc_seq(&_t, 0);
    return _t.ok && _t.p >= _t.end;
}

b8 fude_zoom_texcalc_tidy(const c8* _latex, u32 _len, c8* _out, usize _size) {
    c8 _calc[512];
    if(_size == 0 || !fude_zoom_texcalc(_latex, _len, _calc, sizeof(_calc))) {
        return false;
    }
    // (its relation, outside brackets)
    usize _at = 0, _rl = 0;
    i32 _depth = 0;
    for(usize _i = 0; _calc[_i] != 0 && _rl == 0; _i++) {
        const c8 _c = _calc[_i];
        _depth += _c == '(' ? 1 : (_c == ')' ? -1 : 0);
        if(_depth == 0 && (_c == '=' || _c == '<' || _c == '>')) {
            _at = _i;
            _rl = (_c != '=' && _calc[_i + 1u] == '=') ? 2u : 1u;
        }
    }
    c8 _rel[3] = { 0 };
    c8 _left[512], _right[512];
    if(_rl > 0) {
        memcpy(_rel, _calc + _at, _rl);
        _calc[_at] = 0;
    }
    fude_zoom_calc _c;
    if(!fude_zoom_calc_parse_expr(&_c, _calc) || !fude_zoom_calc_write(&_c, _left, sizeof(_left))) {
        return false;
    }
    if(_rl == 0) {
        return (usize)snprintf(_out, _size, "%s", _left) < _size;
    }
    if(!fude_zoom_calc_parse_expr(&_c, _calc + _at + _rl) || !fude_zoom_calc_write(&_c, _right, sizeof(_right))) {
        return false;
    }
    return (usize)snprintf(_out, _size, "%s %s %s", _left, _rel, _right) < _size;
}

b8 fude_zoom_texcalc_answer(const c8* _latex, u32 _len, c8* _out, usize _size) {
    while(_len > 0 && (_latex[_len - 1u] == ' ' || _latex[_len - 1u] == '\n' || _latex[_len - 1u] == '\t' || _latex[_len - 1u] == '\r')) {
        _len--;
    }
    if(_len < 2u || _latex[_len - 1u] != '=') {
        return false;
    }
    _len--;
    if(memchr(_latex, '=', _len) != NULL || memchr(_latex, '<', _len) != NULL || memchr(_latex, '>', _len) != NULL) {
        return false;   // (an equation already, not a sum to work out)
    }
    c8 _calc[512];
    f64 _v = 0.0;
    if(!fude_zoom_texcalc(_latex, _len, _calc, sizeof(_calc)) || !fude_zoom_calc_number(_calc, &_v)) {
        return false;
    }
    if(fabs(_v) < 1e-13) {
        _v = 0.0;   // (sin(π)'s 1.2·10⁻¹⁶: nothing)
    }
    snprintf(_out, _size, "%.10g", _v);
    return true;
}
