// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_FILTERBAR
#define FUDE_FILTERBAR

#include "rde.h"

// ===========================================================================
// A filter bar: across the top of a screen (Browse), rows of chips — one chosen
// in each (a filter, a sort) — and under them a search field, searching as it is
// typed, with toggles beside it (Draw, Parts, Clear). On a phone (kit.h:
// compact) each row of chips is one line that scrolls sideways, and the toggles
// are their icons alone.
//
// DECLARED by its screen (screen.h: bar): the chips' labels and which is on,
// what choosing one does, what the field searches, the toggles. The widget
// builds it on the UI canvas with the kit (kit.h), shows it while its screen is
// on top, lays it out for the screen's size, and keeps the chips and toggles
// showing what the screen says.
// ===========================================================================

struct fude_app;

#define FUDE_FILTERBAR_CHIPS        16u   // a row's chips, at most
#define FUDE_FILTERBAR_CHIP_ROWS     2u
#define FUDE_FILTERBAR_TOGGLES       3u

typedef struct {
    u32         count;                             // how many (0: count_of's)
    u32       (*count_of)(void);                   // how many, known only at run time (the language's levels)
    const c8* (*label)(u32 _chip);                 // each chip's label (in the language now)
    u32       (*chosen)(const void* _self);        // the chip on
    void      (*choose)(void* _self, u32 _chip);
} fude_filterbar_chips;

typedef struct {
    u32         text;                              // its label (FUDE_TEXT_)
    const c8*   icon;                              // before it
    f32         icon_px;
    b8        (*on)(const void* _self);            // shown chosen while on (NULL: a plain button)
    void      (*press)(void* _self);
    b8        (*available)(const void* _self);     // NULL: always; false: greyed out
    b8          clears_search;                     // it empties the field first (Clear)
    b8          hide_unavailable;                  // unavailable, not shown at all (Parts, in an app whose script has none)
} fude_filterbar_toggle;

typedef struct fude_filterbar_def {
    fude_filterbar_chips  chips[FUDE_FILTERBAR_CHIP_ROWS];
    u32                   chip_rows;
    u32                   search_hint;             // FUDE_TEXT_ (after a magnifier)
    void                (*search)(void* _self, const c8* _text);   // every keystroke
    fude_filterbar_toggle toggles[FUDE_FILTERBAR_TOGGLES];
    u32                   toggle_count;
} fude_filterbar_def;

struct fude_filterbar;
typedef struct {
    struct fude_filterbar* bar;
    u32                    row;     // a chip row (FUDE_FILTERBAR_CHIP_ROWS: a toggle)
    u32                    index;
} fude_filterbar_ref;

typedef struct fude_filterbar {
    const fude_filterbar_def* def;
    void*                     self;            // its screen
    rde_window*               window;
    rde_ui_image*             panel;
    rde_ui_scroll_area*       lines[FUDE_FILTERBAR_CHIP_ROWS];   // each row's chips (a phone's: one line, scrolled)
    rde_ui_button*            chips[FUDE_FILTERBAR_CHIP_ROWS][FUDE_FILTERBAR_CHIPS];
    fude_filterbar_ref        chip_refs[FUDE_FILTERBAR_CHIP_ROWS][FUDE_FILTERBAR_CHIPS];
    rde_ui_text_editor*       field;
    rde_ui_button*            toggles[FUDE_FILTERBAR_TOGGLES];
    fude_filterbar_ref        toggle_refs[FUDE_FILTERBAR_TOGGLES];
    b8                        shown;
    f32                       height;          // UI units, the top safe inset included (laid out)
    rde_vec_2F                _laid_out;       // the screen size it was laid out for
    rde_vec_4I                _insets_for;
    i8                        _compact_for;    // laid out compact (1) or not (0); -1: not yet
    u32                       _chosen_shown[FUDE_FILTERBAR_CHIP_ROWS];
    u8                        _on_shown[FUDE_FILTERBAR_TOGGLES];
    u8                        _hidden[FUDE_FILTERBAR_TOGGLES];   // laid out hidden (hide_unavailable)
} fude_filterbar;

// Built under _root (hidden) for _self, its screen. Must not move afterwards.
void fude_filterbar_create(fude_filterbar* _bar, rde_ui_node* _root, rde_window* _window, const fude_filterbar_def* _def, void* _self);
// Once a frame: shown or hidden; laid out again when the screen changes size;
// the chips and toggles as the screen says.
void fude_filterbar_update(fude_filterbar* _bar, b8 _show);
// Every widget styled again from the theme.
void fude_filterbar_restyle(fude_filterbar* _bar);
// Is _ui (UI canvas units) on the bar, while it shows?
b8   fude_filterbar_hit(const fude_filterbar* _bar, rde_vec_2F _ui);

#endif
