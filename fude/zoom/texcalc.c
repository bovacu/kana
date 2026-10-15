// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/texcalc.h"
#include "zoom/calc.h"
#include "zoom/tex.h"

// ===========================================================================
// See texcalc.h.
//
// A recursive-descent rewrite in one pass, with no tree: fztc_seq reads a run of LaTeX up to a closing bracket,
// fztc_command one \command and fztc_arg one argument, each writing calc's words to the output as it goes. Any step
// that meets something calc cannot take clears the state's ok, which ends every fztc_seq loop and every fztc_arg at
// once. File-local names start with fztc. tidy and answer work on the rewritten text in buffers of 512 bytes.
// ===========================================================================

// How deep fztc_seq and fztc_arg may be open inside one another (each path of the recursion goes through one of them:
// \frac\frac… and \sqrt\sqrt… through fztc_arg and fztc_command alone). Past it the rewrite fails, before the stack
// runs out. 64 groups inside one another still read.
#define FZTC_DEPTH 128u

// The rewrite's state: where it is in the LaTeX, the output written so far, and whether all of it was calc's.
typedef struct {
    const c8* p;       // The next byte to read.
    const c8* end;     // One past the last byte.
    c8*       out;     // The output buffer, of size bytes, kept ended by a 0.
    usize     size;
    usize     n;       // How many bytes have been written.
    b8        ok;      // False once anything was not calc's, or the output did not fit.
    u32       depth;   // How many fztc_seq and fztc_arg are open inside one another (fztc_enter).
} fztc;

// Enters one more fztc_seq or fztc_arg. Returns false, clearing ok, when the rewrite has failed already (so a failure
// stops every level at once, instead of each reading on after it) or FZTC_DEPTH are open; each true is matched by a
// _t->depth-- as the level ends.
RDE_INTERNAL b8 fztc_enter(fztc* _t) {
    if(!_t->ok || _t->depth >= FZTC_DEPTH) {
        _t->ok = false;
        return false;
    }
    _t->depth++;
    return true;
}

// Appends _s to the output, or clears ok when it does not fit with the 0 after it.
RDE_INTERNAL void fztc_put(fztc* _t, const c8* _s) {
    const usize _l = rde_strlen(_s);
    if(_t->n + _l + 1u > _t->size) {
        _t->ok = false;
        return;
    }
    rde_memcpy(_t->out + _t->n, _s, _l);
    _t->n += _l;
    _t->out[_t->n] = 0;
}

// Steps past spaces, tabs and line breaks.
RDE_INTERNAL void fztc_space(fztc* _t) {
    while(_t->p < _t->end && (*_t->p == ' ' || *_t->p == '\t' || *_t->p == '\n' || *_t->p == '\r')) {
        _t->p++;
    }
}

// True for an ASCII letter: what a command's name is made of.
RDE_INTERNAL b8 fztc_letter(c8 _c) {
    return (_c >= 'a' && _c <= 'z') || (_c >= 'A' && _c <= 'Z');
}

// fztc_seq, fztc_command and fztc_arg call one another.
RDE_INTERNAL void fztc_seq(fztc* _t, c8 _close);
RDE_INTERNAL void fztc_command(fztc* _t);

// Rewrites one argument, always in brackets: {…} read up to its '}', one command, or one character (a UTF-8 character
// whole, up to 4 bytes). Nothing left to read clears ok; so does a failed rewrite before it, or one nested too deep
// (fztc_enter).
RDE_INTERNAL void fztc_arg(fztc* _t) {
    if(!fztc_enter(_t)) {
        return;
    }
    fztc_space(_t);
    if(_t->p >= _t->end) {
        _t->ok = false;
    } else {
        fztc_put(_t, "(");
        if(*_t->p == '{') {
            _t->p++;
            fztc_seq(_t, '}');
            if(_t->p >= _t->end || *_t->p != '}') {
                _t->ok = false;
            } else {
                _t->p++;
            }
        } else if(*_t->p == '\\') {
            fztc_command(_t);
        } else {
            // One character: its bytes, a UTF-8 one whole.
            c8 _one[8] = { 0 };
            usize _k = 0;
            do {
                _one[_k++] = *_t->p++;
            } while(_k < 4u && _t->p < _t->end && ((u8)*_t->p & 0xC0u) == 0x80u);
            fztc_put(_t, _one);
        }
        fztc_put(_t, ")");
    }
    _t->depth--;
}

// Rewrites one command, the input at its backslash: its name is its letters (cut to 23), or the one character after
// the backslash. \left, \right and the delimiters' sizes (\big \Big \bigg \Bigg and their l r m forms: tex.h's
// fude_zoom_tex_sizing) are dropped (with a '.' after them, for no delimiter); the commands in _same become their calc
// words; \frac, \sqrt and the text commands write their arguments as texcalc.h says. Anything else clears ok, other
// names starting big among them (\bigcup, \bigoplus, \bigstar: not a size, and not calc's).
RDE_INTERNAL void fztc_command(fztc* _t) {
    _t->p++;   // The backslash.
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
    // The commands calc takes word for word: \sin becomes "sin " (a space before its argument), \le "<=", \, a space.
    static const struct { const c8* tex; const c8* calc; } _same[] = {
        { "cdot", "*" }, { "times", "*" }, { "ast", "*" }, { "div", "/" }, { "pi", "pi" }, { "theta", "\xCE\xB8" },
        { "sin", "sin " }, { "cos", "cos " }, { "tan", "tan " }, { "sinh", "sinh " }, { "cosh", "cosh " }, { "tanh", "tanh " },
        { "arcsin", "asin " }, { "arccos", "acos " }, { "arctan", "atan " }, { "ln", "ln " }, { "log", "log " }, { "exp", "exp " },
        { ",", " " }, { ";", " " }, { ":", " " }, { "!", " " }, { " ", " " }, { "quad", " " }, { "qquad", " " },
        { "{", "(" }, { "}", ")" }, { "|", "|" }, { "vert", "|" }, { "lvert", "|" }, { "rvert", "|" },
        { "le", "<=" }, { "leq", "<=" }, { "ge", ">=" }, { "geq", ">=" }, { "lt", "<" }, { "gt", ">" },
        { "displaystyle", "" }, { "textstyle", "" },
    };
    if(rde_strcmp(_name, "left") == 0 || rde_strcmp(_name, "right") == 0 || fude_zoom_tex_sizing(_name, NULL, NULL)) {
        fztc_space(_t);
        if(_t->p < _t->end && *_t->p == '.') {
            _t->p++;   // No delimiter; any other is the next thing read, as it is.
        }
        return;
    }
    for(u32 _i = 0; _i < sizeof(_same) / sizeof(_same[0]); _i++) {
        if(rde_strcmp(_name, _same[_i].tex) == 0) {
            fztc_put(_t, _same[_i].calc);
            return;
        }
    }
    if(rde_strcmp(_name, "frac") == 0 || rde_strcmp(_name, "dfrac") == 0 || rde_strcmp(_name, "tfrac") == 0 || rde_strcmp(_name, "cfrac") == 0) {
        fztc_put(_t, "(");
        fztc_arg(_t);
        fztc_put(_t, "/");
        fztc_arg(_t);
        fztc_put(_t, ")");
        return;
    }
    if(rde_strcmp(_name, "sqrt") == 0) {
        fztc_space(_t);
        if(_t->p < _t->end && *_t->p == '[') {
            // Its degree first, read into a buffer of its own by a second state, then written after: (a)^(1/(n)).
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
    if(rde_strcmp(_name, "operatorname") == 0 || rde_strcmp(_name, "mathrm") == 0 || rde_strcmp(_name, "text") == 0 || rde_strcmp(_name, "mathit") == 0 ||
       rde_strcmp(_name, "mathbf") == 0) {
        fztc_arg(_t);   // The name is calc's to know: abs, sign, floor and the rest, when it knows them.
        return;
    }
    _t->ok = false;   // \infty, \sum, \alpha and the rest: not calc's.
}

// Rewrites up to _close (0: up to the end), without taking _close itself. { } become brackets, [ ] too, ^ takes one
// argument, ~ is a space, a backslash starts a command, and _ & ! # $ ' a stray } or a \\ line break clear ok. Every
// other byte is written as it is.
RDE_INTERNAL void fztc_seq(fztc* _t, c8 _close) {
    if(!fztc_enter(_t)) {
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
            _t->ok = false;   // Subscripts, columns, factorials, primes: not calc's.
            break;
        case '\\':
            if(_t->p + 1 < _t->end && _t->p[1] == '\\') {
                _t->ok = false;   // A line break: two formulas.
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

// See the header. True only when all of the LaTeX was read and was calc's.
b8 fude_zoom_texcalc(const c8* _latex, u32 _len, c8* _out, usize _size) {
    if(_size == 0) {
        return false;
    }
    _out[0] = 0;
    fztc _t = { _latex, _latex + _len, _out, _size, 0u, true, 0u };
    fztc_seq(&_t, 0);
    return _t.ok && _t.p >= _t.end;
}

// See the header. Rewrites the LaTeX, splits it at its first relation outside brackets, and has calc parse each side
// and write it back with only the brackets it needs.
b8 fude_zoom_texcalc_tidy(const c8* _latex, u32 _len, c8* _out, usize _size) {
    c8 _calc[512];
    if(_size == 0 || !fude_zoom_texcalc(_latex, _len, _calc, sizeof(_calc))) {
        return false;
    }
    // Its relation, outside brackets: = < > or, from the next byte, <= >=.
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
        rde_memcpy(_rel, _calc + _at, _rl);
        _calc[_at] = 0;
    }
    fude_zoom_calc _c;
    if(!fude_zoom_calc_parse_expr(&_c, _calc) || !fude_zoom_calc_write(&_c, _left, sizeof(_left))) {
        return false;
    }
    if(_rl == 0) {
        return (usize)rde_snprintf(_out, _size, "%s", _left) < _size;
    }
    if(!fude_zoom_calc_parse_expr(&_c, _calc + _at + _rl) || !fude_zoom_calc_write(&_c, _right, sizeof(_right))) {
        return false;
    }
    return (usize)rde_snprintf(_out, _size, "%s %s %s", _left, _rel, _right) < _size;
}

// See the header. Strips trailing spaces, takes off the "=", refuses an equation (any = < > before it), and writes
// what calc makes of the rest with "%.10g".
b8 fude_zoom_texcalc_answer(const c8* _latex, u32 _len, c8* _out, usize _size) {
    while(_len > 0 && (_latex[_len - 1u] == ' ' || _latex[_len - 1u] == '\n' || _latex[_len - 1u] == '\t' || _latex[_len - 1u] == '\r')) {
        _len--;
    }
    if(_len < 2u || _latex[_len - 1u] != '=') {
        return false;
    }
    _len--;
    if(rde_memchr(_latex, '=', _len) != NULL || rde_memchr(_latex, '<', _len) != NULL || rde_memchr(_latex, '>', _len) != NULL) {
        return false;   // An equation already, not a sum to work out.
    }
    c8 _calc[512];
    f64 _v = 0.0;
    if(!fude_zoom_texcalc(_latex, _len, _calc, sizeof(_calc)) || !fude_zoom_calc_number(_calc, &_v)) {
        return false;
    }
    if(rde_math_abs_f64(_v) < 1e-13) {
        _v = 0.0;   // Rounding's dust, such as sin(pi)'s 1.2e-16, is 0.
    }
    rde_snprintf(_out, _size, "%.10g", _v);
    return true;
}
