// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/circuit_parts.h"

// ===========================================================================
// See circuit.h. Circuits, each part drawn (fude_zoom_part_draw): its lines, as symbol.h's parts, for symbol.c to
// draw, in its own u, v from -1 to 1 of its half sizes.
//
// The file is in two parts: the pen (fzc, a part being drawn: points, lines, rectangles, circles, arcs, arrowheads,
// leads and a few letters made of lines) with the bodies made with it (a diode's, a gate's, a sized display's); then
// the looks (FZC_LOOK_) in families, a function for each with a case for each look (fzc_draw_passive, _diode,
// _transistor, _source, _switch, _display, _mechanism, _package), and fude_zoom_part_draw, which asks them in turn.
//
// Part of the circuit: circuit_parts.h holds what the modules share and says which module holds what. Prefix fzc_ and
// FZC_. Main thread only, as symbol.c's drawing.
// ===========================================================================

// --- drawn -------------------------------------------------------------------------------------------

// A part being drawn: where its lines go (symbol.h's points and parts), its half sizes, how many segments a full circle
// takes, and the first point of the line being made.
typedef struct {
    rde_arr* points;     // TYPE(fude_zoom_v2): every line's points, one line after another
    rde_arr* parts;      // TYPE(fude_zoom_symbol_part): each line's first point, count and flags
    f64      hw, hh;     // its half sizes: u and v from -1 to 1 are these
    u32      segments;   // a full circle's segments (16 at least)
    u32      first;      // the line being made's first point (fzc_begin)
} fzc;

// Adds a point in the part's own units (x, y from its middle).
RDE_INTERNAL void fzc_pt(fzc* _d, f64 _x, f64 _y) {
    const fude_zoom_v2 _p = { _x, _y };
    rde_arr_add(_d->points, (any)&_p);
}
// Adds a point at (u, v) of the part's half sizes.
RDE_INTERNAL void fzc_uv(fzc* _d, f64 _u, f64 _v) {
    fzc_pt(_d, _u * _d->hw, _v * _d->hh);
}
// Begins a line: its points are those added from here.
RDE_INTERNAL void fzc_begin(fzc* _d) {
    _d->first = (u32)rde_arr_length(_d->points);
}
// Ends the line begun, with _flags (symbol.h's FUDE_ZOOM_SYMBOL_); a line of fewer than 2 points is dropped.
RDE_INTERNAL void fzc_end(fzc* _d, u8 _flags) {
    const u32 _n = (u32)rde_arr_length(_d->points) - _d->first;
    if(_n >= 2u) {
        const fude_zoom_symbol_part _p = { .first = _d->first, .count = _n, .flags = _flags };
        rde_arr_add(_d->parts, (any)&_p);
    } else {
        rde_arr_resize(_d->points, _d->first);
    }
}
// A straight line from (u0, v0) to (u1, v1).
RDE_INTERNAL void fzc_line(fzc* _d, f64 _u0, f64 _v0, f64 _u1, f64 _v1) {
    fzc_begin(_d);
    fzc_uv(_d, _u0, _v0);
    fzc_uv(_d, _u1, _v1);
    fzc_end(_d, 0u);
}
// A line through _n points (u, v pairs in _uv), with _flags.
RDE_INTERNAL void fzc_poly(fzc* _d, const f64* _uv, u32 _n, u8 _flags) {
    fzc_begin(_d);
    for(u32 _i = 0; _i < _n; _i++) {
        fzc_uv(_d, _uv[2u * _i], _uv[2u * _i + 1u]);
    }
    fzc_end(_d, _flags);
}
// A rectangle from corner (u0, v0) to corner (u1, v1), closed.
RDE_INTERNAL void fzc_rect(fzc* _d, f64 _u0, f64 _v0, f64 _u1, f64 _v1, u8 _flags) {
    const f64 _r[8] = { _u0, _v0, _u1, _v0, _u1, _v1, _u0, _v1 };
    fzc_poly(_d, _r, 4u, (u8)(_flags | FUDE_ZOOM_SYMBOL_CLOSED));
}
// A circle round (u, v) (its centre in the box's proportions), _r its own units round.
RDE_INTERNAL void fzc_circle(fzc* _d, f64 _u, f64 _v, f64 _r, u8 _flags) {
    fzc_begin(_d);
    const u32 _k = _d->segments < 16u ? 16u : _d->segments;
    for(u32 _i = 0; _i < _k; _i++) {
        const f64 _a = 2.0 * RDE_PI * (f64)_i / (f64)_k;
        fzc_pt(_d, _u * _d->hw + rde_math_cos_f64(_a) * _r, _v * _d->hh + rde_math_sin_f64(_a) * _r);
    }
    fzc_end(_d, (u8)(_flags | FUDE_ZOOM_SYMBOL_CLOSED));
}
// An arc's points (own units), the part being made.
RDE_INTERNAL void fzc_arc_pts(fzc* _d, f64 _cx, f64 _cy, f64 _rx, f64 _ry, f64 _a0, f64 _a1) {
    u32 _k = (u32)rde_math_ceil_f64((f64)(_d->segments < 16u ? 16u : _d->segments) * rde_math_abs_f64(_a1 - _a0) / (2.0 * RDE_PI));
    _k = _k < 3u ? 3u : _k;
    for(u32 _i = 0; _i <= _k; _i++) {
        const f64 _a = _a0 + (_a1 - _a0) * (f64)_i / (f64)_k;
        fzc_pt(_d, _cx + rde_math_cos_f64(_a) * _rx, _cy + rde_math_sin_f64(_a) * _ry);
    }
}
// A filled arrowhead at (x, y) (own units) pointing along (dx, dy).
RDE_INTERNAL void fzc_head(fzc* _d, f64 _x, f64 _y, f64 _dx, f64 _dy, f64 _len) {
    const f64 _l = rde_math_hypot_f64(_dx, _dy);
    if(!(_l > 0.0)) {
        return;
    }
    const f64 _ux = _dx / _l, _uy = _dy / _l;
    fzc_begin(_d);
    fzc_pt(_d, _x, _y);
    fzc_pt(_d, _x - _ux * _len - _uy * _len * 0.45, _y - _uy * _len + _ux * _len * 0.45);
    fzc_pt(_d, _x - _ux * _len + _uy * _len * 0.45, _y - _uy * _len - _ux * _len * 0.45);
    fzc_end(_d, FUDE_ZOOM_SYMBOL_CLOSED | FUDE_ZOOM_SYMBOL_SOLID);
}
// A line with a head at its end (u, v).
RDE_INTERNAL void fzc_arrow(fzc* _d, f64 _u0, f64 _v0, f64 _u1, f64 _v1, f64 _len) {
    fzc_line(_d, _u0, _v0, _u1, _v1);
    fzc_head(_d, _u1 * _d->hw, _v1 * _d->hh, (_u1 - _u0) * _d->hw, (_v1 - _v0) * _d->hh, _len);
}
// A part's pins' leads: from each pin in to (_to_u of the way, a left or right one's; _to_v, a top or bottom one's).
RDE_INTERNAL void fzc_stubs_to(fzc* _d, const fude_zoom_part* _p, f64 _to_u, f64 _to_v) {
    for(u32 _i = 0; _i < _p->pin_count; _i++) {
        const fude_zoom_pin _q = _p->pins[_i];
        if(_q.side == FUDE_ZOOM_PIN_LEFT || _q.side == FUDE_ZOOM_PIN_RIGHT) {
            fzc_line(_d, _q.u, _q.v, _q.u < 0.0f ? -_to_u : _to_u, _q.v);
        } else {
            fzc_line(_d, _q.u, _q.v, _q.u, _q.v > 0.0f ? _to_v : -_to_v);
        }
    }
}
// A part's pins' leads, both ways the same way in (fzc_stubs_to).
RDE_INTERNAL void fzc_stubs(fzc* _d, const fude_zoom_part* _p, f64 _to) {
    fzc_stubs_to(_d, _p, _to, _to);
}
// The letters of a few marks, as lines (u, v round (cu, cv), _s high): + − V A M.
RDE_INTERNAL void fzc_mark(fzc* _d, c8 _c, f64 _cu, f64 _cv, f64 _s) {
    const f64 _x = _cu * _d->hw, _y = _cv * _d->hh, _h = _s * 0.5;
    if(_c == '+' || _c == '-') {
        fzc_begin(_d); fzc_pt(_d, _x - _h, _y); fzc_pt(_d, _x + _h, _y); fzc_end(_d, 0u);
        if(_c == '+') {
            fzc_begin(_d); fzc_pt(_d, _x, _y - _h); fzc_pt(_d, _x, _y + _h); fzc_end(_d, 0u);
        }
    } else if(_c == 'V') {
        fzc_begin(_d); fzc_pt(_d, _x - _h * 0.8, _y + _h); fzc_pt(_d, _x, _y - _h); fzc_pt(_d, _x + _h * 0.8, _y + _h); fzc_end(_d, 0u);
    } else if(_c == 'A') {
        fzc_begin(_d); fzc_pt(_d, _x - _h * 0.8, _y - _h); fzc_pt(_d, _x, _y + _h); fzc_pt(_d, _x + _h * 0.8, _y - _h); fzc_end(_d, 0u);
        fzc_begin(_d); fzc_pt(_d, _x - _h * 0.45, _y - _h * 0.15); fzc_pt(_d, _x + _h * 0.45, _y - _h * 0.15); fzc_end(_d, 0u);
    } else if(_c == 'M') {
        fzc_begin(_d); fzc_pt(_d, _x - _h * 0.8, _y - _h); fzc_pt(_d, _x - _h * 0.8, _y + _h); fzc_pt(_d, _x, _y); fzc_pt(_d, _x + _h * 0.8, _y + _h);
        fzc_pt(_d, _x + _h * 0.8, _y - _h); fzc_end(_d, 0u);
    }
}

// A diode's triangle and bar (pointing right, its middle at u 0, ±_h high), solid.
RDE_INTERNAL void fzc_diode_body(fzc* _d, f64 _w, f64 _h) {
    const f64 _t[6] = { -_w, -_h, -_w, _h, _w, 0.0 };
    fzc_poly(_d, _t, 3u, FUDE_ZOOM_SYMBOL_CLOSED | FUDE_ZOOM_SYMBOL_SOLID);
    fzc_line(_d, _w, -_h, _w, _h);
    fzc_line(_d, -1.0, 0.0, -_w, 0.0);
    fzc_line(_d, _w, 0.0, 1.0, 0.0);
}

// A logic gate's body (inputs left, output right), its bubble when it inverts.
RDE_INTERNAL void fzc_gate(fzc* _d, u8 _gate) {
    const f64 _hw = _d->hw, _hh = _d->hh;
    const b8 _inv = _gate == FUDE_ZOOM_GATE_NOT || _gate == FUDE_ZOOM_GATE_NAND || _gate == FUDE_ZOOM_GATE_NOR || _gate == FUDE_ZOOM_GATE_XNOR;
    const f64 _bubble = 0.09 * _hw;
    const f64 _out = 0.62 * _hw;   // where the body ends (its tip)
    fzc_begin(_d);
    if(_gate == FUDE_ZOOM_GATE_NOT || _gate == FUDE_ZOOM_GATE_BUFFER) {
        fzc_pt(_d, -0.55 * _hw, -0.8 * _hh); fzc_pt(_d, -0.55 * _hw, 0.8 * _hh); fzc_pt(_d, _out - (_inv ? 2.0 * _bubble : 0.0), 0.0);
    } else if(_gate == FUDE_ZOOM_GATE_AND || _gate == FUDE_ZOOM_GATE_NAND) {
        const f64 _x0 = -0.55 * _hw, _r = 0.85 * _hh, _cx = _out - (_inv ? 2.0 * _bubble : 0.0) - _r;
        fzc_pt(_d, _x0, -_r); fzc_pt(_d, _x0, _r);
        fzc_arc_pts(_d, _cx, 0.0, _r, _r, RDE_PI * 0.5, -RDE_PI * 0.5);
    } else {
        // OR, NOR, XOR, XNOR: a curved back, pointed nose.
        const f64 _x0 = -0.55 * _hw, _tip = _out - (_inv ? 2.0 * _bubble : 0.0), _h = 0.85 * _hh;
        const u32 _k = 16u;
        fzc_pt(_d, _x0, -_h);
        for(u32 _i = 0; _i <= _k; _i++) {   // lower curve to the tip
            const f64 _t = (f64)_i / (f64)_k;
            fzc_pt(_d, _x0 + (_tip - _x0) * _t, -_h * (1.0 - _t * _t));
        }
        for(u32 _i = 1; _i <= _k; _i++) {   // upper curve back
            const f64 _t = 1.0 - (f64)_i / (f64)_k;
            fzc_pt(_d, _x0 + (_tip - _x0) * _t, _h * (1.0 - _t * _t));
        }
        for(u32 _i = 1; _i < _k; _i++) {   // the back, curving in
            const f64 _t = (f64)_i / (f64)_k;
            fzc_pt(_d, _x0 + 0.18 * _hw * rde_math_sin_f64(_t * RDE_PI), _h - 2.0 * _h * _t);
        }
    }
    fzc_end(_d, FUDE_ZOOM_SYMBOL_CLOSED | FUDE_ZOOM_SYMBOL_FILLED);
    if(_gate == FUDE_ZOOM_GATE_XOR || _gate == FUDE_ZOOM_GATE_XNOR) {
        fzc_begin(_d);
        for(u32 _i = 0; _i <= 16u; _i++) {
            const f64 _t = (f64)_i / 16.0;
            fzc_pt(_d, -0.7 * _hw + 0.18 * _hw * rde_math_sin_f64(_t * RDE_PI), 0.85 * _hh - 1.7 * _hh * _t);
        }
        fzc_end(_d, 0u);
    }
    if(_inv) {
        fzc_circle(_d, (_out - _bubble) / _hw, 0.0, _bubble, 0u);
    }
    // Leads.
    const b8 _one = _gate == FUDE_ZOOM_GATE_NOT || _gate == FUDE_ZOOM_GATE_BUFFER;
    if(_one) {
        fzc_line(_d, -1.0, 0.0, -0.55, 0.0);
    } else {
        const f64 _in = _gate == FUDE_ZOOM_GATE_AND || _gate == FUDE_ZOOM_GATE_NAND ? -0.55 : -0.47;
        fzc_line(_d, -1.0, 2.0 / 3.0, _in, 2.0 / 3.0);
        fzc_line(_d, -1.0, -2.0 / 3.0, _in, -2.0 / 3.0);
    }
    fzc_line(_d, _out / _hw, 0.0, 1.0, 0.0);
}

// A sized display (display.h), laid out in its room (its size's: the catalogue's units) — its leads 10 long; a panel's
// digits, a matrix's dots, an LCD's glass where its characters are.
RDE_INTERNAL void fzc_display(fzc* _d, const fude_zoom_part* _part) {
    const u8 F = FUDE_ZOOM_SYMBOL_FILLED, C = FUDE_ZOOM_SYMBOL_CLOSED;
    f64 _w = 2.0 * _d->hw, _h = 2.0 * _d->hh;
    fude_zoom_display_room(_part, &_w, &_h);
    const f64 _rw = _w * 0.5, _rh = _h * 0.5;
    const f64 _iu = 1.0 - 10.0 / _rw, _iv = 1.0 - 10.0 / _rh;
    const u32 _cols = fude_zoom_display_cols(_part), _rows = fude_zoom_display_rows(_part);
    fzc_stubs_to(_d, _part, _iu, _iv);
    if(_part->look == FZC_CHAR_LCD) {
        // Its board under its pins; its glass round its characters (18 × 28 each, from 46 under its top).
        fzc_rect(_d, -1.0, -1.0, 1.0, _iv, F);
        const f64 _sx = -9.0 * (f64)_cols, _sy = _rh - 46.0;
        fzc_rect(_d, (_sx - 10.0) / _rw, (_sy + 10.0) / _rh, (10.0 - _sx) / _rw, (_sy - 28.0 * (f64)_rows - 10.0) / _rh, 0u);
        return;
    }
    fzc_rect(_d, -_iu, -_iv, 1.0, 1.0, F);
    if(_part->look == FZC_SEG_PANEL) {
        // Each digit's segments (32 wide, 84 high, 60 apart over its common), its point.
        static const f64 _seg[7][4] = { { -0.5, 1, 0.5, 1 }, { 0.5, 1, 0.5, 0 }, { 0.5, 0, 0.5, -1 }, { -0.5, -1, 0.5, -1 },
                                        { -0.5, 0, -0.5, -1 }, { -0.5, 1, -0.5, 0 }, { -0.5, 0, 0.5, 0 } };
        for(u32 _k = 0; _k < _cols; _k++) {
            const f64 _cx = -_rw + 80.0 + 60.0 * (f64)_k, _cy = 5.0;
            for(u32 _g = 0; _g < 7u; _g++) {
                fzc_line(_d, (_cx + _seg[_g][0] * 32.0) / _rw, (_cy + _seg[_g][1] * 42.0) / _rh, (_cx + _seg[_g][2] * 32.0) / _rw, (_cy + _seg[_g][3] * 42.0) / _rh);
            }
            fzc_circle(_d, (_cx + 24.0) / _rw, (_cy - 42.0) / _rh, 3.0 * _d->hw / _rw, 0u);
        }
        return;
    }
    // A matrix: a ring for each dot (an octagon, 6 round), 20 apart.
    for(u32 _r = 0; _r < _rows; _r++) {
        for(u32 _c = 0; _c < _cols; _c++) {
            const f64 _cx = -_rw + 40.0 + 20.0 * (f64)_c, _cy = _rh - 20.0 - 20.0 * (f64)_r;
            f64 _uv[16];
            for(u32 _k = 0; _k < 8u; _k++) {
                const f64 _a = RDE_PI * 0.25 * (f64)_k + RDE_PI / 8.0;
                _uv[2u * _k]      = (_cx + 6.0 * rde_math_cos_f64(_a)) / _rw;
                _uv[2u * _k + 1u] = (_cy + 6.0 * rde_math_sin_f64(_a)) / _rh;
            }
            fzc_poly(_d, _uv, 8u, C);
        }
    }
}

// Draws a resistor, a pot, a fuse, a capacitor (an electrolytic's curved plate and its +), an inductor, an LDR or a
// thermistor (a resistor marked), or a transformer's two windings over their core. False: _part is none of them.
RDE_INTERNAL b8 fzc_draw_passive(fzc* _d, const fude_zoom_part* _part, f64 _m) {
    const u8 F = FUDE_ZOOM_SYMBOL_FILLED, S = FUDE_ZOOM_SYMBOL_SOLID;
    switch(_part->look) {
    case FZC_RESISTOR:
        fzc_rect(_d, -0.6, -0.7, 0.6, 0.7, F);
        fzc_line(_d, -1.0, 0.0, -0.6, 0.0);
        fzc_line(_d, 0.6, 0.0, 1.0, 0.0);
        break;
    case FZC_POT:
        fzc_rect(_d, -0.6, -0.85, 0.6, -0.15, F);
        fzc_line(_d, -1.0, -0.5, -0.6, -0.5);
        fzc_line(_d, 0.6, -0.5, 1.0, -0.5);
        fzc_arrow(_d, 0.0, 1.0, 0.0, -0.1, 0.25 * _d->hh);
        break;
    case FZC_FUSE:
        fzc_rect(_d, -0.6, -0.6, 0.6, 0.6, F);
        fzc_line(_d, -1.0, 0.0, 1.0, 0.0);
        break;
    case FZC_CAP:
    case FZC_CAP_POL:
        fzc_line(_d, -0.15, -0.8, -0.15, 0.8);
        if(_part->look == FZC_CAP) {
            fzc_line(_d, 0.15, -0.8, 0.15, 0.8);
        } else {
            fzc_begin(_d);
            fzc_arc_pts(_d, 0.75 * _d->hw, 0.0, 0.55 * _d->hw, 0.8 * _d->hh, RDE_PI * 0.72, RDE_PI * 1.28);
            fzc_end(_d, 0u);
            fzc_mark(_d, '+', -0.45, 0.6, 0.2 * _m);
        }
        fzc_line(_d, -1.0, 0.0, -0.15, 0.0);
        fzc_line(_d, _part->look == FZC_CAP ? 0.15 : 0.2, 0.0, 1.0, 0.0);
        break;
    case FZC_INDUCTOR: {
        fzc_begin(_d);
        fzc_uv(_d, -1.0, 0.0);
        for(u32 _k = 0; _k < 4u; _k++) {
            const f64 _c = (-0.6 + 0.15 + 0.3 * (f64)_k) * _d->hw;
            fzc_arc_pts(_d, _c, 0.0, 0.15 * _d->hw, 0.8 * _d->hh, RDE_PI, 0.0);
        }
        fzc_uv(_d, 1.0, 0.0);
        fzc_end(_d, 0u);
        break;
    }
    case FZC_LDR:
    case FZC_NTC:
        // A resistor's body, its leads; light's two arrows on it (an LDR), or a line across it bent at its foot (an
        // NTC).
        fzc_rect(_d, -0.6, -0.35, 0.6, 0.35, F);
        fzc_line(_d, -1.0, 0.0, -0.6, 0.0);
        fzc_line(_d, 0.6, 0.0, 1.0, 0.0);
        if(_part->look == FZC_LDR) {
            fzc_arrow(_d, -0.55, 1.0, -0.2, 0.45, 0.25 * _d->hh);
            fzc_arrow(_d, -0.05, 1.0, 0.3, 0.45, 0.25 * _d->hh);
        } else {
            fzc_line(_d, -0.75, -0.75, -0.45, -0.75);
            fzc_line(_d, -0.45, -0.75, 0.6, 0.75);
        }
        break;
    case FZC_XFMR: {
        // Two windings facing over two core lines, their dots at the top (in phase), their leads.
        for(u32 _k = 0; _k < 2u; _k++) {
            const f64 _sg = _k == 0u ? -1.0 : 1.0, _cu = 0.38 * _sg;
            fzc_begin(_d);
            for(u32 _t = 0; _t < 4u; _t++) {
                const f64 _cv = (0.375 - 0.25 * (f64)_t) * _d->hh;
                fzc_arc_pts(_d, _cu * _d->hw, _cv, 0.18 * _d->hw, 0.125 * _d->hh, 0.5 * RDE_PI, _k == 0u ? 1.5 * RDE_PI : -0.5 * RDE_PI);
            }
            fzc_end(_d, 0u);
            fzc_line(_d, _sg * 1.0, 0.5, _cu, 0.5);
            fzc_line(_d, _sg * 1.0, -0.5, _cu, -0.5);
            fzc_circle(_d, _cu + _sg * 0.28, 0.62, 0.04 * _d->hw, S);
        }
        fzc_line(_d, -0.08, -0.75, -0.08, 0.75);
        fzc_line(_d, 0.08, -0.75, 0.08, 0.75);
        break;
    }
    default:
        return false;
    }
    return true;
}

// Draws what is made of diodes: a diode, a zener, an LED, a thyristor (an SCR, a TRIAC), a bridge rectifier, a
// photodiode. False: _part is none of them.
RDE_INTERNAL b8 fzc_draw_diode(fzc* _d, const fude_zoom_part* _part, f64 _m) {
    const u8 C = FUDE_ZOOM_SYMBOL_CLOSED, S = FUDE_ZOOM_SYMBOL_SOLID;
    switch(_part->look) {
    case FZC_DIODE:
        fzc_diode_body(_d, 0.3, 0.7);
        break;
    case FZC_ZENER:
        fzc_diode_body(_d, 0.3, 0.7);
        fzc_line(_d, 0.3, 0.7, 0.45, 0.9);
        fzc_line(_d, 0.3, -0.7, 0.15, -0.9);
        break;
    case FZC_LED:
        fzc_diode_body(_d, 0.25, 0.45);
        fzc_arrow(_d, 0.0, 0.5, 0.3, 0.95, 0.12 * _d->hw);
        fzc_arrow(_d, 0.25, 0.4, 0.55, 0.85, 0.12 * _d->hw);
        break;
    case FZC_SCR:
        // A diode, its gate from beside its bar down to its pin.
        fzc_diode_body(_d, 0.3, 0.7);
        fzc_line(_d, 0.3, -0.35, 1.0 / 3.0, -1.0);
        break;
    case FZC_TRIAC: {
        // Two diodes either way between two bars, its gate from the second down.
        fzc_line(_d, -0.3, -0.8, -0.3, 0.8);
        fzc_line(_d, 0.3, -0.8, 0.3, 0.8);
        const f64 _up[6] = { -0.3, 0.05, -0.3, 0.75, 0.3, 0.4 }, _dn[6] = { 0.3, -0.05, 0.3, -0.75, -0.3, -0.4 };
        fzc_poly(_d, _up, 3u, C | S);
        fzc_poly(_d, _dn, 3u, C | S);
        fzc_line(_d, -1.0, 0.0, -0.3, 0.0);
        fzc_line(_d, 0.3, 0.0, 1.0, 0.0);
        fzc_line(_d, 0.3, -0.5, 1.0 / 3.0, -1.0);
        break;
    }
    case FZC_BRIDGE: {
        // Its diamond, a diode on each side: from either AC corner into +, out of − into either.
        const f64 _r = 0.6;
        const f64 _c[4][2] = { { 0.0, _r }, { _r, 0.0 }, { 0.0, -_r }, { -_r, 0.0 } };   // (AC1, +, AC2, −)
        const u32 _ab[4][2] = { { 0u, 1u }, { 2u, 1u }, { 3u, 0u }, { 3u, 2u } };
        for(u32 _k = 0; _k < 4u; _k++) {
            const f64 _ax = _c[_ab[_k][0]][0], _ay = _c[_ab[_k][0]][1], _bx = _c[_ab[_k][1]][0], _by = _c[_ab[_k][1]][1];
            fzc_line(_d, _ax, _ay, _bx, _by);
            const f64 _mx = 0.5 * (_ax + _bx) * _d->hw, _my = 0.5 * (_ay + _by) * _d->hh;
            fzc_head(_d, _mx + (_bx - _ax) * 0.12 * _d->hw, _my + (_by - _ay) * 0.12 * _d->hh, (_bx - _ax) * _d->hw, (_by - _ay) * _d->hh, 0.22 * _m);
            const f64 _px = _mx + (_bx - _ax) * 0.12 * _d->hw, _py = _my + (_by - _ay) * 0.12 * _d->hh, _l = rde_math_hypot_f64((_bx - _ax) * _d->hw, (_by - _ay) * _d->hh);
            const f64 _nx = -(_by - _ay) * _d->hh / _l, _ny = (_bx - _ax) * _d->hw / _l;
            fzc_begin(_d); fzc_pt(_d, _px - _nx * 0.12 * _m, _py - _ny * 0.12 * _m); fzc_pt(_d, _px + _nx * 0.12 * _m, _py + _ny * 0.12 * _m); fzc_end(_d, 0u);
        }
        fzc_line(_d, 0.0, 1.0, 0.0, _r);
        fzc_line(_d, 0.0, -1.0, 0.0, -_r);
        fzc_line(_d, 1.0, 0.0, _r, 0.0);
        fzc_line(_d, -1.0, 0.0, -_r, 0.0);
        fzc_mark(_d, '+', 0.75, 0.25, 0.18 * _m);
        fzc_mark(_d, '-', -0.75, 0.25, 0.18 * _m);
        break;
    }
    case FZC_PHOTODIODE:
        // A diode, light's two arrows into it.
        fzc_diode_body(_d, 0.3, 0.7);
        fzc_arrow(_d, -0.5, 1.0, -0.2, 0.55, 0.12 * _d->hw);
        fzc_arrow(_d, -0.15, 1.0, 0.15, 0.55, 0.12 * _d->hw);
        break;
    default:
        return false;
    }
    return true;
}

// Draws a transistor (bipolar or MOSFET, either way), a phototransistor, or an optocoupler (its LED and its
// phototransistor in one case). False: _part is none of them.
RDE_INTERNAL b8 fzc_draw_transistor(fzc* _d, const fude_zoom_part* _part, f64 _m) {
    const u8 F = FUDE_ZOOM_SYMBOL_FILLED, C = FUDE_ZOOM_SYMBOL_CLOSED, S = FUDE_ZOOM_SYMBOL_SOLID;
    switch(_part->look) {
    case FZC_NPN:
    case FZC_PNP: {
        const f64 _r = 0.8 * _m, _bu = -0.25, _xu = 1.0 / 3.0;
        fzc_circle(_d, 0.05, 0.0, _r, F);
        fzc_line(_d, -1.0, 0.0, _bu, 0.0);
        fzc_line(_d, _bu, -0.5, _bu, 0.5);
        const b8 _p = _part->look == FZC_PNP;
        const f64 _ce = _p ? -1.0 : 1.0;   // (the collector's way: up for NPN, down for PNP)
        fzc_begin(_d); fzc_uv(_d, _bu, 0.25 * _ce); fzc_uv(_d, _xu, 0.65 * _ce); fzc_uv(_d, _xu, _ce); fzc_end(_d, 0u);
        fzc_begin(_d); fzc_uv(_d, _bu, -0.25 * _ce); fzc_uv(_d, _xu, -0.65 * _ce); fzc_uv(_d, _xu, -_ce); fzc_end(_d, 0u);
        const f64 _ex0 = _bu * _d->hw, _ey0 = -0.25 * _ce * _d->hh, _ex1 = _xu * _d->hw, _ey1 = -0.65 * _ce * _d->hh;
        if(!_p) {
            fzc_head(_d, _ex0 + (_ex1 - _ex0) * 0.85, _ey0 + (_ey1 - _ey0) * 0.85, _ex1 - _ex0, _ey1 - _ey0, 0.22 * _m);   // (out of the emitter)
        } else {
            fzc_head(_d, _ex0 + (_ex1 - _ex0) * 0.25, _ey0 + (_ey1 - _ey0) * 0.25, _ex0 - _ex1, _ey0 - _ey1, 0.22 * _m);   // (into the base)
        }
        break;
    }
    case FZC_NMOS:
    case FZC_PMOS: {
        const f64 _r = 0.8 * _m, _xu = 1.0 / 3.0;
        const b8 _p = _part->look == FZC_PMOS;
        const f64 _dv = _p ? -1.0 : 1.0;   // (the drain's way)
        fzc_circle(_d, 0.05, 0.0, _r, F);
        fzc_line(_d, -1.0, 0.0, -0.4, 0.0);
        fzc_line(_d, -0.4, -0.5, -0.4, 0.5);
        fzc_line(_d, -0.25, 0.3, -0.25, 0.6);
        fzc_line(_d, -0.25, -0.15, -0.25, 0.15);
        fzc_line(_d, -0.25, -0.6, -0.25, -0.3);
        fzc_begin(_d); fzc_uv(_d, -0.25, 0.45 * _dv); fzc_uv(_d, _xu, 0.45 * _dv); fzc_uv(_d, _xu, _dv); fzc_end(_d, 0u);
        fzc_begin(_d); fzc_uv(_d, -0.25, -0.45 * _dv); fzc_uv(_d, _xu, -0.45 * _dv); fzc_uv(_d, _xu, -_dv); fzc_end(_d, 0u);
        fzc_line(_d, -0.25, 0.0, _xu, 0.0);
        fzc_line(_d, _xu, 0.0, _xu, -0.45 * _dv);
        if(!_p) {
            fzc_head(_d, -0.22 * _d->hw, 0.0, -1.0, 0.0, 0.2 * _m);
        } else {
            fzc_head(_d, 0.2 * _d->hw, 0.0, 1.0, 0.0, 0.2 * _m);
        }
        break;
    }
    case FZC_OPTO: {
        // Its case; its LED (A over K) down its left, its light across to its phototransistor (C over E) down its
        // right.
        fzc_rect(_d, -0.8, -0.9, 0.8, 0.9, F);
        const f64 _t[6] = { -0.55, 0.25, -0.3, 0.25, -0.425, -0.1 };
        fzc_poly(_d, _t, 3u, C | S);
        fzc_line(_d, -0.55, -0.1, -0.3, -0.1);
        fzc_line(_d, -0.425, 0.25, -0.425, 1.0 / 3.0);
        fzc_line(_d, -0.425, -0.1, -0.425, -1.0 / 3.0);
        fzc_line(_d, -0.8, 1.0 / 3.0, -0.425, 1.0 / 3.0);
        fzc_line(_d, -0.8, -1.0 / 3.0, -0.425, -1.0 / 3.0);
        fzc_arrow(_d, -0.15, 0.15, 0.12, 0.15, 0.1 * _d->hw);
        fzc_arrow(_d, -0.15, -0.05, 0.12, -0.05, 0.1 * _d->hw);
        fzc_line(_d, 0.3, -0.35, 0.3, 0.35);
        fzc_line(_d, 0.3, 0.15, 0.55, 1.0 / 3.0);
        fzc_line(_d, 0.3, -0.15, 0.55, -1.0 / 3.0);
        fzc_line(_d, 0.55, 1.0 / 3.0, 0.8, 1.0 / 3.0);
        fzc_line(_d, 0.55, -1.0 / 3.0, 0.8, -1.0 / 3.0);
        fzc_stubs(_d, _part, 0.8);
        break;
    }
    case FZC_PHOTOTRANS: {
        // A transistor's circle, its base a bar with no lead — light's arrows on it.
        const f64 _r = 0.8 * _m, _bu = -0.25, _xu = 1.0 / 3.0;
        fzc_circle(_d, 0.05, 0.0, _r, F);
        fzc_line(_d, _bu, -0.5, _bu, 0.5);
        fzc_begin(_d); fzc_uv(_d, _bu, 0.25); fzc_uv(_d, _xu, 0.65); fzc_uv(_d, _xu, 1.0); fzc_end(_d, 0u);
        fzc_begin(_d); fzc_uv(_d, _bu, -0.25); fzc_uv(_d, _xu, -0.65); fzc_uv(_d, _xu, -1.0); fzc_end(_d, 0u);
        fzc_head(_d, (_bu + (_xu - _bu) * 0.85) * _d->hw, -(0.25 + 0.4 * 0.85) * _d->hh, (_xu - _bu) * _d->hw, -0.4 * _d->hh, 0.22 * _m);
        fzc_arrow(_d, -0.95, 0.75, -0.4, 0.3, 0.12 * _d->hw);
        fzc_arrow(_d, -0.95, 0.35, -0.4, -0.1, 0.12 * _d->hw);
        break;
    }
    default:
        return false;
    }
    return true;
}

// Draws a source or a meter in its circle (a DC, AC or current source, a clock, a lamp, a motor, a voltmeter, an
// ammeter), a battery, ground, or a supply rail. False: _part is none of them.
RDE_INTERNAL b8 fzc_draw_source(fzc* _d, const fude_zoom_part* _part, f64 _m) {
    const u8 F = FUDE_ZOOM_SYMBOL_FILLED;
    switch(_part->look) {
    case FZC_VSOURCE:
    case FZC_ACSOURCE:
    case FZC_CLOCK:
    case FZC_ISOURCE:
    case FZC_LAMP:
    case FZC_MOTOR:
    case FZC_VOLTMETER:
    case FZC_AMMETER: {
        const b8 _upright = _part->pins[0].side == FUDE_ZOOM_PIN_UP;
        const f64 _r = 0.85 * (_upright ? rde_math_min_f64(_d->hw, _d->hh * 0.6) : rde_math_min_f64(_d->hw * 0.6, _d->hh));
        fzc_circle(_d, 0.0, 0.0, _r, F);
        if(_upright) {
            fzc_line(_d, 0.0, 1.0, 0.0, _r / _d->hh);
            fzc_line(_d, 0.0, -_r / _d->hh, 0.0, -1.0);
        } else {
            fzc_line(_d, -1.0, 0.0, -_r / _d->hw, 0.0);
            fzc_line(_d, _r / _d->hw, 0.0, 1.0, 0.0);
        }
        const f64 _ru = _r / _d->hw, _rv = _r / _d->hh;
        if(_part->look == FZC_VSOURCE) {
            fzc_mark(_d, '+', 0.0, 0.45 * _rv, 0.45 * _r);
            fzc_mark(_d, '-', 0.0, -0.45 * _rv, 0.45 * _r);
        } else if(_part->look == FZC_ACSOURCE) {
            fzc_begin(_d);
            for(u32 _i = 0; _i <= 24u; _i++) {
                const f64 _t = (f64)_i / 24.0;
                fzc_pt(_d, (-0.6 + 1.2 * _t) * _r, 0.35 * _r * rde_math_sin_f64(_t * 2.0 * RDE_PI));
            }
            fzc_end(_d, 0u);
        } else if(_part->look == FZC_CLOCK) {
            const f64 _sq[12] = { -0.6 * _ru, -0.3 * _rv, -0.3 * _ru, -0.3 * _rv, -0.3 * _ru, 0.3 * _rv, 0.3 * _ru, 0.3 * _rv, 0.3 * _ru, -0.3 * _rv, 0.6 * _ru, -0.3 * _rv };
            fzc_poly(_d, _sq, 6u, 0u);
        } else if(_part->look == FZC_ISOURCE) {
            fzc_arrow(_d, 0.0, -0.5 * _rv, 0.0, 0.6 * _rv, 0.3 * _r);
        } else if(_part->look == FZC_LAMP) {
            const f64 _k = 0.7071 * _r;
            fzc_begin(_d); fzc_pt(_d, -_k, -_k); fzc_pt(_d, _k, _k); fzc_end(_d, 0u);
            fzc_begin(_d); fzc_pt(_d, -_k, _k); fzc_pt(_d, _k, -_k); fzc_end(_d, 0u);
        } else {
            fzc_mark(_d, _part->look == FZC_MOTOR ? 'M' : (_part->look == FZC_VOLTMETER ? 'V' : 'A'), 0.0, 0.0, 0.9 * _r);
        }
        break;
    }
    case FZC_BATTERY:
        fzc_line(_d, -0.75, 0.35, 0.75, 0.35);
        fzc_line(_d, -0.35, 0.12, 0.35, 0.12);
        fzc_line(_d, -0.75, -0.12, 0.75, -0.12);
        fzc_line(_d, -0.35, -0.35, 0.35, -0.35);
        fzc_line(_d, 0.0, 1.0, 0.0, 0.35);
        fzc_line(_d, 0.0, -0.35, 0.0, -1.0);
        fzc_mark(_d, '+', 0.6, 0.65, 0.25 * _m);
        break;
    case FZC_GROUND:
        fzc_line(_d, -0.85, 0.2, 0.85, 0.2);
        fzc_line(_d, 0.0, 1.0, 0.0, 0.2);
        fzc_line(_d, -0.5, -0.25, 0.5, -0.25);
        fzc_line(_d, -0.18, -0.7, 0.18, -0.7);
        break;
    case FZC_RAIL: {
        const f64 _a[6] = { -0.7, 0.1, 0.0, 0.9, 0.7, 0.1 };
        fzc_poly(_d, _a, 3u, 0u);
        fzc_line(_d, 0.0, -1.0, 0.0, 0.9);
        break;
    }
    default:
        return false;
    }
    return true;
}

// Draws a switch, a push button, a changeover switch (one pole or two), a buzzer or a speaker. False: _part is none
// of them.
RDE_INTERNAL b8 fzc_draw_switch(fzc* _d, const fude_zoom_part* _part) {
    const u8 F = FUDE_ZOOM_SYMBOL_FILLED, C = FUDE_ZOOM_SYMBOL_CLOSED, S = FUDE_ZOOM_SYMBOL_SOLID;
    switch(_part->look) {
    case FZC_SWITCH:
        fzc_line(_d, -0.5, 0.0, 0.45, 0.65);
        fzc_line(_d, -1.0, 0.0, -0.5, 0.0);
        fzc_line(_d, 0.5, 0.0, 1.0, 0.0);
        fzc_circle(_d, -0.5, 0.0, 0.06 * _d->hw, S);
        fzc_circle(_d, 0.5, 0.0, 0.06 * _d->hw, 0u);
        break;
    case FZC_BUTTON:
        fzc_line(_d, -0.6, 0.3, 0.6, 0.3);
        fzc_line(_d, 0.0, 0.3, 0.0, 0.85);
        fzc_line(_d, -0.25, 0.85, 0.25, 0.85);
        fzc_line(_d, -1.0, 0.0, -0.5, 0.0);
        fzc_line(_d, 0.5, 0.0, 1.0, 0.0);
        fzc_circle(_d, -0.5, 0.0, 0.06 * _d->hw, 0u);
        fzc_circle(_d, 0.5, 0.0, 0.06 * _d->hw, 0u);
        break;
    case FZC_BUZZER: {
        fzc_begin(_d);
        fzc_arc_pts(_d, 0.0, -0.2 * _d->hh, 0.55 * _d->hw, 0.95 * _d->hh, 0.0, RDE_PI);
        fzc_end(_d, C | F);
        fzc_line(_d, -1.0, -0.5, -0.3, -0.5);
        fzc_line(_d, -0.3, -0.5, -0.3, -0.2);
        fzc_line(_d, 1.0, -0.5, 0.3, -0.5);
        fzc_line(_d, 0.3, -0.5, 0.3, -0.2);
        break;
    }
    case FZC_SPDT:
    case FZC_DPDT: {
        // Each pole: its common's lead to its pivot, its lever resting on A, A's and B's contacts and leads; a DPDT's
        // two levers worked together (a dashed link).
        const u32 _poles = _part->look == FZC_DPDT ? 2u : 1u;
        for(u32 _k = 0; _k < _poles; _k++) {
            const f64 _vc = _poles == 2u ? (_k == 0u ? 2.0 / 3.0 : -1.0 / 3.0) : 0.0;
            const f64 _va = _poles == 2u ? _vc : 2.0 / 3.0, _vb = _poles == 2u ? _vc - 1.0 / 3.0 : -2.0 / 3.0;
            fzc_line(_d, -1.0, _vc, -0.5, _vc);
            fzc_line(_d, 0.5, _va, 1.0, _va);
            fzc_line(_d, 0.5, _vb, 1.0, _vb);
            fzc_circle(_d, -0.5, _vc, 0.06 * _d->hw, S);
            fzc_circle(_d, 0.5, _va, 0.06 * _d->hw, 0u);
            fzc_circle(_d, 0.5, _vb, 0.06 * _d->hw, 0u);
            fzc_line(_d, -0.5, _vc, 0.42, _va + (_poles == 2u ? 0.08 : -0.06));
        }
        if(_poles == 2u) {
            fzc_begin(_d);
            fzc_uv(_d, 0.0, 2.0 / 3.0 + 0.04);
            fzc_uv(_d, 0.0, -1.0 / 3.0 + 0.04);
            fzc_end(_d, FUDE_ZOOM_SYMBOL_DASHED);
        }
        break;
    }
    case FZC_SPEAKER: {
        // Its magnet (its leads into it), its cone opening right.
        fzc_rect(_d, -0.55, -0.6, -0.15, 0.6, F);
        const f64 _cone[8] = { -0.15, -0.45, 0.55, -0.95, 0.55, 0.95, -0.15, 0.45 };
        fzc_poly(_d, _cone, 4u, C | F);
        fzc_line(_d, -1.0, 0.5, -0.55, 0.5);
        fzc_line(_d, -1.0, -0.5, -0.55, -0.5);
        break;
    }
    default:
        return false;
    }
    return true;
}

// Draws a display: a 7-segment digit, a sized display (a panel of digits, an LED matrix, a character LCD), a bar
// graph, an oscilloscope, a panel meter. False: _part is none of them.
RDE_INTERNAL b8 fzc_draw_display(fzc* _d, const fude_zoom_part* _part, f64 _m) {
    const u8 F = FUDE_ZOOM_SYMBOL_FILLED, S = FUDE_ZOOM_SYMBOL_SOLID;
    switch(_part->look) {
    case FZC_SEVEN_SEG: {
        fzc_rect(_d, -0.7, -0.75, 0.7, 0.95, F);
        // Its segments (a top, b top right, c bottom right, d bottom, e bottom left, f top left, g middle) as lines.
        const f64 _x0 = -0.3, _x1 = 0.3, _y0 = -0.5, _ym = 0.15, _y1 = 0.8;
        fzc_line(_d, _x0, _y1, _x1, _y1);
        fzc_line(_d, _x1, _y1, _x1, _ym);
        fzc_line(_d, _x1, _ym, _x1, _y0);
        fzc_line(_d, _x0, _y0, _x1, _y0);
        fzc_line(_d, _x0, _ym, _x0, _y0);
        fzc_line(_d, _x0, _y1, _x0, _ym);
        fzc_line(_d, _x0, _ym, _x1, _ym);
        fzc_circle(_d, 0.45, -0.5, 0.04 * _d->hw, S);
        fzc_stubs(_d, _part, 0.7);
        fzc_line(_d, 0.0, -1.0, 0.0, -0.75);
        break;
    }
    case FZC_SEG_PANEL:
    case FZC_MATRIX:
    case FZC_CHAR_LCD:
        fzc_display(_d, _part);
        break;
    case FZC_BAR_GRAPH:
        // Its body, its ten bars down it, its leads.
        fzc_rect(_d, -5.0 / 7.0, -1.0, 5.0 / 7.0, 1.0, F);
        for(u32 _k = 0; _k < 10u; _k++) {
            const f64 _v = 1.0 - 2.0 * (f64)(_k + 1u) / 11.0;
            fzc_rect(_d, -0.28, _v - 0.06, 0.28, _v + 0.06, 0u);
        }
        fzc_stubs(_d, _part, 5.0 / 7.0);
        break;
    case FZC_SCOPE: {
        // Its case, its screen (what it shows drawn in it as it plays), its knobs (rings: their pointers drawn as it
        // plays, as far round as each is set — display.h), its leads.
        fzc_rect(_d, -0.85, -1.0, 1.0, 1.0, F);
        f64 _u0, _v0, _u1, _v1;
        fude_zoom_scope_screen(&_u0, &_v0, &_u1, &_v1);
        fzc_rect(_d, _u0, _v0, _u1, _v1, 0u);
        fzc_line(_d, _u0, 0.5 * (_v0 + _v1), _u1, 0.5 * (_v0 + _v1));
        fzc_line(_d, 0.5 * (_u0 + _u1), _v0, 0.5 * (_u0 + _u1), _v1);
        for(u32 _k = 0; _k < FUDE_ZOOM_SCOPE_KNOBS; _k++) {
            f64 _ku, _kv, _kr;
            fude_zoom_scope_knob(_k, &_ku, &_kv, &_kr);
            fzc_circle(_d, _ku, _kv, _kr * _d->hh, 0u);
        }
        fzc_stubs(_d, _part, 0.85);
        break;
    }
    case FZC_METER:
        // Its case, its LCD's window (its digits drawn in it as it reads: display.h), + and − by its leads.
        fzc_rect(_d, -0.85, -1.0, 0.85, 1.0, F);
        fzc_rect(_d, -0.78, -0.62, 0.78, 0.62, 0u);
        fzc_stubs(_d, _part, 0.85);
        fzc_mark(_d, '+', -0.93, 0.45, 0.22 * _m);
        fzc_mark(_d, '-', 0.93, 0.45, 0.22 * _m);
        break;
    default:
        return false;
    }
    return true;
}

// Draws a part worked by, or working, a mechanism: a solenoid (its plunger what moves), a stepper motor, a rotary
// encoder, a slotted optical sensor. False: _part is none of them.
RDE_INTERNAL b8 fzc_draw_mechanism(fzc* _d, const fude_zoom_part* _part, f64 _m) {
    const u8 F = FUDE_ZOOM_SYMBOL_FILLED, C = FUDE_ZOOM_SYMBOL_CLOSED, S = FUDE_ZOOM_SYMBOL_SOLID;
    switch(_part->look) {
    case FZC_SOLENOID: {
        // Its coil's case (its windings over and under its bore), its leads; its plunger in its bore and out of its
        // right end — what moves as it plays (render.h's moved) —, a hole at its end.
        fzc_rect(_d, -0.8, -1.0, 0.2, 1.0, F);
        fzc_rect(_d, -0.72, -0.34, 0.2, 0.34, 0u);
        for(u32 _k = 0; _k < 5u; _k++) {
            const f64 _u = -0.68 + 0.17 * (f64)_k;
            fzc_line(_d, _u, 0.45, _u + 0.12, 0.9);
            fzc_line(_d, _u, -0.9, _u + 0.12, -0.45);
        }
        fzc_line(_d, -1.0, 0.5, -0.8, 0.5);
        fzc_line(_d, -1.0, -0.5, -0.8, -0.5);
        const f64 _tip = FUDE_ZOOM_SOLENOID_TIP, _r = 0.16 * _d->hh;
        fzc_rect(_d, -0.35, -0.24, _tip - _r / _d->hw, 0.24, F | FUDE_ZOOM_SYMBOL_MOVES);
        fzc_circle(_d, _tip, 0.0, 0.3 * _d->hh, F | FUDE_ZOOM_SYMBOL_MOVES);
        fzc_circle(_d, _tip, 0.0, _r * 0.6, FUDE_ZOOM_SYMBOL_MOVES);
        break;
    }
    case FZC_STEPPER:
        // Its face (its pins' names down its left), its rotor's boss, its shaft in its middle (what is pinned there it
        // turns: coupling.h).
        fzc_rect(_d, -0.8, -0.9, 0.8, 0.9, F);
        fzc_circle(_d, 0.0, 0.0, 0.5 * _d->hh, 0u);
        fzc_circle(_d, 0.0, 0.0, 0.12 * _d->hh, S);
        fzc_stubs(_d, _part, 0.8);
        fzc_mark(_d, 'M', 0.5, -0.65, 0.25 * _m);
        break;
    case FZC_ENCODER:
        // Its body, its shaft's knob in its middle, a notch on it; its leads.
        fzc_rect(_d, -0.7, -0.8, 0.7, 0.8, F);
        fzc_circle(_d, 0.0, 0.0, 0.32 * _m, 0u);
        fzc_line(_d, 0.0, 0.08, 0.0, 0.3);
        fzc_stubs(_d, _part, 0.7);
        break;
    case FZC_SLOT: {
        // Its U: two arms on a base, its slot between them (open at its top); its LED's beam across the slot.
        const f64 _u[16] = { -0.8, -1.0, 0.8, -1.0, 0.8, 1.0, 0.2, 1.0, 0.2, -0.4, -0.2, -0.4, -0.2, 1.0, -0.8, 1.0 };
        fzc_poly(_d, _u, 8u, C | F);
        fzc_begin(_d);
        fzc_uv(_d, -0.2, 0.3);
        fzc_uv(_d, 0.12, 0.3);
        fzc_end(_d, FUDE_ZOOM_SYMBOL_DASHED);
        fzc_head(_d, 0.2 * _d->hw, 0.3 * _d->hh, 1.0, 0.0, 0.1 * _d->hw);
        fzc_stubs(_d, _part, 0.8);
        break;
    }
    default:
        return false;
    }
    return true;
}

// Draws a part in a package or a body of its own: an op-amp, a logic gate, a flip-flop, a logic input or probe, a
// chip, a module, a custom part's block, a regulator, a board, a breadboard. False: _part is none of them.
RDE_INTERNAL b8 fzc_draw_package(fzc* _d, const fude_zoom_part* _part, f64 _m) {
    const u8 F = FUDE_ZOOM_SYMBOL_FILLED, C = FUDE_ZOOM_SYMBOL_CLOSED;
    switch(_part->look) {
    case FZC_OPAMP: {
        const f64 _t[6] = { -0.6, -0.9, -0.6, 0.9, 0.75, 0.0 };
        fzc_poly(_d, _t, 3u, C | F);
        fzc_line(_d, -1.0, 0.5, -0.6, 0.5);
        fzc_line(_d, -1.0, -0.5, -0.6, -0.5);
        fzc_line(_d, 0.75, 0.0, 1.0, 0.0);
        fzc_line(_d, 0.0, 1.0, 0.0, 0.9 * 0.75 / 1.35);
        fzc_line(_d, 0.0, -1.0, 0.0, -0.9 * 0.75 / 1.35);
        fzc_mark(_d, '-', -0.45, 0.5, 0.15 * _m);
        fzc_mark(_d, '+', -0.45, -0.5, 0.15 * _m);
        break;
    }
    case FZC_GATE:
        fzc_gate(_d, _part->gate);
        break;
    case FZC_FLIPFLOP:
        fzc_rect(_d, -0.6, -0.9, 0.6, 0.9, F);
        fzc_stubs(_d, _part, 0.6);
        for(u32 _k = 0; _k < _part->pin_count; _k++) {
            if(_part->pins[_k].name != NULL && rde_strcmp(_part->pins[_k].name, "CLK") == 0) {
                // (the clock's notch, at its pin: none on a latch)
                const f64 _cv = (f64)_part->pins[_k].v;
                fzc_begin(_d); fzc_uv(_d, -0.6, _cv + 0.15); fzc_uv(_d, -0.42, _cv); fzc_uv(_d, -0.6, _cv - 0.15); fzc_end(_d, 0u);
            }
        }
        break;
    case FZC_LOGIC_IN:
        fzc_rect(_d, -0.8, -0.7, 0.55, 0.7, F);
        fzc_line(_d, 0.55, 0.0, 1.0, 0.0);
        fzc_rect(_d, -0.55, -0.3, 0.3, 0.3, 0u);   // (its toggle)
        break;
    case FZC_LOGIC_OUT:
        fzc_circle(_d, 0.2, 0.0, 0.6 * _m, F);
        fzc_line(_d, -1.0, 0.0, 0.2 - 0.6 * _m / _d->hw, 0.0);
        break;
    case FZC_CHIP:
        fzc_rect(_d, -0.6, -1.0, 0.6, 1.0, F);
        fzc_stubs(_d, _part, 0.6);
        fzc_begin(_d);   // (its notch, pin 1's end)
        fzc_arc_pts(_d, 0.0, _d->hh, 0.12 * _d->hw, 0.12 * _d->hw, RDE_PI, 2.0 * RDE_PI);
        fzc_end(_d, 0u);
        break;
    case FZC_MODULE:
        fzc_rect(_d, -0.65, -1.0, 1.0, 1.0, F);
        fzc_stubs(_d, _part, 0.65);
        break;
    case FZC_BLOCK: {
        // (a custom part: a block, its pins' stubs out of it; above and below too where it has pins there)
        b8 _up = false, _down = false;
        for(u32 _k = 0; _k < _part->pin_count; _k++) {
            _up   = _up || _part->pins[_k].side == FUDE_ZOOM_PIN_UP;
            _down = _down || _part->pins[_k].side == FUDE_ZOOM_PIN_DOWN;
        }
        fzc_rect(_d, -0.75, _down ? -0.75 : -1.0, 0.75, _up ? 0.75 : 1.0, F);
        fzc_stubs(_d, _part, 0.75);
        break;
    }
    case FZC_REG3:
        fzc_rect(_d, -0.55, -0.45, 0.55, 1.0, F);
        fzc_stubs(_d, _part, 0.55);
        fzc_line(_d, 0.0, -1.0, 0.0, -0.45);
        break;
    case FZC_BOARD: {
            const f64 _r = 0.06 * rde_math_min_f64(_d->hw, _d->hh);
            fzc_begin(_d);
            fzc_arc_pts(_d, 0.8 * _d->hw - _r, -_d->hh + _r, _r, _r, -RDE_PI * 0.5, 0.0);
            fzc_arc_pts(_d, 0.8 * _d->hw - _r, _d->hh - _r, _r, _r, 0.0, RDE_PI * 0.5);
            fzc_arc_pts(_d, -0.8 * _d->hw + _r, _d->hh - _r, _r, _r, RDE_PI * 0.5, RDE_PI);
            fzc_arc_pts(_d, -0.8 * _d->hw + _r, -_d->hh + _r, _r, _r, RDE_PI, RDE_PI * 1.5);
            fzc_end(_d, C | F);
        }
        fzc_stubs(_d, _part, 0.8);
        break;
    case FZC_BREADBOARD: {
        fzc_rect(_d, -1.0, -1.0, 1.0, 1.0, F);
        // Its rails' lines (+ and −, along), the gap down its middle, and its holes (each a short dash).
        const f64 _x0 = -1.0 + 2.0 * 1.0 / (f64)FZC_BB_W, _x1 = 1.0 - 2.0 * 1.0 / (f64)FZC_BB_W;
        const f32 _rail_y[4] = { 0.4f, 2.6f, 15.9f, 18.1f };
        for(u32 _k = 0; _k < 4u; _k++) {
            const f64 _v = 1.0 - 2.0 * (f64)_rail_y[_k] / (f64)FZC_BB_H;
            fzc_line(_d, _x0, _v, _x1, _v);
        }
        const f64 _gap = 1.0 - 2.0 * 9.25 / (f64)FZC_BB_H;
        fzc_rect(_d, _x0, _gap - 0.6 / (f64)FZC_BB_H, _x1, _gap + 0.6 / (f64)FZC_BB_H, 0u);
        const f64 _dash = 0.16 * 2.0 / (f64)FZC_BB_W;
        for(u32 _i = 0; _i < (u32)_part->pin_count; _i++) {
            fude_zoom_pin _q;
            fude_zoom_part_pin(_part, _i, &_q);
            fzc_line(_d, _q.u - _dash, _q.v, _q.u + _dash, _q.v);
        }
        fzc_mark(_d, '+', -1.0 + 2.0 * 0.6 / (f64)FZC_BB_W, 1.0 - 2.0 * 1.0 / (f64)FZC_BB_H, 0.5 * 2.0 * _d->hh / (f64)FZC_BB_H);
        fzc_mark(_d, '-', -1.0 + 2.0 * 0.6 / (f64)FZC_BB_W, 1.0 - 2.0 * 2.0 / (f64)FZC_BB_H, 0.5 * 2.0 * _d->hh / (f64)FZC_BB_H);
        break;
    }
    default:
        return false;
    }
    return true;
}

// See the header. Its circles take _segments, held from 16 to 128. Each look is drawn by its family's function
// (fzc_draw_passive and the others above), its body and leads in its own u, v.
u32 fude_zoom_part_draw(const fude_zoom_part* _part, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points, rde_arr* _parts) {
    rde_arr_clear(_points);
    rde_arr_clear(_parts);
    if(_part == NULL) {
        return 0;
    }
    fzc _d = { _points, _parts, rde_math_abs_f64(_hw), rde_math_abs_f64(_hh), _segments < 16u ? 16u : (_segments > 128u ? 128u : _segments), 0u };
    const f64 _m = rde_math_min_f64(_d.hw, _d.hh);   // A round part's room.
    const u8 F = FUDE_ZOOM_SYMBOL_FILLED;
    // Each look is its family's (fzc_draw_*): a look in none of them is drawn as a box.
    if(!fzc_draw_passive(&_d, _part, _m) && !fzc_draw_diode(&_d, _part, _m) && !fzc_draw_transistor(&_d, _part, _m) &&
       !fzc_draw_source(&_d, _part, _m) && !fzc_draw_switch(&_d, _part) && !fzc_draw_display(&_d, _part, _m) &&
       !fzc_draw_mechanism(&_d, _part, _m) && !fzc_draw_package(&_d, _part, _m)) {
        fzc_rect(&_d, -1.0, -1.0, 1.0, 1.0, F);
    }
    return (u32)rde_arr_length(_parts);
}
