#include "lasso.h"
#include "draw.h"
#include "theme.h"

#include <string.h>

// ===========================================================================
// See lasso.h.
// ===========================================================================

#define KANA_LASSO_LINE_PX     1.2f
#define KANA_LASSO_GLOW_PX     4.0f

void kana_lasso_init(kana_lasso* _lasso) {
    memset(_lasso, 0, sizeof(*_lasso));

    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _lasso->clipboard.strokes  = rde_arr_new(sizeof(kana_ink_stroke), _heap);
    _lasso->clipboard.points   = rde_arr_new(sizeof(kana_ink_point),  _heap);
    _lasso->_duplicate.strokes = rde_arr_new(sizeof(kana_ink_stroke), _heap);
    _lasso->_duplicate.points  = rde_arr_new(sizeof(kana_ink_point),  _heap);
    _lasso->loop               = rde_arr_new(sizeof(rde_vec_2F), _heap);
    _lasso->selected           = rde_arr_new(sizeof(u32),        _heap);
    _lasso->_scratch_positions = rde_arr_new(sizeof(rde_vec_2F), _heap);
}

void kana_lasso_destroy(kana_lasso* _lasso) {
    rde_arr* _arrays[] = { &_lasso->loop, &_lasso->selected, &_lasso->_scratch_positions,
                           &_lasso->clipboard.strokes, &_lasso->clipboard.points, &_lasso->_duplicate.strokes, &_lasso->_duplicate.points };

    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }

    memset(_lasso, 0, sizeof(*_lasso));
}

RDE_INTERNAL u32*        kana_lasso_ids(const kana_lasso* _lasso)    { return (u32*)_lasso->selected.memory; }
RDE_INTERNAL rde_vec_2F* kana_lasso_points(const kana_lasso* _lasso) { return (rde_vec_2F*)_lasso->loop.memory; }

b8 kana_lasso_busy(const kana_lasso* _lasso) {
    return _lasso->state != KANA_LASSO_IDLE;
}

u32 kana_lasso_count(const kana_lasso* _lasso) {
    return (u32)rde_arr_length(&_lasso->selected);
}

void kana_lasso_clear(kana_lasso* _lasso, kana_ink* _ink) {
    if(_lasso->state == KANA_LASSO_MOVING) {
        kana_ink_move_end(_ink);
    }

    _lasso->state = KANA_LASSO_IDLE;
    rde_arr_clear(&_lasso->loop);
    rde_arr_clear(&_lasso->selected);
}

RDE_INTERNAL b8 kana_lasso_selectable(const kana_ink* _ink, u32 _id) {
    return _id < kana_ink_stroke_count(_ink) && kana_ink_stroke_at(_ink, _id)->alive;
}

void kana_lasso_sync(kana_lasso* _lasso, const kana_ink* _ink) {
    u32*      _ids   = kana_lasso_ids(_lasso);
    const u32 _count = kana_lasso_count(_lasso);
    u32       _kept  = 0;

    for(u32 _i = 0; _i < _count; _i++) {
        if(kana_lasso_selectable(_ink, _ids[_i])) {
            _ids[_kept++] = _ids[_i];
        }
    }

    // rde_arr has clear but no truncate; this is rde_arr_clear's own operation.
    _lasso->selected.count = _kept;
}

b8 kana_lasso_bounds(const kana_lasso* _lasso, const kana_ink* _ink, rde_vec_2F* _min, rde_vec_2F* _max) {
    const u32* _ids   = kana_lasso_ids(_lasso);
    b8         _found = false;

    for(u32 _i = 0; _i < kana_lasso_count(_lasso); _i++) {
        if(!kana_lasso_selectable(_ink, _ids[_i])) {
            continue;
        }

        const kana_ink_stroke* _s = kana_ink_stroke_at(_ink, _ids[_i]);
        if(!_found) {
            *_min  = _s->bounds_min;
            *_max  = _s->bounds_max;
            _found = true;
        } else {
            *_min = (rde_vec_2F){ _s->bounds_min.x < _min->x ? _s->bounds_min.x : _min->x, _s->bounds_min.y < _min->y ? _s->bounds_min.y : _min->y };
            *_max = (rde_vec_2F){ _s->bounds_max.x > _max->x ? _s->bounds_max.x : _max->x, _s->bounds_max.y > _max->y ? _s->bounds_max.y : _max->y };
        }
    }

    return _found;
}

void kana_lasso_delete(kana_lasso* _lasso, kana_ink* _ink) {
    kana_lasso_sync(_lasso, _ink);
    kana_ink_erase_strokes(_ink, kana_lasso_ids(_lasso), kana_lasso_count(_lasso));
    kana_lasso_clear(_lasso, _ink);
}

// --- clipboard -------------------------------------------------------------------

// The selection into _clip, positions relative to its centre. Selections are
// always built in stroke order, so a paste keeps the order they were written in.
RDE_INTERNAL b8 kana_lasso_copy_into(kana_lasso* _lasso, const kana_ink* _ink, kana_clip* _clip) {
    kana_lasso_sync(_lasso, _ink);

    rde_vec_2F _min;
    rde_vec_2F _max;
    if(!kana_lasso_bounds(_lasso, _ink, &_min, &_max)) {
        return false;
    }

    rde_arr_clear(&_clip->strokes);
    rde_arr_clear(&_clip->points);
    const rde_vec_2F _center = { (_min.x + _max.x) * 0.5f, (_min.y + _max.y) * 0.5f };
    const u32*       _ids    = kana_lasso_ids(_lasso);

    for(u32 _i = 0; _i < kana_lasso_count(_lasso); _i++) {
        kana_ink_stroke _stroke = *kana_ink_stroke_at(_ink, _ids[_i]);
        const kana_ink_point* _src = kana_ink_stroke_points(_ink, &_stroke);

        _stroke.first_point = (u32)rde_arr_length(&_clip->points);
        kana_ink_point* _dst = rde_arr_add_n(&_clip->points, _stroke.point_count);
        memcpy(_dst, _src, (usize)_stroke.point_count * sizeof(kana_ink_point));
        for(u32 _p = 0; _p < _stroke.point_count; _p++) {
            _dst[_p].position.x -= _center.x;
            _dst[_p].position.y -= _center.y;
        }

        rde_arr_add(&_clip->strokes, &_stroke);
    }

    return true;
}

// Adds _clip centred at _canvas (one undo step) and selects what it added.
RDE_INTERNAL void kana_lasso_paste_from(kana_lasso* _lasso, kana_ink* _ink, const kana_clip* _clip, rde_vec_2F _canvas) {
    const u32 _count = (u32)rde_arr_length(&_clip->strokes);
    const u32 _first = kana_ink_add_strokes(_ink, (const kana_ink_stroke*)_clip->strokes.memory, _count,
                                            (const kana_ink_point*)_clip->points.memory, _canvas);

    kana_lasso_clear(_lasso, _ink);
    if(_first == UINT32_MAX) {
        return;
    }

    for(u32 _id = _first; _id < kana_ink_stroke_count(_ink); _id++) {
        rde_arr_add(&_lasso->selected, &_id);
    }
}

void kana_lasso_copy(kana_lasso* _lasso, const kana_ink* _ink) {
    kana_lasso_copy_into(_lasso, _ink, &_lasso->clipboard);
}

void kana_lasso_cut(kana_lasso* _lasso, kana_ink* _ink) {
    if(kana_lasso_copy_into(_lasso, _ink, &_lasso->clipboard)) {
        kana_lasso_delete(_lasso, _ink);
    }
}

void kana_lasso_duplicate(kana_lasso* _lasso, kana_ink* _ink, f32 _zoom) {
    rde_vec_2F _min;
    rde_vec_2F _max;
    kana_lasso_sync(_lasso, _ink);
    if(!kana_lasso_bounds(_lasso, _ink, &_min, &_max) || !kana_lasso_copy_into(_lasso, _ink, &_lasso->_duplicate)) {
        return;
    }

    // Down and to the right on SCREEN (canvas Y is up), the same distance at any zoom.
    const f32 _step = KANA_LASSO_DUPLICATE / _zoom;
    kana_lasso_paste_from(_lasso, _ink, &_lasso->_duplicate, (rde_vec_2F){ (_min.x + _max.x) * 0.5f + _step, (_min.y + _max.y) * 0.5f - _step });
}

void kana_lasso_paste(kana_lasso* _lasso, kana_ink* _ink, rde_vec_2F _canvas) {
    kana_lasso_paste_from(_lasso, _ink, &_lasso->clipboard, _canvas);
}

void kana_lasso_paste_clip(kana_lasso* _lasso, kana_ink* _ink, const kana_clip* _clip, rde_vec_2F _canvas) {
    kana_lasso_paste_from(_lasso, _ink, _clip, _canvas);
}

b8 kana_lasso_can_paste(const kana_lasso* _lasso) {
    return rde_arr_length(&_lasso->clipboard.strokes) > 0;
}

void kana_lasso_select_all(kana_lasso* _lasso, kana_ink* _ink) {
    kana_lasso_clear(_lasso, _ink);

    for(u32 _id = 0; _id < kana_ink_stroke_count(_ink); _id++) {
        if(kana_ink_stroke_at(_ink, _id)->alive) {
            rde_arr_add(&_lasso->selected, &_id);
        }
    }
}

// --- selecting -----------------------------------------------------------------

// Even-odd rule: a loop that crosses itself still selects sensibly.
RDE_INTERNAL b8 kana_lasso_inside(const rde_vec_2F* _poly, u32 _n, rde_vec_2F _p) {
    b8 _in = false;

    for(u32 _i = 0, _j = _n - 1; _i < _n; _j = _i++) {
        const rde_vec_2F _a = _poly[_i];
        const rde_vec_2F _b = _poly[_j];

        if((_a.y > _p.y) != (_b.y > _p.y) && _p.x < (_b.x - _a.x) * (_p.y - _a.y) / (_b.y - _a.y) + _a.x) {
            _in = !_in;
        }
    }

    return _in;
}

RDE_INTERNAL void kana_lasso_select_loop(kana_lasso* _lasso, const kana_ink* _ink, f32 _zoom) {
    rde_arr_clear(&_lasso->selected);

    const rde_vec_2F* _poly = kana_lasso_points(_lasso);
    const u32         _n    = (u32)rde_arr_length(&_lasso->loop);
    if(_n == 0) {
        return;
    }

    rde_vec_2F _min = _poly[0];
    rde_vec_2F _max = _poly[0];
    for(u32 _i = 1; _i < _n; _i++) {
        _min = (rde_vec_2F){ _poly[_i].x < _min.x ? _poly[_i].x : _min.x, _poly[_i].y < _min.y ? _poly[_i].y : _min.y };
        _max = (rde_vec_2F){ _poly[_i].x > _max.x ? _poly[_i].x : _max.x, _poly[_i].y > _max.y ? _poly[_i].y : _max.y };
    }

    // Too small to be a loop: a tap, which takes the stroke under the pen.
    if(_n < 3 || ((_max.x - _min.x) * _zoom < KANA_LASSO_TAP_SIZE && (_max.y - _min.y) * _zoom < KANA_LASSO_TAP_SIZE)) {
        u32 _hit = kana_ink_pick(_ink, _poly[0], KANA_LASSO_PICK_RADIUS / _zoom);
        if(_hit != UINT32_MAX) {
            rde_arr_add(&_lasso->selected, &_hit);
        }
        return;
    }

    for(u32 _s = 0; _s < kana_ink_stroke_count(_ink); _s++) {
        const kana_ink_stroke* _stroke = kana_ink_stroke_at(_ink, _s);

        if(!_stroke->alive || _stroke->point_count == 0 ||
           _stroke->bounds_max.x < _min.x || _stroke->bounds_min.x > _max.x ||
           _stroke->bounds_max.y < _min.y || _stroke->bounds_min.y > _max.y) {
            continue;
        }

        const kana_ink_point* _points = kana_ink_stroke_points(_ink, _stroke);
        u32                   _inside = 0;

        for(u32 _p = 0; _p < _stroke->point_count; _p++) {
            const rde_vec_2F _q = _points[_p].position;
            // The loop's box first: most points of a stroke that only brushes the
            // loop are rejected without walking the polygon.
            if(_q.x >= _min.x && _q.x <= _max.x && _q.y >= _min.y && _q.y <= _max.y && kana_lasso_inside(_poly, _n, _q)) {
                _inside++;
            }
        }

        if((f32)_inside >= KANA_LASSO_INSIDE * (f32)_stroke->point_count) {
            u32 _id = _s;
            rde_arr_add(&_lasso->selected, &_id);
        }
    }
}

// --- the pen ---------------------------------------------------------------------

void kana_lasso_pen_down(kana_lasso* _lasso, kana_ink* _ink, rde_vec_2F _canvas, f32 _zoom) {
    kana_lasso_sync(_lasso, _ink);

    rde_vec_2F _min;
    rde_vec_2F _max;
    const f32  _pad = (KANA_LASSO_BOX_PAD + KANA_LASSO_GRAB_PAD) / _zoom;

    if(kana_lasso_bounds(_lasso, _ink, &_min, &_max) &&
       _canvas.x >= _min.x - _pad && _canvas.x <= _max.x + _pad &&
       _canvas.y >= _min.y - _pad && _canvas.y <= _max.y + _pad) {
        _lasso->state = KANA_LASSO_MOVING;
        _lasso->grab  = _canvas;
        kana_ink_move_begin(_ink, kana_lasso_ids(_lasso), kana_lasso_count(_lasso));
        return;
    }

    // Anywhere else: a new loop, and the old selection goes.
    rde_arr_clear(&_lasso->selected);
    rde_arr_clear(&_lasso->loop);
    rde_arr_add(&_lasso->loop, &_canvas);
    _lasso->state = KANA_LASSO_LOOPING;
}

void kana_lasso_pen_moved(kana_lasso* _lasso, kana_ink* _ink, rde_vec_2F _canvas, f32 _zoom) {
    if(_lasso->state == KANA_LASSO_MOVING) {
        kana_ink_move_by(_ink, (rde_vec_2F){ _canvas.x - _lasso->grab.x, _canvas.y - _lasso->grab.y });
        _lasso->grab = _canvas;
        return;
    }

    if(_lasso->state == KANA_LASSO_LOOPING) {
        const rde_vec_2F _last = kana_lasso_points(_lasso)[rde_arr_length(&_lasso->loop) - 1];
        const f32        _dx   = (_canvas.x - _last.x) * _zoom;
        const f32        _dy   = (_canvas.y - _last.y) * _zoom;

        if(_dx * _dx + _dy * _dy >= KANA_LASSO_STEP * KANA_LASSO_STEP) {
            rde_arr_add(&_lasso->loop, &_canvas);
        }
    }
}

void kana_lasso_pen_up(kana_lasso* _lasso, kana_ink* _ink, f32 _zoom) {
    if(_lasso->state == KANA_LASSO_MOVING) {
        kana_ink_move_end(_ink);
    } else if(_lasso->state == KANA_LASSO_LOOPING) {
        kana_lasso_select_loop(_lasso, _ink, _zoom);
        rde_arr_clear(&_lasso->loop);
    }

    _lasso->state = KANA_LASSO_IDLE;
}

// --- drawing ---------------------------------------------------------------------

void kana_lasso_render_under(kana_lasso* _lasso, kana_ink* _ink, rde_vec_2F _offset, f32 _zoom) {
    kana_lasso_sync(_lasso, _ink);

    const u32* _ids = kana_lasso_ids(_lasso);
    for(u32 _i = 0; _i < kana_lasso_count(_lasso); _i++) {
        kana_ink_draw_stroke(_ink, _ids[_i], _offset, _zoom, KANA_LASSO_GLOW_PX, kana_theme_active()->select_glow);
    }
}

// A screen-space polyline through the scratch, as one antialiased stroke.
RDE_INTERNAL void kana_lasso_line(kana_lasso* _lasso, const rde_vec_2F* _points, u32 _count, rde_color _color) {
    RDE_UNUSED(_lasso);
    kana_draw_stroke_even(_points, _count, KANA_LASSO_LINE_PX * 0.5f, _color);
}

void kana_lasso_render_over(kana_lasso* _lasso, const kana_ink* _ink, rde_vec_2F _offset, f32 _zoom) {
    if(_lasso->state == KANA_LASSO_LOOPING) {
        const u32 _n = (u32)rde_arr_length(&_lasso->loop);
        if(_n < 2) {
            return;
        }

        const rde_vec_2F* _loop = kana_lasso_points(_lasso);
        rde_arr_clear(&_lasso->_scratch_positions);
        rde_vec_2F* _screen = rde_arr_add_n(&_lasso->_scratch_positions, _n);
        for(u32 _i = 0; _i < _n; _i++) {
            _screen[_i] = (rde_vec_2F){ _loop[_i].x * _zoom + _offset.x, _loop[_i].y * _zoom + _offset.y };
        }

        kana_lasso_line(_lasso, _screen, _n, kana_theme_active()->select);

        // Where the loop will close, fainter.
        const rde_vec_2F _close[2] = { _screen[_n - 1], _screen[0] };
        rde_color        _faint    = kana_theme_active()->select;
        _faint.a                   = 110;
        kana_lasso_line(_lasso, _close, 2, _faint);
        return;
    }

    rde_vec_2F _min;
    rde_vec_2F _max;
    if(!kana_lasso_bounds(_lasso, _ink, &_min, &_max)) {
        return;
    }

    const f32        _pad = KANA_LASSO_BOX_PAD;
    const rde_vec_2F _lo  = { _min.x * _zoom + _offset.x - _pad, _min.y * _zoom + _offset.y - _pad };
    const rde_vec_2F _hi  = { _max.x * _zoom + _offset.x + _pad, _max.y * _zoom + _offset.y + _pad };

    rde_rendering_2d_draw_rectangle((rde_vec_2F){ (_lo.x + _hi.x) * 0.5f, (_lo.y + _hi.y) * 0.5f },
                                    (rde_vec_2F){ _hi.x - _lo.x, _hi.y - _lo.y }, kana_theme_active()->select_fill);

    const rde_vec_2F _box[5] = { _lo, { _hi.x, _lo.y }, _hi, { _lo.x, _hi.y }, _lo };
    kana_lasso_line(_lasso, _box, 5, kana_theme_active()->select);
}
