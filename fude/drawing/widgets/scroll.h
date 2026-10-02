#ifndef FUDE_SCROLL
#define FUDE_SCROLL

#include "rde.h"

// ===========================================================================
// A vertical scroller for full-screen lists (Browse's grid, the kana chart):
// one pointer drags the content — with inertia after a fling — and a press that
// neither moves nor lasts is a TAP, for the owner to resolve into an item.
//
// Positions are screen space (Y up). `offset` is how far the content has moved
// up: 0 is the top, and it grows as the finger pushes the content up.
// ===========================================================================

#define FUDE_SCROLL_TAP_SLOP 10.0f    // a press that moves further is a drag
#define FUDE_SCROLL_TAP_TIME 0.6      // ...or that lasts longer is not a tap
#define FUDE_SCROLL_FRICTION 4.0f     // inertia decay, per second

RDE_STRUCT {
    f32        offset;
    f32        velocity;

    b8         pressing;
    b8         dragging;
    rde_vec_2F press_at;
    rde_vec_2F last_at;
    f64        press_time;
    f64        last_time;

    b8         tapped;      // see fude_scroller_take_tap
    rde_vec_2F tap_at;
} fude_scroller;

void fude_scroller_down(fude_scroller* _s, rde_vec_2F _screen, f64 _time);
void fude_scroller_moved(fude_scroller* _s, rde_vec_2F _screen, f64 _time);
void fude_scroller_up(fude_scroller* _s, f64 _time);
// Stops any movement and forgets the pointer.
void fude_scroller_stop(fude_scroller* _s);

// Once a frame: inertia, and the offset kept within the content.
void fude_scroller_update(fude_scroller* _s, f32 _dt, f32 _content_height, f32 _view_height);

// A tap happened: where. Once per tap.
b8   fude_scroller_take_tap(fude_scroller* _s, rde_vec_2F* _at);

#endif
