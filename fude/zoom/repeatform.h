// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_REPEATFORM_H
#define FUDE_ZOOM_REPEATFORM_H

#include "rde.h"
#include "drawing/widgets/kit.h"
#include "zoom/units.h"

// ===========================================================================
// What the lasso holds, repeated or mirrored (the lasso's Repeat: page.c), on a
// card high on the screen, over where the keyboard comes —
//
//   its title
//   In a row · Round · Mirror
//   In a row:  Copies [3]     Across [ ]   Up [ ]        (from one to the next)
//   Round:     Copies [6]     Angle [360°] (all of them, the original one)
//              Centre X [ ]   Y [ ]        (on a sheet: from its bottom left
//              Turn them                    corner; else from its middle)
//   Mirror:    Left ↔ right · Top ↔ bottom
//              Keep the original   Line at [ ]  (its X, or its Y: on a sheet from
//                                               its corner; else from the middle)
//   Cancel, Apply
//
// Sizes read as the numpad reads them (units.h); one that cannot be read keeps
// the card up and says which.
// ===========================================================================

typedef enum {
    FUDE_ZOOM_REPEAT_ROW = 0,
    FUDE_ZOOM_REPEAT_ROUND,
    FUDE_ZOOM_REPEAT_MIRROR,
    FUDE_ZOOM_REPEAT_MODES
} FUDE_ZOOM_REPEAT_;

typedef struct {
    u8  mode;           // FUDE_ZOOM_REPEAT_
    u32 copies;         // in a row: new copies; round: how many in all, the original one of them
    f64 dx_mm, dy_mm;   // in a row: from one to the next
    f64 angle;          // round: degrees, all the way round them (360: a whole turn, evenly)
    f64 cx_mm, cy_mm;   // round: its centre
    b8  has_xy;         // on a sheet: the centre from its corner (else from the middle of what is repeated)
    b8  turn;           // round: each copy turned as it goes round
    u8  axis;           // mirror: 0 left ↔ right (across an upright line), 1 top ↔ bottom
    b8  keep;           // mirror: the original kept (a mirrored copy beside it)
    f64 at_mm;          // mirror: where its line is (its X, or its Y), as has_xy says
    f64 edge_mm[2];     // ...to begin with: kept, by its edge (its right, its bottom: the copy beside it); else
    f64 mid_mm[2];      //    through its middle (turned over where it is) — each axis's
} fude_zoom_repeat_values;

typedef void (*fude_zoom_repeat_done)(void* _self, const fude_zoom_repeat_values* _typed);

#define FUDE_ZOOM_REPEAT_FIELDS 8u   // in a row: copies, across, up; round: copies, angle, centre x, centre y; mirror: its line

struct fude_zoom_repeat_form;
typedef struct {
    struct fude_zoom_repeat_form* form;
    u32                           index;
} fude_zoom_repeat_ref;

typedef struct fude_zoom_repeat_form {
    fude_kit_modal          modal;
    rde_window*             window;
    rde_font*               font;
    rde_ui_label*           title;
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
    fude_zoom_repeat_values was;
    b8                      setting;     // a field being filled in (not typed)
    b8                      at_typed;    // the mirror's line typed (not its own to begin with)
    fude_zoom_units_style   units;
    fude_zoom_repeat_done   done;
    void*                   self;
    f64                     opened_at;
    rde_vec_2F              laid_out;
} fude_zoom_repeat_form;

void fude_zoom_repeat_build(fude_zoom_repeat_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font);
void fude_zoom_repeat_forget(fude_zoom_repeat_form* _form);
// Up with _now's values (its mode the tab shown); Apply calls _done with what was typed (the card put away first).
void fude_zoom_repeat_open(fude_zoom_repeat_form* _form, const fude_zoom_repeat_values* _now, const fude_zoom_units_style* _units,
                           fude_zoom_repeat_done _done, void* _self);
void fude_zoom_repeat_close(fude_zoom_repeat_form* _form);
b8   fude_zoom_repeat_shown(const fude_zoom_repeat_form* _form);
void fude_zoom_repeat_update(fude_zoom_repeat_form* _form);
void fude_zoom_repeat_restyle(fude_zoom_repeat_form* _form);

// A developer's look: a mode chosen, fields typed (NULL: as they are; FUDE_ZOOM_REPEAT_FIELDS of them), the mirror's
// axis (-1: as it is) and keep (-1: as it is), then (_apply) Apply pressed.
void fude_zoom_repeat_look(fude_zoom_repeat_form* _form, u8 _mode, const c8* const* _typed, i32 _axis, i32 _keep, b8 _apply);

#endif
