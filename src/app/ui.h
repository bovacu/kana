#ifndef KANA_UI
#define KANA_UI

#include "rde.h"
#include "app/app.h"
#include "widgets/row.h"
#include "widgets/toolbar.h"
#include "widgets/pagemenu.h"
#include "widgets/filterbar.h"
#include "widgets/side.h"
#include "widgets/wordcard.h"

// ===========================================================================
// Kana's retained UI: one RDE UI canvas over everything, and every widget on it —
//
//   the floating toolbar and its panels (toolbar.h), over the page;
//   the page's menus and the translation card (pagemenu.h);
//   each screen's rows of buttons (row.h) and, where a screen declares them, its
//   filter bar (filterbar.h) and its field at the top right — shown while that
//   screen is on top (app.h);
//   the side panel and Settings (side.h); the word card (wordcard.h), over all.
//
// Built again whole when the language changes (kana_ui_follow_language): what
// the widgets show comes from the app's state, so nothing is lost but what was
// typed. The fonts are loaded once: Roboto with Slug, Noto Sans JP behind it for
// Japanese, Phosphor last for the icons, and Phosphor Fill on its own.
// ===========================================================================

#define KANA_UI_ROWS 32u   // the screens' rows, all told

// A screen's field's callback context: the field and its screen.
typedef struct {
    rde_ui_text_editor*     field;
    const kana_screen_slot* slot;
} kana_ui_field_ref;

typedef struct kana_ui {
    kana_app*           app;
    rde_window*         window;
    rde_ui_canvas*      canvas;
    rde_font*           font;            // the UI font (and the screens'): Roboto
    rde_font*           font_jp;         // Noto Sans JP, its fallback: Japanese typed or shown as text
    rde_font*           font_icons;      // Phosphor Regular, its last fallback: icons as text (icons.h)
    rde_font*           font_icons_fill; // Phosphor Fill, a font of its own: an icon showing something on

    kana_toolbar        bar;
    kana_pagemenu       page;
    kana_row            rows[KANA_UI_ROWS];
    u32                 row_first[KANA_SCREEN_COUNT];   // each screen's first row in rows[]
    kana_filterbar      bars[KANA_SCREEN_COUNT];        // a screen's bar, where it declares one
    rde_ui_text_editor* fields[KANA_SCREEN_COUNT];      // a screen's field, where it declares one
    kana_ui_field_ref   field_refs[KANA_SCREEN_COUNT];
    kana_side           side;
    kana_wordcard       word;

    u32                 _text_revision;                // the language the widgets were built in (kana_text_revision)
    rde_vec_4I          _insets_seen;                  // the safe area the bar is laid out for
    rde_vec_4F          _field_for;                    // the screen size and insets the fields were placed for
    b8                  _open_seen[KANA_SCREEN_COUNT]; // which screens were open last frame
    b8                  _field_shown[KANA_SCREEN_COUNT];
} kana_ui;

// The fonts loaded, everything built (the app's screens already in place).
void       kana_ui_init(kana_ui* _ui, kana_app* _app);
void       kana_ui_destroy(kana_ui* _ui);
// Once a frame, and after anything that changes what the widgets show: the
// screen on top's row (and bar, and field) shown and the others hidden, the
// toolbar and the page's menus hidden under a screen, the side panel, the word card.
void       kana_ui_update(kana_ui* _ui);
// A row's press done (row.h: after): the same.
void       kana_ui_after_press(kana_app* _app);
// Is this point on any of it? _screen is Kana's screen space (centre origin, Y
// up) — what pen positions convert to. The page asks before writing.
b8         kana_ui_hit(const kana_ui* _ui, rde_vec_2F _screen);
// Every widget restyled from the theme (after kana_theme_set).
void       kana_ui_apply_theme(kana_ui* _ui);
// Where the screen on top draws (screen.h): under its bar, over its row.
void       kana_ui_frame(const kana_ui* _ui, KANA_SCREEN_ _screen, kana_screen_frame* _frame);
// Once a frame, outside the UI's own events (a language chosen in Settings
// rebuilds the whole UI, the button that chose it included).
void       kana_ui_follow_language(kana_ui* _ui);
// The app went to the background or the OS is short of memory: the fonts give
// back the GPU memory their glyphs grew into (rde_font_trim). Before drawing.
void       kana_ui_trim_fonts(kana_ui* _ui);
// Button _button of the row of the screen on top pressed, as a tap would (a
// developer's look: --press). False when there is no such button.
b8         kana_ui_press(kana_ui* _ui, u32 _button);
// Phosphor Regular (the UI font when it did not load).
rde_font*  kana_ui_icon_font(const kana_ui* _ui);

#endif
