// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/instrument.h"
#include "zoom/fill.h"
#include "drawing/widgets/draw.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FUDE_ZOOM_INSTRUMENT_PI 3.14159265358979323846

// Their sizes on the screen.
#define FUDE_ZOOM_RULER_LENGTH   560.0
#define FUDE_ZOOM_RULER_WIDTH    FUDE_ZOOM_INSTRUMENT_RULER_WIDTH
#define FUDE_ZOOM_RULER_ZERO     12.0    // its graduations start this far in from its end
#define FUDE_ZOOM_SQUARE_45_LEG  280.0
#define FUDE_ZOOM_SQUARE_30_LEG  340.0   // the long leg (the short one: this over √3)
#define FUDE_ZOOM_SQUARE_HOLLOW  36.0    // the band round a set square's hollow middle
#define FUDE_ZOOM_PROTRACTOR_R   160.0
#define FUDE_ZOOM_ARC_SIDES      72u
#define FUDE_ZOOM_CIRCLE_RIM     34.0    // a circle template's plate round its hole (an ellipse template's too)
#define FUDE_ZOOM_CORNER_BAND    48.0    // a corner radius gauge's plate past its round

// A French curve's outline (its own units, round it counter-clockwise; a smooth
// curve through them): a long gentle side, a tight turn at one end, a looser
// one at the other, about 300 by 160.
static const fude_zoom_v2 FUDE_ZOOM_FRENCH_CURVE[] = {
    { -150.0, -10.0 }, { -132.0, -52.0 }, { -88.0, -76.0 }, { -26.0, -78.0 }, { 40.0, -58.0 }, { 98.0, -20.0 },
    { 140.0, 32.0 }, { 152.0, 70.0 }, { 128.0, 82.0 }, { 92.0, 56.0 }, { 46.0, 28.0 }, { -6.0, 16.0 },
    { -62.0, 22.0 }, { -110.0, 30.0 }, { -142.0, 18.0 },
};
#define FUDE_ZOOM_FRENCH_POINTS (sizeof(FUDE_ZOOM_FRENCH_CURVE) / sizeof(FUDE_ZOOM_FRENCH_CURVE[0]))

// --- where things are ------------------------------------------------------------------------

RDE_INTERNAL void fude_zoom_instrument_raise(fude_zoom_instruments* _ins, u32 _slot);
RDE_INTERNAL void fude_zoom_instruments_regrab(fude_zoom_instruments* _ins);

RDE_INTERNAL fude_zoom_v2 fude_zoom_instrument_place(const fude_zoom_instrument* _t, fude_zoom_v2 _local) {
    const f64 _c = cos(_t->angle), _s = sin(_t->angle);
    return (fude_zoom_v2){ _t->at.x + _local.x * _c - _local.y * _s, _t->at.y + _local.x * _s + _local.y * _c };
}

RDE_INTERNAL fude_zoom_v2 fude_zoom_instrument_unplace(const fude_zoom_instrument* _t, fude_zoom_v2 _screen) {
    const f64 _c = cos(_t->angle), _s = sin(_t->angle);
    const f64 _x = _screen.x - _t->at.x, _y = _screen.y - _t->at.y;
    return (fude_zoom_v2){ _x * _c + _y * _s, -_x * _s + _y * _c };
}

// How much bigger than when it came out (a ruler: longer; a set square, a protractor: bigger).
// A ruler's band's half (as wide as it lies on the drawing).
RDE_INTERNAL f64 fude_zoom_ruler_half(const fude_zoom_instrument* _t) {
    return FUDE_ZOOM_RULER_WIDTH * 0.5 * (_t->thick > 0.0 ? _t->thick : 1.0);
}

// A corner radius gauge's round, and its plate's half side (the round at its lower left).
RDE_INTERNAL f64 fude_zoom_corner_r(const fude_zoom_instrument* _t) {
    return _t->radius > 1.0 ? _t->radius : FUDE_ZOOM_COMPASS_R * 0.5;
}

f64 fude_zoom_instrument_band(const fude_zoom_instrument* _t) {
    if(_t->radius2 > 1.0) {
        return _t->radius2;
    }
    return _t->kind == FUDE_ZOOM_INSTRUMENT_CORNER ? FUDE_ZOOM_CORNER_BAND : FUDE_ZOOM_CIRCLE_RIM;
}

// Its knob's, ×'s and tabs' scale: as much bigger as it is on the screen (instrument.h' look), not too small to take nor huge.
RDE_INTERNAL f64 fude_zoom_instrument_icons(const fude_zoom_instrument* _t) {
    return fmin(fmax(_t->look > 0.0 ? _t->look : 1.0, 0.3), 2.0);
}

// A circle template's rim (real: its band), an ellipse template's (as much bigger as it is).
RDE_INTERNAL f64 fude_zoom_instrument_rim(const fude_zoom_instrument* _t) {
    if(_t->kind == FUDE_ZOOM_INSTRUMENT_CIRCLE) {
        return fude_zoom_instrument_band(_t);
    }
    return FUDE_ZOOM_CIRCLE_RIM * (_t->look > 0.0 ? _t->look : 1.0);
}

RDE_INTERNAL f64 fude_zoom_corner_h(const fude_zoom_instrument* _t) {
    return (fude_zoom_corner_r(_t) + fude_zoom_instrument_band(_t)) * 0.5;
}

RDE_INTERNAL f64 fude_zoom_instrument_size(const fude_zoom_instrument* _t) {
    return _t->size > 0.05 ? _t->size : 1.0;
}

// A set square's corners (its own units, the right angle first, then along
// its long leg, then the short), round its middle.
RDE_INTERNAL void fude_zoom_square_corners(u32 _kind, f64 _size, fude_zoom_v2 _out[3]) {
    const f64 _a = (_kind == FUDE_ZOOM_INSTRUMENT_SQUARE_45 ? FUDE_ZOOM_SQUARE_45_LEG : FUDE_ZOOM_SQUARE_30_LEG) * _size;
    const f64 _b = (_kind == FUDE_ZOOM_INSTRUMENT_SQUARE_45 ? FUDE_ZOOM_SQUARE_45_LEG : FUDE_ZOOM_SQUARE_30_LEG / sqrt(3.0)) * _size;
    _out[0] = (fude_zoom_v2){ -_a / 3.0, -_b / 3.0 };
    _out[1] = (fude_zoom_v2){ 2.0 * _a / 3.0, -_b / 3.0 };
    _out[2] = (fude_zoom_v2){ -_a / 3.0, 2.0 * _b / 3.0 };
}

u32 fude_zoom_instrument_edges(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_instrument_edge* _out) {
    const fude_zoom_instrument* _t    = &_ins->tools[_slot];
    const u32                   _kind = _t->kind;
    memset(_out, 0, sizeof(fude_zoom_instrument_edge) * FUDE_ZOOM_INSTRUMENT_EDGES);
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
        _out[4].from   = _t->angle + FUDE_ZOOM_INSTRUMENT_PI;
        _out[4].span   = FUDE_ZOOM_INSTRUMENT_PI * 0.5;
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
        _out[0].span   = 2.0 * FUDE_ZOOM_INSTRUMENT_PI;
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
        _out[0].span   = 2.0 * FUDE_ZOOM_INSTRUMENT_PI;
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
    _out[1].span      = FUDE_ZOOM_INSTRUMENT_PI;
    return 2u;
}

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

fude_zoom_v2 fude_zoom_instrument_compass_hinge(const fude_zoom_instruments* _ins, u32 _slot) {
    const fude_zoom_instrument* _t = &_ins->tools[_slot];
    const f64 _cr = _t->radius > 1.0 ? _t->radius : FUDE_ZOOM_COMPASS_R;
    // Above the middle of its legs, as high as they are apart (never too low to take).
    return fude_zoom_instrument_place(_t, (fude_zoom_v2){ _cr * 0.5, fmax(_cr * 0.55, 48.0) });
}

fude_zoom_v2 fude_zoom_instrument_compass_label(const fude_zoom_instruments* _ins, u32 _slot) {
    const fude_zoom_instrument* _t = &_ins->tools[_slot];
    if(_t->kind == FUDE_ZOOM_INSTRUMENT_CIRCLE) {
        const f64 _cr = _t->radius > 1.0 ? _t->radius : FUDE_ZOOM_COMPASS_R;
        return (fude_zoom_v2){ _t->at.x, _t->at.y - _cr - fude_zoom_instrument_rim(_t) - 18.0 };   // under its plate (upright)
    }
    if(_t->kind == FUDE_ZOOM_INSTRUMENT_CORNER) {
        // Under its plate, however it is turned.
        const f64 _h = fude_zoom_corner_h(_t);
        return (fude_zoom_v2){ _t->at.x, _t->at.y - (fabs(sin(_t->angle)) + fabs(cos(_t->angle))) * _h - 18.0 };
    }
    if(_t->kind == FUDE_ZOOM_INSTRUMENT_ELLIPSE) {
        // Under its plate, however it is turned.
        const f64 _ry = (_t->radius2 > 1.0 ? _t->radius2 : FUDE_ZOOM_COMPASS_R * 0.45) + fude_zoom_instrument_rim(_t);
        const f64 _rx = (_t->radius > 1.0 ? _t->radius : FUDE_ZOOM_COMPASS_R * 0.75) + fude_zoom_instrument_rim(_t);
        const f64 _down = fabs(sin(_t->angle)) * _rx + fabs(cos(_t->angle)) * _ry;
        return (fude_zoom_v2){ _t->at.x, _t->at.y - _down - 18.0 };
    }
    const fude_zoom_v2 _h = fude_zoom_instrument_compass_hinge(_ins, _slot);
    return (fude_zoom_v2){ _h.x, _h.y + 30.0 };
}

b8 fude_zoom_instrument_round(u8 _kind) {
    return _kind == FUDE_ZOOM_INSTRUMENT_COMPASS || _kind == FUDE_ZOOM_INSTRUMENT_CIRCLE || _kind == FUDE_ZOOM_INSTRUMENT_ELLIPSE ||
           _kind == FUDE_ZOOM_INSTRUMENT_CORNER;
}

// An ellipse template's hole's half sizes, its plate's.
RDE_INTERNAL void fude_zoom_instrument_ellipse(const fude_zoom_instrument* _t, f64* _rx, f64* _ry, f64* _hx, f64* _hy) {
    *_rx = _t->radius > 1.0 ? _t->radius : FUDE_ZOOM_COMPASS_R * 0.75;
    *_ry = _t->radius2 > 1.0 ? _t->radius2 : FUDE_ZOOM_COMPASS_R * 0.45;
    *_hx = *_rx + fude_zoom_instrument_rim(_t);
    *_hy = *_ry + fude_zoom_instrument_rim(_t);
}

u32 fude_zoom_instrument_curve(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2* _out, u32 _max) {
    const fude_zoom_instrument* _t = &_ins->tools[_slot];
    const u32 _n = _max < FUDE_ZOOM_INSTRUMENT_CURVE_POINTS ? _max : FUDE_ZOOM_INSTRUMENT_CURVE_POINTS;
    if(_t->kind == FUDE_ZOOM_INSTRUMENT_ELLIPSE) {
        f64 _rx, _ry, _hx, _hy;
        fude_zoom_instrument_ellipse(_t, &_rx, &_ry, &_hx, &_hy);
        for(u32 _i = 0; _i < _n; _i++) {
            const f64 _a = 2.0 * FUDE_ZOOM_INSTRUMENT_PI * (f64)_i / (f64)_n;
            _out[_i] = fude_zoom_instrument_place(_t, (fude_zoom_v2){ cos(_a) * _rx, sin(_a) * _ry });
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
RDE_INTERNAL u32 fude_zoom_instrument_curve_of(const fude_zoom_instruments* _ins, u32 _slot, const fude_zoom_instrument_edge* _e, fude_zoom_v2* _out, u32 _max) {
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
RDE_INTERNAL f64 fude_zoom_instrument_curve_length(const fude_zoom_v2* _c, u32 _n, b8 _open) {
    f64 _total = 0.0;
    const u32 _segs = _open ? (_n > 0u ? _n - 1u : 0u) : _n;
    for(u32 _i = 0; _i < _segs; _i++) {
        _total += hypot(_c[(_i + 1u) % _n].x - _c[_i].x, _c[(_i + 1u) % _n].y - _c[_i].y);
    }
    return _total;
}

// The point of a curve (_n points: closed, or _open with two ends) nearest _p: how far round it is from
// its first point (along it), how far _p is from it. _window > 0: only within that far round either way of
// _near (the pen going on along it, never jumping across).
RDE_INTERNAL fude_zoom_v2 fude_zoom_instrument_on_curve(const fude_zoom_v2* _c, u32 _n, fude_zoom_v2 _p, f64 _near, f64 _window, f64* _along, f64* _off, f64* _round, b8 _open) {
    const u32 _segs  = _open ? (_n > 0u ? _n - 1u : 0u) : _n;
    const f64 _total = fude_zoom_instrument_curve_length(_c, _n, _open);
    *_round = _total;
    f64 _best = 1e300, _at = 0.0, _run = 0.0;
    fude_zoom_v2 _q = _n > 0u ? _c[0] : (fude_zoom_v2){ 0.0, 0.0 };
    if(_open && _n == 1u) {
        *_along = 0.0;
        *_off   = hypot(_p.x - _q.x, _p.y - _q.y);
        return _q;
    }
    for(u32 _i = 0; _i < _segs; _i++) {
        const fude_zoom_v2 _a = _c[_i], _b = _c[(_i + 1u) % _n];
        const f64 _dx = _b.x - _a.x, _dy = _b.y - _a.y, _l = hypot(_dx, _dy);
        const f64 _t  = _l > 0.0 ? fmin(fmax(((_p.x - _a.x) * _dx + (_p.y - _a.y) * _dy) / (_l * _l), 0.0), 1.0) : 0.0;
        const f64 _al = _run + _t * _l;
        _run += _l;
        if(_window > 0.0 && _total > 0.0) {
            f64 _d;
            if(_open) {
                _d = fabs(_al - _near);
            } else {
                _d = fmod(fabs(_al - fmod(_near, _total) + _total), _total);
                _d = fmin(_d, _total - _d);
            }
            if(_d > _window) {
                continue;
            }
        }
        const fude_zoom_v2 _r = { _a.x + _dx * _t, _a.y + _dy * _t };
        const f64 _d = hypot(_p.x - _r.x, _p.y - _r.y);
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
RDE_INTERNAL fude_zoom_v2 fude_zoom_instrument_curve_at(const fude_zoom_v2* _c, u32 _n, f64 _along, fude_zoom_v2* _dir, b8 _open) {
    const f64 _total = fude_zoom_instrument_curve_length(_c, _n, _open);
    const u32 _segs  = _open ? (_n > 0u ? _n - 1u : 0u) : _n;
    f64 _left = _open ? fmin(fmax(_along, 0.0), _total) : (_total > 0.0 ? fmod(fmod(_along, _total) + _total, _total) : 0.0);
    for(u32 _i = 0; _i < _segs; _i++) {
        const fude_zoom_v2 _a = _c[_i], _b = _c[(_i + 1u) % _n];
        const f64 _l = hypot(_b.x - _a.x, _b.y - _a.y);
        if(_left <= _l || _i + 1u == _segs) {
            const f64 _t = _l > 0.0 ? fmin(_left / _l, 1.0) : 0.0;
            *_dir = _l > 0.0 ? (fude_zoom_v2){ (_b.x - _a.x) / _l, (_b.y - _a.y) / _l } : (fude_zoom_v2){ 1.0, 0.0 };
            return (fude_zoom_v2){ _a.x + (_b.x - _a.x) * _t, _a.y + (_b.y - _a.y) * _t };
        }
        _left -= _l;
    }
    *_dir = (fude_zoom_v2){ 1.0, 0.0 };
    return _c[0];
}

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
        const fude_zoom_v2 _l[3] = { { -14.0, -10.0 }, { _cr + 14.0, -10.0 }, { _cr * 0.5, fmax(_cr * 0.55, 48.0) + 18.0 } };
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
                const f64 _t = _e[_i].from + (_e[_i].span > 0.0 ? _e[_i].span : FUDE_ZOOM_INSTRUMENT_PI) * (f64)_s / (f64)FUDE_ZOOM_ARC_SIDES;
                _out[_k++] = (fude_zoom_v2){ _e[_i].centre.x + cos(_t) * _e[_i].radius, _e[_i].centre.y + sin(_t) * _e[_i].radius };
            }
        } else if(_k < _max) {
            _out[_k++] = _e[_i].a;
        }
    }
    return _k;
}

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

fude_zoom_v2 fude_zoom_instrument_local(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2 _screen) {
    return fude_zoom_instrument_unplace(&_ins->tools[_slot], _screen);
}

fude_zoom_v2 fude_zoom_instrument_screen(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2 _local) {
    return fude_zoom_instrument_place(&_ins->tools[_slot], _local);
}

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
RDE_INTERNAL b8 fude_zoom_instrument_grow_local(const fude_zoom_instrument* _t, fude_zoom_v2* _tab, fude_zoom_v2* _fixed) {
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

b8 fude_zoom_instrument_grow_at(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2* _at) {
    fude_zoom_v2 _tab, _fixed;
    if(!fude_zoom_instrument_grow_local(&_ins->tools[_slot], &_tab, &_fixed)) {
        return false;
    }
    *_at = fude_zoom_instrument_place(&_ins->tools[_slot], _tab);
    return true;
}

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
RDE_INTERNAL i32 fude_zoom_instruments_grow_under(const fude_zoom_instruments* _ins, fude_zoom_v2 _p) {
    for(u32 _i = FUDE_ZOOM_INSTRUMENT_MOST; _i-- > 0;) {
        const u32 _k = _ins->order[_i];
        fude_zoom_v2 _at;
        if(_ins->tools[_k].shown && fude_zoom_instrument_grow_at(_ins, _k, &_at) && hypot(_p.x - _at.x, _p.y - _at.y) <= FUDE_ZOOM_INSTRUMENT_GROW * fude_zoom_instrument_icons(&_ins->tools[_k]) + 6.0) {
            return (i32)_k;
        }
    }
    return -1;
}

// A hand takes slot _k by its size tab.
RDE_INTERNAL void fude_zoom_instruments_take_grow(fude_zoom_instruments* _ins, u32 _k, u64 _id, fude_zoom_v2 _at) {
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
RDE_INTERNAL i32 fude_zoom_instruments_close_under(const fude_zoom_instruments* _ins, fude_zoom_v2 _p) {
    for(u32 _i = FUDE_ZOOM_INSTRUMENT_MOST; _i-- > 0;) {
        const u32 _k = _ins->order[_i];
        if(!_ins->tools[_k].shown) {
            continue;
        }
        const fude_zoom_v2 _x = fude_zoom_instrument_close_at(_ins, _k);
        if(hypot(_p.x - _x.x, _p.y - _x.y) <= FUDE_ZOOM_INSTRUMENT_CLOSE * fude_zoom_instrument_icons(&_ins->tools[_k]) + 5.0) {
            return (i32)_k;
        }
    }
    return -1;
}

RDE_INTERNAL f64 fude_zoom_instrument_edge_off(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2 _at);

RDE_INTERNAL b8 fude_zoom_instrument_inside(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2 _p) {
    if(_ins->tools[_slot].kind == FUDE_ZOOM_INSTRUMENT_STENCIL) {
        // Its plate, but not near its lines (the pen there draws along one).
        const fude_zoom_instrument* _t = &_ins->tools[_slot];
        const f64 _k = fude_zoom_instrument_size(_t);
        const fude_zoom_v2 _l = fude_zoom_instrument_unplace(_t, _p);
        if(fabs(_l.x) > _ins->stencils[_slot].hw * _k || fabs(_l.y) > _ins->stencils[_slot].hh * _k) {
            return false;
        }
        return fude_zoom_instrument_edge_off(_ins, _slot, _p) > FUDE_ZOOM_INSTRUMENT_SNAP * 0.9;
    }
    if(_ins->tools[_slot].kind == FUDE_ZOOM_INSTRUMENT_ELLIPSE) {
        const fude_zoom_instrument* _t = &_ins->tools[_slot];
        f64 _rx, _ry, _hx, _hy;
        fude_zoom_instrument_ellipse(_t, &_rx, &_ry, &_hx, &_hy);
        const fude_zoom_v2 _l = fude_zoom_instrument_unplace(_t, _p);
        return fabs(_l.x) <= _hx && fabs(_l.y) <= _hy && (_l.x * _l.x) / (_rx * _rx) + (_l.y * _l.y) / (_ry * _ry) > 1.0;
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
        return fabs(_l.x) <= _cr + fude_zoom_instrument_rim(_t) && fabs(_l.y) <= _cr + fude_zoom_instrument_rim(_t) && hypot(_l.x, _l.y) > _cr;
    }
    fude_zoom_v2 _o[FUDE_ZOOM_ARC_SIDES + 8u];
    const u32 _n = fude_zoom_instrument_outline(_ins, _slot, _o, FUDE_ZOOM_ARC_SIDES + 8u);
    b8 _in = false;
    for(u32 _i = 0, _j = _n - 1u; _i < _n; _j = _i++) {
        if(((_o[_i].y > _p.y) != (_o[_j].y > _p.y)) && (_p.x < (_o[_j].x - _o[_i].x) * (_p.y - _o[_i].y) / (_o[_j].y - _o[_i].y) + _o[_i].x)) {
            _in = !_in;
        }
    }
    return _in;
}

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

void fude_zoom_instruments_init(fude_zoom_instruments* _ins) {
    memset(_ins, 0, sizeof(*_ins));
    for(u32 _i = 0; _i < FUDE_ZOOM_INSTRUMENT_MOST; _i++) {
        _ins->order[_i] = (u8)_i;
    }
    _ins->held    = -1;
    _ins->docked  = -1;
    _ins->ruled   = -1;
    _ins->closing = -1;
}

b8 fude_zoom_instruments_any(const fude_zoom_instruments* _ins) {
    for(u32 _i = 0; _i < FUDE_ZOOM_INSTRUMENT_MOST; _i++) {
        if(_ins->tools[_i].shown) {
            return true;
        }
    }
    return false;
}

u32 fude_zoom_instruments_count(const fude_zoom_instruments* _ins, FUDE_ZOOM_INSTRUMENT_ _kind) {
    u32 _n = 0;
    for(u32 _i = 0; _i < FUDE_ZOOM_INSTRUMENT_MOST; _i++) {
        _n += _ins->tools[_i].shown && _ins->tools[_i].kind == (u8)_kind ? 1u : 0u;
    }
    return _n;
}

// Brought to the top of the pile.
RDE_INTERNAL void fude_zoom_instrument_raise(fude_zoom_instruments* _ins, u32 _slot) {
    u32 _w = 0;
    for(u32 _i = 0; _i < FUDE_ZOOM_INSTRUMENT_MOST; _i++) {
        if(_ins->order[_i] != _slot) {
            _ins->order[_w++] = _ins->order[_i];
        }
    }
    _ins->order[FUDE_ZOOM_INSTRUMENT_MOST - 1u] = (u8)_slot;
}

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
    memset(_t, 0, sizeof(*_t));
    _t->shown  = true;
    _t->kind   = (u8)_kind;
    _t->serial = ++_ins->serials;
    _t->size   = 1.0;
    _t->thick  = 1.0;
    _t->pin    = -1;
    // Where a hand would put it: the ruler across the middle, a little low;
    // the set squares and the protractor a little above and to either side;
    // another of the same a little further down and to the right.
    const f64 _w = fmin(_half.x, 400.0);
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

// --- hands on one ------------------------------------------------------------------------------

RDE_INTERNAL f64 fude_zoom_instrument_wrap(f64 _a) {
    while(_a > FUDE_ZOOM_INSTRUMENT_PI)   { _a -= 2.0 * FUDE_ZOOM_INSTRUMENT_PI; }
    while(_a <= -FUDE_ZOOM_INSTRUMENT_PI) { _a += 2.0 * FUDE_ZOOM_INSTRUMENT_PI; }
    return _a;
}

// A turn resting a moment at each 15°.
RDE_INTERNAL f64 fude_zoom_instrument_detent(f64 _a) {
    const f64 _step = FUDE_ZOOM_INSTRUMENT_PI / 12.0;
    const f64 _near = round(_a / _step) * _step;
    return fabs(_a - _near) < FUDE_ZOOM_INSTRUMENT_DETENT ? _near : _a;
}

// The hands as they are now become where the next moves are measured from.
RDE_INTERNAL void fude_zoom_instruments_regrab(fude_zoom_instruments* _ins) {
    if(_ins->held < 0) {
        return;
    }
    const fude_zoom_instrument* _t = &_ins->tools[_ins->held];
    _ins->grab_at    = _t->at;
    _ins->grab_angle = _t->angle;
    _ins->grab_r     = _t->radius;
    _ins->grab_size  = fude_zoom_instrument_size(_t);
    _ins->grab_hand[0] = _ins->hand_at[0];
    _ins->grab_hand[1] = _ins->hand_at[1];
}

RDE_INTERNAL fude_zoom_v2 fude_zoom_v2_norm(fude_zoom_v2 _v) {
    const f64 _l = hypot(_v.x, _v.y);
    return _l > 0.0 ? (fude_zoom_v2){ _v.x / _l, _v.y / _l } : (fude_zoom_v2){ 1.0, 0.0 };
}

RDE_INTERNAL f64 fude_zoom_v2_dot(fude_zoom_v2 _a, fude_zoom_v2 _b) { return _a.x * _b.x + _a.y * _b.y; }

// Sliding along the edge it lies on (_dir): how far on (or back) it goes to rest flush against the
// next one in its way — an edge of its lying along another's (facing it, nearly parallel), a corner
// of its on another's edge, or another's corner on an edge of its — within a dock's reach of it now
// (0: none near). The held one, as it is now.
RDE_INTERNAL f64 fude_zoom_instruments_against(const fude_zoom_instruments* _ins, fude_zoom_v2 _dir) {
    fude_zoom_instrument_edge _ea[FUDE_ZOOM_INSTRUMENT_EDGES];
    const u32 _na = fude_zoom_instrument_edges(_ins, (u32)_ins->held, _ea);
    f64 _best = 0.0, _best_d = FUDE_ZOOM_INSTRUMENT_DOCK;
    for(u32 _k = 0; _k < FUDE_ZOOM_INSTRUMENT_MOST; _k++) {
        if((i32)_k == _ins->held || (i32)_k == _ins->docked || !_ins->tools[_k].shown || _ins->tools[_k].kind == FUDE_ZOOM_INSTRUMENT_COMPASS) {
            continue;
        }
        fude_zoom_instrument_edge _eb[FUDE_ZOOM_INSTRUMENT_EDGES];
        const u32 _nb = fude_zoom_instrument_edges(_ins, _k, _eb);
        for(u32 _j = 0; _j < _nb; _j++) {
            if(_eb[_j].arc || _eb[_j].curve) {
                continue;
            }
            const fude_zoom_v2 _db  = fude_zoom_v2_norm((fude_zoom_v2){ _eb[_j].b.x - _eb[_j].a.x, _eb[_j].b.y - _eb[_j].a.y });
            const fude_zoom_v2 _out = { _db.y, -_db.x };   // (away from its body)
            const f64 _rate = fude_zoom_v2_dot(_dir, _out);   // how the gap to its line changes, a step along
            const f64 _blen = hypot(_eb[_j].b.x - _eb[_j].a.x, _eb[_j].b.y - _eb[_j].a.y);
            for(u32 _i = 0; _i < _na; _i++) {
                if(_ea[_i].arc || _ea[_i].curve) {
                    continue;
                }
                // A corner of ours (each edge's start) on its edge, from outside it.
                const f64 _gap = fude_zoom_v2_dot((fude_zoom_v2){ _ea[_i].a.x - _eb[_j].a.x, _ea[_i].a.y - _eb[_j].a.y }, _out);
                const f64 _at  = fude_zoom_v2_dot((fude_zoom_v2){ _ea[_i].a.x - _eb[_j].a.x, _ea[_i].a.y - _eb[_j].a.y }, _db);
                if(fabs(_rate) > 0.15 && _at >= -1.0 && _at <= _blen + 1.0 && _gap > -FUDE_ZOOM_INSTRUMENT_DOCK && _gap < FUDE_ZOOM_INSTRUMENT_DOCK) {
                    const f64 _d = -_gap / _rate;
                    if(fabs(_d) < _best_d) { _best_d = fabs(_d); _best = _d; }
                }
                // Its corner on an edge of ours, from outside ours.
                const fude_zoom_v2 _da   = fude_zoom_v2_norm((fude_zoom_v2){ _ea[_i].b.x - _ea[_i].a.x, _ea[_i].b.y - _ea[_i].a.y });
                const fude_zoom_v2 _oa   = { _da.y, -_da.x };
                const f64          _ra   = -fude_zoom_v2_dot(_dir, _oa);   // (we move: its corner's gap to our line changes the other way)
                const f64          _alen = hypot(_ea[_i].b.x - _ea[_i].a.x, _ea[_i].b.y - _ea[_i].a.y);
                const f64 _gap2 = fude_zoom_v2_dot((fude_zoom_v2){ _eb[_j].a.x - _ea[_i].a.x, _eb[_j].a.y - _ea[_i].a.y }, _oa);
                const f64 _at2  = fude_zoom_v2_dot((fude_zoom_v2){ _eb[_j].a.x - _ea[_i].a.x, _eb[_j].a.y - _ea[_i].a.y }, _da);
                if(fabs(_ra) > 0.15 && _at2 >= -1.0 && _at2 <= _alen + 1.0 && _gap2 > -FUDE_ZOOM_INSTRUMENT_DOCK && _gap2 < FUDE_ZOOM_INSTRUMENT_DOCK) {
                    const f64 _d = -_gap2 / _ra;
                    if(fabs(_d) < _best_d) { _best_d = fabs(_d); _best = _d; }
                }
            }
        }
    }
    return _best;
}

// The others it lies against now (besides the one it slides along): every edge of theirs an edge or a
// corner of its is on (within a hair), into its contacts.
RDE_INTERNAL void fude_zoom_instruments_find_contacts(fude_zoom_instruments* _ins) {
    _ins->contacts = 0;
    if(_ins->held < 0) {
        return;
    }
    fude_zoom_instrument_edge _ea[FUDE_ZOOM_INSTRUMENT_EDGES];
    const u32 _na = fude_zoom_instrument_edges(_ins, (u32)_ins->held, _ea);
    for(u32 _k = 0; _k < FUDE_ZOOM_INSTRUMENT_MOST && _ins->contacts < FUDE_ZOOM_INSTRUMENT_CONTACTS; _k++) {
        if((i32)_k == _ins->held || !_ins->tools[_k].shown || _ins->tools[_k].kind == FUDE_ZOOM_INSTRUMENT_COMPASS) {
            continue;
        }
        fude_zoom_instrument_edge _eb[FUDE_ZOOM_INSTRUMENT_EDGES];
        const u32 _nb = fude_zoom_instrument_edges(_ins, _k, _eb);
        for(u32 _j = 0; _j < _nb && _ins->contacts < FUDE_ZOOM_INSTRUMENT_CONTACTS; _j++) {
            if(_eb[_j].arc || _eb[_j].curve || ((i32)_k == _ins->docked && _j == _ins->dock_edge)) {
                continue;
            }
            const fude_zoom_v2 _db  = fude_zoom_v2_norm((fude_zoom_v2){ _eb[_j].b.x - _eb[_j].a.x, _eb[_j].b.y - _eb[_j].a.y });
            const fude_zoom_v2 _out = { _db.y, -_db.x };
            const f64 _blen = hypot(_eb[_j].b.x - _eb[_j].a.x, _eb[_j].b.y - _eb[_j].a.y);
            for(u32 _i = 0; _i < _na; _i++) {
                if(_ea[_i].arc || _ea[_i].curve) {
                    continue;
                }
                const f64 _g0 = fude_zoom_v2_dot((fude_zoom_v2){ _ea[_i].a.x - _eb[_j].a.x, _ea[_i].a.y - _eb[_j].a.y }, _out);
                const f64 _p0 = fude_zoom_v2_dot((fude_zoom_v2){ _ea[_i].a.x - _eb[_j].a.x, _ea[_i].a.y - _eb[_j].a.y }, _db);
                if(fabs(_g0) <= 0.5 && _p0 >= -0.5 && _p0 <= _blen + 0.5) {
                    _ins->contact_slot[_ins->contacts] = (i32)_k;
                    _ins->contact_edge[_ins->contacts] = _j;
                    _ins->contact_own[_ins->contacts]  = _i;
                    _ins->contacts++;
                    break;
                }
            }
        }
    }
}

// The held instrument, dragged to a place of its own: against another's edge
// it docks (turned to lie exactly along it, touching it from outside), then
// slides along that edge until pulled away from it — stopping flush against
// the next one in its way, so it rests against two or more at once.
RDE_INTERNAL void fude_zoom_instruments_slide(fude_zoom_instruments* _ins, fude_zoom_v2 _want) {
    fude_zoom_instrument* _t = &_ins->tools[_ins->held];
    _ins->contacts = 0;
    if(_ins->docked >= 0) {
        fude_zoom_instrument_edge _eb[FUDE_ZOOM_INSTRUMENT_EDGES];
        fude_zoom_instrument_edges(_ins, (u32)_ins->docked, _eb);
        const fude_zoom_instrument_edge* _e   = &_eb[_ins->dock_edge];
        const fude_zoom_v2               _dir = fude_zoom_v2_norm((fude_zoom_v2){ _e->b.x - _e->a.x, _e->b.y - _e->a.y });
        const fude_zoom_v2               _out = { _dir.y, -_dir.x };
        const fude_zoom_v2               _d   = { _want.x - _ins->dock_from.x, _want.y - _ins->dock_from.y };
        if(fabs(fude_zoom_v2_dot(_d, _out)) <= FUDE_ZOOM_INSTRUMENT_UNDOCK) {
            f64 _along = fude_zoom_v2_dot(_d, _dir);
            _t->at = (fude_zoom_v2){ _ins->dock_from.x + _dir.x * _along, _ins->dock_from.y + _dir.y * _along };
            // Up against the next one in its way (within a dock's reach): flush with it, resting on both.
            _along += fude_zoom_instruments_against(_ins, _dir);
            _t->at = (fude_zoom_v2){ _ins->dock_from.x + _dir.x * _along, _ins->dock_from.y + _dir.y * _along };
            fude_zoom_instruments_find_contacts(_ins);
            // Slid off the end of it: free again.
            fude_zoom_instrument_edge _ea[FUDE_ZOOM_INSTRUMENT_EDGES];
            fude_zoom_instrument_edges(_ins, (u32)_ins->held, _ea);
            const f64 _len = hypot(_e->b.x - _e->a.x, _e->b.y - _e->a.y);
            const f64 _p0  = fude_zoom_v2_dot((fude_zoom_v2){ _ea[_ins->own_edge].a.x - _e->a.x, _ea[_ins->own_edge].a.y - _e->a.y }, _dir);
            const f64 _p1  = fude_zoom_v2_dot((fude_zoom_v2){ _ea[_ins->own_edge].b.x - _e->a.x, _ea[_ins->own_edge].b.y - _e->a.y }, _dir);
            if(fmax(_p0, _p1) < 0.0 || fmin(_p0, _p1) > _len) {
                _ins->docked = -1;
            }
            return;
        }
        _ins->docked = -1;
    }
    _t->at = _want;
    // Near enough another's edge, and nearly parallel: docked to it.
    f64 _best = FUDE_ZOOM_INSTRUMENT_DOCK;
    i32 _to = -1;
    u32 _own = 0, _theirs = 0;
    f64 _turn = 0.0;
    fude_zoom_instrument_edge _ea[FUDE_ZOOM_INSTRUMENT_EDGES];
    const u32 _na = fude_zoom_instrument_edges(_ins, (u32)_ins->held, _ea);
    for(u32 _k = 0; _k < FUDE_ZOOM_INSTRUMENT_MOST; _k++) {
        if((i32)_k == _ins->held || !_ins->tools[_k].shown) {
            continue;
        }
        fude_zoom_instrument_edge _eb[FUDE_ZOOM_INSTRUMENT_EDGES];
        const u32 _nb = fude_zoom_instrument_edges(_ins, _k, _eb);
        for(u32 _i = 0; _i < _na; _i++) {
            if(_ea[_i].arc) {
                continue;
            }
            const fude_zoom_v2 _da = fude_zoom_v2_norm((fude_zoom_v2){ _ea[_i].b.x - _ea[_i].a.x, _ea[_i].b.y - _ea[_i].a.y });
            for(u32 _j = 0; _j < _nb; _j++) {
                if(_eb[_j].arc) {
                    continue;
                }
                const fude_zoom_v2 _db = fude_zoom_v2_norm((fude_zoom_v2){ _eb[_j].b.x - _eb[_j].a.x, _eb[_j].b.y - _eb[_j].a.y });
                // Facing each other: going opposite ways along the same line.
                const f64 _off = fude_zoom_instrument_wrap(atan2(-_db.y, -_db.x) - atan2(_da.y, _da.x));
                if(fabs(_off) > FUDE_ZOOM_INSTRUMENT_DOCK_TURN) {
                    continue;
                }
                const fude_zoom_v2 _out = { _db.y, -_db.x };
                const fude_zoom_v2 _mid = { (_ea[_i].a.x + _ea[_i].b.x) * 0.5, (_ea[_i].a.y + _ea[_i].b.y) * 0.5 };
                const f64 _gap = fude_zoom_v2_dot((fude_zoom_v2){ _mid.x - _eb[_j].a.x, _mid.y - _eb[_j].a.y }, _out);
                // Overlapping along it.
                const f64 _len = hypot(_eb[_j].b.x - _eb[_j].a.x, _eb[_j].b.y - _eb[_j].a.y);
                const f64 _p0  = fude_zoom_v2_dot((fude_zoom_v2){ _ea[_i].a.x - _eb[_j].a.x, _ea[_i].a.y - _eb[_j].a.y }, _db);
                const f64 _p1  = fude_zoom_v2_dot((fude_zoom_v2){ _ea[_i].b.x - _eb[_j].a.x, _ea[_i].b.y - _eb[_j].a.y }, _db);
                if(fmax(_p0, _p1) < 8.0 || fmin(_p0, _p1) > _len - 8.0) {
                    continue;
                }
                if(fabs(_gap) < _best) {
                    _best   = fabs(_gap);
                    _to     = (i32)_k;
                    _own    = _i;
                    _theirs = _j;
                    _turn   = _off;
                }
            }
        }
    }
    if(_to < 0) {
        return;
    }
    // Turned to lie exactly along it (round the middle of its own edge), then
    // moved onto its line.
    const fude_zoom_v2 _pivot = { (_ea[_own].a.x + _ea[_own].b.x) * 0.5, (_ea[_own].a.y + _ea[_own].b.y) * 0.5 };
    const f64 _c = cos(_turn), _s = sin(_turn);
    const fude_zoom_v2 _rel = { _t->at.x - _pivot.x, _t->at.y - _pivot.y };
    _t->at    = (fude_zoom_v2){ _pivot.x + _rel.x * _c - _rel.y * _s, _pivot.y + _rel.x * _s + _rel.y * _c };
    _t->angle = fude_zoom_instrument_wrap(_t->angle + _turn);
    fude_zoom_instrument_edge _eb[FUDE_ZOOM_INSTRUMENT_EDGES];
    fude_zoom_instrument_edges(_ins, (u32)_to, _eb);
    fude_zoom_instrument_edges(_ins, (u32)_ins->held, _ea);
    const fude_zoom_v2 _db  = fude_zoom_v2_norm((fude_zoom_v2){ _eb[_theirs].b.x - _eb[_theirs].a.x, _eb[_theirs].b.y - _eb[_theirs].a.y });
    const fude_zoom_v2 _out = { _db.y, -_db.x };
    const fude_zoom_v2 _mid = { (_ea[_own].a.x + _ea[_own].b.x) * 0.5, (_ea[_own].a.y + _ea[_own].b.y) * 0.5 };
    const f64 _gap = fude_zoom_v2_dot((fude_zoom_v2){ _mid.x - _eb[_theirs].a.x, _mid.y - _eb[_theirs].a.y }, _out);
    _t->at = (fude_zoom_v2){ _t->at.x - _out.x * _gap, _t->at.y - _out.y * _gap };
    _ins->docked    = _to;
    _ins->dock_edge = _theirs;
    _ins->own_edge  = _own;
    // Up against the next one in its way along it too, at once (resting on both).
    const f64 _more = fude_zoom_instruments_against(_ins, _db);
    _t->at = (fude_zoom_v2){ _t->at.x + _db.x * _more, _t->at.y + _db.y * _more };
    fude_zoom_instruments_find_contacts(_ins);
    _ins->dock_from = _t->at;
    // What the hands do next is measured from here (the turn just made is the instrument's, not theirs).
    _ins->grab_at    = (fude_zoom_v2){ _ins->grab_at.x + (_t->at.x - _want.x), _ins->grab_at.y + (_t->at.y - _want.y) };
    _ins->grab_angle = _t->angle;
}

RDE_INTERNAL void fude_zoom_instruments_follow(fude_zoom_instruments* _ins) {
    fude_zoom_instrument* _t = &_ins->tools[_ins->held];
    if(_ins->growing) {
        // Its size tab: as long (or as big) as reaches the hand, along its own way; the end that does not grow kept where it is.
        fude_zoom_v2 _tab, _fixed;
        if(!fude_zoom_instrument_grow_local(_t, &_tab, &_fixed)) {
            return;
        }
        const fude_zoom_v2 _fs  = fude_zoom_instrument_place(_t, _fixed);
        const fude_zoom_v2 _dir = { cos(_t->angle), sin(_t->angle) };
        const f64 _reach = fude_zoom_v2_dot((fude_zoom_v2){ _ins->hand_at[0].x - _fs.x, _ins->hand_at[0].y - _fs.y }, _dir) - FUDE_ZOOM_INSTRUMENT_GROW_OUT * fude_zoom_instrument_icons(_t);
        const f64 _whole = fude_zoom_v2_dot((fude_zoom_v2){ _tab.x - _fixed.x, _tab.y - _fixed.y }, (fude_zoom_v2){ 1.0, 0.0 }) - FUDE_ZOOM_INSTRUMENT_GROW_OUT * fude_zoom_instrument_icons(_t);
        const f64 _k     = fude_zoom_instrument_size(_t);
        const f64 _min   = _t->kind == FUDE_ZOOM_INSTRUMENT_RULER ? 0.3 : 0.5;
        _t->size = fmax(_whole > 1.0 ? _k * _reach / _whole : _k, _min);   // (no top: zoomed in, it is long on the screen already)
        // Its middle moved so the end that stays, stays.
        fude_zoom_v2 _tab2, _fixed2;
        fude_zoom_instrument_grow_local(_t, &_tab2, &_fixed2);
        const fude_zoom_v2 _now = fude_zoom_instrument_place(_t, _fixed2);
        _t->at = (fude_zoom_v2){ _t->at.x + _fs.x - _now.x, _t->at.y + _fs.y - _now.y };
        return;
    }
    if(_ins->turning && _t->kind == FUDE_ZOOM_INSTRUMENT_COMPASS) {
        // The compass's hinge: the pencil goes where the hand takes it (its radius and its way), the needle stays.
        const fude_zoom_v2 _pencil = { _t->at.x + cos(_ins->grab_angle) * _ins->grab_r + _ins->hand_at[0].x - _ins->grab_hand[0].x,
                                       _t->at.y + sin(_ins->grab_angle) * _ins->grab_r + _ins->hand_at[0].y - _ins->grab_hand[0].y };
        _t->radius      = fmax(hypot(_pencil.x - _t->at.x, _pencil.y - _t->at.y), 4.0);
        _t->angle       = atan2(_pencil.y - _t->at.y, _pencil.x - _t->at.x);
        _ins->turned_at = rde_engine_get_time_now();
        return;
    }
    if(_ins->turning) {
        // The knob: turned round its middle as the pen goes round it.
        const f64 _from = atan2(_ins->grab_hand[0].y - _t->at.y, _ins->grab_hand[0].x - _t->at.x);
        const f64 _now  = atan2(_ins->hand_at[0].y - _t->at.y, _ins->hand_at[0].x - _t->at.x);
        _t->angle      = fude_zoom_instrument_detent(fude_zoom_instrument_wrap(_ins->grab_angle + (_now - _from)));
        _ins->docked   = -1;
        _ins->turned_at = rde_engine_get_time_now();
        return;
    }
    if(_ins->hands == 1u) {
        const fude_zoom_v2 _want = { _ins->grab_at.x + _ins->hand_at[0].x - _ins->grab_hand[0].x, _ins->grab_at.y + _ins->hand_at[0].y - _ins->grab_hand[0].y };
        fude_zoom_instruments_slide(_ins, _want);
        return;
    }
    // Two hands: turned and moved as the line between them is (never scaled).
    const fude_zoom_v2 _g0 = _ins->grab_hand[0], _g1 = _ins->grab_hand[1];
    const fude_zoom_v2 _h0 = _ins->hand_at[0],   _h1 = _ins->hand_at[1];
    const f64 _turn  = fude_zoom_instrument_wrap(atan2(_h1.y - _h0.y, _h1.x - _h0.x) - atan2(_g1.y - _g0.y, _g1.x - _g0.x));
    const f64 _angle = fude_zoom_instrument_detent(fude_zoom_instrument_wrap(_ins->grab_angle + _turn));
    const f64 _used  = _angle - _ins->grab_angle;
    const fude_zoom_v2 _gm = { (_g0.x + _g1.x) * 0.5, (_g0.y + _g1.y) * 0.5 };
    const fude_zoom_v2 _hm = { (_h0.x + _h1.x) * 0.5, (_h0.y + _h1.y) * 0.5 };
    const f64 _c = cos(_used), _s = sin(_used);
    const fude_zoom_v2 _rel = { _ins->grab_at.x - _gm.x, _ins->grab_at.y - _gm.y };
    _t->at    = (fude_zoom_v2){ _hm.x + _rel.x * _c - _rel.y * _s, _hm.y + _rel.x * _s + _rel.y * _c };
    _t->angle = _angle;
    _ins->docked    = -1;
    _ins->turned_at = rde_engine_get_time_now();
}

RDE_INTERNAL void fude_zoom_instruments_hold(fude_zoom_instruments* _ins, u64 _id, fude_zoom_v2 _at, u32 _k);
RDE_INTERNAL fude_zoom_v2 fude_zoom_instrument_on_edge(const fude_zoom_instrument_edge* _e, fude_zoom_v2 _p, f64* _along, f64* _off);

b8 fude_zoom_instruments_finger_down(fude_zoom_instruments* _ins, u64 _id, fude_zoom_v2 _at) {
    if(_ins->closing_hand != 0u) {
        return false;   // (one hand on a × at a time)
    }
    if(_ins->held < 0) {
        const i32 _x = fude_zoom_instruments_close_under(_ins, _at);
        if(_x >= 0) {
            _ins->closing      = _x;
            _ins->closing_hand = _id;
            _ins->closing_at   = _at;
            return true;
        }
        const i32 _g = fude_zoom_instruments_grow_under(_ins, _at);
        if(_g >= 0) {
            fude_zoom_instruments_take_grow(_ins, (u32)_g, _id, _at);
            return true;
        }
    }
    if(_ins->held >= 0) {
        // A second hand near the one held turns it.
        if(_ins->hands == 1u && !_ins->turning && _id != _ins->hand_id[0]) {
            const fude_zoom_instrument* _t = &_ins->tools[_ins->held];
            if(fude_zoom_instrument_inside(_ins, (u32)_ins->held, _at) || hypot(_at.x - _t->at.x, _at.y - _t->at.y) < 360.0) {
                _ins->hand_id[1] = _id;
                _ins->hand_at[1] = _at;
                _ins->hands      = 2u;
                fude_zoom_instruments_regrab(_ins);
                return true;
            }
        }
        return false;
    }
    const i32 _k = fude_zoom_instruments_at(_ins, _at);
    if(_k < 0) {
        return false;
    }
    fude_zoom_instruments_hold(_ins, _id, _at, (u32)_k);
    return true;
}

RDE_INTERNAL void fude_zoom_instruments_hold(fude_zoom_instruments* _ins, u64 _id, fude_zoom_v2 _at, u32 _k) {
    _ins->held       = (i32)_k;
    _ins->hands      = 1u;
    _ins->hand_id[0] = _id;
    _ins->hand_at[0] = _at;
    _ins->turning    = false;
    fude_zoom_instrument_raise(_ins, _k);
    fude_zoom_instruments_regrab(_ins);
}

// Half an instrument's narrowest width on the screen (its outline's box, its own way round): a
// ruler's, half its band.
RDE_INTERNAL f64 fude_zoom_instrument_thin(const fude_zoom_instruments* _ins, u32 _slot) {
    fude_zoom_v2 _o[FUDE_ZOOM_ARC_SIDES + 8u];
    const u32 _n = fude_zoom_instrument_outline(_ins, _slot, _o, FUDE_ZOOM_ARC_SIDES + 8u);
    if(_n < 3u) {
        return 1e9;
    }
    fude_zoom_box _b = fude_zoom_box_empty();
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_v2 _l = fude_zoom_instrument_unplace(&_ins->tools[_slot], _o[_i]);
        _b = fude_zoom_box_union(_b, (fude_zoom_box){ _l.x, _l.y, _l.x, _l.y });
    }
    return 0.5 * fmin(_b.max_x - _b.min_x, _b.max_y - _b.min_y);
}

// How near _at is to an instrument's nearest edge (screen points).
RDE_INTERNAL f64 fude_zoom_instrument_edge_off(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2 _at) {
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    const u32 _n = fude_zoom_instrument_edges(_ins, _slot, _e);
    f64 _best = 1e300;
    for(u32 _j = 0; _j < _n; _j++) {
        f64 _along, _off;
        if(_e[_j].curve) {
            fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
            f64 _round;
            const u32 _m = fude_zoom_instrument_curve_of(_ins, _slot, &_e[_j], _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
            if(_m == 0u) {
                continue;
            }
            fude_zoom_instrument_on_curve(_c, _m, _at, 0.0, 0.0, &_along, &_off, &_round, _e[_j].open);
        } else {
            fude_zoom_instrument_on_edge(&_e[_j], _at, &_along, &_off);
        }
        _best = fmin(_best, _off);
    }
    return _best;
}

// A hand on a × going off it: it is not put away (the hand stays the instruments', doing nothing).
RDE_INTERNAL b8 fude_zoom_instruments_closing_moved(fude_zoom_instruments* _ins, u64 _id, fude_zoom_v2 _at) {
    if(_ins->closing_hand == 0u || _ins->closing_hand != _id) {
        return false;
    }
    if(hypot(_at.x - _ins->closing_at.x, _at.y - _ins->closing_at.y) > FUDE_ZOOM_INSTRUMENT_CLOSE + 8.0) {
        _ins->closing = -1;
    }
    return true;
}

// That hand lifted: on the × still, put away.
RDE_INTERNAL b8 fude_zoom_instruments_closing_up(fude_zoom_instruments* _ins, u64 _id) {
    if(_ins->closing_hand == 0u || _ins->closing_hand != _id) {
        return false;
    }
    if(_ins->closing >= 0) {
        fude_zoom_instrument_remove(_ins, (u32)_ins->closing);
    }
    _ins->closing      = -1;
    _ins->closing_hand = 0u;
    return true;
}

b8 fude_zoom_instruments_finger_moved(fude_zoom_instruments* _ins, u64 _id, fude_zoom_v2 _at) {
    if(fude_zoom_instruments_closing_moved(_ins, _id, _at)) {
        return true;
    }
    if(_ins->held < 0) {
        return false;
    }
    for(u32 _i = 0; _i < _ins->hands; _i++) {
        if(_ins->hand_id[_i] == _id) {
            _ins->hand_at[_i] = _at;
            fude_zoom_instruments_follow(_ins);
            return true;
        }
    }
    return false;
}

b8 fude_zoom_instruments_finger_up(fude_zoom_instruments* _ins, u64 _id) {
    if(fude_zoom_instruments_closing_up(_ins, _id)) {
        return true;
    }
    if(_ins->held < 0) {
        return false;
    }
    for(u32 _i = 0; _i < _ins->hands; _i++) {
        if(_ins->hand_id[_i] == _id) {
            if(_ins->hands == 2u) {
                // The other hand goes on dragging it alone.
                _ins->hand_id[0] = _ins->hand_id[1 - _i];
                _ins->hand_at[0] = _ins->hand_at[1 - _i];
                _ins->hands      = 1u;
                fude_zoom_instruments_regrab(_ins);
            } else {
                // Let go: it rests where it is (touching the other still, it
                // docks again as soon as it is dragged).
                _ins->held    = -1;
                _ins->hands   = 0u;
                _ins->turning = false;
                _ins->growing = false;
                _ins->docked  = -1;
            }
            return true;
        }
    }
    return false;
}

// --- the pen --------------------------------------------------------------------------------

// Where on edge _e the point nearest _p is: its distance along the edge (an
// angle on an arc), and how far _p is from it.
RDE_INTERNAL fude_zoom_v2 fude_zoom_instrument_on_edge(const fude_zoom_instrument_edge* _e, fude_zoom_v2 _p, f64* _along, f64* _off) {
    if(_e->arc && _e->span >= 2.0 * FUDE_ZOOM_INSTRUMENT_PI - 1e-9) {
        // All the way round (the compass): anywhere on it, from where its pencil is.
        f64 _t = atan2(_p.y - _e->centre.y, _p.x - _e->centre.x) - _e->from;
        _t = fmod(_t + 4.0 * FUDE_ZOOM_INSTRUMENT_PI, 2.0 * FUDE_ZOOM_INSTRUMENT_PI);
        const fude_zoom_v2 _q = { _e->centre.x + cos(_e->from + _t) * _e->radius, _e->centre.y + sin(_e->from + _t) * _e->radius };
        *_along = _t;
        *_off   = hypot(_p.x - _q.x, _p.y - _q.y);
        return _q;
    }
    if(_e->arc) {
        // Part of the way round (the protractor's half, a corner gauge's quarter): past either end, the nearer.
        const f64 _span = _e->span > 0.0 ? _e->span : FUDE_ZOOM_INSTRUMENT_PI;
        f64 _t = fude_zoom_instrument_wrap(atan2(_p.y - _e->centre.y, _p.x - _e->centre.x) - _e->from);
        if(_t < 0.0 || _t > _span) {
            _t = fabs(fude_zoom_instrument_wrap(_t)) <= fabs(fude_zoom_instrument_wrap(_t - _span)) ? 0.0 : _span;
        }
        const fude_zoom_v2 _q = { _e->centre.x + cos(_e->from + _t) * _e->radius, _e->centre.y + sin(_e->from + _t) * _e->radius };
        *_along = _t;
        *_off   = hypot(_p.x - _q.x, _p.y - _q.y);
        return _q;
    }
    const fude_zoom_v2 _d   = { _e->b.x - _e->a.x, _e->b.y - _e->a.y };
    const f64          _len = hypot(_d.x, _d.y);
    const fude_zoom_v2 _dir = fude_zoom_v2_norm(_d);
    const f64          _t   = fmin(fmax(fude_zoom_v2_dot((fude_zoom_v2){ _p.x - _e->a.x, _p.y - _e->a.y }, _dir), 0.0), _len);
    const fude_zoom_v2 _q   = { _e->a.x + _dir.x * _t, _e->a.y + _dir.y * _t };
    *_along = _t;
    *_off   = hypot(_p.x - _q.x, _p.y - _q.y);
    return _q;
}

// --- chained: one edge's crossings with the others ------------------------------------------

// Edge _j of _slot as a line of points on the screen (a straight one its ends; an arc or a
// curve many, closed ones closed). How many.
RDE_INTERNAL u32 fude_zoom_instrument_edge_line(const fude_zoom_instruments* _ins, u32 _slot, u32 _j, fude_zoom_v2* _out, u32 _max) {
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    const u32 _n = fude_zoom_instrument_edges(_ins, _slot, _e);
    if(_j >= _n || _max < 2u) {
        return 0;
    }
    const fude_zoom_instrument_edge* _ed = &_e[_j];
    if(_ed->curve) {
        const u32 _m = fude_zoom_instrument_curve_of(_ins, _slot, _ed, _out, _max - 1u);
        if(_m > 0 && !_ed->open) {
            _out[_m] = _out[0];   // (a loop closed: its last piece back to its first point)
            return _m + 1u;
        }
        return _m;
    }
    if(_ed->arc) {
        const u32 _sides = _max - 1u < 128u ? _max - 1u : 128u;
        for(u32 _i = 0; _i <= _sides; _i++) {
            const f64 _t = _ed->from + _ed->span * (f64)_i / (f64)_sides;
            _out[_i] = (fude_zoom_v2){ _ed->centre.x + cos(_t) * _ed->radius, _ed->centre.y + sin(_t) * _ed->radius };
        }
        return _sides + 1u;
    }
    _out[0] = _ed->a;
    _out[1] = _ed->b;
    return 2u;
}

// Where segments a-b and c-d cross (true), into _out: up to half a point past either's ends (instruments
// lying flush: one's edge through another's corner, a hair either side of it); lying along each other, none.
RDE_INTERNAL b8 fude_zoom_instrument_segments_cross(fude_zoom_v2 _a, fude_zoom_v2 _b, fude_zoom_v2 _c, fude_zoom_v2 _d, fude_zoom_v2* _out) {
    const fude_zoom_v2 _r = { _b.x - _a.x, _b.y - _a.y }, _q = { _d.x - _c.x, _d.y - _c.y };
    const f64 _den = _r.x * _q.y - _r.y * _q.x;
    const f64 _lr = hypot(_r.x, _r.y), _lq = hypot(_q.x, _q.y);
    if(fabs(_den) <= 1e-9 * _lr * _lq || !(_lr > 1e-12) || !(_lq > 1e-12)) {
        return false;
    }
    const fude_zoom_v2 _ca = { _c.x - _a.x, _c.y - _a.y };
    const f64 _t = (_ca.x * _q.y - _ca.y * _q.x) / _den, _u = (_ca.x * _r.y - _ca.y * _r.x) / _den;
    const f64 _st = 0.5 / _lr, _su = 0.5 / _lq;
    if(_t < -_st || _t > 1.0 + _st || _u < -_su || _u > 1.0 + _su) {
        return false;
    }
    *_out = (fude_zoom_v2){ _a.x + _r.x * _t, _a.y + _r.y * _t };
    return true;
}

// The point of edge _j of _slot nearest _p, and how far along it (its own measure: a
// straight one's distance from a, an arc's angle, a curve's length round it).
RDE_INTERNAL fude_zoom_v2 fude_zoom_instrument_edge_nearest(const fude_zoom_instruments* _ins, u32 _slot, u32 _j, fude_zoom_v2 _p, f64* _along, f64* _off) {
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    fude_zoom_instrument_edges(_ins, _slot, _e);
    if(_e[_j].curve) {
        fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
        f64 _round;
        const u32 _m = fude_zoom_instrument_curve_of(_ins, _slot, &_e[_j], _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
        if(_m == 0u) {
            *_along = 0.0;
            *_off   = 1e300;
            return _p;
        }
        return fude_zoom_instrument_on_curve(_c, _m, _p, 0.0, 0.0, _along, _off, &_round, _e[_j].open);
    }
    return fude_zoom_instrument_on_edge(&_e[_j], _p, _along, _off);
}

// How far round the edge once (2π, a curve's length; 0: it does not go round).
RDE_INTERNAL f64 fude_zoom_instrument_edge_round(const fude_zoom_instruments* _ins, u32 _slot, u32 _j) {
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    fude_zoom_instrument_edges(_ins, _slot, _e);
    if(_e[_j].curve) {
        if(_e[_j].open) {
            return 0.0;   // (a line with ends: never round and round)
        }
        fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
        const u32 _m = fude_zoom_instrument_curve_of(_ins, _slot, &_e[_j], _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
        return fude_zoom_instrument_curve_length(_c, _m, false);
    }
    return _e[_j].arc && _e[_j].span >= 2.0 * FUDE_ZOOM_INSTRUMENT_PI - 1e-9 ? 2.0 * FUDE_ZOOM_INSTRUMENT_PI : 0.0;
}

// The edge drawn along now: its crossings with every other edge shown (another
// instrument's, this one's others), kept (the instruments do not move while it draws).
RDE_INTERNAL void fude_zoom_instruments_find_crosses(fude_zoom_instruments* _ins) {
    _ins->cross_n = 0;
    if(_ins->ruled < 0) {
        return;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_v2* _mine  = _heap->malloc(_heap->allocator, sizeof(fude_zoom_v2) * (FUDE_ZOOM_INSTRUMENT_CURVE_POINTS + 2u));
    fude_zoom_v2* _other = _heap->malloc(_heap->allocator, sizeof(fude_zoom_v2) * (FUDE_ZOOM_INSTRUMENT_CURVE_POINTS + 2u));
    const u32 _nm = fude_zoom_instrument_edge_line(_ins, (u32)_ins->ruled, _ins->ruled_edge, _mine, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS + 2u);
    for(u32 _k = 0; _k < FUDE_ZOOM_INSTRUMENT_MOST; _k++) {
        if(!_ins->tools[_k].shown) {
            continue;
        }
        fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
        const u32 _ne = fude_zoom_instrument_edges(_ins, _k, _e);
        for(u32 _j = 0; _j < _ne; _j++) {
            if((i32)_k == _ins->ruled && _j == _ins->ruled_edge) {
                continue;
            }
            const u32 _no = fude_zoom_instrument_edge_line(_ins, _k, _j, _other, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS + 2u);
            for(u32 _a = 0; _a + 1u < _nm; _a++) {
                for(u32 _b = 0; _b + 1u < _no; _b++) {
                    fude_zoom_v2 _x;
                    if(!fude_zoom_instrument_segments_cross(_mine[_a], _mine[_a + 1u], _other[_b], _other[_b + 1u], &_x)) {
                        continue;
                    }
                    // (the same edge's crossing found twice, at a shared end of its pieces: once — another
                    // edge through the same point is kept: one lying flush runs through this one's corner)
                    b8 _again = false;
                    for(u32 _i = 0; _i < _ins->cross_n && !_again; _i++) {
                        _again = _ins->cross_slot[_i] == _k && _ins->cross_edge[_i] == _j &&
                                 hypot(_ins->cross_at[_i].x - _x.x, _ins->cross_at[_i].y - _x.y) < 0.5;
                    }
                    if(_again || _ins->cross_n >= FUDE_ZOOM_INSTRUMENT_CROSSES) {
                        continue;
                    }
                    f64 _along, _off;
                    _x = fude_zoom_instrument_edge_nearest(_ins, (u32)_ins->ruled, _ins->ruled_edge, _x, &_along, &_off);
                    _ins->cross_at[_ins->cross_n]    = _x;
                    _ins->cross_along[_ins->cross_n] = _along;
                    _ins->cross_slot[_ins->cross_n]  = _k;
                    _ins->cross_edge[_ins->cross_n]  = _j;
                    _ins->cross_n++;
                }
            }
        }
    }
    _heap->free(_heap->allocator, _mine);
    _heap->free(_heap->allocator, _other);
}

b8 fude_zoom_instruments_ruled_turn(fude_zoom_instruments* _ins, fude_zoom_v2* _corner, fude_zoom_v2* _out_before) {
    if(!_ins->ruled_turned) {
        return false;
    }
    _ins->ruled_turned = false;
    *_corner     = _ins->ruled_corner;
    *_out_before = _ins->ruled_out_before;
    return true;
}

RDE_INTERNAL fude_zoom_v2 fude_zoom_instruments_follow_edge(fude_zoom_instruments* _ins, fude_zoom_v2 _at);
RDE_INTERNAL fude_zoom_v2 fude_zoom_instruments_chain(fude_zoom_instruments* _ins, fude_zoom_v2 _at, f64 _before, fude_zoom_v2 _q);

FUDE_ZOOM_INSTRUMENT_PEN_ fude_zoom_instruments_pen_down(fude_zoom_instruments* _ins, fude_zoom_v2 _at, fude_zoom_v2* _snapped) {
    _ins->ruled = -1;
    // A × under the pen first: lifted there, that one is put away.
    if(_ins->held < 0 && _ins->closing_hand == 0u) {
        const i32 _x = fude_zoom_instruments_close_under(_ins, _at);
        if(_x >= 0) {
            _ins->closing      = _x;
            _ins->closing_hand = FUDE_ZOOM_INSTRUMENT_PEN_HAND;
            _ins->closing_at   = _at;
            return FUDE_ZOOM_INSTRUMENT_PEN_HELD;
        }
        const i32 _g = fude_zoom_instruments_grow_under(_ins, _at);
        if(_g >= 0) {
            fude_zoom_instruments_take_grow(_ins, (u32)_g, FUDE_ZOOM_INSTRUMENT_PEN_HAND, _at);
            return FUDE_ZOOM_INSTRUMENT_PEN_HELD;
        }
    }
    // The knob of the one on top under the pen, then its edges, then its body.
    for(u32 _i = FUDE_ZOOM_INSTRUMENT_MOST; _i-- > 0;) {
        const u32 _k = _ins->order[_i];
        if(!_ins->tools[_k].shown) {
            continue;
        }
        const fude_zoom_v2 _knob = fude_zoom_instrument_knob(_ins, _k);
        if(hypot(_at.x - _knob.x, _at.y - _knob.y) <= FUDE_ZOOM_INSTRUMENT_KNOB * fude_zoom_instrument_icons(&_ins->tools[_k]) + 6.0 && _ins->held < 0) {
            _ins->held       = (i32)_k;
            _ins->hands      = 1u;
            _ins->hand_id[0] = FUDE_ZOOM_INSTRUMENT_PEN_HAND;
            _ins->hand_at[0] = _at;
            _ins->turning    = true;
            fude_zoom_instrument_raise(_ins, _k);
            fude_zoom_instruments_regrab(_ins);
            return FUDE_ZOOM_INSTRUMENT_PEN_HELD;
        }
    }
    // On a body (the one on top): it is held — the pen draws along an edge from outside it, as a
    // pencil against a ruler does (from a hair inside the edge too). A thin one (a true-size ruler
    // zoomed out is a narrow band) is held from a little round it as well, so there is always
    // FUDE_ZOOM_INSTRUMENT_GRIP of it to take; drawing along it begins past that. Not the compass:
    // its circle runs through its own pencil.
    for(u32 _i = FUDE_ZOOM_INSTRUMENT_MOST; _i-- > 0 && _ins->held < 0;) {
        const u32 _k = _ins->order[_i];
        if(!_ins->tools[_k].shown || _ins->tools[_k].kind == FUDE_ZOOM_INSTRUMENT_COMPASS) {
            continue;
        }
        const b8  _in   = fude_zoom_instrument_inside(_ins, _k, _at);
        const f64 _thin = fude_zoom_instrument_thin(_ins, _k);
        const f64 _off  = fude_zoom_instrument_edge_off(_ins, _k, _at);
        // (On it, but a hair from another's edge — a ruler lying across another, the pen at the one under it: that edge.)
        f64 _other = 1e300;
        for(u32 _o = 0; _in && _o < FUDE_ZOOM_INSTRUMENT_MOST; _o++) {
            if(_o != _k && _ins->tools[_o].shown) {
                _other = fmin(_other, fude_zoom_instrument_edge_off(_ins, _o, _at));
            }
        }
        if((_in && _off > fmin(3.0, _thin * 0.25) && _other > 6.0) || (!_in && _off <= fmax(0.0, FUDE_ZOOM_INSTRUMENT_GRIP - _thin))) {
            fude_zoom_instruments_hold(_ins, FUDE_ZOOM_INSTRUMENT_PEN_HAND, _at, _k);
            return FUDE_ZOOM_INSTRUMENT_PEN_HELD;
        }
        if(_in) {
            break;   // (on the one on top, by its edge: it draws, not one under it)
        }
    }
    f64 _best = FUDE_ZOOM_INSTRUMENT_SNAP;
    for(u32 _i = FUDE_ZOOM_INSTRUMENT_MOST; _i-- > 0;) {
        const u32 _k = _ins->order[_i];
        if(!_ins->tools[_k].shown) {
            continue;
        }
        fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
        const u32 _n = fude_zoom_instrument_edges(_ins, _k, _e);
        for(u32 _j = 0; _j < _n; _j++) {
            f64 _along, _off;
            fude_zoom_v2 _q;
            if(_e[_j].curve) {
                fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
                f64 _round;
                const u32 _m = fude_zoom_instrument_curve_of(_ins, _k, &_e[_j], _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
                if(_m == 0u) {
                    continue;
                }
                _q = fude_zoom_instrument_on_curve(_c, _m, _at, 0.0, 0.0, &_along, &_off, &_round, _e[_j].open);
            } else {
                _q = fude_zoom_instrument_on_edge(&_e[_j], _at, &_along, &_off);
            }
            if(_off < _best) {
                _best             = _off;
                _ins->ruled       = (i32)_k;
                _ins->ruled_edge  = _j;
                _ins->ruled_from  = _along;
                _ins->ruled_now   = _along;
                *_snapped         = _q;
            }
        }
        if(_ins->ruled >= 0) {
            _ins->ruled_start  = *_snapped;
            _ins->ruled_cornered = false;
            _ins->ruled_held   = -1;
            _ins->ruled_turned = false;
            fude_zoom_instruments_find_crosses(_ins);
            return FUDE_ZOOM_INSTRUMENT_PEN_RULED;   // the one on top wins
        }
    }
    if(_ins->held < 0 && fude_zoom_instruments_finger_down(_ins, FUDE_ZOOM_INSTRUMENT_PEN_HAND, _at)) {
        return FUDE_ZOOM_INSTRUMENT_PEN_HELD;
    }
    return FUDE_ZOOM_INSTRUMENT_PEN_NONE;
}

fude_zoom_v2 fude_zoom_instruments_pen_moved(fude_zoom_instruments* _ins, fude_zoom_v2 _at) {
    if(fude_zoom_instruments_closing_moved(_ins, FUDE_ZOOM_INSTRUMENT_PEN_HAND, _at)) {
        return _at;
    }
    if(_ins->ruled >= 0) {
        const f64          _before = _ins->ruled_now;
        const fude_zoom_v2 _q      = fude_zoom_instruments_follow_edge(_ins, _at);
        return fude_zoom_instruments_chain(_ins, _at, _before, _q);
    }
    if(_ins->held >= 0 && _ins->hand_id[0] == FUDE_ZOOM_INSTRUMENT_PEN_HAND) {
        _ins->hand_at[0] = _at;
        fude_zoom_instruments_follow(_ins);
    }
    return _at;
}

// The pen along the edge it draws along: the point of it nearest the pen, going on from where it
// was (round and round counted on), ruled_now moved there.
RDE_INTERNAL fude_zoom_v2 fude_zoom_instruments_follow_edge(fude_zoom_instruments* _ins, fude_zoom_v2 _at) {
    {
        fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
        fude_zoom_instrument_edges(_ins, (u32)_ins->ruled, _e);
        f64 _along, _off;
        if(_e[_ins->ruled_edge].curve) {
            // Along the curve from where the pen is on it (a quarter of the way round at most: never across it);
            // round and round, counted on.
            fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
            const b8  _open  = _e[_ins->ruled_edge].open;
            const u32 _m     = fude_zoom_instrument_curve_of(_ins, (u32)_ins->ruled, &_e[_ins->ruled_edge], _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
            if(_m == 0u) {
                return _at;
            }
            f64       _round = fude_zoom_instrument_curve_length(_c, _m, _open);
            const fude_zoom_v2 _r = fude_zoom_instrument_on_curve(_c, _m, _at, _ins->ruled_now, _open ? fmax(_round * 0.25, 80.0) : _round * 0.25, &_along, &_off, &_round, _open);
            while(!_open && _round > 0.0 && _along - _ins->ruled_now > _round * 0.5) { _along -= _round; }
            while(!_open && _round > 0.0 && _ins->ruled_now - _along > _round * 0.5) { _along += _round; }
            _ins->ruled_now = _along;
            return _r;
        }
        const fude_zoom_instrument_edge* _ed = &_e[_ins->ruled_edge];
        if(_ed->arc && _ed->span < 2.0 * FUDE_ZOOM_INSTRUMENT_PI - 1e-9 && hypot(_at.x - _ed->centre.x, _at.y - _ed->centre.y) < _ed->radius * 0.6) {
            // Part of the way round (a corner gauge's, the protractor's), the pen well in over the plate: where it
            // points from there swings, and would take the line back to an end at once — it stays where it was.
            const f64 _t = fmin(fmax(_ins->ruled_now, 0.0), _ed->span > 0.0 ? _ed->span : FUDE_ZOOM_INSTRUMENT_PI);
            return (fude_zoom_v2){ _ed->centre.x + cos(_ed->from + _t) * _ed->radius, _ed->centre.y + sin(_ed->from + _t) * _ed->radius };
        }
        const fude_zoom_v2 _q = fude_zoom_instrument_on_edge(_ed, _at, &_along, &_off);
        if(_e[_ins->ruled_edge].span >= 2.0 * FUDE_ZOOM_INSTRUMENT_PI - 1e-9) {
            // Round and round: the turn counted on from where it was (past a whole turn, more than 360°).
            while(_along - _ins->ruled_now > FUDE_ZOOM_INSTRUMENT_PI)  { _along -= 2.0 * FUDE_ZOOM_INSTRUMENT_PI; }
            while(_ins->ruled_now - _along > FUDE_ZOOM_INSTRUMENT_PI)  { _along += 2.0 * FUDE_ZOOM_INSTRUMENT_PI; }
        }
        _ins->ruled_now = _along;
        return _q;
    }
}

// Chained (instrument.h): the pen gone past a crossing of this edge with another — nearer
// that one, the line turns onto it at the crossing; nearer this one, it stays at the crossing
// when going on would be into the other instrument. _before: how far along it was.
RDE_INTERNAL u32 fude_zoom_instruments_chain_depth = 0;

RDE_INTERNAL fude_zoom_v2 fude_zoom_instruments_chain(fude_zoom_instruments* _ins, fude_zoom_v2 _at, f64 _before, fude_zoom_v2 _q) {
    if(_ins->cross_n == 0) {
        return _q;
    }
    const f64 _round = fude_zoom_instrument_edge_round(_ins, (u32)_ins->ruled, _ins->ruled_edge);
    const f64 _now   = _ins->ruled_now;
    // Turned onto this edge at a corner: not on from it into the one it left (along this edge
    // that way, beside it, is inside that one's body) — held at the corner till the pen goes
    // the other way.
    for(u32 _i = 0; _ins->ruled_cornered && _i < _ins->cross_n; _i++) {
        if(hypot(_ins->cross_at[_i].x - _ins->ruled_start.x, _ins->cross_at[_i].y - _ins->ruled_start.y) >= 3.0 ||
           _ins->tools[_ins->cross_slot[_i]].kind == FUDE_ZOOM_INSTRUMENT_COMPASS || _ins->cross_slot[_i] == (u32)_ins->ruled) {
            continue;   // (its own next edge — a corner gauge's side into its round — is never into itself: its round's
                        // chord runs over its own plate)
        }
        const fude_zoom_v2 _c   = _ins->cross_at[_i];
        const f64          _len = hypot(_q.x - _c.x, _q.y - _c.y);
        if(!(_len > 1e-9)) {
            continue;
        }
        const fude_zoom_v2 _way   = { (_q.x - _c.x) / _len, (_q.y - _c.y) / _len };
        const fude_zoom_v2 _out   = fude_zoom_instruments_ruled_outward(_ins);
        const fude_zoom_v2 _probe = { _c.x + _way.x * 3.0 + _out.x * 1.5, _c.y + _way.y * 3.0 + _out.y * 1.5 };
        if(fude_zoom_instrument_inside(_ins, _ins->cross_slot[_i], _probe)) {
            f64 _x = _ins->cross_along[_i];
            if(_round > 0.0) {
                while(_x - _now > _round * 0.5) { _x -= _round; }
                while(_now - _x > _round * 0.5) { _x += _round; }
            }
            _ins->ruled_now = _x;
            return _c;
        }
    }
    i32 _pick = -1;
    f64 _pick_d = 1e300, _pick_along = 0.0;
    for(u32 _i = 0; _i < _ins->cross_n; _i++) {
        if(hypot(_ins->cross_at[_i].x - _ins->ruled_start.x, _ins->cross_at[_i].y - _ins->ruled_start.y) < 3.0) {
            continue;   // (where the line began on this edge: not turned at)
        }
        f64 _x = _ins->cross_along[_i];
        if(_round > 0.0) {
            while(_x - _before > _round * 0.5) { _x -= _round; }
            while(_before - _x > _round * 0.5) { _x += _round; }
        }
        // Gone past it since the last move (or held at it, the pen still past it the way it came; or the line at it
        // still — at the end of this edge, the pen past it: weighed again each move, else it stayed there for good).
        const b8 _past = (_i == (u32)_ins->ruled_held && (_now - _x) * _ins->ruled_hold_way > 0.0) ||
                         (_now > _before ? (_x > _before + 1e-9 && _x <= _now) : (_x < _before - 1e-9 && _x >= _now)) ||
                         hypot(_ins->cross_at[_i].x - _q.x, _ins->cross_at[_i].y - _q.y) < 0.75;
        if(_past && fabs(_x - _before) < _pick_d) {
            _pick_d     = fabs(_x - _before);
            _pick       = (i32)_i;
            _pick_along = _x;
        }
    }
    if(_pick < 0) {
        _ins->ruled_held = -1;
        return _q;
    }
    // Several edges crossing it there at once (three set squares meeting at a point): of those, the one the
    // pen goes along — the nearest it — not merely the first met (found on the tablet: going round the
    // outside of three, the line turned in along one lying under the others).
    f64 _a2, _off2;
    fude_zoom_instrument_edge_nearest(_ins, _ins->cross_slot[_pick], _ins->cross_edge[_pick], _at, &_a2, &_off2);
    for(u32 _i = 0; _i < _ins->cross_n; _i++) {
        if((i32)_i == _pick || hypot(_ins->cross_at[_i].x - _ins->cross_at[_pick].x, _ins->cross_at[_i].y - _ins->cross_at[_pick].y) > 4.0 ||
           hypot(_ins->cross_at[_i].x - _ins->ruled_start.x, _ins->cross_at[_i].y - _ins->ruled_start.y) < 3.0) {
            continue;
        }
        f64 _ai, _offi;
        fude_zoom_instrument_edge_nearest(_ins, _ins->cross_slot[_i], _ins->cross_edge[_i], _at, &_ai, &_offi);
        if(_offi < _off2) {
            _off2 = _offi;
            _pick = (i32)_i;   // (its along: the nearest's, a hair from it)
        }
    }
    const fude_zoom_v2 _x = _ins->cross_at[_pick];
    // Which edge the pen is nearer now: this one (past the crossing), or the one it crosses.
    const f64 _off1 = hypot(_at.x - _q.x, _at.y - _q.y);
    if(_off2 < _off1) {
        // Turned: the corner at the crossing; the other edge drawn along from it.
        _ins->ruled_now        = _pick_along;
        _ins->ruled_out_before = fude_zoom_instruments_ruled_outward(_ins);
        _ins->ruled_corner     = _x;
        _ins->ruled_turned     = true;
        _ins->ruled            = (i32)_ins->cross_slot[_pick];
        _ins->ruled_edge       = _ins->cross_edge[_pick];
        f64 _ax, _offx;
        fude_zoom_instrument_edge_nearest(_ins, (u32)_ins->ruled, _ins->ruled_edge, _x, &_ax, &_offx);
        _ins->ruled_from  = _ax;
        _ins->ruled_now   = _ax;
        _ins->ruled_start    = _x;
        _ins->ruled_cornered = true;
        _ins->ruled_held     = -1;
        fude_zoom_instruments_find_crosses(_ins);
        const fude_zoom_v2 _q2 = fude_zoom_instruments_follow_edge(_ins, _at);
        if(fude_zoom_instruments_chain_depth >= 4u) {
            return _q2;
        }
        fude_zoom_instruments_chain_depth++;   // (from the corner on, as any move: a few corners at most at once)
        const fude_zoom_v2 _r2 = fude_zoom_instruments_chain(_ins, _at, _ax, _q2);
        fude_zoom_instruments_chain_depth--;
        return _r2;
    }
    // Nearer this one: as with real ones, the line cannot go on into the instrument it meets
    // (past the crossing, beside this edge, is inside the other's body: held at the crossing
    // till the pen turns, or goes back); where nothing is in the way (a compass's circle,
    // leaving one the line began under) it goes on.
    const u32 _other = _ins->cross_slot[_pick];
    b8 _blocked = false;
    if(_ins->tools[_other].kind != FUDE_ZOOM_INSTRUMENT_COMPASS) {
        const f64 _len = hypot(_q.x - _x.x, _q.y - _x.y);
        const fude_zoom_v2 _way = _len > 1e-9 ? (fude_zoom_v2){ (_q.x - _x.x) / _len, (_q.y - _x.y) / _len } : (fude_zoom_v2){ 0.0, 0.0 };
        const fude_zoom_v2 _out = fude_zoom_instruments_ruled_outward(_ins);
        const fude_zoom_v2 _probe = { _x.x + _way.x * 3.0 + _out.x * 1.5, _x.y + _way.y * 3.0 + _out.y * 1.5 };
        _blocked = _len > 1e-9 && fude_zoom_instrument_inside(_ins, _other, _probe);
    }
    if(_blocked) {
        if(_ins->ruled_held != _pick) {
            _ins->ruled_hold_way = _now > _pick_along ? 1.0 : -1.0;   // (the way it came: back the other way, let go)
        }
        _ins->ruled_held = _pick;
        _ins->ruled_now  = _pick_along;
        return _x;
    }
    _ins->ruled_held = -1;
    return _q;
}

void fude_zoom_instruments_pen_up(fude_zoom_instruments* _ins) {
    _ins->ruled = -1;
    fude_zoom_instruments_closing_up(_ins, FUDE_ZOOM_INSTRUMENT_PEN_HAND);
    if(_ins->held >= 0 && _ins->hand_id[0] == FUDE_ZOOM_INSTRUMENT_PEN_HAND) {
        _ins->held    = -1;
        _ins->hands   = 0u;
        _ins->turning = false;
        _ins->growing = false;
        _ins->docked  = -1;
    }
}

b8 fude_zoom_instruments_ruled(const fude_zoom_instruments* _ins, f64* _along, b8* _degrees, fude_zoom_v2* _label_at) {
    if(_ins->ruled < 0) {
        return false;
    }
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    fude_zoom_instrument_edges(_ins, (u32)_ins->ruled, _e);
    const fude_zoom_instrument_edge* _ed = &_e[_ins->ruled_edge];
    if(_ed->curve) {
        // How far along the curve; said beside the pen, outside the instrument.
        fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS], _dir;
        const u32 _m = fude_zoom_instrument_curve_of(_ins, (u32)_ins->ruled, _ed, _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
        if(_m == 0u) {
            return false;
        }
        const fude_zoom_v2 _q = fude_zoom_instrument_curve_at(_c, _m, _ins->ruled_now, &_dir, _ed->open);
        const f64 _side = _ed->hole ? -1.0 : 1.0;
        *_degrees  = false;
        *_along    = fabs(_ins->ruled_now - _ins->ruled_from);
        *_label_at = (fude_zoom_v2){ _q.x + _dir.y * 30.0 * _side, _q.y - _dir.x * 30.0 * _side };
        return true;
    }
    *_degrees = _ed->arc;
    *_along   = _ed->arc ? fabs(_ins->ruled_now - _ins->ruled_from) * 180.0 / FUDE_ZOOM_INSTRUMENT_PI : fabs(_ins->ruled_now - _ins->ruled_from);
    // Beside the pen, outside the edge.
    if(_ed->arc) {
        const f64 _t = _ed->from + _ins->ruled_now;
        *_label_at = (fude_zoom_v2){ _ed->centre.x + cos(_t) * (_ed->radius + 30.0), _ed->centre.y + sin(_t) * (_ed->radius + 30.0) };
    } else {
        const fude_zoom_v2 _dir = fude_zoom_v2_norm((fude_zoom_v2){ _ed->b.x - _ed->a.x, _ed->b.y - _ed->a.y });
        *_label_at = (fude_zoom_v2){ _ed->a.x + _dir.x * _ins->ruled_now + _dir.y * 30.0, _ed->a.y + _dir.y * _ins->ruled_now - _dir.x * 30.0 };
    }
    return true;
}

b8 fude_zoom_instruments_ruled_round(const fude_zoom_instruments* _ins) {
    if(_ins->ruled < 0) {
        return false;
    }
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    fude_zoom_instrument_edges(_ins, (u32)_ins->ruled, _e);
    return _e[_ins->ruled_edge].arc && !_e[_ins->ruled_edge].curve;
}

b8 fude_zoom_instruments_ruled_traced(const fude_zoom_instruments* _ins) {
    if(_ins->ruled < 0) {
        return false;
    }
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    fude_zoom_instrument_edges(_ins, (u32)_ins->ruled, _e);
    return _e[_ins->ruled_edge].trace;
}

b8 fude_zoom_instruments_ruled_straight(const fude_zoom_instruments* _ins) {
    if(_ins->ruled < 0) {
        return false;
    }
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    fude_zoom_instrument_edges(_ins, (u32)_ins->ruled, _e);
    return !_e[_ins->ruled_edge].arc && !_e[_ins->ruled_edge].curve;
}

fude_zoom_v2 fude_zoom_instruments_ruled_outward(const fude_zoom_instruments* _ins) {
    if(_ins->ruled < 0) {
        return (fude_zoom_v2){ 0.0, 0.0 };
    }
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    fude_zoom_instrument_edges(_ins, (u32)_ins->ruled, _e);
    const fude_zoom_instrument_edge* _ed = &_e[_ins->ruled_edge];
    if(_ed->curve) {
        // Round it counter-clockwise, its outside on the right; a hole's inside, on the left.
        fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS], _dir;
        const u32 _m = fude_zoom_instrument_curve_of(_ins, (u32)_ins->ruled, _ed, _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
        if(_m == 0u) {
            return (fude_zoom_v2){ 0.0, 0.0 };
        }
        fude_zoom_instrument_curve_at(_c, _m, _ins->ruled_now, &_dir, _ed->open);
        return _ed->hole ? (fude_zoom_v2){ -_dir.y, _dir.x } : (fude_zoom_v2){ _dir.y, -_dir.x };
    }
    if(_ed->arc) {
        if(_ins->tools[_ins->ruled].kind == FUDE_ZOOM_INSTRUMENT_CIRCLE) {
            const f64 _t = _ed->from + _ins->ruled_now;
            return (fude_zoom_v2){ -cos(_t), -sin(_t) };   // the hole is its outside: the line inside it
        }
        if(_ed->span >= 2.0 * FUDE_ZOOM_INSTRUMENT_PI - 1e-9) {
            return (fude_zoom_v2){ 0.0, 0.0 };
        }
        const f64 _t = _ed->from + _ins->ruled_now;
        return (fude_zoom_v2){ cos(_t), sin(_t) };
    }
    const fude_zoom_v2 _d = fude_zoom_v2_norm((fude_zoom_v2){ _ed->b.x - _ed->a.x, _ed->b.y - _ed->a.y });
    return (fude_zoom_v2){ _d.y, -_d.x };   // (counter-clockwise round it: its outside on the right)
}

// --- drawing them ---------------------------------------------------------------------------

RDE_INTERNAL rde_vec_2F fude_zoom_v2_f(fude_zoom_v2 _v) { return (rde_vec_2F){ (f32)_v.x, (f32)_v.y }; }

// A number for a graduation: no trailing zeros, at most four decimals.
RDE_INTERNAL void fude_zoom_instrument_number(f64 _v, c8* _out, usize _size) {
    snprintf(_out, _size, "%.4f", _v);
    c8* _dot = strchr(_out, '.');
    if(_dot != NULL) {
        c8* _end = _out + strlen(_out) - 1;
        while(_end > _dot && *_end == '0') { *_end-- = 0; }
        if(_end == _dot) { *_end = 0; }
    }
    if(strcmp(_out, "-0") == 0) { snprintf(_out, _size, "0"); }
}

// Text centred on _at (upright on the screen, whatever the instrument's turn:
// the app's font is drawn from curves, which do not turn).
RDE_INTERNAL void fude_zoom_instrument_text(const fude_zoom_instruments_look* _look, const c8* _text, fude_zoom_v2 _at, f32 _px, rde_color _color) {
    if(_look->font == NULL) {
        return;
    }
    const f32 _w = fude_draw_text_width(_look->font, _look->font_px, _text, _px);
    rde_rendering_2d_draw_text_2(_look->font, _text, (rde_vec_3F){ (f32)_at.x - _w * 0.5f, (f32)_at.y - _px * 0.36f, 0.0f },
                                 (rde_vec_2F){ _px / _look->font_px, _px / _look->font_px }, 0.0f, _color);
}

// The graduations along a straight edge: a millimetre's ticks (or a finer or
// coarser step, so they stay at least 7 points apart), every fifth longer,
// every tenth longer still and numbered — in centimetres while they are 10 mm
// apart, as on a real ruler.
RDE_INTERNAL void fude_zoom_instrument_ticks(const fude_zoom_instrument_edge* _e, const fude_zoom_instruments_look* _look, f64 _zero) {
    const fude_zoom_v2 _d   = { _e->b.x - _e->a.x, _e->b.y - _e->a.y };
    const f64          _len = hypot(_d.x, _d.y);
    const fude_zoom_v2 _dir = _e->zero_at_b ? fude_zoom_v2_norm((fude_zoom_v2){ -_d.x, -_d.y }) : fude_zoom_v2_norm(_d);
    const fude_zoom_v2 _o   = _e->zero_at_b ? _e->b : _e->a;
    const fude_zoom_v2 _in  = _e->zero_at_b ? (fude_zoom_v2){ _dir.y, -_dir.x } : (fude_zoom_v2){ -_dir.y, _dir.x };   // into the body
    // A thin plate (a ruler far out: as wide as it lies on the drawing): its marks
    // shorter, its numbers left off (they would meet the other edge's).
    const f64 _fit     = _e->room > 0.0 ? fmin(1.0, _e->room / FUDE_ZOOM_RULER_WIDTH) : 1.0;
    const b8  _numbers = !(_e->room > 0.0) || _e->room >= 44.0;
    if(!(_look->mm_per_point > 0.0)) {
        return;
    }
    // The step: 1, 2 or 5 times a power of ten millimetres.
    const f64 _min_mm = 7.0 * _look->mm_per_point;
    f64 _p10 = pow(10.0, floor(log10(_min_mm)));
    f64 _step = _p10;
    u32 _series = 1u;
    if(_step < _min_mm)       { _step = 2.0 * _p10; _series = 2u; }
    if(_step < _min_mm)       { _step = 5.0 * _p10; _series = 5u; }
    if(_step < _min_mm)       { _step = 10.0 * _p10; _series = 1u; _p10 *= 10.0; }
    const u32 _major = _series == 1u ? 10u : _series == 2u ? 5u : 2u;
    const f64 _major_mm = _step * (f64)_major;
    // The numbers' unit: centimetres for a 10 mm major, metres for a metre's, else millimetres.
    const c8* _unit = "mm";
    f64       _per  = 1.0;
    if(_major_mm >= 1000.0 - 1e-9)                         { _unit = "m";  _per = 1000.0; }
    else if(fabs(_major_mm - 10.0) < 1e-9)                 { _unit = "cm"; _per = 10.0; }
    else if(_major_mm >= 10.0 && _major_mm < 1000.0)       { _unit = "cm"; _per = 10.0; }
    const f64 _step_pt = _step / _look->mm_per_point;
    const f64 _label_every = _major_mm / _look->mm_per_point < 30.0 ? 2.0 : 1.0;   // numbers at least 30 points apart
    const b8  _crowded     = _major_mm / _look->mm_per_point * _label_every < 48.0;
    const u32 _count = (u32)fmin(floor((_len - _zero - 4.0) / _step_pt), 4000.0);
    for(u32 _i = 0; _i <= _count; _i++) {
        const f64 _t = _zero + (f64)_i * _step_pt;
        const b8  _big = _i % _major == 0u;
        const b8  _mid = !_big && _series == 1u && _i % 5u == 0u;
        const f64 _h   = _big ? 14.0 : _mid ? 10.0 : 6.0;
        const fude_zoom_v2 _p = { _o.x + _dir.x * _t, _o.y + _dir.y * _t };
        const fude_zoom_v2 _q = { _p.x + _in.x * _h * _fit, _p.y + _in.y * _h * _fit };
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_p), fude_zoom_v2_f(_q), _look->tick, _big ? 1.3f : 1.0f);
        if(_numbers && _big && fmod((f64)(_i / _major), _label_every) < 0.5) {
            c8 _n[32];
            fude_zoom_instrument_number((f64)_i * _step / _per, _n, sizeof(_n));
            if(_i == 0u && _crowded) {
                snprintf(_n, sizeof(_n), "%s", _unit);   // no room for it between the numbers: in the zero's place
            }
            const fude_zoom_v2 _at = { _p.x + _in.x * 21.0, _p.y + _in.y * 21.0 };
            fude_zoom_instrument_text(_look, _n, _at, 11.0f, _look->text);
        }
    }
    // Its unit, between the first two numbers.
    if(_numbers && !_crowded) {
        const f64 _mid_t = _zero + 0.5 * _step_pt * (f64)_major * _label_every;
        const fude_zoom_v2 _u = { _o.x + _dir.x * _mid_t + _in.x * 21.0, _o.y + _dir.y * _mid_t + _in.y * 21.0 };
        fude_zoom_instrument_text(_look, _unit, _u, 10.0f, _look->text);
    }
}

// The same in inches (the page's units in inches or feet): sixteenths, eighths,
// quarters or halves (as fine as stay 7 points apart), each finer one shorter,
// the inches numbered; zoomed out, inches with each foot numbered, then feet.
RDE_INTERNAL void fude_zoom_instrument_ticks_inch(const fude_zoom_instrument_edge* _e, const fude_zoom_instruments_look* _look, f64 _zero) {
    const fude_zoom_v2 _d   = { _e->b.x - _e->a.x, _e->b.y - _e->a.y };
    const f64          _len = hypot(_d.x, _d.y);
    const fude_zoom_v2 _dir = _e->zero_at_b ? fude_zoom_v2_norm((fude_zoom_v2){ -_d.x, -_d.y }) : fude_zoom_v2_norm(_d);
    const fude_zoom_v2 _o   = _e->zero_at_b ? _e->b : _e->a;
    const fude_zoom_v2 _in  = _e->zero_at_b ? (fude_zoom_v2){ _dir.y, -_dir.x } : (fude_zoom_v2){ -_dir.y, _dir.x };
    // A thin plate (a ruler far out: as wide as it lies on the drawing): its marks
    // shorter, its numbers left off (they would meet the other edge's).
    const f64 _fit     = _e->room > 0.0 ? fmin(1.0, _e->room / FUDE_ZOOM_RULER_WIDTH) : 1.0;
    const b8  _numbers = !(_e->room > 0.0) || _e->room >= 44.0;
    const f64 _min_in = 7.0 * _look->mm_per_point / 25.4;
    f64 _step_in;     // between ticks
    u32 _major;       // ticks between numbers
    f64 _per;         // a number's worth (inches)
    const c8* _unit;
    if(_min_in <= 0.5) {
        _step_in = 1.0 / 16.0;
        while(_step_in < _min_in) { _step_in *= 2.0; }
        _major = (u32)llround(1.0 / _step_in);
        _per   = 1.0;
        _unit  = "in";
    } else if(_min_in <= 1.0) {
        _step_in = 1.0;
        _major   = 12u;
        _per     = 12.0;
        _unit    = "ft";
    } else {
        // Feet, 1, 2 or 5 of a power of ten of them.
        const f64 _min_ft = _min_in / 12.0;
        const f64 _p10 = pow(10.0, floor(log10(_min_ft)));
        f64 _ft = _p10;
        if(_ft < _min_ft) { _ft = 2.0 * _p10; }
        if(_ft < _min_ft) { _ft = 5.0 * _p10; }
        if(_ft < _min_ft) { _ft = 10.0 * _p10; }
        _step_in = _ft * 12.0;
        _major   = 10u;
        _per     = 12.0;
        _unit    = "ft";
    }
    const f64 _step_pt = _step_in * 25.4 / _look->mm_per_point;
    const f64 _label_every = (f64)_major * _step_pt < 30.0 ? 2.0 : 1.0;
    const b8  _crowded     = (f64)_major * _step_pt * _label_every < 48.0;
    const u32 _count = (u32)fmin(floor((_len - _zero - 4.0) / _step_pt), 4000.0);
    for(u32 _i = 0; _i <= _count; _i++) {
        const f64 _t = _zero + (f64)_i * _step_pt;
        const b8  _big = _i % _major == 0u;
        // Shorter for each halving below the number's: the half, the quarter, the eighth, the sixteenth.
        f64 _h = 14.0;
        if(!_big) {
            u32 _level = 1u;
            for(u32 _m = _major / 2u; _m > 0u && _i % _m != 0u; _m /= 2u) {
                _level++;
            }
            _h = _major == 12u ? (_i % 6u == 0u ? 10.0 : 6.0) : fmax(14.0 - 2.5 * (f64)_level, 4.5);
        }
        const fude_zoom_v2 _p = { _o.x + _dir.x * _t, _o.y + _dir.y * _t };
        const fude_zoom_v2 _q = { _p.x + _in.x * _h * _fit, _p.y + _in.y * _h * _fit };
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_p), fude_zoom_v2_f(_q), _look->tick, _big ? 1.3f : 1.0f);
        if(_numbers && _big && fmod((f64)(_i / _major), _label_every) < 0.5) {
            c8 _n[32];
            fude_zoom_instrument_number((f64)_i * _step_in / _per, _n, sizeof(_n));
            if(_i == 0u && _crowded) {
                snprintf(_n, sizeof(_n), "%s", _unit);   // no room for it between the numbers: in the zero's place
            }
            const fude_zoom_v2 _at = { _p.x + _in.x * 21.0, _p.y + _in.y * 21.0 };
            fude_zoom_instrument_text(_look, _n, _at, 11.0f, _look->text);
        }
    }
    if(_numbers && !_crowded) {
        const f64 _mid_t = _zero + 0.5 * _step_pt * (f64)_major * _label_every;   // between the first two numbers
        const fude_zoom_v2 _u = { _o.x + _dir.x * _mid_t + _in.x * 21.0, _o.y + _dir.y * _mid_t + _in.y * 21.0 };
        fude_zoom_instrument_text(_look, _unit, _u, 10.0f, _look->text);
    }
}

// The protractor's degrees: every one ticked, every fifth longer, every
// tenth numbered both ways round (as on a real one: 0 at either end).
RDE_INTERNAL void fude_zoom_instrument_degrees(const fude_zoom_instrument_edge* _e, const fude_zoom_instruments_look* _look) {
    // As many as its size has room for (it is as big as it is on the drawing): every degree
    // ticked and every tenth numbered both ways round; smaller, fewer; small, ticks only.
    const f64 _r     = _e->radius;
    const u32 _tick  = _r >= 110.0 ? 1u : (_r >= 60.0 ? 5u : 10u);
    const u32 _every = _r >= 140.0 ? 10u : (_r >= 90.0 ? 30u : 0u);
    const b8  _both  = _r >= 140.0;
    for(u32 _d = 0; _d <= 180u; _d++) {
        if(_d % _tick != 0u) {
            continue;
        }
        const f64 _t = _e->from + FUDE_ZOOM_INSTRUMENT_PI * (f64)_d / 180.0;
        const f64 _h = (_d % 10u == 0u ? 14.0 : _d % 5u == 0u ? 9.0 : 5.0) * fmin(1.0, _r / 110.0);
        const fude_zoom_v2 _p = { _e->centre.x + cos(_t) * _e->radius, _e->centre.y + sin(_t) * _e->radius };
        const fude_zoom_v2 _q = { _e->centre.x + cos(_t) * (_e->radius - _h), _e->centre.y + sin(_t) * (_e->radius - _h) };
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_p), fude_zoom_v2_f(_q), _look->tick, _d % 10u == 0u ? 1.3f : 1.0f);
        if(_every > 0u && _d % _every == 0u && !_both) {
            c8 _n[8];
            snprintf(_n, sizeof(_n), "%u", _d);
            fude_zoom_instrument_text(_look, _n, (fude_zoom_v2){ _e->centre.x + cos(_t) * (_e->radius - 24.0), _e->centre.y + sin(_t) * (_e->radius - 24.0) }, 10.0f, _look->text);
            continue;
        }
        if(_both && _d % 10u == 0u) {
            c8 _n[8];
            snprintf(_n, sizeof(_n), "%u", _d);
            fude_zoom_instrument_text(_look, _n, (fude_zoom_v2){ _e->centre.x + cos(_t) * (_e->radius - 26.0), _e->centre.y + sin(_t) * (_e->radius - 26.0) }, 10.0f, _look->text);
            snprintf(_n, sizeof(_n), "%u", 180u - _d);
            fude_zoom_instrument_text(_look, _n, (fude_zoom_v2){ _e->centre.x + cos(_t) * (_e->radius - 44.0), _e->centre.y + sin(_t) * (_e->radius - 44.0) }, 9.0f, _look->tick);
        }
    }
    // Its centre: a small cross on the straight edge.
    const fude_zoom_v2 _c = _e->centre;
    const fude_zoom_v2 _u = { cos(_e->from + FUDE_ZOOM_INSTRUMENT_PI * 0.5), sin(_e->from + FUDE_ZOOM_INSTRUMENT_PI * 0.5) };
    rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_c), fude_zoom_v2_f((fude_zoom_v2){ _c.x + _u.x * 18.0, _c.y + _u.y * 18.0 }), _look->tick, 1.3f);
}

// The compass: the circle its pencil goes round (faint; the arc being drawn
// lit), its legs to the hinge, its needle and its pencil, its radius written.
RDE_INTERNAL void fude_zoom_instrument_render_compass(const fude_zoom_instruments* _ins, u32 _slot, const fude_zoom_instruments_look* _look) {
    const fude_zoom_instrument* _t = &_ins->tools[_slot];
    const f64 _r = _t->radius > 1.0 ? _t->radius : FUDE_ZOOM_COMPASS_R;
    rde_color _faint = _look->accent;
    _faint.a = 90u;
    const u32 _sides = 96u;
    for(u32 _i = 0; _i < _sides; _i++) {
        const f64 _a0 = 2.0 * FUDE_ZOOM_INSTRUMENT_PI * (f64)_i / (f64)_sides, _a1 = 2.0 * FUDE_ZOOM_INSTRUMENT_PI * (f64)(_i + 1u) / (f64)_sides;
        if(_i % 2u == 0u) {   // (dashed: the way round, not a line drawn)
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)(_t->at.x + cos(_a0) * _r), (f32)(_t->at.y + sin(_a0) * _r) },
                                         (rde_vec_2F){ (f32)(_t->at.x + cos(_a1) * _r), (f32)(_t->at.y + sin(_a1) * _r) }, _faint, 1.2f);
        }
    }
    if(_ins->ruled == (i32)_slot) {
        const f64 _from = _t->angle + fmin(_ins->ruled_from, _ins->ruled_now), _to = _t->angle + fmax(_ins->ruled_from, _ins->ruled_now);
        const u32 _k = (u32)fmin(fmax((_to - _from) / 0.05, 1.0), 400.0);
        for(u32 _i = 0; _i < _k; _i++) {
            const f64 _a0 = _from + (_to - _from) * (f64)_i / (f64)_k, _a1 = _from + (_to - _from) * (f64)(_i + 1u) / (f64)_k;
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)(_t->at.x + cos(_a0) * _r), (f32)(_t->at.y + sin(_a0) * _r) },
                                         (rde_vec_2F){ (f32)(_t->at.x + cos(_a1) * _r), (f32)(_t->at.y + sin(_a1) * _r) }, _look->accent, 2.4f);
        }
    }
    const fude_zoom_v2 _pencil = fude_zoom_instrument_place(_t, (fude_zoom_v2){ _r, 0.0 });
    const fude_zoom_v2 _hinge  = fude_zoom_instrument_compass_hinge(_ins, _slot);
    // The legs: a body's width, outlined.
    const fude_zoom_v2 _ends[2] = { _t->at, _pencil };
    for(u32 _l = 0; _l < 2u; _l++) {
        const fude_zoom_v2 _d = fude_zoom_v2_norm((fude_zoom_v2){ _hinge.x - _ends[_l].x, _hinge.y - _ends[_l].y });
        const fude_zoom_v2 _n = { -_d.y * 5.0, _d.x * 5.0 };
        const rde_vec_2F _q[4] = { { (f32)(_ends[_l].x + _n.x * 0.3), (f32)(_ends[_l].y + _n.y * 0.3) }, { (f32)(_hinge.x + _n.x), (f32)(_hinge.y + _n.y) },
                                   { (f32)(_hinge.x - _n.x), (f32)(_hinge.y - _n.y) }, { (f32)(_ends[_l].x - _n.x * 0.3), (f32)(_ends[_l].y - _n.y * 0.3) } };
        rde_rendering_2d_draw_polygon(_q, 4u, _look->body, NULL);
        for(u32 _i = 0; _i < 4u; _i++) {
            rde_rendering_2d_draw_line_1(_q[_i], _q[(_i + 1u) % 4u], _look->line, 1.0f);
        }
    }
    // The needle (a fine point) and the pencil (its lead).
    rde_rendering_2d_draw_circle_with_border(fude_zoom_v2_f(_t->at), 4.0f, 16u, _look->body, 1.5f, _look->line, NULL);
    rde_rendering_2d_draw_circle(fude_zoom_v2_f(_t->at), 1.5f, 12u, _look->line, NULL);
    rde_rendering_2d_draw_circle_with_border(fude_zoom_v2_f(_pencil), 4.5f, 16u, _look->accent, 1.2f, _look->line, NULL);
    // The hinge: what opens it.
    rde_rendering_2d_draw_circle_with_border(fude_zoom_v2_f(_hinge), (f32)(FUDE_ZOOM_INSTRUMENT_KNOB * fude_zoom_instrument_icons(_t)), 24u, _look->body, 1.2f,
                                             _ins->turning && _ins->held == (i32)_slot ? _look->accent : _look->line, NULL);
    rde_rendering_2d_draw_circle(fude_zoom_v2_f(_hinge), 3.0f, 12u, _look->line, NULL);
    // Its radius, written above the hinge (tapped: typed).
    if(_look->units != NULL && _look->font != NULL) {
        c8 _v[FUDE_ZOOM_UNITS_TEXT], _said[FUDE_ZOOM_UNITS_TEXT + 8u];
        fude_zoom_units_format(_t->stuck_mm > 0.0 ? _t->stuck_mm : _r * _look->mm_per_point, _look->units, _v, sizeof(_v));
        snprintf(_said, sizeof(_said), "R %s", _v);
        const fude_zoom_v2 _at = fude_zoom_instrument_compass_label(_ins, _slot);
        const f32 _w = fude_draw_text_width(_look->font, _look->font_px, _said, 13.0f) + 16.0f;
        rde_rendering_2d_draw_rounded_rectangle_with_border(fude_zoom_v2_f(_at), (rde_vec_2F){ _w, 24.0f }, 1.0f, 8u, _look->body, 1.0f, _look->accent, NULL);
        fude_zoom_instrument_text(_look, _said, _at, 13.0f, _look->text);
    }
}

// A circle template: its plate round the hole (the drawing seen through the hole),
// a tick at each quarter of the hole's edge and its centre marked, the arc being
// drawn lit; its diameter written under it (tapped: typed).
RDE_INTERNAL void fude_zoom_instrument_render_circle(const fude_zoom_instruments* _ins, u32 _slot, const fude_zoom_instruments_look* _look) {
    const fude_zoom_instrument* _t = &_ins->tools[_slot];
    const f64 _r = _t->radius > 1.0 ? _t->radius : FUDE_ZOOM_COMPASS_R;
    const f64 _h = _r + fude_zoom_instrument_rim(_t);
    const u32 _sides = 96u;
    for(u32 _i = 0; _i < _sides; _i++) {
        // A ray out from the centre to the plate's square edge, both ways round each side.
        rde_vec_2F _q[4];
        for(u32 _k = 0; _k < 2u; _k++) {
            const f64 _a  = 2.0 * FUDE_ZOOM_INSTRUMENT_PI * (f64)(_i + _k) / (f64)_sides;
            const f64 _c  = cos(_a), _sn = sin(_a);
            const f64 _m  = fmax(fabs(_c), fabs(_sn));
            const fude_zoom_v2 _in  = fude_zoom_instrument_place(_t, (fude_zoom_v2){ _c * _r, _sn * _r });
            const fude_zoom_v2 _out = fude_zoom_instrument_place(_t, (fude_zoom_v2){ _c / _m * _h, _sn / _m * _h });
            _q[_k == 0u ? 0u : 1u] = fude_zoom_v2_f(_in);
            _q[_k == 0u ? 3u : 2u] = fude_zoom_v2_f(_out);
        }
        rde_rendering_2d_draw_polygon(_q, 4u, _look->body, NULL);
        rde_rendering_2d_draw_line_1(_q[0], _q[1], _look->line, 1.2f);
    }
    fude_zoom_v2 _sq[4];
    fude_zoom_instrument_outline(_ins, _slot, _sq, 4u);
    for(u32 _i = 0; _i < 4u; _i++) {
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_sq[_i]), fude_zoom_v2_f(_sq[(_i + 1u) % 4u]), _look->line, 1.2f);
    }
    for(u32 _q4 = 0; _q4 < 4u; _q4++) {
        const f64 _a = _t->angle + FUDE_ZOOM_INSTRUMENT_PI * 0.5 * (f64)_q4;
        const fude_zoom_v2 _d = { cos(_a), sin(_a) };
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f((fude_zoom_v2){ _t->at.x + _d.x * _r, _t->at.y + _d.y * _r }),
                                     fude_zoom_v2_f((fude_zoom_v2){ _t->at.x + _d.x * (_r + 14.0), _t->at.y + _d.y * (_r + 14.0) }), _look->tick, 1.3f);
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f((fude_zoom_v2){ _t->at.x + _d.x * 3.0, _t->at.y + _d.y * 3.0 }),
                                     fude_zoom_v2_f((fude_zoom_v2){ _t->at.x + _d.x * 9.0, _t->at.y + _d.y * 9.0 }), _look->tick, 1.0f);
    }
    if(_ins->ruled == (i32)_slot) {
        const f64 _from = _t->angle + fmin(_ins->ruled_from, _ins->ruled_now), _to = _t->angle + fmax(_ins->ruled_from, _ins->ruled_now);
        const u32 _k = (u32)fmin(fmax((_to - _from) / 0.05, 1.0), 400.0);
        for(u32 _i = 0; _i < _k; _i++) {
            const f64 _a0 = _from + (_to - _from) * (f64)_i / (f64)_k, _a1 = _from + (_to - _from) * (f64)(_i + 1u) / (f64)_k;
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)(_t->at.x + cos(_a0) * _r), (f32)(_t->at.y + sin(_a0) * _r) },
                                         (rde_vec_2F){ (f32)(_t->at.x + cos(_a1) * _r), (f32)(_t->at.y + sin(_a1) * _r) }, _look->accent, 2.4f);
        }
    }
    if(_look->units != NULL && _look->font != NULL) {
        c8 _v[FUDE_ZOOM_UNITS_TEXT], _said[FUDE_ZOOM_UNITS_TEXT + 8u];
        fude_zoom_units_format(2.0 * (_t->stuck_mm > 0.0 ? _t->stuck_mm : _r * _look->mm_per_point), _look->units, _v, sizeof(_v));
        snprintf(_said, sizeof(_said), "\xC3\x98 %s", _v);   // (Ø: the font has no ⌀)
        const fude_zoom_v2 _at = fude_zoom_instrument_compass_label(_ins, _slot);
        const f32 _w = fude_draw_text_width(_look->font, _look->font_px, _said, 13.0f) + 16.0f;
        rde_rendering_2d_draw_rounded_rectangle_with_border(fude_zoom_v2_f(_at), (rde_vec_2F){ _w, 24.0f }, 1.0f, 8u, _look->body, 1.0f, _look->accent, NULL);
        fude_zoom_instrument_text(_look, _said, _at, 13.0f, _look->text);
    }
}

// The stretch of a curve the pen has drawn along, lit.
RDE_INTERNAL void fude_zoom_instrument_render_curve_lit(const fude_zoom_instruments* _ins, u32 _slot, const fude_zoom_v2* _c, u32 _m, const fude_zoom_instruments_look* _look, b8 _open) {
    if(_ins->ruled != (i32)_slot || _m < 2u) {
        return;
    }
    const f64 _from = fmin(_ins->ruled_from, _ins->ruled_now), _to = fmax(_ins->ruled_from, _ins->ruled_now);
    const u32 _k = (u32)fmin(fmax((_to - _from) / 3.0, 1.0), 600.0);
    fude_zoom_v2 _dir, _prev = fude_zoom_instrument_curve_at(_c, _m, _from, &_dir, _open);
    for(u32 _i = 1; _i <= _k; _i++) {
        const fude_zoom_v2 _next = fude_zoom_instrument_curve_at(_c, _m, _from + (_to - _from) * (f64)_i / (f64)_k, &_dir, _open);
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_prev), fude_zoom_v2_f(_next), _look->accent, 2.4f);
        _prev = _next;
    }
}

// An ellipse template: its plate round the hole, ticks at its axes' ends, its centre
// marked; its width and height written under it (tapped: typed).
RDE_INTERNAL void fude_zoom_instrument_render_ellipse(const fude_zoom_instruments* _ins, u32 _slot, const fude_zoom_instruments_look* _look) {
    const fude_zoom_instrument* _t = &_ins->tools[_slot];
    f64 _rx, _ry, _hx, _hy;
    fude_zoom_instrument_ellipse(_t, &_rx, &_ry, &_hx, &_hy);
    const u32 _sides = 120u;
    for(u32 _i = 0; _i < _sides; _i++) {
        rde_vec_2F _q[4];
        for(u32 _k = 0; _k < 2u; _k++) {
            const f64 _a = 2.0 * FUDE_ZOOM_INSTRUMENT_PI * (f64)(_i + _k) / (f64)_sides;
            const f64 _c = cos(_a), _sn = sin(_a);
            const f64 _m = fmax(fabs(_c) / _hx, fabs(_sn) / _hy);
            _q[_k == 0u ? 0u : 1u] = fude_zoom_v2_f(fude_zoom_instrument_place(_t, (fude_zoom_v2){ _c * _rx, _sn * _ry }));
            _q[_k == 0u ? 3u : 2u] = fude_zoom_v2_f(fude_zoom_instrument_place(_t, (fude_zoom_v2){ _c / _m, _sn / _m }));
        }
        rde_rendering_2d_draw_polygon(_q, 4u, _look->body, NULL);
        rde_rendering_2d_draw_line_1(_q[0], _q[1], _look->line, 1.2f);
    }
    fude_zoom_v2 _sq[4];
    fude_zoom_instrument_outline(_ins, _slot, _sq, 4u);
    for(u32 _i = 0; _i < 4u; _i++) {
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_sq[_i]), fude_zoom_v2_f(_sq[(_i + 1u) % 4u]), _look->line, 1.2f);
    }
    const fude_zoom_v2 _ends[4][2] = { { { _rx, 0.0 }, { _rx + 14.0, 0.0 } }, { { -_rx, 0.0 }, { -_rx - 14.0, 0.0 } },
                                       { { 0.0, _ry }, { 0.0, _ry + 14.0 } }, { { 0.0, -_ry }, { 0.0, -_ry - 14.0 } } };
    for(u32 _i = 0; _i < 4u; _i++) {
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(fude_zoom_instrument_place(_t, _ends[_i][0])), fude_zoom_v2_f(fude_zoom_instrument_place(_t, _ends[_i][1])), _look->tick, 1.3f);
        const fude_zoom_v2 _d = { _ends[_i][1].x - _ends[_i][0].x, _ends[_i][1].y - _ends[_i][0].y };
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(fude_zoom_instrument_place(_t, (fude_zoom_v2){ _d.x * 0.2, _d.y * 0.2 })),
                                     fude_zoom_v2_f(fude_zoom_instrument_place(_t, (fude_zoom_v2){ _d.x * 0.6, _d.y * 0.6 })), _look->tick, 1.0f);
    }
    fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
    const u32 _m = fude_zoom_instrument_curve(_ins, _slot, _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
    fude_zoom_instrument_render_curve_lit(_ins, _slot, _c, _m, _look, false);
    if(_look->units != NULL && _look->font != NULL) {
        c8 _w[FUDE_ZOOM_UNITS_TEXT], _h[FUDE_ZOOM_UNITS_TEXT], _said[2u * FUDE_ZOOM_UNITS_TEXT + 8u];
        fude_zoom_units_format(2.0 * (_t->stuck_mm > 0.0 ? _t->stuck_mm : _rx * _look->mm_per_point), _look->units, _w, sizeof(_w));
        fude_zoom_units_format(2.0 * (_t->stuck_mm2 > 0.0 ? _t->stuck_mm2 : _ry * _look->mm_per_point), _look->units, _h, sizeof(_h));
        snprintf(_said, sizeof(_said), "%s \xC3\x97 %s", _w, _h);
        const fude_zoom_v2 _at = fude_zoom_instrument_compass_label(_ins, _slot);
        const f32 _tw = fude_draw_text_width(_look->font, _look->font_px, _said, 13.0f) + 16.0f;
        rde_rendering_2d_draw_rounded_rectangle_with_border(fude_zoom_v2_f(_at), (rde_vec_2F){ _tw, 24.0f }, 1.0f, 8u, _look->body, 1.0f, _look->accent, NULL);
        fude_zoom_instrument_text(_look, _said, _at, 13.0f, _look->text);
    }
}

// A French curve: its plate (the curve its edge), the stretch drawn along lit.
RDE_INTERNAL void fude_zoom_instrument_render_french(const fude_zoom_instruments* _ins, u32 _slot, const fude_zoom_instruments_look* _look) {
    fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
    const u32 _m = fude_zoom_instrument_curve(_ins, _slot, _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
    if(_m < 3u) {
        return;
    }
    rde_arr _tri = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    const u32 _n = fude_zoom_fill_triangulate(_c, _m, &_tri);
    const fude_zoom_v2* _tp = (const fude_zoom_v2*)_tri.memory;
    for(u32 _i = 0; _i < _n; _i++) {
        const rde_vec_2F _q[3] = { fude_zoom_v2_f(_tp[3u * _i]), fude_zoom_v2_f(_tp[3u * _i + 1u]), fude_zoom_v2_f(_tp[3u * _i + 2u]) };
        rde_rendering_2d_draw_polygon(_q, 3u, _look->body, NULL);
    }
    rde_arr_free(&_tri);
    for(u32 _i = 0; _i < _m; _i++) {
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_c[_i]), fude_zoom_v2_f(_c[(_i + 1u) % _m]), _look->line, 1.2f);
    }
    fude_zoom_instrument_render_curve_lit(_ins, _slot, _c, _m, _look, false);
}

// Its size tab: a small round grip past its end, two arrows along its way (lit while held).
RDE_INTERNAL void fude_zoom_instrument_render_grow(const fude_zoom_instruments* _ins, u32 _slot, const fude_zoom_instruments_look* _look) {
    fude_zoom_v2 _at;
    if(!fude_zoom_instrument_grow_at(_ins, _slot, &_at)) {
        return;
    }
    const fude_zoom_instrument* _t  = &_ins->tools[_slot];
    const b8                    _on = _ins->growing && _ins->held == (i32)_slot;
    const f64 _g = fude_zoom_instrument_icons(_t);
    rde_rendering_2d_draw_circle_with_border(fude_zoom_v2_f(_at), (f32)(FUDE_ZOOM_INSTRUMENT_GROW * _g), 20u, _on ? _look->accent : _look->body, 1.2f, _look->line, NULL);
    const rde_color    _c = _on ? _look->body : _look->tick;
    const fude_zoom_v2 _d = { cos(_t->angle), sin(_t->angle) }, _n = { -_d.y, _d.x };
    for(i32 _s = -1; _s <= 1; _s += 2) {
        const fude_zoom_v2 _tip  = { _at.x + _d.x * 6.0 * _g * _s, _at.y + _d.y * 6.0 * _g * _s };
        const fude_zoom_v2 _back = { _at.x + _d.x * 2.0 * _g * _s, _at.y + _d.y * 2.0 * _g * _s };
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_tip), fude_zoom_v2_f((fude_zoom_v2){ _back.x + _n.x * 3.5 * _g, _back.y + _n.y * 3.5 * _g }), _c, 1.5f);
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_tip), fude_zoom_v2_f((fude_zoom_v2){ _back.x - _n.x * 3.5 * _g, _back.y - _n.y * 3.5 * _g }), _c, 1.5f);
    }
    rde_rendering_2d_draw_line_1(fude_zoom_v2_f((fude_zoom_v2){ _at.x - _d.x * 6.0 * _g, _at.y - _d.y * 6.0 * _g }), fude_zoom_v2_f((fude_zoom_v2){ _at.x + _d.x * 6.0 * _g, _at.y + _d.y * 6.0 * _g }), _c, 1.5f);
}

// Its ×: a small round button on it (lit while pressed).
RDE_INTERNAL void fude_zoom_instrument_render_close(const fude_zoom_instruments* _ins, u32 _slot, const fude_zoom_instruments_look* _look) {
    const fude_zoom_v2 _x  = fude_zoom_instrument_close_at(_ins, _slot);
    const b8           _on = _ins->closing == (i32)_slot;
    const f64          _g  = fude_zoom_instrument_icons(&_ins->tools[_slot]);
    rde_rendering_2d_draw_circle_with_border(fude_zoom_v2_f(_x), (f32)(FUDE_ZOOM_INSTRUMENT_CLOSE * _g), 20u, _on ? _look->accent : _look->body, 1.2f, _look->line, NULL);
    const f32 _d = (f32)(4.0 * _g);
    const rde_color _c = _on ? _look->body : _look->tick;
    rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_x.x - _d, (f32)_x.y - _d }, (rde_vec_2F){ (f32)_x.x + _d, (f32)_x.y + _d }, _c, 1.5f);
    rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_x.x - _d, (f32)_x.y + _d }, (rde_vec_2F){ (f32)_x.x + _d, (f32)_x.y - _d }, _c, 1.5f);
}

// A stencil: its see-through plate (its lines' box), its lines — the one the pen goes along lit as it goes.
RDE_INTERNAL void fude_zoom_instrument_render_stencil(const fude_zoom_instruments* _ins, u32 _slot, const fude_zoom_instruments_look* _look) {
    fude_zoom_v2 _ol[4];
    const u32 _n = fude_zoom_instrument_outline(_ins, _slot, _ol, 4u);
    rde_vec_2F _pf[4];
    for(u32 _i = 0; _i < _n; _i++) {
        _pf[_i] = fude_zoom_v2_f(_ol[_i]);
    }
    if(_n == 4u) {
        rde_rendering_2d_draw_polygon(_pf, 4u, _look->body, NULL);
        for(u32 _i = 0; _i < 4u; _i++) {
            rde_rendering_2d_draw_line_1(_pf[_i], _pf[(_i + 1u) % 4u], _look->line, 1.0f);
        }
    }
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    const u32 _ne = fude_zoom_instrument_edges(_ins, _slot, _e);
    for(u32 _j = 0; _j < _ne; _j++) {
        fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
        const u32 _m = fude_zoom_instrument_curve_of(_ins, _slot, &_e[_j], _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
        for(u32 _i = 0; _i + 1u < _m + (_e[_j].open ? 0u : 1u) && _m >= 2u; _i++) {
            rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_c[_i]), fude_zoom_v2_f(_c[(_i + 1u) % _m]), _look->tick, 2.0f);
        }
        if(_ins->ruled == (i32)_slot && _ins->ruled_edge == _j) {
            fude_zoom_instrument_render_curve_lit(_ins, _slot, _c, _m, _look, _e[_j].open);
        }
    }
}

void fude_zoom_instruments_render(const fude_zoom_instruments* _ins, const fude_zoom_instruments_look* _look, f64 _now) {
    for(u32 _o = 0; _o < FUDE_ZOOM_INSTRUMENT_MOST; _o++) {
        const u32 _k = _ins->order[_o];
        const fude_zoom_instrument* _t = &_ins->tools[_k];
        if(!_t->shown) {
            continue;
        }
        const u32 _kind = _t->kind;
        if(_kind == FUDE_ZOOM_INSTRUMENT_COMPASS) {
            fude_zoom_instrument_render_compass(_ins, _k, _look);
            fude_zoom_instrument_render_close(_ins, _k, _look);
            continue;
        }
        if(_kind == FUDE_ZOOM_INSTRUMENT_CIRCLE) {
            fude_zoom_instrument_render_circle(_ins, _k, _look);
            fude_zoom_instrument_render_close(_ins, _k, _look);
            continue;
        }
        if(_kind == FUDE_ZOOM_INSTRUMENT_ELLIPSE || _kind == FUDE_ZOOM_INSTRUMENT_CURVE || _kind == FUDE_ZOOM_INSTRUMENT_STENCIL) {
            if(_kind == FUDE_ZOOM_INSTRUMENT_ELLIPSE) {
                fude_zoom_instrument_render_ellipse(_ins, _k, _look);
            } else if(_kind == FUDE_ZOOM_INSTRUMENT_CURVE) {
                fude_zoom_instrument_render_french(_ins, _k, _look);
            } else {
                fude_zoom_instrument_render_stencil(_ins, _k, _look);
            }
            // Its knob (it turns), its ×, (a French curve's) its size tab.
            const fude_zoom_v2 _kn = fude_zoom_instrument_knob(_ins, _k);
            rde_rendering_2d_draw_circle_with_border(fude_zoom_v2_f(_kn), (f32)(FUDE_ZOOM_INSTRUMENT_KNOB * fude_zoom_instrument_icons(&_ins->tools[_k])), 24u, _look->body, 1.2f,
                                                     _ins->turning && _ins->held == (i32)_k ? _look->accent : _look->line, NULL);
            fude_zoom_instrument_render_close(_ins, _k, _look);
            fude_zoom_instrument_render_grow(_ins, _k, _look);
            continue;
        }
        fude_zoom_v2 _ol[FUDE_ZOOM_ARC_SIDES + 8u];
        const u32 _n = fude_zoom_instrument_outline(_ins, _k, _ol, FUDE_ZOOM_ARC_SIDES + 8u);
        rde_vec_2F _pf[FUDE_ZOOM_ARC_SIDES + 8u];
        for(u32 _i = 0; _i < _n; _i++) {
            _pf[_i] = fude_zoom_v2_f(_ol[_i]);
        }
        if(_kind == FUDE_ZOOM_INSTRUMENT_SQUARE_45 || _kind == FUDE_ZOOM_INSTRUMENT_SQUARE_30) {
            // A band round a hollow middle, as a real set square: three quads.
            fude_zoom_v2 _c[3], _h[3];
            fude_zoom_square_corners(_kind, fude_zoom_instrument_size(_t), _c);
            // The hollow: the triangle drawn in by the band's width all round,
            // which is the triangle shrunk about its incentre.
            const f64 _la = _c[1].x - _c[0].x, _lb = _c[2].y - _c[0].y;
            const f64 _ri = (_la + _lb - hypot(_la, _lb)) * 0.5;
            const fude_zoom_v2 _ic = { _c[0].x + _ri, _c[0].y + _ri };
            const f64 _shrink = fmax(0.0, (_ri - FUDE_ZOOM_SQUARE_HOLLOW) / _ri);
            for(u32 _i = 0; _i < 3u; _i++) {
                _h[_i] = fude_zoom_instrument_place(_t, (fude_zoom_v2){ _ic.x + (_c[_i].x - _ic.x) * _shrink, _ic.y + (_c[_i].y - _ic.y) * _shrink });
            }
            for(u32 _i = 0; _i < 3u; _i++) {
                const u32 _j = (_i + 1u) % 3u;
                const rde_vec_2F _q[4] = { _pf[_i], _pf[_j], fude_zoom_v2_f(_h[_j]), fude_zoom_v2_f(_h[_i]) };
                rde_rendering_2d_draw_polygon(_q, 4u, _look->body, NULL);
            }
            for(u32 _i = 0; _i < 3u; _i++) {
                rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_h[_i]), fude_zoom_v2_f(_h[(_i + 1u) % 3u]), _look->line, 1.0f);
            }
        } else {
            rde_rendering_2d_draw_polygon(_pf, _n, _look->body, NULL);
        }
        for(u32 _i = 0; _i < _n; _i++) {
            rde_rendering_2d_draw_line_1(_pf[_i], _pf[(_i + 1u) % _n], _look->line, 1.2f);
        }
        // The graduations.
        fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
        const u32 _ne = fude_zoom_instrument_edges(_ins, _k, _e);
        for(u32 _i = 0; _i < _ne; _i++) {
            if(!_e[_i].graduated) {
                continue;
            }
            if(_e[_i].arc) {
                fude_zoom_instrument_degrees(&_e[_i], _look);
            } else {
                const f64 _zero = _kind == FUDE_ZOOM_INSTRUMENT_RULER ? FUDE_ZOOM_RULER_ZERO : 0.0;
                if(_look->units != NULL && (_look->units->unit == FUDE_ZOOM_UNIT_IN || _look->units->unit == FUDE_ZOOM_UNIT_FT) && _look->mm_per_point > 0.0) {
                    fude_zoom_instrument_ticks_inch(&_e[_i], _look, _zero);   // the page in inches: so are they
                } else {
                    fude_zoom_instrument_ticks(&_e[_i], _look, _zero);
                }
            }
        }
        // The edges at work: the one the pen draws along, the two lying together.
        for(u32 _i = 0; _i < _ne; _i++) {
            b8 _lit = (_ins->ruled == (i32)_k && _ins->ruled_edge == _i) ||
                      (_ins->docked >= 0 && ((_ins->held == (i32)_k && _ins->own_edge == _i) || (_ins->docked == (i32)_k && _ins->dock_edge == _i)));
            for(u32 _c = 0; _ins->docked >= 0 && _ins->held >= 0 && _c < _ins->contacts && !_lit; _c++) {
                _lit = (_ins->contact_slot[_c] == (i32)_k && _ins->contact_edge[_c] == _i);   // (the others it rests against too)
            }
            if(!_lit) {
                continue;
            }
            if(_e[_i].arc) {
                // (an arc part of the way round: a corner gauge's round, lit as it is)
                if(_e[_i].span < 2.0 * FUDE_ZOOM_INSTRUMENT_PI - 1e-9 && !_e[_i].graduated) {
                    for(u32 _s = 0; _s < 24u; _s++) {
                        const f64 _a0 = _e[_i].from + _e[_i].span * (f64)_s / 24.0, _a1 = _e[_i].from + _e[_i].span * (f64)(_s + 1u) / 24.0;
                        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)(_e[_i].centre.x + cos(_a0) * _e[_i].radius), (f32)(_e[_i].centre.y + sin(_a0) * _e[_i].radius) },
                                                     (rde_vec_2F){ (f32)(_e[_i].centre.x + cos(_a1) * _e[_i].radius), (f32)(_e[_i].centre.y + sin(_a1) * _e[_i].radius) }, _look->accent, 2.4f);
                    }
                }
                continue;
            }
            rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_e[_i].a), fude_zoom_v2_f(_e[_i].b), _look->accent, 2.4f);
        }
        // Its knob.
        const fude_zoom_v2 _kn = fude_zoom_instrument_knob(_ins, _k);
        const f64          _kg = fude_zoom_instrument_icons(_t);
        rde_rendering_2d_draw_circle_with_border(fude_zoom_v2_f(_kn), (f32)(FUDE_ZOOM_INSTRUMENT_KNOB * _kg), 24u, _look->body, 1.2f,
                                                 _ins->turning && _ins->held == (i32)_k ? _look->accent : _look->line, NULL);
        const f64 _a = _t->angle;
        for(u32 _i = 0; _i < 2u; _i++) {
            // Two small curved arrows' worth: arcs either side, a turn's sign.
            const f64 _s0 = _a + (_i == 0u ? 0.35 : 0.35 + FUDE_ZOOM_INSTRUMENT_PI);
            for(u32 _s = 0; _s < 6u; _s++) {
                const f64 _u0 = _s0 + 0.32 * (f64)_s, _u1 = _s0 + 0.32 * (f64)(_s + 1u);
                rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)(_kn.x + cos(_u0) * 7.0 * _kg), (f32)(_kn.y + sin(_u0) * 7.0 * _kg) },
                                             (rde_vec_2F){ (f32)(_kn.x + cos(_u1) * 7.0 * _kg), (f32)(_kn.y + sin(_u1) * 7.0 * _kg) }, _look->tick, 1.2f);
            }
        }
        // Its angle, while it turns and a moment after.
        if(_ins->held == (i32)_k && _now - _ins->turned_at < 1.2 && _ins->turned_at > 0.0) {
            f64 _deg = fmod(_t->angle * 180.0 / FUDE_ZOOM_INSTRUMENT_PI + 360.0, 180.0);
            c8 _s[24];
            fude_zoom_instrument_number(round(_deg * 10.0) / 10.0, _s, sizeof(_s));
            strncat(_s, "\xC2\xB0", sizeof(_s) - strlen(_s) - 1u);
            const rde_vec_2F _c = fude_zoom_v2_f(_t->at);
            rde_rendering_2d_draw_rounded_rectangle_with_border(_c, (rde_vec_2F){ 64.0f, 26.0f }, 1.0f, 8u, _look->body, 1.0f, _look->accent, NULL);
            fude_zoom_instrument_text(_look, _s, _t->at, 13.0f, _look->text);
        }
        // A corner gauge's round, said under its plate (tapped: typed).
        if(_kind == FUDE_ZOOM_INSTRUMENT_CORNER && _look->units != NULL && _look->font != NULL) {
            c8 _v[FUDE_ZOOM_UNITS_TEXT], _said[FUDE_ZOOM_UNITS_TEXT + 8u];
            fude_zoom_units_format(_t->stuck_mm > 0.0 ? _t->stuck_mm : fude_zoom_corner_r(_t) * _look->mm_per_point, _look->units, _v, sizeof(_v));
            snprintf(_said, sizeof(_said), "R %s", _v);
            const fude_zoom_v2 _at = fude_zoom_instrument_compass_label(_ins, _k);
            const f32 _w = fude_draw_text_width(_look->font, _look->font_px, _said, 13.0f) + 16.0f;
            rde_rendering_2d_draw_rounded_rectangle_with_border(fude_zoom_v2_f(_at), (rde_vec_2F){ _w, 24.0f }, 1.0f, 8u, _look->body, 1.0f, _look->accent, NULL);
            fude_zoom_instrument_text(_look, _said, _at, 13.0f, _look->text);
        }
        fude_zoom_instrument_render_close(_ins, _k, _look);
        fude_zoom_instrument_render_grow(_ins, _k, _look);
    }
}
