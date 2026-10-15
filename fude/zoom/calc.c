// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/calc.h"

// ===========================================================================
// See calc.h.
//
// Internal names start fzk_ (FZK_ for the steps). The file has three parts: the parser, a recursive descent that emits
// the steps in postfix order as it goes (fzk_expr, a sum, down to fzk_primary); the writers, which run the steps on a
// stack of text instead of numbers (fude_zoom_calc_write, fude_zoom_calc_latex); and the evaluator, which runs them on
// a stack of numbers (fude_zoom_calc_at_with). The grammar, loosest first:
//
//   expr    := term { (+ | -) term }
//   term    := unary { (* | / | %) unary | power }     (a power right after a factor is multiplied: 2x, 3(x+1))
//   unary   := (- | +) unary | power
//   power   := primary [ (^ | **) unary ]               (so 2^3^2 is 2^9, and e^-x reads)
//   primary := number | ( expr ) | |expr| | pi | function unary | x | t | θ | e | another letter
// ===========================================================================

// A step's operation (fude_zoom_calc's op; 0 is none). NUM pushes its value, X pushes x, and VAR the variable whose
// letter is its value (0 for a). ADD to MOD take two numbers and push one: they are kept together, in this order,
// because the code tests that range. NEG and the functions take one.
typedef enum {
    FZK_NUM = 1, FZK_X, FZK_ADD, FZK_SUB, FZK_MUL, FZK_DIV, FZK_POW, FZK_MOD, FZK_NEG,
    FZK_SIN, FZK_COS, FZK_TAN, FZK_ASIN, FZK_ACOS, FZK_ATAN, FZK_SINH, FZK_COSH, FZK_TANH, FZK_SQRT, FZK_CBRT, FZK_ABS, FZK_EXP,
    FZK_LN, FZK_LOG, FZK_LOG2, FZK_FLOOR, FZK_CEIL, FZK_ROUND, FZK_SIGN, FZK_VAR
} FZK_;

// The parser's state: where it is in the text, the function it fills, and false once anything has failed.
typedef struct {
    const c8*       p;
    fude_zoom_calc* c;
    b8              ok;
} fzk;

// Appends a step to the function; one past FUDE_ZOOM_CALC_STEPS fails the parse.
RDE_INTERNAL void fzk_emit(fzk* _k, u8 _op, f64 _v) {
    if(_k->c->count >= FUDE_ZOOM_CALC_STEPS) {
        _k->ok = false;
        return;
    }
    _k->c->op[_k->c->count]    = _op;
    _k->c->value[_k->c->count] = _v;
    _k->c->count++;
}

// Skips spaces and tabs.
RDE_INTERNAL void fzk_space(fzk* _k) {
    while(*_k->p == ' ' || *_k->p == '\t') {
        _k->p++;
    }
}

// Skips spaces; when the text goes on with _s, takes it and returns true.
RDE_INTERNAL b8 fzk_take(fzk* _k, const c8* _s) {
    fzk_space(_k);
    const usize _n = rde_strlen(_s);
    if(rde_strncmp(_k->p, _s, _n) == 0) {
        _k->p += _n;
        return true;
    }
    return false;
}

// The grammar's rules call each other; these two are called before they are defined.
RDE_INTERNAL void fzk_expr(fzk* _k);
RDE_INTERNAL void fzk_unary(fzk* _k);

// Takes a function's name when one comes next, and returns its step; 0 when none does. A name is listed before the
// shorter ones it starts with (asin and sinh before sin, log2 before log), so the longest is taken.
RDE_INTERNAL u8 fzk_function(fzk* _k) {
    static const struct { const c8* name; u8 op; } _f[] = {
        { "asin", FZK_ASIN }, { "acos", FZK_ACOS }, { "atan", FZK_ATAN }, { "sinh", FZK_SINH }, { "cosh", FZK_COSH }, { "tanh", FZK_TANH },
        { "sqrt", FZK_SQRT }, { "cbrt", FZK_CBRT }, { "log2", FZK_LOG2 }, { "floor", FZK_FLOOR }, { "ceil", FZK_CEIL }, { "round", FZK_ROUND },
        { "sign", FZK_SIGN }, { "sin", FZK_SIN }, { "cos", FZK_COS }, { "tan", FZK_TAN }, { "abs", FZK_ABS }, { "exp", FZK_EXP }, { "ln", FZK_LN },
        { "log", FZK_LOG }, { "sen", FZK_SIN }, { "tg", FZK_TAN }, { "\xE2\x88\x9A", FZK_SQRT },
    };
    fzk_space(_k);
    for(u32 _i = 0; _i < sizeof(_f) / sizeof(_f[0]); _i++) {
        const usize _n = rde_strlen(_f[_i].name);
        if(rde_strncmp(_k->p, _f[_i].name, _n) == 0) {
            _k->p += _n;
            return _f[_i].op;
        }
    }
    return 0u;
}

// Returns whether a factor starts here that a factor before it multiplies with nothing between (2x, 3(x+1),
// 2sin(x)): a letter, an opening bracket, π, θ or √. A | does not, so 2|x| does not parse.
RDE_INTERNAL b8 fzk_starts(fzk* _k) {
    fzk_space(_k);
    const c8 _c = *_k->p;
    if(_c == 'x' || _c == 't' || _c == '(' || _c == 'e' || (_c >= 'a' && _c <= 'z')) {
        return true;
    }
    return rde_strncmp(_k->p, "\xCF\x80", 2) == 0 || rde_strncmp(_k->p, "\xCE\xB8", 2) == 0 || rde_strncmp(_k->p, "\xE2\x88\x9A", 3) == 0;   // π θ √
}

// Parses a primary: a number, a bracket, |…| (a size), pi or π, a function and what it applies to, x (or t, θ), e,
// or any other letter as a variable. Function names are tried before x, t and e, so exp and tan are not read as
// e·x·p and t·a·n.
RDE_INTERNAL void fzk_primary(fzk* _k) {
    fzk_space(_k);
    const c8* _p = _k->p;
    if((*_p >= '0' && *_p <= '9') || (*_p == '.' && _p[1] >= '0' && _p[1] <= '9')) {
        // (decimal only: "0x2" is 0 times x times 2, not hex)
        f64 _v = 0.0;
        const usize _len = fude_zoom_calc_decimal(_p, false, &_v);
        if(_len == 0u) {
            _k->ok = false;
            return;
        }
        _k->p = _p + _len;
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
        fzk_emit(_k, FZK_NUM, RDE_PI);
        return;
    }
    const u8 _f = fzk_function(_k);
    if(_f != 0u) {
        fzk_unary(_k);   // (sin(x), sin x, √x)
        fzk_emit(_k, _f, 0.0);
        return;
    }
    if(fzk_take(_k, "x") || fzk_take(_k, "t") || fzk_take(_k, "\xCE\xB8")) {
        _k->c->uses_x = true;
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

// Parses a primary and, after ^ or **, its power: a unary, so the power groups right to left and may have a sign.
RDE_INTERNAL void fzk_power(fzk* _k) {
    fzk_primary(_k);
    if(fzk_take(_k, "^") || fzk_take(_k, "**")) {
        fzk_unary(_k);   // (right to left: 2^3^2 is 2^9; e^-x)
        fzk_emit(_k, FZK_POW, 0.0);
    }
}

// Parses a power with any signs before it: each - (or −) negates it, + is skipped.
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

// Parses a product: unaries joined by * × · / ÷ or %, and by nothing when a factor follows (fzk_starts). It stops
// after 256 operators; the text left over then fails the parse.
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

// Parses a sum: terms joined by + and - (or −), with the same limit of 256.
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

// Parses the first line of _text (its first 255 bytes) into _c: only what is after its first "=" unless _whole, and
// with capitals made small first. Fails, and leaves _c zeroed, when text is left over, a step did not fit, or there
// was nothing to read.
RDE_INTERNAL b8 fzk_parse(fude_zoom_calc* _c, const c8* _text, b8 _whole) {
    rde_memset(_c, 0, sizeof(*_c));
    if(_text == NULL) {
        return false;
    }
    // Its first line; what is after an "=" (y = …, f(x) = …).
    c8 _line[256];
    usize _n = rde_strcspn(_text, "\n");
    _n = _n < sizeof(_line) - 1u ? _n : sizeof(_line) - 1u;
    rde_memcpy(_line, _text, _n);
    _line[_n] = 0;
    const c8* _eq = _whole ? NULL : rde_strchr(_line, '=');
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
        rde_memset(_c, 0, sizeof(*_c));
        return false;
    }
    return true;
}

// See the header.
b8 fude_zoom_calc_parse(fude_zoom_calc* _c, const c8* _text) {
    return fzk_parse(_c, _text, false);
}

// See the header.
b8 fude_zoom_calc_parse_expr(fude_zoom_calc* _c, const c8* _text) {
    return fzk_parse(_c, _text, true);
}

// See the header. The number's bytes are found here, then copied out and handed to rde_strtod, so it can read no
// further than they go.
usize fude_zoom_calc_decimal(const c8* _s, b8 _signed, f64* _v) {
    const c8* _e = _s;
    if(_signed && (*_e == '+' || *_e == '-')) {
        _e++;
    }
    const c8* _digits = _e;
    while(*_e >= '0' && *_e <= '9') {
        _e++;
    }
    usize _whole = (usize)(_e - _digits);
    if(*_e == '.') {
        const c8* _after = _e + 1;
        while(*_after >= '0' && *_after <= '9') {
            _after++;
        }
        if(_whole > 0u || _after > _e + 1) {   // ("2." and ".5", not "." alone)
            _whole += (usize)(_after - _e - 1);
            _e = _after;
        }
    }
    if(_whole == 0u) {
        return 0u;
    }
    if(*_e == 'e' || *_e == 'E') {
        const c8* _x = _e + 1;
        if(*_x == '+' || *_x == '-') {
            _x++;
        }
        if(*_x >= '0' && *_x <= '9') {
            while(*_x >= '0' && *_x <= '9') {
                _x++;
            }
            _e = _x;
        }
    }
    // Read in place, so a number of any length is read whole. rde_strtod goes past what was found only into C's hex
    // ("0x2"), and then what was found is a lone 0 (with its sign), as any longer number has another digit or a point
    // after its first 0: that 0 is the value.
    c8*       _end  = NULL;
    const f64 _read = rde_strtod(_s, &_end);
    *_v = (const c8*)_end <= _e ? _read : (*_s == '-' ? -0.0 : 0.0);
    return (usize)(_e - _s);
}

// See the header: parsed whole, then worked out at x = 0.
b8 fude_zoom_calc_number(const c8* _text, f64* _value) {
    fude_zoom_calc _c;
    if(!fzk_parse(&_c, _text, true) || _c.uses_x || _c.vars != 0u) {
        return false;
    }
    *_value = fude_zoom_calc_at(&_c, 0.0);
    return rde_math_is_finite_f64(*_value);
}

// --- written back ----------------------------------------------------------------------------------

#define FZK_WRITE 384u   // bytes of text a piece may grow to; a function written longer fails

// A piece of the function written back: the text one value on the stack has become.
typedef struct {
    c8 text[FZK_WRITE];
    u8 order;   // how tightly it holds: 1 a sum, 2 a product, 3 a sign, 4 a power or a call, 5 a number or a letter
    b8 number;  // a plain number (2, 0.5: what a letter after it multiplies with nothing between)
} fzk_piece;

// Writes a piece's text into _out, in brackets when _brackets.
RDE_INTERNAL void fzk_wrap(const fzk_piece* _p, b8 _brackets, c8* _out, usize _size) {
    rde_snprintf(_out, _size, _brackets ? "(%s)" : "%s", _p->text);
}

// See the header. The steps run on a stack of written pieces instead of numbers: each operation joins its operands'
// text, with brackets round one that holds less tightly than it does (a piece's order), and a number followed by a
// letter, a call or a bracket is written beside it with no sign (2x). Fails when a piece outgrows FZK_WRITE or the
// steps do not leave exactly one.
b8 fude_zoom_calc_write(const fude_zoom_calc* _c, c8* _out, usize _size) {
    static const c8* const _names[] = { [FZK_SIN] = "sin", [FZK_COS] = "cos", [FZK_TAN] = "tan", [FZK_ASIN] = "asin", [FZK_ACOS] = "acos",
                                        [FZK_ATAN] = "atan", [FZK_SINH] = "sinh", [FZK_COSH] = "cosh", [FZK_TANH] = "tanh", [FZK_SQRT] = "sqrt",
                                        [FZK_CBRT] = "cbrt", [FZK_ABS] = "abs", [FZK_EXP] = "exp", [FZK_LN] = "ln", [FZK_LOG] = "log",
                                        [FZK_LOG2] = "log2", [FZK_FLOOR] = "floor", [FZK_CEIL] = "ceil", [FZK_ROUND] = "round", [FZK_SIGN] = "sign" };
    if(_size == 0) {
        return false;
    }
    _out[0] = 0;
    rde_arr TYPE(fzk_piece) _pieces = rde_arr_new_with_capacity(sizeof(fzk_piece), FUDE_ZOOM_CALC_STEPS, rde_memory_allocator_get_default_std());
    rde_arr_resize(&_pieces, FUDE_ZOOM_CALC_STEPS);
    fzk_piece* _s = (fzk_piece*)_pieces.memory;
    u32 _n = 0;
    b8 _ok = _c->count > 0u;
    c8 _a[FZK_WRITE], _b[FZK_WRITE];
    for(u32 _i = 0; _i < _c->count && _ok; _i++) {
        const u8 _op = _c->op[_i];
        if(_op == FZK_NUM || _op == FZK_X || _op == FZK_VAR) {
            fzk_piece* _p = &_s[_n++];
            _p->order  = 5u;
            _p->number = _op == FZK_NUM;
            if(_op == FZK_X) {
                rde_snprintf(_p->text, sizeof(_p->text), "x");
            } else if(_op == FZK_VAR) {
                rde_snprintf(_p->text, sizeof(_p->text), "%c", (c8)('a' + (u32)_c->value[_i] % 26u));
            } else if(_c->value[_i] == RDE_PI) {
                rde_snprintf(_p->text, sizeof(_p->text), "pi");
                _p->number = false;
            } else if(_c->value[_i] == 2.71828182845904523536) {
                rde_snprintf(_p->text, sizeof(_p->text), "e");
                _p->number = false;
            } else {
                rde_snprintf(_p->text, sizeof(_p->text), "%.12g", _c->value[_i]);
            }
            continue;
        }
        if(_op >= FZK_ADD && _op <= FZK_MOD) {
            if(_n < 2u) {
                _ok = false;
                break;
            }
            fzk_piece* _l = &_s[_n - 2u];
            const fzk_piece* _r = &_s[_n - 1u];
            const u8 _order = _op == FZK_ADD || _op == FZK_SUB ? 1u : (_op == FZK_POW ? 4u : 2u);
            // (the left in brackets when it holds less tightly, a power's even when as tightly: (a^b)^c; the right
            // too, or when as tightly after − / %, which do not turn round, and a power's unless it is a power, a call,
            // a number or a letter)
            const b8 _lb = _l->order < _order || (_op == FZK_POW && _l->order <= _order);
            const b8 _rb = _r->order < _order || (_r->order == _order && (_op == FZK_SUB || _op == FZK_DIV || _op == FZK_MOD)) ||
                           (_op == FZK_POW && _r->order < 4u);
            fzk_wrap(_l, _lb, _a, sizeof(_a));
            fzk_wrap(_r, _rb, _b, sizeof(_b));
            const c8* _sign = _op == FZK_ADD ? " + " : _op == FZK_SUB ? " - " : _op == FZK_DIV ? "/" : _op == FZK_POW ? "^" : _op == FZK_MOD ? " % " : "*";
            // (a number then a letter, a call or a bracket: side by side — 2x, 3sin(x), 2(x + 1))
            if(_op == FZK_MUL && _l->number && !_lb && (_rb || (_b[0] >= 'a' && _b[0] <= 'z'))) {
                _sign = "";
            }
            const int _w = rde_snprintf(_l->text, sizeof(_l->text), "%s%s%s", _a, _sign, _b);
            _ok = _w > 0 && (usize)_w < sizeof(_l->text);
            _l->order  = _order;
            _l->number = false;
            _n--;
            continue;
        }
        if(_n < 1u) {
            _ok = false;
            break;
        }
        fzk_piece* _p = &_s[_n - 1u];
        if(_op == FZK_NEG) {
            fzk_wrap(_p, _p->order < 3u, _a, sizeof(_a));
            const int _w = rde_snprintf(_p->text, sizeof(_p->text), "-%s", _a);
            _ok = _w > 0 && (usize)_w < sizeof(_p->text);
            _p->order = 3u;
        } else if(_op < sizeof(_names) / sizeof(_names[0]) && _names[_op] != NULL) {
            rde_snprintf(_a, sizeof(_a), "%s", _p->text);
            const int _w = rde_snprintf(_p->text, sizeof(_p->text), "%s(%s)", _names[_op], _a);
            _ok = _w > 0 && (usize)_w < sizeof(_p->text);
            _p->order = 4u;   // (as a power: sin(x)^2 is read as sin(x^2) — under a power it needs its brackets)
        } else {
            _ok = false;
        }
        _p->number = false;
    }
    _ok = _ok && _n == 1u && rde_strlen(_s[0].text) < _size;
    if(_ok) {
        rde_snprintf(_out, _size, "%s", _s[0].text);
    }
    rde_arr_free(&_pieces);
    return _ok;
}

// Writes a number as LaTeX: 0.5, 12, 10^{20}, 1.5 \times 10^{20} (an exponent from %.12g written out).
RDE_INTERNAL void fzk_latex_number(f64 _v, c8* _out, usize _size) {
    c8 _t[48];
    rde_snprintf(_t, sizeof(_t), "%.12g", _v);
    c8* _e = rde_strchr(_t, 'e');
    if(_e == NULL) {
        rde_snprintf(_out, _size, "%s", _t);
        return;
    }
    *_e = 0;
    const int _exp = (i32)rde_strtol(_e + 1, NULL, 10);
    if(rde_strcmp(_t, "1") == 0) {
        rde_snprintf(_out, _size, "10^{%d}", _exp);
    } else {
        rde_snprintf(_out, _size, "%s \\times 10^{%d}", _t, _exp);
    }
}

// See the header. The same stack of pieces as fude_zoom_calc_write, in LaTeX: a division is a \frac (it needs no
// brackets, though one is bracketed as a power's base), a power's exponent goes in braces, and a function's argument
// is in \left(…\right) unless it is a letter or a plain number (\sin x).
b8 fude_zoom_calc_latex(const fude_zoom_calc* _c, const c8* _x, c8* _out, usize _size) {
    if(_size == 0) {
        return false;
    }
    _out[0] = 0;
    // (each piece: its words, how tightly it holds — as calc_write's —, and whether it is a fraction or a plain number)
    typedef struct { c8 text[FZK_WRITE]; u8 order; b8 number, frac; } fzk_tex;
    rde_arr TYPE(fzk_tex) _pieces = rde_arr_new_with_capacity(sizeof(fzk_tex), FUDE_ZOOM_CALC_STEPS, rde_memory_allocator_get_default_std());
    rde_arr_resize(&_pieces, FUDE_ZOOM_CALC_STEPS);
    fzk_tex* _s = (fzk_tex*)_pieces.memory;
    u32 _n = 0;
    b8 _ok = _c->count > 0u;
    c8 _a[FZK_WRITE], _b[FZK_WRITE];
    // Writes piece _p's text into the array _to, in \left(…\right) when _yes.
    #define FZK_BR(_p, _yes, _to) rde_snprintf((_to), sizeof(_to), (_yes) ? "\\left(%s\\right)" : "%s", (_p)->text)
    for(u32 _i = 0; _i < _c->count && _ok; _i++) {
        const u8 _op = _c->op[_i];
        if(_op == FZK_NUM || _op == FZK_X || _op == FZK_VAR) {
            fzk_tex* _p = &_s[_n++];
            _p->order  = 5u;
            _p->number = _op == FZK_NUM;
            _p->frac   = false;
            if(_op == FZK_X) {
                rde_snprintf(_p->text, sizeof(_p->text), "%s", _x);
            } else if(_op == FZK_VAR) {
                rde_snprintf(_p->text, sizeof(_p->text), "%c", (c8)('a' + (u32)_c->value[_i] % 26u));
            } else if(_c->value[_i] == RDE_PI) {
                rde_snprintf(_p->text, sizeof(_p->text), "\\pi");
                _p->number = false;
            } else if(_c->value[_i] == 2.71828182845904523536) {
                rde_snprintf(_p->text, sizeof(_p->text), "e");
                _p->number = false;
            } else {
                fzk_latex_number(_c->value[_i], _p->text, sizeof(_p->text));
                _p->order = rde_strchr(_p->text, '^') != NULL ? 2u : 5u;   // (1.5 \times 10^{20}: a product)
            }
            continue;
        }
        if(_op >= FZK_ADD && _op <= FZK_MOD) {
            if(_n < 2u) {
                _ok = false;
                break;
            }
            fzk_tex* _l = &_s[_n - 2u];
            const fzk_tex* _r = &_s[_n - 1u];
            int _w = 0;
            if(_op == FZK_DIV) {
                _w = rde_snprintf(_a, sizeof(_a), "\\frac{%s}{%s}", _l->text, _r->text);
                _l->order = 5u;
                _l->frac  = true;
            } else if(_op == FZK_POW) {
                // (its base in brackets unless a letter or a number: (x + 1)^{2}, (\sin x)^{2},
                // \left(\frac{1}{2}\right)^{2})
                FZK_BR(_l, _l->order < 5u || _l->frac, _b);
                _w = rde_snprintf(_a, sizeof(_a), "%s^{%s}", _b, _r->text);
                _l->order = 4u;
                _l->frac  = false;
            } else {
                const u8 _order = _op == FZK_ADD || _op == FZK_SUB ? 1u : 2u;
                const b8 _lb = _l->order < _order;
                const b8 _rb = _r->order < _order || (_r->order == _order && (_op == FZK_SUB || _op == FZK_MOD));
                c8 _left[FZK_WRITE];
                FZK_BR(_l, _lb, _left);
                FZK_BR(_r, _rb, _b);
                const c8* _sign = _op == FZK_ADD ? " + " : _op == FZK_SUB ? " - " : _op == FZK_MOD ? " \\operatorname{mod} " : " ";
                // (a product side by side, 2x, x y, 2\sin x, but with a dot when the right one starts with a digit:
                // 2 \cdot 3, x \cdot 2)
                if(_op == FZK_MUL && ((_r->number && !_rb) || (_b[0] >= '0' && _b[0] <= '9'))) {
                    _sign = " \\cdot ";
                }
                _w = rde_snprintf(_a, sizeof(_a), "%s%s%s", _left, _sign, _b);
                _l->order = _order;
                _l->frac  = false;
            }
            _ok = _w > 0 && (usize)_w < sizeof(_a);
            rde_snprintf(_l->text, sizeof(_l->text), "%s", _a);
            _l->number = false;
            _n--;
            continue;
        }
        if(_n < 1u) {
            _ok = false;
            break;
        }
        fzk_tex* _p = &_s[_n - 1u];
        int _w = 0;
        const b8 _simple = _p->order == 5u && !_p->frac;   // (a letter, a number: \sin x; else \sin\left(…\right))
        switch(_op) {
        case FZK_NEG:   FZK_BR(_p, _p->order < 3u, _b); _w = rde_snprintf(_a, sizeof(_a), "-%s", _b); _p->order = 3u; break;
        case FZK_SQRT:  _w = rde_snprintf(_a, sizeof(_a), "\\sqrt{%s}", _p->text); _p->order = 5u; break;
        case FZK_CBRT:  _w = rde_snprintf(_a, sizeof(_a), "\\sqrt[3]{%s}", _p->text); _p->order = 5u; break;
        case FZK_ABS:   _w = rde_snprintf(_a, sizeof(_a), "\\left|%s\\right|", _p->text); _p->order = 5u; break;
        case FZK_FLOOR: _w = rde_snprintf(_a, sizeof(_a), "\\left\\lfloor %s\\right\\rfloor", _p->text); _p->order = 5u; break;
        case FZK_CEIL:  _w = rde_snprintf(_a, sizeof(_a), "\\left\\lceil %s\\right\\rceil", _p->text); _p->order = 5u; break;
        case FZK_EXP:   _w = rde_snprintf(_a, sizeof(_a), "e^{%s}", _p->text); _p->order = 4u; break;
        default: {
            static const c8* const _tex[] = { [FZK_SIN] = "\\sin", [FZK_COS] = "\\cos", [FZK_TAN] = "\\tan", [FZK_ASIN] = "\\arcsin",
                                              [FZK_ACOS] = "\\arccos", [FZK_ATAN] = "\\arctan", [FZK_SINH] = "\\sinh", [FZK_COSH] = "\\cosh",
                                              [FZK_TANH] = "\\tanh", [FZK_LN] = "\\ln", [FZK_LOG] = "\\log", [FZK_LOG2] = "\\log_{2}",
                                              [FZK_ROUND] = "\\operatorname{round}", [FZK_SIGN] = "\\operatorname{sign}" };
            if(_op >= sizeof(_tex) / sizeof(_tex[0]) || _tex[_op] == NULL) {
                _ok = false;
                break;
            }
            _w = _simple ? rde_snprintf(_a, sizeof(_a), "%s %s", _tex[_op], _p->text) : rde_snprintf(_a, sizeof(_a), "%s\\left(%s\\right)", _tex[_op], _p->text);
            _p->order = 4u;
            break;
        }
        }
        if(!_ok) {
            break;
        }
        _ok = _w > 0 && (usize)_w < sizeof(_a);
        rde_snprintf(_p->text, sizeof(_p->text), "%s", _a);
        _p->number = false;
        _p->frac   = false;
    }
    #undef FZK_BR
    _ok = _ok && _n == 1u && rde_strlen(_s[0].text) < _size;
    if(_ok) {
        rde_snprintf(_out, _size, "%s", _s[0].text);
    }
    rde_arr_free(&_pieces);
    return _ok;
}

// See the header: no variables are given, so a function that uses one gives NaN.
f64 fude_zoom_calc_at(const fude_zoom_calc* _c, f64 _x) {
    return fude_zoom_calc_at_with(_c, _x, NULL);
}

// See the header. The steps run on a stack of numbers; a step without its operands, or a stack not left with one
// number, gives NaN (a parsed function has neither).
f64 fude_zoom_calc_at_with(const fude_zoom_calc* _c, f64 _x, const f64* _vars) {
    f64 _s[FUDE_ZOOM_CALC_STEPS];
    u32 _n = 0;
    for(u32 _i = 0; _i < _c->count; _i++) {
        const u8 _op = _c->op[_i];
        if(_op == FZK_NUM || _op == FZK_X || _op == FZK_VAR) {
            _s[_n++] = _op == FZK_NUM ? _c->value[_i] : (_op == FZK_X ? _x : (_vars != NULL ? _vars[(u32)_c->value[_i] % 26u] : RDE_NAN_F64));
            continue;
        }
        if(_op >= FZK_ADD && _op <= FZK_MOD) {
            if(_n < 2u) {
                return RDE_NAN_F64;
            }
            const f64 _b = _s[--_n], _a = _s[_n - 1u];
            f64 _r = 0.0;
            switch(_op) {
            case FZK_ADD: _r = _a + _b; break;
            case FZK_SUB: _r = _a - _b; break;
            case FZK_MUL: _r = _a * _b; break;
            case FZK_DIV: _r = _b != 0.0 ? _a / _b : RDE_NAN_F64; break;
            case FZK_POW: _r = rde_math_pow_f64(_a, _b); break;
            default:      _r = _b != 0.0 ? rde_math_fmod_f64(_a, _b) : RDE_NAN_F64; break;
            }
            _s[_n - 1u] = _r;
            continue;
        }
        if(_n < 1u) {
            return RDE_NAN_F64;
        }
        const f64 _a = _s[_n - 1u];
        f64 _r;
        switch(_op) {
        case FZK_NEG:   _r = -_a; break;
        case FZK_SIN:   _r = rde_math_sin_f64(_a); break;
        case FZK_COS:   _r = rde_math_cos_f64(_a); break;
        case FZK_TAN:   _r = rde_math_tan_f64(_a); break;
        case FZK_ASIN:  _r = rde_math_asin_f64(_a); break;
        case FZK_ACOS:  _r = rde_math_acos_f64(_a); break;
        case FZK_ATAN:  _r = rde_math_atan_f64(_a); break;
        case FZK_SINH:  _r = rde_math_sinh_f64(_a); break;
        case FZK_COSH:  _r = rde_math_cosh_f64(_a); break;
        case FZK_TANH:  _r = rde_math_tanh_f64(_a); break;
        case FZK_SQRT:  _r = _a >= 0.0 ? rde_math_sqrt_f64(_a) : RDE_NAN_F64; break;
        case FZK_CBRT:  _r = rde_math_cbrt_f64(_a); break;
        case FZK_ABS:   _r = rde_math_abs_f64(_a); break;
        case FZK_EXP:   _r = rde_math_exp_f64(_a); break;
        case FZK_LN:    _r = _a > 0.0 ? rde_math_log_f64(_a) : RDE_NAN_F64; break;
        case FZK_LOG:   _r = _a > 0.0 ? rde_math_log10_f64(_a) : RDE_NAN_F64; break;
        case FZK_LOG2:  _r = _a > 0.0 ? rde_math_log2_f64(_a) : RDE_NAN_F64; break;
        case FZK_FLOOR: _r = rde_math_floor_f64(_a); break;
        case FZK_CEIL:  _r = rde_math_ceil_f64(_a); break;
        case FZK_ROUND: _r = rde_math_round_f64(_a); break;
        default:        _r = _a > 0.0 ? 1.0 : (_a < 0.0 ? -1.0 : 0.0); break;
        }
        _s[_n - 1u] = _r;
    }
    return _n == 1u ? _s[0] : RDE_NAN_F64;
}
