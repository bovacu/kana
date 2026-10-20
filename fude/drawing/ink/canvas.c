// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/ink/canvas.h"
#include "drawing/base/redraw.h"
#include "drawing/base/draw.h"
#include "drawing/base/theme.h"

// ===========================================================================
// See canvas.h.
//
// A flick is a velocity in screen units a second, measured while fingers pan (smoothed over the last moves) and,
// when reading, carried on by fude_canvas_update after the last finger lifts, decaying exponentially by
// FUDE_CANVAS_FRICTION. The reading frame's bounds are applied after every change of the view
// (fude_canvas_reader_clamp).
// ===========================================================================

// Declared here because fude_canvas_update, above its definition, needs it.
RDE_INTERNAL u32 fude_canvas_active_fingers(const fude_canvas* _canvas);

// Returns a paper size in canvas units: 110 small, 240 large, 160 medium (and for any other value).
f32 fude_canvas_paper_units(FUDE_PAPER_SIZE_ _size) {
    return _size == FUDE_PAPER_SMALL ? 110.0f : _size == FUDE_PAPER_LARGE ? 240.0f : 160.0f;
}

// Clears the canvas (the dots, no page turned, no fingers), sets medium paper and resets the view.
void fude_canvas_init(fude_canvas* _canvas) {
    rde_memset(_canvas, 0, sizeof(*_canvas));
    _canvas->paper_size = FUDE_PAPER_MEDIUM;
    fude_canvas_reset_view(_canvas);
}

// Keeps a reader's view on its pages, in screen units: the zoom no less than the fit (100%), the pages centred across
// when they fit the width and otherwise no further in than the gutters, the first page's top no lower than under the
// menu, and the last page's bottom no higher than the room under it. A flick's speed is stopped along any direction
// where the view was held back. Does nothing when not reading.
RDE_INTERNAL void fude_canvas_reader_clamp(fude_canvas* _canvas) {
    const fude_canvas_reader* _r = &_canvas->reader;
    fude_view*                _v = &_canvas->view;
    if(!_r->on || _r->fit <= 0.0f) {
        return;
    }
    _v->zoom = rde_math_clamp_f32(_v->zoom, _r->fit, rde_math_max_f32(_r->fit, FUDE_CANVAS_ZOOM_MAX));
    // Across: centred when the pages fit; zoomed in, their edges come no further in than the gutters.
    const f32 _centre = (f32)(_r->safe.x - _r->safe.z) * 0.5f;
    const f32 _half   = (f32)(_r->window.x - _r->safe.x - _r->safe.z) * 0.5f - FUDE_CANVAS_READER_GUTTER;
    const f32 _pages  = _r->width * _v->zoom * 0.5f;
    if(_pages <= _half + 0.5f) {
        _v->offset.x = _centre;
        _canvas->velocity.x = 0.0f;
    } else {
        const f32 _x = rde_math_clamp_f32(_v->offset.x, _centre + _half - _pages, _centre - _half + _pages);
        _canvas->velocity.x = _x != _v->offset.x ? 0.0f : _canvas->velocity.x;
        _v->offset.x        = _x;
    }
    // Up and down: the first page's top no lower than under the menu, the last page's bottom no higher than its room.
    // _lowest and _highest bound the offset; when the pages are shorter than the screen, the first page's top wins.
    // _r->bottom is below the first page's top (0), so it is negative.
    const f32 _lowest  = (f32)_r->window.y * 0.5f - (f32)_r->safe.y - FUDE_CANVAS_READER_TOP;
    const f32 _highest = -(f32)_r->window.y * 0.5f + (f32)_r->safe.w + FUDE_CANVAS_READER_BOTTOM - _r->bottom * _v->zoom;
    const f32 _y       = _highest < _lowest ? _lowest : rde_math_clamp_f32(_v->offset.y, _lowest, _highest);
    _canvas->velocity.y = _y != _v->offset.y ? 0.0f : _canvas->velocity.y;
    _v->offset.y        = _y;
}

// Puts the view back at zoom 1 with the page's origin at the screen's centre, or, when reading, the first page's top
// at 100%; stops any flick.
void fude_canvas_reset_view(fude_canvas* _canvas) {
    _canvas->view.offset = (rde_vec_2F){ 0.0f, 0.0f };
    _canvas->view.zoom   = 1.0f;
    _canvas->coasting    = false;
    _canvas->velocity    = (rde_vec_2F){ 0.0f, 0.0f };
    if(_canvas->reader.on && _canvas->reader.fit > 0.0f) {
        // Reading: show the first page's top, at 100%.
        _canvas->view.zoom     = _canvas->reader.fit;
        _canvas->view.offset.x = (f32)(_canvas->reader.safe.x - _canvas->reader.safe.z) * 0.5f;
        _canvas->view.offset.y = (f32)_canvas->reader.window.y * 0.5f - (f32)_canvas->reader.safe.y - FUDE_CANVAS_READER_TOP;
    }
}

// Returns the zoom at which pages _width wide fill the screen between the safe insets and the gutters, within the
// zoom's limits.
f32 fude_canvas_reader_fit(f32 _width, rde_vec_2I _window, rde_vec_4I _safe) {
    const f32 _room = (f32)(_window.x - _safe.x - _safe.z) - 2.0f * FUDE_CANVAS_READER_GUTTER;
    return rde_math_clamp_f32(_room / rde_math_max_f32(_width, 1.0f), FUDE_CANVAS_ZOOM_MIN, FUDE_CANVAS_ZOOM_MAX);
}

// Turns the reading frame on for pages _width wide down to canvas y _bottom, or off. When it turns on, the screen it
// was fitted to is forgotten, so the next update fits it.
void fude_canvas_set_reader(fude_canvas* _canvas, b8 _on, f32 _width, f32 _bottom) {
    if(!_on) {
        _canvas->reader.on = false;
        return;
    }
    if(!_canvas->reader.on) {
        _canvas->reader.window = (rde_vec_2I){ 0, 0 };   // Fitted on the next update.
    }
    _canvas->reader.on     = true;
    _canvas->reader.width  = _width;
    _canvas->reader.bottom = _bottom;
}

// Once a frame: fits the reading frame again when the screen or its safe area changed, carries a flick on while no
// finger is down, and keeps a reader's view on its pages. The frame's step is capped at 0.1 s, so a long pause does
// not throw the page.
void fude_canvas_update(fude_canvas* _canvas, rde_vec_2I _window, rde_vec_4I _safe) {
    const f64 _now = rde_engine_get_time_now();
    const f32 _dt  = _canvas->ticked_at > 0.0 ? (f32)rde_math_min_f64(0.1, _now - _canvas->ticked_at) : 0.0f;
    _canvas->ticked_at = _now;
    fude_canvas_reader* _r = &_canvas->reader;
    fude_view*          _v = &_canvas->view;
    if(_r->on && (_window.x != _r->window.x || _window.y != _r->window.y || rde_memcmp(&_safe, &_r->safe, sizeof(_safe)) != 0)) {
        // Fit the pages again, keeping the zoom relative to 100% and the same canvas point at the top middle.
        const f32  _fit = fude_canvas_reader_fit(_r->width, _window, _safe);
        if(_r->window.x > 0 && _r->fit > 0.0f) {
            const rde_vec_2F _top = { ((f32)(_r->safe.x - _r->safe.z) * 0.5f - _v->offset.x) / _v->zoom,
                                      ((f32)_r->window.y * 0.5f - (f32)_r->safe.y - _v->offset.y) / _v->zoom };
            _v->zoom     = _v->zoom / _r->fit * _fit;
            _v->offset.x = (f32)(_safe.x - _safe.z) * 0.5f - _top.x * _v->zoom;
            _v->offset.y = (f32)_window.y * 0.5f - (f32)_safe.y - _top.y * _v->zoom;
        }
        _r->fit    = _fit;
        _r->window = _window;
        _r->safe   = _safe;
        fude_redraw_changed();   // (render on demand: a safe area can arrive frames after the launch, with no event)
    }
    // Carry a flick on, slowing it down; it stops once it is slower than FUDE_CANVAS_COAST_MIN both ways.
    if(_canvas->coasting && fude_canvas_active_fingers(_canvas) == 0) {
        fude_redraw_changed();   // (render on demand: the flick moves the page every frame)
        _v->offset.x += _canvas->velocity.x * _dt;
        _v->offset.y += _canvas->velocity.y * _dt;
        const f32 _keep = rde_math_exp_f32(-FUDE_CANVAS_FRICTION * _dt);
        _canvas->velocity.x *= _keep;
        _canvas->velocity.y *= _keep;
        if(rde_math_abs_f32(_canvas->velocity.x) < FUDE_CANVAS_COAST_MIN && rde_math_abs_f32(_canvas->velocity.y) < FUDE_CANVAS_COAST_MIN) {
            _canvas->coasting = false;
        }
    }
    fude_canvas_reader_clamp(_canvas);
}

// Returns the zoom relative to the fit when reading, else the zoom itself.
f32 fude_canvas_zoom_shown(const fude_canvas* _canvas) {
    return _canvas->reader.on && _canvas->reader.fit > 0.0f ? _canvas->view.zoom / _canvas->reader.fit : _canvas->view.zoom;
}

// Converts a screen point to canvas space: canvas = (screen - offset) / zoom.
rde_vec_2F fude_canvas_from_screen(const fude_canvas* _canvas, rde_vec_2F _screen) {
    return (rde_vec_2F){
        (_screen.x - _canvas->view.offset.x) / _canvas->view.zoom,
        (_screen.y - _canvas->view.offset.y) / _canvas->view.zoom
    };
}

// Converts a canvas point to screen space: screen = canvas * zoom + offset.
rde_vec_2F fude_canvas_to_screen(const fude_canvas* _canvas, rde_vec_2F _canvas_pos) {
    return (rde_vec_2F){
        _canvas_pos.x * _canvas->view.zoom + _canvas->view.offset.x,
        _canvas_pos.y * _canvas->view.zoom + _canvas->view.offset.y
    };
}

// Returns the tracked finger with this id, or NULL when it is not down (or was dropped).
RDE_INTERNAL fude_canvas_finger* fude_canvas_find_finger(fude_canvas* _canvas, u64 _finger_id) {
    for(u32 _i = 0; _i < FUDE_CANVAS_MAX_FINGERS; _i++) {
        if(_canvas->fingers[_i].active && _canvas->fingers[_i].finger_id == _finger_id) {
            return &_canvas->fingers[_i];
        }
    }

    return NULL;
}

// Returns how many tracked fingers are down.
RDE_INTERNAL u32 fude_canvas_active_fingers(const fude_canvas* _canvas) {
    u32 _count = 0;

    for(u32 _i = 0; _i < FUDE_CANVAS_MAX_FINGERS; _i++) {
        _count += _canvas->fingers[_i].active ? 1u : 0u;
    }

    return _count;
}

// The fingers that move the page are the first two active slots. Returns the other finger of that pair, or NULL when
// _finger is alone. *_drives is set false for a finger outside the pair (a third finger), which moves nothing.
RDE_INTERNAL fude_canvas_finger* fude_canvas_pair_partner(fude_canvas* _canvas, const fude_canvas_finger* _finger, b8* _drives) {
    fude_canvas_finger* _pair[2] = { NULL, NULL };
    u32                 _found   = 0;

    for(u32 _i = 0; _i < FUDE_CANVAS_MAX_FINGERS && _found < 2; _i++) {
        if(_canvas->fingers[_i].active) {
            _pair[_found++] = &_canvas->fingers[_i];
        }
    }

    *_drives = _pair[0] == _finger || _pair[1] == _finger;
    return _pair[0] == _finger ? _pair[1] : _pair[0];
}

// Tracks a new finger in the first free slot (a sixth finger is ignored) and stops any flick. The first finger down
// starts a gesture that may turn out to be a tap or a long press.
void fude_canvas_finger_down(fude_canvas* _canvas, u64 _finger_id, rde_vec_2F _screen) {
    if(fude_canvas_find_finger(_canvas, _finger_id) != NULL) {
        return;
    }

    _canvas->coasting = false;   // A finger down stops a flick.
    // The first finger starts a gesture that might turn out to be a tap.
    if(fude_canvas_active_fingers(_canvas) == 0) {
        _canvas->velocity = (rde_vec_2F){ 0.0f, 0.0f };
        _canvas->tap_start    = rde_engine_get_time_now();
        _canvas->tap_fingers  = 0;
        _canvas->tap_spoiled  = false;
        _canvas->tap_view     = _canvas->view;
        _canvas->long_pressed = false;
    }

    for(u32 _i = 0; _i < FUDE_CANVAS_MAX_FINGERS; _i++) {
        if(!_canvas->fingers[_i].active) {
            _canvas->fingers[_i] = (fude_canvas_finger){ .active = true, .finger_id = _finger_id, .position = _screen, .start = _screen };
            break;
        }
    }

    const u32 _down = fude_canvas_active_fingers(_canvas);
    _canvas->tap_fingers = _down > _canvas->tap_fingers ? _down : _canvas->tap_fingers;
}

// Moves a finger: one finger pans the page; with two, the page pans and zooms so the canvas point under their middle
// stays under it. A finger that travels past the slop spoils a tap. The page's speed is measured for a flick.
void fude_canvas_finger_moved(fude_canvas* _canvas, u64 _finger_id, rde_vec_2F _screen) {
    fude_canvas_finger* _finger = fude_canvas_find_finger(_canvas, _finger_id);

    if(_finger == NULL) {
        return;
    }

    const f32 _travel_x = _screen.x - _finger->start.x;
    const f32 _travel_y = _screen.y - _finger->start.y;
    if(_travel_x * _travel_x + _travel_y * _travel_y > FUDE_CANVAS_TAP_SLOP * FUDE_CANVAS_TAP_SLOP) {
        _canvas->tap_spoiled = true;
    }

    b8                        _drives = false;
    const rde_vec_2F          _old    = _finger->position;
    const fude_canvas_finger* _other  = fude_canvas_pair_partner(_canvas, _finger, &_drives);
    fude_view*                _view   = &_canvas->view;

    if(!_drives) {
        _finger->position = _screen;
        return;
    }

    const fude_view _before = *_view;
    rde_vec_2F      _moved  = { _screen.x - _old.x, _screen.y - _old.y };   // How far the page (or the middle) went.
    if(_other == NULL) {
        // One finger: the page follows it.
        _view->offset.x += _screen.x - _old.x;
        _view->offset.y += _screen.y - _old.y;
    } else {
        // Two fingers: the canvas point under the old midpoint ends up under the new one, with the zoom scaled by how
        // much the fingers spread, so pinching and panning happen in one move.
        const rde_vec_2F _old_mid = { (_old.x + _other->position.x) * 0.5f, (_old.y + _other->position.y) * 0.5f };
        const rde_vec_2F _new_mid = { (_screen.x + _other->position.x) * 0.5f, (_screen.y + _other->position.y) * 0.5f };
        _moved = (rde_vec_2F){ _new_mid.x - _old_mid.x, _new_mid.y - _old_mid.y };

        const f32 _old_dx   = _old.x - _other->position.x;
        const f32 _old_dy   = _old.y - _other->position.y;
        const f32 _new_dx   = _screen.x - _other->position.x;
        const f32 _new_dy   = _screen.y - _other->position.y;
        const f32 _old_dist = rde_math_sqrt_f32(_old_dx * _old_dx + _old_dy * _old_dy);
        const f32 _new_dist = rde_math_sqrt_f32(_new_dx * _new_dx + _new_dy * _new_dy);

        f32 _new_zoom = _view->zoom;
        // Fingers less than 4 units apart give a ratio that is all noise, so they only pan.
        if(_old_dist > 4.0f && _new_dist > 4.0f) {
            _new_zoom = rde_math_clamp_f32(_view->zoom * (_new_dist / _old_dist), FUDE_CANVAS_ZOOM_MIN, FUDE_CANVAS_ZOOM_MAX);
        }

        const rde_vec_2F _anchor = fude_canvas_from_screen(_canvas, _old_mid);
        _view->zoom     = _new_zoom;
        _view->offset.x = _new_mid.x - _anchor.x * _new_zoom;
        _view->offset.y = _new_mid.y - _anchor.y * _new_zoom;
    }
    fude_canvas_reader_clamp(_canvas);

    // Measure how fast the page moves (one finger's pan, or two fingers' middle) for a flick to carry on. Only what
    // the reading frame let through counts: a move across that was held back gives no speed across. Moves less than
    // 0.5 ms or more than 0.2 s apart are not measured, and each new speed is blended 70% into the old.
    const f64 _now = rde_engine_get_time_now();
    const f32 _dt  = (f32)(_now - _canvas->moved_at);
    if(_dt > 0.0005f && _dt < 0.2f) {
        const b8         _across = _view->offset.x != _before.offset.x || _view->zoom != _before.zoom;
        const rde_vec_2F _speed  = { _across ? _moved.x / _dt : 0.0f, _moved.y / _dt };
        _canvas->velocity.x      = _canvas->velocity.x * 0.3f + _speed.x * 0.7f;
        _canvas->velocity.y      = _canvas->velocity.y * 0.3f + _speed.y * 0.7f;
    }
    _canvas->moved_at = _now;

    _finger->position = _screen;
}

// Lifts a finger. When the last one lifts it may start a flick (when reading), and returns a two- or three-finger tap
// when the gesture was one: no finger moved, it took at most FUDE_CANVAS_TAP_MAX_TIME, and it was not a long press.
FUDE_CANVAS_TAP_ fude_canvas_finger_up(fude_canvas* _canvas, u64 _finger_id) {
    fude_canvas_finger* _finger = fude_canvas_find_finger(_canvas, _finger_id);

    if(_finger == NULL) {
        return FUDE_CANVAS_TAP_NONE;
    }

    _finger->active = false;

    // When reading, the last finger lifting from a page that moved within the last 0.08 s, faster than four times
    // FUDE_CANVAS_COAST_MIN, lets the page carry on.
    if(fude_canvas_active_fingers(_canvas) == 0 && _canvas->reader.on && _canvas->tap_spoiled && !_canvas->long_pressed &&
       rde_engine_get_time_now() - _canvas->moved_at < 0.08 &&
       (rde_math_abs_f32(_canvas->velocity.x) > FUDE_CANVAS_COAST_MIN * 4.0f || rde_math_abs_f32(_canvas->velocity.y) > FUDE_CANVAS_COAST_MIN * 4.0f)) {
        _canvas->coasting = true;
    }

    // The gesture is not over until the LAST finger lifts, because the fingers of one tap never lift at exactly the
    // same moment.
    if(fude_canvas_active_fingers(_canvas) > 0 || _canvas->tap_spoiled) {
        return FUDE_CANVAS_TAP_NONE;
    }

    if(rde_engine_get_time_now() - _canvas->tap_start > FUDE_CANVAS_TAP_MAX_TIME) {
        return FUDE_CANVAS_TAP_NONE;
    }

    const FUDE_CANVAS_TAP_ _tap = _canvas->tap_fingers == 2 ? FUDE_CANVAS_TAP_TWO
                                : _canvas->tap_fingers == 3 ? FUDE_CANVAS_TAP_THREE
                                                            : FUDE_CANVAS_TAP_NONE;
    if(_tap != FUDE_CANVAS_TAP_NONE) {
        // Two resting fingers still wobble the pinch a little, so put the page back where it was.
        _canvas->view = _canvas->tap_view;
    }

    return _tap;
}

// Returns true once per gesture when one finger, the only one all gesture, has been held still for
// FUDE_CANVAS_LONG_PRESS_TIME. The finger is dropped and the view put back as it was. See the header.
b8 fude_canvas_long_press(fude_canvas* _canvas, rde_vec_2F* _screen) {
    if(_canvas->long_pressed || _canvas->tap_spoiled || _canvas->tap_fingers != 1 || fude_canvas_active_fingers(_canvas) != 1) {
        return false;
    }
    const f64 _held = rde_engine_get_time_now() - _canvas->tap_start;
    if(_held < FUDE_CANVAS_LONG_PRESS_TIME) {
        fude_redraw_after(FUDE_CANVAS_LONG_PRESS_TIME - _held);   // (render on demand: a finger held still sends nothing)
        return false;
    }

    for(u32 _i = 0; _i < FUDE_CANVAS_MAX_FINGERS; _i++) {
        if(_canvas->fingers[_i].active) {
            *_screen = _canvas->fingers[_i].position;
            // Drop the finger, so it neither pans nor, on lifting, makes a tap.
            _canvas->fingers[_i].active = false;
        }
    }

    _canvas->long_pressed = true;
    _canvas->tap_spoiled  = true;
    _canvas->view         = _canvas->tap_view;
    return true;
}

// Drops every finger and spoils the tap.
void fude_canvas_release_fingers(fude_canvas* _canvas) {
    for(u32 _i = 0; _i < FUDE_CANVAS_MAX_FINGERS; _i++) {
        _canvas->fingers[_i].active = false;
    }

    // The pen came down, so whatever the fingers were doing was not a tap.
    _canvas->tap_spoiled = true;
}

// Draws practice squares over the canvas rectangle _min.._max (what the screen shows): the squares' edges as lines,
// and through each square the dashed centre cross. The dashes are placed in canvas units, so they stay put while the
// page pans. Square k spans ((k - 0.5)S, (k + 0.5)S), so one square is centred on the page's origin, where Reset puts
// the middle of the screen.
RDE_INTERNAL void fude_canvas_draw_squares(const fude_canvas* _canvas, rde_vec_2F _min, rde_vec_2F _max) {
    // The theme's canvas colours: the edges in its dot colour, the crosses two thirds of the way from the page's colour
    // to the dots'.
    const fude_theme* _theme  = fude_theme_active();
    const f32         _s      = fude_canvas_paper_units(_canvas->paper_size);
    const f32         _screen = _s * _canvas->view.zoom;
    const rde_color   _edge   = _theme->page_dots;
    const rde_color   _guide  = {
        (u8)(((u32)_theme->page.r + (u32)_theme->page_dots.r * 2u) / 3u),
        (u8)(((u32)_theme->page.g + (u32)_theme->page_dots.g * 2u) / 3u),
        (u8)(((u32)_theme->page.b + (u32)_theme->page_dots.b * 2u) / 3u),
        _theme->page_dots.a
    };

    const f32 _ex0 = (rde_math_floor_f32(_min.x / _s - 0.5f) + 0.5f) * _s;
    const f32 _ey0 = (rde_math_floor_f32(_min.y / _s - 0.5f) + 0.5f) * _s;
    for(f32 _x = _ex0; _x <= _max.x; _x += _s) {
        fude_draw_line(fude_canvas_to_screen(_canvas, (rde_vec_2F){ _x, _min.y }), fude_canvas_to_screen(_canvas, (rde_vec_2F){ _x, _max.y }), 0.6f, _edge);
    }
    for(f32 _y = _ey0; _y <= _max.y; _y += _s) {
        fude_draw_line(fude_canvas_to_screen(_canvas, (rde_vec_2F){ _min.x, _y }), fude_canvas_to_screen(_canvas, (rde_vec_2F){ _max.x, _y }), 0.6f, _edge);
    }

    if(_screen < FUDE_CANVAS_SQUARE_CROSS) {
        return;   // Too small to write in anyway, so a cross would only be noise.
    }

    // The crosses: whole lines through the squares' centres, dashed, each dash 1 pixel wide. Each dash's centre goes
    // through fude_draw_at, as the edges' lines do (fude_draw_line), so in a mirrored or kept box (draw.h) the two stay
    // together.
    const f32 _dash   = rde_math_max_f32(4.0f, _screen / 40.0f) / _canvas->view.zoom;   // In canvas units.
    const f32 _period = _dash * 2.0f;
    const f32 _zoom   = _canvas->view.zoom;
    for(f32 _x = rde_math_ceil_f32(_min.x / _s) * _s; _x <= _max.x; _x += _s) {
        for(f32 _y = rde_math_floor_f32(_min.y / _period) * _period; _y <= _max.y; _y += _period) {
            const rde_vec_2F _c = fude_canvas_to_screen(_canvas, (rde_vec_2F){ _x, _y + _dash * 0.5f });
            rde_rendering_2d_draw_rectangle(fude_draw_at(_c), (rde_vec_2F){ 1.0f, _dash * _zoom }, _guide);
        }
    }
    for(f32 _y = rde_math_ceil_f32(_min.y / _s) * _s; _y <= _max.y; _y += _s) {
        for(f32 _x = rde_math_floor_f32(_min.x / _period) * _period; _x <= _max.x; _x += _period) {
            const rde_vec_2F _c = fude_canvas_to_screen(_canvas, (rde_vec_2F){ _x + _dash * 0.5f, _y });
            rde_rendering_2d_draw_rectangle(fude_draw_at(_c), (rde_vec_2F){ _dash * _zoom, 1.0f }, _guide);
        }
    }
}

// Draws ruled lines across the canvas rectangle _min.._max, where the squares' edges would be.
RDE_INTERNAL void fude_canvas_draw_lines(const fude_canvas* _canvas, rde_vec_2F _min, rde_vec_2F _max) {
    const f32 _s = fude_canvas_paper_units(_canvas->paper_size);
    for(f32 _y = (rde_math_floor_f32(_min.y / _s - 0.5f) + 0.5f) * _s; _y <= _max.y; _y += _s) {
        fude_draw_line(fude_canvas_to_screen(_canvas, (rde_vec_2F){ _min.x, _y }), fude_canvas_to_screen(_canvas, (rde_vec_2F){ _max.x, _y }), 0.6f,
                       fude_theme_active()->page_dots);
    }
}

// Draws the page's paper over what the screen shows: nothing, squares, lines, or else the dot grid (which also stands
// in for an app's own papers, drawn by the app).
void fude_canvas_draw_grid(const fude_canvas* _canvas, rde_vec_2I _window_size) {
    const fude_view* _view = &_canvas->view;

    // The dots' spacing snaps by powers of two, so the dots never crowd into a grey wash when zoomed out, nor thin out
    // to nothing when zoomed in.
    f32 _spacing = FUDE_CANVAS_GRID_SPACING;
    while(_spacing * _view->zoom < FUDE_CANVAS_GRID_MIN_SCREEN) { _spacing *= 2.0f; }
    while(_spacing * _view->zoom > FUDE_CANVAS_GRID_MAX_SCREEN) { _spacing *= 0.5f; }

    // The canvas rectangle the screen shows now.
    const f32        _hw  = (f32)_window_size.x * 0.5f;
    const f32        _hh  = (f32)_window_size.y * 0.5f;
    const rde_vec_2F _min = fude_canvas_from_screen(_canvas, (rde_vec_2F){ -_hw, -_hh });
    const rde_vec_2F _max = fude_canvas_from_screen(_canvas, (rde_vec_2F){  _hw,  _hh });

    if(_canvas->page.paper == FUDE_PAPER_NONE) {
        return;
    }
    if(_canvas->page.paper == FUDE_PAPER_SQUARES) {
        fude_canvas_draw_squares(_canvas, _min, _max);
        return;
    }
    if(_canvas->page.paper == FUDE_PAPER_LINES) {
        fude_canvas_draw_lines(_canvas, _min, _max);
        return;
    }

    const f32 _x0 = rde_math_floor_f32(_min.x / _spacing) * _spacing;
    const f32 _y0 = rde_math_floor_f32(_min.y / _spacing) * _spacing;

    for(f32 _y = _y0; _y <= _max.y; _y += _spacing) {
        for(f32 _x = _x0; _x <= _max.x; _x += _spacing) {
            const rde_vec_2F _screen = fude_canvas_to_screen(_canvas, (rde_vec_2F){ _x, _y });
            rde_rendering_2d_draw_rectangle(fude_draw_at(_screen), (rde_vec_2F){ FUDE_CANVAS_GRID_DOT, FUDE_CANVAS_GRID_DOT }, fude_theme_active()->page_dots);
        }
    }
}

// --- a document's turned pages -----------------------------------------------------------------

// Returns how many pages are turned: the list's length, 0 before it is made.
u32 fude_page_turn_count(const fude_page* _p) {
    return rde_arr_is_inited(&_p->turns) ? (u32)rde_arr_length(&_p->turns) : 0u;
}

// Returns page _page's quarter turns (0 to 3), or 0 when it is not in the list.
u8 fude_page_turned(const fude_page* _p, u32 _page) {
    const fude_page_turn* _turns = (const fude_page_turn*)_p->turns.memory;
    for(u32 _i = 0; _i < fude_page_turn_count(_p); _i++) {
        if(_turns[_i].page == _page) {
            return _turns[_i].quarters & 3u;
        }
    }
    return 0u;
}

// Adds a quarter turn to page _page; a fourth takes it out of the list (the last entry moves into its place). A page
// not yet turned is added with one quarter when its number fits in 16 bits (FUDE_PAGE_TURN_PAGES), the list made the
// first time.
b8 fude_page_turn_more(fude_page* _p, u32 _page) {
    fude_page_turn* _turns = (fude_page_turn*)_p->turns.memory;
    const u32       _count = fude_page_turn_count(_p);
    for(u32 _i = 0; _i < _count; _i++) {
        if(_turns[_i].page == _page) {
            _turns[_i].quarters = (u8)((_turns[_i].quarters + 1u) & 3u);
            if(_turns[_i].quarters == 0u) {   // A full turn: upright again, so out of the list.
                _turns[_i] = _turns[_count - 1u];
                rde_arr_resize(&_p->turns, _count - 1u);
            }
            _p->revision++;
            return true;
        }
    }
    if(_page >= FUDE_PAGE_TURN_PAGES) {
        return false;
    }
    if(!rde_arr_is_inited(&_p->turns)) {
        _p->turns = rde_arr_new(sizeof(fude_page_turn), rde_memory_allocator_get_default_std());
    }
    const fude_page_turn _turn = { (u16)_page, 1u };
    rde_arr_add(&_p->turns, (any)&_turn);
    _p->revision++;
    return true;
}

// Frees the list of turns and sets the dots; the revision goes on, so whoever follows it sees the change.
void fude_page_reset(fude_page* _p) {
    if(rde_arr_is_inited(&_p->turns)) {
        rde_arr_free(&_p->turns);
    }
    const u32 _revision = _p->revision;
    rde_memset(_p, 0, sizeof(*_p));   // The dots (FUDE_PAPER_DOTS is 0), no list.
    _p->revision = _revision + 1u;
}
