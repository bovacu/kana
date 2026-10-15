// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_THEME
#define FUDE_THEME

#include "rde.h"

// ===========================================================================
// Themes: every colour the app draws with, in one table per theme, so a change of theme changes the page, the ink,
// the stroke-order animation, the practice sheet, the text and the toolbar all at once.
//
// Where it sits: drawing/base. Every module that draws asks fude_theme_active() at draw time; no module keeps colours
// of its own. An app may put its own set of themes in place of the core's (fude_theme_use).
// Its main types and their lifecycle: fude_theme tables are constant. The core's live in theme.c; an app's own set
// belongs to the app, and is used by pointer.
// Threads: main thread only.
// Design: the default ink is not a colour but "the theme's ink" (FUDE_THEME_INK, stored in strokes and settings as
// rde_color {0,0,0,0}), so writing stays readable when the page turns dark. Strokes written in a palette colour keep
// that colour. FUDE_THEME_PAGE_FILL works the same way for the page's colour.
// ===========================================================================

// The themes, by their place in the table.
typedef enum {
    FUDE_THEME_PAPER = 0,
    FUDE_THEME_WASHI,
    FUDE_THEME_NIGHT,
    FUDE_THEME_MATCHA,
    FUDE_THEME_SAKURA,
    FUDE_THEME_COUNT
} FUDE_THEME_;

// Means "use the theme's ink": fully transparent, which no real stroke colour is.
#define FUDE_THEME_INK (rde_color){ 0, 0, 0, 0 }
// Means "use the theme's page": a fill the colour of the page under it, whatever the theme (Sketching's diagram
// shapes use it, so lines behind them do not show through). No real colour has this value.
#define FUDE_THEME_PAGE_FILL (rde_color){ 255, 254, 253, 1 }

// One theme's colours.
RDE_STRUCT {
    const c8* name;

    // The page and what is written on it.
    rde_color page;            // every scene's background
    rde_color page_dots;       // the canvas's dot grid
    rde_color text;            // text on the page
    rde_color text_soft;       // secondary text: captions, labels, okurigana
    rde_color ink;             // the default ink, and characters drawn from their strokes
    rde_color line;            // thin separators (grid cells)
    rde_color hud;             // the diagnostic HUD

    // A character drawing itself stroke by stroke (the viewer, Practice's demonstration).
    rde_color ghost;           // strokes not written yet
    // The tip of the pen drawing it, and the numbers on its strokes.
    rde_color pen_tip;
    rde_color stroke_number;

    // The practice sheet.
    rde_color sheet;           // a square's fill
    rde_color sheet_outline;
    rde_color sheet_guide;     // the dashed centre cross
    rde_color reference;       // the model shown behind scored ink (translucent)
    rde_color score_good;
    rde_color score_fair;
    rde_color score_poor;

    // Editing.
    rde_color select;          // the lasso's line and box
    rde_color select_glow;     // around selected strokes (translucent)
    rde_color select_fill;     // inside the box (translucent)
    rde_color samples;        // raw sample dots (diagnostic)

    // The UI's surfaces and its single accent colour. Light themes have light panels with dark text on them; Night
    // has dark ones. The accent marks the way forward (a primary button), a chosen thing (a tint of it behind, the
    // accent on top) and the study marks' Studying.
    rde_color surface;         // panels, cards, the bar
    rde_color surface_2;       // on a surface: a plain button, a chip, a track
    rde_color outline;         // hairline borders
    rde_color accent;
    rde_color on_accent;       // text and icons on the accent
    rde_color tint;            // the accent, faint: behind a chosen thing (translucent)

    // The UI chrome: toolbar, bars, menus.
    rde_color panel;
    rde_color panel_border;
    rde_color grip;
    rde_color button;
    rde_color button_selected;
    rde_color button_text;
    rde_color button_text_disabled;
    rde_color danger;          // destructive buttons (Delete)
    rde_color field;           // a text field's box
    rde_color field_border;    // a text field's edge, clearer than a panel's hairline
    rde_color field_placeholder;
    rde_color slider_track;
    rde_color slider_fill;
    rde_color slider_thumb;
    rde_color swatch_border;
} fude_theme;

// fude_theme_active returns the current theme (never NULL), and fude_theme_index says which it is. fude_theme_set
// makes another one current, and fude_theme_get returns any theme by its place; an unknown place means Paper.
// The pointers stay valid while the set they point into is in use.
const fude_theme* fude_theme_active(void);
FUDE_THEME_       fude_theme_index(void);
void              fude_theme_set(FUDE_THEME_ _theme);
const fude_theme* fude_theme_get(FUDE_THEME_ _theme);
// Puts an app's own themes in place of the core's: _themes holds FUDE_THEME_COUNT of them in the same places (Paper
// first, Night third, as exports and the core count on), and _texts the text id (FUDE_TEXT_) of each one's name.
// Both are kept, not copied. NULL goes back to the core's. Call before the UI is built.
void              fude_theme_use(const fude_theme* _themes, const u32* _texts);
// fude_theme_text returns a theme's name as a text id (FUDE_TEXT_); fude_theme_core returns the core's own theme at
// that place, which an app's set starts from.
u32               fude_theme_text(FUDE_THEME_ _theme);
const fude_theme* fude_theme_core(FUDE_THEME_ _theme);

// The bounds of a score's grade (scores run from 0 to 100): good from 80, fair from 55, poor below. Practice and the
// Album grade alike.
#define FUDE_THEME_GRADE_GOOD 80.0f
#define FUDE_THEME_GRADE_FAIR 55.0f
// Returns a score's colour: the theme's good, fair or poor colour.
rde_color         fude_theme_grade(f32 _score);

// Returns _c lighter (_delta > 0) or darker (_delta < 0) by _delta in each channel, clamped; its alpha is kept.
rde_color         fude_theme_shade(rde_color _c, i32 _delta);
// Returns a colour read from a file as it means now. Before themes, the default ink was saved as 30 30 36 255; that
// now means "the theme's ink" (FUDE_THEME_INK), so old pages and settings follow the theme too. Any other colour
// comes back as it is.
rde_color         fude_theme_saved_color(rde_color _color);
// fude_theme_resolve returns a stroke or brush colour as it is drawn: FUDE_THEME_INK becomes the theme's ink, and
// FUDE_THEME_PAGE_FILL the theme's page. fude_theme_is_ink returns whether a colour means the theme's ink (any
// colour whose alpha is 0).
rde_color         fude_theme_resolve(rde_color _color);
b8                fude_theme_is_ink(rde_color _color);

#endif
