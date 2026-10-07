// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/handfind.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See handfind.h.
// ===========================================================================

typedef struct {
    u32 object;
    u32 frame;
    u64 z;
} fude_zoom_hand_stroke;

RDE_INTERNAL int fude_zoom_hand_by_frame(const void* _a, const void* _b) {
    const fude_zoom_hand_stroke* _x = (const fude_zoom_hand_stroke*)_a;
    const fude_zoom_hand_stroke* _y = (const fude_zoom_hand_stroke*)_b;
    if(_x->frame != _y->frame) {
        return _x->frame < _y->frame ? -1 : 1;
    }
    return _x->z < _y->z ? -1 : (_x->z > _y->z ? 1 : 0);
}

RDE_INTERNAL int fude_zoom_hand_f64(const void* _a, const void* _b) {
    const f64 _x = *(const f64*)_a, _y = *(const f64*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

RDE_INTERNAL int fude_zoom_hand_u64(const void* _a, const void* _b) {
    const u64 _x = *(const u64*)_a, _y = *(const u64*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

RDE_INTERNAL int fude_zoom_hand_pair(const void* _a, const void* _b) {
    const u32* _x = (const u32*)_a;
    const u32* _y = (const u32*)_b;
    return _x[0] != _y[0] ? (_x[0] < _y[0] ? -1 : 1) : (_x[1] < _y[1] ? -1 : (_x[1] > _y[1] ? 1 : 0));
}

RDE_INTERNAL u32 fude_zoom_hand_root(u32* _up, u32 _i) {
    while(_up[_i] != _i) {
        _up[_i] = _up[_up[_i]];
        _i      = _up[_i];
    }
    return _i;
}

// Two strokes' boxes along one line of writing?
RDE_INTERNAL b8 fude_zoom_hand_beside(fude_zoom_box _a, fude_zoom_box _b, f64 _h) {
    const f64 _ha = _a.max_y - _a.min_y, _hb = _b.max_y - _b.min_y;
    const f64 _over = fmin(_a.max_y, _b.max_y) - fmax(_a.min_y, _b.min_y);
    const f64 _mid  = fabs((_a.min_y + _a.max_y) - (_b.min_y + _b.max_y)) * 0.5;
    const b8  _level = _over >= 0.25 * fmin(_ha, _hb) || _mid <= 0.6 * _h;
    const f64 _gap  = fmax(_a.min_x, _b.min_x) - fmin(_a.max_x, _b.max_x);   // (< 0: they overlap across)
    return _level && _gap <= _h;
}

u32 fude_zoom_hand_lines(const fude_zoom_scene* _s, rde_arr* _lines, rde_arr* _strokes) {
    rde_arr_clear(_lines);
    rde_arr_clear(_strokes);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    const u32 _total = fude_zoom_scene_object_count(_s);
    // The pen strokes shown, by frame, in the order drawn.
    rde_arr _all = rde_arr_new(sizeof(fude_zoom_hand_stroke), _heap);
    for(u32 _i = 0; _i < _total; _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_STROKE || (_o->flags & FUDE_ZOOM_FLAG_MARKER) || _o->count == 0 ||
           fude_zoom_scene_hides(_s, _o) || !fude_zoom_scene_frame_shown(_s, _o->frame)) {
            continue;
        }
        const fude_zoom_hand_stroke _h = { _i, _o->frame, _o->z };
        rde_arr_add(&_all, (any)&_h);
    }
    const u32 _n = (u32)rde_arr_length(&_all);
    fude_zoom_hand_stroke* _st = (fude_zoom_hand_stroke*)_all.memory;
    if(_n > 1u) {
        qsort(_st, _n, sizeof(fude_zoom_hand_stroke), fude_zoom_hand_by_frame);
    }
    rde_arr TYPE(u32) _at_arr    = rde_arr_new(sizeof(u32), _heap);   // an object's place in _st (or none)
    rde_arr TYPE(u32) _up_arr    = rde_arr_new(sizeof(u32), _heap);
    rde_arr TYPE(f64) _hs_arr    = rde_arr_new(sizeof(f64), _heap);
    rde_arr TYPE(b8)  _write_arr = rde_arr_new(sizeof(b8), _heap);
    rde_arr_resize(&_at_arr, _n > 0u ? _total : 0u);
    rde_arr_resize(&_up_arr, _n);
    rde_arr_resize(&_hs_arr, _n);
    rde_arr_resize(&_write_arr, _n);
    u32* _at    = (u32*)_at_arr.memory;   // (sized once: they stay put)
    u32* _up    = (u32*)_up_arr.memory;
    f64* _hs    = (f64*)_hs_arr.memory;
    b8*  _write = (b8*)_write_arr.memory;
    rde_arr _found = rde_arr_new(sizeof(u32), _heap);
    rde_arr TYPE(u32) _pairs_arr = rde_arr_new(sizeof(u32), _heap);   // a frame's (group, place) pairs
    if(_n > 0u) {
        for(u32 _i = 0; _i < _total; _i++) {
            _at[_i] = FUDE_ZOOM_NONE;
        }
        for(u32 _i = 0; _i < _n; _i++) {
            _at[_st[_i].object] = _i;
            _up[_i] = _i;
        }
    }
    for(u32 _from = 0; _from < _n;) {
        u32 _to = _from;
        while(_to < _n && _st[_to].frame == _st[_from].frame) {
            _to++;
        }
        // The frame's usual stroke height: the middle one (a dot's nothing counted).
        u32 _k = 0;
        for(u32 _i = _from; _i < _to; _i++) {
            const fude_zoom_box _b = fude_zoom_scene_object(_s, _st[_i].object)->box;
            if(_b.max_y - _b.min_y > 0.0) {
                _hs[_k++] = _b.max_y - _b.min_y;
            }
        }
        f64 _h = 0.0;
        if(_k > 0u) {
            qsort(_hs, _k, sizeof(f64), fude_zoom_hand_f64);
            _h = _hs[_k / 2u];
        }
        for(u32 _i = _from; _i < _to; _i++) {
            const fude_zoom_box _b = fude_zoom_scene_object(_s, _st[_i].object)->box;
            _write[_i] = _h > 0.0 && fmax(_b.max_x - _b.min_x, _b.max_y - _b.min_y) <= FUDE_ZOOM_HAND_TALL * _h;
        }
        // Joined to those beside it along a line.
        for(u32 _i = _from; _i < _to && _h > 0.0; _i++) {
            if(!_write[_i]) {
                continue;
            }
            const fude_zoom_box _b = fude_zoom_scene_object(_s, _st[_i].object)->box;
            rde_arr_clear(&_found);
            fude_zoom_scene_query(_s, _st[_i].frame, (fude_zoom_box){ _b.min_x - _h, _b.min_y - _h * 0.25, _b.max_x + _h, _b.max_y + _h * 0.25 }, &_found);
            for(u32 _f = 0; _f < (u32)rde_arr_length(&_found); _f++) {
                const u32 _o = ((const u32*)_found.memory)[_f];
                const u32 _j = _o < _total ? _at[_o] : FUDE_ZOOM_NONE;
                if(_j == FUDE_ZOOM_NONE || _j == _i || !_write[_j] || !fude_zoom_hand_beside(_b, fude_zoom_scene_object(_s, _o)->box, _h)) {
                    continue;
                }
                const u32 _ri = fude_zoom_hand_root(_up, _i), _rj = fude_zoom_hand_root(_up, _j);
                if(_ri != _rj) {
                    _up[_ri > _rj ? _ri : _rj] = _ri > _rj ? _rj : _ri;
                }
            }
        }
        // Each group a line: its strokes in the order drawn (the pairs sorted by group, then by place: _st's order), its box and key.
        u32 _m = 0;
        rde_arr_resize(&_pairs_arr, _h > 0.0 ? (_to - _from) * 2u : 0u);
        u32* _pairs = (u32*)_pairs_arr.memory;
        for(u32 _i = _from; _i < _to && _h > 0.0; _i++) {
            if(_write[_i]) {
                _pairs[2u * _m]      = fude_zoom_hand_root(_up, _i);
                _pairs[2u * _m + 1u] = _i;
                _m++;
            }
        }
        if(_m > 1u) {
            qsort(_pairs, _m, 2u * sizeof(u32), fude_zoom_hand_pair);
        }
        rde_arr _ids = rde_arr_new(sizeof(u64), _heap);
        for(u32 _g = 0; _g < _m;) {
            u32 _e = _g;
            while(_e < _m && _pairs[2u * _e] == _pairs[2u * _g]) {
                _e++;
            }
            fude_zoom_hand_line _line = { _st[_pairs[2u * _g + 1u]].frame, fude_zoom_box_empty(), 0u, (u32)rde_arr_length(_strokes), _e - _g, _h };
            rde_arr_clear(&_ids);
            for(u32 _j = _g; _j < _e; _j++) {
                const u32               _object = _st[_pairs[2u * _j + 1u]].object;
                const fude_zoom_object* _o      = fude_zoom_scene_object(_s, _object);
                rde_arr_add(_strokes, (any)&_object);
                rde_arr_add(&_ids, (any)&_o->id);
                _line.box = fude_zoom_box_union(_line.box, _o->box);
            }
            qsort(_ids.memory, _line.count, sizeof(u64), fude_zoom_hand_u64);
            u64 _key = 14695981039346656037ull;
            for(u32 _j = 0; _j < _line.count; _j++) {
                const u64 _id = ((const u64*)_ids.memory)[_j];
                for(u32 _b = 0; _b < 8u; _b++) {
                    _key ^= (_id >> (8u * _b)) & 0xFFu;
                    _key *= 1099511628211ull;
                }
            }
            _line.key = _key;
            rde_arr_add(_lines, (any)&_line);
            _g = _e;
        }
        rde_arr_free(&_ids);
        _from = _to;
    }
    rde_arr_free(&_pairs_arr);
    rde_arr_free(&_found);
    rde_arr_free(&_all);
    rde_arr_free(&_at_arr);
    rde_arr_free(&_up_arr);
    rde_arr_free(&_hs_arr);
    rde_arr_free(&_write_arr);
    return (u32)rde_arr_length(_lines);
}
