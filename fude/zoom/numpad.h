// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_NUMPAD
#define FUDE_ZOOM_NUMPAD

#include "rde.h"
#include "drawing/widgets/kit.h"
#include "zoom/units.h"

// ===========================================================================
// Numpad: the app's own numpad for typing a length or an angle exactly. It is never the system keyboard, which
// covers the drawing and has no fraction keys (docs/research/infinite_canvas_research.md §2.10, item 1).
//
// Where it sits: zoom, the canvas apps' layer, over drawing/widgets/kit.h and units.h. The page (page.h) owns one and
// opens it for the instruments' sizes, a compass's radius, the lasso's Offset and other exact values.
// Its main types and their lifecycle: fude_zoom_numpad is the page's struct. fude_zoom_numpad_build makes its widgets
// under the UI's root, which owns and frees them; fude_zoom_numpad_forget drops the pointers when the canvas's UI goes.
// The module also keeps a pointer to the last pad built, for its keys' callbacks, so there is one numpad at a time.
// Threads: main thread only; it is UI.
// Design:
//   A card over the page with the digits, a point, a space and a slash for fractions (1 3/8), feet and inches, the
//   four operations, and the unit a bare number means. What is typed is read as it is typed (units.h) and shown
//   worked out beneath it, or "?" when it cannot be read. OK hands the text to whoever opened it, and only when it
//   reads; Cancel, or a tap off the card, closes it without handing anything back.
// ===========================================================================

// The keys on the card, OK and Cancel included.
#define FUDE_ZOOM_NUMPAD_KEYS 23u

// What OK calls: _self is what the opener handed fude_zoom_numpad_open, _text the text as typed, _unit the unit a bare
// number means, and _value the value worked out (millimetres, or degrees for an angle).
typedef void (*fude_zoom_numpad_done)(any _self, const c8* _text, FUDE_ZOOM_UNIT_ _unit, f64 _value);

// The numpad: its widgets, and what is being typed.
typedef struct {
    fude_kit_modal        modal;
    // Where the text shows as it is typed (NULL until built, and again after forget).
    rde_ui_button*        display;
    // The line under it with the text worked out.
    rde_ui_button*        preview;
    rde_ui_button*        prompt;     // what is asked for, over the display (only shown when something is asked)
    c8                    asked[160];
    rde_ui_button*        keys[FUDE_ZOOM_NUMPAD_KEYS];
    rde_window*           window;
    // The text typed so far.
    c8                    text[64];
    // The unit a bare number means; the unit key cycles through them.
    FUDE_ZOOM_UNIT_       unit;
    // True when an angle is typed rather than a length.
    b8                    angle;
    // The canvas's variables a typed value may use (the opener's; may be NULL).
    const fude_zoom_vars* vars;
    fude_zoom_numpad_done done;
    any                   self;
    // The text worked out, and whether it could be read.
    f64                   value;
    b8                    valid;
    // The screen size it was last laid out for, so it is placed again only when that changes.
    rde_vec_2F            laid_out;
} fude_zoom_numpad;

// fude_zoom_numpad_build makes the numpad's widgets under _root, hidden. fude_zoom_numpad_forget drops the pointers to
// them when the canvas's UI goes (the UI frees the widgets themselves); every other call then does nothing.
void fude_zoom_numpad_build(fude_zoom_numpad* _pad, rde_ui_node* _root, rde_window* _window);
void fude_zoom_numpad_forget(fude_zoom_numpad* _pad);
// Opens the numpad with _text already in it (NULL: empty), for a length (a bare number taken in _unit) or, with
// _angle, an angle. _vars may be NULL. OK calls _done with _self. Does nothing before fude_zoom_numpad_build.
void fude_zoom_numpad_open(fude_zoom_numpad* _pad, const c8* _text, FUDE_ZOOM_UNIT_ _unit, b8 _angle, const fude_zoom_vars* _vars,
                           fude_zoom_numpad_done _done, any _self);
// Shows what it asks for on a short line over the display, making the card taller. Call it after opening; an empty
// or NULL _asked hides the line.
void fude_zoom_numpad_ask(fude_zoom_numpad* _pad, const c8* _asked);
// fude_zoom_numpad_close hides it without calling back; fude_zoom_numpad_shown returns whether it is open.
void fude_zoom_numpad_close(fude_zoom_numpad* _pad);
b8   fude_zoom_numpad_shown(const fude_zoom_numpad* _pad);
// fude_zoom_numpad_update is called once a frame and places the card again when the screen's size changes.
// fude_zoom_numpad_restyle gives its widgets the active theme's colours.
void fude_zoom_numpad_update(fude_zoom_numpad* _pad);
void fude_zoom_numpad_restyle(fude_zoom_numpad* _pad);

#endif
