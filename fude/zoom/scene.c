// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/scene_parts.h"

// ===========================================================================
// See scene.h.
//
// In order: the tables and ids; the scene; frames (their order, their index, their FRAME objects, their journal
// records); payloads (blobs, shared by copies and counted); objects of each kind made, read and moved; the camera. The
// history is in scene_history.c, and the records, as the file and the journal keep them, in scene_records.c. Every
// change that is not a replay from the file is written into the journal as it happens, and bumps the revision.
//
// Each frame's index is only added to. Each object goes in once, with its box (one whose box comes from its payload is
// boxed first); one that moves goes in again at its new place, and its old entry counts as stale. Queries filter what
// they find by each object's own box and drop the duplicates (sorting what they found only while an entry is stale),
// and the index is built again (fude_zoom_scene_reindex) once dead and stale entries pile up, which is checked each
// time an object moves or becomes garbage. While a file loads (scene.h, loading), objects go into their frames'
// orders as they come and into no index; fude_zoom_scene_loaded sorts each order and builds each index once. FRAME
// objects are in their parent's order but in no index.
//
// Part of the scene: scene_parts.h holds what its modules share and says which module holds what. Main thread only.
// ===========================================================================

// The scene's tables as arrays; valid until the next element is added to that table.
fude_zoom_frame*  fude_zoom_frames(const fude_zoom_scene* _s)  { return (fude_zoom_frame*)_s->frames.memory; }
fude_zoom_object* fude_zoom_objects(const fude_zoom_scene* _s) { return (fude_zoom_object*)_s->objects.memory; }
fude_zoom_blob*   fude_zoom_blobs(const fude_zoom_scene* _s)   { return (fude_zoom_blob*)_s->blobs.memory; }
fude_zoom_action* fude_zoom_actions(const fude_zoom_scene* _s) { return (fude_zoom_action*)_s->actions.memory; }
u32*              fude_zoom_targets(const fude_zoom_scene* _s) { return (u32*)_s->targets.memory; }
fude_zoom_place*  fude_zoom_places(const fude_zoom_scene* _s)  { return (fude_zoom_place*)_s->places.memory; }
// An array of u32 as a pointer.
u32*              fude_zoom_u32s(const rde_arr* _a)            { return (u32*)_a->memory; }

// Returns an array's length as a u32.
u32 fude_zoom_len(const rde_arr* _a) {
    return (u32)rde_arr_length(_a);
}

// --- id → slot ------------------------------------------------------------------------------

// Makes an empty map from an id (u64) to a slot (u32), on the std allocator.
RDE_INTERNAL rde_hash_map fude_zoom_ids_new(void) {
    return rde_hash_map_new(sizeof(fude_zoom_id), sizeof(u32), rde_hash_map_fn_u64_hash, rde_hash_map_fn_u64_cmp, NULL, rde_memory_allocator_get_default_std());
}

// Maps _id to _slot: a new id, or one that is there already (a FRAME object made again) moved to _slot.
void fude_zoom_ids_set(rde_hash_map* _m, fude_zoom_id _id, u32 _slot) {
    if(!rde_hash_map_add(_m, &_id, &_slot)) {
        rde_hash_map_update_entry(_m, &_id, &_slot);
    }
}

// Returns _id's slot, or FUDE_ZOOM_NONE. Ids are never 0 (the counter starts at 1, the file's are checked), so 0 is
// never there.
RDE_INTERNAL u32 fude_zoom_ids_get(const rde_hash_map* _m, fude_zoom_id _id) {
    const u32* _slot = _m->memory != NULL ? rde_hash_map_find(_m, &_id) : NULL;
    return _slot != NULL ? *_slot : FUDE_ZOOM_NONE;
}

// --- the scene ---------------------------------------------------------------------------

// Sets up an empty frame: its arrays on the std allocator, its index empty, no parent or FRAME object, an empty anchor,
// the identity place, and its first order key one step in.
void fude_zoom_frame_init(fude_zoom_frame* _f) {
    rde_memset(_f, 0, sizeof(*_f));
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

// Frees a frame's arrays and its index.
RDE_INTERNAL void fude_zoom_frame_destroy(fude_zoom_frame* _f) {
    rde_arr* _arrays[] = { &_f->order, &_f->kids, &_f->arrival, &_f->dirty, &_f->offsets, &_f->sizes };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    fude_zoom_index_destroy(&_f->index);
}

// See the header.
void fude_zoom_scene_init(fude_zoom_scene* _s, u32 _device) {
    rde_memset(_s, 0, sizeof(*_s));
    // A canvas has no natural size limit: everything on the standard heap (the engine's default allocator is a fixed
    // budget, and running it out is fatal).
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _s->frames     = rde_arr_new(sizeof(fude_zoom_frame), _heap);
    _s->objects    = rde_arr_new(sizeof(fude_zoom_object), _heap);
    _s->blobs      = rde_arr_new(sizeof(fude_zoom_blob), _heap);
    _s->free_blobs = rde_stack_new(sizeof(u32), _heap);
    _s->actions    = rde_arr_new(sizeof(fude_zoom_action), _heap);
    _s->targets    = rde_arr_new(sizeof(u32), _heap);
    _s->places     = rde_arr_new(sizeof(fude_zoom_place), _heap);
    _s->object_ids = fude_zoom_ids_new();
    _s->frame_ids  = fude_zoom_ids_new();
    _s->journal    = fude_bytes_new(4096u);
    _s->device         = _device;
    _s->history_budget = FUDE_ZOOM_HISTORY_BUDGET;
    for(u32 _l = 0; _l < FUDE_ZOOM_LAYERS; _l++) {
        _s->layer_rank[_l] = (u8)_l;
    }

    fude_zoom_frame* _root = rde_arr_add(&_s->frames, NULL);
    fude_zoom_frame_init(_root);
    _root->id = fude_zoom_scene_new_id(_s);
    fude_zoom_ids_set(&_s->frame_ids, _root->id, 0);
    _s->root   = 0;
    _s->home   = 0;
    _s->camera = (fude_zoom_camera){ 0, { 0.0, 0.0 }, 1.0 };
}

// See the header. Safe on a zeroed scene.
void fude_zoom_scene_destroy(fude_zoom_scene* _s) {
    if(rde_arr_is_inited(&_s->frames)) {
        for(u32 _i = 0; _i < fude_zoom_len(&_s->frames); _i++) {
            fude_zoom_frame_destroy(&fude_zoom_frames(_s)[_i]);
        }
    }
    if(rde_arr_is_inited(&_s->blobs)) {
        for(u32 _i = 0; _i < fude_zoom_len(&_s->blobs); _i++) {
            if(rde_arr_is_inited(&fude_zoom_blobs(_s)[_i].data)) {
                rde_arr_free(&fude_zoom_blobs(_s)[_i].data);
            }
        }
    }
    rde_arr* _arrays[] = { &_s->frames, &_s->objects, &_s->blobs, &_s->actions, &_s->targets, &_s->places, &_s->journal };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    if(rde_stack_is_inited(&_s->free_blobs)) {
        rde_stack_free(&_s->free_blobs);
    }
    rde_hash_map_free(&_s->object_ids);
    rde_hash_map_free(&_s->frame_ids);
    rde_memset(_s, 0, sizeof(*_s));
}

// See the header.
fude_zoom_id fude_zoom_scene_new_id(fude_zoom_scene* _s) {
    _s->counter++;
    return ((fude_zoom_id)_s->device << 32) | (fude_zoom_id)_s->counter;
}

// Notes an id the file brought, so new ids never repeat it: this device's counter goes past it.
void fude_zoom_scene_saw_id(fude_zoom_scene* _s, fude_zoom_id _id) {
    if((u32)(_id >> 32) == _s->device && (u32)_id > _s->counter) {
        _s->counter = (u32)_id;
    }
}

// See the header. Neither is checked against the table's length.
fude_zoom_frame* fude_zoom_scene_frame(const fude_zoom_scene* _s, u32 _slot) {
    return &fude_zoom_frames(_s)[_slot];
}

// See the header.
fude_zoom_object* fude_zoom_scene_object(const fude_zoom_scene* _s, u32 _index) {
    return &fude_zoom_objects(_s)[_index];
}

// See the header.
u32 fude_zoom_scene_frame_count(const fude_zoom_scene* _s) {
    return fude_zoom_len(&_s->frames);
}

// See the header.
u32 fude_zoom_scene_object_count(const fude_zoom_scene* _s) {
    return fude_zoom_len(&_s->objects);
}

// See the header. A hash map lookup.
u32 fude_zoom_scene_find_object(const fude_zoom_scene* _s, fude_zoom_id _id) {
    return fude_zoom_ids_get(&_s->object_ids, _id);
}

// See the header. A hash map lookup.
u32 fude_zoom_scene_find_frame(const fude_zoom_scene* _s, fude_zoom_id _id) {
    return fude_zoom_ids_get(&_s->frame_ids, _id);
}

// See the header. A walk up the parents, at most 100000 of them.
u32 fude_zoom_scene_depth(const fude_zoom_scene* _s, u32 _frame) {
    u32 _d = 0;
    while(fude_zoom_frames(_s)[_frame].parent != FUDE_ZOOM_NONE && _d < 100000u) {
        _frame = fude_zoom_frames(_s)[_frame].parent;
        _d++;
    }
    return _d;
}

// See the header. A pass over every object.
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

// Starts a journal op: its kind, and a size filled in by fude_zoom_journal_op_end; *_at is where the size is.
void fude_zoom_journal_op_begin(fude_zoom_scene* _s, u8 _op, u32* _at) {
    fude_put_u8(&_s->journal, _op);
    *_at = fude_bytes_size(&_s->journal);
    fude_put_u32(&_s->journal, 0u);
}

// Ends a journal op begun at _at: its size written in.
void fude_zoom_journal_op_end(fude_zoom_scene* _s, u32 _at) {
    fude_chunk_end(&_s->journal, _at);   // The same 4-byte size fill-in a chunk's is.
}

// --- frames ------------------------------------------------------------------------------

// Marks the bucket an object is in as changed (the next checkpoint writes it).
void fude_zoom_scene_touch(fude_zoom_scene* _s, const fude_zoom_object* _o) {
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

// Returns where in _f's order an object with key _z goes: after every key ≤ _z (a binary search).
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

// Puts an object into its frame's order by its key, and moves the frame's next key past it. While a file loads it is
// appended as it comes instead (put in its place, each object loaded moved every one after it), and
// fude_zoom_order_settle sorts the order once at the end into the order putting each in its place gives.
RDE_INTERNAL void fude_zoom_order_insert(fude_zoom_scene* _s, u32 _frame, u32 _object) {
    fude_zoom_frame* _f = &fude_zoom_frames(_s)[_frame];
    const u64        _z = fude_zoom_objects(_s)[_object].z;
    if(_s->loading) {
        rde_arr_add(&_f->order, &_object);
    } else {
        rde_arr_insert(&_f->order, fude_zoom_order_slot(_s, _f, _z), &_object);
    }
    if(_z >= _f->z_next) {
        _f->z_next = (_z / FUDE_ZOOM_Z_STEP + 1u) * FUDE_ZOOM_Z_STEP;
    }
}

// One object of a frame's order while it is sorted (fude_zoom_order_settle): its key, and the object.
typedef struct {
    u64 z;
    u32 object;
} fude_zoom_order_entry;

// rde_sort's order for a loaded frame's order: by key, then by the object's index.
RDE_INTERNAL i32 fude_zoom_order_by_key(const any _a, const any _b) {
    const fude_zoom_order_entry* _x = (const fude_zoom_order_entry*)_a;
    const fude_zoom_order_entry* _y = (const fude_zoom_order_entry*)_b;
    if(_x->z != _y->z) {
        return _x->z < _y->z ? -1 : 1;
    }
    return (_x->object > _y->object) - (_x->object < _y->object);
}

// See scene_parts.h. One at a time, each object went after every one whose key is at most its own: a stable sort by
// key of the order they came in. Every object in a loaded order was made while it loaded, in the order it came, so
// their indices are that order: a sort by key, then by index, gives the same order (rde_sort itself is not stable).
// Most orders come in their keys' order already (objects arrive in the order they were made), and are left as they
// are; pieces an eraser cut, copies on another layer and FRAME objects (read before the objects round them) are what
// arrive out of it.
void fude_zoom_order_settle(fude_zoom_scene* _s, u32 _frame) {
    fude_zoom_frame* _f      = &fude_zoom_frames(_s)[_frame];
    u32*             _order  = fude_zoom_u32s(&_f->order);
    const u32        _n      = fude_zoom_len(&_f->order);
    u32              _sorted = 1u;   // how many at its start are in order
    while(_sorted < _n && fude_zoom_objects(_s)[_order[_sorted - 1u]].z <= fude_zoom_objects(_s)[_order[_sorted]].z) {
        _sorted++;
    }
    if(_sorted >= _n) {
        return;
    }
    rde_arr TYPE(fude_zoom_order_entry) _entries = rde_arr_new(sizeof(fude_zoom_order_entry), rde_memory_allocator_get_default_std());
    fude_zoom_order_entry* _e = rde_arr_add_n(&_entries, _n);   // (sized once: it stays put)
    for(u32 _i = 0; _i < _n; _i++) {
        _e[_i] = (fude_zoom_order_entry){ fude_zoom_objects(_s)[_order[_i]].z, _order[_i] };
    }
    rde_sort(_e, _n, sizeof(_e[0]), fude_zoom_order_by_key);
    for(u32 _i = 0; _i < _n; _i++) {
        _order[_i] = _e[_i].object;
    }
    rde_arr_free(&_entries);
}

// --- frames' indices ---------------------------------------------------------------------

// Puts an object into its frame's index at its box, unless a file is loading (fude_zoom_scene_loaded builds every
// index whole, once).
RDE_INTERNAL void fude_zoom_scene_index_add(fude_zoom_scene* _s, u32 _object) {
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(!_s->loading) {
        fude_zoom_index_insert(&fude_zoom_frames(_s)[_o->frame].index, _object, _o->box);
    }
}

// See scene_parts.h. Built from the frame's arrival order (every object in it, garbage left out).
void fude_zoom_scene_index_build(fude_zoom_scene* _s, u32 _frame) {
    fude_zoom_frame* _f = &fude_zoom_frames(_s)[_frame];
    const u32 _n = fude_zoom_len(&_f->arrival);
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
    fude_zoom_index_build(&_f->index, fude_zoom_u32s(&_values), (const fude_zoom_box*)_boxes.memory, fude_zoom_len(&_values));
    _f->dead  = 0;
    _f->stale = 0;
    rde_arr_free(&_values);
    rde_arr_free(&_boxes);
}

// Builds a frame's index again (packed) once its dead and stale entries are more than 64 and more than half its
// objects: called each time an object in it moves (a stale entry) or becomes garbage (a dead one), so neither piles
// up past that. Not while a file loads: its indices are built at the end.
RDE_INTERNAL void fude_zoom_scene_reindex(fude_zoom_scene* _s, u32 _frame) {
    const fude_zoom_frame* _f     = &fude_zoom_frames(_s)[_frame];
    const u32              _waste = _f->dead + _f->stale;
    if(_s->loading || _waste <= 64u || _waste * 2u <= fude_zoom_len(&_f->arrival)) {
        return;
    }
    fude_zoom_scene_index_build(_s, _frame);
}

// Makes the FRAME object for frame _child in its parent, at key _z (0: on top): alive unless the frame is removed, its
// box the frame's anchor. Nothing for a root.
void fude_zoom_scene_frame_object(fude_zoom_scene* _s, u32 _child, u64 _z) {
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
    fude_zoom_ids_set(&_s->object_ids, _o.id, _index);
    fude_zoom_frames(_s)[_child].object = _index;
    fude_zoom_order_insert(_s, _c->parent, _index);
    rde_arr_add(&fude_zoom_frames(_s)[_c->parent].kids, (any)&_index);
}

// Writes the frame's own record into the journal, and its parents' first if the file does not have them yet (a frame
// is only written once something is drawn in it: zooming about leaves no trace). Not while replaying.
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

// See the header. Nothing for a frame already kept, or removed.
void fude_zoom_scene_keep_frame(fude_zoom_scene* _s, u32 _frame) {
    if(_frame < fude_zoom_scene_frame_count(_s) && !fude_zoom_frames(_s)[_frame].saved && !fude_zoom_frames(_s)[_frame].removed) {
        fude_zoom_journal_frame(_s, _frame);
    }
}

// See the header.
u32 fude_zoom_scene_new_frame(fude_zoom_scene* _s, u32 _parent, fude_zoom_xform _xf, fude_zoom_box _anchor) {
    fude_zoom_frame* _f = rde_arr_add(&_s->frames, NULL);
    fude_zoom_frame_init(_f);
    _f->id     = fude_zoom_scene_new_id(_s);
    _f->parent = _parent;
    _f->xf     = _xf;
    _f->anchor = _anchor;
    const u32 _slot = fude_zoom_len(&_s->frames) - 1u;
    fude_zoom_ids_set(&_s->frame_ids, _f->id, _slot);
    fude_zoom_scene_frame_object(_s, _slot, 0);
    return _slot;
}

// See the header. The old root's anchor in the new root is what it holds as far as it is known (its live objects'
// boxes) at the new scale. The new root, the old one and an OP_ROOT are journaled only when the old root is in the
// file already.
u32 fude_zoom_scene_new_root(fude_zoom_scene* _s) {
    const u32 _old  = _s->root;
    fude_zoom_frame* _f = rde_arr_add(&_s->frames, NULL);
    fude_zoom_frame_init(_f);
    _f->id = fude_zoom_scene_new_id(_s);
    const u32 _slot = fude_zoom_len(&_s->frames) - 1u;
    fude_zoom_ids_set(&_s->frame_ids, _f->id, _slot);

    fude_zoom_frame* _o = &fude_zoom_frames(_s)[_old];
    _o->parent = _slot;
    _o->xf     = (fude_zoom_xform){ 0.0, 0.0, FUDE_ZOOM_CHILD_SCALE, 0.0 };
    _o->anchor = fude_zoom_box_empty();
    // Its anchor in the new root: what it holds, as far as it is known (its objects' boxes), at the new scale.
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

// See the header. Each side's chain is walked up to the same depth, then both up together to the frame they share;
// the result is the way up from _from, then down to _to.
fude_zoom_sim fude_zoom_scene_sim(const fude_zoom_scene* _s, u32 _from, u32 _to) {
    // Each side's chain of frames up to the root, and the nearest frame both have.
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

// See the header. At most 4096 frames up are looked at.
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

// See the header.
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

// See the header. A frame in the file is journaled again with its new anchor.
void fude_zoom_scene_frame_reach(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box) {
    fude_zoom_frame* _f = &fude_zoom_frames(_s)[_frame];
    if(_f->parent == FUDE_ZOOM_NONE || fude_zoom_box_is_empty(_box)) {
        return;
    }
    const fude_zoom_box _in_parent = fude_zoom_sim_box(fude_zoom_sim_from_xform(_f->xf), _box);
    const fude_zoom_box _grown     = fude_zoom_box_union(_f->anchor, _in_parent);
    if(rde_memcmp(&_grown, &_f->anchor, sizeof(_grown)) == 0) {
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

// Makes a payload of _size bytes copied from _data, its room just what it holds, with one reference. Returns its
// handle: its slot plus 1 (0 means none), reusing a freed slot first.
u32 fude_zoom_blob_new(fude_zoom_scene* _s, const u8* _data, u32 _size) {
    // Its room just what it holds: a canvas keeps many.
    fude_zoom_blob _b = { .data = rde_arr_new_with_capacity(sizeof(u8), (usize)_size + 1u, rde_memory_allocator_get_default_std()), .refs = 1u,
                          .serial = ++_s->blob_serials };
    if(_size > 0) {
        rde_memcpy(rde_arr_add_n(&_b.data, _size), _data, _size);
    }
    const u32* _freed = (const u32*)rde_stack_peek(&_s->free_blobs);   // (the slot freed last, if any)
    if(_freed != NULL) {
        const u32 _slot = *_freed;
        rde_stack_pop(&_s->free_blobs);
        fude_zoom_blobs(_s)[_slot] = _b;
        return _slot + 1u;
    }
    rde_arr_add(&_s->blobs, &_b);
    return fude_zoom_len(&_s->blobs);
}

// See the header.
u32 fude_zoom_blob_serial(const fude_zoom_scene* _s, u32 _handle) {
    return _handle != 0 ? fude_zoom_blobs(_s)[_handle - 1u].serial : 0u;
}

// Lets go of one reference to a payload; the last one frees its bytes and its slot. Handle 0 does nothing.
RDE_INTERNAL void fude_zoom_blob_release(fude_zoom_scene* _s, u32 _handle) {
    if(_handle == 0) {
        return;
    }
    fude_zoom_blob* _b = &fude_zoom_blobs(_s)[_handle - 1u];
    if(_b->refs > 1u) {
        _b->refs--;
        return;
    }
    rde_arr_free(&_b->data);
    rde_memset(_b, 0, sizeof(*_b));
    const u32 _slot = _handle - 1u;
    rde_stack_push(&_s->free_blobs, (const any)&_slot);
}

// Makes an object garbage when it is dead and no undo step names it: its points let go for good, its bucket marked
// changed (the file leaves it out), and its frame's dead entries counted, the index built again once they pile up
// (fude_zoom_scene_reindex). Frames are never collected.
void fude_zoom_scene_collect(fude_zoom_scene* _s, u32 _object) {
    fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(_o->kind == FUDE_ZOOM_KIND_FRAME || (_o->flags & (FUDE_ZOOM_FLAG_ALIVE | FUDE_ZOOM_FLAG_GARBAGE)) || _o->history > 0) {
        return;
    }
    _o->flags |= FUDE_ZOOM_FLAG_GARBAGE;
    fude_zoom_blob_release(_s, _o->blob);
    _o->blob = 0;
    fude_zoom_scene_touch(_s, _o);
    fude_zoom_frames(_s)[_o->frame].dead++;
    fude_zoom_scene_reindex(_s, _o->frame);
}

// --- objects -----------------------------------------------------------------------------

// See the header.
fude_zoom_v2 fude_zoom_paper_snap(fude_zoom_v2 _at, f64 _scale) {
    const f64 _step = FUDE_ZOOM_PAPER_ALIGN * _scale;
    return (fude_zoom_v2){ rde_math_round_f64(_at.x / _step) * _step, rde_math_round_f64(_at.y / _step) * _step };
}

// See the header: floor(log2(1 / (16 · z))), kept between −100 and 100.
i8 fude_zoom_quantum_for(f64 _z) {
    const f64 _q = rde_math_floor_f64(rde_math_log2_f64(1.0 / (16.0 * _z)));
    return (i8)(_q < -100.0 ? -100 : (_q > 100.0 ? 100 : (i32)_q));
}

// See the header.
fude_zoom_v2 fude_zoom_scene_local_at(const fude_zoom_object* _o, const fude_zoom_qpoint* _p) {
    return (fude_zoom_v2){ rde_math_ldexp_f64((f64)_p->x, _o->q), rde_math_ldexp_f64((f64)_p->y, _o->q) };
}

// See the header. The common case, neither turned nor scaled, is a plain sum.
fude_zoom_v2 fude_zoom_scene_point_at(const fude_zoom_object* _o, const fude_zoom_qpoint* _p) {
    const fude_zoom_v2 _l = fude_zoom_scene_local_at(_o, _p);
    if(_o->rotation == 0.0 && _o->scale == 1.0) {
        return (fude_zoom_v2){ _o->t.x + _l.x, _o->t.y + _l.y };
    }
    return fude_zoom_sim_apply(fude_zoom_object_sim(_o), _l);
}

// See the header. Pressure is a share of the half-width in 1023ths.
f32 fude_zoom_scene_local_radius_at(const fude_zoom_object* _o, const fude_zoom_qpoint* _p) {
    if(!(_o->flags & FUDE_ZOOM_FLAG_PRESSURE)) {
        return _o->radius;
    }
    return _o->radius * (f32)_p->pressure / 1023.0f;
}

// See the header.
f32 fude_zoom_scene_radius_at(const fude_zoom_object* _o, const fude_zoom_qpoint* _p) {
    return (f32)((f64)fude_zoom_scene_local_radius_at(_o, _p) * _o->scale);
}

// See the header.
fude_zoom_sim fude_zoom_object_sim(const fude_zoom_object* _o) {
    return (fude_zoom_sim){ _o->scale * rde_math_cos_f64(_o->rotation), _o->scale * rde_math_sin_f64(_o->rotation), _o->t.x, _o->t.y };
}

// Returns the box round a stroke's or a fill's points where they are, each grown by its half-width.
RDE_INTERNAL fude_zoom_box fude_zoom_stroke_box(const fude_zoom_object* _o, const fude_zoom_qpoint* _points, u32 _count) {
    fude_zoom_box _b = fude_zoom_box_empty();
    for(u32 _i = 0; _i < _count; _i++) {
        const fude_zoom_v2 _p = fude_zoom_scene_point_at(_o, &_points[_i]);
        const f64          _r = fude_zoom_scene_radius_at(_o, &_points[_i]);
        _b = fude_zoom_box_union(_b, (fude_zoom_box){ _p.x - _r, _p.y - _r, _p.x + _r, _p.y + _r });
    }
    return _b;
}

// See scene_parts.h.
u32 fude_zoom_scene_put_in(fude_zoom_scene* _s, fude_zoom_object* _o, b8 _boxed) {
    fude_zoom_frame* _f = &fude_zoom_frames(_s)[_o->frame];
    _o->file_pos = fude_zoom_len(&_f->arrival);
    rde_arr_add(&_s->objects, _o);
    const u32 _index = fude_zoom_len(&_s->objects) - 1u;
    fude_zoom_ids_set(&_s->object_ids, _o->id, _index);
    _f = &fude_zoom_frames(_s)[_o->frame];
    rde_arr_add(&_f->arrival, (any)&_index);
    const fude_zoom_object* _placed = &fude_zoom_objects(_s)[_index];
    if(!(_placed->flags & FUDE_ZOOM_FLAG_GARBAGE)) {
        fude_zoom_order_insert(_s, _placed->frame, _index);
        if(_boxed) {
            fude_zoom_scene_index_add(_s, _index);
        }
    }
    return _index;
}

// See the header.
u32 fude_zoom_scene_add_stroke(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _t, i8 _q, const fude_zoom_qpoint* _points, u32 _count,
                               u8 _channels, rde_color _color, f32 _radius, u8 _flags, fude_zoom_id _gesture, u64 _z) {
    return fude_zoom_scene_add_stroke_at(_s, _frame, (fude_zoom_place){ _t, 0.0, 1.0 }, _q, _points, _count, _channels, _color, _radius, _flags, _gesture, _z);
}

// Makes a new object of kind _kind from a stroke's or a fill's points (encoded by codec.h): alive, in _frame at
// _place, on the scene's layer, boxed, indexed and journaled, its frame journaled first if it was not. Returns its
// index.
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
    const u32 _index = fude_zoom_scene_put_in(_s, &_o, true);
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

// See the header.
u32 fude_zoom_scene_add_stroke_at(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, i8 _q, const fude_zoom_qpoint* _points, u32 _count,
                                  u8 _channels, rde_color _color, f32 _radius, u8 _flags, fude_zoom_id _gesture, u64 _z) {
    return fude_zoom_scene_add_points(_s, FUDE_ZOOM_KIND_STROKE, _frame, _place, _q, _points, _count, _channels, _color, _radius, _flags, _gesture, _z);
}

// See the header.
u32 fude_zoom_scene_add_fill(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, i8 _q, const fude_zoom_qpoint* _points, u32 _count,
                             rde_color _color, u64 _z) {
    return fude_zoom_scene_add_fill_flags(_s, _frame, _place, _q, _points, _count, _color, 0u, _z);
}

// See the header. The time channel is kept only when some point has a ring other than 0.
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

// Declared here for the adders below; defined with the places.
RDE_INTERNAL fude_zoom_box fude_zoom_shape_box(const fude_zoom_scene* _s, u32 _object);
RDE_INTERNAL void          fude_zoom_scene_rebox(fude_zoom_scene* _s, u32 _object);

// See the header. Never with its own fill.
u32 fude_zoom_scene_add_shape(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, u8 _type, const f64* _numbers, u32 _count,
                              rde_color _color, f32 _radius, u8 _flags, u64 _z) {
    return fude_zoom_scene_add_shape_fill(_s, _frame, _place, _type, _numbers, _count, _color, _radius, (u8)(_flags & ~FUDE_ZOOM_FLAG_FILL_OWN), (rde_color){ 0, 0, 0, 0 }, _z);
}

// See the header. In the scene's layer and style.
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
    // Placed with no box: its own (from its outline, which needs it placed) worked out, then into the index once.
    const u32 _index = fude_zoom_scene_put_in(_s, &_o, false);
    fude_zoom_objects(_s)[_index].box = fude_zoom_shape_box(_s, _index);
    fude_zoom_scene_index_add(_s, _index);
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

// See the header. Its payload: f64 zoom, u32 number.
u32 fude_zoom_scene_add_mark(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _at, f64 _z, u32 _number) {
    u8 _payload[sizeof(f64) + sizeof(u32)];
    rde_memcpy(_payload, &_z, sizeof(f64));
    rde_memcpy(_payload + sizeof(f64), &_number, sizeof(u32));
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
    const u32 _index = fude_zoom_scene_put_in(_s, &_o, true);
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

// See the header. Its payload: u16 index, u8 flags, then up to 64 bytes of its name.
u32 fude_zoom_scene_add_layer(fude_zoom_scene* _s, u32 _frame, u16 _index, u8 _flags, const c8* _name) {
    const usize _len = _name != NULL ? rde_strlen(_name) : 0u;
    u8 _payload[3u + 64u];
    const usize _keep = _len < 64u ? _len : 64u;
    rde_memcpy(_payload, &_index, sizeof(u16));
    _payload[2] = _flags;
    if(_keep > 0) {
        rde_memcpy(_payload + 3u, _name, _keep);
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
    const u32 _at = fude_zoom_scene_put_in(_s, &_o, true);
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

// See the header.
b8 fude_zoom_scene_layer_of(const fude_zoom_scene* _s, u32 _object, u16* _index, u8* _flags, c8* _name, usize _size) {
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(_o->kind != FUDE_ZOOM_KIND_LAYER || _o->blob == 0) {
        return false;
    }
    const fude_zoom_blob* _b = &fude_zoom_blobs(_s)[_o->blob - 1u];
    if(fude_zoom_blob_size(_b) < 3u) {
        return false;
    }
    if(_index != NULL) { rde_memcpy(_index, fude_zoom_blob_bytes(_b), sizeof(u16)); }
    if(_flags != NULL) { *_flags = fude_zoom_blob_bytes(_b)[2]; }
    if(_name != NULL && _size > 0) {
        const usize _n = fude_zoom_blob_size(_b) - 3u < _size - 1u ? fude_zoom_blob_size(_b) - 3u : _size - 1u;
        rde_memcpy(_name, fude_zoom_blob_bytes(_b) + 3u, _n);
        _name[_n] = 0;
    }
    return true;
}

// See the header. The latest alive LAYER object for each index (by order key) wins; a layer never moved keeps its
// index as its rank.
void fude_zoom_scene_layers_refresh(fude_zoom_scene* _s) {
    u32 _hidden = 0, _locked = 0;
    u64 _seen_z[FUDE_ZOOM_LAYERS];
    rde_memset(_seen_z, 0, sizeof(_seen_z));
    for(u32 _l = 0; _l < FUDE_ZOOM_LAYERS; _l++) {
        _s->layer_rank[_l] = (u8)_l;   // Never moved: in their indexes' order.
    }
    for(u32 _i = 0; _i < fude_zoom_len(&_s->objects); _i++) {
        const fude_zoom_object* _o = &fude_zoom_objects(_s)[_i];
        u16 _index;
        u8  _flags;
        if(_o->kind != FUDE_ZOOM_KIND_LAYER || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || !fude_zoom_scene_layer_of(_s, _i, &_index, &_flags, NULL, 0)) {
            continue;
        }
        if(_index >= FUDE_ZOOM_LAYERS || _o->z < _seen_z[_index]) {
            continue;   // The latest of an index's wins.
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

// See the header.
b8 fude_zoom_scene_mark_of(const fude_zoom_scene* _s, u32 _object, f64* _z, u32* _number) {
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(_o->kind != FUDE_ZOOM_KIND_MARK || _o->blob == 0) {
        return false;
    }
    const fude_zoom_blob* _b = &fude_zoom_blobs(_s)[_o->blob - 1u];
    if(fude_zoom_blob_size(_b) < sizeof(f64) + sizeof(u32)) {
        return false;
    }
    if(_z != NULL) {
        rde_memcpy(_z, fude_zoom_blob_bytes(_b), sizeof(f64));
    }
    if(_number != NULL) {
        rde_memcpy(_number, fude_zoom_blob_bytes(_b) + sizeof(f64), sizeof(u32));
    }
    return true;
}

// Makes an object of a kind whose payload is bytes of its own (a picture, a text) in the frame: boxed (from its
// payload's sizes), indexed and journaled. Returns its index.
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
    // Placed with no box: its own (from its payload's sizes: it must be placed first) worked out, then indexed once.
    const u32 _index = fude_zoom_scene_put_in(_s, &_o, false);
    fude_zoom_scene_rebox(_s, _index);
    fude_zoom_scene_index_add(_s, _index);
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

// See the header. Its payload: f64 half-width, f64 half-height, then the picture's bytes; it is drawn in white.
u32 fude_zoom_scene_add_image(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, f64 _hw, f64 _hh, const u8* _bytes, u32 _size, u64 _z) {
    // Its payload: its half-size first, then the picture as it came.
    rde_arr TYPE(u8) _buf = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_buf, (usize)_size + 2u * sizeof(f64));
    u8* _payload = (u8*)_buf.memory;
    rde_memcpy(_payload, &_hw, sizeof(f64));
    rde_memcpy(_payload + sizeof(f64), &_hh, sizeof(f64));
    rde_memcpy(_payload + 2u * sizeof(f64), _bytes, _size);
    const u32 _index = fude_zoom_scene_add_payload(_s, FUDE_ZOOM_KIND_IMAGE, _frame, _place, _payload, _size + 2u * (u32)sizeof(f64),
                                                   (rde_color){ 255, 255, 255, 255 }, (rde_color){ 0, 0, 0, 0 }, _z);
    rde_arr_free(&_buf);
    return _index;
}

// A text's payload's head: f64 size, f64 width, f64 height, u32 style; its words follow.
#define FUDE_ZOOM_TEXT_HEAD (3u * (u32)sizeof(f64) + (u32)sizeof(u32))

// See the header.
u32 fude_zoom_scene_add_text(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, f64 _size, f64 _w, f64 _h, u8 _style,
                             rde_color _color, rde_color _fill, const c8* _text, u32 _len, u64 _z) {
    rde_arr TYPE(u8) _buf = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_buf, (usize)_len + FUDE_ZOOM_TEXT_HEAD);
    u8* _payload = (u8*)_buf.memory;
    const u32 _st = _style;
    rde_memcpy(_payload, &_size, sizeof(f64));
    rde_memcpy(_payload + sizeof(f64), &_w, sizeof(f64));
    rde_memcpy(_payload + 2u * sizeof(f64), &_h, sizeof(f64));
    rde_memcpy(_payload + 3u * sizeof(f64), &_st, sizeof(u32));
    if(_len > 0) {
        rde_memcpy(_payload + FUDE_ZOOM_TEXT_HEAD, _text, _len);
    }
    const u32 _index = fude_zoom_scene_add_payload(_s, FUDE_ZOOM_KIND_TEXT, _frame, _place, _payload, _len + FUDE_ZOOM_TEXT_HEAD, _color, _fill, _z);
    rde_arr_free(&_buf);
    return _index;
}

// See the header.
b8 fude_zoom_scene_text(const fude_zoom_scene* _s, u32 _object, f64* _size, f64* _w, f64* _h, u8* _style, const c8** _text, u32* _len) {
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(_o->kind != FUDE_ZOOM_KIND_TEXT || _o->blob == 0) {
        return false;
    }
    const fude_zoom_blob* _b = &fude_zoom_blobs(_s)[_o->blob - 1u];
    if(fude_zoom_blob_size(_b) < FUDE_ZOOM_TEXT_HEAD) {
        return false;
    }
    u32 _st = 0;
    if(_size != NULL)  { rde_memcpy(_size, fude_zoom_blob_bytes(_b), sizeof(f64)); }
    if(_w != NULL)     { rde_memcpy(_w, fude_zoom_blob_bytes(_b) + sizeof(f64), sizeof(f64)); }
    if(_h != NULL)     { rde_memcpy(_h, fude_zoom_blob_bytes(_b) + 2u * sizeof(f64), sizeof(f64)); }
    rde_memcpy(&_st, fude_zoom_blob_bytes(_b) + 3u * sizeof(f64), sizeof(u32));
    if(_style != NULL) { *_style = (u8)_st; }
    if(_text != NULL)  { *_text = (const c8*)(fude_zoom_blob_bytes(_b) + FUDE_ZOOM_TEXT_HEAD); }
    if(_len != NULL)   { *_len = fude_zoom_blob_size(_b) - FUDE_ZOOM_TEXT_HEAD; }
    return true;
}

// See the header. Its box runs from its translation right and down.
void fude_zoom_scene_text_corners(const fude_zoom_scene* _s, u32 _object, fude_zoom_v2 _out[4]) {
    f64 _w = 0.0, _h = 0.0;
    fude_zoom_scene_text(_s, _object, NULL, &_w, &_h, NULL, NULL, NULL);
    const fude_zoom_sim _sim = fude_zoom_object_sim(&fude_zoom_objects(_s)[_object]);
    const fude_zoom_v2  _c[4] = { { 0.0, 0.0 }, { 0.0, -_h }, { _w, -_h }, { _w, 0.0 } };
    for(u32 _i = 0; _i < 4u; _i++) {
        _out[_i] = fude_zoom_sim_apply(_sim, _c[_i]);
    }
}

// See the header.
b8 fude_zoom_scene_image(const fude_zoom_scene* _s, u32 _object, f64* _hw, f64* _hh, const u8** _bytes, u32* _size) {
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(_o->kind != FUDE_ZOOM_KIND_IMAGE || _o->blob == 0) {
        return false;
    }
    const fude_zoom_blob* _b = &fude_zoom_blobs(_s)[_o->blob - 1u];
    if(fude_zoom_blob_size(_b) < 2u * sizeof(f64)) {
        return false;
    }
    if(_hw != NULL) {
        rde_memcpy(_hw, fude_zoom_blob_bytes(_b), sizeof(f64));
    }
    if(_hh != NULL) {
        rde_memcpy(_hh, fude_zoom_blob_bytes(_b) + sizeof(f64), sizeof(f64));
    }
    if(_bytes != NULL) {
        *_bytes = fude_zoom_blob_bytes(_b) + 2u * sizeof(f64);
    }
    if(_size != NULL) {
        *_size = fude_zoom_blob_size(_b) - 2u * (u32)sizeof(f64);
    }
    return true;
}

// See the header.
void fude_zoom_scene_image_corners(const fude_zoom_scene* _s, u32 _object, fude_zoom_v2 _out[4]) {
    f64 _hw = 0.0, _hh = 0.0;
    fude_zoom_scene_image(_s, _object, &_hw, &_hh, NULL, NULL);
    const fude_zoom_sim _sim = fude_zoom_object_sim(&fude_zoom_objects(_s)[_object]);
    const fude_zoom_v2  _c[4] = { { -_hw, -_hh }, { _hw, -_hh }, { _hw, _hh }, { -_hw, _hh } };
    for(u32 _i = 0; _i < 4u; _i++) {
        _out[_i] = fude_zoom_sim_apply(_sim, _c[_i]);
    }
}

// See the header. Numbers are the payload's doubles, as they are in memory.
u32 fude_zoom_scene_shape_numbers(const fude_zoom_scene* _s, u32 _object, f64* _out, u32 _max) {
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(_o->kind != FUDE_ZOOM_KIND_SHAPE || _o->blob == 0) {
        return 0;
    }
    const fude_zoom_blob* _b = &fude_zoom_blobs(_s)[_o->blob - 1u];
    u32 _n = fude_zoom_blob_size(_b) / (u32)sizeof(f64);
    _n = _n < _max ? _n : _max;
    rde_memcpy(_out, fude_zoom_blob_bytes(_b), (usize)_n * sizeof(f64));
    return _n;
}

// See the header.
u32 fude_zoom_scene_shape_numbers_all(const fude_zoom_scene* _s, u32 _object, rde_arr* _out) {
    rde_arr_clear(_out);
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(_o->kind != FUDE_ZOOM_KIND_SHAPE || _o->blob == 0) {
        return 0;
    }
    const fude_zoom_blob* _b = &fude_zoom_blobs(_s)[_o->blob - 1u];
    const u32 _n = fude_zoom_blob_size(_b) / (u32)sizeof(f64);
    if(_n > 0) {
        rde_memcpy(rde_arr_add_n(_out, _n), fude_zoom_blob_bytes(_b), (usize)_n * sizeof(f64));
    }
    return _n;
}

// Writes a shape's outline where it is, in its frame's units, into _out: cleared first (shape.h's outline clears it),
// then each point taken through the shape's own place.
void fude_zoom_scene_shape_outline(const fude_zoom_scene* _s, u32 _object, u32 _segments, rde_arr* _out, b8* _closed) {
    // All its numbers: a cut board's outline is past the usual few.
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

// See the header.
b8 fude_zoom_scene_object_outline(const fude_zoom_scene* _s, u32 _object, u32 _segments, rde_arr* _out, b8* _closed) {
    rde_arr_clear(_out);
    *_closed = false;
    if(_object >= fude_zoom_scene_object_count(_s)) {
        return false;
    }
    const fude_zoom_object _o = fude_zoom_objects(_s)[_object];
    if(!(_o.flags & FUDE_ZOOM_FLAG_ALIVE)) {
        return false;
    }
    if(_o.kind == FUDE_ZOOM_KIND_SHAPE) {
        const u8 _t = _o.channels;
        if(_t == FUDE_ZOOM_SHAPE_ARROW || _t == FUDE_ZOOM_SHAPE_DIMENSION || _t == FUDE_ZOOM_SHAPE_RADIAL || _t == FUDE_ZOOM_SHAPE_ANGLE ||
           _t == FUDE_ZOOM_SHAPE_GUIDE || fude_zoom_shape_is_attribute(_t)) {
            return false;   // A measure, a connector, drawing help, an attribute: no outline of their own.
        }
        fude_zoom_scene_shape_outline(_s, _object, _segments, _out, _closed);
    } else if(_o.kind == FUDE_ZOOM_KIND_FILL || _o.kind == FUDE_ZOOM_KIND_STROKE) {
        rde_arr _q = rde_arr_new(sizeof(fude_zoom_qpoint), rde_memory_allocator_get_default_std());
        if(_o.count > 0u) {
            rde_arr_add_n(&_q, _o.count);
        }
        if(_o.count == 0u || !fude_zoom_scene_points(_s, _object, (fude_zoom_qpoint*)_q.memory)) {
            rde_arr_free(&_q);
            return false;
        }
        // A fill: its first ring, its outline; a stroke: its line, closed when its ends are within two of its widths.
        const fude_zoom_qpoint* _p = (const fude_zoom_qpoint*)_q.memory;
        for(u32 _k = 0; _k < _o.count && (_o.kind == FUDE_ZOOM_KIND_STROKE || _p[_k].time == _p[0].time); _k++) {
            const fude_zoom_v2 _at = fude_zoom_scene_point_at(&_o, &_p[_k]);
            rde_arr_add(_out, (any)&_at);
        }
        rde_arr_free(&_q);
        const u32 _m = (u32)rde_arr_length(_out);
        if(_o.kind == FUDE_ZOOM_KIND_FILL) {
            *_closed = true;
        } else if(_m >= 2u) {
            const fude_zoom_v2* _p2 = (const fude_zoom_v2*)_out->memory;
            *_closed = rde_math_hypot_f64(_p2[0].x - _p2[_m - 1u].x, _p2[0].y - _p2[_m - 1u].y) <= (f64)_o.radius * _o.scale * 4.0;
        }
    } else {
        return false;
    }
    return rde_arr_length(_out) >= 2u;
}

// See the header. A FRAME object's change goes into its frame's record; any other's into an OP_ALIVE.
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

// See the header.
void fude_zoom_scene_discard(fude_zoom_scene* _s, u32 _object) {
    fude_zoom_scene_set_alive(_s, _object, false);
    fude_zoom_scene_collect(_s, _object);
}

// See the header. Every object is looked at once, in index order, so _died lists them as they were made.
u32 fude_zoom_scene_clear(fude_zoom_scene* _s, rde_arr* _died) {
    u32 _n = 0;
    for(u32 _i = 0; _i < fude_zoom_len(&_s->objects); _i++) {
        const fude_zoom_object* _o = &fude_zoom_objects(_s)[_i];
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind == FUDE_ZOOM_KIND_FRAME || _o->kind == FUDE_ZOOM_KIND_MARK || _o->kind == FUDE_ZOOM_KIND_LAYER ||
           !fude_zoom_scene_touchable(_s, _o)) {
            continue;
        }
        fude_zoom_scene_set_alive(_s, _i, false);
        rde_arr_add(_died, (any)&_i);
        _n++;
    }
    return _n;
}

// See the header.
b8 fude_zoom_scene_points(const fude_zoom_scene* _s, u32 _object, fude_zoom_qpoint* _out) {
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(_o->blob == 0) {
        return false;
    }
    const fude_zoom_blob* _b = &fude_zoom_blobs(_s)[_o->blob - 1u];
    return fude_zoom_codec_decode(fude_zoom_blob_bytes(_b), fude_zoom_blob_size(_b), _out, _o->count, _o->channels);
}

// See the header.
void fude_zoom_scene_query(const fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, rde_arr* _out) {
    const u32 _from = fude_zoom_len(_out);
    fude_zoom_index_query(&fude_zoom_frames(_s)[_frame].index, _box, _out);
    // Keep the alive ones, in place, once each: a moved object is in the index at its old place too, its own box
    // decides, and only one entry stays (sorted by index then, when any entries are stale: only a move leaves one,
    // and they go when the index is built again).
    u32* _v = fude_zoom_u32s(_out);
    u32  _w = _from;
    for(u32 _i = _from; _i < fude_zoom_len(_out); _i++) {
        const fude_zoom_object* _o = &fude_zoom_objects(_s)[_v[_i]];
        if(_o->flags & FUDE_ZOOM_FLAG_ALIVE && fude_zoom_box_overlaps(_o->box, _box)) {
            _v[_w++] = _v[_i];
        }
    }
    rde_arr_resize(_out, _w);
    if(fude_zoom_frames(_s)[_frame].stale > 0 && _w - _from > 1u) {
        rde_sort(&_v[_from], _w - _from, sizeof(u32), fude_zoom_by_u32);
        u32 _k = _from + 1u;
        for(u32 _i = _from + 1u; _i < _w; _i++) {
            if(_v[_i] != _v[_k - 1u]) {
                _v[_k++] = _v[_i];
            }
        }
        rde_arr_resize(_out, _k);
    }
}

// --- places: moving, scaling, turning ----------------------------------------------------

// See the header.
fude_zoom_place fude_zoom_scene_place_of(const fude_zoom_scene* _s, u32 _object) {
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    if(_o->kind == FUDE_ZOOM_KIND_FRAME) {
        const fude_zoom_xform* _x = &fude_zoom_frames(_s)[_o->child].xf;
        return (fude_zoom_place){ { _x->ox, _x->oy }, _x->rotation, _x->scale };
    }
    return (fude_zoom_place){ _o->t, _o->rotation, _o->scale };
}

// See the header.
fude_zoom_place fude_zoom_place_moved(fude_zoom_place _p, fude_zoom_sim _m) {
    const fude_zoom_sim _was = { _p.scale * rde_math_cos_f64(_p.rotation), _p.scale * rde_math_sin_f64(_p.rotation), _p.t.x, _p.t.y };
    const fude_zoom_sim _now = fude_zoom_sim_compose(_m, _was);
    return (fude_zoom_place){ { _now.tx, _now.ty }, rde_math_atan2_f64(_now.b, _now.a), rde_math_hypot_f64(_now.a, _now.b) };
}

// Returns a shape's box: its outline where it is (round curves cut in 96), grown by its half-width.
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

// Sets an object's box again from where it is now: a shape's from its outline, a picture's or a text's from its
// corners, a stroke's or a fill's from its points.
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
    rde_arr TYPE(fude_zoom_qpoint) _q = rde_arr_new(sizeof(fude_zoom_qpoint), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_q, _o->count);
    if(fude_zoom_scene_points(_s, _object, (fude_zoom_qpoint*)_q.memory)) {
        _o = &fude_zoom_objects(_s)[_object];
        _o->box = fude_zoom_stroke_box(_o, (const fude_zoom_qpoint*)_q.memory, _o->count);
    }
    rde_arr_free(&_q);
}

// See the header. A FRAME object's frame moves in its parent (its anchor moved with it, its record journaled); any
// other object is boxed again, put into the index again at its new place (not while a file loads: the index is built
// at the end) and journaled as an OP_PLACE.
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
    if(!_s->loading) {
        // In again at its new place: its old entry is stale now.
        fude_zoom_scene_index_add(_s, _object);
        fude_zoom_frames(_s)[_o->frame].stale++;
    }
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

// Makes a copy of _object (its points shared, not copied) in _frame at _place: on top (_z 0) or at _z, on _layer, its
// own gesture (_gesture 0) or a later piece of _gesture. Returns its index, or FUDE_ZOOM_NONE for a frame or an object
// with no payload.
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
    // Placed with the original's box: its own where it is worked out, then into the index once.
    const u32 _index = fude_zoom_scene_put_in(_s, &_o, false);
    fude_zoom_scene_rebox(_s, _index);
    fude_zoom_scene_index_add(_s, _index);
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

// See the header.
u32 fude_zoom_scene_copy_stroke(fude_zoom_scene* _s, u32 _object, u32 _frame, fude_zoom_place _place) {
    return fude_zoom_scene_copy_as(_s, _object, _frame, _place, 0, fude_zoom_objects(_s)[_object].layer, 0);
}

// See the header.
u32 fude_zoom_scene_copy_to_layer(fude_zoom_scene* _s, u32 _object, u16 _layer, fude_zoom_id _gesture) {
    const fude_zoom_object* _o = &fude_zoom_objects(_s)[_object];
    return fude_zoom_scene_copy_as(_s, _object, _o->frame, fude_zoom_scene_place_of(_s, _object), _o->z, _layer, _gesture);
}

// --- the camera --------------------------------------------------------------------------

// See the header.
fude_zoom_v2 fude_zoom_camera_to_frame(const fude_zoom_camera* _c, fude_zoom_v2 _screen) {
    return (fude_zoom_v2){ _c->at.x + _screen.x / _c->z, _c->at.y + _screen.y / _c->z };
}

// See the header.
fude_zoom_v2 fude_zoom_camera_to_screen(const fude_zoom_camera* _c, fude_zoom_v2 _p) {
    return (fude_zoom_v2){ (_p.x - _c->at.x) * _c->z, (_p.y - _c->at.y) * _c->z };
}

// See the header.
void fude_zoom_camera_zoom_at(fude_zoom_scene* _s, fude_zoom_v2 _screen, f64 _factor) {
    // The frame point under _screen stays under it.
    fude_zoom_camera* _c     = &_s->camera;
    const fude_zoom_v2 _held = fude_zoom_camera_to_frame(_c, _screen);
    _c->z  *= _factor;
    _c->at  = (fude_zoom_v2){ _held.x - _screen.x / _c->z, _held.y - _screen.y / _c->z };
}

// See the header.
void fude_zoom_camera_pan(fude_zoom_scene* _s, fude_zoom_v2 _delta) {
    _s->camera.at.x -= _delta.x / _s->camera.z;
    _s->camera.at.y -= _delta.y / _s->camera.z;
}

// Moves the camera into frame _to, its point and zoom carried over, and marks _to as the latest visited.
RDE_INTERNAL void fude_zoom_camera_move(fude_zoom_scene* _s, u32 _to) {
    fude_zoom_camera*   _c   = &_s->camera;
    const fude_zoom_sim _sim = fude_zoom_scene_sim(_s, _c->frame, _to);
    _c->at    = fude_zoom_sim_apply(_sim, _c->at);
    _c->z    /= fude_zoom_sim_scale(_sim);
    _c->frame = _to;
    fude_zoom_frames(_s)[_to].visited = ++_s->clock;
}

// Drops a frame made for the camera and left with nothing in it: removed, its FRAME object dead. Nothing for a root, a
// frame in the file, or one in use.
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

// Returns the child of _frame the camera goes into at point _at (frame units): the most recently visited alive one
// whose anchor holds it, and of those visited alike (every one, in a canvas just opened) the first in the frame's
// order; FUDE_ZOOM_NONE when none does. It walks the frame's children (kids), not all its objects: the camera settles
// every frame, and looks here each time it is panned outside its own frame's anchor. The first in the order is the one
// with the lowest key and, of equal keys, the first listed: kids and the order take each FRAME object in as it is
// made, the order putting it after every key at most its own.
RDE_INTERNAL u32 fude_zoom_camera_child_at(const fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _at) {
    const fude_zoom_frame* _f      = &fude_zoom_frames(_s)[_frame];
    u32                    _best   = FUDE_ZOOM_NONE;
    u64                    _best_z = 0;
    for(u32 _i = 0; _i < fude_zoom_len(&_f->kids); _i++) {
        const fude_zoom_object* _o = &fude_zoom_objects(_s)[fude_zoom_u32s(&_f->kids)[_i]];
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
            continue;
        }
        const fude_zoom_frame* _c = &fude_zoom_frames(_s)[_o->child];
        if(!fude_zoom_box_contains(_c->anchor, _at)) {
            continue;
        }
        const u32 _visited = _best != FUDE_ZOOM_NONE ? fude_zoom_frames(_s)[_best].visited : 0u;
        if(_best == FUDE_ZOOM_NONE || _c->visited > _visited || (_c->visited == _visited && _o->z < _best_z)) {
            _best   = _o->child;
            _best_z = _o->z;
        }
    }
    return _best;
}

// See the header. At most 256 frames are stepped through at a time. An empty canvas does not zoom out past its root:
// there is nothing out there to see.
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
                    _c->z = FUDE_ZOOM_Z_MIN;   // An empty canvas: nothing out there to see.
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

    // Panned out of its frame's anchor onto a sibling's: handed over, so new strokes go where their neighbours are.
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

// See the header.
fude_zoom_sim fude_zoom_camera_sim(const fude_zoom_camera* _c) {
    return (fude_zoom_sim){ _c->z, 0.0, -_c->at.x * _c->z, -_c->at.y * _c->z };
}

// See the header. The frame left is dropped when it was made for the camera and nothing is in it.
void fude_zoom_camera_look_at(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _at, f64 _z) {
    const u32 _left = _s->camera.frame;
    _s->camera = (fude_zoom_camera){ _frame, _at, _z };
    fude_zoom_frames(_s)[_frame].visited = ++_s->clock;
    if(_left != _frame) {
        fude_zoom_scene_drop_if_unused(_s, _left);
    }
}

// Returns whether anything alive drawn after frame _child in its parent overlaps _box (parent units), where a new frame
// made on top would be drawn over it. Drawn after is by draw key (fude_zoom_scene_draw_key), as the renderer sorts: so
// what a hidden layer holds (drawn nowhere) does not count, and of the rest only what is on a layer of the frame's rank
// and later in the order, since a new frame on top gets that rank and the next order key. What a layer ranked above
// holds is drawn over any frame there, a new one too, and what one ranked below holds is drawn under this one.
RDE_INTERNAL b8 fude_zoom_scene_covered(fude_zoom_scene* _s, u32 _child, fude_zoom_box _box) {
    const fude_zoom_frame* _c = &fude_zoom_frames(_s)[_child];
    if(_c->parent == FUDE_ZOOM_NONE || _c->object == FUDE_ZOOM_NONE) {
        return false;
    }
    const fude_zoom_object* _frame = &fude_zoom_objects(_s)[_c->object];
    const u64               _slot  = _frame->z;
    const u64               _rank  = fude_zoom_scene_draw_key(_s, _frame) >> 56;
    const fude_zoom_frame*  _p     = &fude_zoom_frames(_s)[_c->parent];
    for(u32 _i = fude_zoom_order_slot(_s, _p, _slot); _i < fude_zoom_len(&_p->order); _i++) {
        const fude_zoom_object* _o = &fude_zoom_objects(_s)[fude_zoom_u32s(&_p->order)[_i]];
        if(_o->z > _slot && _o->flags & FUDE_ZOOM_FLAG_ALIVE && !fude_zoom_scene_hides(_s, _o) && (fude_zoom_scene_draw_key(_s, _o) >> 56) == _rank &&
           fude_zoom_box_overlaps(_o->box, _box)) {
            return true;
        }
    }
    return false;
}

// See the header. Only the three levels up that the renderer draws exactly are looked at.
u32 fude_zoom_camera_drawing_frame(fude_zoom_scene* _s, fude_zoom_box _box) {
    // The levels the renderer draws exactly (three up): the highest whose place in its parent is covered by something
    // later.
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
    // A new frame on top at that level, and one inside it for each level below, down to the camera's depth: each at the
    // camera (its origin the camera's point, on the paper's lattice), at the same scale as the one it stands in for.
    // Each anchor is the stroke's box in the new parent's units: the old parent's, moved by where the camera is in
    // each.
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
