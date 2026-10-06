// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_EXTENSION
#define FUDE_EXTENSION

#include "rde.h"
#include "drawing/widgets/row.h"
#include "drawing/base/save.h"

// ===========================================================================
// What an app adds to the drawing core (fude_app.ext): the page, the toolbar,
// the side panel and Settings are the core's, and each has a place the app
// fills — its own tools on the bar, its rows of the page's menus, its entries
// in the side panel and its sections in Settings — and hooks where its own
// widgets, drawing, files and settings join the core's.
//
// Every field may be left out (zero): a plain drawing app is the core alone.
// The study layer fills most of them for a study app (study/app/study.h:
// FUDE_STUDY_EXTENSION), and the app adds its own (Kana: its side panel).
// ===========================================================================

struct fude_app;
struct fude_ui;

struct fude_doc_line;   // doc.h

// A tool of the app's on the toolbar, after Paper (Kana's camera).
#define FUDE_EXTENSION_TOOLS   8u
#define FUDE_EXTENSION_CHOICES 16u

// One of a tool's choices, in its panel: its name, its icon, and what choosing
// it does — true: the panel stays open (a toggle among its choices).
typedef struct {
    u32       text;                       // FUDE_TEXT_
    const c8* icon;                       // icons.h
    b8      (*press)(struct fude_app* _app, u32 _index);
} fude_extension_choice;

typedef struct {
    u32       text;                       // its name (FUDE_TEXT_): the button's label, kept hidden
    const c8* icon;                       // icons.h
    void    (*press)(struct fude_app* _app);   // the bar's panels are closed first, the UI catches up after
    b8      (*available)(void);           // NULL: always; false: left out of the bar, not shown dead
    // Choices in a panel beside the bar, which the tool opens and closes like
    // Paper's (press is not called then; NULL: none) — Sketching's shapes —,
    // which of them show chosen, and whether the tool itself does: a tool of
    // the app's own that the pen uses, the bar's (pen, eraser...) not chosen
    // while it is. Choosing one of the bar's tools counts (fude_toolbar.tool_taps).
    const fude_extension_choice* choices;
    u32       choice_count;               // at most FUDE_EXTENSION_CHOICES
    b8      (*chosen)(const struct fude_app* _app, u32 _index);
    b8      (*selected)(const struct fude_app* _app);
} fude_extension_tool;

// An entry of the side panel's navigation: a button that opens what is the app's.
#define FUDE_EXTENSION_NAV 8u
typedef struct {
    u32       text;                       // FUDE_TEXT_
    const c8* icon;                       // icons.h, or a character
    f32       icon_px;
    // Pressed (_arg: the entry's own). False: the panel stays open (Kana's Reviews with none due).
    b8      (*press)(struct fude_app* _app, u32 _arg);
    u32       arg;
    b8      (*enabled)(const struct fude_app* _app);   // NULL: always (asked as the panel is built)
    // A count after its name (counted_text, "Reviews · {0}"), asked each frame the
    // panel is open — the hook keeps it cheap. NULL, or 0: just its name.
    u32     (*count)(struct fude_app* _app);
    u32       counted_text;
} fude_extension_nav_entry;

typedef struct {
    u32                             title;     // its header (FUDE_TEXT_)
    const fude_extension_nav_entry* entries;
    u32                             count;     // at most FUDE_EXTENSION_NAV
} fude_extension_nav;

// A section of the app's own in Settings, between Paper and About (Kana's
// Handwriting: Google ML Kit). Its widgets are its own, kept where it likes.
typedef struct {
    // Built on Settings' card (and again when the language changes).
    void (*build)(struct fude_ui* _ui, rde_ui_node* _card);
    // Placed from _y down, in the card's units (_margin each side of a card _card_w
    // wide): where the next goes.
    f32  (*layout)(struct fude_ui* _ui, f32 _y, f32 _margin, f32 _card_w);
    // What it shows, as things are now: once a frame while Settings is open (only
    // a change is shown), and _force as Settings opens or restyles.
    void (*refresh)(struct fude_ui* _ui, b8 _force);
    void (*restyle)(struct fude_ui* _ui);
} fude_extension_section;

#define FUDE_EXTENSION_SECTIONS 2u

// A button of the app's in the side panel, over the tutorial's: its words and
// icon, shown while shown() says so (asked each frame), and what a press does
// (the panel closes first). Study apps: Set up the voice, while there is none.
typedef struct fude_extension_side_button {
    u32       text;   // FUDE_TEXT_ id
    const c8* icon;   // icons.h
    b8      (*shown)(struct fude_app* _app);
    void    (*press)(struct fude_app* _app);
} fude_extension_side_button;

// A page of the app's own in place of the ink page (Sketching's deep-zoom
// canvas, zoom/page.h). The core keeps its ink as the brush's settings — the
// toolbar's colours and sizes, Settings' width — but what is drawn, undone,
// cleared and saved is the app's: the toolbar's buttons and the session ask it.
// The shell hands it the pen, the fingers and the frame itself.
typedef struct fude_page_kind {
    b8   (*undo)(struct fude_app* _app);
    b8   (*redo)(struct fude_app* _app);
    b8   (*can_undo)(const struct fude_app* _app);
    b8   (*can_redo)(const struct fude_app* _app);
    void (*clear)(struct fude_app* _app);        // undoable
    void (*reset_view)(struct fude_app* _app);
    // The session: the canvas _canvas opened (its notes.h id: on launch, when
    // another is chosen, after an import); a number that moves with every change
    // worth saving; busy (a stroke open: not now); saved — _leaving: the canvas
    // is being left or the app is going, so everything goes to disk now.
    void (*open)(struct fude_app* _app, u32 _canvas);
    u32  (*revision)(const struct fude_app* _app);
    b8   (*busy)(const struct fude_app* _app);
    b8   (*save)(struct fude_app* _app, b8 _leaving);
    // Its lasso's selection, for the page's menus (pagemenu.h): its box on the
    // app's screen (centre origin, Y up) and whether the pen is still at it (the
    // menu hides) — false: nothing selected. The menus' buttons on it
    // (FUDE_PAGE_CMD_; _at: where Paste lands, on the app's screen), and whether
    // there is anything to paste.
    b8   (*selection)(const struct fude_app* _app, rde_vec_2F* _min, rde_vec_2F* _max, b8* _busy);
    void (*command)(struct fude_app* _app, u32 _cmd, rde_vec_2F _at);
    b8   (*can_paste)(const struct fude_app* _app);
    // What the pickers brought (import.h: Files, Photos, the camera's scan) onto
    // the page — Sketching's pictures — instead of a canvas of its own. _paths:
    // the files, in order (the pickers' copies are deleted after).
    void (*imported)(struct fude_app* _app, u8 _kind, const c8* const* _paths, u32 _count);
    // Back (Android's, the desktop's Escape) for a mode of the page's own — Sketching's presenting, its map —
    // before a selection is let go: true, it closed one (optional).
    b8   (*back)(struct fude_app* _app);
} fude_page_kind;

typedef enum {
    FUDE_PAGE_CMD_CUT = 0,
    FUDE_PAGE_CMD_COPY,
    FUDE_PAGE_CMD_DUPLICATE,
    FUDE_PAGE_CMD_DELETE,
    FUDE_PAGE_CMD_PASTE,
    FUDE_PAGE_CMD_SELECT_ALL,
    FUDE_PAGE_CMD_DESELECT
} FUDE_PAGE_CMD_;

typedef struct fude_extension {
    // The page, when it is the app's own (NULL: the ink page).
    const fude_page_kind* page_kind;

    // The toolbar: the app's tools.
    const fude_extension_tool* tools;
    u32                        tool_count;   // at most FUDE_EXTENSION_TOOLS
    // The brush always sized on the screen (fude_ink.brush_scale SCREEN), and
    // the toolbar's Page/Screen left out — Sketching's, whose pen is the same
    // size on screen at any zoom.
    b8                         brush_on_screen;

    // The page's menus (pagemenu.h): the row over a lasso selection and the one at
    // a long press (NULL: the core's own — Cut, Copy, Duplicate, Delete; Paste,
    // Select all). An app's rows use the core's buttons where it wants them
    // (FUDE_PAGEMENU_BUTTON_CUT...). Once a frame, before the selection's row
    // shows: its own work (menu_update) and its buttons' faces; at a long press,
    // the context row's faces (the core's own are set first).
    const fude_row_def* selection_row;
    const fude_row_def* context_row;
    // Over an area of a PDF's text the lasso took (no ink: lasso.h) — the app's
    // text buttons (Kana: Copy as text, Translate, Save word). None: the lasso
    // takes ink only.
    const fude_row_def* text_row;
    void (*menu_update)(struct fude_app* _app);
    void (*selection_faces)(struct fude_app* _app, const fude_row_def* _row, fude_row_face* _faces);
    void (*context_faces)(struct fude_app* _app, const fude_row_def* _row, fude_row_face* _faces);

    // The side panel: its navigation (NULL: none), its sections in Settings, and
    // the tutorial (NULL: no such button).
    const fude_extension_nav*     nav;
    const fude_extension_section* sections;
    u32                           section_count;   // at most FUDE_EXTENSION_SECTIONS
    void (*tutorial)(struct fude_app* _app);
    const fude_extension_side_button* side_button;   // NULL: none

    // Its own widgets on the UI canvas (ui.h), over the core's: built (and again
    // when the language changes), let go before the canvas goes, once a frame
    // (_full: a screen is up), restyled, and whether a point is on them (_screen:
    // the app's screen space; _canvas: the UI canvas's).
    void (*ui_build)(struct fude_ui* _ui, rde_ui_node* _root);
    void (*ui_forget)(struct fude_ui* _ui);
    void (*ui_update)(struct fude_ui* _ui, b8 _full);
    void (*ui_restyle)(struct fude_ui* _ui);
    b8   (*ui_hit)(const struct fude_ui* _ui, rde_vec_2F _screen, rde_vec_2F _canvas);

    // Over the page: what it draws above the ink, and a press there (true: it
    // was on what it drew — the page still gets it).
    void (*page_render)(struct fude_app* _app, rde_window* _window);
    b8   (*page_press)(struct fude_app* _app, rde_vec_2F _screen);

    // The session (session.h): its files opened (_dir: the save folder), before
    // the settings are read; its settings into and out of the settings file; and
    // the very first launch (no settings yet).
    void (*session_open)(struct fude_app* _app, const c8* _dir);
    void (*settings_gather)(const struct fude_app* _app, fude_settings* _settings);
    void (*settings_apply)(struct fude_app* _app, const fude_settings* _settings);
    void (*first_launch)(struct fude_app* _app);

    // Its library (doc.h): books in its assets, to read and write on — the
    // Library screen lists them (Kana: its lectures). None: no books there.
    const struct fude_doc_book* library;
    u32                         library_count;

    // Reading pictures: a document's pages with no text of their own (scans,
    // photos), read once each so the lasso and Search find their words (doc.h).
    // _rgba (_w x _h, 4 bytes a pixel, the top row first) read — false when it
    // cannot (now: then it is not asked again until the document opens again).
    // Then once a frame: true once read, its lines into _out (at most _max: their
    // boxes in the picture's pixels, from its top-left) and how many into *_count.
    // None: pictures are not read (Kana: ML Kit's Japanese text recognition).
    b8 (*picture_read)(const u8* _rgba, u32 _w, u32 _h);
    b8 (*picture_lines)(struct fude_doc_line* _out, u32 _max, u32* _count);
} fude_extension;

#endif
