// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/valueform.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/base/icons.h"

// ===========================================================================
// See valueform.h.
//
// The card is the kit's form (kit.h: its title, Cancel, a tap off it, laying it out again, and the keypad's typing).
// It is built once, hidden, and only filled and shown when opened; it is laid out again on opening and when the
// screen's size changes. Its own names start with fude_zoom_valuef_ and FUDE_ZOOM_VALUEF_ (the header's are
// fude_zoom_value_). Every widget's user data is the form, its card or a fude_zoom_value_ref in it, so the form must
// stay put while the UI lives (it is the page's).
// ===========================================================================

// The card's measures, in screen points: a key's height and a chip's (its margin and gaps are the kit's form's).
#define FUDE_ZOOM_VALUEF_KEY  52.0f   // a key's height
#define FUDE_ZOOM_VALUEF_CHIP 40.0f   // a unit's or a way's

// The keys' faces, in their order (10 the point, 11 the minus, 12 back), and what each types (kit.h:
// fude_kit_form_type).
RDE_INTERNAL const c8* const FUDE_ZOOM_VALUEF_FACES[FUDE_ZOOM_VALUE_KEYS] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "\xE2\x88\x92", "" };
RDE_INTERNAL const c8        FUDE_ZOOM_VALUEF_TYPES[FUDE_ZOOM_VALUE_KEYS] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', '-', '\b' };

// Shows the number as typed ("0" when nothing is) with the chosen unit's name after it, and marks the chosen chips.
RDE_INTERNAL void fude_zoom_valuef_show(fude_zoom_value_form* _form) {
    c8 _say[48];
    rde_snprintf(_say, sizeof(_say), "%s%s%s", _form->typed[0] != 0 ? _form->typed : "0", _form->unit_count > 0u ? " " : "",
             _form->unit_count > 0u ? _form->unit_names[_form->unit] : "");
    rde_ui_button_set_text(_form->display, _say);
    for(u32 _i = 0; _i < FUDE_ZOOM_VALUE_UNITS; _i++) {
        fude_kit_button_chip(_form->units[_i], _i == _form->unit);
    }
    for(u32 _i = 0; _i < FUDE_ZOOM_VALUE_WAYS; _i++) {
        fude_kit_button_chip(_form->ways[_i], _i == _form->way);
    }
}

// A key pressed (its index as in FUDE_ZOOM_VALUEF_FACES), typed as the kit's keypad types (kit.h:
// fude_kit_form_type): the first key other than back replaces the number the card opened with; back takes off the last
// character; the minus turns the sign; the point is added once (after a 0 when nothing is before it); and a digit is
// added while the number has fewer than 12 characters.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_valuef_on_key(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_value_ref* _ref = (const fude_zoom_value_ref*)_user_data;
    fude_zoom_value_form* _form = _ref->form;
    fude_kit_form_type(_form->typed, sizeof(_form->typed), 12u, true, &_form->fresh, FUDE_ZOOM_VALUEF_TYPES[_ref->index]);
    fude_zoom_valuef_show(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A unit chip tapped: it is the one chosen.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_valuef_on_unit(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_value_ref* _ref = (const fude_zoom_value_ref*)_user_data;
    _ref->form->unit = _ref->index;
    fude_zoom_valuef_show(_ref->form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A way chip tapped: it is the one chosen.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_valuef_on_way(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_value_ref* _ref = (const fude_zoom_value_ref*)_user_data;
    _ref->form->way = _ref->index;
    fude_zoom_valuef_show(_ref->form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// OK: puts the card away, then hands the number typed (0 for nothing), the unit and the way to the opener's callback;
// a number that is not finite is handed to no one.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_valuef_on_ok(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_value_form* _form = (fude_zoom_value_form*)_user_data;
    const fude_zoom_value_done _done = _form->done;
    any const _self = _form->self;
    const f64 _number = _form->typed[0] != 0 ? rde_strtod(_form->typed, NULL) : 0.0;
    const u32 _unit = _form->unit, _way = _form->way;
    fude_zoom_value_close(_form);
    if(_done != NULL && rde_math_is_finite_f64(_number)) {
        _done(_self, _number, _unit, _way);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A tap on the number's display does nothing, and is taken so it does not reach the card's background.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_valuef_on_display(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// See the header. Every chip is made now, inactive; fude_zoom_valuef_layout shows the ones a part has.
void fude_zoom_value_build(fude_zoom_value_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font) {
    rde_memset(_form, 0, sizeof(*_form));
    rde_ui_node* _card = fude_kit_form_build(&_form->card, _root, _window, _font, NULL);
    _form->display = fude_kit_button(_card, "", fude_zoom_valuef_on_display, _form);
    for(u32 _i = 0; _i < FUDE_ZOOM_VALUE_KEYS; _i++) {
        _form->key_refs[_i] = (fude_zoom_value_ref){ _form, _i };
        _form->keys[_i] = fude_kit_button(_card, FUDE_ZOOM_VALUEF_FACES[_i], fude_zoom_valuef_on_key, &_form->key_refs[_i]);
    }
    fude_kit_icon(_form->keys[12], FUDE_ICON_BACK, FUDE_KIT_ICON_LEFT, 20.0f);
    for(u32 _i = 0; _i < FUDE_ZOOM_VALUE_UNITS; _i++) {
        _form->unit_refs[_i] = (fude_zoom_value_ref){ _form, _i };
        _form->units[_i] = fude_kit_button(_card, "", fude_zoom_valuef_on_unit, &_form->unit_refs[_i]);
        rde_ui_node_set_active(rde_ui_button_as_node(_form->units[_i]), false);
    }
    for(u32 _i = 0; _i < FUDE_ZOOM_VALUE_WAYS; _i++) {
        _form->way_refs[_i] = (fude_zoom_value_ref){ _form, _i };
        _form->ways[_i] = fude_kit_button(_card, "", fude_zoom_valuef_on_way, &_form->way_refs[_i]);
        rde_ui_node_set_active(rde_ui_button_as_node(_form->ways[_i]), false);
    }
    _form->ok     = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_SIZE_APPLY), fude_zoom_valuef_on_ok, _form);
    _form->cancel = fude_kit_button(_card, fude_text(FUDE_TEXT_CANCEL), fude_kit_form_on_cancel, &_form->card);
    fude_zoom_value_restyle(_form);
    fude_kit_modal_show(&_form->card.modal, false);
}

// See the header. With the card NULL, every other call does nothing.
void fude_zoom_value_forget(fude_zoom_value_form* _form) {
    rde_memset(_form, 0, sizeof(*_form));
}

// See the header.
b8 fude_zoom_value_shown(const fude_zoom_value_form* _form) {
    return fude_kit_form_shown(&_form->card);
}

// See the header.
void fude_zoom_value_restyle(fude_zoom_value_form* _form) {
    if(!fude_kit_form_restyle(&_form->card)) {
        return;
    }
    fude_kit_button_plain(_form->display);
    for(u32 _i = 0; _i < FUDE_ZOOM_VALUE_KEYS; _i++) {
        fude_kit_button_plain(_form->keys[_i]);
    }
    fude_kit_button_primary(_form->ok);
    fude_kit_button_quiet(_form->cancel);
    fude_zoom_valuef_show(_form);
}

// Places the card in the middle of the screen, at most 380 points wide, and its widgets from the top down: the title,
// the number with back beside it, the keypad, a row of unit chips and a row of way chips (each only when there are
// any), then Cancel and OK at the bottom right.
RDE_INTERNAL void fude_zoom_valuef_layout(fude_zoom_value_form* _form) {
    const rde_vec_2F _screen = fude_kit_screen_size(_form->card.window);
    const f32 _pad = FUDE_KIT_FORM_PAD, _gap = FUDE_KIT_FORM_GAP, _kh = FUDE_ZOOM_VALUEF_KEY, _ch = FUDE_ZOOM_VALUEF_CHIP;
    const f32 _w  = rde_math_min_f32(380.0f, _screen.x - 32.0f);
    const f32 _iw = _w - 2.0f * _pad;
    const u32 _rows = (_form->unit_count > 0u ? 1u : 0u) + (_form->way_count > 0u ? 1u : 0u);
    const f32 _h  = _pad + 28.0f + _gap + 52.0f + _gap + 4.0f * (_kh + _gap) + (f32)_rows * (_ch + _gap) + 48.0f + _pad;
    f32 _y = fude_kit_form_place(&_form->card, _screen, FUDE_KIT_FORM_MIDDLE, _w, _h);
    // The number as typed, back a digit beside it.
    const f32 _bw = 64.0f;
    fude_kit_place(rde_ui_button_as_node(_form->display), (rde_vec_2F){ _pad + (_iw - _bw - _gap) * 0.5f, _y - 26.0f }, (rde_vec_2F){ _iw - _bw - _gap, 52.0f });
    fude_kit_place(rde_ui_button_as_node(_form->keys[12]), (rde_vec_2F){ _w - _pad - _bw * 0.5f, _y - 26.0f }, (rde_vec_2F){ _bw, 52.0f });
    _y -= 52.0f + _gap;
    // 7 8 9 / 4 5 6 / 1 2 3 / − 0 .
    static const u32 _order[12] = { 7, 8, 9, 4, 5, 6, 1, 2, 3, 11, 0, 10 };
    const f32 _kw = (_iw - 2.0f * _gap) / 3.0f;
    fude_kit_form_keypad(_form->keys, _order, 12u, _pad, _y, _kw, _kh);
    _y -= 4.0f * (_kh + _gap);
    // Its units, then its ways: chips across, as wide as they share.
    for(u32 _row = 0; _row < 2u; _row++) {
        const u32 _n = _row == 0u ? _form->unit_count : _form->way_count;
        rde_ui_button** _chips = _row == 0u ? _form->units : _form->ways;
        const u32 _most = _row == 0u ? FUDE_ZOOM_VALUE_UNITS : FUDE_ZOOM_VALUE_WAYS;
        for(u32 _i = 0; _i < _most; _i++) {
            rde_ui_node_set_active(rde_ui_button_as_node(_chips[_i]), _i < _n);
        }
        if(_n == 0u) {
            continue;
        }
        const f32 _cw = (_iw - (f32)(_n - 1u) * _gap) / (f32)_n;
        for(u32 _i = 0; _i < _n; _i++) {
            fude_kit_place(rde_ui_button_as_node(_chips[_i]), (rde_vec_2F){ _pad + (f32)_i * (_cw + _gap) + _cw * 0.5f, _y - _ch * 0.5f }, (rde_vec_2F){ _cw, _ch });
        }
        _y -= _ch + _gap;
    }
    fude_kit_form_buttons(_form->cancel, _form->ok, _w);
}

// See the header. More units or ways than the card has chips are left off, and a chosen index out of range chooses
// the first.
void fude_zoom_value_open(fude_zoom_value_form* _form, const c8* _title, const c8* _number, const c8* const* _units, u32 _unit_count, u32 _unit,
                          const c8* const* _ways, u32 _way_count, u32 _way, fude_zoom_value_done _done, any _self) {
    if(_form->card.modal.card == NULL) {
        return;
    }
    _form->unit_count = _units != NULL ? (_unit_count < FUDE_ZOOM_VALUE_UNITS ? _unit_count : FUDE_ZOOM_VALUE_UNITS) : 0u;
    _form->way_count  = _ways != NULL ? (_way_count < FUDE_ZOOM_VALUE_WAYS ? _way_count : FUDE_ZOOM_VALUE_WAYS) : 0u;
    _form->unit = _unit < _form->unit_count ? _unit : 0u;
    _form->way  = _way < _form->way_count ? _way : 0u;
    _form->done = _done;
    _form->self = _self;
    _form->fresh = true;
    rde_snprintf(_form->typed, sizeof(_form->typed), "%s", _number != NULL ? _number : "");
    rde_ui_label_set_text(_form->card.title, _title);
    for(u32 _i = 0; _i < _form->unit_count; _i++) {
        rde_snprintf(_form->unit_names[_i], sizeof(_form->unit_names[_i]), "%s", _units[_i]);
        rde_ui_button_set_text(_form->units[_i], _units[_i]);
    }
    for(u32 _i = 0; _i < _form->way_count; _i++) {
        rde_ui_button_set_text(_form->ways[_i], _ways[_i]);
    }
    fude_kit_form_show(&_form->card);
    fude_zoom_valuef_layout(_form);
    fude_zoom_value_restyle(_form);
}

// See the header.
void fude_zoom_value_close(fude_zoom_value_form* _form) {
    fude_kit_form_close(&_form->card);
}

// See the header.
void fude_zoom_value_update(fude_zoom_value_form* _form) {
    if(fude_kit_form_resized(&_form->card)) {
        fude_zoom_valuef_layout(_form);
    }
}

// See the header. OK is pressed through its own callback, so the opener's callback runs as on a real press.
void fude_zoom_value_look(fude_zoom_value_form* _form, const c8* _typed, u32 _unit, u32 _way) {
    if(!fude_zoom_value_shown(_form)) {
        return;
    }
    rde_snprintf(_form->typed, sizeof(_form->typed), "%s", _typed);
    _form->fresh = false;
    _form->unit = _unit < _form->unit_count ? _unit : _form->unit;
    _form->way  = _way < _form->way_count ? _way : _form->way;
    fude_zoom_valuef_show(_form);
    fude_zoom_valuef_on_ok(NULL, NULL, _form);
}
