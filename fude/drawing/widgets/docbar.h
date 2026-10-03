#ifndef FUDE_DOCBAR
#define FUDE_DOCBAR

#include "rde.h"

// ===========================================================================
// The document bar: while a canvas over a document is on the page (doc.h), a
// small bar at the top of the screen —
//
//   Search: a field for the words, Return finds them through the whole PDF (its
//   own text: a scan has none); how many and which one ("3 / 12"), the one
//   before and the next (round), Close;
//   the page in the middle of the screen, "3 / 74": a tap asks for a page,
//   Return goes there;
//   Turn page: the page in the middle a quarter turn clockwise (a sideways
//   scan upright), what is written on it with it;
//   Export: the document as a PDF with what is written on it (doc.h) — shared
//   (a device's share sheet) or, on a computer, saved where the learner says.
//
// Hidden under a screen, and on a canvas without a document.
// ===========================================================================

struct fude_app;

typedef enum {
    FUDE_DOCBAR_IDLE = 0,   // Search, and the page
    FUDE_DOCBAR_SEARCH,     // the words, the matches, before and next, Close
    FUDE_DOCBAR_PAGE        // a page's number, Close
} FUDE_DOCBAR_;

typedef struct {
    struct fude_app*    app;
    u8                  mode;          // FUDE_DOCBAR_
    b8                  shown;
    rde_ui_image*       panel;
    rde_ui_button*      search;
    rde_ui_button*      page;          // "3 / 74"
    rde_ui_button*      share;         // Export
    rde_ui_button*      turn;          // the page in the middle, a quarter turn clockwise
    rde_ui_text_editor* field;         // the words, or the page's number
    rde_ui_label*       count;         // "3 / 12", "No matches"; "/ 74"
    rde_ui_button*      before;
    rde_ui_button*      next;
    rde_ui_button*      close;
    rde_vec_2F          center, size;  // the panel, UI canvas units (for hits)
    u32                 _page_said;    // what page shows (its number, page count << 16)
    u32                 _count_said;   // what count shows
    u8                  _mode_laid;    // the mode laid out (0xFF: not yet)
    rde_vec_2F          _laid_for;     // the screen size laid out for
    rde_vec_4I          _insets_for;   // ...and the safe area
} fude_docbar;

void fude_docbar_create(fude_docbar* _bar, rde_ui_node* _root, struct fude_app* _app);
// Once a frame: shown over a document's canvas (not under a screen: _hidden),
// what it says kept up.
void fude_docbar_update(fude_docbar* _bar, b8 _hidden);
void fude_docbar_restyle(fude_docbar* _bar);
// Is _ui (UI canvas units) on it?
b8   fude_docbar_hit(const fude_docbar* _bar, rde_vec_2F _ui);
// Search open with _words in its field, and searched for (as Return would).
void fude_docbar_search(fude_docbar* _bar, const c8* _words);
// Back (Android's, Escape): its search or its page number closed, as its Close
// does. False: it was showing neither.
b8   fude_docbar_back(fude_docbar* _bar);

#endif
