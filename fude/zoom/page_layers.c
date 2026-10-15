// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"

// ===========================================================================
// Page, layers (scene.h): their panel, and what the lasso holds moved to another layer.
//
// The panel's rows are found again from the scene each time they are shown (fude_zoom_page_find_layers). A layer's
// name, flags and place in the pile are kept in a LAYER object, so every change to one is a new LAYER object in place
// of its last, pushed as an undo step.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// --- layers (scene.h) ------------------------------------------------------------------------

// Returns row _row's place in the pile (higher: over): the rank its flags keep, else its index.
RDE_INTERNAL i32 fude_zoom_page_layer_rank(const fude_zoom_page* _page, u32 _row) {
    const i32 _r = fude_zoom_layer_rank_of(_page->layer_flags[_row]);
    return _r >= 0 ? _r : (i32)_page->layer_index[_row];
}

// The layers as the panel lists them, into the page's rows (at most FUDE_ZOOM_PAGE_LAYER_ROWS): each layer a LAYER
// object names (the latest for an index) or that holds something, and layer 0 unless it is neither while others are
// listed; how many things each holds into _counts (may be NULL). Frames, marks and a stroke's continuations are not
// counted. Sorted as they lie, the top one first.
void fude_zoom_page_find_layers(fude_zoom_page* _page, u32* _counts) {
    fude_zoom_scene* _s = &_page->scene;
    u64 _z[FUDE_ZOOM_LAYERS];
    u32 _obj[FUDE_ZOOM_LAYERS];
    u8  _flags[FUDE_ZOOM_LAYERS];
    b8  _named[FUDE_ZOOM_LAYERS];
    rde_memset(_z, 0, sizeof(_z));
    rde_memset(_named, 0, sizeof(_named));
    for(u32 _i = 0; _i < FUDE_ZOOM_LAYERS; _i++) {
        _obj[_i] = FUDE_ZOOM_NONE;
        _flags[_i] = 0;
    }
    u32 _held[FUDE_ZOOM_LAYERS];
    rde_memset(_held, 0, sizeof(_held));
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
            continue;
        }
        u16 _index;
        u8  _f;
        if(_o->kind == FUDE_ZOOM_KIND_LAYER) {
            if(fude_zoom_scene_layer_of(_s, _i, &_index, &_f, NULL, 0) && _index < FUDE_ZOOM_LAYERS && _o->z >= _z[_index]) {
                _z[_index] = _o->z; _obj[_index] = _i; _flags[_index] = _f; _named[_index] = true;
            }
        } else if(_o->kind != FUDE_ZOOM_KIND_FRAME && _o->kind != FUDE_ZOOM_KIND_MARK && _o->layer < FUDE_ZOOM_LAYERS && !(_o->flags & FUDE_ZOOM_FLAG_CONTINUES)) {
            _held[_o->layer]++;
        }
    }
    _page->layer_count = 0;
    // Layer 0 is there unless deleted (neither named nor holding anything) with others listed.
    b8 _others = false;
    for(u32 _l = 1; _l < FUDE_ZOOM_LAYERS; _l++) {
        _others = _others || _named[_l] || _held[_l] > 0;
    }
    for(u32 _l = 0; _l < FUDE_ZOOM_LAYERS && _page->layer_count < FUDE_ZOOM_PAGE_LAYER_ROWS; _l++) {
        if(!_named[_l] && _held[_l] == 0 && (_l != 0 || _others)) {
            continue;
        }
        const u32 _row = _page->layer_count++;
        _page->layer_index[_row]  = (u16)_l;
        _page->layer_flags[_row]  = _flags[_l];
        _page->layer_object[_row] = _obj[_l];
        _page->layer_names[_row][0] = 0;
        if(_obj[_l] != FUDE_ZOOM_NONE) {
            fude_zoom_scene_layer_of(_s, _obj[_l], NULL, NULL, _page->layer_names[_row], sizeof(_page->layer_names[_row]));
        }
        if(_page->layer_names[_row][0] == 0) {
            FUDE_TEXTF(_page->layer_names[_row], FUDE_TEXT_ZOOM_LAYER_N, FUDE_TN(_l + 1u));
        }
        if(_counts != NULL) {
            _counts[_row] = _held[_l];
        }
    }
    // Listed as they lie: the top one first (each one's rank: as its flags keep it, or its index).
    for(u32 _i = 1; _i < _page->layer_count; _i++) {
        for(u32 _j = _i; _j > 0u && fude_zoom_page_layer_rank(_page, _j) > fude_zoom_page_layer_rank(_page, _j - 1u); _j--) {
            const u16 _ix = _page->layer_index[_j];  _page->layer_index[_j]  = _page->layer_index[_j - 1u];  _page->layer_index[_j - 1u]  = _ix;
            const u8  _fl = _page->layer_flags[_j];  _page->layer_flags[_j]  = _page->layer_flags[_j - 1u];  _page->layer_flags[_j - 1u]  = _fl;
            const u32 _ob = _page->layer_object[_j]; _page->layer_object[_j] = _page->layer_object[_j - 1u]; _page->layer_object[_j - 1u] = _ob;
            c8 _nm[sizeof(_page->layer_names[0])];
            rde_memcpy(_nm, _page->layer_names[_j], sizeof(_nm));
            rde_memcpy(_page->layer_names[_j], _page->layer_names[_j - 1u], sizeof(_nm));
            rde_memcpy(_page->layer_names[_j - 1u], _nm, sizeof(_nm));
            if(_counts != NULL) {
                const u32 _c = _counts[_j]; _counts[_j] = _counts[_j - 1u]; _counts[_j - 1u] = _c;
            }
        }
    }
}

// The rows' words and looks, as things are now.
void fude_zoom_page_show_layers(fude_zoom_page* _page) {
    if(_page->layer_add == NULL) {
        return;
    }
    u32 _counts[FUDE_ZOOM_PAGE_LAYER_ROWS];
    fude_zoom_page_find_layers(_page, _counts);
    const fude_theme* _t = fude_theme_active();
    for(u32 _i = 0; _i < _page->layer_count; _i++) {
        c8 _said[64];
        rde_snprintf(_said, sizeof(_said), "%s \xC2\xB7 %u", _page->layer_names[_i], _counts[_i]);
        rde_ui_button_set_text(_page->layer_rows[_i], _said);
        const b8 _current = _page->layer_index[_i] == _page->scene.layer;
        fude_kit_button_colors(_page->layer_rows[_i], _current ? _t->tint : _t->surface, _current ? 2.0f : 1.0f, _current ? _t->accent : _t->outline);
        fude_kit_icon(_page->layer_eyes[_i], (_page->layer_flags[_i] & FUDE_ZOOM_LAYER_HIDDEN) ? FUDE_ICON_HIDE : FUDE_ICON_SHOW, FUDE_KIT_ICON_ONLY, 17.0f);
        if(_page->layer_flags[_i] & FUDE_ZOOM_LAYER_LOCKED) { fude_kit_button_selected(_page->layer_locks[_i]); }
        else                                                { fude_kit_button_quiet(_page->layer_locks[_i]); }
        // The top one goes no higher, the bottom one no lower.
        rde_ui_node_set_interactable(rde_ui_button_as_node(_page->layer_ups[_i]), _i > 0u);
        rde_ui_node_set_interactable(rde_ui_button_as_node(_page->layer_downs[_i]), _i + 1u < _page->layer_count);
        fude_kit_button_colors(_page->layer_ups[_i], _t->surface, 1.0f, _i > 0u ? _t->outline : _t->surface);
        fude_kit_button_colors(_page->layer_downs[_i], _t->surface, 1.0f, _i + 1u < _page->layer_count ? _t->outline : _t->surface);
    }
    _page->layers_seen = _page->scene.revision;
    _page->layers_dirty = true;   // (rows shown again for how many there are)
}

// A layer's flags or name changed: a new LAYER object for it in place of its last (one undo step).
void fude_zoom_page_layer_set(fude_zoom_page* _page, u32 _row, u8 _flags, const c8* _name) {
    fude_zoom_scene* _s    = &_page->scene;
    const u32        _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    const u32 _old = _page->layer_object[_row];
    const u32 _new = fude_zoom_scene_add_layer(_s, _home, _page->layer_index[_row], _flags, _name);
    if(_old != FUDE_ZOOM_NONE) {
        fude_zoom_scene_set_alive(_s, _old, false);
    }
    fude_zoom_history_push(_s, _home, (fude_zoom_box){ 0.0, 0.0, 0.0, 0.0 }, _old != FUDE_ZOOM_NONE ? &_old : NULL, _old != FUDE_ZOOM_NONE ? 1u : 0u, &_new, 1u);
    fude_zoom_scene_layers_refresh(_s);
    fude_zoom_page_show_layers(_page);
}

// Its flags changed, its name kept (a layer never named keeps the name it is given in the app's language).
RDE_INTERNAL void fude_zoom_page_layer_flags(fude_zoom_page* _page, u32 _row, u8 _flags) {
    fude_zoom_page_layer_set(_page, _row, _flags, _page->layer_object[_row] != FUDE_ZOOM_NONE ? _page->layer_names[_row] : "");
}

// The lasso's To layer done: what it holds moved onto the row's layer, the panel put away.
RDE_INTERNAL void fude_zoom_page_move_to_layer(fude_zoom_page* _page, u32 _row) {
    _page->layer_moving = false;
    if(_row >= _page->layer_count) {
        return;
    }
    if(fude_zoom_select_to_layer(&_page->selection, &_page->scene, _page->layer_index[_row]) > 0) {
        c8 _say[96];
        FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_MOVED_TO, FUDE_TS(_page->layer_names[_row]));
        fude_notice_show(_say);
    }
    _page->layers_open = false;
}

// A layer's row tapped (_user_data: the row). While To layer waits, what the lasso holds goes onto it; the layer
// drawn on tapped again brings up the text card to rename it; any other becomes the layer drawn on, with a tip on
// renaming the first two times.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_layer(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    static u32 _tips = 0;
    fude_zoom_page* _page = fude_zoom_page_the;
    const u32 _row = (u32)(uintptr_t)_user_data;
    if(_page == NULL || _row >= _page->layer_count) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    if(_page->layer_moving && rde_arr_length(&_page->selection.picks) > 0) {
        fude_zoom_page_move_to_layer(_page, _row);
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    _page->layer_moving = false;   // (the selection let go meanwhile)
    if(_page->layer_index[_row] == _page->scene.layer && _page->text_field != NULL) {
        // The chosen one again: its name on the card, to change.
        const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
        if(_bytes > 0) {
            rde_ui_text_editor_delete_range(_page->text_field, 0, _bytes);
        }
        rde_ui_text_editor_insert_at(_page->text_field, 0, _page->layer_names[_row], rde_strlen(_page->layer_names[_row]));
        rde_ui_text_editor_select_all(_page->text_field);
        rde_ui_text_editor_set_placeholder(_page->text_field, fude_text(FUDE_TEXT_ZOOM_LAYER_RENAME));
        _page->text_object    = FUDE_ZOOM_NONE;
        _page->text_style     = FUDE_ZOOM_PAGE_TEXT_RENAME;
        _page->layer_renaming = _row;
        _page->layers_open    = false;   // (the card over everything; the panel back after Save)
        fude_zoom_page_text_card(_page, true);
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    _page->scene.layer = _page->layer_index[_row];   // new things go on it
    fude_zoom_page_show_layers(_page);
    if(_tips < 2u) {
        _tips++;
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_LAYER_RENAME_TIP));
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A layer's eye: hidden or shown. Hiding the layer drawn on moves drawing to the first that shows; when none does,
// it stays shown and a notice says so.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_layer_eye(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_page* _page = fude_zoom_page_the;
    const u32 _row = (u32)(uintptr_t)_user_data;
    if(_page == NULL || _row >= _page->layer_count) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    const b8 _hide = !(_page->layer_flags[_row] & FUDE_ZOOM_LAYER_HIDDEN);
    if(_hide && _page->layer_index[_row] == _page->scene.layer) {
        // Hiding the one drawn on: new things go on the first that shows.
        for(u32 _i = 0; _i < _page->layer_count; _i++) {
            if(_i != _row && !(_page->layer_flags[_i] & FUDE_ZOOM_LAYER_HIDDEN)) {
                _page->scene.layer = _page->layer_index[_i];
                break;
            }
        }
        if(_page->layer_index[_row] == _page->scene.layer) {
            fude_notice_show(fude_text(FUDE_TEXT_ZOOM_LAYER_LAST));
            return RDE_UI_EVENT_RESULT_CONSUME;
        }
    }
    fude_zoom_page_layer_flags(_page, _row, (u8)(_page->layer_flags[_row] ^ FUDE_ZOOM_LAYER_HIDDEN));
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A layer's lock: locked or unlocked.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_layer_lock(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_page* _page = fude_zoom_page_the;
    const u32 _row = (u32)(uintptr_t)_user_data;
    if(_page != NULL && _row < _page->layer_count) {
        fude_zoom_page_layer_flags(_page, _row, (u8)(_page->layer_flags[_row] ^ FUDE_ZOOM_LAYER_LOCKED));
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Row _row moved _by places in the list (-1: up, over the one above it): every
// layer given its place anew (the top row the highest), those that changed a
// new LAYER object each, one undo step.
void fude_zoom_page_layer_move(fude_zoom_page* _page, u32 _row, i32 _by) {
    const i32 _to = (i32)_row + _by;
    if(_row >= _page->layer_count || _to < 0 || _to >= (i32)_page->layer_count) {
        return;
    }
    fude_zoom_scene* _s    = &_page->scene;
    const u32        _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    u32 _order[FUDE_ZOOM_PAGE_LAYER_ROWS];
    for(u32 _i = 0; _i < _page->layer_count; _i++) {
        _order[_i] = _i;
    }
    _order[_row] = (u32)_to;
    _order[_to]  = _row;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _died = rde_arr_new(sizeof(u32), _heap), _born = rde_arr_new(sizeof(u32), _heap);
    for(u32 _p = 0; _p < _page->layer_count; _p++) {
        const u32 _r     = _order[_p];
        const u32 _rank  = _page->layer_count - 1u - _p;
        const u8  _flags = fude_zoom_layer_with_rank(_page->layer_flags[_r], _rank);
        if(_flags == _page->layer_flags[_r]) {
            continue;
        }
        const u32 _old = _page->layer_object[_r];
        const u32 _new = fude_zoom_scene_add_layer(_s, _home, _page->layer_index[_r], _flags, _old != FUDE_ZOOM_NONE ? _page->layer_names[_r] : "");
        rde_arr_add(&_born, (any)&_new);
        if(_old != FUDE_ZOOM_NONE) {
            fude_zoom_scene_set_alive(_s, _old, false);
            rde_arr_add(&_died, (any)&_old);
        }
    }
    if(rde_arr_length(&_born) > 0) {
        fude_zoom_history_push(_s, _home, (fude_zoom_box){ 0.0, 0.0, 0.0, 0.0 }, (const u32*)_died.memory, (u32)rde_arr_length(&_died),
                               (const u32*)_born.memory, (u32)rde_arr_length(&_born));
    }
    rde_arr_free(&_died);
    rde_arr_free(&_born);
    fude_zoom_scene_layers_refresh(_s);
    fude_zoom_page_show_layers(_page);
}

// A layer's up arrow: moved over the one above it.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_layer_up(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    if(fude_zoom_page_the != NULL && fude_zoom_page_the->open) {
        fude_zoom_page_layer_move(fude_zoom_page_the, (u32)(uintptr_t)_user_data, -1);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A layer's down arrow: moved under the one below it.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_layer_down(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    if(fude_zoom_page_the != NULL && fude_zoom_page_the->open) {
        fude_zoom_page_layer_move(fude_zoom_page_the, (u32)(uintptr_t)_user_data, 1);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A layer's trash: it and everything on it let go, at every depth (one undo step brings them back).
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_layer_drop(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_page* _page = fude_zoom_page_the;
    const u32 _row = (u32)(uintptr_t)_user_data;
    if(_page == NULL || !_page->open || _row >= _page->layer_count) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    if(_page->layer_count <= 1u) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_LAYER_ONLY));
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    fude_zoom_scene* _s     = &_page->scene;
    const u16        _index = _page->layer_index[_row];
    c8 _name[40];
    rde_snprintf(_name, sizeof(_name), "%s", _page->layer_names[_row]);
    rde_arr _died = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    u32 _things = 0;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind == FUDE_ZOOM_KIND_FRAME || _o->kind == FUDE_ZOOM_KIND_MARK) {
            continue;   // (a frame is on no layer; what is in it goes by its own)
        }
        u16 _of = _o->layer;
        if(_o->kind == FUDE_ZOOM_KIND_LAYER && !fude_zoom_scene_layer_of(_s, _i, &_of, NULL, NULL, 0)) {
            continue;
        }
        if(_of != _index) {
            continue;
        }
        _things += _o->kind != FUDE_ZOOM_KIND_LAYER && !(_o->flags & FUDE_ZOOM_FLAG_CONTINUES) ? 1u : 0u;
        fude_zoom_scene_set_alive(_s, _i, false);
        rde_arr_add(&_died, (any)&_i);
    }
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    if(rde_arr_length(&_died) > 0) {
        fude_zoom_history_push(_s, _home, (fude_zoom_box){ 0.0, 0.0, 0.0, 0.0 }, (const u32*)_died.memory, (u32)rde_arr_length(&_died), NULL, 0);
    }
    rde_arr_free(&_died);
    fude_zoom_scene_layers_refresh(_s);
    fude_zoom_page_find_layers(_page, NULL);
    // Drawing on it: on the first left now.
    b8 _still = false;
    for(u32 _i = 0; _i < _page->layer_count; _i++) {
        _still = _still || _page->layer_index[_i] == _s->layer;
    }
    if(!_still && _page->layer_count > 0) {
        _s->layer = _page->layer_index[0];
    }
    fude_zoom_select_clear(&_page->selection);
    fude_zoom_page_show_layers(_page);
    c8 _say[200];
    FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_LAYER_DROPPED, FUDE_TS(_name), FUDE_TN(_things));
    fude_notice_show(_say);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// New layer: the next index, named, drawn on from now.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_layer_add(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->open) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    u16 _next = 0;
    for(u32 _i = 0; _i < _page->layer_count; _i++) {
        _next = _page->layer_index[_i] >= _next ? (u16)(_page->layer_index[_i] + 1u) : _next;
    }
    if(_next >= FUDE_ZOOM_LAYERS || _page->layer_count >= FUDE_ZOOM_PAGE_LAYER_ROWS) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    fude_zoom_scene* _s    = &_page->scene;
    const u32        _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    c8 _name[40];
    FUDE_TEXTF(_name, FUDE_TEXT_ZOOM_LAYER_N, FUDE_TN(_next + 1u));
    // On top of the others.
    i32 _top = (i32)_next;
    for(u32 _i = 0; _i < _page->layer_count; _i++) {
        _top = fude_zoom_page_layer_rank(_page, _i) >= _top ? fude_zoom_page_layer_rank(_page, _i) + 1 : _top;
    }
    const u8  _flags = _top > (i32)_next ? fude_zoom_layer_with_rank(0u, (u32)(_top < 30 ? _top : 30)) : 0u;
    const u32 _new = fude_zoom_scene_add_layer(_s, _home, _next, _flags, _name);
    fude_zoom_history_push(_s, _home, (fude_zoom_box){ 0.0, 0.0, 0.0, 0.0 }, NULL, 0, &_new, 1u);
    _s->layer = _next;
    fude_zoom_page_show_layers(_page);
    if(_page->layer_moving) {
        // To layer, then New layer: the selection goes onto the new one.
        for(u32 _i = 0; _i < _page->layer_count; _i++) {
            if(_page->layer_index[_i] == _next) {
                fude_zoom_page_move_to_layer(_page, _i);
                break;
            }
        }
        _page->layer_moving = false;
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// The lasso's To layer: the panel opened for the layer to move it to.
void fude_zoom_page_to_layer_selection(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->open || rde_arr_length(&_page->selection.picks) == 0) {
        return;
    }
    _page->layer_moving = true;
    _page->layers_open  = true;
    _page->places_open  = false;
    fude_zoom_page_show_layers(_page);
    fude_notice_show(fude_text(FUDE_TEXT_ZOOM_TO_LAYER_HOW));
}

// The toolbar's Layers: the panel shown or put away.
void fude_zoom_layers_press(fude_app* _app) {
    RDE_UNUSED(_app);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->open) {
        return;
    }
    _page->layers_open  = !_page->layers_open;
    _page->places_open  = false;
    _page->layer_moving = false;
    if(_page->layers_open) {
        fude_zoom_page_show_layers(_page);
    }
}

// The toolbar's Layers: whether it shows as chosen (the panel is open).
b8 fude_zoom_layers_selected(const fude_app* _app) {
    RDE_UNUSED(_app);
    return fude_zoom_page_the != NULL && fude_zoom_page_the->layers_open;
}
