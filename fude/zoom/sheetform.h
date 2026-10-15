// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SHEETFORM_H
#define FUDE_ZOOM_SHEETFORM_H

#include "rde.h"
#include "drawing/widgets/kit.h"
#include "zoom/units.h"
#include "zoom/sheet.h"

// ===========================================================================
// Sheetform: a measured sheet's card (sheet.h), opened by Insert, Measured sheet, or by tapping a sheet's size. Like
// the card for the parts to cut (partsform.h), it sits high on the screen, over where the keyboard comes up:
//
//   its title
//   Paper:  Free size, A4, A3, A2, A1, A0, Letter, Tabloid
//           Portrait, Landscape                                   Grid
//   Scale:  1:1, 1:2, 1:5, 1:10, 1:20, 1:50, 1:100
//   its width × its height (its TRUE size, read as the numpad reads a length: "600", "60 cm", "23 5/8")
//   what it prints on ("Prints on 297 × 210 mm"), or what could not be read
//   Cancel, Insert (or Done)
//
// Where it sits: zoom, the canvas apps' layer, over drawing/widgets/kit.h, units.h and sheet.h. The page (page.h) owns
// one and opens it.
// Its card is drawing/widgets/kit.h's form (fude_kit_form): the card, its title, Cancel, a tap off it and laying it
// out again are the kit's; the rest is this card's own.
// Its main types and their lifecycle: fude_zoom_sheet_form is the page's struct. fude_zoom_sheet_build makes its
// widgets under the UI's root, which owns and frees them; fude_zoom_sheet_forget clears the struct when the canvas's
// UI goes.
// Threads: main thread only; it is UI.
// Design: choosing a paper makes the sheet that paper times the scale (A3 at 1:50 is 21 × 14.85 m), because a sheet is
//   always drawn at its true size. Typing a size makes it a free size, and the scale is then only how it prints.
//   Insert is refused, with the reason on the hint line, while either size cannot be read. A tap off the card closes
//   it, except in the first 0.6 seconds, so the release of the press that opened it does not close it again.
// ===========================================================================

// How many scales the card offers (1:1 to 1:100).
#define FUDE_ZOOM_SHEET_SCALES 7u

// What Insert (or Done) calls, after the card is put away: _self is what the opener handed fude_zoom_sheet_open,
// _w_mm and _h_mm the sheet's true size, _scale N of 1:N, and _flags its FUDE_ZOOM_SHEET_ bits.
typedef void (*fude_zoom_sheet_done)(any _self, f64 _w_mm, f64 _h_mm, f64 _scale, u32 _flags);

struct fude_zoom_sheet_form;
// A chip's user data: the card it is on, and its place in the card's refs.
typedef struct {
    struct fude_zoom_sheet_form* form;
    u32                          index;
} fude_zoom_sheet_ref;

// The sheet's card: its widgets and what is chosen on it.
typedef struct fude_zoom_sheet_form {
    fude_kit_form         card;   // The card, its title, when it opened and the screen it was laid out for (kit.h).
    rde_ui_label*         paper_label;
    rde_ui_button*        papers[1u + FUDE_ZOOM_PAPER_COUNT];   // free size, then each paper
    rde_ui_button*        ways[2];                               // portrait, landscape
    rde_ui_button*        grid;
    rde_ui_label*         scale_label;
    rde_ui_button*        scales[FUDE_ZOOM_SHEET_SCALES];
    rde_ui_text_editor*   width;
    rde_ui_text_editor*   height;
    rde_ui_label*         times;
    rde_ui_label*         hint;                                  // what it prints on, or what could not be read
    rde_ui_button*        cancel;
    rde_ui_button*        save;
    // The chips' user data: the papers, then the two ways up, then the scales.
    fude_zoom_sheet_ref   refs[1u + FUDE_ZOOM_PAPER_COUNT + 2u + FUDE_ZOOM_SHEET_SCALES];
    u32                   paper;          // FUDE_ZOOM_PAPER_, or FUDE_ZOOM_NONE for a free size
    b8                    landscape;
    b8                    grid_on;
    // The scale chosen: N of 1:N.
    f64                   scale;
    b8                    problem;        // the hint says what could not be read
    // How the sizes are written in the fields (the canvas's units).
    fude_zoom_units_style units;
    fude_zoom_sheet_done  done;
    any                   self;
} fude_zoom_sheet_form;

// fude_zoom_sheet_build makes the card's widgets under _root, hidden, with _font for its words.
// fude_zoom_sheet_forget clears the struct when the canvas's UI goes (the UI frees the widgets); every other call then
// does nothing.
void fude_zoom_sheet_build(fude_zoom_sheet_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font);
void fude_zoom_sheet_forget(fude_zoom_sheet_form* _form);
// Opens the card titled _title for a sheet _w_mm × _h_mm at 1:_scale with _flags, its paper found from those, its sizes
// written in _units. Its button says Insert when _new, else Done, and calls _done with _self once both sizes read.
void fude_zoom_sheet_open(fude_zoom_sheet_form* _form, const c8* _title, b8 _new, f64 _w_mm, f64 _h_mm, f64 _scale, u32 _flags,
                          const fude_zoom_units_style* _units, fude_zoom_sheet_done _done, any _self);
// fude_zoom_sheet_close puts the card away without calling back; fude_zoom_sheet_shown returns whether it is up.
void fude_zoom_sheet_close(fude_zoom_sheet_form* _form);
b8   fude_zoom_sheet_shown(const fude_zoom_sheet_form* _form);
// fude_zoom_sheet_update is called once a frame and lays the card out again when the screen's size changes.
// fude_zoom_sheet_restyle gives its widgets the active theme's colours.
void fude_zoom_sheet_update(fude_zoom_sheet_form* _form);
void fude_zoom_sheet_restyle(fude_zoom_sheet_form* _form);

// For the look harness (drawing/app/look.h, a developer's launch flags): presses the card's buttons as a person would.
// It chooses scale number _scale (an index; none when out of range), then paper _paper (FUDE_ZOOM_NONE: none), then
// the way up, types the sizes _w and _h (NULL: left as they are), and with _save presses Insert. It does nothing while
// the card is not up.
void fude_zoom_sheet_look(fude_zoom_sheet_form* _form, u32 _paper, b8 _landscape, u32 _scale, const c8* _w, const c8* _h, b8 _save);

#endif
