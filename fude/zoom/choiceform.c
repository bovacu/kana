// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/choiceform.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"

// ===========================================================================
// See choiceform.h.
//
// The card is the kit's form (kit.h: its title, Cancel, a tap off it, laying it out again). Its widgets are made once,
// FUDE_ZOOM_CHOICE_MAX choice buttons among them, and every card opened after is those same widgets with new words:
// the buttons past the count are hidden. Its own functions and constants are prefixed fude_zoom_choicef_ and
// FUDE_ZOOM_CHOICEF_. Sizes are UI points.
// ===========================================================================

#define FUDE_ZOOM_CHOICEF_ROW 48.0f   // A choice button's height.

// A choice pressed: puts the card away, then calls the opener's done with the choice's index. The card goes first, so a
// callback that opens another card leaves that one up.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_choicef_on_choice(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_choice_ref* _ref  = (const fude_zoom_choice_ref*)_user_data;
    fude_zoom_choice_form*      _form = _ref->form;
    const fude_zoom_choice_done _done = _form->done;
    any const                 _self = _form->self;
    fude_zoom_choice_close(_form);
    if(_done != NULL) {
        _done(_self, _ref->index);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// See the header. Every choice button is made here, hidden, each handed its own ref (the form and its index) as its
// user data.
void fude_zoom_choice_build(fude_zoom_choice_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font) {
    rde_memset(_form, 0, sizeof(*_form));
    rde_ui_node* _card = fude_kit_form_build(&_form->card, _root, _window, _font, NULL);
    for(u32 _i = 0; _i < FUDE_ZOOM_CHOICE_MAX; _i++) {
        _form->refs[_i]    = (fude_zoom_choice_ref){ _form, _i };
        _form->choices[_i] = fude_kit_button(_card, "", fude_zoom_choicef_on_choice, &_form->refs[_i]);
        rde_ui_node_set_active(rde_ui_button_as_node(_form->choices[_i]), false);
    }
    _form->cancel = fude_kit_button(_card, fude_text(FUDE_TEXT_CANCEL), fude_kit_form_on_cancel, &_form->card);
    fude_zoom_choice_restyle(_form);
    fude_kit_modal_show(&_form->card.modal, false);
}

// See the header: with the card NULL, every other call does nothing.
void fude_zoom_choice_forget(fude_zoom_choice_form* _form) {
    rde_memset(_form, 0, sizeof(*_form));
}

// See the header: false before the card is built and after it is forgotten.
b8 fude_zoom_choice_shown(const fude_zoom_choice_form* _form) {
    return fude_kit_form_shown(&_form->card);
}

// See the header: the choices plain buttons and Cancel a quiet one.
void fude_zoom_choice_restyle(fude_zoom_choice_form* _form) {
    if(!fude_kit_form_restyle(&_form->card)) {
        return;
    }
    for(u32 _i = 0; _i < FUDE_ZOOM_CHOICE_MAX; _i++) {
        fude_kit_button_plain(_form->choices[_i]);
    }
    fude_kit_button_quiet(_form->cancel);
}

// Lays the card out in the middle of the screen for its count of choices: at most 440 points wide (16 less than the
// screen on each side), the title at the top, the choices under it, and Cancel at the bottom right.
RDE_INTERNAL void fude_zoom_choicef_layout(fude_zoom_choice_form* _form) {
    const rde_vec_2F _screen = fude_kit_screen_size(_form->card.window);
    const f32 _pad = FUDE_KIT_FORM_PAD, _gap = FUDE_KIT_FORM_GAP, _rh = FUDE_ZOOM_CHOICEF_ROW;
    const f32 _w  = rde_math_min_f32(440.0f, _screen.x - 32.0f);
    const f32 _iw = _w - 2.0f * _pad;
    const f32 _h  = _pad + 28.0f + _gap + (f32)_form->count * (_rh + _gap) + 44.0f + _pad;
    f32 _y = fude_kit_form_place(&_form->card, _screen, FUDE_KIT_FORM_MIDDLE, _w, _h);
    for(u32 _i = 0; _i < FUDE_ZOOM_CHOICE_MAX; _i++) {
        const b8 _on = _i < _form->count;
        rde_ui_node_set_active(rde_ui_button_as_node(_form->choices[_i]), _on);
        if(_on) {
            fude_kit_place(rde_ui_button_as_node(_form->choices[_i]), (rde_vec_2F){ _pad + _iw * 0.5f, _y - _rh * 0.5f }, (rde_vec_2F){ _iw, _rh });
            _y -= _rh + _gap;
        }
    }
    fude_kit_form_buttons(NULL, _form->cancel, _w);
}

// See the header. Choices past FUDE_ZOOM_CHOICE_MAX are left off; the card's opening time is kept for the tap off it.
void fude_zoom_choice_open(fude_zoom_choice_form* _form, const c8* _title, const c8* const* _labels, const c8* const* _icons, u32 _count,
                           fude_zoom_choice_done _done, any _self) {
    if(_form->card.modal.card == NULL) {
        return;
    }
    _form->count = _count < FUDE_ZOOM_CHOICE_MAX ? _count : FUDE_ZOOM_CHOICE_MAX;
    _form->done  = _done;
    _form->self  = _self;
    rde_ui_label_set_text(_form->card.title, _title);
    for(u32 _i = 0; _i < _form->count; _i++) {
        rde_ui_button_set_text(_form->choices[_i], _labels[_i]);
        // The same buttons serve every card: a choice with no icon this time loses the one an earlier card gave it.
        if(_icons != NULL && _icons[_i] != NULL) {
            fude_kit_icon(_form->choices[_i], _icons[_i], FUDE_KIT_ICON_LEFT, 18.0f);
        } else {
            fude_kit_icon_clear(_form->choices[_i]);
        }
    }
    fude_kit_form_show(&_form->card);
    fude_zoom_choicef_layout(_form);
    fude_zoom_choice_restyle(_form);
}

// See the header.
void fude_zoom_choice_close(fude_zoom_choice_form* _form) {
    fude_kit_form_close(&_form->card);
}

// See the header: lays the card out again only while it is up and the screen's size has changed.
void fude_zoom_choice_update(fude_zoom_choice_form* _form) {
    if(fude_kit_form_resized(&_form->card)) {
        fude_zoom_choicef_layout(_form);
    }
}

// See the header: presses the choice as a tap would, through its button's own handler.
void fude_zoom_choice_look(fude_zoom_choice_form* _form, u32 _index) {
    if(fude_zoom_choice_shown(_form) && _index < _form->count) {
        fude_zoom_choicef_on_choice(NULL, NULL, &_form->refs[_index]);
    }
}
