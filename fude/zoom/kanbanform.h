// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_KANBANFORM_H
#define FUDE_ZOOM_KANBANFORM_H

#include "rde.h"
#include "drawing/widgets/kit.h"

// ===========================================================================
// A Kanban board's columns (Insert → Kanban board): a card high on the screen,
// over where the keyboard comes, as the parts to cut are typed (partsform.h) —
//
//   its title
//   Templates: Kanban, Scrum, Retrospective, Week (each puts its columns in the rows)
//   a list that scrolls, a row a column: its name, and a trash
//   Add column, under the list
//   Cancel, Insert
//
// It comes up with the Kanban template's columns, the keyboard down (a row tapped
// brings it); a row left empty is skipped.
// ===========================================================================

#define FUDE_ZOOM_KANBAN_ROWS      10u    // columns at most
#define FUDE_ZOOM_KANBAN_TEMPLATES 4u
#define FUDE_ZOOM_KANBAN_NAME      128u   // a column's name's bytes, its NUL included, at most

// What Insert hands back: the columns' names, left to right (none empty).
typedef void (*fude_zoom_kanban_done)(void* _self, const c8* const* _names, u32 _n);

struct fude_zoom_kanban_form;
typedef struct {
    struct fude_zoom_kanban_form* form;
    u32                           index;
} fude_zoom_kanban_ref;

typedef struct fude_zoom_kanban_form {
    fude_kit_modal        modal;
    rde_window*           window;
    rde_font*             font;
    rde_ui_label*         title;
    rde_ui_label*         templates_label;
    rde_ui_button*        templates[FUDE_ZOOM_KANBAN_TEMPLATES];
    fude_zoom_kanban_ref  template_refs[FUDE_ZOOM_KANBAN_TEMPLATES];
    rde_ui_scroll_area*   list;
    rde_ui_text_editor*   names[FUDE_ZOOM_KANBAN_ROWS];
    rde_ui_button*        drops[FUDE_ZOOM_KANBAN_ROWS];
    fude_zoom_kanban_ref  refs[FUDE_ZOOM_KANBAN_ROWS];
    u32                   rows;
    rde_ui_button*        add;
    rde_ui_label*         problem;           // nothing named (empty: fine)
    rde_ui_button*        cancel;
    rde_ui_button*        save;
    fude_zoom_kanban_done done;
    void*                 self;
    f64                   opened_at;
    rde_vec_2F            laid_out;
} fude_zoom_kanban_form;

// Its widgets under _root, hidden; and let go (the canvas going).
void fude_zoom_kanban_build(fude_zoom_kanban_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font);
void fude_zoom_kanban_forget(fude_zoom_kanban_form* _form);
// Up, the Kanban template's columns in its rows. Insert calls _done (the card put away first).
void fude_zoom_kanban_open(fude_zoom_kanban_form* _form, fude_zoom_kanban_done _done, void* _self);
void fude_zoom_kanban_close(fude_zoom_kanban_form* _form);
b8   fude_zoom_kanban_shown(const fude_zoom_kanban_form* _form);
// Once a frame: laid out again when the screen changes.
void fude_zoom_kanban_update(fude_zoom_kanban_form* _form);
void fude_zoom_kanban_restyle(fude_zoom_kanban_form* _form);

// A template's columns (_which: 0 Kanban, 1 Scrum, 2 Retrospective, 3 Week), in the app's language, into
// _out. How many.
u32  fude_zoom_kanban_template(u32 _which, const c8* _out[FUDE_ZOOM_KANBAN_ROWS]);

// A developer's look: template _which in its rows (and _extra a row more, "Ideas"), then (_insert) Insert pressed.
void fude_zoom_kanban_look(fude_zoom_kanban_form* _form, u32 _which, b8 _extra, b8 _insert);

#endif
