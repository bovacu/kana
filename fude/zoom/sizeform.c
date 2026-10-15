// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/sizeform.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/base/draw.h"
#include "drawing/widgets/kit.h"

// ===========================================================================
// See sizeform.h.
//
// The card is the kit's form (kit.h: its title, Cancel, a tap off it, laying it out again, its fields and what could
// not be read). It is built once, hidden, and filled and shown when opened; it is laid out again on opening and when
// the screen's size changes. The values it opened with (was) are kept while it is up: they say which fields show, and
// the proportions a kept-in-proportion side follows. Its own names start with fude_zoom_sizef_ and FUDE_ZOOM_SIZEF_
// (the header's are fude_zoom_size_). The file holds the area's format first, then the presses, then the
// card built and laid out.
// ===========================================================================

// The card's measures, in screen points: a row's height (its margin and gaps are the kit's form's).
#define FUDE_ZOOM_SIZEF_ROW_H  44.0f

// The fields, in the order of fields[] and names[] (and of fude_zoom_size_look's _typed).
enum { FUDE_ZOOM_SIZEF_W = 0, FUDE_ZOOM_SIZEF_H, FUDE_ZOOM_SIZEF_ANGLE, FUDE_ZOOM_SIZEF_X, FUDE_ZOOM_SIZEF_Y };

// See the header. The number is written with two decimals, then its trailing zeros (and a point left bare) are taken
// off.
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
    rde_snprintf(_n, sizeof(_n), "%.2f", _mm2 / _per);
    c8* _dot = rde_strchr(_n, '.');
    if(_dot != NULL) {
        c8* _end = _n + rde_strlen(_n) - 1;
        while(_end > _dot && *_end == '0') { *_end-- = 0; }
        if(_end == _dot) { *_end = 0; }
    }
    rde_snprintf(_out, _size, "%s %s", _n, _name);
}

// Writes a length of _mm millimetres into field _i, in the canvas's units, as the card's own filling in (kit.h:
// fude_kit_form_set), which fude_zoom_sizef_on_change does not take for typing.
RDE_INTERNAL void fude_zoom_sizef_set_length(fude_zoom_size_form* _form, u32 _i, f64 _mm) {
    c8 _t[FUDE_ZOOM_UNITS_TEXT];
    fude_zoom_units_format(_mm, &_form->units, _t, sizeof(_t));
    fude_kit_form_set(&_form->card, _form->fields[_i], _t);
}

// Reads field _i into *_out: a length in millimetres, read as the numpad reads one (units.h), or for the angle field
// degrees. Returns false when it cannot be read, is not finite, or is a width, height or length not above 0 (X and Y
// may be anything).
RDE_INTERNAL b8 fude_zoom_sizef_read(const fude_zoom_size_form* _form, u32 _i, f64* _out) {
    c8 _t[96];
    fude_kit_field_text(_form->fields[_i], _t, sizeof(_t));
    fude_zoom_units_error _e;
    if(_i == FUDE_ZOOM_SIZEF_ANGLE) {
        return fude_zoom_units_angle(_t, NULL, _out, &_e) && rde_math_is_finite_f64(*_out);
    }
    return fude_zoom_units_length(_t, _form->units.unit, NULL, _out, &_e) && rde_math_is_finite_f64(*_out) && (_i >= FUDE_ZOOM_SIZEF_X || *_out > 0.0);
}

// Returns whether the sides are kept in proportion: the lock is on, or the thing can only be scaled whole.
RDE_INTERNAL b8 fude_zoom_sizef_locked(const fude_zoom_size_form* _form) {
    return _form->was.locked || !_form->was.can_unlock;
}

// Shows the lock's chip as chosen when the sides are kept in proportion.
RDE_INTERNAL void fude_zoom_sizef_show_lock(fude_zoom_size_form* _form) {
    fude_kit_button_chip(_form->lock, _form->was.locked);
}

// --- presses -------------------------------------------------------------------------------

// A field's text changed. Typing clears the problem line; with the sides kept in proportion, a width or height that
// can be read sets the other side to match, in the proportions the card opened with.
RDE_INTERNAL void fude_zoom_sizef_on_change(rde_ui_node* _node, any _user_data) {
    fude_zoom_size_form* _form = (fude_zoom_size_form*)_user_data;
    if(_form == NULL || _form->card.setting) {
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

// Keep proportions tapped: the lock is turned on or off, in was, so Apply hands it back.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_sizef_on_lock(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_size_form* _form = (fude_zoom_size_form*)_user_data;
    _form->was.locked = !_form->was.locked;
    fude_zoom_sizef_show_lock(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Returns whether field _i shows: the height only for what has one, X and Y only on a measured sheet.
RDE_INTERNAL b8 fude_zoom_sizef_shows(const fude_zoom_size_form* _form, u32 _i) {
    return _i == FUDE_ZOOM_SIZEF_H ? _form->was.has_h : (_i == FUDE_ZOOM_SIZEF_X || _i == FUDE_ZOOM_SIZEF_Y) ? _form->was.has_xy : true;
}

// Apply: reads every field shown into a copy of the values the card opened with. A field that cannot be read keeps
// the card up, says which on the problem line, and takes the focus; when all can be, the card is put away and the
// opener's callback gets the values.
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
            fude_kit_form_bad(_form->problem, _form->fields[_i], FUDE_TEXT_ZOOM_SIZE_BAD);
            return RDE_UI_EVENT_RESULT_CONSUME;
        }
    }
    const fude_zoom_size_done _done = _form->done;
    any const               _self = _form->self;
    fude_zoom_size_close(_form);
    if(_done != NULL) {
        _done(_self, &_typed);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Enter in a field: the focus goes on to the next field shown; in the last one, Apply.
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

// See the header. Each field is a one-line text editor of at most 32 characters, its node's user data the form.
void fude_zoom_size_build(fude_zoom_size_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font) {
    rde_memset(_form, 0, sizeof(*_form));
    rde_ui_node* _card = fude_kit_form_build(&_form->card, _root, _window, _font, "");
    for(u32 _i = 0; _i < FUDE_ZOOM_SIZE_FIELDS; _i++) {
        _form->names[_i]  = fude_kit_form_label(&_form->card, _card, "", 15.0f);
        _form->fields[_i] = fude_kit_form_field(&_form->card, _card, 32u, NULL, (any)_form, fude_zoom_sizef_on_change, fude_zoom_sizef_on_submit);
    }
    rde_ui_label_set_text(_form->names[FUDE_ZOOM_SIZEF_X], "X");
    rde_ui_label_set_text(_form->names[FUDE_ZOOM_SIZEF_Y], "Y");
    _form->lock    = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_SIZE_KEEP), fude_zoom_sizef_on_lock, _form);
    _form->from    = fude_kit_form_label(&_form->card, _card, fude_text(FUDE_TEXT_ZOOM_SIZE_FROM), 13.0f);
    _form->info    = fude_kit_form_label(&_form->card, _card, "", 14.0f);
    _form->problem = fude_kit_form_label(&_form->card, _card, "", 14.0f);
    _form->cancel  = fude_kit_button(_card, fude_text(FUDE_TEXT_CANCEL), fude_kit_form_on_cancel, &_form->card);
    _form->save    = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_SIZE_APPLY), fude_zoom_sizef_on_save, _form);
    fude_zoom_size_restyle(_form);
    fude_kit_modal_show(&_form->card.modal, false);
}

// See the header. With the modal's card NULL, the calls that check it do nothing.
void fude_zoom_size_forget(fude_zoom_size_form* _form) {
    rde_memset(_form, 0, sizeof(*_form));
}

// See the header.
b8 fude_zoom_size_shown(const fude_zoom_size_form* _form) {
    return fude_kit_form_shown(&_form->card);
}

// See the header.
void fude_zoom_size_restyle(fude_zoom_size_form* _form) {
    if(!fude_kit_form_restyle(&_form->card)) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
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

// Places the card high on the screen, at most 560 points wide, so the keyboard comes up under it. From the top down:
// the title; two fields a row, a name before each (width and height, then the angle with the lock beside it, then X
// and Y on a sheet, with what they count from under them); the info line and the problem line; Cancel and Apply at
// the bottom right. Remembers the screen size, for fude_zoom_size_update.
RDE_INTERNAL void fude_zoom_sizef_layout(fude_zoom_size_form* _form) {
    const rde_vec_2F _screen = fude_kit_screen_size(_form->card.window);
    const f32 _pad = FUDE_KIT_FORM_PAD, _gap = FUDE_KIT_FORM_GAP, _rh = FUDE_ZOOM_SIZEF_ROW_H;
    const f32 _w  = rde_math_min_f32(560.0f, _screen.x - 32.0f);
    const f32 _iw = _w - 2.0f * _pad;
    const b8  _xy = _form->was.has_xy;
    const f32 _h  = _pad + 28.0f + _gap + 2.0f * (_rh + _gap) + (_xy ? _rh + _gap + 20.0f + _gap : 0.0f) + 22.0f + 22.0f + _gap + 44.0f + _pad;
    f32 _y = fude_kit_form_place(&_form->card, _screen, FUDE_KIT_FORM_HIGH_KEPT, _w, _h);
    const f32 _col = (_iw - 2.0f * _gap) * 0.5f;           // a column: a name and its field
    const f32 _nw  = rde_math_min_f32(84.0f, _col * 0.42f), _fw = _col - _nw - 4.0f;
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
        fude_kit_form_named(_form->names[_i], _form->fields[_i], _x, _ry, _nw, _fw, _rh);
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
    fude_kit_form_buttons(_form->cancel, _form->save, _w);
}

// See the header. Every field is filled, the hidden ones too; a turn's angle field starts at 0.
void fude_zoom_size_open(fude_zoom_size_form* _form, const c8* _title, const fude_zoom_size_values* _now, const c8* _info,
                         const fude_zoom_units_style* _units, fude_zoom_size_done _done, any _self) {
    if(_form->card.modal.card == NULL) {
        return;
    }
    _form->was   = *_now;
    _form->units = *_units;
    _form->done  = _done;
    _form->self  = _self;
    rde_ui_label_set_text(_form->card.title, _title);
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
    fude_kit_form_set(&_form->card, _form->fields[FUDE_ZOOM_SIZEF_ANGLE], _deg);
    fude_kit_form_show(&_form->card);
    fude_zoom_sizef_layout(_form);
    fude_zoom_size_restyle(_form);   // (the keyboard stays down: a field tapped brings it)
}

// See the header.
void fude_zoom_size_close(fude_zoom_size_form* _form) {
    fude_kit_form_close(&_form->card);
}

// See the header.
void fude_zoom_size_update(fude_zoom_size_form* _form) {
    if(fude_kit_form_resized(&_form->card)) {
        fude_zoom_sizef_layout(_form);
    }
}

// See the header. Each field typed goes through fude_zoom_sizef_on_change, as typing does, so a side kept in
// proportion follows it. It does nothing while the card is not up, as the other cards' look calls do.
void fude_zoom_size_look(fude_zoom_size_form* _form, const c8* const _typed[FUDE_ZOOM_SIZE_FIELDS], i32 _locked, b8 _apply) {
    if(!fude_zoom_size_shown(_form)) {
        return;
    }
    if(_locked >= 0) {
        _form->was.locked = _locked != 0;
        fude_zoom_sizef_show_lock(_form);
    }
    for(u32 _i = 0; _i < FUDE_ZOOM_SIZE_FIELDS; _i++) {
        if(_typed != NULL && _typed[_i] != NULL) {
            fude_kit_form_set(&_form->card, _form->fields[_i], _typed[_i]);
            fude_zoom_sizef_on_change(rde_ui_text_editor_as_node(_form->fields[_i]), _form);
        }
    }
    if(_apply) {
        fude_zoom_sizef_on_save(NULL, NULL, _form);
    }
}
