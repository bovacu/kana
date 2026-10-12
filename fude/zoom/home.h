// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_HOME
#define FUDE_ZOOM_HOME

#include "rde.h"
#include "drawing/app/screen.h"
#include "drawing/ink/notes.h"
#include "drawing/widgets/scroll.h"
#include "zoom/homelist.h"

// ===========================================================================
// THE HOME SCREEN (0.1.92): the app's canvases as cards — InfiNote's notebooks,
// InfiniWorkshop's projects — each with its cover (a picture of it the page
// keeps beside it: <id>.cover.png), its name, whether it is a notebook, and when
// it was last left; its folders first, the canvases most recently left first.
//
//   a tap on a card     opens the canvas (or goes into the folder)
//   a card held         its Rename and Delete (the side panel's card for it)
//   Back                out of a folder, else back to the canvas
//   New                 a canvas made in the folder shown, opened (the app's own:
//                       InfiNote's a notebook)
//
// Over it, once, the app's welcome (first-run guidance): a few lines and two
// ways in — the app's first step (InfiniWorkshop: its examples) or a new canvas.
// ===========================================================================

#define FUDE_ZOOM_HOME_COVERS   48u      // covers kept loaded at once (the longest unseen let go)
#define FUDE_ZOOM_HOME_WELCOMED 0x04u    // fude_app.cards_read: the welcome seen (readcard.h's bits are 0x01, 0x02)
#define FUDE_ZOOM_HOME_LINES    3u       // the welcome's lines, at most


typedef struct {
    rde_vec_2F min, max;
} fude_zoom_home_box;

typedef struct {
    u32          note;
    rde_texture* texture;
    b8           tried;    // (looked for: none there)
    u64          seen;     // the frame it was last drawn
} fude_zoom_home_cover;

typedef struct fude_zoom_home {
    struct fude_app* app;
    b8               open;
    u32              folder;      // the folder shown (0: the top level)
    // The app's: its header, its New button and what it does, its welcome.
    u32              title;       // FUDE_TEXT_
    u32              new_text;
    void           (*on_new)(struct fude_app* _app, u32 _folder);
    u32              welcome_title;                         // FUDE_TEXT_ (FUDE_TEXT_COUNT: no welcome)
    u32              welcome_lines[FUDE_ZOOM_HOME_LINES];   // (FUDE_TEXT_COUNT: none)
    u32              welcome_first;                         // its first step's button
    void           (*on_first)(struct fude_app* _app);
    b8               welcome;     // up now

    fude_scroller    scroller;
    f32              content_h;
    rde_vec_2F       list_min, list_max;
    rde_arr          cards;       // fude_zoom_home_card
    rde_arr          boxes;       // fude_zoom_home_box: each card where it was drawn
    u32              gathered_rev;
    u32              gathered_folder;
    b8               gathered;
    fude_zoom_home_cover covers[FUDE_ZOOM_HOME_COVERS];
    u64              frame;

    // A press held still: its card's buttons.
    b8               pressing;
    f64              press_at;
    rde_vec_2F       press;
    b8               press_moved;
    u32              held;        // the card (index) whose buttons are up; FUDE_ZOOM_HOME_NONE: none
    fude_zoom_home_box chips[2];  // Rename, Delete
    b8               chips_shown;
    fude_zoom_home_box welcome_buttons[2];   // its first step, a new canvas
} fude_zoom_home;

#define FUDE_ZOOM_HOME_NONE 0xFFFFFFFFu

void fude_zoom_home_init(fude_zoom_home* _home, struct fude_app* _app);
void fude_zoom_home_destroy(fude_zoom_home* _home);
void fude_zoom_home_open(fude_zoom_home* _home);
void fude_zoom_home_close(fude_zoom_home* _home);
// The welcome over it (the side panel's Tutorial; the first launch).
void fude_zoom_home_show_welcome(fude_zoom_home* _home);
// A canvas's cover written again: loaded afresh when next shown.
void fude_zoom_home_forget_cover(fude_zoom_home* _home, u32 _note);

extern const fude_screen FUDE_ZOOM_HOME_SCREEN;

#endif
