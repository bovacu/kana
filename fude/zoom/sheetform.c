// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/sheetform.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/widgets/draw.h"
#include "drawing/widgets/icons.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FUDE_ZOOM_SHEETF_PAD    16.0f
#define FUDE_ZOOM_SHEETF_GAP    8.0f
#define FUDE_ZOOM_SHEETF_CHIP_H 36.0f
#define FUDE_ZOOM_SHEETF_ROW_H  44.0f
#define FUDE_ZOOM_SHEETF_FIELD_PX 18u

#define FUDE_ZOOM_SHEETF_PAPERS (1u + FUDE_ZOOM_PAPER_COUNT)
#define FUDE_ZOOM_SHEETF_WAYS   FUDE_ZOOM_SHEETF_PAPERS                   // (their refs' first index)
#define FUDE_ZOOM_SHEETF_SCALE0 (FUDE_ZOOM_SHEETF_PAPERS + 2u)

static const f64 FUDE_ZOOM_SHEETF_SCALES[FUDE_ZOOM_SHEET_SCALES] = { 1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0 };
static const c8* const FUDE_ZOOM_SHEETF_SCALE_NAMES[FUDE_ZOOM_SHEET_SCALES] = { "1:1", "1:2", "1:5", "1:10", "1:20", "1:50", "1:100" };

RDE_INTERNAL const c8* fude_zoom_sheetf_paper_name(u32 _i) {
    static const c8* const _a[5] = { "A4", "A3", "A2", "A1", "A0" };
    if(_i == 0u) {
        return fude_text(FUDE_TEXT_ZOOM_SHEET_FREE);
    }
    const u32 _p = _i - 1u;
    return _p < 5u ? _a[_p] : fude_text(_p == FUDE_ZOOM_PAPER_LETTER ? FUDE_TEXT_ZOOM_SHEET_LETTER : FUDE_TEXT_ZOOM_SHEET_TABLOID);
}

// A field's text (a copy, "" when empty), into _out.
RDE_INTERNAL void fude_zoom_sheetf_text(rde_ui_text_editor* _field, c8* _out, usize _size) {
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

RDE_INTERNAL void fude_zoom_sheetf_set_text(fude_zoom_sheet_form* _form, rde_ui_text_editor* _field, const c8* _text) {
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

// A field read as a length (the numpad's way); false: it cannot be.
RDE_INTERNAL b8 fude_zoom_sheetf_read(const fude_zoom_sheet_form* _form, rde_ui_text_editor* _field, f64* _mm) {
    c8 _t[96];
    fude_zoom_sheetf_text(_field, _t, sizeof(_t));
    fude_zoom_units_error _e;
    return fude_zoom_units_length(_t, _form->units.unit, NULL, _mm, &_e) && *_mm > 0.0 && isfinite(*_mm);
}

RDE_INTERNAL void fude_zoom_sheetf_layout(fude_zoom_sheet_form* _form);

// The chips as chosen now, and under the sizes what it prints on (or nothing, when they cannot be read).
RDE_INTERNAL void fude_zoom_sheetf_show(fude_zoom_sheet_form* _form) {
    for(u32 _i = 0; _i < FUDE_ZOOM_SHEETF_PAPERS; _i++) {
        fude_kit_button_chip(_form->papers[_i], _i == 0u ? _form->paper == FUDE_ZOOM_NONE : _form->paper == _i - 1u);
    }
    fude_kit_button_chip(_form->ways[0], !_form->landscape);
    fude_kit_button_chip(_form->ways[1], _form->landscape);
    fude_kit_button_chip(_form->grid, _form->grid_on);
    for(u32 _i = 0; _i < FUDE_ZOOM_SHEET_SCALES; _i++) {
        fude_kit_button_chip(_form->scales[_i], fabs(_form->scale - FUDE_ZOOM_SHEETF_SCALES[_i]) < 1e-9);
    }
    if(_form->problem) {
        return;   // (what could not be read stays said until a size is typed again)
    }
    f64 _w = 0.0, _h = 0.0;
    if(!fude_zoom_sheetf_read(_form, _form->width, &_w) || !fude_zoom_sheetf_read(_form, _form->height, &_h)) {
        rde_ui_label_set_text(_form->hint, "");
        return;
    }
    c8 _ws[FUDE_ZOOM_UNITS_TEXT], _hs[FUDE_ZOOM_UNITS_TEXT], _size[2u * FUDE_ZOOM_UNITS_TEXT + 40u], _say[300];
    fude_zoom_units_format(_w / _form->scale, &_form->units, _ws, sizeof(_ws));
    fude_zoom_units_format(_h / _form->scale, &_form->units, _hs, sizeof(_hs));
    const u32 _paper = fude_zoom_paper_find(_w / _form->scale, _h / _form->scale);
    if(_paper != FUDE_ZOOM_NONE) {
        snprintf(_size, sizeof(_size), "%s \xC3\x97 %s (%s)", _ws, _hs, fude_zoom_sheetf_paper_name(_paper + 1u));
    } else {
        snprintf(_size, sizeof(_size), "%s \xC3\x97 %s", _ws, _hs);
    }
    FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_SHEET_PRINTS, FUDE_TS(_size));
    rde_ui_label_set_text(_form->hint, _say);
    rde_ui_label_set_color(_form->hint, fude_theme_active()->text_soft);
}

// Its paper (the way chosen) times its scale, into the fields.
RDE_INTERNAL void fude_zoom_sheetf_fill(fude_zoom_sheet_form* _form) {
    if(_form->paper == FUDE_ZOOM_NONE) {
        return;
    }
    f64 _w, _h;
    fude_zoom_paper_size(_form->paper, &_w, &_h);
    if(_form->landscape) {
        const f64 _t = _w; _w = _h; _h = _t;
    }
    c8 _ws[FUDE_ZOOM_UNITS_TEXT], _hs[FUDE_ZOOM_UNITS_TEXT];
    fude_zoom_units_format(_w * _form->scale, &_form->units, _ws, sizeof(_ws));
    fude_zoom_units_format(_h * _form->scale, &_form->units, _hs, sizeof(_hs));
    fude_zoom_sheetf_set_text(_form, _form->width, _ws);
    fude_zoom_sheetf_set_text(_form, _form->height, _hs);
    _form->problem = false;
}

// --- presses -------------------------------------------------------------------------------

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_sheetf_on_paper(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_sheet_ref* _ref  = (const fude_zoom_sheet_ref*)_user_data;
    fude_zoom_sheet_form*      _form = _ref->form;
    _form->paper = _ref->index == 0u ? FUDE_ZOOM_NONE : _ref->index - 1u;
    fude_zoom_sheetf_fill(_form);
    fude_zoom_sheetf_show(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Portrait or landscape: a paper's turned; a free size's two sides swapped.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_sheetf_on_way(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_sheet_ref* _ref  = (const fude_zoom_sheet_ref*)_user_data;
    fude_zoom_sheet_form*      _form = _ref->form;
    const b8 _landscape = _ref->index - FUDE_ZOOM_SHEETF_WAYS == 1u;
    if(_landscape == _form->landscape) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    _form->landscape = _landscape;
    if(_form->paper != FUDE_ZOOM_NONE) {
        fude_zoom_sheetf_fill(_form);
    } else {
        c8 _w[96], _h[96];
        fude_zoom_sheetf_text(_form->width, _w, sizeof(_w));
        fude_zoom_sheetf_text(_form->height, _h, sizeof(_h));
        fude_zoom_sheetf_set_text(_form, _form->width, _h);
        fude_zoom_sheetf_set_text(_form, _form->height, _w);
    }
    fude_zoom_sheetf_show(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_sheetf_on_grid(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_sheet_form* _form = (fude_zoom_sheet_form*)_user_data;
    _form->grid_on = !_form->grid_on;
    fude_zoom_sheetf_show(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A scale: a paper's size again at it; a free size stays (only what it prints on changes).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_sheetf_on_scale(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_sheet_ref* _ref  = (const fude_zoom_sheet_ref*)_user_data;
    fude_zoom_sheet_form*      _form = _ref->form;
    _form->scale = FUDE_ZOOM_SHEETF_SCALES[(_ref->index - FUDE_ZOOM_SHEETF_SCALE0) % FUDE_ZOOM_SHEET_SCALES];
    fude_zoom_sheetf_fill(_form);
    fude_zoom_sheetf_show(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A size typed: a free size from now on (the paper's chip let go), what it prints on said again.
RDE_INTERNAL void fude_zoom_sheetf_on_change(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    fude_zoom_sheet_form* _form = (fude_zoom_sheet_form*)_user_data;
    if(_form == NULL || _form->setting) {
        return;
    }
    _form->paper   = FUDE_ZOOM_NONE;
    _form->problem = false;
    fude_zoom_sheetf_show(_form);
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_sheetf_on_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_sheet_close((fude_zoom_sheet_form*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A tap off the card: put away — not the release of the press that opened it.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_sheetf_on_dismiss(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_sheet_form* _form = (fude_zoom_sheet_form*)_user_data;
    if(rde_engine_get_time_now() - _form->opened_at >= 0.6) {
        fude_zoom_sheet_close(_form);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Insert (Done): both sizes read, or the one that cannot be keeps the card up, said.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_sheetf_on_save(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_sheet_form* _form = (fude_zoom_sheet_form*)_user_data;
    f64 _w = 0.0, _h = 0.0;
    const b8 _w_ok = fude_zoom_sheetf_read(_form, _form->width, &_w);
    const b8 _h_ok = _w_ok && fude_zoom_sheetf_read(_form, _form->height, &_h);
    if(!_w_ok || !_h_ok) {
        c8 _t[96], _say[200];
        fude_zoom_sheetf_text(!_w_ok ? _form->width : _form->height, _t, sizeof(_t));
        FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_SHEET_BAD, FUDE_TS(_t[0] != 0 ? _t : "?"));
        rde_ui_label_set_text(_form->hint, _say);
        rde_ui_label_set_color(_form->hint, fude_theme_active()->score_poor);
        _form->problem = true;
        rde_ui_node_focus(rde_ui_text_editor_as_node(!_w_ok ? _form->width : _form->height));
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    const fude_zoom_sheet_done _done  = _form->done;
    void* const                _self  = _form->self;
    const f64                  _scale = _form->scale;
    const u32                  _flags = _form->grid_on ? FUDE_ZOOM_SHEET_GRID : 0u;
    fude_zoom_sheet_close(_form);
    if(_done != NULL) {
        _done(_self, _w, _h, _scale, _flags);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Enter in the width: on to the height; in the height: Insert.
RDE_INTERNAL void fude_zoom_sheetf_on_submit(rde_ui_node* _node, any _user_data) {
    fude_zoom_sheet_form* _form = (fude_zoom_sheet_form*)_user_data;
    if(_form == NULL) {
        return;
    }
    if(_node == rde_ui_text_editor_as_node(_form->width)) {
        rde_ui_node_focus(rde_ui_text_editor_as_node(_form->height));
    } else {
        fude_zoom_sheetf_on_save(NULL, NULL, _form);
    }
}

// --- built, laid out --------------------------------------------------------------------------

RDE_INTERNAL rde_ui_label* fude_zoom_sheetf_label(fude_zoom_sheet_form* _form, rde_ui_node* _parent, const c8* _text, f32 _px) {
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

RDE_INTERNAL rde_ui_text_editor* fude_zoom_sheetf_field(fude_zoom_sheet_form* _form, rde_ui_node* _parent, FUDE_TEXT_ _placeholder) {
    rde_ui_text_editor* _f = rde_ui_text_editor_create(_form->font, NULL);
    rde_ui_text_editor_set_multiline(_f, false);
    rde_ui_text_editor_set_allow_multicursor(_f, false);
    rde_ui_text_editor_set_font_size(_f, FUDE_ZOOM_SHEETF_FIELD_PX);
    rde_ui_text_editor_set_max_chars(_f, 32u);
    rde_ui_text_editor_set_content_insets(_f, 10.0f, 8.0f, 10.0f, 8.0f);
    rde_ui_text_editor_set_placeholder(_f, fude_text(_placeholder));
    rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_f), (any)_form);
    rde_ui_text_editor_set_on_change(_f, fude_zoom_sheetf_on_change);
    rde_ui_text_editor_set_on_submit(_f, fude_zoom_sheetf_on_submit);
    fude_kit_field_box(_parent, _f);
    return _f;
}

void fude_zoom_sheet_build(fude_zoom_sheet_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font) {
    memset(_form, 0, sizeof(*_form));
    _form->window = _window;
    _form->font   = _font;
    _form->scale  = 1.0;
    _form->paper  = FUDE_ZOOM_NONE;
    fude_kit_modal_create(&_form->modal, _root, fude_zoom_sheetf_on_dismiss, _form);
    rde_ui_node* _card = rde_ui_image_as_node(_form->modal.card);
    _form->title       = fude_zoom_sheetf_label(_form, _card, "", 17.0f);
    _form->paper_label = fude_zoom_sheetf_label(_form, _card, fude_text(FUDE_TEXT_ZOOM_SHEET_PAPER), 15.0f);
    _form->scale_label = fude_zoom_sheetf_label(_form, _card, fude_text(FUDE_TEXT_ZOOM_SHEET_SCALE), 15.0f);
    for(u32 _i = 0; _i < FUDE_ZOOM_SHEETF_PAPERS; _i++) {
        _form->refs[_i] = (fude_zoom_sheet_ref){ _form, _i };
        _form->papers[_i] = fude_kit_button(_card, fude_zoom_sheetf_paper_name(_i), fude_zoom_sheetf_on_paper, &_form->refs[_i]);
    }
    for(u32 _i = 0; _i < 2u; _i++) {
        _form->refs[FUDE_ZOOM_SHEETF_WAYS + _i] = (fude_zoom_sheet_ref){ _form, FUDE_ZOOM_SHEETF_WAYS + _i };
        _form->ways[_i] = fude_kit_button(_card, fude_text(_i == 0u ? FUDE_TEXT_ZOOM_SHEET_PORTRAIT : FUDE_TEXT_ZOOM_SHEET_LANDSCAPE), fude_zoom_sheetf_on_way,
                                          &_form->refs[FUDE_ZOOM_SHEETF_WAYS + _i]);
    }
    _form->grid = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_SHEET_GRID), fude_zoom_sheetf_on_grid, _form);
    for(u32 _i = 0; _i < FUDE_ZOOM_SHEET_SCALES; _i++) {
        _form->refs[FUDE_ZOOM_SHEETF_SCALE0 + _i] = (fude_zoom_sheet_ref){ _form, FUDE_ZOOM_SHEETF_SCALE0 + _i };
        _form->scales[_i] = fude_kit_button(_card, FUDE_ZOOM_SHEETF_SCALE_NAMES[_i], fude_zoom_sheetf_on_scale, &_form->refs[FUDE_ZOOM_SHEETF_SCALE0 + _i]);
    }
    _form->width  = fude_zoom_sheetf_field(_form, _card, FUDE_TEXT_ZOOM_PART_WIDTH);
    _form->height = fude_zoom_sheetf_field(_form, _card, FUDE_TEXT_ZOOM_PART_HEIGHT);
    _form->times  = fude_zoom_sheetf_label(_form, _card, "\xC3\x97", 18.0f);
    rde_ui_label_set_alignment(_form->times, RDE_UI_LABEL_H_ALIGN_CENTER, RDE_UI_LABEL_V_ALIGN_MIDDLE);
    _form->hint   = fude_zoom_sheetf_label(_form, _card, "", 14.0f);
    _form->cancel = fude_kit_button(_card, fude_text(FUDE_TEXT_CANCEL), fude_zoom_sheetf_on_cancel, _form);
    _form->save   = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_INSERT), fude_zoom_sheetf_on_save, _form);
    fude_zoom_sheet_restyle(_form);
    fude_kit_modal_show(&_form->modal, false);
}

void fude_zoom_sheet_forget(fude_zoom_sheet_form* _form) {
    memset(_form, 0, sizeof(*_form));
}

b8 fude_zoom_sheet_shown(const fude_zoom_sheet_form* _form) {
    return _form->modal.card != NULL && _form->modal.shown;
}

void fude_zoom_sheet_restyle(fude_zoom_sheet_form* _form) {
    if(_form->modal.card == NULL) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    fude_kit_modal_restyle(&_form->modal, 16.0f);
    rde_ui_label_set_color(_form->title, _t->text);
    rde_ui_label_set_color(_form->paper_label, _t->text_soft);
    rde_ui_label_set_color(_form->scale_label, _t->text_soft);
    rde_ui_label_set_color(_form->times, _t->text_soft);
    rde_ui_label_set_color(_form->hint, _form->problem ? _t->score_poor : _t->text_soft);
    fude_kit_style_field(_form->width);
    fude_kit_style_field(_form->height);
    fude_kit_button_plain(_form->cancel);
    fude_kit_button_primary(_form->save);
    fude_zoom_sheetf_show(_form);
}

// Chips from _left along the card's line at _top (card-local, Y up), a new line when the next does not fit
// before _right. How tall they are (_place false: only measured).
RDE_INTERNAL f32 fude_zoom_sheetf_chips(rde_ui_button* const* _chips, const c8* const* _names, u32 _n, f32 _left, f32 _top, f32 _right, b8 _place) {
    const f32 _gap = FUDE_ZOOM_SHEETF_GAP, _h = FUDE_ZOOM_SHEETF_CHIP_H;
    f32 _x = _left, _y = 0.0f;
    for(u32 _i = 0; _i < _n; _i++) {
        const f32 _w = fminf(_right - _left, fmaxf(52.0f, 26.0f + fude_draw_text_width(fude_kit_font(), (f32)FUDE_KIT_FONT_SIZE, _names[_i], FUDE_KIT_TEXT_SCALE * (f32)FUDE_KIT_FONT_SIZE)));
        if(_x > _left && _x + _w > _right + 0.5f) {
            _x  = _left;
            _y += _h + _gap;
        }
        if(_place) {
            fude_kit_place(rde_ui_button_as_node(_chips[_i]), (rde_vec_2F){ _x + _w * 0.5f, _top - _y - _h * 0.5f }, (rde_vec_2F){ _w, _h });
        }
        _x += _w + _gap;
    }
    return _y + _h;
}

// The card high on the screen (the keyboard comes up under it).
RDE_INTERNAL void fude_zoom_sheetf_layout(fude_zoom_sheet_form* _form) {
    const rde_vec_2F _screen = fude_kit_screen_size(_form->window);
    const f32 _pad = FUDE_ZOOM_SHEETF_PAD, _gap = FUDE_ZOOM_SHEETF_GAP, _ch = FUDE_ZOOM_SHEETF_CHIP_H;
    const f32 _w  = fminf(580.0f, _screen.x - 32.0f);
    const f32 _iw = _w - 2.0f * _pad;
    // The rows' labels beside their chips (over them when the card is narrow).
    const f32 _lw = fminf(120.0f, 14.0f + fmaxf(fude_draw_text_width(fude_kit_font(), (f32)FUDE_KIT_FONT_SIZE, fude_text(FUDE_TEXT_ZOOM_SHEET_PAPER), 15.0f),
                                                fude_draw_text_width(fude_kit_font(), (f32)FUDE_KIT_FONT_SIZE, fude_text(FUDE_TEXT_ZOOM_SHEET_SCALE), 15.0f)));
    const b8  _above = _iw - _lw < 300.0f;
    const f32 _cl    = _pad + (_above ? 0.0f : _lw);
    const f32 _right = _pad + _iw;
    const c8* _paper_names[FUDE_ZOOM_SHEETF_PAPERS];
    for(u32 _i = 0; _i < FUDE_ZOOM_SHEETF_PAPERS; _i++) {
        _paper_names[_i] = fude_zoom_sheetf_paper_name(_i);
    }
    const c8* _way_names[2] = { fude_text(FUDE_TEXT_ZOOM_SHEET_PORTRAIT), fude_text(FUDE_TEXT_ZOOM_SHEET_LANDSCAPE) };
    const f32 _grid_w = fminf(fmaxf(80.0f, 30.0f + fude_draw_text_width(fude_kit_font(), (f32)FUDE_KIT_FONT_SIZE, fude_text(FUDE_TEXT_ZOOM_SHEET_GRID), FUDE_KIT_TEXT_SCALE * (f32)FUDE_KIT_FONT_SIZE)), 180.0f);
    const f32 _label_h = _above ? 22.0f + _gap * 0.5f : 0.0f;
    const f32 _ph = fude_zoom_sheetf_chips(_form->papers, _paper_names, FUDE_ZOOM_SHEETF_PAPERS, _cl, 0.0f, _right, false) + _label_h;
    const f32 _wh = fude_zoom_sheetf_chips(_form->ways, _way_names, 2u, _cl, 0.0f, _right - _grid_w - _gap, false);
    const f32 _sh = fude_zoom_sheetf_chips(_form->scales, FUDE_ZOOM_SHEETF_SCALE_NAMES, FUDE_ZOOM_SHEET_SCALES, _cl, 0.0f, _right, false) + _label_h;
    const f32 _h  = _pad + 28.0f + _gap + _ph + _gap + _wh + _gap + _sh + _gap + FUDE_ZOOM_SHEETF_ROW_H + _gap + 22.0f + _gap + 44.0f + _pad;
    _form->laid_out = _screen;
    fude_kit_modal_place(&_form->modal, _form->window, (rde_vec_2F){ _screen.x * 0.5f, fmaxf(_screen.y - 90.0f - _h * 0.5f, _h * 0.5f + 8.0f) }, (rde_vec_2F){ _w, _h });
    // Top to bottom (card-local: the bottom left the origin).
    f32 _y = _h - _pad;
    fude_kit_place(rde_ui_label_as_node(_form->title), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 14.0f }, (rde_vec_2F){ _iw, 28.0f });
    _y -= 28.0f + _gap;
    if(_above) {
        fude_kit_place(rde_ui_label_as_node(_form->paper_label), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 11.0f }, (rde_vec_2F){ _iw, 22.0f });
        fude_zoom_sheetf_chips(_form->papers, _paper_names, FUDE_ZOOM_SHEETF_PAPERS, _cl, _y - _label_h, _right, true);
    } else {
        fude_kit_place(rde_ui_label_as_node(_form->paper_label), (rde_vec_2F){ _pad + _lw * 0.5f, _y - _ch * 0.5f }, (rde_vec_2F){ _lw, _ch });
        fude_zoom_sheetf_chips(_form->papers, _paper_names, FUDE_ZOOM_SHEETF_PAPERS, _cl, _y, _right, true);
    }
    _y -= _ph + _gap;
    fude_zoom_sheetf_chips(_form->ways, _way_names, 2u, _cl, _y, _right - _grid_w - _gap, true);
    fude_kit_place(rde_ui_button_as_node(_form->grid), (rde_vec_2F){ _right - _grid_w * 0.5f, _y - _ch * 0.5f }, (rde_vec_2F){ _grid_w, _ch });
    _y -= _wh + _gap;
    if(_above) {
        fude_kit_place(rde_ui_label_as_node(_form->scale_label), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 11.0f }, (rde_vec_2F){ _iw, 22.0f });
        fude_zoom_sheetf_chips(_form->scales, FUDE_ZOOM_SHEETF_SCALE_NAMES, FUDE_ZOOM_SHEET_SCALES, _cl, _y - _label_h, _right, true);
    } else {
        fude_kit_place(rde_ui_label_as_node(_form->scale_label), (rde_vec_2F){ _pad + _lw * 0.5f, _y - _ch * 0.5f }, (rde_vec_2F){ _lw, _ch });
        fude_zoom_sheetf_chips(_form->scales, FUDE_ZOOM_SHEETF_SCALE_NAMES, FUDE_ZOOM_SHEET_SCALES, _cl, _y, _right, true);
    }
    _y -= _sh + _gap;
    const f32 _xw = 28.0f, _fw = (_iw - _xw - 2.0f * _gap) * 0.5f, _ry = _y - FUDE_ZOOM_SHEETF_ROW_H * 0.5f;
    fude_kit_place(fude_kit_field_node(_form->width), (rde_vec_2F){ _pad + _fw * 0.5f, _ry }, (rde_vec_2F){ _fw, FUDE_ZOOM_SHEETF_ROW_H });
    fude_kit_place(rde_ui_label_as_node(_form->times), (rde_vec_2F){ _pad + _fw + _gap + _xw * 0.5f, _ry }, (rde_vec_2F){ _xw, FUDE_ZOOM_SHEETF_ROW_H });
    fude_kit_place(fude_kit_field_node(_form->height), (rde_vec_2F){ _pad + _fw + 2.0f * _gap + _xw + _fw * 0.5f, _ry }, (rde_vec_2F){ _fw, FUDE_ZOOM_SHEETF_ROW_H });
    _y -= FUDE_ZOOM_SHEETF_ROW_H + _gap;
    fude_kit_place(rde_ui_label_as_node(_form->hint), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 11.0f }, (rde_vec_2F){ _iw, 22.0f });
    fude_kit_place(rde_ui_button_as_node(_form->cancel), (rde_vec_2F){ _w - _pad - 120.0f - _gap - 60.0f, _pad + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_form->save), (rde_vec_2F){ _w - _pad - 60.0f, _pad + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
}

void fude_zoom_sheet_open(fude_zoom_sheet_form* _form, const c8* _title, b8 _new, f64 _w_mm, f64 _h_mm, f64 _scale, u32 _flags,
                          const fude_zoom_units_style* _units, fude_zoom_sheet_done _done, void* _self) {
    if(_form->modal.card == NULL) {
        return;
    }
    _form->done      = _done;
    _form->self      = _self;
    _form->units     = *_units;
    _form->scale     = _scale >= 1.0 ? _scale : 1.0;
    _form->grid_on   = (_flags & FUDE_ZOOM_SHEET_GRID) != 0u;
    _form->landscape = _w_mm > _h_mm;
    _form->problem   = false;
    _form->paper     = fude_zoom_paper_find(_w_mm / _form->scale, _h_mm / _form->scale);
    c8 _ws[FUDE_ZOOM_UNITS_TEXT], _hs[FUDE_ZOOM_UNITS_TEXT];
    fude_zoom_units_format(_w_mm, _units, _ws, sizeof(_ws));
    fude_zoom_units_format(_h_mm, _units, _hs, sizeof(_hs));
    fude_zoom_sheetf_set_text(_form, _form->width, _ws);
    fude_zoom_sheetf_set_text(_form, _form->height, _hs);
    rde_ui_label_set_text(_form->title, _title);
    rde_ui_button_set_text(_form->save, fude_text(_new ? FUDE_TEXT_ZOOM_INSERT : FUDE_TEXT_ZOOM_SHEET_DONE));
    fude_kit_modal_show(&_form->modal, true);
    _form->opened_at = rde_engine_get_time_now();
    fude_zoom_sheetf_layout(_form);
    fude_zoom_sheet_restyle(_form);   // (the chips as chosen; the keyboard stays down: a size tapped brings it)
}

void fude_zoom_sheet_close(fude_zoom_sheet_form* _form) {
    if(_form->modal.card != NULL) {
        fude_kit_modal_show(&_form->modal, false);
    }
}

void fude_zoom_sheet_update(fude_zoom_sheet_form* _form) {
    if(!fude_zoom_sheet_shown(_form)) {
        return;
    }
    const rde_vec_2F _screen = fude_kit_screen_size(_form->window);
    if(_screen.x != _form->laid_out.x || _screen.y != _form->laid_out.y) {
        fude_zoom_sheetf_layout(_form);
    }
}

void fude_zoom_sheet_look(fude_zoom_sheet_form* _form, u32 _paper, b8 _landscape, u32 _scale, const c8* _w, const c8* _h, b8 _save) {
    if(_scale < FUDE_ZOOM_SHEET_SCALES) {
        fude_zoom_sheetf_on_scale(NULL, NULL, &_form->refs[FUDE_ZOOM_SHEETF_SCALE0 + _scale]);
    }
    if(_paper != FUDE_ZOOM_NONE && _paper < FUDE_ZOOM_PAPER_COUNT) {
        fude_zoom_sheetf_on_paper(NULL, NULL, &_form->refs[1u + _paper]);
    }
    fude_zoom_sheetf_on_way(NULL, NULL, &_form->refs[FUDE_ZOOM_SHEETF_WAYS + (_landscape ? 1u : 0u)]);
    if(_w != NULL) {
        fude_zoom_sheetf_set_text(_form, _form->width, _w);
        fude_zoom_sheetf_on_change(NULL, _form);
    }
    if(_h != NULL) {
        fude_zoom_sheetf_set_text(_form, _form->height, _h);
        fude_zoom_sheetf_on_change(NULL, _form);
    }
    if(_save) {
        fude_zoom_sheetf_on_save(NULL, NULL, _form);
    }
}
