#include "widgets/filterbar.h"
#include "widgets/kit.h"
#include "widgets/draw.h"
#include "widgets/icons.h"
#include "base/text.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See filterbar.h.
// ===========================================================================

#define KANA_FILTERBAR_ROW_H    36.0f
#define KANA_FILTERBAR_FIELD_H  44.0f
#define KANA_FILTERBAR_SIDE_W   76.0f    // a toggle beside the field
#define KANA_FILTERBAR_PAD      10.0f
#define KANA_FILTERBAR_GAP      4.0f
#define KANA_FILTERBAR_FIELD_PX 14u

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_filterbar_on_chip(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_filterbar_ref* _ref = (const kana_filterbar_ref*)_user_data;
    _ref->bar->def->chips[_ref->row].choose(_ref->bar->self, _ref->index);
    kana_filterbar_update(_ref->bar, _ref->bar->shown);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_filterbar_on_toggle(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_filterbar_ref*    _ref    = (const kana_filterbar_ref*)_user_data;
    kana_filterbar*              _bar    = _ref->bar;
    const kana_filterbar_toggle* _toggle = &_bar->def->toggles[_ref->index];
    if(_toggle->clears_search) {
        const usize _bytes = rde_ui_text_editor_get_byte_count(_bar->field);
        if(_bytes > 0) {
            rde_ui_text_editor_delete_range(_bar->field, 0, _bytes);
        }
    }
    _toggle->press(_bar->self);
    kana_filterbar_update(_bar, _bar->shown);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Every keystroke searches.
RDE_INTERNAL void kana_filterbar_on_search(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    kana_filterbar* _bar  = (kana_filterbar*)_user_data;
    c8*             _text = rde_ui_text_editor_get_text(_bar->field, 0, rde_ui_text_editor_get_byte_count(_bar->field));
    _bar->def->search(_bar->self, _text != NULL ? _text : "");
    rde_ui_text_editor_free_text(_bar->field, _text);
}

void kana_filterbar_create(kana_filterbar* _bar, rde_ui_node* _root, rde_window* _window, const kana_filterbar_def* _def, void* _self) {
    memset(_bar, 0, sizeof(*_bar));
    _bar->def    = _def;
    _bar->self   = _self;
    _bar->window = _window;
    _bar->panel  = rde_ui_image_create(NULL);
    rde_ui_node* _node = rde_ui_image_as_node(_bar->panel);
    rde_ui_node_set_blocks_input(_node, true);
    rde_ui_node_add_child(_root, _node);

    for(u32 _r = 0; _r < _def->chip_rows && _r < KANA_FILTERBAR_CHIP_ROWS; _r++) {
        for(u32 _i = 0; _i < _def->chips[_r].count && _i < KANA_FILTERBAR_CHIPS; _i++) {
            _bar->chip_refs[_r][_i] = (kana_filterbar_ref){ _bar, _r, _i };
            _bar->chips[_r][_i]     = kana_kit_button(_node, _def->chips[_r].label(_i), kana_filterbar_on_chip, &_bar->chip_refs[_r][_i]);
        }
    }
    for(u32 _t = 0; _t < _def->toggle_count && _t < KANA_FILTERBAR_TOGGLES; _t++) {
        const kana_filterbar_toggle* _toggle = &_def->toggles[_t];
        _bar->toggle_refs[_t] = (kana_filterbar_ref){ _bar, KANA_FILTERBAR_CHIP_ROWS, _t };
        _bar->toggles[_t]     = kana_kit_button(_node, kana_text((KANA_TEXT_)_toggle->text), kana_filterbar_on_toggle, &_bar->toggle_refs[_t]);
        if(_toggle->available != NULL && !_toggle->available(_self)) {
            kana_kit_set_enabled(_bar->toggles[_t], false);
        }
        kana_kit_icon(_bar->toggles[_t], _toggle->icon, KANA_KIT_ICON_LEFT, _toggle->icon_px);
    }

    _bar->field = rde_ui_text_editor_create(kana_kit_font(), NULL);
    rde_ui_text_editor_set_multiline(_bar->field, false);
    rde_ui_text_editor_set_font_size(_bar->field, KANA_FILTERBAR_FIELD_PX);
    c8 _hint[160];
    snprintf(_hint, sizeof(_hint), KANA_ICON_SEARCH "  %s", kana_text((KANA_TEXT_)_def->search_hint));
    rde_ui_text_editor_set_placeholder(_bar->field, _hint);
    rde_ui_text_editor_set_content_insets(_bar->field, 12.0f, 8.0f, 12.0f, 8.0f);
    rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_bar->field), _bar);
    rde_ui_text_editor_set_on_change(_bar->field, kana_filterbar_on_search);
    kana_kit_field_box(_node, _bar->field);

    for(u32 _r = 0; _r < KANA_FILTERBAR_CHIP_ROWS; _r++) {
        _bar->_chosen_shown[_r] = UINT32_MAX;
    }
    memset(_bar->_on_shown, 0xFF, sizeof(_bar->_on_shown));
    rde_ui_node_set_active(_node, false);
}

// A row of chips from _left, each as wide as its label (and padding), centred
// on _y; squeezed together when they would pass _width (the labels shrink to
// fit their chips).
RDE_INTERNAL void kana_filterbar_chip_row(kana_filterbar* _bar, u32 _r, f32 _left, f32 _width, f32 _y) {
    const kana_filterbar_chips* _chips = &_bar->def->chips[_r];
    const u32 _count = _chips->count < KANA_FILTERBAR_CHIPS ? _chips->count : KANA_FILTERBAR_CHIPS;
    f32       _w[KANA_FILTERBAR_CHIPS];
    f32       _total = KANA_FILTERBAR_GAP * (f32)(_count - 1u);
    for(u32 _i = 0; _i < _count; _i++) {
        _w[_i]  = fmaxf(48.0f, kana_draw_text_width(kana_kit_font(), (f32)KANA_KIT_FONT_SIZE, _chips->label(_i), KANA_KIT_TEXT_SCALE * (f32)KANA_KIT_FONT_SIZE) + 26.0f);
        _total += _w[_i];
    }
    const f32 _squeeze = _total > _width ? (_width - KANA_FILTERBAR_GAP * (f32)(_count - 1u)) / (_total - KANA_FILTERBAR_GAP * (f32)(_count - 1u)) : 1.0f;
    f32 _x = _left;
    for(u32 _i = 0; _i < _count; _i++) {
        const f32 _cw = _w[_i] * _squeeze;
        kana_kit_place(rde_ui_button_as_node(_bar->chips[_r][_i]), (rde_vec_2F){ _x + _cw * 0.5f, _y }, (rde_vec_2F){ _cw, KANA_FILTERBAR_ROW_H });
        _x += _cw + KANA_FILTERBAR_GAP;
    }
}

// Across the top, the status-bar strip included: the chip rows, then the field
// with the toggles after it.
RDE_INTERNAL void kana_filterbar_layout(kana_filterbar* _bar) {
    const rde_vec_2F _screen = kana_kit_screen_size(_bar->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_bar->window);   // left, top, right, bottom
    const f32        _pad    = KANA_FILTERBAR_PAD;
    const f32        _gap    = KANA_FILTERBAR_GAP;
    const f32        _left   = (f32)_insets.x + _pad;
    const f32        _width  = _screen.x - (f32)(_insets.x + _insets.z) - 2.0f * _pad;
    const u32        _rows   = _bar->def->chip_rows;
    const f32        _height = (f32)_insets.y + _pad + (f32)_rows * (KANA_FILTERBAR_ROW_H + _gap) + KANA_FILTERBAR_FIELD_H + _pad;

    _bar->_laid_out   = _screen;
    _bar->_insets_for = _insets;
    _bar->height      = _height;
    kana_kit_place(rde_ui_image_as_node(_bar->panel), (rde_vec_2F){ _screen.x * 0.5f, _screen.y - _height * 0.5f }, (rde_vec_2F){ _screen.x, _height });

    // Rows, top to bottom (panel-local: bottom-left origin).
    f32 _y = _height - (f32)_insets.y - _pad - KANA_FILTERBAR_ROW_H * 0.5f;
    for(u32 _r = 0; _r < _rows; _r++) {
        kana_filterbar_chip_row(_bar, _r, _left, _width, _y);
        _y -= _r + 1u < _rows ? KANA_FILTERBAR_ROW_H + _gap : 0.0f;
    }

    _y -= (KANA_FILTERBAR_ROW_H + KANA_FILTERBAR_FIELD_H) * 0.5f + _gap;
    const u32 _n     = _bar->def->toggle_count;
    const f32 _field = _width - (f32)_n * (KANA_FILTERBAR_SIDE_W + _gap);
    kana_kit_place(kana_kit_field_node(_bar->field), (rde_vec_2F){ _left + _field * 0.5f, _y }, (rde_vec_2F){ _field, KANA_FILTERBAR_FIELD_H });
    for(u32 _t = 0; _t < _n; _t++) {
        // The last against the right edge; the others after the field.
        const f32 _x = _t + 1u == _n ? _left + _width - KANA_FILTERBAR_SIDE_W * 0.5f
                                     : _left + _field + (f32)(_t + 1u) * _gap + KANA_FILTERBAR_SIDE_W * ((f32)_t + 0.5f);
        kana_kit_place(rde_ui_button_as_node(_bar->toggles[_t]), (rde_vec_2F){ _x, _y }, (rde_vec_2F){ KANA_FILTERBAR_SIDE_W, KANA_FILTERBAR_FIELD_H });
    }
}

// The chips and toggles as the screen says (only what changed).
RDE_INTERNAL void kana_filterbar_refresh(kana_filterbar* _bar, b8 _force) {
    for(u32 _r = 0; _r < _bar->def->chip_rows; _r++) {
        const u32 _chosen = _bar->def->chips[_r].chosen(_bar->self);
        if(!_force && _chosen == _bar->_chosen_shown[_r]) {
            continue;
        }
        _bar->_chosen_shown[_r] = _chosen;
        for(u32 _i = 0; _i < _bar->def->chips[_r].count && _i < KANA_FILTERBAR_CHIPS; _i++) {
            kana_kit_button_chip(_bar->chips[_r][_i], _i == _chosen);
        }
    }
    for(u32 _t = 0; _t < _bar->def->toggle_count; _t++) {
        const kana_filterbar_toggle* _toggle = &_bar->def->toggles[_t];
        const u8 _on = _toggle->on != NULL && _toggle->on(_bar->self) ? 1u : 0u;
        if(!_force && _on == _bar->_on_shown[_t]) {
            continue;
        }
        _bar->_on_shown[_t] = _on;
        if(_on) {
            kana_kit_button_selected(_bar->toggles[_t]);
        } else {
            kana_kit_restyle_button(_bar->toggles[_t]);
        }
    }
}

void kana_filterbar_update(kana_filterbar* _bar, b8 _show) {
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
    const rde_vec_2F _screen = kana_kit_screen_size(_bar->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_bar->window);
    if(memcmp(&_screen, &_bar->_laid_out, sizeof(rde_vec_2F)) != 0 || memcmp(&_insets, &_bar->_insets_for, sizeof(rde_vec_4I)) != 0) {
        kana_filterbar_layout(_bar);
    }
    kana_filterbar_refresh(_bar, false);
}

void kana_filterbar_restyle(kana_filterbar* _bar) {
    if(_bar->panel == NULL) {
        return;
    }
    kana_kit_style_panel(_bar->panel, 0.0f, 1.0f);
    kana_kit_style_field(_bar->field);
    kana_filterbar_refresh(_bar, true);
}

b8 kana_filterbar_hit(const kana_filterbar* _bar, rde_vec_2F _ui) {
    // Everything above its bottom edge.
    return _bar->shown && _ui.y >= kana_kit_screen_size(_bar->window).y - _bar->height;
}
