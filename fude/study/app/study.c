#include "study/app/study.h"
#include "drawing/app/ui.h"
#include "drawing/widgets/kit.h"
#include "drawing/widgets/side.h"
#include "drawing/base/text.h"
#include "study/models/marks.h"
#include "study/models/review.h"
#include "study/services/mlkit.h"
#include "study/services/textscan.h"
#include "study/services/speech.h"
#include "drawing/widgets/notice.h"
#include "drawing/widgets/icons.h"
#include "drawing/doc/doc.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See study.h.
// ===========================================================================

u32 fude_study_reviews_count(fude_app* _app) {
    fude_study* _study = FUDE_STUDY(_app);
    const u32   _for   = fude_marks_revision() * 31u + fude_reviews_revision() * 7u + fude_reviews_today();
    if(!_study->_reviews_known || _for != _study->_reviews_for) {
        u32 _records[FUDE_REVIEW_SESSION];
        _study->_reviews_count = fude_study_reviews_due(_app, _records, FUDE_REVIEW_SESSION);
        _study->_reviews_for   = _for;
        _study->_reviews_known = true;
    }
    return _study->_reviews_count;
}

// --- lifetime ------------------------------------------------------------------------------

void fude_study_init(fude_study* _study) {
    fude_pagetext_init(&_study->text, &_study->app);
}

void fude_study_destroy(fude_study* _study) {
    fude_pagetext_destroy(&_study->text);
}

void fude_study_update(fude_app* _app) {
    fude_study* _study = FUDE_STUDY(_app);
    if(_study->selection != NULL && _study->selection->active) {
        b8 _ticking = false;
        for(u32 _s = 0; _s < _app->screen_count; _s++) {
            _ticking = _ticking || (_app->screens[_s].vt != NULL && _app->screens[_s].vt->selects && fude_app_is_open(_app, _s));
        }
        _study->selection->active = _ticking;
    }
    if(fude_speech_take_hint()) {
        fude_notice_show(fude_text(FUDE_TEXT_SPEECH_BETTER_VOICE));   // the basic voice spoke: where better ones are
    }
}

// --- the page read as text ------------------------------------------------------------------

void fude_study_menu_update(fude_app* _app) {
    fude_pagetext_update(&FUDE_STUDY(_app)->text);
}

void fude_study_selection_faces(fude_app* _app, const fude_row_def* _row, fude_row_face* _faces) {
    fude_pagetext_selection_faces(&FUDE_STUDY(_app)->text, _row, _faces);
}

void fude_study_context_faces(fude_app* _app, const fude_row_def* _row, fude_row_face* _faces) {
    fude_pagetext_context_faces(&FUDE_STUDY(_app)->text, _row, _faces);
}

void fude_study_page_render(fude_app* _app, rde_window* _window) {
    fude_pagetext_render(&FUDE_STUDY(_app)->text, _window);   // Translate with Google's card, by the selection
}

b8 fude_study_page_press(fude_app* _app, rde_vec_2F _screen) {
    return fude_pagetext_press(&FUDE_STUDY(_app)->text, _screen);   // the card's speaker, a word on it
}

// --- its widgets ------------------------------------------------------------------------------

void fude_study_ui_build(fude_ui* _ui, rde_ui_node* _root) {
    fude_wordcard_create(_ui, _root);   // over everything: it opens from any screen
}

void fude_study_ui_update(fude_ui* _ui, b8 _full) {
    RDE_UNUSED(_full);
    fude_wordcard_update(_ui);   // a word asked for (wordcard.h), opened; laid out again when the screen turns
}

void fude_study_ui_restyle(fude_ui* _ui) {
    fude_wordcard_apply_theme(_ui);
}

// The translation card by the selection; the word card (while it is open, the whole screen).
b8 fude_study_ui_hit(const fude_ui* _ui, rde_vec_2F _screen, rde_vec_2F _canvas) {
    RDE_UNUSED(_canvas);
    const fude_study* _study = FUDE_STUDY(_ui->app);
    return _study->word.open || fude_pagetext_hit(&_study->text, _screen);
}

void fude_study_write_text(fude_app* _app, const c8* _text, rde_vec_2F _canvas) {
    fude_pagetext_write(&FUDE_STUDY(_app)->text, _text, _canvas);
}

// --- the toolbar's camera -------------------------------------------------------------------

// Text from a photo with the camera live straight away; what is written goes to
// the middle of the page as it is on screen. Only where text can be read from it.
RDE_INTERNAL void fude_study_camera(fude_app* _app) {
    fude_study* _study = FUDE_STUDY(_app);
    fude_scan_open(_study->scan, _app->window, fude_canvas_from_screen(_app->canvas, (rde_vec_2F){ 0.0f, 0.0f }));
    fude_scan_camera(_study->scan);
}

const fude_extension_tool FUDE_STUDY_CAMERA = { FUDE_TEXT_SCAN_CAMERA, FUDE_ICON_CAMERA, fude_study_camera, fude_textscan_available };

// --- Settings' Handwriting ----------------------------------------------------------------
//
// Google ML Kit on or off, and where its model is: a header, the toggle, the
// model's state (Download / Retry at its right).

RDE_INTERNAL void fude_study_hand_refresh(fude_ui* _ui, b8 _force) {
    fude_study*       _study = FUDE_STUDY(_ui->app);
    const FUDE_MLKIT_ _state = fude_mlkit_state();
    const b8          _on    = fude_mlkit_enabled();
    const i32         _shown = (i32)_state + (_on ? 0 : 100);
    if(_study->mlkit_toggle == NULL || (!_force && _shown == _study->_mlkit_shown)) {
        return;
    }
    _study->_mlkit_shown = _shown;

    const c8* _status   = "";
    const c8* _download = NULL;
    if(_state == FUDE_MLKIT_UNAVAILABLE) {
        _status = fude_text(FUDE_TEXT_MLKIT_UNAVAILABLE);
    } else if(!_on) {
        _status = fude_text(FUDE_TEXT_MLKIT_OFF);
    } else if(_state == FUDE_MLKIT_READY) {
        _status = fude_text(FUDE_TEXT_MLKIT_READY);
    } else if(_state == FUDE_MLKIT_DOWNLOADING) {
        _status = fude_text(FUDE_TEXT_MLKIT_DOWNLOADING);
    } else if(_state == FUDE_MLKIT_FAILED) {
        _status   = fude_text(FUDE_TEXT_MLKIT_FAILED);
        _download = fude_text(FUDE_TEXT_RETRY);
    } else {
        _status   = fude_text(FUDE_TEXT_MLKIT_MISSING);
        _download = fude_text(FUDE_TEXT_DOWNLOAD);
    }
    rde_ui_label_set_text(_study->mlkit_status, _status);
    rde_ui_node_set_active(rde_ui_button_as_node(_study->mlkit_download), _download != NULL);
    if(_download != NULL) {
        rde_ui_button_set_text(_study->mlkit_download, _download);
    }
    rde_ui_button_set_text(_study->mlkit_toggle, fude_text(_on && _state != FUDE_MLKIT_UNAVAILABLE ? FUDE_TEXT_ON : FUDE_TEXT_OFF));
    if(_on && _state != FUDE_MLKIT_UNAVAILABLE) { fude_kit_button_selected(_study->mlkit_toggle); } else { fude_kit_button_plain(_study->mlkit_toggle); }
    fude_kit_set_enabled(_study->mlkit_toggle, _state != FUDE_MLKIT_UNAVAILABLE);
}

// On: its model is fetched if it is missing.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_study_on_mlkit(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_mlkit_set_enabled(!fude_mlkit_enabled());
    fude_mlkit_prepare();
    fude_study_hand_refresh((fude_ui*)_user_data, false);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_study_on_mlkit_download(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_mlkit_prepare();
    fude_study_hand_refresh((fude_ui*)_user_data, false);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL void fude_study_hand_build(fude_ui* _ui, rde_ui_node* _card) {
    fude_study* _study     = FUDE_STUDY(_ui->app);
    _study->hand_label     = fude_side_label(_ui, _card, fude_text(FUDE_TEXT_SETTINGS_HANDWRITING), FUDE_SIDE_HEADER_PX);
    _study->mlkit_label    = fude_side_label(_ui, _card, fude_text(FUDE_TEXT_SETTINGS_MLKIT), FUDE_SIDE_ROW_PX);
    _study->mlkit_toggle   = fude_kit_button(_card, fude_text(FUDE_TEXT_ON), fude_study_on_mlkit, _ui);
    _study->mlkit_status   = fude_side_label(_ui, _card, "", FUDE_SIDE_SMALL_PX);
    rde_ui_label_set_wrap(_study->mlkit_status, true);
    _study->mlkit_download = fude_kit_button(_card, fude_text(FUDE_TEXT_DOWNLOAD), fude_study_on_mlkit_download, _ui);
    _study->_mlkit_shown   = -1;
}

RDE_INTERNAL f32 fude_study_hand_layout(fude_ui* _ui, f32 _y, f32 _m, f32 _kw) {
    fude_study* _study = FUDE_STUDY(_ui->app);
    const f32   _lw    = _kw - 2.0f * _m;
    fude_kit_place(rde_ui_label_as_node(_study->hand_label), (rde_vec_2F){ _m + _lw * 0.5f, _y }, (rde_vec_2F){ _lw, 30.0f });
    _y -= 40.0f;
    fude_kit_place(rde_ui_label_as_node(_study->mlkit_label), (rde_vec_2F){ _m + _lw * 0.35f, _y }, (rde_vec_2F){ _lw * 0.7f, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_study->mlkit_toggle), (rde_vec_2F){ _kw - _m - 60.0f, _y }, (rde_vec_2F){ 120.0f, 40.0f });
    _y -= 46.0f;
    fude_kit_place(rde_ui_label_as_node(_study->mlkit_status), (rde_vec_2F){ _m + (_lw - 130.0f) * 0.5f, _y }, (rde_vec_2F){ _lw - 130.0f, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_study->mlkit_download), (rde_vec_2F){ _kw - _m - 60.0f, _y }, (rde_vec_2F){ 120.0f, 40.0f });
    return _y - 52.0f;
}

RDE_INTERNAL void fude_study_hand_restyle(fude_ui* _ui) {
    fude_study*       _study = FUDE_STUDY(_ui->app);
    const fude_theme* _t     = fude_theme_active();
    fude_kit_restyle_button(_study->mlkit_toggle);
    fude_kit_restyle_button(_study->mlkit_download);
    rde_ui_label_set_color(_study->hand_label, _t->text_soft);
    rde_ui_label_set_color(_study->mlkit_label, _t->text);
    rde_ui_label_set_color(_study->mlkit_status, _t->text_soft);
}

const fude_extension_section FUDE_STUDY_HANDWRITING = { fude_study_hand_build, fude_study_hand_layout, fude_study_hand_refresh, fude_study_hand_restyle };

// --- pictures read: a document's scanned pages ---------------------------------------------

RDE_INTERNAL b8  fude_study_picture_demo_on = false;
RDE_INTERNAL b8  fude_study_picture_demo_due = false;
RDE_INTERNAL u32 fude_study_picture_demo_w, fude_study_picture_demo_h;

void fude_study_picture_demo(b8 _on) {
#if defined(RDE_DEBUG)
    fude_study_picture_demo_on = _on;
#else
    RDE_UNUSED(_on);
#endif
}

b8 fude_study_picture_read(const u8* _rgba, u32 _w, u32 _h) {
    if(fude_study_picture_demo_on) {
        fude_study_picture_demo_due = true;
        fude_study_picture_demo_w   = _w;
        fude_study_picture_demo_h   = _h;
        return true;
    }
    return fude_textscan_read_page(_rgba, _w, _h);
}

b8 fude_study_picture_lines(struct fude_doc_line* _out, u32 _max, u32* _count) {
    *_count = 0;
    if(fude_study_picture_demo_on) {
        // Made up: four lines down the page's top half, the same on every page.
        if(!fude_study_picture_demo_due) {
            return false;
        }
        fude_study_picture_demo_due = false;
        static const c8* const _says[4] = { "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\xE3\x82\x92\xE5\x8B\x89\xE5\xBC\xB7\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x99",   // 日本語を勉強します
                                            "Scan line two", "\xE3\x81\x82\xE3\x82\x8A\xE3\x81\x8C\xE3\x81\xA8\xE3\x81\x86",   // ありがとう
                                            "The last line read" };
        const f32 _w = (f32)fude_study_picture_demo_w, _h = (f32)fude_study_picture_demo_h;
        for(u32 _i = 0; _i < 4u && _i < _max; _i++) {
            struct fude_doc_line* _l = &_out[(*_count)++];
            memset(_l, 0, sizeof(*_l));
            _l->from = (rde_vec_2F){ _w * 0.12f, _h * (0.18f + 0.08f * (f32)_i) };
            _l->size = (rde_vec_2F){ _w * 0.6f, _h * 0.035f };
            snprintf(_l->text, sizeof(_l->text), "%s", _says[_i]);
        }
        return true;
    }
    fude_textscan_result _r;
    if(!fude_textscan_poll_page(&_r)) {
        return false;
    }
    // Each line's box: its corners' extent, in the picture's pixels.
    for(u32 _i = 0; _i < _r.line_count && *_count < _max; _i++) {
        const fude_textscan_line* _line = &_r.lines[_i];
        rde_vec_2F _lo = _line->corners[0], _hi = _line->corners[0];
        for(u32 _k = 1; _k < 4u; _k++) {
            _lo = (rde_vec_2F){ fminf(_lo.x, _line->corners[_k].x), fminf(_lo.y, _line->corners[_k].y) };
            _hi = (rde_vec_2F){ fmaxf(_hi.x, _line->corners[_k].x), fmaxf(_hi.y, _line->corners[_k].y) };
        }
        struct fude_doc_line* _l = &_out[(*_count)++];
        memset(_l, 0, sizeof(*_l));
        _l->from = _lo;
        _l->size = (rde_vec_2F){ _hi.x - _lo.x, _hi.y - _lo.y };
        snprintf(_l->text, sizeof(_l->text), "%s", _line->text);
    }
    return true;
}
