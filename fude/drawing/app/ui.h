#ifndef FUDE_UI
#define FUDE_UI

#include "rde.h"
#include "drawing/app/app.h"
#include "drawing/widgets/row.h"
#include "drawing/widgets/toolbar.h"
#include "drawing/widgets/pagemenu.h"
#include "drawing/widgets/filterbar.h"
#include "drawing/widgets/side.h"
#include "drawing/widgets/docbar.h"

// ===========================================================================
// The retained UI: one RDE UI canvas over everything, and every widget on it —
//
//   the floating toolbar and its panels (toolbar.h), over the page;
//   the page's menus and the translation card (pagemenu.h); the document bar
//   over a canvas with a PDF (docbar.h);
//   each screen's rows of buttons (row.h) and, where a screen declares them, its
//   filter bar (filterbar.h) and its field at the top right — shown while that
//   screen is on top (app.h);
//   the side panel and Settings (side.h); the app's own widgets (extension.h:
//   Kana's word card), over all.
//
// Built again whole when the language changes (fude_ui_follow_language): what
// the widgets show comes from the app's state, so nothing is lost but what was
// typed. The fonts are loaded once: Roboto with Slug, the app's script font
// behind it (Kana: Noto Sans JP), Phosphor last for the icons, and Phosphor Fill
// on its own.
// ===========================================================================

#define FUDE_UI_ROWS 32u   // the screens' rows, all told

// A screen's field's callback context: the field and its screen.
typedef struct {
    rde_ui_text_editor*     field;
    const fude_screen_slot* slot;
} fude_ui_field_ref;

typedef struct fude_ui {
    fude_app*           app;
    rde_window*         window;
    rde_ui_canvas*      canvas;
    rde_font*           font;            // the UI font (and the screens'): Roboto
    rde_font*           font_script;     // the app's script font (info.h), its fallback: the language typed or shown as text
    rde_font*           font_latin;      // the app's Latin beyond Roboto (info.h), the next fallback: readings' letters
    rde_font*           font_icons;      // Phosphor Regular, its last fallback: icons as text (icons.h)
    rde_font*           font_icons_fill; // Phosphor Fill, a font of its own: an icon showing something on

    fude_toolbar        bar;
    fude_pagemenu       page;
    fude_docbar         docbar;          // over a document's canvas: Search, the page (docbar.h)
    fude_row            rows[FUDE_UI_ROWS];
    u32                 row_first[FUDE_APP_SCREENS];   // each screen's first row in rows[]
    fude_filterbar      bars[FUDE_APP_SCREENS];        // a screen's bar, where it declares one
    rde_ui_text_editor* fields[FUDE_APP_SCREENS];      // a screen's field, where it declares one
    fude_ui_field_ref   field_refs[FUDE_APP_SCREENS];
    fude_side           side;

    u32                 _text_revision;                // the language the widgets were built in (fude_text_revision)
    rde_vec_4I          _insets_seen;                  // the safe area the bar is laid out for
    rde_vec_2F          _screen_seen;                  // and the screen (UI units: the interface's size changes them)
    rde_vec_4F          _field_for;                    // the screen size and insets the fields were placed for
    b8                  _open_seen[FUDE_APP_SCREENS]; // which screens were open last frame
    b8                  _field_shown[FUDE_APP_SCREENS];
} fude_ui;

// The fonts loaded, everything built (the app's screens already in place).
void       fude_ui_init(fude_ui* _ui, fude_app* _app);
void       fude_ui_destroy(fude_ui* _ui);
// Once a frame, and after anything that changes what the widgets show: the
// screen on top's row (and bar, and field) shown and the others hidden, the
// toolbar and the page's menus hidden under a screen, the side panel, the word card.
void       fude_ui_update(fude_ui* _ui);
// A row's press done (row.h: after): the same.
void       fude_ui_after_press(fude_app* _app);
// Is this point on any of it? _screen is the app's screen space (centre origin,
// Y up) — what pen positions convert to. The page asks before writing.
b8         fude_ui_hit(const fude_ui* _ui, rde_vec_2F _screen);
// Every widget restyled from the theme (after fude_theme_set).
void       fude_ui_apply_theme(fude_ui* _ui);
// Where the screen on top draws (screen.h): under its bar, over its row.
void       fude_ui_frame(const fude_ui* _ui, u32 _screen, fude_screen_frame* _frame);
// Once a frame, outside the UI's own events (a language chosen in Settings
// rebuilds the whole UI, the button that chose it included).
void       fude_ui_follow_language(fude_ui* _ui);
// The app went to the background or the OS is short of memory: the fonts give
// back the GPU memory their glyphs grew into (rde_font_trim). Before drawing.
void       fude_ui_trim_fonts(fude_ui* _ui);
// Button _button of the row of the screen on top pressed, as a tap would (a
// developer's look: --press). False when there is no such button.
b8         fude_ui_press(fude_ui* _ui, u32 _button);
// Back (Android's, Escape): the row on top's Back, else its Close, pressed as a
// tap would. False: it has neither there now (left out, greyed, or no row).
b8         fude_ui_press_back(fude_ui* _ui);
// Phosphor Regular (the UI font when it did not load).
rde_font*  fude_ui_icon_font(const fude_ui* _ui);

#endif
