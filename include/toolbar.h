#ifndef KANA_TOOLBAR
#define KANA_TOOLBAR

#include "rde.h"
#include "ink.h"
#include "canvas.h"
#include "lasso.h"
#include "viewer.h"
#include "browse.h"
#include "chart.h"
#include "practice.h"
#include "album.h"
#include "side.h"
#include "notes.h"
#include "check.h"

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
    KANA_TOOL_ERASE,
    KANA_TOOL_LASSO
} KANA_TOOL_;

// The UI font: Roboto, rendered with Slug (curves, sharp at any size), with Noto
// Sans JP behind it for Japanese. Loaded at this size; everything that draws
// text with it scales from here. The rest of the app draws its text with
// toolbar.font too — see kana.c.
#define KANA_TOOLBAR_FONT_SIZE   32

#define KANA_TOOLBAR_PALETTE_COUNT 8

// Slider range for the brush half-width (see kana_ink.constant_radius).
#define KANA_TOOLBAR_SIZE_MIN  0.5f
#define KANA_TOOLBAR_SIZE_MAX 12.0f

typedef struct kana_toolbar kana_toolbar;

#define KANA_TOOLBAR_MENU_MAX 7

// A floating row of buttons: the menu over a lasso selection, the page's
// context menu.
RDE_STRUCT {
    rde_ui_image*  panel;
    rde_ui_button* buttons[KANA_TOOLBAR_MENU_MAX];
    u32            count;
    b8             open;
    rde_vec_2F     center;   // UI canvas units
    rde_vec_2F     size;
} kana_toolbar_menu;

// One swatch's callback context: which toolbar, which colour.
RDE_STRUCT {
    kana_toolbar* toolbar;
    u32           index;
} kana_toolbar_swatch_ref;

// One Browse chip's callback context: which toolbar, which filter or sort.
typedef kana_toolbar_swatch_ref kana_toolbar_chip_ref;

struct kana_toolbar {
    rde_ui_canvas* ui;
    rde_font*      font;
    rde_font*      font_jp;         // Noto Sans JP, font's fallback: Japanese typed or shown as text
    rde_window*    window;

    // What the toolbar drives.
    kana_ink*      ink;
    kana_canvas*   view;
    kana_lasso*    lasso;
    kana_viewer*   viewer;
    kana_browse*   browse;
    kana_chart*    chart;
    kana_practice* practice;
    kana_album*    album;
    kana_notes*    notes;
    kana_check*    check;
    b8*            show_hud;

    KANA_TOOL_     tool;
    b8             vertical;
    rde_vec_2F     center;          // panel centre, UI canvas units (bottom-left origin, Y up)
    rde_vec_2F     panel_size;

    rde_ui_image*  panel;
    // The grip's end of the bar: what drags it, and a double tap there folds the
    // bar down to just this (minimized) and opens it again. The handle drawn in it.
    rde_ui_image*  grip_area;
    rde_ui_image*  grip;
    b8             minimized;
    // The tools, in a strip that scrolls along the bar when they do not all fit
    // (and more can be added without the bar outgrowing the screen).
    rde_ui_scroll_area* strip;
    rde_ui_button* undo;
    rde_ui_button* redo;
    rde_ui_button* draw;
    rde_ui_button* erase;
    rde_ui_button* lasso_tool;
    rde_ui_button* clear;
    rde_ui_slider* size;
    rde_ui_button* color;
    rde_ui_button* brush_scale;
    rde_ui_button* rotate;
    rde_ui_button* reset_view;

    rde_ui_image*            palette;
    rde_ui_button*           swatches[KANA_TOOLBAR_PALETTE_COUNT];
    kana_toolbar_swatch_ref  swatch_refs[KANA_TOOLBAR_PALETTE_COUNT];
    b8                       palette_open;
    rde_vec_2F               palette_center;   // UI canvas units, set whenever it is placed
    rde_vec_2F               palette_size;

    // Over a lasso selection: Cut, Copy, Duplicate, Delete.
    kana_toolbar_menu        selection_menu;
    f64                      copied_until;     // Copy reads "Copied" until then (engine clock)
    // The page's context menu, opened by a long press: Paste, Select all.
    kana_toolbar_menu        context_menu;
    rde_vec_2F               context_canvas;   // where it was opened, on the page — where Paste lands
    // Under the character viewer: Prev, Replay, Next, Close. While the viewer is
    // open the bar itself and every other menu hide.
    kana_toolbar_menu        viewer_menu;
    b8                       _viewer_shown;     // a full-screen scene is up: the floating bar is hidden
    // The kana chart's row: Hiragana, Katakana (jumps), Close.
    kana_toolbar_menu        chart_menu;
    // Browse's row, at the bottom like the chart's and the album's: Practice (its
    // list as a set), Close.
    kana_toolbar_menu        browse_menu;
    // Practice's row: Back, Undo, Clear, Score, fewer / more squares.
    kana_toolbar_menu        practice_menu;
    // Check's row: Back, Stroke order, Practice; and its "I meant…" field, at
    // the top right of its screen.
    kana_toolbar_menu        check_menu;
    rde_ui_text_editor*      check_field;
    b8                       _check_field_shown;
    rde_vec_4F               _check_field_for;   // the screen size and insets it was placed for
    // The album's rows: the overview's (the sorts, Close) and a character page's
    // (Back, Practice).
    kana_toolbar_menu        album_menu;
    kana_toolbar_menu        album_page_menu;
    kana_toolbar_chip_ref    album_sort_refs[KANA_ALBUM_SORT_COUNT];
    u32                      _album_practice_shown;   // the count "Practice n" shows
    // Practice over a set: Back, Undo, Clear, Score, Next (Finish on the last);
    // and the set's summary: Weakest again, Done.
    kana_toolbar_menu        practice_set_menu;
    kana_toolbar_menu        practice_summary_menu;
    b8                       _finish_shown;
    b8                       _guided_shown;      // what the Guided buttons show
    b8                       _can_score_shown;   // Score pressable (not in guided steps 1 and 2)

    // Browse's bar, across the top of the screen: filters, sorts, the search
    // field, Draw/Clear, Close.
    rde_ui_image*            browse_bar;
    rde_ui_button*           filter_chips[KANA_FILTER_COUNT];
    kana_toolbar_chip_ref    filter_refs[KANA_FILTER_COUNT];
    rde_ui_button*           sort_chips[KANA_SORT_COUNT];
    kana_toolbar_chip_ref    sort_refs[KANA_SORT_COUNT];
    rde_ui_text_editor*      search_field;
    rde_ui_button*           draw_toggle;
    rde_ui_button*           parts_toggle;        // the parts panel (search by component)
    rde_ui_button*           pad_clear;
    b8                       _browse_shown;
    rde_vec_2F               _browse_laid_out;   // the screen size the bar was laid out for
    rde_vec_4I               _insets_seen;       // the safe area the layouts are for (see kana_toolbar_update_viewer)
    f32                      browse_bar_height;  // UI units, including the top safe inset

    // Navigation, the themes, Settings: the side panel (side.h).
    kana_side                side;

    // Grip drag, and the grip's last tap (for a double tap).
    rde_vec_2F     drag_start_center;
    rde_vec_2F     drag_press;
    f64            grip_tapped;        // engine clock (0: no tap waiting for its second)
    rde_vec_2F     grip_tapped_at;

    // What Undo/Redo currently show, so kana_toolbar_update only touches them on
    // a change.
    b8             _history_shown;
    b8             _can_undo_shown;
    b8             _can_redo_shown;
};

void       kana_toolbar_init(kana_toolbar* _toolbar, rde_window* _window, kana_ink* _ink, kana_canvas* _view, kana_lasso* _lasso,
                             kana_viewer* _viewer, kana_browse* _browse, kana_chart* _chart, kana_practice* _practice, kana_album* _album, kana_notes* _notes, kana_check* _check, b8* _show_hud);
void       kana_toolbar_destroy(kana_toolbar* _toolbar);

// Is this point on the toolbar, its open palette or an open menu? _screen is
// Kana's screen space (centre-origin, Y up) — what pen positions convert to.
b8         kana_toolbar_hit(const kana_toolbar* _toolbar, rde_vec_2F _screen);

// Re-reads ink state into the widgets (after a keyboard shortcut changed it), and
// restyles every widget from the current theme (after kana_theme_set).
void       kana_toolbar_sync(kana_toolbar* _toolbar);

// The page's context menu, for a long press at _screen (Kana screen space).
// _canvas is the same point on the page: where Paste will land. It floats just
// above the finger so the finger doesn't cover it. Closed by choosing an item or
// by kana_toolbar_close_context_menu (the caller closes it on any other press).
void       kana_toolbar_open_context_menu(kana_toolbar* _toolbar, rde_vec_2F _screen, rde_vec_2F _canvas);
void       kana_toolbar_close_context_menu(kana_toolbar* _toolbar);

// Puts the bar back where a save left it (orientation, centre in UI canvas units,
// folded to its grip or not). Clamped on screen, so a centre from a bigger or
// rotated screen is fine.
void       kana_toolbar_set_placement(kana_toolbar* _toolbar, b8 _vertical, rde_vec_2F _center, b8 _minimized);

// Once a frame: greys Undo/Redo out when there is nothing to undo/redo, and
// shows the selection menu over a lasso selection. Cheap — it only touches the
// widgets when something changed.
void       kana_toolbar_update(kana_toolbar* _toolbar);

#endif
