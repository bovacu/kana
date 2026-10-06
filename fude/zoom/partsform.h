// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PARTSFORM_H
#define FUDE_ZOOM_PARTSFORM_H

#include "rde.h"
#include "drawing/widgets/kit.h"
#include "zoom/units.h"

// ===========================================================================
// The parts to cut from a board (the selection's Fit parts): a card high on
// the screen, over where the keyboard comes —
//
//   its title (the board's name)
//   the units the sizes are in: mm, cm, in, ft · in (the page's to begin with)
//   a list that scrolls, a row a part: its width × its height, and a trash
//   Add part, under the list
//   the saw's kerf, as it is now (what is left between the parts)
//   Cancel, Cut
//
// A size is read as the numpad reads one (units.h: "600", "60 cm", "23 5/8",
// "1' 11 5/8\"", a comma or a point for the decimals); a row left empty is
// skipped, one that cannot be read keeps the card up and says which.
// ===========================================================================

#define FUDE_ZOOM_PARTS_ROWS 40u

// What Cut hands back: each part's two sizes (millimetres, as typed: across, then up).
typedef struct {
    f64 width, height;
} fude_zoom_parts_size;
typedef void (*fude_zoom_parts_done)(void* _self, const fude_zoom_parts_size* _parts, u32 _n);

struct fude_zoom_parts_form;
typedef struct {
    struct fude_zoom_parts_form* form;
    u32                          index;
} fude_zoom_parts_ref;

typedef struct fude_zoom_parts_form {
    fude_kit_modal       modal;
    rde_window*          window;
    rde_font*            font;
    rde_ui_label*        title;
    rde_ui_label*        units_label;
    rde_ui_button*       units[4];
    u8                   unit_choice;       // 0 mm, 1 cm, 2 in, 3 ft · in (the page's choices)
    rde_ui_scroll_area*  list;
    rde_ui_text_editor*  widths[FUDE_ZOOM_PARTS_ROWS];
    rde_ui_text_editor*  heights[FUDE_ZOOM_PARTS_ROWS];
    rde_ui_label*        times[FUDE_ZOOM_PARTS_ROWS];
    rde_ui_button*       drops[FUDE_ZOOM_PARTS_ROWS];
    fude_zoom_parts_ref  refs[FUDE_ZOOM_PARTS_ROWS];
    u32                  rows;
    rde_ui_button*       add;
    rde_ui_label*        kerf;
    rde_ui_label*        problem;           // a row that could not be read (empty: none)
    rde_ui_button*       cancel;
    rde_ui_button*       save;
    fude_zoom_parts_done done;
    void*                self;
    f64                  opened_at;
    rde_vec_2F           laid_out;
} fude_zoom_parts_form;

// Its widgets under _root, hidden; and let go (the canvas going).
void fude_zoom_parts_build(fude_zoom_parts_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font);
void fude_zoom_parts_forget(fude_zoom_parts_form* _form);
// Up, one empty row in it: _title over it, _kerf under the list ("Saw kerf: 3 mm"),
// the sizes in _unit_choice to begin with. Cut calls _done (the card put away first).
void fude_zoom_parts_open(fude_zoom_parts_form* _form, const c8* _title, const c8* _kerf, u8 _unit_choice, fude_zoom_parts_done _done, void* _self);
void fude_zoom_parts_close(fude_zoom_parts_form* _form);
b8   fude_zoom_parts_shown(const fude_zoom_parts_form* _form);
// Once a frame: laid out again when the screen changes.
void fude_zoom_parts_update(fude_zoom_parts_form* _form);
void fude_zoom_parts_restyle(fude_zoom_parts_form* _form);

// A developer's look: rows filled from _rows ("600 x 300; 200 x 100": a row each), then (_cut) Cut pressed.
void fude_zoom_parts_look(fude_zoom_parts_form* _form, const c8* _rows, b8 _cut);

// A row's text read as a length in _unit_choice's unit (millimetres). False: not one.
b8   fude_zoom_parts_read(const c8* _text, u8 _unit_choice, f64* _mm);

#endif
