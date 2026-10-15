// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/select.h"
#include "zoom/connect.h"
#include "zoom/shape.h"
#include "zoom/circuit.h"
#include "zoom/props.h"
#include "drawing/base/theme.h"

// ===========================================================================
// See select.h.
//
// Laid out as: the selection's state and its box on screen, the loop (what it takes), the pen (a press, a move, a
// lift, and the drag committed), drawing, and the commands (delete, to a layer, copy and paste, Repeat's put,
// arranging). Everything is worked on the screen, where the pen is: a drag is one similarity on screen (drag), and
// only when it is committed is it carried into each pick's own frame. The few names not starting fude_zoom_select_
// are internal: fude_zoom_picks, fzsel_clip_add. What is inside the loop is rde_math's even-odd test, in f64.
// ===========================================================================

#define FUDE_ZOOM_SELECT_LOOP_MAX 160u   // a loop's points kept for the inside test (a long one thinned)
#define FUDE_ZOOM_SELECT_SAMPLES  48u    // a stroke's points tested, at most

// Returns the picks as an array.
RDE_INTERNAL fude_zoom_pick* fude_zoom_picks(const fude_zoom_selection* _sel) {
    return (fude_zoom_pick*)_sel->picks.memory;
}

// Returns how many things are picked.
RDE_INTERNAL u32 fude_zoom_pick_count(const fude_zoom_selection* _sel) {
    return (u32)rde_arr_length(&_sel->picks);
}

// See the header.
void fude_zoom_select_init(fude_zoom_selection* _sel) {
    rde_memset(_sel, 0, sizeof(*_sel));
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

// Empties the clipboard, freeing each clip's bytes.
RDE_INTERNAL void fude_zoom_select_clip_clear(fude_zoom_selection* _sel) {
    fude_zoom_select_clips_free(&_sel->clip);
}

// See the header. Safe on a selection never made (zeroed).
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
    rde_memset(_sel, 0, sizeof(*_sel));
}

RDE_INTERNAL void fude_zoom_select_hold(fude_zoom_selection* _sel, const fude_zoom_scene* _s);
RDE_INTERNAL void fude_zoom_select_commit(fude_zoom_selection* _sel, fude_zoom_scene* _s);

// Puts every object back on the canvas: none is drawn by the selection.
RDE_INTERNAL void fude_zoom_select_unlift(fude_zoom_selection* _sel) {
    if(rde_arr_length(&_sel->lifted) > 0) {
        rde_memset(_sel->lifted.memory, 0, rde_arr_length(&_sel->lifted));
    }
}

// See the header. A loop or a drag under way is dropped too.
void fude_zoom_select_clear(fude_zoom_selection* _sel) {
    fude_zoom_select_unlift(_sel);
    rde_arr_clear(&_sel->picks);
    rde_arr_clear(&_sel->loop);
    _sel->grab = FUDE_ZOOM_GRAB_NONE;
    _sel->box  = fude_zoom_box_empty();
}

// See the header.
b8 fude_zoom_select_any(const fude_zoom_selection* _sel) {
    return fude_zoom_pick_count(_sel) > 0;
}

// See the header.
b8 fude_zoom_select_busy(const fude_zoom_selection* _sel) {
    return _sel->grab != FUDE_ZOOM_GRAB_NONE;
}

// --- where things are on screen -------------------------------------------------------------

// Returns a frame's units to the screen, with the camera as it is now.
RDE_INTERNAL fude_zoom_sim fude_zoom_select_to_screen(const fude_zoom_scene* _s, u32 _frame) {
    return fude_zoom_sim_compose(fude_zoom_camera_sim(&_s->camera), fude_zoom_scene_sim(_s, _frame, _s->camera.frame));
}

// Returns the frame a pick lives in (a FRAME object's: its parent).
RDE_INTERNAL u32 fude_zoom_pick_frame(const fude_zoom_scene* _s, const fude_zoom_pick* _p) {
    return fude_zoom_scene_object(_s, _p->object)->frame;
}

// See the header. Picks are dropped only while nothing is dragged, so a drag keeps its picks in step with before. The
// box follows the drag; a guide counts only for its part on the screen (half set), since it reaches far past it.
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
        rde_arr_resize(&_sel->picks, _w);
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
                    _sel->box = fude_zoom_box_union(_sel->box, (fude_zoom_box){ rde_math_min_f64(_a.x, _b.x), rde_math_min_f64(_a.y, _b.y), rde_math_max_f64(_a.x, _b.x), rde_math_max_f64(_a.y, _b.y) });
                }
            }
            rde_arr_free(&_pts);
            continue;
        }
        _sel->box = fude_zoom_box_union(_sel->box, fude_zoom_sim_box(_to, _o->box));
    }
}

// See the header. Padded by FUDE_ZOOM_SELECT_PAD.
b8 fude_zoom_select_box(const fude_zoom_selection* _sel, fude_zoom_box* _out) {
    if(!fude_zoom_select_any(_sel) || fude_zoom_box_is_empty(_sel->box)) {
        return false;
    }
    *_out = fude_zoom_box_grow(_sel->box, FUDE_ZOOM_SELECT_PAD);
    return true;
}

// Pushes things moved as one undo step: the circuit's wires joined to them (circuit.h) made again to their pins where
// they are now, in the same step.
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

// Returns whether frame _f is _ancestor itself, or inside it at any depth (a chain of 100000 frames at most is
// followed).
RDE_INTERNAL b8 fude_zoom_within(const fude_zoom_scene* _s, u32 _f, u32 _ancestor) {
    for(u32 _k = 0; _f != FUDE_ZOOM_NONE && _k < 100000u; _k++) {
        if(_f == _ancestor) {
            return true;
        }
        _f = fude_zoom_scene_frame(_s, _f)->parent;
    }
    return false;
}

// The objects picked now, as a set for fude_zoom_select_add to look in (a lasso over thousands of strokes would
// otherwise go through every pick for each one: O(n²)). Made on the std allocator; the caller frees it, and adds to
// the picks only through fude_zoom_select_add while it is used, which keeps the two in step.
RDE_INTERNAL rde_hash_set fude_zoom_select_picked(const fude_zoom_selection* _sel) {
    rde_hash_set _set = rde_hash_set_new(sizeof(u32), rde_hash_map_fn_u32_hash, rde_hash_map_fn_u32_cmp, NULL, rde_memory_allocator_get_default_std());
    for(u32 _i = 0; _i < fude_zoom_pick_count(_sel); _i++) {
        rde_hash_set_add(&_set, (const any)&fude_zoom_picks(_sel)[_i].object);
    }
    return _set;
}

// Picks _object, unless it is picked already (in _picked, fude_zoom_select_picked's set, which it is added to).
RDE_INTERNAL void fude_zoom_select_add(fude_zoom_selection* _sel, rde_hash_set* _picked, u32 _object) {
    if(!rde_hash_set_add(_picked, (const any)&_object)) {
        return;
    }
    const fude_zoom_pick _p = { _object };
    rde_arr_add(&_sel->picks, (any)&_p);
}

// Picks what the loop (_poly, _n points on the screen, 3 at least unless _all) takes, in the frames the last draw went
// into at the camera's depth and below (_all: everything there, the loop ignored), by select.h's rules. A frame taken
// whole is picked as its FRAME object, and the frames inside it are passed over.
RDE_INTERNAL void fude_zoom_select_take(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_renderer* _r, const rde_vec_2D* _poly, u32 _n, b8 _all) {
    rde_arr _frames = rde_arr_new(sizeof(fude_zoom_visible), rde_memory_allocator_get_default_std());
    fude_zoom_render_editable(_r, _s, &_frames);
    rde_arr _whole = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_hash_set _picked = fude_zoom_select_picked(_sel);
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
                fude_zoom_select_add(_sel, &_picked, _object);
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
                    _in += rde_math_point_in_polygon_2d_f64(_at, _poly, _n) ? 1u : 0u;
                }
                if(_in >= 3u) {
                    fude_zoom_select_add(_sel, &_picked, _object);
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
                    _in += rde_math_point_in_polygon_2d_f64(_at, _poly, _n) ? 1u : 0u;
                }
                if(_in * 2u > (u32)rde_arr_length(&_pts)) {
                    fude_zoom_select_add(_sel, &_picked, _object);
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
                _in += rde_math_point_in_polygon_2d_f64(_at, _poly, _n) ? 1u : 0u;
                _tested++;
            }
            if(_in * 2u > _tested) {
                fude_zoom_select_add(_sel, &_picked, _object);
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
                    _in = rde_math_point_in_polygon_2d_f64(_at, _poly, _n);
                }
            }
            if(_in) {
                fude_zoom_select_add(_sel, &_picked, _object);
                rde_arr_add(&_whole, (any)&_o->child);
            }
        }
    }
    rde_arr_free(&_frames);
    rde_arr_free(&_whole);
    rde_hash_set_free(&_picked);
}

// --- the pen -----------------------------------------------------------------------------------

// Returns the handle under _p (screen): a corner (its index 0 to 3: min-min, max-min, min-max, max-max), the knob
// (4), inside the box (5); -1: none. A handle within FUDE_ZOOM_SELECT_GRAB is taken before the box.
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
        if(rde_math_hypot_f64(_p.x - _c[_i].x, _p.y - _c[_i].y) <= FUDE_ZOOM_SELECT_GRAB) {
            return _i;
        }
    }
    return fude_zoom_box_contains(_b, (fude_zoom_v2){ _p.x, _p.y }) ? 5 : -1;
}

// See the header.
i32 fude_zoom_select_handle_at(const fude_zoom_selection* _sel, rde_vec_2F _screen) {
    return fude_zoom_select_any(_sel) ? fude_zoom_select_handle(_sel, _screen) : -1;
}

// See the header. A drag's pivot: a turn goes round the box's middle, a scale round the corner opposite the one taken.
void fude_zoom_select_down(fude_zoom_selection* _sel, fude_zoom_scene* _s, rde_vec_2F _screen) {
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

// Keeps where each pick was (before), and lifts each off the canvas while it is dragged (lifted grows to the scene's
// object count).
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

// See the header. Nothing when nothing is selected or the pen is at it.
void fude_zoom_select_begin(fude_zoom_selection* _sel, fude_zoom_scene* _s) {
    if(!fude_zoom_select_any(_sel) || fude_zoom_select_busy(_sel)) {
        return;
    }
    _sel->grab = FUDE_ZOOM_GRAB_MOVE;
    _sel->drag = fude_zoom_sim_identity();
    fude_zoom_select_hold(_sel, _s);
}

// See the header. Only after fude_zoom_select_begin (or during a move).
void fude_zoom_select_end(fude_zoom_selection* _sel, fude_zoom_scene* _s, fude_zoom_sim _drag) {
    if(_sel->grab != FUDE_ZOOM_GRAB_MOVE) {
        return;
    }
    _sel->drag = _drag;
    fude_zoom_select_update(_sel, _s);   // (its box where it goes: what the undo step shows)
    fude_zoom_select_commit(_sel, _s);
}

// See the header. What is lifted already (a pick) is not added twice.
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

// See the header.
void fude_zoom_select_keep(fude_zoom_selection* _sel, const fude_zoom_scene* _s, u32 _n) {
    if(_n < fude_zoom_pick_count(_sel)) {
        rde_arr_resize(&_sel->picks, _n);
        fude_zoom_select_update(_sel, _s);
    }
}

// See the header. The loop takes a point only 2 points (screen) from its last. A drag is worked out afresh from where
// the pen took hold: a move by the pen's way, a scale by how far it is from the pivot now over then (never below
// 0.02), a turn by its angle round the pivot.
void fude_zoom_select_moved(fude_zoom_selection* _sel, rde_vec_2F _screen) {
    if(_sel->grab == FUDE_ZOOM_GRAB_LOOP) {
        const rde_vec_2F* _last = &((const rde_vec_2F*)_sel->loop.memory)[rde_arr_length(&_sel->loop) - 1u];
        if(rde_math_abs_f32(_screen.x - _last->x) + rde_math_abs_f32(_screen.y - _last->y) >= 2.0f) {
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
            const f64 _was = rde_math_hypot_f64(_sel->grab_at.x - _px, _sel->grab_at.y - _py);
            const f64 _now = rde_math_hypot_f64(_screen.x - _px, _screen.y - _py);
            _a = _was > 1.0 ? rde_math_max_f64(_now / _was, 0.02) : 1.0;
        } else {
            f64 _turn = rde_math_atan2_f64(_screen.y - _py, _screen.x - _px) - rde_math_atan2_f64(_sel->grab_at.y - _py, _sel->grab_at.x - _px);
            while(_turn > RDE_PI)   { _turn -= 2.0 * RDE_PI; }
            while(_turn <= -RDE_PI) { _turn += 2.0 * RDE_PI; }
            // Resting a moment at each 15° (within 1.5° of it), as the instruments do.
            const f64 _step = RDE_PI / 12.0;
            const f64 _near = rde_math_round_f64(_turn / _step) * _step;
            _turn = rde_math_abs_f64(_turn - _near) < 0.026 ? _near : _turn;
            _sel->turn = _turn;
            _a = rde_math_cos_f64(_turn);
            _b = rde_math_sin_f64(_turn);
        }
        // Round the pivot: p' = pivot + R·s·(p − pivot).
        _sel->drag = (fude_zoom_sim){ _a, _b, _px - (_a * _px - _b * _py), _py - (_b * _px + _a * _py) };
    }
}

// See the header. The angle is said 34 points beyond where the knob was taken, turned with it.
b8 fude_zoom_select_turning(const fude_zoom_selection* _sel, f64* _degrees, rde_vec_2F* _at) {
    if(_sel->grab != FUDE_ZOOM_GRAB_ROTATE) {
        return false;
    }
    *_degrees = _sel->turn * 180.0 / RDE_PI;
    // Over the knob as it was taken (the box above the pivot), turned with it.
    const f64 _dx = _sel->grab_at.x - _sel->pivot.x, _dy = _sel->grab_at.y - _sel->pivot.y;
    const f64 _len = rde_math_hypot_f64(_dx, _dy), _c = rde_math_cos_f64(_sel->turn), _s = rde_math_sin_f64(_sel->turn);
    const f64 _out = _len > 1.0 ? (_len + 34.0) / _len : 1.0;
    *_at = (rde_vec_2F){ (f32)(_sel->pivot.x + (_dx * _c - _dy * _s) * _out), (f32)(_sel->pivot.y + (_dx * _s + _dy * _c) * _out) };
    return true;
}

// See the header. A loop of fewer than 3 points takes nothing; one of more than FUDE_ZOOM_SELECT_LOOP_MAX is thinned to
// that many, evenly.
void fude_zoom_select_up(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_renderer* _r) {
    if(_sel->grab == FUDE_ZOOM_GRAB_LOOP) {
        _sel->grab = FUDE_ZOOM_GRAB_NONE;
        const u32 _n = (u32)rde_arr_length(&_sel->loop);
        if(_n >= 3u) {
            // A long loop thinned for the test (its shape kept), in f64 (exactly the loop's f32 points) for the inside
            // test, which works out where the pick's points are in f64.
            rde_vec_2D _poly[FUDE_ZOOM_SELECT_LOOP_MAX];
            const u32  _keep = _n < FUDE_ZOOM_SELECT_LOOP_MAX ? _n : FUDE_ZOOM_SELECT_LOOP_MAX;
            for(u32 _i = 0; _i < _keep; _i++) {
                const rde_vec_2F _q = ((const rde_vec_2F*)_sel->loop.memory)[(u64)_i * _n / _keep];
                _poly[_i] = (rde_vec_2D){ (f64)_q.x, (f64)_q.y };
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

// See the header.
void fude_zoom_select_abort(fude_zoom_selection* _sel, const fude_zoom_scene* _s) {
    if(_sel->grab == FUDE_ZOOM_GRAB_NONE) {
        return;
    }
    if(_sel->grab == FUDE_ZOOM_GRAB_LOOP) {
        rde_arr_clear(&_sel->loop);
    } else {
        fude_zoom_select_unlift(_sel);   // (the drag only ever shown: the picks never left their places)
    }
    _sel->grab = FUDE_ZOOM_GRAB_NONE;
    _sel->drag = fude_zoom_sim_identity();
    fude_zoom_select_update(_sel, _s);
}

// Writes the frames the _n objects of _ids are in into _out (an rde_arr of u32, cleared first), each once, by slot:
// those whose connectors follow what moved (fude_zoom_connect_follow works one frame at a time).
RDE_INTERNAL void fude_zoom_select_frames_of(const fude_zoom_scene* _s, const u32* _ids, u32 _n, rde_arr* _out) {
    rde_arr_clear(_out);
    for(u32 _i = 0; _i < _n; _i++) {
        rde_arr_add(_out, (any)&fude_zoom_scene_object(_s, _ids[_i])->frame);
    }
    rde_arr_sort(_out, fude_zoom_by_u32);
    u32* _f = (u32*)_out->memory;
    u32  _w = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(_out); _i++) {
        if(_w == 0u || _f[_w - 1u] != _f[_i]) {
            _f[_w++] = _f[_i];
        }
    }
    rde_arr_resize(_out, _w);
}

// Where the pen lifted, the canvas takes it back: each pick's place moved by the drag (seen in its own frame's units),
// one undo step, with the connectors in every frame the picks are in following. A drag that moved nothing makes no
// step.
RDE_INTERNAL void fude_zoom_select_commit(fude_zoom_selection* _sel, fude_zoom_scene* _s) {
    const u32 _n = fude_zoom_pick_count(_sel);
    const b8  _moved = rde_math_abs_f64(_sel->drag.a - 1.0) > 1e-9 || rde_math_abs_f64(_sel->drag.b) > 1e-9 || rde_math_abs_f64(_sel->drag.tx) > 1e-6 || rde_math_abs_f64(_sel->drag.ty) > 1e-6;
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
        rde_memcpy(rde_arr_add_n(&_all_ids, _n), _ids, (usize)_n * sizeof(u32));
        rde_memcpy(rde_arr_add_n(&_all_before, _n), _sel->before.memory, (usize)_n * sizeof(fude_zoom_place));
        rde_memcpy(rde_arr_add_n(&_all_after, _n), _after, (usize)_n * sizeof(fude_zoom_place));
        rde_arr _frames = rde_arr_new(sizeof(u32), _heap);
        fude_zoom_select_frames_of(_s, _ids, _n, &_frames);
        for(u32 _k = 0; _k < (u32)rde_arr_length(&_frames); _k++) {
            fude_zoom_connect_follow(_s, ((const u32*)_frames.memory)[_k], _ids, _n, &_all_ids, &_all_before, &_all_after);
        }
        rde_arr_free(&_frames);
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

#define FUDE_ZOOM_SELECT_DASHES 4096u   // a side's dashes on the screen at most: a stretch no screen has draws none

// Draws a side of the box, dashed, from _a to _b (screen units, round the middle): only the stretch on screen and a
// margin (zoom.h's fude_zoom_clip_line), since deep in a side runs to millions of points and each dash is a line
// drawn. Its dashes keep their places counted from _a, so they stand still as the page moves; they are placed from
// where the stretch starts and counted by index, because a place millions of points along the side would not move
// when a period is added to it (the loop would never end) and would put the dashes off by its rounding.
RDE_INTERNAL void fude_zoom_select_side(fude_zoom_v2 _a, fude_zoom_v2 _b, fude_zoom_v2 _half, rde_color _color) {
    const f64 _dash = 6.0, _period = 10.0, _margin = 8.0;
    fude_zoom_v2 _p = _a, _q = _b;
    if(!fude_zoom_clip_line(&_p, &_q, (fude_zoom_box){ -_half.x - _margin, -_half.y - _margin, _half.x + _margin, _half.y + _margin })) {
        return;
    }
    const f64 _span = rde_math_hypot_f64(_q.x - _p.x, _q.y - _p.y);   // (the stretch's length)
    if(!(_span > 0.0)) {
        return;
    }
    // How far into a period the stretch starts (its distance from _a, a period at a time), then its dashes from the
    // start of that period: the first may begin before the stretch, in the margin.
    const f64 _phase  = rde_math_fmod_f64(rde_math_hypot_f64(_p.x - _a.x, _p.y - _a.y), _period);
    const f64 _n      = rde_math_ceil_f64((_phase + _span) / _period);
    const u32 _dashes = _n > 0.0 && _n <= (f64)FUDE_ZOOM_SELECT_DASHES ? (u32)_n : 0u;
    const f64 _ux = (_q.x - _p.x) / _span, _uy = (_q.y - _p.y) / _span;
    for(u32 _i = 0; _i < _dashes; _i++) {
        const f64 _d = (f64)_i * _period - _phase;   // (from _p)
        const f64 _e = rde_math_min_f64(_d + _dash, _span);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)(_p.x + _ux * _d), (f32)(_p.y + _uy * _d) }, (rde_vec_2F){ (f32)(_p.x + _ux * _e), (f32)(_p.y + _uy * _e) }, _color, 1.2f);
    }
}

// See the header. While a drag is under way, what is lifted is drawn here, moved by it; frames get no glow.
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

// See the header.
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

// Adds to _todo what moving onto a layer takes, as (object, its pick or FUDE_ZOOM_NONE) pairs: _object itself, or
// everything inside it when it is a frame (64 frames deep at most). Dead things, bookmarks, layers' records and what a
// hidden or locked layer holds are left out.
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

// See the header. Nothing while the pen is at the selection. Things already on _layer stay as they are.
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
    // Each one copied onto the layer where it is, in its order (a stroke's first piece first, so its later pieces join
    // the copy). Connectors after.
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

// Appends a copy of object _object to _out, as it is on the screen now (not a frame, a mark, a layer, nothing gone).
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

// An object and where it is drawn, for putting objects in the order they are drawn (fude_zoom_select_drawn_order): the
// draw key (scene.h's fude_zoom_scene_draw_key) and index of it and of each FRAME object round it, a pair of u64s for
// each, from it up to the root's level, a run of a shared array.
typedef struct {
    u32        object;
    u32        from, count;   // its run in the shared array: its first pair, and how many pairs
    const u64* path;          // that run, set once every run is written (the array may move while they are)
} fude_zoom_select_drawn;

// rde_sort's order for fude_zoom_select_drawn: the order they are drawn in. Two runs are compared from the root's level
// down, pair by pair: by draw key, then (two at the same key) by index, the one made first first; the same pair is the
// same frame, so the next pair is compared inside it. An object inside a frame that is the other comes after it.
RDE_INTERNAL int fude_zoom_select_by_drawn(const any _a, const any _b) {
    const fude_zoom_select_drawn* _x = (const fude_zoom_select_drawn*)_a;
    const fude_zoom_select_drawn* _y = (const fude_zoom_select_drawn*)_b;
    for(u32 _i = 0; _i < _x->count && _i < _y->count; _i++) {
        const u64* _p = &_x->path[2u * (_x->count - 1u - _i)];
        const u64* _q = &_y->path[2u * (_y->count - 1u - _i)];
        if(_p[0] != _q[0]) {
            return _p[0] < _q[0] ? -1 : 1;
        }
        if(_p[1] != _q[1]) {
            return _p[1] < _q[1] ? -1 : 1;
        }
    }
    return _x->count < _y->count ? -1 : (_x->count > _y->count ? 1 : 0);
}

// Writes the _n objects of _objects that exist into _out (an rde_arr of u32, cleared first) in the order the renderer
// draws them, across frames too: within a frame by draw key, and a frame's things where its FRAME object is in its
// parent's order. So copies put down in that order lie over each other as the things did.
RDE_INTERNAL void fude_zoom_select_drawn_order(const fude_zoom_scene* _s, const u32* _objects, u32 _n, rde_arr* _out) {
    rde_arr_clear(_out);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _items = rde_arr_new(sizeof(fude_zoom_select_drawn), _heap), _paths = rde_arr_new(sizeof(u64), _heap);
    const u32 _count = fude_zoom_scene_object_count(_s);
    for(u32 _i = 0; _i < _n; _i++) {
        if(_objects[_i] >= _count) {
            continue;
        }
        fude_zoom_select_drawn _d = { _objects[_i], (u32)(rde_arr_length(&_paths) / 2u), 0u, NULL };
        // It, then the FRAME object of each frame round it, up to the root's level.
        u32 _o = _objects[_i];
        for(;;) {
            const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _o);
            const u64 _pair[2] = { fude_zoom_scene_draw_key(_s, _ob), (u64)_o };
            rde_memcpy(rde_arr_add_n(&_paths, 2u), _pair, sizeof(_pair));
            _d.count++;
            const u32 _up = _ob->frame != FUDE_ZOOM_NONE ? fude_zoom_scene_frame(_s, _ob->frame)->object : FUDE_ZOOM_NONE;
            if(_up == FUDE_ZOOM_NONE || _up >= _count) {
                break;
            }
            _o = _up;
        }
        rde_arr_add(&_items, (any)&_d);
    }
    fude_zoom_select_drawn* _it = (fude_zoom_select_drawn*)_items.memory;
    const u32 _m = (u32)rde_arr_length(&_items);
    for(u32 _i = 0; _i < _m; _i++) {
        _it[_i].path = &((const u64*)_paths.memory)[2u * _it[_i].from];
    }
    rde_arr_sort(&_items, fude_zoom_select_by_drawn);
    _it = (fude_zoom_select_drawn*)_items.memory;
    for(u32 _i = 0; _i < _m; _i++) {
        rde_arr_add(_out, (any)&_it[_i].object);
    }
    rde_arr_free(&_items);
    rde_arr_free(&_paths);
}

// See the header. The objects are copied in the order they are drawn (fude_zoom_select_drawn_order); the properties
// after them all.
void fude_zoom_select_clips_of(const fude_zoom_scene* _s, const u32* _objects, u32 _n, rde_arr* _out) {
    const u32 _count = fude_zoom_scene_object_count(_s);
    rde_arr _copied = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_copied, _count);
    rde_arr _order = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_select_drawn_order(_s, _objects, _n, &_order);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_order); _i++) {
        const u32 _o = ((const u32*)_order.memory)[_i];
        fzsel_clip_add(_s, _o, _out);
        ((u8*)_copied.memory)[_o] = 1u;
    }
    rde_arr_free(&_order);
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

// See the header. Room for one byte more than _size, so an empty clip still has an array.
void fude_zoom_clip_put(fude_zoom_clip* _c, const u8* _data, u32 _size) {
    _c->bytes = rde_arr_new_with_capacity(sizeof(u8), (usize)_size + 1u, rde_memory_allocator_get_default_std());
    if(_size > 0u) {
        rde_memcpy(rde_arr_add_n(&_c->bytes, _size), _data, _size);
    }
}

// See the header.
void fude_zoom_select_clips_free(rde_arr* _clips) {
    fude_zoom_clip* _c = (fude_zoom_clip*)_clips->memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(_clips); _i++) {
        rde_arr_free(&_c[_i].bytes);
    }
    rde_arr_clear(_clips);
}

// Appends copies of what is selected to _out (an rde_arr of fude_zoom_clip), as fude_zoom_select_clips_of makes them:
// in the order they are drawn, so a paste keeps what is over what (a loop's picks come in the order each frame's
// R-tree, index.h, returns them). Returns the selection's box's middle on the screen, where a paste of them centres.
RDE_INTERNAL fude_zoom_v2 fzsel_clips_of_picks(const fude_zoom_selection* _sel, const fude_zoom_scene* _s, rde_arr* _out) {
    rde_arr _objects = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    for(u32 _i = 0; _i < fude_zoom_pick_count(_sel); _i++) {
        rde_arr_add(&_objects, (any)&fude_zoom_picks(_sel)[_i].object);
    }
    fude_zoom_select_clips_of(_s, (const u32*)_objects.memory, (u32)rde_arr_length(&_objects), _out);
    rde_arr_free(&_objects);
    return (fude_zoom_v2){ (_sel->box.min_x + _sel->box.max_x) * 0.5, (_sel->box.min_y + _sel->box.max_y) * 0.5 };
}

// See the header.
void fude_zoom_select_copy(fude_zoom_selection* _sel, const fude_zoom_scene* _s) {
    if(!fude_zoom_select_any(_sel)) {
        return;
    }
    fude_zoom_select_clip_clear(_sel);
    _sel->clip_center = fzsel_clips_of_picks(_sel, _s, &_sel->clip);
}

// See the header.
b8 fude_zoom_select_can_paste(const fude_zoom_selection* _sel) {
    return rde_arr_length(&_sel->clip) > 0;
}

// See the header.
void fude_zoom_select_paste(fude_zoom_selection* _sel, fude_zoom_scene* _s, rde_vec_2F _screen) {
    fude_zoom_select_paste_clips(_sel, _s, (const fude_zoom_clip*)_sel->clip.memory, (u32)rde_arr_length(&_sel->clip), _sel->clip_center, _screen);
}

// See the header: fude_zoom_select_put with one move, from _center to _screen, after letting go of the selection.
void fude_zoom_select_paste_clips(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_clip* _clips, u32 _n, fude_zoom_v2 _center, rde_vec_2F _screen) {
    if(_n == 0) {
        return;
    }
    fude_zoom_select_clear(_sel);
    const fude_zoom_sim _shift = { 1.0, 0.0, (f64)_screen.x - _center.x, (f64)_screen.y - _center.y };
    fude_zoom_select_put(_sel, _s, _clips, _n, &_shift, 1u, NULL, NULL, 0u);
}

// Returns a turn's angle brought within half a turn either way.
RDE_INTERNAL f64 fude_zoom_select_wrap(f64 _a) {
    while(_a > RDE_PI)  { _a -= 2.0 * RDE_PI; }
    while(_a < -RDE_PI) { _a += 2.0 * RDE_PI; }
    return _a;
}

// Returns where a copy goes on the screen: _move after its place there, or, mirrored across _mirror's line, the
// similarity that with its own y turned over (*_flip) is that reflection of it. Text and pictures are never turned over
// (they would read backwards): theirs keeps them the right way round, their box where the reflection puts it.
RDE_INTERNAL fude_zoom_sim fude_zoom_select_put_sim(const fude_zoom_clip* _c, const fude_zoom_sim* _move, const fude_zoom_select_mirror* _mirror, b8* _flip) {
    *_flip = false;
    if(_mirror == NULL) {
        return fude_zoom_sim_compose(*_move, _c->on_screen);
    }
    const f64 _th = rde_math_atan2_f64(_c->on_screen.b, _c->on_screen.a), _k = rde_math_hypot_f64(_c->on_screen.a, _c->on_screen.b);
    const f64 _ca = rde_math_cos_f64(2.0 * _mirror->angle), _sa = rde_math_sin_f64(2.0 * _mirror->angle);
    // The reflection of a point (of the screen).
    #define FUDE_ZOOM_REFLECT(_p) ((fude_zoom_v2){ _mirror->at.x + _ca * ((_p).x - _mirror->at.x) + _sa * ((_p).y - _mirror->at.y), \
                                                    _mirror->at.y + _sa * ((_p).x - _mirror->at.x) - _ca * ((_p).y - _mirror->at.y) })
    const fude_zoom_v2 _t = FUDE_ZOOM_REFLECT(((fude_zoom_v2){ _c->on_screen.tx, _c->on_screen.ty }));
    const b8 _box = _c->look.kind == FUDE_ZOOM_KIND_TEXT || _c->look.kind == FUDE_ZOOM_KIND_IMAGE;
    if(!_box) {
        *_flip = true;
        const f64 _r = 2.0 * _mirror->angle - _th;
        return (fude_zoom_sim){ _k * rde_math_cos_f64(_r), _k * rde_math_sin_f64(_r), _t.x, _t.y };
    }
    // The right way round: its across turned back (its top left the reflection of its top right) or its up (of its
    // bottom left) — whichever leaves it turned as it was.
    const f64 _ra = 2.0 * _mirror->angle - _th + RDE_PI, _rb = 2.0 * _mirror->angle - _th;
    const b8  _across = rde_math_abs_f64(fude_zoom_select_wrap(_ra - _th)) <= rde_math_abs_f64(fude_zoom_select_wrap(_rb - _th));
    const f64 _r = _across ? _ra : _rb;
    fude_zoom_v2 _at = _t;
    if(_c->look.kind == FUDE_ZOOM_KIND_TEXT && fude_zoom_clip_size(_c) >= 3u * sizeof(f64)) {
        f64 _w, _h;
        rde_memcpy(&_w, fude_zoom_clip_data(_c) + sizeof(f64), sizeof(f64));
        rde_memcpy(&_h, fude_zoom_clip_data(_c) + 2u * sizeof(f64), sizeof(f64));
        const fude_zoom_v2 _corner = fude_zoom_sim_apply(_c->on_screen, _across ? (fude_zoom_v2){ _w, 0.0 } : (fude_zoom_v2){ 0.0, -_h });
        _at = FUDE_ZOOM_REFLECT(_corner);
    }
    #undef FUDE_ZOOM_REFLECT
    return (fude_zoom_sim){ _k * rde_math_cos_f64(_r), _k * rde_math_sin_f64(_r), _at.x, _at.y };
}

// Returns the id of the copy made of the thing whose id is _id (_now_id, by their originals' _was_id, _n of each), or 0
// when it was not copied (or _id is 0): what a copy joined to it is joined to.
RDE_INTERNAL fude_zoom_id fzsel_copy_of(fude_zoom_id _id, const fude_zoom_id* _was_id, const fude_zoom_id* _now_id, u32 _n) {
    fude_zoom_id _to = 0;
    for(u32 _q = 0; _q < _n && _id != 0; _q++) {
        _to = _was_id[_q] == _id ? _now_id[_q] : _to;
    }
    return _to;
}

// Makes _num (*_nn its numbers, *_cnt of them) hold _want numbers at least, the new ones after its own: room for ids
// kept in two halves where an older one kept each as one number's bits.
RDE_INTERNAL void fzsel_grow(rde_arr* _num, f64** _nn, u32* _cnt, u32 _want) {
    if(*_cnt < _want) {
        rde_arr_add_n(_num, _want - *_cnt);
        *_nn  = (f64*)_num->memory;   // (it may have moved)
        *_cnt = _want;
    }
}

// A copy of shape clip _c put into _frame at _at, as Repeat's put makes it (_flip: turned over; _dim: a dimension, a
// circle's size or a connector, joined to the copies of what it measures or joins, else to nothing: a copy never
// follows the originals; _props: a property, on the copy of its thing): its numbers in _num (scratch), the ids of the
// copies made so far (_now_id, by their originals' _was_id, _n of each), its line's style as it was. The ids it keeps
// are written in two halves (an older one's, each one number's bits, made so too). The object made into *_o. False: a
// property whose thing was not copied (not put).
RDE_INTERNAL b8 fude_zoom_select_put_shape(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _at, const fude_zoom_clip* _c, b8 _flip, b8 _dim, b8 _props,
                                           const fude_zoom_id* _was_id, const fude_zoom_id* _now_id, u32 _n, rde_arr* _num, u32* _o) {
    const u32 _count = fude_zoom_clip_size(_c) / (u32)sizeof(f64);
    rde_arr_clear(_num);
    f64* _nn = rde_arr_add_n(_num, _count > 0u ? _count : 1u);
    rde_memcpy(_nn, fude_zoom_clip_data(_c), (usize)_count * sizeof(f64));
    if(_flip) {
        fude_zoom_shape_mirror(_c->look.channels, _nn, _count);
    }
    u32 _cnt = _count;
    const u8 _type = _c->look.channels;
    if(_props) {
        // On the copy of its thing (none of it made: not put).
        const fude_zoom_id _to = fzsel_copy_of(_cnt >= 3u ? fude_zoom_id_get(&_nn[1]) : 0u, _was_id, _now_id, _n);
        if(_to == 0u) {
            return false;
        }
        fude_zoom_id_put(&_nn[1], _to);
    }
    fude_zoom_id _ids[2];
    i32          _keys[2];
    const u32    _k = _type == FUDE_ZOOM_SHAPE_ARROW ? fude_zoom_arrow_points(_nn, _cnt) : 0u;
    if(_dim && _type == FUDE_ZOOM_SHAPE_RADIAL && _cnt >= 4u) {
        // A circle's size: on the copy of its circle.
        const fude_zoom_id _to = fzsel_copy_of(fude_zoom_connect_radial_id(_nn, _cnt), _was_id, _now_id, _n);
        fzsel_grow(_num, &_nn, &_cnt, FUDE_ZOOM_RADIAL_NUMBERS);
        fude_zoom_id_put(&_nn[3], _to);
    } else if(_dim && _type == FUDE_ZOOM_SHAPE_ARROW) {
        // A connector: each end on the copy of what it joined (a loose end where that was not copied).
        if(_k >= 2u && fude_zoom_connect_arrow_ends(_nn, _cnt, _k, &_ids[0], &_ids[1])) {
            fzsel_grow(_num, &_nn, &_cnt, FUDE_ZOOM_ARROW_NUMBERS(_k));
            fude_zoom_connect_arrow_put(_nn, _k, fzsel_copy_of(_ids[0], _was_id, _now_id, _n), fzsel_copy_of(_ids[1], _was_id, _now_id, _n));
        }
    } else if(_dim && fude_zoom_connect_dimension_ends(_nn, _cnt, _ids, _keys)) {
        // A dimension: each end on the copy of what it measured.
        _ids[0] = fzsel_copy_of(_ids[0], _was_id, _now_id, _n);
        _ids[1] = fzsel_copy_of(_ids[1], _was_id, _now_id, _n);
        fzsel_grow(_num, &_nn, &_cnt, FUDE_ZOOM_DIMENSION_REFS);
        fude_zoom_connect_dimension_put(_nn, _ids, _keys);
    }
    _s->style = _type == FUDE_ZOOM_SHAPE_SYMBOL ? 0 : _c->look.q;   // (its line's style, as it was)
    *_o = fude_zoom_scene_add_shape_fill(_s, _frame, _at, _type, _nn, _cnt, _c->look.color, _c->look.radius,
                                         (u8)(_c->look.flags & (FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN)), _c->look.fill, 0);
    _s->style = 0;
    return true;
}

// See the header. Each copy is made new, on top, in the clips' order; dimensions, connectors and properties go in a
// second pass, after what they are joined to, so they can be joined to its copy (a dimension's or a connector's end
// whose thing was not copied is joined to nothing; a property whose thing was not copied is not put). Bookmarks are
// never put.
u32 fude_zoom_select_put(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_clip* _clips, u32 _n, const fude_zoom_sim* _moves, u32 _m,
                         const fude_zoom_select_mirror* _mirror, const u32* _dead, u32 _dead_n) {
    if(_n == 0) {
        return 0u;
    }
    // Each copy as it looked on screen, moved, into the camera's frame (the same size on screen,
    // whatever the zoom now).
    const u32           _frame   = _s->camera.frame;
    const fude_zoom_sim _from_sc = fude_zoom_sim_inverse(fude_zoom_camera_sim(&_s->camera));
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _born = rde_arr_new(sizeof(u32), _heap);
    rde_arr _num  = rde_arr_new(sizeof(f64), _heap);
    rde_hash_set _picked = fude_zoom_select_picked(_sel);
    const u32 _times = _mirror != NULL ? 1u : _m;
    // Each copy's things' ids, by their originals': a dimension copied with what it measures is joined to the copies.
    rde_arr TYPE(fude_zoom_id) _id_arr = rde_arr_new(sizeof(fude_zoom_id), _heap);
    fude_zoom_id* _was_id = (fude_zoom_id*)rde_arr_add_n(&_id_arr, 2u * _n);   // (sized once: it stays put)
    fude_zoom_id* _now_id = _was_id + _n;
    for(u32 _k = 0; _k < _times; _k++) {
        rde_memset(_now_id, 0, (usize)_n * sizeof(fude_zoom_id));
        for(u32 _j = 0; _j < 2u * _n; _j++) {
            // (dimensions after the rest: what they are joined to made first)
            const u32 _i = _j % _n;
            const fude_zoom_clip* _c = &_clips[_i];
            const b8 _dim   = _c->look.kind == FUDE_ZOOM_KIND_SHAPE && (_c->look.channels == FUDE_ZOOM_SHAPE_DIMENSION || _c->look.channels == FUDE_ZOOM_SHAPE_RADIAL ||
                                                                   _c->look.channels == FUDE_ZOOM_SHAPE_ARROW);
            const b8 _props = _c->look.kind == FUDE_ZOOM_KIND_SHAPE && _c->look.channels == FUDE_ZOOM_SHAPE_PROPS;
            _was_id[_i] = _c->look.id;
            if(_c->look.kind == FUDE_ZOOM_KIND_MARK || (_dim || _props) != (_j >= _n)) {
                continue;
            }
            b8 _flip = false;
            const fude_zoom_sim   _m2 = fude_zoom_sim_compose(_from_sc, fude_zoom_select_put_sim(_c, _mirror != NULL ? NULL : &_moves[_k], _mirror, &_flip));
            const fude_zoom_place _at = { { _m2.tx, _m2.ty }, rde_math_atan2_f64(_m2.b, _m2.a), rde_math_hypot_f64(_m2.a, _m2.b) };
            u32 _o = FUDE_ZOOM_NONE;
            if(_c->look.kind == FUDE_ZOOM_KIND_IMAGE && fude_zoom_clip_size(_c) > 2u * sizeof(f64)) {
                f64 _hw, _hh;
                rde_memcpy(&_hw, fude_zoom_clip_data(_c), sizeof(f64));
                rde_memcpy(&_hh, fude_zoom_clip_data(_c) + sizeof(f64), sizeof(f64));
                _o = fude_zoom_scene_add_image(_s, _frame, _at, _hw, _hh, fude_zoom_clip_data(_c) + 2u * sizeof(f64), fude_zoom_clip_size(_c) - 2u * (u32)sizeof(f64), 0);
            } else if(_c->look.kind == FUDE_ZOOM_KIND_TEXT && fude_zoom_clip_size(_c) >= 3u * sizeof(f64) + sizeof(u32)) {
                f64 _size, _w, _h;
                u32 _style;
                rde_memcpy(&_size, fude_zoom_clip_data(_c), sizeof(f64));
                rde_memcpy(&_w, fude_zoom_clip_data(_c) + sizeof(f64), sizeof(f64));
                rde_memcpy(&_h, fude_zoom_clip_data(_c) + 2u * sizeof(f64), sizeof(f64));
                rde_memcpy(&_style, fude_zoom_clip_data(_c) + 3u * sizeof(f64), sizeof(u32));
                const u32 _head = 3u * (u32)sizeof(f64) + (u32)sizeof(u32);
                _o = fude_zoom_scene_add_text(_s, _frame, _at, _size, _w, _h, (u8)_style, _c->look.color, _c->look.fill,
                                              (const c8*)(fude_zoom_clip_data(_c) + _head), fude_zoom_clip_size(_c) - _head, 0);
            } else if(_c->look.kind == FUDE_ZOOM_KIND_SHAPE) {
                if(!fude_zoom_select_put_shape(_s, _frame, _at, _c, _flip, _dim, _props, _was_id, _now_id, _n, &_num, &_o)) {
                    continue;
                }
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
                    fude_zoom_select_add(_sel, &_picked, _o);   // (a property is not held: it goes with its thing)
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
    rde_hash_set_free(&_picked);
    fude_zoom_select_update(_sel, _s);
    return _made;
}

// See the header. The copy lands right and down of the original by FUDE_ZOOM_SELECT_DUPLICATE points. Its clips are its
// own, freed after it: the clipboard keeps what was copied (as Duplicate leaves it in other apps).
void fude_zoom_select_duplicate(fude_zoom_selection* _sel, fude_zoom_scene* _s) {
    if(!fude_zoom_select_any(_sel)) {
        return;
    }
    rde_arr            _clips  = rde_arr_new(sizeof(fude_zoom_clip), rde_memory_allocator_get_default_std());
    const fude_zoom_v2 _center = fzsel_clips_of_picks(_sel, _s, &_clips);
    const rde_vec_2F   _at     = { (f32)(_center.x + FUDE_ZOOM_SELECT_DUPLICATE), (f32)(_center.y - FUDE_ZOOM_SELECT_DUPLICATE) };
    fude_zoom_select_paste_clips(_sel, _s, (const fude_zoom_clip*)_clips.memory, (u32)rde_arr_length(&_clips), _center, _at);
    fude_zoom_select_clips_free(&_clips);
    rde_arr_free(&_clips);
}

// See the header.
void fude_zoom_select_all(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_renderer* _r) {
    fude_zoom_select_clear(_sel);
    fude_zoom_select_take(_sel, _s, _r, NULL, 0, true);
    fude_zoom_select_update(_sel, _s);
}

// --- arranging ---------------------------------------------------------------------------------

// One pick being arranged: its index among the picks and its box.
typedef struct {
    u32           pick;
    fude_zoom_box box;    // on the screen
} fude_zoom_arranged;

// rde_sort's order for spreading across: by the box's middle, left to right.
RDE_INTERNAL int fude_zoom_arranged_by_x(const any _a, const any _b) {
    const f64 _x = ((const fude_zoom_arranged*)_a)->box.min_x + ((const fude_zoom_arranged*)_a)->box.max_x;
    const f64 _y = ((const fude_zoom_arranged*)_b)->box.min_x + ((const fude_zoom_arranged*)_b)->box.max_x;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// rde_sort's order for spreading down: by the box's middle, bottom to top.
RDE_INTERNAL int fude_zoom_arranged_by_y(const any _a, const any _b) {
    const f64 _x = ((const fude_zoom_arranged*)_a)->box.min_y + ((const fude_zoom_arranged*)_a)->box.max_y;
    const f64 _y = ((const fude_zoom_arranged*)_b)->box.min_y + ((const fude_zoom_arranged*)_b)->box.max_y;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// See the header. The boxes are the objects' boxes on the screen (not padded); spreading lays them from the
// selection's one edge to the other in the order of their middles, the gaps between them even. Each move is a slide of
// its place in its own frame; the connectors followed are those in every frame what moved is in.
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
        rde_sort(_a, _n, sizeof(_a[0]), _x ? fude_zoom_arranged_by_x : fude_zoom_arranged_by_y);
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
        if(rde_math_abs_f64(_d[_i].x) + rde_math_abs_f64(_d[_i].y) < 1e-9) {
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
        rde_memcpy(rde_arr_add_n(&_just, _moved), _ids.memory, (usize)_moved * sizeof(u32));
        // (in every frame what moved is in, not the camera's alone: a loop takes things deeper down too)
        rde_arr _frames = rde_arr_new(sizeof(u32), _heap);
        fude_zoom_select_frames_of(_s, (const u32*)_just.memory, _moved, &_frames);
        for(u32 _k = 0; _k < (u32)rde_arr_length(&_frames); _k++) {
            fude_zoom_connect_follow(_s, ((const u32*)_frames.memory)[_k], (const u32*)_just.memory, _moved, &_ids, &_before, &_after);
        }
        rde_arr_free(&_frames);
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
