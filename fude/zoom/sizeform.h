// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SIZEFORM_H
#define FUDE_ZOOM_SIZEFORM_H

#include "rde.h"
#include "drawing/widgets/kit.h"
#include "zoom/units.h"

// ===========================================================================
// Sizeform: the card that makes what the lasso holds an exact size, opened by tapping its size (page_measure.c). Like
// the card for the parts to cut (partsform.h), it sits high on the screen, over where the keyboard comes up:
//
//   its title
//   Width  [   ]    Height [   ]        (for a line, only Length)
//   Angle  [   ]    Keep proportions     (what can only be scaled whole always keeps them, and its angle is a turn
//                                         to make, starting from 0)
//   X      [   ]    Y      [   ]         (on a measured sheet: its box's bottom left corner, measured from the
//   from the sheet's bottom left corner   sheet's)
//   its area and perimeter (or length), as it is now
//   Cancel, Apply
//
// Where it sits: zoom, the canvas apps' layer, over drawing/widgets/kit.h and units.h. The page (page.h) owns one and
// does the resizing itself when Apply hands back the values.
// Its card is drawing/widgets/kit.h's form (fude_kit_form): the card, its title, Cancel, a tap off it and laying it
// out again are the kit's; the rest is this card's own.
// Its main types and their lifecycle: fude_zoom_size_form is the page's struct. fude_zoom_size_build makes its widgets
// under the UI's root, which owns and frees them; fude_zoom_size_forget clears the struct when the canvas's UI goes.
// fude_zoom_size_values is a plain value.
// Threads: main thread only; it is UI.
// Design: each field is read as the numpad reads one (units.h: "600", "60 cm", "23 5/8", sums). A field that cannot
//   be read keeps the card up and says which. A width or height must be above 0. With proportions kept, typing one
//   side changes the other to match. See docs/infinite_canvas_design.md §24 (The size card).
// ===========================================================================

// What the card opens with (which also decides which fields show) and what Apply hands back.
typedef struct {
    f64 w_mm, h_mm;   // its size (for a line, its length in w_mm)
    f64 angle;        // degrees, counter-clockwise (with turn, the turn to make, starting at 0)
    f64 x_mm, y_mm;   // its box's bottom left corner from the sheet's (with has_xy)
    b8  has_h;        // it has a height (it is not a line)
    b8  has_xy;       // it is on a sheet, so where it is shows
    b8  can_unlock;   // its sides can change apart (a rectangle's, an ellipse's, a board's), so the lock shows
    b8  locked;       // its sides are kept in proportion
    b8  turn;         // the angle is a turn to make, for what has no angle of its own
} fude_zoom_size_values;

// What Apply calls, after the card is put away: _self is what the opener handed fude_zoom_size_open, and _typed the
// values as opened with the shown fields as typed. The pointer is only valid during the call.
typedef void (*fude_zoom_size_done)(any _self, const fude_zoom_size_values* _typed);

#define FUDE_ZOOM_SIZE_FIELDS 5u   // width, height, angle, x, y

// The size card: its widgets and the values being edited.
typedef struct {
    fude_kit_form         card;      // The card, its title, when it opened and the screen it was laid out for (kit.h).
    rde_ui_label*         names[FUDE_ZOOM_SIZE_FIELDS];
    rde_ui_text_editor*   fields[FUDE_ZOOM_SIZE_FIELDS];
    rde_ui_button*        lock;
    rde_ui_label*         from;      // where X and Y count from
    rde_ui_label*         info;      // its area and perimeter (or length)
    rde_ui_label*         problem;   // what could not be read (empty when all is well)
    rde_ui_button*        cancel;
    rde_ui_button*        save;
    fude_zoom_size_values was;       // as it opened, with the lock as changed since
    // How lengths are written in the fields (the canvas's units).
    fude_zoom_units_style units;
    fude_zoom_size_done   done;
    any                   self;
} fude_zoom_size_form;

// fude_zoom_size_build makes the card's widgets under _root, hidden, with _font for its words.
// fude_zoom_size_forget clears the struct when the canvas's UI goes (the UI frees the widgets); every other call then
// does nothing.
void fude_zoom_size_build(fude_zoom_size_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font);
void fude_zoom_size_forget(fude_zoom_size_form* _form);
// Opens the card titled _title with _now's values, which also decide which fields show, the line _info under them
// (its area and perimeter; NULL: none), and sizes written in _units. Apply calls _done with _self.
void fude_zoom_size_open(fude_zoom_size_form* _form, const c8* _title, const fude_zoom_size_values* _now, const c8* _info,
                         const fude_zoom_units_style* _units, fude_zoom_size_done _done, any _self);
// fude_zoom_size_close puts the card away without calling back; fude_zoom_size_shown returns whether it is up;
// fude_zoom_size_update, called once a frame, lays it out again when the screen's size changes;
// fude_zoom_size_restyle gives its widgets the active theme's colours.
void fude_zoom_size_close(fude_zoom_size_form* _form);
b8   fude_zoom_size_shown(const fude_zoom_size_form* _form);
void fude_zoom_size_update(fude_zoom_size_form* _form);
void fude_zoom_size_restyle(fude_zoom_size_form* _form);

// For the look harness (drawing/app/look.h, a developer's launch flags): sets the lock (_locked; -1 leaves it), types
// the fields in _typed (width, height, angle, x, y; a NULL list or entry leaves a field as it is), and with _apply
// presses Apply. It does nothing while the card is not up.
void fude_zoom_size_look(fude_zoom_size_form* _form, const c8* const _typed[FUDE_ZOOM_SIZE_FIELDS], i32 _locked, b8 _apply);

// Writes an area of _mm2 square millimetres in _style's units squared, with at most two decimals ("1250 mm²",
// "12.5 cm²", "0.42 m²", "18 in²", "2.5 ft²"). An area of a square metre or more is written in m², and one of a
// square foot or more in ft² when the units are inches.
void fude_zoom_size_area_format(f64 _mm2, const fude_zoom_units_style* _style, c8* _out, usize _size);

#endif
