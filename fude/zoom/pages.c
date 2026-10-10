// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/pages.h"
#include "zoom/nav.h"
#include "drawing/base/kfile.h"

#include <math.h>

// ===========================================================================
// See pages.h.
// ===========================================================================

#define FZG_KIND    0x45474150u   // 'PAGE'
#define FZG_VERSION 1u

RDE_INTERNAL const f64 FZG_SIZES[FUDE_ZOOM_PAGES_SIZES][2] = {
    { 210.0, 297.0 }, { 148.0, 210.0 }, { 215.9, 279.4 }, { 210.0, 210.0 },
};

void fude_zoom_pages_init(fude_zoom_pages* _p) {
    _p->on    = false;
    _p->w     = FZG_SIZES[FUDE_ZOOM_PAGES_A4][0];
    _p->h     = FZG_SIZES[FUDE_ZOOM_PAGES_A4][1];
    _p->count = 1u;
}

void fude_zoom_pages_size(fude_zoom_pages* _p, u8 _size) {
    const u8 _k = _size < FUDE_ZOOM_PAGES_SIZES ? _size : FUDE_ZOOM_PAGES_A4;
    fude_zoom_pages_custom(_p, FZG_SIZES[_k][0], FZG_SIZES[_k][1]);
}

void fude_zoom_pages_custom(fude_zoom_pages* _p, f64 _w, f64 _h) {
    if(!(_w > 0.0) || !(_h > 0.0)) {
        return;
    }
    _p->on    = true;
    _p->w     = _w;
    _p->h     = _h;
    _p->count = _p->count > 0u ? _p->count : 1u;
}

fude_zoom_box fude_zoom_pages_box(const fude_zoom_pages* _p, u32 _i) {
    const f64 _top = -(f64)_i * (_p->h + FUDE_ZOOM_PAGES_GAP);
    return (fude_zoom_box){ -_p->w * 0.5, _top - _p->h, _p->w * 0.5, _top };
}

fude_zoom_box fude_zoom_pages_bounds(const fude_zoom_pages* _p) {
    const fude_zoom_box _first = fude_zoom_pages_box(_p, 0u), _last = fude_zoom_pages_box(_p, _p->count > 0u ? _p->count - 1u : 0u);
    return (fude_zoom_box){ _first.min_x, _last.min_y, _first.max_x, _first.max_y };
}

u32 fude_zoom_pages_at(const fude_zoom_pages* _p, fude_zoom_v2 _at) {
    // (each page and the gap under it: one step down)
    const f64 _step = _p->h + FUDE_ZOOM_PAGES_GAP;
    const f64 _k    = floor(-_at.y / _step);
    if(!(_k > 0.0) || _p->count == 0u) {
        return 0u;
    }
    return _k >= (f64)(_p->count - 1u) ? _p->count - 1u : (u32)_k;
}

b8 fude_zoom_pages_used(const fude_zoom_pages* _p, const fude_zoom_scene* _s, u32 _i) {
    const fude_zoom_box _box = fude_zoom_pages_box(_p, _i);
    rde_arr _found = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_scene_query(_s, _s->home, _box, &_found);
    b8 _used = false;
    for(u32 _k = 0; _k < (u32)rde_arr_length(&_found) && !_used; _k++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, ((const u32*)_found.memory)[_k]);
        _used = _o->kind != FUDE_ZOOM_KIND_MARK && _o->kind != FUDE_ZOOM_KIND_LAYER && _o->kind != FUDE_ZOOM_KIND_FRAME && !fude_zoom_scene_hides(_s, _o);
    }
    rde_arr_free(&_found);
    // (what was drawn zoomed in: in a frame in the home frame — its drawing, not the view it was made for)
    const fude_zoom_frame* _home = fude_zoom_scene_frame(_s, _s->home);
    for(u32 _k = 0; _k < (u32)rde_arr_length(&_home->kids) && !_used; _k++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, ((const u32*)_home->kids.memory)[_k]);
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || !fude_zoom_box_overlaps(_o->box, _box)) {
            continue;
        }
        const fude_zoom_box _in = fude_zoom_nav_drawn(_s, _o->child, FUDE_ZOOM_NONE);
        _used = !fude_zoom_box_is_empty(_in) &&
                fude_zoom_box_overlaps(fude_zoom_sim_box(fude_zoom_sim_from_xform(fude_zoom_scene_frame(_s, _o->child)->xf), _in), _box);
    }
    return _used;
}

b8 fude_zoom_pages_grow(fude_zoom_pages* _p, const fude_zoom_scene* _s) {
    if(!_p->on) {
        return false;
    }
    b8 _grew = false;
    while(_p->count < FUDE_ZOOM_PAGES_MOST && fude_zoom_pages_used(_p, _s, _p->count - 1u)) {
        _p->count++;
        _grew = true;
    }
    return _grew;
}

f64 fude_zoom_pages_spacing(f64 _true, f64 _z, f64 _out, f64 _in, f64* _fade) {
    const f64 _screen = _true * _z;
    if(_screen < _out) {
        *_fade = fmin(fmax((_screen - _out * 0.25) / (_out * 0.75), 0.0), 1.0);
        return _true;
    }
    f64 _s = _true;
    for(u32 _k = 0; _k < 200u && _s * _z >= 2.0 * _in; _k++) {
        _s *= 0.5;
    }
    *_fade = _s < _true ? fmin(fmax((_s * _z - _in) / _in, 0.0), 1.0) : 1.0;
    return _s;
}

b8 fude_zoom_pages_save(const fude_zoom_pages* _p, const c8* _path) {
    fude_bytes _b = fude_bytes_new(64u);
    fude_put_header(&_b, FZG_VERSION, FZG_KIND);
    fude_put_u8(&_b, _p->on ? 1u : 0u);
    fude_put_f64(&_b, _p->w);
    fude_put_f64(&_b, _p->h);
    fude_put_u32(&_b, _p->count);
    return fude_bytes_write_and_free(&_b, _path, NULL);
}

b8 fude_zoom_pages_load(fude_zoom_pages* _p, const c8* _path) {
    fude_zoom_pages_init(_p);
    if(!rde_file_exists(_path)) {
        return false;
    }
    u32 _size = 0;
    u8* _data = fude_file_read(_path, &_size);
    if(_data == NULL) {
        return false;
    }
    fude_reader _r = fude_reader_make(_data, _size);
    b8 _ok = fude_read_header(&_r, FZG_VERSION, FZG_KIND);
    if(_ok) {
        const b8  _on    = fude_get_u8(&_r) != 0u;
        const f64 _w     = fude_get_f64(&_r), _h = fude_get_f64(&_r);
        const u32 _count = fude_get_u32(&_r);
        _ok = _r.ok && _w > 0.0 && _h > 0.0 && _w < 1e7 && _h < 1e7;
        if(_ok) {
            _p->on    = _on;
            _p->w     = _w;
            _p->h     = _h;
            _p->count = _count < 1u ? 1u : (_count > FUDE_ZOOM_PAGES_MOST ? FUDE_ZOOM_PAGES_MOST : _count);
        }
    }
    fude_file_free(_data);
    return _ok && _p->on;
}
