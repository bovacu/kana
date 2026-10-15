// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/app/side_parts.h"
#include "drawing/app/ui.h"
#include "drawing/widgets/notice.h"
#include "drawing/widgets/kit.h"
#include "drawing/ink/notes.h"
#include "drawing/base/kfile.h"
#include "drawing/base/icons.h"
#include "drawing/base/text.h"
#include "drawing/base/draw.h"

// ===========================================================================
// See side.h.
//
// Everything is built once by fude_side_create, hidden, and fude_side_update shows what is open and lays it out again
// when the screen or the safe area changes. The notes list is the exception: its rows are built again whenever the
// notes change (their revision) or another canvas opens (side_list.c, which side_parts.h joins to this file). Licences
// can be very long (ML Kit's notices alone are about
// 25,000 lines), so the text is wrapped once into line starts and only the lines on screen get one of
// FUDE_SIDE_LICENCE_LINES labels, reused as it scrolls.
// ===========================================================================

// Layout, in UI units (side_parts.h has the gap and the notes' row, which the list shares).
#define FUDE_SIDE_MARGIN    16.0f
#define FUDE_SIDE_ROW_H     44.0f
#define FUDE_SIDE_HEADER_H  30.0f     // A section's title.
#define FUDE_SIDE_MENU_W    52.0f
#define FUDE_SIDE_MENU_H    44.0f
#define FUDE_SIDE_CARD_W    580.0f
#define FUDE_SIDE_CARD_H    888.0f
#define FUDE_SIDE_LICENCE_PX 11.0f    // Licences' text.
#define FUDE_SIDE_NOTE_CARD (rde_vec_2F){ 540.0f, 196.0f }
#define FUDE_SIDE_CARD_PAD  22.0f     // Inside a card.
#define FUDE_SIDE_EDGE     44.0f     // A drag this near the list's top or bottom scrolls it...
#define FUDE_SIDE_AUTOSCROLL 9.0f     // ...this many units a frame.

// The version, the credits (About) and the documents of Licences are the app's (info.h). Kana's credits, for example,
// are the short ones its data's licences require to be shown; Licences shows them in full.

// Returns how many documents Licences shows: the app's, up to FUDE_SIDE_LICENCE_DOCS.
RDE_INTERNAL u32 fude_side_licence_count(const fude_ui* _ui) {
    const u32 _n = _ui->app->info->licence_count;
    return _n < FUDE_SIDE_LICENCE_DOCS ? _n : FUDE_SIDE_LICENCE_DOCS;
}

// --- helpers ----------------------------------------------------------------------

// Makes a label as the panel's are. See the header.
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

// Shows or hides _node, touching it only when _show differs from *_shown, which is kept up to date.
RDE_INTERNAL void fude_side_show(rde_ui_node* _node, b8 _show, b8* _shown) {
    if(_show != *_shown) {
        *_shown = _show;
        rde_ui_node_set_active(_node, _show);
    }
}

// Returns how many sections the app adds to Settings (extension.h), up to FUDE_EXTENSION_SECTIONS.
RDE_INTERNAL u32 fude_side_section_count(const fude_ui* _ui) {
    const fude_extension* _ext = fude_app_ext(_ui->app);
    return _ext->sections == NULL ? 0u : _ext->section_count < FUDE_EXTENSION_SECTIONS ? _ext->section_count : FUDE_EXTENSION_SECTIONS;
}

// Shows the current choices in Settings: the app's sections, the pen's width mode, the paper size, the interface's
// size and the language.
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
    for(u32 _i = 0; _i < 3u; _i++) {
        if(fude_app_ui_size(_ui->app) == (FUDE_UI_SIZE_)(FUDE_UI_SIZE_SMALL + _i)) { fude_kit_button_selected(_side->ui_sizes[_i]); }
        else                                                                       { fude_kit_button_plain(_side->ui_sizes[_i]); }
    }
    for(u32 _i = 0; _i < FUDE_TEXT_LANGUAGES; _i++) {
        if(FUDE_TEXT_LANGUAGE_LIST[_i].language == fude_text_language()) { fude_kit_button_selected(_side->languages[_i]); }
        else                                                               { fude_kit_button_quiet(_side->languages[_i]); }
        fude_kit_button_round(_side->languages[_i], 12.0f);
    }
}

// Closes the panel; the UI shows it on its next update.
void fude_side_close(fude_ui* _ui) {
    _ui->side.open = false;
}

// --- callbacks ----------------------------------------------------------------------

// The menu button opens or closes the panel. Opening it closes the toolbar's palette and the page's context menu.
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

// A tap on the backdrop closes the panel.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_backdrop(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->side.open = false;
    fude_ui_update(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Back closes the topmost card the way its own button does, so it calls the buttons' handlers, declared here and
// defined below; they use no node or event, so NULL is passed for them.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_licences_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_data_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_faq_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_settings_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_card_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);

// Closes the topmost of Licences, Your data, FAQ & Contact, a note's card, Settings and the panel. Returns false when
// none was open.
b8 fude_side_back(fude_ui* _ui) {
    fude_side* _side = &_ui->side;
    if(_side->licences_open)                          { fude_side_on_licences_close(NULL, NULL, _ui); }
    else if(_side->data_open)                         { fude_side_on_data_close(NULL, NULL, _ui); }
    else if(_side->faq_open)                          { fude_side_on_faq_close(NULL, NULL, _ui); }
    else if(_side->card_mode != FUDE_SIDE_CARD_NONE)  { fude_side_on_card_cancel(NULL, NULL, _ui); }
    else if(_side->settings_open)                     { fude_side_on_settings_close(NULL, NULL, _ui); }
    else if(_side->open)                              { fude_side_on_backdrop(NULL, NULL, _ui); }
    else                                              { return false; }
    fude_ui_update(_ui);
    return true;
}

// Returns the app's navigation (extension.h), or NULL when it has none or it is empty.
RDE_INTERNAL const fude_extension_nav* fude_side_nav(const fude_ui* _ui) {
    const fude_extension_nav* _nav = fude_app_ext(_ui->app)->nav;
    return _nav != NULL && _nav->count > 0u ? _nav : NULL;
}

// Returns how many navigation entries there are, up to FUDE_EXTENSION_NAV.
RDE_INTERNAL u32 fude_side_nav_count(const fude_ui* _ui) {
    const fude_extension_nav* _nav = fude_side_nav(_ui);
    return _nav == NULL ? 0u : _nav->count < FUDE_EXTENSION_NAV ? _nav->count : FUDE_EXTENSION_NAV;
}

// An entry of the app's navigation was pressed (its button's ref says which): it opens what it opens over the page,
// and the panel closes, unless the entry says to stay (Kana's Reviews with none due, for example, shows a notice
// instead).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_nav(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_side_button_ref*      _ref   = (const fude_side_button_ref*)_user_data;
    fude_ui*                        _ui    = _ref->ui;
    const fude_extension_nav_entry* _entry = &fude_side_nav(_ui)->entries[_ref->value];
    if(_entry->press != NULL && !_entry->press(_ui->app, _entry->arg)) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    _ui->side.open = false;
    fude_ui_update(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A theme was chosen: everything restyles at once, and Settings stays open so themes can be compared.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_theme(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_side_button_ref* _ref = (const fude_side_button_ref*)_user_data;
    fude_theme_set((FUDE_THEME_)_ref->value);
    fude_ui_apply_theme(_ref->ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A language was chosen: it is set now, and the UI is built again in it next frame, because it cannot be rebuilt
// while its own button is being pressed (fude_ui_follow_language).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_language(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_side_button_ref* _ref = (const fude_side_button_ref*)_user_data;
    if(_ref->value < FUDE_TEXT_LANGUAGES && FUDE_TEXT_LANGUAGE_LIST[_ref->value].language != fude_text_language()) {
        fude_text_set_language(FUDE_TEXT_LANGUAGE_LIST[_ref->value].language);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Settings, at the panel's foot: closes the panel and opens Settings, scrolled to its top and showing the current
// choices.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_settings(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->side.open          = false;
    _ui->side.settings_open = true;
    rde_ui_scroll_area_set_scroll(_ui->side.settings_body, (rde_vec_2F){ 0.0f, 0.0f });   // From its top.
    fude_side_refresh_settings(_ui);
    fude_ui_update(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Settings' Close.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_settings_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->side.settings_open = false;
    fude_ui_update(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// --- Your data --------------------------------------------------------------------------

// Returns what Your data asked for, once, and clears it.
FUDE_SIDE_DATA_ fude_side_take_data_request(fude_ui* _ui) {
    const FUDE_SIDE_DATA_ _r = _ui->side.data_request;
    _ui->side.data_request = FUDE_SIDE_DATA_NONE;
    return _r;
}

// Sets the message Your data's card shows, copied (cut to fit data_message); _bad shows it in the warning colour.
void fude_side_data_message(fude_ui* _ui, const c8* _text, b8 _bad) {
    rde_snprintf(_ui->side.data_message, sizeof(_ui->side.data_message), "%s", _text != NULL ? _text : "");
    _ui->side.data_message_bad = _bad;
}

// Frees the backup kept for Replace, if any, and ends the question.
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

// Takes ownership of a picked, whole backup for Replace (freeing any kept before) and shows the question.
void fude_side_data_confirm(fude_ui* _ui, u8* _backup, usize _size, const c8* _question) {
    fude_side_data_done(_ui);
    _ui->side.data_backup      = _backup;
    _ui->side.data_backup_size = _size;
    _ui->side.data_confirming  = true;
    fude_side_data_message(_ui, _question, false);
}

// Rate: closes the panel and opens the store's page for a review. RDE opens the App Store's write-review page once
// the app's store id is set, and the rating sheet until then; on Android, Play's listing.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_rate(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->side.open = false;
    rde_mobile_open_review_page(_ui->app->info->store_id);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// The app's own button (Set up the voice, for example): the panel closes, then the button does what it does.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_app_button(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->side.open = false;
    fude_app_ext(_ui->app)->side_button->press(_ui->app);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// The app's tutorial again (Kana's welcome, for example): the panel closes, and the tutorial opens.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_tutorial(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->side.open = false;
    fude_app_ext(_ui->app)->tutorial(_ui->app);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// FAQ & Contact: its card, over the panel.

// Returns the FAQ's address: the app's own, or else the shared one (FUDE_APP_FAQ_URL). Empty means there is none.
RDE_INTERNAL const c8* fude_side_faq_url(const fude_ui* _ui) {
    return _ui->app->info->faq_url != NULL ? _ui->app->info->faq_url : FUDE_APP_FAQ_URL;
}

// FAQ & Contact opens its card.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_faq(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    ((fude_ui*)_user_data)->side.faq_open = true;
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// FAQ & Contact's Close.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_faq_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    ((fude_ui*)_user_data)->side.faq_open = false;
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Opens the FAQ's page in the browser, when there is one.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_faq_link(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const c8* _url = fude_side_faq_url((const fude_ui*)_user_data);
    if(_url[0] != 0) {
        rde_engine_open_url(_url);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Opens a new mail to the contact address, with the app's name and version as its subject. The subject is
// percent-encoded for the mailto: link, keeping only letters, digits, '.' and '-' as they are.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_faq_mail(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_ui* _ui = (const fude_ui*)_user_data;
    c8 _subject[96];
    rde_snprintf(_subject, sizeof(_subject), "%s %s", _ui->app->info->name, _ui->app->info->version);
    c8    _url[256];
    usize _n = (usize)rde_snprintf(_url, sizeof(_url), "mailto:%s?subject=", FUDE_APP_CONTACT);
    for(const c8* _p = _subject; *_p != 0 && _n + 4u < sizeof(_url); _p++) {
        const u8 _c = (u8)*_p;
        if((_c >= 'A' && _c <= 'Z') || (_c >= 'a' && _c <= 'z') || (_c >= '0' && _c <= '9') || _c == '.' || _c == '-') {
            _url[_n++] = (c8)_c;
        } else {
            _n += (usize)rde_snprintf(_url + _n, sizeof(_url) - _n, "%%%02X", _c);   // Such as a space or "!".
        }
    }
    _url[_n] = 0;
    rde_engine_open_url(_url);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Your data, in Settings: opens its card with no message.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_data(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->side.data_open = true;
    fude_side_data_message(_ui, "", false);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Your data's Close: closes the card and lets go of a picked backup.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_data_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->side.data_open = false;
    fude_side_data_done(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Export and Import ask the session, which does the work on its next update.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_data_export(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    ((fude_ui*)_user_data)->side.data_request = FUDE_SIDE_DATA_EXPORT;
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// See above.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_data_import(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    ((fude_ui*)_user_data)->side.data_request = FUDE_SIDE_DATA_IMPORT;
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Replace asks the session to put the picked backup in place of everything, only while a backup is waiting.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_data_replace(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    if(_ui->side.data_confirming) {
        _ui->side.data_request = FUDE_SIDE_DATA_REPLACE;
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Cancel lets go of the picked backup and clears the question.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_data_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    fude_side_data_done(_ui);
    fude_side_data_message(_ui, "", false);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Pen width: even, or following the pen's pressure.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_width_even(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->app->ink->width_mode = FUDE_INK_WIDTH_MODE_CONSTANT;
    fude_side_refresh_settings(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// See above.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_width_pressure(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->app->ink->width_mode = FUDE_INK_WIDTH_MODE_PRESSURE;
    fude_side_refresh_settings(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Sets the size of lines and squares, for every canvas (the toolbar's Paper chooses the paper for one canvas).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_paper_size(fude_ui* _ui, FUDE_PAPER_SIZE_ _size) {
    _ui->app->canvas->paper_size = _size;
    fude_side_refresh_settings(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}
// The paper size buttons: Small...
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_paper_small(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    return fude_side_paper_size((fude_ui*)_user_data, FUDE_PAPER_SMALL);
}
// ...Medium...
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_paper_medium(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    return fude_side_paper_size((fude_ui*)_user_data, FUDE_PAPER_MEDIUM);
}
// ...and Large.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_paper_large(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    return fude_side_paper_size((fude_ui*)_user_data, FUDE_PAPER_LARGE);
}

// The interface's size was chosen (the button's ref holds it): everything is laid out again for it next frame.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_ui_size(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_side_button_ref* _ref = (const fude_side_button_ref*)_user_data;
    _ref->ui->app->ui_size = (u8)_ref->value;
    fude_app_apply_ui_size(_ref->ui->app);
    fude_side_refresh_settings(_ref->ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// --- licences ------------------------------------------------------------------------------

// Frees the licence text and its line starts, and forgets the lines.
RDE_INTERNAL void fude_side_licence_free(fude_side* _side) {
    if(rde_arr_is_inited(&_side->_licence_text)) {   // Both are made together (fude_side_licence_load).
        rde_arr_free(&_side->_licence_text);
        rde_arr_free(&_side->_licence_starts);
    }
    _side->_licence_count  = 0;
    _side->_licence_first  = -1;
}

// Reads document _doc (its files, up to three, one after the other), cuts it into lines that fit the scroll area's
// width (at spaces where there are some), and scrolls to its top. A document none of whose files read shows a
// sentence saying so.
RDE_INTERNAL void fude_side_licence_load(fude_ui* _ui, u32 _doc) {
    fude_side* _side = &_ui->side;
    fude_side_licence_free(_side);
    _side->licences_doc = _doc;

    // The files, one after the other, each followed by a blank line. They are kept as bytes, since a file read has
    // no NUL at its end.
    rde_arr* _all = &_side->_licence_text;
    *_all                  = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
    _side->_licence_starts = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    const fude_app_licence* _licence = _doc < fude_side_licence_count(_ui) ? &_ui->app->info->licences[_doc] : NULL;   // (past the table: none)
    for(u32 _f = 0; _licence != NULL && _f < 3 && _licence->files[_f] != NULL; _f++) {
        u32 _n = 0;
        u8* _data = fude_file_read(_licence->files[_f], &_n);
        if(_data == NULL) {
            continue;
        }
        if(_n > 0u) {
            rde_memcpy(rde_arr_add_n(_all, _n), _data, _n);
        }
        rde_memcpy(rde_arr_add_n(_all, 2u), "\n\n", 2u);
        fude_file_free(_data);
    }
    if(rde_arr_length(_all) == 0u) {   // None read.
        const c8*   _missing = fude_text(FUDE_TEXT_LICENCE_MISSING);
        const usize _n       = rde_strlen(_missing);
        if(_n > 0u) {
            rde_memcpy(rde_arr_add_n(_all, _n), _missing, _n);
        }
    }
    const usize _size = rde_arr_length(_all);
    *(c8*)rde_arr_add_n(_all, 1u) = 0;   // A NUL after it, not counted in _size.
    c8* _text = (c8*)_all->memory;       // Safe to keep: it is not grown again.
    for(usize _i = 0; _i < _size; _i++) {
        if(_text[_i] == '\t' || _text[_i] == '\r' || _text[_i] == '\f') {
            _text[_i] = ' ';
        }
    }

    // Tabs, carriage returns and form feeds became spaces above. Now, how many characters a line holds, from the
    // width of an average one: a sample sentence is measured.
    const f32        _scale = FUDE_SIDE_LICENCE_PX / (f32)FUDE_KIT_FONT_SIZE;
    const c8*        _probe = "The quick brown fox jumps over the lazy dog, THE QUICK BROWN FOX 0123456789.";
    const rde_vec_2F _ruler = _ui->font != NULL ? rde_rich_text_measure(_probe, _ui->font, _scale, 100000.0f, false) : (rde_vec_2F){ 600.0f, 18.0f };
    const f32        _each  = rde_math_max_f32(1.0f, _ruler.x / (f32)rde_strlen(_probe));
    const u32        _cols  = (u32)rde_math_max_f32(20.0f, rde_math_floor_f32((_side->_licence_width - 12.0f) / _each) - 2.0f);
    _side->_licence_line_h  = rde_math_max_f32(12.0f, _ruler.y * 1.12f);

    // Each line ends at a newline, or at the last space before _cols characters (at _cols when there is none).
    // Characters are counted, not bytes. The starts get one more entry at the end, the text's size.
    usize _at = 0;
    while(_at < _size) {
        *(u32*)rde_arr_add_n(&_side->_licence_starts, 1u) = (u32)_at;
        _side->_licence_count++;
        usize _i = _at, _space = 0;
        u32   _chars = 0;
        while(_i < _size && _text[_i] != '\n' && _chars < _cols) {
            if(_text[_i] == ' ') {
                _space = _i;
            }
            _i++;
            while(_i < _size && ((u8)_text[_i] & 0xC0u) == 0x80u) {
                _i++;   // The rest of a UTF-8 character.
            }
            _chars++;
        }
        if(_i < _size && _text[_i] == '\n') {
            _at = _i + 1;
        } else if(_i < _size && _space > _at) {
            _at = _space + 1;
        } else {
            _at = _i;
        }
    }
    *(u32*)rde_arr_add_n(&_side->_licence_starts, 1u) = (u32)_size;

    const f32 _content = rde_math_max_f32(1.0f, (f32)_side->_licence_count * _side->_licence_line_h + 8.0f);
    rde_ui_scroll_area_set_content_size(_side->licences_text, (rde_vec_2F){ _side->_licence_width, _content });
    rde_ui_scroll_area_set_scroll(_side->licences_text, (rde_vec_2F){ 0.0f, 0.0f });
    for(u32 _d = 0; _d < fude_side_licence_count(_ui); _d++) {
        fude_kit_button_chip(_side->licences_docs[_d], _d == _doc);
    }
}

// Gives the labels to the lines on screen, starting two lines above the first visible one. They are placed again only
// when that first line changes, so scrolling within a line costs nothing.
RDE_INTERNAL void fude_side_licence_scroll(fude_ui* _ui) {
    fude_side* _side = &_ui->side;
    if(!rde_arr_is_inited(&_side->_licence_text)) {
        return;
    }
    const c8*  _text   = (const c8*)_side->_licence_text.memory;
    const u32* _starts = (const u32*)_side->_licence_starts.memory;
    const rde_vec_2F _scroll  = rde_ui_scroll_area_get_scroll(_side->licences_text);
    const i32        _first   = (i32)rde_math_max_f32(0.0f, rde_math_floor_f32(_scroll.y / _side->_licence_line_h) - 2.0f);
    if(_first == _side->_licence_first) {
        return;
    }
    _side->_licence_first = _first;
    const f32 _content = rde_math_max_f32(1.0f, (f32)_side->_licence_count * _side->_licence_line_h + 8.0f);
    c8        _line[1024];
    for(u32 _k = 0; _k < FUDE_SIDE_LICENCE_LINES; _k++) {
        const u32    _n     = (u32)_first + _k;
        rde_ui_node* _label = rde_ui_label_as_node(_side->licences_lines[_k]);
        if(_n >= _side->_licence_count) {
            rde_ui_node_set_active(_label, false);
            continue;
        }
        u32 _from = _starts[_n], _to = _starts[_n + 1];
        while(_to > _from && (_text[_to - 1] == '\n' || _text[_to - 1] == ' ')) {
            _to--;
        }
        const u32 _len = _to - _from < sizeof(_line) - 1 ? _to - _from : (u32)sizeof(_line) - 1;
        rde_memcpy(_line, _text + _from, _len);
        _line[_len] = 0;
        rde_ui_label_set_text(_side->licences_lines[_k], _line);
        rde_ui_node_set_active(_label, true);
        fude_kit_place(_label, (rde_vec_2F){ 6.0f + (_side->_licence_width - 12.0f) * 0.5f, _content - 4.0f - ((f32)_n + 0.5f) * _side->_licence_line_h },
                           (rde_vec_2F){ _side->_licence_width - 12.0f, _side->_licence_line_h });
    }
}

// Licences, in Settings: opens its card on the document shown last.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_licences(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->side.licences_open = true;
    fude_side_licence_load(_ui, _ui->side.licences_doc);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Opens Settings, and Licences over it on document _licences unless that is negative or past the last.
void fude_side_open_settings(fude_ui* _ui, i32 _licences) {
    _ui->side.open          = false;
    _ui->side.settings_open = true;
    rde_ui_scroll_area_set_scroll(_ui->side.settings_body, (rde_vec_2F){ 0.0f, 0.0f });   // From its top.
    fude_side_refresh_settings(_ui);
    if(_licences >= 0 && _licences < (i32)fude_side_licence_count(_ui)) {
        _ui->side.licences_open = true;
        fude_side_licence_load(_ui, (u32)_licences);
    }
}

// Licences' Close: closes the card and frees the text.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_licences_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    _ui->side.licences_open = false;
    fude_side_licence_free(&_ui->side);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A document's chip in Licences: shows that document.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_licence_doc(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_side_button_ref* _ref = (const fude_side_button_ref*)_user_data;
    fude_side_licence_load(_ref->ui, _ref->value);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// --- notes -----------------------------------------------------------------------------
// The card a row's "…" opens (fude_side_card, side_parts.h) and + Canvas and + Folder; the rows are side_list.c's.

// + Canvas: makes a new canvas at the top level, with a new name, and opens it.
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

// + Folder: makes a new folder at the top level and opens the card to name it straight away.
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

// The note card's Rename and Delete switch it to asking for the name, or for the deletion to be confirmed.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_card_rename(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    fude_side_card(_ui, FUDE_SIDE_CARD_RENAME, _ui->side.card_note);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// See above.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_card_delete(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    fude_side_card(_ui, FUDE_SIDE_CARD_DELETE, _ui->side.card_note);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A folder's New folder: makes a folder in it, and asks for its name straight away.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_card_add_folder(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    c8 _name[FUDE_NOTE_NAME];
    fude_notes_new_name(_ui->app->notes, FUDE_NOTE_FOLDER, _name, sizeof(_name));
    const u32 _id = fude_notes_add(_ui->app->notes, FUDE_NOTE_FOLDER, _ui->side.card_note, _name);
    fude_side_card(_ui, _id != 0 ? FUDE_SIDE_CARD_RENAME : FUDE_SIDE_CARD_NONE, _id);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A folder's New canvas: makes a canvas in it and opens it, closing the card and the panel.
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

// The note card's Cancel, or a tap on its backdrop: closes it, changing nothing.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_card_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_side_card((fude_ui*)_user_data, FUDE_SIDE_CARD_NONE, 0);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// The note card's Save (renaming the note to what the field says) or Delete, then the card closes.
RDE_INTERNAL void fude_side_card_confirm(fude_ui* _ui) {
    fude_side* _side = &_ui->side;
    if(_side->card_mode == FUDE_SIDE_CARD_RENAME) {
        const usize _bytes = rde_ui_text_editor_get_byte_count(_side->note_field);
        c8*         _text  = rde_ui_text_editor_get_text(_side->note_field, 0, _bytes);
        fude_notes_rename(_ui->app->notes, _side->card_note, _text != NULL ? _text : "");
        rde_ui_text_editor_free_text(_side->note_field, _text);
    } else if(_side->card_mode == FUDE_SIDE_CARD_DELETE) {
        fude_notes_remove(_ui->app->notes, _side->card_note);   // If it was open, the session opens another.
    }
    fude_side_card(_ui, FUDE_SIDE_CARD_NONE, 0);
}

// The note card's Save or Delete button.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_card_confirm(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_side_card_confirm((fude_ui*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Return in the name field saves, as Save does.
RDE_INTERNAL void fude_side_on_field_submit(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    fude_side_card_confirm((fude_ui*)_user_data);
}

// Returns the note card's size: narrower on a screen narrower than it (a phone).
RDE_INTERNAL rde_vec_2F fude_side_note_card_size(fude_ui* _ui) {
    const rde_vec_2F _screen = fude_kit_screen_size(_ui->window);
    return (rde_vec_2F){ rde_math_min_f32(FUDE_SIDE_NOTE_CARD.x, _screen.x - 2.0f * FUDE_SIDE_MARGIN), FUDE_SIDE_NOTE_CARD.y };
}

// Lays the note card out for its mode: the title, a line of text or the name field, and the mode's buttons in a row
// from the right, the others hidden.
RDE_INTERNAL void fude_side_card_layout(fude_ui* _ui) {
    fude_side*       _side = &_ui->side;
    const rde_vec_2F _size = fude_side_note_card_size(_ui);
    const b8         _folder = fude_notes_find(_ui->app->notes, _side->card_note) != NULL &&
                               fude_notes_find(_ui->app->notes, _side->card_note)->kind == FUDE_NOTE_FOLDER;

    // From the right: Cancel is last among the actions; when confirming, the confirm button comes first.
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
    // Each button is as wide as its label (at least 84 units); they are squeezed when the row would not fit.
    f32 _w[6];
    f32 _total = FUDE_SIDE_GAP * (f32)(_count - 1u);
    for(u32 _k = 0; _k < _count; _k++) {
        const c8* _text = _row[_k]->internal_label != NULL ? _row[_k]->internal_label->text : "";
        _w[_k]  = rde_math_max_f32(84.0f, fude_draw_text_width(_ui->font, (f32)FUDE_KIT_FONT_SIZE, _text != NULL ? _text : "", 13.0f) + 32.0f);
        _total += _w[_k];
    }
    const f32 _room    = _size.x - 2.0f * FUDE_SIDE_CARD_PAD;
    // Too many for one row on a narrow card (on a phone, a folder's five): Cancel and Delete stay at the bottom, and
    // the others go on a row of their own over them.
    const u32 _first   = _total > _room && _count > 3u ? 2u : _count;   // How many on the bottom row.

    // The card: a title, a line of text (or the name field), and the buttons at the bottom. It is a row taller when
    // the buttons take two rows. It sits a little above the screen's middle (at 60% of its height).
    const rde_vec_2F _screen = fude_kit_screen_size(_ui->window);
    const rde_vec_2F _nc     = { _size.x, _size.y + (_first < _count ? 40.0f + FUDE_SIDE_GAP : 0.0f) };
    fude_kit_modal_place(&_side->note, _ui->window, (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.6f }, _nc);
    fude_kit_place(rde_ui_label_as_node(_side->note_title), (rde_vec_2F){ _nc.x * 0.5f, _nc.y - FUDE_SIDE_CARD_PAD - 14.0f }, (rde_vec_2F){ _nc.x - 2.0f * FUDE_SIDE_CARD_PAD, 32.0f });
    fude_kit_place(rde_ui_label_as_node(_side->note_body), (rde_vec_2F){ _nc.x * 0.5f, _nc.y - FUDE_SIDE_CARD_PAD - 54.0f }, (rde_vec_2F){ _nc.x - 2.0f * FUDE_SIDE_CARD_PAD, 44.0f });
    fude_kit_place(fude_kit_field_node(_side->note_field), (rde_vec_2F){ _nc.x * 0.5f, _nc.y - FUDE_SIDE_CARD_PAD - 62.0f }, (rde_vec_2F){ _nc.x - 2.0f * FUDE_SIDE_CARD_PAD, 44.0f });
    for(u32 _part = 0; _part < 2u; _part++) {
        const u32 _from = _part == 0u ? 0u : _first;
        const u32 _to   = _part == 0u ? _first : _count;
        if(_from >= _to) {
            continue;
        }
        f32 _sum = FUDE_SIDE_GAP * (f32)(_to - _from - 1u);
        for(u32 _k = _from; _k < _to; _k++) {
            _sum += _w[_k];
        }
        const f32 _gaps    = FUDE_SIDE_GAP * (f32)(_to - _from - 1u);
        const f32 _squeeze = _sum > _room ? (_room - _gaps) / (_sum - _gaps) : 1.0f;
        f32       _x       = _size.x - FUDE_SIDE_CARD_PAD;
        for(u32 _k = _from; _k < _to; _k++) {
            const f32 _bw = _w[_k] * _squeeze;
            fude_kit_place(rde_ui_button_as_node(_row[_k]), (rde_vec_2F){ _x - _bw * 0.5f, FUDE_SIDE_CARD_PAD + 20.0f + (f32)_part * (40.0f + FUDE_SIDE_GAP) },
                           (rde_vec_2F){ _bw, 40.0f });
            _x -= _bw + FUDE_SIDE_GAP;
        }
    }
    rde_ui_node_set_active(fude_kit_field_node(_side->note_field), _side->card_mode == FUDE_SIDE_CARD_RENAME);
    rde_ui_node_set_active(rde_ui_label_as_node(_side->note_body), _side->card_mode != FUDE_SIDE_CARD_RENAME);
}

// Opens the note card on _note in _mode, or closes it with FUDE_SIDE_CARD_NONE (or when the note is gone): sets the
// title, the text, the name field and the confirm button for the mode, then lays it out (side_parts.h).
void fude_side_card(fude_ui* _ui, FUDE_SIDE_CARD_ _mode, u32 _note) {
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
    c8       _body[512] = "";
    if(_mode == FUDE_SIDE_CARD_ACTIONS) {
        rde_snprintf(_title, sizeof(_title), "%s", _n->name);
        if(_folder) {
            const u32 _in = fude_notes_count_in(_ui->app->notes, _n->id);
            FUDE_TEXTF(_body, FUDE_TEXT_NOTE_FOLDER_BODY, FUDE_TN(_in));
        } else {
            rde_snprintf(_body, sizeof(_body), "%s", fude_text(FUDE_TEXT_CANVAS));
        }
    } else if(_mode == FUDE_SIDE_CARD_RENAME) {
        rde_snprintf(_title, sizeof(_title), "%s", fude_text(_folder ? FUDE_TEXT_NOTE_RENAME_FOLDER : FUDE_TEXT_NOTE_RENAME_CANVAS));
        const usize _bytes = rde_ui_text_editor_get_byte_count(_side->note_field);
        if(_bytes > 0) {
            rde_ui_text_editor_delete_range(_side->note_field, 0, _bytes);
        }
        rde_ui_text_editor_insert_at(_side->note_field, 0, _n->name, rde_strlen(_n->name));
        rde_ui_text_editor_select_all(_side->note_field);
    } else {
        FUDE_TEXTF(_title, FUDE_TEXT_NOTE_DELETE_TITLE, FUDE_TS(_n->name));
        const u32 _in = _folder ? fude_notes_count_in(_ui->app->notes, _n->id) : 0u;
        if(_folder && _in > 0) {
            FUDE_TEXTF(_body, FUDE_TEXT_NOTE_DELETE_FOLDER, FUDE_TN(_in));
        } else {
            rde_snprintf(_body, sizeof(_body), "%s", fude_text(FUDE_TEXT_NOTE_UNDONE));
        }
    }
    rde_ui_label_set_text(_side->note_title, _title);
    rde_ui_label_set_text(_side->note_body, _body);
    c8 _confirm[512];
    rde_snprintf(_confirm, sizeof(_confirm), "%s  %s", _mode == FUDE_SIDE_CARD_DELETE ? FUDE_ICON_TRASH : FUDE_ICON_CHECK,
             fude_text(_mode == FUDE_SIDE_CARD_DELETE ? FUDE_TEXT_DELETE : FUDE_TEXT_SAVE));
    rde_ui_button_set_text(_side->note_confirm, _confirm);
    if(_mode == FUDE_SIDE_CARD_DELETE) {
        fude_kit_button_danger(_side->note_confirm);
    } else {
        fude_kit_button_primary(_side->note_confirm);
    }
    fude_side_card_layout(_ui);

    if(_mode == FUDE_SIDE_CARD_RENAME) {
        rde_ui_node_focus(rde_ui_text_editor_as_node(_side->note_field));   // The keyboard comes up.
    }
}

// (The notes list, its rows and dragging them are side_list.c's.)

// --- layout ---------------------------------------------------------------------------

// About's text (built with the card), measured by the layout. An rde_str on the std allocator, so an app's credits
// are never cut however long they are; made as the card is built and freed with the UI (fude_side_forget).
RDE_INTERNAL rde_str fude_side_about;

// Places Settings' rows down from _top (in its body; _m is the margin and _kw the card's width), up to About's header,
// and returns where About's text starts. A narrow card (a phone) puts a setting's name over its choices, and the
// languages in rows of three.
RDE_INTERNAL f32 fude_side_settings_rows(fude_ui* _ui, f32 _top, f32 _m, f32 _kw) {
    fude_side* _side   = &_ui->side;
    const f32  _lw     = _kw - 2.0f * _m;
    const b8   _narrow = _lw < 460.0f;

    // The theme: a header, then one button per theme across the card.
    f32 _ry = _top - 24.0f;
    fude_kit_place(rde_ui_label_as_node(_side->theme_label), (rde_vec_2F){ _m + _lw * 0.5f, _ry }, (rde_vec_2F){ _lw, 30.0f });
    _ry -= 42.0f;
    const f32 _tw = (_lw - FUDE_SIDE_GAP * (f32)(FUDE_THEME_COUNT - 1u)) / (f32)FUDE_THEME_COUNT;
    for(u32 _i = 0; _i < FUDE_THEME_COUNT; _i++) {
        fude_kit_place(rde_ui_button_as_node(_side->themes[_i]), (rde_vec_2F){ _m + (f32)_i * (_tw + FUDE_SIDE_GAP) + _tw * 0.5f, _ry },
                           (rde_vec_2F){ _tw, 40.0f });
    }
    // The language: a header, then a flag and its name for each, a short last row centred.
    _ry -= 50.0f;
    fude_kit_place(rde_ui_label_as_node(_side->language_label), (rde_vec_2F){ _m + _lw * 0.5f, _ry }, (rde_vec_2F){ _lw, 30.0f });
    _ry -= 48.0f;
    // Only the languages offered: an app that teaches no language (Sketching) has no fourth one (text.h).
    const u32 _count = fude_text_language_count();
    const u32 _per   = _narrow ? 3u : _count;
    const f32 _gw    = (_lw - FUDE_SIDE_GAP * (f32)(_per - 1u)) / (f32)_per;
    for(u32 _i = 0, _k = 0; _i < FUDE_TEXT_LANGUAGES; _i++) {
        rde_ui_node_set_active(rde_ui_button_as_node(_side->languages[_i]), fude_text_language_shown(_i));
        if(!fude_text_language_shown(_i)) {
            continue;
        }
        const u32 _row   = _k / _per;
        const u32 _in    = _count - _row * _per < _per ? _count - _row * _per : _per;
        const f32 _shift = (f32)(_per - _in) * (_gw + FUDE_SIDE_GAP) * 0.5f;
        fude_kit_place(rde_ui_button_as_node(_side->languages[_i]), (rde_vec_2F){ _m + _shift + (f32)(_k % _per) * (_gw + FUDE_SIDE_GAP) + _gw * 0.5f, _ry - (f32)_row * (62.0f + FUDE_SIDE_GAP) },
                           (rde_vec_2F){ _gw, 62.0f });
        _k++;
    }
    _ry -= (f32)((_count - 1u) / _per) * (62.0f + FUDE_SIDE_GAP);
    _ry -= 66.0f;
    if(_narrow) {
        // Each setting's name, then its choices across under it.
        fude_kit_place(rde_ui_label_as_node(_side->width_label), (rde_vec_2F){ _m + _lw * 0.5f, _ry }, (rde_vec_2F){ _lw, 30.0f });
        _ry -= 38.0f;
        const f32 _ww = (_lw - FUDE_SIDE_GAP) * 0.5f;
        fude_kit_place(rde_ui_button_as_node(_side->width_even), (rde_vec_2F){ _m + _ww * 0.5f, _ry }, (rde_vec_2F){ _ww, 40.0f });
        fude_kit_place(rde_ui_button_as_node(_side->width_pressure), (rde_vec_2F){ _m + _ww * 1.5f + FUDE_SIDE_GAP, _ry }, (rde_vec_2F){ _ww, 40.0f });
        _ry -= 50.0f;
        fude_kit_place(rde_ui_label_as_node(_side->paper_label), (rde_vec_2F){ _m + _lw * 0.5f, _ry }, (rde_vec_2F){ _lw, 30.0f });
        _ry -= 38.0f;
        const f32 _pw = (_lw - FUDE_SIDE_GAP * (f32)(FUDE_PAPER_SIZE_COUNT - 1u)) / (f32)FUDE_PAPER_SIZE_COUNT;
        for(u32 _i = 0; _i < FUDE_PAPER_SIZE_COUNT; _i++) {
            fude_kit_place(rde_ui_button_as_node(_side->paper_sizes[_i]), (rde_vec_2F){ _m + (f32)_i * (_pw + FUDE_SIDE_GAP) + _pw * 0.5f, _ry }, (rde_vec_2F){ _pw, 40.0f });
        }
        _ry -= 50.0f;
        fude_kit_place(rde_ui_label_as_node(_side->ui_size_label), (rde_vec_2F){ _m + _lw * 0.5f, _ry }, (rde_vec_2F){ _lw, 30.0f });
        _ry -= 38.0f;
        for(u32 _i = 0; _i < 3u; _i++) {
            fude_kit_place(rde_ui_button_as_node(_side->ui_sizes[_i]), (rde_vec_2F){ _m + (f32)_i * (_pw + FUDE_SIDE_GAP) + _pw * 0.5f, _ry }, (rde_vec_2F){ _pw, 40.0f });
        }
    } else {
        fude_kit_place(rde_ui_label_as_node(_side->width_label), (rde_vec_2F){ _m + _lw * 0.25f, _ry }, (rde_vec_2F){ _lw * 0.5f, 44.0f });
        fude_kit_place(rde_ui_button_as_node(_side->width_even), (rde_vec_2F){ _kw - _m - 186.0f, _ry }, (rde_vec_2F){ 120.0f, 40.0f });
        fude_kit_place(rde_ui_button_as_node(_side->width_pressure), (rde_vec_2F){ _kw - _m - 60.0f, _ry }, (rde_vec_2F){ 120.0f, 40.0f });
        _ry -= 54.0f;
        fude_kit_place(rde_ui_label_as_node(_side->paper_label), (rde_vec_2F){ _m + (_lw - 318.0f) * 0.5f, _ry }, (rde_vec_2F){ _lw - 318.0f, 44.0f });
        for(u32 _i = 0; _i < FUDE_PAPER_SIZE_COUNT; _i++) {
            const f32 _from_right = (f32)(FUDE_PAPER_SIZE_COUNT - 1u - _i) * (100.0f + FUDE_SIDE_GAP);
            fude_kit_place(rde_ui_button_as_node(_side->paper_sizes[_i]), (rde_vec_2F){ _kw - _m - 50.0f - _from_right, _ry }, (rde_vec_2F){ 100.0f, 40.0f });
        }
        _ry -= 54.0f;
        fude_kit_place(rde_ui_label_as_node(_side->ui_size_label), (rde_vec_2F){ _m + (_lw - 318.0f) * 0.5f, _ry }, (rde_vec_2F){ _lw - 318.0f, 44.0f });
        for(u32 _i = 0; _i < 3u; _i++) {
            const f32 _from_right = (f32)(2u - _i) * (100.0f + FUDE_SIDE_GAP);
            fude_kit_place(rde_ui_button_as_node(_side->ui_sizes[_i]), (rde_vec_2F){ _kw - _m - 50.0f - _from_right, _ry }, (rde_vec_2F){ 100.0f, 40.0f });
        }
    }
    _ry -= 56.0f;
    // The app's sections (Kana's Handwriting, for example), then About.
    for(u32 _i = 0; _i < fude_side_section_count(_ui); _i++) {
        _ry = fude_app_ext(_ui->app)->sections[_i].layout(_ui, _ry, _m, _kw);
    }
    fude_kit_place(rde_ui_label_as_node(_side->about_label), (rde_vec_2F){ _m + _lw * 0.5f, _ry }, (rde_vec_2F){ _lw, 34.0f });
    return _ry - 20.0f;
}

// Lays out the bottom of the panel: the version, the rows over it from FAQ & Contact up, and the notes list in the
// room left between them and _y, the bottom of the notes' header. _x0 and _cw: the rows' left edge and width.
RDE_INTERNAL void fude_side_layout_foot(fude_ui* _ui, rde_vec_4I _insets, f32 _x0, f32 _cw, f32 _y) {
    fude_side* _side = &_ui->side;
    // The bottom: the version, small, at the very bottom. Over it, from the bottom up: FAQ & Contact, Settings, the
    // tutorial (the app's, if it has one), Rate (where there is a store) and the app's own button (while it is
    // wanted), as rows like the navigation's, their icons in the same column. Where the screen is too short for that
    // and two rows of notes (landscape), the tutorial and Settings share a row. The notes' list ends above the highest
    // row, with whatever room is left. FUDE_SIDE_FOOT_W is a row's width when it shares: its label's, plus the icon.
    #define FUDE_SIDE_FOOT_W(_label) rde_math_min_f32(_cw, fude_draw_text_width(fude_kit_font(), (f32)FUDE_KIT_FONT_SIZE, (_label), FUDE_KIT_TEXT_SCALE * (f32)FUDE_KIT_FONT_SIZE) + 15.0f * FUDE_KIT_EM + 40.0f)
#if defined(RDE_PLATFORM_MOBILE)
    const b8  _rate     = true;
#else
    const b8  _rate     = false;
#endif
    const f32 _foot_h   = 40.0f;
    const f32 _vy       = (f32)_insets.w + FUDE_SIDE_MARGIN * 0.5f + 9.0f;   // The version's middle.
    const f32 _first    = _vy + 9.0f + FUDE_SIDE_GAP + _foot_h * 0.5f;          // The lowest row's middle.
    const b8  _app_b    = _side->app_button != NULL && _side->_app_button_shown;
    const u32 _stacked  = 2u + (_side->tutorial != NULL ? 1u : 0u) + (_rate ? 1u : 0u) + (_app_b ? 1u : 0u);   // FAQ & Contact, Settings, ...
    const f32 _room     = _y - (_first + (f32)(_stacked - 1u) * (_foot_h + FUDE_SIDE_GAP) + _foot_h * 0.5f + FUDE_SIDE_MARGIN);
    const b8  _compact  = _side->tutorial != NULL && _room < 2.0f * FUDE_SIDE_NOTE_H;
    // FAQ & Contact is the lowest, under Settings.
    fude_kit_place(rde_ui_button_as_node(_side->faq_button), (rde_vec_2F){ _x0 + _cw * 0.5f, _first }, (rde_vec_2F){ _cw, _foot_h });
    f32       _top      = _first + _foot_h + FUDE_SIDE_GAP;   // The highest row's middle so far.
    if(_compact) {
        const f32 _tw = FUDE_SIDE_FOOT_W(fude_text(FUDE_TEXT_TUTORIAL));
        const f32 _sw = FUDE_SIDE_FOOT_W(fude_text(FUDE_TEXT_SETTINGS));
        fude_kit_place(rde_ui_button_as_node(_side->tutorial), (rde_vec_2F){ _x0 + _tw * 0.5f, _top }, (rde_vec_2F){ _tw, _foot_h });
        fude_kit_place(rde_ui_button_as_node(_side->settings_button), (rde_vec_2F){ _x0 + _tw + FUDE_SIDE_GAP + _sw * 0.5f, _top }, (rde_vec_2F){ _sw, _foot_h });
    } else {
        fude_kit_place(rde_ui_button_as_node(_side->settings_button), (rde_vec_2F){ _x0 + _cw * 0.5f, _top }, (rde_vec_2F){ _cw, _foot_h });
        if(_side->tutorial != NULL) {
            _top += _foot_h + FUDE_SIDE_GAP;
            fude_kit_place(rde_ui_button_as_node(_side->tutorial), (rde_vec_2F){ _x0 + _cw * 0.5f, _top }, (rde_vec_2F){ _cw, _foot_h });
        }
    }
    if(_rate) {
        _top += _foot_h + FUDE_SIDE_GAP;
        fude_kit_place(rde_ui_button_as_node(_side->rate), (rde_vec_2F){ _x0 + _cw * 0.5f, _top }, (rde_vec_2F){ _cw, _foot_h });
    }
    if(_app_b) {
        _top += _foot_h + FUDE_SIDE_GAP;
        fude_kit_place(rde_ui_button_as_node(_side->app_button), (rde_vec_2F){ _x0 + _cw * 0.5f, _top }, (rde_vec_2F){ _cw, _foot_h });
    }
    #undef FUDE_SIDE_FOOT_W
    fude_kit_place(rde_ui_label_as_node(_side->version), (rde_vec_2F){ _x0 + 10.0f + (_cw - 10.0f) * 0.5f, _vy }, (rde_vec_2F){ _cw - 10.0f, 18.0f });   // Under the icons.
    const f32 _list_bottom = _top + _foot_h * 0.5f + FUDE_SIDE_MARGIN;
    const f32 _list_h      = rde_math_max_f32(0.0f, _y - _list_bottom);   // Never over the rows under it.
    fude_kit_place(rde_ui_scroll_area_as_node(_side->notes_list), (rde_vec_2F){ _x0 + _cw * 0.5f, _list_bottom + _list_h * 0.5f }, (rde_vec_2F){ _cw, rde_math_max_f32(1.0f, _list_h) });
    _side->_list_bl   = (rde_vec_2F){ _x0, _list_bottom };
    _side->_list_size = (rde_vec_2F){ _cw, _list_h };
    _side->_list_width  = _cw;
    _side->_notes_built = false;   // The rows are laid out for the list's width, so they are built again.
}

// Lays out the panel at the left edge of the screen (_screen in size, _insets its safe area), then its contents from
// the top down: the app's navigation, the notes' header, and (fude_side_layout_foot) the notes list and the rows under
// it.
RDE_INTERNAL void fude_side_layout_panel(fude_ui* _ui, rde_vec_2F _screen, rde_vec_4I _insets) {
    fude_side* _side = &_ui->side;
    // The backdrops cover the screen; the panel is at the left edge, full height, at most 85% of the screen's width.
    const f32 _w = rde_math_min_f32(FUDE_SIDE_WIDTH + (f32)_insets.x, _screen.x * 0.85f);
    fude_kit_modal_place(&_side->panel, _ui->window, (rde_vec_2F){ _w * 0.5f, _screen.y * 0.5f }, (rde_vec_2F){ _w, _screen.y });

    // The panel's contents, top down (in the panel's units), below the menu button. The two macros place a section's
    // header and a full-width row, and move _y down past it.
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

    // Notes: the header with + Folder and + Canvas at its right, then the list down to the bottom rows.
    fude_kit_place(rde_ui_label_as_node(_side->notes_label), (rde_vec_2F){ _x0 + (_cw - 200.0f) * 0.5f, _y - 18.0f }, (rde_vec_2F){ _cw - 200.0f, 36.0f });
    fude_kit_place(rde_ui_button_as_node(_side->new_canvas), (rde_vec_2F){ _x0 + _cw - 48.0f, _y - 18.0f }, (rde_vec_2F){ 96.0f, 36.0f });
    fude_kit_place(rde_ui_button_as_node(_side->new_folder), (rde_vec_2F){ _x0 + _cw - 96.0f - FUDE_SIDE_GAP - 48.0f, _y - 18.0f }, (rde_vec_2F){ 96.0f, 36.0f });
    _y -= 36.0f + FUDE_SIDE_GAP;

    #undef FUDE_SIDE_ROW
    #undef FUDE_SIDE_HEADER

    fude_side_layout_foot(_ui, _insets, _x0, _cw, _y);
}

// Lays out Settings' card under its title: the body that scrolls (its rows, then About's text) and the buttons at the
// bottom. _kw and _kh: the card's size; _m: its margin; _lw: its width inside the margins.
RDE_INTERNAL void fude_side_layout_settings_body(fude_ui* _ui, f32 _kw, f32 _kh, f32 _m, f32 _lw) {
    fude_side* _side = &_ui->side;
    // The rest scrolls between the title and the buttons. Its rows are laid out from the top of its content twice:
    // once to measure them, and once more where they end up.
    const f32 _body_lo = _m + 44.0f + 12.0f;
    const f32 _body_hi = _kh - 60.0f;
    const f32 _view_h  = rde_math_max_f32(1.0f, _body_hi - _body_lo);
    fude_kit_place(rde_ui_scroll_area_as_node(_side->settings_body), (rde_vec_2F){ _kw * 0.5f, (_body_lo + _body_hi) * 0.5f }, (rde_vec_2F){ _kw, _view_h });
    // About's text is as tall as its lines, with spare room because measuring and drawing can differ by a little.
    const u32 _about_n = fude_draw_text_wrap_lines(_ui->font, (f32)FUDE_KIT_FONT_SIZE, rde_str_to_char_ptr(&fude_side_about), FUDE_SIDE_SMALL_PX, _lw - 12.0f);
    const f32 _about_h = (f32)_about_n * FUDE_SIDE_SMALL_PX * 1.4f + 8.0f;
    const f32 _used    = _view_h - fude_side_settings_rows(_ui, _view_h, _m, _kw);
    const f32 _body_h  = rde_math_max_f32(_view_h, _used + _about_h);
    const f32 _text_top = fude_side_settings_rows(_ui, _body_h, _m, _kw);
    rde_ui_scroll_area_set_content_size(_side->settings_body, (rde_vec_2F){ _kw, _body_h });
    fude_kit_place(rde_ui_label_as_node(_side->about_text), (rde_vec_2F){ _m + _lw * 0.5f, _text_top * 0.5f }, (rde_vec_2F){ _lw, rde_math_max_f32(20.0f, _text_top) });

    // The buttons (Your data, Licences, Close): at the right, or across a narrow card.
    rde_ui_button* const _foot[] = { _side->data_button, _side->licences_button, _side->settings_close };
    const b8             _foot_across = _lw < 3.0f * 120.0f + 2.0f * FUDE_SIDE_GAP + 120.0f;
    const f32            _fw          = _foot_across ? (_lw - 2.0f * FUDE_SIDE_GAP) / 3.0f : 120.0f;
    const f32            _fx          = _foot_across ? _m : _kw - _m - 3.0f * 120.0f - 2.0f * FUDE_SIDE_GAP;
    for(u32 _i = 0; _i < 3u; _i++) {
        fude_kit_place(rde_ui_button_as_node(_foot[_i]), (rde_vec_2F){ _fx + (f32)_i * (_fw + FUDE_SIDE_GAP) + _fw * 0.5f, _m + 22.0f }, (rde_vec_2F){ _fw, 44.0f });
    }
}

// Lays out Your data's card in the middle of the screen (_screen in size), the size of Settings' card: _kw by _kh, with
// margin _m and _lw wide inside the margins.
RDE_INTERNAL void fude_side_layout_data(fude_ui* _ui, rde_vec_2F _screen, f32 _kw, f32 _kh, f32 _m, f32 _lw) {
    fude_side* _side = &_ui->side;
    // Your data: a card the same size, over Settings. From the top: the title, what the app does with the user's
    // data, then Export and Import (or Replace and Cancel, in the same places), the answer, and Close at the bottom.
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
                           (rde_vec_2F){ _lw, rde_math_max_f32(20.0f, _text_hi - _text_lo) });
        const f32 _bw = rde_math_min_f32(200.0f, (_lw - FUDE_SIDE_GAP) * 0.5f);
        fude_kit_place(rde_ui_button_as_node(_side->data_export), (rde_vec_2F){ _m + _bw * 0.5f, _row_y }, (rde_vec_2F){ _bw, 44.0f });
        fude_kit_place(rde_ui_button_as_node(_side->data_import), (rde_vec_2F){ _m + _bw * 1.5f + FUDE_SIDE_GAP, _row_y }, (rde_vec_2F){ _bw, 44.0f });
        fude_kit_place(rde_ui_button_as_node(_side->data_replace), (rde_vec_2F){ _m + _bw * 0.5f, _row_y }, (rde_vec_2F){ _bw, 44.0f });
        fude_kit_place(rde_ui_button_as_node(_side->data_cancel), (rde_vec_2F){ _m + _bw * 1.5f + FUDE_SIDE_GAP, _row_y }, (rde_vec_2F){ _bw, 44.0f });
        fude_kit_place(rde_ui_label_as_node(_side->data_status), (rde_vec_2F){ _m + _lw * 0.5f, (_status_lo + _status_hi) * 0.5f },
                           (rde_vec_2F){ _lw, _status_hi - _status_lo });
        fude_kit_place(rde_ui_button_as_node(_side->data_close), (rde_vec_2F){ _kw - _m - 60.0f, _close_y }, (rde_vec_2F){ 120.0f, 44.0f });
    }
}

// Lays out the menu button, the panel and its contents, and the cards, for the screen's size and safe area now.
RDE_INTERNAL void fude_side_layout(fude_ui* _ui) {
    fude_side*       _side   = &_ui->side;
    const rde_vec_2F _screen = fude_kit_screen_size((_ui)->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_ui->window);   // Left, top, right, bottom.
    _side->_laid_out        = _screen;
    _side->_laid_out_insets = _insets;

    // The menu button: top-left, in the safe area.
    _side->_menu_size   = (rde_vec_2F){ FUDE_SIDE_MENU_W, FUDE_SIDE_MENU_H };
    _side->_menu_center = (rde_vec_2F){ (f32)_insets.x + FUDE_SIDE_MARGIN + FUDE_SIDE_MENU_W * 0.5f,
                                        _screen.y - (f32)_insets.y - FUDE_SIDE_MARGIN * 0.5f - FUDE_SIDE_MENU_H * 0.5f };
    fude_kit_place(rde_ui_button_as_node(_side->menu_button), _side->_menu_center, _side->_menu_size);

    fude_side_layout_panel(_ui, _screen, _insets);

    // Settings: a card in the middle of the screen, its title at the top.
    const f32 _kw = rde_math_min_f32(FUDE_SIDE_CARD_W, _screen.x - 2.0f * FUDE_SIDE_MARGIN);
    const f32 _kh = rde_math_min_f32(FUDE_SIDE_CARD_H, _screen.y - (f32)(_insets.y + _insets.w) - 2.0f * FUDE_SIDE_MARGIN);
    const f32 _m  = 20.0f;
    const f32 _lw = _kw - 2.0f * _m;
    fude_kit_modal_place(&_side->settings, _ui->window, (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.5f }, (rde_vec_2F){ _kw, _kh });
    fude_kit_place(rde_ui_label_as_node(_side->settings_title), (rde_vec_2F){ _m + _lw * 0.5f, _kh - 36.0f }, (rde_vec_2F){ _lw, 44.0f });

    fude_side_layout_settings_body(_ui, _kw, _kh, _m, _lw);

    fude_side_layout_data(_ui, _screen, _kw, _kh, _m, _lw);

    // Licences: a card the same size, over Settings: the title, a chip for each document, the text scrolling between
    // them and Close, and the app's copyright by Close.
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
                       (rde_vec_2F){ _lw, rde_math_max_f32(40.0f, _lt_top - _lt_bottom) });
    fude_kit_place(rde_ui_button_as_node(_side->licences_close), (rde_vec_2F){ _kw - _m - 60.0f, _m + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
    fude_kit_place(rde_ui_label_as_node(_side->licences_copyright), (rde_vec_2F){ _m + (_lw - 132.0f) * 0.5f, _m + 22.0f }, (rde_vec_2F){ _lw - 132.0f, 44.0f });   // By Close.
    if(_side->licences_open) {
        fude_side_licence_load(_ui, _side->licences_doc);   // A new width: wrap the lines again.
    }

    // FAQ & Contact: a smaller card with the FAQ's page, then where to write. It is shorter without the FAQ's link.
    const b8  _fl = fude_side_faq_url(_ui)[0] != 0;
    const f32 _fh = rde_math_min_f32(_fl ? 300.0f : 240.0f, _kh);   // Without it: shorter.
    fude_kit_modal_place(&_side->faq, _ui->window, (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.5f }, (rde_vec_2F){ _kw, _fh });
    fude_kit_place(rde_ui_label_as_node(_side->faq_title), (rde_vec_2F){ _m + _lw * 0.5f, _fh - 36.0f }, (rde_vec_2F){ _lw, 44.0f });
    f32 _fy = _fh - 92.0f;
    if(_fl) {
        fude_kit_place(rde_ui_button_as_node(_side->faq_link), (rde_vec_2F){ _m + _lw * 0.5f, _fy }, (rde_vec_2F){ _lw, 44.0f });
        _fy -= 60.0f;
    }
    fude_kit_place(rde_ui_label_as_node(_side->faq_text), (rde_vec_2F){ _m + _lw * 0.5f, _fy }, (rde_vec_2F){ _lw, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_side->faq_mail), (rde_vec_2F){ _m + _lw * 0.5f, _fy - 46.0f }, (rde_vec_2F){ _lw, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_side->faq_close), (rde_vec_2F){ _kw - _m - 60.0f, _m + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });

    // The note card has its own layout, since its height follows its buttons' rows.
    fude_side_card_layout(_ui);
}

// --- lifetime ---------------------------------------------------------------------------

// The languages' flags, loaded once and never unloaded: they outlive a rebuilt UI.
RDE_INTERNAL rde_texture* fude_side_flags[FUDE_TEXT_LANGUAGES];

// Dresses a language's button: its flag (3:2, 30 by 20 units) at the top, and its name under it in small type.
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

// For the look harness: opens the panel and the note card for _note in _mode.
void fude_side_look_card(fude_ui* _ui, u32 _note, FUDE_SIDE_CARD_ _mode) {
    _ui->side.open = true;
    fude_side_card(_ui, _mode, _note);
}

// Asks for a new layout next frame, by forgetting the screen size it was laid out for.
void fude_side_relayout(fude_ui* _ui) {
    _ui->side._laid_out = (rde_vec_2F){ 0.0f, 0.0f };   // No screen is that size.
}

// Frees the notes list's arrays, About's text and the licence text. The widgets go with the UI canvas.
void fude_side_forget(fude_ui* _ui) {
    fude_side* _side = &_ui->side;
    if(rde_arr_is_inited(&_side->_rows)) {
        rde_arr_free(&_side->_rows);
        rde_arr_free(&_side->_note_refs);
    }
    _side->_row_count = 0;
    if(rde_str_is_inited(&fude_side_about)) {
        rde_str_free(&fude_side_about);
    }
    fude_side_licence_free(_side);
}

// Builds what the panel holds, in _panel: the app's navigation, the notes' header and list, what a drag shows over the
// list, and the rows at the bottom (the version, Settings, FAQ & Contact, the tutorial, Rate and the app's button).
RDE_INTERNAL void fude_side_create_panel_contents(fude_ui* _ui, rde_ui_node* _panel) {
    fude_side* _side = &_ui->side;
    // The app's navigation: each button's ref says which entry.
    const fude_extension_nav* _nav = fude_side_nav(_ui);
    if(_nav != NULL) {
        _side->nav_label = fude_side_label(_ui, _panel, fude_text((FUDE_TEXT_)_nav->title), FUDE_SIDE_HEADER_PX);
    }
    for(u32 _i = 0; _i < fude_side_nav_count(_ui); _i++) {
        const fude_extension_nav_entry* _entry = &_nav->entries[_i];
        _side->nav_refs[_i]    = (fude_side_button_ref){ _ui, _i };
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

    // What a drag shows, over the list: the line, the box, and the name following the pointer.
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

    rde_str _version = rde_str_new_fmt(rde_memory_allocator_get_default_std(), "%s %s", _ui->app->info->name, _ui->app->info->version);
    _side->version         = fude_side_label(_ui, _panel, rde_str_to_char_ptr(&_version), 11.0f);   // (the label keeps a copy)
    rde_str_free(&_version);
    _side->settings_button = fude_kit_button(_panel, fude_text(FUDE_TEXT_SETTINGS), fude_side_on_settings, _ui);
    fude_kit_icon(_side->settings_button, FUDE_ICON_SETTINGS, FUDE_KIT_ICON_LEFT, 15.0f);
    _side->faq_button = fude_kit_button(_panel, fude_text(FUDE_TEXT_FAQ_CONTACT), fude_side_on_faq, _ui);
    fude_kit_icon(_side->faq_button, FUDE_ICON_HINT, FUDE_KIT_ICON_LEFT, 15.0f);
    if(fude_app_ext(_ui->app)->tutorial != NULL) {
        _side->tutorial = fude_kit_button(_panel, fude_text(FUDE_TEXT_TUTORIAL), fude_side_on_tutorial, _ui);
        fude_kit_icon(_side->tutorial, FUDE_ICON_INFO, FUDE_KIT_ICON_LEFT, 15.0f);
    }
    _side->rate = fude_kit_button(_panel, fude_text(FUDE_TEXT_RATE), fude_side_on_rate, _ui);
    fude_kit_icon(_side->rate, FUDE_ICON_STAR, FUDE_KIT_ICON_LEFT, 15.0f);
#if !defined(RDE_PLATFORM_MOBILE)
    rde_ui_node_set_active(rde_ui_button_as_node(_side->rate), false);   // There is no store here.
#endif
    if(fude_app_ext(_ui->app)->side_button != NULL) {
        const fude_extension_side_button* _b = fude_app_ext(_ui->app)->side_button;
        _side->app_button = fude_kit_button(_panel, fude_text((FUDE_TEXT_)_b->text), fude_side_on_app_button, _ui);
        fude_kit_icon(_side->app_button, _b->icon, FUDE_KIT_ICON_LEFT, 15.0f);
        rde_ui_node_set_active(rde_ui_button_as_node(_side->app_button), false);   // Until wanted (fude_side_update).
    }
}

// Builds Settings under _root: its card, its title, the body that scrolls (the theme, the language, the pen's width,
// the paper's size, the interface's size, the app's sections and About), and its buttons (Close, Licences, Your data).
// It also makes About's text (fude_side_about).
RDE_INTERNAL void fude_side_create_settings(fude_ui* _ui, rde_ui_node* _root) {
    fude_side* _side = &_ui->side;
    // Settings, over everything; a tap on its backdrop closes it.
    fude_kit_modal_create(&_side->settings, _root, fude_side_on_settings_close, _ui);
    rde_ui_node* _card = rde_ui_image_as_node(_side->settings.card);

    _side->settings_title = fude_side_label(_ui, _card, fude_text(FUDE_TEXT_SETTINGS), FUDE_SIDE_TITLE_PX);
    // The rest, up to the buttons, scrolls, because a phone's card is too short for it.
    _side->settings_body = rde_ui_scroll_area_create(NULL);
    rde_ui_scroll_area_set_bar_thickness(_side->settings_body, 4.0f);
    rde_ui_node_add_child(_card, rde_ui_scroll_area_as_node(_side->settings_body));
    rde_ui_node* _body = rde_ui_scroll_area_as_node(_side->settings_body);
    _side->theme_label    = fude_side_label(_ui, _body, fude_text(FUDE_TEXT_SETTINGS_THEME), FUDE_SIDE_HEADER_PX);
    for(u32 _i = 0; _i < FUDE_THEME_COUNT; _i++) {
        _side->theme_refs[_i] = (fude_side_button_ref){ _ui, _i };
        _side->themes[_i]     = fude_kit_button(_body, fude_text((FUDE_TEXT_)fude_theme_text((FUDE_THEME_)_i)), fude_side_on_theme, &_side->theme_refs[_i]);   // In FUDE_THEME_ order; an app may name them.
    }
    _side->language_label = fude_side_label(_ui, _body, fude_text(FUDE_TEXT_SETTINGS_LANGUAGE), FUDE_SIDE_HEADER_PX);
    for(u32 _i = 0; _i < FUDE_TEXT_LANGUAGES; _i++) {
        _side->language_refs[_i] = (fude_side_button_ref){ _ui, _i };
        _side->languages[_i]     = fude_kit_button(_body, FUDE_TEXT_LANGUAGE_LIST[_i].name, fude_side_on_language, &_side->language_refs[_i]);
        fude_side_flag(_side->languages[_i], _i);
    }
    _side->width_label    = fude_side_label(_ui, _body, fude_text(FUDE_TEXT_SETTINGS_PEN_WIDTH), FUDE_SIDE_ROW_PX);
    _side->width_even     = fude_kit_button(_body, fude_text(FUDE_TEXT_SETTINGS_EVEN), fude_side_on_width_even, _ui);
    _side->width_pressure = fude_kit_button(_body, fude_text(FUDE_TEXT_SETTINGS_PRESSURE), fude_side_on_width_pressure, _ui);
    _side->paper_label  = fude_side_label(_ui, _body, fude_text(FUDE_TEXT_SETTINGS_PAPER), FUDE_SIDE_ROW_PX);
    _side->paper_sizes[FUDE_PAPER_SMALL]  = fude_kit_button(_body, fude_text(FUDE_TEXT_SIZE_SMALL), fude_side_on_paper_small, _ui);
    _side->paper_sizes[FUDE_PAPER_MEDIUM] = fude_kit_button(_body, fude_text(FUDE_TEXT_SIZE_MEDIUM), fude_side_on_paper_medium, _ui);
    _side->paper_sizes[FUDE_PAPER_LARGE]  = fude_kit_button(_body, fude_text(FUDE_TEXT_SIZE_LARGE), fude_side_on_paper_large, _ui);
    _side->ui_size_label = fude_side_label(_ui, _body, fude_text(FUDE_TEXT_SETTINGS_UI_SIZE), FUDE_SIDE_ROW_PX);
    for(u32 _i = 0; _i < 3u; _i++) {
        _side->ui_size_refs[_i] = (fude_side_button_ref){ _ui, FUDE_UI_SIZE_SMALL + _i };
        _side->ui_sizes[_i]     = fude_kit_button(_body, fude_text((FUDE_TEXT_)(FUDE_TEXT_SIZE_SMALL + _i)), fude_side_on_ui_size, &_side->ui_size_refs[_i]);
    }
    _side->about_label    = fude_side_label(_ui, _body, fude_text(FUDE_TEXT_SETTINGS_ABOUT), FUDE_SIDE_HEADER_PX);

    for(u32 _i = 0; _i < fude_side_section_count(_ui); _i++) {
        fude_app_ext(_ui->app)->sections[_i].build(_ui, _body);
    }

    // About: the version and the build's date (one short line: the app's name, its version and a date), the copyright,
    // then the app's credits if it has any. The UI forgets the panel before it builds it again, so this is made anew.
    // The line's room is bound by what goes in it: a template of a few words, a version of a few characters
    // (version.h) and __DATE__'s 11, far under 256 bytes in every language.
    c8 _built[256];
    FUDE_TEXTF(_built, FUDE_TEXT_ABOUT_BUILT, FUDE_TS(_ui->app->info->version), FUDE_TS(__DATE__));
    fude_side_about = rde_str_new_fmt(rde_memory_allocator_get_default_std(), "%s\n%s", _built, FUDE_APP_COPYRIGHT);
    if(_ui->app->info->credits < FUDE_TEXT_COUNT) {
        rde_str_append_fmt(&fude_side_about, NULL, "\n\n%s", fude_text((FUDE_TEXT_)_ui->app->info->credits));
    }
    _side->about_text = fude_side_label(_ui, _body, rde_str_to_char_ptr(&fude_side_about), FUDE_SIDE_SMALL_PX);
    rde_ui_label_set_wrap(_side->about_text, true);
    // It is as tall as its lines, measured near enough; when it is a little short, the text shrinks rather than
    // spills.
    rde_ui_label_set_auto_fit(_side->about_text, true);
    rde_ui_label_set_auto_fit_min_scale(_side->about_text, 0.7f);
    rde_ui_label_set_alignment(_side->about_text, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_TOP);
    _side->settings_close = fude_kit_button(_card, fude_text(FUDE_TEXT_CLOSE), fude_side_on_settings_close, _ui);
    _side->licences_button = fude_kit_button(_card, fude_text(FUDE_TEXT_LICENCES), fude_side_on_licences, _ui);
    _side->data_button     = fude_kit_button(_card, fude_text(FUDE_TEXT_DATA), fude_side_on_data, _ui);
}

// Builds Your data under _root: its card, its title, what the app does with the user's data, the answer to an export
// or an import, and its buttons (Export, Import, Replace, Cancel, Close).
RDE_INTERNAL void fude_side_create_data(fude_ui* _ui, rde_ui_node* _root) {
    fude_side* _side = &_ui->side;
    // Your data, over Settings: what the app does with the user's data, then Export and Import.
    fude_kit_modal_create(&_side->data, _root, fude_side_on_data_close, _ui);
    rde_ui_node* _dcard = rde_ui_image_as_node(_side->data.card);
    _side->data_title = fude_side_label(_ui, _dcard, fude_text(FUDE_TEXT_DATA), FUDE_SIDE_TITLE_PX);
    {
        // Three paragraphs: the app is offline; where the platform keeps a copy; how to keep one yourself.
#if defined(RDE_PLATFORM_IOS)
        const FUDE_TEXT_ _backup = FUDE_TEXT_DATA_BACKUP_IOS;
#elif defined(RDE_PLATFORM_ANDROID)
        const FUDE_TEXT_ _backup = FUDE_TEXT_DATA_BACKUP_ANDROID;
#else
        const FUDE_TEXT_ _backup = FUDE_TEXT_DATA_BACKUP_DESKTOP;
#endif
        // Whole, however long the language makes them (Hindi's and Thai's come near 2 KB together).
        rde_str _text = rde_str_new_fmt(rde_memory_allocator_get_default_std(), "%s\n\n%s\n\n%s", fude_text(FUDE_TEXT_DATA_OFFLINE), fude_text(_backup),
                                        fude_text(FUDE_TEXT_DATA_HOW));
        _side->data_text = fude_side_label(_ui, _dcard, rde_str_to_char_ptr(&_text), FUDE_SIDE_BODY_PX);   // (the label keeps a copy)
        rde_str_free(&_text);
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
    _side->_data_shown[0]      = 1;   // Differs from any message, so the first update shows it.
    _side->_data_confirm_shown = true;
}

// Builds the note card under _root: its title, its text, the name field, and its buttons (Rename, Canvas, Folder,
// Delete, Cancel, Save).
RDE_INTERNAL void fude_side_create_note_card(fude_ui* _ui, rde_ui_node* _root) {
    fude_side* _side = &_ui->side;
    // The note card, over the panel, with a backdrop that cancels it. A name may be FUDE_NOTE_NAME - 8 characters long.
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
    c8 _with_icon[512];   // An icon and one word (Rename, Canvas, Folder, Delete): far under its room in any language.
    rde_snprintf(_with_icon, sizeof(_with_icon), FUDE_ICON_DRAW "  %s", fude_text(FUDE_TEXT_RENAME));
    _side->note_rename  = fude_kit_button(_note_card, _with_icon, fude_side_on_card_rename, _ui);
    rde_snprintf(_with_icon, sizeof(_with_icon), FUDE_ICON_FILE_ADD "  %s", fude_text(FUDE_TEXT_CANVAS));
    _side->note_add     = fude_kit_button(_note_card, _with_icon, fude_side_on_card_add, _ui);
    rde_snprintf(_with_icon, sizeof(_with_icon), FUDE_ICON_FOLDER_ADD "  %s", fude_text(FUDE_TEXT_FOLDER));
    _side->note_add_folder = fude_kit_button(_note_card, _with_icon, fude_side_on_card_add_folder, _ui);
    rde_snprintf(_with_icon, sizeof(_with_icon), FUDE_ICON_TRASH "  %s", fude_text(FUDE_TEXT_DELETE));
    _side->note_delete  = fude_kit_button(_note_card, _with_icon, fude_side_on_card_delete, _ui);
    _side->note_cancel  = fude_kit_button(_note_card, fude_text(FUDE_TEXT_CANCEL), fude_side_on_card_cancel, _ui);
    _side->note_confirm = fude_kit_button(_note_card, fude_text(FUDE_TEXT_SAVE), fude_side_on_card_confirm, _ui);
}

// Builds the panel, the menu button, Settings and every card under _root, hidden, and lays them out; the UI styles them
// after (fude_side_apply_theme, from the theme). The order they are added is the order they stack.
void fude_side_create(fude_ui* _ui, rde_ui_node* _root) {
    fude_side* _side = &_ui->side;
    rde_memset(_side, 0, sizeof(*_side));

    // The panel over its backdrop; the menu button goes over both (it is added after).
    fude_kit_modal_create(&_side->panel, _root, fude_side_on_backdrop, _ui);
    rde_ui_node* _panel = rde_ui_image_as_node(_side->panel.card);

    fude_side_create_panel_contents(_ui, _panel);

    // The menu button: an icon on a small card of its own, over the page.
    _side->menu_button = fude_kit_button(_root, fude_text(FUDE_TEXT_MENU), fude_side_on_menu, _ui);
    fude_kit_icon(_side->menu_button, FUDE_ICON_MENU, FUDE_KIT_ICON_ONLY, 17.0f);
    // At the left edge it is inside Android's back-swipe strip, where RDE drops a press as the start of a gesture.
    // It is claimed back, so the OS leaves its rect alone.
    rde_ui_node_set_gesture_exclusive(rde_ui_button_as_node(_side->menu_button), true);

    fude_side_create_settings(_ui, _root);

    fude_side_create_data(_ui, _root);

    // Licences, over Settings.
    fude_kit_modal_create(&_side->licences, _root, fude_side_on_licences_close, _ui);
    rde_ui_node* _lcard = rde_ui_image_as_node(_side->licences.card);
    _side->licences_title = fude_side_label(_ui, _lcard, fude_text(FUDE_TEXT_LICENCES), FUDE_SIDE_TITLE_PX);
    for(u32 _i = 0; _i < fude_side_licence_count(_ui); _i++) {
        _side->licences_refs[_i] = (fude_side_button_ref){ _ui, _i };
        _side->licences_docs[_i] = fude_kit_button(_lcard, fude_text((FUDE_TEXT_)_ui->app->info->licences[_i].name), fude_side_on_licence_doc, &_side->licences_refs[_i]);
    }
    _side->licences_text = rde_ui_scroll_area_create(NULL);
    rde_ui_scroll_area_set_bar_thickness(_side->licences_text, 4.0f);
    rde_ui_node_add_child(_lcard, rde_ui_scroll_area_as_node(_side->licences_text));
    for(u32 _k = 0; _k < FUDE_SIDE_LICENCE_LINES; _k++) {
        _side->licences_lines[_k] = fude_side_label(_ui, rde_ui_scroll_area_as_node(_side->licences_text), "", FUDE_SIDE_LICENCE_PX);
        rde_ui_node_set_active(rde_ui_label_as_node(_side->licences_lines[_k]), false);
    }
    _side->licences_close = fude_kit_button(_lcard, fude_text(FUDE_TEXT_CLOSE), fude_side_on_licences_close, _ui);
    {
        rde_str _copyright = rde_str_new_fmt(rde_memory_allocator_get_default_std(), "%s %s", _ui->app->info->name, FUDE_APP_COPYRIGHT);
        _side->licences_copyright = fude_side_label(_ui, _lcard, rde_str_to_char_ptr(&_copyright), FUDE_SIDE_SMALL_PX);   // (the label keeps a copy)
        rde_str_free(&_copyright);
    }
    _side->_licence_first = -1;

    // FAQ & Contact, over the panel. The link shows only when there is an FAQ page.
    fude_kit_modal_create(&_side->faq, _root, fude_side_on_faq_close, _ui);
    rde_ui_node* _fcard = rde_ui_image_as_node(_side->faq.card);
    _side->faq_title = fude_side_label(_ui, _fcard, fude_text(FUDE_TEXT_FAQ_CONTACT), FUDE_SIDE_TITLE_PX);
    _side->faq_link  = fude_kit_button(_fcard, fude_text(FUDE_TEXT_FAQ_OPEN), fude_side_on_faq_link, _ui);
    fude_kit_icon(_side->faq_link, FUDE_ICON_EXPORT, FUDE_KIT_ICON_LEFT, 15.0f);
    rde_ui_node_set_active(rde_ui_button_as_node(_side->faq_link), fude_side_faq_url(_ui)[0] != 0);
    _side->faq_text  = fude_side_label(_ui, _fcard, fude_text(FUDE_TEXT_CONTACT_WRITE), FUDE_SIDE_ROW_PX);
    rde_ui_label_set_wrap(_side->faq_text, true);
    _side->faq_mail  = fude_kit_button(_fcard, FUDE_APP_CONTACT, fude_side_on_faq_mail, _ui);
    _side->faq_close = fude_kit_button(_fcard, fude_text(FUDE_TEXT_CLOSE), fude_side_on_faq_close, _ui);

    fude_side_create_note_card(_ui, _root);

    // Everything stays hidden until asked for (the cards are made hidden); the menu button shows with the page.
    rde_ui_node_set_active(rde_ui_button_as_node(_side->menu_button), false);

    fude_side_layout(_ui);
}

// --- every frame ------------------------------------------------------------------------

// Shows what is open and keeps it up to date. See the header. A card closes with what it sits over: Licences and Your
// data with Settings, FAQ & Contact and the note card with the panel.
void fude_side_update(fude_ui* _ui, b8 _full) {
    fude_side* _side = &_ui->side;
    if(_side->panel.card == NULL) {
        return;
    }
    // The app's button comes and goes (once the voice is installed, for example), and the panel is laid out again.
    if(_side->app_button != NULL) {
        const b8 _wanted = fude_app_ext(_ui->app)->side_button->shown(_ui->app);
        if(_wanted != _side->_app_button_shown) {
            _side->_app_button_shown = _wanted;
            rde_ui_node_set_active(rde_ui_button_as_node(_side->app_button), _wanted);
            fude_side_relayout(_ui);
        }
    }

    // Lay it out again when the screen turns, and when the safe area changes: at launch iOS reports none (SDL has the
    // whole window until the view is laid out), then the status bar's arrives a frame or two later.
    const rde_vec_2F _screen = fude_kit_screen_size((_ui)->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_ui->window);
    if(rde_memcmp(&_screen, &_side->_laid_out, sizeof(rde_vec_2F)) != 0 || rde_memcmp(&_insets, &_side->_laid_out_insets, sizeof(rde_vec_4I)) != 0) {
        fude_side_layout(_ui);
    }

    if(_full) {
        _side->open = false;          // A screen took the whole screen, so the panel is done.
    }

    fude_side_show(rde_ui_button_as_node(_side->menu_button), !_full, &_side->_shown_menu);
    // The navigation's counts (Kana's "Reviews · n", for example): a label changes only when its count does.
    for(u32 _i = 0; _side->open && _i < fude_side_nav_count(_ui); _i++) {
        const fude_extension_nav_entry* _entry = &fude_side_nav(_ui)->entries[_i];
        const u32                       _n     = _entry->count != NULL ? _entry->count(_ui->app) : 0u;
        if(_entry->count != NULL && _n != _side->_nav_counts[_i]) {
            _side->_nav_counts[_i] = _n;
            c8 _label[512];
            if(_n > 0) { FUDE_TEXTF(_label, (FUDE_TEXT_)_entry->counted_text, FUDE_TN(_n)); }
            else       { rde_snprintf(_label, sizeof(_label), "%s", fude_text((FUDE_TEXT_)_entry->text)); }
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
        fude_app_ext(_ui->app)->sections[_i].refresh(_ui, false);   // What moves on by itself (Kana's ML Kit download).
    }
    if(!_side->settings_open && _side->data_open) {
        _side->data_open = false;
        fude_side_data_done(_ui);
    }
    fude_kit_modal_show(&_side->data, _side->data_open);
    if(_side->data_open) {
        if(rde_strcmp(_side->_data_shown, _side->data_message) != 0) {
            rde_snprintf(_side->_data_shown, sizeof(_side->_data_shown), "%s", _side->data_message);
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

    if(!_side->open && _side->faq_open) {
        _side->faq_open = false;   // The panel is gone, so its card goes too.
    }
    fude_kit_modal_show(&_side->faq, _side->faq_open);

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

    // The notes list is rebuilt when the notes changed, another canvas opened, or the list's size changed; never in
    // the middle of a drag, because the handle being dragged would go with it.
    if(_side->open && _ui->app->notes != NULL && _side->drag_id == 0 &&
       (!_side->_notes_built || _side->_notes_revision != _ui->app->notes->revision || _side->_notes_open != _ui->app->notes->open)) {
        fude_side_build_notes(_ui);
    }
}

// Whether _at is on the menu button, or anywhere while the panel, Settings or a card shows (the cards sit over those).
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

// Restyles every widget of the panel, Settings and the cards from the active theme. Does nothing before it is built.
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
    rde_ui_button* const _quiet[] = { _side->new_folder, _side->new_canvas, _side->settings_button, _side->faq_button, _side->tutorial, _side->rate, _side->app_button };
    for(u32 _i = 0; _i < sizeof(_quiet) / sizeof(_quiet[0]); _i++) {
        if(_quiet[_i] != NULL) {
            fude_kit_restyle_quiet(_quiet[_i]);
        }
    }
    rde_ui_button* const _buttons[] = { _side->width_even, _side->width_pressure, _side->settings_close,
                                        _side->paper_sizes[0], _side->paper_sizes[1], _side->paper_sizes[2],
                                        _side->ui_sizes[0], _side->ui_sizes[1], _side->ui_sizes[2],
                                        _side->licences_button, _side->licences_close,
                                        _side->data_button, _side->data_export, _side->data_import, _side->data_cancel, _side->data_close,
                                        _side->faq_link, _side->faq_mail, _side->faq_close,
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
    fude_kit_button_primary(_side->faq_close);
    fude_kit_modal_restyle(&_side->faq, 18.0f);
    fude_kit_button_danger(_side->data_replace);
    fude_kit_modal_restyle(&_side->data, 18.0f);
    _side->_data_shown[0] = 1;   // So the answer line's colour is set again.
    fude_kit_modal_restyle(&_side->note, 18.0f);
    fude_kit_modal_restyle(&_side->licences, 18.0f);
    rde_ui_scroll_area* const _scrolled[] = { _side->licences_text, _side->settings_body };
    for(u32 _k = 0; _k < sizeof(_scrolled) / sizeof(_scrolled[0]); _k++) {
        rde_ui_scroll_area_set_background_color(_scrolled[_k], (rde_color){ 0, 0, 0, 0 });
        rde_ui_scroll_area_set_track_color(_scrolled[_k], (rde_color){ 0, 0, 0, 0 });
        rde_ui_scroll_area_set_thumb_colors(_scrolled[_k], _t->grip, fude_theme_shade(_t->grip, 20), fude_theme_shade(_t->grip, 40));
    }
    for(u32 _k = 0; _k < FUDE_SIDE_LICENCE_LINES; _k++) {
        rde_ui_label_set_color(_side->licences_lines[_k], _t->button_text);
    }
    fude_kit_button_danger(_side->note_delete);
    fude_kit_style_field(_side->note_field);
    rde_ui_scroll_area_set_background_color(_side->notes_list, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_track_color(_side->notes_list, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_thumb_colors(_side->notes_list, _t->grip, fude_theme_shade(_t->grip, 20), fude_theme_shade(_t->grip, 40));
    _side->_notes_built = false;   // The rows restyle as they are built again.

    rde_ui_image_set_style(_side->drop_line, RDE_UI_STATE_NORMAL, fude_kit_style(_t->select, 1.5f));
    rde_ui_style _box    = fude_kit_style(_t->select_fill, 10.0f);
    _box.border_width    = 2.0f;
    _box.border_color    = _t->select;
    rde_ui_image_set_style(_side->drop_box, RDE_UI_STATE_NORMAL, _box);
    rde_ui_style _ghost  = fude_kit_style(_t->accent, 10.0f);
    rde_ui_image_set_style(_side->drag_ghost, RDE_UI_STATE_NORMAL, _ghost);
    rde_ui_label_set_color(_side->drag_ghost_label, _t->on_accent);

    // Each theme's button previews it in its own page and text colours; the current one is ringed in the accent.
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
    rde_ui_label* const _texts[] = { _side->settings_title, _side->width_label, _side->paper_label, _side->ui_size_label, _side->note_title,
                                     _side->licences_title, _side->data_title, _side->data_text, _side->faq_title, _side->faq_text };
    for(u32 _i = 0; _i < sizeof(_texts) / sizeof(_texts[0]); _i++) {
        rde_ui_label_set_color(_texts[_i], _t->text);
    }
    rde_ui_label* const _soft[] = { _side->about_text, _side->note_body, _side->licences_copyright };
    for(u32 _i = 0; _i < sizeof(_soft) / sizeof(_soft[0]); _i++) {
        rde_ui_label_set_color(_soft[_i], _t->text_soft);
    }
    rde_ui_label_set_color(_side->version, _t->text_soft);
    for(u32 _i = 0; _i < fude_side_section_count(_ui); _i++) {
        fude_app_ext(_ui->app)->sections[_i].restyle(_ui);
    }

    fude_side_refresh_settings(_ui);
}
