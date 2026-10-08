// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/limitsform.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/widgets/icons.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FUDE_ZOOM_LIMITSF_PAD   16.0f
#define FUDE_ZOOM_LIMITSF_GAP   8.0f
#define FUDE_ZOOM_LIMITSF_CHIP  40.0f   // a real part's, a unit's
#define FUDE_ZOOM_LIMITSF_FIELD 44.0f   // a limit's row
#define FUDE_ZOOM_LIMITSF_KEY   48.0f   // a key's height (less, on a short screen)

// The keys' faces, in their order (10 the point, 11 back).
RDE_INTERNAL const c8* const FUDE_ZOOM_LIMITSF_FACES[FUDE_ZOOM_LIMITS_KEYS] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "" };

// A limit's units: the whole one and a thousandth of it (none: volts only).
RDE_INTERNAL const c8* fude_zoom_limitsf_unit(u32 _field, u32 _unit) {
    if(_field == FUDE_ZOOM_LIMIT_POWER) {
        return _unit == 0u ? "W" : "mW";
    }
    if(_field == FUDE_ZOOM_LIMIT_CURRENT) {
        return _unit == 0u ? "A" : "mA";
    }
    return "V";
}

RDE_INTERNAL b8 fude_zoom_limitsf_milli(u32 _field) {
    return _field == FUDE_ZOOM_LIMIT_POWER || _field == FUDE_ZOOM_LIMIT_CURRENT;
}

// A number as written on the card: at most 4 figures, no trailing zeros.
RDE_INTERNAL void fude_zoom_limitsf_number(f64 _v, c8* _out, usize _size) {
    snprintf(_out, _size, "%.4g", _v);
}

// A limit as its row says it: its name, then how much in the unit it reads best in (no such limit: none).
RDE_INTERNAL void fude_zoom_limitsf_row(const fude_zoom_limits_form* _form, u32 _field, c8* _out, usize _size) {
    static const FUDE_TEXT_ _names[FUDE_ZOOM_LIMIT_COUNT] = { FUDE_TEXT_ZOOM_LIMIT_POWER, FUDE_TEXT_ZOOM_LIMIT_CURRENT, FUDE_TEXT_ZOOM_LIMIT_VOLTAGE,
                                                              FUDE_TEXT_ZOOM_LIMIT_REVERSE };
    const c8* _name = fude_text(_field == FUDE_ZOOM_LIMIT_REVERSE && _form->gate ? FUDE_TEXT_ZOOM_LIMIT_GATE : _names[_field]);
    const f64 _v = _form->most[_field];
    if(!(_v > 0.0)) {
        snprintf(_out, _size, "%s:  %s", _name, fude_text(FUDE_TEXT_ZOOM_LIMIT_NONE));
        return;
    }
    const b8 _small = fude_zoom_limitsf_milli(_field) && _v < 1.0;
    c8 _n[24];
    fude_zoom_limitsf_number(_small ? _v * 1000.0 : _v, _n, sizeof(_n));
    snprintf(_out, _size, "%s:  %s %s", _name, _n, fude_zoom_limitsf_unit(_field, _small ? 1u : 0u));
}

RDE_INTERNAL void fude_zoom_limitsf_show(fude_zoom_limits_form* _form) {
    for(u32 _k = 0; _k < FUDE_ZOOM_LIMIT_COUNT; _k++) {
        c8 _say[96];
        fude_zoom_limitsf_row(_form, _k, _say, sizeof(_say));
        rde_ui_button_set_text(_form->fields[_k], _say);
        fude_kit_button_chip(_form->fields[_k], _k == _form->field);
    }
    for(u32 _i = 0; _i < FUDE_ZOOM_LIMITS_PRESETS; _i++) {
        fude_kit_button_chip(_form->presets[_i], (i32)_i == _form->preset);
    }
    const b8 _two = fude_zoom_limitsf_milli(_form->field);
    for(u32 _u = 0; _u < 2u; _u++) {
        rde_ui_button_set_text(_form->units[_u], fude_zoom_limitsf_unit(_form->field, _u));
        rde_ui_node_set_active(rde_ui_button_as_node(_form->units[_u]), _two || _u == 0u);
        fude_kit_button_chip(_form->units[_u], _u == _form->unit);
    }
}

// Field _field to be typed: its number in the unit it reads best in, replaced by the first key.
RDE_INTERNAL void fude_zoom_limitsf_pick(fude_zoom_limits_form* _form, u32 _field) {
    _form->field = _field;
    const f64 _v = _form->most[_field];
    _form->unit  = fude_zoom_limitsf_milli(_field) && _v > 0.0 && _v < 1.0 ? 1u : 0u;
    if(_v > 0.0) {
        fude_zoom_limitsf_number(_form->unit == 1u ? _v * 1000.0 : _v, _form->typed, sizeof(_form->typed));
    } else {
        _form->typed[0] = 0;
    }
    _form->fresh = true;
}

// What is typed, in its unit, into the field being typed (its own limits now: no real part's).
RDE_INTERNAL void fude_zoom_limitsf_take(fude_zoom_limits_form* _form) {
    const f64 _n = _form->typed[0] != 0 ? strtod(_form->typed, NULL) : 0.0;
    _form->most[_form->field] = isfinite(_n) && _n > 0.0 ? _n * (_form->unit == 1u ? 1e-3 : 1.0) : 0.0;
    _form->preset = -1;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_limitsf_on_key(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_limits_ref* _ref = (const fude_zoom_limits_ref*)_user_data;
    fude_zoom_limits_form* _form = _ref->form;
    usize _n = strlen(_form->typed);
    if(_form->fresh && _ref->index != 11u) {
        _form->typed[0] = 0;   // (what it was goes as a new number is typed)
        _n = 0;
    }
    _form->fresh = false;
    if(_ref->index == 11u) {
        if(_n > 0u) {
            _form->typed[_n - 1u] = 0;
        }
    } else if(_ref->index == 10u) {
        if(strchr(_form->typed, '.') == NULL && _n + 2u < sizeof(_form->typed)) {
            if(_n == 0u) {
                _form->typed[_n++] = '0';
            }
            _form->typed[_n++] = '.';
            _form->typed[_n] = 0;
        }
    } else if(_n + 1u < sizeof(_form->typed) && _n < 9u) {
        _form->typed[_n] = (c8)('0' + _ref->index);
        _form->typed[_n + 1u] = 0;
    }
    fude_zoom_limitsf_take(_form);
    fude_zoom_limitsf_show(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_limitsf_on_unit(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_limits_ref* _ref = (const fude_zoom_limits_ref*)_user_data;
    fude_zoom_limits_form* _form = _ref->form;
    if(_ref->index != _form->unit && fude_zoom_limitsf_milli(_form->field)) {
        _form->unit = _ref->index;
        if(_form->typed[0] != 0) {
            fude_zoom_limitsf_take(_form);   // (the number as typed, in the other unit)
        }
    }
    fude_zoom_limitsf_show(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_limitsf_on_field(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_limits_ref* _ref = (const fude_zoom_limits_ref*)_user_data;
    fude_zoom_limitsf_pick(_ref->form, _ref->index);
    fude_zoom_limitsf_show(_ref->form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_limitsf_on_preset(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_limits_ref* _ref = (const fude_zoom_limits_ref*)_user_data;
    fude_zoom_limits_form* _form = _ref->form;
    if(_form->table != NULL && _ref->index < _form->table_count) {
        memcpy(_form->most, _form->table[_ref->index].most, sizeof(_form->most));
        _form->preset = (i32)_ref->index;
        fude_zoom_limitsf_pick(_form, _form->field);
    }
    fude_zoom_limitsf_show(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_limitsf_on_typical(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_limits_form* _form = (fude_zoom_limits_form*)_user_data;
    memcpy(_form->most, _form->typical, sizeof(_form->most));
    _form->preset = _form->typical_preset;
    fude_zoom_limitsf_pick(_form, _form->field);
    fude_zoom_limitsf_show(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_limitsf_on_apply(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_limits_form* _form = (fude_zoom_limits_form*)_user_data;
    const fude_zoom_limits_done _done = _form->done;
    void* const _self = _form->self;
    f64 _most[FUDE_ZOOM_LIMIT_COUNT];
    memcpy(_most, _form->most, sizeof(_most));
    const i32 _preset = _form->preset;
    fude_zoom_limits_close(_form);
    if(_done != NULL) {
        _done(_self, _most, _preset);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_limitsf_on_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_limits_close((fude_zoom_limits_form*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A tap off the card: put away — not the release of the press that opened it.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_limitsf_on_dismiss(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_limits_form* _form = (fude_zoom_limits_form*)_user_data;
    if(rde_engine_get_time_now() - _form->opened_at >= 0.6) {
        fude_zoom_limits_close(_form);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL rde_ui_label* fude_zoom_limitsf_label(rde_ui_node* _card, rde_font* _font, f32 _px) {
    rde_ui_label* _l = rde_ui_label_create(NULL);
    rde_ui_label_set_font(_l, _font);
    rde_ui_label_set_font_scale(_l, _px / (f32)FUDE_KIT_FONT_SIZE);
    rde_ui_label_set_alignment(_l, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_MIDDLE);
    rde_ui_label_set_auto_fit(_l, true);   // a longer language shrinks it, never cuts it
    rde_ui_label_set_auto_fit_min_scale(_l, 0.6f);
    rde_ui_node_set_raycast_target(rde_ui_label_as_node(_l), false);
    rde_ui_node_add_child(_card, rde_ui_label_as_node(_l));
    return _l;
}

void fude_zoom_limits_build(fude_zoom_limits_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font) {
    memset(_form, 0, sizeof(*_form));
    _form->window = _window;
    _form->font   = _font;
    fude_kit_modal_create(&_form->modal, _root, fude_zoom_limitsf_on_dismiss, _form);
    rde_ui_node* _card = rde_ui_image_as_node(_form->modal.card);
    _form->title = fude_zoom_limitsf_label(_card, _font, 17.0f);
    _form->how   = fude_zoom_limitsf_label(_card, _font, 13.0f);
    for(u32 _i = 0; _i < FUDE_ZOOM_LIMITS_PRESETS; _i++) {
        _form->preset_refs[_i] = (fude_zoom_limits_ref){ _form, _i };
        _form->presets[_i] = fude_kit_button(_card, "", fude_zoom_limitsf_on_preset, &_form->preset_refs[_i]);
        rde_ui_node_set_active(rde_ui_button_as_node(_form->presets[_i]), false);
    }
    for(u32 _k = 0; _k < FUDE_ZOOM_LIMIT_COUNT; _k++) {
        _form->field_refs[_k] = (fude_zoom_limits_ref){ _form, _k };
        _form->fields[_k] = fude_kit_button(_card, "", fude_zoom_limitsf_on_field, &_form->field_refs[_k]);
        rde_ui_node_set_active(rde_ui_button_as_node(_form->fields[_k]), false);
    }
    for(u32 _i = 0; _i < FUDE_ZOOM_LIMITS_KEYS; _i++) {
        _form->key_refs[_i] = (fude_zoom_limits_ref){ _form, _i };
        _form->keys[_i] = fude_kit_button(_card, FUDE_ZOOM_LIMITSF_FACES[_i], fude_zoom_limitsf_on_key, &_form->key_refs[_i]);
    }
    fude_kit_icon(_form->keys[11], FUDE_ICON_BACK, FUDE_KIT_ICON_LEFT, 20.0f);
    for(u32 _u = 0; _u < 2u; _u++) {
        _form->unit_refs[_u] = (fude_zoom_limits_ref){ _form, _u };
        _form->units[_u] = fude_kit_button(_card, "", fude_zoom_limitsf_on_unit, &_form->unit_refs[_u]);
    }
    _form->restore = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_LIMITS_TYPICAL), fude_zoom_limitsf_on_typical, _form);
    _form->apply   = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_SIZE_APPLY), fude_zoom_limitsf_on_apply, _form);
    _form->cancel  = fude_kit_button(_card, fude_text(FUDE_TEXT_CANCEL), fude_zoom_limitsf_on_cancel, _form);
    fude_zoom_limits_restyle(_form);
    fude_kit_modal_show(&_form->modal, false);
}

void fude_zoom_limits_forget(fude_zoom_limits_form* _form) {
    memset(_form, 0, sizeof(*_form));
}

b8 fude_zoom_limits_shown(const fude_zoom_limits_form* _form) {
    return _form->modal.card != NULL && _form->modal.shown;
}

void fude_zoom_limits_restyle(fude_zoom_limits_form* _form) {
    if(_form->modal.card == NULL) {
        return;
    }
    fude_kit_modal_restyle(&_form->modal, 16.0f);
    rde_ui_label_set_color(_form->title, fude_theme_active()->text);
    rde_ui_label_set_color(_form->how, fude_theme_active()->text_soft);
    for(u32 _i = 0; _i < FUDE_ZOOM_LIMITS_KEYS; _i++) {
        fude_kit_button_plain(_form->keys[_i]);
    }
    fude_kit_button_quiet(_form->restore);
    fude_kit_button_primary(_form->apply);
    fude_kit_button_quiet(_form->cancel);
    fude_zoom_limitsf_show(_form);
}

RDE_INTERNAL void fude_zoom_limitsf_layout(fude_zoom_limits_form* _form) {
    const rde_vec_2F _screen = fude_kit_screen_size(_form->window);
    const f32 _pad = FUDE_ZOOM_LIMITSF_PAD, _gap = FUDE_ZOOM_LIMITSF_GAP, _ch = FUDE_ZOOM_LIMITSF_CHIP, _fh = FUDE_ZOOM_LIMITSF_FIELD;
    const f32 _w  = fminf(420.0f, _screen.x - 32.0f);
    const f32 _iw = _w - 2.0f * _pad;
    const u32 _presets = _form->table_count < FUDE_ZOOM_LIMITS_PRESETS ? _form->table_count : FUDE_ZOOM_LIMITS_PRESETS;
    const u32 _preset_rows = (_presets + 2u) / 3u;
    u32 _fields = 0;
    for(u32 _k = 0; _k < FUDE_ZOOM_LIMIT_COUNT; _k++) {
        _fields += (_form->kinds >> _k) & 1u;
    }
    // (its keys as tall as the screen lets them be: 48 points, 34 at the least)
    const f32 _rest = _pad + 28.0f + _gap + 20.0f + _gap + (f32)_preset_rows * (_ch + _gap) + (f32)_fields * (_fh + _gap) + (_ch + _gap) + 44.0f + _pad;
    const f32 _kh = fmaxf(fminf(FUDE_ZOOM_LIMITSF_KEY, (_screen.y - 32.0f - _rest) / 4.0f - _gap), 34.0f);
    const f32 _h  = _rest + 4.0f * (_kh + _gap);
    _form->laid_out = _screen;
    fude_kit_modal_place(&_form->modal, _form->window, (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.5f }, (rde_vec_2F){ _w, _h });
    f32 _y = _h - _pad;
    fude_kit_place(rde_ui_label_as_node(_form->title), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 14.0f }, (rde_vec_2F){ _iw, 28.0f });
    _y -= 28.0f + _gap;
    fude_kit_place(rde_ui_label_as_node(_form->how), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 10.0f }, (rde_vec_2F){ _iw, 20.0f });
    _y -= 20.0f + _gap;
    // Its real parts: three a row.
    const f32 _pw = (_iw - 2.0f * _gap) / 3.0f;
    for(u32 _i = 0; _i < FUDE_ZOOM_LIMITS_PRESETS; _i++) {
        rde_ui_node_set_active(rde_ui_button_as_node(_form->presets[_i]), _i < _presets);
        if(_i < _presets) {
            fude_kit_place(rde_ui_button_as_node(_form->presets[_i]), (rde_vec_2F){ _pad + (f32)(_i % 3u) * (_pw + _gap) + _pw * 0.5f, _y - (f32)(_i / 3u) * (_ch + _gap) - _ch * 0.5f },
                           (rde_vec_2F){ _pw, _ch });
        }
    }
    _y -= (f32)_preset_rows * (_ch + _gap);
    // Its limits, a row each.
    for(u32 _k = 0; _k < FUDE_ZOOM_LIMIT_COUNT; _k++) {
        const b8 _has = (_form->kinds >> _k) & 1u;
        rde_ui_node_set_active(rde_ui_button_as_node(_form->fields[_k]), _has);
        if(_has) {
            fude_kit_place(rde_ui_button_as_node(_form->fields[_k]), (rde_vec_2F){ _pad + _iw * 0.5f, _y - _fh * 0.5f }, (rde_vec_2F){ _iw, _fh });
            _y -= _fh + _gap;
        }
    }
    // The units of the one being typed, back a digit beside them; the keys: 7 8 9 / 4 5 6 / 1 2 3 / . 0.
    const f32 _kw = (_iw - 2.0f * _gap) / 3.0f;
    for(u32 _u = 0; _u < 2u; _u++) {
        fude_kit_place(rde_ui_button_as_node(_form->units[_u]), (rde_vec_2F){ _pad + (f32)_u * (_kw + _gap) + _kw * 0.5f, _y - _ch * 0.5f }, (rde_vec_2F){ _kw, _ch });
    }
    fude_kit_place(rde_ui_button_as_node(_form->keys[11]), (rde_vec_2F){ _pad + 2.0f * (_kw + _gap) + _kw * 0.5f, _y - _ch * 0.5f }, (rde_vec_2F){ _kw, _ch });
    _y -= _ch + _gap;
    static const u32 _order[11] = { 7, 8, 9, 4, 5, 6, 1, 2, 3, 10, 0 };
    for(u32 _i = 0; _i < 11u; _i++) {
        const f32 _x = _pad + (f32)(_i % 3u) * (_kw + _gap) + _kw * 0.5f;
        const f32 _ky = _y - (f32)(_i / 3u) * (_kh + _gap) - _kh * 0.5f;
        fude_kit_place(rde_ui_button_as_node(_form->keys[_order[_i]]), (rde_vec_2F){ _x, _ky }, (rde_vec_2F){ _kw, _kh });
    }
    fude_kit_place(rde_ui_button_as_node(_form->restore), (rde_vec_2F){ _pad + 60.0f, _pad + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_form->apply), (rde_vec_2F){ _w - _pad - 60.0f, _pad + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_form->cancel), (rde_vec_2F){ _w - _pad - 120.0f - _gap - 60.0f, _pad + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
}

void fude_zoom_limits_open(fude_zoom_limits_form* _form, const fude_zoom_part* _part, const c8* _name, const f64* _most, i32 _preset,
                           const f64* _typical, i32 _typical_preset, fude_zoom_limits_done _done, void* _self) {
    if(_form->modal.card == NULL || _part == NULL) {
        return;
    }
    _form->table          = fude_zoom_limits_presets(_part, &_form->table_count);
    _form->kinds          = fude_zoom_limits_kinds(_part);
    _form->gate           = fude_zoom_limits_gate(_part);
    _form->preset         = _preset;
    _form->typical_preset = _typical_preset;
    memcpy(_form->most, _most, sizeof(_form->most));
    memcpy(_form->typical, _typical, sizeof(_form->typical));
    _form->done = _done;
    _form->self = _self;
    c8 _title[160];
    FUDE_TEXTF(_title, FUDE_TEXT_ZOOM_LIMITS_TITLE, FUDE_TS(_name));
    rde_ui_label_set_text(_form->title, _title);
    rde_ui_label_set_text(_form->how, fude_text(FUDE_TEXT_ZOOM_LIMITS_HOW));
    for(u32 _i = 0; _i < FUDE_ZOOM_LIMITS_PRESETS; _i++) {
        rde_ui_button_set_text(_form->presets[_i], _form->table != NULL && _i < _form->table_count ? _form->table[_i].name : "");
    }
    // (the first limit it has, to be typed)
    u32 _first = 0;
    while(_first + 1u < FUDE_ZOOM_LIMIT_COUNT && !((_form->kinds >> _first) & 1u)) {
        _first++;
    }
    fude_zoom_limitsf_pick(_form, _first);
    fude_kit_modal_show(&_form->modal, true);
    _form->opened_at = rde_engine_get_time_now();
    fude_zoom_limitsf_layout(_form);
    fude_zoom_limits_restyle(_form);
}

void fude_zoom_limits_close(fude_zoom_limits_form* _form) {
    if(_form->modal.card != NULL) {
        fude_kit_modal_show(&_form->modal, false);
    }
}

void fude_zoom_limits_update(fude_zoom_limits_form* _form) {
    if(!fude_zoom_limits_shown(_form)) {
        return;
    }
    const rde_vec_2F _screen = fude_kit_screen_size(_form->window);
    if(_screen.x != _form->laid_out.x || _screen.y != _form->laid_out.y) {
        fude_zoom_limitsf_layout(_form);
    }
}

void fude_zoom_limits_look(fude_zoom_limits_form* _form, i32 _preset, u32 _field, const c8* _typed, u32 _unit, u32 _finish) {
    if(!fude_zoom_limits_shown(_form)) {
        return;
    }
    if(_preset >= 0 && (u32)_preset < FUDE_ZOOM_LIMITS_PRESETS) {
        fude_zoom_limitsf_on_preset(NULL, NULL, &_form->preset_refs[_preset]);
    }
    if(_typed != NULL && _field < FUDE_ZOOM_LIMIT_COUNT) {
        fude_zoom_limitsf_pick(_form, _field);
        snprintf(_form->typed, sizeof(_form->typed), "%s", _typed);
        _form->fresh = false;
        _form->unit  = fude_zoom_limitsf_milli(_field) && _unit == 1u ? 1u : 0u;
        fude_zoom_limitsf_take(_form);
    }
    fude_zoom_limitsf_show(_form);
    if(_finish == 2u) {
        fude_zoom_limitsf_on_typical(NULL, NULL, _form);
    }
    if(_finish >= 1u) {
        fude_zoom_limitsf_on_apply(NULL, NULL, _form);
    }
}
