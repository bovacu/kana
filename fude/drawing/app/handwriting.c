// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/app/handwriting.h"
#include "drawing/base/redraw.h"
#include "drawing/app/ui.h"
#include "drawing/app/side.h"
#include "drawing/widgets/kit.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "drawing/ink/mlkit.h"

// ===========================================================================
// See handwriting.h.
// ===========================================================================

// Shows ML Kit's switch and its model's state. To save work each frame, it changes the labels only when what they show
// has changed (the state, plus 100 when ML Kit is off), unless _force asks for it anyway.
void fude_handwriting_refresh(fude_ui* _ui, b8 _force) {
    fude_handwriting* _hw    = &_ui->handwriting;
    const FUDE_MLKIT_ _state = fude_mlkit_state();
    const b8          _on    = fude_mlkit_enabled();
    const i32         _shown = (i32)_state + (_on ? 0 : 100);
    if(_state == FUDE_MLKIT_DOWNLOADING) {
        fude_redraw_after(FUDE_REDRAW_POLL);   // (render on demand: the download's end is polled, with no event)
    }
    if(_hw->toggle == NULL || (!_force && _shown == _hw->_shown)) {
        return;
    }
    _hw->_shown = _shown;
    fude_redraw_changed();

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
    rde_ui_label_set_text(_hw->status, _status);
    if(rde_ui_node_is_active(rde_ui_button_as_node(_hw->download)) != (_download != NULL)) {
        fude_side_relayout(_ui);   // the state's line takes the button's room, or gives it back
    }
    rde_ui_node_set_active(rde_ui_button_as_node(_hw->download), _download != NULL);
    if(_download != NULL) {
        rde_ui_button_set_text(_hw->download, _download);
    }
    rde_ui_button_set_text(_hw->toggle, fude_text(_on && _state != FUDE_MLKIT_UNAVAILABLE ? FUDE_TEXT_ON : FUDE_TEXT_OFF));
    if(_on && _state != FUDE_MLKIT_UNAVAILABLE) { fude_kit_button_selected(_hw->toggle); } else { fude_kit_button_plain(_hw->toggle); }
    fude_kit_set_enabled(_hw->toggle, _state != FUDE_MLKIT_UNAVAILABLE);
}

// The toggle turns ML Kit on or off. When it is turned on, its model is fetched if it is missing.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_handwriting_on_toggle(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_mlkit_set_enabled(!fude_mlkit_enabled());
    fude_mlkit_prepare();
    fude_handwriting_refresh((fude_ui*)_user_data, false);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Download or Retry: fetches the model again.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_handwriting_on_download(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_mlkit_prepare();
    fude_handwriting_refresh((fude_ui*)_user_data, false);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Builds the section's labels and buttons on Settings' card.
void fude_handwriting_build(fude_ui* _ui, rde_ui_node* _card) {
    fude_handwriting* _hw = &_ui->handwriting;
    _hw->header   = fude_side_label(_ui, _card, fude_text(FUDE_TEXT_SETTINGS_HANDWRITING), FUDE_SIDE_HEADER_PX);
    _hw->label    = fude_side_label(_ui, _card, fude_text(FUDE_TEXT_SETTINGS_MLKIT), FUDE_SIDE_ROW_PX);
    _hw->toggle   = fude_kit_button(_card, fude_text(FUDE_TEXT_ON), fude_handwriting_on_toggle, _ui);
    _hw->status   = fude_side_label(_ui, _card, "", FUDE_SIDE_SMALL_PX);
    rde_ui_label_set_wrap(_hw->status, true);
    _hw->download = fude_kit_button(_card, fude_text(FUDE_TEXT_DOWNLOAD), fude_handwriting_on_download, _ui);
    _hw->_shown   = -1;
}

// Lays the section out from _y down (_m is the margin and _kw the card's width, in screen units), and returns where
// the next section starts.
f32 fude_handwriting_layout(fude_ui* _ui, f32 _y, f32 _m, f32 _kw) {
    fude_handwriting* _hw = &_ui->handwriting;
    const f32         _lw = _kw - 2.0f * _m;
    fude_kit_place(rde_ui_label_as_node(_hw->header), (rde_vec_2F){ _m + _lw * 0.5f, _y }, (rde_vec_2F){ _lw, 30.0f });
    _y -= 40.0f;
    fude_kit_place(rde_ui_label_as_node(_hw->label), (rde_vec_2F){ _m + rde_math_min_f32(_lw * 0.7f, _lw - 130.0f) * 0.5f, _y }, (rde_vec_2F){ rde_math_min_f32(_lw * 0.7f, _lw - 130.0f), 44.0f });   // it keeps clear of the toggle
    fude_kit_place(rde_ui_button_as_node(_hw->toggle), (rde_vec_2F){ _kw - _m - 60.0f, _y }, (rde_vec_2F){ 120.0f, 40.0f });
    _y -= 46.0f;
    const f32 _sw = rde_ui_node_is_active(rde_ui_button_as_node(_hw->download)) ? _lw - 130.0f : _lw;   // beside Download, when it shows
    fude_kit_place(rde_ui_label_as_node(_hw->status), (rde_vec_2F){ _m + _sw * 0.5f, _y }, (rde_vec_2F){ _sw, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_hw->download), (rde_vec_2F){ _kw - _m - 60.0f, _y }, (rde_vec_2F){ 120.0f, 40.0f });
    return _y - 52.0f;
}

// Gives the section the active theme's colours.
void fude_handwriting_restyle(fude_ui* _ui) {
    fude_handwriting* _hw = &_ui->handwriting;
    const fude_theme* _t  = fude_theme_active();
    fude_kit_restyle_button(_hw->toggle);
    fude_kit_restyle_button(_hw->download);
    rde_ui_label_set_color(_hw->header, _t->text_soft);
    rde_ui_label_set_color(_hw->label, _t->text);
    rde_ui_label_set_color(_hw->status, _t->text_soft);
}
