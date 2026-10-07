// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/map.h"
#include "zoom/shape.h"
#include "zoom/symbol.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See map.h.
// ===========================================================================

#define FUDE_ZOOM_MAP_MARGIN 0.06   // round what the map shows, of its larger side

b8 fude_zoom_map_is_area(const fude_zoom_scene* _s, u32 _object) {
    if(_object >= fude_zoom_scene_object_count(_s)) {
        return false;
    }
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_SYMBOL) {
        return false;
    }
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _count = fude_zoom_scene_shape_numbers(_s, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    const u32 _area  = fude_zoom_symbol_find("area");
    return _count >= 4u && _area != FUDE_ZOOM_NONE && (u32)_n[0] == _area;
}

fude_zoom_box fude_zoom_map_contents(const fude_zoom_scene* _s) {
    fude_zoom_box _all = fude_zoom_box_empty();
    const u32 _root = _s->root;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->frame != _root || _o->kind == FUDE_ZOOM_KIND_MARK || _o->kind == FUDE_ZOOM_KIND_LAYER ||
           fude_zoom_scene_hides(_s, _o)) {
            continue;
        }
        if(_o->kind == FUDE_ZOOM_KIND_FRAME) {
            const fude_zoom_frame* _c = fude_zoom_scene_frame(_s, _o->child);
            if(_c->removed || !fude_zoom_scene_frame_used(_s, _o->child)) {
                continue;   // (made for a camera that passed through: nothing in it)
            }
        }
        if(_o->kind == FUDE_ZOOM_KIND_SHAPE && (_o->channels == FUDE_ZOOM_SHAPE_GUIDE || fude_zoom_shape_is_attribute(_o->channels))) {
            continue;   // (a guide reaches far past what it helps draw)
        }
        _all = fude_zoom_box_union(_all, _o->box);
    }
    return _all;
}

fude_zoom_box fude_zoom_map_view(const fude_zoom_scene* _s, fude_zoom_v2 _half) {
    const fude_zoom_camera* _c = &_s->camera;
    const fude_zoom_box _view = { _c->at.x - _half.x / _c->z, _c->at.y - _half.y / _c->z, _c->at.x + _half.x / _c->z, _c->at.y + _half.y / _c->z };
    return fude_zoom_sim_box(fude_zoom_scene_sim(_s, _c->frame, _s->root), _view);
}

u32 fude_zoom_map_areas(const fude_zoom_scene* _s, fude_zoom_map_area* _out, u32 _max) {
    u32 _n = 0;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s) && _n < _max; _i++) {
        if(!fude_zoom_map_is_area(_s, _i)) {
            continue;
        }
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if(fude_zoom_scene_hides(_s, _o) || !fude_zoom_scene_frame_shown(_s, _o->frame)) {
            continue;
        }
        f64 _num[FUDE_ZOOM_SHAPE_NUMBERS];
        const u32 _count = fude_zoom_scene_shape_numbers(_s, _i, _num, FUDE_ZOOM_SHAPE_NUMBERS);
        c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
        fude_zoom_symbol_text(_num, _count, _text, sizeof(_text));
        // Its name: its first line, cut at a whole character.
        usize _len = strcspn(_text, "\n");
        if(_len > FUDE_ZOOM_MAP_NAME - 1u) {
            _len = FUDE_ZOOM_MAP_NAME - 1u;
            while(_len > 0 && ((u8)_text[_len] & 0xC0u) == 0x80u) {
                _len--;
            }
        }
        _out[_n].object = _i;
        _out[_n].box    = fude_zoom_sim_box(fude_zoom_scene_sim(_s, _o->frame, _s->root), _o->box);
        memcpy(_out[_n].name, _text, _len);
        _out[_n].name[_len] = 0;
        _n++;
    }
    return _n;
}

// The number a name begins with (spaces first allowed), or -1.
RDE_INTERNAL i64 fude_zoom_map_leading(const c8* _name) {
    while(*_name == ' ') {
        _name++;
    }
    if(*_name < '0' || *_name > '9') {
        return -1;
    }
    i64 _v = 0;
    for(u32 _d = 0; *_name >= '0' && *_name <= '9' && _d < 15u; _name++, _d++) {
        _v = _v * 10 + (*_name - '0');
    }
    return _v;
}

void fude_zoom_map_areas_order(fude_zoom_map_area* _areas, u32 _n) {
    // (a few: one at a time into its place, the same numbers and the unnumbered keeping the order they had)
    for(u32 _i = 1; _i < _n; _i++) {
        const fude_zoom_map_area _a = _areas[_i];
        const i64 _key = fude_zoom_map_leading(_a.name);
        u32 _j = _i;
        while(_j > 0) {
            const i64 _prev = fude_zoom_map_leading(_areas[_j - 1u].name);
            const b8  _after = _key >= 0 && (_prev < 0 || _prev > _key);
            if(!_after) {
                break;
            }
            _areas[_j] = _areas[_j - 1u];
            _j--;
        }
        _areas[_j] = _a;
    }
}

u32 fude_zoom_map_places(const fude_zoom_scene* _s, fude_zoom_map_place* _out, u32 _max) {
    u32 _n = 0;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        u32 _number = 0;
        if(_o->kind != FUDE_ZOOM_KIND_MARK || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || !fude_zoom_scene_mark_of(_s, _i, NULL, &_number)) {
            continue;
        }
        const fude_zoom_map_place _p = { _i, _number, fude_zoom_sim_apply(fude_zoom_scene_sim(_s, _o->frame, _s->root), _o->t) };
        // In by number (a few: one at a time into its place); past _max, the highest numbers left out.
        u32 _at = _n;
        while(_at > 0 && _out[_at - 1u].number > _p.number) {
            _at--;
        }
        if(_at >= _max) {
            continue;
        }
        const u32 _keep = _n < _max ? _n : _max - 1u;
        memmove(&_out[_at + 1u], &_out[_at], (usize)(_keep - _at) * sizeof(_out[0]));
        _out[_at] = _p;
        _n = _keep + 1u;
    }
    return _n;
}

fude_zoom_box fude_zoom_map_world(fude_zoom_box _contents, fude_zoom_box _view, f64 _aspect) {
    fude_zoom_box _b = fude_zoom_box_is_empty(_contents) ? _view : fude_zoom_box_union(_contents, _view);
    if(fude_zoom_box_is_empty(_b)) {
        _b = (fude_zoom_box){ -1.0, -1.0, 1.0, 1.0 };
    }
    if(fude_zoom_box_is_empty(_contents)) {
        // Nothing drawn: the view in the middle of a little more round it.
        const f64 _w = _b.max_x - _b.min_x, _h = _b.max_y - _b.min_y;
        _b = (fude_zoom_box){ _b.min_x - _w, _b.min_y - _h, _b.max_x + _w, _b.max_y + _h };
    }
    f64 _w = fmax(_b.max_x - _b.min_x, 1e-300), _h = fmax(_b.max_y - _b.min_y, 1e-300);
    const f64 _margin = fmax(_w, _h) * FUDE_ZOOM_MAP_MARGIN;
    _w += 2.0 * _margin;
    _h += 2.0 * _margin;
    _aspect = _aspect > 0.0 ? _aspect : 1.0;
    if(_h / _w < _aspect) {
        _h = _w * _aspect;
    } else {
        _w = _h / _aspect;
    }
    const fude_zoom_v2 _mid = { (_b.min_x + _b.max_x) * 0.5, (_b.min_y + _b.max_y) * 0.5 };
    return (fude_zoom_box){ _mid.x - _w * 0.5, _mid.y - _h * 0.5, _mid.x + _w * 0.5, _mid.y + _h * 0.5 };
}

fude_zoom_camera fude_zoom_map_tap_view(const fude_zoom_scene* _s, fude_zoom_v2 _half, fude_zoom_box _world, fude_zoom_v2 _p) {
    const fude_zoom_box _view  = fude_zoom_map_view(_s, _half);
    const f64           _ww    = fmax(_world.max_x - _world.min_x, 1e-300);
    const f64           _vw    = fmin(fmax(_view.max_x - _view.min_x, _ww / 9.0), _ww * 2.0 / 3.0);
    return (fude_zoom_camera){ _s->root, _p, 2.0 * _half.x / fmax(_vw, 1e-300) };
}
