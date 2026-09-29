#ifndef KANA_TOOLBAR
#define KANA_TOOLBAR

#include "rde.h"
#include "ink.h"
#include "canvas.h"

// ===========================================================================
// The floating toolbar: a movable bar of tools that can sit anywhere on screen,
// vertical or horizontal. An RDE UI canvas of its own, text in Slug.
//
// Children are placed by hand (kana_toolbar_layout) rather than by an hbox/vbox:
// rotating the bar is then just laying the same children out along the other
// axis, and none of the container sizing rules come into it.
//
// INPUT: the RDE UI only ever sees MOUSE events — finger taps and pen touches
// reach it as SDL's synthetic mouse, AFTER the app has already had the pen/touch
// event. So the app must ask kana_toolbar_hit before starting ink, an erase or a
// pan, or writing on a button would also draw under it.
// ===========================================================================

typedef enum {
    KANA_TOOL_DRAW = 0,
    KANA_TOOL_ERASE
} KANA_TOOL_;

#define KANA_TOOLBAR_PALETTE_COUNT 8

// Slider range for the brush half-width (see kana_ink.constant_radius).
#define KANA_TOOLBAR_SIZE_MIN  0.5f
#define KANA_TOOLBAR_SIZE_MAX 12.0f

typedef struct kana_toolbar kana_toolbar;

// One swatch's callback context: which toolbar, which colour.
RDE_STRUCT {
    kana_toolbar* toolbar;
    u32           index;
} kana_toolbar_swatch_ref;

struct kana_toolbar {
    rde_ui_canvas* ui;
    rde_font*      font;
    rde_window*    window;

    // What the toolbar drives.
    kana_ink*      ink;
    kana_canvas*   view;
    b8*            show_hud;

    KANA_TOOL_     tool;
    b8             vertical;
    rde_vec_2F     center;          // panel centre, UI canvas units (bottom-left origin, Y up)
    rde_vec_2F     panel_size;
    rde_vec_2F     color_button_center;   // panel-local, for placing the palette

    rde_ui_image*  panel;
    rde_ui_image*  grip;
    rde_ui_button* draw;
    rde_ui_button* erase;
    rde_ui_button* clear;
    rde_ui_slider* size;
    rde_ui_button* color;
    rde_ui_button* brush_scale;
    rde_ui_button* rotate;
    rde_ui_button* reset_view;
    rde_ui_button* hud;

    rde_ui_image*            palette;
    rde_ui_button*           swatches[KANA_TOOLBAR_PALETTE_COUNT];
    kana_toolbar_swatch_ref  swatch_refs[KANA_TOOLBAR_PALETTE_COUNT];
    b8                       palette_open;
    rde_vec_2F               palette_center;   // UI canvas units, set whenever it is placed
    rde_vec_2F               palette_size;

    // Grip drag.
    rde_vec_2F     drag_start_center;
    rde_vec_2F     drag_press;
};

void       kana_toolbar_init(kana_toolbar* _toolbar, rde_window* _window, kana_ink* _ink, kana_canvas* _view, b8* _show_hud);
void       kana_toolbar_destroy(kana_toolbar* _toolbar);

// Is this point on the toolbar or its open palette? _screen is Kana's screen
// space (centre-origin, Y up) — what pen positions convert to.
b8         kana_toolbar_hit(const kana_toolbar* _toolbar, rde_vec_2F _screen);

// Re-reads ink state into the widgets (after a keyboard shortcut changed it).
void       kana_toolbar_sync(kana_toolbar* _toolbar);

#endif
