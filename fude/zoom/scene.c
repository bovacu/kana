// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/scene.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See scene.h.
// ===========================================================================

RDE_INTERNAL fude_zoom_frame*  fude_zoom_frames(const fude_zoom_scene* _s)  { return (fude_zoom_frame*)_s->frames.memory; }
RDE_INTERNAL fude_zoom_object* fude_zoom_objects(const fude_zoom_scene* _s) { return (fude_zoom_object*)_s->objects.memory; }
RDE_INTERNAL fude_zoom_blob*   fude_zoom_blobs(const fude_zoom_scene* _s)   { return (fude_zoom_blob*)_s->blobs.memory; }
RDE_INTERNAL fude_zoom_action* fude_zoom_actions(const fude_zoom_scene* _s) { return (fude_zoom_action*)_s->actions.memory; }
RDE_INTERNAL u32*              fude_zoom_targets(const fude_zoom_scene* _s) { return (u32*)_s->targets.memory; }
RDE_INTERNAL fude_zoom_place*  fude_zoom_places(const fude_zoom_scene* _s)  { return (fude_zoom_place*)_s->places.memory; }
RDE_INTERNAL u32*              fude_zoom_u32s(const rde_arr* _a)            { return (u32*)_a->memory; }

RDE_INTERNAL u32 fude_zoom_len(const rde_arr* _a) {
    return (u32)rde_arr_length(_a);
}

RDE_INTERNAL int fude_zoom_by_u32(const void* _a, const void* _b) {
    const u32 _x = *(const u32*)_a, _y = *(const u32*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// --- little-endian 64-bit fields (kfile.h has up to 32) ------------------------------------

void fude_put_u64(fude_bytes* _b, u64 _v) {
    fude_put_u32(_b, (u32)_v);
    fude_put_u32(_b, (u32)(_v >> 32));
}

void fude_put_f64(fude_bytes* _b, f64 _v) {
    u64 _bits;
    memcpy(&_bits, &_v, sizeof(_bits));
    fude_put_u64(_b, _bits);
}

u64 fude_get_u64(fude_reader* _r) {
    const u64 _lo = fude_get_u32(_r);
    const u64 _hi = fude_get_u32(_r);
    return _lo | (_hi << 32);
}

f64 fude_get_f64(fude_reader* _r) {
    const u64 _bits = fude_get_u64(_r);
    f64       _v;
    memcpy(&_v, &_bits, sizeof(_v));
    return _v;
}

// --- id → slot ------------------------------------------------------------------------------

RDE_INTERNAL u32 fude_zoom_hash(fude_zoom_id _id) {
    _id ^= _id >> 33;
    _id *= 0xff51afd7ed558ccdull;
    _id ^= _id >> 33;
    return (u32)_id;
}

RDE_INTERNAL void fude_zoom_map_free(fude_zoom_map* _m) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    if(_m->keys != NULL) {
        _heap->free(_heap->allocator, _m->keys);
        _heap->free(_heap->allocator, _m->values);
    }
    memset(_m, 0, sizeof(*_m));
}

RDE_INTERNAL void fude_zoom_map_set(fude_zoom_map* _m, fude_zoom_id _id, u32 _value);

RDE_INTERNAL void fude_zoom_map_grow(fude_zoom_map* _m) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_map _old = *_m;
    _m->capacity = _old.capacity == 0 ? 64u : _old.capacity * 2u;
    _m->count    = 0;
    _m->keys     = _heap->calloc(_heap->allocator, _m->capacity, sizeof(fude_zoom_id));
    _m->values   = _heap->calloc(_heap->allocator, _m->capacity, sizeof(u32));
    for(u32 _i = 0; _i < _old.capacity; _i++) {
        if(_old.keys[_i] != 0) {
            fude_zoom_map_set(_m, _old.keys[_i], _old.values[_i]);
        }
    }
    if(_old.keys != NULL) {
        _heap->free(_heap->allocator, _old.keys);
        _heap->free(_heap->allocator, _old.values);
    }
}

// Ids are never 0 (the counter starts at 1), so 0 marks an empty slot.
RDE_INTERNAL void fude_zoom_map_set(fude_zoom_map* _m, fude_zoom_id _id, u32 _value) {
    if((_m->count + 1u) * 2u > _m->capacity) {
        fude_zoom_map_grow(_m);
    }
    u32 _at = fude_zoom_hash(_id) & (_m->capacity - 1u);
    while(_m->keys[_at] != 0 && _m->keys[_at] != _id) {
        _at = (_at + 1u) & (_m->capacity - 1u);
    }
    if(_m->keys[_at] == 0) {
        _m->count++;
    }
    _m->keys[_at]   = _id;
    _m->values[_at] = _value;
}

RDE_INTERNAL u32 fude_zoom_map_get(const fude_zoom_map* _m, fude_zoom_id _id) {
    if(_m->capacity == 0 || _id == 0) {
        return FUDE_ZOOM_NONE;
    }
    u32 _at = fude_zoom_hash(_id) & (_m->capacity - 1u);
    while(_m->keys[_at] != 0) {
        if(_m->keys[_at] == _id) {
            return _m->values[_at];
        }
        _at = (_at + 1u) & (_m->capacity - 1u);
    }
    return FUDE_ZOOM_NONE;
}

// --- the scene ---------------------------------------------------------------------------

RDE_INTERNAL void fude_zoom_frame_init(fude_zoom_frame* _f) {
    memset(_f, 0, sizeof(*_f));
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _f->order   = rde_arr_new(sizeof(u32), _heap);
    _f->kids    = rde_arr_new(sizeof(u32), _heap);
    _f->arrival = rde_arr_new(sizeof(u32), _heap);
    _f->dirty   = rde_arr_new(sizeof(u8), _heap);
    _f->offsets = rde_arr_new(sizeof(u64), _heap);
    _f->sizes   = rde_arr_new(sizeof(u32), _heap);
    fude_zoom_index_init(&_f->index);
    _f->parent  = FUDE_ZOOM_NONE;
    _f->object  = FUDE_ZOOM_NONE;
    _f->anchor  = fude_zoom_box_empty();
    _f->z_next  = FUDE_ZOOM_Z_STEP;
    _f->xf      = (fude_zoom_xform){ 0.0, 0.0, 1.0, 0.0 };
}

RDE_INTERNAL void fude_zoom_frame_destroy(fude_zoom_frame* _f) {
    rde_arr* _arrays[] = { &_f->order, &_f->kids, &_f->arrival, &_f->dirty, &_f->offsets, &_f->sizes };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    fude_zoom_index_destroy(&_f->index);
}

void fude_zoom_scene_init(fude_zoom_scene* _s, u32 _device) {
    memset(_s, 0, sizeof(*_s));
    // A canvas has no natural size limit: everything on the standard heap (the
    // engine's default allocator is a fixed budget, and running it out is fatal).
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _s->frames     = rde_arr_new(sizeof(fude_zoom_frame), _heap);
    _s->objects    = rde_arr_new(sizeof(fude_zoom_object), _heap);
    _s->blobs      = rde_arr_new(sizeof(fude_zoom_blob), _heap);
    _s->free_blobs = rde_arr_new(sizeof(u32), _heap);
    _s->actions    = rde_arr_new(sizeof(fude_zoom_action), _heap);
    _s->targets    = rde_arr_new(sizeof(u32), _heap);
    _s->places     = rde_arr_new(sizeof(fude_zoom_place), _heap);
    _s->journal    = fude_bytes_new(4096u);
    _s->device         = _device;
    _s->history_budget = FUDE_ZOOM_HISTORY_BUDGET;
    for(u32 _l = 0; _l < FUDE_ZOOM_LAYERS; _l++) {
        _s->layer_rank[_l] = (u8)_l;
    }

    fude_zoom_frame* _root = rde_arr_add(&_s->frames, NULL);
    fude_zoom_frame_init(_root);
    _root->id = fude_zoom_scene_new_id(_s);
    fude_zoom_map_set(&_s->frame_ids, _root->id, 0);
    _s->root   = 0;
    _s->home   = 0;
    _s->camera = (fude_zoom_camera){ 0, { 0.0, 0.0 }, 1.0 };
}

void fude_zoom_scene_destroy(fude_zoom_scene* _s) {
    if(rde_arr_is_inited(&_s->frames)) {
        for(u32 _i = 0; _i < fude_zoom_len(&_s->frames); _i++) {
            fude_zoom_frame_destroy(&fude_zoom_frames(_s)[_i]);
        }
    }
    if(rde_arr_is_inited(&_s->blobs)) {
        rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
        for(u32 _i = 0; _i < fude_zoom_len(&_s->blobs); _i++) {
            if(fude_zoom_blobs(_s)[_i].data != NULL) {
                _heap->free(_heap->allocator, fude_zoom_blobs(_s)[_i].data);
            }
        }
    }
    rde_arr* _arrays[] = { &_s->frames, &_s->objects, &_s->blobs, &_s->free_blobs, &_s->actions, &_s->targets, &_s->places, &_s->journal };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    fude_zoom_map_free(&_s->object_ids);
    fude_zoom_map_free(&_s->frame_ids);
    memset(_s, 0, sizeof(*_s));
}

fude_zoom_id fude_zoom_scene_new_id(fude_zoom_scene* _s) {
    _s->counter++;
    return ((fude_zoom_id)_s->device << 32) | (fude_zoom_id)_s->counter;
}

// An id the file brought: new ids must not repeat it.
RDE_INTERNAL void fude_zoom_scene_saw_id(fude_zoom_scene* _s, fude_zoom_id _id) {
    if((u32)(_id >> 32) == _s->device && (u32)_id > _s->counter) {
        _s->counter = (u32)_id;
    }
}

fude_zoom_frame* fude_zoom_scene_frame(const fude_zoom_scene* _s, u32 _slot) {
    return &fude_zoom_frames(_s)[_slot];
}

fude_zoom_object* fude_zoom_scene_object(const fude_zoom_scene* _s, u32 _index) {
    return &fude_zoom_objects(_s)[_index];
}

u32 fude_zoom_scene_frame_count(const fude_zoom_scene* _s) {
    return fude_zoom_len(&_s->frames);
}

u32 fude_zoom_scene_object_count(const fude_zoom_scene* _s) {
    return fude_zoom_len(&_s->objects);
}

u32 fude_zoom_scene_find_object(const fude_zoom_scene* _s, fude_zoom_id _id) {
    return fude_zoom_map_get(&_s->object_ids, _id);
}

u32 fude_zoom_scene_find_frame(const fude_zoom_scene* _s, fude_zoom_id _id) {
    return fude_zoom_map_get(&_s->frame_ids, _id);
}

u32 fude_zoom_scene_depth(const fude_zoom_scene* _s, u32 _frame) {
    u32 _d = 0;
    while(fude_zoom_frames(_s)[_frame].parent != FUDE_ZOOM_NONE && _d < 100000u) {
        _frame = fude_zoom_frames(_s)[_frame].parent;
        _d++;
    }
    return _d;
}

u32 fude_zoom_scene_alive_strokes(const fude_zoom_scene* _s, u64* _points) {
    u32 _n   = 0;
    u64 _pts = 0;
    for(u32 _i = 0; _i < fude_zoom_len(&_s->objects); _i++) {
        const fude_zoom_object* _o = &fude_zoom_objects(_s)[_i];
        if(_o->kind == FUDE_ZOOM_KIND_STROKE && (_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
            _n++;
            _pts += _o->count;
        }
    }
    if(_points != NULL) {
        *_points = _pts;
    }
    return _n;
}

RDE_INTERNAL void fude_zoom_journal_op_begin(fude_zoom_scene* _s, u8 _op, u32* _at) {
    fude_put_u8(&_s->journal, _op);
    *_at = fude_bytes_size(&_s->journal);
    fude_put_u32(&_s->journal, 0u);
}

RDE_INTERNAL void fude_zoom_journal_op_end(fude_zoom_scene* _s, u32 _at) {
    fude_chunk_end(&_s->journal, _at);   // the same 4-byte size fill-in a chunk's is
}

// --- frames ------------------------------------------------------------------------------

// Marks the bucket an object is in as changed (the next checkpoint writes it).
RDE_INTERNAL void fude_zoom_scene_touch(fude_zoom_scene* _s, const fude_zoom_object* _o) {
    if(_o->kind == FUDE_ZOOM_KIND_FRAME || _o->file_pos == FUDE_ZOOM_NONE) {
        return;
    }
    fude_zoom_frame* _f      = &fude_zoom_frames(_s)[_o->frame];
    const u32        _bucket = _o->file_pos / FUDE_ZOOM_BUCKET;
    while(fude_zoom_len(&_f->dirty) <= _bucket) {
        const u8 _zero = 0;
        rde_arr_add(&_f->dirty, (any)&_zero);
    }
    ((u8*)_f->dirty.memory)[_bucket] = 1u;
}

// Where in _f's order an object with key _z goes: after every key ≤ _z.
RDE_INTERNAL u32 fude_zoom_order_slot(const fude_zoom_scene* _s, const fude_zoom_frame* _f, u64 _z) {
    const u32* _order = fude_zoom_u32s(&_f->order);
    u32        _lo    = 0;
    u32        _hi    = fude_zoom_len(&_f->order);
    while(_lo < _hi) {
        const u32 _mid = (_lo + _hi) / 2u;
        if(fude_zoom_objects(_s)[_order[_mid]].z <= _z) {
            _lo = _mid + 1u;
        } else {
            _hi = _mid;
        }
    }
    return _lo;
}

RDE_INTERNAL void fude_zoom_order_insert(fude_zoom_scene* _s, u32 _frame, u32 _object) {
    fude_zoom_frame* _f  = &fude_zoom_frames(_s)[_frame];
    const u64        _z  = fude_zoom_objects(_s)[_object].z;
    const u32        _at = fude_zoom_order_slot(_s, _f, _z);
    rde_arr_insert(&_f->order, _at, &_object);
    if(_z >= _f->z_next) {
        _f->z_next = (_z / FUDE_ZOOM_Z_STEP + 1u) * FUDE_ZOOM_Z_STEP;
    }
}

// A FRAME object for frame _child in its parent, at key _z (0: on top).
RDE_INTERNAL void fude_zoom_scene_frame_object(fude_zoom_scene* _s, u32 _child, u64 _z) {
    fude_zoom_frame* _c = &fude_zoom_frames(_s)[_child];
    if(_c->parent == FUDE_ZOOM_NONE) {
        return;
    }
    if(_z == 0) {
        _z = fude_zoom_frames(_s)[_c->parent].z_next;
    }
    fude_zoom_object _o = {
        .id = _c->id, .gesture = _c->id, .frame = _c->parent, .child = _child, .blob = 0, .count = 0,
        .file_pos = FUDE_ZOOM_NONE, .history = 0, .z = _z, .box = _c->anchor, .kind = FUDE_ZOOM_KIND_FRAME,
        .flags = _c->removed ? 0u : FUDE_ZOOM_FLAG_ALIVE, .scale = 1.0,
    };
    rde_arr_add(&_s->objects, &_o);
    const u32 _index = fude_zoom_len(&_s->objects) - 1u;
    fude_zoom_map_set(&_s->object_ids, _o.id, _index);
    fude_zoom_frames(_s)[_child].object = _index;
    fude_zoom_order_insert(_s, _c->parent, _index);
    rde_arr_add(&fude_zoom_frames(_s)[_c->parent].kids, (any)&_index);
}

// The frame's own record into the journal, and its parents' first if the file
// does not have them yet (a frame is only written once something is drawn in
// it: zooming about leaves no trace).
RDE_INTERNAL void fude_zoom_journal_frame(fude_zoom_scene* _s, u32 _frame) {
    if(_s->replaying) {
        return;
    }
    fude_zoom_frame* _f = &fude_zoom_frames(_s)[_frame];
    if(_f->parent != FUDE_ZOOM_NONE && !fude_zoom_frames(_s)[_f->parent].saved) {
        fude_zoom_journal_frame(_s, _f->parent);
        _f = &fude_zoom_frames(_s)[_frame];
    }
    u32 _at;
    fude_zoom_journal_op_begin(_s, FUDE_ZOOM_OP_FRAME, &_at);
    fude_zoom_put_frame(_s, &_s->journal, _frame);
    fude_zoom_journal_op_end(_s, _at);
    _f->saved = true;
    _s->revision++;
}

void fude_zoom_scene_keep_frame(fude_zoom_scene* _s, u32 _frame) {
    if(_frame < fude_zoom_scene_frame_count(_s) && !fude_zoom_frames(_s)[_frame].saved && !fude_zoom_frames(_s)[_frame].removed) {
        fude_zoom_journal_frame(_s, _frame);
    }
}

u32 fude_zoom_scene_new_frame(fude_zoom_scene* _s, u32 _parent, fude_zoom_xform _xf, fude_zoom_box _anchor) {
    fude_zoom_frame* _f = rde_arr_add(&_s->frames, NULL);
    fude_zoom_frame_init(_f);
    _f->id     = fude_zoom_scene_new_id(_s);
    _f->parent = _parent;
    _f->xf     = _xf;
    _f->anchor = _anchor;
    const u32 _slot = fude_zoom_len(&_s->frames) - 1u;
    fude_zoom_map_set(&_s->frame_ids, _f->id, _slot);
    fude_zoom_scene_frame_object(_s, _slot, 0);
    return _slot;
}

u32 fude_zoom_scene_new_root(fude_zoom_scene* _s) {
    const u32 _old  = _s->root;
    fude_zoom_frame* _f = rde_arr_add(&_s->frames, NULL);
    fude_zoom_frame_init(_f);
    _f->id = fude_zoom_scene_new_id(_s);
    const u32 _slot = fude_zoom_len(&_s->frames) - 1u;
    fude_zoom_map_set(&_s->frame_ids, _f->id, _slot);

    fude_zoom_frame* _o = &fude_zoom_frames(_s)[_old];
    _o->parent = _slot;
    _o->xf     = (fude_zoom_xform){ 0.0, 0.0, FUDE_ZOOM_CHILD_SCALE, 0.0 };
    _o->anchor = fude_zoom_box_empty();
    // Its anchor in the new root: what it holds, as far as it is known (its
    // objects' boxes), at the new scale.
    fude_zoom_box _content = fude_zoom_box_empty();
    for(u32 _i = 0; _i < fude_zoom_len(&_o->order); _i++) {
        const fude_zoom_object* _ob = &fude_zoom_objects(_s)[fude_zoom_u32s(&_o->order)[_i]];
        if(_ob->flags & FUDE_ZOOM_FLAG_ALIVE) {
            _content = fude_zoom_box_union(_content, _ob->box);
        }
    }
    _o->anchor = fude_zoom_sim_box(fude_zoom_sim_from_xform(_o->xf), _content);
    _s->root   = _slot;
    fude_zoom_scene_frame_object(_s, _old, 0);

    if(!_s->replaying && fude_zoom_frames(_s)[_old].saved) {
        fude_zoom_journal_frame(_s, _slot);
        fude_zoom_journal_frame(_s, _old);
        u32 _at;
        fude_zoom_journal_op_begin(_s, FUDE_ZOOM_OP_ROOT, &_at);
        fude_put_u64(&_s->journal, fude_zoom_frames(_s)[_slot].id);
        fude_zoom_journal_op_end(_s, _at);
    }
    return _slot;
}

fude_zoom_sim fude_zoom_scene_sim(const fude_zoom_scene* _s, u32 _from, u32 _to) {
    // Each side's chain of frames up to the root; the nearest frame both have.
    const u32 _df = fude_zoom_scene_depth(_s, _from);
    const u32 _dt = fude_zoom_scene_depth(_s, _to);
    fude_zoom_sim _up_from = fude_zoom_sim_identity();
    fude_zoom_sim _up_to   = fude_zoom_sim_identity();
    u32 _a = _from, _b = _to, _da = _df, _db = _dt;
    while(_da > _db) {
        _up_from = fude_zoom_sim_compose(fude_zoom_sim_from_xform(fude_zoom_frames(_s)[_a].xf), _up_from);
        _a = fude_zoom_frames(_s)[_a].parent;
        _da--;
    }
    while(_db > _da) {
        _up_to = fude_zoom_sim_compose(fude_zoom_sim_from_xform(fude_zoom_frames(_s)[_b].xf), _up_to);
        _b = fude_zoom_frames(_s)[_b].parent;
        _db--;
    }
    while(_a != _b) {
        _up_from = fude_zoom_sim_compose(fude_zoom_sim_from_xform(fude_zoom_frames(_s)[_a].xf), _up_from);
        _up_to   = fude_zoom_sim_compose(fude_zoom_sim_from_xform(fude_zoom_frames(_s)[_b].xf), _up_to);
        _a = fude_zoom_frames(_s)[_a].parent;
        _b = fude_zoom_frames(_s)[_b].parent;
    }
    return fude_zoom_sim_compose(fude_zoom_sim_inverse(_up_to), _up_from);
}

b8 fude_zoom_scene_frame_shown(const fude_zoom_scene* _s, u32 _frame) {
    for(u32 _depth = 0; _frame != FUDE_ZOOM_NONE && _depth < 4096u; _depth++) {
        const fude_zoom_frame* _f = &fude_zoom_frames(_s)[_frame];
        if(_f->removed) {
            return false;
        }
        if(_f->object != FUDE_ZOOM_NONE) {
            const fude_zoom_object* _o = &fude_zoom_objects(_s)[_f->object];
            if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || fude_zoom_scene_hides(_s, _o)) {
                return false;
            }
        }
        _frame = _f->parent;
    }
    return true;
}

b8 fude_zoom_scene_frame_used(const fude_zoom_scene* _s, u32 _frame) {
    const fude_zoom_frame* _f = &fude_zoom_frames(_s)[_frame];
    for(u32 _i = 0; _i < fude_zoom_len(&_f->order); _i++) {
        const fude_zoom_object* _o = &fude_zoom_objects(_s)[fude_zoom_u32s(&_f->order)[_i]];
        if(_o->flags & FUDE_ZOOM_FLAG_ALIVE || (_o->kind != FUDE_ZOOM_KIND_FRAME && !(_o->flags & FUDE_ZOOM_FLAG_GARBAGE))) {
            return true;
        }
    }
    return false;
}

void fude_zoom_scene_frame_reach(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box) {
    fude_zoom_frame* _f = &fude_zoom_frames(_s)[_frame];
    if(_f->parent == FUDE_ZOOM_NONE || fude_zoom_box_is_empty(_box)) {
        return;
    }
    const fude_zoom_box _in_parent = fude_zoom_sim_box(fude_zoom_sim_from_xform(_f->xf), _box);
    const fude_zoom_box _grown     = fude_zoom_box_union(_f->anchor, _in_parent);
    if(memcmp(&_grown, &_f->anchor, sizeof(_grown)) == 0) {
        return;
    }
    _f->anchor = _grown;
    if(_f->object != FUDE_ZOOM_NONE) {
        fude_zoom_objects(_s)[_f->object].box = _grown;
    }
    if(_f->saved) {
        fude_zoom_journal_frame(_s, _frame);
    }
    // The parent's anchor holds it too.
    fude_zoom_scene_frame_reach(_s, _f->parent, _grown);
}

// --- payloads ----------------------------------------------------------------------------

RDE_INTERNAL u32 fude_zoom_blob_new(fude_zoom_scene* _s, const u8* _data, u32 _size) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_blob _b = { .data = _heap->malloc(_heap->allocator, _size > 0 ? _size : 1u), .size = _size, .refs = 1u };
    if(_size > 0) {
        memcpy(_b.data, _data, _size);
    }
    if(fude_zoom_len(&_s->free_blobs) > 0) {
        const u32 _slot = fude_zoom_u32s(&_s->free_blobs)[fude_zoom_len(&_s->free_blobs) - 1u];
        _s->free_blobs.count--;
        fude_zoom_blobs(_s)[_slot] = _b;
        return _slot + 1u;
    }
    rde_arr_add(&_s->blobs, &_b);
    return fude_zoom_len(&_s->blobs);
}

RDE_INTERNAL void fude_zoom_blob_release(fude_zoom_scene* _s, u32 _handle) {
    if(_handle == 0) {
        return;
    }
    fude_zoom_blob* _b = &fude_zoom_blobs(_s)[_handle - 1u];
    if(_b->refs > 1u) {
        _b->refs--;
        return;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _heap->free(_heap->allocator, _b->data);
    memset(_b, 0, sizeof(*_b));
    const u32 _slot = _handle - 1u;
    rde_arr_add(&_s->free_blobs, (any)&_slot);
}

// Dead, and no undo step names it: garbage — its points let go for good.
RDE_INTERNAL void fude_zoom_scene_collect(fude_zoom_scene* _s, u32 _object) {
    fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(_o->kind == FUDE_ZOOM_KIND_FRAME || (_o->flags & (FUDE_ZOOM_FLAG_ALIVE | FUDE_ZOOM_FLAG_GARBAGE)) || _o->history > 0) {
        return;
    }
    _o->flags |= FUDE_ZOOM_FLAG_GARBAGE;
    fude_zoom_blob_release(_s, _o->blob);
    _o->blob = 0;
    fude_zoom_scene_touch(_s, _o);
    fude_zoom_frames(_s)[_o->frame].dead++;
}

// --- objects -----------------------------------------------------------------------------

fude_zoom_v2 fude_zoom_paper_snap(fude_zoom_v2 _at, f64 _scale) {
    const f64 _step = FUDE_ZOOM_PAPER_ALIGN * _scale;
    return (fude_zoom_v2){ round(_at.x / _step) * _step, round(_at.y / _step) * _step };
}

i8 fude_zoom_quantum_for(f64 _z) {
    const f64 _q = floor(log2(1.0 / (16.0 * _z)));
    return (i8)(_q < -100.0 ? -100 : (_q > 100.0 ? 100 : (i32)_q));
}

fude_zoom_v2 fude_zoom_scene_local_at(const fude_zoom_object* _o, const fude_zoom_qpoint* _p) {
    return (fude_zoom_v2){ ldexp((f64)_p->x, _o->q), ldexp((f64)_p->y, _o->q) };
}

fude_zoom_v2 fude_zoom_scene_point_at(const fude_zoom_object* _o, const fude_zoom_qpoint* _p) {
    const fude_zoom_v2 _l = fude_zoom_scene_local_at(_o, _p);
    if(_o->rotation == 0.0 && _o->scale == 1.0) {
        return (fude_zoom_v2){ _o->t.x + _l.x, _o->t.y + _l.y };
    }
    return fude_zoom_sim_apply(fude_zoom_object_sim(_o), _l);
}

f32 fude_zoom_scene_local_radius_at(const fude_zoom_object* _o, const fude_zoom_qpoint* _p) {
    if(!(_o->flags & FUDE_ZOOM_FLAG_PRESSURE)) {
        return _o->radius;
    }
    return _o->radius * (f32)_p->pressure / 1023.0f;
}

f32 fude_zoom_scene_radius_at(const fude_zoom_object* _o, const fude_zoom_qpoint* _p) {
    return (f32)((f64)fude_zoom_scene_local_radius_at(_o, _p) * _o->scale);
}

fude_zoom_sim fude_zoom_object_sim(const fude_zoom_object* _o) {
    return (fude_zoom_sim){ _o->scale * cos(_o->rotation), _o->scale * sin(_o->rotation), _o->t.x, _o->t.y };
}

RDE_INTERNAL fude_zoom_box fude_zoom_stroke_box(const fude_zoom_object* _o, const fude_zoom_qpoint* _points, u32 _count) {
    fude_zoom_box _b = fude_zoom_box_empty();
    for(u32 _i = 0; _i < _count; _i++) {
        const fude_zoom_v2 _p = fude_zoom_scene_point_at(_o, &_points[_i]);
        const f64          _r = fude_zoom_scene_radius_at(_o, &_points[_i]);
        _b = fude_zoom_box_union(_b, (fude_zoom_box){ _p.x - _r, _p.y - _r, _p.x + _r, _p.y + _r });
    }
    return _b;
}

// An object record put into its frame: the table, the order, the arrival
// order, the index. _o->frame says which.
RDE_INTERNAL u32 fude_zoom_scene_put_in(fude_zoom_scene* _s, fude_zoom_object* _o) {
    fude_zoom_frame* _f = &fude_zoom_frames(_s)[_o->frame];
    _o->file_pos = fude_zoom_len(&_f->arrival);
    rde_arr_add(&_s->objects, _o);
    const u32 _index = fude_zoom_len(&_s->objects) - 1u;
    fude_zoom_map_set(&_s->object_ids, _o->id, _index);
    _f = &fude_zoom_frames(_s)[_o->frame];
    rde_arr_add(&_f->arrival, (any)&_index);
    const fude_zoom_object* _placed = &fude_zoom_objects(_s)[_index];
    if(!(_placed->flags & FUDE_ZOOM_FLAG_GARBAGE)) {
        fude_zoom_order_insert(_s, _placed->frame, _index);
        fude_zoom_index_insert(&fude_zoom_frames(_s)[_placed->frame].index, _index, _placed->box);
    }
    return _index;
}

u32 fude_zoom_scene_add_stroke(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _t, i8 _q, const fude_zoom_qpoint* _points, u32 _count,
                               u8 _channels, rde_color _color, f32 _radius, u8 _flags, fude_zoom_id _gesture, u64 _z) {
    return fude_zoom_scene_add_stroke_at(_s, _frame, (fude_zoom_place){ _t, 0.0, 1.0 }, _q, _points, _count, _channels, _color, _radius, _flags, _gesture, _z);
}

// A stroke's or a fill's points (codec.h) into a new object of kind _kind.
RDE_INTERNAL u32 fude_zoom_scene_add_points(fude_zoom_scene* _s, u8 _kind, u32 _frame, fude_zoom_place _place, i8 _q, const fude_zoom_qpoint* _points, u32 _count,
                                            u8 _channels, rde_color _color, f32 _radius, u8 _flags, fude_zoom_id _gesture, u64 _z) {
    const fude_zoom_v2 _t = _place.t;
    fude_bytes _bytes = fude_bytes_new(_count * 2u + 16u);
    fude_zoom_codec_encode(&_bytes, _points, _count, _channels);

    fude_zoom_object _o = {
        .id = fude_zoom_scene_new_id(_s), .frame = _frame, .child = FUDE_ZOOM_NONE, .count = _count,
        .z = _z != 0 ? _z : fude_zoom_frames(_s)[_frame].z_next, .t = _t, .color = _color, .radius = _radius,
        .kind = _kind, .flags = (u8)((_flags | FUDE_ZOOM_FLAG_ALIVE) & ~FUDE_ZOOM_FLAG_GARBAGE), .channels = _channels, .q = _q, .layer = _s->layer,
        .rotation = _place.rotation, .scale = _place.scale,
    };
    _o.gesture = _gesture != 0 ? _gesture : _o.id;
    _o.box     = fude_zoom_stroke_box(&_o, _points, _count);
    _o.blob    = fude_zoom_blob_new(_s, (const u8*)_bytes.memory, fude_bytes_size(&_bytes));
    rde_arr_free(&_bytes);

    if(!fude_zoom_frames(_s)[_frame].saved) {
        fude_zoom_journal_frame(_s, _frame);
    }
    const u32 _index = fude_zoom_scene_put_in(_s, &_o);
    fude_zoom_scene_touch(_s, &fude_zoom_objects(_s)[_index]);
    fude_zoom_scene_frame_reach(_s, _frame, fude_zoom_objects(_s)[_index].box);
    if(!_s->replaying) {
        u32 _at;
        fude_zoom_journal_op_begin(_s, FUDE_ZOOM_OP_OBJECT, &_at);
        fude_zoom_put_object(_s, &_s->journal, _index);
        fude_zoom_journal_op_end(_s, _at);
    }
    _s->revision++;
    return _index;
}

u32 fude_zoom_scene_add_stroke_at(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, i8 _q, const fude_zoom_qpoint* _points, u32 _count,
                                  u8 _channels, rde_color _color, f32 _radius, u8 _flags, fude_zoom_id _gesture, u64 _z) {
    return fude_zoom_scene_add_points(_s, FUDE_ZOOM_KIND_STROKE, _frame, _place, _q, _points, _count, _channels, _color, _radius, _flags, _gesture, _z);
}

u32 fude_zoom_scene_add_fill(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, i8 _q, const fude_zoom_qpoint* _points, u32 _count,
                             rde_color _color, u64 _z) {
    return fude_zoom_scene_add_fill_flags(_s, _frame, _place, _q, _points, _count, _color, 0u, _z);
}

u32 fude_zoom_scene_add_fill_flags(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, i8 _q, const fude_zoom_qpoint* _points, u32 _count,
                                   rde_color _color, u8 _flags, u64 _z) {
    // Its points' rings (fill.h) in the time channel, once the eraser has cut it.
    u8 _channels = 0u;
    for(u32 _i = 0; _i < _count && _channels == 0u; _i++) {
        _channels = _points[_i].time != 0u ? (u8)FUDE_ZOOM_CHANNEL_TIME : 0u;
    }
    return fude_zoom_scene_add_points(_s, FUDE_ZOOM_KIND_FILL, _frame, _place, _q, _points, _count, _channels, _color, 0.0f,
                                      (u8)(_flags & FUDE_ZOOM_FLAG_MARKER), 0, _z);
}

RDE_INTERNAL fude_zoom_box fude_zoom_shape_box(const fude_zoom_scene* _s, u32 _object);
RDE_INTERNAL void          fude_zoom_scene_rebox(fude_zoom_scene* _s, u32 _object);

u32 fude_zoom_scene_add_shape(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, u8 _type, const f64* _numbers, u32 _count,
                              rde_color _color, f32 _radius, u8 _flags, u64 _z) {
    return fude_zoom_scene_add_shape_fill(_s, _frame, _place, _type, _numbers, _count, _color, _radius, (u8)(_flags & ~FUDE_ZOOM_FLAG_FILL_OWN), (rde_color){ 0, 0, 0, 0 }, _z);
}

u32 fude_zoom_scene_add_shape_fill(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, u8 _type, const f64* _numbers, u32 _count,
                                   rde_color _color, f32 _radius, u8 _flags, rde_color _fill, u64 _z) {
    // Its numbers as they are in memory: little-endian doubles on every device the apps run on.
    fude_zoom_object _o = {
        .id = fude_zoom_scene_new_id(_s), .frame = _frame, .child = FUDE_ZOOM_NONE, .count = _count,
        .z = _z != 0 ? _z : fude_zoom_frames(_s)[_frame].z_next, .t = _place.t, .color = _color, .fill = _fill, .radius = _radius,
        .kind = FUDE_ZOOM_KIND_SHAPE, .flags = (u8)((_flags | FUDE_ZOOM_FLAG_ALIVE) & ~(FUDE_ZOOM_FLAG_GARBAGE | FUDE_ZOOM_FLAG_CONTINUES)), .channels = _type, .layer = _s->layer, .q = _s->style,
        .rotation = _place.rotation, .scale = _place.scale,
    };
    _o.gesture = _o.id;
    _o.blob    = fude_zoom_blob_new(_s, (const u8*)_numbers, _count * (u32)sizeof(f64));
    if(!fude_zoom_frames(_s)[_frame].saved) {
        fude_zoom_journal_frame(_s, _frame);
    }
    const u32 _index = fude_zoom_scene_put_in(_s, &_o);
    // Placed with no box: in again with its own.
    fude_zoom_objects(_s)[_index].box = fude_zoom_shape_box(_s, _index);
    fude_zoom_frame* _f = &fude_zoom_frames(_s)[_frame];
    fude_zoom_index_insert(&_f->index, _index, fude_zoom_objects(_s)[_index].box);
    _f->stale++;
    fude_zoom_scene_touch(_s, &fude_zoom_objects(_s)[_index]);
    fude_zoom_scene_frame_reach(_s, _frame, fude_zoom_objects(_s)[_index].box);
    if(!_s->replaying) {
        u32 _at;
        fude_zoom_journal_op_begin(_s, FUDE_ZOOM_OP_OBJECT, &_at);
        fude_zoom_put_object(_s, &_s->journal, _index);
        fude_zoom_journal_op_end(_s, _at);
    }
    _s->revision++;
    return _index;
}

u32 fude_zoom_scene_add_mark(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _at, f64 _z, u32 _number) {
    u8 _payload[sizeof(f64) + sizeof(u32)];
    memcpy(_payload, &_z, sizeof(f64));
    memcpy(_payload + sizeof(f64), &_number, sizeof(u32));
    fude_zoom_object _o = {
        .id = fude_zoom_scene_new_id(_s), .frame = _frame, .child = FUDE_ZOOM_NONE, .count = 0,
        .z = fude_zoom_frames(_s)[_frame].z_next, .t = _at, .box = { _at.x, _at.y, _at.x, _at.y },
        .kind = FUDE_ZOOM_KIND_MARK, .flags = FUDE_ZOOM_FLAG_ALIVE, .rotation = 0.0, .scale = 1.0,
    };
    _o.gesture = _o.id;
    _o.blob    = fude_zoom_blob_new(_s, _payload, (u32)sizeof(_payload));
    if(!fude_zoom_frames(_s)[_frame].saved) {
        fude_zoom_journal_frame(_s, _frame);
    }
    const u32 _index = fude_zoom_scene_put_in(_s, &_o);
    fude_zoom_scene_touch(_s, &fude_zoom_objects(_s)[_index]);
    if(!_s->replaying) {
        u32 _at_op;
        fude_zoom_journal_op_begin(_s, FUDE_ZOOM_OP_OBJECT, &_at_op);
        fude_zoom_put_object(_s, &_s->journal, _index);
        fude_zoom_journal_op_end(_s, _at_op);
    }
    _s->revision++;
    return _index;
}

u32 fude_zoom_scene_add_layer(fude_zoom_scene* _s, u32 _frame, u16 _index, u8 _flags, const c8* _name) {
    const usize _len = _name != NULL ? strlen(_name) : 0u;
    u8 _payload[3u + 64u];
    const usize _keep = _len < 64u ? _len : 64u;
    memcpy(_payload, &_index, sizeof(u16));
    _payload[2] = _flags;
    if(_keep > 0) {
        memcpy(_payload + 3u, _name, _keep);
    }
    fude_zoom_object _o = {
        .id = fude_zoom_scene_new_id(_s), .frame = _frame, .child = FUDE_ZOOM_NONE, .count = 0,
        .z = fude_zoom_frames(_s)[_frame].z_next, .t = { 0.0, 0.0 }, .box = { 0.0, 0.0, 0.0, 0.0 },
        .kind = FUDE_ZOOM_KIND_LAYER, .flags = FUDE_ZOOM_FLAG_ALIVE, .rotation = 0.0, .scale = 1.0,
    };
    _o.gesture = _o.id;
    _o.blob    = fude_zoom_blob_new(_s, _payload, (u32)(3u + _keep));
    if(!fude_zoom_frames(_s)[_frame].saved) {
        fude_zoom_journal_frame(_s, _frame);
    }
    const u32 _at = fude_zoom_scene_put_in(_s, &_o);
    fude_zoom_scene_touch(_s, &fude_zoom_objects(_s)[_at]);
    if(!_s->replaying) {
        u32 _op;
        fude_zoom_journal_op_begin(_s, FUDE_ZOOM_OP_OBJECT, &_op);
        fude_zoom_put_object(_s, &_s->journal, _at);
        fude_zoom_journal_op_end(_s, _op);
    }
    _s->revision++;
    return _at;
}

b8 fude_zoom_scene_layer_of(const fude_zoom_scene* _s, u32 _object, u16* _index, u8* _flags, c8* _name, usize _size) {
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(_o->kind != FUDE_ZOOM_KIND_LAYER || _o->blob == 0) {
        return false;
    }
    const fude_zoom_blob* _b = &fude_zoom_blobs(_s)[_o->blob - 1u];
    if(_b->size < 3u) {
        return false;
    }
    if(_index != NULL) { memcpy(_index, _b->data, sizeof(u16)); }
    if(_flags != NULL) { *_flags = _b->data[2]; }
    if(_name != NULL && _size > 0) {
        const usize _n = _b->size - 3u < _size - 1u ? _b->size - 3u : _size - 1u;
        memcpy(_name, _b->data + 3u, _n);
        _name[_n] = 0;
    }
    return true;
}

void fude_zoom_scene_layers_refresh(fude_zoom_scene* _s) {
    u32 _hidden = 0, _locked = 0;
    u64 _seen_z[FUDE_ZOOM_LAYERS];
    memset(_seen_z, 0, sizeof(_seen_z));
    for(u32 _l = 0; _l < FUDE_ZOOM_LAYERS; _l++) {
        _s->layer_rank[_l] = (u8)_l;   // never moved: in their indexes' order
    }
    for(u32 _i = 0; _i < fude_zoom_len(&_s->objects); _i++) {
        const fude_zoom_object* _o = &fude_zoom_objects(_s)[_i];
        u16 _index;
        u8  _flags;
        if(_o->kind != FUDE_ZOOM_KIND_LAYER || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || !fude_zoom_scene_layer_of(_s, _i, &_index, &_flags, NULL, 0)) {
            continue;
        }
        if(_index >= FUDE_ZOOM_LAYERS || _o->z < _seen_z[_index]) {
            continue;   // (the latest of an index's wins)
        }
        _seen_z[_index] = _o->z;
        const i32 _rank = fude_zoom_layer_rank_of(_flags);
        _s->layer_rank[_index] = (u8)(_rank >= 0 ? _rank : (i32)_index);
        _hidden = (_flags & FUDE_ZOOM_LAYER_HIDDEN) ? (_hidden | (1u << _index)) : (_hidden & ~(1u << _index));
        _locked = (_flags & FUDE_ZOOM_LAYER_LOCKED) ? (_locked | (1u << _index)) : (_locked & ~(1u << _index));
    }
    _s->layers_hidden = _hidden;
    _s->layers_locked = _locked;
}

b8 fude_zoom_scene_mark_of(const fude_zoom_scene* _s, u32 _object, f64* _z, u32* _number) {
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(_o->kind != FUDE_ZOOM_KIND_MARK || _o->blob == 0) {
        return false;
    }
    const fude_zoom_blob* _b = &fude_zoom_blobs(_s)[_o->blob - 1u];
    if(_b->size < sizeof(f64) + sizeof(u32)) {
        return false;
    }
    if(_z != NULL) {
        memcpy(_z, _b->data, sizeof(f64));
    }
    if(_number != NULL) {
        memcpy(_number, _b->data + sizeof(f64), sizeof(u32));
    }
    return true;
}

// An object of a kind whose payload is bytes of its own (a picture, a text),
// in the frame, boxed, indexed and journalled.
RDE_INTERNAL u32 fude_zoom_scene_add_payload(fude_zoom_scene* _s, u8 _kind, u32 _frame, fude_zoom_place _place, const u8* _payload, u32 _size,
                                             rde_color _color, rde_color _fill, u64 _z) {
    fude_zoom_object _o = {
        .id = fude_zoom_scene_new_id(_s), .frame = _frame, .child = FUDE_ZOOM_NONE, .count = _size,
        .z = _z != 0 ? _z : fude_zoom_frames(_s)[_frame].z_next, .t = _place.t, .color = _color, .fill = _fill,
        .kind = _kind, .flags = FUDE_ZOOM_FLAG_ALIVE, .rotation = _place.rotation, .scale = _place.scale, .layer = _s->layer,
    };
    _o.gesture = _o.id;
    _o.blob    = fude_zoom_blob_new(_s, _payload, _o.count);
    if(!fude_zoom_frames(_s)[_frame].saved) {
        fude_zoom_journal_frame(_s, _frame);
    }
    const u32 _index = fude_zoom_scene_put_in(_s, &_o);
    fude_zoom_scene_rebox(_s, _index);
    fude_zoom_frame* _f = &fude_zoom_frames(_s)[_frame];
    fude_zoom_index_insert(&_f->index, _index, fude_zoom_objects(_s)[_index].box);
    _f->stale++;
    fude_zoom_scene_touch(_s, &fude_zoom_objects(_s)[_index]);
    fude_zoom_scene_frame_reach(_s, _frame, fude_zoom_objects(_s)[_index].box);
    if(!_s->replaying) {
        u32 _at;
        fude_zoom_journal_op_begin(_s, FUDE_ZOOM_OP_OBJECT, &_at);
        fude_zoom_put_object(_s, &_s->journal, _index);
        fude_zoom_journal_op_end(_s, _at);
    }
    _s->revision++;
    return _index;
}

u32 fude_zoom_scene_add_image(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, f64 _hw, f64 _hh, const u8* _bytes, u32 _size, u64 _z) {
    // Its payload: its half-size first, then the picture as it came.
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    u8* _payload = _heap->malloc(_heap->allocator, (usize)_size + 2u * sizeof(f64));
    memcpy(_payload, &_hw, sizeof(f64));
    memcpy(_payload + sizeof(f64), &_hh, sizeof(f64));
    memcpy(_payload + 2u * sizeof(f64), _bytes, _size);
    const u32 _index = fude_zoom_scene_add_payload(_s, FUDE_ZOOM_KIND_IMAGE, _frame, _place, _payload, _size + 2u * (u32)sizeof(f64),
                                                   (rde_color){ 255, 255, 255, 255 }, (rde_color){ 0, 0, 0, 0 }, _z);
    _heap->free(_heap->allocator, _payload);
    return _index;
}

#define FUDE_ZOOM_TEXT_HEAD (3u * (u32)sizeof(f64) + (u32)sizeof(u32))

u32 fude_zoom_scene_add_text(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, f64 _size, f64 _w, f64 _h, u8 _style,
                             rde_color _color, rde_color _fill, const c8* _text, u32 _len, u64 _z) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    u8* _payload = _heap->malloc(_heap->allocator, (usize)_len + FUDE_ZOOM_TEXT_HEAD);
    const u32 _st = _style;
    memcpy(_payload, &_size, sizeof(f64));
    memcpy(_payload + sizeof(f64), &_w, sizeof(f64));
    memcpy(_payload + 2u * sizeof(f64), &_h, sizeof(f64));
    memcpy(_payload + 3u * sizeof(f64), &_st, sizeof(u32));
    if(_len > 0) {
        memcpy(_payload + FUDE_ZOOM_TEXT_HEAD, _text, _len);
    }
    const u32 _index = fude_zoom_scene_add_payload(_s, FUDE_ZOOM_KIND_TEXT, _frame, _place, _payload, _len + FUDE_ZOOM_TEXT_HEAD, _color, _fill, _z);
    _heap->free(_heap->allocator, _payload);
    return _index;
}

b8 fude_zoom_scene_text(const fude_zoom_scene* _s, u32 _object, f64* _size, f64* _w, f64* _h, u8* _style, const c8** _text, u32* _len) {
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(_o->kind != FUDE_ZOOM_KIND_TEXT || _o->blob == 0) {
        return false;
    }
    const fude_zoom_blob* _b = &fude_zoom_blobs(_s)[_o->blob - 1u];
    if(_b->size < FUDE_ZOOM_TEXT_HEAD) {
        return false;
    }
    u32 _st = 0;
    if(_size != NULL)  { memcpy(_size, _b->data, sizeof(f64)); }
    if(_w != NULL)     { memcpy(_w, _b->data + sizeof(f64), sizeof(f64)); }
    if(_h != NULL)     { memcpy(_h, _b->data + 2u * sizeof(f64), sizeof(f64)); }
    memcpy(&_st, _b->data + 3u * sizeof(f64), sizeof(u32));
    if(_style != NULL) { *_style = (u8)_st; }
    if(_text != NULL)  { *_text = (const c8*)(_b->data + FUDE_ZOOM_TEXT_HEAD); }
    if(_len != NULL)   { *_len = _b->size - FUDE_ZOOM_TEXT_HEAD; }
    return true;
}

void fude_zoom_scene_text_corners(const fude_zoom_scene* _s, u32 _object, fude_zoom_v2 _out[4]) {
    f64 _w = 0.0, _h = 0.0;
    fude_zoom_scene_text(_s, _object, NULL, &_w, &_h, NULL, NULL, NULL);
    const fude_zoom_sim _sim = fude_zoom_object_sim(&fude_zoom_objects(_s)[_object]);
    const fude_zoom_v2  _c[4] = { { 0.0, 0.0 }, { 0.0, -_h }, { _w, -_h }, { _w, 0.0 } };
    for(u32 _i = 0; _i < 4u; _i++) {
        _out[_i] = fude_zoom_sim_apply(_sim, _c[_i]);
    }
}

b8 fude_zoom_scene_image(const fude_zoom_scene* _s, u32 _object, f64* _hw, f64* _hh, const u8** _bytes, u32* _size) {
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(_o->kind != FUDE_ZOOM_KIND_IMAGE || _o->blob == 0) {
        return false;
    }
    const fude_zoom_blob* _b = &fude_zoom_blobs(_s)[_o->blob - 1u];
    if(_b->size < 2u * sizeof(f64)) {
        return false;
    }
    if(_hw != NULL) {
        memcpy(_hw, _b->data, sizeof(f64));
    }
    if(_hh != NULL) {
        memcpy(_hh, _b->data + sizeof(f64), sizeof(f64));
    }
    if(_bytes != NULL) {
        *_bytes = _b->data + 2u * sizeof(f64);
    }
    if(_size != NULL) {
        *_size = _b->size - 2u * (u32)sizeof(f64);
    }
    return true;
}

void fude_zoom_scene_image_corners(const fude_zoom_scene* _s, u32 _object, fude_zoom_v2 _out[4]) {
    f64 _hw = 0.0, _hh = 0.0;
    fude_zoom_scene_image(_s, _object, &_hw, &_hh, NULL, NULL);
    const fude_zoom_sim _sim = fude_zoom_object_sim(&fude_zoom_objects(_s)[_object]);
    const fude_zoom_v2  _c[4] = { { -_hw, -_hh }, { _hw, -_hh }, { _hw, _hh }, { -_hw, _hh } };
    for(u32 _i = 0; _i < 4u; _i++) {
        _out[_i] = fude_zoom_sim_apply(_sim, _c[_i]);
    }
}

u32 fude_zoom_scene_shape_numbers(const fude_zoom_scene* _s, u32 _object, f64* _out, u32 _max) {
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(_o->kind != FUDE_ZOOM_KIND_SHAPE || _o->blob == 0) {
        return 0;
    }
    const fude_zoom_blob* _b = &fude_zoom_blobs(_s)[_o->blob - 1u];
    u32 _n = _b->size / (u32)sizeof(f64);
    _n = _n < _max ? _n : _max;
    memcpy(_out, _b->data, (usize)_n * sizeof(f64));
    return _n;
}

u32 fude_zoom_scene_shape_numbers_all(const fude_zoom_scene* _s, u32 _object, rde_arr* _out) {
    rde_arr_clear(_out);
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(_o->kind != FUDE_ZOOM_KIND_SHAPE || _o->blob == 0) {
        return 0;
    }
    const fude_zoom_blob* _b = &fude_zoom_blobs(_s)[_o->blob - 1u];
    const u32 _n = _b->size / (u32)sizeof(f64);
    if(_n > 0) {
        memcpy(rde_arr_add_n(_out, _n), _b->data, (usize)_n * sizeof(f64));
    }
    return _n;
}

void fude_zoom_scene_shape_outline(const fude_zoom_scene* _s, u32 _object, u32 _segments, rde_arr* _out, b8* _closed) {
    // All its numbers (a cut board's outline is past the usual few).
    rde_arr _all = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std());
    const u32 _count = fude_zoom_scene_shape_numbers_all(_s, _object, &_all);
    const f64* _n = (const f64*)_all.memory;
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    fude_zoom_shape_outline(_o->channels, _n, _count, _segments, _out, _closed);
    rde_arr_free(&_all);
    const fude_zoom_sim _sim = fude_zoom_object_sim(_o);
    fude_zoom_v2* _p = (fude_zoom_v2*)_out->memory;
    for(u32 _i = 0; _i < fude_zoom_len(_out); _i++) {
        _p[_i] = fude_zoom_sim_apply(_sim, _p[_i]);
    }
}

void fude_zoom_scene_set_alive(fude_zoom_scene* _s, u32 _object, b8 _alive) {
    fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(((_o->flags & FUDE_ZOOM_FLAG_ALIVE) != 0) == _alive || (_o->flags & FUDE_ZOOM_FLAG_GARBAGE)) {
        return;
    }
    if(_o->kind == FUDE_ZOOM_KIND_FRAME) {
        // A frame deleted (or brought back): its record says so.
        _o->flags = (u8)(_alive ? (_o->flags | FUDE_ZOOM_FLAG_ALIVE) : (_o->flags & ~FUDE_ZOOM_FLAG_ALIVE));
        fude_zoom_frames(_s)[_o->child].removed = !_alive;
        if(fude_zoom_frames(_s)[_o->child].saved) {
            fude_zoom_journal_frame(_s, _o->child);
        }
        _s->revision++;
        return;
    }
    _o->flags = (u8)(_alive ? (_o->flags | FUDE_ZOOM_FLAG_ALIVE) : (_o->flags & ~FUDE_ZOOM_FLAG_ALIVE));
    fude_zoom_scene_touch(_s, _o);
    if(!_s->replaying) {
        u32 _at;
        fude_zoom_journal_op_begin(_s, FUDE_ZOOM_OP_ALIVE, &_at);
        fude_put_u64(&_s->journal, _o->id);
        fude_put_u8(&_s->journal, _alive ? 1u : 0u);
        fude_zoom_journal_op_end(_s, _at);
    }
    _s->revision++;
}

void fude_zoom_scene_discard(fude_zoom_scene* _s, u32 _object) {
    fude_zoom_scene_set_alive(_s, _object, false);
    fude_zoom_scene_collect(_s, _object);
}

b8 fude_zoom_scene_points(const fude_zoom_scene* _s, u32 _object, fude_zoom_qpoint* _out) {
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(_o->blob == 0) {
        return false;
    }
    const fude_zoom_blob* _b = &fude_zoom_blobs(_s)[_o->blob - 1u];
    return fude_zoom_codec_decode(_b->data, _b->size, _out, _o->count, _o->channels);
}

void fude_zoom_scene_query(const fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, rde_arr* _out) {
    const u32 _from = fude_zoom_len(_out);
    fude_zoom_index_query(&fude_zoom_frames(_s)[_frame].index, _box, _out);
    // Keep the alive ones, in place — once each (a moved object is in the index
    // at its old place too: its own box decides, and only one entry stays).
    u32* _v = fude_zoom_u32s(_out);
    u32  _w = _from;
    for(u32 _i = _from; _i < fude_zoom_len(_out); _i++) {
        const fude_zoom_object* _o = &fude_zoom_objects(_s)[_v[_i]];
        if(_o->flags & FUDE_ZOOM_FLAG_ALIVE && fude_zoom_box_overlaps(_o->box, _box)) {
            _v[_w++] = _v[_i];
        }
    }
    _out->count = _w;
    if(fude_zoom_frames(_s)[_frame].stale > 0 && _w - _from > 1u) {
        qsort(&_v[_from], _w - _from, sizeof(u32), fude_zoom_by_u32);
        u32 _k = _from + 1u;
        for(u32 _i = _from + 1u; _i < _w; _i++) {
            if(_v[_i] != _v[_k - 1u]) {
                _v[_k++] = _v[_i];
            }
        }
        _out->count = _k;
    }
}

// --- places: moving, scaling, turning ----------------------------------------------------

fude_zoom_place fude_zoom_scene_place_of(const fude_zoom_scene* _s, u32 _object) {
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(_o->kind == FUDE_ZOOM_KIND_FRAME) {
        const fude_zoom_xform* _x = &fude_zoom_frames(_s)[_o->child].xf;
        return (fude_zoom_place){ { _x->ox, _x->oy }, _x->rotation, _x->scale };
    }
    return (fude_zoom_place){ _o->t, _o->rotation, _o->scale };
}

fude_zoom_place fude_zoom_place_moved(fude_zoom_place _p, fude_zoom_sim _m) {
    const fude_zoom_sim _was = { _p.scale * cos(_p.rotation), _p.scale * sin(_p.rotation), _p.t.x, _p.t.y };
    const fude_zoom_sim _now = fude_zoom_sim_compose(_m, _was);
    return (fude_zoom_place){ { _now.tx, _now.ty }, atan2(_now.b, _now.a), hypot(_now.a, _now.b) };
}

// Its index again from what is in it, when moved and dead entries pile up.
RDE_INTERNAL void fude_zoom_scene_reindex(fude_zoom_scene* _s, u32 _frame) {
    fude_zoom_frame* _f = &fude_zoom_frames(_s)[_frame];
    const u32 _n = fude_zoom_len(&_f->arrival);
    if(_f->dead + _f->stale <= 64u || (_f->dead + _f->stale) * 2u <= _n) {
        return;
    }
    rde_arr _values = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr _boxes  = rde_arr_new(sizeof(fude_zoom_box), rde_memory_allocator_get_default_std());
    for(u32 _i = 0; _i < _n; _i++) {
        const u32               _index = fude_zoom_u32s(&_f->arrival)[_i];
        const fude_zoom_object* _o     = &fude_zoom_objects(_s)[_index];
        if(!(_o->flags & FUDE_ZOOM_FLAG_GARBAGE)) {
            rde_arr_add(&_values, (any)&_index);
            rde_arr_add(&_boxes, (any)&_o->box);
        }
    }
    _f = &fude_zoom_frames(_s)[_frame];
    fude_zoom_index_build(&_f->index, fude_zoom_u32s(&_values), (const fude_zoom_box*)_boxes.memory, fude_zoom_len(&_values));
    _f->dead  = 0;
    _f->stale = 0;
    rde_arr_free(&_values);
    rde_arr_free(&_boxes);
}

// A shape's box: its outline where it is, and its half-width round it.
RDE_INTERNAL fude_zoom_box fude_zoom_shape_box(const fude_zoom_scene* _s, u32 _object) {
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    b8      _closed;
    fude_zoom_scene_shape_outline(_s, _object, 96u, &_pts, &_closed);
    fude_zoom_box _b = fude_zoom_box_empty();
    for(u32 _i = 0; _i < fude_zoom_len(&_pts); _i++) {
        const fude_zoom_v2 _p = ((const fude_zoom_v2*)_pts.memory)[_i];
        _b = fude_zoom_box_union(_b, (fude_zoom_box){ _p.x, _p.y, _p.x, _p.y });
    }
    rde_arr_free(&_pts);
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    return fude_zoom_box_grow(_b, (f64)_o->radius * _o->scale);
}

// A stroke's box from its points where it is now (a shape's from its outline).
RDE_INTERNAL void fude_zoom_scene_rebox(fude_zoom_scene* _s, u32 _object) {
    fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(_o->blob == 0 || _o->count == 0) {
        return;
    }
    if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
        const fude_zoom_box _b = fude_zoom_shape_box(_s, _object);
        fude_zoom_objects(_s)[_object].box = _b;
        return;
    }
    if(_o->kind == FUDE_ZOOM_KIND_IMAGE || _o->kind == FUDE_ZOOM_KIND_TEXT) {
        fude_zoom_v2 _c[4];
        if(_o->kind == FUDE_ZOOM_KIND_IMAGE) {
            fude_zoom_scene_image_corners(_s, _object, _c);
        } else {
            fude_zoom_scene_text_corners(_s, _object, _c);
        }
        fude_zoom_box _b = fude_zoom_box_empty();
        for(u32 _i = 0; _i < 4u; _i++) {
            _b = fude_zoom_box_union(_b, (fude_zoom_box){ _c[_i].x, _c[_i].y, _c[_i].x, _c[_i].y });
        }
        fude_zoom_objects(_s)[_object].box = _b;
        return;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_qpoint*     _q    = _heap->malloc(_heap->allocator, (usize)_o->count * sizeof(fude_zoom_qpoint));
    if(fude_zoom_scene_points(_s, _object, _q)) {
        _o = &fude_zoom_objects(_s)[_object];
        _o->box = fude_zoom_stroke_box(_o, _q, _o->count);
    }
    _heap->free(_heap->allocator, _q);
}

void fude_zoom_scene_set_place(fude_zoom_scene* _s, u32 _object, fude_zoom_place _p) {
    fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(_o->kind == FUDE_ZOOM_KIND_FRAME) {
        // The frame moves in its parent; its anchor goes with it.
        fude_zoom_frame*    _f   = &fude_zoom_frames(_s)[_o->child];
        const fude_zoom_sim _old = fude_zoom_sim_from_xform(_f->xf);
        _f->xf = (fude_zoom_xform){ _p.t.x, _p.t.y, _p.scale, _p.rotation };
        const fude_zoom_sim _new = fude_zoom_sim_from_xform(_f->xf);
        _f->anchor = fude_zoom_sim_box(fude_zoom_sim_compose(_new, fude_zoom_sim_inverse(_old)), _f->anchor);
        _o->box    = _f->anchor;
        if(_f->saved) {
            fude_zoom_journal_frame(_s, _o->child);
        }
        fude_zoom_scene_frame_reach(_s, _o->frame, _o->box);
        _s->revision++;
        return;
    }
    _o->t        = _p.t;
    _o->rotation = _p.rotation;
    _o->scale    = _p.scale;
    fude_zoom_scene_rebox(_s, _object);
    _o = &fude_zoom_objects(_s)[_object];
    fude_zoom_frame* _f = &fude_zoom_frames(_s)[_o->frame];
    fude_zoom_index_insert(&_f->index, _object, _o->box);
    _f->stale++;
    fude_zoom_scene_touch(_s, _o);
    fude_zoom_scene_frame_reach(_s, _o->frame, _o->box);
    if(!_s->replaying) {
        u32 _at;
        fude_zoom_journal_op_begin(_s, FUDE_ZOOM_OP_PLACE, &_at);
        fude_put_u64(&_s->journal, _o->id);
        fude_put_f64(&_s->journal, _o->t.x);
        fude_put_f64(&_s->journal, _o->t.y);
        fude_put_f64(&_s->journal, _o->rotation);
        fude_put_f64(&_s->journal, _o->scale);
        fude_zoom_journal_op_end(_s, _at);
    }
    fude_zoom_scene_reindex(_s, _o->frame);
    _s->revision++;
}

// A copy of _object (its points shared) in _frame at _place: on top (_z 0) or
// at _z, on _layer, its own gesture (_gesture 0) or a later piece of _gesture.
RDE_INTERNAL u32 fude_zoom_scene_copy_as(fude_zoom_scene* _s, u32 _object, u32 _frame, fude_zoom_place _place, u64 _z, u16 _layer, fude_zoom_id _gesture) {
    fude_zoom_object _o = fude_zoom_objects(_s)[_object];
    if(_o.kind == FUDE_ZOOM_KIND_FRAME || _o.blob == 0) {
        return FUDE_ZOOM_NONE;
    }
    fude_zoom_blobs(_s)[_o.blob - 1u].refs++;
    _o.id       = fude_zoom_scene_new_id(_s);
    _o.gesture  = _gesture != 0 ? _gesture : _o.id;
    _o.frame    = _frame;
    _o.history  = 0;
    _o.z        = _z != 0 ? _z : fude_zoom_frames(_s)[_frame].z_next;
    _o.layer    = _layer;
    _o.flags    = (u8)((_o.flags | FUDE_ZOOM_FLAG_ALIVE) & ~(FUDE_ZOOM_FLAG_GARBAGE | (_gesture != 0 ? 0u : FUDE_ZOOM_FLAG_CONTINUES)));
    _o.t        = _place.t;
    _o.rotation = _place.rotation;
    _o.scale    = _place.scale;
    if(!fude_zoom_frames(_s)[_frame].saved) {
        fude_zoom_journal_frame(_s, _frame);
    }
    const u32 _index = fude_zoom_scene_put_in(_s, &_o);
    fude_zoom_scene_rebox(_s, _index);
    // Placed with the old box: in again with the right one.
    fude_zoom_frame* _f = &fude_zoom_frames(_s)[_frame];
    fude_zoom_index_insert(&_f->index, _index, fude_zoom_objects(_s)[_index].box);
    _f->stale++;
    fude_zoom_scene_touch(_s, &fude_zoom_objects(_s)[_index]);
    fude_zoom_scene_frame_reach(_s, _frame, fude_zoom_objects(_s)[_index].box);
    if(!_s->replaying) {
        u32 _at;
        fude_zoom_journal_op_begin(_s, FUDE_ZOOM_OP_OBJECT, &_at);
        fude_zoom_put_object(_s, &_s->journal, _index);
        fude_zoom_journal_op_end(_s, _at);
    }
    _s->revision++;
    return _index;
}

u32 fude_zoom_scene_copy_stroke(fude_zoom_scene* _s, u32 _object, u32 _frame, fude_zoom_place _place) {
    return fude_zoom_scene_copy_as(_s, _object, _frame, _place, 0, fude_zoom_objects(_s)[_object].layer, 0);
}

u32 fude_zoom_scene_copy_to_layer(fude_zoom_scene* _s, u32 _object, u16 _layer, fude_zoom_id _gesture) {
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    return fude_zoom_scene_copy_as(_s, _object, _o->frame, fude_zoom_scene_place_of(_s, _object), _o->z, _layer, _gesture);
}

// --- the camera --------------------------------------------------------------------------

fude_zoom_v2 fude_zoom_camera_to_frame(const fude_zoom_camera* _c, fude_zoom_v2 _screen) {
    return (fude_zoom_v2){ _c->at.x + _screen.x / _c->z, _c->at.y + _screen.y / _c->z };
}

fude_zoom_v2 fude_zoom_camera_to_screen(const fude_zoom_camera* _c, fude_zoom_v2 _p) {
    return (fude_zoom_v2){ (_p.x - _c->at.x) * _c->z, (_p.y - _c->at.y) * _c->z };
}

void fude_zoom_camera_zoom_at(fude_zoom_scene* _s, fude_zoom_v2 _screen, f64 _factor) {
    // The frame point under _screen stays under it.
    fude_zoom_camera* _c     = &_s->camera;
    const fude_zoom_v2 _held = fude_zoom_camera_to_frame(_c, _screen);
    _c->z  *= _factor;
    _c->at  = (fude_zoom_v2){ _held.x - _screen.x / _c->z, _held.y - _screen.y / _c->z };
}

void fude_zoom_camera_pan(fude_zoom_scene* _s, fude_zoom_v2 _delta) {
    _s->camera.at.x -= _delta.x / _s->camera.z;
    _s->camera.at.y -= _delta.y / _s->camera.z;
}

// The camera moved into frame _to, its point and zoom carried over.
RDE_INTERNAL void fude_zoom_camera_move(fude_zoom_scene* _s, u32 _to) {
    fude_zoom_camera*   _c   = &_s->camera;
    const fude_zoom_sim _sim = fude_zoom_scene_sim(_s, _c->frame, _to);
    _c->at    = fude_zoom_sim_apply(_sim, _c->at);
    _c->z    /= fude_zoom_sim_scale(_sim);
    _c->frame = _to;
    fude_zoom_frames(_s)[_to].visited = ++_s->clock;
}

// A frame made for the camera and left with nothing in it: gone.
RDE_INTERNAL void fude_zoom_scene_drop_if_unused(fude_zoom_scene* _s, u32 _frame) {
    fude_zoom_frame* _f = &fude_zoom_frames(_s)[_frame];
    if(_f->parent == FUDE_ZOOM_NONE || _f->removed || _f->saved || fude_zoom_scene_frame_used(_s, _frame)) {
        return;
    }
    _f->removed = true;
    if(_f->object != FUDE_ZOOM_NONE) {
        fude_zoom_objects(_s)[_f->object].flags &= (u8)~FUDE_ZOOM_FLAG_ALIVE;
    }
}

// The child of _frame the camera goes into at point _at (frame units): the
// most recently visited whose anchor holds it.
RDE_INTERNAL u32 fude_zoom_camera_child_at(const fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _at) {
    const fude_zoom_frame* _f    = &fude_zoom_frames(_s)[_frame];
    u32                    _best = FUDE_ZOOM_NONE;
    for(u32 _i = 0; _i < fude_zoom_len(&_f->order); _i++) {
        const fude_zoom_object* _o = &fude_zoom_objects(_s)[fude_zoom_u32s(&_f->order)[_i]];
        if(_o->kind != FUDE_ZOOM_KIND_FRAME || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
            continue;
        }
        const fude_zoom_frame* _c = &fude_zoom_frames(_s)[_o->child];
        if(!fude_zoom_box_contains(_c->anchor, _at)) {
            continue;
        }
        if(_best == FUDE_ZOOM_NONE || _c->visited > fude_zoom_frames(_s)[_best].visited) {
            _best = _o->child;
        }
    }
    return _best;
}

void fude_zoom_camera_settle(fude_zoom_scene* _s, fude_zoom_v2 _half) {
    for(u32 _step = 0; _step < 256u; _step++) {
        fude_zoom_camera* _c = &_s->camera;
        const u32 _from = _c->frame;
        if(_c->z > FUDE_ZOOM_Z_MAX) {
            u32 _child = fude_zoom_camera_child_at(_s, _from, _c->at);
            if(_child == FUDE_ZOOM_NONE) {
                const fude_zoom_box _view = { _c->at.x - _half.x / _c->z, _c->at.y - _half.y / _c->z, _c->at.x + _half.x / _c->z, _c->at.y + _half.y / _c->z };
                const fude_zoom_v2  _o    = fude_zoom_paper_snap(_c->at, FUDE_ZOOM_CHILD_SCALE);
                _child = fude_zoom_scene_new_frame(_s, _from, (fude_zoom_xform){ _o.x, _o.y, FUDE_ZOOM_CHILD_SCALE, 0.0 }, _view);
            }
            fude_zoom_camera_move(_s, _child);
        } else if(_c->z < FUDE_ZOOM_Z_MIN) {
            if(fude_zoom_frames(_s)[_from].parent == FUDE_ZOOM_NONE) {
                if(!fude_zoom_scene_frame_used(_s, _from)) {
                    _c->z = FUDE_ZOOM_Z_MIN;   // an empty canvas: nothing out there to see
                    break;
                }
                fude_zoom_scene_new_root(_s);
            }
            fude_zoom_camera_move(_s, fude_zoom_frames(_s)[_from].parent);
            fude_zoom_scene_drop_if_unused(_s, _from);
        } else {
            break;
        }
    }

    // Panned out of its frame's anchor onto a sibling's: handed over, so new
    // strokes go where their neighbours are.
    const u32 _frame  = _s->camera.frame;
    const u32 _parent = fude_zoom_frames(_s)[_frame].parent;
    if(_parent == FUDE_ZOOM_NONE) {
        return;
    }
    const fude_zoom_v2 _in_parent = fude_zoom_sim_apply(fude_zoom_sim_from_xform(fude_zoom_frames(_s)[_frame].xf), _s->camera.at);
    if(fude_zoom_box_contains(fude_zoom_frames(_s)[_frame].anchor, _in_parent)) {
        return;
    }
    const u32 _other = fude_zoom_camera_child_at(_s, _parent, _in_parent);
    if(_other != FUDE_ZOOM_NONE && _other != _frame) {
        fude_zoom_camera_move(_s, _other);
        fude_zoom_scene_drop_if_unused(_s, _frame);
    }
}

fude_zoom_sim fude_zoom_camera_sim(const fude_zoom_camera* _c) {
    return (fude_zoom_sim){ _c->z, 0.0, -_c->at.x * _c->z, -_c->at.y * _c->z };
}

void fude_zoom_camera_look_at(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _at, f64 _z) {
    const u32 _left = _s->camera.frame;
    _s->camera = (fude_zoom_camera){ _frame, _at, _z };
    fude_zoom_frames(_s)[_frame].visited = ++_s->clock;
    if(_left != _frame) {
        fude_zoom_scene_drop_if_unused(_s, _left);
    }
}

// Is anything alive drawn after frame _child in its parent, over _box (parent units)?
RDE_INTERNAL b8 fude_zoom_scene_covered(fude_zoom_scene* _s, u32 _child, fude_zoom_box _box) {
    const fude_zoom_frame* _c = &fude_zoom_frames(_s)[_child];
    if(_c->parent == FUDE_ZOOM_NONE || _c->object == FUDE_ZOOM_NONE) {
        return false;
    }
    const u64        _slot = fude_zoom_objects(_s)[_c->object].z;
    const fude_zoom_frame* _p = &fude_zoom_frames(_s)[_c->parent];
    for(u32 _i = fude_zoom_order_slot(_s, _p, _slot); _i < fude_zoom_len(&_p->order); _i++) {
        const fude_zoom_object* _o = &fude_zoom_objects(_s)[fude_zoom_u32s(&_p->order)[_i]];
        if(_o->z > _slot && _o->flags & FUDE_ZOOM_FLAG_ALIVE && fude_zoom_box_overlaps(_o->box, _box)) {
            return true;
        }
    }
    return false;
}

u32 fude_zoom_camera_drawing_frame(fude_zoom_scene* _s, fude_zoom_box _box) {
    // The levels the renderer draws exactly (three up): the highest whose place
    // in its parent is covered by something later.
    u32           _chain[4];
    fude_zoom_box _boxes[4];
    u32           _levels  = 0;
    i32           _covered = -1;
    u32           _f       = _s->camera.frame;
    fude_zoom_box _b       = _box;
    while(_levels < 3u && fude_zoom_frames(_s)[_f].parent != FUDE_ZOOM_NONE) {
        const fude_zoom_box _in_parent = fude_zoom_sim_box(fude_zoom_sim_from_xform(fude_zoom_frames(_s)[_f].xf), _b);
        _chain[_levels] = _f;
        _boxes[_levels] = _in_parent;
        if(fude_zoom_scene_covered(_s, _f, _in_parent)) {
            _covered = (i32)_levels;
        }
        _levels++;
        _f = fude_zoom_frames(_s)[_f].parent;
        _b = _in_parent;
    }
    if(_covered < 0) {
        return _s->camera.frame;
    }
    // A new frame on top at that level, and one inside it for each level below,
    // down to the camera's depth: each at the camera (its origin the camera's
    // point, on the paper's lattice), at the same scale as the one it stands in
    // for. Each anchor is the stroke's box in the new parent's units: the old
    // parent's, moved by where the camera is in each.
    fude_zoom_v2 _cam[4];
    for(u32 _l = 0; _l < _levels; _l++) {
        _cam[_l] = fude_zoom_sim_apply(fude_zoom_scene_sim(_s, _s->camera.frame, _chain[_l]), _s->camera.at);
    }
    const f64    _z     = _s->camera.z;
    u32          _above = fude_zoom_frames(_s)[_chain[_covered]].parent;
    fude_zoom_v2 _here  = fude_zoom_sim_apply(fude_zoom_scene_sim(_s, _s->camera.frame, _above), _s->camera.at);
    for(i32 _l = _covered; _l >= 0; _l--) {
        const f64     _scale  = fude_zoom_frames(_s)[_chain[_l]].xf.scale;
        fude_zoom_box _anchor = _boxes[_l];
        if(_l != _covered) {
            const fude_zoom_v2 _d = { _here.x - _cam[_l + 1].x, _here.y - _cam[_l + 1].y };
            _anchor = (fude_zoom_box){ _anchor.min_x + _d.x, _anchor.min_y + _d.y, _anchor.max_x + _d.x, _anchor.max_y + _d.y };
        }
        const fude_zoom_v2 _o = fude_zoom_paper_snap(_here, _scale);
        _above = fude_zoom_scene_new_frame(_s, _above, (fude_zoom_xform){ _o.x, _o.y, _scale, 0.0 }, _anchor);
        _here  = (fude_zoom_v2){ (_here.x - _o.x) / _scale, (_here.y - _o.y) / _scale };
    }
    fude_zoom_camera_look_at(_s, _above, _here, _z);
    return _above;
}

// --- the history -------------------------------------------------------------------------

RDE_INTERNAL u32 fude_zoom_action_bytes(const fude_zoom_scene* _s, const u32* _ids, u32 _count) {
    u32 _bytes = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        const fude_zoom_object* _o = &fude_zoom_objects(_s)[_ids[_i]];
        if(_o->blob != 0) {
            _bytes += fude_zoom_blobs(_s)[_o->blob - 1u].size + (u32)sizeof(fude_zoom_object);
        }
    }
    return _bytes;
}

// The objects an action names lose that name; the ones nothing keeps any more
// become garbage.
RDE_INTERNAL void fude_zoom_action_forget(fude_zoom_scene* _s, const fude_zoom_action* _a) {
    const u32* _t = fude_zoom_targets(_s);
    for(u32 _i = 0; _i < _a->count; _i++) {
        fude_zoom_objects(_s)[_t[_a->first + _i]].history--;
        fude_zoom_scene_collect(_s, _t[_a->first + _i]);
    }
    for(u32 _i = 0; _i < _a->born; _i++) {
        fude_zoom_objects(_s)[_t[_a->born_first + _i]].history--;
        fude_zoom_scene_collect(_s, _t[_a->born_first + _i]);
    }
    for(u32 _i = 0; _i < _a->moved; _i++) {
        fude_zoom_objects(_s)[_t[_a->moved_first + _i]].history--;
        fude_zoom_scene_collect(_s, _t[_a->moved_first + _i]);
    }
}

// Drops the actions past action_count (the undone ones).
RDE_INTERNAL void fude_zoom_history_drop_redo(fude_zoom_scene* _s) {
    while(fude_zoom_len(&_s->actions) > _s->action_count) {
        const fude_zoom_action _a = fude_zoom_actions(_s)[fude_zoom_len(&_s->actions) - 1u];
        _s->actions.count--;
        _s->history_bytes -= _a.bytes;
        fude_zoom_action_forget(_s, &_a);
    }
    // Targets and places only grow at the end: the applied actions' ends are where they stop.
    u32 _end = 0, _places = 0;
    if(_s->action_count > 0) {
        const fude_zoom_action* _last = &fude_zoom_actions(_s)[_s->action_count - 1u];
        _end    = _last->moved_first + _last->moved;
        _places = _last->place_first + _last->moved * 2u;
    }
    if(_end < _s->targets.count) {
        _s->targets.count = _end;
    }
    if(_places < _s->places.count) {
        _s->places.count = _places;
    }
}

// Drops the oldest _n actions (the budget).
RDE_INTERNAL void fude_zoom_history_drop_oldest(fude_zoom_scene* _s, u32 _n) {
    if(_n == 0 || _n > _s->action_count) {
        return;
    }
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_action _a = fude_zoom_actions(_s)[_i];
        _s->history_bytes -= _a.bytes;
        fude_zoom_action_forget(_s, &_a);
    }
    // The targets of what is left, moved to the front.
    const fude_zoom_action* _next = &fude_zoom_actions(_s)[_n < fude_zoom_len(&_s->actions) ? _n : 0];
    const u32 _cut  = _n < fude_zoom_len(&_s->actions) ? _next->first : _s->targets.count;
    const u32 _pcut = _n < fude_zoom_len(&_s->actions) ? _next->place_first : _s->places.count;
    u32* _t = fude_zoom_targets(_s);
    memmove(_t, _t + _cut, (usize)(_s->targets.count - _cut) * sizeof(u32));
    _s->targets.count -= _cut;
    fude_zoom_place* _pl = fude_zoom_places(_s);
    memmove(_pl, _pl + _pcut, (usize)(_s->places.count - _pcut) * sizeof(fude_zoom_place));
    _s->places.count -= _pcut;
    fude_zoom_action* _acts = fude_zoom_actions(_s);
    memmove(_acts, _acts + _n, (usize)(fude_zoom_len(&_s->actions) - _n) * sizeof(fude_zoom_action));
    _s->actions.count -= _n;
    _s->action_count  -= _n;
    _s->history_dropped += _n;
    for(u32 _i = 0; _i < fude_zoom_len(&_s->actions); _i++) {
        _acts[_i].first       -= _cut;
        _acts[_i].born_first  -= _cut;
        _acts[_i].moved_first -= _cut;
        _acts[_i].place_first -= _pcut;
    }
    if(!_s->replaying) {
        u32 _at;
        fude_zoom_journal_op_begin(_s, FUDE_ZOOM_OP_DROP, &_at);
        fude_put_u32(&_s->journal, _n);
        fude_zoom_journal_op_end(_s, _at);
    }
}

// The action appended, as it is (its flags are already what it did).
RDE_INTERNAL void fude_zoom_history_append(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, const u32* _died, u32 _died_count, const u32* _born, u32 _born_count,
                                           const u32* _moved, const fude_zoom_place* _before, const fude_zoom_place* _after, u32 _moved_count) {
    fude_zoom_history_drop_redo(_s);
    fude_zoom_action _a = { .frame = _frame, .box = _box };
    _a.type  = _died_count == 0 ? FUDE_ZOOM_ACTION_BORN : (_born_count == 0 ? FUDE_ZOOM_ACTION_DIED : FUDE_ZOOM_ACTION_REPLACED);
    _a.first = fude_zoom_len(&_s->targets);
    _a.count = _died_count;
    if(_died_count > 0) {
        memcpy(rde_arr_add_n(&_s->targets, _died_count), _died, (usize)_died_count * sizeof(u32));
    }
    _a.born_first = fude_zoom_len(&_s->targets);
    _a.born       = _born_count;
    if(_born_count > 0) {
        memcpy(rde_arr_add_n(&_s->targets, _born_count), _born, (usize)_born_count * sizeof(u32));
    }
    _a.moved_first = fude_zoom_len(&_s->targets);
    _a.moved       = _moved_count;
    _a.place_first = fude_zoom_len(&_s->places);
    if(_moved_count > 0) {
        memcpy(rde_arr_add_n(&_s->targets, _moved_count), _moved, (usize)_moved_count * sizeof(u32));
        fude_zoom_place* _pl = rde_arr_add_n(&_s->places, (usize)_moved_count * 2u);
        for(u32 _i = 0; _i < _moved_count; _i++) {
            _pl[_i * 2u]      = _before[_i];
            _pl[_i * 2u + 1u] = _after[_i];
        }
        _a.type = FUDE_ZOOM_ACTION_MOVED;
    }
    for(u32 _i = 0; _i < _died_count; _i++) {
        fude_zoom_objects(_s)[_died[_i]].history++;
    }
    for(u32 _i = 0; _i < _born_count; _i++) {
        fude_zoom_objects(_s)[_born[_i]].history++;
    }
    for(u32 _i = 0; _i < _moved_count; _i++) {
        fude_zoom_objects(_s)[_moved[_i]].history++;
    }
    _a.bytes = fude_zoom_action_bytes(_s, _died, _died_count) + fude_zoom_action_bytes(_s, _born, _born_count) + _moved_count * (u32)(sizeof(u32) + 2u * sizeof(fude_zoom_place));
    rde_arr_add(&_s->actions, &_a);
    _s->action_count  = fude_zoom_len(&_s->actions);
    _s->history_pushes++;
    _s->history_bytes += _a.bytes;
}

RDE_INTERNAL void fude_zoom_history_push_all(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, const u32* _died, u32 _died_count, const u32* _born, u32 _born_count,
                                             const u32* _moved, const fude_zoom_place* _before, const fude_zoom_place* _after, u32 _moved_count) {
    if(_died_count == 0 && _born_count == 0 && _moved_count == 0) {
        return;
    }
    fude_zoom_history_append(_s, _frame, _box, _died, _died_count, _born, _born_count, _moved, _before, _after, _moved_count);
    if(!_s->replaying) {
        u32 _at;
        fude_zoom_journal_op_begin(_s, FUDE_ZOOM_OP_PUSH, &_at);
        fude_zoom_put_action(_s, &_s->journal, _s->action_count - 1u);
        fude_zoom_journal_op_end(_s, _at);
    }
    // Over the budget: the oldest steps go (never the one just done).
    u32 _drop = 0;
    u64 _bytes = _s->history_bytes;
    while(_bytes > _s->history_budget && _drop + 1u < _s->action_count) {
        _bytes -= fude_zoom_actions(_s)[_drop].bytes;
        _drop++;
    }
    fude_zoom_history_drop_oldest(_s, _drop);
    _s->revision++;
}

void fude_zoom_history_push(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, const u32* _died, u32 _died_count, const u32* _born, u32 _born_count) {
    fude_zoom_history_push_all(_s, _frame, _box, _died, _died_count, _born, _born_count, NULL, NULL, NULL, 0);
}

void fude_zoom_history_push_moved(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, const u32* _objects, const fude_zoom_place* _before, const fude_zoom_place* _after, u32 _count) {
    fude_zoom_history_push_all(_s, _frame, _box, NULL, 0, NULL, 0, _objects, _before, _after, _count);
}

RDE_INTERNAL void fude_zoom_history_journal_count(fude_zoom_scene* _s) {
    if(_s->replaying) {
        return;
    }
    u32 _at;
    fude_zoom_journal_op_begin(_s, FUDE_ZOOM_OP_COUNT, &_at);
    fude_put_u32(&_s->journal, _s->action_count);
    fude_zoom_journal_op_end(_s, _at);
}

b8 fude_zoom_history_undo(fude_zoom_scene* _s) {
    if(_s->action_count == 0) {
        return false;
    }
    const fude_zoom_action _a = fude_zoom_actions(_s)[_s->action_count - 1u];
    for(u32 _i = 0; _i < _a.born; _i++) {
        fude_zoom_scene_set_alive(_s, fude_zoom_targets(_s)[_a.born_first + _i], false);
    }
    for(u32 _i = 0; _i < _a.count; _i++) {
        fude_zoom_scene_set_alive(_s, fude_zoom_targets(_s)[_a.first + _i], true);
    }
    for(u32 _i = 0; _i < _a.moved; _i++) {
        fude_zoom_scene_set_place(_s, fude_zoom_targets(_s)[_a.moved_first + _i], fude_zoom_places(_s)[_a.place_first + _i * 2u]);
    }
    _s->action_count--;
    fude_zoom_history_journal_count(_s);
    _s->revision++;
    return true;
}

b8 fude_zoom_history_redo(fude_zoom_scene* _s) {
    if(_s->action_count >= fude_zoom_len(&_s->actions)) {
        return false;
    }
    const fude_zoom_action _a = fude_zoom_actions(_s)[_s->action_count];
    for(u32 _i = 0; _i < _a.count; _i++) {
        fude_zoom_scene_set_alive(_s, fude_zoom_targets(_s)[_a.first + _i], false);
    }
    for(u32 _i = 0; _i < _a.born; _i++) {
        fude_zoom_scene_set_alive(_s, fude_zoom_targets(_s)[_a.born_first + _i], true);
    }
    for(u32 _i = 0; _i < _a.moved; _i++) {
        fude_zoom_scene_set_place(_s, fude_zoom_targets(_s)[_a.moved_first + _i], fude_zoom_places(_s)[_a.place_first + _i * 2u + 1u]);
    }
    _s->action_count++;
    fude_zoom_history_journal_count(_s);
    _s->revision++;
    return true;
}

b8 fude_zoom_history_can_undo(const fude_zoom_scene* _s) {
    return _s->action_count > 0;
}

b8 fude_zoom_history_can_redo(const fude_zoom_scene* _s) {
    return _s->action_count < fude_zoom_len(&_s->actions);
}

const fude_zoom_action* fude_zoom_history_next_undo(const fude_zoom_scene* _s) {
    return _s->action_count > 0 ? &fude_zoom_actions(_s)[_s->action_count - 1u] : NULL;
}

const fude_zoom_action* fude_zoom_history_next_redo(const fude_zoom_scene* _s) {
    return _s->action_count < fude_zoom_len(&_s->actions) ? &fude_zoom_actions(_s)[_s->action_count] : NULL;
}

// --- records -----------------------------------------------------------------------------
//
// FRAME   u64 id, u64 parent id (0: the root), f64 ox oy scale rotation,
//         f64 anchor min_x min_y max_x max_y, u64 its order key in the parent,
//         u8 removed
// OBJECT  u64 id, u64 gesture, u64 frame id, u8 kind flags channels, i8 q,
//         u16 layer, u64 z, f64 tx ty, f64 box ×4, u8 r g b a, f32 radius,
//         u32 count, u32 points' size, the points (none for garbage),
//         f64 rotation, f64 scale (later: a record without them is 0 and 1)
// ACTION  u64 frame id, f64 box ×4, u32 died, their ids, u32 born, their ids,
//         u32 moved, per one its id and f64 tx ty rotation scale before, then after
// New fields go at the END of a record; readers take records by their size
// where they come in arrays (zfile.h).

void fude_zoom_put_frame(const fude_zoom_scene* _s, fude_bytes* _b, u32 _frame) {
    const fude_zoom_frame* _f = &fude_zoom_frames(_s)[_frame];
    fude_put_u64(_b, _f->id);
    fude_put_u64(_b, _f->parent != FUDE_ZOOM_NONE ? fude_zoom_frames(_s)[_f->parent].id : 0u);
    fude_put_f64(_b, _f->xf.ox);
    fude_put_f64(_b, _f->xf.oy);
    fude_put_f64(_b, _f->xf.scale);
    fude_put_f64(_b, _f->xf.rotation);
    fude_put_f64(_b, _f->anchor.min_x);
    fude_put_f64(_b, _f->anchor.min_y);
    fude_put_f64(_b, _f->anchor.max_x);
    fude_put_f64(_b, _f->anchor.max_y);
    fude_put_u64(_b, _f->object != FUDE_ZOOM_NONE ? fude_zoom_objects(_s)[_f->object].z : 0u);
    fude_put_u8(_b, _f->removed ? 1u : 0u);
    fude_put_u8(_b, _frame == _s->home ? 1u : 0u);   // (from before it: none says so — the root is)
}

void fude_zoom_put_object(const fude_zoom_scene* _s, fude_bytes* _b, u32 _object) {
    const fude_zoom_object* _o       = &fude_zoom_objects(_s)[_object];
    const b8                _garbage = (_o->flags & FUDE_ZOOM_FLAG_GARBAGE) != 0;
    fude_put_u64(_b, _o->id);
    fude_put_u64(_b, _o->gesture);
    fude_put_u64(_b, fude_zoom_frames(_s)[_o->frame].id);
    fude_put_u8(_b, _o->kind);
    fude_put_u8(_b, _o->flags);
    fude_put_u8(_b, _o->channels);
    fude_put_u8(_b, (u8)_o->q);
    fude_put_u16(_b, _o->layer);
    fude_put_u64(_b, _o->z);
    fude_put_f64(_b, _o->t.x);
    fude_put_f64(_b, _o->t.y);
    fude_put_f64(_b, _o->box.min_x);
    fude_put_f64(_b, _o->box.min_y);
    fude_put_f64(_b, _o->box.max_x);
    fude_put_f64(_b, _o->box.max_y);
    fude_put_color(_b, _o->color);
    fude_put_f32(_b, _o->radius);
    fude_put_u32(_b, _o->count);
    if(_garbage || _o->blob == 0) {
        fude_put_u32(_b, 0u);
    } else {
        const fude_zoom_blob* _bl = &fude_zoom_blobs(_s)[_o->blob - 1u];
        fude_put_u32(_b, _bl->size);
        fude_put_data(_b, _bl->data, _bl->size);
    }
    fude_put_f64(_b, _o->rotation);
    fude_put_f64(_b, _o->scale);
    fude_put_color(_b, _o->fill);   // (from before it: none)
}

RDE_INTERNAL void fude_zoom_put_place(fude_bytes* _b, fude_zoom_place _p) {
    fude_put_f64(_b, _p.t.x);
    fude_put_f64(_b, _p.t.y);
    fude_put_f64(_b, _p.rotation);
    fude_put_f64(_b, _p.scale);
}

RDE_INTERNAL fude_zoom_place fude_zoom_get_place(fude_reader* _r) {
    fude_zoom_place _p;
    _p.t.x      = fude_get_f64(_r);
    _p.t.y      = fude_get_f64(_r);
    _p.rotation = fude_get_f64(_r);
    _p.scale    = fude_get_f64(_r);
    return _p;
}

void fude_zoom_put_action(const fude_zoom_scene* _s, fude_bytes* _b, u32 _action) {
    const fude_zoom_action* _a = &fude_zoom_actions(_s)[_action];
    const u32*              _t = fude_zoom_targets(_s);
    fude_put_u64(_b, fude_zoom_frames(_s)[_a->frame].id);
    fude_put_f64(_b, _a->box.min_x);
    fude_put_f64(_b, _a->box.min_y);
    fude_put_f64(_b, _a->box.max_x);
    fude_put_f64(_b, _a->box.max_y);
    fude_put_u32(_b, _a->count);
    for(u32 _i = 0; _i < _a->count; _i++) {
        fude_put_u64(_b, fude_zoom_objects(_s)[_t[_a->first + _i]].id);
    }
    fude_put_u32(_b, _a->born);
    for(u32 _i = 0; _i < _a->born; _i++) {
        fude_put_u64(_b, fude_zoom_objects(_s)[_t[_a->born_first + _i]].id);
    }
    fude_put_u32(_b, _a->moved);
    for(u32 _i = 0; _i < _a->moved; _i++) {
        fude_put_u64(_b, fude_zoom_objects(_s)[_t[_a->moved_first + _i]].id);
        fude_zoom_put_place(_b, fude_zoom_places(_s)[_a->place_first + _i * 2u]);
        fude_zoom_put_place(_b, fude_zoom_places(_s)[_a->place_first + _i * 2u + 1u]);
    }
}

b8 fude_zoom_get_frame(fude_zoom_scene* _s, fude_reader* _r) {
    const fude_zoom_id _id     = fude_get_u64(_r);
    const fude_zoom_id _parent = fude_get_u64(_r);
    fude_zoom_xform    _xf;
    _xf.ox       = fude_get_f64(_r);
    _xf.oy       = fude_get_f64(_r);
    _xf.scale    = fude_get_f64(_r);
    _xf.rotation = fude_get_f64(_r);
    fude_zoom_box _anchor;
    _anchor.min_x = fude_get_f64(_r);
    _anchor.min_y = fude_get_f64(_r);
    _anchor.max_x = fude_get_f64(_r);
    _anchor.max_y = fude_get_f64(_r);
    const u64 _z       = fude_get_u64(_r);
    const b8  _removed = fude_get_u8(_r) != 0;
    const b8  _home    = fude_reader_has(_r, 1u) && fude_get_u8(_r) == 1u;
    if(!_r->ok || _id == 0 || !(_xf.scale > 0.0)) {
        return false;
    }
    fude_zoom_scene_saw_id(_s, _id);
    const u32 _parent_slot = _parent != 0 ? fude_zoom_scene_find_frame(_s, _parent) : FUDE_ZOOM_NONE;
    if(_parent != 0 && _parent_slot == FUDE_ZOOM_NONE) {
        return false;   // parents come first: one that does not is not a frame of this canvas
    }

    u32 _slot = fude_zoom_scene_find_frame(_s, _id);
    if(_slot == FUDE_ZOOM_NONE) {
        fude_zoom_frame* _f = rde_arr_add(&_s->frames, NULL);
        fude_zoom_frame_init(_f);
        _f->id = _id;
        _slot  = fude_zoom_len(&_s->frames) - 1u;
        fude_zoom_map_set(&_s->frame_ids, _id, _slot);
    }
    if(_home) {
        _s->home = _slot;
    }
    fude_zoom_frame* _f        = &fude_zoom_frames(_s)[_slot];
    const b8         _reparent = _f->parent != _parent_slot;
    _f->parent  = _parent_slot;
    _f->xf      = _xf;
    _f->anchor  = _anchor;
    _f->removed = _removed;
    _f->saved   = true;
    if(_reparent || (_f->object == FUDE_ZOOM_NONE && _parent_slot != FUDE_ZOOM_NONE)) {
        // Moved under another (a new root above it): its old FRAME object, if
        // any, is left dead in the old parent's list; a new one in the new parent.
        if(_f->object != FUDE_ZOOM_NONE) {
            fude_zoom_objects(_s)[_f->object].flags &= (u8)~FUDE_ZOOM_FLAG_ALIVE;
            _f->object = FUDE_ZOOM_NONE;
        }
        if(_parent_slot != FUDE_ZOOM_NONE) {
            fude_zoom_scene_frame_object(_s, _slot, _z != 0 ? _z : FUDE_ZOOM_Z_STEP);
        }
    } else if(_f->object != FUDE_ZOOM_NONE) {
        fude_zoom_object* _o = &fude_zoom_objects(_s)[_f->object];
        _o->box   = _anchor;
        _o->flags = _removed ? 0u : FUDE_ZOOM_FLAG_ALIVE;
    }
    if(_parent_slot == FUDE_ZOOM_NONE) {
        _s->root = _slot;
    }
    return true;
}

b8 fude_zoom_get_object(fude_zoom_scene* _s, fude_reader* _r) {
    fude_zoom_object _o = { .child = FUDE_ZOOM_NONE };
    _o.id                 = fude_get_u64(_r);
    _o.gesture            = fude_get_u64(_r);
    const fude_zoom_id _f = fude_get_u64(_r);
    _o.kind               = fude_get_u8(_r);
    _o.flags              = fude_get_u8(_r);
    _o.channels           = fude_get_u8(_r);
    _o.q                  = (i8)fude_get_u8(_r);
    _o.layer              = fude_get_u16(_r);
    _o.z                  = fude_get_u64(_r);
    _o.t.x                = fude_get_f64(_r);
    _o.t.y                = fude_get_f64(_r);
    _o.box.min_x          = fude_get_f64(_r);
    _o.box.min_y          = fude_get_f64(_r);
    _o.box.max_x          = fude_get_f64(_r);
    _o.box.max_y          = fude_get_f64(_r);
    _o.color              = fude_get_color(_r);
    _o.radius             = fude_get_f32(_r);
    _o.count              = fude_get_u32(_r);
    const u32 _size       = fude_get_u32(_r);
    if(!_r->ok || _o.id == 0 || (_o.kind != FUDE_ZOOM_KIND_STROKE && _o.kind != FUDE_ZOOM_KIND_SHAPE && _o.kind != FUDE_ZOOM_KIND_IMAGE && _o.kind != FUDE_ZOOM_KIND_FILL &&
                                 _o.kind != FUDE_ZOOM_KIND_MARK && _o.kind != FUDE_ZOOM_KIND_TEXT && _o.kind != FUDE_ZOOM_KIND_LAYER) ||
       !fude_reader_has(_r, _size)) {
        return false;
    }
    const u8* _points = _r->data + _r->pos;
    _r->pos += _size;
    _o.rotation = 0.0;
    _o.scale    = 1.0;
    if(fude_reader_has(_r, 16u)) {
        _o.rotation = fude_get_f64(_r);
        _o.scale    = fude_get_f64(_r);
    }
    if(fude_reader_has(_r, 4u)) {
        _o.fill = fude_get_color(_r);
    }
    fude_zoom_scene_saw_id(_s, _o.id);
    _o.frame = fude_zoom_scene_find_frame(_s, _f);
    if(_o.frame == FUDE_ZOOM_NONE) {
        return false;
    }
    if(fude_zoom_scene_find_object(_s, _o.id) != FUDE_ZOOM_NONE) {
        return true;   // already here (a later copy of what was loaded): the first stands
    }
    if(_size == 0) {
        _o.flags = (u8)((_o.flags | FUDE_ZOOM_FLAG_GARBAGE) & ~FUDE_ZOOM_FLAG_ALIVE);
    } else {
        _o.blob = fude_zoom_blob_new(_s, _points, _size);
    }
    const u32 _index = fude_zoom_scene_put_in(_s, &_o);
    if(fude_zoom_objects(_s)[_index].flags & FUDE_ZOOM_FLAG_GARBAGE) {
        fude_zoom_frames(_s)[_o.frame].dead++;
    }
    fude_zoom_scene_touch(_s, &fude_zoom_objects(_s)[_index]);
    return true;
}

b8 fude_zoom_get_action(fude_zoom_scene* _s, fude_reader* _r) {
    const fude_zoom_id _frame_id = fude_get_u64(_r);
    fude_zoom_box      _box;
    _box.min_x = fude_get_f64(_r);
    _box.min_y = fude_get_f64(_r);
    _box.max_x = fude_get_f64(_r);
    _box.max_y = fude_get_f64(_r);
    rde_arr _died = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr _born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    b8      _ok   = true;
    for(u32 _side = 0; _side < 2u && _ok; _side++) {
        const u32 _n = fude_get_u32(_r);
        if(!_r->ok || !fude_reader_has(_r, _n * 8u)) {
            _ok = false;
            break;
        }
        for(u32 _i = 0; _i < _n; _i++) {
            const u32 _index = fude_zoom_scene_find_object(_s, fude_get_u64(_r));
            if(_index == FUDE_ZOOM_NONE) {
                _ok = false;   // an action over something the file does not have: the history stops here
                break;
            }
            rde_arr_add(_side == 0 ? &_died : &_born, (any)&_index);
        }
    }
    rde_arr _moved  = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr _before = rde_arr_new(sizeof(fude_zoom_place), rde_memory_allocator_get_default_std());
    rde_arr _after  = rde_arr_new(sizeof(fude_zoom_place), rde_memory_allocator_get_default_std());
    if(_ok && fude_reader_has(_r, 4u)) {
        const u32 _n = fude_get_u32(_r);
        for(u32 _i = 0; _i < _n && _ok; _i++) {
            const u32             _index = fude_zoom_scene_find_object(_s, fude_get_u64(_r));
            const fude_zoom_place _b     = fude_zoom_get_place(_r);
            const fude_zoom_place _a     = fude_zoom_get_place(_r);
            _ok = _r->ok && _index != FUDE_ZOOM_NONE;
            if(_ok) {
                rde_arr_add(&_moved, (any)&_index);
                rde_arr_add(&_before, (any)&_b);
                rde_arr_add(&_after, (any)&_a);
            }
        }
    }
    const u32 _frame = fude_zoom_scene_find_frame(_s, _frame_id);
    if(_ok && _frame != FUDE_ZOOM_NONE) {
        fude_zoom_history_append(_s, _frame, _box, fude_zoom_u32s(&_died), fude_zoom_len(&_died), fude_zoom_u32s(&_born), fude_zoom_len(&_born),
                                 fude_zoom_u32s(&_moved), (const fude_zoom_place*)_before.memory, (const fude_zoom_place*)_after.memory, fude_zoom_len(&_moved));
    }
    rde_arr_free(&_died);
    rde_arr_free(&_born);
    rde_arr_free(&_moved);
    rde_arr_free(&_before);
    rde_arr_free(&_after);
    return _ok && _frame != FUDE_ZOOM_NONE;
}

b8 fude_zoom_apply_ops(fude_zoom_scene* _s, fude_reader* _r) {
    while(_r->ok && _r->pos < _r->size) {
        const u8  _op   = fude_get_u8(_r);
        const u32 _size = fude_get_u32(_r);
        if(!_r->ok || !fude_reader_has(_r, _size)) {
            return false;
        }
        fude_reader _body = fude_reader_make(_r->data + _r->pos, _size);
        _r->pos += _size;
        b8 _ok = true;
        switch(_op) {
            case FUDE_ZOOM_OP_FRAME:  _ok = fude_zoom_get_frame(_s, &_body);  break;
            case FUDE_ZOOM_OP_OBJECT: _ok = fude_zoom_get_object(_s, &_body); break;
            case FUDE_ZOOM_OP_ALIVE: {
                const u32 _index = fude_zoom_scene_find_object(_s, fude_get_u64(&_body));
                const b8  _alive = fude_get_u8(&_body) != 0;
                _ok = _body.ok && _index != FUDE_ZOOM_NONE;
                if(_ok) {
                    fude_zoom_scene_set_alive(_s, _index, _alive);
                }
                break;
            }
            case FUDE_ZOOM_OP_PUSH:   _ok = fude_zoom_get_action(_s, &_body); break;
            case FUDE_ZOOM_OP_COUNT: {
                const u32 _count = fude_get_u32(&_body);
                _ok = _body.ok && _count <= fude_zoom_len(&_s->actions);
                if(_ok) {
                    _s->action_count = _count;
                }
                break;
            }
            case FUDE_ZOOM_OP_DROP: {
                const u32 _n = fude_get_u32(&_body);
                _ok = _body.ok && _n <= _s->action_count;
                if(_ok) {
                    fude_zoom_history_drop_oldest(_s, _n);
                }
                break;
            }
            case FUDE_ZOOM_OP_PLACE: {
                const u32             _index = fude_zoom_scene_find_object(_s, fude_get_u64(&_body));
                const fude_zoom_place _p     = fude_zoom_get_place(&_body);
                _ok = _body.ok && _index != FUDE_ZOOM_NONE;
                if(_ok) {
                    fude_zoom_scene_set_place(_s, _index, _p);
                }
                break;
            }
            case FUDE_ZOOM_OP_ROOT: {
                const u32 _slot = fude_zoom_scene_find_frame(_s, fude_get_u64(&_body));
                _ok = _body.ok && _slot != FUDE_ZOOM_NONE;
                if(_ok) {
                    _s->root = _slot;
                }
                break;
            }
            default: break;   // an op of a newer build: skipped
        }
        if(!_ok) {
            return false;
        }
    }
    return _r->ok;
}

void fude_zoom_scene_loaded(fude_zoom_scene* _s) {
    // What no undo step can bring back is garbage now.
    for(u32 _i = 0; _i < fude_zoom_len(&_s->objects); _i++) {
        fude_zoom_scene_collect(_s, _i);
    }
    rde_arr _values = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr _boxes  = rde_arr_new(sizeof(fude_zoom_box), rde_memory_allocator_get_default_std());
    for(u32 _f = 0; _f < fude_zoom_len(&_s->frames); _f++) {
        fude_zoom_frame* _fr = &fude_zoom_frames(_s)[_f];
        rde_arr_clear(&_values);
        rde_arr_clear(&_boxes);
        for(u32 _i = 0; _i < fude_zoom_len(&_fr->arrival); _i++) {
            const u32               _index = fude_zoom_u32s(&_fr->arrival)[_i];
            const fude_zoom_object* _o     = &fude_zoom_objects(_s)[_index];
            if(!(_o->flags & FUDE_ZOOM_FLAG_GARBAGE)) {
                rde_arr_add(&_values, (any)&_index);
                rde_arr_add(&_boxes, (any)&_o->box);
            }
        }
        fude_zoom_index_build(&_fr->index, fude_zoom_u32s(&_values), (const fude_zoom_box*)_boxes.memory, fude_zoom_len(&_values));
        _fr->dead  = 0;
        _fr->stale = 0;
    }
    rde_arr_free(&_values);
    rde_arr_free(&_boxes);
    rde_arr_clear(&_s->journal);
    // A file from before frames said which is home: the root is.
    if(_s->home == FUDE_ZOOM_NONE || _s->home >= fude_zoom_len(&_s->frames) || fude_zoom_frames(_s)[_s->home].removed) {
        _s->home = _s->root;
    }
}
