// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/scene_parts.h"

// ===========================================================================
// See scene.h. The scene's history: undo and redo. Each action names the objects it took away, made and
// moved (their places before and after), and keeps them alive while it does; what the history holds is kept under its
// budget (the oldest actions dropped, never the one just done), what was undone is dropped when something new is done,
// and each push, drop, and undo or redo is journaled.
//
// In order: what an action keeps alive and lets go; the undone actions and the oldest dropped; an action appended and
// pushed (tagged, with what moved); undo and redo, and what each would do next.
//
// Part of the scene: scene_parts.h holds what its modules share and says which module holds what. Main thread only.
// ===========================================================================

// --- the history -------------------------------------------------------------------------

// Returns the bytes an action keeps alive for these objects: each one's payload and its record.
RDE_INTERNAL u32 fude_zoom_action_bytes(const fude_zoom_scene* _s, const u32* _ids, u32 _count) {
    u32 _bytes = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        const fude_zoom_object* _o = &fude_zoom_objects(_s)[_ids[_i]];
        if(_o->blob != 0) {
            _bytes += fude_zoom_blob_size(&fude_zoom_blobs(_s)[_o->blob - 1u]) + (u32)sizeof(fude_zoom_object);
        }
    }
    return _bytes;
}

// The objects an action names lose that name; the ones nothing keeps any more become garbage.
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

// Drops the actions past action_count (the undone ones), and the targets and places only they used.
RDE_INTERNAL void fude_zoom_history_drop_redo(fude_zoom_scene* _s) {
    while(fude_zoom_len(&_s->actions) > _s->action_count) {
        const fude_zoom_action _a = fude_zoom_actions(_s)[fude_zoom_len(&_s->actions) - 1u];
        rde_arr_remove(&_s->actions, fude_zoom_len(&_s->actions) - 1u);
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
    if(_end < fude_zoom_len(&_s->targets)) {
        rde_arr_resize(&_s->targets, _end);
    }
    if(_places < fude_zoom_len(&_s->places)) {
        rde_arr_resize(&_s->places, _places);
    }
}

// Drops the oldest _n actions (over the budget, or as the journal says): their objects forgotten, the targets and
// places moved to the front, and the rest renumbered. Journaled as an OP_DROP.
void fude_zoom_history_drop_oldest(fude_zoom_scene* _s, u32 _n) {
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
    const u32 _cut  = _n < fude_zoom_len(&_s->actions) ? _next->first : fude_zoom_len(&_s->targets);
    const u32 _pcut = _n < fude_zoom_len(&_s->actions) ? _next->place_first : fude_zoom_len(&_s->places);
    u32* _t = fude_zoom_targets(_s);
    rde_memmove(_t, _t + _cut, (usize)(fude_zoom_len(&_s->targets) - _cut) * sizeof(u32));
    rde_arr_resize(&_s->targets, fude_zoom_len(&_s->targets) - _cut);
    fude_zoom_place* _pl = fude_zoom_places(_s);
    rde_memmove(_pl, _pl + _pcut, (usize)(fude_zoom_len(&_s->places) - _pcut) * sizeof(fude_zoom_place));
    rde_arr_resize(&_s->places, fude_zoom_len(&_s->places) - _pcut);
    fude_zoom_action* _acts = fude_zoom_actions(_s);
    rde_memmove(_acts, _acts + _n, (usize)(fude_zoom_len(&_s->actions) - _n) * sizeof(fude_zoom_action));
    rde_arr_resize(&_s->actions, fude_zoom_len(&_s->actions) - _n);
    _s->action_count    -= _n;
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

// Appends an action as it is (its flags are already what it did), dropping what was undone first: its type from what
// it names (MOVED when anything moved), each object it names counted, and its bytes against the budget. Not
// journaled: the callers do that.
void fude_zoom_history_append(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, const u32* _died, u32 _died_count, const u32* _born, u32 _born_count,
                              const u32* _moved, const fude_zoom_place* _before, const fude_zoom_place* _after, u32 _moved_count, u32 _tag) {
    fude_zoom_history_drop_redo(_s);
    fude_zoom_action _a = { .frame = _frame, .box = _box, .tag = _tag };
    _a.type  = _died_count == 0 ? FUDE_ZOOM_ACTION_BORN : (_born_count == 0 ? FUDE_ZOOM_ACTION_DIED : FUDE_ZOOM_ACTION_REPLACED);
    _a.first = fude_zoom_len(&_s->targets);
    _a.count = _died_count;
    if(_died_count > 0) {
        rde_memcpy(rde_arr_add_n(&_s->targets, _died_count), _died, (usize)_died_count * sizeof(u32));
    }
    _a.born_first = fude_zoom_len(&_s->targets);
    _a.born       = _born_count;
    if(_born_count > 0) {
        rde_memcpy(rde_arr_add_n(&_s->targets, _born_count), _born, (usize)_born_count * sizeof(u32));
    }
    _a.moved_first = fude_zoom_len(&_s->targets);
    _a.moved       = _moved_count;
    _a.place_first = fude_zoom_len(&_s->places);
    if(_moved_count > 0) {
        rde_memcpy(rde_arr_add_n(&_s->targets, _moved_count), _moved, (usize)_moved_count * sizeof(u32));
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

// See the header. Journaled as an OP_PUSH; then, over the budget, the oldest steps go, never the one just done.
void fude_zoom_history_push_tagged(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, const u32* _died, u32 _died_count, const u32* _born, u32 _born_count,
                                   const u32* _moved, const fude_zoom_place* _before, const fude_zoom_place* _after, u32 _moved_count, u32 _tag) {
    if(_died_count == 0 && _born_count == 0 && _moved_count == 0 && _tag == 0u) {
        return;
    }
    fude_zoom_history_append(_s, _frame, _box, _died, _died_count, _born, _born_count, _moved, _before, _after, _moved_count, _tag);
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

// See the header.
void fude_zoom_history_push_all(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, const u32* _died, u32 _died_count, const u32* _born, u32 _born_count,
                                const u32* _moved, const fude_zoom_place* _before, const fude_zoom_place* _after, u32 _moved_count) {
    fude_zoom_history_push_tagged(_s, _frame, _box, _died, _died_count, _born, _born_count, _moved, _before, _after, _moved_count, 0u);
}

// See the header.
void fude_zoom_history_push(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, const u32* _died, u32 _died_count, const u32* _born, u32 _born_count) {
    fude_zoom_history_push_all(_s, _frame, _box, _died, _died_count, _born, _born_count, NULL, NULL, NULL, 0);
}

// See the header.
void fude_zoom_history_push_moved(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, const u32* _objects, const fude_zoom_place* _before, const fude_zoom_place* _after, u32 _count) {
    fude_zoom_history_push_all(_s, _frame, _box, NULL, 0, NULL, 0, _objects, _before, _after, _count);
}

// Journals the action count after an undo or redo (OP_COUNT). Not while replaying.
RDE_INTERNAL void fude_zoom_history_journal_count(fude_zoom_scene* _s) {
    if(_s->replaying) {
        return;
    }
    u32 _at;
    fude_zoom_journal_op_begin(_s, FUDE_ZOOM_OP_COUNT, &_at);
    fude_put_u32(&_s->journal, _s->action_count);
    fude_zoom_journal_op_end(_s, _at);
}

// See the header. What the step made dies, what it erased lives again, what it moved goes back.
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

// See the header.
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

// See the header.
b8 fude_zoom_history_can_undo(const fude_zoom_scene* _s) {
    return _s->action_count > 0;
}

// See the header.
b8 fude_zoom_history_can_redo(const fude_zoom_scene* _s) {
    return _s->action_count < fude_zoom_len(&_s->actions);
}

// See the header.
const fude_zoom_action* fude_zoom_history_next_undo(const fude_zoom_scene* _s) {
    return _s->action_count > 0 ? &fude_zoom_actions(_s)[_s->action_count - 1u] : NULL;
}

// See the header.
const fude_zoom_action* fude_zoom_history_next_redo(const fude_zoom_scene* _s) {
    return _s->action_count < fude_zoom_len(&_s->actions) ? &fude_zoom_actions(_s)[_s->action_count] : NULL;
}
