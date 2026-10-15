// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_VALUEFORM_H
#define FUDE_ZOOM_VALUEFORM_H

#include "rde.h"
#include "drawing/widgets/kit.h"

// ===========================================================================
// Valueform: a part's value asked for on a card (Sketching's Value, on the lasso's row): a resistor's resistance, a
// capacitor's capacitance, a source's volts, a motor's speed, a weight's mass.
//
// Where it sits: zoom, the canvas apps' layer, over drawing/widgets/kit.h, whose form (fude_kit_form) is its card. The
// page (page.h) owns one and opens it for the part the lasso holds.
// Its main types and their lifecycle: fude_zoom_value_form is the page's struct. fude_zoom_value_build makes its
// widgets under the UI's root, which owns and frees them; fude_zoom_value_forget clears the struct when the canvas's
// UI goes.
// Threads: main thread only; it is UI.
// Design: the card shows the part's name; the number as it is typed on the card's own keys (digits, a point, a minus,
//   back a digit), never the system keyboard, which would cover the drawing; the units it can be in, a chip each
//   (Ω kΩ MΩ, pF nF µF…), one chosen; and, where the part has them, a row of ways it can be (an LED's colour, a
//   motor's way round). OK hands the number on with the unit's and the way's index; Cancel, or a tap off the card
//   (not within 0.6 seconds of opening), hands nothing back.
// ===========================================================================

// The most unit chips and way chips the card shows; more are left off.
#define FUDE_ZOOM_VALUE_UNITS 5u
#define FUDE_ZOOM_VALUE_WAYS  5u
#define FUDE_ZOOM_VALUE_KEYS  13u   // 0 to 9, the point, the minus, back

// What OK calls, after the card is put away: _self is what the opener handed fude_zoom_value_open, _number the number
// typed (0 when nothing was), and _unit and _way the indices chosen. It is not called when the number is not finite.
typedef void (*fude_zoom_value_done)(any _self, f64 _number, u32 _unit, u32 _way);

struct fude_zoom_value_form;
// A key's or a chip's user data: the card it is on, and its index.
typedef struct {
    struct fude_zoom_value_form* form;
    u32                          index;
} fude_zoom_value_ref;

// The value card: its widgets and what is typed and chosen on it.
typedef struct fude_zoom_value_form {
    fude_kit_form        card;   // The card, its title, when it opened and the screen it was laid out for (kit.h).
    rde_ui_button*       display;
    rde_ui_button*       keys[FUDE_ZOOM_VALUE_KEYS];
    rde_ui_button*       units[FUDE_ZOOM_VALUE_UNITS];
    rde_ui_button*       ways[FUDE_ZOOM_VALUE_WAYS];
    rde_ui_button*       ok;
    rde_ui_button*       cancel;
    fude_zoom_value_ref  key_refs[FUDE_ZOOM_VALUE_KEYS];
    fude_zoom_value_ref  unit_refs[FUDE_ZOOM_VALUE_UNITS];
    fude_zoom_value_ref  way_refs[FUDE_ZOOM_VALUE_WAYS];
    // The number as typed (12 characters at most), and the units' names.
    c8                   typed[24];
    c8                   unit_names[FUDE_ZOOM_VALUE_UNITS][16];
    // How many unit and way chips there are, and which of each is chosen.
    u32                  unit_count, unit;
    u32                  way_count, way;
    b8                   fresh;        // still the number it opened with, which the first key typed replaces
    fude_zoom_value_done done;
    any                  self;
} fude_zoom_value_form;

// fude_zoom_value_build makes the card's widgets under _root, hidden, with _font for its words.
// fude_zoom_value_forget clears the struct when the canvas's UI goes (the UI frees the widgets); every other call then
// does nothing.
void fude_zoom_value_build(fude_zoom_value_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font);
void fude_zoom_value_forget(fude_zoom_value_form* _form);
// Opens the card titled _title showing _number as typed ("4.7"), with _unit_count units named in _units (_unit
// chosen) and _way_count ways named in _ways (_way chosen). A NULL or empty list leaves its row off. OK calls _done
// with _self.
void fude_zoom_value_open(fude_zoom_value_form* _form, const c8* _title, const c8* _number, const c8* const* _units, u32 _unit_count, u32 _unit,
                          const c8* const* _ways, u32 _way_count, u32 _way, fude_zoom_value_done _done, any _self);
// fude_zoom_value_close puts the card away without calling back; fude_zoom_value_shown returns whether it is up;
// fude_zoom_value_update, called once a frame, lays it out again when the screen's size changes;
// fude_zoom_value_restyle gives its widgets the active theme's colours.
void fude_zoom_value_close(fude_zoom_value_form* _form);
b8   fude_zoom_value_shown(const fude_zoom_value_form* _form);
void fude_zoom_value_update(fude_zoom_value_form* _form);
void fude_zoom_value_restyle(fude_zoom_value_form* _form);
// For the look harness (drawing/app/look.h, a developer's launch flags): with the card up, types _typed, chooses unit
// _unit and way _way (left as they are when out of range) and presses OK.
void fude_zoom_value_look(fude_zoom_value_form* _form, const c8* _typed, u32 _unit, u32 _way);

#endif
