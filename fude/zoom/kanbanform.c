// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/kanbanform.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/base/draw.h"
#include "drawing/base/icons.h"
#include "drawing/widgets/kit.h"

// ===========================================================================
// See kanbanform.h.
//
// The card is the kit's form (kit.h: its title, Cancel, a tap off it, laying it out again, its fields, list and chips).
// Its widgets are made once, FUDE_ZOOM_KANBAN_ROWS rows among them, and the rows past the count are hidden; a row
// dropped moves the names under it up rather than moving widgets. The file is in three parts: the templates and
// the rows' text, the presses, then the card built and laid out. Its own functions and constants share the header's
// prefix, fude_zoom_kanban_ and FUDE_ZOOM_KANBAN_. Sizes are UI points.
// ===========================================================================

#define FUDE_ZOOM_KANBAN_ROW_H   44.0f   // A column's row: its name's field and its trash button.
#define FUDE_ZOOM_KANBAN_CHIP_H  36.0f   // A template's chip.

// The templates' chips' words, in FUDE_ZOOM_KANBAN_TEMPLATES order: Kanban, Scrum, Retrospective, Week.
static const FUDE_TEXT_ FUDE_ZOOM_KANBAN_TEMPLATE_NAMES[FUDE_ZOOM_KANBAN_TEMPLATES] = {
    FUDE_TEXT_ZOOM_KANBAN_KANBAN, FUDE_TEXT_ZOOM_KANBAN_SCRUM, FUDE_TEXT_ZOOM_KANBAN_RETRO, FUDE_TEXT_ZOOM_KANBAN_WEEK,
};

// See the header.
u32 fude_zoom_kanban_template(u32 _which, const c8* _out[FUDE_ZOOM_KANBAN_ROWS]) {
    static const FUDE_TEXT_ _kanban[] = { FUDE_TEXT_ZOOM_KANBAN_TODO, FUDE_TEXT_ZOOM_KANBAN_DOING, FUDE_TEXT_ZOOM_KANBAN_DONE };
    static const FUDE_TEXT_ _scrum[]  = { FUDE_TEXT_ZOOM_KANBAN_BACKLOG, FUDE_TEXT_ZOOM_KANBAN_TODO, FUDE_TEXT_ZOOM_KANBAN_DOING, FUDE_TEXT_ZOOM_KANBAN_REVIEW, FUDE_TEXT_ZOOM_KANBAN_DONE };
    static const FUDE_TEXT_ _retro[]  = { FUDE_TEXT_ZOOM_KANBAN_WELL, FUDE_TEXT_ZOOM_KANBAN_IMPROVE, FUDE_TEXT_ZOOM_KANBAN_ACTIONS };
    static const FUDE_TEXT_ _week[]   = { FUDE_TEXT_ZOOM_KANBAN_MON, FUDE_TEXT_ZOOM_KANBAN_TUE, FUDE_TEXT_ZOOM_KANBAN_WED, FUDE_TEXT_ZOOM_KANBAN_THU, FUDE_TEXT_ZOOM_KANBAN_FRI };
    const FUDE_TEXT_* _list = _which == 1u ? _scrum : _which == 2u ? _retro : _which == 3u ? _week : _kanban;
    const u32 _n = _which == 1u ? 5u : _which == 2u ? 3u : _which == 3u ? 5u : 3u;
    for(u32 _i = 0; _i < _n; _i++) {
        _out[_i] = fude_text(_list[_i]);
    }
    return _n;
}

// Declared ahead: filling the rows lays the card out again (defined with the building, below).
RDE_INTERNAL void fude_zoom_kanban_layout(fude_zoom_kanban_form* _form);

// Puts a template's columns in the rows (the rest emptied), clears the problem line, lays the card out for them and
// scrolls the list back to its top.
RDE_INTERNAL void fude_zoom_kanban_fill(fude_zoom_kanban_form* _form, u32 _which) {
    const c8* _names[FUDE_ZOOM_KANBAN_ROWS];
    const u32 _n = fude_zoom_kanban_template(_which, _names);
    for(u32 _i = 0; _i < FUDE_ZOOM_KANBAN_ROWS; _i++) {
        fude_kit_form_set(&_form->card, _form->names[_i], _i < _n ? _names[_i] : "");
    }
    _form->rows = _n;
    rde_ui_label_set_text(_form->problem, "");
    fude_zoom_kanban_layout(_form);
    rde_ui_scroll_area_set_scroll(_form->list, (rde_vec_2F){ 0.0f, 0.0f });
}

// --- presses -------------------------------------------------------------------------------

// A template's chip: its columns in the rows (set, never toggled: a press can come twice on Android).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_kanban_on_template(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_kanban_ref* _ref = (const fude_zoom_kanban_ref*)_user_data;
    fude_zoom_kanban_fill(_ref->form, _ref->index);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A row's trash: the names under it move up a place and the last row goes; a row alone is only emptied, so there is
// always one.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_kanban_on_drop(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_kanban_ref* _ref  = (const fude_zoom_kanban_ref*)_user_data;
    fude_zoom_kanban_form*      _form = _ref->form;
    if(_ref->index >= _form->rows) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    fude_kit_form_drop_row(_form->names, _ref->index, _form->rows, FUDE_ZOOM_KANBAN_NAME);
    if(_form->rows > 1u) {
        _form->rows--;
    }
    rde_ui_label_set_text(_form->problem, "");
    fude_zoom_kanban_layout(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Add column: a new empty row at the end, scrolled into sight, its field focused (which brings the keyboard up).
// Nothing happens once there are FUDE_ZOOM_KANBAN_ROWS.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_kanban_on_add(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_kanban_form* _form = (fude_zoom_kanban_form*)_user_data;
    if(_form->rows >= FUDE_ZOOM_KANBAN_ROWS) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    const u32 _row = _form->rows++;
    fude_kit_form_set(&_form->card, _form->names[_row], "");
    fude_zoom_kanban_layout(_form);
    // The new row in sight (the list's end), its name to type.
    rde_ui_scroll_area_set_scroll(_form->list, (rde_vec_2F){ 0.0f, 1e9f });
    rde_ui_node_focus(rde_ui_text_editor_as_node(_form->names[_row]));
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Insert: the rows' names, trimmed of spaces and tabs, empty ones skipped, are handed to the opener's done after the
// card is put away. With none named at all, the card stays up, the problem line says so, and the first row is focused.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_kanban_on_save(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_kanban_form* _form = (fude_zoom_kanban_form*)_user_data;
    c8        _names[FUDE_ZOOM_KANBAN_ROWS][FUDE_ZOOM_KANBAN_NAME];
    const c8* _each[FUDE_ZOOM_KANBAN_ROWS];
    u32       _n = 0;
    for(u32 _i = 0; _i < _form->rows; _i++) {
        c8 _t[FUDE_ZOOM_KANBAN_NAME];
        fude_kit_field_text(_form->names[_i], _t, sizeof(_t));
        const c8* _a = _t;
        while(*_a == ' ' || *_a == '\t') {
            _a++;
        }
        usize _len = rde_strlen(_a);
        while(_len > 0 && (_a[_len - 1u] == ' ' || _a[_len - 1u] == '\t')) {
            _len--;
        }
        if(_len == 0) {
            continue;
        }
        rde_memcpy(_names[_n], _a, _len);
        _names[_n][_len] = 0;
        _each[_n]        = _names[_n];
        _n++;
    }
    if(_n == 0) {
        rde_ui_label_set_text(_form->problem, fude_text(FUDE_TEXT_ZOOM_KANBAN_NONE));
        rde_ui_node_focus(rde_ui_text_editor_as_node(_form->names[0]));
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    const fude_zoom_kanban_done _done = _form->done;
    any const                 _self = _form->self;
    fude_zoom_kanban_close(_form);
    if(_done != NULL) {
        _done(_self, _each, _n);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Enter in a row: on to the next one, or from the last a new one (as Add column).
RDE_INTERNAL void fude_zoom_kanban_on_submit(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    const fude_zoom_kanban_ref* _ref  = (const fude_zoom_kanban_ref*)_user_data;
    fude_zoom_kanban_form*      _form = _ref->form;
    if(_ref->index + 1u < _form->rows) {
        rde_ui_node_focus(rde_ui_text_editor_as_node(_form->names[_ref->index + 1u]));
    } else {
        fude_zoom_kanban_on_add(NULL, NULL, _form);
    }
}

// --- built, laid out --------------------------------------------------------------------------

// See the header. Each row is a one-line field of at most 60 characters and a trash button, both inside the list; the
// field's node and the button are handed the row's ref (the form and its index) as their user data.
void fude_zoom_kanban_build(fude_zoom_kanban_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font) {
    rde_memset(_form, 0, sizeof(*_form));
    rde_ui_node* _card = fude_kit_form_build(&_form->card, _root, _window, _font, fude_text(FUDE_TEXT_ZOOM_KANBAN_TITLE));
    _form->templates_label = fude_kit_form_label(&_form->card, _card, fude_text(FUDE_TEXT_ZOOM_KANBAN_TEMPLATES), 15.0f);
    for(u32 _i = 0; _i < FUDE_ZOOM_KANBAN_TEMPLATES; _i++) {
        _form->template_refs[_i] = (fude_zoom_kanban_ref){ _form, _i };
        _form->templates[_i]     = fude_kit_button(_card, fude_text(FUDE_ZOOM_KANBAN_TEMPLATE_NAMES[_i]), fude_zoom_kanban_on_template, &_form->template_refs[_i]);
    }
    _form->list = fude_kit_form_list(_card);
    rde_ui_node* _list = rde_ui_scroll_area_as_node(_form->list);
    for(u32 _i = 0; _i < FUDE_ZOOM_KANBAN_ROWS; _i++) {
        _form->refs[_i]  = (fude_zoom_kanban_ref){ _form, _i };
        _form->names[_i] = fude_kit_form_field(&_form->card, _list, 60u, fude_text(FUDE_TEXT_ZOOM_KANBAN_COLUMN), (any)&_form->refs[_i], NULL,
                                               fude_zoom_kanban_on_submit);
        _form->drops[_i] = fude_kit_button(_list, fude_text(FUDE_TEXT_ZOOM_PART_DROP), fude_zoom_kanban_on_drop, &_form->refs[_i]);
        fude_kit_icon(_form->drops[_i], FUDE_ICON_TRASH, FUDE_KIT_ICON_ONLY, 16.0f);
    }
    c8 _with_icon[160];
    rde_snprintf(_with_icon, sizeof(_with_icon), FUDE_ICON_PLUS "  %s", fude_text(FUDE_TEXT_ZOOM_KANBAN_ADD));
    _form->add     = fude_kit_button(_card, _with_icon, fude_zoom_kanban_on_add, _form);
    _form->problem = fude_kit_form_label(&_form->card, _card, "", 14.0f);
    _form->cancel  = fude_kit_button(_card, fude_text(FUDE_TEXT_CANCEL), fude_kit_form_on_cancel, &_form->card);
    _form->save    = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_INSERT), fude_zoom_kanban_on_save, _form);
    fude_zoom_kanban_restyle(_form);
    fude_kit_modal_show(&_form->card.modal, false);
}

// See the header: with the card NULL, every other call does nothing.
void fude_zoom_kanban_forget(fude_zoom_kanban_form* _form) {
    rde_memset(_form, 0, sizeof(*_form));
}

// See the header: false before the card is built and after it is forgotten.
b8 fude_zoom_kanban_shown(const fude_zoom_kanban_form* _form) {
    return fude_kit_form_shown(&_form->card);
}

// See the header. The templates' chips are never shown lit: the card keeps no template chosen.
void fude_zoom_kanban_restyle(fude_zoom_kanban_form* _form) {
    if(!fude_kit_form_restyle(&_form->card)) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    rde_ui_label_set_color(_form->templates_label, _t->text_soft);
    rde_ui_label_set_color(_form->problem, _t->score_poor);
    for(u32 _i = 0; _i < FUDE_ZOOM_KANBAN_TEMPLATES; _i++) {
        fude_kit_button_chip(_form->templates[_i], false);
    }
    for(u32 _i = 0; _i < FUDE_ZOOM_KANBAN_ROWS; _i++) {
        fude_kit_style_field(_form->names[_i]);
        fude_kit_button_quiet(_form->drops[_i]);
        fude_kit_button_round(_form->drops[_i], 10.0f);
    }
    fude_kit_button_colors(_form->add, _t->surface, 1.0f, _t->outline);
    fude_kit_button_round(_form->add, 10.0f);
    fude_kit_button_plain(_form->cancel);
    fude_kit_button_primary(_form->save);
}

// Places the templates' chips from _left along the card's line at _top (card-local, Y up), starting a new line when
// the next does not fit before _right (kit.h: fude_kit_form_chips); each is as wide as its words and 28 points, at
// least 64. Returns how tall they are; with _place false they are only measured.
RDE_INTERNAL f32 fude_zoom_kanban_chips(fude_zoom_kanban_form* _form, f32 _left, f32 _top, f32 _right, b8 _place) {
    const c8* _names[FUDE_ZOOM_KANBAN_TEMPLATES];
    for(u32 _i = 0; _i < FUDE_ZOOM_KANBAN_TEMPLATES; _i++) {
        _names[_i] = fude_text(FUDE_ZOOM_KANBAN_TEMPLATE_NAMES[_i]);
    }
    return fude_kit_form_chips(_form->templates, _names, FUDE_ZOOM_KANBAN_TEMPLATES, 64.0f, 28.0f, FUDE_ZOOM_KANBAN_CHIP_H, _left, _top, _right, _place);
}

// Lays the card out high on the screen (the keyboard comes up under it), at most 560 points wide: its list as tall as
// its rows, up to what leaves room for the rest above the keyboard, and at least one row. Remembers the screen's
// size, so fude_zoom_kanban_update knows when to do it again.
RDE_INTERNAL void fude_zoom_kanban_layout(fude_zoom_kanban_form* _form) {
    const rde_vec_2F _screen = fude_kit_screen_size(_form->card.window);
    const f32 _pad = FUDE_KIT_FORM_PAD, _gap = FUDE_KIT_FORM_GAP, _rh = FUDE_ZOOM_KANBAN_ROW_H;
    const f32 _w  = rde_math_min_f32(560.0f, _screen.x - 32.0f);
    const f32 _iw = _w - 2.0f * _pad;
    // The templates' label beside their chips (over them when the card is narrow).
    const f32 _lw    = rde_math_min_f32(130.0f, 12.0f + fude_draw_text_width(fude_kit_font(), (f32)FUDE_KIT_FONT_SIZE, fude_text(FUDE_TEXT_ZOOM_KANBAN_TEMPLATES), 15.0f));
    const b8  _above = _iw - _lw < 260.0f;
    const f32 _cl    = _pad + (_above ? 0.0f : _lw);
    const f32 _ch    = fude_zoom_kanban_chips(_form, _cl, 0.0f, _pad + _iw, false) + (_above ? 22.0f + _gap * 0.5f : 0.0f);
    const f32 _fixed = _pad + 28.0f + _gap + _ch + _gap + _gap + 40.0f + _gap + 22.0f + _gap + 44.0f + _pad;
    // Over a keyboard about as tall as half the screen; the card's top is 90 points under the screen's.
    const f32 _room  = rde_math_max_f32(_screen.y * 0.55f - 90.0f - _fixed, _rh + _gap);
    const f32 _all   = (f32)_form->rows * (_rh + _gap) - _gap;
    const f32 _lh    = rde_math_min_f32(_all, _room);
    const f32 _h     = _fixed + _lh;
    // Top to bottom (card-local: the bottom left the origin), from under the title.
    f32 _y = fude_kit_form_place(&_form->card, _screen, FUDE_KIT_FORM_HIGH, _w, _h);
    if(_above) {
        fude_kit_place(rde_ui_label_as_node(_form->templates_label), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 11.0f }, (rde_vec_2F){ _iw, 22.0f });
        fude_zoom_kanban_chips(_form, _cl, _y - 22.0f - _gap * 0.5f, _pad + _iw, true);
    } else {
        fude_kit_place(rde_ui_label_as_node(_form->templates_label), (rde_vec_2F){ _pad + _lw * 0.5f, _y - FUDE_ZOOM_KANBAN_CHIP_H * 0.5f }, (rde_vec_2F){ _lw, FUDE_ZOOM_KANBAN_CHIP_H });
        fude_zoom_kanban_chips(_form, _cl, _y, _pad + _iw, true);
    }
    _y -= _ch + _gap;
    // The list: its rows from the top of its content.
    fude_kit_place(rde_ui_scroll_area_as_node(_form->list), (rde_vec_2F){ _pad + _iw * 0.5f, _y - _lh * 0.5f }, (rde_vec_2F){ _iw, _lh });
    rde_ui_scroll_area_set_content_size(_form->list, (rde_vec_2F){ _iw, _all });
    const f32 _dw = 44.0f;
    const f32 _fw = _iw - _dw - _gap;
    for(u32 _i = 0; _i < FUDE_ZOOM_KANBAN_ROWS; _i++) {
        const b8  _on = _i < _form->rows;
        const f32 _ry = _all - (f32)_i * (_rh + _gap) - _rh * 0.5f;
        rde_ui_node_set_active(fude_kit_field_node(_form->names[_i]), _on);
        rde_ui_node_set_active(rde_ui_button_as_node(_form->drops[_i]), _on);
        if(!_on) {
            continue;
        }
        fude_kit_place(fude_kit_field_node(_form->names[_i]), (rde_vec_2F){ _fw * 0.5f, _ry }, (rde_vec_2F){ _fw, _rh });
        fude_kit_place(rde_ui_button_as_node(_form->drops[_i]), (rde_vec_2F){ _iw - _dw * 0.5f, _ry }, (rde_vec_2F){ _dw, _rh });
    }
    _y -= _lh + _gap;
    fude_kit_set_enabled(_form->add, _form->rows < FUDE_ZOOM_KANBAN_ROWS);
    fude_kit_place(rde_ui_button_as_node(_form->add), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 20.0f }, (rde_vec_2F){ _iw, 40.0f });
    _y -= 40.0f + _gap;
    fude_kit_place(rde_ui_label_as_node(_form->problem), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 11.0f }, (rde_vec_2F){ _iw, 22.0f });
    fude_kit_form_buttons(_form->cancel, _form->save, _w);
}

// See the header. The card's opening time is kept for the tap off it.
void fude_zoom_kanban_open(fude_zoom_kanban_form* _form, fude_zoom_kanban_done _done, any _self) {
    if(_form->card.modal.card == NULL) {
        return;
    }
    _form->done = _done;
    _form->self = _self;
    fude_kit_form_show(&_form->card);
    fude_zoom_kanban_fill(_form, 0u);   // No row focused: the keyboard stays down, the rows filled in already.
}

// See the header.
void fude_zoom_kanban_close(fude_zoom_kanban_form* _form) {
    fude_kit_form_close(&_form->card);
}

// See the header: lays the card out again only while it is up and the screen's size has changed.
void fude_zoom_kanban_update(fude_zoom_kanban_form* _form) {
    if(fude_kit_form_resized(&_form->card)) {
        fude_zoom_kanban_layout(_form);
    }
}

// See the header. The harness opens the card just before (page_look.c); it does nothing while the card is not up, as
// its widgets may not be built and Insert would call a callback nobody handed it.
void fude_zoom_kanban_look(fude_zoom_kanban_form* _form, u32 _which, b8 _extra, b8 _insert) {
    if(!fude_zoom_kanban_shown(_form)) {
        return;
    }
    fude_zoom_kanban_fill(_form, _which);
    if(_extra && _form->rows < FUDE_ZOOM_KANBAN_ROWS) {
        fude_kit_form_set(&_form->card, _form->names[_form->rows++], "Ideas");
        fude_zoom_kanban_layout(_form);
    }
    if(_insert) {
        fude_zoom_kanban_on_save(NULL, NULL, _form);
    }
}
