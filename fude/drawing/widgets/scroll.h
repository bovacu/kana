// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SCROLL
#define FUDE_SCROLL

#include "rde.h"

// ===========================================================================
// Scroller: a vertical scroller for full-screen lists (Browse's grid, the kana chart). One pointer drags the content,
// with inertia after a fling, and a press that neither moves nor lasts is a TAP, which the owner resolves into an
// item.
//
// Where it sits: drawing/widgets. The Library and reading cards (drawing/app) and the study apps' list screens use it.
//
// Its main types and their lifecycle: a fude_scroller is plain data inside its owner, zeroed to start; there is
// nothing to free. The owner feeds it the pointer's events and calls fude_scroller_update once a frame.
//
// Threads: main thread only.
//
// Positions are in screen space (Y up). `offset` is how far the content has moved up: 0 is the top, and it grows as
// the finger pushes the content up.
// ===========================================================================

#define FUDE_SCROLL_TAP_SLOP 10.0f    // A press that moves further than this is a drag...
#define FUDE_SCROLL_TAP_TIME 0.6      // ...and one that lasts longer than this, in seconds, is not a tap.
#define FUDE_SCROLL_FRICTION 4.0f     // How fast the inertia dies away, per second.

// A scroller's state. The owner reads `offset`; the rest is the scroller's own.
RDE_STRUCT {
    f32        offset;
    f32        velocity;

    b8         pressing;
    b8         dragging;
    rde_vec_2F press_at;
    rde_vec_2F last_at;
    f64        press_time;
    f64        last_time;

    b8         tapped;      // A tap is waiting to be taken (fude_scroller_take_tap)...
    rde_vec_2F tap_at;      // ...and where it was.
} fude_scroller;

// The pointer went down at _screen, moved to _screen, or went up, at time _time (seconds, on one clock). A move only
// counts while pressed; going up without having moved, soon enough, makes a tap.
void fude_scroller_down(fude_scroller* _s, rde_vec_2F _screen, f64 _time);
void fude_scroller_moved(fude_scroller* _s, rde_vec_2F _screen, f64 _time);
void fude_scroller_up(fude_scroller* _s, f64 _time);
// Stops any movement and forgets the pointer, and any tap not yet taken.
void fude_scroller_stop(fude_scroller* _s);

// Called once a frame: moves the content on by its inertia, and keeps the offset within the content (between 0 and
// _content_height - _view_height).
void fude_scroller_update(fude_scroller* _s, f32 _dt, f32 _content_height, f32 _view_height);

// Returns true once for each tap, with where it was in *_at; false when there is no tap waiting.
b8   fude_scroller_take_tap(fude_scroller* _s, rde_vec_2F* _at);

#endif
