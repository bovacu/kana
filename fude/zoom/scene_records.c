// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/scene_parts.h"

// ===========================================================================
// See scene.h. The scene's records, as the file and the journal keep them: a frame, an object and an action
// written (fude_zoom_put_*) and read (fude_zoom_get_*), their layout in the comment below; a journal's operations
// applied (fude_zoom_apply_ops); and a scene made whole once it is loaded (fude_zoom_scene_loaded).
//
// Part of the scene: scene_parts.h holds what its modules share and says which module holds what. Main thread only.
// ===========================================================================

// --- records -----------------------------------------------------------------------------
//
// FRAME   u64 id, u64 parent id (0: the root), f64 ox oy scale rotation,
//         f64 anchor min_x min_y max_x max_y, u64 its order key in the parent,
//         u8 removed, u8 home (1: the canvas's home frame; a record without it is not home)
// OBJECT  u64 id, u64 gesture, u64 frame id, u8 kind flags channels, i8 q,
//         u16 layer, u64 z, f64 tx ty, f64 box ×4, u8 r g b a, f32 radius,
//         u32 count, u32 points' size, the points (none for garbage),
//         f64 rotation, f64 scale (a record without them: 0 and 1),
//         u8 r g b a fill (a record without it: none)
// ACTION  u64 frame id, f64 box ×4, u32 died, their ids, u32 born, their ids,
//         u32 moved, per one its id and f64 tx ty rotation scale before, then after,
//         u32 tag (0.1.94; a record without it: 0)
// New fields go at the END of a record; readers take records by their size
// where they come in arrays (zfile.h).


// See the header and the layout above.
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
    fude_put_u8(_b, _frame == _s->home ? 1u : 0u);   // From before it, none says so: the root is home.
}

// See the header and the layout above. A garbage object's points are not written.
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
        fude_put_u32(_b, fude_zoom_blob_size(_bl));
        fude_put_data(_b, fude_zoom_blob_bytes(_bl), fude_zoom_blob_size(_bl));
    }
    fude_put_f64(_b, _o->rotation);
    fude_put_f64(_b, _o->scale);
    fude_put_color(_b, _o->fill);   // From before it: none.
}

// Writes a place: f64 tx ty rotation scale.
RDE_INTERNAL void fude_zoom_put_place(fude_bytes* _b, fude_zoom_place _p) {
    fude_put_f64(_b, _p.t.x);
    fude_put_f64(_b, _p.t.y);
    fude_put_f64(_b, _p.rotation);
    fude_put_f64(_b, _p.scale);
}

// Reads a place written by fude_zoom_put_place.
RDE_INTERNAL fude_zoom_place fude_zoom_get_place(fude_reader* _r) {
    fude_zoom_place _p;
    _p.t.x      = fude_get_f64(_r);
    _p.t.y      = fude_get_f64(_r);
    _p.rotation = fude_get_f64(_r);
    _p.scale    = fude_get_f64(_r);
    return _p;
}

// See the header and the layout above.
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
    fude_put_u32(_b, _a->tag);   // 0.1.94: at the record's end, read only where it is there.
}

// See the header. Its parent must be read first; a frame that is there already (by id) is updated in place. A frame
// moved under another (a new root above it) gets a new FRAME object, the old one left dead.
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
        return false;   // Parents come first: one that does not is not a frame of this canvas.
    }

    u32 _slot = fude_zoom_scene_find_frame(_s, _id);
    if(_slot == FUDE_ZOOM_NONE) {
        fude_zoom_frame* _f = rde_arr_add(&_s->frames, NULL);
        fude_zoom_frame_init(_f);
        _f->id = _id;
        _slot  = fude_zoom_len(&_s->frames) - 1u;
        fude_zoom_ids_set(&_s->frame_ids, _id, _slot);
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
        // Moved under another (a new root above it): its old FRAME object, if any, is left dead in the old parent's
        // list; a new one in the new parent.
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

// See the header. Only the kinds this build knows are taken. An object whose id is there already is skipped (a later
// copy of what was loaded: the first stands); one with no points is garbage.
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
        return true;   // Already here (a later copy of what was loaded): the first stands.
    }
    if(_size == 0) {
        _o.flags = (u8)((_o.flags | FUDE_ZOOM_FLAG_GARBAGE) & ~FUDE_ZOOM_FLAG_ALIVE);
    } else {
        _o.blob = fude_zoom_blob_new(_s, _points, _size);
    }
    const u32 _index = fude_zoom_scene_put_in(_s, &_o, true);
    if(fude_zoom_objects(_s)[_index].flags & FUDE_ZOOM_FLAG_GARBAGE) {
        fude_zoom_frames(_s)[_o.frame].dead++;
    }
    fude_zoom_scene_touch(_s, &fude_zoom_objects(_s)[_index]);
    return true;
}

// See the header. An action naming an object or a frame the file does not have is not appended, and false says the
// history stops there.
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
                _ok = false;   // An action over something the file does not have: the history stops here.
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
    const u32 _tag   = _ok && fude_reader_has(_r, 4u) ? fude_get_u32(_r) : 0u;   // A step kept before 0.1.94: none.
    const u32 _frame = fude_zoom_scene_find_frame(_s, _frame_id);
    if(_ok && _frame != FUDE_ZOOM_NONE) {
        fude_zoom_history_append(_s, _frame, _box, fude_zoom_u32s(&_died), fude_zoom_len(&_died), fude_zoom_u32s(&_born), fude_zoom_len(&_born),
                                 fude_zoom_u32s(&_moved), (const fude_zoom_place*)_before.memory, (const fude_zoom_place*)_after.memory, fude_zoom_len(&_moved), _tag);
    }
    rde_arr_free(&_died);
    rde_arr_free(&_born);
    rde_arr_free(&_moved);
    rde_arr_free(&_before);
    rde_arr_free(&_after);
    return _ok && _frame != FUDE_ZOOM_NONE;
}

// See the header. Each op is read from a reader of its own size, so an op longer than this build reads is skipped
// past whole.
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
            default: break;   // An op of a newer build: skipped.
        }
        if(!_ok) {
            return false;
        }
    }
    return _r->ok;
}

// See the header. While the file was read (scene.h, loading), each frame's order took its objects as they came and its
// index took none: each order is sorted and each index built, packed, once, here.
void fude_zoom_scene_loaded(fude_zoom_scene* _s) {
    // What no undo step can bring back is garbage now.
    for(u32 _i = 0; _i < fude_zoom_len(&_s->objects); _i++) {
        fude_zoom_scene_collect(_s, _i);
    }
    for(u32 _f = 0; _f < fude_zoom_len(&_s->frames); _f++) {
        if(_s->loading) {
            fude_zoom_order_settle(_s, _f);
        }
        fude_zoom_scene_index_build(_s, _f);
    }
    _s->loading = false;
    rde_arr_clear(&_s->journal);
    // A file from before frames said which is home: the root is.
    if(_s->home == FUDE_ZOOM_NONE || _s->home >= fude_zoom_len(&_s->frames) || fude_zoom_frames(_s)[_s->home].removed) {
        _s->home = _s->root;
    }
}
