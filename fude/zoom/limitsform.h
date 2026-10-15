// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_LIMITSFORM_H
#define FUDE_ZOOM_LIMITSFORM_H

#include "rde.h"
#include "drawing/widgets/kit.h"
#include "zoom/limits.h"

// ===========================================================================
// Limitsform: a circuit part's limits (limits.h) asked for on a card (Sketching's Limits, on the lasso's row when it
// holds one circuit part alone).
//
// Where it sits: zoom, the canvas apps' layer, over drawing/widgets/kit.h and limits.h. The page (page.h) owns one and
// opens it for the part the lasso holds.
// Its card is drawing/widgets/kit.h's form (fude_kit_form): the card, its title, Cancel, a tap off it and laying it
// out again are the kit's; the rest is this card's own.
// Its main types and their lifecycle: fude_zoom_limits_form is the page's struct. fude_zoom_limits_build makes its
// widgets under the UI's root, which owns and frees them; fude_zoom_limits_forget clears the struct when the canvas's
// UI goes.
// Threads: main thread only; it is UI.
// Design: the card shows the real parts the part can be, a chip each (choosing one sets all its limits), and a row for
//   each limit it has (its power, its current, its voltage, the voltage the wrong way round, or its gate's). A row
//   tapped is typed on the card's own keys, never the system keyboard, in its units (W or mW, A or mA, V); 0 means no
//   such limit. Typical puts back what its text, or its kind, says. Apply hands the limits on with the real part's
//   index (-1: limits of its own); Cancel, or a tap off the card (not within 0.6 seconds of opening), hands nothing
//   back. See docs/infinite_canvas_design.md §47.
// ===========================================================================

// The most real parts' chips the card shows.
#define FUDE_ZOOM_LIMITS_PRESETS 6u
#define FUDE_ZOOM_LIMITS_KEYS    12u   // 0 to 9, the point, back

// What Apply calls, after the card is put away: _self is what the opener handed fude_zoom_limits_open, _most the
// FUDE_ZOOM_LIMIT_COUNT limits in whole units (W, A, V; 0 for none), valid only during the call, and _preset the real
// part they are (-1: the part's own).
typedef void (*fude_zoom_limits_done)(any _self, const f64* _most, i32 _preset);

struct fude_zoom_limits_form;
// A chip's, a row's or a key's user data: the card it is on, and its index.
typedef struct {
    struct fude_zoom_limits_form* form;
    u32                           index;
} fude_zoom_limits_ref;

// The limits card: its widgets and the limits being edited.
typedef struct fude_zoom_limits_form {
    fude_kit_form         card;   // The card, its title, when it opened and the screen it was laid out for (kit.h).
    // A line saying how the card works, under the title.
    rde_ui_label*         how;
    rde_ui_button*        presets[FUDE_ZOOM_LIMITS_PRESETS];
    rde_ui_button*        fields[FUDE_ZOOM_LIMIT_COUNT];
    rde_ui_button*        keys[FUDE_ZOOM_LIMITS_KEYS];
    rde_ui_button*        units[2];
    rde_ui_button*        restore;          // the Typical button
    rde_ui_button*        apply;
    rde_ui_button*        cancel;
    fude_zoom_limits_ref  preset_refs[FUDE_ZOOM_LIMITS_PRESETS];
    fude_zoom_limits_ref  field_refs[FUDE_ZOOM_LIMIT_COUNT];
    fude_zoom_limits_ref  key_refs[FUDE_ZOOM_LIMITS_KEYS];
    fude_zoom_limits_ref  unit_refs[2];
    const fude_zoom_limits_preset* table;   // the part's real parts (limits.h)
    u32                   table_count;
    u32                   kinds;            // the limits it has, one bit each
    b8                    gate;             // its REVERSE limit is its gate's
    // Its limits as edited, and as its text or kind says (for Typical); whole units, 0 for none.
    f64                   most[FUDE_ZOOM_LIMIT_COUNT];
    f64                   typical[FUDE_ZOOM_LIMIT_COUNT];
    // The real part each of those is (-1: its own).
    i32                   preset, typical_preset;
    u32                   field;            // the limit being typed (FUDE_ZOOM_LIMIT_)
    u32                   unit;             // its unit: 0 the whole one (W, A, V), 1 a thousandth (mW, mA)
    // The number as typed (9 characters at most).
    c8                    typed[16];
    b8                    fresh;            // the field still shows its old value, which the first key typed replaces
    fude_zoom_limits_done done;
    any                   self;
} fude_zoom_limits_form;

// fude_zoom_limits_build makes the card's widgets under _root, hidden, with _font for its words.
// fude_zoom_limits_forget clears the struct when the canvas's UI goes (the UI frees the widgets); every other call then
// does nothing.
void fude_zoom_limits_build(fude_zoom_limits_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font);
void fude_zoom_limits_forget(fude_zoom_limits_form* _form);
// Opens the card for part _part, titled with _name: its limits now (_most, FUDE_ZOOM_LIMIT_COUNT of them; _preset the
// real part they are, -1 its own) and its typical ones (_typical and _typical_preset, as its text says), both copied.
// Its first limit is ready to type. Apply calls _done with _self. Does nothing for a NULL part.
void fude_zoom_limits_open(fude_zoom_limits_form* _form, const fude_zoom_part* _part, const c8* _name, const f64* _most, i32 _preset,
                           const f64* _typical, i32 _typical_preset, fude_zoom_limits_done _done, any _self);
// fude_zoom_limits_close puts the card away without calling back; fude_zoom_limits_shown returns whether it is up;
// fude_zoom_limits_update, called once a frame, lays it out again when the screen's size changes;
// fude_zoom_limits_restyle gives its widgets the active theme's colours.
void fude_zoom_limits_close(fude_zoom_limits_form* _form);
b8   fude_zoom_limits_shown(const fude_zoom_limits_form* _form);
void fude_zoom_limits_update(fude_zoom_limits_form* _form);
void fude_zoom_limits_restyle(fude_zoom_limits_form* _form);
// For the look harness (drawing/app/look.h, a developer's launch flags): with the card up, chooses real part _preset
// (-1: none), then types _typed (NULL: nothing) into limit _field in unit _unit, then finishes as _finish says: 0
// leaves the card up, 1 presses Apply, 2 presses Typical and then Apply.
void fude_zoom_limits_look(fude_zoom_limits_form* _form, i32 _preset, u32 _field, const c8* _typed, u32 _unit, u32 _finish);

#endif
