#include "scroll.h"

#include <math.h>

// ===========================================================================
// See scroll.h.
// ===========================================================================

void kana_scroller_down(kana_scroller* _s, rde_vec_2F _screen, f64 _time) {
    _s->velocity   = 0.0f;
    _s->pressing   = true;
    _s->dragging   = false;
    _s->press_at   = _screen;
    _s->last_at    = _screen;
    _s->press_time = _time;
    _s->last_time  = _time;
}

void kana_scroller_moved(kana_scroller* _s, rde_vec_2F _screen, f64 _time) {
    if(!_s->pressing) {
        return;
    }

    const f32 _dx = _screen.x - _s->press_at.x;
    const f32 _dy = _screen.y - _s->press_at.y;
    if(!_s->dragging && _dx * _dx + _dy * _dy > KANA_SCROLL_TAP_SLOP * KANA_SCROLL_TAP_SLOP) {
        _s->dragging = true;
    }

    if(_s->dragging) {
        // The content follows the finger: moving up shows what is further down.
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

void kana_scroller_up(kana_scroller* _s, f64 _time) {
    if(!_s->pressing) {
        return;
    }
    _s->pressing = false;

    if(!_s->dragging && _time - _s->press_time < KANA_SCROLL_TAP_TIME) {
        _s->tapped = true;
        _s->tap_at = _s->press_at;
    }

    // A finger that rested before lifting has no fling left.
    if(_time - _s->last_time > 0.08) {
        _s->velocity = 0.0f;
    }
}

void kana_scroller_stop(kana_scroller* _s) {
    _s->pressing = false;
    _s->dragging = false;
    _s->velocity = 0.0f;
    _s->tapped   = false;
}

void kana_scroller_update(kana_scroller* _s, f32 _dt, f32 _content_height, f32 _view_height) {
    if(!_s->pressing && fabsf(_s->velocity) > 1.0f) {
        _s->offset   += _s->velocity * _dt;
        _s->velocity *= expf(-KANA_SCROLL_FRICTION * _dt);
    }

    const f32 _limit = fmaxf(0.0f, _content_height - _view_height);
    if(_s->offset < 0.0f || _s->offset > _limit) {
        _s->offset   = rde_math_clamp_f32(_s->offset, 0.0f, _limit);
        _s->velocity = 0.0f;
    }
}

b8 kana_scroller_take_tap(kana_scroller* _s, rde_vec_2F* _at) {
    if(!_s->tapped) {
        return false;
    }
    _s->tapped = false;
    *_at       = _s->tap_at;
    return true;
}
