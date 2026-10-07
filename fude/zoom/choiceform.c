// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/choiceform.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include <math.h>
#include <string.h>

#define FUDE_ZOOM_CHOICEF_PAD 16.0f
#define FUDE_ZOOM_CHOICEF_GAP 8.0f
#define FUDE_ZOOM_CHOICEF_ROW 48.0f

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_choicef_on_choice(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_choice_ref* _ref  = (const fude_zoom_choice_ref*)_user_data;
    fude_zoom_choice_form*      _form = _ref->form;
    const fude_zoom_choice_done _done = _form->done;
    void* const                 _self = _form->self;
    fude_zoom_choice_close(_form);
    if(_done != NULL) {
        _done(_self, _ref->index);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_choicef_on_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_choice_close((fude_zoom_choice_form*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A tap off the card: put away — not the release of the press that opened it.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_choicef_on_dismiss(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_choice_form* _form = (fude_zoom_choice_form*)_user_data;
    if(rde_engine_get_time_now() - _form->opened_at >= 0.6) {
        fude_zoom_choice_close(_form);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

void fude_zoom_choice_build(fude_zoom_choice_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font) {
    memset(_form, 0, sizeof(*_form));
    _form->window = _window;
    _form->font   = _font;
    fude_kit_modal_create(&_form->modal, _root, fude_zoom_choicef_on_dismiss, _form);
    rde_ui_node* _card = rde_ui_image_as_node(_form->modal.card);
    _form->title = rde_ui_label_create(NULL);
    rde_ui_label_set_font(_form->title, _font);
    rde_ui_label_set_font_scale(_form->title, 17.0f / (f32)FUDE_KIT_FONT_SIZE);
    rde_ui_label_set_alignment(_form->title, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_MIDDLE);
    rde_ui_label_set_auto_fit(_form->title, true);   // a longer language shrinks it, never cuts it
    rde_ui_label_set_auto_fit_min_scale(_form->title, 0.6f);
    rde_ui_node_set_raycast_target(rde_ui_label_as_node(_form->title), false);
    rde_ui_node_add_child(_card, rde_ui_label_as_node(_form->title));
    for(u32 _i = 0; _i < FUDE_ZOOM_CHOICE_MAX; _i++) {
        _form->refs[_i]    = (fude_zoom_choice_ref){ _form, _i };
        _form->choices[_i] = fude_kit_button(_card, "", fude_zoom_choicef_on_choice, &_form->refs[_i]);
        rde_ui_node_set_active(rde_ui_button_as_node(_form->choices[_i]), false);
    }
    _form->cancel = fude_kit_button(_card, fude_text(FUDE_TEXT_CANCEL), fude_zoom_choicef_on_cancel, _form);
    fude_zoom_choice_restyle(_form);
    fude_kit_modal_show(&_form->modal, false);
}

void fude_zoom_choice_forget(fude_zoom_choice_form* _form) {
    memset(_form, 0, sizeof(*_form));
}

b8 fude_zoom_choice_shown(const fude_zoom_choice_form* _form) {
    return _form->modal.card != NULL && _form->modal.shown;
}

void fude_zoom_choice_restyle(fude_zoom_choice_form* _form) {
    if(_form->modal.card == NULL) {
        return;
    }
    fude_kit_modal_restyle(&_form->modal, 16.0f);
    rde_ui_label_set_color(_form->title, fude_theme_active()->text);
    for(u32 _i = 0; _i < FUDE_ZOOM_CHOICE_MAX; _i++) {
        fude_kit_button_plain(_form->choices[_i]);
    }
    fude_kit_button_quiet(_form->cancel);
}

RDE_INTERNAL void fude_zoom_choicef_layout(fude_zoom_choice_form* _form) {
    const rde_vec_2F _screen = fude_kit_screen_size(_form->window);
    const f32 _pad = FUDE_ZOOM_CHOICEF_PAD, _gap = FUDE_ZOOM_CHOICEF_GAP, _rh = FUDE_ZOOM_CHOICEF_ROW;
    const f32 _w  = fminf(440.0f, _screen.x - 32.0f);
    const f32 _iw = _w - 2.0f * _pad;
    const f32 _h  = _pad + 28.0f + _gap + (f32)_form->count * (_rh + _gap) + 44.0f + _pad;
    _form->laid_out = _screen;
    fude_kit_modal_place(&_form->modal, _form->window, (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.5f }, (rde_vec_2F){ _w, _h });
    f32 _y = _h - _pad;
    fude_kit_place(rde_ui_label_as_node(_form->title), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 14.0f }, (rde_vec_2F){ _iw, 28.0f });
    _y -= 28.0f + _gap;
    for(u32 _i = 0; _i < FUDE_ZOOM_CHOICE_MAX; _i++) {
        const b8 _on = _i < _form->count;
        rde_ui_node_set_active(rde_ui_button_as_node(_form->choices[_i]), _on);
        if(_on) {
            fude_kit_place(rde_ui_button_as_node(_form->choices[_i]), (rde_vec_2F){ _pad + _iw * 0.5f, _y - _rh * 0.5f }, (rde_vec_2F){ _iw, _rh });
            _y -= _rh + _gap;
        }
    }
    fude_kit_place(rde_ui_button_as_node(_form->cancel), (rde_vec_2F){ _w - _pad - 60.0f, _pad + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
}

void fude_zoom_choice_open(fude_zoom_choice_form* _form, const c8* _title, const c8* const* _labels, const c8* const* _icons, u32 _count,
                           fude_zoom_choice_done _done, void* _self) {
    if(_form->modal.card == NULL) {
        return;
    }
    _form->count = _count < FUDE_ZOOM_CHOICE_MAX ? _count : FUDE_ZOOM_CHOICE_MAX;
    _form->done  = _done;
    _form->self  = _self;
    rde_ui_label_set_text(_form->title, _title);
    for(u32 _i = 0; _i < _form->count; _i++) {
        rde_ui_button_set_text(_form->choices[_i], _labels[_i]);
        if(_icons != NULL && _icons[_i] != NULL) {
            fude_kit_icon(_form->choices[_i], _icons[_i], FUDE_KIT_ICON_LEFT, 18.0f);
        }
    }
    fude_kit_modal_show(&_form->modal, true);
    _form->opened_at = rde_engine_get_time_now();
    fude_zoom_choicef_layout(_form);
    fude_zoom_choice_restyle(_form);
}

void fude_zoom_choice_close(fude_zoom_choice_form* _form) {
    if(_form->modal.card != NULL) {
        fude_kit_modal_show(&_form->modal, false);
    }
}

void fude_zoom_choice_update(fude_zoom_choice_form* _form) {
    if(!fude_zoom_choice_shown(_form)) {
        return;
    }
    const rde_vec_2F _screen = fude_kit_screen_size(_form->window);
    if(_screen.x != _form->laid_out.x || _screen.y != _form->laid_out.y) {
        fude_zoom_choicef_layout(_form);
    }
}

void fude_zoom_choice_look(fude_zoom_choice_form* _form, u32 _index) {
    if(fude_zoom_choice_shown(_form) && _index < _form->count) {
        fude_zoom_choicef_on_choice(NULL, NULL, &_form->refs[_index]);
    }
}
