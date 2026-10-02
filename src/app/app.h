#ifndef KANA_APP
#define KANA_APP

#include "rde.h"
#include "screens/screen.h"
#include "chars/kanji.h"
#include "chars/catalog.h"
#include "ink/ink.h"
#include "ink/canvas.h"
#include "ink/lasso.h"
#include "ink/notes.h"
#include "study/select.h"
#include "study/sheet.h"
#include "screens/viewer.h"
#include "screens/browse.h"
#include "screens/chart.h"
#include "screens/practice.h"
#include "screens/album.h"
#include "screens/check.h"
#include "screens/exam.h"
#include "screens/stats.h"
#include "screens/scan.h"
#include "screens/vocabview.h"
#include "screens/wordexam.h"
#include "screens/welcome.h"

// ===========================================================================
// The app: what Kana holds, for the parts that act on more than their own —
// the screens' rows (row.h), the toolbar, the side panel, the word card — and
// the ways from one screen to another (the verbs below).
//
// THE LAYERS. Models keep what is learned and written and know nothing of the
// screen (kanji.h, vocab.h, review.h, marks.h, ink.h...). Widgets are pieces of
// UI that know no screen (kit.h, row.h, toolbar.h, side.h, wordcard.h,
// filterbar.h, notice.h, draw.h, glyph.h, scroll.h). Screens are what fills the
// screen (screen.h): each draws itself, takes the pointer, and declares its row
// of buttons — what they say and what they do. The app (kana.c, app.c) owns
// them all, keeps the screens in order, and hands each event and frame to the
// one on top; a screen reaches another only through the verbs here.
// ===========================================================================

struct kana_ui;
struct kana_page_input;

// Every screen, in the order they stack: the first open one is on top (and gets
// the pointer, its row, the frame). The page is under them all.
typedef enum {
    KANA_SCREEN_WELCOME = 0,   // over everything (the first time, and from Settings)
    KANA_SCREEN_PRACTICE,      // over whatever opened it
    KANA_SCREEN_VIEWER,        // over the lists, an exam's results, Check, Statistics
    KANA_SCREEN_SCAN,
    KANA_SCREEN_WORDEXAM,      // over the Vocabulary
    KANA_SCREEN_EXAM,          // over the album (a kept exam)
    KANA_SCREEN_STATS,
    KANA_SCREEN_VOCAB,
    KANA_SCREEN_CHECK,
    KANA_SCREEN_ALBUM,
    KANA_SCREEN_CHART,
    KANA_SCREEN_BROWSE,
    KANA_SCREEN_COUNT
} KANA_SCREEN_;

// The pointer the screens follow: one at a time, whichever pressed first.
typedef enum { KANA_POINTER_NONE = 0, KANA_POINTER_PEN, KANA_POINTER_FINGER, KANA_POINTER_MOUSE } KANA_POINTER_;

typedef struct kana_app {
    rde_window*      window;
    rde_font*        font;           // the UI font (the screens' text), at font_px
    f32              font_px;
    kana_kanji_db*   db;             // the character data (NULL: missing — the app still draws)
    kana_catalog*    catalog;        // its parts and shapes (catalog.h)

    // The page.
    kana_ink*        ink;
    kana_canvas*     canvas;
    kana_lasso*      lasso;
    kana_notes*      notes;          // the canvases and their folders
    struct kana_page_input* page;          // writing on it, moving it (page.h)
    b8               finger_writes;  // a tablet: one finger writes too (the toolbar's hand); a pen coming down turns it off
    b8               pen_ever;       // a pen has written here, ever (saved): until one has, a tablet starts with the hand on

    // The screens, and the ticks Browse and the chart share.
    kana_screen_slot screens[KANA_SCREEN_COUNT];   // in KANA_SCREEN_ order
    kana_viewer*     viewer;
    kana_browse*     browse;
    kana_chart*      chart;
    kana_practice*   practice;
    kana_album*      album;
    kana_check*      check;
    kana_exam*       exam;
    kana_stats*      stats;
    kana_scan*       scan;
    kana_vocabview*  vocab;
    kana_wordexam*   wordexam;
    kana_welcome*    welcome;
    kana_selection*  selection;

    struct kana_ui*  ui;             // the retained UI (ui.h)

    // The screens' pointer (kana_app_screen_event).
    KANA_POINTER_    pointer;
    u64              pointer_finger;
    rde_vec_2F       pointer_last;
    const kana_screen_slot* top_seen;   // the screen on top last frame (to tell one coming back to the top)

    // A practice sheet asked for (kana_app_sheet): made by the owner next frame.
    u32              sheet[KANA_SHEET_MAX + 1u];   // one more: too many to fit is told
    u32              sheet_count;
} kana_app;

// The screen on top (NULL: none — the page). The table's order is the stack's.
const kana_screen_slot* kana_app_top(const kana_app* _app);
// That screen's id (KANA_SCREEN_COUNT: the page).
KANA_SCREEN_           kana_app_top_id(const kana_app* _app);
b8                     kana_app_is_open(const kana_app* _app, KANA_SCREEN_ _screen);

// Once a frame, for the screen on top: its frame (and what it hands on), Escape,
// the desktop mouse polled; a screen back on top resumes. False when none is
// open (the page's frame is the caller's).
b8   kana_app_update(kana_app* _app, f32 _dt);
// The screen on top drawn (an overlay — the welcome — over the one under it, or
// the page: false then, the page is the caller's to draw first).
b8   kana_app_render(kana_app* _app);
void kana_app_render_overlays(kana_app* _app);
// A pen, finger or mouse event for the screen on top (one pointer at a time).
void kana_app_screen_event(kana_app* _app, rde_event* _event);
// Every screen closed (an import replaced what they show).
void kana_app_close_all(kana_app* _app);

// A pen position (window pixels: top-left origin, Y down) in Kana's screen space
// (centre origin, Y up); a touch's (centre origin, Y down) too.
rde_vec_2F kana_app_window_to_screen(const kana_app* _app, rde_vec_2F _pixel);
rde_vec_2F kana_app_touch_to_screen(rde_vec_2I _touch);

// --- the verbs: from one screen to another --------------------------------------------

// Practice: one character alone, several as a set.
void kana_app_practice(kana_app* _app, const u32* _records, u32 _count);
// Practice as a set, even of one (a word's kanji: Next and Finish, the summary).
void kana_app_practice_set(kana_app* _app, const u32* _records, u32 _count);
// The viewer on _records[_at], walking them.
void kana_app_view(kana_app* _app, const u32* _records, u32 _count, u32 _at);
// An exam of _records, straight to its preview.
void kana_app_exam(kana_app* _app, const u32* _records, u32 _count);
// A practice sheet (sheet.h) of _records: shared or saved by the owner, next frame.
void kana_app_sheet(kana_app* _app, const u32* _records, u32 _count);
// _text written on the page at _canvas in the characters' own strokes, selected (Paste text's way).
void kana_app_write_text(kana_app* _app, const c8* _text, rde_vec_2F _canvas);
// A screen opened from the side panel (the lasso's selection let go first).
void kana_app_open(kana_app* _app, KANA_SCREEN_ _screen);
// The characters' reviews due today (review.h), of the Studying and Known ones:
// records into _out (at most _max); how many.
u32  kana_app_reviews_due(kana_app* _app, u32* _out, u32 _max);
// Them as an exam — or, with none due, a notice saying so (false).
b8   kana_app_review(kana_app* _app);

// Select mode's row (select.h), Browse's and the chart's alike: All, None, Study,
// Exam, Sheet, Save list, Practice n, Done — on the ticked characters. Its faces
// from the ticks.
#define KANA_APP_SELECT_COUNT 8u
extern const kana_row_button KANA_APP_SELECT_BUTTONS[KANA_APP_SELECT_COUNT];
void kana_app_select_faces(const kana_selection* _selection, kana_row_face* _faces);

// A tablet's hand on the toolbar: one finger writes (on), or only the pen (off).
// The page says which, whoever switched it.
void kana_app_set_finger_writes(kana_app* _app, b8 _on);

#endif
