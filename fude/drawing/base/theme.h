#ifndef FUDE_THEME
#define FUDE_THEME

#include "rde.h"

// ===========================================================================
// Themes: every colour Kana draws with, in one table per theme, so a change of
// theme changes the page, the ink, the stroke-order animation, the practice
// sheet, the text and the toolbar all at once. No module keeps colours of its
// own: they ask fude_theme_active() at draw time.
//
// INK. The default ink is not a colour but "the theme's ink" — FUDE_THEME_INK,
// stored in strokes and settings as rde_color {0,0,0,0} — so writing stays
// readable when the page turns dark. Strokes written in a palette colour keep it.
// ===========================================================================

typedef enum {
    FUDE_THEME_PAPER = 0,
    FUDE_THEME_WASHI,
    FUDE_THEME_NIGHT,
    FUDE_THEME_MATCHA,
    FUDE_THEME_SAKURA,
    FUDE_THEME_COUNT
} FUDE_THEME_;

// "Use the theme's ink": fully transparent, which no real stroke colour is.
#define FUDE_THEME_INK (rde_color){ 0, 0, 0, 0 }

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

    // A character writing itself (viewer, practice demo).
    rde_color ghost;           // strokes not written yet
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
    rde_color eraser;          // eraser strokes (the spike's red)
    rde_color samples;         // raw sample dots (diagnostic)

    // The UI's surfaces and its one accent. Light themes have light panels (dark
    // text on them), Night dark ones. The accent is the way on (a primary
    // button), a chosen thing (a tint of it behind, the accent on top) and the
    // study marks' Studying.
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
    rde_color field_border;    // ...and its edge: clearer than a panel's hairline
    rde_color field_placeholder;
    rde_color slider_track;
    rde_color slider_fill;
    rde_color slider_thumb;
    rde_color swatch_border;
} fude_theme;

// The current theme (never NULL).
const fude_theme* fude_theme_active(void);
FUDE_THEME_       fude_theme_index(void);
void              fude_theme_set(FUDE_THEME_ _theme);
const fude_theme* fude_theme_get(FUDE_THEME_ _theme);

// A score's colour (0..100): good from 80, fair from 55, poor below — Practice
// and the Album grade alike.
#define FUDE_THEME_GRADE_GOOD 80.0f
#define FUDE_THEME_GRADE_FAIR 55.0f
rde_color         fude_theme_grade(f32 _score);

// A stroke or brush colour as drawn: FUDE_THEME_INK becomes the theme's ink.
rde_color         fude_theme_resolve(rde_color _color);
b8                fude_theme_is_ink(rde_color _color);

#endif
