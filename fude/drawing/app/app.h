// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_APP
#define FUDE_APP

#include "rde.h"
#include "drawing/app/screen.h"
#include "drawing/app/info.h"
#include "drawing/app/extension.h"
#include "drawing/ink/ink.h"
#include "drawing/ink/canvas.h"
#include "drawing/ink/lasso.h"
#include "drawing/ink/notes.h"

// ===========================================================================
// App: the app as the drawing core knows it. It holds the page (and the canvases), the screens stacked over it, the
// retained UI over everything, and what the app is (info.h) and adds to the core (extension.h).
//
// Where it sits: drawing/app, the top of the drawing core. Every app's shell (study/app/shell.c, apps/draw,
// apps/sketching) fills one fude_app and drives it; nearly everything in drawing/app takes it.
//
// THE LAYERS. Models keep what is written and know nothing of the screen (ink.h, lasso.h, notes.h...). Widgets are
// pieces of UI that know no screen (kit.h, row.h, toolbar.h, side.h, filterbar.h, notice.h, draw.h, scroll.h).
// Screens are what fills the screen (screen.h): each draws itself, takes the pointer, and declares its row of buttons
// (what they say and what they do). The app owns them all, keeps the screens in order, and hands each event and frame
// to the one on top.
//
// Its main types and their lifecycle: the shell owns the fude_app (a static) for the app's whole life and fills its
// fields; the models it points to (the ink, canvas, lasso, notes and page) are the shell's too. A study app's
// fude_app is the first member of its fude_study (study/app/study.h), which holds the character data and the study's
// screens, and whose verbs are the ways from one screen to another.
//
// Threads: main thread only. fude_app_start also readies the apps' Java on Android, which must be found on this,
// the engine's, thread.
//
// Files: none of its own, but fude_app_start chooses the save folder (save.h) and installs the crash reports
// (crash.h).
// ===========================================================================

struct fude_ui;
struct fude_page_input;

#define FUDE_APP_SCREENS 16u   // The most screens an app stacks.

// The pointer the screens follow. There is one at a time: whichever pressed first.
typedef enum { FUDE_POINTER_NONE = 0, FUDE_POINTER_PEN, FUDE_POINTER_FINGER, FUDE_POINTER_MOUSE } FUDE_POINTER_;

// How big everything on screen is (Settings' Interface size), set as the size of a window unit
// (rde_window_set_ui_scale). AUTO stays until one is chosen: it is Small on a phone, because a UI laid out for tablets
// at a tablet's sizes crowds a phone's narrow screen, and Medium on anything bigger.
typedef enum {
    FUDE_UI_SIZE_AUTO = 0,
    FUDE_UI_SIZE_SMALL,
    FUDE_UI_SIZE_MEDIUM,
    FUDE_UI_SIZE_LARGE,
    FUDE_UI_SIZE_COUNT
} FUDE_UI_SIZE_;

// The app. The shell fills info, ext, the window, the fonts, the page's models and the screen table; fude_ui_init sets
// ui, the session (session.h) restores the saved fields, and app.c keeps the pointer.
typedef struct fude_app {
    const fude_app_info*  info;      // What the app is (info.h).
    const fude_extension* ext;       // What it adds to the core (extension.h; NULL when nothing).
    rde_window*      window;
    rde_font*        font;           // The UI font (the screens' text), at font_px.
    f32              font_px;

    // The page.
    fude_ink*        ink;
    fude_canvas*     canvas;
    fude_lasso*      lasso;
    fude_notes*      notes;          // The canvases and their folders.
    struct fude_page_input* page;    // Writing on the page and moving it (page.h).
    // On a tablet, one finger writes too (the toolbar's hand); a pen coming down turns it off.
    b8               finger_writes;
    // A pen has ever written in this app (saved). Until one has, a tablet starts with the hand on.
    b8               pen_ever;
    u8               cards_read;     // The must-read cards closed, one bit each (saved; readcard.h).
    u8               ui_size;        // FUDE_UI_SIZE_ (saved): how big everything is; AUTO means the device's size.

    // The screens, in the order they stack: the first open one is on top, and gets the pointer, its row and the
    // frame. The page is under them all. The order is the app's own (a study app's: FUDE_STUDY_SCREEN_, in
    // study/app/shell.h).
    fude_screen_slot screens[FUDE_APP_SCREENS];
    u32              screen_count;

    struct fude_ui*  ui;             // The retained UI (ui.h).

    // The pointer the screens follow (fude_app_screen_event): which kind, which finger, and where it last was.
    FUDE_POINTER_    pointer;
    u64              pointer_finger;
    rde_vec_2F       pointer_last;
    // The screen on top last frame, to tell when a screen comes back to the top.
    const fude_screen_slot* top_seen;
} fude_app;

// Hands the app's info to what needs it before anything else runs: the save folder on a device (from its id), the
// crash reports, and the window's title. The shell calls it as soon as the app's fields are filled. Every app has an
// info with a name: start stops the app with a clear message when it has none, so nothing after it checks.
void                  fude_app_start(fude_app* _app);
// Sets the window's units the way the apps lay out in it, before anything reads its size (the camera, the UI): on
// Android in dp, like iOS's points, so an Android tablet gets an iPad's sizes rather than half of them
// (rde_window_set_density_scaling). The shell calls it first thing.
void                  fude_app_window(rde_window* _window);
// What the app adds (extension.h): its own, or an empty one (never NULL), so callers need not check.
const fude_extension* fude_app_ext(const fude_app* _app);
// The interface's size in effect: the one chosen, or the device's (never AUTO).
FUDE_UI_SIZE_         fude_app_ui_size(const fude_app* _app);
// Sets the window's units for that size. Called each frame, since a window may be resized or a phone turned.
void                  fude_app_apply_ui_size(fude_app* _app);
// The app's id (info.h): its own, or else its name in lowercase, letters and digits only. The latter is a static
// buffer, rewritten by each call.
const c8*             fude_app_id(const fude_app* _app);

// The screen on top: the first open one in the table, whose order is the stack's. NULL when none is open and the
// page shows.
const fude_screen_slot* fude_app_top(const fude_app* _app);
// Whether screen _screen (its place in the table) is open.
b8                     fude_app_is_open(const fude_app* _app, u32 _screen);

// Called once a frame. Applies the interface's size, handles Back and Escape, resumes a screen that is back on top,
// polls the desktop mouse, and runs the top screen's update (which may hand things on). Returns false when no screen
// is open; the page's frame is then the caller's to run.
b8   fude_app_update(fude_app* _app, f32 _dt);
// fude_app_render draws the first open screen that is not an overlay, and returns false when there is none: the
// caller then draws the page. fude_app_render_overlays then draws the open overlays (the welcome) over it.
b8   fude_app_render(fude_app* _app);
void fude_app_render_overlays(fude_app* _app);
// Hands a pen, finger or mouse event to the screen on top, one pointer at a time. A press on the UI is the UI's.
void fude_app_screen_event(fude_app* _app, rde_event* _event);
// Closes every screen, each by as many steps as it has (after an import replaced what they show).
void fude_app_close_all(fude_app* _app);

// Converts a pen position (window pixels: top-left origin, Y down), or a touch's (centre origin, Y down), to the
// app's screen space (centre origin, Y up). Getting this wrong shows as ink mirrored about the middle of the screen.
rde_vec_2F fude_app_window_to_screen(const fude_app* _app, rde_vec_2F _pixel);
rde_vec_2F fude_app_touch_to_screen(rde_vec_2I _touch);

// Sets a tablet's hand on the toolbar: on, one finger writes; off, only the pen does. A notice on the page says which,
// whoever switched it.
void fude_app_set_finger_writes(fude_app* _app, b8 _on);

#endif
