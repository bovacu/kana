// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/render_parts.h"
#include "zoom/cut.h"

// ===========================================================================
// See render.h. The renderer, strokes and fills: polygons clipped to the screen (a stroke far bigger than it, cut on
// the CPU), a stroke drawn (its points as small offsets from one f64 translation; only the runs near the screen when it
// is far bigger than it), and a fill drawn (its triangles made once and kept with its points; its edge made soft).
//
// The file goes: the clipping (fude_zoom_clip_convex, fude_zoom_draw_clipped, a huge round join, a huge stroke), the
// strokes, then the fills.
//
// Most drawing functions take _as and _extra: _as, when not NULL, draws the whole object in that colour (the
// selection's glow, or a faint thing), _extra points wider on each side.
//
// Part of the renderer: render_parts.h holds what the modules share and says which module holds what. Internal names
// start fude_zoom_render_ (fude_zoom_draw_ and fude_zoom_clip_ for the clipping). Main thread only, inside the app's 2D
// drawing block, as render.h says.
// ===========================================================================

#define FUDE_ZOOM_RENDER_ARC     32u      // segments of a huge round join's visible arc

// --- polygons clipped to the screen (strokes far bigger than it) -----------------------------------

#define FUDE_ZOOM_CLIP_MAX 800u   // the most points a polygon being clipped has

// _subject clipped by the convex polygon _clip (either winding): into _out (at most _max points), its count. One edge
// of _clip at a time (Sutherland–Hodgman), in two static buffers; a subject of more than FUDE_ZOOM_CLIP_MAX points
// gives 0.
RDE_INTERNAL u32 fude_zoom_clip_convex(const fude_zoom_v2* _subject, u32 _n, const fude_zoom_v2* _clip, u32 _m, fude_zoom_v2* _out, u32 _max) {
    static fude_zoom_v2 _a[FUDE_ZOOM_CLIP_MAX], _b[FUDE_ZOOM_CLIP_MAX];
    if(_n > FUDE_ZOOM_CLIP_MAX) {
        return 0;
    }
    rde_memcpy(_a, _subject, _n * sizeof(fude_zoom_v2));
    // Which side is inside: the clip polygon's winding (its signed area's sign, summed as cut.h sums it).
    const f64 _side = fude_zoom_cut_area(_clip, _m) >= 0.0 ? 1.0 : -1.0;
    u32 _count = _n;
    for(u32 _e = 0; _e < _m && _count > 0; _e++) {
        const fude_zoom_v2 _c0 = _clip[_e], _c1 = _clip[(_e + 1u) % _m];
        u32 _k = 0;
        for(u32 _i = 0; _i < _count && _k + 2u < FUDE_ZOOM_CLIP_MAX; _i++) {
            const fude_zoom_v2 _p = _a[_i], _q = _a[(_i + 1u) % _count];
            const f64 _dp = _side * ((_c1.x - _c0.x) * (_p.y - _c0.y) - (_c1.y - _c0.y) * (_p.x - _c0.x));
            const f64 _dq = _side * ((_c1.x - _c0.x) * (_q.y - _c0.y) - (_c1.y - _c0.y) * (_q.x - _c0.x));
            if(_dp >= 0.0) {
                _b[_k++] = _p;
            }
            if((_dp >= 0.0) != (_dq >= 0.0)) {
                const f64 _t = _dp / (_dp - _dq);
                _b[_k++] = (fude_zoom_v2){ _p.x + (_q.x - _p.x) * _t, _p.y + (_q.y - _p.y) * _t };
            }
        }
        rde_memcpy(_a, _b, _k * sizeof(fude_zoom_v2));
        _count = _k;
    }
    if(_count > _max) {
        _count = _max;
    }
    rde_memcpy(_out, _a, _count * sizeof(fude_zoom_v2));
    return _count;
}

// Fills what of the convex polygon _poly is on the guard box (±_guard, screen units): the box clipped by the polygon.
RDE_INTERNAL void fude_zoom_draw_clipped(const fude_zoom_v2* _poly, u32 _n, fude_zoom_v2 _guard, rde_color _color) {
    const fude_zoom_v2 _box[4] = { { -_guard.x, -_guard.y }, { _guard.x, -_guard.y }, { _guard.x, _guard.y }, { -_guard.x, _guard.y } };
    static fude_zoom_v2 _out[FUDE_ZOOM_CLIP_MAX];
    const u32 _k = fude_zoom_clip_convex(_box, 4u, _poly, _n, _out, FUDE_ZOOM_CLIP_MAX);
    if(_k < 3u) {
        return;
    }
    static rde_vec_2F _pts[FUDE_ZOOM_CLIP_MAX];
    for(u32 _i = 0; _i < _k; _i++) {
        _pts[_i] = (rde_vec_2F){ (f32)_out[_i].x, (f32)_out[_i].y };
    }
    rde_rendering_2d_draw_polygon(_pts, _k, _color, NULL);
}

// A round join far bigger than the screen: what of it is on the guard box.
RDE_INTERNAL void fude_zoom_draw_huge_disc(fude_zoom_v2 _c, f64 _r, fude_zoom_v2 _guard, rde_color _color) {
    const fude_zoom_v2 _corners[4] = { { -_guard.x, -_guard.y }, { _guard.x, -_guard.y }, { _guard.x, _guard.y }, { -_guard.x, _guard.y } };
    b8 _all_in = true;
    for(u32 _i = 0; _i < 4u; _i++) {
        if(rde_math_hypot_f64(_corners[_i].x - _c.x, _corners[_i].y - _c.y) >= _r) {
            _all_in = false;
        }
    }
    if(_all_in) {
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ (f32)(_guard.x * 2.0), (f32)(_guard.y * 2.0) }, _color);
        return;
    }
    // The nearest point of the box: past the radius, the disc misses it.
    const f64 _nx = _c.x < -_guard.x ? -_guard.x : (_c.x > _guard.x ? _guard.x : _c.x);
    const f64 _ny = _c.y < -_guard.y ? -_guard.y : (_c.y > _guard.y ? _guard.y : _c.y);
    if(rde_math_hypot_f64(_nx - _c.x, _ny - _c.y) >= _r) {
        return;
    }
    // The angles the box spans from the centre (outside it: under half a turn).
    const f64 _mid = rde_math_atan2_f64(-_c.y, -_c.x);
    f64 _lo = 0.0, _hi = 0.0;
    for(u32 _i = 0; _i < 4u; _i++) {
        f64 _a = rde_math_atan2_f64(_corners[_i].y - _c.y, _corners[_i].x - _c.x) - _mid;
        while(_a > RDE_PI) { _a -= 2.0 * RDE_PI; }
        while(_a < -RDE_PI) { _a += 2.0 * RDE_PI; }
        _lo = _a < _lo ? _a : _lo;
        _hi = _a > _hi ? _a : _hi;
    }
    fude_zoom_v2 _sector[FUDE_ZOOM_RENDER_ARC + 2u];
    _sector[0] = _c;
    for(u32 _k = 0; _k <= FUDE_ZOOM_RENDER_ARC; _k++) {
        const f64 _a = _mid + _lo + (_hi - _lo) * (f64)_k / (f64)FUDE_ZOOM_RENDER_ARC;
        _sector[_k + 1u] = (fude_zoom_v2){ _c.x + rde_math_cos_f64(_a) * _r, _c.y + rde_math_sin_f64(_a) * _r };
    }
    fude_zoom_draw_clipped(_sector, FUDE_ZOOM_RENDER_ARC + 2u, _guard, _color);
}

// A stroke many screens across: each segment's band and each join, cut to the box.
void fude_zoom_draw_huge_stroke(const fude_zoom_v2* _p, const f64* _rad, u32 _n, fude_zoom_v2 _guard, rde_color _color) {
    for(u32 _i = 0; _i < _n; _i++) {
        if(_rad[_i] >= FUDE_ZOOM_RENDER_FAR || rde_math_abs_f64(_p[_i].x) >= FUDE_ZOOM_RENDER_FAR || rde_math_abs_f64(_p[_i].y) >= FUDE_ZOOM_RENDER_FAR) {
            fude_zoom_draw_huge_disc(_p[_i], _rad[_i], _guard, _color);
        } else {
            const f32 _res = (f32)(_rad[_i] / 2.0);
            rde_rendering_2d_draw_circle((rde_vec_2F){ (f32)_p[_i].x, (f32)_p[_i].y }, (f32)_rad[_i], (u32)(_res < 12.0f ? 12.0f : (_res > 96.0f ? 96.0f : _res)), _color, NULL);
        }
        if(_i + 1u == _n) {
            break;
        }
        const fude_zoom_v2 _a = _p[_i], _b = _p[_i + 1u];
        const f64 _len = rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y);
        if(_len <= 0.0) {
            continue;
        }
        const f64 _nx = -(_b.y - _a.y) / _len, _ny = (_b.x - _a.x) / _len;
        const fude_zoom_v2 _band[4] = {
            { _a.x + _nx * _rad[_i], _a.y + _ny * _rad[_i] }, { _b.x + _nx * _rad[_i + 1u], _b.y + _ny * _rad[_i + 1u] },
            { _b.x - _nx * _rad[_i + 1u], _b.y - _ny * _rad[_i + 1u] }, { _a.x - _nx * _rad[_i], _a.y - _ny * _rad[_i] },
        };
        fude_zoom_draw_clipped(_band, 4u, _guard, _color);
    }
}

// --- strokes ---------------------------------------------------------------------------------

// Draws a stroke in its own colour.
void fude_zoom_render_stroke(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half) {
    fude_zoom_render_stroke_as(_r, _s, _object, _to_screen, _half, NULL, 0.0f);
}

// Draws stroke _object through _to_screen; _as: drawn all in that colour (a glow), _extra pt wider each side. Under a
// third of a pixel it is not drawn. One whose points or width reach FUDE_ZOOM_RENDER_FAR is cut to the screen on the
// CPU (fude_zoom_draw_huge_stroke); one far bigger than the screen is drawn only in the runs that come near it.
void fude_zoom_render_stroke_as(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half, const rde_color* _as, f32 _extra) {
    const fude_zoom_object* _o     = fude_zoom_scene_object(_s, _object);
    const f64               _scale = fude_zoom_sim_scale(_to_screen);
    const f64               _size  = rde_math_max_f64(_o->box.max_x - _o->box.min_x, _o->box.max_y - _o->box.min_y) * _scale;
    if(_size < 0.35) {
        return;   // under a third of a pixel: nothing to see
    }
    const fude_zoom_decoded* _d = fude_zoom_render_decoded(_r, _s, _object);
    if(_d == NULL || _d->count == 0) {
        return;
    }
    const rde_color _color  = _as != NULL ? *_as : fude_theme_resolve(_o->color);
    const b8        _marker = (_o->flags & FUDE_ZOOM_FLAG_MARKER) != 0;
    const f32*      _xy     = (const f32*)_d->xy.memory;
    const f32*      _dr     = (const f32*)_d->radius.memory;

    // The stroke's own place (its rotation and scale) and then the frame's way
    // to the screen; the translation through the f64 transform once, the points
    // as small offsets from it — exact for the whole stroke.
    const fude_zoom_sim _all = fude_zoom_sim_compose(_to_screen, fude_zoom_object_sim(_o));
    const fude_zoom_v2  _t   = { _all.tx, _all.ty };
    const f64 _a = _all.a, _b = _all.b;
    const f64 _wscale = _scale * _o->scale;   // half-widths: as drawn, to the screen
    const f64 _max_r  = (f64)_o->radius * _wscale;
    const b8  _far   = _max_r >= FUDE_ZOOM_RENDER_FAR || rde_math_abs_f64(_t.x) + _size >= FUDE_ZOOM_RENDER_FAR || rde_math_abs_f64(_t.y) + _size >= FUDE_ZOOM_RENDER_FAR;

    if(_far) {
        rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
        rde_arr _p_arr   = rde_arr_new(sizeof(fude_zoom_v2), _heap);
        rde_arr _rad_arr = rde_arr_new(sizeof(f64), _heap);
        rde_arr_resize(&_p_arr, _d->count);
        rde_arr_resize(&_rad_arr, _d->count);
        fude_zoom_v2* _p   = (fude_zoom_v2*)_p_arr.memory;   // (sized once: they stay put)
        f64*          _rad = (f64*)_rad_arr.memory;
        for(u32 _i = 0; _i < _d->count; _i++) {
            const f64 _x = _xy[_i * 2u], _y = _xy[_i * 2u + 1u];
            _p[_i]   = (fude_zoom_v2){ _t.x + _a * _x - _b * _y, _t.y + _b * _x + _a * _y };
            _rad[_i] = rde_math_max_f64((f64)_dr[_i] * _wscale, (f64)FUDE_ZOOM_RENDER_MIN_RADIUS) + (f64)_extra;
        }
        fude_zoom_draw_huge_stroke(_p, _rad, _d->count, (fude_zoom_v2){ _half.x * 2.0, _half.y * 2.0 }, _color);
        rde_arr_free(&_p_arr);
        rde_arr_free(&_rad_arr);
        _r->strokes_drawn++;
        _r->points_drawn += _d->count;
        return;
    }

    // Points closer than a fraction of a pixel to the last one drawn are left
    // out (zoomed out, a long stroke is a few pixels: a few points); the
    // marker's, closer than most of its width (it is see-through: where its
    // outline folds over itself it would show darker — the ink page's rule).
    rde_arr_clear(&_r->screen);
    rde_arr_clear(&_r->radii);
    rde_vec_2F* _pts = rde_arr_add_n(&_r->screen, _d->count);
    f32*        _rr  = rde_arr_add_n(&_r->radii, _d->count);
    u32         _n   = 0;
    for(u32 _i = 0; _i < _d->count; _i++) {
        const f64 _x  = _xy[_i * 2u], _y = _xy[_i * 2u + 1u];
        const f32 _sx = (f32)(_t.x + _a * _x - _b * _y);
        const f32 _sy = (f32)(_t.y + _b * _x + _a * _y);
        f32       _sr = (f32)((f64)_dr[_i] * _wscale);
        _sr = (_sr < FUDE_ZOOM_RENDER_MIN_RADIUS ? FUDE_ZOOM_RENDER_MIN_RADIUS : _sr) + _extra;
        if(_n > 0 && _i + 1u < _d->count) {
            const f32 _dx   = _sx - _pts[_n - 1u].x, _dy = _sy - _pts[_n - 1u].y;
            const f32 _step = _marker ? _sr * 0.75f : 0.6f;
            if(_dx * _dx + _dy * _dy < _step * _step) {
                continue;
            }
        }
        _pts[_n] = (rde_vec_2F){ _sx, _sy };
        _rr[_n]  = _sr;
        _n++;
    }
    if(_n < 2u || _size <= 2.0 * (_half.x + _half.y)) {
        rde_rendering_2d_draw_stroke(_pts, _rr, _n, _color);
        _r->strokes_drawn++;
        _r->points_drawn += _n;
        return;
    }
    // Far bigger than the screen (zoomed in on it): only the runs of it that
    // come near the screen, each a stroke of its own — every round join of the
    // whole stroke tessellated, far out of sight, runs the engine's buffers out
    // (draws dropped: the ink flickers, then goes). A run starts and ends at
    // points further than their width from the screen: its caps are not seen.
    const f32 _hx = (f32)_half.x + 2.0f, _hy = (f32)_half.y + 2.0f;
    u32 _start = UINT32_MAX, _drawn = 0;
    for(u32 _i = 0; _i + 1u < _n; _i++) {
        const f32 _w    = rde_math_max_f32(_rr[_i], _rr[_i + 1u]);
        const b8  _near = rde_math_min_f32(_pts[_i].x, _pts[_i + 1u].x) - _w <= _hx && rde_math_max_f32(_pts[_i].x, _pts[_i + 1u].x) + _w >= -_hx &&
                          rde_math_min_f32(_pts[_i].y, _pts[_i + 1u].y) - _w <= _hy && rde_math_max_f32(_pts[_i].y, _pts[_i + 1u].y) + _w >= -_hy;
        if(_near && _start == UINT32_MAX) {
            _start = _i;
        } else if(!_near && _start != UINT32_MAX) {
            rde_rendering_2d_draw_stroke(&_pts[_start], &_rr[_start], _i - _start + 1u, _color);
            _drawn += _i - _start + 1u;
            _start  = UINT32_MAX;
        }
    }
    if(_start != UINT32_MAX) {
        rde_rendering_2d_draw_stroke(&_pts[_start], &_rr[_start], _n - _start, _color);
        _drawn += _n - _start;
    }
    if(_drawn > 0) {
        _r->strokes_drawn++;
        _r->points_drawn += _drawn;
    }
}

// --- fills (fill.h) -------------------------------------------------------------------------------

// Triangles (three points each, screen units) drawn in _color.
void fude_zoom_render_triangles(const fude_zoom_v2* _t, u32 _count, rde_color _color) {
    for(u32 _i = 0; _i < _count; _i++) {
        const fude_zoom_v2* _c = &_t[_i * 3u];
        rde_rendering_2d_draw_triangle((rde_vec_2F){ (f32)_c[0].x, (f32)_c[0].y }, (rde_vec_2F){ (f32)_c[1].x, (f32)_c[1].y },
                                       (rde_vec_2F){ (f32)_c[2].x, (f32)_c[2].y }, _color, NULL);
    }
}

// Triangles (three points each, in their own units) drawn through _to in _color: each corner placed in f64, then
// handed to the engine as f32, as every point on the screen is.
void fude_zoom_render_triangles_through(const fude_zoom_v2* _t, u32 _count, fude_zoom_sim _to, rde_color _color) {
    for(u32 _i = 0; _i < _count; _i++) {
        const fude_zoom_v2 _a = fude_zoom_sim_apply(_to, _t[_i * 3u]), _b = fude_zoom_sim_apply(_to, _t[_i * 3u + 1u]), _c = fude_zoom_sim_apply(_to, _t[_i * 3u + 2u]);
        rde_rendering_2d_draw_triangle((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, (rde_vec_2F){ (f32)_c.x, (f32)_c.y }, _color, NULL);
    }
}

// A polygon's inside (screen units, any shape): what of it is on the guard box,
// as triangles. For one far bigger than the screen, or seen once.
void fude_zoom_render_polygon_fill(fude_zoom_renderer* _r, const fude_zoom_v2* _p, u32 _n, fude_zoom_v2 _guard, rde_color _color) {
    const u32 _k = fude_zoom_fill_clip(_p, _n, (fude_zoom_box){ -_guard.x, -_guard.y, _guard.x, _guard.y }, &_r->cut);
    rde_arr_clear(&_r->fill);
    const u32 _t = fude_zoom_fill_triangulate((const fude_zoom_v2*)_r->cut.memory, _k, &_r->fill);
    fude_zoom_render_triangles((const fude_zoom_v2*)_r->fill.memory, _t, _color);
}

#define FUDE_ZOOM_RENDER_EDGE 0.55f   // the soft edge round a fill (pt): a hairline of its own colour along its edge

// A fill's edges made soft: its triangles end hard, so its edge is drawn over
// them as a hairline of its colour, which the stroke renderer smooths as it does
// a pen's line (a line rubbed out keeps the look of a line). Only the edge as it
// shows: a cut fill's (fill.h's edges_rings, made once and kept with it) leaves
// out its outline where the eraser took it and its cuts beyond the outline.
RDE_INTERNAL void fude_zoom_render_fill_edges(fude_zoom_renderer* _r, fude_zoom_decoded* _d, fude_zoom_sim _all, rde_color _color, f32 _radius) {
    if(rde_arr_is_inited(&_d->rings) && !rde_arr_is_inited(&_d->edges)) {
        rde_arr_clear(&_r->shape);
        fude_zoom_v2* _p  = rde_arr_add_n(&_r->shape, _d->count);
        const f32*    _at = (const f32*)_d->xy.memory;
        for(u32 _i = 0; _i < _d->count; _i++) {
            _p[_i] = (fude_zoom_v2){ _at[_i * 2u], _at[_i * 2u + 1u] };
        }
        rde_arr_clear(&_r->fill);
        rde_arr _lines = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        const u32 _k = fude_zoom_fill_edges_rings((const fude_zoom_v2*)_r->shape.memory, (const u32*)_d->rings.memory, _d->count, &_r->fill, &_lines);
        rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
        _d->edges      = rde_arr_new(sizeof(f32), _heap);
        _d->edge_lines = rde_arr_new(sizeof(u32), _heap);
        rde_arr_resize(&_d->edges, (usize)_k * 2u);
        rde_arr_resize(&_d->edge_lines, _k);
        _d->edge_count = _k;
        const fude_zoom_v2* _e  = (const fude_zoom_v2*)_r->fill.memory;
        f32*                _ex = (f32*)_d->edges.memory;
        u32*                _el = (u32*)_d->edge_lines.memory;
        for(u32 _i = 0; _i < _k; _i++) {
            _ex[_i * 2u]      = (f32)_e[_i].x;
            _ex[_i * 2u + 1u] = (f32)_e[_i].y;
            _el[_i]           = ((const u32*)_lines.memory)[_i];
        }
        rde_arr_free(&_lines);
        _r->cache_bytes += (u64)_k * 12u;
    }
    const b8   _cut   = rde_arr_is_inited(&_d->rings);
    const u32  _count = _cut ? _d->edge_count : _d->count;
    const f32* _xy    = (const f32*)(_cut ? _d->edges.memory : _d->xy.memory);
    const u32* _ids   = (const u32*)_d->edge_lines.memory;
    for(u32 _start = 0; _start < _count;) {
        u32 _end = _start + 1u;
        while(_end < _count && (!_cut || _ids[_end] == _ids[_start])) {
            _end++;
        }
        const u32 _k = _end - _start;
        const u32 _m = _cut ? _k : _k + 1u;   // (an uncut fill's outline: all round, back to its first point)
        if(_k >= 2u) {
            rde_arr_clear(&_r->screen);
            rde_arr_clear(&_r->radii);
            rde_vec_2F* _pts = rde_arr_add_n(&_r->screen, _m);
            f32*        _rr  = rde_arr_add_n(&_r->radii, _m);
            for(u32 _i = 0; _i < _m; _i++) {
                const u32 _j = _start + _i % _k;
                const fude_zoom_v2 _p = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _xy[_j * 2u], _xy[_j * 2u + 1u] });
                _pts[_i] = (rde_vec_2F){ (f32)_p.x, (f32)_p.y };
                _rr[_i]  = _radius;
            }
            rde_rendering_2d_draw_stroke(_pts, _rr, _m, _color);
        }
        _start = _end;
    }
}

// A FILL: its triangles (made once, kept with its points) through its place; a
// fill far bigger than the screen cut to it first. _as: a glow — its outline.
void fude_zoom_render_fill(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half, const rde_color* _as, f32 _extra) {
    const fude_zoom_object* _o    = fude_zoom_scene_object(_s, _object);
    const f64               _size = rde_math_max_f64(_o->box.max_x - _o->box.min_x, _o->box.max_y - _o->box.min_y) * fude_zoom_sim_scale(_to_screen);
    if(_size < 0.35 || _o->count < 3u) {
        return;
    }
    fude_zoom_decoded* _d = (fude_zoom_decoded*)fude_zoom_render_decoded(_r, _s, _object);
    if(_d == NULL) {
        return;
    }
    const f32* _xy      = (const f32*)_d->xy.memory;
    const u32* _ring_of = (const u32*)_d->rings.memory;   // (NULL: all the outline's)
    const fude_zoom_sim _all = fude_zoom_sim_compose(_to_screen, fude_zoom_object_sim(_o));
    if(_as != NULL) {
        fude_zoom_render_fill_edges(_r, _d, _all, *_as, 1.5f + _extra);   // (its edge as it shows: not where the eraser took it)
        return;
    }
    const rde_color    _color = fude_theme_resolve(_o->color);
    const fude_zoom_v2 _guard = { _half.x * 2.0, _half.y * 2.0 };
    if(_size >= FUDE_ZOOM_RENDER_FAR * 0.1) {
        // Far bigger than the screen: each ring cut to it, on screen, then filled.
        rde_arr_clear(&_r->shape);
        rde_arr_clear(&_r->radii);   // (the cut rings' numbers, as f32)
        for(u32 _start = 0; _start < _d->count;) {
            const u32 _ring = _ring_of != NULL ? _ring_of[_start] : 0u;
            u32 _end = _start + 1u;
            while(_end < _d->count && (_ring_of != NULL ? _ring_of[_end] : 0u) == _ring) {
                _end++;
            }
            rde_arr_clear(&_r->fill);
            fude_zoom_v2* _p = rde_arr_add_n(&_r->fill, _end - _start);
            for(u32 _i = _start; _i < _end; _i++) {
                _p[_i - _start] = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _xy[_i * 2u], _xy[_i * 2u + 1u] });
            }
            const u32 _k = fude_zoom_fill_clip((const fude_zoom_v2*)_r->fill.memory, _end - _start, (fude_zoom_box){ -_guard.x, -_guard.y, _guard.x, _guard.y }, &_r->cut);
            if(_k >= 3u) {
                rde_memcpy(rde_arr_add_n(&_r->shape, _k), _r->cut.memory, (usize)_k * sizeof(fude_zoom_v2));
                f32* _rr = rde_arr_add_n(&_r->radii, _k);
                for(u32 _i = 0; _i < _k; _i++) {
                    _rr[_i] = (f32)_ring;
                }
            }
            _start = _end;
        }
        const u32 _m = (u32)rde_arr_length(&_r->shape);
        rde_arr _rings_arr = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        rde_arr_resize(&_rings_arr, _m);
        u32* _rings = (u32*)_rings_arr.memory;
        for(u32 _i = 0; _i < _m; _i++) {
            _rings[_i] = (u32)((const f32*)_r->radii.memory)[_i];
        }
        rde_arr_clear(&_r->fill);
        const u32 _t = fude_zoom_fill_triangulate_rings((const fude_zoom_v2*)_r->shape.memory, _rings, _m, &_r->fill);
        fude_zoom_render_triangles((const fude_zoom_v2*)_r->fill.memory, _t, _color);
        rde_arr_free(&_rings_arr);
    } else {
        if(!rde_arr_is_inited(&_d->tris)) {
            rde_arr_clear(&_r->shape);
            fude_zoom_v2* _p = rde_arr_add_n(&_r->shape, _d->count);
            for(u32 _i = 0; _i < _d->count; _i++) {
                _p[_i] = (fude_zoom_v2){ _xy[_i * 2u], _xy[_i * 2u + 1u] };
            }
            rde_arr_clear(&_r->fill);
            const u32 _t = fude_zoom_fill_triangulate_rings((const fude_zoom_v2*)_r->shape.memory, _ring_of, _d->count, &_r->fill);
            _d->tris = rde_arr_new(sizeof(f32), rde_memory_allocator_get_default_std());
            rde_arr_resize(&_d->tris, (usize)_t * 6u);
            _d->tri_count = _t;
            const fude_zoom_v2* _f  = (const fude_zoom_v2*)_r->fill.memory;
            f32*                _tr = (f32*)_d->tris.memory;
            for(u32 _i = 0; _i < _t * 3u; _i++) {
                _tr[_i * 2u]      = (f32)_f[_i].x;
                _tr[_i * 2u + 1u] = (f32)_f[_i].y;
            }
            _r->cache_bytes += (u64)_t * 24u;
        }
        const f32* _tris = (const f32*)_d->tris.memory;
        for(u32 _i = 0; _i < _d->tri_count; _i++) {
            const f32* _c = &_tris[_i * 6u];
            const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _c[0], _c[1] });
            const fude_zoom_v2 _b = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _c[2], _c[3] });
            const fude_zoom_v2 _e = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _c[4], _c[5] });
            rde_rendering_2d_draw_triangle((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, (rde_vec_2F){ (f32)_e.x, (f32)_e.y }, _color, NULL);
        }
        if(_color.a == 255u && _size >= 3.0) {
            fude_zoom_render_fill_edges(_r, _d, _all, _color, FUDE_ZOOM_RENDER_EDGE);   // (a see-through marker's would show darker where they overlap)
        }
    }
    _r->strokes_drawn++;
    _r->points_drawn += _d->count;
}
