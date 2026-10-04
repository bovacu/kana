// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_PAGEMENU
#define FUDE_PAGEMENU

#include "rde.h"
#include "drawing/widgets/row.h"
#include "drawing/widgets/icons.h"
#include "drawing/base/text.h"
#include "drawing/ink/lasso.h"

// ===========================================================================
// The page's menus, rows (row.h) floating over it:
//
//   over a lasso selection — Cut, Copy, Duplicate, Delete — just above the
//   selection's box (below it with no room), hidden while it is dragged;
//
//   at a long press (right-click on a computer) — Paste, Select all — just over
//   the finger.
//
// Those are the core's own rows. An app puts its own together instead
// (extension.h: selection_row, context_row), with the core's buttons where it
// wants them (FUDE_PAGEMENU_BUTTON_ below) and its own between: Kana's reading
// of the page (study/widgets/pagetext.h) — Copy as text, Translate, Save word,
// Check; Paste text, Text from a photo.
// ===========================================================================

struct fude_app;

typedef struct fude_pagemenu {
    struct fude_app*    app;
    fude_row            selection;        // over the lasso's selection
    fude_row            text;             // over an area of a PDF's text the lasso took (the app's text row)
    fude_row            context;          // the page's, at a long press
    fude_row_face       context_faces[FUDE_ROW_BUTTONS];   // as it opened (what can be pasted then)
    rde_vec_2F          context_canvas;   // where it was opened, on the page: where Paste lands

    u32                 copied;           // the button saying "Copied" (FUDE_ROW_NONE: none)...
    f64                 copied_until;     // ...until then (engine clock)
} fude_pagemenu;

// The core's buttons, for an app's rows (extension.h).
void fude_pagemenu_on_cut(struct fude_app* _app, void* _self, u32 _arg);
void fude_pagemenu_on_copy(struct fude_app* _app, void* _self, u32 _arg);
void fude_pagemenu_on_duplicate(struct fude_app* _app, void* _self, u32 _arg);
void fude_pagemenu_on_delete(struct fude_app* _app, void* _self, u32 _arg);
void fude_pagemenu_on_paste(struct fude_app* _app, void* _self, u32 _arg);
void fude_pagemenu_on_select_all(struct fude_app* _app, void* _self, u32 _arg);
#define FUDE_PAGEMENU_BUTTON_CUT        { FUDE_TEXT_SEL_CUT,       FUDE_ICON_CUT,        fude_pagemenu_on_cut,        0, FUDE_ROW_QUIET,  false, NULL }
#define FUDE_PAGEMENU_BUTTON_COPY       { FUDE_TEXT_SEL_COPY,      FUDE_ICON_COPY,       fude_pagemenu_on_copy,       0, FUDE_ROW_QUIET,  false, NULL }
#define FUDE_PAGEMENU_BUTTON_DUPLICATE  { FUDE_TEXT_SEL_DUPLICATE, FUDE_ICON_DUPLICATE,  fude_pagemenu_on_duplicate,  0, FUDE_ROW_QUIET,  false, NULL }
#define FUDE_PAGEMENU_BUTTON_DELETE     { FUDE_TEXT_DELETE,        FUDE_ICON_TRASH,      fude_pagemenu_on_delete,     0, FUDE_ROW_DANGER, false, NULL }
#define FUDE_PAGEMENU_BUTTON_PASTE      { FUDE_TEXT_CTX_PASTE,      FUDE_ICON_PASTE,      fude_pagemenu_on_paste,      0, FUDE_ROW_QUIET, false, NULL }
#define FUDE_PAGEMENU_BUTTON_SELECT_ALL { FUDE_TEXT_CTX_SELECT_ALL, FUDE_ICON_SELECT_ALL, fude_pagemenu_on_select_all, 0, FUDE_ROW_QUIET, false, NULL }

void fude_pagemenu_init(fude_pagemenu* _menu, struct fude_app* _app);
// The rows, built (again) under _root.
void fude_pagemenu_build(fude_pagemenu* _menu, rde_ui_node* _root);
void fude_pagemenu_restyle(fude_pagemenu* _menu);
// Once a frame: the app's own work first (extension.h: menu_update), the
// selection's menu placed (or hidden under a screen, _hidden), its faces.
void fude_pagemenu_update(fude_pagemenu* _menu, b8 _hidden);
// Is _ui (UI canvas units) on a menu?
b8   fude_pagemenu_hit(const fude_pagemenu* _menu, rde_vec_2F _ui);
// The selection's button that does _press says "Copied" a moment (Copy; an
// app's own: Kana's Copy as text).
void fude_pagemenu_copied(fude_pagemenu* _menu, fude_row_press _press);

// The context menu for a long press at _screen (the app's screen space);
// _canvas is the same point on the page, where Paste lands. It floats just above
// the finger. Closed by choosing an item, or by fude_pagemenu_close_context (any
// other press).
void fude_pagemenu_open_context(fude_pagemenu* _menu, rde_vec_2F _screen, rde_vec_2F _canvas);
void fude_pagemenu_close_context(fude_pagemenu* _menu);

#endif
