// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_UI
#define FUDE_UI

#include "rde.h"
#include "drawing/app/app.h"
#include "drawing/widgets/row.h"
#include "drawing/app/toolbar.h"
#include "drawing/app/pagemenu.h"
#include "drawing/widgets/filterbar.h"
#include "drawing/app/side.h"
#include "drawing/app/docbar.h"
#include "drawing/app/crashcard.h"
#include "drawing/app/handwriting.h"

// ===========================================================================
// UI: the retained UI, one RDE UI canvas over everything, and every widget on it:
//
//   the floating toolbar and its panels (toolbar.h), over the page;
//   the page's menus and the translation card (pagemenu.h), and the document bar over a canvas with a PDF (docbar.h);
//   each screen's rows of buttons (row.h) and, where a screen declares them, its filter bar (filterbar.h) and its
//   field at the top right, shown while that screen is on top (app.h);
//   the side panel and Settings (side.h); the crash card (crashcard.h); and the app's own widgets (extension.h:
//   a study app's word card), over all of them.
//
// Where it sits: drawing/app. The shell owns one fude_ui and calls it each frame; the app (app.h), the page and the
// app's extension reach the widgets through fude_app.ui.
//
// Its main types and their lifecycle: the shell makes the fude_ui (a static) with fude_ui_init, after the app's
// screens are in place, and frees it with fude_ui_destroy. All the widgets are built again, whole, when the language
// changes (fude_ui_follow_language): what they show comes from the app's state, so nothing is lost but what was
// being typed.
//
// Threads: main thread only, like all of RDE's UI.
//
// Files: loads the fonts once: Roboto with Slug, the app's script font behind it (Japanese: Noto Sans JP), Phosphor
// last for the icons, and Phosphor Fill on its own.
//
// Design: the canvas is CONSTANT_PIXEL, so one UI unit is one window unit: the bar is the same size on every screen
// and positions match the pen's. Its automatic safe area is off, because it would move the root and every widget with
// it away from where fude_ui_hit tests them; fude_kit_clamp keeps what floats clear of the unsafe edges instead.
// ===========================================================================

#define FUDE_UI_ROWS 32u   // The most rows of buttons, counting every screen's.

// What a screen's field hands its callbacks: the field and its screen.
typedef struct {
    rde_ui_text_editor*     field;
    const fude_screen_slot* slot;
} fude_ui_field_ref;

// The retained UI. The fields are ui.c's; the widgets' owners read their own parts.
typedef struct fude_ui {
    fude_app*           app;
    rde_window*         window;
    rde_ui_canvas*      canvas;
    rde_font*           font;            // The UI font, and the screens': Roboto.
    // The app's script font (info.h), the UI font's fallback: the language as it is typed or shown as text.
    rde_font*           font_script;
    rde_font*           font_latin;      // The app's Latin font beyond Roboto (info.h), the next fallback: readings.
    rde_font*           font_icons;      // Phosphor Regular, the last fallback: icons written as text (icons.h).
    rde_font*           font_icons_fill; // Phosphor Fill, a font of its own: an icon showing that something is on.

    fude_toolbar        bar;
    fude_pagemenu       page;
    fude_docbar         docbar;          // Over a document's canvas: Search and the page (docbar.h).
    fude_row            rows[FUDE_UI_ROWS];
    u32                 row_first[FUDE_APP_SCREENS];   // Each screen's first row in rows[].
    fude_filterbar      bars[FUDE_APP_SCREENS];        // A screen's bar, where it declares one.
    rde_ui_text_editor* fields[FUDE_APP_SCREENS];      // A screen's field, where it declares one.
    fude_ui_field_ref   field_refs[FUDE_APP_SCREENS];
    fude_side           side;
    fude_crashcard      crash;     // Asks about the last session's crash report (crashcard.h).
    fude_handwriting    handwriting;   // Settings' Handwriting, in an app that lists it (handwriting.h).
    // The page alone on the screen (a canvas app presenting): the bar, the menus and the menu button are put away.
    b8                  chrome_hidden;

    u32                 _text_revision;                // The language the widgets were built in (fude_text_revision).
    // The safe area and the screen size (in UI units, which the interface's size changes) the bar is laid out for.
    rde_vec_4I          _insets_seen;
    rde_vec_2F          _screen_seen;
    rde_vec_4F          _field_for;                    // The screen size and insets the fields were placed for.
    b8                  _open_seen[FUDE_APP_SCREENS]; // Which screens were open last frame.
    b8                  _field_shown[FUDE_APP_SCREENS]; // Which screens' fields are showing.
} fude_ui;

// fude_ui_init loads the fonts and builds everything; the app's screens must already be in place. fude_ui_destroy
// lets go of the canvas, its widgets, what draw.c keeps for the fonts' texts (fude_draw_forget) and the fonts.
void       fude_ui_init(fude_ui* _ui, fude_app* _app);
void       fude_ui_destroy(fude_ui* _ui);
// Brings the widgets up to date: the screen on top's row (and bar, and field) shown and the others hidden, the toolbar
// and the page's menus hidden under a screen, the side panel, the app's own widgets (the word card). Call it once a
// frame, and after anything that changes what the widgets show.
void       fude_ui_update(fude_ui* _ui);
// The same, for a row's `after` hook (row.h), called once a press is done.
void       fude_ui_after_press(fude_app* _app);
// Whether a point is on any of the UI. _screen is in the app's screen space (centre origin, Y up), which is what pen
// positions convert to. The page asks before writing.
b8         fude_ui_hit(const fude_ui* _ui, rde_vec_2F _screen);
// Restyles every widget from the theme; call it after fude_theme_set.
void       fude_ui_apply_theme(fude_ui* _ui);
// Fills in where screen _screen (its place in the app's table) draws (screen.h): below its bar, above its row.
void       fude_ui_frame(const fude_ui* _ui, u32 _screen, fude_screen_frame* _frame);
// Rebuilds the whole UI when the language has changed. Call it once a frame, outside the UI's own events, because a
// language chosen in Settings rebuilds everything, the button that chose it included.
void       fude_ui_follow_language(fude_ui* _ui);
// Builds every widget again now, as a language change does (when an app's tools changed: a canvas app's topics). Never
// call it from inside a widget's own press; do it the frame after.
void       fude_ui_rebuild_now(fude_ui* _ui);
// Whether the UI is laid out right to left (a right-to-left language, or --rtl).
b8         fude_ui_rtl(void);
// Makes the fonts give back the GPU memory their glyphs grew into (rde_font_trim), and draw.c the memory of the
// texts' shapes it keeps (fude_draw_forget), when the app went to the background or the OS is short of memory. Call
// it before drawing.
void       fude_ui_trim_fonts(fude_ui* _ui);
// Presses button _button of the row of the screen on top, as a tap would (a developer's look: --press). Returns false
// when there is no such button, or it is greyed out.
b8         fude_ui_press(fude_ui* _ui, u32 _button);
// For Back (Android's, or Escape): presses the top row's Back button, else its Close, as a tap would. Returns false
// when it has neither there now (left out, greyed out, or no row).
b8         fude_ui_press_back(fude_ui* _ui);
// Phosphor Regular, or the UI font when Phosphor did not load.
rde_font*  fude_ui_icon_font(const fude_ui* _ui);

#endif
