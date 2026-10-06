// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_NUMPAD
#define FUDE_ZOOM_NUMPAD

#include "rde.h"
#include "drawing/widgets/kit.h"
#include "zoom/units.h"

// ===========================================================================
// Our own numpad (research §2.10, item 1): never the system keyboard, which
// covers the drawing and has no fraction keys. A card over the page with the
// digits, a point, a space and a slash for fractions (1 3/8), feet and inches,
// the four operations, and the unit a bare number means; what is typed is read
// as it is typed (units.h) and shown worked out beneath it, or what is wrong.
// OK hands the text to whoever opened it; Cancel, or a tap off the card, does not.
// ===========================================================================

#define FUDE_ZOOM_NUMPAD_KEYS 23u

// What OK hands back: the text as typed, the unit a bare number means, and
// the value worked out (millimetres, or degrees for an angle's).
typedef void (*fude_zoom_numpad_done)(void* _self, const c8* _text, FUDE_ZOOM_UNIT_ _unit, f64 _value);

typedef struct {
    fude_kit_modal        modal;
    rde_ui_button*        display;
    rde_ui_button*        preview;
    rde_ui_button*        prompt;     // what is asked for, over the display (only when asked)
    c8                    asked[160];
    rde_ui_button*        keys[FUDE_ZOOM_NUMPAD_KEYS];
    rde_window*           window;
    c8                    text[64];
    FUDE_ZOOM_UNIT_       unit;
    b8                    angle;
    const fude_zoom_vars* vars;
    fude_zoom_numpad_done done;
    void*                 self;
    f64                   value;
    b8                    valid;
    rde_vec_2F            laid_out;
} fude_zoom_numpad;

// Its widgets under _root, hidden; and let go (the canvas going).
void fude_zoom_numpad_build(fude_zoom_numpad* _pad, rde_ui_node* _root, rde_window* _window);
void fude_zoom_numpad_forget(fude_zoom_numpad* _pad);
// Opened with _text already in it: a length (a bare number in _unit) or an angle.
void fude_zoom_numpad_open(fude_zoom_numpad* _pad, const c8* _text, FUDE_ZOOM_UNIT_ _unit, b8 _angle, const fude_zoom_vars* _vars,
                           fude_zoom_numpad_done _done, void* _self);
// What it asks for, on a line over the display (after opening it; a short line).
void fude_zoom_numpad_ask(fude_zoom_numpad* _pad, const c8* _asked);
void fude_zoom_numpad_close(fude_zoom_numpad* _pad);
b8   fude_zoom_numpad_shown(const fude_zoom_numpad* _pad);
// Once a frame: placed again when the screen changes.
void fude_zoom_numpad_update(fude_zoom_numpad* _pad);
void fude_zoom_numpad_restyle(fude_zoom_numpad* _pad);

#endif
