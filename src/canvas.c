#include "canvas.h"
#include "draw.h"
#include "theme.h"

#include <math.h>
#include <string.h>

// ===========================================================================
// See canvas.h.
// ===========================================================================

f32 kana_canvas_square_units(KANA_SQUARES_SIZE_ _size) {
    return _size == KANA_SQUARES_SMALL ? 110.0f : _size == KANA_SQUARES_LARGE ? 240.0f : 160.0f;
}

void kana_canvas_init(kana_canvas* _canvas) {
    memset(_canvas, 0, sizeof(*_canvas));
    _canvas->square_size = KANA_SQUARES_MEDIUM;
    kana_canvas_reset_view(_canvas);
}

void kana_canvas_reset_view(kana_canvas* _canvas) {
    _canvas->view.offset = (rde_vec_2F){ 0.0f, 0.0f };
    _canvas->view.zoom   = 1.0f;
}

rde_vec_2F kana_canvas_from_screen(const kana_canvas* _canvas, rde_vec_2F _screen) {
    return (rde_vec_2F){
        (_screen.x - _canvas->view.offset.x) / _canvas->view.zoom,
        (_screen.y - _canvas->view.offset.y) / _canvas->view.zoom
    };
}

rde_vec_2F kana_canvas_to_screen(const kana_canvas* _canvas, rde_vec_2F _canvas_pos) {
    return (rde_vec_2F){
        _canvas_pos.x * _canvas->view.zoom + _canvas->view.offset.x,
        _canvas_pos.y * _canvas->view.zoom + _canvas->view.offset.y
    };
}

RDE_INTERNAL kana_canvas_finger* kana_canvas_find_finger(kana_canvas* _canvas, u64 _finger_id) {
    for(u32 _i = 0; _i < KANA_CANVAS_MAX_FINGERS; _i++) {
        if(_canvas->fingers[_i].active && _canvas->fingers[_i].finger_id == _finger_id) {
            return &_canvas->fingers[_i];
        }
    }

    return NULL;
}

RDE_INTERNAL u32 kana_canvas_active_fingers(const kana_canvas* _canvas) {
    u32 _count = 0;

    for(u32 _i = 0; _i < KANA_CANVAS_MAX_FINGERS; _i++) {
        _count += _canvas->fingers[_i].active ? 1u : 0u;
    }

    return _count;
}

// The fingers that move the page are the first two active slots. Returns the
// other one of that pair for _finger, NULL if it is alone; *_drives is false for
// a finger outside the pair (a third finger), which moves nothing.
RDE_INTERNAL kana_canvas_finger* kana_canvas_pair_partner(kana_canvas* _canvas, const kana_canvas_finger* _finger, b8* _drives) {
    kana_canvas_finger* _pair[2] = { NULL, NULL };
    u32                 _found   = 0;

    for(u32 _i = 0; _i < KANA_CANVAS_MAX_FINGERS && _found < 2; _i++) {
        if(_canvas->fingers[_i].active) {
            _pair[_found++] = &_canvas->fingers[_i];
        }
    }

    *_drives = _pair[0] == _finger || _pair[1] == _finger;
    return _pair[0] == _finger ? _pair[1] : _pair[0];
}

void kana_canvas_finger_down(kana_canvas* _canvas, u64 _finger_id, rde_vec_2F _screen) {
    if(kana_canvas_find_finger(_canvas, _finger_id) != NULL) {
        return;
    }

    // The first finger starts a gesture that might turn out to be a tap.
    if(kana_canvas_active_fingers(_canvas) == 0) {
        _canvas->tap_start    = rde_engine_get_time_now();
        _canvas->tap_fingers  = 0;
        _canvas->tap_spoiled  = false;
        _canvas->tap_view     = _canvas->view;
        _canvas->long_pressed = false;
    }

    for(u32 _i = 0; _i < KANA_CANVAS_MAX_FINGERS; _i++) {
        if(!_canvas->fingers[_i].active) {
            _canvas->fingers[_i] = (kana_canvas_finger){ .active = true, .finger_id = _finger_id, .position = _screen, .start = _screen };
            break;
        }
    }

    const u32 _down = kana_canvas_active_fingers(_canvas);
    _canvas->tap_fingers = _down > _canvas->tap_fingers ? _down : _canvas->tap_fingers;
}

void kana_canvas_finger_moved(kana_canvas* _canvas, u64 _finger_id, rde_vec_2F _screen) {
    kana_canvas_finger* _finger = kana_canvas_find_finger(_canvas, _finger_id);

    if(_finger == NULL) {
        return;
    }

    const f32 _travel_x = _screen.x - _finger->start.x;
    const f32 _travel_y = _screen.y - _finger->start.y;
    if(_travel_x * _travel_x + _travel_y * _travel_y > KANA_CANVAS_TAP_SLOP * KANA_CANVAS_TAP_SLOP) {
        _canvas->tap_spoiled = true;
    }

    b8                        _drives = false;
    const rde_vec_2F          _old    = _finger->position;
    const kana_canvas_finger* _other  = kana_canvas_pair_partner(_canvas, _finger, &_drives);
    kana_view*                _view   = &_canvas->view;

    if(!_drives) {
        _finger->position = _screen;
        return;
    }

    if(_other == NULL) {
        // One finger: the page follows it.
        _view->offset.x += _screen.x - _old.x;
        _view->offset.y += _screen.y - _old.y;
    } else {
        // Two: the canvas point under the old midpoint ends up under the new one,
        // scaled by how much the fingers spread — pinch and pan in one move.
        const rde_vec_2F _old_mid = { (_old.x + _other->position.x) * 0.5f, (_old.y + _other->position.y) * 0.5f };
        const rde_vec_2F _new_mid = { (_screen.x + _other->position.x) * 0.5f, (_screen.y + _other->position.y) * 0.5f };

        const f32 _old_dx   = _old.x - _other->position.x;
        const f32 _old_dy   = _old.y - _other->position.y;
        const f32 _new_dx   = _screen.x - _other->position.x;
        const f32 _new_dy   = _screen.y - _other->position.y;
        const f32 _old_dist = sqrtf(_old_dx * _old_dx + _old_dy * _old_dy);
        const f32 _new_dist = sqrtf(_new_dx * _new_dx + _new_dy * _new_dy);

        f32 _new_zoom = _view->zoom;
        // Fingers a hair apart give a ratio that is all noise: pan only.
        if(_old_dist > 4.0f && _new_dist > 4.0f) {
            _new_zoom = rde_math_clamp_f32(_view->zoom * (_new_dist / _old_dist), KANA_CANVAS_ZOOM_MIN, KANA_CANVAS_ZOOM_MAX);
        }

        const rde_vec_2F _anchor = kana_canvas_from_screen(_canvas, _old_mid);
        _view->zoom     = _new_zoom;
        _view->offset.x = _new_mid.x - _anchor.x * _new_zoom;
        _view->offset.y = _new_mid.y - _anchor.y * _new_zoom;
    }

    _finger->position = _screen;
}

KANA_CANVAS_TAP_ kana_canvas_finger_up(kana_canvas* _canvas, u64 _finger_id) {
    kana_canvas_finger* _finger = kana_canvas_find_finger(_canvas, _finger_id);

    if(_finger == NULL) {
        return KANA_CANVAS_TAP_NONE;
    }

    _finger->active = false;

    // Not over until the LAST finger lifts: fingers of one tap never lift at
    // exactly the same moment.
    if(kana_canvas_active_fingers(_canvas) > 0 || _canvas->tap_spoiled) {
        return KANA_CANVAS_TAP_NONE;
    }

    if(rde_engine_get_time_now() - _canvas->tap_start > KANA_CANVAS_TAP_MAX_TIME) {
        return KANA_CANVAS_TAP_NONE;
    }

    const KANA_CANVAS_TAP_ _tap = _canvas->tap_fingers == 2 ? KANA_CANVAS_TAP_TWO
                                : _canvas->tap_fingers == 3 ? KANA_CANVAS_TAP_THREE
                                                            : KANA_CANVAS_TAP_NONE;
    if(_tap != KANA_CANVAS_TAP_NONE) {
        // Two resting fingers still wobble the pinch a little: put the page back.
        _canvas->view = _canvas->tap_view;
    }

    return _tap;
}

b8 kana_canvas_long_press(kana_canvas* _canvas, rde_vec_2F* _screen) {
    if(_canvas->long_pressed || _canvas->tap_spoiled || _canvas->tap_fingers != 1 || kana_canvas_active_fingers(_canvas) != 1 ||
       rde_engine_get_time_now() - _canvas->tap_start < KANA_CANVAS_LONG_PRESS_TIME) {
        return false;
    }

    for(u32 _i = 0; _i < KANA_CANVAS_MAX_FINGERS; _i++) {
        if(_canvas->fingers[_i].active) {
            *_screen = _canvas->fingers[_i].position;
            // Done with this finger: it neither pans nor, on lifting, taps.
            _canvas->fingers[_i].active = false;
        }
    }

    _canvas->long_pressed = true;
    _canvas->tap_spoiled  = true;
    _canvas->view         = _canvas->tap_view;
    return true;
}

void kana_canvas_release_fingers(kana_canvas* _canvas) {
    for(u32 _i = 0; _i < KANA_CANVAS_MAX_FINGERS; _i++) {
        _canvas->fingers[_i].active = false;
    }

    // The pen came down: whatever the fingers were doing, it wasn't a tap.
    _canvas->tap_spoiled = true;
}

// Practice squares over what the screen shows: the squares' edges as lines, and
// through each the dashed centre cross (dashes set in the page, so they stay
// put while it pans). Square k spans ((k - 0.5)S, (k + 0.5)S): one is centred on
// the page's origin, where Reset puts the middle of the screen.
RDE_INTERNAL void kana_canvas_draw_squares(const kana_canvas* _canvas, rde_vec_2F _min, rde_vec_2F _max) {
    // The theme's canvas colours: the edges as its dots, the crosses between those
    // and the page.
    const kana_theme* _theme  = kana_theme_active();
    const f32         _s      = kana_canvas_square_units(_canvas->square_size);
    const f32         _screen = _s * _canvas->view.zoom;
    const rde_color   _edge   = _theme->page_dots;
    const rde_color   _guide  = {
        (u8)(((u32)_theme->page.r + (u32)_theme->page_dots.r * 2u) / 3u),
        (u8)(((u32)_theme->page.g + (u32)_theme->page_dots.g * 2u) / 3u),
        (u8)(((u32)_theme->page.b + (u32)_theme->page_dots.b * 2u) / 3u),
        _theme->page_dots.a
    };

    const f32 _ex0 = (floorf(_min.x / _s - 0.5f) + 0.5f) * _s;
    const f32 _ey0 = (floorf(_min.y / _s - 0.5f) + 0.5f) * _s;
    for(f32 _x = _ex0; _x <= _max.x; _x += _s) {
        kana_draw_line(kana_canvas_to_screen(_canvas, (rde_vec_2F){ _x, _min.y }), kana_canvas_to_screen(_canvas, (rde_vec_2F){ _x, _max.y }), 0.6f, _edge);
    }
    for(f32 _y = _ey0; _y <= _max.y; _y += _s) {
        kana_draw_line(kana_canvas_to_screen(_canvas, (rde_vec_2F){ _min.x, _y }), kana_canvas_to_screen(_canvas, (rde_vec_2F){ _max.x, _y }), 0.6f, _edge);
    }

    if(_screen < KANA_CANVAS_SQUARE_CROSS) {
        return;   // too small to write in anyway: the cross would only be noise
    }

    // The crosses: whole lines through the squares' centres, dashed.
    const f32 _dash   = fmaxf(4.0f, _screen / 40.0f) / _canvas->view.zoom;   // canvas units
    const f32 _period = _dash * 2.0f;
    const f32 _zoom   = _canvas->view.zoom;
    for(f32 _x = ceilf(_min.x / _s) * _s; _x <= _max.x; _x += _s) {
        for(f32 _y = floorf(_min.y / _period) * _period; _y <= _max.y; _y += _period) {
            const rde_vec_2F _c = kana_canvas_to_screen(_canvas, (rde_vec_2F){ _x, _y + _dash * 0.5f });
            rde_rendering_2d_draw_rectangle(_c, (rde_vec_2F){ 1.0f, _dash * _zoom }, _guide);
        }
    }
    for(f32 _y = ceilf(_min.y / _s) * _s; _y <= _max.y; _y += _s) {
        for(f32 _x = floorf(_min.x / _period) * _period; _x <= _max.x; _x += _period) {
            const rde_vec_2F _c = kana_canvas_to_screen(_canvas, (rde_vec_2F){ _x + _dash * 0.5f, _y });
            rde_rendering_2d_draw_rectangle(_c, (rde_vec_2F){ _dash * _zoom, 1.0f }, _guide);
        }
    }
}

void kana_canvas_draw_grid(const kana_canvas* _canvas, rde_vec_2I _window_size) {
    const kana_view* _view = &_canvas->view;

    // Spacing snaps by powers of two so the dots never crowd into a grey wash
    // when zoomed out, nor thin out to nothing when zoomed in.
    f32 _spacing = KANA_CANVAS_GRID_SPACING;
    while(_spacing * _view->zoom < KANA_CANVAS_GRID_MIN_SCREEN) { _spacing *= 2.0f; }
    while(_spacing * _view->zoom > KANA_CANVAS_GRID_MAX_SCREEN) { _spacing *= 0.5f; }

    // The canvas rect the screen currently shows.
    const f32        _hw  = (f32)_window_size.x * 0.5f;
    const f32        _hh  = (f32)_window_size.y * 0.5f;
    const rde_vec_2F _min = kana_canvas_from_screen(_canvas, (rde_vec_2F){ -_hw, -_hh });
    const rde_vec_2F _max = kana_canvas_from_screen(_canvas, (rde_vec_2F){  _hw,  _hh });

    if(_canvas->page.squares) {
        kana_canvas_draw_squares(_canvas, _min, _max);
        return;
    }

    const f32 _x0 = floorf(_min.x / _spacing) * _spacing;
    const f32 _y0 = floorf(_min.y / _spacing) * _spacing;

    for(f32 _y = _y0; _y <= _max.y; _y += _spacing) {
        for(f32 _x = _x0; _x <= _max.x; _x += _spacing) {
            const rde_vec_2F _screen = kana_canvas_to_screen(_canvas, (rde_vec_2F){ _x, _y });
            rde_rendering_2d_draw_rectangle(_screen, (rde_vec_2F){ KANA_CANVAS_GRID_DOT, KANA_CANVAS_GRID_DOT }, kana_theme_active()->page_dots);
        }
    }
}
