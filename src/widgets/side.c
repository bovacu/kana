#include "widgets/side.h"
#include "study/review.h"
#include "study/marks.h"
#include "app/ui.h"
#include "widgets/notice.h"
#include "widgets/kit.h"
#include "app/version.h"
#include "ink/notes.h"
#include "services/mlkit.h"
#include "base/kfile.h"
#include "widgets/icons.h"
#include "base/text.h"
#include "widgets/draw.h"

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
#define KANA_SIDE_CARD_H    888.0f
#define KANA_SIDE_LICENCE_PX 11.0f    // Licences' text
// Text sizes (the UI font's units; Slug draws an em ~1.31 x these).
#define KANA_SIDE_TITLE_PX      20.0f   // a card's title: Settings, Licences
#define KANA_SIDE_CARD_TITLE_PX 17.0f   // a small card's (a note's)
#define KANA_SIDE_HEADER_PX     10.5f   // a section's: STUDY, THEME
#define KANA_SIDE_ROW_PX        14.0f   // a setting's name
#define KANA_SIDE_BODY_PX       13.0f
#define KANA_SIDE_SMALL_PX      11.5f   // notes, the credits
#define KANA_SIDE_NOTE_H    40.0f     // a row of the notes list
#define KANA_SIDE_DOTS_W    44.0f     // its "…"
#define KANA_SIDE_INDENT    18.0f     // a canvas in a folder
#define KANA_SIDE_NOTE_CARD (rde_vec_2F){ 540.0f, 196.0f }
#define KANA_SIDE_CARD_PAD  22.0f     // inside a card
#define KANA_SIDE_HANDLE_W  28.0f     // a row's drag handle
#define KANA_SIDE_EDGE      44.0f     // a drag this near the list's top or bottom scrolls it
#define KANA_SIDE_AUTOSCROLL 9.0f     // ...this many units a frame

// The credits the character data's licences require be shown to users
// (assets/data/LICENSE-data.txt has them in full) are KANA_TEXT_CREDITS, in
// About: short — in full they are Settings › Licences (Data: the attribution
// KanjiVG and EDRDG require; ML Kit: Google's terms and notices).

// Licences: what each document is made of (files in the app, one after the other).
static const struct { KANA_TEXT_ name; const c8* files[3]; } KANA_SIDE_LICENCES[KANA_SIDE_LICENCE_DOCS] = {
    { KANA_TEXT_LICENCE_DATA,      { "assets/data/LICENSE-data.txt", NULL, NULL } },
    { KANA_TEXT_LICENCE_FONTS,     { "assets/fonts/LICENSE-Roboto.txt", "assets/fonts/LICENSE-NotoSansJP.txt", "assets/fonts/LICENSE-Phosphor.txt" } },
    { KANA_TEXT_LICENCE_LIBRARIES, { "assets/licenses/libraries.txt", NULL, NULL } },
    { KANA_TEXT_LICENCE_MLKIT,     { "assets/licenses/ml-kit-notices.txt", NULL, NULL } },
};

// --- helpers ----------------------------------------------------------------------

RDE_INTERNAL rde_ui_label* kana_side_label(kana_ui* _ui, rde_ui_node* _parent, const c8* _text, f32 _px) {
    rde_ui_label* _label = rde_ui_label_create(NULL);
    rde_ui_label_set_font(_label, _ui->font);
    rde_ui_label_set_text(_label, _text);
    rde_ui_label_set_font_scale(_label, _px / (f32)KANA_KIT_FONT_SIZE);
    rde_ui_label_set_alignment(_label, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_MIDDLE);
    // A longer language shrinks it to fit, never cuts it.
    rde_ui_label_set_auto_fit(_label, true);
    rde_ui_label_set_auto_fit_min_scale(_label, 0.6f);
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

// The Handwriting row: ML Kit on or off, and where its model is — shown again
// only when that changed.
RDE_INTERNAL void kana_side_refresh_mlkit(kana_ui* _ui) {
    kana_side*        _side  = &_ui->side;
    const KANA_MLKIT_ _state = kana_mlkit_state();
    const b8          _on    = kana_mlkit_enabled();
    const i32         _shown = (i32)_state + (_on ? 0 : 100);
    if(_side->mlkit_toggle == NULL || _shown == _side->_mlkit_shown) {
        return;
    }
    _side->_mlkit_shown = _shown;

    const c8* _status   = "";
    const c8* _download = NULL;
    if(_state == KANA_MLKIT_UNAVAILABLE) {
        _status = kana_text(KANA_TEXT_MLKIT_UNAVAILABLE);
    } else if(!_on) {
        _status = kana_text(KANA_TEXT_MLKIT_OFF);
    } else if(_state == KANA_MLKIT_READY) {
        _status = kana_text(KANA_TEXT_MLKIT_READY);
    } else if(_state == KANA_MLKIT_DOWNLOADING) {
        _status = kana_text(KANA_TEXT_MLKIT_DOWNLOADING);
    } else if(_state == KANA_MLKIT_FAILED) {
        _status   = kana_text(KANA_TEXT_MLKIT_FAILED);
        _download = kana_text(KANA_TEXT_RETRY);
    } else {
        _status   = kana_text(KANA_TEXT_MLKIT_MISSING);
        _download = kana_text(KANA_TEXT_DOWNLOAD);
    }
    rde_ui_label_set_text(_side->mlkit_status, _status);
    rde_ui_node_set_active(rde_ui_button_as_node(_side->mlkit_download), _download != NULL);
    if(_download != NULL) {
        rde_ui_button_set_text(_side->mlkit_download, _download);
    }
    rde_ui_button_set_text(_side->mlkit_toggle, kana_text(_on && _state != KANA_MLKIT_UNAVAILABLE ? KANA_TEXT_ON : KANA_TEXT_OFF));
    if(_on && _state != KANA_MLKIT_UNAVAILABLE) { kana_kit_button_selected(_side->mlkit_toggle); } else { kana_kit_button_plain(_side->mlkit_toggle); }
    kana_kit_set_enabled(_side->mlkit_toggle, _state != KANA_MLKIT_UNAVAILABLE);
}

// What the settings show: the pen's width mode, the paper size, ML Kit.
RDE_INTERNAL void kana_side_refresh_settings(kana_ui* _ui) {
    kana_side* _side = &_ui->side;
    _side->_mlkit_shown = -1;
    kana_side_refresh_mlkit(_ui);
    const b8 _pressure = _ui->app->ink->width_mode == KANA_INK_WIDTH_MODE_PRESSURE;
    if(_pressure) { kana_kit_button_plain(_side->width_even); kana_kit_button_selected(_side->width_pressure); }
    else          { kana_kit_button_selected(_side->width_even); kana_kit_button_plain(_side->width_pressure); }

    for(u32 _i = 0; _i < KANA_PAPER_SIZE_COUNT; _i++) {
        if(_ui->app->canvas->paper_size == (KANA_PAPER_SIZE_)_i) { kana_kit_button_selected(_side->paper_sizes[_i]); }
        else                                                     { kana_kit_button_plain(_side->paper_sizes[_i]); }
    }
    for(u32 _i = 0; _i < KANA_TEXT_LANGUAGES; _i++) {
        if(KANA_TEXT_LANGUAGE_LIST[_i].language == kana_text_language()) { kana_kit_button_selected(_side->languages[_i]); }
        else                                                               { kana_kit_button_quiet(_side->languages[_i]); }
        kana_kit_button_round(_side->languages[_i], 12.0f);
    }
}

void kana_side_close(kana_ui* _ui) {
    _ui->side.open = false;
}

// --- callbacks ----------------------------------------------------------------------

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_menu(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    _ui->side.open = !_ui->side.open;
    if(_ui->side.open) {
        kana_toolbar_set_palette_open(&_ui->bar, false);
        kana_pagemenu_close_context(&_ui->page);
    }
    kana_ui_update(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_backdrop(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    _ui->side.open = false;
    kana_ui_update(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A screen of the side panel's (its button's ref: the screen, app.h), opened
// over the page; the panel is done. Reviews: with none due, a notice instead,
// and the panel stays.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_screen(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_side_theme_ref* _ref = (const kana_side_theme_ref*)_user_data;
    kana_ui*                   _ui  = _ref->ui;
    if(_ref->value == KANA_SIDE_REVIEWS) {
        if(!kana_app_review(_ui->app)) {
            return RDE_UI_EVENT_RESULT_CONSUME;
        }
    } else {
        kana_app_open(_ui->app, (KANA_SCREEN_)_ref->value);
    }
    _ui->side.open = false;
    kana_ui_update(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A theme: everything restyles at once; Settings stays open to compare.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_theme(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_side_theme_ref* _ref = (const kana_side_theme_ref*)_user_data;
    kana_theme_set((KANA_THEME_)_ref->value);
    kana_ui_apply_theme(_ref->ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A language: read now; the UI is built again in it next frame (it cannot be
// while its own button is being pressed: kana_ui_follow_language).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_language(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_side_theme_ref* _ref = (const kana_side_theme_ref*)_user_data;
    if(_ref->value < KANA_TEXT_LANGUAGES && KANA_TEXT_LANGUAGE_LIST[_ref->value].language != kana_text_language()) {
        kana_text_set_language(KANA_TEXT_LANGUAGE_LIST[_ref->value].language);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_settings(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    _ui->side.open          = false;
    _ui->side.settings_open = true;
    kana_side_refresh_settings(_ui);
    kana_ui_update(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_settings_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    _ui->side.settings_open = false;
    kana_ui_update(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// --- Your data --------------------------------------------------------------------------

KANA_SIDE_DATA_ kana_side_take_data_request(kana_ui* _ui) {
    const KANA_SIDE_DATA_ _r = _ui->side.data_request;
    _ui->side.data_request = KANA_SIDE_DATA_NONE;
    return _r;
}

void kana_side_data_message(kana_ui* _ui, const c8* _text, b8 _bad) {
    snprintf(_ui->side.data_message, sizeof(_ui->side.data_message), "%s", _text != NULL ? _text : "");
    _ui->side.data_message_bad = _bad;
}

void kana_side_data_done(kana_ui* _ui) {
    kana_side* _side = &_ui->side;
    if(_side->data_backup != NULL) {
        rde_memory_allocator* _a = rde_memory_allocator_get_default_std();
        _a->free(_a->allocator, _side->data_backup);
    }
    _side->data_backup      = NULL;
    _side->data_backup_size = 0;
    _side->data_confirming  = false;
}

void kana_side_data_confirm(kana_ui* _ui, u8* _backup, usize _size, const c8* _question) {
    kana_side_data_done(_ui);
    _ui->side.data_backup      = _backup;
    _ui->side.data_backup_size = _size;
    _ui->side.data_confirming  = true;
    kana_side_data_message(_ui, _question, false);
}

// Rate Kana: the store's page for a review (RDE: the App Store's write-review
// page once KANA_APP_STORE_ID is set, the rating sheet until then; Play's listing).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_rate(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    _ui->side.open = false;
    rde_mobile_open_review_page(KANA_APP_STORE_ID);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// The tutorial again (the welcome, welcome.h): the panel closes, kana.c shows it.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_tutorial(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    _ui->side.open            = false;
    _ui->side.welcome_request = true;
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_data(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    _ui->side.data_open = true;
    kana_side_data_message(_ui, "", false);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_data_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    _ui->side.data_open = false;
    kana_side_data_done(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_data_export(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    ((kana_ui*)_user_data)->side.data_request = KANA_SIDE_DATA_EXPORT;
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_data_import(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    ((kana_ui*)_user_data)->side.data_request = KANA_SIDE_DATA_IMPORT;
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_data_replace(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    if(_ui->side.data_confirming) {
        _ui->side.data_request = KANA_SIDE_DATA_REPLACE;
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_data_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    kana_side_data_done(_ui);
    kana_side_data_message(_ui, "", false);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_width_even(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    _ui->app->ink->width_mode = KANA_INK_WIDTH_MODE_CONSTANT;
    kana_side_refresh_settings(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_width_pressure(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    _ui->app->ink->width_mode = KANA_INK_WIDTH_MODE_PRESSURE;
    kana_side_refresh_settings(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// The size of lines and squares, on every canvas (the toolbar's Paper chooses
// them for one).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_paper_size(kana_ui* _ui, KANA_PAPER_SIZE_ _size) {
    _ui->app->canvas->paper_size = _size;
    kana_side_refresh_settings(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_paper_small(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    return kana_side_paper_size((kana_ui*)_user_data, KANA_PAPER_SMALL);
}
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_paper_medium(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    return kana_side_paper_size((kana_ui*)_user_data, KANA_PAPER_MEDIUM);
}
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_paper_large(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    return kana_side_paper_size((kana_ui*)_user_data, KANA_PAPER_LARGE);
}

// Google ML Kit on or off: on, its model is fetched if it is missing.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_mlkit(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    kana_mlkit_set_enabled(!kana_mlkit_enabled());
    kana_mlkit_prepare();
    kana_side_refresh_mlkit(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_mlkit_download(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_mlkit_prepare();
    kana_side_refresh_mlkit((kana_ui*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// --- licences ------------------------------------------------------------------------------

RDE_INTERNAL void kana_side_licence_free(kana_side* _side) {
    free(_side->_licence_text);
    free(_side->_licence_starts);
    _side->_licence_text   = NULL;
    _side->_licence_starts = NULL;
    _side->_licence_count  = 0;
    _side->_licence_first  = -1;
}

// Document _doc read, cut into lines that fit the scroll area's width (on the
// spaces where there are some), and scrolled to its top.
RDE_INTERNAL void kana_side_licence_load(kana_ui* _ui, u32 _doc) {
    kana_side* _side = &_ui->side;
    kana_side_licence_free(_side);
    _side->licences_doc = _doc;

    // The files, one after the other.
    usize _size = 0;
    for(u32 _f = 0; _f < 3 && KANA_SIDE_LICENCES[_doc].files[_f] != NULL; _f++) {
        u32 _n = 0;
        u8* _data = kana_file_read(KANA_SIDE_LICENCES[_doc].files[_f], &_n);
        if(_data == NULL) {
            continue;
        }
        _side->_licence_text = (c8*)realloc(_side->_licence_text, _size + _n + 3);
        memcpy(_side->_licence_text + _size, _data, _n);
        _size += _n;
        _side->_licence_text[_size++] = '\n';
        _side->_licence_text[_size++] = '\n';
        kana_file_free(_data);
    }
    if(_side->_licence_text == NULL) {
        const c8* _missing = kana_text(KANA_TEXT_LICENCE_MISSING);
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
    const f32        _scale = KANA_SIDE_LICENCE_PX / (f32)KANA_KIT_FONT_SIZE;
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
    for(u32 _d = 0; _d < KANA_SIDE_LICENCE_DOCS; _d++) {
        kana_kit_button_chip(_side->licences_docs[_d], _d == _doc);
    }
}

// The lines on screen get the labels: placed only when the first one changed.
RDE_INTERNAL void kana_side_licence_scroll(kana_ui* _ui) {
    kana_side* _side = &_ui->side;
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
    for(u32 _k = 0; _k < KANA_SIDE_LICENCE_LINES; _k++) {
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
        kana_kit_place(_label, (rde_vec_2F){ 6.0f + (_side->_licence_width - 12.0f) * 0.5f, _content - 4.0f - ((f32)_n + 0.5f) * _side->_licence_line_h },
                           (rde_vec_2F){ _side->_licence_width - 12.0f, _side->_licence_line_h });
    }
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_licences(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    _ui->side.licences_open = true;
    kana_side_licence_load(_ui, _ui->side.licences_doc);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

void kana_side_open_settings(kana_ui* _ui, i32 _licences) {
    _ui->side.open          = false;
    _ui->side.settings_open = true;
    kana_side_refresh_settings(_ui);
    if(_licences >= 0 && _licences < (i32)KANA_SIDE_LICENCE_DOCS) {
        _ui->side.licences_open = true;
        kana_side_licence_load(_ui, (u32)_licences);
    }
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_licences_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    _ui->side.licences_open = false;
    kana_side_licence_free(&_ui->side);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_licence_doc(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_side_theme_ref* _ref = (const kana_side_theme_ref*)_user_data;
    kana_side_licence_load(_ref->ui, _ref->value);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// --- notes -----------------------------------------------------------------------------

RDE_INTERNAL void kana_side_card(kana_ui* _ui, KANA_SIDE_CARD_ _mode, u32 _note);

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_note(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_side_note_ref* _ref     = (const kana_side_note_ref*)_user_data;
    kana_ui*             _ui = _ref->ui;
    const kana_note*          _note    = kana_notes_find(_ui->app->notes, _ref->id);
    if(_note == NULL) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    if(_note->kind == KANA_NOTE_FOLDER) {
        kana_notes_set_expanded(_ui->app->notes, _note->id, !_note->expanded);
    } else {
        kana_notes_open(_ui->app->notes, _note->id);   // kana.c sees it and switches the page
        _ui->side.open = false;
        kana_ui_update(_ui);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_note_dots(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_side_note_ref* _ref = (const kana_side_note_ref*)_user_data;
    kana_side_card(_ref->ui, KANA_SIDE_CARD_ACTIONS, _ref->id);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// + Canvas: a new one at the top level, opened.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_new_canvas(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    c8 _name[KANA_NOTE_NAME];
    kana_notes_new_name(_ui->app->notes, KANA_NOTE_CANVAS, _name, sizeof(_name));
    const u32 _id = kana_notes_add(_ui->app->notes, KANA_NOTE_CANVAS, 0, _name);
    if(_id != 0) {
        kana_notes_open(_ui->app->notes, _id);
        _ui->side.open = false;
        kana_ui_update(_ui);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// + Folder: a new one, named straight away.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_new_folder(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    c8 _name[KANA_NOTE_NAME];
    kana_notes_new_name(_ui->app->notes, KANA_NOTE_FOLDER, _name, sizeof(_name));
    const u32 _id = kana_notes_add(_ui->app->notes, KANA_NOTE_FOLDER, 0, _name);
    if(_id != 0) {
        kana_side_card(_ui, KANA_SIDE_CARD_RENAME, _id);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_card_rename(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    kana_side_card(_ui, KANA_SIDE_CARD_RENAME, _ui->side.card_note);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_card_delete(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    kana_side_card(_ui, KANA_SIDE_CARD_DELETE, _ui->side.card_note);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A folder's new folder: made in it, and named straight away.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_card_add_folder(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    c8 _name[KANA_NOTE_NAME];
    kana_notes_new_name(_ui->app->notes, KANA_NOTE_FOLDER, _name, sizeof(_name));
    const u32 _id = kana_notes_add(_ui->app->notes, KANA_NOTE_FOLDER, _ui->side.card_note, _name);
    kana_side_card(_ui, _id != 0 ? KANA_SIDE_CARD_RENAME : KANA_SIDE_CARD_NONE, _id);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A folder's new canvas: made in it, and opened.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_card_add(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ui* _ui = (kana_ui*)_user_data;
    c8 _name[KANA_NOTE_NAME];
    kana_notes_new_name(_ui->app->notes, KANA_NOTE_CANVAS, _name, sizeof(_name));
    const u32 _id = kana_notes_add(_ui->app->notes, KANA_NOTE_CANVAS, _ui->side.card_note, _name);
    kana_side_card(_ui, KANA_SIDE_CARD_NONE, 0);
    if(_id != 0) {
        kana_notes_open(_ui->app->notes, _id);
        _ui->side.open = false;
    }
    kana_ui_update(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_card_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_side_card((kana_ui*)_user_data, KANA_SIDE_CARD_NONE, 0);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Save (rename) or Delete.
RDE_INTERNAL void kana_side_card_confirm(kana_ui* _ui) {
    kana_side* _side = &_ui->side;
    if(_side->card_mode == KANA_SIDE_CARD_RENAME) {
        const usize _bytes = rde_ui_text_editor_get_byte_count(_side->note_field);
        c8*         _text  = rde_ui_text_editor_get_text(_side->note_field, 0, _bytes);
        kana_notes_rename(_ui->app->notes, _side->card_note, _text != NULL ? _text : "");
        rde_ui_text_editor_free_text(_side->note_field, _text);
    } else if(_side->card_mode == KANA_SIDE_CARD_DELETE) {
        kana_notes_remove(_ui->app->notes, _side->card_note);   // the open canvas gone: kana.c opens another
    }
    kana_side_card(_ui, KANA_SIDE_CARD_NONE, 0);
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_card_confirm(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_side_card_confirm((kana_ui*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Return in the name field: save.
RDE_INTERNAL void kana_side_on_field_submit(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    kana_side_card_confirm((kana_ui*)_user_data);
}

// The card's buttons for its mode, in a row from the right.
RDE_INTERNAL void kana_side_card_layout(kana_ui* _ui) {
    kana_side*       _side = &_ui->side;
    const rde_vec_2F _size = KANA_SIDE_NOTE_CARD;
    const b8         _folder = kana_notes_find(_ui->app->notes, _side->card_note) != NULL &&
                               kana_notes_find(_ui->app->notes, _side->card_note)->kind == KANA_NOTE_FOLDER;

    // From the right: Cancel at the end of the actions (the confirm first in its row).
    rde_ui_button* _row[6];
    u32            _count = 0;
    if(_side->card_mode == KANA_SIDE_CARD_ACTIONS) {
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
    f32 _total = KANA_SIDE_GAP * (f32)(_count - 1u);
    for(u32 _k = 0; _k < _count; _k++) {
        const c8* _text = _row[_k]->internal_label != NULL ? _row[_k]->internal_label->text : "";
        _w[_k]  = fmaxf(84.0f, kana_draw_text_width(_ui->font, (f32)KANA_KIT_FONT_SIZE, _text != NULL ? _text : "", 13.0f) + 32.0f);
        _total += _w[_k];
    }
    const f32 _room    = _size.x - 2.0f * KANA_SIDE_CARD_PAD;
    const f32 _squeeze = _total > _room ? (_room - KANA_SIDE_GAP * (f32)(_count - 1u)) / (_total - KANA_SIDE_GAP * (f32)(_count - 1u)) : 1.0f;
    f32 _x = _size.x - KANA_SIDE_CARD_PAD;
    for(u32 _k = 0; _k < _count; _k++) {
        const f32 _bw = _w[_k] * _squeeze;
        kana_kit_place(rde_ui_button_as_node(_row[_k]), (rde_vec_2F){ _x - _bw * 0.5f, KANA_SIDE_CARD_PAD + 20.0f }, (rde_vec_2F){ _bw, 40.0f });
        _x -= _bw + KANA_SIDE_GAP;
    }
    rde_ui_node_set_active(kana_kit_field_node(_side->note_field), _side->card_mode == KANA_SIDE_CARD_RENAME);
    rde_ui_node_set_active(rde_ui_label_as_node(_side->note_body), _side->card_mode != KANA_SIDE_CARD_RENAME);
}

// Opens the note card on a note in a mode (NONE closes it).
RDE_INTERNAL void kana_side_card(kana_ui* _ui, KANA_SIDE_CARD_ _mode, u32 _note) {
    kana_side*       _side = &_ui->side;
    const kana_note* _n    = kana_notes_find(_ui->app->notes, _note);
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
            const u32 _in = kana_notes_count_in(_ui->app->notes, _n->id);
            KANA_TEXTF(_body, KANA_TEXT_NOTE_FOLDER_BODY, KANA_TN(_in));
        } else {
            snprintf(_body, sizeof(_body), "%s", kana_text(KANA_TEXT_CANVAS));
        }
    } else if(_mode == KANA_SIDE_CARD_RENAME) {
        snprintf(_title, sizeof(_title), "%s", kana_text(_folder ? KANA_TEXT_NOTE_RENAME_FOLDER : KANA_TEXT_NOTE_RENAME_CANVAS));
        const usize _bytes = rde_ui_text_editor_get_byte_count(_side->note_field);
        if(_bytes > 0) {
            rde_ui_text_editor_delete_range(_side->note_field, 0, _bytes);
        }
        rde_ui_text_editor_insert_at(_side->note_field, 0, _n->name, strlen(_n->name));
        rde_ui_text_editor_select_all(_side->note_field);
    } else {
        KANA_TEXTF(_title, KANA_TEXT_NOTE_DELETE_TITLE, KANA_TS(_n->name));
        const u32 _in = _folder ? kana_notes_count_in(_ui->app->notes, _n->id) : 0u;
        if(_folder && _in > 0) {
            KANA_TEXTF(_body, KANA_TEXT_NOTE_DELETE_FOLDER, KANA_TN(_in));
        } else {
            snprintf(_body, sizeof(_body), "%s", kana_text(KANA_TEXT_NOTE_UNDONE));
        }
    }
    rde_ui_label_set_text(_side->note_title, _title);
    rde_ui_label_set_text(_side->note_body, _body);
    c8 _confirm[64];
    snprintf(_confirm, sizeof(_confirm), "%s  %s", _mode == KANA_SIDE_CARD_DELETE ? KANA_ICON_TRASH : KANA_ICON_CHECK,
             kana_text(_mode == KANA_SIDE_CARD_DELETE ? KANA_TEXT_DELETE : KANA_TEXT_SAVE));
    rde_ui_button_set_text(_side->note_confirm, _confirm);
    if(_mode == KANA_SIDE_CARD_DELETE) {
        kana_kit_button_danger(_side->note_confirm);
    } else {
        kana_kit_button_primary(_side->note_confirm);
    }
    kana_side_card_layout(_ui);

    if(_mode == KANA_SIDE_CARD_RENAME) {
        rde_ui_node_focus(rde_ui_text_editor_as_node(_side->note_field));   // the keyboard comes up
    }
}

// --- dragging a row ------------------------------------------------------------------

RDE_INTERNAL void kana_side_hide_drop(kana_side* _side) {
    rde_ui_node_set_active(rde_ui_image_as_node(_side->drop_line), false);
    rde_ui_node_set_active(rde_ui_image_as_node(_side->drop_box), false);
}

// Where the dragged row would land with the pointer at _side->drag_at, shown by
// the line (between rows, indented to the level it lands at) or the box (into a
// folder: the middle of its row). A folder never lands inside itself.
RDE_INTERNAL void kana_side_drop_target(kana_ui* _ui) {
    kana_side*       _side  = &_ui->side;
    const kana_note* _moving = kana_notes_find(_ui->app->notes, _side->drag_id);
    const f32        _pitch = KANA_SIDE_NOTE_H + KANA_SIDE_GAP;
    const f32        _top   = _side->_list_bl.y + _side->_list_size.y;
    const f32        _scroll = rde_ui_scroll_area_get_scroll(_side->notes_list).y;

    _side->drop_valid = false;
    kana_side_hide_drop(_side);
    if(_moving == NULL || _side->_row_count == 0 ||
       _side->drag_at.x < _side->_list_bl.x - 20.0f || _side->drag_at.x > _side->_list_bl.x + _side->_list_size.x + 60.0f) {
        return;
    }

    const f32 _cy = rde_math_clamp_f32(_top - _side->drag_at.y, 0.0f, _side->_list_size.y) + _scroll;   // from the content's top
    i32       _r  = (i32)floorf(_cy / _pitch);
    f32       _t  = (_cy - (f32)_r * _pitch) / KANA_SIDE_NOTE_H;   // down the row: 0 top, 1 bottom
    b8        _after_all = false;
    if(_r >= (i32)_side->_row_count) {
        _r         = (i32)_side->_row_count - 1;
        _after_all = true;
    }
    const kana_side_row* _row = &_side->_rows[_r];

    b8  _into  = false;
    u32 _depth = 0;
    u32 _line  = (u32)_r;   // the line sits at the top of this row (or past the last)
    if(!_after_all && _row->kind == KANA_NOTE_FOLDER && _t > 0.28f && _t < 0.72f) {
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
    if(_moving->kind == KANA_NOTE_FOLDER && _side->drop_parent != 0 && kana_notes_is_within(_ui->app->notes, _side->drop_parent, _moving->id)) {
        return;
    }
    if(_side->drop_before == _moving->id) {
        return;
    }
    _side->drop_valid = true;

    if(_into) {
        const f32 _indent = (f32)_row->depth * KANA_SIDE_INDENT;
        const f32 _y      = _top - ((f32)_r * _pitch - _scroll) - KANA_SIDE_NOTE_H * 0.5f;
        kana_kit_place(rde_ui_image_as_node(_side->drop_box), (rde_vec_2F){ _side->_list_bl.x + (_indent + _side->_list_size.x) * 0.5f, _y },
                           (rde_vec_2F){ _side->_list_size.x - _indent + 4.0f, KANA_SIDE_NOTE_H + 4.0f });
        rde_ui_node_set_active(rde_ui_image_as_node(_side->drop_box), true);
    } else {
        const f32 _indent = (f32)_depth * KANA_SIDE_INDENT;
        const f32 _y      = _top - ((f32)_line * _pitch - _scroll) + KANA_SIDE_GAP * 0.5f;
        if(_y > _top + KANA_SIDE_GAP || _y < _side->_list_bl.y - KANA_SIDE_GAP) {
            return;   // valid, but scrolled out of sight
        }
        kana_kit_place(rde_ui_image_as_node(_side->drop_line), (rde_vec_2F){ _side->_list_bl.x + (_indent + _side->_list_size.x) * 0.5f, _y },
                           (rde_vec_2F){ _side->_list_size.x - _indent, 3.0f });
        rde_ui_node_set_active(rde_ui_image_as_node(_side->drop_line), true);
    }
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_drag_begin(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node);
    const kana_side_note_ref* _ref  = (const kana_side_note_ref*)_user_data;
    kana_side*                _side = &_ref->ui->side;
    const kana_note*          _note = kana_notes_find(_ref->ui->app->notes, _ref->id);
    if(_note == NULL) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    _side->drag_id = _ref->id;
    _side->drag_at = _info->position;
    rde_ui_label_set_text(_side->drag_ghost_label, _note->name);
    rde_ui_node_set_active(rde_ui_image_as_node(_side->drag_ghost), true);
    kana_kit_place(rde_ui_image_as_node(_side->drag_ghost), (rde_vec_2F){ _info->position.x + 90.0f, _info->position.y }, (rde_vec_2F){ 170.0f, 36.0f });
    kana_side_drop_target(_ref->ui);
    return RDE_UI_EVENT_RESULT_CONSUME;   // the handle's: the list does not scroll with it
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_drag_move(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node);
    const kana_side_note_ref* _ref  = (const kana_side_note_ref*)_user_data;
    kana_side*                _side = &_ref->ui->side;
    if(_side->drag_id == 0) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    _side->drag_at = _info->position;
    kana_kit_place(rde_ui_image_as_node(_side->drag_ghost), (rde_vec_2F){ _info->position.x + 90.0f, _info->position.y }, (rde_vec_2F){ 170.0f, 36.0f });
    kana_side_drop_target(_ref->ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_side_on_drag_end(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node);
    const kana_side_note_ref* _ref     = (const kana_side_note_ref*)_user_data;
    kana_ui*             _ui = _ref->ui;
    kana_side*                _side    = &_ui->side;
    if(_side->drag_id == 0) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    _side->drag_at = _info->position;
    kana_side_drop_target(_ui);
    if(_side->drop_valid) {
        kana_notes_move(_ui->app->notes, _side->drag_id, _side->drop_parent, _side->drop_before);   // the list rebuilds next frame
    }
    _side->drag_id = 0;
    kana_side_hide_drop(_side);
    rde_ui_node_set_active(rde_ui_image_as_node(_side->drag_ghost), false);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// --- the list ---------------------------------------------------------------------------

// The rows in order, depth first: a folder, then (when open) what is in it.
RDE_INTERNAL void kana_side_collect_rows(const kana_notes* _notes, u32 _parent, u8 _depth, kana_side_row* _out, u32* _count) {
    const kana_note* _all = (const kana_note*)_notes->notes.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_notes->notes); _i++) {
        if(_all[_i].parent != _parent) {
            continue;
        }
        _out[(*_count)++] = (kana_side_row){ _all[_i].id, _all[_i].parent, _depth, _all[_i].kind, _all[_i].expanded };
        if(_all[_i].kind == KANA_NOTE_FOLDER && _all[_i].expanded && _depth < 32u) {
            kana_side_collect_rows(_notes, _all[_i].id, (u8)(_depth + 1u), _out, _count);
        }
    }
}

// The list, built again: each row a drag handle, the name (a folder's with +/– and
// how many canvases are in it), and "…". The open canvas is marked.
RDE_INTERNAL void kana_side_build_notes(kana_ui* _ui) {
    kana_side*        _side  = &_ui->side;
    const kana_notes* _notes = _ui->app->notes;
    const u32         _count = (u32)rde_arr_length(&_notes->notes);

    rde_ui_scroll_area_clear_contents(_side->notes_list);
    free(_side->_note_refs);
    free(_side->_rows);
    _side->_note_refs      = (kana_side_note_ref*)calloc(_count > 0 ? _count : 1u, sizeof(kana_side_note_ref));
    _side->_rows           = (kana_side_row*)calloc(_count > 0 ? _count : 1u, sizeof(kana_side_row));
    _side->_row_count      = 0;
    _side->_notes_revision = _notes->revision;
    _side->_notes_open     = _notes->open;
    _side->_notes_built    = true;
    kana_side_collect_rows(_notes, 0, 0, _side->_rows, &_side->_row_count);

    const f32 _width   = _side->_list_width;
    const f32 _content = fmaxf(1.0f, (f32)_side->_row_count * (KANA_SIDE_NOTE_H + KANA_SIDE_GAP));
    rde_ui_scroll_area_set_content_size(_side->notes_list, (rde_vec_2F){ _width, _content });
    rde_ui_node* _list = rde_ui_scroll_area_as_node(_side->notes_list);
    const kana_theme* _t = kana_theme_active();

    for(u32 _r = 0; _r < _side->_row_count; _r++) {
        const kana_side_row* _row = &_side->_rows[_r];
        const kana_note*     _n   = kana_notes_find(_notes, _row->id);
        const b8             _folder = _row->kind == KANA_NOTE_FOLDER;
        const f32            _indent = (f32)_row->depth * KANA_SIDE_INDENT;
        const f32            _y      = _content - (f32)_r * (KANA_SIDE_NOTE_H + KANA_SIDE_GAP) - KANA_SIDE_NOTE_H * 0.5f;

        _side->_note_refs[_r] = (kana_side_note_ref){ _ui, _row->id };
        kana_side_note_ref* _ref = &_side->_note_refs[_r];

        // The handle: three bars; a drag from it moves the row.
        rde_ui_image* _handle = rde_ui_image_create(NULL);
        rde_ui_node*  _h      = rde_ui_image_as_node(_handle);
        rde_ui_image_set_style(_handle, RDE_UI_STATE_NORMAL, kana_kit_style((rde_color){ 0, 0, 0, 0 }, 8.0f));
        rde_ui_node_set_blocks_input(_h, true);
        rde_ui_node_set_user_data(_h, _ref);
        rde_ui_node_set_callback(_h, RDE_UI_EVENT_MOUSE_DRAG_BEGIN, kana_side_on_drag_begin);
        rde_ui_node_set_callback(_h, RDE_UI_EVENT_MOUSE_DRAG_MOVE,  kana_side_on_drag_move);
        rde_ui_node_set_callback(_h, RDE_UI_EVENT_MOUSE_DRAG_END,   kana_side_on_drag_end);
        rde_ui_node_add_child(_list, _h);
        kana_kit_place(_h, (rde_vec_2F){ _indent + KANA_SIDE_HANDLE_W * 0.5f, _y }, (rde_vec_2F){ KANA_SIDE_HANDLE_W, KANA_SIDE_NOTE_H });
        rde_ui_label* _dots6 = kana_side_label(_ui, _h, KANA_ICON_GRIP_V, 14.0f);
        rde_ui_label_set_font(_dots6, _ui->font_icons != NULL ? _ui->font_icons : _ui->font);
        rde_ui_label_set_alignment(_dots6, RDE_UI_LABEL_H_ALIGN_CENTER, RDE_UI_LABEL_V_ALIGN_MIDDLE);
        rde_ui_label_set_color(_dots6, _t->grip);
        const f32 _back = kana_kit_icon_bearing(KANA_ICON_GRIP_V) * 14.0f * KANA_KIT_EM;   // centred
        kana_kit_place(rde_ui_label_as_node(_dots6), (rde_vec_2F){ KANA_SIDE_HANDLE_W * 0.5f - _back, KANA_SIDE_NOTE_H * 0.5f }, (rde_vec_2F){ KANA_SIDE_HANDLE_W, KANA_SIDE_NOTE_H });

        c8 _label[KANA_NOTE_NAME + 32];
        if(_folder) {
            snprintf(_label, sizeof(_label), "%s  (%u)", _n->name, kana_notes_count_in(_notes, _n->id));
        } else {
            snprintf(_label, sizeof(_label), "%s", _n->name);
        }

        const f32        _x0       = _indent + KANA_SIDE_HANDLE_W + KANA_SIDE_GAP;
        const rde_vec_2F _row_size = { _width - _x0 - KANA_SIDE_DOTS_W - KANA_SIDE_GAP, KANA_SIDE_NOTE_H };
        rde_ui_button*   _button   = kana_kit_button(_list, _label, kana_side_on_note, _ui);
        rde_ui_button_set_on_click(_button, kana_side_on_note, _ref);
        kana_kit_place(rde_ui_button_as_node(_button), (rde_vec_2F){ _x0 + _row_size.x * 0.5f, _y }, _row_size);
        kana_kit_icon(_button, _folder ? (_row->expanded ? KANA_ICON_FOLDER_OPEN : KANA_ICON_FOLDER) : KANA_ICON_NOTE, KANA_KIT_ICON_LEFT, 15.0f);
        if(!_folder && _row->id == _notes->open) {
            kana_kit_button_selected(_button);   // the canvas on the page
        } else {
            kana_kit_button_quiet(_button);
            kana_kit_icon_color(_button, _t->text_soft);
        }

        rde_ui_button* _dots = kana_kit_button(_list, "\xE2\x80\xA6", kana_side_on_note_dots, _ui);   // …
        rde_ui_button_set_on_click(_dots, kana_side_on_note_dots, _ref);
        kana_kit_icon(_dots, KANA_ICON_MORE, KANA_KIT_ICON_ONLY, 17.0f);
        kana_kit_button_quiet(_dots);
        kana_kit_place(rde_ui_button_as_node(_dots), (rde_vec_2F){ _width - KANA_SIDE_DOTS_W * 0.5f, _y }, (rde_vec_2F){ KANA_SIDE_DOTS_W, KANA_SIDE_NOTE_H });
    }
}

// --- layout ---------------------------------------------------------------------------

RDE_INTERNAL void kana_side_layout(kana_ui* _ui) {
    kana_side*       _side   = &_ui->side;
    const rde_vec_2F _screen = kana_kit_screen_size((_ui)->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_ui->window);   // left, top, right, bottom
    _side->_laid_out        = _screen;
    _side->_laid_out_insets = _insets;

    // The menu button: top-left, in the safe area.
    _side->_menu_size   = (rde_vec_2F){ KANA_SIDE_MENU_W, KANA_SIDE_MENU_H };
    _side->_menu_center = (rde_vec_2F){ (f32)_insets.x + KANA_SIDE_MARGIN + KANA_SIDE_MENU_W * 0.5f,
                                        _screen.y - (f32)_insets.y - KANA_SIDE_MARGIN * 0.5f - KANA_SIDE_MENU_H * 0.5f };
    kana_kit_place(rde_ui_button_as_node(_side->menu_button), _side->_menu_center, _side->_menu_size);

    // The backdrops cover the screen; the panel is the left edge, full height.
    const f32 _w = fminf(KANA_SIDE_WIDTH + (f32)_insets.x, _screen.x * 0.85f);
    kana_kit_modal_place(&_side->panel, _ui->window, (rde_vec_2F){ _w * 0.5f, _screen.y * 0.5f }, (rde_vec_2F){ _w, _screen.y });

    // The panel's contents, top down (panel-local), below the menu button.
    const f32 _x0 = (f32)_insets.x + KANA_SIDE_MARGIN;
    const f32 _cw = _w - _x0 - KANA_SIDE_MARGIN;
    f32       _y  = _screen.y - (f32)_insets.y - KANA_SIDE_MARGIN - KANA_SIDE_MENU_H - KANA_SIDE_MARGIN;

    #define KANA_SIDE_HEADER(_label) do { \
        kana_kit_place(rde_ui_label_as_node(_label), (rde_vec_2F){ _x0 + _cw * 0.5f, _y - KANA_SIDE_HEADER_H * 0.5f }, (rde_vec_2F){ _cw, KANA_SIDE_HEADER_H }); \
        _y -= KANA_SIDE_HEADER_H; \
    } while(0)
    #define KANA_SIDE_ROW(_button) do { \
        kana_kit_place(rde_ui_button_as_node(_button), (rde_vec_2F){ _x0 + _cw * 0.5f, _y - KANA_SIDE_ROW_H * 0.5f }, (rde_vec_2F){ _cw, KANA_SIDE_ROW_H }); \
        _y -= KANA_SIDE_ROW_H + KANA_SIDE_GAP; \
    } while(0)

    KANA_SIDE_HEADER(_side->study_label);
    KANA_SIDE_ROW(_side->kanji);
    KANA_SIDE_ROW(_side->kana);
    KANA_SIDE_ROW(_side->album);
    KANA_SIDE_ROW(_side->reviews);
    KANA_SIDE_ROW(_side->vocabulary);
    KANA_SIDE_ROW(_side->exams);
    KANA_SIDE_ROW(_side->statistics);
    _y -= KANA_SIDE_MARGIN * 0.5f;

    // Notes: the header with + Folder / + Canvas at its right, then the list down
    // to the bottom row.
    kana_kit_place(rde_ui_label_as_node(_side->notes_label), (rde_vec_2F){ _x0 + (_cw - 200.0f) * 0.5f, _y - 18.0f }, (rde_vec_2F){ _cw - 200.0f, 36.0f });
    kana_kit_place(rde_ui_button_as_node(_side->new_canvas), (rde_vec_2F){ _x0 + _cw - 48.0f, _y - 18.0f }, (rde_vec_2F){ 96.0f, 36.0f });
    kana_kit_place(rde_ui_button_as_node(_side->new_folder), (rde_vec_2F){ _x0 + _cw - 96.0f - KANA_SIDE_GAP - 48.0f, _y - 18.0f }, (rde_vec_2F){ 96.0f, 36.0f });
    _y -= 36.0f + KANA_SIDE_GAP;

    #undef KANA_SIDE_ROW
    #undef KANA_SIDE_HEADER

    // The bottom: the version on the left, Settings on the right; the tutorial
    // over them.
    const f32 _by = (f32)_insets.w + KANA_SIDE_MARGIN + 20.0f;
    const f32 _ty = _by + 20.0f + KANA_SIDE_GAP + 20.0f;
    kana_kit_place(rde_ui_button_as_node(_side->tutorial), (rde_vec_2F){ _x0 + _cw - 60.0f, _ty }, (rde_vec_2F){ 120.0f, 40.0f });
    // Rate Kana over the tutorial, where there is a store.
#if defined(RDE_PLATFORM_MOBILE)
    const f32 _ry_rate = _ty + 20.0f + KANA_SIDE_GAP + 20.0f;
    kana_kit_place(rde_ui_button_as_node(_side->rate), (rde_vec_2F){ _x0 + _cw - 60.0f, _ry_rate }, (rde_vec_2F){ 120.0f, 40.0f });
    const f32 _list_bottom = _ry_rate + 20.0f + KANA_SIDE_MARGIN;
#else
    const f32 _list_bottom = _ty + 20.0f + KANA_SIDE_MARGIN;
#endif
    const f32 _list_h      = fmaxf(KANA_SIDE_NOTE_H, _y - _list_bottom);
    kana_kit_place(rde_ui_scroll_area_as_node(_side->notes_list), (rde_vec_2F){ _x0 + _cw * 0.5f, _list_bottom + _list_h * 0.5f }, (rde_vec_2F){ _cw, _list_h });
    _side->_list_bl   = (rde_vec_2F){ _x0, _list_bottom };
    _side->_list_size = (rde_vec_2F){ _cw, _list_h };
    _side->_list_width  = _cw;
    _side->_notes_built = false;   // rows are laid out for the list's width
    kana_kit_place(rde_ui_button_as_node(_side->settings_button), (rde_vec_2F){ _x0 + _cw - 60.0f, _by }, (rde_vec_2F){ 120.0f, 40.0f });
    kana_kit_place(rde_ui_label_as_node(_side->version), (rde_vec_2F){ _x0 + (_cw - 130.0f) * 0.5f, _by }, (rde_vec_2F){ _cw - 130.0f, 40.0f });

    // Settings: a card in the middle.
    const f32 _kw = fminf(KANA_SIDE_CARD_W, _screen.x - 2.0f * KANA_SIDE_MARGIN);
    const f32 _kh = fminf(KANA_SIDE_CARD_H, _screen.y - (f32)(_insets.y + _insets.w) - 2.0f * KANA_SIDE_MARGIN);
    const f32 _m  = 20.0f;
    const f32 _lw = _kw - 2.0f * _m;
    kana_kit_modal_place(&_side->settings, _ui->window, (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.5f }, (rde_vec_2F){ _kw, _kh });
    kana_kit_place(rde_ui_label_as_node(_side->settings_title), (rde_vec_2F){ _m + _lw * 0.5f, _kh - 36.0f }, (rde_vec_2F){ _lw, 44.0f });

    // The theme: a header, then one button per theme across the card.
    f32 _ry = _kh - 84.0f;
    kana_kit_place(rde_ui_label_as_node(_side->theme_label), (rde_vec_2F){ _m + _lw * 0.5f, _ry }, (rde_vec_2F){ _lw, 30.0f });
    _ry -= 42.0f;
    const f32 _tw = (_lw - KANA_SIDE_GAP * (f32)(KANA_THEME_COUNT - 1u)) / (f32)KANA_THEME_COUNT;
    for(u32 _i = 0; _i < KANA_THEME_COUNT; _i++) {
        kana_kit_place(rde_ui_button_as_node(_side->themes[_i]), (rde_vec_2F){ _m + (f32)_i * (_tw + KANA_SIDE_GAP) + _tw * 0.5f, _ry },
                           (rde_vec_2F){ _tw, 40.0f });
    }
    // The language: a header, then a flag and its name for each.
    _ry -= 50.0f;
    kana_kit_place(rde_ui_label_as_node(_side->language_label), (rde_vec_2F){ _m + _lw * 0.5f, _ry }, (rde_vec_2F){ _lw, 30.0f });
    _ry -= 48.0f;
    const f32 _gw = (_lw - KANA_SIDE_GAP * (f32)(KANA_TEXT_LANGUAGES - 1u)) / (f32)KANA_TEXT_LANGUAGES;
    for(u32 _i = 0; _i < KANA_TEXT_LANGUAGES; _i++) {
        kana_kit_place(rde_ui_button_as_node(_side->languages[_i]), (rde_vec_2F){ _m + (f32)_i * (_gw + KANA_SIDE_GAP) + _gw * 0.5f, _ry },
                           (rde_vec_2F){ _gw, 62.0f });
    }
    _ry -= 66.0f;
    kana_kit_place(rde_ui_label_as_node(_side->width_label), (rde_vec_2F){ _m + _lw * 0.25f, _ry }, (rde_vec_2F){ _lw * 0.5f, 44.0f });
    kana_kit_place(rde_ui_button_as_node(_side->width_even), (rde_vec_2F){ _kw - _m - 186.0f, _ry }, (rde_vec_2F){ 120.0f, 40.0f });
    kana_kit_place(rde_ui_button_as_node(_side->width_pressure), (rde_vec_2F){ _kw - _m - 60.0f, _ry }, (rde_vec_2F){ 120.0f, 40.0f });
    _ry -= 54.0f;
    kana_kit_place(rde_ui_label_as_node(_side->paper_label), (rde_vec_2F){ _m + (_lw - 318.0f) * 0.5f, _ry }, (rde_vec_2F){ _lw - 318.0f, 44.0f });
    for(u32 _i = 0; _i < KANA_PAPER_SIZE_COUNT; _i++) {
        const f32 _from_right = (f32)(KANA_PAPER_SIZE_COUNT - 1u - _i) * (100.0f + KANA_SIDE_GAP);
        kana_kit_place(rde_ui_button_as_node(_side->paper_sizes[_i]), (rde_vec_2F){ _kw - _m - 50.0f - _from_right, _ry }, (rde_vec_2F){ 100.0f, 40.0f });
    }
    _ry -= 56.0f;
    // Handwriting: a header, ML Kit on or off, and its model's state (Download / Retry at its right).
    kana_kit_place(rde_ui_label_as_node(_side->hand_label), (rde_vec_2F){ _m + _lw * 0.5f, _ry }, (rde_vec_2F){ _lw, 30.0f });
    _ry -= 40.0f;
    kana_kit_place(rde_ui_label_as_node(_side->mlkit_label), (rde_vec_2F){ _m + _lw * 0.35f, _ry }, (rde_vec_2F){ _lw * 0.7f, 44.0f });
    kana_kit_place(rde_ui_button_as_node(_side->mlkit_toggle), (rde_vec_2F){ _kw - _m - 60.0f, _ry }, (rde_vec_2F){ 120.0f, 40.0f });
    _ry -= 46.0f;
    kana_kit_place(rde_ui_label_as_node(_side->mlkit_status), (rde_vec_2F){ _m + (_lw - 130.0f) * 0.5f, _ry }, (rde_vec_2F){ _lw - 130.0f, 44.0f });
    kana_kit_place(rde_ui_button_as_node(_side->mlkit_download), (rde_vec_2F){ _kw - _m - 60.0f, _ry }, (rde_vec_2F){ 120.0f, 40.0f });
    _ry -= 52.0f;
    kana_kit_place(rde_ui_label_as_node(_side->about_label), (rde_vec_2F){ _m + _lw * 0.5f, _ry }, (rde_vec_2F){ _lw, 34.0f });

    const f32 _text_top    = _ry - 20.0f;
    const f32 _text_bottom = _m + 44.0f + 12.0f;
    kana_kit_place(rde_ui_label_as_node(_side->about_text), (rde_vec_2F){ _m + _lw * 0.5f, (_text_top + _text_bottom) * 0.5f },
                       (rde_vec_2F){ _lw, fmaxf(20.0f, _text_top - _text_bottom) });
    kana_kit_place(rde_ui_button_as_node(_side->settings_close), (rde_vec_2F){ _kw - _m - 60.0f, _m + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
    kana_kit_place(rde_ui_button_as_node(_side->licences_button), (rde_vec_2F){ _kw - _m - 186.0f, _m + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
    kana_kit_place(rde_ui_button_as_node(_side->data_button), (rde_vec_2F){ _kw - _m - 312.0f, _m + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });

    // Your data: the same card's size, over Settings — the title, what Kana
    // does with your data, then the answer line, Export / Import (or Replace /
    // Cancel) and Close at the bottom.
    kana_kit_modal_place(&_side->data, _ui->window, (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.5f }, (rde_vec_2F){ _kw, _kh });
    kana_kit_place(rde_ui_label_as_node(_side->data_title), (rde_vec_2F){ _m + _lw * 0.5f, _kh - 36.0f }, (rde_vec_2F){ _lw, 44.0f });
    {
        const f32 _close_y   = _m + 22.0f;
        const f32 _status_lo = _close_y + 22.0f + 12.0f;
        const f32 _status_hi = _status_lo + 84.0f;
        const f32 _row_y     = _status_hi + 12.0f + 22.0f;
        const f32 _text_lo   = _row_y + 22.0f + 18.0f;
        const f32 _text_hi   = _kh - 66.0f;
        kana_kit_place(rde_ui_label_as_node(_side->data_text), (rde_vec_2F){ _m + _lw * 0.5f, (_text_lo + _text_hi) * 0.5f },
                           (rde_vec_2F){ _lw, fmaxf(20.0f, _text_hi - _text_lo) });
        const f32 _bw = fminf(200.0f, (_lw - KANA_SIDE_GAP) * 0.5f);
        kana_kit_place(rde_ui_button_as_node(_side->data_export), (rde_vec_2F){ _m + _bw * 0.5f, _row_y }, (rde_vec_2F){ _bw, 44.0f });
        kana_kit_place(rde_ui_button_as_node(_side->data_import), (rde_vec_2F){ _m + _bw * 1.5f + KANA_SIDE_GAP, _row_y }, (rde_vec_2F){ _bw, 44.0f });
        kana_kit_place(rde_ui_button_as_node(_side->data_replace), (rde_vec_2F){ _m + _bw * 0.5f, _row_y }, (rde_vec_2F){ _bw, 44.0f });
        kana_kit_place(rde_ui_button_as_node(_side->data_cancel), (rde_vec_2F){ _m + _bw * 1.5f + KANA_SIDE_GAP, _row_y }, (rde_vec_2F){ _bw, 44.0f });
        kana_kit_place(rde_ui_label_as_node(_side->data_status), (rde_vec_2F){ _m + _lw * 0.5f, (_status_lo + _status_hi) * 0.5f },
                           (rde_vec_2F){ _lw, _status_hi - _status_lo });
        kana_kit_place(rde_ui_button_as_node(_side->data_close), (rde_vec_2F){ _kw - _m - 60.0f, _close_y }, (rde_vec_2F){ 120.0f, 44.0f });
    }

    // Licences: the same card's size, over Settings: the title, the documents,
    // the text scrolling between them and Close.
    kana_kit_modal_place(&_side->licences, _ui->window, (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.5f }, (rde_vec_2F){ _kw, _kh });
    kana_kit_place(rde_ui_label_as_node(_side->licences_title), (rde_vec_2F){ _m + _lw * 0.5f, _kh - 36.0f }, (rde_vec_2F){ _lw, 44.0f });
    const f32 _dw = (_lw - KANA_SIDE_GAP * (f32)(KANA_SIDE_LICENCE_DOCS - 1u)) / (f32)KANA_SIDE_LICENCE_DOCS;
    for(u32 _i = 0; _i < KANA_SIDE_LICENCE_DOCS; _i++) {
        kana_kit_place(rde_ui_button_as_node(_side->licences_docs[_i]), (rde_vec_2F){ _m + (f32)_i * (_dw + KANA_SIDE_GAP) + _dw * 0.5f, _kh - 88.0f },
                           (rde_vec_2F){ _dw, 40.0f });
    }
    const f32 _lt_top    = _kh - 116.0f;
    const f32 _lt_bottom = _m + 44.0f + 12.0f;
    _side->_licence_width = _lw;
    kana_kit_place(rde_ui_scroll_area_as_node(_side->licences_text), (rde_vec_2F){ _m + _lw * 0.5f, (_lt_top + _lt_bottom) * 0.5f },
                       (rde_vec_2F){ _lw, fmaxf(40.0f, _lt_top - _lt_bottom) });
    kana_kit_place(rde_ui_button_as_node(_side->licences_close), (rde_vec_2F){ _kw - _m - 60.0f, _m + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
    if(_side->licences_open) {
        kana_side_licence_load(_ui, _side->licences_doc);   // new width: the lines again
    }

    // The note card: a title, a line (or the name field), buttons at the bottom.
    const rde_vec_2F _nc = KANA_SIDE_NOTE_CARD;
    kana_kit_modal_place(&_side->note, _ui->window, (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.6f }, _nc);
    kana_kit_place(rde_ui_label_as_node(_side->note_title), (rde_vec_2F){ _nc.x * 0.5f, _nc.y - KANA_SIDE_CARD_PAD - 14.0f }, (rde_vec_2F){ _nc.x - 2.0f * KANA_SIDE_CARD_PAD, 32.0f });
    kana_kit_place(rde_ui_label_as_node(_side->note_body), (rde_vec_2F){ _nc.x * 0.5f, _nc.y - KANA_SIDE_CARD_PAD - 54.0f }, (rde_vec_2F){ _nc.x - 2.0f * KANA_SIDE_CARD_PAD, 44.0f });
    kana_kit_place(kana_kit_field_node(_side->note_field), (rde_vec_2F){ _nc.x * 0.5f, _nc.y - KANA_SIDE_CARD_PAD - 62.0f }, (rde_vec_2F){ _nc.x - 2.0f * KANA_SIDE_CARD_PAD, 44.0f });
    if(_side->card_mode != KANA_SIDE_CARD_NONE) {
        kana_side_card_layout(_ui);
    }
}

// --- lifetime ---------------------------------------------------------------------------

// The flags, loaded once (they outlive a rebuilt UI).
RDE_INTERNAL rde_texture* kana_side_flags[KANA_TEXT_LANGUAGES];

// A language's button: its flag at the top, its name under it.
RDE_INTERNAL void kana_side_flag(rde_ui_button* _button, u32 _language) {
    if(kana_side_flags[_language] == NULL) {
        kana_side_flags[_language] = rde_texture_load(KANA_TEXT_LANGUAGE_LIST[_language].flag, NULL);
    }
    rde_ui_image* _flag = rde_ui_image_create(NULL);
    rde_ui_node*  _n    = rde_ui_image_as_node(_flag);
    rde_ui_node_set_raycast_target(_n, false);
    rde_ui_node_set_interactable(_n, false);
    rde_ui_style _s = rde_ui_style_default();
    _s.texture      = kana_side_flags[_language];
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
        rde_ui_label_set_font_scale(_button->internal_label, 11.0f / (f32)KANA_KIT_FONT_SIZE);
    }
}

void kana_side_forget(kana_ui* _ui) {
    kana_side* _side = &_ui->side;
    free(_side->_note_refs);
    free(_side->_rows);
    _side->_note_refs = NULL;
    _side->_rows      = NULL;
    kana_side_licence_free(_side);
}

void kana_side_create(kana_ui* _ui, rde_ui_node* _root) {
    kana_side* _side = &_ui->side;
    memset(_side, 0, sizeof(*_side));

    // The panel over its backdrop; the menu button goes over both (added after).
    kana_kit_modal_create(&_side->panel, _root, kana_side_on_backdrop, _ui);
    rde_ui_node* _panel = rde_ui_image_as_node(_side->panel.card);

    _side->study_label = kana_side_label(_ui, _panel, kana_text(KANA_TEXT_SIDE_STUDY), KANA_SIDE_HEADER_PX);
    // The screens it opens: each button's ref says which.
    const u32 _opens[KANA_SIDE_SCREENS] = { KANA_SCREEN_BROWSE, KANA_SCREEN_CHART, KANA_SCREEN_ALBUM, KANA_SCREEN_EXAM, KANA_SIDE_REVIEWS,
                                                      KANA_SCREEN_VOCAB, KANA_SCREEN_STATS };
    for(u32 _i = 0; _i < KANA_SIDE_SCREENS; _i++) {
        _side->screen_refs[_i] = (kana_side_theme_ref){ _ui, _opens[_i] };
    }
    _side->kanji       = kana_kit_button(_panel, kana_text(KANA_TEXT_KANJI), kana_side_on_screen, &_side->screen_refs[0]);
    _side->kana        = kana_kit_button(_panel, kana_text(KANA_TEXT_KANA), kana_side_on_screen, &_side->screen_refs[1]);
    _side->album       = kana_kit_button(_panel, kana_text(KANA_TEXT_ALBUM), kana_side_on_screen, &_side->screen_refs[2]);
    _side->exams       = kana_kit_button(_panel, kana_text(KANA_TEXT_EXAMS), kana_side_on_screen, &_side->screen_refs[3]);
    _side->reviews     = kana_kit_button(_panel, kana_text(KANA_TEXT_REVIEWS), kana_side_on_screen, &_side->screen_refs[4]);
    _side->_reviews_shown = UINT32_MAX;
    _side->vocabulary  = kana_kit_button(_panel, kana_text(KANA_TEXT_VOCAB), kana_side_on_screen, &_side->screen_refs[5]);
    _side->statistics  = kana_kit_button(_panel, kana_text(KANA_TEXT_STATISTICS), kana_side_on_screen, &_side->screen_refs[6]);
    // The rows' icons: the Japanese ones as characters.
    kana_kit_icon(_side->kanji, "\xE5\xAD\x97", KANA_KIT_ICON_LEFT, 15.0f);        // 字
    kana_kit_icon(_side->kana, "\xE3\x81\x82", KANA_KIT_ICON_LEFT, 15.0f);         // あ
    kana_kit_icon(_side->album, KANA_ICON_BOOKS, KANA_KIT_ICON_LEFT, 16.0f);
    kana_kit_icon(_side->exams, "\xE8\xA9\xA6", KANA_KIT_ICON_LEFT, 15.0f);        // 試
    kana_kit_icon(_side->reviews, "\xE5\xBE\xA9", KANA_KIT_ICON_LEFT, 15.0f);      // 復
    kana_kit_icon(_side->vocabulary, "\xE8\xAA\x9E", KANA_KIT_ICON_LEFT, 15.0f);   // 語
    kana_kit_icon(_side->statistics, KANA_ICON_CHART_LINE, KANA_KIT_ICON_LEFT, 16.0f);
    if(!kana_browse_available(_ui->app->browse)) { kana_kit_set_enabled(_side->kanji, false); }   // no character data
    if(kana_chart_count(_ui->app->chart) == 0)   { kana_kit_set_enabled(_side->kana, false); }
    if(_ui->app->album->db == NULL)              { kana_kit_set_enabled(_side->album, false); }
    if(!kana_browse_available(_ui->app->browse)) { kana_kit_set_enabled(_side->exams, false); kana_kit_set_enabled(_side->reviews, false); }

    _side->notes_label = kana_side_label(_ui, _panel, kana_text(KANA_TEXT_SIDE_NOTES), KANA_SIDE_HEADER_PX);
    _side->new_folder  = kana_kit_button(_panel, kana_text(KANA_TEXT_FOLDER), kana_side_on_new_folder, _ui);
    _side->new_canvas  = kana_kit_button(_panel, kana_text(KANA_TEXT_CANVAS), kana_side_on_new_canvas, _ui);
    kana_kit_icon(_side->new_folder, KANA_ICON_FOLDER_ADD, KANA_KIT_ICON_LEFT, 14.0f);
    kana_kit_icon(_side->new_canvas, KANA_ICON_FILE_ADD, KANA_KIT_ICON_LEFT, 14.0f);
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
    _side->drag_ghost_label = kana_side_label(_ui, rde_ui_image_as_node(_side->drag_ghost), "", 13.0f);
    kana_kit_place(rde_ui_label_as_node(_side->drag_ghost_label), (rde_vec_2F){ 85.0f, 18.0f }, (rde_vec_2F){ 150.0f, 36.0f });

    c8 _version[64];
    snprintf(_version, sizeof(_version), "Kana %s", KANA_VERSION);
    _side->version         = kana_side_label(_ui, _panel, _version, 11.0f);
    _side->settings_button = kana_kit_button(_panel, kana_text(KANA_TEXT_SETTINGS), kana_side_on_settings, _ui);
    kana_kit_icon(_side->settings_button, KANA_ICON_SETTINGS, KANA_KIT_ICON_LEFT, 15.0f);
    _side->tutorial = kana_kit_button(_panel, kana_text(KANA_TEXT_TUTORIAL), kana_side_on_tutorial, _ui);
    kana_kit_icon(_side->tutorial, KANA_ICON_INFO, KANA_KIT_ICON_LEFT, 15.0f);
    _side->rate = kana_kit_button(_panel, kana_text(KANA_TEXT_RATE), kana_side_on_rate, _ui);
    kana_kit_icon(_side->rate, KANA_ICON_STAR, KANA_KIT_ICON_LEFT, 15.0f);
#if !defined(RDE_PLATFORM_MOBILE)
    rde_ui_node_set_active(rde_ui_button_as_node(_side->rate), false);   // no store here
#endif

    // The menu button: an icon on a small card of its own, over the page.
    _side->menu_button = kana_kit_button(_root, kana_text(KANA_TEXT_MENU), kana_side_on_menu, _ui);
    kana_kit_icon(_side->menu_button, KANA_ICON_MENU, KANA_KIT_ICON_ONLY, 17.0f);

    // Settings, over everything.
    kana_kit_modal_create(&_side->settings, _root, kana_side_on_settings_close, _ui);
    rde_ui_node* _card = rde_ui_image_as_node(_side->settings.card);

    _side->settings_title = kana_side_label(_ui, _card, kana_text(KANA_TEXT_SETTINGS), KANA_SIDE_TITLE_PX);
    _side->theme_label    = kana_side_label(_ui, _card, kana_text(KANA_TEXT_SETTINGS_THEME), KANA_SIDE_HEADER_PX);
    for(u32 _i = 0; _i < KANA_THEME_COUNT; _i++) {
        _side->theme_refs[_i] = (kana_side_theme_ref){ _ui, _i };
        _side->themes[_i]     = kana_kit_button(_card, kana_text((KANA_TEXT_)(KANA_TEXT_THEME_PAPER + _i)), kana_side_on_theme, _ui);   // KANA_THEME_ order
        rde_ui_button_set_on_click(_side->themes[_i], kana_side_on_theme, &_side->theme_refs[_i]);
    }
    _side->language_label = kana_side_label(_ui, _card, kana_text(KANA_TEXT_SETTINGS_LANGUAGE), KANA_SIDE_HEADER_PX);
    for(u32 _i = 0; _i < KANA_TEXT_LANGUAGES; _i++) {
        _side->language_refs[_i] = (kana_side_theme_ref){ _ui, _i };
        _side->languages[_i]     = kana_kit_button(_card, KANA_TEXT_LANGUAGE_LIST[_i].name, kana_side_on_language, _ui);
        rde_ui_button_set_on_click(_side->languages[_i], kana_side_on_language, &_side->language_refs[_i]);
        kana_side_flag(_side->languages[_i], _i);
    }
    _side->width_label    = kana_side_label(_ui, _card, kana_text(KANA_TEXT_SETTINGS_PEN_WIDTH), KANA_SIDE_ROW_PX);
    _side->width_even     = kana_kit_button(_card, kana_text(KANA_TEXT_SETTINGS_EVEN), kana_side_on_width_even, _ui);
    _side->width_pressure = kana_kit_button(_card, kana_text(KANA_TEXT_SETTINGS_PRESSURE), kana_side_on_width_pressure, _ui);
    _side->paper_label  = kana_side_label(_ui, _card, kana_text(KANA_TEXT_SETTINGS_PAPER), KANA_SIDE_ROW_PX);
    _side->paper_sizes[KANA_PAPER_SMALL]  = kana_kit_button(_card, kana_text(KANA_TEXT_SIZE_SMALL), kana_side_on_paper_small, _ui);
    _side->paper_sizes[KANA_PAPER_MEDIUM] = kana_kit_button(_card, kana_text(KANA_TEXT_SIZE_MEDIUM), kana_side_on_paper_medium, _ui);
    _side->paper_sizes[KANA_PAPER_LARGE]  = kana_kit_button(_card, kana_text(KANA_TEXT_SIZE_LARGE), kana_side_on_paper_large, _ui);
    _side->about_label    = kana_side_label(_ui, _card, kana_text(KANA_TEXT_SETTINGS_ABOUT), KANA_SIDE_HEADER_PX);

    _side->hand_label     = kana_side_label(_ui, _card, kana_text(KANA_TEXT_SETTINGS_HANDWRITING), KANA_SIDE_HEADER_PX);
    _side->mlkit_label    = kana_side_label(_ui, _card, kana_text(KANA_TEXT_SETTINGS_MLKIT), KANA_SIDE_ROW_PX);
    _side->mlkit_toggle   = kana_kit_button(_card, kana_text(KANA_TEXT_ON), kana_side_on_mlkit, _ui);
    _side->mlkit_status   = kana_side_label(_ui, _card, "", KANA_SIDE_SMALL_PX);
    rde_ui_label_set_wrap(_side->mlkit_status, true);
    _side->mlkit_download = kana_kit_button(_card, kana_text(KANA_TEXT_DOWNLOAD), kana_side_on_mlkit_download, _ui);
    _side->_mlkit_shown   = -1;

    c8 _about[1536];
    KANA_TEXTF(_about, KANA_TEXT_ABOUT_BUILT, KANA_TS(KANA_VERSION), KANA_TS(__DATE__));
    snprintf(_about + strlen(_about), sizeof(_about) - strlen(_about), "\n\n%s", kana_text(KANA_TEXT_CREDITS));
    _side->about_text = kana_side_label(_ui, _card, _about, KANA_SIDE_SMALL_PX);
    rde_ui_label_set_wrap(_side->about_text, true);
    // A short screen (landscape) gives it less room: smaller rather than over the buttons.
    rde_ui_label_set_auto_fit(_side->about_text, true);
    rde_ui_label_set_auto_fit_min_scale(_side->about_text, 0.7f);
    rde_ui_label_set_alignment(_side->about_text, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_TOP);
    _side->settings_close = kana_kit_button(_card, kana_text(KANA_TEXT_CLOSE), kana_side_on_settings_close, _ui);
    _side->licences_button = kana_kit_button(_card, kana_text(KANA_TEXT_LICENCES), kana_side_on_licences, _ui);
    _side->data_button     = kana_kit_button(_card, kana_text(KANA_TEXT_DATA), kana_side_on_data, _ui);

    // Your data, over Settings: what Kana does with it, then Export and Import.
    kana_kit_modal_create(&_side->data, _root, kana_side_on_data_close, _ui);
    rde_ui_node* _dcard = rde_ui_image_as_node(_side->data.card);
    _side->data_title = kana_side_label(_ui, _dcard, kana_text(KANA_TEXT_DATA), KANA_SIDE_TITLE_PX);
    {
        // Offline; where the platform keeps a copy; how to keep one yourself.
#if defined(RDE_PLATFORM_IOS)
        const KANA_TEXT_ _backup = KANA_TEXT_DATA_BACKUP_IOS;
#elif defined(RDE_PLATFORM_ANDROID)
        const KANA_TEXT_ _backup = KANA_TEXT_DATA_BACKUP_ANDROID;
#else
        const KANA_TEXT_ _backup = KANA_TEXT_DATA_BACKUP_DESKTOP;
#endif
        c8 _text[2048];
        snprintf(_text, sizeof(_text), "%s\n\n%s\n\n%s", kana_text(KANA_TEXT_DATA_OFFLINE), kana_text(_backup), kana_text(KANA_TEXT_DATA_HOW));
        _side->data_text = kana_side_label(_ui, _dcard, _text, KANA_SIDE_BODY_PX);
    }
    rde_ui_label_set_wrap(_side->data_text, true);
    rde_ui_label_set_auto_fit(_side->data_text, true);
    rde_ui_label_set_auto_fit_min_scale(_side->data_text, 0.7f);
    rde_ui_label_set_alignment(_side->data_text, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_TOP);
    _side->data_status = kana_side_label(_ui, _dcard, "", KANA_SIDE_BODY_PX);
    rde_ui_label_set_wrap(_side->data_status, true);
    rde_ui_label_set_auto_fit(_side->data_status, true);
    rde_ui_label_set_auto_fit_min_scale(_side->data_status, 0.7f);
    rde_ui_label_set_alignment(_side->data_status, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_TOP);
    _side->data_export  = kana_kit_button(_dcard, kana_text(KANA_TEXT_DATA_EXPORT), kana_side_on_data_export, _ui);
    _side->data_import  = kana_kit_button(_dcard, kana_text(KANA_TEXT_DATA_IMPORT), kana_side_on_data_import, _ui);
    _side->data_replace = kana_kit_button(_dcard, kana_text(KANA_TEXT_DATA_REPLACE), kana_side_on_data_replace, _ui);
    _side->data_cancel  = kana_kit_button(_dcard, kana_text(KANA_TEXT_CANCEL), kana_side_on_data_cancel, _ui);
    _side->data_close   = kana_kit_button(_dcard, kana_text(KANA_TEXT_CLOSE), kana_side_on_data_close, _ui);
    kana_kit_icon(_side->data_export, KANA_ICON_EXPORT, KANA_KIT_ICON_LEFT, 15.0f);
    kana_kit_icon(_side->data_import, KANA_ICON_IMPORT, KANA_KIT_ICON_LEFT, 15.0f);
    _side->_data_shown[0]      = 1;   // not yet: shown on the first update
    _side->_data_confirm_shown = true;

    // Licences, over Settings.
    kana_kit_modal_create(&_side->licences, _root, kana_side_on_licences_close, _ui);
    rde_ui_node* _lcard = rde_ui_image_as_node(_side->licences.card);
    _side->licences_title = kana_side_label(_ui, _lcard, kana_text(KANA_TEXT_LICENCES), KANA_SIDE_TITLE_PX);
    for(u32 _i = 0; _i < KANA_SIDE_LICENCE_DOCS; _i++) {
        _side->licences_refs[_i] = (kana_side_theme_ref){ _ui, _i };
        _side->licences_docs[_i] = kana_kit_button(_lcard, kana_text(KANA_SIDE_LICENCES[_i].name), kana_side_on_licence_doc, _ui);
        rde_ui_button_set_on_click(_side->licences_docs[_i], kana_side_on_licence_doc, &_side->licences_refs[_i]);
    }
    _side->licences_text = rde_ui_scroll_area_create(NULL);
    rde_ui_scroll_area_set_bar_thickness(_side->licences_text, 4.0f);
    rde_ui_node_add_child(_lcard, rde_ui_scroll_area_as_node(_side->licences_text));
    for(u32 _k = 0; _k < KANA_SIDE_LICENCE_LINES; _k++) {
        _side->licences_lines[_k] = kana_side_label(_ui, rde_ui_scroll_area_as_node(_side->licences_text), "", KANA_SIDE_LICENCE_PX);
        rde_ui_node_set_active(rde_ui_label_as_node(_side->licences_lines[_k]), false);
    }
    _side->licences_close = kana_kit_button(_lcard, kana_text(KANA_TEXT_CLOSE), kana_side_on_licences_close, _ui);
    _side->_licence_first = -1;

    // The note card, over the panel (and a backdrop that cancels it).
    kana_kit_modal_create(&_side->note, _root, kana_side_on_card_cancel, _ui);
    rde_ui_node* _note_card = rde_ui_image_as_node(_side->note.card);
    _side->note_title = kana_side_label(_ui, _note_card, "", KANA_SIDE_CARD_TITLE_PX);
    _side->note_body  = kana_side_label(_ui, _note_card, "", KANA_SIDE_BODY_PX);
    rde_ui_label_set_wrap(_side->note_body, true);
    _side->note_field = rde_ui_text_editor_create(_ui->font, NULL);
    rde_ui_text_editor_set_multiline(_side->note_field, false);
    rde_ui_text_editor_set_font_size(_side->note_field, 14u);
    rde_ui_text_editor_set_max_chars(_side->note_field, KANA_NOTE_NAME - 8);
    rde_ui_text_editor_set_content_insets(_side->note_field, 12.0f, 8.0f, 12.0f, 8.0f);
    rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_side->note_field), _ui);
    rde_ui_text_editor_set_on_submit(_side->note_field, kana_side_on_field_submit);
    kana_kit_field_box(_note_card, _side->note_field);
    c8 _with_icon[96];
    snprintf(_with_icon, sizeof(_with_icon), KANA_ICON_DRAW "  %s", kana_text(KANA_TEXT_RENAME));
    _side->note_rename  = kana_kit_button(_note_card, _with_icon, kana_side_on_card_rename, _ui);
    snprintf(_with_icon, sizeof(_with_icon), KANA_ICON_FILE_ADD "  %s", kana_text(KANA_TEXT_CANVAS));
    _side->note_add     = kana_kit_button(_note_card, _with_icon, kana_side_on_card_add, _ui);
    snprintf(_with_icon, sizeof(_with_icon), KANA_ICON_FOLDER_ADD "  %s", kana_text(KANA_TEXT_FOLDER));
    _side->note_add_folder = kana_kit_button(_note_card, _with_icon, kana_side_on_card_add_folder, _ui);
    snprintf(_with_icon, sizeof(_with_icon), KANA_ICON_TRASH "  %s", kana_text(KANA_TEXT_DELETE));
    _side->note_delete  = kana_kit_button(_note_card, _with_icon, kana_side_on_card_delete, _ui);
    _side->note_cancel  = kana_kit_button(_note_card, kana_text(KANA_TEXT_CANCEL), kana_side_on_card_cancel, _ui);
    _side->note_confirm = kana_kit_button(_note_card, kana_text(KANA_TEXT_SAVE), kana_side_on_card_confirm, _ui);

    // All hidden until asked for (the cards are, made); the menu button shows with the page.
    rde_ui_node_set_active(rde_ui_button_as_node(_side->menu_button), false);

    kana_side_layout(_ui);
}

// --- every frame ------------------------------------------------------------------------

void kana_side_update(kana_ui* _ui, b8 _full) {
    kana_side* _side = &_ui->side;
    if(_side->panel.card == NULL) {
        return;
    }

    // Laid out again when the screen rotates — and when the safe area changes:
    // at start iOS reports none (SDL has the whole window until the view is laid
    // out), then the status bar's arrives a frame or two later.
    const rde_vec_2F _screen = kana_kit_screen_size((_ui)->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_ui->window);
    if(memcmp(&_screen, &_side->_laid_out, sizeof(rde_vec_2F)) != 0 || memcmp(&_insets, &_side->_laid_out_insets, sizeof(rde_vec_4I)) != 0) {
        kana_side_layout(_ui);
    }

    if(_full) {
        _side->open = false;          // a scene took the screen: the panel is done
    }

    kana_side_show(rde_ui_button_as_node(_side->menu_button), !_full, &_side->_shown_menu);
    // Reviews · n: counted again when a mark, an answer or the day changes.
    if(_side->open) {
        const u32 _for = kana_marks_revision() * 31u + kana_reviews_revision() * 7u + kana_reviews_today();
        if(_for != _side->_reviews_for || _side->_reviews_shown == UINT32_MAX) {
            u32       _records[KANA_REVIEW_SESSION];
            const u32 _n = kana_app_reviews_due(_ui->app, _records, KANA_REVIEW_SESSION);
            _side->_reviews_for = _for;
            if(_n != _side->_reviews_shown) {
                _side->_reviews_shown = _n;
                c8 _label[64];
                if(_n > 0) { KANA_TEXTF(_label, KANA_TEXT_REVIEWS_N, KANA_TN(_n)); }
                else       { snprintf(_label, sizeof(_label), "%s", kana_text(KANA_TEXT_REVIEWS)); }
                rde_ui_button_set_text(_side->reviews, _label);
            }
        }
    }
    kana_kit_modal_show(&_side->panel, _side->open);
    kana_kit_modal_show(&_side->settings, _side->settings_open);
    if(!_side->settings_open && _side->licences_open) {
        _side->licences_open = false;
        kana_side_licence_free(_side);
    }
    kana_kit_modal_show(&_side->licences, _side->licences_open);
    if(_side->licences_open) {
        kana_side_licence_scroll(_ui);
    }
    if(_side->settings_open) {
        kana_side_refresh_mlkit(_ui);   // the model's download moves on by itself
    }
    if(!_side->settings_open && _side->data_open) {
        _side->data_open = false;
        kana_side_data_done(_ui);
    }
    kana_kit_modal_show(&_side->data, _side->data_open);
    if(_side->data_open) {
        if(strcmp(_side->_data_shown, _side->data_message) != 0) {
            snprintf(_side->_data_shown, sizeof(_side->_data_shown), "%s", _side->data_message);
            rde_ui_label_set_text(_side->data_status, _side->data_message);
            const kana_theme* _t = kana_theme_active();
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

    const b8 _card = _side->card_mode != KANA_SIDE_CARD_NONE && _side->open;
    kana_kit_modal_show(&_side->note, _card);
    if(!_side->open) {
        _side->card_mode = KANA_SIDE_CARD_NONE;
    }

    // A drag near the list's top or bottom scrolls it.
    if(_side->drag_id != 0) {
        const f32  _top    = _side->_list_bl.y + _side->_list_size.y;
        rde_vec_2F _scroll = rde_ui_scroll_area_get_scroll(_side->notes_list);
        if(_side->drag_at.y > _top - KANA_SIDE_EDGE) {
            _scroll.y -= KANA_SIDE_AUTOSCROLL;
        } else if(_side->drag_at.y < _side->_list_bl.y + KANA_SIDE_EDGE) {
            _scroll.y += KANA_SIDE_AUTOSCROLL;
        }
        rde_ui_scroll_area_set_scroll(_side->notes_list, _scroll);
        kana_side_drop_target(_ui);
    }
    if(!_side->open && _side->drag_id != 0) {
        _side->drag_id = 0;
        kana_side_hide_drop(_side);
        rde_ui_node_set_active(rde_ui_image_as_node(_side->drag_ghost), false);
    }

    // The notes list, rebuilt when the notes changed (or the list's size did) —
    // never mid-drag: the handle being dragged would go with it.
    if(_side->open && _ui->app->notes != NULL && _side->drag_id == 0 &&
       (!_side->_notes_built || _side->_notes_revision != _ui->app->notes->revision || _side->_notes_open != _ui->app->notes->open)) {
        kana_side_build_notes(_ui);
    }
}

b8 kana_side_hit(const kana_ui* _ui, rde_vec_2F _at) {
    const kana_side* _side = &_ui->side;
    if(_side->panel.shown || _side->settings.shown || _side->note.shown || _side->licences.shown) {
        return true;
    }
    return _side->_shown_menu &&
           _at.x >= _side->_menu_center.x - _side->_menu_size.x * 0.5f && _at.x <= _side->_menu_center.x + _side->_menu_size.x * 0.5f &&
           _at.y >= _side->_menu_center.y - _side->_menu_size.y * 0.5f && _at.y <= _side->_menu_center.y + _side->_menu_size.y * 0.5f;
}

// --- the theme ----------------------------------------------------------------------------

void kana_side_apply_theme(kana_ui* _ui) {
    kana_side*        _side = &_ui->side;
    const kana_theme* _t    = kana_theme_active();
    if(_side->panel.card == NULL) {
        return;
    }

    kana_kit_modal_restyle(&_side->panel, 0.0f);
    kana_kit_modal_restyle(&_side->settings, 18.0f);

    // The menu button: a small card on the page.
    kana_kit_button_colors(_side->menu_button, _t->surface, 1.0f, _t->outline);
    kana_kit_button_round(_side->menu_button, 12.0f);

    // The panel's rows are quiet, their icons in the accent.
    rde_ui_button* const _rows[] = { _side->kanji, _side->kana, _side->album, _side->reviews, _side->vocabulary, _side->exams, _side->statistics };
    for(u32 _i = 0; _i < sizeof(_rows) / sizeof(_rows[0]); _i++) {
        kana_kit_restyle_quiet(_rows[_i]);
        kana_kit_icon_color(_rows[_i], _t->accent);
    }
    rde_ui_button* const _quiet[] = { _side->new_folder, _side->new_canvas, _side->settings_button, _side->tutorial, _side->rate };
    for(u32 _i = 0; _i < sizeof(_quiet) / sizeof(_quiet[0]); _i++) {
        kana_kit_restyle_quiet(_quiet[_i]);
    }
    rde_ui_button* const _buttons[] = { _side->width_even, _side->width_pressure, _side->settings_close,
                                        _side->paper_sizes[0], _side->paper_sizes[1], _side->paper_sizes[2],
                                        _side->mlkit_toggle, _side->mlkit_download, _side->licences_button, _side->licences_close,
                                        _side->data_button, _side->data_export, _side->data_import, _side->data_cancel, _side->data_close,
                                        _side->licences_docs[0], _side->licences_docs[1], _side->licences_docs[2], _side->licences_docs[3],
                                        _side->note_rename, _side->note_add, _side->note_add_folder, _side->note_delete, _side->note_cancel, _side->note_confirm };
    for(u32 _i = 0; _i < sizeof(_buttons) / sizeof(_buttons[0]); _i++) {
        kana_kit_restyle_button(_buttons[_i]);
    }
    kana_kit_button_primary(_side->settings_close);
    kana_kit_button_primary(_side->licences_close);
    kana_kit_button_primary(_side->data_close);
    kana_kit_button_danger(_side->data_replace);
    kana_kit_modal_restyle(&_side->data, 18.0f);
    _side->_data_shown[0] = 1;   // the answer line's colour again
    kana_kit_modal_restyle(&_side->note, 18.0f);
    kana_kit_modal_restyle(&_side->licences, 18.0f);
    rde_ui_scroll_area_set_background_color(_side->licences_text, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_track_color(_side->licences_text, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_thumb_colors(_side->licences_text, _t->grip, kana_kit_shade(_t->grip, 20), kana_kit_shade(_t->grip, 40));
    for(u32 _k = 0; _k < KANA_SIDE_LICENCE_LINES; _k++) {
        rde_ui_label_set_color(_side->licences_lines[_k], _t->button_text);
    }
    kana_kit_button_danger(_side->note_delete);
    kana_kit_style_field(_side->note_field);
    rde_ui_scroll_area_set_background_color(_side->notes_list, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_track_color(_side->notes_list, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_thumb_colors(_side->notes_list, _t->grip, kana_kit_shade(_t->grip, 20), kana_kit_shade(_t->grip, 40));
    _side->_notes_built = false;   // the rows restyle as they are built again

    rde_ui_image_set_style(_side->drop_line, RDE_UI_STATE_NORMAL, kana_kit_style(_t->select, 1.5f));
    rde_ui_style _box    = kana_kit_style(_t->select_fill, 10.0f);
    _box.border_width    = 2.0f;
    _box.border_color    = _t->select;
    rde_ui_image_set_style(_side->drop_box, RDE_UI_STATE_NORMAL, _box);
    rde_ui_style _ghost  = kana_kit_style(_t->accent, 10.0f);
    rde_ui_image_set_style(_side->drag_ghost, RDE_UI_STATE_NORMAL, _ghost);
    rde_ui_label_set_color(_side->drag_ghost_label, _t->on_accent);

    // Each theme's button previews it: its page, its text; the current one ringed.
    for(u32 _i = 0; _i < KANA_THEME_COUNT; _i++) {
        const kana_theme* _other   = kana_theme_get((KANA_THEME_)_i);
        const b8          _current = (KANA_THEME_)_i == kana_theme_index();
        kana_kit_button_colors(_side->themes[_i], _other->page, _current ? 3.0f : 1.0f, _current ? _t->accent : _other->outline);
        rde_ui_label_set_color(_side->themes[_i]->internal_label, _other->text);
    }

    rde_ui_label* const _headers[] = { _side->study_label, _side->theme_label, _side->language_label, _side->notes_label, _side->about_label, _side->hand_label };
    for(u32 _i = 0; _i < sizeof(_headers) / sizeof(_headers[0]); _i++) {
        rde_ui_label_set_color(_headers[_i], _t->text_soft);
    }
    rde_ui_label* const _texts[] = { _side->settings_title, _side->width_label, _side->paper_label, _side->note_title,
                                     _side->mlkit_label, _side->licences_title, _side->data_title, _side->data_text };
    for(u32 _i = 0; _i < sizeof(_texts) / sizeof(_texts[0]); _i++) {
        rde_ui_label_set_color(_texts[_i], _t->text);
    }
    rde_ui_label* const _soft[] = { _side->about_text, _side->note_body, _side->mlkit_status };
    for(u32 _i = 0; _i < sizeof(_soft) / sizeof(_soft[0]); _i++) {
        rde_ui_label_set_color(_soft[_i], _t->text_soft);
    }
    rde_ui_label_set_color(_side->version, _t->text_soft);

    kana_side_refresh_settings(_ui);
}
