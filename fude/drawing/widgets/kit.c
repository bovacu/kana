// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/widgets/kit.h"
#include "drawing/base/theme.h"
#include "drawing/base/draw.h"
#include "drawing/base/utf8.h"
#include "drawing/base/text.h"

// ===========================================================================
// See kit.h.
//
// A button's icon is an extra label added as the button's child; it is told apart from other children by its user
// data, which points at one of the tags below. The looks are worked out from one base colour per button, and the
// label's colour is worked out from that base again (fude_kit_label_color), so restyling after a theme change needs
// nothing remembered.
// ===========================================================================

// The fonts (fude_kit_set_fonts): the UI font (Roboto, with its fallbacks behind it) for a button's label and for an
// icon that is a character of a script (字, あ, م), and Phosphor's two weights.
RDE_INTERNAL rde_font* fude_kit_text_font     = NULL;
RDE_INTERNAL rde_font* fude_kit_icons_regular = NULL;
RDE_INTERNAL rde_font* fude_kit_icons_fill    = NULL;
// The tags an icon label's user data points at, telling it from the button's own label: a Phosphor icon (which turns
// Fill when its button is chosen) or a character. A text field's box (fude_kit_field_box) is told apart the same way.
// Only their addresses matter.
RDE_INTERNAL const c8 fude_kit_tag_icon      = 1;
RDE_INTERNAL const c8 fude_kit_tag_glyph     = 2;
RDE_INTERNAL const c8 fude_kit_tag_field_box = 3;

// Keeps the fonts; with no icon font, icons are drawn in the UI font.
void fude_kit_set_fonts(rde_font* _text, rde_font* _icons, rde_font* _icons_fill) {
    fude_kit_text_font     = _text;
    fude_kit_icons_regular = _icons != NULL ? _icons : _text;
    fude_kit_icons_fill    = _icons_fill;
}

// Returns the UI font, or NULL before fude_kit_set_fonts.
rde_font* fude_kit_font(void) {
    return fude_kit_text_font;
}

// --- styles -------------------------------------------------------------------

// Each icon's left bearing, in Phosphor's units (1024 to an em): where its ink starts. A centred label lands a glyph
// its whole bearing right of centre: the text engine centres the line by its ink's right edge (half the bearing),
// then justifies the row by its ink span (the other half). fude_kit_icon moves it back. Measured from
// assets/fonts/Phosphor-Regular.ttf for the icons in icons.h, and sorted by code point, because it is binary-searched.
// Fill draws the same shapes filled. An icon not in the table is left where it lands.
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
    { 0xE2C2u, 128 },
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
    { 0xE326u, 192 },
    { 0xE32Au, 128 },
    { 0xE34Cu, 128 },
    { 0xE392u,  32 },
    { 0xE398u,  32 },
    { 0xE39Cu,  96 },
    { 0xE39Eu, 160 },
    { 0xE3ACu, 128 },
    { 0xE3B4u, 128 },
    { 0xE3D0u, 256 },
    { 0xE3D4u, 128 },
    { 0xE3DCu,  64 },
    { 0xE3EEu,  96 },
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
    { 0xE46Cu, 160 },
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
    { 0xE5F4u,  32 },
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
    { 0xE802u, 160 },
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

// Returns the icon's left bearing in ems, looked up in FUDE_KIT_ICON_BEARINGS by the glyph's first character; 0 for
// anything not in the table (Phosphor's icons are in the Private Use Area, so a letter is never found).
f32 fude_kit_icon_bearing(const c8* _glyph) {
    u32       _len;
    const u32 _cp = fude_utf8_decode(_glyph, &_len);
    u32 _lo = 0, _hi = (u32)(sizeof(FUDE_KIT_ICON_BEARINGS) / sizeof(FUDE_KIT_ICON_BEARINGS[0]));
    while(_lo < _hi) {
        const u32 _mid = (_lo + _hi) / 2u;
        if(FUDE_KIT_ICON_BEARINGS[_mid].codepoint < _cp) { _lo = _mid + 1u; }
        else                                                 { _hi = _mid; }
    }
    return _lo < sizeof(FUDE_KIT_ICON_BEARINGS) / sizeof(FUDE_KIT_ICON_BEARINGS[0]) && FUDE_KIT_ICON_BEARINGS[_lo].codepoint == _cp
         ? (f32)FUDE_KIT_ICON_BEARINGS[_lo].bearing / 1024.0f : 0.0f;
}

// Returns how far left to move an icon _px tall: its bearing times its em, negative in a right-to-left UI.
f32 fude_kit_icon_back(const c8* _glyph, f32 _px) {
    return fude_kit_icon_bearing(_glyph) * _px * FUDE_KIT_EM * (fude_draw_is_rtl() ? -1.0f : 1.0f);
}

// Returns RDE's default style with _tint and _radius.
rde_ui_style fude_kit_style(rde_color _tint, f32 _radius) {
    rde_ui_style _s  = rde_ui_style_default();
    _s.tint          = _tint;
    _s.corner_radius = _radius;
    return _s;
}

// Returns whether two colours are the same in every channel.
RDE_INTERNAL b8 fude_kit_same_color(rde_color _a, rde_color _b) {
    return _a.r == _b.r && _a.g == _b.g && _a.b == _b.b && _a.a == _b.a;
}

// Returns _c shaded by _amount for a pressed or hovered look: darker on a light colour, lighter on a dark one (by its
// luma).
RDE_INTERNAL rde_color fude_kit_press(rde_color _c, i32 _amount) {
    const f32 _luma = 0.299f * (f32)_c.r + 0.587f * (f32)_c.g + 0.114f * (f32)_c.b;
    return fude_theme_shade(_c, _luma > 128.0f ? -_amount : _amount);
}

// Returns the button's icon label (fude_kit_icon), found among its children by its tag, or NULL when it has none.
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

// Returns the colour for a label on the button's normal colour: on the accent (or Delete's red), the colour on the
// accent (white); on the accent's tint (chosen), the accent; on anything else, the button's text colour.
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

// Colours the button's label and its icon in _color.
RDE_INTERNAL void fude_kit_paint(rde_ui_button* _button, rde_color _color) {
    if(_button->internal_label != NULL) {
        rde_ui_label_set_color(_button->internal_label, _color);
    }
    rde_ui_label* _icon = fude_kit_icon_of(_button);
    if(_icon != NULL) {
        rde_ui_label_set_color(_icon, _color);
    }
}

// Sets the normal, hovered, pressed and disabled looks from one base colour (corners of 10), with an optional border,
// and colours the label and icon to go on it. A clear base (a quiet button) shows the plain button's colour while
// pressed.
void fude_kit_button_colors(rde_ui_button* _button, rde_color _base, f32 _border_width, rde_color _border) {
    const fude_theme* _t     = fude_theme_active();
    const b8          _clear = _base.a == 0;
    rde_ui_style      _s     = fude_kit_style(_base, 10.0f);
    _s.border_width  = _border_width;
    _s.border_color  = _border;
    rde_ui_button_set_style(_button, RDE_UI_STATE_NORMAL, _s);
#if defined(RDE_PLATFORM_MOBILE)
    // No hover look on a touch screen (the hovered style stays the normal one): a lifted finger leaves the pointer on
    // what it tapped, and a hover tint there would look as if the button were left on.
#else
    _s.tint = _clear ? _t->button : fude_kit_press(_base, 8);
#endif
    rde_ui_button_set_style(_button, RDE_UI_STATE_HOVERED, _s);
    _s.tint = _clear ? fude_kit_press(_t->button, 14) : fude_kit_press(_base, 22);
    rde_ui_button_set_style(_button, RDE_UI_STATE_PRESSED, _s);
    // Disabled: a coloured button goes plain, and its label greys (fude_kit_set_enabled).
    const b8 _strong = fude_kit_same_color(_base, _t->accent) || fude_kit_same_color(_base, _t->danger) ||
                       fude_kit_same_color(_base, _t->tint);
    _s.tint = _strong ? _t->button : _base;
    rde_ui_button_set_style(_button, RDE_UI_STATE_DISABLED, _s);
    fude_kit_paint(_button, rde_ui_button_as_node(_button)->interactable ? fude_kit_label_color(_button) : _t->button_text_disabled);
}

// Draws the button's Phosphor icon in its regular weight, or in Fill (_fill, for a chosen button) when that font is
// set. A character used as an icon is left alone.
RDE_INTERNAL void fude_kit_icon_weight(rde_ui_button* _button, b8 _fill) {
    rde_ui_label* _icon = fude_kit_icon_of(_button);
    if(_icon != NULL && rde_ui_label_as_node(_icon)->user_data == (any)&fude_kit_tag_icon) {
        rde_font* _font = _fill && fude_kit_icons_fill != NULL ? fude_kit_icons_fill : fude_kit_icons_regular;
        if(_font != NULL) {
            rde_ui_label_set_font(_icon, _font);
        }
    }
}

// The plain look: the theme's button colour, a regular icon.
void fude_kit_button_plain(rde_ui_button* _button) {
    fude_kit_button_colors(_button, fude_theme_active()->button, 0.0f, (rde_color){ 0, 0, 0, 0 });
    fude_kit_icon_weight(_button, false);
}

// The quiet look: clear until pressed, a regular icon.
void fude_kit_button_quiet(rde_ui_button* _button) {
    fude_kit_button_colors(_button, (rde_color){ 0, 0, 0, 0 }, 0.0f, (rde_color){ 0, 0, 0, 0 });
    fude_kit_icon_weight(_button, false);
}

// The selected look: the accent's tint, a Fill icon.
void fude_kit_button_selected(rde_ui_button* _button) {
    fude_kit_button_colors(_button, fude_theme_active()->tint, 0.0f, (rde_color){ 0, 0, 0, 0 });
    fude_kit_icon_weight(_button, true);
}

// The primary look: the accent, a regular icon.
void fude_kit_button_primary(rde_ui_button* _button) {
    fude_kit_button_colors(_button, fude_theme_active()->accent, 0.0f, (rde_color){ 0, 0, 0, 0 });
    fude_kit_icon_weight(_button, false);
}

// The danger look: the theme's danger colour, a regular icon.
void fude_kit_button_danger(rde_ui_button* _button) {
    fude_kit_button_colors(_button, fude_theme_active()->danger, 0.0f, (rde_color){ 0, 0, 0, 0 });
    fude_kit_icon_weight(_button, false);
}

// The quiet look with the label and icon in the danger colour (Delete in a row of quiet buttons); a disabled button
// keeps its grey.
void fude_kit_button_danger_quiet(rde_ui_button* _button) {
    fude_kit_button_quiet(_button);
    if(rde_ui_button_as_node(_button)->interactable) {
        fude_kit_paint(_button, fude_theme_active()->danger);
    }
}

// Styles a chip (Browse's filters and sorts) as a pill: primary when chosen, plain when not, with round ends.
void fude_kit_button_chip(rde_ui_button* _button, b8 _on) {
    if(_on) {
        fude_kit_button_primary(_button);
    } else {
        fude_kit_button_plain(_button);
    }
    fude_kit_button_round(_button, 999.0f);
}

// Sets every state's corner radius (the looks above use 10 units).
void fude_kit_button_round(rde_ui_button* _button, f32 _radius) {
    const RDE_UI_STATE_ _states[] = { RDE_UI_STATE_NORMAL, RDE_UI_STATE_HOVERED, RDE_UI_STATE_PRESSED, RDE_UI_STATE_DISABLED };
    for(u32 _i = 0; _i < sizeof(_states) / sizeof(_states[0]); _i++) {
        rde_ui_style _s  = _button->styles.styles[_states[_i]];
        _s.corner_radius = _radius;
        rde_ui_button_set_style(_button, _states[_i], _s);
    }
}

// Returns near-black on a light background (luma over 140), near-white on a dark one.
rde_color fude_kit_text_on(rde_color _background) {
    const f32 _luma = 0.299f * (f32)_background.r + 0.587f * (f32)_background.g + 0.114f * (f32)_background.b;
    return _luma > 140.0f ? (rde_color){ 20, 20, 24, 255 } : (rde_color){ 240, 240, 244, 255 };
}

// Styles a panel (the bar, the palette, a menu, a card) in the theme's colours: the panel colour and a border of
// _border_width in the panel's border colour.
void fude_kit_style_panel(rde_ui_image* _panel, f32 _radius, f32 _border_width) {
    rde_ui_style _s = fude_kit_style(fude_theme_active()->panel, _radius);
    _s.border_width = _border_width;
    _s.border_color = fude_theme_active()->panel_border;
    rde_ui_image_set_style(_panel, RDE_UI_STATE_NORMAL, _s);
}

// --- creation helpers ----------------------------------------------------------

// Pins _node bottom-left to its parent with a centre pivot, so set_position places its CENTRE in the parent's own
// units (bottom-left origin).
void fude_kit_place(rde_ui_node* _node, rde_vec_2F _center, rde_vec_2F _size) {
    rde_ui_node_set_anchor_preset(_node, RDE_UI_ANCHOR_PRESET_BOTTOM_LEFT);
    rde_ui_node_set_pivot(_node, (rde_vec_2F){ 0.5f, 0.5f });
    rde_ui_node_set_size(_node, _size);
    rde_ui_node_set_position(_node, _center);
}

// fude_kit_place for a point on the screen (where the bar was dragged, where a tap opened a menu): a right-to-left UI
// does not mirror its place (rde.h: rde_ui_node_set_physical_placement), while what it holds is still mirrored.
void fude_kit_place_at(rde_ui_node* _node, rde_vec_2F _center, rde_vec_2F _size) {
    rde_ui_node_set_physical_placement(_node, true);
    fude_kit_place(_node, _center, _size);
}

// Pins _node to its parent's edges: its rect is the parent's anchor box (_anchor_min to _anchor_max, as fractions)
// moved in by _inset_min and _inset_max.
void fude_kit_pin(rde_ui_node* _node, rde_vec_2F _anchor_min, rde_vec_2F _anchor_max, rde_vec_2F _inset_min, rde_vec_2F _inset_max) {
    rde_ui_node_set_anchors(_node, _anchor_min, _anchor_max);
    rde_ui_node_set_pivot(_node, (rde_vec_2F){ 0.5f, 0.5f });
    rde_ui_node_set_offsets(_node, _inset_min, (rde_vec_2F){ -_inset_max.x, -_inset_max.y });
}

// Makes a plain button with _text under _parent; its label shrinks to fit (down to 30%) rather than being cut.
rde_ui_button* fude_kit_button(rde_ui_node* _parent, const c8* _text, rde_ui_event_callback _on_click, any _user_data) {
    rde_ui_button* _button = rde_ui_button_create(fude_kit_text_font, NULL);
    rde_ui_button_set_text(_button, _text);
    fude_kit_button_plain(_button);

    // Fit the text and never cut it: a long word shrinks into the button.
    rde_ui_label_set_font_scale(_button->internal_label, FUDE_KIT_TEXT_SCALE);
    rde_ui_label_set_auto_fit(_button->internal_label, true);
    rde_ui_label_set_auto_fit_min_scale(_button->internal_label, 0.3f);
    rde_ui_label_set_color(_button->internal_label, fude_theme_active()->button_text);

    rde_ui_button_set_on_click(_button, _on_click, _user_data);
    rde_ui_node_add_child(_parent, rde_ui_button_as_node(_button));
    return _button;
}

// Returns how far up a letter used as an icon, _px tall, has to go for its ink to sit in the middle. The label centres
// the font's line (from its ascender to its descender), and letters sit on that line very differently: Arabic's م
// hangs below the baseline while ف and ك sit on it and rise, so each would land at a height of its own. Phosphor's
// icons are drawn centred on the line and need no rise. The text is shaped each call, so it is for building UI, not
// for every frame.
RDE_INTERNAL f32 fude_kit_glyph_rise(rde_font* _font, const c8* _glyph, f32 _px) {
    if(_font == NULL) {
        return 0.0f;
    }
    rde_text_engine_shaped_text _s = rde_text_engine_shape_text(_font, NULL, _glyph);
    // In shape space y points down from the baseline, so a glyph's top is negative.
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
    const f32 _line_mid = (-(f32)_s.max_ascender - (f32)_s.max_descender) * 0.5f;   // max_descender is negative.
    rde_text_engine_free_shaped_text(&_s);
    return _ink ? ((_top + _bottom) * 0.5f - _line_mid) * _px / (f32)FUDE_KIT_FONT_SIZE : 0.0f;   // Low ink goes up.
}

// Returns whether _glyph starts with a private-use character (U+E000 to U+F8FF), where Phosphor's icons are. Any other
// character is a script's, drawn in the UI font: the forms just past that area too (full-width letters, half-width
// kana), which the icon font does not have.
RDE_INTERNAL b8 fude_kit_is_phosphor(const c8* _glyph) {
    u32       _len = 0;
    const u32 _cp  = fude_utf8_decode(_glyph, &_len);
    return _cp >= 0xE000u && _cp <= 0xF8FFu;
}

// Gives _button an icon (from icons.h, or a character drawn in the UI font), _px tall, set out as _at says: alone,
// over its label (a row's buttons), or before it (a list's rows, left-aligned). Calling it again on the same button
// changes the icon. A Phosphor icon (fude_kit_is_phosphor) is moved back by its bearing; a character is raised so its
// ink is centred. A directional Phosphor icon is turned in a right-to-left UI (fude_draw_icon_dir).
void fude_kit_icon(rde_ui_button* _button, const c8* _glyph, FUDE_KIT_ICON_AT_ _at, f32 _px) {
    const b8      _phosphor = fude_kit_is_phosphor(_glyph);
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
    rde_font* _font = _phosphor ? fude_kit_icons_regular : fude_kit_text_font;
    if(_font != NULL) {
        rde_ui_label_set_font(_icon, _font);
    }
    rde_ui_label_set_font_scale(_icon, _px / (f32)FUDE_KIT_FONT_SIZE);
    rde_ui_label_set_text(_icon, _phosphor ? fude_draw_icon_dir(_glyph) : _glyph);   // Turned right to left.

    rde_ui_label* _text = _button->internal_label;
    // Move it back left by the icon's bearing (see FUDE_KIT_ICON_BEARINGS and fude_kit_icon_back).
    const f32 _back = _phosphor ? fude_kit_icon_back(_glyph, _px) : 0.0f;
    // Raise a letter so it is centred on its ink (fude_kit_glyph_rise).
    const f32 _up = _phosphor ? 0.0f : fude_kit_glyph_rise(_font, _glyph, _px);
    if(_at == FUDE_KIT_ICON_ONLY) {
        fude_kit_pin(_n, (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 1.0f, 1.0f }, (rde_vec_2F){ -_back, _up }, (rde_vec_2F){ _back, -_up });
        if(_text != NULL) {
            rde_ui_node_set_active(rde_ui_label_as_node(_text), false);
        }
    } else if(_at == FUDE_KIT_ICON_ABOVE) {
        // The icon in the top of the button, and the label on a line under it, at caption size.
        fude_kit_pin(_n, (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 1.0f, 1.0f }, (rde_vec_2F){ -_back, 20.0f + _up }, (rde_vec_2F){ _back, 3.0f - _up });
        if(_text != NULL) {
            fude_kit_pin(rde_ui_label_as_node(_text), (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 1.0f, 0.0f }, (rde_vec_2F){ 3.0f, 5.0f }, (rde_vec_2F){ 3.0f, -21.0f });
            rde_ui_label_set_font_scale(_text, FUDE_KIT_CAPTION_PX / (f32)FUDE_KIT_FONT_SIZE);
        }
    } else {
        // The icon at the left, 10 units in, and the label left-aligned after it.
        const f32 _em = _px * FUDE_KIT_EM;
        fude_kit_pin(_n, (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 0.0f, 1.0f }, (rde_vec_2F){ 10.0f - _back, _up }, (rde_vec_2F){ -(10.0f + _em - _back), -_up });
        if(_text != NULL) {
            fude_kit_pin(rde_ui_label_as_node(_text), (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 1.0f, 1.0f }, (rde_vec_2F){ 20.0f + _em, 0.0f }, (rde_vec_2F){ 8.0f, 0.0f });
            rde_ui_label_set_alignment(_text, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_MIDDLE);
        }
    }
    fude_kit_paint(_button, rde_ui_button_as_node(_button)->interactable ? fude_kit_label_color(_button) : fude_theme_active()->button_text_disabled);
}

// Destroys the button's icon and puts its label back as fude_kit_button made it; a button with no icon is untouched.
void fude_kit_icon_clear(rde_ui_button* _button) {
    rde_ui_label* _icon = fude_kit_icon_of(_button);
    if(_icon == NULL) {
        return;
    }
    rde_ui_label_destroy(_icon);
    // Put the label back as fude_kit_button made it: shown, filling the button, centred, at the text's size.
    rde_ui_label* _text = _button->internal_label;
    if(_text != NULL) {
        rde_ui_node* _t = rde_ui_label_as_node(_text);
        rde_ui_node_set_active(_t, true);
        fude_kit_pin(_t, (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 1.0f, 1.0f }, (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 0.0f, 0.0f });
        rde_ui_label_set_alignment(_text, RDE_UI_LABEL_H_ALIGN_CENTER, RDE_UI_LABEL_V_ALIGN_MIDDLE);
        rde_ui_label_set_font_scale(_text, FUDE_KIT_TEXT_SCALE);
    }
}

// Colours the icon alone (a list's row: the icon in the accent); a disabled button keeps its grey.
void fude_kit_icon_color(rde_ui_button* _button, rde_color _color) {
    rde_ui_label* _icon = fude_kit_icon_of(_button);
    if(_icon != NULL && rde_ui_button_as_node(_button)->interactable) {
        rde_ui_label_set_color(_icon, _color);
    }
}

// Puts a text field in its box: a rounded frame in the field's colour with a hairline, which holds the field. The
// field's own background is turned off (fude_kit_style_field), because the text editor's is a plain rectangle. The
// box is what is placed and shown (fude_kit_field_node); the field fills it.
void fude_kit_field_box(rde_ui_node* _parent, rde_ui_text_editor* _field) {
    rde_ui_image* _frame = rde_ui_image_create(NULL);
    rde_ui_node*  _n     = rde_ui_image_as_node(_frame);
    rde_ui_node_set_raycast_target(_n, false);
    rde_ui_node_set_user_data(_n, (any)&fude_kit_tag_field_box);   // How fude_kit_field_node knows it.
    rde_ui_node_add_child(_parent, _n);
    rde_ui_node* _f = rde_ui_text_editor_as_node(_field);
    rde_ui_node_add_child(_n, _f);
    fude_kit_pin(_f, (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 1.0f, 1.0f }, (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 0.0f, 0.0f });
}

// Returns the field's box when it has one, else the field itself.
rde_ui_node* fude_kit_field_node(rde_ui_text_editor* _field) {
    rde_ui_node* _f      = rde_ui_text_editor_as_node(_field);
    rde_ui_node* _parent = rde_ui_node_get_parent(_f);
    return _parent != NULL && _parent->user_data == (any)&fude_kit_tag_field_box ? _parent : _f;
}

// Styles a field in the theme's colours: no background of its own, its placeholder, text and caret (the accent), and
// its box with a 1-unit border.
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

// Makes the button pressable or not, and colours its label and icon to match.
void fude_kit_set_enabled(rde_ui_button* _button, b8 _enabled) {
    rde_ui_node_set_interactable(rde_ui_button_as_node(_button), _enabled);
    fude_kit_paint(_button, _enabled ? fude_kit_label_color(_button) : fude_theme_active()->button_text_disabled);
}

// Makes a button quiet again (a row's button, a tool), its label coloured by whether it can be pressed.
void fude_kit_restyle_quiet(rde_ui_button* _button) {
    fude_kit_button_quiet(_button);
    fude_kit_set_enabled(_button, rde_ui_button_as_node(_button)->interactable);
}

// Makes a button plain again, its label coloured by whether it can be pressed.
void fude_kit_restyle_button(rde_ui_button* _button) {
    fude_kit_button_plain(_button);
    fude_kit_set_enabled(_button, rde_ui_button_as_node(_button)->interactable);
}

// Returns whether the window's shorter side is under FUDE_KIT_COMPACT_BELOW, as on a phone.
b8 fude_kit_compact(rde_window* _window) {
    const rde_vec_2F _s = fude_kit_screen_size(_window);
    return rde_math_min_f32(_s.x, _s.y) < FUDE_KIT_COMPACT_BELOW;
}

// Returns the window's size; UI canvas units are window units.
rde_vec_2F fude_kit_screen_size(rde_window* _window) {
    const rde_vec_2I _w = rde_window_get_size(_window);
    return (rde_vec_2F){ (f32)_w.x, (f32)_w.y };
}

// Returns _center moved so a rect of _size stays FUDE_KIT_SCREEN_EDGE inside the SAFE AREA, clear of the status bar,
// notch and home indicator; a rect too big for it is centred in it. Done here rather than by the UI canvas, because
// the canvas's own safe area moves the root (see the note where the canvas is made).
rde_vec_2F fude_kit_clamp(rde_window* _window, rde_vec_2F _center, rde_vec_2F _size) {
    const rde_vec_2F _screen = fude_kit_screen_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);   // Left, top, right, bottom.
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

// The backdrop's dimming: black at alpha 110.
#define FUDE_KIT_BACKDROP (rde_color){ 0, 0, 0, 110 }

// Builds the backdrop (a full-screen button whose press dismisses) and the card, which blocks input to what is under
// it, both hidden.
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

// Places the backdrop over the whole screen and the card at _center.
void fude_kit_modal_place(fude_kit_modal* _modal, rde_window* _window, rde_vec_2F _center, rde_vec_2F _size) {
    const rde_vec_2F _screen = fude_kit_screen_size(_window);
    fude_kit_place(rde_ui_button_as_node(_modal->backdrop), (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.5f }, _screen);
    fude_kit_place(rde_ui_image_as_node(_modal->card), _center, _size);
}

// Shows or hides the backdrop and the card together, only when that changes.
void fude_kit_modal_show(fude_kit_modal* _modal, b8 _show) {
    if(_show != _modal->shown) {
        _modal->shown = _show;
        rde_ui_node_set_active(rde_ui_button_as_node(_modal->backdrop), _show);
        rde_ui_node_set_active(rde_ui_image_as_node(_modal->card), _show);
    }
}

// Styles the backdrop's dimming and the card as a panel with corners of _radius.
void fude_kit_modal_restyle(fude_kit_modal* _modal, f32 _radius) {
    fude_kit_button_colors(_modal->backdrop, FUDE_KIT_BACKDROP, 0.0f, (rde_color){ 0, 0, 0, 0 });
    fude_kit_style_panel(_modal->card, _radius, 1.0f);
}

// --- a field's text -----------------------------------------------------------------------------

// Copies the field's text into _out, cut between whole characters; "" when it is empty.
void fude_kit_field_text(rde_ui_text_editor* _field, c8* _out, usize _size) {
    _out[0] = 0;
    const usize _bytes = rde_ui_text_editor_get_byte_count(_field);
    c8*         _text  = _bytes > 0 ? rde_ui_text_editor_get_text(_field, 0, _bytes) : NULL;
    if(_text != NULL) {
        fude_utf8_set(_out, _size, _text);
        rde_ui_text_editor_free_text(_field, _text);
    }
}

// Copies the field's text into _out without the spaces at either end (only ' ', not tabs or line breaks).
void fude_kit_field_text_trimmed(rde_ui_text_editor* _field, c8* _out, usize _size) {
    fude_kit_field_text(_field, _out, _size);
    const c8* _s = _out;
    while(*_s == ' ') {
        _s++;
    }
    usize _n = rde_strlen(_s);
    while(_n > 0 && _s[_n - 1u] == ' ') {
        _n--;
    }
    rde_memmove(_out, _s, _n);
    _out[_n] = 0;
}

// --- a form: a card asking for something -----------------------------------------------------------------------------

// A tap off a form's card puts it away, unless it comes within FUDE_KIT_FORM_GRACE seconds of the card opening: that is
// the release of the press that opened it.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_kit_form_on_dismiss(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_kit_form* _form = (fude_kit_form*)_user_data;
    if(rde_engine_get_time_now() - _form->opened_at >= FUDE_KIT_FORM_GRACE) {
        fude_kit_form_close(_form);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// See the header.
RDE_UI_EVENT_RESULT_ fude_kit_form_on_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_kit_form_close((fude_kit_form*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// See the header. The title is the card's first child, so it is drawn under what the form adds.
rde_ui_node* fude_kit_form_build(fude_kit_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font, const c8* _title) {
    rde_memset(_form, 0, sizeof(*_form));
    _form->window = _window;
    _form->font   = _font;
    fude_kit_modal_create(&_form->modal, _root, fude_kit_form_on_dismiss, _form);
    rde_ui_node* _card = rde_ui_image_as_node(_form->modal.card);
    _form->title = fude_kit_form_label(_form, _card, _title, 17.0f);
    return _card;
}

// See the header.
b8 fude_kit_form_shown(const fude_kit_form* _form) {
    return _form->modal.card != NULL && _form->modal.shown;
}

// See the header.
void fude_kit_form_show(fude_kit_form* _form) {
    fude_kit_modal_show(&_form->modal, true);
    _form->opened_at = rde_engine_get_time_now();
}

// See the header.
void fude_kit_form_close(fude_kit_form* _form) {
    if(_form->modal.card != NULL) {
        fude_kit_modal_show(&_form->modal, false);
    }
}

// See the header.
b8 fude_kit_form_resized(const fude_kit_form* _form) {
    if(!fude_kit_form_shown(_form)) {
        return false;
    }
    const rde_vec_2F _screen = fude_kit_screen_size(_form->window);
    return _screen.x != _form->laid_out.x || _screen.y != _form->laid_out.y;
}

// See the header.
b8 fude_kit_form_restyle(fude_kit_form* _form) {
    if(_form->modal.card == NULL) {
        return false;
    }
    fude_kit_modal_restyle(&_form->modal, 16.0f);
    rde_ui_label_set_color(_form->title, fude_theme_active()->text);
    return true;
}

// See the header. The backdrop covers the screen; the title is 28 points tall.
f32 fude_kit_form_place(fude_kit_form* _form, rde_vec_2F _screen, FUDE_KIT_FORM_AT_ _at, f32 _w, f32 _h) {
    const f32 _pad = FUDE_KIT_FORM_PAD, _gap = FUDE_KIT_FORM_GAP;
    const f32 _iw  = _w - 2.0f * _pad;
    rde_vec_2F _center = { _screen.x * 0.5f, _screen.y * 0.5f };
    if(_at == FUDE_KIT_FORM_HIGH) {
        _center.y = _screen.y - 90.0f - _h * 0.5f;
    } else if(_at == FUDE_KIT_FORM_HIGH_KEPT) {
        _center.y = rde_math_max_f32(_screen.y - 90.0f - _h * 0.5f, _h * 0.5f + 8.0f);
    }
    _form->laid_out = _screen;
    fude_kit_modal_place(&_form->modal, _form->window, _center, (rde_vec_2F){ _w, _h });
    f32 _y = _h - _pad;
    fude_kit_place(rde_ui_label_as_node(_form->title), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 14.0f }, (rde_vec_2F){ _iw, 28.0f });
    _y -= 28.0f + _gap;
    return _y;
}

// See the header.
void fude_kit_form_buttons(rde_ui_button* _before, rde_ui_button* _last, f32 _w) {
    const f32 _pad = FUDE_KIT_FORM_PAD, _gap = FUDE_KIT_FORM_GAP;
    if(_before != NULL) {
        fude_kit_place(rde_ui_button_as_node(_before), (rde_vec_2F){ _w - _pad - 120.0f - _gap - 60.0f, _pad + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
    }
    fude_kit_place(rde_ui_button_as_node(_last), (rde_vec_2F){ _w - _pad - 60.0f, _pad + 22.0f }, (rde_vec_2F){ 120.0f, 44.0f });
}

// See the header.
rde_ui_label* fude_kit_form_label(const fude_kit_form* _form, rde_ui_node* _parent, const c8* _text, f32 _px) {
    rde_ui_label* _label = rde_ui_label_create(NULL);
    rde_ui_label_set_font(_label, _form->font);
    if(_text != NULL) {
        rde_ui_label_set_text(_label, _text);
    }
    rde_ui_label_set_font_scale(_label, _px / (f32)FUDE_KIT_FONT_SIZE);
    rde_ui_label_set_alignment(_label, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_MIDDLE);
    rde_ui_label_set_auto_fit(_label, true);   // a longer language shrinks it, never cuts it
    rde_ui_label_set_auto_fit_min_scale(_label, 0.6f);
    rde_ui_node_set_raycast_target(rde_ui_label_as_node(_label), false);
    rde_ui_node_add_child(_parent, rde_ui_label_as_node(_label));
    return _label;
}

// See the header. The text sits 10 points in from the box's sides and 8 from its top and bottom.
rde_ui_text_editor* fude_kit_form_field(const fude_kit_form* _form, rde_ui_node* _parent, u32 _max_chars, const c8* _placeholder, any _user_data,
                                        rde_ui_value_changed_callback _on_change, rde_ui_value_changed_callback _on_submit) {
    rde_ui_text_editor* _f = rde_ui_text_editor_create(_form->font, NULL);
    rde_ui_text_editor_set_multiline(_f, false);
    rde_ui_text_editor_set_allow_multicursor(_f, false);
    rde_ui_text_editor_set_font_size(_f, FUDE_KIT_FORM_FIELD_PX);
    rde_ui_text_editor_set_max_chars(_f, _max_chars);
    rde_ui_text_editor_set_content_insets(_f, 10.0f, 8.0f, 10.0f, 8.0f);
    if(_placeholder != NULL) {
        rde_ui_text_editor_set_placeholder(_f, _placeholder);
    }
    rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_f), _user_data);
    if(_on_change != NULL) {
        rde_ui_text_editor_set_on_change(_f, _on_change);
    }
    if(_on_submit != NULL) {
        rde_ui_text_editor_set_on_submit(_f, _on_submit);
    }
    fude_kit_field_box(_parent, _f);
    return _f;
}

// Replaces a field's whole text with _text ("" empties it).
RDE_INTERNAL void fude_kit_field_replace(rde_ui_text_editor* _field, const c8* _text) {
    const usize _bytes = rde_ui_text_editor_get_byte_count(_field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_field, 0, _bytes);
    }
    if(_text[0] != 0) {
        rde_ui_text_editor_insert_at(_field, 0, _text, rde_strlen(_text));
    }
}

// See the header.
void fude_kit_form_set(fude_kit_form* _form, rde_ui_text_editor* _field, const c8* _text) {
    _form->setting = true;
    fude_kit_field_replace(_field, _text);
    _form->setting = false;
}

// See the header. The field's text is read into 96 bytes, as much as a form's fields hold.
void fude_kit_form_bad(rde_ui_label* _problem, rde_ui_text_editor* _field, u32 _says) {
    c8 _t[96], _say[200];
    fude_kit_field_text(_field, _t, sizeof(_t));
    FUDE_TEXTF(_say, _says, FUDE_TS(_t[0] != 0 ? _t : "?"));
    rde_ui_label_set_text(_problem, _say);
    rde_ui_node_focus(rde_ui_text_editor_as_node(_field));
}

// See the header.
void fude_kit_form_named(rde_ui_label* _name, rde_ui_text_editor* _field, f32 _x, f32 _y, f32 _nw, f32 _fw, f32 _h) {
    fude_kit_place(rde_ui_label_as_node(_name), (rde_vec_2F){ _x + _nw * 0.5f, _y }, (rde_vec_2F){ _nw, _h });
    fude_kit_place(fude_kit_field_node(_field), (rde_vec_2F){ _x + _nw + 4.0f + _fw * 0.5f, _y }, (rde_vec_2F){ _fw, _h });
}

// See the header.
rde_ui_scroll_area* fude_kit_form_list(rde_ui_node* _parent) {
    rde_ui_scroll_area* _list = rde_ui_scroll_area_create(NULL);
    rde_ui_scroll_area_set_bar_thickness(_list, 3.0f);
    rde_ui_scroll_area_set_background_color(_list, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_track_color(_list, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_keyboard_inset_enabled(_list, false);
    rde_ui_node_add_child(_parent, rde_ui_scroll_area_as_node(_list));
    return _list;
}

// See the header. _bytes is held to 256, more than a form's row holds.
void fude_kit_form_drop_row(rde_ui_text_editor* const* _fields, u32 _row, u32 _rows, usize _bytes) {
    c8 _text[256];
    const usize _size = _bytes < sizeof(_text) ? _bytes : sizeof(_text);
    for(u32 _i = _row; _i + 1u < _rows; _i++) {
        fude_kit_field_text(_fields[_i + 1u], _text, _size);
        fude_kit_field_replace(_fields[_i], _text);
    }
    if(_rows > 0u) {
        fude_kit_field_replace(_fields[_rows - 1u], "");
    }
}

// See the header. A chip's name is measured as a button's label is drawn (FUDE_KIT_TEXT_SCALE of the UI font).
f32 fude_kit_form_chips(rde_ui_button* const* _chips, const c8* const* _names, u32 _n, f32 _least, f32 _pad, f32 _h, f32 _left, f32 _top, f32 _right, b8 _place) {
    const f32 _gap = FUDE_KIT_FORM_GAP;
    f32 _x = _left, _y = 0.0f;
    for(u32 _i = 0; _i < _n; _i++) {
        const f32 _w = rde_math_min_f32(_right - _left, rde_math_max_f32(_least, _pad + fude_draw_text_width(fude_kit_font(), (f32)FUDE_KIT_FONT_SIZE, _names[_i], FUDE_KIT_TEXT_SCALE * (f32)FUDE_KIT_FONT_SIZE)));
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

// See the header.
void fude_kit_form_keypad(rde_ui_button* const* _keys, const u32* _order, u32 _n, f32 _left, f32 _top, f32 _kw, f32 _kh) {
    const f32 _gap = FUDE_KIT_FORM_GAP;
    for(u32 _i = 0; _i < _n; _i++) {
        const f32 _x  = _left + (f32)(_i % 3u) * (_kw + _gap) + _kw * 0.5f;
        const f32 _ky = _top - (f32)(_i / 3u) * (_kh + _gap) - _kh * 0.5f;
        fude_kit_place(rde_ui_button_as_node(_keys[_order[_i]]), (rde_vec_2F){ _x, _ky }, (rde_vec_2F){ _kw, _kh });
    }
}

// See the header. Room is always kept for the ending 0.
void fude_kit_form_type(c8* _typed, usize _size, u32 _most, b8 _signed, b8* _fresh, c8 _key) {
    usize _n = rde_strlen(_typed);
    if(*_fresh && _key != '\b') {
        _typed[0] = 0;   // (what it opened with goes as a new number is typed)
        _n = 0;
    }
    *_fresh = false;
    if(_key == '\b') {
        if(_n > 0u) {
            _typed[_n - 1u] = 0;
        }
    } else if(_key == '-') {
        if(!_signed) {
            return;
        }
        // (the number's sign turned)
        if(_typed[0] == '-') {
            rde_memmove(_typed, _typed + 1, _n);
        } else if(_n + 1u < _size) {
            rde_memmove(_typed + 1, _typed, _n + 1u);
            _typed[0] = '-';
        }
    } else if(_key == '.') {
        if(rde_strchr(_typed, '.') == NULL && _n + 2u < _size) {
            if(_n == 0u || (_signed && _n == 1u && _typed[0] == '-')) {
                _typed[_n++] = '0';
            }
            _typed[_n++] = '.';
            _typed[_n] = 0;
        }
    } else if(_key >= '0' && _key <= '9' && _n + 1u < _size && _n < _most) {
        _typed[_n] = _key;
        _typed[_n + 1u] = 0;
    }
}
