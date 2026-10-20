// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_LIBRARY
#define FUDE_LIBRARY

#include "rde.h"
#include "drawing/doc/doc.h"
#include "drawing/widgets/scroll.h"

// ===========================================================================
// Library: the screen of documents to read and write on (doc.h). It lists:
//
//   the app's books (free lessons, say, in its assets), a card each with its cover, title, what it is, and who made
//   it and its licence. A tap opens the book on a canvas of its own in the notes (the same canvas again the next
//   time);
//   the learner's own documents: the canvases over a PDF they brought in, newest first. A tap opens one;
//   and its row: Back, From Files (a PDF or pictures), From Photos and Scan pages (the photo library and the document
//   camera: import.h).
//
// What it opens closes every screen, so the page is there with the document on it. While it is open it also takes
// what an import brings in.
//
// Where it sits: drawing/app. It is a screen (screen.h: FUDE_LIBRARY_SCREEN) that the study apps' shell puts in its
// table; it opens canvases through documents.h and imports through import.h.
//
// Its main types and their lifecycle: the shell holds one fude_library (a static) for the app's life, set up by
// fude_library_init and freed by fude_library_destroy.
//
// Threads: main thread only.
//
// Files: loads the books' covers (PNGs in the app's assets) the first time each is drawn.
// ===========================================================================

struct fude_app;

#define FUDE_LIBRARY_BOOKS 16u   // The most of the app's books shown (from its extension; a study app has a handful).

// One of the learner's documents, as listed: its canvas's id, and its row as drawn last frame (screen space, for
// taps).
typedef struct {
    u32        id;
    rde_vec_2F min, max;
} fude_library_own;

// The Library screen's state. The fields are library.c's.
typedef struct {
    struct fude_app* app;
    b8               open;
    fude_scroller    scroller;
    rde_vec_2F       list_min, list_max;   // The list's area, last frame.
    f32              content_h;
    // Each book's cover once loaded (NULL: none), and whether loading was tried, so a missing one is not tried again.
    rde_texture*     covers[FUDE_LIBRARY_BOOKS];
    b8               cover_tried[FUDE_LIBRARY_BOOKS];
    // As drawn last frame (screen space), for taps: the books' cards (the learner's rows are in `own`).
    rde_vec_2F       book_min[FUDE_LIBRARY_BOOKS], book_max[FUDE_LIBRARY_BOOKS];
    // The learner's documents, newest first, gathered again each frame it is drawn (every one: there is no limit).
    rde_arr TYPE(fude_library_own) own;
} fude_library;

// fude_library_init sets the Library up, closed, for _app; fude_library_destroy frees its list and its covers.
void fude_library_init(fude_library* _lib, struct fude_app* _app);
void fude_library_destroy(fude_library* _lib);
// Opens the Library scrolled to its top, or closes it (the row's Back, Escape).
void fude_library_open(fude_library* _lib);
void fude_library_close(fude_library* _lib);

// The screen's pointer (screen space) and frame, as its fude_screen calls them: the list scrolls, a tap opens what is
// under it, and the update also takes an import that came in. fude_library_render draws it between _top and _bottom.
void fude_library_pointer_down(fude_library* _lib, rde_vec_2F _screen, f64 _time);
void fude_library_pointer_moved(fude_library* _lib, rde_vec_2F _screen, f64 _time);
void fude_library_pointer_up(fude_library* _lib, f64 _time);
void fude_library_update(fude_library* _lib, f32 _dt);
void fude_library_render(fude_library* _lib, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// The Library as a screen (screen.h), with its row of Back, From Files, From Photos and Scan pages. Its slot's self
// is the fude_library.
extern const struct fude_screen FUDE_LIBRARY_SCREEN;

#endif
