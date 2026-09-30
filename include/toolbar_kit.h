#ifndef KANA_TOOLBAR_KIT
#define KANA_TOOLBAR_KIT

#include "rde.h"

// ===========================================================================
// The toolbar's UI kit, for the other chrome built on the same canvas and font
// (side.c): styles from the theme, buttons, placement. Defined in toolbar.c.
// ===========================================================================

struct kana_toolbar;

// How an icon sits in its button (kana_toolbar_icon).
typedef enum {
    KANA_TOOLBAR_ICON_ONLY = 0,    // the icon alone (the floating bar's tools); the label, if any, hidden
    KANA_TOOLBAR_ICON_ABOVE,       // over a small label (a screen's row of buttons)
    KANA_TOOLBAR_ICON_LEFT         // before a left-aligned label (a list's rows)
} KANA_TOOLBAR_ICON_AT_;

// Sizes are the fonts' (KANA_TOOLBAR_FONT_SIZE units); Slug draws a character's
// em about this many times as tall — a Japanese character is that wide.
#define KANA_TOOLBAR_EM         1.31f
#define KANA_TOOLBAR_CAPTION_PX 9.5f    // a label under an icon (~12.5 on screen)

rde_ui_style   kana_toolbar_style(rde_color _tint, f32 _radius);
rde_color      kana_toolbar_shade(rde_color _c, i32 _delta);
// Normal / hovered / pressed / disabled from one base colour, with an optional
// border; the label (and icon) coloured to go on it.
void           kana_toolbar_button_colors(rde_ui_button* _button, rde_color _base, f32 _border_width, rde_color _border);
// The looks, from the theme:
//   plain     a secondary button: the surface's second shade, the text colour
//   quiet     no background until pressed (tools, a row's buttons)
//   selected  chosen, a toggle on: the accent's tint, the accent, a Fill icon
//   primary   the way on: the accent, white
//   danger    destructive: red, white (danger_quiet: red on nothing)
void           kana_toolbar_button_plain(rde_ui_button* _button);
void           kana_toolbar_button_quiet(rde_ui_button* _button);
void           kana_toolbar_button_selected(rde_ui_button* _button);
void           kana_toolbar_button_primary(rde_ui_button* _button);
void           kana_toolbar_button_danger(rde_ui_button* _button);
void           kana_toolbar_button_danger_quiet(rde_ui_button* _button);
// A pill: the accent when on, plain when not.
void           kana_toolbar_button_chip(rde_ui_button* _button, b8 _on);
// Every state's corner radius (the looks give 10).
void           kana_toolbar_button_round(rde_ui_button* _button, f32 _radius);
// Near-black on a light background, near-white on a dark one.
rde_color      kana_toolbar_text_on(rde_color _background);
// A panel in the theme's colours: the surface and a hairline border.
void           kana_toolbar_style_panel(rde_ui_image* _panel, f32 _radius, f32 _border_width);
// Pins _node bottom-left to its parent with a centre pivot: _center is its centre
// in parent-local units (bottom-left origin).
void           kana_toolbar_place(rde_ui_node* _node, rde_vec_2F _center, rde_vec_2F _size);
// A text button, plain, under _parent; _on_click gets the toolbar.
rde_ui_button* kana_toolbar_button(struct kana_toolbar* _toolbar, rde_ui_node* _parent, const c8* _text, rde_ui_event_callback _on_click);
// An icon for _button (icons.h, or a Japanese character), _px tall, set out as
// _at says. Again on the same button changes it. A chosen (selected) button's
// Phosphor icon turns Fill.
void           kana_toolbar_icon(rde_ui_button* _button, const c8* _glyph, KANA_TOOLBAR_ICON_AT_ _at, f32 _px);
// A Phosphor icon's left bearing, in ems (0 for anything else). A label lands
// an icon half of it right of centre: an icon label placed by hand moves back
// by half of it times the icon's em (kana_toolbar_icon does this itself).
f32            kana_toolbar_icon_bearing(const c8* _glyph);
// The icon alone in _color (after a look, which colours label and icon alike).
void           kana_toolbar_icon_color(rde_ui_button* _button, rde_color _color);
// A text field in a rounded box under _parent (instead of adding the field
// itself); the node to place and show is the box: kana_toolbar_field_node. Its
// colours (box, text, caret) from the theme: kana_toolbar_style_field.
void           kana_toolbar_field_box(rde_ui_node* _parent, rde_ui_text_editor* _field);
rde_ui_node*   kana_toolbar_field_node(rde_ui_text_editor* _field);
void           kana_toolbar_style_field(rde_ui_text_editor* _field);
void           kana_toolbar_set_enabled(rde_ui_button* _button, b8 _enabled);
// Plain (or quiet) again, its label coloured by whether it can be pressed.
void           kana_toolbar_restyle_button(rde_ui_button* _button);
void           kana_toolbar_restyle_quiet(rde_ui_button* _button);
// The window in UI canvas units (bottom-left origin).
rde_vec_2F     kana_toolbar_virtual_size(const struct kana_toolbar* _toolbar);
// A rect of _size centred at _center kept inside the safe area.
rde_vec_2F     kana_toolbar_clamp(const struct kana_toolbar* _toolbar, rde_vec_2F _center, rde_vec_2F _size);
void           kana_toolbar_set_palette_open(struct kana_toolbar* _toolbar, b8 _open);

#endif
