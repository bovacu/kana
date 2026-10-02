#ifndef FUDE_LIBRARY
#define FUDE_LIBRARY

#include "rde.h"
#include "drawing/doc/doc.h"
#include "drawing/widgets/scroll.h"

// ===========================================================================
// The Library: documents to read and write on (doc.h).
//
//   the app's books (Kana: free lessons, in its assets) — a card each: its
//   cover, title, what it is, who made it and its licence; a tap opens it, on a
//   canvas of its own in the notes (the same one again the next time);
//   the learner's own: the canvases over a PDF they brought in, newest first —
//   a tap opens one;
//   the row: Back, From Files (a PDF or pictures), From Photos and Scan pages
//   (iOS: the photo library, the document camera — import.h).
//
// What it opens closes every screen: the page is there, the document on it.
// ===========================================================================

struct fude_app;

#define FUDE_LIBRARY_BOOKS 16u   // the app's books shown, at most
#define FUDE_LIBRARY_OWN   64u   // the learner's documents listed, at most

typedef struct {
    struct fude_app* app;
    b8               open;
    fude_scroller    scroller;
    rde_vec_2F       list_min, list_max;   // the list's area, last frame
    f32              content_h;
    rde_texture*     covers[FUDE_LIBRARY_BOOKS];
    b8               cover_tried[FUDE_LIBRARY_BOOKS];
    // As drawn last frame (screen space), for taps: the books' cards, the learner's rows.
    rde_vec_2F       book_min[FUDE_LIBRARY_BOOKS], book_max[FUDE_LIBRARY_BOOKS];
    u32              own[FUDE_LIBRARY_OWN];   // their canvases' ids
    u32              own_count;
    rde_vec_2F       own_min[FUDE_LIBRARY_OWN], own_max[FUDE_LIBRARY_OWN];
} fude_library;

void fude_library_init(fude_library* _lib, struct fude_app* _app);
void fude_library_destroy(fude_library* _lib);
void fude_library_open(fude_library* _lib);
void fude_library_close(fude_library* _lib);

void fude_library_pointer_down(fude_library* _lib, rde_vec_2F _screen, f64 _time);
void fude_library_pointer_moved(fude_library* _lib, rde_vec_2F _screen, f64 _time);
void fude_library_pointer_up(fude_library* _lib, f64 _time);
void fude_library_update(fude_library* _lib, f32 _dt);
void fude_library_render(fude_library* _lib, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// The screen (screen.h).
extern const struct fude_screen FUDE_LIBRARY_SCREEN;

#endif
