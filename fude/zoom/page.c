// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"

// ===========================================================================
// See page.h.
//
// page.c is the coordinator of the page driver: the one page (fude_zoom_page_the, the state every module reaches),
// undo and redo, the page kind's hooks (FUDE_ZOOM_PAGE_KIND) and the page's init and destroy. Everything else is in
// the page_*.c modules, by what it does; page_parts.h is what they share and says which module holds what.
// ===========================================================================

// The one page (the page kind's hooks are handed the app, not the page).
fude_zoom_page* fude_zoom_page_the = NULL;

// See page.h: the window's size, halved.
fude_zoom_v2 fude_zoom_page_half(const fude_zoom_page* _page) {
    const rde_vec_2I _size = rde_window_get_size(_page->app->window);
    return (fude_zoom_v2){ (f64)_size.x * 0.5, (f64)_size.y * 0.5 };
}

// See page_parts.h: the screen, out from the camera's point by half its size over the zoom.
fude_zoom_box fude_zoom_page_view(const fude_zoom_page* _page) {
    const fude_zoom_camera* _c = &_page->scene.camera;
    const fude_zoom_v2      _h = fude_zoom_page_half(_page);
    return (fude_zoom_box){ _c->at.x - _h.x / _c->z, _c->at.y - _h.y / _c->z, _c->at.x + _h.x / _c->z, _c->at.y + _h.y / _c->z };
}

// --- undo, redo, and going to the change ---------------------------------------------------------

// Flies the camera to an undone or redone change, unless it is on the screen already and at least 4 points across
// there. The change is shown in its own frame, zoomed so it fills about 60% of the screen's width or height, within
// the zoom's limits.
RDE_INTERNAL void fude_zoom_page_show(fude_zoom_page* _page, const fude_zoom_action* _a) {
    if(_a == NULL || fude_zoom_box_is_empty(_a->box)) {
        return;
    }
    fude_zoom_scene* _s = &_page->scene;
    // Where it is, in the camera's frame: on screen there, nothing to do.
    const fude_zoom_box _in_cam = fude_zoom_sim_box(fude_zoom_scene_sim(_s, _a->frame, _s->camera.frame), _a->box);
    const f64 _size_now = rde_math_max_f64(_in_cam.max_x - _in_cam.min_x, _in_cam.max_y - _in_cam.min_y) * _s->camera.z;
    if(fude_zoom_box_overlaps(_in_cam, fude_zoom_page_view(_page)) && _size_now >= 4.0) {
        return;
    }
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    const f64 _w = rde_math_max_f64(_a->box.max_x - _a->box.min_x, 1e-300), _h = rde_math_max_f64(_a->box.max_y - _a->box.min_y, 1e-300);
    f64 _z = rde_math_min_f64(_half.x * 1.2 / _w, _half.y * 1.2 / _h);
    _z = _z > FUDE_ZOOM_Z_MAX ? FUDE_ZOOM_Z_MAX : (_z < FUDE_ZOOM_Z_MIN ? FUDE_ZOOM_Z_MIN : _z);
    fude_zoom_page_fly(_page, (fude_zoom_camera){ _a->frame, { (_a->box.min_x + _a->box.max_x) * 0.5, (_a->box.min_y + _a->box.max_y) * 0.5 }, _z }, false);
}

// See page_parts.h. Refused while a stroke, an erase or the lasso is under way, and while playing (with a notice).
// The instruments' step is undone when it is the latest; else the drawing's, copied before it is undone so that a
// notebook's added page can be taken out again and the camera flown to it.
b8 fude_zoom_page_undo(fude_zoom_page* _page) {
    if(!_page->open || _page->drawing || _page->erasing || fude_zoom_select_busy(&_page->selection)) {
        return false;
    }
    if(_page->play_on) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PLAY_LOCKED));
        return false;
    }
    if(fude_zoom_page_tools_undo(_page)) {
        return true;   // the instruments' step, the latest
    }
    const fude_zoom_action* _a = fude_zoom_history_next_undo(&_page->scene);
    const fude_zoom_action  _copy = _a != NULL ? *_a : (fude_zoom_action){ 0 };
    if(!fude_zoom_history_undo(&_page->scene)) {
        return false;
    }
    fude_zoom_page_pages_step(_page, _copy.tag, true);   // (a page added: taken out again)
    fude_zoom_page_show(_page, &_copy);
    return true;
}

// See page_parts.h. Refused as fude_zoom_page_undo is; the instruments' next step is redone when it comes before the
// drawing's.
b8 fude_zoom_page_redo(fude_zoom_page* _page) {
    if(!_page->open || _page->drawing || _page->erasing || fude_zoom_select_busy(&_page->selection)) {
        return false;
    }
    if(_page->play_on) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PLAY_LOCKED));
        return false;
    }
    if(fude_zoom_page_tools_redo(_page)) {
        return true;   // the instruments' step, the next
    }
    const fude_zoom_action* _a = fude_zoom_history_next_redo(&_page->scene);
    const fude_zoom_action  _copy = _a != NULL ? *_a : (fude_zoom_action){ 0 };
    if(!fude_zoom_history_redo(&_page->scene)) {
        return false;
    }
    fude_zoom_page_pages_step(_page, _copy.tag, false);   // (a page added: added again)
    fude_zoom_page_show(_page, &_copy);
    return true;
}

// The bar's bin: what the lasso holds, if anything (as Delete); else what Clear takes (scene.h's fude_zoom_scene_clear:
// all that is drawn, but not the places, the layers themselves, or what a hidden or locked layer holds), erased as one
// undoable step. Neither while playing.
void fude_zoom_page_clear(fude_zoom_page* _page) {
    if(!_page->open || _page->drawing || _page->erasing || fude_zoom_select_busy(&_page->selection)) {
        return;
    }
    if(_page->play_on) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PLAY_LOCKED));
        return;
    }
    if(fude_zoom_select_any(&_page->selection)) {
        fude_zoom_select_delete(&_page->selection, &_page->scene);
        return;
    }
    fude_zoom_scene* _s = &_page->scene;
    rde_arr _died = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_scene_clear(_s, &_died);
    fude_zoom_history_push(_s, _s->camera.frame, fude_zoom_page_view(_page), (const u32*)_died.memory, (u32)rde_arr_length(&_died), NULL, 0);
    rde_arr_free(&_died);
}

// See page_parts.h.
void fude_zoom_page_reset_view(fude_zoom_page* _page) {
    if(!_page->open) {
        return;
    }
    fude_zoom_page_fly(_page, (fude_zoom_camera){ _page->scene.home, { 0.0, 0.0 }, 1.0 }, true);
}

// See page_parts.h: the modes are partial, whole strokes and trim, then round again.
void fude_zoom_page_next_eraser(fude_zoom_page* _page) {
    _page->erase_mode = (u8)((_page->erase_mode + 1u) % FUDE_ZOOM_ERASE_COUNT);
    const FUDE_TEXT_ _says[FUDE_ZOOM_ERASE_COUNT] = { FUDE_TEXT_ZOOM_ERASER_PARTIAL, FUDE_TEXT_ZOOM_ERASER_STROKE, FUDE_TEXT_ZOOM_ERASER_TRIM };
    fude_notice_show(fude_text(_says[_page->erase_mode]));
}

// --- the page kind's hooks (extension.h) ----------------------------------------------------------

// Undo and Redo from the bar or the keys: the page's.
RDE_INTERNAL b8   fude_zoom_kind_undo(fude_app* _app)            { RDE_UNUSED(_app); return fude_zoom_page_undo(fude_zoom_page_the); }
// (see fude_zoom_kind_undo)
RDE_INTERNAL b8   fude_zoom_kind_redo(fude_app* _app)            { RDE_UNUSED(_app); return fude_zoom_page_redo(fude_zoom_page_the); }
// Whether Undo can do anything: a canvas is open, nothing is playing, and the drawing or the instruments have a step.
RDE_INTERNAL b8   fude_zoom_kind_can_undo(const fude_app* _app)  { RDE_UNUSED(_app); return fude_zoom_page_the->open && !fude_zoom_page_the->play_on && (fude_zoom_history_can_undo(&fude_zoom_page_the->scene) || fude_zoom_page_the->tools_done > 0u); }
// Whether Redo can do anything: the drawing has a step to redo, or the instruments do and nothing was drawn since
// (a drawing's step since then drops their redo: fude_zoom_page_tools_history).
RDE_INTERNAL b8   fude_zoom_kind_can_redo(const fude_app* _app)  {
    RDE_UNUSED(_app);
    const fude_zoom_page* _p = fude_zoom_page_the;
    return _p->open && !_p->play_on && (fude_zoom_history_can_redo(&_p->scene) ||
                        (_p->scene.history_pushes == _p->tools_seen && _p->tools_done < (u32)rde_arr_length(&_p->tools_steps)));
}
// The bar's bin: fude_zoom_page_clear.
RDE_INTERNAL void fude_zoom_kind_clear(fude_app* _app)           { RDE_UNUSED(_app); fude_zoom_page_clear(fude_zoom_page_the); }
// Reset view: fude_zoom_page_reset_view.
RDE_INTERNAL void fude_zoom_kind_reset_view(fude_app* _app)      { RDE_UNUSED(_app); fude_zoom_page_reset_view(fude_zoom_page_the); }
// The session opens canvas _c (a note's id) on the page.
RDE_INTERNAL void fude_zoom_kind_open(fude_app* _app, u32 _c)    { RDE_UNUSED(_app); fude_zoom_page_open(fude_zoom_page_the, _c); }
// The canvas's revision, which moves with every change worth saving (0 with none open).
RDE_INTERNAL u32  fude_zoom_kind_revision(const fude_app* _app)  { RDE_UNUSED(_app); return fude_zoom_page_the->open ? fude_zoom_page_the->scene.revision : 0u; }
// Whether a stroke, an erase, a shape or the lasso is under way: the session does not save then.
RDE_INTERNAL b8   fude_zoom_kind_busy(const fude_app* _app)      { RDE_UNUSED(_app); return fude_zoom_page_the->drawing || fude_zoom_page_the->erasing || fude_zoom_page_the->shaping || fude_zoom_select_busy(&fude_zoom_page_the->selection); }
// Saves the canvas now; _leave when the canvas is being left or the app is going, and then a template being edited
// is kept first.
RDE_INTERNAL b8   fude_zoom_kind_save(fude_app* _app, b8 _leave) {
    RDE_UNUSED(_app);
    if(_leave) {
        fude_zoom_page_template_keep(fude_zoom_page_the);   // (the app put away: a template's kept now — it may not come back)
    }
    return fude_zoom_page_save_now(fude_zoom_page_the, _leave);
}

// The box the page's menu sits over: the lasso's selection with its turning knob, or a PDF's text in the loop when
// nothing else is. *_busy says the lasso is still at work (the menu hides). Returns false when there is neither and
// the lasso is not busy; while it is busy and has no box yet, true with the box not written.
RDE_INTERNAL b8 fude_zoom_kind_selection(const fude_app* _app, rde_vec_2F* _min, rde_vec_2F* _max, b8* _busy) {
    RDE_UNUSED(_app);
    const fude_zoom_selection* _sel = &fude_zoom_page_the->selection;
    fude_zoom_box _b;
    *_busy = fude_zoom_select_busy(_sel);
    if(fude_zoom_page_the->open && !*_busy && !fude_zoom_select_any(_sel) && fude_zoom_page_the->pdf_text_on) {
        // Only a PDF's text in the loop: the row over it (Copy text).
        const fude_zoom_sim _home = fude_zoom_page_frame_sim(fude_zoom_page_the, fude_zoom_page_the->scene.home);
        const fude_zoom_box _t    = fude_zoom_page_the->pdf_text_box;
        const fude_zoom_v2  _a    = fude_zoom_sim_apply(_home, (fude_zoom_v2){ _t.min_x, _t.min_y });
        const fude_zoom_v2  _c    = fude_zoom_sim_apply(_home, (fude_zoom_v2){ _t.max_x, _t.max_y });
        *_min = (rde_vec_2F){ (f32)rde_math_min_f64(_a.x, _c.x), (f32)rde_math_min_f64(_a.y, _c.y) };
        *_max = (rde_vec_2F){ (f32)rde_math_max_f64(_a.x, _c.x), (f32)rde_math_max_f64(_a.y, _c.y) };
        return true;
    }
    if(!fude_zoom_page_the->open || !fude_zoom_select_box(_sel, &_b)) {
        return *_busy;
    }
    // The turning knob over the box is part of it, for the menu: it sits above that.
    *_min = (rde_vec_2F){ (f32)_b.min_x, (f32)_b.min_y };
    *_max = (rde_vec_2F){ (f32)_b.max_x, (f32)_b.max_y + FUDE_ZOOM_SELECT_KNOB + FUDE_ZOOM_SELECT_HANDLE };
    return true;
}

// Runs the page's menu's command _cmd (Cut, Copy, Duplicate, Delete, Paste at _at, Select all, Deselect) on the
// lasso's selection. While playing only Copy runs; Copy takes a PDF's text when only that is in the loop.
RDE_INTERNAL void fude_zoom_kind_command(fude_app* _app, u32 _cmd, rde_vec_2F _at) {
    fude_zoom_page*      _page = fude_zoom_page_the;
    fude_zoom_selection* _sel  = &_page->selection;
    if(!_page->open || _page->drawing || _page->erasing || fude_zoom_select_busy(&_page->selection)) {
        return;
    }
    if(_page->play_on && _cmd != FUDE_PAGE_CMD_COPY) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PLAY_LOCKED));   // (playing: nothing edited)
        return;
    }
    switch(_cmd) {
        case FUDE_PAGE_CMD_CUT:        fude_zoom_select_copy(_sel, &_page->scene); fude_zoom_select_delete(_sel, &_page->scene); break;
        case FUDE_PAGE_CMD_COPY:
            if(_page->pdf_text_on && !fude_zoom_select_any(_sel)) {
                fude_zoom_page_copy_pdf_text(_page);   // (only a PDF's text in the loop: that)
            } else {
                fude_zoom_select_copy(_sel, &_page->scene);
            }
            break;
        case FUDE_PAGE_CMD_DUPLICATE:  fude_zoom_select_duplicate(_sel, &_page->scene); break;
        case FUDE_PAGE_CMD_DELETE:     fude_zoom_select_delete(_sel, &_page->scene); break;
        case FUDE_PAGE_CMD_PASTE:      fude_zoom_select_paste(_sel, &_page->scene, _at); break;
        case FUDE_PAGE_CMD_SELECT_ALL: fude_zoom_select_all(_sel, &_page->scene, &_page->renderer); break;
        case FUDE_PAGE_CMD_DESELECT:   fude_zoom_select_clear(_sel); break;
        default: break;
    }
    _page->tool_seen = (u8)_app->ui->bar.tool;   // a Paste or Select all chose the Lasso: not a change to let it go for
    fude_ui_update(_app->ui);
}

// Whether Paste has something to paste.
RDE_INTERNAL b8 fude_zoom_kind_can_paste(const fude_app* _app) {
    RDE_UNUSED(_app);
    return fude_zoom_select_can_paste(&fude_zoom_page_the->selection);
}

// See page_parts.h. Back and Escape close the first of these that is up, in this order: Present, the map, the pages
// strip, Find's panel of every canvas, the audio panel (its playing stopped), a text being written on the page (it is
// saved), the diagram library, my pieces; then a template opened from a canvas goes back to that canvas. Returns
// false when none was, so the app's own Back runs.
b8 fude_zoom_kind_back(fude_app* _app) {
    RDE_UNUSED(_app);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->open) {
        return false;
    }
    if(_page->presenting) {
        fude_zoom_page_present_end(_page);
        return true;
    }
    if(_page->map_open) {
        _page->map_open = false;
        return true;
    }
    if(_page->strip_open) {
        _page->strip_open = false;
        return true;
    }
    if(_page->everywhere_shown) {
        _page->everywhere_shown = false;
        return true;
    }
    if(_page->audio_panel) {
        _page->audio_panel = false;
        fude_zoom_page_audio_stop_playing(_page);
        return true;
    }
    if(_page->inline_on) {
        fude_zoom_page_inline_end(_page);   // (a text being written on the page: saved)
        return true;
    }
    if(_page->library_open) {
        _page->library_open = false;
        return true;
    }
    if(_page->pieces_open) {
        _page->pieces_open = false;
        return true;
    }
    // A template opened from a canvas: back to it (what it holds kept as it closes).
    if(_page->template_on && _page->template_return != 0u && _page->template_return != _page->canvas &&
       fude_notes_find(_page->app->notes, _page->template_return) != NULL) {
        fude_notes_open(_page->app->notes, _page->template_return);
        return true;
    }
    return false;
}

// The page kind the shell drives the canvas through (extension.h).
const fude_page_kind FUDE_ZOOM_PAGE_KIND = {
    .undo = fude_zoom_kind_undo, .redo = fude_zoom_kind_redo, .can_undo = fude_zoom_kind_can_undo, .can_redo = fude_zoom_kind_can_redo,
    .clear = fude_zoom_kind_clear, .reset_view = fude_zoom_kind_reset_view,
    .open = fude_zoom_kind_open, .revision = fude_zoom_kind_revision, .busy = fude_zoom_kind_busy, .save = fude_zoom_kind_save,
    .selection = fude_zoom_kind_selection, .command = fude_zoom_kind_command, .can_paste = fude_zoom_kind_can_paste,
    .imported = fude_zoom_kind_imported, .back = fude_zoom_kind_back,
};

// --- the page ----------------------------------------------------------------------------------

// See page.h.
void fude_zoom_page_init(fude_zoom_page* _page, fude_app* _app) {
    fude_zoom_topic_fill(FUDE_ZOOM_TOPIC_GENERAL);   // (the bar's tools: General's until a canvas says)
    rde_memset(_page, 0, sizeof(*_page));
    _page->app = _app;
    _page->piece_armed = -1;
    // Ids are (device << 32) | counter: a device number new each launch keeps
    // them unique without keeping it anywhere (the counter goes on from what
    // the file has for the same number).
    _page->device = (u32)rde_date_now_unix_seconds() ^ (u32)(uintptr_t)_page ^ 0x5A17C0DEu;
    if(_page->device == 0) {
        _page->device = 1u;
    }
    fude_zoom_render_init(&_page->renderer);
    _page->everywhere_found = rde_arr_new(sizeof(fude_zoom_page_found), rde_memory_allocator_get_default_std());
    fude_zoom_audio_init(&_page->audio);
    _page->audio_faint    = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    _page->audio_faint_at = FUDE_ZOOM_NONE;
    _page->audio_replay   = true;
    _page->heard          = rde_arr_new(sizeof(fude_zoom_page_heard), rde_memory_allocator_get_default_std());
    _page->strip_pick     = FUDE_ZOOM_NONE;
    _page->strip_lru      = rde_lru_new(FUDE_ZOOM_PAGE_STRIP_PICS, rde_memory_allocator_get_default_std());
    _page->laser          = rde_ring_buffer_new(sizeof(fude_zoom_page_laser_point), FUDE_ZOOM_PAGE_LASER, rde_memory_allocator_get_default_std());
    fude_zoom_formulas_init(&_page->formulas);
    fude_zoom_handmath_chars_init(&_page->hand_chars);
    fude_zoom_formulas_load(&_page->formulas, "assets/fonts/NotoSansMath-Regular.ttf");   // (none: formulas left undrawn)
    _page->renderer.formulas = &_page->formulas;
    fude_zoom_export_formulas(&_page->formulas);   // (exports write them as drawn)
    fude_zoom_eraser_init(&_page->eraser);
    fude_ink_init(&_page->capture);
    fude_zoom_select_init(&_page->selection);
    fude_zoom_circuit_init(&_page->circuit);
    _page->circuit_held = FUDE_ZOOM_NONE;
    _page->circuit_letting = false;
    _page->knob_part = FUDE_ZOOM_NONE;
    _page->play_rate = 1.0;
    fude_zoom_mech_plan_init(&_page->mech);
    fude_zoom_mech_world_init(&_page->mech_run);
    fude_zoom_coupling_init(&_page->coupling);
    fude_zoom_symbol_art_load("assets/parts");   // (the library's symbols drawn from SVG art where the app has it: once)
    _page->play_scope   = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    _page->play_slider  = -1;
    _page->value_object = FUDE_ZOOM_NONE;
    _page->mech_lifted = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    _page->play_moved  = rde_arr_new(sizeof(f32), rde_memory_allocator_get_default_std());
    _page->circuit_labelled = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    _page->align_movers  = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    _page->align_wires   = rde_arr_new(sizeof(fude_zoom_page_drag_wire), rde_memory_allocator_get_default_std());
    _page->align_targets = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    _page->align_x = _page->align_y = RDE_NAN_F64;
    _page->renderer.lifted = &_page->selection.lifted;
    _page->editable     = rde_arr_new(sizeof(fude_zoom_visible), rde_memory_allocator_get_default_std());
    _page->play_sizes   = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std());
    _page->junctions      = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    _page->junctions_view = rde_arr_new(sizeof(fude_zoom_visible), rde_memory_allocator_get_default_std());
    _page->circuit_found    = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    _page->junctions_points = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    _page->junctions_ends   = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    _page->junctions_lines  = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    _page->attributes       = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    _page->mech_trail       = rde_arr_new(sizeof(rde_vec_2F), rde_memory_allocator_get_default_std());
    _page->mech_trail_runs  = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    _page->mech_trail_radii = rde_arr_new(sizeof(f32), rde_memory_allocator_get_default_std());
    _page->mech_belt_loop   = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    _page->examples_todo = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    _page->custom_items  = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_page_library_make(_page);   // (the diagram library's tiles, family by family)
    _page->context_target = FUDE_ZOOM_NONE;
    _page->snap_scratch = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    _page->raw          = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    _page->smoothed     = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    _page->smooth_level = FUDE_ZOOM_SMOOTH_DEFAULT;
    _page->place_here   = -1;
    _page->board_editing = FUDE_ZOOM_NONE;
    _page->symbol_editing = FUDE_ZOOM_NONE;
    _page->connector_heads = FUDE_ZOOM_ARROW_END;
    _page->node_grab      = -1;
    _page->node_tap_index = -1;
    _page->node_focus     = -1;
    _page->anchor_tool    = -1;
    fude_zoom_page_previews_free(_page);   // (none yet: their places marked free)
    fude_zoom_instruments_init(&_page->instruments);
    fude_zoom_pdfview_init(&_page->pdf);
    _page->units = (fude_zoom_units_style){ FUDE_ZOOM_UNIT_MM, 1u, 16u, false };
    _page->read_strokes = rde_arr_new(sizeof(fude_zoom_id), rde_memory_allocator_get_default_std());
    _page->tools_steps  = rde_arr_new(sizeof(fude_zoom_tools_step), rde_memory_allocator_get_default_std());
    _page->text_object = FUDE_ZOOM_NONE;
    fude_zoom_page_the = _page;
}

// See page.h.
void fude_zoom_page_destroy(fude_zoom_page* _page) {
    fude_zoom_symbol_art_free();   // loaded once, as the page was made
    if(rde_arr_is_inited(&_page->paper_lines)) {
        rde_arr_free(&_page->paper_lines);
    }
    fude_zoom_page_close(_page);
    fude_zoom_page_strip_free(_page);   // (its pictures, if close left any; then the slots' keeper)
    rde_lru_free(&_page->strip_lru);
    if(rde_ring_buffer_is_inited(&_page->laser)) {
        rde_ring_buffer_free(&_page->laser);
    }
    if(rde_arr_is_inited(&_page->read_strokes)) {
        rde_arr_free(&_page->read_strokes);
    }
    if(rde_arr_is_inited(&_page->tools_steps)) {
        rde_arr_free(&_page->tools_steps);
    }
    fude_zoom_render_destroy(&_page->renderer);
    rde_arr_free(&_page->everywhere_found);
    fude_zoom_voice_record_stop(&_page->recorder);
    fude_zoom_voice_close(&_page->player);
    fude_zoom_audio_free(&_page->audio);
    rde_arr_free(&_page->audio_faint);
    fude_zoom_page_heard_forget(_page);
    rde_arr_free(&_page->heard);
    fude_listen_shutdown();
    if(_page->cover_target != NULL) {
        rde_render_texture_unload(_page->cover_target);
        _page->cover_target = NULL;
    }
    fude_zoom_export_formulas(NULL);
    fude_zoom_handmath_chars_free(&_page->hand_chars);
    fude_zoom_formulas_free(&_page->formulas);
    fude_zoom_eraser_destroy(&_page->eraser);
    fude_ink_destroy(&_page->capture);
    fude_zoom_select_destroy(&_page->selection);
    fude_zoom_page_play_stop(_page);
    fude_zoom_page_sound_off(_page);   // (before its circuit goes: it lets go of the circuit's samples)
    fude_zoom_circuit_destroy(&_page->circuit);
    fude_zoom_mech_world_destroy(&_page->mech_run);
    fude_zoom_coupling_destroy(&_page->coupling);
    rde_arr_free(&_page->play_scope);
    fude_zoom_mech_plan_destroy(&_page->mech);
    rde_arr_free(&_page->mech_lifted);
    rde_arr_free(&_page->play_moved);
    rde_arr_free(&_page->circuit_labelled);
    rde_arr_free(&_page->align_movers);
    rde_arr_free(&_page->align_wires);
    rde_arr_free(&_page->align_targets);
    if(rde_arr_is_inited(&_page->editable)) {
        rde_arr_free(&_page->editable);
        rde_arr_free(&_page->play_sizes);
        rde_arr_free(&_page->junctions);
        rde_arr_free(&_page->junctions_view);
        rde_arr_free(&_page->circuit_found);
        rde_arr_free(&_page->junctions_points);
        rde_arr_free(&_page->junctions_ends);
        rde_arr_free(&_page->junctions_lines);
        rde_arr_free(&_page->attributes);
        rde_arr_free(&_page->mech_trail);
        rde_arr_free(&_page->mech_trail_runs);
        rde_arr_free(&_page->mech_trail_radii);
        rde_arr_free(&_page->mech_belt_loop);
        rde_arr_free(&_page->examples_todo);
        rde_arr_free(&_page->custom_items);
        rde_arr_free(&_page->library_kinds);
    }
    if(_page->templates_ready) {
        fude_zoom_templates_free(&_page->templates);
        _page->templates_ready = false;
    }
    if(rde_arr_is_inited(&_page->snap_scratch)) {
        rde_arr_free(&_page->snap_scratch);
    }
    if(rde_arr_is_inited(&_page->raw)) {
        rde_arr_free(&_page->raw);
    }
    if(rde_arr_is_inited(&_page->smoothed)) {
        rde_arr_free(&_page->smoothed);
    }
    if(rde_arr_is_inited(&_page->nudge_items)) {
        _page->renderer.lifted = NULL;
        rde_arr_free(&_page->nudge_items);
        rde_arr_free(&_page->nudge_points);
        rde_arr_free(&_page->nudge_radii);
        rde_arr_free(&_page->nudge_handles);
        rde_arr_free(&_page->nudge_lifted);   // (made with the others: page_pen.c)
    }
    if(rde_arr_is_inited(&_page->export_keep)) {
        rde_arr_free(&_page->export_keep);
        rde_arr_free(&_page->export_hidden);
    }
    if(rde_arr_is_inited(&_page->hand_lines)) {
        rde_arr_free(&_page->hand_lines);
        rde_arr_free(&_page->hand_strokes);
        rde_hash_map_free(&_page->hand_said);
    }
    if(fude_zoom_page_the == _page) {
        fude_zoom_page_the = NULL;
    }
}

// See page.h.
void fude_zoom_page_trim(fude_zoom_page* _page) {
    fude_zoom_render_trim(&_page->renderer);
    fude_zoom_page_strip_free(_page);   // (drawn again when shown)
    rde_font* const _fonts[3] = { _page->renderer.font_bold, _page->renderer.font_italic, _page->renderer.font_bold_italic };
    for(u32 _i = 0; _i < 3u; _i++) {
        if(_fonts[_i] != NULL) {
            rde_font_trim(_fonts[_i]);   // (their glyphs come back as they are drawn)
        }
    }
    fude_zoom_page_previews_free(_page);   // (drawn again when wanted)
    fude_zoom_page_map_free(_page);
}

// See page.h: what a pen's event would do, timed now.
void fude_zoom_page_look_pen(fude_zoom_page* _page, u8 _phase, rde_vec_2F _screen) {
    _page->capture.sample_time = rde_engine_get_time_now();
    if(_phase == 0u) {
        fude_zoom_page_down(_page, _screen, true, false);
    } else if(_phase == 1u) {
        fude_zoom_page_moved(_page, _screen);
    } else {
        fude_zoom_page_up(_page);
    }
}
