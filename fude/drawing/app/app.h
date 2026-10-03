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
// The app, as the drawing core knows it: the page (and the canvases), the
// screens stacked over it, the retained UI over everything — and what the app
// is (info.h) and adds (extension.h).
//
// THE LAYERS. Models keep what is written and know nothing of the screen (ink.h,
// lasso.h, notes.h...). Widgets are pieces of UI that know no screen (kit.h,
// row.h, toolbar.h, side.h, filterbar.h, notice.h, draw.h, scroll.h). Screens
// are what fills the screen (screen.h): each draws itself, takes the pointer,
// and declares its row of buttons — what they say and what they do. The app
// owns them all, keeps the screens in order, and hands each event and frame to
// the one on top.
//
// A study app's fude_app is the first member of its fude_study (study/app/
// study.h), which holds the character data and the study's screens, and whose
// verbs are the ways from one of them to another.
// ===========================================================================

struct fude_ui;
struct fude_page_input;

#define FUDE_APP_SCREENS 16u   // the screens an app stacks, at most

// The pointer the screens follow: one at a time, whichever pressed first.
typedef enum { FUDE_POINTER_NONE = 0, FUDE_POINTER_PEN, FUDE_POINTER_FINGER, FUDE_POINTER_MOUSE } FUDE_POINTER_;

typedef struct fude_app {
    const fude_app_info*  info;      // what the app is (info.h)
    const fude_extension* ext;       // what it adds to the core (extension.h; NULL: nothing)
    rde_window*      window;
    rde_font*        font;           // the UI font (the screens' text), at font_px
    f32              font_px;

    // The page.
    fude_ink*        ink;
    fude_canvas*     canvas;
    fude_lasso*      lasso;
    fude_notes*      notes;          // the canvases and their folders
    struct fude_page_input* page;    // writing on it, moving it (page.h)
    b8               finger_writes;  // a tablet: one finger writes too (the toolbar's hand); a pen coming down turns it off
    b8               pen_ever;       // a pen has written here, ever (saved): until one has, a tablet starts with the hand on

    // The screens, in the order they stack: the first open one is on top (and
    // gets the pointer, its row, the frame). The page is under them all. The
    // app's order (Kana: KANA_SCREEN_).
    fude_screen_slot screens[FUDE_APP_SCREENS];
    u32              screen_count;

    struct fude_ui*  ui;             // the retained UI (ui.h)

    // The screens' pointer (fude_app_screen_event).
    FUDE_POINTER_    pointer;
    u64              pointer_finger;
    rde_vec_2F       pointer_last;
    const fude_screen_slot* top_seen;   // the screen on top last frame (to tell one coming back to the top)
} fude_app;

// The app's info given to what needs it before anything else runs — its save
// folder on a device (its id), the window's title. The shell calls it as soon as
// the app's fields are filled.
void                  fude_app_start(fude_app* _app);
// The window as the apps lay out in it, before anything reads its size (the
// camera, the UI): on Android in dp, as iOS's points — an iPad's sizes on an
// Android tablet, not half of them (rde_window_set_density_scaling). The shell
// calls it first thing.
void                  fude_app_window(rde_window* _window);
// What the app adds (extension.h): its own, or nothing.
const fude_extension* fude_app_ext(const fude_app* _app);
// Its id (info.h): its own, or its name in lowercase (letters and digits).
const c8*             fude_app_id(const fude_app* _app);

// The screen on top (NULL: none — the page). The table's order is the stack's.
const fude_screen_slot* fude_app_top(const fude_app* _app);
// Is screen _screen (its place in the table) open?
b8                     fude_app_is_open(const fude_app* _app, u32 _screen);

// Once a frame, for the screen on top: its frame (and what it hands on), Escape,
// the desktop mouse polled; a screen back on top resumes. False when none is
// open (the page's frame is the caller's).
b8   fude_app_update(fude_app* _app, f32 _dt);
// The screen on top drawn (an overlay — the welcome — over the one under it, or
// the page: false then, the page is the caller's to draw first).
b8   fude_app_render(fude_app* _app);
void fude_app_render_overlays(fude_app* _app);
// A pen, finger or mouse event for the screen on top (one pointer at a time).
void fude_app_screen_event(fude_app* _app, rde_event* _event);
// Every screen closed (an import replaced what they show).
void fude_app_close_all(fude_app* _app);

// A pen position (window pixels: top-left origin, Y down) in the app's screen
// space (centre origin, Y up); a touch's (centre origin, Y down) too.
rde_vec_2F fude_app_window_to_screen(const fude_app* _app, rde_vec_2F _pixel);
rde_vec_2F fude_app_touch_to_screen(rde_vec_2I _touch);

// A tablet's hand on the toolbar: one finger writes (on), or only the pen (off).
// The page says which, whoever switched it.
void fude_app_set_finger_writes(fude_app* _app, b8 _on);

#endif
