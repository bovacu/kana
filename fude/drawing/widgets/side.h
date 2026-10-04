// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SIDE
#define FUDE_SIDE

#include "rde.h"
#include "drawing/widgets/kit.h"
#include "drawing/ink/canvas.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/app/info.h"
#include "drawing/app/extension.h"

// ===========================================================================
// The side panel and Settings — the app's navigation, off the floating toolbar.
//
// A menu button at the top-left of the page opens a panel from the left edge,
// over the page (a dimmed backdrop; a tap on it closes the panel):
//   the app's navigation (extension.h; Kana's Study: Kanji, Kana, Album...)
//   Notes     the folders and canvases (notes.h), folders within folders: +
//             Folder and + Canvas; a tap opens a canvas or opens/closes a folder;
//             "…" on a row: rename, delete, and for a folder a new canvas or
//             folder in it. The handle at a row's left DRAGS it: a line shows
//             where it will go (indented to the level it lands at), a box a
//             folder it will go into; near the list's ends it scrolls.
//   ...and at the bottom the version on the left, Settings on the right.
// Settings is a card over everything: the theme (each shown in its own colours),
// the language (a flag and its name each), pen width, paper, the app's own
// sections (extension.h), and About — the version and the app's credits.
//
// Built on the UI's canvas (ui.h) with the kit (kit.h); the UI owns it and
// drives it (fude_side_update, fude_side_hit, fude_side_apply_theme).
// ===========================================================================

#define FUDE_SIDE_WIDTH  300.0f
// Text sizes (the UI font's units; Slug draws an em ~1.31 x these).
#define FUDE_SIDE_TITLE_PX      20.0f   // a card's title: Settings, Licences
#define FUDE_SIDE_CARD_TITLE_PX 17.0f   // a small card's (a note's)
#define FUDE_SIDE_HEADER_PX     10.5f   // a section's: STUDY, THEME
#define FUDE_SIDE_ROW_PX        14.0f   // a setting's name
#define FUDE_SIDE_BODY_PX       13.0f
#define FUDE_SIDE_SMALL_PX      11.5f   // notes, the credits
#define FUDE_SIDE_LICENCE_DOCS  FUDE_APP_LICENCES   // Licences: the app's documents (Kana: Data, Fonts, Libraries, ML Kit)
#define FUDE_SIDE_LICENCE_LINES 48    // labels for the lines on screen, reused as it scrolls

RDE_STRUCT {
    struct fude_ui* ui;
    u32                  value;   // a theme, a language, a screen: what the button is for
} fude_side_theme_ref;

// A row of the notes list: which note.
RDE_STRUCT {
    struct fude_ui* ui;
    u32                  id;
} fude_side_note_ref;

// A row of the notes list as built: which note, how deep.
RDE_STRUCT {
    u32 id;
    u32 parent;
    u8  depth;
    u8  kind;
    b8  expanded;
} fude_side_row;

// The note card: what it is showing.
typedef enum {
    FUDE_SIDE_CARD_NONE = 0,
    FUDE_SIDE_CARD_ACTIONS,     // rename / delete / (a folder's) new canvas
    FUDE_SIDE_CARD_RENAME,
    FUDE_SIDE_CARD_DELETE
} FUDE_SIDE_CARD_;

// What Your data asks kana.c for.
typedef enum {
    FUDE_SIDE_DATA_NONE = 0,
    FUDE_SIDE_DATA_EXPORT,    // everything into one file, to share or save
    FUDE_SIDE_DATA_IMPORT,    // a backup picked (its question asked)
    FUDE_SIDE_DATA_REPLACE    // Replace pressed: the picked backup in place of everything
} FUDE_SIDE_DATA_;

RDE_STRUCT {
    b8              open;
    b8              settings_open;

    rde_ui_button*  menu_button;          // top-left, on the page
    fude_kit_modal  panel;                // the panel, over a backdrop: a tap on it closes it

    // The app's navigation (extension.h): its header, an entry each, and the
    // count each shows (UINT32_MAX: not yet).
    rde_ui_label*   nav_label;
    rde_ui_button*  nav[FUDE_EXTENSION_NAV];
    fude_side_theme_ref nav_refs[FUDE_EXTENSION_NAV];
    u32             _nav_counts[FUDE_EXTENSION_NAV];
    rde_ui_label*   theme_label;
    rde_ui_button*  themes[FUDE_THEME_COUNT];
    fude_side_theme_ref theme_refs[FUDE_THEME_COUNT];
    // The language: a flag and its name each (text.h's list; the refs' theme: the index).
    rde_ui_label*   language_label;
    rde_ui_button*  languages[FUDE_TEXT_LANGUAGES];
    fude_side_theme_ref language_refs[FUDE_TEXT_LANGUAGES];
    rde_ui_label*   notes_label;
    rde_ui_button*  new_folder;
    rde_ui_button*  new_canvas;
    rde_ui_scroll_area* notes_list;
    fude_side_note_ref* _note_refs;       // a row's and its "…"'s, rebuilt with the list
    u32             _notes_revision;      // what the list was built for
    u32             _notes_open;
    b8              _notes_built;
    f32             _list_width;          // as laid out
    rde_vec_2F      _list_bl;             // the list's bottom-left, UI units (the panel sits at the origin)
    rde_vec_2F      _list_size;
    fude_side_row*  _rows;                // the list as built, top to bottom
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
    u8              card_mode;            // FUDE_SIDE_CARD_
    u32             card_note;
    fude_kit_modal  note;                 // the note card, over the panel (a tap on its backdrop cancels it)
    rde_ui_label*   note_title;
    rde_ui_label*   note_body;
    rde_ui_text_editor* note_field;
    rde_ui_button*  note_rename;
    rde_ui_button*  note_add;             // a folder's: a new canvas in it
    rde_ui_button*  note_add_folder;      // a folder's: a new folder in it
    rde_ui_button*  note_delete;
    rde_ui_button*  note_cancel;
    rde_ui_button*  note_confirm;         // Save / Delete
    rde_ui_label*   version;
    rde_ui_button*  settings_button;

    // Settings.
    fude_kit_modal  settings;
    rde_ui_label*   settings_title;
    rde_ui_scroll_area* settings_body;   // all between the title and the buttons: scrolled when the card is short (a phone)
    rde_ui_label*   width_label;
    rde_ui_button*  width_even;
    rde_ui_button*  width_pressure;
    rde_ui_label*   paper_label;        // lines' and squares' size (canvas.h), for every canvas
    rde_ui_button*  paper_sizes[FUDE_PAPER_SIZE_COUNT];
    rde_ui_label*   ui_size_label;      // the interface's size (app.h): Small, Medium, Large
    rde_ui_button*  ui_sizes[3];
    fude_side_theme_ref ui_size_refs[3];
    rde_ui_label*   about_label;
    rde_ui_label*   about_text;
    rde_ui_button*  settings_close;
    rde_ui_button*  licences_button;

    // Licences: a card over Settings, one document at a time, scrolled. Only the
    // lines on screen have labels (ML Kit's notices alone are ~25,000 lines).
    b8                  licences_open;
    fude_kit_modal      licences;
    rde_ui_label*       licences_title;
    rde_ui_label*       licences_copyright;   // the app's (info.h), by Close
    rde_ui_button*      licences_docs[FUDE_SIDE_LICENCE_DOCS];
    fude_side_theme_ref licences_refs[FUDE_SIDE_LICENCE_DOCS];
    rde_ui_scroll_area* licences_text;
    rde_ui_label*       licences_lines[FUDE_SIDE_LICENCE_LINES];
    rde_ui_button*      licences_close;
    u32                 licences_doc;
    c8*                 _licence_text;    // the document shown
    u32*                _licence_starts;  // where each of its wrapped lines starts (and one more: the end)
    u32                 _licence_count;   // lines
    i32                 _licence_first;   // the first line with a label now (-1: none placed)
    f32                 _licence_line_h;
    f32                 _licence_width;   // the scroll area's, for the lines

    rde_ui_button*      tutorial;             // in the panel, over Settings: the app's tutorial again (NULL: it has none)
    rde_ui_button*      rate;                 // over it, on a phone or tablet: the store's review page
    rde_ui_button*      app_button;           // over all, while it is wanted: the app's own (extension.h's side_button)
    b8                  _app_button_shown;

    // Your data: a card over Settings — Kana is offline, the platform's own
    // backup, Export and Import (backup.h). kana.c does the work (it owns the
    // saves): fude_side_take_data_request, and the answer in data_message.
    // FAQ & Contact, the panel's lowest row: a card over the panel with the FAQ's
    // page (when there is one) and the address to write to (info.h).
    rde_ui_button*      faq_button;
    b8                  faq_open;
    fude_kit_modal      faq;
    rde_ui_label*       faq_title;
    rde_ui_button*      faq_link;             // the FAQ's page (hidden without one)
    rde_ui_label*       faq_text;             // what the address is for
    rde_ui_button*      faq_mail;             // the address: a tap writes to it
    rde_ui_button*      faq_close;

    b8                  data_open;
    rde_ui_button*      data_button;          // in Settings, by Licences
    fude_kit_modal      data;
    rde_ui_label*       data_title;
    rde_ui_label*       data_text;
    rde_ui_label*       data_status;
    rde_ui_button*      data_export;
    rde_ui_button*      data_import;
    rde_ui_button*      data_replace;         // a picked backup: Replace (or Cancel) everything with it
    rde_ui_button*      data_cancel;
    rde_ui_button*      data_close;
    FUDE_SIDE_DATA_     data_request;
    b8                  data_confirming;      // a backup picked and whole: waiting for Replace or Cancel
    u8*                 data_backup;          // it (default std allocator), until then
    usize               data_backup_size;
    c8                  data_message[400];    // what the last Export / Import did (or the question)
    b8                  data_message_bad;
    c8                  _data_shown[400];
    b8                  _data_confirm_shown;

    rde_vec_2F      _laid_out;            // the screen size the layout is for...
    rde_vec_4I      _laid_out_insets;     // ...and the safe area (iOS reports it only after the first frames)
    b8              _shown_menu;
    rde_vec_2F      _menu_center;         // UI units, for hit tests
    rde_vec_2F      _menu_size;
} fude_side;

// A label as the panel's and Settings' are (left, middle; a longer language
// shrinks it to fit): for an app's own section of Settings (extension.h) too.
rde_ui_label* fude_side_label(struct fude_ui* _ui, rde_ui_node* _parent, const c8* _text, f32 _px);

// Builds it under _root, hidden.
void fude_side_create(struct fude_ui* _ui, rde_ui_node* _root);
// Once a frame: what shows (_full: a full-screen scene is up — no menu button,
// no panel), and a new layout when the screen changed.
void fude_side_update(struct fude_ui* _ui, b8 _full);
void fude_side_apply_theme(struct fude_ui* _ui);
// Laid out again next frame: an app's section of Settings changed its rows.
void fude_side_relayout(struct fude_ui* _ui);
// Is this UI point (bottom-left origin) the side's? While the panel or Settings
// is open, everywhere is.
b8   fude_side_hit(const struct fude_ui* _ui, rde_vec_2F _at);
void fude_side_close(struct fude_ui* _ui);
// A look's: the panel open and the note card for _note — its actions, or (_mode) renaming it.
void fude_side_look_card(struct fude_ui* _ui, u32 _note, FUDE_SIDE_CARD_ _mode);
// Back (Android's, Escape): the topmost of the panel's cards — Licences, Your
// data, a note's card, Settings — or the panel, closed as its own Close or
// Cancel does. False: none was open.
b8   fude_side_back(struct fude_ui* _ui);
// Opens Settings — and over it Licences on document _licences, if that is not
// negative (developer launch options, kana.c).
void fude_side_open_settings(struct fude_ui* _ui, i32 _licences);

// Your data, for kana.c: what was asked for, once (FUDE_SIDE_DATA_NONE: nothing);
// the answer the card shows (_bad: in the warning colour); a picked backup,
// whole, kept for Replace with its question; and the import done (or not): the
// backup let go.
FUDE_SIDE_DATA_ fude_side_take_data_request(struct fude_ui* _ui);
void fude_side_data_message(struct fude_ui* _ui, const c8* _text, b8 _bad);
void fude_side_data_confirm(struct fude_ui* _ui, u8* _backup, usize _size, const c8* _question);
void fude_side_data_done(struct fude_ui* _ui);
// Frees what the panel holds besides its widgets (before the UI is built again).
void fude_side_forget(struct fude_ui* _ui);

#endif
