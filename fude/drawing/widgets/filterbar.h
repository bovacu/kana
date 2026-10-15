// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_FILTERBAR
#define FUDE_FILTERBAR

#include "rde.h"

// ===========================================================================
// Filter bar: a bar across the top of a screen (Browse) with rows of chips, one chosen in each row (a filter, a
// sort), and under them a search field that searches as it is typed, with toggles beside it (Draw, Parts, Clear). On
// a phone (kit.h: compact) each row of chips is one line that scrolls sideways, and the toggles show their icons alone.
//
// Where it sits: drawing/widgets, built with the kit (kit.h). A screen declares its bar (screen.h: bar), and the UI
// (ui.c) builds one for each screen that has one.
//
// Its main types and their lifecycle: the screen DECLARES the bar as data (fude_filterbar_def: the chips' labels and
// which is on, what choosing one does, what the field searches, the toggles), usually a static constant. The UI holds
// the widget (fude_filterbar), builds it on the UI canvas with fude_filterbar_create, shows it while its screen is on
// top, lays it out for the screen's size, and keeps the chips and toggles showing what the screen says. Its nodes
// belong to the UI tree, so it has no destroy function.
//
// Threads: main thread only, like all of RDE's UI. The declaration's callbacks are called there too.
// ===========================================================================

struct fude_app;

#define FUDE_FILTERBAR_CHIPS        16u   // The most chips in a row; more are left out.
#define FUDE_FILTERBAR_CHIP_ROWS     2u   // The most rows of chips.
#define FUDE_FILTERBAR_TOGGLES       3u   // The most toggles beside the field.

// One row of chips, as declared. Only one chip of a row is chosen at a time.
typedef struct {
    u32         count;                             // How many chips (0 to ask count_of).
    u32       (*count_of)(void);                   // How many, when known only at run time (a language's levels).
    const c8* (*label)(u32 _chip);                 // Each chip's label, in the current language.
    u32       (*chosen)(const any _self);        // The chip that is on now.
    void      (*choose)(any _self, u32 _chip);     // Called when a chip is pressed.
} fude_filterbar_chips;

// One toggle beside the search field, as declared.
typedef struct {
    u32         text;                              // Its label (a FUDE_TEXT_ id).
    const c8*   icon;                              // Shown before the label.
    f32         icon_px;
    b8        (*on)(const any _self);            // It shows chosen while this says so (NULL: a plain button).
    void      (*press)(any _self);
    // NULL means always available; false greys it out. It is asked each frame the bar shows, so the toggle follows
    // what it depends on (data that loads later, a mode that changes).
    b8        (*available)(const any _self);
    b8          clears_search;                     // A press empties the field first (Clear).
    // When unavailable it is not shown at all (Parts, in an app whose script has none). This is asked each frame.
    b8          hide_unavailable;
} fude_filterbar_toggle;

// A screen's filter bar, as declared. _self in the callbacks is the screen.
typedef struct fude_filterbar_def {
    fude_filterbar_chips  chips[FUDE_FILTERBAR_CHIP_ROWS];
    u32                   chip_rows;
    u32                   search_hint;             // A FUDE_TEXT_ id, shown in the empty field after a magnifier.
    void                (*search)(any _self, const c8* _text);   // Called on every keystroke.
    fude_filterbar_toggle toggles[FUDE_FILTERBAR_TOGGLES];
    u32                   toggle_count;
} fude_filterbar_def;

struct fude_filterbar;
// What a chip's or a toggle's press callback gets: its bar, and which one it is.
typedef struct {
    struct fude_filterbar* bar;
    u32                    row;     // A chip row, or FUDE_FILTERBAR_CHIP_ROWS for a toggle.
    u32                    index;
} fude_filterbar_ref;

// A filter bar built on the UI canvas. The fields are the bar's own; its owner only reads them.
typedef struct fude_filterbar {
    const fude_filterbar_def* def;
    any                       self;            // Its screen.
    rde_window*               window;
    rde_ui_image*             panel;
    // Each row's chips, in a scroll area (on a phone, one line that scrolls).
    rde_ui_scroll_area*       lines[FUDE_FILTERBAR_CHIP_ROWS];
    rde_ui_button*            chips[FUDE_FILTERBAR_CHIP_ROWS][FUDE_FILTERBAR_CHIPS];
    fude_filterbar_ref        chip_refs[FUDE_FILTERBAR_CHIP_ROWS][FUDE_FILTERBAR_CHIPS];
    rde_ui_text_editor*       field;
    rde_ui_button*            toggles[FUDE_FILTERBAR_TOGGLES];
    fude_filterbar_ref        toggle_refs[FUDE_FILTERBAR_TOGGLES];
    b8                        shown;
    f32                       height;          // In UI units, the top safe inset included (set by the layout).
    rde_vec_2F                _laid_out;       // The screen size it was laid out for.
    rde_vec_4I                _insets_for;     // The safe-area insets it was laid out for.
    i8                        _compact_for;    // Laid out compact (1) or not (0); -1 before the first layout.
    u32                       _chosen_shown[FUDE_FILTERBAR_CHIP_ROWS];   // Each row's chosen chip, as last shown.
    // How many chips each row was made with. Layout and refresh use it rather than asking count_of again, which could
    // answer more than there are buttons for; the bar is made again when the UI is (a language change).
    u32                       _chips_made[FUDE_FILTERBAR_CHIP_ROWS];
    u8                        _on_shown[FUDE_FILTERBAR_TOGGLES];         // Each toggle's on state, as last shown.
    u8                        _available_shown[FUDE_FILTERBAR_TOGGLES];  // Each toggle's availability, as last shown.
    u8                        _hidden[FUDE_FILTERBAR_TOGGLES];   // Laid out hidden (hide_unavailable).
} fude_filterbar;

// Builds the bar under _root, hidden, for _self, its screen. _bar must not move afterwards, because its buttons point
// back at it.
void fude_filterbar_create(fude_filterbar* _bar, rde_ui_node* _root, rde_window* _window, const fude_filterbar_def* _def, any _self);
// Called once a frame: shows or hides the bar, lays it out again when the screen's size or safe area changes (or a
// hidden toggle comes or goes), and shows the chips and toggles as the screen says.
void fude_filterbar_update(fude_filterbar* _bar, b8 _show);
// Styles every widget again from the theme.
void fude_filterbar_restyle(fude_filterbar* _bar);
// Whether the point _ui (UI canvas units) is on the bar while it shows: anywhere above its bottom edge.
b8   fude_filterbar_hit(const fude_filterbar* _bar, rde_vec_2F _ui);

#endif
