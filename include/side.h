#ifndef KANA_SIDE
#define KANA_SIDE

#include "rde.h"
#include "theme.h"

// ===========================================================================
// The side panel and Settings — the app's navigation, off the floating toolbar.
//
// A menu button at the top-left of the page opens a panel from the left edge,
// over the page (a dimmed backdrop; a tap on it closes the panel):
//   Study     Kanji (Browse), Kana (the chart), Album
//   Notes     the folders and canvases (notes.h): + Folder and + Canvas; a tap
//             opens a canvas or opens/closes a folder; "…" on a row: rename,
//             delete, and for a folder a new canvas in it
//   ...and at the bottom the version on the left, Settings on the right.
// Settings is a card over everything: the theme (each shown in its own colours),
// the diagnostics HUD, pen width, and About — the version and the credits the
// character data's licences require.
//
// Built on the toolbar's UI canvas and font (toolbar_kit.h); the toolbar owns it
// and drives it (kana_side_update, kana_side_hit, kana_side_apply_theme).
// ===========================================================================

#define KANA_SIDE_WIDTH  300.0f

RDE_STRUCT {
    struct kana_toolbar* toolbar;
    u32                  theme;
} kana_side_theme_ref;

// A row of the notes list: which note.
RDE_STRUCT {
    struct kana_toolbar* toolbar;
    u32                  id;
} kana_side_note_ref;

// The note card: what it is showing.
typedef enum {
    KANA_SIDE_CARD_NONE = 0,
    KANA_SIDE_CARD_ACTIONS,     // rename / delete / (a folder's) new canvas
    KANA_SIDE_CARD_RENAME,
    KANA_SIDE_CARD_DELETE
} KANA_SIDE_CARD_;

RDE_STRUCT {
    b8              open;
    b8              settings_open;

    rde_ui_button*  menu_button;          // top-left, on the page
    rde_ui_image*   menu_bars[3];         // its icon
    rde_ui_button*  backdrop;             // under the panel: a tap closes it
    rde_ui_image*   panel;

    rde_ui_label*   study_label;
    rde_ui_button*  kanji;
    rde_ui_button*  kana;
    rde_ui_button*  album;
    rde_ui_label*   theme_label;
    rde_ui_button*  themes[KANA_THEME_COUNT];
    kana_side_theme_ref theme_refs[KANA_THEME_COUNT];
    rde_ui_label*   notes_label;
    rde_ui_button*  new_folder;
    rde_ui_button*  new_canvas;
    rde_ui_scroll_area* notes_list;
    kana_side_note_ref* _note_refs;       // a row's and its "…"'s, rebuilt with the list
    u32             _notes_revision;      // what the list was built for
    u32             _notes_open;
    b8              _notes_built;
    f32             _list_width;          // as laid out

    // The note card, over the panel.
    u8              card_mode;            // KANA_SIDE_CARD_
    u32             card_note;
    rde_ui_button*  note_backdrop;
    rde_ui_image*   note_card;
    rde_ui_label*   note_title;
    rde_ui_label*   note_body;
    rde_ui_text_editor* note_field;
    rde_ui_button*  note_rename;
    rde_ui_button*  note_add;             // a folder's: a new canvas in it
    rde_ui_button*  note_delete;
    rde_ui_button*  note_cancel;
    rde_ui_button*  note_confirm;         // Save / Delete
    b8              _shown_note_card;
    rde_ui_label*   version;
    rde_ui_button*  settings_button;

    // Settings.
    rde_ui_button*  settings_backdrop;
    rde_ui_image*   card;
    rde_ui_label*   settings_title;
    rde_ui_label*   hud_label;
    rde_ui_button*  hud_toggle;
    rde_ui_label*   width_label;
    rde_ui_button*  width_even;
    rde_ui_button*  width_pressure;
    rde_ui_label*   about_label;
    rde_ui_label*   about_text;
    rde_ui_button*  settings_close;

    rde_vec_2F      _laid_out;            // the screen size the layout is for...
    rde_vec_4I      _laid_out_insets;     // ...and the safe area (iOS reports it only after the first frames)
    b8              _shown_panel;
    b8              _shown_settings;
    b8              _shown_menu;
    rde_vec_2F      _menu_center;         // UI units, for hit tests
    rde_vec_2F      _menu_size;
} kana_side;

// Builds it under _root, hidden.
void kana_side_create(struct kana_toolbar* _toolbar, rde_ui_node* _root);
// Once a frame: what shows (_full: a full-screen scene is up — no menu button,
// no panel), and a new layout when the screen changed.
void kana_side_update(struct kana_toolbar* _toolbar, b8 _full);
void kana_side_apply_theme(struct kana_toolbar* _toolbar);
// Is this UI point (bottom-left origin) the side's? While the panel or Settings
// is open, everywhere is.
b8   kana_side_hit(const struct kana_toolbar* _toolbar, rde_vec_2F _ui);
void kana_side_close(struct kana_toolbar* _toolbar);

#endif
