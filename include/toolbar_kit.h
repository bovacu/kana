#ifndef KANA_TOOLBAR_KIT
#define KANA_TOOLBAR_KIT

#include "rde.h"

// ===========================================================================
// The toolbar's UI kit, for the other chrome built on the same canvas and font
// (side.c): styles from the theme, buttons, placement. Defined in toolbar.c.
// ===========================================================================

struct kana_toolbar;

rde_ui_style   kana_toolbar_style(rde_color _tint, f32 _radius);
rde_color      kana_toolbar_shade(rde_color _c, i32 _delta);
// Normal / hovered / pressed / disabled from one base colour, with an optional border.
void           kana_toolbar_button_colors(rde_ui_button* _button, rde_color _base, f32 _border_width, rde_color _border);
void           kana_toolbar_button_plain(rde_ui_button* _button);
void           kana_toolbar_button_selected(rde_ui_button* _button);
// Dark text on a light background, the theme's button text on a dark one.
rde_color      kana_toolbar_text_on(rde_color _background);
// A panel in the theme's colours.
void           kana_toolbar_style_panel(rde_ui_image* _panel, f32 _radius, f32 _border_width);
// Pins _node bottom-left to its parent with a centre pivot: _center is its centre
// in parent-local units (bottom-left origin).
void           kana_toolbar_place(rde_ui_node* _node, rde_vec_2F _center, rde_vec_2F _size);
// A text button in the toolbar's style under _parent; _on_click gets the toolbar.
rde_ui_button* kana_toolbar_button(struct kana_toolbar* _toolbar, rde_ui_node* _parent, const c8* _text, rde_ui_event_callback _on_click);
void           kana_toolbar_set_enabled(rde_ui_button* _button, b8 _enabled);
// Plain again, its label coloured by whether it can be pressed.
void           kana_toolbar_restyle_button(rde_ui_button* _button);
// The window in UI canvas units (bottom-left origin).
rde_vec_2F     kana_toolbar_virtual_size(const struct kana_toolbar* _toolbar);
// A rect of _size centred at _center kept inside the safe area.
rde_vec_2F     kana_toolbar_clamp(const struct kana_toolbar* _toolbar, rde_vec_2F _center, rde_vec_2F _size);
void           kana_toolbar_set_palette_open(struct kana_toolbar* _toolbar, b8 _open);

#endif
