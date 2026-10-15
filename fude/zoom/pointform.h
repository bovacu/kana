// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_POINTFORM_H
#define FUDE_ZOOM_POINTFORM_H

#include "rde.h"
#include "drawing/widgets/kit.h"
#include "zoom/units.h"

// ===========================================================================
// Pointform: a point typed for the Curve tool (its "Type a point" chip, page_shapes.c), on a card high on the screen,
// over where the keyboard comes up:
//
//   its title
//   At · From the last point · Length and angle
//   At:                X [   ]        Y [   ]     (from the sheet's bottom left corner; off a sheet, from the
//                                                   canvas's middle)
//   From the last:     Across [   ]   Up [   ]
//   Length and angle:  Length [   ]   Angle [   ] (from the last point, the sheet's X direction being 0°)
//   Close, Finish, Add point
//
// Where it sits: zoom, the canvas apps' layer, over drawing/widgets/kit.h and units.h. The page (page.h) owns one and
// adds the points to the curve it is drawing.
// Its card is drawing/widgets/kit.h's form (fude_kit_form): the card, its title, Cancel, a tap off it and laying it
// out again are the kit's; the rest is this card's own.
// Its main types and their lifecycle: fude_zoom_point_form is the page's struct. fude_zoom_point_build makes its
// widgets under the UI's root, which owns and frees them; fude_zoom_point_forget clears the struct when the canvas's
// UI goes. fude_zoom_point_values is a plain value.
// Threads: main thread only; it is UI.
// Design: Add point keeps the card up for the next point (each one a corner, where the line turns); the page then
//   calls fude_zoom_point_added to empty the fields. Finish adds what is typed, if anything, and ends the line. Sizes
//   are read as the numpad reads them (units.h); a field that cannot be read keeps the card up and says which. The
//   two modes measured from the last point are only offered once there is a last point.
// ===========================================================================

// How the point is given: at a place, by an offset from the last point, or by a length and an angle from it.
typedef enum {
    FUDE_ZOOM_POINT_AT = 0,
    FUDE_ZOOM_POINT_BY,
    FUDE_ZOOM_POINT_POLAR,
    // How many modes there are.
    FUDE_ZOOM_POINT_MODES
} FUDE_ZOOM_POINT_;

// A point as typed.
typedef struct {
    u8  mode;     // FUDE_ZOOM_POINT_
    f64 a, b;     // AT: x, y (mm); BY: across, up (mm); POLAR: length (mm), angle (degrees, counter-clockwise)
} fude_zoom_point_values;

// What Add point and Finish call: _self is what the opener handed fude_zoom_point_open, _typed the point (NULL when
// Finish was pressed with nothing typed; valid only during the call), and _finish whether the line ends after it.
// Finish puts the card away before calling; Add point leaves it up.
typedef void (*fude_zoom_point_done)(any _self, const fude_zoom_point_values* _typed, b8 _finish);

struct fude_zoom_point_form;
// A mode chip's user data: the card it is on, and its mode.
typedef struct {
    struct fude_zoom_point_form* form;
    u32                          index;
} fude_zoom_point_ref;

// The point card: its widgets and the mode chosen.
typedef struct fude_zoom_point_form {
    fude_kit_form         card;   // The card, its title, when it opened and the screen it was laid out for (kit.h).
    rde_ui_button*        modes[FUDE_ZOOM_POINT_MODES];
    rde_ui_label*         names[2];
    rde_ui_text_editor*   fields[2];
    rde_ui_label*         hint;
    rde_ui_label*         problem;
    rde_ui_button*        close;
    rde_ui_button*        finish;
    rde_ui_button*        add;
    fude_zoom_point_ref   refs[FUDE_ZOOM_POINT_MODES];
    // FUDE_ZOOM_POINT_: the mode chosen.
    u8                    mode;
    b8                    has_last;   // there is a point to go from (otherwise only At is offered)
    b8                    on_sheet;   // At counts from a sheet's corner (otherwise from the canvas's middle)
    // How lengths are read and written (the canvas's units).
    fude_zoom_units_style units;
    fude_zoom_point_done  done;
    any                   self;
} fude_zoom_point_form;

// fude_zoom_point_build makes the card's widgets under _root, hidden, with _font for its words.
// fude_zoom_point_forget clears the struct when the canvas's UI goes (the UI frees the widgets); every other call then
// does nothing.
void fude_zoom_point_build(fude_zoom_point_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font);
void fude_zoom_point_forget(fude_zoom_point_form* _form);
// Opens the card in _mode (At when there is no point to go from, _has_last false) with its fields empty and the first
// one focused. _on_sheet says At counts from a sheet's corner. Lengths are read in _units. _done is called with _self.
void fude_zoom_point_open(fude_zoom_point_form* _form, u8 _mode, b8 _has_last, b8 _on_sheet, const fude_zoom_units_style* _units,
                          fude_zoom_point_done _done, any _self);
// Tells the card a point was added: there is now one to go from, and its fields are emptied for the next. The card
// stays up. Does nothing when it is not shown.
void fude_zoom_point_added(fude_zoom_point_form* _form);
// fude_zoom_point_close puts the card away without calling back; fude_zoom_point_shown returns whether it is up;
// fude_zoom_point_update, called once a frame, lays it out again when the screen's size changes;
// fude_zoom_point_restyle gives its widgets the active theme's colours.
void fude_zoom_point_close(fude_zoom_point_form* _form);
b8   fude_zoom_point_shown(const fude_zoom_point_form* _form);
void fude_zoom_point_update(fude_zoom_point_form* _form);
void fude_zoom_point_restyle(fude_zoom_point_form* _form);

// For the look harness (drawing/app/look.h, a developer's launch flags): with the card up, chooses _mode (if it is
// offered), types _a and _b into the two fields (NULL leaves a field as it is), then presses Add point (_press 1) or
// Finish (2), or nothing (0).
void fude_zoom_point_look(fude_zoom_point_form* _form, u8 _mode, const c8* _a, const c8* _b, u8 _press);

#endif
