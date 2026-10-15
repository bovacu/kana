// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/symbol_parts.h"

// ===========================================================================
// See symbol.h. Symbols, the other families' draw functions: maths' boxes, sewing, plain shapes, Venn diagrams,
// geometry's marks, science, stamps (in their own colours) and brackets (one, or a pair round a text).
//
// Each symbol is drawn by a function of its half sizes (hw, hh), fzs_<what>: its parts, in its own units round its
// middle (Y up). Most are drawn in the box's own proportions (u, v from -1 to 1, as the box is: fude_zoom_sym_uv); an
// icon's round bits are round whatever its box (a radius of the smaller half, FZS_R). The first part a function makes
// is the symbol's outline. Each is in the table FUDE_ZOOM_SYMBOLS (symbol.c), and drawn with symbol.c's pen; a family's
// own helpers (fzs_fit, fzs_star_of, fzs_tint, fzs_bracket and the like) come before its draw functions.
//
// Part of the symbols: symbol_parts.h holds what the modules share and says which module holds what. Internal names:
// the pen's fude_zoom_sym_, the draw functions' fzs_ and FZS_. Main thread only, as symbol.h says.
// ===========================================================================

// Maths (plot.h): a graph's and axes' box (their axes and curves drawn from their text: render_symbol.c, export.c), a
// number line's line.

// Graph, axes and the charts: their box, filled as the shape is; what is in it is drawn from their text.
void fzs_graph(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FUDE_ZOOM_SYMBOL_FILLED);
}
// Number line: its line across the middle (its ticks and numbers are drawn from its text).
void fzs_number_line(fude_zoom_sym* _s) {
    fude_zoom_sym_line(_s, -0.92, 0.0, 0.92, 0.0, 0u);
}
// Sewing: a grainline (an arrow each end), the fold (a bracket, its arrows to the fold), a notch, a button, a
// buttonhole, a dart, a pattern piece's label.

// Grainline: a line across with a head at each end.
void fzs_grainline(fude_zoom_sym* _s) {
    fude_zoom_sym_line(_s, -0.95, 0.0, 0.95, 0.0, 0u);
    fude_zoom_sym_head(_s, 0.95 * _s->hw, 0.0, 1.0, 0.0, 0.08 * _s->hw);
    fude_zoom_sym_head(_s, -0.95 * _s->hw, 0.0, -1.0, 0.0, 0.08 * _s->hw);
}
// Place on fold: a bracket open at the bottom, its feet arrows down to the fold.
void fzs_fold(fude_zoom_sym* _s) {
    const f64 _b[8] = { -0.9, -1.0, -0.9, 0.6, 0.9, 0.6, 0.9, -1.0 };
    fude_zoom_sym_poly(_s, _b, 4u, 0u);
    fude_zoom_sym_head(_s, -0.9 * _s->hw, -_s->hh, 0.0, -1.0, 0.25 * _s->hh);
    fude_zoom_sym_head(_s, 0.9 * _s->hw, -_s->hh, 0.0, -1.0, 0.25 * _s->hh);
}
// Notch: a solid triangle pointing down.
void fzs_notch(fude_zoom_sym* _s) {
    const f64 _t[6] = { -0.8, 1.0, 0.8, 1.0, 0.0, -1.0 };
    fude_zoom_sym_poly(_s, _t, 3u, FUDE_ZOOM_SYMBOL_CLOSED | FUDE_ZOOM_SYMBOL_SOLID);
}
// Button: a round with four holes.
void fzs_button(fude_zoom_sym* _s) {
    fude_zoom_sym_ellipse(_s, 0.0, 0.0, _s->hw * 0.95, _s->hh * 0.95, FUDE_ZOOM_SYMBOL_FILLED);
    for(u32 _i = 0; _i < 4u; _i++) {
        const f64 _x = (_i % 2u == 0u ? -0.3 : 0.3) * _s->hw, _y = (_i < 2u ? -0.3 : 0.3) * _s->hh;
        fude_zoom_sym_ellipse(_s, _x, _y, _s->hw * 0.1, _s->hh * 0.1, 0u);
    }
}
// Buttonhole: a line across with a bar at each end.
void fzs_buttonhole(fude_zoom_sym* _s) {
    fude_zoom_sym_line(_s, -0.9, 0.0, 0.9, 0.0, 0u);
    fude_zoom_sym_line(_s, -0.9, -0.9, -0.9, 0.9, 0u);
    fude_zoom_sym_line(_s, 0.9, -0.9, 0.9, 0.9, 0u);
}
// Dart: a dashed V with its fold, dashed, down its middle.
void fzs_dart(fude_zoom_sym* _s) {
    const f64 _v[6] = { -1.0, 1.0, 0.0, -1.0, 1.0, 1.0 };
    fude_zoom_sym_poly(_s, _v, 3u, FUDE_ZOOM_SYMBOL_DASHED);
    fude_zoom_sym_line(_s, 0.0, 1.0, 0.0, -1.0, FUDE_ZOOM_SYMBOL_DASHED);   // (its fold)
}
// Pattern label: a dashed rectangle.
void fzs_pattern_label(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FUDE_ZOOM_SYMBOL_DASHED);
}

// Plain shapes (0.1.85).

// A closed shape of _n points in any units (pairs), stretched to fill the box (its own widest and tallest at its
// sides).
RDE_INTERNAL void fzs_fit(fude_zoom_sym* _s, const f64* _xy, u32 _n, u8 _flags) {
    f64 _x0 = 1e30, _y0 = 1e30, _x1 = -1e30, _y1 = -1e30;
    for(u32 _i = 0; _i < _n; _i++) {
        _x0 = rde_math_min_f64(_x0, _xy[2u * _i]); _x1 = rde_math_max_f64(_x1, _xy[2u * _i]);
        _y0 = rde_math_min_f64(_y0, _xy[2u * _i + 1u]); _y1 = rde_math_max_f64(_y1, _xy[2u * _i + 1u]);
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

// The most corners fzs_star_of draws: a star of 24 points, or a polygon of 48 sides. Its callers ask 12 points at
// most; a bigger one is drawn with these many, so its corners always fit the array they are made in.
#define FZS_STAR_CORNERS 48u

// A star of _n points (_inner: its notches' radius, its points' 1; 1: a polygon of _n sides), the first point up,
// filling the box. Past FZS_STAR_CORNERS corners, as many as that.
RDE_INTERNAL void fzs_star_of(fude_zoom_sym* _s, u32 _n, f64 _inner) {
    f64 _xy[2u * FZS_STAR_CORNERS];
    const u32 _want = _inner < 1.0 ? 2u * _n : _n;
    const u32 _m    = _want < FZS_STAR_CORNERS ? _want : FZS_STAR_CORNERS;   // (even: a star's points and notches alternate)
    for(u32 _i = 0; _i < _m; _i++) {
        const f64 _a = RDE_PI * 0.5 + 2.0 * RDE_PI * (f64)_i / (f64)_m;
        const f64 _r = _inner < 1.0 && (_i & 1u) ? _inner : 1.0;
        _xy[2u * _i] = rde_math_cos_f64(_a) * _r;
        _xy[2u * _i + 1u] = rde_math_sin_f64(_a) * _r;
    }
    fzs_fit(_s, _xy, _m, FZS_OUT);
}

// Rectangle.
void fzs_rectangle(fude_zoom_sym* _s)  { fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT); }
// Rounded rectangle.
void fzs_round_rect(fude_zoom_sym* _s) { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.3, FZS_OUT); }
// Circle: an ellipse filling the box.
void fzs_circle(fude_zoom_sym* _s)     { fude_zoom_sym_ellipse(_s, 0, 0, _s->hw, _s->hh, FZS_OUT); }
// Parallelogram: leaning right, its slant the same however wide it is.
void fzs_parallelogram(fude_zoom_sym* _s) {
    const f64 _k = rde_math_min_f64(0.5, _s->hh / _s->hw * 0.5);   // (its slant the same however wide)
    const f64 _d[8] = { -1, -1, 1 - _k, -1, 1, 1, -1 + _k, 1 };
    fude_zoom_sym_poly(_s, _d, 4u, FZS_OUT);
}
// Trapezoid: narrower at the top, its slant the same however wide it is.
void fzs_trapezoid(fude_zoom_sym* _s) {
    const f64 _k = rde_math_min_f64(0.5, _s->hh / _s->hw * 0.5);
    const f64 _d[8] = { -1, -1, 1, -1, 1 - _k, 1, -1 + _k, 1 };
    fude_zoom_sym_poly(_s, _d, 4u, FZS_OUT);
}
// Right triangle: its right angle at the bottom left.
void fzs_right_triangle(fude_zoom_sym* _s) { const f64 _d[6] = { -1, -1, 1, -1, -1, 1 }; fude_zoom_sym_poly(_s, _d, 3u, FZS_OUT); }
// Pentagon: a corner up, filling the box.
void fzs_pentagon(fude_zoom_sym* _s)   { fzs_star_of(_s, 5u, 1.0); }
// Octagon: its sides square to the box.
void fzs_octagon(fude_zoom_sym* _s)    { fude_zoom_sym_regular(_s, 8u, RDE_PI / 8.0, FZS_OUT); }
// Star: five points.
void fzs_star(fude_zoom_sym* _s)       { fzs_star_of(_s, 5u, 0.382); }
// Burst: twelve short points.
void fzs_burst(fude_zoom_sym* _s)      { fzs_star_of(_s, 12u, 0.74); }
// Cross: a plus of bars a third of the box thick.
void fzs_cross(fude_zoom_sym* _s) {
    const f64 _t = 0.34;
    const f64 _d[24] = { -_t, 1, _t, 1, _t, _t, 1, _t, 1, -_t, _t, -_t, _t, -1, -_t, -1, -_t, -_t, -1, -_t, -1, _t, -_t, _t };
    fude_zoom_sym_poly(_s, _d, 12u, FZS_OUT);
}
// Heart: the classic curve, stretched to the box.
void fzs_heart(fude_zoom_sym* _s) {
    // The classic curve: x = 16 sin³t, y = 13 cos t − 5 cos 2t − 2 cos 3t − cos 4t.
    f64 _xy[2u * 64u];
    for(u32 _i = 0; _i < 64u; _i++) {
        const f64 _t = 2.0 * RDE_PI * (f64)_i / 64.0, _sn = rde_math_sin_f64(_t);
        _xy[2u * _i] = 16.0 * _sn * _sn * _sn;
        _xy[2u * _i + 1u] = 13.0 * rde_math_cos_f64(_t) - 5.0 * rde_math_cos_f64(2.0 * _t) - 2.0 * rde_math_cos_f64(3.0 * _t) - rde_math_cos_f64(4.0 * _t);
    }
    fzs_fit(_s, _xy, 64u, FZS_OUT);
}

// Speech bubble: a rounded body over its tail, down from its left (its text in its body).
void fzs_speech(fude_zoom_sym* _s) {
    const f64 _w = _s->hw, _h = _s->hh, _yb = -_h * FZS_BUBBLE_FOOT;
    const f64 _r = rde_math_min_f64(_w, (_h - _yb) * 0.5) * 0.45, _q = RDE_PI * 0.5;
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
// Thought bubble: a cloud of scallops over two little rounds.
void fzs_thought(fude_zoom_sym* _s) {
    const f64 _w = _s->hw, _h = _s->hh, _cy = _h * 0.22, _ry = _h * 0.78;
    fude_zoom_sym_begin(_s);
    const u32 _k = 9u * 12u;
    for(u32 _i = 0; _i < _k; _i++) {
        const f64 _a = 2.0 * RDE_PI * (f64)_i / (f64)_k;
        const f64 _f = 0.86 + 0.14 * rde_math_abs_f64(rde_math_sin_f64(_a * 4.5));
        fude_zoom_sym_pt(_s, rde_math_cos_f64(_a) * _w * _f, _cy + rde_math_sin_f64(_a) * _ry * _f);
    }
    fude_zoom_sym_end(_s, FZS_OUT);
    const f64 _m = FZS_R(_s);
    fude_zoom_sym_ellipse(_s, -_w * 0.5, -_h * 0.72, _m * 0.13, _m * 0.13, FZS_OUT);
    fude_zoom_sym_ellipse(_s, -_w * 0.7, -_h * 0.92, _m * 0.08, _m * 0.08, FZS_OUT);
}
// Arc: the top half of an ellipse whose middle is the box's bottom.
void fzs_arc(fude_zoom_sym* _s) {
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, 0.0, -_s->hh, _s->hw, 2.0 * _s->hh, 0.0, RDE_PI);
    fude_zoom_sym_end(_s, 0u);
}
// Pie: three quarters of a round.
void fzs_pie(fude_zoom_sym* _s) {
    // Three quarters of a round: a slice out of its top right.
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, 0.0, 0.0);
    fude_zoom_sym_arc_pts(_s, 0.0, 0.0, _s->hw, _s->hh, RDE_PI * 0.5, RDE_PI * 2.0);
    fude_zoom_sym_end(_s, FZS_OUT);
}
// Ring: a round with a round hole.
void fzs_ring(fude_zoom_sym* _s) {
    fude_zoom_sym_ellipse(_s, 0, 0, _s->hw, _s->hh, FZS_OUT);
    fude_zoom_sym_ellipse(_s, 0, 0, _s->hw * 0.55, _s->hh * 0.55, FZS_OUT | FUDE_ZOOM_SYMBOL_JOIN);   // (its hole)
}

// Cube: its front, its top and its right side (its text on its front).
void fzs_cube(fude_zoom_sym* _s) {
    const f64 _d = FZS_CUBE_DEPTH, _f = 1.0 - _d;
    const f64 _o[12] = { -1, -1, _f, -1, 1, -1 + _d, 1, 1, -1 + _d, 1, -1, _f };
    fude_zoom_sym_poly(_s, _o, 6u, FZS_OUT);
    const f64 _l[6] = { -1, _f, _f, _f, _f, -1 };
    fude_zoom_sym_poly(_s, _l, 3u, 0u);
    fude_zoom_sym_line(_s, _f, _f, 1, 1, 0u);
}
// Block arrow: a shaft and a head as long as the arrow is thick, however long it is.
void fzs_block_arrow(fude_zoom_sym* _s) {
    const f64 _w = _s->hw, _h = _s->hh, _head = rde_math_min_f64(_w, _h * 1.1), _t = _h * 0.42;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_w, -_t); fude_zoom_sym_pt(_s, _w - _head, -_t); fude_zoom_sym_pt(_s, _w - _head, -_h); fude_zoom_sym_pt(_s, _w, 0.0);
    fude_zoom_sym_pt(_s, _w - _head, _h); fude_zoom_sym_pt(_s, _w - _head, _t); fude_zoom_sym_pt(_s, -_w, _t);
    fude_zoom_sym_end(_s, FZS_OUT);
}
// Double block arrow: a head at each end.
void fzs_double_arrow(fude_zoom_sym* _s) {
    const f64 _w = _s->hw, _h = _s->hh, _head = rde_math_min_f64(_w * 0.5, _h * 1.1), _t = _h * 0.42;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_w, 0.0); fude_zoom_sym_pt(_s, -_w + _head, -_h); fude_zoom_sym_pt(_s, -_w + _head, -_t);
    fude_zoom_sym_pt(_s, _w - _head, -_t); fude_zoom_sym_pt(_s, _w - _head, -_h); fude_zoom_sym_pt(_s, _w, 0.0);
    fude_zoom_sym_pt(_s, _w - _head, _h); fude_zoom_sym_pt(_s, _w - _head, _t); fude_zoom_sym_pt(_s, -_w + _head, _t);
    fude_zoom_sym_pt(_s, -_w + _head, _h);
    fude_zoom_sym_end(_s, FZS_OUT);
}
// Curved arrow: a band arching over from its left foot to a head on its right.
void fzs_curved_arrow(fude_zoom_sym* _s) {
    // A band over the top of an ellipse, from its left foot round to a head pointing down on its right.
    const f64 _e = 0.6, _ri[2] = { 0.55, 1.2 }, _ro[2] = { 0.85, 1.75 };
    f64 _xy[2u * 64u];
    u32 _n = 0;
    const u32 _k = 24u;
    for(u32 _i = 0; _i <= _k; _i++) {   // (its outer side, from its left foot over the top)
        const f64 _a = RDE_PI + (_e - RDE_PI) * (f64)_i / (f64)_k;
        _xy[2u * _n] = rde_math_cos_f64(_a) * _ro[0]; _xy[2u * _n + 1u] = -1.0 + rde_math_sin_f64(_a) * _ro[1]; _n++;
    }
    const f64 _po[2] = { rde_math_cos_f64(_e) * _ro[0], -1.0 + rde_math_sin_f64(_e) * _ro[1] }, _pi[2] = { rde_math_cos_f64(_e) * _ri[0], -1.0 + rde_math_sin_f64(_e) * _ri[1] };
    const f64 _mid[2] = { (_po[0] + _pi[0]) * 0.5, (_po[1] + _pi[1]) * 0.5 };
    f64 _dir[2] = { rde_math_sin_f64(_e) * (_ro[0] + _ri[0]) * 0.5, -rde_math_cos_f64(_e) * (_ro[1] + _ri[1]) * 0.5 };
    const f64 _dl = rde_math_hypot_f64(_dir[0], _dir[1]);
    _dir[0] /= _dl; _dir[1] /= _dl;
    _xy[2u * _n] = _po[0] + (_po[0] - _mid[0]) * 0.7; _xy[2u * _n + 1u] = _po[1] + (_po[1] - _mid[1]) * 0.7; _n++;
    _xy[2u * _n] = _mid[0] + _dir[0] * 0.75; _xy[2u * _n + 1u] = _mid[1] + _dir[1] * 0.75; _n++;
    _xy[2u * _n] = _pi[0] + (_pi[0] - _mid[0]) * 0.7; _xy[2u * _n + 1u] = _pi[1] + (_pi[1] - _mid[1]) * 0.7; _n++;
    for(u32 _i = 0; _i <= _k; _i++) {   // (its inner side, back to its left foot)
        const f64 _a = _e + (RDE_PI - _e) * (f64)_i / (f64)_k;
        _xy[2u * _n] = rde_math_cos_f64(_a) * _ri[0]; _xy[2u * _n + 1u] = -1.0 + rde_math_sin_f64(_a) * _ri[1]; _n++;
    }
    fzs_fit(_s, _xy, _n, FZS_OUT);
}
// Chevron: a band pointing right, notched on its left.
void fzs_chevron(fude_zoom_sym* _s) {
    const f64 _w = _s->hw, _h = _s->hh, _d = rde_math_min_f64(_w * 0.5, _h * 0.6);
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

// The radius of a two-circle Venn diagram's circles for half sizes _hw × _hh. Venn diagrams' circles are not filled,
// so each is seen through the others.
f64 fzs_venn2_r(f64 _hw, f64 _hh) { return rde_math_min_f64(_hh, _hw * 0.62); }
// Venn 2: two overlapping circles side by side.
void fzs_venn2(fude_zoom_sym* _s) {
    const f64 _r = fzs_venn2_r(_s->hw, _s->hh), _d = _s->hw - _r;
    fzs_round(_s, -_d, 0.0, _r, FZS_LOOP);
    fzs_round(_s, _d, 0.0, _r, FZS_LOOP);
}
// The radius of a three-circle Venn diagram's circles.
f64 fzs_venn3_r(f64 _hw, f64 _hh) { return rde_math_min_f64(_hw, _hh) * 0.6; }
// Venn 3: three overlapping circles, one up and two below.
void fzs_venn3(fude_zoom_sym* _s) {
    const f64 _r = fzs_venn3_r(_s->hw, _s->hh), _d = _r * 0.62;
    for(u32 _i = 0; _i < 3u; _i++) {
        const f64 _a = RDE_PI * 0.5 + 2.0 * RDE_PI * (f64)_i / 3.0;
        fzs_round(_s, rde_math_cos_f64(_a) * _d, rde_math_sin_f64(_a) * _d - _s->hh * 0.05, _r, FZS_LOOP);
    }
}

// Geometry's marks (in the box's proportions: a mark rotates with the lasso to sit on what it marks).

// Angle mark: two arms meeting at 50° in the bottom left corner, an arc between them.
void fzs_angle_mark(fude_zoom_sym* _s) {
    const f64 _vx = -_s->hw * 0.85, _vy = -_s->hh * 0.8, _a = 50.0 * RDE_PI / 180.0;
    const f64 _len = rde_math_min_f64((_s->hw - _vx) / rde_math_cos_f64(_a), (_s->hh - _vy) / rde_math_sin_f64(_a));
    fude_zoom_sym_line(_s, -0.85, -0.8, 1.0, -0.8, 0u);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, _vx, _vy);
    fude_zoom_sym_pt(_s, _vx + rde_math_cos_f64(_a) * _len, _vy + rde_math_sin_f64(_a) * _len);
    fude_zoom_sym_end(_s, 0u);
    const f64 _r = FZS_R(_s) * 0.6;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, _vx, _vy, _r, _r, 0.0, _a);
    fude_zoom_sym_end(_s, 0u);
}
// Right angle mark: two arms meeting square in the bottom left corner, a little square in the corner.
void fzs_right_angle_mark(fude_zoom_sym* _s) {
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
// Equal mark: one tick.
void fzs_tick1(fude_zoom_sym* _s) { fzs_ticks(_s, 1u); }
// Double equal mark: two ticks.
void fzs_tick2(fude_zoom_sym* _s) { fzs_ticks(_s, 2u); }
// Triple equal mark: three ticks.
void fzs_tick3(fude_zoom_sym* _s) { fzs_ticks(_s, 3u); }
// Arrowheads along a side (parallels): _n of them.
RDE_INTERNAL void fzs_parallels(fude_zoom_sym* _s, u32 _n) {
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _u = _n == 1u ? 0.3 : (_i == 0u ? -0.15 : 0.75);
        const f64 _d[6] = { _u - 0.6, 1.0, _u, 0.0, _u - 0.6, -1.0 };
        fude_zoom_sym_poly(_s, _d, 3u, 0u);
    }
}
// Parallel mark: one arrowhead.
void fzs_parallel1(fude_zoom_sym* _s) { fzs_parallels(_s, 1u); }
// Double parallel mark: two arrowheads.
void fzs_parallel2(fude_zoom_sym* _s) { fzs_parallels(_s, 2u); }
// Point: a solid dot.
void fzs_point(fude_zoom_sym* _s) { fzs_round(_s, 0.0, 0.0, FZS_R(_s) * 0.45, FZS_SOLID); }
// Segment: a line with a dot at each end.
void fzs_segment(fude_zoom_sym* _s) {
    fude_zoom_sym_line(_s, -0.9, 0.0, 0.9, 0.0, 0u);
    const f64 _r = _s->hh * 0.45;
    fzs_round(_s, -_s->hw * 0.9, 0.0, _r, FZS_SOLID);
    fzs_round(_s, _s->hw * 0.9, 0.0, _r, FZS_SOLID);
}
// Ray: a dot and a line from it, its head on the right.
void fzs_ray(fude_zoom_sym* _s) {
    fude_zoom_sym_arrow(_s, -_s->hw * 0.9, 0.0, _s->hw, 0.0, _s->hh * 1.6);
    fzs_round(_s, -_s->hw * 0.9, 0.0, _s->hh * 0.45, FZS_SOLID);
}
// Line: a head at each end.
void fzs_line_both(fude_zoom_sym* _s) {
    fude_zoom_sym_arrow(_s, -_s->hw * 0.2, 0.0, _s->hw, 0.0, _s->hh * 1.6);
    fude_zoom_sym_arrow(_s, _s->hw * 0.2, 0.0, -_s->hw, 0.0, _s->hh * 1.6);
}
// Vector: an arrow from the bottom left corner to the top right.
void fzs_vector(fude_zoom_sym* _s) { fude_zoom_sym_arrow(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.5); }
// Circle and radius: a circle, its middle dotted, a radius out to its top right.
void fzs_circle_radius(fude_zoom_sym* _s) {
    fzs_round(_s, 0.0, 0.0, FZS_R(_s), FZS_LOOP);
    fzs_round(_s, 0.0, 0.0, FZS_R(_s) * 0.05, FZS_SOLID);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, 0.0, 0.0);
    fude_zoom_sym_pt(_s, FZS_R(_s) * 0.7071, FZS_R(_s) * 0.7071);
    fude_zoom_sym_end(_s, 0u);
}
// Protractor: a half round with its ticks, its middle marked.
void fzs_protractor(fude_zoom_sym* _s) {
    // Half a round on its flat side, a tick every 10°, longer every 30°, and its middle marked.
    const f64 _r = rde_math_min_f64(_s->hw, _s->hh * 2.0), _y = -_s->hh;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, 0.0, _y, _r, _r, 0.0, RDE_PI);
    fude_zoom_sym_end(_s, FZS_OUT);
    for(u32 _i = 0; _i <= 18u; _i++) {
        const f64 _a = RDE_PI * (f64)_i / 18.0, _in = _i % 3u == 0u ? 0.82 : 0.9;
        fude_zoom_sym_begin(_s);
        fude_zoom_sym_pt(_s, rde_math_cos_f64(_a) * _r, _y + rde_math_sin_f64(_a) * _r);
        fude_zoom_sym_pt(_s, rde_math_cos_f64(_a) * _r * _in, _y + rde_math_sin_f64(_a) * _r * _in);
        fude_zoom_sym_end(_s, 0u);
    }
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, 0.0, _y, _r * 0.25, _r * 0.25, 0.0, RDE_PI);
    fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_line(_s, 0.0, -1.0, 0.0, -1.0 + 0.3 * _r / _s->hh, 0u);
}

// Science: glassware (open at the top: their box what they are taken by), optics, an atom, a cell, DNA.

// An open outline of _n u, v pairs: glassware's sides, open at the top.
RDE_INTERNAL void fzs_open(fude_zoom_sym* _s, const f64* _uv, u32 _n) { fude_zoom_sym_poly(_s, _uv, _n, 0u); }
// _n ticks _len long (u) up a side at _x, from _v0 to _v1 evenly (one alone: half way; none for 0).
RDE_INTERNAL void fzs_graduations(fude_zoom_sym* _s, f64 _x, f64 _v0, f64 _v1, u32 _n, f64 _len) {
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _v = _v0 + (_v1 - _v0) * (_n > 1u ? (f64)_i / (f64)(_n - 1u) : 0.5);
        fude_zoom_sym_line(_s, _x, _v, _x - _len, _v, 0u);
    }
}
// Beaker: open at the top, its lip turned out, graduations up its right.
void fzs_beaker(fude_zoom_sym* _s) {
    const f64 _d[10] = { -1.0, 1.0, -0.85, 0.85, -0.85, -1.0, 0.85, -1.0, 0.85, 1.0 };
    fzs_open(_s, _d, 5u);
    fzs_graduations(_s, 0.85, -0.6, 0.6, 4u, 0.25);
}
// Flask: a conical flask, its neck's rim.
void fzs_flask(fude_zoom_sym* _s) {
    const f64 _d[12] = { -0.25, 1.0, -0.25, 0.35, -0.95, -1.0, 0.95, -1.0, 0.25, 0.35, 0.25, 1.0 };
    fzs_open(_s, _d, 6u);
    fude_zoom_sym_line(_s, -0.32, 1.0, 0.32, 1.0, 0u);
}
// Round flask: a round body and a straight neck.
void fzs_round_flask(fude_zoom_sym* _s) {
    const f64 _r = rde_math_min_f64(_s->hw, _s->hh * 0.65), _cy = -_s->hh + _r;
    const f64 _neck = rde_math_min_f64(_r * 0.3, _s->hw * 0.3);
    const f64 _a = rde_math_asin_f64(_neck / _r);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_neck, _s->hh);
    fude_zoom_sym_arc_pts(_s, 0.0, _cy, _r, _r, RDE_PI * 0.5 + _a, RDE_PI * 2.5 - _a);
    fude_zoom_sym_pt(_s, _neck, _s->hh);
    fude_zoom_sym_end(_s, 0u);
}
// Test tube: round at the bottom, its rim across the top.
void fzs_test_tube(fude_zoom_sym* _s) {
    const f64 _w = _s->hw * 0.7, _r = _w;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_w, _s->hh);
    fude_zoom_sym_arc_pts(_s, 0.0, -_s->hh + _r, _w, _r, RDE_PI, 2.0 * RDE_PI);
    fude_zoom_sym_pt(_s, _w, _s->hh);
    fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_line(_s, -0.9, 1.0, 0.9, 1.0, 0u);
}
// Measuring cylinder: a narrow tube on a foot, graduations up its right.
void fzs_cylinder_measure(fude_zoom_sym* _s) {
    const f64 _d[8] = { -0.4, 1.0, -0.4, -0.85, 0.4, -0.85, 0.4, 1.0 };
    fzs_open(_s, _d, 4u);
    fude_zoom_sym_rect(_s, -0.8, -1.0, 0.8, -0.85, FZS_OUT);
    fzs_graduations(_s, 0.4, -0.6, 0.8, 8u, 0.2);
}
// Funnel: its cone and its stem, as two open sides.
void fzs_funnel(fude_zoom_sym* _s) {
    const f64 _l[6] = { -1.0, 1.0, -0.15, -0.15, -0.15, -1.0 }, _r[6] = { 1.0, 1.0, 0.15, -0.15, 0.15, -1.0 };
    fzs_open(_s, _l, 3u);
    fzs_open(_s, _r, 3u);
}
// Burner: a tube on a base and an orange flame over it.
void fzs_burner(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -0.22, -0.75, 0.22, 0.35, FZS_OUT);
    fude_zoom_sym_rect(_s, -0.8, -1.0, 0.8, -0.75, FZS_OUT);
    // its flame: a drop, round low, pointed at its top
    fude_zoom_sym_begin(_s);
    const u32 _n = 16u;
    for(u32 _i = 0; _i <= _n; _i++) {
        const f64 _t = (f64)_i / (f64)_n;
        fude_zoom_sym_uv(_s, -0.88 * (1.0 - _t) * rde_math_sqrt_f64(_t), 0.38 + 0.62 * _t);
    }
    for(u32 _i = 1; _i < _n; _i++) {
        const f64 _t = 1.0 - (f64)_i / (f64)_n;
        fude_zoom_sym_uv(_s, 0.88 * (1.0 - _t) * rde_math_sqrt_f64(_t), 0.38 + 0.62 * _t);
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
// Convex lens.
void fzs_lens_convex(fude_zoom_sym* _s)  { fzs_lens(_s, true); }
// Concave lens.
void fzs_lens_concave(fude_zoom_sym* _s) { fzs_lens(_s, false); }
// A mirror: its face (flat, or bowed in) and strokes behind it.
RDE_INTERNAL void fzs_mirror(fude_zoom_sym* _s, b8 _curved) {
    const u32 _n = _curved ? 16u : 1u;
    fude_zoom_sym_begin(_s);
    for(u32 _i = 0; _i <= _n; _i++) {
        const f64 _t = -1.0 + 2.0 * (f64)_i / (f64)_n;
        fude_zoom_sym_pt(_s, (_curved ? 0.6 * _t * _t - 0.3 : 0.0) * _s->hw, _t * _s->hh);
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
// Mirror: flat.
void fzs_mirror_flat(fude_zoom_sym* _s)   { fzs_mirror(_s, false); }
// Curved mirror.
void fzs_mirror_curved(fude_zoom_sym* _s) { fzs_mirror(_s, true); }
// Prism: a pale blue triangle.
void fzs_prism(fude_zoom_sym* _s) {
    const f64 _d[6] = { -1, -1, 1, -1, 0, 1 };
    fude_zoom_sym_poly(_s, _d, 3u, FZS_OUT);
    fzs_tint(_s, (rde_color){ 150, 200, 240, 90 }, (rde_color){ 0, 0, 0, 0 }, 0.0f);
}
// Light source: a yellow round with eight rays.
void fzs_lamp(fude_zoom_sym* _s) {
    const f64 _r = FZS_R(_s);
    fzs_round(_s, 0.0, 0.0, _r * 0.45, FZS_OUT);
    fzs_tint(_s, (rde_color){ 250, 220, 90, 255 }, (rde_color){ 0, 0, 0, 0 }, 0.0f);
    for(u32 _i = 0; _i < 8u; _i++) {
        const f64 _a = 2.0 * RDE_PI * (f64)_i / 8.0;
        fude_zoom_sym_begin(_s);
        fude_zoom_sym_pt(_s, rde_math_cos_f64(_a) * _r * 0.6, rde_math_sin_f64(_a) * _r * 0.6);
        fude_zoom_sym_pt(_s, rde_math_cos_f64(_a) * _r, rde_math_sin_f64(_a) * _r);
        fude_zoom_sym_end(_s, 0u);
    }
}
// Atom: three orbits round its nucleus.
void fzs_atom(fude_zoom_sym* _s) {
    // Three orbits (ellipses turned 60° from each other) round its nucleus.
    for(u32 _k = 0; _k < 3u; _k++) {
        const f64 _t = RDE_PI * (f64)_k / 3.0, _c = rde_math_cos_f64(_t), _sn = rde_math_sin_f64(_t);
        fude_zoom_sym_begin(_s);
        for(u32 _i = 0; _i < 48u; _i++) {
            const f64 _a = 2.0 * RDE_PI * (f64)_i / 48.0;
            const f64 _x = rde_math_cos_f64(_a) * _s->hw, _y = rde_math_sin_f64(_a) * _s->hh * 0.32;
            fude_zoom_sym_pt(_s, _x * _c - _y * _sn * (_s->hh / _s->hw), _x * _sn * (_s->hh / _s->hw) + _y * _c);
        }
        fude_zoom_sym_end(_s, FZS_LOOP);
    }
    fzs_round(_s, 0.0, 0.0, FZS_R(_s) * 0.16, FZS_SOLID);
}
// Benzene ring: a hexagon with a circle in it.
void fzs_benzene(fude_zoom_sym* _s) {
    fude_zoom_sym_regular(_s, 6u, RDE_PI * 0.5, FZS_OUT);
    fude_zoom_sym_ellipse(_s, 0.0, 0.0, _s->hw * 0.55, _s->hh * 0.55, FZS_LOOP);
}
// Cell: an ellipse, its purple nucleus and four small bodies.
void fzs_cell(fude_zoom_sym* _s) {
    fude_zoom_sym_ellipse(_s, 0.0, 0.0, _s->hw, _s->hh, FZS_OUT);
    fude_zoom_sym_ellipse(_s, _s->hw * 0.12, _s->hh * 0.05, _s->hw * 0.28, _s->hh * 0.3, FZS_LOOP);
    fzs_tint(_s, (rde_color){ 170, 120, 220, 110 }, (rde_color){ 0, 0, 0, 0 }, 0.0f);
    const f64 _dots[8] = { -0.55, 0.3, -0.45, -0.4, 0.55, -0.35, 0.6, 0.35 };
    for(u32 _i = 0; _i < 4u; _i++) {
        fude_zoom_sym_ellipse(_s, _dots[2u * _i] * _s->hw, _dots[2u * _i + 1u] * _s->hh, _s->hw * 0.08, _s->hh * 0.06, FZS_LOOP);
    }
}
// DNA: two strands twisting along it, rungs between them.
void fzs_dna(fude_zoom_sym* _s) {
    // Two strands round each other along it, rungs between them.
    for(u32 _k = 0; _k < 2u; _k++) {
        fude_zoom_sym_begin(_s);
        for(u32 _i = 0; _i <= 48u; _i++) {
            const f64 _t = (f64)_i / 48.0;
            fude_zoom_sym_uv(_s, -1.0 + 2.0 * _t, 0.85 * rde_math_sin_f64(_t * 4.0 * RDE_PI + (_k == 0u ? 0.0 : RDE_PI)));
        }
        fude_zoom_sym_end(_s, 0u);
    }
    for(u32 _i = 1; _i < 16u; _i++) {
        const f64 _t = (f64)_i / 16.0, _v = 0.85 * rde_math_sin_f64(_t * 4.0 * RDE_PI);
        if(rde_math_abs_f64(_v) > 0.15) {
            fude_zoom_sym_line(_s, -1.0 + 2.0 * _t, _v, -1.0 + 2.0 * _t, -_v, 0u);
        }
    }
}
// Thermometer: a tube on a red bulb, red up to its middle, graduations up its left.
void fzs_thermometer(fude_zoom_sym* _s) {
    const f64 _w = _s->hw * 0.35, _b = rde_math_min_f64(_s->hw * 0.75, _s->hh * 0.3);
    fude_zoom_sym_rounded(_s, -_w, -_s->hh + _b, _w, _s->hh, _w, FZS_OUT);
    fzs_round(_s, 0.0, -_s->hh + _b, _b, FZS_OUT);
    fzs_tint(_s, (rde_color){ 225, 70, 60, 255 }, (rde_color){ 0, 0, 0, 0 }, 0.0f);
    fude_zoom_sym_rect(_s, -0.15, -1.0 + _b / _s->hh, 0.15, 0.2, FZS_SOLID);
    fzs_tint(_s, (rde_color){ 225, 70, 60, 255 }, (rde_color){ 225, 70, 60, 255 }, 0.0f);
    fzs_graduations(_s, -0.35, -0.2, 0.85, 6u, 0.25);
}

// Stamps, in their own colours: a thick stroke where a pen would write one. The colours they use (FZS_NONE: no fill,
// or the symbol's own line).
#define FZS_GREEN  ((rde_color){ 60, 170, 90, 255 })
#define FZS_RED    ((rde_color){ 220, 70, 60, 255 })
#define FZS_AMBER  ((rde_color){ 245, 170, 40, 255 })
#define FZS_BLUE   ((rde_color){ 70, 130, 230, 255 })
#define FZS_GOLD   ((rde_color){ 245, 195, 50, 255 })
#define FZS_DARK   ((rde_color){ 40, 40, 44, 255 })
#define FZS_WHITE  ((rde_color){ 255, 255, 255, 255 })
#define FZS_NONE   ((rde_color){ 0, 0, 0, 0 })
// A thick stroke along _n u, v pairs in colour _c, _w of the smaller half size wide, as a pen would write it.
RDE_INTERNAL void fzs_stroke(fude_zoom_sym* _s, const f64* _uv, u32 _n, rde_color _c, f64 _w) {
    fude_zoom_sym_poly(_s, _uv, _n, 0u);
    fzs_tint(_s, FZS_NONE, _c, (f32)(_w * FZS_R(_s)));
}
// Check stamp: a green tick.
void fzs_stamp_check(fude_zoom_sym* _s) {
    const f64 _d[6] = { -0.75, 0.0, -0.25, -0.6, 0.8, 0.7 };
    fzs_stroke(_s, _d, 3u, FZS_GREEN, 0.22);
}
// Cross stamp: a red ×.
void fzs_stamp_cross(fude_zoom_sym* _s) {
    const f64 _a[4] = { -0.7, -0.7, 0.7, 0.7 }, _b[4] = { -0.7, 0.7, 0.7, -0.7 };
    fzs_stroke(_s, _a, 2u, FZS_RED, 0.22);
    fzs_stroke(_s, _b, 2u, FZS_RED, 0.22);
}
// Warning stamp: an amber triangle with a dark "!".
void fzs_stamp_warning(fude_zoom_sym* _s) {
    const f64 _d[6] = { -1.0, -0.9, 1.0, -0.9, 0.0, 1.0 };
    fude_zoom_sym_poly(_s, _d, 3u, FZS_OUT);
    fzs_tint(_s, FZS_AMBER, FZS_DARK, (f32)(0.06 * FZS_R(_s)));
    const f64 _bar[4] = { 0.0, 0.45, 0.0, -0.3 };
    fzs_stroke(_s, _bar, 2u, FZS_DARK, 0.15);
    fzs_round(_s, 0.0, -_s->hh * 0.6, FZS_R(_s) * 0.09, FZS_SOLID);
    fzs_tint(_s, FZS_DARK, FZS_DARK, 0.0f);
}
// Question stamp: a blue "?".
void fzs_stamp_question(fude_zoom_sym* _s) {
    fude_zoom_sym_begin(_s);
    const f64 _r = 0.42;
    for(u32 _i = 0; _i <= 20u; _i++) {
        const f64 _a = RDE_PI * (1.15 - 1.55 * (f64)_i / 20.0);
        fude_zoom_sym_uv(_s, rde_math_cos_f64(_a) * _r * 1.2, 0.42 + rde_math_sin_f64(_a) * _r);
    }
    fude_zoom_sym_uv(_s, 0.0, -0.05);
    fude_zoom_sym_uv(_s, 0.0, -0.3);
    fude_zoom_sym_end(_s, 0u);
    fzs_tint(_s, FZS_NONE, FZS_BLUE, (f32)(0.17 * FZS_R(_s)));
    fzs_round(_s, 0.0, -_s->hh * 0.72, FZS_R(_s) * 0.13, FZS_SOLID);
    fzs_tint(_s, FZS_BLUE, FZS_BLUE, 0.0f);
}
// Exclamation stamp: a red "!".
void fzs_stamp_exclamation(fude_zoom_sym* _s) {
    const f64 _bar[4] = { 0.0, 0.85, 0.0, -0.3 };
    fzs_stroke(_s, _bar, 2u, FZS_RED, 0.4);
    fzs_round(_s, 0.0, -_s->hh * 0.72, FZS_R(_s) * 0.13, FZS_SOLID);
    fzs_tint(_s, FZS_RED, FZS_RED, 0.0f);
}
// Info stamp: a blue round with a white "i".
void fzs_stamp_info(fude_zoom_sym* _s) {
    fzs_round(_s, 0.0, 0.0, FZS_R(_s), FZS_OUT);
    fzs_tint(_s, FZS_BLUE, FZS_BLUE, 0.0f);
    const f64 _bar[4] = { 0.0, 0.15, 0.0, -0.6 };
    fzs_stroke(_s, _bar, 2u, FZS_WHITE, 0.16);
    fzs_round(_s, 0.0, _s->hh * 0.45, FZS_R(_s) * 0.11, FZS_SOLID);
    fzs_tint(_s, FZS_WHITE, FZS_WHITE, 0.0f);
}
// Star stamp: a gold star.
void fzs_stamp_star(fude_zoom_sym* _s) {
    fzs_star_of(_s, 5u, 0.42);
    fzs_tint(_s, FZS_GOLD, (rde_color){ 210, 150, 30, 255 }, 0.0f);
}
// Heart stamp: a red heart.
void fzs_stamp_heart(fude_zoom_sym* _s) {
    fzs_heart(_s);
    fzs_tint(_s, FZS_RED, FZS_RED, 0.0f);
}
// Idea stamp: a yellow light bulb.
void fzs_stamp_bulb(fude_zoom_sym* _s) {
    // Its glass round over its neck, and its base's bands.
    const f64 _r = rde_math_min_f64(_s->hw * 0.62, _s->hh * 0.5), _cy = _s->hh - _r - _s->hh * 0.08, _neck = _r * 0.45;
    const f64 _a = rde_math_asin_f64(_neck / _r);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_neck, -_s->hh * 0.35);
    fude_zoom_sym_arc_pts(_s, 0.0, _cy, _r, _r, RDE_PI * 1.5 - (RDE_PI * 0.5 - _a) * 0.6, -RDE_PI * 0.5 + (RDE_PI * 0.5 - _a) * 0.6);
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
// Flag stamp: a red pennant on a pole in the symbol's ink.
void fzs_stamp_flag(fude_zoom_sym* _s) {
    fude_zoom_sym_line(_s, -0.8, -1.0, -0.8, 1.0, 0u);
    fzs_tint(_s, FZS_NONE, FZS_NONE, (f32)(0.07 * FZS_R(_s)));   // (its pole in its ink: seen on any paper)
    const f64 _d[8] = { -0.8, 1.0, 0.9, 0.62, -0.8, 0.2, -0.8, 1.0 };
    fude_zoom_sym_poly(_s, _d, 3u, FZS_OUT);
    fzs_tint(_s, FZS_RED, FZS_RED, 0.0f);
}
// Pin stamp: a red map pin with a white hole.
void fzs_stamp_pin(fude_zoom_sym* _s) {
    // A drop upside down: a round over a point; a hole in it.
    const f64 _r = rde_math_min_f64(_s->hw, _s->hh * 0.62), _cy = _s->hh - _r;
    const f64 _a = rde_math_asin_f64(rde_math_min_f64(_r / (_cy + _s->hh), 1.0));
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, 0.0, -_s->hh);
    fude_zoom_sym_arc_pts(_s, 0.0, _cy, _r, _r, -_a, RDE_PI + _a);
    fude_zoom_sym_end(_s, FZS_OUT);
    fzs_tint(_s, FZS_RED, FZS_RED, 0.0f);
    fzs_round(_s, 0.0, _cy, _r * 0.38, FZS_SOLID);
    fzs_tint(_s, FZS_WHITE, FZS_WHITE, 0.0f);
}
// Clock stamp: a clock face, its ticks and its hands, drawn thick.
void fzs_stamp_clock(fude_zoom_sym* _s) {
    const f64 _r = FZS_R(_s);
    fzs_round(_s, 0.0, 0.0, _r, FZS_OUT);
    for(u32 _i = 0; _i < 12u; _i++) {
        const f64 _a = 2.0 * RDE_PI * (f64)_i / 12.0;
        fude_zoom_sym_begin(_s);
        fude_zoom_sym_pt(_s, rde_math_cos_f64(_a) * _r * 0.85, rde_math_sin_f64(_a) * _r * 0.85);
        fude_zoom_sym_pt(_s, rde_math_cos_f64(_a) * _r * 0.95, rde_math_sin_f64(_a) * _r * 0.95);
        fude_zoom_sym_end(_s, 0u);
    }
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, 0.0, _r * 0.6); fude_zoom_sym_pt(_s, 0.0, 0.0); fude_zoom_sym_pt(_s, _r * 0.42, -_r * 0.2);
    fude_zoom_sym_end(_s, 0u);
    fzs_tint(_s, FZS_NONE, FZS_NONE, (f32)(0.07 * _r));
}

// Brackets (0.1.85): each drawn across from its back (its spine, the side a "(" bulges to) to its front (its open
// side) — t from 0 to 1 — and _h each way along it from its middle; lying along x (_across: under or over a row).

// A bracket's kind: ( [ { ⟨ | ‖ ⌊ ⌈ and their right-hand ones.
typedef enum {
    FZB_ROUND = 0, FZB_SQUARE, FZB_CURLY, FZB_ANGLE, FZB_BAR, FZB_DOUBLE, FZB_FLOOR, FZB_CEILING,
} FZB_;

// A bracket being drawn: the symbol it goes into, where its back and its front are, its half length, and whether it
// lies across (along x) rather than standing up.
typedef struct {
    fude_zoom_sym* s;
    f64            back, front;   // where t is 0 and 1 (x; y when it lies across)
    f64            h;
    b8             across;
} fzs_bk;

// Adds the point _t of the way from the bracket's back to its front, _along from its middle.
RDE_INTERNAL void fzs_bk_pt(const fzs_bk* _k, f64 _t, f64 _along) {
    const f64 _c = _k->back + (_k->front - _k->back) * _t;
    if(_k->across) {
        fude_zoom_sym_pt(_k->s, _along, _c);
    } else {
        fude_zoom_sym_pt(_k->s, _c, _along);
    }
}

// Adds a straight line between two of the bracket's points (t, along).
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
        fzs_bk_pt(_k, _t + rde_math_cos_f64(_a) * _rt, _along + rde_math_sin_f64(_a) * _ra);
    }
}

// Draws a bracket of kind _kind from _back to _front (x, or y when _across), _h each way along it from its middle.
RDE_INTERNAL void fzs_bracket(fude_zoom_sym* _s, u8 _kind, f64 _back, f64 _front, f64 _h, b8 _across) {
    const fzs_bk _k = { _s, _back, _front, _h, _across };
    const f64 _pi = RDE_PI, _h2 = _pi * 0.5;
    const f64 _wide = rde_math_abs_f64(_front - _back);
    const u32 _n = _s->segments / 4u < 6u ? 6u : _s->segments / 4u;
    switch(_kind) {
        case FZB_ROUND: {
            // An arc of a circle's sixth, as wide as it is (the taller, the flatter: as a typeset one stretches).
            const f64 _a = _pi / 3.0;
            fude_zoom_sym_begin(_s);
            for(u32 _i = 0; _i <= 2u * _n; _i++) {
                const f64 _p = -_a + 2.0 * _a * (f64)_i / (f64)(2u * _n);
                fzs_bk_pt(&_k, (1.0 - rde_math_cos_f64(_p)) / (1.0 - rde_math_cos_f64(_a)), _h * rde_math_sin_f64(_p) / rde_math_sin_f64(_a));
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
            const f64 _ra = rde_math_min_f64(_wide * 0.7, _h * 0.25);
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

// One bracket on its own, as tall as its box, a little in from its sides: its back on the left (a "("), or on the
// right.
RDE_INTERNAL void fzs_bracket_left(fude_zoom_sym* _s, u8 _kind)  { fzs_bracket(_s, _kind, -_s->hw * 0.7, _s->hw * 0.7, _s->hh, false); }
// See fzs_bracket_left: its back on the right (a ")").
RDE_INTERNAL void fzs_bracket_right(fude_zoom_sym* _s, u8 _kind) { fzs_bracket(_s, _kind, _s->hw * 0.7, -_s->hw * 0.7, _s->hh, false); }
// Left parenthesis.
void fzs_paren_l(fude_zoom_sym* _s)   { fzs_bracket_left(_s, FZB_ROUND); }
// Right parenthesis.
void fzs_paren_r(fude_zoom_sym* _s)   { fzs_bracket_right(_s, FZB_ROUND); }
// Left square bracket.
void fzs_square_l(fude_zoom_sym* _s)  { fzs_bracket_left(_s, FZB_SQUARE); }
// Right square bracket.
void fzs_square_r(fude_zoom_sym* _s)  { fzs_bracket_right(_s, FZB_SQUARE); }
// Left brace.
void fzs_curly_l(fude_zoom_sym* _s)   { fzs_bracket_left(_s, FZB_CURLY); }
// Right brace.
void fzs_curly_r(fude_zoom_sym* _s)   { fzs_bracket_right(_s, FZB_CURLY); }
// Left angle bracket.
void fzs_angle_l(fude_zoom_sym* _s)   { fzs_bracket_left(_s, FZB_ANGLE); }
// Right angle bracket.
void fzs_angle_r(fude_zoom_sym* _s)   { fzs_bracket_right(_s, FZB_ANGLE); }
// Vertical bar.
void fzs_vbar(fude_zoom_sym* _s)      { fzs_bracket_left(_s, FZB_BAR); }
// Double bar.
void fzs_double_bar(fude_zoom_sym* _s) { fzs_bracket_left(_s, FZB_DOUBLE); }
// Left floor.
void fzs_floor_l(fude_zoom_sym* _s)   { fzs_bracket_left(_s, FZB_FLOOR); }
// Right floor.
void fzs_floor_r(fude_zoom_sym* _s)   { fzs_bracket_right(_s, FZB_FLOOR); }
// Left ceiling.
void fzs_ceiling_l(fude_zoom_sym* _s) { fzs_bracket_left(_s, FZB_CEILING); }
// Right ceiling.
void fzs_ceiling_r(fude_zoom_sym* _s) { fzs_bracket_right(_s, FZB_CEILING); }
// Under brace: a brace lying under a row, its point down, to its text under it.
void fzs_brace_under(fude_zoom_sym* _s) { fzs_bracket(_s, FZB_CURLY, -_s->hh * 0.7, _s->hh * 0.7, _s->hw, true); }
// Over brace: a brace lying over a row, its point up.
void fzs_brace_over(fude_zoom_sym* _s)  { fzs_bracket(_s, FZB_CURLY, _s->hh * 0.7, -_s->hh * 0.7, _s->hw, true); }

// A pair round a text: each as wide as its height asks (a typeset one's), never more than a third of the box.
f64 fzs_pair_width(f64 _hw, f64 _hh) {
    return rde_math_min_f64(_hw * 0.3, rde_math_max_f64(_hh * 0.22, 6.0));
}
// A pair of brackets of kind _kind round a text, one at each side of the box.
RDE_INTERNAL void fzs_pair(fude_zoom_sym* _s, u8 _kind) {
    const f64 _bw = fzs_pair_width(_s->hw, _s->hh);
    fzs_bracket(_s, _kind, -_s->hw, -_s->hw + _bw, _s->hh, false);
    fzs_bracket(_s, _kind, _s->hw, _s->hw - _bw, _s->hh, false);
}
// Parentheses round a text.
void fzs_pair_round(fude_zoom_sym* _s)  { fzs_pair(_s, FZB_ROUND); }
// Square brackets round a text.
void fzs_pair_square(fude_zoom_sym* _s) { fzs_pair(_s, FZB_SQUARE); }
// Braces round a text.
void fzs_pair_curly(fude_zoom_sym* _s)  { fzs_pair(_s, FZB_CURLY); }
// Angle brackets round a text.
void fzs_pair_angle(fude_zoom_sym* _s)  { fzs_pair(_s, FZB_ANGLE); }
// Bars round a text (an absolute value).
void fzs_pair_bar(fude_zoom_sym* _s)    { fzs_pair(_s, FZB_BAR); }
// Double bars round a text (a norm).
void fzs_pair_double(fude_zoom_sym* _s) { fzs_pair(_s, FZB_DOUBLE); }
