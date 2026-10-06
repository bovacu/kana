// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/symbol.h"
#include "zoom/scene.h"

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

u32 fude_zoom_symbol_parts(u32 _kind, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points, rde_arr* _parts) {
    rde_arr_clear(_points);
    rde_arr_clear(_parts);
    if(_kind >= FUDE_ZOOM_SYMBOL_N) {
        return 0;
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
