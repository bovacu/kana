// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/instrument_parts.h"

// ===========================================================================
// See instrument.h. The instruments, where things are and the pile.
//
// Everything is worked out again from each instrument's few numbers (where it is, its turn, its sizes) whenever it is
// asked: its edges, keys, outline, knob, × and tab are never kept. In order: where things are (a point of its own units
// on the screen and back; its sizes, edges and keys; a compass's hinge and label; a template's ellipse, a French
// curve's and a stencil's curves; its outline, its knob, its × and its size tab; which one is under a point); then
// shown and put away (the pile: one added, raised to the top, removed).
//
// Part of the instruments: instrument_parts.h holds what the modules share and says which module holds what. Names:
// fude_zoom_instrument_ for one instrument (a slot), fude_zoom_instruments_ for what works on them all or on what the
// hands and the pen do. Main thread only.
// ===========================================================================

// A French curve's outline (its own units, round it counter-clockwise; a smooth
// curve through them): a long gentle side, a tight turn at one end, a looser
// one at the other, about 300 by 160.
static const fude_zoom_v2 FUDE_ZOOM_FRENCH_CURVE[] = {
    { -150.0, -10.0 }, { -132.0, -52.0 }, { -88.0, -76.0 }, { -26.0, -78.0 }, { 40.0, -58.0 }, { 98.0, -20.0 },
    { 140.0, 32.0 }, { 152.0, 70.0 }, { 128.0, 82.0 }, { 92.0, 56.0 }, { 46.0, 28.0 }, { -6.0, 16.0 },
    { -62.0, 22.0 }, { -110.0, 30.0 }, { -142.0, 18.0 },
};
#define FUDE_ZOOM_FRENCH_POINTS (sizeof(FUDE_ZOOM_FRENCH_CURVE) / sizeof(FUDE_ZOOM_FRENCH_CURVE[0]))   // how many

// --- where things are ------------------------------------------------------------------------

// A point of the instrument's own units (its middle the origin, turned with it) on the screen.
fude_zoom_v2 fude_zoom_instrument_place(const fude_zoom_instrument* _t, fude_zoom_v2 _local) {
    const f64 _c = rde_math_cos_f64(_t->angle), _s = rde_math_sin_f64(_t->angle);
    return (fude_zoom_v2){ _t->at.x + _local.x * _c - _local.y * _s, _t->at.y + _local.x * _s + _local.y * _c };
}

// A point of the screen in the instrument's own units: fude_zoom_instrument_place undone.
fude_zoom_v2 fude_zoom_instrument_unplace(const fude_zoom_instrument* _t, fude_zoom_v2 _screen) {
    const f64 _c = rde_math_cos_f64(_t->angle), _s = rde_math_sin_f64(_t->angle);
    const f64 _x = _screen.x - _t->at.x, _y = _screen.y - _t->at.y;
    return (fude_zoom_v2){ _x * _c + _y * _s, -_x * _s + _y * _c };
}

// A ruler's band's half (as wide as it lies on the drawing).
RDE_INTERNAL f64 fude_zoom_ruler_half(const fude_zoom_instrument* _t) {
    return FUDE_ZOOM_RULER_WIDTH * 0.5 * (_t->thick > 0.0 ? _t->thick : 1.0);
}

// A corner radius gauge's round, and its plate's half side (the round at its lower left).
f64 fude_zoom_corner_r(const fude_zoom_instrument* _t) {
    return _t->radius > 1.0 ? _t->radius : FUDE_ZOOM_COMPASS_R * 0.5;
}

// See the header.
f64 fude_zoom_instrument_band(const fude_zoom_instrument* _t) {
    if(_t->radius2 > 1.0) {
        return _t->radius2;
    }
    return _t->kind == FUDE_ZOOM_INSTRUMENT_CORNER ? FUDE_ZOOM_CORNER_BAND : FUDE_ZOOM_CIRCLE_RIM;
}

// Its knob's, ×'s and tabs' scale: as much bigger as it is on the screen (instrument.h's look), not too small to take
// nor huge.
f64 fude_zoom_instrument_icons(const fude_zoom_instrument* _t) {
    return rde_math_min_f64(rde_math_max_f64(_t->look > 0.0 ? _t->look : 1.0, 0.3), 2.0);
}

// A circle template's rim (real: its band), an ellipse template's (as much bigger as it is).
f64 fude_zoom_instrument_rim(const fude_zoom_instrument* _t) {
    if(_t->kind == FUDE_ZOOM_INSTRUMENT_CIRCLE) {
        return fude_zoom_instrument_band(_t);
    }
    return FUDE_ZOOM_CIRCLE_RIM * (_t->look > 0.0 ? _t->look : 1.0);
}

// A corner radius gauge's plate's half side: its round and the band past it, halved.
RDE_INTERNAL f64 fude_zoom_corner_h(const fude_zoom_instrument* _t) {
    return (fude_zoom_corner_r(_t) + fude_zoom_instrument_band(_t)) * 0.5;
}

// How much bigger than when it came out (a ruler: longer; a set square, a protractor: bigger); 1 when not set.
f64 fude_zoom_instrument_size(const fude_zoom_instrument* _t) {
    return _t->size > 0.05 ? _t->size : 1.0;
}

// A set square's corners (its own units, the right angle first, then along
// its long leg, then the short), round its middle.
void fude_zoom_square_corners(u32 _kind, f64 _size, fude_zoom_v2 _out[3]) {
    const f64 _a = (_kind == FUDE_ZOOM_INSTRUMENT_SQUARE_45 ? FUDE_ZOOM_SQUARE_45_LEG : FUDE_ZOOM_SQUARE_30_LEG) * _size;
    const f64 _b = (_kind == FUDE_ZOOM_INSTRUMENT_SQUARE_45 ? FUDE_ZOOM_SQUARE_45_LEG : FUDE_ZOOM_SQUARE_30_LEG / rde_math_sqrt_f64(3.0)) * _size;
    _out[0] = (fude_zoom_v2){ -_a / 3.0, -_b / 3.0 };
    _out[1] = (fude_zoom_v2){ 2.0 * _a / 3.0, -_b / 3.0 };
    _out[2] = (fude_zoom_v2){ -_a / 3.0, 2.0 * _b / 3.0 };
}

// See the header. Each kind's edges, counter-clockwise: a ruler's four sides (its long ones graduated from the same
// end), a set square's three (its legs graduated from the right angle), an ellipse template's or a French curve's one
// curve, a corner gauge's four sides and its round, a circle template's or a compass's whole circle, a stencil's lines
// (up to FUDE_ZOOM_INSTRUMENT_EDGES), and the protractor's straight edge and graduated half round.
u32 fude_zoom_instrument_edges(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_instrument_edge* _out) {
    const fude_zoom_instrument* _t    = &_ins->tools[_slot];
    const u32                   _kind = _t->kind;
    rde_memset(_out, 0, sizeof(fude_zoom_instrument_edge) * FUDE_ZOOM_INSTRUMENT_EDGES);
    if(_kind == FUDE_ZOOM_INSTRUMENT_RULER) {
        const f64 _l = FUDE_ZOOM_RULER_LENGTH * fude_zoom_instrument_size(_t) * 0.5, _w = fude_zoom_ruler_half(_t);
        const fude_zoom_v2 _c[4] = { { -_l, -_w }, { _l, -_w }, { _l, _w }, { -_l, _w } };
        for(u32 _i = 0; _i < 4u; _i++) {
            _out[_i].a    = fude_zoom_instrument_place(_t, _c[_i]);
            _out[_i].b    = fude_zoom_instrument_place(_t, _c[(_i + 1u) % 4u]);
            _out[_i].room = 2.0 * _w;
        }
        _out[0].graduated = true;                         // the bottom, from its left end
        _out[2].graduated = true; _out[2].zero_at_b = true;   // the top, from the same end
        return 4u;
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_SQUARE_45 || _kind == FUDE_ZOOM_INSTRUMENT_SQUARE_30) {
        fude_zoom_v2 _c[3];
        fude_zoom_square_corners(_kind, fude_zoom_instrument_size(_t), _c);
        for(u32 _i = 0; _i < 3u; _i++) {
            _out[_i].a = fude_zoom_instrument_place(_t, _c[_i]);
            _out[_i].b = fude_zoom_instrument_place(_t, _c[(_i + 1u) % 3u]);
        }
        _out[0].graduated = true;                         // the legs, from the right angle
        _out[2].graduated = true; _out[2].zero_at_b = true;
        return 3u;
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_ELLIPSE || _kind == FUDE_ZOOM_INSTRUMENT_CURVE) {
        // An ellipse template's hole, a French curve's edge: a closed curve (fude_zoom_instrument_curve).
        _out[0].curve = true;
        _out[0].hole  = _kind == FUDE_ZOOM_INSTRUMENT_ELLIPSE;
        _out[0].a     = _t->at;
        _out[0].b     = _t->at;
        return 1u;
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_CORNER) {
        // A corner radius gauge: its sides round from the end of its round, then the round
        // (a quarter turn, its outside away from its centre: the line outside the plate).
        const f64 _r = fude_zoom_corner_r(_t), _h = fude_zoom_corner_h(_t);
        const fude_zoom_v2 _p[5] = { { -_h + _r, -_h }, { _h, -_h }, { _h, _h }, { -_h, _h }, { -_h, -_h + _r } };
        for(u32 _i = 0; _i < 4u; _i++) {
            _out[_i].a = fude_zoom_instrument_place(_t, _p[_i]);
            _out[_i].b = fude_zoom_instrument_place(_t, _p[_i + 1u]);
        }
        _out[4].arc    = true;
        _out[4].centre = fude_zoom_instrument_place(_t, (fude_zoom_v2){ -_h + _r, -_h + _r });
        _out[4].radius = _r;
        _out[4].from   = _t->angle + RDE_PI;
        _out[4].span   = RDE_PI * 0.5;
        _out[4].a      = _out[3].b;
        _out[4].b      = _out[0].a;
        return 5u;
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_CIRCLE) {
        // A circle template: its hole's edge, all the way round.
        const f64 _cr = _t->radius > 1.0 ? _t->radius : FUDE_ZOOM_COMPASS_R;
        _out[0].arc    = true;
        _out[0].centre = _t->at;
        _out[0].radius = _cr;
        _out[0].from   = _t->angle;
        _out[0].span   = 2.0 * RDE_PI;
        _out[0].a      = fude_zoom_instrument_place(_t, (fude_zoom_v2){ _cr, 0.0 });
        _out[0].b      = _out[0].a;
        return 1u;
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_COMPASS) {
        // The compass: the circle its pencil goes round, from where the pencil is.
        const f64 _cr = _t->radius > 1.0 ? _t->radius : FUDE_ZOOM_COMPASS_R;
        _out[0].arc    = true;
        _out[0].centre = _t->at;
        _out[0].radius = _cr;
        _out[0].from   = _t->angle;
        _out[0].span   = 2.0 * RDE_PI;
        _out[0].a      = fude_zoom_instrument_place(_t, (fude_zoom_v2){ _cr, 0.0 });
        _out[0].b      = _out[0].a;
        return 1u;
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_STENCIL) {
        // A stencil: each of its lines an edge of its own, traced (fude_zoom_instrument_curve_of: its points).
        const fude_zoom_stencil* _st = &_ins->stencils[_slot];
        const f64 _k = fude_zoom_instrument_size(_t);
        u32 _n = 0;
        for(u32 _i = 0; _i < _st->lines && _i < FUDE_ZOOM_INSTRUMENT_EDGES; _i++) {
            if(_st->count[_i] < 2u) {
                continue;
            }
            const fude_zoom_v2 _a = _st->points[_st->first[_i]], _b = _st->points[_st->first[_i] + _st->count[_i] - 1u];
            _out[_n].curve = true;
            _out[_n].line  = (u8)_i;
            _out[_n].open  = !_st->closed[_i];
            _out[_n].trace = true;
            _out[_n].a     = fude_zoom_instrument_place(_t, (fude_zoom_v2){ _a.x * _k, _a.y * _k });
            _out[_n].b     = fude_zoom_instrument_place(_t, (fude_zoom_v2){ _b.x * _k, _b.y * _k });
            _n++;
        }
        return _n;
    }
    // The protractor: its straight edge, then its round one.
    const f64 _r = FUDE_ZOOM_PROTRACTOR_R * fude_zoom_instrument_size(_t);
    _out[0].a = fude_zoom_instrument_place(_t, (fude_zoom_v2){ -_r, 0.0 });
    _out[0].b = fude_zoom_instrument_place(_t, (fude_zoom_v2){ _r, 0.0 });
    _out[1].arc       = true;
    _out[1].centre    = _t->at;
    _out[1].radius    = _r;
    _out[1].from      = _t->angle;
    _out[1].a         = _out[0].b;
    _out[1].b         = _out[0].a;
    _out[1].graduated = true;
    _out[1].span      = RDE_PI;
    return 2u;
}

// See the header. A corner gauge's first key is the corner its round rounds, so it snaps onto a corner by it.
u32 fude_zoom_instrument_keys(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2* _out) {
    const fude_zoom_instrument* _t    = &_ins->tools[_slot];
    const u32                   _kind = _t->kind;
    u32 _k = 0;
    if(_kind == FUDE_ZOOM_INSTRUMENT_COMPASS || _kind == FUDE_ZOOM_INSTRUMENT_CIRCLE || _kind == FUDE_ZOOM_INSTRUMENT_ELLIPSE ||
       _kind == FUDE_ZOOM_INSTRUMENT_CURVE) {
        _out[_k++] = _t->at;   // the needle; the hole's centre; the curve's middle
        return _k;
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_PROTRACTOR) {
        _out[_k++] = _t->at;   // its centre, then its straight edge's ends
        _out[_k++] = fude_zoom_instrument_place(_t, (fude_zoom_v2){ -FUDE_ZOOM_PROTRACTOR_R * fude_zoom_instrument_size(_t), 0.0 });
        _out[_k++] = fude_zoom_instrument_place(_t, (fude_zoom_v2){ FUDE_ZOOM_PROTRACTOR_R * fude_zoom_instrument_size(_t), 0.0 });
        return _k;
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_CORNER) {
        const f64 _h = fude_zoom_corner_h(_t);
        _out[_k++] = fude_zoom_instrument_place(_t, (fude_zoom_v2){ -_h, -_h });   // the corner its round rounds: snapped onto one's point
    }
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    const u32 _n = fude_zoom_instrument_edges(_ins, _slot, _e);
    for(u32 _i = 0; _i < _n && _k < FUDE_ZOOM_INSTRUMENT_KEYS; _i++) {
        _out[_k++] = _e[_i].a;   // its corners
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_RULER) {
        // Its zero marks, along both long edges (what a length is measured from).
        const f64 _l = FUDE_ZOOM_RULER_LENGTH * fude_zoom_instrument_size(_t) * 0.5, _w = fude_zoom_ruler_half(_t);
        _out[_k++] = fude_zoom_instrument_place(_t, (fude_zoom_v2){ -_l + FUDE_ZOOM_RULER_ZERO, -_w });
        _out[_k++] = fude_zoom_instrument_place(_t, (fude_zoom_v2){ -_l + FUDE_ZOOM_RULER_ZERO, _w });
    }
    return _k;
}

// See the header.
fude_zoom_v2 fude_zoom_instrument_compass_hinge(const fude_zoom_instruments* _ins, u32 _slot) {
    const fude_zoom_instrument* _t = &_ins->tools[_slot];
    const f64 _cr = _t->radius > 1.0 ? _t->radius : FUDE_ZOOM_COMPASS_R;
    // Above the middle of its legs, as high as they are apart (never too low to take).
    return fude_zoom_instrument_place(_t, (fude_zoom_v2){ _cr * 0.5, rde_math_max_f64(_cr * 0.55, 48.0) });
}

// See the header. A compass's is 30 points above its hinge; the others' are 18 points under their plates, as they
// are turned.
fude_zoom_v2 fude_zoom_instrument_compass_label(const fude_zoom_instruments* _ins, u32 _slot) {
    const fude_zoom_instrument* _t = &_ins->tools[_slot];
    if(_t->kind == FUDE_ZOOM_INSTRUMENT_CIRCLE) {
        const f64 _cr = _t->radius > 1.0 ? _t->radius : FUDE_ZOOM_COMPASS_R;
        return (fude_zoom_v2){ _t->at.x, _t->at.y - _cr - fude_zoom_instrument_rim(_t) - 18.0 };   // under its plate (upright)
    }
    if(_t->kind == FUDE_ZOOM_INSTRUMENT_CORNER) {
        // Under its plate, however it is turned.
        const f64 _h = fude_zoom_corner_h(_t);
        return (fude_zoom_v2){ _t->at.x, _t->at.y - (rde_math_abs_f64(rde_math_sin_f64(_t->angle)) + rde_math_abs_f64(rde_math_cos_f64(_t->angle))) * _h - 18.0 };
    }
    if(_t->kind == FUDE_ZOOM_INSTRUMENT_ELLIPSE) {
        // Under its plate, however it is turned.
        const f64 _ry = (_t->radius2 > 1.0 ? _t->radius2 : FUDE_ZOOM_COMPASS_R * 0.45) + fude_zoom_instrument_rim(_t);
        const f64 _rx = (_t->radius > 1.0 ? _t->radius : FUDE_ZOOM_COMPASS_R * 0.75) + fude_zoom_instrument_rim(_t);
        const f64 _down = rde_math_abs_f64(rde_math_sin_f64(_t->angle)) * _rx + rde_math_abs_f64(rde_math_cos_f64(_t->angle)) * _ry;
        return (fude_zoom_v2){ _t->at.x, _t->at.y - _down - 18.0 };
    }
    const fude_zoom_v2 _h = fude_zoom_instrument_compass_hinge(_ins, _slot);
    return (fude_zoom_v2){ _h.x, _h.y + 30.0 };
}

// See the header.
b8 fude_zoom_instrument_round(u8 _kind) {
    return _kind == FUDE_ZOOM_INSTRUMENT_COMPASS || _kind == FUDE_ZOOM_INSTRUMENT_CIRCLE || _kind == FUDE_ZOOM_INSTRUMENT_ELLIPSE ||
           _kind == FUDE_ZOOM_INSTRUMENT_CORNER;
}

// An ellipse template's hole's half sizes, its plate's.
void fude_zoom_instrument_ellipse(const fude_zoom_instrument* _t, f64* _rx, f64* _ry, f64* _hx, f64* _hy) {
    *_rx = _t->radius > 1.0 ? _t->radius : FUDE_ZOOM_COMPASS_R * 0.75;
    *_ry = _t->radius2 > 1.0 ? _t->radius2 : FUDE_ZOOM_COMPASS_R * 0.45;
    *_hx = *_rx + fude_zoom_instrument_rim(_t);
    *_hy = *_ry + fude_zoom_instrument_rim(_t);
}

// See the header. A French curve's points are as many between each two of FUDE_ZOOM_FRENCH_CURVE's, on the closed
// Catmull-Rom curve through them, so it gives a multiple of their count.
u32 fude_zoom_instrument_curve(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2* _out, u32 _max) {
    const fude_zoom_instrument* _t = &_ins->tools[_slot];
    const u32 _n = _max < FUDE_ZOOM_INSTRUMENT_CURVE_POINTS ? _max : FUDE_ZOOM_INSTRUMENT_CURVE_POINTS;
    if(_t->kind == FUDE_ZOOM_INSTRUMENT_ELLIPSE) {
        f64 _rx, _ry, _hx, _hy;
        fude_zoom_instrument_ellipse(_t, &_rx, &_ry, &_hx, &_hy);
        for(u32 _i = 0; _i < _n; _i++) {
            const f64 _a = 2.0 * RDE_PI * (f64)_i / (f64)_n;
            _out[_i] = fude_zoom_instrument_place(_t, (fude_zoom_v2){ rde_math_cos_f64(_a) * _rx, rde_math_sin_f64(_a) * _ry });
        }
        return _n;
    }
    if(_t->kind == FUDE_ZOOM_INSTRUMENT_CURVE) {
        // Smooth through its points (Catmull-Rom, closed), as many between each two.
        const u32 _m   = FUDE_ZOOM_FRENCH_POINTS;
        const u32 _per = _n / _m;
        const f64 _k   = fude_zoom_instrument_size(_t);
        u32 _o = 0;
        for(u32 _i = 0; _i < _m; _i++) {
            const fude_zoom_v2 _p0 = FUDE_ZOOM_FRENCH_CURVE[(_i + _m - 1u) % _m], _p1 = FUDE_ZOOM_FRENCH_CURVE[_i];
            const fude_zoom_v2 _p2 = FUDE_ZOOM_FRENCH_CURVE[(_i + 1u) % _m], _p3 = FUDE_ZOOM_FRENCH_CURVE[(_i + 2u) % _m];
            for(u32 _s = 0; _s < _per && _o < _n; _s++) {
                const f64 _u = (f64)_s / (f64)_per, _u2 = _u * _u, _u3 = _u2 * _u;
                const fude_zoom_v2 _q = {
                    0.5 * ((2.0 * _p1.x) + (-_p0.x + _p2.x) * _u + (2.0 * _p0.x - 5.0 * _p1.x + 4.0 * _p2.x - _p3.x) * _u2 + (-_p0.x + 3.0 * _p1.x - 3.0 * _p2.x + _p3.x) * _u3),
                    0.5 * ((2.0 * _p1.y) + (-_p0.y + _p2.y) * _u + (2.0 * _p0.y - 5.0 * _p1.y + 4.0 * _p2.y - _p3.y) * _u2 + (-_p0.y + 3.0 * _p1.y - 3.0 * _p2.y + _p3.y) * _u3),
                };
                _out[_o++] = fude_zoom_instrument_place(_t, (fude_zoom_v2){ _q.x * _k, _q.y * _k });
            }
        }
        return _o;
    }
    return 0;
}

// An edge's curve on the screen — a stencil's line, else the instrument's one (an ellipse template's hole, a
// French curve's edge): how many points.
u32 fude_zoom_instrument_curve_of(const fude_zoom_instruments* _ins, u32 _slot, const fude_zoom_instrument_edge* _e, fude_zoom_v2* _out, u32 _max) {
    const fude_zoom_instrument* _t = &_ins->tools[_slot];
    if(_t->kind != FUDE_ZOOM_INSTRUMENT_STENCIL) {
        return fude_zoom_instrument_curve(_ins, _slot, _out, _max);
    }
    const fude_zoom_stencil* _st = &_ins->stencils[_slot];
    if(_e->line >= _st->lines) {
        return 0;
    }
    const u32 _n = _st->count[_e->line] < _max ? _st->count[_e->line] : _max;
    const f64 _k = fude_zoom_instrument_size(_t);
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_v2 _p = _st->points[_st->first[_e->line] + _i];
        _out[_i] = fude_zoom_instrument_place(_t, (fude_zoom_v2){ _p.x * _k, _p.y * _k });
    }
    return _n;
}

// How long a curve is (a closed one, round; _open: from its first point to its last).
f64 fude_zoom_instrument_curve_length(const fude_zoom_v2* _c, u32 _n, b8 _open) {
    f64 _total = 0.0;
    const u32 _segs = _open ? (_n > 0u ? _n - 1u : 0u) : _n;
    for(u32 _i = 0; _i < _segs; _i++) {
        _total += rde_math_hypot_f64(_c[(_i + 1u) % _n].x - _c[_i].x, _c[(_i + 1u) % _n].y - _c[_i].y);
    }
    return _total;
}

// The point of a curve (_n points: closed, or _open with two ends) nearest _p: how far round it is from
// its first point (along it), how far _p is from it. _window > 0: only within that far round either way of
// _near (the pen going on along it, never jumping across).
fude_zoom_v2 fude_zoom_instrument_on_curve(const fude_zoom_v2* _c, u32 _n, fude_zoom_v2 _p, f64 _near, f64 _window, f64* _along, f64* _off, f64* _round, b8 _open) {
    const u32 _segs  = _open ? (_n > 0u ? _n - 1u : 0u) : _n;
    const f64 _total = fude_zoom_instrument_curve_length(_c, _n, _open);
    *_round = _total;
    f64 _best = 1e300, _at = 0.0, _run = 0.0;
    fude_zoom_v2 _q = _n > 0u ? _c[0] : (fude_zoom_v2){ 0.0, 0.0 };
    if(_open && _n == 1u) {
        *_along = 0.0;
        *_off   = rde_math_hypot_f64(_p.x - _q.x, _p.y - _q.y);
        return _q;
    }
    for(u32 _i = 0; _i < _segs; _i++) {
        const fude_zoom_v2 _a = _c[_i], _b = _c[(_i + 1u) % _n];
        const f64 _dx = _b.x - _a.x, _dy = _b.y - _a.y, _l = rde_math_hypot_f64(_dx, _dy);
        const f64 _t  = _l > 0.0 ? rde_math_min_f64(rde_math_max_f64(((_p.x - _a.x) * _dx + (_p.y - _a.y) * _dy) / (_l * _l), 0.0), 1.0) : 0.0;
        const f64 _al = _run + _t * _l;
        _run += _l;
        if(_window > 0.0 && _total > 0.0) {
            f64 _d;
            if(_open) {
                _d = rde_math_abs_f64(_al - _near);
            } else {
                _d = rde_math_fmod_f64(rde_math_abs_f64(_al - rde_math_fmod_f64(_near, _total) + _total), _total);
                _d = rde_math_min_f64(_d, _total - _d);
            }
            if(_d > _window) {
                continue;
            }
        }
        const fude_zoom_v2 _r = { _a.x + _dx * _t, _a.y + _dy * _t };
        const f64 _d = rde_math_hypot_f64(_p.x - _r.x, _p.y - _r.y);
        if(_d < _best) {
            _best = _d;
            _at   = _al;
            _q    = _r;
        }
    }
    *_along = _at;
    *_off   = _best;
    return _q;
}

// A curve's point _along round it (_open: along it, held at its ends), and the way it goes there.
fude_zoom_v2 fude_zoom_instrument_curve_at(const fude_zoom_v2* _c, u32 _n, f64 _along, fude_zoom_v2* _dir, b8 _open) {
    const f64 _total = fude_zoom_instrument_curve_length(_c, _n, _open);
    const u32 _segs  = _open ? (_n > 0u ? _n - 1u : 0u) : _n;
    f64 _left = _open ? rde_math_min_f64(rde_math_max_f64(_along, 0.0), _total) : (_total > 0.0 ? rde_math_fmod_f64(rde_math_fmod_f64(_along, _total) + _total, _total) : 0.0);
    for(u32 _i = 0; _i < _segs; _i++) {
        const fude_zoom_v2 _a = _c[_i], _b = _c[(_i + 1u) % _n];
        const f64 _l = rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y);
        if(_left <= _l || _i + 1u == _segs) {
            const f64 _t = _l > 0.0 ? rde_math_min_f64(_left / _l, 1.0) : 0.0;
            *_dir = _l > 0.0 ? (fude_zoom_v2){ (_b.x - _a.x) / _l, (_b.y - _a.y) / _l } : (fude_zoom_v2){ 1.0, 0.0 };
            return (fude_zoom_v2){ _a.x + (_b.x - _a.x) * _t, _a.y + (_b.y - _a.y) * _t };
        }
        _left -= _l;
    }
    *_dir = (fude_zoom_v2){ 1.0, 0.0 };
    return _c[0];
}

// See the header. A stencil's, an ellipse template's and a circle template's are their plates' rectangles; a French
// curve's is its curve; a compass's is the triangle its legs make, a little wider than they are (what a hand takes
// it by); the others' are their edges, an arc as FUDE_ZOOM_ARC_SIDES sides.
u32 fude_zoom_instrument_outline(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2* _out, u32 _max) {
    if(_ins->tools[_slot].kind == FUDE_ZOOM_INSTRUMENT_STENCIL) {
        // Its plate: its lines' box and a margin.
        const fude_zoom_instrument* _t = &_ins->tools[_slot];
        const f64 _k = fude_zoom_instrument_size(_t), _hw = _ins->stencils[_slot].hw * _k, _hh = _ins->stencils[_slot].hh * _k;
        const fude_zoom_v2 _c[4] = { { -_hw, -_hh }, { _hw, -_hh }, { _hw, _hh }, { -_hw, _hh } };
        u32 _o = 0;
        for(u32 _i = 0; _i < 4u && _o < _max; _i++) {
            _out[_o++] = fude_zoom_instrument_place(_t, _c[_i]);
        }
        return _o;
    }
    if(_ins->tools[_slot].kind == FUDE_ZOOM_INSTRUMENT_CURVE) {
        return fude_zoom_instrument_curve(_ins, _slot, _out, _max);   // the curve is its edge all round
    }
    if(_ins->tools[_slot].kind == FUDE_ZOOM_INSTRUMENT_ELLIPSE) {
        const fude_zoom_instrument* _t = &_ins->tools[_slot];
        f64 _rx, _ry, _hx, _hy;
        fude_zoom_instrument_ellipse(_t, &_rx, &_ry, &_hx, &_hy);
        const fude_zoom_v2 _c[4] = { { -_hx, -_hy }, { _hx, -_hy }, { _hx, _hy }, { -_hx, _hy } };
        u32 _k = 0;
        for(u32 _i = 0; _i < 4u && _k < _max; _i++) {
            _out[_k++] = fude_zoom_instrument_place(_t, _c[_i]);
        }
        return _k;
    }
    if(_ins->tools[_slot].kind == FUDE_ZOOM_INSTRUMENT_CIRCLE) {
        // Its plate: a square round the hole.
        const fude_zoom_instrument* _t = &_ins->tools[_slot];
        const f64 _h = (_t->radius > 1.0 ? _t->radius : FUDE_ZOOM_COMPASS_R) + fude_zoom_instrument_rim(_t);
        const fude_zoom_v2 _c[4] = { { -_h, -_h }, { _h, -_h }, { _h, _h }, { -_h, _h } };
        u32 _k = 0;
        for(u32 _i = 0; _i < 4u && _k < _max; _i++) {
            _out[_k++] = fude_zoom_instrument_place(_t, _c[_i]);
        }
        return _k;
    }
    if(_ins->tools[_slot].kind == FUDE_ZOOM_INSTRUMENT_COMPASS) {
        // What a hand takes it by: its legs (needle, pencil, hinge), a little wider than they are.
        const fude_zoom_instrument* _t = &_ins->tools[_slot];
        const f64 _cr = _t->radius > 1.0 ? _t->radius : FUDE_ZOOM_COMPASS_R;
        const fude_zoom_v2 _l[3] = { { -14.0, -10.0 }, { _cr + 14.0, -10.0 }, { _cr * 0.5, rde_math_max_f64(_cr * 0.55, 48.0) + 18.0 } };
        u32 _k = 0;
        for(u32 _i = 0; _i < 3u && _k < _max; _i++) {
            _out[_k++] = fude_zoom_instrument_place(_t, _l[_i]);
        }
        return _k;
    }
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    const u32 _n = fude_zoom_instrument_edges(_ins, _slot, _e);
    u32 _k = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        if(_e[_i].arc) {
            for(u32 _s = 0; _s < FUDE_ZOOM_ARC_SIDES && _k < _max; _s++) {
                const f64 _t = _e[_i].from + (_e[_i].span > 0.0 ? _e[_i].span : RDE_PI) * (f64)_s / (f64)FUDE_ZOOM_ARC_SIDES;
                _out[_k++] = (fude_zoom_v2){ _e[_i].centre.x + rde_math_cos_f64(_t) * _e[_i].radius, _e[_i].centre.y + rde_math_sin_f64(_t) * _e[_i].radius };
            }
        } else if(_k < _max) {
            _out[_k++] = _e[_i].a;
        }
    }
    return _k;
}

// See the header. Each kind's is where its body has room for it, scaled with its icons.
fude_zoom_v2 fude_zoom_instrument_knob(const fude_zoom_instruments* _ins, u32 _slot) {
    const fude_zoom_instrument* _t    = &_ins->tools[_slot];
    const u32                   _kind = _t->kind;
    if(_kind == FUDE_ZOOM_INSTRUMENT_RULER) {
        return fude_zoom_instrument_place(_t, (fude_zoom_v2){ FUDE_ZOOM_RULER_LENGTH * fude_zoom_instrument_size(_t) * 0.5 - 24.0 * fude_zoom_instrument_icons(_t), 0.0 });
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_PROTRACTOR) {
        return fude_zoom_instrument_place(_t, (fude_zoom_v2){ 0.0, FUDE_ZOOM_PROTRACTOR_R * fude_zoom_instrument_size(_t) * 0.42 });
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_COMPASS) {
        return fude_zoom_instrument_compass_hinge(_ins, _slot);
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_CIRCLE) {
        return (fude_zoom_v2){ 1e12, 1e12 };   // (none: a round hole needs no turning)
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_CORNER) {
        const f64 _h = fude_zoom_corner_h(_t);
        return fude_zoom_instrument_place(_t, (fude_zoom_v2){ _h - 20.0 * fude_zoom_instrument_icons(_t), _h - 20.0 * fude_zoom_instrument_icons(_t) });   // its plate's far corner
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_ELLIPSE) {
        f64 _rx, _ry, _hx, _hy;
        fude_zoom_instrument_ellipse(_t, &_rx, &_ry, &_hx, &_hy);
        return fude_zoom_instrument_place(_t, (fude_zoom_v2){ _hx - fude_zoom_instrument_rim(_t) * 0.5, -_hy + fude_zoom_instrument_rim(_t) * 0.5 });   // its plate's lower right corner
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_CURVE) {
        const f64 _k = fude_zoom_instrument_size(_t);
        return fude_zoom_instrument_place(_t, (fude_zoom_v2){ -70.0 * _k, -30.0 * _k });   // in its wide part
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_STENCIL) {
        const f64 _k = fude_zoom_instrument_size(_t), _g = 20.0 * fude_zoom_instrument_icons(_t);
        return fude_zoom_instrument_place(_t, (fude_zoom_v2){ _ins->stencils[_slot].hw * _k - _g, -_ins->stencils[_slot].hh * _k + _g });   // its plate's lower right corner
    }
    // A set square's: in the band near its sharpest corner.
    fude_zoom_v2 _c[3];
    fude_zoom_square_corners(_kind, fude_zoom_instrument_size(_t), _c);
    return fude_zoom_instrument_place(_t, (fude_zoom_v2){ _c[0].x + (_c[1].x - _c[0].x) * 0.70, _c[0].y + 22.0 * fude_zoom_instrument_icons(_t) });
}

// See the header.
fude_zoom_v2 fude_zoom_instrument_local(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2 _screen) {
    return fude_zoom_instrument_unplace(&_ins->tools[_slot], _screen);
}

// See the header.
fude_zoom_v2 fude_zoom_instrument_screen(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2 _local) {
    return fude_zoom_instrument_place(&_ins->tools[_slot], _local);
}

// See the header. Each kind's is beside its knob or in a corner of its plate, scaled with its icons.
fude_zoom_v2 fude_zoom_instrument_close_at(const fude_zoom_instruments* _ins, u32 _slot) {
    const fude_zoom_instrument* _t    = &_ins->tools[_slot];
    const u32                   _kind = _t->kind;
    if(_kind == FUDE_ZOOM_INSTRUMENT_RULER) {
        return fude_zoom_instrument_place(_t, (fude_zoom_v2){ FUDE_ZOOM_RULER_LENGTH * fude_zoom_instrument_size(_t) * 0.5 - 58.0 * fude_zoom_instrument_icons(_t), 0.0 });   // beside its knob
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_PROTRACTOR) {
        return fude_zoom_instrument_place(_t, (fude_zoom_v2){ -34.0 * fude_zoom_instrument_icons(_t), FUDE_ZOOM_PROTRACTOR_R * fude_zoom_instrument_size(_t) * 0.42 });
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_COMPASS) {
        const fude_zoom_v2 _h = fude_zoom_instrument_compass_hinge(_ins, _slot);
        return (fude_zoom_v2){ _h.x + 36.0 * fude_zoom_instrument_icons(_t), _h.y };   // right of its hinge (its radius is written above it)
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_CIRCLE) {
        const f64 _h = (_t->radius > 1.0 ? _t->radius : FUDE_ZOOM_COMPASS_R) + fude_zoom_instrument_rim(_t) * 0.5;
        return fude_zoom_instrument_place(_t, (fude_zoom_v2){ _h, _h });   // in its plate's corner
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_CORNER) {
        const f64 _h = fude_zoom_corner_h(_t);
        return fude_zoom_instrument_place(_t, (fude_zoom_v2){ _h - 54.0 * fude_zoom_instrument_icons(_t), _h - 20.0 * fude_zoom_instrument_icons(_t) });   // beside its knob
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_ELLIPSE) {
        f64 _rx, _ry, _hx, _hy;
        fude_zoom_instrument_ellipse(_t, &_rx, &_ry, &_hx, &_hy);
        return fude_zoom_instrument_place(_t, (fude_zoom_v2){ _hx - fude_zoom_instrument_rim(_t) * 0.5, _hy - fude_zoom_instrument_rim(_t) * 0.5 });
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_CURVE) {
        const f64 _k = fude_zoom_instrument_size(_t);
        return fude_zoom_instrument_place(_t, (fude_zoom_v2){ -28.0 * _k, -38.0 * _k });
    }
    if(_kind == FUDE_ZOOM_INSTRUMENT_STENCIL) {
        const f64 _k = fude_zoom_instrument_size(_t), _g = 20.0 * fude_zoom_instrument_icons(_t);
        return fude_zoom_instrument_place(_t, (fude_zoom_v2){ _ins->stencils[_slot].hw * _k - _g, _ins->stencils[_slot].hh * _k - _g });   // its upper right corner
    }
    fude_zoom_v2 _c[3];
    fude_zoom_square_corners(_kind, fude_zoom_instrument_size(_t), _c);
    return fude_zoom_instrument_place(_t, (fude_zoom_v2){ _c[0].x + (_c[1].x - _c[0].x) * 0.52, _c[0].y + 22.0 * fude_zoom_instrument_icons(_t) });
}

// Its size tab (a compass has none: its hinge opens it), just past the end that
// grows, and the point that stays where it is as it grows (a ruler's zero end, a
// set square's right angle, a protractor's centre). Its own units.
b8 fude_zoom_instrument_grow_local(const fude_zoom_instrument* _t, fude_zoom_v2* _tab, fude_zoom_v2* _fixed) {
    const f64 _k = fude_zoom_instrument_size(_t);
    if(_t->kind == FUDE_ZOOM_INSTRUMENT_RULER) {
        *_tab   = (fude_zoom_v2){ FUDE_ZOOM_RULER_LENGTH * _k * 0.5 + FUDE_ZOOM_INSTRUMENT_GROW_OUT * fude_zoom_instrument_icons(_t), 0.0 };
        *_fixed = (fude_zoom_v2){ -FUDE_ZOOM_RULER_LENGTH * _k * 0.5, 0.0 };
        return true;
    }
    if(_t->kind == FUDE_ZOOM_INSTRUMENT_PROTRACTOR) {
        *_tab   = (fude_zoom_v2){ FUDE_ZOOM_PROTRACTOR_R * _k + FUDE_ZOOM_INSTRUMENT_GROW_OUT * fude_zoom_instrument_icons(_t), 0.0 };
        *_fixed = (fude_zoom_v2){ 0.0, 0.0 };
        return true;
    }
    if(_t->kind == FUDE_ZOOM_INSTRUMENT_CURVE) {
        *_tab   = (fude_zoom_v2){ 152.0 * _k + FUDE_ZOOM_INSTRUMENT_GROW_OUT * fude_zoom_instrument_icons(_t), 70.0 * _k };   // past its loose end
        *_fixed = (fude_zoom_v2){ -150.0 * _k, 70.0 * _k };
        return true;
    }
    if(_t->kind == FUDE_ZOOM_INSTRUMENT_STENCIL) {
        // (its plate's half sizes kept in radius, radius2: the page's, as it laid it)
        *_tab   = (fude_zoom_v2){ _t->radius * _k + FUDE_ZOOM_INSTRUMENT_GROW_OUT * fude_zoom_instrument_icons(_t), 0.0 };
        *_fixed = (fude_zoom_v2){ -_t->radius * _k, 0.0 };
        return true;
    }
    if(_t->kind == FUDE_ZOOM_INSTRUMENT_SQUARE_45 || _t->kind == FUDE_ZOOM_INSTRUMENT_SQUARE_30) {
        fude_zoom_v2 _c[3];
        fude_zoom_square_corners(_t->kind, _k, _c);
        *_tab   = (fude_zoom_v2){ _c[1].x + FUDE_ZOOM_INSTRUMENT_GROW_OUT * fude_zoom_instrument_icons(_t), _c[1].y };
        *_fixed = _c[0];
        return true;
    }
    return false;
}

// See the header.
b8 fude_zoom_instrument_grow_at(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2* _at) {
    fude_zoom_v2 _tab, _fixed;
    if(!fude_zoom_instrument_grow_local(&_ins->tools[_slot], &_tab, &_fixed)) {
        return false;
    }
    *_at = fude_zoom_instrument_place(&_ins->tools[_slot], _tab);
    return true;
}

// See the header. 302 is a French curve's width at size 1 (its points span -150 to 152).
f64 fude_zoom_instrument_extent_unit(u8 _kind) {
    switch(_kind) {
        case FUDE_ZOOM_INSTRUMENT_RULER:      return FUDE_ZOOM_RULER_LENGTH;
        case FUDE_ZOOM_INSTRUMENT_SQUARE_45:  return FUDE_ZOOM_SQUARE_45_LEG;
        case FUDE_ZOOM_INSTRUMENT_SQUARE_30:  return FUDE_ZOOM_SQUARE_30_LEG;
        case FUDE_ZOOM_INSTRUMENT_PROTRACTOR: return FUDE_ZOOM_PROTRACTOR_R;
        case FUDE_ZOOM_INSTRUMENT_CURVE:      return 302.0;
        case FUDE_ZOOM_INSTRUMENT_STENCIL:    return FUDE_ZOOM_STENCIL_W;
        default:                              return 1.0;
    }
}

// See the header.
f64 fude_zoom_instrument_extent(const fude_zoom_instruments* _ins, u32 _slot) {
    const fude_zoom_instrument* _t = &_ins->tools[_slot];
    const f64 _k = fude_zoom_instrument_size(_t);
    switch(_t->kind) {
        case FUDE_ZOOM_INSTRUMENT_RULER:      return FUDE_ZOOM_RULER_LENGTH * _k;
        case FUDE_ZOOM_INSTRUMENT_SQUARE_45:  return FUDE_ZOOM_SQUARE_45_LEG * _k;
        case FUDE_ZOOM_INSTRUMENT_SQUARE_30:  return FUDE_ZOOM_SQUARE_30_LEG * _k;
        case FUDE_ZOOM_INSTRUMENT_PROTRACTOR: return FUDE_ZOOM_PROTRACTOR_R * _k;
        case FUDE_ZOOM_INSTRUMENT_CURVE:      return 302.0 * _k;
        case FUDE_ZOOM_INSTRUMENT_STENCIL:    return FUDE_ZOOM_STENCIL_W * _k;
        default:                              return _t->radius;
    }
}

// The shown one whose size tab is at _p, the one on top (-1: none).
i32 fude_zoom_instruments_grow_under(const fude_zoom_instruments* _ins, fude_zoom_v2 _p) {
    for(u32 _i = FUDE_ZOOM_INSTRUMENT_MOST; _i-- > 0;) {
        const u32 _k = _ins->order[_i];
        fude_zoom_v2 _at;
        if(_ins->tools[_k].shown && fude_zoom_instrument_grow_at(_ins, _k, &_at) && rde_math_hypot_f64(_p.x - _at.x, _p.y - _at.y) <= FUDE_ZOOM_INSTRUMENT_GROW * fude_zoom_instrument_icons(&_ins->tools[_k]) + 6.0) {
            return (i32)_k;
        }
    }
    return -1;
}

// A hand takes slot _k by its size tab.
void fude_zoom_instruments_take_grow(fude_zoom_instruments* _ins, u32 _k, u64 _id, fude_zoom_v2 _at) {
    _ins->held       = (i32)_k;
    _ins->hands      = 1u;
    _ins->hand_id[0] = _id;
    _ins->hand_at[0] = _at;
    _ins->turning    = false;
    _ins->growing    = true;
    _ins->docked     = -1;
    fude_zoom_instrument_raise(_ins, _k);
    fude_zoom_instruments_regrab(_ins);
}

// The shown one whose × is at _p, the one on top (-1: none).
i32 fude_zoom_instruments_close_under(const fude_zoom_instruments* _ins, fude_zoom_v2 _p) {
    for(u32 _i = FUDE_ZOOM_INSTRUMENT_MOST; _i-- > 0;) {
        const u32 _k = _ins->order[_i];
        if(!_ins->tools[_k].shown) {
            continue;
        }
        const fude_zoom_v2 _x = fude_zoom_instrument_close_at(_ins, _k);
        if(rde_math_hypot_f64(_p.x - _x.x, _p.y - _x.y) <= FUDE_ZOOM_INSTRUMENT_CLOSE * fude_zoom_instrument_icons(&_ins->tools[_k]) + 5.0) {
            return (i32)_k;
        }
    }
    return -1;
}

// Returns whether _p is on instrument _slot's body (what a hand takes it by): a stencil's plate away from its lines,
// a template's plate and not its hole, a French curve's inside its curve, else inside its outline (even-odd).
b8 fude_zoom_instrument_inside(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2 _p) {
    if(_ins->tools[_slot].kind == FUDE_ZOOM_INSTRUMENT_STENCIL) {
        // Its plate, but not near its lines (the pen there draws along one).
        const fude_zoom_instrument* _t = &_ins->tools[_slot];
        const f64 _k = fude_zoom_instrument_size(_t);
        const fude_zoom_v2 _l = fude_zoom_instrument_unplace(_t, _p);
        if(rde_math_abs_f64(_l.x) > _ins->stencils[_slot].hw * _k || rde_math_abs_f64(_l.y) > _ins->stencils[_slot].hh * _k) {
            return false;
        }
        return fude_zoom_instrument_edge_off(_ins, _slot, _p) > FUDE_ZOOM_INSTRUMENT_SNAP * 0.9;
    }
    if(_ins->tools[_slot].kind == FUDE_ZOOM_INSTRUMENT_ELLIPSE) {
        const fude_zoom_instrument* _t = &_ins->tools[_slot];
        f64 _rx, _ry, _hx, _hy;
        fude_zoom_instrument_ellipse(_t, &_rx, &_ry, &_hx, &_hy);
        const fude_zoom_v2 _l = fude_zoom_instrument_unplace(_t, _p);
        return rde_math_abs_f64(_l.x) <= _hx && rde_math_abs_f64(_l.y) <= _hy && (_l.x * _l.x) / (_rx * _rx) + (_l.y * _l.y) / (_ry * _ry) > 1.0;
    }
    if(_ins->tools[_slot].kind == FUDE_ZOOM_INSTRUMENT_CURVE) {
        fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
        const u32 _n = fude_zoom_instrument_curve(_ins, _slot, _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
        return _n >= 3u && fude_zoom_fill_inside(_c, _n, _p);
    }
    if(_ins->tools[_slot].kind == FUDE_ZOOM_INSTRUMENT_CIRCLE) {
        // Its plate, not its hole (the pen in the hole draws).
        const fude_zoom_instrument* _t = &_ins->tools[_slot];
        const f64 _cr = _t->radius > 1.0 ? _t->radius : FUDE_ZOOM_COMPASS_R;
        const fude_zoom_v2 _l = fude_zoom_instrument_unplace(_t, _p);
        return rde_math_abs_f64(_l.x) <= _cr + fude_zoom_instrument_rim(_t) && rde_math_abs_f64(_l.y) <= _cr + fude_zoom_instrument_rim(_t) && rde_math_hypot_f64(_l.x, _l.y) > _cr;
    }
    fude_zoom_v2 _o[FUDE_ZOOM_ARC_SIDES + 8u];
    const u32 _n = fude_zoom_instrument_outline(_ins, _slot, _o, FUDE_ZOOM_ARC_SIDES + 8u);
    return rde_math_point_in_polygon_2d_f64(_p, _o, _n);   // (every outline here has 3 corners at least)
}

// See the header: from the top of the pile down.
i32 fude_zoom_instruments_at(const fude_zoom_instruments* _ins, fude_zoom_v2 _screen) {
    for(u32 _i = FUDE_ZOOM_INSTRUMENT_MOST; _i-- > 0;) {
        const u32 _k = _ins->order[_i];
        if(_ins->tools[_k].shown && fude_zoom_instrument_inside(_ins, _k, _screen)) {
            return (i32)_k;
        }
    }
    return -1;
}

// --- shown, put away -------------------------------------------------------------------------

// See the header. The pile's order starts as the slots' own.
void fude_zoom_instruments_init(fude_zoom_instruments* _ins) {
    rde_memset(_ins, 0, sizeof(*_ins));
    for(u32 _i = 0; _i < FUDE_ZOOM_INSTRUMENT_MOST; _i++) {
        _ins->order[_i] = (u8)_i;
    }
    _ins->held    = -1;
    _ins->docked  = -1;
    _ins->ruled   = -1;
    _ins->closing = -1;
}

// See the header.
b8 fude_zoom_instruments_any(const fude_zoom_instruments* _ins) {
    for(u32 _i = 0; _i < FUDE_ZOOM_INSTRUMENT_MOST; _i++) {
        if(_ins->tools[_i].shown) {
            return true;
        }
    }
    return false;
}

// See the header.
u32 fude_zoom_instruments_count(const fude_zoom_instruments* _ins, FUDE_ZOOM_INSTRUMENT_ _kind) {
    u32 _n = 0;
    for(u32 _i = 0; _i < FUDE_ZOOM_INSTRUMENT_MOST; _i++) {
        _n += _ins->tools[_i].shown && _ins->tools[_i].kind == (u8)_kind ? 1u : 0u;
    }
    return _n;
}

// Brought to the top of the pile.
void fude_zoom_instrument_raise(fude_zoom_instruments* _ins, u32 _slot) {
    u32 _w = 0;
    for(u32 _i = 0; _i < FUDE_ZOOM_INSTRUMENT_MOST; _i++) {
        if(_ins->order[_i] != _slot) {
            _ins->order[_w++] = _ins->order[_i];
        }
    }
    _ins->order[FUDE_ZOOM_INSTRUMENT_MOST - 1u] = (u8)_slot;
}

// See the header. The first free slot is used, cleared, and given the next serial; a round one starts at its first
// sizes. Each of a kind goes 36 points further down and right; the seventh goes back to where the first went.
i32 fude_zoom_instrument_add(fude_zoom_instruments* _ins, FUDE_ZOOM_INSTRUMENT_ _kind, fude_zoom_v2 _half) {
    i32 _slot = -1;
    for(u32 _i = 0; _i < FUDE_ZOOM_INSTRUMENT_MOST && _slot < 0; _i++) {
        if(!_ins->tools[_i].shown) {
            _slot = (i32)_i;
        }
    }
    if(_slot < 0) {
        return -1;
    }
    const u32 _before = fude_zoom_instruments_count(_ins, _kind);
    fude_zoom_instrument* _t = &_ins->tools[_slot];
    rde_memset(_t, 0, sizeof(*_t));
    _t->shown  = true;
    _t->kind   = (u8)_kind;
    _t->serial = ++_ins->serials;
    _t->size   = 1.0;
    _t->thick  = 1.0;
    _t->pin    = -1;
    // Where a hand would put it: the ruler across the middle, a little low;
    // the set squares and the protractor a little above and to either side;
    // another of the same a little further down and to the right.
    const f64 _w = rde_math_min_f64(_half.x, 400.0);
    switch(_kind) {
        case FUDE_ZOOM_INSTRUMENT_RULER:      _t->at = (fude_zoom_v2){ 0.0, -_half.y * 0.25 }; break;
        case FUDE_ZOOM_INSTRUMENT_SQUARE_45:  _t->at = (fude_zoom_v2){ -_w * 0.35, _half.y * 0.15 }; break;
        case FUDE_ZOOM_INSTRUMENT_SQUARE_30:  _t->at = (fude_zoom_v2){ _w * 0.30, _half.y * 0.15 }; break;
        case FUDE_ZOOM_INSTRUMENT_COMPASS:    _t->at = (fude_zoom_v2){ -FUDE_ZOOM_COMPASS_R * 0.5, -_half.y * 0.1 };
                                              _t->radius = FUDE_ZOOM_COMPASS_R; break;
        case FUDE_ZOOM_INSTRUMENT_CIRCLE:     _t->at = (fude_zoom_v2){ _w * 0.25, -_half.y * 0.05 };
                                              _t->radius = FUDE_ZOOM_COMPASS_R * 0.6; break;
        case FUDE_ZOOM_INSTRUMENT_ELLIPSE:    _t->at = (fude_zoom_v2){ _w * 0.25, -_half.y * 0.05 };
                                              _t->radius = FUDE_ZOOM_COMPASS_R * 0.75; _t->radius2 = FUDE_ZOOM_COMPASS_R * 0.45; break;
        case FUDE_ZOOM_INSTRUMENT_CURVE:      _t->at = (fude_zoom_v2){ -_w * 0.15, -_half.y * 0.2 }; break;
        case FUDE_ZOOM_INSTRUMENT_CORNER:     _t->at = (fude_zoom_v2){ -_w * 0.25, -_half.y * 0.05 };
                                              _t->radius = FUDE_ZOOM_COMPASS_R * 0.5; break;
        default:                              _t->at = (fude_zoom_v2){ 0.0, _half.y * 0.05 }; break;
    }
    const f64 _step = 36.0 * (f64)(_before % 6u);
    _t->at = (fude_zoom_v2){ _t->at.x + _step, _t->at.y - _step };
    fude_zoom_instrument_raise(_ins, (u32)_slot);
    return _slot;
}

// See the header. A slot out of range does nothing.
void fude_zoom_instrument_remove(fude_zoom_instruments* _ins, u32 _slot) {
    if(_slot >= FUDE_ZOOM_INSTRUMENT_MOST) {
        return;
    }
    _ins->tools[_slot].shown = false;
    if(_ins->held == (i32)_slot)    { _ins->held = -1; _ins->hands = 0; _ins->turning = false; _ins->growing = false; _ins->docked = -1; }
    if(_ins->ruled == (i32)_slot)   { _ins->ruled = -1; }
    if(_ins->docked == (i32)_slot)  { _ins->docked = -1; }
    if(_ins->closing == (i32)_slot) { _ins->closing = -1; }
}
