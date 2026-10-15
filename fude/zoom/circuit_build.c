// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/circuit_parts.h"

// ===========================================================================
// See circuit.h. Circuits, the circuit worked out from the canvas (fude_zoom_circuit_build_in): its parts (their
// state kept, their text read, their limits, their grid) and its wires in scope; the terminals (each pin, each wire's
// end, each pin's lead) and which touch; the wires cut where others' ends meet them; same-named pins and a breadboard's
// strips joined; ground; the nodes; the vertices for the wires' currents; then its logic, its rails held, its blocks
// and its longest step.
//
// The file holds a terminal (fzc_term), then what the build reads with (a wire's ends' ids, the orders it sorts by, a
// pin's lead), the longest step (fzc_step_most), and fude_zoom_circuit_build_in: its steps (fzc_build_*, in the order
// it takes them) share what they work on in a context (fzc_build), each taking what it reads under the names the steps
// had in one function and handing back what it made.
//
// Nothing in it goes through everything for each thing (a canvas's circuit may have thousands of parts and wires): a
// wire's end finds its part through an index of objects (part_of_object), what lies near a point is found in a grid of
// segments (circuit_parts.h: fzc_seg_grid), and the terminals touching are found sorted along x. (A wire saved before
// 0.1.49, its ids rounded, is the one exception: its part is looked for among every object, fude_zoom_wire_part.)
//
// Which terminals are one node (net) and which are one place (touch) are RDE's disjoint sets. A set's name is its
// smallest terminal (rde_disjoint_set's rule, the one the hand-written union-find here had too), and the nodes and
// vertices are numbered by walking the terminals in order and giving a set its number at its name: so the numbers
// the solver sees do not depend on the order the terminals were joined in.
//
// Part of the circuit: circuit_parts.h holds what the modules share and says which module holds what. Prefix fzc_ and
// FZC_. Main thread only.
// ===========================================================================

// --- the circuit, from the canvas ---------------------------------------------------------------------

// A terminal: a part's pin or a wire's end, where things join (the build's).
typedef struct {
    fude_zoom_v2 at;      // where it is, in the home frame's units
    f64          unit;    // its part's grid there (how near counts as touching)
    u32          part;    // its part (in the circuit's), or FUDE_ZOOM_NONE: a wire's end
    u32          pin;     // ...its pin; a wire's: which end (0, 1)
    u32          wire;    // a wire's end: its wire (in the circuit's)
} fzc_term;

// An id kept in the build's refs (its bits as they are).
RDE_INTERNAL fude_zoom_id fzc_ref_id(const f64* _r) {
    fude_zoom_id _id;
    rde_memcpy(&_id, _r, sizeof(_id));
    return _id;
}

// The order of u64s, for rde_sort.
RDE_INTERNAL int fzc_u64_order(const any _a, const any _b) {
    const u64 _x = *(const u64*)_a, _y = *(const u64*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// A terminal's place along x, and the terminal (fzc_build_touching sorts the terminals by it).
typedef struct {
    f64 x;
    u32 term;
} fzc_along_x;

// The order of terminals along x: by x, then by terminal (so the order is the same however they were found).
RDE_INTERNAL int fzc_along_x_order(const any _a, const any _b) {
    const fzc_along_x* _p = (const fzc_along_x*)_a, *_q = (const fzc_along_x*)_b;
    return _p->x < _q->x ? -1 : (_p->x > _q->x ? 1 : (_p->term < _q->term ? -1 : (_p->term > _q->term ? 1 : 0)));
}

// A pin's lead's inner end (its part's own u, v): 45% of the way in from it toward its part's middle, along its side's
// way — where a gate's, a chip's, a block's drawn lead meets its body. False: none (a breadboard's hole).
RDE_INTERNAL b8 fzc_lead(const fude_zoom_part* _part, u32 _k, fude_zoom_pin* _out) {
    if(!fude_zoom_part_pin(_part, _k, _out) || _out->side == FUDE_ZOOM_PIN_ANY) {
        return false;
    }
    if(_out->side == FUDE_ZOOM_PIN_LEFT || _out->side == FUDE_ZOOM_PIN_RIGHT) {
        _out->u *= 0.55f;
    } else {
        _out->v *= 0.55f;
    }
    return true;
}

// Its longest step: a millisecond, finer for what changes faster — a 40th of a source's period, a 50th of an
// oscilloscope's sweep (its samples drawn straight across a step, where the steps follow how its curves bend: 5 steps a
// division at least — one a sample stepped a piano's quiet scope 40 000 times a second).
#define FZC_SCOPE_STEPS 50.0   // an oscilloscope's sweep in steps at least

// Works out the circuit's longest step (step) from its parts, as said above; never under a microsecond.
void fzc_step_most(fude_zoom_circuit* _c) {
    const fude_zoom_circuit_part* _parts = (const fude_zoom_circuit_part*)_c->parts.memory;
    _c->step = 1e-3;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_c->parts); _i++) {
        const u8 _m = _parts[_i].part->model;
        if((_m == FUDE_ZOOM_MODEL_ACSOURCE && _parts[_i].value[1] > 0.0) || (_m == FUDE_ZOOM_MODEL_CLOCK && _parts[_i].value[0] > 0.0)) {
            const f64 _f = _m == FUDE_ZOOM_MODEL_ACSOURCE ? _parts[_i].value[1] : _parts[_i].value[0];
            _c->step = rde_math_min_f64(_c->step, rde_math_max_f64(1.0 / (40.0 * _f), 1e-6));
        }
        if(_m == FUDE_ZOOM_MODEL_SCOPE && _parts[_i].value[0] > 0.0) {
            _c->step = rde_math_min_f64(_c->step, rde_math_max_f64(FUDE_ZOOM_SCOPE_DIVS_X * _parts[_i].value[0] / FZC_SCOPE_STEPS, 1e-6));
        }
    }
}

// What fude_zoom_circuit_build_in works with from step to step: the circuit, the canvas, what is in scope (NULL:
// everything) and the home frame; the parts and wires read, and how many; each wire's ends' ids and pins (four
// numbers a wire) and each part's grid; the terminals (each part's pins, then from wire_first each wire's two ends),
// each part's first terminal and each pin's lead's inner end; for each terminal its node's set (net), its place's
// (touch) and whether something holds it (held); the junctions (a wire and the end on it) and how far along it each
// is; ground's terminal; each terminal's node, its set's size, and its vertex; and each object's part, so that a wire's
// end finds its part at once rather than by going through every part. Each step takes what it reads of it
// under the names the function had (an array's handle by value, read only), and hands back what it made; the
// function frees the arrays at its end.
typedef struct {
    fude_zoom_circuit*      c;
    const fude_zoom_scene*  s;
    const u8*               scope;
    u32                     home;
    fude_zoom_circuit_part* parts;
    fude_zoom_circuit_wire* wires;
    u32                     np, nw;
    rde_arr TYPE(f64)       refs;
    rde_arr TYPE(f64)       units;
    rde_arr TYPE(fzc_term)  terms;
    rde_arr TYPE(u32)       first;
    rde_arr TYPE(fude_zoom_v2) leads;
    u32                     wire_first, nt;
    rde_disjoint_set        net;     // the terminals joined: one node (each set named by its smallest terminal)
    rde_disjoint_set        touch;   // ...touching: one place (a vertex)
    rde_arr TYPE(u8)        held_arr;
    rde_arr TYPE(u32)       junction;
    rde_arr TYPE(f64)       along;
    u32                     ground;
    rde_arr TYPE(u32)       node_of_arr;
    rde_arr TYPE(u32)       size_arr;
    rde_arr TYPE(u32)       vertex_arr;
    rde_arr TYPE(u32)       part_of_object;   // each object's part in the circuit (FUDE_ZOOM_NONE: none), by its index
} fzc_build;

// Reads part _i (object _o) into the circuit when it is a part in scope: its state kept from what it was (_was, what
// it was when the circuit was last built, NULL when it was not in it; what it held in _was_store) where it is still the
// same part, its text read, what it holds, its limits, its probe's light; and its grid, the circuit's the smallest.
RDE_INTERNAL void fzc_build_part(fzc_build* _build, u32 _i, const fude_zoom_object* _o, const fude_zoom_circuit_part* _was, const rde_arr* _was_store) {
    fude_zoom_circuit* _c = _build->c;
    const fude_zoom_scene* _s = _build->s;
    const u8* _scope = _build->scope;
    const u32 _home = _build->home;
    c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
    const fude_zoom_part* _part = fude_zoom_part_of(_s, _i);
    if(_part == NULL || (_scope != NULL && !_scope[_i])) {
        return;
    }
    fude_zoom_circuit_part _p;
    rde_memset(&_p, 0, sizeof(_p));
    _p.object   = _i;
    _p.part     = _part;
    b8 _fresh = true;
    const fude_zoom_circuit_part* _kept = NULL;
    if(_was != NULL && _was->object == _i && _was->part == _part) {
        _kept = _was;
        rde_memcpy(_p.state, _was->state, sizeof(_p.state));
        _p.switch_on   = _was->switch_on;
        _p.drive       = _was->drive;
        _p.drive_high  = _was->drive_high;
        _p.drive_blink = _was->drive_blink;
        _p.shown       = _was->shown;
        _p.heat        = _was->heat;
        _p.burnt       = _was->burnt;
        _p.worst       = _was->worst;
        _p.worst_seen  = _was->worst_seen;
        _p.told        = _was->told;
        _fresh = false;
    }
    fzc_text_of(_s, _i, _text, sizeof(_text));
    fzc_read(&_p, _text, _fresh);
    // What it holds beyond its state (a display's lights as seen, an LCD's controller): as it was, kept; else new.
    _p.store_size = fzc_store_size(_part);
    _p.store_at   = (u32)rde_arr_length(&_c->store);
    if(_p.store_size > 0u) {
        u8* _mine = (u8*)rde_arr_add_n(&_c->store, _p.store_size);
        if(_kept != NULL && _kept->store_size == _p.store_size && (usize)_kept->store_at + _p.store_size <= rde_arr_length(_was_store)) {
            rde_memcpy(_mine, (const u8*)_was_store->memory + _kept->store_at, _p.store_size);
        } else {
            rde_memset(_mine, 0, _p.store_size);
        }
    }
    // Its limits: its own (its card's), else those of the real part its text names, else a typical one's.
    _p.limits = fude_zoom_limits_typical(_part, _text, _p.value);
    const u32 _lim = _i < (u32)rde_arr_length(&_c->limits_of) ? ((const u32*)_c->limits_of.memory)[_i] : FUDE_ZOOM_NONE;
    i32 _preset = -1;
    if(_lim != FUDE_ZOOM_NONE) {
        fude_zoom_props_limits(_s, _lim, _p.limits.most, &_preset);
    }
    _p.lit = FUDE_ZOOM_PROBE_LIT;
    if(_part->model == FUDE_ZOOM_MODEL_LOGIC_OUT) {
        const u32 _light = fude_zoom_props_find(_s, _i, FUDE_ZOOM_PROPS_LIGHT);
        if(_light != FUDE_ZOOM_NONE) {
            fude_zoom_props_light(_s, _light, &_p.lit);
        }
    }
    rde_arr_add(&_c->parts, (any)&_p);
    // Its grid: a tenth of its pins' room as it came, in the home frame's units.
    f64 _n[3];
    const u32 _nn = fude_zoom_scene_shape_numbers(_s, _i, _n, 3u);
    const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of(fude_zoom_symbol_kind_of(_n, _nn));
    const f64 _k = fude_zoom_sim_scale(fude_zoom_scene_sim(_s, _o->frame, _home)) * _o->scale;
    const f64 _room = _info != NULL ? fude_zoom_part_room_h(_part, (f64)_info->h) : 0.0;
    f64 _unit = _room > 0.0 ? _n[2] * _k / (_room * 0.5) * 10.0 : _n[2] * _k * 0.5;
    if(_part->look == FZC_BREADBOARD) {
        _unit = 2.0 * _n[2] * _k / (f64)FZC_BB_H;   // (a hole apart)
    }
    rde_arr_add(&_build->units, (any)&_unit);
    _c->unit = _c->unit > 0.0 ? rde_math_min_f64(_c->unit, _unit) : _unit;
}

// Reads wire _i (object _o) into the circuit: its points in the home frame and its length, its dots' phases kept from
// what it was (_was: it when the circuit was last built, NULL when it was not in it), and its ends' ids and pins kept
// in refs.
RDE_INTERNAL void fzc_build_wire(fzc_build* _build, u32 _i, const fude_zoom_object* _o, const fude_zoom_circuit_wire* _was) {
    fude_zoom_circuit* _c = _build->c;
    const fude_zoom_scene* _s = _build->s;
    const u32 _home = _build->home;
    f64 _n[FUDE_ZOOM_WIRE_NUMBERS];
    const u32 _cnt = fude_zoom_scene_shape_numbers(_s, _i, _n, FUDE_ZOOM_WIRE_NUMBERS);
    fude_zoom_circuit_wire _w;
    rde_memset(&_w, 0, sizeof(_w));
    fude_zoom_id _from, _to;
    i32 _fp, _tp;
    _w.count = fude_zoom_wire_of(_n, _cnt, _w.points, &_from, &_fp, &_to, &_tp);
    if(_w.count < 2u) {
        return;
    }
    const fude_zoom_sim _to_home = fude_zoom_sim_compose(fude_zoom_scene_sim(_s, _o->frame, _home), fude_zoom_object_sim(_o));
    for(u32 _k = 0; _k < _w.count; _k++) {
        _w.points[_k] = fude_zoom_sim_apply(_to_home, _w.points[_k]);
        if(_k > 0u) {
            _w.length += rde_math_hypot_f64(_w.points[_k].x - _w.points[_k - 1u].x, _w.points[_k].y - _w.points[_k - 1u].y);
        }
    }
    _w.object = _i;
    _w.node   = FUDE_ZOOM_NONE;
    if(_was != NULL && _was->object == _i) {
        rde_memcpy(_w.phase, _was->phase, sizeof(_w.phase));
    }
    rde_arr_add(&_c->wires, (any)&_w);
    f64 _r[4] = { 0.0, (f64)_fp, 0.0, (f64)_tp };   // (each id's bits as they are: fzc_ref_id)
    rde_memcpy(&_r[0], &_from, sizeof(f64));
    rde_memcpy(&_r[2], &_to, sizeof(f64));
    for(u32 _k = 0; _k < 4u; _k++) {
        rde_arr_add(&_build->refs, (any)&_r[_k]);
    }
}

// Reads what is on the canvas into the circuit: what it was kept aside (each part and wire found again by its object,
// through an index for each object, not a search for each), each part's limits attribute found, then each part and wire
// alive and shown (fzc_build_part, fzc_build_wire), in the canvas's order; then each object's part (part_of_object).
RDE_INTERNAL void fzc_build_read(fzc_build* _build) {
    fude_zoom_circuit* _c = _build->c;
    const fude_zoom_scene* _s = _build->s;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    // What it was (each part's state kept where it is still there).
    const u32 _was_n = (u32)rde_arr_length(&_c->parts);
    rde_arr TYPE(fude_zoom_circuit_part) _was_arr = rde_arr_new(sizeof(fude_zoom_circuit_part), _heap);
    rde_arr_add_arr(&_was_arr, &_c->parts);
    const fude_zoom_circuit_part* _was = (const fude_zoom_circuit_part*)_was_arr.memory;
    const u32 _was_w = (u32)rde_arr_length(&_c->wires);
    rde_arr TYPE(fude_zoom_circuit_wire) _was_wires_arr = rde_arr_new(sizeof(fude_zoom_circuit_wire), _heap);
    rde_arr_add_arr(&_was_wires_arr, &_c->wires);
    const fude_zoom_circuit_wire* _was_wires = (const fude_zoom_circuit_wire*)_was_wires_arr.memory;
    rde_arr TYPE(u8) _was_store = rde_arr_new(sizeof(u8), _heap);
    rde_arr_add_arr(&_was_store, &_c->store);
    // Each object's part and wire as they were (FUDE_ZOOM_NONE: none), by its index.
    const u32 _objects = fude_zoom_scene_object_count(_s);
    rde_arr TYPE(u32) _was_at_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_was_at_arr, 2u * (usize)_objects);
    u32* _was_part = (u32*)_was_at_arr.memory, *_was_wire = _was_part + _objects;
    for(u32 _i = 0; _i < 2u * _objects; _i++) {
        _was_part[_i] = FUDE_ZOOM_NONE;
    }
    for(u32 _k = 0; _k < _was_n; _k++) {
        if(_was[_k].object < _objects && _was_part[_was[_k].object] == FUDE_ZOOM_NONE) {
            _was_part[_was[_k].object] = _k;
        }
    }
    for(u32 _k = 0; _k < _was_w; _k++) {
        if(_was_wires[_k].object < _objects) {
            _was_wire[_was_wires[_k].object] = _k;
        }
    }
    rde_arr_clear(&_c->parts);
    rde_arr_clear(&_c->wires);
    rde_arr_clear(&_c->store);
    // Each part's limits attribute, found once (props.h: each names the object it is on).
    rde_arr_resize(&_c->limits_of, 0u);
    rde_arr_resize(&_c->limits_of, fude_zoom_scene_object_count(_s));
    u32* _limits_of = (u32*)_c->limits_of.memory;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        _limits_of[_i] = FUDE_ZOOM_NONE;
    }
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        if(fude_zoom_props_kind(_s, _i) == FUDE_ZOOM_PROPS_LIMITS) {
            const u32 _t = fude_zoom_props_target(_s, _i);
            if(_t != FUDE_ZOOM_NONE) {
                _limits_of[_t] = _i;
            }
        }
    }
    _build->home  = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    _build->refs  = rde_arr_new(sizeof(f64), _heap);   // each wire's ends' (id, pin, id, pin)
    _build->units = rde_arr_new(sizeof(f64), _heap);   // each part's grid
    _c->unit = 0.0;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || fude_zoom_scene_hides(_s, _o) || !fude_zoom_scene_frame_shown(_s, _o->frame)) {
            continue;
        }
        if(_o->channels == FUDE_ZOOM_SHAPE_SYMBOL) {
            fzc_build_part(_build, _i, _o, _i < _objects && _was_part[_i] != FUDE_ZOOM_NONE ? &_was[_was_part[_i]] : NULL, &_was_store);
        } else if(_o->channels == FUDE_ZOOM_SHAPE_WIRE) {
            fzc_build_wire(_build, _i, _o, _i < _objects && _was_wire[_i] != FUDE_ZOOM_NONE ? &_was_wires[_was_wire[_i]] : NULL);
        }
    }
    rde_arr_free(&_was_at_arr);
    rde_arr_free(&_was_arr);
    rde_arr_free(&_was_wires_arr);
    rde_arr_free(&_was_store);
    // Each object's part now.
    _build->part_of_object = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_build->part_of_object, _objects);
    u32* _part_of = (u32*)_build->part_of_object.memory;
    for(u32 _i = 0; _i < _objects; _i++) {
        _part_of[_i] = FUDE_ZOOM_NONE;
    }
    const fude_zoom_circuit_part* _now = (const fude_zoom_circuit_part*)_c->parts.memory;
    for(u32 _k = 0; _k < (u32)rde_arr_length(&_c->parts); _k++) {
        _part_of[_now[_k].object] = _k;
    }
}

// Keeps only the wires in scope: its own, those joined to its parts, and those joined to such wires (an end of one on
// the other, either way), and so on. Which wires are joined so is found once, each end looking only at the segments
// near it (fzc_seg_grid); then those in scope reach the rest through it (a walk), rather than every wire against every
// other until nothing more joins.
RDE_INTERNAL void fzc_build_scope(fzc_build* _build) {
    fude_zoom_circuit* _c = _build->c;
    const fude_zoom_scene* _s = _build->s;
    const u8* _scope = _build->scope;
    const u32 _home = _build->home;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _refs = _build->refs;
    fude_zoom_circuit_wire* _ws = (fude_zoom_circuit_wire*)_c->wires.memory;
    f64* _rf = (f64*)_refs.memory;
    const u32 _nw0 = (u32)rde_arr_length(&_c->wires);
    const u32* _part_of = (const u32*)_build->part_of_object.memory;
    const u32 _objects = (u32)rde_arr_length(&_build->part_of_object);
    rde_arr TYPE(u8) _in_arr = rde_arr_new(sizeof(u8), _heap);
    rde_arr_resize(&_in_arr, _nw0);
    u8* _in = (u8*)_in_arr.memory;   // (sized once: it stays put)
    for(u32 _w = 0; _w < _nw0; _w++) {
        _in[_w] = _scope[_ws[_w].object] ? 1u : 0u;
        for(u32 _e = 0; _e < 2u && !_in[_w]; _e++) {
            const fude_zoom_id _id = fzc_ref_id(&_rf[4u * _w + 2u * _e]);
            const u32 _obj = fude_zoom_wire_part(_s, _id, (i32)_rf[4u * _w + 2u * _e + 1u], _home, _ws[_w].points[_e == 0u ? 0u : _ws[_w].count - 1u]);
            _in[_w] = _obj != FUDE_ZOOM_NONE && _obj < _objects && _part_of[_obj] != FUDE_ZOOM_NONE ? 1u : 0u;
        }
    }
    // Which wires are joined: each end on another's way (within _tol), both ways round, as a list for each wire.
    const f64 _tol = (_c->unit > 0.0 ? _c->unit : 1.0) * 0.5;
    rde_arr TYPE(fzc_seg) _segs = rde_arr_new(sizeof(fzc_seg), _heap);
    for(u32 _w = 0; _w < _nw0; _w++) {
        for(u32 _k = 0; _k + 1u < _ws[_w].count; _k++) {
            const fzc_seg _sg = { _w, _ws[_w].points[_k], _ws[_w].points[_k + 1u], _tol };
            rde_arr_add(&_segs, (any)&_sg);
        }
    }
    fzc_seg_grid _grid;
    fzc_grid_make(&_grid, (const fzc_seg*)_segs.memory, (u32)rde_arr_length(&_segs));
    rde_arr TYPE(u64) _pairs = rde_arr_new(sizeof(u64), _heap);   // (w << 32 | v: w is joined to v)
    rde_arr TYPE(u32) _near = rde_arr_new(sizeof(u32), _heap);
    for(u32 _w = 0; _w < _nw0; _w++) {
        for(u32 _e = 0; _e < 2u; _e++) {
            const fude_zoom_v2 _end = _ws[_w].points[_e == 0u ? 0u : _ws[_w].count - 1u];
            fzc_grid_near(&_grid, _end, &_near);
            for(u32 _j = 0; _j < (u32)rde_arr_length(&_near); _j++) {
                const fzc_seg* _sg = &((const fzc_seg*)_segs.memory)[((const u32*)_near.memory)[_j]];
                if(_sg->owner != _w && rde_math_point_segment_distance_2d_f64(_end, _sg->a, _sg->b) <= _tol) {
                    const u64 _there = ((u64)_w << 32) | _sg->owner, _back = ((u64)_sg->owner << 32) | _w;
                    rde_arr_add(&_pairs, (any)&_there);
                    rde_arr_add(&_pairs, (any)&_back);
                }
            }
        }
    }
    fzc_grid_free(&_grid);
    rde_arr_free(&_near);
    rde_arr_free(&_segs);
    // Each wire's joins together (sorted), then a walk from the wires in scope through them.
    const u32 _np = (u32)rde_arr_length(&_pairs);
    rde_sort(_pairs.memory, _np, sizeof(u64), fzc_u64_order);
    const u64* _pr = (const u64*)_pairs.memory;
    rde_arr TYPE(u32) _first_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_first_arr, _nw0 + 1u);
    u32* _first = (u32*)_first_arr.memory;
    rde_memset(_first, 0, ((usize)_nw0 + 1u) * sizeof(u32));
    for(u32 _i = 0; _i < _np; _i++) {
        _first[(u32)(_pr[_i] >> 32) + 1u]++;
    }
    for(u32 _w = 0; _w < _nw0; _w++) {
        _first[_w + 1u] += _first[_w];
    }
    rde_arr TYPE(u32) _queue = rde_arr_new(sizeof(u32), _heap);
    for(u32 _w = 0; _w < _nw0; _w++) {
        if(_in[_w]) {
            rde_arr_add(&_queue, (any)&_w);
        }
    }
    for(u32 _head = 0; _head < (u32)rde_arr_length(&_queue); _head++) {
        const u32 _w = ((const u32*)_queue.memory)[_head];
        for(u32 _i = _first[_w]; _i < _first[_w + 1u]; _i++) {
            const u32 _v = (u32)(_pr[_i] & 0xFFFFFFFFull);
            if(!_in[_v]) {
                _in[_v] = 1u;
                rde_arr_add(&_queue, (any)&_v);
            }
        }
    }
    rde_arr_free(&_queue);
    rde_arr_free(&_first_arr);
    rde_arr_free(&_pairs);
    u32 _kept = 0;
    for(u32 _w = 0; _w < _nw0; _w++) {
        if(_in[_w]) {
            _ws[_kept] = _ws[_w];
            rde_memmove(&_rf[4u * _kept], &_rf[4u * _w], 4u * sizeof(f64));
            _kept++;
        }
    }
    rde_arr_resize(&_c->wires, _kept);
    rde_arr_resize(&_refs, 4u * _kept);
    rde_arr_free(&_in_arr);
    _build->refs = _refs;
}

// Makes the terminals: each part's pins (with each pin's lead's inner end), then each wire's two ends; and the sets
// they will be joined in (each its own), and none held yet.
RDE_INTERNAL void fzc_build_terminals(fzc_build* _build) {
    fude_zoom_circuit* _c = _build->c;
    const fude_zoom_scene* _s = _build->s;
    const u32 _home = _build->home;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_circuit_part* _parts = _build->parts;
    fude_zoom_circuit_wire* _wires = _build->wires;
    const u32 _np = _build->np;
    const u32 _nw = _build->nw;
    const rde_arr _units = _build->units;
    // Terminals: each part's pins, each wire's two ends. A pin's lead too: from it in toward its part's middle (as its
    // drawing's lead goes: a gate's, a chip's stub) — a wire's end dropped on it is on the pin.
    rde_arr _terms = rde_arr_new(sizeof(fzc_term), _heap);
    rde_arr _first = rde_arr_new(sizeof(u32), _heap);   // each part's first terminal
    rde_arr TYPE(fude_zoom_v2) _leads = rde_arr_new(sizeof(fude_zoom_v2), _heap);   // each pin's lead's inner end
    for(u32 _i = 0; _i < _np; _i++) {
        const u32 _at = (u32)rde_arr_length(&_terms);
        rde_arr_add(&_first, (any)&_at);
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _parts[_i].object);
        const fude_zoom_sim _up = fude_zoom_scene_sim(_s, _o->frame, _home);
        f64 _hn[3] = { 0.0, 0.0, 0.0 };
        fude_zoom_scene_shape_numbers(_s, _parts[_i].object, _hn, 3u);
        const fude_zoom_sim _whole = fude_zoom_sim_compose(_up, fude_zoom_object_sim(_o));
        for(u32 _k = 0; _k < _parts[_i].part->pin_count; _k++) {
            fzc_term _t = { { 0.0, 0.0 }, ((const f64*)_units.memory)[_i], _i, _k, FUDE_ZOOM_NONE };
            fude_zoom_v2 _q, _in;
            fude_zoom_pin _pin;
            if(fude_zoom_part_pin_at(_s, _parts[_i].object, _k, &_q)) {
                _t.at = fude_zoom_sim_apply(_up, _q);
            }
            _in = _t.at;
            if(fzc_lead(_parts[_i].part, _k, &_pin)) {
                _in = fude_zoom_sim_apply(_whole, (fude_zoom_v2){ (f64)_pin.u * _hn[1], (f64)_pin.v * _hn[2] });
            }
            rde_arr_add(&_terms, (any)&_t);
            rde_arr_add(&_leads, (any)&_in);
        }
    }
    const u32 _wire_first = (u32)rde_arr_length(&_terms);
    for(u32 _w = 0; _w < _nw; _w++) {
        for(u32 _e = 0; _e < 2u; _e++) {
            const fzc_term _t = { _wires[_w].points[_e == 0u ? 0u : _wires[_w].count - 1u], _c->unit, FUDE_ZOOM_NONE, _e, _w };
            rde_arr_add(&_terms, (any)&_t);
        }
    }
    const u32 _nt = (u32)rde_arr_length(&_terms);
    rde_arr TYPE(u8) _held_arr = rde_arr_new(sizeof(u8), _heap);   // each wire end: something holds it (a pin, a wire)
    rde_arr_resize(&_held_arr, _nt);
    _build->terms      = _terms;
    _build->first      = _first;
    _build->leads      = _leads;
    _build->wire_first = _wire_first;
    _build->nt         = _nt;
    _build->net        = rde_disjoint_set_new(_nt, _heap);   // joined: one node; each terminal its own set to start
    _build->touch      = rde_disjoint_set_new(_nt, _heap);   // ...touching: one place (a vertex)
    _build->held_arr   = _held_arr;
}

// Joins each wire's two ends (one node), and each end to the pin it keeps, found by its part's id (one node, one
// place, held).
RDE_INTERNAL void fzc_build_kept(fzc_build* _build) {
    const fude_zoom_scene* _s = _build->s;
    const u32 _home = _build->home;
    fude_zoom_circuit_part* _parts = _build->parts;
    fude_zoom_circuit_wire* _wires = _build->wires;
    const u32 _np = _build->np;
    const u32 _nw = _build->nw;
    const rde_arr _refs = _build->refs;
    const rde_arr _first = _build->first;
    const u32 _wire_first = _build->wire_first;
    rde_disjoint_set* _net = &_build->net;
    rde_disjoint_set* _touch = &_build->touch;
    u8* _held = (u8*)_build->held_arr.memory;
    const u32* _part_of = (const u32*)_build->part_of_object.memory;
    const u32 _objects = (u32)rde_arr_length(&_build->part_of_object);
    // A wire's ends to the pins they keep, and to each other.
    for(u32 _w = 0; _w < _nw; _w++) {
        const f64* _r = &((const f64*)_refs.memory)[4u * _w];
        rde_disjoint_set_union(_net, _wire_first + 2u * _w, _wire_first + 2u * _w + 1u);
        for(u32 _e = 0; _e < 2u; _e++) {
            const fude_zoom_id _id = fzc_ref_id(&_r[2u * _e]);
            const i32 _pin = (i32)_r[2u * _e + 1u];
            if(_id == 0u || _pin < 0) {
                continue;
            }
            const u32 _obj = fude_zoom_wire_part(_s, _id, _pin, _home, _wires[_w].points[_e == 0u ? 0u : _wires[_w].count - 1u]);
            const u32 _i = _obj < _objects ? _part_of[_obj] : FUDE_ZOOM_NONE;   // (its part in the circuit: one look)
            if(_i != FUDE_ZOOM_NONE && _i < _np && (u32)_pin < _parts[_i].part->pin_count) {
                const u32 _pt = ((const u32*)_first.memory)[_i] + (u32)_pin;
                rde_disjoint_set_union(_net, _wire_first + 2u * _w + _e, _pt);
                rde_disjoint_set_union(_touch, _wire_first + 2u * _w + _e, _pt);
                _held[_wire_first + 2u * _w + _e] = 1u;
            }
        }
    }
}

// Joins the terminals that touch (within 0.3 of their grid): one node and one place, both held; never a part's own pins
// by being near.
RDE_INTERNAL void fzc_build_touching(fzc_build* _build) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    const u32 _nt = _build->nt;
    fzc_term* _tm = (fzc_term*)_build->terms.memory;
    rde_disjoint_set* _net = &_build->net;
    rde_disjoint_set* _touch = &_build->touch;
    u8* _held = (u8*)_build->held_arr.memory;
    // Terminals where each other is (sorted along x, each looking on only as far as it could touch).
    rde_arr TYPE(fzc_along_x) _along_arr = rde_arr_new(sizeof(fzc_along_x), _heap);
    rde_arr_resize(&_along_arr, _nt);
    fzc_along_x* _along = (fzc_along_x*)_along_arr.memory;
    for(u32 _i = 0; _i < _nt; _i++) {
        _along[_i] = (fzc_along_x){ _tm[_i].at.x, _i };
    }
    rde_sort(_along, _nt, sizeof(fzc_along_x), fzc_along_x_order);
    rde_arr TYPE(u32) _order_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_order_arr, _nt);
    u32* _order = (u32*)_order_arr.memory;
    for(u32 _i = 0; _i < _nt; _i++) {
        _order[_i] = _along[_i].term;
    }
    rde_arr_free(&_along_arr);
    f64 _reach = 0.0;
    for(u32 _i = 0; _i < _nt; _i++) {
        _reach = rde_math_max_f64(_reach, 0.3 * _tm[_i].unit);
    }
    for(u32 _i = 0; _i < _nt; _i++) {
        const fzc_term* _a = &_tm[_order[_i]];
        for(u32 _j = _i + 1u; _j < _nt && _tm[_order[_j]].at.x - _a->at.x <= _reach; _j++) {
            const fzc_term* _b = &_tm[_order[_j]];
            if(_a->part != FUDE_ZOOM_NONE && _a->part == _b->part) {
                continue;   // (a part's own pins: never by being near)
            }
            if(rde_math_hypot_f64(_b->at.x - _a->at.x, _b->at.y - _a->at.y) <= 0.3 * rde_math_min_f64(_a->unit, _b->unit)) {
                rde_disjoint_set_union(_net, _order[_i], _order[_j]);
                rde_disjoint_set_union(_touch, _order[_i], _order[_j]);
                _held[_order[_i]] = _held[_order[_j]] = 1u;
            }
        }
    }
    rde_arr_free(&_order_arr);
}

// Joins a wire's end that lies along another wire (not at its ends) to it: a junction, noted with how far along it.
// Each end looks only at the segments near it (fzc_seg_grid), in the order of their wires and then along each (as a
// search through every wire takes them), and joins each wire at the first of its segments it lies along.
RDE_INTERNAL void fzc_build_junctions(fzc_build* _build) {
    fude_zoom_circuit* _c = _build->c;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_circuit_wire* _wires = _build->wires;
    const u32 _nw = _build->nw;
    const rde_arr _refs = _build->refs;
    const u32 _wire_first = _build->wire_first;
    const u32 _nt = _build->nt;
    fzc_term* _tm = (fzc_term*)_build->terms.memory;
    rde_disjoint_set* _net = &_build->net;
    u8* _held = (u8*)_build->held_arr.memory;
    // A wire's end on another wire, along it: joined there (a junction: that wire's pieces meet at it).
    rde_arr_clear(&_c->cut_vertex);
    for(u32 _w = 0; _w < _nw; _w++) {
        _wires[_w].pieces = 1u;
        _wires[_w].cut[0] = 0.0;
        _wires[_w].cut[1] = _wires[_w].length;
    }
    // Every wire's segments (wire by wire, along each), each with how far along its wire it starts.
    const f64 _reach = 0.3 * _c->unit;
    rde_arr TYPE(fzc_seg) _segs = rde_arr_new(sizeof(fzc_seg), _heap);
    rde_arr TYPE(f64) _starts = rde_arr_new(sizeof(f64), _heap);
    for(u32 _w = 0; _w < _nw; _w++) {
        f64 _len = 0.0;
        for(u32 _k = 0; _k + 1u < _wires[_w].count; _k++) {
            const fzc_seg _sg = { _w, _wires[_w].points[_k], _wires[_w].points[_k + 1u], _reach };
            rde_arr_add(&_segs, (any)&_sg);
            rde_arr_add(&_starts, (any)&_len);
            const fude_zoom_v2 _d = { _sg.b.x - _sg.a.x, _sg.b.y - _sg.a.y };
            _len += rde_math_sqrt_f64(_d.x * _d.x + _d.y * _d.y);
        }
    }
    fzc_seg_grid _grid;
    fzc_grid_make(&_grid, (const fzc_seg*)_segs.memory, (u32)rde_arr_length(&_segs));
    rde_arr TYPE(u32) _near = rde_arr_new(sizeof(u32), _heap);
    rde_arr _junction = rde_arr_new(sizeof(u32), _heap);   // (host wire, terminal) pairs, with the length along it
    rde_arr _along    = rde_arr_new(sizeof(f64), _heap);
    for(u32 _t = _wire_first; _t < _nt; _t++) {
        const fude_zoom_v2 _p = _tm[_t].at;
        const f64* _r = &((const f64*)_refs.memory)[4u * _tm[_t].wire];
        if(_r[2u * _tm[_t].pin] != 0.0 && _r[2u * _tm[_t].pin + 1u] >= 0.0) {
            continue;   // (an end at a pin: a wire passing over the pin is not joined to it)
        }
        fzc_grid_near(&_grid, _p, &_near);
        u32 _joined = FUDE_ZOOM_NONE;   // (the wire this end was last joined to: the rest of its segments passed over)
        for(u32 _j = 0; _j < (u32)rde_arr_length(&_near); _j++) {
            const u32 _i = ((const u32*)_near.memory)[_j];
            const fzc_seg* _sg = &((const fzc_seg*)_segs.memory)[_i];
            const u32 _w = _sg->owner;
            if(_w == _tm[_t].wire || _w == _joined) {
                continue;
            }
            const fude_zoom_v2 _d = { _sg->b.x - _sg->a.x, _sg->b.y - _sg->a.y };
            const f64 _l = rde_math_sqrt_f64(_d.x * _d.x + _d.y * _d.y);
            const f64 _u = rde_math_segment_closest_t_2d_f64(_p, _sg->a, _sg->b);
            const f64 _off = rde_math_point_segment_distance_2d_f64(_p, _sg->a, _sg->b);
            const f64 _at = ((const f64*)_starts.memory)[_i] + _u * _l;
            if(_off <= _reach && _at > _reach && _at < _wires[_w].length - _reach) {
                rde_disjoint_set_union(_net, _t, _wire_first + 2u * _w);
                _held[_t] = 1u;
                const u32 _pair[2] = { _w, _t };
                rde_arr_add(&_junction, (any)&_pair[0]);
                rde_arr_add(&_junction, (any)&_pair[1]);
                rde_arr_add(&_along, (any)&_at);
                _joined = _w;
            }
        }
    }
    fzc_grid_free(&_grid);
    rde_arr_free(&_near);
    rde_arr_free(&_starts);
    rde_arr_free(&_segs);
    _build->junction = _junction;
    _build->along    = _along;
}

// Joins a wire's end held by nothing yet that lies on a pin's lead to that pin (the first such pin, in the terminals'
// order); then marks the ends held by nothing as loose. Each end looks only at the leads near it (fzc_seg_grid).
RDE_INTERNAL void fzc_build_leads(fzc_build* _build) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_circuit_wire* _wires = _build->wires;
    const u32 _nw = _build->nw;
    const rde_arr _leads = _build->leads;
    const u32 _wire_first = _build->wire_first;
    const u32 _nt = _build->nt;
    fzc_term* _tm = (fzc_term*)_build->terms.memory;
    rde_disjoint_set* _net = &_build->net;
    rde_disjoint_set* _touch = &_build->touch;
    u8* _held = (u8*)_build->held_arr.memory;
    // A wire's end held by nothing yet, on a pin's lead: on the pin (it looks joined there: it is). Each lead of some
    // length a segment, owned by its pin's terminal, its reach its part's: the most an end is let be off it.
    const fude_zoom_v2* _lead = (const fude_zoom_v2*)_leads.memory;
    rde_arr TYPE(fzc_seg) _segs = rde_arr_new(sizeof(fzc_seg), _heap);
    for(u32 _q = 0; _q < _wire_first; _q++) {
        const fude_zoom_v2 _a = _tm[_q].at, _b = _lead[_q];
        const fude_zoom_v2 _d = { _b.x - _a.x, _b.y - _a.y };
        if(_d.x * _d.x + _d.y * _d.y > 0.0) {
            const fzc_seg _sg = { _q, _a, _b, 0.3 * _tm[_q].unit };
            rde_arr_add(&_segs, (any)&_sg);
        }
    }
    fzc_seg_grid _grid;
    fzc_grid_make(&_grid, (const fzc_seg*)_segs.memory, (u32)rde_arr_length(&_segs));
    rde_arr TYPE(u32) _near = rde_arr_new(sizeof(u32), _heap);
    for(u32 _t = _wire_first; _t < _nt; _t++) {
        if(_held[_t]) {
            continue;
        }
        const fude_zoom_v2 _p = _tm[_t].at;
        fzc_grid_near(&_grid, _p, &_near);
        for(u32 _j = 0; _j < (u32)rde_arr_length(&_near); _j++) {
            const fzc_seg* _sg = &((const fzc_seg*)_segs.memory)[((const u32*)_near.memory)[_j]];
            const u32 _q = _sg->owner;
            if(rde_math_point_segment_distance_2d_f64(_p, _sg->a, _sg->b) <= 0.3 * rde_math_min_f64(_tm[_q].unit, _tm[_t].unit)) {
                rde_disjoint_set_union(_net, _t, _q);
                rde_disjoint_set_union(_touch, _t, _q);
                _held[_t] = 1u;
                break;
            }
        }
    }
    fzc_grid_free(&_grid);
    rde_arr_free(&_near);
    rde_arr_free(&_segs);
    for(u32 _w = 0; _w < _nw; _w++) {
        _wires[_w].loose[0] = !_held[_wire_first + 2u * _w];
        _wires[_w].loose[1] = !_held[_wire_first + 2u * _w + 1u];
    }
}

// Cuts each wire into pieces where other wires' ends meet it (FUDE_ZOOM_WIRE_PIECES at most).
RDE_INTERNAL void fzc_build_cuts(fzc_build* _build) {
    fude_zoom_circuit_wire* _wires = _build->wires;
    const rde_arr _junction = _build->junction;
    const rde_arr _along = _build->along;
    // Each wire's pieces: cut where other wires' ends meet it.
    const u32 _nj = (u32)rde_arr_length(&_along);
    for(u32 _j = 0; _j < _nj; _j++) {
        fude_zoom_circuit_wire* _w = &_wires[((const u32*)_junction.memory)[2u * _j]];
        const f64 _at = ((const f64*)_along.memory)[_j];
        if(_w->pieces >= FUDE_ZOOM_WIRE_PIECES) {
            continue;
        }
        u32 _k = _w->pieces;   // (sorted: in after the cuts below it)
        while(_k > 0u && _w->cut[_k - 1u] > _at) {
            _k--;
        }
        if(_k == 0u || rde_math_abs_f64(_w->cut[_k - 1u] - _at) < 1e-9 * rde_math_max_f64(_w->length, 1.0)) {
            continue;
        }
        rde_memmove(&_w->cut[_k + 1u], &_w->cut[_k], (usize)(_w->pieces + 1u - _k) * sizeof(f64));
        _w->cut[_k] = _at;
        _w->pieces++;
    }
}

// Joins the same-named pins inside a part (a board's GNDs) and a breadboard's strips.
RDE_INTERNAL void fzc_build_same_names(fzc_build* _build) {
    fude_zoom_circuit_part* _parts = _build->parts;
    const u32 _np = _build->np;
    const rde_arr _first = _build->first;
    rde_disjoint_set* _net = &_build->net;
    // Same-named pins inside a part (a board's GNDs, its 5Vs) are one; a breadboard's strips each one.
    for(u32 _i = 0; _i < _np; _i++) {
        const fude_zoom_part* _part = _parts[_i].part;
        const u32 _f = ((const u32*)_first.memory)[_i];
        if(_part->look == FZC_BREADBOARD) {
            // (each strip's first hole, by the strip's number: fude_zoom_breadboard_strip's, a rail's row or 100 or
            // 200 and a column, so under 256 for every hole of the one breadboard the catalogue has,
            // FUDE_ZOOM_BREADBOARD_ROWS × FUDE_ZOOM_BREADBOARD_COLS. The bound is that part's design, not what a person
            // draws; the modulo only guards the array, and a bigger breadboard would need more room here.)
            u32 _strip_at[256];
            for(u32 _k = 0; _k < 256u; _k++) {
                _strip_at[_k] = FUDE_ZOOM_NONE;
            }
            for(u32 _k = 0; _k < _part->pin_count; _k++) {
                const u32 _st = fude_zoom_breadboard_strip(_k) % 256u;
                if(_strip_at[_st] == FUDE_ZOOM_NONE) {
                    _strip_at[_st] = _f + _k;
                } else {
                    rde_disjoint_set_union(_net, _strip_at[_st], _f + _k);
                }
            }
            continue;
        }
        for(u32 _a = 0; _a < _part->pin_count; _a++) {
            if(_part->pins[_a].name[0] == 0) {
                continue;
            }
            for(u32 _b = _a + 1u; _b < _part->pin_count; _b++) {
                // (exactly the same name: a board's GNDs are one; a CD4511's a and A are not)
                if(rde_strcmp(_part->pins[_a].name, _part->pins[_b].name) == 0) {
                    rde_disjoint_set_union(_net, _f + _a, _f + _b);
                }
            }
        }
    }
}

// Finds ground: every ground part's pin (joined), else a board's GND, else the first source's minus.
RDE_INTERNAL void fzc_build_ground(fzc_build* _build) {
    fude_zoom_circuit* _c = _build->c;
    fude_zoom_circuit_part* _parts = _build->parts;
    const u32 _np = _build->np;
    const rde_arr _first = _build->first;
    rde_disjoint_set* _net = &_build->net;
    // Ground: every ground part's, else a board's GND, else the first source's minus.
    u32 _ground = FUDE_ZOOM_NONE;
    for(u32 _i = 0; _i < _np; _i++) {
        if(_parts[_i].part->model == FUDE_ZOOM_MODEL_GROUND) {
            const u32 _t = ((const u32*)_first.memory)[_i];
            if(_ground == FUDE_ZOOM_NONE) {
                _ground = _t;
            } else {
                rde_disjoint_set_union(_net, _ground, _t);
            }
        }
    }
    _c->grounded = _ground != FUDE_ZOOM_NONE;
    for(u32 _i = 0; _i < _np && _ground == FUDE_ZOOM_NONE; _i++) {
        const fude_zoom_part* _part = _parts[_i].part;
        if(_part->model == FUDE_ZOOM_MODEL_BOARD) {
            for(u32 _k = 0; _k < _part->pin_count; _k++) {
                if(fzc_same_name(_part->pins[_k].name, "GND")) {
                    _ground = ((const u32*)_first.memory)[_i] + _k;
                    break;
                }
            }
        }
    }
    for(u32 _i = 0; _i < _np && _ground == FUDE_ZOOM_NONE; _i++) {
        const u8 _m = _parts[_i].part->model;
        if(_m == FUDE_ZOOM_MODEL_VSOURCE || _m == FUDE_ZOOM_MODEL_BATTERY || _m == FUDE_ZOOM_MODEL_ACSOURCE || _m == FUDE_ZOOM_MODEL_CLOCK || _m == FUDE_ZOOM_MODEL_ISOURCE) {
            _ground = ((const u32*)_first.memory)[_i] + 1u;
        }
    }
    _build->ground = _ground;
}

// Numbers the nodes (ground 0): each set a part with a model has a pin in, but a pin joined to nothing; each part's
// pins' nodes and each wire's node.
RDE_INTERNAL void fzc_build_nodes(fzc_build* _build) {
    fude_zoom_circuit* _c = _build->c;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_circuit_part* _parts = _build->parts;
    fude_zoom_circuit_wire* _wires = _build->wires;
    const u32 _np = _build->np;
    const u32 _nw = _build->nw;
    const rde_arr _first = _build->first;
    const u32 _wire_first = _build->wire_first;
    const u32 _nt = _build->nt;
    rde_disjoint_set* _net = &_build->net;
    const u32 _ground = _build->ground;
    // Nodes: what something works on (a part's pin with a model), ground 0.
    rde_arr TYPE(u32) _node_of_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_node_of_arr, _nt);
    u32* _node_of = (u32*)_node_of_arr.memory;
    for(u32 _i = 0; _i < _nt; _i++) {
        _node_of[_i] = FUDE_ZOOM_NONE;
    }
    const u32 _ground_root = _ground != FUDE_ZOOM_NONE ? rde_disjoint_set_find(_net, _ground) : FUDE_ZOOM_NONE;
    if(_ground_root != FUDE_ZOOM_NONE) {
        _node_of[_ground_root] = 0u;
    }
    // (a pin joined to nothing: no node — nothing flows through it, an op-amp's supply left open is its own)
    rde_arr TYPE(u32) _size_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_size_arr, _nt);
    u32* _size = (u32*)_size_arr.memory;
    for(u32 _i = 0; _i < _nt; _i++) {
        _size[rde_disjoint_set_find(_net, _i)]++;
    }
    // Each part's pins' places in the circuit's pin_nodes and pin_amps (all its pins, FZC_PIN_ROOM at least), joined to
    // nothing and carrying nothing until found.
    u32 _room = 0;
    for(u32 _i = 0; _i < _np; _i++) {
        _parts[_i].pin_at   = _room;
        _parts[_i].pin_room = _parts[_i].part->pin_count > FZC_PIN_ROOM ? _parts[_i].part->pin_count : FZC_PIN_ROOM;
        _room += _parts[_i].pin_room;
    }
    rde_arr_resize(&_c->pin_nodes, _room > 0u ? _room : 1u);
    rde_arr_resize(&_c->pin_amps, _room > 0u ? _room : 1u);
    for(u32 _k = 0; _k < _room; _k++) {
        ((u32*)_c->pin_nodes.memory)[_k] = FUDE_ZOOM_NONE;
        ((f64*)_c->pin_amps.memory)[_k]  = 0.0;
    }
    for(u32 _i = 0; _i < _np; _i++) {
        _parts[_i].node  = (u32*)_c->pin_nodes.memory + _parts[_i].pin_at;
        _parts[_i].pin_i = (f64*)_c->pin_amps.memory + _parts[_i].pin_at;
    }
    u32 _nodes = 1u;
    for(u32 _i = 0; _i < _np; _i++) {
        const fude_zoom_part* _part = _parts[_i].part;
        const u32 _f = ((const u32*)_first.memory)[_i];
        if(_part->model == FUDE_ZOOM_MODEL_NONE) {
            continue;
        }
        for(u32 _k = 0; _k < _part->pin_count; _k++) {
            const u32 _r = rde_disjoint_set_find(_net, _f + _k);
            if(_size[_r] < 2u && _r != _ground_root && _part->model != FUDE_ZOOM_MODEL_TIMER555) {
                continue;   // (a 555's control pin is its own divider's, left open)
            }
            if(_node_of[_r] == FUDE_ZOOM_NONE) {
                _node_of[_r] = _nodes++;
            }
            _parts[_i].node[_k] = _node_of[_r];
        }
    }
    _c->nodes = _nodes;
    rde_arr_resize(&_c->v, _nodes);   // (those still there as they were: where Newton starts)
    for(u32 _w = 0; _w < _nw; _w++) {
        _wires[_w].node = _node_of[rde_disjoint_set_find(_net, _wire_first + 2u * _w)];
    }
    _build->node_of_arr = _node_of_arr;
    _build->size_arr    = _size_arr;
}

// Numbers the vertices for the wires' currents (touching terminals one), each wire piece's ends' and each pin's.
RDE_INTERNAL void fzc_build_vertices(fzc_build* _build) {
    fude_zoom_circuit* _c = _build->c;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_circuit_part* _parts = _build->parts;
    fude_zoom_circuit_wire* _wires = _build->wires;
    const u32 _np = _build->np;
    const u32 _nw = _build->nw;
    const rde_arr _first = _build->first;
    const u32 _wire_first = _build->wire_first;
    const u32 _nt = _build->nt;
    rde_disjoint_set* _touch = &_build->touch;
    const rde_arr _junction = _build->junction;
    const rde_arr _along = _build->along;
    const u32 _nj = (u32)rde_arr_length(&_along);
    // Vertices (for the wires' currents): touching terminals one; a junction's cut is the touching end's.
    rde_arr TYPE(u32) _vertex_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_vertex_arr, _nt);
    u32* _vertex = (u32*)_vertex_arr.memory;
    u32 _nv = 0;
    for(u32 _i = 0; _i < _nt; _i++) {
        _vertex[_i] = FUDE_ZOOM_NONE;
    }
    for(u32 _i = 0; _i < _nt; _i++) {
        const u32 _r = rde_disjoint_set_find(_touch, _i);
        if(_vertex[_r] == FUDE_ZOOM_NONE) {
            _vertex[_r] = _nv++;
        }
        _vertex[_i] = _vertex[_r];
    }
    _c->vertices = _nv;
    rde_arr_clear(&_c->cut_vertex);
    u32* _cv = _nw > 0u ? (u32*)rde_arr_add_n(&_c->cut_vertex, _nw * (FUDE_ZOOM_WIRE_PIECES + 1u)) : NULL;   // (parts alone, no wire: none)
    for(u32 _w = 0; _w < _nw; _w++) {
        for(u32 _k = 0; _k <= FUDE_ZOOM_WIRE_PIECES; _k++) {
            _cv[_w * (FUDE_ZOOM_WIRE_PIECES + 1u) + _k] = FUDE_ZOOM_NONE;
        }
        _cv[_w * (FUDE_ZOOM_WIRE_PIECES + 1u)]                     = _vertex[_wire_first + 2u * _w];
        _cv[_w * (FUDE_ZOOM_WIRE_PIECES + 1u) + _wires[_w].pieces] = _vertex[_wire_first + 2u * _w + 1u];
    }
    for(u32 _j = 0; _j < _nj; _j++) {
        const u32 _w = ((const u32*)_junction.memory)[2u * _j], _t = ((const u32*)_junction.memory)[2u * _j + 1u];
        const f64 _at = ((const f64*)_along.memory)[_j];
        for(u32 _k = 1; _k < _wires[_w].pieces; _k++) {
            if(rde_math_abs_f64(_wires[_w].cut[_k] - _at) < 1e-9 * rde_math_max_f64(_wires[_w].length, 1.0)) {
                _cv[_w * (FUDE_ZOOM_WIRE_PIECES + 1u) + _k] = _vertex[_t];
            }
        }
    }
    rde_arr_clear(&_c->pin_vertex);
    const u32 _room = (u32)rde_arr_length(&_c->pin_nodes);   // (each part's pin_room places, as fzc_build_nodes made them)
    u32* _pv = (u32*)rde_arr_add_n(&_c->pin_vertex, _room);
    for(u32 _k = 0; _k < _room; _k++) {
        _pv[_k] = FUDE_ZOOM_NONE;
    }
    for(u32 _i = 0; _i < _np; _i++) {
        const u32 _f = ((const u32*)_first.memory)[_i];
        for(u32 _k = 0; _k < _parts[_i].part->pin_count; _k++) {
            _pv[_parts[_i].pin_at + _k] = _vertex[_f + _k];
        }
    }
    _build->vertex_arr = _vertex_arr;
}

// See the header. In steps, each a function above with its comment: the parts (their state kept, their text read,
// their limits, their grid) and the wires in scope; the terminals (each pin, each wire's end, each pin's lead) and
// which touch; the wires cut where others' ends meet them; same-named pins and a breadboard's strips joined; ground;
// the nodes; the vertices for the wires' currents; then its logic, its rails held, its blocks and its step.
u32 fude_zoom_circuit_build_in(fude_zoom_circuit* _c, const fude_zoom_scene* _s, const u8* _scope) {
    fzc_build _build;
    rde_memset(&_build, 0, sizeof(_build));
    _build.c     = _c;
    _build.s     = _s;
    _build.scope = _scope;
    fzc_build_read(&_build);
    if(_scope != NULL && rde_arr_length(&_c->wires) > 0u) {
        fzc_build_scope(&_build);
    }
    fude_zoom_circuit_part* _parts = (fude_zoom_circuit_part*)_c->parts.memory;
    fude_zoom_circuit_wire* _wires = (fude_zoom_circuit_wire*)_c->wires.memory;
    const u32 _np = (u32)rde_arr_length(&_c->parts), _nw = (u32)rde_arr_length(&_c->wires);
    if(!(_c->unit > 0.0)) {
        _c->unit = 1.0;
    }
    _build.parts = _parts;
    _build.wires = _wires;
    _build.np    = _np;
    _build.nw    = _nw;
    fzc_build_terminals(&_build);
    fzc_build_kept(&_build);
    fzc_build_touching(&_build);
    fzc_build_junctions(&_build);
    fzc_build_leads(&_build);
    fzc_build_cuts(&_build);
    fzc_build_same_names(&_build);
    fzc_build_ground(&_build);
    fzc_build_nodes(&_build);
    fzc_build_vertices(&_build);
    // Its logic, made from what the parts are now (logic.h).
    fude_zoom_logic_build(_c->logic, _c);
    // Its rails held, its pins that sense (the logic's inputs among them), its blocks.
    rde_arr_clear(&_c->couplings);
    rde_arr_clear(&_c->joins);
    fzc_held(_c);
    fzc_blocks(_c);
    // Its step of time (fzc_step_most); what it is now a jump.
    fude_zoom_circuit_jump(_c);
    fzc_step_most(_c);
    rde_disjoint_set_free(&_build.net);
    rde_disjoint_set_free(&_build.touch);
    rde_arr_free(&_build.size_arr);
    rde_arr_free(&_build.node_of_arr);
    rde_arr_free(&_build.vertex_arr);
    rde_arr_free(&_build.terms);
    rde_arr_free(&_build.first);
    rde_arr_free(&_build.refs);
    rde_arr_free(&_build.units);
    rde_arr_free(&_build.junction);
    rde_arr_free(&_build.along);
    rde_arr_free(&_build.leads);
    rde_arr_free(&_build.held_arr);
    rde_arr_free(&_build.part_of_object);
    return _np;
}
