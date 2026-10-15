// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_REPEATFORM_H
#define FUDE_ZOOM_REPEATFORM_H

#include "rde.h"
#include "drawing/widgets/kit.h"
#include "zoom/units.h"

// ===========================================================================
// Repeatform: the card for repeating or mirroring what the lasso holds (the lasso's Repeat, page_measure.c). It sits
// high on the screen, over where the keyboard comes up:
//
//   its title
//   In a row · Round · Mirror
//   In a row:  Copies [3]     Across [ ]   Up [ ]        (from one copy to the next)
//   Round:     Copies [6]     Angle [360°] (for all of them, the original being one)
//              Centre X [ ]   Y [ ]        (on a sheet, from its bottom left corner; else from the middle of what
//              Turn them                    is repeated)
//   Mirror:    Left ↔ right · Top ↔ bottom
//              Keep the original   Line at [ ]  (its X, or its Y: on a sheet from its corner; else from the middle)
//   Cancel, Apply
//
// Where it sits: zoom, the canvas apps' layer, over drawing/widgets/kit.h and units.h. The page (page.h) owns one and
// does the repeating itself when Apply hands back the values.
// Its card is drawing/widgets/kit.h's form (fude_kit_form): the card, its title, Cancel, a tap off it and laying it
// out again are the kit's; the rest is this card's own.
// Its main types and their lifecycle: fude_zoom_repeat_form is the page's struct. fude_zoom_repeat_build makes its
// widgets under the UI's root, which owns and frees them; fude_zoom_repeat_forget clears the struct when the canvas's
// UI goes. fude_zoom_repeat_values is a plain value.
// Threads: main thread only; it is UI.
// Design: sizes are read as the numpad reads them (units.h). A field that cannot be read keeps the card up and says
//   which. Counts must be whole numbers from 1 (in a row) or 2 (round) up to 500, and the angle must not be 0 or more
//   than 360°. Only the chosen mode's fields are read. See docs/infinite_canvas_design.md §24 (Repeat).
// ===========================================================================

// The card's modes, one tab each.
typedef enum {
    FUDE_ZOOM_REPEAT_ROW = 0,
    FUDE_ZOOM_REPEAT_ROUND,
    FUDE_ZOOM_REPEAT_MIRROR,
    // How many modes there are.
    FUDE_ZOOM_REPEAT_MODES
} FUDE_ZOOM_REPEAT_;

// What the card opens with and what Apply hands back.
typedef struct {
    u8  mode;           // FUDE_ZOOM_REPEAT_
    u32 copies;         // in a row: how many new copies; round: how many in all, the original being one of them
    f64 dx_mm, dy_mm;   // in a row: the step from one copy to the next
    f64 angle;          // round: degrees spanned by all of them (360: a whole turn, shared evenly)
    f64 cx_mm, cy_mm;   // round: its centre
    b8  has_xy;         // on a sheet, positions are from its corner (else from the middle of what is repeated)
    b8  turn;           // round: each copy is turned as it goes round
    u8  axis;           // mirror: 0 left ↔ right (across an upright line), 1 top ↔ bottom
    b8  keep;           // mirror: the original is kept, with a mirrored copy beside it
    f64 at_mm;          // mirror: where its line is (its X, or its Y), measured as has_xy says
    f64 edge_mm[2];     // the line to start with, for each axis: when kept, by its right or bottom edge (the copy
    f64 mid_mm[2];      //    beside it); otherwise through its middle (turned over where it is)
} fude_zoom_repeat_values;

// What Apply calls, after the card is put away: _self is what the opener handed fude_zoom_repeat_open, and _typed the
// values as opened with the chosen mode's fields as typed. The pointer is only valid during the call.
typedef void (*fude_zoom_repeat_done)(any _self, const fude_zoom_repeat_values* _typed);

#define FUDE_ZOOM_REPEAT_FIELDS 8u   // row: copies, across, up; round: copies, angle, centre x, y; mirror: its line

struct fude_zoom_repeat_form;
// A chip's user data: the card it is on, and its index (the modes, then the two axes).
typedef struct {
    struct fude_zoom_repeat_form* form;
    u32                           index;
} fude_zoom_repeat_ref;

// The repeat card: its widgets and the values being edited.
typedef struct fude_zoom_repeat_form {
    fude_kit_form           card;   // The card, its title, when it opened and the screen it was laid out for (kit.h).
    rde_ui_button*          modes[FUDE_ZOOM_REPEAT_MODES];
    rde_ui_label*           names[FUDE_ZOOM_REPEAT_FIELDS];
    rde_ui_text_editor*     fields[FUDE_ZOOM_REPEAT_FIELDS];
    rde_ui_button*          turn;
    rde_ui_button*          axes[2];
    rde_ui_button*          keep;
    rde_ui_label*           hint;
    rde_ui_label*           problem;
    rde_ui_button*          cancel;
    rde_ui_button*          save;
    fude_zoom_repeat_ref    refs[FUDE_ZOOM_REPEAT_MODES + 2u];
    // The values it opened with, as changed by the chips since.
    fude_zoom_repeat_values was;
    b8                      at_typed;    // the mirror's line was typed, so choosing an axis or Keep leaves it alone
    // How lengths are written in the fields (the canvas's units).
    fude_zoom_units_style   units;
    fude_zoom_repeat_done   done;
    any                     self;
} fude_zoom_repeat_form;

// fude_zoom_repeat_build makes the card's widgets under _root, hidden, with _font for its words.
// fude_zoom_repeat_forget clears the struct when the canvas's UI goes (the UI frees the widgets); every other call then
// does nothing.
void fude_zoom_repeat_build(fude_zoom_repeat_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font);
void fude_zoom_repeat_forget(fude_zoom_repeat_form* _form);
// Opens the card with _now's values, its mode's tab shown, lengths written in _units. A row's copies start at 3 when
// _now has none, a round's at 6, and the angle at 360° when _now's is 0. Apply calls _done with _self.
void fude_zoom_repeat_open(fude_zoom_repeat_form* _form, const fude_zoom_repeat_values* _now, const fude_zoom_units_style* _units,
                           fude_zoom_repeat_done _done, any _self);
// fude_zoom_repeat_close puts the card away without calling back; fude_zoom_repeat_shown returns whether it is up;
// fude_zoom_repeat_update, called once a frame, lays it out again when the screen's size changes;
// fude_zoom_repeat_restyle gives its widgets the active theme's colours.
void fude_zoom_repeat_close(fude_zoom_repeat_form* _form);
b8   fude_zoom_repeat_shown(const fude_zoom_repeat_form* _form);
void fude_zoom_repeat_update(fude_zoom_repeat_form* _form);
void fude_zoom_repeat_restyle(fude_zoom_repeat_form* _form);

// For the look harness (drawing/app/look.h, a developer's launch flags): chooses mode _mode, types the fields in
// _typed (FUDE_ZOOM_REPEAT_FIELDS of them; a NULL list or entry leaves a field as it is), sets the mirror's axis and
// Keep (-1 leaves each as it is), and with _apply presses Apply. It does nothing while the card is not up.
void fude_zoom_repeat_look(fude_zoom_repeat_form* _form, u8 _mode, const c8* const* _typed, i32 _axis, i32 _keep, b8 _apply);

#endif
