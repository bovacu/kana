// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_REDRAW
#define FUDE_REDRAW

#include "rde.h"

// ===========================================================================
// Redraw: decides once a frame whether the frame is drawn, or the screen is left as it is and the app sleeps (RDE's
// render on demand), so a still screen costs nothing: nothing shaped, tessellated, uploaded or presented.
//
// Where it sits: drawing/base, so every layer above can report to it. The shells (study/app/shell.c, zoom/shell.c,
// and an app's own) start it, report every event, and end each frame with fude_redraw_frame_end from RDE's late
// update, after RDE's UI has moved. The launch flags (drawing/app/look.c) say when every frame is drawn.
//
// THE RULE (fude_redraw_end_frame_at). A frame is DRAWN when any of these holds; otherwise it is UNCHANGED:
//   1. Every frame is drawn (continuous): a developer's launch with flags (look.c: its sequences count frames, and
//      --perf, --shot and the screenshots must measure and look exactly as before), unless --on-demand is given.
//   2. Something changed within the grace period: in this frame or the FUDE_REDRAW_GRACE_FRAMES - 1 before it, or in
//      the last FUDE_REDRAW_GRACE_SECONDS. A change is an event handled, a fude_redraw_changed (something moved this
//      frame), a fude_redraw_request (another thread has something new), or RDE's UI moving on its own
//      (rde_ui_get_wake_after() == 0: a fling, a colour blend).
//   3. A timer is due: a time asked for with fude_redraw_after, or by RDE's UI (a focused field's caret). It draws its
//      one frame, with no grace period after it (a caret blinking would otherwise draw every frame).
// An unchanged frame calls rde_engine_frame_unchanged, and RDE sleeps until an event, a request or the soonest timer;
// with none of them, for as long as nothing happens. RDE itself always draws a frame after a window event or a return
// to the foreground (rde.h).
//
// How a source reports (one call each; docs/architecture.md, Render on demand, lists every source):
//   - it moved this frame, or moves every frame while it runs: fude_redraw_changed, each frame it does (from the
//     update or the render: a call made in a render counts for the next frame);
//   - it changes on its own at a known time (a fade's end, a hold's end, a caret's blink, midnight): fude_redraw_after
//     (the seconds until then), each frame or once;
//   - it waits on an answer it polls, with no event to say it came (a platform's answer read over JNI):
//     fude_redraw_after(FUDE_REDRAW_POLL) each frame it waits;
//   - another thread has finished something for the screen: fude_redraw_request, from that thread.
// A source that forgets to report leaves the screen stale until the next event. In a developer's build the check
// below finds it.
//
// THE CHECK (a developer's build: FUDE_REDRAW_CHECK). While the rule leaves frames undrawn, one is drawn anyway every
// FUDE_REDRAW_CHECK_SECONDS and compared, pixel for pixel, with the picture on screen (the first undrawn frame after
// the last drawn one, drawn and kept as the reference). A difference means something changed that no source reported:
// it is logged as a warning, with what shows (the screen's name), the box of pixels that changed, and the time since
// the last reported change, and the box is written out as it was and as it is (redraw_check_before.png and
// redraw_check_after.png, in the working folder).
//
// Design:
//   - Every change reports itself, and nothing else draws: a still screen is never drawn again, so a source that
//     forgets to report is a visible bug, which the check names in a developer's build instead of hiding it.
//   - The grace period draws what settles a frame or two after a change without having to report it: a layout
//     resolved the frame after, a picture decoded or uploaded a frame later, a short fade nobody reports. A frame
//     count alone would be too short at 120 Hz for a fade; a time alone too few frames after a slow frame (a file
//     opened in one 600 ms frame still gets its next frames drawn). Half a second of frames after a touch costs
//     nothing a person would measure.
//   - A due timer draws the frame it wakes, because a source drawn from the clock (a caret computed as it is drawn)
//     cannot tell in its update that its picture changed.
//   - The check compares the pixels (RDE's rde_window_take_screenshot_bytes: the frame drawn into a texture and read
//     back), because they are what a person sees, whatever drew them, and nothing else RDE offers tells two pictures
//     apart. A read back stalls the GPU for a frame, so it is done only while the screen is still, once a second: a
//     developer's build draws one frame a second when nothing changes. (A hash of a frame's 2D commands, asked of RDE,
//     would make it as reliable without the read back.)
//   - The rule and the check's plan are functions of a small state and the time they are given
//     (fude_redraw_end_frame_at, fude_redraw_check_step), and the comparison a function of two pictures
//     (fude_redraw_compare), so the suite (tests/redraw) drives them without the engine.
//
// Its state: one per process, in redraw.c's statics, made by fude_redraw_start; the check's reference picture on the
// std allocator, as big as the window.
// Threads: the main thread, except fude_redraw_request (any thread: its flag is guarded by a mutex). The check's
// picture arrives in RDE's screenshot callback, on the main thread, once the frame it was taken of is drawn (in
// either rendering mode). One picture is asked for at a time: while one is not delivered (no frame drawn: a minimised
// window), the check asks for no other; and a probe answered after a change was reported is kept as the new reference,
// not compared (it shows the change).
// Files: the check writes redraw_check_before.png and redraw_check_after.png in the working folder on a difference.
// ===========================================================================

// Frames drawn after a change: the frame of the change and the 3 after it, at least.
#define FUDE_REDRAW_GRACE_FRAMES  4u
// Seconds of frames drawn after a change, at least.
#define FUDE_REDRAW_GRACE_SECONDS 0.5
// How often a source waiting on a polled answer looks again (fude_redraw_after(FUDE_REDRAW_POLL)): 10 a second, soon
// enough that an answer is not seen late.
#define FUDE_REDRAW_POLL          0.1
// A timer counts as due this much before its time, so a wake that lands a hair early (the sleep is in whole
// milliseconds) does not sleep again for the rest.
#define FUDE_REDRAW_EARLY         0.002
// How often the check draws a still screen to compare it (a developer's build).
#define FUDE_REDRAW_CHECK_SECONDS 1.0
// The check is on in a developer's build (RDE_DEBUG), never in a release or a measuring build.
#if defined(RDE_DEBUG)
#define FUDE_REDRAW_CHECK 1
#else
#define FUDE_REDRAW_CHECK 0
#endif

// What the rule decided for one frame (fude_redraw_end_frame_at).
typedef struct {
    b8  draw;    // true: the frame is drawn; false: unchanged, the app sleeps
    f64 sleep;   // when unchanged: the longest it may sleep, in seconds (< 0: until an event or a request)
} fude_redraw_verdict;

// What the check does with a frame (fude_redraw_check_step). NONE: nothing (the rule's verdict stands). REFERENCE:
// the frame is drawn anyway and its picture kept, as what the screen shows. PROBE: the frame is drawn anyway and its
// picture compared with the one kept.
typedef enum { FUDE_REDRAW_CHECK_NONE = 0, FUDE_REDRAW_CHECK_REFERENCE, FUDE_REDRAW_CHECK_PROBE } FUDE_REDRAW_CHECK_;

// How two pictures differ (fude_redraw_compare): how many pixels, and the box round them (pixels, from the top left).
typedef struct {
    u32 pixels;   // 0: the same
    u32 x, y, w, h;
} fude_redraw_difference;

// Starts the module, first thing in the app's init: makes the request's mutex, and counts the start as a change (the
// first frames are drawn). Main thread.
void fude_redraw_start(void);
// Sets whether every frame is drawn, as today without render on demand (rule 1). look.c sets it from the flags.
void fude_redraw_set_continuous(b8 _continuous);
// Whether every frame is drawn (rule 1).
b8   fude_redraw_continuous(void);
// Turns the check on or off (FUDE_REDRAW_CHECK until then): for the suite.
void fude_redraw_set_check(b8 _on);

// Something on screen changed this frame, or moves every frame while it runs (rule 2). Main thread.
void fude_redraw_changed(void);
// Something changes on its own in _seconds from now, on the engine's clock (rule 3); 0 or less is the next frame. The
// soonest asked for wins, and it is kept until it is due. Main thread.
void fude_redraw_after(f64 _seconds);
// Another thread has something new for the screen: wakes the app (rde_engine_request_frame), and its next frame
// counts as a change (rule 2). Any thread.
void fude_redraw_request(void);
// When the last change was seen, on the engine's clock (a frame's end; far in the past before any): for work that
// must run a while after any change, such as the canvas's instruments' file. Main thread.
f64  fude_redraw_last_change(void);

// Ends the frame: applies the rule (fude_redraw_end_frame_at) at the engine's time, with RDE's UI's word
// (rde_ui_get_wake_after), then the check (fude_redraw_check_step), and tells RDE (rde_engine_set_render_on_demand,
// rde_engine_frame_unchanged, rde_engine_wake_after; the check's picture with rde_window_take_screenshot_bytes).
// _window: the app's window; _shown: what it shows, by name (the screen on top, or the page), for the check's
// warning. Called by the shells from RDE's late update, the last thing in the frame before it draws.
void fude_redraw_frame_end(rde_window* _window, const c8* _shown);

// The rule for a frame ending at _now (seconds, any steady clock), where RDE's UI says _ui_wake
// (rde_ui_get_wake_after: 0 moving, above 0 a timer, below 0 nothing): takes this frame's reports, decides, and starts
// the next frame's. It touches nothing in the engine; fude_redraw_frame_end hands what it returns to RDE.
fude_redraw_verdict fude_redraw_end_frame_at(f64 _now, f32 _ui_wake);
// The check's plan for a frame ending at _now that the rule draws (_draw) or leaves undrawn (THE CHECK above). A
// drawn frame puts up a new picture, so the next undrawn one is the new reference. Does nothing while every frame is
// drawn, or with the check off.
FUDE_REDRAW_CHECK_ fude_redraw_check_step(b8 _draw, f64 _now);
// The seconds from _now until the check's next probe, so an undrawn frame sleeps no longer (< 0: none planned).
f64 fude_redraw_check_wake(f64 _now);
// Compares two pictures of _w x _h pixels, 4 bytes a pixel, rows from the top: how many pixels differ, and their box.
fude_redraw_difference fude_redraw_compare(const u8* _a, const u8* _b, u32 _w, u32 _h);
// The check's picture asked for (_what: REFERENCE or PROBE) at _now, of a frame showing _shown: what
// fude_redraw_frame_end does as it asks RDE for it; until it is delivered, fude_redraw_check_step plans no other.
void fude_redraw_check_asked(FUDE_REDRAW_CHECK_ _what, const c8* _shown, f64 _now);
// The picture asked for, delivered (_bytes: _w x _h, 4 bytes a pixel, rows from the top; RDE's screenshot callback): a
// probe's compared with the one kept — when nothing was reported changed since it was asked — and a difference warned
// of; either way it is kept, as what the screen shows now. Returns the difference warned of (0 pixels: none).
fude_redraw_difference fude_redraw_check_delivered(const u8* _bytes, u32 _w, u32 _h);

#endif
