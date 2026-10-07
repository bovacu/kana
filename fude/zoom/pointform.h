// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_POINTFORM_H
#define FUDE_ZOOM_POINTFORM_H

#include "rde.h"
#include "drawing/widgets/kit.h"
#include "zoom/units.h"

// ===========================================================================
// A point typed for the Curve tool (its "Type a point" chip: page.c), on a card
// high on the screen, over where the keyboard comes —
//
//   its title
//   At · From the last point · Length and angle
//   At:                X [   ]        Y [   ]     (from the sheet's bottom left
//                                                   corner; off a sheet, from the
//                                                   canvas's middle)
//   From the last:     Across [   ]   Up [   ]
//   Length and angle:  Length [   ]   Angle [   ] (from the last point, the
//                                                   sheet's X its 0°)
//   Close, Finish, Add point
//
// Add point keeps the card up for the next (each a corner: the line turns there);
// Finish adds what is typed, if anything, and ends the line. Sizes read as the
// numpad reads them (units.h); one that cannot be read keeps the card up and says
// which.
// ===========================================================================

typedef enum {
    FUDE_ZOOM_POINT_AT = 0,
    FUDE_ZOOM_POINT_BY,
    FUDE_ZOOM_POINT_POLAR,
    FUDE_ZOOM_POINT_MODES
} FUDE_ZOOM_POINT_;

typedef struct {
    u8  mode;     // FUDE_ZOOM_POINT_
    f64 a, b;     // AT: x, y (mm); BY: across, up (mm); POLAR: length (mm), angle (degrees, counter-clockwise)
} fude_zoom_point_values;

// _typed: the point (NULL: none — Finish with nothing typed); _finish: the line ended after it.
typedef void (*fude_zoom_point_done)(void* _self, const fude_zoom_point_values* _typed, b8 _finish);

struct fude_zoom_point_form;
typedef struct {
    struct fude_zoom_point_form* form;
    u32                          index;
} fude_zoom_point_ref;

typedef struct fude_zoom_point_form {
    fude_kit_modal        modal;
    rde_window*           window;
    rde_font*             font;
    rde_ui_label*         title;
    rde_ui_button*        modes[FUDE_ZOOM_POINT_MODES];
    rde_ui_label*         names[2];
    rde_ui_text_editor*   fields[2];
    rde_ui_label*         hint;
    rde_ui_label*         problem;
    rde_ui_button*        close;
    rde_ui_button*        finish;
    rde_ui_button*        add;
    fude_zoom_point_ref   refs[FUDE_ZOOM_POINT_MODES];
    u8                    mode;
    b8                    has_last;   // a point to go from (else only At)
    b8                    on_sheet;   // At counts from a sheet's corner (else from the canvas's middle)
    b8                    setting;
    fude_zoom_units_style units;
    fude_zoom_point_done  done;
    void*                 self;
    f64                   opened_at;
    rde_vec_2F            laid_out;
} fude_zoom_point_form;

void fude_zoom_point_build(fude_zoom_point_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font);
void fude_zoom_point_forget(fude_zoom_point_form* _form);
// Up in _mode (none to go from: At), its fields empty.
void fude_zoom_point_open(fude_zoom_point_form* _form, u8 _mode, b8 _has_last, b8 _on_sheet, const fude_zoom_units_style* _units,
                          fude_zoom_point_done _done, void* _self);
// A point added: there is one to go from now (the card stays up, its fields emptied).
void fude_zoom_point_added(fude_zoom_point_form* _form);
void fude_zoom_point_close(fude_zoom_point_form* _form);
b8   fude_zoom_point_shown(const fude_zoom_point_form* _form);
void fude_zoom_point_update(fude_zoom_point_form* _form);
void fude_zoom_point_restyle(fude_zoom_point_form* _form);

// A developer's look: a mode, the two fields typed (NULL: as they are), then Add (1) or Finish (2) pressed (0: none).
void fude_zoom_point_look(fude_zoom_point_form* _form, u8 _mode, const c8* _a, const c8* _b, u8 _press);

#endif
