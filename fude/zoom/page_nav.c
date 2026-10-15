// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"

// ===========================================================================
// Page, getting around (nav.h): flying the camera, Back, the marks' arrows, and the extension's widgets for it
// (Back, the depth and its crumbs, Places, the map's and home buttons).
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// --- getting around (nav.h) ---------------------------------------------------------------------

// Flies the camera to _to (nav.h's flight, over the coming frames); with _remember, where it was goes on Back's list,
// the oldest dropped when the list is full (FUDE_ZOOM_PAGE_BACK). Any flight closes the depth's levels.
void fude_zoom_page_fly(fude_zoom_page* _page, fude_zoom_camera _to, b8 _remember) {
    fude_zoom_scene* _s = &_page->scene;
    if(_remember) {
        if(_page->back_count == FUDE_ZOOM_PAGE_BACK) {
            rde_memmove(&_page->back[0], &_page->back[1], sizeof(_page->back[0]) * (FUDE_ZOOM_PAGE_BACK - 1u));
            _page->back_count--;
        }
        _page->back[_page->back_count].frame = fude_zoom_scene_frame(_s, _s->camera.frame)->id;
        _page->back[_page->back_count].at    = _s->camera.at;
        _page->back[_page->back_count].z     = _s->camera.z;
        _page->back_count++;
    }
    _page->crumbs_open = false;
    fude_zoom_fly_begin(&_page->flight, _s, _to, fude_zoom_page_half(_page), rde_engine_get_time_now());
}

// Back: flies to the view the last remembered flight left, without remembering this one. A frame gone from the scene
// is skipped for the view before it; a frame dropped since (made for a camera that left it empty) is seen from its
// nearest parent still there, the view carried up through each frame's place.
void fude_zoom_page_go_back(fude_zoom_page* _page) {
    fude_zoom_scene* _s = &_page->scene;
    while(_page->back_count > 0) {
        _page->back_count--;
        u32 _f = fude_zoom_scene_find_frame(_s, _page->back[_page->back_count].frame);
        if(_f == FUDE_ZOOM_NONE) {
            continue;
        }
        fude_zoom_v2 _at = _page->back[_page->back_count].at;
        f64          _z  = _page->back[_page->back_count].z;
        while(fude_zoom_scene_frame(_s, _f)->removed && fude_zoom_scene_frame(_s, _f)->parent != FUDE_ZOOM_NONE) {
            const fude_zoom_frame* _fr = fude_zoom_scene_frame(_s, _f);
            _at = fude_zoom_sim_apply(fude_zoom_sim_from_xform(_fr->xf), _at);
            _z /= rde_math_abs_f64(_fr->xf.scale);
            _f  = _fr->parent;
        }
        fude_zoom_page_fly(_page, (fude_zoom_camera){ _f, _at, _z }, false);
        return;
    }
}

// A press on one of the marks (an empty screen's arrows and rings) or on its preview: flies to what it points at,
// framed, and clears the marks. Returns true when it was on one.
b8 fude_zoom_page_mark_press(fude_zoom_page* _page, rde_vec_2F _screen) {
    for(u32 _i = 0; _i < _page->mark_count; _i++) {
        const fude_zoom_nav_mark* _m = &_page->marks[_i];
        const f32 _dx = _screen.x - _m->at.x, _dy = _screen.y - _m->at.y;
        rde_vec_2F _pc;
        const i8   _pv = _page->mark_preview[_i];
        const b8   _on_preview = _pv >= 0 && _page->preview_ready[_pv] && fude_zoom_page_preview_at(_page, _m, fude_zoom_page_half(_page), &_pc) &&
                                 rde_math_abs_f32(_screen.x - _pc.x) <= FUDE_ZOOM_PAGE_PREVIEW_W * 0.5f + 4.0f && rde_math_abs_f32(_screen.y - _pc.y) <= FUDE_ZOOM_PAGE_PREVIEW_H * 0.5f + 4.0f;
        if(_on_preview || _dx * _dx + _dy * _dy <= FUDE_ZOOM_PAGE_MARK_REACH * FUDE_ZOOM_PAGE_MARK_REACH) {
            fude_zoom_page_fly(_page, fude_zoom_nav_framing(fude_zoom_page_half(_page), _m->frame, _m->box, 0.6), true);
            _page->mark_count = 0;
            return true;
        }
    }
    return false;
}

// Fills the depth's levels: what is drawn at each level up from the camera, framed (nav.h's fude_zoom_nav_levels).
RDE_INTERNAL void fude_zoom_page_find_crumbs(fude_zoom_page* _page) {
    _page->crumb_count = fude_zoom_nav_levels(&_page->scene, fude_zoom_page_half(_page), _page->crumb_views, FUDE_ZOOM_PAGE_CRUMBS);
}

// --- getting around: Back and the depth (the extension's widgets, ui.h) ----------------------

// The home button: the app's home screen (its canvases).
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_home(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page != NULL && _page->on_home != NULL) {
        fude_zoom_page_inline_end(_page);   // A text being written is saved first,
        fude_zoom_page_cover_now(_page);    // and the canvas's cover made as it is now.
        _page->on_home(_page->app);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Back's button: fude_zoom_page_go_back.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_back(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    if(fude_zoom_page_the != NULL) {
        fude_zoom_page_go_back(fude_zoom_page_the);
    }
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// The depth tapped: its levels shown over it, or put away; Places is closed. Each level's button is named by its
// depth, and with the true size calibrated a last button flies to the true size.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_depth(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    _page->crumbs_open = !_page->crumbs_open;
    _page->places_open = false;
    if(_page->crumbs_open) {
        fude_zoom_page_find_crumbs(_page);
        // Calibrated: the true size last (the levels' words are their depths).
        if(_page->true_mm_per_point > 0.0f) {
            if(_page->crumb_count == FUDE_ZOOM_PAGE_CRUMBS) {
                _page->crumb_count--;
            }
            _page->crumb_views[_page->crumb_count++] = fude_zoom_page_true_view(_page);
        }
        for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_CRUMBS; _i++) {
            if(_page->crumbs[_i] == NULL || _i >= _page->crumb_count) {
                continue;
            }
            if(_page->true_mm_per_point > 0.0f && _i == _page->crumb_count - 1u) {
                rde_ui_button_set_text(_page->crumbs[_i], fude_text(FUDE_TEXT_ZOOM_TRUE_SIZE_VIEW));
                continue;
            }
            c8 _said[32];
            fude_zoom_nav_say(fude_zoom_nav_depth_of(&_page->scene, _page->crumb_views[_i]), _said, sizeof _said);
            rde_ui_button_set_text(_page->crumbs[_i], _said);
        }
        _page->laid_out = (rde_vec_2F){ 0.0f, 0.0f };   // Placed again for how many there are.
    }
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// A level of the depth tapped (its index in _user_data): flies to it, remembered for Back.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_crumb(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_page* _page = fude_zoom_page_the;
    const u32       _i    = (u32)(uintptr_t)_user_data;
    if(_page != NULL && _i < _page->crumb_count) {
        fude_zoom_page_fly(_page, _page->crumb_views[_i], true);
    }
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Returns the index of the bookmark (a MARK object: scene.h) the view is at, or -1: within 24 screen points of it and
// within half a zoom level (a factor of about 1.4) of its zoom.
RDE_INTERNAL i32 fude_zoom_page_mark_here(const fude_zoom_page* _page) {
    const fude_zoom_scene* _s = &_page->scene;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        f64 _mz;
        if(_o->kind != FUDE_ZOOM_KIND_MARK || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || !fude_zoom_scene_mark_of(_s, _i, &_mz, NULL)) {
            continue;
        }
        const fude_zoom_sim _to = fude_zoom_scene_sim(_s, _o->frame, _s->camera.frame);
        const fude_zoom_v2  _at = fude_zoom_sim_apply(_to, _o->t);
        const f64           _z  = _mz / fude_zoom_sim_scale(_to);   // Its zoom in the camera's frame's units.
        if(rde_math_hypot_f64(_at.x - _s->camera.at.x, _at.y - _s->camera.at.y) * _s->camera.z < 24.0 && rde_math_abs_f64(rde_math_log2_f64(_z / _s->camera.z)) < 0.5) {
            return (i32)_i;
        }
    }
    return -1;
}

// Fills Places as things are now: row 0 says Mark or Unmark for the view, and the rows after it the bookmarks alive,
// the newest first (at most FUDE_ZOOM_PAGE_PLACES), each with its number and its depth.
RDE_INTERNAL void fude_zoom_page_find_places(fude_zoom_page* _page) {
    const fude_zoom_scene* _s = &_page->scene;
    _page->place_here  = fude_zoom_page_mark_here(_page);
    _page->place_count = 0;
    for(u32 _i = fude_zoom_scene_object_count(_s); _i-- > 0 && _page->place_count < FUDE_ZOOM_PAGE_PLACES;) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if(_o->kind == FUDE_ZOOM_KIND_MARK && (_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
            _page->place_marks[_page->place_count++] = _i;
        }
    }
    if(_page->place_rows[0] == NULL) {
        return;
    }
    rde_ui_button_set_text(_page->place_rows[0], fude_text(_page->place_here >= 0 ? FUDE_TEXT_ZOOM_PLACE_UNMARK : FUDE_TEXT_ZOOM_PLACE_MARK));
    for(u32 _r = 0; _r < _page->place_count; _r++) {
        const u32               _m = _page->place_marks[_r];
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _m);
        f64 _z = 1.0;
        u32 _number = 0;
        fude_zoom_scene_mark_of(_s, _m, &_z, &_number);
        c8 _depth[32], _name[96], _row[160];
        fude_zoom_nav_say(fude_zoom_nav_depth_of(_s, (fude_zoom_camera){ _o->frame, _o->t, _z }), _depth, sizeof _depth);
        FUDE_TEXTF(_name, FUDE_TEXT_ZOOM_PLACE_N, FUDE_TN(_number));
        rde_snprintf(_row, sizeof _row, "%s \xC2\xB7 %s", _name, _depth);
        rde_ui_button_set_text(_page->place_rows[_r + 1u], _row);
    }
    _page->laid_out = (rde_vec_2F){ 0.0f, 0.0f };   // Placed again for how many there are.
}

// Lets a bookmark go, as an undo step whose box is the view it kept (so undo flies back to it), and says so.
RDE_INTERNAL void fude_zoom_page_unmark(fude_zoom_page* _page, u32 _mark) {
    fude_zoom_scene*        _s = &_page->scene;
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _mark);
    f64 _z = 1.0;
    fude_zoom_scene_mark_of(_s, _mark, &_z, NULL);
    const fude_zoom_v2  _half = fude_zoom_page_half(_page);
    const fude_zoom_box _view = { _o->t.x - _half.x / _z, _o->t.y - _half.y / _z, _o->t.x + _half.x / _z, _o->t.y + _half.y / _z };
    const u32           _frame = _o->frame;
    fude_zoom_scene_set_alive(_s, _mark, false);
    fude_zoom_history_push(_s, _frame, _view, &_mark, 1, NULL, 0);
    fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PLACE_UNMARKED));
}

// The map's button: its panel up or put away. A second press within 0.35 s is taken as the same tap, since a tablet's
// comes twice.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_map(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    fude_zoom_page* _page = fude_zoom_page_the;
    const f64       _now  = rde_engine_get_time_now();
    if(_page == NULL || !_page->open || _now - _page->map_pressed_at < 0.35) {
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    _page->map_pressed_at = _now;
    if(_page->map_open) {
        _page->map_open = false;
    } else {
        fude_zoom_page_map_open(_page);
    }
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Places' button: the list up (filled as things are now) or put away; the layers' panel and the depth's levels close.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_places(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page != NULL) {
        _page->places_open = !_page->places_open;
        _page->layers_open = false;
        _page->crumbs_open = false;
        if(_page->places_open) {
            fude_zoom_page_find_places(_page);
        }
    }
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// A place's ×: lets it go and fills the list again as it is now.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_place_drop(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_page* _page = fude_zoom_page_the;
    const u32       _row  = (u32)(uintptr_t)_user_data;
    if(_page == NULL || !_page->open || _row >= _page->place_count) {
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    fude_zoom_page_unmark(_page, _page->place_marks[_row]);
    fude_zoom_page_find_places(_page);
    _page->shown_place_rows = !_page->shown_place_rows;   // Flipped: the rows are shown again for how many there are.
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// A row of Places: row 0 marks the view (or unmarks the place it is at); the others fly to theirs.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_place(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_page* _page = fude_zoom_page_the;
    const u32       _row  = (u32)(uintptr_t)_user_data;
    if(_page == NULL || !_page->open) {
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    fude_zoom_scene* _s = &_page->scene;
    if(_row == 0u) {
        _page->place_here = fude_zoom_page_mark_here(_page);   // as the view is now
        if(_page->place_here >= 0) {
            fude_zoom_page_unmark(_page, (u32)_page->place_here);
        } else {
            // Numbered after every one there has been (a number is never given twice).
            u32 _number = 0;
            for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
                u32 _n = 0;
                if(fude_zoom_scene_mark_of(_s, _i, NULL, &_n)) {
                    _number = _n > _number ? _n : _number;
                }
            }
            const u32 _m = fude_zoom_scene_add_mark(_s, _s->camera.frame, _s->camera.at, _s->camera.z, _number + 1u);
            fude_zoom_history_push(_s, _s->camera.frame, fude_zoom_page_view(_page), NULL, 0, &_m, 1);
            c8 _name[96], _line[160];
            FUDE_TEXTF(_name, FUDE_TEXT_ZOOM_PLACE_N, FUDE_TN(_number + 1u));
            FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_PLACE_MARKED, FUDE_TS(_name));
            fude_notice_show(_line);
        }
        _page->places_open = false;
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    if(_row - 1u < _page->place_count) {
        const u32               _m = _page->place_marks[_row - 1u];
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _m);
        f64 _z = 1.0;
        if(fude_zoom_scene_mark_of(_s, _m, &_z, NULL)) {
            fude_zoom_page_fly(_page, (fude_zoom_camera){ _o->frame, _o->t, _z }, true);
        }
    }
    _page->places_open = false;
    return RDE_UI_EVENT_RESULT_DEFAULT;
}
