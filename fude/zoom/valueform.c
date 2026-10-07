// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/valueform.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/widgets/icons.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FUDE_ZOOM_VALUEF_PAD  16.0f
#define FUDE_ZOOM_VALUEF_GAP  8.0f
#define FUDE_ZOOM_VALUEF_KEY  52.0f   // a key's height
#define FUDE_ZOOM_VALUEF_CHIP 40.0f   // a unit's or a way's

// The keys' faces, in their order (10 the point, 11 the minus, 12 back).
RDE_INTERNAL const c8* const FUDE_ZOOM_VALUEF_FACES[FUDE_ZOOM_VALUE_KEYS] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "\xE2\x88\x92", "" };

RDE_INTERNAL void fude_zoom_valuef_show(fude_zoom_value_form* _form) {
    c8 _say[48];
    snprintf(_say, sizeof(_say), "%s%s%s", _form->typed[0] != 0 ? _form->typed : "0", _form->unit_count > 0u ? " " : "",
             _form->unit_count > 0u ? _form->unit_names[_form->unit] : "");
    rde_ui_button_set_text(_form->display, _say);
    for(u32 _i = 0; _i < FUDE_ZOOM_VALUE_UNITS; _i++) {
        fude_kit_button_chip(_form->units[_i], _i == _form->unit);
    }
    for(u32 _i = 0; _i < FUDE_ZOOM_VALUE_WAYS; _i++) {
        fude_kit_button_chip(_form->ways[_i], _i == _form->way);
    }
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_valuef_on_key(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_value_ref* _ref = (const fude_zoom_value_ref*)_user_data;
    fude_zoom_value_form* _form = _ref->form;
    usize _n = strlen(_form->typed);
    if(_form->fresh && _ref->index != 12u) {
        _form->typed[0] = 0;   // (what it opened with goes as a new number is typed)
        _n = 0;
    }
    _form->fresh = false;
    if(_ref->index == 12u) {
        if(_n > 0u) {
            _form->typed[_n - 1u] = 0;
        }
    } else if(_ref->index == 11u) {
        // (the minus: the number's sign turned)
        if(_form->typed[0] == '-') {
            memmove(_form->typed, _form->typed + 1, _n);
        } else if(_n + 1u < sizeof(_form->typed)) {
            memmove(_form->typed + 1, _form->typed, _n + 1u);
            _form->typed[0] = '-';
        }
    } else if(_ref->index == 10u) {
        if(strchr(_form->typed, '.') == NULL && _n + 2u < sizeof(_form->typed)) {
            if(_n == 0u || (_n == 1u && _form->typed[0] == '-')) {
                _form->typed[_n++] = '0';
            }
            _form->typed[_n++] = '.';
            _form->typed[_n] = 0;
        }
    } else if(_n + 1u < sizeof(_form->typed) && _n < 12u) {
        _form->typed[_n] = (c8)('0' + _ref->index);
        _form->typed[_n + 1u] = 0;
    }
    fude_zoom_valuef_show(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_valuef_on_unit(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_value_ref* _ref = (const fude_zoom_value_ref*)_user_data;
    _ref->form->unit = _ref->index;
    fude_zoom_valuef_show(_ref->form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_valuef_on_way(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_value_ref* _ref = (const fude_zoom_value_ref*)_user_data;
    _ref->form->way = _ref->index;
    fude_zoom_valuef_show(_ref->form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_valuef_on_ok(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_value_form* _form = (fude_zoom_value_form*)_user_data;
    const fude_zoom_value_done _done = _form->done;
    void* const _self = _form->self;
    const f64 _number = _form->typed[0] != 0 ? strtod(_form->typed, NULL) : 0.0;
    const u32 _unit = _form->unit, _way = _form->way;
    fude_zoom_value_close(_form);
    if(_done != NULL && isfinite(_number)) {
        _done(_self, _number, _unit, _way);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_valuef_on_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_value_close((fude_zoom_value_form*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A tap off the card: put away — not the release of the press that opened it.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_valuef_on_dismiss(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_value_form* _form = (fude_zoom_value_form*)_user_data;
    if(rde_engine_get_time_now() - _form->opened_at >= 0.6) {
        fude_zoom_value_close(_form);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_valuef_on_display(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

void fude_zoom_value_build(fude_zoom_value_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font) {
    memset(_form, 0, sizeof(*_form));
    _form->window = _window;
    _form->font   = _font;
    fude_kit_modal_create(&_form->modal, _root, fude_zoom_valuef_on_dismiss, _form);
    rde_ui_node* _card = rde_ui_image_as_node(_form->modal.card);
    _form->title = rde_ui_label_create(NULL);
    rde_ui_label_set_font(_form->title, _font);
    rde_ui_label_set_font_scale(_form->title, 17.0f / (f32)FUDE_KIT_FONT_SIZE);
    rde_ui_label_set_alignment(_form->title, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_MIDDLE);
    rde_ui_label_set_auto_fit(_form->title, true);   // a longer language shrinks it, never cuts it
    rde_ui_label_set_auto_fit_min_scale(_form->title, 0.6f);
    rde_ui_node_set_raycast_target(rde_ui_label_as_node(_form->title), false);
    rde_ui_node_add_child(_card, rde_ui_label_as_node(_form->title));
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
    _form->cancel = fude_kit_button(_card, fude_text(FUDE_TEXT_CANCEL), fude_zoom_valuef_on_cancel, _form);
    fude_zoom_value_restyle(_form);
    fude_kit_modal_show(&_form->modal, false);
}

void fude_zoom_value_forget(fude_zoom_value_form* _form) {
    memset(_form, 0, sizeof(*_form));
}

b8 fude_zoom_value_shown(const fude_zoom_value_form* _form) {
    return _form->modal.card != NULL && _form->modal.shown;
}

void fude_zoom_value_restyle(fude_zoom_value_form* _form) {
    if(_form->modal.card == NULL) {
        return;
    }
    fude_kit_modal_restyle(&_form->modal, 16.0f);
    rde_ui_label_set_color(_form->title, fude_theme_active()->text);
    fude_kit_button_plain(_form->display);
    for(u32 _i = 0; _i < FUDE_ZOOM_VALUE_KEYS; _i++) {
        fude_kit_button_plain(_form->keys[_i]);
    }
    fude_kit_button_primary(_form->ok);
    fude_kit_button_quiet(_form->cancel);
    fude_zoom_valuef_show(_form);
}

RDE_INTERNAL void fude_zoom_valuef_layout(fude_zoom_value_form* _form) {
    const rde_vec_2F _screen = fude_kit_screen_size(_form->window);
    const f32 _pad = FUDE_ZOOM_VALUEF_PAD, _gap = FUDE_ZOOM_VALUEF_GAP, _kh = FUDE_ZOOM_VALUEF_KEY, _ch = FUDE_ZOOM_VALUEF_CHIP;
    const f32 _w  = fminf(380.0f, _screen.x - 32.0f);
    const f32 _iw = _w - 2.0f * _pad;
    const u32 _rows = (_form->unit_count > 0u ? 1u : 0u) + (_form->way_count > 0u ? 1u : 0u);
    const f32 _h  = _pad + 28.0f + _gap + 52.0f + _gap + 4.0f * (_kh + _gap) + (f32)_rows * (_ch + _gap) + 48.0f + _pad;
    _form->laid_out = _screen;
    fude_kit_modal_place(&_form->modal, _form->window, (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.5f }, (rde_vec_2F){ _w, _h });
    f32 _y = _h - _pad;
    fude_kit_place(rde_ui_label_as_node(_form->title), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 14.0f }, (rde_vec_2F){ _iw, 28.0f });
    _y -= 28.0f + _gap;
    // The number as typed, back a digit beside it.
    const f32 _bw = 64.0f;
    fude_kit_place(rde_ui_button_as_node(_form->display), (rde_vec_2F){ _pad + (_iw - _bw - _gap) * 0.5f, _y - 26.0f }, (rde_vec_2F){ _iw - _bw - _gap, 52.0f });
    fude_kit_place(rde_ui_button_as_node(_form->keys[12]), (rde_vec_2F){ _w - _pad - _bw * 0.5f, _y - 26.0f }, (rde_vec_2F){ _bw, 52.0f });
    _y -= 52.0f + _gap;
    // 7 8 9 / 4 5 6 / 1 2 3 / − 0 .
    static const u32 _order[12] = { 7, 8, 9, 4, 5, 6, 1, 2, 3, 11, 0, 10 };
    const f32 _kw = (_iw - 2.0f * _gap) / 3.0f;
    for(u32 _i = 0; _i < 12u; _i++) {
        const f32 _x = _pad + (f32)(_i % 3u) * (_kw + _gap) + _kw * 0.5f;
        const f32 _ky = _y - (f32)(_i / 3u) * (_kh + _gap) - _kh * 0.5f;
        fude_kit_place(rde_ui_button_as_node(_form->keys[_order[_i]]), (rde_vec_2F){ _x, _ky }, (rde_vec_2F){ _kw, _kh });
    }
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
    fude_kit_place(rde_ui_button_as_node(_form->ok), (rde_vec_2F){ _w - _pad - 60.0f, _pad + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_form->cancel), (rde_vec_2F){ _w - _pad - 120.0f - _gap - 60.0f, _pad + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
}

void fude_zoom_value_open(fude_zoom_value_form* _form, const c8* _title, const c8* _number, const c8* const* _units, u32 _unit_count, u32 _unit,
                          const c8* const* _ways, u32 _way_count, u32 _way, fude_zoom_value_done _done, void* _self) {
    if(_form->modal.card == NULL) {
        return;
    }
    _form->unit_count = _units != NULL ? (_unit_count < FUDE_ZOOM_VALUE_UNITS ? _unit_count : FUDE_ZOOM_VALUE_UNITS) : 0u;
    _form->way_count  = _ways != NULL ? (_way_count < FUDE_ZOOM_VALUE_WAYS ? _way_count : FUDE_ZOOM_VALUE_WAYS) : 0u;
    _form->unit = _unit < _form->unit_count ? _unit : 0u;
    _form->way  = _way < _form->way_count ? _way : 0u;
    _form->done = _done;
    _form->self = _self;
    _form->fresh = true;
    snprintf(_form->typed, sizeof(_form->typed), "%s", _number != NULL ? _number : "");
    rde_ui_label_set_text(_form->title, _title);
    for(u32 _i = 0; _i < _form->unit_count; _i++) {
        snprintf(_form->unit_names[_i], sizeof(_form->unit_names[_i]), "%s", _units[_i]);
        rde_ui_button_set_text(_form->units[_i], _units[_i]);
    }
    for(u32 _i = 0; _i < _form->way_count; _i++) {
        rde_ui_button_set_text(_form->ways[_i], _ways[_i]);
    }
    fude_kit_modal_show(&_form->modal, true);
    _form->opened_at = rde_engine_get_time_now();
    fude_zoom_valuef_layout(_form);
    fude_zoom_value_restyle(_form);
}

void fude_zoom_value_close(fude_zoom_value_form* _form) {
    if(_form->modal.card != NULL) {
        fude_kit_modal_show(&_form->modal, false);
    }
}

void fude_zoom_value_update(fude_zoom_value_form* _form) {
    if(!fude_zoom_value_shown(_form)) {
        return;
    }
    const rde_vec_2F _screen = fude_kit_screen_size(_form->window);
    if(_screen.x != _form->laid_out.x || _screen.y != _form->laid_out.y) {
        fude_zoom_valuef_layout(_form);
    }
}

void fude_zoom_value_look(fude_zoom_value_form* _form, const c8* _typed, u32 _unit, u32 _way) {
    if(!fude_zoom_value_shown(_form)) {
        return;
    }
    snprintf(_form->typed, sizeof(_form->typed), "%s", _typed);
    _form->fresh = false;
    _form->unit = _unit < _form->unit_count ? _unit : _form->unit;
    _form->way  = _way < _form->way_count ? _way : _form->way;
    fude_zoom_valuef_show(_form);
    fude_zoom_valuef_on_ok(NULL, NULL, _form);
}
