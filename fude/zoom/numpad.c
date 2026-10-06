// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/numpad.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/widgets/icons.h"
#include <stdio.h>
#include <string.h>

#define FUDE_ZOOM_NUMPAD_KEY_W  58.0f
#define FUDE_ZOOM_NUMPAD_KEY_H  48.0f
#define FUDE_ZOOM_NUMPAD_GAP    6.0f
#define FUDE_ZOOM_NUMPAD_PAD    12.0f
#define FUDE_ZOOM_NUMPAD_COLS   5u
#define FUDE_ZOOM_NUMPAD_ROWS   5u      // of keys; the display and its working-out above them

// The keys, row by row from the top: what each types (NULL: does something).
typedef enum {
    FUDE_ZOOM_KEY_TYPE = 0,
    FUDE_ZOOM_KEY_BACK,
    FUDE_ZOOM_KEY_CLEAR,
    FUDE_ZOOM_KEY_UNIT,
    FUDE_ZOOM_KEY_CANCEL,
    FUDE_ZOOM_KEY_OK
} FUDE_ZOOM_KEY_;

RDE_INTERNAL const struct { const c8* label; const c8* types; u8 does; u8 col, row, span; } FUDE_ZOOM_NUMPAD_LAYOUT[FUDE_ZOOM_NUMPAD_KEYS] = {
    { "7", "7", FUDE_ZOOM_KEY_TYPE, 0, 0, 1 }, { "8", "8", FUDE_ZOOM_KEY_TYPE, 1, 0, 1 }, { "9", "9", FUDE_ZOOM_KEY_TYPE, 2, 0, 1 },
    { "", NULL, FUDE_ZOOM_KEY_BACK, 3, 0, 1 },  { "C", NULL, FUDE_ZOOM_KEY_CLEAR, 4, 0, 1 },
    { "4", "4", FUDE_ZOOM_KEY_TYPE, 0, 1, 1 }, { "5", "5", FUDE_ZOOM_KEY_TYPE, 1, 1, 1 }, { "6", "6", FUDE_ZOOM_KEY_TYPE, 2, 1, 1 },
    { "+", "+", FUDE_ZOOM_KEY_TYPE, 3, 1, 1 }, { "\xE2\x88\x92", "-", FUDE_ZOOM_KEY_TYPE, 4, 1, 1 },
    { "1", "1", FUDE_ZOOM_KEY_TYPE, 0, 2, 1 }, { "2", "2", FUDE_ZOOM_KEY_TYPE, 1, 2, 1 }, { "3", "3", FUDE_ZOOM_KEY_TYPE, 2, 2, 1 },
    { "\xC3\x97", "*", FUDE_ZOOM_KEY_TYPE, 3, 2, 1 }, { "/", "/", FUDE_ZOOM_KEY_TYPE, 4, 2, 1 },
    { "0", "0", FUDE_ZOOM_KEY_TYPE, 0, 3, 1 }, { ".", ".", FUDE_ZOOM_KEY_TYPE, 1, 3, 1 }, { "1 \xC2\xBD", " ", FUDE_ZOOM_KEY_TYPE, 2, 3, 1 },
    { "'", "'", FUDE_ZOOM_KEY_TYPE, 3, 3, 1 }, { "\"", "\"", FUDE_ZOOM_KEY_TYPE, 4, 3, 1 },
    { "", NULL, FUDE_ZOOM_KEY_CANCEL, 0, 4, 2 }, { "mm", NULL, FUDE_ZOOM_KEY_UNIT, 2, 4, 1 }, { "", NULL, FUDE_ZOOM_KEY_OK, 3, 4, 2 },
};

// The one numpad (a key's own data is its index).
RDE_INTERNAL fude_zoom_numpad* fude_zoom_numpad_the = NULL;

#define FUDE_ZOOM_NUMPAD_ASK_H 26.0f   // the line of what is asked for

RDE_INTERNAL rde_vec_2F fude_zoom_numpad_size(const fude_zoom_numpad* _pad) {
    return (rde_vec_2F){ FUDE_ZOOM_NUMPAD_PAD * 2.0f + (f32)FUDE_ZOOM_NUMPAD_COLS * FUDE_ZOOM_NUMPAD_KEY_W + (f32)(FUDE_ZOOM_NUMPAD_COLS - 1u) * FUDE_ZOOM_NUMPAD_GAP,
                         FUDE_ZOOM_NUMPAD_PAD * 2.0f + (f32)(FUDE_ZOOM_NUMPAD_ROWS + 1u) * (FUDE_ZOOM_NUMPAD_KEY_H + FUDE_ZOOM_NUMPAD_GAP) + 18.0f +
                         (_pad->asked[0] != 0 ? FUDE_ZOOM_NUMPAD_ASK_H : 0.0f) };
}

// What is typed, read again: the display, and beneath it what it works out to.
RDE_INTERNAL void fude_zoom_numpad_show_text(fude_zoom_numpad* _pad) {
    if(_pad->display == NULL) {
        return;
    }
    rde_ui_button_set_text(_pad->display, _pad->text[0] != 0 ? _pad->text : " ");
    fude_zoom_units_error _e;
    _pad->valid = _pad->angle ? fude_zoom_units_angle(_pad->text, _pad->vars, &_pad->value, &_e)
                              : fude_zoom_units_length(_pad->text, _pad->unit, _pad->vars, &_pad->value, &_e);
    c8 _said[FUDE_ZOOM_UNITS_TEXT + 8u];
    if(_pad->text[0] == 0) {
        snprintf(_said, sizeof(_said), " ");
    } else if(!_pad->valid) {
        snprintf(_said, sizeof(_said), "?");
    } else if(_pad->angle) {
        c8 _v[FUDE_ZOOM_UNITS_TEXT];
        fude_zoom_units_format_angle(_pad->value, 2u, _v, sizeof(_v));
        snprintf(_said, sizeof(_said), "= %s", _v);
    } else {
        c8 _v[FUDE_ZOOM_UNITS_TEXT];
        const fude_zoom_units_style _style = { _pad->unit, 3u, 64u, _pad->unit == FUDE_ZOOM_UNIT_FT };
        fude_zoom_units_format(_pad->value, &_style, _v, sizeof(_v));
        snprintf(_said, sizeof(_said), "= %s", _v);
    }
    rde_ui_button_set_text(_pad->preview, _said);
    rde_ui_button_set_text(_pad->keys[21], fude_zoom_unit_suffix(_pad->unit));
    fude_kit_set_enabled(_pad->keys[22], _pad->valid);
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_numpad_on_key(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_numpad* _pad = fude_zoom_numpad_the;
    const u32 _k = (u32)(uintptr_t)_user_data;
    if(_pad == NULL || _k >= FUDE_ZOOM_NUMPAD_KEYS) {
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    const usize _len = strlen(_pad->text);
    switch(FUDE_ZOOM_NUMPAD_LAYOUT[_k].does) {
        case FUDE_ZOOM_KEY_TYPE:
            if(_len + strlen(FUDE_ZOOM_NUMPAD_LAYOUT[_k].types) < sizeof(_pad->text)) {
                strcat(_pad->text, FUDE_ZOOM_NUMPAD_LAYOUT[_k].types);
            }
            break;
        case FUDE_ZOOM_KEY_BACK:
            if(_len > 0) {
                _pad->text[_len - 1u] = 0;
            }
            break;
        case FUDE_ZOOM_KEY_CLEAR:
            _pad->text[0] = 0;
            break;
        case FUDE_ZOOM_KEY_UNIT:
            _pad->unit = (FUDE_ZOOM_UNIT_)((_pad->unit + 1u) % FUDE_ZOOM_UNIT_COUNT);
            break;
        case FUDE_ZOOM_KEY_CANCEL:
            fude_zoom_numpad_close(_pad);
            return RDE_UI_EVENT_RESULT_DEFAULT;
        case FUDE_ZOOM_KEY_OK:
            fude_zoom_numpad_show_text(_pad);
            if(_pad->valid) {
                fude_zoom_numpad_close(_pad);
                if(_pad->done != NULL) {
                    _pad->done(_pad->self, _pad->text, _pad->unit, _pad->value);
                }
            }
            return RDE_UI_EVENT_RESULT_DEFAULT;
        default: break;
    }
    fude_zoom_numpad_show_text(_pad);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_numpad_on_dismiss(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_numpad_close((fude_zoom_numpad*)_user_data);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

void fude_zoom_numpad_build(fude_zoom_numpad* _pad, rde_ui_node* _root, rde_window* _window) {
    _pad->window = _window;
    fude_kit_modal_create(&_pad->modal, _root, fude_zoom_numpad_on_dismiss, _pad);
    rde_ui_node* _card = rde_ui_image_as_node(_pad->modal.card);
    _pad->display = fude_kit_button(_card, " ", NULL, NULL);
    _pad->preview = fude_kit_button(_card, " ", NULL, NULL);
    _pad->prompt  = fude_kit_button(_card, " ", NULL, NULL);
    rde_ui_node_set_active(rde_ui_button_as_node(_pad->prompt), false);
    _pad->asked[0] = 0;
    for(u32 _i = 0; _i < FUDE_ZOOM_NUMPAD_KEYS; _i++) {
        _pad->keys[_i] = fude_kit_button(_card, FUDE_ZOOM_NUMPAD_LAYOUT[_i].label, fude_zoom_numpad_on_key, (any)(uintptr_t)_i);
    }
    fude_zoom_numpad_the = _pad;
    fude_kit_icon(_pad->keys[3], FUDE_ICON_BACK, FUDE_KIT_ICON_ONLY, 18.0f);
    fude_kit_icon(_pad->keys[20], FUDE_ICON_CLOSE, FUDE_KIT_ICON_ONLY, 18.0f);
    fude_kit_icon(_pad->keys[22], FUDE_ICON_CHECK, FUDE_KIT_ICON_ONLY, 20.0f);
    _pad->laid_out = (rde_vec_2F){ 0.0f, 0.0f };
    fude_zoom_numpad_restyle(_pad);
}

void fude_zoom_numpad_forget(fude_zoom_numpad* _pad) {
    memset(&_pad->modal, 0, sizeof(_pad->modal));
    _pad->display = NULL;
    _pad->preview = NULL;
    _pad->prompt  = NULL;
    memset(_pad->keys, 0, sizeof(_pad->keys));
}

void fude_zoom_numpad_restyle(fude_zoom_numpad* _pad) {
    if(_pad->display == NULL) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    fude_kit_modal_restyle(&_pad->modal, 16.0f);
    fude_kit_button_colors(_pad->display, _t->field, 1.0f, _t->field_border);
    fude_kit_button_round(_pad->display, 10.0f);
    fude_kit_button_colors(_pad->preview, _t->surface, 0.0f, _t->surface);
    fude_kit_button_colors(_pad->prompt, _t->surface, 0.0f, _t->surface);
    for(u32 _i = 0; _i < FUDE_ZOOM_NUMPAD_KEYS; _i++) {
        const b8 _ok = FUDE_ZOOM_NUMPAD_LAYOUT[_i].does == FUDE_ZOOM_KEY_OK;
        fude_kit_button_colors(_pad->keys[_i], _ok ? _t->accent : _t->surface_2, 1.0f, _t->outline);
        fude_kit_button_round(_pad->keys[_i], 10.0f);
        if(_ok) {
            fude_kit_icon_color(_pad->keys[_i], _t->on_accent);
        }
    }
}

void fude_zoom_numpad_update(fude_zoom_numpad* _pad) {
    if(_pad->display == NULL || !_pad->modal.shown) {
        return;
    }
    const rde_vec_2F _screen = fude_kit_screen_size(_pad->window);
    if(_screen.x == _pad->laid_out.x && _screen.y == _pad->laid_out.y) {
        return;
    }
    _pad->laid_out = _screen;
    // The card low in the middle, clear of the bottom row of chips; its keys in its own units (bottom-left origin).
    const rde_vec_2F _size = fude_zoom_numpad_size(_pad);
    fude_kit_modal_place(&_pad->modal, _pad->window, (rde_vec_2F){ _screen.x * 0.5f, 70.0f + _size.y * 0.5f }, _size);
    const f32 _kw = FUDE_ZOOM_NUMPAD_KEY_W, _kh = FUDE_ZOOM_NUMPAD_KEY_H, _g = FUDE_ZOOM_NUMPAD_GAP, _p = FUDE_ZOOM_NUMPAD_PAD;
    for(u32 _i = 0; _i < FUDE_ZOOM_NUMPAD_KEYS; _i++) {
        const f32 _span = (f32)FUDE_ZOOM_NUMPAD_LAYOUT[_i].span;
        const f32 _w    = _kw * _span + _g * (_span - 1.0f);
        const f32 _x    = _p + (f32)FUDE_ZOOM_NUMPAD_LAYOUT[_i].col * (_kw + _g) + _w * 0.5f;
        const f32 _y    = _p + (f32)(FUDE_ZOOM_NUMPAD_ROWS - 1u - FUDE_ZOOM_NUMPAD_LAYOUT[_i].row) * (_kh + _g) + _kh * 0.5f;
        fude_kit_place(rde_ui_button_as_node(_pad->keys[_i]), (rde_vec_2F){ _x, _y }, (rde_vec_2F){ _w, _kh });
    }
    const f32 _wide = _size.x - _p * 2.0f;
    const f32 _top  = _p + (f32)FUDE_ZOOM_NUMPAD_ROWS * (_kh + _g);
    fude_kit_place(rde_ui_button_as_node(_pad->preview), (rde_vec_2F){ _size.x * 0.5f, _top + 9.0f }, (rde_vec_2F){ _wide, 18.0f });
    fude_kit_place(rde_ui_button_as_node(_pad->display), (rde_vec_2F){ _size.x * 0.5f, _top + 18.0f + _g + _kh * 0.5f }, (rde_vec_2F){ _wide, _kh });
    fude_kit_place(rde_ui_button_as_node(_pad->prompt), (rde_vec_2F){ _size.x * 0.5f, _top + 18.0f + 2.0f * _g + _kh + FUDE_ZOOM_NUMPAD_ASK_H * 0.5f - 2.0f },
                   (rde_vec_2F){ _wide, FUDE_ZOOM_NUMPAD_ASK_H });
}

void fude_zoom_numpad_open(fude_zoom_numpad* _pad, const c8* _text, FUDE_ZOOM_UNIT_ _unit, b8 _angle, const fude_zoom_vars* _vars,
                           fude_zoom_numpad_done _done, void* _self) {
    if(_pad->display == NULL) {
        return;
    }
    snprintf(_pad->text, sizeof(_pad->text), "%s", _text != NULL ? _text : "");
    _pad->unit  = _unit;
    _pad->angle = _angle;
    _pad->vars  = _vars;
    _pad->done  = _done;
    _pad->self  = _self;
    _pad->asked[0] = 0;
    rde_ui_node_set_active(rde_ui_button_as_node(_pad->prompt), false);
    fude_kit_modal_show(&_pad->modal, true);
    _pad->laid_out = (rde_vec_2F){ 0.0f, 0.0f };
    fude_zoom_numpad_update(_pad);
    fude_zoom_numpad_show_text(_pad);
}

void fude_zoom_numpad_ask(fude_zoom_numpad* _pad, const c8* _asked) {
    if(_pad->prompt == NULL) {
        return;
    }
    snprintf(_pad->asked, sizeof(_pad->asked), "%s", _asked != NULL ? _asked : "");
    rde_ui_button_set_text(_pad->prompt, _pad->asked[0] != 0 ? _pad->asked : " ");
    rde_ui_node_set_active(rde_ui_button_as_node(_pad->prompt), _pad->asked[0] != 0 && _pad->modal.shown);
    _pad->laid_out = (rde_vec_2F){ 0.0f, 0.0f };   // (the card taller)
    fude_zoom_numpad_update(_pad);
}

void fude_zoom_numpad_close(fude_zoom_numpad* _pad) {
    if(_pad->display != NULL) {
        fude_kit_modal_show(&_pad->modal, false);
    }
}

b8 fude_zoom_numpad_shown(const fude_zoom_numpad* _pad) {
    return _pad->display != NULL && _pad->modal.shown;
}
