#include "widgets/row.h"
#include "widgets/kit.h"
#include "widgets/draw.h"
#include "base/text.h"
#include "widgets/icons.h"

#include <stdio.h>
#include <string.h>

// ===========================================================================
// See row.h.
// ===========================================================================

#define KANA_ROW_BUTTON_H     56.0f
#define KANA_ROW_BUTTON_MIN_W 68.0f
#define KANA_ROW_BUTTON_MAX_W 136.0f
#define KANA_ROW_ICON_PX      17.0f
#define KANA_ROW_PADDING      6.0f
#define KANA_ROW_SPACING      4.0f
#define KANA_ROW_RADIUS       18.0f
#define KANA_ROW_BUTTON_ROUND 12.0f

f32 kana_row_height(void) {
    return KANA_ROW_BUTTON_H + 2.0f * KANA_ROW_PADDING;
}

void kana_row_face_count(kana_row_face* _face, u32 _text, u32 _n) {
    kana_text_format(_face->label, sizeof(_face->label), (KANA_TEXT_)_text, (const kana_text_arg[]){ KANA_TN(_n) }, 1u);
}

void kana_row_face_next(kana_row_face* _face, b8 _last) {
    if(_last) {
        snprintf(_face->label, sizeof(_face->label), "%s", kana_text(KANA_TEXT_FINISH));
        _face->icon = KANA_ICON_FINISH;
    }
}

// A press: the button's function, then the UI catches up.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_row_on_press(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_row_ref*    _ref    = (const kana_row_ref*)_user_data;
    kana_row*              _row    = _ref->row;
    const kana_row_button* _button = &_row->def->buttons[_ref->index];
    if(_button->press != NULL) {
        _button->press(_row->app, _row->self, _button->arg);
    }
    if(_row->after != NULL) {
        _row->after(_row->app);
    }
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// One button as its face says: label, icon, whether it can be pressed, its look
// (chosen, or as declared) — in that order, as each paints the label over the last.
RDE_INTERNAL void kana_row_paint(kana_row* _row, u32 _i) {
    rde_ui_button*         _b    = _row->buttons[_i];
    const kana_row_face*   _face = &_row->shown[_i];
    const kana_row_button* _decl = &_row->def->buttons[_i];
    rde_ui_button_set_text(_b, _face->label[0] != 0 ? _face->label : _row->base[_i]);
    kana_kit_icon(_b, _face->icon != NULL ? _face->icon : _decl->icon, KANA_KIT_ICON_ABOVE, KANA_ROW_ICON_PX);
    rde_ui_node_set_interactable(rde_ui_button_as_node(_b), !_face->disabled);
    if(_face->selected) {
        kana_kit_button_selected(_b);
    } else if(_decl->look == KANA_ROW_PRIMARY) {
        kana_kit_button_primary(_b);
    } else if(_decl->look == KANA_ROW_DANGER) {
        kana_kit_button_danger_quiet(_b);
    } else if(_decl->look == KANA_ROW_PLAIN) {
        kana_kit_button_plain(_b);
    } else {
        kana_kit_button_quiet(_b);
    }
    kana_kit_button_round(_b, KANA_ROW_BUTTON_ROUND);
}

void kana_row_create(kana_row* _row, rde_ui_node* _root, const kana_row_def* _def, struct kana_app* _app, void* _self, void (*_after)(struct kana_app*)) {
    memset(_row, 0, sizeof(*_row));
    _row->def   = _def;
    _row->app   = _app;
    _row->self  = _self;
    _row->after = _after;
    _row->panel = rde_ui_image_create(NULL);
    rde_ui_node* _node = rde_ui_image_as_node(_row->panel);
    rde_ui_node_set_blocks_input(_node, true);
    rde_ui_node_add_child(_root, _node);

    const u32 _count = _def->count < KANA_ROW_BUTTONS ? _def->count : KANA_ROW_BUTTONS;
    b8        _here[KANA_ROW_BUTTONS];
    f32       _widths[KANA_ROW_BUTTONS];
    u32       _shown = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        const kana_row_button* _b = &_def->buttons[_i];
        _here[_i] = _b->present == NULL || _b->present();
        _shown   += _here[_i] ? 1u : 0u;
        if(_b->counted) {
            kana_text_format(_row->base[_i], sizeof(_row->base[_i]), (KANA_TEXT_)_b->text, (const kana_text_arg[]){ KANA_TN(0) }, 1u);
        } else {
            snprintf(_row->base[_i], sizeof(_row->base[_i]), "%s", kana_text((KANA_TEXT_)_b->text));
        }
    }

    // Each button as wide as its label needs; a label ending in a count ("Practice 0")
    // with room for three digits.
    f32 _total = 2.0f * KANA_ROW_PADDING + KANA_ROW_SPACING * (f32)(_shown > 0 ? _shown - 1u : 0u);
    for(u32 _i = 0; _i < _count; _i++) {
        _widths[_i] = 0.0f;
        if(!_here[_i]) {
            continue;
        }
        c8          _measure[KANA_ROW_LABEL + 4];
        const usize _len          = strlen(_row->base[_i]);
        const b8    _count_at_end = _len >= 2 && _row->base[_i][_len - 1] == '0' && _row->base[_i][_len - 2] == ' ';
        snprintf(_measure, sizeof(_measure), _count_at_end ? "%s00" : "%s", _row->base[_i]);
        const f32 _w = kana_draw_text_width(kana_kit_font(), (f32)KANA_KIT_FONT_SIZE, _measure, KANA_KIT_CAPTION_PX) + 24.0f;
        _widths[_i]  = rde_math_clamp_f32(_w, KANA_ROW_BUTTON_MIN_W, KANA_ROW_BUTTON_MAX_W);
        _total      += _widths[_i];
    }
    _row->size = (rde_vec_2F){ _total, kana_row_height() };

    f32 _x = KANA_ROW_PADDING;
    for(u32 _i = 0; _i < _count; _i++) {
        if(!_here[_i]) {
            continue;
        }
        _row->refs[_i]    = (kana_row_ref){ _row, _i };
        _row->buttons[_i] = kana_kit_button(_node, _row->base[_i], kana_row_on_press, &_row->refs[_i]);
        kana_kit_place(rde_ui_button_as_node(_row->buttons[_i]), (rde_vec_2F){ _x + _widths[_i] * 0.5f, _row->size.y * 0.5f },
                       (rde_vec_2F){ _widths[_i], KANA_ROW_BUTTON_H });
        _x += _widths[_i] + KANA_ROW_SPACING;
    }
    kana_row_restyle(_row);
    rde_ui_node_set_active(_node, false);
}

void kana_row_restyle(kana_row* _row) {
    kana_kit_style_panel(_row->panel, KANA_ROW_RADIUS, 1.0f);
    for(u32 _i = 0; _i < KANA_ROW_BUTTONS; _i++) {
        if(_row->buttons[_i] != NULL) {
            kana_row_paint(_row, _i);
        }
    }
}

RDE_INTERNAL b8 kana_row_same_face(const kana_row_face* _a, const kana_row_face* _b) {
    return _a->icon == _b->icon && _a->disabled == _b->disabled && _a->selected == _b->selected && strcmp(_a->label, _b->label) == 0;
}

void kana_row_apply(kana_row* _row, const kana_row_face* _faces) {
    for(u32 _i = 0; _i < KANA_ROW_BUTTONS && _i < _row->def->count; _i++) {
        if(_row->buttons[_i] == NULL || kana_row_same_face(&_faces[_i], &_row->shown[_i])) {
            continue;
        }
        _row->shown[_i] = _faces[_i];
        kana_row_paint(_row, _i);
    }
}

rde_vec_2F kana_row_bottom(const kana_row* _row, rde_window* _window) {
    const rde_vec_2F _screen = kana_kit_screen_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    return (rde_vec_2F){ _screen.x * 0.5f, (f32)_insets.w + KANA_KIT_SCREEN_EDGE + _row->size.y * 0.5f };
}

void kana_row_show(kana_row* _row, rde_window* _window, b8 _show, rde_vec_2F _center) {
    if(_row->panel == NULL) {
        return;
    }
    if(_show) {
        _center = kana_kit_clamp(_window, _center, _row->size);
        if(!_row->open || memcmp(&_center, &_row->center, sizeof(rde_vec_2F)) != 0) {
            _row->center = _center;
            kana_kit_place(rde_ui_image_as_node(_row->panel), _center, _row->size);
        }
    }
    if(_show != _row->open) {
        _row->open = _show;
        rde_ui_node_set_active(rde_ui_image_as_node(_row->panel), _show);
    }
}

b8 kana_row_hit(const kana_row* _row, rde_vec_2F _ui) {
    return _row->open && _ui.x >= _row->center.x - _row->size.x * 0.5f && _ui.x <= _row->center.x + _row->size.x * 0.5f &&
           _ui.y >= _row->center.y - _row->size.y * 0.5f && _ui.y <= _row->center.y + _row->size.y * 0.5f;
}
