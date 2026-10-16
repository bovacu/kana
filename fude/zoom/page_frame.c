// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"

// ===========================================================================
// Page, each frame: fude_zoom_page_update, which keeps everything the page shows up to date once a frame.
//
// The file holds fude_zoom_page_update's steps (fude_zoom_page_update_*, in the order it takes them, each handed the
// locals it reads: the app, the window, the scene, which keys are held), then the function itself. Prefix
// fude_zoom_page_.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

#define FUDE_ZOOM_PAGE_WHEEL      1.12    // the zoom a wheel notch gives

// --- each frame ---------------------------------------------------------------------------------

// The layers' bits read again when the canvas changed, and the chip of what the lasso holds alone kept up: a line's
// or a dimension's length, a plain board's or a sheet's card (fude_zoom_page_update's).
RDE_INTERNAL void fude_zoom_page_update_layers_chip(fude_zoom_page* _page, fude_zoom_scene* _s) {
    // The layers' hidden and locked bits, again after any change (an undo of one, a canvas opened).
    if(_s->revision != _page->layers_mask_seen) {
        fude_zoom_scene_layers_refresh(_s);
        _page->layers_mask_seen = _s->revision;
    }
    // The lasso holding one line or dimension: its length on the chip, to type (and drive with); a plain board or a
    // sheet: its card's (a cut board has none: fude_zoom_page_board_cut).
    if(rde_arr_length(&_page->selection.picks) == 1u && !fude_zoom_select_busy(&_page->selection)) {
        const u32               _one = ((const fude_zoom_pick*)_page->selection.picks.memory)[0].object;
        const fude_zoom_object* _o   = fude_zoom_scene_object(_s, _one);
        if((_o->flags & FUDE_ZOOM_FLAG_ALIVE) && _o->kind == FUDE_ZOOM_KIND_SHAPE &&
           (_o->channels == FUDE_ZOOM_SHAPE_LINE || _o->channels == FUDE_ZOOM_SHAPE_DIMENSION ||
            (_o->channels == FUDE_ZOOM_SHAPE_BOARD && !fude_zoom_page_board_cut(_page, _one)) || _o->channels == FUDE_ZOOM_SHAPE_SHEET)) {
            _page->line_chip_object = _one;
            _page->line_chip_until  = rde_math_max_f64(_page->line_chip_until, rde_engine_get_time_now() + 0.3);
        }
    }
}

// What is under way, carried on: what the pickers brought, the handwriting read and written, a PNG export or a
// zoom video's frame, a flight, and the pen resting at a stroke's end (its shape tried) (fude_zoom_page_update's).
RDE_INTERNAL void fude_zoom_page_update_under_way(fude_zoom_page* _page, fude_app* _app, fude_zoom_scene* _s) {
    // What the pickers brought (Files, Photos): onto the page (fude_zoom_kind_imported).
    fude_import_update(_app);
    fude_zoom_page_read_update(_page);
    fude_zoom_page_hand_update(_page);

    // A PNG export drawn last frame: read back and written now (a zoom video's frame: into it).
    if(_page->export_stage == 2u) {
        fude_zoom_page_export_finish(_page);
    } else if(_page->export_stage == 11u) {
        fude_zoom_page_video_step(_page);
    }

    // A flight under way (nav.h): the camera where it has it now.
    if(_page->flight.active) {
        fude_zoom_fly_step(&_page->flight, _s, fude_zoom_page_half(_page), rde_engine_get_time_now());
    }

    // The pen resting at a stroke's end: a shape, if it looks like one.
    if(_page->drawing && !_page->snapped && !_page->ruled && rde_engine_get_time_now() - _page->moved_at >= FUDE_ZOOM_PAGE_HOLD) {
        fude_zoom_page_try_snap(_page);
        _page->moved_at = rde_engine_get_time_now() + 1e9;   // once a rest (moving again tries again)
    }
}

// The toolbar's taps since the last frame: Erase again (the eraser's next mode), another tool (the Shapes tool
// and the pen's modes let go) (fude_zoom_page_update's).
RDE_INTERNAL void fude_zoom_page_update_bar(fude_zoom_page* _page, fude_app* _app) {
    // Erase pressed again on the toolbar: the eraser's next mode.
    // (A rebuilt toolbar — another language — counts from 0 again: not a press.)
    if(_app->ui->bar.erase_taps != _page->erase_taps_seen) {
        if(_app->ui->bar.erase_taps > _page->erase_taps_seen) {
            fude_zoom_page_next_eraser(_page);
        }
        _page->erase_taps_seen = _app->ui->bar.erase_taps;
    }
    // One of the bar's tools chosen: the Shapes tool lets go.
    if(_app->ui->bar.tool_taps != _page->tool_taps_seen) {
        if(_app->ui->bar.tool_taps > _page->tool_taps_seen && _page->curve_tool) {
            fude_zoom_page_curve_commit(_page, false);   // what was tapped so far is kept
            _page->curve_tool = false;
            fude_toolbar_refresh(&_app->ui->bar);
        }
        if(_app->ui->bar.tool_taps > _page->tool_taps_seen && (_page->shape_tool != 0 || _page->fill_tool || _page->measure_tool != 0 || _page->text_tool != 0)) {
            fude_zoom_page_pen_modes_off(_page);
            fude_toolbar_refresh(&_app->ui->bar);
        }
        _page->tool_taps_seen = _app->ui->bar.tool_taps;
    }
}

// The keys pressed this frame, as fude_zoom_page_update says above where it reads which are held (_typing: a text
// field has the keyboard; _command, _shift: those keys held): Present's keys while presenting, undo and redo, then
// the letters.
RDE_INTERNAL void fude_zoom_page_update_keys(fude_zoom_page* _page, rde_window* _window, fude_zoom_scene* _s, b8 _typing, b8 _command, b8 _shift) {
    if(_page->presenting) {
        fude_zoom_page_present_keys(_page, _window);   // (on, back: nothing else is done while presenting)
    } else if(!_typing && _command) {
        if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_Z)) {
            if(_shift) {
                fude_zoom_page_redo(_page);
            } else {
                fude_zoom_page_undo(_page);
            }
        }
        if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_Y)) {
            fude_zoom_page_redo(_page);
        }
    } else if(!_typing) {
        if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_R)) {
            fude_zoom_page_reset_view(_page);
        }
        if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_E)) {
            fude_zoom_page_next_eraser(_page);
        }
        if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_BACKSPACE) || rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_DELETE)) {
            fude_zoom_select_delete(&_page->selection, _s);
        }
        if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_P)) {
            fude_zoom_page_present_start(_page);
        }
        if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_M)) {
            if(_page->map_open) {
                _page->map_open = false;
            } else {
                fude_zoom_page_map_open(_page);
            }
        }
    }
}

#if !defined(RDE_PLATFORM_MOBILE)
// A computer's mouse, read once a frame (there is none on a phone or a tablet): where it hovers, the left button's
// moves sent on while a tool has it, the page panned while the right button pans it, and the wheel zooming about
// the pointer (fude_zoom_page_update's).
RDE_INTERNAL void fude_zoom_page_update_mouse(fude_zoom_page* _page, fude_app* _app, rde_window* _window, fude_zoom_scene* _s) {
    const rde_vec_2I _m      = rde_input_mouse_get_position(_window);
    const rde_vec_2F _screen = { (f32)_m.x, (f32)_m.y };
    _page->hover_at     = _screen;
    _page->hover_eraser = false;
    _page->hover_on     = !fude_look_owns_pointer() && !fude_ui_hit(_app->ui, _screen);   // (a screenshot's: no real mouse)
    if(rde_input_mouse_is_button_pressed(_window, RDE_MOUSE_BUTTON_LEFT) &&
       (_page->drawing || _page->erasing || _page->shaping || fude_zoom_select_busy(&_page->selection) || _page->present_pen_down || _page->map_hand != 0u || _page->stretching)) {
        _page->capture.sample_time = rde_engine_get_time_now();
        fude_zoom_page_moved(_page, _screen);
    }
    if(_page->mouse_panning) {
        if(rde_math_abs_f32(_screen.x - _page->mouse_down.x) + rde_math_abs_f32(_screen.y - _page->mouse_down.y) > 4.0f) {
            _page->mouse_dragged = true;
            _page->flight.active = false;
        }
        fude_zoom_camera_pan(_s, (fude_zoom_v2){ _screen.x - _page->mouse_last.x, _screen.y - _page->mouse_last.y });
        fude_zoom_page_pan_speed(_page, (fude_zoom_v2){ _screen.x - _page->mouse_last.x, _screen.y - _page->mouse_last.y });
        _page->mouse_last = _screen;
    }
    const rde_vec_2F _wheel = rde_input_mouse_get_scrolled(_window);
    if(_wheel.y != 0.0f && !_page->drawing && !_page->erasing && !fude_ui_hit(_app->ui, _screen)) {
        _page->flight.active = false;
        fude_zoom_camera_zoom_at(_s, (fude_zoom_v2){ _screen.x, _screen.y }, rde_math_pow_f64(FUDE_ZOOM_PAGE_WHEEL, (f64)_wheel.y));
    }
}
#endif

// What follows the view, after the camera: the instruments' changes made a step, a PDF's pages (the view kept on
// them, their pieces drawn, a search's first match flown to), a notebook's pages, the recordings, and the words
// looked for in a canvas Find opened (fude_zoom_page_update's).
RDE_INTERNAL void fude_zoom_page_update_views(fude_zoom_page* _page, fude_zoom_scene* _s) {
    fude_zoom_page_tools_history(_page);   // (the instruments left changed: a step)
    // Over a PDF: the view kept on its pages; their pieces drawn; a search's first match gone to.
    if(fude_zoom_pdfview_is_open(&_page->pdf)) {
        fude_zoom_page_keep_view_over(_page, fude_zoom_pdfview_bounds(&_page->pdf), 0.0);
        fude_zoom_pdfview_update(&_page->pdf, fude_zoom_page_frame_sim(_page, _s->home), fude_zoom_page_half(_page));
        fude_zoom_box _match;
        if(fude_zoom_pdfview_search_wanted(&_page->pdf, &_match)) {
            fude_zoom_page_fly(_page, fude_zoom_nav_framing(fude_zoom_page_half(_page), _s->home, _match, 0.15), true);
            _page->find_next = _page->find_pdf_base + 1u;
            c8 _say[96];
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_FIND_OF, FUDE_TN(_page->find_next), FUDE_TN(_page->find_pdf_base + _page->pdf.match_count));
            fude_notice_show(_say);
        }
    }
    fude_zoom_page_pages_follow(_page);   // (a notebook: its view on its pages, as many as it needs)
    fude_zoom_page_audio_update(_page);   // (a recording made or played)
    if(_page->find_after[0] != 0 && _page->open && ++_page->find_after_frames >= 3u) {
        // (opened from Find's other canvases: the words found in it, the others looked through again from here)
        c8 _words[128];
        rde_snprintf(_words, sizeof(_words), "%s", _page->find_after);
        _page->find_after[0]        = 0;
        _page->find_after_frames    = 0u;
        _page->find_query[0]        = 0;
        _page->everywhere_query[0]  = 0;
        fude_zoom_page_find(_page, _words);
    }
}

// The journal written to disk about every second while there is anything new, and the instruments' file when only
// they changed: last in fude_zoom_page_update, after everything that may have changed the canvas.
RDE_INTERNAL void fude_zoom_page_update_flush(fude_zoom_page* _page, fude_zoom_scene* _s) {
    // The journal on disk about every second while there is anything new.
    const f64 _now = rde_engine_get_time_now();
    if(!_page->drawing && !_page->erasing && fude_bytes_size(&_s->journal) > 0 && _now - _page->flushed_at >= FUDE_ZOOM_PAGE_FLUSH) {
        fude_zoom_page_save_now(_page, false);
    } else if(!_page->drawing && _page->instruments.held < 0 && _now - _page->flushed_at >= FUDE_ZOOM_PAGE_FLUSH) {
        fude_zoom_page_keep_frames(_page, false);   // (the frames they lie in on file first)
        if(fude_bytes_size(&_s->journal) > 0) {
            fude_zoom_page_save_now(_page, false);
        } else {
            fude_zoom_page_tools_save(_page);   // (the instruments as they lie now, when they changed)
        }
        _page->flushed_at = _now;
    }
}

// See page.h. Nothing while the page is closed. Its steps run in a fixed order, each a block of its own here or a
// step above (fude_zoom_page_update_*): first what follows the drawing as it is (a flick, the examples, a template,
// the test canvas, the instruments, the layers, the lasso's chip), then what is under way, the bar, the keys, the
// fingers and the mouse, the camera and the selection, Play, the PDF's and the notebook's pages, the recordings, and
// last the journal flushed to disk.
void fude_zoom_page_update(fude_zoom_page* _page) {
    fude_app*   _app    = _page->app;
    rde_window* _window = _app->window;
    fude_zoom_scene* _s = &_page->scene;
    if(!_page->open) {
        return;
    }
    fude_zoom_page_coast(_page);                // (a flick carrying on)
    fude_zoom_page_examples_follow(_page);      // (Insert → Examples: the one waiting drawn on its canvas, open now)
    fude_zoom_page_template_follow(_page);      // (a template being made: filled on its canvas, open now)
#if FUDE_ZOOM_INKTEST_OFFERED
    fude_zoom_page_inktest_follow(_page);       // (Settings' test canvas: filled with the test ink, open now)
#endif
    fude_zoom_page_library_hold(_page);         // (a hand held on a Custom tile: its card)
    fude_zoom_page_instruments_follow(_page);   // (on the drawing as it is now, before the pen meets them)
#if !defined(RDE_PLATFORM_MOBILE)
    if(_page->anchor_dragging && _page->anchor_drag_hand == FUDE_ZOOM_PAGE_HAND_MOUSE) {
        const rde_vec_2I _m = rde_input_mouse_get_position(_app->window);
        fude_zoom_page_grip_moved(_page, (rde_vec_2F){ (f32)_m.x, (f32)_m.y }, FUDE_ZOOM_PAGE_HAND_MOUSE);
    }
#endif
    fude_zoom_page_update_layers_chip(_page, _s);

    fude_zoom_page_update_under_way(_page, _app, _s);

    fude_zoom_page_update_bar(_page, _app);

    // A keyboard's shortcuts, as on the drawing core's page (drawing/app/page.c), and never while a text field has the
    // keyboard (a text written on the page, a rename, a search): what is typed there is text, and Backspace there must
    // not delete the selection. Undo and redo are the platform's own, with Command or Control held: Z undoes, Shift+Z
    // or Y redoes. No letter clears the canvas: on a tablet with a keyboard, a slip of a finger must never lose a
    // drawing (Clear is on the toolbar).
    const b8 _typing  = _app->ui != NULL && rde_ui_canvas_get_focused(_app->ui->canvas) != NULL;
    const b8 _command = rde_input_key_is_pressed(_window, RDE_KEYBOARD_KEY_LEFT_GUI) || rde_input_key_is_pressed(_window, RDE_KEYBOARD_KEY_RIGHT_GUI) ||
                        rde_input_key_is_pressed(_window, RDE_KEYBOARD_KEY_LEFT_CTRL) || rde_input_key_is_pressed(_window, RDE_KEYBOARD_KEY_RIGHT_CTRL);
    const b8 _shift   = rde_input_key_is_pressed(_window, RDE_KEYBOARD_KEY_LEFT_SHIFT) || rde_input_key_is_pressed(_window, RDE_KEYBOARD_KEY_RIGHT_SHIFT);
    fude_zoom_page_update_keys(_page, _window, _s, _typing, _command, _shift);

    // Another tool chosen: the selection is let go (a text being written: saved).
    if((u8)_app->ui->bar.tool != _page->tool_seen && _page->inline_on) {
        fude_zoom_page_inline_end(_page);
    }
    if((u8)_app->ui->bar.tool != _page->tool_seen) {
        _page->tool_seen = (u8)_app->ui->bar.tool;
        if(_app->ui->bar.tool != FUDE_TOOL_LASSO && !fude_zoom_select_busy(&_page->selection)) {
            fude_zoom_select_clear(&_page->selection);
        }
    }

    // One finger held still (fingers moving the page, not drawing): the page's menu.
    if(!_page->long_pressed && !_page->tap_spoiled && fude_zoom_page_finger_count(_page) == 1u && !_page->play_on &&
       rde_engine_get_time_now() - _page->tap_start >= FUDE_ZOOM_PAGE_LONG_PRESS) {
        for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_FINGERS; _i++) {
            if(_page->fingers[_i].active) {
                _page->long_pressed = true;
                _page->tap_spoiled  = true;   // nor a tap
                fude_zoom_page_context_open(_page, _page->fingers[_i].at);
                break;
            }
        }
    }

    // A computer's mouse: where it hovers, the left button's moves sent on while a tool has it, the page panned while
    // the mouse pans it (past 4 points it is a drag, not a click), and the wheel zooming about the pointer.
#if !defined(RDE_PLATFORM_MOBILE)
    fude_zoom_page_update_mouse(_page, _app, _window, _s);
#endif

    // The camera kept in range (fude_zoom_camera_settle: into a child frame or up to the parent as its zoom says), only
    // while nothing is being drawn, rubbed out, lassoed or flown.
    if(!_page->drawing && !_page->erasing && !fude_zoom_select_busy(&_page->selection) && !_page->flight.active) {
        fude_zoom_camera_settle(_s, fude_zoom_page_half(_page));
    }
    _page->selection.half = fude_zoom_page_half(_page);
    fude_zoom_select_update(&_page->selection, _s);
    {
        const f64 _t0 = rde_engine_get_time_now();
        fude_zoom_page_play_step(_page);   // (what plays: on its own clock)
        _page->perf_play += rde_engine_get_time_now() - _t0;
    }
    // B: Back, to the view the last flight left (not while presenting: only Present's own keys act then).
    if(!_typing && !_command && !_page->presenting && rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_B)) {
        fude_zoom_page_go_back(_page);
    }

    fude_zoom_page_update_views(_page, _s);
    fude_zoom_page_update_flush(_page, _s);
}
