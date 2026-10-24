// The app's pointer hand-off (fude/drawing/app/app.c: fude_app_screen_event): each touch reaches the screen on top
// with the event's own time, so a list's fling is read from the finger's real speed. On a phone several moves arrive
// in one frame; handed the frame's time instead, they all had one time and a quick flick did not coast (here the
// clock stands still, tests/support/engine.c, so the frame's time is the same for every event). A drag that rested
// before lifting still does not coast. And a finger the system takes away (RDE_EVENT_TYPE_MOBILE_TOUCH_CANCELED:
// Android cancels a resting hand's touch as the pen comes down) is let go: before, the app followed it for good, and on
// a screen where only the pen writes (the exam) no stroke was taken until the screen closed. The rest of the app (its
// UI, page, toolbar) is not under test: stand-ins below.
#include "drawing/app/app.h"
#include "drawing/app/ui.h"
#include "drawing/app/page.h"
#include "drawing/base/crash.h"
#include "drawing/base/save.h"
#include "drawing/base/text.h"
#include "drawing/widgets/notice.h"
#include "drawing/widgets/kit.h"
#include "drawing/ink/lasso.h"
#include <stdio.h>

static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

// --- what app.c calls besides the pointer hand-off: nothing to do, nothing hit ------------------------------------
void      fude_crash_install(const c8* _dir, const c8* _app, const c8* _version) { (void)_dir; (void)_app; (void)_version; }
void      fude_save_set_folder(const c8* _name) { (void)_name; }
const c8* fude_save_dir(void) { return ""; }
const c8* fude_text(FUDE_TEXT_ _id) { (void)_id; return ""; }
void      fude_notice_show(const c8* _text) { (void)_text; }
void      fude_page_pen_came(fude_page_input* _page) { (void)_page; }
void      fude_pagemenu_close_context(fude_pagemenu* _menu) { (void)_menu; }
b8        fude_row_close_menu(fude_row* _row) { (void)_row; return false; }
b8        fude_side_back(struct fude_ui* _ui) { (void)_ui; return false; }
b8        fude_docbar_back(fude_docbar* _bar) { (void)_bar; return false; }
void      fude_toolbar_refresh(fude_toolbar* _toolbar) { (void)_toolbar; }
void      fude_toolbar_set_palette_open(fude_toolbar* _toolbar, b8 _open) { (void)_toolbar; (void)_open; }
void      fude_toolbar_set_paper_open(fude_toolbar* _toolbar, b8 _open) { (void)_toolbar; (void)_open; }
void      fude_ui_update(fude_ui* _ui) { (void)_ui; }
void      fude_ui_frame(const fude_ui* _ui, u32 _screen, fude_screen_frame* _frame) { (void)_ui; (void)_screen; (void)_frame; }
b8        fude_ui_hit(const fude_ui* _ui, rde_vec_2F _screen) { (void)_ui; (void)_screen; return false; }   // (no UI over the screen)
b8        fude_ui_press_back(fude_ui* _ui) { (void)_ui; return false; }
b8        fude_lasso_busy(const fude_lasso* _lasso) { (void)_lasso; return false; }
b8        fude_lasso_box(const fude_lasso* _lasso, const fude_ink* _ink, rde_vec_2F* _min, rde_vec_2F* _max) { (void)_lasso; (void)_ink; (void)_min; (void)_max; return false; }
void      fude_lasso_clear(fude_lasso* _lasso, fude_ink* _ink) { (void)_lasso; (void)_ink; }

// --- a list screen: its scroller follows the pointer, as the apps' lists do --------------------------------------
typedef struct {
    rde_ui_scroller scroller;
    f64             last_time;   // the time the last pointer call was given
} list_screen;

static b8   list_is_open(const any _self) { (void)_self; return true; }
static void list_down(any _self, rde_vec_2F _at, b8 _pen, f64 _now) {
    (void)_pen;
    list_screen* _l = (list_screen*)_self;
    _l->last_time   = _now;
    rde_ui_scroller_pointer_down(&_l->scroller, _at, _now);
}
static void list_moved(any _self, rde_vec_2F _at, f64 _now) {
    list_screen* _l = (list_screen*)_self;
    _l->last_time   = _now;
    rde_ui_scroller_pointer_moved(&_l->scroller, _at, _now);
}
static void list_up(any _self, rde_vec_2F _at, f64 _now) {
    (void)_at;
    list_screen* _l = (list_screen*)_self;
    _l->last_time   = _now;
    rde_ui_scroller_pointer_up(&_l->scroller, _now);
}
static const fude_screen LIST_SCREEN = {
    .name = "list", .input = FUDE_SCREEN_INPUT_POINT, .is_open = list_is_open,
    .pointer_down = list_down, .pointer_moved = list_moved, .pointer_up = list_up,
};

// --- an exam's answer square: only the pen writes, so a resting hand does nothing (study/screens/exam.c) -----------
typedef struct {
    u32 presses;   // pointer_downs it was given
    u32 strokes;   // ...the pen's among them, each starting a stroke
    u32 moves;     // moves while a stroke is drawn
    u32 cancels;   // pointer_cancels it was given
    b8  writing;
} square_screen;

static void square_down(any _self, rde_vec_2F _at, b8 _pen, f64 _now) {
    (void)_at; (void)_now;
    square_screen* _s = (square_screen*)_self;
    _s->presses++;
    if(_pen) {
        _s->strokes++;
        _s->writing = true;
    }
}
static void square_moved(any _self, rde_vec_2F _at, f64 _now) {
    (void)_at; (void)_now;
    square_screen* _s = (square_screen*)_self;
    _s->moves += _s->writing ? 1u : 0u;
}
static void square_up(any _self, rde_vec_2F _at, f64 _now) { (void)_at; (void)_now; ((square_screen*)_self)->writing = false; }
static void square_cancel(any _self, f64 _now) {
    (void)_now;
    square_screen* _s = (square_screen*)_self;
    _s->cancels++;
    _s->writing = false;
}
static const fude_screen SQUARE_SCREEN = {
    .name = "square", .input = FUDE_SCREEN_INPUT_POINT, .is_open = list_is_open,
    .pointer_down = square_down, .pointer_moved = square_moved, .pointer_up = square_up, .pointer_cancel = square_cancel,
};

// Finger _finger's event of _type at (_x, _y) in screen space, at _seconds on the events' clock.
static rde_event finger(RDE_EVENT_TYPE_ _type, u64 _finger, i32 _x, i32 _y, f64 _seconds) {
    rde_event _e;
    rde_memset(&_e, 0, sizeof(_e));
    _e.type                                      = _type;
    _e.time_stamp                                = (u64)(_seconds * 1e9 + 0.5);
    _e.data.mobile_event_data.finger_id          = _finger;
    _e.data.mobile_event_data.init_touch_position  = (rde_vec_2I){ _x, _y };
    _e.data.mobile_event_data.moved_touch_position = (rde_vec_2I){ _x, _y };
    _e.data.mobile_event_data.end_touch_position   = (rde_vec_2I){ _x, _y };
    return _e;
}

// A touch event of _type at _y (screen space, the middle of the window across), at _seconds on the events' clock.
static rde_event touch(RDE_EVENT_TYPE_ _type, i32 _y, f64 _seconds) {
    return finger(_type, 1u, 0, _y, _seconds);
}

// The pen's event of _type at (_x, _y) in screen space, at _seconds.
static rde_event pen(RDE_EVENT_TYPE_ _type, f32 _x, f32 _y, f64 _seconds) {
    rde_event _e;
    rde_memset(&_e, 0, sizeof(_e));
    _e.type                         = _type;
    _e.time_stamp                   = (u64)(_seconds * 1e9 + 0.5);
    _e.data.pen_event_data.position = (rde_vec_2F){ _x, _y };
    _e.data.pen_event_data.pen_id   = 1u;
    return _e;
}

static void send(fude_app* _app, rde_event _e) {
    fude_app_screen_event(_app, &_e);
}

// A stroke of the pen from (_x, _y): down, three moves, up, from _t.
static void pen_stroke(fude_app* _app, f32 _x, f32 _y, f64 _t) {
    send(_app, pen(RDE_EVENT_TYPE_PEN_DOWN, _x, _y, _t));
    for(u32 _k = 1; _k <= 3u; _k++) {
        send(_app, pen(RDE_EVENT_TYPE_PEN_MOVED, _x + 10.0f * (f32)_k, _y, _t + 0.01 * (f64)_k));
    }
    send(_app, pen(RDE_EVENT_TYPE_PEN_UP, _x + 30.0f, _y, _t + 0.04));
}

// A hand rests on the glass before the pen (the app follows its finger, which writes nothing in the square), and
// Android cancels the hand's touch as the pen comes down: the pen's strokes are taken, and so is a finger after.
static void hand_cancelled_then_pen(void) {
    square_screen _sq;
    rde_memset(&_sq, 0, sizeof(_sq));
    fude_app _app;
    rde_memset(&_app, 0, sizeof(_app));
    _app.screens[0]   = (fude_screen_slot){ .vt = &SQUARE_SCREEN, .self = &_sq };
    _app.screen_count = 1u;

    send(&_app, finger(RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN, 4u, -200, -300, 1.0));
    CHECK(_app.pointer == FUDE_POINTER_FINGER && _sq.presses == 1u && _sq.strokes == 0u);
    send(&_app, finger(RDE_EVENT_TYPE_MOBILE_TOUCH_CANCELED, 4u, -200, -300, 1.2));
    CHECK(_app.pointer == FUDE_POINTER_NONE);   // (before: FINGER, until the screen closed)
    CHECK(_sq.cancels == 1u);
    pen_stroke(&_app, 0.0f, 0.0f, 1.21);
    pen_stroke(&_app, 0.0f, 40.0f, 1.5);
    CHECK(_sq.strokes == 2u && _sq.moves == 6u);   // (before: 0 and 0: every pen press was refused)
    CHECK(_app.pointer == FUDE_POINTER_NONE);
    send(&_app, finger(RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN, 4u, 0, 0, 2.0));   // (Android reuses the id)
    CHECK(_app.pointer == FUDE_POINTER_FINGER && _sq.presses == 4u);
    send(&_app, finger(RDE_EVENT_TYPE_MOBILE_TOUCH_UP, 4u, 0, 0, 2.1));
    CHECK(_app.pointer == FUDE_POINTER_NONE && _sq.cancels == 1u);   // (a lift is no cancel)
}

// While the pen writes, a second finger lands (a palm) and is cancelled: it was never followed, so the stroke goes on
// and the screen hears no cancel.
static void palm_cancelled_mid_stroke(void) {
    square_screen _sq;
    rde_memset(&_sq, 0, sizeof(_sq));
    fude_app _app;
    rde_memset(&_app, 0, sizeof(_app));
    _app.screens[0]   = (fude_screen_slot){ .vt = &SQUARE_SCREEN, .self = &_sq };
    _app.screen_count = 1u;

    send(&_app, pen(RDE_EVENT_TYPE_PEN_DOWN, 0.0f, 0.0f, 1.0));
    send(&_app, pen(RDE_EVENT_TYPE_PEN_MOVED, 10.0f, 0.0f, 1.01));
    send(&_app, finger(RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN, 5u, 100, -300, 1.02));
    send(&_app, finger(RDE_EVENT_TYPE_MOBILE_TOUCH_CANCELED, 5u, 100, -300, 1.03));
    send(&_app, pen(RDE_EVENT_TYPE_PEN_MOVED, 20.0f, 0.0f, 1.04));
    CHECK(_app.pointer == FUDE_POINTER_PEN && _sq.writing && _sq.moves == 2u);
    send(&_app, pen(RDE_EVENT_TYPE_PEN_UP, 20.0f, 0.0f, 1.05));
    CHECK(_app.pointer == FUDE_POINTER_NONE && _sq.strokes == 1u && _sq.presses == 1u && _sq.cancels == 0u);
}

// A list (no pointer_cancel, as the apps' lists): a quick touch the system cancels is no tap, and the next touch is
// taken, and taps.
static void list_touch_cancelled(void) {
    list_screen _list;
    rde_memset(&_list, 0, sizeof(_list));
    fude_kit_scroller_init(&_list.scroller);
    fude_app _app;
    rde_memset(&_app, 0, sizeof(_app));
    _app.screens[0]   = (fude_screen_slot){ .vt = &LIST_SCREEN, .self = &_list };
    _app.screen_count = 1u;

    send(&_app, finger(RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN, 1u, 0, 0, 1.0));
    send(&_app, finger(RDE_EVENT_TYPE_MOBILE_TOUCH_CANCELED, 1u, 0, 0, 1.05));   // (an edge gesture claimed it)
    CHECK(_app.pointer == FUDE_POINTER_NONE);
    CHECK(!rde_ui_scroller_take_tap(&_list.scroller, NULL));
    send(&_app, finger(RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN, 2u, 0, 0, 2.0));
    send(&_app, finger(RDE_EVENT_TYPE_MOBILE_TOUCH_UP, 2u, 0, 0, 2.05));
    CHECK(rde_ui_scroller_take_tap(&_list.scroller, NULL));   // (before: the second touch was never taken)
}

// A finger goes down, moves up 40 units every 16 ms three times (all in one frame, as a phone sends them), and lifts
// _lift_after seconds after its last move. Returns the list's fling speed then.
static f32 flick(fude_app* _app, list_screen* _list, f64 _lift_after) {
    fude_kit_scroller_init(&_list->scroller);
    rde_ui_scroller_update(&_list->scroller, 0.0f, (rde_vec_2F){ 0.0f, 10000.0f }, (rde_vec_2F){ 0.0f, 100.0f });   // (its sizes)
    const f64 _t0 = 10.0;
    rde_event _e  = touch(RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN, 0, _t0);
    fude_app_screen_event(_app, &_e);
    CHECK(_list->last_time == (f64)_e.time_stamp * 1e-9);   // (the event's time, not the still clock's 1.0)
    for(u32 _k = 1; _k <= 3u; _k++) {
        _e = touch(RDE_EVENT_TYPE_MOBILE_TOUCH_MOVED, 40 * (i32)_k, _t0 + 0.016 * (f64)_k);
        fude_app_screen_event(_app, &_e);
        CHECK(_list->last_time == (f64)_e.time_stamp * 1e-9);
    }
    _e = touch(RDE_EVENT_TYPE_MOBILE_TOUCH_UP, 120, _t0 + 0.048 + _lift_after);
    fude_app_screen_event(_app, &_e);
    CHECK(_list->last_time == (f64)_e.time_stamp * 1e-9);
    CHECK(_app->pointer == FUDE_POINTER_NONE);
    return _list->scroller.velocity.y;
}

int main(int _argc, char** _argv) {
    (void)_argc; (void)_argv;
    list_screen _list;
    rde_memset(&_list, 0, sizeof(_list));
    fude_app _app;
    rde_memset(&_app, 0, sizeof(_app));
    _app.screens[0]   = (fude_screen_slot){ .vt = &LIST_SCREEN, .self = &_list };
    _app.screen_count = 1u;

    // A quick flick coasts at about its own speed: 120 units in 48 ms, 2,500 units a second (before: every event had
    // the frame's one time, no speed could be read, and it stopped dead).
    const f32 _quick = flick(&_app, &_list, 0.008);
    CHECK(_quick > 2400.0f && _quick < 2600.0f);
    // The same drag, held still half a second before lifting, does not coast.
    CHECK(flick(&_app, &_list, 0.5) == 0.0f);

    hand_cancelled_then_pen();
    palm_cancelled_mid_stroke();
    list_touch_cancelled();

    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
