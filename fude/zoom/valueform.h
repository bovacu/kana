// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_VALUEFORM_H
#define FUDE_ZOOM_VALUEFORM_H

#include "rde.h"
#include "drawing/widgets/kit.h"

// ===========================================================================
// A part's value asked on a card (Sketching's Value, on the lasso's row: a
// resistor's, a capacitor's, a source's volts, a motor's speed, a weight's
// mass): its name; the number as it is typed on our own keys (digits, a point,
// a minus, back a digit) — never the system keyboard; the units it can be in,
// a chip each (Ω kΩ MΩ, pF nF µF…), one chosen; and, where the part has them,
// a row of ways it can be (an LED's colour, a motor's way round). OK hands the
// number on with the unit's and the way's index; Cancel, or a tap off the
// card, nothing.
// ===========================================================================

#define FUDE_ZOOM_VALUE_UNITS 5u
#define FUDE_ZOOM_VALUE_WAYS  5u
#define FUDE_ZOOM_VALUE_KEYS  13u   // 0–9, the point, the minus, back

typedef void (*fude_zoom_value_done)(void* _self, f64 _number, u32 _unit, u32 _way);

struct fude_zoom_value_form;
typedef struct {
    struct fude_zoom_value_form* form;
    u32                          index;
} fude_zoom_value_ref;

typedef struct fude_zoom_value_form {
    fude_kit_modal       modal;
    rde_window*          window;
    rde_font*            font;
    rde_ui_label*        title;
    rde_ui_button*       display;
    rde_ui_button*       keys[FUDE_ZOOM_VALUE_KEYS];
    rde_ui_button*       units[FUDE_ZOOM_VALUE_UNITS];
    rde_ui_button*       ways[FUDE_ZOOM_VALUE_WAYS];
    rde_ui_button*       ok;
    rde_ui_button*       cancel;
    fude_zoom_value_ref  key_refs[FUDE_ZOOM_VALUE_KEYS];
    fude_zoom_value_ref  unit_refs[FUDE_ZOOM_VALUE_UNITS];
    fude_zoom_value_ref  way_refs[FUDE_ZOOM_VALUE_WAYS];
    c8                   typed[24];
    c8                   unit_names[FUDE_ZOOM_VALUE_UNITS][16];
    u32                  unit_count, unit;
    u32                  way_count, way;
    b8                   fresh;        // what it opened with: the first key typed replaces it
    fude_zoom_value_done done;
    void*                self;
    f64                  opened_at;
    rde_vec_2F           laid_out;
} fude_zoom_value_form;

void fude_zoom_value_build(fude_zoom_value_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font);
void fude_zoom_value_forget(fude_zoom_value_form* _form);
// Up with _title, _number shown (as typed: "4.7"), _units' names (_unit_count; _unit chosen) and _ways' (NULL or
// none: no row; _way chosen); _done called on OK.
void fude_zoom_value_open(fude_zoom_value_form* _form, const c8* _title, const c8* _number, const c8* const* _units, u32 _unit_count, u32 _unit,
                          const c8* const* _ways, u32 _way_count, u32 _way, fude_zoom_value_done _done, void* _self);
void fude_zoom_value_close(fude_zoom_value_form* _form);
b8   fude_zoom_value_shown(const fude_zoom_value_form* _form);
void fude_zoom_value_update(fude_zoom_value_form* _form);
void fude_zoom_value_restyle(fude_zoom_value_form* _form);
// A developer's look: _typed typed, unit _unit and way _way chosen, OK pressed.
void fude_zoom_value_look(fude_zoom_value_form* _form, const c8* _typed, u32 _unit, u32 _way);

#endif
