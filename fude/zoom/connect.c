// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/connect.h"
#include "zoom/shape.h"

// ===========================================================================
// See connect.h.
//
// No state of its own: what is joined is kept in the shapes' numbers (an arrow's ids after its points and heads, a
// dimension's after its offset, a constraint's two ids), and every link is found again by id when it is needed. In
// order: a connector's ends on outlines, an arrow's numbers, the dimensions that follow, the constraints, then
// fude_zoom_connect_follow (which moves all three kinds), adding and remapping connectors, and driving. The file's
// own names start with fude_zoom_connect_.
// ===========================================================================

// See the header.
fude_zoom_v2 fude_zoom_connect_middle(const fude_zoom_scene* _s, u32 _object) {
    const fude_zoom_box _b = fude_zoom_scene_object(_s, _object)->box;
    return (fude_zoom_v2){ (_b.min_x + _b.max_x) * 0.5, (_b.min_y + _b.max_y) * 0.5 };
}

// See the header. Of the outline's crossings with the segment from the middle to _toward, the one nearest _toward is
// taken, so a concave outline is left where the line last crosses it. A shape's outline is cut into 48 segments.
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
        rde_memcpy(rde_arr_add_n(&_pts, 4u), _c, sizeof(_c));
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
        if(rde_math_abs_f64(_den) < 1e-300) {
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

// See the header. An id's bits in an older arrow are copied out whole, as they were put in.
b8 fude_zoom_connect_arrow_ends(const f64* _n, u32 _count, u32 _k, fude_zoom_id* _from, fude_zoom_id* _to) {
    *_from = *_to = 0u;
    if(_count >= FUDE_ZOOM_ARROW_NUMBERS(_k)) {
        *_from = fude_zoom_id_get(&_n[2u + 2u * _k]);
        *_to   = fude_zoom_id_get(&_n[4u + 2u * _k]);
        return true;
    }
    if(_count >= FUDE_ZOOM_ARROW_NUMBERS_BITS(_k)) {
        rde_memcpy(_from, &_n[2u + 2u * _k], sizeof(fude_zoom_id));
        rde_memcpy(_to, &_n[3u + 2u * _k], sizeof(fude_zoom_id));
        return true;
    }
    return false;
}

// See the header.
void fude_zoom_connect_arrow_put(f64* _n, u32 _k, fude_zoom_id _from, fude_zoom_id _to) {
    fude_zoom_id_put(&_n[2u + 2u * _k], _from);
    fude_zoom_id_put(&_n[4u + 2u * _k], _to);
}

// Reads a live arrow's numbers into _n (room for FUDE_ZOOM_SHAPE_NUMBERS) and *_count, its point count into *_k, and
// the ids it joins into *_from and *_to (0 for a loose end: fude_zoom_connect_arrow_ends). Returns false when _object
// is not a live arrow, its point count is not one an arrow has (fude_zoom_arrow_points: under 2, or a damaged file's),
// or its numbers are too few for its points and ids.
RDE_INTERNAL b8 fude_zoom_connect_numbers(const fude_zoom_scene* _s, u32 _object, f64* _n, u32* _count, u32* _k, fude_zoom_id* _from, fude_zoom_id* _to) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if(_o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_ARROW || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
        return false;
    }
    *_count = fude_zoom_scene_shape_numbers(_s, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    *_k     = fude_zoom_arrow_points(_n, *_count);
    return *_k >= 2u && fude_zoom_connect_arrow_ends(_n, *_count, *_k, _from, _to);
}

// --- dimensions that follow ---------------------------------------------------------------------

// See the header. A shape's centre is its translation for an ellipse, an arc or a symbol, else the mean of its
// outline's points at one segment.
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
        rde_arr TYPE(fude_zoom_qpoint) _q_arr = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
        fude_zoom_qpoint* _q = (fude_zoom_qpoint*)rde_arr_add_n(&_q_arr, _o->count);
        const b8 _ok = fude_zoom_scene_points(_s, _object, _q);
        if(_ok) {
            *_out = fude_zoom_scene_point_at(_o, &_q[_key == 0 ? 0u : _o->count - 1u]);
        }
        rde_arr_free(&_q_arr);
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

// See the header. The radius is the shape's first number scaled: an ellipse's radius across, which is a circle's
// radius.
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
    *_radius = rde_math_abs_f64(_n[0]) * _o->scale;   // (an ellipse's across: a circle's radius)
    return true;
}

// A dimension's or a RADIAL's numbers as fude_zoom_connect_dim_of reads them: its type, its numbers (at most
// FUDE_ZOOM_DIMENSION_REFS) and how many, and the ids and snap keys of what its two ends are on (a RADIAL's circle is
// id[0]; an end not joined has id 0).
typedef struct {
    u8           type;
    f64          n[FUDE_ZOOM_DIMENSION_REFS];
    u32          count;
    fude_zoom_id id[2];
    i32          key[2];
} fude_zoom_connect_dim;

// Reads a live DIMENSION's or RADIAL's numbers and what it is joined to into *_d. Returns false when it is neither,
// or is joined to nothing.
RDE_INTERNAL b8 fude_zoom_connect_dim_of(const fude_zoom_scene* _s, u32 _object, fude_zoom_connect_dim* _d) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || (_o->channels != FUDE_ZOOM_SHAPE_DIMENSION && _o->channels != FUDE_ZOOM_SHAPE_RADIAL)) {
        return false;
    }
    rde_memset(_d, 0, sizeof(*_d));
    _d->type  = _o->channels;
    _d->count = fude_zoom_scene_shape_numbers(_s, _object, _d->n, FUDE_ZOOM_DIMENSION_REFS);
    if(_d->type == FUDE_ZOOM_SHAPE_DIMENSION) {
        return fude_zoom_connect_dimension_ends(_d->n, _d->count, _d->id, _d->key) && (_d->id[0] != 0 || _d->id[1] != 0);
    }
    if(_d->type == FUDE_ZOOM_SHAPE_RADIAL) {
        _d->id[0] = fude_zoom_connect_radial_id(_d->n, _d->count);
        return _d->id[0] != 0;
    }
    return false;
}

// See the header. Four numbers are an older RADIAL's, its id's bits copied whole into number 3.
fude_zoom_id fude_zoom_connect_radial_id(const f64* _n, u32 _count) {
    if(_count >= FUDE_ZOOM_RADIAL_NUMBERS) {
        return fude_zoom_id_get(&_n[3]);
    }
    fude_zoom_id _id = 0;
    if(_count == 4u) {
        rde_memcpy(&_id, &_n[3], sizeof(fude_zoom_id));
    }
    return _id;
}

// Returns a key kept as a number: _v when it is a whole number an i32 holds, else FUDE_ZOOM_SNAP_NO_KEY (casting a
// number out of range, as a damaged file's may be, would be undefined).
RDE_INTERNAL i32 fude_zoom_connect_key_of(f64 _v) {
    return _v >= -2147483648.0 && _v <= 2147483647.0 && _v == rde_math_floor_f64(_v) ? (i32)_v : FUDE_ZOOM_SNAP_NO_KEY;
}

// See the header. FUDE_ZOOM_DIMENSION_REFS numbers or more are kept as they are now (ids at 4 and 7, keys at 6 and 9);
// exactly FUDE_ZOOM_DIMENSION_REFS_BITS are an older file's (ids' bits at 4 and 6, keys at 5 and 7).
b8 fude_zoom_connect_dimension_ends(const f64* _n, u32 _count, fude_zoom_id _id[2], i32 _key[2]) {
    _id[0] = _id[1] = 0u;
    _key[0] = _key[1] = FUDE_ZOOM_SNAP_NO_KEY;
    if(_count >= FUDE_ZOOM_DIMENSION_REFS) {
        _id[0]  = fude_zoom_id_get(&_n[4]);
        _id[1]  = fude_zoom_id_get(&_n[7]);
        _key[0] = fude_zoom_connect_key_of(_n[6]);
        _key[1] = fude_zoom_connect_key_of(_n[9]);
        return true;
    }
    if(_count == FUDE_ZOOM_DIMENSION_REFS_BITS) {
        rde_memcpy(&_id[0], &_n[4], sizeof(fude_zoom_id));
        rde_memcpy(&_id[1], &_n[6], sizeof(fude_zoom_id));
        _key[0] = fude_zoom_connect_key_of(_n[5]);
        _key[1] = fude_zoom_connect_key_of(_n[7]);
        return true;
    }
    return false;
}

// See the header.
void fude_zoom_connect_dimension_put(f64* _n, const fude_zoom_id _id[2], const i32 _key[2]) {
    _n[3] = 0.0;
    fude_zoom_id_put(&_n[4], _id[0]);
    _n[6] = (f64)_key[0];
    fude_zoom_id_put(&_n[7], _id[1]);
    _n[9] = (f64)_key[1];
}

// Works out where a dimension's two ends should be now (frame units) into *_a2 and *_b2: each joined end (its thing's
// object index in _objs, FUDE_ZOOM_NONE for none) at its thing's point, a loose one where it is (_a, _b as it is). A
// RADIAL's are its circle's centre and the point on the circle along its line's way. Returns false when there is
// nothing to go by: no joined end found, or a RADIAL's circle gone or its line of no length.
RDE_INTERNAL b8 fude_zoom_connect_dim_ends(const fude_zoom_scene* _s, const fude_zoom_connect_dim* _d, const u32* _objs, fude_zoom_v2 _a, fude_zoom_v2 _b,
                                           fude_zoom_v2* _a2, fude_zoom_v2* _b2) {
    if(_d->type == FUDE_ZOOM_SHAPE_RADIAL) {
        fude_zoom_v2 _c;
        f64 _r;
        const f64 _was = rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y);
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

// Works out the similarity taking _a to _a2 and _b to _b2 (turned, scaled, slid) into *_m. Returns false when either
// pair is one point.
RDE_INTERNAL b8 fude_zoom_connect_two(fude_zoom_v2 _a, fude_zoom_v2 _b, fude_zoom_v2 _a2, fude_zoom_v2 _b2, fude_zoom_sim* _m) {
    const f64 _old = rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y), _new = rde_math_hypot_f64(_b2.x - _a2.x, _b2.y - _a2.y);
    if(!(_old > 0.0) || !(_new > 0.0)) {
        return false;
    }
    const f64 _turn = rde_math_atan2_f64(_b2.y - _a2.y, _b2.x - _a2.x) - rde_math_atan2_f64(_b.y - _a.y, _b.x - _a.x);
    const f64 _sc   = _new / _old;
    const f64 _ca   = rde_math_cos_f64(_turn) * _sc, _sa = rde_math_sin_f64(_turn) * _sc;
    *_m = (fude_zoom_sim){ _ca, _sa, _a2.x - (_ca * _a.x - _sa * _a.y), _a2.y - (_sa * _a.x + _ca * _a.y) };
    return true;
}

// Writes a dimension's ends as it is (frame units) into *_a and *_b: a dimension's measured points, a RADIAL's centre
// and its line's end.
RDE_INTERNAL void fude_zoom_connect_dim_now(const fude_zoom_scene* _s, u32 _object, const fude_zoom_connect_dim* _d, fude_zoom_v2* _a, fude_zoom_v2* _b) {
    const fude_zoom_sim _sim = fude_zoom_object_sim(fude_zoom_scene_object(_s, _object));
    *_a = fude_zoom_sim_apply(_sim, (fude_zoom_v2){ 0.0, 0.0 });
    *_b = _d->type == FUDE_ZOOM_SHAPE_RADIAL ? fude_zoom_sim_apply(_sim, (fude_zoom_v2){ rde_math_cos_f64(_d->n[1]) * _d->n[0], rde_math_sin_f64(_d->n[1]) * _d->n[0] })
                                             : fude_zoom_sim_apply(_sim, (fude_zoom_v2){ _d->n[0], _d->n[1] });
}

// See the header. A dimension joined to nothing keeps only its three numbers; one joined to anything keeps all
// FUDE_ZOOM_DIMENSION_REFS (shape.h's layout: fude_zoom_connect_dimension_put), an end joined to nothing with id 0.
u32 fude_zoom_connect_dimension(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _offset, u32 _a_obj, i32 _a_key, u32 _b_obj, i32 _b_key,
                                rde_color _color, f32 _radius) {
    f64 _n[FUDE_ZOOM_DIMENSION_REFS] = { _b.x - _a.x, _b.y - _a.y, _offset };
    const b8 _ja = _a_obj != FUDE_ZOOM_NONE && _a_key != FUDE_ZOOM_SNAP_NO_KEY && _a_obj < fude_zoom_scene_object_count(_s) && fude_zoom_scene_object(_s, _a_obj)->frame == _frame;
    const b8 _jb = _b_obj != FUDE_ZOOM_NONE && _b_key != FUDE_ZOOM_SNAP_NO_KEY && _b_obj < fude_zoom_scene_object_count(_s) && fude_zoom_scene_object(_s, _b_obj)->frame == _frame;
    const fude_zoom_id _ids[2]  = { _ja ? fude_zoom_scene_object(_s, _a_obj)->id : 0u, _jb ? fude_zoom_scene_object(_s, _b_obj)->id : 0u };
    const i32          _keys[2] = { _ja ? _a_key : 0, _jb ? _b_key : 0 };
    fude_zoom_connect_dimension_put(_n, _ids, _keys);
    return fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ _a, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_DIMENSION, _n, _ja || _jb ? FUDE_ZOOM_DIMENSION_REFS : 3u,
                                     _color, _radius, 0u, 0);
}

// See the header. When its ends cannot be found again it is made again where it is, with its new ids. It keeps its
// layer, line style, colour, width, flags and z; the scene's layer and style are put back after.
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
    rde_memcpy(_num, _d.n, sizeof(_num));
    u32 _count = _d.count;
    if(_d.type == FUDE_ZOOM_SHAPE_RADIAL) {
        _num[0] = rde_math_hypot_f64(_b2.x - _a2.x, _b2.y - _a2.y) / _k;
        _num[1] = rde_math_atan2_f64(_b2.y - _a2.y, _b2.x - _a2.x);
        fude_zoom_id_put(&_num[3], _d.id[0]);   // (an older one's id written as it is now: two halves)
        _count = FUDE_ZOOM_RADIAL_NUMBERS;
    } else {
        // (its line's side kept: the offset's sign as seen from its own way round)
        _num[0] = (_b2.x - _a2.x) / _k;
        _num[1] = (_b2.y - _a2.y) / _k;
        fude_zoom_connect_dimension_put(_num, _d.id, _d.key);   // (an older one's ids written as they are now: halves)
        _count = FUDE_ZOOM_DIMENSION_REFS;
    }
    const u16 _layer = _s->layer;
    _s->layer = _o.layer;
    _s->style = _o.channels == FUDE_ZOOM_SHAPE_SYMBOL || _o.kind != FUDE_ZOOM_KIND_SHAPE ? 0 : _o.q;   // (its own line, as its layer)
    const u32 _made = fude_zoom_scene_add_shape_fill(_s, _o.frame, (fude_zoom_place){ _a2, 0.0, _k }, _o.channels, _num, _count, _o.color, _o.radius,
                                                     (u8)(_o.flags & (FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN)), _o.fill, _o.z);
    _s->layer = _layer;
    _s->style = 0;
    return _made;
}

// --- constraints ---------------------------------------------------------------------------------

// See the header. Two layouts are read: five numbers (the kind, then each id as two halves) and the three of
// 0.1.45-0.1.48 (the kind, then each id as one f64). A line found that is not a live LINE counts as gone.
b8 fude_zoom_connect_constraint_of(const fude_zoom_scene* _s, u32 _object, u8* _kind, u32* _a, u32* _b) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_CONSTRAINT) {
        return false;
    }
    f64 _n[5];
    const u32 _count = fude_zoom_scene_shape_numbers(_s, _object, _n, 5u);
    if(_count != 5u && _count != 3u) {
        return false;
    }
    *_kind = (u8)_n[0];
    if(_count == 5u) {
        // (each line's id its two halves: scene.h's fude_zoom_id_put)
        const fude_zoom_id _ia = fude_zoom_id_get(&_n[1]), _ib = fude_zoom_id_get(&_n[3]);
        *_a = _ia != 0u ? fude_zoom_scene_find_object(_s, _ia) : FUDE_ZOOM_NONE;
        *_b = _ib != 0u ? fude_zoom_scene_find_object(_s, _ib) : FUDE_ZOOM_NONE;
    } else {
        // (0.1.45–0.1.48's: each id one number, rounded once a device's half is large — the line in its
        // frame it rounds from)
        for(u32 _k = 0; _k < 2u; _k++) {
            u32* _l = _k == 0u ? _a : _b;
            *_l = _n[1u + _k] > 0.0 ? fude_zoom_scene_find_object(_s, (fude_zoom_id)_n[1u + _k]) : FUDE_ZOOM_NONE;
            for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s) && *_l == FUDE_ZOOM_NONE && _n[1u + _k] > 0.0; _i++) {
                const fude_zoom_object* _q = fude_zoom_scene_object(_s, _i);
                if((_q->flags & FUDE_ZOOM_FLAG_ALIVE) && _q->kind == FUDE_ZOOM_KIND_SHAPE && _q->channels == FUDE_ZOOM_SHAPE_LINE && _q->frame == _o->frame &&
                   (f64)_q->id == _n[1u + _k]) {
                    *_l = _i;
                }
            }
        }
    }
    for(u32 _k = 0; _k < 2u; _k++) {
        u32* _l = _k == 0u ? _a : _b;
        if(*_l != FUDE_ZOOM_NONE) {
            const fude_zoom_object* _lo = fude_zoom_scene_object(_s, *_l);
            if(!(_lo->flags & FUDE_ZOOM_FLAG_ALIVE) || _lo->kind != FUDE_ZOOM_KIND_SHAPE || _lo->channels != FUDE_ZOOM_SHAPE_LINE) {
                *_l = FUDE_ZOOM_NONE;
            }
        }
    }
    return true;
}

// Writes a line's two ends now (its frame's units) into *_p0 and *_p1. Returns false when its outline is not two
// points.
RDE_INTERNAL b8 fude_zoom_connect_line_ends(const fude_zoom_scene* _s, u32 _line, fude_zoom_v2* _p0, fude_zoom_v2* _p1) {
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    b8 _closed;
    fude_zoom_scene_shape_outline(_s, _line, 1u, &_pts, &_closed);
    const b8 _ok = rde_arr_length(&_pts) == 2u;
    if(_ok) {
        *_p0 = ((const fude_zoom_v2*)_pts.memory)[0];
        *_p1 = ((const fude_zoom_v2*)_pts.memory)[1];
    }
    rde_arr_free(&_pts);
    return _ok;
}

// See the header. EQUAL scales _b round its middle to _a's length; PARALLEL and SQUARE turn it the least way that
// keeps it so, under a quarter turn either way, since a line turned half round is the same line.
b8 fude_zoom_connect_constrain(const fude_zoom_scene* _s, u8 _kind, u32 _a, u32 _b, fude_zoom_sim* _out) {
    fude_zoom_v2 _a0, _a1, _b0, _b1;
    if(_a == FUDE_ZOOM_NONE || _b == FUDE_ZOOM_NONE || fude_zoom_scene_object(_s, _a)->frame != fude_zoom_scene_object(_s, _b)->frame ||
       !fude_zoom_connect_line_ends(_s, _a, &_a0, &_a1) || !fude_zoom_connect_line_ends(_s, _b, &_b0, &_b1)) {
        return false;
    }
    const f64 _la = rde_math_hypot_f64(_a1.x - _a0.x, _a1.y - _a0.y), _lb = rde_math_hypot_f64(_b1.x - _b0.x, _b1.y - _b0.y);
    if(!(_la > 0.0) || !(_lb > 0.0)) {
        return false;
    }
    f64 _turn = 0.0, _k = 1.0;
    if(_kind == FUDE_ZOOM_CONSTRAINT_EQUAL) {
        _k = _la / _lb;
    } else {
        // The nearest way that keeps it so (a line has no front: half a turn is the same line).
        const f64 _want = rde_math_atan2_f64(_a1.y - _a0.y, _a1.x - _a0.x) + (_kind == FUDE_ZOOM_CONSTRAINT_SQUARE ? RDE_PI * 0.5 : 0.0);
        _turn = _want - rde_math_atan2_f64(_b1.y - _b0.y, _b1.x - _b0.x);
        _turn = rde_math_fmod_f64(_turn, RDE_PI);
        if(_turn > RDE_PI * 0.5)  { _turn -= RDE_PI; }
        if(_turn < -RDE_PI * 0.5) { _turn += RDE_PI; }
    }
    if(rde_math_abs_f64(_turn) < 1e-12 && rde_math_abs_f64(_k - 1.0) < 1e-12) {
        return false;
    }
    const fude_zoom_v2 _m = { (_b0.x + _b1.x) * 0.5, (_b0.y + _b1.y) * 0.5 };
    const f64 _ca = rde_math_cos_f64(_turn) * _k, _sa = rde_math_sin_f64(_turn) * _k;
    *_out = (fude_zoom_sim){ _ca, _sa, _m.x - (_ca * _m.x - _sa * _m.y), _m.y - (_sa * _m.x + _ca * _m.y) };
    return true;
}

// See the header. It takes _a's colour; the scene's layer is put back after.
u32 fude_zoom_connect_constraint_add(fude_zoom_scene* _s, u8 _kind, u32 _a, u32 _b) {
    const fude_zoom_object* _oa = fude_zoom_scene_object(_s, _a);
    const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _b);
    f64 _n[5] = { (f64)_kind, 0.0, 0.0, 0.0, 0.0 };
    fude_zoom_id_put(&_n[1], _oa->id);
    fude_zoom_id_put(&_n[3], _ob->id);
    const fude_zoom_v2 _at = { (_oa->box.min_x + _oa->box.max_x + _ob->box.min_x + _ob->box.max_x) * 0.25, (_oa->box.min_y + _oa->box.max_y + _ob->box.min_y + _ob->box.max_y) * 0.25 };
    const u16 _was = _s->layer;
    _s->layer = _oa->layer;
    const u32 _c = fude_zoom_scene_add_shape(_s, _oa->frame, (fude_zoom_place){ _at, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_CONSTRAINT, _n, 5u, _oa->color, 0.0f, 0u, 0);
    _s->layer = _was;
    return _c;
}

// The constraints' passes of fude_zoom_connect_follow: up to six over _constraints (_count live ones between two
// lines, in the walk's order), each moving the line kept so with one in _now (a byte for each of the scene's _all
// objects) and marking it there. _at holds where each object is in _objects (its last place there, as a search from the
// front kept the last one found; FUDE_ZOOM_NONE: not there), kept in step as lines are added. Returns how many were
// added to _objects.
RDE_INTERNAL u32 fude_zoom_connect_hold(fude_zoom_scene* _s, const u32* _constraints, u32 _count, u8* _now, u32 _all, rde_arr* _objects, rde_arr* _before,
                                        rde_arr* _after) {
    rde_arr TYPE(u32) _at_arr = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_at_arr, _all);
    u32* _at = (u32*)_at_arr.memory;
    rde_memset(_at, 0xFF, (usize)_all * sizeof(u32));   // (FUDE_ZOOM_NONE)
    for(u32 _k = 0; _k < (u32)rde_arr_length(_objects); _k++) {
        const u32 _o = ((const u32*)_objects->memory)[_k];
        if(_o < _all) {
            _at[_o] = _k;
        }
    }
    u32 _made = 0;
    for(u32 _pass = 0; _pass < 6u; _pass++) {
        b8 _changed = false;
        for(u32 _c = 0; _c < _count; _c++) {
            u8  _kind;
            u32 _a, _b;
            // (as when it was noted: moves change none of it; checked anyway, so a line no longer one is passed by)
            if(!fude_zoom_connect_constraint_of(_s, _constraints[_c], &_kind, &_a, &_b) || _a >= _all || _b >= _all) {
                continue;
            }
            const b8 _am = _now[_a] != 0u, _bm = _now[_b] != 0u;
            fude_zoom_sim _m;
            if(_am == _bm || !fude_zoom_connect_constrain(_s, _kind, _am ? _a : _b, _am ? _b : _a, &_m)) {
                continue;
            }
            const u32 _other = _am ? _b : _a;
            const fude_zoom_place _place = fude_zoom_scene_place_of(_s, _other);
            const fude_zoom_place _next  = fude_zoom_place_moved(_place, _m);
            // (already followed in this step: its place after it, changed; else a new one)
            if(_at[_other] != FUDE_ZOOM_NONE) {
                ((fude_zoom_place*)_after->memory)[_at[_other]] = _next;
            } else {
                _at[_other] = (u32)rde_arr_length(_objects);
                rde_arr_add(_objects, (any)&_other);
                rde_arr_add(_before, (any)&_place);
                rde_arr_add(_after, (any)&_next);
                _made++;
            }
            fude_zoom_scene_set_place(_s, _other, _next);
            _now[_other] = 1u;
            _changed     = true;
        }
        if(!_changed) {
            break;
        }
    }
    rde_arr_free(&_at_arr);
    return _made;
}

// Returns whether _object is marked in _set, a byte for each of the scene's _all objects (FUDE_ZOOM_NONE never is).
RDE_INTERNAL b8 fude_zoom_connect_marked(const u8* _set, u32 _all, u32 _object) {
    return _object < _all && _set[_object] != 0u;
}

// See the header. One walk over the frame's objects moves the dimensions and connectors joined to what moved, and
// notes its constraints; then up to six passes over those constraints move each line kept so with one that has moved,
// so a chain of constraints is followed up to six links. A line already followed in this step keeps its first place
// before, and only its place after changes.
// The walk is the frame's arrival order: its objects by index (objects are only ever added, each to its frame's
// arrival, and a frame's objects stay in it), the order every object of the scene was gone through in before, so the
// changes come in the same order. What has moved is a byte an object (_now: the moved, and each line as it follows),
// and where an object is in _objects a number an object (fude_zoom_connect_hold), so neither is a search through a
// list: the walk is O(the frame's objects), a pass O(its constraints). Both tables are the size of the scene's
// objects, made for this call. An entry of _moved that is no object (FUDE_ZOOM_NONE) moves nothing.
u32 fude_zoom_connect_follow(fude_zoom_scene* _s, u32 _frame, const u32* _moved, u32 _count, rde_arr* _objects, rde_arr* _before, rde_arr* _after) {
    if(_count == 0 || _frame >= fude_zoom_scene_frame_count(_s)) {
        return 0;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    const u32             _all  = fude_zoom_scene_object_count(_s);
    rde_arr TYPE(u8) _now_arr   = rde_arr_new(sizeof(u8), _heap);
    rde_arr_resize(&_now_arr, _all);
    u8* _now = (u8*)_now_arr.memory;
    for(u32 _m = 0; _m < _count; _m++) {
        if(_moved[_m] < _all) {
            _now[_moved[_m]] = 1u;
        }
    }
    rde_arr TYPE(u32) _constraints = rde_arr_new(sizeof(u32), _heap);
    u32 _made = 0;
    for(u32 _w = 0; _w < (u32)rde_arr_length(&fude_zoom_scene_frame(_s, _frame)->arrival); _w++) {
        const u32 _i = ((const u32*)fude_zoom_scene_frame(_s, _frame)->arrival.memory)[_w];
        f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
        u32 _nc, _k;
        fude_zoom_id _from, _to;
        // A dimension joined to something moved (not moved itself): its ends where their points are now.
        fude_zoom_connect_dim _dim;
        if(fude_zoom_connect_dim_of(_s, _i, &_dim)) {
            const u32 _objs[2] = { _dim.id[0] != 0 ? fude_zoom_scene_find_object(_s, _dim.id[0]) : FUDE_ZOOM_NONE,
                                   _dim.id[1] != 0 ? fude_zoom_scene_find_object(_s, _dim.id[1]) : FUDE_ZOOM_NONE };
            const b8 _touched = fude_zoom_connect_marked(_now, _all, _objs[0]) || fude_zoom_connect_marked(_now, _all, _objs[1]);
            fude_zoom_v2 _a, _b, _a2, _b2;
            fude_zoom_sim _m;
            if(!_touched || _now[_i]) {
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
        // A constraint between two lines there: for the passes below.
        u8  _kind;
        u32 _la, _lb;
        if(fude_zoom_connect_constraint_of(_s, _i, &_kind, &_la, &_lb)) {
            if(_la != FUDE_ZOOM_NONE && _lb != FUDE_ZOOM_NONE) {
                rde_arr_add(&_constraints, (any)&_i);
            }
            continue;
        }
        if(!fude_zoom_connect_numbers(_s, _i, _n, &_nc, &_k, &_from, &_to) || (_from == 0 && _to == 0)) {
            continue;
        }
        // Joined to something moved (and not moved itself)?
        const u32 _a_obj = _from != 0 ? fude_zoom_scene_find_object(_s, _from) : FUDE_ZOOM_NONE;
        const u32 _b_obj = _to != 0 ? fude_zoom_scene_find_object(_s, _to) : FUDE_ZOOM_NONE;
        if(!(fude_zoom_connect_marked(_now, _all, _a_obj) || fude_zoom_connect_marked(_now, _all, _b_obj)) || _now[_i]) {
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
        const f64 _old = rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y), _new = rde_math_hypot_f64(_b2.x - _a2.x, _b2.y - _a2.y);
        if(!(_old > 0.0) || !(_new > 0.0)) {
            continue;
        }
        // The similarity taking a to a2 and b to b2: turned, scaled, slid.
        const f64 _turn = rde_math_atan2_f64(_b2.y - _a2.y, _b2.x - _a2.x) - rde_math_atan2_f64(_b.y - _a.y, _b.x - _a.x);
        const f64 _sc   = _new / _old;
        const f64 _ca   = rde_math_cos_f64(_turn) * _sc, _sa = rde_math_sin_f64(_turn) * _sc;
        const fude_zoom_sim _m = { _ca, _sa, _a2.x - (_ca * _a.x - _sa * _a.y), _a2.y - (_sa * _a.x + _ca * _a.y) };
        const fude_zoom_place _next = fude_zoom_place_moved(_place, _m);
        rde_arr_add(_objects, (any)&_i);
        rde_arr_add(_before, (any)&_place);
        rde_arr_add(_after, (any)&_next);
        fude_zoom_scene_set_place(_s, _i, _next);
        _made++;
    }
    const u32 _nk = (u32)rde_arr_length(&_constraints);
    if(_nk > 0u) {
        _made += fude_zoom_connect_hold(_s, (const u32*)_constraints.memory, _nk, _now, _all, _objects, _before, _after);
    }
    rde_arr_free(&_constraints);
    rde_arr_free(&_now_arr);
    return _made;
}

// See the header. Its numbers: 2 points (0, 0 and _b - _a, from _a), its heads, and the two ids in two halves each
// (fude_zoom_connect_arrow_put).
u32 fude_zoom_connect_add(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _a, fude_zoom_v2 _b, u32 _from, u32 _to, u32 _heads, rde_color _color, f32 _radius) {
    f64 _n[FUDE_ZOOM_ARROW_NUMBERS(2u)];
    _n[0] = 2.0;
    _n[1] = 0.0; _n[2] = 0.0;
    _n[3] = _b.x - _a.x; _n[4] = _b.y - _a.y;
    _n[5] = (f64)_heads;
    const fude_zoom_id _fa = _from != FUDE_ZOOM_NONE ? fude_zoom_scene_object(_s, _from)->id : 0u;
    const fude_zoom_id _tb = _to != FUDE_ZOOM_NONE ? fude_zoom_scene_object(_s, _to)->id : 0u;
    fude_zoom_connect_arrow_put(_n, 2u, _fa, _tb);
    return fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ _a, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ARROW, _n, FUDE_ZOOM_ARROW_NUMBERS(2u), _color, _radius, 0u, 0);
}

// See the header. A layer other than its own also counts as a change. It is made with the scene's style 0: no line
// style.
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
    fude_zoom_connect_arrow_put(_num, _k, _from, _to);   // (an older one's: two numbers more)
    _count = _count < FUDE_ZOOM_ARROW_NUMBERS(_k) ? FUDE_ZOOM_ARROW_NUMBERS(_k) : _count;
    const fude_zoom_object _o = *fude_zoom_scene_object(_s, _arrow);
    const u16 _was = _s->layer;
    _s->layer = _layer;
    _s->style = 0;
    const u32 _made = fude_zoom_scene_add_shape_fill(_s, _o.frame, fude_zoom_scene_place_of(_s, _arrow), FUDE_ZOOM_SHAPE_ARROW, _num, _count,
                                                     _o.color, _o.radius, _o.flags, _o.fill, _o.z);
    _s->layer = _was;
    return _made;
}

// See the header. The frame's order is copied first, since each object made again joins it while the loop runs. A
// line or a dimension with both ends at _from is left alone; a rectangle flattened to nothing by the move is too.
u32 fude_zoom_connect_drive(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _from, fude_zoom_v2 _to, f64 _tol, u32 _skip, rde_arr* _died, rde_arr* _born) {
    // Its objects as they are now (the new ones go in as we go).
    rde_memory_allocator* _heap  = rde_memory_allocator_get_default_std();
    const rde_arr*        _order = &fude_zoom_scene_frame(_s, _frame)->order;
    const u32             _count = (u32)rde_arr_length(_order);
    if(_count == 0) {
        return 0;
    }
    rde_arr TYPE(u32) _list_arr = rde_arr_new(sizeof(u32), _heap);
    u32* _list = (u32*)rde_arr_add_n(&_list_arr, _count);   // (sized once: it stays put)
    rde_memcpy(_list, _order->memory, (usize)_count * sizeof(u32));
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
        // Whether the object's own point (_x, _y) is within _tol of _from, in the frame.
#define FUDE_ZOOM_DRIVE_AT(_x, _y) (rde_math_hypot_f64(fude_zoom_sim_apply(_sim, (fude_zoom_v2){ (_x), (_y) }).x - _from.x, fude_zoom_sim_apply(_sim, (fude_zoom_v2){ (_x), (_y) }).y - _from.y) <= _tol)
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
                    const f64 _hw = rde_math_abs_f64(_p.x - _across.x) * 0.5, _hh = rde_math_abs_f64(_p.y - _across.y) * 0.5;
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
    rde_arr_free(&_list_arr);
    return _made;
}
