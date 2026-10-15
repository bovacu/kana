// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/repeatform.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/base/draw.h"
#include "drawing/widgets/kit.h"

// ===========================================================================
// See repeatform.h.
//
// The card is the kit's form (kit.h: its title, Cancel, a tap off it, laying it out again, its fields and what could
// not be read). Its own helpers and constants are fude_zoom_repeatf_ and FUDE_ZOOM_REPEATF_ (the "f" for the form),
// apart from the public fude_zoom_repeat_. Every mode's fields are made once, at build, and only the chosen mode's are
// shown and laid out; three small tables say which mode each field is of and where it goes. The values being edited are
// kept in `was` (the chips change it at once), and the fields' texts are read only when Apply is pressed.
// ===========================================================================

// The card's measures, in the UI's points: a row of fields' height and a chip's (its margin and gaps are the kit's
// form's).
#define FUDE_ZOOM_REPEATF_ROW_H  44.0f
#define FUDE_ZOOM_REPEATF_CHIP_H 38.0f

// The fields, in the order of the card's arrays (names, fields): the row's three, the round's four, the mirror's one.
enum {
    FUDE_ZOOM_REPEATF_ROW_COPIES = 0, FUDE_ZOOM_REPEATF_ROW_X, FUDE_ZOOM_REPEATF_ROW_Y,
    FUDE_ZOOM_REPEATF_ROUND_COPIES, FUDE_ZOOM_REPEATF_ROUND_ANGLE, FUDE_ZOOM_REPEATF_ROUND_X, FUDE_ZOOM_REPEATF_ROUND_Y,
    FUDE_ZOOM_REPEATF_MIRROR_AT,
};
// The index in refs of the first axis chip's ref: the axes' refs come after the modes'.
#define FUDE_ZOOM_REPEATF_AXES FUDE_ZOOM_REPEAT_MODES

// Which mode each field is of, and its place (row, column) in it.
static const u8 FUDE_ZOOM_REPEATF_MODE_OF[FUDE_ZOOM_REPEAT_FIELDS] = { 0, 0, 0, 1, 1, 1, 1, 2 };
static const u8 FUDE_ZOOM_REPEATF_ROW_OF[FUDE_ZOOM_REPEAT_FIELDS]  = { 0, 1, 1, 0, 0, 1, 1, 1 };
static const u8 FUDE_ZOOM_REPEATF_COL_OF[FUDE_ZOOM_REPEAT_FIELDS]  = { 0, 0, 1, 0, 1, 0, 1, 1 };

// Writes length _mm into field _i in the canvas's units, as the card's own writing (kit.h: fude_kit_form_set), so the
// field's change callback knows the card wrote it, not the hand.
RDE_INTERNAL void fude_zoom_repeatf_set_length(fude_zoom_repeat_form* _form, u32 _i, f64 _mm) {
    c8 _t[FUDE_ZOOM_UNITS_TEXT];
    fude_zoom_units_format(_mm, &_form->units, _t, sizeof(_t));
    fude_kit_form_set(&_form->card, _form->fields[_i], _t);
}

// Used by the presses; it is defined with the building, below.
RDE_INTERNAL void fude_zoom_repeatf_layout(fude_zoom_repeat_form* _form);

// The mirror's line where it goes to begin with (by its edge when kept, else through its middle), while not typed.
RDE_INTERNAL void fude_zoom_repeatf_line(fude_zoom_repeat_form* _form) {
    if(!_form->at_typed) {
        const u32 _a = _form->was.axis != 0u ? 1u : 0u;
        fude_zoom_repeatf_set_length(_form, FUDE_ZOOM_REPEATF_MIRROR_AT, _form->was.keep ? _form->was.edge_mm[_a] : _form->was.mid_mm[_a]);
    }
}

// Shows `was` on the chips (the mode's tab, Turn, the axis, Keep) and sets the hint for the mode: on a sheet, the
// hint says positions are from its corner; elsewhere the round and mirror hints say they are from the middle.
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

// A mode's tab: chooses that mode, clears what was wrong, and lays the card out for its fields.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_repeatf_on_mode(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_repeat_ref* _ref = (const fude_zoom_repeat_ref*)_user_data;
    _ref->form->was.mode = (u8)_ref->index;
    rde_ui_label_set_text(_ref->form->problem, "");
    fude_zoom_repeatf_layout(_ref->form);
    fude_zoom_repeatf_show(_ref->form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// An axis chip: chooses left ↔ right or top ↔ bottom, and moves the mirror's line to suit unless it was typed.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_repeatf_on_axis(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_repeat_ref* _ref = (const fude_zoom_repeat_ref*)_user_data;
    _ref->form->was.axis = (u8)(_ref->index - FUDE_ZOOM_REPEATF_AXES);
    fude_zoom_repeatf_line(_ref->form);
    fude_zoom_repeatf_show(_ref->form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Turn them: toggles whether each copy round a centre is turned as it goes.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_repeatf_on_turn(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_repeat_form* _form = (fude_zoom_repeat_form*)_user_data;
    _form->was.turn = !_form->was.turn;
    fude_zoom_repeatf_show(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Keep the original: toggles it, and moves the mirror's line to suit unless it was typed.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_repeatf_on_keep(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_repeat_form* _form = (fude_zoom_repeat_form*)_user_data;
    _form->was.keep = !_form->was.keep;
    fude_zoom_repeatf_line(_form);
    fude_zoom_repeatf_show(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Reads field _i into *_out: a count (a whole number from 1 in a row, or from 2 round, up to 500), an angle (not 0,
// at most 360° either way) or a length in millimetres. Returns false when it cannot be read or is out of range.
RDE_INTERNAL b8 fude_zoom_repeatf_read(const fude_zoom_repeat_form* _form, u32 _i, f64* _out) {
    c8 _t[96];
    fude_kit_field_text(_form->fields[_i], _t, sizeof(_t));
    fude_zoom_units_error _e;
    if(_i == FUDE_ZOOM_REPEATF_ROW_COPIES || _i == FUDE_ZOOM_REPEATF_ROUND_COPIES) {
        const f64 _least = _i == FUDE_ZOOM_REPEATF_ROW_COPIES ? 1.0 : 2.0;
        return fude_zoom_units_number(_t, NULL, _out, &_e) && rde_math_abs_f64(*_out - rde_math_round_f64(*_out)) < 1e-9 && *_out >= _least && *_out <= 500.0;
    }
    if(_i == FUDE_ZOOM_REPEATF_ROUND_ANGLE) {
        return fude_zoom_units_angle(_t, NULL, _out, &_e) && rde_math_abs_f64(*_out) > 1e-9 && rde_math_abs_f64(*_out) <= 360.0 + 1e-9;
    }
    return fude_zoom_units_length(_t, _form->units.unit, NULL, _out, &_e) && rde_math_is_finite_f64(*_out);
}

// Apply: reads the chosen mode's fields; the first that cannot be read is named on the card and given the focus, and
// the card stays. Otherwise the values go into a copy of `was`, the card is put away, and then the opener's callback
// is called.
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
            fude_kit_form_bad(_form->problem, _form->fields[_i], FUDE_TEXT_ZOOM_SIZE_BAD);
            return RDE_UI_EVENT_RESULT_CONSUME;
        }
    }
    if(_typed.mode == FUDE_ZOOM_REPEAT_ROW) {
        _typed.copies = (u32)rde_math_round_to_i64_f64(_v[FUDE_ZOOM_REPEATF_ROW_COPIES]);
        _typed.dx_mm  = _v[FUDE_ZOOM_REPEATF_ROW_X];
        _typed.dy_mm  = _v[FUDE_ZOOM_REPEATF_ROW_Y];
    } else if(_typed.mode == FUDE_ZOOM_REPEAT_ROUND) {
        _typed.copies = (u32)rde_math_round_to_i64_f64(_v[FUDE_ZOOM_REPEATF_ROUND_COPIES]);
        _typed.angle  = _v[FUDE_ZOOM_REPEATF_ROUND_ANGLE];
        _typed.cx_mm  = _v[FUDE_ZOOM_REPEATF_ROUND_X];
        _typed.cy_mm  = _v[FUDE_ZOOM_REPEATF_ROUND_Y];
    } else {
        _typed.at_mm = _v[FUDE_ZOOM_REPEATF_MIRROR_AT];
    }
    const fude_zoom_repeat_done _done = _form->done;
    any const                 _self = _form->self;
    fude_zoom_repeat_close(_form);
    if(_done != NULL) {
        _done(_self, &_typed);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A field changed: when it was typed into (not written by the card), what was wrong is cleared, and when it is the
// mirror's line, the line stays where typed (an axis or Keep chosen after leaves it).
RDE_INTERNAL void fude_zoom_repeatf_on_change(rde_ui_node* _node, any _user_data) {
    fude_zoom_repeat_form* _form = (fude_zoom_repeat_form*)_user_data;
    if(_form == NULL || _form->card.setting) {
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

// See repeatform.h. Makes the title, the mode tabs, every mode's names and fields (one line each, at most 32
// characters), Turn, the axes, Keep, the hint, the line of what is wrong, Cancel and Apply, all on the modal card.
void fude_zoom_repeat_build(fude_zoom_repeat_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font) {
    rde_memset(_form, 0, sizeof(*_form));
    rde_ui_node* _card = fude_kit_form_build(&_form->card, _root, _window, _font, fude_text(FUDE_TEXT_ZOOM_REPEAT));
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
        _form->names[_i]  = fude_kit_form_label(&_form->card, _card, fude_text(_names[_i]), 15.0f);
        _form->fields[_i] = fude_kit_form_field(&_form->card, _card, 32u, NULL, (any)_form, fude_zoom_repeatf_on_change, fude_zoom_repeatf_on_submit);
    }
    _form->turn = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_REPEAT_TURN), fude_zoom_repeatf_on_turn, _form);
    for(u32 _i = 0; _i < 2u; _i++) {
        _form->refs[FUDE_ZOOM_REPEATF_AXES + _i] = (fude_zoom_repeat_ref){ _form, FUDE_ZOOM_REPEATF_AXES + _i };
        _form->axes[_i] = fude_kit_button(_card, fude_text(_i == 0u ? FUDE_TEXT_ZOOM_REPEAT_ACROSS_LR : FUDE_TEXT_ZOOM_REPEAT_ACROSS_TB), fude_zoom_repeatf_on_axis,
                                          &_form->refs[FUDE_ZOOM_REPEATF_AXES + _i]);
    }
    _form->keep    = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_REPEAT_KEEP), fude_zoom_repeatf_on_keep, _form);
    _form->hint    = fude_kit_form_label(&_form->card, _card, "", 13.0f);
    _form->problem = fude_kit_form_label(&_form->card, _card, "", 14.0f);
    _form->cancel  = fude_kit_button(_card, fude_text(FUDE_TEXT_CANCEL), fude_kit_form_on_cancel, &_form->card);
    _form->save    = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_SIZE_APPLY), fude_zoom_repeatf_on_save, _form);
    fude_zoom_repeat_restyle(_form);
    fude_kit_modal_show(&_form->card.modal, false);
}

// See repeatform.h.
void fude_zoom_repeat_forget(fude_zoom_repeat_form* _form) {
    rde_memset(_form, 0, sizeof(*_form));
}

// See repeatform.h.
b8 fude_zoom_repeat_shown(const fude_zoom_repeat_form* _form) {
    return fude_kit_form_shown(&_form->card);
}

// See repeatform.h. What is wrong is shown in the theme's poor-score colour.
void fude_zoom_repeat_restyle(fude_zoom_repeat_form* _form) {
    if(!fude_kit_form_restyle(&_form->card)) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
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

// Lays the card out, high on the screen (90 points under its top when there is room), at most 560 points wide: its
// title, its tabs, the mode's rows (two fields a row, a name before each, or a chip in a field's place), its hint, the
// line of what is wrong, and Cancel and Apply at its foot. Only the chosen mode's widgets are shown.
RDE_INTERNAL void fude_zoom_repeatf_layout(fude_zoom_repeat_form* _form) {
    const rde_vec_2F _screen = fude_kit_screen_size(_form->card.window);
    const f32 _pad = FUDE_KIT_FORM_PAD, _gap = FUDE_KIT_FORM_GAP, _rh = FUDE_ZOOM_REPEATF_ROW_H, _ch = FUDE_ZOOM_REPEATF_CHIP_H;
    const f32 _w  = rde_math_min_f32(560.0f, _screen.x - 32.0f);
    const f32 _iw = _w - 2.0f * _pad;
    const u8  _mode = _form->was.mode;
    const u32 _rows = _mode == FUDE_ZOOM_REPEAT_ROW ? 2u : _mode == FUDE_ZOOM_REPEAT_ROUND ? 3u : 2u;
    const f32 _h  = _pad + 28.0f + _gap + _ch + _gap + (f32)_rows * (_rh + _gap) + 20.0f + 22.0f + _gap + 44.0f + _pad;
    f32 _y = fude_kit_form_place(&_form->card, _screen, FUDE_KIT_FORM_HIGH_KEPT, _w, _h);
    const f32 _tw = (_iw - 2.0f * _gap) / 3.0f;
    for(u32 _i = 0; _i < FUDE_ZOOM_REPEAT_MODES; _i++) {
        fude_kit_place(rde_ui_button_as_node(_form->modes[_i]), (rde_vec_2F){ _pad + (f32)_i * (_tw + _gap) + _tw * 0.5f, _y - _ch * 0.5f }, (rde_vec_2F){ _tw, _ch });
    }
    _y -= _ch + _gap;
    const f32 _col = (_iw - 2.0f * _gap) * 0.5f;
    const f32 _nw  = rde_math_min_f32(96.0f, _col * 0.45f), _fw = _col - _nw - 4.0f;
    for(u32 _i = 0; _i < FUDE_ZOOM_REPEAT_FIELDS; _i++) {
        const b8 _on = FUDE_ZOOM_REPEATF_MODE_OF[_i] == _mode;
        rde_ui_node_set_active(rde_ui_label_as_node(_form->names[_i]), _on);
        rde_ui_node_set_active(fude_kit_field_node(_form->fields[_i]), _on);
        if(!_on) {
            continue;
        }
        const f32 _x  = _pad + (f32)FUDE_ZOOM_REPEATF_COL_OF[_i] * (_col + 2.0f * _gap);
        const f32 _ry = _y - (f32)FUDE_ZOOM_REPEATF_ROW_OF[_i] * (_rh + _gap) - _rh * 0.5f;
        fude_kit_form_named(_form->names[_i], _form->fields[_i], _x, _ry, _nw, _fw, _rh);
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
    fude_kit_form_buttons(_form->cancel, _form->save, _w);
}

// See repeatform.h. The row's copies are _now's (3 when it has none); the round's always start at 6. The mirror's line
// starts where `was` puts it (fude_zoom_repeatf_line).
void fude_zoom_repeat_open(fude_zoom_repeat_form* _form, const fude_zoom_repeat_values* _now, const fude_zoom_units_style* _units,
                           fude_zoom_repeat_done _done, any _self) {
    if(_form->card.modal.card == NULL) {
        return;
    }
    _form->was   = *_now;
    _form->units = *_units;
    _form->done  = _done;
    _form->self  = _self;
    c8 _n[32];
    rde_snprintf(_n, sizeof(_n), "%u", _now->copies > 0u ? _now->copies : 3u);
    fude_kit_form_set(&_form->card, _form->fields[FUDE_ZOOM_REPEATF_ROW_COPIES], _n);
    fude_kit_form_set(&_form->card, _form->fields[FUDE_ZOOM_REPEATF_ROUND_COPIES], "6");
    _form->at_typed = false;
    fude_zoom_repeatf_line(_form);
    fude_zoom_repeatf_set_length(_form, FUDE_ZOOM_REPEATF_ROW_X, _now->dx_mm);
    fude_zoom_repeatf_set_length(_form, FUDE_ZOOM_REPEATF_ROW_Y, _now->dy_mm);
    fude_zoom_repeatf_set_length(_form, FUDE_ZOOM_REPEATF_ROUND_X, _now->cx_mm);
    fude_zoom_repeatf_set_length(_form, FUDE_ZOOM_REPEATF_ROUND_Y, _now->cy_mm);
    c8 _deg[FUDE_ZOOM_UNITS_TEXT];
    fude_zoom_units_format_angle(rde_math_abs_f64(_now->angle) > 1e-9 ? _now->angle : 360.0, 2u, _deg, sizeof(_deg));
    fude_kit_form_set(&_form->card, _form->fields[FUDE_ZOOM_REPEATF_ROUND_ANGLE], _deg);
    rde_ui_label_set_text(_form->problem, "");
    fude_kit_form_show(&_form->card);
    fude_zoom_repeatf_layout(_form);
    fude_zoom_repeat_restyle(_form);
}

// See repeatform.h.
void fude_zoom_repeat_close(fude_zoom_repeat_form* _form) {
    fude_kit_form_close(&_form->card);
}

// See repeatform.h.
void fude_zoom_repeat_update(fude_zoom_repeat_form* _form) {
    if(fude_kit_form_resized(&_form->card)) {
        fude_zoom_repeatf_layout(_form);
    }
}

// See repeatform.h. It presses the card's own tabs and Apply, so the harness goes through what a tap does, and does
// nothing while the card is not up.
void fude_zoom_repeat_look(fude_zoom_repeat_form* _form, u8 _mode, const c8* const* _typed, i32 _axis, i32 _keep, b8 _apply) {
    if(!fude_zoom_repeat_shown(_form)) {
        return;
    }
    fude_zoom_repeatf_on_mode(NULL, NULL, &_form->refs[_mode % FUDE_ZOOM_REPEAT_MODES]);
    for(u32 _i = 0; _typed != NULL && _i < FUDE_ZOOM_REPEAT_FIELDS; _i++) {
        if(_typed[_i] != NULL) {
            fude_kit_form_set(&_form->card, _form->fields[_i], _typed[_i]);
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
