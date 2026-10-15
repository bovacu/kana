// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/render_parts.h"

// ===========================================================================
// See render.h. The renderer, symbols (symbol.h): a symbol's geometry made once for its kind, size and detail and kept
// (its parts, and each filled part's triangles), then drawn (fude_zoom_render_symbol): its parts filled and lined, a
// graph's or chart's lines from plot.h (kept, or made each frame while it plays) with its numbers and legend, a part's
// pins' names and its face's words, and its text in its boxes.
//
// The file goes: a polyline dashed, the geometry kept, then fude_zoom_render_symbol's context and steps
// (fude_zoom_render_symbol_*, in the order it takes them) and fude_zoom_render_symbol.
//
// Most drawing functions take _as and _extra: _as, when not NULL, draws the whole object in that colour (the
// selection's glow, or a faint thing), _extra points wider on each side.
//
// Part of the renderer: render_parts.h holds what the modules share and says which module holds what. Internal names
// start fude_zoom_render_ (fude_zoom_draw_ and fude_zoom_clip_ for the clipping). Main thread only, inside the app's 2D
// drawing block, as render.h says.
// ===========================================================================

// A polyline on the screen dashed: _on long, _off apart.
RDE_INTERNAL void fude_zoom_render_dashed(const fude_zoom_v2* _p, u32 _n, b8 _closed, f32 _radius, f64 _on, f64 _off, rde_color _color) {
    const u32 _m = _closed ? _n + 1u : _n;
    f64 _left = _on;
    b8  _draw = true;
    for(u32 _i = 1; _i < _m; _i++) {
        fude_zoom_v2 _a = _p[_i - 1u];
        const fude_zoom_v2 _b = _p[_i % _n];
        f64 _seg = rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y);
        while(_seg > 1e-9) {
            const f64 _t = rde_math_min_f64(_left, _seg);
            const fude_zoom_v2 _c = { _a.x + (_b.x - _a.x) * _t / _seg, _a.y + (_b.y - _a.y) * _t / _seg };
            if(_draw) {
                rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_c.x, (f32)_c.y }, _color, (f32)rde_math_max_f64(_radius * 2.0f, 1.0f));
            }
            _seg  -= _t;
            _left -= _t;
            _a     = _c;
            if(_left <= 1e-9) {
                _draw = !_draw;
                _left = _draw ? _on : _off;
            }
        }
    }
}

// Symbol _kind's geometry at half sizes _hw × _hh and _segments (a step of 8: what zooming changes little, kept), or
// custom part _custom's: its parts' points (its own units), its parts, each filled part's triangles. Kept by all of
// these (render_parts.h's fude_zoom_render_geo_key) in the renderer's kept geometry (render.c), or made now.
RDE_INTERNAL const fude_zoom_geo* fude_zoom_render_symbol_made(fude_zoom_renderer* _r, u32 _kind, f64 _hw, f64 _hh, u32 _segments,
                                                               const fude_zoom_part* _custom) {
    _segments = (_segments + 7u) / 8u * 8u;
    fude_zoom_render_geo_key _key;
    rde_memset(&_key, 0, sizeof(_key));
    _key.what     = FUDE_ZOOM_RENDER_GEO_SYMBOL;
    _key.kind     = _kind;
    _key.segments = _segments;
    _key.hw       = _hw;
    _key.hh       = _hh;
    _key.custom   = (u64)(uintptr_t)_custom;
    const fude_zoom_geo* _kept = fude_zoom_render_geo_find(_r, &_key);
    if(_kept != NULL) {
        return _kept;
    }
    fude_zoom_geo* _g = fude_zoom_render_geo_begin(_r);
    fude_zoom_render_fresh(&_g->points, sizeof(fude_zoom_v2));
    fude_zoom_render_fresh(&_g->parts, sizeof(fude_zoom_symbol_part));
    fude_zoom_render_fresh(&_g->tris, sizeof(fude_zoom_v2));
    fude_zoom_render_fresh(&_g->tri_range, sizeof(u32));
    _g->parts_n = _custom != NULL ? fude_zoom_part_draw(_custom, _hw, _hh, _segments, &_g->points, &_g->parts)
                                  : fude_zoom_symbol_parts(_kind, _hw, _hh, _segments, &_g->points, &_g->parts);
    // Each part that can be filled: its rings (itself, then the parts joined to it — its holes) as triangles.
    const fude_zoom_v2* _p = (const fude_zoom_v2*)_g->points.memory;
    const fude_zoom_symbol_part* _pa = (const fude_zoom_symbol_part*)_g->parts.memory;
    rde_arr_clear(&_r->ring_ids);
    rde_arr_clear(&_r->shape);
    for(u32 _i = 0; _i < _g->parts_n; _i++) {
        const u8 _fl = _pa[_i].flags;
        u32 _range[2] = { (u32)(rde_arr_length(&_g->tris) / 3u), 0u };
        if((_fl & (FUDE_ZOOM_SYMBOL_SOLID | FUDE_ZOOM_SYMBOL_TINTED | FUDE_ZOOM_SYMBOL_FILLED)) && _pa[_i].count >= 3u && !(_fl & FUDE_ZOOM_SYMBOL_JOIN)) {
            rde_arr_clear(&_r->ring_ids);
            rde_arr_clear(&_r->shape);
            u32 _ring = 0;
            for(u32 _j = _i; _j < _g->parts_n && (_j == _i || (_pa[_j].flags & FUDE_ZOOM_SYMBOL_JOIN)); _j++, _ring++) {
                for(u32 _t = 0; _t < _pa[_j].count; _t++) {
                    rde_arr_add(&_r->ring_ids, (any)&_ring);
                    rde_arr_add(&_r->shape, (any)&_p[_pa[_j].first + _t]);
                }
            }
            rde_arr_clear(&_r->fill);
            _range[1] = fude_zoom_fill_triangulate_rings((const fude_zoom_v2*)_r->shape.memory, (const u32*)_r->ring_ids.memory, (u32)rde_arr_length(&_r->shape), &_r->fill);
            if(_range[1] > 0u) {
                rde_memcpy(rde_arr_add_n(&_g->tris, (usize)_range[1] * 3u), _r->fill.memory, (usize)_range[1] * 3u * sizeof(fude_zoom_v2));
            }
        }
        rde_memcpy(rde_arr_add_n(&_g->tri_range, 2u), _range, sizeof(_range));
    }
    _g->triangulated = true;
    return fude_zoom_render_geo_keep(_r, &_key);
}

// See the header.
const fude_zoom_geo* fude_zoom_render_symbol_geo(fude_zoom_renderer* _r, u32 _kind, f64 _hw, f64 _hh, u32 _segments) {
    return fude_zoom_render_symbol_made(_r, _kind, _hw, _hh, _segments, NULL);
}

// A symbol being drawn (fude_zoom_render_symbol): what its steps share, each under the name it had there.
typedef struct {
    fude_zoom_renderer*          r;
    const f64*                   n;        // its numbers (count of them): its kind, half sizes, letters' size, text
    u32                          count;
    const rde_color*             as;       // all in this colour (a glow), or NULL
    f32                          extra;    // points wider each side
    u32                          kind;
    const fude_zoom_symbol_info* info;
    fude_zoom_sim                all;      // its own units to the screen
    f64                          k;        // ...their scale
    rde_color                    color;    // its line's colour
    f64                          radius;   // its line's half width on the screen
    b8                           filled;   // filled as the shape is...
    rde_color                    fill;     // ...in this colour
    rde_memory_allocator*        heap;
    const fude_zoom_part*        custom;   // the circuit's part it is (made from its text when it is a custom one), or NULL
} fude_zoom_render_sym;

// Its parts: its geometry (made once for its kind, size and detail, a custom part as its definition's pins are)
// into the screen, what moves in it moved, then each part filled (as the shape is, solid, or tinted) and its line
// drawn (dashed where it is; points nearer than a pixel left out).
RDE_INTERNAL void fude_zoom_render_symbol_parts(const fude_zoom_render_sym* _sy) {
    fude_zoom_renderer* _r = _sy->r;
    const f64* _n = _sy->n;
    const rde_color* _as = _sy->as;
    const f32 _extra = _sy->extra;
    const u32 _kind = _sy->kind;
    const fude_zoom_sim _all = _sy->all;
    const f64 _k = _sy->k;
    const rde_color _color = _sy->color;
    const f64 _radius = _sy->radius;
    const b8 _filled = _sy->filled;
    const rde_color _fill = _sy->fill;
    const fude_zoom_part* _custom = _sy->custom;
    const fude_zoom_geo* _geo = fude_zoom_render_symbol_made(_r, _kind, _n[1], _n[2], fude_zoom_shape_segments(rde_math_max_f64(_n[1], _n[2]) * 2.0 * _k),
                                                             fude_zoom_part_made(_custom) ? _custom : NULL);
    const u32 _np = _geo->parts_n;
    rde_arr_clear(&_r->shape);
    const fude_zoom_v2* _local = (const fude_zoom_v2*)_geo->points.memory;
    fude_zoom_v2* _p = (fude_zoom_v2*)rde_arr_add_n(&_r->shape, rde_arr_length(&_geo->points));
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_geo->points); _i++) {
        _p[_i] = fude_zoom_sim_apply(_all, _local[_i]);
    }
    const fude_zoom_symbol_part* _pa = (const fude_zoom_symbol_part*)_geo->parts.memory;
    const u32* _range = (const u32*)_geo->tri_range.memory;
    const fude_zoom_v2* _tris = (const fude_zoom_v2*)_geo->tris.memory;
    // (what moves in it, as far along it as it has: on the screen, its own width's way that far)
    const f64 _along = _r->moved != NULL && _r->drawing < (u32)rde_arr_length(_r->moved) ? (f64)((const f32*)_r->moved->memory)[_r->drawing] : 0.0;
    const fude_zoom_v2 _shift = { _all.a * _along, _all.b * _along };
    for(u32 _i = 0; _i < _np; _i++) {
        const u8 _fl = _pa[_i].flags;
        if((_fl & FUDE_ZOOM_SYMBOL_MOVES) && _along != 0.0) {
            for(u32 _j = 0; _j < _pa[_i].count; _j++) {
                _p[_pa[_i].first + _j].x += _shift.x;
                _p[_pa[_i].first + _j].y += _shift.y;
            }
        }
    }
    for(u32 _i = 0; _i < _np; _i++) {
        const fude_zoom_v2* _q = &_p[_pa[_i].first];
        const u32           _m = _pa[_i].count;
        const u8            _fl = _pa[_i].flags;
        const fude_zoom_v2  _by = (_fl & FUDE_ZOOM_SYMBOL_MOVES) ? _shift : (fude_zoom_v2){ 0.0, 0.0 };
        const b8 _solid = (_fl & FUDE_ZOOM_SYMBOL_SOLID) != 0u, _tinted = (_fl & FUDE_ZOOM_SYMBOL_TINTED) != 0u;
        if((_solid || _tinted || ((_fl & FUDE_ZOOM_SYMBOL_FILLED) && _filled)) && _range[2u * _i + 1u] > 0u) {
            rde_color _fc = _solid ? _color : (_tinted ? _pa[_i].fill : _fill);
            if(_as != NULL && _tinted) {
                _fc = *_as;   // (a ghost: all in its colour)
            }
            // (its triangles, made with it, into the screen)
            const fude_zoom_v2* _t = &_tris[3u * _range[2u * _i]];
            for(u32 _j = 0; _j < _range[2u * _i + 1u]; _j++) {
                const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, _t[3u * _j]), _b = fude_zoom_sim_apply(_all, _t[3u * _j + 1u]), _c = fude_zoom_sim_apply(_all, _t[3u * _j + 2u]);
                rde_rendering_2d_draw_triangle((rde_vec_2F){ (f32)(_a.x + _by.x), (f32)(_a.y + _by.y) }, (rde_vec_2F){ (f32)(_b.x + _by.x), (f32)(_b.y + _by.y) },
                                               (rde_vec_2F){ (f32)(_c.x + _by.x), (f32)(_c.y + _by.y) }, _fc, NULL);
            }
        }
        if(_fl & FUDE_ZOOM_SYMBOL_NO_LINE) {
            continue;
        }
        const rde_color _lc = _pa[_i].line.a > 0u && _as == NULL ? _pa[_i].line : _color;
        const f64 _lr = _pa[_i].width > 0.0f ? rde_math_max_f64((f64)_pa[_i].width * 0.5 * _k, (f64)FUDE_ZOOM_RENDER_MIN_RADIUS) + (f64)_extra : _radius;
        const b8 _closed = (_fl & FUDE_ZOOM_SYMBOL_CLOSED) != 0u;
        if(_fl & FUDE_ZOOM_SYMBOL_DASHED) {
            fude_zoom_render_dashed(_q, _m, _closed, (f32)_lr, rde_math_max_f64(_lr * 4.0, 6.0), rde_math_max_f64(_lr * 3.0, 4.0), _lc);
            continue;
        }
        const u32 _w = _closed ? _m + 1u : _m;
        if(_w == 0u) {
            continue;   // (no points: nothing to draw)
        }
        // (its points on the screen, those nearer than a pixel to the one before left out: a gear's teeth far out are
        // its rim, not hundreds of points in a few pixels; its last kept, however near)
        rde_arr_clear(&_r->screen);
        rde_arr_clear(&_r->radii);
        rde_vec_2F* _sp = rde_arr_add_n(&_r->screen, _w);
        f32*        _sr = rde_arr_add_n(&_r->radii, _w);
        u32 _kept = 0;
        for(u32 _j = 0; _j < _w; _j++) {
            const rde_vec_2F _v = { (f32)_q[_j % _m].x, (f32)_q[_j % _m].y };
            if(_kept > 0u && _j + 1u < _w && rde_math_abs_f32(_v.x - _sp[_kept - 1u].x) < 1.0f && rde_math_abs_f32(_v.y - _sp[_kept - 1u].y) < 1.0f) {
                continue;
            }
            _sp[_kept]   = _v;
            _sr[_kept++] = (f32)_lr;
        }
        if(_kept >= 2u) {
            rde_rendering_2d_draw_stroke(_sp, _sr, _kept, _lc);
        }
    }
}

// A graph's or a chart's lines (_ppp, its points; _pap, its parts): its grid faint, its curves each its colour
// (_curve), a region or an area under a curve faint and filled, a chart's bars or slices solid, a strict
// inequality's edge dashed.
RDE_INTERNAL void fude_zoom_render_symbol_plot_lines(const fude_zoom_render_sym* _sy, const rde_arr* _ppp, const rde_arr* _pap, const rde_color* _curve) {
    fude_zoom_renderer* _r = _sy->r;
    const fude_zoom_sim _all = _sy->all;
    const rde_color _color = _sy->color;
    const f64 _radius = _sy->radius;
    rde_color _grid = _color;
    _grid.a = (u8)((u32)_color.a * 30u / 100u);
    const fude_zoom_v2* _q = (const fude_zoom_v2*)_ppp->memory;
    const fude_zoom_symbol_part* _qp = (const fude_zoom_symbol_part*)_pap->memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(_pap); _i++) {
        const u8 _fl = _qp[_i].flags;
        if(_fl >= 64u) {
            // (a region an inequality holds, an area under a curve: its colour, faint, filled; a chart's bar or
            // slice, 128 + k: solid)
            rde_color _c = _fl >= 128u ? _curve[(_fl - 128u) % 6u] : _curve[(_fl - 64u) % 6u];
            _c.a = _fl >= 128u ? 255u : 56u;
            rde_arr_clear(&_r->fill);
            for(u32 _j = 0; _j < _qp[_i].count; _j++) {
                const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, _q[_qp[_i].first + _j]);
                rde_arr_add(&_r->fill, (any)&_a);
            }
            rde_arr_clear(&_r->cut);
            const u32 _nt = fude_zoom_fill_triangulate((const fude_zoom_v2*)_r->fill.memory, (u32)rde_arr_length(&_r->fill), &_r->cut);
            const fude_zoom_v2* _t3 = (const fude_zoom_v2*)_r->cut.memory;
            for(u32 _j = 0; _j < _nt; _j++) {
                rde_rendering_2d_draw_triangle((rde_vec_2F){ (f32)_t3[3u * _j].x, (f32)_t3[3u * _j].y }, (rde_vec_2F){ (f32)_t3[3u * _j + 1u].x, (f32)_t3[3u * _j + 1u].y },
                                               (rde_vec_2F){ (f32)_t3[3u * _j + 2u].x, (f32)_t3[3u * _j + 2u].y }, _c, NULL);
            }
            continue;
        }
        const b8 _curved = _fl >= 32u;
        const b8 _dashes = _curved && ((_fl - 32u) & 8u) != 0u;   // (a strict inequality's edge: not in it)
        const rde_color _c = _curved ? _curve[((_fl - 32u) & 7u) % 6u] : ((_fl & FUDE_ZOOM_SYMBOL_DASHED) ? _grid : _color);
        const f32 _w = _curved ? (f32)rde_math_max_f64(_radius * 2.6, 2.0) : (f32)rde_math_max_f64(_radius * ((_fl & FUDE_ZOOM_SYMBOL_DASHED) ? 1.0 : 1.6), 1.0);
        f32 _along = 0.0f;   // (dashes: 3 widths on, 2 off — at least 8 and 5 points)
        const f32 _on = rde_math_max_f32(8.0f, _w * 3.0f), _period = _on + rde_math_max_f32(5.0f, _w * 2.0f);
        for(u32 _j = 0; _j + 1u < _qp[_i].count; _j++) {
            const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, _q[_qp[_i].first + _j]), _b = fude_zoom_sim_apply(_all, _q[_qp[_i].first + _j + 1u]);
            if(!_dashes) {
                rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, _c, _w);
                continue;
            }
            const f32 _len = (f32)rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y);
            for(f32 _s = 0.0f; _s < _len;) {
                const f32 _in = rde_math_fmod_f32(_along + _s, _period);
                const f32 _step = rde_math_min_f32(_len - _s, _in < _on ? _on - _in : _period - _in);
                if(_in < _on && _len > 0.0f) {
                    const f32 _t0 = _s / _len, _t1 = (_s + _step) / _len;
                    rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)(_a.x + (_b.x - _a.x) * _t0), (f32)(_a.y + (_b.y - _a.y) * _t0) },
                                                 (rde_vec_2F){ (f32)(_a.x + (_b.x - _a.x) * _t1), (f32)(_a.y + (_b.y - _a.y) * _t1) }, _c, _w);
                }
                _s += rde_math_max_f32(_step, 1e-3f);
            }
            _along += _len;
        }
    }
}

// A graph's or a chart's words (_plp, its labels): its ticks' numbers and its points' first, then its legend over
// them on its paper, each line set as maths (formula.h) or as its words, in its curve's colour (_curve).
RDE_INTERNAL void fude_zoom_render_symbol_plot_labels(const fude_zoom_render_sym* _sy, const rde_arr* _plp, const rde_color* _curve) {
    fude_zoom_renderer* _r = _sy->r;
    const f64* _n = _sy->n;
    const fude_zoom_symbol_info* _info = _sy->info;
    const fude_zoom_sim _all = _sy->all;
    const f64 _k = _sy->k;
    const f32 _lpx = (f32)(fude_zoom_plot_label_height(_info->id, _n[1], _n[2]) * _k);
    if(_r->font != NULL && _lpx >= 5.0f) {
        const fude_zoom_plot_label* _lb = (const fude_zoom_plot_label*)_plp->memory;
        // The rest first (ticks' numbers, points'), then the legend over them on its paper: each line set as maths
        // in text style (its LaTeX: formula.h) or as its words, stacked down from where its first is, as tall as
        // each is.
        for(u32 _i = 0; _i < (u32)rde_arr_length(_plp); _i++) {
            if(_lb[_i].align == 4u) {
                continue;
            }
            const fude_zoom_v2 _at = fude_zoom_sim_apply(_all, _lb[_i].at);
            const f32 _tw = fude_draw_text_width(_r->font, _r->font_px, _lb[_i].text, _lpx);
            const f32 _x = _lb[_i].align == 1u ? (f32)_at.x - _tw : (_lb[_i].align >= 3u ? (f32)_at.x : (f32)_at.x - _tw * 0.5f);
            const f32 _y = _lb[_i].align == 0u ? (f32)_at.y - _lpx * 0.7f : (f32)_at.y - _lpx * 0.36f;
            rde_rendering_2d_draw_text_2(_r->font, _lb[_i].text, (rde_vec_3F){ _x, _y, 0.0f }, (rde_vec_2F){ _lpx / _r->font_px, _lpx / _r->font_px }, 0.0f,
                                         _lb[_i].curve > 0u ? _curve[(_lb[_i].curve - 1u) % 6u] : fude_theme_active()->text_soft);
        }
        const f32 _em = _lpx * 1.25f;
        const b8  _maths = _r->formulas != NULL && _r->formulas->ready;
        f32 _left = 0.0f, _top = 0.0f, _right = -1e30f, _y = 0.0f;
        u32 _rows = 0;
        for(u32 _pass = 0; _pass < 2u; _pass++) {
            // (pass 0 measures; pass 1 lays its paper down, then draws — the settings found again as kept)
            if(_pass == 1u && _rows > 0u) {
                rde_color _paper = fude_theme_active()->page;
                _paper.a = 220u;
                const f32 _l0 = _left - _lpx * 0.3f, _l1 = _right + _lpx * 0.3f, _b0 = _y + _lpx * 0.05f, _b1 = _top;
                rde_rendering_2d_draw_rectangle((rde_vec_2F){ (_l0 + _l1) * 0.5f, (_b0 + _b1) * 0.5f }, (rde_vec_2F){ _l1 - _l0, _b1 - _b0 }, _paper);
            }
            b8 _first = true;
            for(u32 _i = 0; _i < (u32)rde_arr_length(_plp); _i++) {
                if(_lb[_i].align != 4u) {
                    continue;
                }
                const fude_zoom_v2 _at = fude_zoom_sim_apply(_all, _lb[_i].at);
                if(_first) {
                    _left  = (f32)_at.x;
                    _top   = (f32)_at.y + _lpx * 0.7f;
                    _y     = _top - _lpx * 0.15f;
                    _first = false;
                }
                const fude_zoom_tex_box* _box = _maths && _lb[_i].latex[0] != 0 ?
                                                fude_zoom_formulas_set_as(_r->formulas, _lb[_i].latex, (u32)rde_strlen(_lb[_i].latex), false) : NULL;
                const f32 _up   = rde_math_max_f32(_lpx * 0.72f, _box != NULL ? (f32)_box->height * _em : 0.0f);
                const f32 _down = rde_math_max_f32(_lpx * 0.25f, _box != NULL ? (f32)_box->depth * _em : 0.0f);
                const f32 _base = _y - _up;
                const rde_color _c = _lb[_i].curve > 0u ? _curve[(_lb[_i].curve - 1u) % 6u] : fude_theme_active()->text_soft;
                if(_pass == 0u) {
                    _right = rde_math_max_f32(_right, _left + (_box != NULL ? (f32)_box->width * _em : fude_draw_text_width(_r->font, _r->font_px, _lb[_i].text, _lpx)));
                    _rows++;
                } else if(_box != NULL) {
                    fude_zoom_render_formula(_r, _box, (fude_zoom_sim){ (f64)_em, 0.0, (f64)_left, (f64)_base }, _c);
                } else {
                    rde_rendering_2d_draw_text_2(_r->font, _lb[_i].text, (rde_vec_3F){ _left, _base, 0.0f }, (rde_vec_2F){ _lpx / _r->font_px, _lpx / _r->font_px }, 0.0f, _c);
                }
                _y = _base - _down - _lpx * 0.3f;
            }
        }
    }
}

// Maths (plot.h): a graph's, axes', a number line's or a chart's lines and words from its text, made each frame
// while it plays (as its time and its sliders say), else as kept (fude_zoom_render_plot_made).
RDE_INTERNAL void fude_zoom_render_symbol_plot(const fude_zoom_render_sym* _sy) {
    fude_zoom_renderer* _r = _sy->r;
    const f64* _n = _sy->n;
    const u32 _count = _sy->count;
    const fude_zoom_symbol_info* _info = _sy->info;
    rde_memory_allocator* _heap = _sy->heap;
    c8 _ptext[FUDE_ZOOM_SYMBOL_TEXT];
    fude_zoom_symbol_text(_n, _count, _ptext, sizeof(_ptext));
    // (playing: made each frame as its time and its sliders say; standing still: as kept)
    const b8 _playing = _r->plot_play != NULL && _r->plot_mask != NULL && _r->drawing < (u32)rde_arr_length(_r->plot_mask) &&
                        ((const u8*)_r->plot_mask->memory)[_r->drawing] != 0u;
    rde_arr _made_pp = { 0 }, _made_pa = { 0 }, _made_pl = { 0 };
    const rde_arr *_ppp, *_pap, *_plp;
    if(_playing) {
        _made_pp = rde_arr_new(sizeof(fude_zoom_v2), _heap);
        _made_pa = rde_arr_new(sizeof(fude_zoom_symbol_part), _heap);
        _made_pl = rde_arr_new(sizeof(fude_zoom_plot_label), _heap);
        fude_zoom_plot_lines_play(_info->id, _ptext, _n[1], _n[2], _r->plot_play, &_made_pp, &_made_pa, &_made_pl);
        _ppp = &_made_pp; _pap = &_made_pa; _plp = &_made_pl;
    } else {
        const fude_zoom_plot_made* _made = fude_zoom_render_plot_made(_r, _info->id, _ptext, _n[1], _n[2]);
        _ppp = &_made->points; _pap = &_made->parts; _plp = &_made->labels;
    }
    static const rde_color _curve[6] = { { 70, 140, 255, 255 }, { 240, 80, 70, 255 }, { 60, 190, 110, 255 }, { 245, 160, 40, 255 }, { 170, 100, 240, 255 }, { 40, 190, 200, 255 } };
    fude_zoom_render_symbol_plot_lines(_sy, _ppp, _pap, _curve);
    fude_zoom_render_symbol_plot_labels(_sy, _plp, _curve);
    if(_playing) {
        rde_arr_free(&_made_pp);
        rde_arr_free(&_made_pa);
        rde_arr_free(&_made_pl);
    }
}

// A circuit's part's words (_part; NULL: none): its named pins' names by them, inside it (a chip's, a module's, a
// board's), and the words on its face by what they name (an oscilloscope's knobs'), smaller, upright.
RDE_INTERNAL void fude_zoom_render_symbol_names(const fude_zoom_render_sym* _sy, const fude_zoom_part* _part) {
    fude_zoom_renderer* _r = _sy->r;
    const f64* _n = _sy->n;
    const rde_color* _as = _sy->as;
    const fude_zoom_symbol_info* _info = _sy->info;
    const fude_zoom_sim _all = _sy->all;
    const f64 _k = _sy->k;
    f32 _inset = 0.0f, _inset_v = 0.8f;
    fude_zoom_part_insets(_part, &_inset, &_inset_v);
    if(_as == NULL && _inset > 0.0f && _r->font != NULL && _info->h > 0.0f) {
        const f64 _pitch = _n[2] * _k / (fude_zoom_part_room_h(_part, (f64)_info->h) * 0.5) * 20.0;   // (pins 20 points apart as it came)
        const f32 _px = (f32)rde_math_min_f64(_pitch * 0.55, 13.0);
        if(_px >= 6.0f) {
            const rde_color _soft = fude_theme_active()->text_soft;
            for(u32 _i = 0; _i < _part->pin_count; _i++) {
                const fude_zoom_pin _q = _part->pins[_i];
                if(_q.name == NULL || _q.name[0] == 0) {
                    continue;
                }
                f32 _w = fude_draw_text_width(_r->font, _r->font_px, _q.name, _px), _qpx = _px;
                if((_q.side == FUDE_ZOOM_PIN_UP || _q.side == FUDE_ZOOM_PIN_DOWN) && _w > 0.9f * (f32)_pitch) {
                    // (side by side along a top or bottom: each as narrow as the pins are apart — an LCD's VSS VDD V0)
                    _qpx *= 0.9f * (f32)_pitch / _w;
                    _w    = 0.9f * (f32)_pitch;
                }
                fude_zoom_v2 _at;
                f32 _x;
                if(_q.side == FUDE_ZOOM_PIN_LEFT || _q.side == FUDE_ZOOM_PIN_RIGHT) {
                    const f64 _u = _q.side == FUDE_ZOOM_PIN_LEFT ? -(f64)_inset : (f64)_inset;
                    _at = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _u * _n[1], (f64)_q.v * _n[2] });
                    // (left or right of its body's edge on the screen, the part as it is turned)
                    const fude_zoom_v2 _in = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, (f64)_q.v * _n[2] });
                    _x = _in.x >= _at.x ? (f32)_at.x + 3.0f : (f32)_at.x - 3.0f - _w;
                } else {
                    _at = fude_zoom_sim_apply(_all, (fude_zoom_v2){ (f64)_q.u * _n[1], (f64)(_q.side == FUDE_ZOOM_PIN_UP ? _inset_v : -_inset_v) * _n[2] });
                    _x = (f32)_at.x - _w * 0.5f;
                }
                rde_rendering_2d_draw_text_2(_r->font, _q.name, (rde_vec_3F){ _x, (f32)_at.y - _qpx * 0.36f, 0.0f }, (rde_vec_2F){ _qpx / _r->font_px, _qpx / _r->font_px }, 0.0f, _soft);
            }
        }
    }
    // Words on its face by what they name (an oscilloscope's knobs'), smaller than its pins' names, upright.
    fude_zoom_part_label _words[16];
    const u32 _nw = _as == NULL && _r->font != NULL && _info->h > 0.0f ? fude_zoom_part_labels(_part, _words, 16u) : 0u;
    if(_nw > 0u) {
        const f64 _pitch = _n[2] * _k / (fude_zoom_part_room_h(_part, (f64)_info->h) * 0.5) * 20.0;
        const f32 _px = (f32)rde_math_min_f64(_pitch * 0.4, 11.0);
        if(_px >= 5.0f) {
            for(u32 _i = 0; _i < _nw; _i++) {
                const fude_zoom_v2 _at = fude_zoom_sim_apply(_all, (fude_zoom_v2){ (f64)_words[_i].u * _n[1], (f64)_words[_i].v * _n[2] });
                const f32 _w = fude_draw_text_width(_r->font, _r->font_px, _words[_i].text, _px);
                rde_rendering_2d_draw_text_2(_r->font, _words[_i].text, (rde_vec_3F){ (f32)_at.x - _w * 0.5f, (f32)_at.y - _px * 0.36f, 0.0f },
                                             (rde_vec_2F){ _px / _r->font_px, _px / _r->font_px }, 0.0f, fude_theme_active()->text_soft);
            }
        }
    }
}

// Its text: its parts in its boxes (a class's compartments, the lines between them drawn here), each in its box
// (a circuit's part's value upright under it), read as a note when it has marks; _part: the circuit's part it is.
RDE_INTERNAL void fude_zoom_render_symbol_text(const fude_zoom_render_sym* _sy, const fude_zoom_part* _part) {
    fude_zoom_renderer* _r = _sy->r;
    const f64* _n = _sy->n;
    const u32 _count = _sy->count;
    const rde_color* _as = _sy->as;
    const u32 _kind = _sy->kind;
    const fude_zoom_symbol_info* _info = _sy->info;
    const fude_zoom_sim _all = _sy->all;
    const f64 _k = _sy->k;
    const rde_color _color = _sy->color;
    const f64 _radius = _sy->radius;
    // Its text: its parts in its boxes (a class's compartments, the lines between them drawn here).
    c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
    fude_zoom_symbol_text(_n, _count, _text, sizeof(_text));
    u32 _from[8], _len[8];
    const u32 _split = fude_zoom_symbol_text_split(_text, _from, _len, 8u);
    fude_zoom_box _boxes[8];
    const b8  _parts_text = _info->place == FUDE_ZOOM_SYMBOL_TEXT_PARTS || _info->place == FUDE_ZOOM_SYMBOL_TEXT_REGIONS;   // (a part each in its box)
    const u32 _nb = fude_zoom_symbol_text_boxes(_kind, _n[1], _n[2], _parts_text ? _split : 1u, _boxes, 8u);
    if(_info->place == FUDE_ZOOM_SYMBOL_TEXT_PARTS && _info->boxes != 4u) {
        for(u32 _i = 1; _i < _nb; _i++) {
            const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, (fude_zoom_v2){ -_n[1], _boxes[_i].max_y });
            const fude_zoom_v2 _b = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _n[1], _boxes[_i].max_y });
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, _color, (f32)rde_math_max_f64(_radius * 2.0, 1.0));
        }
    }
    if(_as == NULL && _text[0] != 0 && !fude_zoom_plot_is(_info->id)) {   // (a graph's: its legend, above)
        const rde_color _ink = _color;   // (its line's colour)
        const u32 _shown = _parts_text ? (_split < _nb ? _split : _nb) : 1u;
        for(u32 _i = 0; _i < _shown; _i++) {
            fude_zoom_box _b = fude_zoom_sim_box(_all, _boxes[_i]);
            if(_part != NULL && _info->place == FUDE_ZOOM_SYMBOL_TEXT_BELOW) {
                // (a circuit's part, however it is turned: its value upright under it, as wide as it needs)
                const fude_zoom_box _on = fude_zoom_sim_box(_all, (fude_zoom_box){ -_n[1], -_n[2], _n[1], _n[2] });
                const f64 _px = _n[3] * _k, _wide = rde_math_max_f64((_on.max_x - _on.min_x) * 2.5, _px * 12.0);
                _b = (fude_zoom_box){ (_on.min_x + _on.max_x) * 0.5 - _wide * 0.5, _on.min_y - _px * 1.8, (_on.min_x + _on.max_x) * 0.5 + _wide * 0.5, _on.min_y - 2.0 };
            }
            const u32 _l = _parts_text ? _len[_i] : (u32)rde_strlen(_text);
            const u32 _f = _parts_text ? _from[_i] : 0u;
            const b8  _top = (_info->place == FUDE_ZOOM_SYMBOL_TEXT_PARTS && _i > 0u) ||   // (a class's attributes from the top of theirs)
                             _info->place == FUDE_ZOOM_SYMBOL_TEXT_CORNER;                // (an area's name from its corner)
            // (a class's attributes and operations: from the left, as UML has them; a bracket's against it)
            const u8 _align = _top || _info->place == FUDE_ZOOM_SYMBOL_TEXT_OUT_RIGHT ? FUDE_ZOOM_ALIGN_LEFT :
                              _info->place == FUDE_ZOOM_SYMBOL_TEXT_OUT_LEFT ? FUDE_ZOOM_ALIGN_RIGHT : FUDE_ZOOM_ALIGN_CENTRE;
            // A label with marks (rich.h: a Kanban card's tasks, a link): read as a note, in its box's middle.
            const fude_zoom_rich* _rich = _info->place != FUDE_ZOOM_SYMBOL_TEXT_PARTS && _k > 0.0 ?
                fude_zoom_render_rich_aligned(_r, &_text[_f], _l, _n[3], (_b.max_x - _b.min_x) / _k, _align == FUDE_ZOOM_ALIGN_LEFT ? 0u : _align == FUDE_ZOOM_ALIGN_RIGHT ? 2u : 1u) : NULL;
            if(_rich != NULL) {
                const f64 _tall = (f64)_rich->h * _k;
                const f64 _down = _top ? 0.0 : rde_math_max_f64(((_b.max_y - _b.min_y) - _tall) * 0.5, 0.0);
                fude_zoom_render_rich_draw(_r, _rich, &_text[_f], _b.min_x, _b.max_y - _down, _k, _n[3], _ink);
                continue;
            }
            fude_zoom_render_text_aligned(_r, &_text[_f], _l, _b, _n[3] * _k, _top, _align, _ink);
        }
    }
}

// A symbol (symbol.h): its parts — filled as the shape is, or solid in its line's colour — and their lines
// (dashed where they are), its compartments' lines, and its text in its boxes. A graph's lines come from plot.h (kept,
// or made each frame while it plays), a part's pins' names and its face's words are written by them, and what moves in
// it is drawn as far along as render.h's moved says.
// Its own steps are the fude_zoom_render_symbol_* above, each with its comment.
void fude_zoom_render_symbol(fude_zoom_renderer* _r, const fude_zoom_object* _o, const f64* _n, u32 _count, fude_zoom_sim _to_screen,
                             fude_zoom_v2 _half, const rde_color* _as, f32 _extra) {
    RDE_UNUSED(_half);
    if(_count < 4u) {
        return;
    }
    const u32 _kind = fude_zoom_symbol_kind_of(_n, _count);
    const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of(_kind);
    if(_info == NULL) {
        return;
    }
    const fude_zoom_sim _all    = fude_zoom_sim_compose(_to_screen, fude_zoom_object_sim(_o));
    const f64           _k      = fude_zoom_sim_scale(_all);
    const rde_color     _color  = _as != NULL ? *_as : fude_theme_resolve(_o->color);
    const f64           _radius = rde_math_max_f64((f64)_o->radius * _k, (f64)FUDE_ZOOM_RENDER_MIN_RADIUS) + (f64)_extra;
    const b8            _filled = (_o->flags & FUDE_ZOOM_FLAG_FILLED) && _as == NULL;
    const rde_color     _fill   = (_o->flags & FUDE_ZOOM_FLAG_FILL_OWN) ? fude_theme_resolve(_o->fill) : _color;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    // Its geometry, made once for its kind, size and detail (a custom part: drawn as its definition's pins are; a sized
    // display, as big as its text says), into the screen.
    const fude_zoom_part* _custom = fude_zoom_part_of_numbers(_n, _count);
    const fude_zoom_render_sym _sy = { _r, _n, _count, _as, _extra, _kind, _info, _all, _k, _color, _radius, _filled, _fill, _heap, _custom };
    fude_zoom_render_symbol_parts(&_sy);
    // Maths (plot.h): a graph's, axes', a number line's lines from its text — its grid faint, its curves each its
    // colour — and its ticks' numbers.
    if(_as == NULL && fude_zoom_plot_is(_info->id)) {
        fude_zoom_render_symbol_plot(&_sy);
    }
    // A circuit's part with named pins (a chip, a module, a board): their names by them, inside it.
    const fude_zoom_part* _part = _custom;
    fude_zoom_render_symbol_names(&_sy, _part);
    fude_zoom_render_symbol_text(&_sy, _part);
}
