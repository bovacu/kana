// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/widgets/scroll.h"
#include "drawing/base/redraw.h"

// ===========================================================================
// See scroll.h.
//
// The fling's velocity is in screen units a second, smoothed over the drag's moves (each new move counts 30%), and
// dies away exponentially by FUDE_SCROLL_FRICTION once the pointer is up.
// ===========================================================================

// Starts a press at _screen: stops any fling, and the press may yet turn out to be a tap or a drag.
void fude_scroller_down(fude_scroller* _s, rde_vec_2F _screen, f64 _time) {
    _s->velocity   = 0.0f;
    _s->pressing   = true;
    _s->dragging   = false;
    _s->press_at   = _screen;
    _s->last_at    = _screen;
    _s->press_time = _time;
    _s->last_time  = _time;
}

// Moves the pressed pointer to _screen. Once it has moved further than FUDE_SCROLL_TAP_SLOP from where it went down it
// is a drag, which moves the content with the pointer and measures its speed for a fling.
void fude_scroller_moved(fude_scroller* _s, rde_vec_2F _screen, f64 _time) {
    if(!_s->pressing) {
        return;
    }

    const f32 _dx = _screen.x - _s->press_at.x;
    const f32 _dy = _screen.y - _s->press_at.y;
    if(!_s->dragging && _dx * _dx + _dy * _dy > FUDE_SCROLL_TAP_SLOP * FUDE_SCROLL_TAP_SLOP) {
        _s->dragging = true;
    }

    if(_s->dragging) {
        // The content follows the finger, so moving up shows what is further down.
        const f32 _step = _screen.y - _s->last_at.y;
        _s->offset += _step;

        const f64 _dt = _time - _s->last_time;
        if(_dt > 1e-4) {
            _s->velocity = 0.7f * _s->velocity + 0.3f * (f32)((f64)_step / _dt);
        }
    }

    _s->last_at   = _screen;
    _s->last_time = _time;
}

// Ends the press: a tap when it never became a drag and lasted less than FUDE_SCROLL_TAP_TIME. A drag whose pointer
// rested more than 0.08 s before lifting leaves no fling.
void fude_scroller_up(fude_scroller* _s, f64 _time) {
    if(!_s->pressing) {
        return;
    }
    _s->pressing = false;

    if(!_s->dragging && _time - _s->press_time < FUDE_SCROLL_TAP_TIME) {
        _s->tapped = true;
        _s->tap_at = _s->press_at;
    }

    // A finger that rested before lifting has no fling left.
    if(_time - _s->last_time > 0.08) {
        _s->velocity = 0.0f;
    }
}

// Stops any movement and forgets the press and any waiting tap.
void fude_scroller_stop(fude_scroller* _s) {
    _s->pressing = false;
    _s->dragging = false;
    _s->velocity = 0.0f;
    _s->tapped   = false;
}

// Carries a fling on (while faster than 1 unit a second and the pointer is up), then keeps the offset between 0 and
// _content_height - _view_height; hitting either end stops the fling. _dt is the frame's time in seconds.
void fude_scroller_update(fude_scroller* _s, f32 _dt, f32 _content_height, f32 _view_height) {
    if(!_s->pressing && rde_math_abs_f32(_s->velocity) > 1.0f) {
        fude_redraw_changed();   // (render on demand: the fling moves the list every frame)
        _s->offset   += _s->velocity * _dt;
        _s->velocity *= rde_math_exp_f32(-FUDE_SCROLL_FRICTION * _dt);
    }

    const f32 _limit = rde_math_max_f32(0.0f, _content_height - _view_height);
    if(_s->offset < 0.0f || _s->offset > _limit) {
        _s->offset   = rde_math_clamp_f32(_s->offset, 0.0f, _limit);
        _s->velocity = 0.0f;
    }
}

// Hands over the waiting tap's position once; false when there is none.
b8 fude_scroller_take_tap(fude_scroller* _s, rde_vec_2F* _at) {
    if(!_s->tapped) {
        return false;
    }
    _s->tapped = false;
    *_at       = _s->tap_at;
    return true;
}
