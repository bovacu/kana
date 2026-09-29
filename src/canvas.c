#include "canvas.h"

#include <math.h>
#include <string.h>

// ===========================================================================
// See canvas.h.
// ===========================================================================

void kana_canvas_init(kana_canvas* _canvas) {
    memset(_canvas, 0, sizeof(*_canvas));
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

// The other active finger, if there is one.
RDE_INTERNAL kana_canvas_finger* kana_canvas_other_finger(kana_canvas* _canvas, const kana_canvas_finger* _finger) {
    for(u32 _i = 0; _i < KANA_CANVAS_MAX_FINGERS; _i++) {
        if(_canvas->fingers[_i].active && &_canvas->fingers[_i] != _finger) {
            return &_canvas->fingers[_i];
        }
    }

    return NULL;
}

void kana_canvas_finger_down(kana_canvas* _canvas, u64 _finger_id, rde_vec_2F _screen) {
    if(kana_canvas_find_finger(_canvas, _finger_id) != NULL) {
        return;
    }

    for(u32 _i = 0; _i < KANA_CANVAS_MAX_FINGERS; _i++) {
        if(!_canvas->fingers[_i].active) {
            _canvas->fingers[_i] = (kana_canvas_finger){ .active = true, .finger_id = _finger_id, .position = _screen };
            return;
        }
    }
}

void kana_canvas_finger_moved(kana_canvas* _canvas, u64 _finger_id, rde_vec_2F _screen) {
    kana_canvas_finger* _finger = kana_canvas_find_finger(_canvas, _finger_id);

    if(_finger == NULL) {
        return;
    }

    const rde_vec_2F          _old   = _finger->position;
    const kana_canvas_finger* _other = kana_canvas_other_finger(_canvas, _finger);
    kana_view*                _view  = &_canvas->view;

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

void kana_canvas_finger_up(kana_canvas* _canvas, u64 _finger_id) {
    kana_canvas_finger* _finger = kana_canvas_find_finger(_canvas, _finger_id);

    if(_finger != NULL) {
        _finger->active = false;
    }
}

void kana_canvas_release_fingers(kana_canvas* _canvas) {
    for(u32 _i = 0; _i < KANA_CANVAS_MAX_FINGERS; _i++) {
        _canvas->fingers[_i].active = false;
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

    const f32 _x0 = floorf(_min.x / _spacing) * _spacing;
    const f32 _y0 = floorf(_min.y / _spacing) * _spacing;

    for(f32 _y = _y0; _y <= _max.y; _y += _spacing) {
        for(f32 _x = _x0; _x <= _max.x; _x += _spacing) {
            const rde_vec_2F _screen = kana_canvas_to_screen(_canvas, (rde_vec_2F){ _x, _y });
            rde_rendering_2d_draw_rectangle(_screen, (rde_vec_2F){ KANA_CANVAS_GRID_DOT, KANA_CANVAS_GRID_DOT }, KANA_CANVAS_GRID_COLOR);
        }
    }
}
