// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_KIT
#define FUDE_KIT

#include "rde.h"

// ===========================================================================
// UI kit: the helpers every piece of the app's retained UI is made with (the toolbar, the screens' rows, the side
// panel, the word card): looks from the theme (theme.h), buttons with icons (icons.h), text fields, cards over a
// dimmed backdrop, forms (a card asking for something: the canvas's cards are built on them), and placement on the UI
// canvas. Nothing here knows any screen.
//
// Where it sits: drawing/widgets, above drawing/base. Everything that builds UI uses it: the other widgets (row.h,
// filterbar.h), the app shell (drawing/app) and the study apps' widgets.
//
// Its main types and their lifecycle: the widgets made here are RDE UI nodes added under the parent passed in, so
// they belong to that UI tree and go with it. A fude_kit_modal is held by its caller; its two nodes belong to the
// root it was built under. A fude_kit_form too: it is part of its card's struct, whose owner clears it when the UI
// goes (the UI frees the nodes).
//
// Threads: main thread only, like all of RDE's UI.
//
// Units: UI canvas units are window units (the canvas is CONSTANT_PIXEL), with the origin at the bottom-left and Y
// up. The app's screen space uses the same units with the origin at the centre.
//
// Design: a Phosphor icon is moved back by its left bearing, because the text engine centres a line by its ink, which
// lands the icon its whole bearing right of centre; kit.c keeps a table of the bearings (FUDE_KIT_ICON_BEARINGS). On
// a touch screen buttons have no hover look, because a lifted finger leaves the pointer on what it tapped and a hover
// tint there would look like the button was left on. A form is the cards' common part, written once: its card, title,
// Cancel, the tap off it (ignored within FUDE_KIT_FORM_GRACE of opening), its layout's frame and its fields' handling;
// each card keeps its own widgets, its own rows' layout and its presses.
// ===========================================================================

// How an icon sits in its button (fude_kit_icon).
typedef enum {
    FUDE_KIT_ICON_ONLY = 0,    // The icon alone (the floating bar's tools); the label, if any, is hidden.
    FUDE_KIT_ICON_ABOVE,       // Over a small label (a row's buttons).
    FUDE_KIT_ICON_LEFT         // Before a left-aligned label (a list's rows).
} FUDE_KIT_ICON_AT_;

// The fonts are loaded at this size; everything that draws text with them scales from here.
#define FUDE_KIT_FONT_SIZE   32
// Sizes are in the fonts' units (FUDE_KIT_FONT_SIZE). Slug draws a character's em about this many times as tall, and a
// Japanese character is that wide.
#define FUDE_KIT_EM          1.31f
#define FUDE_KIT_CAPTION_PX  9.5f              // A label under an icon (about 12.5 on screen).
// A button's label: 13 units of text from the 32 px font (about 17 on screen).
#define FUDE_KIT_TEXT_SCALE  (13.0f / 32.0f)
// What floats (the bar, a row, a card) stays this far inside the safe area.
#define FUDE_KIT_SCREEN_EDGE 8.0f

// Sets the fonts the kit draws with: the UI font (for a button's label, and for a Japanese character used as an
// icon), and Phosphor Regular and Fill (icons.h). With NULL for _icons the UI font is used for icons. Call it once
// the fonts are loaded, before building any UI. fude_kit_font returns the UI font (NULL before it is set).
void           fude_kit_set_fonts(rde_font* _text, rde_font* _icons, rde_font* _icons_fill);
rde_font*      fude_kit_font(void);

// RDE's default style with the given tint and corner radius.
rde_ui_style   fude_kit_style(rde_color _tint, f32 _radius);
// Sets the normal, hovered, pressed and disabled looks from one base colour, with an optional border, and colours the
// label and icon to go on it. A clear base (a quiet button) shows the plain button's colour while pressed.
void           fude_kit_button_colors(rde_ui_button* _button, rde_color _base, f32 _border_width, rde_color _border);
// The looks, from the theme:
//   plain     a secondary button: the surface's second shade, with the text colour.
//   quiet     no background until pressed (tools, a row's buttons).
//   selected  chosen, or a toggle that is on: the accent's tint, the accent, and a Fill icon.
//   primary   the way forward: the accent, with white.
//   danger    destructive: red, with white. danger_quiet is red on no background.
void           fude_kit_button_plain(rde_ui_button* _button);
void           fude_kit_button_quiet(rde_ui_button* _button);
void           fude_kit_button_selected(rde_ui_button* _button);
void           fude_kit_button_primary(rde_ui_button* _button);
void           fude_kit_button_danger(rde_ui_button* _button);
void           fude_kit_button_danger_quiet(rde_ui_button* _button);
// Styles a chip (Browse's filters and sorts) as a pill: the accent when _on, plain when not.
void           fude_kit_button_chip(rde_ui_button* _button, b8 _on);
// Sets every state's corner radius (the looks above use 10).
void           fude_kit_button_round(rde_ui_button* _button, f32 _radius);
// A text colour that reads on _background: near-black on a light one, near-white on a dark one.
rde_color      fude_kit_text_on(rde_color _background);
// Styles a panel (the bar, the palette, a menu, a card) in the theme's colours: the surface and a hairline border.
void           fude_kit_style_panel(rde_ui_image* _panel, f32 _radius, f32 _border_width);
// Pins _node to its parent's bottom-left with a centre pivot, so _center is its centre in the parent's own units
// (origin at the bottom-left).
void           fude_kit_place(rde_ui_node* _node, rde_vec_2F _center, rde_vec_2F _size);
// The same for a point on the screen (where the bar was dragged, where a tap opened a menu): it is not mirrored in a
// right-to-left UI, though what the node holds still is.
void           fude_kit_place_at(rde_ui_node* _node, rde_vec_2F _center, rde_vec_2F _size);
// Pins _node to its parent's edges: its rect is the parent's anchor box (_anchor_min to _anchor_max, as fractions)
// moved in by _inset_min and _inset_max.
void           fude_kit_pin(rde_ui_node* _node, rde_vec_2F _anchor_min, rde_vec_2F _anchor_max, rde_vec_2F _inset_min, rde_vec_2F _inset_max);
// Makes a plain text button under _parent, which owns it; a press calls _on_click with _user_data. A long label
// shrinks to fit rather than being cut.
rde_ui_button* fude_kit_button(rde_ui_node* _parent, const c8* _text, rde_ui_event_callback _on_click, any _user_data);
// Gives _button an icon (from icons.h, or a Japanese character drawn in the UI font), _px tall, set out as _at says.
// Calling it again on the same button changes the icon. A chosen (selected) button's Phosphor icon turns to Fill.
void           fude_kit_icon(rde_ui_button* _button, const c8* _glyph, FUDE_KIT_ICON_AT_ _at, f32 _px);
// Takes _button's icon away, and puts its label back as fude_kit_button made it (filling the button, centred). For a
// button used again with no icon after it had one (a choice card's choices). A button with no icon is left as it is.
void           fude_kit_icon_clear(rde_ui_button* _button);
// A Phosphor icon's left bearing in ems (0 for anything else). A centred label puts an icon that far right of
// centre, so an icon label placed by hand must move back by this times the icon's em (fude_kit_icon does it itself).
f32            fude_kit_icon_bearing(const c8* _glyph);
// How far left to lay out a Phosphor icon _px tall so its ink is centred: its bearing. Right to left it is the other
// way round, because the placement there is mirrored (rde.h: RDE_UI_DIRECTION_) and that brings it back to the left.
f32            fude_kit_icon_back(const c8* _glyph, f32 _px);
// Colours the icon alone (a list's row: the icon in the accent). Call it after a look, which colours label and icon
// alike. A disabled button is left as it is.
void           fude_kit_icon_color(rde_ui_button* _button, rde_color _color);
// Puts a text field in a rounded box under _parent, instead of adding the field itself, because the field's own
// background is a plain rectangle. The node to place and show is the box: fude_kit_field_node returns it (or the
// field, when it has no box). fude_kit_style_field takes the colours (box, text, caret) from the theme.
void           fude_kit_field_box(rde_ui_node* _parent, rde_ui_text_editor* _field);
rde_ui_node*   fude_kit_field_node(rde_ui_text_editor* _field);
void           fude_kit_style_field(rde_ui_text_editor* _field);
// Makes _button pressable or not, greying its label and icon when not.
void           fude_kit_set_enabled(rde_ui_button* _button, b8 _enabled);
// Makes a button plain (or quiet) again, after a theme change, with its label coloured by whether it can be pressed.
void           fude_kit_restyle_button(rde_ui_button* _button);
void           fude_kit_restyle_quiet(rde_ui_button* _button);
// A card over a dimmed backdrop. The backdrop covers the screen, and a tap on it (outside the card) calls the
// _on_dismiss given to fude_kit_modal_create with its _user_data. Both are shown and hidden together; what goes on
// the card is added to `card`.
typedef struct {
    rde_ui_button* backdrop;
    rde_ui_image*  card;
    b8             shown;
} fude_kit_modal;
// Builds the backdrop and the card under _root, hidden.
void           fude_kit_modal_create(fude_kit_modal* _modal, rde_ui_node* _root, rde_ui_event_callback _on_dismiss, any _user_data);
// Places the backdrop over the whole screen, and the card of _size centred at _center (UI units).
void           fude_kit_modal_place(fude_kit_modal* _modal, rde_window* _window, rde_vec_2F _center, rde_vec_2F _size);
// Shows or hides the card and its backdrop; the UI is only touched when this changes something.
void           fude_kit_modal_show(fude_kit_modal* _modal, b8 _show);
// Styles the backdrop's dimming and the card's panel (corners of _radius) from the theme.
void           fude_kit_modal_restyle(fude_kit_modal* _modal, f32 _radius);
// --- A form: a card asking for something (the canvas's cards, zoom's *form.c) ---------------------------------------
// A card over a dimmed backdrop (a fude_kit_modal) with a title along its top. It is built once, hidden, and shown
// again for each question; it is laid out again when the screen's size changes; Cancel, or a tap off the card, puts it
// away, but a tap off it within FUDE_KIT_FORM_GRACE seconds of its opening is ignored: that is the release of the press
// that opened it. A card keeps a fude_kit_form in its own struct, adds what it asks for under the node
// fude_kit_form_build returns, and keeps only what is its own: its widgets, its layout between the title and its
// buttons, its presses.
#define FUDE_KIT_FORM_PAD      16.0f   // A form's margin round what is on it, in points.
#define FUDE_KIT_FORM_GAP      8.0f    // Between its rows, and between the things in a row.
#define FUDE_KIT_FORM_GRACE    0.6     // Seconds after opening in which a tap off the card is not taken.
#define FUDE_KIT_FORM_FIELD_PX 18u     // A form's text fields' letters, in pixels.

// Where a form's card sits on the screen.
typedef enum {
    FUDE_KIT_FORM_MIDDLE = 0,   // In the middle of the screen.
    FUDE_KIT_FORM_HIGH,         // Its top 90 points under the screen's, so the keyboard comes up under it.
    FUDE_KIT_FORM_HIGH_KEPT     // The same, but never lower than 8 points over the screen's bottom.
} FUDE_KIT_FORM_AT_;

// A form's card, its title and what every form keeps. Its fields are kit.c's, but setting, which a form's change
// callbacks read.
typedef struct {
    fude_kit_modal modal;
    rde_window*    window;
    rde_font*      font;
    rde_ui_label*  title;
    f64            opened_at;   // When it was opened (engine seconds), so a tap off the card right after is ignored.
    rde_vec_2F     laid_out;    // The screen size it was last laid out for.
    b8             setting;     // A field is being filled in by the form (fude_kit_form_set), not typed.
} fude_kit_form;

// Clears _form and builds its card under _root, hidden, with its title (17 points, left-aligned, shrinking to 0.6 of
// that to fit) saying _title (NULL: nothing yet), in _font. Returns the card's node, for what goes on it; it belongs to
// _root's UI. Every other call does nothing while modal.card is NULL (before this, and after the owner clears the form
// as the UI goes).
rde_ui_node*        fude_kit_form_build(fude_kit_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font, const c8* _title);
// Whether the card is up (false before it is built).
b8                  fude_kit_form_shown(const fude_kit_form* _form);
// Shows the card and notes when, for the tap off it. The caller lays it out and styles it after.
void                fude_kit_form_show(fude_kit_form* _form);
// Puts the card away; nothing is handed back.
void                fude_kit_form_close(fude_kit_form* _form);
// Whether the card is up and the screen's size has changed since it was laid out: the caller lays it out again (once
// a frame).
b8                  fude_kit_form_resized(const fude_kit_form* _form);
// Gives the backdrop, the card (16-point corners) and the title the active theme's colours. Returns false, doing
// nothing, when the card is not built: the caller then styles nothing of its own either.
b8                  fude_kit_form_restyle(fude_kit_form* _form);
// Places the card, _w by _h points, where _at says on a screen _screen in size (fude_kit_screen_size's), its title
// across its top inside its margins, and remembers _screen for fude_kit_form_resized. Returns the height (card-local,
// Y up from its bottom) under the title and a gap, where the card's next row goes.
f32                 fude_kit_form_place(fude_kit_form* _form, rde_vec_2F _screen, FUDE_KIT_FORM_AT_ _at, f32 _w, f32 _h);
// Places a card's buttons along its bottom right, 120 by 44 points each: _last at the corner and _before (NULL: none)
// just left of it (Cancel, then OK). _w is the card's width.
void                fude_kit_form_buttons(rde_ui_button* _before, rde_ui_button* _last, f32 _w);
// A Cancel button's handler (its user data the fude_kit_form): puts the card away, calling nothing.
RDE_UI_EVENT_RESULT_ fude_kit_form_on_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// Makes a label under _parent in the form's font saying _text (NULL: nothing yet), _px points tall, left-aligned and
// middled, shrinking to 0.6 of that to fit (never cut); taps go through it.
rde_ui_label*       fude_kit_form_label(const fude_kit_form* _form, rde_ui_node* _parent, const c8* _text, f32 _px);
// Makes a one-line text field of at most _max_chars characters, FUDE_KIT_FORM_FIELD_PX letters, in a rounded box under
// _parent (fude_kit_field_box), its placeholder _placeholder (NULL: none), its node's user data _user_data, and its
// changes and Enter calling _on_change and _on_submit (NULL: none).
rde_ui_text_editor* fude_kit_form_field(const fude_kit_form* _form, rde_ui_node* _parent, u32 _max_chars, const c8* _placeholder, any _user_data,
                                        rde_ui_value_changed_callback _on_change, rde_ui_value_changed_callback _on_submit);
// Writes _text into _field in place of what it has ("" empties it), with the form's setting raised meanwhile, so the
// field's change callback can tell the form's own writing from typing.
void                fude_kit_form_set(fude_kit_form* _form, rde_ui_text_editor* _field, const c8* _text);
// Says on _problem what of _field could not be read, with text id _says (a FUDE_TEXT_ with one string: the field's
// text, "?" when it is empty), and gives _field the keyboard.
void                fude_kit_form_bad(rde_ui_label* _problem, rde_ui_text_editor* _field, u32 _says);
// Places a field with its name before it along a row _h tall whose middle is at height _y (card-local): the name _nw
// points wide from _x, the field _fw wide 4 points after it.
void                fude_kit_form_named(rde_ui_label* _name, rde_ui_text_editor* _field, f32 _x, f32 _y, f32 _nw, f32 _fw, f32 _h);
// Makes a scrolling list for a form's rows under _parent: no background, a thin bar, and no room kept for the keyboard
// (the card is above it already: that room pushed the rows out of sight while it was up).
rde_ui_scroll_area* fude_kit_form_list(rde_ui_node* _parent);
// Drops row _row of a list of _rows rows of fields (_fields, one a row): the texts under it move up a row (each cut to
// _bytes bytes, at a whole character) and the last row is emptied. The caller counts the rows.
void                fude_kit_form_drop_row(rde_ui_text_editor* const* _fields, u32 _row, u32 _rows, usize _bytes);
// Places chips from _left along the card's line at _top (card-local, Y up), each as wide as its name (_names) and
// _pad more, _least at least and never wider than the line, _h tall, starting a new line when the next does not fit
// before _right. Returns how tall they are; with _place false they are only measured.
f32                 fude_kit_form_chips(rde_ui_button* const* _chips, const c8* const* _names, u32 _n, f32 _least, f32 _pad, f32 _h, f32 _left, f32 _top,
                                        f32 _right, b8 _place);
// Places _n keys of a keypad three to a row, _kw by _kh points each, from _left along the line at _top down: _keys
// taken in _order (the keys' indices, row by row: 7 8 9, 4 5 6...).
void                fude_kit_form_keypad(rde_ui_button* const* _keys, const u32* _order, u32 _n, f32 _left, f32 _top, f32 _kw, f32 _kh);
// Types key _key on a keypad into _typed (_size bytes): a digit '0' to '9' (while it has fewer than _most characters),
// '.' (once, after a 0 when nothing, or with _signed only a minus, is before it), '-' (with _signed: the sign turned)
// or '\b' (back: the last character off). While *_fresh, the first key but back replaces what was there;
// *_fresh is then cleared.
void                fude_kit_form_type(c8* _typed, usize _size, u32 _most, b8 _signed, b8* _fresh, c8 _key);

// The window's size in UI canvas units.
rde_vec_2F     fude_kit_screen_size(rde_window* _window);
// A screen whose shorter side is under this many window units (points, dp) is a phone's (fude_kit_compact).
#define FUDE_KIT_COMPACT_BELOW 600.0f
// Whether the window is a phone's screen: its shorter side is under FUDE_KIT_COMPACT_BELOW window units (points, dp),
// in either orientation. Tablets (an iPad mini's 744) are not, but a tablet's narrow split view is. What is laid out
// for a compact screen fits a phone: rows with a More button, chips on lines that scroll, a field on a line of its
// own...
b8             fude_kit_compact(rde_window* _window);
// Moves a rect of _size centred at _center inside the safe area, clear of the status bar, notch and home indicator,
// and returns its new centre. It is done here rather than by the UI canvas, because the canvas's own safe area would
// move the root.
rde_vec_2F     fude_kit_clamp(rde_window* _window, rde_vec_2F _center, rde_vec_2F _size);


// Copies a text field's text into _out (_size bytes, cut at a whole character). fude_kit_field_text_trimmed also
// drops the spaces around it. The field is not const: its text comes out as a copy that only the field can free.
void fude_kit_field_text(rde_ui_text_editor* _field, c8* _out, usize _size);
void fude_kit_field_text_trimmed(rde_ui_text_editor* _field, c8* _out, usize _size);

#endif
