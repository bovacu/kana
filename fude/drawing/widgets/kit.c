// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/widgets/kit.h"
#include "drawing/base/theme.h"
#include "drawing/widgets/draw.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See kit.h.
// ===========================================================================

// The fonts (fude_kit_set_fonts): the UI font (Roboto, its fallbacks behind it)
// for a button's label and an icon that is a Japanese character (字, あ), and
// Phosphor's two weights.
RDE_INTERNAL rde_font* fude_kit_text_font     = NULL;
RDE_INTERNAL rde_font* fude_kit_icons_regular = NULL;
RDE_INTERNAL rde_font* fude_kit_icons_fill    = NULL;
RDE_INTERNAL rde_font* fude_kit_glyph_font    = NULL;
// An icon label's user data, telling it from the button's own label: a Phosphor
// icon (it turns Fill when its button is chosen) or a character.
RDE_INTERNAL const c8 fude_kit_tag_icon  = 1;
RDE_INTERNAL const c8 fude_kit_tag_glyph = 2;

void fude_kit_set_fonts(rde_font* _text, rde_font* _icons, rde_font* _icons_fill) {
    fude_kit_text_font     = _text;
    fude_kit_icons_regular = _icons != NULL ? _icons : _text;
    fude_kit_icons_fill    = _icons_fill;
    fude_kit_glyph_font    = _text;
}

rde_font* fude_kit_font(void) {
    return fude_kit_text_font;
}

// --- styles -------------------------------------------------------------------

// Each icon's left bearing (Phosphor units, 1024 an em): where its ink starts.
// A centred label lands a glyph its whole bearing right of centre: the text
// engine centres the line by its ink's right edge (half the bearing), then
// justifies the row by its ink span (the other half). fude_kit_icon moves
// it back. Measured from assets/fonts/Phosphor-Regular.ttf for the icons in icons.h (sorted by
// code point; Fill draws the same shapes filled). An icon not here is left as is.
static const struct { u32 codepoint; u16 bearing; } FUDE_KIT_ICON_BEARINGS[] = {
    { 0xE010u, 128 },
    { 0xE016u,  96 },
    { 0xE036u, 128 },
    { 0xE038u,  64 },
    { 0xE058u, 128 },
    { 0xE06Cu, 128 },
    { 0xE094u, 128 },
    { 0xE0E4u, 160 },
    { 0xE0E6u,  64 },
    { 0xE0EAu, 224 },
    { 0xE0F8u,  96 },
    { 0xE10Au, 128 },
    { 0xE10Eu,  96 },
    { 0xE136u, 160 },
    { 0xE138u, 288 },
    { 0xE13Au, 352 },
    { 0xE13Cu, 160 },
    { 0xE144u, 128 },
    { 0xE146u, 128 },
    { 0xE148u, 128 },
    { 0xE150u,  96 },
    { 0xE156u,  96 },
    { 0xE182u, 128 },
    { 0xE184u,  96 },
    { 0xE18Au,  96 },
    { 0xE18Cu,  96 },
    { 0xE196u, 160 },
    { 0xE198u, 160 },
    { 0xE19Au,  96 },
    { 0xE1CAu, 128 },
    { 0xE1CCu, 128 },
    { 0xE1D6u,  64 },
    { 0xE1E6u, 160 },
    { 0xE1FCu, 192 },
    { 0xE1FEu, 192 },
    { 0xE21Eu,  96 },
    { 0xE220u,  32 },
    { 0xE224u,  32 },
    { 0xE230u, 160 },
    { 0xE236u, 160 },
    { 0xE242u, 160 },
    { 0xE244u, 160 },
    { 0xE24Au,  96 },
    { 0xE256u,  96 },
    { 0xE258u,  96 },
    { 0xE266u,  96 },
    { 0xE272u,  64 },
    { 0xE296u, 160 },
    { 0xE29Au, 128 },
    { 0xE2A2u,  96 },
    { 0xE2A6u,  96 },
    { 0xE2CAu,  96 },
    { 0xE2CEu,  96 },
    { 0xE2D8u,  64 },
    { 0xE2DCu, 160 },
    { 0xE2F0u, 128 },
    { 0xE2F2u, 128 },
    { 0xE2F6u, 127 },
    { 0xE308u, 128 },
    { 0xE30Cu,  95 },
    { 0xE31Au,  96 },
    { 0xE32Au, 128 },
    { 0xE34Cu, 128 },
    { 0xE392u,  32 },
    { 0xE39Cu,  96 },
    { 0xE39Eu, 160 },
    { 0xE3ACu, 128 },
    { 0xE3B4u, 128 },
    { 0xE3D0u, 256 },
    { 0xE3D4u, 128 },
    { 0xE3DCu,  64 },
    { 0xE3F0u,  96 },
    { 0xE3F6u,  96 },
    { 0xE422u,  96 },
    { 0xE432u,  96 },
    { 0xE444u, 160 },
    { 0xE44Au,  64 },
    { 0xE45Eu, 128 },
    { 0xE466u,  96 },
    { 0xE468u,  32 },
    { 0xE46Au,  64 },
    { 0xE47Cu,  95 },
    { 0xE48Au, 192 },
    { 0xE4A2u,  96 },
    { 0xE4A6u, 128 },
    { 0xE4ACu,  64 },
    { 0xE4B0u,  64 },
    { 0xE4B2u,  64 },
    { 0xE4F6u, 192 },
    { 0xE506u, 128 },
    { 0xE50Au, 128 },
    { 0xE50Cu,  96 },
    { 0xE50Eu, 128 },
    { 0xE510u,  96 },
    { 0xE512u, 128 },
    { 0xE538u, 160 },
    { 0xE53Au,  32 },
    { 0xE546u, 192 },
    { 0xE5A2u, 128 },
    { 0xE5ACu, 128 },
    { 0xE606u,  64 },
    { 0xE626u,  96 },
    { 0xE62Cu,   0 },
    { 0xE654u,  96 },
    { 0xE67Cu,  32 },
    { 0xE67Eu,  32 },
    { 0xE682u, 160 },
    { 0xE69Au, 128 },
    { 0xE69Eu, 128 },
    { 0xE6B8u,  64 },
    { 0xE6C8u,  96 },
    { 0xE6CEu, 128 },
    { 0xE6D2u, 128 },
    { 0xE6ECu,  32 },
    { 0xE6EEu,  32 },
    { 0xE702u, 160 },
    { 0xE742u,  96 },
    { 0xE746u, 128 },
    { 0xE74Eu,  32 },
    { 0xE758u, 160 },
    { 0xE792u,  96 },
    { 0xE794u, 192 },
    { 0xE7BCu,  96 },
    { 0xE806u, 132 },
    { 0xEA0Eu, 224 },
    { 0xEA38u, 160 },
    { 0xEA9Au,  64 },
    { 0xEADCu, 128 },
    { 0xEAE0u,  96 },
    { 0xEAE2u, 320 },
    { 0xEAF0u, 160 },
    { 0xEB06u,  64 },
    { 0xEB58u,  95 },
    { 0xEBB6u, 128 },
    { 0xEC5Eu,  64 },
    { 0xEC76u,  64 },
    { 0xEDC6u,  64 },
    { 0xEDF2u, 192 },
};

f32 fude_kit_icon_bearing(const c8* _glyph) {
    const u8* _u = (const u8*)_glyph;
    if((_u[0] & 0xF0u) != 0xE0u) {
        return 0.0f;   // not a three-byte character: not Phosphor's
    }
    const u32 _cp = ((u32)(_u[0] & 0x0Fu) << 12) | ((u32)(_u[1] & 0x3Fu) << 6) | (u32)(_u[2] & 0x3Fu);
    u32 _lo = 0, _hi = (u32)(sizeof(FUDE_KIT_ICON_BEARINGS) / sizeof(FUDE_KIT_ICON_BEARINGS[0]));
    while(_lo < _hi) {
        const u32 _mid = (_lo + _hi) / 2u;
        if(FUDE_KIT_ICON_BEARINGS[_mid].codepoint < _cp) { _lo = _mid + 1u; }
        else                                                 { _hi = _mid; }
    }
    return _lo < sizeof(FUDE_KIT_ICON_BEARINGS) / sizeof(FUDE_KIT_ICON_BEARINGS[0]) && FUDE_KIT_ICON_BEARINGS[_lo].codepoint == _cp
         ? (f32)FUDE_KIT_ICON_BEARINGS[_lo].bearing / 1024.0f : 0.0f;
}

f32 fude_kit_icon_back(const c8* _glyph, f32 _px) {
    return fude_kit_icon_bearing(_glyph) * _px * FUDE_KIT_EM * (fude_draw_is_rtl() ? -1.0f : 1.0f);
}

rde_ui_style fude_kit_style(rde_color _tint, f32 _radius) {
    rde_ui_style _s  = rde_ui_style_default();
    _s.tint          = _tint;
    _s.corner_radius = _radius;
    return _s;
}

rde_color fude_kit_shade(rde_color _c, i32 _delta) {
    const i32 _r = (i32)_c.r + _delta;
    const i32 _g = (i32)_c.g + _delta;
    const i32 _b = (i32)_c.b + _delta;
    return (rde_color){ (u8)(_r < 0 ? 0 : (_r > 255 ? 255 : _r)),
                        (u8)(_g < 0 ? 0 : (_g > 255 ? 255 : _g)),
                        (u8)(_b < 0 ? 0 : (_b > 255 ? 255 : _b)), _c.a };
}

RDE_INTERNAL b8 fude_kit_same_color(rde_color _a, rde_color _b) {
    return _a.r == _b.r && _a.g == _b.g && _a.b == _b.b && _a.a == _b.a;
}

// Pressed a little: darker on a light colour, lighter on a dark one.
RDE_INTERNAL rde_color fude_kit_press(rde_color _c, i32 _amount) {
    const f32 _luma = 0.299f * (f32)_c.r + 0.587f * (f32)_c.g + 0.114f * (f32)_c.b;
    return fude_kit_shade(_c, _luma > 128.0f ? -_amount : _amount);
}

// The button's icon label (fude_kit_icon), NULL when it has none.
RDE_INTERNAL rde_ui_label* fude_kit_icon_of(rde_ui_button* _button) {
    rde_ui_node* _node = rde_ui_button_as_node(_button);
    for(u32 _i = 0; _i < rde_ui_node_get_child_count(_node); _i++) {
        rde_ui_node* _child = rde_ui_node_get_child(_node, _i);
        const any    _tag   = _child->user_data;
        if(_tag == (any)&fude_kit_tag_icon || _tag == (any)&fude_kit_tag_glyph) {
            return (rde_ui_label*)_child;
        }
    }
    return NULL;
}

// What goes on the button's colour: on the accent (or Delete's red), white; on
// the accent's tint (chosen), the accent; on anything else, the text colour.
RDE_INTERNAL rde_color fude_kit_label_color(rde_ui_button* _button) {
    const fude_theme* _t  = fude_theme_active();
    const rde_color   _bg = _button->styles.styles[RDE_UI_STATE_NORMAL].tint;
    if(fude_kit_same_color(_bg, _t->accent) || fude_kit_same_color(_bg, _t->danger)) {
        return _t->on_accent;
    }
    if(fude_kit_same_color(_bg, _t->tint)) {
        return _t->accent;
    }
    return _t->button_text;
}

// The label and the icon in _color.
RDE_INTERNAL void fude_kit_paint(rde_ui_button* _button, rde_color _color) {
    if(_button->internal_label != NULL) {
        rde_ui_label_set_color(_button->internal_label, _color);
    }
    rde_ui_label* _icon = fude_kit_icon_of(_button);
    if(_icon != NULL) {
        rde_ui_label_set_color(_icon, _color);
    }
}

// Normal / hovered / pressed / disabled from one base colour, with an optional
// border; the label and icon coloured to go on it. A clear base (a quiet
// button) shows the plain button's colour while pressed.
void fude_kit_button_colors(rde_ui_button* _button, rde_color _base, f32 _border_width, rde_color _border) {
    const fude_theme* _t     = fude_theme_active();
    const b8          _clear = _base.a == 0;
    rde_ui_style      _s     = fude_kit_style(_base, 10.0f);
    _s.border_width  = _border_width;
    _s.border_color  = _border;
    rde_ui_button_set_style(_button, RDE_UI_STATE_NORMAL, _s);
#if defined(RDE_PLATFORM_MOBILE)
    // No hover on a touch screen: a finger lifted leaves the pointer on what it
    // tapped, and a hover tint there would look like the button left on.
#else
    _s.tint = _clear ? _t->button : fude_kit_press(_base, 8);
#endif
    rde_ui_button_set_style(_button, RDE_UI_STATE_HOVERED, _s);
    _s.tint = _clear ? fude_kit_press(_t->button, 14) : fude_kit_press(_base, 22);
    rde_ui_button_set_style(_button, RDE_UI_STATE_PRESSED, _s);
    // Disabled: a coloured button goes plain (its label greys, fude_kit_set_enabled).
    const b8 _strong = fude_kit_same_color(_base, _t->accent) || fude_kit_same_color(_base, _t->danger) ||
                       fude_kit_same_color(_base, _t->tint);
    _s.tint = _strong ? _t->button : _base;
    rde_ui_button_set_style(_button, RDE_UI_STATE_DISABLED, _s);
    fude_kit_paint(_button, rde_ui_button_as_node(_button)->interactable ? fude_kit_label_color(_button) : _t->button_text_disabled);
}

// A Phosphor icon in its regular weight, or Fill (a chosen button's).
RDE_INTERNAL void fude_kit_icon_weight(rde_ui_button* _button, b8 _fill) {
    rde_ui_label* _icon = fude_kit_icon_of(_button);
    if(_icon != NULL && rde_ui_label_as_node(_icon)->user_data == (any)&fude_kit_tag_icon) {
        rde_font* _font = _fill && fude_kit_icons_fill != NULL ? fude_kit_icons_fill : fude_kit_icons_regular;
        if(_font != NULL) {
            rde_ui_label_set_font(_icon, _font);
        }
    }
}

void fude_kit_button_plain(rde_ui_button* _button) {
    fude_kit_button_colors(_button, fude_theme_active()->button, 0.0f, (rde_color){ 0, 0, 0, 0 });
    fude_kit_icon_weight(_button, false);
}

void fude_kit_button_quiet(rde_ui_button* _button) {
    fude_kit_button_colors(_button, (rde_color){ 0, 0, 0, 0 }, 0.0f, (rde_color){ 0, 0, 0, 0 });
    fude_kit_icon_weight(_button, false);
}

void fude_kit_button_selected(rde_ui_button* _button) {
    fude_kit_button_colors(_button, fude_theme_active()->tint, 0.0f, (rde_color){ 0, 0, 0, 0 });
    fude_kit_icon_weight(_button, true);
}

void fude_kit_button_primary(rde_ui_button* _button) {
    fude_kit_button_colors(_button, fude_theme_active()->accent, 0.0f, (rde_color){ 0, 0, 0, 0 });
    fude_kit_icon_weight(_button, false);
}

void fude_kit_button_danger(rde_ui_button* _button) {
    fude_kit_button_colors(_button, fude_theme_active()->danger, 0.0f, (rde_color){ 0, 0, 0, 0 });
    fude_kit_icon_weight(_button, false);
}

// Quiet, its label and icon in the danger colour (Delete in a row of quiet ones).
void fude_kit_button_danger_quiet(rde_ui_button* _button) {
    fude_kit_button_quiet(_button);
    if(rde_ui_button_as_node(_button)->interactable) {
        fude_kit_paint(_button, fude_theme_active()->danger);
    }
}

// A chip (Browse's filters and sorts): a pill, the accent when chosen.
void fude_kit_button_chip(rde_ui_button* _button, b8 _on) {
    if(_on) {
        fude_kit_button_primary(_button);
    } else {
        fude_kit_button_plain(_button);
    }
    fude_kit_button_round(_button, 999.0f);
}

// Every state's corners (the looks above are 10 units).
void fude_kit_button_round(rde_ui_button* _button, f32 _radius) {
    const RDE_UI_STATE_ _states[] = { RDE_UI_STATE_NORMAL, RDE_UI_STATE_HOVERED, RDE_UI_STATE_PRESSED, RDE_UI_STATE_DISABLED };
    for(u32 _i = 0; _i < sizeof(_states) / sizeof(_states[0]); _i++) {
        rde_ui_style _s  = _button->styles.styles[_states[_i]];
        _s.corner_radius = _radius;
        rde_ui_button_set_style(_button, _states[_i], _s);
    }
}

rde_color fude_kit_text_on(rde_color _background) {
    const f32 _luma = 0.299f * (f32)_background.r + 0.587f * (f32)_background.g + 0.114f * (f32)_background.b;
    return _luma > 140.0f ? (rde_color){ 20, 20, 24, 255 } : (rde_color){ 240, 240, 244, 255 };
}

// A panel (the bar, the palette, a menu, a card) in the theme's colours: the
// surface, a hairline border.
void fude_kit_style_panel(rde_ui_image* _panel, f32 _radius, f32 _border_width) {
    rde_ui_style _s = fude_kit_style(fude_theme_active()->panel, _radius);
    _s.border_width = _border_width;
    _s.border_color = fude_theme_active()->panel_border;
    rde_ui_image_set_style(_panel, RDE_UI_STATE_NORMAL, _s);
}

// --- creation helpers ----------------------------------------------------------

// Every child is pinned bottom-left to its parent with a centre pivot, so
// set_position places its CENTRE in parent-local units (bottom-left origin).
void fude_kit_place(rde_ui_node* _node, rde_vec_2F _center, rde_vec_2F _size) {
    rde_ui_node_set_anchor_preset(_node, RDE_UI_ANCHOR_PRESET_BOTTOM_LEFT);
    rde_ui_node_set_pivot(_node, (rde_vec_2F){ 0.5f, 0.5f });
    rde_ui_node_set_size(_node, _size);
    rde_ui_node_set_position(_node, _center);
}

// fude_kit_place for a point on the screen (where the bar was dragged, where a
// tap opened a menu): a right-to-left UI does not mirror it (rde.h:
// rde_ui_node_set_physical_placement), while what it holds still is.
void fude_kit_place_at(rde_ui_node* _node, rde_vec_2F _center, rde_vec_2F _size) {
    rde_ui_node_set_physical_placement(_node, true);
    fude_kit_place(_node, _center, _size);
}

// Pins _node to its parent's edges: its rect is the parent's anchor box
// (_anchor_min.._anchor_max, fractions) moved in by _inset_min / _inset_max.
void fude_kit_pin(rde_ui_node* _node, rde_vec_2F _anchor_min, rde_vec_2F _anchor_max, rde_vec_2F _inset_min, rde_vec_2F _inset_max) {
    rde_ui_node_set_anchors(_node, _anchor_min, _anchor_max);
    rde_ui_node_set_pivot(_node, (rde_vec_2F){ 0.5f, 0.5f });
    rde_ui_node_set_offsets(_node, _inset_min, (rde_vec_2F){ -_inset_max.x, -_inset_max.y });
}

rde_ui_button* fude_kit_button(rde_ui_node* _parent, const c8* _text, rde_ui_event_callback _on_click, any _user_data) {
    rde_ui_button* _button = rde_ui_button_create(fude_kit_text_font, NULL);
    rde_ui_button_set_text(_button, _text);
    fude_kit_button_plain(_button);

    // Fit the text, never truncate it: shrink a long word into the button.
    rde_ui_label_set_font_scale(_button->internal_label, FUDE_KIT_TEXT_SCALE);
    rde_ui_label_set_auto_fit(_button->internal_label, true);
    rde_ui_label_set_auto_fit_min_scale(_button->internal_label, 0.3f);
    rde_ui_label_set_color(_button->internal_label, fude_theme_active()->button_text);

    rde_ui_button_set_on_click(_button, _on_click, _user_data);
    rde_ui_node_add_child(_parent, rde_ui_button_as_node(_button));
    return _button;
}

// Gives _button an icon (icons.h; or a Japanese character, drawn in the UI
// font), _px tall, set out as _at says: alone, over its label (a row's
// buttons), or before it (a list's rows, left-aligned). Again on the same button
// changes the icon.
// How far up a letter used as an icon has to go for its ink to sit in the middle.
// The label centres the font's line (its ascender to its descender), and letters
// sit on that line very differently: Arabic's م hangs below the baseline while ف
// and ك sit on it and rise, so each would land at a height of its own. Phosphor's
// icons are drawn centred on the line and need none.
RDE_INTERNAL f32 fude_kit_glyph_rise(rde_font* _font, const c8* _glyph, f32 _px) {
    if(_font == NULL) {
        return 0.0f;
    }
    rde_text_engine_shaped_text _s = rde_text_engine_shape_text(_font, NULL, _glyph);
    // Shape space: y down from the baseline (a glyph's top is negative).
    f32 _top = 0.0f, _bottom = 0.0f;
    b8  _ink = false;
    rde_arr_foreach(rde_text_engine_shaped_line, _line, &_s.lines) {
        rde_arr_foreach(rde_text_engine_shaped_char, _c, &_line->chars) {
            if(_c->bounding_box.height <= 0) {
                continue;
            }
            const f32 _t = (f32)_c->bounding_box.y, _b = (f32)(_c->bounding_box.y + _c->bounding_box.height);
            _top    = _ink && _top    < _t ? _top    : _t;
            _bottom = _ink && _bottom > _b ? _bottom : _b;
            _ink    = true;
        }
    }
    const f32 _line_mid = (-(f32)_s.max_ascender - (f32)_s.max_descender) * 0.5f;   // max_descender is negative
    rde_text_engine_free_shaped_text(&_s);
    return _ink ? ((_top + _bottom) * 0.5f - _line_mid) * _px / (f32)FUDE_KIT_FONT_SIZE : 0.0f;   // ink below the line's middle: up
}

void fude_kit_icon(rde_ui_button* _button, const c8* _glyph, FUDE_KIT_ICON_AT_ _at, f32 _px) {
    const b8      _phosphor = (u8)_glyph[0] == 0xEEu || (u8)_glyph[0] == 0xEFu;   // U+E000..U+F8FF: Phosphor's private use
    rde_ui_label* _icon     = fude_kit_icon_of(_button);
    if(_icon == NULL) {
        _icon = rde_ui_label_create(NULL);
        rde_ui_label_set_alignment(_icon, RDE_UI_LABEL_H_ALIGN_CENTER, RDE_UI_LABEL_V_ALIGN_MIDDLE);
        rde_ui_node* _n = rde_ui_label_as_node(_icon);
        rde_ui_node_set_raycast_target(_n, false);
        rde_ui_node_set_interactable(_n, false);
        rde_ui_node_add_child(rde_ui_button_as_node(_button), _n);
    }
    rde_ui_node* _n = rde_ui_label_as_node(_icon);
    rde_ui_node_set_user_data(_n, (any)(_phosphor ? &fude_kit_tag_icon : &fude_kit_tag_glyph));
    rde_font* _font = _phosphor ? fude_kit_icons_regular : fude_kit_glyph_font;
    if(_font != NULL) {
        rde_ui_label_set_font(_icon, _font);
    }
    rde_ui_label_set_font_scale(_icon, _px / (f32)FUDE_KIT_FONT_SIZE);
    rde_ui_label_set_text(_icon, _phosphor ? fude_draw_icon_dir(_glyph) : _glyph);   // a directional icon mirrored right to left

    rde_ui_label* _text = _button->internal_label;
    // Back left by the icon's bearing (see FUDE_KIT_ICON_BEARINGS, fude_kit_icon_back).
    const f32 _back = _phosphor ? fude_kit_icon_back(_glyph, _px) : 0.0f;
    // A letter centred on its ink (fude_kit_glyph_rise).
    const f32 _up = _phosphor ? 0.0f : fude_kit_glyph_rise(_font, _glyph, _px);
    if(_at == FUDE_KIT_ICON_ONLY) {
        fude_kit_pin(_n, (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 1.0f, 1.0f }, (rde_vec_2F){ -_back, _up }, (rde_vec_2F){ _back, -_up });
        if(_text != NULL) {
            rde_ui_node_set_active(rde_ui_label_as_node(_text), false);
        }
    } else if(_at == FUDE_KIT_ICON_ABOVE) {
        // The icon in the top of the button, the label a line under it.
        fude_kit_pin(_n, (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 1.0f, 1.0f }, (rde_vec_2F){ -_back, 20.0f + _up }, (rde_vec_2F){ _back, 3.0f - _up });
        if(_text != NULL) {
            fude_kit_pin(rde_ui_label_as_node(_text), (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 1.0f, 0.0f }, (rde_vec_2F){ 3.0f, 5.0f }, (rde_vec_2F){ 3.0f, -21.0f });
            rde_ui_label_set_font_scale(_text, FUDE_KIT_CAPTION_PX / (f32)FUDE_KIT_FONT_SIZE);
        }
    } else {
        // At the left, the label left-aligned after it.
        const f32 _em = _px * FUDE_KIT_EM;
        fude_kit_pin(_n, (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 0.0f, 1.0f }, (rde_vec_2F){ 10.0f - _back, _up }, (rde_vec_2F){ -(10.0f + _em - _back), -_up });
        if(_text != NULL) {
            fude_kit_pin(rde_ui_label_as_node(_text), (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 1.0f, 1.0f }, (rde_vec_2F){ 20.0f + _em, 0.0f }, (rde_vec_2F){ 8.0f, 0.0f });
            rde_ui_label_set_alignment(_text, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_MIDDLE);
        }
    }
    fude_kit_paint(_button, rde_ui_button_as_node(_button)->interactable ? fude_kit_label_color(_button) : fude_theme_active()->button_text_disabled);
}

// The icon alone in another colour (a list's row: the icon in the accent).
void fude_kit_icon_color(rde_ui_button* _button, rde_color _color) {
    rde_ui_label* _icon = fude_kit_icon_of(_button);
    if(_icon != NULL && rde_ui_button_as_node(_button)->interactable) {
        rde_ui_label_set_color(_icon, _color);
    }
}

// A text field in its box: a rounded frame in the field's colour with a
// hairline, which holds the field (the field draws no background of its own —
// the text editor's is a plain rectangle). Placed and shown through the box
// (fude_kit_field_node); the field fills it.
void fude_kit_field_box(rde_ui_node* _parent, rde_ui_text_editor* _field) {
    rde_ui_image* _frame = rde_ui_image_create(NULL);
    rde_ui_node*  _n     = rde_ui_image_as_node(_frame);
    rde_ui_node_set_raycast_target(_n, false);
    rde_ui_node_set_user_data(_n, (any)&fude_kit_tag_glyph);   // known again by fude_kit_field_node
    rde_ui_node_add_child(_parent, _n);
    rde_ui_node* _f = rde_ui_text_editor_as_node(_field);
    rde_ui_node_add_child(_n, _f);
    fude_kit_pin(_f, (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 1.0f, 1.0f }, (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 0.0f, 0.0f });
}

rde_ui_node* fude_kit_field_node(rde_ui_text_editor* _field) {
    rde_ui_node* _f      = rde_ui_text_editor_as_node(_field);
    rde_ui_node* _parent = rde_ui_node_get_parent(_f);
    return _parent != NULL && _parent->user_data == (any)&fude_kit_tag_glyph ? _parent : _f;
}

// A field in the theme's colours: its box, its text, its caret.
void fude_kit_style_field(rde_ui_text_editor* _field) {
    const fude_theme* _t = fude_theme_active();
    rde_ui_text_editor_set_background(_field, false, _t->field);
    rde_ui_text_editor_set_placeholder_color(_field, _t->field_placeholder);
    rde_ui_text_editor_set_text_color(_field, _t->text);
    rde_ui_text_editor_set_caret_color(_field, _t->accent);
    rde_ui_node* _box = fude_kit_field_node(_field);
    if(_box != rde_ui_text_editor_as_node(_field)) {
        rde_ui_style _s = fude_kit_style(_t->field, 10.0f);
        _s.border_width = 1.0f;
        _s.border_color = _t->field_border;
        rde_ui_image_set_style((rde_ui_image*)_box, RDE_UI_STATE_NORMAL, _s);
    }
}

void fude_kit_set_enabled(rde_ui_button* _button, b8 _enabled) {
    rde_ui_node_set_interactable(rde_ui_button_as_node(_button), _enabled);
    fude_kit_paint(_button, _enabled ? fude_kit_label_color(_button) : fude_theme_active()->button_text_disabled);
}

// Quiet again (a row's button, a tool), its label coloured by whether it can be pressed.
void fude_kit_restyle_quiet(rde_ui_button* _button) {
    fude_kit_button_quiet(_button);
    fude_kit_set_enabled(_button, rde_ui_button_as_node(_button)->interactable);
}

// A plain button again, its label coloured by whether it can be pressed.
void fude_kit_restyle_button(rde_ui_button* _button) {
    fude_kit_button_plain(_button);
    fude_kit_set_enabled(_button, rde_ui_button_as_node(_button)->interactable);
}

b8 fude_kit_compact(rde_window* _window) {
    const rde_vec_2F _s = fude_kit_screen_size(_window);
    return fminf(_s.x, _s.y) < FUDE_KIT_COMPACT_BELOW;
}

rde_vec_2F fude_kit_screen_size(rde_window* _window) {
    const rde_vec_2I _w = rde_window_get_size(_window);
    return (rde_vec_2F){ (f32)_w.x, (f32)_w.y };
}

// Keeps a rect of _size centred at _center inside the SAFE AREA — clear of the
// status bar, notch and home indicator. Done here rather than by the UI canvas:
// its own safe area moves the root (see the note where the canvas is made).
rde_vec_2F fude_kit_clamp(rde_window* _window, rde_vec_2F _center, rde_vec_2F _size) {
    const rde_vec_2F _screen = fude_kit_screen_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32 _min_x = (f32)_insets.x + FUDE_KIT_SCREEN_EDGE + _size.x * 0.5f;
    const f32 _max_x = _screen.x - (f32)_insets.z - FUDE_KIT_SCREEN_EDGE - _size.x * 0.5f;
    const f32 _min_y = (f32)_insets.w + FUDE_KIT_SCREEN_EDGE + _size.y * 0.5f;
    const f32 _max_y = _screen.y - (f32)_insets.y - FUDE_KIT_SCREEN_EDGE - _size.y * 0.5f;
    return (rde_vec_2F){
        _min_x <= _max_x ? rde_math_clamp_f32(_center.x, _min_x, _max_x) : (_min_x + _max_x) * 0.5f,
        _min_y <= _max_y ? rde_math_clamp_f32(_center.y, _min_y, _max_y) : (_min_y + _max_y) * 0.5f
    };
}

// --- a card over a dimmed backdrop ---------------------------------------------------------

#define FUDE_KIT_BACKDROP (rde_color){ 0, 0, 0, 110 }

void fude_kit_modal_create(fude_kit_modal* _modal, rde_ui_node* _root, rde_ui_event_callback _on_dismiss, any _user_data) {
    _modal->backdrop = rde_ui_button_create(NULL, NULL);
    rde_ui_button_set_on_click(_modal->backdrop, _on_dismiss, _user_data);
    rde_ui_node_add_child(_root, rde_ui_button_as_node(_modal->backdrop));
    _modal->card = rde_ui_image_create(NULL);
    rde_ui_node* _card = rde_ui_image_as_node(_modal->card);
    rde_ui_node_set_blocks_input(_card, true);
    rde_ui_node_add_child(_root, _card);
    rde_ui_node_set_active(rde_ui_button_as_node(_modal->backdrop), false);
    rde_ui_node_set_active(_card, false);
    _modal->shown = false;
}

void fude_kit_modal_place(fude_kit_modal* _modal, rde_window* _window, rde_vec_2F _center, rde_vec_2F _size) {
    const rde_vec_2F _screen = fude_kit_screen_size(_window);
    fude_kit_place(rde_ui_button_as_node(_modal->backdrop), (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.5f }, _screen);
    fude_kit_place(rde_ui_image_as_node(_modal->card), _center, _size);
}

void fude_kit_modal_show(fude_kit_modal* _modal, b8 _show) {
    if(_show != _modal->shown) {
        _modal->shown = _show;
        rde_ui_node_set_active(rde_ui_button_as_node(_modal->backdrop), _show);
        rde_ui_node_set_active(rde_ui_image_as_node(_modal->card), _show);
    }
}

void fude_kit_modal_restyle(fude_kit_modal* _modal, f32 _radius) {
    fude_kit_button_colors(_modal->backdrop, FUDE_KIT_BACKDROP, 0.0f, (rde_color){ 0, 0, 0, 0 });
    fude_kit_style_panel(_modal->card, _radius, 1.0f);
}
