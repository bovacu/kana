// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/symbol.h"
#include "zoom/scene.h"
#include "zoom/circuit.h"
#include "zoom/mech.h"
#include "zoom/plan.h"

#include <math.h>
#include <string.h>

// ===========================================================================
// See symbol.h. Each symbol is drawn by a function of its half sizes (hw, hh):
// its parts, in its own units round its middle (Y up). Most are drawn in the
// box's own proportions (u, v from -1 to 1, as the box is); an icon's round
// bits are round whatever its box (a radius of the smaller half).
// ===========================================================================

#define FUDE_ZOOM_SYMBOL_PI 3.141592653589793

typedef struct {
    rde_arr* points;
    rde_arr* parts;
    f64      hw, hh;
    u32      segments;
    u32      first;
} fude_zoom_sym;

RDE_INTERNAL void fude_zoom_sym_pt(fude_zoom_sym* _s, f64 _x, f64 _y) {
    const fude_zoom_v2 _p = { _x, _y };
    rde_arr_add(_s->points, (any)&_p);
}

// In the box's proportions: u, v from -1 to 1.
RDE_INTERNAL void fude_zoom_sym_uv(fude_zoom_sym* _s, f64 _u, f64 _v) {
    fude_zoom_sym_pt(_s, _u * _s->hw, _v * _s->hh);
}

RDE_INTERNAL void fude_zoom_sym_begin(fude_zoom_sym* _s) {
    _s->first = (u32)rde_arr_length(_s->points);
}

RDE_INTERNAL void fude_zoom_sym_end(fude_zoom_sym* _s, u8 _flags) {
    const u32 _n = (u32)rde_arr_length(_s->points) - _s->first;
    if(_n >= 2u || ((_flags & FUDE_ZOOM_SYMBOL_SOLID) && _n >= 1u)) {
        const fude_zoom_symbol_part _p = { _s->first, _n, _flags };
        rde_arr_add(_s->parts, (any)&_p);
    } else {
        _s->points->count -= _n;   // (too few points: none)
    }
}

// A polygon or line in u, v (pairs).
RDE_INTERNAL void fude_zoom_sym_poly(fude_zoom_sym* _s, const f64* _uv, u32 _n, u8 _flags) {
    fude_zoom_sym_begin(_s);
    for(u32 _i = 0; _i < _n; _i++) {
        fude_zoom_sym_uv(_s, _uv[2u * _i], _uv[2u * _i + 1u]);
    }
    fude_zoom_sym_end(_s, _flags);
}

RDE_INTERNAL void fude_zoom_sym_line(fude_zoom_sym* _s, f64 _u0, f64 _v0, f64 _u1, f64 _v1, u8 _flags) {
    const f64 _l[4] = { _u0, _v0, _u1, _v1 };
    fude_zoom_sym_poly(_s, _l, 2u, _flags);
}

RDE_INTERNAL void fude_zoom_sym_rect(fude_zoom_sym* _s, f64 _u0, f64 _v0, f64 _u1, f64 _v1, u8 _flags) {
    const f64 _r[8] = { _u0, _v0, _u1, _v0, _u1, _v1, _u0, _v1 };
    fude_zoom_sym_poly(_s, _r, 4u, (u8)(_flags | FUDE_ZOOM_SYMBOL_CLOSED));
}

// An arc of the ellipse round (cx, cy) (own units) _rx by _ry, from angle _a0 to _a1 (radians), its points
// added to the part being made (its ends included).
RDE_INTERNAL void fude_zoom_sym_arc_pts(fude_zoom_sym* _s, f64 _cx, f64 _cy, f64 _rx, f64 _ry, f64 _a0, f64 _a1) {
    u32 _k = (u32)ceil((f64)_s->segments * fabs(_a1 - _a0) / (2.0 * FUDE_ZOOM_SYMBOL_PI));
    _k = _k < 2u ? 2u : _k;
    for(u32 _i = 0; _i <= _k; _i++) {
        const f64 _a = _a0 + (_a1 - _a0) * (f64)_i / (f64)_k;
        fude_zoom_sym_pt(_s, _cx + cos(_a) * _rx, _cy + sin(_a) * _ry);
    }
}

// An ellipse (own units); a whole one closed.
RDE_INTERNAL void fude_zoom_sym_ellipse(fude_zoom_sym* _s, f64 _cx, f64 _cy, f64 _rx, f64 _ry, u8 _flags) {
    fude_zoom_sym_begin(_s);
    const u32 _k = _s->segments < 12u ? 12u : _s->segments;
    for(u32 _i = 0; _i < _k; _i++) {
        const f64 _a = 2.0 * FUDE_ZOOM_SYMBOL_PI * (f64)_i / (f64)_k;
        fude_zoom_sym_pt(_s, _cx + cos(_a) * _rx, _cy + sin(_a) * _ry);
    }
    fude_zoom_sym_end(_s, (u8)(_flags | FUDE_ZOOM_SYMBOL_CLOSED));
}

// A rounded rectangle (own units), its corners _r round.
RDE_INTERNAL void fude_zoom_sym_rounded(fude_zoom_sym* _s, f64 _x0, f64 _y0, f64 _x1, f64 _y1, f64 _r, u8 _flags) {
    _r = fmin(_r, fmin(fabs(_x1 - _x0), fabs(_y1 - _y0)) * 0.5);
    fude_zoom_sym_begin(_s);
    const f64 _h = FUDE_ZOOM_SYMBOL_PI * 0.5;
    fude_zoom_sym_arc_pts(_s, _x1 - _r, _y0 + _r, _r, _r, -_h, 0.0);
    fude_zoom_sym_arc_pts(_s, _x1 - _r, _y1 - _r, _r, _r, 0.0, _h);
    fude_zoom_sym_arc_pts(_s, _x0 + _r, _y1 - _r, _r, _r, _h, 2.0 * _h);
    fude_zoom_sym_arc_pts(_s, _x0 + _r, _y0 + _r, _r, _r, 2.0 * _h, 3.0 * _h);
    fude_zoom_sym_end(_s, (u8)(_flags | FUDE_ZOOM_SYMBOL_CLOSED));
}

// A wave from (x0, y) to (x1, y) (own units), _a high, _n half waves.
RDE_INTERNAL void fude_zoom_sym_wave_pts(fude_zoom_sym* _s, f64 _x0, f64 _x1, f64 _y, f64 _a, u32 _n) {
    const u32 _k = 12u * _n;
    for(u32 _i = 0; _i <= _k; _i++) {
        const f64 _t = (f64)_i / (f64)_k;
        fude_zoom_sym_pt(_s, _x0 + (_x1 - _x0) * _t, _y + _a * sin(_t * FUDE_ZOOM_SYMBOL_PI * (f64)_n));
    }
}

// An arrowhead at (x, y) (own units) pointing along (dx, dy), _len long: filled.
RDE_INTERNAL void fude_zoom_sym_head(fude_zoom_sym* _s, f64 _x, f64 _y, f64 _dx, f64 _dy, f64 _len) {
    const f64 _l = hypot(_dx, _dy);
    if(!(_l > 0.0)) {
        return;
    }
    const f64 _ux = _dx / _l, _uy = _dy / _l;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, _x, _y);
    fude_zoom_sym_pt(_s, _x - _ux * _len - _uy * _len * 0.5, _y - _uy * _len + _ux * _len * 0.5);
    fude_zoom_sym_pt(_s, _x - _ux * _len + _uy * _len * 0.5, _y - _uy * _len - _ux * _len * 0.5);
    fude_zoom_sym_end(_s, FUDE_ZOOM_SYMBOL_CLOSED | FUDE_ZOOM_SYMBOL_SOLID);
}

// A line with a head at its end (own units).
RDE_INTERNAL void fude_zoom_sym_arrow(fude_zoom_sym* _s, f64 _x0, f64 _y0, f64 _x1, f64 _y1, f64 _len) {
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, _x0, _y0);
    fude_zoom_sym_pt(_s, _x1, _y1);
    fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_head(_s, _x1, _y1, _x1 - _x0, _y1 - _y0, _len);
}

// A regular polygon round the middle, _n sides, the first corner at angle _a.
RDE_INTERNAL void fude_zoom_sym_regular(fude_zoom_sym* _s, u32 _n, f64 _a, u8 _flags) {
    fude_zoom_sym_begin(_s);
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _t = _a + 2.0 * FUDE_ZOOM_SYMBOL_PI * (f64)_i / (f64)_n;
        fude_zoom_sym_uv(_s, cos(_t), sin(_t));
    }
    fude_zoom_sym_end(_s, (u8)(_flags | FUDE_ZOOM_SYMBOL_CLOSED));
}

#define FZS_OUT  (FUDE_ZOOM_SYMBOL_CLOSED | FUDE_ZOOM_SYMBOL_FILLED)   // an outline: closed, filled as the shape is
#define FZS_LOOP FUDE_ZOOM_SYMBOL_CLOSED
#define FZS_SOLID (FUDE_ZOOM_SYMBOL_CLOSED | FUDE_ZOOM_SYMBOL_SOLID)

// --- the catalogue ---------------------------------------------------------------------------------

typedef void (*fude_zoom_sym_draw)(fude_zoom_sym* _s);

typedef struct {
    fude_zoom_symbol_info info;
    fude_zoom_sym_draw    draw;
} fude_zoom_sym_entry;

// The smaller half (an icon's round bits stay round).
#define FZS_R(_s) fmin((_s)->hw, (_s)->hh)

// Flowchart.
RDE_INTERNAL void fzs_process(fude_zoom_sym* _s)    { fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT); }
RDE_INTERNAL void fzs_rounded(fude_zoom_sym* _s)    { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.35, FZS_OUT); }
RDE_INTERNAL void fzs_decision(fude_zoom_sym* _s)   { const f64 _d[8] = { 0, -1, 1, 0, 0, 1, -1, 0 }; fude_zoom_sym_poly(_s, _d, 4u, FZS_OUT); }
RDE_INTERNAL void fzs_terminator(fude_zoom_sym* _s) { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, _s->hh, FZS_OUT); }
RDE_INTERNAL void fzs_data(fude_zoom_sym* _s)       { const f64 _d[8] = { -1, -1, 0.75, -1, 1, 1, -0.75, 1 }; fude_zoom_sym_poly(_s, _d, 4u, FZS_OUT); }
RDE_INTERNAL void fzs_predefined(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    fude_zoom_sym_line(_s, -0.82, -1, -0.82, 1, 0u);
    fude_zoom_sym_line(_s, 0.82, -1, 0.82, 1, 0u);
}
RDE_INTERNAL void fzs_document(fude_zoom_sym* _s) {
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_uv(_s, -1, 1);
    fude_zoom_sym_wave_pts(_s, -_s->hw, _s->hw, -_s->hh * 0.82, -_s->hh * 0.16, 2u);
    fude_zoom_sym_uv(_s, 1, 1);
    fude_zoom_sym_end(_s, FZS_OUT);
}
RDE_INTERNAL void fzs_documents(fude_zoom_sym* _s) {
    // The front one (its outline: what fills and connects), the two behind it showing above and right of it.
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_uv(_s, -1, 0.76);
    fude_zoom_sym_wave_pts(_s, -_s->hw, 0.76 * _s->hw, -_s->hh * 0.84, -_s->hh * 0.14, 2u);
    fude_zoom_sym_uv(_s, 0.76, 0.76);
    fude_zoom_sym_end(_s, FZS_OUT);
    for(u32 _k = 1; _k <= 2u; _k++) {
        const f64 _o = 0.12 * (f64)_k;
        const f64 _l[6] = { -1.0 + _o, 0.76 + _o, 0.76 + _o, 0.76 + _o, 0.76 + _o, -0.7 + _o };
        fude_zoom_sym_poly(_s, _l, 3u, 0u);
    }
}

RDE_INTERNAL void fzs_manual_input(fude_zoom_sym* _s) { const f64 _d[8] = { -1, -1, 1, -1, 1, 1, -1, 0.45 }; fude_zoom_sym_poly(_s, _d, 4u, FZS_OUT); }
RDE_INTERNAL void fzs_manual_op(fude_zoom_sym* _s)    { const f64 _d[8] = { -0.75, -1, 0.75, -1, 1, 1, -1, 1 }; fude_zoom_sym_poly(_s, _d, 4u, FZS_OUT); }
RDE_INTERNAL void fzs_preparation(fude_zoom_sym* _s)  { const f64 _d[12] = { -1, 0, -0.72, -1, 0.72, -1, 1, 0, 0.72, 1, -0.72, 1 }; fude_zoom_sym_poly(_s, _d, 6u, FZS_OUT); }
RDE_INTERNAL void fzs_delay(fude_zoom_sym* _s) {
    fude_zoom_sym_begin(_s);
    const f64 _r = _s->hh;
    fude_zoom_sym_uv(_s, -1, -1);
    fude_zoom_sym_arc_pts(_s, _s->hw - _r, 0.0, _r, _r, -FUDE_ZOOM_SYMBOL_PI * 0.5, FUDE_ZOOM_SYMBOL_PI * 0.5);
    fude_zoom_sym_uv(_s, -1, 1);
    fude_zoom_sym_end(_s, FZS_OUT);
}
RDE_INTERNAL void fzs_stored_data(fude_zoom_sym* _s) {
    // Curved both sides: its left a half round out, its right a half round in.
    const f64 _rx = _s->hw * 0.18;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, -_s->hw + _rx, 0.0, _rx, _s->hh, FUDE_ZOOM_SYMBOL_PI * 0.5, FUDE_ZOOM_SYMBOL_PI * 1.5);
    fude_zoom_sym_arc_pts(_s, _s->hw, 0.0, _rx, _s->hh, FUDE_ZOOM_SYMBOL_PI * 1.5, FUDE_ZOOM_SYMBOL_PI * 0.5);
    fude_zoom_sym_end(_s, FZS_OUT);
}
RDE_INTERNAL void fzs_internal_storage(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    fude_zoom_sym_line(_s, -0.78, -1, -0.78, 1, 0u);
    fude_zoom_sym_line(_s, -1, 0.7, 1, 0.7, 0u);
}
RDE_INTERNAL void fzs_display(fude_zoom_sym* _s) {
    fude_zoom_sym_begin(_s);
    const f64 _r = _s->hh;
    fude_zoom_sym_uv(_s, -1, 0);
    fude_zoom_sym_uv(_s, -0.7, -1);
    fude_zoom_sym_arc_pts(_s, _s->hw - _r * 0.6, 0.0, _r * 0.6, _r, -FUDE_ZOOM_SYMBOL_PI * 0.5, FUDE_ZOOM_SYMBOL_PI * 0.5);
    fude_zoom_sym_uv(_s, -0.7, 1);
    fude_zoom_sym_end(_s, FZS_OUT);
}
RDE_INTERNAL void fzs_offpage(fude_zoom_sym* _s)   { const f64 _d[10] = { -1, 1, 1, 1, 1, -0.3, 0, -1, -1, -0.3 }; fude_zoom_sym_poly(_s, _d, 5u, FZS_OUT); }
RDE_INTERNAL void fzs_connector(fude_zoom_sym* _s) { fude_zoom_sym_ellipse(_s, 0, 0, _s->hw, _s->hh, FZS_OUT); }
RDE_INTERNAL void fzs_merge(fude_zoom_sym* _s)     { const f64 _d[6] = { -1, 1, 1, 1, 0, -1 }; fude_zoom_sym_poly(_s, _d, 3u, FZS_OUT); }
RDE_INTERNAL void fzs_extract(fude_zoom_sym* _s)   { const f64 _d[6] = { -1, -1, 1, -1, 0, 1 }; fude_zoom_sym_poly(_s, _d, 3u, FZS_OUT); }
RDE_INTERNAL void fzs_or(fude_zoom_sym* _s) {
    fude_zoom_sym_ellipse(_s, 0, 0, _s->hw, _s->hh, FZS_OUT);
    fude_zoom_sym_line(_s, -1, 0, 1, 0, 0u);
    fude_zoom_sym_line(_s, 0, -1, 0, 1, 0u);
}
RDE_INTERNAL void fzs_summing(fude_zoom_sym* _s) {
    fude_zoom_sym_ellipse(_s, 0, 0, _s->hw, _s->hh, FZS_OUT);
    fude_zoom_sym_line(_s, -0.7071, -0.7071, 0.7071, 0.7071, 0u);
    fude_zoom_sym_line(_s, -0.7071, 0.7071, 0.7071, -0.7071, 0u);
}
RDE_INTERNAL void fzs_collate(fude_zoom_sym* _s)   { const f64 _d[8] = { -1, 1, 1, 1, -1, -1, 1, -1 }; fude_zoom_sym_poly(_s, _d, 4u, FZS_OUT); }
RDE_INTERNAL void fzs_sort(fude_zoom_sym* _s) {
    const f64 _d[8] = { 0, -1, 1, 0, 0, 1, -1, 0 };
    fude_zoom_sym_poly(_s, _d, 4u, FZS_OUT);
    fude_zoom_sym_line(_s, -1, 0, 1, 0, 0u);
}
RDE_INTERNAL void fzs_annotation(fude_zoom_sym* _s) { const f64 _d[8] = { -0.8, 1, -1, 1, -1, -1, -0.8, -1 }; fude_zoom_sym_poly(_s, _d, 4u, 0u); }
RDE_INTERNAL void fzs_card(fude_zoom_sym* _s)       { const f64 _d[10] = { -1, 0.55, -0.7, 1, 1, 1, 1, -1, -1, -1 }; fude_zoom_sym_poly(_s, _d, 5u, FZS_OUT); }
RDE_INTERNAL void fzs_tape(fude_zoom_sym* _s) {
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_wave_pts(_s, -_s->hw, _s->hw, -_s->hh * 0.85, -_s->hh * 0.15, 2u);
    fude_zoom_sym_wave_pts(_s, _s->hw, -_s->hw, _s->hh * 0.85, -_s->hh * 0.15, 2u);
    fude_zoom_sym_end(_s, FZS_OUT);
}
RDE_INTERNAL void fzs_loop_limit(fude_zoom_sym* _s) { const f64 _d[12] = { -1, -1, 1, -1, 1, 0.5, 0.75, 1, -0.75, 1, -1, 0.5 }; fude_zoom_sym_poly(_s, _d, 6u, FZS_OUT); }
RDE_INTERNAL void fzs_triangle(fude_zoom_sym* _s)   { const f64 _d[6] = { -1, -1, 1, -1, 0, 1 }; fude_zoom_sym_poly(_s, _d, 3u, FZS_OUT); }
RDE_INTERNAL void fzs_hexagon(fude_zoom_sym* _s)    { fude_zoom_sym_regular(_s, 6u, 0.0, FZS_OUT); }
RDE_INTERNAL void fzs_ellipse(fude_zoom_sym* _s)    { fude_zoom_sym_ellipse(_s, 0, 0, _s->hw, _s->hh, FZS_OUT); }
RDE_INTERNAL void fzs_callout(fude_zoom_sym* _s) {
    const f64 _d[14] = { -1, -0.45, -1, 1, 1, 1, 1, -0.45, -0.2, -0.45, -0.55, -1, -0.5, -0.45 };
    fude_zoom_sym_poly(_s, _d, 7u, FZS_OUT);
}

// UML structure.
RDE_INTERNAL void fzs_class(fude_zoom_sym* _s) { fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT); }   // (its compartments' lines: from its text, page/render)
RDE_INTERNAL void fzs_interface(fude_zoom_sym* _s) {
    // A lollipop: a circle on a stalk (its name under it).
    const f64 _r = FZS_R(_s) * 0.5;
    fude_zoom_sym_ellipse(_s, 0, _s->hh - _r, _r, _r, FZS_OUT);
    fude_zoom_sym_line(_s, 0, (_s->hh - 2.0 * _r) / _s->hh, 0, -1, 0u);
}
RDE_INTERNAL void fzs_package(fude_zoom_sym* _s) {
    const f64 _d[14] = { -1, -1, 1, -1, 1, 0.7, -0.35, 0.7, -0.35, 1, -1, 1, -1, 0.7 };
    fude_zoom_sym_poly(_s, _d, 6u, FZS_OUT);
    fude_zoom_sym_line(_s, -1, 0.7, -0.35, 0.7, 0u);
}
RDE_INTERNAL void fzs_note(fude_zoom_sym* _s) {
    const f64 _f = FZS_R(_s) * 0.3;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_s->hw, -_s->hh);
    fude_zoom_sym_pt(_s, _s->hw, -_s->hh);
    fude_zoom_sym_pt(_s, _s->hw, _s->hh - _f);
    fude_zoom_sym_pt(_s, _s->hw - _f, _s->hh);
    fude_zoom_sym_pt(_s, -_s->hw, _s->hh);
    fude_zoom_sym_end(_s, FZS_OUT);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, _s->hw - _f, _s->hh);
    fude_zoom_sym_pt(_s, _s->hw - _f, _s->hh - _f);
    fude_zoom_sym_pt(_s, _s->hw, _s->hh - _f);
    fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_component(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    // Its mark, top right: a box with two tabs on its left.
    const f64 _r = FZS_R(_s) * 0.18, _x = _s->hw - _r * 2.2, _y = _s->hh - _r * 2.0;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, _x - _r, _y - _r * 1.2); fude_zoom_sym_pt(_s, _x + _r, _y - _r * 1.2);
    fude_zoom_sym_pt(_s, _x + _r, _y + _r * 1.2); fude_zoom_sym_pt(_s, _x - _r, _y + _r * 1.2);
    fude_zoom_sym_end(_s, FZS_LOOP);
    for(i32 _k = -1; _k <= 1; _k += 2) {
        fude_zoom_sym_begin(_s);
        const f64 _ty = _y + (f64)_k * _r * 0.55;
        fude_zoom_sym_pt(_s, _x - _r * 1.5, _ty - _r * 0.28); fude_zoom_sym_pt(_s, _x - _r * 0.5, _ty - _r * 0.28);
        fude_zoom_sym_pt(_s, _x - _r * 0.5, _ty + _r * 0.28); fude_zoom_sym_pt(_s, _x - _r * 1.5, _ty + _r * 0.28);
        fude_zoom_sym_end(_s, FZS_LOOP | FUDE_ZOOM_SYMBOL_FILLED);
    }
}
RDE_INTERNAL void fzs_node(fude_zoom_sym* _s) {
    // A box seen from a corner: its front, its top, its side.
    const f64 _d = FZS_R(_s) * 0.25;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_s->hw, -_s->hh); fude_zoom_sym_pt(_s, _s->hw - _d, -_s->hh); fude_zoom_sym_pt(_s, _s->hw, -_s->hh + _d);
    fude_zoom_sym_pt(_s, _s->hw, _s->hh); fude_zoom_sym_pt(_s, -_s->hw + _d, _s->hh); fude_zoom_sym_pt(_s, -_s->hw, _s->hh - _d);
    fude_zoom_sym_end(_s, FZS_OUT);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_s->hw, _s->hh - _d); fude_zoom_sym_pt(_s, _s->hw - _d, _s->hh - _d); fude_zoom_sym_pt(_s, _s->hw, _s->hh);
    fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, _s->hw - _d, _s->hh - _d); fude_zoom_sym_pt(_s, _s->hw - _d, -_s->hh);
    fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_artifact(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    const f64 _r = FZS_R(_s) * 0.16, _x = _s->hw - _r * 2.0, _y = _s->hh - _r * 2.4, _f = _r * 0.6;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, _x - _r, _y - _r * 1.3); fude_zoom_sym_pt(_s, _x + _r, _y - _r * 1.3); fude_zoom_sym_pt(_s, _x + _r, _y + _r * 1.3 - _f);
    fude_zoom_sym_pt(_s, _x + _r - _f, _y + _r * 1.3); fude_zoom_sym_pt(_s, _x - _r, _y + _r * 1.3);
    fude_zoom_sym_end(_s, FZS_LOOP);
}
RDE_INTERNAL void fzs_actor(fude_zoom_sym* _s) {
    // A stick figure filling its box, its name under it.
    const f64 _h = _s->hh, _w = _s->hw, _head = fmin(_w * 0.42, _h * 0.2);
    fude_zoom_sym_ellipse(_s, 0, _h - _head, _head, _head, FZS_LOOP);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, 0, _h - 2.0 * _head); fude_zoom_sym_pt(_s, 0, -_h * 0.25); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_w, _h * 0.32); fude_zoom_sym_pt(_s, _w, _h * 0.32); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_w * 0.85, -_h); fude_zoom_sym_pt(_s, 0, -_h * 0.25); fude_zoom_sym_pt(_s, _w * 0.85, -_h); fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_use_case(fude_zoom_sym* _s) { fude_zoom_sym_ellipse(_s, 0, 0, _s->hw, _s->hh, FZS_OUT); }
RDE_INTERNAL void fzs_boundary(fude_zoom_sym* _s) { fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT); }
RDE_INTERNAL void fzs_socket(fude_zoom_sym* _s) {
    // A required interface: a half circle open to the right, on a stalk.
    const f64 _r = FZS_R(_s) * 0.5;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, 0, _s->hh - _r, _r, _r, FUDE_ZOOM_SYMBOL_PI * 0.15, FUDE_ZOOM_SYMBOL_PI * 0.85);
    fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_line(_s, 0, (_s->hh - _r) / _s->hh, 0, -1, 0u);
}

// UML behaviour.
RDE_INTERNAL void fzs_lifeline(fude_zoom_sym* _s) {
    const f64 _top = 1.0 - 2.0 * fmin(0.18, 30.0 / fmax(_s->hh, 1e-9));
    fude_zoom_sym_rect(_s, -1, _top, 1, 1, FZS_OUT);
    fude_zoom_sym_line(_s, 0, _top, 0, -1, FUDE_ZOOM_SYMBOL_DASHED);
}
RDE_INTERNAL void fzs_activation(fude_zoom_sym* _s) { fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT); }
RDE_INTERNAL void fzs_destroy(fude_zoom_sym* _s) {
    fude_zoom_sym_line(_s, -1, -1, 1, 1, 0u);
    fude_zoom_sym_line(_s, -1, 1, 1, -1, 0u);
}
RDE_INTERNAL void fzs_fragment(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    // Its tag, top left: a pentagon round its operator (alt, loop, opt — its text's first line).
    const f64 _tw = fmin(_s->hw * 0.5, 70.0), _th = fmin(_s->hh * 0.25, 24.0);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_s->hw, _s->hh - _th);
    fude_zoom_sym_pt(_s, -_s->hw + _tw - _th * 0.5, _s->hh - _th);
    fude_zoom_sym_pt(_s, -_s->hw + _tw, _s->hh - _th * 0.5);
    fude_zoom_sym_pt(_s, -_s->hw + _tw, _s->hh);
    fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_initial(fude_zoom_sym* _s)   { fude_zoom_sym_ellipse(_s, 0, 0, FZS_R(_s), FZS_R(_s), FZS_SOLID | FUDE_ZOOM_SYMBOL_FILLED); }
RDE_INTERNAL void fzs_final(fude_zoom_sym* _s) {
    fude_zoom_sym_ellipse(_s, 0, 0, FZS_R(_s), FZS_R(_s), FZS_OUT);
    fude_zoom_sym_ellipse(_s, 0, 0, FZS_R(_s) * 0.6, FZS_R(_s) * 0.6, FZS_SOLID);
}
RDE_INTERNAL void fzs_flow_final(fude_zoom_sym* _s) {
    fude_zoom_sym_ellipse(_s, 0, 0, FZS_R(_s), FZS_R(_s), FZS_OUT);
    const f64 _k = 0.7071 * FZS_R(_s);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_k, -_k); fude_zoom_sym_pt(_s, _k, _k); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_k, _k); fude_zoom_sym_pt(_s, _k, -_k); fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_bar(fude_zoom_sym* _s)       { fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_SOLID | FUDE_ZOOM_SYMBOL_FILLED); }
RDE_INTERNAL void fzs_action(fude_zoom_sym* _s)    { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.45, FZS_OUT); }
RDE_INTERNAL void fzs_state(fude_zoom_sym* _s)     { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.3, FZS_OUT); }
RDE_INTERNAL void fzs_send(fude_zoom_sym* _s)      { const f64 _d[10] = { -1, -1, 0.7, -1, 1, 0, 0.7, 1, -1, 1 }; fude_zoom_sym_poly(_s, _d, 5u, FZS_OUT); }
RDE_INTERNAL void fzs_receive(fude_zoom_sym* _s)   { const f64 _d[10] = { -1, -1, 1, -1, 1, 1, -1, 1, -0.7, 0 }; fude_zoom_sym_poly(_s, _d, 5u, FZS_OUT); }
RDE_INTERNAL void fzs_hourglass(fude_zoom_sym* _s) { const f64 _d[8] = { -1, 1, 1, 1, -1, -1, 1, -1 }; fude_zoom_sym_poly(_s, _d, 4u, FZS_OUT); }
RDE_INTERNAL void fzs_history(fude_zoom_sym* _s) {
    fude_zoom_sym_ellipse(_s, 0, 0, FZS_R(_s), FZS_R(_s), FZS_OUT);
    const f64 _r = FZS_R(_s);
    // An H.
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_r * 0.32, -_r * 0.45); fude_zoom_sym_pt(_s, -_r * 0.32, _r * 0.45); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, _r * 0.32, -_r * 0.45); fude_zoom_sym_pt(_s, _r * 0.32, _r * 0.45); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_r * 0.32, 0); fude_zoom_sym_pt(_s, _r * 0.32, 0); fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_small_decision(fude_zoom_sym* _s) { fzs_decision(_s); }
// A header band's line: where its text box ends (fude_zoom_symbol_text_boxes' HEADER).
RDE_INTERNAL f64 fzs_header_v(const fude_zoom_sym* _s) {
    return 1.0 - fmin(_s->hh * 0.32, 44.0) / fmax(_s->hh, 1e-9);
}

RDE_INTERNAL void fzs_vlane(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    fude_zoom_sym_line(_s, -1, fzs_header_v(_s), 1, fzs_header_v(_s), 0u);
}

// Data.
RDE_INTERNAL void fzs_cylinder(fude_zoom_sym* _s) {
    const f64 _ry = fmin(_s->hh * 0.18, _s->hw * 0.35);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, 0, _s->hh - _ry, _s->hw, _ry, 0.0, FUDE_ZOOM_SYMBOL_PI);
    fude_zoom_sym_arc_pts(_s, 0, -_s->hh + _ry, _s->hw, _ry, FUDE_ZOOM_SYMBOL_PI, 2.0 * FUDE_ZOOM_SYMBOL_PI);
    fude_zoom_sym_end(_s, FZS_OUT);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, 0, _s->hh - _ry, _s->hw, _ry, FUDE_ZOOM_SYMBOL_PI, 2.0 * FUDE_ZOOM_SYMBOL_PI);
    fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_table(fude_zoom_sym* _s) { fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT); }   // (its header's line: from its text)
RDE_INTERNAL void fzs_entity(fude_zoom_sym* _s) { fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT); }
RDE_INTERNAL void fzs_weak_entity(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    const f64 _g = FZS_R(_s) * 0.12;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_s->hw + _g, -_s->hh + _g); fude_zoom_sym_pt(_s, _s->hw - _g, -_s->hh + _g);
    fude_zoom_sym_pt(_s, _s->hw - _g, _s->hh - _g); fude_zoom_sym_pt(_s, -_s->hw + _g, _s->hh - _g);
    fude_zoom_sym_end(_s, FZS_LOOP);
}
RDE_INTERNAL void fzs_relationship(fude_zoom_sym* _s) { fzs_decision(_s); }
RDE_INTERNAL void fzs_weak_relationship(fude_zoom_sym* _s) {
    fzs_decision(_s);
    const f64 _k = 0.78;
    const f64 _d[8] = { 0, -_k, _k, 0, 0, _k, -_k, 0 };
    fude_zoom_sym_poly(_s, _d, 4u, FZS_LOOP);
}
RDE_INTERNAL void fzs_attribute(fude_zoom_sym* _s) { fude_zoom_sym_ellipse(_s, 0, 0, _s->hw, _s->hh, FZS_OUT); }
RDE_INTERNAL void fzs_multivalued(fude_zoom_sym* _s) {
    fude_zoom_sym_ellipse(_s, 0, 0, _s->hw, _s->hh, FZS_OUT);
    const f64 _g = FZS_R(_s) * 0.14;
    fude_zoom_sym_ellipse(_s, 0, 0, _s->hw - _g, _s->hh - _g, FZS_LOOP);
}
RDE_INTERNAL void fzs_derived(fude_zoom_sym* _s) { fude_zoom_sym_ellipse(_s, 0, 0, _s->hw, _s->hh, FZS_OUT | FUDE_ZOOM_SYMBOL_DASHED); }
RDE_INTERNAL void fzs_file(fude_zoom_sym* _s) {
    const f64 _f = FZS_R(_s) * 0.45;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_s->hw, -_s->hh); fude_zoom_sym_pt(_s, _s->hw, -_s->hh); fude_zoom_sym_pt(_s, _s->hw, _s->hh - _f);
    fude_zoom_sym_pt(_s, _s->hw - _f, _s->hh); fude_zoom_sym_pt(_s, -_s->hw, _s->hh);
    fude_zoom_sym_end(_s, FZS_OUT);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, _s->hw - _f, _s->hh); fude_zoom_sym_pt(_s, _s->hw - _f, _s->hh - _f); fude_zoom_sym_pt(_s, _s->hw, _s->hh - _f);
    fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_disks(fude_zoom_sym* _s) {
    // Storage: three discs stacked.
    const f64 _ry = fmin(_s->hh * 0.14, _s->hw * 0.3);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, 0, _s->hh - _ry, _s->hw, _ry, 0.0, FUDE_ZOOM_SYMBOL_PI);
    fude_zoom_sym_arc_pts(_s, 0, -_s->hh + _ry, _s->hw, _ry, FUDE_ZOOM_SYMBOL_PI, 2.0 * FUDE_ZOOM_SYMBOL_PI);
    fude_zoom_sym_end(_s, FZS_OUT);
    for(u32 _k = 0; _k < 3u; _k++) {
        const f64 _y = _s->hh - _ry - (f64)_k * (2.0 * _s->hh - 2.0 * _ry) / 3.0;
        fude_zoom_sym_begin(_s);
        fude_zoom_sym_arc_pts(_s, 0, _y, _s->hw, _ry, FUDE_ZOOM_SYMBOL_PI, 2.0 * FUDE_ZOOM_SYMBOL_PI);
        fude_zoom_sym_end(_s, 0u);
    }
}

// Architecture (C4, servers and services, devices, network).
RDE_INTERNAL void fzs_person(fude_zoom_sym* _s) {
    // C4's person: a round head over a rounded body (its text in the body).
    const f64 _hr = fmin(_s->hw * 0.32, _s->hh * 0.24);
    const f64 _body_top = _s->hh - 2.0 * _hr + _hr * 0.25;
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _body_top, fmin(_s->hw, _s->hh) * 0.35, FZS_OUT);
    fude_zoom_sym_ellipse(_s, 0, _s->hh - _hr, _hr, _hr, FZS_LOOP | FUDE_ZOOM_SYMBOL_FILLED);
}
RDE_INTERNAL void fzs_system(fude_zoom_sym* _s)   { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.12, FZS_OUT); }
RDE_INTERNAL void fzs_external(fude_zoom_sym* _s) { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.12, FZS_OUT | FUDE_ZOOM_SYMBOL_DASHED); }
RDE_INTERNAL void fzs_c4_boundary(fude_zoom_sym* _s) { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.05, FZS_OUT | FUDE_ZOOM_SYMBOL_DASHED); }
RDE_INTERNAL void fzs_server(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.08, FZS_OUT);
    for(u32 _k = 1; _k < 3u; _k++) {
        const f64 _v = 1.0 - 2.0 * (f64)_k / 3.0;
        fude_zoom_sym_line(_s, -1, _v, 1, _v, 0u);
    }
    for(u32 _k = 0; _k < 3u; _k++) {
        const f64 _v = 1.0 - (2.0 * (f64)_k + 1.0) / 3.0;
        const f64 _r = FZS_R(_s) * 0.07;
        fude_zoom_sym_ellipse(_s, _s->hw * 0.62, _v * _s->hh, _r, _r, FZS_SOLID);
        fude_zoom_sym_line(_s, -0.7, _v, 0.2, _v, 0u);
    }
}
RDE_INTERNAL void fzs_desktop(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -0.45, 1, 1, FZS_OUT);
    fude_zoom_sym_rect(_s, -0.88, -0.3, 0.88, 0.88, FZS_LOOP);
    const f64 _d[8] = { -0.12, -0.45, 0.12, -0.45, 0.2, -0.85, -0.2, -0.85 };
    fude_zoom_sym_poly(_s, _d, 4u, FZS_LOOP);
    fude_zoom_sym_line(_s, -0.45, -1, 0.45, -1, 0u);
    fude_zoom_sym_line(_s, -0.45, -0.85, 0.45, -0.85, 0u);
}
RDE_INTERNAL void fzs_laptop(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw * 0.8, -_s->hh * 0.55, _s->hw * 0.8, _s->hh, FZS_R(_s) * 0.06, FZS_OUT);
    fude_zoom_sym_rect(_s, -0.7, -0.4, 0.7, 0.86, FZS_LOOP);
    const f64 _d[8] = { -0.8, -0.55, 0.8, -0.55, 1, -1, -1, -1 };
    fude_zoom_sym_poly(_s, _d, 4u, FZS_LOOP | FUDE_ZOOM_SYMBOL_FILLED);
}
RDE_INTERNAL void fzs_phone(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.25, FZS_OUT);
    fude_zoom_sym_rect(_s, -0.82, -0.75, 0.82, 0.8, FZS_LOOP);
    const f64 _r = FZS_R(_s) * 0.09;
    fude_zoom_sym_ellipse(_s, 0, -_s->hh * 0.87, _r, _r, FZS_LOOP);
}
RDE_INTERNAL void fzs_tablet(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.14, FZS_OUT);
    fude_zoom_sym_rect(_s, -0.86, -0.8, 0.86, 0.86, FZS_LOOP);
}
RDE_INTERNAL void fzs_browser(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.06, FZS_OUT);
    const f64 _bar = fmin(0.32, 22.0 / fmax(_s->hh, 1e-9));
    fude_zoom_sym_line(_s, -1, 1.0 - _bar, 1, 1.0 - _bar, 0u);
    const f64 _r = fmin(_bar * _s->hh * 0.22, _s->hw * 0.05);
    for(u32 _k = 0; _k < 3u; _k++) {
        fude_zoom_sym_ellipse(_s, -_s->hw + _r * (2.6 + 3.0 * (f64)_k), _s->hh * (1.0 - _bar * 0.5), _r, _r, FZS_LOOP);
    }
}
RDE_INTERNAL void fzs_cloud(fude_zoom_sym* _s) {
    // Bumps round a base: arcs of circles along its top and ends, flat underneath.
    fude_zoom_sym_begin(_s);
    const f64 _w = _s->hw, _h = _s->hh;
    fude_zoom_sym_pt(_s, -_w * 0.7, -_h);
    fude_zoom_sym_pt(_s, _w * 0.7, -_h);
    fude_zoom_sym_arc_pts(_s, _w * 0.68, -_h * 0.3, _w * 0.32, _h * 0.7, -FUDE_ZOOM_SYMBOL_PI * 0.5, FUDE_ZOOM_SYMBOL_PI * 0.55);
    fude_zoom_sym_arc_pts(_s, _w * 0.18, _h * 0.25, _w * 0.42, _h * 0.75, FUDE_ZOOM_SYMBOL_PI * 0.1, FUDE_ZOOM_SYMBOL_PI * 0.95);
    fude_zoom_sym_arc_pts(_s, -_w * 0.42, _h * 0.05, _w * 0.3, _h * 0.55, FUDE_ZOOM_SYMBOL_PI * 0.35, FUDE_ZOOM_SYMBOL_PI * 1.05);
    fude_zoom_sym_arc_pts(_s, -_w * 0.7, -_h * 0.5, _w * 0.3, _h * 0.5, FUDE_ZOOM_SYMBOL_PI * 0.6, FUDE_ZOOM_SYMBOL_PI * 1.5);
    fude_zoom_sym_end(_s, FZS_OUT);
}
RDE_INTERNAL void fzs_queue(fude_zoom_sym* _s) {
    // A pipe on its side, slots along it (messages).
    const f64 _rx = fmin(_s->hw * 0.16, _s->hh * 0.45);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, _s->hw - _rx, 0, _rx, _s->hh, -FUDE_ZOOM_SYMBOL_PI * 0.5, FUDE_ZOOM_SYMBOL_PI * 0.5);
    fude_zoom_sym_arc_pts(_s, -_s->hw + _rx, 0, _rx, _s->hh, FUDE_ZOOM_SYMBOL_PI * 0.5, FUDE_ZOOM_SYMBOL_PI * 1.5);
    fude_zoom_sym_end(_s, FZS_OUT);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, _s->hw - _rx, 0, _rx, _s->hh, FUDE_ZOOM_SYMBOL_PI * 0.5, FUDE_ZOOM_SYMBOL_PI * 1.5);
    fude_zoom_sym_end(_s, 0u);
    for(u32 _k = 0; _k < 3u; _k++) {
        const f64 _x = -_s->hw + _rx * 2.2 + (f64)_k * (_s->hw * 2.0 - _rx * 4.0) / 3.0;
        fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, _x, -_s->hh * 0.55); fude_zoom_sym_pt(_s, _x, _s->hh * 0.55); fude_zoom_sym_end(_s, 0u);
    }
}
RDE_INTERNAL void fzs_cache(fude_zoom_sym* _s) {
    fzs_cylinder(_s);
    // A lightning bolt (fast).
    const f64 _r = FZS_R(_s) * 0.55;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, _r * 0.15, _r * 0.9); fude_zoom_sym_pt(_s, -_r * 0.45, -_r * 0.05); fude_zoom_sym_pt(_s, -_r * 0.02, -_r * 0.05);
    fude_zoom_sym_pt(_s, -_r * 0.2, -_r * 0.9); fude_zoom_sym_pt(_s, _r * 0.45, _r * 0.15); fude_zoom_sym_pt(_s, _r * 0.02, _r * 0.15);
    fude_zoom_sym_end(_s, FZS_SOLID);
}
RDE_INTERNAL void fzs_balancer(fude_zoom_sym* _s) {
    const f64 _r = FZS_R(_s);
    fude_zoom_sym_ellipse(_s, 0, 0, _r, _r, FZS_OUT);
    // One in from the left, three out to the right.
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_r * 0.7, 0); fude_zoom_sym_pt(_s, -_r * 0.1, 0); fude_zoom_sym_end(_s, 0u);
    for(i32 _k = -1; _k <= 1; _k++) {
        fude_zoom_sym_arrow(_s, -_r * 0.1, 0, _r * 0.65, (f64)_k * _r * 0.45, _r * 0.18);
    }
}
RDE_INTERNAL void fzs_gateway(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.12, FZS_OUT);
    // "< >": code going through.
    const f64 _r = FZS_R(_s) * 0.42;
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_r * 0.35, _r * 0.6); fude_zoom_sym_pt(_s, -_r, 0); fude_zoom_sym_pt(_s, -_r * 0.35, -_r * 0.6); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, _r * 0.35, _r * 0.6); fude_zoom_sym_pt(_s, _r, 0); fude_zoom_sym_pt(_s, _r * 0.35, -_r * 0.6); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, _r * 0.15, _r * 0.7); fude_zoom_sym_pt(_s, -_r * 0.15, -_r * 0.7); fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_microservice(fude_zoom_sym* _s) { fude_zoom_sym_regular(_s, 6u, 0.0, FZS_OUT); }
RDE_INTERNAL void fzs_function(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.2, FZS_OUT);
    // A λ.
    const f64 _r = FZS_R(_s) * 0.6;
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_r * 0.55, _r * 0.85); fude_zoom_sym_pt(_s, -_r * 0.25, _r * 0.85); fude_zoom_sym_pt(_s, _r * 0.6, -_r * 0.85); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_r * 0.6, -_r * 0.85); fude_zoom_sym_pt(_s, _r * 0.08, _r * 0.25); fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_container(fude_zoom_sym* _s) {
    // A shipping container: a box with its ribs.
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    for(u32 _k = 1; _k < 6u; _k++) {
        const f64 _u = -1.0 + 2.0 * (f64)_k / 6.0;
        fude_zoom_sym_line(_s, _u, -0.8, _u, 0.8, 0u);
    }
}
RDE_INTERNAL void fzs_pod(fude_zoom_sym* _s) { fude_zoom_sym_regular(_s, 7u, FUDE_ZOOM_SYMBOL_PI * 0.5, FZS_OUT); }
RDE_INTERNAL void fzs_firewall(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    for(u32 _row = 1; _row < 4u; _row++) {
        const f64 _v = -1.0 + 2.0 * (f64)_row / 4.0;
        fude_zoom_sym_line(_s, -1, _v, 1, _v, 0u);
    }
    for(u32 _row = 0; _row < 4u; _row++) {
        const f64 _v0 = -1.0 + 2.0 * (f64)_row / 4.0, _v1 = _v0 + 0.5;
        for(u32 _k = 0; _k < 3u; _k++) {
            const f64 _u = -1.0 + (2.0 * (f64)_k + (_row % 2u == 0u ? 1.0 : 2.0)) / 3.0 * (2.0 / 2.0);
            if(_u > -1.0 && _u < 1.0) {
                fude_zoom_sym_line(_s, _u, _v0, _u, _v1, 0u);
            }
        }
    }
}
RDE_INTERNAL void fzs_bucket(fude_zoom_sym* _s) {
    const f64 _ry = fmin(_s->hh * 0.16, _s->hw * 0.3);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, 0, _s->hh - _ry, _s->hw, _ry, 0.0, FUDE_ZOOM_SYMBOL_PI);
    fude_zoom_sym_pt(_s, -_s->hw * 0.75, -_s->hh);
    fude_zoom_sym_pt(_s, _s->hw * 0.75, -_s->hh);
    fude_zoom_sym_end(_s, FZS_OUT);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, 0, _s->hh - _ry, _s->hw, _ry, FUDE_ZOOM_SYMBOL_PI, 2.0 * FUDE_ZOOM_SYMBOL_PI);
    fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_globe(fude_zoom_sym* _s) {
    const f64 _r = FZS_R(_s);
    fude_zoom_sym_ellipse(_s, 0, 0, _r, _r, FZS_OUT);
    fude_zoom_sym_ellipse(_s, 0, 0, _r * 0.45, _r, FZS_LOOP);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, 0, -_r); fude_zoom_sym_pt(_s, 0, _r); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_r, 0); fude_zoom_sym_pt(_s, _r, 0); fude_zoom_sym_end(_s, 0u);
    for(i32 _k = -1; _k <= 1; _k += 2) {
        const f64 _y = (f64)_k * _r * 0.55, _x = sqrt(fmax(_r * _r - _y * _y, 0.0));
        fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_x, _y); fude_zoom_sym_pt(_s, _x, _y); fude_zoom_sym_end(_s, 0u);
    }
}
RDE_INTERNAL void fzs_router(fude_zoom_sym* _s) {
    const f64 _r = FZS_R(_s);
    fude_zoom_sym_ellipse(_s, 0, 0, _r, _r, FZS_OUT);
    // Four ways out.
    fude_zoom_sym_arrow(_s, _r * 0.1, _r * 0.1, _r * 0.7, _r * 0.7, _r * 0.2);
    fude_zoom_sym_arrow(_s, -_r * 0.1, -_r * 0.1, -_r * 0.7, -_r * 0.7, _r * 0.2);
    fude_zoom_sym_arrow(_s, _r * 0.7, -_r * 0.7, _r * 0.1, -_r * 0.1, _r * 0.2);
    fude_zoom_sym_arrow(_s, -_r * 0.7, _r * 0.7, -_r * 0.1, _r * 0.1, _r * 0.2);
}
RDE_INTERNAL void fzs_switch(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.15, FZS_OUT);
    const f64 _l = _s->hw * 0.55, _h = _s->hh * 0.32, _a = FZS_R(_s) * 0.22;
    fude_zoom_sym_arrow(_s, -_l, _h, _l, _h, _a);
    fude_zoom_sym_arrow(_s, _l, -_h, -_l, -_h, _a);
}
RDE_INTERNAL void fzs_user(fude_zoom_sym* _s) {
    // A head and shoulders.
    const f64 _r = fmin(_s->hw * 0.45, _s->hh * 0.3);
    fude_zoom_sym_ellipse(_s, 0, _s->hh - _r, _r, _r, FZS_LOOP | FUDE_ZOOM_SYMBOL_FILLED);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, 0, -_s->hh, _s->hw, _s->hh - 2.0 * _r - _s->hh * 0.08 + _s->hh, 0.0, FUDE_ZOOM_SYMBOL_PI);
    fude_zoom_sym_end(_s, FZS_OUT);
}
RDE_INTERNAL void fzs_bus(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, _s->hh, FZS_OUT);
    fude_zoom_sym_arrow(_s, -_s->hw * 0.8, 0, _s->hw * 0.8, 0, FZS_R(_s) * 0.5);
}
RDE_INTERNAL void fzs_lock(fude_zoom_sym* _s) {
    // A padlock.
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh * 0.15, FZS_R(_s) * 0.12, FZS_OUT);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_s->hw * 0.6, _s->hh * 0.15);
    fude_zoom_sym_arc_pts(_s, 0, _s->hh * 0.4, _s->hw * 0.6, _s->hh * 0.6, FUDE_ZOOM_SYMBOL_PI, 0.0);
    fude_zoom_sym_pt(_s, _s->hw * 0.6, _s->hh * 0.15);
    fude_zoom_sym_end(_s, 0u);
    const f64 _r = FZS_R(_s) * 0.16;
    fude_zoom_sym_ellipse(_s, 0, -_s->hh * 0.4, _r, _r, FZS_SOLID);
}
RDE_INTERNAL void fzs_clock(fude_zoom_sym* _s) {
    const f64 _r = FZS_R(_s);
    fude_zoom_sym_ellipse(_s, 0, 0, _r, _r, FZS_OUT);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, 0, _r * 0.62); fude_zoom_sym_pt(_s, 0, 0); fude_zoom_sym_pt(_s, _r * 0.45, -_r * 0.2); fude_zoom_sym_end(_s, 0u);
    for(u32 _k = 0; _k < 12u; _k++) {
        const f64 _a = 2.0 * FUDE_ZOOM_SYMBOL_PI * (f64)_k / 12.0;
        fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, cos(_a) * _r * 0.85, sin(_a) * _r * 0.85); fude_zoom_sym_pt(_s, cos(_a) * _r, sin(_a) * _r); fude_zoom_sym_end(_s, 0u);
    }
}
RDE_INTERNAL void fzs_gear(fude_zoom_sym* _s) {
    const f64 _r = FZS_R(_s);
    fude_zoom_sym_begin(_s);
    const u32 _teeth = 8u;
    for(u32 _k = 0; _k < _teeth; _k++) {
        const f64 _a = 2.0 * FUDE_ZOOM_SYMBOL_PI * (f64)_k / (f64)_teeth, _t = FUDE_ZOOM_SYMBOL_PI / (f64)_teeth;
        fude_zoom_sym_pt(_s, cos(_a - _t * 0.55) * _r * 0.78, sin(_a - _t * 0.55) * _r * 0.78);
        fude_zoom_sym_pt(_s, cos(_a - _t * 0.3) * _r, sin(_a - _t * 0.3) * _r);
        fude_zoom_sym_pt(_s, cos(_a + _t * 0.3) * _r, sin(_a + _t * 0.3) * _r);
        fude_zoom_sym_pt(_s, cos(_a + _t * 0.55) * _r * 0.78, sin(_a + _t * 0.55) * _r * 0.78);
    }
    fude_zoom_sym_end(_s, FZS_OUT);
    fude_zoom_sym_ellipse(_s, 0, 0, _r * 0.32, _r * 0.32, FZS_LOOP);
}
RDE_INTERNAL void fzs_folder(fude_zoom_sym* _s) {
    const f64 _d[14] = { -1, -1, 1, -1, 1, 0.7, -0.1, 0.7, -0.25, 1, -1, 1, -1, 0.7 };
    fude_zoom_sym_poly(_s, _d, 6u, FZS_OUT);
}
RDE_INTERNAL void fzs_chart(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    const f64 _d[10] = { -0.8, -0.6, -0.35, 0.1, 0.05, -0.25, 0.45, 0.45, 0.8, 0.2 };
    fude_zoom_sym_poly(_s, _d, 5u, 0u);
}
RDE_INTERNAL void fzs_container_db(fude_zoom_sym* _s) { fzs_cylinder(_s); }
RDE_INTERNAL void fzs_internet(fude_zoom_sym* _s) { fzs_cloud(_s); }

// BPMN.
RDE_INTERNAL void fzs_start_event(fude_zoom_sym* _s) { fude_zoom_sym_ellipse(_s, 0, 0, FZS_R(_s), FZS_R(_s), FZS_OUT); }
RDE_INTERNAL void fzs_intermediate(fude_zoom_sym* _s) {
    fude_zoom_sym_ellipse(_s, 0, 0, FZS_R(_s), FZS_R(_s), FZS_OUT);
    fude_zoom_sym_ellipse(_s, 0, 0, FZS_R(_s) * 0.84, FZS_R(_s) * 0.84, FZS_LOOP);
}
RDE_INTERNAL void fzs_end_event(fude_zoom_sym* _s) {
    // A thick ring: two close circles, the band between them solid.
    fude_zoom_sym_ellipse(_s, 0, 0, FZS_R(_s), FZS_R(_s), FZS_OUT);
    fude_zoom_sym_begin(_s);
    const u32 _k = 48u;
    for(u32 _i = 0; _i <= _k; _i++) {
        const f64 _a = 2.0 * FUDE_ZOOM_SYMBOL_PI * (f64)_i / (f64)_k;
        fude_zoom_sym_pt(_s, cos(_a) * FZS_R(_s), sin(_a) * FZS_R(_s));
    }
    for(u32 _i = 0; _i <= _k; _i++) {
        const f64 _a = -2.0 * FUDE_ZOOM_SYMBOL_PI * (f64)_i / (f64)_k;
        fude_zoom_sym_pt(_s, cos(_a) * FZS_R(_s) * 0.82, sin(_a) * FZS_R(_s) * 0.82);
    }
    fude_zoom_sym_end(_s, FZS_SOLID);
}
RDE_INTERNAL void fzs_timer_event(fude_zoom_sym* _s) {
    fzs_intermediate(_s);
    const f64 _r = FZS_R(_s) * 0.6;
    fude_zoom_sym_ellipse(_s, 0, 0, _r, _r, FZS_LOOP);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, 0, _r * 0.75); fude_zoom_sym_pt(_s, 0, 0); fude_zoom_sym_pt(_s, _r * 0.5, 0); fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_message_event(fude_zoom_sym* _s) {
    fzs_intermediate(_s);
    const f64 _w = FZS_R(_s) * 0.5, _h = FZS_R(_s) * 0.33;
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_w, -_h); fude_zoom_sym_pt(_s, _w, -_h); fude_zoom_sym_pt(_s, _w, _h); fude_zoom_sym_pt(_s, -_w, _h); fude_zoom_sym_end(_s, FZS_LOOP);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_w, _h); fude_zoom_sym_pt(_s, 0, 0); fude_zoom_sym_pt(_s, _w, _h); fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_task(fude_zoom_sym* _s) { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.2, FZS_OUT); }
RDE_INTERNAL void fzs_subprocess(fude_zoom_sym* _s) {
    fzs_task(_s);
    const f64 _b = FZS_R(_s) * 0.16;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_b, -_s->hh); fude_zoom_sym_pt(_s, _b, -_s->hh); fude_zoom_sym_pt(_s, _b, -_s->hh + 2.0 * _b); fude_zoom_sym_pt(_s, -_b, -_s->hh + 2.0 * _b);
    fude_zoom_sym_end(_s, FZS_LOOP);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_b * 0.6, -_s->hh + _b); fude_zoom_sym_pt(_s, _b * 0.6, -_s->hh + _b); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, 0, -_s->hh + _b * 0.4); fude_zoom_sym_pt(_s, 0, -_s->hh + _b * 1.6); fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_xor_gateway(fude_zoom_sym* _s) {
    fzs_decision(_s);
    fude_zoom_sym_line(_s, -0.32, -0.32, 0.32, 0.32, 0u);
    fude_zoom_sym_line(_s, -0.32, 0.32, 0.32, -0.32, 0u);
}
RDE_INTERNAL void fzs_and_gateway(fude_zoom_sym* _s) {
    fzs_decision(_s);
    fude_zoom_sym_line(_s, -0.45, 0, 0.45, 0, 0u);
    fude_zoom_sym_line(_s, 0, -0.45, 0, 0.45, 0u);
}
RDE_INTERNAL void fzs_or_gateway(fude_zoom_sym* _s) {
    fzs_decision(_s);
    fude_zoom_sym_ellipse(_s, 0, 0, _s->hw * 0.4, _s->hh * 0.4, FZS_LOOP);
}
RDE_INTERNAL void fzs_data_object(fude_zoom_sym* _s) { fzs_file(_s); }
RDE_INTERNAL void fzs_data_store(fude_zoom_sym* _s)  { fzs_cylinder(_s); }
RDE_INTERNAL void fzs_pool(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    const f64 _u = -1.0 + fmin(_s->hw * 0.16, 34.0) / fmax(_s->hw, 1e-9);   // (where its text box ends: LEFT)
    fude_zoom_sym_line(_s, _u, -1, _u, 1, 0u);
}
RDE_INTERNAL void fzs_group(fude_zoom_sym* _s) { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.08, FZS_OUT | FUDE_ZOOM_SYMBOL_DASHED); }

// Planning.
RDE_INTERNAL void fzs_column(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, fmin(_s->hw, _s->hh) * 0.04, FZS_OUT);
    fude_zoom_sym_line(_s, -1, fzs_header_v(_s), 1, fzs_header_v(_s), 0u);
}
RDE_INTERNAL void fzs_task_card(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.1, FZS_OUT);
    // Its tag down its left edge.
    const f64 _t = fmin(_s->hw * 0.05, 6.0);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_s->hw + _t * 0.5, -_s->hh * 0.75); fude_zoom_sym_pt(_s, -_s->hw + _t * 1.5, -_s->hh * 0.75);
    fude_zoom_sym_pt(_s, -_s->hw + _t * 1.5, _s->hh * 0.75); fude_zoom_sym_pt(_s, -_s->hw + _t * 0.5, _s->hh * 0.75);
    fude_zoom_sym_end(_s, FZS_SOLID);
}
RDE_INTERNAL void fzs_hlane(fude_zoom_sym* _s) { fzs_pool(_s); }
// An area (map.h): a region of the canvas, named — its outline only (its name in its corner).
RDE_INTERNAL void fzs_area(fude_zoom_sym* _s) { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.025, FZS_OUT); }
RDE_INTERNAL void fzs_milestone(fude_zoom_sym* _s) {
    const f64 _d[8] = { 0, -1, 1, 0, 0, 1, -1, 0 };
    fude_zoom_sym_poly(_s, _d, 4u, FZS_SOLID | FUDE_ZOOM_SYMBOL_FILLED);
}
RDE_INTERNAL void fzs_matrix(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    fude_zoom_sym_line(_s, 0, -1, 0, 1, 0u);
    fude_zoom_sym_line(_s, -1, 0, 1, 0, 0u);
}
RDE_INTERNAL void fzs_arrow_block(fude_zoom_sym* _s) {
    const f64 _d[14] = { -1, -0.4, 0.35, -0.4, 0.35, -1, 1, 0, 0.35, 1, 0.35, 0.4, -1, 0.4 };
    fude_zoom_sym_poly(_s, _d, 7u, FZS_OUT);
}
RDE_INTERNAL void fzs_timeline(fude_zoom_sym* _s) {
    fude_zoom_sym_arrow(_s, -_s->hw, 0, _s->hw, 0, fmin(_s->hh, 14.0));
    for(u32 _k = 0; _k < 5u; _k++) {
        const f64 _x = -_s->hw + (f64)_k * _s->hw * 2.0 / 5.0 + _s->hw * 0.15;
        fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, _x, -_s->hh * 0.5); fude_zoom_sym_pt(_s, _x, _s->hh * 0.5); fude_zoom_sym_end(_s, 0u);
    }
}

// More (the research's: the sequence diagram's heads, a terminal, a key, an envelope, code, a team, a brace,
// a DFD's data store, an event stream's topic).
RDE_INTERNAL void fzs_seq_boundary(fude_zoom_sym* _s) {
    const f64 _r = fmin(_s->hw * 0.8, _s->hh);
    const f64 _cx = _s->hw - _r;
    fude_zoom_sym_ellipse(_s, _cx, 0, _r, _r, FZS_OUT);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_s->hw, -_r * 0.6); fude_zoom_sym_pt(_s, -_s->hw, _r * 0.6); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_s->hw, 0); fude_zoom_sym_pt(_s, _cx - _r, 0); fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_seq_control(fude_zoom_sym* _s) {
    const f64 _r = FZS_R(_s) * 0.9;
    fude_zoom_sym_ellipse(_s, 0, -_s->hh * 0.08, _r, _r, FZS_OUT);
    // Its arrowhead on top, going round.
    const f64 _y = -_s->hh * 0.08 + _r;
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, _r * 0.25, _y + _r * 0.28); fude_zoom_sym_pt(_s, -_r * 0.2, _y); fude_zoom_sym_pt(_s, _r * 0.25, _y - _r * 0.28); fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_seq_entity(fude_zoom_sym* _s) {
    const f64 _r = FZS_R(_s) * 0.9;
    fude_zoom_sym_ellipse(_s, 0, _s->hh * 0.08, _r, _r, FZS_OUT);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_r, _s->hh * 0.08 - _r); fude_zoom_sym_pt(_s, _r, _s->hh * 0.08 - _r); fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_terminal(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.08, FZS_OUT);
    const f64 _bar = fmin(0.28, 18.0 / fmax(_s->hh, 1e-9));
    fude_zoom_sym_line(_s, -1, 1.0 - _bar, 1, 1.0 - _bar, 0u);
    const f64 _r = FZS_R(_s) * 0.3;
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_s->hw * 0.7, _r * 0.6); fude_zoom_sym_pt(_s, -_s->hw * 0.7 + _r, 0); fude_zoom_sym_pt(_s, -_s->hw * 0.7, -_r * 0.6); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_s->hw * 0.7 + _r * 1.4, -_r * 0.7); fude_zoom_sym_pt(_s, -_s->hw * 0.7 + _r * 2.6, -_r * 0.7); fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_key(fude_zoom_sym* _s) {
    const f64 _r = fmin(_s->hh * 0.55, _s->hw * 0.32);
    fude_zoom_sym_ellipse(_s, -_s->hw + _r, 0, _r, _r, FZS_OUT);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_s->hw + 2.0 * _r, 0); fude_zoom_sym_pt(_s, _s->hw, 0); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, _s->hw * 0.55, 0); fude_zoom_sym_pt(_s, _s->hw * 0.55, -_r * 0.7); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, _s->hw * 0.85, 0); fude_zoom_sym_pt(_s, _s->hw * 0.85, -_r * 0.7); fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_envelope(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    const f64 _d[6] = { -1, 1, 0, -0.15, 1, 1 };
    fude_zoom_sym_poly(_s, _d, 3u, 0u);
}
RDE_INTERNAL void fzs_code(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.15, FZS_OUT);
    const f64 _r = FZS_R(_s) * 0.6;
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_r * 0.3, _r * 0.55); fude_zoom_sym_pt(_s, -_r * 0.85, 0); fude_zoom_sym_pt(_s, -_r * 0.3, -_r * 0.55); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, _r * 0.3, _r * 0.55); fude_zoom_sym_pt(_s, _r * 0.85, 0); fude_zoom_sym_pt(_s, _r * 0.3, -_r * 0.55); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, _r * 0.15, _r * 0.7); fude_zoom_sym_pt(_s, -_r * 0.15, -_r * 0.7); fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_users(fude_zoom_sym* _s) {
    // Two heads and shoulders, one behind the other.
    const f64 _r = fmin(_s->hw * 0.26, _s->hh * 0.28);
    for(u32 _k = 0; _k < 2u; _k++) {
        const f64 _x = _k == 0u ? _s->hw * 0.28 : -_s->hw * 0.18, _top = _k == 0u ? _s->hh : _s->hh * 0.82;
        const f64 _sh = _k == 0u ? _s->hw * 0.72 : _s->hw * 0.82;
        fude_zoom_sym_ellipse(_s, _x, _top - _r, _r, _r, FZS_LOOP | FUDE_ZOOM_SYMBOL_FILLED);
        fude_zoom_sym_begin(_s);
        fude_zoom_sym_arc_pts(_s, _x, -_s->hh, _sh, (_top - 2.0 * _r - _s->hh * 0.06) + _s->hh, 0.0, FUDE_ZOOM_SYMBOL_PI);
        fude_zoom_sym_end(_s, FZS_OUT);
    }
}
RDE_INTERNAL void fzs_brace(fude_zoom_sym* _s) {
    // A "{": its points along its left, its middle out.
    fude_zoom_sym_begin(_s);
    const f64 _w = _s->hw, _h = _s->hh;
    fude_zoom_sym_pt(_s, -_w * 0.6, _h);
    fude_zoom_sym_arc_pts(_s, -_w * 0.6, _h * 0.85, _w * 0.3, _h * 0.15, FUDE_ZOOM_SYMBOL_PI * 0.5, FUDE_ZOOM_SYMBOL_PI);
    fude_zoom_sym_pt(_s, -_w * 0.9, _h * 0.15);
    fude_zoom_sym_arc_pts(_s, -_w * 1.2, _h * 0.15, _w * 0.3, _h * 0.15, 0.0, -FUDE_ZOOM_SYMBOL_PI * 0.5);
    fude_zoom_sym_arc_pts(_s, -_w * 1.2, -_h * 0.15, _w * 0.3, _h * 0.15, FUDE_ZOOM_SYMBOL_PI * 0.5, 0.0);
    fude_zoom_sym_pt(_s, -_w * 0.9, -_h * 0.85);
    fude_zoom_sym_arc_pts(_s, -_w * 0.6, -_h * 0.85, _w * 0.3, _h * 0.15, FUDE_ZOOM_SYMBOL_PI, FUDE_ZOOM_SYMBOL_PI * 1.5);
    fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_dfd_store(fude_zoom_sym* _s) {
    fude_zoom_sym_line(_s, -1, 1, 1, 1, 0u);
    fude_zoom_sym_line(_s, -1, -1, 1, -1, 0u);
}
RDE_INTERNAL void fzs_topic(fude_zoom_sym* _s) {
    // An event stream: three partitions, each a pipe, the newest end to the right.
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    for(u32 _k = 1; _k < 3u; _k++) {
        const f64 _v = 1.0 - 2.0 * (f64)_k / 3.0;
        fude_zoom_sym_line(_s, -1, _v, 1, _v, 0u);
    }
    for(u32 _k = 0; _k < 3u; _k++) {
        const f64 _v0 = 1.0 - 2.0 * (f64)_k / 3.0, _v1 = _v0 - 2.0 / 3.0;
        for(u32 _c = 1; _c < 5u; _c++) {
            const f64 _u = -1.0 + 2.0 * (f64)_c / 6.0;
            fude_zoom_sym_line(_s, _u, _v0, _u, _v1, 0u);
        }
    }
}


// Electronics (circuit.h): each part drawn there, by its id.
RDE_INTERNAL void fzs_part_0(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("resistor"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_1(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("potentiometer"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_2(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("capacitor"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_3(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("electrolytic"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_4(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("inductor"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_5(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("diode"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_6(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("LED"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_7(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("zener"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_8(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("NPN"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_9(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("PNP"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_10(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("N-MOSFET"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_11(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("P-MOSFET"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_12(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("DC source"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_13(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("battery"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_14(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("AC source"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_15(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("clock"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_16(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("current source"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_17(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("ground"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_18(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("supply rail"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_19(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("SPST switch"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_20(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("push button"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_21(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("lamp"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_22(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("motor"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_23(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("buzzer"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_24(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("fuse"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_25(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("op-amp"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_26(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("voltmeter"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_27(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("ammeter"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_28(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("7-segment"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_29(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("breadboard"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_30(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("AND gate"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_31(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("OR gate"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_32(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("NOT gate"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_33(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("NAND gate"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_34(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("NOR gate"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_35(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("XOR gate"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_36(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("XNOR gate"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_37(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("buffer"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_38(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("D flip-flop"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_39(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("T flip-flop"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_40(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("logic input"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_41(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("logic probe"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_42(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("NE555"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_43(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("LM358"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_44(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("ATmega328P"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_45(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("74HC595"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_46(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("L293D"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_47(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("ULN2003"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_48(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("74HC00"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_49(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("74HC08"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_50(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("74HC32"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_51(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("74HC86"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_52(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("74HC04"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_53(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("MAX7219"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_54(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("PCF8574"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_55(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("7805"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_56(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("AMS1117"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_57(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("DS3231"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_58(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("MPU6050"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_59(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("nRF24L01"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_60(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("HC-05"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_61(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("HC-SR04"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_62(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("DHT11"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_63(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("servo"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_64(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("TP4056"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_65(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("WS2812B"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_66(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("L298N"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_67(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("relay module"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_68(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("I2C LCD"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_69(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("OLED"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_70(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("Arduino Uno"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_71(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("Arduino Nano"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_72(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("Raspberry Pi Pico"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_73(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("Raspberry Pi GPIO"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_74(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("ESP32"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_75(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("ESP8266"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }


// Mechanisms (mech.h): each part drawn there, by its id.
RDE_INTERNAL void fzs_mech_0(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("link"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_1(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("plate"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_2(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("gear 10T"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_3(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("gear 15T"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_4(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("gear 20T"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_5(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("gear 30T"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_6(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("gear 40T"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_7(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("gear 60T"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_8(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("fixed pivot"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_9(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("drive motor"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_10(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("spring"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_11(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("weight"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_12(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("wheel"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_13(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("wall"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_14(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("pulley"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_15(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("rope"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_16(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("crate"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_17(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("rail"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_18(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("slider"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_19(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("rack"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_20(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("pin"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_chip_7402(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("74HC02"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_chip_7474(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("74HC74"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_chip_74138(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("74HC138"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_chip_74157(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("74HC157"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_chip_74161(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("74HC161"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_chip_74173(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("74HC173"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_chip_74245(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("74HC245"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_chip_74283(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("74HC283"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_chip_74189(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("74HC189"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_disp_panel(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("7-segment panel"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_disp_matrix(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("LED matrix"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_disp_bar(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("LED bar graph"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_chip_3914(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("LM3914"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_chip_4511(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("CD4511"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_disp_meter(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("panel meter"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_disp_lcd(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("character LCD"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_disp_scope(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("oscilloscope"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_tracer(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("tracer"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_crank(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("hand crank"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_belt(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("belt"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_chain(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("chain"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_sprocket(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("sprocket"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_cam(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("cam"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_follower(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("follower"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_ratchet(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("ratchet"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_pawl(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("pawl"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_damper(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("damper"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_mech_worm(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find("worm"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_spdt(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("SPDT switch"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_dpdt(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("DPDT switch"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_ldr(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("LDR"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_ntc(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("thermistor"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_solenoid(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("solenoid"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_stepper(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("stepper motor"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_encoder(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("rotary encoder"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_jk(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("JK flip-flop"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_sr(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("SR latch"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_lm393(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("LM393"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_scr(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("SCR"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_triac(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("TRIAC"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_xfmr(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("transformer"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_bridge(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("bridge rectifier"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_opto(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("optocoupler"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_photodiode(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("photodiode"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_phototrans(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("phototransistor"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_speaker(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("speaker"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_cd4017(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("CD4017"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_part_slot(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("slotted sensor"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_custom(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find("custom part"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }


// Building plans (plan.h): each part drawn there, by its id.
RDE_INTERNAL void fzs_plan_0(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("door 70"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_1(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("door 80"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_2(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("door 90"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_3(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("double door"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_4(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("sliding door"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_5(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("window 60"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_6(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("window 100"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_7(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("window 120"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_8(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("window 150"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_9(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("window 200"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_10(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("opening 90"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_11(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("single bed"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_12(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("double bed"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_13(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("queen bed"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_14(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("sofa"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_15(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("armchair"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_16(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("dining table"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_17(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("round table"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_18(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("chair"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_19(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("desk"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_20(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("wardrobe"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_21(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("kitchen counter"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_22(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("kitchen sink"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_23(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("hob"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_24(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("fridge"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_25(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("washing machine"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_26(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("toilet"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_27(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("bathtub"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_28(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("shower"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_29(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("washbasin"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_30(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("stairs"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_31(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("socket"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_32(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("double socket"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_33(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("light switch"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_34(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("two-way switch"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_35(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("dimmer"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_36(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("bell push"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_37(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("ceiling light"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_38(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("wall light"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_39(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("tube light"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_40(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("consumer unit"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_41(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("junction box"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_42(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("TV point"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_43(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("data point"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_44(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("smoke detector"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_45(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("thermostat"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_46(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("ceiling fan"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
RDE_INTERNAL void fzs_plan_47(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find("meter"), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }

// Maths (plot.h): a graph's and axes' box (their axes and curves drawn from their text: render.c, export.c), a number
// line's line.
RDE_INTERNAL void fzs_graph(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FUDE_ZOOM_SYMBOL_FILLED);
}
RDE_INTERNAL void fzs_number_line(fude_zoom_sym* _s) {
    fude_zoom_sym_line(_s, -0.92, 0.0, 0.92, 0.0, 0u);
}
// Sewing: a grainline (an arrow each end), the fold (a bracket, its arrows to the fold), a notch, a button, a buttonhole,
// a dart, a pattern piece's label.
RDE_INTERNAL void fzs_grainline(fude_zoom_sym* _s) {
    fude_zoom_sym_line(_s, -0.95, 0.0, 0.95, 0.0, 0u);
    fude_zoom_sym_head(_s, 0.95 * _s->hw, 0.0, 1.0, 0.0, 0.08 * _s->hw);
    fude_zoom_sym_head(_s, -0.95 * _s->hw, 0.0, -1.0, 0.0, 0.08 * _s->hw);
}
RDE_INTERNAL void fzs_fold(fude_zoom_sym* _s) {
    const f64 _b[8] = { -0.9, -1.0, -0.9, 0.6, 0.9, 0.6, 0.9, -1.0 };
    fude_zoom_sym_poly(_s, _b, 4u, 0u);
    fude_zoom_sym_head(_s, -0.9 * _s->hw, -_s->hh, 0.0, -1.0, 0.25 * _s->hh);
    fude_zoom_sym_head(_s, 0.9 * _s->hw, -_s->hh, 0.0, -1.0, 0.25 * _s->hh);
}
RDE_INTERNAL void fzs_notch(fude_zoom_sym* _s) {
    const f64 _t[6] = { -0.8, 1.0, 0.8, 1.0, 0.0, -1.0 };
    fude_zoom_sym_poly(_s, _t, 3u, FUDE_ZOOM_SYMBOL_CLOSED | FUDE_ZOOM_SYMBOL_SOLID);
}
RDE_INTERNAL void fzs_button(fude_zoom_sym* _s) {
    fude_zoom_sym_ellipse(_s, 0.0, 0.0, _s->hw * 0.95, _s->hh * 0.95, FUDE_ZOOM_SYMBOL_FILLED);
    for(u32 _i = 0; _i < 4u; _i++) {
        const f64 _x = (_i % 2u == 0u ? -0.3 : 0.3) * _s->hw, _y = (_i < 2u ? -0.3 : 0.3) * _s->hh;
        fude_zoom_sym_ellipse(_s, _x, _y, _s->hw * 0.1, _s->hh * 0.1, 0u);
    }
}
RDE_INTERNAL void fzs_buttonhole(fude_zoom_sym* _s) {
    fude_zoom_sym_line(_s, -0.9, 0.0, 0.9, 0.0, 0u);
    fude_zoom_sym_line(_s, -0.9, -0.9, -0.9, 0.9, 0u);
    fude_zoom_sym_line(_s, 0.9, -0.9, 0.9, 0.9, 0u);
}
RDE_INTERNAL void fzs_dart(fude_zoom_sym* _s) {
    const f64 _v[6] = { -1.0, 1.0, 0.0, -1.0, 1.0, 1.0 };
    fude_zoom_sym_poly(_s, _v, 3u, FUDE_ZOOM_SYMBOL_DASHED);
    fude_zoom_sym_line(_s, 0.0, 1.0, 0.0, -1.0, FUDE_ZOOM_SYMBOL_DASHED);   // (its fold)
}
RDE_INTERNAL void fzs_pattern_label(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FUDE_ZOOM_SYMBOL_DASHED);
}

// Plain shapes (0.1.85).

// A closed shape of _n points in any units (pairs), stretched to fill the box (its own widest and tallest at its sides).
RDE_INTERNAL void fzs_fit(fude_zoom_sym* _s, const f64* _xy, u32 _n, u8 _flags) {
    f64 _x0 = 1e30, _y0 = 1e30, _x1 = -1e30, _y1 = -1e30;
    for(u32 _i = 0; _i < _n; _i++) {
        _x0 = fmin(_x0, _xy[2u * _i]); _x1 = fmax(_x1, _xy[2u * _i]);
        _y0 = fmin(_y0, _xy[2u * _i + 1u]); _y1 = fmax(_y1, _xy[2u * _i + 1u]);
    }
    if(!(_x1 > _x0) || !(_y1 > _y0)) {
        return;
    }
    fude_zoom_sym_begin(_s);
    for(u32 _i = 0; _i < _n; _i++) {
        fude_zoom_sym_uv(_s, -1.0 + 2.0 * (_xy[2u * _i] - _x0) / (_x1 - _x0), -1.0 + 2.0 * (_xy[2u * _i + 1u] - _y0) / (_y1 - _y0));
    }
    fude_zoom_sym_end(_s, _flags);
}

// A star of _n points (_inner: its notches' radius, its points' 1; 1: a polygon of _n sides), the first point up,
// filling the box.
RDE_INTERNAL void fzs_star_of(fude_zoom_sym* _s, u32 _n, f64 _inner) {
    f64 _xy[2u * 2u * 24u];
    const u32 _m = _inner < 1.0 ? 2u * _n : _n;
    for(u32 _i = 0; _i < _m && _i < 48u; _i++) {
        const f64 _a = FUDE_ZOOM_SYMBOL_PI * 0.5 + 2.0 * FUDE_ZOOM_SYMBOL_PI * (f64)_i / (f64)_m;
        const f64 _r = _inner < 1.0 && (_i & 1u) ? _inner : 1.0;
        _xy[2u * _i] = cos(_a) * _r;
        _xy[2u * _i + 1u] = sin(_a) * _r;
    }
    fzs_fit(_s, _xy, _m, FZS_OUT);
}

RDE_INTERNAL void fzs_rectangle(fude_zoom_sym* _s)  { fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT); }
RDE_INTERNAL void fzs_round_rect(fude_zoom_sym* _s) { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.3, FZS_OUT); }
RDE_INTERNAL void fzs_circle(fude_zoom_sym* _s)     { fude_zoom_sym_ellipse(_s, 0, 0, _s->hw, _s->hh, FZS_OUT); }
RDE_INTERNAL void fzs_parallelogram(fude_zoom_sym* _s) {
    const f64 _k = fmin(0.5, _s->hh / _s->hw * 0.5);   // (its slant the same however wide)
    const f64 _d[8] = { -1, -1, 1 - _k, -1, 1, 1, -1 + _k, 1 };
    fude_zoom_sym_poly(_s, _d, 4u, FZS_OUT);
}
RDE_INTERNAL void fzs_trapezoid(fude_zoom_sym* _s) {
    const f64 _k = fmin(0.5, _s->hh / _s->hw * 0.5);
    const f64 _d[8] = { -1, -1, 1, -1, 1 - _k, 1, -1 + _k, 1 };
    fude_zoom_sym_poly(_s, _d, 4u, FZS_OUT);
}
RDE_INTERNAL void fzs_right_triangle(fude_zoom_sym* _s) { const f64 _d[6] = { -1, -1, 1, -1, -1, 1 }; fude_zoom_sym_poly(_s, _d, 3u, FZS_OUT); }
RDE_INTERNAL void fzs_pentagon(fude_zoom_sym* _s)   { fzs_star_of(_s, 5u, 1.0); }
RDE_INTERNAL void fzs_octagon(fude_zoom_sym* _s)    { fude_zoom_sym_regular(_s, 8u, FUDE_ZOOM_SYMBOL_PI / 8.0, FZS_OUT); }
RDE_INTERNAL void fzs_star(fude_zoom_sym* _s)       { fzs_star_of(_s, 5u, 0.382); }
RDE_INTERNAL void fzs_burst(fude_zoom_sym* _s)      { fzs_star_of(_s, 12u, 0.74); }
RDE_INTERNAL void fzs_cross(fude_zoom_sym* _s) {
    const f64 _t = 0.34;
    const f64 _d[24] = { -_t, 1, _t, 1, _t, _t, 1, _t, 1, -_t, _t, -_t, _t, -1, -_t, -1, -_t, -_t, -1, -_t, -1, _t, -_t, _t };
    fude_zoom_sym_poly(_s, _d, 12u, FZS_OUT);
}
RDE_INTERNAL void fzs_heart(fude_zoom_sym* _s) {
    // The classic curve: x = 16 sin³t, y = 13 cos t − 5 cos 2t − 2 cos 3t − cos 4t.
    f64 _xy[2u * 64u];
    for(u32 _i = 0; _i < 64u; _i++) {
        const f64 _t = 2.0 * FUDE_ZOOM_SYMBOL_PI * (f64)_i / 64.0, _sn = sin(_t);
        _xy[2u * _i] = 16.0 * _sn * _sn * _sn;
        _xy[2u * _i + 1u] = 13.0 * cos(_t) - 5.0 * cos(2.0 * _t) - 2.0 * cos(3.0 * _t) - cos(4.0 * _t);
    }
    fzs_fit(_s, _xy, 64u, FZS_OUT);
}
// A speech bubble: a rounded body over its tail, down from its left (its text in its body).
#define FZS_BUBBLE_FOOT 0.45   // (its body's bottom, as v)
RDE_INTERNAL void fzs_speech(fude_zoom_sym* _s) {
    const f64 _w = _s->hw, _h = _s->hh, _yb = -_h * FZS_BUBBLE_FOOT;
    const f64 _r = fmin(_w, (_h - _yb) * 0.5) * 0.45, _q = FUDE_ZOOM_SYMBOL_PI * 0.5;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, _w - _r, _yb + _r, _r, _r, -_q, 0.0);
    fude_zoom_sym_arc_pts(_s, _w - _r, _h - _r, _r, _r, 0.0, _q);
    fude_zoom_sym_arc_pts(_s, -_w + _r, _h - _r, _r, _r, _q, 2.0 * _q);
    fude_zoom_sym_arc_pts(_s, -_w + _r, _yb + _r, _r, _r, 2.0 * _q, 3.0 * _q);
    fude_zoom_sym_pt(_s, -_w * 0.45, _yb);
    fude_zoom_sym_pt(_s, -_w * 0.62, -_h);
    fude_zoom_sym_pt(_s, -_w * 0.12, _yb);
    fude_zoom_sym_end(_s, FZS_OUT);
}
// A thought bubble: a cloud of scallops over two little rounds.
RDE_INTERNAL void fzs_thought(fude_zoom_sym* _s) {
    const f64 _w = _s->hw, _h = _s->hh, _cy = _h * 0.22, _ry = _h * 0.78;
    fude_zoom_sym_begin(_s);
    const u32 _k = 9u * 12u;
    for(u32 _i = 0; _i < _k; _i++) {
        const f64 _a = 2.0 * FUDE_ZOOM_SYMBOL_PI * (f64)_i / (f64)_k;
        const f64 _f = 0.86 + 0.14 * fabs(sin(_a * 4.5));
        fude_zoom_sym_pt(_s, cos(_a) * _w * _f, _cy + sin(_a) * _ry * _f);
    }
    fude_zoom_sym_end(_s, FZS_OUT);
    const f64 _m = FZS_R(_s);
    fude_zoom_sym_ellipse(_s, -_w * 0.5, -_h * 0.72, _m * 0.13, _m * 0.13, FZS_OUT);
    fude_zoom_sym_ellipse(_s, -_w * 0.7, -_h * 0.92, _m * 0.08, _m * 0.08, FZS_OUT);
}
RDE_INTERNAL void fzs_arc(fude_zoom_sym* _s) {
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, 0.0, -_s->hh, _s->hw, 2.0 * _s->hh, 0.0, FUDE_ZOOM_SYMBOL_PI);
    fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_pie(fude_zoom_sym* _s) {
    // Three quarters of a round: a slice out of its top right.
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, 0.0, 0.0);
    fude_zoom_sym_arc_pts(_s, 0.0, 0.0, _s->hw, _s->hh, FUDE_ZOOM_SYMBOL_PI * 0.5, FUDE_ZOOM_SYMBOL_PI * 2.0);
    fude_zoom_sym_end(_s, FZS_OUT);
}
RDE_INTERNAL void fzs_ring(fude_zoom_sym* _s) {
    fude_zoom_sym_ellipse(_s, 0, 0, _s->hw, _s->hh, FZS_OUT);
    fude_zoom_sym_ellipse(_s, 0, 0, _s->hw * 0.55, _s->hh * 0.55, FZS_OUT | FUDE_ZOOM_SYMBOL_JOIN);   // (its hole)
}
// A cube: its front, its top and its right side (its text on its front).
#define FZS_CUBE_DEPTH 0.5   // (its depth, of its box's half: as u and v)
RDE_INTERNAL void fzs_cube(fude_zoom_sym* _s) {
    const f64 _d = FZS_CUBE_DEPTH, _f = 1.0 - _d;
    const f64 _o[12] = { -1, -1, _f, -1, 1, -1 + _d, 1, 1, -1 + _d, 1, -1, _f };
    fude_zoom_sym_poly(_s, _o, 6u, FZS_OUT);
    const f64 _l[6] = { -1, _f, _f, _f, _f, -1 };
    fude_zoom_sym_poly(_s, _l, 3u, 0u);
    fude_zoom_sym_line(_s, _f, _f, 1, 1, 0u);
}
// Block arrows: a shaft and a head as long as the arrow is thick, however long it is.
RDE_INTERNAL void fzs_block_arrow(fude_zoom_sym* _s) {
    const f64 _w = _s->hw, _h = _s->hh, _head = fmin(_w, _h * 1.1), _t = _h * 0.42;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_w, -_t); fude_zoom_sym_pt(_s, _w - _head, -_t); fude_zoom_sym_pt(_s, _w - _head, -_h); fude_zoom_sym_pt(_s, _w, 0.0);
    fude_zoom_sym_pt(_s, _w - _head, _h); fude_zoom_sym_pt(_s, _w - _head, _t); fude_zoom_sym_pt(_s, -_w, _t);
    fude_zoom_sym_end(_s, FZS_OUT);
}
RDE_INTERNAL void fzs_double_arrow(fude_zoom_sym* _s) {
    const f64 _w = _s->hw, _h = _s->hh, _head = fmin(_w * 0.5, _h * 1.1), _t = _h * 0.42;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_w, 0.0); fude_zoom_sym_pt(_s, -_w + _head, -_h); fude_zoom_sym_pt(_s, -_w + _head, -_t);
    fude_zoom_sym_pt(_s, _w - _head, -_t); fude_zoom_sym_pt(_s, _w - _head, -_h); fude_zoom_sym_pt(_s, _w, 0.0);
    fude_zoom_sym_pt(_s, _w - _head, _h); fude_zoom_sym_pt(_s, _w - _head, _t); fude_zoom_sym_pt(_s, -_w + _head, _t);
    fude_zoom_sym_pt(_s, -_w + _head, _h);
    fude_zoom_sym_end(_s, FZS_OUT);
}
RDE_INTERNAL void fzs_curved_arrow(fude_zoom_sym* _s) {
    // A band over the top of an ellipse, from its left foot round to a head pointing down on its right.
    const f64 _e = 0.6, _ri[2] = { 0.55, 1.2 }, _ro[2] = { 0.85, 1.75 };
    f64 _xy[2u * 64u];
    u32 _n = 0;
    const u32 _k = 24u;
    for(u32 _i = 0; _i <= _k; _i++) {   // (its outer side, from its left foot over the top)
        const f64 _a = FUDE_ZOOM_SYMBOL_PI + (_e - FUDE_ZOOM_SYMBOL_PI) * (f64)_i / (f64)_k;
        _xy[2u * _n] = cos(_a) * _ro[0]; _xy[2u * _n + 1u] = -1.0 + sin(_a) * _ro[1]; _n++;
    }
    const f64 _po[2] = { cos(_e) * _ro[0], -1.0 + sin(_e) * _ro[1] }, _pi[2] = { cos(_e) * _ri[0], -1.0 + sin(_e) * _ri[1] };
    const f64 _mid[2] = { (_po[0] + _pi[0]) * 0.5, (_po[1] + _pi[1]) * 0.5 };
    f64 _dir[2] = { sin(_e) * (_ro[0] + _ri[0]) * 0.5, -cos(_e) * (_ro[1] + _ri[1]) * 0.5 };
    const f64 _dl = hypot(_dir[0], _dir[1]);
    _dir[0] /= _dl; _dir[1] /= _dl;
    _xy[2u * _n] = _po[0] + (_po[0] - _mid[0]) * 0.7; _xy[2u * _n + 1u] = _po[1] + (_po[1] - _mid[1]) * 0.7; _n++;
    _xy[2u * _n] = _mid[0] + _dir[0] * 0.75; _xy[2u * _n + 1u] = _mid[1] + _dir[1] * 0.75; _n++;
    _xy[2u * _n] = _pi[0] + (_pi[0] - _mid[0]) * 0.7; _xy[2u * _n + 1u] = _pi[1] + (_pi[1] - _mid[1]) * 0.7; _n++;
    for(u32 _i = 0; _i <= _k; _i++) {   // (its inner side, back to its left foot)
        const f64 _a = _e + (FUDE_ZOOM_SYMBOL_PI - _e) * (f64)_i / (f64)_k;
        _xy[2u * _n] = cos(_a) * _ri[0]; _xy[2u * _n + 1u] = -1.0 + sin(_a) * _ri[1]; _n++;
    }
    fzs_fit(_s, _xy, _n, FZS_OUT);
}
RDE_INTERNAL void fzs_chevron(fude_zoom_sym* _s) {
    const f64 _w = _s->hw, _h = _s->hh, _d = fmin(_w * 0.5, _h * 0.6);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_w, -_h); fude_zoom_sym_pt(_s, _w - _d, -_h); fude_zoom_sym_pt(_s, _w, 0.0);
    fude_zoom_sym_pt(_s, _w - _d, _h); fude_zoom_sym_pt(_s, -_w, _h); fude_zoom_sym_pt(_s, -_w + _d, 0.0);
    fude_zoom_sym_end(_s, FZS_OUT);
}

// --- 0.1.86: Venn diagrams, geometry's marks, science, stamps ------------------------------------------------

// The last part made given its own colours: _fill (TINTED: alpha 0 none), its line's _line (alpha 0: the symbol's own),
// _width (its own units; 0: the symbol's line).
RDE_INTERNAL void fzs_tint(fude_zoom_sym* _s, rde_color _fill, rde_color _line, f32 _width) {
    const u32 _n = (u32)rde_arr_length(_s->parts);
    if(_n == 0u) {
        return;
    }
    fude_zoom_symbol_part* _p = &((fude_zoom_symbol_part*)_s->parts->memory)[_n - 1u];
    if(_fill.a > 0u) {
        _p->flags = (u8)((_p->flags & ~FUDE_ZOOM_SYMBOL_FILLED) | FUDE_ZOOM_SYMBOL_TINTED);
    }
    _p->fill  = _fill;
    _p->line  = _line;
    _p->width = _width;
}

// A circle round (x, y) in own units, its points' flags _flags.
RDE_INTERNAL void fzs_round(fude_zoom_sym* _s, f64 _x, f64 _y, f64 _r, u8 _flags) {
    fude_zoom_sym_ellipse(_s, _x, _y, _r, _r, _flags);
}

// Venn diagrams: their circles (not filled: each seen through the others).
RDE_INTERNAL f64 fzs_venn2_r(f64 _hw, f64 _hh) { return fmin(_hh, _hw * 0.62); }
RDE_INTERNAL void fzs_venn2(fude_zoom_sym* _s) {
    const f64 _r = fzs_venn2_r(_s->hw, _s->hh), _d = _s->hw - _r;
    fzs_round(_s, -_d, 0.0, _r, FZS_LOOP);
    fzs_round(_s, _d, 0.0, _r, FZS_LOOP);
}
RDE_INTERNAL f64 fzs_venn3_r(f64 _hw, f64 _hh) { return fmin(_hw, _hh) * 0.6; }
RDE_INTERNAL void fzs_venn3(fude_zoom_sym* _s) {
    const f64 _r = fzs_venn3_r(_s->hw, _s->hh), _d = _r * 0.62;
    for(u32 _i = 0; _i < 3u; _i++) {
        const f64 _a = FUDE_ZOOM_SYMBOL_PI * 0.5 + 2.0 * FUDE_ZOOM_SYMBOL_PI * (f64)_i / 3.0;
        fzs_round(_s, cos(_a) * _d, sin(_a) * _d - _s->hh * 0.05, _r, FZS_LOOP);
    }
}

// Geometry's marks (in the box's proportions: a mark rotates with the lasso to sit on what it marks).
RDE_INTERNAL void fzs_angle_mark(fude_zoom_sym* _s) {
    const f64 _vx = -_s->hw * 0.85, _vy = -_s->hh * 0.8, _a = 50.0 * FUDE_ZOOM_SYMBOL_PI / 180.0;
    const f64 _len = fmin((_s->hw - _vx) / cos(_a), (_s->hh - _vy) / sin(_a));
    fude_zoom_sym_line(_s, -0.85, -0.8, 1.0, -0.8, 0u);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, _vx, _vy);
    fude_zoom_sym_pt(_s, _vx + cos(_a) * _len, _vy + sin(_a) * _len);
    fude_zoom_sym_end(_s, 0u);
    const f64 _r = FZS_R(_s) * 0.6;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, _vx, _vy, _r, _r, 0.0, _a);
    fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_right_angle_mark(fude_zoom_sym* _s) {
    fude_zoom_sym_line(_s, -0.85, -0.85, 1.0, -0.85, 0u);
    fude_zoom_sym_line(_s, -0.85, -0.85, -0.85, 1.0, 0u);
    const f64 _q = FZS_R(_s) * 0.45, _x = -_s->hw * 0.85, _y = -_s->hh * 0.85;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, _x + _q, _y); fude_zoom_sym_pt(_s, _x + _q, _y + _q); fude_zoom_sym_pt(_s, _x, _y + _q);
    fude_zoom_sym_end(_s, 0u);
}
// Ticks across a side (equal lengths): _n of them.
RDE_INTERNAL void fzs_ticks(fude_zoom_sym* _s, u32 _n) {
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _u = _n == 1u ? 0.0 : -0.6 + 1.2 * (f64)_i / (f64)(_n - 1u);
        fude_zoom_sym_line(_s, _u, -1.0, _u, 1.0, 0u);
    }
}
RDE_INTERNAL void fzs_tick1(fude_zoom_sym* _s) { fzs_ticks(_s, 1u); }
RDE_INTERNAL void fzs_tick2(fude_zoom_sym* _s) { fzs_ticks(_s, 2u); }
RDE_INTERNAL void fzs_tick3(fude_zoom_sym* _s) { fzs_ticks(_s, 3u); }
// Arrowheads along a side (parallels): _n of them.
RDE_INTERNAL void fzs_parallels(fude_zoom_sym* _s, u32 _n) {
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _u = _n == 1u ? 0.3 : (_i == 0u ? -0.15 : 0.75);
        const f64 _d[6] = { _u - 0.6, 1.0, _u, 0.0, _u - 0.6, -1.0 };
        fude_zoom_sym_poly(_s, _d, 3u, 0u);
    }
}
RDE_INTERNAL void fzs_parallel1(fude_zoom_sym* _s) { fzs_parallels(_s, 1u); }
RDE_INTERNAL void fzs_parallel2(fude_zoom_sym* _s) { fzs_parallels(_s, 2u); }
RDE_INTERNAL void fzs_point(fude_zoom_sym* _s) { fzs_round(_s, 0.0, 0.0, FZS_R(_s) * 0.45, FZS_SOLID); }
RDE_INTERNAL void fzs_segment(fude_zoom_sym* _s) {
    fude_zoom_sym_line(_s, -0.9, 0.0, 0.9, 0.0, 0u);
    const f64 _r = _s->hh * 0.45;
    fzs_round(_s, -_s->hw * 0.9, 0.0, _r, FZS_SOLID);
    fzs_round(_s, _s->hw * 0.9, 0.0, _r, FZS_SOLID);
}
RDE_INTERNAL void fzs_ray(fude_zoom_sym* _s) {
    fude_zoom_sym_arrow(_s, -_s->hw * 0.9, 0.0, _s->hw, 0.0, _s->hh * 1.6);
    fzs_round(_s, -_s->hw * 0.9, 0.0, _s->hh * 0.45, FZS_SOLID);
}
RDE_INTERNAL void fzs_line_both(fude_zoom_sym* _s) {
    fude_zoom_sym_arrow(_s, -_s->hw * 0.2, 0.0, _s->hw, 0.0, _s->hh * 1.6);
    fude_zoom_sym_arrow(_s, _s->hw * 0.2, 0.0, -_s->hw, 0.0, _s->hh * 1.6);
}
RDE_INTERNAL void fzs_vector(fude_zoom_sym* _s) { fude_zoom_sym_arrow(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.5); }
RDE_INTERNAL void fzs_circle_radius(fude_zoom_sym* _s) {
    fzs_round(_s, 0.0, 0.0, FZS_R(_s), FZS_LOOP);
    fzs_round(_s, 0.0, 0.0, FZS_R(_s) * 0.05, FZS_SOLID);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, 0.0, 0.0);
    fude_zoom_sym_pt(_s, FZS_R(_s) * 0.7071, FZS_R(_s) * 0.7071);
    fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_protractor(fude_zoom_sym* _s) {
    // Half a round on its flat side, a tick every 10°, longer every 30°, and its middle marked.
    const f64 _r = fmin(_s->hw, _s->hh * 2.0), _y = -_s->hh;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, 0.0, _y, _r, _r, 0.0, FUDE_ZOOM_SYMBOL_PI);
    fude_zoom_sym_end(_s, FZS_OUT);
    for(u32 _i = 0; _i <= 18u; _i++) {
        const f64 _a = FUDE_ZOOM_SYMBOL_PI * (f64)_i / 18.0, _in = _i % 3u == 0u ? 0.82 : 0.9;
        fude_zoom_sym_begin(_s);
        fude_zoom_sym_pt(_s, cos(_a) * _r, _y + sin(_a) * _r);
        fude_zoom_sym_pt(_s, cos(_a) * _r * _in, _y + sin(_a) * _r * _in);
        fude_zoom_sym_end(_s, 0u);
    }
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, 0.0, _y, _r * 0.25, _r * 0.25, 0.0, FUDE_ZOOM_SYMBOL_PI);
    fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_line(_s, 0.0, -1.0, 0.0, -1.0 + 0.3 * _r / _s->hh, 0u);
}

// Science: glassware (open at the top: their box what they are taken by), optics, an atom, a cell, DNA.
RDE_INTERNAL void fzs_open(fude_zoom_sym* _s, const f64* _uv, u32 _n) { fude_zoom_sym_poly(_s, _uv, _n, 0u); }
RDE_INTERNAL void fzs_graduations(fude_zoom_sym* _s, f64 _x, f64 _v0, f64 _v1, u32 _n, f64 _len) {
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _v = _v0 + (_v1 - _v0) * (f64)_i / (f64)(_n - 1u);
        fude_zoom_sym_line(_s, _x, _v, _x - _len, _v, 0u);
    }
}
RDE_INTERNAL void fzs_beaker(fude_zoom_sym* _s) {
    const f64 _d[10] = { -1.0, 1.0, -0.85, 0.85, -0.85, -1.0, 0.85, -1.0, 0.85, 1.0 };
    fzs_open(_s, _d, 5u);
    fzs_graduations(_s, 0.85, -0.6, 0.6, 4u, 0.25);
}
RDE_INTERNAL void fzs_flask(fude_zoom_sym* _s) {
    const f64 _d[12] = { -0.25, 1.0, -0.25, 0.35, -0.95, -1.0, 0.95, -1.0, 0.25, 0.35, 0.25, 1.0 };
    fzs_open(_s, _d, 6u);
    fude_zoom_sym_line(_s, -0.32, 1.0, 0.32, 1.0, 0u);
}
RDE_INTERNAL void fzs_round_flask(fude_zoom_sym* _s) {
    const f64 _r = fmin(_s->hw, _s->hh * 0.65), _cy = -_s->hh + _r;
    const f64 _neck = fmin(_r * 0.3, _s->hw * 0.3);
    const f64 _a = asin(_neck / _r);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_neck, _s->hh);
    fude_zoom_sym_arc_pts(_s, 0.0, _cy, _r, _r, FUDE_ZOOM_SYMBOL_PI * 0.5 + _a, FUDE_ZOOM_SYMBOL_PI * 2.5 - _a);
    fude_zoom_sym_pt(_s, _neck, _s->hh);
    fude_zoom_sym_end(_s, 0u);
}
RDE_INTERNAL void fzs_test_tube(fude_zoom_sym* _s) {
    const f64 _w = _s->hw * 0.7, _r = _w;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_w, _s->hh);
    fude_zoom_sym_arc_pts(_s, 0.0, -_s->hh + _r, _w, _r, FUDE_ZOOM_SYMBOL_PI, 2.0 * FUDE_ZOOM_SYMBOL_PI);
    fude_zoom_sym_pt(_s, _w, _s->hh);
    fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_line(_s, -0.9, 1.0, 0.9, 1.0, 0u);
}
RDE_INTERNAL void fzs_cylinder_measure(fude_zoom_sym* _s) {
    const f64 _d[8] = { -0.4, 1.0, -0.4, -0.85, 0.4, -0.85, 0.4, 1.0 };
    fzs_open(_s, _d, 4u);
    fude_zoom_sym_rect(_s, -0.8, -1.0, 0.8, -0.85, FZS_OUT);
    fzs_graduations(_s, 0.4, -0.6, 0.8, 8u, 0.2);
}
RDE_INTERNAL void fzs_funnel(fude_zoom_sym* _s) {
    const f64 _l[6] = { -1.0, 1.0, -0.15, -0.15, -0.15, -1.0 }, _r[6] = { 1.0, 1.0, 0.15, -0.15, 0.15, -1.0 };
    fzs_open(_s, _l, 3u);
    fzs_open(_s, _r, 3u);
}
RDE_INTERNAL void fzs_burner(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -0.22, -0.75, 0.22, 0.35, FZS_OUT);
    fude_zoom_sym_rect(_s, -0.8, -1.0, 0.8, -0.75, FZS_OUT);
    // its flame: a drop, round low, pointed at its top
    fude_zoom_sym_begin(_s);
    const u32 _n = 16u;
    for(u32 _i = 0; _i <= _n; _i++) {
        const f64 _t = (f64)_i / (f64)_n;
        fude_zoom_sym_uv(_s, -0.88 * (1.0 - _t) * sqrt(_t), 0.38 + 0.62 * _t);
    }
    for(u32 _i = 1; _i < _n; _i++) {
        const f64 _t = 1.0 - (f64)_i / (f64)_n;
        fude_zoom_sym_uv(_s, 0.88 * (1.0 - _t) * sqrt(_t), 0.38 + 0.62 * _t);
    }
    fude_zoom_sym_end(_s, FZS_OUT);
    fzs_tint(_s, (rde_color){ 245, 150, 40, 230 }, (rde_color){ 0, 0, 0, 0 }, 0.0f);
}
// A lens: its two faces as parabolas — bowed out (converging) or in.
RDE_INTERNAL void fzs_lens(fude_zoom_sym* _s, b8 _convex) {
    const u32 _n = 16u;
    fude_zoom_sym_begin(_s);
    for(u32 _i = 0; _i <= _n; _i++) {
        const f64 _t = 1.0 - 2.0 * (f64)_i / (f64)_n;
        fude_zoom_sym_uv(_s, _convex ? 1.0 - _t * _t : 0.35 + 0.65 * _t * _t, _t);
    }
    for(u32 _i = 0; _i <= _n; _i++) {
        const f64 _t = -1.0 + 2.0 * (f64)_i / (f64)_n;
        fude_zoom_sym_uv(_s, _convex ? -(1.0 - _t * _t) : -(0.35 + 0.65 * _t * _t), _t);
    }
    fude_zoom_sym_end(_s, FZS_OUT);
    fzs_tint(_s, (rde_color){ 150, 200, 240, 90 }, (rde_color){ 0, 0, 0, 0 }, 0.0f);
}
RDE_INTERNAL void fzs_lens_convex(fude_zoom_sym* _s)  { fzs_lens(_s, true); }
RDE_INTERNAL void fzs_lens_concave(fude_zoom_sym* _s) { fzs_lens(_s, false); }
// A mirror: its face (flat, or bowed in) and strokes behind it.
RDE_INTERNAL void fzs_mirror(fude_zoom_sym* _s, b8 _curved) {
    const u32 _n = _curved ? 16u : 1u;
    fude_zoom_v2 _face[17];
    fude_zoom_sym_begin(_s);
    for(u32 _i = 0; _i <= _n; _i++) {
        const f64 _t = -1.0 + 2.0 * (f64)_i / (f64)_n;
        _face[_i] = (fude_zoom_v2){ (_curved ? 0.6 * _t * _t - 0.3 : 0.0) * _s->hw, _t * _s->hh };
        fude_zoom_sym_pt(_s, _face[_i].x, _face[_i].y);
    }
    fude_zoom_sym_end(_s, 0u);
    for(u32 _k = 0; _k < 8u; _k++) {
        const f64 _t = -0.9 + 1.8 * (f64)_k / 7.0;
        const f64 _x = (_curved ? 0.6 * _t * _t - 0.3 : 0.0) * _s->hw, _y = _t * _s->hh;
        fude_zoom_sym_begin(_s);
        fude_zoom_sym_pt(_s, _x, _y);
        fude_zoom_sym_pt(_s, _x + _s->hw * 0.4, _y - _s->hw * 0.4);
        fude_zoom_sym_end(_s, 0u);
    }
}
RDE_INTERNAL void fzs_mirror_flat(fude_zoom_sym* _s)   { fzs_mirror(_s, false); }
RDE_INTERNAL void fzs_mirror_curved(fude_zoom_sym* _s) { fzs_mirror(_s, true); }
RDE_INTERNAL void fzs_prism(fude_zoom_sym* _s) {
    const f64 _d[6] = { -1, -1, 1, -1, 0, 1 };
    fude_zoom_sym_poly(_s, _d, 3u, FZS_OUT);
    fzs_tint(_s, (rde_color){ 150, 200, 240, 90 }, (rde_color){ 0, 0, 0, 0 }, 0.0f);
}
RDE_INTERNAL void fzs_lamp(fude_zoom_sym* _s) {
    const f64 _r = FZS_R(_s);
    fzs_round(_s, 0.0, 0.0, _r * 0.45, FZS_OUT);
    fzs_tint(_s, (rde_color){ 250, 220, 90, 255 }, (rde_color){ 0, 0, 0, 0 }, 0.0f);
    for(u32 _i = 0; _i < 8u; _i++) {
        const f64 _a = 2.0 * FUDE_ZOOM_SYMBOL_PI * (f64)_i / 8.0;
        fude_zoom_sym_begin(_s);
        fude_zoom_sym_pt(_s, cos(_a) * _r * 0.6, sin(_a) * _r * 0.6);
        fude_zoom_sym_pt(_s, cos(_a) * _r, sin(_a) * _r);
        fude_zoom_sym_end(_s, 0u);
    }
}
RDE_INTERNAL void fzs_atom(fude_zoom_sym* _s) {
    // Three orbits (ellipses turned 60° from each other) round its nucleus.
    for(u32 _k = 0; _k < 3u; _k++) {
        const f64 _t = FUDE_ZOOM_SYMBOL_PI * (f64)_k / 3.0, _c = cos(_t), _sn = sin(_t);
        fude_zoom_sym_begin(_s);
        for(u32 _i = 0; _i < 48u; _i++) {
            const f64 _a = 2.0 * FUDE_ZOOM_SYMBOL_PI * (f64)_i / 48.0;
            const f64 _x = cos(_a) * _s->hw, _y = sin(_a) * _s->hh * 0.32;
            fude_zoom_sym_pt(_s, _x * _c - _y * _sn * (_s->hh / _s->hw), _x * _sn * (_s->hh / _s->hw) + _y * _c);
        }
        fude_zoom_sym_end(_s, FZS_LOOP);
    }
    fzs_round(_s, 0.0, 0.0, FZS_R(_s) * 0.16, FZS_SOLID);
}
RDE_INTERNAL void fzs_benzene(fude_zoom_sym* _s) {
    fude_zoom_sym_regular(_s, 6u, FUDE_ZOOM_SYMBOL_PI * 0.5, FZS_OUT);
    fude_zoom_sym_ellipse(_s, 0.0, 0.0, _s->hw * 0.55, _s->hh * 0.55, FZS_LOOP);
}
RDE_INTERNAL void fzs_cell(fude_zoom_sym* _s) {
    fude_zoom_sym_ellipse(_s, 0.0, 0.0, _s->hw, _s->hh, FZS_OUT);
    fude_zoom_sym_ellipse(_s, _s->hw * 0.12, _s->hh * 0.05, _s->hw * 0.28, _s->hh * 0.3, FZS_LOOP);
    fzs_tint(_s, (rde_color){ 170, 120, 220, 110 }, (rde_color){ 0, 0, 0, 0 }, 0.0f);
    const f64 _dots[8] = { -0.55, 0.3, -0.45, -0.4, 0.55, -0.35, 0.6, 0.35 };
    for(u32 _i = 0; _i < 4u; _i++) {
        fude_zoom_sym_ellipse(_s, _dots[2u * _i] * _s->hw, _dots[2u * _i + 1u] * _s->hh, _s->hw * 0.08, _s->hh * 0.06, FZS_LOOP);
    }
}
RDE_INTERNAL void fzs_dna(fude_zoom_sym* _s) {
    // Two strands round each other along it, rungs between them.
    for(u32 _k = 0; _k < 2u; _k++) {
        fude_zoom_sym_begin(_s);
        for(u32 _i = 0; _i <= 48u; _i++) {
            const f64 _t = (f64)_i / 48.0;
            fude_zoom_sym_uv(_s, -1.0 + 2.0 * _t, 0.85 * sin(_t * 4.0 * FUDE_ZOOM_SYMBOL_PI + (_k == 0u ? 0.0 : FUDE_ZOOM_SYMBOL_PI)));
        }
        fude_zoom_sym_end(_s, 0u);
    }
    for(u32 _i = 1; _i < 16u; _i++) {
        const f64 _t = (f64)_i / 16.0, _v = 0.85 * sin(_t * 4.0 * FUDE_ZOOM_SYMBOL_PI);
        if(fabs(_v) > 0.15) {
            fude_zoom_sym_line(_s, -1.0 + 2.0 * _t, _v, -1.0 + 2.0 * _t, -_v, 0u);
        }
    }
}
RDE_INTERNAL void fzs_thermometer(fude_zoom_sym* _s) {
    const f64 _w = _s->hw * 0.35, _b = fmin(_s->hw * 0.75, _s->hh * 0.3);
    fude_zoom_sym_rounded(_s, -_w, -_s->hh + _b, _w, _s->hh, _w, FZS_OUT);
    fzs_round(_s, 0.0, -_s->hh + _b, _b, FZS_OUT);
    fzs_tint(_s, (rde_color){ 225, 70, 60, 255 }, (rde_color){ 0, 0, 0, 0 }, 0.0f);
    fude_zoom_sym_rect(_s, -0.15, -1.0 + _b / _s->hh, 0.15, 0.2, FZS_SOLID);
    fzs_tint(_s, (rde_color){ 225, 70, 60, 255 }, (rde_color){ 225, 70, 60, 255 }, 0.0f);
    fzs_graduations(_s, -0.35, -0.2, 0.85, 6u, 0.25);
}

// Stamps, in their own colours: a thick stroke where a pen would write one.
#define FZS_GREEN  ((rde_color){ 60, 170, 90, 255 })
#define FZS_RED    ((rde_color){ 220, 70, 60, 255 })
#define FZS_AMBER  ((rde_color){ 245, 170, 40, 255 })
#define FZS_BLUE   ((rde_color){ 70, 130, 230, 255 })
#define FZS_GOLD   ((rde_color){ 245, 195, 50, 255 })
#define FZS_DARK   ((rde_color){ 40, 40, 44, 255 })
#define FZS_WHITE  ((rde_color){ 255, 255, 255, 255 })
#define FZS_NONE   ((rde_color){ 0, 0, 0, 0 })
RDE_INTERNAL void fzs_stroke(fude_zoom_sym* _s, const f64* _uv, u32 _n, rde_color _c, f64 _w) {
    fude_zoom_sym_poly(_s, _uv, _n, 0u);
    fzs_tint(_s, FZS_NONE, _c, (f32)(_w * FZS_R(_s)));
}
RDE_INTERNAL void fzs_stamp_check(fude_zoom_sym* _s) {
    const f64 _d[6] = { -0.75, 0.0, -0.25, -0.6, 0.8, 0.7 };
    fzs_stroke(_s, _d, 3u, FZS_GREEN, 0.22);
}
RDE_INTERNAL void fzs_stamp_cross(fude_zoom_sym* _s) {
    const f64 _a[4] = { -0.7, -0.7, 0.7, 0.7 }, _b[4] = { -0.7, 0.7, 0.7, -0.7 };
    fzs_stroke(_s, _a, 2u, FZS_RED, 0.22);
    fzs_stroke(_s, _b, 2u, FZS_RED, 0.22);
}
RDE_INTERNAL void fzs_stamp_warning(fude_zoom_sym* _s) {
    const f64 _d[6] = { -1.0, -0.9, 1.0, -0.9, 0.0, 1.0 };
    fude_zoom_sym_poly(_s, _d, 3u, FZS_OUT);
    fzs_tint(_s, FZS_AMBER, FZS_DARK, (f32)(0.06 * FZS_R(_s)));
    const f64 _bar[4] = { 0.0, 0.45, 0.0, -0.3 };
    fzs_stroke(_s, _bar, 2u, FZS_DARK, 0.15);
    fzs_round(_s, 0.0, -_s->hh * 0.6, FZS_R(_s) * 0.09, FZS_SOLID);
    fzs_tint(_s, FZS_DARK, FZS_DARK, 0.0f);
}
RDE_INTERNAL void fzs_stamp_question(fude_zoom_sym* _s) {
    fude_zoom_sym_begin(_s);
    const f64 _r = 0.42;
    for(u32 _i = 0; _i <= 20u; _i++) {
        const f64 _a = FUDE_ZOOM_SYMBOL_PI * (1.15 - 1.55 * (f64)_i / 20.0);
        fude_zoom_sym_uv(_s, cos(_a) * _r * 1.2, 0.42 + sin(_a) * _r);
    }
    fude_zoom_sym_uv(_s, 0.0, -0.05);
    fude_zoom_sym_uv(_s, 0.0, -0.3);
    fude_zoom_sym_end(_s, 0u);
    fzs_tint(_s, FZS_NONE, FZS_BLUE, (f32)(0.17 * FZS_R(_s)));
    fzs_round(_s, 0.0, -_s->hh * 0.72, FZS_R(_s) * 0.13, FZS_SOLID);
    fzs_tint(_s, FZS_BLUE, FZS_BLUE, 0.0f);
}
RDE_INTERNAL void fzs_stamp_exclamation(fude_zoom_sym* _s) {
    const f64 _bar[4] = { 0.0, 0.85, 0.0, -0.3 };
    fzs_stroke(_s, _bar, 2u, FZS_RED, 0.4);
    fzs_round(_s, 0.0, -_s->hh * 0.72, FZS_R(_s) * 0.13, FZS_SOLID);
    fzs_tint(_s, FZS_RED, FZS_RED, 0.0f);
}
RDE_INTERNAL void fzs_stamp_info(fude_zoom_sym* _s) {
    fzs_round(_s, 0.0, 0.0, FZS_R(_s), FZS_OUT);
    fzs_tint(_s, FZS_BLUE, FZS_BLUE, 0.0f);
    const f64 _bar[4] = { 0.0, 0.15, 0.0, -0.6 };
    fzs_stroke(_s, _bar, 2u, FZS_WHITE, 0.16);
    fzs_round(_s, 0.0, _s->hh * 0.45, FZS_R(_s) * 0.11, FZS_SOLID);
    fzs_tint(_s, FZS_WHITE, FZS_WHITE, 0.0f);
}
RDE_INTERNAL void fzs_stamp_star(fude_zoom_sym* _s) {
    fzs_star_of(_s, 5u, 0.42);
    fzs_tint(_s, FZS_GOLD, (rde_color){ 210, 150, 30, 255 }, 0.0f);
}
RDE_INTERNAL void fzs_stamp_heart(fude_zoom_sym* _s) {
    fzs_heart(_s);
    fzs_tint(_s, FZS_RED, FZS_RED, 0.0f);
}
RDE_INTERNAL void fzs_stamp_bulb(fude_zoom_sym* _s) {
    // Its glass round over its neck, and its base's bands.
    const f64 _r = fmin(_s->hw * 0.62, _s->hh * 0.5), _cy = _s->hh - _r - _s->hh * 0.08, _neck = _r * 0.45;
    const f64 _a = asin(_neck / _r);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_neck, -_s->hh * 0.35);
    fude_zoom_sym_arc_pts(_s, 0.0, _cy, _r, _r, FUDE_ZOOM_SYMBOL_PI * 1.5 - (FUDE_ZOOM_SYMBOL_PI * 0.5 - _a) * 0.6, -FUDE_ZOOM_SYMBOL_PI * 0.5 + (FUDE_ZOOM_SYMBOL_PI * 0.5 - _a) * 0.6);
    fude_zoom_sym_pt(_s, _neck, -_s->hh * 0.35);
    fude_zoom_sym_end(_s, FZS_OUT);
    fzs_tint(_s, (rde_color){ 252, 225, 100, 255 }, FZS_DARK, 0.0f);
    for(u32 _i = 0; _i < 3u; _i++) {
        const f64 _y = -_s->hh * (0.47 + 0.15 * (f64)_i);
        fude_zoom_sym_begin(_s);
        fude_zoom_sym_pt(_s, -_neck, _y);
        fude_zoom_sym_pt(_s, _neck, _y);
        fude_zoom_sym_end(_s, 0u);
        fzs_tint(_s, FZS_NONE, (rde_color){ 120, 120, 128, 255 }, (f32)(0.07 * FZS_R(_s)));
    }
}
RDE_INTERNAL void fzs_stamp_flag(fude_zoom_sym* _s) {
    fude_zoom_sym_line(_s, -0.8, -1.0, -0.8, 1.0, 0u);
    fzs_tint(_s, FZS_NONE, FZS_NONE, (f32)(0.07 * FZS_R(_s)));   // (its pole in its ink: seen on any paper)
    const f64 _d[8] = { -0.8, 1.0, 0.9, 0.62, -0.8, 0.2, -0.8, 1.0 };
    fude_zoom_sym_poly(_s, _d, 3u, FZS_OUT);
    fzs_tint(_s, FZS_RED, FZS_RED, 0.0f);
}
RDE_INTERNAL void fzs_stamp_pin(fude_zoom_sym* _s) {
    // A drop upside down: a round over a point; a hole in it.
    const f64 _r = fmin(_s->hw, _s->hh * 0.62), _cy = _s->hh - _r;
    const f64 _a = asin(fmin(_r / (_cy + _s->hh), 1.0));
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, 0.0, -_s->hh);
    fude_zoom_sym_arc_pts(_s, 0.0, _cy, _r, _r, -_a, FUDE_ZOOM_SYMBOL_PI + _a);
    fude_zoom_sym_end(_s, FZS_OUT);
    fzs_tint(_s, FZS_RED, FZS_RED, 0.0f);
    fzs_round(_s, 0.0, _cy, _r * 0.38, FZS_SOLID);
    fzs_tint(_s, FZS_WHITE, FZS_WHITE, 0.0f);
}
RDE_INTERNAL void fzs_stamp_clock(fude_zoom_sym* _s) {
    const f64 _r = FZS_R(_s);
    fzs_round(_s, 0.0, 0.0, _r, FZS_OUT);
    for(u32 _i = 0; _i < 12u; _i++) {
        const f64 _a = 2.0 * FUDE_ZOOM_SYMBOL_PI * (f64)_i / 12.0;
        fude_zoom_sym_begin(_s);
        fude_zoom_sym_pt(_s, cos(_a) * _r * 0.85, sin(_a) * _r * 0.85);
        fude_zoom_sym_pt(_s, cos(_a) * _r * 0.95, sin(_a) * _r * 0.95);
        fude_zoom_sym_end(_s, 0u);
    }
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, 0.0, _r * 0.6); fude_zoom_sym_pt(_s, 0.0, 0.0); fude_zoom_sym_pt(_s, _r * 0.42, -_r * 0.2);
    fude_zoom_sym_end(_s, 0u);
    fzs_tint(_s, FZS_NONE, FZS_NONE, (f32)(0.07 * _r));
}

// Brackets (0.1.85): each drawn across from its back (its spine, the side a "(" bulges to) to its front (its open
// side) — t from 0 to 1 — and _h each way along it from its middle; lying along x (_across: under or over a row).
typedef enum {
    FZB_ROUND = 0, FZB_SQUARE, FZB_CURLY, FZB_ANGLE, FZB_BAR, FZB_DOUBLE, FZB_FLOOR, FZB_CEILING,
} FZB_;

typedef struct {
    fude_zoom_sym* s;
    f64            back, front;   // where t is 0 and 1 (x; y when it lies across)
    f64            h;
    b8             across;
} fzs_bk;

RDE_INTERNAL void fzs_bk_pt(const fzs_bk* _k, f64 _t, f64 _along) {
    const f64 _c = _k->back + (_k->front - _k->back) * _t;
    if(_k->across) {
        fude_zoom_sym_pt(_k->s, _along, _c);
    } else {
        fude_zoom_sym_pt(_k->s, _c, _along);
    }
}

RDE_INTERNAL void fzs_bk_line(const fzs_bk* _k, f64 _t0, f64 _a0, f64 _t1, f64 _a1) {
    fude_zoom_sym_begin(_k->s);
    fzs_bk_pt(_k, _t0, _a0);
    fzs_bk_pt(_k, _t1, _a1);
    fude_zoom_sym_end(_k->s, 0u);
}

// A quarter of an ellipse round (t, along) _rt across and _ra along, from angle _a0 to _a1 (_n steps; its first point
// left out when _from is 1: where the one before it ended).
RDE_INTERNAL void fzs_bk_arc(const fzs_bk* _k, f64 _t, f64 _along, f64 _rt, f64 _ra, f64 _a0, f64 _a1, u32 _n, u32 _from) {
    for(u32 _i = _from; _i <= _n; _i++) {
        const f64 _a = _a0 + (_a1 - _a0) * (f64)_i / (f64)_n;
        fzs_bk_pt(_k, _t + cos(_a) * _rt, _along + sin(_a) * _ra);
    }
}

RDE_INTERNAL void fzs_bracket(fude_zoom_sym* _s, u8 _kind, f64 _back, f64 _front, f64 _h, b8 _across) {
    const fzs_bk _k = { _s, _back, _front, _h, _across };
    const f64 _pi = FUDE_ZOOM_SYMBOL_PI, _h2 = _pi * 0.5;
    const f64 _wide = fabs(_front - _back);
    const u32 _n = _s->segments / 4u < 6u ? 6u : _s->segments / 4u;
    switch(_kind) {
        case FZB_ROUND: {
            // An arc of a circle's sixth, as wide as it is (the taller, the flatter: as a typeset one stretches).
            const f64 _a = _pi / 3.0;
            fude_zoom_sym_begin(_s);
            for(u32 _i = 0; _i <= 2u * _n; _i++) {
                const f64 _p = -_a + 2.0 * _a * (f64)_i / (f64)(2u * _n);
                fzs_bk_pt(&_k, (1.0 - cos(_p)) / (1.0 - cos(_a)), _h * sin(_p) / sin(_a));
            }
            fude_zoom_sym_end(_s, 0u);
            break;
        }
        case FZB_SQUARE:
            fude_zoom_sym_begin(_s);
            fzs_bk_pt(&_k, 1.0, _h); fzs_bk_pt(&_k, 0.0, _h); fzs_bk_pt(&_k, 0.0, -_h); fzs_bk_pt(&_k, 1.0, -_h);
            fude_zoom_sym_end(_s, 0u);
            break;
        case FZB_CURLY: {
            // Its spine halfway across; its curls as round as its width lets them, however long it is.
            const f64 _ra = fmin(_wide * 0.7, _h * 0.25);
            fude_zoom_sym_begin(_s);
            fzs_bk_arc(&_k, 1.0, _h - _ra, 0.5, _ra, _h2, _pi, _n, 0u);
            fzs_bk_arc(&_k, 0.0, _ra, 0.5, _ra, 0.0, -_h2, _n, 0u);
            fzs_bk_arc(&_k, 0.0, -_ra, 0.5, _ra, _h2, 0.0, _n, 1u);
            fzs_bk_arc(&_k, 1.0, -_h + _ra, 0.5, _ra, _pi, _pi + _h2, _n, 0u);
            fude_zoom_sym_end(_s, 0u);
            break;
        }
        case FZB_ANGLE:
            fude_zoom_sym_begin(_s);
            fzs_bk_pt(&_k, 1.0, _h); fzs_bk_pt(&_k, 0.0, 0.0); fzs_bk_pt(&_k, 1.0, -_h);
            fude_zoom_sym_end(_s, 0u);
            break;
        case FZB_BAR:
            fzs_bk_line(&_k, 0.5, _h, 0.5, -_h);
            break;
        case FZB_DOUBLE:
            fzs_bk_line(&_k, 0.2, _h, 0.2, -_h);
            fzs_bk_line(&_k, 0.8, _h, 0.8, -_h);
            break;
        case FZB_FLOOR:
            fude_zoom_sym_begin(_s);
            fzs_bk_pt(&_k, 0.0, _h); fzs_bk_pt(&_k, 0.0, -_h); fzs_bk_pt(&_k, 1.0, -_h);
            fude_zoom_sym_end(_s, 0u);
            break;
        case FZB_CEILING:
            fude_zoom_sym_begin(_s);
            fzs_bk_pt(&_k, 1.0, _h); fzs_bk_pt(&_k, 0.0, _h); fzs_bk_pt(&_k, 0.0, -_h);
            fude_zoom_sym_end(_s, 0u);
            break;
        default:
            break;
    }
}

// One on its own, as tall as its box, a little in from its sides: its back on the left (a "("), or on the right.
RDE_INTERNAL void fzs_bracket_left(fude_zoom_sym* _s, u8 _kind)  { fzs_bracket(_s, _kind, -_s->hw * 0.7, _s->hw * 0.7, _s->hh, false); }
RDE_INTERNAL void fzs_bracket_right(fude_zoom_sym* _s, u8 _kind) { fzs_bracket(_s, _kind, _s->hw * 0.7, -_s->hw * 0.7, _s->hh, false); }
RDE_INTERNAL void fzs_paren_l(fude_zoom_sym* _s)   { fzs_bracket_left(_s, FZB_ROUND); }
RDE_INTERNAL void fzs_paren_r(fude_zoom_sym* _s)   { fzs_bracket_right(_s, FZB_ROUND); }
RDE_INTERNAL void fzs_square_l(fude_zoom_sym* _s)  { fzs_bracket_left(_s, FZB_SQUARE); }
RDE_INTERNAL void fzs_square_r(fude_zoom_sym* _s)  { fzs_bracket_right(_s, FZB_SQUARE); }
RDE_INTERNAL void fzs_curly_l(fude_zoom_sym* _s)   { fzs_bracket_left(_s, FZB_CURLY); }
RDE_INTERNAL void fzs_curly_r(fude_zoom_sym* _s)   { fzs_bracket_right(_s, FZB_CURLY); }
RDE_INTERNAL void fzs_angle_l(fude_zoom_sym* _s)   { fzs_bracket_left(_s, FZB_ANGLE); }
RDE_INTERNAL void fzs_angle_r(fude_zoom_sym* _s)   { fzs_bracket_right(_s, FZB_ANGLE); }
RDE_INTERNAL void fzs_vbar(fude_zoom_sym* _s)      { fzs_bracket_left(_s, FZB_BAR); }
RDE_INTERNAL void fzs_double_bar(fude_zoom_sym* _s) { fzs_bracket_left(_s, FZB_DOUBLE); }
RDE_INTERNAL void fzs_floor_l(fude_zoom_sym* _s)   { fzs_bracket_left(_s, FZB_FLOOR); }
RDE_INTERNAL void fzs_floor_r(fude_zoom_sym* _s)   { fzs_bracket_right(_s, FZB_FLOOR); }
RDE_INTERNAL void fzs_ceiling_l(fude_zoom_sym* _s) { fzs_bracket_left(_s, FZB_CEILING); }
RDE_INTERNAL void fzs_ceiling_r(fude_zoom_sym* _s) { fzs_bracket_right(_s, FZB_CEILING); }
// A brace under a row (its point down, to its text under it), or over one.
RDE_INTERNAL void fzs_brace_under(fude_zoom_sym* _s) { fzs_bracket(_s, FZB_CURLY, -_s->hh * 0.7, _s->hh * 0.7, _s->hw, true); }
RDE_INTERNAL void fzs_brace_over(fude_zoom_sym* _s)  { fzs_bracket(_s, FZB_CURLY, _s->hh * 0.7, -_s->hh * 0.7, _s->hw, true); }

// A pair round a text: each as wide as its height asks (a typeset one's), never more than a third of the box.
RDE_INTERNAL f64 fzs_pair_width(f64 _hw, f64 _hh) {
    return fmin(_hw * 0.3, fmax(_hh * 0.22, 6.0));
}
RDE_INTERNAL void fzs_pair(fude_zoom_sym* _s, u8 _kind) {
    const f64 _bw = fzs_pair_width(_s->hw, _s->hh);
    fzs_bracket(_s, _kind, -_s->hw, -_s->hw + _bw, _s->hh, false);
    fzs_bracket(_s, _kind, _s->hw, _s->hw - _bw, _s->hh, false);
}
RDE_INTERNAL void fzs_pair_round(fude_zoom_sym* _s)  { fzs_pair(_s, FZB_ROUND); }
RDE_INTERNAL void fzs_pair_square(fude_zoom_sym* _s) { fzs_pair(_s, FZB_SQUARE); }
RDE_INTERNAL void fzs_pair_curly(fude_zoom_sym* _s)  { fzs_pair(_s, FZB_CURLY); }
RDE_INTERNAL void fzs_pair_angle(fude_zoom_sym* _s)  { fzs_pair(_s, FZB_ANGLE); }
RDE_INTERNAL void fzs_pair_bar(fude_zoom_sym* _s)    { fzs_pair(_s, FZB_BAR); }
RDE_INTERNAL void fzs_pair_double(fude_zoom_sym* _s) { fzs_pair(_s, FZB_DOUBLE); }

#define FZS(_id, _name, _fam, _w, _h, _place, _boxes, _hull, _fn) \
    { { _id, FUDE_ZOOM_SYMBOL_FAMILY_##_fam, _w, _h, FUDE_ZOOM_SYMBOL_TEXT_##_place, _boxes, _hull }, _fn }   // (_name: page.c's, in the same order)

// (Append-only: a symbol's kind is its place here, kept in the canvases.)
RDE_INTERNAL const fude_zoom_sym_entry FUDE_ZOOM_SYMBOLS[] = {
    // Flowchart and basic.
    FZS("process",          ZOOM_SYM_PROCESS,        FLOW, 120, 70, INSIDE, 1, false, fzs_process),
    FZS("alternate",        ZOOM_SYM_ALTERNATE,      FLOW, 120, 70, INSIDE, 1, false, fzs_rounded),
    FZS("decision",         ZOOM_SYM_DECISION,       FLOW, 120, 84, INSIDE, 1, false, fzs_decision),
    FZS("terminator",       ZOOM_SYM_TERMINATOR,     FLOW, 120, 56, INSIDE, 1, false, fzs_terminator),
    FZS("data",             ZOOM_SYM_DATA,           FLOW, 120, 70, INSIDE, 1, false, fzs_data),
    FZS("predefined",       ZOOM_SYM_PREDEFINED,     FLOW, 130, 70, INSIDE, 1, false, fzs_predefined),
    FZS("document",         ZOOM_SYM_DOCUMENT,       FLOW, 120, 76, INSIDE, 1, false, fzs_document),
    FZS("documents",        ZOOM_SYM_DOCUMENTS,      FLOW, 124, 84, INSIDE, 1, false, fzs_documents),
    FZS("manual input",     ZOOM_SYM_MANUAL_INPUT,   FLOW, 120, 70, INSIDE, 1, false, fzs_manual_input),
    FZS("manual operation", ZOOM_SYM_MANUAL_OP,      FLOW, 120, 70, INSIDE, 1, false, fzs_manual_op),
    FZS("preparation",      ZOOM_SYM_PREPARATION,    FLOW, 130, 70, INSIDE, 1, false, fzs_preparation),
    FZS("delay",            ZOOM_SYM_DELAY,          FLOW, 110, 70, INSIDE, 1, false, fzs_delay),
    FZS("stored data",      ZOOM_SYM_STORED_DATA,    FLOW, 120, 70, INSIDE, 1, false, fzs_stored_data),
    FZS("internal storage", ZOOM_SYM_INTERNAL,       FLOW, 100, 84, INSIDE, 1, false, fzs_internal_storage),
    FZS("display",          ZOOM_SYM_DISPLAY,        FLOW, 130, 70, INSIDE, 1, false, fzs_display),
    FZS("off-page",         ZOOM_SYM_OFFPAGE,        FLOW, 64, 70,  INSIDE, 1, false, fzs_offpage),
    FZS("connector",        ZOOM_SYM_CONNECTOR,      FLOW, 50, 50,  INSIDE, 1, false, fzs_connector),
    FZS("merge",            ZOOM_SYM_MERGE,          FLOW, 84, 70,  INSIDE, 1, false, fzs_merge),
    FZS("extract",          ZOOM_SYM_EXTRACT,        FLOW, 84, 70,  INSIDE, 1, false, fzs_extract),
    FZS("or",               ZOOM_SYM_OR,             FLOW, 50, 50,  BELOW,  1, false, fzs_or),
    FZS("summing junction", ZOOM_SYM_SUMMING,        FLOW, 50, 50,  BELOW,  1, false, fzs_summing),
    FZS("collate",          ZOOM_SYM_COLLATE,        FLOW, 64, 76,  BELOW,  1, false, fzs_collate),
    FZS("sort",             ZOOM_SYM_SORT,           FLOW, 76, 76,  BELOW,  1, false, fzs_sort),
    FZS("annotation",       ZOOM_SYM_ANNOTATION,     FLOW, 130, 60, INSIDE, 1, true,  fzs_annotation),
    FZS("card",             ZOOM_SYM_CARD,           FLOW, 116, 70, INSIDE, 1, false, fzs_card),
    FZS("punched tape",     ZOOM_SYM_TAPE,           FLOW, 120, 70, INSIDE, 1, false, fzs_tape),
    FZS("loop limit",       ZOOM_SYM_LOOP_LIMIT,     FLOW, 120, 60, INSIDE, 1, false, fzs_loop_limit),
    FZS("triangle",         ZOOM_SYM_TRIANGLE,       FLOW, 90, 80,  INSIDE, 1, false, fzs_triangle),
    FZS("hexagon",          ZOOM_SYM_HEXAGON,        FLOW, 110, 90, INSIDE, 1, false, fzs_hexagon),
    FZS("ellipse",          ZOOM_SYM_ELLIPSE,        FLOW, 120, 76, INSIDE, 1, false, fzs_ellipse),
    FZS("callout",          ZOOM_SYM_CALLOUT,        FLOW, 140, 90, INSIDE, 1, false, fzs_callout),
    // UML structure.
    FZS("class",            ZOOM_SYM_CLASS,          UML, 180, 130, PARTS, 3, false, fzs_class),
    FZS("interface",        ZOOM_SYM_INTERFACE,      UML, 40, 60,   BELOW, 1, true,  fzs_interface),
    FZS("required",         ZOOM_SYM_REQUIRED,       UML, 40, 60,   BELOW, 1, true,  fzs_socket),
    FZS("package",          ZOOM_SYM_PACKAGE,        UML, 170, 120, INSIDE, 1, false, fzs_package),
    FZS("note",             ZOOM_SYM_NOTE,           UML, 150, 90,  INSIDE, 1, false, fzs_note),
    FZS("component",        ZOOM_SYM_COMPONENT,      UML, 170, 90,  INSIDE, 1, false, fzs_component),
    FZS("node",             ZOOM_SYM_NODE,           UML, 160, 110, INSIDE, 1, false, fzs_node),
    FZS("artifact",         ZOOM_SYM_ARTIFACT,       UML, 150, 80,  INSIDE, 1, false, fzs_artifact),
    FZS("actor",            ZOOM_SYM_ACTOR,          UML, 46, 90,   BELOW, 1, true,  fzs_actor),
    FZS("use case",         ZOOM_SYM_USE_CASE,       UML, 150, 70,  INSIDE, 1, false, fzs_use_case),
    FZS("system boundary",  ZOOM_SYM_BOUNDARY,       UML, 280, 340, HEADER, 1, false, fzs_boundary),
    FZS("object",           ZOOM_SYM_OBJECT,         UML, 170, 90,  PARTS, 2, false, fzs_class),
    FZS("enumeration",      ZOOM_SYM_ENUM,           UML, 170, 110, PARTS, 2, false, fzs_class),
    // UML behaviour.
    FZS("lifeline",         ZOOM_SYM_LIFELINE,       BEHAVIOUR, 120, 320, HEADER, 1, true, fzs_lifeline),
    FZS("activation",       ZOOM_SYM_ACTIVATION,     BEHAVIOUR, 16, 120,  BELOW, 1, false, fzs_activation),
    FZS("destroy",          ZOOM_SYM_DESTROY,        BEHAVIOUR, 36, 36,   BELOW, 1, true,  fzs_destroy),
    FZS("fragment",         ZOOM_SYM_FRAGMENT,       BEHAVIOUR, 300, 200, HEADER, 1, false, fzs_fragment),
    FZS("initial",          ZOOM_SYM_INITIAL,        BEHAVIOUR, 30, 30,   BELOW, 1, false, fzs_initial),
    FZS("final",            ZOOM_SYM_FINAL,          BEHAVIOUR, 34, 34,   BELOW, 1, false, fzs_final),
    FZS("flow final",       ZOOM_SYM_FLOW_FINAL,     BEHAVIOUR, 34, 34,   BELOW, 1, false, fzs_flow_final),
    FZS("fork / join",      ZOOM_SYM_FORK,           BEHAVIOUR, 170, 10,  BELOW, 1, false, fzs_bar),
    FZS("action",           ZOOM_SYM_ACTION,         BEHAVIOUR, 140, 64,  INSIDE, 1, false, fzs_action),
    FZS("state",            ZOOM_SYM_STATE,          BEHAVIOUR, 150, 90,  PARTS, 2, false, fzs_state),
    FZS("choice",           ZOOM_SYM_CHOICE,         BEHAVIOUR, 50, 50,   BELOW, 1, false, fzs_small_decision),
    FZS("send signal",      ZOOM_SYM_SEND,           BEHAVIOUR, 130, 60,  INSIDE, 1, false, fzs_send),
    FZS("accept signal",    ZOOM_SYM_RECEIVE,        BEHAVIOUR, 130, 60,  INSIDE, 1, false, fzs_receive),
    FZS("time event",       ZOOM_SYM_TIME_EVENT,     BEHAVIOUR, 36, 46,   BELOW, 1, false, fzs_hourglass),
    FZS("history",          ZOOM_SYM_HISTORY,        BEHAVIOUR, 36, 36,   BELOW, 1, false, fzs_history),
    FZS("swimlane",         ZOOM_SYM_SWIMLANE,       BEHAVIOUR, 220, 420, HEADER, 1, false, fzs_vlane),
    // Data.
    FZS("database",         ZOOM_SYM_DATABASE,       DATA, 90, 110,  INSIDE, 1, false, fzs_cylinder),
    FZS("table",            ZOOM_SYM_TABLE,          DATA, 170, 150, PARTS, 2, false, fzs_table),
    FZS("entity",           ZOOM_SYM_ENTITY,         DATA, 130, 70,  INSIDE, 1, false, fzs_entity),
    FZS("weak entity",      ZOOM_SYM_WEAK_ENTITY,    DATA, 140, 74,  INSIDE, 1, false, fzs_weak_entity),
    FZS("relationship",     ZOOM_SYM_RELATIONSHIP,   DATA, 130, 84,  INSIDE, 1, false, fzs_relationship),
    FZS("weak relationship", ZOOM_SYM_WEAK_RELATION, DATA, 140, 90,  INSIDE, 1, false, fzs_weak_relationship),
    FZS("attribute",        ZOOM_SYM_ATTRIBUTE,      DATA, 120, 60,  INSIDE, 1, false, fzs_attribute),
    FZS("multivalued",      ZOOM_SYM_MULTIVALUED,    DATA, 130, 66,  INSIDE, 1, false, fzs_multivalued),
    FZS("derived",          ZOOM_SYM_DERIVED,        DATA, 120, 60,  INSIDE, 1, false, fzs_derived),
    FZS("file",             ZOOM_SYM_FILE,           DATA, 60, 76,   BELOW, 1, false, fzs_file),
    FZS("storage",          ZOOM_SYM_STORAGE,        DATA, 90, 110,  BELOW, 1, false, fzs_disks),
    // Architecture.
    FZS("person",           ZOOM_SYM_PERSON,         ARCHITECTURE, 120, 150, INSIDE, 1, false, fzs_person),
    FZS("software system",  ZOOM_SYM_SYSTEM,         ARCHITECTURE, 180, 110, INSIDE, 1, false, fzs_system),
    FZS("external system",  ZOOM_SYM_EXTERNAL,       ARCHITECTURE, 180, 110, INSIDE, 1, false, fzs_external),
    FZS("boundary",         ZOOM_SYM_C4_BOUNDARY,    ARCHITECTURE, 360, 260, HEADER, 1, false, fzs_c4_boundary),
    FZS("server",           ZOOM_SYM_SERVER,         ARCHITECTURE, 70, 96,   BELOW, 1, false, fzs_server),
    FZS("desktop",          ZOOM_SYM_DESKTOP,        ARCHITECTURE, 96, 80,   BELOW, 1, false, fzs_desktop),
    FZS("laptop",           ZOOM_SYM_LAPTOP,         ARCHITECTURE, 104, 70,  BELOW, 1, false, fzs_laptop),
    FZS("phone",            ZOOM_SYM_PHONE,          ARCHITECTURE, 46, 86,   BELOW, 1, false, fzs_phone),
    FZS("tablet",           ZOOM_SYM_TABLET,         ARCHITECTURE, 70, 92,   BELOW, 1, false, fzs_tablet),
    FZS("browser",          ZOOM_SYM_BROWSER,        ARCHITECTURE, 150, 104, INSIDE, 1, false, fzs_browser),
    FZS("cloud",            ZOOM_SYM_CLOUD,          ARCHITECTURE, 150, 96,  INSIDE, 1, false, fzs_cloud),
    FZS("queue",            ZOOM_SYM_QUEUE,          ARCHITECTURE, 150, 56,  BELOW, 1, false, fzs_queue),
    FZS("cache",            ZOOM_SYM_CACHE,          ARCHITECTURE, 80, 100,  BELOW, 1, false, fzs_cache),
    FZS("load balancer",    ZOOM_SYM_BALANCER,       ARCHITECTURE, 70, 70,   BELOW, 1, false, fzs_balancer),
    FZS("API gateway",      ZOOM_SYM_GATEWAY,        ARCHITECTURE, 84, 70,   BELOW, 1, false, fzs_gateway),
    FZS("microservice",     ZOOM_SYM_MICROSERVICE,   ARCHITECTURE, 110, 96,  INSIDE, 1, false, fzs_microservice),
    FZS("function",         ZOOM_SYM_FUNCTION,       ARCHITECTURE, 66, 66,   BELOW, 1, false, fzs_function),
    FZS("container",        ZOOM_SYM_CONTAINER,      ARCHITECTURE, 110, 64,  BELOW, 1, false, fzs_container),
    FZS("pod",              ZOOM_SYM_POD,            ARCHITECTURE, 76, 76,   BELOW, 1, false, fzs_pod),
    FZS("firewall",         ZOOM_SYM_FIREWALL,       ARCHITECTURE, 80, 70,   BELOW, 1, false, fzs_firewall),
    FZS("bucket",           ZOOM_SYM_BUCKET,         ARCHITECTURE, 70, 80,   BELOW, 1, false, fzs_bucket),
    FZS("globe",            ZOOM_SYM_GLOBE,          ARCHITECTURE, 70, 70,   BELOW, 1, false, fzs_globe),
    FZS("router",           ZOOM_SYM_ROUTER,         ARCHITECTURE, 70, 70,   BELOW, 1, false, fzs_router),
    FZS("switch",           ZOOM_SYM_SWITCH,         ARCHITECTURE, 100, 52,  BELOW, 1, false, fzs_switch),
    FZS("user",             ZOOM_SYM_USER,           ARCHITECTURE, 60, 70,   BELOW, 1, true,  fzs_user),
    FZS("event bus",        ZOOM_SYM_BUS,            ARCHITECTURE, 260, 44,  INSIDE, 1, false, fzs_bus),
    FZS("security",         ZOOM_SYM_LOCK,           ARCHITECTURE, 50, 64,   BELOW, 1, false, fzs_lock),
    FZS("scheduler",        ZOOM_SYM_CLOCK,          ARCHITECTURE, 64, 64,   BELOW, 1, false, fzs_clock),
    FZS("service",          ZOOM_SYM_GEAR,           ARCHITECTURE, 68, 68,   BELOW, 1, false, fzs_gear),
    FZS("folder",           ZOOM_SYM_FOLDER,         ARCHITECTURE, 76, 58,   BELOW, 1, false, fzs_folder),
    FZS("monitoring",       ZOOM_SYM_CHART,          ARCHITECTURE, 96, 70,   BELOW, 1, false, fzs_chart),
    FZS("container database", ZOOM_SYM_CONTAINER_DB, ARCHITECTURE, 120, 130, INSIDE, 1, false, fzs_container_db),
    FZS("internet",         ZOOM_SYM_INTERNET,       ARCHITECTURE, 130, 84,  INSIDE, 1, false, fzs_internet),
    // BPMN.
    FZS("start event",      ZOOM_SYM_START_EVENT,    BPMN, 40, 40,   BELOW, 1, false, fzs_start_event),
    FZS("intermediate event", ZOOM_SYM_INTERMEDIATE, BPMN, 40, 40,   BELOW, 1, false, fzs_intermediate),
    FZS("end event",        ZOOM_SYM_END_EVENT,      BPMN, 40, 40,   BELOW, 1, false, fzs_end_event),
    FZS("timer event",      ZOOM_SYM_TIMER_EVENT,    BPMN, 40, 40,   BELOW, 1, false, fzs_timer_event),
    FZS("message event",    ZOOM_SYM_MESSAGE_EVENT,  BPMN, 40, 40,   BELOW, 1, false, fzs_message_event),
    FZS("task",             ZOOM_SYM_TASK,           BPMN, 130, 80,  INSIDE, 1, false, fzs_task),
    FZS("sub-process",      ZOOM_SYM_SUBPROCESS,     BPMN, 140, 90,  INSIDE, 1, false, fzs_subprocess),
    FZS("exclusive gateway", ZOOM_SYM_XOR_GATEWAY,   BPMN, 56, 56,   BELOW, 1, false, fzs_xor_gateway),
    FZS("parallel gateway", ZOOM_SYM_AND_GATEWAY,    BPMN, 56, 56,   BELOW, 1, false, fzs_and_gateway),
    FZS("inclusive gateway", ZOOM_SYM_OR_GATEWAY,    BPMN, 56, 56,   BELOW, 1, false, fzs_or_gateway),
    FZS("data object",      ZOOM_SYM_DATA_OBJECT,    BPMN, 46, 60,   BELOW, 1, false, fzs_data_object),
    FZS("data store",       ZOOM_SYM_DATA_STORE,     BPMN, 60, 60,   BELOW, 1, false, fzs_data_store),
    FZS("pool / lane",      ZOOM_SYM_POOL,           BPMN, 560, 170, LEFT, 1, false, fzs_pool),
    FZS("group",            ZOOM_SYM_GROUP,          BPMN, 240, 160, HEADER, 1, false, fzs_group),
    // Planning.
    FZS("column",           ZOOM_SYM_COLUMN,         PLANNING, 230, 520, HEADER, 1, false, fzs_column),
    FZS("task card",        ZOOM_SYM_TASK_CARD,      PLANNING, 190, 90,  INSIDE, 1, false, fzs_task_card),
    FZS("lane",             ZOOM_SYM_LANE,           PLANNING, 700, 160, LEFT, 1, false, fzs_hlane),
    FZS("milestone",        ZOOM_SYM_MILESTONE,      PLANNING, 28, 28,   BELOW, 1, false, fzs_milestone),
    FZS("2 × 2 matrix",     ZOOM_SYM_MATRIX,         PLANNING, 340, 340, PARTS, 4, false, fzs_matrix),
    FZS("arrow",            ZOOM_SYM_ARROW_BLOCK,    PLANNING, 150, 70,  INSIDE, 1, false, fzs_arrow_block),
    FZS("timeline",         ZOOM_SYM_TIMELINE,       PLANNING, 420, 30,  BELOW, 1, true,  fzs_timeline),
    // (Appended.)
    FZS("boundary object",  ZOOM_SYM_SEQ_BOUNDARY,   BEHAVIOUR, 70, 50,   BELOW, 1, false, fzs_seq_boundary),
    FZS("control object",   ZOOM_SYM_SEQ_CONTROL,    BEHAVIOUR, 50, 54,   BELOW, 1, false, fzs_seq_control),
    FZS("entity object",    ZOOM_SYM_SEQ_ENTITY,     BEHAVIOUR, 50, 54,   BELOW, 1, false, fzs_seq_entity),
    FZS("terminal",         ZOOM_SYM_TERMINAL,       ARCHITECTURE, 100, 70, BELOW, 1, false, fzs_terminal),
    FZS("key",              ZOOM_SYM_KEY,            ARCHITECTURE, 84, 36,  BELOW, 1, true,  fzs_key),
    FZS("envelope",         ZOOM_SYM_ENVELOPE,       ARCHITECTURE, 80, 54,  BELOW, 1, false, fzs_envelope),
    FZS("code",             ZOOM_SYM_CODE,           ARCHITECTURE, 70, 60,  BELOW, 1, false, fzs_code),
    FZS("users",            ZOOM_SYM_USERS,          ARCHITECTURE, 80, 70,  BELOW, 1, true,  fzs_users),
    FZS("event topic",      ZOOM_SYM_TOPIC,          ARCHITECTURE, 150, 70, BELOW, 1, false, fzs_topic),
    FZS("brace",            ZOOM_SYM_BRACE,          FLOW, 40, 120,  LEFT, 1, true,  fzs_brace),
    FZS("DFD data store",   ZOOM_SYM_DFD_STORE,      DATA, 150, 50,  INSIDE, 1, true,  fzs_dfd_store),
    FZS("area",             ZOOM_SYM_AREA,           PLANNING, 600, 400, CORNER, 1, false, fzs_area),   // (made by Insert's Area, not the library's)
    // Electronics, logic, chips and modules, boards (circuit.h).
    FZS("resistor",            ZOOM_SYM_E_RESISTOR,    ELECTRONICS, 60, 20, BELOW, 1, false, fzs_part_0),
    FZS("potentiometer",       ZOOM_SYM_E_POT,         ELECTRONICS, 60, 40, BELOW, 1, false, fzs_part_1),
    FZS("capacitor",           ZOOM_SYM_E_CAP,         ELECTRONICS, 40, 40, BELOW, 1, false, fzs_part_2),
    FZS("electrolytic",        ZOOM_SYM_E_ECAP,        ELECTRONICS, 40, 40, BELOW, 1, false, fzs_part_3),
    FZS("inductor",            ZOOM_SYM_E_IND,         ELECTRONICS, 60, 20, BELOW, 1, false, fzs_part_4),
    FZS("diode",               ZOOM_SYM_E_DIODE,       ELECTRONICS, 60, 30, BELOW, 1, false, fzs_part_5),
    FZS("LED",                 ZOOM_SYM_E_LED,         ELECTRONICS, 60, 40, BELOW, 1, false, fzs_part_6),
    FZS("zener",               ZOOM_SYM_E_ZENER,       ELECTRONICS, 60, 30, BELOW, 1, false, fzs_part_7),
    FZS("NPN",                 ZOOM_SYM_E_NPN,         ELECTRONICS, 60, 60, BELOW, 1, false, fzs_part_8),
    FZS("PNP",                 ZOOM_SYM_E_PNP,         ELECTRONICS, 60, 60, BELOW, 1, false, fzs_part_9),
    FZS("N-MOSFET",            ZOOM_SYM_E_NMOS,        ELECTRONICS, 60, 60, BELOW, 1, false, fzs_part_10),
    FZS("P-MOSFET",            ZOOM_SYM_E_PMOS,        ELECTRONICS, 60, 60, BELOW, 1, false, fzs_part_11),
    FZS("DC source",           ZOOM_SYM_E_VSRC,        ELECTRONICS, 40, 60, BELOW, 1, false, fzs_part_12),
    FZS("battery",             ZOOM_SYM_E_BATT,        ELECTRONICS, 40, 60, BELOW, 1, false, fzs_part_13),
    FZS("AC source",           ZOOM_SYM_E_AC,          ELECTRONICS, 40, 60, BELOW, 1, false, fzs_part_14),
    FZS("clock",               ZOOM_SYM_E_CLOCK,       ELECTRONICS, 40, 60, BELOW, 1, false, fzs_part_15),
    FZS("current source",      ZOOM_SYM_E_ISRC,        ELECTRONICS, 40, 60, BELOW, 1, false, fzs_part_16),
    FZS("ground",              ZOOM_SYM_E_GND,         ELECTRONICS, 40, 40, BELOW, 1, false, fzs_part_17),
    FZS("supply rail",         ZOOM_SYM_E_RAIL,        ELECTRONICS, 60, 20, INSIDE, 1, false, fzs_part_18),
    FZS("SPST switch",         ZOOM_SYM_E_SWITCH,      ELECTRONICS, 60, 40, BELOW, 1, false, fzs_part_19),
    FZS("push button",         ZOOM_SYM_E_BUTTON,      ELECTRONICS, 60, 40, BELOW, 1, false, fzs_part_20),
    FZS("lamp",                ZOOM_SYM_E_LAMP,        ELECTRONICS, 40, 40, BELOW, 1, false, fzs_part_21),
    FZS("motor",               ZOOM_SYM_E_MOTOR,       ELECTRONICS, 40, 40, BELOW, 1, false, fzs_part_22),
    FZS("buzzer",              ZOOM_SYM_E_BUZZER,      ELECTRONICS, 60, 40, BELOW, 1, false, fzs_part_23),
    FZS("fuse",                ZOOM_SYM_E_FUSE,        ELECTRONICS, 60, 20, BELOW, 1, false, fzs_part_24),
    FZS("op-amp",              ZOOM_SYM_E_OPAMP,       ELECTRONICS, 80, 80, BELOW, 1, false, fzs_part_25),
    FZS("voltmeter",           ZOOM_SYM_E_VOLT,        ELECTRONICS, 40, 40, BELOW, 1, false, fzs_part_26),
    FZS("ammeter",             ZOOM_SYM_E_AMP,         ELECTRONICS, 40, 40, BELOW, 1, false, fzs_part_27),
    FZS("7-segment",           ZOOM_SYM_E_7SEG,        ELECTRONICS, 60, 120, BELOW, 1, false, fzs_part_28),
    FZS("breadboard",          ZOOM_SYM_E_BREADBOARD,  ELECTRONICS, 330, 185, BELOW, 1, true, fzs_part_29),
    FZS("AND gate",            ZOOM_SYM_L_AND,         LOGIC, 80, 60, BELOW, 1, false, fzs_part_30),
    FZS("OR gate",             ZOOM_SYM_L_OR,          LOGIC, 80, 60, BELOW, 1, false, fzs_part_31),
    FZS("NOT gate",            ZOOM_SYM_L_NOT,         LOGIC, 80, 60, BELOW, 1, false, fzs_part_32),
    FZS("NAND gate",           ZOOM_SYM_L_NAND,        LOGIC, 80, 60, BELOW, 1, false, fzs_part_33),
    FZS("NOR gate",            ZOOM_SYM_L_NOR,         LOGIC, 80, 60, BELOW, 1, false, fzs_part_34),
    FZS("XOR gate",            ZOOM_SYM_L_XOR,         LOGIC, 80, 60, BELOW, 1, false, fzs_part_35),
    FZS("XNOR gate",           ZOOM_SYM_L_XNOR,        LOGIC, 80, 60, BELOW, 1, false, fzs_part_36),
    FZS("buffer",              ZOOM_SYM_L_BUF,         LOGIC, 80, 60, BELOW, 1, false, fzs_part_37),
    FZS("D flip-flop",         ZOOM_SYM_L_DFF,         LOGIC, 80, 80, BELOW, 1, false, fzs_part_38),
    FZS("T flip-flop",         ZOOM_SYM_L_TFF,         LOGIC, 80, 80, BELOW, 1, false, fzs_part_39),
    FZS("logic input",         ZOOM_SYM_L_IN,          LOGIC, 60, 40, BELOW, 1, false, fzs_part_40),
    FZS("logic probe",         ZOOM_SYM_L_OUT,         LOGIC, 40, 40, BELOW, 1, false, fzs_part_41),
    FZS("NE555",               ZOOM_SYM_C_555,         CHIPS, 200, 100, INSIDE, 1, true, fzs_part_42),
    FZS("LM358",               ZOOM_SYM_C_LM358,       CHIPS, 200, 100, INSIDE, 1, true, fzs_part_43),
    FZS("ATmega328P",          ZOOM_SYM_C_328P,        CHIPS, 220, 300, INSIDE, 1, true, fzs_part_44),
    FZS("74HC595",             ZOOM_SYM_C_595,         CHIPS, 200, 180, INSIDE, 1, true, fzs_part_45),
    FZS("L293D",               ZOOM_SYM_C_293,         CHIPS, 200, 180, INSIDE, 1, true, fzs_part_46),
    FZS("ULN2003",             ZOOM_SYM_C_2003,        CHIPS, 200, 180, INSIDE, 1, true, fzs_part_47),
    FZS("74HC00",              ZOOM_SYM_C_7400,        CHIPS, 200, 160, INSIDE, 1, true, fzs_part_48),
    FZS("74HC08",              ZOOM_SYM_C_7408,        CHIPS, 200, 160, INSIDE, 1, true, fzs_part_49),
    FZS("74HC32",              ZOOM_SYM_C_7432,        CHIPS, 200, 160, INSIDE, 1, true, fzs_part_50),
    FZS("74HC86",              ZOOM_SYM_C_7486,        CHIPS, 200, 160, INSIDE, 1, true, fzs_part_51),
    FZS("74HC04",              ZOOM_SYM_C_7404,        CHIPS, 200, 160, INSIDE, 1, true, fzs_part_52),
    FZS("MAX7219",             ZOOM_SYM_C_7219,        CHIPS, 220, 260, INSIDE, 1, true, fzs_part_53),
    FZS("PCF8574",             ZOOM_SYM_C_8574,        CHIPS, 200, 180, INSIDE, 1, true, fzs_part_54),
    FZS("7805",                ZOOM_SYM_C_7805,        CHIPS, 80, 40, INSIDE, 1, true, fzs_part_55),
    FZS("AMS1117",             ZOOM_SYM_C_1117,        CHIPS, 100, 40, INSIDE, 1, true, fzs_part_56),
    FZS("DS3231",              ZOOM_SYM_M_DS3231,      CHIPS, 200, 140, INSIDE, 1, true, fzs_part_57),
    FZS("MPU6050",             ZOOM_SYM_M_MPU,         CHIPS, 200, 180, INSIDE, 1, true, fzs_part_58),
    FZS("nRF24L01",            ZOOM_SYM_M_NRF,         CHIPS, 200, 180, INSIDE, 1, true, fzs_part_59),
    FZS("HC-05",               ZOOM_SYM_M_HC05,        CHIPS, 200, 140, INSIDE, 1, true, fzs_part_60),
    FZS("HC-SR04",             ZOOM_SYM_M_SR04,        CHIPS, 200, 100, INSIDE, 1, true, fzs_part_61),
    FZS("DHT11",               ZOOM_SYM_M_DHT,         CHIPS, 120, 80, INSIDE, 1, true, fzs_part_62),
    FZS("servo",               ZOOM_SYM_M_SERVO,       CHIPS, 120, 80, BELOW, 1, true, fzs_part_63),   // (its name under it: its horn turns over its middle)
    FZS("TP4056",              ZOOM_SYM_M_TP4056,      CHIPS, 200, 140, INSIDE, 1, true, fzs_part_64),
    FZS("WS2812B",             ZOOM_SYM_M_WS2812,      CHIPS, 120, 100, INSIDE, 1, true, fzs_part_65),
    FZS("L298N",               ZOOM_SYM_M_L298N,       CHIPS, 200, 280, INSIDE, 1, true, fzs_part_66),
    FZS("relay module",        ZOOM_SYM_M_RELAY,       CHIPS, 200, 140, INSIDE, 1, true, fzs_part_67),
    FZS("I2C LCD",             ZOOM_SYM_M_LCD,         CHIPS, 200, 100, INSIDE, 1, true, fzs_part_68),
    FZS("OLED",                ZOOM_SYM_M_OLED,        CHIPS, 200, 100, INSIDE, 1, true, fzs_part_69),
    FZS("Arduino Uno",         ZOOM_SYM_B_UNO,         BOARDS, 200, 340, INSIDE, 1, true, fzs_part_70),
    FZS("Arduino Nano",        ZOOM_SYM_B_NANO,        BOARDS, 180, 320, INSIDE, 1, true, fzs_part_71),
    FZS("Raspberry Pi Pico",   ZOOM_SYM_B_PICO,        BOARDS, 200, 420, INSIDE, 1, true, fzs_part_72),
    FZS("Raspberry Pi GPIO",   ZOOM_SYM_B_PI,          BOARDS, 220, 420, INSIDE, 1, true, fzs_part_73),
    FZS("ESP32",               ZOOM_SYM_B_ESP32,       BOARDS, 200, 320, INSIDE, 1, true, fzs_part_74),
    FZS("ESP8266",             ZOOM_SYM_B_8266,        BOARDS, 200, 320, INSIDE, 1, true, fzs_part_75),
    // Mechanisms (mech.h).
    FZS("link",                ZOOM_SYM_M_LINK,        MECHANISMS, 120, 24, BELOW, 1, false, fzs_mech_0),
    FZS("plate",               ZOOM_SYM_M_PLATE,       MECHANISMS, 100, 90, BELOW, 1, false, fzs_mech_1),
    FZS("gear 10T",            ZOOM_SYM_M_GEAR10,      MECHANISMS, 60, 60, BELOW, 1, false, fzs_mech_2),
    FZS("gear 15T",            ZOOM_SYM_M_GEAR15,      MECHANISMS, 85, 85, BELOW, 1, false, fzs_mech_3),
    FZS("gear 20T",            ZOOM_SYM_M_GEAR20,      MECHANISMS, 110, 110, BELOW, 1, false, fzs_mech_4),
    FZS("gear 30T",            ZOOM_SYM_M_GEAR30,      MECHANISMS, 160, 160, BELOW, 1, false, fzs_mech_5),
    FZS("gear 40T",            ZOOM_SYM_M_GEAR40,      MECHANISMS, 210, 210, BELOW, 1, false, fzs_mech_6),
    FZS("gear 60T",            ZOOM_SYM_M_GEAR60,      MECHANISMS, 310, 310, BELOW, 1, false, fzs_mech_7),
    FZS("fixed pivot",         ZOOM_SYM_M_PIVOT,       MECHANISMS, 40, 40, BELOW, 1, false, fzs_mech_8),
    FZS("drive motor",         ZOOM_SYM_M_MOTOR,       MECHANISMS, 60, 60, BELOW, 1, false, fzs_mech_9),
    FZS("spring",              ZOOM_SYM_M_SPRING,      MECHANISMS, 100, 24, BELOW, 1, false, fzs_mech_10),
    FZS("weight",              ZOOM_SYM_M_WEIGHT,      MECHANISMS, 40, 40, BELOW, 1, false, fzs_mech_11),
    FZS("wheel",               ZOOM_SYM_M_WHEEL,       MECHANISMS, 80, 80, BELOW, 1, false, fzs_mech_12),
    FZS("wall",                ZOOM_SYM_M_WALL,        MECHANISMS, 240, 20, BELOW, 1, false, fzs_mech_13),
    // Floor plans and house wiring (plan.h): a true-size one's w, h its centimetres (page.c puts it in so).
    FZS("door 70",             ZOOM_SYM_P_DOOR70,      FLOORPLAN, 70, 80, BELOW, 1, false, fzs_plan_0),
    FZS("door 80",             ZOOM_SYM_P_DOOR80,      FLOORPLAN, 80, 90, BELOW, 1, false, fzs_plan_1),
    FZS("door 90",             ZOOM_SYM_P_DOOR90,      FLOORPLAN, 90, 100, BELOW, 1, false, fzs_plan_2),
    FZS("double door",         ZOOM_SYM_P_DOOR2,       FLOORPLAN, 140, 80, BELOW, 1, false, fzs_plan_3),
    FZS("sliding door",        ZOOM_SYM_P_SLIDING,     FLOORPLAN, 160, 10, BELOW, 1, false, fzs_plan_4),
    FZS("window 60",           ZOOM_SYM_P_WIN60,       FLOORPLAN, 60, 10, BELOW, 1, false, fzs_plan_5),
    FZS("window 100",          ZOOM_SYM_P_WIN100,      FLOORPLAN, 100, 10, BELOW, 1, false, fzs_plan_6),
    FZS("window 120",          ZOOM_SYM_P_WIN120,      FLOORPLAN, 120, 10, BELOW, 1, false, fzs_plan_7),
    FZS("window 150",          ZOOM_SYM_P_WIN150,      FLOORPLAN, 150, 10, BELOW, 1, false, fzs_plan_8),
    FZS("window 200",          ZOOM_SYM_P_WIN200,      FLOORPLAN, 200, 10, BELOW, 1, false, fzs_plan_9),
    FZS("opening 90",          ZOOM_SYM_P_OPENING,     FLOORPLAN, 90, 10, BELOW, 1, false, fzs_plan_10),
    FZS("single bed",          ZOOM_SYM_P_BED1,        FLOORPLAN, 90, 190, BELOW, 1, false, fzs_plan_11),
    FZS("double bed",          ZOOM_SYM_P_BED2,        FLOORPLAN, 140, 190, BELOW, 1, false, fzs_plan_12),
    FZS("queen bed",           ZOOM_SYM_P_BED3,        FLOORPLAN, 160, 200, BELOW, 1, false, fzs_plan_13),
    FZS("sofa",                ZOOM_SYM_P_SOFA,        FLOORPLAN, 200, 90, BELOW, 1, false, fzs_plan_14),
    FZS("armchair",            ZOOM_SYM_P_ARMCHAIR,    FLOORPLAN, 80, 80, BELOW, 1, false, fzs_plan_15),
    FZS("dining table",        ZOOM_SYM_P_TABLE,       FLOORPLAN, 160, 90, BELOW, 1, false, fzs_plan_16),
    FZS("round table",         ZOOM_SYM_P_RTABLE,      FLOORPLAN, 100, 100, BELOW, 1, false, fzs_plan_17),
    FZS("chair",               ZOOM_SYM_P_CHAIR,       FLOORPLAN, 45, 50, BELOW, 1, false, fzs_plan_18),
    FZS("desk",                ZOOM_SYM_P_DESK,        FLOORPLAN, 140, 70, BELOW, 1, false, fzs_plan_19),
    FZS("wardrobe",            ZOOM_SYM_P_WARDROBE,    FLOORPLAN, 120, 60, BELOW, 1, false, fzs_plan_20),
    FZS("kitchen counter",     ZOOM_SYM_P_COUNTER,     FLOORPLAN, 240, 60, BELOW, 1, false, fzs_plan_21),
    FZS("kitchen sink",        ZOOM_SYM_P_SINK,        FLOORPLAN, 80, 50, BELOW, 1, false, fzs_plan_22),
    FZS("hob",                 ZOOM_SYM_P_HOB,         FLOORPLAN, 60, 60, BELOW, 1, false, fzs_plan_23),
    FZS("fridge",              ZOOM_SYM_P_FRIDGE,      FLOORPLAN, 60, 65, BELOW, 1, false, fzs_plan_24),
    FZS("washing machine",     ZOOM_SYM_P_WASHER,      FLOORPLAN, 60, 60, BELOW, 1, false, fzs_plan_25),
    FZS("toilet",              ZOOM_SYM_P_TOILET,      FLOORPLAN, 40, 65, BELOW, 1, false, fzs_plan_26),
    FZS("bathtub",             ZOOM_SYM_P_BATH,        FLOORPLAN, 170, 75, BELOW, 1, false, fzs_plan_27),
    FZS("shower",              ZOOM_SYM_P_SHOWER,      FLOORPLAN, 90, 90, BELOW, 1, false, fzs_plan_28),
    FZS("washbasin",           ZOOM_SYM_P_BASIN,       FLOORPLAN, 60, 45, BELOW, 1, false, fzs_plan_29),
    FZS("stairs",              ZOOM_SYM_P_STAIRS,      FLOORPLAN, 100, 280, BELOW, 1, false, fzs_plan_30),
    FZS("socket",              ZOOM_SYM_W_SOCKET,      WIRING, 40, 30, BELOW, 1, false, fzs_plan_31),
    FZS("double socket",       ZOOM_SYM_W_SOCKET2,     WIRING, 50, 30, BELOW, 1, false, fzs_plan_32),
    FZS("light switch",        ZOOM_SYM_W_SWITCH,      WIRING, 36, 36, BELOW, 1, false, fzs_plan_33),
    FZS("two-way switch",      ZOOM_SYM_W_SWITCH2,     WIRING, 36, 36, BELOW, 1, false, fzs_plan_34),
    FZS("dimmer",              ZOOM_SYM_W_DIMMER,      WIRING, 36, 36, BELOW, 1, false, fzs_plan_35),
    FZS("bell push",           ZOOM_SYM_W_BELL,        WIRING, 30, 30, BELOW, 1, false, fzs_plan_36),
    FZS("ceiling light",       ZOOM_SYM_W_LIGHT,       WIRING, 40, 40, BELOW, 1, false, fzs_plan_37),
    FZS("wall light",          ZOOM_SYM_W_WLIGHT,      WIRING, 40, 40, BELOW, 1, false, fzs_plan_38),
    FZS("tube light",          ZOOM_SYM_W_TUBE,        WIRING, 80, 20, BELOW, 1, false, fzs_plan_39),
    FZS("consumer unit",       ZOOM_SYM_W_BOARD,       WIRING, 60, 30, BELOW, 1, false, fzs_plan_40),
    FZS("junction box",        ZOOM_SYM_W_JUNCTION,    WIRING, 24, 24, BELOW, 1, false, fzs_plan_41),
    FZS("TV point",            ZOOM_SYM_W_TV,          WIRING, 36, 30, BELOW, 1, false, fzs_plan_42),
    FZS("data point",          ZOOM_SYM_W_DATA,        WIRING, 36, 30, BELOW, 1, false, fzs_plan_43),
    FZS("smoke detector",      ZOOM_SYM_W_SMOKE,       WIRING, 36, 36, BELOW, 1, false, fzs_plan_44),
    FZS("thermostat",          ZOOM_SYM_W_THERMO,      WIRING, 36, 36, BELOW, 1, false, fzs_plan_45),
    FZS("ceiling fan",         ZOOM_SYM_W_FAN,         WIRING, 50, 50, BELOW, 1, false, fzs_plan_46),
    FZS("meter",               ZOOM_SYM_W_METER,       WIRING, 44, 30, BELOW, 1, false, fzs_plan_47),
    // Maths (plot.h) and sewing.
    FZS("graph",               ZOOM_SYM_X_GRAPH,       MATHS, 320, 220, CORNER, 1, true, fzs_graph),
    FZS("axes",                ZOOM_SYM_X_AXES,        MATHS, 240, 240, CORNER, 1, true, fzs_graph),
    FZS("number line",         ZOOM_SYM_X_LINE,        MATHS, 320, 50, BELOW, 1, true, fzs_number_line),
    FZS("grainline",           ZOOM_SYM_S_GRAIN,       SEWING, 160, 20, BELOW, 1, true, fzs_grainline),
    FZS("place on fold",       ZOOM_SYM_S_FOLD,        SEWING, 160, 40, BELOW, 1, true, fzs_fold),
    FZS("notch",               ZOOM_SYM_S_NOTCH,       SEWING, 16, 20, BELOW, 1, true, fzs_notch),
    FZS("button",              ZOOM_SYM_S_BUTTON,      SEWING, 30, 30, BELOW, 1, false, fzs_button),
    FZS("buttonhole",          ZOOM_SYM_S_HOLE,        SEWING, 50, 12, BELOW, 1, true, fzs_buttonhole),
    FZS("dart",                ZOOM_SYM_S_DART,        SEWING, 40, 100, BELOW, 1, true, fzs_dart),
    FZS("pattern label",       ZOOM_SYM_S_LABEL,       SEWING, 180, 90, INSIDE, 1, true, fzs_pattern_label),
    // (0.1.49: what hangs, and what it hangs from)
    FZS("pulley",              ZOOM_SYM_M_PULLEY,      MECHANISMS, 80, 80, BELOW, 1, false, fzs_mech_14),
    FZS("rope",                ZOOM_SYM_M_ROPE,        MECHANISMS, 200, 12, BELOW, 1, false, fzs_mech_15),
    FZS("crate",               ZOOM_SYM_M_CRATE,       MECHANISMS, 60, 50, BELOW, 1, false, fzs_mech_16),
    // (0.1.50: what slides)
    FZS("rail",                ZOOM_SYM_M_RAIL,        MECHANISMS, 300, 16, BELOW, 1, false, fzs_mech_17),
    FZS("slider",              ZOOM_SYM_M_SLIDER,      MECHANISMS, 60, 36, BELOW, 1, false, fzs_mech_18),
    FZS("rack",                ZOOM_SYM_M_RACK,        MECHANISMS, 300, 30, BELOW, 1, false, fzs_mech_19),
    // (0.1.51: what joins drawn bodies)
    FZS("pin",                 ZOOM_SYM_M_PIN,         MECHANISMS, 16, 16, BELOW, 1, false, fzs_mech_20),
    // (0.1.51: the simulation's chips (sim/chips.c), and a person's own parts)
    FZS("74HC02",              ZOOM_SYM_C_7402,      CHIPS, 200, 160, INSIDE, 1, true, fzs_chip_7402),
    FZS("74HC74",              ZOOM_SYM_C_7474,      CHIPS, 200, 160, INSIDE, 1, true, fzs_chip_7474),
    FZS("74HC138",             ZOOM_SYM_C_74138,     CHIPS, 200, 180, INSIDE, 1, true, fzs_chip_74138),
    FZS("74HC157",             ZOOM_SYM_C_74157,     CHIPS, 200, 180, INSIDE, 1, true, fzs_chip_74157),
    FZS("74HC161",             ZOOM_SYM_C_74161,     CHIPS, 200, 180, INSIDE, 1, true, fzs_chip_74161),
    FZS("74HC173",             ZOOM_SYM_C_74173,     CHIPS, 200, 180, INSIDE, 1, true, fzs_chip_74173),
    FZS("74HC245",             ZOOM_SYM_C_74245,     CHIPS, 200, 220, INSIDE, 1, true, fzs_chip_74245),
    FZS("74HC283",             ZOOM_SYM_C_74283,     CHIPS, 200, 180, INSIDE, 1, true, fzs_chip_74283),
    FZS("74HC189",             ZOOM_SYM_C_74189,     CHIPS, 200, 180, INSIDE, 1, true, fzs_chip_74189),
    FZS("custom part",         ZOOM_SYM_L_CUSTOM,      LOGIC, 120, 120, INSIDE, 1, true, fzs_custom),
    // Displays (display.h): the sized ones as their kind comes — 4 digits, 8 × 8, 16 × 2.
    FZS("7-segment panel",     ZOOM_SYM_E_SEGPANEL,    ELECTRONICS, 300, 180, BELOW, 1, true, fzs_disp_panel),
    FZS("LED matrix",          ZOOM_SYM_E_MATRIX,      ELECTRONICS, 200, 200, BELOW, 1, true, fzs_disp_matrix),
    FZS("LED bar graph",       ZOOM_SYM_E_BARGRAPH,    ELECTRONICS, 140, 220, BELOW, 1, true, fzs_disp_bar),
    FZS("LM3914",              ZOOM_SYM_C_3914,        CHIPS, 200, 200, INSIDE, 1, true, fzs_chip_3914),
    FZS("CD4511",              ZOOM_SYM_C_4511,        CHIPS, 200, 180, INSIDE, 1, true, fzs_chip_4511),
    FZS("panel meter",         ZOOM_SYM_E_METER,       ELECTRONICS, 160, 80, BELOW, 1, true, fzs_disp_meter),
    FZS("character LCD",       ZOOM_SYM_M_CHARLCD,     CHIPS, 340, 130, BELOW, 1, true, fzs_disp_lcd),
    FZS("oscilloscope",        ZOOM_SYM_E_SCOPE,       ELECTRONICS, 300, 160, BELOW, 1, true, fzs_disp_scope),
    FZS("tracer",              ZOOM_SYM_M_TRACER,      MECHANISMS, 16, 16, BELOW, 1, false, fzs_mech_tracer),
    FZS("hand crank",          ZOOM_SYM_M_CRANK,       MECHANISMS, 60, 60, BELOW, 1, false, fzs_mech_crank),
    FZS("SPDT switch",         ZOOM_SYM_E_SPDT,        ELECTRONICS, 60, 60, BELOW, 1, false, fzs_part_spdt),
    FZS("DPDT switch",         ZOOM_SYM_E_DPDT,        ELECTRONICS, 60, 120, BELOW, 1, false, fzs_part_dpdt),
    FZS("LDR",                 ZOOM_SYM_E_LDR,         ELECTRONICS, 60, 40, BELOW, 1, false, fzs_part_ldr),
    FZS("thermistor",          ZOOM_SYM_E_NTC,         ELECTRONICS, 60, 40, BELOW, 1, false, fzs_part_ntc),
    // (0.1.73: worked by, or working, a mechanism)
    FZS("solenoid",            ZOOM_SYM_E_SOLENOID,    ELECTRONICS, 100, 40, BELOW, 1, true, fzs_part_solenoid),
    FZS("stepper motor",       ZOOM_SYM_E_STEPPER,     ELECTRONICS, 160, 120, BELOW, 1, true, fzs_part_stepper),
    FZS("rotary encoder",      ZOOM_SYM_E_ENCODER,     ELECTRONICS, 80, 80, BELOW, 1, true, fzs_part_encoder),
    FZS("slotted sensor",      ZOOM_SYM_E_SLOT,        ELECTRONICS, 80, 60, BELOW, 1, false, fzs_part_slot),
    // (0.1.74: belts and chains, cams, ratchets, dampers, worms)
    FZS("belt",                ZOOM_SYM_M_BELT,        MECHANISMS, 240, 40, BELOW, 1, true, fzs_mech_belt),
    FZS("chain",               ZOOM_SYM_M_CHAIN,       MECHANISMS, 240, 40, BELOW, 1, true, fzs_mech_chain),
    FZS("sprocket",            ZOOM_SYM_M_SPROCKET,    MECHANISMS, 60, 60, BELOW, 1, false, fzs_mech_sprocket),
    FZS("cam",                 ZOOM_SYM_M_CAM,         MECHANISMS, 80, 80, BELOW, 1, false, fzs_mech_cam),
    FZS("follower",            ZOOM_SYM_M_FOLLOWER,    MECHANISMS, 120, 30, BELOW, 1, true, fzs_mech_follower),
    FZS("ratchet",             ZOOM_SYM_M_RATCHET,     MECHANISMS, 80, 80, BELOW, 1, false, fzs_mech_ratchet),
    FZS("pawl",                ZOOM_SYM_M_PAWL,        MECHANISMS, 70, 16, BELOW, 1, false, fzs_mech_pawl),
    FZS("damper",              ZOOM_SYM_M_DAMPER,      MECHANISMS, 120, 24, BELOW, 1, true, fzs_mech_damper),
    FZS("worm",                ZOOM_SYM_M_WORM,        MECHANISMS, 160, 40, BELOW, 1, true, fzs_mech_worm),
    // (0.1.75: more parts)
    FZS("JK flip-flop",        ZOOM_SYM_L_JK,          LOGIC, 80, 80, BELOW, 1, false, fzs_part_jk),
    FZS("SR latch",            ZOOM_SYM_L_SR,          LOGIC, 80, 80, BELOW, 1, false, fzs_part_sr),
    FZS("LM393",               ZOOM_SYM_C_LM393,       CHIPS, 200, 100, INSIDE, 1, true, fzs_part_lm393),
    FZS("SCR",                 ZOOM_SYM_E_SCR,         ELECTRONICS, 60, 40, BELOW, 1, false, fzs_part_scr),
    FZS("TRIAC",               ZOOM_SYM_E_TRIAC,       ELECTRONICS, 60, 40, BELOW, 1, false, fzs_part_triac),
    FZS("transformer",         ZOOM_SYM_E_XFMR,        ELECTRONICS, 80, 80, BELOW, 1, true, fzs_part_xfmr),
    FZS("bridge rectifier",    ZOOM_SYM_E_BRIDGE,      ELECTRONICS, 80, 80, BELOW, 1, true, fzs_part_bridge),
    FZS("optocoupler",         ZOOM_SYM_E_OPTO,        ELECTRONICS, 100, 60, BELOW, 1, false, fzs_part_opto),
    FZS("photodiode",          ZOOM_SYM_E_PHOTODIODE,  ELECTRONICS, 60, 40, BELOW, 1, false, fzs_part_photodiode),
    FZS("phototransistor",     ZOOM_SYM_E_PHOTOTRANS,  ELECTRONICS, 60, 60, BELOW, 1, false, fzs_part_phototrans),
    FZS("speaker",             ZOOM_SYM_E_SPEAKER,     ELECTRONICS, 60, 40, BELOW, 1, true, fzs_part_speaker),
    FZS("CD4017",              ZOOM_SYM_C_4017,        CHIPS, 200, 180, INSIDE, 1, true, fzs_part_cd4017),
    // (0.1.85: brackets, on their own and in pairs)
    FZS("left parenthesis",    ZOOM_SYM_B_PAREN_L,     BRACKETS, 30, 150, OUT_LEFT, 1, true, fzs_paren_l),
    FZS("right parenthesis",   ZOOM_SYM_B_PAREN_R,     BRACKETS, 30, 150, OUT_RIGHT, 1, true, fzs_paren_r),
    FZS("left square bracket", ZOOM_SYM_B_SQUARE_L,    BRACKETS, 30, 150, OUT_LEFT, 1, true, fzs_square_l),
    FZS("right square bracket", ZOOM_SYM_B_SQUARE_R,   BRACKETS, 30, 150, OUT_RIGHT, 1, true, fzs_square_r),
    FZS("left brace",          ZOOM_SYM_B_CURLY_L,     BRACKETS, 30, 150, OUT_LEFT, 1, true, fzs_curly_l),
    FZS("right brace",         ZOOM_SYM_B_CURLY_R,     BRACKETS, 30, 150, OUT_RIGHT, 1, true, fzs_curly_r),
    FZS("left angle bracket",  ZOOM_SYM_B_ANGLE_L,     BRACKETS, 30, 150, OUT_LEFT, 1, true, fzs_angle_l),
    FZS("right angle bracket", ZOOM_SYM_B_ANGLE_R,     BRACKETS, 30, 150, OUT_RIGHT, 1, true, fzs_angle_r),
    FZS("vertical bar",        ZOOM_SYM_B_BAR,         BRACKETS, 20, 150, OUT_RIGHT, 1, true, fzs_vbar),
    FZS("double bar",          ZOOM_SYM_B_DOUBLE,      BRACKETS, 24, 150, OUT_RIGHT, 1, true, fzs_double_bar),
    FZS("left floor",          ZOOM_SYM_B_FLOOR_L,     BRACKETS, 30, 150, OUT_LEFT, 1, true, fzs_floor_l),
    FZS("right floor",         ZOOM_SYM_B_FLOOR_R,     BRACKETS, 30, 150, OUT_RIGHT, 1, true, fzs_floor_r),
    FZS("left ceiling",        ZOOM_SYM_B_CEILING_L,   BRACKETS, 30, 150, OUT_LEFT, 1, true, fzs_ceiling_l),
    FZS("right ceiling",       ZOOM_SYM_B_CEILING_R,   BRACKETS, 30, 150, OUT_RIGHT, 1, true, fzs_ceiling_r),
    FZS("under brace",         ZOOM_SYM_B_UNDER,       BRACKETS, 220, 30, BELOW, 1, true, fzs_brace_under),
    FZS("over brace",          ZOOM_SYM_B_OVER,        BRACKETS, 220, 30, ABOVE, 1, true, fzs_brace_over),
    FZS("parentheses",         ZOOM_SYM_B_PARENS,      BRACKETS, 180, 80, INSIDE, 1, true, fzs_pair_round),
    FZS("square brackets",     ZOOM_SYM_B_SQUARES,     BRACKETS, 180, 80, INSIDE, 1, true, fzs_pair_square),
    FZS("braces",              ZOOM_SYM_B_CURLIES,     BRACKETS, 180, 80, INSIDE, 1, true, fzs_pair_curly),
    FZS("angle brackets",      ZOOM_SYM_B_ANGLES,      BRACKETS, 180, 80, INSIDE, 1, true, fzs_pair_angle),
    FZS("bars",                ZOOM_SYM_B_BARS,        BRACKETS, 180, 80, INSIDE, 1, true, fzs_pair_bar),
    FZS("double bars",         ZOOM_SYM_B_DOUBLES,     BRACKETS, 180, 80, INSIDE, 1, true, fzs_pair_double),
    // (0.1.85: plain shapes)
    FZS("rectangle",           ZOOM_SYM_P_RECT,        SHAPES, 120, 76, INSIDE, 1, false, fzs_rectangle),
    FZS("rounded rectangle",   ZOOM_SYM_P_ROUNDED,     SHAPES, 120, 76, INSIDE, 1, false, fzs_round_rect),
    FZS("circle",              ZOOM_SYM_P_CIRCLE,      SHAPES, 90, 90, INSIDE, 1, false, fzs_circle),
    FZS("diamond",             ZOOM_SYM_P_DIAMOND,     SHAPES, 110, 90, INSIDE, 1, false, fzs_decision),
    FZS("parallelogram",       ZOOM_SYM_P_PARALLEL,    SHAPES, 130, 70, INSIDE, 1, false, fzs_parallelogram),
    FZS("trapezoid",           ZOOM_SYM_P_TRAPEZOID,   SHAPES, 130, 70, INSIDE, 1, false, fzs_trapezoid),
    FZS("isosceles triangle",  ZOOM_SYM_P_TRIANGLE,    SHAPES, 100, 86, INSIDE, 1, false, fzs_triangle),
    FZS("right triangle",      ZOOM_SYM_P_RIGHT_TRI,   SHAPES, 100, 86, INSIDE, 1, false, fzs_right_triangle),
    FZS("pentagon",            ZOOM_SYM_P_PENTAGON,    SHAPES, 100, 96, INSIDE, 1, false, fzs_pentagon),
    FZS("regular hexagon",     ZOOM_SYM_P_HEXAGON,     SHAPES, 110, 96, INSIDE, 1, false, fzs_hexagon),
    FZS("octagon",             ZOOM_SYM_P_OCTAGON,     SHAPES, 96, 96, INSIDE, 1, false, fzs_octagon),
    FZS("star",                ZOOM_SYM_P_STAR,        SHAPES, 100, 96, INSIDE, 1, false, fzs_star),
    FZS("burst",               ZOOM_SYM_P_BURST,       SHAPES, 110, 110, INSIDE, 1, false, fzs_burst),
    FZS("cross",               ZOOM_SYM_P_CROSS,       SHAPES, 90, 90, INSIDE, 1, false, fzs_cross),
    FZS("heart",               ZOOM_SYM_P_HEART,       SHAPES, 100, 90, INSIDE, 1, false, fzs_heart),
    FZS("cloud shape",         ZOOM_SYM_P_CLOUD,       SHAPES, 150, 96, INSIDE, 1, false, fzs_cloud),
    FZS("speech bubble",       ZOOM_SYM_P_SPEECH,      SHAPES, 150, 100, INSIDE, 1, false, fzs_speech),
    FZS("thought bubble",      ZOOM_SYM_P_THOUGHT,     SHAPES, 150, 110, INSIDE, 1, false, fzs_thought),
    FZS("arc",                 ZOOM_SYM_P_ARC,         SHAPES, 120, 60, BELOW, 1, true, fzs_arc),
    FZS("pie",                 ZOOM_SYM_P_PIE,         SHAPES, 96, 96, BELOW, 1, false, fzs_pie),
    FZS("ring",                ZOOM_SYM_P_RING,        SHAPES, 96, 96, BELOW, 1, false, fzs_ring),
    FZS("cylinder",            ZOOM_SYM_P_CYLINDER,    SHAPES, 90, 110, INSIDE, 1, false, fzs_cylinder),
    FZS("cube",                ZOOM_SYM_P_CUBE,        SHAPES, 110, 110, INSIDE, 1, false, fzs_cube),
    FZS("block arrow",         ZOOM_SYM_P_ARROW,       SHAPES, 150, 70, INSIDE, 1, false, fzs_block_arrow),
    FZS("double block arrow",  ZOOM_SYM_P_DOUBLE_ARROW, SHAPES, 170, 70, INSIDE, 1, false, fzs_double_arrow),
    FZS("curved arrow",        ZOOM_SYM_P_CURVED_ARROW, SHAPES, 120, 90, BELOW, 1, false, fzs_curved_arrow),
    FZS("chevron",             ZOOM_SYM_P_CHEVRON,     SHAPES, 130, 70, INSIDE, 1, false, fzs_chevron),
    // (0.1.86: Venn diagrams, charts, geometry's marks, science, stamps)
    FZS("venn 2",              ZOOM_SYM_P_VENN2,       SHAPES, 240, 150, REGIONS, 3, true, fzs_venn2),
    FZS("venn 3",              ZOOM_SYM_P_VENN3,       SHAPES, 220, 210, REGIONS, 4, true, fzs_venn3),
    FZS("bar chart",           ZOOM_SYM_K_BARS,        CHARTS, 320, 220, CORNER, 1, true, fzs_graph),
    FZS("line chart",          ZOOM_SYM_K_LINES,       CHARTS, 320, 220, CORNER, 1, true, fzs_graph),
    FZS("pie chart",           ZOOM_SYM_K_PIE,         CHARTS, 280, 240, CORNER, 1, true, fzs_graph),
    FZS("angle mark",          ZOOM_SYM_G_ANGLE,       GEOMETRY, 90, 70, BELOW, 1, true, fzs_angle_mark),
    FZS("right angle mark",    ZOOM_SYM_G_RIGHT,       GEOMETRY, 80, 80, BELOW, 1, true, fzs_right_angle_mark),
    FZS("equal mark",          ZOOM_SYM_G_TICK1,       GEOMETRY, 8, 24, BELOW, 1, true, fzs_tick1),
    FZS("double equal mark",   ZOOM_SYM_G_TICK2,       GEOMETRY, 16, 24, BELOW, 1, true, fzs_tick2),
    FZS("triple equal mark",   ZOOM_SYM_G_TICK3,       GEOMETRY, 22, 24, BELOW, 1, true, fzs_tick3),
    FZS("parallel mark",       ZOOM_SYM_G_PAR1,        GEOMETRY, 16, 20, BELOW, 1, true, fzs_parallel1),
    FZS("double parallel mark", ZOOM_SYM_G_PAR2,       GEOMETRY, 26, 20, BELOW, 1, true, fzs_parallel2),
    FZS("point",               ZOOM_SYM_G_POINT,       GEOMETRY, 16, 16, OUT_RIGHT, 1, true, fzs_point),
    FZS("segment",             ZOOM_SYM_G_SEGMENT,     GEOMETRY, 160, 14, BELOW, 1, true, fzs_segment),
    FZS("ray",                 ZOOM_SYM_G_RAY,         GEOMETRY, 160, 14, BELOW, 1, true, fzs_ray),
    FZS("line",                ZOOM_SYM_G_LINE,        GEOMETRY, 180, 14, BELOW, 1, true, fzs_line_both),
    FZS("vector",              ZOOM_SYM_G_VECTOR,      GEOMETRY, 120, 70, ABOVE, 1, true, fzs_vector),
    FZS("circle and radius",   ZOOM_SYM_G_RADIUS,      GEOMETRY, 120, 120, BELOW, 1, false, fzs_circle_radius),
    FZS("protractor",          ZOOM_SYM_G_PROTRACTOR,  GEOMETRY, 220, 110, BELOW, 1, false, fzs_protractor),
    FZS("beaker",              ZOOM_SYM_S_BEAKER,      SCIENCE, 80, 100, BELOW, 1, true, fzs_beaker),
    FZS("flask",               ZOOM_SYM_S_FLASK,       SCIENCE, 90, 110, BELOW, 1, true, fzs_flask),
    FZS("round flask",         ZOOM_SYM_S_ROUND_FLASK, SCIENCE, 90, 120, BELOW, 1, true, fzs_round_flask),
    FZS("test tube",           ZOOM_SYM_S_TUBE,        SCIENCE, 30, 120, BELOW, 1, true, fzs_test_tube),
    FZS("measuring cylinder",  ZOOM_SYM_S_CYLINDER,    SCIENCE, 50, 140, BELOW, 1, true, fzs_cylinder_measure),
    FZS("funnel",              ZOOM_SYM_S_FUNNEL,      SCIENCE, 90, 100, BELOW, 1, true, fzs_funnel),
    FZS("burner",              ZOOM_SYM_S_BURNER,      SCIENCE, 60, 120, BELOW, 1, true, fzs_burner),
    FZS("convex lens",         ZOOM_SYM_S_CONVEX,      SCIENCE, 36, 130, BELOW, 1, false, fzs_lens_convex),
    FZS("concave lens",        ZOOM_SYM_S_CONCAVE,     SCIENCE, 40, 130, BELOW, 1, false, fzs_lens_concave),
    FZS("mirror",              ZOOM_SYM_S_MIRROR,      SCIENCE, 30, 130, BELOW, 1, true, fzs_mirror_flat),
    FZS("curved mirror",       ZOOM_SYM_S_MIRROR2,     SCIENCE, 40, 130, BELOW, 1, true, fzs_mirror_curved),
    FZS("prism",               ZOOM_SYM_S_PRISM,       SCIENCE, 110, 100, BELOW, 1, false, fzs_prism),
    FZS("light source",        ZOOM_SYM_S_LAMP,        SCIENCE, 80, 80, BELOW, 1, true, fzs_lamp),
    FZS("atom",                ZOOM_SYM_S_ATOM,        SCIENCE, 110, 110, BELOW, 1, true, fzs_atom),
    FZS("benzene ring",        ZOOM_SYM_S_BENZENE,     SCIENCE, 90, 100, BELOW, 1, false, fzs_benzene),
    FZS("cell",                ZOOM_SYM_S_CELL,        SCIENCE, 140, 100, BELOW, 1, false, fzs_cell),
    FZS("DNA",                 ZOOM_SYM_S_DNA,         SCIENCE, 180, 60, BELOW, 1, true, fzs_dna),
    FZS("thermometer",         ZOOM_SYM_S_THERMO,      SCIENCE, 36, 130, BELOW, 1, true, fzs_thermometer),
    FZS("check stamp",         ZOOM_SYM_T_CHECK,       STAMPS, 60, 60, BELOW, 1, true, fzs_stamp_check),
    FZS("cross stamp",         ZOOM_SYM_T_CROSS,       STAMPS, 60, 60, BELOW, 1, true, fzs_stamp_cross),
    FZS("warning stamp",       ZOOM_SYM_T_WARNING,     STAMPS, 66, 60, BELOW, 1, false, fzs_stamp_warning),
    FZS("question stamp",      ZOOM_SYM_T_QUESTION,    STAMPS, 46, 66, BELOW, 1, true, fzs_stamp_question),
    FZS("exclamation stamp",   ZOOM_SYM_T_EXCLAIM,     STAMPS, 30, 66, BELOW, 1, true, fzs_stamp_exclamation),
    FZS("info stamp",          ZOOM_SYM_T_INFO,        STAMPS, 60, 60, BELOW, 1, false, fzs_stamp_info),
    FZS("star stamp",          ZOOM_SYM_T_STAR,        STAMPS, 64, 62, BELOW, 1, false, fzs_stamp_star),
    FZS("heart stamp",         ZOOM_SYM_T_HEART,       STAMPS, 62, 56, BELOW, 1, false, fzs_stamp_heart),
    FZS("idea stamp",          ZOOM_SYM_T_BULB,        STAMPS, 50, 70, BELOW, 1, true, fzs_stamp_bulb),
    FZS("flag stamp",          ZOOM_SYM_T_FLAG,        STAMPS, 54, 66, BELOW, 1, true, fzs_stamp_flag),
    FZS("pin stamp",           ZOOM_SYM_T_PIN,         STAMPS, 44, 66, BELOW, 1, false, fzs_stamp_pin),
    FZS("clock stamp",         ZOOM_SYM_T_CLOCK,       STAMPS, 60, 60, BELOW, 1, false, fzs_stamp_clock),
};

#define FUDE_ZOOM_SYMBOL_N ((u32)(sizeof(FUDE_ZOOM_SYMBOLS) / sizeof(FUDE_ZOOM_SYMBOLS[0])))

u32 fude_zoom_symbol_find(const c8* _id) {
    for(u32 _k = 0; _k < FUDE_ZOOM_SYMBOL_N; _k++) {
        if(strcmp(FUDE_ZOOM_SYMBOLS[_k].info.id, _id) == 0) {
            return _k;
        }
    }
    return FUDE_ZOOM_NONE;
}

u32 fude_zoom_symbol_count(void) {
    return FUDE_ZOOM_SYMBOL_N;
}

const fude_zoom_symbol_info* fude_zoom_symbol_info_of(u32 _kind) {
    return _kind < FUDE_ZOOM_SYMBOL_N ? &FUDE_ZOOM_SYMBOLS[_kind].info : NULL;
}

RDE_INTERNAL fude_zoom_symbol_art_fn fzs_art = NULL;

void fude_zoom_symbol_set_art(fude_zoom_symbol_art_fn _fn) {
    fzs_art = _fn;
}

u32 fude_zoom_symbol_parts(u32 _kind, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points, rde_arr* _parts) {
    rde_arr_clear(_points);
    rde_arr_clear(_parts);
    if(_kind >= FUDE_ZOOM_SYMBOL_N) {
        return 0;
    }
    if(fzs_art != NULL) {
        // (its art, where the app has it)
        const u32 _n = fzs_art(_kind, fabs(_hw), fabs(_hh), _segments < 16u ? 16u : (_segments > 256u ? 256u : _segments), _points, _parts);
        if(_n > 0u) {
            return _n;
        }
        rde_arr_clear(_points);
        rde_arr_clear(_parts);
    }
    fude_zoom_sym _s = { _points, _parts, fabs(_hw), fabs(_hh), _segments < 16u ? 16u : (_segments > 256u ? 256u : _segments), 0u };
    FUDE_ZOOM_SYMBOLS[_kind].draw(&_s);
    return (u32)rde_arr_length(_parts);
}

u32 fude_zoom_symbol_outline(u32 _kind, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points) {
    rde_arr_clear(_points);
    if(_kind >= FUDE_ZOOM_SYMBOL_N) {
        return 0;
    }
    const f64 _w = fabs(_hw), _h = fabs(_hh);
    if(FUDE_ZOOM_SYMBOLS[_kind].info.box_hull) {
        const fude_zoom_v2 _c[4] = { { -_w, -_h }, { _w, -_h }, { _w, _h }, { -_w, _h } };
        for(u32 _i = 0; _i < 4u; _i++) {
            rde_arr_add(_points, (any)&_c[_i]);
        }
        return 4u;
    }
    rde_arr _all = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std()), _parts = rde_arr_new(sizeof(fude_zoom_symbol_part), rde_memory_allocator_get_default_std());
    if(fude_zoom_symbol_parts(_kind, _w, _h, _segments, &_all, &_parts) > 0u) {
        const fude_zoom_symbol_part* _p = (const fude_zoom_symbol_part*)_parts.memory;
        for(u32 _i = 0; _i < _p[0].count; _i++) {
            rde_arr_add(_points, (any)&((const fude_zoom_v2*)_all.memory)[_p[0].first + _i]);
        }
    }
    rde_arr_free(&_all);
    rde_arr_free(&_parts);
    return (u32)rde_arr_length(_points);
}

u32 fude_zoom_symbol_text_boxes(u32 _kind, f64 _hw, f64 _hh, u32 _count, fude_zoom_box* _out, u32 _max) {
    const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of(_kind);
    if(_info == NULL || _max == 0u) {
        return 0;
    }
    const f64 _w = fabs(_hw), _h = fabs(_hh);
    _count = _count < _info->boxes ? _info->boxes : _count;
    _count = _count > _max ? _max : _count;
    const f64 _pad = fmin(_w, _h) * 0.12;
    switch(_info->place) {
        case FUDE_ZOOM_SYMBOL_TEXT_BELOW:
            _out[0] = (fude_zoom_box){ -_w * 2.5, -_h - fmax(_h * 0.9, 26.0), _w * 2.5, -_h - 2.0 };
            return 1u;
        case FUDE_ZOOM_SYMBOL_TEXT_HEADER: {
            const f64 _band = fmin(_h * 0.32, 44.0);
            _out[0] = (fude_zoom_box){ -_w + _pad, _h - _band, _w - _pad, _h };
            return 1u;
        }
        case FUDE_ZOOM_SYMBOL_TEXT_LEFT: {
            const f64 _band = fmin(_w * 0.16, 34.0);
            _out[0] = (fude_zoom_box){ -_w, -_h, -_w + _band, _h };
            return 1u;
        }
        case FUDE_ZOOM_SYMBOL_TEXT_OUT_LEFT:
        case FUDE_ZOOM_SYMBOL_TEXT_OUT_RIGHT: {
            // (beside it, as tall as it is: its text in the middle of its height, against it)
            const f64 _gap = fmax(_w * 0.3, 4.0), _wide = fmax(_h * 1.2, 180.0);
            _out[0] = _info->place == FUDE_ZOOM_SYMBOL_TEXT_OUT_LEFT ? (fude_zoom_box){ -_w - _gap - _wide, -_h, -_w - _gap, _h }
                                                                      : (fude_zoom_box){ _w + _gap, -_h, _w + _gap + _wide, _h };
            return 1u;
        }
        case FUDE_ZOOM_SYMBOL_TEXT_ABOVE:
            _out[0] = (fude_zoom_box){ -_w * 2.5, _h + 2.0, _w * 2.5, _h + fmax(_h * 0.9, 26.0) };
            return 1u;
        case FUDE_ZOOM_SYMBOL_TEXT_REGIONS: {
            // A Venn diagram's regions: each circle's own part, then where they all meet.
            u32 _n = 0;
            if(_kind < FUDE_ZOOM_SYMBOL_N && FUDE_ZOOM_SYMBOLS[_kind].draw == fzs_venn3) {
                const f64 _r = fzs_venn3_r(_w, _h), _d = _r * 0.62;
                for(u32 _i = 0; _i < 3u && _n < _max; _i++) {
                    const f64 _a = FUDE_ZOOM_SYMBOL_PI * 0.5 + 2.0 * FUDE_ZOOM_SYMBOL_PI * (f64)_i / 3.0;
                    const f64 _cx = cos(_a) * (_d + _r * 0.42), _cy = sin(_a) * (_d + _r * 0.42) - _h * 0.05;
                    _out[_n++] = (fude_zoom_box){ _cx - _r * 0.5, _cy - _r * 0.3, _cx + _r * 0.5, _cy + _r * 0.3 };
                }
                if(_n < _max) {
                    _out[_n++] = (fude_zoom_box){ -_r * 0.3, -_r * 0.25 - _h * 0.05, _r * 0.3, _r * 0.25 - _h * 0.05 };
                }
                return _n;
            }
            const f64 _r = fzs_venn2_r(_w, _h), _d = _w - _r, _lens = fmax(_r - _d, _r * 0.2);
            const fude_zoom_box _q[3] = { { -_w + _r * 0.1, -_r * 0.5, -_d - _lens * 0.5 + _r * 0.05, _r * 0.5 }, { -_lens * 0.9, -_r * 0.5, _lens * 0.9, _r * 0.5 },
                                          { _d + _lens * 0.5 - _r * 0.05, -_r * 0.5, _w - _r * 0.1, _r * 0.5 } };
            for(u32 _i = 0; _i < 3u && _n < _max; _i++) {
                _out[_n++] = _q[_i];
            }
            return _n;
        }
        case FUDE_ZOOM_SYMBOL_TEXT_CORNER: {
            const f64 _in = fmin(_w, _h) * 0.035;   // (from its top left, a little in)
            _out[0] = (fude_zoom_box){ -_w + _in, -_h + _in, _w - _in, _h - _in };
            return 1u;
        }
        case FUDE_ZOOM_SYMBOL_TEXT_PARTS: {
            if(_info->boxes == 4u) {
                // A 2 × 2: its quarters, top left first.
                const fude_zoom_box _q[4] = { { -_w, 0, 0, _h }, { 0, 0, _w, _h }, { -_w, -_h, 0, 0 }, { 0, -_h, _w, 0 } };
                const u32 _n = _max < 4u ? _max : 4u;
                for(u32 _i = 0; _i < _n; _i++) {
                    _out[_i] = (fude_zoom_box){ _q[_i].min_x + _pad, _q[_i].min_y + _pad, _q[_i].max_x - _pad, _q[_i].max_y - _pad };
                }
                return _n;
            }
            // Compartments: the first (a name) a band, the rest share what is left.
            const f64 _first = fmin(_h * 2.0 / (f64)(_count + 1u), 40.0);
            _out[0] = (fude_zoom_box){ -_w + _pad, _h - _first, _w - _pad, _h };
            for(u32 _i = 1; _i < _count; _i++) {
                const f64 _each = (2.0 * _h - _first) / (f64)(_count - 1u);
                const f64 _top  = _h - _first - (f64)(_i - 1u) * _each;
                _out[_i] = (fude_zoom_box){ -_w + _pad, _top - _each, _w - _pad, _top };
            }
            return _count;
        }
        default:
            break;
    }
    // Inside: its middle (a person's body, a browser's page, a lifeline's head: their own).
    if(_kind < FUDE_ZOOM_SYMBOL_N && FUDE_ZOOM_SYMBOLS[_kind].draw == fzs_person) {
        _out[0] = (fude_zoom_box){ -_w + _pad, -_h + _pad, _w - _pad, _h * 0.35 };
    } else if(_kind < FUDE_ZOOM_SYMBOL_N && FUDE_ZOOM_SYMBOLS[_kind].draw == fzs_browser) {
        _out[0] = (fude_zoom_box){ -_w + _pad, -_h + _pad, _w - _pad, _h * 0.6 };
    } else if(_kind < FUDE_ZOOM_SYMBOL_N && FUDE_ZOOM_SYMBOLS[_kind].draw == fzs_task_card) {
        const f64 _tag = fmin(_w * 0.05, 6.0) * 1.5;   // (clear of its tag down its left: a task's box beside it)
        _out[0] = (fude_zoom_box){ -_w + _tag + _pad, -_h + _pad, _w - _pad, _h - _pad };
    } else if(_kind < FUDE_ZOOM_SYMBOL_N && FUDE_ZOOM_SYMBOLS[_kind].draw == fzs_speech) {
        _out[0] = (fude_zoom_box){ -_w + _pad, -_h * FZS_BUBBLE_FOOT + _pad, _w - _pad, _h - _pad };   // (its body)
    } else if(_kind < FUDE_ZOOM_SYMBOL_N && FUDE_ZOOM_SYMBOLS[_kind].draw == fzs_thought) {
        _out[0] = (fude_zoom_box){ -_w * 0.7, -_h * 0.3, _w * 0.7, _h * 0.75 };
    } else if(_kind < FUDE_ZOOM_SYMBOL_N && FUDE_ZOOM_SYMBOLS[_kind].draw == fzs_cube) {
        _out[0] = (fude_zoom_box){ -_w + _pad, -_h + _pad, _w * (1.0 - FZS_CUBE_DEPTH) - _pad, _h * (1.0 - FZS_CUBE_DEPTH) - _pad };   // (its front)
    } else if(_info->family == FUDE_ZOOM_SYMBOL_FAMILY_BRACKETS) {
        const f64 _in = fzs_pair_width(_w, _h) + _pad;   // (between its pair)
        _out[0] = (fude_zoom_box){ -_w + _in, -_h + _pad, _w - _in, _h - _pad };
    } else if(_kind < FUDE_ZOOM_SYMBOL_N && FUDE_ZOOM_SYMBOLS[_kind].draw == fzs_decision) {
        _out[0] = (fude_zoom_box){ -_w * 0.6, -_h * 0.6, _w * 0.6, _h * 0.6 };
    } else {
        _out[0] = (fude_zoom_box){ -_w + _pad, -_h + _pad, _w - _pad, _h - _pad };
    }
    return 1u;
}

// --- a connector's ends -------------------------------------------------------------------------

f64 fude_zoom_symbol_head(u32 _style, b8 _dot, fude_zoom_v2 _tip, fude_zoom_v2 _from, f64 _size, rde_arr* _points, rde_arr* _parts) {
    rde_arr_clear(_points);
    rde_arr_clear(_parts);
    const f64 _len = hypot(_tip.x - _from.x, _tip.y - _from.y);
    if(!(_len > 0.0) || !(_size > 0.0)) {
        return 0.0;
    }
    fude_zoom_sym _s = { _points, _parts, 1.0, 1.0, 32u, 0u };
    const fude_zoom_v2 _u = { (_tip.x - _from.x) / _len, (_tip.y - _from.y) / _len }, _n = { -_u.y, _u.x };
    // A point _back behind the tip along the line, _side across it.
    #define FZH(_back, _side) fude_zoom_sym_pt(&_s, _tip.x - _u.x * (_back) + _n.x * (_side), _tip.y - _u.y * (_back) + _n.y * (_side))
    #define FZH_LINE(_b0, _s0, _b1, _s1) do { fude_zoom_sym_begin(&_s); FZH(_b0, _s0); FZH(_b1, _s1); fude_zoom_sym_end(&_s, 0u); } while(0)
    #define FZH_CIRCLE(_back, _r, _flags) fude_zoom_sym_ellipse(&_s, _tip.x - _u.x * (_back), _tip.y - _u.y * (_back), (_r), (_r), (u8)(_flags))
    #define FZH_CROW(_apex) do { FZH_LINE(_apex, 0.0, 0.0, _size * 0.6); FZH_LINE(_apex, 0.0, 0.0, 0.0); FZH_LINE(_apex, 0.0, 0.0, -_size * 0.6); } while(0)
    const f64 _l = _size;
    switch(_style) {
        case FUDE_ZOOM_HEAD_OPEN:
            fude_zoom_sym_begin(&_s); FZH(_l, _l * 0.5); FZH(0.0, 0.0); FZH(_l, -_l * 0.5); fude_zoom_sym_end(&_s, 0u);
            return 0.0;
        case FUDE_ZOOM_HEAD_HOLLOW:
            fude_zoom_sym_begin(&_s); FZH(0.0, 0.0); FZH(_l * 1.1, _l * 0.55); FZH(_l * 1.1, -_l * 0.55); fude_zoom_sym_end(&_s, FZS_OUT);
            return _l * 1.1;
        case FUDE_ZOOM_HEAD_DIAMOND:
        case FUDE_ZOOM_HEAD_HOLLOW_DIAMOND:
            fude_zoom_sym_begin(&_s); FZH(0.0, 0.0); FZH(_l * 0.9, _l * 0.45); FZH(_l * 1.8, 0.0); FZH(_l * 0.9, -_l * 0.45);
            fude_zoom_sym_end(&_s, _style == FUDE_ZOOM_HEAD_DIAMOND ? FZS_SOLID : FZS_OUT);
            return _l * 1.8;
        case FUDE_ZOOM_HEAD_DOT:
            FZH_CIRCLE(_l * 0.35, _l * 0.35, FZS_SOLID);
            return 0.0;
        case FUDE_ZOOM_HEAD_RING:
            FZH_CIRCLE(_l * 0.4, _l * 0.4, FZS_OUT);
            return _l * 0.8;
        case FUDE_ZOOM_HEAD_BAR:
            FZH_LINE(_l * 0.15, _l * 0.5, _l * 0.15, -_l * 0.5);
            return 0.0;
        case FUDE_ZOOM_HEAD_CROSS:
            FZH_LINE(_l * 0.3, _l * 0.35, _l * 1.0, -_l * 0.35);
            FZH_LINE(_l * 0.3, -_l * 0.35, _l * 1.0, _l * 0.35);
            return 0.0;
        case FUDE_ZOOM_HEAD_ONE:
            FZH_LINE(_l * 0.7, _l * 0.55, _l * 0.7, -_l * 0.55);
            return 0.0;
        case FUDE_ZOOM_HEAD_ONLY_ONE:
            FZH_LINE(_l * 0.5, _l * 0.55, _l * 0.5, -_l * 0.55);
            FZH_LINE(_l * 0.9, _l * 0.55, _l * 0.9, -_l * 0.55);
            return 0.0;
        case FUDE_ZOOM_HEAD_MANY:
            FZH_CROW(_l * 1.2);
            return 0.0;
        case FUDE_ZOOM_HEAD_ONE_MANY:
            FZH_CROW(_l * 1.2);
            FZH_LINE(_l * 1.5, _l * 0.55, _l * 1.5, -_l * 0.55);
            return 0.0;
        case FUDE_ZOOM_HEAD_ZERO_ONE:
            FZH_LINE(_l * 0.6, _l * 0.55, _l * 0.6, -_l * 0.55);
            FZH_CIRCLE(_l * 1.4, _l * 0.4, FZS_OUT);
            return 0.0;
        case FUDE_ZOOM_HEAD_ZERO_MANY:
            FZH_CROW(_l * 1.2);
            FZH_CIRCLE(_l * 1.65, _l * 0.4, FZS_OUT);
            return 0.0;
        default:
            if(_dot) {
                FZH_CIRCLE(_l * 0.35, _l * 0.35, FZS_SOLID);
                return 0.0;
            }
            fude_zoom_sym_begin(&_s); FZH(0.0, 0.0); FZH(_l, _l * 0.45); FZH(_l, -_l * 0.45); fude_zoom_sym_end(&_s, FZS_SOLID);
            return _l * 0.8;
    }
    #undef FZH
    #undef FZH_LINE
    #undef FZH_CIRCLE
    #undef FZH_CROW
}

// --- its numbers --------------------------------------------------------------------------------

u32 fude_zoom_symbol_numbers(f64* _n, u32 _kind, f64 _hw, f64 _hh, f64 _px, const c8* _text) {
    memset(_n, 0, 4u * sizeof(f64));
    _n[0] = (f64)_kind;
    _n[1] = _hw;
    _n[2] = _hh;
    _n[3] = _px;
    usize _len = _text != NULL ? strlen(_text) : 0u;
    if(_len > FUDE_ZOOM_SYMBOL_TEXT - 1u) {
        _len = FUDE_ZOOM_SYMBOL_TEXT - 1u;
        while(_len > 0 && ((u8)_text[_len] & 0xC0u) == 0x80u) {
            _len--;
        }
    }
    const u32 _words = (u32)((_len + 1u + 7u) / 8u);
    u8 _bytes[FUDE_ZOOM_SYMBOL_TEXT];
    memset(_bytes, 0, (usize)_words * 8u);
    if(_len > 0) {
        memcpy(_bytes, _text, _len);
    }
    memcpy(&_n[4], _bytes, (usize)_words * 8u);
    return 4u + _words;
}

void fude_zoom_symbol_text(const f64* _n, u32 _count, c8* _out, usize _size) {
    if(_size == 0) {
        return;
    }
    _out[0] = 0;
    if(_count <= 4u) {
        return;
    }
    const u32 _most  = FUDE_ZOOM_SYMBOL_TEXT / 8u;
    const u32 _words = _count - 4u > _most ? _most : _count - 4u;
    const c8* _bytes = (const c8*)&_n[4];
    usize _len = strnlen(_bytes, (usize)_words * 8u);
    if(_len >= _size) {
        _len = _size - 1u;
        while(_len > 0 && ((u8)_bytes[_len] & 0xC0u) == 0x80u) {
            _len--;
        }
    }
    memcpy(_out, _bytes, _len);
    _out[_len] = 0;
}

u32 fude_zoom_symbol_text_split(const c8* _text, u32* _from, u32* _len, u32 _max) {
    if(_max == 0u) {
        return 0;
    }
    u32 _n = 0, _start = 0, _i = 0;
    const u32 _all = (u32)strlen(_text);
    while(_i <= _all) {
        // A line of its own that is "---" ends a part.
        const u32 _line = _i;
        while(_i < _all && _text[_i] != '\n') {
            _i++;
        }
        const b8 _rule = _i - _line == 3u && strncmp(&_text[_line], "---", 3) == 0;
        if(_rule && _n + 1u < _max) {
            u32 _end = _line;
            while(_end > _start && (_text[_end - 1u] == '\n')) {
                _end--;
            }
            _from[_n] = _start;
            _len[_n]  = _end - _start;
            _n++;
            _start = _i + 1u < _all ? _i + 1u : _all;
        }
        _i++;
    }
    _from[_n] = _start > _all ? _all : _start;
    _len[_n]  = _all - _from[_n];
    return _n + 1u;
}
