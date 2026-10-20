// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_EXTENSION
#define FUDE_EXTENSION

#include "rde.h"
#include "drawing/widgets/row.h"
#include "drawing/app/settings.h"

// ===========================================================================
// Extension: what an app adds to the drawing core (fude_app.ext). The page, the toolbar, the side panel and Settings
// are the core's, and each has a place the app fills: its own tools on the bar, its rows of the page's menus, its
// entries in the side panel and its sections in Settings. Hooks join its own widgets, drawing, files and settings to
// the core's.
//
// Every field may be left out (zero): a plain drawing app is the core alone. The study layer fills most of them for a
// study app (study/app/study.h: FUDE_STUDY_EXTENSION), and the app adds its own (its side panel, for one).
//
// Where it sits: drawing/app. The app gives a constant fude_extension in fude_app.ext; the core reads it through
// fude_app_ext (app.h), which never returns NULL.
//
// Threads: every hook is called on the main thread.
// ===========================================================================

struct fude_app;
struct fude_ui;

// The tools of the app's own on the toolbar, after Paper (a camera, say): at most FUDE_EXTENSION_TOOLS (an app with
// fewer keeps the bar as it was), each with at most FUDE_EXTENSION_CHOICES choices.
#define FUDE_EXTENSION_TOOLS   12u
#define FUDE_EXTENSION_CHOICES 24u

// One of a tool's choices, in its panel: its name, its icon, and what choosing it does. press returns true to keep
// the panel open (a toggle among its choices).
typedef struct {
    u32       text;                       // A FUDE_TEXT_ id.
    const c8* icon;                       // From icons.h.
    b8      (*press)(struct fude_app* _app, u32 _index);
} fude_extension_choice;

// A tool of the app's own on the toolbar.
typedef struct {
    u32       text;                       // Its name (a FUDE_TEXT_ id): the button's label, kept hidden.
    const c8* icon;                       // From icons.h.
    // Called on a press, after the bar's panels are closed; the UI catches up after it. NULL for a tool whose press
    // only opens its panel (choices).
    void    (*press)(struct fude_app* _app);
    // NULL means always; false leaves it out of the bar (asked at each layout), rather than showing it greyed out.
    b8      (*available)(void);
    // Choices in a panel beside the bar, which the tool opens and closes like Paper's (press is not called then;
    // NULL: no panel), as the shapes tool does. chosen says which of them show chosen, and selected whether the tool
    // itself does: a tool of the app's own that the pen uses, while the bar's own tools (pen, eraser...) show not
    // chosen. Choosing one of the bar's tools counts in fude_toolbar.tool_taps, so the app can let go of its own.
    const fude_extension_choice* choices;
    u32       choice_count;               // At most FUDE_EXTENSION_CHOICES.
    b8      (*chosen)(const struct fude_app* _app, u32 _index);
    b8      (*selected)(const struct fude_app* _app);
    // It sits on the bar just above Paper, not with the app's other tools (a page's size).
    b8        by_paper;
} fude_extension_tool;

// An entry of the side panel's navigation: a button that opens something of the app's. At most FUDE_EXTENSION_NAV.
#define FUDE_EXTENSION_NAV 8u
typedef struct {
    u32       text;                       // A FUDE_TEXT_ id.
    const c8* icon;                       // From icons.h, or a character.
    f32       icon_px;
    // Called on a press, with the entry's own arg. Returning false keeps the panel open (Reviews, with none due).
    b8      (*press)(struct fude_app* _app, u32 _arg);
    u32       arg;
    b8      (*enabled)(const struct fude_app* _app);   // NULL means always; asked as the panel is built.
    // A count shown after its name (counted_text, "Reviews · {0}"). It is asked each frame the panel is open, so the
    // hook must be cheap. NULL, or a count of 0, shows just its name.
    u32     (*count)(struct fude_app* _app);
    u32       counted_text;
} fude_extension_nav_entry;

// The side panel's navigation: a header and its entries.
typedef struct {
    u32                             title;     // Its header (a FUDE_TEXT_ id).
    const fude_extension_nav_entry* entries;
    u32                             count;     // At most FUDE_EXTENSION_NAV.
} fude_extension_nav;

// A section of the app's own in Settings, between Paper and About (the core's Handwriting, handwriting.h; the canvas's
// Measurements). Its widgets are its own, kept where it likes.
typedef struct {
    // Builds the section on Settings' card (and again when the language changes).
    void (*build)(struct fude_ui* _ui, rde_ui_node* _card);
    // Places the section from _y down, in the card's units (_margin on each side of a card _card_w wide), and returns
    // where the next one goes.
    f32  (*layout)(struct fude_ui* _ui, f32 _y, f32 _margin, f32 _card_w);
    // Shows things as they are now: called once a frame while Settings is open (it should change only what changed),
    // and with _force as Settings opens or restyles.
    void (*refresh)(struct fude_ui* _ui, b8 _force);
    void (*restyle)(struct fude_ui* _ui);
} fude_extension_section;

#define FUDE_EXTENSION_SECTIONS 3u   // The most sections an app adds to Settings.

// A paper of the app's own (canvas.h's FUDE_PAPER_ values past the core's four), offered in the Paper panel after the
// core's.
typedef struct {
    u8        paper;   // FUDE_PAPER_
    u32       text;    // Its name: a FUDE_TEXT_ id.
    const c8* icon;    // From icons.h.
} fude_extension_paper;

// A button of the app's in the side panel, above the tutorial's: its words and icon, shown while shown() says so
// (asked each frame), and what a press does (the panel closes first). The study apps' is Set up the voice, while
// there is no voice.
typedef struct fude_extension_side_button {
    u32       text;   // A FUDE_TEXT_ id.
    const c8* icon;   // From icons.h.
    b8      (*shown)(struct fude_app* _app);
    void    (*press)(struct fude_app* _app);
} fude_extension_side_button;

// A page of the app's own in place of the ink page (the canvas apps' deep-zoom canvas, zoom/page.h). The core keeps its
// ink as the brush's settings (the toolbar's colours and sizes, Settings' width), but what is drawn, undone, cleared
// and saved is the app's: the toolbar's buttons and the session ask it. The shell hands it the pen, the fingers and
// the frame itself. Every hook is required but back: the core calls them without asking whether they are set, since a
// page of its own that could not undo, open or save would lose work silently, and the one page kind there is
// (zoom/page.h's FUDE_ZOOM_PAGE_KIND) fills them all.
typedef struct fude_page_kind {
    b8   (*undo)(struct fude_app* _app);
    b8   (*redo)(struct fude_app* _app);
    b8   (*can_undo)(const struct fude_app* _app);
    b8   (*can_redo)(const struct fude_app* _app);
    void (*clear)(struct fude_app* _app);        // Undoable.
    void (*reset_view)(struct fude_app* _app);
    // For the session: open is called when canvas _canvas (its notes.h id) opens, at launch, when another is chosen,
    // or after an import. revision returns a number that moves with every change worth saving. busy says not to save
    // now (a stroke is open). save writes it and returns false on failure; _leaving means the canvas is being left or
    // the app is going, so everything goes to disk now.
    void (*open)(struct fude_app* _app, u32 _canvas);
    u32  (*revision)(const struct fude_app* _app);
    b8   (*busy)(const struct fude_app* _app);
    b8   (*save)(struct fude_app* _app, b8 _leaving);
    // Its lasso's selection, for the page's menus (pagemenu.h). selection returns false when nothing is selected;
    // otherwise it gives the box on the app's screen (centre origin, Y up) and whether the pen is still at it (then
    // the menu hides). command does a menu button's work on it (a FUDE_PAGE_CMD_; _at is where Paste lands, on the
    // app's screen), and can_paste says whether there is anything to paste.
    b8   (*selection)(const struct fude_app* _app, rde_vec_2F* _min, rde_vec_2F* _max, b8* _busy);
    void (*command)(struct fude_app* _app, u32 _cmd, rde_vec_2F _at);
    b8   (*can_paste)(const struct fude_app* _app);
    // Takes what the pickers brought (import.h: Files, Photos, the camera's scan) onto the page (the canvas apps'
    // pictures), instead of making a canvas of its own. _kind is a FUDE_IMPORT_, and _paths the files, in order; the
    // pickers' copies are deleted after it returns.
    void (*imported)(struct fude_app* _app, u8 _kind, const c8* const* _paths, u32 _count);
    // Back (Android's, or the desktop's Escape) for a mode of the page's own (the canvas's presenting, its map), asked
    // before a selection is let go. It returns true when it closed one. Optional (NULL).
    b8   (*back)(struct fude_app* _app);
} fude_page_kind;

// The page menus' commands, for a page of the app's own (fude_page_kind.command).
typedef enum {
    FUDE_PAGE_CMD_CUT = 0,
    FUDE_PAGE_CMD_COPY,
    FUDE_PAGE_CMD_DUPLICATE,
    FUDE_PAGE_CMD_DELETE,
    FUDE_PAGE_CMD_PASTE,
    FUDE_PAGE_CMD_SELECT_ALL,
    FUDE_PAGE_CMD_DESELECT
} FUDE_PAGE_CMD_;

// What an app adds to the core. Every field may be zero.
typedef struct fude_extension {
    // The page, when it is the app's own (NULL: the ink page).
    const fude_page_kind* page_kind;

    // The toolbar: the app's tools.
    const fude_extension_tool* tools;
    u32                        tool_count;   // At most FUDE_EXTENSION_TOOLS.
    // The brush is always sized on the screen (fude_ink.brush_scale SCREEN), and the toolbar's Page/Screen is left
    // out: for an app whose pen should be the same size on screen at any zoom. No app sets it today (the canvas apps
    // size the pen on the canvas, as their instruments want: zoom/shell.c).
    b8                         brush_on_screen;

    // The page's menus (pagemenu.h): the row over a lasso selection and the one at a long press (NULL: the core's
    // own, Cut, Copy, Duplicate, Delete; and Paste, Select all). An app's rows use the core's buttons where it wants
    // them (FUDE_PAGEMENU_BUTTON_CUT...). Once a frame, before the selection's row shows, the core calls menu_update
    // for the app's own work and selection_faces for its buttons' faces (for the text row too); at a long press it
    // calls context_faces for the context row's faces, after setting the core's own.
    const fude_row_def* selection_row;
    const fude_row_def* context_row;
    // The app's text buttons over an area of a PDF's text the lasso took when it took no ink (lasso.h) (a study
    // app: Copy as text, Translate, Save word). NULL: the lasso takes ink only.
    const fude_row_def* text_row;
    void (*menu_update)(struct fude_app* _app);
    void (*selection_faces)(struct fude_app* _app, const fude_row_def* _row, fude_row_face* _faces);
    void (*context_faces)(struct fude_app* _app, const fude_row_def* _row, fude_row_face* _faces);

    // The side panel: its navigation (NULL: none), its sections in Settings, and the tutorial (NULL: no such button).
    const fude_extension_nav*     nav;
    const fude_extension_section* sections;
    u32                           section_count;   // At most FUDE_EXTENSION_SECTIONS.
    void (*tutorial)(struct fude_app* _app);
    const fude_extension_side_button* side_button;   // NULL when none.

    // Its own widgets on the UI canvas (ui.h), over the core's. ui_build builds them (and again when the language
    // changes); ui_forget lets go of them before the canvas goes; ui_update runs once a frame (_full: a screen is up);
    // ui_restyle restyles them; ui_hit says whether a point is on them (_screen in the app's screen space, _canvas in
    // the UI canvas's).
    void (*ui_build)(struct fude_ui* _ui, rde_ui_node* _root);
    void (*ui_forget)(struct fude_ui* _ui);
    void (*ui_update)(struct fude_ui* _ui, b8 _full);
    void (*ui_restyle)(struct fude_ui* _ui);
    b8   (*ui_hit)(const struct fude_ui* _ui, rde_vec_2F _screen, rde_vec_2F _canvas);

    // Over the page: page_render draws above the ink, and page_press is told of every press there, to act on what it
    // drew. Whether the press then also reaches the page is ui_hit's to say: what the app drew over the page counts as
    // its UI there.
    void (*page_render)(struct fude_app* _app, rde_window* _window);
    void (*page_press)(struct fude_app* _app, rde_vec_2F _screen);

    // The session (session.h): session_open opens its files (_dir: the save folder) before the settings are read;
    // settings_gather and settings_apply move its settings into and out of the settings file; and first_launch runs at
    // the very first launch (no settings yet).
    void (*session_open)(struct fude_app* _app, const c8* _dir);
    void (*settings_gather)(const struct fude_app* _app, fude_settings* _settings);
    void (*settings_apply)(struct fude_app* _app, const fude_settings* _settings);
    void (*first_launch)(struct fude_app* _app);

    // Its library (doc.h): books in its assets to read and write on, which the Library screen lists (a study app: its
    // lectures). None: there are no books there.
    const struct fude_doc_book* library;
    u32                         library_count;

    // Reading pictures: a document's pages with no text of their own (scans, photos) are read once each, so the lasso
    // and Search find their words (doc.h). picture_read starts reading _rgba (_w x _h, 4 bytes a pixel, the top row
    // first), and returns false when it cannot now (it is then not asked again until the document opens again).
    // picture_lines is then asked once a frame: it returns true once the picture is read, with every line it read
    // added to _out (an rde_arr of fude_doc_line, which the document owns and empties before asking), their boxes in
    // the picture's pixels from its top-left. NULL: pictures are not read (a study app: ML Kit's, in its language).
    b8 (*picture_read)(const u8* _rgba, u32 _w, u32 _h);
    b8 (*picture_lines)(rde_arr* _out);

    // Its own papers for the page (a canvas app's isometric, hexagons…), offered after the core's four; none means just
    // those four. paper_offered says which of them the page open now takes (NULL: all): a page's layout is offered
    // only on a notebook's pages.
    const fude_extension_paper* papers;
    u32                         paper_count;
    b8 (*paper_offered)(const struct fude_app* _app, u8 _paper);
} fude_extension;

#endif
