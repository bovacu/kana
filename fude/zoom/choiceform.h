// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_CHOICEFORM_H
#define FUDE_ZOOM_CHOICEFORM_H

#include "rde.h"
#include "drawing/widgets/kit.h"

// ===========================================================================
// Choiceform: a choice asked for on a small card in the middle of the screen: its title, up to FUDE_ZOOM_CHOICE_MAX
// buttons (each a line, with an icon before it), and Cancel.
//
// Where it sits: zoom, the canvas apps' layer, over drawing/widgets/kit.h, whose form (fude_kit_form) is its card. The
// page (page.h) owns one and uses it for the Examples, a custom part's menu, Combine, Constrain and
// other short choices.
// Its main types and their lifecycle: fude_zoom_choice_form is the page's struct. fude_zoom_choice_build makes its
// widgets under the UI's root, which owns and frees them; fude_zoom_choice_forget clears the struct when the canvas's
// UI goes.
// Threads: main thread only; it is UI.
// Design: a button pressed puts the card away and hands its index on; Cancel, or a tap off the card, hands nothing
//   back. A tap off the card within 0.6 seconds of opening is ignored, so the release of the press that opened it does
//   not close it again.
// ===========================================================================

// The most choices the card shows; more are left off.
#define FUDE_ZOOM_CHOICE_MAX 6u

// What a choice calls, after the card is put away: _self is what the opener handed fude_zoom_choice_open, and _index
// the choice pressed, counting from 0.
typedef void (*fude_zoom_choice_done)(any _self, u32 _index);

struct fude_zoom_choice_form;
// A choice button's user data: the card it is on, and its index.
typedef struct {
    struct fude_zoom_choice_form* form;
    u32                           index;
} fude_zoom_choice_ref;

// The choice card: its widgets and who asked.
typedef struct fude_zoom_choice_form {
    fude_kit_form         card;   // The card, its title, when it opened and the screen it was laid out for (kit.h).
    rde_ui_button*        choices[FUDE_ZOOM_CHOICE_MAX];
    rde_ui_button*        cancel;
    fude_zoom_choice_ref  refs[FUDE_ZOOM_CHOICE_MAX];
    // How many choices are shown.
    u32                   count;
    fude_zoom_choice_done done;
    any                   self;
} fude_zoom_choice_form;

// fude_zoom_choice_build makes the card's widgets under _root, hidden, with _font for its words.
// fude_zoom_choice_forget clears the struct when the canvas's UI goes (the UI frees the widgets); every other call then
// does nothing.
void fude_zoom_choice_build(fude_zoom_choice_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font);
void fude_zoom_choice_forget(fude_zoom_choice_form* _form);
// Opens the card titled _title with _count choices (at most FUDE_ZOOM_CHOICE_MAX), labelled _labels, with the icons
// _icons before them (a NULL list or entry: no icon is set). Pressing one calls _done with _self and its index.
void fude_zoom_choice_open(fude_zoom_choice_form* _form, const c8* _title, const c8* const* _labels, const c8* const* _icons, u32 _count,
                           fude_zoom_choice_done _done, any _self);
// fude_zoom_choice_close puts the card away without calling back; fude_zoom_choice_shown returns whether it is up;
// fude_zoom_choice_update, called once a frame, lays it out again when the screen's size changes;
// fude_zoom_choice_restyle gives its widgets the active theme's colours.
void fude_zoom_choice_close(fude_zoom_choice_form* _form);
b8   fude_zoom_choice_shown(const fude_zoom_choice_form* _form);
void fude_zoom_choice_update(fude_zoom_choice_form* _form);
void fude_zoom_choice_restyle(fude_zoom_choice_form* _form);
// For the look harness (drawing/app/look.h, a developer's launch flags): presses choice _index, when the card is up
// and has it.
void fude_zoom_choice_look(fude_zoom_choice_form* _form, u32 _index);

#endif
