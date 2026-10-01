#ifndef KANA_SIDE
#define KANA_SIDE

#include "rde.h"
#include "canvas.h"
#include "theme.h"
#include "text.h"

// ===========================================================================
// The side panel and Settings — the app's navigation, off the floating toolbar.
//
// A menu button at the top-left of the page opens a panel from the left edge,
// over the page (a dimmed backdrop; a tap on it closes the panel):
//   Study     Kanji (Browse), Kana (the chart), Album
//   Notes     the folders and canvases (notes.h), folders within folders: +
//             Folder and + Canvas; a tap opens a canvas or opens/closes a folder;
//             "…" on a row: rename, delete, and for a folder a new canvas or
//             folder in it. The handle at a row's left DRAGS it: a line shows
//             where it will go (indented to the level it lands at), a box a
//             folder it will go into; near the list's ends it scrolls.
//   ...and at the bottom the version on the left, Settings on the right.
// Settings is a card over everything: the theme (each shown in its own colours),
// the language (a flag and its name each), the diagnostics HUD, pen width, and About — the version and the credits the
// character data's licences require.
//
// Built on the toolbar's UI canvas and font (toolbar_kit.h); the toolbar owns it
// and drives it (kana_side_update, kana_side_hit, kana_side_apply_theme).
// ===========================================================================

#define KANA_SIDE_WIDTH  300.0f
#define KANA_SIDE_LICENCE_DOCS  4     // Licences: Data, Fonts, Libraries, ML Kit
#define KANA_SIDE_LICENCE_LINES 48    // labels for the lines on screen, reused as it scrolls

RDE_STRUCT {
    struct kana_toolbar* toolbar;
    u32                  theme;
} kana_side_theme_ref;

// A row of the notes list: which note.
RDE_STRUCT {
    struct kana_toolbar* toolbar;
    u32                  id;
} kana_side_note_ref;

// A row of the notes list as built: which note, how deep.
RDE_STRUCT {
    u32 id;
    u32 parent;
    u8  depth;
    u8  kind;
    b8  expanded;
} kana_side_row;

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
    rde_ui_button*  backdrop;             // under the panel: a tap closes it
    rde_ui_image*   panel;

    rde_ui_label*   study_label;
    rde_ui_button*  kanji;
    rde_ui_button*  kana;
    rde_ui_button*  album;
    rde_ui_button*  exams;
    rde_ui_button*  statistics;
    rde_ui_label*   theme_label;
    rde_ui_button*  themes[KANA_THEME_COUNT];
    kana_side_theme_ref theme_refs[KANA_THEME_COUNT];
    // The language: a flag and its name each (text.h's list; the refs' theme: the index).
    rde_ui_label*   language_label;
    rde_ui_button*  languages[KANA_TEXT_LANGUAGES];
    kana_side_theme_ref language_refs[KANA_TEXT_LANGUAGES];
    rde_ui_label*   notes_label;
    rde_ui_button*  new_folder;
    rde_ui_button*  new_canvas;
    rde_ui_scroll_area* notes_list;
    kana_side_note_ref* _note_refs;       // a row's and its "…"'s, rebuilt with the list
    u32             _notes_revision;      // what the list was built for
    u32             _notes_open;
    b8              _notes_built;
    f32             _list_width;          // as laid out
    rde_vec_2F      _list_bl;             // the list's bottom-left, UI units (the panel sits at the origin)
    rde_vec_2F      _list_size;
    kana_side_row*  _rows;                // the list as built, top to bottom
    u32             _row_count;

    // Dragging a row by its handle.
    u32             drag_id;              // 0: nothing is being dragged
    rde_vec_2F      drag_at;              // the pointer, UI units
    u32             drop_parent;          // where it would land...
    u32             drop_before;
    b8              drop_valid;
    rde_ui_image*   drop_line;            // ...shown: the line where it goes
    rde_ui_image*   drop_box;             // or a box around the folder it goes into
    rde_ui_image*   drag_ghost;           // its name, following the pointer
    rde_ui_label*   drag_ghost_label;

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
    rde_ui_button*  note_add_folder;      // a folder's: a new folder in it
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
    rde_ui_label*   paper_label;        // lines' and squares' size (canvas.h), for every canvas
    rde_ui_button*  paper_sizes[KANA_PAPER_SIZE_COUNT];
    rde_ui_label*   about_label;
    rde_ui_label*   about_text;
    rde_ui_button*  settings_close;
    rde_ui_button*  licences_button;

    // Handwriting (in Settings): Google ML Kit on or off (mlkit.h), and its model.
    rde_ui_label*   hand_label;
    rde_ui_label*   mlkit_label;
    rde_ui_button*  mlkit_toggle;
    rde_ui_label*   mlkit_status;
    rde_ui_button*  mlkit_download;       // Download / Retry, while there is no model
    i32             _mlkit_shown;         // what the row shows: the state, +100 when off; -1 none yet

    // Licences: a card over Settings, one document at a time, scrolled. Only the
    // lines on screen have labels (ML Kit's notices alone are ~25,000 lines).
    b8                  licences_open;
    rde_ui_button*      licences_backdrop;
    rde_ui_image*       licences_card;
    rde_ui_label*       licences_title;
    rde_ui_button*      licences_docs[KANA_SIDE_LICENCE_DOCS];
    kana_side_theme_ref licences_refs[KANA_SIDE_LICENCE_DOCS];
    rde_ui_scroll_area* licences_text;
    rde_ui_label*       licences_lines[KANA_SIDE_LICENCE_LINES];
    rde_ui_button*      licences_close;
    u32                 licences_doc;
    c8*                 _licence_text;    // the document shown
    u32*                _licence_starts;  // where each of its wrapped lines starts (and one more: the end)
    u32                 _licence_count;   // lines
    i32                 _licence_first;   // the first line with a label now (-1: none placed)
    f32                 _licence_line_h;
    f32                 _licence_width;   // the scroll area's, for the lines
    b8                  _shown_licences;

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
// Opens Settings — and over it Licences on document _licences, if that is not
// negative (developer launch options, kana.c).
void kana_side_open_settings(struct kana_toolbar* _toolbar, i32 _licences);
// Frees what the panel holds besides its widgets (before the UI is built again).
void kana_side_forget(struct kana_toolbar* _toolbar);

#endif
