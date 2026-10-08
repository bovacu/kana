// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/props.h"
#include "zoom/shape.h"
#include <math.h>
#include <string.h>

// An attribute's numbers (FUDE_ZOOM_NONE: not one).
RDE_INTERNAL u32 fzp_numbers(const fude_zoom_scene* _s, u32 _props, f64* _n) {
    if(_props >= fude_zoom_scene_object_count(_s)) {
        return 0u;
    }
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _props);
    if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_PROPS) {
        return 0u;
    }
    return fude_zoom_scene_shape_numbers(_s, _props, _n, FUDE_ZOOM_PROPS_NUMBERS);
}

u8 fude_zoom_props_kind(const fude_zoom_scene* _s, u32 _props) {
    f64 _n[FUDE_ZOOM_PROPS_NUMBERS];
    return fzp_numbers(_s, _props, _n) >= 3u && _n[0] >= 1.0 && _n[0] < 256.0 ? (u8)_n[0] : 0u;
}

u32 fude_zoom_props_target(const fude_zoom_scene* _s, u32 _props) {
    f64 _n[FUDE_ZOOM_PROPS_NUMBERS];
    if(fzp_numbers(_s, _props, _n) < 3u) {
        return FUDE_ZOOM_NONE;
    }
    const u32 _t = fude_zoom_scene_find_object(_s, fude_zoom_id_get(&_n[1]));
    return _t != FUDE_ZOOM_NONE && (fude_zoom_scene_object(_s, _t)->flags & FUDE_ZOOM_FLAG_ALIVE) ? _t : FUDE_ZOOM_NONE;
}

u32 fude_zoom_props_find(const fude_zoom_scene* _s, u32 _target, u8 _kind) {
    if(_target >= fude_zoom_scene_object_count(_s)) {
        return FUDE_ZOOM_NONE;
    }
    const fude_zoom_id _id = fude_zoom_scene_object(_s, _target)->id;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        f64 _n[FUDE_ZOOM_PROPS_NUMBERS];
        if(fzp_numbers(_s, _i, _n) >= 3u && (u8)_n[0] == _kind && fude_zoom_id_get(&_n[1]) == _id) {
            return _i;
        }
    }
    return FUDE_ZOOM_NONE;
}

b8 fude_zoom_props_body(const fude_zoom_scene* _s, u32 _props, fude_zoom_body_props* _out) {
    f64 _n[FUDE_ZOOM_PROPS_NUMBERS];
    if(fzp_numbers(_s, _props, _n) < FUDE_ZOOM_PROPS_NUMBERS || (u8)_n[0] != FUDE_ZOOM_PROPS_BODY) {
        return false;
    }
    _out->material = _n[3] >= 0.0 ? (u32)_n[3] : 0u;
    _out->fixed    = _n[4] >= 0.5;
    _out->mass     = _n[5] > 0.0 ? _n[5] : 0.0;
    _out->friction = _n[6];
    _out->bounce   = _n[7];
    return true;
}

// An attribute with numbers _n (its id's halves among them) by object _target (its frame, its layer, its middle).
RDE_INTERNAL u32 fzp_add(fude_zoom_scene* _s, u32 _target, const f64* _n, u32 _count) {
    const fude_zoom_object* _t = fude_zoom_scene_object(_s, _target);
    const fude_zoom_v2 _at = { (_t->box.min_x + _t->box.max_x) * 0.5, (_t->box.min_y + _t->box.max_y) * 0.5 };
    const u32 _frame = _t->frame;
    const u16 _layer = _t->layer, _was = _s->layer;
    const rde_color _color = _t->color;
    _s->layer = _layer;
    const u32 _made = fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ _at, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_PROPS, _n, _count, _color, 0.0f, 0u, 0);
    _s->layer = _was;
    return _made;
}

u32 fude_zoom_props_add_body(fude_zoom_scene* _s, u32 _target, const fude_zoom_body_props* _body) {
    f64 _n[FUDE_ZOOM_PROPS_NUMBERS] = { (f64)FUDE_ZOOM_PROPS_BODY, 0.0, 0.0, (f64)_body->material, _body->fixed ? 1.0 : 0.0, fmax(_body->mass, 0.0), _body->friction, _body->bounce };
    fude_zoom_id_put(&_n[1], fude_zoom_scene_object(_s, _target)->id);
    return fzp_add(_s, _target, _n, FUDE_ZOOM_PROPS_NUMBERS);
}

#define FZP_LIGHT_NUMBERS 7u   // a light's: kind, the id's two halves, red, green, blue, alpha

b8 fude_zoom_props_light(const fude_zoom_scene* _s, u32 _props, rde_color* _out) {
    f64 _n[FUDE_ZOOM_PROPS_NUMBERS];
    if(fzp_numbers(_s, _props, _n) < FZP_LIGHT_NUMBERS || (u8)_n[0] != FUDE_ZOOM_PROPS_LIGHT) {
        return false;
    }
    u8 _c[4];
    for(u32 _i = 0; _i < 4u; _i++) {
        _c[_i] = (u8)fmin(fmax(isfinite(_n[3u + _i]) ? _n[3u + _i] : 255.0, 0.0), 255.0);
    }
    *_out = (rde_color){ _c[0], _c[1], _c[2], _c[3] };
    return true;
}

u32 fude_zoom_props_add_light(fude_zoom_scene* _s, u32 _target, rde_color _color) {
    f64 _n[FZP_LIGHT_NUMBERS] = { (f64)FUDE_ZOOM_PROPS_LIGHT, 0.0, 0.0, (f64)_color.r, (f64)_color.g, (f64)_color.b, (f64)_color.a };
    fude_zoom_id_put(&_n[1], fude_zoom_scene_object(_s, _target)->id);
    return fzp_add(_s, _target, _n, FZP_LIGHT_NUMBERS);
}

#define FZP_LIMITS_NUMBERS 8u   // limits': kind, the id's two halves, four limits, the real part's index + 1 (0: its own)

b8 fude_zoom_props_limits(const fude_zoom_scene* _s, u32 _props, f64* _most, i32* _preset) {
    f64 _n[FUDE_ZOOM_PROPS_NUMBERS];
    if(fzp_numbers(_s, _props, _n) < FZP_LIMITS_NUMBERS || (u8)_n[0] != FUDE_ZOOM_PROPS_LIMITS) {
        return false;
    }
    for(u32 _k = 0; _k < 4u; _k++) {
        _most[_k] = isfinite(_n[3u + _k]) && _n[3u + _k] > 0.0 ? _n[3u + _k] : 0.0;
    }
    *_preset = isfinite(_n[7]) && _n[7] >= 1.0 ? (i32)_n[7] - 1 : -1;
    return true;
}

u32 fude_zoom_props_add_limits(fude_zoom_scene* _s, u32 _target, const f64* _most, i32 _preset) {
    f64 _n[FZP_LIMITS_NUMBERS] = { (f64)FUDE_ZOOM_PROPS_LIMITS, 0.0, 0.0, fmax(_most[0], 0.0), fmax(_most[1], 0.0), fmax(_most[2], 0.0), fmax(_most[3], 0.0),
                                   _preset >= 0 ? (f64)_preset + 1.0 : 0.0 };
    fude_zoom_id_put(&_n[1], fude_zoom_scene_object(_s, _target)->id);
    return fzp_add(_s, _target, _n, FZP_LIMITS_NUMBERS);
}

u32 fude_zoom_props_remap(fude_zoom_scene* _s, u32 _props, const fude_zoom_id* _old, const fude_zoom_id* _new, u32 _n) {
    f64 _num[FUDE_ZOOM_PROPS_NUMBERS];
    const u32 _count = fzp_numbers(_s, _props, _num);
    if(_count < 3u) {
        return FUDE_ZOOM_NONE;
    }
    const fude_zoom_id _id = fude_zoom_id_get(&_num[1]);
    for(u32 _i = 0; _i < _n; _i++) {
        if(_id != 0u && _id == _old[_i]) {
            const u32 _target = fude_zoom_scene_find_object(_s, _new[_i]);
            if(_target == FUDE_ZOOM_NONE) {
                return FUDE_ZOOM_NONE;
            }
            fude_zoom_id_put(&_num[1], _new[_i]);
            return fzp_add(_s, _target, _num, _count);
        }
    }
    return FUDE_ZOOM_NONE;
}
