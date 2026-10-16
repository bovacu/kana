// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/render_parts.h"
#include "zoom/cut.h"

// ===========================================================================
// See render.h. The renderer, strokes and fills: polygons clipped to the screen (a stroke far bigger than it, cut on
// the CPU), a stroke drawn (its points as small offsets from one f64 translation; its mesh kept while the view pans;
// only the runs near the screen when it is far bigger than it), and a fill drawn (its triangles made once and kept
// with its points; its edge made soft).
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

// A stroke near the screen is tessellated (rde_rendering_2d_tessellate_stroke) in screen units round where its
// translation lands on the screen, and moved there as its mesh is drawn (rde_rendering_2d_draw_stroke_mesh). Every
// number its mesh is made from but that translation stays the same while the view pans, so the mesh is kept with the
// stroke's decoded points and drawn again, moved, until its size or turn on the screen (a zoom), or a pixel's size,
// changes. A kept mesh and one made in the same draw are the same numbers, so keeping it changes no pixel; against
// tessellating where the stroke is on the screen (as before 2026-10-15), positions differ by the rounding of the move
// alone, a small fraction of a pixel. docs/design/rendering.md, "Strokes' meshes", has the key, bound and numbers.

// Draws a stroke in its own colour.
void fude_zoom_render_stroke(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half) {
    fude_zoom_render_stroke_as(_r, _s, _object, _to_screen, _half, NULL, 0.0f);
}

// Stroke _d's points as drawn, into the renderer's screen and radii; returns how many. Each point is turned and scaled
// by (_a, _b), its way to the screen without the move, so the points are offsets from where its translation lands on
// the screen (a pan leaves them as they are); each half-width is scaled by _wscale, never under
// FUDE_ZOOM_RENDER_MIN_RADIUS, and _extra wider. Points closer than a fraction of a pixel to the last one kept are left
// out (zoomed out, a long stroke is a few pixels: a few points); the marker's, closer than most of its width (it is
// see-through: where its outline folds over itself it would show darker — the ink page's rule); the last is always
// kept. The distances are the screen's, so which points are kept does not depend on where the view is either.
RDE_INTERNAL u32 fude_zoom_render_stroke_points(fude_zoom_renderer* _r, const fude_zoom_decoded* _d, f64 _a, f64 _b, f64 _wscale, f32 _extra, b8 _marker) {
    const f32* _xy = (const f32*)_d->xy.memory;
    const f32* _dr = (const f32*)_d->radius.memory;
    rde_arr_clear(&_r->screen);
    rde_arr_clear(&_r->radii);
    rde_vec_2F* _pts = rde_arr_add_n(&_r->screen, _d->count);
    f32*        _rr  = rde_arr_add_n(&_r->radii, _d->count);
    u32         _n   = 0;
    for(u32 _i = 0; _i < _d->count; _i++) {
        const f64 _x  = _xy[_i * 2u], _y = _xy[_i * 2u + 1u];
        const f32 _sx = (f32)(_a * _x - _b * _y);
        const f32 _sy = (f32)(_b * _x + _a * _y);
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
    return _n;
}

// Lets stroke _d's kept mesh go, its bytes off the budget (its decoded points stay).
RDE_INTERNAL void fude_zoom_render_mesh_let_go(fude_zoom_renderer* _r, fude_zoom_decoded* _d) {
    _r->mesh_bytes -= _d->mesh_bytes;
    _d->mesh_bytes  = 0;
    rde_stroke_mesh_free(&_d->mesh);
}

// Makes room for _bytes more of kept meshes: while the budget (mesh_budget) would be passed, the mesh drawn longest
// ago, never one drawn in this draw, is let go (its stroke's decoded points stay), found by a scan of the draw stamps
// as the decoded strokes' are (render.c's fude_zoom_render_decoded says why not an rde_lru). Returns whether there is
// room. A draw that finds nothing to let go looks no more until the next draw (mesh_full): a screen of more strokes
// than the budget holds scans the cache once a draw, not once a stroke.
RDE_INTERNAL b8 fude_zoom_render_mesh_room(fude_zoom_renderer* _r, u64 _bytes) {
    while(_r->mesh_bytes + _bytes > _r->mesh_budget) {
        if(_r->mesh_full == _r->draws) {
            return false;
        }
        fude_zoom_decoded* _d     = (fude_zoom_decoded*)_r->cache.memory;
        u32                _worst = UINT32_MAX;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_r->cache); _i++) {
            if(_d[_i].mesh_bytes > 0u && _d[_i].mesh_used != _r->draws && (_worst == UINT32_MAX || _d[_i].mesh_used < _d[_worst].mesh_used)) {
                _worst = _i;
            }
        }
        if(_worst == UINT32_MAX) {
            _r->mesh_full = _r->draws;
            return false;
        }
        fude_zoom_render_mesh_let_go(_r, &_d[_worst]);
    }
    return true;
}

// What mesh _m holds in bytes: its arrays' room, as counted against the budget.
RDE_INTERNAL u64 fude_zoom_render_mesh_held(const rde_stroke_mesh* _m) {
    return (u64)_m->vertices.capacity * sizeof(rde_stroke_vertex) + (u64)_m->indices.capacity * sizeof(u32);
}

// Swaps stroke _d's mesh and the scratch, arrays and all (both are on the std allocator).
RDE_INTERNAL void fude_zoom_render_mesh_swap(fude_zoom_renderer* _r, fude_zoom_decoded* _d) {
    const rde_stroke_mesh _was = _d->mesh;
    _d->mesh          = _r->mesh_scratch;
    _r->mesh_scratch  = _was;
}

// Stroke _d's mesh, drawn turned and scaled to the screen by (_a, _b), its half-widths scaled by _wscale and _extra
// wider, a pixel _px wide, and how many points it is made from (*_points): the kept one, or made now.
//
// The cache: each stroke's mesh is kept in its decoded entry (render.c's cache), keyed by every number it is made from
// but the translation, compared exactly: the stroke (its entry: an object's points never change, an edit makes new
// objects, and a closed canvas trims the cache), _a, _b, _wscale and _px. The marker flag, the other thing it is made
// from, is the stroke's own and never changes. The colour is not in a mesh (it is given as the mesh is drawn), so a
// theme, a faint stroke or another colour draws the kept one. Its bound is bytes, mesh_budget
// (FUDE_ZOOM_RENDER_MESHES), counted as the arrays' room; let go with the entry, by mesh_room past the budget, and by
// fude_zoom_render_trim. A mesh that finds no room is drawn from the scratch and made again next draw, the stroke's
// stale one let go: past the budget the meshes kept stay kept, rather than each pushing out the next. A glow (_extra)
// is always made into the scratch and not kept: it would push out the stroke's own, drawn in the same draw. The pointer
// returned is good until the next stroke is drawn.
//
// A stroke's first mesh is made into the scratch and copied into arrays of its size. One it has already is made again
// over its own arrays, so a zoom, which makes every mesh each frame, copies and allocates nothing more than before
// (they grow when it does, and are made again at its size when it uses under half of them).
RDE_INTERNAL const rde_stroke_mesh* fude_zoom_render_stroke_mesh(fude_zoom_renderer* _r, fude_zoom_decoded* _d, b8 _marker, f64 _a, f64 _b, f64 _wscale, f32 _extra, f32 _px, u32* _points) {
    if(_extra == 0.0f && _d->mesh_bytes > 0u && _d->mesh_a == _a && _d->mesh_b == _b && _d->mesh_wscale == _wscale && _d->mesh_px == _px) {
        _d->mesh_used = _r->draws;
        *_points      = _d->mesh_points;
        return &_d->mesh;
    }
    b8 _own = _extra == 0.0f && _d->mesh_bytes > 0u;
    *_points = fude_zoom_render_stroke_points(_r, _d, _a, _b, _wscale, _extra, _marker);
    rde_rendering_2d_tessellate_stroke((const rde_vec_2F*)_r->screen.memory, (const f32*)_r->radii.memory, *_points, _px, _own ? &_d->mesh : &_r->mesh_scratch);
    _r->meshes_made++;
    if(_extra != 0.0f) {
        return &_r->mesh_scratch;
    }
    _r->mesh_bytes -= _d->mesh_bytes;   // (its stale mesh off the budget; drawn now, it is never the one let go for room)
    _d->mesh_bytes  = 0u;
    _d->mesh_used   = _r->draws;
    const rde_stroke_mesh* _made = _own ? &_d->mesh : &_r->mesh_scratch;
    const u32 _nv   = (u32)rde_arr_length(&_made->vertices), _ni = (u32)rde_arr_length(&_made->indices);
    const u64 _need = (u64)(_nv + 1u) * sizeof(rde_stroke_vertex) + (u64)(_ni + 1u) * sizeof(u32);   // (rde_arr grows when full)
    if(_own && fude_zoom_render_mesh_held(&_d->mesh) > 2u * _need) {
        fude_zoom_render_mesh_swap(_r, _d);   // (under half of its arrays used: into the scratch, then copied to its size)
        rde_stroke_mesh_free(&_d->mesh);
        _own = false;
    }
    if(!fude_zoom_render_mesh_room(_r, _own ? fude_zoom_render_mesh_held(&_d->mesh) : _need)) {
        if(_own) {
            fude_zoom_render_mesh_swap(_r, _d);   // (drawn from the scratch; the arrays it had go)
        }
        rde_stroke_mesh_free(&_d->mesh);
        return &_r->mesh_scratch;
    }
    if(!_own) {
        rde_stroke_mesh_free(&_d->mesh);
        rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
        _d->mesh = (rde_stroke_mesh){ rde_arr_new_with_capacity(sizeof(rde_stroke_vertex), _nv + 1u, _heap), rde_arr_new_with_capacity(sizeof(u32), _ni + 1u, _heap) };
        rde_memcpy(rde_arr_add_n(&_d->mesh.vertices, _nv), _r->mesh_scratch.vertices.memory, (usize)_nv * sizeof(rde_stroke_vertex));
        rde_memcpy(rde_arr_add_n(&_d->mesh.indices, _ni), _r->mesh_scratch.indices.memory, (usize)_ni * sizeof(u32));
    }
    _d->mesh_a      = _a;
    _d->mesh_b      = _b;
    _d->mesh_wscale = _wscale;
    _d->mesh_px     = _px;
    _d->mesh_points = *_points;
    _d->mesh_bytes  = fude_zoom_render_mesh_held(&_d->mesh);
    _r->mesh_bytes += _d->mesh_bytes;
    return &_d->mesh;
}

// Points _p (offsets from _at on the screen) with half-widths _rr, _n of them, tessellated into the scratch mesh and
// drawn moved to _at in _color: a stroke, or a run of one, drawn once.
RDE_INTERNAL void fude_zoom_render_stroke_once(fude_zoom_renderer* _r, const rde_vec_2F* _p, const f32* _rr, u32 _n, f32 _px, rde_vec_2F _at, rde_color _color) {
    rde_rendering_2d_tessellate_stroke(_p, _rr, _n, _px, &_r->mesh_scratch);
    rde_rendering_2d_draw_stroke_mesh(&_r->mesh_scratch, _at, _color);
    _r->meshes_made++;
}

// Draws stroke _object through _to_screen; _as: drawn all in that colour (a glow), _extra pt wider each side. Under a
// third of a pixel it is not drawn. One whose points or width reach FUDE_ZOOM_RENDER_FAR is cut to the screen on the
// CPU (fude_zoom_draw_huge_stroke); one far bigger than the screen is drawn only in the runs that come near it; any
// other is drawn whole, from its mesh (kept while the view pans: fude_zoom_render_stroke_mesh).
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
        const f32*            _xy   = (const f32*)_d->xy.memory;
        const f32*            _dr   = (const f32*)_d->radius.memory;
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

    const f32        _px = rde_rendering_2d_get_pixel_size();
    const rde_vec_2F _at = { (f32)_t.x, (f32)_t.y };   // (where its translation lands: the mesh is moved there)
    if(_size <= 2.0 * (_half.x + _half.y)) {
        u32 _n = 0;
        const rde_stroke_mesh* _mesh = fude_zoom_render_stroke_mesh(_r, (fude_zoom_decoded*)_d, _marker, _a, _b, _wscale, _extra, _px, &_n);
        rde_rendering_2d_draw_stroke_mesh(_mesh, _at, _color);
        _r->strokes_drawn++;
        _r->points_drawn += _n;
        return;
    }
    // Far bigger than the screen (zoomed in on it): only the runs of it that
    // come near the screen, each a stroke of its own — every round join of the
    // whole stroke tessellated, far out of sight, runs the engine's buffers out
    // (draws dropped: the ink flickers, then goes). A run starts and ends at
    // points further than their width from the screen: its caps are not seen.
    // Made each draw, not kept: which runs are drawn changes as the view moves.
    const u32         _n   = fude_zoom_render_stroke_points(_r, _d, _a, _b, _wscale, _extra, _marker);
    const rde_vec_2F* _pts = (const rde_vec_2F*)_r->screen.memory;
    const f32*        _rr  = (const f32*)_r->radii.memory;
    if(_n < 2u) {
        fude_zoom_render_stroke_once(_r, _pts, _rr, _n, _px, _at, _color);
        _r->strokes_drawn++;
        _r->points_drawn += _n;
        return;
    }
    const f32 _hx = (f32)_half.x + 2.0f, _hy = (f32)_half.y + 2.0f;
    u32 _start = UINT32_MAX, _drawn = 0;
    for(u32 _i = 0; _i + 1u < _n; _i++) {
        const f32 _w    = rde_math_max_f32(_rr[_i], _rr[_i + 1u]);
        const f32 _x0   = _pts[_i].x + _at.x, _x1 = _pts[_i + 1u].x + _at.x, _y0 = _pts[_i].y + _at.y, _y1 = _pts[_i + 1u].y + _at.y;
        const b8  _near = rde_math_min_f32(_x0, _x1) - _w <= _hx && rde_math_max_f32(_x0, _x1) + _w >= -_hx &&
                          rde_math_min_f32(_y0, _y1) - _w <= _hy && rde_math_max_f32(_y0, _y1) + _w >= -_hy;
        if(_near && _start == UINT32_MAX) {
            _start = _i;
        } else if(!_near && _start != UINT32_MAX) {
            fude_zoom_render_stroke_once(_r, &_pts[_start], &_rr[_start], _i - _start + 1u, _px, _at, _color);
            _drawn += _i - _start + 1u;
            _start  = UINT32_MAX;
        }
    }
    if(_start != UINT32_MAX) {
        fude_zoom_render_stroke_once(_r, &_pts[_start], &_rr[_start], _n - _start, _px, _at, _color);
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
