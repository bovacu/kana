// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/sheetform.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/base/draw.h"
#include "drawing/base/icons.h"

// ===========================================================================
// See sheetform.h.
//
// The card is the kit's form (kit.h: its title, Cancel, a tap off it, laying it out again, its fields, chips and what
// could not be read). Internal names start fude_zoom_sheetf_. In order: the fields' text and how it is read, the card's
// state shown on its widgets (fude_zoom_sheetf_show, after every change), the presses, building and styling, the
// layout, and the public calls. The sizes live only in the two fields, as text: they are read again
// (fude_zoom_units_length) whenever they are needed, and a paper or a scale chosen writes new text into them.
// ===========================================================================

// The card's measures, in points (its margin and gaps are the kit's form's).
#define FUDE_ZOOM_SHEETF_CHIP_H 36.0f   // a chip's height
#define FUDE_ZOOM_SHEETF_ROW_H  44.0f   // the sizes' row

// The chips' places in the card's refs: the papers (free size first), then the two ways up, then the scales.
#define FUDE_ZOOM_SHEETF_PAPERS (1u + FUDE_ZOOM_PAPER_COUNT)
#define FUDE_ZOOM_SHEETF_WAYS   FUDE_ZOOM_SHEETF_PAPERS                   // (their refs' first index)
#define FUDE_ZOOM_SHEETF_SCALE0 (FUDE_ZOOM_SHEETF_PAPERS + 2u)

// The scales offered, N of 1:N, and their chips' names.
static const f64 FUDE_ZOOM_SHEETF_SCALES[FUDE_ZOOM_SHEET_SCALES] = { 1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0 };
static const c8* const FUDE_ZOOM_SHEETF_SCALE_NAMES[FUDE_ZOOM_SHEET_SCALES] = { "1:1", "1:2", "1:5", "1:10", "1:20", "1:50", "1:100" };

// Returns paper chip _i's name: 0 the free size, then each FUDE_ZOOM_PAPER_ (the A sizes' names are the same in
// every language; Letter's and Tabloid's are the app's text).
RDE_INTERNAL const c8* fude_zoom_sheetf_paper_name(u32 _i) {
    static const c8* const _a[5] = { "A4", "A3", "A2", "A1", "A0" };
    if(_i == 0u) {
        return fude_text(FUDE_TEXT_ZOOM_SHEET_FREE);
    }
    const u32 _p = _i - 1u;
    return _p < 5u ? _a[_p] : fude_text(_p == FUDE_ZOOM_PAPER_LETTER ? FUDE_TEXT_ZOOM_SHEET_LETTER : FUDE_TEXT_ZOOM_SHEET_TABLOID);
}

// Reads a field as a length in millimetres (units.h, as the numpad reads one, in the canvas's unit when none is
// typed) into *_mm. Returns false when it cannot be read, or is not a finite length above 0.
RDE_INTERNAL b8 fude_zoom_sheetf_read(const fude_zoom_sheet_form* _form, rde_ui_text_editor* _field, f64* _mm) {
    c8 _t[96];
    fude_kit_field_text(_field, _t, sizeof(_t));
    fude_zoom_units_error _e;
    return fude_zoom_units_length(_t, _form->units.unit, NULL, _mm, &_e) && *_mm > 0.0 && rde_math_is_finite_f64(*_mm);
}

RDE_INTERNAL void fude_zoom_sheetf_layout(fude_zoom_sheet_form* _form);

// Shows the chips as chosen now, and under the sizes what it prints on (the size divided by the scale, and the paper
// that is, if any), or nothing when they cannot be read. While the hint says what could not be read, it is left.
RDE_INTERNAL void fude_zoom_sheetf_show(fude_zoom_sheet_form* _form) {
    for(u32 _i = 0; _i < FUDE_ZOOM_SHEETF_PAPERS; _i++) {
        fude_kit_button_chip(_form->papers[_i], _i == 0u ? _form->paper == FUDE_ZOOM_NONE : _form->paper == _i - 1u);
    }
    fude_kit_button_chip(_form->ways[0], !_form->landscape);
    fude_kit_button_chip(_form->ways[1], _form->landscape);
    fude_kit_button_chip(_form->grid, _form->grid_on);
    for(u32 _i = 0; _i < FUDE_ZOOM_SHEET_SCALES; _i++) {
        fude_kit_button_chip(_form->scales[_i], rde_math_abs_f64(_form->scale - FUDE_ZOOM_SHEETF_SCALES[_i]) < 1e-9);
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
        rde_snprintf(_size, sizeof(_size), "%s \xC3\x97 %s (%s)", _ws, _hs, fude_zoom_sheetf_paper_name(_paper + 1u));
    } else {
        rde_snprintf(_size, sizeof(_size), "%s \xC3\x97 %s", _ws, _hs);
    }
    FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_SHEET_PRINTS, FUDE_TS(_size));
    rde_ui_label_set_text(_form->hint, _say);
    rde_ui_label_set_color(_form->hint, fude_theme_active()->text_soft);
}

// Writes its paper's size, the way up chosen, times its scale into the fields, which clears any problem said. Nothing
// for a free size.
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
    fude_kit_form_set(&_form->card, _form->width, _ws);
    fude_kit_form_set(&_form->card, _form->height, _hs);
    _form->problem = false;
}

// --- presses -------------------------------------------------------------------------------

// A paper's chip: the sizes become that paper's at the scale; the free size's chip leaves them as they are.
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
        fude_kit_field_text(_form->width, _w, sizeof(_w));
        fude_kit_field_text(_form->height, _h, sizeof(_h));
        fude_kit_form_set(&_form->card, _form->width, _h);
        fude_kit_form_set(&_form->card, _form->height, _w);
    }
    fude_zoom_sheetf_show(_form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Grid: on or off.
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
    if(_form == NULL || _form->card.setting) {
        return;
    }
    _form->paper   = FUDE_ZOOM_NONE;
    _form->problem = false;
    fude_zoom_sheetf_show(_form);
}

// Insert (Done): with both sizes read, the card is put away and the callback given the sheet. Otherwise the card stays
// up, the hint says which text could not be read (the width's first), and that field takes the keyboard.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_sheetf_on_save(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_sheet_form* _form = (fude_zoom_sheet_form*)_user_data;
    f64 _w = 0.0, _h = 0.0;
    const b8 _w_ok = fude_zoom_sheetf_read(_form, _form->width, &_w);
    const b8 _h_ok = _w_ok && fude_zoom_sheetf_read(_form, _form->height, &_h);
    if(!_w_ok || !_h_ok) {
        fude_kit_form_bad(_form->hint, !_w_ok ? _form->width : _form->height, FUDE_TEXT_ZOOM_SHEET_BAD);
        rde_ui_label_set_color(_form->hint, fude_theme_active()->score_poor);
        _form->problem = true;
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    const fude_zoom_sheet_done _done  = _form->done;
    any const                _self  = _form->self;
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

// Makes a one-line size field (32 characters at most) in a box on the card, its placeholder _placeholder; its
// changes and Enter come to the card's handlers.
RDE_INTERNAL rde_ui_text_editor* fude_zoom_sheetf_field(fude_zoom_sheet_form* _form, rde_ui_node* _parent, FUDE_TEXT_ _placeholder) {
    return fude_kit_form_field(&_form->card, _parent, 32u, fude_text(_placeholder), (any)_form, fude_zoom_sheetf_on_change, fude_zoom_sheetf_on_submit);
}

// See the header. It starts at 1:1 and a free size.
void fude_zoom_sheet_build(fude_zoom_sheet_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font) {
    rde_memset(_form, 0, sizeof(*_form));
    _form->scale  = 1.0;
    _form->paper  = FUDE_ZOOM_NONE;
    rde_ui_node* _card = fude_kit_form_build(&_form->card, _root, _window, _font, "");
    _form->paper_label = fude_kit_form_label(&_form->card, _card, fude_text(FUDE_TEXT_ZOOM_SHEET_PAPER), 15.0f);
    _form->scale_label = fude_kit_form_label(&_form->card, _card, fude_text(FUDE_TEXT_ZOOM_SHEET_SCALE), 15.0f);
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
    _form->times  = fude_kit_form_label(&_form->card, _card, "\xC3\x97", 18.0f);
    rde_ui_label_set_alignment(_form->times, RDE_UI_LABEL_H_ALIGN_CENTER, RDE_UI_LABEL_V_ALIGN_MIDDLE);
    _form->hint   = fude_kit_form_label(&_form->card, _card, "", 14.0f);
    _form->cancel = fude_kit_button(_card, fude_text(FUDE_TEXT_CANCEL), fude_kit_form_on_cancel, &_form->card);
    _form->save   = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_INSERT), fude_zoom_sheetf_on_save, _form);
    fude_zoom_sheet_restyle(_form);
    fude_kit_modal_show(&_form->card.modal, false);
}

// See the header.
void fude_zoom_sheet_forget(fude_zoom_sheet_form* _form) {
    rde_memset(_form, 0, sizeof(*_form));
}

// See the header. False too before it is built or after it is forgotten.
b8 fude_zoom_sheet_shown(const fude_zoom_sheet_form* _form) {
    return fude_kit_form_shown(&_form->card);
}

// See the header. Shows the card's state again too, since a chip's colour says whether it is chosen.
void fude_zoom_sheet_restyle(fude_zoom_sheet_form* _form) {
    if(!fude_kit_form_restyle(&_form->card)) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
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

// Places chips from _left along the card's line at _top (card-local, Y up), each as wide as its name and 26 points
// (52 at least), starting a new line when the next does not fit before _right (kit.h: fude_kit_form_chips). Returns
// how tall they are; with _place false they are only measured.
RDE_INTERNAL f32 fude_zoom_sheetf_chips(rde_ui_button* const* _chips, const c8* const* _names, u32 _n, f32 _left, f32 _top, f32 _right, b8 _place) {
    return fude_kit_form_chips(_chips, _names, _n, 52.0f, 26.0f, FUDE_ZOOM_SHEETF_CHIP_H, _left, _top, _right, _place);
}

// Lays the card out high on the screen (the keyboard comes up under it), at most 580 points wide, from the top down:
// the title, the papers, the ways up with Grid at their right, the scales, the sizes, the hint, and Cancel and Insert.
// Its height is what its rows need.
RDE_INTERNAL void fude_zoom_sheetf_layout(fude_zoom_sheet_form* _form) {
    const rde_vec_2F _screen = fude_kit_screen_size(_form->card.window);
    const f32 _pad = FUDE_KIT_FORM_PAD, _gap = FUDE_KIT_FORM_GAP, _ch = FUDE_ZOOM_SHEETF_CHIP_H;
    const f32 _w  = rde_math_min_f32(580.0f, _screen.x - 32.0f);
    const f32 _iw = _w - 2.0f * _pad;
    // The rows' labels beside their chips (over them when the card is narrow).
    const f32 _lw = rde_math_min_f32(120.0f, 14.0f + rde_math_max_f32(fude_draw_text_width(fude_kit_font(), (f32)FUDE_KIT_FONT_SIZE, fude_text(FUDE_TEXT_ZOOM_SHEET_PAPER), 15.0f),
                                                fude_draw_text_width(fude_kit_font(), (f32)FUDE_KIT_FONT_SIZE, fude_text(FUDE_TEXT_ZOOM_SHEET_SCALE), 15.0f)));
    const b8  _above = _iw - _lw < 300.0f;
    const f32 _cl    = _pad + (_above ? 0.0f : _lw);
    const f32 _right = _pad + _iw;
    const c8* _paper_names[FUDE_ZOOM_SHEETF_PAPERS];
    for(u32 _i = 0; _i < FUDE_ZOOM_SHEETF_PAPERS; _i++) {
        _paper_names[_i] = fude_zoom_sheetf_paper_name(_i);
    }
    const c8* _way_names[2] = { fude_text(FUDE_TEXT_ZOOM_SHEET_PORTRAIT), fude_text(FUDE_TEXT_ZOOM_SHEET_LANDSCAPE) };
    const f32 _grid_w = rde_math_min_f32(rde_math_max_f32(80.0f, 30.0f + fude_draw_text_width(fude_kit_font(), (f32)FUDE_KIT_FONT_SIZE, fude_text(FUDE_TEXT_ZOOM_SHEET_GRID), FUDE_KIT_TEXT_SCALE * (f32)FUDE_KIT_FONT_SIZE)), 180.0f);
    const f32 _label_h = _above ? 22.0f + _gap * 0.5f : 0.0f;
    const f32 _ph = fude_zoom_sheetf_chips(_form->papers, _paper_names, FUDE_ZOOM_SHEETF_PAPERS, _cl, 0.0f, _right, false) + _label_h;
    const f32 _wh = fude_zoom_sheetf_chips(_form->ways, _way_names, 2u, _cl, 0.0f, _right - _grid_w - _gap, false);
    const f32 _sh = fude_zoom_sheetf_chips(_form->scales, FUDE_ZOOM_SHEETF_SCALE_NAMES, FUDE_ZOOM_SHEET_SCALES, _cl, 0.0f, _right, false) + _label_h;
    const f32 _h  = _pad + 28.0f + _gap + _ph + _gap + _wh + _gap + _sh + _gap + FUDE_ZOOM_SHEETF_ROW_H + _gap + 22.0f + _gap + 44.0f + _pad;
    // Top to bottom (card-local: the bottom left the origin), from under the title.
    f32 _y = fude_kit_form_place(&_form->card, _screen, FUDE_KIT_FORM_HIGH_KEPT, _w, _h);
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
    fude_kit_place(rde_ui_text_editor_as_node(_form->width), (rde_vec_2F){ _pad + _fw * 0.5f, _ry }, (rde_vec_2F){ _fw, FUDE_ZOOM_SHEETF_ROW_H });
    fude_kit_place(rde_ui_label_as_node(_form->times), (rde_vec_2F){ _pad + _fw + _gap + _xw * 0.5f, _ry }, (rde_vec_2F){ _xw, FUDE_ZOOM_SHEETF_ROW_H });
    fude_kit_place(rde_ui_text_editor_as_node(_form->height), (rde_vec_2F){ _pad + _fw + 2.0f * _gap + _xw + _fw * 0.5f, _ry }, (rde_vec_2F){ _fw, FUDE_ZOOM_SHEETF_ROW_H });
    _y -= FUDE_ZOOM_SHEETF_ROW_H + _gap;
    fude_kit_place(rde_ui_label_as_node(_form->hint), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 11.0f }, (rde_vec_2F){ _iw, 22.0f });
    fude_kit_form_buttons(_form->cancel, _form->save, _w);
}

// See the header. Its way up is landscape when the sheet is wider than tall; a scale under 1 opens as 1:1.
void fude_zoom_sheet_open(fude_zoom_sheet_form* _form, const c8* _title, b8 _new, f64 _w_mm, f64 _h_mm, f64 _scale, u32 _flags,
                          const fude_zoom_units_style* _units, fude_zoom_sheet_done _done, any _self) {
    if(_form->card.modal.card == NULL) {
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
    fude_kit_form_set(&_form->card, _form->width, _ws);
    fude_kit_form_set(&_form->card, _form->height, _hs);
    rde_ui_label_set_text(_form->card.title, _title);
    rde_ui_button_set_text(_form->save, fude_text(_new ? FUDE_TEXT_ZOOM_INSERT : FUDE_TEXT_ZOOM_SHEET_DONE));
    fude_kit_form_show(&_form->card);
    fude_zoom_sheetf_layout(_form);
    fude_zoom_sheet_restyle(_form);   // (the chips as chosen; the keyboard stays down: a size tapped brings it)
}

// See the header.
void fude_zoom_sheet_close(fude_zoom_sheet_form* _form) {
    fude_kit_form_close(&_form->card);
}

// See the header.
void fude_zoom_sheet_update(fude_zoom_sheet_form* _form) {
    if(fude_kit_form_resized(&_form->card)) {
        fude_zoom_sheetf_layout(_form);
    }
}

// See the header. It calls the card's own handlers, as presses would, and does nothing while the card is not up.
void fude_zoom_sheet_look(fude_zoom_sheet_form* _form, u32 _paper, b8 _landscape, u32 _scale, const c8* _w, const c8* _h, b8 _save) {
    if(!fude_zoom_sheet_shown(_form)) {
        return;
    }
    if(_scale < FUDE_ZOOM_SHEET_SCALES) {
        fude_zoom_sheetf_on_scale(NULL, NULL, &_form->refs[FUDE_ZOOM_SHEETF_SCALE0 + _scale]);
    }
    if(_paper != FUDE_ZOOM_NONE && _paper < FUDE_ZOOM_PAPER_COUNT) {
        fude_zoom_sheetf_on_paper(NULL, NULL, &_form->refs[1u + _paper]);
    }
    fude_zoom_sheetf_on_way(NULL, NULL, &_form->refs[FUDE_ZOOM_SHEETF_WAYS + (_landscape ? 1u : 0u)]);
    if(_w != NULL) {
        fude_kit_form_set(&_form->card, _form->width, _w);
        fude_zoom_sheetf_on_change(NULL, _form);
    }
    if(_h != NULL) {
        fude_kit_form_set(&_form->card, _form->height, _h);
        fude_zoom_sheetf_on_change(NULL, _form);
    }
    if(_save) {
        fude_zoom_sheetf_on_save(NULL, NULL, _form);
    }
}
