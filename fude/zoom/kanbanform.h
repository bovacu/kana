// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_KANBANFORM_H
#define FUDE_ZOOM_KANBANFORM_H

#include "rde.h"
#include "drawing/widgets/kit.h"

// ===========================================================================
// Kanbanform: the card that asks for a Kanban board's columns (Insert, Kanban board). Like the card for the parts to
// cut (partsform.h), it sits high on the screen, over where the keyboard comes up:
//
//   its title
//   Templates: Kanban, Scrum, Retrospective, Week (each puts its columns in the rows)
//   a list that scrolls, one row for each column: its name, and a trash button
//   Add column, under the list
//   Cancel, Insert
//
// Where it sits: zoom, the canvas apps' layer, over drawing/widgets/kit.h. The page (page.h) owns one and builds the
// board from the names Insert hands back.
// Its card is drawing/widgets/kit.h's form (fude_kit_form): the card, its title, Cancel, a tap off it and laying it
// out again are the kit's; the rest is this card's own.
// Its main types and their lifecycle: fude_zoom_kanban_form is the page's struct. fude_zoom_kanban_build makes its
// widgets under the UI's root, which owns and frees them; fude_zoom_kanban_forget clears the struct when the canvas's
// UI goes.
// Threads: main thread only; it is UI.
// Design: it comes up with the Kanban template's columns and the keyboard down, since the rows are already filled in;
//   tapping a row brings the keyboard up. Names are trimmed and a row left empty is skipped; with none named at all,
//   Insert keeps the card up and says so. A template's chip always sets its columns rather than toggling, because a
//   press can arrive twice on Android. See docs/infinite_canvas_design.md §21 (Kanban boards).
// ===========================================================================

// The card's sizes; its FUDE_ZOOM_KANBAN_TEMPLATES templates are Kanban, Scrum, Retrospective and Week.
#define FUDE_ZOOM_KANBAN_ROWS      10u    // the most columns a board can be given here
#define FUDE_ZOOM_KANBAN_TEMPLATES 4u
#define FUDE_ZOOM_KANBAN_NAME      128u   // the most bytes of a column's name, its ending 0 included

// What Insert calls, after the card is put away: _self is what the opener handed fude_zoom_kanban_open, and _names the
// _n columns' names, left to right, none empty. The names are only valid during the call.
typedef void (*fude_zoom_kanban_done)(any _self, const c8* const* _names, u32 _n);

struct fude_zoom_kanban_form;
// A chip's or a row's user data: the card it is on, and its index.
typedef struct {
    struct fude_zoom_kanban_form* form;
    u32                           index;
} fude_zoom_kanban_ref;

// The Kanban card: its widgets and its rows.
typedef struct fude_zoom_kanban_form {
    fude_kit_form         card;   // The card, its title, when it opened and the screen it was laid out for (kit.h).
    rde_ui_label*         templates_label;
    rde_ui_button*        templates[FUDE_ZOOM_KANBAN_TEMPLATES];
    fude_zoom_kanban_ref  template_refs[FUDE_ZOOM_KANBAN_TEMPLATES];
    rde_ui_scroll_area*   list;
    rde_ui_text_editor*   names[FUDE_ZOOM_KANBAN_ROWS];
    rde_ui_button*        drops[FUDE_ZOOM_KANBAN_ROWS];
    fude_zoom_kanban_ref  refs[FUDE_ZOOM_KANBAN_ROWS];
    // How many rows are shown.
    u32                   rows;
    rde_ui_button*        add;
    rde_ui_label*         problem;           // says nothing was named (empty when all is well)
    rde_ui_button*        cancel;
    rde_ui_button*        save;
    fude_zoom_kanban_done done;
    any                   self;
} fude_zoom_kanban_form;

// fude_zoom_kanban_build makes the card's widgets under _root, hidden, with _font for its words.
// fude_zoom_kanban_forget clears the struct when the canvas's UI goes (the UI frees the widgets); every other call then
// does nothing.
void fude_zoom_kanban_build(fude_zoom_kanban_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font);
void fude_zoom_kanban_forget(fude_zoom_kanban_form* _form);
// Opens the card with the Kanban template's columns in its rows. Insert calls _done with _self.
void fude_zoom_kanban_open(fude_zoom_kanban_form* _form, fude_zoom_kanban_done _done, any _self);
// fude_zoom_kanban_close puts the card away without calling back; fude_zoom_kanban_shown returns whether it is up.
void fude_zoom_kanban_close(fude_zoom_kanban_form* _form);
b8   fude_zoom_kanban_shown(const fude_zoom_kanban_form* _form);
// fude_zoom_kanban_update is called once a frame and lays the card out again when the screen's size changes.
// fude_zoom_kanban_restyle gives its widgets the active theme's colours.
void fude_zoom_kanban_update(fude_zoom_kanban_form* _form);
void fude_zoom_kanban_restyle(fude_zoom_kanban_form* _form);

// Writes a template's column names into _out, in the app's language, and returns how many (3 or 5). _which is 0 Kanban,
// 1 Scrum, 2 Retrospective, 3 Week; any other gives Kanban. The names stay valid until the language changes (text.h).
u32  fude_zoom_kanban_template(u32 _which, const c8* _out[FUDE_ZOOM_KANBAN_ROWS]);

// For the look harness (drawing/app/look.h, a developer's launch flags): puts template _which in the rows, with _extra
// one more row named "Ideas", and with _insert presses Insert. It does nothing while the card is not up.
void fude_zoom_kanban_look(fude_zoom_kanban_form* _form, u32 _which, b8 _extra, b8 _insert);

#endif
