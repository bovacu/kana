// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/partsform.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/widgets/icons.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FUDE_ZOOM_PARTS_PAD    16.0f
#define FUDE_ZOOM_PARTS_GAP    8.0f
#define FUDE_ZOOM_PARTS_ROW_H  44.0f
#define FUDE_ZOOM_PARTS_FIELD_PX 18u

static const c8* const FUDE_ZOOM_PARTS_UNIT_NAMES[4] = { "mm", "cm", "in", "ft \xC2\xB7 in" };

b8 fude_zoom_parts_read(const c8* _text, u8 _unit_choice, f64* _mm) {
    static const FUDE_ZOOM_UNIT_ _units[4] = { FUDE_ZOOM_UNIT_MM, FUDE_ZOOM_UNIT_CM, FUDE_ZOOM_UNIT_IN, FUDE_ZOOM_UNIT_IN };
    fude_zoom_units_error _e;
    return fude_zoom_units_length(_text, _units[_unit_choice < 4u ? _unit_choice : 0u], NULL, _mm, &_e) && *_mm > 0.0;
}

// A field's text (a copy, "" when empty), into _out.
RDE_INTERNAL void fude_zoom_parts_text(rde_ui_text_editor* _field, c8* _out, usize _size) {
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

RDE_INTERNAL void fude_zoom_parts_set_text(rde_ui_text_editor* _field, const c8* _text) {
    const usize _bytes = rde_ui_text_editor_get_byte_count(_field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_field, 0, _bytes);
    }
    if(_text[0] != 0) {
        rde_ui_text_editor_insert_at(_field, 0, _text, strlen(_text));
    }
}

RDE_INTERNAL b8 fude_zoom_parts_blank(const c8* _t) {
    for(; *_t != 0; _t++) {
        if(*_t != ' ' && *_t != '\t') {
            return false;
        }
    }
    return true;
}

RDE_INTERNAL void fude_zoom_parts_layout(fude_zoom_parts_form* _form);

RDE_INTERNAL void fude_zoom_parts_show_units(fude_zoom_parts_form* _form) {
    for(u32 _i = 0; _i < 4u; _i++) {
        if(_i == _form->unit_choice) { fude_kit_button_selected(_form->units[_i]); }
        else                         { fude_kit_button_quiet(_form->units[_i]); }
    }
}

// --- presses -------------------------------------------------------------------------------

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_parts_on_unit(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_parts_ref* _ref = (const fude_zoom_parts_ref*)_user_data;
    _ref->form->unit_choice = (u8)_ref->index;
    fude_zoom_parts_show_units(_ref->form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A row's trash: the rows under it move up a place (the last one only emptied when it is alone).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_parts_on_drop(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_parts_ref* _ref  = (const fude_zoom_parts_ref*)_user_data;
    fude_zoom_parts_form*      _form = _ref->form;
    if(_ref->index >= _form->rows) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    c8 _w[64], _h[64];
    for(u32 _i = _ref->index; _i + 1u < _form->rows; _i++) {
        fude_zoom_parts_text(_form->widths[_i + 1u], _w, sizeof(_w));
        fude_zoom_parts_text(_form->heights[_i + 1u], _h, sizeof(_h));
        fude_zoom_parts_set_text(_form->widths[_i], _w);
        fude_zoom_parts_set_text(_form->heights[_i], _h);
    }
    fude_zoom_parts_set_text(_form->widths[_form->rows - 1u], "");
    fude_zoom_parts_set_text(_form->heights[_form->rows - 1u], "");
    if(_form->rows > 1u) {
        _form->rows--;
    }
    rde_ui_label_set_text(_form->problem, "");
    fude_zoom_parts_layout(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_parts_on_add(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_parts_form* _form = (fude_zoom_parts_form*)_user_data;
    if(_form->rows >= FUDE_ZOOM_PARTS_ROWS) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    const u32 _row = _form->rows++;
    fude_zoom_parts_set_text(_form->widths[_row], "");
    fude_zoom_parts_set_text(_form->heights[_row], "");
    fude_zoom_parts_layout(_form);
    // The new row in sight (the list's end), its width to type.
    rde_ui_scroll_area_set_scroll(_form->list, (rde_vec_2F){ 0.0f, 1e9f });
    rde_ui_node_focus(rde_ui_text_editor_as_node(_form->widths[_row]));
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_parts_on_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_parts_close((fude_zoom_parts_form*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A tap off the card: put away — not the release of the press that opened it.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_parts_on_dismiss(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_parts_form* _form = (fude_zoom_parts_form*)_user_data;
    if(rde_engine_get_time_now() - _form->opened_at >= 0.6) {
        fude_zoom_parts_close(_form);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Cut: every row read (empty ones skipped); one that cannot be read keeps the card up, said.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_parts_on_save(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_parts_form* _form = (fude_zoom_parts_form*)_user_data;
    fude_zoom_parts_size  _parts[FUDE_ZOOM_PARTS_ROWS];
    u32                   _n = 0;
    for(u32 _i = 0; _i < _form->rows; _i++) {
        c8 _w[64], _h[64];
        fude_zoom_parts_text(_form->widths[_i], _w, sizeof(_w));
        fude_zoom_parts_text(_form->heights[_i], _h, sizeof(_h));
        if(fude_zoom_parts_blank(_w) && fude_zoom_parts_blank(_h)) {
            continue;
        }
        f64 _wm = 0.0, _hm = 0.0;
        const b8 _w_ok = fude_zoom_parts_read(_w, _form->unit_choice, &_wm);
        const b8 _h_ok = fude_zoom_parts_read(_h, _form->unit_choice, &_hm);
        if(!_w_ok || !_h_ok) {
            c8 _say[200];
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_PART_BAD, FUDE_TN(_i + 1u), FUDE_TS(!_w_ok ? (fude_zoom_parts_blank(_w) ? "?" : _w) : (fude_zoom_parts_blank(_h) ? "?" : _h)));
            rde_ui_label_set_text(_form->problem, _say);
            rde_ui_node_focus(rde_ui_text_editor_as_node(!_w_ok ? _form->widths[_i] : _form->heights[_i]));
            return RDE_UI_EVENT_RESULT_CONSUME;
        }
        _parts[_n++] = (fude_zoom_parts_size){ _wm, _hm };
    }
    if(_n == 0) {
        rde_ui_label_set_text(_form->problem, fude_text(FUDE_TEXT_ZOOM_PART_NONE));
        rde_ui_node_focus(rde_ui_text_editor_as_node(_form->widths[0]));
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    const fude_zoom_parts_done _done = _form->done;
    void* const                _self = _form->self;
    fude_zoom_parts_close(_form);
    if(_done != NULL) {
        _done(_self, _parts, _n);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Enter in a width: on to its height; in a height: on to the next row's width (a new one at the end).
RDE_INTERNAL void fude_zoom_parts_on_submit(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    const fude_zoom_parts_ref* _ref  = (const fude_zoom_parts_ref*)_user_data;
    fude_zoom_parts_form*      _form = _ref->form;
    const u32 _row = _ref->index / 2u;
    if(_ref->index % 2u == 0u) {
        rde_ui_node_focus(rde_ui_text_editor_as_node(_form->heights[_row]));
    } else if(_row + 1u < _form->rows) {
        rde_ui_node_focus(rde_ui_text_editor_as_node(_form->widths[_row + 1u]));
    } else {
        fude_zoom_parts_on_add(NULL, NULL, _form);
    }
}

// --- built, laid out --------------------------------------------------------------------------

RDE_INTERNAL rde_ui_label* fude_zoom_parts_label(fude_zoom_parts_form* _form, rde_ui_node* _parent, const c8* _text, f32 _px) {
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

void fude_zoom_parts_build(fude_zoom_parts_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font) {
    memset(_form, 0, sizeof(*_form));
    _form->window = _window;
    _form->font   = _font;
    fude_kit_modal_create(&_form->modal, _root, fude_zoom_parts_on_dismiss, _form);
    rde_ui_node* _card = rde_ui_image_as_node(_form->modal.card);
    _form->title       = fude_zoom_parts_label(_form, _card, "", 17.0f);
    _form->units_label = fude_zoom_parts_label(_form, _card, fude_text(FUDE_TEXT_ZOOM_UNITS), 15.0f);
    for(u32 _i = 0; _i < 4u; _i++) {
        _form->refs[_i] = (fude_zoom_parts_ref){ _form, _i };   // (the units' refs: the rows' are made below, after)
    }
    static fude_zoom_parts_ref _unit_refs_store[4];   // (one form at a time in an app)
    for(u32 _i = 0; _i < 4u; _i++) {
        _unit_refs_store[_i] = (fude_zoom_parts_ref){ _form, _i };
        _form->units[_i] = fude_kit_button(_card, FUDE_ZOOM_PARTS_UNIT_NAMES[_i], fude_zoom_parts_on_unit, &_unit_refs_store[_i]);
    }
    _form->list = rde_ui_scroll_area_create(NULL);
    rde_ui_scroll_area_set_bar_thickness(_form->list, 3.0f);
    rde_ui_scroll_area_set_background_color(_form->list, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_track_color(_form->list, (rde_color){ 0, 0, 0, 0 });
    // The card is above the keyboard already: no room kept under the rows for it (found on the tablet: that room
    // pushed the rows out of sight while the keyboard was up, back again once it went down).
    rde_ui_scroll_area_set_keyboard_inset_enabled(_form->list, false);
    rde_ui_node* _list = rde_ui_scroll_area_as_node(_form->list);
    rde_ui_node_add_child(_card, _list);
    static fude_zoom_parts_ref _field_refs[2u * FUDE_ZOOM_PARTS_ROWS];
    for(u32 _i = 0; _i < FUDE_ZOOM_PARTS_ROWS; _i++) {
        _form->refs[_i] = (fude_zoom_parts_ref){ _form, _i };
        for(u32 _k = 0; _k < 2u; _k++) {
            rde_ui_text_editor* _f = rde_ui_text_editor_create(_font, NULL);
            rde_ui_text_editor_set_multiline(_f, false);
            rde_ui_text_editor_set_allow_multicursor(_f, false);
            rde_ui_text_editor_set_font_size(_f, FUDE_ZOOM_PARTS_FIELD_PX);
            rde_ui_text_editor_set_max_chars(_f, 24u);
            rde_ui_text_editor_set_content_insets(_f, 10.0f, 8.0f, 10.0f, 8.0f);
            rde_ui_text_editor_set_placeholder(_f, fude_text(_k == 0u ? FUDE_TEXT_ZOOM_PART_WIDTH : FUDE_TEXT_ZOOM_PART_HEIGHT));
            _field_refs[2u * _i + _k] = (fude_zoom_parts_ref){ _form, 2u * _i + _k };
            rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_f), (any)&_field_refs[2u * _i + _k]);
            rde_ui_text_editor_set_on_submit(_f, fude_zoom_parts_on_submit);
            fude_kit_field_box(_list, _f);
            if(_k == 0u) { _form->widths[_i] = _f; } else { _form->heights[_i] = _f; }
        }
        _form->times[_i] = fude_zoom_parts_label(_form, _list, "\xC3\x97", 18.0f);
        rde_ui_label_set_alignment(_form->times[_i], RDE_UI_LABEL_H_ALIGN_CENTER, RDE_UI_LABEL_V_ALIGN_MIDDLE);
        _form->drops[_i] = fude_kit_button(_list, fude_text(FUDE_TEXT_ZOOM_PART_DROP), fude_zoom_parts_on_drop, &_form->refs[_i]);
        fude_kit_icon(_form->drops[_i], FUDE_ICON_TRASH, FUDE_KIT_ICON_ONLY, 16.0f);
    }
    c8 _with_icon[160];
    snprintf(_with_icon, sizeof(_with_icon), FUDE_ICON_PLUS "  %s", fude_text(FUDE_TEXT_ZOOM_PART_ADD));
    _form->add     = fude_kit_button(_card, _with_icon, fude_zoom_parts_on_add, _form);
    _form->kerf    = fude_zoom_parts_label(_form, _card, "", 14.0f);
    _form->problem = fude_zoom_parts_label(_form, _card, "", 14.0f);
    _form->cancel  = fude_kit_button(_card, fude_text(FUDE_TEXT_CANCEL), fude_zoom_parts_on_cancel, _form);
    _form->save    = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_PART_CUT), fude_zoom_parts_on_save, _form);
    fude_zoom_parts_restyle(_form);
    fude_kit_modal_show(&_form->modal, false);
}

void fude_zoom_parts_forget(fude_zoom_parts_form* _form) {
    memset(_form, 0, sizeof(*_form));
}

b8 fude_zoom_parts_shown(const fude_zoom_parts_form* _form) {
    return _form->modal.card != NULL && _form->modal.shown;
}

void fude_zoom_parts_restyle(fude_zoom_parts_form* _form) {
    if(_form->modal.card == NULL) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    fude_kit_modal_restyle(&_form->modal, 16.0f);
    rde_ui_label* const _labels[4] = { _form->title, _form->units_label, _form->kerf, _form->problem };
    for(u32 _i = 0; _i < 4u; _i++) {
        rde_ui_label_set_color(_labels[_i], _i == 3u ? _t->score_poor : (_i == 0u ? _t->text : _t->text_soft));
    }
    for(u32 _i = 0; _i < FUDE_ZOOM_PARTS_ROWS; _i++) {
        fude_kit_style_field(_form->widths[_i]);
        fude_kit_style_field(_form->heights[_i]);
        rde_ui_label_set_color(_form->times[_i], _t->text_soft);
        fude_kit_button_quiet(_form->drops[_i]);
        fude_kit_button_round(_form->drops[_i], 10.0f);
    }
    fude_zoom_parts_show_units(_form);
    for(u32 _i = 0; _i < 4u; _i++) {
        fude_kit_button_round(_form->units[_i], 10.0f);
    }
    fude_kit_button_colors(_form->add, _t->surface, 1.0f, _t->outline);
    fude_kit_button_round(_form->add, 10.0f);
    fude_kit_button_plain(_form->cancel);
    fude_kit_button_primary(_form->save);
}

// The card high on the screen (the keyboard comes up under it): its list as tall
// as its rows, up to what leaves room for the rest above the keyboard.
RDE_INTERNAL void fude_zoom_parts_layout(fude_zoom_parts_form* _form) {
    const rde_vec_2F _screen = fude_kit_screen_size(_form->window);
    const f32 _pad = FUDE_ZOOM_PARTS_PAD, _gap = FUDE_ZOOM_PARTS_GAP, _rh = FUDE_ZOOM_PARTS_ROW_H;
    const f32 _w     = fminf(560.0f, _screen.x - 32.0f);
    const f32 _fixed = _pad + 28.0f + _gap + 40.0f + _gap + _gap + 40.0f + _gap + 22.0f + 22.0f + _gap + 44.0f + _pad;
    const f32 _room  = fmaxf(_screen.y * 0.55f - 90.0f - _fixed, _rh + _gap);   // over a keyboard about as tall as half the screen
    const f32 _all   = (f32)_form->rows * (_rh + _gap) - _gap;
    const f32 _lh    = fminf(_all, _room);
    const f32 _h     = _fixed + _lh;
    _form->laid_out = _screen;
    fude_kit_modal_place(&_form->modal, _form->window, (rde_vec_2F){ _screen.x * 0.5f, _screen.y - 90.0f - _h * 0.5f }, (rde_vec_2F){ _w, _h });
    // Top to bottom (card-local: the bottom left the origin).
    const f32 _iw = _w - 2.0f * _pad;
    f32 _y = _h - _pad - 14.0f;
    fude_kit_place(rde_ui_label_as_node(_form->title), (rde_vec_2F){ _pad + _iw * 0.5f, _y }, (rde_vec_2F){ _iw, 28.0f });
    _y -= 14.0f + _gap + 20.0f;
    const f32 _uw = 70.0f, _ul = fminf(110.0f, _iw - 4.0f * (_uw + 6.0f) - 74.0f + 70.0f);
    fude_kit_place(rde_ui_label_as_node(_form->units_label), (rde_vec_2F){ _pad + _ul * 0.5f, _y }, (rde_vec_2F){ _ul, 40.0f });
    for(u32 _i = 0; _i < 4u; _i++) {
        const f32 _bw = _i == 3u ? 84.0f : _uw;
        const f32 _x  = _pad + _ul + 6.0f + (f32)_i * (_uw + 6.0f) + _bw * 0.5f;
        fude_kit_place(rde_ui_button_as_node(_form->units[_i]), (rde_vec_2F){ _x, _y }, (rde_vec_2F){ _bw, 40.0f });
    }
    _y -= 20.0f + _gap;
    // The list: its rows from the top of its content.
    fude_kit_place(rde_ui_scroll_area_as_node(_form->list), (rde_vec_2F){ _pad + _iw * 0.5f, _y - _lh * 0.5f }, (rde_vec_2F){ _iw, _lh });
    rde_ui_scroll_area_set_content_size(_form->list, (rde_vec_2F){ _iw, _all });
    const f32 _dw = 44.0f, _xw = 28.0f;
    const f32 _fw = (_iw - _dw - _xw - 3.0f * _gap) * 0.5f;
    for(u32 _i = 0; _i < FUDE_ZOOM_PARTS_ROWS; _i++) {
        const b8  _on = _i < _form->rows;
        const f32 _ry = _all - (f32)_i * (_rh + _gap) - _rh * 0.5f;
        rde_ui_node_set_active(fude_kit_field_node(_form->widths[_i]), _on);
        rde_ui_node_set_active(fude_kit_field_node(_form->heights[_i]), _on);
        rde_ui_node_set_active(rde_ui_label_as_node(_form->times[_i]), _on);
        rde_ui_node_set_active(rde_ui_button_as_node(_form->drops[_i]), _on);
        if(!_on) {
            continue;
        }
        fude_kit_place(fude_kit_field_node(_form->widths[_i]), (rde_vec_2F){ _fw * 0.5f, _ry }, (rde_vec_2F){ _fw, _rh });
        fude_kit_place(rde_ui_label_as_node(_form->times[_i]), (rde_vec_2F){ _fw + _gap + _xw * 0.5f, _ry }, (rde_vec_2F){ _xw, _rh });
        fude_kit_place(fude_kit_field_node(_form->heights[_i]), (rde_vec_2F){ _fw + 2.0f * _gap + _xw + _fw * 0.5f, _ry }, (rde_vec_2F){ _fw, _rh });
        fude_kit_place(rde_ui_button_as_node(_form->drops[_i]), (rde_vec_2F){ _iw - _dw * 0.5f, _ry }, (rde_vec_2F){ _dw, _rh });
    }
    _y -= _lh + _gap;
    fude_kit_place(rde_ui_button_as_node(_form->add), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 20.0f }, (rde_vec_2F){ _iw, 40.0f });
    _y -= 40.0f + _gap;
    fude_kit_place(rde_ui_label_as_node(_form->kerf), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 11.0f }, (rde_vec_2F){ _iw, 22.0f });
    _y -= 22.0f;
    fude_kit_place(rde_ui_label_as_node(_form->problem), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 11.0f }, (rde_vec_2F){ _iw, 22.0f });
    fude_kit_place(rde_ui_button_as_node(_form->cancel), (rde_vec_2F){ _w - _pad - 120.0f - _gap - 60.0f, _pad + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_form->save), (rde_vec_2F){ _w - _pad - 60.0f, _pad + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
}

void fude_zoom_parts_open(fude_zoom_parts_form* _form, const c8* _title, const c8* _kerf, u8 _unit_choice, fude_zoom_parts_done _done, void* _self) {
    if(_form->modal.card == NULL) {
        return;
    }
    _form->done        = _done;
    _form->self        = _self;
    _form->unit_choice = _unit_choice < 4u ? _unit_choice : 0u;
    _form->rows        = 1u;
    for(u32 _i = 0; _i < FUDE_ZOOM_PARTS_ROWS; _i++) {
        fude_zoom_parts_set_text(_form->widths[_i], "");
        fude_zoom_parts_set_text(_form->heights[_i], "");
    }
    rde_ui_label_set_text(_form->title, _title);
    rde_ui_label_set_text(_form->kerf, _kerf);
    rde_ui_label_set_text(_form->problem, "");
    fude_zoom_parts_show_units(_form);
    fude_kit_modal_show(&_form->modal, true);
    _form->opened_at = rde_engine_get_time_now();
    fude_zoom_parts_layout(_form);
    rde_ui_scroll_area_set_scroll(_form->list, (rde_vec_2F){ 0.0f, 0.0f });
    rde_ui_node_focus(rde_ui_text_editor_as_node(_form->widths[0]));   // the keyboard comes up
}

void fude_zoom_parts_close(fude_zoom_parts_form* _form) {
    if(_form->modal.card != NULL) {
        fude_kit_modal_show(&_form->modal, false);
    }
}

void fude_zoom_parts_update(fude_zoom_parts_form* _form) {
    if(!fude_zoom_parts_shown(_form)) {
        return;
    }
    const rde_vec_2F _screen = fude_kit_screen_size(_form->window);
    if(_screen.x != _form->laid_out.x || _screen.y != _form->laid_out.y) {
        fude_zoom_parts_layout(_form);
    }
}

void fude_zoom_parts_look(fude_zoom_parts_form* _form, const c8* _rows, b8 _cut) {
    u32 _row = 0;
    for(const c8* _p = _rows; *_p != 0 && _row < FUDE_ZOOM_PARTS_ROWS;) {
        const c8* _end = strchr(_p, ';');
        const usize _len = _end != NULL ? (usize)(_end - _p) : strlen(_p);
        c8 _one[128];
        snprintf(_one, sizeof(_one), "%.*s", (int)_len, _p);
        c8* _x = strchr(_one, 'x');
        if(_x != NULL) {
            *_x = 0;
            if(_row >= _form->rows) {
                _form->rows = _row + 1u;
            }
            fude_zoom_parts_set_text(_form->widths[_row], _one);
            fude_zoom_parts_set_text(_form->heights[_row], _x + 1);
            _row++;
        }
        if(_end == NULL) {
            break;
        }
        _p = _end + 1;
    }
    fude_zoom_parts_layout(_form);
    if(_cut) {
        fude_zoom_parts_on_save(NULL, NULL, _form);
    }
}
