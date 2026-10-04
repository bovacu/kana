// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/render.h"
#include "zoom/fill.h"
#include "drawing/base/theme.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See render.h.
// ===========================================================================

#define FUDE_ZOOM_RENDER_F64_EPS 2.220446049250313e-16
#define FUDE_ZOOM_RENDER_FAR     1.0e5    // screen units: past this a stroke is cut to the screen on the CPU
#define FUDE_ZOOM_RENDER_ARC     32u      // segments of a huge round join's visible arc

typedef struct {
    u64 z;
    u32 object;
} fude_zoom_render_item;

void fude_zoom_render_init(fude_zoom_renderer* _r) {
    memset(_r, 0, sizeof(*_r));
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _r->visible      = rde_arr_new(sizeof(fude_zoom_visible), _heap);
    _r->cache        = rde_arr_new(sizeof(fude_zoom_decoded), _heap);
    _r->slot_of      = rde_arr_new(sizeof(u32), _heap);
    _r->found        = rde_arr_new(sizeof(u32), _heap);
    _r->screen       = rde_arr_new(sizeof(rde_vec_2F), _heap);
    _r->radii        = rde_arr_new(sizeof(f32), _heap);
    _r->q            = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
    _r->shape        = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    _r->fill         = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    _r->cut          = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    _r->pictures     = rde_arr_new(sizeof(fude_zoom_picture), _heap);
    _r->cache_budget = FUDE_ZOOM_RENDER_CACHE;
}

void fude_zoom_render_trim(fude_zoom_renderer* _r) {
    // The pictures off the GPU too: they come back from their bytes when seen.
    fude_zoom_picture* _p = (fude_zoom_picture*)_r->pictures.memory;
    for(u32 _i = 0; rde_arr_is_inited(&_r->pictures) && _i < (u32)rde_arr_length(&_r->pictures); _i++) {
        if(_p[_i].texture != NULL) {
            rde_texture_unload(_p[_i].texture);
        }
    }
    if(rde_arr_is_inited(&_r->pictures)) {
        rde_arr_clear(&_r->pictures);
    }
    _r->picture_bytes = 0;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_decoded*    _d    = (fude_zoom_decoded*)_r->cache.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_r->cache); _i++) {
        _heap->free(_heap->allocator, _d[_i].xy);
        _heap->free(_heap->allocator, _d[_i].radius);
        if(_d[_i].tris != NULL) {
            _heap->free(_heap->allocator, _d[_i].tris);
        }
        if(_d[_i].rings != NULL) {
            _heap->free(_heap->allocator, _d[_i].rings);
        }
    }
    rde_arr_clear(&_r->cache);
    rde_arr_clear(&_r->slot_of);
    _r->cache_bytes = 0;
}

void fude_zoom_render_destroy(fude_zoom_renderer* _r) {
    if(rde_arr_is_inited(&_r->cache)) {
        fude_zoom_render_trim(_r);
    }
    rde_arr* _arrays[] = { &_r->visible, &_r->cache, &_r->slot_of, &_r->found, &_r->screen, &_r->radii, &_r->q, &_r->shape, &_r->pictures, &_r->fill, &_r->cut };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    memset(_r, 0, sizeof(*_r));
}

// How far off, in screen units, an f64 transform with this translation can be.
RDE_INTERNAL f64 fude_zoom_render_error(fude_zoom_sim _s) {
    return (fabs(_s.tx) + fabs(_s.ty)) * FUDE_ZOOM_RENDER_F64_EPS;
}

// --- decoded strokes ------------------------------------------------------------------------

RDE_INTERNAL void fude_zoom_render_evict(fude_zoom_renderer* _r, u32 _entry) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_decoded*    _d    = (fude_zoom_decoded*)_r->cache.memory;
    u32*                  _slot = (u32*)_r->slot_of.memory;
    _r->cache_bytes -= (u64)_d[_entry].count * 12u + (u64)_d[_entry].tri_count * 24u;
    _heap->free(_heap->allocator, _d[_entry].xy);
    _heap->free(_heap->allocator, _d[_entry].radius);
    if(_d[_entry].tris != NULL) {
        _heap->free(_heap->allocator, _d[_entry].tris);
    }
    if(_d[_entry].rings != NULL) {
        _heap->free(_heap->allocator, _d[_entry].rings);
    }
    _slot[_d[_entry].object] = 0;
    const u32 _last = (u32)rde_arr_length(&_r->cache) - 1u;
    if(_entry != _last) {
        _d[_entry] = _d[_last];
        _slot[_d[_entry].object] = _entry + 1u;
    }
    _r->cache.count--;
}

RDE_INTERNAL const fude_zoom_decoded* fude_zoom_render_decoded(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object) {
    while((u32)rde_arr_length(&_r->slot_of) <= _object) {
        const u32 _grow = fude_zoom_scene_object_count(_s) - (u32)rde_arr_length(&_r->slot_of);
        memset(rde_arr_add_n(&_r->slot_of, _grow > 0 ? _grow : 1u), 0, (usize)(_grow > 0 ? _grow : 1u) * sizeof(u32));
    }
    u32* _slot = (u32*)_r->slot_of.memory;
    if(_slot[_object] != 0) {
        fude_zoom_decoded* _d = &((fude_zoom_decoded*)_r->cache.memory)[_slot[_object] - 1u];
        _d->used = _r->draws;
        return _d;
    }
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    rde_arr_clear(&_r->q);
    rde_arr_add_n(&_r->q, _o->count);
    if(!fude_zoom_scene_points(_s, _object, (fude_zoom_qpoint*)_r->q.memory)) {
        return NULL;
    }
    // Over the budget: the least recently drawn go (never what this draw used).
    while(_r->cache_bytes + (u64)_o->count * 12u > _r->cache_budget && rde_arr_length(&_r->cache) > 0) {
        const fude_zoom_decoded* _d     = (const fude_zoom_decoded*)_r->cache.memory;
        u32                      _worst = UINT32_MAX;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_r->cache); _i++) {
            if(_d[_i].used != _r->draws && (_worst == UINT32_MAX || _d[_i].used < _d[_worst].used)) {
                _worst = _i;
            }
        }
        if(_worst == UINT32_MAX) {
            break;
        }
        fude_zoom_render_evict(_r, _worst);
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_decoded _e = {
        .object = _object, .count = _o->count, .used = _r->draws,
        .xy = _heap->malloc(_heap->allocator, (usize)_o->count * 2u * sizeof(f32)),
        .radius = _heap->malloc(_heap->allocator, (usize)_o->count * sizeof(f32)),
    };
    const fude_zoom_qpoint* _q = (const fude_zoom_qpoint*)_r->q.memory;
    for(u32 _i = 0; _i < _o->count; _i++) {
        _e.xy[_i * 2u]      = (f32)ldexp((f64)_q[_i].x, _o->q);
        _e.xy[_i * 2u + 1u] = (f32)ldexp((f64)_q[_i].y, _o->q);
        _e.radius[_i]       = fude_zoom_scene_local_radius_at(_o, &_q[_i]);   // before the object's scale: that can change
    }
    if(_o->kind == FUDE_ZOOM_KIND_FILL && (_o->channels & FUDE_ZOOM_CHANNEL_TIME)) {
        _e.rings = _heap->malloc(_heap->allocator, (usize)_o->count * sizeof(u32));
        for(u32 _i = 0; _i < _o->count; _i++) {
            _e.rings[_i] = _q[_i].time;
        }
    }
    rde_arr_add(&_r->cache, &_e);
    _r->cache_bytes += (u64)_o->count * 12u;
    _slot = (u32*)_r->slot_of.memory;
    _slot[_object] = (u32)rde_arr_length(&_r->cache);
    return &((fude_zoom_decoded*)_r->cache.memory)[rde_arr_length(&_r->cache) - 1u];
}

// --- polygons clipped to the screen (strokes far bigger than it) -----------------------------------

// _subject clipped by the convex polygon _clip (either winding): into _out, its count.
#define FUDE_ZOOM_CLIP_MAX 800u

RDE_INTERNAL u32 fude_zoom_clip_convex(const fude_zoom_v2* _subject, u32 _n, const fude_zoom_v2* _clip, u32 _m, fude_zoom_v2* _out, u32 _max) {
    static fude_zoom_v2 _a[FUDE_ZOOM_CLIP_MAX], _b[FUDE_ZOOM_CLIP_MAX];
    if(_n > FUDE_ZOOM_CLIP_MAX) {
        return 0;
    }
    memcpy(_a, _subject, _n * sizeof(fude_zoom_v2));
    // Which side is inside: the clip polygon's winding.
    f64 _area = 0.0;
    for(u32 _i = 0; _i < _m; _i++) {
        const fude_zoom_v2 _p = _clip[_i], _q = _clip[(_i + 1u) % _m];
        _area += _p.x * _q.y - _q.x * _p.y;
    }
    const f64 _side = _area >= 0.0 ? 1.0 : -1.0;
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
        memcpy(_a, _b, _k * sizeof(fude_zoom_v2));
        _count = _k;
    }
    if(_count > _max) {
        _count = _max;
    }
    memcpy(_out, _a, _count * sizeof(fude_zoom_v2));
    return _count;
}

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
        if(hypot(_corners[_i].x - _c.x, _corners[_i].y - _c.y) >= _r) {
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
    if(hypot(_nx - _c.x, _ny - _c.y) >= _r) {
        return;
    }
    // The angles the box spans from the centre (outside it: under half a turn).
    const f64 _mid = atan2(-_c.y, -_c.x);
    f64 _lo = 0.0, _hi = 0.0;
    for(u32 _i = 0; _i < 4u; _i++) {
        f64 _a = atan2(_corners[_i].y - _c.y, _corners[_i].x - _c.x) - _mid;
        while(_a > 3.141592653589793) { _a -= 6.283185307179586; }
        while(_a < -3.141592653589793) { _a += 6.283185307179586; }
        _lo = _a < _lo ? _a : _lo;
        _hi = _a > _hi ? _a : _hi;
    }
    fude_zoom_v2 _sector[FUDE_ZOOM_RENDER_ARC + 2u];
    _sector[0] = _c;
    for(u32 _k = 0; _k <= FUDE_ZOOM_RENDER_ARC; _k++) {
        const f64 _a = _mid + _lo + (_hi - _lo) * (f64)_k / (f64)FUDE_ZOOM_RENDER_ARC;
        _sector[_k + 1u] = (fude_zoom_v2){ _c.x + cos(_a) * _r, _c.y + sin(_a) * _r };
    }
    fude_zoom_draw_clipped(_sector, FUDE_ZOOM_RENDER_ARC + 2u, _guard, _color);
}

// A stroke many screens across: each segment's band and each join, cut to the box.
RDE_INTERNAL void fude_zoom_draw_huge_stroke(const fude_zoom_v2* _p, const f64* _rad, u32 _n, fude_zoom_v2 _guard, rde_color _color) {
    for(u32 _i = 0; _i < _n; _i++) {
        if(_rad[_i] >= FUDE_ZOOM_RENDER_FAR || fabs(_p[_i].x) >= FUDE_ZOOM_RENDER_FAR || fabs(_p[_i].y) >= FUDE_ZOOM_RENDER_FAR) {
            fude_zoom_draw_huge_disc(_p[_i], _rad[_i], _guard, _color);
        } else {
            const f32 _res = (f32)(_rad[_i] / 2.0);
            rde_rendering_2d_draw_circle((rde_vec_2F){ (f32)_p[_i].x, (f32)_p[_i].y }, (f32)_rad[_i], (u32)(_res < 12.0f ? 12.0f : (_res > 96.0f ? 96.0f : _res)), _color, NULL);
        }
        if(_i + 1u == _n) {
            break;
        }
        const fude_zoom_v2 _a = _p[_i], _b = _p[_i + 1u];
        const f64 _len = hypot(_b.x - _a.x, _b.y - _a.y);
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

RDE_INTERNAL void fude_zoom_render_stroke_as(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half, const rde_color* _as, f32 _extra);

RDE_INTERNAL void fude_zoom_render_stroke(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half) {
    fude_zoom_render_stroke_as(_r, _s, _object, _to_screen, _half, NULL, 0.0f);
}

// _as: drawn all in that colour (a glow), _extra pt wider each side.
RDE_INTERNAL void fude_zoom_render_stroke_as(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half, const rde_color* _as, f32 _extra) {
    const fude_zoom_object* _o     = fude_zoom_scene_object(_s, _object);
    const f64               _scale = fude_zoom_sim_scale(_to_screen);
    const f64               _size  = fmax(_o->box.max_x - _o->box.min_x, _o->box.max_y - _o->box.min_y) * _scale;
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
    const b8  _far   = _max_r >= FUDE_ZOOM_RENDER_FAR || fabs(_t.x) + _size >= FUDE_ZOOM_RENDER_FAR || fabs(_t.y) + _size >= FUDE_ZOOM_RENDER_FAR;

    if(_far) {
        rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
        fude_zoom_v2* _p   = _heap->malloc(_heap->allocator, (usize)_d->count * sizeof(fude_zoom_v2));
        f64*          _rad = _heap->malloc(_heap->allocator, (usize)_d->count * sizeof(f64));
        for(u32 _i = 0; _i < _d->count; _i++) {
            const f64 _x = _d->xy[_i * 2u], _y = _d->xy[_i * 2u + 1u];
            _p[_i]   = (fude_zoom_v2){ _t.x + _a * _x - _b * _y, _t.y + _b * _x + _a * _y };
            _rad[_i] = fmax((f64)_d->radius[_i] * _wscale, (f64)FUDE_ZOOM_RENDER_MIN_RADIUS) + (f64)_extra;
        }
        fude_zoom_draw_huge_stroke(_p, _rad, _d->count, (fude_zoom_v2){ _half.x * 2.0, _half.y * 2.0 }, _color);
        _heap->free(_heap->allocator, _p);
        _heap->free(_heap->allocator, _rad);
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
        const f64 _x  = _d->xy[_i * 2u], _y = _d->xy[_i * 2u + 1u];
        const f32 _sx = (f32)(_t.x + _a * _x - _b * _y);
        const f32 _sy = (f32)(_t.y + _b * _x + _a * _y);
        f32       _sr = (f32)((f64)_d->radius[_i] * _wscale);
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
        const f32 _w    = fmaxf(_rr[_i], _rr[_i + 1u]);
        const b8  _near = fminf(_pts[_i].x, _pts[_i + 1u].x) - _w <= _hx && fmaxf(_pts[_i].x, _pts[_i + 1u].x) + _w >= -_hx &&
                          fminf(_pts[_i].y, _pts[_i + 1u].y) - _w <= _hy && fmaxf(_pts[_i].y, _pts[_i + 1u].y) + _w >= -_hy;
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

// --- shapes ----------------------------------------------------------------------------------

// A shape (shape.h): its outline from its numbers, cut as finely as its size on
// screen asks; its line, then filled when it says so. _as and _extra as for
// strokes (a glow: the line only).
// --- fills (fill.h) -------------------------------------------------------------------------------

// Triangles (three points each, screen units) drawn in _color.
RDE_INTERNAL void fude_zoom_render_triangles(const fude_zoom_v2* _t, u32 _count, rde_color _color) {
    for(u32 _i = 0; _i < _count; _i++) {
        const fude_zoom_v2* _c = &_t[_i * 3u];
        rde_rendering_2d_draw_triangle((rde_vec_2F){ (f32)_c[0].x, (f32)_c[0].y }, (rde_vec_2F){ (f32)_c[1].x, (f32)_c[1].y },
                                       (rde_vec_2F){ (f32)_c[2].x, (f32)_c[2].y }, _color, NULL);
    }
}

// A polygon's inside (screen units, any shape): what of it is on the guard box,
// as triangles. For one far bigger than the screen, or seen once.
RDE_INTERNAL void fude_zoom_render_polygon_fill(fude_zoom_renderer* _r, const fude_zoom_v2* _p, u32 _n, fude_zoom_v2 _guard, rde_color _color) {
    const u32 _k = fude_zoom_fill_clip(_p, _n, (fude_zoom_box){ -_guard.x, -_guard.y, _guard.x, _guard.y }, &_r->cut);
    rde_arr_clear(&_r->fill);
    const u32 _t = fude_zoom_fill_triangulate((const fude_zoom_v2*)_r->cut.memory, _k, &_r->fill);
    fude_zoom_render_triangles((const fude_zoom_v2*)_r->fill.memory, _t, _color);
}

// A FILL: its triangles (made once, kept with its points) through its place; a
// fill far bigger than the screen cut to it first. _as: a glow — its outline.
RDE_INTERNAL void fude_zoom_render_fill(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half, const rde_color* _as, f32 _extra) {
    const fude_zoom_object* _o    = fude_zoom_scene_object(_s, _object);
    const f64               _size = fmax(_o->box.max_x - _o->box.min_x, _o->box.max_y - _o->box.min_y) * fude_zoom_sim_scale(_to_screen);
    if(_size < 0.35 || _o->count < 3u) {
        return;
    }
    fude_zoom_decoded* _d = (fude_zoom_decoded*)fude_zoom_render_decoded(_r, _s, _object);
    if(_d == NULL) {
        return;
    }
    const fude_zoom_sim _all = fude_zoom_sim_compose(_to_screen, fude_zoom_object_sim(_o));
    if(_as != NULL) {
        rde_arr_clear(&_r->screen);
        rde_arr_clear(&_r->radii);
        u32 _outline = 0;   // the outline's points: the first ring
        while(_outline < _d->count && (_d->rings == NULL || _d->rings[_outline] == _d->rings[0])) {
            _outline++;
        }
        rde_vec_2F* _pts = rde_arr_add_n(&_r->screen, _outline + 1u);
        f32*        _rr  = rde_arr_add_n(&_r->radii, _outline + 1u);
        for(u32 _i = 0; _i <= _outline; _i++) {
            const u32 _j = _i % _outline;
            const fude_zoom_v2 _p = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _d->xy[_j * 2u], _d->xy[_j * 2u + 1u] });
            _pts[_i] = (rde_vec_2F){ (f32)_p.x, (f32)_p.y };
            _rr[_i]  = 1.5f + _extra;
        }
        rde_rendering_2d_draw_stroke(_pts, _rr, _outline + 1u, *_as);
        return;
    }
    const rde_color    _color = fude_theme_resolve(_o->color);
    const fude_zoom_v2 _guard = { _half.x * 2.0, _half.y * 2.0 };
    if(_size >= FUDE_ZOOM_RENDER_FAR * 0.1) {
        // Far bigger than the screen: each ring cut to it, on screen, then filled.
        rde_arr_clear(&_r->shape);
        rde_arr_clear(&_r->radii);   // (the cut rings' numbers, as f32)
        for(u32 _start = 0; _start < _d->count;) {
            const u32 _ring = _d->rings != NULL ? _d->rings[_start] : 0u;
            u32 _end = _start + 1u;
            while(_end < _d->count && (_d->rings != NULL ? _d->rings[_end] : 0u) == _ring) {
                _end++;
            }
            rde_arr_clear(&_r->fill);
            fude_zoom_v2* _p = rde_arr_add_n(&_r->fill, _end - _start);
            for(u32 _i = _start; _i < _end; _i++) {
                _p[_i - _start] = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _d->xy[_i * 2u], _d->xy[_i * 2u + 1u] });
            }
            const u32 _k = fude_zoom_fill_clip((const fude_zoom_v2*)_r->fill.memory, _end - _start, (fude_zoom_box){ -_guard.x, -_guard.y, _guard.x, _guard.y }, &_r->cut);
            if(_k >= 3u) {
                memcpy(rde_arr_add_n(&_r->shape, _k), _r->cut.memory, (usize)_k * sizeof(fude_zoom_v2));
                f32* _rr = rde_arr_add_n(&_r->radii, _k);
                for(u32 _i = 0; _i < _k; _i++) {
                    _rr[_i] = (f32)_ring;
                }
            }
            _start = _end;
        }
        const u32 _m = (u32)rde_arr_length(&_r->shape);
        rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
        u32* _rings = _heap->malloc(_heap->allocator, (usize)(_m > 0 ? _m : 1u) * sizeof(u32));
        for(u32 _i = 0; _i < _m; _i++) {
            _rings[_i] = (u32)((const f32*)_r->radii.memory)[_i];
        }
        rde_arr_clear(&_r->fill);
        const u32 _t = fude_zoom_fill_triangulate_rings((const fude_zoom_v2*)_r->shape.memory, _rings, _m, &_r->fill);
        fude_zoom_render_triangles((const fude_zoom_v2*)_r->fill.memory, _t, _color);
        _heap->free(_heap->allocator, _rings);
    } else {
        if(_d->tris == NULL) {
            rde_arr_clear(&_r->shape);
            fude_zoom_v2* _p = rde_arr_add_n(&_r->shape, _d->count);
            for(u32 _i = 0; _i < _d->count; _i++) {
                _p[_i] = (fude_zoom_v2){ _d->xy[_i * 2u], _d->xy[_i * 2u + 1u] };
            }
            rde_arr_clear(&_r->fill);
            const u32 _t = fude_zoom_fill_triangulate_rings((const fude_zoom_v2*)_r->shape.memory, _d->rings, _d->count, &_r->fill);
            rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
            _d->tris      = _heap->malloc(_heap->allocator, (usize)(_t > 0 ? _t : 1u) * 6u * sizeof(f32));
            _d->tri_count = _t;
            const fude_zoom_v2* _f = (const fude_zoom_v2*)_r->fill.memory;
            for(u32 _i = 0; _i < _t * 3u; _i++) {
                _d->tris[_i * 2u]      = (f32)_f[_i].x;
                _d->tris[_i * 2u + 1u] = (f32)_f[_i].y;
            }
            _r->cache_bytes += (u64)_t * 24u;
        }
        for(u32 _i = 0; _i < _d->tri_count; _i++) {
            const f32* _c = &_d->tris[_i * 6u];
            const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _c[0], _c[1] });
            const fude_zoom_v2 _b = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _c[2], _c[3] });
            const fude_zoom_v2 _e = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _c[4], _c[5] });
            rde_rendering_2d_draw_triangle((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, (rde_vec_2F){ (f32)_e.x, (f32)_e.y }, _color, NULL);
        }
    }
    _r->strokes_drawn++;
    _r->points_drawn += _d->count;
}

RDE_INTERNAL void fude_zoom_render_shape(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half, const rde_color* _as, f32 _extra) {
    const fude_zoom_object* _o     = fude_zoom_scene_object(_s, _object);
    const f64               _scale = fude_zoom_sim_scale(_to_screen);
    const f64               _size  = fmax(_o->box.max_x - _o->box.min_x, _o->box.max_y - _o->box.min_y) * _scale;
    if(_size < 0.35) {
        return;
    }
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _count = fude_zoom_scene_shape_numbers(_s, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    if(_count == 0) {
        return;
    }
    rde_arr* _local = &_r->shape;
    b8       _closed;
    fude_zoom_shape_outline(_o->channels, _n, _count, fude_zoom_shape_segments(_size), _local, &_closed);
    const u32 _k = (u32)rde_arr_length(_local);
    if(_k < 2u || _k + 1u > FUDE_ZOOM_CLIP_MAX) {
        return;
    }
    const fude_zoom_sim _all    = fude_zoom_sim_compose(_to_screen, fude_zoom_object_sim(_o));
    const rde_color     _color  = _as != NULL ? *_as : fude_theme_resolve(_o->color);
    const f64           _radius = fmax((f64)_o->radius * fude_zoom_sim_scale(_all), (f64)FUDE_ZOOM_RENDER_MIN_RADIUS) + (f64)_extra;
    static fude_zoom_v2 _p[FUDE_ZOOM_CLIP_MAX + 1u];
    b8 _far = _radius >= FUDE_ZOOM_RENDER_FAR;
    for(u32 _i = 0; _i < _k; _i++) {
        _p[_i] = fude_zoom_sim_apply(_all, ((const fude_zoom_v2*)_local->memory)[_i]);
        _far = _far || fabs(_p[_i].x) >= FUDE_ZOOM_RENDER_FAR || fabs(_p[_i].y) >= FUDE_ZOOM_RENDER_FAR;
    }
    const fude_zoom_v2 _guard = { _half.x * 2.0, _half.y * 2.0 };
    const u32 _m = _closed ? _k + 1u : _k;
    _p[_k] = _p[0];
    // Its fill: its own colour (the Fill tool's) under the line, so the line
    // stays whole over it; its line's colour after it (below).
    const b8        _filled = _closed && (_o->flags & FUDE_ZOOM_FLAG_FILLED) && _as == NULL;
    const rde_color _fill   = (_o->flags & FUDE_ZOOM_FLAG_FILL_OWN) ? fude_theme_resolve(_o->fill) : _color;
    const b8        _under  = _filled && memcmp(&_fill, &_color, sizeof(rde_color)) != 0;
    if(_under) {
        fude_zoom_render_polygon_fill(_r, _p, _k, _guard, _fill);
    }
    if(_far) {
        static f64 _rad[FUDE_ZOOM_CLIP_MAX + 1u];
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
    // The fill after the line, to its middle: the line's soft inner edge is under
    // it (over a fill of its own colour that edge would show as a light seam: 2D
    // blending adds a soft edge's colour to what is under it).
    if(_filled && !_under) {
        if(_far || _o->channels == FUDE_ZOOM_SHAPE_POLYGON) {
            fude_zoom_render_polygon_fill(_r, _p, _k, _guard, _color);   // a polygon may be concave
        } else {
            static rde_vec_2F _f[FUDE_ZOOM_CLIP_MAX];
            for(u32 _i = 0; _i < _k; _i++) {
                _f[_i] = (rde_vec_2F){ (f32)_p[_i].x, (f32)_p[_i].y };
            }
            rde_rendering_2d_draw_polygon(_f, _k, _color, NULL);
        }
    }
    _r->strokes_drawn++;
    _r->points_drawn += _m;
}

// --- pictures --------------------------------------------------------------------------------

rde_texture* fude_zoom_render_picture(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, rde_vec_2UI* _size) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    fude_zoom_picture*      _p = (fude_zoom_picture*)_r->pictures.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_r->pictures); _i++) {
        if(_p[_i].blob == _o->blob) {
            _p[_i].used = _r->draws;
            if(_size != NULL && _p[_i].texture != NULL) {
                *_size = rde_texture_get_size(_p[_i].texture);
            }
            return _p[_i].texture;
        }
    }
    f64 _hw, _hh;
    const u8* _bytes = NULL;
    u32       _n     = 0;
    if(!fude_zoom_scene_image(_s, _object, &_hw, &_hh, &_bytes, &_n)) {
        return NULL;
    }
    rde_texture* _t = NULL;
    RDE_TRY({ _t = rde_texture_load_from_memory(_bytes, _n, "picture", NULL, NULL); });
    const rde_vec_2UI _px = _t != NULL ? rde_texture_get_size(_t) : (rde_vec_2UI){ 0, 0 };
    const u64 _cost = (u64)_px.x * (u64)_px.y * 4u;
    // Over the budget: the least recently drawn off the GPU (never this draw's).
    while(_r->picture_bytes + _cost > FUDE_ZOOM_RENDER_TEXTURES && rde_arr_length(&_r->pictures) > 0) {
        _p = (fude_zoom_picture*)_r->pictures.memory;
        u32 _worst = UINT32_MAX;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_r->pictures); _i++) {
            if(_p[_i].used != _r->draws && (_worst == UINT32_MAX || _p[_i].used < _p[_worst].used)) {
                _worst = _i;
            }
        }
        if(_worst == UINT32_MAX) {
            break;
        }
        if(_p[_worst].texture != NULL) {
            rde_texture_unload(_p[_worst].texture);
        }
        _r->picture_bytes -= _p[_worst].bytes;
        _p[_worst] = _p[rde_arr_length(&_r->pictures) - 1u];
        _r->pictures.count--;
    }
    const fude_zoom_picture _e = { _o->blob, _t, _cost, _r->draws };
    rde_arr_add(&_r->pictures, (any)&_e);
    _r->picture_bytes += _cost;
    if(_size != NULL) {
        *_size = _px;
    }
    return _t;
}

// A picture: its quad through the transforms. Many screens across (deep inside
// it), only the part on the screen is drawn — a sub-rectangle of its pixels,
// whose corners are then near the screen (no far-out vertex).
RDE_INTERNAL void fude_zoom_render_image(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half, const rde_color* _as, f32 _extra) {
    const fude_zoom_object* _o     = fude_zoom_scene_object(_s, _object);
    const f64               _size  = fmax(_o->box.max_x - _o->box.min_x, _o->box.max_y - _o->box.min_y) * fude_zoom_sim_scale(_to_screen);
    if(_size < 0.5) {
        return;
    }
    f64 _hw, _hh;
    if(!fude_zoom_scene_image(_s, _object, &_hw, &_hh, NULL, NULL)) {
        return;
    }
    const fude_zoom_sim _all   = fude_zoom_sim_compose(_to_screen, fude_zoom_object_sim(_o));
    const f64           _angle = atan2(_all.b, _all.a);
    const f64           _k     = fude_zoom_sim_scale(_all);
    if(_as != NULL) {
        // A glow: its outline.
        const fude_zoom_v2 _c[4] = { { -_hw, -_hh }, { _hw, -_hh }, { _hw, _hh }, { -_hw, _hh } };
        rde_vec_2F _pts[4];
        for(u32 _i = 0; _i < 4u; _i++) {
            const fude_zoom_v2 _p = fude_zoom_sim_apply(_all, _c[_i]);
            _pts[_i] = (rde_vec_2F){ (f32)_p.x, (f32)_p.y };
        }
        rde_rendering_2d_draw_polygon_border(_pts, 4u, 1.5f + _extra, *_as, NULL);
        return;
    }
    rde_vec_2UI  _px;
    rde_texture* _t = fude_zoom_render_picture(_r, _s, _object, &_px);
    if(_t == NULL || _px.x == 0u || _px.y == 0u) {
        return;
    }
    // The part of it on the screen (a box round the screen, in its own units),
    // in its pixels: all of it, unless it reaches far past the screen.
    f64 _u0 = -_hw, _v0 = -_hh, _u1 = _hw, _v1 = _hh;
    if(_k * fmax(_hw, _hh) > FUDE_ZOOM_RENDER_FAR * 0.1) {
        const fude_zoom_box _seen = fude_zoom_sim_box(fude_zoom_sim_inverse(_all), (fude_zoom_box){ -_half.x * 1.5, -_half.y * 1.5, _half.x * 1.5, _half.y * 1.5 });
        _u0 = fmax(_u0, _seen.min_x); _v0 = fmax(_v0, _seen.min_y);
        _u1 = fmin(_u1, _seen.max_x); _v1 = fmin(_v1, _seen.max_y);
        if(_u0 >= _u1 || _v0 >= _v1) {
            return;
        }
    }
    // Its own units → pixels, from its bottom-left (the engine's way for a
    // texture's rectangles: Y up, as here).
    const f64 _sx = (f64)_px.x / (2.0 * _hw), _sy = (f64)_px.y / (2.0 * _hh);
    const rde_vec_2F _bl = { (f32)((_u0 + _hw) * _sx), (f32)((_v0 + _hh) * _sy) };
    const rde_vec_2F _tr = { (f32)((_u1 + _hw) * _sx), (f32)((_v1 + _hh) * _sy) };
    const fude_zoom_v2 _mid = fude_zoom_sim_apply(_all, (fude_zoom_v2){ (_u0 + _u1) * 0.5, (_v0 + _v1) * 0.5 });
    // The whole picture drawn at its pixels' size times this is its size on screen.
    const rde_vec_2F _scale = { (f32)(_k / _sx), (f32)(_k / _sy) };
    rde_rendering_2d_draw_texture_partial_2(_t, (rde_vec_3F){ (f32)_mid.x, (f32)_mid.y, 0.0f }, _scale, (f32)(-_angle * 57.29577951308232), (rde_color){ 255, 255, 255, 255 }, _bl, _tr);
    _r->strokes_drawn++;
}

// --- frames ----------------------------------------------------------------------------------

RDE_INTERNAL int fude_zoom_render_by_z(const void* _a, const void* _b) {
    const u64 _za = ((const fude_zoom_render_item*)_a)->z, _zb = ((const fude_zoom_render_item*)_b)->z;
    return _za < _zb ? -1 : (_za > _zb ? 1 : 0);
}

RDE_INTERNAL void fude_zoom_render_frame(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _frame, fude_zoom_sim _to_screen, fude_zoom_v2 _half, u32 _depth) {
    if(_depth > 4096u) {
        return;
    }
    const fude_zoom_visible _v = { _frame, _to_screen, fude_zoom_scene_depth(_s, _frame) };
    rde_arr_add(&_r->visible, (any)&_v);
    _r->frames_drawn++;

    // The screen (a little more) in this frame's units.
    const fude_zoom_box _screen = { -_half.x - 2.0, -_half.y - 2.0, _half.x + 2.0, _half.y + 2.0 };
    const fude_zoom_box _view   = fude_zoom_sim_box(fude_zoom_sim_inverse(_to_screen), _screen);

    const fude_zoom_frame* _f = fude_zoom_scene_frame(_s, _frame);
    rde_arr _list = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_scene_query(_s, _frame, _view, &_list);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_f->kids); _i++) {
        const u32               _k = ((const u32*)_f->kids.memory)[_i];
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _k);
        if(_o->flags & FUDE_ZOOM_FLAG_ALIVE && fude_zoom_box_overlaps(_o->box, _view)) {
            rde_arr_add(&_list, (any)&_k);
        }
    }
    const u32 _n = (u32)rde_arr_length(&_list);
    rde_memory_allocator* _heap  = rde_memory_allocator_get_default_std();
    fude_zoom_render_item* _items = _heap->malloc(_heap->allocator, (usize)(_n > 0 ? _n : 1u) * sizeof(fude_zoom_render_item));
    for(u32 _i = 0; _i < _n; _i++) {
        const u32 _o = ((const u32*)_list.memory)[_i];
        _items[_i] = (fude_zoom_render_item){ fude_zoom_scene_object(_s, _o)->z, _o };
    }
    rde_arr_free(&_list);
    qsort(_items, _n, sizeof(_items[0]), fude_zoom_render_by_z);

    // Markers under the rest within a frame, as on the ink page.
    const u8* _lifted   = _r->lifted != NULL ? (const u8*)_r->lifted->memory : NULL;
    const u32 _lifted_n = _r->lifted != NULL ? (u32)rde_arr_length(_r->lifted) : 0u;
    for(u32 _pass = 0; _pass < 2u; _pass++) {
        for(u32 _i = 0; _i < _n; _i++) {
            if(_items[_i].object < _lifted_n && _lifted[_items[_i].object]) {
                continue;   // the selection draws it, where the drag has it
            }
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, _items[_i].object);
            if(_o->kind == FUDE_ZOOM_KIND_STROKE) {
                if(((_o->flags & FUDE_ZOOM_FLAG_MARKER) != 0) == (_pass == 0u)) {
                    fude_zoom_render_stroke(_r, _s, _items[_i].object, _to_screen, _half);
                }
            } else if(_pass == 1u && _o->kind == FUDE_ZOOM_KIND_SHAPE) {
                fude_zoom_render_shape(_r, _s, _items[_i].object, _to_screen, _half, NULL, 0.0f);
            } else if(_pass == 1u && _o->kind == FUDE_ZOOM_KIND_IMAGE) {
                fude_zoom_render_image(_r, _s, _items[_i].object, _to_screen, _half, NULL, 0.0f);
            } else if(_o->kind == FUDE_ZOOM_KIND_FILL && ((_o->flags & FUDE_ZOOM_FLAG_MARKER) != 0) == (_pass == 0u)) {
                fude_zoom_render_fill(_r, _s, _items[_i].object, _to_screen, _half, NULL, 0.0f);   // (a marker rubbed out: with the markers)
            } else if(_pass == 1u && _o->kind == FUDE_ZOOM_KIND_FRAME) {
                const fude_zoom_frame* _c       = fude_zoom_scene_frame(_s, _o->child);
                const fude_zoom_sim    _c_sim   = fude_zoom_sim_compose(_to_screen, fude_zoom_sim_from_xform(_c->xf));
                const f64              _on_screen = fmax(_c->anchor.max_x - _c->anchor.min_x, _c->anchor.max_y - _c->anchor.min_y) * fude_zoom_sim_scale(_to_screen);
                if(_on_screen >= FUDE_ZOOM_RENDER_MIN_PX) {
                    fude_zoom_render_frame(_r, _s, _o->child, _c_sim, _half, _depth + 1u);
                }
            }
        }
    }
    _heap->free(_heap->allocator, _items);
}

// The colour something covering a point shows: a shape's own fill, if it has one.
RDE_INTERNAL rde_color fude_zoom_render_cover_color(const fude_zoom_object* _o) {
    return fude_theme_resolve(_o->kind == FUDE_ZOOM_KIND_SHAPE && (_o->flags & FUDE_ZOOM_FLAG_FILL_OWN) ? _o->fill : _o->color);
}

// What is drawn round point _p (frame _frame's units) there: the topmost alive
// stroke covering it below order key _slot (*_under) and above it (*_over).
RDE_INTERNAL void fude_zoom_render_cover(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _p, u64 _slot, u32* _under, u32* _over) {
    rde_arr_clear(&_r->found);
    fude_zoom_scene_query(_s, _frame, (fude_zoom_box){ _p.x, _p.y, _p.x, _p.y }, &_r->found);
    u64 _best_under = 0, _best_over = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_r->found); _i++) {
        const u32               _object = ((const u32*)_r->found.memory)[_i];
        const fude_zoom_object* _o      = fude_zoom_scene_object(_s, _object);
        if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
            // Inside a filled shape, or on its line.
            b8 _closed;
            fude_zoom_scene_shape_outline(_s, _object, 360u, &_r->shape, &_closed);
            const fude_zoom_v2* _pp = (const fude_zoom_v2*)_r->shape.memory;
            const u32 _k = (u32)rde_arr_length(&_r->shape);
            b8 _in = false;
            if(_closed && (_o->flags & FUDE_ZOOM_FLAG_FILLED)) {
                for(u32 _a = 0, _b = _k - 1u; _a < _k; _b = _a++) {
                    if((_pp[_a].y > _p.y) != (_pp[_b].y > _p.y) && _p.x < (_pp[_b].x - _pp[_a].x) * (_p.y - _pp[_a].y) / (_pp[_b].y - _pp[_a].y) + _pp[_a].x) {
                        _in = !_in;
                    }
                }
            }
            const f64 _rr = (f64)_o->radius * _o->scale;
            for(u32 _a = 0; _a + 1u < _k + (_closed ? 1u : 0u) && !_in; _a++) {
                const fude_zoom_v2 _u = _pp[_a], _v = _pp[(_a + 1u) % _k];
                const f64 _dx = _v.x - _u.x, _dy = _v.y - _u.y, _l2 = _dx * _dx + _dy * _dy;
                f64 _t = _l2 > 0.0 ? ((_p.x - _u.x) * _dx + (_p.y - _u.y) * _dy) / _l2 : 0.0;
                _t = _t < 0.0 ? 0.0 : (_t > 1.0 ? 1.0 : _t);
                _in = hypot(_u.x + _dx * _t - _p.x, _u.y + _dy * _t - _p.y) <= _rr;
            }
            if(_in && _o->z < _slot && (*_under == FUDE_ZOOM_NONE || _o->z > _best_under)) {
                *_under = _object; _best_under = _o->z;
            }
            if(_in && _o->z > _slot && (*_over == FUDE_ZOOM_NONE || _o->z > _best_over)) {
                *_over = _object; _best_over = _o->z;
            }
            continue;
        }
        if(_o->kind == FUDE_ZOOM_KIND_FILL && _o->count >= 3u) {
            // Inside it.
            const fude_zoom_decoded* _d = fude_zoom_render_decoded(_r, _s, _object);
            if(_d == NULL) {
                continue;
            }
            const fude_zoom_v2 _in_o = fude_zoom_sim_apply(fude_zoom_sim_inverse(fude_zoom_object_sim(_o)), _p);
            rde_arr_clear(&_r->fill);
            fude_zoom_v2* _pts = rde_arr_add_n(&_r->fill, _d->count);
            for(u32 _a = 0; _a < _d->count; _a++) {
                _pts[_a] = (fude_zoom_v2){ _d->xy[_a * 2u], _d->xy[_a * 2u + 1u] };
            }
            const b8 _in = fude_zoom_fill_inside_rings((const fude_zoom_v2*)_r->fill.memory, _d->rings, _d->count, _in_o);
            if(_in && _o->z < _slot && (*_under == FUDE_ZOOM_NONE || _o->z > _best_under)) {
                *_under = _object; _best_under = _o->z;
            }
            if(_in && _o->z > _slot && (*_over == FUDE_ZOOM_NONE || _o->z > _best_over)) {
                *_over = _object; _best_over = _o->z;
            }
            continue;
        }
        if(_o->kind != FUDE_ZOOM_KIND_STROKE || _o->count == 0) {
            continue;
        }
        rde_arr_clear(&_r->q);
        rde_arr_add_n(&_r->q, _o->count);
        const fude_zoom_qpoint* _q = (const fude_zoom_qpoint*)_r->q.memory;
        if(!fude_zoom_scene_points(_s, _object, (fude_zoom_qpoint*)_r->q.memory)) {
            continue;
        }
        b8 _covers = false;
        for(u32 _k = 0; _k < _o->count && !_covers; _k++) {
            const fude_zoom_v2 _a  = fude_zoom_scene_point_at(_o, &_q[_k]);
            const fude_zoom_v2 _b  = _k + 1u < _o->count ? fude_zoom_scene_point_at(_o, &_q[_k + 1u]) : _a;
            const f64          _rr = fmax(fude_zoom_scene_radius_at(_o, &_q[_k]), _k + 1u < _o->count ? fude_zoom_scene_radius_at(_o, &_q[_k + 1u]) : 0.0);
            const f64 _dx = _b.x - _a.x, _dy = _b.y - _a.y, _len2 = _dx * _dx + _dy * _dy;
            f64 _t = _len2 > 0.0 ? ((_p.x - _a.x) * _dx + (_p.y - _a.y) * _dy) / _len2 : 0.0;
            _t = _t < 0.0 ? 0.0 : (_t > 1.0 ? 1.0 : _t);
            const f64 _ex = _a.x + _dx * _t - _p.x, _ey = _a.y + _dy * _t - _p.y;
            _covers = _ex * _ex + _ey * _ey <= _rr * _rr;
        }
        if(!_covers) {
            continue;
        }
        if(_o->z < _slot && (*_under == FUDE_ZOOM_NONE || _o->z > _best_under)) {
            *_under     = _object;
            _best_under = _o->z;
        }
        if(_o->z > _slot && (*_over == FUDE_ZOOM_NONE || _o->z > _best_over)) {
            *_over     = _object;
            _best_over = _o->z;
        }
    }
}

void fude_zoom_render(fude_zoom_renderer* _r, const fude_zoom_scene* _s, fude_zoom_v2 _half, rde_color _paper) {
    _r->draws++;
    _r->strokes_drawn = _r->points_drawn = _r->frames_drawn = 0;
    rde_arr_clear(&_r->visible);
    const fude_zoom_camera* _cam     = &_s->camera;
    const fude_zoom_sim     _cam_sim = fude_zoom_camera_sim(_cam);

    // The highest frame drawn exactly (three up at most).
    u32           _top     = _cam->frame;
    fude_zoom_sim _top_sim = _cam_sim;
    for(u32 _l = 0; _l < 3u; _l++) {
        const u32 _up = fude_zoom_scene_frame(_s, _top)->parent;
        if(_up == FUDE_ZOOM_NONE) {
            break;
        }
        const fude_zoom_sim _up_sim = fude_zoom_sim_compose(_cam_sim, fude_zoom_scene_sim(_s, _up, _cam->frame));
        if(fude_zoom_render_error(_up_sim) >= FUDE_ZOOM_RENDER_EXACT_PT) {
            break;
        }
        _top     = _up;
        _top_sim = _up_sim;
    }

    // Above it, the backdrop: from the root down, what covers the camera's point.
    rde_color _back = _paper;
    b8        _has_over = false;
    rde_color _over_color = _paper;
    {
        u32 _chain[256];
        u32 _levels = 0;
        for(u32 _f = fude_zoom_scene_frame(_s, _top)->parent; _f != FUDE_ZOOM_NONE && _levels < 256u; _f = fude_zoom_scene_frame(_s, _f)->parent) {
            _chain[_levels++] = _f;
        }
        u32 _below = _top;   // the frame of the path one level down
        for(i32 _l = (i32)_levels - 1; _l >= 0; _l--) {
            const u32 _f    = _chain[_l];
            // The child on the path: from the top, the next one down.
            _below = _l > 0 ? _chain[_l - 1] : _top;
            const fude_zoom_frame* _child = fude_zoom_scene_frame(_s, _below);
            const u64 _slot = _child->object != FUDE_ZOOM_NONE ? fude_zoom_scene_object(_s, _child->object)->z : 0u;
            const fude_zoom_v2 _p = fude_zoom_sim_apply(fude_zoom_scene_sim(_s, _cam->frame, _f), _cam->at);
            u32 _under = FUDE_ZOOM_NONE, _over = FUDE_ZOOM_NONE;
            fude_zoom_render_cover(_r, _s, _f, _p, _slot, &_under, &_over);
            if(_over != FUDE_ZOOM_NONE && !_has_over) {
                _has_over   = true;
                _over_color = fude_zoom_render_cover_color(fude_zoom_scene_object(_s, _over));
            }
            if(_under != FUDE_ZOOM_NONE) {
                _back = fude_zoom_render_cover_color(fude_zoom_scene_object(_s, _under));
            }
        }
    }
    if(memcmp(&_back, &_paper, sizeof(_back)) != 0) {
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ (f32)(_half.x * 2.0 + 4.0), (f32)(_half.y * 2.0 + 4.0) }, _back);
    }

    // The top frame, with the frames beside it in its parent (a cousin's
    // drawing can reach the screen too), in that parent's order.
    const u32 _parent = fude_zoom_scene_frame(_s, _top)->parent;
    if(_parent == FUDE_ZOOM_NONE) {
        fude_zoom_render_frame(_r, _s, _top, _top_sim, _half, 0u);
    } else {
        const fude_zoom_frame* _p = fude_zoom_scene_frame(_s, _parent);
        const u32 _n = (u32)rde_arr_length(&_p->kids);
        rde_memory_allocator*  _heap  = rde_memory_allocator_get_default_std();
        fude_zoom_render_item* _items = _heap->malloc(_heap->allocator, (usize)(_n > 0 ? _n : 1u) * sizeof(fude_zoom_render_item));
        for(u32 _i = 0; _i < _n; _i++) {
            const u32 _k = ((const u32*)_p->kids.memory)[_i];
            _items[_i] = (fude_zoom_render_item){ fude_zoom_scene_object(_s, _k)->z, _k };
        }
        qsort(_items, _n, sizeof(_items[0]), fude_zoom_render_by_z);
        const fude_zoom_box _screen = { -_half.x, -_half.y, _half.x, _half.y };
        for(u32 _i = 0; _i < _n; _i++) {
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, _items[_i].object);
            if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
                continue;
            }
            const u32           _kid     = _o->child;
            const fude_zoom_sim _kid_sim = _kid == _top ? _top_sim : fude_zoom_sim_compose(_top_sim, fude_zoom_scene_sim(_s, _kid, _top));
            const fude_zoom_frame* _kf   = fude_zoom_scene_frame(_s, _kid);
            const fude_zoom_box _in_kid  = fude_zoom_sim_box(fude_zoom_sim_inverse(fude_zoom_sim_from_xform(_kf->xf)), _kf->anchor);
            if(_kid == _top || fude_zoom_box_overlaps(fude_zoom_sim_box(_kid_sim, _in_kid), _screen)) {
                fude_zoom_render_frame(_r, _s, _kid, _kid_sim, _half, 0u);
            }
        }
        _heap->free(_heap->allocator, _items);
    }

    if(_has_over) {
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ (f32)(_half.x * 2.0 + 4.0), (f32)(_half.y * 2.0 + 4.0) }, _over_color);
    }
}

void fude_zoom_render_object(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if(_o->kind == FUDE_ZOOM_KIND_STROKE) {
        fude_zoom_render_stroke(_r, _s, _object, _to_screen, _half);
        return;
    }
    if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
        fude_zoom_render_shape(_r, _s, _object, _to_screen, _half, NULL, 0.0f);
        return;
    }
    if(_o->kind == FUDE_ZOOM_KIND_IMAGE) {
        fude_zoom_render_image(_r, _s, _object, _to_screen, _half, NULL, 0.0f);
        return;
    }
    if(_o->kind == FUDE_ZOOM_KIND_FILL) {
        fude_zoom_render_fill(_r, _s, _object, _to_screen, _half, NULL, 0.0f);
        return;
    }
    // A frame: everything in it, through its own place — not one of the frames
    // the pen can reach this draw (the list is as the canvas was drawn).
    const u32           _kept = (u32)rde_arr_length(&_r->visible);
    const fude_zoom_sim _c    = fude_zoom_sim_compose(_to_screen, fude_zoom_sim_from_xform(fude_zoom_scene_frame(_s, _o->child)->xf));
    const rde_arr*      _was  = _r->lifted;
    _r->lifted = NULL;
    fude_zoom_render_frame(_r, _s, _o->child, _c, _half, 1u);
    _r->lifted = _was;
    _r->visible.count = _kept;
}

void fude_zoom_render_glow(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half, f32 _extra, rde_color _color) {
    if(fude_zoom_scene_object(_s, _object)->kind == FUDE_ZOOM_KIND_SHAPE) {
        fude_zoom_render_shape(_r, _s, _object, _to_screen, _half, &_color, _extra);
        return;
    }
    if(fude_zoom_scene_object(_s, _object)->kind == FUDE_ZOOM_KIND_IMAGE) {
        fude_zoom_render_image(_r, _s, _object, _to_screen, _half, &_color, _extra);
        return;
    }
    if(fude_zoom_scene_object(_s, _object)->kind == FUDE_ZOOM_KIND_FILL) {
        fude_zoom_render_fill(_r, _s, _object, _to_screen, _half, &_color, _extra);
        return;
    }
    fude_zoom_render_stroke_as(_r, _s, _object, _to_screen, _half, &_color, _extra);
}

u32 fude_zoom_render_editable(const fude_zoom_renderer* _r, const fude_zoom_scene* _s, rde_arr* _out) {
    const u32 _depth = fude_zoom_scene_depth(_s, _s->camera.frame);
    u32       _n     = 0;
    const fude_zoom_visible* _v = (const fude_zoom_visible*)_r->visible.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_r->visible); _i++) {
        // The camera's depth and below; above it while its units are still a
        // few points on screen (what is seen there can be erased, lassoed,
        // filled — further up a stroke's width runs to thousands of points).
        if(_v[_i].depth >= _depth || fude_zoom_sim_scale(_v[_i].to_screen) <= FUDE_ZOOM_RENDER_EDIT_UP) {
            rde_arr_add(_out, (any)&_v[_i]);
            _n++;
        }
    }
    return _n;
}
