#include "drawing/widgets/docbar.h"
#include "drawing/app/app.h"
#include "drawing/app/page.h"
#include "drawing/app/session.h"
#include "drawing/ink/notes.h"
#include "drawing/widgets/notice.h"
#include "drawing/app/ui.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "drawing/doc/doc.h"
#include "drawing/doc/pdf.h"
#include "drawing/widgets/icons.h"
#include "drawing/widgets/kit.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See docbar.h. Built with the kit; laid out again when its mode or the screen
// changes.
// ===========================================================================

#define FUDE_DOCBAR_H       48.0f   // the panel's height
#define FUDE_DOCBAR_PAD     4.0f
#define FUDE_DOCBAR_BUTTON  40.0f   // an icon button's side
#define FUDE_DOCBAR_PAGE_W  104.0f  // "123 / 456"
#define FUDE_DOCBAR_FIELD_W 240.0f  // the words
#define FUDE_DOCBAR_NUM_W   96.0f   // a page's number
#define FUDE_DOCBAR_COUNT_W 96.0f   // "12 / 345", "No matches"
#define FUDE_DOCBAR_PX      15u

#define FUDE_DOCBAR_DOC(_bar) (&(_bar)->app->page->doc)

// --- the mode -------------------------------------------------------------------------------

RDE_INTERNAL void fude_docbar_clear_field(fude_docbar* _bar) {
    const usize _bytes = rde_ui_text_editor_get_byte_count(_bar->field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_bar->field, 0, _bytes);
    }
}

// Into _mode: the field for it (empty for a page), the keyboard up; back to the
// bar's two buttons, down.
RDE_INTERNAL void fude_docbar_set_mode(fude_docbar* _bar, u8 _mode) {
    _bar->mode        = _mode;
    _bar->_mode_laid  = 0xFFu;
    _bar->_count_said = UINT32_MAX;
    if(_mode == FUDE_DOCBAR_PAGE) {
        fude_docbar_clear_field(_bar);
        rde_ui_text_editor_set_placeholder(_bar->field, fude_text(FUDE_TEXT_DOC_PAGE));
        rde_ui_text_editor_set_char_filter(_bar->field, RDE_UI_TEXT_EDITOR_FILTER_INTEGER);
    } else if(_mode == FUDE_DOCBAR_SEARCH) {
        rde_ui_text_editor_set_placeholder(_bar->field, fude_text(FUDE_TEXT_DOC_SEARCH_HINT));
        rde_ui_text_editor_set_char_filter(_bar->field, RDE_UI_TEXT_EDITOR_FILTER_NONE);
    }
    fude_docbar_update(_bar, false);   // laid out: the field placed before it takes the keyboard
    if(_mode != FUDE_DOCBAR_IDLE) {
        rde_ui_node_focus(rde_ui_text_editor_as_node(_bar->field));
    } else if(_bar->app->ui != NULL && _bar->app->ui->canvas != NULL) {
        rde_ui_canvas_blur(_bar->app->ui->canvas);
    }
}

// --- callbacks --------------------------------------------------------------------------------

#define FUDE_DOCBAR_CALLBACK(_name) RDE_INTERNAL RDE_UI_EVENT_RESULT_ _name(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data)
#define FUDE_DOCBAR_SELF            RDE_UNUSED(_node); RDE_UNUSED(_info); fude_docbar* _bar = (fude_docbar*)_user_data

FUDE_DOCBAR_CALLBACK(fude_docbar_on_search) { FUDE_DOCBAR_SELF; fude_docbar_set_mode(_bar, FUDE_DOCBAR_SEARCH); return RDE_UI_EVENT_RESULT_CONSUME; }
FUDE_DOCBAR_CALLBACK(fude_docbar_on_page)   { FUDE_DOCBAR_SELF; fude_docbar_set_mode(_bar, FUDE_DOCBAR_PAGE); return RDE_UI_EVENT_RESULT_CONSUME; }
FUDE_DOCBAR_CALLBACK(fude_docbar_on_before) { FUDE_DOCBAR_SELF; fude_doc_search_step(FUDE_DOCBAR_DOC(_bar), _bar->app->canvas, -1); return RDE_UI_EVENT_RESULT_CONSUME; }
FUDE_DOCBAR_CALLBACK(fude_docbar_on_next)   { FUDE_DOCBAR_SELF; fude_doc_search_step(FUDE_DOCBAR_DOC(_bar), _bar->app->canvas, 1); return RDE_UI_EVENT_RESULT_CONSUME; }

// --- Export ---------------------------------------------------------------------------------

// The canvas's name, as a file's (no folder signs in it).
RDE_INTERNAL void fude_docbar_file_name(const fude_docbar* _bar, c8* _out, usize _size) {
    const fude_note* _note = fude_notes_find(_bar->app->notes, _bar->app->notes->open);
    snprintf(_out, _size, "%s", _note != NULL && _note->name[0] != 0 ? _note->name : "Document");
    for(c8* _c = _out; *_c != 0; _c++) {
        if(*_c == '/' || *_c == '\\' || *_c == ':') {
            *_c = '-';
        }
    }
}

RDE_INTERNAL b8 fude_docbar_write(fude_docbar* _bar, const c8* _path) {
    if(!fude_doc_export(FUDE_DOCBAR_DOC(_bar), _bar->app->ink, _path)) {
        fude_notice_show(fude_text(FUDE_TEXT_DOC_EXPORT_FAILED));
        return false;
    }
    return true;
}

#if !defined(RDE_PLATFORM_MOBILE)
// A computer: where to save it, chosen.
RDE_INTERNAL void fude_docbar_on_export_path(const c8* const* _paths, u32 _count, i32 _filter, any _user_data) {
    RDE_UNUSED(_filter);
    fude_docbar* _bar = (fude_docbar*)_user_data;
    if(_paths == NULL || _count == 0u) {
        return;   // cancelled, or no dialog
    }
    c8 _path[RDE_MAX_PATH];
    fude_session_with_extension(_path, sizeof(_path), _paths[0], ".pdf", ".PDF");
    if(fude_docbar_write(_bar, _path)) {
        c8 _line[RDE_MAX_PATH + 64];
        FUDE_TEXTF(_line, FUDE_TEXT_DOC_EXPORTED, FUDE_TS(_path));
        fude_notice_show(_line);
    }
}
#endif

FUDE_DOCBAR_CALLBACK(fude_docbar_on_share) {
    FUDE_DOCBAR_SELF;
    c8 _name[FUDE_NOTE_NAME];
    fude_docbar_file_name(_bar, _name, sizeof(_name));
#if defined(RDE_PLATFORM_MOBILE)
    // Written to the outbox, then shared from there (the share sheet: Files, Mail, AirDrop...).
    c8 _path[RDE_MAX_PATH];
    fude_session_outbox_path(_path, sizeof(_path), _name, "pdf");
    if(fude_docbar_write(_bar, _path) && !rde_mobile_share_file(_path, "application/pdf", _name)) {
        fude_notice_show(fude_text(FUDE_TEXT_DOC_EXPORT_FAILED));
    }
#else
    static const rde_dialog_filter _pdf = { "PDF", "pdf" };
    rde_dialog_save_file(_bar->app->window, &_pdf, 1u, NULL, fude_docbar_on_export_path, _bar);
#endif
    return RDE_UI_EVENT_RESULT_CONSUME;
}

FUDE_DOCBAR_CALLBACK(fude_docbar_on_turn) {
    FUDE_DOCBAR_SELF;
    fude_doc* _doc = FUDE_DOCBAR_DOC(_bar);
    if(fude_doc_page_count(_doc) > 0u && !fude_doc_turn_page(_doc, _bar->app, fude_doc_page_at(_doc, _bar->app->canvas->view))) {
        fude_notice_show(fude_text(FUDE_TEXT_DOC_TURN_FULL));
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

FUDE_DOCBAR_CALLBACK(fude_docbar_on_close) {
    FUDE_DOCBAR_SELF;
    if(_bar->mode == FUDE_DOCBAR_SEARCH) {
        fude_doc_search_stop(FUDE_DOCBAR_DOC(_bar));   // the marks go with it
    }
    fude_docbar_set_mode(_bar, FUDE_DOCBAR_IDLE);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

b8 fude_docbar_back(fude_docbar* _bar) {
    if(_bar->mode == FUDE_DOCBAR_IDLE) {
        return false;
    }
    fude_docbar_on_close(NULL, NULL, _bar);
    return true;
}

// Return: the words searched for, or the page gone to.
RDE_INTERNAL void fude_docbar_on_submit(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    fude_docbar* _bar  = (fude_docbar*)_user_data;
    c8*          _text = rde_ui_text_editor_get_text(_bar->field, 0, rde_ui_text_editor_get_byte_count(_bar->field));
    const c8*    _s    = _text != NULL ? _text : "";
    if(_bar->mode == FUDE_DOCBAR_SEARCH) {
        fude_doc_search(FUDE_DOCBAR_DOC(_bar), _s);
        _bar->_count_said = UINT32_MAX;
    } else if(_bar->mode == FUDE_DOCBAR_PAGE) {
        const long _n = strtol(_s, NULL, 10);
        if(_n >= 1) {
            fude_doc_go_to_page(FUDE_DOCBAR_DOC(_bar), _bar->app->canvas, (u32)(_n - 1));
        }
        fude_docbar_set_mode(_bar, FUDE_DOCBAR_IDLE);
    }
    if(_text != NULL) {
        rde_ui_text_editor_free_text(_bar->field, _text);
    }
}

// --- layout --------------------------------------------------------------------------------

// In a row from the panel's left: each node its width, centred down the panel.
RDE_INTERNAL f32 fude_docbar_put(rde_ui_node* _node, f32 _x, f32 _w, f32 _h) {
    fude_kit_place(_node, (rde_vec_2F){ _x + _w * 0.5f, FUDE_DOCBAR_H * 0.5f }, (rde_vec_2F){ _w, _h });
    return _x + _w + FUDE_DOCBAR_PAD;
}

// The mode's widgets placed (the rest put away), the panel as wide as they need,
// at the top middle of the screen under the safe area.
RDE_INTERNAL void fude_docbar_layout(fude_docbar* _bar) {
    const rde_vec_2F _screen = fude_kit_screen_size(_bar->app->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_bar->app->window);   // left, top, right, bottom
    const u8         _mode   = _bar->mode;
    _bar->_mode_laid  = _mode;
    _bar->_laid_for   = _screen;
    _bar->_insets_for = _insets;
    const b8 _idle   = _mode == FUDE_DOCBAR_IDLE;
    const b8 _search = _mode == FUDE_DOCBAR_SEARCH;
    // Search and Export only where the platform reads a PDF's text and writes one (PDFKit and Core Graphics; PDFBox on Android).
    const b8 _finds  = fude_pdf_text_available();
    const b8 _writes = fude_pdf_write_available();
    rde_ui_node_set_active(rde_ui_button_as_node(_bar->search), _idle && _finds);
    rde_ui_node_set_active(rde_ui_button_as_node(_bar->page), _idle);
    rde_ui_node_set_active(rde_ui_button_as_node(_bar->share), _idle && _writes);
    rde_ui_node_set_active(rde_ui_button_as_node(_bar->turn), _idle);
    rde_ui_node_set_active(fude_kit_field_node(_bar->field), !_idle);
    rde_ui_node_set_active(rde_ui_label_as_node(_bar->count), !_idle);
    rde_ui_node_set_active(rde_ui_button_as_node(_bar->before), _search);
    rde_ui_node_set_active(rde_ui_button_as_node(_bar->next), _search);
    rde_ui_node_set_active(rde_ui_button_as_node(_bar->close), !_idle);

    f32 _x = FUDE_DOCBAR_PAD;
    if(_idle) {
        if(_finds) {
            _x = fude_docbar_put(rde_ui_button_as_node(_bar->search), _x, FUDE_DOCBAR_BUTTON, FUDE_DOCBAR_BUTTON);
        }
        _x = fude_docbar_put(rde_ui_button_as_node(_bar->page), _x, FUDE_DOCBAR_PAGE_W, FUDE_DOCBAR_BUTTON);
        _x = fude_docbar_put(rde_ui_button_as_node(_bar->turn), _x, FUDE_DOCBAR_BUTTON, FUDE_DOCBAR_BUTTON);
        if(_writes) {
            _x = fude_docbar_put(rde_ui_button_as_node(_bar->share), _x, FUDE_DOCBAR_BUTTON, FUDE_DOCBAR_BUTTON);
        }
    } else {
        // The words' field as wide as the screen allows (a phone's is narrow).
        const f32 _rest  = (_search ? 3.0f : 1.0f) * (FUDE_DOCBAR_BUTTON + FUDE_DOCBAR_PAD) + FUDE_DOCBAR_COUNT_W + 3.0f * FUDE_DOCBAR_PAD;
        const f32 _room  = _screen.x - (f32)(_insets.x + _insets.z) - 2.0f * 72.0f - _rest;   // clear of the menu button
        const f32 _field = _search ? fminf(FUDE_DOCBAR_FIELD_W, fmaxf(120.0f, _room)) : FUDE_DOCBAR_NUM_W;
        _x = fude_docbar_put(fude_kit_field_node(_bar->field), _x, _field, FUDE_DOCBAR_H - 2.0f * FUDE_DOCBAR_PAD);
        _x = fude_docbar_put(rde_ui_label_as_node(_bar->count), _x, _search ? FUDE_DOCBAR_COUNT_W : 64.0f, FUDE_DOCBAR_BUTTON);
        if(_search) {
            _x = fude_docbar_put(rde_ui_button_as_node(_bar->before), _x, FUDE_DOCBAR_BUTTON, FUDE_DOCBAR_BUTTON);
            _x = fude_docbar_put(rde_ui_button_as_node(_bar->next), _x, FUDE_DOCBAR_BUTTON, FUDE_DOCBAR_BUTTON);
        }
        _x = fude_docbar_put(rde_ui_button_as_node(_bar->close), _x, FUDE_DOCBAR_BUTTON, FUDE_DOCBAR_BUTTON);
    }
    _bar->size   = (rde_vec_2F){ _x, FUDE_DOCBAR_H };
    _bar->center = (rde_vec_2F){ (_screen.x + (f32)(_insets.x - _insets.z)) * 0.5f, _screen.y - (f32)_insets.y - 8.0f - FUDE_DOCBAR_H * 0.5f };
    fude_kit_place(rde_ui_image_as_node(_bar->panel), _bar->center, _bar->size);
}

// --- each frame -----------------------------------------------------------------------------

void fude_docbar_update(fude_docbar* _bar, b8 _hidden) {
    if(_bar->panel == NULL) {
        return;
    }
    fude_doc* _doc   = FUDE_DOCBAR_DOC(_bar);
    const u32 _pages = fude_doc_page_count(_doc);
    const b8  _show  = !_hidden && _pages > 0u;
    if(_show != _bar->shown) {
        _bar->shown = _show;
        rde_ui_node_set_active(rde_ui_image_as_node(_bar->panel), _show);
        if(!_show && _bar->mode != FUDE_DOCBAR_IDLE) {
            _bar->mode       = FUDE_DOCBAR_IDLE;   // another canvas, or a screen over it: the bar starts over
            _bar->_mode_laid = 0xFFu;
        }
    }
    if(!_show) {
        return;
    }
    // The page in the middle, out of how many.
    const u32 _at  = fude_doc_page_at(_doc, _bar->app->canvas->view) + 1u;
    const u32 _key = _at | (_pages << 16);
    if(_key != _bar->_page_said) {
        _bar->_page_said = _key;
        c8 _line[32];
        snprintf(_line, sizeof(_line), "%u / %u", _at, _pages);
        rde_ui_button_set_text(_bar->page, _line);
        fude_kit_icon(_bar->page, FUDE_ICON_PAGE_NUMBER, FUDE_KIT_ICON_LEFT, 14.0f);
    }
    // The matches (searching), or how many pages there are (a page asked for).
    u32 _shown_at = 0u;
    b8  _done     = true;
    const u32 _n  = fude_doc_search_count(_doc, &_shown_at, &_done);
    const u32 _count_key = _bar->mode == FUDE_DOCBAR_PAGE ? 0x80000000u | _pages
                         : (_n << 12) ^ (_shown_at << 1) ^ (_done ? 1u : 0u) ^ (_doc->searching ? 0x40000000u : 0u);
    if(_count_key != _bar->_count_said) {
        _bar->_count_said = _count_key;
        c8 _line[64];
        if(_bar->mode == FUDE_DOCBAR_PAGE) {
            snprintf(_line, sizeof(_line), "/ %u", _pages);
        } else if(_n > 0u) {
            snprintf(_line, sizeof(_line), "%u / %u", _shown_at + 1u, _n);
        } else if(_doc->searching) {
            snprintf(_line, sizeof(_line), "%s", fude_text(_done ? FUDE_TEXT_DOC_NO_MATCHES : FUDE_TEXT_DOC_SEARCHING));
        } else {
            _line[0] = 0;
        }
        rde_ui_label_set_text(_bar->count, _line);
    }
    // Laid out again for another mode, a rotation, or the safe area: at start iOS
    // reports none, and the status bar's arrives a frame or two later.
    const rde_vec_2F _screen = fude_kit_screen_size(_bar->app->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_bar->app->window);
    if(_bar->_mode_laid != _bar->mode || memcmp(&_screen, &_bar->_laid_for, sizeof(rde_vec_2F)) != 0 ||
       memcmp(&_insets, &_bar->_insets_for, sizeof(rde_vec_4I)) != 0) {
        fude_docbar_layout(_bar);
    }
}

b8 fude_docbar_hit(const fude_docbar* _bar, rde_vec_2F _ui) {
    return _bar->shown && _ui.x >= _bar->center.x - _bar->size.x * 0.5f && _ui.x <= _bar->center.x + _bar->size.x * 0.5f &&
           _ui.y >= _bar->center.y - _bar->size.y * 0.5f && _ui.y <= _bar->center.y + _bar->size.y * 0.5f;
}

void fude_docbar_restyle(fude_docbar* _bar) {
    if(_bar->panel == NULL) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    fude_kit_style_panel(_bar->panel, FUDE_DOCBAR_H * 0.5f, 1.0f);
    fude_kit_style_field(_bar->field);
    rde_ui_label_set_color(_bar->count, _t->text_soft);
    rde_ui_button* const _buttons[] = { _bar->search, _bar->page, _bar->turn, _bar->share, _bar->before, _bar->next, _bar->close };
    for(u32 _i = 0; _i < sizeof(_buttons) / sizeof(_buttons[0]); _i++) {
        fude_kit_restyle_quiet(_buttons[_i]);
    }
}

// --- lifetime -------------------------------------------------------------------------------

void fude_docbar_create(fude_docbar* _bar, rde_ui_node* _root, fude_app* _app) {
    memset(_bar, 0, sizeof(*_bar));
    _bar->app         = _app;
    _bar->_mode_laid  = 0xFFu;
    _bar->_count_said = UINT32_MAX;
    _bar->panel       = rde_ui_image_create(NULL);
    rde_ui_node* _p   = rde_ui_image_as_node(_bar->panel);
    rde_ui_node_set_blocks_input(_p, true);
    rde_ui_node_add_child(_root, _p);

    _bar->search = fude_kit_button(_p, fude_text(FUDE_TEXT_DOC_SEARCH), fude_docbar_on_search, _bar);
    fude_kit_icon(_bar->search, FUDE_ICON_SEARCH, FUDE_KIT_ICON_ONLY, 18.0f);
    _bar->page   = fude_kit_button(_p, "", fude_docbar_on_page, _bar);
    _bar->turn   = fude_kit_button(_p, fude_text(FUDE_TEXT_DOC_TURN), fude_docbar_on_turn, _bar);
    fude_kit_icon(_bar->turn, FUDE_ICON_TURN_PAGE, FUDE_KIT_ICON_ONLY, 18.0f);
    _bar->share  = fude_kit_button(_p, fude_text(FUDE_TEXT_DOC_EXPORT), fude_docbar_on_share, _bar);
    fude_kit_icon(_bar->share, FUDE_ICON_EXPORT, FUDE_KIT_ICON_ONLY, 18.0f);
    _bar->before = fude_kit_button(_p, fude_text(FUDE_TEXT_DOC_BEFORE), fude_docbar_on_before, _bar);
    fude_kit_icon(_bar->before, FUDE_ICON_UP, FUDE_KIT_ICON_ONLY, 18.0f);
    _bar->next   = fude_kit_button(_p, fude_text(FUDE_TEXT_DOC_NEXT), fude_docbar_on_next, _bar);
    fude_kit_icon(_bar->next, FUDE_ICON_DOWN, FUDE_KIT_ICON_ONLY, 18.0f);
    _bar->close  = fude_kit_button(_p, fude_text(FUDE_TEXT_CLOSE), fude_docbar_on_close, _bar);
    fude_kit_icon(_bar->close, FUDE_ICON_CLOSE, FUDE_KIT_ICON_ONLY, 18.0f);

    _bar->count = rde_ui_label_create(NULL);
    rde_ui_label_set_font(_bar->count, fude_kit_font());
    rde_ui_label_set_font_scale(_bar->count, 14.0f / (f32)FUDE_KIT_FONT_SIZE);
    rde_ui_label_set_alignment(_bar->count, RDE_UI_LABEL_H_ALIGN_CENTER, RDE_UI_LABEL_V_ALIGN_MIDDLE);
    rde_ui_label_set_auto_fit(_bar->count, true);
    rde_ui_label_set_auto_fit_min_scale(_bar->count, 0.6f);
    rde_ui_node_set_raycast_target(rde_ui_label_as_node(_bar->count), false);
    rde_ui_node_add_child(_p, rde_ui_label_as_node(_bar->count));

    _bar->field = rde_ui_text_editor_create(fude_kit_font(), NULL);
    rde_ui_text_editor_set_multiline(_bar->field, false);
    rde_ui_text_editor_set_font_size(_bar->field, FUDE_DOCBAR_PX);
    rde_ui_text_editor_set_max_chars(_bar->field, 64u);
    rde_ui_text_editor_set_content_insets(_bar->field, 12.0f, 6.0f, 12.0f, 6.0f);
    rde_ui_text_editor_add_plugin(_bar->field, rde_ui_text_editor_plugin_ime_get());   // Japanese from the keyboard
    rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_bar->field), _bar);
    rde_ui_text_editor_set_on_submit(_bar->field, fude_docbar_on_submit);
    fude_kit_field_box(_p, _bar->field);

    rde_ui_node_set_active(_p, false);
    fude_docbar_restyle(_bar);
}

void fude_docbar_search(fude_docbar* _bar, const c8* _words) {
    if(_bar->panel == NULL || _words == NULL) {
        return;
    }
    fude_docbar_set_mode(_bar, FUDE_DOCBAR_SEARCH);
    fude_docbar_clear_field(_bar);
    rde_ui_text_editor_insert_at(_bar->field, 0, _words, strlen(_words));
    fude_docbar_on_submit(rde_ui_text_editor_as_node(_bar->field), _bar);
}
