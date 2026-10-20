// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"
#include "drawing/base/redraw.h"

// ===========================================================================
// Page, text: texts and sticky notes (the Insert tool's card), writing on the page (0.1.90), and typed text
// written out by hand (hand.h), with formulas, Offset and To curve on the lasso's row.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// --- text and sticky notes (the Insert tool) -----------------------------------------------

// Whether a card's style asks for a name, one line: a named area's, a piece's, a custom part's.
#define FUDE_ZOOM_PAGE_TEXT_NAMES(_style) ((_style) == FUDE_ZOOM_PAGE_TEXT_AREA || (_style) == FUDE_ZOOM_PAGE_TEXT_PIECE || (_style) == FUDE_ZOOM_PAGE_TEXT_CUSTOM)
#define FUDE_ZOOM_PAGE_FORMULA_PREVIEW 132.0f   // the Formula card's room for it set (points)

// The text on top under _screen (FUDE_ZOOM_NONE: none); its frame's way onto the screen into *_to (may be NULL).
RDE_INTERNAL u32 fude_zoom_page_text_under(fude_zoom_page* _page, rde_vec_2F _screen, fude_zoom_sim* _to) {
    fude_zoom_scene* _s = &_page->scene;
    u32 _text = FUDE_ZOOM_NONE;
    rde_arr_clear(&_page->editable);
    fude_zoom_render_editable(&_page->renderer, _s, &_page->editable);
    const fude_zoom_visible* _v = (const fude_zoom_visible*)_page->editable.memory;
    rde_arr _found = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    u64 _top = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->editable); _i++) {
        const fude_zoom_v2 _at = fude_zoom_sim_apply(fude_zoom_sim_inverse(_v[_i].to_screen), (fude_zoom_v2){ _screen.x, _screen.y });
        rde_arr_clear(&_found);
        fude_zoom_scene_query(_s, _v[_i].frame, (fude_zoom_box){ _at.x, _at.y, _at.x, _at.y }, &_found);
        for(u32 _k = 0; _k < (u32)rde_arr_length(&_found); _k++) {
            const u32 _o = ((const u32*)_found.memory)[_k];
            const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _o);
            if(_ob->kind != FUDE_ZOOM_KIND_TEXT || !(_ob->flags & FUDE_ZOOM_FLAG_ALIVE) || !fude_zoom_scene_touchable(_s, _ob) || _ob->z < _top) {
                continue;
            }
            fude_zoom_v2 _c[4];
            fude_zoom_scene_text_corners(_s, _o, _c);
            // Inside its box (a parallelogram once turned): on the inside of each side.
            b8 _in = true;
            for(u32 _e = 0; _e < 4u && _in; _e++) {
                const fude_zoom_v2 _a = _c[_e], _b = _c[(_e + 1u) % 4u];
                _in = (_b.x - _a.x) * (_at.y - _a.y) - (_b.y - _a.y) * (_at.x - _a.x) >= 0.0;
            }
            if(_in) {
                _text = _o;
                _top  = _ob->z;
                if(_to != NULL) {
                    *_to = _v[_i].to_screen;
                }
            }
        }
    }
    rde_arr_free(&_found);
    return _text;
}

// See page_parts.h. A new text goes in the frame strokes would (fude_zoom_camera_drawing_frame), and is a sticky note
// or a formula as the tool is set (text_tool 2 or 3).
void fude_zoom_page_text_at(fude_zoom_page* _page, rde_vec_2F _screen) {
    fude_zoom_page_inline_end(_page);   // (one being written on the page: saved first)
    if(_page->text_field == NULL) {
        return;
    }
    fude_zoom_scene* _s = &_page->scene;
    _page->text_object = fude_zoom_page_text_under(_page, _screen, NULL);
    // The field: what it says now, or empty.
    const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_page->text_field, 0, _bytes);
    }
    if(_page->text_object != FUDE_ZOOM_NONE) {
        const c8* _text = NULL;
        u32       _len  = 0;
        fude_zoom_scene_text(_s, _page->text_object, NULL, NULL, NULL, &_page->text_style, &_text, &_len);
        rde_ui_text_editor_insert_at(_page->text_field, 0, _text, _len);
    } else {
        const fude_zoom_v2 _at   = fude_zoom_camera_to_frame(&_s->camera, (fude_zoom_v2){ _screen.x, _screen.y });
        const f64          _near = 4.0 / _s->camera.z;
        _page->text_frame = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _at.x - _near, _at.y - _near, _at.x + _near, _at.y + _near });
        _page->text_at    = fude_zoom_sim_apply(fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _page->text_frame)), (fude_zoom_v2){ _screen.x, _screen.y });
        _page->text_style = _page->text_tool == 2u ? (u8)FUDE_ZOOM_TEXT_STICKY : _page->text_tool == 3u ? (u8)FUDE_ZOOM_TEXT_FORMULA : (u8)FUDE_ZOOM_TEXT_PLAIN;
    }
    // (a formula: its LaTeX, an example of it shown while empty)
    rde_ui_text_editor_set_placeholder(_page->text_field, _page->text_style == FUDE_ZOOM_TEXT_FORMULA ? fude_text(FUDE_TEXT_ZOOM_FORMULA_HINT) : "");
    fude_zoom_page_text_card(_page, true);
}

// Text _object made again with the words _text (_len bytes), all else as it was: one undo step. The new one.
RDE_INTERNAL u32 fude_zoom_page_retext(fude_zoom_page* _page, u32 _object, const c8* _text, u32 _len) {
    fude_zoom_scene* _s = &_page->scene;
    const fude_zoom_object _old = *fude_zoom_scene_object(_s, _object);
    f64 _size = 0.0, _w = 0.0, _h = 0.0;
    u8  _style = 0;
    if(!fude_zoom_scene_text(_s, _object, &_size, &_w, &_h, &_style, NULL, NULL)) {
        return FUDE_ZOOM_NONE;
    }
    const u16 _layer = _s->layer;
    _s->layer = _old.layer;
    const u32 _made = fude_zoom_scene_add_text(_s, _old.frame, fude_zoom_scene_place_of(_s, _object), _size, _w, _h, _style, _old.color, _old.fill, _text, _len, _old.z);
    _s->layer = _layer;
    fude_zoom_scene_set_alive(_s, _object, false);
    fude_zoom_history_push(_s, _old.frame, fude_zoom_box_union(_old.box, fude_zoom_scene_object(_s, _made)->box), &_object, 1u, &_made, 1u);
    // (held by the lasso: the new one held instead)
    fude_zoom_pick* _p = (fude_zoom_pick*)_page->selection.picks.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        if(_p[_i].object == _object) {
            _p[_i].object = _made;
        }
    }
    fude_zoom_select_update(&_page->selection, _s);
    _page->map_stale = true;
    return _made;
}

// The note under _screen — a text's words, or a diagram shape's label (a Kanban card's) — with marks in it, laid out as
// it is drawn: into *_object, *_rich, and where _screen is on it (*_x, *_y: its units, Y down from its words' top
// left), *_k a unit of it on the screen; its words into _words (_size bytes). False: none.
b8 fude_zoom_page_rich_under(fude_zoom_page* _page, rde_vec_2F _screen, u32* _object, const fude_zoom_rich** _rich, f32* _x, f32* _y, f64* _k,
                                         c8* _words, usize _size) {
    fude_zoom_scene* _s = &_page->scene;
    fude_zoom_sim _to;
    u32 _o = fude_zoom_page_text_under(_page, _screen, &_to);
    if(_o != FUDE_ZOOM_NONE) {
        fude_zoom_v2 _origin;
        const fude_zoom_rich* _r = fude_zoom_render_rich_of(&_page->renderer, _s, _o, &_origin);
        if(_r != NULL) {
            const fude_zoom_sim _all = fude_zoom_sim_compose(_to, fude_zoom_object_sim(fude_zoom_scene_object(_s, _o)));
            const fude_zoom_v2  _at  = fude_zoom_sim_apply(fude_zoom_sim_inverse(_all), (fude_zoom_v2){ _screen.x, _screen.y });
            const c8* _text = NULL;
            u32 _len = 0;
            fude_zoom_scene_text(_s, _o, NULL, NULL, NULL, NULL, &_text, &_len);
            rde_snprintf(_words, _size, "%.*s", (int)_len, _text);
            *_object = _o;
            *_rich   = _r;
            *_x      = (f32)(_at.x - _origin.x);
            *_y      = (f32)(_origin.y - _at.y);
            *_k      = rde_math_max_f64(fude_zoom_sim_scale(_all), 1e-12);
            return true;
        }
    }
    // A diagram shape's label, as the renderer lays it in its box (render_symbol.c's fude_zoom_render_symbol).
    _o = fude_zoom_page_under(_page, _screen);
    if(_o >= fude_zoom_scene_object_count(_s)) {
        return false;
    }
    const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _o);
    if(_ob->kind != FUDE_ZOOM_KIND_SHAPE || _ob->channels != FUDE_ZOOM_SHAPE_SYMBOL) {
        return false;
    }
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _count = fude_zoom_scene_shape_numbers(_s, _o, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    const u32 _kind = _count >= 4u ? fude_zoom_symbol_kind_of(_n, _count) : FUDE_ZOOM_NONE;
    const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of(_kind);
    if(_info == NULL || _info->place == FUDE_ZOOM_SYMBOL_TEXT_PARTS) {
        return false;
    }
    fude_zoom_symbol_text(_n, _count, _words, _size);
    fude_zoom_box _boxes[8];
    if(_words[0] == 0 || fude_zoom_symbol_text_boxes(_kind, _n[1], _n[2], 1u, _boxes, 8u) == 0u) {
        return false;
    }
    const fude_zoom_sim _all = fude_zoom_sim_compose(fude_zoom_page_frame_sim(_page, _ob->frame), fude_zoom_object_sim(_ob));
    const f64 _kk = fude_zoom_sim_scale(_all);
    const fude_zoom_box _b = fude_zoom_sim_box(_all, _boxes[0]);
    const b8 _top  = _info->place == FUDE_ZOOM_SYMBOL_TEXT_CORNER;
    const u8 _side = _top || _info->place == FUDE_ZOOM_SYMBOL_TEXT_OUT_RIGHT ? 0u : _info->place == FUDE_ZOOM_SYMBOL_TEXT_OUT_LEFT ? 2u : 1u;
    const fude_zoom_rich* _r = _kk > 0.0 ? fude_zoom_render_rich_aligned(&_page->renderer, _words, (u32)rde_strlen(_words), _n[3], (_b.max_x - _b.min_x) / _kk, _side) : NULL;
    if(_r == NULL) {
        return false;
    }
    const f64 _down = _top ? 0.0 : rde_math_max_f64(((_b.max_y - _b.min_y) - (f64)_r->h * _kk) * 0.5, 0.0);
    *_object = _o;
    *_rich   = _r;
    *_x      = (f32)((_screen.x - _b.min_x) / _kk);
    *_y      = (f32)((_b.max_y - _down - _screen.y) / _kk);
    *_k      = _kk;
    return true;
}

// A tap at _screen on a note's task — ticked, or unticked (one undo step) — or on one of its links (opened, unless
// only _tasks): a text's, or a diagram shape's label's. True: it was one.
b8 fude_zoom_page_rich_tap(fude_zoom_page* _page, rde_vec_2F _screen, b8 _tasks) {
    u32 _o = FUDE_ZOOM_NONE;
    const fude_zoom_rich* _rich = NULL;
    f32 _x = 0.0f, _y = 0.0f;
    f64 _k = 1.0;
    c8  _words[FUDE_ZOOM_SYMBOL_TEXT * 8];
    if(!fude_zoom_page_rich_under(_page, _screen, &_o, &_rich, &_x, &_y, &_k, _words, sizeof(_words))) {
        return false;
    }
    const u32 _h = fude_zoom_rich_hit_at(_rich, _x, _y, (f32)(FUDE_ZOOM_PAGE_RICH_REACH / _k));
    if(_h == FUDE_ZOOM_RICH_NONE) {
        return false;
    }
    const fude_zoom_rich_hit _hit = ((const fude_zoom_rich_hit*)_rich->hits.memory)[_h];   // (a copy: the layout goes with the text)
    const u32 _len = (u32)rde_strlen(_words);
    if(_hit.kind == FUDE_ZOOM_RICH_HIT_LINK) {
        if(_tasks) {
            return false;
        }
        c8 _url[1024];
        fude_zoom_rich_address(_words, _hit.at, _hit.len, _url, sizeof(_url));
        rde_engine_open_url(_url);
        return true;
    }
    if(fude_zoom_rich_tick(_words, _len, _hit.at)) {
        if(fude_zoom_scene_object(&_page->scene, _o)->kind == FUDE_ZOOM_KIND_TEXT) {
            fude_zoom_page_retext(_page, _o, _words, _len);
        } else {
            _page->symbol_editing = _o;   // (its label made again: as its card would)
            fude_zoom_page_symbol_text_commit(_page, _words);
        }
    }
    return true;
}

// Is there a note's link under _screen (a text's, a diagram shape's label's)?
b8 fude_zoom_page_link_at(fude_zoom_page* _page, rde_vec_2F _screen) {
    u32 _o = FUDE_ZOOM_NONE;
    const fude_zoom_rich* _rich = NULL;
    f32 _x = 0.0f, _y = 0.0f;
    f64 _k = 1.0;
    c8  _words[FUDE_ZOOM_SYMBOL_TEXT * 8];
    if(!fude_zoom_page_rich_under(_page, _screen, &_o, &_rich, &_x, &_y, &_k, _words, sizeof(_words))) {
        return false;
    }
    const u32 _h = fude_zoom_rich_hit_at(_rich, _x, _y, (f32)(FUDE_ZOOM_PAGE_RICH_REACH * 0.5 / _k));
    return _h != FUDE_ZOOM_RICH_NONE && ((const fude_zoom_rich_hit*)_rich->hits.memory)[_h].kind == FUDE_ZOOM_RICH_HIT_LINK;
}

// Text _object (a text, a sticky note, a formula) on the text card to change — as the text tool's tap on it opens it.
void fude_zoom_page_text_edit(fude_zoom_page* _page, u32 _object) {
    fude_zoom_page_inline_end(_page);   // (one being written on the page: saved first)
    fude_zoom_scene* _s = &_page->scene;
    if(_page->text_field == NULL || _object >= fude_zoom_scene_object_count(_s)) {
        return;
    }
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    const c8* _text = NULL;
    u32       _len  = 0;
    if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_TEXT ||
       !fude_zoom_scene_text(_s, _object, NULL, NULL, NULL, &_page->text_style, &_text, &_len)) {
        return;
    }
    _page->text_object = _object;
    const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_page->text_field, 0, _bytes);
    }
    rde_ui_text_editor_insert_at(_page->text_field, 0, _text, _len);
    rde_ui_text_editor_set_placeholder(_page->text_field, _page->text_style == FUDE_ZOOM_TEXT_FORMULA ? fude_text(FUDE_TEXT_ZOOM_FORMULA_HINT) : "");
    fude_zoom_page_text_card(_page, true);
}

// The one text the lasso holds (a text, a note, a formula): FUDE_ZOOM_NONE when none, or more than it.
u32 fude_zoom_page_lone_text(const fude_zoom_page* _page) {
    if(rde_arr_length(&_page->selection.picks) != 1u) {
        return FUDE_ZOOM_NONE;
    }
    const u32 _o = ((const fude_zoom_pick*)_page->selection.picks.memory)[0].object;
    const fude_zoom_object* _b = _o < fude_zoom_scene_object_count(&_page->scene) ? fude_zoom_scene_object(&_page->scene, _o) : NULL;
    return _b != NULL && (_b->flags & FUDE_ZOOM_FLAG_ALIVE) && _b->kind == FUDE_ZOOM_KIND_TEXT ? _o : FUDE_ZOOM_NONE;
}

// The lasso's row's Edit text: the one text it holds on the card.
void fude_zoom_page_text_edit_selection(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page != NULL && fude_zoom_page_lone_text(_page) != FUDE_ZOOM_NONE) {
        fude_zoom_page_text_edit(_page, fude_zoom_page_lone_text(_page));
    }
}

// Writing on the page, defined below.
RDE_INTERNAL void fude_zoom_page_inline_finish(fude_zoom_page* _page);

// The card's text when it names something (fude_zoom_page_text_save_now's): a parts list to fit into a board, a
// named area's, a piece's or a custom part's name, a symbol's text, a board's name; each handed to its own. False:
// the card is none of those.
RDE_INTERNAL b8 fude_zoom_page_text_save_naming(fude_zoom_page* _page) {
    if(_page->text_style == FUDE_ZOOM_PAGE_TEXT_PARTS) {
        const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
        c8*         _text  = rde_ui_text_editor_get_text(_page->text_field, 0, _bytes);
        if(_text != NULL) {
            fude_zoom_page_fit_list(_page, _page->fit_stock, _text);
            rde_ui_text_editor_free_text(_page->text_field, _text);
        }
        rde_ui_text_editor_set_placeholder(_page->text_field, "");
        return true;
    }
    if(FUDE_ZOOM_PAGE_TEXT_NAMES(_page->text_style)) {
        const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
        c8*         _text  = rde_ui_text_editor_get_text(_page->text_field, 0, _bytes);
        if(_page->text_style == FUDE_ZOOM_PAGE_TEXT_PIECE) {
            fude_zoom_page_piece_commit(_page, _text != NULL ? _text : "");
        } else if(_page->text_style == FUDE_ZOOM_PAGE_TEXT_CUSTOM) {
            fude_zoom_page_custom_rename(_page, _text != NULL ? _text : "");
        } else {
            fude_zoom_page_area_commit(_page, _text != NULL ? _text : "");
        }
        if(_text != NULL) {
            rde_ui_text_editor_free_text(_page->text_field, _text);
        }
        rde_ui_text_editor_set_placeholder(_page->text_field, "");
        return true;
    }
    if(_page->text_style == FUDE_ZOOM_PAGE_TEXT_SYMBOL) {
        const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
        c8*         _text  = rde_ui_text_editor_get_text(_page->text_field, 0, _bytes);
        if(_text != NULL) {
            usize _n = rde_strlen(_text);
            while(_n > 0 && (_text[_n - 1u] == '\n' || _text[_n - 1u] == ' ')) {
                _text[--_n] = 0;
            }
            fude_zoom_page_symbol_text_commit(_page, _text);
            rde_ui_text_editor_free_text(_page->text_field, _text);
        }
        rde_ui_text_editor_set_placeholder(_page->text_field, "");
        return true;
    }
    if(_page->text_style == FUDE_ZOOM_PAGE_TEXT_BOARD) {
        const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
        c8*         _text  = rde_ui_text_editor_get_text(_page->text_field, 0, _bytes);
        if(_text != NULL) {
            usize _n = rde_strcspn(_text, "\r\n");
            _text[_n] = 0;
            while(_n > 0 && _text[_n - 1u] == ' ') {
                _text[--_n] = 0;
            }
            fude_zoom_page_board_commit(_page, _text);
            rde_ui_text_editor_free_text(_page->text_field, _text);
        }
        rde_ui_text_editor_set_placeholder(_page->text_field, "");
        return true;
    }
    return false;
}

// The card's text when a tool asked for it (fude_zoom_page_text_save_now's): a layer's new name, Find's words,
// a diagram's Mermaid text; each handed to its own. False: the card is none of those.
RDE_INTERNAL b8 fude_zoom_page_text_save_tools(fude_zoom_page* _page) {
    if(_page->text_style == FUDE_ZOOM_PAGE_TEXT_RENAME) {
        const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
        c8*         _text  = rde_ui_text_editor_get_text(_page->text_field, 0, _bytes);
        if(_text != NULL) {
            // One line, as long as a row keeps (cut at a whole character).
            usize _n = rde_strcspn(_text, "\r\n");
            _text[_n] = 0;
            while(_n > 0 && _text[_n - 1u] == ' ') {
                _text[--_n] = 0;
            }
            if(_n >= sizeof(_page->layer_names[0])) {
                _n = sizeof(_page->layer_names[0]) - 1u;
                while(_n > 0 && ((u8)_text[_n] & 0xC0u) == 0x80u) {
                    _n--;
                }
                _text[_n] = 0;
            }
            fude_zoom_page_find_layers(_page, NULL);
            if(_n > 0 && _page->layer_renaming < _page->layer_count && rde_strcmp(_text, _page->layer_names[_page->layer_renaming]) != 0) {
                fude_zoom_page_layer_set(_page, _page->layer_renaming, _page->layer_flags[_page->layer_renaming], _text);
            }
            _page->layers_open = true;
            fude_zoom_page_show_layers(_page);
            rde_ui_text_editor_free_text(_page->text_field, _text);
        }
        rde_ui_text_editor_set_placeholder(_page->text_field, "");
        return true;
    }
    if(_page->text_style == FUDE_ZOOM_PAGE_TEXT_FIND) {
        const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
        c8*         _text  = rde_ui_text_editor_get_text(_page->text_field, 0, _bytes);
        if(_text != NULL) {
            usize _n = rde_strlen(_text);
            while(_n > 0 && (_text[_n - 1u] == '\n' || _text[_n - 1u] == ' ')) {
                _text[--_n] = 0;
            }
            if(_n > 0) {
                fude_zoom_page_find(_page, _text);
            }
            rde_ui_text_editor_free_text(_page->text_field, _text);
        }
        rde_ui_text_editor_set_placeholder(_page->text_field, "");
        rde_ui_button_set_text(_page->text_save, fude_text(FUDE_TEXT_SAVE));
        return true;
    }
    if(_page->text_style == FUDE_ZOOM_PAGE_TEXT_MERMAID) {
        const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
        c8*         _text  = rde_ui_text_editor_get_text(_page->text_field, 0, _bytes);
        if(_text != NULL) {
            fude_zoom_page_import_mermaid(_page, _text);
            rde_ui_text_editor_free_text(_page->text_field, _text);
        }
        rde_ui_text_editor_set_placeholder(_page->text_field, "");
        return true;
    }
    return false;
}

// The card's text as a text on the canvas (fude_zoom_page_text_save_now's): laid out as its style says (a sticky
// note wrapped to its width, a formula as it is set, a note with marks as it is laid out, a text as wide as its
// longest line) and put down, or the one tapped changed (its frame, place, layer, line and colours kept), or let
// go when emptied. One undo step.
RDE_INTERNAL void fude_zoom_page_text_put(fude_zoom_page* _page) {
    fude_zoom_scene* _s    = &_page->scene;
    rde_font*        _font = _page->app->font;
    const usize      _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
    c8*              _text  = rde_ui_text_editor_get_text(_page->text_field, 0, _bytes);
    if(_text == NULL || _font == NULL) {
        if(_text != NULL) {
            rde_ui_text_editor_free_text(_page->text_field, _text);
        }
        return;
    }
    const b8 _editing = _page->text_object != FUDE_ZOOM_NONE && _page->text_object < fude_zoom_scene_object_count(_s);
    fude_zoom_object _old;
    rde_memset(&_old, 0, sizeof(_old));
    if(_editing) {
        _old = *fude_zoom_scene_object(_s, _page->text_object);
    }
    const u32 _frame = _editing ? _old.frame : _page->text_frame;
    const fude_zoom_sim _to = fude_zoom_sim_compose(fude_zoom_page_frame_sim(_page, _frame), _editing ? fude_zoom_object_sim(&_old) : (fude_zoom_sim){ 1.0, 0.0, 0.0, 0.0 });
    const f64 _k = fude_zoom_sim_scale(_to);
    // Its letters' size: as it was, or the tool's on screen now.
    f64 _size = 0.0, _was_w = 0.0;
    if(_editing) {
        fude_zoom_scene_text(_s, _page->text_object, &_size, &_was_w, NULL, NULL, NULL, NULL);
    } else {
        _size = (_page->text_style == FUDE_ZOOM_TEXT_STICKY ? FUDE_ZOOM_PAGE_STICKY_PX :
                 _page->text_style == FUDE_ZOOM_TEXT_FORMULA ? FUDE_ZOOM_PAGE_FORMULA_PX : FUDE_ZOOM_PAGE_TEXT_PX) / _k;
    }
    // Trailing line breaks and spaces dropped.
    usize _len = rde_strlen(_text);
    while(_len > 0 && (_text[_len - 1u] == '\n' || _text[_len - 1u] == ' ' || _text[_len - 1u] == '\r')) {
        _text[--_len] = 0;
    }
    const f32 _px   = (f32)(_size * _k);
    const f32 _line = _px * (f32)FUDE_ZOOM_PAGE_TEXT_LINE;
    f64 _w = 0.0, _h = 0.0;
    if(_page->text_style == FUDE_ZOOM_TEXT_FORMULA) {
        // As big as it is set (tex.h), its pad round it.
        fude_zoom_formulas_size(fude_zoom_formulas_set(&_page->formulas, _text, (u32)_len), _size, &_w, &_h);
    } else if(_page->text_style == FUDE_ZOOM_TEXT_STICKY) {
        // (as wide as it was: its sides may have been stretched)
        const f32 _inner = _editing && _was_w > 0.0 ? rde_math_max_f32((f32)(_was_w * _k) - 1.2f * _px, _px * 4.0f) : _px * (f32)FUDE_ZOOM_PAGE_STICKY_W;
        const fude_zoom_rich* _rich = _len > 0 ? fude_zoom_render_rich(&_page->renderer, _text, (u32)_len, _size, (f64)_inner / _k) : NULL;
        const u32 _lines = _len > 0 ? fude_draw_text_wrap_lines(_font, _page->app->font_px, _text, _px, _inner) : 1u;
        _w = ((f64)_inner + 1.2 * (f64)_px) / _k;
        _h = _rich != NULL ? (f64)_rich->h + 1.2 * _size : ((f64)_lines * (f64)_line + 1.2 * (f64)_px) / _k;
    } else if(_len > 0 && fude_zoom_render_rich(&_page->renderer, _text, (u32)_len, _size, 0.0) != NULL) {
        // Read as a note (its marks shown, not written): as wide and tall as it is laid out.
        const fude_zoom_rich* _rich = fude_zoom_render_rich(&_page->renderer, _text, (u32)_len, _size, 0.0);
        _w = (f64)_rich->w + _size * 0.3;
        _h = (f64)_rich->h + _size * 0.2;
    } else {
        // As wide as its longest line.
        u32 _lines = 0;
        f32 _widest = 0.0f;
        for(const c8* _p = _text; ; ) {
            const c8* _end = rde_strchr(_p, '\n');
            const usize _n = _end != NULL ? (usize)(_end - _p) : rde_strlen(_p);
            c8 _one[512];
            rde_snprintf(_one, sizeof(_one), "%.*s", (int)(_n < sizeof(_one) - 1u ? _n : sizeof(_one) - 1u), _p);
            _widest = rde_math_max_f32(_widest, fude_draw_text_width(_font, _page->app->font_px, _one, _px));
            _lines++;
            if(_end == NULL) {
                break;
            }
            _p = _end + 1;
        }
        _w = ((f64)_widest + (f64)_px * 0.3) / _k;
        _h = ((f64)_lines * (f64)_line + (f64)_px * 0.2) / _k;
    }
    const b8        _sticky = _page->text_style == FUDE_ZOOM_TEXT_STICKY;
    const rde_color _ink    = _editing ? _old.color : (_sticky ? (rde_color){ 46, 46, 54, 255 } : _page->app->ink->color);
    const rde_color _paper  = _editing ? _old.fill : (rde_color){ 255, 233, 140, 255 };
    u32 _died = FUDE_ZOOM_NONE, _born = FUDE_ZOOM_NONE;
    if(_len > 0) {
        const fude_zoom_place _place = _editing ? fude_zoom_scene_place_of(_s, _page->text_object) : (fude_zoom_place){ _page->text_at, 0.0, 1.0 };
        const u16 _layer_was = _s->layer;
        if(_editing) {
            _s->layer = _old.layer;
            _s->style = _old.channels == FUDE_ZOOM_SHAPE_SYMBOL || _old.kind != FUDE_ZOOM_KIND_SHAPE ? 0 : _old.q;   // (its own line, as its layer)
        }
        _born = fude_zoom_scene_add_text(_s, _frame, _place, _size, _w, _h, _page->text_style, _ink, _paper, _text, (u32)_len, _editing ? _old.z : 0);
        _s->layer = _layer_was;
        _s->style = 0;
    }
    if(_editing) {
        _died = _page->text_object;
        fude_zoom_scene_set_alive(_s, _died, false);
    }
    rde_ui_text_editor_free_text(_page->text_field, _text);
    if(_died == FUDE_ZOOM_NONE && _born == FUDE_ZOOM_NONE) {
        return;
    }
    fude_zoom_box _box = _born != FUDE_ZOOM_NONE ? fude_zoom_scene_object(_s, _born)->box : _old.box;
    if(_editing) {
        _box = fude_zoom_box_union(_box, _old.box);
    }
    fude_zoom_history_push(_s, _frame, _box, _died != FUDE_ZOOM_NONE ? &_died : NULL, _died != FUDE_ZOOM_NONE ? 1u : 0u,
                           _born != FUDE_ZOOM_NONE ? &_born : NULL, _born != FUDE_ZOOM_NONE ? 1u : 0u);
}

// Save on the card, by its style. A name, a parts list, a symbol's text, a board's name, a layer's name, Find's words
// or Mermaid text each go to their own; otherwise the text is laid out (a sticky note wrapped to its width, a formula
// as it is set, a note with marks as it is laid out, a text as wide as its longest line) and put down, or the one
// tapped changed, or let go when emptied. One undo step.
// Its steps are above: fude_zoom_page_text_save_naming and fude_zoom_page_text_save_tools (the styles that go to
// their own), then fude_zoom_page_text_put.
RDE_INTERNAL void fude_zoom_page_text_save_now(fude_zoom_page* _page) {
    fude_zoom_page_text_card(_page, false);
    if(fude_zoom_page_text_save_naming(_page) || fude_zoom_page_text_save_tools(_page)) {
        return;
    }
    fude_zoom_page_text_put(_page);
}

// What the card (or the page, written on it) says, saved: as its style asks (a text made or changed, one undo step).
void fude_zoom_page_text_save(fude_zoom_page* _page) {
    const b8 _inline = _page->inline_on;
    fude_zoom_page_text_save_now(_page);
    if(_inline) {
        fude_zoom_page_inline_finish(_page);
    }
}

// See page_parts.h.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_text_save(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    if(fude_zoom_page_the != NULL) {
        fude_zoom_page_text_save(fude_zoom_page_the);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A tap off the card: as Cancel — but not the lift of the press that opened it (the text tool opens it on the press;
// its release lands on the backdrop), so a tap within 0.6 s of its opening is let go.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_text_dismiss(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    if(fude_zoom_page_the != NULL && rde_engine_get_time_now() - fude_zoom_page_the->text_opened_at < 0.6) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    return fude_zoom_page_on_text_cancel(_node, _info, _user_data);
}

// See page_parts.h. The Save button's word and the field's hint are put back for the next use (Find changes them).
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_text_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    if(fude_zoom_page_the != NULL) {
        fude_zoom_page_text_card(fude_zoom_page_the, false);
        if(fude_zoom_page_the->text_save != NULL) {
            rde_ui_button_set_text(fude_zoom_page_the->text_save, fude_text(FUDE_TEXT_SAVE));
            rde_ui_text_editor_set_placeholder(fude_zoom_page_the->text_field, "");
        }
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A note's mark put in by its button (rich.h): the field's words made again with it, its selection kept on them.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_text_format(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || _page->text_field == NULL) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
    c8* _text = _bytes > 0u ? rde_ui_text_editor_get_text(_page->text_field, 0, _bytes) : NULL;
    const u32 _len = _text != NULL ? (u32)rde_strlen(_text) : 0u;
    const rde_ui_text_editor_cursor _c = rde_ui_text_editor_get_primary_cursor(_page->text_field);
    const u32 _index  = (u32)(_c.index <= _len ? _c.index : _len);
    const u32 _anchor = _c.anchor >= 0 && (u64)_c.anchor <= _len ? (u32)_c.anchor : _index;
    rde_arr _out = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_out, (usize)_len * 7u + 256u);
    u32 _a = 0, _b = 0;
    if(fude_zoom_rich_format(_text != NULL ? _text : "", _len, _anchor < _index ? _anchor : _index, _anchor < _index ? _index : _anchor, (u8)(uintptr_t)_user_data,
                             (c8*)_out.memory, (u32)rde_arr_length(&_out), &_a, &_b)) {
        if(_bytes > 0u) {
            rde_ui_text_editor_delete_range(_page->text_field, 0, _bytes);
        }
        rde_ui_text_editor_insert_at(_page->text_field, 0, (const c8*)_out.memory, rde_strlen((const c8*)_out.memory));
        if(_a == _b) {
            rde_ui_text_editor_set_cursor(_page->text_field, 0, _a);
        } else {
            rde_ui_text_editor_set_selection(_page->text_field, 0, _a, _b);
        }
    }
    rde_arr_free(&_out);
    if(_text != NULL) {
        rde_ui_text_editor_free_text(_page->text_field, _text);
    }
    rde_ui_node_focus(rde_ui_text_editor_as_node(_page->text_field));   // (the keyboard stays up)
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// The material chips (naming a board): each in its wood, the one it is made of ringed in the accent.
void fude_zoom_page_style_material_chips(fude_zoom_page* _page) {
    const fude_theme* _t = fude_theme_active();
    for(u32 _i = 0; _i < FUDE_ZOOM_MATERIAL_COUNT; _i++) {
        if(_page->material_chips[_i] == NULL) {
            continue;
        }
        const b8 _on = _i == _page->board_material;
        const rde_color _wood = fude_zoom_material_wood((u8)_i);
        fude_kit_button_colors(_page->material_chips[_i], _wood, _on ? 3.0f : 1.0f, _on ? _t->accent : _t->outline);
        fude_kit_button_round(_page->material_chips[_i], 999.0f);
        rde_ui_label_set_color(_page->material_chips[_i]->internal_label, fude_kit_text_on(_wood));   // (the kit's label is the theme's on a colour of its own)
    }
}

// See page_parts.h.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_material(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_page* _page = fude_zoom_page_the;
    const u32 _k = (u32)(uintptr_t)_user_data;
    if(_page != NULL && _k < FUDE_ZOOM_MATERIAL_COUNT) {
        _page->board_material = (u8)_k;
        fude_zoom_page_style_material_chips(_page);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

#define FUDE_ZOOM_PAGE_CHIP_H   36.0f   // a material chip's height (card units)
#define FUDE_ZOOM_PAGE_CHIP_GAP 8.0f    // between chips, and between their rows

// The chips laid out in rows across _width (each as wide as its name: never cut), their
// first row's top at _top (card units, Y up), shown or not. How tall the rows are.
RDE_INTERNAL f32 fude_zoom_page_place_material_chips(fude_zoom_page* _page, f32 _left, f32 _top, f32 _width, b8 _show) {
    f32 _x = 0.0f, _y = 0.0f;
    u32 _rows = 0;
    for(u32 _i = 0; _i < FUDE_ZOOM_MATERIAL_COUNT; _i++) {
        rde_ui_button* _chip = _page->material_chips[_i];
        if(_chip == NULL) {
            continue;
        }
        rde_ui_node_set_active(rde_ui_button_as_node(_chip), _show);
        if(!_show) {
            continue;
        }
        const c8* _name = _i == 0u ? fude_text(FUDE_TEXT_ZOOM_MATERIAL_NONE) : _page->material_words[_i];
        const f32 _w = rde_math_min_f32(_width, rde_math_max_f32(64.0f, 28.0f + fude_draw_text_width(fude_kit_font(), (f32)FUDE_KIT_FONT_SIZE, _name, FUDE_KIT_TEXT_SCALE * (f32)FUDE_KIT_FONT_SIZE)));
        if(_rows == 0u || _x + _w > _width + 0.5f) {
            _x = 0.0f;
            _y = (f32)_rows * (FUDE_ZOOM_PAGE_CHIP_H + FUDE_ZOOM_PAGE_CHIP_GAP);
            _rows++;
        }
        rde_ui_button_set_text(_chip, _name);
        fude_kit_place(rde_ui_button_as_node(_chip), (rde_vec_2F){ _left + _x + _w * 0.5f, _top - _y - FUDE_ZOOM_PAGE_CHIP_H * 0.5f }, (rde_vec_2F){ _w, FUDE_ZOOM_PAGE_CHIP_H });
        _x += _w + FUDE_ZOOM_PAGE_CHIP_GAP;
    }
    return _rows == 0u ? 0.0f : (f32)_rows * FUDE_ZOOM_PAGE_CHIP_H + (f32)(_rows - 1u) * FUDE_ZOOM_PAGE_CHIP_GAP;
}

// Writing on the page, defined below.
RDE_INTERNAL void fude_zoom_page_inline_begin(fude_zoom_page* _page);
RDE_INTERNAL void fude_zoom_page_inline_bar_show(fude_zoom_page* _page, b8 _show);

// See page_parts.h. A text or a sticky note is written on the page instead (fude_zoom_page_inline_begin), and putting
// it away then only hides its bar and the keyboard. The card is laid out for its style: one line for a name, a board's
// material chips, room for a formula set as it is typed, a note's marks' buttons.
void fude_zoom_page_text_card(fude_zoom_page* _page, b8 _show) {
    if(_page->text_field == NULL) {
        return;
    }
    // A text or a sticky note: written on the page where it is, not on the card.
    if(_show && !_page->inline_on && _page->inline_field != NULL &&
       (_page->text_style == FUDE_ZOOM_TEXT_PLAIN || _page->text_style == FUDE_ZOOM_TEXT_STICKY)) {
        fude_zoom_page_inline_begin(_page);
        return;
    }
    if(!_show && _page->inline_on) {
        fude_zoom_page_inline_bar_show(_page, false);
        rde_ui_canvas_blur(_page->app->ui->canvas);   // (the keyboard down)
        return;
    }
    fude_kit_modal_show(&_page->text_card, _show);
    if(!_show) {
        rde_ui_canvas_blur(_page->app->ui->canvas);   // (the keyboard down with it: a Find left it up on Android)
    }
    if(_show) {
        _page->text_opened_at = rde_engine_get_time_now();
        // High on the screen, over where the keyboard comes. Naming a board: its name a line, what
        // it is made of under it (the material chips).
        const rde_vec_2F _screen = fude_kit_screen_size(_page->app->window);
        const b8         _board  = _page->text_style == FUDE_ZOOM_PAGE_TEXT_BOARD;
        const b8         _area   = FUDE_ZOOM_PAGE_TEXT_NAMES(_page->text_style);   // (a name: one line, as a board's)
        rde_vec_2F       _size   = { rde_math_min_f32(560.0f, _screen.x - 32.0f), 236.0f };
        fude_zoom_page_material_words(_page->material_words, "");
        const f32 _chips = fude_zoom_page_place_material_chips(_page, 16.0f, 0.0f, _size.x - 32.0f, _board);   // (how tall, first)
        if(_board) {
            _size.y = 16.0f + 52.0f + 14.0f + _chips + 14.0f + 44.0f + 16.0f;
            fude_zoom_page_place_material_chips(_page, 16.0f, _size.y - 16.0f - 52.0f - 14.0f, _size.x - 32.0f, true);
            fude_zoom_page_style_material_chips(_page);
        }
        if(_area) {
            _size.y = 16.0f + 52.0f + 14.0f + 44.0f + 16.0f;
        }
        if(_page->text_style == FUDE_ZOOM_TEXT_FORMULA) {
            _size.y += FUDE_ZOOM_PAGE_FORMULA_PREVIEW;   // (room for it set as it is typed: fude_zoom_page_render_top)
        }
        // A note's words (a text, a sticky note): its marks' buttons in a row over Cancel and Save.
        const b8 _marks = _page->text_style == FUDE_ZOOM_TEXT_PLAIN || _page->text_style == FUDE_ZOOM_TEXT_STICKY;
        if(_marks) {
            _size.y += 52.0f;
        }
        const rde_vec_2F _at = { _screen.x * 0.5f, _screen.y - 90.0f - _size.y * 0.5f };
        _page->text_card_at   = _at;
        _page->text_card_size = _size;
        fude_kit_modal_place(&_page->text_card, _page->app->window, _at, _size);
        if(_board || _area) {
            fude_kit_place(fude_kit_field_node(_page->text_field), (rde_vec_2F){ _size.x * 0.5f, _size.y - 16.0f - 26.0f }, (rde_vec_2F){ _size.x - 32.0f, 52.0f });
        } else {
            fude_kit_place(fude_kit_field_node(_page->text_field), (rde_vec_2F){ _size.x * 0.5f, _size.y - 16.0f - 72.0f }, (rde_vec_2F){ _size.x - 32.0f, 144.0f });
        }
        {
            const f32 _gap = 6.0f, _side = rde_math_min_f32(44.0f, (_size.x - 32.0f - _gap * (f32)(FUDE_ZOOM_RICH_DOS - 1u)) / (f32)FUDE_ZOOM_RICH_DOS);
            const f32 _link = rde_math_min_f32(_size.x - 32.0f - (_side + _gap) * (f32)(FUDE_ZOOM_RICH_DOS - 1u), 96.0f);   // (its word: wider)
            f32 _x = 16.0f;
            for(u32 _i = 0; _i < FUDE_ZOOM_RICH_DOS; _i++) {
                const f32 _w = _i + 1u == FUDE_ZOOM_RICH_DOS ? _link : _side;
                rde_ui_node_set_active(rde_ui_button_as_node(_page->text_formats[_i]), _marks);
                fude_kit_place(rde_ui_button_as_node(_page->text_formats[_i]), (rde_vec_2F){ _x + _w * 0.5f, 16.0f + 44.0f + 12.0f + 20.0f }, (rde_vec_2F){ _w, 40.0f });
                _x += _w + _gap;
            }
            // (B in bold letters, I slanted: as they will be)
            if(_page->renderer.font_bold != NULL) {
                rde_ui_label_set_font(_page->text_formats[FUDE_ZOOM_RICH_DO_BOLD]->internal_label, _page->renderer.font_bold);
            }
            if(_page->renderer.font_italic != NULL) {
                rde_ui_label_set_font(_page->text_formats[FUDE_ZOOM_RICH_DO_ITALIC]->internal_label, _page->renderer.font_italic);
            }
            if(_page->renderer.font_bold != NULL) {
                rde_ui_label_set_font(_page->text_formats[FUDE_ZOOM_RICH_DO_HEADING]->internal_label, _page->renderer.font_bold);
            }
        }
        fude_kit_place(rde_ui_button_as_node(_page->text_cancel), (rde_vec_2F){ _size.x - 16.0f - 120.0f - 8.0f - 60.0f, 16.0f + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
        fude_kit_place(rde_ui_button_as_node(_page->text_save), (rde_vec_2F){ _size.x - 16.0f - 60.0f, 16.0f + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
        rde_ui_node_focus(rde_ui_text_editor_as_node(_page->text_field));   // the keyboard comes up
    }
}

// --- writing on the page (0.1.90) ---------------------------------------------------------------------
//
// A text or a sticky note is written where it is: its words drawn on the page as they are written (rich.h's lay_raw:
// its marks faint, its words styled), a caret and the selection drawn by the page, the keyboard a hidden field's
// (inline_field, which is text_field while it is up, so the card's Save reads it). A tap in its words moves the caret,
// a drag selects; a tap off it, Done, Back or another tool: it is saved (one undo step), as the card's Save did. Its
// bar of marks floats over it: B I H, lists, numbers, tasks, a table, a link (rich.h's fude_zoom_rich_format), cut,
// copy, paste, Done.

#define FUDE_ZOOM_PAGE_INLINE_BTN   40.0f   // its bar's buttons
#define FUDE_ZOOM_PAGE_INLINE_GAP   4.0f    // between them
#define FUDE_ZOOM_PAGE_INLINE_LINK  64.0f   // (its link's: a word)
#define FUDE_ZOOM_PAGE_INLINE_REACH 14.0    // screen points round its words a tap still lands in them
#define FUDE_ZOOM_PAGE_INLINE_KEYS  24.0    // screen points its caret is kept over the keyboard

// Its words now (allocated: freed with rde_ui_text_editor_free_text), and how long.
RDE_INTERNAL c8* fude_zoom_page_inline_words(fude_zoom_page* _page, u32* _len) {
    const usize _bytes = rde_ui_text_editor_get_byte_count(_page->inline_field);
    c8* _text = rde_ui_text_editor_get_text(_page->inline_field, 0, _bytes);
    *_len = _text != NULL ? (u32)rde_strlen(_text) : 0u;
    return _text;
}

// Starts writing on the page: the card's words moved to the hidden field (text_field is that field until it ends), and
// where the text is: an edited one's own place and size (the renderer leaves it out meanwhile), or a new one's where
// the tool went down, at the tool's size on the screen now.
RDE_INTERNAL void fude_zoom_page_inline_begin(fude_zoom_page* _page) {
    fude_zoom_scene* _s = &_page->scene;
    // Its words: the card's field's, as the text tool's tap or Edit filled it.
    {
        const usize _bytes = rde_ui_text_editor_get_byte_count(_page->card_field);
        c8* _text = rde_ui_text_editor_get_text(_page->card_field, 0, _bytes);
        const usize _had = rde_ui_text_editor_get_byte_count(_page->inline_field);
        if(_had > 0u) {
            rde_ui_text_editor_delete_range(_page->inline_field, 0, _had);
        }
        if(_text != NULL) {
            rde_ui_text_editor_insert_at(_page->inline_field, 0, _text, rde_strlen(_text));
            rde_ui_text_editor_set_cursor(_page->inline_field, 0, rde_strlen(_text));
            rde_ui_text_editor_free_text(_page->card_field, _text);
        }
    }
    _page->text_field = _page->inline_field;
    const b8 _sticky = _page->text_style == FUDE_ZOOM_TEXT_STICKY;
    const b8 _editing = _page->text_object != FUDE_ZOOM_NONE && _page->text_object < fude_zoom_scene_object_count(_s);
    if(_editing) {
        // Where it is: its own units (its top left at 0, 0) in its frame, as it was.
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _page->text_object);
        f64 _size = 0.0, _w = 0.0, _h = 0.0;
        fude_zoom_scene_text(_s, _page->text_object, &_size, &_w, &_h, NULL, NULL, NULL);
        _page->inline_frame = _o->frame;
        _page->inline_sim   = fude_zoom_object_sim(_o);
        _page->inline_size  = _size;
        _page->inline_w     = _sticky ? _w : 0.0;
        _page->inline_h     = _sticky ? _h : 0.0;
        _page->inline_ink   = _o->color;
        _page->inline_paper = _o->fill;
    } else {
        // A new one where the tool went down, its letters the tool's on the screen now.
        const f64 _k = rde_math_max_f64(fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, _page->text_frame)), 1e-300);
        _page->inline_frame = _page->text_frame;
        _page->inline_sim   = (fude_zoom_sim){ 1.0, 0.0, _page->text_at.x, _page->text_at.y };
        _page->inline_size  = (_sticky ? FUDE_ZOOM_PAGE_STICKY_PX : FUDE_ZOOM_PAGE_TEXT_PX) / _k;
        _page->inline_w     = _sticky ? _page->inline_size * (FUDE_ZOOM_PAGE_STICKY_W + 1.2) : 0.0;
        _page->inline_h     = _sticky ? _page->inline_size * (FUDE_ZOOM_PAGE_TEXT_LINE + 1.2) : 0.0;
        _page->inline_ink   = _sticky ? (rde_color){ 46, 46, 54, 255 } : _page->app->ink->color;
        _page->inline_paper = (rde_color){ 255, 233, 140, 255 };
    }
    if(!_page->inline_rich_made) {
        fude_zoom_rich_init(&_page->inline_rich);
        _page->inline_rich_made = true;
    }
    _page->renderer.skip    = _editing ? _page->text_object : FUDE_ZOOM_NONE;
    _page->inline_on        = true;
    _page->inline_started   = rde_engine_get_time_now();
    _page->inline_selecting = false;
    _page->inline_box       = (fude_zoom_box){ 1e9, 1e9, -1e9, -1e9 };
    _page->text_opened_at   = _page->inline_started;
    fude_zoom_page_inline_bar_show(_page, true);
    rde_ui_node_focus(rde_ui_text_editor_as_node(_page->inline_field));   // the keyboard comes up
}

// Written: the page's own again (its words saved already: text_save).
RDE_INTERNAL void fude_zoom_page_inline_finish(fude_zoom_page* _page) {
    _page->inline_on        = false;
    _page->inline_selecting = false;
    _page->renderer.skip    = FUDE_ZOOM_NONE;
    _page->text_field       = _page->card_field;
    fude_zoom_page_inline_bar_show(_page, false);
    rde_ui_canvas_blur(_page->app->ui->canvas);   // (the keyboard down)
}

// See page_parts.h.
void fude_zoom_page_inline_end(fude_zoom_page* _page) {
    if(_page->inline_on) {
        fude_zoom_page_text_save(_page);
    }
}

// A point of the screen in its words' own layout (rich.h: from their top left, Y down).
RDE_INTERNAL void fude_zoom_page_inline_point(const fude_zoom_page* _page, rde_vec_2F _screen, f32* _x, f32* _y) {
    const f64 _k = rde_math_max_f64(_page->inline_k, 1e-300);
    *_x = (f32)(((f64)_screen.x - (f64)_page->inline_origin.x) / _k);
    *_y = (f32)(((f64)_page->inline_origin.y - (f64)_screen.y) / _k);
}

// See page_parts.h. In its words (within FUDE_ZOOM_PAGE_INLINE_REACH), the caret there and a drag from it selects; off
// it, it is saved, and the press goes no further. True: taken.
b8 fude_zoom_page_inline_down(fude_zoom_page* _page, rde_vec_2F _screen) {
    if(!_page->inline_on) {
        return false;
    }
    const fude_zoom_box _b = _page->inline_box;
    const f64 _r = FUDE_ZOOM_PAGE_INLINE_REACH;
    if(_screen.x >= _b.min_x - _r && _screen.x <= _b.max_x + _r && _screen.y >= _b.min_y - _r && _screen.y <= _b.max_y + _r) {
        u32 _len = 0;
        c8* _text = fude_zoom_page_inline_words(_page, &_len);
        f32 _x, _y;
        fude_zoom_page_inline_point(_page, _screen, &_x, &_y);
        const u32 _at = fude_zoom_rich_byte_at(&_page->inline_rich, _text != NULL ? _text : "", _len, _x, _y, fude_zoom_render_measure, &_page->renderer);
        if(_text != NULL) {
            rde_ui_text_editor_free_text(_page->inline_field, _text);
        }
        rde_ui_text_editor_set_cursor(_page->inline_field, 0, _at);
        _page->inline_anchor    = _at;
        _page->inline_selecting = true;
        _page->inline_started   = rde_engine_get_time_now();   // (the caret shown at once)
        rde_ui_node_focus(rde_ui_text_editor_as_node(_page->inline_field));
        return true;
    }
    fude_zoom_page_inline_end(_page);
    return true;
}

// See page_parts.h: from the caret where the press went down to the byte under the hand.
b8 fude_zoom_page_inline_moved(fude_zoom_page* _page, rde_vec_2F _screen) {
    if(!_page->inline_on || !_page->inline_selecting) {
        return false;
    }
    u32 _len = 0;
    c8* _text = fude_zoom_page_inline_words(_page, &_len);
    f32 _x, _y;
    fude_zoom_page_inline_point(_page, _screen, &_x, &_y);
    const u32 _at = fude_zoom_rich_byte_at(&_page->inline_rich, _text != NULL ? _text : "", _len, _x, _y, fude_zoom_render_measure, &_page->renderer);
    if(_text != NULL) {
        rde_ui_text_editor_free_text(_page->inline_field, _text);
    }
    if(_at == _page->inline_anchor) {
        rde_ui_text_editor_set_cursor(_page->inline_field, 0, _at);
    } else {
        rde_ui_text_editor_set_selection(_page->inline_field, 0, _page->inline_anchor, _at);
    }
    return true;
}

// See page_parts.h.
b8 fude_zoom_page_inline_up(fude_zoom_page* _page) {
    if(!_page->inline_on || !_page->inline_selecting) {
        return false;
    }
    _page->inline_selecting = false;
    return true;
}

// See page_parts.h. Its words drawn on the page as written, its caret, its selection; its bar placed over it; the
// keyboard kept off it (the camera moved up as far as the keyboard covers the caret). It also keeps inline_box,
// inline_k and inline_origin, where the presses find its words.
void fude_zoom_page_render_inline(fude_zoom_page* _page) {
    if(!_page->inline_on || !_page->open || _page->inline_field == NULL) {
        return;
    }
    fude_zoom_scene* _s = &_page->scene;
    const fude_theme* _t = fude_theme_active();
    u32 _len = 0;
    c8* _text = fude_zoom_page_inline_words(_page, &_len);
    const c8* _w = _text != NULL ? _text : "";
    const b8  _sticky = _page->text_style == FUDE_ZOOM_TEXT_STICKY;
    const f64 _size = _page->inline_size;
    const f64 _pad  = _sticky ? _size * 0.6 : 0.0;
    fude_zoom_rich_lay_raw(&_page->inline_rich, _w, _len, _sticky ? (f32)rde_math_max_f64(_page->inline_w - 2.0 * _pad, _size) : 0.0f, (f32)_size,
                           (f32)FUDE_ZOOM_PAGE_TEXT_LINE, fude_zoom_render_measure, &_page->renderer);
    const fude_zoom_rich* _r = &_page->inline_rich;
    // Where it is on the screen: upright, as texts are drawn (render_text.c), its top left where its own is.
    const fude_zoom_sim _all = fude_zoom_sim_compose(fude_zoom_page_frame_sim(_page, _page->inline_frame), _page->inline_sim);
    const f64 _k = fude_zoom_sim_scale(_all);
    const fude_zoom_v2 _tl = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 0.0 });
    const f64 _bw = _sticky ? _page->inline_w : rde_math_max_f64((f64)_r->w, _size * 4.0) + _size * 0.3;
    const f64 _bh = _sticky ? rde_math_max_f64(_page->inline_h, (f64)_r->h + 2.0 * _pad) : rde_math_max_f64((f64)_r->h, _size * FUDE_ZOOM_PAGE_TEXT_LINE) + _size * 0.2;
    _page->inline_box    = (fude_zoom_box){ _tl.x, _tl.y - _bh * _k, _tl.x + _bw * _k, _tl.y };
    _page->inline_k      = _k;
    _page->inline_origin = (rde_vec_2F){ (f32)(_tl.x + _pad * _k), (f32)(_tl.y - _pad * _k) };
    const rde_vec_2F _mid = { (f32)(_tl.x + _bw * _k * 0.5), (f32)(_tl.y - _bh * _k * 0.5) }, _sz = { (f32)(_bw * _k), (f32)(_bh * _k) };
    const rde_color _ink = fude_theme_resolve(_page->inline_ink);
    if(_sticky) {
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ _mid.x + (f32)(_size * _k * 0.12), _mid.y - (f32)(_size * _k * 0.16) }, _sz, (rde_color){ 0, 0, 0, 40 });
        rde_rendering_2d_draw_rectangle(_mid, _sz, _page->inline_paper);
    }
    {
        // (its edges, faintly: where it is being written)
        rde_color _edge = _t->accent;
        _edge.a = 140;
        const f32 _m = 4.0f;
        rde_rendering_2d_draw_rectangle_border(_mid, (rde_vec_2F){ _sz.x + 2.0f * _m, _sz.y + 2.0f * _m }, 1.2f, _edge, NULL);
    }
    const f64 _ox = (f64)_page->inline_origin.x, _oy = (f64)_page->inline_origin.y;
    #define FZI_AT(_x, _y) (rde_vec_2F){ (f32)(_ox + (f64)(_x) * _k), (f32)(_oy - (f64)(_y) * _k) }
    // Its selection, under its words.
    const rde_ui_text_editor_cursor _cur = rde_ui_text_editor_get_primary_cursor(_page->inline_field);
    const u32 _index = (u32)(_cur.index <= _len ? _cur.index : _len);
    const fude_zoom_rich_run* _q = (const fude_zoom_rich_run*)_r->runs.memory;
    if(_cur.anchor >= 0 && (u32)_cur.anchor != _index) {
        const u32 _s0 = (u32)_cur.anchor < _index ? (u32)_cur.anchor : _index, _s1 = (u32)_cur.anchor < _index ? _index : (u32)_cur.anchor;
        rde_color _sel = _t->accent;
        _sel.a = 70;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_r->runs); _i++) {
            const fude_zoom_rich_run _b = _q[_i];
            const u32 _a = _b.from > _s0 ? _b.from : _s0, _e = _b.from + _b.len < _s1 ? _b.from + _b.len : _s1;
            if(_a >= _e) {
                continue;
            }
            const f32 _x0 = _b.x + fude_zoom_render_measure(&_page->renderer, &_w[_b.from], _a - _b.from, _b.px, _b.style);
            const f32 _x1 = _b.x + fude_zoom_render_measure(&_page->renderer, &_w[_b.from], _e - _b.from, _b.px, _b.style);
            const rde_vec_2F _p0 = FZI_AT(_x0, _b.y - _b.px * 0.95f), _p1 = FZI_AT(_x1, _b.y + _b.px * 0.3f);
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ (_p0.x + _p1.x) * 0.5f, (_p0.y + _p1.y) * 0.5f }, (rde_vec_2F){ rde_math_abs_f32(_p1.x - _p0.x), rde_math_abs_f32(_p1.y - _p0.y) }, _sel);
        }
    }
    // Its words: styled as they will be, its marks faint.
    c8 _small[512];
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_r->runs); _i++) {
        const fude_zoom_rich_run _b = _q[_i];
        const f64 _px = (f64)_b.px * _k;
        if(_px < 3.0 || _px > 3000.0 || _page->renderer.font == NULL) {
            continue;
        }
        const u32 _n = _b.len < sizeof(_small) - 1u ? _b.len : (u32)sizeof(_small) - 1u;
        rde_memcpy(_small, &_w[_b.from], _n);
        _small[_n] = 0;
        rde_color _c = (_b.style & FUDE_ZOOM_RICH_LINK) ? _page->renderer.link_color : _ink;
        if(_b.style & FUDE_ZOOM_RICH_MARK) {
            _c.a = (u8)((u32)_c.a * 40u / 100u);
        } else if(_b.style & FUDE_ZOOM_RICH_SOFT) {
            _c.a = (u8)((u32)_c.a * 55u / 100u);
        }
        const u8 _face = (u8)(_b.style & (FUDE_ZOOM_RICH_BOLD | FUDE_ZOOM_RICH_ITALIC));
        rde_font* _font = _face == (FUDE_ZOOM_RICH_BOLD | FUDE_ZOOM_RICH_ITALIC) && _page->renderer.font_bold_italic != NULL ? _page->renderer.font_bold_italic :
                          _face == FUDE_ZOOM_RICH_BOLD && _page->renderer.font_bold != NULL ? _page->renderer.font_bold :
                          _face == FUDE_ZOOM_RICH_ITALIC && _page->renderer.font_italic != NULL ? _page->renderer.font_italic : _page->renderer.font;
        const rde_vec_2F _at = FZI_AT(_b.x, _b.y);
        fude_draw_text_placed(_font, _small, (rde_vec_3F){ _at.x, _at.y, 0.0f }, (rde_vec_2F){ (f32)_px / _page->renderer.font_px, (f32)_px / _page->renderer.font_px }, 0.0f, _c);
        if(_b.style & FUDE_ZOOM_RICH_STRUCK) {
            rde_rendering_2d_draw_line_1(FZI_AT(_b.x, _b.y - _b.px * 0.3f), FZI_AT(_b.x + _b.w, _b.y - _b.px * 0.3f), _c, (f32)rde_math_max_f64(1.0, _px * 0.07));
        }
    }
    // Its caret (blinking), the hidden field by it (the keyboard's candidates there).
    f32 _cx = 0.0f, _cy = 0.0f, _cpx = 0.0f;
    fude_zoom_rich_caret(_r, _w, _len, _index, fude_zoom_render_measure, &_page->renderer, &_cx, &_cy, &_cpx);
    const rde_vec_2F _c0 = FZI_AT(_cx, _cy + _cpx * 0.25f), _c1 = FZI_AT(_cx, _cy - _cpx * 0.95f);
    const f64 _blink = rde_math_fmod_f64(rde_engine_get_time_now() - _page->inline_started, 1.0);
    if(_blink < 0.6) {
        rde_rendering_2d_draw_line_1(_c0, _c1, _t->accent, 2.0f);
    }
    fude_redraw_after(_blink < 0.6 ? 0.6 - _blink : 1.0 - _blink);   // (render on demand: the caret's next blink)
    #undef FZI_AT
    if(_text != NULL) {
        rde_ui_text_editor_free_text(_page->inline_field, _text);
    }
    const rde_vec_2F _ui = fude_kit_screen_size(_page->app->window);
    fude_kit_place(rde_ui_text_editor_as_node(_page->inline_field), (rde_vec_2F){ -40.0f, -40.0f }, (rde_vec_2F){ 2.0f, 2.0f });   // (off the screen: the page draws its words)
    // The keyboard kept off its caret: the view up, as far as it covers it.
    const f32 _keys = rde_keyboard_get_height();
    if(_keys > 0.0f) {
        const rde_vec_2I _win = rde_window_get_size(_page->app->window);
        const f32 _top = fude_app_window_to_screen(_page->app, (rde_vec_2F){ 0.0f, (f32)_win.y - _keys }).y;
        const f32 _need = _top + (f32)FUDE_ZOOM_PAGE_INLINE_KEYS - _c0.y;
        if(_need > 0.5f && _s->camera.z > 0.0) {
            fude_redraw_changed();   // (render on demand: the keyboard rises with no event of the app's)
            _s->camera.at.y -= (f64)_need / _s->camera.z;
            fude_zoom_camera_settle(_s, fude_zoom_page_half(_page));
        }
    }
    // Its bar: over it, or under it where there is no room over it — across the screen as far as it may.
    if(_page->inline_bar != NULL) {
        const f32 _bar_w = (f32)(FUDE_ZOOM_PAGE_INLINE_BUTTONS - 1u) * (FUDE_ZOOM_PAGE_INLINE_BTN + FUDE_ZOOM_PAGE_INLINE_GAP) + FUDE_ZOOM_PAGE_INLINE_LINK +
                           FUDE_ZOOM_PAGE_INLINE_GAP + 16.0f;
        const f32 _bar_h = FUDE_ZOOM_PAGE_INLINE_BTN + 12.0f;
        const fude_zoom_v2 _half = fude_zoom_page_half(_page);
        f32 _bx = (f32)((_page->inline_box.min_x + _page->inline_box.max_x) * 0.5);
        _bx = rde_math_min_f32(rde_math_max_f32(_bx, (f32)-_half.x + _bar_w * 0.5f + 8.0f), (f32)_half.x - _bar_w * 0.5f - 8.0f);
        f32 _by = (f32)_page->inline_box.max_y + 14.0f + _bar_h * 0.5f;
        if(_by + _bar_h * 0.5f > (f32)_half.y - 70.0f) {
            _by = (f32)_page->inline_box.min_y - 14.0f - _bar_h * 0.5f;   // (no room over it: under it)
        }
        {
            f64 _x0 = (f64)(_bx - _bar_w * 0.5f), _x1 = (f64)(_bx + _bar_w * 0.5f);
            fude_zoom_page_clear_of_bar(_page, &_x0, &_x1, (f64)(_by - _bar_h * 0.5f), (f64)(_by + _bar_h * 0.5f), (f64)_bar_w);   // (wherever the toolbar is)
            _bx = (f32)((_x0 + _x1) * 0.5);
        }
        _page->inline_bar_at   = (rde_vec_2F){ _bx + _ui.x * 0.5f, _by + _ui.y * 0.5f };
        _page->inline_bar_size = (rde_vec_2F){ _bar_w, _bar_h };
        fude_kit_place(rde_ui_image_as_node(_page->inline_bar), _page->inline_bar_at, _page->inline_bar_size);
        f32 _x = 8.0f;
        for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_INLINE_BUTTONS; _i++) {
            const f32 _bw2 = _i == FUDE_ZOOM_RICH_DO_LINK ? FUDE_ZOOM_PAGE_INLINE_LINK : FUDE_ZOOM_PAGE_INLINE_BTN;
            fude_kit_place(rde_ui_button_as_node(_page->inline_buttons[_i]), (rde_vec_2F){ _x + _bw2 * 0.5f, _bar_h * 0.5f }, (rde_vec_2F){ _bw2, FUDE_ZOOM_PAGE_INLINE_BTN });
            _x += _bw2 + FUDE_ZOOM_PAGE_INLINE_GAP;
        }
    }
}

// Shows or hides the bar of a text written on the page (doing nothing when it already is so).
RDE_INTERNAL void fude_zoom_page_inline_bar_show(fude_zoom_page* _page, b8 _show) {
    if(_page->inline_bar == NULL || _page->inline_bar_shown == _show) {
        return;
    }
    _page->inline_bar_shown = _show;
    rde_ui_node_set_active(rde_ui_image_as_node(_page->inline_bar), _show);
    // (B in bold letters, I slanted, H bold: as they will be)
    if(_show && _page->renderer.font_bold != NULL) {
        rde_ui_label_set_font(_page->inline_buttons[FUDE_ZOOM_RICH_DO_BOLD]->internal_label, _page->renderer.font_bold);
        rde_ui_label_set_font(_page->inline_buttons[FUDE_ZOOM_RICH_DO_HEADING]->internal_label, _page->renderer.font_bold);
    }
    if(_show && _page->renderer.font_italic != NULL) {
        rde_ui_label_set_font(_page->inline_buttons[FUDE_ZOOM_RICH_DO_ITALIC]->internal_label, _page->renderer.font_italic);
    }
}

// Its bar's cut, copy, paste and Done (its marks: fude_zoom_page_on_text_format, as the card's).
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_inline_tool(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->inline_on) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    const u32 _what = (u32)(uintptr_t)_user_data;
    if(_what == 0u) {
        rde_ui_text_editor_cut(_page->inline_field);
    } else if(_what == 1u) {
        rde_ui_text_editor_copy(_page->inline_field);
    } else if(_what == 2u) {
        rde_ui_text_editor_paste(_page->inline_field);
    } else {
        fude_zoom_page_inline_end(_page);
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    rde_ui_node_focus(rde_ui_text_editor_as_node(_page->inline_field));
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Over the UI: the Formula card's formula set as it is typed (between its field and its buttons, as big as fits up to
// a canvas formula's size), and how many of its commands it did not know — so what is typed is seen before it is put.
void fude_zoom_page_render_top(fude_zoom_page* _page) {
    if(_page->text_field == NULL || !_page->text_card.shown || _page->text_style != FUDE_ZOOM_TEXT_FORMULA || !_page->formulas.ready) {
        return;
    }
    const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
    if(_bytes == 0) {
        return;
    }
    c8* _text = rde_ui_text_editor_get_text(_page->text_field, 0, _bytes);
    if(_text == NULL) {
        return;
    }
    const fude_zoom_tex_box* _box = fude_zoom_formulas_set(&_page->formulas, _text, (u32)rde_strlen(_text));
    rde_ui_text_editor_free_text(_page->text_field, _text);
    if(_box == NULL || !(_box->width > 0.0)) {
        return;
    }
    const fude_theme*  _t      = fude_theme_active();
    const rde_vec_2F   _screen = fude_kit_screen_size(_page->app->window);
    // (its room, the card's own units — bottom left, Y up — to the drawing's: the middle of the screen)
    const f32 _left   = _page->text_card_at.x - _page->text_card_size.x * 0.5f + 16.0f - _screen.x * 0.5f;
    const f32 _bottom = _page->text_card_at.y - _page->text_card_size.y * 0.5f + 16.0f + 44.0f + 10.0f - _screen.y * 0.5f;
    const f32 _wide   = _page->text_card_size.x - 32.0f;
    const f32 _high   = FUDE_ZOOM_PAGE_FORMULA_PREVIEW - 20.0f;
    const u32 _unknown = _box->unknown;
    const f32 _note   = _unknown > 0u ? 18.0f : 0.0f;   // (a line under it saying so)
    const f64 _tall   = _box->height + _box->depth;
    const f64 _em     = rde_math_min_f64(FUDE_ZOOM_PAGE_FORMULA_PX * 1.25, rde_math_min_f64((f64)_wide / _box->width, (f64)(_high - _note) / rde_math_max_f64(_tall, 0.5)));
    const f64 _x      = (f64)_left + ((f64)_wide - _box->width * _em) * 0.5;
    const f64 _y      = (f64)_bottom + (f64)_note + ((f64)(_high - _note) - _tall * _em) * 0.5 + _box->depth * _em;
    fude_zoom_render_formula(&_page->renderer, _box, (fude_zoom_sim){ _em, 0.0, _x, _y }, _t->text);
    if(_unknown > 0u && _page->app->font != NULL) {
        c8 _say[160];
        FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_FORMULA_UNKNOWN, FUDE_TN(_unknown));
        const f32 _px  = 13.0f;
        const f32 _w   = fude_draw_text_width(_page->app->font, _page->app->font_px, _say, _px);
        fude_draw_text_placed(_page->app->font, _say, (rde_vec_3F){ _left + (_wide - _w) * 0.5f, _bottom + 2.0f, 0.0f },
                              (rde_vec_2F){ _px / _page->app->font_px, _px / _page->app->font_px }, 0.0f, _t->text_soft);
    }
}

// --- typed text written out by hand (hand.h) ---------------------------------------------------

// The typed texts selected (sticky notes are left as they are).
u32 fude_zoom_page_lassoed_texts(const fude_zoom_page* _page) {
    const fude_zoom_scene* _s = &_page->scene;
    u32 _n = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        const u32 _o = ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object;
        f64 _size, _w, _h;
        u8  _style;
        const c8* _text;
        u32 _len;
        if((fude_zoom_scene_object(_s, _o)->flags & FUDE_ZOOM_FLAG_ALIVE) && fude_zoom_scene_object(_s, _o)->kind == FUDE_ZOOM_KIND_TEXT &&
           fude_zoom_scene_text(_s, _o, &_size, &_w, &_h, &_style, &_text, &_len) && _style == FUDE_ZOOM_TEXT_PLAIN) {
            _n++;
        }
    }
    return _n;
}

// The formulas selected (texts of LaTeX: formula.h).
u32 fude_zoom_page_lassoed_formulas(const fude_zoom_page* _page) {
    const fude_zoom_scene* _s = &_page->scene;
    u32 _n = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        const u32 _o = ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object;
        u8 _style = 0;
        if((fude_zoom_scene_object(_s, _o)->flags & FUDE_ZOOM_FLAG_ALIVE) && fude_zoom_scene_object(_s, _o)->kind == FUDE_ZOOM_KIND_TEXT &&
           fude_zoom_scene_text(_s, _o, NULL, NULL, NULL, &_style, NULL, NULL) && _style == FUDE_ZOOM_TEXT_FORMULA) {
            _n++;
        }
    }
    return _n;
}

// The selection's Graph: its formulas (as calc's words: texcalc.h — "y = \frac{1}{x}", "x^2 + y^2 = 4") drawn on a
// graph to the right of them, as big on the screen as a graph comes, the lasso on it; one undo step. None it can draw:
// said so.
void fude_zoom_page_graph_formulas(fude_zoom_page* _page) {
    fude_zoom_scene* _s = &_page->scene;
    const u32 _kind = fude_zoom_symbol_find("graph");
    const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of(_kind);
    if(_info == NULL || _page->play_on) {
        return;
    }
    c8 _text[FUDE_ZOOM_SYMBOL_TEXT] = { 0 };
    usize _used = 0;
    u32 _lines = 0, _frame = FUDE_ZOOM_NONE;
    f64 _right = -1e300, _top = -1e300, _bottom = 1e300;   // (the formulas on the screen)
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        const u32 _o = ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object;
        const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _o);
        u8 _style = 0;
        const c8* _words = NULL;
        u32 _len = 0;
        if(!(_ob->flags & FUDE_ZOOM_FLAG_ALIVE) || _ob->kind != FUDE_ZOOM_KIND_TEXT || !fude_zoom_scene_text(_s, _o, NULL, NULL, NULL, &_style, &_words, &_len) ||
           _style != FUDE_ZOOM_TEXT_FORMULA) {
            continue;
        }
        c8 _calc[256];
        if(_lines >= 6u || !fude_zoom_texcalc_tidy(_words, _len, _calc, sizeof(_calc)) || _calc[0] == 0 || _used + rde_strlen(_calc) + 2u >= sizeof(_text)) {
            continue;
        }
        _used += (usize)rde_snprintf(_text + _used, sizeof(_text) - _used, "%s%s", _lines > 0u ? "\n" : "", _calc);
        _lines++;
        if(_frame == FUDE_ZOOM_NONE) {
            _frame = _ob->frame;
        }
        fude_zoom_v2 _c[4];
        fude_zoom_scene_text_corners(_s, _o, _c);
        const fude_zoom_sim _to = fude_zoom_page_frame_sim(_page, _ob->frame);
        for(u32 _k = 0; _k < 4u; _k++) {
            const fude_zoom_v2 _q = fude_zoom_sim_apply(_to, _c[_k]);
            _right = rde_math_max_f64(_right, _q.x);
            _top = rde_math_max_f64(_top, _q.y);
            _bottom = rde_math_min_f64(_bottom, _q.y);
        }
    }
    if(_lines == 0u) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_GRAPH_NOTHING));
        return;
    }
    // Beside them: its left edge a little right of theirs, its middle level with theirs — in the first one's frame.
    const fude_zoom_sim _fs = fude_zoom_page_frame_sim(_page, _frame);
    const f64 _k = rde_math_max_f64(fude_zoom_sim_scale(_fs), 1e-300);
    const fude_zoom_v2 _at_screen = { _right + 24.0 + (f64)_info->w * 0.5, 0.5 * (_top + _bottom) };
    const fude_zoom_v2 _mid = fude_zoom_sim_apply(fude_zoom_sim_inverse(_fs), _at_screen);
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS + 200];
    const u32 _count = fude_zoom_symbol_numbers(_n, _kind, (f64)_info->w * 0.5 / _k, (f64)_info->h * 0.5 / _k, FUDE_ZOOM_PAGE_SYMBOL_PX / _k, _text);
    const u32 _made = fude_zoom_scene_add_shape_fill(_s, _frame, (fude_zoom_place){ _mid, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_SYMBOL, _n, _count, _page->app->ink->color,
                                                     (f32)(1.0 / _k), FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN, FUDE_THEME_PAGE_FILL, 0);
    fude_zoom_history_push(_s, _frame, fude_zoom_scene_object(_s, _made)->box, NULL, 0, &_made, 1);
    fude_zoom_select_clear(&_page->selection);
    rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _made });
    fude_zoom_select_update(&_page->selection, _s);
}

// The selection's formulas written out by hand (handmath.h): each its strokes where it was, as big, in its colour, on
// its layer, in place of it. One undo step.
void fude_zoom_page_formula_hand_selection(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->formulas.ready) {
        return;
    }
    fude_zoom_scene*      _s    = &_page->scene;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_handmath_chars_read(&_page->hand_chars, &_page->formulas.font);
    rde_arr _died = rde_arr_new(sizeof(u32), _heap), _born = rde_arr_new(sizeof(u32), _heap);
    rde_arr _pts  = rde_arr_new(sizeof(fude_zoom_v2), _heap), _ends = rde_arr_new(sizeof(u32), _heap), _path = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    u32 _frame = FUDE_ZOOM_NONE;
    const u16 _was = _s->layer;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        const u32 _o = ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object;
        const fude_zoom_object _obj = *fude_zoom_scene_object(_s, _o);
        f64 _size = 0.0;
        u8  _style = 0;
        const c8* _text = NULL;
        u32 _len = 0;
        if(!(_obj.flags & FUDE_ZOOM_FLAG_ALIVE) || _obj.kind != FUDE_ZOOM_KIND_TEXT || !fude_zoom_scene_text(_s, _o, &_size, NULL, NULL, &_style, &_text, &_len) ||
           _style != FUDE_ZOOM_TEXT_FORMULA || _len == 0) {
            continue;
        }
        const fude_zoom_tex_box* _box = fude_zoom_formulas_set(&_page->formulas, _text, _len);
        if(_box == NULL) {
            continue;
        }
        rde_arr_clear(&_pts);
        rde_arr_clear(&_ends);
        fude_zoom_handmath_write(&_page->hand_chars, &_page->formulas.font, _box, _size, (u32)(_obj.id & 0xFFFFFFFFu), &_pts, &_ends, NULL);
        // (its baseline where the drawn formula's is: its pad in from the box's top left — render_text.c)
        const f64 _ox = FUDE_ZOOM_FORMULA_PAD * _size, _oy = -(FUDE_ZOOM_FORMULA_PAD + _box->height) * _size;
        const f64 _c = rde_math_cos_f64(_obj.rotation), _sn = rde_math_sin_f64(_obj.rotation);
        const fude_zoom_v2* _p = (const fude_zoom_v2*)_pts.memory;
        const u32*          _e = (const u32*)_ends.memory;
        _s->layer = _obj.layer;
        for(u32 _k = 0, _from = 0; _k < (u32)rde_arr_length(&_ends); _from = _e[_k], _k++) {
            rde_arr_clear(&_path);
            for(u32 _j = _from; _j < _e[_k]; _j++) {
                const fude_zoom_v2 _l = { (_p[_j].x + _ox) * _obj.scale, (_p[_j].y + _oy) * _obj.scale };
                const fude_zoom_v2 _f = { _obj.t.x + _l.x * _c - _l.y * _sn, _obj.t.y + _l.x * _sn + _l.y * _c };
                rde_arr_add(&_path, (any)&_f);
            }
            fude_zoom_page_add_path(_page, _obj.frame, (const fude_zoom_v2*)_path.memory, (u32)rde_arr_length(&_path), false, _obj.color,
                                    (f32)(_size * 0.04 * _obj.scale), &_born);
        }
        fude_zoom_scene_set_alive(_s, _o, false);
        rde_arr_add(&_died, (any)&_o);
        _frame = _frame == FUDE_ZOOM_NONE ? _obj.frame : _frame;
    }
    _s->layer = _was;
    if(rde_arr_length(&_died) > 0) {
        fude_zoom_box _box = fude_zoom_scene_object(_s, ((const u32*)_died.memory)[0])->box;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_born); _i++) {
            _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, ((const u32*)_born.memory)[_i])->box);
        }
        fude_zoom_history_push(_s, _frame, _box, (const u32*)_died.memory, (u32)rde_arr_length(&_died), (const u32*)_born.memory, (u32)rde_arr_length(&_born));
        fude_zoom_select_clear(&_page->selection);
    }
    rde_arr_free(&_died);
    rde_arr_free(&_born);
    rde_arr_free(&_pts);
    rde_arr_free(&_ends);
    rde_arr_free(&_path);
}

// The selection's handwriting read as a formula (mathread.h): its pen strokes (not a marker's), in the order they were
// drawn, read; a formula in their place as tall as they were, in the first one's colour, and only those strokes gone.
// One undo step; the lasso on it. Nothing read: said so.
void fude_zoom_page_formula_read_selection(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return;
    }
    fude_zoom_scene*      _s    = &_page->scene;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _strokes = rde_arr_new(sizeof(u32), _heap);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        fude_zoom_page_take_strokes(_s, ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object, &_strokes, 0);
    }
    rde_arr_sort(&_strokes, fude_zoom_by_u32);   // (in the order drawn: an object's place among them)
    const u32* _list = (const u32*)_strokes.memory;
    const u32 _n = (u32)rde_arr_length(&_strokes);
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap), _ends = rde_arr_new(sizeof(u32), _heap), _q = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
    rde_arr _read_arr = rde_arr_new(sizeof(u32), _heap);   // the strokes read: only these become the formula
    const u32 _frame = _n > 0u ? fude_zoom_scene_object(_s, _list[0])->frame : FUDE_ZOOM_NONE;
    const fude_zoom_sim _back = _n > 0u ? fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _frame)) : (fude_zoom_sim){ 1.0, 0.0, 0.0, 0.0 };
    fude_zoom_box _ink = fude_zoom_box_empty();
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _list[_i]);
        if(_o->count == 0u || (_o->flags & FUDE_ZOOM_FLAG_MARKER)) {
            continue;
        }
        rde_arr_clear(&_q);
        rde_arr_add_n(&_q, _o->count);
        if(!fude_zoom_scene_points(_s, _list[_i], (fude_zoom_qpoint*)_q.memory)) {
            continue;
        }
        rde_arr_add(&_read_arr, (any)&_list[_i]);
        // (every one in the first one's frame's units)
        const fude_zoom_sim _to = fude_zoom_sim_compose(_back, fude_zoom_page_frame_sim(_page, _o->frame));
        for(u32 _p = 0; _p < _o->count; _p++) {
            const fude_zoom_v2 _at = fude_zoom_sim_apply(_to, fude_zoom_scene_point_at(_o, &((const fude_zoom_qpoint*)_q.memory)[_p]));
            rde_arr_add(&_pts, (any)&_at);
            _ink = fude_zoom_box_union(_ink, (fude_zoom_box){ _at.x, _at.y, _at.x, _at.y });
        }
        const u32 _end = (u32)rde_arr_length(&_pts);
        rde_arr_add(&_ends, (any)&_end);
    }
    c8 _latex[1024];
    u32 _symbols = 0;
    const b8 _read = rde_arr_length(&_ends) > 0u && fude_zoom_mathread((const fude_zoom_v2*)_pts.memory, (const u32*)_ends.memory, (u32)rde_arr_length(&_ends), _latex,
                                                                      sizeof(_latex), &_symbols);
    const fude_zoom_tex_box* _box = _read ? fude_zoom_formulas_set(&_page->formulas, _latex, (u32)rde_strlen(_latex)) : NULL;
    if(_box == NULL) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_FORMULA_NOT_READ));
    } else {
        // As tall as the handwriting, where it was; its strokes gone.
        const f64 _size = (_ink.max_y - _ink.min_y) / rde_math_max_f64(_box->height + _box->depth, 0.5);
        f64 _w = 0.0, _h = 0.0;
        fude_zoom_formulas_size(_box, _size, &_w, &_h);
        const fude_zoom_v2 _tl = { _ink.min_x - FUDE_ZOOM_FORMULA_PAD * _size, _ink.max_y + FUDE_ZOOM_FORMULA_PAD * _size };
        // In the first stroke read's ink and layer (not a marker's the lasso held too).
        const u32* _read = (const u32*)_read_arr.memory;
        const u32  _nr   = (u32)rde_arr_length(&_read_arr);
        const fude_zoom_object* _first = fude_zoom_scene_object(_s, _read[0]);
        const u16 _was = _s->layer;
        _s->layer = _first->layer;
        const u32 _made = fude_zoom_scene_add_text(_s, _frame, (fude_zoom_place){ _tl, 0.0, 1.0 }, _size, _w, _h, FUDE_ZOOM_TEXT_FORMULA, _first->color,
                                                   (rde_color){ 0, 0, 0, 0 }, _latex, (u32)rde_strlen(_latex), 0);
        _s->layer = _was;
        fude_zoom_box _all = fude_zoom_scene_object(_s, _made)->box;
        // Only the strokes it read go: a marker's strokes, and any whose points could not be had, stay as they are.
        for(u32 _i = 0; _i < _nr; _i++) {
            fude_zoom_scene_set_alive(_s, _read[_i], false);
        }
        fude_zoom_history_push(_s, _frame, _all, _read, _nr, &_made, 1u);
        fude_zoom_select_clear(&_page->selection);
        rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _made });
        fude_zoom_select_update(&_page->selection, _s);
    }
    rde_arr_free(&_strokes);
    rde_arr_free(&_pts);
    rde_arr_free(&_ends);
    rde_arr_free(&_q);
    rde_arr_free(&_read_arr);
}

// See page_parts.h: formulas on the lasso's row (Formula, Handwriting, Graph) only where the product's Insert offers
// Formula (product.h).
b8 fude_zoom_formula_present(void) {
    return (fude_zoom_product_get()->inserts & (1u << FUDE_ZOOM_INSERT_FORMULA)) != 0u;
}

// The selection's Handwriting: each typed text written out where it was, as
// pen strokes in its colour, a capital as tall as its own, its lines as far
// apart; the text gone. One undo step.
void fude_zoom_page_handwrite_selection(fude_zoom_page* _page) {
    fude_zoom_scene*      _s    = &_page->scene;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _died = rde_arr_new(sizeof(u32), _heap), _born = rde_arr_new(sizeof(u32), _heap);
    rde_arr _pts  = rde_arr_new(sizeof(fude_zoom_v2), _heap), _ends = rde_arr_new(sizeof(u32), _heap), _path = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr _copy_arr = rde_arr_new(sizeof(c8), _heap);
    u32 _frame = FUDE_ZOOM_NONE;
    const u16 _was = _s->layer;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        const u32 _o = ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object;
        const fude_zoom_object _obj = *fude_zoom_scene_object(_s, _o);
        f64 _size, _w, _h;
        u8  _style;
        const c8* _text;
        u32 _len;
        if(!(_obj.flags & FUDE_ZOOM_FLAG_ALIVE) || _obj.kind != FUDE_ZOOM_KIND_TEXT || !fude_zoom_scene_text(_s, _o, &_size, &_w, &_h, &_style, &_text, &_len) ||
           _style != FUDE_ZOOM_TEXT_PLAIN || _len == 0) {
            continue;
        }
        rde_arr_resize(&_copy_arr, (usize)_len + 1u);
        c8* _copy = (c8*)_copy_arr.memory;
        rde_memcpy(_copy, _text, _len);
        _copy[_len] = 0;
        rde_arr_clear(&_pts);
        rde_arr_clear(&_ends);
        // A capital as tall as the font's (about seven tenths of its size), the first line's
        // baseline where the font's is; its own seed (the same text, written the same way).
        const f64 _cap = _size * 0.68;
        fude_zoom_hand_write(_copy, _cap, (u32)(_obj.id & 0xFFFFFFFFu), &_pts, &_ends);
        const f64 _c = rde_math_cos_f64(_obj.rotation), _sn = rde_math_sin_f64(_obj.rotation);
        const fude_zoom_v2* _p = (const fude_zoom_v2*)_pts.memory;
        const u32*          _e = (const u32*)_ends.memory;
        _s->layer = _obj.layer;
        for(u32 _k = 0, _from = 0; _k < (u32)rde_arr_length(&_ends); _from = _e[_k], _k++) {
            rde_arr_clear(&_path);
            for(u32 _j = _from; _j < _e[_k]; _j++) {
                const fude_zoom_v2 _l = { _p[_j].x * _obj.scale, (_p[_j].y - _size * 0.95) * _obj.scale };
                const fude_zoom_v2 _f = { _obj.t.x + _l.x * _c - _l.y * _sn, _obj.t.y + _l.x * _sn + _l.y * _c };
                rde_arr_add(&_path, (any)&_f);
            }
            fude_zoom_page_add_path(_page, _obj.frame, (const fude_zoom_v2*)_path.memory, (u32)rde_arr_length(&_path), false, _obj.color,
                                    (f32)(_size * 0.045 * _obj.scale), &_born);
        }
        fude_zoom_scene_set_alive(_s, _o, false);
        rde_arr_add(&_died, (any)&_o);
        _frame = _frame == FUDE_ZOOM_NONE ? _obj.frame : _frame;
    }
    _s->layer = _was;
    if(rde_arr_length(&_died) > 0) {
        fude_zoom_box _box = fude_zoom_scene_object(_s, ((const u32*)_died.memory)[0])->box;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_born); _i++) {
            _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, ((const u32*)_born.memory)[_i])->box);
        }
        fude_zoom_history_push(_s, _frame, _box, (const u32*)_died.memory, (u32)rde_arr_length(&_died), (const u32*)_born.memory, (u32)rde_arr_length(&_born));
        fude_zoom_select_clear(&_page->selection);
    }
    rde_arr_free(&_died);
    rde_arr_free(&_born);
    rde_arr_free(&_pts);
    rde_arr_free(&_ends);
    rde_arr_free(&_path);
    rde_arr_free(&_copy_arr);
}

// The numpad's OK on Offset: each selected thing's outline offset by the
// length typed (out; in when negative) drawn beside it, in its colour, on its
// layer — closed shapes, fills and closed lines by their outline, an open line
// by the band either side of it. One undo step.
void fude_zoom_page_offset_typed(any _self, const c8* _text, FUDE_ZOOM_UNIT_ _unit, f64 _mm) {
    RDE_UNUSED(_text); RDE_UNUSED(_unit);
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    fude_zoom_scene* _s = &_page->scene;
    if(!_page->open || !(rde_math_abs_f64(_mm) > 0.0)) {
        return;
    }
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _born = rde_arr_new(sizeof(u32), _heap), _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap), _q = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
    rde_arr _out  = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    const u16 _layer_was = _s->layer;
    fude_zoom_box _box = fude_zoom_box_empty();
    u32 _frame = _s->camera.frame;
    const u32 _n = (u32)rde_arr_length(&_page->selection.picks);
    for(u32 _i = 0; _i < _n; _i++) {
        const u32 _object = ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object;
        const fude_zoom_object _o = *fude_zoom_scene_object(_s, _object);
        b8 _closed = true;
        if(!fude_zoom_scene_object_outline(_s, _object, 96u, &_pts, &_closed)) {
            continue;   // (no outline of its own: scene.h)
        }
        const u32 _m = (u32)rde_arr_length(&_pts);
        if(_m < 2u) {
            continue;
        }
        // The length in its frame's units.
        const f64 _per = FUDE_ZOOM_PAGE_MM_PER_UNIT * fude_zoom_sim_scale(fude_zoom_scene_sim(_s, _o.frame, _home));
        const f64 _d   = _mm / _per;
        if(_closed && _m >= 3u) {
            fude_zoom_fill_offset((const fude_zoom_v2*)_pts.memory, _m, _d, &_out);
        } else if(_d > 0.0) {
            fude_zoom_fill_sweep((const fude_zoom_v2*)_pts.memory, _m, _d, &_out);
        } else {
            continue;
        }
        if(rde_arr_length(&_out) < 3u) {
            continue;
        }
        _s->layer = _o.layer;
        _s->style = _o.channels == FUDE_ZOOM_SHAPE_SYMBOL || _o.kind != FUDE_ZOOM_KIND_SHAPE ? 0 : _o.q;   // (its own line, as its layer)
        const f64 _k = fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, _o.frame));
        fude_zoom_page_add_path(_page, _o.frame, (const fude_zoom_v2*)_out.memory, (u32)rde_arr_length(&_out), true, _o.color, (f32)(0.6 / rde_math_max_f64(_k, 1e-300)), &_born);
        _frame = _o.frame;
    }
    _s->layer = _layer_was;
    _s->style = 0;
    const u32 _made = (u32)rde_arr_length(&_born);
    if(_made > 0 && fude_zoom_topic_now == FUDE_ZOOM_TOPIC_SEWING && _mm > 0.0) {
        // A seam allowance: the new outline the cutting line, what it was drawn round the stitching line (dashed).
        rde_arr _dead = rde_arr_new(sizeof(u32), _heap);
        fude_zoom_box _all = fude_zoom_box_empty();
        for(u32 _i = 0; _i < _made; _i++) {
            const fude_zoom_object* _b = fude_zoom_scene_object(_s, ((const u32*)_born.memory)[_i]);
            _all = fude_zoom_box_union(_all, fude_zoom_sim_box(fude_zoom_scene_sim(_s, _b->frame, _s->camera.frame), _b->box));
        }
        fude_zoom_page_relook_into(_page, FUDE_ZOOM_LINE_DASHED, -1, 0.0, &_dead, &_born, &_all);
        fude_zoom_history_push(_s, _s->camera.frame, _all, (const u32*)_dead.memory, (u32)rde_arr_length(&_dead), (const u32*)_born.memory, (u32)rde_arr_length(&_born));
        rde_arr_free(&_dead);
    } else if(_made > 0) {
        for(u32 _i = 0; _i < _made; _i++) {
            _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, ((const u32*)_born.memory)[_i])->box);
        }
        fude_zoom_history_push(_s, _frame, _box, NULL, 0, (const u32*)_born.memory, _made);
    } else {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_OFFSET_NOTHING));
    }
    rde_arr_free(&_born); rde_arr_free(&_pts); rde_arr_free(&_q); rde_arr_free(&_out);
}

// The board the lasso holds alone (FUDE_ZOOM_NONE: none, or more than it).
u32 fude_zoom_page_lone_board(const fude_zoom_page* _page) {
    if(rde_arr_length(&_page->selection.picks) != 1u) {
        return FUDE_ZOOM_NONE;
    }
    const u32 _o = ((const fude_zoom_pick*)_page->selection.picks.memory)[0].object;
    const fude_zoom_object* _b = _o < fude_zoom_scene_object_count(&_page->scene) ? fude_zoom_scene_object(&_page->scene, _o) : NULL;
    return _b != NULL && (_b->flags & FUDE_ZOOM_FLAG_ALIVE) && _b->kind == FUDE_ZOOM_KIND_SHAPE && _b->channels == FUDE_ZOOM_SHAPE_BOARD ? _o : FUDE_ZOOM_NONE;
}

// Its grain turned (along its length ↔ across), where it is, a cut one's outline kept: one undo step.
RDE_INTERNAL void fude_zoom_page_turn_grain(fude_zoom_page* _page, u32 _board) {
    fude_zoom_scene* _s = &_page->scene;
    rde_arr _n = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std());
    const u32 _count = fude_zoom_scene_shape_numbers_all(_s, _board, &_n);
    if(_count < 4u) {
        rde_arr_free(&_n);
        return;
    }
    f64* _num = (f64*)_n.memory;
    _num[3] = (f64)(fude_zoom_board_look(_num, _count) ^ 1u);   // (a board's fourth number: its grain, its material over it)
    const fude_zoom_object _old   = *fude_zoom_scene_object(_s, _board);
    const fude_zoom_place  _place = fude_zoom_scene_place_of(_s, _board);
    const u16 _layer_was = _s->layer;
    _s->layer = _old.layer;
    _s->style = _old.channels == FUDE_ZOOM_SHAPE_SYMBOL || _old.kind != FUDE_ZOOM_KIND_SHAPE ? 0 : _old.q;   // (its own line, as its layer)
    const u32 _new = fude_zoom_scene_add_shape_fill(_s, _old.frame, _place, FUDE_ZOOM_SHAPE_BOARD, _num, _count, _old.color, _old.radius,
                                                    (u8)(_old.flags & (FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN)), _old.fill, _old.z);
    _s->layer = _layer_was;
    _s->style = 0;
    rde_arr_free(&_n);
    fude_zoom_scene_set_alive(_s, _board, false);
    ((fude_zoom_pick*)_page->selection.picks.memory)[0].object = _new;
    rde_arr _dead = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std()), _made = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr_add(&_dead, (any)&_board);
    rde_arr_add(&_made, (any)&_new);
    fude_zoom_page_rebind(_page, &_board, &_new, 1u, &_dead, &_made);   // (connectors joined to it follow it)
    fude_zoom_history_push(_s, _old.frame, fude_zoom_box_union(_old.box, fude_zoom_scene_object(_s, _new)->box),
                           (const u32*)_dead.memory, (u32)rde_arr_length(&_dead), (const u32*)_made.memory, (u32)rde_arr_length(&_made));
    rde_arr_free(&_dead);
    rde_arr_free(&_made);
    fude_zoom_select_update(&_page->selection, _s);
}

// See page_parts.h. The length asked for starts at 3 mm, or at a 1 cm seam allowance in Sewing.
void fude_zoom_page_offset_selection(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->open || rde_arr_length(&_page->selection.picks) == 0) {
        return;
    }
    // A board alone: the button turns its grain instead (an outline offset from a board is not wanted).
    const u32 _board = fude_zoom_page_lone_board(_page);
    if(_board != FUDE_ZOOM_NONE) {
        fude_zoom_page_turn_grain(_page, _board);
        fude_ui_update(_page->app->ui);
        return;
    }
    // (a seam allowance: 1 cm)
    fude_zoom_numpad_open(&_page->numpad, fude_zoom_topic_now == FUDE_ZOOM_TOPIC_SEWING ? "10" : "3", FUDE_ZOOM_UNIT_MM, false, NULL, fude_zoom_page_offset_typed, _page);
}

// Appends to _out the strokes object _object is or holds: a stroke itself, and the strokes inside a frame, at most 64
// frames deep. What To curve and Formula look at; dead or untouchable objects are left out.
void fude_zoom_page_take_strokes(const fude_zoom_scene* _s, u32 _object, rde_arr* _out, u32 _depth) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || !fude_zoom_scene_touchable(_s, _o)) {
        return;
    }
    if(_o->kind == FUDE_ZOOM_KIND_STROKE) {
        rde_arr_add(_out, (any)&_object);
    } else if(_o->kind == FUDE_ZOOM_KIND_FRAME && _depth < 64u) {
        const fude_zoom_frame* _f = fude_zoom_scene_frame(_s, _o->child);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_f->order); _i++) {
            fude_zoom_page_take_strokes(_s, ((const u32*)_f->order.memory)[_i], _out, _depth + 1u);
        }
    }
}
