// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/partsform.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/base/icons.h"
#include "drawing/widgets/kit.h"

// ===========================================================================
// See partsform.h.
//
// The card is the kit's form (kit.h: its title, Cancel, a tap off it, laying it out again, its fields and list). Its
// widgets are made once, FUDE_ZOOM_PARTS_ROWS rows among them, and the rows past the count are hidden; a row dropped
// moves the sizes under it up rather than moving widgets. Every button's and field's user data is a ref
// kept in the form itself. The file is in three parts: reading a size and the rows' text, the presses, then the card
// built and laid out. Its own functions and constants share the header's prefix, fude_zoom_parts_ and
// FUDE_ZOOM_PARTS_. Sizes are UI points.
// ===========================================================================

#define FUDE_ZOOM_PARTS_ROW_H  44.0f   // A part's row: its width, the ×, its height and its trash button.
#define FUDE_ZOOM_PARTS_UNITS_MOST  110.0f   // The units' label beside their buttons: at most this wide,
#define FUDE_ZOOM_PARTS_UNITS_LEAST 64.0f    // ...and at least this (narrower: a row of its own over them)

// The unit buttons' words, in unit_choice's order (as Settings' Measurements has them).
static const c8* const FUDE_ZOOM_PARTS_UNIT_NAMES[4] = { "mm", "cm", "in", "ft \xC2\xB7 in" };

// See the header. Feet and inches reads a bare number as inches, as the numpad does (units.h).
b8 fude_zoom_parts_read(const c8* _text, u8 _unit_choice, f64* _mm) {
    // Each unit choice's unit for a bare number (feet and inches: inches).
    static const FUDE_ZOOM_UNIT_ _units[4] = { FUDE_ZOOM_UNIT_MM, FUDE_ZOOM_UNIT_CM, FUDE_ZOOM_UNIT_IN, FUDE_ZOOM_UNIT_IN };
    fude_zoom_units_error _e;
    return fude_zoom_units_length(_text, _units[_unit_choice < 4u ? _unit_choice : 0u], NULL, _mm, &_e) && *_mm > 0.0;
}

// Returns whether _t is empty or only spaces and tabs.
RDE_INTERNAL b8 fude_zoom_parts_blank(const c8* _t) {
    for(; *_t != 0; _t++) {
        if(*_t != ' ' && *_t != '\t') {
            return false;
        }
    }
    return true;
}

// Declared ahead: a row dropped or added lays the card out again (defined with the building, below).
RDE_INTERNAL void fude_zoom_parts_layout(fude_zoom_parts_form* _form);

// Shows the unit chosen: its button selected, the others quiet.
RDE_INTERNAL void fude_zoom_parts_show_units(fude_zoom_parts_form* _form) {
    for(u32 _i = 0; _i < 4u; _i++) {
        if(_i == _form->unit_choice) { fude_kit_button_selected(_form->units[_i]); }
        else                         { fude_kit_button_quiet(_form->units[_i]); }
    }
}

// --- presses -------------------------------------------------------------------------------

// A unit's button: the sizes are read in it from now on (the rows' text is left as typed).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_parts_on_unit(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_parts_ref* _ref = (const fude_zoom_parts_ref*)_user_data;
    _ref->form->unit_choice = (u8)_ref->index;
    fude_zoom_parts_show_units(_ref->form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A row's trash: the sizes under it move up a place and the last row goes; a row alone is only emptied, so there is
// always one.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_parts_on_drop(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_parts_ref* _ref  = (const fude_zoom_parts_ref*)_user_data;
    fude_zoom_parts_form*      _form = _ref->form;
    if(_ref->index >= _form->rows) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    fude_kit_form_drop_row(_form->widths, _ref->index, _form->rows, 64u);
    fude_kit_form_drop_row(_form->heights, _ref->index, _form->rows, 64u);
    if(_form->rows > 1u) {
        _form->rows--;
    }
    rde_ui_label_set_text(_form->problem, "");
    fude_zoom_parts_layout(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Add part: a new empty row at the end, scrolled into sight, its width focused. Nothing happens once there are
// FUDE_ZOOM_PARTS_ROWS.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_parts_on_add(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_parts_form* _form = (fude_zoom_parts_form*)_user_data;
    if(_form->rows >= FUDE_ZOOM_PARTS_ROWS) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    const u32 _row = _form->rows++;
    fude_kit_form_set(&_form->card, _form->widths[_row], "");
    fude_kit_form_set(&_form->card, _form->heights[_row], "");
    fude_zoom_parts_layout(_form);
    // The new row in sight (the list's end), its width to type.
    rde_ui_scroll_area_set_scroll(_form->list, (rde_vec_2F){ 0.0f, 1e9f });
    rde_ui_node_focus(rde_ui_text_editor_as_node(_form->widths[_row]));
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Cut: every row read in the unit chosen (rows with both sizes blank skipped), then the card put away and the sizes
// handed to the opener's done. The first row that cannot be read keeps the card up: the problem line names the row
// and what was typed ("?" for a blank size), and the size at fault is focused. With no row filled in at all, the card
// stays up and says so.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_parts_on_save(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_parts_form* _form = (fude_zoom_parts_form*)_user_data;
    fude_zoom_parts_size  _parts[FUDE_ZOOM_PARTS_ROWS];
    u32                   _n = 0;
    for(u32 _i = 0; _i < _form->rows; _i++) {
        c8 _w[64], _h[64];
        fude_kit_field_text(_form->widths[_i], _w, sizeof(_w));
        fude_kit_field_text(_form->heights[_i], _h, sizeof(_h));
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
    any const                _self = _form->self;
    fude_zoom_parts_close(_form);
    if(_done != NULL) {
        _done(_self, _parts, _n);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Enter in a width: on to its height; in a height: on to the next row's width, or from the last row a new one (as Add
// part). A field's ref index is its row times 2, plus 1 for a height.
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

// See the header. Each row is two one-line fields of at most 24 characters with a × between them, and a trash button,
// all inside the list.
void fude_zoom_parts_build(fude_zoom_parts_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font) {
    rde_memset(_form, 0, sizeof(*_form));
    rde_ui_node* _card = fude_kit_form_build(&_form->card, _root, _window, _font, "");
    _form->units_label = fude_kit_form_label(&_form->card, _card, fude_text(FUDE_TEXT_ZOOM_UNITS), 15.0f);
    for(u32 _i = 0; _i < 4u; _i++) {
        _form->unit_refs[_i] = (fude_zoom_parts_ref){ _form, _i };
        _form->units[_i]     = fude_kit_button(_card, FUDE_ZOOM_PARTS_UNIT_NAMES[_i], fude_zoom_parts_on_unit, &_form->unit_refs[_i]);
    }
    // (the kit's list: no room kept under the rows for the keyboard, which pushed them out of sight on the tablet)
    _form->list = fude_kit_form_list(_card);
    rde_ui_node* _list = rde_ui_scroll_area_as_node(_form->list);
    for(u32 _i = 0; _i < FUDE_ZOOM_PARTS_ROWS; _i++) {
        _form->refs[_i] = (fude_zoom_parts_ref){ _form, _i };
        for(u32 _k = 0; _k < 2u; _k++) {
            _form->field_refs[2u * _i + _k] = (fude_zoom_parts_ref){ _form, 2u * _i + _k };
            rde_ui_text_editor* _f = fude_kit_form_field(&_form->card, _list, 24u, fude_text(_k == 0u ? FUDE_TEXT_ZOOM_PART_WIDTH : FUDE_TEXT_ZOOM_PART_HEIGHT),
                                                         (any)&_form->field_refs[2u * _i + _k], NULL, fude_zoom_parts_on_submit);
            if(_k == 0u) { _form->widths[_i] = _f; } else { _form->heights[_i] = _f; }
        }
        _form->times[_i] = fude_kit_form_label(&_form->card, _list, "\xC3\x97", 18.0f);
        rde_ui_label_set_alignment(_form->times[_i], RDE_UI_LABEL_H_ALIGN_CENTER, RDE_UI_LABEL_V_ALIGN_MIDDLE);
        _form->drops[_i] = fude_kit_button(_list, fude_text(FUDE_TEXT_ZOOM_PART_DROP), fude_zoom_parts_on_drop, &_form->refs[_i]);
        fude_kit_icon(_form->drops[_i], FUDE_ICON_TRASH, FUDE_KIT_ICON_ONLY, 16.0f);
    }
    c8 _with_icon[160];
    rde_snprintf(_with_icon, sizeof(_with_icon), FUDE_ICON_PLUS "  %s", fude_text(FUDE_TEXT_ZOOM_PART_ADD));
    _form->add     = fude_kit_button(_card, _with_icon, fude_zoom_parts_on_add, _form);
    _form->kerf    = fude_kit_form_label(&_form->card, _card, "", 14.0f);
    _form->problem = fude_kit_form_label(&_form->card, _card, "", 14.0f);
    _form->cancel  = fude_kit_button(_card, fude_text(FUDE_TEXT_CANCEL), fude_kit_form_on_cancel, &_form->card);
    _form->save    = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_PART_CUT), fude_zoom_parts_on_save, _form);
    fude_zoom_parts_restyle(_form);
    fude_kit_modal_show(&_form->card.modal, false);
}

// See the header: with the card NULL, every other call does nothing.
void fude_zoom_parts_forget(fude_zoom_parts_form* _form) {
    rde_memset(_form, 0, sizeof(*_form));
}

// See the header: false before the card is built and after it is forgotten.
b8 fude_zoom_parts_shown(const fude_zoom_parts_form* _form) {
    return fude_kit_form_shown(&_form->card);
}

// See the header. The title is in the text colour (the kit's), the problem line in the poor score's, the other labels
// soft.
void fude_zoom_parts_restyle(fude_zoom_parts_form* _form) {
    if(!fude_kit_form_restyle(&_form->card)) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    rde_ui_label_set_color(_form->units_label, _t->text_soft);
    rde_ui_label_set_color(_form->kerf, _t->text_soft);
    rde_ui_label_set_color(_form->problem, _t->score_poor);
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

// Lays the card out high on the screen (the keyboard comes up under it), at most 560 points wide: its list as tall as
// its rows, up to what leaves room for the rest above the keyboard, and at least one row. The units' label and their
// four buttons share a row when they fit across it, the label at least FUDE_ZOOM_PARTS_UNITS_LEAST wide; on a narrower
// card (a phone held upright) the label has a row of its own over the buttons, which share the card's width.
// Remembers the screen's size, so fude_zoom_parts_update knows when to do it again.
RDE_INTERNAL void fude_zoom_parts_layout(fude_zoom_parts_form* _form) {
    const rde_vec_2F _screen = fude_kit_screen_size(_form->card.window);
    const f32 _pad = FUDE_KIT_FORM_PAD, _gap = FUDE_KIT_FORM_GAP, _rh = FUDE_ZOOM_PARTS_ROW_H;
    const f32 _w     = rde_math_min_f32(560.0f, _screen.x - 32.0f);
    const f32 _iw    = _w - 2.0f * _pad;
    // The four unit buttons: 70 wide (ft · in 84), 6 apart, after the label.
    const f32 _uw = 70.0f, _ug = 6.0f, _buttons = 3.0f * (_uw + _ug) + 84.0f;
    const b8  _one_row = _iw - _ug - _buttons >= FUDE_ZOOM_PARTS_UNITS_LEAST;
    const f32 _fixed = _pad + 28.0f + _gap + 40.0f + _gap + _gap + 40.0f + _gap + 22.0f + 22.0f + _gap + 44.0f + _pad + (_one_row ? 0.0f : 40.0f + _gap);
    // Over a keyboard about as tall as half the screen; the card's top is 90 points under the screen's.
    const f32 _room  = rde_math_max_f32(_screen.y * 0.55f - 90.0f - _fixed, _rh + _gap);
    const f32 _all   = (f32)_form->rows * (_rh + _gap) - _gap;
    const f32 _lh    = rde_math_min_f32(_all, _room);
    const f32 _h     = _fixed + _lh;
    fude_kit_form_place(&_form->card, _screen, FUDE_KIT_FORM_HIGH, _w, _h);
    // Top to bottom (card-local: the bottom left the origin), from the title's middle (its line's centre).
    f32 _y = _h - _pad - 14.0f;
    _y -= 14.0f + _gap + 20.0f;
    if(_one_row) {
        // The units' label, at most 110 wide, then the four buttons.
        const f32 _ul = rde_math_min_f32(FUDE_ZOOM_PARTS_UNITS_MOST, _iw - _ug - _buttons);
        fude_kit_place(rde_ui_label_as_node(_form->units_label), (rde_vec_2F){ _pad + _ul * 0.5f, _y }, (rde_vec_2F){ _ul, 40.0f });
        for(u32 _i = 0; _i < 4u; _i++) {
            const f32 _bw = _i == 3u ? 84.0f : _uw;
            const f32 _x  = _pad + _ul + _ug + (f32)_i * (_uw + _ug) + _bw * 0.5f;
            fude_kit_place(rde_ui_button_as_node(_form->units[_i]), (rde_vec_2F){ _x, _y }, (rde_vec_2F){ _bw, 40.0f });
        }
    } else {
        // The label across the card, then the buttons under it sharing its width as 70 and 84 do.
        fude_kit_place(rde_ui_label_as_node(_form->units_label), (rde_vec_2F){ _pad + _iw * 0.5f, _y }, (rde_vec_2F){ _iw, 40.0f });
        _y -= 40.0f + _gap;
        const f32 _k = (_iw - 3.0f * _ug) / (3.0f * _uw + 84.0f);
        f32 _x = _pad;
        for(u32 _i = 0; _i < 4u; _i++) {
            const f32 _bw = (_i == 3u ? 84.0f : _uw) * _k;
            fude_kit_place(rde_ui_button_as_node(_form->units[_i]), (rde_vec_2F){ _x + _bw * 0.5f, _y }, (rde_vec_2F){ _bw, 40.0f });
            _x += _bw + _ug;
        }
    }
    _y -= 20.0f + _gap;
    // The list: its rows from the top of its content.
    fude_kit_place(rde_ui_scroll_area_as_node(_form->list), (rde_vec_2F){ _pad + _iw * 0.5f, _y - _lh * 0.5f }, (rde_vec_2F){ _iw, _lh });
    rde_ui_scroll_area_set_content_size(_form->list, (rde_vec_2F){ _iw, _all });
    // A row: the width, the × (_xw), the height, and the trash button (_dw); the two fields share what is left.
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
    fude_kit_form_buttons(_form->cancel, _form->save, _w);
}

// See the header. Every row is emptied, not only the one shown; the card's opening time is kept for the tap off it.
void fude_zoom_parts_open(fude_zoom_parts_form* _form, const c8* _title, const c8* _kerf, u8 _unit_choice, fude_zoom_parts_done _done, any _self) {
    if(_form->card.modal.card == NULL) {
        return;
    }
    _form->done        = _done;
    _form->self        = _self;
    _form->unit_choice = _unit_choice < 4u ? _unit_choice : 0u;
    _form->rows        = 1u;
    for(u32 _i = 0; _i < FUDE_ZOOM_PARTS_ROWS; _i++) {
        fude_kit_form_set(&_form->card, _form->widths[_i], "");
        fude_kit_form_set(&_form->card, _form->heights[_i], "");
    }
    rde_ui_label_set_text(_form->card.title, _title);
    rde_ui_label_set_text(_form->kerf, _kerf);
    rde_ui_label_set_text(_form->problem, "");
    fude_zoom_parts_show_units(_form);
    fude_kit_form_show(&_form->card);
    fude_zoom_parts_layout(_form);
    rde_ui_scroll_area_set_scroll(_form->list, (rde_vec_2F){ 0.0f, 0.0f });
    rde_ui_node_focus(rde_ui_text_editor_as_node(_form->widths[0]));   // the keyboard comes up
}

// See the header.
void fude_zoom_parts_close(fude_zoom_parts_form* _form) {
    fude_kit_form_close(&_form->card);
}

// See the header: lays the card out again only while it is up and the screen's size has changed.
void fude_zoom_parts_update(fude_zoom_parts_form* _form) {
    if(fude_kit_form_resized(&_form->card)) {
        fude_zoom_parts_layout(_form);
    }
}

// See the header. Each part is split at its first 'x'; one with no 'x' is skipped. Rows past those filled keep what
// they had. It does nothing while the card is not up: its widgets may not be built, and Cut would call a callback
// nobody handed it.
void fude_zoom_parts_look(fude_zoom_parts_form* _form, const c8* _rows, b8 _cut) {
    if(!fude_zoom_parts_shown(_form)) {
        return;
    }
    u32 _row = 0;
    for(const c8* _p = _rows; *_p != 0 && _row < FUDE_ZOOM_PARTS_ROWS;) {
        const c8* _end = rde_strchr(_p, ';');
        const usize _len = _end != NULL ? (usize)(_end - _p) : rde_strlen(_p);
        c8 _one[128];
        rde_snprintf(_one, sizeof(_one), "%.*s", (int)_len, _p);
        c8* _x = rde_strchr(_one, 'x');
        if(_x != NULL) {
            *_x = 0;
            if(_row >= _form->rows) {
                _form->rows = _row + 1u;
            }
            fude_kit_form_set(&_form->card, _form->widths[_row], _one);
            fude_kit_form_set(&_form->card, _form->heights[_row], _x + 1);
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
