// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/connect.h"
#include "zoom/shape.h"
#include <math.h>
#include <string.h>

fude_zoom_v2 fude_zoom_connect_middle(const fude_zoom_scene* _s, u32 _object) {
    const fude_zoom_box _b = fude_zoom_scene_object(_s, _object)->box;
    return (fude_zoom_v2){ (_b.min_x + _b.max_x) * 0.5, (_b.min_y + _b.max_y) * 0.5 };
}

fude_zoom_v2 fude_zoom_connect_edge(const fude_zoom_scene* _s, u32 _object, fude_zoom_v2 _toward) {
    const fude_zoom_object* _o   = fude_zoom_scene_object(_s, _object);
    const fude_zoom_v2      _mid = fude_zoom_connect_middle(_s, _object);
    // Its outline, frame units: a shape's own, a text's or a picture's corners, else its box.
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    b8 _closed = true;
    if(_o->kind == FUDE_ZOOM_KIND_SHAPE && _o->channels != FUDE_ZOOM_SHAPE_ARROW) {
        fude_zoom_scene_shape_outline(_s, _object, 48u, &_pts, &_closed);
    } else if(_o->kind == FUDE_ZOOM_KIND_TEXT || _o->kind == FUDE_ZOOM_KIND_IMAGE) {
        fude_zoom_v2* _c = rde_arr_add_n(&_pts, 4u);
        if(_o->kind == FUDE_ZOOM_KIND_TEXT) {
            fude_zoom_scene_text_corners(_s, _object, _c);
        } else {
            fude_zoom_scene_image_corners(_s, _object, _c);
        }
    }
    if(rde_arr_length(&_pts) < 3u || !_closed) {
        rde_arr_clear(&_pts);
        const fude_zoom_box _b = _o->box;
        const fude_zoom_v2  _c[4] = { { _b.min_x, _b.min_y }, { _b.max_x, _b.min_y }, { _b.max_x, _b.max_y }, { _b.min_x, _b.max_y } };
        memcpy(rde_arr_add_n(&_pts, 4u), _c, sizeof(_c));
    }
    // Where the segment from its middle to _toward crosses the outline, the crossing nearest _toward.
    const fude_zoom_v2* _p = (const fude_zoom_v2*)_pts.memory;
    const u32           _n = (u32)rde_arr_length(&_pts);
    const fude_zoom_v2  _d = { _toward.x - _mid.x, _toward.y - _mid.y };
    f64 _best = -1.0;
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_v2 _a = _p[_i], _b = _p[(_i + 1u) % _n];
        const fude_zoom_v2 _e = { _b.x - _a.x, _b.y - _a.y };
        const f64 _den = _d.x * _e.y - _d.y * _e.x;
        if(fabs(_den) < 1e-300) {
            continue;
        }
        const f64 _t = ((_a.x - _mid.x) * _e.y - (_a.y - _mid.y) * _e.x) / _den;   // along the segment
        const f64 _u = ((_a.x - _mid.x) * _d.y - (_a.y - _mid.y) * _d.x) / _den;   // along the side
        if(_t >= 0.0 && _t <= 1.0 && _u >= 0.0 && _u <= 1.0 && _t > _best) {
            _best = _t;
        }
    }
    rde_arr_free(&_pts);
    if(_best < 0.0) {
        return _mid;   // _toward inside it: from its middle
    }
    return (fude_zoom_v2){ _mid.x + _d.x * _best, _mid.y + _d.y * _best };
}

// An arrow's numbers: its points (local), heads, and the ids it joins. False: not one.
RDE_INTERNAL b8 fude_zoom_connect_numbers(const fude_zoom_scene* _s, u32 _object, f64* _n, u32* _count, u32* _k, fude_zoom_id* _from, fude_zoom_id* _to) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if(_o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_ARROW || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
        return false;
    }
    *_count = fude_zoom_scene_shape_numbers(_s, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    *_k     = *_count >= 1u ? (u32)_n[0] : 0u;
    if(*_k < 2u || *_k > FUDE_ZOOM_ARROW_POINTS || *_count < 2u * *_k + 4u) {
        return false;
    }
    memcpy(_from, &_n[2u + 2u * *_k], sizeof(fude_zoom_id));
    memcpy(_to, &_n[3u + 2u * *_k], sizeof(fude_zoom_id));
    return true;
}

// --- dimensions that follow ---------------------------------------------------------------------

b8 fude_zoom_connect_point(const fude_zoom_scene* _s, u32 _object, i32 _key, fude_zoom_v2* _out) {
    if(_object >= fude_zoom_scene_object_count(_s) || _key == FUDE_ZOOM_SNAP_NO_KEY) {
        return false;
    }
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
        return false;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    if(_o->kind == FUDE_ZOOM_KIND_STROKE && _o->count > 0u && (_key == 0 || _key == 1)) {
        fude_zoom_qpoint* _q = _heap->malloc(_heap->allocator, (usize)_o->count * sizeof(fude_zoom_qpoint));
        const b8 _ok = _q != NULL && fude_zoom_scene_points(_s, _object, _q);
        if(_ok) {
            *_out = fude_zoom_scene_point_at(_o, &_q[_key == 0 ? 0u : _o->count - 1u]);
        }
        _heap->free(_heap->allocator, _q);
        return _ok;
    }
    if(_o->kind != FUDE_ZOOM_KIND_SHAPE) {
        return false;
    }
    const u8 _type = _o->channels;
    if(_key == FUDE_ZOOM_SNAP_CENTRE_KEY && (_type == FUDE_ZOOM_SHAPE_ELLIPSE || _type == FUDE_ZOOM_SHAPE_ARC || _type == FUDE_ZOOM_SHAPE_SYMBOL)) {
        *_out = fude_zoom_sim_apply(fude_zoom_object_sim(_o), (fude_zoom_v2){ 0.0, 0.0 });
        return true;
    }
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    b8 _closed;
    fude_zoom_scene_shape_outline(_s, _object, 1u, &_pts, &_closed);
    const fude_zoom_v2* _p = (const fude_zoom_v2*)_pts.memory;
    const u32 _n = (u32)rde_arr_length(&_pts);
    b8 _ok = false;
    if(_key == FUDE_ZOOM_SNAP_CENTRE_KEY && _n > 0u) {
        fude_zoom_v2 _m = { 0.0, 0.0 };
        for(u32 _i = 0; _i < _n; _i++) {
            _m.x += _p[_i].x;
            _m.y += _p[_i].y;
        }
        *_out = (fude_zoom_v2){ _m.x / (f64)_n, _m.y / (f64)_n };
        _ok = true;
    } else if(_key >= 0 && (u32)_key < _n) {
        *_out = _p[_key];
        _ok = true;
    }
    rde_arr_free(&_pts);
    return _ok;
}

b8 fude_zoom_connect_round(const fude_zoom_scene* _s, u32 _object, fude_zoom_v2* _centre, f64* _radius) {
    if(_object >= fude_zoom_scene_object_count(_s)) {
        return false;
    }
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || (_o->channels != FUDE_ZOOM_SHAPE_ELLIPSE && _o->channels != FUDE_ZOOM_SHAPE_ARC)) {
        return false;
    }
    f64 _n[3];
    if(fude_zoom_scene_shape_numbers(_s, _object, _n, 3u) < 2u) {
        return false;
    }
    *_centre = fude_zoom_sim_apply(fude_zoom_object_sim(_o), (fude_zoom_v2){ 0.0, 0.0 });
    *_radius = fabs(_n[0]) * _o->scale;   // (an ellipse's across: a circle's radius)
    return true;
}

// A dimension's ends' things and points (FUDE_ZOOM_SHAPE_ DIMENSION with its refs), or a RADIAL's circle.
typedef struct {
    u8           type;
    f64          n[FUDE_ZOOM_DIMENSION_REFS];
    u32          count;
    fude_zoom_id id[2];
    i32          key[2];
} fude_zoom_connect_dim;

RDE_INTERNAL b8 fude_zoom_connect_dim_of(const fude_zoom_scene* _s, u32 _object, fude_zoom_connect_dim* _d) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || (_o->channels != FUDE_ZOOM_SHAPE_DIMENSION && _o->channels != FUDE_ZOOM_SHAPE_RADIAL)) {
        return false;
    }
    memset(_d, 0, sizeof(*_d));
    _d->type  = _o->channels;
    _d->count = fude_zoom_scene_shape_numbers(_s, _object, _d->n, FUDE_ZOOM_DIMENSION_REFS);
    if(_d->type == FUDE_ZOOM_SHAPE_DIMENSION && _d->count >= FUDE_ZOOM_DIMENSION_REFS) {
        memcpy(&_d->id[0], &_d->n[4], sizeof(fude_zoom_id));
        memcpy(&_d->id[1], &_d->n[6], sizeof(fude_zoom_id));
        _d->key[0] = (i32)_d->n[5];
        _d->key[1] = (i32)_d->n[7];
        return _d->id[0] != 0 || _d->id[1] != 0;
    }
    if(_d->type == FUDE_ZOOM_SHAPE_RADIAL && _d->count >= 4u) {
        memcpy(&_d->id[0], &_d->n[3], sizeof(fude_zoom_id));
        return _d->id[0] != 0;
    }
    return false;
}

// Where a dimension's two ends should be now (frame units): each joined end at its thing's point, a loose one where it
// is (_a, _b as it is). A RADIAL: its centre and its line's end. False: nothing to go by.
RDE_INTERNAL b8 fude_zoom_connect_dim_ends(const fude_zoom_scene* _s, const fude_zoom_connect_dim* _d, const u32* _objs, fude_zoom_v2 _a, fude_zoom_v2 _b,
                                           fude_zoom_v2* _a2, fude_zoom_v2* _b2) {
    if(_d->type == FUDE_ZOOM_SHAPE_RADIAL) {
        fude_zoom_v2 _c;
        f64 _r;
        const f64 _was = hypot(_b.x - _a.x, _b.y - _a.y);
        if(!fude_zoom_connect_round(_s, _objs[0], &_c, &_r) || !(_was > 0.0)) {
            return false;
        }
        *_a2 = _c;
        *_b2 = (fude_zoom_v2){ _c.x + (_b.x - _a.x) * _r / _was, _c.y + (_b.y - _a.y) * _r / _was };
        return true;
    }
    *_a2 = _a;
    *_b2 = _b;
    const b8 _ka = _objs[0] != FUDE_ZOOM_NONE && fude_zoom_connect_point(_s, _objs[0], _d->key[0], _a2);
    const b8 _kb = _objs[1] != FUDE_ZOOM_NONE && fude_zoom_connect_point(_s, _objs[1], _d->key[1], _b2);
    return _ka || _kb;
}

// The similarity taking _a to _a2 and _b to _b2 (turned, scaled, slid). False: either pair a point.
RDE_INTERNAL b8 fude_zoom_connect_two(fude_zoom_v2 _a, fude_zoom_v2 _b, fude_zoom_v2 _a2, fude_zoom_v2 _b2, fude_zoom_sim* _m) {
    const f64 _old = hypot(_b.x - _a.x, _b.y - _a.y), _new = hypot(_b2.x - _a2.x, _b2.y - _a2.y);
    if(!(_old > 0.0) || !(_new > 0.0)) {
        return false;
    }
    const f64 _turn = atan2(_b2.y - _a2.y, _b2.x - _a2.x) - atan2(_b.y - _a.y, _b.x - _a.x);
    const f64 _sc   = _new / _old;
    const f64 _ca   = cos(_turn) * _sc, _sa = sin(_turn) * _sc;
    *_m = (fude_zoom_sim){ _ca, _sa, _a2.x - (_ca * _a.x - _sa * _a.y), _a2.y - (_sa * _a.x + _ca * _a.y) };
    return true;
}

// Its ends as it is (frame units): a dimension's measured points, a RADIAL's centre and its line's end.
RDE_INTERNAL void fude_zoom_connect_dim_now(const fude_zoom_scene* _s, u32 _object, const fude_zoom_connect_dim* _d, fude_zoom_v2* _a, fude_zoom_v2* _b) {
    const fude_zoom_sim _sim = fude_zoom_object_sim(fude_zoom_scene_object(_s, _object));
    *_a = fude_zoom_sim_apply(_sim, (fude_zoom_v2){ 0.0, 0.0 });
    *_b = _d->type == FUDE_ZOOM_SHAPE_RADIAL ? fude_zoom_sim_apply(_sim, (fude_zoom_v2){ cos(_d->n[1]) * _d->n[0], sin(_d->n[1]) * _d->n[0] })
                                             : fude_zoom_sim_apply(_sim, (fude_zoom_v2){ _d->n[0], _d->n[1] });
}

u32 fude_zoom_connect_dimension(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _offset, u32 _a_obj, i32 _a_key, u32 _b_obj, i32 _b_key,
                                rde_color _color, f32 _radius) {
    f64 _n[FUDE_ZOOM_DIMENSION_REFS] = { _b.x - _a.x, _b.y - _a.y, _offset, 0.0, 0.0, 0.0, 0.0, 0.0 };
    const b8 _ja = _a_obj != FUDE_ZOOM_NONE && _a_key != FUDE_ZOOM_SNAP_NO_KEY && _a_obj < fude_zoom_scene_object_count(_s) && fude_zoom_scene_object(_s, _a_obj)->frame == _frame;
    const b8 _jb = _b_obj != FUDE_ZOOM_NONE && _b_key != FUDE_ZOOM_SNAP_NO_KEY && _b_obj < fude_zoom_scene_object_count(_s) && fude_zoom_scene_object(_s, _b_obj)->frame == _frame;
    if(_ja) {
        const fude_zoom_id _id = fude_zoom_scene_object(_s, _a_obj)->id;
        memcpy(&_n[4], &_id, sizeof(fude_zoom_id));
        _n[5] = (f64)_a_key;
    }
    if(_jb) {
        const fude_zoom_id _id = fude_zoom_scene_object(_s, _b_obj)->id;
        memcpy(&_n[6], &_id, sizeof(fude_zoom_id));
        _n[7] = (f64)_b_key;
    }
    return fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ _a, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_DIMENSION, _n, _ja || _jb ? FUDE_ZOOM_DIMENSION_REFS : 3u,
                                     _color, _radius, 0u, 0);
}

u32 fude_zoom_connect_dim_remap(fude_zoom_scene* _s, u32 _dim, const fude_zoom_id* _old, const fude_zoom_id* _new, u32 _n) {
    fude_zoom_connect_dim _d;
    if(_dim >= fude_zoom_scene_object_count(_s) || !fude_zoom_connect_dim_of(_s, _dim, &_d)) {
        return FUDE_ZOOM_NONE;
    }
    b8 _any = false;
    for(u32 _e = 0; _e < 2u; _e++) {
        for(u32 _i = 0; _i < _n && _d.id[_e] != 0; _i++) {
            if(_d.id[_e] == _old[_i]) {
                _d.id[_e] = _new[_i];
                _any = true;
                break;
            }
        }
    }
    if(!_any) {
        return FUDE_ZOOM_NONE;
    }
    const fude_zoom_object _o = *fude_zoom_scene_object(_s, _dim);
    const u32 _objs[2] = { _d.id[0] != 0 ? fude_zoom_scene_find_object(_s, _d.id[0]) : FUDE_ZOOM_NONE, _d.id[1] != 0 ? fude_zoom_scene_find_object(_s, _d.id[1]) : FUDE_ZOOM_NONE };
    fude_zoom_v2 _a, _b, _a2, _b2;
    fude_zoom_connect_dim_now(_s, _dim, &_d, &_a, &_b);
    if(!fude_zoom_connect_dim_ends(_s, &_d, _objs, _a, _b, &_a2, &_b2)) {
        _a2 = _a;
        _b2 = _b;
    }
    // Made again where its ends are now: unturned, its scale (and so its line's offset) as it was.
    const f64 _k = _o.scale > 0.0 ? _o.scale : 1.0;
    f64 _num[FUDE_ZOOM_DIMENSION_REFS];
    memcpy(_num, _d.n, sizeof(_num));
    if(_d.type == FUDE_ZOOM_SHAPE_RADIAL) {
        _num[0] = hypot(_b2.x - _a2.x, _b2.y - _a2.y) / _k;
        _num[1] = atan2(_b2.y - _a2.y, _b2.x - _a2.x);
        memcpy(&_num[3], &_d.id[0], sizeof(fude_zoom_id));
    } else {
        // (its line's side kept: the offset's sign as seen from its own way round)
        _num[0] = (_b2.x - _a2.x) / _k;
        _num[1] = (_b2.y - _a2.y) / _k;
        memcpy(&_num[4], &_d.id[0], sizeof(fude_zoom_id));
        memcpy(&_num[6], &_d.id[1], sizeof(fude_zoom_id));
    }
    const u16 _layer = _s->layer;
    _s->layer = _o.layer;
    _s->style = _o.channels == FUDE_ZOOM_SHAPE_SYMBOL || _o.kind != FUDE_ZOOM_KIND_SHAPE ? 0 : _o.q;   // (its own line, as its layer)
    const u32 _made = fude_zoom_scene_add_shape_fill(_s, _o.frame, (fude_zoom_place){ _a2, 0.0, _k }, _o.channels, _num, _d.count, _o.color, _o.radius,
                                                     (u8)(_o.flags & (FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN)), _o.fill, _o.z);
    _s->layer = _layer;
    _s->style = 0;
    return _made;
}

u32 fude_zoom_connect_follow(fude_zoom_scene* _s, u32 _frame, const u32* _moved, u32 _count, rde_arr* _objects, rde_arr* _before, rde_arr* _after) {
    if(_count == 0) {
        return 0;
    }
    u32 _made = 0;
    const fude_zoom_frame* _f = fude_zoom_scene_frame(_s, _frame);
    RDE_UNUSED(_f);
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
        u32 _nc, _k;
        fude_zoom_id _from, _to;
        if(fude_zoom_scene_object(_s, _i)->frame != _frame) {
            continue;
        }
        // A dimension joined to something moved (not moved itself): its ends where their points are now.
        fude_zoom_connect_dim _dim;
        if(fude_zoom_connect_dim_of(_s, _i, &_dim)) {
            const u32 _objs[2] = { _dim.id[0] != 0 ? fude_zoom_scene_find_object(_s, _dim.id[0]) : FUDE_ZOOM_NONE,
                                   _dim.id[1] != 0 ? fude_zoom_scene_find_object(_s, _dim.id[1]) : FUDE_ZOOM_NONE };
            b8 _touched = false, _self = false;
            for(u32 _m = 0; _m < _count; _m++) {
                _touched = _touched || (_objs[0] != FUDE_ZOOM_NONE && _moved[_m] == _objs[0]) || (_objs[1] != FUDE_ZOOM_NONE && _moved[_m] == _objs[1]);
                _self    = _self || _moved[_m] == _i;
            }
            fude_zoom_v2 _a, _b, _a2, _b2;
            fude_zoom_sim _m;
            if(!_touched || _self) {
                continue;
            }
            fude_zoom_connect_dim_now(_s, _i, &_dim, &_a, &_b);
            if(!fude_zoom_connect_dim_ends(_s, &_dim, _objs, _a, _b, &_a2, &_b2) || !fude_zoom_connect_two(_a, _b, _a2, _b2, &_m)) {
                continue;
            }
            const fude_zoom_place _place = fude_zoom_scene_place_of(_s, _i);
            const fude_zoom_place _next  = fude_zoom_place_moved(_place, _m);
            rde_arr_add(_objects, (any)&_i);
            rde_arr_add(_before, (any)&_place);
            rde_arr_add(_after, (any)&_next);
            fude_zoom_scene_set_place(_s, _i, _next);
            _made++;
            continue;
        }
        if(!fude_zoom_connect_numbers(_s, _i, _n, &_nc, &_k, &_from, &_to) || (_from == 0 && _to == 0)) {
            continue;
        }
        // Joined to something moved (and not moved itself)?
        b8 _touched = false, _self = false;
        const u32 _a_obj = _from != 0 ? fude_zoom_scene_find_object(_s, _from) : FUDE_ZOOM_NONE;
        const u32 _b_obj = _to != 0 ? fude_zoom_scene_find_object(_s, _to) : FUDE_ZOOM_NONE;
        for(u32 _m = 0; _m < _count; _m++) {
            _touched = _touched || _moved[_m] == _a_obj || _moved[_m] == _b_obj;
            _self    = _self || _moved[_m] == _i;
        }
        if(!_touched || _self) {
            continue;
        }
        // Where its ends are and where they should be (frame units).
        const fude_zoom_place _place = fude_zoom_scene_place_of(_s, _i);
        const fude_zoom_sim   _sim   = fude_zoom_object_sim(fude_zoom_scene_object(_s, _i));
        const fude_zoom_v2    _a     = fude_zoom_sim_apply(_sim, (fude_zoom_v2){ _n[1], _n[2] });
        const fude_zoom_v2    _b     = fude_zoom_sim_apply(_sim, (fude_zoom_v2){ _n[2u * _k - 1u], _n[2u * _k] });
        const b8 _a_ok = _a_obj != FUDE_ZOOM_NONE && (fude_zoom_scene_object(_s, _a_obj)->flags & FUDE_ZOOM_FLAG_ALIVE) && fude_zoom_scene_object(_s, _a_obj)->frame == _frame;
        const b8 _b_ok = _b_obj != FUDE_ZOOM_NONE && (fude_zoom_scene_object(_s, _b_obj)->flags & FUDE_ZOOM_FLAG_ALIVE) && fude_zoom_scene_object(_s, _b_obj)->frame == _frame;
        const fude_zoom_v2 _am = _a_ok ? fude_zoom_connect_middle(_s, _a_obj) : _a;
        const fude_zoom_v2 _bm = _b_ok ? fude_zoom_connect_middle(_s, _b_obj) : _b;
        const fude_zoom_v2 _a2 = _a_ok ? fude_zoom_connect_edge(_s, _a_obj, _bm) : _a;
        const fude_zoom_v2 _b2 = _b_ok ? fude_zoom_connect_edge(_s, _b_obj, _am) : _b;
        const f64 _old = hypot(_b.x - _a.x, _b.y - _a.y), _new = hypot(_b2.x - _a2.x, _b2.y - _a2.y);
        if(!(_old > 0.0) || !(_new > 0.0)) {
            continue;
        }
        // The similarity taking a to a2 and b to b2: turned, scaled, slid.
        const f64 _turn = atan2(_b2.y - _a2.y, _b2.x - _a2.x) - atan2(_b.y - _a.y, _b.x - _a.x);
        const f64 _sc   = _new / _old;
        const f64 _ca   = cos(_turn) * _sc, _sa = sin(_turn) * _sc;
        const fude_zoom_sim _m = { _ca, _sa, _a2.x - (_ca * _a.x - _sa * _a.y), _a2.y - (_sa * _a.x + _ca * _a.y) };
        const fude_zoom_place _next = fude_zoom_place_moved(_place, _m);
        rde_arr_add(_objects, (any)&_i);
        rde_arr_add(_before, (any)&_place);
        rde_arr_add(_after, (any)&_next);
        fude_zoom_scene_set_place(_s, _i, _next);
        _made++;
    }
    return _made;
}

u32 fude_zoom_connect_add(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _a, fude_zoom_v2 _b, u32 _from, u32 _to, u32 _heads, rde_color _color, f32 _radius) {
    f64 _n[8];
    _n[0] = 2.0;
    _n[1] = 0.0; _n[2] = 0.0;
    _n[3] = _b.x - _a.x; _n[4] = _b.y - _a.y;
    _n[5] = (f64)_heads;
    const fude_zoom_id _fa = _from != FUDE_ZOOM_NONE ? fude_zoom_scene_object(_s, _from)->id : 0u;
    const fude_zoom_id _tb = _to != FUDE_ZOOM_NONE ? fude_zoom_scene_object(_s, _to)->id : 0u;
    memcpy(&_n[6], &_fa, sizeof(fude_zoom_id));
    memcpy(&_n[7], &_tb, sizeof(fude_zoom_id));
    return fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ _a, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ARROW, _n, 8u, _color, _radius, 0u, 0);
}

u32 fude_zoom_connect_remap(fude_zoom_scene* _s, u32 _arrow, const fude_zoom_id* _old, const fude_zoom_id* _new, u32 _n, u16 _layer) {
    f64 _num[FUDE_ZOOM_SHAPE_NUMBERS];
    u32 _count, _k;
    fude_zoom_id _from, _to;
    if(!fude_zoom_connect_numbers(_s, _arrow, _num, &_count, &_k, &_from, &_to)) {
        return FUDE_ZOOM_NONE;
    }
    b8 _changed = fude_zoom_scene_object(_s, _arrow)->layer != _layer;
    for(u32 _i = 0; _i < _n; _i++) {
        if(_from != 0 && _from == _old[_i]) { _from = _new[_i]; _changed = true; }
        if(_to != 0 && _to == _old[_i])     { _to = _new[_i];   _changed = true; }
    }
    if(!_changed) {
        return FUDE_ZOOM_NONE;
    }
    memcpy(&_num[2u + 2u * _k], &_from, sizeof(fude_zoom_id));
    memcpy(&_num[3u + 2u * _k], &_to, sizeof(fude_zoom_id));
    const fude_zoom_object _o = *fude_zoom_scene_object(_s, _arrow);
    const u16 _was = _s->layer;
    _s->layer = _layer;
    _s->style = 0;
    const u32 _made = fude_zoom_scene_add_shape_fill(_s, _o.frame, fude_zoom_scene_place_of(_s, _arrow), FUDE_ZOOM_SHAPE_ARROW, _num, _count,
                                                     _o.color, _o.radius, _o.flags, _o.fill, _o.z);
    _s->layer = _was;
    return _made;
}

u32 fude_zoom_connect_drive(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _from, fude_zoom_v2 _to, f64 _tol, u32 _skip, rde_arr* _died, rde_arr* _born) {
    // Its objects as they are now (the new ones go in as we go).
    rde_memory_allocator* _heap  = rde_memory_allocator_get_default_std();
    const rde_arr*        _order = &fude_zoom_scene_frame(_s, _frame)->order;
    const u32             _count = (u32)rde_arr_length(_order);
    if(_count == 0) {
        return 0;
    }
    u32* _list = _heap->malloc(_heap->allocator, (usize)_count * sizeof(u32));
    memcpy(_list, _order->memory, (usize)_count * sizeof(u32));
    u32 _made = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        const u32               _object = _list[_i];
        const fude_zoom_object* _o      = fude_zoom_scene_object(_s, _object);
        if(_object == _skip || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || !fude_zoom_scene_touchable(_s, _o)) {
            continue;
        }
        const u8 _type = _o->channels;
        if(_type != FUDE_ZOOM_SHAPE_LINE && _type != FUDE_ZOOM_SHAPE_DIMENSION && _type != FUDE_ZOOM_SHAPE_POLYGON && _type != FUDE_ZOOM_SHAPE_RECT &&
           _type != FUDE_ZOOM_SHAPE_PATH) {
            continue;
        }
        f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
        const u32 _nc = fude_zoom_scene_shape_numbers(_s, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS);
        const fude_zoom_sim   _sim   = fude_zoom_object_sim(_o);
        const fude_zoom_v2    _p     = fude_zoom_sim_apply(fude_zoom_sim_inverse(_sim), _to);   // where it goes, in its own units
        fude_zoom_place       _place = fude_zoom_scene_place_of(_s, _object);
        b8 _moved = false;
#define FUDE_ZOOM_DRIVE_AT(_x, _y) (hypot(fude_zoom_sim_apply(_sim, (fude_zoom_v2){ (_x), (_y) }).x - _from.x, fude_zoom_sim_apply(_sim, (fude_zoom_v2){ (_x), (_y) }).y - _from.y) <= _tol)
        if((_type == FUDE_ZOOM_SHAPE_LINE || _type == FUDE_ZOOM_SHAPE_DIMENSION) && _nc >= 2u) {
            const b8 _start = FUDE_ZOOM_DRIVE_AT(0.0, 0.0), _end = FUDE_ZOOM_DRIVE_AT(_n[0], _n[1]);
            if(_end && !_start) {
                _n[0] = _p.x; _n[1] = _p.y;
                _moved = true;
            } else if(_start && !_end) {
                // Its start there: it starts there now, its end where it was.
                _n[0] -= _p.x; _n[1] -= _p.y;
                _place.t = _to;
                _moved = true;
            }
        } else if(_type == FUDE_ZOOM_SHAPE_POLYGON || _type == FUDE_ZOOM_SHAPE_PATH) {
            // (a path's first number is its bits; its handles, when it keeps them, after its nodes: not driven)
            u32 _end = _nc;
            if(_type == FUDE_ZOOM_SHAPE_PATH) {
                u64 _bits;
                fude_zoom_v2 _np[FUDE_ZOOM_PATH_NODES], _nh[FUDE_ZOOM_PATH_NODES];
                _end = 1u + 2u * fude_zoom_path_unpack(_n, _nc, &_bits, _np, _nh);
            }
            for(u32 _k = _type == FUDE_ZOOM_SHAPE_PATH ? 1u : 0u; _k + 1u < _end; _k += 2u) {
                if(FUDE_ZOOM_DRIVE_AT(_n[_k], _n[_k + 1u])) {
                    _n[_k] = _p.x; _n[_k + 1u] = _p.y;
                    _moved = true;
                }
            }
        } else if(_type == FUDE_ZOOM_SHAPE_RECT && _nc >= 2u) {
            // A corner there: it goes, the one across from it stays.
            for(u32 _k = 0; _k < 4u && !_moved; _k++) {
                const fude_zoom_v2 _c = { (_k & 1u) ? _n[0] : -_n[0], (_k & 2u) ? _n[1] : -_n[1] };
                if(FUDE_ZOOM_DRIVE_AT(_c.x, _c.y)) {
                    const fude_zoom_v2 _across = { -_c.x, -_c.y };
                    const fude_zoom_v2 _mid    = { (_across.x + _p.x) * 0.5, (_across.y + _p.y) * 0.5 };
                    const f64 _hw = fabs(_p.x - _across.x) * 0.5, _hh = fabs(_p.y - _across.y) * 0.5;
                    if(_hw > 0.0 && _hh > 0.0) {   // (flattened to nothing: left as it was)
                        _n[0]    = _hw;
                        _n[1]    = _hh;
                        _place.t = fude_zoom_sim_apply(_sim, _mid);
                        _moved   = true;
                    }
                    break;
                }
            }
        }
#undef FUDE_ZOOM_DRIVE_AT
        if(!_moved) {
            continue;
        }
        const fude_zoom_object _old = *_o;
        const u16 _was = _s->layer;
        _s->layer = _old.layer;
        _s->style = _old.channels == FUDE_ZOOM_SHAPE_SYMBOL || _old.kind != FUDE_ZOOM_KIND_SHAPE ? 0 : _old.q;   // (its own line, as its layer)
        const u32 _new = fude_zoom_scene_add_shape_fill(_s, _frame, _place, _type, _n, _nc, _old.color, _old.radius, _old.flags, _old.fill, _old.z);
        _s->layer = _was;
        _s->style = 0;
        fude_zoom_scene_set_alive(_s, _object, false);
        rde_arr_add(_died, (any)&_object);
        rde_arr_add(_born, (any)&_new);
        _made++;
    }
    _heap->free(_heap->allocator, _list);
    return _made;
}
