// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/render.h"
#include "zoom/fill.h"
#include "zoom/shape.h"
#include "zoom/sheet.h"
#include "zoom/symbol.h"
#include "zoom/circuit.h"
#include "zoom/plot.h"
#include "drawing/base/theme.h"
#include "drawing/widgets/draw.h"

#include <math.h>
#include <stdio.h>
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
    _r->mm_per_unit  = 1.0;
    _r->units        = (fude_zoom_units_style){ FUDE_ZOOM_UNIT_MM, 1u, 16u, false };
    _r->drawing      = FUDE_ZOOM_NONE;
    _r->pictures     = rde_arr_new(sizeof(fude_zoom_picture), _heap);
    _r->cache_budget = FUDE_ZOOM_RENDER_CACHE;
    _r->symbols      = rde_arr_new(sizeof(fude_zoom_symbol_geo), _heap);
    _r->symbol_of    = rde_hash_map_new(sizeof(u64), sizeof(u32), rde_hash_map_fn_u64_hash, rde_hash_map_fn_u64_cmp, NULL, _heap);
}

// The symbols' geometry let go (made again as they are drawn).
RDE_INTERNAL void fude_zoom_render_symbols_forget(fude_zoom_renderer* _r) {
    if(!rde_arr_is_inited(&_r->symbols)) {
        return;
    }
    fude_zoom_symbol_geo* _g = (fude_zoom_symbol_geo*)_r->symbols.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_r->symbols); _i++) {
        rde_arr_free(&_g[_i].points);
        rde_arr_free(&_g[_i].parts);
        rde_arr_free(&_g[_i].tris);
        rde_arr_free(&_g[_i].tri_range);
    }
    rde_arr_clear(&_r->symbols);
    rde_hash_map_clear(&_r->symbol_of);
    _r->symbol_points = 0;
}

// A decoded stroke's arrays let go.
RDE_INTERNAL void fude_zoom_render_let_go(fude_zoom_decoded* _d) {
    rde_arr* _arrays[] = { &_d->xy, &_d->radius, &_d->tris, &_d->rings, &_d->edges, &_d->edge_lines };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
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
    fude_zoom_decoded* _d = (fude_zoom_decoded*)_r->cache.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_r->cache); _i++) {
        fude_zoom_render_let_go(&_d[_i]);
    }
    rde_arr_clear(&_r->cache);
    rde_arr_clear(&_r->slot_of);
    _r->cache_bytes = 0;
    fude_zoom_render_symbols_forget(_r);
}

void fude_zoom_render_destroy(fude_zoom_renderer* _r) {
    if(rde_arr_is_inited(&_r->cache)) {
        fude_zoom_render_trim(_r);
    }
    rde_arr* _arrays[] = { &_r->visible, &_r->cache, &_r->slot_of, &_r->found, &_r->screen, &_r->radii, &_r->q, &_r->shape, &_r->pictures, &_r->fill, &_r->cut,
                           &_r->symbols };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    if(_r->symbol_of.capacity > 0u) {
        rde_hash_map_free(&_r->symbol_of);
    }
    memset(_r, 0, sizeof(*_r));
}

// How far off, in screen units, an f64 transform with this translation can be.
RDE_INTERNAL f64 fude_zoom_render_error(fude_zoom_sim _s) {
    return (fabs(_s.tx) + fabs(_s.ty)) * FUDE_ZOOM_RENDER_F64_EPS;
}

// --- decoded strokes ------------------------------------------------------------------------

RDE_INTERNAL void fude_zoom_render_evict(fude_zoom_renderer* _r, u32 _entry) {
    fude_zoom_decoded* _d    = (fude_zoom_decoded*)_r->cache.memory;
    u32*               _slot = (u32*)_r->slot_of.memory;
    _r->cache_bytes -= (u64)_d[_entry].count * 12u + (u64)_d[_entry].tri_count * 24u + (u64)_d[_entry].edge_count * 12u;
    fude_zoom_render_let_go(&_d[_entry]);
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
        .xy = rde_arr_new(sizeof(f32), _heap),
        .radius = rde_arr_new(sizeof(f32), _heap),
    };
    rde_arr_resize(&_e.xy, (usize)_o->count * 2u);
    rde_arr_resize(&_e.radius, _o->count);
    f32* _xy  = (f32*)_e.xy.memory;
    f32* _rad = (f32*)_e.radius.memory;
    const fude_zoom_qpoint* _q = (const fude_zoom_qpoint*)_r->q.memory;
    for(u32 _i = 0; _i < _o->count; _i++) {
        _xy[_i * 2u]      = (f32)ldexp((f64)_q[_i].x, _o->q);
        _xy[_i * 2u + 1u] = (f32)ldexp((f64)_q[_i].y, _o->q);
        _rad[_i]          = fude_zoom_scene_local_radius_at(_o, &_q[_i]);   // before the object's scale: that can change
    }
    if(_o->kind == FUDE_ZOOM_KIND_FILL && (_o->channels & FUDE_ZOOM_CHANNEL_TIME)) {
        _e.rings = rde_arr_new(sizeof(u32), _heap);
        rde_arr_resize(&_e.rings, _o->count);
        u32* _rings = (u32*)_e.rings.memory;
        for(u32 _i = 0; _i < _o->count; _i++) {
            _rings[_i] = _q[_i].time;
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
    const b8  _far   = _max_r >= FUDE_ZOOM_RENDER_FAR || fabs(_t.x) + _size >= FUDE_ZOOM_RENDER_FAR || fabs(_t.y) + _size >= FUDE_ZOOM_RENDER_FAR;

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
            _rad[_i] = fmax((f64)_dr[_i] * _wscale, (f64)FUDE_ZOOM_RENDER_MIN_RADIUS) + (f64)_extra;
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
                memcpy(rde_arr_add_n(&_r->shape, _k), _r->cut.memory, (usize)_k * sizeof(fude_zoom_v2));
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

void fude_zoom_render_dimension(const fude_zoom_renderer* _r, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _offset, f32 _width, rde_color _color, const c8* _label) {
    const f64 _len = hypot(_b.x - _a.x, _b.y - _a.y);
    if(_len < 1.0) {
        return;
    }
    const fude_zoom_v2 _d    = { (_b.x - _a.x) / _len, (_b.y - _a.y) / _len };
    const fude_zoom_v2 _left = { -_d.y, _d.x };
    const f64          _side = _offset >= 0.0 ? 1.0 : -1.0;
    const fude_zoom_v2 _a2   = { _a.x + _left.x * _offset, _a.y + _left.y * _offset };
    const fude_zoom_v2 _b2   = { _b.x + _left.x * _offset, _b.y + _left.y * _offset };
    // The extension lines: from just off what it measures to just past its line.
    if(fabs(_offset) > 3.0) {
        const f64 _gap = 3.0 * _side, _over = _offset + 5.0 * _side;
        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)(_a.x + _left.x * _gap), (f32)(_a.y + _left.y * _gap) }, (rde_vec_2F){ (f32)(_a.x + _left.x * _over), (f32)(_a.y + _left.y * _over) }, _color, 1.0f);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)(_b.x + _left.x * _gap), (f32)(_b.y + _left.y * _gap) }, (rde_vec_2F){ (f32)(_b.x + _left.x * _over), (f32)(_b.y + _left.y * _over) }, _color, 1.0f);
    }
    rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a2.x, (f32)_a2.y }, (rde_vec_2F){ (f32)_b2.x, (f32)_b2.y }, _color, fmaxf(_width, 1.0f));
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
    const u32 _k = _count >= 1u ? (u32)_n[0] : 0u;
    if(_k < 2u || _k > FUDE_ZOOM_ARROW_POINTS || _count < 2u * _k + 2u) {
        return;
    }
    const u32           _heads = (u32)_n[1u + 2u * _k];
    const fude_zoom_sim _all   = fude_zoom_sim_compose(_to_screen, fude_zoom_object_sim(_o));
    const f64           _w     = fmax((f64)_o->radius * fude_zoom_sim_scale(_to_screen), 0.75);   // (its own scale is a connector's stretch: not its width)
    const rde_color     _c     = _as != NULL ? *_as : fude_theme_resolve(_o->color);
    rde_vec_2F _p[FUDE_ZOOM_ARROW_POINTS];
    for(u32 _i = 0; _i < _k; _i++) {
        const fude_zoom_v2 _q = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _n[1u + 2u * _i], _n[2u + 2u * _i] });
        _p[_i] = (rde_vec_2F){ (f32)_q.x, (f32)_q.y };
    }
    const f32 _head = (f32)fmax(_w * 3.5 + 5.0, 8.0);
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
        const f32 _dx = _b.x - _a.x, _dy = _b.y - _a.y, _len = sqrtf(_dx * _dx + _dy * _dy);
        if(_len < 0.5f) {
            continue;
        }
        // The line stops short of a head that would show its end through it (a pointed one, a hollow one).
        if(_i + 2u == _k && _trim[0] > 0.0f) {
            const f32 _t = fminf(_trim[0], _len);
            _b = (rde_vec_2F){ _b.x - _dx / _len * _t, _b.y - _dy / _len * _t };
        }
        if(_i == 0u && _trim[1] > 0.0f) {
            const f32 _t = fminf(_trim[1], _len);
            _a = (rde_vec_2F){ _a.x + _dx / _len * _t, _a.y + _dy / _len * _t };
        }
        if(_heads & FUDE_ZOOM_ARROW_DOTTED) {
            const f32 _dash = (f32)fmax(_w * 3.0, 4.0);
            const f32 _seg  = sqrtf((_b.x - _a.x) * (_b.x - _a.x) + (_b.y - _a.y) * (_b.y - _a.y));
            for(f32 _t = 0.0f; _t < _seg; _t += _dash * 2.0f) {
                const f32 _t1 = fminf(_t + _dash, _seg);
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
                    rde_rendering_2d_draw_line_1(_poly[_j], _poly[(_j + 1u) % _pm], _c, (f32)fmax(_w * 2.0, 1.0));
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
    const f64 _mm   = hypot(_n[0], _n[1]) * _o->scale * fude_zoom_sim_scale(fude_zoom_scene_sim(_s, _o->frame, _home)) * _r->mm_per_unit;
    c8 _label[FUDE_ZOOM_UNITS_TEXT];
    fude_zoom_units_format(_mm, &_r->units, _label, sizeof(_label));
    const rde_color _color = _as != NULL ? *_as : fude_theme_resolve(_o->color);
    fude_zoom_render_dimension(_r, _a, _b, _n[2] * _k, (f32)fmax((f64)_o->radius * _k * 2.0, 1.0), _color, _label);
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
    const fude_zoom_v2 _u = { cos(_n[1]) * _n[0], sin(_n[1]) * _n[0] };
    const fude_zoom_v2 _c = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 0.0 });
    const fude_zoom_v2 _e = fude_zoom_sim_apply(_all, _u);
    const fude_zoom_v2 _f = _across ? fude_zoom_sim_apply(_all, (fude_zoom_v2){ -_u.x, -_u.y }) : _c;
    const f64 _len = hypot(_e.x - _c.x, _e.y - _c.y);
    if(_len < 2.0) {
        return;
    }
    const rde_color _color = _as != NULL ? *_as : fude_theme_resolve(_o->color);
    const fude_zoom_v2 _d = { (_e.x - _c.x) / _len, (_e.y - _c.y) / _len };
    rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_f.x, (f32)_f.y }, (rde_vec_2F){ (f32)_e.x, (f32)_e.y }, _color, (f32)fmax((f64)_o->radius * fude_zoom_sim_scale(_all) * 2.0, 1.0));
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
    snprintf(_label, sizeof(_label), _across ? "\xC3\x98 %s" : "R %s", _v);
    const f32 _tw = fude_draw_text_width(_r->font, _r->font_px, _label, 13.0f);
    const f64 _out = (f64)_tw * 0.5 * fabs(_d.x) + 9.0 * fabs(_d.y) + 10.0;
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
    const f64 _turn = atan2(_all.b, _all.a);
    const f64 _from = _n[1] + _turn, _sweep = _n[2];
    const u32 _steps = (u32)fmin(fmax(fabs(_sweep) * _rad / 4.0, 4.0), 256.0);
    rde_vec_2F _prev = { (f32)(_c.x + cos(_from) * _rad), (f32)(_c.y + sin(_from) * _rad) };
    for(u32 _i = 1; _i <= _steps; _i++) {
        const f64 _a = _from + _sweep * (f64)_i / (f64)_steps;
        const rde_vec_2F _p = { (f32)(_c.x + cos(_a) * _rad), (f32)(_c.y + sin(_a) * _rad) };
        rde_rendering_2d_draw_line_1(_prev, _p, _color, 1.0f);
        _prev = _p;
    }
    // Its heads, along the arc at each end (inwards, round it).
    const f64 _sg = _sweep >= 0.0 ? 1.0 : -1.0;
    if(fabs(_sweep) * _rad >= 22.0) {
        const f64 _a0 = _from, _a1 = _from + _sweep;
        fude_zoom_render_head((fude_zoom_v2){ _c.x + cos(_a0) * _rad, _c.y + sin(_a0) * _rad }, (fude_zoom_v2){ sin(_a0) * _sg, -cos(_a0) * _sg }, _color);
        fude_zoom_render_head((fude_zoom_v2){ _c.x + cos(_a1) * _rad, _c.y + sin(_a1) * _rad }, (fude_zoom_v2){ -sin(_a1) * _sg, cos(_a1) * _sg }, _color);
    }
    if(_as != NULL) {
        return;
    }
    c8 _label[FUDE_ZOOM_UNITS_TEXT];
    fude_zoom_units_format_angle(fabs(_sweep) * 180.0 / 3.14159265358979323846, 1u, _label, sizeof(_label));
    const f64 _mid = _from + _sweep * 0.5;
    const f64 _out = _rad + 16.0;
    fude_zoom_render_tag(_r, (rde_vec_2F){ (f32)(_c.x + cos(_mid) * _out), (f32)(_c.y + sin(_mid) * _out) }, _label, _color);
}

// A text: its words in the app's font at its size on the screen, upright (the
// font is drawn from curves, which do not turn); a note on its own paper. Too
// small to read, a note is still its paper and a text a faint bar a line.
RDE_INTERNAL void fude_zoom_render_text(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, const rde_color* _as) {
    f64 _size = 0.0, _w = 0.0, _h = 0.0;
    u8  _style = 0;
    const c8* _text = NULL;
    u32 _len = 0;
    if(!fude_zoom_scene_text(_s, _object, &_size, &_w, &_h, &_style, &_text, &_len)) {
        return;
    }
    const fude_zoom_object* _o   = fude_zoom_scene_object(_s, _object);
    const fude_zoom_sim     _all = fude_zoom_sim_compose(_to_screen, fude_zoom_object_sim(_o));
    const f64               _k   = fude_zoom_sim_scale(_all);
    const fude_zoom_v2      _tl  = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 0.0 });
    const f64 _bw = _w * _k, _bh = _h * _k, _px = _size * _k;
    if(_bw < 0.5 || _bh < 0.5 || _bw > 1e6 || _bh > 1e6) {
        return;
    }
    const rde_color _ink = _as != NULL ? *_as : fude_theme_resolve(_o->color);
    if(_style == FUDE_ZOOM_TEXT_STICKY) {
        const rde_vec_2F _c = { (f32)(_tl.x + _bw * 0.5), (f32)(_tl.y - _bh * 0.5) };
        rde_color _shadow = { 0, 0, 0, 40 };
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ _c.x + (f32)(_px * 0.12), _c.y - (f32)(_px * 0.16) }, (rde_vec_2F){ (f32)_bw, (f32)_bh }, _shadow);
        rde_rendering_2d_draw_rectangle(_c, (rde_vec_2F){ (f32)_bw, (f32)_bh }, _as != NULL ? *_as : _o->fill);
    }
    const f64 _pad  = _style == FUDE_ZOOM_TEXT_STICKY ? _size * 0.6 * _k : 0.0;
    const f64 _line = _px * 1.3;
    if(_px < 3.0 || _r->font == NULL) {
        // Too small to read: a faint bar for each line it fills.
        rde_color _faint = _ink;
        _faint.a = (u8)(_faint.a / 3u);
        const u32 _lines = (u32)fmax(1.0, floor((_bh - 2.0 * _pad) / fmax(_line, 1e-9)));
        for(u32 _i = 0; _i < _lines && _i < 64u; _i++) {
            const f64 _y = _tl.y - _pad - _line * ((f64)_i + 0.5);
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ (f32)(_tl.x + _bw * 0.5), (f32)_y }, (rde_vec_2F){ (f32)fmax(_bw - 2.0 * _pad, 0.5), (f32)fmax(_px * 0.5, 0.5) }, _faint);
        }
        return;
    }
    if(_px > 3000.0) {
        return;   // a few letters filling far more than the screen: nothing readable to draw
    }
    c8      _small[512];
    c8*     _words = _small;
    rde_arr _long  = { 0 };   // (a longer one's)
    if(_len >= sizeof(_small)) {
        _long = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
        rde_arr_resize(&_long, (usize)_len + 1u);
        _words = (c8*)_long.memory;
    }
    memcpy(_words, _text, _len);
    _words[_len] = 0;
    fude_draw_text_wrap(_r->font, _r->font_px, _words, (f32)(_tl.x + _pad), (f32)(_tl.y - _pad - _px * 0.95), (f32)_px, (f32)fmax(_bw - 2.0 * _pad, 1.0), (f32)_line, _ink);
    if(_words != _small) {
        rde_arr_free(&_long);
    }
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
    const f64 _len = hypot(_b.x - _a.x, _b.y - _a.y);
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
        snprintf(_size, sizeof(_size), "%s \xC3\x97 %s \xC3\x97 %s \xC2\xB7 %s", _ls, _ws, _ts, _r->material_words[_material]);
    } else {
        snprintf(_size, sizeof(_size), "%s \xC3\x97 %s \xC3\x97 %s", _ls, _ws, _ts);
    }
    const fude_zoom_v2 _c  = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 0.0 });
    const f32 _px = 13.0f;
    const f32 _sw = fude_draw_text_width(_r->font, _r->font_px, _size, _px);
    const f32 _nw = _name[0] != 0 ? fude_draw_text_width(_r->font, _r->font_px, _name, _px + 2.0f) : 0.0f;
    if((f64)fmaxf(_sw, _nw) > 2.0 * fmax(_l, _w) * 0.9 || 2.0 * fmin(_l, _w) < (_name[0] != 0 ? 40.0 : 22.0)) {
        return;   // too small to say it in
    }
    if(_name[0] != 0) {
        rde_rendering_2d_draw_text_2(_r->font, _name, (rde_vec_3F){ (f32)_c.x - _nw * 0.5f, (f32)_c.y + 3.0f, 0.0f },
                                     (rde_vec_2F){ (_px + 2.0f) / _r->font_px, (_px + 2.0f) / _r->font_px }, 0.0f, _color);
    }
    rde_rendering_2d_draw_text_2(_r->font, _size, (rde_vec_3F){ (f32)_c.x - _sw * 0.5f, (f32)_c.y - (_name[0] != 0 ? _px + 2.0f : _px * 0.36f), 0.0f },
                                 (rde_vec_2F){ _px / _r->font_px, _px / _r->font_px }, 0.0f, _color);
}

// A board cut (cut.h): its material filled (its holes and notches left out),
// each of its rings drawn as its line, its marks over it.
RDE_INTERNAL void fude_zoom_render_cut_board(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half,
                                             const rde_color* _as, f32 _extra) {
    const fude_zoom_object* _o    = fude_zoom_scene_object(_s, _object);
    rde_memory_allocator*   _heap = rde_memory_allocator_get_default_std();
    rde_arr _num = rde_arr_new(sizeof(f64), _heap), _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap), _rings = rde_arr_new(sizeof(u32), _heap);
    const u32 _count = fude_zoom_scene_shape_numbers_all(_s, _object, &_num);
    const u32 _n     = fude_zoom_board_rings((const f64*)_num.memory, _count, &_pts, &_rings);
    const fude_zoom_sim _all    = fude_zoom_sim_compose(_to_screen, fude_zoom_object_sim(_o));
    const rde_color     _color  = _as != NULL ? *_as : fude_theme_resolve(_o->color);
    const f64           _radius = fmax((f64)_o->radius * fude_zoom_sim_scale(_all), (f64)FUDE_ZOOM_RENDER_MIN_RADIUS) + (f64)_extra;
    rde_arr _sp_arr = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr_resize(&_sp_arr, (usize)_n + 1u);
    fude_zoom_v2* _sp = (fude_zoom_v2*)_sp_arr.memory;   // (sized once: it stays put)
    b8 _far = _radius >= FUDE_ZOOM_RENDER_FAR;
    for(u32 _i = 0; _i < _n; _i++) {
        _sp[_i] = fude_zoom_sim_apply(_all, ((const fude_zoom_v2*)_pts.memory)[_i]);
        _far = _far || fabs(_sp[_i].x) >= FUDE_ZOOM_RENDER_FAR || fabs(_sp[_i].y) >= FUDE_ZOOM_RENDER_FAR;
    }
    // Its fill: the material only (each ring cut to round the screen first when it is far bigger than it).
    const b8 _filled = (_o->flags & FUDE_ZOOM_FLAG_FILLED) && _as == NULL;
    const rde_color _fill = (_o->flags & FUDE_ZOOM_FLAG_FILL_OWN) ? fude_theme_resolve(_o->fill) : _color;
    const fude_zoom_v2 _guard = { _half.x * 2.0, _half.y * 2.0 };
    if(_filled && _n >= 3u) {
        rde_arr _clip = rde_arr_new(sizeof(fude_zoom_v2), _heap), _clip_rings = rde_arr_new(sizeof(u32), _heap), _cut = rde_arr_new(sizeof(fude_zoom_v2), _heap);
        for(u32 _from = 0; _from < _n;) {
            u32 _to = _from;
            while(_to < _n && ((const u32*)_rings.memory)[_to] == ((const u32*)_rings.memory)[_from]) {
                _to++;
            }
            const u32 _ring = ((const u32*)_rings.memory)[_from];
            if(_far) {
                const u32 _k = fude_zoom_fill_clip(&_sp[_from], _to - _from, (fude_zoom_box){ -_guard.x, -_guard.y, _guard.x, _guard.y }, &_cut);
                for(u32 _i = 0; _i < _k; _i++) {
                    rde_arr_add(&_clip, (any)&((const fude_zoom_v2*)_cut.memory)[_i]);
                    rde_arr_add(&_clip_rings, (any)&_ring);
                }
            } else {
                for(u32 _i = _from; _i < _to; _i++) {
                    rde_arr_add(&_clip, (any)&_sp[_i]);
                    rde_arr_add(&_clip_rings, (any)&_ring);
                }
            }
            _from = _to;
        }
        rde_arr_clear(&_r->fill);
        const u32 _t = fude_zoom_fill_triangulate_rings((const fude_zoom_v2*)_clip.memory, (const u32*)_clip_rings.memory, (u32)rde_arr_length(&_clip), &_r->fill);
        fude_zoom_render_triangles((const fude_zoom_v2*)_r->fill.memory, _t, _fill);
        rde_arr_free(&_clip); rde_arr_free(&_clip_rings); rde_arr_free(&_cut);
    }
    // Each ring's line.
    rde_arr _ring_arr = rde_arr_new(sizeof(fude_zoom_v2), _heap), _rad_arr = rde_arr_new(sizeof(f64), _heap);
    for(u32 _from = 0; _from < _n;) {
        u32 _to = _from;
        while(_to < _n && ((const u32*)_rings.memory)[_to] == ((const u32*)_rings.memory)[_from]) {
            _to++;
        }
        const u32 _k = _to - _from;
        if(_k >= 2u) {
            rde_arr_resize(&_ring_arr, (usize)_k + 1u);
            rde_arr_resize(&_rad_arr, (usize)_k + 1u);
            fude_zoom_v2* _ring = (fude_zoom_v2*)_ring_arr.memory;
            f64*          _rad  = (f64*)_rad_arr.memory;
            memcpy(_ring, &_sp[_from], (usize)_k * sizeof(fude_zoom_v2));
            _ring[_k] = _ring[0];
            for(u32 _i = 0; _i <= _k; _i++) {
                _rad[_i] = _radius;
            }
            if(_far) {
                fude_zoom_draw_huge_stroke(_ring, _rad, _k + 1u, _guard, _color);
            } else {
                rde_arr_clear(&_r->screen);
                rde_arr_clear(&_r->radii);
                rde_vec_2F* _q  = rde_arr_add_n(&_r->screen, _k + 1u);
                f32*        _rr = rde_arr_add_n(&_r->radii, _k + 1u);
                for(u32 _i = 0; _i <= _k; _i++) {
                    _q[_i]  = (rde_vec_2F){ (f32)_ring[_i].x, (f32)_ring[_i].y };
                    _rr[_i] = (f32)_radius;
                }
                rde_rendering_2d_draw_stroke(_q, _rr, _k + 1u, _color);
            }
        }
        _from = _to;
    }
    rde_arr_free(&_ring_arr);
    rde_arr_free(&_rad_arr);
    if(_as == NULL) {
        fude_zoom_render_board_marks(_r, _s, _o, (const f64*)_num.memory, _count, _all, fude_zoom_render_marks_on(_filled, _fill, _color));
    }
    rde_arr_free(&_sp_arr);
    rde_arr_free(&_num);
    rde_arr_free(&_pts);
    rde_arr_free(&_rings);
    _r->strokes_drawn++;
    _r->points_drawn += _n;
}

// Text in a box on the screen (centre origin, Y up): its lines broken to fit its width, each centred,
// the whole in the box's middle (_top: from its top instead) — never cut, past the box if it must.
void fude_zoom_render_text_in(const fude_zoom_renderer* _r, const c8* _text, u32 _len, fude_zoom_box _box, f64 _px, b8 _top, rde_color _color) {
    fude_zoom_render_text_at(_r, _text, _len, _box, _px, _top, false, _color);
}

void fude_zoom_render_text_at(const fude_zoom_renderer* _r, const c8* _text, u32 _len, fude_zoom_box _box, f64 _px, b8 _top, b8 _left, rde_color _color) {
    if(_r->font == NULL || _len == 0u || !(_px >= 3.0)) {
        return;
    }
    c8      _small[512];
    c8*     _words = _small;
    rde_arr _long  = { 0 };   // (a longer one's)
    if(_len >= sizeof(_small)) {
        _long = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
        rde_arr_resize(&_long, (usize)_len + 1u);
        _words = (c8*)_long.memory;
    }
    memcpy(_words, _text, _len);
    _words[_len] = 0;
    u32 _from[48], _to[48];
    const f32 _width = (f32)fmax(_box.max_x - _box.min_x, _px);
    // (a word wider than the box: the letters smaller, to two thirds — never a word cut in two)
    {
        f32 _widest = 0.0f;
        c8  _word[128];
        for(u32 _i = 0; _i < _len;) {
            while(_i < _len && (_words[_i] == ' ' || _words[_i] == '\n')) {
                _i++;
            }
            u32 _j = _i;
            while(_j < _len && _words[_j] != ' ' && _words[_j] != '\n') {
                _j++;
            }
            if(_j > _i && _j - _i < sizeof(_word)) {
                memcpy(_word, &_words[_i], _j - _i);
                _word[_j - _i] = 0;
                _widest = fmaxf(_widest, fude_draw_text_width(_r->font, _r->font_px, _word, (f32)_px));
            }
            _i = _j;
        }
        if(_widest > _width) {
            _px = fmax(_px * (f64)(_width / _widest), _px * 0.66);
        }
    }
    const u32 _lines = fude_draw_text_wrap_spans(_r->font, _r->font_px, _words, (f32)_px, _width, _from, _to, 48u);
    const f64 _step  = _px * 1.25;
    const f64 _cx    = (_box.min_x + _box.max_x) * 0.5;
    f64       _y     = _top ? _box.max_y - _px * 1.05 : (_box.min_y + _box.max_y) * 0.5 + _step * (f64)_lines * 0.5 - _px * 1.0;
    c8 _line[512];
    for(u32 _i = 0; _i < _lines; _i++, _y -= _step) {
        const u32 _n = _to[_i] - _from[_i] < sizeof(_line) - 1u ? _to[_i] - _from[_i] : (u32)sizeof(_line) - 1u;
        memcpy(_line, &_words[_from[_i]], _n);
        _line[_n] = 0;
        const f32 _w = fude_draw_text_width(_r->font, _r->font_px, _line, (f32)_px);
        rde_rendering_2d_draw_text_2(_r->font, _line, (rde_vec_3F){ _left ? (f32)_box.min_x : (f32)(_cx - _w * 0.5), (f32)_y, 0.0f },
                                     (rde_vec_2F){ (f32)_px / _r->font_px, (f32)_px / _r->font_px }, 0.0f, _color);
    }
    if(_words != _small) {
        rde_arr_free(&_long);
    }
}

// A polyline on the screen dashed: _on long, _off apart.
RDE_INTERNAL void fude_zoom_render_dashed(const fude_zoom_v2* _p, u32 _n, b8 _closed, f32 _radius, f64 _on, f64 _off, rde_color _color) {
    const u32 _m = _closed ? _n + 1u : _n;
    f64 _left = _on;
    b8  _draw = true;
    for(u32 _i = 1; _i < _m; _i++) {
        fude_zoom_v2 _a = _p[_i - 1u];
        const fude_zoom_v2 _b = _p[_i % _n];
        f64 _seg = hypot(_b.x - _a.x, _b.y - _a.y);
        while(_seg > 1e-9) {
            const f64 _t = fmin(_left, _seg);
            const fude_zoom_v2 _c = { _a.x + (_b.x - _a.x) * _t / _seg, _a.y + (_b.y - _a.y) * _t / _seg };
            if(_draw) {
                rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_c.x, (f32)_c.y }, _color, (f32)fmax(_radius * 2.0f, 1.0f));
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

// A symbol (symbol.h): its parts — filled as the shape is, or solid in its line's colour — and their lines
// (dashed where they are), its compartments' lines, and its text in its boxes.
#define FUDE_ZOOM_RENDER_SYMBOLS       2048u      // symbols' geometry kept, at most (all let go, then made again as drawn)
#define FUDE_ZOOM_RENDER_SYMBOL_POINTS (1u << 21)

// Symbol _kind's geometry at half sizes _hw × _hh and _segments (a step of 8: what zooming changes little, kept), or
// custom part _custom's: its parts' points (its own units), its parts, each filled part's triangles. Made once, kept.
RDE_INTERNAL const fude_zoom_symbol_geo* fude_zoom_render_symbol_geo(fude_zoom_renderer* _r, u32 _kind, f64 _hw, f64 _hh, u32 _segments,
                                                                     const fude_zoom_part* _custom) {
    _segments = (_segments + 7u) / 8u * 8u;
    u64 _key = 1469598103934665603ull;
    const u64 _hw_bits = 0, _hh_bits = 0;
    u64 _words[6] = { _kind, _segments, _hw_bits, _hh_bits, (u64)(uintptr_t)_custom, 0 };
    memcpy(&_words[2], &_hw, sizeof(f64));
    memcpy(&_words[3], &_hh, sizeof(f64));
    for(u32 _i = 0; _i < 5u; _i++) {
        _key = (_key ^ _words[_i]) * 1099511628211ull;
        _key ^= _key >> 29;
    }
    const u32* _at = (const u32*)rde_hash_map_find(&_r->symbol_of, &_key);
    if(_at != NULL && *_at < (u32)rde_arr_length(&_r->symbols)) {
        const fude_zoom_symbol_geo* _g = &((const fude_zoom_symbol_geo*)_r->symbols.memory)[*_at];
        if(_g->kind == _kind && _g->segments == _segments && _g->hw == _hw && _g->hh == _hh && _g->custom == (const void*)_custom) {
            return _g;
        }
    }
    if(rde_arr_length(&_r->symbols) >= FUDE_ZOOM_RENDER_SYMBOLS || _r->symbol_points >= FUDE_ZOOM_RENDER_SYMBOL_POINTS) {
        fude_zoom_render_symbols_forget(_r);   // (too many kept: all made again as they are drawn)
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_symbol_geo _g;
    memset(&_g, 0, sizeof(_g));
    _g.kind = _kind; _g.segments = _segments; _g.hw = _hw; _g.hh = _hh; _g.custom = _custom;
    _g.points    = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    _g.parts     = rde_arr_new(sizeof(fude_zoom_symbol_part), _heap);
    _g.tris      = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    _g.tri_range = rde_arr_new(sizeof(u32), _heap);
    _g.parts_n = _custom != NULL ? fude_zoom_part_draw(_custom, _hw, _hh, _segments, &_g.points, &_g.parts)
                                 : fude_zoom_symbol_parts(_kind, _hw, _hh, _segments, &_g.points, &_g.parts);
    // Each part that can be filled: its rings (itself, then the parts joined to it — its holes) as triangles.
    const fude_zoom_v2* _p = (const fude_zoom_v2*)_g.points.memory;
    const fude_zoom_symbol_part* _pa = (const fude_zoom_symbol_part*)_g.parts.memory;
    rde_arr _rings = rde_arr_new(sizeof(u32), _heap), _ring_pts = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    for(u32 _i = 0; _i < _g.parts_n; _i++) {
        const u8 _fl = _pa[_i].flags;
        u32 _range[2] = { (u32)(rde_arr_length(&_g.tris) / 3u), 0u };
        if((_fl & (FUDE_ZOOM_SYMBOL_SOLID | FUDE_ZOOM_SYMBOL_TINTED | FUDE_ZOOM_SYMBOL_FILLED)) && _pa[_i].count >= 3u && !(_fl & FUDE_ZOOM_SYMBOL_JOIN)) {
            rde_arr_clear(&_rings);
            rde_arr_clear(&_ring_pts);
            u32 _ring = 0;
            for(u32 _j = _i; _j < _g.parts_n && (_j == _i || (_pa[_j].flags & FUDE_ZOOM_SYMBOL_JOIN)); _j++, _ring++) {
                for(u32 _t = 0; _t < _pa[_j].count; _t++) {
                    rde_arr_add(&_rings, (any)&_ring);
                    rde_arr_add(&_ring_pts, (any)&_p[_pa[_j].first + _t]);
                }
            }
            rde_arr_clear(&_r->fill);
            _range[1] = fude_zoom_fill_triangulate_rings((const fude_zoom_v2*)_ring_pts.memory, (const u32*)_rings.memory, (u32)rde_arr_length(&_ring_pts), &_r->fill);
            if(_range[1] > 0u) {
                memcpy(rde_arr_add_n(&_g.tris, (usize)_range[1] * 3u), _r->fill.memory, (usize)_range[1] * 3u * sizeof(fude_zoom_v2));
            }
        }
        memcpy(rde_arr_add_n(&_g.tri_range, 2u), _range, sizeof(_range));
    }
    rde_arr_free(&_rings);
    rde_arr_free(&_ring_pts);
    _r->symbol_points += rde_arr_length(&_g.points) + rde_arr_length(&_g.tris);
    rde_arr_add(&_r->symbols, (any)&_g);
    const u32 _place = (u32)rde_arr_length(&_r->symbols) - 1u;
    if(!rde_hash_map_update_entry(&_r->symbol_of, &_key, &_place)) {
        rde_hash_map_add(&_r->symbol_of, &_key, &_place);
    }
    return &((const fude_zoom_symbol_geo*)_r->symbols.memory)[_place];
}

RDE_INTERNAL void fude_zoom_render_symbol(fude_zoom_renderer* _r, const fude_zoom_object* _o, const f64* _n, u32 _count, fude_zoom_sim _to_screen,
                                          fude_zoom_v2 _half, const rde_color* _as, f32 _extra) {
    RDE_UNUSED(_half);
    if(_count < 4u) {
        return;
    }
    const u32 _kind = (u32)_n[0];
    const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of(_kind);
    if(_info == NULL) {
        return;
    }
    const fude_zoom_sim _all    = fude_zoom_sim_compose(_to_screen, fude_zoom_object_sim(_o));
    const f64           _k      = fude_zoom_sim_scale(_all);
    const rde_color     _color  = _as != NULL ? *_as : fude_theme_resolve(_o->color);
    const f64           _radius = fmax((f64)_o->radius * _k, (f64)FUDE_ZOOM_RENDER_MIN_RADIUS) + (f64)_extra;
    const b8            _filled = (_o->flags & FUDE_ZOOM_FLAG_FILLED) && _as == NULL;
    const rde_color     _fill   = (_o->flags & FUDE_ZOOM_FLAG_FILL_OWN) ? fude_theme_resolve(_o->fill) : _color;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    // Its geometry, made once for its kind, size and detail (a custom part: drawn as its definition's pins are; a sized
    // display, as big as its text says), into the screen.
    const fude_zoom_part* _custom = fude_zoom_part_of_numbers(_n, _count);
    const fude_zoom_symbol_geo* _geo = fude_zoom_render_symbol_geo(_r, _kind, _n[1], _n[2], fude_zoom_shape_segments(fmax(_n[1], _n[2]) * 2.0 * _k),
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
    for(u32 _i = 0; _i < _np; _i++) {
        const fude_zoom_v2* _q = &_p[_pa[_i].first];
        const u32           _m = _pa[_i].count;
        const u8            _fl = _pa[_i].flags;
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
                rde_rendering_2d_draw_triangle((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, (rde_vec_2F){ (f32)_c.x, (f32)_c.y }, _fc, NULL);
            }
        }
        if(_fl & FUDE_ZOOM_SYMBOL_NO_LINE) {
            continue;
        }
        const rde_color _lc = _pa[_i].line.a > 0u && _as == NULL ? _pa[_i].line : _color;
        const f64 _lr = _pa[_i].width > 0.0f ? fmax((f64)_pa[_i].width * 0.5 * _k, (f64)FUDE_ZOOM_RENDER_MIN_RADIUS) + (f64)_extra : _radius;
        const b8 _closed = (_fl & FUDE_ZOOM_SYMBOL_CLOSED) != 0u;
        if(_fl & FUDE_ZOOM_SYMBOL_DASHED) {
            fude_zoom_render_dashed(_q, _m, _closed, (f32)_lr, fmax(_lr * 4.0, 6.0), fmax(_lr * 3.0, 4.0), _lc);
            continue;
        }
        const u32 _w = _closed ? _m + 1u : _m;
        if(_w == 0u) {
            continue;   // (no points: nothing to draw)
        }
        // (its points on the screen, those nearer than a pixel to the one before left out: a gear's teeth far out are its
        // rim, not hundreds of points in a few pixels; its last kept, however near)
        rde_arr_clear(&_r->screen);
        rde_arr_clear(&_r->radii);
        rde_vec_2F* _sp = rde_arr_add_n(&_r->screen, _w);
        f32*        _sr = rde_arr_add_n(&_r->radii, _w);
        u32 _kept = 0;
        for(u32 _j = 0; _j < _w; _j++) {
            const rde_vec_2F _v = { (f32)_q[_j % _m].x, (f32)_q[_j % _m].y };
            if(_kept > 0u && _j + 1u < _w && fabsf(_v.x - _sp[_kept - 1u].x) < 1.0f && fabsf(_v.y - _sp[_kept - 1u].y) < 1.0f) {
                continue;
            }
            _sp[_kept]   = _v;
            _sr[_kept++] = (f32)_lr;
        }
        if(_kept >= 2u) {
            rde_rendering_2d_draw_stroke(_sp, _sr, _kept, _lc);
        }
    }
    // Maths (plot.h): a graph's, axes', a number line's lines from its text — its grid faint, its curves each its colour —
    // and its ticks' numbers.
    if(_as == NULL && fude_zoom_plot_is(_info->id)) {
        c8 _ptext[FUDE_ZOOM_SYMBOL_TEXT];
        fude_zoom_symbol_text(_n, _count, _ptext, sizeof(_ptext));
        rde_arr _pp = rde_arr_new(sizeof(fude_zoom_v2), _heap), _pa = rde_arr_new(sizeof(fude_zoom_symbol_part), _heap), _pl = rde_arr_new(sizeof(fude_zoom_plot_label), _heap);
        // (playing: as its time and its sliders say)
        const b8 _playing = _r->plot_play != NULL && _r->plot_mask != NULL && _r->drawing < (u32)rde_arr_length(_r->plot_mask) &&
                            ((const u8*)_r->plot_mask->memory)[_r->drawing] != 0u;
        fude_zoom_plot_lines_play(_info->id, _ptext, _n[1], _n[2], _playing ? _r->plot_play : NULL, &_pp, &_pa, &_pl);
        static const rde_color _curve[6] = { { 70, 140, 255, 255 }, { 240, 80, 70, 255 }, { 60, 190, 110, 255 }, { 245, 160, 40, 255 }, { 170, 100, 240, 255 }, { 40, 190, 200, 255 } };
        rde_color _grid = _color;
        _grid.a = (u8)((u32)_color.a * 30u / 100u);
        const fude_zoom_v2* _q = (const fude_zoom_v2*)_pp.memory;
        const fude_zoom_symbol_part* _qp = (const fude_zoom_symbol_part*)_pa.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_pa); _i++) {
            const u8 _fl = _qp[_i].flags;
            const rde_color _c = _fl >= 32u ? _curve[(_fl - 32u) % 6u] : ((_fl & FUDE_ZOOM_SYMBOL_DASHED) ? _grid : _color);
            const f32 _w = _fl >= 32u ? (f32)fmax(_radius * 2.6, 2.0) : (f32)fmax(_radius * ((_fl & FUDE_ZOOM_SYMBOL_DASHED) ? 1.0 : 1.6), 1.0);
            for(u32 _j = 0; _j + 1u < _qp[_i].count; _j++) {
                const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, _q[_qp[_i].first + _j]), _b = fude_zoom_sim_apply(_all, _q[_qp[_i].first + _j + 1u]);
                rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, _c, _w);
            }
        }
        const f32 _lpx = (f32)(fude_zoom_plot_label_height(_info->id, _n[1], _n[2]) * _k);
        if(_r->font != NULL && _lpx >= 5.0f) {
            const fude_zoom_plot_label* _lb = (const fude_zoom_plot_label*)_pl.memory;
            // (the legend on its paper: the curves under it read past it)
            f32 _l0 = 1e30f, _l1 = -1e30f, _b0 = 1e30f, _b1 = -1e30f;
            for(u32 _i = 0; _i < (u32)rde_arr_length(&_pl); _i++) {
                if(_lb[_i].align == 4u) {
                    const fude_zoom_v2 _at = fude_zoom_sim_apply(_all, _lb[_i].at);
                    const f32 _tw = fude_draw_text_width(_r->font, _r->font_px, _lb[_i].text, _lpx);
                    _l0 = fminf(_l0, (f32)_at.x - _lpx * 0.3f);
                    _l1 = fmaxf(_l1, (f32)_at.x + _tw + _lpx * 0.3f);
                    _b0 = fminf(_b0, (f32)_at.y - _lpx * 0.7f);
                    _b1 = fmaxf(_b1, (f32)_at.y + _lpx * 0.7f);
                }
            }
            if(_l1 > _l0) {
                rde_color _paper = fude_theme_active()->page;
                _paper.a = 220u;
                rde_rendering_2d_draw_rectangle((rde_vec_2F){ (_l0 + _l1) * 0.5f, (_b0 + _b1) * 0.5f }, (rde_vec_2F){ _l1 - _l0, _b1 - _b0 }, _paper);
            }
            for(u32 _i = 0; _i < (u32)rde_arr_length(&_pl); _i++) {
                const fude_zoom_v2 _at = fude_zoom_sim_apply(_all, _lb[_i].at);
                const f32 _tw = fude_draw_text_width(_r->font, _r->font_px, _lb[_i].text, _lpx);
                const f32 _x = _lb[_i].align == 1u ? (f32)_at.x - _tw : (_lb[_i].align >= 3u ? (f32)_at.x : (f32)_at.x - _tw * 0.5f);
                const f32 _y = _lb[_i].align == 0u ? (f32)_at.y - _lpx * 0.7f : (f32)_at.y - _lpx * 0.36f;
                rde_rendering_2d_draw_text_2(_r->font, _lb[_i].text, (rde_vec_3F){ _x, _y, 0.0f }, (rde_vec_2F){ _lpx / _r->font_px, _lpx / _r->font_px }, 0.0f,
                                             _lb[_i].curve > 0u ? _curve[(_lb[_i].curve - 1u) % 6u] : fude_theme_active()->text_soft);
            }
        }
        rde_arr_free(&_pp);
        rde_arr_free(&_pa);
        rde_arr_free(&_pl);
    }
    // A circuit's part with named pins (a chip, a module, a board): their names by them, inside it.
    const fude_zoom_part* _part = _custom;
    f32 _inset = 0.0f, _inset_v = 0.8f;
    fude_zoom_part_insets(_part, &_inset, &_inset_v);
    if(_as == NULL && _inset > 0.0f && _r->font != NULL && _info->h > 0.0f) {
        const f64 _pitch = _n[2] * _k / (fude_zoom_part_room_h(_part, (f64)_info->h) * 0.5) * 20.0;   // (pins 20 points apart as it came)
        const f32 _px = (f32)fmin(_pitch * 0.55, 13.0);
        if(_px >= 6.0f) {
            const rde_color _soft = fude_theme_active()->text_soft;
            for(u32 _i = 0; _i < _part->pin_count; _i++) {
                const fude_zoom_pin _q = _part->pins[_i];
                if(_q.name == NULL || _q.name[0] == 0) {
                    continue;
                }
                f32 _w = fude_draw_text_width(_r->font, _r->font_px, _q.name, _px), _qpx = _px;
                if((_q.side == FUDE_ZOOM_PIN_UP || _q.side == FUDE_ZOOM_PIN_DOWN) && _w > 0.9f * (f32)_pitch) {
                    // (side by side along a top or a bottom: each as narrow as the pins are apart — an LCD's VSS VDD V0)
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
    // Its text: its parts in its boxes (a class's compartments, the lines between them drawn here).
    c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
    fude_zoom_symbol_text(_n, _count, _text, sizeof(_text));
    u32 _from[8], _len[8];
    const u32 _split = fude_zoom_symbol_text_split(_text, _from, _len, 8u);
    fude_zoom_box _boxes[8];
    const u32 _nb = fude_zoom_symbol_text_boxes(_kind, _n[1], _n[2], _info->place == FUDE_ZOOM_SYMBOL_TEXT_PARTS ? _split : 1u, _boxes, 8u);
    if(_info->place == FUDE_ZOOM_SYMBOL_TEXT_PARTS && _info->boxes != 4u) {
        for(u32 _i = 1; _i < _nb; _i++) {
            const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, (fude_zoom_v2){ -_n[1], _boxes[_i].max_y });
            const fude_zoom_v2 _b = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _n[1], _boxes[_i].max_y });
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, _color, (f32)fmax(_radius * 2.0, 1.0));
        }
    }
    if(_as == NULL && _text[0] != 0 && !fude_zoom_plot_is(_info->id)) {   // (a graph's: its legend, above)
        const rde_color _ink = _color;   // (its line's colour)
        const u32 _shown = _info->place == FUDE_ZOOM_SYMBOL_TEXT_PARTS ? (_split < _nb ? _split : _nb) : 1u;
        for(u32 _i = 0; _i < _shown; _i++) {
            fude_zoom_box _b = fude_zoom_sim_box(_all, _boxes[_i]);
            if(_part != NULL && _info->place == FUDE_ZOOM_SYMBOL_TEXT_BELOW) {
                // (a circuit's part, however it is turned: its value upright under it, as wide as it needs)
                const fude_zoom_box _on = fude_zoom_sim_box(_all, (fude_zoom_box){ -_n[1], -_n[2], _n[1], _n[2] });
                const f64 _px = _n[3] * _k, _wide = fmax((_on.max_x - _on.min_x) * 2.5, _px * 12.0);
                _b = (fude_zoom_box){ (_on.min_x + _on.max_x) * 0.5 - _wide * 0.5, _on.min_y - _px * 1.8, (_on.min_x + _on.max_x) * 0.5 + _wide * 0.5, _on.min_y - 2.0 };
            }
            const u32 _l = _info->place == FUDE_ZOOM_SYMBOL_TEXT_PARTS ? _len[_i] : (u32)strlen(_text);
            const u32 _f = _info->place == FUDE_ZOOM_SYMBOL_TEXT_PARTS ? _from[_i] : 0u;
            const b8  _top = (_info->place == FUDE_ZOOM_SYMBOL_TEXT_PARTS && _i > 0u) ||   // (a class's attributes from the top of theirs)
                             _info->place == FUDE_ZOOM_SYMBOL_TEXT_CORNER;                // (an area's name from its corner)
            fude_zoom_render_text_at(_r, &_text[_f], _l, _b, _n[3] * _k, _top, _top, _ink);   // (a class's attributes and operations: from the left, as UML has them)
        }
    }
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
            const f64 _out = fabs(_t[_i].in.x) * (f64)_w * 0.5 + fabs(_t[_i].in.y) * (f64)_px * 0.5;
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
                snprintf(_say, sizeof(_say), "%s \xC3\x97 %s  \xC2\xB7  1:%s", _ws, _hs, _sc);
            } else {
                snprintf(_say, sizeof(_say), "%s \xC3\x97 %s", _ws, _hs);
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
                    _reach = fmax(_reach, _lt[_k].at.x + (f64)fude_draw_text_width(_r->font, _r->font_px, _lt[_k].text, 11.0f) + 5.0);
                }
            }
            for(u32 _b = 0; _b < _nb; _b++) {
                fude_zoom_box _bx = _bands[_b];
                if(_bx.max_y - _bx.min_y > _bx.max_x - _bx.min_x) {
                    _bx.max_x = fmax(_bx.max_x, _reach);
                }
                rde_rendering_2d_draw_rectangle((rde_vec_2F){ (f32)((_bx.min_x + _bx.max_x) * 0.5), (f32)((_bx.min_y + _bx.max_y) * 0.5) },
                                                (rde_vec_2F){ (f32)(_bx.max_x - _bx.min_x), (f32)(_bx.max_y - _bx.min_y) }, _paper);
                // (its inner edge: a line where the band meets the drawing)
                const b8 _top = _bx.max_x - _bx.min_x > _bx.max_y - _bx.min_y;
                if(_r->stuck_count < 4u) {
                    _r->stuck[_r->stuck_count]       = _bx;   // (a guide pulled out of it: page.c)
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
                    const f64 _out = fabs(_t[_k].in.x) * (f64)_w * 0.5 + fabs(_t[_k].in.y) * (f64)_px * 0.5;
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
    rde_arr* _local = &_r->shape;
    b8       _closed;
    fude_zoom_shape_outline(_o->channels, _n, _count, fude_zoom_shape_segments(_size), _local, &_closed);
    const u32 _k = (u32)rde_arr_length(_local);
    if(_k < 2u || _k + 1u > FUDE_ZOOM_CLIP_MAX) {
        return;
    }
    const fude_zoom_sim _all    = fude_zoom_sim_compose(_to_screen, fude_zoom_object_sim(_o));
    const rde_color     _color  = _as != NULL ? *_as : fude_theme_resolve(_o->color);
    f64 _radius = fmax((f64)_o->radius * fude_zoom_sim_scale(_all), (f64)FUDE_ZOOM_RENDER_MIN_RADIUS);
    if(_o->channels == FUDE_ZOOM_SHAPE_SHEET) {
        _radius = fmin(_radius, 1.25);   // (a sheet's edge a line on the screen at any zoom, as its rulers are: never over their numbers)
    }
    _radius += (f64)_extra;
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
    // Hatched: its lines across its inside, half its line's width, 12 of its widths apart (too close on the screen to
    // tell apart: a tint of them).
    const u8 _hatch = fude_zoom_hatch_of(_o->q);
    if(_closed && _hatch != FUDE_ZOOM_HATCH_NONE && _as == NULL) {
        const f64 _w_px  = (f64)_o->radius * fude_zoom_sim_scale(_all) * 2.0;
        const f64 _apart = FUDE_ZOOM_HATCH_APART * _w_px;
        if(_apart >= 3.0) {
            rde_arr_clear(&_r->fill);
            const fude_zoom_box _view = { -_guard.x, -_guard.y, _guard.x, _guard.y };
            fude_zoom_hatch_lines(_p, _k, 0.7853981633974483, _apart, _view, &_r->fill);
            if(_hatch == FUDE_ZOOM_HATCH_CROSS) {
                fude_zoom_hatch_lines(_p, _k, 2.356194490192345, _apart, _view, &_r->fill);
            }
            const fude_zoom_v2* _h = (const fude_zoom_v2*)_r->fill.memory;
            const f32 _hr = (f32)fmax(_w_px * 0.25, 0.5);
            for(u32 _i = 0; _i + 1u < (u32)rde_arr_length(&_r->fill); _i += 2u) {
                rde_vec_2F _seg[2] = { { (f32)_h[_i].x, (f32)_h[_i].y }, { (f32)_h[_i + 1u].x, (f32)_h[_i + 1u].y } };
                const f32  _rr[2]  = { _hr, _hr };
                rde_rendering_2d_draw_stroke(_seg, _rr, 2u, _color);
            }
        } else {
            rde_color _tint = _color;
            _tint.a = (u8)((u32)_color.a / 4u);
            fude_zoom_render_polygon_fill(_r, _p, _k, _guard, _tint);
        }
    }
    if(_o->channels == FUDE_ZOOM_SHAPE_BOARD && _as == NULL) {
        fude_zoom_render_board_marks(_r, _s, _o, _n, _count, _all, fude_zoom_render_marks_on(_filled, _fill, _color));
    }
    if(_o->channels == FUDE_ZOOM_SHAPE_SHEET && _as == NULL) {
        fude_zoom_render_sheet_marks(_r, _s, _o, _n, _count, _all, _half, fude_zoom_render_marks_on(_filled, _fill, _color));
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
    rde_arr _items_arr = rde_arr_new(sizeof(fude_zoom_render_item), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_items_arr, _n);
    fude_zoom_render_item* _items = (fude_zoom_render_item*)_items_arr.memory;   // (sized once: it stays put)
    for(u32 _i = 0; _i < _n; _i++) {
        const u32 _o = ((const u32*)_list.memory)[_i];
        _items[_i] = (fude_zoom_render_item){ fude_zoom_scene_draw_key(_s, fude_zoom_scene_object(_s, _o)), _o };
    }
    rde_arr_free(&_list);
    qsort(_items, _n, sizeof(_items[0]), fude_zoom_render_by_z);

    // In order, markers too: a highlight over what was there before it, under what came after (found on the
    // tablet: a frame drew its markers first, under everything — a board, writing — drawn long before them).
    const u8* _lifted   = _r->lifted != NULL ? (const u8*)_r->lifted->memory : NULL;
    const u32 _lifted_n = _r->lifted != NULL ? (u32)rde_arr_length(_r->lifted) : 0u;
    for(u32 _i = 0; _i < _n; _i++) {
        if(_items[_i].object < _lifted_n && _lifted[_items[_i].object]) {
            continue;   // the selection draws it, where the drag has it
        }
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _items[_i].object);
        if(_o->kind != FUDE_ZOOM_KIND_FRAME && fude_zoom_scene_hides(_s, _o)) {
            continue;   // on a hidden layer
        }
        if(_o->kind == FUDE_ZOOM_KIND_STROKE) {
            fude_zoom_render_stroke(_r, _s, _items[_i].object, _to_screen, _half);
        } else if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
            fude_zoom_render_shape(_r, _s, _items[_i].object, _to_screen, _half, NULL, 0.0f);
        } else if(_o->kind == FUDE_ZOOM_KIND_IMAGE) {
            fude_zoom_render_image(_r, _s, _items[_i].object, _to_screen, _half, NULL, 0.0f);
        } else if(_o->kind == FUDE_ZOOM_KIND_TEXT) {
            fude_zoom_render_text(_r, _s, _items[_i].object, _to_screen, NULL);
        } else if(_o->kind == FUDE_ZOOM_KIND_FILL) {
            fude_zoom_render_fill(_r, _s, _items[_i].object, _to_screen, _half, NULL, 0.0f);
        } else if(_o->kind == FUDE_ZOOM_KIND_FRAME) {
            const fude_zoom_frame* _c       = fude_zoom_scene_frame(_s, _o->child);
            const fude_zoom_sim    _c_sim   = fude_zoom_sim_compose(_to_screen, fude_zoom_sim_from_xform(_c->xf));
            const f64              _on_screen = fmax(_c->anchor.max_x - _c->anchor.min_x, _c->anchor.max_y - _c->anchor.min_y) * fude_zoom_sim_scale(_to_screen);
            if(_on_screen >= FUDE_ZOOM_RENDER_MIN_PX) {
                fude_zoom_render_frame(_r, _s, _o->child, _c_sim, _half, _depth + 1u);
            }
        }
    }
    rde_arr_free(&_items_arr);
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
            const f32* _xy = (const f32*)_d->xy.memory;
            for(u32 _a = 0; _a < _d->count; _a++) {
                _pts[_a] = (fude_zoom_v2){ _xy[_a * 2u], _xy[_a * 2u + 1u] };
            }
            const b8 _in = fude_zoom_fill_inside_rings((const fude_zoom_v2*)_r->fill.memory, (const u32*)_d->rings.memory, _d->count, _in_o);
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
        rde_arr _items_arr = rde_arr_new(sizeof(fude_zoom_render_item), rde_memory_allocator_get_default_std());
        rde_arr_resize(&_items_arr, _n);
        fude_zoom_render_item* _items = (fude_zoom_render_item*)_items_arr.memory;   // (sized once: it stays put)
        for(u32 _i = 0; _i < _n; _i++) {
            const u32 _k = ((const u32*)_p->kids.memory)[_i];
            _items[_i] = (fude_zoom_render_item){ fude_zoom_scene_draw_key(_s, fude_zoom_scene_object(_s, _k)), _k };
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
        rde_arr_free(&_items_arr);
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
    if(_o->kind == FUDE_ZOOM_KIND_TEXT) {
        fude_zoom_render_text(_r, _s, _object, _to_screen, NULL);
        return;
    }
    if(_o->kind != FUDE_ZOOM_KIND_FRAME) {
        return;   // (a bookmark: never drawn)
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
    if(fude_zoom_scene_object(_s, _object)->kind == FUDE_ZOOM_KIND_TEXT) {
        // A text: its box, a little larger.
        fude_zoom_v2 _c[4];
        fude_zoom_scene_text_corners(_s, _object, _c);
        rde_vec_2F _p[4];
        const fude_zoom_v2 _mid = fude_zoom_sim_apply(_to_screen, (fude_zoom_v2){ (_c[0].x + _c[2].x) * 0.5, (_c[0].y + _c[2].y) * 0.5 });
        for(u32 _i = 0; _i < 4u; _i++) {
            const fude_zoom_v2 _q = fude_zoom_sim_apply(_to_screen, _c[_i]);
            const f64 _d = hypot(_q.x - _mid.x, _q.y - _mid.y);
            const f64 _k = _d > 0.0 ? (_d + (f64)_extra) / _d : 1.0;
            _p[_i] = (rde_vec_2F){ (f32)(_mid.x + (_q.x - _mid.x) * _k), (f32)(_mid.y + (_q.y - _mid.y) * _k) };
        }
        rde_rendering_2d_draw_polygon(_p, 4u, _color, NULL);
        return;
    }
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
