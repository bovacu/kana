// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SIZEFORM_H
#define FUDE_ZOOM_SIZEFORM_H

#include "rde.h"
#include "drawing/widgets/kit.h"
#include "zoom/units.h"

// ===========================================================================
// What the lasso holds, made an exact size (its size tapped: page.c), on a card
// high on the screen, over where the keyboard comes, as the parts to cut are
// typed (partsform.h) —
//
//   its title
//   Width  [   ]    Height [   ]        (a line: Length only)
//   Angle  [   ]    Keep proportions     (what only scales whole: always kept;
//                                         its angle a turn by, from 0)
//   X      [   ]    Y      [   ]         (on a measured sheet: its box's bottom
//   from the sheet's bottom left corner   left corner from the sheet's)
//   its area and perimeter (or length), as it is now
//   Cancel, Apply
//
// Each read as the numpad reads one (units.h: "600", "60 cm", "23 5/8", sums);
// one that cannot be read keeps the card up and says which.
// ===========================================================================

typedef struct {
    f64 w_mm, h_mm;   // its size (a line: its length in w_mm)
    f64 angle;        // degrees, counter-clockwise (turn: 0)
    f64 x_mm, y_mm;   // its box's bottom left corner from the sheet's (has_xy)
    b8  has_h;        // a height (not a line)
    b8  has_xy;       // on a sheet: where it is
    b8  can_unlock;   // its sides each its own (a rectangle's, an ellipse's, a board's): the lock shown
    b8  locked;       // its sides kept in proportion
    b8  turn;         // the angle a turn by (what has none of its own)
} fude_zoom_size_values;

typedef void (*fude_zoom_size_done)(void* _self, const fude_zoom_size_values* _typed);

#define FUDE_ZOOM_SIZE_FIELDS 5u   // width, height, angle, x, y

typedef struct {
    fude_kit_modal        modal;
    rde_window*           window;
    rde_font*             font;
    rde_ui_label*         title;
    rde_ui_label*         names[FUDE_ZOOM_SIZE_FIELDS];
    rde_ui_text_editor*   fields[FUDE_ZOOM_SIZE_FIELDS];
    rde_ui_button*        lock;
    rde_ui_label*         from;      // where X and Y count from
    rde_ui_label*         info;      // its area and perimeter (or length)
    rde_ui_label*         problem;   // what could not be read (empty: fine)
    rde_ui_button*        cancel;
    rde_ui_button*        save;
    fude_zoom_size_values was;       // as it opened
    fude_zoom_units_style units;
    b8                    setting;   // the fields being filled in (not typed)
    fude_zoom_size_done   done;
    void*                 self;
    f64                   opened_at;
    rde_vec_2F            laid_out;
} fude_zoom_size_form;

// Its widgets under _root, hidden; and let go (the canvas going).
void fude_zoom_size_build(fude_zoom_size_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font);
void fude_zoom_size_forget(fude_zoom_size_form* _form);
// Up with _now's values (and which fields show), _info under them, sizes said in _units. Apply calls _done with
// what was typed (the card put away first).
void fude_zoom_size_open(fude_zoom_size_form* _form, const c8* _title, const fude_zoom_size_values* _now, const c8* _info,
                         const fude_zoom_units_style* _units, fude_zoom_size_done _done, void* _self);
void fude_zoom_size_close(fude_zoom_size_form* _form);
b8   fude_zoom_size_shown(const fude_zoom_size_form* _form);
void fude_zoom_size_update(fude_zoom_size_form* _form);
void fude_zoom_size_restyle(fude_zoom_size_form* _form);

// A developer's look: fields typed (NULL: as they are; width, height, angle, x, y), the lock (-1: as it is), then
// (_apply) Apply pressed.
void fude_zoom_size_look(fude_zoom_size_form* _form, const c8* const _typed[FUDE_ZOOM_SIZE_FIELDS], i32 _locked, b8 _apply);

// An area (mm²) said in _style's units squared, at most two decimals ("1250 mm²", "12.5 cm²", "0.42 m²", "18 in²",
// "2.5 ft²"): the metric unit the size calls for.
void fude_zoom_size_area_format(f64 _mm2, const fude_zoom_units_style* _style, c8* _out, usize _size);

#endif
