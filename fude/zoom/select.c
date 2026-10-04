// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/select.h"
#include "drawing/base/theme.h"

#include <math.h>
#include <string.h>

// ===========================================================================
// See select.h.
// ===========================================================================

#define FUDE_ZOOM_SELECT_LOOP_MAX 160u   // a loop's points kept for the inside test (a long one thinned)
#define FUDE_ZOOM_SELECT_SAMPLES  48u    // a stroke's points tested, at most

RDE_INTERNAL fude_zoom_pick* fude_zoom_picks(const fude_zoom_selection* _sel) {
    return (fude_zoom_pick*)_sel->picks.memory;
}

RDE_INTERNAL u32 fude_zoom_pick_count(const fude_zoom_selection* _sel) {
    return (u32)rde_arr_length(&_sel->picks);
}

void fude_zoom_select_init(fude_zoom_selection* _sel) {
    memset(_sel, 0, sizeof(*_sel));
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _sel->picks  = rde_arr_new(sizeof(fude_zoom_pick), _heap);
    _sel->loop   = rde_arr_new(sizeof(rde_vec_2F), _heap);
    _sel->before = rde_arr_new(sizeof(fude_zoom_place), _heap);
    _sel->lifted = rde_arr_new(sizeof(u8), _heap);
    _sel->clip   = rde_arr_new(sizeof(fude_zoom_clip), _heap);
    _sel->found  = rde_arr_new(sizeof(u32), _heap);
    _sel->q      = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
    _sel->box    = fude_zoom_box_empty();
}

RDE_INTERNAL void fude_zoom_select_clip_clear(fude_zoom_selection* _sel) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_clip*       _c    = (fude_zoom_clip*)_sel->clip.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_sel->clip); _i++) {
        _heap->free(_heap->allocator, _c[_i].bytes);
    }
    rde_arr_clear(&_sel->clip);
}

void fude_zoom_select_destroy(fude_zoom_selection* _sel) {
    if(rde_arr_is_inited(&_sel->clip)) {
        fude_zoom_select_clip_clear(_sel);
    }
    rde_arr* _arrays[] = { &_sel->picks, &_sel->loop, &_sel->before, &_sel->lifted, &_sel->clip, &_sel->found, &_sel->q };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    memset(_sel, 0, sizeof(*_sel));
}

RDE_INTERNAL void fude_zoom_select_unlift(fude_zoom_selection* _sel) {
    if(rde_arr_length(&_sel->lifted) > 0) {
        memset(_sel->lifted.memory, 0, rde_arr_length(&_sel->lifted));
    }
}

void fude_zoom_select_clear(fude_zoom_selection* _sel) {
    fude_zoom_select_unlift(_sel);
    rde_arr_clear(&_sel->picks);
    rde_arr_clear(&_sel->loop);
    _sel->grab = FUDE_ZOOM_GRAB_NONE;
    _sel->box  = fude_zoom_box_empty();
}

b8 fude_zoom_select_any(const fude_zoom_selection* _sel) {
    return fude_zoom_pick_count(_sel) > 0;
}

b8 fude_zoom_select_busy(const fude_zoom_selection* _sel) {
    return _sel->grab != FUDE_ZOOM_GRAB_NONE;
}

// --- where things are on screen -------------------------------------------------------------

// A frame's units → the screen, now.
RDE_INTERNAL fude_zoom_sim fude_zoom_select_to_screen(const fude_zoom_scene* _s, u32 _frame) {
    return fude_zoom_sim_compose(fude_zoom_camera_sim(&_s->camera), fude_zoom_scene_sim(_s, _frame, _s->camera.frame));
}

// The frame a pick lives in (a FRAME object's: its parent).
RDE_INTERNAL u32 fude_zoom_pick_frame(const fude_zoom_scene* _s, const fude_zoom_pick* _p) {
    return fude_zoom_scene_object(_s, _p->object)->frame;
}

void fude_zoom_select_update(fude_zoom_selection* _sel, const fude_zoom_scene* _s) {
    if(_sel->grab == FUDE_ZOOM_GRAB_NONE) {
        // What died since (an undo, an eraser) is no longer selected.
        fude_zoom_pick* _p = fude_zoom_picks(_sel);
        u32             _w = 0;
        for(u32 _i = 0; _i < fude_zoom_pick_count(_sel); _i++) {
            if(fude_zoom_scene_object(_s, _p[_i].object)->flags & FUDE_ZOOM_FLAG_ALIVE) {
                _p[_w++] = _p[_i];
            }
        }
        _sel->picks.count = _w;
    }
    _sel->box = fude_zoom_box_empty();
    for(u32 _i = 0; _i < fude_zoom_pick_count(_sel); _i++) {
        const fude_zoom_pick* _p  = &fude_zoom_picks(_sel)[_i];
        fude_zoom_sim         _to = fude_zoom_select_to_screen(_s, fude_zoom_pick_frame(_s, _p));
        if(_sel->grab >= FUDE_ZOOM_GRAB_MOVE) {
            _to = fude_zoom_sim_compose(_sel->drag, _to);
        }
        _sel->box = fude_zoom_box_union(_sel->box, fude_zoom_sim_box(_to, fude_zoom_scene_object(_s, _p->object)->box));
    }
}

b8 fude_zoom_select_box(const fude_zoom_selection* _sel, fude_zoom_box* _out) {
    if(!fude_zoom_select_any(_sel) || fude_zoom_box_is_empty(_sel->box)) {
        return false;
    }
    *_out = fude_zoom_box_grow(_sel->box, FUDE_ZOOM_SELECT_PAD);
    return true;
}

// --- the loop ---------------------------------------------------------------------------------

RDE_INTERNAL b8 fude_zoom_inside(const rde_vec_2F* _poly, u32 _n, f64 _x, f64 _y) {
    b8 _in = false;
    for(u32 _i = 0, _j = _n - 1u; _i < _n; _j = _i++) {
        const f64 _xi = _poly[_i].x, _yi = _poly[_i].y, _xj = _poly[_j].x, _yj = _poly[_j].y;
        if((_yi > _y) != (_yj > _y) && _x < (_xj - _xi) * (_y - _yi) / (_yj - _yi) + _xi) {
            _in = !_in;
        }
    }
    return _in;
}

// Is _f _ancestor itself, or inside it at any depth?
RDE_INTERNAL b8 fude_zoom_within(const fude_zoom_scene* _s, u32 _f, u32 _ancestor) {
    for(u32 _k = 0; _f != FUDE_ZOOM_NONE && _k < 100000u; _k++) {
        if(_f == _ancestor) {
            return true;
        }
        _f = fude_zoom_scene_frame(_s, _f)->parent;
    }
    return false;
}

RDE_INTERNAL void fude_zoom_select_add(fude_zoom_selection* _sel, u32 _object) {
    for(u32 _i = 0; _i < fude_zoom_pick_count(_sel); _i++) {
        if(fude_zoom_picks(_sel)[_i].object == _object) {
            return;
        }
    }
    const fude_zoom_pick _p = { _object };
    rde_arr_add(&_sel->picks, (any)&_p);
}

// What the loop (_poly, screen) takes, in the frames the last draw went into
// at the camera's depth and below (_all: everything there, the loop ignored).
RDE_INTERNAL void fude_zoom_select_take(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_renderer* _r, const rde_vec_2F* _poly, u32 _n, b8 _all) {
    rde_arr _frames = rde_arr_new(sizeof(fude_zoom_visible), rde_memory_allocator_get_default_std());
    fude_zoom_render_editable(_r, _s, &_frames);
    rde_arr _whole = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_box _loop = fude_zoom_box_empty();
    for(u32 _i = 0; _i < _n; _i++) {
        _loop = fude_zoom_box_union(_loop, (fude_zoom_box){ _poly[_i].x, _poly[_i].y, _poly[_i].x, _poly[_i].y });
    }
    const fude_zoom_visible* _v = (const fude_zoom_visible*)_frames.memory;
    for(u32 _k = 0; _k < (u32)rde_arr_length(&_frames); _k++) {
        // Inside a frame taken whole: already selected with it.
        b8 _inside_whole = false;
        for(u32 _w = 0; _w < (u32)rde_arr_length(&_whole) && !_inside_whole; _w++) {
            _inside_whole = fude_zoom_within(_s, _v[_k].frame, ((const u32*)_whole.memory)[_w]);
        }
        if(_inside_whole) {
            continue;
        }
        const fude_zoom_sim _to = _v[_k].to_screen;
        // Strokes: most of their points inside.
        rde_arr_clear(&_sel->found);
        if(_all) {
            const fude_zoom_frame* _f = fude_zoom_scene_frame(_s, _v[_k].frame);
            for(u32 _i = 0; _i < (u32)rde_arr_length(&_f->order); _i++) {
                const u32 _o = ((const u32*)_f->order.memory)[_i];
                const u8 _kind = fude_zoom_scene_object(_s, _o)->kind;
                if(_kind != FUDE_ZOOM_KIND_FRAME && (fude_zoom_scene_object(_s, _o)->flags & FUDE_ZOOM_FLAG_ALIVE)) {
                    rde_arr_add(&_sel->found, (any)&_o);
                }
            }
        } else {
            fude_zoom_scene_query(_s, _v[_k].frame, fude_zoom_sim_box(fude_zoom_sim_inverse(_to), _loop), &_sel->found);
        }
        const u32 _found = (u32)rde_arr_length(&_sel->found);
        for(u32 _i = 0; _i < _found; _i++) {
            const u32               _object = ((const u32*)_sel->found.memory)[_i];
            const fude_zoom_object* _o      = fude_zoom_scene_object(_s, _object);
            if(_o->kind == FUDE_ZOOM_KIND_FRAME || _o->count == 0) {
                continue;
            }
            if(_all) {
                fude_zoom_select_add(_sel, _object);
                continue;
            }
            if(_o->kind == FUDE_ZOOM_KIND_IMAGE) {
                // A picture: most of its corners and its middle inside.
                fude_zoom_v2 _c[5];
                fude_zoom_scene_image_corners(_s, _object, _c);
                _c[4] = (fude_zoom_v2){ (_c[0].x + _c[2].x) * 0.5, (_c[0].y + _c[2].y) * 0.5 };
                u32 _in = 0;
                for(u32 _p = 0; _p < 5u; _p++) {
                    const fude_zoom_v2 _at = fude_zoom_sim_apply(_to, _c[_p]);
                    _in += fude_zoom_inside(_poly, _n, _at.x, _at.y) ? 1u : 0u;
                }
                if(_in >= 3u) {
                    fude_zoom_select_add(_sel, _object);
                }
                continue;
            }
            if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
                // Most of its outline inside.
                rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
                b8      _closed;
                fude_zoom_scene_shape_outline(_s, _object, 48u, &_pts, &_closed);
                u32 _in = 0;
                for(u32 _p = 0; _p < (u32)rde_arr_length(&_pts); _p++) {
                    const fude_zoom_v2 _at = fude_zoom_sim_apply(_to, ((const fude_zoom_v2*)_pts.memory)[_p]);
                    _in += fude_zoom_inside(_poly, _n, _at.x, _at.y) ? 1u : 0u;
                }
                if(_in * 2u > (u32)rde_arr_length(&_pts)) {
                    fude_zoom_select_add(_sel, _object);
                }
                rde_arr_free(&_pts);
                continue;
            }
            rde_arr_clear(&_sel->q);
            rde_arr_add_n(&_sel->q, _o->count);
            if(!fude_zoom_scene_points(_s, _object, (fude_zoom_qpoint*)_sel->q.memory)) {
                continue;
            }
            const fude_zoom_qpoint* _q    = (const fude_zoom_qpoint*)_sel->q.memory;
            const u32               _step = _o->count > FUDE_ZOOM_SELECT_SAMPLES ? _o->count / FUDE_ZOOM_SELECT_SAMPLES : 1u;
            u32 _in = 0, _tested = 0;
            for(u32 _p = 0; _p < _o->count; _p += _step) {
                const fude_zoom_v2 _at = fude_zoom_sim_apply(_to, fude_zoom_scene_point_at(_o, &_q[_p]));
                _in += fude_zoom_inside(_poly, _n, _at.x, _at.y) ? 1u : 0u;
                _tested++;
            }
            if(_in * 2u > _tested) {
                fude_zoom_select_add(_sel, _object);
            }
        }
        // Frames in it: whole when all of their anchor is inside.
        const fude_zoom_frame* _f = fude_zoom_scene_frame(_s, _v[_k].frame);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_f->kids); _i++) {
            const u32               _object = ((const u32*)_f->kids.memory)[_i];
            const fude_zoom_object* _o      = fude_zoom_scene_object(_s, _object);
            if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || fude_zoom_box_is_empty(_o->box) || !fude_zoom_scene_frame_used(_s, _o->child)) {
                continue;
            }
            b8 _in = true;
            if(!_all) {
                const fude_zoom_v2 _c[4] = { { _o->box.min_x, _o->box.min_y }, { _o->box.max_x, _o->box.min_y }, { _o->box.min_x, _o->box.max_y }, { _o->box.max_x, _o->box.max_y } };
                for(u32 _j = 0; _j < 4u && _in; _j++) {
                    const fude_zoom_v2 _at = fude_zoom_sim_apply(_to, _c[_j]);
                    _in = fude_zoom_inside(_poly, _n, _at.x, _at.y);
                }
            }
            if(_in) {
                fude_zoom_select_add(_sel, _object);
                rde_arr_add(&_whole, (any)&_o->child);
            }
        }
    }
    rde_arr_free(&_frames);
    rde_arr_free(&_whole);
}

// --- the pen -----------------------------------------------------------------------------------

// The handle under _p (screen): a corner (its index 0..3: min-min, max-min,
// min-max, max-max), the knob (4), inside the box (5); -1: none.
RDE_INTERNAL i32 fude_zoom_select_handle(const fude_zoom_selection* _sel, rde_vec_2F _p) {
    fude_zoom_box _b;
    if(!fude_zoom_select_box(_sel, &_b)) {
        return -1;
    }
    const fude_zoom_v2 _c[5] = {
        { _b.min_x, _b.min_y }, { _b.max_x, _b.min_y }, { _b.min_x, _b.max_y }, { _b.max_x, _b.max_y },
        { (_b.min_x + _b.max_x) * 0.5, _b.max_y + FUDE_ZOOM_SELECT_KNOB },
    };
    for(i32 _i = 0; _i < 5; _i++) {
        if(hypot(_p.x - _c[_i].x, _p.y - _c[_i].y) <= FUDE_ZOOM_SELECT_GRAB) {
            return _i;
        }
    }
    return fude_zoom_box_contains(_b, (fude_zoom_v2){ _p.x, _p.y }) ? 5 : -1;
}

void fude_zoom_select_down(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_renderer* _r, rde_vec_2F _screen) {
    RDE_UNUSED(_r);
    const i32 _h = fude_zoom_select_handle(_sel, _screen);
    if(_h < 0) {
        fude_zoom_select_clear(_sel);
        _sel->grab = FUDE_ZOOM_GRAB_LOOP;
        rde_arr_add(&_sel->loop, (any)&_screen);
        return;
    }
    fude_zoom_box _b;
    fude_zoom_select_box(_sel, &_b);
    _sel->grab_at = _screen;
    _sel->drag    = fude_zoom_sim_identity();
    if(_h == 5) {
        _sel->grab = FUDE_ZOOM_GRAB_MOVE;
    } else if(_h == 4) {
        _sel->grab  = FUDE_ZOOM_GRAB_ROTATE;
        _sel->pivot = (rde_vec_2F){ (f32)((_b.min_x + _b.max_x) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) };
    } else {
        // A corner scales round the opposite one.
        _sel->grab  = FUDE_ZOOM_GRAB_SCALE;
        _sel->pivot = (rde_vec_2F){ (f32)((_h & 1) ? _b.min_x : _b.max_x), (f32)((_h & 2) ? _b.min_y : _b.max_y) };
    }
    // Where each pick was, and lifted off the canvas while dragged.
    rde_arr_clear(&_sel->before);
    while((u32)rde_arr_length(&_sel->lifted) < fude_zoom_scene_object_count(_s)) {
        const u8 _zero = 0;
        rde_arr_add(&_sel->lifted, (any)&_zero);
    }
    for(u32 _i = 0; _i < fude_zoom_pick_count(_sel); _i++) {
        const fude_zoom_place _p = fude_zoom_scene_place_of(_s, fude_zoom_picks(_sel)[_i].object);
        rde_arr_add(&_sel->before, (any)&_p);
        ((u8*)_sel->lifted.memory)[fude_zoom_picks(_sel)[_i].object] = 1u;
    }
}

void fude_zoom_select_moved(fude_zoom_selection* _sel, rde_vec_2F _screen) {
    if(_sel->grab == FUDE_ZOOM_GRAB_LOOP) {
        const rde_vec_2F* _last = &((const rde_vec_2F*)_sel->loop.memory)[rde_arr_length(&_sel->loop) - 1u];
        if(fabsf(_screen.x - _last->x) + fabsf(_screen.y - _last->y) >= 2.0f) {
            rde_arr_add(&_sel->loop, (any)&_screen);
        }
        return;
    }
    const f64 _px = _sel->pivot.x, _py = _sel->pivot.y;
    if(_sel->grab == FUDE_ZOOM_GRAB_MOVE) {
        _sel->drag = (fude_zoom_sim){ 1.0, 0.0, (f64)(_screen.x - _sel->grab_at.x), (f64)(_screen.y - _sel->grab_at.y) };
    } else if(_sel->grab == FUDE_ZOOM_GRAB_SCALE || _sel->grab == FUDE_ZOOM_GRAB_ROTATE) {
        f64 _a = 1.0, _b = 0.0;
        if(_sel->grab == FUDE_ZOOM_GRAB_SCALE) {
            const f64 _was = hypot(_sel->grab_at.x - _px, _sel->grab_at.y - _py);
            const f64 _now = hypot(_screen.x - _px, _screen.y - _py);
            _a = _was > 1.0 ? fmax(_now / _was, 0.02) : 1.0;
        } else {
            const f64 _turn = atan2(_screen.y - _py, _screen.x - _px) - atan2(_sel->grab_at.y - _py, _sel->grab_at.x - _px);
            _a = cos(_turn);
            _b = sin(_turn);
        }
        // Round the pivot: p' = pivot + R·s·(p − pivot).
        _sel->drag = (fude_zoom_sim){ _a, _b, _px - (_a * _px - _b * _py), _py - (_b * _px + _a * _py) };
    }
}

void fude_zoom_select_up(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_renderer* _r) {
    if(_sel->grab == FUDE_ZOOM_GRAB_LOOP) {
        _sel->grab = FUDE_ZOOM_GRAB_NONE;
        const u32 _n = (u32)rde_arr_length(&_sel->loop);
        if(_n >= 3u) {
            // A long loop thinned for the test (its shape kept).
            rde_vec_2F _poly[FUDE_ZOOM_SELECT_LOOP_MAX];
            const u32  _keep = _n < FUDE_ZOOM_SELECT_LOOP_MAX ? _n : FUDE_ZOOM_SELECT_LOOP_MAX;
            for(u32 _i = 0; _i < _keep; _i++) {
                _poly[_i] = ((const rde_vec_2F*)_sel->loop.memory)[(u64)_i * _n / _keep];
            }
            fude_zoom_select_take(_sel, _s, _r, _poly, _keep, false);
        }
        rde_arr_clear(&_sel->loop);
        fude_zoom_select_update(_sel, _s);
        return;
    }
    if(_sel->grab < FUDE_ZOOM_GRAB_MOVE) {
        return;
    }
    // Where the pen lifted, the canvas takes it back: each pick's place moved
    // by the drag (seen in its own frame's units), one undo step.
    const u32 _n = fude_zoom_pick_count(_sel);
    const b8  _moved = fabs(_sel->drag.a - 1.0) > 1e-9 || fabs(_sel->drag.b) > 1e-9 || fabs(_sel->drag.tx) > 1e-6 || fabs(_sel->drag.ty) > 1e-6;
    if(_moved && _n > 0) {
        rde_memory_allocator* _heap   = rde_memory_allocator_get_default_std();
        u32*                  _ids    = _heap->malloc(_heap->allocator, (usize)_n * sizeof(u32));
        fude_zoom_place*      _after  = _heap->malloc(_heap->allocator, (usize)_n * sizeof(fude_zoom_place));
        for(u32 _i = 0; _i < _n; _i++) {
            const fude_zoom_pick* _p  = &fude_zoom_picks(_sel)[_i];
            const fude_zoom_sim   _to = fude_zoom_select_to_screen(_s, fude_zoom_pick_frame(_s, _p));
            const fude_zoom_sim   _m  = fude_zoom_sim_compose(fude_zoom_sim_inverse(_to), fude_zoom_sim_compose(_sel->drag, _to));
            _ids[_i]   = _p->object;
            _after[_i] = fude_zoom_place_moved(((const fude_zoom_place*)_sel->before.memory)[_i], _m);
        }
        for(u32 _i = 0; _i < _n; _i++) {
            fude_zoom_scene_set_place(_s, _ids[_i], _after[_i]);
        }
        const fude_zoom_box _view = fude_zoom_sim_box(fude_zoom_sim_inverse(fude_zoom_camera_sim(&_s->camera)), _sel->box);
        fude_zoom_history_push_moved(_s, _s->camera.frame, _view, _ids, (const fude_zoom_place*)_sel->before.memory, _after, _n);
        _heap->free(_heap->allocator, _ids);
        _heap->free(_heap->allocator, _after);
    }
    fude_zoom_select_unlift(_sel);
    _sel->grab = FUDE_ZOOM_GRAB_NONE;
    _sel->drag = fude_zoom_sim_identity();
    fude_zoom_select_update(_sel, _s);
}

// --- drawing -----------------------------------------------------------------------------------

// A side of the box, dashed, from _a to _b (screen units, round the middle) —
// only the stretch on screen: deep in, a side runs to millions of points, and
// each dash is a line drawn. Its dashes keep their places counted from _a, so
// they stand still as the page moves.
RDE_INTERNAL void fude_zoom_select_side(fude_zoom_v2 _a, fude_zoom_v2 _b, fude_zoom_v2 _half, rde_color _color) {
    const f64 _dash = 6.0, _period = 10.0, _margin = 8.0;
    const f64 _dx = _b.x - _a.x, _dy = _b.y - _a.y;
    const f64 _len = sqrt(_dx * _dx + _dy * _dy);
    if(_len <= 0.0) {
        return;
    }
    // Liang-Barsky against the screen and a margin: t in [_t0, _t1] of the side.
    f64 _t0 = 0.0, _t1 = 1.0;
    const f64 _p[4] = { -_dx, _dx, -_dy, _dy };
    const f64 _q[4] = { _a.x + _half.x + _margin, _half.x + _margin - _a.x, _a.y + _half.y + _margin, _half.y + _margin - _a.y };
    for(u32 _i = 0; _i < 4u; _i++) {
        if(_p[_i] == 0.0) {
            if(_q[_i] < 0.0) {
                return;
            }
            continue;
        }
        const f64 _t = _q[_i] / _p[_i];
        if(_p[_i] < 0.0) {
            _t0 = _t > _t0 ? _t : _t0;
        } else {
            _t1 = _t < _t1 ? _t : _t1;
        }
    }
    if(_t0 >= _t1) {
        return;
    }
    const f64 _ux = _dx / _len, _uy = _dy / _len;
    const f64 _end = _t1 * _len;
    for(f64 _d = floor(_t0 * _len / _period) * _period; _d < _end; _d += _period) {
        const f64 _e = _d + _dash < _end ? _d + _dash : _end;
        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)(_a.x + _ux * _d), (f32)(_a.y + _uy * _d) }, (rde_vec_2F){ (f32)(_a.x + _ux * _e), (f32)(_a.y + _uy * _e) }, _color, 1.2f);
    }
}

void fude_zoom_select_render(fude_zoom_selection* _sel, fude_zoom_renderer* _r, const fude_zoom_scene* _s, fude_zoom_v2 _half) {
    const fude_theme* _theme  = fude_theme_active();
    rde_color         _accent = _theme->accent;
    // What is lifted, where the drag has it; a glow over what is selected.
    rde_color _glow = _accent;
    _glow.a = 70;
    for(u32 _i = 0; _i < fude_zoom_pick_count(_sel); _i++) {
        const fude_zoom_pick* _p  = &fude_zoom_picks(_sel)[_i];
        fude_zoom_sim         _to = fude_zoom_select_to_screen(_s, fude_zoom_pick_frame(_s, _p));
        if(_sel->grab >= FUDE_ZOOM_GRAB_MOVE) {
            _to = fude_zoom_sim_compose(_sel->drag, _to);
            fude_zoom_render_object(_r, _s, _p->object, _to, _half);
        }
        if(fude_zoom_scene_object(_s, _p->object)->kind != FUDE_ZOOM_KIND_FRAME) {
            fude_zoom_render_glow(_r, _s, _p->object, _to, _half, 3.0f, _glow);
        }
    }
    // The box, its corners and its knob.
    fude_zoom_box _b;
    if(fude_zoom_select_box(_sel, &_b) && _sel->grab != FUDE_ZOOM_GRAB_LOOP) {
        const fude_zoom_v2 _k[4] = { { _b.min_x, _b.min_y }, { _b.max_x, _b.min_y }, { _b.max_x, _b.max_y }, { _b.min_x, _b.max_y } };
        const rde_vec_2F   _c[4] = { { (f32)_b.min_x, (f32)_b.min_y }, { (f32)_b.max_x, (f32)_b.min_y }, { (f32)_b.max_x, (f32)_b.max_y }, { (f32)_b.min_x, (f32)_b.max_y } };
        for(u32 _i = 0; _i < 4u; _i++) {
            fude_zoom_select_side(_k[_i], _k[(_i + 1u) % 4u], _half, _accent);
        }
        const rde_vec_2F _top  = { (f32)((_b.min_x + _b.max_x) * 0.5), (f32)_b.max_y };
        const rde_vec_2F _knob = { _top.x, _top.y + FUDE_ZOOM_SELECT_KNOB };
        rde_rendering_2d_draw_line_1(_top, _knob, _accent, 1.2f);
        for(u32 _i = 0; _i < 4u; _i++) {
            rde_rendering_2d_draw_circle_with_border(_c[_i], FUDE_ZOOM_SELECT_HANDLE, 20u, _theme->surface, 1.5f, _accent, NULL);
        }
        rde_rendering_2d_draw_circle_with_border(_knob, FUDE_ZOOM_SELECT_HANDLE, 20u, _accent, 1.5f, _accent, NULL);
    }
    // The loop being drawn.
    if(_sel->grab == FUDE_ZOOM_GRAB_LOOP && rde_arr_length(&_sel->loop) >= 2u) {
        const rde_vec_2F* _pts = (const rde_vec_2F*)_sel->loop.memory;
        for(u32 _i = 0; _i + 1u < (u32)rde_arr_length(&_sel->loop); _i++) {
            rde_rendering_2d_draw_line_segmented(_pts[_i], _pts[_i + 1u], _accent, 1.2f, 5.0f, 4.0f);
        }
    }
}

// --- commands ----------------------------------------------------------------------------------

void fude_zoom_select_delete(fude_zoom_selection* _sel, fude_zoom_scene* _s) {
    const u32 _n = fude_zoom_pick_count(_sel);
    if(_n == 0) {
        return;
    }
    rde_arr _died = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    for(u32 _i = 0; _i < _n; _i++) {
        const u32 _o = fude_zoom_picks(_sel)[_i].object;
        if(fude_zoom_scene_object(_s, _o)->flags & FUDE_ZOOM_FLAG_ALIVE) {
            fude_zoom_scene_set_alive(_s, _o, false);
            rde_arr_add(&_died, (any)&_o);
        }
    }
    const fude_zoom_box _view = fude_zoom_sim_box(fude_zoom_sim_inverse(fude_zoom_camera_sim(&_s->camera)), _sel->box);
    fude_zoom_history_push(_s, _s->camera.frame, _view, (const u32*)_died.memory, (u32)rde_arr_length(&_died), NULL, 0);
    rde_arr_free(&_died);
    fude_zoom_select_clear(_sel);
}

void fude_zoom_select_copy(fude_zoom_selection* _sel, const fude_zoom_scene* _s) {
    if(!fude_zoom_select_any(_sel)) {
        return;
    }
    fude_zoom_select_clip_clear(_sel);
    _sel->clip_center = (fude_zoom_v2){ (_sel->box.min_x + _sel->box.max_x) * 0.5, (_sel->box.min_y + _sel->box.max_y) * 0.5 };
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    // In the order they are drawn, so a paste keeps what is over what.
    for(u32 _i = 0; _i < fude_zoom_pick_count(_sel); _i++) {
        const fude_zoom_pick*   _p = &fude_zoom_picks(_sel)[_i];
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _p->object);
        if(_o->kind == FUDE_ZOOM_KIND_FRAME || _o->blob == 0) {
            continue;
        }
        const fude_zoom_blob* _b = &((const fude_zoom_blob*)_s->blobs.memory)[_o->blob - 1u];
        fude_zoom_clip _c = { .look = *_o, .size = _b->size };
        _c.bytes = _heap->malloc(_heap->allocator, _b->size > 0 ? _b->size : 1u);
        memcpy(_c.bytes, _b->data, _b->size);
        _c.on_screen = fude_zoom_sim_compose(fude_zoom_select_to_screen(_s, _o->frame), fude_zoom_object_sim(_o));
        rde_arr_add(&_sel->clip, &_c);
    }
}

b8 fude_zoom_select_can_paste(const fude_zoom_selection* _sel) {
    return rde_arr_length(&_sel->clip) > 0;
}

void fude_zoom_select_paste(fude_zoom_selection* _sel, fude_zoom_scene* _s, rde_vec_2F _screen) {
    const u32 _n = (u32)rde_arr_length(&_sel->clip);
    if(_n == 0) {
        return;
    }
    fude_zoom_select_clear(_sel);
    // Each copy as it looked on screen, its middle where asked, into the
    // camera's frame (the same size on screen, whatever the zoom now).
    const u32           _frame   = _s->camera.frame;
    const fude_zoom_sim _from_sc = fude_zoom_sim_inverse(fude_zoom_camera_sim(&_s->camera));
    const fude_zoom_sim _shift   = { 1.0, 0.0, (f64)_screen.x - _sel->clip_center.x, (f64)_screen.y - _sel->clip_center.y };
    rde_arr _born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_clip* _c = &((const fude_zoom_clip*)_sel->clip.memory)[_i];
        if(_c->look.kind == FUDE_ZOOM_KIND_IMAGE && _c->size > 2u * sizeof(f64)) {
            const fude_zoom_sim   _m  = fude_zoom_sim_compose(_from_sc, fude_zoom_sim_compose(_shift, _c->on_screen));
            const fude_zoom_place _at = { { _m.tx, _m.ty }, atan2(_m.b, _m.a), hypot(_m.a, _m.b) };
            f64 _hw, _hh;
            memcpy(&_hw, _c->bytes, sizeof(f64));
            memcpy(&_hh, _c->bytes + sizeof(f64), sizeof(f64));
            const u32 _o = fude_zoom_scene_add_image(_s, _frame, _at, _hw, _hh, _c->bytes + 2u * sizeof(f64), _c->size - 2u * (u32)sizeof(f64), 0);
            rde_arr_add(&_born, (any)&_o);
            fude_zoom_select_add(_sel, _o);
            continue;
        }
        if(_c->look.kind == FUDE_ZOOM_KIND_SHAPE) {
            const fude_zoom_sim   _m  = fude_zoom_sim_compose(_from_sc, fude_zoom_sim_compose(_shift, _c->on_screen));
            const fude_zoom_place _at = { { _m.tx, _m.ty }, atan2(_m.b, _m.a), hypot(_m.a, _m.b) };
            const u32 _o = fude_zoom_scene_add_shape_fill(_s, _frame, _at, _c->look.channels, (const f64*)_c->bytes, _c->size / (u32)sizeof(f64),
                                                          _c->look.color, _c->look.radius, (u8)(_c->look.flags & (FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN)),
                                                          _c->look.fill, 0);
            rde_arr_add(&_born, (any)&_o);
            fude_zoom_select_add(_sel, _o);
            continue;
        }
        rde_arr_clear(&_sel->q);
        rde_arr_add_n(&_sel->q, _c->look.count);
        if(!fude_zoom_codec_decode(_c->bytes, _c->size, (fude_zoom_qpoint*)_sel->q.memory, _c->look.count, _c->look.channels)) {
            continue;
        }
        const fude_zoom_sim   _m  = fude_zoom_sim_compose(_from_sc, fude_zoom_sim_compose(_shift, _c->on_screen));
        const fude_zoom_place _at = { { _m.tx, _m.ty }, atan2(_m.b, _m.a), hypot(_m.a, _m.b) };
        if(_c->look.kind == FUDE_ZOOM_KIND_FILL) {
            const u32 _o = fude_zoom_scene_add_fill_flags(_s, _frame, _at, _c->look.q, (const fude_zoom_qpoint*)_sel->q.memory, _c->look.count, _c->look.color, _c->look.flags, 0);
            rde_arr_add(&_born, (any)&_o);
            fude_zoom_select_add(_sel, _o);
            continue;
        }
        const u8 _flags = (u8)(_c->look.flags & (FUDE_ZOOM_FLAG_FROM_PEN | FUDE_ZOOM_FLAG_MARKER | FUDE_ZOOM_FLAG_PRESSURE));
        const u32 _o = fude_zoom_scene_add_stroke_at(_s, _frame, _at, _c->look.q, (const fude_zoom_qpoint*)_sel->q.memory, _c->look.count, _c->look.channels,
                                                     _c->look.color, _c->look.radius, _flags, 0, 0);
        rde_arr_add(&_born, (any)&_o);
        fude_zoom_select_add(_sel, _o);
    }
    fude_zoom_box _box = fude_zoom_box_empty();
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_born); _i++) {
        _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, ((const u32*)_born.memory)[_i])->box);
    }
    fude_zoom_history_push(_s, _frame, _box, NULL, 0, (const u32*)_born.memory, (u32)rde_arr_length(&_born));
    rde_arr_free(&_born);
    fude_zoom_select_update(_sel, _s);
}

void fude_zoom_select_duplicate(fude_zoom_selection* _sel, fude_zoom_scene* _s) {
    if(!fude_zoom_select_any(_sel)) {
        return;
    }
    fude_zoom_select_copy(_sel, _s);
    const rde_vec_2F _at = { (f32)(_sel->clip_center.x + FUDE_ZOOM_SELECT_DUPLICATE), (f32)(_sel->clip_center.y - FUDE_ZOOM_SELECT_DUPLICATE) };
    fude_zoom_select_paste(_sel, _s, _at);
}

void fude_zoom_select_all(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_renderer* _r) {
    fude_zoom_select_clear(_sel);
    fude_zoom_select_take(_sel, _s, _r, NULL, 0, true);
    fude_zoom_select_update(_sel, _s);
}
