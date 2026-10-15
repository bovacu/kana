// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/circuit_parts.h"

// ===========================================================================
// See circuit.h. Circuits, wires: their numbers (shape.h's WIRE: its points, and the part and pin at each end),
// routed squarely from pin to pin, following their parts as they move, made again joined to other parts, and let go
// when what they joined dies.
//
// The file is in three parts: wires (their numbers written and read, the part an end keeps, a route, a wire added, a
// pin's side and its place in another frame, wires following moved parts, a wire joined to new parts); a grid of
// segments (fzc_seg_grid, which the build uses too); and wires left joined to nothing (fude_zoom_wire_orphans), whose
// note at its head says how they are found.
//
// Part of the circuit: circuit_parts.h holds what the modules share and says which module holds what. Prefix fzc_ and
// FZC_. Main thread only.
// ===========================================================================

// --- wires ------------------------------------------------------------------------------------------

// See the header: the count, each point relative to the first, then each end's id (two numbers: fude_zoom_id_put)
// and pin. Points past FUDE_ZOOM_WIRE_POINTS are dropped.
u32 fude_zoom_wire_numbers(f64* _n, const fude_zoom_v2* _points, u32 _count, fude_zoom_id _from, i32 _from_pin, fude_zoom_id _to, i32 _to_pin) {
    _count = _count < FUDE_ZOOM_WIRE_POINTS ? _count : FUDE_ZOOM_WIRE_POINTS;
    _n[0] = (f64)_count;
    for(u32 _i = 0; _i < _count; _i++) {
        _n[1u + 2u * _i] = _points[_i].x - _points[0].x;
        _n[2u + 2u * _i] = _points[_i].y - _points[0].y;
    }
    u32 _k = 1u + 2u * _count;
    fude_zoom_id_put(&_n[_k], _from);
    _k += 2u;
    _n[_k++] = (f64)_from_pin;
    fude_zoom_id_put(&_n[_k], _to);
    _k += 2u;
    _n[_k++] = (f64)_to_pin;
    return _k;
}

// Returns a wire end's pin as its number says, or -1 (none) when the number is not one an i32 holds from -1 up:
// negative, too big or not a number, as a damaged file's may be (casting such a one would be undefined).
RDE_INTERNAL i32 fzc_pin_number(f64 _x) {
    return _x >= -1.0 && _x < 2147483648.0 ? (i32)_x : -1;
}

// Returns an old save's id, written as one number, or 0 (none) when that number is not one a fude_zoom_id holds above
// 0 (a damaged file's: negative, too big, not a number).
RDE_INTERNAL fude_zoom_id fzc_old_id(f64 _x) {
    return _x > 0.0 && _x < 18446744073709551616.0 ? (fude_zoom_id)_x : 0u;
}

// See the header. Numbers enough for two-number ids (7 + 2k) are read as now; for one-number ids (5 + 2k), as an old
// save wrote them; fewer, as a wire with no ends kept. Every number is checked before it is cast (the point count, the
// pins, an old id): a damaged file's is read as no wire, or as no pin or no part at an end.
u32 fude_zoom_wire_of(const f64* _n, u32 _count, fude_zoom_v2* _points, fude_zoom_id* _from, i32* _from_pin, fude_zoom_id* _to, i32* _to_pin) {
    if(_count < 1u || !(_n[0] >= 2.0 && _n[0] <= (f64)FUDE_ZOOM_WIRE_POINTS)) {
        return 0;
    }
    const u32 _k = (u32)_n[0];
    if(_count < 1u + 2u * _k) {
        return 0;
    }
    for(u32 _i = 0; _i < _k && _points != NULL; _i++) {
        _points[_i] = (fude_zoom_v2){ _n[1u + 2u * _i], _n[2u + 2u * _i] };
    }
    const f64* _r = &_n[1u + 2u * _k];
    if(_count >= 7u + 2u * _k) {
        // Each id its two halves.
        if(_from != NULL)     { *_from     = fude_zoom_id_get(&_r[0]); }
        if(_from_pin != NULL) { *_from_pin = fzc_pin_number(_r[2]); }
        if(_to != NULL)       { *_to       = fude_zoom_id_get(&_r[3]); }
        if(_to_pin != NULL)   { *_to_pin   = fzc_pin_number(_r[5]); }
        return _k;
    }
    const b8 _refs = _count >= 5u + 2u * _k;   // An old save's: an id as one number.
    if(_from != NULL)     { *_from     = _refs ? fzc_old_id(_r[0]) : 0u; }
    if(_from_pin != NULL) { *_from_pin = _refs ? fzc_pin_number(_r[1]) : -1; }
    if(_to != NULL)       { *_to       = _refs ? fzc_old_id(_r[2]) : 0u; }
    if(_to_pin != NULL)   { *_to_pin   = _refs ? fzc_pin_number(_r[3]) : -1; }
    return _k;
}

// See the header. The id is looked up first; an old save's rounded id falls back to searching every object.
u32 fude_zoom_wire_part(const fude_zoom_scene* _s, fude_zoom_id _id, i32 _pin, u32 _frame, fude_zoom_v2 _end) {
    if(_id == 0u || _pin < 0) {
        return FUDE_ZOOM_NONE;
    }
    const u32 _o = fude_zoom_scene_find_object(_s, _id);
    if(_o != FUDE_ZOOM_NONE) {
        return _o;
    }
    // An old save's id, rounded as a number: of the parts whose ids round so, the one whose pin is nearest that end.
    const f64 _rounded = (f64)_id;
    u32 _best = FUDE_ZOOM_NONE;
    f64 _least = 1e300;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _i);
        if(!(_ob->flags & FUDE_ZOOM_FLAG_ALIVE) || _ob->kind != FUDE_ZOOM_KIND_SHAPE || _ob->channels != FUDE_ZOOM_SHAPE_SYMBOL || (f64)_ob->id != _rounded) {
            continue;
        }
        fude_zoom_v2 _p;
        if(fude_zoom_part_of(_s, _i) != NULL && fude_zoom_part_pin_in(_s, _i, (u32)_pin, _frame, &_p, NULL, NULL)) {
            const f64 _d = rde_math_hypot_f64(_p.x - _end.x, _p.y - _end.y);
            if(_d < _least) {
                _least = _d;
                _best  = _i;
            }
        }
    }
    return _best;
}

// See the header. The route is _a, its stub's end, one corner, _b's stub's end, _b; then points that add nothing are
// dropped. _way is each FUDE_ZOOM_PIN_SIDE_'s direction.
u32 fude_zoom_wire_route(fude_zoom_v2 _a, u8 _side_a, fude_zoom_v2 _b, u8 _side_b, f64 _stub, fude_zoom_v2* _out) {
    static const f64 _way[4][2] = { { -1, 0 }, { 1, 0 }, { 0, 1 }, { 0, -1 } };
    fude_zoom_v2 _p[6];
    u32 _n = 0;
    _p[_n++] = _a;
    const b8 _sa = _side_a <= FUDE_ZOOM_PIN_DOWN, _sb = _side_b <= FUDE_ZOOM_PIN_DOWN;
    const fude_zoom_v2 _a1 = _sa ? (fude_zoom_v2){ _a.x + _way[_side_a][0] * _stub, _a.y + _way[_side_a][1] * _stub } : _a;
    const fude_zoom_v2 _b1 = _sb ? (fude_zoom_v2){ _b.x + _way[_side_b][0] * _stub, _b.y + _way[_side_b][1] * _stub } : _b;
    if(_sa) {
        _p[_n++] = _a1;
    }
    // Between the stubs' ends: across first when that goes on the way out of _a (or, leaving either way, the longer
    // way).
    const f64 _dx = _b1.x - _a1.x, _dy = _b1.y - _a1.y;
    b8 _across;
    if(_sa) {
        const b8 _horiz = _side_a == FUDE_ZOOM_PIN_LEFT || _side_a == FUDE_ZOOM_PIN_RIGHT;
        const f64 _on = _horiz ? _way[_side_a][0] * _dx : _way[_side_a][1] * _dy;   // How far on its own way b lies.
        _across = _horiz ? _on > 0.0 : !(_on > 0.0);
    } else if(_sb) {
        const b8 _horiz = _side_b == FUDE_ZOOM_PIN_LEFT || _side_b == FUDE_ZOOM_PIN_RIGHT;
        _across = !_horiz;   // Into b along its own way last.
    } else {
        _across = rde_math_abs_f64(_dx) >= rde_math_abs_f64(_dy);
    }
    _p[_n++] = _across ? (fude_zoom_v2){ _b1.x, _a1.y } : (fude_zoom_v2){ _a1.x, _b1.y };
    if(_sb) {
        _p[_n++] = _b1;
    }
    _p[_n++] = _b;
    // Points that add nothing: the same as the one before, or on a straight line through.
    const f64 _eps = 1e-9 * rde_math_max_f64(rde_math_max_f64(rde_math_abs_f64(_a.x), rde_math_abs_f64(_a.y)), rde_math_max_f64(rde_math_max_f64(rde_math_abs_f64(_b.x), rde_math_abs_f64(_b.y)), 1.0));
    u32 _m = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        if(_m > 0u && rde_math_abs_f64(_p[_i].x - _out[_m - 1u].x) <= _eps && rde_math_abs_f64(_p[_i].y - _out[_m - 1u].y) <= _eps) {
            continue;
        }
        if(_m >= 2u) {
            const fude_zoom_v2 _u = _out[_m - 2u], _v = _out[_m - 1u];
            const f64 _cross = (_v.x - _u.x) * (_p[_i].y - _v.y) - (_v.y - _u.y) * (_p[_i].x - _v.x);
            const f64 _dot   = (_v.x - _u.x) * (_p[_i].x - _v.x) + (_v.y - _u.y) * (_p[_i].y - _v.y);
            if(rde_math_abs_f64(_cross) <= _eps * (rde_math_abs_f64(_v.x - _u.x) + rde_math_abs_f64(_v.y - _u.y) + rde_math_abs_f64(_p[_i].x - _v.x) + rde_math_abs_f64(_p[_i].y - _v.y)) && _dot >= 0.0) {
                _out[_m - 1u] = _p[_i];   // Straight on: the middle point goes.
                continue;
            }
        }
        _out[_m++] = _p[_i];
    }
    if(_m < 2u) {
        _out[0] = _a;
        _out[1] = _b;
        _m = 2u;
    }
    return _m;
}

// See the header: a WIRE shape at its first point, its ends kept by their parts' ids (none: 0 and -1).
u32 fude_zoom_wire_add(fude_zoom_scene* _s, u32 _frame, const fude_zoom_v2* _points, u32 _count, u32 _from, i32 _from_pin, u32 _to, i32 _to_pin,
                       rde_color _color, f32 _radius) {
    f64 _n[FUDE_ZOOM_WIRE_NUMBERS];
    const fude_zoom_id _fa = _from != FUDE_ZOOM_NONE ? fude_zoom_scene_object(_s, _from)->id : 0u;
    const fude_zoom_id _tb = _to != FUDE_ZOOM_NONE ? fude_zoom_scene_object(_s, _to)->id : 0u;
    const u32 _k = fude_zoom_wire_numbers(_n, _points, _count, _fa, _from != FUDE_ZOOM_NONE ? _from_pin : -1, _tb, _to != FUDE_ZOOM_NONE ? _to_pin : -1);
    return fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ _points[0], 0.0, 1.0 }, FUDE_ZOOM_SHAPE_WIRE, _n, _k, _color, _radius, 0u, 0);
}

// See the header: the pin's side turned by the object's rotation, taken to the nearest of the four.
u8 fude_zoom_part_side_at(const fude_zoom_scene* _s, u32 _object, u32 _pin) {
    const fude_zoom_part* _p = fude_zoom_part_of(_s, _object);
    fude_zoom_pin _q;
    if(_p == NULL || !fude_zoom_part_pin(_p, _pin, &_q) || _q.side > FUDE_ZOOM_PIN_DOWN) {
        return 255u;
    }
    static const f64 _way[4][2] = { { -1, 0 }, { 1, 0 }, { 0, 1 }, { 0, -1 } };
    const f64 _r = fude_zoom_scene_object(_s, _object)->rotation;
    const f64 _x = _way[_q.side][0] * rde_math_cos_f64(_r) - _way[_q.side][1] * rde_math_sin_f64(_r), _y = _way[_q.side][0] * rde_math_sin_f64(_r) + _way[_q.side][1] * rde_math_cos_f64(_r);
    return rde_math_abs_f64(_x) >= rde_math_abs_f64(_y) ? (_x < 0.0 ? FUDE_ZOOM_PIN_LEFT : FUDE_ZOOM_PIN_RIGHT) : (_y > 0.0 ? FUDE_ZOOM_PIN_UP : FUDE_ZOOM_PIN_DOWN);
}

// See the header. The side is turned again by how _frame is turned against the part's frame.
b8 fude_zoom_part_pin_in(const fude_zoom_scene* _s, u32 _object, u32 _pin, u32 _frame, fude_zoom_v2* _at, u8* _side, f64* _stub) {
    fude_zoom_v2 _p;
    if(!fude_zoom_part_pin_at(_s, _object, _pin, &_p)) {
        return false;
    }
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    const fude_zoom_sim _to = fude_zoom_scene_sim(_s, _o->frame, _frame);
    *_at = fude_zoom_sim_apply(_to, _p);
    if(_side != NULL) {
        u8 _sd = fude_zoom_part_side_at(_s, _object, _pin);
        const f64 _turn = rde_math_atan2_f64(_to.b, _to.a);
        if(_sd <= FUDE_ZOOM_PIN_DOWN && rde_math_abs_f64(_turn) > 1e-6) {
            // That frame turned against the part's: the side as it faces there.
            static const f64 _way[4][2] = { { -1, 0 }, { 1, 0 }, { 0, 1 }, { 0, -1 } };
            const f64 _x = _way[_sd][0] * rde_math_cos_f64(_turn) - _way[_sd][1] * rde_math_sin_f64(_turn), _y = _way[_sd][0] * rde_math_sin_f64(_turn) + _way[_sd][1] * rde_math_cos_f64(_turn);
            _sd = rde_math_abs_f64(_x) >= rde_math_abs_f64(_y) ? (_x < 0.0 ? FUDE_ZOOM_PIN_LEFT : FUDE_ZOOM_PIN_RIGHT) : (_y > 0.0 ? FUDE_ZOOM_PIN_UP : FUDE_ZOOM_PIN_DOWN);
        }
        *_side = _sd;
    }
    if(_stub != NULL) {
        // A tenth of a resistor's length: the parts' grid (fude_zoom_part_unit), in _frame's units.
        f64 _n3[3];
        const fude_zoom_symbol_info* _info = fude_zoom_scene_shape_numbers(_s, _object, _n3, 3u) >= 3u ? fude_zoom_symbol_info_of((u32)_n3[0]) : NULL;
        const f64 _room = _info != NULL ? fude_zoom_part_room_h(fude_zoom_part_of(_s, _object), (f64)_info->h) : 0.0;
        *_stub = _room > 0.0 ? _n3[2] * _o->scale / (_room * 0.5) * 10.0 * fude_zoom_sim_scale(_to) : 0.0;
    }
    return true;
}

// Makes _wire again: its ends at the pins of the parts _from and _to now are (found by id, as fude_zoom_wire_part
// finds them), wherever those parts are, routed square, on the old wire's layer, in its colour and width. Returns the
// new wire, or FUDE_ZOOM_NONE when the old one has no points; the old one is left as it was (the caller lets it go).
RDE_INTERNAL u32 fzc_wire_again(fude_zoom_scene* _s, u32 _wire, fude_zoom_id _from, i32 _from_pin, fude_zoom_id _to, i32 _to_pin) {
    const fude_zoom_object _o = *fude_zoom_scene_object(_s, _wire);
    f64 _n[FUDE_ZOOM_WIRE_NUMBERS];
    const u32 _count = fude_zoom_scene_shape_numbers(_s, _wire, _n, FUDE_ZOOM_WIRE_NUMBERS);
    fude_zoom_v2 _p[FUDE_ZOOM_WIRE_POINTS];
    const u32 _k = fude_zoom_wire_of(_n, _count, _p, NULL, NULL, NULL, NULL);
    if(_k < 2u) {
        return FUDE_ZOOM_NONE;
    }
    const fude_zoom_sim _sim = fude_zoom_object_sim(&_o);
    for(u32 _i = 0; _i < _k; _i++) {
        _p[_i] = fude_zoom_sim_apply(_sim, _p[_i]);
    }
    const u32 _a = fude_zoom_wire_part(_s, _from, _from_pin, _o.frame, _p[0]);
    const u32 _b = fude_zoom_wire_part(_s, _to, _to_pin, _o.frame, _p[_k - 1u]);
    fude_zoom_v2 _pa = _p[0], _pb = _p[_k - 1u];
    // Each end at its pin wherever its part is: in the wire's frame, or another, deeper or shallower.
    u8 _sa = 255u, _sb = 255u;
    f64 _stub_a = 0.0, _stub_b = 0.0;
    const b8 _ha = _a != FUDE_ZOOM_NONE && _from_pin >= 0 && fude_zoom_part_pin_in(_s, _a, (u32)_from_pin, _o.frame, &_pa, &_sa, &_stub_a);
    const b8 _hb = _b != FUDE_ZOOM_NONE && _to_pin >= 0 && fude_zoom_part_pin_in(_s, _b, (u32)_to_pin, _o.frame, &_pb, &_sb, &_stub_b);
    fude_zoom_v2 _r[6];
    const f64 _stub = _ha ? _stub_a : _stub_b;
    const u32 _m = fude_zoom_wire_route(_pa, _ha ? _sa : 255u, _pb, _hb ? _sb : 255u, _stub, _r);
    const u16 _layer = _s->layer;
    _s->layer = _o.layer;
    const u32 _made = fude_zoom_wire_add(_s, _o.frame, _r, _m, _ha ? _a : FUDE_ZOOM_NONE, _from_pin, _hb ? _b : FUDE_ZOOM_NONE, _to_pin, _o.color, _o.radius);
    _s->layer = _layer;
    return _made;
}

// See the header. Every wire on the canvas is looked at; one moved whole with both its parts (or its free ends) is left
// as it is. What moved is marked once, a byte an object, so each wire asks of its parts at once rather than going
// through every object moved (the wires made here, past the objects there were, are none of them).
u32 fude_zoom_wire_follow(fude_zoom_scene* _s, const u32* _moved, u32 _count, rde_arr* _died, rde_arr* _born) {
    u32 _made = 0;
    const u32 _all = fude_zoom_scene_object_count(_s);
    rde_arr TYPE(u8) _moved_arr = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_moved_arr, _all);
    u8* _is_moved = (u8*)_moved_arr.memory;
    rde_memset(_is_moved, 0, _all);
    for(u32 _m = 0; _m < _count; _m++) {
        if(_moved[_m] < _all) {
            _is_moved[_moved[_m]] = 1u;
        }
    }
    for(u32 _i = 0; _i < _all; _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_WIRE) {
            continue;
        }
        f64 _n[FUDE_ZOOM_WIRE_NUMBERS];
        const u32 _c = fude_zoom_scene_shape_numbers(_s, _i, _n, FUDE_ZOOM_WIRE_NUMBERS);
        fude_zoom_id _from, _to;
        i32 _fp, _tp;
        fude_zoom_v2 _ends[FUDE_ZOOM_WIRE_POINTS];
        const u32 _k = fude_zoom_wire_of(_n, _c, _ends, &_from, &_fp, &_to, &_tp);
        if(_k < 2u) {
            continue;
        }
        // The parts its ends keep: by their ids (an old save's, by their pins where its ends are).
        const fude_zoom_sim _ws = fude_zoom_object_sim(_o);
        const u32 _pa = fude_zoom_wire_part(_s, _from, _fp, _o->frame, fude_zoom_sim_apply(_ws, _ends[0]));
        const u32 _pb = fude_zoom_wire_part(_s, _to, _tp, _o->frame, fude_zoom_sim_apply(_ws, _ends[_k - 1u]));
        const b8 _from_moved = _pa != FUDE_ZOOM_NONE && _pa < _all && _is_moved[_pa];
        const b8 _to_moved   = _pb != FUDE_ZOOM_NONE && _pb < _all && _is_moved[_pb];
        const b8 _self       = _is_moved[_i] != 0u;
        // Moved itself: whole with its parts; or, joined to one left where it was, made again to it.
        const b8 _whole = _self && (_from == 0u || _from_moved) && (_to == 0u || _to_moved);
        if(_whole || (!_self && !_from_moved && !_to_moved)) {
            continue;
        }
        const u32 _w = fzc_wire_again(_s, _i, _from, _fp, _to, _tp);
        if(_w != FUDE_ZOOM_NONE) {
            fude_zoom_scene_set_alive(_s, _i, false);
            rde_arr_add(_died, (any)&_i);
            rde_arr_add(_born, (any)&_w);
            _made++;
        }
    }
    rde_arr_free(&_moved_arr);
    return _made;
}

// --- a grid of segments (circuit_parts.h: fzc_seg_grid) ------------------------------------------------------------
//
// What lies within reach of a point is found by looking in that point's cell alone: each segment is put in every cell
// its reach covers, the cells as wide as a segment and its reach are long on average (so a segment is in a few), a
// cell's entries together in one list sorted by cell, found by an rde_hash_map from a cell to its first. A segment that
// would cover more than FZC_GRID_CELLS cells is tried for every point instead. Wires left joined to nothing
// (fude_zoom_wire_orphans) and the circuit's build (circuit_build.c: wires in scope, junctions, leads) use it.

#define FZC_GRID_CELLS 1024   // the most grid cells a segment is put in; one covering more is tried for every point

// Returns the grid cell coordinate _v (home units) is in, for cells _cell wide, held to 32 bits.
RDE_INTERNAL i64 fzc_cell_of(f64 _v, f64 _cell) {
    const f64 _c = rde_math_floor_f64(_v / _cell);
    return _c != _c ? 0 : (i64)rde_math_min_f64(rde_math_max_f64(_c, -2147483648.0), 2147483647.0);
}

// Returns grid cell (_x, _y)'s key: its two coordinates' 32 bits side by side.
RDE_INTERNAL u64 fzc_cell_key(i64 _x, i64 _y) {
    return ((u64)(u32)(i32)_x << 32) | (u64)(u32)(i32)_y;
}

// The order of grid entries: by cell, then by segment (a wire's segments together in a cell).
RDE_INTERNAL int fzc_cell_seg_order(const any _a, const any _b) {
    const fzc_cell_seg* _x = (const fzc_cell_seg*)_a;
    const fzc_cell_seg* _y = (const fzc_cell_seg*)_b;
    return _x->cell != _y->cell ? (_x->cell < _y->cell ? -1 : 1) : (_x->seg < _y->seg ? -1 : (_x->seg > _y->seg ? 1 : 0));
}

// The order of u32s, for rde_sort.
RDE_INTERNAL int fzc_u32_order(const any _a, const any _b) {
    const u32 _x = *(const u32*)_a, _y = *(const u32*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// See circuit_parts.h. Every array on the std allocator; with no segments, an empty grid (every point finds nothing).
void fzc_grid_make(fzc_seg_grid* _g, const fzc_seg* _segs, u32 _n) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _g->in    = rde_arr_new(sizeof(fzc_cell_seg), _heap);
    _g->wide  = rde_arr_new(sizeof(u32), _heap);
    _g->first = rde_hash_map_new(sizeof(u64), sizeof(u32), rde_hash_map_fn_u64_hash, rde_hash_map_fn_u64_cmp, NULL, _heap);
    f64 _size = 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        _size += rde_math_max_f64(rde_math_abs_f64(_segs[_i].b.x - _segs[_i].a.x), rde_math_abs_f64(_segs[_i].b.y - _segs[_i].a.y)) + 2.0 * _segs[_i].tol;
    }
    _g->cell = _n > 0u ? rde_math_max_f64(_size / (f64)_n, 1e-12) : 1.0;
    _g->cell = rde_math_is_nan_f64(_g->cell) ? 1.0 : _g->cell;   // (not a number, from points that are not: one size)
    for(u32 _i = 0; _i < _n; _i++) {
        const fzc_seg* _sg = &_segs[_i];
        const i64 _x0 = fzc_cell_of(rde_math_min_f64(_sg->a.x, _sg->b.x) - _sg->tol, _g->cell), _x1 = fzc_cell_of(rde_math_max_f64(_sg->a.x, _sg->b.x) + _sg->tol, _g->cell);
        const i64 _y0 = fzc_cell_of(rde_math_min_f64(_sg->a.y, _sg->b.y) - _sg->tol, _g->cell), _y1 = fzc_cell_of(rde_math_max_f64(_sg->a.y, _sg->b.y) + _sg->tol, _g->cell);
        if(_x1 - _x0 >= (i64)FZC_GRID_CELLS || _y1 - _y0 >= (i64)FZC_GRID_CELLS || (_x1 - _x0 + 1) * (_y1 - _y0 + 1) > (i64)FZC_GRID_CELLS) {
            rde_arr_add(&_g->wide, (any)&_i);
            continue;
        }
        for(i64 _x = _x0; _x <= _x1; _x++) {
            for(i64 _y = _y0; _y <= _y1; _y++) {
                const fzc_cell_seg _cs = { fzc_cell_key(_x, _y), _i };
                rde_arr_add(&_g->in, (any)&_cs);
            }
        }
    }
    const u32 _nc = (u32)rde_arr_length(&_g->in);
    rde_sort(_g->in.memory, _nc, sizeof(fzc_cell_seg), fzc_cell_seg_order);
    const fzc_cell_seg* _c = (const fzc_cell_seg*)_g->in.memory;
    for(u32 _i = 0; _i < _nc; _i++) {
        if(_i == 0u || _c[_i].cell != _c[_i - 1u].cell) {
            rde_hash_map_add(&_g->first, (any)&_c[_i].cell, (any)&_i);
        }
    }
}

// See circuit_parts.h.
void fzc_grid_free(fzc_seg_grid* _g) {
    rde_hash_map_free(&_g->first);
    rde_arr_free(&_g->in);
    rde_arr_free(&_g->wide);
}

// See circuit_parts.h: the point's cell's segments, then the wide ones, sorted.
void fzc_grid_near(const fzc_seg_grid* _g, fude_zoom_v2 _at, rde_arr* _out) {
    rde_arr_clear(_out);
    const u32 _nc = (u32)rde_arr_length(&_g->in);
    const fzc_cell_seg* _c = (const fzc_cell_seg*)_g->in.memory;
    const u64 _key = fzc_cell_key(fzc_cell_of(_at.x, _g->cell), fzc_cell_of(_at.y, _g->cell));
    const u32* _start = _nc > 0u ? (const u32*)rde_hash_map_find(&_g->first, (any)&_key) : NULL;
    for(u32 _j = _start != NULL ? *_start : _nc; _j < _nc && _c[_j].cell == _key; _j++) {
        rde_arr_add(_out, (any)&_c[_j].seg);
    }
    rde_arr_add_arr(_out, &_g->wide);
    rde_sort(_out->memory, rde_arr_length(_out), sizeof(u32), fzc_u32_order);
}

// --- wires left joined to nothing (fude_zoom_wire_orphans) -------------------------------------------------------
//
// What lies on what is found once a delete, not every round: each wire alive or dying gets a slot, its segments are
// put in a grid (fzc_seg_grid, above), each loose end of a wire alive looks only at the segments near it, and each end
// counts the wires alive and dying it lies on. A wire let go then changes the counts of the ends on it alone, and those
// left on no wire alive go next.

// A loose end of a wire alive (one not at a part's pin): its wire's slot, where it is (home units), and how many wires
// alive and dying it lies on.
typedef struct {
    u32          wire;
    fude_zoom_v2 at;
    u32          live, dying;
} fzc_loose;

// An end lying on a wire: the wire's slot and the end's index.
typedef struct {
    u32 target;
    u32 end;
} fzc_touch;

// What fude_zoom_wire_orphans works with: the scene, its home frame, a flag for each object let go in this delete;
// each wire slot's object (a wire alive or dying, with a way along it) and whether it is to go; the segments (each
// owned by its wire's slot, its reach 1.5 times its wire's half width), the loose ends, and which wires each end lies
// on.
typedef struct {
    fude_zoom_scene*        s;
    u32                     home;
    u8*                     dead;
    rde_arr TYPE(u32)       wires;
    rde_arr TYPE(u8)        going;
    rde_arr TYPE(fzc_seg)   segs;
    rde_arr TYPE(fzc_loose) ends;
    rde_arr TYPE(fzc_touch) touches;
} fzc_orphans;

// The order of touches: by the wire lain on, then by end.
RDE_INTERNAL int fzc_touch_order(const any _a, const any _b) {
    const fzc_touch* _x = (const fzc_touch*)_a;
    const fzc_touch* _y = (const fzc_touch*)_b;
    return _x->target != _y->target ? (_x->target < _y->target ? -1 : 1) : (_x->end < _y->end ? -1 : (_x->end > _y->end ? 1 : 0));
}

// Gathers the wires alive or dying (each a slot, its segments in the home frame), and each loose end of a wire alive;
// a wire alive with an end at a dying part's pin is marked to go. A wire with no way along it (under two points) has no
// slot: nothing lies on it, and it never goes, as before.
RDE_INTERNAL void fzc_orphans_gather(fzc_orphans* _g) {
    fude_zoom_scene* _s = _g->s;
    const u32 _no = fude_zoom_scene_object_count(_s);
    for(u32 _w = 0; _w < _no; _w++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _w);
        if(_o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_WIRE || (!_g->dead[_w] && !(_o->flags & FUDE_ZOOM_FLAG_ALIVE))) {
            continue;
        }
        f64 _n[FUDE_ZOOM_WIRE_NUMBERS];
        const u32 _c = fude_zoom_scene_shape_numbers(_s, _w, _n, FUDE_ZOOM_WIRE_NUMBERS);
        fude_zoom_v2 _q[FUDE_ZOOM_WIRE_POINTS];
        fude_zoom_id _ids[2];
        i32 _pins[2];
        const u32 _k = fude_zoom_wire_of(_n, _c, _q, &_ids[0], &_pins[0], &_ids[1], &_pins[1]);
        if(_k < 2u) {
            continue;
        }
        const u32 _me = (u32)rde_arr_length(&_g->wires);
        const u8  _no_go = 0u;
        rde_arr_add(&_g->wires, (any)&_w);
        rde_arr_add(&_g->going, (any)&_no_go);
        const fude_zoom_sim _up  = fude_zoom_scene_sim(_s, _o->frame, _g->home);
        const fude_zoom_sim _to  = fude_zoom_sim_compose(_up, fude_zoom_object_sim(_o));
        const f64           _tol = rde_math_max_f64((f64)_o->radius * _o->scale * fude_zoom_sim_scale(_up) * 1.5, 1e-9);
        for(u32 _i = 0; _i + 1u < _k; _i++) {
            const fzc_seg _sg = { _me, fude_zoom_sim_apply(_to, _q[_i]), fude_zoom_sim_apply(_to, _q[_i + 1u]), _tol };
            rde_arr_add(&_g->segs, (any)&_sg);
        }
        if(_g->dead[_w]) {
            continue;   // (going already: only what other wires' ends may lie on)
        }
        for(u32 _e = 0; _e < 2u; _e++) {
            if(_ids[_e] != 0u && _pins[_e] >= 0) {
                const u32 _part = fude_zoom_scene_find_object(_s, _ids[_e]);
                if(_part != FUDE_ZOOM_NONE && _part < _no && _g->dead[_part]) {
                    ((u8*)_g->going.memory)[_me] = 1u;   // At a part's pin, and that part gone.
                }
                continue;
            }
            const fzc_loose _end = { _me, fude_zoom_sim_apply(_to, _q[_e == 0u ? 0u : _k - 1u]), 0u, 0u };
            rde_arr_add(&_g->ends, (any)&_end);
        }
    }
}

// Adds end _e's touch of segment _seg's wire when the end lies on it, is not its own wire's end, and has not touched
// that wire already (its touches are those from _from on).
RDE_INTERNAL void fzc_orphans_try(fzc_orphans* _g, u32 _e, u32 _from, u32 _seg) {
    const fzc_seg* _sg  = &((const fzc_seg*)_g->segs.memory)[_seg];
    const fzc_loose* _end = &((const fzc_loose*)_g->ends.memory)[_e];
    if(_sg->owner == _end->wire || !(rde_math_point_segment_distance_2d_f64(_end->at, _sg->a, _sg->b) <= _sg->tol)) {
        return;
    }
    const fzc_touch* _t = (const fzc_touch*)_g->touches.memory;
    for(u32 _i = _from; _i < (u32)rde_arr_length(&_g->touches); _i++) {
        if(_t[_i].target == _sg->owner) {
            return;
        }
    }
    const fzc_touch _new = { _sg->owner, _e };
    rde_arr_add(&_g->touches, (any)&_new);
}

// Finds the wires each loose end lies on (its touches), each end looking only at the segments near it (fzc_seg_grid),
// and counts them, alive and dying.
RDE_INTERNAL void fzc_orphans_touch(fzc_orphans* _g) {
    const u32 _ns = (u32)rde_arr_length(&_g->segs), _ne = (u32)rde_arr_length(&_g->ends);
    if(_ns == 0u || _ne == 0u) {
        return;
    }
    fzc_seg_grid _grid;
    fzc_grid_make(&_grid, (const fzc_seg*)_g->segs.memory, _ns);
    rde_arr TYPE(u32) _near = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    for(u32 _e = 0; _e < _ne; _e++) {
        const u32 _from = (u32)rde_arr_length(&_g->touches);
        fzc_grid_near(&_grid, ((const fzc_loose*)_g->ends.memory)[_e].at, &_near);
        for(u32 _j = 0; _j < (u32)rde_arr_length(&_near); _j++) {
            fzc_orphans_try(_g, _e, _from, ((const u32*)_near.memory)[_j]);
        }
    }
    rde_arr_free(&_near);
    fzc_grid_free(&_grid);
    fzc_loose* _ends = (fzc_loose*)_g->ends.memory;
    const fzc_touch* _t = (const fzc_touch*)_g->touches.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_g->touches); _i++) {
        if(_g->dead[((const u32*)_g->wires.memory)[_t[_i].target]]) {
            _ends[_t[_i].end].dying++;
        } else {
            _ends[_t[_i].end].live++;
        }
    }
}

// See the header. A wire is let go when an end kept a part that died, or a loose end lies on wires that died and on
// none alive; one let go may leave others so in turn. What lies on what is found once (fzc_orphans_touch), and a wire
// let go changes only the ends on it: those it leaves on no wire alive take their wires next, in the order found
// (first the canvas's order). The same wires go as when every end was looked at against every wire, round after round.
u32 fude_zoom_wire_orphans(fude_zoom_scene* _s, rde_arr* _died) {
    const u32 _no = fude_zoom_scene_object_count(_s);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _dead_arr = rde_arr_new(sizeof(u8), _heap);
    rde_arr_resize(&_dead_arr, _no);
    u8* _dead = (u8*)_dead_arr.memory;
    rde_memset(_dead, 0, _no);
    for(u32 _i = 0; _i < (u32)rde_arr_length(_died); _i++) {
        const u32 _o = ((const u32*)_died->memory)[_i];
        if(_o < _no) {
            _dead[_o] = 1u;
        }
    }
    fzc_orphans _g = { _s, _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root, _dead, rde_arr_new(sizeof(u32), _heap), rde_arr_new(sizeof(u8), _heap),
                       rde_arr_new(sizeof(fzc_seg), _heap), rde_arr_new(sizeof(fzc_loose), _heap), rde_arr_new(sizeof(fzc_touch), _heap) };
    fzc_orphans_gather(&_g);
    fzc_orphans_touch(&_g);
    // Each wire's touches together (sorted by it), found by its first: _first[w] to _first[w + 1].
    const u32 _nw = (u32)rde_arr_length(&_g.wires), _nt = (u32)rde_arr_length(&_g.touches);
    rde_sort(_g.touches.memory, _nt, sizeof(fzc_touch), fzc_touch_order);
    const fzc_touch* _t = (const fzc_touch*)_g.touches.memory;
    rde_arr _first_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_first_arr, _nw + 1u);
    u32* _first = (u32*)_first_arr.memory;
    rde_memset(_first, 0, (usize)(_nw + 1u) * sizeof(u32));
    for(u32 _i = 0; _i < _nt; _i++) {
        _first[_t[_i].target + 1u]++;
    }
    for(u32 _w = 0; _w < _nw; _w++) {
        _first[_w + 1u] += _first[_w];
    }
    // The first to go: those at a dying part's pin, and those with an end on dying wires only, in the canvas's order.
    u8* _going = (u8*)_g.going.memory;
    fzc_loose* _ends = (fzc_loose*)_g.ends.memory;
    for(u32 _e = 0; _e < (u32)rde_arr_length(&_g.ends); _e++) {
        if(_ends[_e].dying > 0u && _ends[_e].live == 0u) {
            _going[_ends[_e].wire] = 1u;
        }
    }
    rde_arr _queue = rde_arr_new(sizeof(u32), _heap);
    for(u32 _w = 0; _w < _nw; _w++) {
        if(_going[_w]) {
            rde_arr_add(&_queue, (any)&_w);
        }
    }
    // Each let go in turn: the ends on it lie on one wire alive fewer, and one left on none takes its wire next.
    u32 _made = 0;
    for(u32 _head = 0; _head < (u32)rde_arr_length(&_queue); _head++) {
        const u32 _w = ((const u32*)_queue.memory)[_head];
        u32 _object = ((const u32*)_g.wires.memory)[_w];
        fude_zoom_scene_set_alive(_s, _object, false);
        _dead[_object] = 1u;
        rde_arr_add(_died, (any)&_object);
        _made++;
        for(u32 _i = _first[_w]; _i < _first[_w + 1u]; _i++) {
            fzc_loose* _end = &_ends[_t[_i].end];
            _end->live  = _end->live > 0u ? _end->live - 1u : 0u;
            _end->dying++;
            if(_end->live == 0u && !_going[_end->wire]) {
                _going[_end->wire] = 1u;
                rde_arr_add(&_queue, (any)&_end->wire);
            }
        }
    }
    rde_arr_free(&_queue);
    rde_arr_free(&_first_arr);
    rde_arr_free(&_g.wires);
    rde_arr_free(&_g.going);
    rde_arr_free(&_g.segs);
    rde_arr_free(&_g.ends);
    rde_arr_free(&_g.touches);
    rde_arr_free(&_dead_arr);
    return _made;
}

// See the header: the wire made again with its ends' ids replaced (fzc_wire_again), or FUDE_ZOOM_NONE when none was.
u32 fude_zoom_wire_remap(fude_zoom_scene* _s, u32 _wire, const fude_zoom_id* _old, const fude_zoom_id* _new, u32 _n) {
    f64 _num[FUDE_ZOOM_WIRE_NUMBERS];
    const u32 _c = fude_zoom_scene_shape_numbers(_s, _wire, _num, FUDE_ZOOM_WIRE_NUMBERS);
    fude_zoom_id _from, _to;
    i32 _fp, _tp;
    if(fude_zoom_wire_of(_num, _c, NULL, &_from, &_fp, &_to, &_tp) < 2u) {
        return FUDE_ZOOM_NONE;
    }
    b8 _any = false;
    for(u32 _i = 0; _i < _n; _i++) {
        // An old save's id, rounded as a number, not found as it is: the one it rounds from.
        const b8 _fa = _from != 0u && (_from == _old[_i] || (fude_zoom_scene_find_object(_s, _from) == FUDE_ZOOM_NONE && (f64)_from == (f64)_old[_i]));
        const b8 _tb = _to != 0u && (_to == _old[_i] || (fude_zoom_scene_find_object(_s, _to) == FUDE_ZOOM_NONE && (f64)_to == (f64)_old[_i]));
        if(_fa) { _from = _new[_i]; _any = true; }
        if(_tb) { _to = _new[_i];   _any = true; }
    }
    return _any ? fzc_wire_again(_s, _wire, _from, _fp, _to, _tp) : FUDE_ZOOM_NONE;
}
