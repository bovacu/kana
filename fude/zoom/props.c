// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/props.h"
#include "zoom/shape.h"

// ===========================================================================
// See props.h.
//
// An attribute is a PROPS shape whose numbers are its kind, its thing's id in two halves (fude_zoom_id_put), and then
// the kind's own numbers. Every lookup reads those numbers back from the scene (fzpr_numbers); nothing is cached, and
// fude_zoom_props_find searches every object. Internal names start fzpr_.
// ===========================================================================

// Copies an attribute's numbers into _n (room for FUDE_ZOOM_PROPS_NUMBERS) and returns how many it has: 0 when _props
// is out of range, dead, or not a PROPS shape.
RDE_INTERNAL u32 fzpr_numbers(const fude_zoom_scene* _s, u32 _props, f64* _n) {
    if(_props >= fude_zoom_scene_object_count(_s)) {
        return 0u;
    }
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _props);
    if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_PROPS) {
        return 0u;
    }
    return fude_zoom_scene_shape_numbers(_s, _props, _n, FUDE_ZOOM_PROPS_NUMBERS);
}

// Returns the kind an attribute's first number _n0 holds: 1 to 255, or 0 when it holds none (a number outside that
// range, or not a number). The numbers come from a file, so they are checked before the cast: casting an f64 that a u8
// cannot hold is undefined.
RDE_INTERNAL u8 fzpr_kind_of(f64 _n0) {
    return _n0 >= 1.0 && _n0 < 256.0 ? (u8)_n0 : 0u;
}

// See the header. An attribute with fewer than 3 numbers, or a kind outside 1 to 255, is no attribute: 0.
u8 fude_zoom_props_kind(const fude_zoom_scene* _s, u32 _props) {
    f64 _n[FUDE_ZOOM_PROPS_NUMBERS];
    return fzpr_numbers(_s, _props, _n) >= 3u ? fzpr_kind_of(_n[0]) : 0u;
}

// See the header. The thing is found by the id kept in numbers 1 and 2.
u32 fude_zoom_props_target(const fude_zoom_scene* _s, u32 _props) {
    f64 _n[FUDE_ZOOM_PROPS_NUMBERS];
    if(fzpr_numbers(_s, _props, _n) < 3u) {
        return FUDE_ZOOM_NONE;
    }
    const u32 _t = fude_zoom_scene_find_object(_s, fude_zoom_id_get(&_n[1]));
    return _t != FUDE_ZOOM_NONE && (fude_zoom_scene_object(_s, _t)->flags & FUDE_ZOOM_FLAG_ALIVE) ? _t : FUDE_ZOOM_NONE;
}

// See the header. Searches every object of the scene for an attribute of _kind naming _target's id.
u32 fude_zoom_props_find(const fude_zoom_scene* _s, u32 _target, u8 _kind) {
    if(_target >= fude_zoom_scene_object_count(_s)) {
        return FUDE_ZOOM_NONE;
    }
    const fude_zoom_id _id = fude_zoom_scene_object(_s, _target)->id;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        f64 _n[FUDE_ZOOM_PROPS_NUMBERS];
        if(fzpr_numbers(_s, _i, _n) >= 3u && _kind != 0u && fzpr_kind_of(_n[0]) == _kind && fude_zoom_id_get(&_n[1]) == _id) {
            return _i;
        }
    }
    return FUDE_ZOOM_NONE;
}

// See the header. By kind and channels alone, which an object keeps for its life: whether it is alive, and what its
// numbers say, are for the caller to ask each time.
void fude_zoom_props_gather(const fude_zoom_scene* _s, u32* _looked, rde_arr* _out) {
    const u32 _n = fude_zoom_scene_object_count(_s);
    if(*_looked > _n) {
        rde_arr_clear(_out);
        *_looked = 0u;
    }
    for(u32 _i = *_looked; _i < _n; _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if(_o->kind == FUDE_ZOOM_KIND_SHAPE && _o->channels == FUDE_ZOOM_SHAPE_PROPS) {
            rde_arr_add(_out, (any)&_i);
        }
    }
    *_looked = _n;
}

// See the header. Needs all FUDE_ZOOM_PROPS_NUMBERS numbers; a material that is negative, not a number or past what a
// u32 holds reads as 0, and a mass not above 0 as 0 (worked out from the area).
b8 fude_zoom_props_body(const fude_zoom_scene* _s, u32 _props, fude_zoom_body_props* _out) {
    f64 _n[FUDE_ZOOM_PROPS_NUMBERS];
    if(fzpr_numbers(_s, _props, _n) < FUDE_ZOOM_PROPS_NUMBERS || fzpr_kind_of(_n[0]) != FUDE_ZOOM_PROPS_BODY) {
        return false;
    }
    _out->material = _n[3] >= 0.0 && _n[3] < 4294967296.0 ? (u32)_n[3] : 0u;
    _out->fixed    = _n[4] >= 0.5;
    _out->mass     = _n[5] > 0.0 ? _n[5] : 0.0;
    _out->friction = _n[6];
    _out->bounce   = _n[7];
    return true;
}

// Makes a PROPS shape with _count numbers _n (the id's halves already in them) in _target's frame, on its layer, at
// the middle of its box and in its colour, and returns its index. The scene's current layer is set to _target's for
// the add and put back after.
RDE_INTERNAL u32 fzpr_add(fude_zoom_scene* _s, u32 _target, const f64* _n, u32 _count) {
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

// See the header. A negative mass is kept as 0.
u32 fude_zoom_props_add_body(fude_zoom_scene* _s, u32 _target, const fude_zoom_body_props* _body) {
    f64 _n[FUDE_ZOOM_PROPS_NUMBERS] = { (f64)FUDE_ZOOM_PROPS_BODY, 0.0, 0.0, (f64)_body->material, _body->fixed ? 1.0 : 0.0, rde_math_max_f64(_body->mass, 0.0), _body->friction, _body->bounce };
    fude_zoom_id_put(&_n[1], fude_zoom_scene_object(_s, _target)->id);
    return fzpr_add(_s, _target, _n, FUDE_ZOOM_PROPS_NUMBERS);
}

#define FZPR_LIGHT_NUMBERS 7u   // a light's: kind, the id's two halves, red, green, blue, alpha

// See the header. Each channel is clamped to 0 to 255; one that is not a finite number reads as 255.
b8 fude_zoom_props_light(const fude_zoom_scene* _s, u32 _props, rde_color* _out) {
    f64 _n[FUDE_ZOOM_PROPS_NUMBERS];
    if(fzpr_numbers(_s, _props, _n) < FZPR_LIGHT_NUMBERS || fzpr_kind_of(_n[0]) != FUDE_ZOOM_PROPS_LIGHT) {
        return false;
    }
    u8 _c[4];
    for(u32 _i = 0; _i < 4u; _i++) {
        _c[_i] = (u8)rde_math_min_f64(rde_math_max_f64(rde_math_is_finite_f64(_n[3u + _i]) ? _n[3u + _i] : 255.0, 0.0), 255.0);
    }
    *_out = (rde_color){ _c[0], _c[1], _c[2], _c[3] };
    return true;
}

// See the header.
u32 fude_zoom_props_add_light(fude_zoom_scene* _s, u32 _target, rde_color _color) {
    f64 _n[FZPR_LIGHT_NUMBERS] = { (f64)FUDE_ZOOM_PROPS_LIGHT, 0.0, 0.0, (f64)_color.r, (f64)_color.g, (f64)_color.b, (f64)_color.a };
    fude_zoom_id_put(&_n[1], fude_zoom_scene_object(_s, _target)->id);
    return fzpr_add(_s, _target, _n, FZPR_LIGHT_NUMBERS);
}

#define FZPR_LIMITS_NUMBERS 8u   // limits': kind, the id's two halves, four limits, the real part's index + 1 (0: its own)

// See the header. A limit that is not a finite number above 0 reads as 0 (none); the stored part number is one more
// than the part's index, so 0 (or anything below 1, or past what an i32 holds) reads as -1.
b8 fude_zoom_props_limits(const fude_zoom_scene* _s, u32 _props, f64* _most, i32* _preset) {
    f64 _n[FUDE_ZOOM_PROPS_NUMBERS];
    if(fzpr_numbers(_s, _props, _n) < FZPR_LIMITS_NUMBERS || fzpr_kind_of(_n[0]) != FUDE_ZOOM_PROPS_LIMITS) {
        return false;
    }
    for(u32 _k = 0; _k < 4u; _k++) {
        _most[_k] = rde_math_is_finite_f64(_n[3u + _k]) && _n[3u + _k] > 0.0 ? _n[3u + _k] : 0.0;
    }
    *_preset = _n[7] >= 1.0 && _n[7] < 2147483648.0 ? (i32)_n[7] - 1 : -1;
    return true;
}

// See the header. Negative limits are kept as 0, and _preset is stored as its index + 1 (0 when it is -1).
u32 fude_zoom_props_add_limits(fude_zoom_scene* _s, u32 _target, const f64* _most, i32 _preset) {
    f64 _n[FZPR_LIMITS_NUMBERS] = { (f64)FUDE_ZOOM_PROPS_LIMITS, 0.0, 0.0, rde_math_max_f64(_most[0], 0.0), rde_math_max_f64(_most[1], 0.0), rde_math_max_f64(_most[2], 0.0), rde_math_max_f64(_most[3], 0.0),
                                   _preset >= 0 ? (f64)_preset + 1.0 : 0.0 };
    fude_zoom_id_put(&_n[1], fude_zoom_scene_object(_s, _target)->id);
    return fzpr_add(_s, _target, _n, FZPR_LIMITS_NUMBERS);
}

// See the header. The copy keeps every number of the old attribute but the id; an attribute naming id 0 is never
// remapped.
u32 fude_zoom_props_remap(fude_zoom_scene* _s, u32 _props, const fude_zoom_id* _old, const fude_zoom_id* _new, u32 _n) {
    f64 _num[FUDE_ZOOM_PROPS_NUMBERS];
    const u32 _count = fzpr_numbers(_s, _props, _num);
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
            return fzpr_add(_s, _target, _num, _count);
        }
    }
    return FUDE_ZOOM_NONE;
}
