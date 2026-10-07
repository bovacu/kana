// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/select.h"
#include "zoom/connect.h"
#include "zoom/shape.h"
#include "zoom/circuit.h"
#include "zoom/props.h"
#include "drawing/base/theme.h"

#include <math.h>
#include <stdlib.h>
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
    fude_zoom_clip* _c = (fude_zoom_clip*)_sel->clip.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_sel->clip); _i++) {
        rde_arr_free(&_c[_i].bytes);
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

RDE_INTERNAL void fude_zoom_select_hold(fude_zoom_selection* _sel, const fude_zoom_scene* _s);
RDE_INTERNAL void fude_zoom_select_commit(fude_zoom_selection* _sel, fude_zoom_scene* _s);

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
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _p->object);
        if(_o->kind == FUDE_ZOOM_KIND_SHAPE && _o->channels == FUDE_ZOOM_SHAPE_GUIDE && _sel->half.x > 0.0) {
            // A guide: the part of it on the screen (it reaches far past it).
            const fude_zoom_sim _all = fude_zoom_sim_compose(_to, fude_zoom_object_sim(_o));
            f64 _n[3];
            const u32 _count = fude_zoom_scene_shape_numbers(_s, _p->object, _n, 3u);
            rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
            b8 _closed;
            fude_zoom_shape_outline(FUDE_ZOOM_SHAPE_GUIDE, _n, _count, 1u, &_pts, &_closed);
            if(rde_arr_length(&_pts) == 2u) {
                fude_zoom_v2 _a = fude_zoom_sim_apply(_all, ((const fude_zoom_v2*)_pts.memory)[0]), _b = fude_zoom_sim_apply(_all, ((const fude_zoom_v2*)_pts.memory)[1]);
                if(fude_zoom_clip_line(&_a, &_b, (fude_zoom_box){ -_sel->half.x, -_sel->half.y, _sel->half.x, _sel->half.y })) {
                    _sel->box = fude_zoom_box_union(_sel->box, (fude_zoom_box){ fmin(_a.x, _b.x), fmin(_a.y, _b.y), fmax(_a.x, _b.x), fmax(_a.y, _b.y) });
                }
            }
            rde_arr_free(&_pts);
            continue;
        }
        _sel->box = fude_zoom_box_union(_sel->box, fude_zoom_sim_box(_to, _o->box));
    }
}

b8 fude_zoom_select_box(const fude_zoom_selection* _sel, fude_zoom_box* _out) {
    if(!fude_zoom_select_any(_sel) || fude_zoom_box_is_empty(_sel->box)) {
        return false;
    }
    *_out = fude_zoom_box_grow(_sel->box, FUDE_ZOOM_SELECT_PAD);
    return true;
}

// Things moved, one undo step: the circuit's wires joined to them (circuit.h) made again to their pins where they are now.
RDE_INTERNAL void fude_zoom_select_push_moved(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, const u32* _ids, const fude_zoom_place* _before,
                                              const fude_zoom_place* _after, u32 _n) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _died = rde_arr_new(sizeof(u32), _heap), _born = rde_arr_new(sizeof(u32), _heap);
    fude_zoom_wire_follow(_s, _ids, _n, &_died, &_born);
    fude_zoom_history_push_all(_s, _frame, _box, (const u32*)_died.memory, (u32)rde_arr_length(&_died), (const u32*)_born.memory, (u32)rde_arr_length(&_born),
                               _ids, _before, _after, _n);
    rde_arr_free(&_died);
    rde_arr_free(&_born);
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
            if(_o->kind == FUDE_ZOOM_KIND_FRAME || _o->kind == FUDE_ZOOM_KIND_MARK || _o->kind == FUDE_ZOOM_KIND_LAYER || _o->count == 0 ||
               !fude_zoom_scene_touchable(_s, _o) || (_o->kind == FUDE_ZOOM_KIND_SHAPE && fude_zoom_shape_is_attribute(_o->channels))) {
                continue;   // (a bookmark is never picked: it is not drawn; nor a constraint; nor what a hidden or locked layer holds)
            }
            if(_all) {
                fude_zoom_select_add(_sel, _object);
                continue;
            }
            if(_o->kind == FUDE_ZOOM_KIND_IMAGE || _o->kind == FUDE_ZOOM_KIND_TEXT) {
                // A picture or a text: most of its corners and its middle inside.
                fude_zoom_v2 _c[5];
                if(_o->kind == FUDE_ZOOM_KIND_IMAGE) {
                    fude_zoom_scene_image_corners(_s, _object, _c);
                } else {
                    fude_zoom_scene_text_corners(_s, _object, _c);
                }
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

i32 fude_zoom_select_handle_at(const fude_zoom_selection* _sel, rde_vec_2F _screen) {
    return fude_zoom_select_any(_sel) ? fude_zoom_select_handle(_sel, _screen) : -1;
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
        _sel->turn  = 0.0;
        _sel->pivot = (rde_vec_2F){ (f32)((_b.min_x + _b.max_x) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) };
    } else {
        // A corner scales round the opposite one.
        _sel->grab  = FUDE_ZOOM_GRAB_SCALE;
        _sel->pivot = (rde_vec_2F){ (f32)((_h & 1) ? _b.min_x : _b.max_x), (f32)((_h & 2) ? _b.min_y : _b.max_y) };
    }
    fude_zoom_select_hold(_sel, _s);
}

// Where each pick was, and lifted off the canvas while dragged.
RDE_INTERNAL void fude_zoom_select_hold(fude_zoom_selection* _sel, const fude_zoom_scene* _s) {
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

void fude_zoom_select_begin(fude_zoom_selection* _sel, fude_zoom_scene* _s) {
    if(!fude_zoom_select_any(_sel) || fude_zoom_select_busy(_sel)) {
        return;
    }
    _sel->grab = FUDE_ZOOM_GRAB_MOVE;
    _sel->drag = fude_zoom_sim_identity();
    fude_zoom_select_hold(_sel, _s);
}

void fude_zoom_select_end(fude_zoom_selection* _sel, fude_zoom_scene* _s, fude_zoom_sim _drag) {
    if(_sel->grab != FUDE_ZOOM_GRAB_MOVE) {
        return;
    }
    _sel->drag = _drag;
    fude_zoom_select_update(_sel, _s);   // (its box where it goes: what the undo step shows)
    fude_zoom_select_commit(_sel, _s);
}

void fude_zoom_select_carry(fude_zoom_selection* _sel, fude_zoom_scene* _s, const u32* _objects, u32 _n) {
    if(_sel->grab < FUDE_ZOOM_GRAB_MOVE) {
        return;
    }
    while((u32)rde_arr_length(&_sel->lifted) < fude_zoom_scene_object_count(_s)) {
        const u8 _zero = 0;
        rde_arr_add(&_sel->lifted, (any)&_zero);
    }
    for(u32 _i = 0; _i < _n; _i++) {
        const u32 _o = _objects[_i];
        if(_o >= fude_zoom_scene_object_count(_s) || ((const u8*)_sel->lifted.memory)[_o] != 0u) {
            continue;   // (a pick already)
        }
        const fude_zoom_pick  _pick = { _o };
        const fude_zoom_place _p    = fude_zoom_scene_place_of(_s, _o);
        rde_arr_add(&_sel->picks, (any)&_pick);
        rde_arr_add(&_sel->before, (any)&_p);
        ((u8*)_sel->lifted.memory)[_o] = 1u;
    }
}

void fude_zoom_select_keep(fude_zoom_selection* _sel, const fude_zoom_scene* _s, u32 _n) {
    if(_n < fude_zoom_pick_count(_sel)) {
        _sel->picks.count = _n;
        fude_zoom_select_update(_sel, _s);
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
            f64 _turn = atan2(_screen.y - _py, _screen.x - _px) - atan2(_sel->grab_at.y - _py, _sel->grab_at.x - _px);
            while(_turn > 3.14159265358979323846)   { _turn -= 2.0 * 3.14159265358979323846; }
            while(_turn <= -3.14159265358979323846) { _turn += 2.0 * 3.14159265358979323846; }
            // Resting a moment at each 15° (within 1.5° of it), as the instruments do.
            const f64 _step = 3.14159265358979323846 / 12.0;
            const f64 _near = round(_turn / _step) * _step;
            _turn = fabs(_turn - _near) < 0.026 ? _near : _turn;
            _sel->turn = _turn;
            _a = cos(_turn);
            _b = sin(_turn);
        }
        // Round the pivot: p' = pivot + R·s·(p − pivot).
        _sel->drag = (fude_zoom_sim){ _a, _b, _px - (_a * _px - _b * _py), _py - (_b * _px + _a * _py) };
    }
}

b8 fude_zoom_select_turning(const fude_zoom_selection* _sel, f64* _degrees, rde_vec_2F* _at) {
    if(_sel->grab != FUDE_ZOOM_GRAB_ROTATE) {
        return false;
    }
    *_degrees = _sel->turn * 180.0 / 3.14159265358979323846;
    // Over the knob as it was taken (the box above the pivot), turned with it.
    const f64 _dx = _sel->grab_at.x - _sel->pivot.x, _dy = _sel->grab_at.y - _sel->pivot.y;
    const f64 _len = hypot(_dx, _dy), _c = cos(_sel->turn), _s = sin(_sel->turn);
    const f64 _out = _len > 1.0 ? (_len + 34.0) / _len : 1.0;
    *_at = (rde_vec_2F){ (f32)(_sel->pivot.x + (_dx * _c - _dy * _s) * _out), (f32)(_sel->pivot.y + (_dx * _s + _dy * _c) * _out) };
    return true;
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
    fude_zoom_select_commit(_sel, _s);
}

// Where the pen lifted, the canvas takes it back: each pick's place moved by the drag (seen in its own frame's
// units), one undo step.
RDE_INTERNAL void fude_zoom_select_commit(fude_zoom_selection* _sel, fude_zoom_scene* _s) {
    const u32 _n = fude_zoom_pick_count(_sel);
    const b8  _moved = fabs(_sel->drag.a - 1.0) > 1e-9 || fabs(_sel->drag.b) > 1e-9 || fabs(_sel->drag.tx) > 1e-6 || fabs(_sel->drag.ty) > 1e-6;
    if(_moved && _n > 0) {
        rde_memory_allocator*         _heap      = rde_memory_allocator_get_default_std();
        rde_arr TYPE(u32)             _ids_arr   = rde_arr_new(sizeof(u32), _heap);
        rde_arr TYPE(fude_zoom_place) _after_arr = rde_arr_new(sizeof(fude_zoom_place), _heap);
        u32*                          _ids       = (u32*)rde_arr_add_n(&_ids_arr, _n);   // (sized once: they stay put)
        fude_zoom_place*              _after     = (fude_zoom_place*)rde_arr_add_n(&_after_arr, _n);
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
        // The connectors joined to what moved follow it, in the same step (connect.h).
        rde_arr _all_ids    = rde_arr_new(sizeof(u32), _heap);
        rde_arr _all_before = rde_arr_new(sizeof(fude_zoom_place), _heap);
        rde_arr _all_after  = rde_arr_new(sizeof(fude_zoom_place), _heap);
        memcpy(rde_arr_add_n(&_all_ids, _n), _ids, (usize)_n * sizeof(u32));
        memcpy(rde_arr_add_n(&_all_before, _n), _sel->before.memory, (usize)_n * sizeof(fude_zoom_place));
        memcpy(rde_arr_add_n(&_all_after, _n), _after, (usize)_n * sizeof(fude_zoom_place));
        u32 _frames[8];
        u32 _frame_count = 0;
        for(u32 _i = 0; _i < _n; _i++) {
            const u32 _f = fude_zoom_scene_object(_s, _ids[_i])->frame;
            b8 _seen = false;
            for(u32 _k = 0; _k < _frame_count; _k++) {
                _seen = _seen || _frames[_k] == _f;
            }
            if(!_seen && _frame_count < 8u) {
                _frames[_frame_count++] = _f;
            }
        }
        for(u32 _k = 0; _k < _frame_count; _k++) {
            fude_zoom_connect_follow(_s, _frames[_k], _ids, _n, &_all_ids, &_all_before, &_all_after);
        }
        const fude_zoom_box _view = fude_zoom_sim_box(fude_zoom_sim_inverse(fude_zoom_camera_sim(&_s->camera)), _sel->box);
        fude_zoom_select_push_moved(_s, _s->camera.frame, _view, (const u32*)_all_ids.memory, (const fude_zoom_place*)_all_before.memory,
                                     (const fude_zoom_place*)_all_after.memory, (u32)rde_arr_length(&_all_ids));
        rde_arr_free(&_all_ids);
        rde_arr_free(&_all_before);
        rde_arr_free(&_all_after);
        rde_arr_free(&_ids_arr);
        rde_arr_free(&_after_arr);
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
    fude_zoom_wire_orphans(_s, &_died);   // (the wires their parts kept go with them)
    const fude_zoom_box _view = fude_zoom_sim_box(fude_zoom_sim_inverse(fude_zoom_camera_sim(&_s->camera)), _sel->box);
    fude_zoom_history_push(_s, _s->camera.frame, _view, (const u32*)_died.memory, (u32)rde_arr_length(&_died), NULL, 0);
    rde_arr_free(&_died);
    fude_zoom_select_clear(_sel);
}

// What moving onto a layer takes: _object itself, or everything inside it when it is a frame.
RDE_INTERNAL void fude_zoom_select_layer_take(const fude_zoom_scene* _s, u32 _object, u32 _pick, rde_arr* _todo, u32 _depth) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind == FUDE_ZOOM_KIND_MARK || _o->kind == FUDE_ZOOM_KIND_LAYER || !fude_zoom_scene_touchable(_s, _o)) {
        return;
    }
    if(_o->kind != FUDE_ZOOM_KIND_FRAME) {
        const u32 _item[2] = { _object, _pick };
        rde_arr_add(_todo, (any)_item);
        return;
    }
    if(_depth > 64u) {
        return;
    }
    const fude_zoom_frame* _f = fude_zoom_scene_frame(_s, _o->child);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_f->order); _i++) {
        fude_zoom_select_layer_take(_s, ((const u32*)_f->order.memory)[_i], FUDE_ZOOM_NONE, _todo, _depth + 1u);
    }
}

u32 fude_zoom_select_to_layer(fude_zoom_selection* _sel, fude_zoom_scene* _s, u16 _layer) {
    const u32 _n = fude_zoom_pick_count(_sel);
    if(_n == 0 || fude_zoom_select_busy(_sel)) {
        return 0;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _todo = rde_arr_new(2u * sizeof(u32), _heap);   // (object, its pick or FUDE_ZOOM_NONE)
    for(u32 _i = 0; _i < _n; _i++) {
        fude_zoom_select_layer_take(_s, fude_zoom_picks(_sel)[_i].object, _i, &_todo, 0);
    }
    const u32 _count = (u32)rde_arr_length(&_todo);
    u32* _items = (u32*)_todo.memory;
    rde_arr _died = rde_arr_new(sizeof(u32), _heap), _born = rde_arr_new(sizeof(u32), _heap);
    rde_arr _old  = rde_arr_new(sizeof(fude_zoom_id), _heap), _new = rde_arr_new(sizeof(fude_zoom_id), _heap);
    // Each one copied onto the layer where it is, in its order (a stroke's first
    // piece first, so its later pieces join the copy). Connectors after.
    for(u32 _pass = 0; _pass < 2u; _pass++) {
        for(u32 _i = 0; _i < _count; _i++) {
            const u32 _object = _items[_i * 2u], _pick = _items[_i * 2u + 1u];
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
            if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->layer == _layer || (_o->kind == FUDE_ZOOM_KIND_SHAPE && _o->channels == FUDE_ZOOM_SHAPE_ARROW)) {
                continue;
            }
            const b8 _first = _o->gesture == 0 || _o->gesture == _o->id;
            if(_first != (_pass == 0)) {
                continue;
            }
            fude_zoom_id _gesture = 0;
            for(u32 _k = 0; !_first && _k < (u32)rde_arr_length(&_old); _k++) {
                if(((const fude_zoom_id*)_old.memory)[_k] == _o->gesture) {
                    _gesture = ((const fude_zoom_id*)_new.memory)[_k];
                }
            }
            const fude_zoom_id _id   = _o->id;
            const u32          _copy = fude_zoom_scene_copy_to_layer(_s, _object, _layer, _gesture);   // (the objects may move in memory)
            if(_copy == FUDE_ZOOM_NONE) {
                continue;
            }
            fude_zoom_scene_set_alive(_s, _object, false);
            rde_arr_add(&_died, (any)&_object);
            rde_arr_add(&_born, (any)&_copy);
            rde_arr_add(&_old, (any)&_id);
            const fude_zoom_id _copy_id = fude_zoom_scene_object(_s, _copy)->id;
            rde_arr_add(&_new, (any)&_copy_id);
            if(_pick != FUDE_ZOOM_NONE) {
                fude_zoom_picks(_sel)[_pick].object = _copy;
            }
        }
    }
    // Connectors: those taken onto the layer, and any joined to what was copied joined to the copies.
    for(u32 _i = 0; _i < _count; _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _items[_i * 2u]);
        if(_o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_ARROW) {
            _items[_i * 2u] = FUDE_ZOOM_NONE;   // (what is left: the arrows taken)
        }
    }
    const u32 _objects = fude_zoom_scene_object_count(_s);
    for(u32 _a = 0; _a < _objects; _a++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _a);
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_ARROW) {
            continue;
        }
        u32 _taken = FUDE_ZOOM_NONE;
        for(u32 _i = 0; _i < _count && _taken == FUDE_ZOOM_NONE; _i++) {
            _taken = _items[_i * 2u] == _a ? _i : FUDE_ZOOM_NONE;
        }
        const u16 _to   = _taken != FUDE_ZOOM_NONE ? _layer : _o->layer;
        const u32 _made = fude_zoom_connect_remap(_s, _a, (const fude_zoom_id*)_old.memory, (const fude_zoom_id*)_new.memory, (u32)rde_arr_length(&_old), _to);
        if(_made == FUDE_ZOOM_NONE) {
            continue;
        }
        fude_zoom_scene_set_alive(_s, _a, false);
        rde_arr_add(&_died, (any)&_a);
        rde_arr_add(&_born, (any)&_made);
        if(_taken != FUDE_ZOOM_NONE && _items[_taken * 2u + 1u] != FUDE_ZOOM_NONE) {
            fude_zoom_picks(_sel)[_items[_taken * 2u + 1u]].object = _made;
        }
    }
    const u32 _moved = (u32)rde_arr_length(&_born);
    if(_moved > 0) {
        const fude_zoom_box _view = fude_zoom_sim_box(fude_zoom_sim_inverse(fude_zoom_camera_sim(&_s->camera)), _sel->box);
        fude_zoom_history_push(_s, _s->camera.frame, _view, (const u32*)_died.memory, (u32)rde_arr_length(&_died), (const u32*)_born.memory, _moved);
    }
    rde_arr_free(&_todo); rde_arr_free(&_died); rde_arr_free(&_born); rde_arr_free(&_old); rde_arr_free(&_new);
    fude_zoom_select_update(_sel, _s);
    return _moved;
}

// A copy of object _object onto _out (not a frame, a mark, a layer, nothing gone).
RDE_INTERNAL void fzsel_clip_add(const fude_zoom_scene* _s, u32 _object, rde_arr* _out) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if(_o->kind == FUDE_ZOOM_KIND_FRAME || _o->kind == FUDE_ZOOM_KIND_MARK || _o->kind == FUDE_ZOOM_KIND_LAYER || _o->blob == 0 || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
        return;
    }
    const fude_zoom_blob* _b = &((const fude_zoom_blob*)_s->blobs.memory)[_o->blob - 1u];
    fude_zoom_clip _c = { .look = *_o };
    fude_zoom_clip_put(&_c, fude_zoom_blob_bytes(_b), fude_zoom_blob_size(_b));
    _c.on_screen = fude_zoom_sim_compose(fude_zoom_select_to_screen(_s, _o->frame), fude_zoom_object_sim(_o));
    rde_arr_add(_out, &_c);
}

void fude_zoom_select_clips_of(const fude_zoom_scene* _s, const u32* _objects, u32 _n, rde_arr* _out) {
    const u32 _count = fude_zoom_scene_object_count(_s);
    rde_arr _copied = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_copied, _count);
    for(u32 _i = 0; _i < _n; _i++) {
        if(_objects[_i] >= _count) {
            continue;
        }
        fzsel_clip_add(_s, _objects[_i], _out);
        ((u8*)_copied.memory)[_objects[_i]] = 1u;
    }
    // The properties on them (a body's, a probe's light: props.h) copied with them, to be on their copies.
    for(u32 _a = 0; _a < _count; _a++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _a);
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_PROPS) {
            continue;
        }
        const u32 _t = fude_zoom_props_target(_s, _a);
        if(_t != FUDE_ZOOM_NONE && _t < _count && ((const u8*)_copied.memory)[_t]) {
            fzsel_clip_add(_s, _a, _out);
        }
    }
    rde_arr_free(&_copied);
}

void fude_zoom_clip_put(fude_zoom_clip* _c, const u8* _data, u32 _size) {
    _c->bytes = rde_arr_new_with_capacity(sizeof(u8), (usize)_size + 1u, rde_memory_allocator_get_default_std());
    if(_size > 0u) {
        memcpy(rde_arr_add_n(&_c->bytes, _size), _data, _size);
    }
}

void fude_zoom_select_clips_free(rde_arr* _clips) {
    fude_zoom_clip* _c = (fude_zoom_clip*)_clips->memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(_clips); _i++) {
        rde_arr_free(&_c[_i].bytes);
    }
    rde_arr_clear(_clips);
}

void fude_zoom_select_copy(fude_zoom_selection* _sel, const fude_zoom_scene* _s) {
    if(!fude_zoom_select_any(_sel)) {
        return;
    }
    fude_zoom_select_clip_clear(_sel);
    _sel->clip_center = (fude_zoom_v2){ (_sel->box.min_x + _sel->box.max_x) * 0.5, (_sel->box.min_y + _sel->box.max_y) * 0.5 };
    // In the order they are drawn, so a paste keeps what is over what.
    for(u32 _i = 0; _i < fude_zoom_pick_count(_sel); _i++) {
        fude_zoom_select_clips_of(_s, &fude_zoom_picks(_sel)[_i].object, 1u, &_sel->clip);
    }
}

b8 fude_zoom_select_can_paste(const fude_zoom_selection* _sel) {
    return rde_arr_length(&_sel->clip) > 0;
}

void fude_zoom_select_paste(fude_zoom_selection* _sel, fude_zoom_scene* _s, rde_vec_2F _screen) {
    fude_zoom_select_paste_clips(_sel, _s, (const fude_zoom_clip*)_sel->clip.memory, (u32)rde_arr_length(&_sel->clip), _sel->clip_center, _screen);
}

void fude_zoom_select_paste_clips(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_clip* _clips, u32 _n, fude_zoom_v2 _center, rde_vec_2F _screen) {
    if(_n == 0) {
        return;
    }
    fude_zoom_select_clear(_sel);
    const fude_zoom_sim _shift = { 1.0, 0.0, (f64)_screen.x - _center.x, (f64)_screen.y - _center.y };
    fude_zoom_select_put(_sel, _s, _clips, _n, &_shift, 1u, NULL, NULL, 0u);
}

// A turn's angle brought within half a turn either way.
RDE_INTERNAL f64 fude_zoom_select_wrap(f64 _a) {
    const f64 _pi = 3.14159265358979323846;
    while(_a > _pi)  { _a -= 2.0 * _pi; }
    while(_a < -_pi) { _a += 2.0 * _pi; }
    return _a;
}

// Where a copy goes on the screen: _move ∘ its place there — or, mirrored across _mirror's line, the similarity that
// with its own y turned over (*_flip) is that reflection of it. Text and pictures are never turned over (they would
// read backwards): theirs keeps them the right way round, their box where the reflection puts it.
RDE_INTERNAL fude_zoom_sim fude_zoom_select_put_sim(const fude_zoom_clip* _c, const fude_zoom_sim* _move, const fude_zoom_select_mirror* _mirror, b8* _flip) {
    *_flip = false;
    if(_mirror == NULL) {
        return fude_zoom_sim_compose(*_move, _c->on_screen);
    }
    const f64 _th = atan2(_c->on_screen.b, _c->on_screen.a), _k = hypot(_c->on_screen.a, _c->on_screen.b);
    const f64 _ca = cos(2.0 * _mirror->angle), _sa = sin(2.0 * _mirror->angle);
    // The reflection of a point (of the screen).
    #define FUDE_ZOOM_REFLECT(_p) ((fude_zoom_v2){ _mirror->at.x + _ca * ((_p).x - _mirror->at.x) + _sa * ((_p).y - _mirror->at.y), \
                                                    _mirror->at.y + _sa * ((_p).x - _mirror->at.x) - _ca * ((_p).y - _mirror->at.y) })
    const fude_zoom_v2 _t = FUDE_ZOOM_REFLECT(((fude_zoom_v2){ _c->on_screen.tx, _c->on_screen.ty }));
    const b8 _box = _c->look.kind == FUDE_ZOOM_KIND_TEXT || _c->look.kind == FUDE_ZOOM_KIND_IMAGE;
    if(!_box) {
        *_flip = true;
        const f64 _r = 2.0 * _mirror->angle - _th;
        return (fude_zoom_sim){ _k * cos(_r), _k * sin(_r), _t.x, _t.y };
    }
    // The right way round: its across turned back (its top left the reflection of its top right) or its up (of its
    // bottom left) — whichever leaves it turned as it was.
    const f64 _ra = 2.0 * _mirror->angle - _th + 3.14159265358979323846, _rb = 2.0 * _mirror->angle - _th;
    const b8  _across = fabs(fude_zoom_select_wrap(_ra - _th)) <= fabs(fude_zoom_select_wrap(_rb - _th));
    const f64 _r = _across ? _ra : _rb;
    fude_zoom_v2 _at = _t;
    if(_c->look.kind == FUDE_ZOOM_KIND_TEXT && fude_zoom_clip_size(_c) >= 3u * sizeof(f64)) {
        f64 _w, _h;
        memcpy(&_w, fude_zoom_clip_data(_c) + sizeof(f64), sizeof(f64));
        memcpy(&_h, fude_zoom_clip_data(_c) + 2u * sizeof(f64), sizeof(f64));
        const fude_zoom_v2 _corner = fude_zoom_sim_apply(_c->on_screen, _across ? (fude_zoom_v2){ _w, 0.0 } : (fude_zoom_v2){ 0.0, -_h });
        _at = FUDE_ZOOM_REFLECT(_corner);
    }
    #undef FUDE_ZOOM_REFLECT
    return (fude_zoom_sim){ _k * cos(_r), _k * sin(_r), _at.x, _at.y };
}

u32 fude_zoom_select_put(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_clip* _clips, u32 _n, const fude_zoom_sim* _moves, u32 _m,
                         const fude_zoom_select_mirror* _mirror, const u32* _dead, u32 _dead_n) {
    if(_n == 0) {
        return 0u;
    }
    // Each copy as it looked on screen, moved, into the camera's frame (the same size on screen, whatever the zoom now).
    const u32           _frame   = _s->camera.frame;
    const fude_zoom_sim _from_sc = fude_zoom_sim_inverse(fude_zoom_camera_sim(&_s->camera));
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _born = rde_arr_new(sizeof(u32), _heap);
    rde_arr _num  = rde_arr_new(sizeof(f64), _heap);
    const u32 _times = _mirror != NULL ? 1u : _m;
    // Each copy's things' ids, by their originals': a dimension copied with what it measures is joined to the copies.
    rde_arr TYPE(fude_zoom_id) _id_arr = rde_arr_new(sizeof(fude_zoom_id), _heap);
    fude_zoom_id* _was_id = (fude_zoom_id*)rde_arr_add_n(&_id_arr, 2u * _n);   // (sized once: it stays put)
    fude_zoom_id* _now_id = _was_id + _n;
    for(u32 _k = 0; _k < _times; _k++) {
        memset(_now_id, 0, (usize)_n * sizeof(fude_zoom_id));
        for(u32 _j = 0; _j < 2u * _n; _j++) {
            // (dimensions after the rest: what they are joined to made first)
            const u32 _i = _j % _n;
            const fude_zoom_clip* _c = &_clips[_i];
            const b8 _dim   = _c->look.kind == FUDE_ZOOM_KIND_SHAPE && (_c->look.channels == FUDE_ZOOM_SHAPE_DIMENSION || _c->look.channels == FUDE_ZOOM_SHAPE_RADIAL);
            const b8 _props = _c->look.kind == FUDE_ZOOM_KIND_SHAPE && _c->look.channels == FUDE_ZOOM_SHAPE_PROPS;
            _was_id[_i] = _c->look.id;
            if(_c->look.kind == FUDE_ZOOM_KIND_MARK || (_dim || _props) != (_j >= _n)) {
                continue;
            }
            b8 _flip = false;
            const fude_zoom_sim   _m2 = fude_zoom_sim_compose(_from_sc, fude_zoom_select_put_sim(_c, _mirror != NULL ? NULL : &_moves[_k], _mirror, &_flip));
            const fude_zoom_place _at = { { _m2.tx, _m2.ty }, atan2(_m2.b, _m2.a), hypot(_m2.a, _m2.b) };
            u32 _o = FUDE_ZOOM_NONE;
            if(_c->look.kind == FUDE_ZOOM_KIND_IMAGE && fude_zoom_clip_size(_c) > 2u * sizeof(f64)) {
                f64 _hw, _hh;
                memcpy(&_hw, fude_zoom_clip_data(_c), sizeof(f64));
                memcpy(&_hh, fude_zoom_clip_data(_c) + sizeof(f64), sizeof(f64));
                _o = fude_zoom_scene_add_image(_s, _frame, _at, _hw, _hh, fude_zoom_clip_data(_c) + 2u * sizeof(f64), fude_zoom_clip_size(_c) - 2u * (u32)sizeof(f64), 0);
            } else if(_c->look.kind == FUDE_ZOOM_KIND_TEXT && fude_zoom_clip_size(_c) >= 3u * sizeof(f64) + sizeof(u32)) {
                f64 _size, _w, _h;
                u32 _style;
                memcpy(&_size, fude_zoom_clip_data(_c), sizeof(f64));
                memcpy(&_w, fude_zoom_clip_data(_c) + sizeof(f64), sizeof(f64));
                memcpy(&_h, fude_zoom_clip_data(_c) + 2u * sizeof(f64), sizeof(f64));
                memcpy(&_style, fude_zoom_clip_data(_c) + 3u * sizeof(f64), sizeof(u32));
                const u32 _head = 3u * (u32)sizeof(f64) + (u32)sizeof(u32);
                _o = fude_zoom_scene_add_text(_s, _frame, _at, _size, _w, _h, (u8)_style, _c->look.color, _c->look.fill,
                                              (const c8*)(fude_zoom_clip_data(_c) + _head), fude_zoom_clip_size(_c) - _head, 0);
            } else if(_c->look.kind == FUDE_ZOOM_KIND_SHAPE) {
                const u32 _count = fude_zoom_clip_size(_c) / (u32)sizeof(f64);
                rde_arr_clear(&_num);
                f64* _nn = rde_arr_add_n(&_num, _count > 0u ? _count : 1u);
                memcpy(_nn, fude_zoom_clip_data(_c), (usize)_count * sizeof(f64));
                if(_flip) {
                    fude_zoom_shape_mirror(_c->look.channels, _nn, _count);
                }
                u32 _cnt = _count;
                if(_props) {
                    // On the copy of its thing (none of it made: not put).
                    fude_zoom_id _to = 0;
                    const fude_zoom_id _id = _cnt >= 3u ? fude_zoom_id_get(&_nn[1]) : 0u;
                    for(u32 _q = 0; _q < _n && _id != 0; _q++) {
                        _to = _was_id[_q] == _id ? _now_id[_q] : _to;
                    }
                    if(_to == 0u) {
                        continue;
                    }
                    fude_zoom_id_put(&_nn[1], _to);
                }
                if(_dim) {
                    // Joined to the copies of what it measures, else to nothing (a copy never follows the originals).
                    const u32 _at_id[2] = { _c->look.channels == FUDE_ZOOM_SHAPE_RADIAL ? 3u : 4u, _c->look.channels == FUDE_ZOOM_SHAPE_RADIAL ? FUDE_ZOOM_NONE : 6u };
                    for(u32 _e = 0; _e < 2u; _e++) {
                        if(_at_id[_e] == FUDE_ZOOM_NONE || _at_id[_e] >= _cnt) {
                            continue;
                        }
                        fude_zoom_id _id, _to = 0;
                        memcpy(&_id, &_nn[_at_id[_e]], sizeof(fude_zoom_id));
                        for(u32 _q = 0; _q < _n && _id != 0; _q++) {
                            _to = _was_id[_q] == _id ? _now_id[_q] : _to;
                        }
                        memcpy(&_nn[_at_id[_e]], &_to, sizeof(fude_zoom_id));
                    }
                }
                _s->style = _c->look.channels == FUDE_ZOOM_SHAPE_SYMBOL ? 0 : _c->look.q;   // (its line's style, as it was)
                _o = fude_zoom_scene_add_shape_fill(_s, _frame, _at, _c->look.channels, _nn, _cnt, _c->look.color, _c->look.radius,
                                                    (u8)(_c->look.flags & (FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN)), _c->look.fill, 0);
                _s->style = 0;
            } else if(_c->look.kind == FUDE_ZOOM_KIND_STROKE || _c->look.kind == FUDE_ZOOM_KIND_FILL) {
                rde_arr_clear(&_sel->q);
                rde_arr_add_n(&_sel->q, _c->look.count);
                fude_zoom_qpoint* _qp = (fude_zoom_qpoint*)_sel->q.memory;
                if(!fude_zoom_codec_decode(fude_zoom_clip_data(_c), fude_zoom_clip_size(_c), _qp, _c->look.count, _c->look.channels)) {
                    continue;
                }
                if(_flip) {
                    for(u32 _q = 0; _q < _c->look.count; _q++) {
                        _qp[_q].y = -_qp[_q].y;
                    }
                }
                if(_c->look.kind == FUDE_ZOOM_KIND_FILL) {
                    _o = fude_zoom_scene_add_fill_flags(_s, _frame, _at, _c->look.q, _qp, _c->look.count, _c->look.color, _c->look.flags, 0);
                } else {
                    const u8 _flags = (u8)(_c->look.flags & (FUDE_ZOOM_FLAG_FROM_PEN | FUDE_ZOOM_FLAG_MARKER | FUDE_ZOOM_FLAG_PRESSURE));
                    _o = fude_zoom_scene_add_stroke_at(_s, _frame, _at, _c->look.q, _qp, _c->look.count, _c->look.channels, _c->look.color, _c->look.radius, _flags, 0, 0);
                }
            }
            if(_o != FUDE_ZOOM_NONE) {
                rde_arr_add(&_born, (any)&_o);
                if(!_props) {
                    fude_zoom_select_add(_sel, _o);   // (a property is not held: it goes with its thing)
                }
                _now_id[_i] = fude_zoom_scene_object(_s, _o)->id;
            }
        }
    }
    rde_arr_free(&_id_arr);
    // The originals let go with it (turned over where they were), all in one step.
    fude_zoom_box _box = fude_zoom_box_empty();
    for(u32 _i = 0; _i < _dead_n; _i++) {
        _box = fude_zoom_box_union(_box, fude_zoom_sim_box(fude_zoom_scene_sim(_s, fude_zoom_scene_object(_s, _dead[_i])->frame, _frame), fude_zoom_scene_object(_s, _dead[_i])->box));
        fude_zoom_scene_set_alive(_s, _dead[_i], false);
    }
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_born); _i++) {
        _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, ((const u32*)_born.memory)[_i])->box);
    }
    const u32 _made = (u32)rde_arr_length(&_born);
    if(_made > 0u || _dead_n > 0u) {
        fude_zoom_history_push(_s, _frame, _box, _dead, _dead_n, (const u32*)_born.memory, _made);
    }
    rde_arr_free(&_born);
    rde_arr_free(&_num);
    fude_zoom_select_update(_sel, _s);
    return _made;
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

// --- arranging ---------------------------------------------------------------------------------

typedef struct {
    u32           pick;
    fude_zoom_box box;    // on the screen
} fude_zoom_arranged;

RDE_INTERNAL int fude_zoom_arranged_by_x(const void* _a, const void* _b) {
    const f64 _x = ((const fude_zoom_arranged*)_a)->box.min_x + ((const fude_zoom_arranged*)_a)->box.max_x;
    const f64 _y = ((const fude_zoom_arranged*)_b)->box.min_x + ((const fude_zoom_arranged*)_b)->box.max_x;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

RDE_INTERNAL int fude_zoom_arranged_by_y(const void* _a, const void* _b) {
    const f64 _x = ((const fude_zoom_arranged*)_a)->box.min_y + ((const fude_zoom_arranged*)_a)->box.max_y;
    const f64 _y = ((const fude_zoom_arranged*)_b)->box.min_y + ((const fude_zoom_arranged*)_b)->box.max_y;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

b8 fude_zoom_select_arrange(fude_zoom_selection* _sel, fude_zoom_scene* _s, FUDE_ZOOM_ARRANGE_ _how) {
    const u32 _n = fude_zoom_pick_count(_sel);
    if(_n < 2u || fude_zoom_select_busy(_sel) || ((_how == FUDE_ZOOM_ARRANGE_ACROSS || _how == FUDE_ZOOM_ARRANGE_DOWN) && _n < 3u)) {
        return false;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr TYPE(fude_zoom_arranged) _a_arr = rde_arr_new(sizeof(fude_zoom_arranged), _heap);
    fude_zoom_arranged* _a = (fude_zoom_arranged*)rde_arr_add_n(&_a_arr, _n);   // (sized once: it stays put)
    fude_zoom_box _all = fude_zoom_box_empty();
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_pick* _p = &fude_zoom_picks(_sel)[_i];
        _a[_i].pick = _i;
        _a[_i].box  = fude_zoom_sim_box(fude_zoom_select_to_screen(_s, fude_zoom_pick_frame(_s, _p)), fude_zoom_scene_object(_s, _p->object)->box);
        _all = fude_zoom_box_union(_all, _a[_i].box);
    }
    // Each one's move on the screen.
    rde_arr TYPE(fude_zoom_v2) _d_arr = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr_resize(&_d_arr, _n);
    fude_zoom_v2* _d = (fude_zoom_v2*)_d_arr.memory;
    if(_how == FUDE_ZOOM_ARRANGE_ACROSS || _how == FUDE_ZOOM_ARRANGE_DOWN) {
        const b8 _x = _how == FUDE_ZOOM_ARRANGE_ACROSS;
        qsort(_a, _n, sizeof(_a[0]), _x ? fude_zoom_arranged_by_x : fude_zoom_arranged_by_y);
        f64 _sizes = 0.0;
        for(u32 _i = 0; _i < _n; _i++) {
            _sizes += _x ? _a[_i].box.max_x - _a[_i].box.min_x : _a[_i].box.max_y - _a[_i].box.min_y;
        }
        const f64 _gap = ((_x ? _all.max_x - _all.min_x : _all.max_y - _all.min_y) - _sizes) / (f64)(_n - 1u);
        f64 _at = _x ? _all.min_x : _all.min_y;
        for(u32 _i = 0; _i < _n; _i++) {
            const f64 _lo = _x ? _a[_i].box.min_x : _a[_i].box.min_y, _hi = _x ? _a[_i].box.max_x : _a[_i].box.max_y;
            if(_x) { _d[_a[_i].pick].x = _at - _lo; } else { _d[_a[_i].pick].y = _at - _lo; }
            _at += (_hi - _lo) + _gap;
        }
    } else {
        for(u32 _i = 0; _i < _n; _i++) {
            const fude_zoom_box _b = _a[_i].box;
            fude_zoom_v2* _m = &_d[_a[_i].pick];
            switch(_how) {
                case FUDE_ZOOM_ARRANGE_LEFT:   _m->x = _all.min_x - _b.min_x; break;
                case FUDE_ZOOM_ARRANGE_RIGHT:  _m->x = _all.max_x - _b.max_x; break;
                case FUDE_ZOOM_ARRANGE_CENTRE: _m->x = (_all.min_x + _all.max_x - _b.min_x - _b.max_x) * 0.5; break;
                case FUDE_ZOOM_ARRANGE_TOP:    _m->y = _all.max_y - _b.max_y; break;
                case FUDE_ZOOM_ARRANGE_BOTTOM: _m->y = _all.min_y - _b.min_y; break;
                case FUDE_ZOOM_ARRANGE_MIDDLE: _m->y = (_all.min_y + _all.max_y - _b.min_y - _b.max_y) * 0.5; break;
                default: break;
            }
        }
    }
    // Into each one's frame, as a slide of its place; the connectors after them.
    rde_arr _ids = rde_arr_new(sizeof(u32), _heap), _before = rde_arr_new(sizeof(fude_zoom_place), _heap), _after = rde_arr_new(sizeof(fude_zoom_place), _heap);
    for(u32 _i = 0; _i < _n; _i++) {
        if(fabs(_d[_i].x) + fabs(_d[_i].y) < 1e-9) {
            continue;
        }
        const fude_zoom_pick* _p  = &fude_zoom_picks(_sel)[_i];
        const fude_zoom_sim   _to = fude_zoom_select_to_screen(_s, fude_zoom_pick_frame(_s, _p));
        const f64 _k2 = _to.a * _to.a + _to.b * _to.b;
        const fude_zoom_v2 _f = { (_to.a * _d[_i].x + _to.b * _d[_i].y) / _k2, (-_to.b * _d[_i].x + _to.a * _d[_i].y) / _k2 };
        const fude_zoom_place _was = fude_zoom_scene_place_of(_s, _p->object);
        const fude_zoom_place _now = fude_zoom_place_moved(_was, (fude_zoom_sim){ 1.0, 0.0, _f.x, _f.y });
        fude_zoom_scene_set_place(_s, _p->object, _now);
        rde_arr_add(&_ids, (any)&_p->object);
        rde_arr_add(&_before, (any)&_was);
        rde_arr_add(&_after, (any)&_now);
    }
    const u32 _moved = (u32)rde_arr_length(&_ids);
    if(_moved > 0) {
        rde_arr TYPE(u32) _just = rde_arr_new(sizeof(u32), _heap);
        memcpy(rde_arr_add_n(&_just, _moved), _ids.memory, (usize)_moved * sizeof(u32));
        fude_zoom_connect_follow(_s, _s->camera.frame, (const u32*)_just.memory, _moved, &_ids, &_before, &_after);
        rde_arr_free(&_just);
        const fude_zoom_box _view = fude_zoom_sim_box(fude_zoom_sim_inverse(fude_zoom_camera_sim(&_s->camera)), _all);
        fude_zoom_select_push_moved(_s, _s->camera.frame, _view, (const u32*)_ids.memory, (const fude_zoom_place*)_before.memory,
                                     (const fude_zoom_place*)_after.memory, (u32)rde_arr_length(&_ids));
    }
    rde_arr_free(&_ids);
    rde_arr_free(&_before);
    rde_arr_free(&_after);
    rde_arr_free(&_a_arr);
    rde_arr_free(&_d_arr);
    fude_zoom_select_update(_sel, _s);
    return _moved > 0;
}
