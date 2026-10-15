// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/mathread.h"
#include "zoom/handmath.h"

// ===========================================================================
// See mathread.h.
//
// Internal names start fzmr_ (FZMR_ for constants). The file goes bottom up: the clouds and $P's matching
// (fzmr_cloud_of, fzmr_match, fzmr_classify); the boxes and nodes; the layout (fzmr_region, which reads what a
// fraction, a root, a limit or a script holds as a region of its own, recursively); and fude_zoom_mathread, which
// cuts the strokes into symbols, reads what their size and place say, and lays them out. A symbol, or a part made of
// several, is a fzmr_node: once it becomes a part its cp is 0 and its LaTeX is in its tex, and a node taken into a
// part is marked used. Every size and distance is measured against how tall a symbol is written (fzmr_layout's h),
// so the reader works at any size of writing.
// ===========================================================================

#define FZMR_N       32u     // points in a cloud: what $P matches
#define FZMR_GROUP   3u      // strokes a symbol at most
#define FZMR_MOST    160u    // strokes read at most
#define FZMR_TEX     1024u   // a part's LaTeX at most
#define FZMR_SYMBOL  0.9     // what a symbol costs over its distance (fewer, better-fitting symbols over many poor ones)

// The symbols it knows: their characters and how LaTeX writes them. Letters whose capital is the same shape (c o s u v
// w x z k p) known by their small one, a capital by its height.
typedef struct {
    u32       cp;    // its character (U+E000 and U+E001: handmath.h's second shapes for ∫ and √)
    const c8* tex;   // its LaTeX, with a space after a command
} fzmr_sym;

// Each one's template is drawn from handmath.h's shape for it (fzmr_make_templates). √ has no LaTeX of its own here:
// fzmr_roots writes the \sqrt{…}.
RDE_INTERNAL const fzmr_sym FZMR_SYMS[] = {
    { '0', "0" }, { '1', "1" }, { '2', "2" }, { '3', "3" }, { '4', "4" }, { '5', "5" }, { '6', "6" }, { '7', "7" }, { '8', "8" }, { '9', "9" },
    { 'a', "a" }, { 'b', "b" }, { 'c', "c" }, { 'd', "d" }, { 'e', "e" }, { 'f', "f" }, { 'g', "g" }, { 'h', "h" }, { 'i', "i" }, { 'j', "j" },
    { 'k', "k" }, { 'l', "l" }, { 'm', "m" }, { 'n', "n" }, { 'o', "o" }, { 'p', "p" }, { 'q', "q" }, { 'r', "r" }, { 's', "s" }, { 't', "t" },
    { 'u', "u" }, { 'v', "v" }, { 'w', "w" }, { 'x', "x" }, { 'y', "y" }, { 'z', "z" },
    { 'A', "A" }, { 'B', "B" }, { 'D', "D" }, { 'E', "E" }, { 'F', "F" }, { 'G', "G" }, { 'H', "H" }, { 'L', "L" }, { 'M', "M" }, { 'N', "N" },
    { 'Q', "Q" }, { 'R', "R" }, { 'T', "T" }, { 'Y', "Y" },
    { 0x03B1, "\\alpha " }, { 0x03B2, "\\beta " }, { 0x03B3, "\\gamma " }, { 0x03B4, "\\delta " }, { 0x03B8, "\\theta " },
    { 0x03BB, "\\lambda " }, { 0x03BC, "\\mu " }, { 0x03C0, "\\pi " }, { 0x03C3, "\\sigma " }, { 0x03C6, "\\phi " }, { 0x03C9, "\\omega " },
    { 0x0394, "\\Delta " }, { 0x03A9, "\\Omega " },
    { '+', "+" }, { 0x2212, "-" }, { '=', "=" }, { '<', "<" }, { '>', ">" }, { 0x2264, "\\le " }, { 0x2265, "\\ge " }, { 0x2260, "\\neq " },
    { 0x00B1, "\\pm " }, { 0x00F7, "\\div " }, { '(', "(" }, { ')', ")" }, { '[', "[" }, { ']', "]" }, { '{', "\\{" }, { '}', "\\}" },
    { '/', "/" }, { '!', "!" }, { 0x221E, "\\infty " }, { 0x2211, "\\sum " }, { 0x222B, "\\int " }, { 0x221A, "" }, { 0x2202, "\\partial " },
    { 0x2207, "\\nabla " }, { 0x2192, "\\to " }, { 0x2208, "\\in " },
    { 0xE000, "\\int " }, { 0xE001, "" },   // (an integral written small, a root written with its bar: handmath.h's)
};
#define FZMR_SYMS_COUNT (sizeof(FZMR_SYMS) / sizeof(FZMR_SYMS[0]))   // how many symbols it knows

// Characters it reads by size, place or what is round them (not by shape alone).
#define FZMR_DOT    0x2E      // .
#define FZMR_CDOT   0x22C5    // ⋅
#define FZMR_BAR    0x7C      // |
#define FZMR_TIMES  0x00D7    // ×
#define FZMR_PRIME  0x2032    // ′

// --- clouds -----------------------------------------------------------------------------------------

// A symbol's strokes as $P compares them: FZMR_N points, scaled and centred by fzmr_cloud_of.
typedef struct {
    fude_zoom_v2 p[FZMR_N];   // its points, scaled and centred
    f64          aspect;      // its width over its height, as written (log: 0 square)
} fzmr_cloud;

// Makes _n strokes' points into a cloud: FZMR_N points spread evenly along the strokes (along each stroke's own length,
// not the pen's jumps between them), scaled to a unit square each way on its own (see below), with their mean at the
// origin. Returns false when there are no points.
RDE_INTERNAL b8 fzmr_cloud_of(const fude_zoom_v2* _p, const u32* _ends, u32 _n, fzmr_cloud* _out) {
    f64 _total = 0.0;
    u32 _from = 0;
    for(u32 _s = 0; _s < _n; _s++) {
        for(u32 _i = _from + 1u; _i < _ends[_s]; _i++) {
            _total += rde_math_hypot_f64(_p[_i].x - _p[_i - 1u].x, _p[_i].y - _p[_i - 1u].y);
        }
        _from = _ends[_s];
    }
    u32 _made = 0;
    if(_total <= 1e-12) {
        // (a dot, or dots: their points as they are, repeated)
        const u32 _all = _ends[_n - 1u];
        for(u32 _i = 0; _i < FZMR_N && _all > 0u; _i++) {
            _out->p[_i] = _p[(_i * _all) / FZMR_N];
        }
        _made = _all > 0u ? FZMR_N : 0u;
    } else {
        const f64 _step = _total / (f64)(FZMR_N - 1u);
        f64 _carry = 0.0;   // (how far along since the last point taken)
        _from = 0;
        for(u32 _s = 0; _s < _n && _made < FZMR_N; _s++) {
            if(_ends[_s] <= _from) {
                continue;
            }
            fude_zoom_v2 _prev = _p[_from];
            if(_made == 0u || _carry >= _step * 0.5) {
                _out->p[_made++] = _prev;   // (a stroke's start taken: its own)
                _carry = 0.0;
            }
            for(u32 _i = _from + 1u; _i < _ends[_s] && _made < FZMR_N; _i++) {
                fude_zoom_v2 _cur = _p[_i];
                f64 _d = rde_math_hypot_f64(_cur.x - _prev.x, _cur.y - _prev.y);
                while(_carry + _d >= _step && _made < FZMR_N && _d > 0.0) {
                    const f64 _t = (_step - _carry) / _d;
                    const fude_zoom_v2 _q = { _prev.x + (_cur.x - _prev.x) * _t, _prev.y + (_cur.y - _prev.y) * _t };
                    _out->p[_made++] = _q;
                    _prev  = _q;
                    _d     = rde_math_hypot_f64(_cur.x - _prev.x, _cur.y - _prev.y);
                    _carry = 0.0;
                }
                _carry += _d;
                _prev = _cur;
            }
            _from = _ends[_s];
        }
        while(_made < FZMR_N) {   // (rounding: the last point again)
            _out->p[_made] = _p[_ends[_n - 1u] - 1u];
            _made++;
        }
    }
    if(_made == 0u) {
        return false;
    }
    f64 _x0 = 1e300, _y0 = 1e300, _x1 = -1e300, _y1 = -1e300, _cx = 0.0, _cy = 0.0;
    for(u32 _i = 0; _i < FZMR_N; _i++) {
        _x0 = rde_math_min_f64(_x0, _out->p[_i].x); _x1 = rde_math_max_f64(_x1, _out->p[_i].x);
        _y0 = rde_math_min_f64(_y0, _out->p[_i].y); _y1 = rde_math_max_f64(_y1, _out->p[_i].y);
    }
    // (each way scaled on its own, so a hand's = or x may be as wide as it likes; but a way under 0.35 of the size is
    // not stretched, so a minus stays a line and a 1 upright; how much wider than tall it was is kept in aspect)
    const f64 _size = rde_math_max_f64(rde_math_max_f64(_x1 - _x0, _y1 - _y0), 1e-12);
    const f64 _sx = rde_math_max_f64(_x1 - _x0, 0.35 * _size), _sy = rde_math_max_f64(_y1 - _y0, 0.35 * _size);
    _out->aspect = rde_math_log_f64(_sx / _sy);
    for(u32 _i = 0; _i < FZMR_N; _i++) {
        _out->p[_i].x = (_out->p[_i].x - _x0) / _sx;
        _out->p[_i].y = (_out->p[_i].y - _y0) / _sy;
        _cx += _out->p[_i].x;
        _cy += _out->p[_i].y;
    }
    _cx /= (f64)FZMR_N;
    _cy /= (f64)FZMR_N;
    for(u32 _i = 0; _i < FZMR_N; _i++) {
        _out->p[_i].x -= _cx;
        _out->p[_i].y -= _cy;
    }
    return true;
}

// $P's distance from _a to _b starting at _start: each of _a's points matched to the nearest of _b's not matched yet,
// the earlier ones weighing more.
RDE_INTERNAL f64 fzmr_cloud_distance(const fzmr_cloud* _a, const fzmr_cloud* _b, u32 _start) {
    b8 _matched[FZMR_N] = { false };
    f64 _sum = 0.0;
    u32 _i = _start;
    do {
        f64 _best = 1e300;
        u32 _index = 0;
        for(u32 _j = 0; _j < FZMR_N; _j++) {
            if(!_matched[_j]) {
                const f64 _dx = _a->p[_i].x - _b->p[_j].x, _dy = _a->p[_i].y - _b->p[_j].y;
                const f64 _d = _dx * _dx + _dy * _dy;
                if(_d < _best) {
                    _best  = _d;
                    _index = _j;
                }
            }
        }
        _matched[_index] = true;
        const f64 _weight = 1.0 - (f64)((_i + FZMR_N - _start) % FZMR_N) / (f64)FZMR_N;
        _sum += _weight * rde_math_sqrt_f64(_best);
        _i = (_i + 1u) % FZMR_N;
    } while(_i != _start);
    return _sum;
}

// Returns how far apart two clouds are: $P's least distance, either way round, from a start every √FZMR_N points, plus
// up to 1.2 for how much their aspects differ.
RDE_INTERNAL f64 fzmr_match(const fzmr_cloud* _a, const fzmr_cloud* _b) {
    const u32 _step = (u32)rde_math_floor_f64(rde_math_pow_f64((f64)FZMR_N, 0.5));
    f64 _best = 1e300;
    for(u32 _i = 0; _i < FZMR_N; _i += _step) {
        _best = rde_math_min_f64(_best, rde_math_min_f64(fzmr_cloud_distance(_a, _b, _i), fzmr_cloud_distance(_b, _a, _i)));
    }
    return _best + 0.8 * rde_math_min_f64(rde_math_abs_f64(_a->aspect - _b->aspect), 1.5);   // (an o not a 0, a = not a square)
}

// The symbols' clouds (made once).
RDE_INTERNAL fzmr_cloud fzmr_templates[FZMR_SYMS_COUNT];
RDE_INTERNAL b8         fzmr_template_ok[FZMR_SYMS_COUNT];   // whether handmath.h had a shape for it
RDE_INTERNAL b8         fzmr_ready = false;                  // whether they are made

// Makes the symbols' clouds from handmath.h's shapes, the first time only. A symbol with no shape is left out
// (fzmr_template_ok false).
RDE_INTERNAL void fzmr_make_templates(void) {
    if(fzmr_ready) {
        return;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap), _ends = rde_arr_new(sizeof(u32), _heap);
    for(u32 _i = 0; _i < FZMR_SYMS_COUNT; _i++) {
        rde_arr_clear(&_pts);
        rde_arr_clear(&_ends);
        const u32 _n = fude_zoom_handmath_shape(FZMR_SYMS[_i].cp, &_pts, &_ends);
        fzmr_template_ok[_i] = _n > 0u && fzmr_cloud_of((const fude_zoom_v2*)_pts.memory, (const u32*)_ends.memory, _n, &fzmr_templates[_i]);
    }
    rde_arr_free(&_pts);
    rde_arr_free(&_ends);
    fzmr_ready = true;
}

// Returns the index in FZMR_SYMS of the symbol _p's _n strokes are most like, its distance in *_distance; -1 (the
// distance 1e300) when they make no cloud.
RDE_INTERNAL i32 fzmr_classify(const fude_zoom_v2* _p, const u32* _ends, u32 _n, f64* _distance) {
    fzmr_make_templates();
    *_distance = 1e300;
    fzmr_cloud _c;
    if(_n == 0u || !fzmr_cloud_of(_p, _ends, _n, &_c)) {
        return -1;
    }
    i32 _best = -1;
    for(u32 _i = 0; _i < FZMR_SYMS_COUNT; _i++) {
        if(!fzmr_template_ok[_i]) {
            continue;
        }
        const f64 _d = fzmr_match(&_c, &fzmr_templates[_i]);
        if(_d < *_distance) {
            *_distance = _d;
            _best = (i32)_i;
        }
    }
    return _best;
}

// See the header.
u32 fude_zoom_mathread_symbol(const fude_zoom_v2* _points, const u32* _ends, u32 _strokes, f64* _distance) {
    f64 _d = 0.0;
    const i32 _k = fzmr_classify(_points, _ends, _strokes, &_d);
    if(_distance != NULL) {
        *_distance = _d;
    }
    return _k >= 0 ? FZMR_SYMS[_k].cp : 0u;
}

// --- symbols out of strokes -------------------------------------------------------------------------

// A box, y up: (x0, y0) its lower left corner, (x1, y1) its upper right.
typedef struct {
    f64 x0, y0, x1, y1;
} fzmr_box;

// Returns the box round points _from to _to (one past the last).
RDE_INTERNAL fzmr_box fzmr_box_of(const fude_zoom_v2* _p, u32 _from, u32 _to) {
    fzmr_box _b = { 1e300, 1e300, -1e300, -1e300 };
    for(u32 _i = _from; _i < _to; _i++) {
        _b.x0 = rde_math_min_f64(_b.x0, _p[_i].x); _b.x1 = rde_math_max_f64(_b.x1, _p[_i].x);
        _b.y0 = rde_math_min_f64(_b.y0, _p[_i].y); _b.y1 = rde_math_max_f64(_b.y1, _p[_i].y);
    }
    return _b;
}

// Returns the box round both.
RDE_INTERNAL fzmr_box fzmr_union(fzmr_box _a, fzmr_box _b) {
    return (fzmr_box){ rde_math_min_f64(_a.x0, _b.x0), rde_math_min_f64(_a.y0, _b.y0), rde_math_max_f64(_a.x1, _b.x1), rde_math_max_f64(_a.y1, _b.y1) };
}

// Returns whether a box is flat: under a quarter as tall as it is wide (a bar's, a minus's).
RDE_INTERNAL b8 fzmr_flat(fzmr_box _a) {
    return (_a.y1 - _a.y0) < 0.25 * (_a.x1 - _a.x0);
}

// A symbol read, or a part made of them (a fraction, a root, a base with its scripts): its LaTeX, its box.
typedef struct {
    u32      cp;            // its character (0: a part)
    fzmr_box box;           // round its strokes, or round all a part holds
    c8       tex[FZMR_TEX];  // a part's LaTeX (a symbol's is fzmr_tex_of's)
    b8       used;          // taken into a part, or written: left out from then on
} fzmr_node;

// rde_sort's order for f64s, smallest first: for the strokes' heights' median.
RDE_INTERNAL int fzmr_by_median(const any _a, const any _b) {
    const f64 _x = *(const f64*)_a, _y = *(const f64*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// Returns a character's LaTeX: FZMR_SYMS's, the characters read by size and place, or any other ASCII letter or digit
// as itself (from a static buffer); "" for anything else.
RDE_INTERNAL const c8* fzmr_tex_of(u32 _cp) {
    for(u32 _i = 0; _i < FZMR_SYMS_COUNT; _i++) {
        if(FZMR_SYMS[_i].cp == _cp) {
            return FZMR_SYMS[_i].tex;
        }
    }
    switch(_cp) {
    case FZMR_DOT:   return ".";
    case FZMR_CDOT:  return "\\cdot ";
    case FZMR_BAR:   return "|";
    case FZMR_TIMES: return "\\times ";
    case FZMR_PRIME: return "'";
    case ',':        return ",";
    default:         break;
    }
    // (any other letter or digit: itself)
    static c8 _plain[128][2];
    if(_cp < 128u && ((_cp >= 'a' && _cp <= 'z') || (_cp >= 'A' && _cp <= 'Z') || (_cp >= '0' && _cp <= '9'))) {
        _plain[_cp][0] = (c8)_cp;
        _plain[_cp][1] = 0;
        return _plain[_cp];
    }
    return "";
}

// --- the layout --------------------------------------------------------------------------------------

// What the layout's steps share: the nodes, and how tall a symbol is written.
typedef struct {
    fzmr_node* n;
    f64        h;   // a symbol's height, as these are written (their median)
} fzmr_layout;

// Appends _s to the string in _out (_size bytes), cut short when it does not fit.
RDE_INTERNAL void fzmr_cat(c8* _out, usize _size, const c8* _s) {
    const usize _l = rde_strlen(_out);
    rde_snprintf(_out + _l, _l < _size ? _size - _l : 0u, "%s", _s);
}

// A node's box's middle, across.
RDE_INTERNAL f64 fzmr_cx(const fzmr_node* _n) {
    return 0.5 * (_n->box.x0 + _n->box.x1);
}

// A node's box's middle, up.
RDE_INTERNAL f64 fzmr_cy(const fzmr_node* _n) {
    return 0.5 * (_n->box.y0 + _n->box.y1);
}

// The LaTeX of the nodes _idx (_n of them), laid out: fraction bars and roots and big operators' limits first (each
// what it holds read as its own), then the line left to right with its scripts.
RDE_INTERNAL void fzmr_region(fzmr_layout* _l, u32* _idx, u32 _n, c8* _out, usize _size, u32 _depth);

// Reads nodes _take (_k of them) into _out as a region one level deeper; "" when there are none.
RDE_INTERNAL void fzmr_sub_region(fzmr_layout* _l, u32* _take, u32 _k, c8* _out, usize _size, u32 _depth) {
    _out[0] = 0;
    if(_k > 0u) {
        fzmr_region(_l, _take, _k, _out, _size, _depth + 1u);
    }
}

// Makes the fractions among nodes _idx: a minus wider than 0.6 of a symbol's height with something over it and
// something under it, the widest first (so one over another reads right). Each side is read as a region of its own;
// the bar's node becomes the fraction (cp 0, its \frac in tex, its box round them all) and what is over and under it
// is used. A minus with nothing on one side is marked U+2213 so it is not tried again, and fzmr_region sets it back.
// Returns whether it made any.
RDE_INTERNAL b8 fzmr_fractions(fzmr_layout* _l, u32* _idx, u32 _n, u32 _depth) {
    b8 _any = false;
    for(;;) {
        i32 _bar = -1;
        f64 _wide = 0.0;
        for(u32 _i = 0; _i < _n; _i++) {
            const fzmr_node* _b = &_l->n[_idx[_i]];
            const f64 _w = _b->box.x1 - _b->box.x0;
            if(!_b->used && _b->cp == 0x2212u && _w > _wide && _w > 0.6 * _l->h) {
                _bar  = (i32)_i;
                _wide = _w;
            }
        }
        if(_bar < 0) {
            return _any;
        }
        fzmr_node* _b = &_l->n[_idx[_bar]];
        u32 _above[FZMR_MOST], _below[FZMR_MOST], _na = 0, _nb = 0;
        const f64 _y = fzmr_cy(_b);
        for(u32 _i = 0; _i < _n; _i++) {
            fzmr_node* _o = &_l->n[_idx[_i]];
            if((i32)_i == _bar || _o->used) {
                continue;
            }
            const f64 _cx = fzmr_cx(_o);
            if(_cx < _b->box.x0 - 0.1 * _l->h || _cx > _b->box.x1 + 0.1 * _l->h) {
                continue;
            }
            if(_o->box.y0 >= _y - 0.15 * _l->h && _o->box.y0 - _y < 2.5 * _l->h) {
                _above[_na++] = _idx[_i];
            } else if(_o->box.y1 <= _y + 0.15 * _l->h && _y - _o->box.y1 < 2.5 * _l->h) {
                _below[_nb++] = _idx[_i];
            }
        }
        if(_na == 0u || _nb == 0u) {
            _b->cp = 0x2213u;   // (a minus, not a bar: not looked at again as one — set back below)
            continue;
        }
        c8 _num[FZMR_TEX], _den[FZMR_TEX];
        fzmr_sub_region(_l, _above, _na, _num, sizeof(_num), _depth);   // (each read as its own region)
        fzmr_sub_region(_l, _below, _nb, _den, sizeof(_den), _depth);
        fzmr_box _box = _b->box;
        for(u32 _i = 0; _i < _na; _i++) { _box = fzmr_union(_box, _l->n[_above[_i]].box); _l->n[_above[_i]].used = true; }
        for(u32 _i = 0; _i < _nb; _i++) { _box = fzmr_union(_box, _l->n[_below[_i]].box); _l->n[_below[_i]].used = true; }
        rde_snprintf(_b->tex, sizeof(_b->tex), "\\frac{%s}{%s}", _num, _den);
        _b->cp  = 0u;
        _b->box = (fzmr_box){ _b->box.x0, _box.y0, _b->box.x1, _box.y1 };
        _any = true;
    }
}

// Makes each root sign among nodes _idx a \sqrt of what is under it: the nodes whose middle is in its box, right of
// its first quarter.
RDE_INTERNAL void fzmr_roots(fzmr_layout* _l, u32* _idx, u32 _n, u32 _depth) {
    for(u32 _r = 0; _r < _n; _r++) {
        fzmr_node* _s = &_l->n[_idx[_r]];
        if(_s->used || _s->cp != 0x221Au) {
            continue;
        }
        const f64 _w = _s->box.x1 - _s->box.x0;
        u32 _in[FZMR_MOST], _k = 0;
        for(u32 _i = 0; _i < _n; _i++) {
            fzmr_node* _o = &_l->n[_idx[_i]];
            if(_i == _r || _o->used) {
                continue;
            }
            const f64 _cx = fzmr_cx(_o), _cy = fzmr_cy(_o);
            if(_cx > _s->box.x0 + 0.25 * _w && _cx < _s->box.x1 + 0.1 * _l->h && _cy > _s->box.y0 && _cy < _s->box.y1) {
                _in[_k++] = _idx[_i];
            }
        }
        c8 _body[FZMR_TEX];
        fzmr_sub_region(_l, _in, _k, _body, sizeof(_body), _depth);
        for(u32 _i = 0; _i < _k; _i++) _l->n[_in[_i]].used = true;
        rde_snprintf(_s->tex, sizeof(_s->tex), "\\sqrt{%s}", _body);
        _s->cp = 0u;
    }
}

// Gives each sum and integral among nodes _idx its limits: what is right over and right under a sum, or at an
// integral's top and foot, a little to its right, smaller than three quarters of it. Only ∑ and ∫ take them (the
// reader knows no ∏).
RDE_INTERNAL void fzmr_limits(fzmr_layout* _l, u32* _idx, u32 _n, u32 _depth) {
    for(u32 _r = 0; _r < _n; _r++) {
        fzmr_node* _s = &_l->n[_idx[_r]];
        if(_s->used || (_s->cp != 0x2211u && _s->cp != 0x222Bu)) {
            continue;
        }
        const b8 _int = _s->cp == 0x222Bu;
        u32 _up[FZMR_MOST], _down[FZMR_MOST], _nu = 0, _nd = 0;
        const f64 _h = _s->box.y1 - _s->box.y0;
        for(u32 _i = 0; _i < _n; _i++) {
            fzmr_node* _o = &_l->n[_idx[_i]];
            if(_i == _r || _o->used) {
                continue;
            }
            const f64 _cx = fzmr_cx(_o);
            // (a sum's over and under it; an integral's at its top and foot, a little right)
            const b8 _near = _int ? (_o->box.x0 > _s->box.x0 && _o->box.x0 < _s->box.x1 + 0.5 * _l->h)
                                  : (_cx > _s->box.x0 - 0.2 * _l->h && _cx < _s->box.x1 + 0.2 * _l->h);
            if(!_near || (_o->box.y1 - _o->box.y0) > 0.75 * _h) {
                continue;
            }
            if(_int ? fzmr_cy(_o) > _s->box.y1 - 0.3 * _h : _o->box.y0 > _s->box.y1 - 0.1 * _h) {
                _up[_nu++] = _idx[_i];
            } else if(_int ? fzmr_cy(_o) < _s->box.y0 + 0.3 * _h : _o->box.y1 < _s->box.y0 + 0.1 * _h) {
                _down[_nd++] = _idx[_i];
            }
        }
        if(_nu == 0u && _nd == 0u) {
            continue;
        }
        c8 _a[FZMR_TEX], _b[FZMR_TEX];
        fzmr_sub_region(_l, _down, _nd, _a, sizeof(_a), _depth);
        fzmr_sub_region(_l, _up, _nu, _b, sizeof(_b), _depth);
        for(u32 _i = 0; _i < _nu; _i++) _l->n[_up[_i]].used = true;
        for(u32 _i = 0; _i < _nd; _i++) _l->n[_down[_i]].used = true;
        rde_snprintf(_s->tex, sizeof(_s->tex), "%s", _int ? "\\int" : "\\sum");
        if(_nd > 0u) {
            fzmr_cat(_s->tex, sizeof(_s->tex), "_{");
            fzmr_cat(_s->tex, sizeof(_s->tex), _a);
            fzmr_cat(_s->tex, sizeof(_s->tex), "}");
        }
        if(_nu > 0u) {
            fzmr_cat(_s->tex, sizeof(_s->tex), "^{");
            fzmr_cat(_s->tex, sizeof(_s->tex), _b);
            fzmr_cat(_s->tex, sizeof(_s->tex), "}");
        }
        fzmr_cat(_s->tex, sizeof(_s->tex), " ");
        _s->cp = 0u;
    }
}

// The layout whose node indices fzmr_by_x_plain sorts: rde_sort's compare is given no pointer of its own.
RDE_INTERNAL fzmr_layout* fzmr_sorting;

// rde_sort's order for node indices: by their boxes' left edges.
RDE_INTERNAL int fzmr_by_x_plain(const any _a, const any _b) {
    const f64 _x = fzmr_sorting->n[*(const u32*)_a].box.x0, _y = fzmr_sorting->n[*(const u32*)_b].box.x0;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// A node's own LaTeX: a symbol's, or a part's made.
RDE_INTERNAL const c8* fzmr_own(const fzmr_node* _o) {
    return _o->cp != 0u ? fzmr_tex_of(_o->cp) : _o->tex;
}

// See its declaration above. Fractions are made first, then roots, then limits; the nodes left are one line, each
// base followed by its subscripts and its superscripts. Past 12 levels deep it reads nothing.
RDE_INTERNAL void fzmr_region(fzmr_layout* _l, u32* _idx, u32 _n, c8* _out, usize _size, u32 _depth) {
    _out[0] = 0;
    if(_n == 0u || _depth > 12u) {
        return;
    }
    fzmr_fractions(_l, _idx, _n, _depth);
    fzmr_roots(_l, _idx, _n, _depth);
    fzmr_limits(_l, _idx, _n, _depth);
    for(u32 _i = 0; _i < _n; _i++) {
        if(_l->n[_idx[_i]].cp == 0x2213u) {
            _l->n[_idx[_i]].cp = 0x2212u;   // (the minuses no fraction's bars)
        }
    }
    // What is left: a line, left to right; each thing after a base smaller and higher than its middle a superscript,
    // lower a subscript.
    u32 _line[FZMR_MOST], _k = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        if(!_l->n[_idx[_i]].used) {
            _line[_k++] = _idx[_i];
        }
    }
    fzmr_sorting = _l;
    rde_sort(_line, _k, sizeof(u32), fzmr_by_x_plain);
    for(u32 _i = 0; _i < _k;) {
        fzmr_node* _base = &_l->n[_line[_i]];
        _base->used = true;
        const f64 _bh = rde_math_max_f64(_base->box.y1 - _base->box.y0, 0.45 * _l->h), _mid = fzmr_cy(_base);
        u32 _sup[FZMR_MOST], _sub[FZMR_MOST], _ns = 0, _nb = 0;
        u32 _j = _i + 1u;
        const b8 _scriptable = _base->cp != '=' && _base->cp != '+' && _base->cp != 0x2212u && _base->cp != '<' && _base->cp != '>' &&
                               _base->cp != 0x2264u && _base->cp != 0x2265u && _base->cp != 0x2260u && _base->cp != '(' && _base->cp != FZMR_DOT;
        while(_scriptable && _j < _k) {
            const fzmr_node* _o = &_l->n[_line[_j]];
            const f64 _oh = _o->box.y1 - _o->box.y0;
            const b8 _small = _oh < 0.8 * _bh || _oh < 0.75 * _l->h;
            // (an operation or a relation does not start a script — the = after b is b's, not under it)
            const b8 _sign = _o->cp == '=' || _o->cp == '+' || _o->cp == 0x2212u || _o->cp == '<' || _o->cp == '>' || _o->cp == 0x2264u ||
                             _o->cp == 0x2265u || _o->cp == 0x2260u || _o->cp == 0x00B1u || _o->cp == FZMR_CDOT || _o->cp == FZMR_TIMES;
            if(_sign && _ns == 0u && _nb == 0u) {
                break;
            }
            // (nor ends one: a sign goes on a script only with more of it after — e^{x+1}, not y^2 = 1)
            const b8 _low = _o->box.y1 < _base->box.y1 - 0.4 * _bh && fzmr_cy(_o) < _mid;   // (which script it would go on: its own place)
            if(_sign && (_j + 1u >= _k || (_low ? _l->n[_line[_j + 1u]].box.y1 > _base->box.y1 - 0.4 * _bh
                                                : _l->n[_line[_j + 1u]].box.y0 < _base->box.y0 + 0.4 * _bh))) {
                break;
            }
            // (raised: its foot well over the base's — as small as a script or not, a 2 after an x is as tall as
            // the x; lowered: its top well under the base's)
            const b8 _fits = _oh < 1.2 * _bh || _small;
            // (raised by a small letter's measure, not a tall one's: a root's superscripts sit lower — cramped)
            const f64 _lift = 0.4 * rde_math_min_f64(_bh, 1.05 * _l->h);
            if(_fits && _o->box.y0 > _base->box.y0 + _lift && fzmr_cy(_o) > _mid && _o->cp != FZMR_DOT && _o->cp != ',') {
                _sup[_ns++] = _line[_j++];
            } else if(_fits && _o->box.y1 < _base->box.y1 - 0.4 * _bh && fzmr_cy(_o) < _mid && _o->cp != FZMR_DOT && _o->cp != ',' &&
                      _o->cp != 0x2212u) {
                _sub[_nb++] = _line[_j++];
            } else {
                break;
            }
        }
        fzmr_cat(_out, _size, fzmr_own(_base));
        if(_nb > 0u) {
            c8 _s[FZMR_TEX];
            fzmr_sub_region(_l, _sub, _nb, _s, sizeof(_s), _depth);
            fzmr_cat(_out, _size, "_{");
            fzmr_cat(_out, _size, _s);
            fzmr_cat(_out, _size, "}");
        }
        if(_ns > 0u) {
            c8 _s[FZMR_TEX];
            fzmr_sub_region(_l, _sup, _ns, _s, sizeof(_s), _depth);
            fzmr_cat(_out, _size, "^{");
            fzmr_cat(_out, _size, _s);
            fzmr_cat(_out, _size, "}");
        }
        _i = _j;
    }
}

// Turns sin, cos, log and the rest, written as letters, into their commands (\sin), in place. A name is taken only
// where no letter or backslash comes before it, and longer names are tried first (arcsin and sinh before sin). The
// text is cut short when it grows past FZMR_TEX * 2.
RDE_INTERNAL void fzmr_functions(c8* _s, usize _size) {
    static const c8* const _names[] = { "arcsin", "arccos", "arctan", "sinh", "cosh", "tanh", "sin", "cos", "tan", "cot", "sec", "csc", "log",
                                        "ln", "exp", "lim", "max", "min", "det" };
    c8 _out[FZMR_TEX * 2];
    usize _o = 0;
    for(usize _i = 0; _s[_i] != 0 && _o + 12u < sizeof(_out);) {
        b8 _hit = false;
        if(_i == 0 || (_s[_i - 1u] != '\\' && !(_s[_i - 1u] >= 'a' && _s[_i - 1u] <= 'z') && !(_s[_i - 1u] >= 'A' && _s[_i - 1u] <= 'Z'))) {
            for(u32 _k = 0; _k < sizeof(_names) / sizeof(_names[0]) && !_hit; _k++) {
                const usize _n = rde_strlen(_names[_k]);
                if(rde_strncmp(_s + _i, _names[_k], _n) == 0) {
                    _o += (usize)rde_snprintf(_out + _o, sizeof(_out) - _o, "\\%s ", _names[_k]);
                    _i += _n;
                    _hit = true;
                }
            }
        }
        if(!_hit) {
            _out[_o++] = _s[_i++];
        }
    }
    _out[_o] = 0;
    rde_snprintf(_s, _size, "%s", _out);
}

// --- read -------------------------------------------------------------------------------------------

// Do strokes _a and _b (of _p, _ends) come within _by of each other — cross, or touch (a +'s, an x's, a d's bowl and
// stem; not two letters written side by side)?
RDE_INTERNAL b8 fzmr_meet(const fude_zoom_v2* _p, const u32* _ends, u32 _a, u32 _b, f64 _by) {
    const u32 _a0 = _a == 0u ? 0u : _ends[_a - 1u], _a1 = _ends[_a], _b0 = _b == 0u ? 0u : _ends[_b - 1u], _b1 = _ends[_b];
    const f64 _by2 = _by * _by;
    for(u32 _i = _a0; _i < _a1; _i++) {
        for(u32 _j = _b0; _j < _b1; _j++) {
            const f64 _dx = _p[_i].x - _p[_j].x, _dy = _p[_i].y - _p[_j].y;
            if(_dx * _dx + _dy * _dy <= _by2) {
                return true;
            }
            // (and between its points: a stroke crossing another between two of them)
            if(_j + 1u < _b1 && _i + 1u < _a1) {
                const fude_zoom_v2 _p1 = _p[_i], _p2 = _p[_i + 1u], _q1 = _p[_j], _q2 = _p[_j + 1u];
                const f64 _d1 = (_q2.x - _q1.x) * (_p1.y - _q1.y) - (_q2.y - _q1.y) * (_p1.x - _q1.x);
                const f64 _d2 = (_q2.x - _q1.x) * (_p2.y - _q1.y) - (_q2.y - _q1.y) * (_p2.x - _q1.x);
                const f64 _d3 = (_p2.x - _p1.x) * (_q1.y - _p1.y) - (_p2.y - _p1.y) * (_q1.x - _p1.x);
                const f64 _d4 = (_p2.x - _p1.x) * (_q2.y - _p1.y) - (_p2.y - _p1.y) * (_q2.x - _p1.x);
                if(((_d1 > 0.0) != (_d2 > 0.0)) && ((_d3 > 0.0) != (_d4 > 0.0))) {
                    return true;
                }
            }
        }
    }
    return false;
}

// The cut of the first _n strokes into symbols, for fude_zoom_mathread (_points and _ends the strokes, _sb their boxes,
// _h how tall a symbol is written): for each i, the best reading of the first i strokes, its last symbol one to three
// strokes (_take[i]) and what it is (_what[i]: its FZMR_SYMS index, -2 a dot), each symbol's cost its match's, a cut
// between strokes that meet or lie over each other costing more. _heap: for its scratch.
RDE_INTERNAL void fzmr_cut(const fude_zoom_v2* _points, const u32* _ends, u32 _n, const fzmr_box* _sb, f64 _h, rde_memory_allocator* _heap, u8* _take,
                           i32* _what) {
    f64 _best[FZMR_MOST + 1u];
    _best[0] = 0.0;
    for(u32 _i = 1; _i <= _n; _i++) {
        _best[_i] = 1e300;
        _take[_i] = 1u;
        _what[_i] = -1;
        for(u32 _k = 1; _k <= FZMR_GROUP && _k <= _i; _k++) {
            const u32 _first = _i - _k;
            fzmr_box _u = _sb[_first];
            b8 _close = true;
            f64 _apart = 0.0;   // (a symbol's strokes are over each other: two side by side are two symbols, more likely)
            for(u32 _s = _first + 1u; _s < _i; _s++) {
                // (each next stroke touching what is before it in the symbol, or nearly)
                const f64 _gx = rde_math_max_f64(_sb[_s].x0 - _u.x1, _u.x0 - _sb[_s].x1), _gy = rde_math_max_f64(_sb[_s].y0 - _u.y1, _u.y0 - _sb[_s].y1);
                _close = _close && _gx < 0.4 * _h && _gy < 0.75 * _h;
                const f64 _over = rde_math_min_f64(_sb[_s].x1, _u.x1) - rde_math_max_f64(_sb[_s].x0, _u.x0);
                const f64 _narrow = rde_math_max_f64(rde_math_min_f64(_sb[_s].x1 - _sb[_s].x0, _u.x1 - _u.x0), 0.05 * _h);
                b8 _meets = false;
                for(u32 _t = _first; _t < _s && !_meets; _t++) {
                    _meets = fzmr_meet(_points, _ends, _s, _t, 0.08 * _h);
                }
                // (nor a flat line right under or over the rest — a ≤'s, a ±'s, an ='s other line)
                const b8 _lined = fzmr_flat(_sb[_s]) && _over / _narrow > 0.6 && _gy < 0.4 * _h;   // (_gy: its gap up or down, above)
                _apart += _meets || _lined ? 0.0 : 1.4 + (_over / _narrow < 0.3 ? 0.6 : 0.0);
                _u = fzmr_union(_u, _sb[_s]);
            }
            if(!_close || (_k > 1u && (_u.x1 - _u.x0 > 2.2 * _h || _u.y1 - _u.y0 > 3.0 * _h))) {
                continue;
            }
            // (a dot is a symbol of its own here — an i's, a j's, a !'s found it after, by where it is)
            b8 _dotted = false;
            f64 _largest = 0.0;
            for(u32 _s = _first; _s < _i; _s++) {
                _largest = rde_math_max_f64(_largest, rde_math_max_f64(_sb[_s].x1 - _sb[_s].x0, _sb[_s].y1 - _sb[_s].y0));
            }
            for(u32 _s = _first; _s < _i && _k > 1u; _s++) {
                const f64 _big = rde_math_max_f64(_sb[_s].x1 - _sb[_s].x0, _sb[_s].y1 - _sb[_s].y0);
                _dotted = _dotted || _big < 0.18 * _h || (_big < 0.3 * _largest && _big < 0.35 * _h);   // (a small script's dot too)
            }
            if(_dotted) {
                continue;
            }
            f64 _cost = 0.0;
            i32 _sym = -1;
            const f64 _big = rde_math_max_f64(_u.x1 - _u.x0, _u.y1 - _u.y0);
            // (a dot: tiny, or small beside a stroke before or after it much taller — a small script's i's)
            const f64 _before = _first > 0u ? _sb[_first - 1u].y1 - _sb[_first - 1u].y0 : 0.0, _after = _i < _n ? _sb[_i].y1 - _sb[_i].y0 : 0.0;
            if(_k == 1u && (_big < 0.18 * _h || (_big < 0.35 * _h && _big < 0.3 * rde_math_max_f64(_before, _after)))) {
                _cost = 0.2;   // (a dot: its place says which, below)
                _sym  = -2;
            } else {
                const u32 _from = _first == 0u ? 0u : _ends[_first - 1u];
                rde_arr _e = rde_arr_new(sizeof(u32), _heap);
                for(u32 _s = _first; _s < _i; _s++) {
                    const u32 _end = _ends[_s] - _from;
                    rde_arr_add(&_e, (any)&_end);
                }
                _sym = fzmr_classify(_points + _from, (const u32*)_e.memory, _k, &_cost);
                rde_arr_free(&_e);
                if(_sym < 0) {
                    continue;
                }
            }
            // (cutting between two strokes much over each other, as wide as each other — an ='s, an x's, a +'s — costs)
            // (touching strokes are one symbol's, mostly; two flat ones over each other an ='s)
            f64 _cut = 0.0;
            if(_first > 0u) {
                const fzmr_box _p0 = _sb[_first - 1u], _p1 = _sb[_first];
                const f64 _over = rde_math_min_f64(_p0.x1, _p1.x1) - rde_math_max_f64(_p0.x0, _p1.x0);
                const f64 _wide = rde_math_max_f64(rde_math_max_f64(_p0.x1 - _p0.x0, _p1.x1 - _p1.x0), 0.05 * _h);
                const f64 _gy = rde_math_max_f64(_p1.y0 - _p0.y1, _p0.y0 - _p1.y1);
                if(fzmr_meet(_points, _ends, _first - 1u, _first, 0.08 * _h)) {
                    _cut = 1.2 + 1.0 * rde_math_min_f64(rde_math_max_f64(_over, 0.0) / _wide, 1.0);
                } else if(fzmr_flat(_p0) && fzmr_flat(_p1) && _over > 0.5 * _wide && _gy < 0.6 * _h) {
                    _cut = 2.5;
                }
            }
            const f64 _total = _best[_first] + _cost + FZMR_SYMBOL + _cut + _apart;
#ifdef FZMR_DEBUG
            rde_log_level(RDE_LOG_LEVEL_INFO, "    dp %u..%u sym %d cost %.2f cut %.2f total %.2f", _first, _i - 1u, _sym >= 0 ? (int)FZMR_SYMS[_sym].cp : _sym, _cost, _cut, _total);
#endif
            if(_total < _best[_i]) {
                _best[_i] = _total;
                _take[_i] = (u8)_k;
                _what[_i] = _sym;
            }
        }
    }
}

// Reads the _count nodes by their size and place, for fude_zoom_mathread (_h how tall a symbol is written), as said
// below: a dot over a stroke or alone, a capital, ×, a prime, an l, a root's bar written apart.
RDE_INTERNAL void fzmr_by_place(fzmr_node* _nodes, u32 _count, f64 _h) {
    // Read by size and place: a dot over a 1 or an l an i (over a j-like stroke a j), a dot at the middle a ⋅; a short
    // high tick a prime; a tall bar |; a capital by its height; ×, not x, between two numbers; an l a 1.
    for(u32 _i = 0; _i < _count; _i++) {
        fzmr_node* _o = &_nodes[_i];
        const f64 _w = _o->box.x1 - _o->box.x0, _hh = _o->box.y1 - _o->box.y0;
        if(_o->cp == 'l' || _o->cp == '1') {
            _o->cp = _hh > 1.7 * _h ? FZMR_BAR : '1';   // (a stroke up: a 1, a | when tall — an l before letters: below)
        }
        if(_o->cp == 'o' && _hh > 0.85 * _h) {
            _o->cp = '0';
        }
        static const c8 _same[] = "cosuvwxzkp";
        if(_o->cp < 128u && rde_strchr(_same, (c8)_o->cp) != NULL && _o->cp != 0u && _hh > 1.3 * _h && _w > 0.3 * _h) {
            _o->cp = (u32)(_o->cp - 'a' + 'A');
        }
    }
    for(u32 _i = 0; _i < _count; _i++) {
        fzmr_node* _o = &_nodes[_i];
        if(_o->cp != FZMR_DOT || _o->used) {
            continue;
        }
        // (a dot over the stroke before or after it: that one's dot)
        b8 _taken = false;
        for(u32 _j = 0; _j < _count && !_taken; _j++) {
            fzmr_node* _s = &_nodes[_j];
            if(_j == _i || _s->used || (_s->cp != '1' && _s->cp != 'l' && _s->cp != FZMR_BAR && _s->cp != 'j' && _s->cp != ')' && _s->cp != '!' &&
                                        _s->cp != 0x2032u)) {
                continue;
            }
            const f64 _cx = 0.5 * (_o->box.x0 + _o->box.x1);
            if(_cx > _s->box.x0 - 0.25 * _h && _cx < _s->box.x1 + 0.25 * _h && _o->box.y0 > _s->box.y1 - 0.1 * _h && _o->box.y0 - _s->box.y1 < 0.6 * _h) {
                _s->cp = _s->cp == 'j' || _s->cp == ')' ? 'j' : 'i';
                _s->box = fzmr_union(_s->box, _o->box);
                _o->used = true;
                _taken = true;
            } else if(_cx > _s->box.x0 - 0.25 * _h && _cx < _s->box.x1 + 0.25 * _h && _o->box.y1 < _s->box.y0 + 0.1 * _h && _s->box.y0 - _o->box.y1 < 0.6 * _h) {
                _s->cp = '!';
                _s->box = fzmr_union(_s->box, _o->box);
                _o->used = true;
                _taken = true;
            }
        }
        if(!_taken) {
            // (a dot alone: low a full stop, at the middle of its neighbours a ⋅)
            f64 _y0 = 1e300, _y1 = -1e300;
            for(u32 _j = 0; _j < _count; _j++) {
                if(_j != _i && !_nodes[_j].used && rde_math_abs_f64(0.5 * (_nodes[_j].box.x0 + _nodes[_j].box.x1) - 0.5 * (_o->box.x0 + _o->box.x1)) < 2.0 * _h) {
                    _y0 = rde_math_min_f64(_y0, _nodes[_j].box.y0);
                    _y1 = rde_math_max_f64(_y1, _nodes[_j].box.y1);
                }
            }
            if(_y1 > _y0 && 0.5 * (_o->box.y0 + _o->box.y1) > _y0 + 0.3 * (_y1 - _y0)) {
                _o->cp = FZMR_CDOT;
            }
        }
    }
    // (× between numbers; a short stroke high by what it follows a prime; a 1 right before a letter on its line an l —
    // lim's, ln's, log's)
    for(u32 _i = 0; _i < _count; _i++) {
        fzmr_node* _o = &_nodes[_i];
        if(_o->used) {
            continue;
        }
        if(_o->cp == '1') {
            for(u32 _j = 0; _j < _count; _j++) {
                const fzmr_node* _q = &_nodes[_j];
                if(_j != _i && !_q->used && _q->cp >= 'a' && _q->cp <= 'z' && _q->box.x0 > _o->box.x0 && _q->box.x0 - _o->box.x1 < 0.6 * _h &&
                   rde_math_abs_f64(_q->box.y0 - _o->box.y0) < 0.3 * _h) {
                    _o->cp = 'l';
                    break;
                }
            }
        }
        if(_o->cp == 'x' && _i > 0u && _i + 1u < _count && _nodes[_i - 1u].cp >= '0' && _nodes[_i - 1u].cp <= '9' && _nodes[_i + 1u].cp >= '0' &&
           _nodes[_i + 1u].cp <= '9' && rde_math_abs_f64(_nodes[_i + 1u].box.y0 - _nodes[_i - 1u].box.y0) < 0.3 * _h) {
            _o->cp = FZMR_TIMES;
        }
        if((_o->cp == '/' || _o->cp == '1' || _o->cp == ',') && (_o->box.y1 - _o->box.y0) < 0.5 * _h && _i > 0u &&
           _o->box.y0 > _nodes[_i - 1u].box.y0 + 0.5 * (_nodes[_i - 1u].box.y1 - _nodes[_i - 1u].box.y0)) {
            _o->cp = FZMR_PRIME;
        }
    }
    // (a root's bar written apart from its sign: one root, over what the bar is over)
    for(u32 _i = 0; _i < _count; _i++) {
        fzmr_node* _r = &_nodes[_i];
        if(_r->used || _r->cp != 0x221Au) {
            continue;
        }
        for(u32 _j = 0; _j < _count; _j++) {
            fzmr_node* _b = &_nodes[_j];
            if(_j == _i || _b->used || _b->cp != 0x2212u) {
                continue;
            }
            if(_b->box.x0 > _r->box.x1 - 0.5 * _h && _b->box.x0 < _r->box.x1 + 0.4 * _h && rde_math_abs_f64(_b->box.y1 - _r->box.y1) < 0.4 * _h) {
                _r->box = fzmr_union(_r->box, _b->box);
                _b->used = true;
                break;
            }
        }
    }
}

// See the header. Five steps: how tall a symbol is written (_h, the median of the strokes' heights, the flat ones left
// out); the cut into symbols; the nodes, in writing order; what size and place say (dots, capitals, ×, primes, a
// root's bar); then the layout, and function names made commands. Every distance is measured against _h.
b8 fude_zoom_mathread(const fude_zoom_v2* _points, const u32* _ends, u32 _strokes, c8* _out, usize _size, u32* _symbols) {
    if(_symbols != NULL) {
        *_symbols = 0u;
    }
    if(_size == 0 || _strokes == 0u || _points == NULL || _ends == NULL) {
        return false;
    }
    _out[0] = 0;
    const u32 _n = _strokes < FZMR_MOST ? _strokes : FZMR_MOST;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    // Each stroke's box; how tall a symbol is written (the strokes' heights' median, the flat ones — bars, minuses —
    // not counted).
    fzmr_box _sb[FZMR_MOST];
    f64 _hs[FZMR_MOST];
    u32 _nh = 0;
    for(u32 _s = 0; _s < _n; _s++) {
        _sb[_s] = fzmr_box_of(_points, _s == 0u ? 0u : _ends[_s - 1u], _ends[_s]);
        const f64 _w = _sb[_s].x1 - _sb[_s].x0, _hh = _sb[_s].y1 - _sb[_s].y0;
        if(_hh > 0.3 * _w) {
            _hs[_nh++] = _hh;
        }
    }
    f64 _h = 1.0;
    if(_nh > 0u) {
        rde_sort(_hs, _nh, sizeof(f64), fzmr_by_median);
        _h = rde_math_max_f64(_hs[_nh / 2u], 1e-9);
    }
    // The cut into symbols: best[i] the best reading of the first i strokes; each symbol one to three strokes written
    // one after another and close together.
    u8  _take[FZMR_MOST + 1u];
    i32 _what[FZMR_MOST + 1u];
    fzmr_cut(_points, _ends, _n, _sb, _h, _heap, _take, _what);
    // The symbols, back from the end.
    rde_arr TYPE(fzmr_node) _node_list = rde_arr_new_with_capacity(sizeof(fzmr_node), _n > 0u ? _n : 1u, rde_memory_allocator_get_default_std());
    rde_arr_resize(&_node_list, _n);   // (zeroed)
    fzmr_node* _nodes = (fzmr_node*)_node_list.memory;
    u32 _count = 0;
    for(u32 _i = _n; _i > 0u;) {
        const u32 _k = _take[_i];
        const u32 _first = _i - _k;
        fzmr_box _u = _sb[_first];
        for(u32 _s = _first + 1u; _s < _i; _s++) {
            _u = fzmr_union(_u, _sb[_s]);
        }
        fzmr_node* _o = &_nodes[_count++];
        _o->box = _u;
        _o->cp  = _what[_i] >= 0 ? FZMR_SYMS[_what[_i]].cp : (_what[_i] == -2 ? FZMR_DOT : 0u);
        _o->cp  = _o->cp == 0xE000u ? 0x222Bu : _o->cp == 0xE001u ? 0x221Au : _o->cp;   // (a second shape's: its symbol)
        _i = _first;
    }
    // (written order: they came out last first; none to turn round when there are none, so _b never wraps)
    for(u32 _a = 0, _b = _count > 0u ? _count - 1u : 0u; _a < _b; _a++, _b--) {
        const fzmr_node _t = _nodes[_a];
        _nodes[_a] = _nodes[_b];
        _nodes[_b] = _t;
    }
    fzmr_by_place(_nodes, _count, _h);
#ifdef FZMR_DEBUG
    for(u32 _i = 0; _i < _count; _i++) {
        rde_log_level(RDE_LOG_LEVEL_INFO, "    node %u U+%04X %s box %.1f %.1f %.1f %.1f (h %.1f)", _i, _nodes[_i].cp, _nodes[_i].used ? "used" : "", _nodes[_i].box.x0, _nodes[_i].box.y0,
               _nodes[_i].box.x1, _nodes[_i].box.y1, _h);
    }
#endif
    // The layout.
    u32 _idx[FZMR_MOST], _live = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        if(!_nodes[_i].used && _nodes[_i].cp != 0u) {
            _idx[_live++] = _i;
        } else {
            _nodes[_i].used = true;
        }
    }
    fzmr_layout _l = { _nodes, _h };
    c8 _tex[FZMR_TEX * 2];
    fzmr_region(&_l, _idx, _live, _tex, sizeof(_tex), 0u);
    fzmr_functions(_tex, sizeof(_tex));
    // (the spaces a command left at the end trimmed)
    usize _len = rde_strlen(_tex);
    while(_len > 0 && _tex[_len - 1u] == ' ') {
        _tex[--_len] = 0;
    }
    rde_snprintf(_out, _size, "%s", _tex);
    if(_symbols != NULL) {
        *_symbols = _live;
    }
    rde_arr_free(&_node_list);
    return _live > 0u && _out[0] != 0;
}
