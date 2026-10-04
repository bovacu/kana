#include "drawing/widgets/row.h"
#include "drawing/widgets/kit.h"
#include "drawing/widgets/draw.h"
#include "drawing/base/text.h"
#include "drawing/widgets/icons.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See row.h.
// ===========================================================================

#define FUDE_ROW_BUTTON_H     56.0f
#define FUDE_ROW_BUTTON_MIN_W 68.0f
#define FUDE_ROW_BUTTON_MAX_W 136.0f
#define FUDE_ROW_ICON_PX      17.0f
#define FUDE_ROW_PADDING      6.0f
#define FUDE_ROW_SPACING      4.0f
#define FUDE_ROW_RADIUS       18.0f
#define FUDE_ROW_BUTTON_ROUND 12.0f
#define FUDE_ROW_COMPACT_W    84.0f    // a button's width at most, where they do not all fit
#define FUDE_ROW_ITEM_H       48.0f    // an item in the More card
#define FUDE_ROW_ITEM_MIN_W   200.0f

f32 fude_row_height(void) {
    return FUDE_ROW_BUTTON_H + 2.0f * FUDE_ROW_PADDING;
}

u32 fude_row_def_find(const fude_row_def* _def, fude_row_press _press) {
    for(u32 _i = 0; _def != NULL && _i < _def->count; _i++) {
        if(_def->buttons[_i].press == _press) {
            return _i;
        }
    }
    return FUDE_ROW_NONE;
}

void fude_row_face_count(fude_row_face* _face, u32 _text, u32 _n) {
    fude_text_format(_face->label, sizeof(_face->label), (FUDE_TEXT_)_text, (const fude_text_arg[]){ FUDE_TN(_n) }, 1u);
}

void fude_row_face_next(fude_row_face* _face, b8 _last) {
    if(_last) {
        snprintf(_face->label, sizeof(_face->label), "%s", fude_text(FUDE_TEXT_FINISH));
        _face->icon = FUDE_ICON_FINISH;
    }
}

// A press: the button's function, then the UI catches up.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_row_on_press(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_row_ref*    _ref    = (const fude_row_ref*)_user_data;
    fude_row*              _row    = _ref->row;
    const fude_row_button* _button = &_row->def->buttons[_ref->index];
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
RDE_INTERNAL void fude_row_paint(fude_row* _row, u32 _i) {
    rde_ui_button*         _b    = _row->buttons[_i];
    const fude_row_face*   _face = &_row->shown[_i];
    const fude_row_button* _decl = &_row->def->buttons[_i];
    rde_ui_button_set_text(_b, _face->label[0] != 0 ? _face->label : _row->base[_i]);
    fude_kit_icon(_b, _face->icon != NULL ? _face->icon : _decl->icon, FUDE_KIT_ICON_ABOVE, FUDE_ROW_ICON_PX);
    rde_ui_node_set_interactable(rde_ui_button_as_node(_b), !_face->disabled);
    if(_face->selected) {
        fude_kit_button_selected(_b);
    } else if(_decl->look == FUDE_ROW_PRIMARY) {
        fude_kit_button_primary(_b);
    } else if(_decl->look == FUDE_ROW_DANGER) {
        fude_kit_button_danger_quiet(_b);
    } else if(_decl->look == FUDE_ROW_PLAIN) {
        fude_kit_button_plain(_b);
    } else {
        fude_kit_button_quiet(_b);
    }
    fude_kit_button_round(_b, FUDE_ROW_BUTTON_ROUND);

    // Its item in the More card: the same label, icon and state.
    rde_ui_button* _item = _row->items[_i];
    if(_item != NULL) {
        rde_ui_button_set_text(_item, _face->label[0] != 0 ? _face->label : _row->base[_i]);
        fude_kit_icon(_item, _face->icon != NULL ? _face->icon : _decl->icon, FUDE_KIT_ICON_LEFT, FUDE_ROW_ICON_PX);
        rde_ui_node_set_interactable(rde_ui_button_as_node(_item), !_face->disabled);
        if(_face->selected) {
            fude_kit_button_selected(_item);
        } else {
            fude_kit_button_quiet(_item);
        }
        fude_kit_button_round(_item, FUDE_ROW_BUTTON_ROUND);
    }
}

// --- More: the buttons that do not fit ------------------------------------------------------

RDE_INTERNAL void fude_row_menu_show(fude_row* _row, b8 _open) {
    _row->menu_open = _open;
    fude_kit_modal_show(&_row->menu, _open);
}

b8 fude_row_close_menu(fude_row* _row) {
    if(!_row->menu_open) {
        return false;
    }
    fude_row_menu_show(_row, false);
    return true;
}

// The card, over the row's right end: an item a line, those under More only.
RDE_INTERNAL void fude_row_menu_open(fude_row* _row) {
    if(_row->_window == NULL) {
        return;
    }
    f32 _w = FUDE_ROW_ITEM_MIN_W;
    u32 _n = 0;
    for(u32 _i = 0; _i < FUDE_ROW_BUTTONS; _i++) {
        if(_row->items[_i] != NULL && _row->in_menu[_i]) {
            const c8* _label = _row->shown[_i].label[0] != 0 ? _row->shown[_i].label : _row->base[_i];
            _w = fmaxf(_w, fude_draw_text_width(fude_kit_font(), (f32)FUDE_KIT_FONT_SIZE, _label, FUDE_KIT_TEXT_SCALE * (f32)FUDE_KIT_FONT_SIZE) + 72.0f);
            _n++;
        }
    }
    if(_n == 0) {
        return;
    }
    const rde_vec_2F _size   = { _w + 2.0f * FUDE_ROW_PADDING, (f32)_n * (FUDE_ROW_ITEM_H + FUDE_ROW_SPACING) - FUDE_ROW_SPACING + 2.0f * FUDE_ROW_PADDING };
    const rde_vec_2F _center = fude_kit_clamp(_row->_window, (rde_vec_2F){ _row->center.x + _row->size.x * 0.5f - _size.x * 0.5f,
                                                                            _row->center.y + _row->size.y * 0.5f + FUDE_ROW_SPACING * 2.0f + _size.y * 0.5f }, _size);
    fude_kit_modal_place(&_row->menu, _row->_window, _center, _size);
    f32 _y = _size.y - FUDE_ROW_PADDING - FUDE_ROW_ITEM_H * 0.5f;   // card-local, from the top
    for(u32 _i = 0; _i < FUDE_ROW_BUTTONS; _i++) {
        if(_row->items[_i] == NULL) {
            continue;
        }
        rde_ui_node_set_active(rde_ui_button_as_node(_row->items[_i]), _row->in_menu[_i]);
        if(_row->in_menu[_i]) {
            fude_kit_place(rde_ui_button_as_node(_row->items[_i]), (rde_vec_2F){ _size.x * 0.5f, _y }, (rde_vec_2F){ _w, FUDE_ROW_ITEM_H });
            _y -= FUDE_ROW_ITEM_H + FUDE_ROW_SPACING;
        }
    }
    fude_row_menu_show(_row, true);
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_row_on_more(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_row* _row = (fude_row*)_user_data;
    if(_row->menu_open) {
        fude_row_menu_show(_row, false);
    } else {
        fude_row_menu_open(_row);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_row_on_menu_dismiss(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_row_menu_show((fude_row*)_user_data, false);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// An item: the card closes, then its button's press (fude_row_on_press).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_row_on_press(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_row_on_item(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    const fude_row_ref* _ref = (const fude_row_ref*)_user_data;
    fude_row_menu_show(_ref->row, false);
    return fude_row_on_press(_node, _info, _user_data);
}

// Laid out in _avail (UI units): every button, or — where they do not all fit —
// the primary, Back or Close, then what fits in order, at most FUDE_ROW_COMPACT_W
// each, and More last for the rest.
RDE_INTERNAL void fude_row_layout(fude_row* _row, f32 _avail) {
    const u32 _count = _row->def->count < FUDE_ROW_BUTTONS ? _row->def->count : FUDE_ROW_BUTTONS;
    f32       _natural = 2.0f * FUDE_ROW_PADDING;
    u32       _shown   = 0;
    f32       _w[FUDE_ROW_BUTTONS];
    for(u32 _i = 0; _i < _count; _i++) {
        _w[_i] = _row->widths[_i];
        if(_w[_i] > 0.0f) {
            _natural += _w[_i] + (_shown > 0 ? FUDE_ROW_SPACING : 0.0f);
            _shown++;
        }
    }
    memset(_row->in_menu, 0, sizeof(_row->in_menu));
    const b8 _overflow = _natural > _avail && _shown > 2u && _row->more != NULL;
    if(_overflow) {
        b8  _keep[FUDE_ROW_BUTTONS];
        f32 _left = _avail - 2.0f * FUDE_ROW_PADDING - FUDE_ROW_BUTTON_MIN_W;   // More's room
        for(u32 _i = 0; _i < _count; _i++) {
            const fude_row_button* _b = &_row->def->buttons[_i];
            _w[_i]    = fminf(_w[_i], FUDE_ROW_COMPACT_W);
            _keep[_i] = _w[_i] > 0.0f && (_b->look == FUDE_ROW_PRIMARY || _b->text == FUDE_TEXT_BACK || _b->text == FUDE_TEXT_CLOSE);
            if(_keep[_i]) {
                _left -= _w[_i] + FUDE_ROW_SPACING;
            }
        }
        for(u32 _i = 0; _i < _count; _i++) {
            if(_w[_i] <= 0.0f || _keep[_i]) {
                continue;
            }
            if(_left >= _w[_i] + FUDE_ROW_SPACING) {
                _left -= _w[_i] + FUDE_ROW_SPACING;
            } else {
                _row->in_menu[_i] = true;
            }
        }
    }
    f32 _x = FUDE_ROW_PADDING;
    for(u32 _i = 0; _i < _count; _i++) {
        if(_row->buttons[_i] == NULL) {
            continue;
        }
        rde_ui_node_set_active(rde_ui_button_as_node(_row->buttons[_i]), !_row->in_menu[_i]);
        if(!_row->in_menu[_i]) {
            fude_kit_place(rde_ui_button_as_node(_row->buttons[_i]), (rde_vec_2F){ _x + _w[_i] * 0.5f, fude_row_height() * 0.5f },
                           (rde_vec_2F){ _w[_i], FUDE_ROW_BUTTON_H });
            _x += _w[_i] + FUDE_ROW_SPACING;
        }
    }
    if(_row->more != NULL) {
        rde_ui_node_set_active(rde_ui_button_as_node(_row->more), _overflow);
        if(_overflow) {
            fude_kit_place(rde_ui_button_as_node(_row->more), (rde_vec_2F){ _x + FUDE_ROW_BUTTON_MIN_W * 0.5f, fude_row_height() * 0.5f },
                           (rde_vec_2F){ FUDE_ROW_BUTTON_MIN_W, FUDE_ROW_BUTTON_H });
            _x += FUDE_ROW_BUTTON_MIN_W + FUDE_ROW_SPACING;
        }
    }
    _row->size      = (rde_vec_2F){ _x - FUDE_ROW_SPACING + FUDE_ROW_PADDING, fude_row_height() };
    _row->_laid_for = _avail;
    fude_kit_place_at(rde_ui_image_as_node(_row->panel), _row->center, _row->size);
    if(_row->menu_open) {
        fude_row_menu_show(_row, false);
    }
}

void fude_row_create(fude_row* _row, rde_ui_node* _root, const fude_row_def* _def, struct fude_app* _app, void* _self, void (*_after)(struct fude_app*)) {
    memset(_row, 0, sizeof(*_row));
    _row->def   = _def;
    _row->app   = _app;
    _row->self  = _self;
    _row->after = _after;
    _row->panel = rde_ui_image_create(NULL);
    rde_ui_node* _node = rde_ui_image_as_node(_row->panel);
    rde_ui_node_set_blocks_input(_node, true);
    rde_ui_node_add_child(_root, _node);

    const u32 _count = _def->count < FUDE_ROW_BUTTONS ? _def->count : FUDE_ROW_BUTTONS;
    b8        _here[FUDE_ROW_BUTTONS];
    f32       _widths[FUDE_ROW_BUTTONS];
    u32       _shown = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        const fude_row_button* _b = &_def->buttons[_i];
        _here[_i] = _b->present == NULL || _b->present();
        _shown   += _here[_i] ? 1u : 0u;
        if(_b->counted) {
            fude_text_format(_row->base[_i], sizeof(_row->base[_i]), (FUDE_TEXT_)_b->text, (const fude_text_arg[]){ FUDE_TN(0) }, 1u);
        } else {
            snprintf(_row->base[_i], sizeof(_row->base[_i]), "%s", fude_text((FUDE_TEXT_)_b->text));
        }
    }

    // Each button as wide as its label needs; a label ending in a count ("Practice 0")
    // with room for three digits.
    f32 _total = 2.0f * FUDE_ROW_PADDING + FUDE_ROW_SPACING * (f32)(_shown > 0 ? _shown - 1u : 0u);
    for(u32 _i = 0; _i < _count; _i++) {
        _widths[_i] = 0.0f;
        if(!_here[_i]) {
            continue;
        }
        c8          _measure[FUDE_ROW_LABEL + 4];
        const usize _len          = strlen(_row->base[_i]);
        const b8    _count_at_end = _len >= 2 && _row->base[_i][_len - 1] == '0' && _row->base[_i][_len - 2] == ' ';
        snprintf(_measure, sizeof(_measure), _count_at_end ? "%s00" : "%s", _row->base[_i]);
        const f32 _w = fude_draw_text_width(fude_kit_font(), (f32)FUDE_KIT_FONT_SIZE, _measure, FUDE_KIT_CAPTION_PX) + 24.0f;
        _widths[_i]  = rde_math_clamp_f32(_w, FUDE_ROW_BUTTON_MIN_W, FUDE_ROW_BUTTON_MAX_W);
        _total      += _widths[_i];
    }
    _row->size = (rde_vec_2F){ _total, fude_row_height() };
    memcpy(_row->widths, _widths, sizeof(_row->widths));

    f32 _x = FUDE_ROW_PADDING;
    for(u32 _i = 0; _i < _count; _i++) {
        if(!_here[_i]) {
            continue;
        }
        _row->refs[_i]    = (fude_row_ref){ _row, _i };
        _row->buttons[_i] = fude_kit_button(_node, _row->base[_i], fude_row_on_press, &_row->refs[_i]);
        fude_kit_place(rde_ui_button_as_node(_row->buttons[_i]), (rde_vec_2F){ _x + _widths[_i] * 0.5f, _row->size.y * 0.5f },
                       (rde_vec_2F){ _widths[_i], FUDE_ROW_BUTTON_H });
        _x += _widths[_i] + FUDE_ROW_SPACING;
    }
    // More, for a screen too narrow for them all (a phone), and its card: an item
    // for each button, shown for those under More. A row of one or two needs none.
    if(_shown > 2u) {
        _row->more = fude_kit_button(_node, fude_text(FUDE_TEXT_MORE_MENU), fude_row_on_more, _row);
        rde_ui_node_set_active(rde_ui_button_as_node(_row->more), false);
        fude_kit_modal_create(&_row->menu, _root, fude_row_on_menu_dismiss, _row);
        rde_ui_node* _card = rde_ui_image_as_node(_row->menu.card);
        for(u32 _i = 0; _i < _count; _i++) {
            if(_here[_i]) {
                _row->items[_i] = fude_kit_button(_card, _row->base[_i], fude_row_on_item, &_row->refs[_i]);
            }
        }
        fude_kit_modal_show(&_row->menu, false);
    }
    fude_row_restyle(_row);
    rde_ui_node_set_active(_node, false);
}

void fude_row_restyle(fude_row* _row) {
    fude_kit_style_panel(_row->panel, FUDE_ROW_RADIUS, 1.0f);
    if(_row->more != NULL) {
        fude_kit_icon(_row->more, FUDE_ICON_MORE, FUDE_KIT_ICON_ABOVE, FUDE_ROW_ICON_PX);
        fude_kit_button_quiet(_row->more);
        fude_kit_button_round(_row->more, FUDE_ROW_BUTTON_ROUND);
        fude_kit_modal_restyle(&_row->menu, FUDE_ROW_RADIUS);
    }
    for(u32 _i = 0; _i < FUDE_ROW_BUTTONS; _i++) {
        if(_row->buttons[_i] != NULL) {
            fude_row_paint(_row, _i);
        }
    }
}

RDE_INTERNAL b8 fude_row_same_face(const fude_row_face* _a, const fude_row_face* _b) {
    return _a->icon == _b->icon && _a->disabled == _b->disabled && _a->selected == _b->selected && strcmp(_a->label, _b->label) == 0;
}

void fude_row_apply(fude_row* _row, const fude_row_face* _faces) {
    for(u32 _i = 0; _i < FUDE_ROW_BUTTONS && _i < _row->def->count; _i++) {
        if(_row->buttons[_i] == NULL || fude_row_same_face(&_faces[_i], &_row->shown[_i])) {
            continue;
        }
        _row->shown[_i] = _faces[_i];
        fude_row_paint(_row, _i);
    }
}

rde_vec_2F fude_row_bottom(const fude_row* _row, rde_window* _window) {
    const rde_vec_2F _screen = fude_kit_screen_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    return (rde_vec_2F){ _screen.x * 0.5f, (f32)_insets.w + FUDE_KIT_SCREEN_EDGE + _row->size.y * 0.5f };
}

void fude_row_show(fude_row* _row, rde_window* _window, b8 _show, rde_vec_2F _center) {
    if(_row->panel == NULL) {
        return;
    }
    _row->_window = _window;
    if(_show) {
        const rde_vec_2F _screen = fude_kit_screen_size(_window);
        const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
        const f32        _avail  = _screen.x - (f32)(_insets.x + _insets.z) - 2.0f * FUDE_KIT_SCREEN_EDGE;
        if(_avail < _row->_laid_for - 0.5f || _avail > _row->_laid_for + 0.5f) {
            fude_row_layout(_row, _avail);
            _row->open = false;   // placed again below
        }
        _center = fude_kit_clamp(_window, _center, _row->size);
        if(!_row->open || memcmp(&_center, &_row->center, sizeof(rde_vec_2F)) != 0) {
            _row->center = _center;
            fude_kit_place_at(rde_ui_image_as_node(_row->panel), _center, _row->size);
        }
    }
    if(_show != _row->open) {
        _row->open = _show;
        rde_ui_node_set_active(rde_ui_image_as_node(_row->panel), _show);
    }
    if(!_show && _row->menu_open) {
        fude_row_menu_show(_row, false);
    }
}

b8 fude_row_hit(const fude_row* _row, rde_vec_2F _ui) {
    if(_row->menu_open) {
        return true;   // its card's backdrop covers the screen
    }
    return _row->open && _ui.x >= _row->center.x - _row->size.x * 0.5f && _ui.x <= _row->center.x + _row->size.x * 0.5f &&
           _ui.y >= _row->center.y - _row->size.y * 0.5f && _ui.y <= _row->center.y + _row->size.y * 0.5f;
}
