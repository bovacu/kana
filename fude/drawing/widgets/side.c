#include "drawing/widgets/side.h"
#include "drawing/app/ui.h"
#include "drawing/widgets/notice.h"
#include "drawing/widgets/kit.h"
#include "drawing/ink/notes.h"
#include "drawing/base/kfile.h"
#include "drawing/widgets/icons.h"
#include "drawing/base/text.h"
#include "drawing/widgets/draw.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See side.h.
// ===========================================================================

#define FUDE_SIDE_MARGIN    16.0f
#define FUDE_SIDE_ROW_H     44.0f
#define FUDE_SIDE_GAP       6.0f
#define FUDE_SIDE_HEADER_H  30.0f     // a section's title
#define FUDE_SIDE_MENU_W    52.0f
#define FUDE_SIDE_MENU_H    44.0f
#define FUDE_SIDE_CARD_W    580.0f
#define FUDE_SIDE_CARD_H    888.0f
#define FUDE_SIDE_LICENCE_PX 11.0f    // Licences' text
#define FUDE_SIDE_NOTE_H    40.0f     // a row of the notes list
#define FUDE_SIDE_DOTS_W    44.0f     // its "…"
#define FUDE_SIDE_INDENT    18.0f     // a canvas in a folder
#define FUDE_SIDE_NOTE_CARD (rde_vec_2F){ 540.0f, 196.0f }
#define FUDE_SIDE_CARD_PAD  22.0f     // inside a card
#define FUDE_SIDE_HANDLE_W  28.0f     // a row's drag handle
#define FUDE_SIDE_EDGE      44.0f     // a drag this near the list's top or bottom scrolls it
#define FUDE_SIDE_AUTOSCROLL 9.0f     // ...this many units a frame

// The version, the credits (About) and the documents of Licences are the app's
// (info.h): Kana's credits are the short ones its data's licences require be
// shown, in full in Licences.

// Licences: its documents, the app's.
RDE_INTERNAL u32 fude_side_licence_count(const fude_ui* _ui) {
    const u32 _n = _ui->app->info->licence_count;
    return _n < FUDE_SIDE_LICENCE_DOCS ? _n : FUDE_SIDE_LICENCE_DOCS;
}

// --- helpers ----------------------------------------------------------------------

rde_ui_label* fude_side_label(fude_ui* _ui, rde_ui_node* _parent, const c8* _text, f32 _px) {
    rde_ui_label* _label = rde_ui_label_create(NULL);
    rde_ui_label_set_font(_label, _ui->font);
    rde_ui_label_set_text(_label, _text);
    rde_ui_label_set_font_scale(_label, _px / (f32)FUDE_KIT_FONT_SIZE);
    rde_ui_label_set_alignment(_label, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_MIDDLE);
    // A longer language shrinks it to fit, never cuts it.
    rde_ui_label_set_auto_fit(_label, true);
    rde_ui_label_set_auto_fit_min_scale(_label, 0.6f);
    rde_ui_node_set_raycast_target(rde_ui_label_as_node(_label), false);
    rde_ui_node_add_child(_parent, rde_ui_label_as_node(_label));
    return _label;
}

RDE_INTERNAL void fude_side_show(rde_ui_node* _node, b8 _show, b8* _shown) {
    if(_show != *_shown) {
        *_shown = _show;
        rde_ui_node_set_active(_node, _show);
    }
}

// The app's sections in Settings (extension.h).
RDE_INTERNAL u32 fude_side_section_count(const fude_ui* _ui) {
    const fude_extension* _ext = fude_app_ext(_ui->app);
    return _ext->sections == NULL ? 0u : _ext->section_count < FUDE_EXTENSION_SECTIONS ? _ext->section_count : FUDE_EXTENSION_SECTIONS;
}

// What the settings show: the pen's width mode, the paper size, the app's sections.
RDE_INTERNAL void fude_side_refresh_settings(fude_ui* _ui) {
    fude_side* _side = &_ui->side;
    for(u32 _i = 0; _i < fude_side_section_count(_ui); _i++) {
        fude_app_ext(_ui->app)->sections[_i].refresh(_ui, true);
    }
    const b8 _pressure = _ui->app->ink->width_mode == FUDE_INK_WIDTH_MODE_PRESSURE;
    if(_pressure) { fude_kit_button_plain(_side->width_even); fude_kit_button_selected(_side->width_pressure); }
    else          { fude_kit_button_selected(_side->width_even); fude_kit_button_plain(_side->width_pressure); }

    for(u32 _i = 0; _i < FUDE_PAPER_SIZE_COUNT; _i++) {
        if(_ui->app->canvas->paper_size == (FUDE_PAPER_SIZE_)_i) { fude_kit_button_selected(_side->paper_sizes[_i]); }
        else                                                     { fude_kit_button_plain(_side->paper_sizes[_i]); }
    }
    for(u32 _i = 0; _i < FUDE_TEXT_LANGUAGES; _i++) {
        if(FUDE_TEXT_LANGUAGE_LIST[_i].language == fude_text_language()) { fude_kit_button_selected(_side->languages[_i]); }
        else                                                               { fude_kit_button_quiet(_side->languages[_i]); }
        fude_kit_button_round(_side->languages[_i], 12.0f);
    }
}

void fude_side_close(fude_ui* _ui) {
    _ui->side.open = false;
}

// --- callbacks ----------------------------------------------------------------------

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_menu(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->side.open = !_ui->side.open;
    if(_ui->side.open) {
        fude_toolbar_set_palette_open(&_ui->bar, false);
        fude_pagemenu_close_context(&_ui->page);
    }
    fude_ui_update(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_backdrop(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->side.open = false;
    fude_ui_update(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// The app's navigation (extension.h).
RDE_INTERNAL const fude_extension_nav* fude_side_nav(const fude_ui* _ui) {
    const fude_extension_nav* _nav = fude_app_ext(_ui->app)->nav;
    return _nav != NULL && _nav->count > 0u ? _nav : NULL;
}

RDE_INTERNAL u32 fude_side_nav_count(const fude_ui* _ui) {
    const fude_extension_nav* _nav = fude_side_nav(_ui);
    return _nav == NULL ? 0u : _nav->count < FUDE_EXTENSION_NAV ? _nav->count : FUDE_EXTENSION_NAV;
}

// An entry of the app's navigation (its button's ref: which), pressed: what it
// opens, over the page, and the panel is done — unless it says to stay (Kana's
// Reviews with none due: a notice instead).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_nav(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_side_theme_ref*      _ref   = (const fude_side_theme_ref*)_user_data;
    fude_ui*                        _ui    = _ref->ui;
    const fude_extension_nav_entry* _entry = &fude_side_nav(_ui)->entries[_ref->value];
    if(_entry->press != NULL && !_entry->press(_ui->app, _entry->arg)) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    _ui->side.open = false;
    fude_ui_update(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A theme: everything restyles at once; Settings stays open to compare.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_theme(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_side_theme_ref* _ref = (const fude_side_theme_ref*)_user_data;
    fude_theme_set((FUDE_THEME_)_ref->value);
    fude_ui_apply_theme(_ref->ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A language: read now; the UI is built again in it next frame (it cannot be
// while its own button is being pressed: fude_ui_follow_language).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_language(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_side_theme_ref* _ref = (const fude_side_theme_ref*)_user_data;
    if(_ref->value < FUDE_TEXT_LANGUAGES && FUDE_TEXT_LANGUAGE_LIST[_ref->value].language != fude_text_language()) {
        fude_text_set_language(FUDE_TEXT_LANGUAGE_LIST[_ref->value].language);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_settings(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->side.open          = false;
    _ui->side.settings_open = true;
    fude_side_refresh_settings(_ui);
    fude_ui_update(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_settings_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->side.settings_open = false;
    fude_ui_update(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// --- Your data --------------------------------------------------------------------------

FUDE_SIDE_DATA_ fude_side_take_data_request(fude_ui* _ui) {
    const FUDE_SIDE_DATA_ _r = _ui->side.data_request;
    _ui->side.data_request = FUDE_SIDE_DATA_NONE;
    return _r;
}

void fude_side_data_message(fude_ui* _ui, const c8* _text, b8 _bad) {
    snprintf(_ui->side.data_message, sizeof(_ui->side.data_message), "%s", _text != NULL ? _text : "");
    _ui->side.data_message_bad = _bad;
}

void fude_side_data_done(fude_ui* _ui) {
    fude_side* _side = &_ui->side;
    if(_side->data_backup != NULL) {
        rde_memory_allocator* _a = rde_memory_allocator_get_default_std();
        _a->free(_a->allocator, _side->data_backup);
    }
    _side->data_backup      = NULL;
    _side->data_backup_size = 0;
    _side->data_confirming  = false;
}

void fude_side_data_confirm(fude_ui* _ui, u8* _backup, usize _size, const c8* _question) {
    fude_side_data_done(_ui);
    _ui->side.data_backup      = _backup;
    _ui->side.data_backup_size = _size;
    _ui->side.data_confirming  = true;
    fude_side_data_message(_ui, _question, false);
}

// Rate: the store's page for a review (RDE: the App Store's write-review page
// once the app's store id is set, the rating sheet until then; Play's listing).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_rate(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->side.open = false;
    rde_mobile_open_review_page(_ui->app->info->store_id);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// The tutorial again (the app's: Kana's welcome): the panel closes.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_tutorial(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->side.open = false;
    fude_app_ext(_ui->app)->tutorial(_ui->app);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_data(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->side.data_open = true;
    fude_side_data_message(_ui, "", false);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_data_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->side.data_open = false;
    fude_side_data_done(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_data_export(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    ((fude_ui*)_user_data)->side.data_request = FUDE_SIDE_DATA_EXPORT;
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_data_import(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    ((fude_ui*)_user_data)->side.data_request = FUDE_SIDE_DATA_IMPORT;
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_data_replace(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    if(_ui->side.data_confirming) {
        _ui->side.data_request = FUDE_SIDE_DATA_REPLACE;
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_data_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    fude_side_data_done(_ui);
    fude_side_data_message(_ui, "", false);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_width_even(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->app->ink->width_mode = FUDE_INK_WIDTH_MODE_CONSTANT;
    fude_side_refresh_settings(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_width_pressure(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->app->ink->width_mode = FUDE_INK_WIDTH_MODE_PRESSURE;
    fude_side_refresh_settings(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// The size of lines and squares, on every canvas (the toolbar's Paper chooses
// them for one).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_paper_size(fude_ui* _ui, FUDE_PAPER_SIZE_ _size) {
    _ui->app->canvas->paper_size = _size;
    fude_side_refresh_settings(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_paper_small(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    return fude_side_paper_size((fude_ui*)_user_data, FUDE_PAPER_SMALL);
}
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_paper_medium(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    return fude_side_paper_size((fude_ui*)_user_data, FUDE_PAPER_MEDIUM);
}
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_paper_large(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    return fude_side_paper_size((fude_ui*)_user_data, FUDE_PAPER_LARGE);
}

// --- licences ------------------------------------------------------------------------------

RDE_INTERNAL void fude_side_licence_free(fude_side* _side) {
    free(_side->_licence_text);
    free(_side->_licence_starts);
    _side->_licence_text   = NULL;
    _side->_licence_starts = NULL;
    _side->_licence_count  = 0;
    _side->_licence_first  = -1;
}

// Document _doc read, cut into lines that fit the scroll area's width (on the
// spaces where there are some), and scrolled to its top.
RDE_INTERNAL void fude_side_licence_load(fude_ui* _ui, u32 _doc) {
    fude_side* _side = &_ui->side;
    fude_side_licence_free(_side);
    _side->licences_doc = _doc;

    // The files, one after the other.
    usize _size = 0;
    const fude_app_licence* _licence = &_ui->app->info->licences[_doc];
    for(u32 _f = 0; _f < 3 && _doc < fude_side_licence_count(_ui) && _licence->files[_f] != NULL; _f++) {
        u32 _n = 0;
        u8* _data = fude_file_read(_licence->files[_f], &_n);
        if(_data == NULL) {
            continue;
        }
        _side->_licence_text = (c8*)realloc(_side->_licence_text, _size + _n + 3);
        memcpy(_side->_licence_text + _size, _data, _n);
        _size += _n;
        _side->_licence_text[_size++] = '\n';
        _side->_licence_text[_size++] = '\n';
        fude_file_free(_data);
    }
    if(_side->_licence_text == NULL) {
        const c8* _missing = fude_text(FUDE_TEXT_LICENCE_MISSING);
        _size = strlen(_missing);
        _side->_licence_text = (c8*)malloc(_size + 1);
        memcpy(_side->_licence_text, _missing, _size);
    }
    _side->_licence_text[_size] = 0;
    for(usize _i = 0; _i < _size; _i++) {
        if(_side->_licence_text[_i] == '\t' || _side->_licence_text[_i] == '\r' || _side->_licence_text[_i] == '\f') {
            _side->_licence_text[_i] = ' ';
        }
    }

    // How many characters a line holds, from the width of an average one.
    const f32        _scale = FUDE_SIDE_LICENCE_PX / (f32)FUDE_KIT_FONT_SIZE;
    const c8*        _probe = "The quick brown fox jumps over the lazy dog, THE QUICK BROWN FOX 0123456789.";
    const rde_vec_2F _ruler = _ui->font != NULL ? rde_rich_text_measure(_probe, _ui->font, _scale, 100000.0f, false) : (rde_vec_2F){ 600.0f, 18.0f };
    const f32        _each  = fmaxf(1.0f, _ruler.x / (f32)strlen(_probe));
    const u32        _cols  = (u32)fmaxf(20.0f, floorf((_side->_licence_width - 12.0f) / _each) - 2.0f);
    _side->_licence_line_h  = fmaxf(12.0f, _ruler.y * 1.12f);

    // The lines: at a newline, or at the last space before _cols characters (at
    // _cols when there is none). Characters are counted, not bytes.
    u32 _capacity = 1024;
    _side->_licence_starts = (u32*)malloc(sizeof(u32) * _capacity);
    usize _at = 0;
    while(_at < _size) {
        if(_side->_licence_count + 2 >= _capacity) {
            _capacity *= 2;
            _side->_licence_starts = (u32*)realloc(_side->_licence_starts, sizeof(u32) * _capacity);
        }
        _side->_licence_starts[_side->_licence_count++] = (u32)_at;
        usize _i = _at, _space = 0;
        u32   _chars = 0;
        while(_i < _size && _side->_licence_text[_i] != '\n' && _chars < _cols) {
            if(_side->_licence_text[_i] == ' ') {
                _space = _i;
            }
            _i++;
            while(_i < _size && ((u8)_side->_licence_text[_i] & 0xC0u) == 0x80u) {
                _i++;   // the rest of a UTF-8 character
            }
            _chars++;
        }
        if(_i < _size && _side->_licence_text[_i] == '\n') {
            _at = _i + 1;
        } else if(_i < _size && _space > _at) {
            _at = _space + 1;
        } else {
            _at = _i;
        }
    }
    _side->_licence_starts[_side->_licence_count] = (u32)_size;

    const f32 _content = fmaxf(1.0f, (f32)_side->_licence_count * _side->_licence_line_h + 8.0f);
    rde_ui_scroll_area_set_content_size(_side->licences_text, (rde_vec_2F){ _side->_licence_width, _content });
    rde_ui_scroll_area_set_scroll(_side->licences_text, (rde_vec_2F){ 0.0f, 0.0f });
    for(u32 _d = 0; _d < fude_side_licence_count(_ui); _d++) {
        fude_kit_button_chip(_side->licences_docs[_d], _d == _doc);
    }
}

// The lines on screen get the labels: placed only when the first one changed.
RDE_INTERNAL void fude_side_licence_scroll(fude_ui* _ui) {
    fude_side* _side = &_ui->side;
    if(_side->_licence_text == NULL) {
        return;
    }
    const rde_vec_2F _scroll  = rde_ui_scroll_area_get_scroll(_side->licences_text);
    const i32        _first   = (i32)fmaxf(0.0f, floorf(_scroll.y / _side->_licence_line_h) - 2.0f);
    if(_first == _side->_licence_first) {
        return;
    }
    _side->_licence_first = _first;
    const f32 _content = fmaxf(1.0f, (f32)_side->_licence_count * _side->_licence_line_h + 8.0f);
    c8        _line[1024];
    for(u32 _k = 0; _k < FUDE_SIDE_LICENCE_LINES; _k++) {
        const u32    _n     = (u32)_first + _k;
        rde_ui_node* _label = rde_ui_label_as_node(_side->licences_lines[_k]);
        if(_n >= _side->_licence_count) {
            rde_ui_node_set_active(_label, false);
            continue;
        }
        u32 _from = _side->_licence_starts[_n], _to = _side->_licence_starts[_n + 1];
        while(_to > _from && (_side->_licence_text[_to - 1] == '\n' || _side->_licence_text[_to - 1] == ' ')) {
            _to--;
        }
        const u32 _len = _to - _from < sizeof(_line) - 1 ? _to - _from : (u32)sizeof(_line) - 1;
        memcpy(_line, _side->_licence_text + _from, _len);
        _line[_len] = 0;
        rde_ui_label_set_text(_side->licences_lines[_k], _line);
        rde_ui_node_set_active(_label, true);
        fude_kit_place(_label, (rde_vec_2F){ 6.0f + (_side->_licence_width - 12.0f) * 0.5f, _content - 4.0f - ((f32)_n + 0.5f) * _side->_licence_line_h },
                           (rde_vec_2F){ _side->_licence_width - 12.0f, _side->_licence_line_h });
    }
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_licences(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->side.licences_open = true;
    fude_side_licence_load(_ui, _ui->side.licences_doc);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

void fude_side_open_settings(fude_ui* _ui, i32 _licences) {
    _ui->side.open          = false;
    _ui->side.settings_open = true;
    fude_side_refresh_settings(_ui);
    if(_licences >= 0 && _licences < (i32)fude_side_licence_count(_ui)) {
        _ui->side.licences_open = true;
        fude_side_licence_load(_ui, (u32)_licences);
    }
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_licences_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->side.licences_open = false;
    fude_side_licence_free(&_ui->side);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_licence_doc(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_side_theme_ref* _ref = (const fude_side_theme_ref*)_user_data;
    fude_side_licence_load(_ref->ui, _ref->value);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// --- notes -----------------------------------------------------------------------------

RDE_INTERNAL void fude_side_card(fude_ui* _ui, FUDE_SIDE_CARD_ _mode, u32 _note);

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_note(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_side_note_ref* _ref     = (const fude_side_note_ref*)_user_data;
    fude_ui*             _ui = _ref->ui;
    const fude_note*          _note    = fude_notes_find(_ui->app->notes, _ref->id);
    if(_note == NULL) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    if(_note->kind == FUDE_NOTE_FOLDER) {
        fude_notes_set_expanded(_ui->app->notes, _note->id, !_note->expanded);
    } else {
        fude_notes_open(_ui->app->notes, _note->id);   // kana.c sees it and switches the page
        _ui->side.open = false;
        fude_ui_update(_ui);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_note_dots(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_side_note_ref* _ref = (const fude_side_note_ref*)_user_data;
    fude_side_card(_ref->ui, FUDE_SIDE_CARD_ACTIONS, _ref->id);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// + Canvas: a new one at the top level, opened.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_new_canvas(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    c8 _name[FUDE_NOTE_NAME];
    fude_notes_new_name(_ui->app->notes, FUDE_NOTE_CANVAS, _name, sizeof(_name));
    const u32 _id = fude_notes_add(_ui->app->notes, FUDE_NOTE_CANVAS, 0, _name);
    if(_id != 0) {
        fude_notes_open(_ui->app->notes, _id);
        _ui->side.open = false;
        fude_ui_update(_ui);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// + Folder: a new one, named straight away.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_new_folder(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    c8 _name[FUDE_NOTE_NAME];
    fude_notes_new_name(_ui->app->notes, FUDE_NOTE_FOLDER, _name, sizeof(_name));
    const u32 _id = fude_notes_add(_ui->app->notes, FUDE_NOTE_FOLDER, 0, _name);
    if(_id != 0) {
        fude_side_card(_ui, FUDE_SIDE_CARD_RENAME, _id);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_card_rename(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    fude_side_card(_ui, FUDE_SIDE_CARD_RENAME, _ui->side.card_note);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_card_delete(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    fude_side_card(_ui, FUDE_SIDE_CARD_DELETE, _ui->side.card_note);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A folder's new folder: made in it, and named straight away.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_card_add_folder(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    c8 _name[FUDE_NOTE_NAME];
    fude_notes_new_name(_ui->app->notes, FUDE_NOTE_FOLDER, _name, sizeof(_name));
    const u32 _id = fude_notes_add(_ui->app->notes, FUDE_NOTE_FOLDER, _ui->side.card_note, _name);
    fude_side_card(_ui, _id != 0 ? FUDE_SIDE_CARD_RENAME : FUDE_SIDE_CARD_NONE, _id);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A folder's new canvas: made in it, and opened.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_card_add(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    c8 _name[FUDE_NOTE_NAME];
    fude_notes_new_name(_ui->app->notes, FUDE_NOTE_CANVAS, _name, sizeof(_name));
    const u32 _id = fude_notes_add(_ui->app->notes, FUDE_NOTE_CANVAS, _ui->side.card_note, _name);
    fude_side_card(_ui, FUDE_SIDE_CARD_NONE, 0);
    if(_id != 0) {
        fude_notes_open(_ui->app->notes, _id);
        _ui->side.open = false;
    }
    fude_ui_update(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_card_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_side_card((fude_ui*)_user_data, FUDE_SIDE_CARD_NONE, 0);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Save (rename) or Delete.
RDE_INTERNAL void fude_side_card_confirm(fude_ui* _ui) {
    fude_side* _side = &_ui->side;
    if(_side->card_mode == FUDE_SIDE_CARD_RENAME) {
        const usize _bytes = rde_ui_text_editor_get_byte_count(_side->note_field);
        c8*         _text  = rde_ui_text_editor_get_text(_side->note_field, 0, _bytes);
        fude_notes_rename(_ui->app->notes, _side->card_note, _text != NULL ? _text : "");
        rde_ui_text_editor_free_text(_side->note_field, _text);
    } else if(_side->card_mode == FUDE_SIDE_CARD_DELETE) {
        fude_notes_remove(_ui->app->notes, _side->card_note);   // the open canvas gone: kana.c opens another
    }
    fude_side_card(_ui, FUDE_SIDE_CARD_NONE, 0);
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_card_confirm(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_side_card_confirm((fude_ui*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Return in the name field: save.
RDE_INTERNAL void fude_side_on_field_submit(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    fude_side_card_confirm((fude_ui*)_user_data);
}

// The card's buttons for its mode, in a row from the right.
RDE_INTERNAL void fude_side_card_layout(fude_ui* _ui) {
    fude_side*       _side = &_ui->side;
    const rde_vec_2F _size = FUDE_SIDE_NOTE_CARD;
    const b8         _folder = fude_notes_find(_ui->app->notes, _side->card_note) != NULL &&
                               fude_notes_find(_ui->app->notes, _side->card_note)->kind == FUDE_NOTE_FOLDER;

    // From the right: Cancel at the end of the actions (the confirm first in its row).
    rde_ui_button* _row[6];
    u32            _count = 0;
    if(_side->card_mode == FUDE_SIDE_CARD_ACTIONS) {
        _row[_count++] = _side->note_cancel;
        _row[_count++] = _side->note_delete;
        if(_folder) {
            _row[_count++] = _side->note_add_folder;
            _row[_count++] = _side->note_add;
        }
        _row[_count++] = _side->note_rename;
    } else {
        _row[_count++] = _side->note_confirm;
        _row[_count++] = _side->note_cancel;
    }

    rde_ui_button* const _all[] = { _side->note_rename, _side->note_add, _side->note_add_folder, _side->note_delete, _side->note_cancel, _side->note_confirm };
    for(u32 _i = 0; _i < sizeof(_all) / sizeof(_all[0]); _i++) {
        b8 _used = false;
        for(u32 _k = 0; _k < _count; _k++) {
            _used = _used || _row[_k] == _all[_i];
        }
        rde_ui_node_set_active(rde_ui_button_as_node(_all[_i]), _used);
    }
    // Each as wide as its label; squeezed when the row would not fit.
    f32 _w[6];
    f32 _total = FUDE_SIDE_GAP * (f32)(_count - 1u);
    for(u32 _k = 0; _k < _count; _k++) {
        const c8* _text = _row[_k]->internal_label != NULL ? _row[_k]->internal_label->text : "";
        _w[_k]  = fmaxf(84.0f, fude_draw_text_width(_ui->font, (f32)FUDE_KIT_FONT_SIZE, _text != NULL ? _text : "", 13.0f) + 32.0f);
        _total += _w[_k];
    }
    const f32 _room    = _size.x - 2.0f * FUDE_SIDE_CARD_PAD;
    const f32 _squeeze = _total > _room ? (_room - FUDE_SIDE_GAP * (f32)(_count - 1u)) / (_total - FUDE_SIDE_GAP * (f32)(_count - 1u)) : 1.0f;
    f32 _x = _size.x - FUDE_SIDE_CARD_PAD;
    for(u32 _k = 0; _k < _count; _k++) {
        const f32 _bw = _w[_k] * _squeeze;
        fude_kit_place(rde_ui_button_as_node(_row[_k]), (rde_vec_2F){ _x - _bw * 0.5f, FUDE_SIDE_CARD_PAD + 20.0f }, (rde_vec_2F){ _bw, 40.0f });
        _x -= _bw + FUDE_SIDE_GAP;
    }
    rde_ui_node_set_active(fude_kit_field_node(_side->note_field), _side->card_mode == FUDE_SIDE_CARD_RENAME);
    rde_ui_node_set_active(rde_ui_label_as_node(_side->note_body), _side->card_mode != FUDE_SIDE_CARD_RENAME);
}

// Opens the note card on a note in a mode (NONE closes it).
RDE_INTERNAL void fude_side_card(fude_ui* _ui, FUDE_SIDE_CARD_ _mode, u32 _note) {
    fude_side*       _side = &_ui->side;
    const fude_note* _n    = fude_notes_find(_ui->app->notes, _note);
    if(_n == NULL) {
        _mode = FUDE_SIDE_CARD_NONE;
    }
    _side->card_mode = (u8)_mode;
    _side->card_note = _note;
    if(_mode == FUDE_SIDE_CARD_NONE) {
        return;
    }

    const b8 _folder = _n->kind == FUDE_NOTE_FOLDER;
    c8       _title[FUDE_NOTE_NAME + 32];
    c8       _body[160] = "";
    if(_mode == FUDE_SIDE_CARD_ACTIONS) {
        snprintf(_title, sizeof(_title), "%s", _n->name);
        if(_folder) {
            const u32 _in = fude_notes_count_in(_ui->app->notes, _n->id);
            FUDE_TEXTF(_body, FUDE_TEXT_NOTE_FOLDER_BODY, FUDE_TN(_in));
        } else {
            snprintf(_body, sizeof(_body), "%s", fude_text(FUDE_TEXT_CANVAS));
        }
    } else if(_mode == FUDE_SIDE_CARD_RENAME) {
        snprintf(_title, sizeof(_title), "%s", fude_text(_folder ? FUDE_TEXT_NOTE_RENAME_FOLDER : FUDE_TEXT_NOTE_RENAME_CANVAS));
        const usize _bytes = rde_ui_text_editor_get_byte_count(_side->note_field);
        if(_bytes > 0) {
            rde_ui_text_editor_delete_range(_side->note_field, 0, _bytes);
        }
        rde_ui_text_editor_insert_at(_side->note_field, 0, _n->name, strlen(_n->name));
        rde_ui_text_editor_select_all(_side->note_field);
    } else {
        FUDE_TEXTF(_title, FUDE_TEXT_NOTE_DELETE_TITLE, FUDE_TS(_n->name));
        const u32 _in = _folder ? fude_notes_count_in(_ui->app->notes, _n->id) : 0u;
        if(_folder && _in > 0) {
            FUDE_TEXTF(_body, FUDE_TEXT_NOTE_DELETE_FOLDER, FUDE_TN(_in));
        } else {
            snprintf(_body, sizeof(_body), "%s", fude_text(FUDE_TEXT_NOTE_UNDONE));
        }
    }
    rde_ui_label_set_text(_side->note_title, _title);
    rde_ui_label_set_text(_side->note_body, _body);
    c8 _confirm[64];
    snprintf(_confirm, sizeof(_confirm), "%s  %s", _mode == FUDE_SIDE_CARD_DELETE ? FUDE_ICON_TRASH : FUDE_ICON_CHECK,
             fude_text(_mode == FUDE_SIDE_CARD_DELETE ? FUDE_TEXT_DELETE : FUDE_TEXT_SAVE));
    rde_ui_button_set_text(_side->note_confirm, _confirm);
    if(_mode == FUDE_SIDE_CARD_DELETE) {
        fude_kit_button_danger(_side->note_confirm);
    } else {
        fude_kit_button_primary(_side->note_confirm);
    }
    fude_side_card_layout(_ui);

    if(_mode == FUDE_SIDE_CARD_RENAME) {
        rde_ui_node_focus(rde_ui_text_editor_as_node(_side->note_field));   // the keyboard comes up
    }
}

// --- dragging a row ------------------------------------------------------------------

RDE_INTERNAL void fude_side_hide_drop(fude_side* _side) {
    rde_ui_node_set_active(rde_ui_image_as_node(_side->drop_line), false);
    rde_ui_node_set_active(rde_ui_image_as_node(_side->drop_box), false);
}

// Where the dragged row would land with the pointer at _side->drag_at, shown by
// the line (between rows, indented to the level it lands at) or the box (into a
// folder: the middle of its row). A folder never lands inside itself.
RDE_INTERNAL void fude_side_drop_target(fude_ui* _ui) {
    fude_side*       _side  = &_ui->side;
    const fude_note* _moving = fude_notes_find(_ui->app->notes, _side->drag_id);
    const f32        _pitch = FUDE_SIDE_NOTE_H + FUDE_SIDE_GAP;
    const f32        _top   = _side->_list_bl.y + _side->_list_size.y;
    const f32        _scroll = rde_ui_scroll_area_get_scroll(_side->notes_list).y;

    _side->drop_valid = false;
    fude_side_hide_drop(_side);
    if(_moving == NULL || _side->_row_count == 0 ||
       _side->drag_at.x < _side->_list_bl.x - 20.0f || _side->drag_at.x > _side->_list_bl.x + _side->_list_size.x + 60.0f) {
        return;
    }

    const f32 _cy = rde_math_clamp_f32(_top - _side->drag_at.y, 0.0f, _side->_list_size.y) + _scroll;   // from the content's top
    i32       _r  = (i32)floorf(_cy / _pitch);
    f32       _t  = (_cy - (f32)_r * _pitch) / FUDE_SIDE_NOTE_H;   // down the row: 0 top, 1 bottom
    b8        _after_all = false;
    if(_r >= (i32)_side->_row_count) {
        _r         = (i32)_side->_row_count - 1;
        _after_all = true;
    }
    const fude_side_row* _row = &_side->_rows[_r];

    b8  _into  = false;
    u32 _depth = 0;
    u32 _line  = (u32)_r;   // the line sits at the top of this row (or past the last)
    if(!_after_all && _row->kind == FUDE_NOTE_FOLDER && _t > 0.28f && _t < 0.72f) {
        _into               = true;
        _side->drop_parent  = _row->id;
        _side->drop_before  = 0;
    } else if(!_after_all && _t <= 0.5f) {
        _side->drop_parent  = _row->parent;
        _side->drop_before  = _row->id;
        _depth              = _row->depth;
    } else {
        _line = (u32)_r + 1u;
        if(_line < _side->_row_count && _side->_rows[_line].depth > _row->depth) {
            // After an open folder: first inside it.
            _side->drop_parent = _row->id;
            _side->drop_before = _side->_rows[_line].id;
            _depth             = _side->_rows[_line].depth;
        } else {
            // After the row, in its folder: before its next sibling, or at the end.
            _side->drop_parent = _row->parent;
            _side->drop_before = 0;
            for(u32 _k = _line; _k < _side->_row_count; _k++) {
                if(_side->_rows[_k].depth < _row->depth) {
                    break;
                }
                if(_side->_rows[_k].parent == _row->parent) {
                    _side->drop_before = _side->_rows[_k].id;
                    break;
                }
            }
            _depth = _row->depth;
        }
    }

    // A folder cannot go into itself or anything inside it; a note dropped on its
    // own place goes nowhere.
    if(_moving->kind == FUDE_NOTE_FOLDER && _side->drop_parent != 0 && fude_notes_is_within(_ui->app->notes, _side->drop_parent, _moving->id)) {
        return;
    }
    if(_side->drop_before == _moving->id) {
        return;
    }
    _side->drop_valid = true;

    if(_into) {
        const f32 _indent = (f32)_row->depth * FUDE_SIDE_INDENT;
        const f32 _y      = _top - ((f32)_r * _pitch - _scroll) - FUDE_SIDE_NOTE_H * 0.5f;
        fude_kit_place(rde_ui_image_as_node(_side->drop_box), (rde_vec_2F){ _side->_list_bl.x + (_indent + _side->_list_size.x) * 0.5f, _y },
                           (rde_vec_2F){ _side->_list_size.x - _indent + 4.0f, FUDE_SIDE_NOTE_H + 4.0f });
        rde_ui_node_set_active(rde_ui_image_as_node(_side->drop_box), true);
    } else {
        const f32 _indent = (f32)_depth * FUDE_SIDE_INDENT;
        const f32 _y      = _top - ((f32)_line * _pitch - _scroll) + FUDE_SIDE_GAP * 0.5f;
        if(_y > _top + FUDE_SIDE_GAP || _y < _side->_list_bl.y - FUDE_SIDE_GAP) {
            return;   // valid, but scrolled out of sight
        }
        fude_kit_place(rde_ui_image_as_node(_side->drop_line), (rde_vec_2F){ _side->_list_bl.x + (_indent + _side->_list_size.x) * 0.5f, _y },
                           (rde_vec_2F){ _side->_list_size.x - _indent, 3.0f });
        rde_ui_node_set_active(rde_ui_image_as_node(_side->drop_line), true);
    }
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_drag_begin(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node);
    const fude_side_note_ref* _ref  = (const fude_side_note_ref*)_user_data;
    fude_side*                _side = &_ref->ui->side;
    const fude_note*          _note = fude_notes_find(_ref->ui->app->notes, _ref->id);
    if(_note == NULL) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    _side->drag_id = _ref->id;
    _side->drag_at = _info->position;
    rde_ui_label_set_text(_side->drag_ghost_label, _note->name);
    rde_ui_node_set_active(rde_ui_image_as_node(_side->drag_ghost), true);
    fude_kit_place(rde_ui_image_as_node(_side->drag_ghost), (rde_vec_2F){ _info->position.x + 90.0f, _info->position.y }, (rde_vec_2F){ 170.0f, 36.0f });
    fude_side_drop_target(_ref->ui);
    return RDE_UI_EVENT_RESULT_CONSUME;   // the handle's: the list does not scroll with it
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_drag_move(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node);
    const fude_side_note_ref* _ref  = (const fude_side_note_ref*)_user_data;
    fude_side*                _side = &_ref->ui->side;
    if(_side->drag_id == 0) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    _side->drag_at = _info->position;
    fude_kit_place(rde_ui_image_as_node(_side->drag_ghost), (rde_vec_2F){ _info->position.x + 90.0f, _info->position.y }, (rde_vec_2F){ 170.0f, 36.0f });
    fude_side_drop_target(_ref->ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_drag_end(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node);
    const fude_side_note_ref* _ref     = (const fude_side_note_ref*)_user_data;
    fude_ui*             _ui = _ref->ui;
    fude_side*                _side    = &_ui->side;
    if(_side->drag_id == 0) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    _side->drag_at = _info->position;
    fude_side_drop_target(_ui);
    if(_side->drop_valid) {
        fude_notes_move(_ui->app->notes, _side->drag_id, _side->drop_parent, _side->drop_before);   // the list rebuilds next frame
    }
    _side->drag_id = 0;
    fude_side_hide_drop(_side);
    rde_ui_node_set_active(rde_ui_image_as_node(_side->drag_ghost), false);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// --- the list ---------------------------------------------------------------------------

// The rows in order, depth first: a folder, then (when open) what is in it.
RDE_INTERNAL void fude_side_collect_rows(const fude_notes* _notes, u32 _parent, u8 _depth, fude_side_row* _out, u32* _count) {
    const fude_note* _all = (const fude_note*)_notes->notes.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_notes->notes); _i++) {
        if(_all[_i].parent != _parent) {
            continue;
        }
        _out[(*_count)++] = (fude_side_row){ _all[_i].id, _all[_i].parent, _depth, _all[_i].kind, _all[_i].expanded };
        if(_all[_i].kind == FUDE_NOTE_FOLDER && _all[_i].expanded && _depth < 32u) {
            fude_side_collect_rows(_notes, _all[_i].id, (u8)(_depth + 1u), _out, _count);
        }
    }
}

// The list, built again: each row a drag handle, the name (a folder's with +/– and
// how many canvases are in it), and "…". The open canvas is marked.
RDE_INTERNAL void fude_side_build_notes(fude_ui* _ui) {
    fude_side*        _side  = &_ui->side;
    const fude_notes* _notes = _ui->app->notes;
    const u32         _count = (u32)rde_arr_length(&_notes->notes);

    rde_ui_scroll_area_clear_contents(_side->notes_list);
    free(_side->_note_refs);
    free(_side->_rows);
    _side->_note_refs      = (fude_side_note_ref*)calloc(_count > 0 ? _count : 1u, sizeof(fude_side_note_ref));
    _side->_rows           = (fude_side_row*)calloc(_count > 0 ? _count : 1u, sizeof(fude_side_row));
    _side->_row_count      = 0;
    _side->_notes_revision = _notes->revision;
    _side->_notes_open     = _notes->open;
    _side->_notes_built    = true;
    fude_side_collect_rows(_notes, 0, 0, _side->_rows, &_side->_row_count);

    const f32 _width   = _side->_list_width;
    const f32 _content = fmaxf(1.0f, (f32)_side->_row_count * (FUDE_SIDE_NOTE_H + FUDE_SIDE_GAP));
    rde_ui_scroll_area_set_content_size(_side->notes_list, (rde_vec_2F){ _width, _content });
    rde_ui_node* _list = rde_ui_scroll_area_as_node(_side->notes_list);
    const fude_theme* _t = fude_theme_active();

    for(u32 _r = 0; _r < _side->_row_count; _r++) {
        const fude_side_row* _row = &_side->_rows[_r];
        const fude_note*     _n   = fude_notes_find(_notes, _row->id);
        const b8             _folder = _row->kind == FUDE_NOTE_FOLDER;
        const f32            _indent = (f32)_row->depth * FUDE_SIDE_INDENT;
        const f32            _y      = _content - (f32)_r * (FUDE_SIDE_NOTE_H + FUDE_SIDE_GAP) - FUDE_SIDE_NOTE_H * 0.5f;

        _side->_note_refs[_r] = (fude_side_note_ref){ _ui, _row->id };
        fude_side_note_ref* _ref = &_side->_note_refs[_r];

        // The handle: three bars; a drag from it moves the row.
        rde_ui_image* _handle = rde_ui_image_create(NULL);
        rde_ui_node*  _h      = rde_ui_image_as_node(_handle);
        rde_ui_image_set_style(_handle, RDE_UI_STATE_NORMAL, fude_kit_style((rde_color){ 0, 0, 0, 0 }, 8.0f));
        rde_ui_node_set_blocks_input(_h, true);
        rde_ui_node_set_user_data(_h, _ref);
        rde_ui_node_set_callback(_h, RDE_UI_EVENT_MOUSE_DRAG_BEGIN, fude_side_on_drag_begin);
        rde_ui_node_set_callback(_h, RDE_UI_EVENT_MOUSE_DRAG_MOVE,  fude_side_on_drag_move);
        rde_ui_node_set_callback(_h, RDE_UI_EVENT_MOUSE_DRAG_END,   fude_side_on_drag_end);
        rde_ui_node_add_child(_list, _h);
        fude_kit_place(_h, (rde_vec_2F){ _indent + FUDE_SIDE_HANDLE_W * 0.5f, _y }, (rde_vec_2F){ FUDE_SIDE_HANDLE_W, FUDE_SIDE_NOTE_H });
        rde_ui_label* _dots6 = fude_side_label(_ui, _h, FUDE_ICON_GRIP_V, 14.0f);
        rde_ui_label_set_font(_dots6, _ui->font_icons != NULL ? _ui->font_icons : _ui->font);
        rde_ui_label_set_alignment(_dots6, RDE_UI_LABEL_H_ALIGN_CENTER, RDE_UI_LABEL_V_ALIGN_MIDDLE);
        rde_ui_label_set_color(_dots6, _t->grip);
        const f32 _back = fude_kit_icon_bearing(FUDE_ICON_GRIP_V) * 14.0f * FUDE_KIT_EM;   // centred
        fude_kit_place(rde_ui_label_as_node(_dots6), (rde_vec_2F){ FUDE_SIDE_HANDLE_W * 0.5f - _back, FUDE_SIDE_NOTE_H * 0.5f }, (rde_vec_2F){ FUDE_SIDE_HANDLE_W, FUDE_SIDE_NOTE_H });

        c8 _label[FUDE_NOTE_NAME + 32];
        if(_folder) {
            snprintf(_label, sizeof(_label), "%s  (%u)", _n->name, fude_notes_count_in(_notes, _n->id));
        } else {
            snprintf(_label, sizeof(_label), "%s", _n->name);
        }

        const f32        _x0       = _indent + FUDE_SIDE_HANDLE_W + FUDE_SIDE_GAP;
        const rde_vec_2F _row_size = { _width - _x0 - FUDE_SIDE_DOTS_W - FUDE_SIDE_GAP, FUDE_SIDE_NOTE_H };
        rde_ui_button*   _button   = fude_kit_button(_list, _label, fude_side_on_note, _ui);
        rde_ui_button_set_on_click(_button, fude_side_on_note, _ref);
        fude_kit_place(rde_ui_button_as_node(_button), (rde_vec_2F){ _x0 + _row_size.x * 0.5f, _y }, _row_size);
        fude_kit_icon(_button, _folder ? (_row->expanded ? FUDE_ICON_FOLDER_OPEN : FUDE_ICON_FOLDER) : FUDE_ICON_NOTE, FUDE_KIT_ICON_LEFT, 15.0f);
        if(!_folder && _row->id == _notes->open) {
            fude_kit_button_selected(_button);   // the canvas on the page
        } else {
            fude_kit_button_quiet(_button);
            fude_kit_icon_color(_button, _t->text_soft);
        }

        rde_ui_button* _dots = fude_kit_button(_list, "\xE2\x80\xA6", fude_side_on_note_dots, _ui);   // …
        rde_ui_button_set_on_click(_dots, fude_side_on_note_dots, _ref);
        fude_kit_icon(_dots, FUDE_ICON_MORE, FUDE_KIT_ICON_ONLY, 17.0f);
        fude_kit_button_quiet(_dots);
        fude_kit_place(rde_ui_button_as_node(_dots), (rde_vec_2F){ _width - FUDE_SIDE_DOTS_W * 0.5f, _y }, (rde_vec_2F){ FUDE_SIDE_DOTS_W, FUDE_SIDE_NOTE_H });
    }
}

// --- layout ---------------------------------------------------------------------------

RDE_INTERNAL void fude_side_layout(fude_ui* _ui) {
    fude_side*       _side   = &_ui->side;
    const rde_vec_2F _screen = fude_kit_screen_size((_ui)->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_ui->window);   // left, top, right, bottom
    _side->_laid_out        = _screen;
    _side->_laid_out_insets = _insets;

    // The menu button: top-left, in the safe area.
    _side->_menu_size   = (rde_vec_2F){ FUDE_SIDE_MENU_W, FUDE_SIDE_MENU_H };
    _side->_menu_center = (rde_vec_2F){ (f32)_insets.x + FUDE_SIDE_MARGIN + FUDE_SIDE_MENU_W * 0.5f,
                                        _screen.y - (f32)_insets.y - FUDE_SIDE_MARGIN * 0.5f - FUDE_SIDE_MENU_H * 0.5f };
    fude_kit_place(rde_ui_button_as_node(_side->menu_button), _side->_menu_center, _side->_menu_size);

    // The backdrops cover the screen; the panel is the left edge, full height.
    const f32 _w = fminf(FUDE_SIDE_WIDTH + (f32)_insets.x, _screen.x * 0.85f);
    fude_kit_modal_place(&_side->panel, _ui->window, (rde_vec_2F){ _w * 0.5f, _screen.y * 0.5f }, (rde_vec_2F){ _w, _screen.y });

    // The panel's contents, top down (panel-local), below the menu button.
    const f32 _x0 = (f32)_insets.x + FUDE_SIDE_MARGIN;
    const f32 _cw = _w - _x0 - FUDE_SIDE_MARGIN;
    f32       _y  = _screen.y - (f32)_insets.y - FUDE_SIDE_MARGIN - FUDE_SIDE_MENU_H - FUDE_SIDE_MARGIN;

    #define FUDE_SIDE_HEADER(_label) do { \
        fude_kit_place(rde_ui_label_as_node(_label), (rde_vec_2F){ _x0 + _cw * 0.5f, _y - FUDE_SIDE_HEADER_H * 0.5f }, (rde_vec_2F){ _cw, FUDE_SIDE_HEADER_H }); \
        _y -= FUDE_SIDE_HEADER_H; \
    } while(0)
    #define FUDE_SIDE_ROW(_button) do { \
        fude_kit_place(rde_ui_button_as_node(_button), (rde_vec_2F){ _x0 + _cw * 0.5f, _y - FUDE_SIDE_ROW_H * 0.5f }, (rde_vec_2F){ _cw, FUDE_SIDE_ROW_H }); \
        _y -= FUDE_SIDE_ROW_H + FUDE_SIDE_GAP; \
    } while(0)

    if(fude_side_nav_count(_ui) > 0u) {
        FUDE_SIDE_HEADER(_side->nav_label);
        for(u32 _i = 0; _i < fude_side_nav_count(_ui); _i++) {
            FUDE_SIDE_ROW(_side->nav[_i]);
        }
        _y -= FUDE_SIDE_MARGIN * 0.5f;
    }

    // Notes: the header with + Folder / + Canvas at its right, then the list down
    // to the bottom row.
    fude_kit_place(rde_ui_label_as_node(_side->notes_label), (rde_vec_2F){ _x0 + (_cw - 200.0f) * 0.5f, _y - 18.0f }, (rde_vec_2F){ _cw - 200.0f, 36.0f });
    fude_kit_place(rde_ui_button_as_node(_side->new_canvas), (rde_vec_2F){ _x0 + _cw - 48.0f, _y - 18.0f }, (rde_vec_2F){ 96.0f, 36.0f });
    fude_kit_place(rde_ui_button_as_node(_side->new_folder), (rde_vec_2F){ _x0 + _cw - 96.0f - FUDE_SIDE_GAP - 48.0f, _y - 18.0f }, (rde_vec_2F){ 96.0f, 36.0f });
    _y -= 36.0f + FUDE_SIDE_GAP;

    #undef FUDE_SIDE_ROW
    #undef FUDE_SIDE_HEADER

    // The bottom: the version on the left, Settings on the right; the tutorial
    // (the app's, if it has one) over them, and Rate over that where there is a store.
    const f32 _by  = (f32)_insets.w + FUDE_SIDE_MARGIN + 20.0f;
    f32       _top = _by;   // the highest of them
    if(_side->tutorial != NULL) {
        _top += 20.0f + FUDE_SIDE_GAP + 20.0f;
        fude_kit_place(rde_ui_button_as_node(_side->tutorial), (rde_vec_2F){ _x0 + _cw - 60.0f, _top }, (rde_vec_2F){ 120.0f, 40.0f });
    }
#if defined(RDE_PLATFORM_MOBILE)
    _top += 20.0f + FUDE_SIDE_GAP + 20.0f;
    fude_kit_place(rde_ui_button_as_node(_side->rate), (rde_vec_2F){ _x0 + _cw - 60.0f, _top }, (rde_vec_2F){ 120.0f, 40.0f });
#endif
    const f32 _list_bottom = _top + 20.0f + FUDE_SIDE_MARGIN;
    const f32 _list_h      = fmaxf(FUDE_SIDE_NOTE_H, _y - _list_bottom);
    fude_kit_place(rde_ui_scroll_area_as_node(_side->notes_list), (rde_vec_2F){ _x0 + _cw * 0.5f, _list_bottom + _list_h * 0.5f }, (rde_vec_2F){ _cw, _list_h });
    _side->_list_bl   = (rde_vec_2F){ _x0, _list_bottom };
    _side->_list_size = (rde_vec_2F){ _cw, _list_h };
    _side->_list_width  = _cw;
    _side->_notes_built = false;   // rows are laid out for the list's width
    fude_kit_place(rde_ui_button_as_node(_side->settings_button), (rde_vec_2F){ _x0 + _cw - 60.0f, _by }, (rde_vec_2F){ 120.0f, 40.0f });
    fude_kit_place(rde_ui_label_as_node(_side->version), (rde_vec_2F){ _x0 + (_cw - 130.0f) * 0.5f, _by }, (rde_vec_2F){ _cw - 130.0f, 40.0f });

    // Settings: a card in the middle.
    const f32 _kw = fminf(FUDE_SIDE_CARD_W, _screen.x - 2.0f * FUDE_SIDE_MARGIN);
    const f32 _kh = fminf(FUDE_SIDE_CARD_H, _screen.y - (f32)(_insets.y + _insets.w) - 2.0f * FUDE_SIDE_MARGIN);
    const f32 _m  = 20.0f;
    const f32 _lw = _kw - 2.0f * _m;
    fude_kit_modal_place(&_side->settings, _ui->window, (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.5f }, (rde_vec_2F){ _kw, _kh });
    fude_kit_place(rde_ui_label_as_node(_side->settings_title), (rde_vec_2F){ _m + _lw * 0.5f, _kh - 36.0f }, (rde_vec_2F){ _lw, 44.0f });

    // The theme: a header, then one button per theme across the card.
    f32 _ry = _kh - 84.0f;
    fude_kit_place(rde_ui_label_as_node(_side->theme_label), (rde_vec_2F){ _m + _lw * 0.5f, _ry }, (rde_vec_2F){ _lw, 30.0f });
    _ry -= 42.0f;
    const f32 _tw = (_lw - FUDE_SIDE_GAP * (f32)(FUDE_THEME_COUNT - 1u)) / (f32)FUDE_THEME_COUNT;
    for(u32 _i = 0; _i < FUDE_THEME_COUNT; _i++) {
        fude_kit_place(rde_ui_button_as_node(_side->themes[_i]), (rde_vec_2F){ _m + (f32)_i * (_tw + FUDE_SIDE_GAP) + _tw * 0.5f, _ry },
                           (rde_vec_2F){ _tw, 40.0f });
    }
    // The language: a header, then a flag and its name for each.
    _ry -= 50.0f;
    fude_kit_place(rde_ui_label_as_node(_side->language_label), (rde_vec_2F){ _m + _lw * 0.5f, _ry }, (rde_vec_2F){ _lw, 30.0f });
    _ry -= 48.0f;
    const f32 _gw = (_lw - FUDE_SIDE_GAP * (f32)(FUDE_TEXT_LANGUAGES - 1u)) / (f32)FUDE_TEXT_LANGUAGES;
    for(u32 _i = 0; _i < FUDE_TEXT_LANGUAGES; _i++) {
        fude_kit_place(rde_ui_button_as_node(_side->languages[_i]), (rde_vec_2F){ _m + (f32)_i * (_gw + FUDE_SIDE_GAP) + _gw * 0.5f, _ry },
                           (rde_vec_2F){ _gw, 62.0f });
    }
    _ry -= 66.0f;
    fude_kit_place(rde_ui_label_as_node(_side->width_label), (rde_vec_2F){ _m + _lw * 0.25f, _ry }, (rde_vec_2F){ _lw * 0.5f, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_side->width_even), (rde_vec_2F){ _kw - _m - 186.0f, _ry }, (rde_vec_2F){ 120.0f, 40.0f });
    fude_kit_place(rde_ui_button_as_node(_side->width_pressure), (rde_vec_2F){ _kw - _m - 60.0f, _ry }, (rde_vec_2F){ 120.0f, 40.0f });
    _ry -= 54.0f;
    fude_kit_place(rde_ui_label_as_node(_side->paper_label), (rde_vec_2F){ _m + (_lw - 318.0f) * 0.5f, _ry }, (rde_vec_2F){ _lw - 318.0f, 44.0f });
    for(u32 _i = 0; _i < FUDE_PAPER_SIZE_COUNT; _i++) {
        const f32 _from_right = (f32)(FUDE_PAPER_SIZE_COUNT - 1u - _i) * (100.0f + FUDE_SIDE_GAP);
        fude_kit_place(rde_ui_button_as_node(_side->paper_sizes[_i]), (rde_vec_2F){ _kw - _m - 50.0f - _from_right, _ry }, (rde_vec_2F){ 100.0f, 40.0f });
    }
    _ry -= 56.0f;
    // The app's sections (Kana's Handwriting), then About.
    for(u32 _i = 0; _i < fude_side_section_count(_ui); _i++) {
        _ry = fude_app_ext(_ui->app)->sections[_i].layout(_ui, _ry, _m, _kw);
    }
    fude_kit_place(rde_ui_label_as_node(_side->about_label), (rde_vec_2F){ _m + _lw * 0.5f, _ry }, (rde_vec_2F){ _lw, 34.0f });

    const f32 _text_top    = _ry - 20.0f;
    const f32 _text_bottom = _m + 44.0f + 12.0f;
    fude_kit_place(rde_ui_label_as_node(_side->about_text), (rde_vec_2F){ _m + _lw * 0.5f, (_text_top + _text_bottom) * 0.5f },
                       (rde_vec_2F){ _lw, fmaxf(20.0f, _text_top - _text_bottom) });
    fude_kit_place(rde_ui_button_as_node(_side->settings_close), (rde_vec_2F){ _kw - _m - 60.0f, _m + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_side->licences_button), (rde_vec_2F){ _kw - _m - 186.0f, _m + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_side->data_button), (rde_vec_2F){ _kw - _m - 312.0f, _m + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });

    // Your data: the same card's size, over Settings — the title, what Kana
    // does with your data, then the answer line, Export / Import (or Replace /
    // Cancel) and Close at the bottom.
    fude_kit_modal_place(&_side->data, _ui->window, (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.5f }, (rde_vec_2F){ _kw, _kh });
    fude_kit_place(rde_ui_label_as_node(_side->data_title), (rde_vec_2F){ _m + _lw * 0.5f, _kh - 36.0f }, (rde_vec_2F){ _lw, 44.0f });
    {
        const f32 _close_y   = _m + 22.0f;
        const f32 _status_lo = _close_y + 22.0f + 12.0f;
        const f32 _status_hi = _status_lo + 84.0f;
        const f32 _row_y     = _status_hi + 12.0f + 22.0f;
        const f32 _text_lo   = _row_y + 22.0f + 18.0f;
        const f32 _text_hi   = _kh - 66.0f;
        fude_kit_place(rde_ui_label_as_node(_side->data_text), (rde_vec_2F){ _m + _lw * 0.5f, (_text_lo + _text_hi) * 0.5f },
                           (rde_vec_2F){ _lw, fmaxf(20.0f, _text_hi - _text_lo) });
        const f32 _bw = fminf(200.0f, (_lw - FUDE_SIDE_GAP) * 0.5f);
        fude_kit_place(rde_ui_button_as_node(_side->data_export), (rde_vec_2F){ _m + _bw * 0.5f, _row_y }, (rde_vec_2F){ _bw, 44.0f });
        fude_kit_place(rde_ui_button_as_node(_side->data_import), (rde_vec_2F){ _m + _bw * 1.5f + FUDE_SIDE_GAP, _row_y }, (rde_vec_2F){ _bw, 44.0f });
        fude_kit_place(rde_ui_button_as_node(_side->data_replace), (rde_vec_2F){ _m + _bw * 0.5f, _row_y }, (rde_vec_2F){ _bw, 44.0f });
        fude_kit_place(rde_ui_button_as_node(_side->data_cancel), (rde_vec_2F){ _m + _bw * 1.5f + FUDE_SIDE_GAP, _row_y }, (rde_vec_2F){ _bw, 44.0f });
        fude_kit_place(rde_ui_label_as_node(_side->data_status), (rde_vec_2F){ _m + _lw * 0.5f, (_status_lo + _status_hi) * 0.5f },
                           (rde_vec_2F){ _lw, _status_hi - _status_lo });
        fude_kit_place(rde_ui_button_as_node(_side->data_close), (rde_vec_2F){ _kw - _m - 60.0f, _close_y }, (rde_vec_2F){ 120.0f, 44.0f });
    }

    // Licences: the same card's size, over Settings: the title, the documents,
    // the text scrolling between them and Close.
    fude_kit_modal_place(&_side->licences, _ui->window, (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.5f }, (rde_vec_2F){ _kw, _kh });
    fude_kit_place(rde_ui_label_as_node(_side->licences_title), (rde_vec_2F){ _m + _lw * 0.5f, _kh - 36.0f }, (rde_vec_2F){ _lw, 44.0f });
    const u32 _docs = fude_side_licence_count(_ui) > 0u ? fude_side_licence_count(_ui) : 1u;
    const f32 _dw   = (_lw - FUDE_SIDE_GAP * (f32)(_docs - 1u)) / (f32)_docs;
    for(u32 _i = 0; _i < fude_side_licence_count(_ui); _i++) {
        fude_kit_place(rde_ui_button_as_node(_side->licences_docs[_i]), (rde_vec_2F){ _m + (f32)_i * (_dw + FUDE_SIDE_GAP) + _dw * 0.5f, _kh - 88.0f },
                           (rde_vec_2F){ _dw, 40.0f });
    }
    const f32 _lt_top    = _kh - 116.0f;
    const f32 _lt_bottom = _m + 44.0f + 12.0f;
    _side->_licence_width = _lw;
    fude_kit_place(rde_ui_scroll_area_as_node(_side->licences_text), (rde_vec_2F){ _m + _lw * 0.5f, (_lt_top + _lt_bottom) * 0.5f },
                       (rde_vec_2F){ _lw, fmaxf(40.0f, _lt_top - _lt_bottom) });
    fude_kit_place(rde_ui_button_as_node(_side->licences_close), (rde_vec_2F){ _kw - _m - 60.0f, _m + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
    if(_side->licences_open) {
        fude_side_licence_load(_ui, _side->licences_doc);   // new width: the lines again
    }

    // The note card: a title, a line (or the name field), buttons at the bottom.
    const rde_vec_2F _nc = FUDE_SIDE_NOTE_CARD;
    fude_kit_modal_place(&_side->note, _ui->window, (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.6f }, _nc);
    fude_kit_place(rde_ui_label_as_node(_side->note_title), (rde_vec_2F){ _nc.x * 0.5f, _nc.y - FUDE_SIDE_CARD_PAD - 14.0f }, (rde_vec_2F){ _nc.x - 2.0f * FUDE_SIDE_CARD_PAD, 32.0f });
    fude_kit_place(rde_ui_label_as_node(_side->note_body), (rde_vec_2F){ _nc.x * 0.5f, _nc.y - FUDE_SIDE_CARD_PAD - 54.0f }, (rde_vec_2F){ _nc.x - 2.0f * FUDE_SIDE_CARD_PAD, 44.0f });
    fude_kit_place(fude_kit_field_node(_side->note_field), (rde_vec_2F){ _nc.x * 0.5f, _nc.y - FUDE_SIDE_CARD_PAD - 62.0f }, (rde_vec_2F){ _nc.x - 2.0f * FUDE_SIDE_CARD_PAD, 44.0f });
    if(_side->card_mode != FUDE_SIDE_CARD_NONE) {
        fude_side_card_layout(_ui);
    }
}

// --- lifetime ---------------------------------------------------------------------------

// The flags, loaded once (they outlive a rebuilt UI).
RDE_INTERNAL rde_texture* fude_side_flags[FUDE_TEXT_LANGUAGES];

// A language's button: its flag at the top, its name under it.
RDE_INTERNAL void fude_side_flag(rde_ui_button* _button, u32 _language) {
    if(fude_side_flags[_language] == NULL) {
        fude_side_flags[_language] = rde_texture_load(FUDE_TEXT_LANGUAGE_LIST[_language].flag, NULL);
    }
    rde_ui_image* _flag = rde_ui_image_create(NULL);
    rde_ui_node*  _n    = rde_ui_image_as_node(_flag);
    rde_ui_node_set_raycast_target(_n, false);
    rde_ui_node_set_interactable(_n, false);
    rde_ui_style _s = rde_ui_style_default();
    _s.texture      = fude_side_flags[_language];
    rde_ui_image_set_style(_flag, RDE_UI_STATE_NORMAL, _s);
    rde_ui_node_add_child(rde_ui_button_as_node(_button), _n);
    // 3:2, centred near the top (anchored to the top middle).
    rde_ui_node_set_anchors(_n, (rde_vec_2F){ 0.5f, 1.0f }, (rde_vec_2F){ 0.5f, 1.0f });
    rde_ui_node_set_pivot(_n, (rde_vec_2F){ 0.5f, 0.5f });
    rde_ui_node_set_offsets(_n, (rde_vec_2F){ -15.0f, -8.0f - 20.0f }, (rde_vec_2F){ 15.0f, -8.0f });
    if(_button->internal_label != NULL) {
        rde_ui_node* _t = rde_ui_label_as_node(_button->internal_label);
        rde_ui_node_set_anchors(_t, (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 1.0f, 0.0f });
        rde_ui_node_set_pivot(_t, (rde_vec_2F){ 0.5f, 0.5f });
        rde_ui_node_set_offsets(_t, (rde_vec_2F){ 3.0f, 6.0f }, (rde_vec_2F){ -3.0f, 24.0f });
        rde_ui_label_set_font_scale(_button->internal_label, 11.0f / (f32)FUDE_KIT_FONT_SIZE);
    }
}

void fude_side_forget(fude_ui* _ui) {
    fude_side* _side = &_ui->side;
    free(_side->_note_refs);
    free(_side->_rows);
    _side->_note_refs = NULL;
    _side->_rows      = NULL;
    fude_side_licence_free(_side);
}

void fude_side_create(fude_ui* _ui, rde_ui_node* _root) {
    fude_side* _side = &_ui->side;
    memset(_side, 0, sizeof(*_side));

    // The panel over its backdrop; the menu button goes over both (added after).
    fude_kit_modal_create(&_side->panel, _root, fude_side_on_backdrop, _ui);
    rde_ui_node* _panel = rde_ui_image_as_node(_side->panel.card);

    // The app's navigation: each button's ref says which entry.
    const fude_extension_nav* _nav = fude_side_nav(_ui);
    if(_nav != NULL) {
        _side->nav_label = fude_side_label(_ui, _panel, fude_text((FUDE_TEXT_)_nav->title), FUDE_SIDE_HEADER_PX);
    }
    for(u32 _i = 0; _i < fude_side_nav_count(_ui); _i++) {
        const fude_extension_nav_entry* _entry = &_nav->entries[_i];
        _side->nav_refs[_i]    = (fude_side_theme_ref){ _ui, _i };
        _side->_nav_counts[_i] = UINT32_MAX;
        _side->nav[_i]         = fude_kit_button(_panel, fude_text((FUDE_TEXT_)_entry->text), fude_side_on_nav, &_side->nav_refs[_i]);
        if(_entry->icon != NULL) {
            fude_kit_icon(_side->nav[_i], _entry->icon, FUDE_KIT_ICON_LEFT, _entry->icon_px);
        }
        if(_entry->enabled != NULL && !_entry->enabled(_ui->app)) {
            fude_kit_set_enabled(_side->nav[_i], false);
        }
    }

    _side->notes_label = fude_side_label(_ui, _panel, fude_text(FUDE_TEXT_SIDE_NOTES), FUDE_SIDE_HEADER_PX);
    _side->new_folder  = fude_kit_button(_panel, fude_text(FUDE_TEXT_FOLDER), fude_side_on_new_folder, _ui);
    _side->new_canvas  = fude_kit_button(_panel, fude_text(FUDE_TEXT_CANVAS), fude_side_on_new_canvas, _ui);
    fude_kit_icon(_side->new_folder, FUDE_ICON_FOLDER_ADD, FUDE_KIT_ICON_LEFT, 14.0f);
    fude_kit_icon(_side->new_canvas, FUDE_ICON_FILE_ADD, FUDE_KIT_ICON_LEFT, 14.0f);
    _side->notes_list  = rde_ui_scroll_area_create(NULL);
    rde_ui_scroll_area_set_bar_thickness(_side->notes_list, 3.0f);
    rde_ui_node_add_child(_panel, rde_ui_scroll_area_as_node(_side->notes_list));

    // What a drag shows, over the list: the line, the box, the name following.
    _side->drop_line  = rde_ui_image_create(NULL);
    _side->drop_box   = rde_ui_image_create(NULL);
    _side->drag_ghost = rde_ui_image_create(NULL);
    rde_ui_node* const _drag_nodes[] = { rde_ui_image_as_node(_side->drop_line), rde_ui_image_as_node(_side->drop_box), rde_ui_image_as_node(_side->drag_ghost) };
    for(u32 _i = 0; _i < sizeof(_drag_nodes) / sizeof(_drag_nodes[0]); _i++) {
        rde_ui_node_set_raycast_target(_drag_nodes[_i], false);
        rde_ui_node_add_child(_panel, _drag_nodes[_i]);
        rde_ui_node_set_active(_drag_nodes[_i], false);
    }
    _side->drag_ghost_label = fude_side_label(_ui, rde_ui_image_as_node(_side->drag_ghost), "", 13.0f);
    fude_kit_place(rde_ui_label_as_node(_side->drag_ghost_label), (rde_vec_2F){ 85.0f, 18.0f }, (rde_vec_2F){ 150.0f, 36.0f });

    c8 _version[64];
    snprintf(_version, sizeof(_version), "%s %s", _ui->app->info->name, _ui->app->info->version);
    _side->version         = fude_side_label(_ui, _panel, _version, 11.0f);
    _side->settings_button = fude_kit_button(_panel, fude_text(FUDE_TEXT_SETTINGS), fude_side_on_settings, _ui);
    fude_kit_icon(_side->settings_button, FUDE_ICON_SETTINGS, FUDE_KIT_ICON_LEFT, 15.0f);
    if(fude_app_ext(_ui->app)->tutorial != NULL) {
        _side->tutorial = fude_kit_button(_panel, fude_text(FUDE_TEXT_TUTORIAL), fude_side_on_tutorial, _ui);
        fude_kit_icon(_side->tutorial, FUDE_ICON_INFO, FUDE_KIT_ICON_LEFT, 15.0f);
    }
    _side->rate = fude_kit_button(_panel, fude_text(FUDE_TEXT_RATE), fude_side_on_rate, _ui);
    fude_kit_icon(_side->rate, FUDE_ICON_STAR, FUDE_KIT_ICON_LEFT, 15.0f);
#if !defined(RDE_PLATFORM_MOBILE)
    rde_ui_node_set_active(rde_ui_button_as_node(_side->rate), false);   // no store here
#endif

    // The menu button: an icon on a small card of its own, over the page.
    _side->menu_button = fude_kit_button(_root, fude_text(FUDE_TEXT_MENU), fude_side_on_menu, _ui);
    fude_kit_icon(_side->menu_button, FUDE_ICON_MENU, FUDE_KIT_ICON_ONLY, 17.0f);

    // Settings, over everything.
    fude_kit_modal_create(&_side->settings, _root, fude_side_on_settings_close, _ui);
    rde_ui_node* _card = rde_ui_image_as_node(_side->settings.card);

    _side->settings_title = fude_side_label(_ui, _card, fude_text(FUDE_TEXT_SETTINGS), FUDE_SIDE_TITLE_PX);
    _side->theme_label    = fude_side_label(_ui, _card, fude_text(FUDE_TEXT_SETTINGS_THEME), FUDE_SIDE_HEADER_PX);
    for(u32 _i = 0; _i < FUDE_THEME_COUNT; _i++) {
        _side->theme_refs[_i] = (fude_side_theme_ref){ _ui, _i };
        _side->themes[_i]     = fude_kit_button(_card, fude_text((FUDE_TEXT_)(FUDE_TEXT_THEME_PAPER + _i)), fude_side_on_theme, _ui);   // FUDE_THEME_ order
        rde_ui_button_set_on_click(_side->themes[_i], fude_side_on_theme, &_side->theme_refs[_i]);
    }
    _side->language_label = fude_side_label(_ui, _card, fude_text(FUDE_TEXT_SETTINGS_LANGUAGE), FUDE_SIDE_HEADER_PX);
    for(u32 _i = 0; _i < FUDE_TEXT_LANGUAGES; _i++) {
        _side->language_refs[_i] = (fude_side_theme_ref){ _ui, _i };
        _side->languages[_i]     = fude_kit_button(_card, FUDE_TEXT_LANGUAGE_LIST[_i].name, fude_side_on_language, _ui);
        rde_ui_button_set_on_click(_side->languages[_i], fude_side_on_language, &_side->language_refs[_i]);
        fude_side_flag(_side->languages[_i], _i);
    }
    _side->width_label    = fude_side_label(_ui, _card, fude_text(FUDE_TEXT_SETTINGS_PEN_WIDTH), FUDE_SIDE_ROW_PX);
    _side->width_even     = fude_kit_button(_card, fude_text(FUDE_TEXT_SETTINGS_EVEN), fude_side_on_width_even, _ui);
    _side->width_pressure = fude_kit_button(_card, fude_text(FUDE_TEXT_SETTINGS_PRESSURE), fude_side_on_width_pressure, _ui);
    _side->paper_label  = fude_side_label(_ui, _card, fude_text(FUDE_TEXT_SETTINGS_PAPER), FUDE_SIDE_ROW_PX);
    _side->paper_sizes[FUDE_PAPER_SMALL]  = fude_kit_button(_card, fude_text(FUDE_TEXT_SIZE_SMALL), fude_side_on_paper_small, _ui);
    _side->paper_sizes[FUDE_PAPER_MEDIUM] = fude_kit_button(_card, fude_text(FUDE_TEXT_SIZE_MEDIUM), fude_side_on_paper_medium, _ui);
    _side->paper_sizes[FUDE_PAPER_LARGE]  = fude_kit_button(_card, fude_text(FUDE_TEXT_SIZE_LARGE), fude_side_on_paper_large, _ui);
    _side->about_label    = fude_side_label(_ui, _card, fude_text(FUDE_TEXT_SETTINGS_ABOUT), FUDE_SIDE_HEADER_PX);

    for(u32 _i = 0; _i < fude_side_section_count(_ui); _i++) {
        fude_app_ext(_ui->app)->sections[_i].build(_ui, _card);
    }

    c8 _about[1536];
    FUDE_TEXTF(_about, FUDE_TEXT_ABOUT_BUILT, FUDE_TS(_ui->app->info->version), FUDE_TS(__DATE__));
    if(_ui->app->info->credits < FUDE_TEXT_COUNT) {
        snprintf(_about + strlen(_about), sizeof(_about) - strlen(_about), "\n\n%s", fude_text((FUDE_TEXT_)_ui->app->info->credits));
    }
    _side->about_text = fude_side_label(_ui, _card, _about, FUDE_SIDE_SMALL_PX);
    rde_ui_label_set_wrap(_side->about_text, true);
    // A short screen (landscape) gives it less room: smaller rather than over the buttons.
    rde_ui_label_set_auto_fit(_side->about_text, true);
    rde_ui_label_set_auto_fit_min_scale(_side->about_text, 0.7f);
    rde_ui_label_set_alignment(_side->about_text, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_TOP);
    _side->settings_close = fude_kit_button(_card, fude_text(FUDE_TEXT_CLOSE), fude_side_on_settings_close, _ui);
    _side->licences_button = fude_kit_button(_card, fude_text(FUDE_TEXT_LICENCES), fude_side_on_licences, _ui);
    _side->data_button     = fude_kit_button(_card, fude_text(FUDE_TEXT_DATA), fude_side_on_data, _ui);

    // Your data, over Settings: what Kana does with it, then Export and Import.
    fude_kit_modal_create(&_side->data, _root, fude_side_on_data_close, _ui);
    rde_ui_node* _dcard = rde_ui_image_as_node(_side->data.card);
    _side->data_title = fude_side_label(_ui, _dcard, fude_text(FUDE_TEXT_DATA), FUDE_SIDE_TITLE_PX);
    {
        // Offline; where the platform keeps a copy; how to keep one yourself.
#if defined(RDE_PLATFORM_IOS)
        const FUDE_TEXT_ _backup = FUDE_TEXT_DATA_BACKUP_IOS;
#elif defined(RDE_PLATFORM_ANDROID)
        const FUDE_TEXT_ _backup = FUDE_TEXT_DATA_BACKUP_ANDROID;
#else
        const FUDE_TEXT_ _backup = FUDE_TEXT_DATA_BACKUP_DESKTOP;
#endif
        c8 _text[2048];
        snprintf(_text, sizeof(_text), "%s\n\n%s\n\n%s", fude_text(FUDE_TEXT_DATA_OFFLINE), fude_text(_backup), fude_text(FUDE_TEXT_DATA_HOW));
        _side->data_text = fude_side_label(_ui, _dcard, _text, FUDE_SIDE_BODY_PX);
    }
    rde_ui_label_set_wrap(_side->data_text, true);
    rde_ui_label_set_auto_fit(_side->data_text, true);
    rde_ui_label_set_auto_fit_min_scale(_side->data_text, 0.7f);
    rde_ui_label_set_alignment(_side->data_text, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_TOP);
    _side->data_status = fude_side_label(_ui, _dcard, "", FUDE_SIDE_BODY_PX);
    rde_ui_label_set_wrap(_side->data_status, true);
    rde_ui_label_set_auto_fit(_side->data_status, true);
    rde_ui_label_set_auto_fit_min_scale(_side->data_status, 0.7f);
    rde_ui_label_set_alignment(_side->data_status, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_TOP);
    _side->data_export  = fude_kit_button(_dcard, fude_text(FUDE_TEXT_DATA_EXPORT), fude_side_on_data_export, _ui);
    _side->data_import  = fude_kit_button(_dcard, fude_text(FUDE_TEXT_DATA_IMPORT), fude_side_on_data_import, _ui);
    _side->data_replace = fude_kit_button(_dcard, fude_text(FUDE_TEXT_DATA_REPLACE), fude_side_on_data_replace, _ui);
    _side->data_cancel  = fude_kit_button(_dcard, fude_text(FUDE_TEXT_CANCEL), fude_side_on_data_cancel, _ui);
    _side->data_close   = fude_kit_button(_dcard, fude_text(FUDE_TEXT_CLOSE), fude_side_on_data_close, _ui);
    fude_kit_icon(_side->data_export, FUDE_ICON_EXPORT, FUDE_KIT_ICON_LEFT, 15.0f);
    fude_kit_icon(_side->data_import, FUDE_ICON_IMPORT, FUDE_KIT_ICON_LEFT, 15.0f);
    _side->_data_shown[0]      = 1;   // not yet: shown on the first update
    _side->_data_confirm_shown = true;

    // Licences, over Settings.
    fude_kit_modal_create(&_side->licences, _root, fude_side_on_licences_close, _ui);
    rde_ui_node* _lcard = rde_ui_image_as_node(_side->licences.card);
    _side->licences_title = fude_side_label(_ui, _lcard, fude_text(FUDE_TEXT_LICENCES), FUDE_SIDE_TITLE_PX);
    for(u32 _i = 0; _i < fude_side_licence_count(_ui); _i++) {
        _side->licences_refs[_i] = (fude_side_theme_ref){ _ui, _i };
        _side->licences_docs[_i] = fude_kit_button(_lcard, fude_text((FUDE_TEXT_)_ui->app->info->licences[_i].name), fude_side_on_licence_doc, _ui);
        rde_ui_button_set_on_click(_side->licences_docs[_i], fude_side_on_licence_doc, &_side->licences_refs[_i]);
    }
    _side->licences_text = rde_ui_scroll_area_create(NULL);
    rde_ui_scroll_area_set_bar_thickness(_side->licences_text, 4.0f);
    rde_ui_node_add_child(_lcard, rde_ui_scroll_area_as_node(_side->licences_text));
    for(u32 _k = 0; _k < FUDE_SIDE_LICENCE_LINES; _k++) {
        _side->licences_lines[_k] = fude_side_label(_ui, rde_ui_scroll_area_as_node(_side->licences_text), "", FUDE_SIDE_LICENCE_PX);
        rde_ui_node_set_active(rde_ui_label_as_node(_side->licences_lines[_k]), false);
    }
    _side->licences_close = fude_kit_button(_lcard, fude_text(FUDE_TEXT_CLOSE), fude_side_on_licences_close, _ui);
    _side->_licence_first = -1;

    // The note card, over the panel (and a backdrop that cancels it).
    fude_kit_modal_create(&_side->note, _root, fude_side_on_card_cancel, _ui);
    rde_ui_node* _note_card = rde_ui_image_as_node(_side->note.card);
    _side->note_title = fude_side_label(_ui, _note_card, "", FUDE_SIDE_CARD_TITLE_PX);
    _side->note_body  = fude_side_label(_ui, _note_card, "", FUDE_SIDE_BODY_PX);
    rde_ui_label_set_wrap(_side->note_body, true);
    _side->note_field = rde_ui_text_editor_create(_ui->font, NULL);
    rde_ui_text_editor_set_multiline(_side->note_field, false);
    rde_ui_text_editor_set_font_size(_side->note_field, 14u);
    rde_ui_text_editor_set_max_chars(_side->note_field, FUDE_NOTE_NAME - 8);
    rde_ui_text_editor_set_content_insets(_side->note_field, 12.0f, 8.0f, 12.0f, 8.0f);
    rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_side->note_field), _ui);
    rde_ui_text_editor_set_on_submit(_side->note_field, fude_side_on_field_submit);
    fude_kit_field_box(_note_card, _side->note_field);
    c8 _with_icon[96];
    snprintf(_with_icon, sizeof(_with_icon), FUDE_ICON_DRAW "  %s", fude_text(FUDE_TEXT_RENAME));
    _side->note_rename  = fude_kit_button(_note_card, _with_icon, fude_side_on_card_rename, _ui);
    snprintf(_with_icon, sizeof(_with_icon), FUDE_ICON_FILE_ADD "  %s", fude_text(FUDE_TEXT_CANVAS));
    _side->note_add     = fude_kit_button(_note_card, _with_icon, fude_side_on_card_add, _ui);
    snprintf(_with_icon, sizeof(_with_icon), FUDE_ICON_FOLDER_ADD "  %s", fude_text(FUDE_TEXT_FOLDER));
    _side->note_add_folder = fude_kit_button(_note_card, _with_icon, fude_side_on_card_add_folder, _ui);
    snprintf(_with_icon, sizeof(_with_icon), FUDE_ICON_TRASH "  %s", fude_text(FUDE_TEXT_DELETE));
    _side->note_delete  = fude_kit_button(_note_card, _with_icon, fude_side_on_card_delete, _ui);
    _side->note_cancel  = fude_kit_button(_note_card, fude_text(FUDE_TEXT_CANCEL), fude_side_on_card_cancel, _ui);
    _side->note_confirm = fude_kit_button(_note_card, fude_text(FUDE_TEXT_SAVE), fude_side_on_card_confirm, _ui);

    // All hidden until asked for (the cards are, made); the menu button shows with the page.
    rde_ui_node_set_active(rde_ui_button_as_node(_side->menu_button), false);

    fude_side_layout(_ui);
}

// --- every frame ------------------------------------------------------------------------

void fude_side_update(fude_ui* _ui, b8 _full) {
    fude_side* _side = &_ui->side;
    if(_side->panel.card == NULL) {
        return;
    }

    // Laid out again when the screen rotates — and when the safe area changes:
    // at start iOS reports none (SDL has the whole window until the view is laid
    // out), then the status bar's arrives a frame or two later.
    const rde_vec_2F _screen = fude_kit_screen_size((_ui)->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_ui->window);
    if(memcmp(&_screen, &_side->_laid_out, sizeof(rde_vec_2F)) != 0 || memcmp(&_insets, &_side->_laid_out_insets, sizeof(rde_vec_4I)) != 0) {
        fude_side_layout(_ui);
    }

    if(_full) {
        _side->open = false;          // a scene took the screen: the panel is done
    }

    fude_side_show(rde_ui_button_as_node(_side->menu_button), !_full, &_side->_shown_menu);
    // The navigation's counts (Kana's Reviews · n): a label changes only with its count.
    for(u32 _i = 0; _side->open && _i < fude_side_nav_count(_ui); _i++) {
        const fude_extension_nav_entry* _entry = &fude_side_nav(_ui)->entries[_i];
        const u32                       _n     = _entry->count != NULL ? _entry->count(_ui->app) : 0u;
        if(_entry->count != NULL && _n != _side->_nav_counts[_i]) {
            _side->_nav_counts[_i] = _n;
            c8 _label[64];
            if(_n > 0) { FUDE_TEXTF(_label, (FUDE_TEXT_)_entry->counted_text, FUDE_TN(_n)); }
            else       { snprintf(_label, sizeof(_label), "%s", fude_text((FUDE_TEXT_)_entry->text)); }
            rde_ui_button_set_text(_side->nav[_i], _label);
        }
    }
    fude_kit_modal_show(&_side->panel, _side->open);
    fude_kit_modal_show(&_side->settings, _side->settings_open);
    if(!_side->settings_open && _side->licences_open) {
        _side->licences_open = false;
        fude_side_licence_free(_side);
    }
    fude_kit_modal_show(&_side->licences, _side->licences_open);
    if(_side->licences_open) {
        fude_side_licence_scroll(_ui);
    }
    for(u32 _i = 0; _side->settings_open && _i < fude_side_section_count(_ui); _i++) {
        fude_app_ext(_ui->app)->sections[_i].refresh(_ui, false);   // what moves on by itself (Kana: ML Kit's download)
    }
    if(!_side->settings_open && _side->data_open) {
        _side->data_open = false;
        fude_side_data_done(_ui);
    }
    fude_kit_modal_show(&_side->data, _side->data_open);
    if(_side->data_open) {
        if(strcmp(_side->_data_shown, _side->data_message) != 0) {
            snprintf(_side->_data_shown, sizeof(_side->_data_shown), "%s", _side->data_message);
            rde_ui_label_set_text(_side->data_status, _side->data_message);
            const fude_theme* _t = fude_theme_active();
            rde_ui_label_set_color(_side->data_status, _side->data_message_bad ? _t->score_poor : _t->text);
        }
        if(_side->data_confirming != _side->_data_confirm_shown) {
            _side->_data_confirm_shown = _side->data_confirming;
            rde_ui_node_set_active(rde_ui_button_as_node(_side->data_export), !_side->data_confirming);
            rde_ui_node_set_active(rde_ui_button_as_node(_side->data_import), !_side->data_confirming);
            rde_ui_node_set_active(rde_ui_button_as_node(_side->data_replace), _side->data_confirming);
            rde_ui_node_set_active(rde_ui_button_as_node(_side->data_cancel), _side->data_confirming);
        }
    }

    const b8 _card = _side->card_mode != FUDE_SIDE_CARD_NONE && _side->open;
    fude_kit_modal_show(&_side->note, _card);
    if(!_side->open) {
        _side->card_mode = FUDE_SIDE_CARD_NONE;
    }

    // A drag near the list's top or bottom scrolls it.
    if(_side->drag_id != 0) {
        const f32  _top    = _side->_list_bl.y + _side->_list_size.y;
        rde_vec_2F _scroll = rde_ui_scroll_area_get_scroll(_side->notes_list);
        if(_side->drag_at.y > _top - FUDE_SIDE_EDGE) {
            _scroll.y -= FUDE_SIDE_AUTOSCROLL;
        } else if(_side->drag_at.y < _side->_list_bl.y + FUDE_SIDE_EDGE) {
            _scroll.y += FUDE_SIDE_AUTOSCROLL;
        }
        rde_ui_scroll_area_set_scroll(_side->notes_list, _scroll);
        fude_side_drop_target(_ui);
    }
    if(!_side->open && _side->drag_id != 0) {
        _side->drag_id = 0;
        fude_side_hide_drop(_side);
        rde_ui_node_set_active(rde_ui_image_as_node(_side->drag_ghost), false);
    }

    // The notes list, rebuilt when the notes changed (or the list's size did) —
    // never mid-drag: the handle being dragged would go with it.
    if(_side->open && _ui->app->notes != NULL && _side->drag_id == 0 &&
       (!_side->_notes_built || _side->_notes_revision != _ui->app->notes->revision || _side->_notes_open != _ui->app->notes->open)) {
        fude_side_build_notes(_ui);
    }
}

b8 fude_side_hit(const fude_ui* _ui, rde_vec_2F _at) {
    const fude_side* _side = &_ui->side;
    if(_side->panel.shown || _side->settings.shown || _side->note.shown || _side->licences.shown) {
        return true;
    }
    return _side->_shown_menu &&
           _at.x >= _side->_menu_center.x - _side->_menu_size.x * 0.5f && _at.x <= _side->_menu_center.x + _side->_menu_size.x * 0.5f &&
           _at.y >= _side->_menu_center.y - _side->_menu_size.y * 0.5f && _at.y <= _side->_menu_center.y + _side->_menu_size.y * 0.5f;
}

// --- the theme ----------------------------------------------------------------------------

void fude_side_apply_theme(fude_ui* _ui) {
    fude_side*        _side = &_ui->side;
    const fude_theme* _t    = fude_theme_active();
    if(_side->panel.card == NULL) {
        return;
    }

    fude_kit_modal_restyle(&_side->panel, 0.0f);
    fude_kit_modal_restyle(&_side->settings, 18.0f);

    // The menu button: a small card on the page.
    fude_kit_button_colors(_side->menu_button, _t->surface, 1.0f, _t->outline);
    fude_kit_button_round(_side->menu_button, 12.0f);

    // The panel's rows are quiet, their icons in the accent.
    for(u32 _i = 0; _i < fude_side_nav_count(_ui); _i++) {
        fude_kit_restyle_quiet(_side->nav[_i]);
        fude_kit_icon_color(_side->nav[_i], _t->accent);
    }
    rde_ui_button* const _quiet[] = { _side->new_folder, _side->new_canvas, _side->settings_button, _side->tutorial, _side->rate };
    for(u32 _i = 0; _i < sizeof(_quiet) / sizeof(_quiet[0]); _i++) {
        if(_quiet[_i] != NULL) {
            fude_kit_restyle_quiet(_quiet[_i]);
        }
    }
    rde_ui_button* const _buttons[] = { _side->width_even, _side->width_pressure, _side->settings_close,
                                        _side->paper_sizes[0], _side->paper_sizes[1], _side->paper_sizes[2],
                                        _side->licences_button, _side->licences_close,
                                        _side->data_button, _side->data_export, _side->data_import, _side->data_cancel, _side->data_close,
                                        _side->note_rename, _side->note_add, _side->note_add_folder, _side->note_delete, _side->note_cancel, _side->note_confirm };
    for(u32 _i = 0; _i < sizeof(_buttons) / sizeof(_buttons[0]); _i++) {
        fude_kit_restyle_button(_buttons[_i]);
    }
    for(u32 _i = 0; _i < fude_side_licence_count(_ui); _i++) {
        fude_kit_restyle_button(_side->licences_docs[_i]);
    }
    fude_kit_button_primary(_side->settings_close);
    fude_kit_button_primary(_side->licences_close);
    fude_kit_button_primary(_side->data_close);
    fude_kit_button_danger(_side->data_replace);
    fude_kit_modal_restyle(&_side->data, 18.0f);
    _side->_data_shown[0] = 1;   // the answer line's colour again
    fude_kit_modal_restyle(&_side->note, 18.0f);
    fude_kit_modal_restyle(&_side->licences, 18.0f);
    rde_ui_scroll_area_set_background_color(_side->licences_text, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_track_color(_side->licences_text, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_thumb_colors(_side->licences_text, _t->grip, fude_kit_shade(_t->grip, 20), fude_kit_shade(_t->grip, 40));
    for(u32 _k = 0; _k < FUDE_SIDE_LICENCE_LINES; _k++) {
        rde_ui_label_set_color(_side->licences_lines[_k], _t->button_text);
    }
    fude_kit_button_danger(_side->note_delete);
    fude_kit_style_field(_side->note_field);
    rde_ui_scroll_area_set_background_color(_side->notes_list, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_track_color(_side->notes_list, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_thumb_colors(_side->notes_list, _t->grip, fude_kit_shade(_t->grip, 20), fude_kit_shade(_t->grip, 40));
    _side->_notes_built = false;   // the rows restyle as they are built again

    rde_ui_image_set_style(_side->drop_line, RDE_UI_STATE_NORMAL, fude_kit_style(_t->select, 1.5f));
    rde_ui_style _box    = fude_kit_style(_t->select_fill, 10.0f);
    _box.border_width    = 2.0f;
    _box.border_color    = _t->select;
    rde_ui_image_set_style(_side->drop_box, RDE_UI_STATE_NORMAL, _box);
    rde_ui_style _ghost  = fude_kit_style(_t->accent, 10.0f);
    rde_ui_image_set_style(_side->drag_ghost, RDE_UI_STATE_NORMAL, _ghost);
    rde_ui_label_set_color(_side->drag_ghost_label, _t->on_accent);

    // Each theme's button previews it: its page, its text; the current one ringed.
    for(u32 _i = 0; _i < FUDE_THEME_COUNT; _i++) {
        const fude_theme* _other   = fude_theme_get((FUDE_THEME_)_i);
        const b8          _current = (FUDE_THEME_)_i == fude_theme_index();
        fude_kit_button_colors(_side->themes[_i], _other->page, _current ? 3.0f : 1.0f, _current ? _t->accent : _other->outline);
        rde_ui_label_set_color(_side->themes[_i]->internal_label, _other->text);
    }

    rde_ui_label* const _headers[] = { _side->nav_label, _side->theme_label, _side->language_label, _side->notes_label, _side->about_label };
    for(u32 _i = 0; _i < sizeof(_headers) / sizeof(_headers[0]); _i++) {
        if(_headers[_i] != NULL) {
            rde_ui_label_set_color(_headers[_i], _t->text_soft);
        }
    }
    rde_ui_label* const _texts[] = { _side->settings_title, _side->width_label, _side->paper_label, _side->note_title,
                                     _side->licences_title, _side->data_title, _side->data_text };
    for(u32 _i = 0; _i < sizeof(_texts) / sizeof(_texts[0]); _i++) {
        rde_ui_label_set_color(_texts[_i], _t->text);
    }
    rde_ui_label* const _soft[] = { _side->about_text, _side->note_body };
    for(u32 _i = 0; _i < sizeof(_soft) / sizeof(_soft[0]); _i++) {
        rde_ui_label_set_color(_soft[_i], _t->text_soft);
    }
    rde_ui_label_set_color(_side->version, _t->text_soft);
    for(u32 _i = 0; _i < fude_side_section_count(_ui); _i++) {
        fude_app_ext(_ui->app)->sections[_i].restyle(_ui);
    }

    fude_side_refresh_settings(_ui);
}
