// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/hand.h"
#include "zoom/hand_glyphs.h"
#include "drawing/base/utf8.h"
#include <math.h>
#include <string.h>

// The font's units: the baseline at y 9, a capital's top at -12 (21 units
// tall), y down. Ours: y up, from the baseline.
#define FUDE_ZOOM_HAND_BASE   9.0
#define FUDE_ZOOM_HAND_CAP    21.0
#define FUDE_ZOOM_HAND_XTOP   9.0     // a small letter's top, up from the baseline
#define FUDE_ZOOM_HAND_LINE   1.9     // lines apart, in capitals
#define FUDE_ZOOM_HAND_STEPS  4u      // points smoothed in between each two of the font's
#define FUDE_ZOOM_HAND_JOIN   4.5     // a letter's last stroke ending this near the next one's first (units): one stroke

typedef enum {
    FUDE_ZOOM_HAND_MARK_NONE = 0,
    FUDE_ZOOM_HAND_ACUTE, FUDE_ZOOM_HAND_GRAVE, FUDE_ZOOM_HAND_CIRCUMFLEX, FUDE_ZOOM_HAND_TILDE,
    FUDE_ZOOM_HAND_DIAERESIS, FUDE_ZOOM_HAND_RING, FUDE_ZOOM_HAND_CEDILLA, FUDE_ZOOM_HAND_TURNED
} FUDE_ZOOM_HAND_MARK_;

typedef struct {
    u32 codepoint;
    c8  base;
    u8  mark;
} fude_zoom_hand_letter;

// The accented letters: their plain letter and the mark over (or under) it.
static const fude_zoom_hand_letter FUDE_ZOOM_HAND_LETTERS[] = {
    { 0xE1, 'a', FUDE_ZOOM_HAND_ACUTE }, { 0xE9, 'e', FUDE_ZOOM_HAND_ACUTE }, { 0xED, 'i', FUDE_ZOOM_HAND_ACUTE }, { 0xF3, 'o', FUDE_ZOOM_HAND_ACUTE },
    { 0xFA, 'u', FUDE_ZOOM_HAND_ACUTE }, { 0xFD, 'y', FUDE_ZOOM_HAND_ACUTE },
    { 0xC1, 'A', FUDE_ZOOM_HAND_ACUTE }, { 0xC9, 'E', FUDE_ZOOM_HAND_ACUTE }, { 0xCD, 'I', FUDE_ZOOM_HAND_ACUTE }, { 0xD3, 'O', FUDE_ZOOM_HAND_ACUTE },
    { 0xDA, 'U', FUDE_ZOOM_HAND_ACUTE }, { 0xDD, 'Y', FUDE_ZOOM_HAND_ACUTE },
    { 0xE0, 'a', FUDE_ZOOM_HAND_GRAVE }, { 0xE8, 'e', FUDE_ZOOM_HAND_GRAVE }, { 0xEC, 'i', FUDE_ZOOM_HAND_GRAVE }, { 0xF2, 'o', FUDE_ZOOM_HAND_GRAVE },
    { 0xF9, 'u', FUDE_ZOOM_HAND_GRAVE }, { 0xC0, 'A', FUDE_ZOOM_HAND_GRAVE }, { 0xC8, 'E', FUDE_ZOOM_HAND_GRAVE }, { 0xD9, 'U', FUDE_ZOOM_HAND_GRAVE },
    { 0xE2, 'a', FUDE_ZOOM_HAND_CIRCUMFLEX }, { 0xEA, 'e', FUDE_ZOOM_HAND_CIRCUMFLEX }, { 0xEE, 'i', FUDE_ZOOM_HAND_CIRCUMFLEX }, { 0xF4, 'o', FUDE_ZOOM_HAND_CIRCUMFLEX },
    { 0xFB, 'u', FUDE_ZOOM_HAND_CIRCUMFLEX }, { 0xC2, 'A', FUDE_ZOOM_HAND_CIRCUMFLEX }, { 0xCA, 'E', FUDE_ZOOM_HAND_CIRCUMFLEX }, { 0xD4, 'O', FUDE_ZOOM_HAND_CIRCUMFLEX },
    { 0xE3, 'a', FUDE_ZOOM_HAND_TILDE }, { 0xF5, 'o', FUDE_ZOOM_HAND_TILDE }, { 0xF1, 'n', FUDE_ZOOM_HAND_TILDE },
    { 0xC3, 'A', FUDE_ZOOM_HAND_TILDE }, { 0xD5, 'O', FUDE_ZOOM_HAND_TILDE }, { 0xD1, 'N', FUDE_ZOOM_HAND_TILDE },
    { 0xE4, 'a', FUDE_ZOOM_HAND_DIAERESIS }, { 0xEB, 'e', FUDE_ZOOM_HAND_DIAERESIS }, { 0xEF, 'i', FUDE_ZOOM_HAND_DIAERESIS }, { 0xF6, 'o', FUDE_ZOOM_HAND_DIAERESIS },
    { 0xFC, 'u', FUDE_ZOOM_HAND_DIAERESIS }, { 0xFF, 'y', FUDE_ZOOM_HAND_DIAERESIS }, { 0xC4, 'A', FUDE_ZOOM_HAND_DIAERESIS }, { 0xD6, 'O', FUDE_ZOOM_HAND_DIAERESIS },
    { 0xDC, 'U', FUDE_ZOOM_HAND_DIAERESIS },
    { 0xE5, 'a', FUDE_ZOOM_HAND_RING }, { 0xC5, 'A', FUDE_ZOOM_HAND_RING },
    { 0xE7, 'c', FUDE_ZOOM_HAND_CEDILLA }, { 0xC7, 'C', FUDE_ZOOM_HAND_CEDILLA },
    { 0xBF, '?', FUDE_ZOOM_HAND_TURNED }, { 0xA1, '!', FUDE_ZOOM_HAND_TURNED },
    { 0x2018, '\'', 0 }, { 0x2019, '\'', 0 }, { 0x201C, '"', 0 }, { 0x201D, '"', 0 }, { 0xAB, '"', 0 }, { 0xBB, '"', 0 },
    { 0x2013, '-', 0 }, { 0x2014, '-', 0 }, { 0x2026, '.', 0 }, { 0xBA, 'o', 0 }, { 0xAA, 'a', 0 }, { 0xA0, ' ', 0 },
};

// --- a seeded hand ------------------------------------------------------------------------------

typedef struct {
    u32 state;
} fude_zoom_hand_rng;

RDE_INTERNAL f64 fude_zoom_hand_rand(fude_zoom_hand_rng* _r) {
    _r->state = _r->state * 1664525u + 1013904223u;
    return (f64)(_r->state >> 8) / 16777216.0;   // [0, 1)
}

RDE_INTERNAL f64 fude_zoom_hand_spread(fude_zoom_hand_rng* _r, f64 _by) {
    return (fude_zoom_hand_rand(_r) * 2.0 - 1.0) * _by;
}

// --- a letter's strokes -----------------------------------------------------------------------

// How a letter is put down: where its origin is (units, y up), its slant and size.
typedef struct {
    f64 x, y;          // its baseline's start
    f64 slant;         // x more per unit up
    f64 scale;
    f64 wobble_phase;  // the waver along its strokes
} fude_zoom_hand_place;

// One stroke being built (units, y up, before it is placed).
typedef struct {
    fude_zoom_v2 p[160];
    u32          n;
} fude_zoom_hand_stroke;

RDE_INTERNAL fude_zoom_v2 fude_zoom_hand_at(const fude_zoom_hand_place* _pl, fude_zoom_v2 _u) {
    return (fude_zoom_v2){ _pl->x + (_u.x + _u.y * _pl->slant) * _pl->scale, _pl->y + _u.y * _pl->scale };
}

// A stroke's points smoothed (Catmull-Rom through the font's, a few between each two),
// wavering a little, placed — appended to _points; its end to _ends (an empty or a
// one-point stroke: a dot, a tiny line).
RDE_INTERNAL void fude_zoom_hand_put(const fude_zoom_hand_stroke* _st, const fude_zoom_hand_place* _pl, rde_arr* _points, rde_arr* _ends, b8 _join) {
    const u32 _n = _st->n;
    if(_n == 0) {
        return;
    }
    const fude_zoom_v2* _p = _st->p;
    u32 _made = 0;
    if(_n == 1u) {
        const fude_zoom_v2 _a = fude_zoom_hand_at(_pl, _p[0]);
        const fude_zoom_v2 _b = fude_zoom_hand_at(_pl, (fude_zoom_v2){ _p[0].x + 0.35, _p[0].y + 0.2 });
        rde_arr_add(_points, (any)&_a);
        rde_arr_add(_points, (any)&_b);
        _made = 2u;
    } else {
        for(u32 _i = 0; _i + 1u < _n; _i++) {
            const fude_zoom_v2 _p0 = _p[_i > 0 ? _i - 1u : 0u], _p1 = _p[_i], _p2 = _p[_i + 1u], _p3 = _p[_i + 2u < _n ? _i + 2u : _n - 1u];
            for(u32 _s = (_i == 0u ? 0u : 1u); _s <= FUDE_ZOOM_HAND_STEPS; _s++) {
                const f64 _t = (f64)_s / (f64)FUDE_ZOOM_HAND_STEPS, _t2 = _t * _t, _t3 = _t2 * _t;
                fude_zoom_v2 _u = {
                    0.5 * ((2.0 * _p1.x) + (-_p0.x + _p2.x) * _t + (2.0 * _p0.x - 5.0 * _p1.x + 4.0 * _p2.x - _p3.x) * _t2 + (-_p0.x + 3.0 * _p1.x - 3.0 * _p2.x + _p3.x) * _t3),
                    0.5 * ((2.0 * _p1.y) + (-_p0.y + _p2.y) * _t + (2.0 * _p0.y - 5.0 * _p1.y + 4.0 * _p2.y - _p3.y) * _t2 + (-_p0.y + 3.0 * _p1.y - 3.0 * _p2.y + _p3.y) * _t3),
                };
                // A slow waver, a fraction of a unit, as a hand's line has.
                const f64 _w = _pl->wobble_phase + ((f64)_i + _t) * 0.9;
                _u.x += sin(_w) * 0.18;
                _u.y += cos(_w * 1.3) * 0.14;
                const fude_zoom_v2 _at = fude_zoom_hand_at(_pl, _u);
                rde_arr_add(_points, (any)&_at);
                _made++;
            }
        }
    }
    const u32 _count = (u32)rde_arr_length(_points);
    if(_join && rde_arr_length(_ends) > 0) {
        ((u32*)_ends->memory)[rde_arr_length(_ends) - 1u] = _count;   // run on from the last letter's stroke
    } else {
        rde_arr_add(_ends, (any)&_count);
    }
    RDE_UNUSED(_made);
}

// A mark over (or under) a letter: its own short strokes, in units round _cx and _top (up from the baseline).
RDE_INTERNAL void fude_zoom_hand_mark(u8 _mark, f64 _cx, f64 _top, const fude_zoom_hand_place* _pl, rde_arr* _points, rde_arr* _ends) {
    fude_zoom_hand_stroke _s;
    const f64 _g = 2.0;   // the gap over the letter
    _s.n = 0;
    switch(_mark) {
        case FUDE_ZOOM_HAND_ACUTE:
            _s.p[_s.n++] = (fude_zoom_v2){ _cx - 1.0, _top + _g };
            _s.p[_s.n++] = (fude_zoom_v2){ _cx + 2.5, _top + _g + 3.5 };
            break;
        case FUDE_ZOOM_HAND_GRAVE:
            _s.p[_s.n++] = (fude_zoom_v2){ _cx - 2.5, _top + _g + 3.5 };
            _s.p[_s.n++] = (fude_zoom_v2){ _cx + 1.0, _top + _g };
            break;
        case FUDE_ZOOM_HAND_CIRCUMFLEX:
            _s.p[_s.n++] = (fude_zoom_v2){ _cx - 2.8, _top + _g };
            _s.p[_s.n++] = (fude_zoom_v2){ _cx, _top + _g + 3.0 };
            _s.p[_s.n++] = (fude_zoom_v2){ _cx + 2.8, _top + _g };
            break;
        case FUDE_ZOOM_HAND_TILDE:
            _s.p[_s.n++] = (fude_zoom_v2){ _cx - 3.5, _top + _g + 0.6 };
            _s.p[_s.n++] = (fude_zoom_v2){ _cx - 1.8, _top + _g + 2.2 };
            _s.p[_s.n++] = (fude_zoom_v2){ _cx + 0.2, _top + _g + 1.2 };
            _s.p[_s.n++] = (fude_zoom_v2){ _cx + 1.9, _top + _g + 0.4 };
            _s.p[_s.n++] = (fude_zoom_v2){ _cx + 3.6, _top + _g + 2.0 };
            break;
        case FUDE_ZOOM_HAND_DIAERESIS: {
            fude_zoom_hand_stroke _d = { .n = 1u };
            _d.p[0] = (fude_zoom_v2){ _cx - 2.0, _top + _g + 1.0 };
            fude_zoom_hand_put(&_d, _pl, _points, _ends, false);
            _s.p[_s.n++] = (fude_zoom_v2){ _cx + 2.0, _top + _g + 1.0 };
        } break;
        case FUDE_ZOOM_HAND_RING:
            for(u32 _i = 0; _i <= 8u; _i++) {
                const f64 _a = 6.2831853 * (f64)_i / 8.0;
                _s.p[_s.n++] = (fude_zoom_v2){ _cx + cos(_a) * 1.6, _top + _g + 1.6 + sin(_a) * 1.6 };
            }
            break;
        case FUDE_ZOOM_HAND_CEDILLA:
            _s.p[_s.n++] = (fude_zoom_v2){ _cx, 0.0 };
            _s.p[_s.n++] = (fude_zoom_v2){ _cx + 1.6, -1.8 };
            _s.p[_s.n++] = (fude_zoom_v2){ _cx - 1.0, -3.6 };
            break;
        default:
            return;
    }
    fude_zoom_hand_put(&_s, _pl, _points, _ends, false);
}

// What waits till a run of joined letters ends (written then, so the next letter's
// stroke runs on from its letter's, not from a mark): an accent, or an i's dot.
typedef struct {
    u8                   mark;      // FUDE_ZOOM_HAND_ (0: a dot, the stroke below)
    f64                  cx, top;
    fude_zoom_hand_place place;
    fude_zoom_hand_stroke dot;
} fude_zoom_hand_later;

#define FUDE_ZOOM_HAND_LATER 24u

RDE_INTERNAL void fude_zoom_hand_flush(rde_arr* _later, rde_arr* _points, rde_arr* _ends) {
    const fude_zoom_hand_later* _l = (const fude_zoom_hand_later*)_later->memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(_later); _i++) {
        if(_l[_i].mark != 0) {
            fude_zoom_hand_mark(_l[_i].mark, _l[_i].cx, _l[_i].top, &_l[_i].place, _points, _ends);
        } else {
            fude_zoom_hand_put(&_l[_i].dot, &_l[_i].place, _points, _ends, false);
        }
    }
    rde_arr_clear(_later);
}

u32 fude_zoom_hand_write(const c8* _text, f64 _cap, u32 _seed, rde_arr* _points, rde_arr* _ends) {
    rde_arr TYPE(fude_zoom_hand_later) _later = rde_arr_new(sizeof(fude_zoom_hand_later), rde_memory_allocator_get_default_std());
    const u32 _first_end = (u32)rde_arr_length(_ends);
    fude_zoom_hand_rng _rng = { _seed * 2654435761u + 12345u };
    const f64 _unit = _cap / FUDE_ZOOM_HAND_CAP;   // ours a font unit
    f64  _pen_x = 0.0, _line_y = 0.0;
    b8   _joinable = false;                          // the last letter's last stroke may run on
    fude_zoom_v2 _last_end = { 0.0, 0.0 };           // ...where it ended (units of ours)
    for(const c8* _c = _text; *_c != 0;) {
        u32 _cp = fude_utf8_next(&_c);
        if(_cp == 0) {
            break;
        }
        if(_cp == '\n') {
            fude_zoom_hand_flush(&_later, _points, _ends);
            _pen_x    = 0.0;
            _line_y  -= _cap * FUDE_ZOOM_HAND_LINE;
            _joinable = false;
            continue;
        }
        c8 _base = 0;
        u8 _mark = 0;
        if(_cp >= 32u && _cp < 127u) {
            _base = (c8)_cp;
        } else {
            for(u32 _i = 0; _i < sizeof(FUDE_ZOOM_HAND_LETTERS) / sizeof(FUDE_ZOOM_HAND_LETTERS[0]); _i++) {
                if(FUDE_ZOOM_HAND_LETTERS[_i].codepoint == _cp) {
                    _base = FUDE_ZOOM_HAND_LETTERS[_i].base;
                    _mark = FUDE_ZOOM_HAND_LETTERS[_i].mark;
                    break;
                }
            }
            if(_base == 0) {
                _base = ' ';   // (not one of ours: its room)
            }
        }
        const u32 _g = (u32)(_base - 32);
        const f64 _left = (f64)FUDE_ZOOM_HAND_BEARINGS[_g][0], _right = (f64)FUDE_ZOOM_HAND_BEARINGS[_g][1];
        // This letter's unevenness.
        fude_zoom_hand_place _pl;
        _pl.scale        = _unit * (1.0 + fude_zoom_hand_spread(&_rng, 0.035));
        _pl.slant        = 0.03 + fude_zoom_hand_spread(&_rng, 0.035);
        _pl.x            = _pen_x - _left * _pl.scale;
        _pl.y            = _line_y + fude_zoom_hand_spread(&_rng, 0.35) * _unit;
        _pl.wobble_phase = fude_zoom_hand_rand(&_rng) * 6.2831853;
        if(_base == ' ') {
            fude_zoom_hand_flush(&_later, _points, _ends);
            _pen_x   += (_right - _left) * _unit * (1.0 + fude_zoom_hand_spread(&_rng, 0.08));
            _joinable = false;
            continue;
        }
        // Its strokes, in its units (y up from the baseline); turned over for ¿ ¡.
        const i8* _pts  = &FUDE_ZOOM_HAND_POINTS[FUDE_ZOOM_HAND_STARTS[_g]];
        const u32 _np   = (u32)(FUDE_ZOOM_HAND_STARTS[_g + 1u] - FUDE_ZOOM_HAND_STARTS[_g]) / 2u;
        const b8  _dotless = (_base == 'i' || _base == 'j') && _mark != 0;   // í ï...: the mark in the dot's place
        fude_zoom_hand_stroke _st;
        _st.n = 0;
        b8  _first = true;
        f64 _sum_x = 0.0;
        u32 _sum_n = 0;
        for(u32 _i = 0; _i <= _np; _i++) {
            const b8 _lift = _i == _np || _pts[2u * _i] == -128;
            if(_lift) {
                // A stroke done. A dot (a short stroke over a small letter's top, after
                // its first): written when the run ends — left out where a mark takes its place.
                b8 _dot = false;
                if(!_first && _st.n > 0 && _base >= 'a' && _base <= 'z') {
                    f64 _lo = 1e9, _w = 0.0;
                    for(u32 _k = 0; _k < _st.n; _k++) {
                        _lo = fmin(_lo, _st.p[_k].y);
                        _w  = fmax(_w, fabs(_st.p[_k].x - _st.p[0].x));
                    }
                    _dot = _lo > FUDE_ZOOM_HAND_XTOP + 0.5 && _w < 3.0;
                }
                if(_dot) {
                    if(!_dotless && rde_arr_length(&_later) < FUDE_ZOOM_HAND_LATER) {
                        fude_zoom_hand_later* _l = (fude_zoom_hand_later*)rde_arr_add_n(&_later, 1u);
                        _l->mark  = 0;
                        _l->place = _pl;
                        _l->dot   = _st;
                    }
                } else if(_st.n > 0) {
                    // The first stroke runs on from the last letter's when it begins where that one ended.
                    b8 _join = false;
                    if(_first && _joinable && _mark != FUDE_ZOOM_HAND_TURNED) {
                        const fude_zoom_v2 _start = fude_zoom_hand_at(&_pl, _st.p[0]);
                        _join = hypot(_start.x - _last_end.x, _start.y - _last_end.y) <= FUDE_ZOOM_HAND_JOIN * _unit;
                    }
                    if(_first && !_join) {
                        fude_zoom_hand_flush(&_later, _points, _ends);   // the run before ended: its marks
                    }
                    fude_zoom_hand_put(&_st, &_pl, _points, _ends, _join);
                    _last_end = fude_zoom_hand_at(&_pl, _st.p[_st.n - 1u]);
                    _first    = false;
                }
                _st.n = 0;
                continue;
            }
            fude_zoom_v2 _u = { (f64)_pts[2u * _i], FUDE_ZOOM_HAND_BASE - (f64)_pts[2u * _i + 1u] };
            if(_mark == FUDE_ZOOM_HAND_TURNED) {
                // Turned over round the middle of a capital's height, dropped below the line as ¿ and ¡ are.
                _u = (fude_zoom_v2){ (_left + _right) - _u.x, FUDE_ZOOM_HAND_CAP - _u.y - 7.0 };
            }
            if(_st.n < sizeof(_st.p) / sizeof(_st.p[0])) {
                _st.p[_st.n++] = _u;
            }
            _sum_x += _u.x;
            _sum_n++;
        }
        // Its mark: over its middle, at a small letter's top or a capital's.
        if(_mark != 0 && _mark != FUDE_ZOOM_HAND_TURNED && rde_arr_length(&_later) < FUDE_ZOOM_HAND_LATER) {
            const b8  _capital = _base >= 'A' && _base <= 'Z';
            const f64 _cx      = _sum_n > 0 ? _sum_x / (f64)_sum_n : (_left + _right) * 0.5;
            fude_zoom_hand_later* _l = (fude_zoom_hand_later*)rde_arr_add_n(&_later, 1u);
            _l->mark  = _mark;
            _l->cx    = _cx + (_capital ? 1.5 : 0.8);
            _l->top   = _capital ? FUDE_ZOOM_HAND_CAP : FUDE_ZOOM_HAND_XTOP;
            _l->place = _pl;
        }
        // Small letters run on to the next; capitals, figures and marks may start a run.
        _joinable = (_base >= 'a' && _base <= 'z') || (_base >= 'A' && _base <= 'Z');
        _pen_x   += (_right - _left) * _pl.scale + fude_zoom_hand_spread(&_rng, 0.4) * _unit;
    }
    fude_zoom_hand_flush(&_later, _points, _ends);
    rde_arr_free(&_later);
    return (u32)rde_arr_length(_ends) - _first_end;
}
