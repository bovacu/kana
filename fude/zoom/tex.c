// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/tex.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See tex.h. Two passes, as TeX has: the LaTeX read into a tree of atoms (each
// an ordinary symbol, an operator, a binary operation, a relation, an opening,
// a closing, punctuation or an inner formula — what sets the space between
// them), then laid out into boxes in a style (display, text, script,
// scriptscript; cramped or not — a fraction's denominator, a root's inside),
// each box glyphs and rules placed from its origin.
// ===========================================================================

#define FZT_NONE 0xFFFFFFFFu

// Atoms' classes (The TeXbook, ch. 17), and none (a space between them).
enum { FZT_ORD = 0, FZT_OP, FZT_BIN, FZT_REL, FZT_OPEN, FZT_CLOSE, FZT_PUNCT, FZT_INNER, FZT_NOCLASS };

enum {
    FZT_SYM = 0,    // a symbol: cp, its class, its alphabet
    FZT_GROUP,      // {…}: a (an ordinary atom)
    FZT_BIGOP,      // a big operator: cp
    FZT_TEXTOP,     // an operator's name (\sin, \lim, \operatorname): a, a list of its letters
    FZT_FRAC,       // a over b (FZT_NORULE: none between, as \binom)
    FZT_SQRT,       // the root of a, of degree b
    FZT_SCRIPTS,    // a (the base) with b above and c below
    FZT_DELIM,      // a between cp and cp2 (0: none), grown to it
    FZT_BIG,        // a delimiter cp of a size of its own (\big: amount, ems)
    FZT_MATRIX,     // a: its cells' groups, row after row (rows × cols), between cp and cp2
    FZT_ACCENT,     // cp over a (FZT_WIDE: as wide as it)
    FZT_OVERLINE,   // a under a rule
    FZT_UNDERLINE,  // a over one
    FZT_TEXT,       // a: words, upright
    FZT_SPACE,      // amount ems
    FZT_STYLE       // the style from here on: rows (0 display … 3 scriptscript)
};

enum { FZT_LIMITS = 1, FZT_NOLIMITS = 2, FZT_NORULE = 4, FZT_WIDE = 8, FZT_DSTYLE = 16, FZT_TSTYLE = 32 };
enum { FZT_ALIGN_CENTER = 0, FZT_ALIGN_LEFT, FZT_ALIGN_RL };

// Alphabets.
enum { FZT_F_MATH = 0, FZT_F_UP, FZT_F_BOLD, FZT_F_ITALIC, FZT_F_BB, FZT_F_CAL, FZT_F_FRAK, FZT_F_SF, FZT_F_TT };

typedef struct {
    u8  kind, cls, font, flags;
    u32 cp, cp2;
    u32 a, b, c;
    f64 amount;
    u32 rows, cols;
    u8  align;
} fzt_node;

typedef struct {
    const c8* s;
    u32       n, at;
    rde_arr   nodes;   // fzt_node
    rde_arr   lists;   // rde_arr of u32 (node indices) each
    u8        font;    // the alphabet now (\mathbf{…} while in it)
    b8        bracket; // reading \sqrt's [degree]: a ']' ends a list
    u32       unknown;
    u32       depth;
} fzt_parser;

// --- the tree --------------------------------------------------------------------------------------

RDE_INTERNAL u32 fzt_list_new(fzt_parser* _p) {
    rde_arr _l = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr_add(&_p->lists, (any)&_l);
    return (u32)rde_arr_length(&_p->lists) - 1u;
}

RDE_INTERNAL rde_arr* fzt_list(fzt_parser* _p, u32 _l) {
    return &((rde_arr*)_p->lists.memory)[_l];
}

RDE_INTERNAL void fzt_list_add(fzt_parser* _p, u32 _l, u32 _node) {
    rde_arr_add(fzt_list(_p, _l), (any)&_node);
}

RDE_INTERNAL u32 fzt_node_add(fzt_parser* _p, fzt_node _n) {
    rde_arr_add(&_p->nodes, (any)&_n);
    return (u32)rde_arr_length(&_p->nodes) - 1u;
}

RDE_INTERNAL fzt_node* fzt_node_at(fzt_parser* _p, u32 _i) {
    return &((fzt_node*)_p->nodes.memory)[_i];
}

RDE_INTERNAL fzt_node fzt_make(u8 _kind, u8 _cls) {
    fzt_node _n;
    memset(&_n, 0, sizeof(_n));
    _n.kind = _kind;
    _n.cls  = _cls;
    _n.a = _n.b = _n.c = FZT_NONE;
    return _n;
}

// --- reading ---------------------------------------------------------------------------------------

enum { FZT_T_END = 0, FZT_T_CHAR, FZT_T_CMD, FZT_T_OPEN, FZT_T_CLOSE, FZT_T_SUP, FZT_T_SUB, FZT_T_AMP };

typedef struct {
    u8  kind;
    u32 cp;
    c8  name[24];
    u32 next;    // where the source goes on after it
} fzt_tok;

// A UTF-8 character at _at (its codepoint; *_len its bytes).
RDE_INTERNAL u32 fzt_utf8(const c8* _s, u32 _n, u32 _at, u32* _len) {
    const u8 _c = (u8)_s[_at];
    u32 _k = _c < 0x80u ? 1u : (_c >> 5) == 6u ? 2u : (_c >> 4) == 14u ? 3u : (_c >> 3) == 30u ? 4u : 1u;
    if(_at + _k > _n) {
        _k = 1u;
    }
    u32 _cp = _k == 1u ? _c : _k == 2u ? (_c & 0x1Fu) : _k == 3u ? (_c & 0x0Fu) : (_c & 0x07u);
    for(u32 _i = 1; _i < _k; _i++) {
        _cp = (_cp << 6) | ((u8)_s[_at + _i] & 0x3Fu);
    }
    *_len = _k;
    return _cp;
}

RDE_INTERNAL b8 fzt_letter(c8 _c) {
    return (_c >= 'a' && _c <= 'z') || (_c >= 'A' && _c <= 'Z');
}

// The token at the source's place (spaces between skipped: maths ignores them).
RDE_INTERNAL fzt_tok fzt_peek(const fzt_parser* _p) {
    fzt_tok _t;
    memset(&_t, 0, sizeof(_t));
    u32 _at = _p->at;
    while(_at < _p->n && (_p->s[_at] == ' ' || _p->s[_at] == '\t' || _p->s[_at] == '\n' || _p->s[_at] == '\r')) {
        _at++;
    }
    if(_at >= _p->n || _p->s[_at] == 0) {
        _t.kind = FZT_T_END;
        _t.next = _at;
        return _t;
    }
    const c8 _c = _p->s[_at];
    if(_c == '\\') {
        _t.kind = FZT_T_CMD;
        u32 _k = 0, _i = _at + 1u;
        if(_i < _p->n && fzt_letter(_p->s[_i])) {
            while(_i < _p->n && fzt_letter(_p->s[_i])) {
                if(_k + 1u < sizeof(_t.name)) {
                    _t.name[_k++] = _p->s[_i];
                }
                _i++;
            }
        } else if(_i < _p->n) {
            _t.name[_k++] = _p->s[_i++];   // (\, \{ \\ …)
        }
        _t.name[_k] = 0;
        _t.next = _i;
        return _t;
    }
    _t.next = _at + 1u;
    switch(_c) {
    case '{': _t.kind = FZT_T_OPEN;  return _t;
    case '}': _t.kind = FZT_T_CLOSE; return _t;
    case '^': _t.kind = FZT_T_SUP;   return _t;
    case '_': _t.kind = FZT_T_SUB;   return _t;
    case '&': _t.kind = FZT_T_AMP;   return _t;
    default: break;
    }
    u32 _len = 1;
    _t.kind = FZT_T_CHAR;
    _t.cp   = fzt_utf8(_p->s, _p->n, _at, &_len);
    _t.next = _at + _len;
    return _t;
}

RDE_INTERNAL fzt_tok fzt_take(fzt_parser* _p) {
    const fzt_tok _t = fzt_peek(_p);
    _p->at = _t.next;
    return _t;
}

// --- what the commands are -------------------------------------------------------------------------

typedef struct {
    const c8* name;
    u32       cp;
    u8        cls;
} fzt_sym;

RDE_INTERNAL const fzt_sym FZT_SYMS[] = {
    // Greek (the small ones italic, as TeX sets them; the capitals upright)
    { "alpha", 0x3B1, FZT_ORD }, { "beta", 0x3B2, FZT_ORD }, { "gamma", 0x3B3, FZT_ORD }, { "delta", 0x3B4, FZT_ORD },
    { "epsilon", 0x3F5, FZT_ORD }, { "varepsilon", 0x3B5, FZT_ORD }, { "zeta", 0x3B6, FZT_ORD }, { "eta", 0x3B7, FZT_ORD },
    { "theta", 0x3B8, FZT_ORD }, { "vartheta", 0x3D1, FZT_ORD }, { "iota", 0x3B9, FZT_ORD }, { "kappa", 0x3BA, FZT_ORD },
    { "lambda", 0x3BB, FZT_ORD }, { "mu", 0x3BC, FZT_ORD }, { "nu", 0x3BD, FZT_ORD }, { "xi", 0x3BE, FZT_ORD },
    { "omicron", 0x3BF, FZT_ORD }, { "pi", 0x3C0, FZT_ORD }, { "varpi", 0x3D6, FZT_ORD }, { "rho", 0x3C1, FZT_ORD },
    { "varrho", 0x3F1, FZT_ORD }, { "sigma", 0x3C3, FZT_ORD }, { "varsigma", 0x3C2, FZT_ORD }, { "tau", 0x3C4, FZT_ORD },
    { "upsilon", 0x3C5, FZT_ORD }, { "phi", 0x3D5, FZT_ORD }, { "varphi", 0x3C6, FZT_ORD }, { "chi", 0x3C7, FZT_ORD },
    { "psi", 0x3C8, FZT_ORD }, { "omega", 0x3C9, FZT_ORD },
    { "Gamma", 0x393, FZT_ORD }, { "Delta", 0x394, FZT_ORD }, { "Theta", 0x398, FZT_ORD }, { "Lambda", 0x39B, FZT_ORD },
    { "Xi", 0x39E, FZT_ORD }, { "Pi", 0x3A0, FZT_ORD }, { "Sigma", 0x3A3, FZT_ORD }, { "Upsilon", 0x3A5, FZT_ORD },
    { "Phi", 0x3A6, FZT_ORD }, { "Psi", 0x3A8, FZT_ORD }, { "Omega", 0x3A9, FZT_ORD },
    // ordinary symbols
    { "infty", 0x221E, FZT_ORD }, { "partial", 0x2202, FZT_ORD }, { "nabla", 0x2207, FZT_ORD }, { "hbar", 0x210F, FZT_ORD },
    { "ell", 0x2113, FZT_ORD }, { "forall", 0x2200, FZT_ORD }, { "exists", 0x2203, FZT_ORD }, { "nexists", 0x2204, FZT_ORD },
    { "emptyset", 0x2205, FZT_ORD }, { "varnothing", 0x2205, FZT_ORD }, { "angle", 0x2220, FZT_ORD }, { "prime", 0x2032, FZT_ORD },
    { "neg", 0xAC, FZT_ORD }, { "lnot", 0xAC, FZT_ORD }, { "Re", 0x211C, FZT_ORD }, { "Im", 0x2111, FZT_ORD },
    { "aleph", 0x2135, FZT_ORD }, { "wp", 0x2118, FZT_ORD }, { "top", 0x22A4, FZT_ORD }, { "bot", 0x22A5, FZT_ORD },
    { "triangle", 0x25B3, FZT_ORD }, { "square", 0x25A1, FZT_ORD }, { "degree", 0xB0, FZT_ORD }, { "dagger", 0x2020, FZT_ORD },
    { "ldots", 0x2026, FZT_INNER }, { "dots", 0x2026, FZT_INNER }, { "cdots", 0x22EF, FZT_INNER }, { "vdots", 0x22EE, FZT_ORD },
    { "ddots", 0x22F1, FZT_INNER }, { "imath", 0x1D6A4, FZT_ORD }, { "jmath", 0x1D6A5, FZT_ORD }, { "%", '%', FZT_ORD },
    { "$", '$', FZT_ORD }, { "#", '#', FZT_ORD }, { "&", '&', FZT_ORD }, { "_", '_', FZT_ORD }, { "clubsuit", 0x2663, FZT_ORD },
    { "heartsuit", 0x2661, FZT_ORD }, { "checkmark", 0x2713, FZT_ORD },
    // operations
    { "pm", 0xB1, FZT_BIN }, { "mp", 0x2213, FZT_BIN }, { "times", 0xD7, FZT_BIN }, { "div", 0xF7, FZT_BIN },
    { "cdot", 0x22C5, FZT_BIN }, { "ast", 0x2217, FZT_BIN }, { "star", 0x22C6, FZT_BIN }, { "circ", 0x2218, FZT_BIN },
    { "bullet", 0x2219, FZT_BIN }, { "cap", 0x2229, FZT_BIN }, { "cup", 0x222A, FZT_BIN }, { "wedge", 0x2227, FZT_BIN },
    { "land", 0x2227, FZT_BIN }, { "vee", 0x2228, FZT_BIN }, { "lor", 0x2228, FZT_BIN }, { "setminus", 0x2216, FZT_BIN },
    { "oplus", 0x2295, FZT_BIN }, { "ominus", 0x2296, FZT_BIN }, { "otimes", 0x2297, FZT_BIN }, { "odot", 0x2299, FZT_BIN },
    { "oslash", 0x2298, FZT_BIN }, { "sqcap", 0x2293, FZT_BIN }, { "sqcup", 0x2294, FZT_BIN }, { "wr", 0x2240, FZT_BIN },
    // relations
    { "leq", 0x2264, FZT_REL }, { "le", 0x2264, FZT_REL }, { "geq", 0x2265, FZT_REL }, { "ge", 0x2265, FZT_REL },
    { "neq", 0x2260, FZT_REL }, { "ne", 0x2260, FZT_REL }, { "approx", 0x2248, FZT_REL }, { "equiv", 0x2261, FZT_REL },
    { "sim", 0x223C, FZT_REL }, { "simeq", 0x2243, FZT_REL }, { "cong", 0x2245, FZT_REL }, { "propto", 0x221D, FZT_REL },
    { "ll", 0x226A, FZT_REL }, { "gg", 0x226B, FZT_REL }, { "in", 0x2208, FZT_REL }, { "notin", 0x2209, FZT_REL },
    { "ni", 0x220B, FZT_REL }, { "subset", 0x2282, FZT_REL }, { "supset", 0x2283, FZT_REL }, { "subseteq", 0x2286, FZT_REL },
    { "supseteq", 0x2287, FZT_REL }, { "subsetneq", 0x228A, FZT_REL }, { "perp", 0x27C2, FZT_REL }, { "parallel", 0x2225, FZT_REL },
    { "mid", 0x2223, FZT_REL }, { "nmid", 0x2224, FZT_REL }, { "prec", 0x227A, FZT_REL }, { "succ", 0x227B, FZT_REL },
    { "preceq", 0x2AAF, FZT_REL }, { "succeq", 0x2AB0, FZT_REL }, { "asymp", 0x224D, FZT_REL }, { "doteq", 0x2250, FZT_REL },
    { "models", 0x22A7, FZT_REL }, { "vdash", 0x22A2, FZT_REL }, { "dashv", 0x22A3, FZT_REL }, { "coloneqq", 0x2254, FZT_REL },
    { "to", 0x2192, FZT_REL }, { "rightarrow", 0x2192, FZT_REL }, { "leftarrow", 0x2190, FZT_REL }, { "gets", 0x2190, FZT_REL },
    { "leftrightarrow", 0x2194, FZT_REL }, { "Rightarrow", 0x21D2, FZT_REL }, { "Leftarrow", 0x21D0, FZT_REL },
    { "Leftrightarrow", 0x21D4, FZT_REL }, { "implies", 0x27F9, FZT_REL }, { "impliedby", 0x27F8, FZT_REL }, { "iff", 0x27FA, FZT_REL },
    { "mapsto", 0x21A6, FZT_REL }, { "longrightarrow", 0x27F6, FZT_REL }, { "longleftarrow", 0x27F5, FZT_REL },
    { "longmapsto", 0x27FC, FZT_REL }, { "uparrow", 0x2191, FZT_REL }, { "downarrow", 0x2193, FZT_REL }, { "updownarrow", 0x2195, FZT_REL },
    { "Uparrow", 0x21D1, FZT_REL }, { "Downarrow", 0x21D3, FZT_REL }, { "nearrow", 0x2197, FZT_REL }, { "searrow", 0x2198, FZT_REL },
    { "rightleftharpoons", 0x21CC, FZT_REL }, { "hookrightarrow", 0x21AA, FZT_REL }, { "leadsto", 0x219D, FZT_REL },
    // punctuation, delimiters as symbols
    { "colon", ':', FZT_PUNCT }, { "{", '{', FZT_OPEN }, { "}", '}', FZT_CLOSE }, { "lbrace", '{', FZT_OPEN }, { "rbrace", '}', FZT_CLOSE },
    { "langle", 0x27E8, FZT_OPEN }, { "rangle", 0x27E9, FZT_CLOSE }, { "lfloor", 0x230A, FZT_OPEN }, { "rfloor", 0x230B, FZT_CLOSE },
    { "lceil", 0x2308, FZT_OPEN }, { "rceil", 0x2309, FZT_CLOSE }, { "|", 0x2016, FZT_ORD }, { "Vert", 0x2016, FZT_ORD },
    { "vert", '|', FZT_ORD }, { "lvert", '|', FZT_OPEN }, { "rvert", '|', FZT_CLOSE }, { "lVert", 0x2016, FZT_OPEN },
    { "rVert", 0x2016, FZT_CLOSE }, { "backslash", '\\', FZT_ORD },
};

typedef struct {
    const c8* name;
    u32       cp;
    b8        integral;   // (its limits beside it by default)
} fzt_op;

RDE_INTERNAL const fzt_op FZT_OPS[] = {
    { "sum", 0x2211, false }, { "prod", 0x220F, false }, { "coprod", 0x2210, false }, { "int", 0x222B, true }, { "iint", 0x222C, true },
    { "iiint", 0x222D, true }, { "oint", 0x222E, true }, { "oiint", 0x222F, true }, { "bigcup", 0x22C3, false }, { "bigcap", 0x22C2, false },
    { "bigvee", 0x22C1, false }, { "bigwedge", 0x22C0, false }, { "bigoplus", 0x2A01, false }, { "bigotimes", 0x2A02, false },
    { "bigodot", 0x2A00, false }, { "biguplus", 0x2A04, false }, { "bigsqcup", 0x2A06, false },
};

// Operators by name (set upright), and whether their limits go above and below in display.
typedef struct {
    const c8* name;
    b8        limits;
} fzt_named;

RDE_INTERNAL const fzt_named FZT_NAMED[] = {
    { "sin", false }, { "cos", false }, { "tan", false }, { "cot", false }, { "sec", false }, { "csc", false }, { "arcsin", false },
    { "arccos", false }, { "arctan", false }, { "sinh", false }, { "cosh", false }, { "tanh", false }, { "coth", false }, { "log", false },
    { "ln", false }, { "lg", false }, { "exp", false }, { "dim", false }, { "ker", false }, { "hom", false }, { "arg", false },
    { "deg", false }, { "det", true }, { "gcd", true }, { "lim", true }, { "liminf", true }, { "limsup", true }, { "max", true },
    { "min", true }, { "sup", true }, { "inf", true }, { "Pr", true }, { "mod", false },
};

typedef struct {
    const c8* name;
    u32       cp;
    b8        wide;
} fzt_accent;

RDE_INTERNAL const fzt_accent FZT_ACCENTS[] = {
    { "hat", 0x302, false }, { "widehat", 0x302, true }, { "check", 0x30C, false }, { "widecheck", 0x30C, true },
    { "tilde", 0x303, false }, { "widetilde", 0x303, true }, { "bar", 0x304, false }, { "vec", 0x20D7, false },
    { "overrightarrow", 0x20D7, true }, { "overleftarrow", 0x20D6, true }, { "dot", 0x307, false }, { "ddot", 0x308, false },
    { "dddot", 0x20DB, false }, { "breve", 0x306, false }, { "acute", 0x301, false }, { "grave", 0x300, false },
    { "mathring", 0x30A, false }, { "overbrace", 0x23DE, true }, { "underbrace", 0x23DF, true },
};

typedef struct {
    const c8* name;
    f64       ems;
} fzt_space;

RDE_INTERNAL const fzt_space FZT_SPACES[] = {
    { ",", 3.0 / 18.0 }, { "thinspace", 3.0 / 18.0 }, { ":", 4.0 / 18.0 }, { ">", 4.0 / 18.0 }, { "medspace", 4.0 / 18.0 },
    { ";", 5.0 / 18.0 }, { "thickspace", 5.0 / 18.0 }, { "!", -3.0 / 18.0 }, { "negthinspace", -3.0 / 18.0 }, { " ", 0.25 },
    { "quad", 1.0 }, { "qquad", 2.0 }, { "enspace", 0.5 },
};

RDE_INTERNAL b8 fzt_named_is(const c8* _a, const c8* _b) {
    return strcmp(_a, _b) == 0;
}

// --- the parser ------------------------------------------------------------------------------------

RDE_INTERNAL u32 fzt_parse_list(fzt_parser* _p);

// An ASCII or Unicode character's class and what it is set as (− for -, ∗ for *).
RDE_INTERNAL u8 fzt_char_class(u32* _cp) {
    switch(*_cp) {
    case '+': return FZT_BIN;
    case '-': *_cp = 0x2212; return FZT_BIN;
    case '*': *_cp = 0x2217; return FZT_BIN;
    case '=': case '<': case '>': case ':': return FZT_REL;
    case '(': case '[': return FZT_OPEN;
    case ')': case ']': case '!': case '?': return FZT_CLOSE;
    case ',': case ';': return FZT_PUNCT;
    case 0xB1: case 0x2213: case 0xD7: case 0xF7: case 0x22C5: case 0x2218: case 0x2229: case 0x222A: case 0x2227: case 0x2228:
    case 0x2295: case 0x2297: case 0x2212:
        return FZT_BIN;
    case 0x2264: case 0x2265: case 0x2260: case 0x2248: case 0x2261: case 0x223C: case 0x221D: case 0x2208: case 0x2209:
    case 0x2282: case 0x2283: case 0x2286: case 0x2287: case 0x2192: case 0x2190: case 0x2194: case 0x21D2: case 0x21D4:
    case 0x21A6: case 0x226A: case 0x226B:
        return FZT_REL;
    case 0x27E8: case 0x230A: case 0x2308: return FZT_OPEN;
    case 0x27E9: case 0x230B: case 0x2309: return FZT_CLOSE;
    default: return FZT_ORD;
    }
}

// An argument: a {group}, or one token's atom — a list of it.
RDE_INTERNAL u32 fzt_parse_arg(fzt_parser* _p);

// A delimiter after \left, \right, \big… (0: '.', none).
RDE_INTERNAL u32 fzt_parse_delim(fzt_parser* _p) {
    const fzt_tok _t = fzt_take(_p);
    if(_t.kind == FZT_T_CHAR) {
        return _t.cp == '.' ? 0u : _t.cp;
    }
    if(_t.kind == FZT_T_CMD) {
        for(u32 _i = 0; _i < sizeof(FZT_SYMS) / sizeof(FZT_SYMS[0]); _i++) {
            if(fzt_named_is(_t.name, FZT_SYMS[_i].name)) {
                return FZT_SYMS[_i].cp;
            }
        }
        if(fzt_named_is(_t.name, "lbrack")) { return '['; }
        if(fzt_named_is(_t.name, "rbrack")) { return ']'; }
    }
    _p->unknown++;
    return 0u;
}

// Text as written up to its closing brace (its words upright, spaces kept): a list.
RDE_INTERNAL u32 fzt_parse_text(fzt_parser* _p, u8 _font) {
    const u32 _l = fzt_list_new(_p);
    while(_p->at < _p->n && (_p->s[_p->at] == ' ')) {
        _p->at++;
    }
    if(_p->at >= _p->n || _p->s[_p->at] != '{') {
        return _l;
    }
    _p->at++;
    u32 _depth = 1;
    while(_p->at < _p->n && _p->s[_p->at] != 0) {
        const c8 _c = _p->s[_p->at];
        if(_c == '{') {
            _depth++;
            _p->at++;
            continue;
        }
        if(_c == '}') {
            _p->at++;
            if(--_depth == 0u) {
                break;
            }
            continue;
        }
        if(_c == '\\' && _p->at + 1u < _p->n) {
            _p->at++;   // (\{ \} \\ in text: the character)
        }
        u32 _len = 1;
        const u32 _cp = fzt_utf8(_p->s, _p->n, _p->at, &_len);
        _p->at += _len;
        if(_cp == ' ') {
            fzt_node _s = fzt_make(FZT_SPACE, FZT_NOCLASS);
            _s.amount = 0.25;
            fzt_list_add(_p, _l, fzt_node_add(_p, _s));
            continue;
        }
        fzt_node _n = fzt_make(FZT_SYM, FZT_ORD);
        _n.cp   = _cp;
        _n.font = _font;
        fzt_list_add(_p, _l, fzt_node_add(_p, _n));
    }
    return _l;
}

// A list of the name's letters, upright.
RDE_INTERNAL u32 fzt_word(fzt_parser* _p, const c8* _name) {
    const u32 _l = fzt_list_new(_p);
    for(const c8* _c = _name; *_c != 0; _c++) {
        fzt_node _n = fzt_make(FZT_SYM, FZT_ORD);
        _n.cp   = (u8)*_c;
        _n.font = FZT_F_UP;
        fzt_list_add(_p, _l, fzt_node_add(_p, _n));
    }
    return _l;
}

// \begin{env} … \end{env}: its cells, & between them, \\ between rows.
RDE_INTERNAL u32 fzt_parse_env(fzt_parser* _p) {
    c8 _env[24] = { 0 };
    const u32 _name = fzt_parse_text(_p, FZT_F_UP);
    const rde_arr* _letters = fzt_list(_p, _name);
    for(u32 _i = 0; _i < (u32)rde_arr_length(_letters) && _i + 1u < sizeof(_env); _i++) {
        const fzt_node* _c = fzt_node_at(_p, ((const u32*)_letters->memory)[_i]);
        _env[_i] = _c->kind == FZT_SYM && _c->cp < 128u ? (c8)_c->cp : 0;
    }
    if(fzt_named_is(_env, "array")) {
        fzt_parse_text(_p, FZT_F_UP);   // (its columns' letters: each column centred all the same)
    }
    fzt_node _m = fzt_make(FZT_MATRIX, FZT_ORD);
    _m.align = fzt_named_is(_env, "cases") ? FZT_ALIGN_LEFT : (fzt_named_is(_env, "aligned") || fzt_named_is(_env, "align") || fzt_named_is(_env, "align*") ||
                                                              fzt_named_is(_env, "split")) ? FZT_ALIGN_RL : FZT_ALIGN_CENTER;
    if(fzt_named_is(_env, "pmatrix"))      { _m.cp = '(';    _m.cp2 = ')'; }
    else if(fzt_named_is(_env, "bmatrix")) { _m.cp = '[';    _m.cp2 = ']'; }
    else if(fzt_named_is(_env, "Bmatrix")) { _m.cp = '{';    _m.cp2 = '}'; }
    else if(fzt_named_is(_env, "vmatrix")) { _m.cp = '|';    _m.cp2 = '|'; }
    else if(fzt_named_is(_env, "Vmatrix")) { _m.cp = 0x2016; _m.cp2 = 0x2016; }
    else if(fzt_named_is(_env, "cases"))   { _m.cp = '{';    _m.cp2 = 0u; }
    _m.flags = fzt_named_is(_env, "smallmatrix") ? FZT_TSTYLE : 0u;
    const u32 _cells = fzt_list_new(_p);
    u32 _rows = 1, _cols = 0, _col = 0;
    for(u32 _guard = 0; _guard < 4096u; _guard++) {
        fzt_node _g = fzt_make(FZT_GROUP, FZT_ORD);
        _g.a = fzt_parse_list(_p);
        if(_m.align == FZT_ALIGN_RL && (_col % 2u) == 1u) {
            // (aligned's right-hand cells: an empty atom before them, as TeX puts — the relation they start with spaced)
            const u32 _with = fzt_list_new(_p);
            fzt_node _empty = fzt_make(FZT_GROUP, FZT_ORD);
            _empty.a = fzt_list_new(_p);
            fzt_list_add(_p, _with, fzt_node_add(_p, _empty));
            for(u32 _k = 0; _k < (u32)rde_arr_length(fzt_list(_p, _g.a)); _k++) {
                fzt_list_add(_p, _with, ((const u32*)fzt_list(_p, _g.a)->memory)[_k]);
            }
            _g.a = _with;
        }
        _g.rows = _rows - 1u;   // (its row)
        _g.cols = _col;         // (its column)
        fzt_list_add(_p, _cells, fzt_node_add(_p, _g));
        _col++;
        _cols = _col > _cols ? _col : _cols;
        const fzt_tok _t = fzt_peek(_p);
        if(_t.kind == FZT_T_AMP) {
            fzt_take(_p);
            continue;
        }
        if(_t.kind == FZT_T_CMD && (fzt_named_is(_t.name, "\\") || fzt_named_is(_t.name, "cr"))) {
            fzt_take(_p);
            // (a last \\ with nothing after it before \end: no row more)
            const fzt_tok _after = fzt_peek(_p);
            if(_after.kind == FZT_T_CMD && fzt_named_is(_after.name, "end")) {
                continue;
            }
            _rows++;
            _col = 0;
            continue;
        }
        if(_t.kind == FZT_T_CMD && fzt_named_is(_t.name, "end")) {
            fzt_take(_p);
            fzt_parse_text(_p, FZT_F_UP);
        } else {
            _p->unknown++;   // (the source ended, or a stray }: no \end)
            if(_t.kind == FZT_T_CLOSE) {
                fzt_take(_p);
            }
        }
        break;
    }
    _m.a = _cells;
    _m.rows = _rows;
    _m.cols = _cols;
    return fzt_node_add(_p, _m);
}

// One atom (FZT_NONE: what was read changed the last one, or was nothing).
RDE_INTERNAL u32 fzt_parse_atom(fzt_parser* _p, u32 _list) {
    const fzt_tok _t = fzt_take(_p);
    if(_t.kind == FZT_T_OPEN) {
        fzt_node _g = fzt_make(FZT_GROUP, FZT_ORD);
        _g.a = fzt_parse_list(_p);
        if(fzt_peek(_p).kind == FZT_T_CLOSE) {
            fzt_take(_p);
        } else {
            _p->unknown++;
        }
        return fzt_node_add(_p, _g);
    }
    if(_t.kind == FZT_T_CHAR) {
        if(_t.cp == '~') {
            fzt_node _s = fzt_make(FZT_SPACE, FZT_NOCLASS);
            _s.amount = 0.25;
            return fzt_node_add(_p, _s);
        }
        u32 _cp = _t.cp;
        fzt_node _n = fzt_make(FZT_SYM, fzt_char_class(&_cp));
        _n.cp   = _cp;
        _n.font = _p->font;
        return fzt_node_add(_p, _n);
    }
    if(_t.kind != FZT_T_CMD) {
        return FZT_NONE;
    }
    const c8* _name = _t.name;
    for(u32 _i = 0; _i < sizeof(FZT_SYMS) / sizeof(FZT_SYMS[0]); _i++) {
        if(fzt_named_is(_name, FZT_SYMS[_i].name)) {
            fzt_node _n = fzt_make(FZT_SYM, FZT_SYMS[_i].cls);
            _n.cp   = FZT_SYMS[_i].cp;
            _n.font = _p->font;
            return fzt_node_add(_p, _n);
        }
    }
    for(u32 _i = 0; _i < sizeof(FZT_OPS) / sizeof(FZT_OPS[0]); _i++) {
        if(fzt_named_is(_name, FZT_OPS[_i].name)) {
            fzt_node _n = fzt_make(FZT_BIGOP, FZT_OP);
            _n.cp    = FZT_OPS[_i].cp;
            _n.flags = FZT_OPS[_i].integral ? FZT_NOLIMITS : 0u;
            return fzt_node_add(_p, _n);
        }
    }
    for(u32 _i = 0; _i < sizeof(FZT_NAMED) / sizeof(FZT_NAMED[0]); _i++) {
        if(fzt_named_is(_name, FZT_NAMED[_i].name)) {
            fzt_node _n = fzt_make(FZT_TEXTOP, FZT_OP);
            _n.a     = fzt_word(_p, _name);
            _n.flags = FZT_NAMED[_i].limits ? 0u : FZT_NOLIMITS;
            return fzt_node_add(_p, _n);
        }
    }
    for(u32 _i = 0; _i < sizeof(FZT_ACCENTS) / sizeof(FZT_ACCENTS[0]); _i++) {
        if(fzt_named_is(_name, FZT_ACCENTS[_i].name)) {
            fzt_node _n = fzt_make(FZT_ACCENT, FZT_ORD);
            _n.cp    = FZT_ACCENTS[_i].cp;
            _n.flags = FZT_ACCENTS[_i].wide ? FZT_WIDE : 0u;
            _n.a     = fzt_parse_arg(_p);
            return fzt_node_add(_p, _n);
        }
    }
    for(u32 _i = 0; _i < sizeof(FZT_SPACES) / sizeof(FZT_SPACES[0]); _i++) {
        if(fzt_named_is(_name, FZT_SPACES[_i].name)) {
            fzt_node _n = fzt_make(FZT_SPACE, FZT_NOCLASS);
            _n.amount = FZT_SPACES[_i].ems;
            return fzt_node_add(_p, _n);
        }
    }
    if(fzt_named_is(_name, "frac") || fzt_named_is(_name, "dfrac") || fzt_named_is(_name, "tfrac") || fzt_named_is(_name, "cfrac")) {
        fzt_node _n = fzt_make(FZT_FRAC, FZT_INNER);
        _n.flags = _name[0] == 'd' || _name[0] == 'c' ? FZT_DSTYLE : _name[0] == 't' ? FZT_TSTYLE : 0u;
        _n.a = fzt_parse_arg(_p);
        _n.b = fzt_parse_arg(_p);
        return fzt_node_add(_p, _n);
    }
    if(fzt_named_is(_name, "binom") || fzt_named_is(_name, "dbinom") || fzt_named_is(_name, "tbinom")) {
        fzt_node _f = fzt_make(FZT_FRAC, FZT_INNER);
        _f.flags = FZT_NORULE | (_name[0] == 'd' ? FZT_DSTYLE : _name[0] == 't' ? FZT_TSTYLE : 0u);
        _f.a = fzt_parse_arg(_p);
        _f.b = fzt_parse_arg(_p);
        const u32 _inside = fzt_list_new(_p);
        fzt_list_add(_p, _inside, fzt_node_add(_p, _f));
        fzt_node _d = fzt_make(FZT_DELIM, FZT_INNER);
        _d.cp = '(';
        _d.cp2 = ')';
        _d.a = _inside;
        return fzt_node_add(_p, _d);
    }
    if(fzt_named_is(_name, "sqrt")) {
        fzt_node _n = fzt_make(FZT_SQRT, FZT_ORD);
        u32 _at = _p->at;
        while(_at < _p->n && _p->s[_at] == ' ') {
            _at++;
        }
        if(_at < _p->n && _p->s[_at] == '[') {
            _p->at = _at + 1u;
            const b8 _was = _p->bracket;
            _p->bracket = true;
            _n.b = fzt_parse_list(_p);
            _p->bracket = _was;
            if(_p->at < _p->n && _p->s[_p->at] == ']') {
                _p->at++;
            }
        }
        _n.a = fzt_parse_arg(_p);
        return fzt_node_add(_p, _n);
    }
    if(fzt_named_is(_name, "left")) {
        fzt_node _n = fzt_make(FZT_DELIM, FZT_INNER);
        _n.cp = fzt_parse_delim(_p);
        _n.a  = fzt_parse_list(_p);
        const fzt_tok _r = fzt_peek(_p);
        if(_r.kind == FZT_T_CMD && fzt_named_is(_r.name, "right")) {
            fzt_take(_p);
            _n.cp2 = fzt_parse_delim(_p);
        } else {
            _p->unknown++;
        }
        return fzt_node_add(_p, _n);
    }
    if(fzt_named_is(_name, "middle")) {
        fzt_node _n = fzt_make(FZT_SYM, FZT_REL);
        _n.cp = fzt_parse_delim(_p);
        return _n.cp != 0u ? fzt_node_add(_p, _n) : FZT_NONE;
    }
    if(strncmp(_name, "big", 3) == 0 || strncmp(_name, "Big", 3) == 0) {
        // \big \Big \bigg \Bigg, and their l r m forms: a delimiter of that size
        const c8* _rest = _name + 3;
        f64 _size = 1.2;
        u8  _cls  = FZT_ORD;
        if(strncmp(_rest, "g", 1) == 0) {
            _size = _name[0] == 'b' ? 2.4 : 3.0;
            _rest++;
        } else {
            _size = _name[0] == 'b' ? 1.2 : 1.8;
        }
        if(*_rest == 'l') { _cls = FZT_OPEN; }
        else if(*_rest == 'r') { _cls = FZT_CLOSE; }
        else if(*_rest == 'm') { _cls = FZT_REL; }
        else if(*_rest != 0) {
            _p->unknown++;
            return FZT_NONE;
        }
        fzt_node _n = fzt_make(FZT_BIG, _cls);
        _n.cp = fzt_parse_delim(_p);
        _n.amount = _size;
        return fzt_node_add(_p, _n);
    }
    if(fzt_named_is(_name, "begin")) {
        return fzt_parse_env(_p);
    }
    if(fzt_named_is(_name, "overline") || fzt_named_is(_name, "underline")) {
        fzt_node _n = fzt_make(_name[0] == 'o' ? FZT_OVERLINE : FZT_UNDERLINE, FZT_ORD);
        _n.a = fzt_parse_arg(_p);
        return fzt_node_add(_p, _n);
    }
    if(fzt_named_is(_name, "text") || fzt_named_is(_name, "textrm") || fzt_named_is(_name, "mbox") || fzt_named_is(_name, "textnormal") ||
       fzt_named_is(_name, "textit") || fzt_named_is(_name, "textbf")) {
        fzt_node _n = fzt_make(FZT_TEXT, FZT_ORD);
        _n.a = fzt_parse_text(_p, fzt_named_is(_name, "textit") ? FZT_F_ITALIC : fzt_named_is(_name, "textbf") ? FZT_F_BOLD : FZT_F_UP);
        return fzt_node_add(_p, _n);
    }
    if(fzt_named_is(_name, "operatorname")) {
        fzt_node _n = fzt_make(FZT_TEXTOP, FZT_OP);
        _n.a = fzt_parse_text(_p, FZT_F_UP);
        _n.flags = FZT_NOLIMITS;
        return fzt_node_add(_p, _n);
    }
    static const struct { const c8* name; u8 font; } _fonts[] = {
        { "mathrm", FZT_F_UP }, { "mathbf", FZT_F_BOLD }, { "boldsymbol", FZT_F_BOLD }, { "bm", FZT_F_BOLD }, { "mathit", FZT_F_ITALIC },
        { "mathbb", FZT_F_BB }, { "mathcal", FZT_F_CAL }, { "mathscr", FZT_F_CAL }, { "mathfrak", FZT_F_FRAK }, { "mathsf", FZT_F_SF },
        { "mathtt", FZT_F_TT }, { "mathnormal", FZT_F_MATH },
    };
    for(u32 _i = 0; _i < sizeof(_fonts) / sizeof(_fonts[0]); _i++) {
        if(fzt_named_is(_name, _fonts[_i].name)) {
            const u8 _was = _p->font;
            _p->font = _fonts[_i].font;
            fzt_node _g = fzt_make(FZT_GROUP, FZT_ORD);
            _g.a = fzt_parse_arg(_p);
            _p->font = _was;
            return fzt_node_add(_p, _g);
        }
    }
    static const c8* const _styles[4] = { "displaystyle", "textstyle", "scriptstyle", "scriptscriptstyle" };
    for(u32 _i = 0; _i < 4u; _i++) {
        if(fzt_named_is(_name, _styles[_i])) {
            fzt_node _n = fzt_make(FZT_STYLE, FZT_NOCLASS);
            _n.rows = _i;
            return fzt_node_add(_p, _n);
        }
    }
    if(fzt_named_is(_name, "limits") || fzt_named_is(_name, "nolimits")) {
        // (the operator just before)
        rde_arr* _l = fzt_list(_p, _list);
        if(rde_arr_length(_l) > 0u) {
            fzt_node* _op = fzt_node_at(_p, ((u32*)_l->memory)[rde_arr_length(_l) - 1u]);
            if(_op->kind == FZT_BIGOP || _op->kind == FZT_TEXTOP) {
                _op->flags = (u8)((_op->flags & ~(FZT_LIMITS | FZT_NOLIMITS)) | (_name[0] == 'l' ? FZT_LIMITS : FZT_NOLIMITS));
            }
        }
        return FZT_NONE;
    }
    if(fzt_named_is(_name, "not")) {
        // (a negated relation: its own character where it has one)
        const u32 _next = fzt_parse_atom(_p, _list);
        if(_next != FZT_NONE) {
            fzt_node* _n = fzt_node_at(_p, _next);
            static const u32 _pairs[][2] = { { '=', 0x2260 }, { 0x2208, 0x2209 }, { 0x2282, 0x2284 }, { 0x2283, 0x2285 }, { 0x2261, 0x2262 },
                                             { 0x2264, 0x2270 }, { 0x2265, 0x2271 }, { '<', 0x226E }, { '>', 0x226F }, { 0x223C, 0x2241 },
                                             { 0x2248, 0x2249 }, { 0x2286, 0x2288 }, { 0x2287, 0x2289 } };
            for(u32 _i = 0; _i < sizeof(_pairs) / sizeof(_pairs[0]); _i++) {
                if(_n->kind == FZT_SYM && _n->cp == _pairs[_i][0]) {
                    _n->cp = _pairs[_i][1];
                }
            }
        }
        return _next;
    }
    if(fzt_named_is(_name, "\\") || fzt_named_is(_name, "right") || fzt_named_is(_name, "end")) {
        _p->unknown++;   // (out of place: an environment's or \left's)
        return FZT_NONE;
    }
    // Not known: as written, upright.
    _p->unknown++;
    fzt_node _n = fzt_make(FZT_TEXT, FZT_ORD);
    c8 _shown[28];
    snprintf(_shown, sizeof(_shown), "\\%s", _name);
    _n.a = fzt_word(_p, _shown);
    return fzt_node_add(_p, _n);
}

RDE_INTERNAL u32 fzt_parse_arg(fzt_parser* _p) {
    const fzt_tok _t = fzt_peek(_p);
    if(_t.kind == FZT_T_OPEN) {
        fzt_take(_p);
        const u32 _l = fzt_parse_list(_p);
        if(fzt_peek(_p).kind == FZT_T_CLOSE) {
            fzt_take(_p);
        } else {
            _p->unknown++;
        }
        return _l;
    }
    const u32 _l = fzt_list_new(_p);
    if(_t.kind == FZT_T_END || _t.kind == FZT_T_CLOSE || _t.kind == FZT_T_AMP) {
        _p->unknown++;
        return _l;
    }
    const u32 _n = fzt_parse_atom(_p, _l);
    if(_n != FZT_NONE) {
        fzt_list_add(_p, _l, _n);
    }
    return _l;
}

// Scripts on the list's last atom: where it has none of that kind yet, there; else on an empty atom of their own.
RDE_INTERNAL void fzt_attach(fzt_parser* _p, u32 _list, b8 _sup, u32 _script) {
    rde_arr* _l = fzt_list(_p, _list);
    u32 _last = rde_arr_length(_l) > 0u ? ((u32*)_l->memory)[rde_arr_length(_l) - 1u] : FZT_NONE;
    fzt_node* _ln = _last != FZT_NONE ? fzt_node_at(_p, _last) : NULL;
    if(_ln != NULL && _ln->kind == FZT_SCRIPTS && (_sup ? _ln->b : _ln->c) == FZT_NONE) {
        if(_sup) { _ln->b = _script; } else { _ln->c = _script; }
        return;
    }
    fzt_node _s = fzt_make(FZT_SCRIPTS, FZT_ORD);
    const u32 _base = fzt_list_new(_p);
    if(_ln != NULL && _ln->kind != FZT_SCRIPTS && _ln->kind != FZT_SPACE && _ln->kind != FZT_STYLE) {
        fzt_list_add(_p, _base, _last);
        rde_arr_resize(fzt_list(_p, _list), rde_arr_length(fzt_list(_p, _list)) - 1u);
    }
    _s.a = _base;
    if(_sup) { _s.b = _script; } else { _s.c = _script; }
    fzt_list_add(_p, _list, fzt_node_add(_p, _s));
}

RDE_INTERNAL u32 fzt_parse_list(fzt_parser* _p) {
    const u32 _l = fzt_list_new(_p);
    if(++_p->depth > 64u) {
        _p->depth--;
        _p->unknown++;
        _p->at = _p->n;   // (nested past reason: the rest left out)
        return _l;
    }
    for(u32 _guard = 0; _guard < 100000u; _guard++) {
        const fzt_tok _t = fzt_peek(_p);
        if(_t.kind == FZT_T_END || _t.kind == FZT_T_CLOSE || _t.kind == FZT_T_AMP ||
           (_t.kind == FZT_T_CMD && (fzt_named_is(_t.name, "\\") || fzt_named_is(_t.name, "cr") || fzt_named_is(_t.name, "right") ||
                                     fzt_named_is(_t.name, "end"))) ||
           (_p->bracket && _t.kind == FZT_T_CHAR && _t.cp == ']')) {
            break;
        }
        if(_t.kind == FZT_T_SUP || _t.kind == FZT_T_SUB) {
            fzt_take(_p);
            fzt_attach(_p, _l, _t.kind == FZT_T_SUP, fzt_parse_arg(_p));
            continue;
        }
        if(_t.kind == FZT_T_CHAR && _t.cp == '\'') {
            // primes: their own characters after the base (the font draws a prime up where it goes: as a superscript it
            // went up twice) — ′ ″ ‴ for one, two, three
            fzt_take(_p);
            u32 _count = 1;
            while(fzt_peek(_p).kind == FZT_T_CHAR && fzt_peek(_p).cp == '\'' && _count < 3u) {
                fzt_take(_p);
                _count++;
            }
            fzt_node _pr = fzt_make(FZT_SYM, FZT_ORD);
            _pr.cp = _count == 1u ? 0x2032u : _count == 2u ? 0x2033u : 0x2034u;
            fzt_list_add(_p, _l, fzt_node_add(_p, _pr));
            continue;
        }
        const u32 _n = fzt_parse_atom(_p, _l);
        if(_n != FZT_NONE) {
            fzt_list_add(_p, _l, _n);
        }
        if(_p->at >= _p->n && _t.kind == FZT_T_END) {
            break;
        }
    }
    _p->depth--;
    return _l;
}

// --- boxes -----------------------------------------------------------------------------------------

typedef struct {
    b8  rule;
    u32 glyph;
    f64 x, y, s, w, h;   // a glyph: its origin, its size; a rule: its bottom left, its width and height
} fzt_item;

typedef struct {
    rde_arr items;   // fzt_item
    f64     w, h, d;
    f64     italic;  // a lone glyph's italic correction (a superscript further out by it)
    f64     accent;  // where an accent over it sits
    b8      glyph1;  // one glyph (its italic correction, its accent's place are its own)
    b8      op;      // a big operator
    b8      limits;  // ...its limits above and below
} fzt_box;

RDE_INTERNAL fzt_box fzt_box_new(void) {
    fzt_box _b;
    memset(&_b, 0, sizeof(_b));
    _b.items = rde_arr_new(sizeof(fzt_item), rde_memory_allocator_get_default_std());
    return _b;
}

RDE_INTERNAL void fzt_box_drop(fzt_box* _b) {
    rde_arr_free(&_b->items);
}

// _src's items into _dst, moved by (_dx, _dy); _src let go.
RDE_INTERNAL void fzt_put(fzt_box* _dst, fzt_box* _src, f64 _dx, f64 _dy) {
    const fzt_item* _it = (const fzt_item*)_src->items.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_src->items); _i++) {
        fzt_item _c = _it[_i];
        _c.x += _dx;
        _c.y += _dy;
        rde_arr_add(&_dst->items, (any)&_c);
    }
    fzt_box_drop(_src);
}

RDE_INTERNAL void fzt_rule(fzt_box* _b, f64 _x, f64 _y, f64 _w, f64 _h) {
    const fzt_item _r = { true, 0u, _x, _y, 1.0, _w, _h };
    rde_arr_add(&_b->items, (any)&_r);
}

typedef struct {
    u8 level;      // 0 display, 1 text, 2 script, 3 scriptscript
    b8 cramped;
} fzt_style;

typedef struct {
    const fude_zoom_mfont* f;
    fzt_parser*            p;
    f64                    unit;      // an em's font units
    f64                    script, scriptscript;
} fzt_layout;

RDE_INTERNAL f64 fzt_size(const fzt_layout* _l, fzt_style _s) {
    return _s.level < 2u ? 1.0 : _s.level == 2u ? _l->script : _l->scriptscript;
}

// A constant, ems at style _s.
RDE_INTERNAL f64 fzt_c(const fzt_layout* _l, u32 _which, fzt_style _s) {
    return (f64)fude_zoom_mfont_constant(_l->f, _which) / _l->unit * fzt_size(_l, _s);
}

RDE_INTERNAL fzt_style fzt_sup_style(fzt_style _s) {
    return (fzt_style){ (u8)(_s.level < 2u ? 2u : 3u), _s.cramped };
}
RDE_INTERNAL fzt_style fzt_sub_style(fzt_style _s) {
    return (fzt_style){ (u8)(_s.level < 2u ? 2u : 3u), true };
}
RDE_INTERNAL fzt_style fzt_num_style(fzt_style _s) {
    return (fzt_style){ (u8)(_s.level == 0u ? 1u : _s.level == 1u ? 2u : 3u), _s.cramped };
}
RDE_INTERNAL fzt_style fzt_den_style(fzt_style _s) {
    return (fzt_style){ (u8)(_s.level == 0u ? 1u : _s.level == 1u ? 2u : 3u), true };
}

// Codepoint _cp in alphabet _font (the maths alphanumerics' block; its letters' gaps where Unicode has them elsewhere).
RDE_INTERNAL u32 fzt_alphabet(u32 _cp, u8 _font) {
    const b8 _upper = _cp >= 'A' && _cp <= 'Z', _lower = _cp >= 'a' && _cp <= 'z', _digit = _cp >= '0' && _cp <= '9';
    const b8 _greek = _cp >= 0x3B1 && _cp <= 0x3C9;
    switch(_font) {
    case FZT_F_MATH:
    case FZT_F_ITALIC:
        if(_lower) { return _cp == 'h' ? 0x210Eu : 0x1D44Eu + (_cp - 'a'); }
        if(_upper) { return 0x1D434u + (_cp - 'A'); }
        if(_greek && _font == FZT_F_MATH) { return 0x1D6FCu + (_cp - 0x3B1); }
        if(_cp == 0x3F5) { return 0x1D716u; }   // (ϵ)
        if(_cp == 0x3D1) { return 0x1D717u; }   // (ϑ)
        if(_cp == 0x3D5) { return 0x1D719u; }   // (ϕ)
        if(_cp == 0x3F1) { return 0x1D71Au; }   // (ϱ)
        if(_cp == 0x3D6) { return 0x1D71Bu; }   // (ϖ)
        return _cp;
    case FZT_F_BOLD:
        if(_upper) { return 0x1D400u + (_cp - 'A'); }
        if(_lower) { return 0x1D41Au + (_cp - 'a'); }
        if(_digit) { return 0x1D7CEu + (_cp - '0'); }
        if(_greek) { return 0x1D6C2u + (_cp - 0x3B1); }
        return _cp;
    case FZT_F_BB: {
        static const u32 _gaps[][2] = { { 'C', 0x2102 }, { 'H', 0x210D }, { 'N', 0x2115 }, { 'P', 0x2119 }, { 'Q', 0x211A }, { 'R', 0x211D }, { 'Z', 0x2124 } };
        for(u32 _i = 0; _i < sizeof(_gaps) / sizeof(_gaps[0]); _i++) {
            if(_cp == _gaps[_i][0]) { return _gaps[_i][1]; }
        }
        if(_upper) { return 0x1D538u + (_cp - 'A'); }
        if(_lower) { return 0x1D552u + (_cp - 'a'); }
        if(_digit) { return 0x1D7D8u + (_cp - '0'); }
        return _cp;
    }
    case FZT_F_CAL: {
        static const u32 _gaps[][2] = { { 'B', 0x212C }, { 'E', 0x2130 }, { 'F', 0x2131 }, { 'H', 0x210B }, { 'I', 0x2110 }, { 'L', 0x2112 },
                                        { 'M', 0x2133 }, { 'R', 0x211B }, { 'e', 0x212F }, { 'g', 0x210A }, { 'o', 0x2134 } };
        for(u32 _i = 0; _i < sizeof(_gaps) / sizeof(_gaps[0]); _i++) {
            if(_cp == _gaps[_i][0]) { return _gaps[_i][1]; }
        }
        if(_upper) { return 0x1D49Cu + (_cp - 'A'); }
        if(_lower) { return 0x1D4B6u + (_cp - 'a'); }
        return _cp;
    }
    case FZT_F_FRAK: {
        static const u32 _gaps[][2] = { { 'C', 0x212D }, { 'H', 0x210C }, { 'I', 0x2111 }, { 'R', 0x211C }, { 'Z', 0x2128 } };
        for(u32 _i = 0; _i < sizeof(_gaps) / sizeof(_gaps[0]); _i++) {
            if(_cp == _gaps[_i][0]) { return _gaps[_i][1]; }
        }
        if(_upper) { return 0x1D504u + (_cp - 'A'); }
        if(_lower) { return 0x1D51Eu + (_cp - 'a'); }
        return _cp;
    }
    case FZT_F_SF:
        if(_upper) { return 0x1D5A0u + (_cp - 'A'); }
        if(_lower) { return 0x1D5BAu + (_cp - 'a'); }
        if(_digit) { return 0x1D7E2u + (_cp - '0'); }
        return _cp;
    case FZT_F_TT:
        if(_upper) { return 0x1D670u + (_cp - 'A'); }
        if(_lower) { return 0x1D68Au + (_cp - 'a'); }
        if(_digit) { return 0x1D7F6u + (_cp - '0'); }
        return _cp;
    default:
        return _cp;
    }
}

// A glyph's box at size _s (ems), its origin at (0, 0).
RDE_INTERNAL fzt_box fzt_glyph_box(const fzt_layout* _l, u32 _g, f64 _s) {
    fzt_box _b = fzt_box_new();
    i32 _box[4];
    fude_zoom_mfont_box(_l->f, _g, _box);
    const f64 _k = _s / _l->unit;
    _b.w      = fude_zoom_mfont_advance(_l->f, _g) * _k;
    _b.h      = fmax(0.0, _box[3] * _k);
    _b.d      = fmax(0.0, -_box[1] * _k);
    _b.italic = fude_zoom_mfont_italic(_l->f, _g) * _k;
    _b.accent = fude_zoom_mfont_accent_at(_l->f, _g) * _k;
    _b.glyph1 = true;
    const fzt_item _it = { false, _g, 0.0, 0.0, _s, 0.0, 0.0 };
    rde_arr_add(&_b.items, (any)&_it);
    return _b;
}

// The glyph for _cp (a question mark where the font has none).
RDE_INTERNAL u32 fzt_glyph(const fzt_layout* _l, u32 _cp) {
    u32 _g = fude_zoom_mfont_glyph(_l->f, _cp);
    return _g != 0u ? _g : fude_zoom_mfont_glyph(_l->f, '?');
}

// _cp grown along (vertically, or across) to at least _need ems at size _s: its biggest form that is enough, else built of
// its pieces (as long as it takes, the pieces' overlaps shared out evenly), else its biggest form.
RDE_INTERNAL fzt_box fzt_stretch(const fzt_layout* _l, u32 _cp, f64 _need, b8 _vertical, f64 _s) {
    const u32 _g = fzt_glyph(_l, _cp);
    const f64 _k = _s / _l->unit;
    u32 _glyphs[32];
    i32 _sizes[32];
    const u32 _nv = fude_zoom_mfont_variants(_l->f, _g, _vertical, _glyphs, _sizes, 32u);
    for(u32 _i = 0; _i < _nv; _i++) {
        if(_sizes[_i] * _k >= _need) {
            return fzt_glyph_box(_l, _glyphs[_i], _s);
        }
    }
    fude_zoom_mfont_part _parts[16];
    const u32 _np = fude_zoom_mfont_assembly(_l->f, _g, _vertical, _parts, 16u);
    if(_np == 0u) {
        return fzt_glyph_box(_l, _nv > 0u ? _glyphs[_nv - 1u] : _g, _s);
    }
    const f64 _min_overlap = fude_zoom_mfont_min_overlap(_l->f) * _k;
    u32 _repeat = 0;
    u32 _count = 0;
    f64 _full = 0.0;
    for(; _repeat < 64u; _repeat++) {
        _count = 0;
        _full  = 0.0;
        for(u32 _i = 0; _i < _np; _i++) {
            const u32 _times = _parts[_i].extender ? _repeat : 1u;
            _count += _times;
            _full  += _times * _parts[_i].advance * _k;
        }
        if(_count > 0u && _full - (f64)(_count - 1u) * _min_overlap >= _need) {
            break;
        }
    }
    // (the overlap: as much as gets it to _need, no less than the least, no more than the pieces' connectors allow)
    f64 _overlap = _count > 1u ? (_full - _need) / (f64)(_count - 1u) : 0.0;
    f64 _most = 1e30;
    u32 _prev = FZT_NONE;
    for(u32 _i = 0; _i < _np; _i++) {
        const u32 _times = _parts[_i].extender ? _repeat : 1u;
        for(u32 _t = 0; _t < _times; _t++) {
            if(_prev != FZT_NONE) {
                _most = fmin(_most, fmin(_parts[_prev].end, _parts[_i].start) * _k);
            }
            _prev = _i;
        }
    }
    _overlap = fmax(_min_overlap, fmin(_overlap, _most < 1e29 ? _most : _overlap));
    fzt_box _b = fzt_box_new();
    f64 _at = 0.0, _across = 0.0;
    for(u32 _i = 0; _i < _np; _i++) {
        const u32 _times = _parts[_i].extender ? _repeat : 1u;
        for(u32 _t = 0; _t < _times; _t++) {
            const fzt_item _it = { false, _parts[_i].glyph, _vertical ? 0.0 : _at, _vertical ? _at : 0.0, _s, 0.0, 0.0 };
            rde_arr_add(&_b.items, (any)&_it);
            i32 _box[4];
            fude_zoom_mfont_box(_l->f, _parts[_i].glyph, _box);
            _across = fmax(_across, _vertical ? fude_zoom_mfont_advance(_l->f, _parts[_i].glyph) * _k : _box[3] * _k);
            _at += _parts[_i].advance * _k - _overlap;
        }
    }
    const f64 _length = _at + _overlap;
    if(_vertical) {
        _b.w = _across;
        _b.h = _length;
        _b.d = 0.0;
    } else {
        i32 _box[4];
        fude_zoom_mfont_box(_l->f, _parts[0].glyph, _box);
        _b.w = _length;
        _b.h = _across;
        _b.d = fmax(0.0, -_box[1] * _k);
    }
    return _b;
}

// _cp as a delimiter round _h above the axis and _d below it (or a size of its own): grown to cover them, centred on the axis.
RDE_INTERNAL fzt_box fzt_delimiter(const fzt_layout* _l, u32 _cp, f64 _cover, fzt_style _s) {
    const f64 _size = fzt_size(_l, _s);
    if(_cp == 0u) {
        fzt_box _none = fzt_box_new();
        _none.w = 0.12 * _size;   // (TeX's null delimiter space)
        return _none;
    }
    fzt_box _b = fzt_stretch(_l, _cp, _cover, true, _size);
    const f64 _axis = fzt_c(_l, FUDE_ZOOM_MATH_AXIS_HEIGHT, _s);
    const f64 _shift = (_b.h - _b.d) * 0.5 - _axis;   // (lowered by this: its middle on the axis)
    fzt_item* _it = (fzt_item*)_b.items.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_b.items); _i++) {
        _it[_i].y -= _shift;
    }
    _b.h -= _shift;
    _b.d += _shift;
    _b.glyph1 = false;
    return _b;
}

RDE_INTERNAL fzt_box fzt_layout_list(fzt_layout* _l, u32 _list, fzt_style _s);
RDE_INTERNAL fzt_box fzt_layout_node(fzt_layout* _l, u32 _node, fzt_style _s, u8* _cls);

// Big operator: in display its display form (as tall as the font asks, else its biggest), centred on the axis.
RDE_INTERNAL fzt_box fzt_layout_bigop(fzt_layout* _l, const fzt_node* _n, fzt_style _s) {
    const f64 _size = fzt_size(_l, _s);
    const u32 _g = fzt_glyph(_l, _n->cp);
    u32 _use = _g;
    if(_s.level == 0u) {
        u32 _glyphs[16];
        i32 _sizes[16];
        const u32 _nv = fude_zoom_mfont_variants(_l->f, _g, true, _glyphs, _sizes, 16u);
        const i32 _want = fude_zoom_mfont_constant(_l->f, FUDE_ZOOM_MATH_DISPLAY_OPERATOR_MIN_HEIGHT);
        for(u32 _i = 0; _i < _nv; _i++) {
            _use = _glyphs[_i];
            if(_sizes[_i] >= _want) {
                break;
            }
        }
    }
    fzt_box _b = fzt_glyph_box(_l, _use, _size);
    const f64 _axis = fzt_c(_l, FUDE_ZOOM_MATH_AXIS_HEIGHT, _s);
    const f64 _shift = (_b.h - _b.d) * 0.5 - _axis;
    ((fzt_item*)_b.items.memory)[0].y -= _shift;
    _b.h -= _shift;
    _b.d += _shift;
    _b.op = true;
    _b.limits = (_n->flags & FZT_LIMITS) || (_s.level == 0u && !(_n->flags & FZT_NOLIMITS));
    return _b;
}

// Limits above and below an operator (each centred, the upper a half italic correction right, the lower as far left).
RDE_INTERNAL fzt_box fzt_layout_limits(fzt_layout* _l, fzt_box* _base, u32 _sup, u32 _sub, fzt_style _s) {
    fzt_box _b = fzt_box_new();
    fzt_box _up = _sup != FZT_NONE ? fzt_layout_list(_l, _sup, fzt_sup_style(_s)) : fzt_box_new();
    fzt_box _dn = _sub != FZT_NONE ? fzt_layout_list(_l, _sub, fzt_sub_style(_s)) : fzt_box_new();
    const f64 _italic = _base->italic;
    const f64 _w = fmax(_base->w, fmax(_up.w + _italic, _dn.w + _italic));
    f64 _h = _base->h, _d = _base->d;
    const f64 _up_shift = _sup != FZT_NONE ? fmax(fzt_c(_l, FUDE_ZOOM_MATH_UPPER_LIMIT_BASELINE_RISE_MIN, _s), _up.d + fzt_c(_l, FUDE_ZOOM_MATH_UPPER_LIMIT_GAP_MIN, _s)) : 0.0;
    const f64 _dn_shift = _sub != FZT_NONE ? fmax(fzt_c(_l, FUDE_ZOOM_MATH_LOWER_LIMIT_BASELINE_DROP_MIN, _s), _dn.h + fzt_c(_l, FUDE_ZOOM_MATH_LOWER_LIMIT_GAP_MIN, _s)) : 0.0;
    const f64 _bw = _base->w;
    fzt_put(&_b, _base, (_w - _bw) * 0.5, 0.0);
    if(_sup != FZT_NONE) {
        const f64 _y = _h + _up_shift;
        _h = _y + _up.h;
        const f64 _uw = _up.w;
        fzt_put(&_b, &_up, (_w - _uw) * 0.5 + _italic * 0.5, _y);
    } else {
        fzt_box_drop(&_up);
    }
    if(_sub != FZT_NONE) {
        const f64 _y = -(_d + _dn_shift);
        _d = -_y + _dn.d;
        const f64 _dw = _dn.w;
        fzt_put(&_b, &_dn, (_w - _dw) * 0.5 - _italic * 0.5, _y);
    } else {
        fzt_box_drop(&_dn);
    }
    _b.w = _w;
    _b.h = _h;
    _b.d = _d;
    return _b;
}

// Scripts beside a base (TeX's rule 18, with the font's shifts and gaps).
RDE_INTERNAL fzt_box fzt_layout_scripts(fzt_layout* _l, fzt_box* _base, u32 _sup, u32 _sub, fzt_style _s) {
    if(_base->op && _base->limits) {
        return fzt_layout_limits(_l, _base, _sup, _sub, _s);
    }
    fzt_box _b = fzt_box_new();
    fzt_box _up = _sup != FZT_NONE ? fzt_layout_list(_l, _sup, fzt_sup_style(_s)) : fzt_box_new();
    fzt_box _dn = _sub != FZT_NONE ? fzt_layout_list(_l, _sub, fzt_sub_style(_s)) : fzt_box_new();
    const b8 _char = _base->glyph1 && !_base->op;
    f64 _u = _char ? 0.0 : _base->h - fzt_c(_l, FUDE_ZOOM_MATH_SUPERSCRIPT_BASELINE_DROP_MAX, _s);
    f64 _v = _char ? 0.0 : _base->d + fzt_c(_l, FUDE_ZOOM_MATH_SUBSCRIPT_BASELINE_DROP_MIN, _s);
    if(_sup != FZT_NONE) {
        _u = fmax(_u, fzt_c(_l, _s.cramped ? FUDE_ZOOM_MATH_SUPERSCRIPT_SHIFT_UP_CRAMPED : FUDE_ZOOM_MATH_SUPERSCRIPT_SHIFT_UP, _s));
        _u = fmax(_u, _up.d + fzt_c(_l, FUDE_ZOOM_MATH_SUPERSCRIPT_BOTTOM_MIN, _s));
    }
    if(_sub != FZT_NONE) {
        _v = fmax(_v, fzt_c(_l, FUDE_ZOOM_MATH_SUBSCRIPT_SHIFT_DOWN, _s));
        if(_sup == FZT_NONE) {
            _v = fmax(_v, _dn.h - fzt_c(_l, FUDE_ZOOM_MATH_SUBSCRIPT_TOP_MAX, _s));
        }
    }
    if(_sup != FZT_NONE && _sub != FZT_NONE) {
        const f64 _gap = (_u - _up.d) - (_dn.h - _v);
        const f64 _least = fzt_c(_l, FUDE_ZOOM_MATH_SUB_SUPERSCRIPT_GAP_MIN, _s);
        if(_gap < _least) {
            _v += _least - _gap;
            const f64 _room = fzt_c(_l, FUDE_ZOOM_MATH_SUPERSCRIPT_BOTTOM_MAX_WITH_SUBSCRIPT, _s) - (_u - _up.d);
            if(_room > 0.0) {
                _u += _room;
                _v -= _room;
            }
        }
    }
    // (the base as it is; a letter's superscript out past its italic correction, a subscript not — a big operator's
    // advance has its slant in it already: its superscript at its end, its subscript in under it by the slant)
    const f64 _bw = _base->w, _italic = _base->italic;
    const f64 _bh = _base->h, _bd = _base->d;
    fzt_put(&_b, _base, 0.0, 0.0);
    const f64 _space = fzt_c(_l, FUDE_ZOOM_MATH_SPACE_AFTER_SCRIPT, _s);
    f64 _w = _bw;
    f64 _h = _bh, _d = _bd;
    if(_sup != FZT_NONE) {
        const f64 _x = _bw + (_base->op ? 0.0 : _italic);
        _w = fmax(_w, _x + _up.w + _space);
        _h = fmax(_h, _u + _up.h);
        fzt_put(&_b, &_up, _x, _u);
    } else {
        fzt_box_drop(&_up);
    }
    if(_sub != FZT_NONE) {
        const f64 _x = _bw - (_base->op ? _italic : 0.0);
        _w = fmax(_w, _x + _dn.w + _space);
        _d = fmax(_d, _v + _dn.d);
        fzt_put(&_b, &_dn, _x, -_v);
    } else {
        fzt_box_drop(&_dn);
    }
    _b.w = _w;
    _b.h = _h;
    _b.d = _d;
    return _b;
}

RDE_INTERNAL fzt_box fzt_layout_frac(fzt_layout* _l, const fzt_node* _n, fzt_style _s) {
    if(_n->flags & FZT_DSTYLE) { _s.level = 0u; }
    else if(_n->flags & FZT_TSTYLE) { _s.level = _s.level < 1u ? 1u : _s.level; }
    const b8 _display = _s.level == 0u;
    fzt_box _num = fzt_layout_list(_l, _n->a, fzt_num_style(_s));
    fzt_box _den = fzt_layout_list(_l, _n->b, fzt_den_style(_s));
    const f64 _axis = fzt_c(_l, FUDE_ZOOM_MATH_AXIS_HEIGHT, _s);
    const b8  _rule = !(_n->flags & FZT_NORULE);
    const f64 _t    = _rule ? fzt_c(_l, FUDE_ZOOM_MATH_FRACTION_RULE_THICKNESS, _s) : 0.0;
    f64 _up, _down;
    if(_rule) {
        _up   = fzt_c(_l, _display ? FUDE_ZOOM_MATH_FRACTION_NUMERATOR_DISPLAY_STYLE_SHIFT_UP : FUDE_ZOOM_MATH_FRACTION_NUMERATOR_SHIFT_UP, _s);
        _down = fzt_c(_l, _display ? FUDE_ZOOM_MATH_FRACTION_DENOMINATOR_DISPLAY_STYLE_SHIFT_DOWN : FUDE_ZOOM_MATH_FRACTION_DENOMINATOR_SHIFT_DOWN, _s);
        const f64 _gap_up = fzt_c(_l, _display ? FUDE_ZOOM_MATH_FRACTION_NUM_DISPLAY_STYLE_GAP_MIN : FUDE_ZOOM_MATH_FRACTION_NUMERATOR_GAP_MIN, _s);
        const f64 _gap_dn = fzt_c(_l, _display ? FUDE_ZOOM_MATH_FRACTION_DENOM_DISPLAY_STYLE_GAP_MIN : FUDE_ZOOM_MATH_FRACTION_DENOMINATOR_GAP_MIN, _s);
        _up   = fmax(_up, _num.d + _axis + _t * 0.5 + _gap_up);
        _down = fmax(_down, _den.h - (_axis - _t * 0.5) + _gap_dn);
    } else {
        _up   = fzt_c(_l, _display ? FUDE_ZOOM_MATH_STACK_TOP_DISPLAY_STYLE_SHIFT_UP : FUDE_ZOOM_MATH_STACK_TOP_SHIFT_UP, _s);
        _down = fzt_c(_l, _display ? FUDE_ZOOM_MATH_STACK_BOTTOM_DISPLAY_STYLE_SHIFT_DOWN : FUDE_ZOOM_MATH_STACK_BOTTOM_SHIFT_DOWN, _s);
        const f64 _gap = fzt_c(_l, _display ? FUDE_ZOOM_MATH_STACK_DISPLAY_STYLE_GAP_MIN : FUDE_ZOOM_MATH_STACK_GAP_MIN, _s);
        const f64 _have = (_up - _num.d) - (_den.h - _down);
        if(_have < _gap) {
            _up   += (_gap - _have) * 0.5;
            _down += (_gap - _have) * 0.5;
        }
    }
    const f64 _pad = 0.12 * fzt_size(_l, _s);
    const f64 _w = fmax(_num.w, _den.w) + 2.0 * _pad;
    fzt_box _b = fzt_box_new();
    const f64 _nw = _num.w, _dw = _den.w, _nh = _num.h, _dd = _den.d;
    fzt_put(&_b, &_num, (_w - _nw) * 0.5, _up);
    fzt_put(&_b, &_den, (_w - _dw) * 0.5, -_down);
    if(_rule) {
        fzt_rule(&_b, _pad * 0.5, _axis - _t * 0.5, _w - _pad, _t);
    }
    _b.w = _w;
    _b.h = _up + _nh;
    _b.d = _down + _dd;
    return _b;
}

RDE_INTERNAL fzt_box fzt_layout_sqrt(fzt_layout* _l, const fzt_node* _n, fzt_style _s) {
    const f64 _size = fzt_size(_l, _s);
    fzt_box _in = fzt_layout_list(_l, _n->a, (fzt_style){ _s.level, true });
    const f64 _t     = fzt_c(_l, FUDE_ZOOM_MATH_RADICAL_RULE_THICKNESS, _s);
    f64       _gap   = fzt_c(_l, _s.level == 0u ? FUDE_ZOOM_MATH_RADICAL_DISPLAY_STYLE_VERTICAL_GAP : FUDE_ZOOM_MATH_RADICAL_VERTICAL_GAP, _s);
    const f64 _extra = fzt_c(_l, FUDE_ZOOM_MATH_RADICAL_EXTRA_ASCENDER, _s);
    const f64 _need  = _in.h + _in.d + _gap + _t;
    fzt_box _root = fzt_stretch(_l, 0x221A, _need, true, _size);
    // (the sign's top at the rule's: what it has past what is needed shared out as more gap)
    const f64 _tall = _root.h + _root.d;
    if(_tall > _need) {
        _gap += (_tall - _need) * 0.5;
    }
    const f64 _top = _in.h + _gap + _t;              // the rule's top
    const f64 _shift = _top - _root.h;               // (the sign raised by this)
    fzt_box _b = fzt_box_new();
    f64 _x = 0.0;
    f64 _h = _top + _extra, _d = fmax(_in.d, _root.d - _shift);
    if(_n->b != FZT_NONE) {
        fzt_box _deg = fzt_layout_list(_l, _n->b, (fzt_style){ 3u, true });
        const f64 _before = fzt_c(_l, FUDE_ZOOM_MATH_RADICAL_KERN_BEFORE_DEGREE, _s);
        const f64 _after  = fzt_c(_l, FUDE_ZOOM_MATH_RADICAL_KERN_AFTER_DEGREE, _s);
        const f64 _raise  = fude_zoom_mfont_constant(_l->f, FUDE_ZOOM_MATH_RADICAL_DEGREE_BOTTOM_RAISE_PERCENT) / 100.0;
        const f64 _y = -(_root.d - _shift) + _tall * _raise;
        const f64 _dw = _deg.w, _dh = _deg.h;
        fzt_put(&_b, &_deg, _before, _y + _deg.d);
        _h = fmax(_h, _y + _dh);
        _x = fmax(0.0, _before + _dw + _after);
    }
    const f64 _rw = _root.w;
    fzt_put(&_b, &_root, _x, _shift);
    const f64 _iw = _in.w;
    fzt_put(&_b, &_in, _x + _rw, 0.0);
    fzt_rule(&_b, _x + _rw - 0.02 * _size, _top - _t, _iw + 0.02 * _size, _t);
    _b.w = _x + _rw + _iw;
    _b.h = _h;
    _b.d = _d;
    return _b;
}

// What is between \left and \right, the delimiters grown to it (TeX's rule 19: 90% of it, or 0.5 em short at most).
RDE_INTERNAL fzt_box fzt_layout_delim(fzt_layout* _l, fzt_box* _in, u32 _open, u32 _close, fzt_style _s) {
    const f64 _axis = fzt_c(_l, FUDE_ZOOM_MATH_AXIS_HEIGHT, _s);
    const f64 _delta = fmax(_in->h - _axis, _in->d + _axis);
    const f64 _cover = fmax(2.0 * _delta * 0.901, 2.0 * _delta - 0.5 * fzt_size(_l, _s));
    fzt_box _a = fzt_delimiter(_l, _open, _cover, _s);
    fzt_box _c = fzt_delimiter(_l, _close, _cover, _s);
    fzt_box _b = fzt_box_new();
    const f64 _aw = _a.w, _iw = _in->w, _cw = _c.w;
    _b.h = fmax(_in->h, fmax(_a.h, _c.h));
    _b.d = fmax(_in->d, fmax(_a.d, _c.d));
    fzt_put(&_b, &_a, 0.0, 0.0);
    fzt_put(&_b, _in, _aw, 0.0);
    fzt_put(&_b, &_c, _aw + _iw, 0.0);
    _b.w = _aw + _iw + _cw;
    return _b;
}

RDE_INTERNAL fzt_box fzt_layout_matrix(fzt_layout* _l, const fzt_node* _n, fzt_style _s) {
    const fzt_style _cell = { (u8)((_n->flags & FZT_TSTYLE) ? 2u : (_s.level < 1u ? 1u : _s.level)), false };
    const f64 _size = fzt_size(_l, _s);
    const u32 _rows = _n->rows > 0u ? _n->rows : 1u, _cols = _n->cols > 0u ? _n->cols : 1u;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _boxes_arr = rde_arr_new(sizeof(fzt_box), _heap), _cw_arr = rde_arr_new(sizeof(f64), _heap), _rh_arr = rde_arr_new(sizeof(f64), _heap),
            _rd_arr = rde_arr_new(sizeof(f64), _heap), _has_arr = rde_arr_new(sizeof(u8), _heap);
    rde_arr_resize(&_boxes_arr, (usize)_rows * _cols);
    rde_arr_resize(&_has_arr, (usize)_rows * _cols);
    rde_arr_resize(&_cw_arr, _cols);
    rde_arr_resize(&_rh_arr, _rows);
    rde_arr_resize(&_rd_arr, _rows);
    fzt_box* _boxes = (fzt_box*)_boxes_arr.memory;
    u8* _has = (u8*)_has_arr.memory;
    f64* _cw = (f64*)_cw_arr.memory;
    f64* _rh = (f64*)_rh_arr.memory;
    f64* _rd = (f64*)_rd_arr.memory;
    memset(_has, 0, (usize)_rows * _cols);
    for(u32 _i = 0; _i < _cols; _i++) { _cw[_i] = 0.0; }
    for(u32 _i = 0; _i < _rows; _i++) { _rh[_i] = 0.7 * _size; _rd[_i] = 0.3 * _size; }   // (an empty row: a strut's)
    const rde_arr* _cells = fzt_list(_l->p, _n->a);
    for(u32 _k = 0; _k < (u32)rde_arr_length(_cells); _k++) {
        const fzt_node* _g = fzt_node_at(_l->p, ((const u32*)_cells->memory)[_k]);
        if(_g->rows >= _rows || _g->cols >= _cols) {
            continue;
        }
        const u32 _at = _g->rows * _cols + _g->cols;
        _boxes[_at] = fzt_layout_list(_l, _g->a, _cell);
        _has[_at] = 1u;
        _cw[_g->cols] = fmax(_cw[_g->cols], _boxes[_at].w);
        _rh[_g->rows] = fmax(_rh[_g->rows], _boxes[_at].h);
        _rd[_g->rows] = fmax(_rd[_g->rows], _boxes[_at].d);
    }
    // (rows a baseline 1.2 ems apart at least, a gap between each; columns an em apart — aligned's pairs closer, as =)
    const f64 _row_gap = 0.25 * _size, _pitch = 1.2 * _size;
    const f64 _col_gap = _n->align == FZT_ALIGN_RL ? 0.0 : (_n->align == FZT_ALIGN_LEFT ? 1.0 : 0.8) * _size;
    f64 _y = 0.0, _total = 0.0;
    rde_arr _base_arr = rde_arr_new(sizeof(f64), _heap);
    rde_arr_resize(&_base_arr, _rows);
    f64* _base = (f64*)_base_arr.memory;
    for(u32 _r = 0; _r < _rows; _r++) {
        if(_r > 0u) {
            _y -= fmax(_rd[_r - 1u] + _row_gap + _rh[_r], _pitch);
        }
        _base[_r] = _y;
    }
    _total = _rh[0] - (_base[_rows - 1u] - _rd[_rows - 1u]);
    const f64 _axis = fzt_c(_l, FUDE_ZOOM_MATH_AXIS_HEIGHT, _s);
    const f64 _lift = _axis - (_rh[0] + _base[_rows - 1u] - _rd[_rows - 1u]) * 0.5;   // (the whole centred on the axis: row 0's baseline this high)
    fzt_box _b = fzt_box_new();
    f64 _x = 0.0;
    for(u32 _c = 0; _c < _cols; _c++) {
        for(u32 _r = 0; _r < _rows; _r++) {
            const u32 _at = _r * _cols + _c;
            if(!_has[_at]) {
                continue;
            }
            const f64 _w = _boxes[_at].w;
            const b8  _right = _n->align == FZT_ALIGN_RL && (_c % 2u) == 0u;
            const b8  _left  = _n->align == FZT_ALIGN_LEFT || (_n->align == FZT_ALIGN_RL && (_c % 2u) == 1u);
            const f64 _dx = _right ? _cw[_c] - _w : _left ? 0.0 : (_cw[_c] - _w) * 0.5;
            fzt_put(&_b, &_boxes[_at], _x + _dx, _base[_r] + _lift);
        }
        _x += _cw[_c] + (_c + 1u < _cols ? _col_gap : 0.0);
    }
    _b.w = _x;
    _b.h = _rh[0] + _lift;
    _b.d = -(_base[_rows - 1u] + _lift - _rd[_rows - 1u]);
    (void)_total;
    rde_arr_free(&_boxes_arr);
    rde_arr_free(&_cw_arr);
    rde_arr_free(&_rh_arr);
    rde_arr_free(&_rd_arr);
    rde_arr_free(&_has_arr);
    rde_arr_free(&_base_arr);
    if(_n->cp != 0u || _n->cp2 != 0u) {
        // (a little room inside the brackets)
        fzt_box _padded = fzt_box_new();
        const f64 _pad = 0.1 * _size, _bw = _b.w, _bh = _b.h, _bd = _b.d;
        fzt_put(&_padded, &_b, _pad, 0.0);
        _padded.w = _bw + 2.0 * _pad;
        _padded.h = _bh;
        _padded.d = _bd;
        return fzt_layout_delim(_l, &_padded, _n->cp, _n->cp2, _s);
    }
    return _b;
}

// How far a box's ink reaches: up and down, left and right (its glyphs' boxes, its rules).
RDE_INTERNAL void fzt_ink(const fzt_layout* _l, const fzt_box* _b, f64* _top, f64* _bottom, f64* _left, f64* _right) {
    *_top = *_right = -1e30;
    *_bottom = *_left = 1e30;
    const fzt_item* _it = (const fzt_item*)_b->items.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_b->items); _i++) {
        if(_it[_i].rule) {
            *_top    = fmax(*_top, _it[_i].y + _it[_i].h);
            *_bottom = fmin(*_bottom, _it[_i].y);
            *_left   = fmin(*_left, _it[_i].x);
            *_right  = fmax(*_right, _it[_i].x + _it[_i].w);
            continue;
        }
        i32 _box[4];
        if(fude_zoom_mfont_box(_l->f, _it[_i].glyph, _box)) {
            const f64 _k = _it[_i].s / _l->unit;
            *_top    = fmax(*_top, _it[_i].y + _box[3] * _k);
            *_bottom = fmin(*_bottom, _it[_i].y + _box[1] * _k);
            *_left   = fmin(*_left, _it[_i].x + _box[0] * _k);
            *_right  = fmax(*_right, _it[_i].x + _box[2] * _k);
        }
    }
    if(*_top < *_bottom) {
        *_top = *_bottom = 0.0;
    }
    if(*_right < *_left) {
        *_left = *_right = 0.0;
    }
}

RDE_INTERNAL fzt_box fzt_layout_accent(fzt_layout* _l, const fzt_node* _n, fzt_style _s) {
    const f64 _size = fzt_size(_l, _s);
    fzt_box _base = fzt_layout_list(_l, _n->a, (fzt_style){ _s.level, true });
    const b8 _brace_over = _n->cp == 0x23DE, _brace_under = _n->cp == 0x23DF;
    const u32 _ag = fzt_glyph(_l, _n->cp);
    fzt_box _acc = (_n->flags & FZT_WIDE) ? fzt_stretch(_l, _n->cp, _base.w, false, _size) : fzt_glyph_box(_l, _ag, _size);
    // (over its base's accent point — a letter's own where it is one glyph; its middle else —, by its own: a wide one's
    // ink's middle, as its forms have no width of their own)
    f64 _top, _bottom, _left, _right;
    fzt_ink(_l, &_acc, &_top, &_bottom, &_left, &_right);
    const f64 _over = _base.glyph1 && !(_n->flags & FZT_WIDE) ? _base.accent : _base.w * 0.5;
    const f64 _mark = (_n->flags & FZT_WIDE) ? (_left + _right) * 0.5 : fude_zoom_mfont_accent_at(_l->f, _ag) * _size / _l->unit;
    const f64 _bw = _base.w, _bh = _base.h, _bd = _base.d;
    const f64 _gap = (_brace_over || _brace_under ? 0.15 : 0.08) * _size;
    // (a combining accent sits over an x-height letter as it is drawn: raised by what the base stands above that; a
    // brace a gap over or under it)
    const f64 _y = _brace_under ? -(_bd + _gap) - _top : _brace_over ? _bh + _gap - _bottom
                                                       : fmax(0.0, _bh - fzt_c(_l, FUDE_ZOOM_MATH_ACCENT_BASE_HEIGHT, _s));
    fzt_box _b = fzt_box_new();
    const f64 _aw = _acc.w;
    fzt_put(&_b, &_base, 0.0, 0.0);
    fzt_put(&_b, &_acc, _over - _mark, _y);
    _b.w = fmax(_bw, fmax(_over - _mark + _aw, _over - _mark + _right));
    _b.h = fmax(_bh, _y + _top);
    _b.d = fmax(_bd, -(_y + _bottom));
    // (a brace's label above or below it, as an operator's limits)
    _b.op = _b.limits = _brace_over || _brace_under;
    return _b;
}

RDE_INTERNAL fzt_box fzt_layout_bar(fzt_layout* _l, const fzt_node* _n, fzt_style _s, b8 _over) {
    fzt_box _in = fzt_layout_list(_l, _n->a, (fzt_style){ _s.level, _over ? true : _s.cramped });
    const f64 _gap = fzt_c(_l, _over ? FUDE_ZOOM_MATH_OVERBAR_VERTICAL_GAP : FUDE_ZOOM_MATH_UNDERBAR_VERTICAL_GAP, _s);
    const f64 _t   = fzt_c(_l, _over ? FUDE_ZOOM_MATH_OVERBAR_RULE_THICKNESS : FUDE_ZOOM_MATH_UNDERBAR_RULE_THICKNESS, _s);
    const f64 _ex  = fzt_c(_l, _over ? FUDE_ZOOM_MATH_OVERBAR_EXTRA_ASCENDER : FUDE_ZOOM_MATH_UNDERBAR_EXTRA_DESCENDER, _s);
    fzt_box _b = fzt_box_new();
    const f64 _w = _in.w, _h = _in.h, _d = _in.d;
    fzt_put(&_b, &_in, 0.0, 0.0);
    if(_over) {
        fzt_rule(&_b, 0.0, _h + _gap, _w, _t);
        _b.h = _h + _gap + _t + _ex;
        _b.d = _d;
    } else {
        fzt_rule(&_b, 0.0, -(_d + _gap + _t), _w, _t);
        _b.h = _h;
        _b.d = _d + _gap + _t + _ex;
    }
    _b.w = _w;
    return _b;
}

RDE_INTERNAL fzt_box fzt_layout_node(fzt_layout* _l, u32 _i, fzt_style _s, u8* _cls) {
    const fzt_node _n = *fzt_node_at(_l->p, _i);   // (a copy: laying out its children may move the nodes' memory)
    *_cls = _n.cls;
    const f64 _size = fzt_size(_l, _s);
    switch(_n.kind) {
    case FZT_SYM: {
        // (letters italic in maths; small Greek too; the rest as they are)
        u32 _cp = _n.cp;
        const b8 _greek_capital = _cp >= 0x391 && _cp <= 0x3A9;
        if(!(_greek_capital && _n.font == FZT_F_MATH)) {
            _cp = fzt_alphabet(_cp, _n.font);
        }
        u32 _g = fude_zoom_mfont_glyph(_l->f, _cp);
        if(_g == 0u) {
            _g = fzt_glyph(_l, _n.cp);
        }
        return fzt_glyph_box(_l, _g, _size);
    }
    case FZT_GROUP: {
        fzt_box _b = fzt_layout_list(_l, _n.a, _s);
        return _b;
    }
    case FZT_BIGOP:
        return fzt_layout_bigop(_l, &_n, _s);
    case FZT_TEXTOP: {
        fzt_box _b = fzt_layout_list(_l, _n.a, _s);
        _b.glyph1 = false;
        _b.op     = true;
        _b.italic = 0.0;
        _b.limits = (_n.flags & FZT_LIMITS) || (_s.level == 0u && !(_n.flags & FZT_NOLIMITS));
        return _b;
    }
    case FZT_SCRIPTS: {
        // (its base: its one atom's box and class)
        fzt_box _base = fzt_box_new();
        const rde_arr* _bl = fzt_list(_l->p, _n.a);
        u8 _bcls = FZT_ORD;
        if(rde_arr_length(_bl) == 1u) {
            fzt_box_drop(&_base);
            _base = fzt_layout_node(_l, ((const u32*)_bl->memory)[0], _s, &_bcls);
        } else if(rde_arr_length(_bl) > 1u) {
            fzt_box_drop(&_base);
            _base = fzt_layout_list(_l, _n.a, _s);
        }
        *_cls = _bcls == FZT_OP ? FZT_OP : _bcls == FZT_REL ? FZT_REL : _bcls == FZT_BIN ? FZT_BIN : _bcls == FZT_INNER ? FZT_INNER : FZT_ORD;
        return fzt_layout_scripts(_l, &_base, _n.b, _n.c, _s);
    }
    case FZT_FRAC:
        return fzt_layout_frac(_l, &_n, _s);
    case FZT_SQRT:
        return fzt_layout_sqrt(_l, &_n, _s);
    case FZT_DELIM: {
        fzt_box _in = fzt_layout_list(_l, _n.a, _s);
        return fzt_layout_delim(_l, &_in, _n.cp, _n.cp2, _s);
    }
    case FZT_BIG: {
        fzt_box _b = fzt_delimiter(_l, _n.cp, _n.amount * _size, _s);
        return _b;
    }
    case FZT_MATRIX:
        return fzt_layout_matrix(_l, &_n, _s);
    case FZT_ACCENT:
        return fzt_layout_accent(_l, &_n, _s);
    case FZT_OVERLINE:
        return fzt_layout_bar(_l, &_n, _s, true);
    case FZT_UNDERLINE:
        return fzt_layout_bar(_l, &_n, _s, false);
    case FZT_TEXT: {
        fzt_box _b = fzt_layout_list(_l, _n.a, _s);
        _b.glyph1 = false;
        return _b;
    }
    case FZT_SPACE: {
        fzt_box _b = fzt_box_new();
        _b.w = _n.amount * _size;
        return _b;
    }
    default:
        return fzt_box_new();
    }
}

// The space between two atoms (TeX's table: none, thin, medium, thick; +4: not in scripts' styles).
RDE_INTERNAL const u8 FZT_SPACING[8][8] = {
    /* Ord   */ { 0, 1, 2 | 4, 3 | 4, 0, 0, 0, 1 | 4 },
    /* Op    */ { 1, 1, 0, 3 | 4, 0, 0, 0, 1 | 4 },
    /* Bin   */ { 2 | 4, 2 | 4, 0, 0, 2 | 4, 0, 0, 2 | 4 },
    /* Rel   */ { 3 | 4, 3 | 4, 0, 0, 3 | 4, 0, 0, 3 | 4 },
    /* Open  */ { 0, 0, 0, 0, 0, 0, 0, 0 },
    /* Close */ { 0, 1, 2 | 4, 3 | 4, 0, 0, 0, 1 | 4 },
    /* Punct */ { 1 | 4, 1 | 4, 0, 1 | 4, 1 | 4, 1 | 4, 1 | 4, 1 | 4 },
    /* Inner */ { 1 | 4, 1, 2 | 4, 3 | 4, 1 | 4, 0, 1 | 4, 1 | 4 },
};

RDE_INTERNAL fzt_box fzt_layout_list(fzt_layout* _l, u32 _list, fzt_style _s) {
    fzt_box _out = fzt_box_new();
    if(_list == FZT_NONE) {
        return _out;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _boxes_arr = rde_arr_new(sizeof(fzt_box), _heap), _cls_arr = rde_arr_new(sizeof(u8), _heap), _sty_arr = rde_arr_new(sizeof(fzt_style), _heap);
    const u32 _count = (u32)rde_arr_length(fzt_list(_l->p, _list));
    for(u32 _k = 0; _k < _count; _k++) {
        const u32 _i = ((const u32*)fzt_list(_l->p, _list)->memory)[_k];
        const fzt_node* _n = fzt_node_at(_l->p, _i);
        if(_n->kind == FZT_STYLE) {
            _s.level = (u8)_n->rows;
            continue;
        }
        u8 _cls = FZT_ORD;
        fzt_box _b = fzt_layout_node(_l, _i, _s, &_cls);
        rde_arr_add(&_boxes_arr, (any)&_b);
        rde_arr_add(&_cls_arr, (any)&_cls);
        rde_arr_add(&_sty_arr, (any)&_s);
    }
    const u32 _n = (u32)rde_arr_length(&_boxes_arr);
    fzt_box* _boxes = (fzt_box*)_boxes_arr.memory;
    u8* _cls = (u8*)_cls_arr.memory;
    const fzt_style* _sty = (const fzt_style*)_sty_arr.memory;
    // (an operation with nothing to work on is ordinary: first, or after an operation, operator, relation, opening or
    // punctuation; or before a relation, closing or punctuation)
    u8 _prev = FZT_NOCLASS;
    u32 _prev_at = FZT_NONE;
    for(u32 _k = 0; _k < _n; _k++) {
        if(_cls[_k] == FZT_NOCLASS) {
            continue;
        }
        if(_cls[_k] == FZT_BIN && (_prev == FZT_NOCLASS || _prev == FZT_BIN || _prev == FZT_OP || _prev == FZT_REL || _prev == FZT_OPEN || _prev == FZT_PUNCT)) {
            _cls[_k] = FZT_ORD;
        }
        if((_cls[_k] == FZT_REL || _cls[_k] == FZT_CLOSE || _cls[_k] == FZT_PUNCT) && _prev == FZT_BIN) {
            _cls[_prev_at] = FZT_ORD;
        }
        _prev = _cls[_k];
        _prev_at = _k;
    }
    if(_prev == FZT_BIN) {
        _cls[_prev_at] = FZT_ORD;
    }
    f64 _x = 0.0;
    _prev = FZT_NOCLASS;
    for(u32 _k = 0; _k < _n; _k++) {
        if(_cls[_k] != FZT_NOCLASS && _prev != FZT_NOCLASS) {
            const u8 _code = FZT_SPACING[_prev][_cls[_k]];
            if(!((_code & 4u) && _sty[_k].level >= 2u)) {
                static const f64 _mu[4] = { 0.0, 3.0, 4.0, 5.0 };
                _x += _mu[_code & 3u] / 18.0 * fzt_size(_l, _sty[_k]);
            }
        }
        _out.h = fmax(_out.h, _boxes[_k].h);
        _out.d = fmax(_out.d, _boxes[_k].d);
        const f64 _w = _boxes[_k].w + (_boxes[_k].glyph1 ? _boxes[_k].italic : 0.0);
        if(_n == 1u) {
            _out.italic = _boxes[_k].italic;
            _out.accent = _boxes[_k].accent;
            _out.glyph1 = _boxes[_k].glyph1;
            _out.op     = _boxes[_k].op;
            _out.limits = _boxes[_k].limits;
        }
        fzt_put(&_out, &_boxes[_k], _x, 0.0);
        _x += _n == 1u && _out.glyph1 ? _w - _out.italic : _w;
        if(_cls[_k] != FZT_NOCLASS) {
            _prev = _cls[_k];
        }
    }
    _out.w = _x;
    rde_arr_free(&_boxes_arr);
    rde_arr_free(&_cls_arr);
    rde_arr_free(&_sty_arr);
    return _out;
}

// --- the whole -------------------------------------------------------------------------------------

void fude_zoom_tex_box_init(fude_zoom_tex_box* _b) {
    memset(_b, 0, sizeof(*_b));
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _b->glyphs = rde_arr_new(sizeof(fude_zoom_tex_glyph), _heap);
    _b->rules  = rde_arr_new(sizeof(fude_zoom_tex_rule), _heap);
}

void fude_zoom_tex_box_free(fude_zoom_tex_box* _b) {
    rde_arr_free(&_b->glyphs);
    rde_arr_free(&_b->rules);
    memset(_b, 0, sizeof(*_b));
}

void fude_zoom_tex_set(const fude_zoom_mfont* _font, const c8* _latex, u32 _len, b8 _display, fude_zoom_tex_box* _out) {
    rde_arr_clear(&_out->glyphs);
    rde_arr_clear(&_out->rules);
    _out->width = _out->height = _out->depth = 0.0;
    _out->unknown = 0u;
    if(_font == NULL || _font->size == 0u || _latex == NULL) {
        return;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fzt_parser _p;
    memset(&_p, 0, sizeof(_p));
    _p.s = _latex;
    _p.n = _len;
    _p.nodes = rde_arr_new(sizeof(fzt_node), _heap);
    _p.lists = rde_arr_new(sizeof(rde_arr), _heap);
    u32 _top = fzt_parse_list(&_p);
    // (what stopped it before the end: a stray } or & or \\ — counted, and the rest read on)
    for(u32 _guard = 0; _guard < 1000u && _p.at < _p.n; _guard++) {
        const fzt_tok _t = fzt_take(&_p);
        if(_t.kind == FZT_T_END) {
            break;
        }
        _p.unknown++;
        const u32 _more = fzt_parse_list(&_p);
        const rde_arr* _ml = fzt_list(&_p, _more);
        for(u32 _k = 0; _k < (u32)rde_arr_length(_ml); _k++) {
            fzt_list_add(&_p, _top, ((const u32*)fzt_list(&_p, _more)->memory)[_k]);
        }
    }
    fzt_layout _l = { _font, &_p, (f64)_font->units_per_em, 0.6, 0.5 };
    const i32 _sp = fude_zoom_mfont_constant(_font, FUDE_ZOOM_MATH_SCRIPT_PERCENT), _ssp = fude_zoom_mfont_constant(_font, FUDE_ZOOM_MATH_SCRIPTSCRIPT_PERCENT);
    _l.script       = _sp > 0 ? _sp / 100.0 : 0.7;
    _l.scriptscript = _ssp > 0 ? _ssp / 100.0 : 0.5;
    fzt_box _b = fzt_layout_list(&_l, _top, (fzt_style){ (u8)(_display ? 0u : 1u), false });
    const fzt_item* _it = (const fzt_item*)_b.items.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_b.items); _i++) {
        if(_it[_i].rule) {
            const fude_zoom_tex_rule _r = { _it[_i].x, _it[_i].y, _it[_i].w, _it[_i].h };
            rde_arr_add(&_out->rules, (any)&_r);
        } else {
            const fude_zoom_tex_glyph _g = { _it[_i].glyph, _it[_i].x, _it[_i].y, _it[_i].s };
            rde_arr_add(&_out->glyphs, (any)&_g);
        }
    }
    _out->width   = _b.w;
    _out->height  = _b.h;
    _out->depth   = _b.d;
    _out->unknown = _p.unknown;
    fzt_box_drop(&_b);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_p.lists); _i++) {
        rde_arr_free(&((rde_arr*)_p.lists.memory)[_i]);
    }
    rde_arr_free(&_p.lists);
    rde_arr_free(&_p.nodes);
}
