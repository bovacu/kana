// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"
#include "drawing/base/redraw.h"

// ===========================================================================
// Page, its own widgets (the extension's, ui.h): the line's chip, and the buttons and chips the page puts over the
// canvas, built, styled, updated and hit-tested each frame.
//
// The widgets belong to the UI's root, which frees them; the page keeps pointers to them, made by
// fude_zoom_page_ui_build and set to NULL by fude_zoom_page_ui_forget, so every use checks them. Each one shown or
// hidden has a shown_ flag beside it, and each text a _said copy, so the UI is touched only when something changes.
// The page's cards (choiceform.h, kanbanform.h and the rest) are built, styled, updated and forgotten from here too.
// fude_zoom_page_ui_build's and fude_zoom_page_ui_update's steps are above each of them, in the order it takes
// them. Prefix fude_zoom_page_.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// See page_parts.h. The chip shows one of four things, tried in this order: the size of what the lasso holds (the
// size card), a plain board (its card again; a cut board has no chip of its own), a sheet (its card), else a line's
// length, typed over on the numpad in the unit shown (up to 4 decimals, trailing zeros dropped).
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_line_chip(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->open) {
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    if(_page->size_chip) {
        fude_zoom_page_size_start(_page);   // What the lasso holds: its size, angle and place, to type.
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    if(_page->line_chip_object < fude_zoom_scene_object_count(&_page->scene) && fude_zoom_scene_object(&_page->scene, _page->line_chip_object)->channels == FUDE_ZOOM_SHAPE_BOARD &&
       fude_zoom_scene_object(&_page->scene, _page->line_chip_object)->kind == FUDE_ZOOM_KIND_SHAPE) {
        _page->line_chip_until = 0.0;
        if(!fude_zoom_page_board_cut(_page, _page->line_chip_object)) {
            fude_zoom_page_board_start(_page, _page->line_chip_object);   // A board's sizes and name, again (a cut one's would lose its cuts).
        }
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    if(fude_zoom_page_sheet_of(_page, _page->line_chip_object, NULL, NULL, NULL)) {
        _page->line_chip_until = 0.0;
        fude_zoom_page_sheet_start(_page, _page->line_chip_object);   // A sheet's card: its paper, scale and size.
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    f64 _dx, _dy;
    const f64 _mm = fude_zoom_page_line_mm(_page, _page->line_chip_object, &_dx, &_dy);
    // What it is now, in the unit shown, to type over.
    c8 _text[48];
    const f64 _per = fude_zoom_unit_mm(_page->units.unit);
    rde_snprintf(_text, sizeof(_text), "%.4f", _per > 0.0 ? _mm / _per : _mm);
    c8* _dot = rde_strchr(_text, '.');
    if(_dot != NULL) {
        c8* _end = _text + rde_strlen(_text) - 1;
        while(_end > _dot && *_end == '0') { *_end-- = 0; }
        if(_end == _dot) { *_end = 0; }
    }
    _page->line_chip_until = 0.0;
    fude_zoom_numpad_open(&_page->numpad, _text, _page->units.unit, false, NULL, fude_zoom_page_line_typed, _page);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Gives every one of the page's widgets that exists the active theme's colours and its corners, and restyles the
// cards. The offset pad's two lengths and the chips are outlined in the accent colour.
RDE_INTERNAL void fude_zoom_page_ui_style(fude_zoom_page* _page) {
    const fude_theme* _t = fude_theme_active();
    rde_ui_button* const _all[9] = { _page->back_button, _page->depth_button, _page->places_button, _page->find_button, _page->map_button, _page->pages_button,
                                     _page->home_button, _page->mic_button, _page->listen_button };
    for(u32 _i = 0; _i < 9u; _i++) {
        if(_all[_i] != NULL) {
            fude_kit_button_colors(_all[_i], _t->surface, 1.0f, _t->outline);
            fude_kit_button_round(_all[_i], 12.0f);
        }
    }
    for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_CRUMBS; _i++) {
        if(_page->crumbs[_i] != NULL) {
            fude_kit_button_colors(_page->crumbs[_i], _t->surface, 1.0f, _t->outline);
            fude_kit_button_round(_page->crumbs[_i], 10.0f);
        }
    }
    if(_page->anchor_grip != NULL) {
        fude_kit_button_colors(_page->anchor_grip, _t->surface, 1.0f, _t->outline);
        fude_kit_button_round(_page->anchor_grip, 12.0f);
    }
    for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_ANCHOR_BUTTONS; _i++) {
        if(_page->anchor_buttons[_i] != NULL) {
            fude_kit_button_colors(_page->anchor_buttons[_i], _t->surface, 1.0f, _i == 1u || _i == 4u ? _t->accent : _t->outline);
            fude_kit_button_round(_page->anchor_buttons[_i], 12.0f);
        }
    }
    if(_page->type_chip != NULL) {
        fude_kit_button_colors(_page->type_chip, _t->surface, 1.0f, _t->accent);
        fude_kit_button_round(_page->type_chip, 12.0f);
    }
    if(_page->kerf_chip != NULL) {
        fude_kit_button_colors(_page->kerf_chip, _t->surface, 1.0f, _t->accent);
        fude_kit_button_round(_page->kerf_chip, 12.0f);
    }
    if(_page->line_chip != NULL) {
        fude_kit_button_colors(_page->line_chip, _t->surface, 1.0f, _t->accent);
        fude_kit_button_round(_page->line_chip, 12.0f);
    }
    for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_LAYER_ROWS; _i++) {
        if(_page->layer_rows[_i] != NULL) {
            fude_kit_button_colors(_page->layer_rows[_i], _t->surface, 1.0f, _t->outline);
            fude_kit_button_round(_page->layer_rows[_i], 10.0f);
            fude_kit_button_colors(_page->layer_eyes[_i], _t->surface, 1.0f, _t->outline);
            fude_kit_button_round(_page->layer_eyes[_i], 10.0f);
            fude_kit_button_round(_page->layer_locks[_i], 10.0f);
            fude_kit_button_quiet(_page->layer_drops[_i]);
            fude_kit_button_round(_page->layer_drops[_i], 10.0f);
            fude_kit_button_quiet(_page->layer_ups[_i]);
            fude_kit_button_round(_page->layer_ups[_i], 10.0f);
            fude_kit_button_quiet(_page->layer_downs[_i]);
            fude_kit_button_round(_page->layer_downs[_i], 10.0f);
        }
    }
    if(_page->layer_add != NULL) {
        fude_kit_button_colors(_page->layer_add, _t->surface, 1.0f, _t->outline);
        fude_kit_button_round(_page->layer_add, 10.0f);
    }
    for(u32 _i = 0; _i < 6u; _i++) {
        if(_page->calibrate_keys[_i] != NULL) {
            if(_i == 5u) { fude_kit_button_primary(_page->calibrate_keys[_i]); }
            else         { fude_kit_restyle_button(_page->calibrate_keys[_i]); }
        }
    }
    fude_zoom_numpad_restyle(&_page->numpad);
    fude_zoom_parts_restyle(&_page->parts_form);
    fude_zoom_kanban_restyle(&_page->kanban_form);
    fude_zoom_sheet_restyle(&_page->sheet_form);
    fude_zoom_size_restyle(&_page->size_form);
    fude_zoom_point_restyle(&_page->point_form);
    fude_zoom_choice_restyle(&_page->choice_form);
    fude_zoom_value_restyle(&_page->value_form);
    fude_zoom_body_restyle(&_page->body_form);
    fude_zoom_limits_restyle(&_page->limits_form);
    fude_zoom_repeat_restyle(&_page->repeat_form);
    if(_page->text_field != NULL) {
        fude_kit_modal_restyle(&_page->text_card, 16.0f);
        fude_kit_style_field(_page->text_field);
        fude_kit_button_primary(_page->text_save);
        fude_kit_restyle_button(_page->text_cancel);
        for(u32 _i = 0; _i < FUDE_ZOOM_RICH_DOS; _i++) {
            fude_kit_restyle_button(_page->text_formats[_i]);
        }
        if(_page->inline_bar != NULL) {
            fude_kit_style_panel(_page->inline_bar, 12.0f, 1.0f);
            for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_INLINE_BUTTONS; _i++) {
                fude_kit_restyle_quiet(_page->inline_buttons[_i]);
            }
            fude_kit_button_primary(_page->inline_buttons[FUDE_ZOOM_PAGE_INLINE_BUTTONS - 1u]);   // Done.
        }
        fude_zoom_page_style_material_chips(_page);
    }
    for(u32 _i = 0; _i <= FUDE_ZOOM_PAGE_PLACES; _i++) {
        if(_page->place_rows[_i] != NULL) {
            fude_kit_button_colors(_page->place_rows[_i], _i == 0 ? _t->accent : _t->surface, 1.0f, _t->outline);
            fude_kit_button_round(_page->place_rows[_i], 10.0f);
        }
        if(_i < FUDE_ZOOM_PAGE_PLACES && _page->place_drops[_i] != NULL) {
            fude_kit_button_colors(_page->place_drops[_i], _t->surface, 1.0f, _t->outline);
            fude_kit_button_round(_page->place_drops[_i], 10.0f);
        }
    }
}

// The buttons over the canvas, made hidden (fude_zoom_page_ui_build's): Back, home, the depth and its levels,
// Find, the map, the microphone, the recordings, a notebook's page chip, the bookmarks and their rows (each
// with its ×), and the line's chip.
RDE_INTERNAL void fude_zoom_page_ui_build_buttons(fude_zoom_page* _page, rde_ui_node* _root) {
    _page->back_button = fude_kit_button(_root, fude_text(FUDE_TEXT_ZOOM_GO_BACK), fude_zoom_page_on_back, NULL);
    fude_kit_icon(_page->back_button, FUDE_ICON_BACK, FUDE_KIT_ICON_ONLY, 17.0f);
    _page->home_button = fude_kit_button(_root, fude_text(FUDE_TEXT_ZOOM_HOME), fude_zoom_page_on_home, NULL);
    fude_kit_icon(_page->home_button, FUDE_ICON_HOME, FUDE_KIT_ICON_ONLY, 18.0f);
    rde_ui_node_set_active(rde_ui_button_as_node(_page->home_button), false);
    _page->shown_home = false;
    _page->depth_button = fude_kit_button(_root, "\xC3\x97" "1", fude_zoom_page_on_depth, NULL);
    for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_CRUMBS; _i++) {
        _page->crumbs[_i] = fude_kit_button(_root, "", fude_zoom_page_on_crumb, (any)(uintptr_t)_i);
        rde_ui_node_set_active(rde_ui_button_as_node(_page->crumbs[_i]), false);
    }
    _page->find_button = fude_kit_button(_root, fude_text(FUDE_TEXT_ZOOM_FIND), fude_zoom_page_on_find, NULL);
    fude_kit_icon(_page->find_button, FUDE_ICON_SEARCH, FUDE_KIT_ICON_ONLY, 16.0f);
    rde_ui_node_set_active(rde_ui_button_as_node(_page->find_button), false);
    _page->shown_find = false;
    _page->map_button = fude_kit_button(_root, fude_text(FUDE_TEXT_ZOOM_MAP), fude_zoom_page_on_map, NULL);
    fude_kit_icon(_page->map_button, FUDE_ICON_MAP, FUDE_KIT_ICON_ONLY, 16.0f);
    rde_ui_node_set_active(rde_ui_button_as_node(_page->map_button), false);
    _page->shown_map = false;
    _page->mic_button = fude_kit_button(_root, fude_text(FUDE_TEXT_ZOOM_AUDIO_RECORD), fude_zoom_page_on_mic, NULL);
    fude_kit_icon(_page->mic_button, FUDE_ICON_MIC, FUDE_KIT_ICON_ONLY, 17.0f);
    rde_ui_node_set_active(rde_ui_button_as_node(_page->mic_button), false);
    _page->shown_mic   = false;
    _page->mic_said[0] = 0;
    _page->listen_button = fude_kit_button(_root, fude_text(FUDE_TEXT_ZOOM_AUDIO_RECORDINGS), fude_zoom_page_on_listen, NULL);
    fude_kit_icon(_page->listen_button, FUDE_ICON_WAVEFORM, FUDE_KIT_ICON_ONLY, 17.0f);
    rde_ui_node_set_active(rde_ui_button_as_node(_page->listen_button), false);
    _page->shown_listen = false;
    _page->pages_button = fude_kit_button(_root, "", fude_zoom_page_on_pages, NULL);   // A notebook's: "3 / 12".
    rde_ui_node_set_active(rde_ui_button_as_node(_page->pages_button), false);
    _page->shown_pages   = false;
    _page->pages_said[0] = 0;
    _page->places_button = fude_kit_button(_root, fude_text(FUDE_TEXT_ZOOM_PLACES), fude_zoom_page_on_places, NULL);
    fude_kit_icon(_page->places_button, FUDE_ICON_BOOKMARK, FUDE_KIT_ICON_ONLY, 16.0f);
    for(u32 _i = 0; _i <= FUDE_ZOOM_PAGE_PLACES; _i++) {
        _page->place_rows[_i] = fude_kit_button(_root, "", fude_zoom_page_on_place, (any)(uintptr_t)_i);
        rde_ui_node_set_active(rde_ui_button_as_node(_page->place_rows[_i]), false);
    }
    for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_PLACES; _i++) {
        _page->place_drops[_i] = fude_kit_button(_root, fude_text(FUDE_TEXT_ZOOM_PLACE_DROP), fude_zoom_page_on_place_drop, (any)(uintptr_t)_i);
        fude_kit_icon(_page->place_drops[_i], FUDE_ICON_CLOSE, FUDE_KIT_ICON_ONLY, 14.0f);
        rde_ui_node_set_active(rde_ui_button_as_node(_page->place_drops[_i]), false);
    }
    _page->line_chip = fude_kit_button(_root, "", fude_zoom_page_on_line_chip, NULL);
    rde_ui_node_set_active(rde_ui_button_as_node(_page->line_chip), false);
}

// The offset pad's buttons and its grip, the kerf's chip and Type a point's chip, made hidden
// (fude_zoom_page_ui_build's).
RDE_INTERNAL void fude_zoom_page_ui_build_pad(fude_zoom_page* _page, rde_ui_node* _root) {
    // The offset pad: ◀ X ▶  ▼ Y ▲  × (the two lengths have no icon: their text is set as they change), and its
    // grip.
    static const c8* const _icons[FUDE_ZOOM_PAGE_ANCHOR_BUTTONS] = { FUDE_ICON_PREV, NULL, FUDE_ICON_NEXT, FUDE_ICON_DOWN, NULL, FUDE_ICON_UP, FUDE_ICON_CLOSE };
    for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_ANCHOR_BUTTONS; _i++) {
        _page->anchor_buttons[_i] = fude_kit_button(_root, "", fude_zoom_page_on_anchor, (any)(uintptr_t)_i);
        if(_icons[_i] != NULL) {
            fude_kit_icon(_page->anchor_buttons[_i], _icons[_i], FUDE_KIT_ICON_ONLY, 16.0f);
        }
        rde_ui_node_set_active(rde_ui_button_as_node(_page->anchor_buttons[_i]), false);
    }
    _page->anchor_grip = fude_kit_button(_root, "", NULL, NULL);
    fude_kit_icon(_page->anchor_grip, FUDE_ICON_GRIP_V, FUDE_KIT_ICON_ONLY, 18.0f);
    rde_ui_node_set_active(rde_ui_button_as_node(_page->anchor_grip), false);
    _page->shown_anchor   = false;
    _page->anchor_said[0][0] = 0;
    _page->anchor_said[1][0] = 0;
    _page->kerf_chip = fude_kit_button(_root, "", fude_zoom_page_on_kerf, NULL);
    fude_kit_icon(_page->kerf_chip, FUDE_ICON_CUT, FUDE_KIT_ICON_LEFT, 16.0f);
    rde_ui_node_set_active(rde_ui_button_as_node(_page->kerf_chip), false);
    _page->kerf_chip_said[0] = 0;
    _page->type_chip = fude_kit_button(_root, fude_text(FUDE_TEXT_ZOOM_POINT_CHIP), fude_zoom_page_on_type_chip, NULL);
    fude_kit_icon(_page->type_chip, FUDE_ICON_KEYBOARD, FUDE_KIT_ICON_LEFT, 16.0f);
    rde_ui_node_set_active(rde_ui_button_as_node(_page->type_chip), false);
    _page->shown_type_chip = false;
}

// The text card (its field, Cancel, Save, a note's marks, a board's materials) and writing on the page (its
// hidden field and its bar of marks), made hidden (fude_zoom_page_ui_build's).
RDE_INTERNAL void fude_zoom_page_ui_build_text(fude_zoom_page* _page, rde_ui_node* _root) {
    // A note's marks' buttons, one a FUDE_ZOOM_RICH_DO_ (rich.h), on the card and on the bar of writing on the page: B,
    // I and H as letters, the lists and the table as icons, Link in words. The bar's then go on with Cut, Copy, Paste
    // and Done, icons too.
    static const c8* const _marks[FUDE_ZOOM_RICH_DOS] = { "B", "I", "H", NULL, NULL, NULL, NULL, NULL };
    static const c8* const _icons[FUDE_ZOOM_PAGE_INLINE_BUTTONS] = { NULL, NULL, NULL, FUDE_ICON_LISTS, FUDE_ICON_STROKE_ORDER, FUDE_ICON_LIST_CHECKS, FUDE_ICON_PAPER_SQUARES, NULL,
                                                                     FUDE_ICON_CUT, FUDE_ICON_COPY, FUDE_ICON_TEXT_PASTE, FUDE_ICON_CHECK };
    // The text card: a field over Cancel and Save, on a dimmed page.
    fude_kit_modal_create(&_page->text_card, _root, fude_zoom_page_on_text_dismiss, NULL);
    rde_ui_node* _card = rde_ui_image_as_node(_page->text_card.card);
    _page->text_field = rde_ui_text_editor_create(_page->app->ui->font, NULL);
    rde_ui_text_editor_set_multiline(_page->text_field, true);
    rde_ui_text_editor_set_font_size(_page->text_field, 16u);
    rde_ui_text_editor_set_max_chars(_page->text_field, 4000u);
    rde_ui_text_editor_set_content_insets(_page->text_field, 12.0f, 10.0f, 12.0f, 10.0f);
    fude_kit_field_box(_card, _page->text_field);
    _page->text_cancel = fude_kit_button(_card, fude_text(FUDE_TEXT_CANCEL), fude_zoom_page_on_text_cancel, NULL);
    _page->text_save   = fude_kit_button(_card, fude_text(FUDE_TEXT_SAVE), fude_zoom_page_on_text_save, NULL);
    _page->card_field  = _page->text_field;
    // Writing on the page: a field the keyboard types into, hidden (the page draws its words), and its bar of marks.
    _page->inline_field = rde_ui_text_editor_create(_page->app->ui->font, NULL);
    rde_ui_text_editor_set_multiline(_page->inline_field, true);
    rde_ui_text_editor_set_font_size(_page->inline_field, 16u);
    rde_ui_text_editor_set_max_chars(_page->inline_field, 4000u);
    rde_ui_text_editor_set_background(_page->inline_field, false, (rde_color){ 0, 0, 0, 0 });
    rde_ui_text_editor_set_text_color(_page->inline_field, (rde_color){ 0, 0, 0, 0 });
    rde_ui_text_editor_set_caret_color(_page->inline_field, (rde_color){ 0, 0, 0, 0 });
    rde_ui_text_editor_set_menu_enabled(_page->inline_field, false);
    rde_ui_node_add_child(_root, rde_ui_text_editor_as_node(_page->inline_field));
    fude_kit_place(rde_ui_text_editor_as_node(_page->inline_field), (rde_vec_2F){ -100.0f, -100.0f }, (rde_vec_2F){ 2.0f, 2.0f });
    _page->inline_bar = rde_ui_image_create(NULL);
    rde_ui_node_set_blocks_input(rde_ui_image_as_node(_page->inline_bar), true);
    rde_ui_node_set_gesture_exclusive(rde_ui_image_as_node(_page->inline_bar), true);
    rde_ui_node_add_child(_root, rde_ui_image_as_node(_page->inline_bar));
    // The bar's buttons: a note's marks first, their index a FUDE_ZOOM_RICH_DO_; then Cut, Copy, Paste and Done, their
    // index counted from the first of them.
    for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_INLINE_BUTTONS; _i++) {
        const b8 _mark = _i < FUDE_ZOOM_RICH_DOS;
        const c8* _label = _mark && _marks[_i] != NULL ? _marks[_i] : (_i == FUDE_ZOOM_RICH_DO_LINK ? fude_text(FUDE_TEXT_ZOOM_FORMAT_LINK) : "");
        _page->inline_buttons[_i] = fude_kit_button(rde_ui_image_as_node(_page->inline_bar), _label, _mark ? fude_zoom_page_on_text_format : fude_zoom_page_on_inline_tool,
                                                    (any)(uintptr_t)(_mark ? _i : _i - FUDE_ZOOM_RICH_DOS));
        if(_icons[_i] != NULL) {
            fude_kit_icon(_page->inline_buttons[_i], _icons[_i], FUDE_KIT_ICON_ONLY, 18.0f);
        }
    }
    rde_ui_node_set_active(rde_ui_image_as_node(_page->inline_bar), false);
    _page->inline_bar_shown = false;
    // ...and on the text card, a note's marks a tap each (made hidden: a note's card shows them).
    for(u32 _i = 0; _i < FUDE_ZOOM_RICH_DOS; _i++) {
        _page->text_formats[_i] = fude_kit_button(_card, _marks[_i] != NULL ? _marks[_i] : _icons[_i] != NULL ? "" : fude_text(FUDE_TEXT_ZOOM_FORMAT_LINK),
                                                  fude_zoom_page_on_text_format, (any)(uintptr_t)_i);
        if(_icons[_i] != NULL) {
            fude_kit_icon(_page->text_formats[_i], _icons[_i], FUDE_KIT_ICON_ONLY, 20.0f);
        }
        rde_ui_node_set_active(rde_ui_button_as_node(_page->text_formats[_i]), false);
    }
    // Naming a board: what it is made of, a chip each (shown only then).
    fude_zoom_page_material_words(_page->material_words, "");
    for(u32 _i = 0; _i < FUDE_ZOOM_MATERIAL_COUNT; _i++) {
        _page->material_chips[_i] = fude_kit_button(_card, _i == 0u ? fude_text(FUDE_TEXT_ZOOM_MATERIAL_NONE) : _page->material_words[_i], fude_zoom_page_on_material, (any)(uintptr_t)_i);
        rde_ui_node_set_active(rde_ui_button_as_node(_page->material_chips[_i]), false);
    }
}

// The calibrating keys and the layers' panel (a row each, its eye, lock, ×, up and down; New layer), made
// hidden (fude_zoom_page_ui_build's).
RDE_INTERNAL void fude_zoom_page_ui_build_panels(fude_zoom_page* _page, rde_ui_node* _root) {
    // Calibrating the true size: the card's outline smaller or bigger, Cancel, Save.
    static const c8* const _keys[6] = { "\xE2\x88\x92\xE2\x88\x92", "\xE2\x88\x92", "+", "++", NULL, NULL };
    for(u32 _i = 0; _i < 6u; _i++) {
        _page->calibrate_keys[_i] = fude_kit_button(_root, _keys[_i] != NULL ? _keys[_i] : fude_text(_i == 4u ? FUDE_TEXT_CANCEL : FUDE_TEXT_SAVE),
                                                    fude_zoom_page_on_calibrate_key, (any)(uintptr_t)_i);
        rde_ui_node_set_active(rde_ui_button_as_node(_page->calibrate_keys[_i]), false);
    }
    _page->shown_calibrate = false;
    // The layers' panel: a row each, its eye and its lock; New layer under them.
    for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_LAYER_ROWS; _i++) {
        _page->layer_rows[_i]  = fude_kit_button(_root, "", fude_zoom_page_on_layer, (any)(uintptr_t)_i);
        _page->layer_eyes[_i]  = fude_kit_button(_root, fude_text(FUDE_TEXT_ZOOM_LAYER_SHOW), fude_zoom_page_on_layer_eye, (any)(uintptr_t)_i);
        _page->layer_locks[_i] = fude_kit_button(_root, fude_text(FUDE_TEXT_ZOOM_LAYER_LOCK), fude_zoom_page_on_layer_lock, (any)(uintptr_t)_i);
        _page->layer_drops[_i] = fude_kit_button(_root, fude_text(FUDE_TEXT_ZOOM_LAYER_DROP), fude_zoom_page_on_layer_drop, (any)(uintptr_t)_i);
        _page->layer_ups[_i]   = fude_kit_button(_root, fude_text(FUDE_TEXT_ZOOM_LAYER_UP), fude_zoom_page_on_layer_up, (any)(uintptr_t)_i);
        _page->layer_downs[_i] = fude_kit_button(_root, fude_text(FUDE_TEXT_ZOOM_LAYER_DOWN), fude_zoom_page_on_layer_down, (any)(uintptr_t)_i);
        fude_kit_icon(_page->layer_ups[_i], FUDE_ICON_UP, FUDE_KIT_ICON_ONLY, 16.0f);
        fude_kit_icon(_page->layer_downs[_i], FUDE_ICON_DOWN, FUDE_KIT_ICON_ONLY, 16.0f);
        rde_ui_node_set_active(rde_ui_button_as_node(_page->layer_ups[_i]), false);
        rde_ui_node_set_active(rde_ui_button_as_node(_page->layer_downs[_i]), false);
        fude_kit_icon(_page->layer_eyes[_i], FUDE_ICON_SHOW, FUDE_KIT_ICON_ONLY, 17.0f);
        fude_kit_icon(_page->layer_locks[_i], FUDE_ICON_LOCK, FUDE_KIT_ICON_ONLY, 16.0f);
        fude_kit_icon(_page->layer_drops[_i], FUDE_ICON_TRASH, FUDE_KIT_ICON_ONLY, 16.0f);
        rde_ui_node_set_active(rde_ui_button_as_node(_page->layer_drops[_i]), false);
        rde_ui_node_set_active(rde_ui_button_as_node(_page->layer_rows[_i]), false);
        rde_ui_node_set_active(rde_ui_button_as_node(_page->layer_eyes[_i]), false);
        rde_ui_node_set_active(rde_ui_button_as_node(_page->layer_locks[_i]), false);
    }
    {
        c8 _with_icon[256];
        rde_snprintf(_with_icon, sizeof(_with_icon), FUDE_ICON_PLUS "  %s", fude_text(FUDE_TEXT_ZOOM_LAYER_ADD));
        _page->layer_add = fude_kit_button(_root, _with_icon, fude_zoom_page_on_layer_add, NULL);
        rde_ui_node_set_active(rde_ui_button_as_node(_page->layer_add), false);
    }
    _page->shown_layers = false;
}

// See page.h. Everything is made hidden, then placed and shown by the next fude_zoom_page_ui_update (laid_out is
// cleared so it places them).
// Its steps are above (fude_zoom_page_ui_build_*), in the order it takes them.
void fude_zoom_page_ui_build(fude_ui* _ui, rde_ui_node* _root) {
    RDE_UNUSED(_ui);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return;
    }
    fude_zoom_page_ui_build_buttons(_page, _root);
    fude_zoom_page_ui_build_pad(_page, _root);
    fude_zoom_page_ui_build_text(_page, _root);
    fude_zoom_page_ui_build_panels(_page, _root);
    fude_zoom_numpad_build(&_page->numpad, _root, _page->app->window);
    fude_zoom_parts_build(&_page->parts_form, _root, _page->app->window, _page->app->ui->font);
    fude_zoom_kanban_build(&_page->kanban_form, _root, _page->app->window, _page->app->ui->font);
    fude_zoom_sheet_build(&_page->sheet_form, _root, _page->app->window, _page->app->ui->font);
    fude_zoom_size_build(&_page->size_form, _root, _page->app->window, _page->app->ui->font);
    fude_zoom_point_build(&_page->point_form, _root, _page->app->window, _page->app->ui->font);
    fude_zoom_choice_build(&_page->choice_form, _root, _page->app->window, _page->app->ui->font);
    fude_zoom_value_build(&_page->value_form, _root, _page->app->window, _page->app->ui->font);
    fude_zoom_body_build(&_page->body_form, _root, _page->app->window, _page->app->ui->font);
    fude_zoom_limits_build(&_page->limits_form, _root, _page->app->window, _page->app->ui->font);
    fude_zoom_page_parts_load();   // My parts: their canvases' custom parts drawn and run as they are.
    fude_zoom_repeat_build(&_page->repeat_form, _root, _page->app->window, _page->app->ui->font);
    rde_ui_node_set_active(rde_ui_button_as_node(_page->back_button), false);
    rde_ui_node_set_active(rde_ui_button_as_node(_page->depth_button), false);
    rde_ui_node_set_active(rde_ui_button_as_node(_page->places_button), false);
    _page->shown_back = _page->shown_depth = _page->shown_crumbs = _page->shown_places = _page->shown_place_rows = false;
    _page->shown_line_chip = false;
    _page->line_chip_said[0] = 0;
    _page->depth_said[0] = 0;
    _page->laid_out      = (rde_vec_2F){ 0.0f, 0.0f };
    fude_zoom_page_ui_style(_page);
}

// See page.h: every pointer to a widget set to NULL, and each card forgotten. The text card's format buttons and the
// writing bar's buttons are left as they were; they are reached only through text_field and inline_bar, which are
// cleared.
void fude_zoom_page_ui_forget(fude_ui* _ui) {
    RDE_UNUSED(_ui);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return;
    }
    _page->back_button   = NULL;
    _page->home_button   = NULL;
    _page->depth_button  = NULL;
    _page->places_button = NULL;
    _page->find_button   = NULL;
    _page->map_button    = NULL;
    _page->mic_button    = NULL;
    _page->listen_button = NULL;
    _page->pages_button  = NULL;
    _page->line_chip     = NULL;
    _page->kerf_chip     = NULL;
    _page->type_chip     = NULL;
    rde_memset(_page->anchor_buttons, 0, sizeof(_page->anchor_buttons));
    _page->anchor_grip = NULL;
    rde_memset(_page->calibrate_keys, 0, sizeof(_page->calibrate_keys));
    rde_memset(_page->layer_rows, 0, sizeof(_page->layer_rows));
    rde_memset(_page->layer_eyes, 0, sizeof(_page->layer_eyes));
    rde_memset(_page->layer_locks, 0, sizeof(_page->layer_locks));
    rde_memset(_page->layer_drops, 0, sizeof(_page->layer_drops));
    rde_memset(_page->layer_ups, 0, sizeof(_page->layer_ups));
    rde_memset(_page->layer_downs, 0, sizeof(_page->layer_downs));
    _page->layer_add = NULL;
    _page->text_field    = NULL;
    _page->text_save     = NULL;
    _page->card_field    = NULL;
    _page->inline_field  = NULL;
    _page->inline_bar    = NULL;
    _page->inline_on     = false;
    _page->renderer.skip = FUDE_ZOOM_NONE;
    _page->text_cancel   = NULL;
    rde_memset(_page->material_chips, 0, sizeof(_page->material_chips));
    rde_memset(&_page->text_card, 0, sizeof(_page->text_card));
    fude_zoom_numpad_forget(&_page->numpad);
    fude_zoom_parts_forget(&_page->parts_form);
    fude_zoom_kanban_forget(&_page->kanban_form);
    fude_zoom_sheet_forget(&_page->sheet_form);
    fude_zoom_size_forget(&_page->size_form);
    fude_zoom_point_forget(&_page->point_form);
    fude_zoom_choice_forget(&_page->choice_form);
    fude_zoom_value_forget(&_page->value_form);
    fude_zoom_body_forget(&_page->body_form);
    fude_zoom_limits_forget(&_page->limits_form);
    fude_zoom_repeat_forget(&_page->repeat_form);
    rde_memset(_page->crumbs, 0, sizeof(_page->crumbs));
    rde_memset(_page->place_rows, 0, sizeof(_page->place_rows));
    rde_memset(_page->place_drops, 0, sizeof(_page->place_drops));
}

// See page.h.
void fude_zoom_page_ui_restyle(fude_ui* _ui) {
    RDE_UNUSED(_ui);
    if(fude_zoom_page_the != NULL) {
        fude_zoom_page_ui_style(fude_zoom_page_the);
    }
}

// Shows or hides _button when that changes *_shown, which it then updates. A NULL button does nothing.
RDE_INTERNAL void fude_zoom_page_ui_show(rde_ui_button* _button, b8 _show, b8* _shown) {
    if(_button != NULL && _show != *_shown) {
        *_shown = _show;
        rde_ui_node_set_active(rde_ui_button_as_node(_button), _show);
    }
}

// The buttons placed again when the screen, a notebook, the microphone's offer or the recordings changed, or
// recording began or ended (fude_zoom_page_ui_update's): Back beside the menu button (home before it, where there is
// a home screen); along the bottom left the bookmarks, Find, the map, the microphone, the recordings, a notebook's
// page chip and the depth; the bookmarks' rows and the depth's levels over them. Each centre is kept on the page,
// where the hit test and the next frames find it.
RDE_INTERNAL void fude_zoom_page_ui_lay_out(fude_zoom_page* _page, fude_ui* _ui, rde_vec_2F _screen, b8 _notebook, b8 _audio, b8 _listen, b8 _recording) {
    // (The microphone's chip is in the key: it is offered only once a canvas is open, often at the size the row was
    // first laid out at, and a chip shown but never placed kept the UI's default size, covering the window.)
    if(_screen.x != _page->laid_out.x || _screen.y != _page->laid_out.y || _notebook != _page->laid_pages || _audio != _page->laid_audio ||
       _listen != _page->laid_listen || _recording != _page->laid_recording) {
        _page->laid_out       = _screen;
        _page->laid_pages     = _notebook;
        _page->laid_audio     = _audio;
        _page->laid_listen    = _listen;
        _page->laid_recording = _recording;
        const rde_vec_4I _in = rde_window_get_safe_area_insets(_ui->window);   // Left, top, right, bottom.
        _page->back_center  = (rde_vec_2F){ (f32)_in.x + FUDE_ZOOM_PAGE_UI_MARGIN + FUDE_ZOOM_PAGE_UI_BACK_W * 1.5f + 8.0f,
                                            _screen.y - (f32)_in.y - FUDE_ZOOM_PAGE_UI_MARGIN * 0.5f - FUDE_ZOOM_PAGE_UI_BACK_H * 0.5f };
        // The home button, where there is a home screen, beside the side panel's: Back after it.
        _page->home_center  = _page->back_center;
        if(_page->on_home != NULL) {
            _page->back_center.x += FUDE_ZOOM_PAGE_UI_BACK_W + 8.0f;
        }
        if(_page->home_button != NULL) {
            fude_kit_place(rde_ui_button_as_node(_page->home_button), _page->home_center, (rde_vec_2F){ FUDE_ZOOM_PAGE_UI_BACK_W, FUDE_ZOOM_PAGE_UI_BACK_H });
        }
        _page->places_center = (rde_vec_2F){ (f32)_in.x + FUDE_ZOOM_PAGE_UI_MARGIN + FUDE_ZOOM_PAGE_UI_PLACES_W * 0.5f,
                                             (f32)_in.w + FUDE_ZOOM_PAGE_UI_MARGIN + FUDE_ZOOM_PAGE_UI_CHIP_H * 0.5f };
        // Bookmarks, Find beside them, the map, then the depth (shown only when not ×1: last, so no gap is left).
        _page->find_center  = (rde_vec_2F){ _page->places_center.x + FUDE_ZOOM_PAGE_UI_PLACES_W + FUDE_ZOOM_PAGE_UI_GAP, _page->places_center.y };
        _page->map_center   = (rde_vec_2F){ _page->find_center.x + FUDE_ZOOM_PAGE_UI_PLACES_W + FUDE_ZOOM_PAGE_UI_GAP, _page->places_center.y };
        // The microphone's after the map (wide while it records, its time in it), the recordings' after it, a
        // notebook's pages' chip after them, the depth last.
        f32 _after = _page->map_center.x + FUDE_ZOOM_PAGE_UI_PLACES_W * 0.5f + FUDE_ZOOM_PAGE_UI_GAP;
        const f32 _mic_w = _recording ? FUDE_ZOOM_PAGE_UI_CHIP_W : FUDE_ZOOM_PAGE_UI_PLACES_W;
        if(_audio) {
            _page->mic_center = (rde_vec_2F){ _after + _mic_w * 0.5f, _page->places_center.y };
            _after += _mic_w + FUDE_ZOOM_PAGE_UI_GAP;
            if(_page->mic_button != NULL) {
                fude_kit_place(rde_ui_button_as_node(_page->mic_button), _page->mic_center, (rde_vec_2F){ _mic_w, FUDE_ZOOM_PAGE_UI_CHIP_H });
            }
        }
        if(_listen) {
            _page->listen_center = (rde_vec_2F){ _after + FUDE_ZOOM_PAGE_UI_PLACES_W * 0.5f, _page->places_center.y };
            _after += FUDE_ZOOM_PAGE_UI_PLACES_W + FUDE_ZOOM_PAGE_UI_GAP;
            if(_page->listen_button != NULL) {
                fude_kit_place(rde_ui_button_as_node(_page->listen_button), _page->listen_center, (rde_vec_2F){ FUDE_ZOOM_PAGE_UI_PLACES_W, FUDE_ZOOM_PAGE_UI_CHIP_H });
            }
        }
        _page->pages_center = (rde_vec_2F){ _after + FUDE_ZOOM_PAGE_UI_CHIP_W * 0.5f, _page->places_center.y };
        _page->depth_center = (rde_vec_2F){ _notebook ? _page->pages_center.x + FUDE_ZOOM_PAGE_UI_CHIP_W + FUDE_ZOOM_PAGE_UI_GAP : _after + FUDE_ZOOM_PAGE_UI_CHIP_W * 0.5f,
                                            _page->places_center.y };
        if(_page->pages_button != NULL) {
            fude_kit_place(rde_ui_button_as_node(_page->pages_button), _page->pages_center, (rde_vec_2F){ FUDE_ZOOM_PAGE_UI_CHIP_W, FUDE_ZOOM_PAGE_UI_CHIP_H });
        }
        _page->crumb_size   = (rde_vec_2F){ FUDE_ZOOM_PAGE_UI_CHIP_W, FUDE_ZOOM_PAGE_UI_CHIP_H };
        _page->place_size   = (rde_vec_2F){ FUDE_ZOOM_PAGE_UI_ROW_W, FUDE_ZOOM_PAGE_UI_CHIP_H };
        fude_kit_place(rde_ui_button_as_node(_page->back_button), _page->back_center, (rde_vec_2F){ FUDE_ZOOM_PAGE_UI_BACK_W, FUDE_ZOOM_PAGE_UI_BACK_H });
        fude_kit_place(rde_ui_button_as_node(_page->depth_button), _page->depth_center, _page->crumb_size);
        fude_kit_place(rde_ui_button_as_node(_page->places_button), _page->places_center, (rde_vec_2F){ FUDE_ZOOM_PAGE_UI_PLACES_W, FUDE_ZOOM_PAGE_UI_CHIP_H });
        if(_page->find_button != NULL) {
            fude_kit_place(rde_ui_button_as_node(_page->find_button), _page->find_center, (rde_vec_2F){ FUDE_ZOOM_PAGE_UI_PLACES_W, FUDE_ZOOM_PAGE_UI_CHIP_H });
        }
        if(_page->map_button != NULL) {
            fude_kit_place(rde_ui_button_as_node(_page->map_button), _page->map_center, (rde_vec_2F){ FUDE_ZOOM_PAGE_UI_PLACES_W, FUDE_ZOOM_PAGE_UI_CHIP_H });
        }
        for(u32 _i = 0; _i <= FUDE_ZOOM_PAGE_PLACES; _i++) {
            const rde_vec_2F _at = { (f32)_in.x + FUDE_ZOOM_PAGE_UI_MARGIN + FUDE_ZOOM_PAGE_UI_ROW_W * 0.5f,
                                     _page->places_center.y + (f32)(_i + 1u) * (FUDE_ZOOM_PAGE_UI_CHIP_H + FUDE_ZOOM_PAGE_UI_GAP) };
            fude_kit_place(rde_ui_button_as_node(_page->place_rows[_i]), _at, _page->place_size);
            if(_i > 0) {   // A place's ×, just right of its row.
                fude_kit_place(rde_ui_button_as_node(_page->place_drops[_i - 1u]),
                               (rde_vec_2F){ _at.x + FUDE_ZOOM_PAGE_UI_ROW_W * 0.5f + FUDE_ZOOM_PAGE_UI_GAP + FUDE_ZOOM_PAGE_UI_DROP_W * 0.5f, _at.y },
                               (rde_vec_2F){ FUDE_ZOOM_PAGE_UI_DROP_W, FUDE_ZOOM_PAGE_UI_CHIP_H });
            }
        }
        for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_CRUMBS; _i++) {
            const rde_vec_2F _at = { _page->depth_center.x, _page->depth_center.y + (f32)(_i + 1u) * (FUDE_ZOOM_PAGE_UI_CHIP_H + FUDE_ZOOM_PAGE_UI_GAP) };
            fude_kit_place(rde_ui_button_as_node(_page->crumbs[_i]), _at, _page->crumb_size);
        }
    }
}

// Back, home, the depth and its levels, the bookmarks, Find, the map, the microphone and the recordings shown or
// hidden (fude_zoom_page_ui_update's; _here: the page is in sight). Returns whether a toolbar panel open over the
// bottom left corner covers them (all but Back and home make way for it).
RDE_INTERNAL b8 fude_zoom_page_ui_update_buttons(fude_zoom_page* _page, fude_ui* _ui, b8 _here, b8 _audio, b8 _listen) {
    fude_zoom_page_ui_show(_page->back_button, _here && _page->back_count > 0, &_page->shown_back);
    fude_zoom_page_ui_show(_page->home_button, _here && _page->on_home != NULL, &_page->shown_home);
    // The depth, once it is not the canvas's own (×1), with a canvas open (or always, once the true size is set).
    const f64 _depth = _page->open ? fude_zoom_nav_depth(&_page->scene) : 0.0;
    c8 _said[32];
    fude_zoom_nav_say(_depth, _said, sizeof _said);
    if(rde_strcmp(_said, _page->depth_said) != 0) {
        rde_snprintf(_page->depth_said, sizeof _page->depth_said, "%s", _said);
        rde_ui_button_set_text(_page->depth_button, _said);
    }
    // An open panel of the toolbar's over the bottom left corner (Instruments' choices, a long row): the
    // bookmark, Find and the depth make way for it (they would be over its first choices).
    const fude_toolbar* _tb = &_ui->bar;
    b8 _covered = false;
    if(_tb->tool_open >= 0) {
        const f32 _pl = _tb->tool_panel_center.x - _tb->tool_panel_size.x * 0.5f, _pb = _tb->tool_panel_center.y - _tb->tool_panel_size.y * 0.5f;
        const f32 _pt = _tb->tool_panel_center.y + _tb->tool_panel_size.y * 0.5f;
        const f32 _cr = _page->depth_center.x + FUDE_ZOOM_PAGE_UI_CHIP_W * 0.5f, _ct = _page->places_center.y + FUDE_ZOOM_PAGE_UI_CHIP_H * 0.5f;
        _covered = _pl < _cr && _pb < _ct && _pt > _page->places_center.y - FUDE_ZOOM_PAGE_UI_CHIP_H * 0.5f;
    }
    if(_covered) {
        _page->places_open = false;
        _page->crumbs_open = false;
    }
    fude_zoom_page_ui_show(_page->depth_button, _here && !_covered && (rde_math_abs_f64(_depth) > 0.3 || _page->true_mm_per_point > 0.0f), &_page->shown_depth);
    const b8 _crumbs = _here && _page->crumbs_open && _page->shown_depth;
    if(_crumbs != _page->shown_crumbs) {
        _page->shown_crumbs = _crumbs;
        for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_CRUMBS; _i++) {
            rde_ui_node_set_active(rde_ui_button_as_node(_page->crumbs[_i]), _crumbs && _i < _page->crumb_count);
        }
    }
    if(!_page->shown_depth) {
        _page->crumbs_open = false;
    }
    fude_zoom_page_ui_show(_page->places_button, _here && !_covered, &_page->shown_places);
    fude_zoom_page_ui_show(_page->find_button, _here && !_covered, &_page->shown_find);
    fude_zoom_page_ui_show(_page->map_button, _here && !_covered, &_page->shown_map);
    fude_zoom_page_ui_show(_page->mic_button, _here && !_covered && _audio, &_page->shown_mic);
    fude_zoom_page_ui_show(_page->listen_button, _here && !_covered && _listen, &_page->shown_listen);
    return _covered;
}

// The microphone's words and colours (fude_zoom_page_ui_update's): while recording, Stop and its time in red;
// else the microphone. Changed only when its words change.
RDE_INTERNAL void fude_zoom_page_ui_update_mic(fude_zoom_page* _page, b8 _audio, b8 _recording) {
    if(_audio && _page->mic_button != NULL) {
        // Recording: its time in it, in red; else the microphone.
        c8 _say[24];
        _say[0] = 0;
        if(_recording) {
            fude_zoom_page_audio_time((u64)((rde_engine_get_time_now() - _page->audio_started_at) * 1000.0), _say, sizeof(_say));
        }
        if(rde_strcmp(_say, _page->mic_said) != 0) {
            const b8 _was = _page->mic_said[0] != 0;
            rde_snprintf(_page->mic_said, sizeof(_page->mic_said), "%s", _say);
            const fude_theme* _t = fude_theme_active();
            if(_recording) {
                // Stop and its time, in red.
                rde_ui_button_set_text(_page->mic_button, _say);
                if(!_was) {
                    fude_kit_icon(_page->mic_button, FUDE_ICON_STOP, FUDE_KIT_ICON_LEFT, 15.0f);
                    if(_page->mic_button->internal_label != NULL) {
                        // Its time: the icon alone hid it.
                        rde_ui_node_set_active(rde_ui_label_as_node(_page->mic_button->internal_label), true);
                        rde_ui_label_set_font_scale(_page->mic_button->internal_label, 15.0f / (f32)FUDE_KIT_FONT_SIZE);
                    }
                    fude_kit_button_colors(_page->mic_button, _t->surface, 1.5f, _t->danger);
                    fude_kit_icon_color(_page->mic_button, _t->danger);
                    if(_page->mic_button->internal_label != NULL) {
                        rde_ui_label_set_color(_page->mic_button->internal_label, _t->danger);
                    }
                }
            } else {
                rde_ui_button_set_text(_page->mic_button, fude_text(FUDE_TEXT_ZOOM_AUDIO_RECORD));
                fude_kit_icon(_page->mic_button, FUDE_ICON_MIC, FUDE_KIT_ICON_ONLY, 17.0f);
                fude_kit_button_colors(_page->mic_button, _t->surface, 1.0f, _t->outline);
                fude_kit_icon_color(_page->mic_button, _t->text);
                if(_page->mic_button->internal_label != NULL) {
                    rde_ui_label_set_color(_page->mic_button->internal_label, _t->text);
                }
            }
        }
    }
}

// A notebook's page chip shown and its words kept up; the map, the strip and Find's list put away when the page
// is out of sight; and the bookmarks' rows shown or hidden, closed out of sight (fude_zoom_page_ui_update's).
RDE_INTERNAL void fude_zoom_page_ui_update_places(fude_zoom_page* _page, b8 _here, b8 _covered, b8 _notebook) {
    // A notebook's: the page the view is on, of how many.
    fude_zoom_page_ui_show(_page->pages_button, _here && !_covered && _notebook, &_page->shown_pages);
    if(_notebook && _page->pages_button != NULL) {
        c8 _say[24];
        rde_snprintf(_say, sizeof(_say), "%u / %u", fude_zoom_page_page_now(_page) + 1u, _page->pages.count);
        if(rde_strcmp(_say, _page->pages_said) != 0) {
            rde_snprintf(_page->pages_said, sizeof(_page->pages_said), "%s", _say);
            rde_ui_button_set_text(_page->pages_button, _say);
        }
    }
    if(!_here) {
        _page->map_open   = false;   // A screen up, the side panel, presenting: the map goes.
        _page->strip_open = false;
        _page->everywhere_shown = false;
    }
    const b8 _rows = _here && _page->places_open;
    if(_rows != _page->shown_place_rows) {
        _page->shown_place_rows = _rows;
        for(u32 _i = 0; _i <= FUDE_ZOOM_PAGE_PLACES; _i++) {
            rde_ui_node_set_active(rde_ui_button_as_node(_page->place_rows[_i]), _rows && _i <= _page->place_count);
            if(_i < FUDE_ZOOM_PAGE_PLACES) {
                rde_ui_node_set_active(rde_ui_button_as_node(_page->place_drops[_i]), _rows && _i < _page->place_count);
            }
        }
    }
    if(!_here) {
        _page->places_open = false;
    }
}

// The line's chip (fude_zoom_page_ui_update's), for a while after a line is drawn or the lasso takes one
// (line_chip_until): a sheet's size, a board's sizes and material, or a line's length (none for a cut board, nor
// for a line off the screen), its words set and the chip placed by it. Returns whether it shows.
RDE_INTERNAL b8 fude_zoom_page_ui_update_line_chip(fude_zoom_page* _page, rde_vec_2F _screen, b8 _here) {
    // A new line's length, by its end (canvas units: the screen's, from its bottom left), for a while after it is drawn
    // (line_chip_until). The chip's width is guessed from its bytes, 8 points each.
    // A cut board has none: its sizes typed again would make it a plain rectangle (fude_zoom_page_board_cut).
    if(_here && rde_engine_get_time_now() < _page->line_chip_until) {
        fude_redraw_after(_page->line_chip_until - rde_engine_get_time_now());   // (render on demand: it goes then)
    }
    b8 _chip = _here && rde_engine_get_time_now() < _page->line_chip_until && !fude_zoom_numpad_shown(&_page->numpad) && !_page->drawing &&
               _page->line_chip_object < fude_zoom_scene_object_count(&_page->scene) &&
               (fude_zoom_scene_object(&_page->scene, _page->line_chip_object)->flags & FUDE_ZOOM_FLAG_ALIVE) &&
               !fude_zoom_page_board_cut(_page, _page->line_chip_object);
    f64 _board_mm[3];
    u8  _board_look = 0u;
    fude_zoom_sheet _chip_sheet;
    if(_chip && fude_zoom_page_sheet_of(_page, _page->line_chip_object, &_board_mm[0], &_board_mm[1], &_chip_sheet)) {
        // A sheet's: its size (and its scale), by its top right corner on the screen.
        c8 _said[2u * FUDE_ZOOM_UNITS_TEXT + 48u];
        fude_zoom_page_sheet_said(_page, _board_mm[0], _board_mm[1], _chip_sheet.scale, _said, sizeof(_said));
        if(rde_strcmp(_said, _page->line_chip_said) != 0) {
            rde_snprintf(_page->line_chip_said, sizeof(_page->line_chip_said), "%s", _said);
            rde_ui_button_set_text(_page->line_chip, _said);
        }
        const fude_zoom_object* _o  = fude_zoom_scene_object(&_page->scene, _page->line_chip_object);
        const fude_zoom_box     _sb = fude_zoom_sim_box(fude_zoom_page_frame_sim(_page, _o->frame), _o->box);
        const f32 _w2 = 24.0f + 8.0f * (f32)rde_strlen(_said);
        const rde_vec_2F _c = { (f32)_sb.max_x + _screen.x * 0.5f - _w2 * 0.5f, (f32)_sb.max_y + _screen.y * 0.5f + 24.0f };
        _page->line_chip_center = (rde_vec_2F){ rde_math_min_f32(rde_math_max_f32(_c.x, _w2 * 0.5f + 8.0f), _screen.x - _w2 * 0.5f - 8.0f), rde_math_min_f32(rde_math_max_f32(_c.y, 30.0f), _screen.y - 30.0f) };
        fude_kit_place(rde_ui_button_as_node(_page->line_chip), _page->line_chip_center, (rde_vec_2F){ _w2, 34.0f });
    } else if(_chip && fude_zoom_page_board_of(_page, _page->line_chip_object, _board_mm, &_board_look, NULL, 0)) {
        // A board's: its three sizes (and what it is made of), by its top right corner on the screen.
        c8 _l[FUDE_ZOOM_UNITS_TEXT], _w[FUDE_ZOOM_UNITS_TEXT], _t[FUDE_ZOOM_UNITS_TEXT], _said[3u * FUDE_ZOOM_UNITS_TEXT + 96u];
        fude_zoom_units_format(_board_mm[0], &_page->units, _l, sizeof(_l));
        fude_zoom_units_format(_board_mm[1], &_page->units, _w, sizeof(_w));
        fude_zoom_units_format(_board_mm[2], &_page->units, _t, sizeof(_t));
        if((_board_look >> 1) != FUDE_ZOOM_MATERIAL_NONE) {
            rde_snprintf(_said, sizeof(_said), "%s \xC3\x97 %s \xC3\x97 %s \xC2\xB7 %s", _l, _w, _t, _page->material_words[_board_look >> 1]);
        } else {
            rde_snprintf(_said, sizeof(_said), "%s \xC3\x97 %s \xC3\x97 %s", _l, _w, _t);
        }
        if(rde_strcmp(_said, _page->line_chip_said) != 0) {
            rde_snprintf(_page->line_chip_said, sizeof(_page->line_chip_said), "%s", _said);
            rde_ui_button_set_text(_page->line_chip, _said);
        }
        const fude_zoom_object* _o  = fude_zoom_scene_object(&_page->scene, _page->line_chip_object);
        const fude_zoom_box     _sb = fude_zoom_sim_box(fude_zoom_page_frame_sim(_page, _o->frame), _o->box);
        const f32 _w2 = 24.0f + 8.0f * (f32)rde_strlen(_said);
        const rde_vec_2F _c = { (f32)_sb.max_x + _screen.x * 0.5f - _w2 * 0.5f, (f32)_sb.max_y + _screen.y * 0.5f + 24.0f };
        _page->line_chip_center = (rde_vec_2F){ rde_math_min_f32(rde_math_max_f32(_c.x, _w2 * 0.5f + 8.0f), _screen.x - _w2 * 0.5f - 8.0f), rde_math_min_f32(rde_math_max_f32(_c.y, 30.0f), _screen.y - 30.0f) };
        fude_kit_place(rde_ui_button_as_node(_page->line_chip), _page->line_chip_center, (rde_vec_2F){ _w2, 34.0f });
    } else if(_chip) {
        f64 _dx = 0.0, _dy = 0.0;
        const f64 _mm = fude_zoom_page_line_mm(_page, _page->line_chip_object, &_dx, &_dy);
        const fude_zoom_object* _o = fude_zoom_scene_object(&_page->scene, _page->line_chip_object);
        const fude_zoom_sim _all = fude_zoom_sim_compose(fude_zoom_page_frame_sim(_page, _o->frame), fude_zoom_object_sim(_o));
        const fude_zoom_v2  _a   = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 0.0 });
        const fude_zoom_v2  _b   = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _dx, _dy });
        const f64 _len = rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y);
        _chip = _mm > 0.0 && _len > 1.0 && rde_math_abs_f64(_b.x) < _screen.x * 0.5 && rde_math_abs_f64(_b.y) < _screen.y * 0.5;
        if(_chip) {
            c8 _said[FUDE_ZOOM_UNITS_TEXT];
            fude_zoom_units_format(_mm, &_page->units, _said, sizeof(_said));
            if(rde_strcmp(_said, _page->line_chip_said) != 0) {
                rde_snprintf(_page->line_chip_said, sizeof(_page->line_chip_said), "%s", _said);
                rde_ui_button_set_text(_page->line_chip, _said);
            }
            // Past the end, the way the line goes, a little to its left.
            const fude_zoom_v2 _d = { (_b.x - _a.x) / _len, (_b.y - _a.y) / _len };
            const f32 _w = 24.0f + 8.0f * (f32)rde_strlen(_said);
            const rde_vec_2F _c = { (f32)(_b.x + _d.x * 50.0 - _d.y * 22.0) + _screen.x * 0.5f, (f32)(_b.y + _d.y * 50.0 + _d.x * 22.0) + _screen.y * 0.5f };
            _page->line_chip_center = (rde_vec_2F){ rde_math_min_f32(rde_math_max_f32(_c.x, _w * 0.5f + 8.0f), _screen.x - _w * 0.5f - 8.0f), rde_math_min_f32(rde_math_max_f32(_c.y, 30.0f), _screen.y - 30.0f) };
            fude_kit_place(rde_ui_button_as_node(_page->line_chip), _page->line_chip_center, (rde_vec_2F){ _w, 34.0f });
        }
    }
    return _chip;
}

// The same chip saying the size of anything else the lasso holds, under its box's right corner
// (fude_zoom_page_ui_update's, after fude_zoom_page_ui_update_line_chip, whose answer is _chip). Returns whether
// the chip shows.
RDE_INTERNAL b8 fude_zoom_page_ui_update_size_chip(fude_zoom_page* _page, rde_vec_2F _screen, b8 _here, b8 _chip) {
    // Anything else the lasso holds: its size, under its box's right corner (tapped: the size card).
    _page->size_chip = false;
    if(_here && fude_zoom_select_any(&_page->selection) && !fude_zoom_select_busy(&_page->selection) && !_page->stretching &&
       !fude_zoom_numpad_shown(&_page->numpad) && !_page->drawing && !_page->presenting) {
        const u32 _n1 = (u32)rde_arr_length(&_page->selection.picks);
        const fude_zoom_object* _o1 = _n1 == 1u ? fude_zoom_scene_object(&_page->scene, ((const fude_zoom_pick*)_page->selection.picks.memory)[0].object) : NULL;
        // (a cut board alone has no chip of its own: this one, its size card scaling it with its cuts)
        const b8 _own_chip = _o1 != NULL && _o1->kind == FUDE_ZOOM_KIND_SHAPE &&
                             (_o1->channels == FUDE_ZOOM_SHAPE_LINE || _o1->channels == FUDE_ZOOM_SHAPE_DIMENSION ||
                              (_o1->channels == FUDE_ZOOM_SHAPE_BOARD && !fude_zoom_page_board_cut(_page, ((const fude_zoom_pick*)_page->selection.picks.memory)[0].object)) ||
                              _o1->channels == FUDE_ZOOM_SHAPE_SHEET);
        c8 _said[sizeof(_page->size_said)];
        fude_zoom_box _sb;
        if(!_own_chip && fude_zoom_select_box(&_page->selection, &_sb) && fude_zoom_page_size_said(_page, _said, sizeof(_said))) {
            if(rde_strcmp(_said, _page->line_chip_said) != 0) {
                rde_snprintf(_page->line_chip_said, sizeof(_page->line_chip_said), "%s", _said);
                rde_ui_button_set_text(_page->line_chip, _said);
            }
            const f32 _w = 24.0f + 8.0f * (f32)rde_strlen(_said);
            const rde_vec_2F _c = { (f32)_sb.max_x + _screen.x * 0.5f - _w * 0.5f, (f32)_sb.min_y + _screen.y * 0.5f - 26.0f };
            _page->line_chip_center = (rde_vec_2F){ rde_math_min_f32(rde_math_max_f32(_c.x, _w * 0.5f + 8.0f), _screen.x - _w * 0.5f - 8.0f), rde_math_min_f32(rde_math_max_f32(_c.y, 30.0f), _screen.y - 30.0f) };
            fude_kit_place(rde_ui_button_as_node(_page->line_chip), _page->line_chip_center, (rde_vec_2F){ _w, 34.0f });
            _page->size_chip = true;
            _chip = true;
        }
    }
    return _chip;
}

// The offset pad (fude_zoom_page_ui_update's): shown while an instrument snapped and let go (or slid by hand) is
// alive, its two lengths' words kept up, and placed again when they, the screen or the toolbar change: in the
// middle, clear of the toolbar, or where a hand left it (_kerf: the kerf's chip is under it).
RDE_INTERNAL void fude_zoom_page_ui_update_pad(fude_zoom_page* _page, fude_ui* _ui, rde_vec_2F _screen, b8 _here, b8 _kerf) {
    // An instrument snapped and let go: ◀ X ▶  ▼ Y ▲  ×, which move it exact lengths from the point it snapped to.
    // Slid by hand, its lengths read as it goes.
    const b8 _sliding = _page->anchor_sliding && _page->instruments.held == _page->anchor_tool;
    const b8 _anchor = _here && _page->anchor_buttons[0] != NULL && fude_zoom_page_anchor_alive(_page) &&
                       (_page->instruments.held < 0 || _sliding) && !fude_zoom_numpad_shown(&_page->numpad);
    if(_anchor) {
        c8 _said[2][64];
        fude_zoom_page_anchor_said(_page, 0u, _said[0], sizeof(_said[0]));
        fude_zoom_page_anchor_said(_page, 1u, _said[1], sizeof(_said[1]));
        f32 _y = _page->places_center.y + (_kerf ? FUDE_ZOOM_PAGE_UI_CHIP_H + FUDE_ZOOM_PAGE_UI_GAP : 0.0f);
        if(_page->anchor_custom) {
            // Where a hand left it.
            _y = rde_math_min_f32(rde_math_max_f32(_page->anchor_custom_at.y, FUDE_ZOOM_PAGE_UI_CHIP_H), _screen.y - FUDE_ZOOM_PAGE_UI_CHIP_H);
        }
        if(rde_strcmp(_said[0], _page->anchor_said[0]) != 0 || rde_strcmp(_said[1], _page->anchor_said[1]) != 0 || !_page->shown_anchor ||
           _y != _page->anchor_center.y || _screen.x != _page->anchor_screen_w || _ui->bar.center.x != _page->anchor_bar_x ||
           (_page->anchor_custom && _page->anchor_custom_at.x != _page->anchor_center.x)) {
            _page->anchor_screen_w = _screen.x;
            _page->anchor_bar_x    = _ui->bar.center.x;
            f32 _w[FUDE_ZOOM_PAGE_ANCHOR_BUTTONS];
            // Its grip first (dragged: the pad moved); the lengths as wide as their words, at least 88; a wider gap
            // before ▼ and before ×.
            f32 _all = FUDE_ZOOM_PAGE_ANCHOR_GRIP_W + FUDE_ZOOM_PAGE_UI_GAP;
            for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_ANCHOR_BUTTONS; _i++) {
                if(_i == 1u || _i == 4u) {
                    const c8* _text = _said[_i == 1u ? 0u : 1u];
                    rde_snprintf(_page->anchor_said[_i == 1u ? 0u : 1u], sizeof(_page->anchor_said[0]), "%s", _text);
                    rde_ui_button_set_text(_page->anchor_buttons[_i], _text);
                    _w[_i] = rde_math_max_f32(88.0f, 28.0f + fude_draw_text_width(fude_kit_font(), (f32)FUDE_KIT_FONT_SIZE, _text, FUDE_KIT_TEXT_SCALE * (f32)FUDE_KIT_FONT_SIZE));
                } else {
                    _w[_i] = FUDE_ZOOM_PAGE_UI_PLACES_W;
                }
                _all += _w[_i] + (_i > 0u ? (_i == 3u || _i == 6u ? 3.0f * FUDE_ZOOM_PAGE_UI_GAP : FUDE_ZOOM_PAGE_UI_GAP) : 0.0f);
            }
            // In the middle; clear of the bar (beside it) when it is down there too.
            f32 _cx = _screen.x * 0.5f;
            const fude_toolbar* _bar = &_ui->bar;
            const f32 _bl = _bar->center.x - _bar->panel_size.x * 0.5f, _br = _bar->center.x + _bar->panel_size.x * 0.5f;
            if(!_bar->hidden && rde_math_abs_f32(_y - _bar->center.y) <= (_bar->panel_size.y + FUDE_ZOOM_PAGE_UI_CHIP_H) * 0.5f &&
               _cx + _all * 0.5f > _bl - FUDE_ZOOM_PAGE_UI_GAP && _cx - _all * 0.5f < _br + FUDE_ZOOM_PAGE_UI_GAP) {
                const f32 _right = _br + 2.0f * FUDE_ZOOM_PAGE_UI_GAP + _all * 0.5f, _left = _bl - 2.0f * FUDE_ZOOM_PAGE_UI_GAP - _all * 0.5f;
                _cx = _right + _all * 0.5f <= _screen.x - FUDE_ZOOM_PAGE_UI_MARGIN ? _right : (_left - _all * 0.5f >= FUDE_ZOOM_PAGE_UI_MARGIN ? _left : _cx);
            }
            if(_page->anchor_custom) {
                _cx = rde_math_min_f32(rde_math_max_f32(_page->anchor_custom_at.x, _all * 0.5f + 4.0f), _screen.x - _all * 0.5f - 4.0f);
                _page->anchor_custom_at = (rde_vec_2F){ _cx, _y };
            }
            _page->anchor_center = (rde_vec_2F){ _cx, _y };
            _page->anchor_w      = _all;
            f32 _x = _cx - _all * 0.5f;
            _page->anchor_grip_at = (rde_vec_2F){ _x + FUDE_ZOOM_PAGE_ANCHOR_GRIP_W * 0.5f, _y };
            fude_kit_place(rde_ui_button_as_node(_page->anchor_grip), _page->anchor_grip_at, (rde_vec_2F){ FUDE_ZOOM_PAGE_ANCHOR_GRIP_W, FUDE_ZOOM_PAGE_UI_CHIP_H });
            _x += FUDE_ZOOM_PAGE_ANCHOR_GRIP_W + FUDE_ZOOM_PAGE_UI_GAP;
            for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_ANCHOR_BUTTONS; _i++) {
                _x += _i > 0u ? (_i == 3u || _i == 6u ? 3.0f * FUDE_ZOOM_PAGE_UI_GAP : FUDE_ZOOM_PAGE_UI_GAP) : 0.0f;
                fude_kit_place(rde_ui_button_as_node(_page->anchor_buttons[_i]), (rde_vec_2F){ _x + _w[_i] * 0.5f, _y }, (rde_vec_2F){ _w[_i], FUDE_ZOOM_PAGE_UI_CHIP_H });
                _x += _w[_i];
            }
        }
    }
    if(_anchor != _page->shown_anchor) {
        for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_ANCHOR_BUTTONS; _i++) {
            b8 _was = !_anchor;
            fude_zoom_page_ui_show(_page->anchor_buttons[_i], _anchor, &_was);
        }
        b8 _was = !_anchor;
        fude_zoom_page_ui_show(_page->anchor_grip, _anchor, &_was);
        _page->shown_anchor = _anchor;
    }
}

// The layers' panel (fude_zoom_page_ui_update's): its rows made again when the canvas changed, and placed again
// when it opens or closes or the toolbar moves, never under the toolbar; closed when the page is out of sight.
RDE_INTERNAL void fude_zoom_page_ui_update_layers(fude_zoom_page* _page, fude_ui* _ui, rde_vec_2F _screen, b8 _here) {
    // The layers' panel, top left under the menu: rows of a layer each, New layer under them.
    if(_here && _page->layers_open && _page->layers_seen != _page->scene.revision) {
        fude_zoom_page_show_layers(_page);
    }
    const b8 _layers = _here && _page->layers_open && _page->layer_add != NULL;
    const fude_toolbar* _bar = &_ui->bar;
    if(_layers && (_bar->center.x != _page->layers_bar_at.x || _bar->center.y != _page->layers_bar_at.y ||
                   _bar->panel_size.x != _page->layers_bar_size.x || _bar->panel_size.y != _page->layers_bar_size.y)) {
        _page->layers_dirty = true;   // The toolbar moved: the panel steps round it again.
    }
    if(_layers != _page->shown_layers || _page->layers_dirty) {
        _page->shown_layers = _layers;
        _page->layers_dirty = false;
        _page->layers_bar_at   = _bar->center;
        _page->layers_bar_size = _bar->panel_size;
        const rde_vec_4I _in = rde_window_get_safe_area_insets(_ui->window);
        f32 _left = (f32)_in.x + FUDE_ZOOM_PAGE_UI_MARGIN, _top = _screen.y - (f32)_in.y - 80.0f;
        // Never under the toolbar: beside it when it stands, under it when it lies.
        const f32 _pw = FUDE_ZOOM_PAGE_LAYERS_W, _ph = (f32)(_page->layer_count + 1u) * (FUDE_ZOOM_PAGE_UI_CHIP_H + FUDE_ZOOM_PAGE_UI_GAP);
        const f32 _bl = _bar->center.x - _bar->panel_size.x * 0.5f, _br = _bar->center.x + _bar->panel_size.x * 0.5f;
        const f32 _bb = _bar->center.y - _bar->panel_size.y * 0.5f, _bt = _bar->center.y + _bar->panel_size.y * 0.5f;
        if(_br > _left && _bl < _left + _pw && _bt > _top - _ph && _bb < _top) {
            if(_bar->vertical) {
                _left = _br + FUDE_ZOOM_PAGE_UI_GAP;
            } else {
                _top = _bb - FUDE_ZOOM_PAGE_UI_GAP;
            }
        }
        for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_LAYER_ROWS; _i++) {
            const b8  _show = _layers && _i < _page->layer_count;
            const f32 _y    = _top - (f32)_i * (FUDE_ZOOM_PAGE_UI_CHIP_H + FUDE_ZOOM_PAGE_UI_GAP) - FUDE_ZOOM_PAGE_UI_CHIP_H * 0.5f;
            rde_ui_node_set_active(rde_ui_button_as_node(_page->layer_rows[_i]), _show);
            rde_ui_node_set_active(rde_ui_button_as_node(_page->layer_eyes[_i]), _show);
            rde_ui_node_set_active(rde_ui_button_as_node(_page->layer_locks[_i]), _show);
            rde_ui_node_set_active(rde_ui_button_as_node(_page->layer_drops[_i]), _show);
            rde_ui_node_set_active(rde_ui_button_as_node(_page->layer_ups[_i]), _show);
            rde_ui_node_set_active(rde_ui_button_as_node(_page->layer_downs[_i]), _show);
            fude_kit_place(rde_ui_button_as_node(_page->layer_ups[_i]), (rde_vec_2F){ _left + 200.0f + 3.0f * FUDE_ZOOM_PAGE_UI_GAP + 106.0f, _y }, (rde_vec_2F){ 36.0f, FUDE_ZOOM_PAGE_UI_CHIP_H });
            fude_kit_place(rde_ui_button_as_node(_page->layer_downs[_i]), (rde_vec_2F){ _left + 200.0f + 4.0f * FUDE_ZOOM_PAGE_UI_GAP + 142.0f, _y }, (rde_vec_2F){ 36.0f, FUDE_ZOOM_PAGE_UI_CHIP_H });
            fude_kit_place(rde_ui_button_as_node(_page->layer_drops[_i]), (rde_vec_2F){ _left + 200.0f + 5.0f * FUDE_ZOOM_PAGE_UI_GAP + 182.0f, _y }, (rde_vec_2F){ 44.0f, FUDE_ZOOM_PAGE_UI_CHIP_H });
            fude_kit_place(rde_ui_button_as_node(_page->layer_rows[_i]), (rde_vec_2F){ _left + 100.0f, _y }, (rde_vec_2F){ 200.0f, FUDE_ZOOM_PAGE_UI_CHIP_H });
            fude_kit_place(rde_ui_button_as_node(_page->layer_eyes[_i]), (rde_vec_2F){ _left + 200.0f + FUDE_ZOOM_PAGE_UI_GAP + 22.0f, _y }, (rde_vec_2F){ 44.0f, FUDE_ZOOM_PAGE_UI_CHIP_H });
            fude_kit_place(rde_ui_button_as_node(_page->layer_locks[_i]), (rde_vec_2F){ _left + 200.0f + 2.0f * FUDE_ZOOM_PAGE_UI_GAP + 66.0f, _y }, (rde_vec_2F){ 44.0f, FUDE_ZOOM_PAGE_UI_CHIP_H });
        }
        _page->layers_left = _left;
        _page->layers_top  = _top;
        rde_ui_node_set_active(rde_ui_button_as_node(_page->layer_add), _layers);
        fude_kit_place(rde_ui_button_as_node(_page->layer_add), (rde_vec_2F){ _left + 100.0f, _top - (f32)_page->layer_count * (FUDE_ZOOM_PAGE_UI_CHIP_H + FUDE_ZOOM_PAGE_UI_GAP) - FUDE_ZOOM_PAGE_UI_CHIP_H * 0.5f },
                       (rde_vec_2F){ 200.0f, FUDE_ZOOM_PAGE_UI_CHIP_H });
    }
    if(!_here) {
        _page->layers_open = false;
    }
}

// See page.h. In order: the buttons placed again when the screen, a notebook, the recordings or recording change; Back,
// home, the depth and its levels, Places, Find, the map, the microphone and the recordings shown or hidden (all but
// Back and home make way for a toolbar panel over them); a notebook's page chip; the line's chip (a sheet's, a
// board's, a line's length, or the size of what the lasso holds); the kerf's and Type a point's chips; the offset
// pad; the cards; the layers' panel; and the calibrating keys. _full is true while a screen is up over the page,
// which hides them.
// The longer of those are steps above (fude_zoom_page_ui_lay_out, fude_zoom_page_ui_update_*), in that order.
void fude_zoom_page_ui_update(fude_ui* _ui, b8 _full) {
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || _page->back_button == NULL) {
        return;
    }
    // Placed again when the screen, a notebook, the microphone, the recordings or recording change
    // (fude_zoom_page_ui_lay_out): Back beside the menu button, the depth at the bottom left, its levels over it.
    const rde_vec_2F _screen = fude_kit_screen_size(_ui->window);
    const b8 _notebook = _page->open && _page->pages.on;
    // A page's layout (Cornell, To-do) chosen on a notebook, the canvas made endless since: its paper what it shows
    // there, ruled lines (the Paper button too).
    if(_page->open && !fude_zoom_page_paged(_page) && fude_zoom_paper_is_page((u8)_page->app->canvas->page.paper)) {
        _page->app->canvas->page.paper = FUDE_PAPER_LINES;
    }
    const b8 _audio     = fude_zoom_page_audio_offered() && _page->open;
    const b8 _listen    = _audio && rde_arr_length(&_page->audio.recordings) > 0u;
    const b8 _recording = _page->audio_recording != 0u;
    fude_zoom_page_ui_lay_out(_page, _ui, _screen, _notebook, _audio, _listen, _recording);
    const b8 _here = !_full && _page->open && !_ui->side.open;   // The sidebar open: out of its way.
    const b8 _covered = fude_zoom_page_ui_update_buttons(_page, _ui, _here, _audio, _listen);
    fude_zoom_page_ui_update_mic(_page, _audio, _recording);
    fude_zoom_page_ui_update_places(_page, _here, _covered, _notebook);
    b8 _chip = fude_zoom_page_ui_update_line_chip(_page, _screen, _here);
    _chip = fude_zoom_page_ui_update_size_chip(_page, _screen, _here, _chip);
    fude_zoom_page_ui_show(_page->line_chip, _chip, &_page->shown_line_chip);
    // The Saw on: its kerf, bottom middle (tapped: typed). Placed only when its words change or it comes up.
    const b8 _kerf = _here && _page->saw && _page->kerf_chip != NULL && !fude_zoom_numpad_shown(&_page->numpad);
    if(_kerf) {
        c8 _said[96];
        fude_zoom_page_kerf_said(_page, _said, sizeof(_said));
        if(rde_strcmp(_said, _page->kerf_chip_said) != 0 || !_page->shown_kerf_chip) {
            rde_snprintf(_page->kerf_chip_said, sizeof(_page->kerf_chip_said), "%s", _said);
            rde_ui_button_set_text(_page->kerf_chip, _said);
            const f32 _w = 56.0f + 8.0f * (f32)rde_strlen(_said);
            _page->kerf_chip_center = (rde_vec_2F){ _screen.x * 0.5f, _page->places_center.y };
            fude_kit_place(rde_ui_button_as_node(_page->kerf_chip), _page->kerf_chip_center, (rde_vec_2F){ _w, FUDE_ZOOM_PAGE_UI_CHIP_H });
        }
    }
    fude_zoom_page_ui_show(_page->kerf_chip, _kerf, &_page->shown_kerf_chip);
    // The Curve tool on: Type a point, bottom middle (over the kerf's, if that shows too).
    const b8 _type = _here && _page->curve_tool && _page->type_chip != NULL && !fude_zoom_numpad_shown(&_page->numpad) && !fude_zoom_point_shown(&_page->point_form);
    if(_type) {
        const f32 _w = 56.0f + 8.0f * (f32)rde_strlen(fude_text(FUDE_TEXT_ZOOM_POINT_CHIP));
        const b8 _row = fude_zoom_select_any(&_page->selection);   // The selection's row along the bottom: over it.
        _page->type_chip_center = (rde_vec_2F){ _screen.x * 0.5f, _page->places_center.y + (_kerf ? FUDE_ZOOM_PAGE_UI_CHIP_H + FUDE_ZOOM_PAGE_UI_GAP : 0.0f) +
                                                                  (_row ? 76.0f : 0.0f) };
        fude_kit_place(rde_ui_button_as_node(_page->type_chip), _page->type_chip_center, (rde_vec_2F){ _w, FUDE_ZOOM_PAGE_UI_CHIP_H });
    }
    fude_zoom_page_ui_show(_page->type_chip, _type, &_page->shown_type_chip);
    fude_zoom_page_ui_update_pad(_page, _ui, _screen, _here, _kerf);
    fude_zoom_numpad_update(&_page->numpad);
    fude_zoom_parts_update(&_page->parts_form);
    fude_zoom_kanban_update(&_page->kanban_form);
    fude_zoom_sheet_update(&_page->sheet_form);
    fude_zoom_size_update(&_page->size_form);
    fude_zoom_point_update(&_page->point_form);
    fude_zoom_choice_update(&_page->choice_form);
    fude_zoom_value_update(&_page->value_form);
    fude_zoom_body_update(&_page->body_form);
    fude_zoom_limits_update(&_page->limits_form);
    fude_zoom_repeat_update(&_page->repeat_form);
    fude_zoom_page_ui_update_layers(_page, _ui, _screen, _here);
    // Calibrating: its keys in a row under the card's outline.
    const b8 _cal = _page->calibrating && !_full;
    if(_cal != _page->shown_calibrate && _page->calibrate_keys[0] != NULL) {
        _page->shown_calibrate = _cal;
        for(u32 _i = 0; _i < 6u; _i++) {
            rde_ui_node_set_active(rde_ui_button_as_node(_page->calibrate_keys[_i]), _cal);
            const f32 _w = _i < 4u ? 64.0f : 120.0f;
            const f32 _x = _i < 4u ? _screen.x * 0.5f + ((f32)_i - 1.5f) * 72.0f : _screen.x * 0.5f + (_i == 4u ? -66.0f : 66.0f);
            const f32 _y = _screen.y * 0.5f - 40.0f - (_i < 4u ? 120.0f : 180.0f);
            fude_kit_place(rde_ui_button_as_node(_page->calibrate_keys[_i]), (rde_vec_2F){ _x, _y }, (rde_vec_2F){ _w, 46.0f });
        }
    }
}

// See page.h. _canvas is the point in the UI's own units (origin at the bottom left), as the widgets were placed; each
// widget shown is tested by the box it was placed in (a chip's width guessed from its words, as it was placed).
b8 fude_zoom_page_ui_hit(const fude_ui* _ui, rde_vec_2F _screen, rde_vec_2F _canvas) {
    RDE_UNUSED(_ui); RDE_UNUSED(_screen);
    const fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return false;
    }
    const f32 _bw = FUDE_ZOOM_PAGE_UI_BACK_W * 0.5f, _bh = FUDE_ZOOM_PAGE_UI_BACK_H * 0.5f;
    if(fude_zoom_numpad_shown(&_page->numpad) || fude_zoom_parts_shown(&_page->parts_form) || fude_zoom_kanban_shown(&_page->kanban_form) || fude_zoom_sheet_shown(&_page->sheet_form) || fude_zoom_size_shown(&_page->size_form) || fude_zoom_repeat_shown(&_page->repeat_form) || fude_zoom_point_shown(&_page->point_form) || fude_zoom_choice_shown(&_page->choice_form) || fude_zoom_value_shown(&_page->value_form) || fude_zoom_body_shown(&_page->body_form) || fude_zoom_limits_shown(&_page->limits_form) || (_page->text_field != NULL && _page->text_card.shown) || _page->calibrating) {
        return true;   // A card over the whole screen (a tap off it puts it away).
    }
    if(_page->inline_on && _page->inline_bar_shown && rde_math_abs_f32(_canvas.x - _page->inline_bar_at.x) <= _page->inline_bar_size.x * 0.5f &&
       rde_math_abs_f32(_canvas.y - _page->inline_bar_at.y) <= _page->inline_bar_size.y * 0.5f) {
        return true;   // The bar over a text being written: its buttons', not a touch off the text.
    }
    if(_page->shown_layers && _page->layer_add != NULL) {
        // The panel's column: its rows, their eyes and locks, New layer.
        const f32 _left = _page->layers_left, _right = _left + FUDE_ZOOM_PAGE_LAYERS_W;
        const f32 _top  = _page->layers_top;
        const f32 _bottom = _top - (f32)(_page->layer_count + 1u) * (FUDE_ZOOM_PAGE_UI_CHIP_H + FUDE_ZOOM_PAGE_UI_GAP);
        if(_canvas.x >= _left && _canvas.x <= _right && _canvas.y <= _top && _canvas.y >= _bottom) {
            return true;
        }
    }
    if(_page->shown_anchor && rde_math_abs_f32(_canvas.x - _page->anchor_center.x) <= _page->anchor_w * 0.5f &&
       rde_math_abs_f32(_canvas.y - _page->anchor_center.y) <= FUDE_ZOOM_PAGE_UI_CHIP_H * 0.5f) {
        return true;
    }
    if(_page->shown_kerf_chip && rde_math_abs_f32(_canvas.x - _page->kerf_chip_center.x) <= 28.0f + 4.0f * (f32)rde_strlen(_page->kerf_chip_said) &&
       rde_math_abs_f32(_canvas.y - _page->kerf_chip_center.y) <= FUDE_ZOOM_PAGE_UI_CHIP_H * 0.5f) {
        return true;
    }
    if(_page->shown_type_chip && rde_math_abs_f32(_canvas.x - _page->type_chip_center.x) <= 28.0f + 4.0f * (f32)rde_strlen(fude_text(FUDE_TEXT_ZOOM_POINT_CHIP)) &&
       rde_math_abs_f32(_canvas.y - _page->type_chip_center.y) <= FUDE_ZOOM_PAGE_UI_CHIP_H * 0.5f) {
        return true;
    }
    if(_page->shown_line_chip && rde_math_abs_f32(_canvas.x - _page->line_chip_center.x) <= 24.0f + 4.0f * (f32)rde_strlen(_page->line_chip_said) &&
       rde_math_abs_f32(_canvas.y - _page->line_chip_center.y) <= 17.0f) {
        return true;
    }
    if(_page->shown_home && rde_math_abs_f32(_canvas.x - _page->home_center.x) <= _bw && rde_math_abs_f32(_canvas.y - _page->home_center.y) <= _bh) {
        return true;
    }
    if(_page->shown_back && rde_math_abs_f32(_canvas.x - _page->back_center.x) <= _bw && rde_math_abs_f32(_canvas.y - _page->back_center.y) <= _bh) {
        return true;
    }
    const f32 _cw = _page->crumb_size.x * 0.5f, _ch = _page->crumb_size.y * 0.5f;
    if(_page->shown_depth && rde_math_abs_f32(_canvas.x - _page->depth_center.x) <= _cw && rde_math_abs_f32(_canvas.y - _page->depth_center.y) <= _ch) {
        return true;
    }
    if(_page->shown_crumbs) {
        const f32 _top = _page->depth_center.y + (f32)_page->crumb_count * (FUDE_ZOOM_PAGE_UI_CHIP_H + FUDE_ZOOM_PAGE_UI_GAP) + _ch;
        if(rde_math_abs_f32(_canvas.x - _page->depth_center.x) <= _cw && _canvas.y >= _page->depth_center.y && _canvas.y <= _top) {
            return true;
        }
    }
    if(_page->shown_find && rde_math_abs_f32(_canvas.x - _page->find_center.x) <= FUDE_ZOOM_PAGE_UI_PLACES_W * 0.5f && rde_math_abs_f32(_canvas.y - _page->find_center.y) <= _ch) {
        return true;
    }
    if(_page->shown_mic && rde_math_abs_f32(_canvas.x - _page->mic_center.x) <= (_page->audio_recording != 0u ? FUDE_ZOOM_PAGE_UI_CHIP_W : FUDE_ZOOM_PAGE_UI_PLACES_W) * 0.5f &&
       rde_math_abs_f32(_canvas.y - _page->mic_center.y) <= _ch) {
        return true;
    }
    if(_page->shown_listen && rde_math_abs_f32(_canvas.x - _page->listen_center.x) <= FUDE_ZOOM_PAGE_UI_PLACES_W * 0.5f && rde_math_abs_f32(_canvas.y - _page->listen_center.y) <= _ch) {
        return true;
    }
    // A notebook's page chip ("3 / 12"), placed as a chip is (FUDE_ZOOM_PAGE_UI_CHIP_W by _CHIP_H).
    if(_page->shown_pages && rde_math_abs_f32(_canvas.x - _page->pages_center.x) <= FUDE_ZOOM_PAGE_UI_CHIP_W * 0.5f &&
       rde_math_abs_f32(_canvas.y - _page->pages_center.y) <= FUDE_ZOOM_PAGE_UI_CHIP_H * 0.5f) {
        return true;
    }
    if(_page->shown_map && rde_math_abs_f32(_canvas.x - _page->map_center.x) <= FUDE_ZOOM_PAGE_UI_PLACES_W * 0.5f && rde_math_abs_f32(_canvas.y - _page->map_center.y) <= _ch) {
        return true;
    }
    if(_page->shown_places && rde_math_abs_f32(_canvas.x - _page->places_center.x) <= FUDE_ZOOM_PAGE_UI_PLACES_W * 0.5f && rde_math_abs_f32(_canvas.y - _page->places_center.y) <= _ch) {
        return true;
    }
    if(_page->shown_place_rows) {
        const f32 _left = _page->places_center.x - FUDE_ZOOM_PAGE_UI_PLACES_W * 0.5f;
        const f32 _top  = _page->places_center.y + (f32)(_page->place_count + 1u) * (FUDE_ZOOM_PAGE_UI_CHIP_H + FUDE_ZOOM_PAGE_UI_GAP) + _ch;
        if(_canvas.x >= _left && _canvas.x <= _left + FUDE_ZOOM_PAGE_UI_ROW_W + FUDE_ZOOM_PAGE_UI_GAP + FUDE_ZOOM_PAGE_UI_DROP_W && _canvas.y >= _page->places_center.y && _canvas.y <= _top) {
            return true;
        }
    }
    return false;
}
