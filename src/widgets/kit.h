#ifndef KANA_KIT
#define KANA_KIT

#include "rde.h"

// ===========================================================================
// The UI kit: what every piece of Kana's retained UI is made with — the toolbar,
// the screens' rows (row.h), the side panel, the word card. Looks from the theme
// (theme.h), buttons with icons (icons.h), text fields, placement on the UI
// canvas. Nothing here knows any screen.
//
// UI canvas units are window units (the canvas is CONSTANT_PIXEL), bottom-left
// origin, Y up; Kana's screen space is the same units, centre origin.
// ===========================================================================

// How an icon sits in its button (kana_kit_icon).
typedef enum {
    KANA_KIT_ICON_ONLY = 0,    // the icon alone (the floating bar's tools); the label, if any, hidden
    KANA_KIT_ICON_ABOVE,       // over a small label (a row's buttons)
    KANA_KIT_ICON_LEFT         // before a left-aligned label (a list's rows)
} KANA_KIT_ICON_AT_;

// The fonts are loaded at this size; everything that draws text with them
// scales from here.
#define KANA_KIT_FONT_SIZE   32
// Sizes are the fonts' (KANA_KIT_FONT_SIZE units); Slug draws a character's em
// about this many times as tall — a Japanese character is that wide.
#define KANA_KIT_EM          1.31f
#define KANA_KIT_CAPTION_PX  9.5f              // a label under an icon (~12.5 on screen)
#define KANA_KIT_TEXT_SCALE  (13.0f / 32.0f)   // a button's label: 13 units of text from the 32 px font (~17 on screen)
#define KANA_KIT_SCREEN_EDGE 8.0f              // what floats (the bar, a row, a card) stays this far inside the safe area

// The fonts the kit draws with: the UI font (a button's label; a Japanese
// character as an icon), Phosphor Regular and Fill (icons.h; NULL: the UI font).
void           kana_kit_set_fonts(rde_font* _text, rde_font* _icons, rde_font* _icons_fill);
rde_font*      kana_kit_font(void);

rde_ui_style   kana_kit_style(rde_color _tint, f32 _radius);
rde_color      kana_kit_shade(rde_color _c, i32 _delta);
// Normal / hovered / pressed / disabled from one base colour, with an optional
// border; the label (and icon) coloured to go on it.
void           kana_kit_button_colors(rde_ui_button* _button, rde_color _base, f32 _border_width, rde_color _border);
// The looks, from the theme:
//   plain     a secondary button: the surface's second shade, the text colour
//   quiet     no background until pressed (tools, a row's buttons)
//   selected  chosen, a toggle on: the accent's tint, the accent, a Fill icon
//   primary   the way on: the accent, white
//   danger    destructive: red, white (danger_quiet: red on nothing)
void           kana_kit_button_plain(rde_ui_button* _button);
void           kana_kit_button_quiet(rde_ui_button* _button);
void           kana_kit_button_selected(rde_ui_button* _button);
void           kana_kit_button_primary(rde_ui_button* _button);
void           kana_kit_button_danger(rde_ui_button* _button);
void           kana_kit_button_danger_quiet(rde_ui_button* _button);
// A pill: the accent when on, plain when not.
void           kana_kit_button_chip(rde_ui_button* _button, b8 _on);
// Every state's corner radius (the looks give 10).
void           kana_kit_button_round(rde_ui_button* _button, f32 _radius);
// Near-black on a light background, near-white on a dark one.
rde_color      kana_kit_text_on(rde_color _background);
// A panel in the theme's colours: the surface and a hairline border.
void           kana_kit_style_panel(rde_ui_image* _panel, f32 _radius, f32 _border_width);
// Pins _node bottom-left to its parent with a centre pivot: _center is its centre
// in parent-local units (bottom-left origin).
void           kana_kit_place(rde_ui_node* _node, rde_vec_2F _center, rde_vec_2F _size);
// Pins _node to its parent's edges: its rect is the parent's anchor box
// (_anchor_min.._anchor_max, fractions) moved in by _inset_min / _inset_max.
void           kana_kit_pin(rde_ui_node* _node, rde_vec_2F _anchor_min, rde_vec_2F _anchor_max, rde_vec_2F _inset_min, rde_vec_2F _inset_max);
// A text button, plain, under _parent; _on_click gets _user_data.
rde_ui_button* kana_kit_button(rde_ui_node* _parent, const c8* _text, rde_ui_event_callback _on_click, any _user_data);
// An icon for _button (icons.h, or a Japanese character), _px tall, set out as
// _at says. Again on the same button changes it. A chosen (selected) button's
// Phosphor icon turns Fill.
void           kana_kit_icon(rde_ui_button* _button, const c8* _glyph, KANA_KIT_ICON_AT_ _at, f32 _px);
// A Phosphor icon's left bearing, in ems (0 for anything else). A centred label
// lands an icon that far right of centre: an icon label placed by hand moves
// back by it times the icon's em (kana_kit_icon does this itself).
f32            kana_kit_icon_bearing(const c8* _glyph);
// The icon alone in _color (after a look, which colours label and icon alike).
void           kana_kit_icon_color(rde_ui_button* _button, rde_color _color);
// A text field in a rounded box under _parent (instead of adding the field
// itself); the node to place and show is the box: kana_kit_field_node. Its
// colours (box, text, caret) from the theme: kana_kit_style_field.
void           kana_kit_field_box(rde_ui_node* _parent, rde_ui_text_editor* _field);
rde_ui_node*   kana_kit_field_node(rde_ui_text_editor* _field);
void           kana_kit_style_field(rde_ui_text_editor* _field);
void           kana_kit_set_enabled(rde_ui_button* _button, b8 _enabled);
// Plain (or quiet) again, its label coloured by whether it can be pressed.
void           kana_kit_restyle_button(rde_ui_button* _button);
void           kana_kit_restyle_quiet(rde_ui_button* _button);
// A card over a dimmed backdrop: the backdrop covers the screen, and a tap on it
// (outside the card) calls _on_dismiss with _user_data. Shown and hidden together;
// what goes on the card is added to `card`.
typedef struct {
    rde_ui_button* backdrop;
    rde_ui_image*  card;
    b8             shown;
} kana_kit_modal;
// Built under _root, hidden.
void           kana_kit_modal_create(kana_kit_modal* _modal, rde_ui_node* _root, rde_ui_event_callback _on_dismiss, any _user_data);
// The backdrop over the whole screen; the card of _size centred at _center (UI units).
void           kana_kit_modal_place(kana_kit_modal* _modal, rde_window* _window, rde_vec_2F _center, rde_vec_2F _size);
// Shown or hidden (only touches the UI on a change).
void           kana_kit_modal_show(kana_kit_modal* _modal, b8 _show);
// The backdrop's dimming and the card's panel (its corners _radius), from the theme.
void           kana_kit_modal_restyle(kana_kit_modal* _modal, f32 _radius);
// The window in UI canvas units (bottom-left origin).
rde_vec_2F     kana_kit_screen_size(rde_window* _window);
// A rect of _size centred at _center kept inside the safe area — clear of the
// status bar, notch and home indicator.
rde_vec_2F     kana_kit_clamp(rde_window* _window, rde_vec_2F _center, rde_vec_2F _size);

#endif
