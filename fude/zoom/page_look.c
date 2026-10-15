// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"

// ===========================================================================
// Page, the developer's look harness (drawing/app/look.h): the fude_zoom_page_look_* functions, which drive the
// page as a hand would, for the screenshots and checks.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// See page.h: the Pages tool's choice, made through its own callback.
void fude_zoom_page_look_pages(fude_zoom_page* _page, u32 _choice) {
    RDE_UNUSED(_page);
    fude_zoom_pages_choose(NULL, _choice);
}

// See page.h. Each is the callback its button calls; any _what past 4 (and 2) is Back.
void fude_zoom_page_look_nav(fude_zoom_page* _page, u8 _what) {
    if(_what == 0u) {
        fude_zoom_page_reset_view(_page);
    } else if(_what == 1u) {
        fude_zoom_page_on_depth(NULL, NULL, NULL);
    } else if(_what == 3u) {
        fude_zoom_page_on_place(NULL, NULL, (any)(uintptr_t)0u);   // mark (or unmark) the view
    } else if(_what == 4u) {
        fude_zoom_page_on_places(NULL, NULL, NULL);
    } else {
        fude_zoom_page_go_back(_page);
    }
}

// See page.h. The numpad opened with _text, for a length, as a line's length is typed.
void fude_zoom_page_look_numpad(fude_zoom_page* _page, const c8* _text) {
    fude_zoom_numpad_open(&_page->numpad, _text, _page->units.unit, false, NULL, fude_zoom_page_line_typed, _page);
}

// See page.h. Kinds 0 to 2 are written and saved; a formula (3), and a text or a note written on the page (4, 5), are
// left open with their words typed.
void fude_zoom_page_look_text(fude_zoom_page* _page, rde_vec_2F _screen, u8 _kind, const c8* _words) {
    _page->text_tool = _kind < 3u ? (u8)(_kind + 1u) : _kind == 3u ? 3u : _kind == 5u ? 2u : 1u;
    fude_zoom_page_text_at(_page, _screen);
    if(_page->text_field != NULL) {
        const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
        rde_ui_text_editor_delete_range(_page->text_field, 0, _bytes);
        rde_ui_text_editor_insert_at(_page->text_field, 0, _words, rde_strlen(_words));
        rde_ui_text_editor_set_cursor(_page->text_field, 0, rde_strlen(_words));
        if(_kind < 3u) {   // (3: a formula's card left open, its words typed; 4, 5: a text, a note, written on the page, left open)
            fude_zoom_page_text_save(_page);
        }
    }
}

// See page.h: Settings' Calibrate pressed.
void fude_zoom_page_look_calibrate(fude_zoom_page* _page) {
    fude_zoom_page_on_calibrate(NULL, NULL, NULL);
    RDE_UNUSED(_page);
}

// See page.h.
void fude_zoom_page_look_mermaid(fude_zoom_page* _page, const c8* _text) {
    fude_zoom_page_import_mermaid(_page, _text);
}

// See page.h. 0 the Layers button, 1 a layer added, 2 row _row's eye, 4 everything moved to row _row's layer, 5 row
// _row chosen (its name on the card), 7 row _row moved down a place; anything else row _row's lock.
void fude_zoom_page_look_layers(fude_zoom_page* _page, u8 _what, u32 _row) {
    if(_what == 0u) {
        fude_zoom_layers_press(_page->app);
    } else if(_what == 1u) {
        fude_zoom_page_on_layer_add(NULL, NULL, NULL);
    } else if(_what == 2u) {
        fude_zoom_page_on_layer_eye(NULL, NULL, (any)(uintptr_t)_row);
    } else if(_what == 4u) {
        // Everything lassoed, To layer, the row tapped.
        fude_zoom_select_all(&_page->selection, &_page->scene, &_page->renderer);
        fude_zoom_page_to_layer_selection(_page->app, NULL, 0);
        fude_zoom_page_on_layer(NULL, NULL, (any)(uintptr_t)_row);
    } else if(_what == 5u) {
        fude_zoom_page_on_layer(NULL, NULL, (any)(uintptr_t)_row);   // (the chosen one: its name on the card)
    } else if(_what == 7u) {
        fude_zoom_page_layer_move(_page, _row, 1);   // the row moved down a place
    } else {
        fude_zoom_page_on_layer_lock(NULL, NULL, (any)(uintptr_t)_row);
    }
}

// See page.h.
void fude_zoom_page_look_rename(fude_zoom_page* _page, const c8* _name) {
    const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_page->text_field, 0, _bytes);
    }
    rde_ui_text_editor_insert_at(_page->text_field, 0, _name, rde_strlen(_name));
    fude_zoom_page_text_save(_page);
}

// See page.h. The shapes are made in the camera's frame, 250 points left of and 60 below the view's middle; _k is that
// frame's units a screen point.
void fude_zoom_page_look_drive(fude_zoom_page* _page, u8 _step) {
    fude_zoom_scene* _s = &_page->scene;
    const u32 _frame = _s->camera.frame;
    const fude_zoom_sim _back = fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _frame));
    const f64 _scale = fude_zoom_sim_scale(_back);
    const f64 _k = _scale > 0.0 ? _scale : 1.0;   // frame units a point (a frame of no size, or none: 1)
    const fude_zoom_v2 _a = fude_zoom_sim_apply(_back, (fude_zoom_v2){ -250.0, -60.0 });
    const rde_color _ink = _page->app->ink->color;
    if(_step == 0u) {
        // A line 300 points long, its dimension over it, a square on its end, a line up from its end.
        const f64 _line[2] = { 300.0 * _k, 0.0 }, _dim[3] = { 300.0 * _k, 0.0, 30.0 * _k }, _sq[3] = { 60.0 * _k, 60.0 * _k, 0.0 }, _up[2] = { 0.0, 180.0 * _k };
        u32 _born[4];
        _born[0] = fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ _a, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, _line, 2u, _ink, (f32)(1.5 * _k), 0u, 0);
        _born[1] = fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ _a, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_DIMENSION, _dim, 3u, _ink, (f32)(0.5 * _k), 0u, 0);
        _born[2] = fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ { _a.x + 360.0 * _k, _a.y - 60.0 * _k }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, _sq, 3u, _ink, (f32)(1.5 * _k), 0u, 0);
        _born[3] = fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ { _a.x + 300.0 * _k, _a.y }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, _up, 2u, _ink, (f32)(1.5 * _k), 0u, 0);
        fude_zoom_history_push(_s, _frame, fude_zoom_scene_object(_s, _born[0])->box, NULL, 0, _born, 4u);
        _page->line_chip_object = _born[1];
        _page->line_chip_until  = rde_engine_get_time_now() + FUDE_ZOOM_PAGE_LINE_CHIP;
    } else {
        f64 _dx, _dy;
        const f64 _mm = fude_zoom_page_line_mm(_page, _page->line_chip_object, &_dx, &_dy);
        fude_zoom_page_line_typed(_page, "", FUDE_ZOOM_UNIT_MM, _mm * 1.5);   // half as long again
    }
}

// See page.h: a canvas open as a notebook (its pages on).
b8 fude_zoom_page_is_notebook(const fude_zoom_page* _page) {
    return _page != NULL && _page->open && fude_zoom_page_paged(_page);
}

// See page.h. Only in a notebook.
void fude_zoom_page_look_strip(fude_zoom_page* _page) {
    if(_page->open && _page->pages.on) {
        fude_zoom_page_strip_open(_page);
    }
}

// See page.h. 0: a recording made up of a 330 Hz tone _value seconds long (10 when 0), written as the recorder writes
// one, with every object on the canvas linked to it in the order they were made, spread evenly over it. 1: the audio
// panel opened. 2: the newest recording played from _value milliseconds. 3: the microphone's chip pressed. 4 to 8:
// speech to text, as the comment inside says.
void fude_zoom_page_look_audio(fude_zoom_page* _page, u8 _what, u32 _value) {
    if(_what == 0u) {
        // A made-up recording (a tone, _value seconds), what is on the canvas linked to it in the order it was made,
        // spread over it — as if it had been written while it ran.
        const u32 _seconds = _value > 0u ? _value : 10u;
        const u32 _number  = fude_zoom_audio_begin(&_page->audio, (u64)rde_date_now_unix_seconds(), FUDE_ZOOM_AUDIO_RATE);
        c8 _path[RDE_MAX_PATH];
        fude_zoom_page_audio_rec_path(_page->canvas, _number, _path, sizeof(_path));
        fude_bytes _b = fude_bytes_new(_seconds * 8200u);
        fude_zoom_adpcm_state _st = { 0 };
        const u32 _blocks = _seconds * FUDE_ZOOM_AUDIO_RATE / FUDE_ZOOM_ADPCM_FRAMES;
        i16 _pcm[FUDE_ZOOM_ADPCM_FRAMES];
        u8  _block[FUDE_ZOOM_ADPCM_BLOCK];
        for(u32 _k = 0; _k < _blocks; _k++) {
            for(u32 _i = 0; _i < FUDE_ZOOM_ADPCM_FRAMES; _i++) {
                const f64 _t = (f64)(_k * FUDE_ZOOM_ADPCM_FRAMES + _i) / (f64)FUDE_ZOOM_AUDIO_RATE;
                _pcm[_i] = (i16)(6000.0 * rde_math_sin_f64(2.0 * RDE_PI * 330.0 * _t));
            }
            fude_zoom_adpcm_encode(&_st, _pcm, FUDE_ZOOM_ADPCM_FRAMES, _block);
            fude_put_data(&_b, _block, FUDE_ZOOM_ADPCM_BLOCK);
        }
        fude_bytes_write_and_free(&_b, _path, NULL);
        fude_zoom_audio_find(&_page->audio, _number)->frames = _blocks * FUDE_ZOOM_ADPCM_FRAMES;
        u32 _alive = 0;
        for(u32 _o = 0; _o < fude_zoom_scene_object_count(&_page->scene); _o++) {
            const fude_zoom_object* _ob = fude_zoom_scene_object(&_page->scene, _o);
            _alive += (_ob->flags & FUDE_ZOOM_FLAG_ALIVE) && _ob->kind != FUDE_ZOOM_KIND_FRAME && _ob->kind != FUDE_ZOOM_KIND_LAYER ? 1u : 0u;
        }
        u32 _k = 0;
        for(u32 _o = 0; _o < fude_zoom_scene_object_count(&_page->scene); _o++) {
            const fude_zoom_object* _ob = fude_zoom_scene_object(&_page->scene, _o);
            if((_ob->flags & FUDE_ZOOM_FLAG_ALIVE) && _ob->kind != FUDE_ZOOM_KIND_FRAME && _ob->kind != FUDE_ZOOM_KIND_LAYER) {
                fude_zoom_audio_add_link(&_page->audio, _ob->id, _number, (_k++ + 1u) * _seconds * 1000u / (_alive + 1u));
            }
        }
        fude_zoom_page_audio_keep(_page);
    } else if(_what == 1u) {
        _page->audio_panel = true;
        _page->strip_open  = false;
    } else if(_what == 2u) {
        const u32 _n = (u32)rde_arr_length(&_page->audio.recordings);
        if(_n > 0u && fude_zoom_page_audio_load(_page, ((const fude_zoom_recording*)_page->audio.recordings.memory)[_n - 1u].number, false)) {
            fude_zoom_voice_seek(&_page->player, (u32)((u64)_value * _page->player.rate / 1000u));
            _page->audio_faint_at = FUDE_ZOOM_NONE;
        }
    } else if(_what == 3u) {
        fude_zoom_page_audio_toggle(_page);   // (the microphone's chip pressed)
    } else if(_what >= 4u && _what <= 8u) {
        // (speech: 4 the model downloaded, 5 the newest recording transcribed, 6 written as text, 7 as handwriting,
        // 8 Live text on)
        const u32 _n = (u32)rde_arr_length(&_page->audio.recordings);
        const u32 _newest = _n > 0u ? ((const fude_zoom_recording*)_page->audio.recordings.memory)[_n - 1u].number : 0u;
        if(_what == 4u) {
            fude_zoom_page_listen_download(_page);
        } else if(_what == 5u && _newest != 0u) {
            fude_zoom_page_heard_of(_page, _newest, true)->wanted = true;
        } else if(_what == 6u && _newest != 0u) {
            fude_zoom_page_listen_as_text(_page, _newest);
        } else if(_what == 7u && _newest != 0u) {
            fude_zoom_page_listen_as_hand(_page, _newest);
        } else if(_what == 8u) {
            _page->heard_live = true;
        }
    }
}

// See page.h. The WAV is taken to be 16 kHz, mono, 16-bit, as the recordings are: its format chunk is not read, only
// its data chunk found. Its samples are encoded as the recorder does, the last block filled out.
void fude_zoom_page_look_audio_import(fude_zoom_page* _page, const c8* _wav) {
    // A WAV file (16 kHz, mono, 16-bit) made a recording of the canvas, as if it had been recorded here.
    u32 _size = 0;
    u8* _data = fude_file_read(_wav, &_size);
    if(_data == NULL || _size < 44u) {
        fude_file_free(_data);   // (NULL is fine)
        return;
    }
    // Its 'data' chunk, whatever comes before it. The walk counts in 64 bits: a chunk's length near 4 GB (a damaged
    // file's) steps past the end instead of wrapping round to the start and walking for ever.
    u64 _at = 12u;
    u32 _from = 0u, _len = 0u;
    while(_at + 8u <= (u64)_size) {
        const u32 _c = (u32)_at;
        const u32 _n = (u32)_data[_c + 4u] | ((u32)_data[_c + 5u] << 8) | ((u32)_data[_c + 6u] << 16) | ((u32)_data[_c + 7u] << 24);
        if(rde_memcmp(_data + _c, "data", 4) == 0) {
            _from = _c + 8u;
            _len  = _n < _size - _from ? _n : _size - _from;
            break;
        }
        _at += 8u + (u64)_n + (_n & 1u);
    }
    const u32 _frames = _len / 2u;
    const u32 _number = fude_zoom_audio_begin(&_page->audio, (u64)rde_date_now_unix_seconds(), FUDE_ZOOM_AUDIO_RATE);
    c8 _path[RDE_MAX_PATH];
    fude_zoom_page_audio_rec_path(_page->canvas, _number, _path, sizeof(_path));
    fude_bytes _b = fude_bytes_new(_frames / 2u + 1024u);
    fude_zoom_adpcm_state _st = { 0 };
    i16 _pcm[FUDE_ZOOM_ADPCM_FRAMES];
    u8  _block[FUDE_ZOOM_ADPCM_BLOCK];
    for(u32 _k = 0; _k < _frames; _k += FUDE_ZOOM_ADPCM_FRAMES) {
        const u32 _m = _frames - _k < FUDE_ZOOM_ADPCM_FRAMES ? _frames - _k : FUDE_ZOOM_ADPCM_FRAMES;
        for(u32 _i = 0; _i < _m; _i++) {
            _pcm[_i] = (i16)((u16)_data[_from + (_k + _i) * 2u] | ((u16)_data[_from + (_k + _i) * 2u + 1u] << 8));
        }
        fude_zoom_adpcm_encode(&_st, _pcm, _m, _block);
        fude_put_data(&_b, _block, FUDE_ZOOM_ADPCM_BLOCK);
    }
    fude_bytes_write_and_free(&_b, _path, NULL);
    fude_zoom_audio_find(&_page->audio, _number)->frames = ((_frames + FUDE_ZOOM_ADPCM_FRAMES - 1u) / FUDE_ZOOM_ADPCM_FRAMES) * FUDE_ZOOM_ADPCM_FRAMES;
    fude_zoom_page_audio_keep(_page);
    fude_file_free(_data);
}

// See page.h. 0 and 1: page _i picked up in the strip, and with 1 held over place _j; 2 and up: the pages' action
// _what - 2 on pages _i and _j (fude_zoom_page_pages_do).
void fude_zoom_page_look_strip_do(fude_zoom_page* _page, u8 _what, u32 _i, u32 _j) {
    if(_what >= 2u) {
        fude_zoom_page_pages_do(_page, (u8)(_what - 2u), _i, _j);
        return;
    }
    fude_zoom_page_strip_layout _l;
    fude_zoom_page_strip_lay(_page, &_l);
    _page->strip_pick   = _i;
    _page->strip_lifted = _what == 1u;
    if(_page->strip_lifted) {
        const fude_zoom_box _b = fude_zoom_page_strip_slot(_page, &_l, (f64)_j);
        _page->strip_at   = (rde_vec_2F){ (f32)((_b.min_x + _b.max_x) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) };
        _page->strip_drop = _j;
    }
}

// See page.h. Every tab shown, on family _family.
void fude_zoom_page_look_library(fude_zoom_page* _page, u8 _family) {
    _page->library_open   = true;
    _page->library_tabs   = (1u << FUDE_ZOOM_PAGE_LIBRARY_TABS_N) - 1u;
    _page->library_family = (u8)(_family % FUDE_ZOOM_PAGE_LIBRARY_TABS_N);
    _page->library_scroll = 0.0;
}

// See page.h. The camera is panned so that the view's middle is _dx, _dy screen points from where it was, the symbol
// put there and its text saved, then panned back. In _text, '|' stands for a line break and '~' for '=', which the
// engine would take for its own in a launch argument.
void fude_zoom_page_look_symbol(fude_zoom_page* _page, const c8* _id, const c8* _text, f64 _dx, f64 _dy) {
    const u32 _kind = fude_zoom_symbol_find(_id);
    if(_kind == FUDE_ZOOM_NONE) {
        rde_log_color(RDE_LOG_COLOR_RED, "look: no symbol \"%s\"", _id);
        return;
    }
    fude_zoom_camera_pan(&_page->scene, (fude_zoom_v2){ -_dx, -_dy });   // (put at that point of the screen)
    fude_zoom_page_symbol_put(_page, _kind);
    fude_zoom_page_text_card(_page, false);
    c8 _t[FUDE_ZOOM_SYMBOL_TEXT];
    rde_snprintf(_t, sizeof(_t), "%s", _text);
    for(c8* _c = _t; *_c != 0; _c++) {
        if(*_c == '|') {
            *_c = '\n';   // (a look's lines)
        } else if(*_c == '~') {
            *_c = '=';     // (an argument with "=" the engine reads as its own)
        }
    }
    fude_zoom_page_symbol_text_commit(_page, _t);
    fude_zoom_camera_pan(&_page->scene, (fude_zoom_v2){ _dx, _dy });
    fude_zoom_select_clear(&_page->selection);
}

// See page.h. Every sticky note lassoed, as if just let go where it is, then tidied into its column.
void fude_zoom_page_look_tidy(fude_zoom_page* _page) {
    // Every sticky note lassoed, as if just let go where it is.
    fude_zoom_select_clear(&_page->selection);
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(&_page->scene); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(&_page->scene, _i);
        u8 _style = 0;
        if((_o->flags & FUDE_ZOOM_FLAG_ALIVE) && _o->kind == FUDE_ZOOM_KIND_TEXT && fude_zoom_scene_text(&_page->scene, _i, NULL, NULL, NULL, &_style, NULL, NULL) &&
           _style == FUDE_ZOOM_TEXT_STICKY) {
            rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _i });
        }
    }
    fude_zoom_select_update(&_page->selection, &_page->scene);
    fude_zoom_page_kanban_tidy(_page);
}

// See page.h.
void fude_zoom_page_look_map(fude_zoom_page* _page) {
    fude_zoom_page_map_open(_page);
}

// See page.h.
void fude_zoom_page_look_area(fude_zoom_page* _page, const c8* _name) {
    fude_zoom_page_area_commit(_page, _name);
    fude_zoom_select_clear(&_page->selection);
}

// See page.h. _spec is "mode:a:b[:press]". The Curve tool is taken up and the point card opened first when they are
// not.
void fude_zoom_page_look_point(fude_zoom_page* _page, const c8* _spec) {
    c8 _a[32] = { 0 }, _b[32] = { 0 };
    u32 _mode = 0, _press = 0;
    if(rde_sscanf(_spec, "%u:%31[^:]:%31[^:]:%u", &_mode, _a, _b, &_press) < 3) {
        return;
    }
    if(!_page->curve_tool) {
        _page->curve_tool  = true;
        _page->curve_count = 0;
        _page->shape_tool  = 0;
        fude_zoom_page_pen_under(_page);
    }
    if(!fude_zoom_point_shown(&_page->point_form)) {
        fude_zoom_page_on_type_chip(NULL, NULL, NULL);
    }
    fude_zoom_point_look(&_page->point_form, (u8)_mode, _a, _b, (u8)_press);
}

// See page.h. "edit" opens the card on the newest sheet, lassoed. Otherwise the card opens for a new one, and _spec is
// up to five fields split by ':' (the paper's number, landscape when not 0, the scale's number, the width and the
// height), with '!' to press Insert; a '-' with '!' leaves the new sheet unlassoed.
void fude_zoom_page_look_sheet(fude_zoom_page* _page, const c8* _spec) {
    if(rde_strcmp(_spec, "edit") == 0) {
        for(u32 _i = fude_zoom_scene_object_count(&_page->scene); _i-- > 0u;) {
            if(fude_zoom_page_sheet_of(_page, _i, NULL, NULL, NULL)) {
                fude_zoom_select_clear(&_page->selection);
                rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _i });
                fude_zoom_select_update(&_page->selection, &_page->scene);
                fude_zoom_page_sheet_start(_page, _i);
                return;
            }
        }
        return;
    }
    fude_zoom_page_sheet_start(_page, FUDE_ZOOM_NONE);
    c8 _parts[5][24];
    rde_memset(_parts, 0, sizeof(_parts));
    u32 _n = 0;
    for(const c8* _p = _spec; *_p != 0 && *_p != '!' && _n < 5u;) {
        const usize _len = rde_strcspn(_p, ":!");
        rde_snprintf(_parts[_n++], sizeof(_parts[0]), "%.*s", (int)_len, _p);
        _p += _len;
        if(*_p == ':') {
            _p++;
        }
    }
    const i32 _paper = _n > 0u ? (i32)rde_strtol(_parts[0], NULL, 10) : -1;
    fude_zoom_sheet_look(&_page->sheet_form, _paper >= 0 ? (u32)_paper : FUDE_ZOOM_NONE, _n > 1u && (i32)rde_strtol(_parts[1], NULL, 10) != 0, _n > 2u ? (u32)(i32)rde_strtol(_parts[2], NULL, 10) : 0u,
                         _n > 3u ? _parts[3] : NULL, _n > 4u ? _parts[4] : NULL, rde_strchr(_spec, '!') != NULL);
    if(rde_strchr(_spec, '-') != NULL && rde_strchr(_spec, '!') != NULL) {
        fude_zoom_select_clear(&_page->selection);   // (as it lies, not lassoed)
        _page->line_chip_until = 0.0;
    }
}

// See page.h. The size card opened, its fields (width, height, angle, x, y) typed from _spec, split by ';' (an empty
// one left as it is); '~' unlocks the proportions and '!' presses Apply.
void fude_zoom_page_look_size(fude_zoom_page* _page, const c8* _spec) {
    fude_zoom_page_size_start(_page);
    if(!fude_zoom_size_shown(&_page->size_form)) {
        return;
    }
    c8 _parts[FUDE_ZOOM_SIZE_FIELDS][32];
    const c8* _typed[FUDE_ZOOM_SIZE_FIELDS] = { NULL, NULL, NULL, NULL, NULL };
    const c8* _p = _spec;
    for(u32 _i = 0; _i < FUDE_ZOOM_SIZE_FIELDS && *_p != 0 && *_p != '!' && *_p != '~'; _i++) {
        const usize _len = rde_strcspn(_p, ";!~");
        rde_snprintf(_parts[_i], sizeof(_parts[_i]), "%.*s", (int)_len, _p);
        _typed[_i] = _len > 0 ? _parts[_i] : NULL;
        _p += _len;
        if(*_p == ';') {
            _p++;
        }
    }
    fude_zoom_size_look(&_page->size_form, _typed, rde_strchr(_spec, '~') != NULL ? 0 : -1, rde_strchr(_spec, '!') != NULL);
}

// See page.h. _spec is the mode, then the card's fields, then the mirror's axis and Keep, split by ';' (an empty one
// left as it is), with '!' to press Apply.
void fude_zoom_page_look_repeat(fude_zoom_page* _page, const c8* _spec) {
    c8 _parts[FUDE_ZOOM_REPEAT_FIELDS + 3u][32];
    rde_memset(_parts, 0, sizeof(_parts));
    u32 _n = 0;
    for(const c8* _p = _spec; _n < FUDE_ZOOM_REPEAT_FIELDS + 3u;) {
        const usize _len = rde_strcspn(_p, ";!");
        rde_snprintf(_parts[_n++], sizeof(_parts[0]), "%.*s", (int)_len, _p);
        _p += _len;
        if(*_p != ';') {
            break;
        }
        _p++;
    }
    const u8 _mode = (u8)(i32)rde_strtol(_parts[0], NULL, 10);
    fude_zoom_page_repeat_start(_page, _mode);
    if(!fude_zoom_repeat_shown(&_page->repeat_form)) {
        return;
    }
    const c8* _typed[FUDE_ZOOM_REPEAT_FIELDS];
    for(u32 _i = 0; _i < FUDE_ZOOM_REPEAT_FIELDS; _i++) {
        _typed[_i] = 1u + _i < _n && _parts[1u + _i][0] != 0 ? _parts[1u + _i] : NULL;
    }
    const u32 _ax = 1u + FUDE_ZOOM_REPEAT_FIELDS, _kp = _ax + 1u;
    fude_zoom_repeat_look(&_page->repeat_form, _mode, _typed, _n > _ax && _parts[_ax][0] != 0 ? (i32)rde_strtol(_parts[_ax], NULL, 10) : -1, _n > _kp && _parts[_kp][0] != 0 ? (i32)rde_strtol(_parts[_kp], NULL, 10) : -1,
                          rde_strchr(_spec, '!') != NULL);
}

// See page.h. Present started, and gone on to step _step when it has one.
void fude_zoom_page_look_present(fude_zoom_page* _page, u32 _step) {
    fude_zoom_page_present_start(_page);
    if(_page->presenting && _step < _page->present_count && _step != _page->present_step) {
        fude_zoom_page_present_go(_page, _step);
    }
}

// See page.h.
void fude_zoom_page_look_keep(fude_zoom_page* _page, const c8* _name) {
    fude_zoom_page_piece_commit(_page, _name);
}

// See page.h.
void fude_zoom_page_look_pieces(fude_zoom_page* _page) {
    fude_zoom_page_pieces(_page);
    _page->pieces_open = true;
}

// See page.h. The library opened on Custom first (with the circuits' tabs when it was shut). 0 taps tile _i, 1 opens
// its card, and 2 to 4 are its card's Edit (or Inside), Rename (to _name) and Delete.
void fude_zoom_page_look_custom(fude_zoom_page* _page, u32 _i, u32 _action, const c8* _name) {
    if(!_page->library_open) {
        fude_zoom_page_library_open_tabs(_page, FUDE_ZOOM_PAGE_LIBRARY_CIRCUITS);
    }
    _page->library_family = FUDE_ZOOM_PAGE_LIBRARY_CUSTOM;
    if(_i >= fude_zoom_page_custom_items(_page)) {
        rde_log_color(RDE_LOG_COLOR_RED, "look: no Custom tile %u (%u)", _i, (u32)rde_arr_length(&_page->custom_items));
        return;
    }
    const u32 _item = ((const u32*)_page->custom_items.memory)[_i];
    if(_action == 0u) {
        _page->library_open = false;
        fude_zoom_page_custom_tap(_page, _item);
    } else if(_action == 1u) {
        fude_zoom_page_custom_card(_page, _item);
    } else if(fude_zoom_page_custom_of(_page, _item, &_page->custom_kind, _page->custom_key)) {
        if(_action == 2u) {
            fude_zoom_page_custom_chosen(_page, 0u);
        } else if(_action == 3u) {
            fude_zoom_page_custom_rename(_page, _name != NULL ? _name : "");
        } else if(_action == 4u) {
            fude_zoom_page_custom_delete(_page, _page->custom_kind, _page->custom_key);
        }
    }
}

// See page.h.
void fude_zoom_page_look_inside(fude_zoom_page* _page) {
    fude_zoom_page_inside_selection(_page->app, NULL, 0u);
}

// See page.h. Logs when Back is not the page's to take.
void fude_zoom_page_look_back(fude_zoom_page* _page) {
    if(!fude_zoom_kind_back(_page->app)) {
        rde_log_color(RDE_LOG_COLOR_YELLOW, "look: Back is the app's here");
    }
}

// See page.h.
void fude_zoom_page_look_piece_put(fude_zoom_page* _page, u32 _i) {
    fude_zoom_page_piece_put(_page, _i);
}

// See page.h.
void fude_zoom_page_look_stencil(fude_zoom_page* _page, u32 _i) {
    fude_zoom_page_piece_stencil(_page, _i);
}

// See page.h. The laser pen pressed left of the middle and drawn across most of the screen in a wave, without
// lifting.
void fude_zoom_page_look_laser(fude_zoom_page* _page) {
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    fude_zoom_page_present_pen_down(_page, (rde_vec_2F){ (f32)(-_half.x * 0.4), (f32)(_half.y * 0.1) });
    for(u32 _i = 1; _i <= 40u; _i++) {
        const f32 _t = (f32)_i / 40.0f;
        fude_zoom_page_present_pen_moved(_page, (rde_vec_2F){ (f32)(-_half.x * 0.4 + _half.x * 0.8 * _t), (f32)(_half.y * 0.1 + rde_math_sin_f64(_t * 6.28) * 60.0) });
    }
}

// See page.h.
void fude_zoom_page_look_kanban_card(fude_zoom_page* _page, u32 _preset, b8 _extra, b8 _insert) {
    fude_zoom_page_kanban_card(_page);
    fude_zoom_kanban_look(&_page->kanban_form, _preset, _extra, _insert);
}

// See page.h. A template's columns made into a board, unlassoed.
void fude_zoom_page_look_kanban(fude_zoom_page* _page, u32 _preset) {
    c8 _text[512];
    fude_zoom_page_kanban_preset(_preset, _text, sizeof(_text));
    fude_zoom_page_kanban_commit(_page, _text);
    fude_zoom_select_clear(&_page->selection);
}

// See page.h: the Shapes tool's choice, made through its own callback.
void fude_zoom_page_look_shapes_choice(fude_zoom_page* _page, u32 _index) {
    RDE_UNUSED(_page);
    fude_zoom_shapes_choose(NULL, _index);
}

// See page.h. The shapes are measured in millimetres on the canvas; nothing is made when the frame has no true size.
void fude_zoom_page_look_combine(fude_zoom_page* _page, u32 _op) {
    // A rectangle 80 × 50 mm and a circle 25 round over its right side, lassoed; then combined (_op: 0 join, 1 cut out,
    // 2 overlap; 9: Combine's card up; 10 + k: the same, the rectangle filled).
    fude_zoom_scene* _s    = &_page->scene;
    const f64        _near = 4.0 / _s->camera.z;
    const u32 _frame = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _s->camera.at.x - _near, _s->camera.at.y - _near, _s->camera.at.x + _near, _s->camera.at.y + _near });
    const fude_zoom_sim _to = fude_zoom_page_frame_sim(_page, _frame);
    const fude_zoom_v2 _mid = fude_zoom_sim_apply(fude_zoom_sim_inverse(_to), (fude_zoom_v2){ 0.0, 0.0 });
    const f64 _per = fude_zoom_page_home_units(_page, _frame) * FUDE_ZOOM_PAGE_MM_PER_UNIT;
    if(!(_per > 0.0)) {
        return;
    }
    const f64 _u = 1.0 / _per;
    const f32 _r = (f32)(1.0 / rde_math_max_f64(fude_zoom_sim_scale(_to), 1e-300));
    const b8 _filled = _op >= 10u;
    u32 _l[2];
    _l[0] = fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ { _mid.x - 15.0 * _u, _mid.y }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, (const f64[3]){ 40.0 * _u, 25.0 * _u, 0.0 }, 3u,
                                      _page->app->ink->color, _r, _filled ? FUDE_ZOOM_FLAG_FILLED : 0u, 0);
    _l[1] = fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ { _mid.x + 25.0 * _u, _mid.y + 10.0 * _u }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ELLIPSE, (const f64[2]){ 25.0 * _u, 25.0 * _u }, 2u,
                                      _page->app->ink->color, _r, 0u, 0);
    fude_zoom_history_push(_s, _frame, fude_zoom_box_union(fude_zoom_scene_object(_s, _l[0])->box, fude_zoom_scene_object(_s, _l[1])->box), NULL, 0, _l, 2u);
    fude_zoom_select_clear(&_page->selection);
    rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _l[0] });
    rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _l[1] });
    fude_zoom_select_update(&_page->selection, _s);
    const u32 _k = _op % 10u;
    if(_k == 9u) {
        fude_zoom_page_combine_selection(_page->app, NULL, 0u);
    } else if(_k < 3u) {
        static const u8 _ops[3] = { FUDE_ZOOM_CUT_UNION, FUDE_ZOOM_CUT_SUBTRACT, FUDE_ZOOM_CUT_INTERSECT };
        fude_zoom_page_combine(_page, _ops[_k]);
    }
}

// A circuit's part put _x, _y screen points from the view's middle, turned _turn degrees (its size as it comes).
RDE_INTERNAL u32 fude_zoom_page_look_part(fude_zoom_page* _page, u32 _frame, const c8* _id, f64 _x, f64 _y, f64 _turn, const c8* _text, rde_arr* _born) {
    fude_zoom_scene* _s = &_page->scene;
    const u32 _kind = fude_zoom_symbol_find(_id);
    const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of(_kind);
    if(_info == NULL) {
        return FUDE_ZOOM_NONE;
    }
    const fude_zoom_sim _fs = fude_zoom_page_frame_sim(_page, _frame);
    const f64 _k = rde_math_max_f64(fude_zoom_sim_scale(_fs), 1e-300);
    const fude_zoom_v2 _at = fude_zoom_sim_apply(fude_zoom_sim_inverse(_fs), (fude_zoom_v2){ _x, _y });
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS + 200];
    const u32 _count = fude_zoom_symbol_numbers(_n, _kind, (f64)_info->w * 0.5 / _k, (f64)_info->h * 0.5 / _k, 12.0 / _k, _text);
    const u32 _made = fude_zoom_scene_add_shape_fill(_s, _frame, (fude_zoom_place){ _at, _turn * (RDE_PI / 180.0) - rde_math_atan2_f64(_fs.b, _fs.a), 1.0 }, FUDE_ZOOM_SHAPE_SYMBOL,
                                                     _n, _count, _page->app->ink->color, (f32)(1.0 / _k), FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN,
                                                     FUDE_THEME_PAGE_FILL, 0);
    rde_arr_add(_born, (any)&_made);
    return _made;
}

// A wire from part _a's pin _pa to part _b's pin _pb, routed as the Circuit tool routes one, its index added to _born.
// Nothing when either part is missing or has no such pin.
RDE_INTERNAL void fude_zoom_page_look_wire(fude_zoom_page* _page, u32 _frame, u32 _a, u32 _pa, u32 _b, u32 _pb, rde_arr* _born) {
    fude_zoom_scene* _s = &_page->scene;
    fude_zoom_v2 _p0, _p1, _r[6];
    if(_a == FUDE_ZOOM_NONE || _b == FUDE_ZOOM_NONE || !fude_zoom_part_pin_at(_s, _a, _pa, &_p0) || !fude_zoom_part_pin_at(_s, _b, _pb, &_p1)) {
        return;
    }
    const u32 _m = fude_zoom_wire_route(_p0, fude_zoom_part_side_at(_s, _a, _pa), _p1, fude_zoom_part_side_at(_s, _b, _pb), fude_zoom_part_unit(_s, _a), _r);
    const f64 _k = fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, _frame));
    const u32 _w = fude_zoom_wire_add(_s, _frame, _r, _m, _a, (i32)_pa, _b, (i32)_pb, _page->app->ink->color, (f32)(0.9 / _k));
    rde_arr_add(_born, (any)&_w);
}

RDE_INTERNAL u32 fude_zoom_page_look_from = 0;   // (the objects the last demo drew: from this one on)

// One of the examples (examples.h) drawn round the view's middle in frame _frame, as Insert → Examples draws it.
RDE_INTERNAL void fude_zoom_page_look_example(fude_zoom_page* _page, u32 _frame, u32 _example, rde_arr* _born) {
    fude_zoom_example_build(&_page->scene, _frame, fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _frame)), FUDE_THEME_INK, _example,
                            fude_zoom_page_example_title, _born);
}

// See page.h. Demo 7 clears away what the demo before drew (from fude_zoom_page_look_from on) and draws in its place.
// What is drawn is one undo step.
void fude_zoom_page_look_circuit(fude_zoom_page* _page, u32 _demo) {
    // 1: a battery, a switch (on), a resistor, an LED; 2: an Arduino Uno blinking D13 into an LED; 3: a 555 flashing
    // an LED; 4: a breadboard with a resistor and an LED on it, and a battery beside it, not wired; 5: a full adder
    // drawn of gates, its name written by it; 6: a 74HC161 on a breadboard counting a clock into four LEDs; 7: four
    // "Full adder"s (made of 5) joined into a 4-bit adder, switches and probes on it. Then the topic Electronics.
    fude_zoom_scene* _s    = &_page->scene;
    if(_demo == 7u) {
        // (what the demo before drew cleared away: this one where it was)
        for(u32 _i = fude_zoom_page_look_from; _i < fude_zoom_scene_object_count(_s); _i++) {
            fude_zoom_scene_set_alive(_s, _i, false);
        }
        fude_zoom_select_clear(&_page->selection);
    }
    fude_zoom_page_look_from = fude_zoom_scene_object_count(_s);
    const f64        _near = 4.0 / _s->camera.z;
    const u32 _f = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _s->camera.at.x - _near, _s->camera.at.y - _near, _s->camera.at.x + _near, _s->camera.at.y + _near });
    rde_arr _born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    if(_demo == 1u) {
        fude_zoom_page_look_example(_page, _f, FUDE_ZOOM_EXAMPLE_TORCH, &_born);
    } else if(_demo == 2u) {
        const u32 _uno = fude_zoom_page_look_part(_page, _f, "Arduino Uno", -150, 0, 0, "Arduino Uno\nD13 blink", &_born);
        const u32 _r   = fude_zoom_page_look_part(_page, _f, "resistor", 120, 160, 0, "220", &_born);
        const u32 _led = fude_zoom_page_look_part(_page, _f, "LED", 260, 60, -90, "green", &_born);
        fude_zoom_page_look_wire(_page, _f, _uno, 15, _r, 0, &_born);    // (D13)
        fude_zoom_page_look_wire(_page, _f, _r, 1, _led, 0, &_born);
        fude_zoom_page_look_wire(_page, _f, _led, 1, _uno, 14, &_born);  // (GND)
    } else if(_demo == 3u) {
        fude_zoom_page_look_example(_page, _f, FUDE_ZOOM_EXAMPLE_FLASHER, &_born);
    } else if(_demo == 4u) {
        const u32 _bb = fude_zoom_page_look_part(_page, _f, "breadboard", 0, 0, 0, "", &_born);
        fude_zoom_v2 _h[4];
        // (four of its holes, by their pins' numbers, a row's columns times the row plus the column: two in row 2 for
        // the resistor, two in row 9 for the LED)
        const u32 _cols = FUDE_ZOOM_BREADBOARD_COLS;
        const u32 _idx[4] = { 2u * _cols + 2u, 2u * _cols + 8u, 9u * _cols + 8u, 9u * _cols + 12u };
        const fude_zoom_sim _fs = fude_zoom_page_frame_sim(_page, _f);
        for(u32 _i = 0; _i < 4u; _i++) {
            fude_zoom_part_pin_at(_s, _bb, _idx[_i], &_h[_i]);
            _h[_i] = fude_zoom_sim_apply(_fs, _h[_i]);
        }
        // (the resistor over the first two holes, the LED over the last two, 10 points right of their middle; the
        // battery beside the board)
        fude_zoom_page_look_part(_page, _f, "resistor", (_h[0].x + _h[1].x) * 0.5, _h[0].y, 0, "220", &_born);
        fude_zoom_page_look_part(_page, _f, "LED", (_h[2].x + _h[3].x) * 0.5 + 10.0, _h[2].y, 0, "red", &_born);
        const u32 _bat = fude_zoom_page_look_part(_page, _f, "battery", -260, 0, 0, "5V", &_born);
        RDE_UNUSED(_bat);
    } else if(_demo == 5u) {
        fude_zoom_page_look_example(_page, _f, FUDE_ZOOM_EXAMPLE_FULL_ADDER, &_born);   // (its title its name: Make part's)
    } else if(_demo == 6u) {
        fude_zoom_page_look_example(_page, _f, FUDE_ZOOM_EXAMPLE_COUNTER, &_born);
    } else if(_demo == 7u) {
        // (four of the full adder made of demo 5, a carry along them; switches into them, probes out)
        u32 _fa[4];
        const u32 _ci = fude_zoom_page_look_part(_page, _f, "logic input", -330, 330, 0, "0", &_born);
        for(u32 _i = 0; _i < 4u; _i++) {
            const f64 _y = 240.0 - (f64)_i * 160.0;
            _fa[_i] = fude_zoom_page_look_sized(_page, _f, "custom part", 0, _y, 0, 110, 100, "Full adder", &_born);
            const u32 _a = fude_zoom_page_look_part(_page, _f, "logic input", -330, _y + 40.0, 0, (_i & 1u) ? "1" : "0", &_born);
            const u32 _b = fude_zoom_page_look_part(_page, _f, "logic input", -330, _y - 10.0, 0, "1", &_born);
            const u32 _p = fude_zoom_page_look_part(_page, _f, "logic probe", 260, _y, 0, "", &_born);
            fude_zoom_page_look_wire(_page, _f, _a, 0, _fa[_i], 0, &_born);
            fude_zoom_page_look_wire(_page, _f, _b, 0, _fa[_i], 1, &_born);
            fude_zoom_page_look_wire(_page, _f, _fa[_i], 3, _p, 0, &_born);
            fude_zoom_page_look_wire(_page, _f, _i == 0u ? _ci : _fa[_i - 1u], _i == 0u ? 0u : 4u, _fa[_i], 2, &_born);
        }
        const u32 _co = fude_zoom_page_look_part(_page, _f, "logic probe", 260, -400, 0, "", &_born);
        fude_zoom_page_look_wire(_page, _f, _fa[3], 4, _co, 0, &_born);
    }
    if(rde_arr_length(&_born) > 0u) {
        fude_zoom_history_push(_s, _f, fude_zoom_scene_object(_s, ((const u32*)_born.memory)[0])->box, NULL, 0, (const u32*)_born.memory, (u32)rde_arr_length(&_born));
    }
    rde_arr_free(&_born);
    _page->topic = FUDE_ZOOM_TOPIC_ELECTRONICS;
}

// A part _w × _h points (0: as it comes) put _x, _y screen points from the view's middle, turned _turn degrees.
// See page_parts.h. As fude_zoom_page_look_part, at the size asked for. Returns the part's index, or FUDE_ZOOM_NONE
// when _id is not a symbol.
u32 fude_zoom_page_look_sized(fude_zoom_page* _page, u32 _frame, const c8* _id, f64 _x, f64 _y, f64 _turn, f64 _w, f64 _h, const c8* _text, rde_arr* _born) {
    fude_zoom_scene* _s = &_page->scene;
    const u32 _kind = fude_zoom_symbol_find(_id);
    const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of(_kind);
    if(_info == NULL) {
        return FUDE_ZOOM_NONE;
    }
    const fude_zoom_sim _fs = fude_zoom_page_frame_sim(_page, _frame);
    const f64 _k = rde_math_max_f64(fude_zoom_sim_scale(_fs), 1e-300);
    const fude_zoom_v2 _at = fude_zoom_sim_apply(fude_zoom_sim_inverse(_fs), (fude_zoom_v2){ _x, _y });
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS + 200];
    const u32 _count = fude_zoom_symbol_numbers(_n, _kind, (_w > 0.0 ? _w : (f64)_info->w) * 0.5 / _k, (_h > 0.0 ? _h : (f64)_info->h) * 0.5 / _k, 12.0 / _k, _text);
    const u32 _made = fude_zoom_scene_add_shape_fill(_s, _frame, (fude_zoom_place){ _at, _turn * (RDE_PI / 180.0) - rde_math_atan2_f64(_fs.b, _fs.a), 1.0 }, FUDE_ZOOM_SHAPE_SYMBOL,
                                                     _n, _count, _page->app->ink->color, (f32)(1.0 / _k), FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN,
                                                     FUDE_THEME_PAGE_FILL, 0);
    rde_arr_add(_born, (any)&_made);
    return _made;
}

// See page.h. Each demo is one of the examples (examples.h), drawn as one undo step.
void fude_zoom_page_look_mech(fude_zoom_page* _page, u32 _demo) {
    // 1: a crank and rocker on a motor; 2: a gear train on a motor; 3: a pendulum and a weight on a spring; 4: a
    // pulley; 5: a slider-crank; 6: a rack and pinion; 7: drawings made bodies; 8: a gear train on a rack (Borja's).
    // Topic Mechanisms.
    fude_zoom_scene* _s    = &_page->scene;
    const f64        _near = 4.0 / _s->camera.z;
    const u32 _f = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _s->camera.at.x - _near, _s->camera.at.y - _near, _s->camera.at.x + _near, _s->camera.at.y + _near });
    rde_arr _born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    static const u32 _examples[9] = { 0u, FUDE_ZOOM_EXAMPLE_CRANK, FUDE_ZOOM_EXAMPLE_GEAR_TRAIN, FUDE_ZOOM_EXAMPLE_PENDULUM, FUDE_ZOOM_EXAMPLE_PULLEYS,
                                      FUDE_ZOOM_EXAMPLE_PISTON, FUDE_ZOOM_EXAMPLE_RACK, FUDE_ZOOM_EXAMPLE_BODIES, FUDE_ZOOM_EXAMPLE_GEARS_RACK };
    if(_demo >= 1u && _demo <= 8u) {
        fude_zoom_page_look_example(_page, _f, _examples[_demo], &_born);
    }
    if(rde_arr_length(&_born) > 0u) {
        fude_zoom_history_push(_s, _f, fude_zoom_scene_object(_s, ((const u32*)_born.memory)[0])->box, NULL, 0, (const u32*)_born.memory, (u32)rde_arr_length(&_born));
    }
    rde_arr_free(&_born);
    _page->topic = FUDE_ZOOM_TOPIC_MECHANISMS;
}

// See page.h. Steps 0 and 1 zoom out until the room fits most of the screen, then draw it; any later step labels the
// room as the Plan tool's Room does, tapped 300 mm left of and 400 mm below the middle.
void fude_zoom_page_look_plan(fude_zoom_page* _page, u32 _step) {
    // A room 4 × 3 m (its walls 10 cm thick, the top one 25), a door, a window, a double bed, a socket, a ceiling
    // light; then (_step 2) labelled. The view zoomed out to it first.
    fude_zoom_scene* _s = &_page->scene;
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    if(_step <= 1u) {
        const f64 _want = 4600.0 / (_half.x * 1.6);   // (mm a point: the room across most of the screen)
        for(u32 _i = 0; _i < 64u && fude_zoom_page_mm_per_point(_page) < _want * 0.98; _i++) {
            fude_zoom_camera_zoom_at(_s, (fude_zoom_v2){ 0.0, 0.0 }, rde_math_max_f64(fude_zoom_page_mm_per_point(_page) / _want, 0.5));
            fude_zoom_camera_settle(_s, _half);
        }
        const f64 _near = 4.0 / _s->camera.z;
        const u32 _f = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _s->camera.at.x - _near, _s->camera.at.y - _near, _s->camera.at.x + _near, _s->camera.at.y + _near });
        const fude_zoom_sim _fs = fude_zoom_page_frame_sim(_page, _f);
        const f64 _per = fude_zoom_page_home_units(_page, _f) * FUDE_ZOOM_PAGE_MM_PER_UNIT;   // mm a unit
        const fude_zoom_v2 _o = fude_zoom_sim_apply(fude_zoom_sim_inverse(_fs), (fude_zoom_v2){ 0.0, 0.0 });
        rde_arr _born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        // (corners from the room's bottom left, mm; the view's middle its middle)
        const f64 _c[5][2] = { { 0, 0 }, { 4000, 0 }, { 4000, 3000 }, { 0, 3000 }, { 0, 0 } };
        for(u32 _i = 0; _i < 4u; _i++) {
            const fude_zoom_v2 _a = { _o.x + (_c[_i][0] - 2000.0) / _per, _o.y + (_c[_i][1] - 1500.0) / _per };
            const f64 _n[3] = { (_c[_i + 1u][0] - _c[_i][0]) / _per, (_c[_i + 1u][1] - _c[_i][1]) / _per, (_i == 2u ? 250.0 : 100.0) / _per };
            const u32 _w = fude_zoom_scene_add_shape(_s, _f, (fude_zoom_place){ _a, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_WALL, _n, 3u, _page->app->ink->color, (f32)(0.3 / fude_zoom_sim_scale(_fs)),
                                                     FUDE_ZOOM_FLAG_FILLED, 0);
            rde_arr_add(&_born, (any)&_w);
        }
        rde_arr_free(&_born);
        // (its parts where they go: screen points from the middle, from millimetres)
        const f64 _mpp = fude_zoom_page_mm_per_point(_page);
        struct { const c8* id; f64 x, y, turn; } _put[5] = {
            { "door 80", 1000.0 - 2000.0, 450.0 - 1500.0 - 50.0, 0.0 }, { "window 120", 0.0, 1500.0, 0.0 }, { "double bed", 3200.0 - 2000.0, 1950.0 - 1500.0, 0.0 },
            { "socket", 2300.0 - 2000.0, 2900.0 - 1500.0, 180.0 }, { "ceiling light", 0.0, 0.0, 0.0 } };
        for(u32 _i = 0; _i < 5u; _i++) {
            const u32 _kind = fude_zoom_symbol_find(_put[_i].id);
            fude_zoom_page_symbol_put(_page, _kind);
            if(rde_arr_length(&_page->selection.picks) == 1u) {
                const u32 _made = ((const fude_zoom_pick*)_page->selection.picks.memory)[0].object;
                fude_zoom_place _pl = fude_zoom_scene_place_of(_s, _made);
                _pl.t = fude_zoom_sim_apply(fude_zoom_sim_inverse(_fs), (fude_zoom_v2){ _put[_i].x / _mpp, _put[_i].y / _mpp });
                _pl.rotation += _put[_i].turn * (RDE_PI / 180.0);
                fude_zoom_scene_set_place(_s, _made, _pl);
            }
        }
        fude_zoom_select_clear(&_page->selection);
        _page->topic = FUDE_ZOOM_TOPIC_FLOORPLAN;
    } else {
        fude_zoom_page_room_at(_page, (rde_vec_2F){ -300.0f / (f32)fude_zoom_page_mm_per_point(_page), -400.0f / (f32)fude_zoom_page_mm_per_point(_page) });
    }
}

// See page.h: everything lassoed, then Motion's first choice, Play.
void fude_zoom_page_look_mech_run(fude_zoom_page* _page) {
    fude_zoom_select_all(&_page->selection, &_page->scene, &_page->renderer);   // (everything lassoed, then Play)
    fude_zoom_motion_choose(_page->app, 0u);
}

// See page.h: everything lassoed, then the Circuit tool's Play.
void fude_zoom_page_look_circuit_run(fude_zoom_page* _page) {
    fude_zoom_select_all(&_page->selection, &_page->scene, &_page->renderer);
    fude_zoom_circuit_choose(_page->app, 1u);
}

// A look's: the first symbol _id lassoed alone.
RDE_INTERNAL void fude_zoom_page_look_lasso_symbol(fude_zoom_page* _page, const c8* _id) {
    fude_zoom_scene* _s = &_page->scene;
    const u32 _kind = fude_zoom_symbol_find(_id);
    fude_zoom_select_clear(&_page->selection);
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        f64 _n[1];
        if((_o->flags & FUDE_ZOOM_FLAG_ALIVE) && _o->kind == FUDE_ZOOM_KIND_SHAPE && _o->channels == FUDE_ZOOM_SHAPE_SYMBOL &&
           _kind != FUDE_ZOOM_NONE && fude_zoom_scene_shape_numbers(_s, _i, _n, 1u) == 1u && fude_zoom_symbol_kind_of(_n, 1u) == _kind) {
            rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _i });
            break;
        }
    }
    fude_zoom_select_update(&_page->selection, _s);
}

// See page.h. The first symbol _id lassoed alone, its limits' card opened and filled.
void fude_zoom_page_look_limits(fude_zoom_page* _page, const c8* _id, i32 _preset, u32 _field, const c8* _typed, u32 _unit, u32 _finish) {
    fude_zoom_page_look_lasso_symbol(_page, _id);
    fude_zoom_page_limits_selection(_page->app, NULL, 0u);
    fude_zoom_limits_look(&_page->limits_form, _preset, _field, _typed != NULL && _typed[0] != 0 ? _typed : NULL, _unit, _finish);
}

// See page.h. When Value opens a list of colours instead of the card (an LED's, a probe's), choice _unit of it is
// taken and the list closed.
void fude_zoom_page_look_value(fude_zoom_page* _page, const c8* _id, const c8* _typed, u32 _unit, u32 _way) {
    // The first symbol _id lassoed alone: Value, _typed in unit _unit (way _way), applied.
    fude_zoom_scene* _s = &_page->scene;
    fude_zoom_page_look_lasso_symbol(_page, _id);
    fude_zoom_page_value_selection(_page->app, NULL, 0u);
    if(fude_zoom_choice_shown(&_page->choice_form)) {
        // (a colour's list: its choice _unit, the card closed)
        fude_zoom_choice_close(&_page->choice_form);
        const fude_zoom_part* _part = _page->value_object != FUDE_ZOOM_NONE ? fude_zoom_part_of(_s, _page->value_object) : NULL;
        (_part != NULL && _part->model == FUDE_ZOOM_MODEL_LOGIC_OUT ? fude_zoom_page_value_probe : fude_zoom_page_value_led)(_page, _unit);
        return;
    }
    fude_zoom_value_look(&_page->value_form, _typed, _unit, _way);
}

// See page.h.
void fude_zoom_page_look_examples(fude_zoom_page* _page, u32 _which) {
    fude_zoom_page_examples_chosen(_page, _which);
}

// See page.h.
void fude_zoom_page_look_fps(fude_zoom_page* _page, b8 _on) {
    _page->show_fps = _on;
}

// See page.h. An example past the last does nothing.
void fude_zoom_page_look_draw_example(fude_zoom_page* _page, u32 _example) {
    if(_example < FUDE_ZOOM_EXAMPLE_COUNT) {
        fude_zoom_page_examples_draw(_page, _example);
    }
}

// See page.h. With _delete, the menu's Delete; otherwise the page's menu opened at _screen.
void fude_zoom_page_look_context(fude_zoom_page* _page, rde_vec_2F _screen, b8 _delete) {
    if(_delete) {
        fude_zoom_page_context_delete(_page->app, &_page->app->ui->page, 0u);
    } else {
        fude_zoom_page_context_open(_page, _screen);
    }
}

// See page.h. The press and the drag go to the lasso, and the page's alignment guides are worked out for it, the
// hand kept down.
void fude_zoom_page_look_align(fude_zoom_page* _page, const c8* _id, f64 _dx, f64 _dy) {
    // The first symbol _id lassoed alone, taken by its middle and dragged by _dx, _dy screen points — held there (its
    // lines showing what it lines up with).
    fude_zoom_scene* _s = &_page->scene;
    const u32 _kind = fude_zoom_symbol_find(_id);
    fude_zoom_select_clear(&_page->selection);
    u32 _found = FUDE_ZOOM_NONE;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s) && _found == FUDE_ZOOM_NONE; _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        f64 _n[1];
        if((_o->flags & FUDE_ZOOM_FLAG_ALIVE) && _o->kind == FUDE_ZOOM_KIND_SHAPE && _o->channels == FUDE_ZOOM_SHAPE_SYMBOL &&
           _kind != FUDE_ZOOM_NONE && fude_zoom_scene_shape_numbers(_s, _i, _n, 1u) == 1u && fude_zoom_symbol_kind_of(_n, 1u) == _kind) {
            _found = _i;
        }
    }
    if(_found == FUDE_ZOOM_NONE) {
        return;
    }
    rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _found });
    fude_zoom_select_update(&_page->selection, _s);
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _found);
    const fude_zoom_v2 _mid = fude_zoom_sim_apply(fude_zoom_page_frame_sim(_page, _o->frame), fude_zoom_scene_place_of(_s, _found).t);
    const rde_vec_2F _at = { (f32)_mid.x, (f32)_mid.y };
    fude_zoom_select_down(&_page->selection, _s, _at);
    fude_zoom_select_moved(&_page->selection, (rde_vec_2F){ (f32)(_mid.x + _dx), (f32)(_mid.y + _dy) });
    fude_zoom_page_align(_page);
}

// See page.h. What the last demo drew (from fude_zoom_page_look_from on) lassoed, then Make part.
void fude_zoom_page_look_make_part(fude_zoom_page* _page) {
    // What the last demo drew lassoed, made a part.
    fude_zoom_select_clear(&_page->selection);
    for(u32 _i = fude_zoom_page_look_from; _i < fude_zoom_scene_object_count(&_page->scene); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(&_page->scene, _i);
        if((_o->flags & FUDE_ZOOM_FLAG_ALIVE) && _o->kind != FUDE_ZOOM_KIND_FRAME) {
            rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _i });
        }
    }
    fude_zoom_select_update(&_page->selection, &_page->scene);
    fude_zoom_page_make_part(_page);
}

// See page.h. With _all, everything lassoed first; the body card then filled and, with _apply, applied.
void fude_zoom_page_look_body(fude_zoom_page* _page, b8 _all, u32 _material, b8 _fixed, b8 _apply) {
    if(_all) {
        fude_zoom_select_all(&_page->selection, &_page->scene, &_page->renderer);
    }
    fude_zoom_page_body_selection(_page->app, NULL, 0u);
    fude_zoom_body_look(&_page->body_form, _material, _fixed, _apply ? 1u : 0u);
}

// See page.h. Only while playing, and only for a slider Play shows.
void fude_zoom_page_look_slider(fude_zoom_page* _page, u32 _slider, f64 _t) {
    // Play's slider _slider set _t of the way along.
    if(!_page->play_on || _slider >= _page->play_var_count) {
        return;
    }
    fude_zoom_page_play_layout _l;
    fude_zoom_page_play_lay(_page, &_l);
    const fude_zoom_box _b = _l.sliders[_slider];
    fude_zoom_page_play_slide(_page, _slider, (rde_vec_2F){ (f32)(_b.min_x + (_b.max_x - _b.min_x) * _t), (f32)((_b.min_y + _b.max_y) * 0.5) });
}

// See page.h. The lines are measured in millimetres on the canvas; nothing is made when the frame has no true size. The
// trim taps the upright line 25 mm over the middle, and the chamfer takes the two newest lines.
void fude_zoom_page_look_lines(fude_zoom_page* _page, u32 _step) {
    // A line across (from -80 to -20 mm), an upright one at +40 (from -50 to +50), a circle round (-50, -30) 15 round;
    // the first lassoed: then (1) extended, (2) the upright one's top trimmed away, (3) the two lines' corner chamfered
    // 8.
    fude_zoom_scene* _s    = &_page->scene;
    const f64        _near = 4.0 / _s->camera.z;
    const u32 _frame = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _s->camera.at.x - _near, _s->camera.at.y - _near, _s->camera.at.x + _near, _s->camera.at.y + _near });
    const fude_zoom_sim _to = fude_zoom_page_frame_sim(_page, _frame);
    const fude_zoom_v2 _mid = fude_zoom_sim_apply(fude_zoom_sim_inverse(_to), (fude_zoom_v2){ 0.0, 0.0 });
    const f64 _per = fude_zoom_page_home_units(_page, _frame) * FUDE_ZOOM_PAGE_MM_PER_UNIT;
    const f64 _k   = fude_zoom_sim_scale(_to);
    if(!(_per > 0.0)) {
        return;
    }
    const f64 _u = 1.0 / _per;
    const f32 _r = (f32)(1.0 / rde_math_max_f64(_k, 1e-300));
    const rde_color _ink = _page->app->ink->color;
    u32 _l[3];
    _l[0] = fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ { _mid.x - 80.0 * _u, _mid.y }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, (const f64[2]){ 60.0 * _u, 0.0 }, 2u, _ink, _r, 0u, 0);
    _l[1] = fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ { _mid.x + 40.0 * _u, _mid.y - 50.0 * _u }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, (const f64[2]){ 0.0, 100.0 * _u }, 2u, _ink, _r, 0u, 0);
    _l[2] = fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ { _mid.x - 50.0 * _u, _mid.y - 30.0 * _u }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ELLIPSE, (const f64[2]){ 15.0 * _u, 15.0 * _u }, 2u, _ink, _r, 0u, 0);
    const fude_zoom_box _made = fude_zoom_box_union(fude_zoom_box_union(fude_zoom_scene_object(_s, _l[0])->box, fude_zoom_scene_object(_s, _l[1])->box),
                                                    fude_zoom_scene_object(_s, _l[2])->box);   // (the circle's too)
    fude_zoom_history_push(_s, _frame, _made, NULL, 0, _l, 3u);
    fude_zoom_select_clear(&_page->selection);
    rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _l[0] });
    fude_zoom_select_update(&_page->selection, _s);
    if(_step >= 1u) {
        fude_zoom_page_extend(_page);
    }
    if(_step >= 2u) {
        const fude_zoom_v2 _tap = fude_zoom_sim_apply(_to, (fude_zoom_v2){ _mid.x + 40.0 * _u, _mid.y + 25.0 * _u });
        fude_zoom_page_trim_at(_page, (rde_vec_2F){ (f32)_tap.x, (f32)_tap.y });
    }
    if(_step >= 3u) {
        // (the extended line and what is left of the upright one: a corner)
        u32 _n = 0;
        fude_zoom_select_clear(&_page->selection);
        for(u32 _i = fude_zoom_scene_object_count(_s); _i-- > 0u && _n < 2u;) {
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
            if((_o->flags & FUDE_ZOOM_FLAG_ALIVE) && _o->kind == FUDE_ZOOM_KIND_SHAPE && _o->channels == FUDE_ZOOM_SHAPE_LINE) {
                rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _i });
                _n++;
            }
        }
        fude_zoom_select_update(&_page->selection, _s);
        fude_zoom_page_chamfer(_page, 8.0);
    }
}

// See page.h. Lines measured in millimetres; with _mm over 0, the corner filleted by that much.
void fude_zoom_page_look_fillet(fude_zoom_page* _page, f64 _mm) {
    // Two lines 60 mm long meeting in a corner in the middle of the view (the second drawn towards it), lassoed.
    fude_zoom_scene* _s    = &_page->scene;
    const f64        _near = 4.0 / _s->camera.z;
    const u32 _frame = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _s->camera.at.x - _near, _s->camera.at.y - _near, _s->camera.at.x + _near, _s->camera.at.y + _near });
    const fude_zoom_v2 _mid = fude_zoom_sim_apply(fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _frame)), (fude_zoom_v2){ 0.0, 0.0 });
    const f64 _per = fude_zoom_page_home_units(_page, _frame) * FUDE_ZOOM_PAGE_MM_PER_UNIT;
    const f64 _k   = fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, _frame));
    if(!(_per > 0.0)) {
        return;
    }
    const f64 _arm = 60.0 / _per;
    const fude_zoom_v2 _corner = { _mid.x - _arm * 0.5, _mid.y - _arm * 0.5 };
    const f64 _n1[2] = { _arm, 0.0 }, _n2[2] = { 0.0, -_arm };
    const f32 _r = (f32)(1.0 / rde_math_max_f64(_k, 1e-300));
    u32 _l[2];
    _l[0] = fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ _corner, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, _n1, 2u, _page->app->ink->color, _r, 0u, 0);
    _l[1] = fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ { _corner.x, _corner.y + _arm }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, _n2, 2u, _page->app->ink->color, _r, 0u, 0);
    fude_zoom_history_push(_s, _frame, fude_zoom_box_union(fude_zoom_scene_object(_s, _l[0])->box, fude_zoom_scene_object(_s, _l[1])->box), NULL, 0, _l, 2u);
    fude_zoom_select_clear(&_page->selection);
    rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _l[0] });
    rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _l[1] });
    fude_zoom_select_update(&_page->selection, _s);
    if(_mm > 0.0) {
        fude_zoom_page_fillet(_page, _mm);   // (as typed on the numpad)
    }
}

// See page.h. 0: the board card opened. 3: the card opened again, 18 mm typed as its third size, and oak chosen.
// Otherwise a side (720 × 400 × 18) and a top (800 × 300 × 22) are made side by side, the side lassoed, or both with
// step 2; with step 4 the side walnut and the top oak.
void fude_zoom_page_look_board(fude_zoom_page* _page, u8 _step) {
    if(_step == 0u) {
        fude_zoom_page_board_start(_page, FUDE_ZOOM_NONE);   // its first size asked
        return;
    }
    if(_step == 3u) {
        // Its sizes typed: its name asked, what it is made of under it (oak chosen).
        fude_zoom_page_board_start(_page, FUDE_ZOOM_NONE);
        fude_zoom_numpad_close(&_page->numpad);
        fude_zoom_page_board_typed(_page, "18", _page->units.unit, 18.0, 2u);
        fude_zoom_page_on_material(NULL, NULL, (any)(uintptr_t)FUDE_ZOOM_MATERIAL_OAK);
        return;
    }
    // A side and a top, side by side; the side lassoed (its chip up). Step 4: the side walnut, the top oak.
    fude_zoom_page_board_start(_page, FUDE_ZOOM_NONE);
    fude_zoom_numpad_close(&_page->numpad);
    _page->board_mm[0] = 720.0; _page->board_mm[1] = 400.0; _page->board_mm[2] = 18.0;
    _page->board_material = _step == 4u ? (u8)FUDE_ZOOM_MATERIAL_WALNUT : (u8)FUDE_ZOOM_MATERIAL_NONE;
    fude_zoom_camera_pan(&_page->scene, (fude_zoom_v2){ 180.0, 0.0 });
    fude_zoom_page_board_commit(_page, "Lateral");
    const u32 _side = fude_zoom_scene_object_count(&_page->scene) - 1u;
    fude_zoom_page_board_start(_page, FUDE_ZOOM_NONE);
    fude_zoom_numpad_close(&_page->numpad);
    _page->board_mm[0] = 800.0; _page->board_mm[1] = 300.0; _page->board_mm[2] = 22.0;
    _page->board_material = _step == 4u ? (u8)FUDE_ZOOM_MATERIAL_OAK : (u8)FUDE_ZOOM_MATERIAL_NONE;
    fude_zoom_camera_pan(&_page->scene, (fude_zoom_v2){ -400.0, 0.0 });
    fude_zoom_page_board_commit(_page, "Tapa");
    fude_zoom_camera_pan(&_page->scene, (fude_zoom_v2){ 220.0, 0.0 });
    fude_zoom_select_clear(&_page->selection);
    rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _side });
    if(_step == 2u) {
        rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _side + 1u });   // both
    }
    fude_zoom_select_update(&_page->selection, &_page->scene);
}

// See page.h. The topmost instrument of kind _kind is snapped as if dropped where it lies, from three times the usual
// reach; then, when the offset pad is up, its across and up are set and applied.
void fude_zoom_page_look_anchor(fude_zoom_page* _page, u32 _kind, f64 _across, f64 _up) {
    fude_zoom_instruments* _ins = &_page->instruments;
    i32 _slot = -1;
    for(u32 _i = 0; _i < FUDE_ZOOM_INSTRUMENT_MOST; _i++) {
        if(_ins->tools[_ins->order[_i]].shown && _ins->tools[_ins->order[_i]].kind == (u8)_kind) {
            _slot = (i32)_ins->order[_i];   // (the one on top)
        }
    }
    if(_slot < 0) {
        return;
    }
    _ins->held = _slot;   // as if dropped where it is: snapped (from a little further than a hand's reach)
    fude_zoom_page_instrument_snap(_page, FUDE_ZOOM_SNAP_RADIUS * 3.0);
    _ins->held = -1;
    _page->snap_hit.kind = FUDE_ZOOM_SNAP_NONE;
    if(_page->anchor_tool >= 0) {
        _page->anchor_mm[0] = _across;
        _page->anchor_mm[1] = _up;
        fude_zoom_page_anchor_apply(_page);
    }
}

// See page.h.
void fude_zoom_page_look_joint(fude_zoom_page* _page, u8 _kind, f64 _a, f64 _b, f64 _c) {
    _page->joint_kind  = _kind;
    _page->joint_mm[0] = _a;
    _page->joint_mm[1] = _b;
    _page->joint_mm[2] = _c;
    fude_zoom_page_joint_put(_page);
}

// See page.h.
void fude_zoom_page_look_topic(fude_zoom_page* _page, u32 _topic) {
    fude_zoom_topic_choose(_page->app, _topic);
}

// See page.h.
void fude_zoom_page_look_grain(fude_zoom_page* _page) {
    fude_zoom_page_offset_selection(_page->app, NULL, 0u);
}

// See page.h. The lasso taken up as the bar's tap takes it, and drawn round the box from _a to _b in 32 moves; the
// text found in a PDF under it is logged.
void fude_zoom_page_look_loop(fude_zoom_page* _page, rde_vec_2F _a, rde_vec_2F _b) {
    _page->shape_tool   = 0;   // (the lasso in hand, as the bar's tap puts it: no app tool over it)
    _page->measure_tool = 0;
    fude_toolbar_set_tool(&_page->app->ui->bar, FUDE_TOOL_LASSO);
    _page->tool_seen      = (u8)FUDE_TOOL_LASSO;
    _page->tool_taps_seen = _page->app->ui->bar.tool_taps;
    const rde_vec_2F _c[5] = { _a, { _b.x, _a.y }, _b, { _a.x, _b.y }, _a };
    fude_zoom_page_down(_page, _c[0], true, false);
    for(u32 _i = 1; _i < 5u; _i++) {
        for(u32 _k = 1; _k <= 8u; _k++) {
            const f32 _t = (f32)_k / 8.0f;
            fude_zoom_page_moved(_page, (rde_vec_2F){ _c[_i - 1u].x + (_c[_i].x - _c[_i - 1u].x) * _t, _c[_i - 1u].y + (_c[_i].y - _c[_i - 1u].y) * _t });
        }
    }
    fude_zoom_page_up(_page);
    c8 _text[2048];
    if(_page->pdf_text_on && fude_zoom_page_pdf_text(_page, _page->pdf_text_box, _text, sizeof(_text)) > 0u) {
        rde_log_color(RDE_LOG_COLOR_GREEN, "look: the loop's text: \"%s\"", _text);
    } else {
        rde_log_color(RDE_LOG_COLOR_GREEN, "look: the loop's text: none");
    }
}

// See page.h. Finger 7 on the instruments: down, moved (snapping), or up.
void fude_zoom_page_look_finger(fude_zoom_page* _page, u32 _phase, fude_zoom_v2 _at) {
    if(_phase == 0u) {
        fude_zoom_instruments_finger_down(&_page->instruments, 7u, _at);
    } else if(_phase == 1u) {
        if(fude_zoom_instruments_finger_moved(&_page->instruments, 7u, _at)) {
            fude_zoom_page_instrument_snap(_page, FUDE_ZOOM_SNAP_RADIUS * 1.6);
        }
    } else {
        fude_zoom_instruments_finger_up(&_page->instruments, 7u);
    }
}

// See page.h. Every text lassoed, then written out by hand.
void fude_zoom_page_look_handwrite(fude_zoom_page* _page) {
    fude_zoom_select_clear(&_page->selection);
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(&_page->scene); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(&_page->scene, _i);
        if((_o->flags & FUDE_ZOOM_FLAG_ALIVE) && _o->kind == FUDE_ZOOM_KIND_TEXT) {
            rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _i });
        }
    }
    fude_zoom_select_update(&_page->selection, &_page->scene);
    fude_zoom_page_handwrite_selection(_page);
}

// See page.h. The offset pad's grip pressed with the pen, dragged by _dx, _dy points and let go.
void fude_zoom_page_look_grip(fude_zoom_page* _page, f32 _dx, f32 _dy) {
    const rde_vec_2F _size = fude_kit_screen_size(_page->app->window);
    const rde_vec_2F _at   = { _page->anchor_grip_at.x - _size.x * 0.5f, _page->anchor_grip_at.y - _size.y * 0.5f };
    if(fude_zoom_page_grip_down(_page, _at, FUDE_ZOOM_PAGE_HAND_PEN)) {
        fude_zoom_page_grip_moved(_page, (rde_vec_2F){ _at.x + _dx, _at.y + _dy }, FUDE_ZOOM_PAGE_HAND_PEN);
        fude_zoom_page_grip_up(_page, FUDE_ZOOM_PAGE_HAND_PEN);
    }
}

// See page.h. The topmost instrument of kind _kind that has a grow tab: the tab pressed and moved _by along the
// instrument, let go unless _hold.
void fude_zoom_page_look_grow(fude_zoom_page* _page, u32 _kind, f64 _by, b8 _hold) {
    fude_zoom_instruments* _ins = &_page->instruments;
    for(u32 _i = FUDE_ZOOM_INSTRUMENT_MOST; _i-- > 0;) {
        const u32 _k = _ins->order[_i];
        fude_zoom_v2 _tab, _on;
        if(!_ins->tools[_k].shown || _ins->tools[_k].kind != (u8)_kind || !fude_zoom_instrument_grow_at(_ins, _k, &_tab)) {
            continue;
        }
        const f64 _a = _ins->tools[_k].angle;
        fude_zoom_instruments_pen_down(_ins, _tab, &_on);
        fude_zoom_instruments_pen_moved(_ins, (fude_zoom_v2){ _tab.x + rde_math_cos_f64(_a) * _by, _tab.y + rde_math_sin_f64(_a) * _by });
        if(!_hold) {
            fude_zoom_instruments_pen_up(_ins);
        }
        return;
    }
}

// See page.h. An empty _text, or a lasso without exactly one stock board, presses Fit parts instead.
void fude_zoom_page_look_fit(fude_zoom_page* _page, const c8* _text) {
    u32 _stock = FUDE_ZOOM_NONE;
    if(_text[0] == 0 || fude_zoom_page_lassoed_boards(_page, &_stock) != 1u) {
        fude_zoom_page_fit_selection(_page);   // the row's Fit parts pressed
        return;
    }
    fude_zoom_page_fit_list(_page, _stock, _text);
}

// See page.h. On the newest path or polygon: 0 lassoed; 1 its second node dragged; 2 a node added halfway along its
// first stretch; 3 its second node held mid-drag; 4 its second node tapped, its handle shown; 5 as 4, then a path's
// handle dragged out.
void fude_zoom_page_look_nodes(fude_zoom_page* _page, u8 _step) {
    // The last path or polygon lassoed; then its second node dragged; then a node added on its first stretch.
    u32 _last = FUDE_ZOOM_NONE;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(&_page->scene); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(&_page->scene, _i);
        if((_o->flags & FUDE_ZOOM_FLAG_ALIVE) && _o->kind == FUDE_ZOOM_KIND_SHAPE && (_o->channels == FUDE_ZOOM_SHAPE_PATH || _o->channels == FUDE_ZOOM_SHAPE_POLYGON)) {
            _last = _i;
        }
    }
    if(_last == FUDE_ZOOM_NONE) {
        return;
    }
    if(_step == 0u) {
        fude_zoom_select_clear(&_page->selection);
        rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _last });
        fude_zoom_select_update(&_page->selection, &_page->scene);
        return;
    }
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    u32 _count, _first;
    fude_zoom_sim _to;
    fude_zoom_page_nodes(_page, _last, _n, &_count, &_first, &_to);
    const fude_zoom_v2 _a = fude_zoom_sim_apply(_to, (fude_zoom_v2){ _n[_first], _n[_first + 1u] });
    const fude_zoom_v2 _b = fude_zoom_sim_apply(_to, (fude_zoom_v2){ _n[_first + 2u], _n[_first + 3u] });
    if(_step >= 4u) {
        // Its second node tapped (its handle shown); with 5, the handle's way-out end dragged out.
        fude_zoom_page_node_down(_page, (rde_vec_2F){ (f32)_b.x, (f32)_b.y });
        fude_zoom_page_node_up(_page);
        if(_step == 5u && _first == 1u) {
            u64 _bits;
            fude_zoom_v2 _p[FUDE_ZOOM_PATH_NODES], _h[FUDE_ZOOM_PATH_NODES];
            const u32 _m = fude_zoom_path_unpack(_n, _count, &_bits, _p, _h);
            const fude_zoom_v2 _t   = fude_zoom_path_tangent(_p, _h, _m, _bits, 1u, true);
            const fude_zoom_v2 _end = fude_zoom_sim_apply(_to, (fude_zoom_v2){ _p[1].x + _t.x, _p[1].y + _t.y });
            fude_zoom_page_node_down(_page, (rde_vec_2F){ (f32)_end.x, (f32)_end.y });
            fude_zoom_page_node_moved(_page, (rde_vec_2F){ (f32)_end.x + 90.0f, (f32)_end.y + 90.0f });
            fude_zoom_page_node_up(_page);
        }
        return;
    }
    if(_step == 1u) {
        fude_zoom_page_node_down(_page, (rde_vec_2F){ (f32)_b.x, (f32)_b.y });
        fude_zoom_page_node_moved(_page, (rde_vec_2F){ (f32)_b.x + 40.0f, (f32)_b.y - 120.0f });
        fude_zoom_page_node_up(_page);
    } else if(_step == 2u) {
        // On the line half way along its first stretch, as drawn.
        rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
        b8 _closed;
        fude_zoom_shape_outline(fude_zoom_scene_object(&_page->scene, _last)->channels, _n, _count, 64u, &_pts, &_closed);
        const u32 _k = (u32)rde_arr_length(&_pts);
        u32 _mid = 0;
        f64 _d = 1e300;
        for(u32 _i = 0; _i < _k; _i++) {
            const fude_zoom_v2 _p = fude_zoom_sim_apply(_to, ((const fude_zoom_v2*)_pts.memory)[_i]);
            const f64 _e = rde_math_abs_f64(rde_math_hypot_f64(_p.x - _a.x, _p.y - _a.y) - rde_math_hypot_f64(_p.x - _b.x, _p.y - _b.y)) + (_i > _k / 2u ? 1e9 : 0.0);
            if(_e < _d) { _d = _e; _mid = _i; }
        }
        const fude_zoom_v2 _q = fude_zoom_sim_apply(_to, ((const fude_zoom_v2*)_pts.memory)[_mid]);
        rde_arr_free(&_pts);
        fude_zoom_page_node_down(_page, (rde_vec_2F){ (f32)_q.x, (f32)_q.y });
        _page->node_last_at = (rde_vec_2F){ (f32)_q.x, (f32)_q.y };
        fude_zoom_page_node_up(_page);
    } else {
        // Held without going anywhere: the drag shown (its line as it would be).
        fude_zoom_page_node_down(_page, (rde_vec_2F){ (f32)_b.x, (f32)_b.y });
        fude_zoom_page_node_moved(_page, (rde_vec_2F){ (f32)_b.x + 40.0f, (f32)_b.y - 120.0f });
    }
}

// See page.h. Everything lassoed; 0 the formula written out by hand, else the handwriting read as a formula.
void fude_zoom_page_look_formula(fude_zoom_page* _page, u8 _what) {
    fude_zoom_select_all(&_page->selection, &_page->scene, &_page->renderer);
    if(_what == 0u) {
        fude_zoom_page_formula_hand_selection(_page->app, NULL, 0);
    } else {
        fude_zoom_page_formula_read_selection(_page->app, NULL, 0);
    }
}

// See page.h. Everything lassoed, then To curve.
void fude_zoom_page_look_to_curve(fude_zoom_page* _page) {
    fude_zoom_select_all(&_page->selection, &_page->scene, &_page->renderer);
    fude_zoom_page_curve_selection(_page->app, NULL, 0);
}

// See page.h: holes _mm millimetres across, as typed on the numpad.
void fude_zoom_page_look_holes(fude_zoom_page* _page, f64 _mm) {
    fude_zoom_page_holes_typed(_page, "", FUDE_ZOOM_UNIT_MM, _mm);
}

// See page.h: an offset of _mm millimetres, as typed on the numpad.
void fude_zoom_page_look_offset(fude_zoom_page* _page, f64 _mm) {
    fude_zoom_page_offset_typed(_page, "", FUDE_ZOOM_UNIT_MM, _mm);
}

// See page.h.
void fude_zoom_page_look_find(fude_zoom_page* _page, const c8* _words) {
    fude_zoom_page_find(_page, _words);
}
