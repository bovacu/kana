// Render on demand's rule (fude/drawing/base/redraw.h), driven frame by frame with a clock the test moves: the start
// is drawn; a change is drawn with its grace period after it (half a second of frames at 64 Hz, and at least four
// frames when frames are slow), then the frames are left undrawn, and a still screen stays undrawn however long it
// waits (no frame comes on its own); every kind of source wakes it — an event's change, a timer (kept across frames
// until due, the soonest of several, a hair early counted as due, and drawing its one frame with no grace after it),
// RDE's UI moving or timing, a request from another thread; an unchanged frame sleeps until its timer, or with none
// until something happens; every frame is drawn while continuous; and the glue hands the verdict to RDE (render on
// demand on, or off when continuous). The check (a developer's build): its plan (the first undrawn frame after a drawn
// one is the reference, a probe each period after while the screen stays still, nothing while continuous or off) and
// its comparison (the pixels that differ, and their box). Then the sources that report from their own code: the
// scroller's fling, the canvas's flick and its long press, a notice. Last, 20000 random frames, each checked against the
// whole rule written out again as its properties.
#include "drawing/base/redraw.h"
#include "drawing/widgets/kit.h"
#include "drawing/widgets/notice.h"
#include "drawing/ink/canvas.h"
#include <stdio.h>
#include <math.h>

static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

// The clock: the module is handed it (fude_redraw_end_frame_at), and the sources read it as the engine's (this
// definition wins over tests/support/engine.c's still one).
static f64 now_s = 100.0;
f64 rde_engine_get_time_now(void) { return now_s; }

// One frame _dt after the last, ending with RDE's UI saying _ui (rde_ui_get_wake_after).
static fude_redraw_verdict frame_ui(f64 _dt, f32 _ui) {
    now_s += _dt;
    return fude_redraw_end_frame_at(now_s, _ui);
}
static fude_redraw_verdict frame(f64 _dt) {
    return frame_ui(_dt, -1.0f);
}
static b8 near(f64 _a, f64 _b) {
    return fabs(_a - _b) < 1e-9;
}

// Frames at _dt until one is unchanged (at most _most): returns how many were drawn before it.
static u32 drawn_until_quiet(f64 _dt, u32 _most) {
    u32 _n = 0;
    while(_n < _most && frame(_dt).draw) {
        _n++;
    }
    return _n;
}

// A still screen, for a test to start from: the grace period behind it, and any timer waiting long due.
static void settle(void) {
    drawn_until_quiet(1.0 / 64.0, 1000u);
    now_s += 5.0;
    drawn_until_quiet(1.0 / 64.0, 1000u);
}

// The worker of the request case: asks from its own thread.
static any request_from_thread(rde_thread* _t, any _data) {
    (void)_t; (void)_data;
    fude_redraw_request();
    return NULL;
}

static void test_rule(void) {
    fude_redraw_start();
    fude_redraw_set_continuous(false);
    CHECK(frame(0.0).draw);   // the start is a change

    // Grace at 64 Hz (1/64 s is exact): half a second of frames after the change, then unchanged.
    settle();
    fude_redraw_changed();
    CHECK(frame(1.0 / 64.0).draw);
    CHECK(drawn_until_quiet(1.0 / 64.0, 1000u) == 31u);   // the frames at 1/64 ... 31/64 s; at 32/64 = 0.5 s, unchanged
    // Its sleep, with no timer: until something happens (below 0).
    settle();
    {
        fude_redraw_changed();
        frame(1.0 / 64.0);
        const f64 _t0 = now_s;
        for(u32 _k = 1; _k < 32u; _k++) {
            frame(1.0 / 64.0);
        }
        const fude_redraw_verdict _v = frame(1.0 / 64.0);
        CHECK(!_v.draw);
        CHECK(_v.sleep < 0.0);
        CHECK(near(now_s - _t0, 0.5));
        CHECK(near(fude_redraw_last_change(), _t0));
    }

    // Slow frames: the frame of the change and the three after it, though they are more than half a second apart.
    settle();
    fude_redraw_changed();
    CHECK(frame(0.75).draw);
    CHECK(frame(0.75).draw);
    CHECK(frame(0.75).draw);
    CHECK(frame(0.75).draw);
    CHECK(!frame(0.75).draw);

    // A still screen stays undrawn, however long it waits: nothing draws but a change or a timer.
    settle();
    for(u32 _k = 0; _k < 50u; _k++) {
        const fude_redraw_verdict _q = frame(_k < 25u ? 1.0 : 100.0);
        CHECK(!_q.draw && _q.sleep < 0.0);
    }

    // An event (the shells report every event as a change) after a still while: drawn, and its grace again.
    settle();
    fude_redraw_changed();
    CHECK(frame(0.2).draw);
    CHECK(frame(0.2).draw);

    // A timer: unchanged until it is due, sleeping no longer; a frame woken early by something else sleeps the rest;
    // the due frame is drawn, alone.
    settle();
    fude_redraw_after(0.25);
    {
        fude_redraw_verdict _v = frame(0.0);
        CHECK(!_v.draw && near(_v.sleep, 0.25));
        _v = frame(0.1);
        CHECK(!_v.draw && near(_v.sleep, 0.15));
        CHECK(frame(0.15).draw);
        CHECK(!frame(1.0 / 64.0).draw);
    }
    // ...asked once, kept over the frames between.
    settle();
    fude_redraw_after(0.75);
    CHECK(!frame(0.0).draw);
    CHECK(!frame(0.3).draw);
    CHECK(!frame(0.3).draw);
    CHECK(frame(0.15).draw);
    // ...the soonest of several, asked in one frame or across frames.
    settle();
    fude_redraw_after(0.5);
    fude_redraw_after(0.2);
    CHECK(near(frame(0.0).sleep, 0.2));
    now_s += 0.05;
    fude_redraw_after(0.1);   // (from now: 0.05 s into the 0.2 s one)
    CHECK(near(frame(0.0).sleep, 0.1));
    CHECK(frame(0.1).draw);
    // ...and a hair early (the sleep is in whole milliseconds) counts as due, more than that not.
    settle();
    fude_redraw_after(0.3);
    frame(0.0);
    CHECK(!frame(0.297).draw);
    CHECK(frame(0.0015).draw);
    // ...0, or a time already past: the next frame.
    settle();
    fude_redraw_after(-1.0);
    CHECK(frame(0.0).draw);
    // ...a timer is not a change: it starts no grace period.
    settle();
    fude_redraw_after(0.0);
    CHECK(frame(0.0).draw);
    CHECK(!frame(1.0 / 64.0).draw);

    // RDE's UI: 0 is a change (a fling, a colour blend); above 0 a timer (a caret); below 0 nothing.
    settle();
    CHECK(frame_ui(0.0, 0.0f).draw);
    CHECK(frame(1.0 / 64.0).draw);   // (its grace)
    settle();
    {
        const fude_redraw_verdict _v = frame_ui(0.0, 0.5f);
        CHECK(!_v.draw && near(_v.sleep, 0.5));
        CHECK(frame(0.5).draw);
        CHECK(!frame_ui(1.0 / 64.0, -1.0f).draw);
    }

    // A request from another thread: the next frame's end is a change.
    settle();
    {
        rde_thread* _t = rde_thread_run(request_from_thread, NULL);
        CHECK(_t != NULL);
        rde_thread_wait(_t);
        CHECK(frame(0.0).draw);
        CHECK(frame(1.0 / 64.0).draw);   // (its grace)
    }

    // Continuous: every frame drawn, however still; then render on demand again.
    settle();
    fude_redraw_set_continuous(true);
    CHECK(fude_redraw_continuous());
    for(u32 _k = 0; _k < 200u; _k++) {
        CHECK(frame(0.1).draw);
    }
    fude_redraw_set_continuous(false);
    CHECK(!frame(0.1).draw);

    // The glue: render on demand on in RDE unless continuous (RDE's UI is not running here: it says nothing; no
    // window, so the check asks for no picture).
    fude_redraw_set_continuous(false);
    fude_redraw_frame_end(NULL, "the page");
    CHECK(rde_engine_is_render_on_demand());
    fude_redraw_set_continuous(true);
    fude_redraw_frame_end(NULL, "the page");
    CHECK(!rde_engine_is_render_on_demand());
    fude_redraw_set_continuous(false);
    rde_engine_set_render_on_demand(false);
}

// The check (redraw.h: THE CHECK): its plan frame by frame, and its comparison of two pictures.
static void test_check(void) {
    fude_redraw_set_continuous(false);
    fude_redraw_set_check(true);
    const f64 _t = 500.0;
    // A drawn frame: nothing; the first undrawn one after it: the reference, and a probe planned a period on.
    CHECK(fude_redraw_check_step(true, _t) == FUDE_REDRAW_CHECK_NONE);
    CHECK(fude_redraw_check_step(false, _t + 0.1) == FUDE_REDRAW_CHECK_REFERENCE);
    CHECK(near(fude_redraw_check_wake(_t + 0.1), FUDE_REDRAW_CHECK_SECONDS));
    // Undrawn frames before it: nothing, each sleeping no longer than to the probe.
    CHECK(fude_redraw_check_step(false, _t + 0.6) == FUDE_REDRAW_CHECK_NONE);
    CHECK(near(fude_redraw_check_wake(_t + 0.6), FUDE_REDRAW_CHECK_SECONDS - 0.5));
    // The probe, a hair early counted; then the next a period on, for as long as the screen stays still.
    CHECK(fude_redraw_check_step(false, _t + 0.1 + FUDE_REDRAW_CHECK_SECONDS - 0.001) == FUDE_REDRAW_CHECK_PROBE);
    CHECK(fude_redraw_check_step(false, _t + 1.5) == FUDE_REDRAW_CHECK_NONE);
    CHECK(fude_redraw_check_step(false, _t + 2.2) == FUDE_REDRAW_CHECK_PROBE);
    // A drawn frame puts up a new picture: no probe planned, and the next undrawn frame is the reference again.
    CHECK(fude_redraw_check_step(true, _t + 2.3) == FUDE_REDRAW_CHECK_NONE);
    CHECK(fude_redraw_check_wake(_t + 2.3) < 0.0);
    CHECK(fude_redraw_check_step(false, _t + 2.4) == FUDE_REDRAW_CHECK_REFERENCE);
    // Nothing while every frame is drawn, nor with the check off.
    fude_redraw_set_continuous(true);
    CHECK(fude_redraw_check_step(false, _t + 9.0) == FUDE_REDRAW_CHECK_NONE);
    CHECK(fude_redraw_check_wake(_t + 9.0) < 0.0);
    fude_redraw_set_continuous(false);
    fude_redraw_set_check(false);
    CHECK(fude_redraw_check_step(false, _t + 10.0) == FUDE_REDRAW_CHECK_NONE);
    CHECK(fude_redraw_check_wake(_t + 10.0) < 0.0);

    // One picture at a time: asked (as fude_redraw_frame_end asks RDE), nothing more is planned until it is delivered —
    // a minimised window draws no frame, and each probe after the first was asked again for nothing.
    fude_redraw_set_check(true);
    CHECK(fude_redraw_check_step(true, _t + 20.0) == FUDE_REDRAW_CHECK_NONE);
    CHECK(fude_redraw_check_step(false, _t + 20.1) == FUDE_REDRAW_CHECK_REFERENCE);
    fude_redraw_check_asked(FUDE_REDRAW_CHECK_REFERENCE, "the page", _t + 20.1);
    CHECK(fude_redraw_check_step(false, _t + 20.1 + 3.0 * FUDE_REDRAW_CHECK_SECONDS) == FUDE_REDRAW_CHECK_NONE);
    {
        static u8 _ref[4 * 4 * 4], _later[4 * 4 * 4];
        _later[0] = 255u;   // (one pixel differs)
        CHECK(fude_redraw_check_delivered(_ref, 4u, 4u).pixels == 0u);   // (a reference: kept)
        CHECK(fude_redraw_check_step(false, _t + 20.2 + 3.0 * FUDE_REDRAW_CHECK_SECONDS) == FUDE_REDRAW_CHECK_PROBE);
        // A probe answered after a change was reported (the frame drawn after a restore): kept, not compared — no
        // warning for a change that was reported.
        fude_redraw_check_asked(FUDE_REDRAW_CHECK_PROBE, "the page", now_s);
        fude_redraw_changed();
        frame(0.1);
        CHECK(fude_redraw_check_delivered(_later, 4u, 4u).pixels == 0u);
        // A probe with nothing reported since: compared, its difference warned of.
        fude_redraw_check_asked(FUDE_REDRAW_CHECK_PROBE, "the page", now_s + 1.0);
        CHECK(fude_redraw_check_delivered(_ref, 4u, 4u).pixels == 1u);
        remove("redraw_check_before.png");   // (the warning's two crops)
        remove("redraw_check_after.png");
    }
    fude_redraw_set_check(false);

    // The comparison: the same pictures; one pixel off (in any byte, its alpha too); two far apart, boxed together.
    {
        enum { W = 37, H = 23 };
        static u8 _a[W * H * 4], _b[W * H * 4];
        for(u32 _i = 0; _i < W * H * 4; _i++) {
            _a[_i] = _b[_i] = (u8)(_i * 7u);
        }
        fude_redraw_difference _d = fude_redraw_compare(_a, _b, W, H);
        CHECK(_d.pixels == 0u);
        _b[(5u * W + 9u) * 4u + 3u] ^= 1u;
        _d = fude_redraw_compare(_a, _b, W, H);
        CHECK(_d.pixels == 1u && _d.x == 9u && _d.y == 5u && _d.w == 1u && _d.h == 1u);
        _b[(20u * W + 2u) * 4u + 0u] ^= 0x80u;
        _d = fude_redraw_compare(_a, _b, W, H);
        CHECK(_d.pixels == 2u && _d.x == 2u && _d.y == 5u && _d.w == 8u && _d.h == 16u);
        _b[(0u * W + 36u) * 4u + 1u] ^= 0x40u;
        _d = fude_redraw_compare(_a, _b, W, H);
        CHECK(_d.pixels == 3u && _d.x == 2u && _d.y == 0u && _d.w == 35u && _d.h == 21u);
    }
}

// The sources that report from their own code.
static void test_sources(void) {
    fude_redraw_set_continuous(false);

    // A list's fling (RDE's rde_ui_scroller, moved by drawing/widgets/kit.h: fude_kit_scroller_update): drawn while it
    // moves the list, still once it has stopped.
    settle();
    {
        rde_ui_scroller _s;
        fude_kit_scroller_init(&_s);
        fude_kit_scroller_update(&_s, 1.0f / 64.0f, 1.0e6f, 100.0f);   // (its sizes, which a drag is kept within)
        rde_ui_scroller_pointer_down(&_s, (rde_vec_2F){ 0.0f, 0.0f }, now_s);
        for(u32 _k = 1; _k <= 6u; _k++) {
            rde_ui_scroller_pointer_moved(&_s, (rde_vec_2F){ 0.0f, 20.0f * (f32)_k }, now_s + 0.01 * (f64)_k);
        }
        rde_ui_scroller_pointer_up(&_s, now_s + 0.07);
        CHECK(_s.velocity.y > 1000.0f);   // (100 over the last 0.05 s... 2,000 a second)
        settle();
        u32 _moving = 0;
        b8  _moves  = true;
        for(u32 _k = 0; _k < 2000u && _moves; _k++) {
            _moves = fude_kit_scroller_update(&_s, 1.0f / 64.0f, 1.0e6f, 100.0f);
            CHECK(frame(1.0 / 64.0).draw);
            _moving += _moves ? 1u : 0u;
        }
        CHECK(_moving > 10u);
        CHECK(drawn_until_quiet(1.0 / 64.0, 1000u) <= 32u);   // then only its grace
        CHECK(!fude_kit_scroller_update(&_s, 1.0f / 64.0f, 1.0e6f, 100.0f));
        CHECK(!frame(1.0 / 64.0).draw);
    }

    // The canvas (drawing/ink/canvas.c): a finger held still is a timer for the long press, then the press is drawn;
    // a flick moves the page every frame until it stops.
    settle();
    {
        static fude_canvas _c;
        fude_canvas_init(&_c);
        fude_canvas_finger_down(&_c, 1u, (rde_vec_2F){ 0.0f, 0.0f });
        rde_vec_2F _at;
        now_s += 0.1;
        CHECK(!fude_canvas_long_press(&_c, &_at));
        const fude_redraw_verdict _v = frame(0.0);
        CHECK(!_v.draw && near(_v.sleep, FUDE_CANVAS_LONG_PRESS_TIME - 0.1));
        now_s += _v.sleep;
        CHECK(fude_canvas_long_press(&_c, &_at));
        CHECK(frame(0.0).draw);

        settle();
        fude_canvas_init(&_c);
        _c.coasting   = true;
        _c.velocity   = (rde_vec_2F){ 900.0f, 0.0f };
        _c.ticked_at  = now_s;
        u32 _frames = 0;
        while(_c.coasting && _frames < 5000u) {
            now_s += 1.0 / 64.0;
            fude_canvas_update(&_c, (rde_vec_2I){ 800, 600 }, (rde_vec_4I){ 0, 0, 0, 0 });
            CHECK(fude_redraw_end_frame_at(now_s, -1.0f).draw);
            _frames++;
        }
        CHECK(_frames > 10u && !_c.coasting);
    }

    // A notice (drawing/widgets/notice.c), often shown by a job's answer: drawn.
    settle();
    fude_notice_show("saved");
    CHECK(frame(0.0).draw);
}

// 20000 random frames: random gaps, events, timers, RDE's UI's words, requests and continuous spells, each frame's
// verdict checked against the rule (redraw.h) written out as its properties, with the state kept here on its own.
static u64 seed = 0x9E3779B97F4A7C15ull;
static u32 next(void) {
    seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
    return (u32)(seed >> 16);
}
static f64 unit(void) {
    return (f64)(next() % 1000000u) / 1000000.0;
}

static void test_random(void) {
    fude_redraw_set_continuous(false);
    settle();
    // The model: when the last change was, how many frames since, the soonest timer waiting, the last frame drawn.
    f64 _changed_at = -1e9, _due = -1.0;
    u32 _quiet      = 1000u;
    b8  _continuous = false;
    u32 _bad = 0;
    for(u32 _k = 0; _k < 20000u; _k++) {
        const f64 _dt = (next() % 4u == 0u) ? unit() * 1.5 : unit() * 0.05;
        now_s += _dt;
        b8 _change = false;
        if(next() % 10u == 0u) {
            fude_redraw_changed();
            _change = true;
        }
        f64 _asked = -1.0;
        const u32 _timers = next() % 8u == 0u ? 1u + next() % 3u : 0u;
        for(u32 _t = 0; _t < _timers; _t++) {
            const f64 _s = unit() * 2.0 - 0.1;
            fude_redraw_after(_s);
            const f64 _c = _s < 0.0 ? 0.0 : _s;
            _asked = _asked < 0.0 || _c < _asked ? _c : _asked;
        }
        f32 _ui = -1.0f;
        const u32 _u = next() % 30u;
        if(_u == 0u) { _ui = 0.0f; _change = true; }
        else if(_u == 1u) { _ui = (f32)(unit() * 0.6 + 0.001); _asked = _asked < 0.0 || (f64)_ui < _asked ? (f64)_ui : _asked; }
        if(next() % 50u == 0u) {
            fude_redraw_request();
            _change = true;
        }
        if(next() % 400u == 0u) {
            _continuous = !_continuous;
            fude_redraw_set_continuous(_continuous);
        }
        const fude_redraw_verdict _v = fude_redraw_end_frame_at(now_s, _ui);

        // The model's own frame: the timer kept as an absolute time, due a hair early; the grace.
        if(_asked >= 0.0 && (_due < 0.0 || now_s + _asked < _due)) {
            _due = now_s + _asked;
        }
        const b8 _is_due = _due >= 0.0 && now_s >= _due - FUDE_REDRAW_EARLY;
        if(_is_due) {
            _due = -1.0;
        }
        if(_change) { _quiet = 0u; _changed_at = now_s; } else if(_quiet < 1000000u) { _quiet++; }
        const b8 _grace = _quiet < FUDE_REDRAW_GRACE_FRAMES || now_s - _changed_at < FUDE_REDRAW_GRACE_SECONDS;
        const b8 _must  = _continuous || _grace || _is_due;
        if(_v.draw != _must) {
            _bad++;
        }
        // Unchanged: it sleeps until the timer waiting, or with none until something happens.
        if(!_must && (_due >= 0.0 ? !near(now_s + _v.sleep, _due) : !(_v.sleep < 0.0))) {
            _bad++;
        }
    }
    CHECK(_bad == 0u);
    fude_redraw_set_continuous(false);
}

int main(int _argc, char** _argv) {
    (void)_argc; (void)_argv;
    test_rule();
    test_check();
    test_sources();
    test_random();
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
