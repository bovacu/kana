// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/pointform.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/base/draw.h"
#include "drawing/widgets/kit.h"

// ===========================================================================
// See pointform.h.
//
// The card is the kit's form (kit.h: its title, a tap off it, laying it out again, its fields and what could not be
// read). Internal names start fude_zoom_pointf_ (FUDE_ZOOM_POINTF_ for its sizes). The file goes: the fields read and
// written, the presses, then the card built and laid out (in the card's units: points, Y up from its bottom left).
// ===========================================================================

#define FUDE_ZOOM_POINTF_ROW_H    44.0f   // the fields' row

// Empties both fields, as the card's own change (kit.h: fude_kit_form_set), so it is not taken for typing (which
// clears the problem said).
RDE_INTERNAL void fude_zoom_pointf_clear(fude_zoom_point_form* _form) {
    for(u32 _i = 0; _i < 2u; _i++) {
        fude_kit_form_set(&_form->card, _form->fields[_i], "");
    }
}

// Field _i read (the second of Length and angle: degrees). False: it cannot be; *_empty: nothing in it.
RDE_INTERNAL b8 fude_zoom_pointf_read(const fude_zoom_point_form* _form, u32 _i, f64* _out, b8* _empty) {
    c8 _t[96];
    fude_kit_field_text(_form->fields[_i], _t, sizeof(_t));
    *_empty = _t[0] == 0;
    fude_zoom_units_error _e;
    if(_form->mode == FUDE_ZOOM_POINT_POLAR && _i == 1u) {
        return fude_zoom_units_angle(_t, NULL, _out, &_e) && rde_math_is_finite_f64(*_out);
    }
    return fude_zoom_units_length(_t, _form->units.unit, NULL, _out, &_e) && rde_math_is_finite_f64(*_out);
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

// A mode chip: its mode chosen, any problem said cleared.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_pointf_on_mode(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_point_ref* _ref = (const fude_zoom_point_ref*)_user_data;
    _ref->form->mode = (u8)_ref->index;
    rde_ui_label_set_text(_ref->form->problem, "");
    fude_zoom_pointf_show(_ref->form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A field typed in: the problem said is cleared (not when the card itself filled or emptied it).
RDE_INTERNAL void fude_zoom_pointf_on_change(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    fude_zoom_point_form* _form = (fude_zoom_point_form*)_user_data;
    if(_form != NULL && !_form->card.setting) {
        rde_ui_label_set_text(_form->problem, "");
    }
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
            fude_kit_form_bad(_form->problem, _form->fields[_i], FUDE_TEXT_ZOOM_SIZE_BAD);
            return;
        }
    }
    const fude_zoom_point_done _done = _form->done;
    any const                _self = _form->self;
    if(_finish) {
        fude_zoom_point_close(_form);
    }
    if(_done != NULL) {
        _done(_self, _finish && _none ? NULL : &_typed, _finish);
    }
}

// Add point.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_pointf_on_add(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_pointf_go((fude_zoom_point_form*)_user_data, false);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Finish.
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

// See the header.
void fude_zoom_point_build(fude_zoom_point_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font) {
    rde_memset(_form, 0, sizeof(*_form));
    rde_ui_node* _card = fude_kit_form_build(&_form->card, _root, _window, _font, fude_text(FUDE_TEXT_ZOOM_POINT_TITLE));
    static const u32 _mode_names[FUDE_ZOOM_POINT_MODES] = { FUDE_TEXT_ZOOM_POINT_AT, FUDE_TEXT_ZOOM_POINT_BY, FUDE_TEXT_ZOOM_POINT_POLAR };
    for(u32 _i = 0; _i < FUDE_ZOOM_POINT_MODES; _i++) {
        _form->refs[_i]  = (fude_zoom_point_ref){ _form, _i };
        _form->modes[_i] = fude_kit_button(_card, fude_text(_mode_names[_i]), fude_zoom_pointf_on_mode, &_form->refs[_i]);
    }
    for(u32 _i = 0; _i < 2u; _i++) {
        _form->names[_i]  = fude_kit_form_label(&_form->card, _card, "", 15.0f);
        _form->fields[_i] = fude_kit_form_field(&_form->card, _card, 32u, NULL, (any)_form, fude_zoom_pointf_on_change, fude_zoom_pointf_on_submit);
    }
    _form->hint    = fude_kit_form_label(&_form->card, _card, "", 13.0f);
    _form->problem = fude_kit_form_label(&_form->card, _card, "", 14.0f);
    _form->close   = fude_kit_button(_card, fude_text(FUDE_TEXT_CLOSE), fude_kit_form_on_cancel, &_form->card);   // (as Cancel)
    _form->finish  = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_POINT_FINISH), fude_zoom_pointf_on_finish, _form);
    _form->add     = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_POINT_ADD), fude_zoom_pointf_on_add, _form);
    fude_zoom_point_restyle(_form);
    fude_kit_modal_show(&_form->card.modal, false);
}

// See the header.
void fude_zoom_point_forget(fude_zoom_point_form* _form) {
    rde_memset(_form, 0, sizeof(*_form));
}

// See the header.
b8 fude_zoom_point_shown(const fude_zoom_point_form* _form) {
    return fude_kit_form_shown(&_form->card);
}

// See the header.
void fude_zoom_point_restyle(fude_zoom_point_form* _form) {
    if(!fude_kit_form_restyle(&_form->card)) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
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

// The card high on the screen (the keyboard comes up under it), laid out for the screen's size now, which is kept in
// laid_out.
RDE_INTERNAL void fude_zoom_pointf_layout(fude_zoom_point_form* _form) {
    const rde_vec_2F _screen = fude_kit_screen_size(_form->card.window);
    const f32 _pad = FUDE_KIT_FORM_PAD, _gap = FUDE_KIT_FORM_GAP, _rh = FUDE_ZOOM_POINTF_ROW_H, _ch = 38.0f;
    const f32 _w  = rde_math_min_f32(560.0f, _screen.x - 32.0f);
    const f32 _iw = _w - 2.0f * _pad;
    const f32 _h  = _pad + 28.0f + _gap + _ch + _gap + _rh + _gap + 20.0f + 22.0f + _gap + 44.0f + _pad;
    f32 _y = fude_kit_form_place(&_form->card, _screen, FUDE_KIT_FORM_HIGH_KEPT, _w, _h);
    const f32 _tw = (_iw - 2.0f * _gap) / 3.0f;
    for(u32 _i = 0; _i < FUDE_ZOOM_POINT_MODES; _i++) {
        fude_kit_place(rde_ui_button_as_node(_form->modes[_i]), (rde_vec_2F){ _pad + (f32)_i * (_tw + _gap) + _tw * 0.5f, _y - _ch * 0.5f }, (rde_vec_2F){ _tw, _ch });
    }
    _y -= _ch + _gap;
    const f32 _col = (_iw - 2.0f * _gap) * 0.5f;
    const f32 _nw  = rde_math_min_f32(84.0f, _col * 0.42f), _fw = _col - _nw - 4.0f;
    for(u32 _i = 0; _i < 2u; _i++) {
        const f32 _x = _pad + (f32)_i * (_col + 2.0f * _gap);
        fude_kit_form_named(_form->names[_i], _form->fields[_i], _x, _y - _rh * 0.5f, _nw, _fw, _rh);
    }
    _y -= _rh + _gap;
    fude_kit_place(rde_ui_label_as_node(_form->hint), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 10.0f }, (rde_vec_2F){ _iw, 20.0f });
    _y -= 20.0f;
    fude_kit_place(rde_ui_label_as_node(_form->problem), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 11.0f }, (rde_vec_2F){ _iw, 22.0f });
    const f32 _bw = rde_math_min_f32(140.0f, (_iw - 2.0f * _gap) / 3.0f);
    fude_kit_place(rde_ui_button_as_node(_form->close), (rde_vec_2F){ _pad + _bw * 0.5f, _pad + 22.0f }, (rde_vec_2F){ _bw, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_form->finish), (rde_vec_2F){ _w - _pad - _bw - _gap - _bw * 0.5f, _pad + 22.0f }, (rde_vec_2F){ _bw, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_form->add), (rde_vec_2F){ _w - _pad - _bw * 0.5f, _pad + 22.0f }, (rde_vec_2F){ _bw, 44.0f });
}

// See the header.
void fude_zoom_point_open(fude_zoom_point_form* _form, u8 _mode, b8 _has_last, b8 _on_sheet, const fude_zoom_units_style* _units,
                          fude_zoom_point_done _done, any _self) {
    if(_form->card.modal.card == NULL) {
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
    fude_kit_form_show(&_form->card);
    fude_zoom_pointf_layout(_form);
    fude_zoom_point_restyle(_form);
    fude_zoom_pointf_show(_form);
    rde_ui_node_focus(rde_ui_text_editor_as_node(_form->fields[0]));
}

// See the header.
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

// See the header.
void fude_zoom_point_close(fude_zoom_point_form* _form) {
    fude_kit_form_close(&_form->card);
}

// See the header.
void fude_zoom_point_update(fude_zoom_point_form* _form) {
    if(fude_kit_form_resized(&_form->card)) {
        fude_zoom_pointf_layout(_form);
    }
}

// See the header.
void fude_zoom_point_look(fude_zoom_point_form* _form, u8 _mode, const c8* _a, const c8* _b, u8 _press) {
    if(!fude_zoom_point_shown(_form)) {
        return;
    }
    if(_mode < FUDE_ZOOM_POINT_MODES && (_mode == FUDE_ZOOM_POINT_AT || _form->has_last)) {
        _form->mode = _mode;
        fude_zoom_pointf_show(_form);
    }
    if(_a != NULL) {
        fude_kit_form_set(&_form->card, _form->fields[0], _a);
    }
    if(_b != NULL) {
        fude_kit_form_set(&_form->card, _form->fields[1], _b);
    }
    if(_press == 1u) {
        fude_zoom_pointf_go(_form, false);
    } else if(_press == 2u) {
        fude_zoom_pointf_go(_form, true);
    }
}
