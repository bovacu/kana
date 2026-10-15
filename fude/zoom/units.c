// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/units.h"

// ===========================================================================
// See units.h, which has the grammar.
//
// Laid out as: the units, the variables' table, the reader, and the writer. The reader is a recursive descent parser,
// one function a level of the grammar, from the bottom up: fude_zoom_units_number_token (a number, mixed number or
// quotient), fude_zoom_units_primary (a number, a variable or a bracketed sum), fude_zoom_units_postfix (its unit,
// and a number in feet's inches), fude_zoom_units_unary (signs), fude_zoom_units_product (* / × ÷) and
// fude_zoom_units_sum (+ - −). Each value carries whether it is a measure or a plain number, combined by
// fude_zoom_units_combine; a parse stops at the first error and records it. Brackets and signs count toward
// FUDE_ZOOM_UNITS_DEEPEST, so no text can nest the reader deeper than that. The writer counts whole steps of the
// precision in a u64, so the digits and fractions it writes are exact; a coarser step is taken when they would pass
// 2^53, and past 2^53 whole units it writes a short power of ten instead.
// ===========================================================================

// The fields: what a measure is in each, and which units it takes.
#define FUDE_ZOOM_UNITS_FIELD_LENGTH 0u
#define FUDE_ZOOM_UNITS_FIELD_ANGLE  1u
#define FUDE_ZOOM_UNITS_FIELD_NUMBER 2u

// What fude_zoom_units_read_unit finds, beyond the length units: degrees, a word that is no unit, or nothing that
// looks like a unit.
#define FUDE_ZOOM_UNITS_DEGREE FUDE_ZOOM_UNIT_COUNT
#define FUDE_ZOOM_UNITS_WORD   (FUDE_ZOOM_UNIT_COUNT + 1u)
#define FUDE_ZOOM_UNITS_NONE   0xFFu

#define FUDE_ZOOM_UNITS_DEEPEST 64u                  // brackets and signs nested at most (units.h's RANGE error)
#define FUDE_ZOOM_UNITS_EXACT   9007199254740992.0   // 2^53: every whole number below it is a double

// The powers of ten a double holds exactly: a decimal is its digits over one, a single rounding, so 9.525 reads as
// the double nearest 9.525.
static const f64 FUDE_ZOOM_UNITS_TEN[23] = {
    1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,
    1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22,
};

// The length units' words, in FUDE_ZOOM_UNIT_'s order, as typed (in any case).
static const c8* const FUDE_ZOOM_UNITS_WORDS[FUDE_ZOOM_UNIT_COUNT] = { "mm", "cm", "m", "in", "ft" };

// See the header.
f64 fude_zoom_unit_mm(FUDE_ZOOM_UNIT_ _u) {
    switch(_u) {
        case FUDE_ZOOM_UNIT_MM: return 1.0;
        case FUDE_ZOOM_UNIT_CM: return 10.0;
        case FUDE_ZOOM_UNIT_M:  return 1000.0;
        case FUDE_ZOOM_UNIT_IN: return 25.4;
        case FUDE_ZOOM_UNIT_FT: return 304.8;
        default:                return 0.0;
    }
}

// See the header.
const c8* fude_zoom_unit_suffix(FUDE_ZOOM_UNIT_ _u) {
    switch(_u) {
        case FUDE_ZOOM_UNIT_MM: return "mm";
        case FUDE_ZOOM_UNIT_CM: return "cm";
        case FUDE_ZOOM_UNIT_M:  return "m";
        case FUDE_ZOOM_UNIT_IN: return "\"";
        case FUDE_ZOOM_UNIT_FT: return "'";
        default:                return "";
    }
}

// Returns 10 to the _n: from the exact table from 0 to 22, else from rde_math_pow_f64.
RDE_INTERNAL f64 fude_zoom_units_ten(i32 _n) {
    return _n >= 0 && _n <= 22 ? FUDE_ZOOM_UNITS_TEN[_n] : rde_math_pow_f64(10.0, (f64)_n);
}

// --- names ------------------------------------------------------------------

// Returns _c in lower case, ASCII only: a locale must not change what a name matches.
RDE_INTERNAL c8 fude_zoom_units_lower(c8 _c) {
    return _c >= 'A' && _c <= 'Z' ? (c8)(_c + ('a' - 'A')) : _c;
}

// Returns whether _c can begin a name or a unit word: a letter or '_'.
RDE_INTERNAL b8 fude_zoom_units_word_start(c8 _c) {
    return (_c >= 'A' && _c <= 'Z') || (_c >= 'a' && _c <= 'z') || _c == '_';
}

// Returns whether _c can go on in a name: a letter, '_' or a digit.
RDE_INTERNAL b8 fude_zoom_units_word_part(c8 _c) {
    return fude_zoom_units_word_start(_c) || (_c >= '0' && _c <= '9');
}

// Returns whether the _n bytes at _a are the whole of the string _b, in any case.
RDE_INTERNAL b8 fude_zoom_units_same(const c8* _a, u32 _n, const c8* _b) {
    for(u32 _i = 0; _i < _n; _i++) {
        if(_b[_i] == 0 || fude_zoom_units_lower(_a[_i]) != fude_zoom_units_lower(_b[_i])) {
            return false;
        }
    }
    return _b[_n] == 0;
}

// Returns which unit word the _n bytes at _s are, or FUDE_ZOOM_UNITS_WORD when they are none.
RDE_INTERNAL u8 fude_zoom_units_unit_word(const c8* _s, u32 _n) {
    for(u8 _u = 0; _u < FUDE_ZOOM_UNIT_COUNT; _u++) {
        if(fude_zoom_units_same(_s, _n, FUDE_ZOOM_UNITS_WORDS[_u])) {
            return _u;
        }
    }
    return FUDE_ZOOM_UNITS_WORD;
}

// Returns the variable named by the _n bytes at _s, in any case, or NULL (also when there is no table). The pointer
// is into the table; it is not const so fude_zoom_vars_set can change what it finds.
RDE_INTERNAL fude_zoom_var* fude_zoom_units_lookup(const fude_zoom_vars* _v, const c8* _s, u32 _n) {
    if(_v == NULL) {
        return NULL;
    }
    for(u32 _i = 0; _i < _v->count && _i < FUDE_ZOOM_VARS_MOST; _i++) {
        if(fude_zoom_units_same(_s, _n, _v->vars[_i].name)) {
            return (fude_zoom_var*)&_v->vars[_i];
        }
    }
    return NULL;
}

// See the header. The name must start with a letter or '_' and go on in letters, digits and '_'; a new one goes at
// the table's end, and one already there keeps its place but takes the new name's case.
b8 fude_zoom_vars_set(fude_zoom_vars* _v, const c8* _name, f64 _value, b8 _length) {
    if(_v == NULL || _name == NULL || !rde_math_is_finite_f64(_value) || !fude_zoom_units_word_start(_name[0])) {
        return false;
    }
    u32 _n = 0;
    while(_name[_n] != 0) {
        if(!fude_zoom_units_word_part(_name[_n]) || _n + 1u >= FUDE_ZOOM_VAR_NAME) {
            return false;
        }
        _n++;
    }
    // 2 m must stay two metres, never 2 times a variable called m.
    if(fude_zoom_units_unit_word(_name, _n) != FUDE_ZOOM_UNITS_WORD) {
        return false;
    }
    fude_zoom_var* _var = fude_zoom_units_lookup(_v, _name, _n);
    if(_var == NULL) {
        if(_v->count >= FUDE_ZOOM_VARS_MOST) {
            return false;
        }
        _var = &_v->vars[_v->count++];
    }
    rde_memset(_var->name, 0, sizeof _var->name);
    rde_memcpy(_var->name, _name, _n);
    _var->value  = _value;
    _var->length = _length;
    return true;
}

// See the header.
const fude_zoom_var* fude_zoom_vars_find(const fude_zoom_vars* _v, const c8* _name) {
    return _name != NULL ? fude_zoom_units_lookup(_v, _name, (u32)rde_strlen(_name)) : NULL;
}

// See the header.
b8 fude_zoom_vars_remove(fude_zoom_vars* _v, const c8* _name) {
    const fude_zoom_var* _var = fude_zoom_vars_find(_v, _name);
    if(_var == NULL) {
        return false;
    }
    const u32 _i = (u32)(_var - _v->vars);
    rde_memmove(&_v->vars[_i], &_v->vars[_i + 1u], (usize)(_v->count - _i - 1u) * sizeof(fude_zoom_var));
    _v->count--;
    return true;
}

// --- reading ----------------------------------------------------------------

// A value as it is read: a number, and what kind it is (units.h's KINDS).
typedef struct {
    f64 v;
    b8  measure;   // a length (mm) or an angle (degrees), as the field has it; else plain
} fude_zoom_units_value;

// The reader's state while it goes through one text.
typedef struct {
    const c8*             text;
    u32                   at;      // the byte read next
    u8                    field;   // FUDE_ZOOM_UNITS_FIELD_
    f64                   plain;   // a plain number's worth in the field's measure: the default unit's mm, or 1
    u32                   depth;   // brackets and signs open now, up to FUDE_ZOOM_UNITS_DEEPEST
    const fude_zoom_vars* vars;    // may be NULL: no variables
    fude_zoom_units_error error;   // the first error met
} fude_zoom_units_parser;

// Records error _code at byte _at, and returns false, so a reader can return it straight away.
RDE_INTERNAL b8 fude_zoom_units_fail(fude_zoom_units_parser* _p, FUDE_ZOOM_UNITS_ERROR_ _code, u32 _at) {
    _p->error.code = _code;
    _p->error.at   = _at;
    return false;
}

// Returns the length of _token when the text has it at _at, else 0.
RDE_INTERNAL u32 fude_zoom_units_is(const c8* _t, u32 _at, const c8* _token) {
    const u32 _n = (u32)rde_strlen(_token);
    for(u32 _i = 0; _i < _n; _i++) {
        if(_t[_at + _i] != _token[_i]) {
            return 0;
        }
    }
    return _n;
}

// Returns where the text goes on past spaces at _at: ASCII ones (tabs and line breaks too), and the no-break ones
// (U+00A0, U+202F) a phone puts in.
RDE_INTERNAL u32 fude_zoom_units_skip(const c8* _t, u32 _at) {
    for(;;) {
        if(_t[_at] == ' ' || _t[_at] == '\t' || _t[_at] == '\n' || _t[_at] == '\r') {
            _at++;
        } else if(fude_zoom_units_is(_t, _at, "\xC2\xA0") || fude_zoom_units_is(_t, _at, "\xE2\x80\xAF")) {
            _at += _t[_at] == '\xC2' ? 2u : 3u;
        } else {
            return _at;
        }
    }
}

// Returns whether _c is an ASCII digit.
RDE_INTERNAL b8 fude_zoom_units_digit(c8 _c) {
    return _c >= '0' && _c <= '9';
}

// Returns whether a number starts at _at: a digit, or a point ('.' or ',') with a digit after it.
RDE_INTERNAL b8 fude_zoom_units_number_at(const c8* _t, u32 _at) {
    return fude_zoom_units_digit(_t[_at]) || ((_t[_at] == '.' || _t[_at] == ',') && fude_zoom_units_digit(_t[_at + 1u]));
}

// Reads a decimal at _at: its value into *_out, where it ends into *_end, and whether it had no point into *_whole.
// Returns false when no number starts there. The first 19 figures that count are kept in a whole number (19 always
// fit a u64); later ones before the point only scale it, later ones after it are past what a double holds anyway.
RDE_INTERNAL b8 fude_zoom_units_decimal(const c8* _t, u32 _at, f64* _out, u32* _end, b8* _whole) {
    if(!fude_zoom_units_number_at(_t, _at)) {
        return false;
    }
    u64 _m = 0;
    i32 _scale = 0;
    u32 _figures = 0;
    b8  _point = false;
    u32 _i = _at;
    for(;; _i++) {
        const c8 _c = _t[_i];
        if(fude_zoom_units_digit(_c)) {
            if(_figures < 19u) {
                _m = _m * 10u + (u64)(_c - '0');
                _figures += _m != 0u;
                _scale -= _point;
            } else if(!_point) {
                _scale++;
            }
        } else if((_c == '.' || _c == ',') && !_point) {
            _point = true;
        } else {
            break;
        }
    }
    *_out   = _scale >= 0 ? (f64)_m * fude_zoom_units_ten(_scale) : (f64)_m / fude_zoom_units_ten(-_scale);
    *_end   = _i;
    *_whole = !_point;
    return true;
}

// Reads a fraction written tight at _at, both parts whole numbers (3/8), into *_top and *_bottom, and where it ends
// into *_end. Returns false when there is none there.
RDE_INTERNAL b8 fude_zoom_units_fraction(const c8* _t, u32 _at, f64* _top, f64* _bottom, u32* _end) {
    u32 _mid;
    b8  _whole;
    if(!fude_zoom_units_digit(_t[_at]) || !fude_zoom_units_decimal(_t, _at, _top, &_mid, &_whole) || !_whole || _t[_mid] != '/') {
        return false;
    }
    return fude_zoom_units_digit(_t[_mid + 1u]) && fude_zoom_units_decimal(_t, _mid + 1u, _bottom, _end, &_whole) && _whole;
}

// Returns whether a '*' '/' '×' '÷' comes next, spaces aside; then 1-3/8 is not read as a mixed number.
RDE_INTERNAL b8 fude_zoom_units_product_next(const c8* _t, u32 _at) {
    _at = fude_zoom_units_skip(_t, _at);
    return _t[_at] == '*' || _t[_at] == '/' || fude_zoom_units_is(_t, _at, "\xC3\x97") || fude_zoom_units_is(_t, _at, "\xC3\xB7");
}

// Reads a number at the parser's place into *_out: a decimal, a mixed number (1-3/8, 1 3/8) or a quotient (3/8,
// 1200/3), and moves past it. Fails for a quotient over 0 and for a value that is not finite.
RDE_INTERNAL b8 fude_zoom_units_number_token(fude_zoom_units_parser* _p, f64* _out) {
    const c8* _t = _p->text;
    f64 _a;
    u32 _end;
    b8  _whole;
    fude_zoom_units_decimal(_t, _p->at, &_a, &_end, &_whole);
    if(!rde_math_is_finite_f64(_a)) {
        return fude_zoom_units_fail(_p, FUDE_ZOOM_UNITS_ERROR_RANGE, _p->at);
    }
    if(_whole) {
        f64 _top, _bottom;
        u32 _fend;
        if(_t[_end] == '-' && fude_zoom_units_fraction(_t, _end + 1u, &_top, &_bottom, &_fend) && _top < _bottom && !fude_zoom_units_product_next(_t, _fend)) {
            *_out  = _a + _top / _bottom;
            _p->at = _fend;
            return true;
        }
        const u32 _next = fude_zoom_units_skip(_t, _end);
        if(_next > _end && fude_zoom_units_fraction(_t, _next, &_top, &_bottom, &_fend) && _top < _bottom) {
            *_out  = _a + _top / _bottom;
            _p->at = _fend;
            return true;
        }
    }
    if(_t[_end] == '/' && fude_zoom_units_number_at(_t, _end + 1u)) {
        f64 _b;
        u32 _bend;
        fude_zoom_units_decimal(_t, _end + 1u, &_b, &_bend, &_whole);
        if(_b == 0.0) {
            return fude_zoom_units_fail(_p, FUDE_ZOOM_UNITS_ERROR_ZERO, _end);
        }
        *_out = _a / _b;
        if(!rde_math_is_finite_f64(*_out)) {
            return fude_zoom_units_fail(_p, FUDE_ZOOM_UNITS_ERROR_RANGE, _p->at);
        }
        _p->at = _bend;
        return true;
    }
    *_out  = _a;
    _p->at = _end;
    return true;
}

// Returns what unit, if any, is written at _at (its bytes into *_n): a length unit, a degree, a word that is no unit,
// or NONE. _name: a word goes on through digits (a variable's name); after a number it stops at one, so 2ft3in is
// feet and inches.
RDE_INTERNAL u8 fude_zoom_units_read_unit(const c8* _t, u32 _at, b8 _name, u32* _n) {
    *_n = 0;
    if(fude_zoom_units_word_start(_t[_at])) {
        u32 _e = _at;
        while(_name ? fude_zoom_units_word_part(_t[_e]) : fude_zoom_units_word_start(_t[_e])) {
            _e++;
        }
        *_n = _e - _at;
        return fude_zoom_units_unit_word(&_t[_at], *_n);
    }
    if(_t[_at] == '"') {
        *_n = 1u;
        return FUDE_ZOOM_UNIT_IN;
    }
    if(_t[_at] == '\'') {
        // Two apostrophes are how inches are typed without a " key.
        *_n = _t[_at + 1u] == '\'' ? 2u : 1u;
        return *_n == 2u ? FUDE_ZOOM_UNIT_IN : FUDE_ZOOM_UNIT_FT;
    }
    if((*_n = fude_zoom_units_is(_t, _at, "\xE2\x80\xB3")) != 0u) {
        return FUDE_ZOOM_UNIT_IN;
    }
    if((*_n = fude_zoom_units_is(_t, _at, "\xE2\x80\xB2")) != 0u) {
        return FUDE_ZOOM_UNIT_FT;
    }
    // ° and º: a Spanish keyboard's degree is the ordinal.
    if((*_n = fude_zoom_units_is(_t, _at, "\xC2\xB0")) != 0u || (*_n = fude_zoom_units_is(_t, _at, "\xC2\xBA")) != 0u) {
        return FUDE_ZOOM_UNITS_DEGREE;
    }
    return FUDE_ZOOM_UNITS_NONE;
}

// Puts a unit on a value, which becomes a measure (a length is turned into mm). Fails when the field does not take
// the unit, or the value is a measure already.
RDE_INTERNAL b8 fude_zoom_units_apply(fude_zoom_units_parser* _p, fude_zoom_units_value* _v, u8 _unit, u32 _at) {
    const b8 _fits = _unit == FUDE_ZOOM_UNITS_DEGREE ? _p->field == FUDE_ZOOM_UNITS_FIELD_ANGLE
                   : _unit < FUDE_ZOOM_UNIT_COUNT     ? _p->field == FUDE_ZOOM_UNITS_FIELD_LENGTH
                   : false;
    if(!_fits || _v->measure) {
        return fude_zoom_units_fail(_p, FUDE_ZOOM_UNITS_ERROR_UNIT, _at);
    }
    if(_unit < FUDE_ZOOM_UNIT_COUNT) {
        _v->v *= fude_zoom_unit_mm((FUDE_ZOOM_UNIT_)_unit);
    }
    _v->measure = true;
    return true;
}

RDE_INTERNAL b8 fude_zoom_units_sum(fude_zoom_units_parser* _p, fude_zoom_units_value* _v);

// Reads a number, a variable or a bracketed sum into *_v. *_kind: 0 a number, 1 a group, 2 a variable. A length
// variable outside a length field is an error.
RDE_INTERNAL b8 fude_zoom_units_primary(fude_zoom_units_parser* _p, fude_zoom_units_value* _v, u8* _kind) {
    const c8* _t = _p->text;
    _p->at = fude_zoom_units_skip(_t, _p->at);
    const u32 _at = _p->at;
    if(_t[_at] == 0) {
        return fude_zoom_units_fail(_p, FUDE_ZOOM_UNITS_ERROR_SYNTAX, _at);
    }
    if(_t[_at] == '(') {
        if(++_p->depth > FUDE_ZOOM_UNITS_DEEPEST) {
            return fude_zoom_units_fail(_p, FUDE_ZOOM_UNITS_ERROR_RANGE, _at);
        }
        _p->at++;
        if(!fude_zoom_units_sum(_p, _v)) {
            return false;
        }
        _p->at = fude_zoom_units_skip(_t, _p->at);
        if(_t[_p->at] != ')') {
            return _t[_p->at] == 0 ? fude_zoom_units_fail(_p, FUDE_ZOOM_UNITS_ERROR_PAREN, _at)
                                   : fude_zoom_units_fail(_p, FUDE_ZOOM_UNITS_ERROR_SYNTAX, _p->at);
        }
        _p->at++;
        _p->depth--;
        *_kind = 1u;
        return true;
    }
    if(fude_zoom_units_number_at(_t, _at)) {
        _v->measure = false;
        *_kind      = 0u;
        return fude_zoom_units_number_token(_p, &_v->v);
    }
    u32      _n;
    const u8 _unit = fude_zoom_units_read_unit(_t, _at, true, &_n);
    if(_unit == FUDE_ZOOM_UNITS_WORD) {
        const fude_zoom_var* _var = fude_zoom_units_lookup(_p->vars, &_t[_at], _n);
        if(_var == NULL) {
            return fude_zoom_units_fail(_p, FUDE_ZOOM_UNITS_ERROR_NAME, _at);
        }
        if(_var->length && _p->field != FUDE_ZOOM_UNITS_FIELD_LENGTH) {
            return fude_zoom_units_fail(_p, FUDE_ZOOM_UNITS_ERROR_DIMENSION, _at);
        }
        _v->v       = _var->value;
        _v->measure = _var->length;
        _p->at     += _n;
        *_kind      = 2u;
        return true;
    }
    return fude_zoom_units_fail(_p, _unit != FUDE_ZOOM_UNITS_NONE ? FUDE_ZOOM_UNITS_ERROR_UNIT : FUDE_ZOOM_UNITS_ERROR_SYNTAX, _at);
}

// Reads a primary and its unit, and after a number in feet, its inches. A second unit after the first is an error.
RDE_INTERNAL b8 fude_zoom_units_postfix(fude_zoom_units_parser* _p, fude_zoom_units_value* _v) {
    const c8* _t = _p->text;
    u8 _kind;
    if(!fude_zoom_units_primary(_p, _v, &_kind)) {
        return false;
    }
    u32 _at = fude_zoom_units_skip(_t, _p->at), _n;
    u8  _unit = fude_zoom_units_read_unit(_t, _at, false, &_n);
    if(_unit == FUDE_ZOOM_UNITS_NONE) {
        return true;
    }
    if(_kind == 2u) {
        // stock mm: a variable has its unit already, or is plain and wants brackets. Any other word after it is two
        // operands with nothing between them, left for the caller to refuse.
        return _unit == FUDE_ZOOM_UNITS_WORD ? true : fude_zoom_units_fail(_p, FUDE_ZOOM_UNITS_ERROR_UNIT, _at);
    }
    if(!fude_zoom_units_apply(_p, _v, _unit, _at)) {
        return false;
    }
    _p->at = _at + _n;
    if(_unit == FUDE_ZOOM_UNIT_FT && _kind == 0u) {
        // The inches: a number next (1' 6", 1'6), or a tight hyphen and a number (the drawing office's 1'-6"). Like
        // 1-3/8, a hyphen is a subtraction after all if a product follows (2'-6*2) or the number has a unit of its
        // own other than inches (2'-6mm).
        const u32 _feet = _p->at;
        const b8  _dash = _t[_feet] == '-' && fude_zoom_units_number_at(_t, _feet + 1u);
        const u32 _in   = _dash ? _feet + 1u : fude_zoom_units_skip(_t, _feet);
        if(fude_zoom_units_number_at(_t, _in)) {
            f64 _inches;
            _p->at = _in;
            if(!fude_zoom_units_number_token(_p, &_inches)) {
                return false;
            }
            _at   = fude_zoom_units_skip(_t, _p->at);
            _unit = fude_zoom_units_read_unit(_t, _at, false, &_n);
            const b8 _other = _unit != FUDE_ZOOM_UNITS_NONE && _unit != FUDE_ZOOM_UNIT_IN;
            if(_other && !_dash) {
                return fude_zoom_units_fail(_p, FUDE_ZOOM_UNITS_ERROR_UNIT, _at);
            }
            if(_unit == FUDE_ZOOM_UNIT_IN) {
                _p->at = _at + _n;
            }
            if(_dash && (_other || fude_zoom_units_product_next(_t, _p->at))) {
                _p->at = _feet;
            } else {
                _v->v += _inches * 25.4;
            }
        }
    }
    // 2mm mm, 2mm": a second unit.
    _at = fude_zoom_units_skip(_t, _p->at);
    if(fude_zoom_units_read_unit(_t, _at, false, &_n) != FUDE_ZOOM_UNITS_NONE) {
        return fude_zoom_units_fail(_p, FUDE_ZOOM_UNITS_ERROR_UNIT, _at);
    }
    return true;
}

// Reads a signed value: any number of '+', '-' and '−' before a postfix, each nesting one deeper.
RDE_INTERNAL b8 fude_zoom_units_unary(fude_zoom_units_parser* _p, fude_zoom_units_value* _v) {
    const c8* _t = _p->text;
    _p->at = fude_zoom_units_skip(_t, _p->at);
    const u32 _n = _t[_p->at] == '-' || _t[_p->at] == '+' ? 1u : fude_zoom_units_is(_t, _p->at, "\xE2\x88\x92");
    if(_n == 0u) {
        return fude_zoom_units_postfix(_p, _v);
    }
    if(++_p->depth > FUDE_ZOOM_UNITS_DEEPEST) {
        return fude_zoom_units_fail(_p, FUDE_ZOOM_UNITS_ERROR_RANGE, _p->at);
    }
    const b8 _minus = _t[_p->at] != '+';
    _p->at += _n;
    if(!fude_zoom_units_unary(_p, _v)) {
        return false;
    }
    _p->depth--;
    _v->v = _minus ? -_v->v : _v->v;
    return true;
}

// Works out _a op _b by the kinds' rules (units.h) into _a; _op is '+', '-', '*' or '/'. _at: the operator, for
// errors. Fails on measure × measure, plain ÷ measure, a division by 0, or a result that is not finite.
RDE_INTERNAL b8 fude_zoom_units_combine(fude_zoom_units_parser* _p, fude_zoom_units_value* _a, fude_zoom_units_value _b, c8 _op, u32 _at) {
    switch(_op) {
        case '+':
        case '-':
            // The plain one is in the field's unit: 450 - 36 mm is 414 mm.
            if(_a->measure != _b.measure) {
                if(_a->measure) {
                    _b.v *= _p->plain;
                } else {
                    _a->v *= _p->plain;
                }
                _a->measure = true;
            }
            _a->v = _op == '+' ? _a->v + _b.v : _a->v - _b.v;
            break;
        case '*':
            if(_a->measure && _b.measure) {
                return fude_zoom_units_fail(_p, FUDE_ZOOM_UNITS_ERROR_DIMENSION, _at);
            }
            _a->v      *= _b.v;
            _a->measure = _a->measure || _b.measure;
            break;
        default:
            if(!_a->measure && _b.measure) {
                return fude_zoom_units_fail(_p, FUDE_ZOOM_UNITS_ERROR_DIMENSION, _at);
            }
            if(_b.v == 0.0) {
                return fude_zoom_units_fail(_p, FUDE_ZOOM_UNITS_ERROR_ZERO, _at);
            }
            _a->v      /= _b.v;
            _a->measure = _a->measure && !_b.measure;
            break;
    }
    return rde_math_is_finite_f64(_a->v) ? true : fude_zoom_units_fail(_p, FUDE_ZOOM_UNITS_ERROR_RANGE, _at);
}

// Reads unaries joined by * / × ÷, left to right.
RDE_INTERNAL b8 fude_zoom_units_product(fude_zoom_units_parser* _p, fude_zoom_units_value* _v) {
    const c8* _t = _p->text;
    if(!fude_zoom_units_unary(_p, _v)) {
        return false;
    }
    for(;;) {
        const u32 _at = fude_zoom_units_skip(_t, _p->at);
        c8  _op = 0;
        u32 _n  = 1u;
        if(_t[_at] == '*' || _t[_at] == '/') {
            _op = _t[_at];
        } else if((_n = fude_zoom_units_is(_t, _at, "\xC3\x97")) != 0u) {
            _op = '*';
        } else if((_n = fude_zoom_units_is(_t, _at, "\xC3\xB7")) != 0u) {
            _op = '/';
        } else {
            return true;
        }
        fude_zoom_units_value _b;
        _p->at = _at + _n;
        if(!fude_zoom_units_unary(_p, &_b) || !fude_zoom_units_combine(_p, _v, _b, _op, _at)) {
            return false;
        }
    }
}

// Reads products joined by + - −, left to right.
RDE_INTERNAL b8 fude_zoom_units_sum(fude_zoom_units_parser* _p, fude_zoom_units_value* _v) {
    const c8* _t = _p->text;
    if(!fude_zoom_units_product(_p, _v)) {
        return false;
    }
    for(;;) {
        const u32 _at = fude_zoom_units_skip(_t, _p->at);
        c8  _op = 0;
        u32 _n  = 1u;
        if(_t[_at] == '+' || _t[_at] == '-') {
            _op = _t[_at];
        } else if((_n = fude_zoom_units_is(_t, _at, "\xE2\x88\x92")) != 0u) {
            _op = '-';
        } else {
            return true;
        }
        fude_zoom_units_value _b;
        _p->at = _at + _n;
        if(!fude_zoom_units_product(_p, &_b) || !fude_zoom_units_combine(_p, _v, _b, _op, _at)) {
            return false;
        }
    }
}

// Reads the whole of _text as a sum in a field of kind _field into *_out (left alone on failure), a plain result taken
// as _plain of the field's measure, and writes what went wrong to *_error (may be NULL). Anything left after the sum
// is a syntax error.
RDE_INTERNAL b8 fude_zoom_units_parse(const c8* _text, u8 _field, f64 _plain, const fude_zoom_vars* _vars, f64* _out, fude_zoom_units_error* _error) {
    fude_zoom_units_parser _p = { _text, 0u, _field, _plain, 0u, _vars, { FUDE_ZOOM_UNITS_OK, 0u } };
    fude_zoom_units_value  _v = { 0.0, false };
    b8 _ok = false;
    if(_text == NULL) {
        fude_zoom_units_fail(&_p, FUDE_ZOOM_UNITS_ERROR_EMPTY, 0u);
    } else {
        // "≈ 17.46 mm", as format wrote it: the value is what it shows.
        _p.at  = fude_zoom_units_skip(_text, 0u);
        _p.at += fude_zoom_units_is(_text, _p.at, "\xE2\x89\x88");
        _p.at  = fude_zoom_units_skip(_text, _p.at);
        if(_text[_p.at] == 0) {
            fude_zoom_units_fail(&_p, FUDE_ZOOM_UNITS_ERROR_EMPTY, 0u);
        } else if(fude_zoom_units_sum(&_p, &_v)) {
            _p.at = fude_zoom_units_skip(_text, _p.at);
            if(_text[_p.at] != 0) {
                fude_zoom_units_fail(&_p, FUDE_ZOOM_UNITS_ERROR_SYNTAX, _p.at);
            } else {
                const f64 _result = _v.measure ? _v.v : _v.v * _plain;
                _ok = rde_math_is_finite_f64(_result) ? true : fude_zoom_units_fail(&_p, FUDE_ZOOM_UNITS_ERROR_RANGE, _p.at);
                if(_ok) {
                    *_out = _result;
                }
            }
        }
    }
    if(_error != NULL) {
        *_error = _p.error;
    }
    return _ok;
}

// See the header. A plain number is _unit's millimetres, or millimetres when _unit is none.
b8 fude_zoom_units_length(const c8* _text, FUDE_ZOOM_UNIT_ _unit, const fude_zoom_vars* _vars, f64* _mm, fude_zoom_units_error* _error) {
    const f64 _plain = fude_zoom_unit_mm(_unit) > 0.0 ? fude_zoom_unit_mm(_unit) : 1.0;
    return fude_zoom_units_parse(_text, FUDE_ZOOM_UNITS_FIELD_LENGTH, _plain, _vars, _mm, _error);
}

// See the header. A plain number is in degrees.
b8 fude_zoom_units_angle(const c8* _text, const fude_zoom_vars* _vars, f64* _degrees, fude_zoom_units_error* _error) {
    return fude_zoom_units_parse(_text, FUDE_ZOOM_UNITS_FIELD_ANGLE, 1.0, _vars, _degrees, _error);
}

// See the header.
b8 fude_zoom_units_number(const c8* _text, const fude_zoom_vars* _vars, f64* _out, fude_zoom_units_error* _error) {
    return fude_zoom_units_parse(_text, FUDE_ZOOM_UNITS_FIELD_NUMBER, 1.0, _vars, _out, _error);
}

// --- saying -----------------------------------------------------------------

// Writes into the caller's buffer a whole token at a time, so a short buffer never ends inside a character.
typedef struct {
    c8*   out;
    usize size;
    usize n;      // the bytes written so far, the 0 after them not counted
    b8    full;   // a token did not fit: nothing more is written
} fude_zoom_units_writer;

// Appends _s whole, with its 0 after it, or nothing and nothing more once one did not fit.
RDE_INTERNAL void fude_zoom_units_put(fude_zoom_units_writer* _w, const c8* _s) {
    const usize _n = rde_strlen(_s);
    if(_w->full || _w->n + _n + 1u > _w->size) {
        _w->full = true;
        return;
    }
    rde_memcpy(_w->out + _w->n, _s, _n);
    _w->n += _n;
    _w->out[_w->n] = 0;
}

// Appends a whole number.
RDE_INTERNAL void fude_zoom_units_put_u64(fude_zoom_units_writer* _w, u64 _x) {
    c8 _s[24];
    rde_snprintf(_s, sizeof _s, "%llu", (unsigned long long)_x);
    fude_zoom_units_put(_w, _s);
}

// Appends _n steps of 1/_q as decimals (_q a power of ten): 175 of 1/100 is "1.75", trailing zeros dropped.
RDE_INTERNAL void fude_zoom_units_put_decimal(fude_zoom_units_writer* _w, u64 _n, u64 _q) {
    fude_zoom_units_put_u64(_w, _n / _q);
    u64 _rest = _n % _q;
    if(_rest == 0u) {
        return;
    }
    c8  _s[24];
    u32 _k = 0;
    _s[_k++] = '.';
    for(u64 _d = _q / 10u; _d > 0u && _rest > 0u; _d /= 10u) {
        _s[_k++] = (c8)('0' + _rest / _d);
        _rest   %= _d;
    }
    _s[_k] = 0;
    fude_zoom_units_put(_w, _s);
}

// Appends _n steps of 1/_q (_q a power of two) as a whole and a reduced fraction: 22 of 1/16 is "1-3/8". The whole
// is left out when it is 0 and there is a fraction.
RDE_INTERNAL void fude_zoom_units_put_fraction(fude_zoom_units_writer* _w, u64 _n, u64 _q) {
    const u64 _whole = _n / _q;
    u64 _top = _n % _q, _bottom = _q;
    while(_top > 0u && (_top & 1u) == 0u) {
        _top    >>= 1u;
        _bottom >>= 1u;
    }
    if(_whole > 0u || _top == 0u) {
        fude_zoom_units_put_u64(_w, _whole);
    }
    if(_top > 0u) {
        c8 _s[48];
        rde_snprintf(_s, sizeof _s, "%s%llu/%llu", _whole > 0u ? "-" : "", (unsigned long long)_top, (unsigned long long)_bottom);
        fude_zoom_units_put(_w, _s);
    }
}

// Appends a value past 2^53 of the unit, more than any drawing holds: four figures and a power of ten, so the text
// stays short ("≈ 1.235e+20").
RDE_INTERNAL void fude_zoom_units_put_huge(fude_zoom_units_writer* _w, f64 _x) {
    c8 _s[32];
    rde_snprintf(_s, sizeof _s, "\xE2\x89\x88 %.3e", _x);
    fude_zoom_units_put(_w, _s);
}

// Returns whether _shown is what _x is, near enough to say so without ≈: within 1e-9 of it, or of a unit (a
// millimetre, a degree) when it is smaller, since arithmetic's last bits are not a rounding.
RDE_INTERNAL b8 fude_zoom_units_exact(f64 _shown, f64 _x) {
    return rde_math_abs_f64(_shown - _x) <= 1e-9 * rde_math_max_f64(1.0, rde_math_abs_f64(_x));
}

// Writes into *_n how many steps of 1/_q are in _x, rounded, when they fit a double exactly; the step is made coarser
// till they do (by ten, or by two for a fraction), down to whole ones, and *_q is changed to match. Returns false when
// even whole ones do not fit.
RDE_INTERNAL b8 fude_zoom_units_steps(f64 _x, u64* _q, b8 _fraction, u64* _n) {
    while(*_q > 1u && _x * (f64)*_q >= FUDE_ZOOM_UNITS_EXACT) {
        *_q = _fraction ? *_q / 2u : *_q / 10u;
    }
    if(_x * (f64)*_q >= FUDE_ZOOM_UNITS_EXACT) {
        return false;
    }
    *_n = (u64)rde_math_round_to_i64_f64(_x * (f64)*_q);
    return true;
}

// See the header. The value is rounded to whole steps of the precision first (in inches for feet and inches, so a
// value is split into feet after it is rounded), and the sign, the ≈, the feet, the number and the unit are written
// after.
u32 fude_zoom_units_format(f64 _mm, const fude_zoom_units_style* _style, c8* _out, usize _size) {
    fude_zoom_units_writer _w = { _out, _size, 0u, false };
    if(_size > 0u) {
        _out[0] = 0;
    }
    if(_style == NULL || !rde_math_is_finite_f64(_mm)) {
        return 0;
    }
    const FUDE_ZOOM_UNIT_ _unit = _style->unit < FUDE_ZOOM_UNIT_COUNT ? _style->unit : FUDE_ZOOM_UNIT_MM;
    const b8 _imperial  = _unit == FUDE_ZOOM_UNIT_IN || _unit == FUDE_ZOOM_UNIT_FT;
    const b8 _split     = _imperial && _style->feet_inches;
    const b8 _feet_only = _unit == FUDE_ZOOM_UNIT_FT && !_split;
    // What is counted: inches, unless it is decimal feet or metric.
    const f64 _per = _feet_only ? 304.8 : _imperial ? 25.4 : fude_zoom_unit_mm(_unit);
    u64 _den = 0;
    if(_imperial && !_feet_only && _style->denominator > 0u) {
        _den = 64u;
        while(_den > _style->denominator) {
            _den >>= 1u;
        }
    }
    const b8 _fraction = _den > 0u;
    u64 _q = _fraction ? _den : (u64)FUDE_ZOOM_UNITS_TEN[_style->decimals < FUDE_ZOOM_UNITS_DECIMALS ? _style->decimals : FUDE_ZOOM_UNITS_DECIMALS];
    const f64 _x = rde_math_abs_f64(_mm) / _per;
    u64 _n = 0;
    if(!fude_zoom_units_steps(_x, &_q, _fraction, &_n)) {
        fude_zoom_units_put_huge(&_w, _mm < 0.0 ? -_x : _x);
        fude_zoom_units_put(&_w, _imperial ? (_feet_only ? "'" : "\"") : " ");
        if(!_imperial) {
            fude_zoom_units_put(&_w, fude_zoom_unit_suffix(_unit));
        }
        return (u32)_w.n;
    }
    const f64 _shown = (f64)_n / (f64)_q * _per;
    if(!fude_zoom_units_exact(_shown, rde_math_abs_f64(_mm))) {
        fude_zoom_units_put(&_w, "\xE2\x89\x88 ");
    }
    // No "-0": a value that rounds to nothing has no sign.
    if(_mm < 0.0 && _n > 0u) {
        fude_zoom_units_put(&_w, "-");
    }
    if(_split && _n >= 12u * _q) {
        // Rounded first and split after, so 11.999" is 1', never 0' 12".
        fude_zoom_units_put_u64(&_w, _n / (12u * _q));
        fude_zoom_units_put(&_w, "'");
        _n %= 12u * _q;
        if(_n == 0u) {
            return (u32)_w.n;
        }
        fude_zoom_units_put(&_w, " ");
    }
    if(_fraction) {
        fude_zoom_units_put_fraction(&_w, _n, _q);
    } else {
        fude_zoom_units_put_decimal(&_w, _n, _q);
    }
    if(!_imperial) {
        fude_zoom_units_put(&_w, " ");
    }
    fude_zoom_units_put(&_w, _feet_only ? "'" : _imperial ? "\"" : fude_zoom_unit_suffix(_unit));
    return (u32)_w.n;
}

// See the header. As fude_zoom_units_format, in degrees.
u32 fude_zoom_units_format_angle(f64 _degrees, u8 _decimals, c8* _out, usize _size) {
    fude_zoom_units_writer _w = { _out, _size, 0u, false };
    if(_size > 0u) {
        _out[0] = 0;
    }
    if(!rde_math_is_finite_f64(_degrees)) {
        return 0;
    }
    u64 _q = (u64)FUDE_ZOOM_UNITS_TEN[_decimals < FUDE_ZOOM_UNITS_DECIMALS ? _decimals : FUDE_ZOOM_UNITS_DECIMALS];
    u64 _n = 0;
    if(!fude_zoom_units_steps(rde_math_abs_f64(_degrees), &_q, false, &_n)) {
        fude_zoom_units_put_huge(&_w, _degrees);
        fude_zoom_units_put(&_w, "\xC2\xB0");
        return (u32)_w.n;
    }
    if(!fude_zoom_units_exact((f64)_n / (f64)_q, rde_math_abs_f64(_degrees))) {
        fude_zoom_units_put(&_w, "\xE2\x89\x88 ");
    }
    if(_degrees < 0.0 && _n > 0u) {
        fude_zoom_units_put(&_w, "-");
    }
    fude_zoom_units_put_decimal(&_w, _n, _q);
    fude_zoom_units_put(&_w, "\xC2\xB0");
    return (u32)_w.n;
}
