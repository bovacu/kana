// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_DOCBAR
#define FUDE_DOCBAR

#include "rde.h"

// ===========================================================================
// Document bar: while a canvas over a document is on the page (doc.h), a small bar at the top of the screen with:
//
//   Search: a field for the words, where Return finds them through the whole PDF (its own text, and the text read
//   from its pictures); how many matches and which one is shown ("3 / 12"); the one before and the next, wrapping
//   round; and Close;
//   the page in the middle of the screen, "3 / 74": a tap asks for a page number, and Return goes there;
//   Turn page: turns the page in the middle a quarter turn clockwise (to set a sideways scan upright), with what is
//   written on it;
//   Export: the document as a PDF with what is written on it (doc.h), shared through a device's share sheet or, on a
//   computer, saved where the learner says.
//
// It is hidden under a screen, and on a canvas without a document.
//
// Where it sits: drawing/app. The UI (ui.h) holds the fude_docbar (fude_ui.docbar) and drives it; it works on the
// page's document (page.h: fude_page_input.doc).
//
// Its main types and their lifecycle: fude_docbar_create builds its widgets under the UI's root; they belong to the
// UI tree and are built again on every rebuild of the UI.
//
// Threads: main thread only, like all of RDE's UI.
//
// Files: Export writes a PDF, to the outbox on a device (session.h) or where the learner chose on a computer.
// ===========================================================================

struct fude_app;

// What the bar is showing.
typedef enum {
    FUDE_DOCBAR_IDLE = 0,   // Search, the page, Turn page and Export.
    FUDE_DOCBAR_SEARCH,     // The words, the matches, before and next, Close.
    FUDE_DOCBAR_PAGE        // A page's number, Close.
} FUDE_DOCBAR_;

// The document bar. The fields are docbar.c's; the UI only reads `shown`.
typedef struct {
    struct fude_app*    app;
    u8                  mode;          // FUDE_DOCBAR_
    b8                  shown;
    rde_ui_image*       panel;
    rde_ui_button*      search;
    rde_ui_button*      page;          // "3 / 74"
    rde_ui_button*      share;         // Export.
    rde_ui_button*      turn;          // Turns the page in the middle a quarter turn clockwise.
    rde_ui_text_editor* field;         // The words, or the page's number.
    rde_ui_label*       count;         // "3 / 12" or "No matches" while searching; "/ 74" while asking for a page.
    rde_ui_button*      before;
    rde_ui_button*      next;
    rde_ui_button*      close;
    rde_vec_2F          center, size;  // The panel's place, in UI canvas units (for hit tests).
    // What the page button says, packed as the page's number | (the page count << 16).
    // What the page label and the count say now, kept as the numbers they are made from and compared whole (so two
    // different states never look alike), so the labels change only when what they say does.
    u32                 _page_said[2];    // The page in the middle, and how many.
    u32                 _count_said[5];   // The mode, the pages, the matches, the one shown, and done and searching.
    u8                  _mode_laid;    // The mode laid out (0xFF when not laid out yet).
    rde_vec_2F          _laid_for;     // The screen size it was laid out for...
    rde_vec_4I          _insets_for;   // ...and the safe area.
} fude_docbar;

// Builds the bar under _root, hidden.
void fude_docbar_create(fude_docbar* _bar, rde_ui_node* _root, struct fude_app* _app);
// Called once a frame: shows the bar over a document's canvas (not when _hidden, under a screen or the side panel),
// and keeps what it says up to date. Hiding it ends a search or a page request.
void fude_docbar_update(fude_docbar* _bar, b8 _hidden);
// Restyles the bar from the theme.
void fude_docbar_restyle(fude_docbar* _bar);
// Whether the point _ui (UI canvas units) is on the bar while it shows.
b8   fude_docbar_hit(const fude_docbar* _bar, rde_vec_2F _ui);
// Opens Search with _words in its field and searches for them, as Return would (the look harness uses it).
void fude_docbar_search(fude_docbar* _bar, const c8* _words);
// For Back (Android's, or Escape): closes the search or the page request, as its Close does. Returns false when it
// was showing neither.
b8   fude_docbar_back(fude_docbar* _bar);

#endif
