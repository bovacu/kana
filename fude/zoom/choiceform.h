// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_CHOICEFORM_H
#define FUDE_ZOOM_CHOICEFORM_H

#include "rde.h"
#include "drawing/widgets/kit.h"

// ===========================================================================
// A choice asked on a small card in the middle of the screen: its title, up to
// FUDE_ZOOM_CHOICE_MAX buttons (each a line, an icon before it), and Cancel. A
// button pressed puts the card away and hands its index on; Cancel, or a tap
// off the card, does not.
// ===========================================================================

#define FUDE_ZOOM_CHOICE_MAX 6u

typedef void (*fude_zoom_choice_done)(void* _self, u32 _index);

struct fude_zoom_choice_form;
typedef struct {
    struct fude_zoom_choice_form* form;
    u32                           index;
} fude_zoom_choice_ref;

typedef struct fude_zoom_choice_form {
    fude_kit_modal        modal;
    rde_window*           window;
    rde_font*             font;
    rde_ui_label*         title;
    rde_ui_button*        choices[FUDE_ZOOM_CHOICE_MAX];
    rde_ui_button*        cancel;
    fude_zoom_choice_ref  refs[FUDE_ZOOM_CHOICE_MAX];
    u32                   count;
    fude_zoom_choice_done done;
    void*                 self;
    f64                   opened_at;
    rde_vec_2F            laid_out;
} fude_zoom_choice_form;

void fude_zoom_choice_build(fude_zoom_choice_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font);
void fude_zoom_choice_forget(fude_zoom_choice_form* _form);
// Up with _title and _count choices (_labels, _icons: NULL for none), _done called with the one pressed.
void fude_zoom_choice_open(fude_zoom_choice_form* _form, const c8* _title, const c8* const* _labels, const c8* const* _icons, u32 _count,
                           fude_zoom_choice_done _done, void* _self);
void fude_zoom_choice_close(fude_zoom_choice_form* _form);
b8   fude_zoom_choice_shown(const fude_zoom_choice_form* _form);
void fude_zoom_choice_update(fude_zoom_choice_form* _form);
void fude_zoom_choice_restyle(fude_zoom_choice_form* _form);
// A developer's look: choice _index pressed.
void fude_zoom_choice_look(fude_zoom_choice_form* _form, u32 _index);

#endif
