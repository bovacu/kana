#ifndef KANA_THEME
#define KANA_THEME

#include "rde.h"

// ===========================================================================
// Themes: every colour Kana draws with, in one table per theme, so a change of
// theme changes the page, the ink, the stroke-order animation, the practice
// sheet, the text and the toolbar all at once. No module keeps colours of its
// own: they ask kana_theme_active() at draw time.
//
// INK. The default ink is not a colour but "the theme's ink" — KANA_THEME_INK,
// stored in strokes and settings as rde_color {0,0,0,0} — so writing stays
// readable when the page turns dark. Strokes written in a palette colour keep it.
// ===========================================================================

typedef enum {
    KANA_THEME_PAPER = 0,
    KANA_THEME_WASHI,
    KANA_THEME_NIGHT,
    KANA_THEME_MATCHA,
    KANA_THEME_SAKURA,
    KANA_THEME_COUNT
} KANA_THEME_;

// "Use the theme's ink": fully transparent, which no real stroke colour is.
#define KANA_THEME_INK (rde_color){ 0, 0, 0, 0 }

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

    // The UI chrome: toolbar, bars, menus.
    rde_color panel;
    rde_color panel_border;
    rde_color grip;
    rde_color button;
    rde_color button_selected;
    rde_color button_text;
    rde_color button_text_disabled;
    rde_color danger;          // destructive buttons (Delete)
    rde_color field;           // the search field
    rde_color field_placeholder;
    rde_color slider_track;
    rde_color slider_fill;
    rde_color slider_thumb;
    rde_color swatch_border;
} kana_theme;

// The current theme (never NULL).
const kana_theme* kana_theme_active(void);
KANA_THEME_       kana_theme_index(void);
void              kana_theme_set(KANA_THEME_ _theme);
const kana_theme* kana_theme_get(KANA_THEME_ _theme);

// A score's colour (0..100): good from 80, fair from 55, poor below — Practice
// and the Album grade alike.
#define KANA_THEME_GRADE_GOOD 80.0f
#define KANA_THEME_GRADE_FAIR 55.0f
rde_color         kana_theme_grade(f32 _score);

// A stroke or brush colour as drawn: KANA_THEME_INK becomes the theme's ink.
rde_color         kana_theme_resolve(rde_color _color);
b8                kana_theme_is_ink(rde_color _color);

#endif
