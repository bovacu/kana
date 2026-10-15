// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_HOME
#define FUDE_ZOOM_HOME

#include "rde.h"
#include "drawing/app/screen.h"
#include "drawing/ink/notes.h"
#include "drawing/widgets/scroll.h"
#include "zoom/homelist.h"

// ===========================================================================
// Home: the home screen (0.1.92), the app's canvases shown as cards (InfiNote's notebooks, InfiniWorkshop's
// projects). Each card has its cover (a picture of the canvas the page keeps beside it, <id>.cover.png), its name,
// whether it is a notebook, and when it was last left. A folder's folders come first, then its canvases, the most
// recently left first.
//
//   a tap on a card     opens the canvas (or goes into the folder)
//   a card held         shows its Rename and Delete (they open the side panel's card for it)
//   Back                goes out of a folder, or else back to the canvas
//   New                 makes a canvas in the folder shown and opens it (the app's own kind: InfiNote's is a notebook)
//
// Over it, once, comes the app's welcome (first-run guidance): a few lines and two ways in, the app's first step
// (InfiniWorkshop: its examples) or a new canvas.
//
// Where it sits: fude/zoom, a screen (drawing/app/screen.h) of the canvas apps' shell (zoom/shell.c),
// over homelist.h, which gathers its cards.
// Its main types and their lifecycle: the shell owns one fude_zoom_home, made with fude_zoom_home_init and freed with
// fude_zoom_home_destroy, and sets the fields for its own app (title, new_text, on_new, the welcome) after init.
// Covers are textures loaded as cards are shown, at most a few a frame, and unloaded when the screen closes.
// Threads: main thread only.
// Files: reads each canvas's <id>.cover.png, and (through homelist.h) its <id>.words and <id>.pages; writes none.
// ===========================================================================

#define FUDE_ZOOM_HOME_COVERS   48u      // covers kept loaded at once (the one drawn longest ago makes room)
#define FUDE_ZOOM_HOME_WELCOMED 0x04u    // fude_app.cards_read's bit for the welcome seen (readcard.h has 0x01, 0x02)
#define FUDE_ZOOM_HOME_LINES    3u       // the most lines the welcome has


// A box on screen.
typedef struct {
    rde_vec_2F min, max;
} fude_zoom_home_box;

// A canvas's cover, loaded or looked for (whose it is: the home's cover_lru's key for its slot, the canvas's id).
typedef struct {
    rde_texture* texture;
    b8           tried;    // it was looked for (when texture is NULL, there is none)
} fude_zoom_home_cover;

// The home screen.
typedef struct fude_zoom_home {
    struct fude_app* app;
    b8               open;
    u32              folder;      // the folder shown (0 for the top level)
    // Set by the app: its header, its New button's label and what it does, and its welcome.
    u32              title;       // FUDE_TEXT_
    u32              new_text;
    void           (*on_new)(struct fude_app* _app, u32 _folder);
    u32              welcome_title;                         // FUDE_TEXT_ (FUDE_TEXT_COUNT: no welcome)
    u32              welcome_lines[FUDE_ZOOM_HOME_LINES];   // (FUDE_TEXT_COUNT for an unused line)
    u32              welcome_first;                         // the label of its first step's button
    void           (*on_first)(struct fude_app* _app);
    b8               welcome;     // the welcome is up now

    fude_scroller    scroller;
    f32              content_h;
    rde_vec_2F       list_min, list_max;
    rde_arr          cards;       // fude_zoom_home_card
    rde_arr          boxes;       // fude_zoom_home_box: where each card was drawn
    // What the cards were gathered for (notes.h's revision and the folder), to gather them again when either changes.
    u32              gathered_rev;
    u32              gathered_folder;
    b8               gathered;
    // The covers loaded or looked for, one for each slot of cover_lru (a fixed number: they are textures, and only so
    // many cards show at once), which knows whose each is (its canvas's id, a u32) and which was drawn longest ago.
    fude_zoom_home_cover covers[FUDE_ZOOM_HOME_COVERS];
    rde_lru          cover_lru;

    // A press held still shows its card's buttons.
    b8               pressing;
    f64              press_at;
    rde_vec_2F       press;
    b8               press_moved;
    u32              held;        // the index of the card whose buttons are up (FUDE_ZOOM_HOME_NONE: none)
    fude_zoom_home_box chips[2];  // Rename, Delete
    b8               chips_shown;
    fude_zoom_home_box welcome_buttons[2];   // the welcome's first step, and a new canvas
} fude_zoom_home;

// No card.
#define FUDE_ZOOM_HOME_NONE 0xFFFFFFFFu

// fude_zoom_home_init makes the screen, closed, for _app with the plain canvas labels and no welcome;
// fude_zoom_home_destroy unloads its covers and frees it.
void fude_zoom_home_init(fude_zoom_home* _home, struct fude_app* _app);
void fude_zoom_home_destroy(fude_zoom_home* _home);
// fude_zoom_home_open shows the screen (back at the top level when the folder it showed is gone);
// fude_zoom_home_close hides it and its welcome, and unloads its covers, which the canvas does not need.
void fude_zoom_home_open(fude_zoom_home* _home);
void fude_zoom_home_close(fude_zoom_home* _home);
// Shows the welcome over the screen, opening it first if need be (the side panel's Tutorial; the first launch). Does
// nothing when the app has no welcome.
void fude_zoom_home_show_welcome(fude_zoom_home* _home);
// Forgets a canvas's cover after it was written again, so it is loaded afresh when next shown.
void fude_zoom_home_forget_cover(fude_zoom_home* _home, u32 _note);

// The screen's entry for the app's table of screens (screen.h).
extern const fude_screen FUDE_ZOOM_HOME_SCREEN;

#endif
