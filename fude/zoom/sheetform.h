// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SHEETFORM_H
#define FUDE_ZOOM_SHEETFORM_H

#include "rde.h"
#include "drawing/widgets/kit.h"
#include "zoom/units.h"
#include "zoom/sheet.h"

// ===========================================================================
// A measured sheet's card (Insert → Sheet, or a sheet's size tapped: sheet.h),
// high on the screen, over where the keyboard comes, as the parts to cut are
// typed (partsform.h) —
//
//   its title
//   Paper:  Free size, A4, A3, A2, A1, A0, Letter, Tabloid
//           Portrait, Landscape                                   Grid
//   Scale:  1:1, 1:2, 1:5, 1:10, 1:20, 1:50, 1:100
//   its width × its height (its TRUE size, as the numpad reads a length:
//   "600", "60 cm", "23 5/8")
//   what it prints on ("Prints on 297 × 210 mm"), or what could not be read
//   Cancel, Insert (or Done)
//
// A paper chosen makes it that paper times the scale (A3 at 1:50: 21 × 14.85 m);
// a size typed makes it a free size (the scale is then only how it prints).
// ===========================================================================

#define FUDE_ZOOM_SHEET_SCALES 7u

// What Insert hands back: its true size (mm), its scale (N of 1:N) and its bits (FUDE_ZOOM_SHEET_).
typedef void (*fude_zoom_sheet_done)(void* _self, f64 _w_mm, f64 _h_mm, f64 _scale, u32 _flags);

struct fude_zoom_sheet_form;
typedef struct {
    struct fude_zoom_sheet_form* form;
    u32                          index;
} fude_zoom_sheet_ref;

typedef struct fude_zoom_sheet_form {
    fude_kit_modal        modal;
    rde_window*           window;
    rde_font*             font;
    rde_ui_label*         title;
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
    fude_zoom_sheet_ref   refs[1u + FUDE_ZOOM_PAPER_COUNT + 2u + FUDE_ZOOM_SHEET_SCALES];
    u32                   paper;          // FUDE_ZOOM_PAPER_, or FUDE_ZOOM_NONE: a free size
    b8                    landscape;
    b8                    grid_on;
    f64                   scale;
    b8                    problem;        // the hint says what could not be read
    b8                    setting;        // the fields being filled in (not typed)
    fude_zoom_units_style units;
    fude_zoom_sheet_done  done;
    void*                 self;
    f64                   opened_at;
    rde_vec_2F            laid_out;
} fude_zoom_sheet_form;

// Its widgets under _root, hidden; and let go (the canvas going).
void fude_zoom_sheet_build(fude_zoom_sheet_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font);
void fude_zoom_sheet_forget(fude_zoom_sheet_form* _form);
// Up, as _w_mm × _h_mm at 1:_scale (its paper found from them), _flags its bits, sizes said in _units;
// _new: Insert, else Done. Its button calls _done (the card put away first).
void fude_zoom_sheet_open(fude_zoom_sheet_form* _form, const c8* _title, b8 _new, f64 _w_mm, f64 _h_mm, f64 _scale, u32 _flags,
                          const fude_zoom_units_style* _units, fude_zoom_sheet_done _done, void* _self);
void fude_zoom_sheet_close(fude_zoom_sheet_form* _form);
b8   fude_zoom_sheet_shown(const fude_zoom_sheet_form* _form);
// Once a frame: laid out again when the screen changes.
void fude_zoom_sheet_update(fude_zoom_sheet_form* _form);
void fude_zoom_sheet_restyle(fude_zoom_sheet_form* _form);

// A developer's look: a paper chosen (FUDE_ZOOM_NONE: none), the way, a scale chosen (its index), sizes typed
// (NULL: none), then (_save) its button pressed.
void fude_zoom_sheet_look(fude_zoom_sheet_form* _form, u32 _paper, b8 _landscape, u32 _scale, const c8* _w, const c8* _h, b8 _save);

#endif
