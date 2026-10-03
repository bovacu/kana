#include "drawing/widgets/filterbar.h"
#include "drawing/widgets/kit.h"
#include "drawing/widgets/draw.h"
#include "drawing/widgets/icons.h"
#include "drawing/base/text.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See filterbar.h.
// ===========================================================================

#define FUDE_FILTERBAR_ROW_H    36.0f
#define FUDE_FILTERBAR_FIELD_H  44.0f
#define FUDE_FILTERBAR_SIDE_W   76.0f    // a toggle beside the field
#define FUDE_FILTERBAR_ICON_W   48.0f    // ...on a phone: its icon alone
#define FUDE_FILTERBAR_PAD      10.0f
#define FUDE_FILTERBAR_GAP      4.0f
#define FUDE_FILTERBAR_FIELD_PX 14u

// A row's chips: as many as it says, at most FUDE_FILTERBAR_CHIPS.
RDE_INTERNAL u32 fude_filterbar_chip_count(const fude_filterbar_chips* _chips) {
    const u32 _n = _chips->count != 0u || _chips->count_of == NULL ? _chips->count : _chips->count_of();
    return _n < FUDE_FILTERBAR_CHIPS ? _n : FUDE_FILTERBAR_CHIPS;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_filterbar_on_chip(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_filterbar_ref* _ref = (const fude_filterbar_ref*)_user_data;
    _ref->bar->def->chips[_ref->row].choose(_ref->bar->self, _ref->index);
    fude_filterbar_update(_ref->bar, _ref->bar->shown);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_filterbar_on_toggle(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_filterbar_ref*    _ref    = (const fude_filterbar_ref*)_user_data;
    fude_filterbar*              _bar    = _ref->bar;
    const fude_filterbar_toggle* _toggle = &_bar->def->toggles[_ref->index];
    if(_toggle->clears_search) {
        const usize _bytes = rde_ui_text_editor_get_byte_count(_bar->field);
        if(_bytes > 0) {
            rde_ui_text_editor_delete_range(_bar->field, 0, _bytes);
        }
    }
    _toggle->press(_bar->self);
    fude_filterbar_update(_bar, _bar->shown);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Every keystroke searches.
RDE_INTERNAL void fude_filterbar_on_search(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    fude_filterbar* _bar  = (fude_filterbar*)_user_data;
    c8*             _text = rde_ui_text_editor_get_text(_bar->field, 0, rde_ui_text_editor_get_byte_count(_bar->field));
    _bar->def->search(_bar->self, _text != NULL ? _text : "");
    rde_ui_text_editor_free_text(_bar->field, _text);
}

void fude_filterbar_create(fude_filterbar* _bar, rde_ui_node* _root, rde_window* _window, const fude_filterbar_def* _def, void* _self) {
    memset(_bar, 0, sizeof(*_bar));
    _bar->def    = _def;
    _bar->self   = _self;
    _bar->window = _window;
    _bar->panel  = rde_ui_image_create(NULL);
    rde_ui_node* _node = rde_ui_image_as_node(_bar->panel);
    rde_ui_node_set_blocks_input(_node, true);
    rde_ui_node_add_child(_root, _node);

    for(u32 _r = 0; _r < _def->chip_rows && _r < FUDE_FILTERBAR_CHIP_ROWS; _r++) {
        _bar->lines[_r] = rde_ui_scroll_area_create(NULL);
        rde_ui_scroll_area_set_bar_thickness(_bar->lines[_r], 0.0f);
        rde_ui_scroll_area_set_background_color(_bar->lines[_r], (rde_color){ 0, 0, 0, 0 });   // the bar's panel shows through
        rde_ui_scroll_area_set_track_color(_bar->lines[_r], (rde_color){ 0, 0, 0, 0 });
        rde_ui_node* _line = rde_ui_scroll_area_as_node(_bar->lines[_r]);
        rde_ui_node_add_child(_node, _line);
        for(u32 _i = 0; _i < fude_filterbar_chip_count(&_def->chips[_r]); _i++) {
            _bar->chip_refs[_r][_i] = (fude_filterbar_ref){ _bar, _r, _i };
            _bar->chips[_r][_i]     = fude_kit_button(_line, _def->chips[_r].label(_i), fude_filterbar_on_chip, &_bar->chip_refs[_r][_i]);
        }
    }
    _bar->_compact_for = -1;
    for(u32 _t = 0; _t < _def->toggle_count && _t < FUDE_FILTERBAR_TOGGLES; _t++) {
        const fude_filterbar_toggle* _toggle = &_def->toggles[_t];
        _bar->toggle_refs[_t] = (fude_filterbar_ref){ _bar, FUDE_FILTERBAR_CHIP_ROWS, _t };
        _bar->toggles[_t]     = fude_kit_button(_node, fude_text((FUDE_TEXT_)_toggle->text), fude_filterbar_on_toggle, &_bar->toggle_refs[_t]);
        if(_toggle->available != NULL && !_toggle->available(_self)) {
            fude_kit_set_enabled(_bar->toggles[_t], false);
        }
        fude_kit_icon(_bar->toggles[_t], _toggle->icon, FUDE_KIT_ICON_LEFT, _toggle->icon_px);
    }

    _bar->field = rde_ui_text_editor_create(fude_kit_font(), NULL);
    rde_ui_text_editor_set_multiline(_bar->field, false);
    rde_ui_text_editor_set_font_size(_bar->field, FUDE_FILTERBAR_FIELD_PX);
    c8 _hint[160];
    snprintf(_hint, sizeof(_hint), FUDE_ICON_SEARCH "  %s", fude_text((FUDE_TEXT_)_def->search_hint));
    rde_ui_text_editor_set_placeholder(_bar->field, _hint);
    rde_ui_text_editor_set_content_insets(_bar->field, 12.0f, 8.0f, 12.0f, 8.0f);
    rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_bar->field), _bar);
    rde_ui_text_editor_set_on_change(_bar->field, fude_filterbar_on_search);
    fude_kit_field_box(_node, _bar->field);

    for(u32 _r = 0; _r < FUDE_FILTERBAR_CHIP_ROWS; _r++) {
        _bar->_chosen_shown[_r] = UINT32_MAX;
    }
    memset(_bar->_on_shown, 0xFF, sizeof(_bar->_on_shown));
    rde_ui_node_set_active(_node, false);
}

// A row of chips in its line's content (bottom-left origin), each as wide as its
// label (and padding), the first line's centred on _y; one that would pass _width
// starts another line below (a language's many levels: HSK 1 ... HSK 7–9) —
// unless _one_line (a phone's, scrolled). Placed only when _place; how many
// lines it takes, and in *_widest how wide.
RDE_INTERNAL u32 fude_filterbar_chip_row(fude_filterbar* _bar, u32 _r, f32 _width, f32 _y, b8 _one_line, b8 _place, f32* _widest) {
    const fude_filterbar_chips* _chips = &_bar->def->chips[_r];
    const u32 _count = fude_filterbar_chip_count(_chips);
    u32       _lines = 1u;
    f32       _x     = 0.0f;
    *_widest         = 0.0f;
    for(u32 _i = 0; _i < _count; _i++) {
        const f32 _cw = fmaxf(48.0f, fude_draw_text_width(fude_kit_font(), (f32)FUDE_KIT_FONT_SIZE, _chips->label(_i), FUDE_KIT_TEXT_SCALE * (f32)FUDE_KIT_FONT_SIZE) + 26.0f);
        if(!_one_line && _x > 0.0f && _x + _cw > _width) {
            _x  = 0.0f;
            _y -= FUDE_FILTERBAR_ROW_H + FUDE_FILTERBAR_GAP;
            _lines++;
        }
        if(_place) {
            fude_kit_place(rde_ui_button_as_node(_bar->chips[_r][_i]), (rde_vec_2F){ _x + _cw * 0.5f, _y }, (rde_vec_2F){ _cw, FUDE_FILTERBAR_ROW_H });
        }
        _x       += _cw + FUDE_FILTERBAR_GAP;
        *_widest  = fmaxf(*_widest, _x - FUDE_FILTERBAR_GAP);
    }
    return _lines;
}

// Across the top, the status-bar strip included: the chip rows, then the field
// with the toggles after it.
RDE_INTERNAL void fude_filterbar_layout(fude_filterbar* _bar) {
    const rde_vec_2F _screen = fude_kit_screen_size(_bar->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_bar->window);   // left, top, right, bottom
    const f32        _pad    = FUDE_FILTERBAR_PAD;
    const f32        _gap    = FUDE_FILTERBAR_GAP;
    const f32        _left   = (f32)_insets.x + _pad;
    const f32        _width  = _screen.x - (f32)(_insets.x + _insets.z) - 2.0f * _pad;
    const u32        _rows   = _bar->def->chip_rows;
    const b8         _compact = fude_kit_compact(_bar->window);
    u32              _lines  = 0;   // the chip rows' lines, wrapped (a phone's: one each)
    u32              _per[FUDE_FILTERBAR_CHIP_ROWS];
    f32              _wide[FUDE_FILTERBAR_CHIP_ROWS];
    for(u32 _r = 0; _r < _rows; _r++) {
        _per[_r] = fude_filterbar_chip_row(_bar, _r, _width, 0.0f, _compact, false, &_wide[_r]);
        _lines  += _per[_r];
    }
    const f32        _height = (f32)_insets.y + _pad + (f32)_lines * (FUDE_FILTERBAR_ROW_H + _gap) + FUDE_FILTERBAR_FIELD_H + _pad;

    _bar->_laid_out   = _screen;
    _bar->_insets_for = _insets;
    _bar->height      = _height;
    fude_kit_place(rde_ui_image_as_node(_bar->panel), (rde_vec_2F){ _screen.x * 0.5f, _screen.y - _height * 0.5f }, (rde_vec_2F){ _screen.x, _height });

    // Rows, top to bottom (panel-local: bottom-left origin), each in its line:
    // as tall as its chips wrap and no wider than the bar — or, on a phone, one
    // line as wide as its chips, scrolled.
    f32 _top = _height - (f32)_insets.y - _pad;   // the next row's top
    for(u32 _r = 0; _r < _rows; _r++) {
        const f32 _h       = (f32)_per[_r] * (FUDE_FILTERBAR_ROW_H + _gap) - _gap;
        const f32 _content = _compact ? fmaxf(_width, _wide[_r]) : _width;
        fude_kit_place(rde_ui_scroll_area_as_node(_bar->lines[_r]), (rde_vec_2F){ _left + _width * 0.5f, _top - _h * 0.5f }, (rde_vec_2F){ _width, _h });
        rde_ui_scroll_area_set_content_size(_bar->lines[_r], (rde_vec_2F){ _content, _h });
        if(_compact != (_bar->_compact_for == 1)) {
            rde_ui_scroll_area_set_scroll(_bar->lines[_r], (rde_vec_2F){ 0.0f, 0.0f });
        }
        fude_filterbar_chip_row(_bar, _r, _width, _h - FUDE_FILTERBAR_ROW_H * 0.5f, _compact, true, &_wide[_r]);
        _top -= _h + _gap;
    }

    // The field, the toggles after it (a phone's: their icons alone).
    f32       _y     = _top - FUDE_FILTERBAR_FIELD_H * 0.5f;
    const u32 _n     = _bar->def->toggle_count;
    const f32 _side  = _compact ? FUDE_FILTERBAR_ICON_W : FUDE_FILTERBAR_SIDE_W;
    const f32 _field = _width - (f32)_n * (_side + _gap);
    if(_compact != (_bar->_compact_for == 1)) {
        for(u32 _t = 0; _t < _n; _t++) {
            const fude_filterbar_toggle* _toggle = &_bar->def->toggles[_t];
            rde_ui_button_set_text(_bar->toggles[_t], _compact ? "" : fude_text((FUDE_TEXT_)_toggle->text));
            fude_kit_icon(_bar->toggles[_t], _toggle->icon, _compact ? FUDE_KIT_ICON_ONLY : FUDE_KIT_ICON_LEFT, _toggle->icon_px);
        }
    }
    _bar->_compact_for = _compact ? 1 : 0;
    fude_kit_place(fude_kit_field_node(_bar->field), (rde_vec_2F){ _left + _field * 0.5f, _y }, (rde_vec_2F){ _field, FUDE_FILTERBAR_FIELD_H });
    for(u32 _t = 0; _t < _n; _t++) {
        // The last against the right edge; the others after the field.
        const f32 _x = _t + 1u == _n ? _left + _width - _side * 0.5f
                                     : _left + _field + (f32)(_t + 1u) * _gap + _side * ((f32)_t + 0.5f);
        fude_kit_place(rde_ui_button_as_node(_bar->toggles[_t]), (rde_vec_2F){ _x, _y }, (rde_vec_2F){ _side, FUDE_FILTERBAR_FIELD_H });
    }
}

// The chips and toggles as the screen says (only what changed).
RDE_INTERNAL void fude_filterbar_refresh(fude_filterbar* _bar, b8 _force) {
    for(u32 _r = 0; _r < _bar->def->chip_rows; _r++) {
        const u32 _chosen = _bar->def->chips[_r].chosen(_bar->self);
        if(!_force && _chosen == _bar->_chosen_shown[_r]) {
            continue;
        }
        _bar->_chosen_shown[_r] = _chosen;
        for(u32 _i = 0; _i < fude_filterbar_chip_count(&_bar->def->chips[_r]); _i++) {
            fude_kit_button_chip(_bar->chips[_r][_i], _i == _chosen);
        }
    }
    for(u32 _t = 0; _t < _bar->def->toggle_count; _t++) {
        const fude_filterbar_toggle* _toggle = &_bar->def->toggles[_t];
        const u8 _on = _toggle->on != NULL && _toggle->on(_bar->self) ? 1u : 0u;
        if(!_force && _on == _bar->_on_shown[_t]) {
            continue;
        }
        _bar->_on_shown[_t] = _on;
        if(_on) {
            fude_kit_button_selected(_bar->toggles[_t]);
        } else {
            fude_kit_restyle_button(_bar->toggles[_t]);
        }
    }
}

void fude_filterbar_update(fude_filterbar* _bar, b8 _show) {
    if(_bar->panel == NULL) {
        return;
    }
    if(_show != _bar->shown) {
        _bar->shown = _show;
        rde_ui_node_set_active(rde_ui_image_as_node(_bar->panel), _show);
    }
    if(!_show) {
        return;
    }
    // The first time, when the screen turns, and when the safe area arrives (at
    // start iOS reports none, then the status bar's a frame or two later).
    const rde_vec_2F _screen = fude_kit_screen_size(_bar->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_bar->window);
    if(memcmp(&_screen, &_bar->_laid_out, sizeof(rde_vec_2F)) != 0 || memcmp(&_insets, &_bar->_insets_for, sizeof(rde_vec_4I)) != 0) {
        fude_filterbar_layout(_bar);
    }
    fude_filterbar_refresh(_bar, false);
}

void fude_filterbar_restyle(fude_filterbar* _bar) {
    if(_bar->panel == NULL) {
        return;
    }
    fude_kit_style_panel(_bar->panel, 0.0f, 1.0f);
    fude_kit_style_field(_bar->field);
    fude_filterbar_refresh(_bar, true);
}

b8 fude_filterbar_hit(const fude_filterbar* _bar, rde_vec_2F _ui) {
    // Everything above its bottom edge.
    return _bar->shown && _ui.y >= fude_kit_screen_size(_bar->window).y - _bar->height;
}
