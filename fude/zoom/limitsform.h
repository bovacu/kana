// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_LIMITSFORM_H
#define FUDE_ZOOM_LIMITSFORM_H

#include "rde.h"
#include "drawing/widgets/kit.h"
#include "zoom/limits.h"

// ===========================================================================
// A part's LIMITS asked on a card (Sketching's Limits, on the lasso's row: a
// circuit's part alone — limits.h): the real parts it can be, a chip each
// (choosing one sets them all); each limit it has, a row (its power, its
// current, its voltage, the wrong way round, or its gate's) — tapped, typed
// on our own keys in its units (W or mW, A or mA, V); 0, no such limit.
// Typical puts back what its text, or its kind, says. Apply hands them on
// with the real part's index (-1: its own); Cancel, or a tap off the card,
// nothing.
// ===========================================================================

#define FUDE_ZOOM_LIMITS_PRESETS 6u
#define FUDE_ZOOM_LIMITS_KEYS    12u   // 0–9, the point, back

typedef void (*fude_zoom_limits_done)(void* _self, const f64* _most, i32 _preset);

struct fude_zoom_limits_form;
typedef struct {
    struct fude_zoom_limits_form* form;
    u32                           index;
} fude_zoom_limits_ref;

typedef struct fude_zoom_limits_form {
    fude_kit_modal        modal;
    rde_window*           window;
    rde_font*             font;
    rde_ui_label*         title;
    rde_ui_label*         how;
    rde_ui_button*        presets[FUDE_ZOOM_LIMITS_PRESETS];
    rde_ui_button*        fields[FUDE_ZOOM_LIMIT_COUNT];
    rde_ui_button*        keys[FUDE_ZOOM_LIMITS_KEYS];
    rde_ui_button*        units[2];
    rde_ui_button*        restore;          // Typical
    rde_ui_button*        apply;
    rde_ui_button*        cancel;
    fude_zoom_limits_ref  preset_refs[FUDE_ZOOM_LIMITS_PRESETS];
    fude_zoom_limits_ref  field_refs[FUDE_ZOOM_LIMIT_COUNT];
    fude_zoom_limits_ref  key_refs[FUDE_ZOOM_LIMITS_KEYS];
    fude_zoom_limits_ref  unit_refs[2];
    const fude_zoom_limits_preset* table;   // the part's real parts (limits.h)
    u32                   table_count;
    u32                   kinds;            // its limits (a bit each)
    b8                    gate;             // its REVERSE its gate's
    f64                   most[FUDE_ZOOM_LIMIT_COUNT];
    f64                   typical[FUDE_ZOOM_LIMIT_COUNT];
    i32                   preset, typical_preset;
    u32                   field;            // the one being typed (FUDE_ZOOM_LIMIT_)
    u32                   unit;             // its unit: 0 the whole one (W, A, V), 1 a thousandth (mW, mA)
    c8                    typed[16];
    b8                    fresh;            // the field as it was: the first key typed replaces it
    fude_zoom_limits_done done;
    void*                 self;
    f64                   opened_at;
    rde_vec_2F            laid_out;
} fude_zoom_limits_form;

void fude_zoom_limits_build(fude_zoom_limits_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font);
void fude_zoom_limits_forget(fude_zoom_limits_form* _form);
// Up for part _part named _name: its limits now (_most; _preset the real part they are, -1: its own) and its typical ones
// (_typical, _typical_preset: as its text says); _done on Apply.
void fude_zoom_limits_open(fude_zoom_limits_form* _form, const fude_zoom_part* _part, const c8* _name, const f64* _most, i32 _preset,
                           const f64* _typical, i32 _typical_preset, fude_zoom_limits_done _done, void* _self);
void fude_zoom_limits_close(fude_zoom_limits_form* _form);
b8   fude_zoom_limits_shown(const fude_zoom_limits_form* _form);
void fude_zoom_limits_update(fude_zoom_limits_form* _form);
void fude_zoom_limits_restyle(fude_zoom_limits_form* _form);
// A developer's look: real part _preset chosen (-1: none), then field _field typed _typed (NULL: none) in unit _unit,
// then (_finish) 0 left up, 1 Apply, 2 Typical and Apply.
void fude_zoom_limits_look(fude_zoom_limits_form* _form, i32 _preset, u32 _field, const c8* _typed, u32 _unit, u32 _finish);

#endif
