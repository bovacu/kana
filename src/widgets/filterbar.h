#ifndef KANA_FILTERBAR
#define KANA_FILTERBAR

#include "rde.h"

// ===========================================================================
// A filter bar: across the top of a screen (Browse), rows of chips — one chosen
// in each (a filter, a sort) — and under them a search field, searching as it is
// typed, with toggles beside it (Draw, Parts, Clear).
//
// DECLARED by its screen (screen.h: bar): the chips' labels and which is on,
// what choosing one does, what the field searches, the toggles. The widget
// builds it on the UI canvas with the kit (kit.h), shows it while its screen is
// on top, lays it out for the screen's size, and keeps the chips and toggles
// showing what the screen says.
// ===========================================================================

struct kana_app;

#define KANA_FILTERBAR_CHIPS        16u   // a row's chips, at most
#define KANA_FILTERBAR_CHIP_ROWS     2u
#define KANA_FILTERBAR_TOGGLES       3u

typedef struct {
    u32         count;
    const c8* (*label)(u32 _chip);                 // each chip's label (in the language now)
    u32       (*chosen)(const void* _self);        // the chip on
    void      (*choose)(void* _self, u32 _chip);
} kana_filterbar_chips;

typedef struct {
    u32         text;                              // its label (KANA_TEXT_)
    const c8*   icon;                              // before it
    f32         icon_px;
    b8        (*on)(const void* _self);            // shown chosen while on (NULL: a plain button)
    void      (*press)(void* _self);
    b8        (*available)(const void* _self);     // NULL: always; false: greyed out
    b8          clears_search;                     // it empties the field first (Clear)
} kana_filterbar_toggle;

typedef struct kana_filterbar_def {
    kana_filterbar_chips  chips[KANA_FILTERBAR_CHIP_ROWS];
    u32                   chip_rows;
    u32                   search_hint;             // KANA_TEXT_ (after a magnifier)
    void                (*search)(void* _self, const c8* _text);   // every keystroke
    kana_filterbar_toggle toggles[KANA_FILTERBAR_TOGGLES];
    u32                   toggle_count;
} kana_filterbar_def;

struct kana_filterbar;
typedef struct {
    struct kana_filterbar* bar;
    u32                    row;     // a chip row (KANA_FILTERBAR_CHIP_ROWS: a toggle)
    u32                    index;
} kana_filterbar_ref;

typedef struct kana_filterbar {
    const kana_filterbar_def* def;
    void*                     self;            // its screen
    rde_window*               window;
    rde_ui_image*             panel;
    rde_ui_button*            chips[KANA_FILTERBAR_CHIP_ROWS][KANA_FILTERBAR_CHIPS];
    kana_filterbar_ref        chip_refs[KANA_FILTERBAR_CHIP_ROWS][KANA_FILTERBAR_CHIPS];
    rde_ui_text_editor*       field;
    rde_ui_button*            toggles[KANA_FILTERBAR_TOGGLES];
    kana_filterbar_ref        toggle_refs[KANA_FILTERBAR_TOGGLES];
    b8                        shown;
    f32                       height;          // UI units, the top safe inset included (laid out)
    rde_vec_2F                _laid_out;       // the screen size it was laid out for
    rde_vec_4I                _insets_for;
    u32                       _chosen_shown[KANA_FILTERBAR_CHIP_ROWS];
    u8                        _on_shown[KANA_FILTERBAR_TOGGLES];
} kana_filterbar;

// Built under _root (hidden) for _self, its screen. Must not move afterwards.
void kana_filterbar_create(kana_filterbar* _bar, rde_ui_node* _root, rde_window* _window, const kana_filterbar_def* _def, void* _self);
// Once a frame: shown or hidden; laid out again when the screen changes size;
// the chips and toggles as the screen says.
void kana_filterbar_update(kana_filterbar* _bar, b8 _show);
// Every widget styled again from the theme.
void kana_filterbar_restyle(kana_filterbar* _bar);
// Is _ui (UI canvas units) on the bar, while it shows?
b8   kana_filterbar_hit(const kana_filterbar* _bar, rde_vec_2F _ui);

#endif
