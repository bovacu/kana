// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/ink/lasso.h"
#include "drawing/base/draw.h"
#include "drawing/base/theme.h"

// ===========================================================================
// See lasso.h.
//
// The selection is an array of stroke indices, always in stroke order; it is checked against the ink (fude_lasso_sync)
// before it is read, because an undo or an erase can kill a selected stroke at any time.
// ===========================================================================

// The width of the loop's and the box's line, and how much wider than the ink the selection's glow is (both in
// screen units).
#define FUDE_LASSO_LINE_PX     1.2f
#define FUDE_LASSO_GLOW_PX     4.0f

// Zeroes the lasso and makes its arrays on the std allocator.
void fude_lasso_init(fude_lasso* _lasso) {
    rde_memset(_lasso, 0, sizeof(*_lasso));

    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _lasso->clipboard.strokes  = rde_arr_new(sizeof(fude_ink_stroke), _heap);
    _lasso->clipboard.points   = rde_arr_new(sizeof(fude_ink_point),  _heap);
    _lasso->_duplicate.strokes = rde_arr_new(sizeof(fude_ink_stroke), _heap);
    _lasso->_duplicate.points  = rde_arr_new(sizeof(fude_ink_point),  _heap);
    _lasso->loop               = rde_arr_new(sizeof(rde_vec_2F), _heap);
    _lasso->selected           = rde_arr_new(sizeof(u32),        _heap);
    _lasso->_scratch_positions = rde_arr_new(sizeof(rde_vec_2F), _heap);
}

// Frees whichever arrays were made, the clipboard included, and zeroes the lasso.
void fude_lasso_destroy(fude_lasso* _lasso) {
    rde_arr* _arrays[] = { &_lasso->loop, &_lasso->selected, &_lasso->_scratch_positions,
                           &_lasso->clipboard.strokes, &_lasso->clipboard.points, &_lasso->_duplicate.strokes, &_lasso->_duplicate.points };

    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }

    rde_memset(_lasso, 0, sizeof(*_lasso));
}

// Typed views of the selection and of the loop's points; take them again after adding to either array.
RDE_INTERNAL u32*        fude_lasso_ids(const fude_lasso* _lasso)    { return (u32*)_lasso->selected.memory; }
RDE_INTERNAL rde_vec_2F* fude_lasso_points(const fude_lasso* _lasso) { return (rde_vec_2F*)_lasso->loop.memory; }

// Returns whether a loop or a move is in progress.
b8 fude_lasso_busy(const fude_lasso* _lasso) {
    return _lasso->state != FUDE_LASSO_IDLE;
}

// Returns how many strokes are selected, as last synced.
u32 fude_lasso_count(const fude_lasso* _lasso) {
    return (u32)rde_arr_length(&_lasso->selected);
}

// Deselects everything: finishes a move as an edit, drops a loop, and forgets the loop's and the text area's boxes.
void fude_lasso_clear(fude_lasso* _lasso, fude_ink* _ink) {
    if(_lasso->state == FUDE_LASSO_MOVING) {
        fude_ink_move_end(_ink);
    }

    _lasso->state = FUDE_LASSO_IDLE;
    rde_arr_clear(&_lasso->loop);
    rde_arr_clear(&_lasso->selected);
    _lasso->looped  = false;
    _lasso->area_on = false;
}

// Returns whether _id is a stroke of the ink that is alive.
RDE_INTERNAL b8 fude_lasso_selectable(const fude_ink* _ink, u32 _id) {
    return _id < fude_ink_stroke_count(_ink) && fude_ink_stroke_at(_ink, _id)->alive;
}

// Drops the selected strokes that are no longer alive, keeping the others in order.
void fude_lasso_sync(fude_lasso* _lasso, const fude_ink* _ink) {
    u32*      _ids   = fude_lasso_ids(_lasso);
    const u32 _count = fude_lasso_count(_lasso);
    u32       _kept  = 0;

    for(u32 _i = 0; _i < _count; _i++) {
        if(fude_lasso_selectable(_ink, _ids[_i])) {
            _ids[_kept++] = _ids[_i];
        }
    }

    rde_arr_resize(&_lasso->selected, _kept);
}

// Writes the union of the selected alive strokes' bounds; false when none is selected.
b8 fude_lasso_bounds(const fude_lasso* _lasso, const fude_ink* _ink, rde_vec_2F* _min, rde_vec_2F* _max) {
    const u32* _ids   = fude_lasso_ids(_lasso);
    b8         _found = false;

    for(u32 _i = 0; _i < fude_lasso_count(_lasso); _i++) {
        if(!fude_lasso_selectable(_ink, _ids[_i])) {
            continue;
        }

        const fude_ink_stroke* _s = fude_ink_stroke_at(_ink, _ids[_i]);
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

// Writes the selected ink's box, else the selected text area's; false when neither is selected.
b8 fude_lasso_box(const fude_lasso* _lasso, const fude_ink* _ink, rde_vec_2F* _min, rde_vec_2F* _max) {
    return fude_lasso_bounds(_lasso, _ink, _min, _max) || fude_lasso_area(_lasso, _min, _max);
}

// Writes the last loop's box when the loop selected no strokes; false otherwise, or after a tap.
b8 fude_lasso_looped(const fude_lasso* _lasso, rde_vec_2F* _min, rde_vec_2F* _max) {
    if(!_lasso->looped || fude_lasso_count(_lasso) > 0) {
        return false;
    }
    *_min = _lasso->loop_min;
    *_max = _lasso->loop_max;
    return true;
}

// Selects an area of the page's text, dropping any selected strokes.
void fude_lasso_select_area(fude_lasso* _lasso, rde_vec_2F _min, rde_vec_2F _max) {
    rde_arr_clear(&_lasso->selected);
    _lasso->area_on  = true;
    _lasso->area_min = _min;
    _lasso->area_max = _max;
}

// Writes the selected text area's box; false when there is none, strokes are selected, or the lasso is busy.
b8 fude_lasso_area(const fude_lasso* _lasso, rde_vec_2F* _min, rde_vec_2F* _max) {
    if(!_lasso->area_on || fude_lasso_count(_lasso) > 0 || _lasso->state != FUDE_LASSO_IDLE) {
        return false;
    }
    *_min = _lasso->area_min;
    *_max = _lasso->area_max;
    return true;
}

// Erases the selected strokes as one edit and deselects.
void fude_lasso_delete(fude_lasso* _lasso, fude_ink* _ink) {
    fude_lasso_sync(_lasso, _ink);
    fude_ink_erase_strokes(_ink, fude_lasso_ids(_lasso), fude_lasso_count(_lasso));
    fude_lasso_clear(_lasso, _ink);
}

// --- clipboard -------------------------------------------------------------------

// Copies the selected strokes into _clip, with positions relative to the selection's centre. Selections are always
// built in stroke order, so a paste keeps the order the strokes were written in. Returns false, leaving _clip as it
// was, when nothing is selected.
RDE_INTERNAL b8 fude_lasso_copy_into(fude_lasso* _lasso, const fude_ink* _ink, fude_clip* _clip) {
    fude_lasso_sync(_lasso, _ink);

    rde_vec_2F _min;
    rde_vec_2F _max;
    if(!fude_lasso_bounds(_lasso, _ink, &_min, &_max)) {
        return false;
    }

    rde_arr_clear(&_clip->strokes);
    rde_arr_clear(&_clip->points);
    const rde_vec_2F _center = { (_min.x + _max.x) * 0.5f, (_min.y + _max.y) * 0.5f };
    const u32*       _ids    = fude_lasso_ids(_lasso);

    for(u32 _i = 0; _i < fude_lasso_count(_lasso); _i++) {
        fude_ink_stroke _stroke = *fude_ink_stroke_at(_ink, _ids[_i]);
        const fude_ink_point* _src = fude_ink_stroke_points(_ink, &_stroke);

        _stroke.first_point = (u32)rde_arr_length(&_clip->points);
        fude_ink_point* _dst = rde_arr_add_n(&_clip->points, _stroke.point_count);
        rde_memcpy(_dst, _src, (usize)_stroke.point_count * sizeof(fude_ink_point));
        for(u32 _p = 0; _p < _stroke.point_count; _p++) {
            _dst[_p].position.x -= _center.x;
            _dst[_p].position.y -= _center.y;
        }

        rde_arr_add(&_clip->strokes, &_stroke);
    }

    return true;
}

// Adds _clip's strokes centred at _canvas as one undo step, and selects what was added (nothing when nothing was).
RDE_INTERNAL void fude_lasso_paste_from(fude_lasso* _lasso, fude_ink* _ink, const fude_clip* _clip, rde_vec_2F _canvas) {
    const u32 _count = (u32)rde_arr_length(&_clip->strokes);
    const u32 _first = fude_ink_add_strokes(_ink, (const fude_ink_stroke*)_clip->strokes.memory, _count,
                                            (const fude_ink_point*)_clip->points.memory, _canvas);

    fude_lasso_clear(_lasso, _ink);
    if(_first == UINT32_MAX) {
        return;
    }

    for(u32 _id = _first; _id < fude_ink_stroke_count(_ink); _id++) {
        rde_arr_add(&_lasso->selected, &_id);
    }
}

// Copies the selection into the clipboard, keeping the selection.
void fude_lasso_copy(fude_lasso* _lasso, const fude_ink* _ink) {
    fude_lasso_copy_into(_lasso, _ink, &_lasso->clipboard);
}

// Copies the selection into the clipboard, then deletes it as one edit.
void fude_lasso_cut(fude_lasso* _lasso, fude_ink* _ink) {
    if(fude_lasso_copy_into(_lasso, _ink, &_lasso->clipboard)) {
        fude_lasso_delete(_lasso, _ink);
    }
}

// Adds a copy of the selection FUDE_LASSO_DUPLICATE screen units down and to the right and selects it, through a
// clip of its own so the clipboard is untouched.
void fude_lasso_duplicate(fude_lasso* _lasso, fude_ink* _ink, f32 _zoom) {
    rde_vec_2F _min;
    rde_vec_2F _max;
    fude_lasso_sync(_lasso, _ink);
    if(!fude_lasso_bounds(_lasso, _ink, &_min, &_max) || !fude_lasso_copy_into(_lasso, _ink, &_lasso->_duplicate)) {
        return;
    }

    // Down and to the right on the SCREEN (canvas Y is up), the same distance at any zoom.
    const f32 _step = FUDE_LASSO_DUPLICATE / _zoom;
    fude_lasso_paste_from(_lasso, _ink, &_lasso->_duplicate, (rde_vec_2F){ (_min.x + _max.x) * 0.5f + _step, (_min.y + _max.y) * 0.5f - _step });
}

// Pastes the clipboard centred at _canvas and selects what was added.
void fude_lasso_paste(fude_lasso* _lasso, fude_ink* _ink, rde_vec_2F _canvas) {
    fude_lasso_paste_from(_lasso, _ink, &_lasso->clipboard, _canvas);
}

// Pastes the caller's clip centred at _canvas and selects what was added; the clipboard is untouched.
void fude_lasso_paste_clip(fude_lasso* _lasso, fude_ink* _ink, const fude_clip* _clip, rde_vec_2F _canvas) {
    fude_lasso_paste_from(_lasso, _ink, _clip, _canvas);
}

// Returns whether the clipboard holds any stroke.
b8 fude_lasso_can_paste(const fude_lasso* _lasso) {
    return rde_arr_length(&_lasso->clipboard.strokes) > 0;
}

// Deselects, then selects every alive stroke in stroke order.
void fude_lasso_select_all(fude_lasso* _lasso, fude_ink* _ink) {
    fude_lasso_clear(_lasso, _ink);

    for(u32 _id = 0; _id < fude_ink_stroke_count(_ink); _id++) {
        if(fude_ink_stroke_at(_ink, _id)->alive) {
            rde_arr_add(&_lasso->selected, &_id);
        }
    }
}

// --- selecting -----------------------------------------------------------------

// Selects what the finished loop took: the stroke under the pen for a tap, else every alive stroke with at least
// FUDE_LASSO_INSIDE of its points inside the loop. The loop's box is kept for fude_lasso_looped.
RDE_INTERNAL void fude_lasso_select_loop(fude_lasso* _lasso, const fude_ink* _ink, f32 _zoom) {
    rde_arr_clear(&_lasso->selected);

    const rde_vec_2F* _poly = fude_lasso_points(_lasso);
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

    // Too small to be a loop (fewer than 3 points, or within FUDE_LASSO_TAP_SIZE on screen): a tap, which takes the
    // stroke under the pen.
    if(_n < 3 || ((_max.x - _min.x) * _zoom < FUDE_LASSO_TAP_SIZE && (_max.y - _min.y) * _zoom < FUDE_LASSO_TAP_SIZE)) {
        u32 _hit = fude_ink_pick(_ink, _poly[0], FUDE_LASSO_PICK_RADIUS / _zoom);
        if(_hit != UINT32_MAX) {
            rde_arr_add(&_lasso->selected, &_hit);
        }
        return;
    }

    // Keep the loop's box: when the loop takes no ink, the page may use it to select its own text (see the header).
    _lasso->looped   = true;
    _lasso->loop_min = _min;
    _lasso->loop_max = _max;

    for(u32 _s = 0; _s < fude_ink_stroke_count(_ink); _s++) {
        const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _s);

        if(!_stroke->alive || _stroke->point_count == 0 ||
           _stroke->bounds_max.x < _min.x || _stroke->bounds_min.x > _max.x ||
           _stroke->bounds_max.y < _min.y || _stroke->bounds_min.y > _max.y) {
            continue;
        }

        const fude_ink_point* _points = fude_ink_stroke_points(_ink, _stroke);
        u32                   _inside = 0;

        for(u32 _p = 0; _p < _stroke->point_count; _p++) {
            const rde_vec_2F _q = _points[_p].position;
            // Test the loop's box first, so most points of a stroke that only brushes the loop are rejected without
            // walking the polygon. Inside is by the even-odd rule (rde_math's), so a loop that crosses itself still
            // selects sensibly; the loop has 3 points at least here.
            if(_q.x >= _min.x && _q.x <= _max.x && _q.y >= _min.y && _q.y <= _max.y && rde_math_point_in_polygon_2d(_q, _poly, _n)) {
                _inside++;
            }
        }

        if((f32)_inside >= FUDE_LASSO_INSIDE * (f32)_stroke->point_count) {
            u32 _id = _s;
            rde_arr_add(&_lasso->selected, &_id);
        }
    }
}

// --- the pen ---------------------------------------------------------------------

// Starts a move when the press is inside the selection's drawn box (or within FUDE_LASSO_GRAB_PAD of it), else a new
// loop, which drops the selection and any text area.
void fude_lasso_pen_down(fude_lasso* _lasso, fude_ink* _ink, rde_vec_2F _canvas, f32 _zoom) {
    fude_lasso_sync(_lasso, _ink);

    rde_vec_2F _min;
    rde_vec_2F _max;
    const f32  _pad = (FUDE_LASSO_BOX_PAD + FUDE_LASSO_GRAB_PAD) / _zoom;

    if(fude_lasso_bounds(_lasso, _ink, &_min, &_max) &&
       _canvas.x >= _min.x - _pad && _canvas.x <= _max.x + _pad &&
       _canvas.y >= _min.y - _pad && _canvas.y <= _max.y + _pad) {
        _lasso->state = FUDE_LASSO_MOVING;
        _lasso->grab  = _canvas;
        fude_ink_move_begin(_ink, fude_lasso_ids(_lasso), fude_lasso_count(_lasso));
        return;
    }

    // Anywhere else: start a new loop, and drop the old selection (an area of text too).
    rde_arr_clear(&_lasso->selected);
    rde_arr_clear(&_lasso->loop);
    _lasso->looped  = false;
    _lasso->area_on = false;
    rde_arr_add(&_lasso->loop, &_canvas);
    _lasso->state = FUDE_LASSO_LOOPING;
}

// Drags the selection by how far the pen moved, or adds a point to the loop when it is at least FUDE_LASSO_STEP
// (on screen) from the last one.
void fude_lasso_pen_moved(fude_lasso* _lasso, fude_ink* _ink, rde_vec_2F _canvas, f32 _zoom) {
    if(_lasso->state == FUDE_LASSO_MOVING) {
        fude_ink_move_by(_ink, (rde_vec_2F){ _canvas.x - _lasso->grab.x, _canvas.y - _lasso->grab.y });
        _lasso->grab = _canvas;
        return;
    }

    if(_lasso->state == FUDE_LASSO_LOOPING) {
        const rde_vec_2F _last = fude_lasso_points(_lasso)[rde_arr_length(&_lasso->loop) - 1];
        const f32        _dx   = (_canvas.x - _last.x) * _zoom;
        const f32        _dy   = (_canvas.y - _last.y) * _zoom;

        if(_dx * _dx + _dy * _dy >= FUDE_LASSO_STEP * FUDE_LASSO_STEP) {
            rde_arr_add(&_lasso->loop, &_canvas);
        }
    }
}

// Ends a move as one edit, or selects what the loop took; the lasso goes idle.
void fude_lasso_pen_up(fude_lasso* _lasso, fude_ink* _ink, f32 _zoom) {
    if(_lasso->state == FUDE_LASSO_MOVING) {
        fude_ink_move_end(_ink);
    } else if(_lasso->state == FUDE_LASSO_LOOPING) {
        fude_lasso_select_loop(_lasso, _ink, _zoom);
        rde_arr_clear(&_lasso->loop);
    }

    _lasso->state = FUDE_LASSO_IDLE;
}

// --- drawing ---------------------------------------------------------------------

// Draws the glow under each selected stroke, after dropping dead strokes from the selection.
void fude_lasso_render_under(fude_lasso* _lasso, fude_ink* _ink, rde_vec_2F _offset, f32 _zoom) {
    fude_lasso_sync(_lasso, _ink);

    const u32* _ids = fude_lasso_ids(_lasso);
    for(u32 _i = 0; _i < fude_lasso_count(_lasso); _i++) {
        fude_ink_draw_stroke(_ink, _ids[_i], _offset, _zoom, FUDE_LASSO_GLOW_PX, fude_theme_active()->select_glow);
    }
}

// Draws a polyline of screen points as one antialiased stroke FUDE_LASSO_LINE_PX wide.
RDE_INTERNAL void fude_lasso_line(const rde_vec_2F* _points, u32 _count, rde_color _color) {
    fude_draw_stroke_even(_points, _count, FUDE_LASSO_LINE_PX * 0.5f, _color);
}

// Draws the loop being drawn (with a fainter line where it will close), or else the selection's box: a translucent
// fill and an outline, FUDE_LASSO_BOX_PAD outside the ink.
void fude_lasso_render_over(fude_lasso* _lasso, const fude_ink* _ink, rde_vec_2F _offset, f32 _zoom) {
    if(_lasso->state == FUDE_LASSO_LOOPING) {
        const u32 _n = (u32)rde_arr_length(&_lasso->loop);
        if(_n < 2) {
            return;
        }

        const rde_vec_2F* _loop = fude_lasso_points(_lasso);
        rde_arr_clear(&_lasso->_scratch_positions);
        rde_vec_2F* _screen = rde_arr_add_n(&_lasso->_scratch_positions, _n);
        for(u32 _i = 0; _i < _n; _i++) {
            _screen[_i] = (rde_vec_2F){ _loop[_i].x * _zoom + _offset.x, _loop[_i].y * _zoom + _offset.y };
        }

        fude_lasso_line(_screen, _n, fude_theme_active()->select);

        // Where the loop will close, drawn fainter (alpha 110).
        const rde_vec_2F _close[2] = { _screen[_n - 1], _screen[0] };
        rde_color        _faint    = fude_theme_active()->select;
        _faint.a                   = 110;
        fude_lasso_line(_close, 2, _faint);
        return;
    }

    rde_vec_2F _min;
    rde_vec_2F _max;
    if(!fude_lasso_box(_lasso, _ink, &_min, &_max)) {
        return;
    }

    const f32        _pad = FUDE_LASSO_BOX_PAD;
    const rde_vec_2F _lo  = { _min.x * _zoom + _offset.x - _pad, _min.y * _zoom + _offset.y - _pad };
    const rde_vec_2F _hi  = { _max.x * _zoom + _offset.x + _pad, _max.y * _zoom + _offset.y + _pad };

    // The fill's centre goes through fude_draw_at, as the outline's points do (fude_draw_stroke_even), so the two stay
    // together in a mirrored or kept box (draw.h).
    rde_rendering_2d_draw_rectangle(fude_draw_at((rde_vec_2F){ (_lo.x + _hi.x) * 0.5f, (_lo.y + _hi.y) * 0.5f }),
                                    (rde_vec_2F){ _hi.x - _lo.x, _hi.y - _lo.y }, fude_theme_active()->select_fill);

    const rde_vec_2F _box[5] = { _lo, { _hi.x, _lo.y }, _hi, { _lo.x, _hi.y }, _lo };
    fude_lasso_line(_box, 5, fude_theme_active()->select);
}
