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
// Side panel: the side panel and Settings, the app's navigation, kept off the floating toolbar.
//
// A menu button at the top-left of the page opens a panel from the left edge, over the page, on a dimmed backdrop (a
// tap on the backdrop closes the panel). It holds:
//   the app's navigation (extension.h; Kana's Study: Kanji, Kana, Album...);
//   Notes: the folders and canvases (notes.h), folders within folders, with + Folder and + Canvas. A tap opens a
//           canvas, or opens or closes a folder. "…" on a row offers rename and delete, and for a folder a new canvas
//           or folder in it. The handle at a row's left DRAGS it: a line shows where it will go (indented to the level
//           it lands at), or a box the folder it will go into; near the list's ends the list scrolls;
//   ...and at the bottom the version on the left and Settings on the right.
// Settings is a card over everything: the theme (each shown in its own colours), the language (a flag and its name
// each), pen width, paper, the interface's size, the app's own sections (extension.h), and About (the version and the
// app's credits). Over Settings open Licences and Your data; over the panel opens FAQ & Contact.
//
// Where it sits: drawing/app. It is built on the UI's canvas (ui.h) with the kit (kit.h); the UI owns the fude_side
// (fude_ui.side) and drives it (fude_side_update, fude_side_hit, fude_side_apply_theme). The session (session.c)
// does Your data's work, and the look harness (look.c) opens its cards.
//
// Its main types and their lifecycle: fude_side_create builds the widgets under the UI's root, hidden; they belong to
// the UI tree. fude_side_forget frees what the panel holds besides its widgets, before the UI is built again or
// destroyed.
//
// Threads: main thread only, like all of RDE's UI.
//
// Files: reads the licence documents from the app's assets (info.h: fude_app_info.licences) when Licences shows one.
// ===========================================================================

#define FUDE_SIDE_WIDTH  300.0f   // The panel's width, in UI units.
// Text sizes, in the UI font's units (Slug draws an em about 1.31 times these).
#define FUDE_SIDE_TITLE_PX      20.0f   // A card's title: Settings, Licences.
#define FUDE_SIDE_CARD_TITLE_PX 17.0f   // A small card's title (a note's).
#define FUDE_SIDE_HEADER_PX     10.5f   // A section's header: STUDY, THEME.
#define FUDE_SIDE_ROW_PX        14.0f   // A setting's name.
#define FUDE_SIDE_BODY_PX       13.0f
#define FUDE_SIDE_SMALL_PX      11.5f   // Notes, the credits.
// The most documents in Licences: the app's (Kana: Data, Fonts, Libraries, ML Kit).
#define FUDE_SIDE_LICENCE_DOCS  FUDE_APP_LICENCES
#define FUDE_SIDE_LICENCE_LINES 48    // Labels for the lines on screen, reused as it scrolls.

// What a button's callback gets: the UI, and what the button is for. Every list of buttons here uses it: a theme, a
// language, a navigation entry, an interface size, a licence document.
RDE_STRUCT {
    struct fude_ui* ui;
    u32                  value;   // A theme, a language, an entry, a size or a document: the button's own value.
} fude_side_button_ref;

// What a row of the notes list (or its "…") hands its callback: which note.
RDE_STRUCT {
    struct fude_ui* ui;
    u32                  id;
} fude_side_note_ref;

// A row of the notes list as built: which note, its parent, how deep it is, what kind, whether it is expanded, and
// where it was in the notes' list (fude_notes.notes) as the list was built, which holds until the notes change and the
// list is built again.
RDE_STRUCT {
    u32 id;
    u32 parent;
    u8  depth;
    u8  kind;
    b8  expanded;
    u32 index;
} fude_side_row;

// What the note card is showing.
typedef enum {
    FUDE_SIDE_CARD_NONE = 0,
    FUDE_SIDE_CARD_ACTIONS,     // Rename, Delete, and for a folder New canvas and New folder.
    FUDE_SIDE_CARD_RENAME,
    FUDE_SIDE_CARD_DELETE
} FUDE_SIDE_CARD_;

// What Your data asks the session (session.c) to do.
typedef enum {
    FUDE_SIDE_DATA_NONE = 0,
    FUDE_SIDE_DATA_EXPORT,    // Write everything into one file, to share or save.
    FUDE_SIDE_DATA_IMPORT,    // Pick a backup, and ask whether to replace everything with it.
    FUDE_SIDE_DATA_REPLACE    // Replace was pressed: put the picked backup in place of everything.
} FUDE_SIDE_DATA_;

// The side panel, Settings and their cards. The UI reads open and settings_open; the rest is side.c's.
RDE_STRUCT {
    b8              open;
    b8              settings_open;

    rde_ui_button*  menu_button;          // At the top-left, on the page.
    fude_kit_modal  panel;                // The panel, over a backdrop: a tap on the backdrop closes it.

    // The app's navigation (extension.h): its header, a button for each entry, and the count each shows
    // (UINT32_MAX until it is first shown).
    rde_ui_label*   nav_label;
    rde_ui_button*  nav[FUDE_EXTENSION_NAV];
    fude_side_button_ref nav_refs[FUDE_EXTENSION_NAV];
    u32             _nav_counts[FUDE_EXTENSION_NAV];
    rde_ui_label*   theme_label;
    rde_ui_button*  themes[FUDE_THEME_COUNT];
    fude_side_button_ref theme_refs[FUDE_THEME_COUNT];
    // The language: a flag and its name for each (text.h's list; each ref's value is its index there).
    rde_ui_label*   language_label;
    rde_ui_button*  languages[FUDE_TEXT_LANGUAGES];
    fude_side_button_ref language_refs[FUDE_TEXT_LANGUAGES];
    rde_ui_label*   notes_label;
    rde_ui_button*  new_folder;
    rde_ui_button*  new_canvas;
    rde_ui_scroll_area* notes_list;
    // The refs of each row and its "…", rebuilt with the list.
    rde_arr TYPE(fude_side_note_ref) _note_refs;
    u32             _notes_revision;      // The notes' revision the list was built for...
    u32             _notes_open;          // ...and the canvas that was open then.
    b8              _notes_built;
    f32             _list_width;          // As laid out.
    rde_vec_2F      _list_bl;             // The list's bottom-left, in UI units (the panel sits at the origin).
    rde_vec_2F      _list_size;
    rde_arr TYPE(fude_side_row) _rows;    // The list as built, top to bottom.
    u32             _row_count;

    // Dragging a row by its handle.
    u32             drag_id;              // 0 when nothing is being dragged.
    rde_vec_2F      drag_at;              // The pointer, in UI units.
    u32             drop_parent;          // Where it would land...
    u32             drop_before;
    b8              drop_valid;
    rde_ui_image*   drop_line;            // ...shown as the line where it goes,
    rde_ui_image*   drop_box;             // or as a box around the folder it goes into.
    rde_ui_image*   drag_ghost;           // Its name, following the pointer.
    rde_ui_label*   drag_ghost_label;

    // The note card, over the panel.
    u8              card_mode;            // FUDE_SIDE_CARD_
    u32             card_note;
    fude_kit_modal  note;                 // The note card itself; a tap on its backdrop cancels it.
    rde_ui_label*   note_title;
    rde_ui_label*   note_body;
    rde_ui_text_editor* note_field;
    rde_ui_button*  note_rename;
    rde_ui_button*  note_add;             // For a folder: a new canvas in it.
    rde_ui_button*  note_add_folder;      // For a folder: a new folder in it.
    rde_ui_button*  note_delete;
    rde_ui_button*  note_cancel;
    rde_ui_button*  note_confirm;         // Save or Delete.
    rde_ui_label*   version;
    rde_ui_button*  settings_button;

    // Settings.
    fude_kit_modal  settings;
    rde_ui_label*   settings_title;
    // Everything between the title and the buttons, which scrolls when the card is short (on a phone).
    rde_ui_scroll_area* settings_body;
    rde_ui_label*   width_label;
    rde_ui_button*  width_even;
    rde_ui_button*  width_pressure;
    rde_ui_label*   paper_label;        // The size of lines and squares (canvas.h), for every canvas.
    rde_ui_button*  paper_sizes[FUDE_PAPER_SIZE_COUNT];
    rde_ui_label*   ui_size_label;      // The interface's size (app.h): Small, Medium, Large.
    rde_ui_button*  ui_sizes[3];
    fude_side_button_ref ui_size_refs[3];
    rde_ui_label*   about_label;
    rde_ui_label*   about_text;
    rde_ui_button*  settings_close;
    rde_ui_button*  licences_button;

    // Licences: a card over Settings showing one document at a time, scrolled. Only the lines on screen have labels,
    // because ML Kit's notices alone are about 25,000 lines.
    b8                  licences_open;
    fude_kit_modal      licences;
    rde_ui_label*       licences_title;
    rde_ui_label*       licences_copyright;   // The app's copyright (info.h), by Close.
    rde_ui_button*      licences_docs[FUDE_SIDE_LICENCE_DOCS];
    fude_side_button_ref licences_refs[FUDE_SIDE_LICENCE_DOCS];
    rde_ui_scroll_area* licences_text;
    rde_ui_label*       licences_lines[FUDE_SIDE_LICENCE_LINES];
    rde_ui_button*      licences_close;
    u32                 licences_doc;
    rde_arr TYPE(c8)    _licence_text;    // The document shown.
    rde_arr TYPE(u32)   _licence_starts;  // Where each of its wrapped lines starts, and one more for the end.
    u32                 _licence_count;   // The number of lines.
    i32                 _licence_first;   // The first line with a label now (-1 when none is placed).
    f32                 _licence_line_h;
    f32                 _licence_width;   // The scroll area's width, for the lines.

    // In the panel, above Settings: the app's tutorial again (NULL when it has none).
    rde_ui_button*      tutorial;
    rde_ui_button*      rate;                 // Above that, on a phone or tablet: the store's review page.
    // Above everything, while it is wanted: the app's own button (extension.h's side_button).
    rde_ui_button*      app_button;
    b8                  _app_button_shown;

    // FAQ & Contact, the panel's lowest row: a card over the panel with the FAQ's page (when there is one) and the
    // address to write to (info.h).
    rde_ui_button*      faq_button;
    b8                  faq_open;
    fude_kit_modal      faq;
    rde_ui_label*       faq_title;
    rde_ui_button*      faq_link;             // The FAQ's page (hidden when there is none).
    rde_ui_label*       faq_text;             // What the address is for.
    rde_ui_button*      faq_mail;             // The address: a tap writes to it.
    rde_ui_button*      faq_close;

    // Your data: a card over Settings explaining that the app is offline and the platform keeps its own backup, with
    // Export and Import (backup.h). The session does the work, since it owns the saves: it takes the request with
    // fude_side_take_data_request and puts its answer in data_message.
    b8                  data_open;
    rde_ui_button*      data_button;          // In Settings, by Licences.
    fude_kit_modal      data;
    rde_ui_label*       data_title;
    rde_ui_label*       data_text;
    rde_ui_label*       data_status;
    rde_ui_button*      data_export;
    rde_ui_button*      data_import;
    rde_ui_button*      data_replace;         // With a picked backup: Replace everything with it (or Cancel).
    rde_ui_button*      data_cancel;
    rde_ui_button*      data_close;
    FUDE_SIDE_DATA_     data_request;
    b8                  data_confirming;      // A backup was picked and is whole: waiting for Replace or Cancel.
    u8*                 data_backup;          // That backup (default std allocator), owned here until then.
    usize               data_backup_size;
    c8                  data_message[400];    // What the last Export or Import did (or the question).
    b8                  data_message_bad;
    c8                  _data_shown[400];
    b8                  _data_confirm_shown;

    rde_vec_2F      _laid_out;            // The screen size the layout is for...
    rde_vec_4I      _laid_out_insets;     // ...and the safe area (iOS reports it only after the first frames).
    b8              _shown_menu;
    rde_vec_2F      _menu_center;         // The menu button's place in UI units, for hit tests.
    rde_vec_2F      _menu_size;
} fude_side;

// Makes a label under _parent as the panel's and Settings' are: aligned left and middle, _px tall, and shrunk to fit
// for a longer language rather than cut. An app's own section of Settings (extension.h) uses it too.
rde_ui_label* fude_side_label(struct fude_ui* _ui, rde_ui_node* _parent, const c8* _text, f32 _px);

// Builds the menu button, the panel, Settings and their cards under _root, hidden.
void fude_side_create(struct fude_ui* _ui, rde_ui_node* _root);
// Called once a frame: shows what is open (_full means a full-screen scene is up, so there is no menu button and no
// panel), and lays everything out again when the screen changed.
void fude_side_update(struct fude_ui* _ui, b8 _full);
// Restyles every widget of the panel and its cards from the theme.
void fude_side_apply_theme(struct fude_ui* _ui);
// Asks for a new layout next frame, for when an app's section of Settings changed its rows.
void fude_side_relayout(struct fude_ui* _ui);
// Whether the UI point _at (bottom-left origin) belongs to the side panel: the menu button, or anywhere at all while
// the panel, Settings or one of their cards is open.
b8   fude_side_hit(const struct fude_ui* _ui, rde_vec_2F _at);
// Closes the panel; the UI shows it on its next update.
void fude_side_close(struct fude_ui* _ui);
// For the look harness: opens the panel and the note card for _note, in _mode (its actions, renaming it, deleting it).
void fude_side_look_card(struct fude_ui* _ui, u32 _note, FUDE_SIDE_CARD_ _mode);
// For Back (Android's, or Escape): closes the topmost of the panel's cards (Licences, Your data, FAQ & Contact, a
// note's card, Settings), or else the panel, as its own Close or Cancel does. Returns false when none was open.
b8   fude_side_back(struct fude_ui* _ui);
// Opens Settings, and over it Licences on document _licences unless that is negative (the developer launch options:
// look.c).
void fude_side_open_settings(struct fude_ui* _ui, i32 _licences);

// Your data, for the session (session.c). fude_side_take_data_request returns what was asked for, once
// (FUDE_SIDE_DATA_NONE when nothing). fude_side_data_message sets the answer the card shows (copied; _bad shows it
// in the warning colour). fude_side_data_confirm keeps a picked, whole backup for Replace and shows its question; the
// panel takes ownership of _backup, which must come from the default std allocator. fude_side_data_done lets that
// backup go, once the import is done or given up.
FUDE_SIDE_DATA_ fude_side_take_data_request(struct fude_ui* _ui);
void fude_side_data_message(struct fude_ui* _ui, const c8* _text, b8 _bad);
void fude_side_data_confirm(struct fude_ui* _ui, u8* _backup, usize _size, const c8* _question);
void fude_side_data_done(struct fude_ui* _ui);
// Frees what the panel holds besides its widgets (the notes list's arrays, the licence text), before the UI is built
// again or destroyed.
void fude_side_forget(struct fude_ui* _ui);

#endif
