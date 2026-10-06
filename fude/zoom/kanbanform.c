// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/kanbanform.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/widgets/draw.h"
#include "drawing/widgets/icons.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FUDE_ZOOM_KANBAN_PAD     16.0f
#define FUDE_ZOOM_KANBAN_GAP     8.0f
#define FUDE_ZOOM_KANBAN_ROW_H   44.0f
#define FUDE_ZOOM_KANBAN_CHIP_H  36.0f
#define FUDE_ZOOM_KANBAN_FIELD_PX 18u

static const FUDE_TEXT_ FUDE_ZOOM_KANBAN_TEMPLATE_NAMES[FUDE_ZOOM_KANBAN_TEMPLATES] = {
    FUDE_TEXT_ZOOM_KANBAN_KANBAN, FUDE_TEXT_ZOOM_KANBAN_SCRUM, FUDE_TEXT_ZOOM_KANBAN_RETRO, FUDE_TEXT_ZOOM_KANBAN_WEEK,
};

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

// A field's text (a copy, "" when empty), into _out.
RDE_INTERNAL void fude_zoom_kanban_text(rde_ui_text_editor* _field, c8* _out, usize _size) {
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

RDE_INTERNAL void fude_zoom_kanban_set_text(rde_ui_text_editor* _field, const c8* _text) {
    const usize _bytes = rde_ui_text_editor_get_byte_count(_field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_field, 0, _bytes);
    }
    if(_text[0] != 0) {
        rde_ui_text_editor_insert_at(_field, 0, _text, strlen(_text));
    }
}

RDE_INTERNAL void fude_zoom_kanban_layout(fude_zoom_kanban_form* _form);

// A template's columns in the rows (the rest emptied), from the top.
RDE_INTERNAL void fude_zoom_kanban_fill(fude_zoom_kanban_form* _form, u32 _which) {
    const c8* _names[FUDE_ZOOM_KANBAN_ROWS];
    const u32 _n = fude_zoom_kanban_template(_which, _names);
    for(u32 _i = 0; _i < FUDE_ZOOM_KANBAN_ROWS; _i++) {
        fude_zoom_kanban_set_text(_form->names[_i], _i < _n ? _names[_i] : "");
    }
    _form->rows = _n;
    rde_ui_label_set_text(_form->problem, "");
    fude_zoom_kanban_layout(_form);
    rde_ui_scroll_area_set_scroll(_form->list, (rde_vec_2F){ 0.0f, 0.0f });
}

// --- presses -------------------------------------------------------------------------------

// A template: its columns in the rows (set, never toggled: a press can come twice on Android).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_kanban_on_template(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_kanban_ref* _ref = (const fude_zoom_kanban_ref*)_user_data;
    fude_zoom_kanban_fill(_ref->form, _ref->index);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A row's trash: the rows under it move up a place (the last one only emptied when it is alone).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_kanban_on_drop(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_kanban_ref* _ref  = (const fude_zoom_kanban_ref*)_user_data;
    fude_zoom_kanban_form*      _form = _ref->form;
    if(_ref->index >= _form->rows) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    c8 _name[FUDE_ZOOM_KANBAN_NAME];
    for(u32 _i = _ref->index; _i + 1u < _form->rows; _i++) {
        fude_zoom_kanban_text(_form->names[_i + 1u], _name, sizeof(_name));
        fude_zoom_kanban_set_text(_form->names[_i], _name);
    }
    fude_zoom_kanban_set_text(_form->names[_form->rows - 1u], "");
    if(_form->rows > 1u) {
        _form->rows--;
    }
    rde_ui_label_set_text(_form->problem, "");
    fude_zoom_kanban_layout(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_kanban_on_add(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_kanban_form* _form = (fude_zoom_kanban_form*)_user_data;
    if(_form->rows >= FUDE_ZOOM_KANBAN_ROWS) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    const u32 _row = _form->rows++;
    fude_zoom_kanban_set_text(_form->names[_row], "");
    fude_zoom_kanban_layout(_form);
    // The new row in sight (the list's end), its name to type.
    rde_ui_scroll_area_set_scroll(_form->list, (rde_vec_2F){ 0.0f, 1e9f });
    rde_ui_node_focus(rde_ui_text_editor_as_node(_form->names[_row]));
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_kanban_on_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_kanban_close((fude_zoom_kanban_form*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A tap off the card: put away — not the release of the press that opened it.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_kanban_on_dismiss(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_kanban_form* _form = (fude_zoom_kanban_form*)_user_data;
    if(rde_engine_get_time_now() - _form->opened_at >= 0.6) {
        fude_zoom_kanban_close(_form);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Insert: the rows' names, trimmed, empty ones skipped; none at all keeps the card up, said.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_kanban_on_save(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_kanban_form* _form = (fude_zoom_kanban_form*)_user_data;
    c8        _names[FUDE_ZOOM_KANBAN_ROWS][FUDE_ZOOM_KANBAN_NAME];
    const c8* _each[FUDE_ZOOM_KANBAN_ROWS];
    u32       _n = 0;
    for(u32 _i = 0; _i < _form->rows; _i++) {
        c8 _t[FUDE_ZOOM_KANBAN_NAME];
        fude_zoom_kanban_text(_form->names[_i], _t, sizeof(_t));
        const c8* _a = _t;
        while(*_a == ' ' || *_a == '\t') {
            _a++;
        }
        usize _len = strlen(_a);
        while(_len > 0 && (_a[_len - 1u] == ' ' || _a[_len - 1u] == '\t')) {
            _len--;
        }
        if(_len == 0) {
            continue;
        }
        memcpy(_names[_n], _a, _len);
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
    void* const                 _self = _form->self;
    fude_zoom_kanban_close(_form);
    if(_done != NULL) {
        _done(_self, _each, _n);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Enter in a row: on to the next one (a new one at the end).
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

RDE_INTERNAL rde_ui_label* fude_zoom_kanban_label(fude_zoom_kanban_form* _form, rde_ui_node* _parent, const c8* _text, f32 _px) {
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

void fude_zoom_kanban_build(fude_zoom_kanban_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font) {
    memset(_form, 0, sizeof(*_form));
    _form->window = _window;
    _form->font   = _font;
    fude_kit_modal_create(&_form->modal, _root, fude_zoom_kanban_on_dismiss, _form);
    rde_ui_node* _card = rde_ui_image_as_node(_form->modal.card);
    _form->title           = fude_zoom_kanban_label(_form, _card, fude_text(FUDE_TEXT_ZOOM_KANBAN_TITLE), 17.0f);
    _form->templates_label = fude_zoom_kanban_label(_form, _card, fude_text(FUDE_TEXT_ZOOM_KANBAN_TEMPLATES), 15.0f);
    for(u32 _i = 0; _i < FUDE_ZOOM_KANBAN_TEMPLATES; _i++) {
        _form->template_refs[_i] = (fude_zoom_kanban_ref){ _form, _i };
        _form->templates[_i]     = fude_kit_button(_card, fude_text(FUDE_ZOOM_KANBAN_TEMPLATE_NAMES[_i]), fude_zoom_kanban_on_template, &_form->template_refs[_i]);
    }
    _form->list = rde_ui_scroll_area_create(NULL);
    rde_ui_scroll_area_set_bar_thickness(_form->list, 3.0f);
    rde_ui_scroll_area_set_background_color(_form->list, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_track_color(_form->list, (rde_color){ 0, 0, 0, 0 });
    // The card is above the keyboard already: no room kept under the rows for it (that room pushed the rows out of
    // sight while the keyboard was up).
    rde_ui_scroll_area_set_keyboard_inset_enabled(_form->list, false);
    rde_ui_node* _list = rde_ui_scroll_area_as_node(_form->list);
    rde_ui_node_add_child(_card, _list);
    for(u32 _i = 0; _i < FUDE_ZOOM_KANBAN_ROWS; _i++) {
        _form->refs[_i] = (fude_zoom_kanban_ref){ _form, _i };
        rde_ui_text_editor* _f = rde_ui_text_editor_create(_font, NULL);
        rde_ui_text_editor_set_multiline(_f, false);
        rde_ui_text_editor_set_allow_multicursor(_f, false);
        rde_ui_text_editor_set_font_size(_f, FUDE_ZOOM_KANBAN_FIELD_PX);
        rde_ui_text_editor_set_max_chars(_f, 60u);
        rde_ui_text_editor_set_content_insets(_f, 10.0f, 8.0f, 10.0f, 8.0f);
        rde_ui_text_editor_set_placeholder(_f, fude_text(FUDE_TEXT_ZOOM_KANBAN_COLUMN));
        rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_f), (any)&_form->refs[_i]);
        rde_ui_text_editor_set_on_submit(_f, fude_zoom_kanban_on_submit);
        fude_kit_field_box(_list, _f);
        _form->names[_i] = _f;
        _form->drops[_i] = fude_kit_button(_list, fude_text(FUDE_TEXT_ZOOM_PART_DROP), fude_zoom_kanban_on_drop, &_form->refs[_i]);
        fude_kit_icon(_form->drops[_i], FUDE_ICON_TRASH, FUDE_KIT_ICON_ONLY, 16.0f);
    }
    c8 _with_icon[160];
    snprintf(_with_icon, sizeof(_with_icon), FUDE_ICON_PLUS "  %s", fude_text(FUDE_TEXT_ZOOM_KANBAN_ADD));
    _form->add     = fude_kit_button(_card, _with_icon, fude_zoom_kanban_on_add, _form);
    _form->problem = fude_zoom_kanban_label(_form, _card, "", 14.0f);
    _form->cancel  = fude_kit_button(_card, fude_text(FUDE_TEXT_CANCEL), fude_zoom_kanban_on_cancel, _form);
    _form->save    = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_INSERT), fude_zoom_kanban_on_save, _form);
    fude_zoom_kanban_restyle(_form);
    fude_kit_modal_show(&_form->modal, false);
}

void fude_zoom_kanban_forget(fude_zoom_kanban_form* _form) {
    memset(_form, 0, sizeof(*_form));
}

b8 fude_zoom_kanban_shown(const fude_zoom_kanban_form* _form) {
    return _form->modal.card != NULL && _form->modal.shown;
}

void fude_zoom_kanban_restyle(fude_zoom_kanban_form* _form) {
    if(_form->modal.card == NULL) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    fude_kit_modal_restyle(&_form->modal, 16.0f);
    rde_ui_label_set_color(_form->title, _t->text);
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

// The templates' chips, from _left along the card's line at _top (card-local, Y up), a new line when the next
// does not fit before _right. How tall they are (_place false: only measured).
RDE_INTERNAL f32 fude_zoom_kanban_chips(fude_zoom_kanban_form* _form, f32 _left, f32 _top, f32 _right, b8 _place) {
    const f32 _gap = FUDE_ZOOM_KANBAN_GAP, _h = FUDE_ZOOM_KANBAN_CHIP_H;
    f32 _x = _left, _y = 0.0f;
    for(u32 _i = 0; _i < FUDE_ZOOM_KANBAN_TEMPLATES; _i++) {
        const c8* _name = fude_text(FUDE_ZOOM_KANBAN_TEMPLATE_NAMES[_i]);
        const f32 _w = fminf(_right - _left, fmaxf(64.0f, 28.0f + fude_draw_text_width(fude_kit_font(), (f32)FUDE_KIT_FONT_SIZE, _name, FUDE_KIT_TEXT_SCALE * (f32)FUDE_KIT_FONT_SIZE)));
        if(_x > _left && _x + _w > _right + 0.5f) {
            _x  = _left;
            _y += _h + _gap;
        }
        if(_place) {
            fude_kit_place(rde_ui_button_as_node(_form->templates[_i]), (rde_vec_2F){ _x + _w * 0.5f, _top - _y - _h * 0.5f }, (rde_vec_2F){ _w, _h });
        }
        _x += _w + _gap;
    }
    return _y + _h;
}

// The card high on the screen (the keyboard comes up under it): its list as tall as its rows, up to
// what leaves room for the rest above the keyboard.
RDE_INTERNAL void fude_zoom_kanban_layout(fude_zoom_kanban_form* _form) {
    const rde_vec_2F _screen = fude_kit_screen_size(_form->window);
    const f32 _pad = FUDE_ZOOM_KANBAN_PAD, _gap = FUDE_ZOOM_KANBAN_GAP, _rh = FUDE_ZOOM_KANBAN_ROW_H;
    const f32 _w  = fminf(560.0f, _screen.x - 32.0f);
    const f32 _iw = _w - 2.0f * _pad;
    // The templates' label beside their chips (over them when the card is narrow).
    const f32 _lw    = fminf(130.0f, 12.0f + fude_draw_text_width(fude_kit_font(), (f32)FUDE_KIT_FONT_SIZE, fude_text(FUDE_TEXT_ZOOM_KANBAN_TEMPLATES), 15.0f));
    const b8  _above = _iw - _lw < 260.0f;
    const f32 _cl    = _pad + (_above ? 0.0f : _lw);
    const f32 _ch    = fude_zoom_kanban_chips(_form, _cl, 0.0f, _pad + _iw, false) + (_above ? 22.0f + _gap * 0.5f : 0.0f);
    const f32 _fixed = _pad + 28.0f + _gap + _ch + _gap + _gap + 40.0f + _gap + 22.0f + _gap + 44.0f + _pad;
    const f32 _room  = fmaxf(_screen.y * 0.55f - 90.0f - _fixed, _rh + _gap);   // over a keyboard about as tall as half the screen
    const f32 _all   = (f32)_form->rows * (_rh + _gap) - _gap;
    const f32 _lh    = fminf(_all, _room);
    const f32 _h     = _fixed + _lh;
    _form->laid_out = _screen;
    fude_kit_modal_place(&_form->modal, _form->window, (rde_vec_2F){ _screen.x * 0.5f, _screen.y - 90.0f - _h * 0.5f }, (rde_vec_2F){ _w, _h });
    // Top to bottom (card-local: the bottom left the origin).
    f32 _y = _h - _pad;
    fude_kit_place(rde_ui_label_as_node(_form->title), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 14.0f }, (rde_vec_2F){ _iw, 28.0f });
    _y -= 28.0f + _gap;
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
    fude_kit_place(rde_ui_button_as_node(_form->cancel), (rde_vec_2F){ _w - _pad - 120.0f - _gap - 60.0f, _pad + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_form->save), (rde_vec_2F){ _w - _pad - 60.0f, _pad + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
}

void fude_zoom_kanban_open(fude_zoom_kanban_form* _form, fude_zoom_kanban_done _done, void* _self) {
    if(_form->modal.card == NULL) {
        return;
    }
    _form->done = _done;
    _form->self = _self;
    fude_kit_modal_show(&_form->modal, true);
    _form->opened_at = rde_engine_get_time_now();
    fude_zoom_kanban_fill(_form, 0u);   // (the keyboard stays down: the rows are filled in already)
}

void fude_zoom_kanban_close(fude_zoom_kanban_form* _form) {
    if(_form->modal.card != NULL) {
        fude_kit_modal_show(&_form->modal, false);
    }
}

void fude_zoom_kanban_update(fude_zoom_kanban_form* _form) {
    if(!fude_zoom_kanban_shown(_form)) {
        return;
    }
    const rde_vec_2F _screen = fude_kit_screen_size(_form->window);
    if(_screen.x != _form->laid_out.x || _screen.y != _form->laid_out.y) {
        fude_zoom_kanban_layout(_form);
    }
}

void fude_zoom_kanban_look(fude_zoom_kanban_form* _form, u32 _which, b8 _extra, b8 _insert) {
    fude_zoom_kanban_fill(_form, _which);
    if(_extra && _form->rows < FUDE_ZOOM_KANBAN_ROWS) {
        fude_zoom_kanban_set_text(_form->names[_form->rows++], "Ideas");
        fude_zoom_kanban_layout(_form);
    }
    if(_insert) {
        fude_zoom_kanban_on_save(NULL, NULL, _form);
    }
}
