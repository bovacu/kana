// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/widgets/filterbar.h"
#include "drawing/widgets/kit.h"
#include "drawing/base/draw.h"
#include "drawing/base/icons.h"
#include "drawing/base/text.h"

// ===========================================================================
// See filterbar.h.
//
// The bar is laid out again only when the screen's size, the safe area, the compact mode or a hidden toggle changes,
// and the chips and toggles are restyled only when what they show changed (the _shown fields), so a frame with no
// change costs only the screen's callbacks.
// ===========================================================================

// Sizes in UI units: a row of chips' height, the field's height, a toggle's width beside the field (and on a phone,
// where it shows its icon alone), the padding inside the bar, and the gap between things. The field's font size is
// in points.
#define FUDE_FILTERBAR_ROW_H    36.0f
#define FUDE_FILTERBAR_FIELD_H  44.0f
#define FUDE_FILTERBAR_SIDE_W   76.0f    // A toggle beside the field.
#define FUDE_FILTERBAR_ICON_W   48.0f    // The same on a phone, showing its icon alone.
#define FUDE_FILTERBAR_PAD      10.0f
#define FUDE_FILTERBAR_GAP      4.0f
#define FUDE_FILTERBAR_FIELD_PX 14u

// Returns how many chips a row has: its count, else what count_of says, at most FUDE_FILTERBAR_CHIPS. It is asked
// once, at create time: the buttons are made for its answer, which is kept (_chips_made) and used by the layout and
// the refresh, so a count_of that later answers more can never have them reach a button that was not made.
RDE_INTERNAL u32 fude_filterbar_chip_count(const fude_filterbar_chips* _chips) {
    const u32 _n = _chips->count != 0u || _chips->count_of == NULL ? _chips->count : _chips->count_of();
    return _n < FUDE_FILTERBAR_CHIPS ? _n : FUDE_FILTERBAR_CHIPS;
}

// A chip's press: tells the screen which chip was chosen, then brings the bar up to date.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_filterbar_on_chip(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_filterbar_ref* _ref = (const fude_filterbar_ref*)_user_data;
    _ref->bar->def->chips[_ref->row].choose(_ref->bar->self, _ref->index);
    fude_filterbar_update(_ref->bar, _ref->bar->shown);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// A toggle's press: empties the field first when the toggle clears the search, calls the screen, then brings the bar
// up to date.
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

// The field's change callback: every keystroke hands the whole text to the screen's search.
RDE_INTERNAL void fude_filterbar_on_search(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    fude_filterbar* _bar  = (fude_filterbar*)_user_data;
    c8*             _text = rde_ui_text_editor_get_text(_bar->field, 0, rde_ui_text_editor_get_byte_count(_bar->field));
    _bar->def->search(_bar->self, _text != NULL ? _text : "");
    rde_ui_text_editor_free_text(_bar->field, _text);
}

// Builds the panel, a scroll area of chip buttons per row, the toggles and the search field under _root, hidden. The
// buttons point back at _bar, so it must not move. See the header.
void fude_filterbar_create(fude_filterbar* _bar, rde_ui_node* _root, rde_window* _window, const fude_filterbar_def* _def, any _self) {
    rde_memset(_bar, 0, sizeof(*_bar));
    _bar->def    = _def;
    _bar->self   = _self;
    _bar->window = _window;
    _bar->panel  = rde_ui_image_create(NULL);
    rde_ui_node* _node = rde_ui_image_as_node(_bar->panel);
    rde_ui_node_set_blocks_input(_node, true);
    rde_ui_node_add_child(_root, _node);

    // A screen's bar is a table fixed in the code: more rows or toggles than there is room for is a programming error,
    // caught here rather than left for the layout and refresh to index past their arrays.
    rde_assert_fatal(_def->chip_rows <= FUDE_FILTERBAR_CHIP_ROWS && _def->toggle_count <= FUDE_FILTERBAR_TOGGLES,
                     "filterbar: more than FUDE_FILTERBAR_CHIP_ROWS (%u) rows or FUDE_FILTERBAR_TOGGLES (%u) toggles",
                     FUDE_FILTERBAR_CHIP_ROWS, FUDE_FILTERBAR_TOGGLES);
    for(u32 _r = 0; _r < _def->chip_rows; _r++) {
        _bar->lines[_r] = rde_ui_scroll_area_create(NULL);
        rde_ui_scroll_area_set_bar_thickness(_bar->lines[_r], 0.0f);
        rde_ui_scroll_area_set_background_color(_bar->lines[_r], (rde_color){ 0, 0, 0, 0 });   // Clear, over the panel.
        rde_ui_scroll_area_set_track_color(_bar->lines[_r], (rde_color){ 0, 0, 0, 0 });
        rde_ui_node* _line = rde_ui_scroll_area_as_node(_bar->lines[_r]);
        rde_ui_node_add_child(_node, _line);
        _bar->_chips_made[_r] = fude_filterbar_chip_count(&_def->chips[_r]);
        for(u32 _i = 0; _i < _bar->_chips_made[_r]; _i++) {
            _bar->chip_refs[_r][_i] = (fude_filterbar_ref){ _bar, _r, _i };
            _bar->chips[_r][_i]     = fude_kit_button(_line, _def->chips[_r].label(_i), fude_filterbar_on_chip, &_bar->chip_refs[_r][_i]);
        }
    }
    _bar->_compact_for = -1;
    for(u32 _t = 0; _t < _def->toggle_count; _t++) {
        const fude_filterbar_toggle* _toggle = &_def->toggles[_t];
        _bar->toggle_refs[_t]      = (fude_filterbar_ref){ _bar, FUDE_FILTERBAR_CHIP_ROWS, _t };
        _bar->toggles[_t]          = fude_kit_button(_node, fude_text((FUDE_TEXT_)_toggle->text), fude_filterbar_on_toggle, &_bar->toggle_refs[_t]);
        _bar->_available_shown[_t] = 1u;   // As built; the first refresh greys it out when it is not available.
        fude_kit_icon(_bar->toggles[_t], _toggle->icon, FUDE_KIT_ICON_LEFT, _toggle->icon_px);
    }

    _bar->field = rde_ui_text_editor_create(fude_kit_font(), NULL);
    rde_ui_text_editor_set_multiline(_bar->field, false);
    rde_ui_text_editor_set_font_size(_bar->field, FUDE_FILTERBAR_FIELD_PX);
    c8 _hint[512];
    rde_snprintf(_hint, sizeof(_hint), FUDE_ICON_SEARCH "  %s", fude_text((FUDE_TEXT_)_def->search_hint));
    rde_ui_text_editor_set_placeholder(_bar->field, _hint);
    rde_ui_text_editor_set_content_insets(_bar->field, 12.0f, 8.0f, 12.0f, 8.0f);
    rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_bar->field), _bar);
    rde_ui_text_editor_set_on_change(_bar->field, fude_filterbar_on_search);
    fude_kit_field_box(_node, _bar->field);

    for(u32 _r = 0; _r < FUDE_FILTERBAR_CHIP_ROWS; _r++) {
        _bar->_chosen_shown[_r] = UINT32_MAX;
    }
    rde_memset(_bar->_on_shown, 0xFF, sizeof(_bar->_on_shown));
    rde_ui_node_set_active(_node, false);
}

// Lays out row _r's chips in its scroll area's content (bottom-left origin): each as wide as its label plus 26 units of
// padding (at least 48), the first line centred on _y. A chip that would pass _width starts another line below (a
// language's many levels: HSK 1 ... HSK 7–9), unless _one_line (a phone's row, which scrolls). The chips are placed
// only when _place. Returns how many lines the row takes, and writes in *_widest how wide its widest line is.
RDE_INTERNAL u32 fude_filterbar_chip_row(fude_filterbar* _bar, u32 _r, f32 _width, f32 _y, b8 _one_line, b8 _place, f32* _widest) {
    const fude_filterbar_chips* _chips = &_bar->def->chips[_r];
    const u32 _count = _bar->_chips_made[_r];
    u32       _lines = 1u;
    f32       _x     = 0.0f;
    *_widest         = 0.0f;
    for(u32 _i = 0; _i < _count; _i++) {
        const f32 _cw = rde_math_max_f32(48.0f, fude_draw_text_width(fude_kit_font(), (f32)FUDE_KIT_FONT_SIZE, _chips->label(_i), FUDE_KIT_TEXT_SCALE * (f32)FUDE_KIT_FONT_SIZE) + 26.0f);
        if(!_one_line && _x > 0.0f && _x + _cw > _width) {
            _x  = 0.0f;
            _y -= FUDE_FILTERBAR_ROW_H + FUDE_FILTERBAR_GAP;
            _lines++;
        }
        if(_place) {
            fude_kit_place(rde_ui_button_as_node(_bar->chips[_r][_i]), (rde_vec_2F){ _x + _cw * 0.5f, _y }, (rde_vec_2F){ _cw, FUDE_FILTERBAR_ROW_H });
        }
        _x       += _cw + FUDE_FILTERBAR_GAP;
        *_widest  = rde_math_max_f32(*_widest, _x - FUDE_FILTERBAR_GAP);
    }
    return _lines;
}

// Returns whether toggle _t is hidden now: it hides when unavailable, and is unavailable.
RDE_INTERNAL b8 fude_filterbar_toggle_hidden(const fude_filterbar* _bar, u32 _t) {
    const fude_filterbar_toggle* _toggle = &_bar->def->toggles[_t];
    return _toggle->hide_unavailable && _toggle->available != NULL && !_toggle->available(_bar->self);
}

// Lays the bar out across the top of the screen, the status bar's strip included: the chip rows, then the field with
// the shown toggles after it. Its height follows how many lines the chips wrap to.
RDE_INTERNAL void fude_filterbar_layout(fude_filterbar* _bar) {
    const rde_vec_2F _screen = fude_kit_screen_size(_bar->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_bar->window);   // Left, top, right, bottom.
    const f32        _pad    = FUDE_FILTERBAR_PAD;
    const f32        _gap    = FUDE_FILTERBAR_GAP;
    const f32        _left   = (f32)_insets.x + _pad;
    const f32        _width  = _screen.x - (f32)(_insets.x + _insets.z) - 2.0f * _pad;
    const u32        _rows   = _bar->def->chip_rows;
    const b8         _compact = fude_kit_compact(_bar->window);
    u32              _lines  = 0;   // The chip rows' lines, wrapped (on a phone, one each).
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

    // The rows, top to bottom (in the panel's space, bottom-left origin), each in its scroll area: as tall as its chips
    // wrap and no wider than the bar, or, on a phone, one line as wide as its chips, which scrolls. A change of compact
    // mode scrolls each row back to its start.
    f32 _top = _height - (f32)_insets.y - _pad;   // The next row's top.
    for(u32 _r = 0; _r < _rows; _r++) {
        const f32 _h       = (f32)_per[_r] * (FUDE_FILTERBAR_ROW_H + _gap) - _gap;
        const f32 _content = _compact ? rde_math_max_f32(_width, _wide[_r]) : _width;
        fude_kit_place(rde_ui_scroll_area_as_node(_bar->lines[_r]), (rde_vec_2F){ _left + _width * 0.5f, _top - _h * 0.5f }, (rde_vec_2F){ _width, _h });
        rde_ui_scroll_area_set_content_size(_bar->lines[_r], (rde_vec_2F){ _content, _h });
        if(_compact != (_bar->_compact_for == 1)) {
            rde_ui_scroll_area_set_scroll(_bar->lines[_r], (rde_vec_2F){ 0.0f, 0.0f });
        }
        fude_filterbar_chip_row(_bar, _r, _width, _h - FUDE_FILTERBAR_ROW_H * 0.5f, _compact, true, &_wide[_r]);
        _top -= _h + _gap;
    }

    // The field, then the shown toggles after it (on a phone, their icons alone).
    f32       _y     = _top - FUDE_FILTERBAR_FIELD_H * 0.5f;
    const u32 _n     = _bar->def->toggle_count;
    u32       _shown = 0;
    for(u32 _t = 0; _t < _n; _t++) {
        _bar->_hidden[_t] = fude_filterbar_toggle_hidden(_bar, _t) ? 1u : 0u;
        rde_ui_node_set_active(rde_ui_button_as_node(_bar->toggles[_t]), !_bar->_hidden[_t]);
        _shown += _bar->_hidden[_t] ? 0u : 1u;
    }
    const f32 _side  = _compact ? FUDE_FILTERBAR_ICON_W : FUDE_FILTERBAR_SIDE_W;
    const f32 _field = _width - (f32)_shown * (_side + _gap);
    if(_compact != (_bar->_compact_for == 1)) {
        for(u32 _t = 0; _t < _n; _t++) {
            const fude_filterbar_toggle* _toggle = &_bar->def->toggles[_t];
            rde_ui_button_set_text(_bar->toggles[_t], _compact ? "" : fude_text((FUDE_TEXT_)_toggle->text));
            fude_kit_icon(_bar->toggles[_t], _toggle->icon, _compact ? FUDE_KIT_ICON_ONLY : FUDE_KIT_ICON_LEFT, _toggle->icon_px);
        }
    }
    _bar->_compact_for = _compact ? 1 : 0;
    fude_kit_place(fude_kit_field_node(_bar->field), (rde_vec_2F){ _left + _field * 0.5f, _y }, (rde_vec_2F){ _field, FUDE_FILTERBAR_FIELD_H });
    for(u32 _t = 0, _k = 0; _t < _n; _t++) {
        if(_bar->_hidden[_t]) {
            continue;
        }
        // The last toggle goes against the right edge, the others after the field.
        const f32 _x = _k + 1u == _shown ? _left + _width - _side * 0.5f
                                         : _left + _field + (f32)(_k + 1u) * _gap + _side * ((f32)_k + 0.5f);
        fude_kit_place(rde_ui_button_as_node(_bar->toggles[_t]), (rde_vec_2F){ _x, _y }, (rde_vec_2F){ _side, FUDE_FILTERBAR_FIELD_H });
        _k++;
    }
}

// Shows the chips and toggles as the screen says: the chosen chip of each row, and each toggle's on state and
// availability. Only what changed is restyled, unless _force.
RDE_INTERNAL void fude_filterbar_refresh(fude_filterbar* _bar, b8 _force) {
    for(u32 _r = 0; _r < _bar->def->chip_rows; _r++) {
        const u32 _chosen = _bar->def->chips[_r].chosen(_bar->self);
        if(!_force && _chosen == _bar->_chosen_shown[_r]) {
            continue;
        }
        _bar->_chosen_shown[_r] = _chosen;
        for(u32 _i = 0; _i < _bar->_chips_made[_r]; _i++) {
            fude_kit_button_chip(_bar->chips[_r][_i], _i == _chosen);
        }
    }
    for(u32 _t = 0; _t < _bar->def->toggle_count; _t++) {
        const fude_filterbar_toggle* _toggle    = &_bar->def->toggles[_t];
        const u8                     _on        = _toggle->on != NULL && _toggle->on(_bar->self) ? 1u : 0u;
        const u8                     _available = _toggle->available == NULL || _toggle->available(_bar->self) ? 1u : 0u;
        if(!_force && _on == _bar->_on_shown[_t] && _available == _bar->_available_shown[_t]) {
            continue;
        }
        _bar->_on_shown[_t]        = _on;
        _bar->_available_shown[_t] = _available;
        if(_on) {
            fude_kit_button_selected(_bar->toggles[_t]);
        } else {
            fude_kit_restyle_button(_bar->toggles[_t]);
        }
        fude_kit_set_enabled(_bar->toggles[_t], _available != 0u);
    }
}

// Shows or hides the bar, lays it out again when needed, and refreshes it. See the header.
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
    // Lay out again the first time, when the screen turns, and when the safe area arrives (at start iOS reports none,
    // then the status bar's a frame or two later).
    const rde_vec_2F _screen = fude_kit_screen_size(_bar->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_bar->window);
    b8               _again  = false;   // Also when a hidden toggle comes (its data loaded) or goes.
    for(u32 _t = 0; _t < _bar->def->toggle_count; _t++) {
        _again = _again || (fude_filterbar_toggle_hidden(_bar, _t) ? 1u : 0u) != _bar->_hidden[_t];
    }
    if(_again || rde_memcmp(&_screen, &_bar->_laid_out, sizeof(rde_vec_2F)) != 0 || rde_memcmp(&_insets, &_bar->_insets_for, sizeof(rde_vec_4I)) != 0) {
        fude_filterbar_layout(_bar);
    }
    fude_filterbar_refresh(_bar, false);
}

// Styles the panel and the field from the theme, and every chip and toggle again.
void fude_filterbar_restyle(fude_filterbar* _bar) {
    if(_bar->panel == NULL) {
        return;
    }
    fude_kit_style_panel(_bar->panel, 0.0f, 1.0f);
    fude_kit_style_field(_bar->field);
    fude_filterbar_refresh(_bar, true);
}

// Returns whether _ui is on the shown bar.
b8 fude_filterbar_hit(const fude_filterbar* _bar, rde_vec_2F _ui) {
    // Everything above its bottom edge counts, the status bar's strip included.
    return _bar->shown && _ui.y >= fude_kit_screen_size(_bar->window).y - _bar->height;
}
