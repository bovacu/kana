// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_BODYFORM_H
#define FUDE_ZOOM_BODYFORM_H

#include "rde.h"
#include "drawing/widgets/kit.h"

// ===========================================================================
// Bodyform: the card that makes a drawing a body that moves in the simulation (Mechanisms' Make body, on the lasso's
// row: props.h).
//
// Where it sits: zoom, the canvas apps' layer, over drawing/widgets/kit.h, whose form (fude_kit_form) is its card; it
// reads the materials from sim/body.h. The page (page.h) owns one and opens it for the drawing the lasso holds.
// Its main types and their lifecycle: fude_zoom_body_form is the page's struct. fude_zoom_body_build makes its widgets
// under the UI's root, which owns and frees them; fude_zoom_body_forget clears the struct when the canvas's UI goes.
// Threads: main thread only; it is UI.
// Design: the card shows its material (sim/body.h's, a chip each), whether it moves or is fixed, its mass as its
//   material and its size make it (a drawing is taken as a plate FUDE_SIM_BODY_THICKNESS thick, since it has no
//   depth), and, when it is a body already, Not a body. Apply hands the choice on; Cancel, or a tap off the card (not
//   within 0.6 seconds of opening), hands nothing back.
// ===========================================================================

// How many materials the card offers: sim/body.h's, in its order.
#define FUDE_ZOOM_BODY_MATERIALS 8u

// What Apply and Not a body call, after the card is put away: _self is what the opener handed fude_zoom_body_open,
// _material sim/body.h's material index, _fixed whether it is fixed (the ground) rather than moving, and _remove
// whether it is to be a body no more.
typedef void (*fude_zoom_body_done)(any _self, u32 _material, b8 _fixed, b8 _remove);

struct fude_zoom_body_form;
// A chip's user data: the card it is on, and its index.
typedef struct {
    struct fude_zoom_body_form* form;
    u32                         index;
} fude_zoom_body_ref;

// The body card: its widgets and what is chosen.
typedef struct fude_zoom_body_form {
    fude_kit_form       card;   // The card, its title, when it opened and the screen it was laid out for (kit.h).
    rde_ui_label*       mass;
    rde_ui_button*      materials[FUDE_ZOOM_BODY_MATERIALS];
    rde_ui_button*      kinds[2];    // moving, fixed
    rde_ui_button*      remove;
    rde_ui_button*      cancel;
    rde_ui_button*      apply;
    fude_zoom_body_ref  material_refs[FUDE_ZOOM_BODY_MATERIALS];
    fude_zoom_body_ref  kind_refs[2];
    // The material chosen (sim/body.h's index), and whether it is fixed.
    u32                 material;
    b8                  fixed;
    b8                  was_body;    // it is a body already, so Not a body shows
    f64                 area_mm2;    // its drawing's area, from which its mass is shown (0: not known)
    fude_zoom_body_done done;
    any                 self;
} fude_zoom_body_form;

// Returns material _material's name (sim/body.h's order) in the person's language, or NULL when there is no such
// material. The string stays valid until the language changes (text.h).
const c8* fude_zoom_body_material_name(u32 _material);

// fude_zoom_body_build makes the card's widgets under _root, hidden, with _font for its words.
// fude_zoom_body_forget clears the struct when the canvas's UI goes (the UI frees the widgets); every other call then
// does nothing.
void fude_zoom_body_build(fude_zoom_body_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font);
void fude_zoom_body_forget(fude_zoom_body_form* _form);
// Opens the card for a drawing of area _area_mm2 (0: not known), as it is now: _material (out of range: the first),
// _fixed, and _was_body when it is a body already. Apply or Not a body calls _done with _self.
void fude_zoom_body_open(fude_zoom_body_form* _form, u32 _material, b8 _fixed, b8 _was_body, f64 _area_mm2, fude_zoom_body_done _done, any _self);
// fude_zoom_body_close puts the card away without calling back; fude_zoom_body_shown returns whether it is up;
// fude_zoom_body_update, called once a frame, lays it out again when the screen's size changes;
// fude_zoom_body_restyle gives its widgets the active theme's colours.
void fude_zoom_body_close(fude_zoom_body_form* _form);
b8   fude_zoom_body_shown(const fude_zoom_body_form* _form);
void fude_zoom_body_update(fude_zoom_body_form* _form);
void fude_zoom_body_restyle(fude_zoom_body_form* _form);
// For the look harness (drawing/app/look.h, a developer's launch flags): with the card up, chooses material _material
// and moving or fixed, then finishes as _finish says: 0 leaves the card up, 1 presses Apply, 2 presses Not a body.
void fude_zoom_body_look(fude_zoom_body_form* _form, u32 _material, b8 _fixed, u32 _finish);

#endif
