// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/repeatform.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/widgets/draw.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FUDE_ZOOM_REPEATF_PAD    16.0f
#define FUDE_ZOOM_REPEATF_GAP    8.0f
#define FUDE_ZOOM_REPEATF_ROW_H  44.0f
#define FUDE_ZOOM_REPEATF_CHIP_H 38.0f
#define FUDE_ZOOM_REPEATF_FIELD_PX 18u

enum {
    FUDE_ZOOM_REPEATF_ROW_COPIES = 0, FUDE_ZOOM_REPEATF_ROW_X, FUDE_ZOOM_REPEATF_ROW_Y,
    FUDE_ZOOM_REPEATF_ROUND_COPIES, FUDE_ZOOM_REPEATF_ROUND_ANGLE, FUDE_ZOOM_REPEATF_ROUND_X, FUDE_ZOOM_REPEATF_ROUND_Y,
    FUDE_ZOOM_REPEATF_MIRROR_AT,
};
#define FUDE_ZOOM_REPEATF_AXES FUDE_ZOOM_REPEAT_MODES   // (the axes' refs after the modes')

// Which mode each field is of, and its place (row, column) in it.
static const u8 FUDE_ZOOM_REPEATF_MODE_OF[FUDE_ZOOM_REPEAT_FIELDS] = { 0, 0, 0, 1, 1, 1, 1, 2 };
static const u8 FUDE_ZOOM_REPEATF_ROW_OF[FUDE_ZOOM_REPEAT_FIELDS]  = { 0, 1, 1, 0, 0, 1, 1, 1 };
static const u8 FUDE_ZOOM_REPEATF_COL_OF[FUDE_ZOOM_REPEAT_FIELDS]  = { 0, 0, 1, 0, 1, 0, 1, 1 };

RDE_INTERNAL void fude_zoom_repeatf_text(rde_ui_text_editor* _field, c8* _out, usize _size) {
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

RDE_INTERNAL void fude_zoom_repeatf_set_text(fude_zoom_repeat_form* _form, rde_ui_text_editor* _field, const c8* _text) {
    _form->setting = true;
    const usize _bytes = rde_ui_text_editor_get_byte_count(_field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_field, 0, _bytes);
    }
    if(_text[0] != 0) {
        rde_ui_text_editor_insert_at(_field, 0, _text, strlen(_text));
    }
    _form->setting = false;
}

RDE_INTERNAL void fude_zoom_repeatf_set_length(fude_zoom_repeat_form* _form, u32 _i, f64 _mm) {
    c8 _t[FUDE_ZOOM_UNITS_TEXT];
    fude_zoom_units_format(_mm, &_form->units, _t, sizeof(_t));
    fude_zoom_repeatf_set_text(_form, _form->fields[_i], _t);
}

RDE_INTERNAL void fude_zoom_repeatf_layout(fude_zoom_repeat_form* _form);

// The mirror's line where it goes to begin with (by its edge when kept, else through its middle), while not typed.
RDE_INTERNAL void fude_zoom_repeatf_line(fude_zoom_repeat_form* _form) {
    if(!_form->at_typed) {
        const u32 _a = _form->was.axis != 0u ? 1u : 0u;
        fude_zoom_repeatf_set_length(_form, FUDE_ZOOM_REPEATF_MIRROR_AT, _form->was.keep ? _form->was.edge_mm[_a] : _form->was.mid_mm[_a]);
    }
}

RDE_INTERNAL void fude_zoom_repeatf_show(fude_zoom_repeat_form* _form) {
    for(u32 _i = 0; _i < FUDE_ZOOM_REPEAT_MODES; _i++) {
        fude_kit_button_chip(_form->modes[_i], _form->was.mode == _i);
    }
    fude_kit_button_chip(_form->turn, _form->was.turn);
    fude_kit_button_chip(_form->axes[0], _form->was.axis == 0u);
    fude_kit_button_chip(_form->axes[1], _form->was.axis == 1u);
    fude_kit_button_chip(_form->keep, _form->was.keep);
    static const FUDE_TEXT_ _hints[FUDE_ZOOM_REPEAT_MODES] = { FUDE_TEXT_ZOOM_REPEAT_ROW_HINT, FUDE_TEXT_ZOOM_REPEAT_ROUND_HINT, FUDE_TEXT_ZOOM_REPEAT_MIRROR_HINT };
    static const FUDE_TEXT_ _free[FUDE_ZOOM_REPEAT_MODES]  = { FUDE_TEXT_ZOOM_REPEAT_ROW_HINT, FUDE_TEXT_ZOOM_REPEAT_ROUND_FREE, FUDE_TEXT_ZOOM_REPEAT_MIRROR_FREE };
    const FUDE_TEXT_ _hint = (_form->was.has_xy ? _hints : _free)[_form->was.mode % FUDE_ZOOM_REPEAT_MODES];
    rde_ui_label_set_text(_form->hint, fude_text(_hint));
}

// --- presses -------------------------------------------------------------------------------

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_repeatf_on_mode(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_repeat_ref* _ref = (const fude_zoom_repeat_ref*)_user_data;
    _ref->form->was.mode = (u8)_ref->index;
    rde_ui_label_set_text(_ref->form->problem, "");
    fude_zoom_repeatf_layout(_ref->form);
    fude_zoom_repeatf_show(_ref->form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_repeatf_on_axis(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_repeat_ref* _ref = (const fude_zoom_repeat_ref*)_user_data;
    _ref->form->was.axis = (u8)(_ref->index - FUDE_ZOOM_REPEATF_AXES);
    fude_zoom_repeatf_line(_ref->form);
    fude_zoom_repeatf_show(_ref->form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_repeatf_on_turn(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_repeat_form* _form = (fude_zoom_repeat_form*)_user_data;
    _form->was.turn = !_form->was.turn;
    fude_zoom_repeatf_show(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_repeatf_on_keep(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_repeat_form* _form = (fude_zoom_repeat_form*)_user_data;
    _form->was.keep = !_form->was.keep;
    fude_zoom_repeatf_line(_form);
    fude_zoom_repeatf_show(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_repeatf_on_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_repeat_close((fude_zoom_repeat_form*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_repeatf_on_dismiss(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_repeat_form* _form = (fude_zoom_repeat_form*)_user_data;
    if(rde_engine_get_time_now() - _form->opened_at >= 0.6) {
        fude_zoom_repeat_close(_form);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Field _i read: a count (a whole one, 1 at least: 2 round), an angle, or a length. False: it cannot be.
RDE_INTERNAL b8 fude_zoom_repeatf_read(const fude_zoom_repeat_form* _form, u32 _i, f64* _out) {
    c8 _t[96];
    fude_zoom_repeatf_text(_form->fields[_i], _t, sizeof(_t));
    fude_zoom_units_error _e;
    if(_i == FUDE_ZOOM_REPEATF_ROW_COPIES || _i == FUDE_ZOOM_REPEATF_ROUND_COPIES) {
        const f64 _least = _i == FUDE_ZOOM_REPEATF_ROW_COPIES ? 1.0 : 2.0;
        return fude_zoom_units_number(_t, NULL, _out, &_e) && fabs(*_out - round(*_out)) < 1e-9 && *_out >= _least && *_out <= 500.0;
    }
    if(_i == FUDE_ZOOM_REPEATF_ROUND_ANGLE) {
        return fude_zoom_units_angle(_t, NULL, _out, &_e) && fabs(*_out) > 1e-9 && fabs(*_out) <= 360.0 + 1e-9;
    }
    return fude_zoom_units_length(_t, _form->units.unit, NULL, _out, &_e) && isfinite(*_out);
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_repeatf_on_save(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_repeat_form*  _form  = (fude_zoom_repeat_form*)_user_data;
    fude_zoom_repeat_values _typed = _form->was;
    f64 _v[FUDE_ZOOM_REPEAT_FIELDS];
    for(u32 _i = 0; _i < FUDE_ZOOM_REPEAT_FIELDS; _i++) {
        if(FUDE_ZOOM_REPEATF_MODE_OF[_i] != _typed.mode) {
            continue;
        }
        if(!fude_zoom_repeatf_read(_form, _i, &_v[_i])) {
            c8 _t[96], _say[200];
            fude_zoom_repeatf_text(_form->fields[_i], _t, sizeof(_t));
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_SIZE_BAD, FUDE_TS(_t[0] != 0 ? _t : "?"));
            rde_ui_label_set_text(_form->problem, _say);
            rde_ui_node_focus(rde_ui_text_editor_as_node(_form->fields[_i]));
            return RDE_UI_EVENT_RESULT_CONSUME;
        }
    }
    if(_typed.mode == FUDE_ZOOM_REPEAT_ROW) {
        _typed.copies = (u32)llround(_v[FUDE_ZOOM_REPEATF_ROW_COPIES]);
        _typed.dx_mm  = _v[FUDE_ZOOM_REPEATF_ROW_X];
        _typed.dy_mm  = _v[FUDE_ZOOM_REPEATF_ROW_Y];
    } else if(_typed.mode == FUDE_ZOOM_REPEAT_ROUND) {
        _typed.copies = (u32)llround(_v[FUDE_ZOOM_REPEATF_ROUND_COPIES]);
        _typed.angle  = _v[FUDE_ZOOM_REPEATF_ROUND_ANGLE];
        _typed.cx_mm  = _v[FUDE_ZOOM_REPEATF_ROUND_X];
        _typed.cy_mm  = _v[FUDE_ZOOM_REPEATF_ROUND_Y];
    } else {
        _typed.at_mm = _v[FUDE_ZOOM_REPEATF_MIRROR_AT];
    }
    const fude_zoom_repeat_done _done = _form->done;
    void* const                 _self = _form->self;
    fude_zoom_repeat_close(_form);
    if(_done != NULL) {
        _done(_self, &_typed);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// The mirror's line typed: it stays where typed (an axis or keep chosen after leaves it).
RDE_INTERNAL void fude_zoom_repeatf_on_change(rde_ui_node* _node, any _user_data) {
    fude_zoom_repeat_form* _form = (fude_zoom_repeat_form*)_user_data;
    if(_form == NULL || _form->setting) {
        return;
    }
    if(_node == rde_ui_text_editor_as_node(_form->fields[FUDE_ZOOM_REPEATF_MIRROR_AT])) {
        _form->at_typed = true;
    }
    rde_ui_label_set_text(_form->problem, "");
}

// Enter: on to the next field of the mode; in its last, Apply.
RDE_INTERNAL void fude_zoom_repeatf_on_submit(rde_ui_node* _node, any _user_data) {
    fude_zoom_repeat_form* _form = (fude_zoom_repeat_form*)_user_data;
    if(_form == NULL) {
        return;
    }
    u32 _at = FUDE_ZOOM_REPEAT_FIELDS;
    for(u32 _i = 0; _i < FUDE_ZOOM_REPEAT_FIELDS; _i++) {
        if(_node == rde_ui_text_editor_as_node(_form->fields[_i])) {
            _at = _i;
        }
    }
    for(u32 _i = _at + 1u; _i < FUDE_ZOOM_REPEAT_FIELDS; _i++) {
        if(FUDE_ZOOM_REPEATF_MODE_OF[_i] == _form->was.mode) {
            rde_ui_node_focus(rde_ui_text_editor_as_node(_form->fields[_i]));
            return;
        }
    }
    fude_zoom_repeatf_on_save(NULL, NULL, _form);
}

// --- built, laid out --------------------------------------------------------------------------

RDE_INTERNAL rde_ui_label* fude_zoom_repeatf_label(fude_zoom_repeat_form* _form, rde_ui_node* _parent, const c8* _text, f32 _px) {
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

void fude_zoom_repeat_build(fude_zoom_repeat_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font) {
    memset(_form, 0, sizeof(*_form));
    _form->window = _window;
    _form->font   = _font;
    fude_kit_modal_create(&_form->modal, _root, fude_zoom_repeatf_on_dismiss, _form);
    rde_ui_node* _card = rde_ui_image_as_node(_form->modal.card);
    _form->title = fude_zoom_repeatf_label(_form, _card, fude_text(FUDE_TEXT_ZOOM_REPEAT), 17.0f);
    static const FUDE_TEXT_ _mode_names[FUDE_ZOOM_REPEAT_MODES] = { FUDE_TEXT_ZOOM_REPEAT_ROW, FUDE_TEXT_ZOOM_REPEAT_ROUND, FUDE_TEXT_ZOOM_REPEAT_MIRROR };
    for(u32 _i = 0; _i < FUDE_ZOOM_REPEAT_MODES; _i++) {
        _form->refs[_i] = (fude_zoom_repeat_ref){ _form, _i };
        _form->modes[_i] = fude_kit_button(_card, fude_text(_mode_names[_i]), fude_zoom_repeatf_on_mode, &_form->refs[_i]);
    }
    static const FUDE_TEXT_ _names[FUDE_ZOOM_REPEAT_FIELDS] = {
        FUDE_TEXT_ZOOM_REPEAT_COPIES, FUDE_TEXT_ZOOM_REPEAT_ACROSS, FUDE_TEXT_ZOOM_REPEAT_UP,
        FUDE_TEXT_ZOOM_REPEAT_COPIES, FUDE_TEXT_ZOOM_SIZE_ANGLE, FUDE_TEXT_ZOOM_REPEAT_CENTRE_X, FUDE_TEXT_ZOOM_REPEAT_CENTRE_Y,
        FUDE_TEXT_ZOOM_REPEAT_LINE_AT,
    };
    for(u32 _i = 0; _i < FUDE_ZOOM_REPEAT_FIELDS; _i++) {
        _form->names[_i] = fude_zoom_repeatf_label(_form, _card, fude_text(_names[_i]), 15.0f);
        rde_ui_text_editor* _f = rde_ui_text_editor_create(_font, NULL);
        rde_ui_text_editor_set_multiline(_f, false);
        rde_ui_text_editor_set_allow_multicursor(_f, false);
        rde_ui_text_editor_set_font_size(_f, FUDE_ZOOM_REPEATF_FIELD_PX);
        rde_ui_text_editor_set_max_chars(_f, 32u);
        rde_ui_text_editor_set_content_insets(_f, 10.0f, 8.0f, 10.0f, 8.0f);
        rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_f), (any)_form);
        rde_ui_text_editor_set_on_submit(_f, fude_zoom_repeatf_on_submit);
        rde_ui_text_editor_set_on_change(_f, fude_zoom_repeatf_on_change);
        fude_kit_field_box(_card, _f);
        _form->fields[_i] = _f;
    }
    _form->turn = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_REPEAT_TURN), fude_zoom_repeatf_on_turn, _form);
    for(u32 _i = 0; _i < 2u; _i++) {
        _form->refs[FUDE_ZOOM_REPEATF_AXES + _i] = (fude_zoom_repeat_ref){ _form, FUDE_ZOOM_REPEATF_AXES + _i };
        _form->axes[_i] = fude_kit_button(_card, fude_text(_i == 0u ? FUDE_TEXT_ZOOM_REPEAT_ACROSS_LR : FUDE_TEXT_ZOOM_REPEAT_ACROSS_TB), fude_zoom_repeatf_on_axis,
                                          &_form->refs[FUDE_ZOOM_REPEATF_AXES + _i]);
    }
    _form->keep    = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_REPEAT_KEEP), fude_zoom_repeatf_on_keep, _form);
    _form->hint    = fude_zoom_repeatf_label(_form, _card, "", 13.0f);
    _form->problem = fude_zoom_repeatf_label(_form, _card, "", 14.0f);
    _form->cancel  = fude_kit_button(_card, fude_text(FUDE_TEXT_CANCEL), fude_zoom_repeatf_on_cancel, _form);
    _form->save    = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_SIZE_APPLY), fude_zoom_repeatf_on_save, _form);
    fude_zoom_repeat_restyle(_form);
    fude_kit_modal_show(&_form->modal, false);
}

void fude_zoom_repeat_forget(fude_zoom_repeat_form* _form) {
    memset(_form, 0, sizeof(*_form));
}

b8 fude_zoom_repeat_shown(const fude_zoom_repeat_form* _form) {
    return _form->modal.card != NULL && _form->modal.shown;
}

void fude_zoom_repeat_restyle(fude_zoom_repeat_form* _form) {
    if(_form->modal.card == NULL) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    fude_kit_modal_restyle(&_form->modal, 16.0f);
    rde_ui_label_set_color(_form->title, _t->text);
    for(u32 _i = 0; _i < FUDE_ZOOM_REPEAT_FIELDS; _i++) {
        rde_ui_label_set_color(_form->names[_i], _t->text_soft);
        fude_kit_style_field(_form->fields[_i]);
    }
    rde_ui_label_set_color(_form->hint, _t->text_soft);
    rde_ui_label_set_color(_form->problem, _t->score_poor);
    fude_kit_button_plain(_form->cancel);
    fude_kit_button_primary(_form->save);
    fude_zoom_repeatf_show(_form);
}

// The card high on the screen: its tabs, then the mode's rows (two fields a row, a name before each), its hint.
RDE_INTERNAL void fude_zoom_repeatf_layout(fude_zoom_repeat_form* _form) {
    const rde_vec_2F _screen = fude_kit_screen_size(_form->window);
    const f32 _pad = FUDE_ZOOM_REPEATF_PAD, _gap = FUDE_ZOOM_REPEATF_GAP, _rh = FUDE_ZOOM_REPEATF_ROW_H, _ch = FUDE_ZOOM_REPEATF_CHIP_H;
    const f32 _w  = fminf(560.0f, _screen.x - 32.0f);
    const f32 _iw = _w - 2.0f * _pad;
    const u8  _mode = _form->was.mode;
    const u32 _rows = _mode == FUDE_ZOOM_REPEAT_ROW ? 2u : _mode == FUDE_ZOOM_REPEAT_ROUND ? 3u : 2u;
    const f32 _h  = _pad + 28.0f + _gap + _ch + _gap + (f32)_rows * (_rh + _gap) + 20.0f + 22.0f + _gap + 44.0f + _pad;
    _form->laid_out = _screen;
    fude_kit_modal_place(&_form->modal, _form->window, (rde_vec_2F){ _screen.x * 0.5f, fmaxf(_screen.y - 90.0f - _h * 0.5f, _h * 0.5f + 8.0f) }, (rde_vec_2F){ _w, _h });
    f32 _y = _h - _pad;
    fude_kit_place(rde_ui_label_as_node(_form->title), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 14.0f }, (rde_vec_2F){ _iw, 28.0f });
    _y -= 28.0f + _gap;
    const f32 _tw = (_iw - 2.0f * _gap) / 3.0f;
    for(u32 _i = 0; _i < FUDE_ZOOM_REPEAT_MODES; _i++) {
        fude_kit_place(rde_ui_button_as_node(_form->modes[_i]), (rde_vec_2F){ _pad + (f32)_i * (_tw + _gap) + _tw * 0.5f, _y - _ch * 0.5f }, (rde_vec_2F){ _tw, _ch });
    }
    _y -= _ch + _gap;
    const f32 _col = (_iw - 2.0f * _gap) * 0.5f;
    const f32 _nw  = fminf(96.0f, _col * 0.45f), _fw = _col - _nw - 4.0f;
    for(u32 _i = 0; _i < FUDE_ZOOM_REPEAT_FIELDS; _i++) {
        const b8 _on = FUDE_ZOOM_REPEATF_MODE_OF[_i] == _mode;
        rde_ui_node_set_active(rde_ui_label_as_node(_form->names[_i]), _on);
        rde_ui_node_set_active(fude_kit_field_node(_form->fields[_i]), _on);
        if(!_on) {
            continue;
        }
        const f32 _x  = _pad + (f32)FUDE_ZOOM_REPEATF_COL_OF[_i] * (_col + 2.0f * _gap);
        const f32 _ry = _y - (f32)FUDE_ZOOM_REPEATF_ROW_OF[_i] * (_rh + _gap) - _rh * 0.5f;
        fude_kit_place(rde_ui_label_as_node(_form->names[_i]), (rde_vec_2F){ _x + _nw * 0.5f, _ry }, (rde_vec_2F){ _nw, _rh });
        fude_kit_place(fude_kit_field_node(_form->fields[_i]), (rde_vec_2F){ _x + _nw + 4.0f + _fw * 0.5f, _ry }, (rde_vec_2F){ _fw, _rh });
    }
    const b8 _round = _mode == FUDE_ZOOM_REPEAT_ROUND, _mirror = _mode == FUDE_ZOOM_REPEAT_MIRROR;
    rde_ui_node_set_active(rde_ui_button_as_node(_form->turn), _round);
    if(_round) {
        fude_kit_place(rde_ui_button_as_node(_form->turn), (rde_vec_2F){ _pad + _col * 0.5f, _y - 2.0f * (_rh + _gap) - _rh * 0.5f }, (rde_vec_2F){ _col, _ch });
    }
    for(u32 _i = 0; _i < 2u; _i++) {
        rde_ui_node_set_active(rde_ui_button_as_node(_form->axes[_i]), _mirror);
        if(_mirror) {
            fude_kit_place(rde_ui_button_as_node(_form->axes[_i]), (rde_vec_2F){ _pad + (f32)_i * (_col + 2.0f * _gap) + _col * 0.5f, _y - _rh * 0.5f }, (rde_vec_2F){ _col, _ch });
        }
    }
    rde_ui_node_set_active(rde_ui_button_as_node(_form->keep), _mirror);
    if(_mirror) {
        fude_kit_place(rde_ui_button_as_node(_form->keep), (rde_vec_2F){ _pad + _col * 0.5f, _y - (_rh + _gap) - _rh * 0.5f }, (rde_vec_2F){ _col, _ch });
    }
    _y -= (f32)_rows * (_rh + _gap);
    fude_kit_place(rde_ui_label_as_node(_form->hint), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 10.0f }, (rde_vec_2F){ _iw, 20.0f });
    _y -= 20.0f;
    fude_kit_place(rde_ui_label_as_node(_form->problem), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 11.0f }, (rde_vec_2F){ _iw, 22.0f });
    fude_kit_place(rde_ui_button_as_node(_form->cancel), (rde_vec_2F){ _w - _pad - 120.0f - _gap - 60.0f, _pad + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_form->save), (rde_vec_2F){ _w - _pad - 60.0f, _pad + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
}

void fude_zoom_repeat_open(fude_zoom_repeat_form* _form, const fude_zoom_repeat_values* _now, const fude_zoom_units_style* _units,
                           fude_zoom_repeat_done _done, void* _self) {
    if(_form->modal.card == NULL) {
        return;
    }
    _form->was   = *_now;
    _form->units = *_units;
    _form->done  = _done;
    _form->self  = _self;
    c8 _n[32];
    snprintf(_n, sizeof(_n), "%u", _now->copies > 0u ? _now->copies : 3u);
    fude_zoom_repeatf_set_text(_form, _form->fields[FUDE_ZOOM_REPEATF_ROW_COPIES], _n);
    fude_zoom_repeatf_set_text(_form, _form->fields[FUDE_ZOOM_REPEATF_ROUND_COPIES], "6");
    _form->at_typed = false;
    fude_zoom_repeatf_line(_form);
    fude_zoom_repeatf_set_length(_form, FUDE_ZOOM_REPEATF_ROW_X, _now->dx_mm);
    fude_zoom_repeatf_set_length(_form, FUDE_ZOOM_REPEATF_ROW_Y, _now->dy_mm);
    fude_zoom_repeatf_set_length(_form, FUDE_ZOOM_REPEATF_ROUND_X, _now->cx_mm);
    fude_zoom_repeatf_set_length(_form, FUDE_ZOOM_REPEATF_ROUND_Y, _now->cy_mm);
    c8 _deg[FUDE_ZOOM_UNITS_TEXT];
    fude_zoom_units_format_angle(fabs(_now->angle) > 1e-9 ? _now->angle : 360.0, 2u, _deg, sizeof(_deg));
    fude_zoom_repeatf_set_text(_form, _form->fields[FUDE_ZOOM_REPEATF_ROUND_ANGLE], _deg);
    rde_ui_label_set_text(_form->problem, "");
    fude_kit_modal_show(&_form->modal, true);
    _form->opened_at = rde_engine_get_time_now();
    fude_zoom_repeatf_layout(_form);
    fude_zoom_repeat_restyle(_form);
}

void fude_zoom_repeat_close(fude_zoom_repeat_form* _form) {
    if(_form->modal.card != NULL) {
        fude_kit_modal_show(&_form->modal, false);
    }
}

void fude_zoom_repeat_update(fude_zoom_repeat_form* _form) {
    if(!fude_zoom_repeat_shown(_form)) {
        return;
    }
    const rde_vec_2F _screen = fude_kit_screen_size(_form->window);
    if(_screen.x != _form->laid_out.x || _screen.y != _form->laid_out.y) {
        fude_zoom_repeatf_layout(_form);
    }
}

void fude_zoom_repeat_look(fude_zoom_repeat_form* _form, u8 _mode, const c8* const* _typed, i32 _axis, i32 _keep, b8 _apply) {
    fude_zoom_repeatf_on_mode(NULL, NULL, &_form->refs[_mode % FUDE_ZOOM_REPEAT_MODES]);
    for(u32 _i = 0; _typed != NULL && _i < FUDE_ZOOM_REPEAT_FIELDS; _i++) {
        if(_typed[_i] != NULL) {
            fude_zoom_repeatf_set_text(_form, _form->fields[_i], _typed[_i]);
            _form->at_typed = _form->at_typed || _i == FUDE_ZOOM_REPEATF_MIRROR_AT;
        }
    }
    if(_axis >= 0) {
        _form->was.axis = (u8)(_axis != 0 ? 1u : 0u);
    }
    if(_keep >= 0) {
        _form->was.keep = _keep != 0;
    }
    fude_zoom_repeatf_line(_form);
    fude_zoom_repeatf_show(_form);
    if(_apply) {
        fude_zoom_repeatf_on_save(NULL, NULL, _form);
    }
}
