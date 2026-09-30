#include "side.h"
#include "toolbar.h"
#include "toolbar_kit.h"
#include "version.h"
#include "notes.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See side.h.
// ===========================================================================

#define KANA_SIDE_MARGIN    16.0f
#define KANA_SIDE_ROW_H     44.0f
#define KANA_SIDE_GAP       6.0f
#define KANA_SIDE_HEADER_H  30.0f     // a section's title
#define KANA_SIDE_MENU_W    52.0f
#define KANA_SIDE_MENU_H    44.0f
#define KANA_SIDE_CARD_W    580.0f
#define KANA_SIDE_CARD_H    590.0f
#define KANA_SIDE_BACKDROP  (rde_color){ 0, 0, 0, 110 }
#define KANA_SIDE_NOTE_H    40.0f     // a row of the notes list
#define KANA_SIDE_DOTS_W    44.0f     // its "…"
#define KANA_SIDE_INDENT    18.0f     // a canvas in a folder
#define KANA_SIDE_NOTE_CARD (rde_vec_2F){ 480.0f, 230.0f }

// The credits the character data's licences require be shown to users
// (assets/data/LICENSE-data.txt has them in full).
static const c8 KANA_SIDE_CREDITS[] =
    "Stroke order: KanjiVG, copyright Ulrich Apel, CC BY-SA 3.0 (kanjivg.tagaini.net).\n"
    "Readings and meanings: KANJIDIC2, property of the Electronic Dictionary Research and Development Group (EDRDG), "
    "used in conformance with its licence, CC BY-SA 4.0 (edrdg.org).\n"
    "JLPT levels: Jonathan Waller's JLPT Resources, CC BY (tanos.co.uk/jlpt); community lists, not official ones.";

// --- helpers ----------------------------------------------------------------------

RDE_INTERNAL rde_ui_label* kana_side_label(kana_toolbar* _toolbar, rde_ui_node* _parent, const c8* _text, f32 _px) {
    rde_ui_label* _label = rde_ui_label_create(NULL);
    rde_ui_label_set_font(_label, _toolbar->font);
    rde_ui_label_set_text(_label, _text);
    rde_ui_label_set_font_scale(_label, _px / (f32)KANA_TOOLBAR_FONT_SIZE);
    rde_ui_label_set_alignment(_label, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_MIDDLE);
    rde_ui_node_set_raycast_target(rde_ui_label_as_node(_label), false);
    rde_ui_node_add_child(_parent, rde_ui_label_as_node(_label));
    return _label;
}

RDE_INTERNAL void kana_side_show(rde_ui_node* _node, b8 _show, b8* _shown) {
    if(_show != *_shown) {
        *_shown = _show;
        rde_ui_node_set_active(_node, _show);
    }
}

// What the settings show: the HUD on or off, the pen's width mode.
RDE_INTERNAL void kana_side_refresh_settings(kana_toolbar* _toolbar) {
    kana_side* _side = &_toolbar->side;
    const b8   _hud  = _toolbar->show_hud != NULL && *_toolbar->show_hud;
    rde_ui_button_set_text(_side->hud_toggle, _hud ? "On" : "Off");
    if(_hud) { kana_toolbar_button_selected(_side->hud_toggle); } else { kana_toolbar_button_plain(_side->hud_toggle); }

    const b8 _pressure = _toolbar->ink->width_mode == KANA_INK_WIDTH_MODE_PRESSURE;
    if(_pressure) { kana_toolbar_button_plain(_side->width_even); kana_toolbar_button_selected(_side->width_pressure); }
    else          { kana_toolbar_button_selected(_side->width_even); kana_toolbar_button_plain(_side->width_pressure); }
}

void kana_side_close(kana_toolbar* _toolbar) {
    _toolbar->side.open = false;
}

// --- callbacks ----------------------------------------------------------------------

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_menu(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    _toolbar->side.open = !_toolbar->side.open;
    if(_toolbar->side.open) {
        kana_toolbar_set_palette_open(_toolbar, false);
        kana_toolbar_close_context_menu(_toolbar);
    }
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_backdrop(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    _toolbar->side.open = false;
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_kanji(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    _toolbar->side.open = false;
    kana_lasso_clear(_toolbar->lasso, _toolbar->ink);
    kana_chart_close(_toolbar->chart);
    kana_browse_open(_toolbar->browse);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_kana(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    _toolbar->side.open = false;
    kana_lasso_clear(_toolbar->lasso, _toolbar->ink);
    kana_browse_close(_toolbar->browse);
    kana_chart_open(_toolbar->chart);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_album(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    _toolbar->side.open = false;
    kana_lasso_clear(_toolbar->lasso, _toolbar->ink);
    kana_album_open(_toolbar->album);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A theme: everything restyles at once; Settings stays open to compare.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_theme(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_side_theme_ref* _ref = (const kana_side_theme_ref*)_user_data;
    kana_theme_set((KANA_THEME_)_ref->theme);
    kana_toolbar_sync(_ref->toolbar);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_settings(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    _toolbar->side.open          = false;
    _toolbar->side.settings_open = true;
    kana_side_refresh_settings(_toolbar);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_settings_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    _toolbar->side.settings_open = false;
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_hud(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    if(_toolbar->show_hud != NULL) {
        *_toolbar->show_hud = !*_toolbar->show_hud;
    }
    kana_side_refresh_settings(_toolbar);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_width_even(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    _toolbar->ink->width_mode = KANA_INK_WIDTH_MODE_CONSTANT;
    kana_side_refresh_settings(_toolbar);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_width_pressure(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    _toolbar->ink->width_mode = KANA_INK_WIDTH_MODE_PRESSURE;
    kana_side_refresh_settings(_toolbar);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// --- notes -----------------------------------------------------------------------------

// A button's text at its left, inside the button (buttons centre their label).
RDE_INTERNAL void kana_side_left_text(rde_ui_button* _button, rde_vec_2F _size, f32 _inset) {
    if(_button->internal_label == NULL) {
        return;
    }
    rde_ui_label_set_alignment(_button->internal_label, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_MIDDLE);
    kana_toolbar_place(rde_ui_label_as_node(_button->internal_label), (rde_vec_2F){ _size.x * 0.5f + _inset * 0.5f, _size.y * 0.5f },
                       (rde_vec_2F){ _size.x - _inset - 6.0f, _size.y });
}

RDE_INTERNAL void kana_side_card(kana_toolbar* _toolbar, KANA_SIDE_CARD_ _mode, u32 _note);

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_note(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_side_note_ref* _ref     = (const kana_side_note_ref*)_user_data;
    kana_toolbar*             _toolbar = _ref->toolbar;
    const kana_note*          _note    = kana_notes_find(_toolbar->notes, _ref->id);
    if(_note == NULL) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    if(_note->kind == KANA_NOTE_FOLDER) {
        kana_notes_set_expanded(_toolbar->notes, _note->id, !_note->expanded);
    } else {
        kana_notes_open(_toolbar->notes, _note->id);   // kana.c sees it and switches the page
        _toolbar->side.open = false;
        kana_toolbar_update(_toolbar);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_note_dots(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_side_note_ref* _ref = (const kana_side_note_ref*)_user_data;
    kana_side_card(_ref->toolbar, KANA_SIDE_CARD_ACTIONS, _ref->id);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// + Canvas: a new one at the top level, opened.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_new_canvas(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    c8 _name[KANA_NOTE_NAME];
    kana_notes_new_name(_toolbar->notes, KANA_NOTE_CANVAS, _name, sizeof(_name));
    const u32 _id = kana_notes_add(_toolbar->notes, KANA_NOTE_CANVAS, 0, _name);
    if(_id != 0) {
        kana_notes_open(_toolbar->notes, _id);
        _toolbar->side.open = false;
        kana_toolbar_update(_toolbar);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// + Folder: a new one, named straight away.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_new_folder(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    c8 _name[KANA_NOTE_NAME];
    kana_notes_new_name(_toolbar->notes, KANA_NOTE_FOLDER, _name, sizeof(_name));
    const u32 _id = kana_notes_add(_toolbar->notes, KANA_NOTE_FOLDER, 0, _name);
    if(_id != 0) {
        kana_side_card(_toolbar, KANA_SIDE_CARD_RENAME, _id);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_card_rename(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_side_card(_toolbar, KANA_SIDE_CARD_RENAME, _toolbar->side.card_note);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_card_delete(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_side_card(_toolbar, KANA_SIDE_CARD_DELETE, _toolbar->side.card_note);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A folder's new canvas: made in it, and opened.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_card_add(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    c8 _name[KANA_NOTE_NAME];
    kana_notes_new_name(_toolbar->notes, KANA_NOTE_CANVAS, _name, sizeof(_name));
    const u32 _id = kana_notes_add(_toolbar->notes, KANA_NOTE_CANVAS, _toolbar->side.card_note, _name);
    kana_side_card(_toolbar, KANA_SIDE_CARD_NONE, 0);
    if(_id != 0) {
        kana_notes_open(_toolbar->notes, _id);
        _toolbar->side.open = false;
    }
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_card_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_side_card((kana_toolbar*)_user_data, KANA_SIDE_CARD_NONE, 0);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Save (rename) or Delete.
RDE_INTERNAL void kana_side_card_confirm(kana_toolbar* _toolbar) {
    kana_side* _side = &_toolbar->side;
    if(_side->card_mode == KANA_SIDE_CARD_RENAME) {
        const usize _bytes = rde_ui_text_editor_get_byte_count(_side->note_field);
        c8*         _text  = rde_ui_text_editor_get_text(_side->note_field, 0, _bytes);
        kana_notes_rename(_toolbar->notes, _side->card_note, _text != NULL ? _text : "");
        rde_ui_text_editor_free_text(_side->note_field, _text);
    } else if(_side->card_mode == KANA_SIDE_CARD_DELETE) {
        kana_notes_remove(_toolbar->notes, _side->card_note);   // the open canvas gone: kana.c opens another
    }
    kana_side_card(_toolbar, KANA_SIDE_CARD_NONE, 0);
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_card_confirm(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_side_card_confirm((kana_toolbar*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Return in the name field: save.
RDE_INTERNAL void kana_side_on_field_submit(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    kana_side_card_confirm((kana_toolbar*)_user_data);
}

// The card's buttons for its mode, in a row from the right.
RDE_INTERNAL void kana_side_card_layout(kana_toolbar* _toolbar) {
    kana_side*       _side = &_toolbar->side;
    const rde_vec_2F _size = KANA_SIDE_NOTE_CARD;
    const b8         _folder = kana_notes_find(_toolbar->notes, _side->card_note) != NULL &&
                               kana_notes_find(_toolbar->notes, _side->card_note)->kind == KANA_NOTE_FOLDER;

    rde_ui_button* _row[5];
    u32            _count = 0;
    if(_side->card_mode == KANA_SIDE_CARD_ACTIONS) {
        _row[_count++] = _side->note_cancel;
        _row[_count++] = _side->note_delete;
        if(_folder) {
            _row[_count++] = _side->note_add;
        }
        _row[_count++] = _side->note_rename;
    } else {
        _row[_count++] = _side->note_confirm;
        _row[_count++] = _side->note_cancel;
    }

    rde_ui_button* const _all[] = { _side->note_rename, _side->note_add, _side->note_delete, _side->note_cancel, _side->note_confirm };
    for(u32 _i = 0; _i < sizeof(_all) / sizeof(_all[0]); _i++) {
        b8 _used = false;
        for(u32 _k = 0; _k < _count; _k++) {
            _used = _used || _row[_k] == _all[_i];
        }
        rde_ui_node_set_active(rde_ui_button_as_node(_all[_i]), _used);
    }
    const f32 _w = 100.0f;
    for(u32 _k = 0; _k < _count; _k++) {
        kana_toolbar_place(rde_ui_button_as_node(_row[_k]), (rde_vec_2F){ _size.x - 20.0f - _w * 0.5f - (f32)_k * (_w + KANA_SIDE_GAP), 20.0f + 22.0f },
                           (rde_vec_2F){ _w, 44.0f });
    }
    rde_ui_node_set_active(rde_ui_text_editor_as_node(_side->note_field), _side->card_mode == KANA_SIDE_CARD_RENAME);
    rde_ui_node_set_active(rde_ui_label_as_node(_side->note_body), _side->card_mode != KANA_SIDE_CARD_RENAME);
}

// Opens the note card on a note in a mode (NONE closes it).
RDE_INTERNAL void kana_side_card(kana_toolbar* _toolbar, KANA_SIDE_CARD_ _mode, u32 _note) {
    kana_side*       _side = &_toolbar->side;
    const kana_note* _n    = kana_notes_find(_toolbar->notes, _note);
    if(_n == NULL) {
        _mode = KANA_SIDE_CARD_NONE;
    }
    _side->card_mode = (u8)_mode;
    _side->card_note = _note;
    if(_mode == KANA_SIDE_CARD_NONE) {
        return;
    }

    const b8 _folder = _n->kind == KANA_NOTE_FOLDER;
    c8       _title[KANA_NOTE_NAME + 32];
    c8       _body[160] = "";
    if(_mode == KANA_SIDE_CARD_ACTIONS) {
        snprintf(_title, sizeof(_title), "%s", _n->name);
        if(_folder) {
            const u32 _in = kana_notes_count_in(_toolbar->notes, _n->id);
            snprintf(_body, sizeof(_body), "Folder, %u canvas%s", _in, _in == 1 ? "" : "es");
        } else {
            snprintf(_body, sizeof(_body), "Canvas");
        }
    } else if(_mode == KANA_SIDE_CARD_RENAME) {
        snprintf(_title, sizeof(_title), "Rename %s", _folder ? "folder" : "canvas");
        const usize _bytes = rde_ui_text_editor_get_byte_count(_side->note_field);
        if(_bytes > 0) {
            rde_ui_text_editor_delete_range(_side->note_field, 0, _bytes);
        }
        rde_ui_text_editor_insert_at(_side->note_field, 0, _n->name, strlen(_n->name));
        rde_ui_text_editor_select_all(_side->note_field);
    } else {
        snprintf(_title, sizeof(_title), "Delete \"%s\"?", _n->name);
        const u32 _in = _folder ? kana_notes_count_in(_toolbar->notes, _n->id) : 0u;
        if(_folder && _in > 0) {
            snprintf(_body, sizeof(_body), "The %u canvas%s in it go too. This cannot be undone.", _in, _in == 1 ? "" : "es");
        } else {
            snprintf(_body, sizeof(_body), "This cannot be undone.");
        }
    }
    rde_ui_label_set_text(_side->note_title, _title);
    rde_ui_label_set_text(_side->note_body, _body);
    rde_ui_button_set_text(_side->note_confirm, _mode == KANA_SIDE_CARD_DELETE ? "Delete" : "Save");
    if(_mode == KANA_SIDE_CARD_DELETE) {
        kana_toolbar_button_colors(_side->note_confirm, kana_theme_active()->danger, 0.0f, (rde_color){ 0, 0, 0, 0 });
    } else {
        kana_toolbar_button_selected(_side->note_confirm);
    }
    kana_side_card_layout(_toolbar);

    if(_mode == KANA_SIDE_CARD_RENAME) {
        rde_ui_node_focus(rde_ui_text_editor_as_node(_side->note_field));   // the keyboard comes up
    }
}

// The list, built again: folders (each followed by its canvases when open), then
// the canvases at the top level. The open canvas is marked.
RDE_INTERNAL void kana_side_build_notes(kana_toolbar* _toolbar) {
    kana_side*        _side  = &_toolbar->side;
    const kana_notes* _notes = _toolbar->notes;
    const kana_note*  _all   = (const kana_note*)_notes->notes.memory;
    const u32         _count = (u32)rde_arr_length(&_notes->notes);

    rde_ui_scroll_area_clear_contents(_side->notes_list);
    free(_side->_note_refs);
    _side->_note_refs      = (kana_side_note_ref*)calloc(_count > 0 ? _count : 1u, sizeof(kana_side_note_ref));
    _side->_notes_revision = _notes->revision;
    _side->_notes_open     = _notes->open;
    _side->_notes_built    = true;

    // The rows, in order.
    u32* _order = (u32*)malloc(sizeof(u32) * (_count > 0 ? _count : 1u));
    u32  _rows  = 0;
    for(u32 _f = 0; _f < _count; _f++) {
        if(_all[_f].kind != KANA_NOTE_FOLDER) {
            continue;
        }
        _order[_rows++] = _f;
        for(u32 _c = 0; _c < _count && _all[_f].expanded; _c++) {
            if(_all[_c].kind == KANA_NOTE_CANVAS && _all[_c].parent == _all[_f].id) {
                _order[_rows++] = _c;
            }
        }
    }
    for(u32 _c = 0; _c < _count; _c++) {
        if(_all[_c].kind == KANA_NOTE_CANVAS && _all[_c].parent == 0) {
            _order[_rows++] = _c;
        }
    }

    const f32 _width   = _side->_list_width;
    const f32 _content = fmaxf(1.0f, (f32)_rows * (KANA_SIDE_NOTE_H + KANA_SIDE_GAP));
    rde_ui_scroll_area_set_content_size(_side->notes_list, (rde_vec_2F){ _width, _content });

    for(u32 _r = 0; _r < _rows; _r++) {
        const kana_note* _n      = &_all[_order[_r]];
        const b8         _folder = _n->kind == KANA_NOTE_FOLDER;
        const f32        _indent = !_folder && _n->parent != 0 ? KANA_SIDE_INDENT : 0.0f;
        const f32        _y      = _content - (f32)_r * (KANA_SIDE_NOTE_H + KANA_SIDE_GAP) - KANA_SIDE_NOTE_H * 0.5f;

        _side->_note_refs[_order[_r]] = (kana_side_note_ref){ _toolbar, _n->id };
        kana_side_note_ref* _ref = &_side->_note_refs[_order[_r]];

        c8 _label[KANA_NOTE_NAME + 32];
        if(_folder) {
            snprintf(_label, sizeof(_label), "%s %s  (%u)", _n->expanded ? "\xE2\x80\x93" : "+", _n->name, kana_notes_count_in(_notes, _n->id));   // – or +
        } else {
            snprintf(_label, sizeof(_label), "%s", _n->name);
        }

        const rde_vec_2F _row_size = { _width - _indent - KANA_SIDE_DOTS_W - KANA_SIDE_GAP, KANA_SIDE_NOTE_H };
        rde_ui_button*   _row      = kana_toolbar_button(_toolbar, rde_ui_scroll_area_as_node(_side->notes_list), _label, kana_side_on_note);
        rde_ui_button_set_on_click(_row, kana_side_on_note, _ref);
        kana_toolbar_place(rde_ui_button_as_node(_row), (rde_vec_2F){ _indent + _row_size.x * 0.5f, _y }, _row_size);
        kana_side_left_text(_row, _row_size, 12.0f);
        if(!_folder && _n->id == _notes->open) {
            kana_toolbar_button_selected(_row);   // the canvas on the page
        }

        rde_ui_button* _dots = kana_toolbar_button(_toolbar, rde_ui_scroll_area_as_node(_side->notes_list), "\xE2\x80\xA6", kana_side_on_note_dots);   // …
        rde_ui_button_set_on_click(_dots, kana_side_on_note_dots, _ref);
        kana_toolbar_place(rde_ui_button_as_node(_dots), (rde_vec_2F){ _width - KANA_SIDE_DOTS_W * 0.5f, _y }, (rde_vec_2F){ KANA_SIDE_DOTS_W, KANA_SIDE_NOTE_H });
    }
    free(_order);
}

// --- layout ---------------------------------------------------------------------------

RDE_INTERNAL void kana_side_layout(kana_toolbar* _toolbar) {
    kana_side*       _side   = &_toolbar->side;
    const rde_vec_2F _screen = kana_toolbar_virtual_size(_toolbar);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_toolbar->window);   // left, top, right, bottom
    _side->_laid_out        = _screen;
    _side->_laid_out_insets = _insets;

    // The menu button: top-left, in the safe area.
    _side->_menu_size   = (rde_vec_2F){ KANA_SIDE_MENU_W, KANA_SIDE_MENU_H };
    _side->_menu_center = (rde_vec_2F){ (f32)_insets.x + KANA_SIDE_MARGIN + KANA_SIDE_MENU_W * 0.5f,
                                        _screen.y - (f32)_insets.y - KANA_SIDE_MARGIN * 0.5f - KANA_SIDE_MENU_H * 0.5f };
    kana_toolbar_place(rde_ui_button_as_node(_side->menu_button), _side->_menu_center, _side->_menu_size);
    for(u32 _i = 0; _i < 3; _i++) {
        kana_toolbar_place(rde_ui_image_as_node(_side->menu_bars[_i]), (rde_vec_2F){ KANA_SIDE_MENU_W * 0.5f, KANA_SIDE_MENU_H * 0.5f + (1.0f - (f32)_i) * 7.0f },
                           (rde_vec_2F){ 20.0f, 3.0f });
    }

    // The backdrops cover the screen; the panel is the left edge, full height.
    kana_toolbar_place(rde_ui_button_as_node(_side->backdrop), (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.5f }, _screen);
    kana_toolbar_place(rde_ui_button_as_node(_side->settings_backdrop), (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.5f }, _screen);
    const f32 _w = fminf(KANA_SIDE_WIDTH + (f32)_insets.x, _screen.x * 0.85f);
    kana_toolbar_place(rde_ui_image_as_node(_side->panel), (rde_vec_2F){ _w * 0.5f, _screen.y * 0.5f }, (rde_vec_2F){ _w, _screen.y });

    // The panel's contents, top down (panel-local), below the menu button.
    const f32 _x0 = (f32)_insets.x + KANA_SIDE_MARGIN;
    const f32 _cw = _w - _x0 - KANA_SIDE_MARGIN;
    f32       _y  = _screen.y - (f32)_insets.y - KANA_SIDE_MARGIN - KANA_SIDE_MENU_H - KANA_SIDE_MARGIN;

    #define KANA_SIDE_HEADER(_label) do { \
        kana_toolbar_place(rde_ui_label_as_node(_label), (rde_vec_2F){ _x0 + _cw * 0.5f, _y - KANA_SIDE_HEADER_H * 0.5f }, (rde_vec_2F){ _cw, KANA_SIDE_HEADER_H }); \
        _y -= KANA_SIDE_HEADER_H; \
    } while(0)
    #define KANA_SIDE_ROW(_button) do { \
        kana_toolbar_place(rde_ui_button_as_node(_button), (rde_vec_2F){ _x0 + _cw * 0.5f, _y - KANA_SIDE_ROW_H * 0.5f }, (rde_vec_2F){ _cw, KANA_SIDE_ROW_H }); \
        _y -= KANA_SIDE_ROW_H + KANA_SIDE_GAP; \
    } while(0)

    KANA_SIDE_HEADER(_side->study_label);
    KANA_SIDE_ROW(_side->kanji);
    KANA_SIDE_ROW(_side->kana);
    KANA_SIDE_ROW(_side->album);
    _y -= KANA_SIDE_MARGIN * 0.5f;

    // Notes: the header with + Folder / + Canvas at its right, then the list down
    // to the bottom row.
    kana_toolbar_place(rde_ui_label_as_node(_side->notes_label), (rde_vec_2F){ _x0 + (_cw - 200.0f) * 0.5f, _y - 18.0f }, (rde_vec_2F){ _cw - 200.0f, 36.0f });
    kana_toolbar_place(rde_ui_button_as_node(_side->new_canvas), (rde_vec_2F){ _x0 + _cw - 48.0f, _y - 18.0f }, (rde_vec_2F){ 96.0f, 36.0f });
    kana_toolbar_place(rde_ui_button_as_node(_side->new_folder), (rde_vec_2F){ _x0 + _cw - 96.0f - KANA_SIDE_GAP - 48.0f, _y - 18.0f }, (rde_vec_2F){ 96.0f, 36.0f });
    _y -= 36.0f + KANA_SIDE_GAP;

    #undef KANA_SIDE_ROW
    #undef KANA_SIDE_HEADER

    // The bottom: the version on the left, Settings on the right.
    const f32 _by = (f32)_insets.w + KANA_SIDE_MARGIN + 20.0f;
    const f32 _list_bottom = _by + 20.0f + KANA_SIDE_MARGIN;
    const f32 _list_h      = fmaxf(KANA_SIDE_NOTE_H, _y - _list_bottom);
    kana_toolbar_place(rde_ui_scroll_area_as_node(_side->notes_list), (rde_vec_2F){ _x0 + _cw * 0.5f, _list_bottom + _list_h * 0.5f }, (rde_vec_2F){ _cw, _list_h });
    _side->_list_width  = _cw;
    _side->_notes_built = false;   // rows are laid out for the list's width
    kana_toolbar_place(rde_ui_button_as_node(_side->settings_button), (rde_vec_2F){ _x0 + _cw - 60.0f, _by }, (rde_vec_2F){ 120.0f, 40.0f });
    kana_toolbar_place(rde_ui_label_as_node(_side->version), (rde_vec_2F){ _x0 + (_cw - 130.0f) * 0.5f, _by }, (rde_vec_2F){ _cw - 130.0f, 40.0f });

    // Settings: a card in the middle.
    const f32 _kw = fminf(KANA_SIDE_CARD_W, _screen.x - 2.0f * KANA_SIDE_MARGIN);
    const f32 _kh = fminf(KANA_SIDE_CARD_H, _screen.y - (f32)(_insets.y + _insets.w) - 2.0f * KANA_SIDE_MARGIN);
    const f32 _m  = 20.0f;
    const f32 _lw = _kw - 2.0f * _m;
    kana_toolbar_place(rde_ui_image_as_node(_side->card), (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.5f }, (rde_vec_2F){ _kw, _kh });
    kana_toolbar_place(rde_ui_label_as_node(_side->settings_title), (rde_vec_2F){ _m + _lw * 0.5f, _kh - 36.0f }, (rde_vec_2F){ _lw, 44.0f });

    // The theme: a header, then one button per theme across the card.
    f32 _ry = _kh - 84.0f;
    kana_toolbar_place(rde_ui_label_as_node(_side->theme_label), (rde_vec_2F){ _m + _lw * 0.5f, _ry }, (rde_vec_2F){ _lw, 30.0f });
    _ry -= 42.0f;
    const f32 _tw = (_lw - KANA_SIDE_GAP * (f32)(KANA_THEME_COUNT - 1u)) / (f32)KANA_THEME_COUNT;
    for(u32 _i = 0; _i < KANA_THEME_COUNT; _i++) {
        kana_toolbar_place(rde_ui_button_as_node(_side->themes[_i]), (rde_vec_2F){ _m + (f32)_i * (_tw + KANA_SIDE_GAP) + _tw * 0.5f, _ry },
                           (rde_vec_2F){ _tw, 40.0f });
    }
    _ry -= 62.0f;
    kana_toolbar_place(rde_ui_label_as_node(_side->hud_label), (rde_vec_2F){ _m + _lw * 0.3f, _ry }, (rde_vec_2F){ _lw * 0.6f, 44.0f });
    kana_toolbar_place(rde_ui_button_as_node(_side->hud_toggle), (rde_vec_2F){ _kw - _m - 60.0f, _ry }, (rde_vec_2F){ 120.0f, 40.0f });
    _ry -= 54.0f;
    kana_toolbar_place(rde_ui_label_as_node(_side->width_label), (rde_vec_2F){ _m + _lw * 0.25f, _ry }, (rde_vec_2F){ _lw * 0.5f, 44.0f });
    kana_toolbar_place(rde_ui_button_as_node(_side->width_even), (rde_vec_2F){ _kw - _m - 186.0f, _ry }, (rde_vec_2F){ 120.0f, 40.0f });
    kana_toolbar_place(rde_ui_button_as_node(_side->width_pressure), (rde_vec_2F){ _kw - _m - 60.0f, _ry }, (rde_vec_2F){ 120.0f, 40.0f });
    _ry -= 56.0f;
    kana_toolbar_place(rde_ui_label_as_node(_side->about_label), (rde_vec_2F){ _m + _lw * 0.5f, _ry }, (rde_vec_2F){ _lw, 34.0f });

    const f32 _text_top    = _ry - 20.0f;
    const f32 _text_bottom = _m + 44.0f + 12.0f;
    kana_toolbar_place(rde_ui_label_as_node(_side->about_text), (rde_vec_2F){ _m + _lw * 0.5f, (_text_top + _text_bottom) * 0.5f },
                       (rde_vec_2F){ _lw, fmaxf(20.0f, _text_top - _text_bottom) });
    kana_toolbar_place(rde_ui_button_as_node(_side->settings_close), (rde_vec_2F){ _kw - _m - 60.0f, _m + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });

    // The note card: a title, a line (or the name field), buttons at the bottom.
    const rde_vec_2F _nc = KANA_SIDE_NOTE_CARD;
    kana_toolbar_place(rde_ui_button_as_node(_side->note_backdrop), (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.5f }, _screen);
    kana_toolbar_place(rde_ui_image_as_node(_side->note_card), (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.6f }, _nc);
    kana_toolbar_place(rde_ui_label_as_node(_side->note_title), (rde_vec_2F){ _nc.x * 0.5f, _nc.y - 36.0f }, (rde_vec_2F){ _nc.x - 40.0f, 40.0f });
    kana_toolbar_place(rde_ui_label_as_node(_side->note_body), (rde_vec_2F){ _nc.x * 0.5f, _nc.y - 92.0f }, (rde_vec_2F){ _nc.x - 40.0f, 60.0f });
    kana_toolbar_place(rde_ui_text_editor_as_node(_side->note_field), (rde_vec_2F){ _nc.x * 0.5f, _nc.y - 100.0f }, (rde_vec_2F){ _nc.x - 40.0f, 44.0f });
    if(_side->card_mode != KANA_SIDE_CARD_NONE) {
        kana_side_card_layout(_toolbar);
    }
}

// --- lifetime ---------------------------------------------------------------------------

void kana_side_create(kana_toolbar* _toolbar, rde_ui_node* _root) {
    kana_side* _side = &_toolbar->side;
    memset(_side, 0, sizeof(*_side));

    // Under the panel, the backdrop; the menu button goes over both (added last).
    _side->backdrop = rde_ui_button_create(NULL, NULL);
    rde_ui_button_set_on_click(_side->backdrop, kana_side_on_backdrop, _toolbar);
    rde_ui_node_add_child(_root, rde_ui_button_as_node(_side->backdrop));

    _side->panel = rde_ui_image_create(NULL);
    rde_ui_node* _panel = rde_ui_image_as_node(_side->panel);
    rde_ui_node_set_blocks_input(_panel, true);
    rde_ui_node_add_child(_root, _panel);

    _side->study_label = kana_side_label(_toolbar, _panel, "STUDY", 14.0f);
    _side->kanji       = kana_toolbar_button(_toolbar, _panel, "Kanji", kana_side_on_kanji);
    _side->kana        = kana_toolbar_button(_toolbar, _panel, "Kana", kana_side_on_kana);
    _side->album       = kana_toolbar_button(_toolbar, _panel, "Album", kana_side_on_album);
    if(!kana_browse_available(_toolbar->browse)) { kana_toolbar_set_enabled(_side->kanji, false); }   // no character data
    if(kana_chart_count(_toolbar->chart) == 0)   { kana_toolbar_set_enabled(_side->kana, false); }
    if(_toolbar->album->db == NULL)              { kana_toolbar_set_enabled(_side->album, false); }

    _side->notes_label = kana_side_label(_toolbar, _panel, "NOTES", 14.0f);
    _side->new_folder  = kana_toolbar_button(_toolbar, _panel, "+ Folder", kana_side_on_new_folder);
    _side->new_canvas  = kana_toolbar_button(_toolbar, _panel, "+ Canvas", kana_side_on_new_canvas);
    _side->notes_list  = rde_ui_scroll_area_create(NULL);
    rde_ui_scroll_area_set_bar_thickness(_side->notes_list, 3.0f);
    rde_ui_node_add_child(_panel, rde_ui_scroll_area_as_node(_side->notes_list));

    c8 _version[64];
    snprintf(_version, sizeof(_version), "Kana %s", KANA_VERSION);
    _side->version         = kana_side_label(_toolbar, _panel, _version, 13.0f);
    _side->settings_button = kana_toolbar_button(_toolbar, _panel, "Settings", kana_side_on_settings);

    // The menu button, with three bars for an icon (they let the press through).
    _side->menu_button = kana_toolbar_button(_toolbar, _root, "", kana_side_on_menu);
    for(u32 _i = 0; _i < 3; _i++) {
        _side->menu_bars[_i] = rde_ui_image_create(NULL);
        rde_ui_node_set_raycast_target(rde_ui_image_as_node(_side->menu_bars[_i]), false);
        rde_ui_node_add_child(rde_ui_button_as_node(_side->menu_button), rde_ui_image_as_node(_side->menu_bars[_i]));
    }

    // Settings, over everything.
    _side->settings_backdrop = rde_ui_button_create(NULL, NULL);
    rde_ui_button_set_on_click(_side->settings_backdrop, kana_side_on_settings_close, _toolbar);
    rde_ui_node_add_child(_root, rde_ui_button_as_node(_side->settings_backdrop));

    _side->card = rde_ui_image_create(NULL);
    rde_ui_node* _card = rde_ui_image_as_node(_side->card);
    rde_ui_node_set_blocks_input(_card, true);
    rde_ui_node_add_child(_root, _card);

    _side->settings_title = kana_side_label(_toolbar, _card, "Settings", 26.0f);
    _side->theme_label    = kana_side_label(_toolbar, _card, "THEME", 14.0f);
    for(u32 _i = 0; _i < KANA_THEME_COUNT; _i++) {
        _side->theme_refs[_i] = (kana_side_theme_ref){ _toolbar, _i };
        _side->themes[_i]     = kana_toolbar_button(_toolbar, _card, kana_theme_get((KANA_THEME_)_i)->name, kana_side_on_theme);
        rde_ui_button_set_on_click(_side->themes[_i], kana_side_on_theme, &_side->theme_refs[_i]);
    }
    _side->hud_label      = kana_side_label(_toolbar, _card, "Diagnostics HUD", 18.0f);
    _side->hud_toggle     = kana_toolbar_button(_toolbar, _card, "Off", kana_side_on_hud);
    _side->width_label    = kana_side_label(_toolbar, _card, "Pen width", 18.0f);
    _side->width_even     = kana_toolbar_button(_toolbar, _card, "Even", kana_side_on_width_even);
    _side->width_pressure = kana_toolbar_button(_toolbar, _card, "Pressure", kana_side_on_width_pressure);
    _side->about_label    = kana_side_label(_toolbar, _card, "ABOUT", 14.0f);

    c8 _about[768];
    snprintf(_about, sizeof(_about), "Kana %s, built %s.\n\n%s", KANA_VERSION, __DATE__, KANA_SIDE_CREDITS);
    _side->about_text = kana_side_label(_toolbar, _card, _about, 14.0f);
    rde_ui_label_set_wrap(_side->about_text, true);
    rde_ui_label_set_alignment(_side->about_text, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_TOP);
    _side->settings_close = kana_toolbar_button(_toolbar, _card, "Close", kana_side_on_settings_close);

    // The note card, over the panel (and a backdrop that cancels it).
    _side->note_backdrop = rde_ui_button_create(NULL, NULL);
    rde_ui_button_set_on_click(_side->note_backdrop, kana_side_on_card_cancel, _toolbar);
    rde_ui_node_add_child(_root, rde_ui_button_as_node(_side->note_backdrop));
    _side->note_card = rde_ui_image_create(NULL);
    rde_ui_node* _note_card = rde_ui_image_as_node(_side->note_card);
    rde_ui_node_set_blocks_input(_note_card, true);
    rde_ui_node_add_child(_root, _note_card);
    _side->note_title = kana_side_label(_toolbar, _note_card, "", 22.0f);
    _side->note_body  = kana_side_label(_toolbar, _note_card, "", 16.0f);
    rde_ui_label_set_wrap(_side->note_body, true);
    _side->note_field = rde_ui_text_editor_create(_toolbar->font, NULL);
    rde_ui_text_editor_set_multiline(_side->note_field, false);
    rde_ui_text_editor_set_font_size(_side->note_field, 18u);
    rde_ui_text_editor_set_max_chars(_side->note_field, KANA_NOTE_NAME - 8);
    rde_ui_text_editor_set_content_insets(_side->note_field, 10.0f, 8.0f, 10.0f, 8.0f);
    rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_side->note_field), _toolbar);
    rde_ui_text_editor_set_on_submit(_side->note_field, kana_side_on_field_submit);
    rde_ui_node_add_child(_note_card, rde_ui_text_editor_as_node(_side->note_field));
    _side->note_rename  = kana_toolbar_button(_toolbar, _note_card, "Rename", kana_side_on_card_rename);
    _side->note_add     = kana_toolbar_button(_toolbar, _note_card, "+ Canvas", kana_side_on_card_add);
    _side->note_delete  = kana_toolbar_button(_toolbar, _note_card, "Delete", kana_side_on_card_delete);
    _side->note_cancel  = kana_toolbar_button(_toolbar, _note_card, "Cancel", kana_side_on_card_cancel);
    _side->note_confirm = kana_toolbar_button(_toolbar, _note_card, "Save", kana_side_on_card_confirm);

    // All hidden until asked for; the menu button shows with the page.
    rde_ui_node* const _hidden[] = { rde_ui_button_as_node(_side->backdrop), _panel, rde_ui_button_as_node(_side->menu_button),
                                     rde_ui_button_as_node(_side->settings_backdrop), _card,
                                     rde_ui_button_as_node(_side->note_backdrop), _note_card };
    for(u32 _i = 0; _i < sizeof(_hidden) / sizeof(_hidden[0]); _i++) {
        rde_ui_node_set_active(_hidden[_i], false);
    }

    kana_side_layout(_toolbar);
}

// --- every frame ------------------------------------------------------------------------

void kana_side_update(kana_toolbar* _toolbar, b8 _full) {
    kana_side* _side = &_toolbar->side;
    if(_side->panel == NULL) {
        return;
    }

    // Laid out again when the screen rotates — and when the safe area changes:
    // at start iOS reports none (SDL has the whole window until the view is laid
    // out), then the status bar's arrives a frame or two later.
    const rde_vec_2F _screen = kana_toolbar_virtual_size(_toolbar);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_toolbar->window);
    if(memcmp(&_screen, &_side->_laid_out, sizeof(rde_vec_2F)) != 0 || memcmp(&_insets, &_side->_laid_out_insets, sizeof(rde_vec_4I)) != 0) {
        kana_side_layout(_toolbar);
    }

    if(_full) {
        _side->open = false;          // a scene took the screen: the panel is done
    }

    kana_side_show(rde_ui_button_as_node(_side->menu_button), !_full, &_side->_shown_menu);
    b8 _backdrop_shown = _side->_shown_panel;
    kana_side_show(rde_ui_button_as_node(_side->backdrop), _side->open, &_backdrop_shown);
    kana_side_show(rde_ui_image_as_node(_side->panel), _side->open, &_side->_shown_panel);
    b8 _settings_backdrop_shown = _side->_shown_settings;
    kana_side_show(rde_ui_button_as_node(_side->settings_backdrop), _side->settings_open, &_settings_backdrop_shown);
    kana_side_show(rde_ui_image_as_node(_side->card), _side->settings_open, &_side->_shown_settings);

    const b8 _card = _side->card_mode != KANA_SIDE_CARD_NONE && _side->open;
    b8 _note_backdrop_shown = _side->_shown_note_card;
    kana_side_show(rde_ui_button_as_node(_side->note_backdrop), _card, &_note_backdrop_shown);
    kana_side_show(rde_ui_image_as_node(_side->note_card), _card, &_side->_shown_note_card);
    if(!_side->open) {
        _side->card_mode = KANA_SIDE_CARD_NONE;
    }

    // The notes list, rebuilt when the notes changed (or the list's size did).
    if(_side->open && _toolbar->notes != NULL &&
       (!_side->_notes_built || _side->_notes_revision != _toolbar->notes->revision || _side->_notes_open != _toolbar->notes->open)) {
        kana_side_build_notes(_toolbar);
    }
}

b8 kana_side_hit(const kana_toolbar* _toolbar, rde_vec_2F _ui) {
    const kana_side* _side = &_toolbar->side;
    if(_side->_shown_panel || _side->_shown_settings || _side->_shown_note_card) {
        return true;
    }
    return _side->_shown_menu &&
           _ui.x >= _side->_menu_center.x - _side->_menu_size.x * 0.5f && _ui.x <= _side->_menu_center.x + _side->_menu_size.x * 0.5f &&
           _ui.y >= _side->_menu_center.y - _side->_menu_size.y * 0.5f && _ui.y <= _side->_menu_center.y + _side->_menu_size.y * 0.5f;
}

// --- the theme ----------------------------------------------------------------------------

void kana_side_apply_theme(kana_toolbar* _toolbar) {
    kana_side*        _side = &_toolbar->side;
    const kana_theme* _t    = kana_theme_active();
    if(_side->panel == NULL) {
        return;
    }

    kana_toolbar_style_panel(_side->panel, 0.0f, 1.0f);
    kana_toolbar_style_panel(_side->card, 14.0f, 1.0f);
    kana_toolbar_button_colors(_side->backdrop, KANA_SIDE_BACKDROP, 0.0f, (rde_color){ 0, 0, 0, 0 });
    kana_toolbar_button_colors(_side->settings_backdrop, KANA_SIDE_BACKDROP, 0.0f, (rde_color){ 0, 0, 0, 0 });

    kana_toolbar_restyle_button(_side->menu_button);
    for(u32 _i = 0; _i < 3; _i++) {
        rde_ui_image_set_style(_side->menu_bars[_i], RDE_UI_STATE_NORMAL, kana_toolbar_style(_t->button_text, 1.5f));
    }

    rde_ui_button* const _buttons[] = { _side->kanji, _side->kana, _side->album, _side->new_folder, _side->new_canvas, _side->settings_button,
                                        _side->hud_toggle, _side->width_even, _side->width_pressure, _side->settings_close,
                                        _side->note_rename, _side->note_add, _side->note_delete, _side->note_cancel, _side->note_confirm };
    for(u32 _i = 0; _i < sizeof(_buttons) / sizeof(_buttons[0]); _i++) {
        kana_toolbar_restyle_button(_buttons[_i]);
    }
    kana_toolbar_style_panel(_side->note_card, 14.0f, 1.0f);
    kana_toolbar_button_colors(_side->note_backdrop, KANA_SIDE_BACKDROP, 0.0f, (rde_color){ 0, 0, 0, 0 });
    kana_toolbar_button_colors(_side->note_delete, _t->danger, 0.0f, (rde_color){ 0, 0, 0, 0 });
    rde_ui_text_editor_set_background(_side->note_field, true, _t->field);
    rde_ui_scroll_area_set_background_color(_side->notes_list, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_track_color(_side->notes_list, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_thumb_colors(_side->notes_list, _t->grip, kana_toolbar_shade(_t->grip, 20), kana_toolbar_shade(_t->grip, 40));
    _side->_notes_built = false;   // the rows restyle as they are built again

    // Each theme's button previews it: its page, its text; the current one ringed.
    for(u32 _i = 0; _i < KANA_THEME_COUNT; _i++) {
        const kana_theme* _other   = kana_theme_get((KANA_THEME_)_i);
        const b8          _current = (KANA_THEME_)_i == kana_theme_index();
        kana_toolbar_button_colors(_side->themes[_i], _other->page, _current ? 3.0f : 1.0f, _current ? _t->button_selected : _other->sheet_outline);
        rde_ui_label_set_color(_side->themes[_i]->internal_label, _other->text);
    }

    rde_ui_label* const _headers[] = { _side->study_label, _side->theme_label, _side->notes_label, _side->about_label };
    for(u32 _i = 0; _i < sizeof(_headers) / sizeof(_headers[0]); _i++) {
        rde_ui_label_set_color(_headers[_i], _t->field_placeholder);
    }
    rde_ui_label* const _texts[] = { _side->settings_title, _side->hud_label, _side->width_label, _side->about_text, _side->note_title, _side->note_body };
    for(u32 _i = 0; _i < sizeof(_texts) / sizeof(_texts[0]); _i++) {
        rde_ui_label_set_color(_texts[_i], _t->button_text);
    }
    rde_ui_label_set_color(_side->version, _t->button_text_disabled);

    kana_side_refresh_settings(_toolbar);
}
