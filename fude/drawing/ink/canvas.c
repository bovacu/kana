#include "drawing/ink/canvas.h"
#include "drawing/widgets/draw.h"
#include "drawing/base/theme.h"

#include <math.h>
#include <string.h>

// ===========================================================================
// See canvas.h.
// ===========================================================================

RDE_INTERNAL u32 fude_canvas_active_fingers(const fude_canvas* _canvas);

f32 fude_canvas_paper_units(FUDE_PAPER_SIZE_ _size) {
    return _size == FUDE_PAPER_SMALL ? 110.0f : _size == FUDE_PAPER_LARGE ? 240.0f : 160.0f;
}

void fude_canvas_init(fude_canvas* _canvas) {
    memset(_canvas, 0, sizeof(*_canvas));
    _canvas->paper_size = FUDE_PAPER_MEDIUM;
    fude_canvas_reset_view(_canvas);
}

// The bounds of a reader's view: where its pages may be (screen units: the
// first page's top, the last's bottom, the pages' edges).
RDE_INTERNAL void fude_canvas_reader_clamp(fude_canvas* _canvas) {
    const fude_canvas_reader* _r = &_canvas->reader;
    fude_view*                _v = &_canvas->view;
    if(!_r->on || _r->fit <= 0.0f) {
        return;
    }
    _v->zoom = rde_math_clamp_f32(_v->zoom, _r->fit, fmaxf(_r->fit, FUDE_CANVAS_ZOOM_MAX));
    // Across: centred at 100%; zoomed in, the pages' edges no further in than the gutters.
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
    // Down: the first page's top no lower than under the menu; the last page's bottom no higher than its room.
    const f32 _lowest  = (f32)_r->window.y * 0.5f - (f32)_r->safe.y - FUDE_CANVAS_READER_TOP;
    const f32 _highest = -(f32)_r->window.y * 0.5f + (f32)_r->safe.w + FUDE_CANVAS_READER_BOTTOM - _r->bottom * _v->zoom;
    const f32 _y       = _highest < _lowest ? _lowest : rde_math_clamp_f32(_v->offset.y, _lowest, _highest);
    _canvas->velocity.y = _y != _v->offset.y ? 0.0f : _canvas->velocity.y;
    _v->offset.y        = _y;
}

void fude_canvas_reset_view(fude_canvas* _canvas) {
    _canvas->view.offset = (rde_vec_2F){ 0.0f, 0.0f };
    _canvas->view.zoom   = 1.0f;
    _canvas->coasting    = false;
    _canvas->velocity    = (rde_vec_2F){ 0.0f, 0.0f };
    if(_canvas->reader.on && _canvas->reader.fit > 0.0f) {
        // Reading: the first page's top, at 100%.
        _canvas->view.zoom     = _canvas->reader.fit;
        _canvas->view.offset.x = (f32)(_canvas->reader.safe.x - _canvas->reader.safe.z) * 0.5f;
        _canvas->view.offset.y = (f32)_canvas->reader.window.y * 0.5f - (f32)_canvas->reader.safe.y - FUDE_CANVAS_READER_TOP;
    }
}

f32 fude_canvas_reader_fit(f32 _width, rde_vec_2I _window, rde_vec_4I _safe) {
    const f32 _room = (f32)(_window.x - _safe.x - _safe.z) - 2.0f * FUDE_CANVAS_READER_GUTTER;
    return rde_math_clamp_f32(_room / fmaxf(_width, 1.0f), FUDE_CANVAS_ZOOM_MIN, FUDE_CANVAS_ZOOM_MAX);
}

void fude_canvas_set_reader(fude_canvas* _canvas, b8 _on, f32 _width, f32 _bottom) {
    if(!_on) {
        _canvas->reader.on = false;
        return;
    }
    if(!_canvas->reader.on) {
        _canvas->reader.window = (rde_vec_2I){ 0, 0 };   // fitted on the next update
    }
    _canvas->reader.on     = true;
    _canvas->reader.width  = _width;
    _canvas->reader.bottom = _bottom;
}

void fude_canvas_update(fude_canvas* _canvas, rde_vec_2I _window, rde_vec_4I _safe) {
    const f64 _now = rde_engine_get_time_now();
    const f32 _dt  = _canvas->ticked_at > 0.0 ? (f32)fmin(0.1, _now - _canvas->ticked_at) : 0.0f;
    _canvas->ticked_at = _now;
    fude_canvas_reader* _r = &_canvas->reader;
    fude_view*          _v = &_canvas->view;
    if(_r->on && (_window.x != _r->window.x || _window.y != _r->window.y || memcmp(&_safe, &_r->safe, sizeof(_safe)) != 0)) {
        // Fitted (again): the same zoom against 100%, the same point at the top middle.
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
    }
    // A flick, carrying on.
    if(_canvas->coasting && fude_canvas_active_fingers(_canvas) == 0) {
        _v->offset.x += _canvas->velocity.x * _dt;
        _v->offset.y += _canvas->velocity.y * _dt;
        const f32 _keep = expf(-FUDE_CANVAS_FRICTION * _dt);
        _canvas->velocity.x *= _keep;
        _canvas->velocity.y *= _keep;
        if(fabsf(_canvas->velocity.x) < FUDE_CANVAS_COAST_MIN && fabsf(_canvas->velocity.y) < FUDE_CANVAS_COAST_MIN) {
            _canvas->coasting = false;
        }
    }
    fude_canvas_reader_clamp(_canvas);
}

f32 fude_canvas_zoom_shown(const fude_canvas* _canvas) {
    return _canvas->reader.on && _canvas->reader.fit > 0.0f ? _canvas->view.zoom / _canvas->reader.fit : _canvas->view.zoom;
}

rde_vec_2F fude_canvas_from_screen(const fude_canvas* _canvas, rde_vec_2F _screen) {
    return (rde_vec_2F){
        (_screen.x - _canvas->view.offset.x) / _canvas->view.zoom,
        (_screen.y - _canvas->view.offset.y) / _canvas->view.zoom
    };
}

rde_vec_2F fude_canvas_to_screen(const fude_canvas* _canvas, rde_vec_2F _canvas_pos) {
    return (rde_vec_2F){
        _canvas_pos.x * _canvas->view.zoom + _canvas->view.offset.x,
        _canvas_pos.y * _canvas->view.zoom + _canvas->view.offset.y
    };
}

RDE_INTERNAL fude_canvas_finger* fude_canvas_find_finger(fude_canvas* _canvas, u64 _finger_id) {
    for(u32 _i = 0; _i < FUDE_CANVAS_MAX_FINGERS; _i++) {
        if(_canvas->fingers[_i].active && _canvas->fingers[_i].finger_id == _finger_id) {
            return &_canvas->fingers[_i];
        }
    }

    return NULL;
}

RDE_INTERNAL u32 fude_canvas_active_fingers(const fude_canvas* _canvas) {
    u32 _count = 0;

    for(u32 _i = 0; _i < FUDE_CANVAS_MAX_FINGERS; _i++) {
        _count += _canvas->fingers[_i].active ? 1u : 0u;
    }

    return _count;
}

// The fingers that move the page are the first two active slots. Returns the
// other one of that pair for _finger, NULL if it is alone; *_drives is false for
// a finger outside the pair (a third finger), which moves nothing.
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

void fude_canvas_finger_down(fude_canvas* _canvas, u64 _finger_id, rde_vec_2F _screen) {
    if(fude_canvas_find_finger(_canvas, _finger_id) != NULL) {
        return;
    }

    _canvas->coasting = false;   // a finger stops a flick
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
    rde_vec_2F      _moved  = { _screen.x - _old.x, _screen.y - _old.y };   // how far the page went (two fingers: their middle)
    if(_other == NULL) {
        // One finger: the page follows it.
        _view->offset.x += _screen.x - _old.x;
        _view->offset.y += _screen.y - _old.y;
    } else {
        // Two: the canvas point under the old midpoint ends up under the new one,
        // scaled by how much the fingers spread — pinch and pan in one move.
        const rde_vec_2F _old_mid = { (_old.x + _other->position.x) * 0.5f, (_old.y + _other->position.y) * 0.5f };
        const rde_vec_2F _new_mid = { (_screen.x + _other->position.x) * 0.5f, (_screen.y + _other->position.y) * 0.5f };
        _moved = (rde_vec_2F){ _new_mid.x - _old_mid.x, _new_mid.y - _old_mid.y };

        const f32 _old_dx   = _old.x - _other->position.x;
        const f32 _old_dy   = _old.y - _other->position.y;
        const f32 _new_dx   = _screen.x - _other->position.x;
        const f32 _new_dy   = _screen.y - _other->position.y;
        const f32 _old_dist = sqrtf(_old_dx * _old_dx + _old_dy * _old_dy);
        const f32 _new_dist = sqrtf(_new_dx * _new_dx + _new_dy * _new_dy);

        f32 _new_zoom = _view->zoom;
        // Fingers a hair apart give a ratio that is all noise: pan only.
        if(_old_dist > 4.0f && _new_dist > 4.0f) {
            _new_zoom = rde_math_clamp_f32(_view->zoom * (_new_dist / _old_dist), FUDE_CANVAS_ZOOM_MIN, FUDE_CANVAS_ZOOM_MAX);
        }

        const rde_vec_2F _anchor = fude_canvas_from_screen(_canvas, _old_mid);
        _view->zoom     = _new_zoom;
        _view->offset.x = _new_mid.x - _anchor.x * _new_zoom;
        _view->offset.y = _new_mid.y - _anchor.y * _new_zoom;
    }
    fude_canvas_reader_clamp(_canvas);

    // How fast it moves (one finger's pan, or two fingers' middle), for a flick
    // to carry on: what the reader's frame let through.
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

FUDE_CANVAS_TAP_ fude_canvas_finger_up(fude_canvas* _canvas, u64 _finger_id) {
    fude_canvas_finger* _finger = fude_canvas_find_finger(_canvas, _finger_id);

    if(_finger == NULL) {
        return FUDE_CANVAS_TAP_NONE;
    }

    _finger->active = false;

    // Reading, the last finger lifting from a moving page: it carries on.
    if(fude_canvas_active_fingers(_canvas) == 0 && _canvas->reader.on && _canvas->tap_spoiled && !_canvas->long_pressed &&
       rde_engine_get_time_now() - _canvas->moved_at < 0.08 &&
       (fabsf(_canvas->velocity.x) > FUDE_CANVAS_COAST_MIN * 4.0f || fabsf(_canvas->velocity.y) > FUDE_CANVAS_COAST_MIN * 4.0f)) {
        _canvas->coasting = true;
    }

    // Not over until the LAST finger lifts: fingers of one tap never lift at
    // exactly the same moment.
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
        // Two resting fingers still wobble the pinch a little: put the page back.
        _canvas->view = _canvas->tap_view;
    }

    return _tap;
}

b8 fude_canvas_long_press(fude_canvas* _canvas, rde_vec_2F* _screen) {
    if(_canvas->long_pressed || _canvas->tap_spoiled || _canvas->tap_fingers != 1 || fude_canvas_active_fingers(_canvas) != 1 ||
       rde_engine_get_time_now() - _canvas->tap_start < FUDE_CANVAS_LONG_PRESS_TIME) {
        return false;
    }

    for(u32 _i = 0; _i < FUDE_CANVAS_MAX_FINGERS; _i++) {
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

void fude_canvas_release_fingers(fude_canvas* _canvas) {
    for(u32 _i = 0; _i < FUDE_CANVAS_MAX_FINGERS; _i++) {
        _canvas->fingers[_i].active = false;
    }

    // The pen came down: whatever the fingers were doing, it wasn't a tap.
    _canvas->tap_spoiled = true;
}

// Practice squares over what the screen shows: the squares' edges as lines, and
// through each the dashed centre cross (dashes set in the page, so they stay
// put while it pans). Square k spans ((k - 0.5)S, (k + 0.5)S): one is centred on
// the page's origin, where Reset puts the middle of the screen.
RDE_INTERNAL void fude_canvas_draw_squares(const fude_canvas* _canvas, rde_vec_2F _min, rde_vec_2F _max) {
    // The theme's canvas colours: the edges as its dots, the crosses between those
    // and the page.
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

    const f32 _ex0 = (floorf(_min.x / _s - 0.5f) + 0.5f) * _s;
    const f32 _ey0 = (floorf(_min.y / _s - 0.5f) + 0.5f) * _s;
    for(f32 _x = _ex0; _x <= _max.x; _x += _s) {
        fude_draw_line(fude_canvas_to_screen(_canvas, (rde_vec_2F){ _x, _min.y }), fude_canvas_to_screen(_canvas, (rde_vec_2F){ _x, _max.y }), 0.6f, _edge);
    }
    for(f32 _y = _ey0; _y <= _max.y; _y += _s) {
        fude_draw_line(fude_canvas_to_screen(_canvas, (rde_vec_2F){ _min.x, _y }), fude_canvas_to_screen(_canvas, (rde_vec_2F){ _max.x, _y }), 0.6f, _edge);
    }

    if(_screen < FUDE_CANVAS_SQUARE_CROSS) {
        return;   // too small to write in anyway: the cross would only be noise
    }

    // The crosses: whole lines through the squares' centres, dashed.
    const f32 _dash   = fmaxf(4.0f, _screen / 40.0f) / _canvas->view.zoom;   // canvas units
    const f32 _period = _dash * 2.0f;
    const f32 _zoom   = _canvas->view.zoom;
    for(f32 _x = ceilf(_min.x / _s) * _s; _x <= _max.x; _x += _s) {
        for(f32 _y = floorf(_min.y / _period) * _period; _y <= _max.y; _y += _period) {
            const rde_vec_2F _c = fude_canvas_to_screen(_canvas, (rde_vec_2F){ _x, _y + _dash * 0.5f });
            rde_rendering_2d_draw_rectangle(_c, (rde_vec_2F){ 1.0f, _dash * _zoom }, _guide);
        }
    }
    for(f32 _y = ceilf(_min.y / _s) * _s; _y <= _max.y; _y += _s) {
        for(f32 _x = floorf(_min.x / _period) * _period; _x <= _max.x; _x += _period) {
            const rde_vec_2F _c = fude_canvas_to_screen(_canvas, (rde_vec_2F){ _x + _dash * 0.5f, _y });
            rde_rendering_2d_draw_rectangle(_c, (rde_vec_2F){ _dash * _zoom, 1.0f }, _guide);
        }
    }
}

// Ruled lines across what the screen shows, where the squares' edges would be.
RDE_INTERNAL void fude_canvas_draw_lines(const fude_canvas* _canvas, rde_vec_2F _min, rde_vec_2F _max) {
    const f32 _s = fude_canvas_paper_units(_canvas->paper_size);
    for(f32 _y = (floorf(_min.y / _s - 0.5f) + 0.5f) * _s; _y <= _max.y; _y += _s) {
        fude_draw_line(fude_canvas_to_screen(_canvas, (rde_vec_2F){ _min.x, _y }), fude_canvas_to_screen(_canvas, (rde_vec_2F){ _max.x, _y }), 0.6f,
                       fude_theme_active()->page_dots);
    }
}

void fude_canvas_draw_grid(const fude_canvas* _canvas, rde_vec_2I _window_size) {
    const fude_view* _view = &_canvas->view;

    // Spacing snaps by powers of two so the dots never crowd into a grey wash
    // when zoomed out, nor thin out to nothing when zoomed in.
    f32 _spacing = FUDE_CANVAS_GRID_SPACING;
    while(_spacing * _view->zoom < FUDE_CANVAS_GRID_MIN_SCREEN) { _spacing *= 2.0f; }
    while(_spacing * _view->zoom > FUDE_CANVAS_GRID_MAX_SCREEN) { _spacing *= 0.5f; }

    // The canvas rect the screen currently shows.
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

    const f32 _x0 = floorf(_min.x / _spacing) * _spacing;
    const f32 _y0 = floorf(_min.y / _spacing) * _spacing;

    for(f32 _y = _y0; _y <= _max.y; _y += _spacing) {
        for(f32 _x = _x0; _x <= _max.x; _x += _spacing) {
            const rde_vec_2F _screen = fude_canvas_to_screen(_canvas, (rde_vec_2F){ _x, _y });
            rde_rendering_2d_draw_rectangle(_screen, (rde_vec_2F){ FUDE_CANVAS_GRID_DOT, FUDE_CANVAS_GRID_DOT }, fude_theme_active()->page_dots);
        }
    }
}

// --- a document's turned pages -----------------------------------------------------------------

u8 fude_page_turned(const fude_page* _p, u32 _page) {
    for(u32 _i = 0; _i < _p->turn_count && _i < FUDE_PAGE_TURNS; _i++) {
        if(_p->turns[_i].page == _page) {
            return _p->turns[_i].quarters & 3u;
        }
    }
    return 0u;
}

b8 fude_page_turn_more(fude_page* _p, u32 _page) {
    for(u32 _i = 0; _i < _p->turn_count; _i++) {
        if(_p->turns[_i].page == _page) {
            _p->turns[_i].quarters = (u8)((_p->turns[_i].quarters + 1u) & 3u);
            if(_p->turns[_i].quarters == 0u) {   // round again: upright, and out of the list
                _p->turns[_i] = _p->turns[--_p->turn_count];
            }
            return true;
        }
    }
    if(_p->turn_count >= FUDE_PAGE_TURNS || _page > 0xFFFFu) {
        return false;
    }
    _p->turns[_p->turn_count++] = (fude_page_turn){ (u16)_page, 1u };
    return true;
}

b8 fude_page_same(const fude_page* _a, const fude_page* _b) {
    if(_a->paper != _b->paper || _a->turn_count != _b->turn_count) {
        return false;
    }
    for(u32 _i = 0; _i < _a->turn_count; _i++) {
        if(fude_page_turned(_b, _a->turns[_i].page) != (_a->turns[_i].quarters & 3u)) {
            return false;
        }
    }
    return true;
}
