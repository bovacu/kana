// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_PAGEMENU
#define FUDE_PAGEMENU

#include "rde.h"
#include "drawing/widgets/row.h"
#include "drawing/base/icons.h"
#include "drawing/base/text.h"
#include "drawing/ink/lasso.h"

// ===========================================================================
// Page menus: the page's menus, rows of buttons (row.h) floating over it:
//
//   over a lasso selection: Cut, Copy, Duplicate, Delete, just above the selection's box (below it when there is no
//   room above), and hidden while the selection is dragged;
//
//   at a long press (a right-click on a computer): Paste, Select all, just above the finger.
//
// Those are the core's own rows. An app puts its own rows together instead (extension.h: selection_row, context_row),
// with the core's buttons where it wants them (FUDE_PAGEMENU_BUTTON_ below) and its own between them: the study
// layer's reading (study/widgets/pagetext.h) adds Copy as text, Translate, Save word and Check, and Paste text and Text
// from a photo. An app may also give a row for an area of a PDF's text that the lasso took (extension.h: text_row).
//
// Where it sits: drawing/app. The UI (ui.h) holds the fude_pagemenu (fude_ui.page) and drives it; the page opens the
// context menu at a long press.
//
// Its main types and their lifecycle: fude_pagemenu_init sets it up once, and fude_pagemenu_build builds its rows
// under the UI's root, again on every rebuild of the UI.
//
// Threads: main thread only, like all of RDE's UI.
// ===========================================================================

struct fude_app;

// The page's menus.
typedef struct fude_pagemenu {
    struct fude_app*    app;
    fude_row            selection;        // Over the lasso's selection.
    fude_row            text;             // Over an area of a PDF's text the lasso took (the app's text row, if any).
    fude_row            context;          // The page's own menu, at a long press.
    // The context menu's faces as it opened (whether there was anything to paste then).
    fude_row_face       context_faces[FUDE_ROW_BUTTONS];
    rde_vec_2F          context_canvas;   // Where it was opened, on the page: where Paste lands.

    // The button saying "Copied": its row (the selection row or the text row) and its place in it (FUDE_ROW_NONE:
    // none)...
    const fude_row*     copied_row;
    u32                 copied;
    f64                 copied_until;     // ...until this time, on the engine clock.
} fude_pagemenu;

// The core's buttons, for an app's own rows (extension.h), as row presses (row.h: fude_row_press; _self is the
// fude_pagemenu). With an app's own page (extension.h: page_kind) each is handed to the page as a FUDE_PAGE_CMD_;
// otherwise it is done on the lasso. Paste and Select all also close the context menu and put the Lasso in hand,
// since what they select is then ready to drag.
void fude_pagemenu_on_cut(struct fude_app* _app, any _self, u32 _arg);
void fude_pagemenu_on_copy(struct fude_app* _app, any _self, u32 _arg);
void fude_pagemenu_on_duplicate(struct fude_app* _app, any _self, u32 _arg);
void fude_pagemenu_on_delete(struct fude_app* _app, any _self, u32 _arg);
void fude_pagemenu_on_paste(struct fude_app* _app, any _self, u32 _arg);
void fude_pagemenu_on_select_all(struct fude_app* _app, any _self, u32 _arg);
#define FUDE_PAGEMENU_BUTTON_CUT        { FUDE_TEXT_SEL_CUT,       FUDE_ICON_CUT,        fude_pagemenu_on_cut,        0, FUDE_ROW_QUIET,  false, NULL }
#define FUDE_PAGEMENU_BUTTON_COPY       { FUDE_TEXT_SEL_COPY,      FUDE_ICON_COPY,       fude_pagemenu_on_copy,       0, FUDE_ROW_QUIET,  false, NULL }
#define FUDE_PAGEMENU_BUTTON_DUPLICATE  { FUDE_TEXT_SEL_DUPLICATE, FUDE_ICON_DUPLICATE,  fude_pagemenu_on_duplicate,  0, FUDE_ROW_QUIET,  false, NULL }
#define FUDE_PAGEMENU_BUTTON_DELETE     { FUDE_TEXT_DELETE,        FUDE_ICON_TRASH,      fude_pagemenu_on_delete,     0, FUDE_ROW_DANGER, false, NULL }
#define FUDE_PAGEMENU_BUTTON_PASTE      { FUDE_TEXT_CTX_PASTE,      FUDE_ICON_PASTE,      fude_pagemenu_on_paste,      0, FUDE_ROW_QUIET, false, NULL }
#define FUDE_PAGEMENU_BUTTON_SELECT_ALL { FUDE_TEXT_CTX_SELECT_ALL, FUDE_ICON_SELECT_ALL, fude_pagemenu_on_select_all, 0, FUDE_ROW_QUIET, false, NULL }

// Sets the menus up, empty, for _app; call it once, before the UI is built.
void fude_pagemenu_init(fude_pagemenu* _menu, struct fude_app* _app);
// Builds the rows under _root, the app's own where it has them; called again on every rebuild of the UI.
void fude_pagemenu_build(fude_pagemenu* _menu, rde_ui_node* _root);
// Restyles the rows from the theme.
void fude_pagemenu_restyle(fude_pagemenu* _menu);
// Called once a frame: runs the app's own work first (extension.h: menu_update), then places the selection's menu
// (or hides it, and closes the context menu, under a screen when _hidden), and shows its buttons' faces.
void fude_pagemenu_update(fude_pagemenu* _menu, b8 _hidden);
// Whether the point _ui (UI canvas units) is on one of the menus.
b8   fude_pagemenu_hit(const fude_pagemenu* _menu, rde_vec_2F _ui);
// Makes the button whose press is _press say "Copied" for a moment (Copy, or an app's own: Copy as text). It is
// looked for in the selection row, then in the text row (an area of a PDF's text), so whichever row was pressed shows
// it.
void fude_pagemenu_copied(fude_pagemenu* _menu, fude_row_press _press);

// Opens the context menu for a long press at _screen (the app's screen space); _canvas is the same point on the page,
// where Paste lands. It floats just above the finger, and is closed by choosing an item or by
// fude_pagemenu_close_context (any other press).
void fude_pagemenu_open_context(fude_pagemenu* _menu, rde_vec_2F _screen, rde_vec_2F _canvas);
void fude_pagemenu_close_context(fude_pagemenu* _menu);

#endif
