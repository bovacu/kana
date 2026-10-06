// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/sizeform.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/widgets/draw.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FUDE_ZOOM_SIZEF_PAD    16.0f
#define FUDE_ZOOM_SIZEF_GAP    8.0f
#define FUDE_ZOOM_SIZEF_ROW_H  44.0f
#define FUDE_ZOOM_SIZEF_FIELD_PX 18u

enum { FUDE_ZOOM_SIZEF_W = 0, FUDE_ZOOM_SIZEF_H, FUDE_ZOOM_SIZEF_ANGLE, FUDE_ZOOM_SIZEF_X, FUDE_ZOOM_SIZEF_Y };

void fude_zoom_size_area_format(f64 _mm2, const fude_zoom_units_style* _style, c8* _out, usize _size) {
    const FUDE_ZOOM_UNIT_ _u = _style->unit;
    f64       _per = 1.0;   // mm² one of the unit
    const c8* _name = "mm\xC2\xB2";
    if(_u == FUDE_ZOOM_UNIT_IN || _u == FUDE_ZOOM_UNIT_FT) {
        const b8 _feet = _u == FUDE_ZOOM_UNIT_FT || _mm2 >= 144.0 * 645.16;
        _per  = _feet ? 92903.04 : 645.16;
        _name = _feet ? "ft\xC2\xB2" : "in\xC2\xB2";
    } else if(_u == FUDE_ZOOM_UNIT_M || _mm2 >= 1e6) {
        _per  = 1e6;
        _name = "m\xC2\xB2";
    } else if(_u == FUDE_ZOOM_UNIT_CM) {
        _per  = 100.0;
        _name = "cm\xC2\xB2";
    }
    c8 _n[48];
    snprintf(_n, sizeof(_n), "%.2f", _mm2 / _per);
    c8* _dot = strchr(_n, '.');
    if(_dot != NULL) {
        c8* _end = _n + strlen(_n) - 1;
        while(_end > _dot && *_end == '0') { *_end-- = 0; }
        if(_end == _dot) { *_end = 0; }
    }
    snprintf(_out, _size, "%s %s", _n, _name);
}

// A field's text (a copy, "" when empty), into _out.
RDE_INTERNAL void fude_zoom_sizef_text(rde_ui_text_editor* _field, c8* _out, usize _size) {
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

RDE_INTERNAL void fude_zoom_sizef_set_text(fude_zoom_size_form* _form, rde_ui_text_editor* _field, const c8* _text) {
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

RDE_INTERNAL void fude_zoom_sizef_set_length(fude_zoom_size_form* _form, u32 _i, f64 _mm) {
    c8 _t[FUDE_ZOOM_UNITS_TEXT];
    fude_zoom_units_format(_mm, &_form->units, _t, sizeof(_t));
    fude_zoom_sizef_set_text(_form, _form->fields[_i], _t);
}

// Field _i read: a length (or, the angle's, degrees). False: it cannot be.
RDE_INTERNAL b8 fude_zoom_sizef_read(const fude_zoom_size_form* _form, u32 _i, f64* _out) {
    c8 _t[96];
    fude_zoom_sizef_text(_form->fields[_i], _t, sizeof(_t));
    fude_zoom_units_error _e;
    if(_i == FUDE_ZOOM_SIZEF_ANGLE) {
        return fude_zoom_units_angle(_t, NULL, _out, &_e) && isfinite(*_out);
    }
    return fude_zoom_units_length(_t, _form->units.unit, NULL, _out, &_e) && isfinite(*_out) && (_i >= FUDE_ZOOM_SIZEF_X || *_out > 0.0);
}

RDE_INTERNAL b8 fude_zoom_sizef_locked(const fude_zoom_size_form* _form) {
    return _form->was.locked || !_form->was.can_unlock;
}

RDE_INTERNAL void fude_zoom_sizef_show_lock(fude_zoom_size_form* _form) {
    fude_kit_button_chip(_form->lock, _form->was.locked);
}

// --- presses -------------------------------------------------------------------------------

// A size typed, its sides kept in proportion: the other side follows.
RDE_INTERNAL void fude_zoom_sizef_on_change(rde_ui_node* _node, any _user_data) {
    fude_zoom_size_form* _form = (fude_zoom_size_form*)_user_data;
    if(_form == NULL || _form->setting) {
        return;
    }
    rde_ui_label_set_text(_form->problem, "");
    if(!_form->was.has_h || !fude_zoom_sizef_locked(_form) || !(_form->was.w_mm > 0.0) || !(_form->was.h_mm > 0.0)) {
        return;
    }
    const b8 _w = _node == fude_kit_field_node(_form->fields[FUDE_ZOOM_SIZEF_W]) || _node == rde_ui_text_editor_as_node(_form->fields[FUDE_ZOOM_SIZEF_W]);
    const b8 _h = _node == fude_kit_field_node(_form->fields[FUDE_ZOOM_SIZEF_H]) || _node == rde_ui_text_editor_as_node(_form->fields[FUDE_ZOOM_SIZEF_H]);
    f64 _v = 0.0;
    if(_w && fude_zoom_sizef_read(_form, FUDE_ZOOM_SIZEF_W, &_v)) {
        fude_zoom_sizef_set_length(_form, FUDE_ZOOM_SIZEF_H, _v * _form->was.h_mm / _form->was.w_mm);
    } else if(_h && fude_zoom_sizef_read(_form, FUDE_ZOOM_SIZEF_H, &_v)) {
        fude_zoom_sizef_set_length(_form, FUDE_ZOOM_SIZEF_W, _v * _form->was.w_mm / _form->was.h_mm);
    }
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_sizef_on_lock(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_size_form* _form = (fude_zoom_size_form*)_user_data;
    _form->was.locked = !_form->was.locked;
    fude_zoom_sizef_show_lock(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_sizef_on_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_size_close((fude_zoom_size_form*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A tap off the card: put away — not the release of the press that opened it.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_sizef_on_dismiss(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_size_form* _form = (fude_zoom_size_form*)_user_data;
    if(rde_engine_get_time_now() - _form->opened_at >= 0.6) {
        fude_zoom_size_close(_form);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL b8 fude_zoom_sizef_shows(const fude_zoom_size_form* _form, u32 _i) {
    return _i == FUDE_ZOOM_SIZEF_H ? _form->was.has_h : (_i == FUDE_ZOOM_SIZEF_X || _i == FUDE_ZOOM_SIZEF_Y) ? _form->was.has_xy : true;
}

// Apply: every field shown read; one that cannot be keeps the card up, said.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_sizef_on_save(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_size_form*  _form  = (fude_zoom_size_form*)_user_data;
    fude_zoom_size_values _typed = _form->was;
    f64* const _into[FUDE_ZOOM_SIZE_FIELDS] = { &_typed.w_mm, &_typed.h_mm, &_typed.angle, &_typed.x_mm, &_typed.y_mm };
    for(u32 _i = 0; _i < FUDE_ZOOM_SIZE_FIELDS; _i++) {
        if(!fude_zoom_sizef_shows(_form, _i)) {
            continue;
        }
        if(!fude_zoom_sizef_read(_form, _i, _into[_i])) {
            c8 _t[96], _say[200];
            fude_zoom_sizef_text(_form->fields[_i], _t, sizeof(_t));
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_SIZE_BAD, FUDE_TS(_t[0] != 0 ? _t : "?"));
            rde_ui_label_set_text(_form->problem, _say);
            rde_ui_node_focus(rde_ui_text_editor_as_node(_form->fields[_i]));
            return RDE_UI_EVENT_RESULT_CONSUME;
        }
    }
    const fude_zoom_size_done _done = _form->done;
    void* const               _self = _form->self;
    fude_zoom_size_close(_form);
    if(_done != NULL) {
        _done(_self, &_typed);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Enter: on to the next field shown; in the last, Apply.
RDE_INTERNAL void fude_zoom_sizef_on_submit(rde_ui_node* _node, any _user_data) {
    fude_zoom_size_form* _form = (fude_zoom_size_form*)_user_data;
    if(_form == NULL) {
        return;
    }
    u32 _at = FUDE_ZOOM_SIZE_FIELDS;
    for(u32 _i = 0; _i < FUDE_ZOOM_SIZE_FIELDS; _i++) {
        if(_node == rde_ui_text_editor_as_node(_form->fields[_i])) {
            _at = _i;
        }
    }
    for(u32 _i = _at + 1u; _i < FUDE_ZOOM_SIZE_FIELDS; _i++) {
        if(fude_zoom_sizef_shows(_form, _i)) {
            rde_ui_node_focus(rde_ui_text_editor_as_node(_form->fields[_i]));
            return;
        }
    }
    fude_zoom_sizef_on_save(NULL, NULL, _form);
}

// --- built, laid out --------------------------------------------------------------------------

RDE_INTERNAL rde_ui_label* fude_zoom_sizef_label(fude_zoom_size_form* _form, rde_ui_node* _parent, const c8* _text, f32 _px) {
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

void fude_zoom_size_build(fude_zoom_size_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font) {
    memset(_form, 0, sizeof(*_form));
    _form->window = _window;
    _form->font   = _font;
    fude_kit_modal_create(&_form->modal, _root, fude_zoom_sizef_on_dismiss, _form);
    rde_ui_node* _card = rde_ui_image_as_node(_form->modal.card);
    _form->title = fude_zoom_sizef_label(_form, _card, "", 17.0f);
    for(u32 _i = 0; _i < FUDE_ZOOM_SIZE_FIELDS; _i++) {
        _form->names[_i] = fude_zoom_sizef_label(_form, _card, "", 15.0f);
        rde_ui_text_editor* _f = rde_ui_text_editor_create(_font, NULL);
        rde_ui_text_editor_set_multiline(_f, false);
        rde_ui_text_editor_set_allow_multicursor(_f, false);
        rde_ui_text_editor_set_font_size(_f, FUDE_ZOOM_SIZEF_FIELD_PX);
        rde_ui_text_editor_set_max_chars(_f, 32u);
        rde_ui_text_editor_set_content_insets(_f, 10.0f, 8.0f, 10.0f, 8.0f);
        rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_f), (any)_form);
        rde_ui_text_editor_set_on_change(_f, fude_zoom_sizef_on_change);
        rde_ui_text_editor_set_on_submit(_f, fude_zoom_sizef_on_submit);
        fude_kit_field_box(_card, _f);
        _form->fields[_i] = _f;
    }
    rde_ui_label_set_text(_form->names[FUDE_ZOOM_SIZEF_X], "X");
    rde_ui_label_set_text(_form->names[FUDE_ZOOM_SIZEF_Y], "Y");
    _form->lock    = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_SIZE_KEEP), fude_zoom_sizef_on_lock, _form);
    _form->from    = fude_zoom_sizef_label(_form, _card, fude_text(FUDE_TEXT_ZOOM_SIZE_FROM), 13.0f);
    _form->info    = fude_zoom_sizef_label(_form, _card, "", 14.0f);
    _form->problem = fude_zoom_sizef_label(_form, _card, "", 14.0f);
    _form->cancel  = fude_kit_button(_card, fude_text(FUDE_TEXT_CANCEL), fude_zoom_sizef_on_cancel, _form);
    _form->save    = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_SIZE_APPLY), fude_zoom_sizef_on_save, _form);
    fude_zoom_size_restyle(_form);
    fude_kit_modal_show(&_form->modal, false);
}

void fude_zoom_size_forget(fude_zoom_size_form* _form) {
    memset(_form, 0, sizeof(*_form));
}

b8 fude_zoom_size_shown(const fude_zoom_size_form* _form) {
    return _form->modal.card != NULL && _form->modal.shown;
}

void fude_zoom_size_restyle(fude_zoom_size_form* _form) {
    if(_form->modal.card == NULL) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    fude_kit_modal_restyle(&_form->modal, 16.0f);
    rde_ui_label_set_color(_form->title, _t->text);
    for(u32 _i = 0; _i < FUDE_ZOOM_SIZE_FIELDS; _i++) {
        rde_ui_label_set_color(_form->names[_i], _t->text_soft);
        fude_kit_style_field(_form->fields[_i]);
    }
    rde_ui_label_set_color(_form->from, _t->text_soft);
    rde_ui_label_set_color(_form->info, _t->text);
    rde_ui_label_set_color(_form->problem, _t->score_poor);
    fude_zoom_sizef_show_lock(_form);
    fude_kit_button_plain(_form->cancel);
    fude_kit_button_primary(_form->save);
}

// The card high on the screen (the keyboard comes up under it): a row of two fields each, a name before each.
RDE_INTERNAL void fude_zoom_sizef_layout(fude_zoom_size_form* _form) {
    const rde_vec_2F _screen = fude_kit_screen_size(_form->window);
    const f32 _pad = FUDE_ZOOM_SIZEF_PAD, _gap = FUDE_ZOOM_SIZEF_GAP, _rh = FUDE_ZOOM_SIZEF_ROW_H;
    const f32 _w  = fminf(560.0f, _screen.x - 32.0f);
    const f32 _iw = _w - 2.0f * _pad;
    const b8  _xy = _form->was.has_xy;
    const f32 _h  = _pad + 28.0f + _gap + 2.0f * (_rh + _gap) + (_xy ? _rh + _gap + 20.0f + _gap : 0.0f) + 22.0f + 22.0f + _gap + 44.0f + _pad;
    _form->laid_out = _screen;
    fude_kit_modal_place(&_form->modal, _form->window, (rde_vec_2F){ _screen.x * 0.5f, fmaxf(_screen.y - 90.0f - _h * 0.5f, _h * 0.5f + 8.0f) }, (rde_vec_2F){ _w, _h });
    const f32 _col = (_iw - 2.0f * _gap) * 0.5f;           // a column: a name and its field
    const f32 _nw  = fminf(84.0f, _col * 0.42f), _fw = _col - _nw - 4.0f;
    f32 _y = _h - _pad;
    fude_kit_place(rde_ui_label_as_node(_form->title), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 14.0f }, (rde_vec_2F){ _iw, 28.0f });
    _y -= 28.0f + _gap;
    // (width height / angle, the lock / x y): each field's column.
    const u32 _row_of[FUDE_ZOOM_SIZE_FIELDS] = { 0u, 0u, 1u, 2u, 2u };
    const u32 _col_of[FUDE_ZOOM_SIZE_FIELDS] = { 0u, 1u, 0u, 0u, 1u };
    for(u32 _i = 0; _i < FUDE_ZOOM_SIZE_FIELDS; _i++) {
        const b8 _on = fude_zoom_sizef_shows(_form, _i);
        rde_ui_node_set_active(rde_ui_label_as_node(_form->names[_i]), _on);
        rde_ui_node_set_active(fude_kit_field_node(_form->fields[_i]), _on);
        if(!_on) {
            continue;
        }
        const f32 _x  = _pad + (f32)_col_of[_i] * (_col + 2.0f * _gap);
        const f32 _ry = _y - (f32)_row_of[_i] * (_rh + _gap) - _rh * 0.5f;
        fude_kit_place(rde_ui_label_as_node(_form->names[_i]), (rde_vec_2F){ _x + _nw * 0.5f, _ry }, (rde_vec_2F){ _nw, _rh });
        fude_kit_place(fude_kit_field_node(_form->fields[_i]), (rde_vec_2F){ _x + _nw + 4.0f + _fw * 0.5f, _ry }, (rde_vec_2F){ _fw, _rh });
    }
    const b8 _lock = _form->was.has_h && _form->was.can_unlock;
    rde_ui_node_set_active(rde_ui_button_as_node(_form->lock), _lock);
    if(_lock) {
        fude_kit_place(rde_ui_button_as_node(_form->lock), (rde_vec_2F){ _pad + _col + 2.0f * _gap + _col * 0.5f, _y - (_rh + _gap) - _rh * 0.5f }, (rde_vec_2F){ _col, 38.0f });
    }
    _y -= 2.0f * (_rh + _gap);
    rde_ui_node_set_active(rde_ui_label_as_node(_form->from), _xy);
    if(_xy) {
        _y -= _rh + _gap;
        fude_kit_place(rde_ui_label_as_node(_form->from), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 10.0f }, (rde_vec_2F){ _iw, 20.0f });
        _y -= 20.0f + _gap;
    }
    fude_kit_place(rde_ui_label_as_node(_form->info), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 11.0f }, (rde_vec_2F){ _iw, 22.0f });
    _y -= 22.0f;
    fude_kit_place(rde_ui_label_as_node(_form->problem), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 11.0f }, (rde_vec_2F){ _iw, 22.0f });
    fude_kit_place(rde_ui_button_as_node(_form->cancel), (rde_vec_2F){ _w - _pad - 120.0f - _gap - 60.0f, _pad + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_form->save), (rde_vec_2F){ _w - _pad - 60.0f, _pad + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
}

void fude_zoom_size_open(fude_zoom_size_form* _form, const c8* _title, const fude_zoom_size_values* _now, const c8* _info,
                         const fude_zoom_units_style* _units, fude_zoom_size_done _done, void* _self) {
    if(_form->modal.card == NULL) {
        return;
    }
    _form->was   = *_now;
    _form->units = *_units;
    _form->done  = _done;
    _form->self  = _self;
    rde_ui_label_set_text(_form->title, _title);
    rde_ui_label_set_text(_form->names[FUDE_ZOOM_SIZEF_W], fude_text(_now->has_h ? FUDE_TEXT_ZOOM_PART_WIDTH : FUDE_TEXT_ZOOM_SIZE_LENGTH));
    rde_ui_label_set_text(_form->names[FUDE_ZOOM_SIZEF_H], fude_text(FUDE_TEXT_ZOOM_PART_HEIGHT));
    rde_ui_label_set_text(_form->names[FUDE_ZOOM_SIZEF_ANGLE], fude_text(_now->turn ? FUDE_TEXT_ZOOM_SIZE_TURN : FUDE_TEXT_ZOOM_SIZE_ANGLE));
    rde_ui_label_set_text(_form->info, _info != NULL ? _info : "");
    rde_ui_label_set_text(_form->problem, "");
    fude_zoom_sizef_set_length(_form, FUDE_ZOOM_SIZEF_W, _now->w_mm);
    fude_zoom_sizef_set_length(_form, FUDE_ZOOM_SIZEF_H, _now->h_mm);
    fude_zoom_sizef_set_length(_form, FUDE_ZOOM_SIZEF_X, _now->x_mm);
    fude_zoom_sizef_set_length(_form, FUDE_ZOOM_SIZEF_Y, _now->y_mm);
    c8 _deg[FUDE_ZOOM_UNITS_TEXT];
    fude_zoom_units_format_angle(_now->turn ? 0.0 : _now->angle, 2u, _deg, sizeof(_deg));
    fude_zoom_sizef_set_text(_form, _form->fields[FUDE_ZOOM_SIZEF_ANGLE], _deg);
    fude_kit_modal_show(&_form->modal, true);
    _form->opened_at = rde_engine_get_time_now();
    fude_zoom_sizef_layout(_form);
    fude_zoom_size_restyle(_form);   // (the keyboard stays down: a field tapped brings it)
}

void fude_zoom_size_close(fude_zoom_size_form* _form) {
    if(_form->modal.card != NULL) {
        fude_kit_modal_show(&_form->modal, false);
    }
}

void fude_zoom_size_update(fude_zoom_size_form* _form) {
    if(!fude_zoom_size_shown(_form)) {
        return;
    }
    const rde_vec_2F _screen = fude_kit_screen_size(_form->window);
    if(_screen.x != _form->laid_out.x || _screen.y != _form->laid_out.y) {
        fude_zoom_sizef_layout(_form);
    }
}

void fude_zoom_size_look(fude_zoom_size_form* _form, const c8* const _typed[FUDE_ZOOM_SIZE_FIELDS], i32 _locked, b8 _apply) {
    if(_locked >= 0) {
        _form->was.locked = _locked != 0;
        fude_zoom_sizef_show_lock(_form);
    }
    for(u32 _i = 0; _i < FUDE_ZOOM_SIZE_FIELDS; _i++) {
        if(_typed != NULL && _typed[_i] != NULL) {
            fude_zoom_sizef_set_text(_form, _form->fields[_i], _typed[_i]);
            fude_zoom_sizef_on_change(rde_ui_text_editor_as_node(_form->fields[_i]), _form);
        }
    }
    if(_apply) {
        fude_zoom_sizef_on_save(NULL, NULL, _form);
    }
}
