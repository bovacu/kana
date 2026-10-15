// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/render_parts.h"

// ===========================================================================
// See render.h. The renderer, shapes (shape.h): a shape's outline from its numbers, its line, its fill and its hatching
// (fude_zoom_render_shape, which hands the shapes that have their own to them); the measures (a dimension, an arrow and
// its heads, a circle's and an angle's sizes); boards (their grain, name and size, and a board cut); sheets (their
// grid, rulers and size, and the rulers stuck to the screen's edges); and guides. A shape's outline and a cut board's
// rings are kept in their own units with their triangles (render.c's kept geometry), and drawn through their place.
//
// The file goes as render.c had it: the measures, boards (a cut board's geometry kept, then drawn), sheets, guides,
// then fude_zoom_render_shape's steps (fude_zoom_render_shape_*, in the order it takes them) and
// fude_zoom_render_shape.
//
// Most drawing functions take _as and _extra: _as, when not NULL, draws the whole object in that colour (the
// selection's glow, or a faint thing), _extra points wider on each side.
//
// Part of the renderer: render_parts.h holds what the modules share and says which module holds what. Internal names
// start fude_zoom_render_ (fude_zoom_draw_ and fude_zoom_clip_ for the clipping). Main thread only, inside the app's 2D
// drawing block, as render.h says.
// ===========================================================================

// --- shapes ----------------------------------------------------------------------------------

// See the header.
void fude_zoom_render_dimension(const fude_zoom_renderer* _r, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _offset, f32 _width, rde_color _color, const c8* _label) {
    const f64 _len = rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y);
    if(_len < 1.0) {
        return;
    }
    const fude_zoom_v2 _d    = { (_b.x - _a.x) / _len, (_b.y - _a.y) / _len };
    const fude_zoom_v2 _left = { -_d.y, _d.x };
    const f64          _side = _offset >= 0.0 ? 1.0 : -1.0;
    const fude_zoom_v2 _a2   = { _a.x + _left.x * _offset, _a.y + _left.y * _offset };
    const fude_zoom_v2 _b2   = { _b.x + _left.x * _offset, _b.y + _left.y * _offset };
    // The extension lines: from just off what it measures to just past its line.
    if(rde_math_abs_f64(_offset) > 3.0) {
        const f64 _gap = 3.0 * _side, _over = _offset + 5.0 * _side;
        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)(_a.x + _left.x * _gap), (f32)(_a.y + _left.y * _gap) }, (rde_vec_2F){ (f32)(_a.x + _left.x * _over), (f32)(_a.y + _left.y * _over) }, _color, 1.0f);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)(_b.x + _left.x * _gap), (f32)(_b.y + _left.y * _gap) }, (rde_vec_2F){ (f32)(_b.x + _left.x * _over), (f32)(_b.y + _left.y * _over) }, _color, 1.0f);
    }
    rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a2.x, (f32)_a2.y }, (rde_vec_2F){ (f32)_b2.x, (f32)_b2.y }, _color, rde_math_max_f32(_width, 1.0f));
    // Its arrows, inside while there is room for them, else outside pointing in.
    const f64 _al = 9.0, _aw = 3.5;
    const f64 _in = _len >= 2.5 * _al ? 1.0 : -1.0;
    for(u32 _end = 0; _end < 2u; _end++) {
        const fude_zoom_v2 _tip = _end == 0u ? _a2 : _b2;
        const f64          _k   = (_end == 0u ? 1.0 : -1.0) * _in;
        const rde_vec_2F   _t[3] = {
            { (f32)_tip.x, (f32)_tip.y },
            { (f32)(_tip.x + _d.x * _al * _k + _left.x * _aw), (f32)(_tip.y + _d.y * _al * _k + _left.y * _aw) },
            { (f32)(_tip.x + _d.x * _al * _k - _left.x * _aw), (f32)(_tip.y + _d.y * _al * _k - _left.y * _aw) },
        };
        rde_rendering_2d_draw_polygon(_t, 3u, _color, NULL);
    }
    // Its number, upright, on the side away from what it measures, on the page's colour.
    if(_label != NULL && _r->font != NULL) {
        const f32 _px = 13.0f;
        const f32 _tw = fude_draw_text_width(_r->font, _r->font_px, _label, _px);
        if((f64)_tw + 12.0 > _len && _len < 40.0) {
            return;   // too short to say it on: left unsaid rather than over its own arrows
        }
        const f64 _lift = 11.0 * _side;
        const rde_vec_2F _c = { (f32)((_a2.x + _b2.x) * 0.5 + _left.x * _lift), (f32)((_a2.y + _b2.y) * 0.5 + _left.y * _lift) };
        rde_rendering_2d_draw_rounded_rectangle_with_border(_c, (rde_vec_2F){ _tw + 10.0f, 18.0f }, 1.0f, 6u, fude_theme_active()->page, 1.0f, fude_theme_active()->page, NULL);
        rde_rendering_2d_draw_text_2(_r->font, _label, (rde_vec_3F){ _c.x - _tw * 0.5f, _c.y - _px * 0.36f, 0.0f }, (rde_vec_2F){ _px / _r->font_px, _px / _r->font_px }, 0.0f, _color);
    }
}

// An arrow (a connector): its line through its points — in dashes when
// dotted — and its heads, pointed or round, sized by its width on screen.
RDE_INTERNAL void fude_zoom_render_arrow_shape(const fude_zoom_object* _o, const f64* _n, u32 _count, fude_zoom_sim _to_screen, const rde_color* _as) {
    const u32 _k = fude_zoom_arrow_points(_n, _count);
    if(_k < 2u || _count < 2u * _k + 2u) {
        return;
    }
    const u32           _heads = fude_zoom_arrow_heads(_n, _count, _k);
    const fude_zoom_sim _all   = fude_zoom_sim_compose(_to_screen, fude_zoom_object_sim(_o));
    const f64           _w     = rde_math_max_f64((f64)_o->radius * fude_zoom_sim_scale(_to_screen), 0.75);   // (its own scale is a connector's stretch: not its width)
    const rde_color     _c     = _as != NULL ? *_as : fude_theme_resolve(_o->color);
    rde_vec_2F _p[FUDE_ZOOM_ARROW_POINTS];
    for(u32 _i = 0; _i < _k; _i++) {
        const fude_zoom_v2 _q = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _n[1u + 2u * _i], _n[2u + 2u * _i] });
        _p[_i] = (rde_vec_2F){ (f32)_q.x, (f32)_q.y };
    }
    const f32 _head = (f32)rde_math_max_f64(_w * 3.5 + 5.0, 8.0);
    // Each end's head (symbol.h: its style, UML's and ER's ends too), and how far the line stops short of it.
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _hp[2] = { rde_arr_new(sizeof(fude_zoom_v2), _heap), rde_arr_new(sizeof(fude_zoom_v2), _heap) };
    rde_arr _hs[2] = { rde_arr_new(sizeof(fude_zoom_symbol_part), _heap), rde_arr_new(sizeof(fude_zoom_symbol_part), _heap) };
    f32 _trim[2] = { 0.0f, 0.0f };   // (the end's, the start's)
    for(u32 _end = 0; _end < 2u; _end++) {
        if(!(_heads & (_end == 0u ? FUDE_ZOOM_ARROW_END : FUDE_ZOOM_ARROW_START))) {
            continue;
        }
        const rde_vec_2F _tip  = _end == 0u ? _p[_k - 1u] : _p[0];
        const rde_vec_2F _from = _end == 0u ? _p[_k - 2u] : _p[1];
        const u32 _style = _end == 0u ? FUDE_ZOOM_ARROW_END_STYLE(_heads) : FUDE_ZOOM_ARROW_START_STYLE(_heads);
        _trim[_end] = (f32)fude_zoom_symbol_head(_style, (_heads & FUDE_ZOOM_ARROW_CIRCLE) != 0u, (fude_zoom_v2){ _tip.x, _tip.y }, (fude_zoom_v2){ _from.x, _from.y },
                                                 _head, &_hp[_end], &_hs[_end]);
    }
    for(u32 _i = 0; _i + 1u < _k; _i++) {
        rde_vec_2F _a = _p[_i], _b = _p[_i + 1u];
        const f32 _dx = _b.x - _a.x, _dy = _b.y - _a.y, _len = rde_math_sqrt_f32(_dx * _dx + _dy * _dy);
        if(_len < 0.5f) {
            continue;
        }
        // The line stops short of a head that would show its end through it (a pointed one, a hollow one).
        if(_i + 2u == _k && _trim[0] > 0.0f) {
            const f32 _t = rde_math_min_f32(_trim[0], _len);
            _b = (rde_vec_2F){ _b.x - _dx / _len * _t, _b.y - _dy / _len * _t };
        }
        if(_i == 0u && _trim[1] > 0.0f) {
            const f32 _t = rde_math_min_f32(_trim[1], _len);
            _a = (rde_vec_2F){ _a.x + _dx / _len * _t, _a.y + _dy / _len * _t };
        }
        if(_heads & FUDE_ZOOM_ARROW_DOTTED) {
            const f32 _dash = (f32)rde_math_max_f64(_w * 3.0, 4.0);
            const f32 _seg  = rde_math_sqrt_f32((_b.x - _a.x) * (_b.x - _a.x) + (_b.y - _a.y) * (_b.y - _a.y));
            for(f32 _t = 0.0f; _t < _seg; _t += _dash * 2.0f) {
                const f32 _t1 = rde_math_min_f32(_t + _dash, _seg);
                rde_rendering_2d_draw_line_1((rde_vec_2F){ _a.x + (_b.x - _a.x) * _t / _seg, _a.y + (_b.y - _a.y) * _t / _seg },
                                             (rde_vec_2F){ _a.x + (_b.x - _a.x) * _t1 / _seg, _a.y + (_b.y - _a.y) * _t1 / _seg }, _c, (f32)(_w * 2.0));
            }
        } else {
            rde_rendering_2d_draw_line_1(_a, _b, _c, (f32)(_w * 2.0));
        }
    }
    const rde_color _page = fude_theme_active()->page;
    for(u32 _end = 0; _end < 2u; _end++) {
        const fude_zoom_v2*          _q = (const fude_zoom_v2*)_hp[_end].memory;
        const fude_zoom_symbol_part* _h = (const fude_zoom_symbol_part*)_hs[_end].memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_hs[_end]); _i++) {
            const u32 _m = _h[_i].count;
            rde_vec_2F _poly[64];
            const u32 _pm = _m < 64u ? _m : 64u;
            for(u32 _j = 0; _j < _pm; _j++) {
                _poly[_j] = (rde_vec_2F){ (f32)_q[_h[_i].first + _j].x, (f32)_q[_h[_i].first + _j].y };
            }
            if((_h[_i].flags & (FUDE_ZOOM_SYMBOL_SOLID | FUDE_ZOOM_SYMBOL_FILLED)) && _pm >= 3u) {
                rde_rendering_2d_draw_polygon(_poly, _pm, (_h[_i].flags & FUDE_ZOOM_SYMBOL_SOLID) ? _c : _page, NULL);   // (convex: a head's are)
            }
            if(!(_h[_i].flags & FUDE_ZOOM_SYMBOL_SOLID) || _as != NULL) {
                const b8 _closed = (_h[_i].flags & FUDE_ZOOM_SYMBOL_CLOSED) != 0u;
                for(u32 _j = 0; _j + (_closed ? 0u : 1u) < _pm; _j++) {
                    rde_rendering_2d_draw_line_1(_poly[_j], _poly[(_j + 1u) % _pm], _c, (f32)rde_math_max_f64(_w * 2.0, 1.0));
                }
            }
        }
        rde_arr_free(&_hp[_end]);
        rde_arr_free(&_hs[_end]);
    }
}

// A dimension of the canvas's: its length in real units on it.
RDE_INTERNAL void fude_zoom_render_dimension_shape(fude_zoom_renderer* _r, const fude_zoom_scene* _s, const fude_zoom_object* _o, const f64* _n, u32 _count,
                                                   fude_zoom_sim _to_screen, const rde_color* _as) {
    if(_count < 3u) {
        return;
    }
    const fude_zoom_sim _all = fude_zoom_sim_compose(_to_screen, fude_zoom_object_sim(_o));
    const fude_zoom_v2  _a   = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 0.0 });
    const fude_zoom_v2  _b   = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _n[0], _n[1] });
    const f64           _k   = fude_zoom_sim_scale(_all);
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    const f64 _mm   = rde_math_hypot_f64(_n[0], _n[1]) * _o->scale * fude_zoom_sim_scale(fude_zoom_scene_sim(_s, _o->frame, _home)) * _r->mm_per_unit;
    c8 _label[FUDE_ZOOM_UNITS_TEXT];
    fude_zoom_units_format(_mm, &_r->units, _label, sizeof(_label));
    const rde_color _color = _as != NULL ? *_as : fude_theme_resolve(_o->color);
    fude_zoom_render_dimension(_r, _a, _b, _n[2] * _k, (f32)rde_math_max_f64((f64)_o->radius * _k * 2.0, 1.0), _color, _label);
}

// A label on the page's colour, its middle at _c (screen), upright.
RDE_INTERNAL void fude_zoom_render_tag(const fude_zoom_renderer* _r, rde_vec_2F _c, const c8* _label, rde_color _color) {
    if(_r->font == NULL) {
        return;
    }
    const f32 _px = 13.0f;
    const f32 _tw = fude_draw_text_width(_r->font, _r->font_px, _label, _px);
    rde_rendering_2d_draw_rounded_rectangle_with_border(_c, (rde_vec_2F){ _tw + 10.0f, 18.0f }, 1.0f, 6u, fude_theme_active()->page, 1.0f, fude_theme_active()->page, NULL);
    rde_rendering_2d_draw_text_2(_r->font, _label, (rde_vec_3F){ _c.x - _tw * 0.5f, _c.y - _px * 0.36f, 0.0f }, (rde_vec_2F){ _px / _r->font_px, _px / _r->font_px }, 0.0f, _color);
}

// An arrowhead at _tip pointing along _d (a unit way, screen).
RDE_INTERNAL void fude_zoom_render_head(fude_zoom_v2 _tip, fude_zoom_v2 _d, rde_color _color) {
    const f64 _al = 9.0, _aw = 3.5;
    const rde_vec_2F _t[3] = {
        { (f32)_tip.x, (f32)_tip.y },
        { (f32)(_tip.x - _d.x * _al - _d.y * _aw), (f32)(_tip.y - _d.y * _al + _d.x * _aw) },
        { (f32)(_tip.x - _d.x * _al + _d.y * _aw), (f32)(_tip.y - _d.y * _al - _d.x * _aw) },
    };
    rde_rendering_2d_draw_polygon(_t, 3u, _color, NULL);
}

// A circle's size (RADIAL): its line from the centre (or right across) to the circle, a head on the circle (both,
// across), and "R 20 mm" or "Ø 40 mm" beyond it.
RDE_INTERNAL void fude_zoom_render_radial(fude_zoom_renderer* _r, const fude_zoom_scene* _s, const fude_zoom_object* _o, const f64* _n, u32 _count,
                                          fude_zoom_sim _to_screen, const rde_color* _as) {
    if(_count < 3u) {
        return;
    }
    const fude_zoom_sim _all = fude_zoom_sim_compose(_to_screen, fude_zoom_object_sim(_o));
    const b8  _across = _n[2] >= 0.5;
    const fude_zoom_v2 _u = { rde_math_cos_f64(_n[1]) * _n[0], rde_math_sin_f64(_n[1]) * _n[0] };
    const fude_zoom_v2 _c = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 0.0 });
    const fude_zoom_v2 _e = fude_zoom_sim_apply(_all, _u);
    const fude_zoom_v2 _f = _across ? fude_zoom_sim_apply(_all, (fude_zoom_v2){ -_u.x, -_u.y }) : _c;
    const f64 _len = rde_math_hypot_f64(_e.x - _c.x, _e.y - _c.y);
    if(_len < 2.0) {
        return;
    }
    const rde_color _color = _as != NULL ? *_as : fude_theme_resolve(_o->color);
    const fude_zoom_v2 _d = { (_e.x - _c.x) / _len, (_e.y - _c.y) / _len };
    rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_f.x, (f32)_f.y }, (rde_vec_2F){ (f32)_e.x, (f32)_e.y }, _color, (f32)rde_math_max_f64((f64)_o->radius * fude_zoom_sim_scale(_all) * 2.0, 1.0));
    fude_zoom_render_head(_e, _d, _color);
    if(_across) {
        fude_zoom_render_head(_f, (fude_zoom_v2){ -_d.x, -_d.y }, _color);
    } else {
        rde_rendering_2d_draw_circle((rde_vec_2F){ (f32)_c.x, (f32)_c.y }, 2.0f, 8u, _color, NULL);
    }
    if(_as != NULL || _r->font == NULL) {
        return;
    }
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    const f64 _mm   = _n[0] * _o->scale * fude_zoom_sim_scale(fude_zoom_scene_sim(_s, _o->frame, _home)) * _r->mm_per_unit;
    c8 _v[FUDE_ZOOM_UNITS_TEXT], _label[FUDE_ZOOM_UNITS_TEXT + 8u];
    fude_zoom_units_format(_across ? 2.0 * _mm : _mm, &_r->units, _v, sizeof(_v));
    rde_snprintf(_label, sizeof(_label), _across ? "\xC3\x98 %s" : "R %s", _v);
    const f32 _tw = fude_draw_text_width(_r->font, _r->font_px, _label, 13.0f);
    const f64 _out = (f64)_tw * 0.5 * rde_math_abs_f64(_d.x) + 9.0 * rde_math_abs_f64(_d.y) + 10.0;
    fude_zoom_render_tag(_r, (rde_vec_2F){ (f32)(_e.x + _d.x * _out), (f32)(_e.y + _d.y * _out) }, _label, _color);
}

// An angle's size (ANGLE): its arc round the corner, a head at each end, its degrees beyond its middle.
RDE_INTERNAL void fude_zoom_render_angle(fude_zoom_renderer* _r, const fude_zoom_object* _o, const f64* _n, u32 _count, fude_zoom_sim _to_screen, const rde_color* _as) {
    if(_count < 3u) {
        return;
    }
    const fude_zoom_sim _all = fude_zoom_sim_compose(_to_screen, fude_zoom_object_sim(_o));
    const f64 _k = fude_zoom_sim_scale(_all);
    const f64 _rad = _n[0] * _k;
    if(_rad < 6.0 || _rad > 1e6) {
        return;
    }
    const rde_color _color = _as != NULL ? *_as : fude_theme_resolve(_o->color);
    const fude_zoom_v2 _c = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 0.0 });
    const f64 _turn = rde_math_atan2_f64(_all.b, _all.a);
    const f64 _from = _n[1] + _turn, _sweep = _n[2];
    const u32 _steps = (u32)rde_math_min_f64(rde_math_max_f64(rde_math_abs_f64(_sweep) * _rad / 4.0, 4.0), 256.0);
    rde_vec_2F _prev = { (f32)(_c.x + rde_math_cos_f64(_from) * _rad), (f32)(_c.y + rde_math_sin_f64(_from) * _rad) };
    for(u32 _i = 1; _i <= _steps; _i++) {
        const f64 _a = _from + _sweep * (f64)_i / (f64)_steps;
        const rde_vec_2F _p = { (f32)(_c.x + rde_math_cos_f64(_a) * _rad), (f32)(_c.y + rde_math_sin_f64(_a) * _rad) };
        rde_rendering_2d_draw_line_1(_prev, _p, _color, 1.0f);
        _prev = _p;
    }
    // Its heads, along the arc at each end (inwards, round it).
    const f64 _sg = _sweep >= 0.0 ? 1.0 : -1.0;
    if(rde_math_abs_f64(_sweep) * _rad >= 22.0) {
        const f64 _a0 = _from, _a1 = _from + _sweep;
        fude_zoom_render_head((fude_zoom_v2){ _c.x + rde_math_cos_f64(_a0) * _rad, _c.y + rde_math_sin_f64(_a0) * _rad }, (fude_zoom_v2){ rde_math_sin_f64(_a0) * _sg, -rde_math_cos_f64(_a0) * _sg }, _color);
        fude_zoom_render_head((fude_zoom_v2){ _c.x + rde_math_cos_f64(_a1) * _rad, _c.y + rde_math_sin_f64(_a1) * _rad }, (fude_zoom_v2){ -rde_math_sin_f64(_a1) * _sg, rde_math_cos_f64(_a1) * _sg }, _color);
    }
    if(_as != NULL) {
        return;
    }
    c8 _label[FUDE_ZOOM_UNITS_TEXT];
    fude_zoom_units_format_angle(rde_math_abs_f64(_sweep) * 180.0 / RDE_PI, 1u, _label, sizeof(_label));
    const f64 _mid = _from + _sweep * 0.5;
    const f64 _out = _rad + 16.0;
    fude_zoom_render_tag(_r, (rde_vec_2F){ (f32)(_c.x + rde_math_cos_f64(_mid) * _out), (f32)(_c.y + rde_math_sin_f64(_mid) * _out) }, _label, _color);
}

// A board's marks' colour: dark on a light wood, light on a dark one (walnut), whatever
// the theme's ink; on no fill, its line's.
RDE_INTERNAL rde_color fude_zoom_render_marks_on(b8 _filled, rde_color _fill, rde_color _line) {
    if(!_filled) {
        return _line;
    }
    const b8 _light = 0.299f * (f32)_fill.r + 0.587f * (f32)_fill.g + 0.114f * (f32)_fill.b > 140.0f;
    return _light ? (rde_color){ 62, 46, 28, 255 } : (rde_color){ 246, 238, 226, 255 };
}

// A board's marks over its outline: its grain, an arrow along it, and its
// name and real size in the middle, upright — left unsaid when too small to read.
RDE_INTERNAL void fude_zoom_render_board_marks(const fude_zoom_renderer* _r, const fude_zoom_scene* _s, const fude_zoom_object* _o, const f64* _n, u32 _count,
                                               fude_zoom_sim _all, rde_color _color) {
    if(_count < 4u) {
        return;
    }
    const f64 _k = fude_zoom_sim_scale(_all);
    const f64 _l = _n[0] * _k, _w = _n[1] * _k;   // half sizes on the screen
    if(_l < 12.0 || _w < 6.0) {
        return;
    }
    // The grain: an arrow both ways along it, low in the board.
    const b8  _across = fude_zoom_board_grain(_n, _count) != 0u;
    const f64 _along  = (_across ? _n[1] : _n[0]) * 0.6, _low = (_across ? _n[0] : _n[1]) * 0.55;
    const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, _across ? (fude_zoom_v2){ -_low, -_along } : (fude_zoom_v2){ -_along, -_low });
    const fude_zoom_v2 _b = fude_zoom_sim_apply(_all, _across ? (fude_zoom_v2){ -_low, _along } : (fude_zoom_v2){ _along, -_low });
    rde_color _soft = _color;
    _soft.a = (u8)(_color.a / 2u);
    const f64 _len = rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y);
    if(_len > 16.0) {
        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, _soft, 1.0f);
        const fude_zoom_v2 _d = { (_b.x - _a.x) / _len, (_b.y - _a.y) / _len }, _side = { -_d.y, _d.x };
        for(u32 _end = 0; _end < 2u; _end++) {
            const fude_zoom_v2 _tip = _end == 0u ? _a : _b;
            const f64          _in  = _end == 0u ? 1.0 : -1.0;
            const rde_vec_2F   _t[3] = {
                { (f32)_tip.x, (f32)_tip.y },
                { (f32)(_tip.x + _d.x * 7.0 * _in + _side.x * 3.0), (f32)(_tip.y + _d.y * 7.0 * _in + _side.y * 3.0) },
                { (f32)(_tip.x + _d.x * 7.0 * _in - _side.x * 3.0), (f32)(_tip.y + _d.y * 7.0 * _in - _side.y * 3.0) },
            };
            rde_rendering_2d_draw_polygon(_t, 3u, _soft, NULL);
        }
    }
    if(_r->font == NULL) {
        return;
    }
    // Its name, and its length, width and thickness in the units chosen.
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    const f64 _mm   = _o->scale * fude_zoom_sim_scale(fude_zoom_scene_sim(_s, _o->frame, _home)) * _r->mm_per_unit;
    c8 _name[FUDE_ZOOM_BOARD_NAME], _ls[FUDE_ZOOM_UNITS_TEXT], _ws[FUDE_ZOOM_UNITS_TEXT], _ts[FUDE_ZOOM_UNITS_TEXT], _size[3u * FUDE_ZOOM_UNITS_TEXT + 96u];
    fude_zoom_board_name(_n, _count, _name, sizeof(_name));
    fude_zoom_units_format(_n[0] * 2.0 * _mm, &_r->units, _ls, sizeof(_ls));
    fude_zoom_units_format(_n[1] * 2.0 * _mm, &_r->units, _ws, sizeof(_ws));
    fude_zoom_units_format(_n[2], &_r->units, _ts, sizeof(_ts));
    // ...and what it is made of, after them.
    const u8 _material = fude_zoom_board_material(_n, _count);
    if(_material != FUDE_ZOOM_MATERIAL_NONE && _r->material_words != NULL) {
        rde_snprintf(_size, sizeof(_size), "%s \xC3\x97 %s \xC3\x97 %s \xC2\xB7 %s", _ls, _ws, _ts, _r->material_words[_material]);
    } else {
        rde_snprintf(_size, sizeof(_size), "%s \xC3\x97 %s \xC3\x97 %s", _ls, _ws, _ts);
    }
    const fude_zoom_v2 _c  = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 0.0 });
    const f32 _px = 13.0f;
    const f32 _sw = fude_draw_text_width(_r->font, _r->font_px, _size, _px);
    const f32 _nw = _name[0] != 0 ? fude_draw_text_width(_r->font, _r->font_px, _name, _px + 2.0f) : 0.0f;
    if((f64)rde_math_max_f32(_sw, _nw) > 2.0 * rde_math_max_f64(_l, _w) * 0.9 || 2.0 * rde_math_min_f64(_l, _w) < (_name[0] != 0 ? 40.0 : 22.0)) {
        return;   // too small to say it in
    }
    if(_name[0] != 0) {
        rde_rendering_2d_draw_text_2(_r->font, _name, (rde_vec_3F){ (f32)_c.x - _nw * 0.5f, (f32)_c.y + 3.0f, 0.0f },
                                     (rde_vec_2F){ (_px + 2.0f) / _r->font_px, (_px + 2.0f) / _r->font_px }, 0.0f, _color);
    }
    rde_rendering_2d_draw_text_2(_r->font, _size, (rde_vec_3F){ (f32)_c.x - _sw * 0.5f, (f32)_c.y - (_name[0] != 0 ? _px + 2.0f : _px * 0.36f), 0.0f },
                                 (rde_vec_2F){ _px / _r->font_px, _px / _r->font_px }, 0.0f, _color);
}

// See render_parts.h. Kept by its type, payload, detail and how many of its numbers it is made from
// (render_parts.h's fude_zoom_render_geo_key): a payload's numbers never change, an edit making a new one.
fude_zoom_geo* fude_zoom_render_shape_geo(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, u32 _segments, u32 _numbers) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    fude_zoom_render_geo_key _key;
    rde_memset(&_key, 0, sizeof(_key));
    _key.what     = FUDE_ZOOM_RENDER_GEO_SHAPE;
    _key.kind     = _o->channels;
    _key.blob     = _o->blob;
    _key.serial   = fude_zoom_blob_serial(_s, _o->blob);
    _key.segments = _segments;
    _key.numbers  = _numbers;
    fude_zoom_geo* _g = fude_zoom_render_geo_find(_r, &_key);
    if(_g != NULL) {
        return _g;
    }
    _g = fude_zoom_render_geo_begin(_r);
    fude_zoom_render_fresh(&_g->points, sizeof(fude_zoom_v2));
    const u32 _count = fude_zoom_scene_shape_numbers_all(_s, _object, &_r->numbers);
    fude_zoom_shape_outline(_o->channels, (const f64*)_r->numbers.memory, _count < _numbers ? _count : _numbers, _segments, &_g->points, &_g->closed);
    return fude_zoom_render_geo_keep(_r, &_key);
}

// A cut board's rings in its own units (shape.h's fude_zoom_board_rings: its points, and the ring each is on), as kept
// by its payload, or made now; its triangles are made when its fill first asks (fude_zoom_render_geo_tris).
RDE_INTERNAL fude_zoom_geo* fude_zoom_render_board_geo(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    fude_zoom_render_geo_key _key;
    rde_memset(&_key, 0, sizeof(_key));
    _key.what   = FUDE_ZOOM_RENDER_GEO_BOARD;
    _key.kind   = _o->channels;
    _key.blob   = _o->blob;
    _key.serial = fude_zoom_blob_serial(_s, _o->blob);
    fude_zoom_geo* _g = fude_zoom_render_geo_find(_r, &_key);
    if(_g != NULL) {
        return _g;
    }
    _g = fude_zoom_render_geo_begin(_r);
    fude_zoom_render_fresh(&_g->points, sizeof(fude_zoom_v2));
    fude_zoom_render_fresh(&_g->rings, sizeof(u32));
    const u32 _count = fude_zoom_scene_shape_numbers_all(_s, _object, &_r->numbers);
    fude_zoom_board_rings((const f64*)_r->numbers.memory, _count, &_g->points, &_g->rings);
    _g->closed = true;
    return fude_zoom_render_geo_keep(_r, &_key);
}

// A cut board's fill when it is far bigger than the screen: each of its rings (_n points on the screen, _sp, each's
// ring in _rings) cut to the box round the screen first, then triangulated, as seen once.
RDE_INTERNAL void fude_zoom_render_cut_board_far(fude_zoom_renderer* _r, const fude_zoom_v2* _sp, const u32* _rings, u32 _n, fude_zoom_v2 _guard, rde_color _fill) {
    rde_arr_clear(&_r->shape);
    rde_arr_clear(&_r->ring_ids);
    for(u32 _from = 0; _from < _n;) {
        u32 _to = _from;
        while(_to < _n && _rings[_to] == _rings[_from]) {
            _to++;
        }
        const u32 _ring = _rings[_from];
        const u32 _k    = fude_zoom_fill_clip(&_sp[_from], _to - _from, (fude_zoom_box){ -_guard.x, -_guard.y, _guard.x, _guard.y }, &_r->cut);
        for(u32 _i = 0; _i < _k; _i++) {
            rde_arr_add(&_r->shape, (any)&((const fude_zoom_v2*)_r->cut.memory)[_i]);
            rde_arr_add(&_r->ring_ids, (any)&_ring);
        }
        _from = _to;
    }
    rde_arr_clear(&_r->fill);
    const u32 _t = fude_zoom_fill_triangulate_rings((const fude_zoom_v2*)_r->shape.memory, (const u32*)_r->ring_ids.memory, (u32)rde_arr_length(&_r->shape), &_r->fill);
    fude_zoom_render_triangles((const fude_zoom_v2*)_r->fill.memory, _t, _fill);
}

// A cut board's rings' lines (_n points on the screen, _sp, each's ring in _rings), each closed: one stroke each, or
// cut to the screen first when the board is _far bigger than it.
RDE_INTERNAL void fude_zoom_render_cut_board_lines(fude_zoom_renderer* _r, const fude_zoom_v2* _sp, const u32* _rings, u32 _n, b8 _far, fude_zoom_v2 _guard,
                                                   f64 _radius, rde_color _color) {
    for(u32 _from = 0; _from < _n;) {
        u32 _to = _from;
        while(_to < _n && _rings[_to] == _rings[_from]) {
            _to++;
        }
        const u32 _k = _to - _from;
        if(_k >= 2u && _far) {
            rde_arr_clear(&_r->cut);
            rde_arr_clear(&_r->shape_radii);
            fude_zoom_v2* _ring = (fude_zoom_v2*)rde_arr_add_n(&_r->cut, (usize)_k + 1u);
            f64*          _rad  = (f64*)rde_arr_add_n(&_r->shape_radii, (usize)_k + 1u);
            rde_memcpy(_ring, &_sp[_from], (usize)_k * sizeof(fude_zoom_v2));
            _ring[_k] = _ring[0];
            for(u32 _i = 0; _i <= _k; _i++) {
                _rad[_i] = _radius;
            }
            fude_zoom_draw_huge_stroke(_ring, _rad, _k + 1u, _guard, _color);
        } else if(_k >= 2u) {
            rde_arr_clear(&_r->screen);
            rde_arr_clear(&_r->radii);
            rde_vec_2F* _q  = rde_arr_add_n(&_r->screen, _k + 1u);
            f32*        _rr = rde_arr_add_n(&_r->radii, _k + 1u);
            for(u32 _i = 0; _i <= _k; _i++) {
                const fude_zoom_v2 _at = _sp[_from + _i % _k];
                _q[_i]  = (rde_vec_2F){ (f32)_at.x, (f32)_at.y };
                _rr[_i] = (f32)_radius;
            }
            rde_rendering_2d_draw_stroke(_q, _rr, _k + 1u, _color);
        }
        _from = _to;
    }
}

// A board cut (cut.h): its material filled (its holes and notches left out), each of its rings drawn as its line, its
// marks over it. Its rings and their triangles are kept in its own units (fude_zoom_render_board_geo) and drawn
// through its place; one far bigger than the screen has each ring cut to it first.
RDE_INTERNAL void fude_zoom_render_cut_board(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half,
                                             const rde_color* _as, f32 _extra) {
    const fude_zoom_object* _o      = fude_zoom_scene_object(_s, _object);
    fude_zoom_geo*          _g      = fude_zoom_render_board_geo(_r, _s, _object);
    const u32               _n      = (u32)rde_arr_length(&_g->points);
    const fude_zoom_v2*     _pts    = (const fude_zoom_v2*)_g->points.memory;
    const u32*              _rings  = (const u32*)_g->rings.memory;
    const fude_zoom_sim     _all    = fude_zoom_sim_compose(_to_screen, fude_zoom_object_sim(_o));
    const rde_color         _color  = _as != NULL ? *_as : fude_theme_resolve(_o->color);
    const f64               _radius = rde_math_max_f64((f64)_o->radius * fude_zoom_sim_scale(_all), (f64)FUDE_ZOOM_RENDER_MIN_RADIUS) + (f64)_extra;
    rde_arr_clear(&_r->shape_screen);
    fude_zoom_v2* _sp = (fude_zoom_v2*)rde_arr_add_n(&_r->shape_screen, (usize)_n + 1u);
    b8 _far = _radius >= FUDE_ZOOM_RENDER_FAR;
    for(u32 _i = 0; _i < _n; _i++) {
        _sp[_i] = fude_zoom_sim_apply(_all, _pts[_i]);
        _far = _far || rde_math_abs_f64(_sp[_i].x) >= FUDE_ZOOM_RENDER_FAR || rde_math_abs_f64(_sp[_i].y) >= FUDE_ZOOM_RENDER_FAR;
    }
    // Its fill: the material only — its kept triangles through its place, or each ring cut to round the screen first
    // when it is far bigger than it.
    const b8 _filled = (_o->flags & FUDE_ZOOM_FLAG_FILLED) && _as == NULL;
    const rde_color _fill = (_o->flags & FUDE_ZOOM_FLAG_FILL_OWN) ? fude_theme_resolve(_o->fill) : _color;
    const fude_zoom_v2 _guard = { _half.x * 2.0, _half.y * 2.0 };
    if(_filled && _n >= 3u && _far) {
        fude_zoom_render_cut_board_far(_r, _sp, _rings, _n, _guard, _fill);
    } else if(_filled && _n >= 3u) {
        const u32 _t = fude_zoom_render_geo_tris(_r, _g);
        fude_zoom_render_triangles_through((const fude_zoom_v2*)_g->tris.memory, _t, _all, _fill);
    }
    fude_zoom_render_cut_board_lines(_r, _sp, _rings, _n, _far, _guard, _radius, _color);
    if(_as == NULL) {
        const u32 _count = fude_zoom_scene_shape_numbers_all(_s, _object, &_r->numbers);
        fude_zoom_render_board_marks(_r, _s, _o, (const f64*)_r->numbers.memory, _count, _all, fude_zoom_render_marks_on(_filled, _fill, _color));
    }
    _r->strokes_drawn++;
    _r->points_drawn += _n;
}

// A sheet's marks over its outline (sheet.h): its grid's lines, its rulers' ticks and numbers (upright, each
// beside its tick), its unit by its first corner, and over its top left corner its size (and its scale).
RDE_INTERNAL void fude_zoom_render_sheet_marks(const fude_zoom_renderer* _r, const fude_zoom_scene* _s, const fude_zoom_object* _o, const f64* _n, u32 _count,
                                               fude_zoom_sim _all, fude_zoom_v2 _half, rde_color _color) {
    fude_zoom_sheet _sheet;
    if(!fude_zoom_sheet_of(_n, _count, &_sheet)) {
        return;
    }
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    const f64 _mm   = _o->scale * fude_zoom_sim_scale(fude_zoom_scene_sim(_s, _o->frame, _home)) * _r->mm_per_unit;   // mm one of its own units
    const b8  _inch = _r->units.unit == FUDE_ZOOM_UNIT_IN || _r->units.unit == FUDE_ZOOM_UNIT_FT;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _lines = rde_arr_new(sizeof(fude_zoom_sheet_line), _heap), _labels = rde_arr_new(sizeof(fude_zoom_sheet_label), _heap);
    fude_zoom_sheet_label _unit;
    fude_zoom_sheet_marks(&_sheet, _all, _mm, _inch, (fude_zoom_box){ -_half.x, -_half.y, _half.x, _half.y }, &_lines, &_labels, &_unit);
    rde_color _grid = _color, _strong = _color;
    _grid.a   = (u8)((u32)_color.a * 22u / 100u);
    _strong.a = (u8)((u32)_color.a * 45u / 100u);
    const fude_zoom_sheet_line* _l = (const fude_zoom_sheet_line*)_lines.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_lines); _i++) {
        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_l[_i].a.x, (f32)_l[_i].a.y }, (rde_vec_2F){ (f32)_l[_i].b.x, (f32)_l[_i].b.y },
                                     _l[_i].weight == 0u ? _color : _l[_i].weight == 2u ? _strong : _grid, 1.0f);
    }
    if(_r->font != NULL) {
        const f32 _px = 11.0f;
        const fude_zoom_sheet_label* _t = (const fude_zoom_sheet_label*)_labels.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_labels); _i++) {
            const f32 _w = fude_draw_text_width(_r->font, _r->font_px, _t[_i].text, _px);
            // Its nearer edge on the tick's end: its middle half its width (or height) further in.
            const f64 _out = rde_math_abs_f64(_t[_i].in.x) * (f64)_w * 0.5 + rde_math_abs_f64(_t[_i].in.y) * (f64)_px * 0.5;
            const fude_zoom_v2 _c = { _t[_i].at.x + _t[_i].in.x * _out, _t[_i].at.y + _t[_i].in.y * _out };
            rde_rendering_2d_draw_text_2(_r->font, _t[_i].text, (rde_vec_3F){ (f32)_c.x - _w * 0.5f, (f32)_c.y - _px * 0.36f, 0.0f },
                                         (rde_vec_2F){ _px / _r->font_px, _px / _r->font_px }, 0.0f, _color);
        }
        if(_unit.text[0] != 0) {
            const f32 _w = fude_draw_text_width(_r->font, _r->font_px, _unit.text, 10.0f);
            rde_rendering_2d_draw_text_2(_r->font, _unit.text, (rde_vec_3F){ (f32)_unit.at.x - _w * 0.5f, (f32)_unit.at.y - 3.6f, 0.0f },
                                         (rde_vec_2F){ 10.0f / _r->font_px, 10.0f / _r->font_px }, 0.0f, _strong);
        }
        // Its size over its top left corner (and its scale when it is not printed at its true size).
        const f64 _k = fude_zoom_sim_scale(_all);
        if(2.0 * _sheet.hw * _k >= 120.0) {
            c8 _ws[FUDE_ZOOM_UNITS_TEXT], _hs[FUDE_ZOOM_UNITS_TEXT], _say[2u * FUDE_ZOOM_UNITS_TEXT + 48u];
            fude_zoom_units_format(2.0 * _sheet.hw * _mm, &_r->units, _ws, sizeof(_ws));
            fude_zoom_units_format(2.0 * _sheet.hh * _mm, &_r->units, _hs, sizeof(_hs));
            if(_sheet.scale > 1.0) {
                c8 _sc[24];
                fude_zoom_sheet_number(_sheet.scale, _sc, sizeof(_sc));
                rde_snprintf(_say, sizeof(_say), "%s \xC3\x97 %s  \xC2\xB7  1:%s", _ws, _hs, _sc);
            } else {
                rde_snprintf(_say, sizeof(_say), "%s \xC3\x97 %s", _ws, _hs);
            }
            const fude_zoom_v2 _tl = fude_zoom_sim_apply(_all, (fude_zoom_v2){ -_sheet.hw, _sheet.hh });
            const fude_zoom_v2 _up = { -_all.b / _k, _all.a / _k };   // (its own up on the screen)
            const f32 _tp = 13.0f;
            rde_rendering_2d_draw_text_2(_r->font, _say, (rde_vec_3F){ (f32)(_tl.x + _up.x * 8.0), (f32)(_tl.y + _up.y * 8.0) + 2.0f, 0.0f },
                                         (rde_vec_2F){ _tp / _r->font_px, _tp / _r->font_px }, 0.0f, _color);
        }
    }
    rde_arr_free(&_lines);
    rde_arr_free(&_labels);
}

// See the header. Each band drawn is kept in stuck (four at most), for a guide pulled out of it (page_measure.c).
void fude_zoom_render_sheets_stuck(fude_zoom_renderer* _r, const fude_zoom_scene* _s, fude_zoom_v2 _half) {
    const fude_theme* _theme = fude_theme_active();
    const fude_zoom_box _view = { -_half.x, -_half.y, _half.x, _half.y };
    const b8 _inch = _r->units.unit == FUDE_ZOOM_UNIT_IN || _r->units.unit == FUDE_ZOOM_UNIT_FT;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _found = rde_arr_new(sizeof(u32), _heap);
    rde_arr _lines = rde_arr_new(sizeof(fude_zoom_sheet_line), _heap), _labels = rde_arr_new(sizeof(fude_zoom_sheet_label), _heap);
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    _r->stuck_count = 0;
    for(u32 _v = 0; _v < (u32)rde_arr_length(&_r->visible); _v++) {
        const fude_zoom_visible _vis = ((const fude_zoom_visible*)_r->visible.memory)[_v];
        rde_arr_clear(&_found);
        fude_zoom_scene_query(_s, _vis.frame, fude_zoom_sim_box(fude_zoom_sim_inverse(_vis.to_screen), _view), &_found);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_found); _i++) {
            const u32 _object = ((const u32*)_found.memory)[_i];
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
            if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_SHEET || fude_zoom_scene_hides(_s, _o) ||
               (_r->lifted != NULL && _object < (u32)rde_arr_length(_r->lifted) && ((const u8*)_r->lifted->memory)[_object] != 0u)) {
                continue;
            }
            f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
            fude_zoom_sheet _sheet;
            if(!fude_zoom_sheet_of(_n, fude_zoom_scene_shape_numbers(_s, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS), &_sheet)) {
                continue;
            }
            const fude_zoom_sim _all = fude_zoom_sim_compose(_vis.to_screen, fude_zoom_object_sim(_o));
            const f64 _mm = _o->scale * fude_zoom_sim_scale(fude_zoom_scene_sim(_s, _o->frame, _home)) * _r->mm_per_unit;
            fude_zoom_box _bands[2];
            u32 _nb = 0;
            fude_zoom_sheet_marks_stuck(&_sheet, _all, _mm, _inch, _view, &_lines, &_labels, NULL, _bands, &_nb);
            if(_nb == 0u) {
                continue;
            }
            const rde_color _ink = fude_theme_resolve(_o->color);
            rde_color _paper = _theme->page, _edge = _ink;
            _paper.a = 236u;
            _edge.a  = (u8)((u32)_ink.a * 45u / 100u);
            // The left band as wide as its widest number (one says its unit).
            f64 _reach = -1e300;
            const fude_zoom_sheet_label* _lt = (const fude_zoom_sheet_label*)_labels.memory;
            for(u32 _k = 0; _k < (u32)rde_arr_length(&_labels) && _r->font != NULL; _k++) {
                if(_lt[_k].in.x > 0.5) {
                    _reach = rde_math_max_f64(_reach, _lt[_k].at.x + (f64)fude_draw_text_width(_r->font, _r->font_px, _lt[_k].text, 11.0f) + 5.0);
                }
            }
            for(u32 _b = 0; _b < _nb; _b++) {
                fude_zoom_box _bx = _bands[_b];
                if(_bx.max_y - _bx.min_y > _bx.max_x - _bx.min_x) {
                    _bx.max_x = rde_math_max_f64(_bx.max_x, _reach);
                }
                rde_rendering_2d_draw_rectangle((rde_vec_2F){ (f32)((_bx.min_x + _bx.max_x) * 0.5), (f32)((_bx.min_y + _bx.max_y) * 0.5) },
                                                (rde_vec_2F){ (f32)(_bx.max_x - _bx.min_x), (f32)(_bx.max_y - _bx.min_y) }, _paper);
                // (its inner edge: a line where the band meets the drawing)
                const b8 _top = _bx.max_x - _bx.min_x > _bx.max_y - _bx.min_y;
                if(_r->stuck_count < 4u) {
                    _r->stuck[_r->stuck_count]       = _bx;   // (a guide pulled out of it: page_measure.c)
                    _r->stuck_sheet[_r->stuck_count] = _object;
                    _r->stuck_top[_r->stuck_count]   = _top ? 1u : 0u;
                    _r->stuck_count++;
                }
                rde_rendering_2d_draw_line_1(_top ? (rde_vec_2F){ (f32)_bx.min_x, (f32)_bx.min_y } : (rde_vec_2F){ (f32)_bx.max_x, (f32)_bx.min_y },
                                             _top ? (rde_vec_2F){ (f32)_bx.max_x, (f32)_bx.min_y } : (rde_vec_2F){ (f32)_bx.max_x, (f32)_bx.max_y }, _edge, 1.0f);
            }
            const fude_zoom_sheet_line* _l = (const fude_zoom_sheet_line*)_lines.memory;
            for(u32 _k = 0; _k < (u32)rde_arr_length(&_lines); _k++) {
                rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_l[_k].a.x, (f32)_l[_k].a.y }, (rde_vec_2F){ (f32)_l[_k].b.x, (f32)_l[_k].b.y }, _ink, 1.0f);
            }
            if(_r->font != NULL) {
                const f32 _px = 11.0f;
                const fude_zoom_sheet_label* _t = (const fude_zoom_sheet_label*)_labels.memory;
                for(u32 _k = 0; _k < (u32)rde_arr_length(&_labels); _k++) {
                    const f32 _w = fude_draw_text_width(_r->font, _r->font_px, _t[_k].text, _px);
                    const f64 _out = rde_math_abs_f64(_t[_k].in.x) * (f64)_w * 0.5 + rde_math_abs_f64(_t[_k].in.y) * (f64)_px * 0.5;
                    const fude_zoom_v2 _c = { _t[_k].at.x + _t[_k].in.x * _out, _t[_k].at.y + _t[_k].in.y * _out };
                    rde_rendering_2d_draw_text_2(_r->font, _t[_k].text, (rde_vec_3F){ (f32)_c.x - _w * 0.5f, (f32)_c.y - _px * 0.36f, 0.0f },
                                                 (rde_vec_2F){ _px / _r->font_px, _px / _r->font_px }, 0.0f, _ink);
                }
            }
        }
    }
    rde_arr_free(&_found);
    rde_arr_free(&_lines);
    rde_arr_free(&_labels);
}

// See the header.
rde_color fude_zoom_guide_color(void) {
    const rde_color _page = fude_theme_active()->page;
    const b8 _dark = (u32)_page.r * 3u + (u32)_page.g * 6u + (u32)_page.b < 1280u;
    return _dark ? (rde_color){ 64, 200, 255, 220 } : (rde_color){ 0, 150, 220, 220 };
}

// A guide: a hairline of its own colour across the screen, as far as it reaches (cut to the screen in f64 first:
// its ends may be far off it).
RDE_INTERNAL void fude_zoom_render_guide(const fude_zoom_renderer* _r, const fude_zoom_object* _o, const f64* _n, u32 _count, fude_zoom_sim _to_screen,
                                         fude_zoom_v2 _half, const rde_color* _as, f32 _extra) {
    if(_r->no_guides && _as == NULL) {
        return;
    }
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    b8      _closed;
    fude_zoom_shape_outline(FUDE_ZOOM_SHAPE_GUIDE, _n, _count, 1u, &_pts, &_closed);
    if(rde_arr_length(&_pts) == 2u) {
        const fude_zoom_sim _all = fude_zoom_sim_compose(_to_screen, fude_zoom_object_sim(_o));
        fude_zoom_v2 _a = fude_zoom_sim_apply(_all, ((const fude_zoom_v2*)_pts.memory)[0]), _b = fude_zoom_sim_apply(_all, ((const fude_zoom_v2*)_pts.memory)[1]);
        if(fude_zoom_clip_line(&_a, &_b, (fude_zoom_box){ -_half.x - 4.0, -_half.y - 4.0, _half.x + 4.0, _half.y + 4.0 })) {
            const rde_color _color = _as != NULL ? *_as : fude_zoom_guide_color();
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, _color, 1.0f + 2.0f * _extra);
        }
    }
    rde_arr_free(&_pts);
}

// A shape's outline being drawn (fude_zoom_render_shape): what its steps share, each under the name it had there.
typedef struct {
    fude_zoom_renderer*     r;
    const fude_zoom_object* o;
    fude_zoom_v2*           p;        // its outline on the screen: k points, then its first again
    u32                     k;
    u32                     m;        // the points its line goes through (closed: k + 1)
    b8                      closed;
    b8                      far;      // reaching FUDE_ZOOM_RENDER_FAR: cut to the screen on the CPU
    fude_zoom_v2            guard;    // the box round the screen what is far is cut to (its half sizes)
    fude_zoom_sim           all;      // its own units to the screen
    rde_color               color;    // its line's colour
    f64                     radius;   // its line's half width on the screen
    b8                      filled;   // filled...
    rde_color               fill;     // ...in this colour
    b8                      under;    // its fill under its line (a colour of its own)
    const rde_color*        as;       // all in this colour (a glow), or NULL
    fude_zoom_geo*          geo;      // its outline in its own units, kept, with its triangles once a fill asks
} fude_zoom_render_outline;

// Its inside in _color: its kept triangles (made from its outline in its own units the first time a fill asks) through
// its place; when it is far bigger than the screen, its outline on the screen cut to it and triangulated there, as
// seen once (no corner is then ever far out).
RDE_INTERNAL void fude_zoom_render_shape_inside(const fude_zoom_render_outline* _sh, rde_color _color) {
    if(_sh->far) {
        fude_zoom_render_polygon_fill(_sh->r, _sh->p, _sh->k, _sh->guard, _color);
        return;
    }
    const u32 _t = fude_zoom_render_geo_tris(_sh->r, _sh->geo);
    fude_zoom_render_triangles_through((const fude_zoom_v2*)_sh->geo->tris.memory, _t, _sh->all, _color);
}

// Its line: its own fill under it first when it has a colour of its own (so the line stays whole over it), then
// its line, dashed or dash and dot as its style says, cut to the screen when it is far, else one stroke.
RDE_INTERNAL void fude_zoom_render_shape_line(const fude_zoom_render_outline* _sh) {
    fude_zoom_renderer* _r = _sh->r;
    const fude_zoom_object* _o = _sh->o;
    fude_zoom_v2* _p = _sh->p;
    const u32 _k = _sh->k;
    const u32 _m = _sh->m;
    const b8 _closed = _sh->closed;
    const b8 _far = _sh->far;
    const fude_zoom_v2 _guard = _sh->guard;
    const rde_color _color = _sh->color;
    const f64 _radius = _sh->radius;
    const rde_color _fill = _sh->fill;
    const b8 _under = _sh->under;
    if(_under) {
        fude_zoom_render_shape_inside(_sh, _fill);
    }
    const u8 _style = fude_zoom_line_style_of(_o->q);
    if(_style > FUDE_ZOOM_LINE_SOLID && _style < FUDE_ZOOM_LINE_STYLES) {
        // Dashed (hidden) or dash and dot (a centre line): its pieces on the screen, each a line of its width.
        rde_arr_clear(&_r->fill);
        const u32 _pieces = fude_zoom_line_dashes(_p, _k, _closed, _style, _radius * 2.0, (fude_zoom_box){ -_guard.x, -_guard.y, _guard.x, _guard.y }, &_r->fill);
        const fude_zoom_v2* _d = (const fude_zoom_v2*)_r->fill.memory;
        for(u32 _i = 0; _i < _pieces; _i++) {
            rde_vec_2F _seg[2] = { { (f32)_d[2u * _i].x, (f32)_d[2u * _i].y }, { (f32)_d[2u * _i + 1u].x, (f32)_d[2u * _i + 1u].y } };
            const f32  _rr[2]  = { (f32)_radius, (f32)_radius };
            rde_rendering_2d_draw_stroke(_seg, _rr, 2u, _color);
        }
    } else if(_far) {
        rde_arr_clear(&_r->shape_radii);
        f64* _rad = (f64*)rde_arr_add_n(&_r->shape_radii, _m);
        for(u32 _i = 0; _i < _m; _i++) {
            _rad[_i] = _radius;
        }
        fude_zoom_draw_huge_stroke(_p, _rad, _m, _guard, _color);
    } else {
        rde_arr_clear(&_r->screen);
        rde_arr_clear(&_r->radii);
        rde_vec_2F* _pts = rde_arr_add_n(&_r->screen, _m);
        f32*        _rr  = rde_arr_add_n(&_r->radii, _m);
        for(u32 _i = 0; _i < _m; _i++) {
            _pts[_i] = (rde_vec_2F){ (f32)_p[_i].x, (f32)_p[_i].y };
            _rr[_i]  = (f32)_radius;
        }
        rde_rendering_2d_draw_stroke(_pts, _rr, _m, _color);
    }
}

// Its fill in its line's colour, after the line (when it has none of its own): a polygon (it may be concave), or one
// far bigger than the screen, as its triangles (fude_zoom_render_shape_inside); else drawn as it is.
RDE_INTERNAL void fude_zoom_render_shape_fill(const fude_zoom_render_outline* _sh) {
    fude_zoom_renderer* _r = _sh->r;
    const fude_zoom_object* _o = _sh->o;
    fude_zoom_v2* _p = _sh->p;
    const u32 _k = _sh->k;
    const b8 _far = _sh->far;
    const rde_color _color = _sh->color;
    const b8 _filled = _sh->filled;
    const b8 _under = _sh->under;
    // The fill after the line, to its middle: the line's soft inner edge is under
    // it (over a fill of its own colour that edge would show as a light seam: 2D
    // blending adds a soft edge's colour to what is under it).
    if(_filled && !_under) {
        if(_far || _o->channels == FUDE_ZOOM_SHAPE_POLYGON) {
            fude_zoom_render_shape_inside(_sh, _color);   // a polygon may be concave
        } else {
            rde_arr_clear(&_r->screen);
            rde_vec_2F* _f = (rde_vec_2F*)rde_arr_add_n(&_r->screen, _k);
            for(u32 _i = 0; _i < _k; _i++) {
                _f[_i] = (rde_vec_2F){ (f32)_p[_i].x, (f32)_p[_i].y };
            }
            rde_rendering_2d_draw_polygon(_f, _k, _color, NULL);
        }
    }
}

// Its hatching (closed, not a glow): its lines across its inside, or a tint of them when they would be too close
// on the screen to tell apart.
RDE_INTERNAL void fude_zoom_render_shape_hatch(const fude_zoom_render_outline* _sh) {
    fude_zoom_renderer* _r = _sh->r;
    const fude_zoom_object* _o = _sh->o;
    fude_zoom_v2* _p = _sh->p;
    const u32 _k = _sh->k;
    const b8 _closed = _sh->closed;
    const fude_zoom_v2 _guard = _sh->guard;
    const fude_zoom_sim _all = _sh->all;
    const rde_color _color = _sh->color;
    const rde_color* _as = _sh->as;
    // Hatched: its lines across its inside, half its line's width, 12 of its widths apart (too close on the screen to
    // tell apart: a tint of them).
    const u8 _hatch = fude_zoom_hatch_of(_o->q);
    if(_closed && _hatch != FUDE_ZOOM_HATCH_NONE && _as == NULL) {
        const f64 _w_px  = (f64)_o->radius * fude_zoom_sim_scale(_all) * 2.0;
        const f64 _apart = FUDE_ZOOM_HATCH_APART * _w_px;
        if(_apart >= 3.0) {
            rde_arr_clear(&_r->fill);
            const fude_zoom_box _view = { -_guard.x, -_guard.y, _guard.x, _guard.y };
            fude_zoom_hatch_lines(_p, _k, RDE_PI * 0.25, _apart, _view, &_r->fill);
            if(_hatch == FUDE_ZOOM_HATCH_CROSS) {
                fude_zoom_hatch_lines(_p, _k, 2.356194490192345, _apart, _view, &_r->fill);
            }
            const fude_zoom_v2* _h = (const fude_zoom_v2*)_r->fill.memory;
            const f32 _hr = (f32)rde_math_max_f64(_w_px * 0.25, 0.5);
            for(u32 _i = 0; _i + 1u < (u32)rde_arr_length(&_r->fill); _i += 2u) {
                rde_vec_2F _seg[2] = { { (f32)_h[_i].x, (f32)_h[_i].y }, { (f32)_h[_i + 1u].x, (f32)_h[_i + 1u].y } };
                const f32  _rr[2]  = { _hr, _hr };
                rde_rendering_2d_draw_stroke(_seg, _rr, 2u, _color);
            }
        } else {
            rde_color _tint = _color;
            _tint.a = (u8)((u32)_color.a / 4u);
            fude_zoom_render_shape_inside(_sh, _tint);
        }
    }
}

// A shape (shape.h): its outline from its numbers, cut as finely as its size on
// screen asks; its line, then filled when it says so. _as and _extra as for
// strokes (a glow: the line only). Dimensions, arrows, sizes, angles, guides, cut
// boards and symbols are drawn by their own functions; attributes not at all.
// Its own steps are the fude_zoom_render_shape_* above, each with its comment.
void fude_zoom_render_shape(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half, const rde_color* _as, f32 _extra) {
    const fude_zoom_object* _o     = fude_zoom_scene_object(_s, _object);
    const f64               _scale = fude_zoom_sim_scale(_to_screen);
    const f64               _size  = rde_math_max_f64(_o->box.max_x - _o->box.min_x, _o->box.max_y - _o->box.min_y) * _scale;
    if(_size < 0.35) {
        return;
    }
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _count = fude_zoom_scene_shape_numbers(_s, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    if(_count == 0) {
        return;
    }
    if(_o->channels == FUDE_ZOOM_SHAPE_DIMENSION) {
        fude_zoom_render_dimension_shape(_r, _s, _o, _n, _count, _to_screen, _as);
        return;
    }
    if(_o->channels == FUDE_ZOOM_SHAPE_ARROW) {
        fude_zoom_render_arrow_shape(_o, _n, _count, _to_screen, _as);
        return;
    }
    if(_o->channels == FUDE_ZOOM_SHAPE_RADIAL) {
        fude_zoom_render_radial(_r, _s, _o, _n, _count, _to_screen, _as);
        return;
    }
    if(_o->channels == FUDE_ZOOM_SHAPE_ANGLE) {
        fude_zoom_render_angle(_r, _o, _n, _count, _to_screen, _as);
        return;
    }
    if(fude_zoom_shape_is_attribute(_o->channels)) {
        return;   // (an attribute of something else: nothing of its own drawn)
    }
    if(_o->channels == FUDE_ZOOM_SHAPE_GUIDE) {
        fude_zoom_render_guide(_r, _o, _n, _count, _to_screen, _half, _as, _extra);
        return;
    }
    if(_o->channels == FUDE_ZOOM_SHAPE_BOARD && fude_zoom_board_is_cut(_n, _count)) {
        fude_zoom_render_cut_board(_r, _s, _object, _to_screen, _half, _as, _extra);
        return;
    }
    if(_o->channels == FUDE_ZOOM_SHAPE_SYMBOL) {
        _r->drawing = _object;
        fude_zoom_render_symbol(_r, _o, _n, _count, _to_screen, _half, _as, _extra);
        _r->drawing = FUDE_ZOOM_NONE;
        return;
    }
    // Its outline in its own units, at the detail its size on the screen asks for: kept (fude_zoom_render_shape_geo).
    fude_zoom_geo* _geo    = fude_zoom_render_shape_geo(_r, _s, _object, fude_zoom_shape_segments(_size), FUDE_ZOOM_SHAPE_NUMBERS);
    const b8       _closed = _geo->closed;
    const u32      _k      = (u32)rde_arr_length(&_geo->points);
    if(_k < 2u) {
        return;
    }
    const fude_zoom_sim _all    = fude_zoom_sim_compose(_to_screen, fude_zoom_object_sim(_o));
    const rde_color     _color  = _as != NULL ? *_as : fude_theme_resolve(_o->color);
    f64 _radius = rde_math_max_f64((f64)_o->radius * fude_zoom_sim_scale(_all), (f64)FUDE_ZOOM_RENDER_MIN_RADIUS);
    if(_o->channels == FUDE_ZOOM_SHAPE_SHEET) {
        _radius = rde_math_min_f64(_radius, 1.25);   // (a sheet's edge a line on the screen at any zoom, as its rulers are: never over their numbers)
    }
    _radius += (f64)_extra;
    // (its outline on the screen, closed by its first point again: any number of points — a fixed buffer here once
    // left a shape of more than 799 points undrawn)
    rde_arr_clear(&_r->shape_screen);
    fude_zoom_v2* _p = (fude_zoom_v2*)rde_arr_add_n(&_r->shape_screen, _k + 1u);
    b8 _far = _radius >= FUDE_ZOOM_RENDER_FAR;
    for(u32 _i = 0; _i < _k; _i++) {
        _p[_i] = fude_zoom_sim_apply(_all, ((const fude_zoom_v2*)_geo->points.memory)[_i]);
        _far = _far || rde_math_abs_f64(_p[_i].x) >= FUDE_ZOOM_RENDER_FAR || rde_math_abs_f64(_p[_i].y) >= FUDE_ZOOM_RENDER_FAR;
    }
    const fude_zoom_v2 _guard = { _half.x * 2.0, _half.y * 2.0 };
    const u32 _m = _closed ? _k + 1u : _k;
    _p[_k] = _p[0];
    // Its fill: its own colour (the Fill tool's) under the line, so the line
    // stays whole over it; its line's colour after it (below).
    const b8        _filled = _closed && (_o->flags & FUDE_ZOOM_FLAG_FILLED) && _as == NULL;
    const rde_color _fill   = (_o->flags & FUDE_ZOOM_FLAG_FILL_OWN) ? fude_theme_resolve(_o->fill) : _color;
    const b8        _under  = _filled && rde_memcmp(&_fill, &_color, sizeof(rde_color)) != 0;
    const fude_zoom_render_outline _sh = { _r, _o, _p, _k, _m, _closed, _far, _guard, _all, _color, _radius, _filled, _fill, _under, _as, _geo };
    fude_zoom_render_shape_line(&_sh);
    fude_zoom_render_shape_fill(&_sh);
    fude_zoom_render_shape_hatch(&_sh);
    if(_o->channels == FUDE_ZOOM_SHAPE_BOARD && _as == NULL) {
        fude_zoom_render_board_marks(_r, _s, _o, _n, _count, _all, fude_zoom_render_marks_on(_filled, _fill, _color));
    }
    if(_o->channels == FUDE_ZOOM_SHAPE_SHEET && _as == NULL) {
        fude_zoom_render_sheet_marks(_r, _s, _o, _n, _count, _all, _half, fude_zoom_render_marks_on(_filled, _fill, _color));
    }
    _r->strokes_drawn++;
    _r->points_drawn += _m;
}
