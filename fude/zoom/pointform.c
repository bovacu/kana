// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/pointform.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/widgets/draw.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FUDE_ZOOM_POINTF_PAD      16.0f
#define FUDE_ZOOM_POINTF_GAP      8.0f
#define FUDE_ZOOM_POINTF_ROW_H    44.0f
#define FUDE_ZOOM_POINTF_FIELD_PX 18u

// A field's text (a copy, "" when empty), into _out.
RDE_INTERNAL void fude_zoom_pointf_text(rde_ui_text_editor* _field, c8* _out, usize _size) {
    _out[0] = 0;
    const usize _bytes = rde_ui_text_editor_get_byte_count(_field);
    if(_bytes == 0) {
        return;
    }
    c8* _t = rde_ui_text_editor_get_text(_field, 0, _bytes);
    if(_t != NULL) {
        snprintf(_out, _size, "%s", _t);
        rde_ui_text_editor_free_text(_field, _t);
    }
}

RDE_INTERNAL void fude_zoom_pointf_clear(fude_zoom_point_form* _form) {
    _form->setting = true;
    for(u32 _i = 0; _i < 2u; _i++) {
        const usize _bytes = rde_ui_text_editor_get_byte_count(_form->fields[_i]);
        if(_bytes > 0) {
            rde_ui_text_editor_delete_range(_form->fields[_i], 0, _bytes);
        }
    }
    _form->setting = false;
}

RDE_INTERNAL void fude_zoom_pointf_set(fude_zoom_point_form* _form, u32 _i, const c8* _text) {
    _form->setting = true;
    const usize _bytes = rde_ui_text_editor_get_byte_count(_form->fields[_i]);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_form->fields[_i], 0, _bytes);
    }
    if(_text[0] != 0) {
        rde_ui_text_editor_insert_at(_form->fields[_i], 0, _text, strlen(_text));
    }
    _form->setting = false;
}

// Field _i read (the second of Length and angle: degrees). False: it cannot be; *_empty: nothing in it.
RDE_INTERNAL b8 fude_zoom_pointf_read(const fude_zoom_point_form* _form, u32 _i, f64* _out, b8* _empty) {
    c8 _t[96];
    fude_zoom_pointf_text(_form->fields[_i], _t, sizeof(_t));
    *_empty = _t[0] == 0;
    fude_zoom_units_error _e;
    if(_form->mode == FUDE_ZOOM_POINT_POLAR && _i == 1u) {
        return fude_zoom_units_angle(_t, NULL, _out, &_e) && isfinite(*_out);
    }
    return fude_zoom_units_length(_t, _form->units.unit, NULL, _out, &_e) && isfinite(*_out);
}

// The names and the hint for its mode, its chips lit.
RDE_INTERNAL void fude_zoom_pointf_show(fude_zoom_point_form* _form) {
    static const u32 _names[FUDE_ZOOM_POINT_MODES][2] = {
        { 0u, 0u },
        { FUDE_TEXT_ZOOM_POINT_ACROSS, FUDE_TEXT_ZOOM_POINT_UP },
        { FUDE_TEXT_ZOOM_SIZE_LENGTH, FUDE_TEXT_ZOOM_SIZE_ANGLE },
    };
    for(u32 _i = 0; _i < FUDE_ZOOM_POINT_MODES; _i++) {
        fude_kit_button_chip(_form->modes[_i], _form->mode == _i);
        rde_ui_node_set_active(rde_ui_button_as_node(_form->modes[_i]), _i == FUDE_ZOOM_POINT_AT || _form->has_last);
    }
    for(u32 _i = 0; _i < 2u; _i++) {
        rde_ui_label_set_text(_form->names[_i], _form->mode == FUDE_ZOOM_POINT_AT ? (_i == 0u ? "X" : "Y") : fude_text(_names[_form->mode][_i]));
    }
    const u32 _hint = _form->mode == FUDE_ZOOM_POINT_AT ? (_form->on_sheet ? FUDE_TEXT_ZOOM_SIZE_FROM : FUDE_TEXT_ZOOM_POINT_FROM_MIDDLE) :
                      _form->mode == FUDE_ZOOM_POINT_BY ? FUDE_TEXT_ZOOM_POINT_FROM_LAST : FUDE_TEXT_ZOOM_POINT_POLAR_HINT;
    rde_ui_label_set_text(_form->hint, fude_text(_hint));
}

// --- presses -------------------------------------------------------------------------------

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_pointf_on_mode(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_point_ref* _ref = (const fude_zoom_point_ref*)_user_data;
    _ref->form->mode = (u8)_ref->index;
    rde_ui_label_set_text(_ref->form->problem, "");
    fude_zoom_pointf_show(_ref->form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL void fude_zoom_pointf_on_change(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    fude_zoom_point_form* _form = (fude_zoom_point_form*)_user_data;
    if(_form != NULL && !_form->setting) {
        rde_ui_label_set_text(_form->problem, "");
    }
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_pointf_on_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_point_close((fude_zoom_point_form*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A tap off the card: put away — not the release of the press that opened it.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_pointf_on_dismiss(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_point_form* _form = (fude_zoom_point_form*)_user_data;
    if(rde_engine_get_time_now() - _form->opened_at >= 0.6) {
        fude_zoom_point_close(_form);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Add point (_finish false) or Finish: both fields read (Finish: both empty is fine, nothing added); one that cannot be
// keeps the card up, said.
RDE_INTERNAL void fude_zoom_pointf_go(fude_zoom_point_form* _form, b8 _finish) {
    fude_zoom_point_values _typed = { _form->mode, 0.0, 0.0 };
    b8 _empty[2] = { false, false };
    b8 _ok[2];
    _ok[0] = fude_zoom_pointf_read(_form, 0u, &_typed.a, &_empty[0]);
    _ok[1] = fude_zoom_pointf_read(_form, 1u, &_typed.b, &_empty[1]);
    const b8 _none = _empty[0] && _empty[1];
    if(!(_finish && _none)) {
        for(u32 _i = 0; _i < 2u; _i++) {
            if(_ok[_i]) {
                continue;
            }
            c8 _t[96], _say[200];
            fude_zoom_pointf_text(_form->fields[_i], _t, sizeof(_t));
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_SIZE_BAD, FUDE_TS(_t[0] != 0 ? _t : "?"));
            rde_ui_label_set_text(_form->problem, _say);
            rde_ui_node_focus(rde_ui_text_editor_as_node(_form->fields[_i]));
            return;
        }
    }
    const fude_zoom_point_done _done = _form->done;
    void* const                _self = _form->self;
    if(_finish) {
        fude_zoom_point_close(_form);
    }
    if(_done != NULL) {
        _done(_self, _finish && _none ? NULL : &_typed, _finish);
    }
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_pointf_on_add(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_pointf_go((fude_zoom_point_form*)_user_data, false);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_pointf_on_finish(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_pointf_go((fude_zoom_point_form*)_user_data, true);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Enter: on to the second field; in it, Add point.
RDE_INTERNAL void fude_zoom_pointf_on_submit(rde_ui_node* _node, any _user_data) {
    fude_zoom_point_form* _form = (fude_zoom_point_form*)_user_data;
    if(_form == NULL) {
        return;
    }
    if(_node == rde_ui_text_editor_as_node(_form->fields[0])) {
        rde_ui_node_focus(rde_ui_text_editor_as_node(_form->fields[1]));
        return;
    }
    fude_zoom_pointf_go(_form, false);
}

// --- built, laid out --------------------------------------------------------------------------

RDE_INTERNAL rde_ui_label* fude_zoom_pointf_label(fude_zoom_point_form* _form, rde_ui_node* _parent, const c8* _text, f32 _px) {
    rde_ui_label* _label = rde_ui_label_create(NULL);
    rde_ui_label_set_font(_label, _form->font);
    rde_ui_label_set_text(_label, _text);
    rde_ui_label_set_font_scale(_label, _px / (f32)FUDE_KIT_FONT_SIZE);
    rde_ui_label_set_alignment(_label, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_MIDDLE);
    rde_ui_label_set_auto_fit(_label, true);   // a longer language shrinks it, never cuts it
    rde_ui_label_set_auto_fit_min_scale(_label, 0.6f);
    rde_ui_node_set_raycast_target(rde_ui_label_as_node(_label), false);
    rde_ui_node_add_child(_parent, rde_ui_label_as_node(_label));
    return _label;
}

void fude_zoom_point_build(fude_zoom_point_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font) {
    memset(_form, 0, sizeof(*_form));
    _form->window = _window;
    _form->font   = _font;
    fude_kit_modal_create(&_form->modal, _root, fude_zoom_pointf_on_dismiss, _form);
    rde_ui_node* _card = rde_ui_image_as_node(_form->modal.card);
    _form->title = fude_zoom_pointf_label(_form, _card, fude_text(FUDE_TEXT_ZOOM_POINT_TITLE), 17.0f);
    static const u32 _mode_names[FUDE_ZOOM_POINT_MODES] = { FUDE_TEXT_ZOOM_POINT_AT, FUDE_TEXT_ZOOM_POINT_BY, FUDE_TEXT_ZOOM_POINT_POLAR };
    for(u32 _i = 0; _i < FUDE_ZOOM_POINT_MODES; _i++) {
        _form->refs[_i]  = (fude_zoom_point_ref){ _form, _i };
        _form->modes[_i] = fude_kit_button(_card, fude_text(_mode_names[_i]), fude_zoom_pointf_on_mode, &_form->refs[_i]);
    }
    for(u32 _i = 0; _i < 2u; _i++) {
        _form->names[_i] = fude_zoom_pointf_label(_form, _card, "", 15.0f);
        rde_ui_text_editor* _f = rde_ui_text_editor_create(_font, NULL);
        rde_ui_text_editor_set_multiline(_f, false);
        rde_ui_text_editor_set_allow_multicursor(_f, false);
        rde_ui_text_editor_set_font_size(_f, FUDE_ZOOM_POINTF_FIELD_PX);
        rde_ui_text_editor_set_max_chars(_f, 32u);
        rde_ui_text_editor_set_content_insets(_f, 10.0f, 8.0f, 10.0f, 8.0f);
        rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_f), (any)_form);
        rde_ui_text_editor_set_on_change(_f, fude_zoom_pointf_on_change);
        rde_ui_text_editor_set_on_submit(_f, fude_zoom_pointf_on_submit);
        fude_kit_field_box(_card, _f);
        _form->fields[_i] = _f;
    }
    _form->hint    = fude_zoom_pointf_label(_form, _card, "", 13.0f);
    _form->problem = fude_zoom_pointf_label(_form, _card, "", 14.0f);
    _form->close   = fude_kit_button(_card, fude_text(FUDE_TEXT_CLOSE), fude_zoom_pointf_on_close, _form);
    _form->finish  = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_POINT_FINISH), fude_zoom_pointf_on_finish, _form);
    _form->add     = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_POINT_ADD), fude_zoom_pointf_on_add, _form);
    fude_zoom_point_restyle(_form);
    fude_kit_modal_show(&_form->modal, false);
}

void fude_zoom_point_forget(fude_zoom_point_form* _form) {
    memset(_form, 0, sizeof(*_form));
}

b8 fude_zoom_point_shown(const fude_zoom_point_form* _form) {
    return _form->modal.card != NULL && _form->modal.shown;
}

void fude_zoom_point_restyle(fude_zoom_point_form* _form) {
    if(_form->modal.card == NULL) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    fude_kit_modal_restyle(&_form->modal, 16.0f);
    rde_ui_label_set_color(_form->title, _t->text);
    for(u32 _i = 0; _i < 2u; _i++) {
        rde_ui_label_set_color(_form->names[_i], _t->text_soft);
        fude_kit_style_field(_form->fields[_i]);
    }
    rde_ui_label_set_color(_form->hint, _t->text_soft);
    rde_ui_label_set_color(_form->problem, _t->score_poor);
    for(u32 _i = 0; _i < FUDE_ZOOM_POINT_MODES; _i++) {
        fude_kit_button_chip(_form->modes[_i], _form->mode == _i);
    }
    fude_kit_button_plain(_form->close);
    fude_kit_button_plain(_form->finish);
    fude_kit_button_primary(_form->add);
}

// The card high on the screen (the keyboard comes up under it).
RDE_INTERNAL void fude_zoom_pointf_layout(fude_zoom_point_form* _form) {
    const rde_vec_2F _screen = fude_kit_screen_size(_form->window);
    const f32 _pad = FUDE_ZOOM_POINTF_PAD, _gap = FUDE_ZOOM_POINTF_GAP, _rh = FUDE_ZOOM_POINTF_ROW_H, _ch = 38.0f;
    const f32 _w  = fminf(560.0f, _screen.x - 32.0f);
    const f32 _iw = _w - 2.0f * _pad;
    const f32 _h  = _pad + 28.0f + _gap + _ch + _gap + _rh + _gap + 20.0f + 22.0f + _gap + 44.0f + _pad;
    _form->laid_out = _screen;
    fude_kit_modal_place(&_form->modal, _form->window, (rde_vec_2F){ _screen.x * 0.5f, fmaxf(_screen.y - 90.0f - _h * 0.5f, _h * 0.5f + 8.0f) }, (rde_vec_2F){ _w, _h });
    f32 _y = _h - _pad;
    fude_kit_place(rde_ui_label_as_node(_form->title), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 14.0f }, (rde_vec_2F){ _iw, 28.0f });
    _y -= 28.0f + _gap;
    const f32 _tw = (_iw - 2.0f * _gap) / 3.0f;
    for(u32 _i = 0; _i < FUDE_ZOOM_POINT_MODES; _i++) {
        fude_kit_place(rde_ui_button_as_node(_form->modes[_i]), (rde_vec_2F){ _pad + (f32)_i * (_tw + _gap) + _tw * 0.5f, _y - _ch * 0.5f }, (rde_vec_2F){ _tw, _ch });
    }
    _y -= _ch + _gap;
    const f32 _col = (_iw - 2.0f * _gap) * 0.5f;
    const f32 _nw  = fminf(84.0f, _col * 0.42f), _fw = _col - _nw - 4.0f;
    for(u32 _i = 0; _i < 2u; _i++) {
        const f32 _x = _pad + (f32)_i * (_col + 2.0f * _gap);
        fude_kit_place(rde_ui_label_as_node(_form->names[_i]), (rde_vec_2F){ _x + _nw * 0.5f, _y - _rh * 0.5f }, (rde_vec_2F){ _nw, _rh });
        fude_kit_place(fude_kit_field_node(_form->fields[_i]), (rde_vec_2F){ _x + _nw + 4.0f + _fw * 0.5f, _y - _rh * 0.5f }, (rde_vec_2F){ _fw, _rh });
    }
    _y -= _rh + _gap;
    fude_kit_place(rde_ui_label_as_node(_form->hint), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 10.0f }, (rde_vec_2F){ _iw, 20.0f });
    _y -= 20.0f;
    fude_kit_place(rde_ui_label_as_node(_form->problem), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 11.0f }, (rde_vec_2F){ _iw, 22.0f });
    const f32 _bw = fminf(140.0f, (_iw - 2.0f * _gap) / 3.0f);
    fude_kit_place(rde_ui_button_as_node(_form->close), (rde_vec_2F){ _pad + _bw * 0.5f, _pad + 22.0f }, (rde_vec_2F){ _bw, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_form->finish), (rde_vec_2F){ _w - _pad - _bw - _gap - _bw * 0.5f, _pad + 22.0f }, (rde_vec_2F){ _bw, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_form->add), (rde_vec_2F){ _w - _pad - _bw * 0.5f, _pad + 22.0f }, (rde_vec_2F){ _bw, 44.0f });
}

void fude_zoom_point_open(fude_zoom_point_form* _form, u8 _mode, b8 _has_last, b8 _on_sheet, const fude_zoom_units_style* _units,
                          fude_zoom_point_done _done, void* _self) {
    if(_form->modal.card == NULL) {
        return;
    }
    _form->has_last = _has_last;
    _form->on_sheet = _on_sheet;
    _form->mode     = _has_last && _mode < FUDE_ZOOM_POINT_MODES ? _mode : (u8)FUDE_ZOOM_POINT_AT;
    _form->units    = *_units;
    _form->done     = _done;
    _form->self     = _self;
    rde_ui_label_set_text(_form->problem, "");
    fude_zoom_pointf_clear(_form);
    fude_kit_modal_show(&_form->modal, true);
    _form->opened_at = rde_engine_get_time_now();
    fude_zoom_pointf_layout(_form);
    fude_zoom_point_restyle(_form);
    fude_zoom_pointf_show(_form);
    rde_ui_node_focus(rde_ui_text_editor_as_node(_form->fields[0]));
}

void fude_zoom_point_added(fude_zoom_point_form* _form) {
    if(!fude_zoom_point_shown(_form)) {
        return;
    }
    _form->has_last = true;
    rde_ui_label_set_text(_form->problem, "");
    fude_zoom_pointf_clear(_form);
    fude_zoom_pointf_show(_form);
    rde_ui_node_focus(rde_ui_text_editor_as_node(_form->fields[0]));
}

void fude_zoom_point_close(fude_zoom_point_form* _form) {
    if(_form->modal.card != NULL) {
        fude_kit_modal_show(&_form->modal, false);
    }
}

void fude_zoom_point_update(fude_zoom_point_form* _form) {
    if(!fude_zoom_point_shown(_form)) {
        return;
    }
    const rde_vec_2F _screen = fude_kit_screen_size(_form->window);
    if(_screen.x != _form->laid_out.x || _screen.y != _form->laid_out.y) {
        fude_zoom_pointf_layout(_form);
    }
}

void fude_zoom_point_look(fude_zoom_point_form* _form, u8 _mode, const c8* _a, const c8* _b, u8 _press) {
    if(!fude_zoom_point_shown(_form)) {
        return;
    }
    if(_mode < FUDE_ZOOM_POINT_MODES && (_mode == FUDE_ZOOM_POINT_AT || _form->has_last)) {
        _form->mode = _mode;
        fude_zoom_pointf_show(_form);
    }
    if(_a != NULL) {
        fude_zoom_pointf_set(_form, 0u, _a);
    }
    if(_b != NULL) {
        fude_zoom_pointf_set(_form, 1u, _b);
    }
    if(_press == 1u) {
        fude_zoom_pointf_go(_form, false);
    } else if(_press == 2u) {
        fude_zoom_pointf_go(_form, true);
    }
}
