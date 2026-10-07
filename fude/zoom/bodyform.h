// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_BODYFORM_H
#define FUDE_ZOOM_BODYFORM_H

#include "rde.h"
#include "drawing/widgets/kit.h"

// ===========================================================================
// A drawing made a body (Mechanisms' Make body, on the lasso's row: props.h):
// a card with its material (sim/body.h's: a chip each), moving or fixed, its
// mass as its material and its size make it, and — when it is a body already
// — Not a body. Apply hands the choice on; Cancel, or a tap off the card,
// nothing.
// ===========================================================================

#define FUDE_ZOOM_BODY_MATERIALS 8u

// _material: sim/body.h's index; _remove: it is to be a body no more.
typedef void (*fude_zoom_body_done)(void* _self, u32 _material, b8 _fixed, b8 _remove);

struct fude_zoom_body_form;
typedef struct {
    struct fude_zoom_body_form* form;
    u32                         index;
} fude_zoom_body_ref;

typedef struct fude_zoom_body_form {
    fude_kit_modal      modal;
    rde_window*         window;
    rde_font*           font;
    rde_ui_label*       title;
    rde_ui_label*       mass;
    rde_ui_button*      materials[FUDE_ZOOM_BODY_MATERIALS];
    rde_ui_button*      kinds[2];    // moving, fixed
    rde_ui_button*      remove;
    rde_ui_button*      cancel;
    rde_ui_button*      apply;
    fude_zoom_body_ref  material_refs[FUDE_ZOOM_BODY_MATERIALS];
    fude_zoom_body_ref  kind_refs[2];
    u32                 material;
    b8                  fixed;
    b8                  was_body;    // (Not a body shown)
    f64                 area_mm2;    // its drawing's (its mass shown from it)
    fude_zoom_body_done done;
    void*               self;
    f64                 opened_at;
    rde_vec_2F          laid_out;
} fude_zoom_body_form;

void fude_zoom_body_build(fude_zoom_body_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font);
void fude_zoom_body_forget(fude_zoom_body_form* _form);
// Up for a drawing of area _area_mm2 (0: not known), as it is now (_was_body: a body already), _done on Apply.
void fude_zoom_body_open(fude_zoom_body_form* _form, u32 _material, b8 _fixed, b8 _was_body, f64 _area_mm2, fude_zoom_body_done _done, void* _self);
void fude_zoom_body_close(fude_zoom_body_form* _form);
b8   fude_zoom_body_shown(const fude_zoom_body_form* _form);
void fude_zoom_body_update(fude_zoom_body_form* _form);
void fude_zoom_body_restyle(fude_zoom_body_form* _form);
// A developer's look: material _material, fixed or not chosen, then (_finish) 0 left up, 1 Apply, 2 Not a body.
void fude_zoom_body_look(fude_zoom_body_form* _form, u32 _material, b8 _fixed, u32 _finish);

#endif
