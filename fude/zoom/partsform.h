// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PARTSFORM_H
#define FUDE_ZOOM_PARTSFORM_H

#include "rde.h"
#include "drawing/widgets/kit.h"
#include "zoom/units.h"

// ===========================================================================
// Partsform: the card that asks for the parts to cut from a board (the selection's Fit parts). It sits high on the
// screen, over where the keyboard comes up:
//
//   its title (the board's name)
//   the units the sizes are in: mm, cm, in, ft · in (the page's units to begin with)
//   a list that scrolls, one row for each part: its width × its height, and a trash button
//   Add part, under the list
//   the saw's kerf as it is now (what is lost between the parts)
//   Cancel, Cut
//
// Where it sits: zoom, the canvas apps' layer, over drawing/widgets/kit.h and units.h. The page (page.h) owns one and
// fits the parts onto the board when Cut hands them back.
// Its card is drawing/widgets/kit.h's form (fude_kit_form): the card, its title, Cancel, a tap off it and laying it
// out again are the kit's; the rest is this card's own.
// Its main types and their lifecycle: fude_zoom_parts_form is the page's struct. fude_zoom_parts_build makes its
// widgets under the UI's root, which owns and frees them; fude_zoom_parts_forget clears the struct when the canvas's
// UI goes. The buttons' and fields' user data (fude_zoom_parts_ref) live in the form itself (refs, unit_refs,
// field_refs), so each form's widgets point to their own form and it must not move while they exist.
// Threads: main thread only; it is UI.
// Design: a size is read as the numpad reads one (units.h: "600", "60 cm", "23 5/8", "1' 11 5/8\"", with a comma or
//   a point for decimals). A row left empty is skipped; a row that cannot be read keeps the card up and says which.
//   It brings the keyboard up at once, focused on the first width, because every part has to be typed.
// ===========================================================================

// The most parts the card can list.
#define FUDE_ZOOM_PARTS_ROWS 40u

// What Cut hands back: each part's two sizes, in millimetres, as typed (across, then up). The callback gets _n parts
// in _parts, valid only during the call, and _self, what the opener handed fude_zoom_parts_open; the card is put away
// first.
typedef struct {
    f64 width, height;
} fude_zoom_parts_size;
typedef void (*fude_zoom_parts_done)(any _self, const fude_zoom_parts_size* _parts, u32 _n);

struct fude_zoom_parts_form;
// A button's or a field's user data: the card it is on, and its index (a field's is its row × 2, plus 1 for a height).
typedef struct {
    struct fude_zoom_parts_form* form;
    u32                          index;
} fude_zoom_parts_ref;

// The parts card: its widgets and its rows.
typedef struct fude_zoom_parts_form {
    fude_kit_form        card;   // The card, its title, when it opened and the screen it was laid out for (kit.h).
    rde_ui_label*        units_label;
    rde_ui_button*       units[4];
    u8                   unit_choice;       // 0 mm, 1 cm, 2 in, 3 ft · in (bare numbers in inches); the page's choices
    rde_ui_scroll_area*  list;
    rde_ui_text_editor*  widths[FUDE_ZOOM_PARTS_ROWS];
    rde_ui_text_editor*  heights[FUDE_ZOOM_PARTS_ROWS];
    rde_ui_label*        times[FUDE_ZOOM_PARTS_ROWS];
    rde_ui_button*       drops[FUDE_ZOOM_PARTS_ROWS];
    // The trash buttons' user data, one for each row; the unit buttons', one each; and the fields', two a row (width,
    // then height). They are the form's own, so a second form would not take the first one's.
    fude_zoom_parts_ref  refs[FUDE_ZOOM_PARTS_ROWS];
    fude_zoom_parts_ref  unit_refs[4];
    fude_zoom_parts_ref  field_refs[2u * FUDE_ZOOM_PARTS_ROWS];
    // How many rows are shown.
    u32                  rows;
    rde_ui_button*       add;
    rde_ui_label*        kerf;
    rde_ui_label*        problem;           // the row that could not be read (empty when all is well)
    rde_ui_button*       cancel;
    rde_ui_button*       save;
    fude_zoom_parts_done done;
    any                  self;
} fude_zoom_parts_form;

// fude_zoom_parts_build makes the card's widgets under _root, hidden, with _font for its words.
// fude_zoom_parts_forget clears the struct when the canvas's UI goes (the UI frees the widgets); every other call then
// does nothing.
void fude_zoom_parts_build(fude_zoom_parts_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font);
void fude_zoom_parts_forget(fude_zoom_parts_form* _form);
// Opens the card with one empty row, _title over it and _kerf under the list ("Saw kerf: 3 mm"), the sizes in
// _unit_choice to begin with (out of range: mm), and the first width focused so the keyboard comes up. Cut calls
// _done with _self.
void fude_zoom_parts_open(fude_zoom_parts_form* _form, const c8* _title, const c8* _kerf, u8 _unit_choice, fude_zoom_parts_done _done, any _self);
// fude_zoom_parts_close puts the card away without calling back; fude_zoom_parts_shown returns whether it is up.
void fude_zoom_parts_close(fude_zoom_parts_form* _form);
b8   fude_zoom_parts_shown(const fude_zoom_parts_form* _form);
// fude_zoom_parts_update is called once a frame and lays the card out again when the screen's size changes.
// fude_zoom_parts_restyle gives its widgets the active theme's colours.
void fude_zoom_parts_update(fude_zoom_parts_form* _form);
void fude_zoom_parts_restyle(fude_zoom_parts_form* _form);

// For the look harness (drawing/app/look.h, a developer's launch flags): fills the rows from _rows ("600 x 300;
// 200 x 100", a row for each part), then with _cut presses Cut. It does nothing while the card is not up.
void fude_zoom_parts_look(fude_zoom_parts_form* _form, const c8* _rows, b8 _cut);

// Reads _text as a length, a bare number taken in _unit_choice's unit, into *_mm (millimetres). Returns false when it
// is not a length or is not above 0.
b8   fude_zoom_parts_read(const c8* _text, u8 _unit_choice, f64* _mm);

#endif
