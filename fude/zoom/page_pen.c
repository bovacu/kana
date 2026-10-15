// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"

// ===========================================================================
// Page, the pen: a stroke drawn (its shape snapped when the pen rests), the Saw, a pen line's pieces, Sculpt and
// Nudge (redrawing or pushing a stroke), the eraser, and paths added as strokes (offsets: kerf, a router's bit).
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// --- the pen: a stroke --------------------------------------------------------------------------

// The capture emptied for the next stroke (its pen state — pressure seen, the
// live pen — kept: it is the same pen).
RDE_INTERNAL void fude_zoom_page_capture_reset(fude_zoom_page* _page) {
    fude_ink* _c = &_page->capture;
    rde_arr_clear(&_c->strokes);
    rde_arr_clear(&_c->points);
    rde_arr_clear(&_c->actions);
    rde_arr_clear(&_c->targets);
    _c->action_count  = 0;
    _c->drawing       = false;
}

// Smoothing (smooth.h): the capture's new points kept as the pen gave them,
// and its unsettled end smoothed again from them — what the capture holds, and
// shows, is the smoothed line. While the pen is down (not _final) the line's
// last stretch bends to the pen itself, so its tip is always under the pen tip;
// when it lifts (_final) the smoothed end takes its place.
RDE_INTERNAL void fude_zoom_page_smooth_tail_as(fude_zoom_page* _page, b8 _final) {
    fude_ink* _c = &_page->capture;
    if(fude_ink_stroke_count(_c) == 0) {
        return;
    }
    const fude_ink_stroke* _st = fude_ink_stroke_at(_c, fude_ink_stroke_count(_c) - 1u);
    fude_ink_point*        _p  = (fude_ink_point*)fude_ink_stroke_points(_c, _st);
    const u32              _n  = _st->point_count;
    for(u32 _i = (u32)rde_arr_length(&_page->raw); _i < _n; _i++) {
        const fude_zoom_v2 _r = { (f64)_p[_i].position.x, (f64)_p[_i].position.y };
        rde_arr_add(&_page->raw, (any)&_r);
        rde_arr_add(&_page->smoothed, (any)&_r);
    }
    const f64 _sigma = fude_zoom_smooth_sigma(_page->stroke_smooth) / _page->draw_z;
    if(!(_sigma > 0.0) || _n < 3u) {
        return;
    }
    const u32           _from = _page->smooth_from;
    fude_zoom_v2*       _out  = (fude_zoom_v2*)_page->smoothed.memory;
    const fude_zoom_v2* _raw  = (const fude_zoom_v2*)_page->raw.memory;
    _page->smooth_from = fude_zoom_smooth_points(_raw, _n, _sigma, _out, _from);
    for(u32 _i = _from; _i < _n; _i++) {
        _p[_i].position = (rde_vec_2F){ (f32)_out[_i].x, (f32)_out[_i].y };
    }
    if(!_final) {
        // The last two widths eased onto the pen: all of it at the tip, none
        // two widths back (all unsettled, so smoothed afresh next time).
        const f64 _reach = 2.0 * _sigma;
        f64 _d = 0.0;
        for(u32 _i = _n - 1u; _i >= _from && _d < _reach; _i--) {
            const f64 _w = (1.0 - _d / _reach) * (1.0 - _d / _reach);
            _p[_i].position = (rde_vec_2F){ (f32)(_out[_i].x + (_raw[_i].x - _out[_i].x) * _w), (f32)(_out[_i].y + (_raw[_i].y - _out[_i].y) * _w) };
            if(_i == 0) {
                break;
            }
            _d += rde_math_hypot_f64(_raw[_i].x - _raw[_i - 1u].x, _raw[_i].y - _raw[_i - 1u].y);
        }
    }
}

// Smooths the stroke's tail while the pen is down: fude_zoom_page_smooth_tail_as, not final.
RDE_INTERNAL void fude_zoom_page_smooth_tail(fude_zoom_page* _page) {
    fude_zoom_page_smooth_tail_as(_page, false);
}

// The pen at _at (from origin, the drawing frame's units) into the stroke: on
// the rope, where its tip is pulled to (nothing while the string is slack).
RDE_INTERNAL void fude_zoom_page_feed(fude_zoom_page* _page, fude_zoom_v2 _at) {
    if(_page->stroke_smooth == FUDE_ZOOM_SMOOTH_ROPE) {
        if(!fude_zoom_smooth_rope(&_page->rope_tip, _at, FUDE_ZOOM_SMOOTH_ROPE_LENGTH / _page->draw_z)) {
            return;
        }
        _at = _page->rope_tip;
    }
    fude_ink_extend(&_page->capture, (rde_vec_2F){ (f32)_at.x, (f32)_at.y });
    fude_zoom_page_smooth_tail(_page);
}

// See page_parts.h. The stroke's points are kept from origin, where the pen went down in the camera's frame (taken
// after fude_zoom_camera_drawing_frame, which may move the camera into a new frame); the brush is the toolbar's, and
// along an instrument the stroke is not smoothed.
void fude_zoom_page_draw_down(fude_zoom_page* _page, rde_vec_2F _screen, b8 _from_pen) {
    fude_app*      _app = _page->app;
    fude_zoom_scene* _s = &_page->scene;
    const fude_zoom_v2 _at = fude_zoom_camera_to_frame(&_s->camera, (fude_zoom_v2){ _screen.x, _screen.y });
    const f64 _near = 4.0 / _s->camera.z;
    // Drawn LATER on top: a new frame when a parent's later ink covers here.
    _page->draw_frame = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _at.x - _near, _at.y - _near, _at.x + _near, _at.y + _near });
    _page->origin     = fude_zoom_camera_to_frame(&_s->camera, (fude_zoom_v2){ _screen.x, _screen.y });
    _page->stroke_smooth = _page->ruled ? (u8)FUDE_ZOOM_SMOOTH_OFF : _page->smooth_level;   // along an instrument: exactly its edge
    _page->draw_z     = _s->camera.z;

    // The brush as the toolbar and Settings have it.
    fude_ink*       _c     = &_page->capture;
    const fude_ink* _brush = _app->ink;
    _c->width_mode      = _brush->width_mode;
    _c->brush_scale     = _brush->brush_scale;
    _c->color           = _brush->color;
    _c->constant_radius = _brush->constant_radius;
    // On the page: the canvas's own size (its home frame's units), caught in the camera frame's.
    _c->page_scale      = (f32)(1.0 / fude_zoom_page_home_units(_page, _s->camera.frame));
    _c->marker_color    = _brush->marker_color;
    _c->marker_radius   = _brush->marker_radius;
    _c->marking         = _app->ui->bar.tool == FUDE_TOOL_MARK;
    _c->zoom            = (f32)_page->draw_z;
    fude_zoom_page_capture_reset(_page);
    fude_ink_begin(_c, (rde_vec_2F){ 0.0f, 0.0f }, _from_pen);
    rde_arr_clear(&_page->raw);
    rde_arr_clear(&_page->smoothed);
    _page->smooth_from = 0;
    _page->rope_tip    = (fude_zoom_v2){ 0.0, 0.0 };
    fude_zoom_page_smooth_tail(_page);
    _page->drawing      = true;
    _page->stroke_began = rde_engine_get_time_now();
    _page->snapped      = false;
    _page->moved_at     = _page->stroke_began;
    _page->moved_from   = _screen;
    _page->pen_now      = _screen;
}

// See page_parts.h. A move of more than FUDE_ZOOM_PAGE_HOLD_SLOP starts the rest the snap waits for again
// (moved_at).
void fude_zoom_page_draw_moved(fude_zoom_page* _page, rde_vec_2F _screen) {
    _page->pen_now = _screen;
    if(_page->snapped) {
        return;   // the shape follows the pen (drawn and placed from pen_now)
    }
    if(rde_math_abs_f32(_screen.x - _page->moved_from.x) + rde_math_abs_f32(_screen.y - _page->moved_from.y) > FUDE_ZOOM_PAGE_HOLD_SLOP) {
        _page->moved_at   = rde_engine_get_time_now();
        _page->moved_from = _screen;
    }
    const fude_zoom_v2 _at = fude_zoom_camera_to_frame(&_page->scene.camera, (fude_zoom_v2){ _screen.x, _screen.y });
    fude_zoom_page_feed(_page, (fude_zoom_v2){ _at.x - _page->origin.x, _at.y - _page->origin.y });
}

// The drawing frame's units → the screen, now.
fude_zoom_sim fude_zoom_page_draw_sim(const fude_zoom_page* _page) {
    return fude_zoom_sim_compose(fude_zoom_camera_sim(&_page->scene.camera), fude_zoom_scene_sim(&_page->scene, _page->draw_frame, _page->scene.camera.frame));
}

// The pen has rested: the stroke so far, if it looks like a shape, becomes it.
void fude_zoom_page_try_snap(fude_zoom_page* _page) {
    const fude_ink* _c = &_page->capture;
    if(fude_ink_stroke_count(_c) == 0) {
        return;
    }
    const fude_ink_stroke* _st = fude_ink_stroke_at(_c, fude_ink_stroke_count(_c) - 1u);
    if(_st->point_count < 3u || _st->marker) {
        return;
    }
    const fude_ink_point* _p  = fude_ink_stroke_points(_c, _st);
    const fude_zoom_sim   _to = fude_zoom_page_draw_sim(_page);
    rde_arr_clear(&_page->snap_scratch);
    fude_zoom_v2* _pts = rde_arr_add_n(&_page->snap_scratch, _st->point_count);
    for(u32 _i = 0; _i < _st->point_count; _i++) {
        _pts[_i] = fude_zoom_sim_apply(_to, (fude_zoom_v2){ _page->origin.x + (f64)_p[_i].position.x, _page->origin.y + (f64)_p[_i].position.y });
    }
    if(fude_zoom_shape_recognize(_pts, _st->point_count, &_page->fit)) {
        _page->snapped  = true;
        _page->snap_pen = _page->pen_now;
    }
}

// The snapped shape as the pen has it now, screen units: a line's end follows
// the pen; the others grow, shrink and turn round their middle as the pen
// moves round it from where it snapped.
fude_zoom_shape_fit fude_zoom_page_fit_now(const fude_zoom_page* _page) {
    fude_zoom_shape_fit _f = _page->fit;
    if(_f.type == FUDE_ZOOM_SHAPE_LINE) {
        _f.n[0] = (f64)_page->pen_now.x - _f.at.x;
        _f.n[1] = (f64)_page->pen_now.y - _f.at.y;
        return _f;
    }
    const f64 _wx = (f64)_page->snap_pen.x - _f.at.x, _wy = (f64)_page->snap_pen.y - _f.at.y;
    const f64 _nx = (f64)_page->pen_now.x - _f.at.x, _ny = (f64)_page->pen_now.y - _f.at.y;
    const f64 _was = rde_math_hypot_f64(_wx, _wy), _now = rde_math_hypot_f64(_nx, _ny);
    if(_was < 4.0 || _now < 4.0) {
        return _f;
    }
    const f64 _k = _now / _was;
    for(u32 _i = 0; _i < _f.count; _i++) {
        _f.n[_i] *= _k;
    }
    _f.rotation += rde_math_atan2_f64(_ny, _nx) - rde_math_atan2_f64(_wy, _wx);
    return _f;
}

// A shape in screen units into the frame strokes go in (_frame, seen through
// _to): its numbers in that frame's units, unscaled. Its index.
u32 fude_zoom_page_add_fit(fude_zoom_page* _page, u32 _frame, fude_zoom_sim _to, const fude_zoom_shape_fit* _f, rde_color _color, f32 _radius, u8 _flags) {
    const fude_zoom_sim _bk = fude_zoom_sim_inverse(_to);
    const f64           _k  = fude_zoom_sim_scale(_bk);
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    for(u32 _i = 0; _i < _f->count; _i++) {
        _n[_i] = _f->n[_i] * _k;   // screen units → the frame's
    }
    const fude_zoom_place _at = { fude_zoom_sim_apply(_bk, _f->at), _f->rotation + rde_math_atan2_f64(_bk.b, _bk.a), 1.0 };
    return fude_zoom_scene_add_shape(&_page->scene, _frame, _at, _f->type, _n, _f->count, _color, _radius, _flags, 0);
}

// A shape in screen units drawn over the page (the one being placed): its line
// _radius pt, in _color, filled too when _filled.
void fude_zoom_page_draw_fit(fude_zoom_page* _page, const fude_zoom_shape_fit* _f, f32 _radius, rde_color _color, b8 _filled) {
    b8 _closed;
    fude_zoom_shape_outline(_f->type, _f->n, _f->count, fude_zoom_shape_segments(400.0), &_page->snap_scratch, &_closed);
    const u32 _k = (u32)rde_arr_length(&_page->snap_scratch);
    if(_k < 2u || _k > 511u) {
        return;
    }
    const f64 _co = rde_math_cos_f64(_f->rotation), _si = rde_math_sin_f64(_f->rotation);
    rde_vec_2F _pts[512];
    f32        _rr[512];
    for(u32 _i = 0; _i < _k; _i++) {
        const fude_zoom_v2 _l = ((const fude_zoom_v2*)_page->snap_scratch.memory)[_i];
        _pts[_i] = (rde_vec_2F){ (f32)(_f->at.x + _l.x * _co - _l.y * _si), (f32)(_f->at.y + _l.x * _si + _l.y * _co) };
        _rr[_i]  = _radius;
    }
    u32 _m = _k;
    if(_closed) {
        _pts[_k] = _pts[0];
        _rr[_k]  = _radius;
        _m++;
    }
    rde_rendering_2d_draw_stroke(_pts, _rr, _m, _color);
    // The fill after the line, as the canvas draws it (render_shape.c).
    if(_closed && _filled) {
        rde_rendering_2d_draw_polygon(_pts, _k, _color, NULL);
    }
}

// --- the Saw (cut.h): what is drawn cuts boards ------------------------------------------------

#define FUDE_ZOOM_PAGE_KERF_MM 0.0   // the saw's cut until another is set: the thinnest (the pieces' edges meet)

// See page_parts.h.
f64 fude_zoom_page_kerf(const fude_zoom_page* _page) {
    return _page->kerf_mm > 0.0f ? (f64)_page->kerf_mm : FUDE_ZOOM_PAGE_KERF_MM;
}

// _cut (a closed polygon, _frame's units) taken out of every board in _frame it
// touches: each one replaced by its pieces, one undo step. How many pieces now
// (FUDE_ZOOM_NONE: no board touched).
RDE_INTERNAL u32 fude_zoom_page_saw_cut(fude_zoom_page* _page, u32 _frame, const fude_zoom_v2* _cut, u32 _n) {
    fude_zoom_scene* _s = &_page->scene;
    if(_n < 3u) {
        return FUDE_ZOOM_NONE;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    // The cut on the screen, then in each frame shown that tools reach: a board
    // is cut at whatever depth it was made (not only in the frame drawn in).
    rde_arr _screen_arr = rde_arr_new(sizeof(fude_zoom_v2), _heap), _local_arr = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr_resize(&_screen_arr, _n);
    rde_arr_resize(&_local_arr, _n);
    fude_zoom_v2* _screen = (fude_zoom_v2*)_screen_arr.memory, *_local = (fude_zoom_v2*)_local_arr.memory;   // (sized once: they stay put)
    const fude_zoom_sim _to = fude_zoom_page_frame_sim(_page, _frame);
    for(u32 _i = 0; _i < _n; _i++) {
        _screen[_i] = fude_zoom_sim_apply(_to, _cut[_i]);
    }
    rde_arr_clear(&_page->editable);
    fude_zoom_render_editable(&_page->renderer, _s, &_page->editable);
    rde_arr _found = rde_arr_new(sizeof(u32), _heap), _died = rde_arr_new(sizeof(u32), _heap), _born = rde_arr_new(sizeof(u32), _heap);
    rde_arr _pairs_d = rde_arr_new(sizeof(u32), _heap), _pairs_b = rde_arr_new(sizeof(u32), _heap);
    u32 _pieces = FUDE_ZOOM_NONE;
    fude_zoom_box _box = fude_zoom_box_empty();
    u32 _cut_frame = _frame;
    for(u32 _v = 0; _v < (u32)rde_arr_length(&_page->editable); _v++) {
        const fude_zoom_visible _vis  = ((const fude_zoom_visible*)_page->editable.memory)[_v];
        const fude_zoom_sim     _back = fude_zoom_sim_inverse(_vis.to_screen);
        fude_zoom_box _fb = fude_zoom_box_empty();
        for(u32 _i = 0; _i < _n; _i++) {
            _local[_i] = fude_zoom_sim_apply(_back, _screen[_i]);
            _fb = fude_zoom_box_union(_fb, (fude_zoom_box){ _local[_i].x, _local[_i].y, _local[_i].x, _local[_i].y });
        }
        rde_arr_clear(&_found);
        fude_zoom_scene_query(_s, _vis.frame, _fb, &_found);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_found); _i++) {
            const u32 _board = ((const u32*)_found.memory)[_i];
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, _board);
            if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_BOARD || !fude_zoom_scene_touchable(_s, _o)) {
                continue;
            }
            const u32 _before = (u32)rde_arr_length(&_born);
            const u32 _made   = fude_zoom_cut_board(_s, _board, _local, NULL, _n, &_born);
            if(_made == FUDE_ZOOM_NONE) {
                continue;
            }
            fude_zoom_scene_set_alive(_s, _board, false);
            rde_arr_add(&_died, (any)&_board);
            _pieces    = (_pieces == FUDE_ZOOM_NONE ? 0u : _pieces) + _made;
            _box       = _fb;
            _cut_frame = _vis.frame;
            if(_made > 0) {
                // Connectors joined to it: to its first piece.
                rde_arr_add(&_pairs_d, (any)&_board);
                rde_arr_add(&_pairs_b, (any)&((const u32*)_born.memory)[_before]);
            }
        }
    }
    if(rde_arr_length(&_died) > 0) {
        fude_zoom_page_rebind(_page, (const u32*)_pairs_d.memory, (const u32*)_pairs_b.memory, (u32)rde_arr_length(&_pairs_d), &_died, &_born);
        fude_zoom_history_push(_s, _cut_frame, _box, (const u32*)_died.memory, (u32)rde_arr_length(&_died), (const u32*)_born.memory, (u32)rde_arr_length(&_born));
        fude_zoom_select_clear(&_page->selection);
    }
    rde_arr_free(&_found); rde_arr_free(&_died); rde_arr_free(&_born); rde_arr_free(&_pairs_d); rde_arr_free(&_pairs_b);
    rde_arr_free(&_screen_arr);
    rde_arr_free(&_local_arr);
    return _pieces;
}

// A line drawn with the Saw (_frame's units): closed (its ends met), its inside
// cut out; else a cut along it, the kerf wide. Said how it went.
void fude_zoom_page_saw_line(fude_zoom_page* _page, u32 _frame, const fude_zoom_v2* _p, u32 _n, b8 _closed) {
    rde_arr _poly = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    if(_closed && _n >= 3u) {
        rde_memcpy(rde_arr_add_n(&_poly, _n), _p, (usize)_n * sizeof(fude_zoom_v2));
    } else {
        // The kerf in the frame's units (the canvas's millimetres), never so thin it would not part the wood.
        const f64 _per  = fude_zoom_page_home_units(_page, _frame) * FUDE_ZOOM_PAGE_MM_PER_UNIT;   // mm a unit of the frame
        const f64 _kerf = rde_math_max_f64(fude_zoom_page_kerf(_page), 0.01) / rde_math_max_f64(_per, 1e-300);
        fude_zoom_cut_band(_p, _n, _kerf, &_poly);
    }
    const u32 _pieces = fude_zoom_page_saw_cut(_page, _frame, (const fude_zoom_v2*)_poly.memory, (u32)rde_arr_length(&_poly));
    if(_pieces == FUDE_ZOOM_NONE) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_SAW_NOTHING));
    } else if(_pieces <= 1u) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_SAW_DONE));
    } else {
        c8 _say[96];
        FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_SAW_PIECES, FUDE_TN(_pieces));
        fude_notice_show(_say);
    }
    rde_arr_free(&_poly);
}

// --- a pen line's pieces --------------------------------------------------------------------

// A pen line comes in pieces (a long one, or one made of pieces): those of one gesture in one frame are put in the
// order drawn, in runs — a new run where a piece does not begin where the one before it ended (a piece between them
// erased).

// rde_sort's order for (z, object index) pairs: by z, then by index.
RDE_INTERNAL int fude_zoom_page_by_z(const any _a, const any _b) {
    const u64* _x = (const u64*)_a;
    const u64* _y = (const u64*)_b;
    return _x[0] < _y[0] ? -1 : (_x[0] > _y[0] ? 1 : (_x[1] < _y[1] ? -1 : (_x[1] > _y[1] ? 1 : 0)));
}

// See page_parts.h. _pieces is sorted in place into the order drawn; _runs must have room for _n. A piece begins a new
// run when its first point is further from the end of the one before than twice its half-width or four quanta.
u32 fude_zoom_page_line_runs(const fude_zoom_scene* _s, u32* _pieces, u32 _n, fude_zoom_run* _runs) {
    if(_n == 0) {
        return 0;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    // In the order drawn: by z (each piece went on after the one before), then by slot.
    rde_arr _keys_arr = rde_arr_new(sizeof(u64), _heap);
    rde_arr_resize(&_keys_arr, (usize)_n * 2u);
    u64* _keys = (u64*)_keys_arr.memory;
    for(u32 _i = 0; _i < _n; _i++) {
        _keys[_i * 2u]      = fude_zoom_scene_object(_s, _pieces[_i])->z;
        _keys[_i * 2u + 1u] = _pieces[_i];
    }
    rde_sort(_keys, _n, 2u * sizeof(u64), fude_zoom_page_by_z);
    for(u32 _i = 0; _i < _n; _i++) {
        _pieces[_i] = (u32)_keys[_i * 2u + 1u];
    }
    rde_arr_free(&_keys_arr);
    // Where each ends and the next begins.
    rde_arr _q = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
    u32 _count = 0;
    fude_zoom_v2 _end = { 0.0, 0.0 };
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _pieces[_i]);
        rde_arr_clear(&_q);
        if(_o->count > 0) {
            rde_arr_add_n(&_q, _o->count);
        }
        const b8 _read = _o->count > 0 && fude_zoom_scene_points(_s, _pieces[_i], (fude_zoom_qpoint*)_q.memory);
        const fude_zoom_v2 _begin = _read ? fude_zoom_scene_point_at(_o, &((const fude_zoom_qpoint*)_q.memory)[0]) : _end;
        const f64 _near = rde_math_max_f64(2.0 * (f64)_o->radius * _o->scale, 4.0 * rde_math_ldexp_f64(1.0, _o->q) * _o->scale);
        if(_count == 0 || rde_math_hypot_f64(_begin.x - _end.x, _begin.y - _end.y) > _near) {
            _runs[_count++] = (fude_zoom_run){ _i, 0u };
        }
        _runs[_count - 1u].count++;
        if(_read) {
            _end = fude_zoom_scene_point_at(_o, &((const fude_zoom_qpoint*)_q.memory)[_o->count - 1u]);
        }
    }
    rde_arr_free(&_q);
    return _count;
}

// A run's points, frame units, into _out (appended; the point pieces share where they join given once).
void fude_zoom_page_run_points(const fude_zoom_scene* _s, const u32* _pieces, fude_zoom_run _run, rde_arr* _out) {
    rde_arr _q = rde_arr_new(sizeof(fude_zoom_qpoint), rde_memory_allocator_get_default_std());
    for(u32 _i = _run.first; _i < _run.first + _run.count; _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _pieces[_i]);
        rde_arr_clear(&_q);
        if(_o->count == 0) {
            continue;
        }
        rde_arr_add_n(&_q, _o->count);
        if(!fude_zoom_scene_points(_s, _pieces[_i], (fude_zoom_qpoint*)_q.memory)) {
            continue;
        }
        for(u32 _k = (_i > _run.first ? 1u : 0u); _k < _o->count; _k++) {
            const fude_zoom_v2 _at = fude_zoom_scene_point_at(_o, &((const fude_zoom_qpoint*)_q.memory)[_k]);
            rde_arr_add(_out, (any)&_at);
        }
    }
    rde_arr_free(&_q);
}

// --- Sculpt (research §2.2: redraw over part of a stroke to fix it) ----------------------------------

// Paths added as strokes, defined with the offsets below.
RDE_INTERNAL void fude_zoom_page_add_path_radii(fude_zoom_page* _page, u32 _frame, const fude_zoom_v2* _p, const f32* _radii, u32 _n, b8 _closed,
                                                rde_color _color, f32 _radius, rde_arr* _born);
RDE_INTERNAL void fude_zoom_page_add_path_with(fude_zoom_page* _page, u32 _frame, const fude_zoom_v2* _p, const f32* _radii, u32 _n, b8 _closed,
                                               rde_color _color, f32 _radius, rde_arr* _born);

#define FUDE_ZOOM_PAGE_SCULPT_REACH 10.0   // screen points a stroke begun this near a line sculpts it

// The stroke just drawn (_p, _n: the capture's, from the page's origin) begun on
// a pen line in the frame drawn in: that line redone — the stretch between
// where the new one began and ended on it replaced by the new one, or, ending
// off it, all of it from there on (the way the new one goes). In the line's
// colour and width; one undo step. False: begun on no line (it is drawn as it is).
RDE_INTERNAL b8 fude_zoom_page_sculpt(fude_zoom_page* _page, const fude_ink_point* _p, u32 _n) {
    fude_zoom_scene* _s     = &_page->scene;
    const u32        _frame = _page->draw_frame;
    const f64        _k     = fude_zoom_sim_scale(fude_zoom_page_draw_sim(_page));
    if(_n < 2u || !(_k > 0.0)) {
        return false;
    }
    const f64 _reach = FUDE_ZOOM_PAGE_SCULPT_REACH / _k;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _new_arr = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr_resize(&_new_arr, _n);
    fude_zoom_v2* _new = (fude_zoom_v2*)_new_arr.memory;   // (sized once: it stays put)
    for(u32 _i = 0; _i < _n; _i++) {
        _new[_i] = (fude_zoom_v2){ _page->origin.x + (f64)_p[_i].position.x, _page->origin.y + (f64)_p[_i].position.y };
    }
    // The line it began on: the nearest pen stroke there (a gesture's pieces together).
    rde_arr _found = rde_arr_new(sizeof(u32), _heap), _q = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
    fude_zoom_scene_query(_s, _frame, (fude_zoom_box){ _new[0].x - _reach, _new[0].y - _reach, _new[0].x + _reach, _new[0].y + _reach }, &_found);
    fude_zoom_id _gesture = 0;
    u32 _hit = FUDE_ZOOM_NONE;
    f64 _best = _reach;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_found); _i++) {
        const u32               _object = ((const u32*)_found.memory)[_i];
        const fude_zoom_object* _o      = fude_zoom_scene_object(_s, _object);
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_STROKE || (_o->flags & FUDE_ZOOM_FLAG_MARKER) || !fude_zoom_scene_touchable(_s, _o) || _o->count == 0) {
            continue;
        }
        rde_arr_clear(&_q);
        rde_arr_add_n(&_q, _o->count);
        if(!fude_zoom_scene_points(_s, _object, (fude_zoom_qpoint*)_q.memory)) {
            continue;
        }
        for(u32 _j = 0; _j < _o->count; _j++) {
            const fude_zoom_v2 _a = fude_zoom_scene_point_at(_o, &((const fude_zoom_qpoint*)_q.memory)[_j]);
            const f64 _d = rde_math_hypot_f64(_a.x - _new[0].x, _a.y - _new[0].y);
            if(_d <= _best) {
                _best    = _d;
                _gesture = _o->gesture != 0 ? _o->gesture : _o->id;
                _hit     = _object;
            }
        }
    }
    b8 _done = false;
    if(_gesture != 0) {
        // Its pieces in the order drawn; of its runs, the one begun on (a piece erased between runs: they are lines of
        // their own).
        rde_arr _old = rde_arr_new(sizeof(fude_zoom_v2), _heap), _pieces = rde_arr_new(sizeof(u32), _heap);
        for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
            if((_o->flags & FUDE_ZOOM_FLAG_ALIVE) && _o->kind == FUDE_ZOOM_KIND_STROKE && _o->frame == _frame && (_o->gesture == _gesture || _o->id == _gesture)) {
                rde_arr_add(&_pieces, (any)&_i);
            }
        }
        const u32 _all = (u32)rde_arr_length(&_pieces);
        rde_arr _runs_arr = rde_arr_new(sizeof(fude_zoom_run), _heap);
        rde_arr_resize(&_runs_arr, _all);
        fude_zoom_run* _runs = (fude_zoom_run*)_runs_arr.memory;
        const u32 _run_count = fude_zoom_page_line_runs(_s, (u32*)_pieces.memory, _all, _runs);
        fude_zoom_run _run = { 0u, 0u };
        for(u32 _r = 0; _r < _run_count; _r++) {
            for(u32 _j = _runs[_r].first; _j < _runs[_r].first + _runs[_r].count; _j++) {
                if(((const u32*)_pieces.memory)[_j] == _hit) {
                    _run = _runs[_r];
                }
            }
        }
        rde_arr_free(&_runs_arr);
        // Only that run's pieces from here on.
        if(_run.count > 0 && _run.first > 0) {
            rde_memmove(_pieces.memory, (u32*)_pieces.memory + _run.first, (usize)_run.count * sizeof(u32));
        }
        rde_arr_resize(&_pieces, _run.count);
        _run.first = 0u;
        fude_zoom_page_run_points(_s, (const u32*)_pieces.memory, _run, &_old);
        rde_color _color  = { 0, 0, 0, 0 };
        f32       _radius = 0.0f;
        if(_run.count > 0) {
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, ((const u32*)_pieces.memory)[0]);
            _color  = _o->color;
            _radius = (f32)((f64)_o->radius * _o->scale);
        }
        const u32           _m = (u32)rde_arr_length(&_old);
        const fude_zoom_v2* _s0 = (const fude_zoom_v2*)_old.memory;
        if(_m >= 2u) {
            rde_arr _out = rde_arr_new(sizeof(fude_zoom_v2), _heap);
            fude_zoom_shape_sculpt(_s0, _m, _new, _n, _reach, &_out);
            rde_arr _born = rde_arr_new(sizeof(u32), _heap);
            const u16 _was = _s->layer;
            _s->layer = fude_zoom_scene_object(_s, ((const u32*)_pieces.memory)[0])->layer;
            fude_zoom_page_add_path(_page, _frame, (const fude_zoom_v2*)_out.memory, (u32)rde_arr_length(&_out), false, _color, _radius, &_born);
            _s->layer = _was;
            if(rde_arr_length(&_born) > 0) {
                fude_zoom_box _box = fude_zoom_box_empty();
                for(u32 _j = 0; _j < (u32)rde_arr_length(&_pieces); _j++) {
                    const u32 _piece = ((const u32*)_pieces.memory)[_j];
                    _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, _piece)->box);
                    fude_zoom_scene_set_alive(_s, _piece, false);
                }
                for(u32 _j = 0; _j < (u32)rde_arr_length(&_born); _j++) {
                    _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, ((const u32*)_born.memory)[_j])->box);
                }
                fude_zoom_history_push(_s, _frame, _box, (const u32*)_pieces.memory, (u32)rde_arr_length(&_pieces), (const u32*)_born.memory, (u32)rde_arr_length(&_born));
                _done = true;
            }
            rde_arr_free(&_born);
            rde_arr_free(&_out);
        }
        rde_arr_free(&_old);
        rde_arr_free(&_pieces);
    }
    rde_arr_free(&_found);
    rde_arr_free(&_q);
    rde_arr_free(&_new_arr);
    return _done;
}

// --- Nudge (research §2.2: push a stroke like string) -------------------------------------------------

#define FUDE_ZOOM_PAGE_NUDGE_REACH 28.0   // screen points the pen pushes, strongest at its middle

// A line Nudge has taken: its points copied out to be pushed, the line itself hidden until the pen lifts, when it is
// put back where it was pushed to.
typedef struct {
    u32       object;         // the line taken
    u32       first, count;   // its points in nudge_points (their half-widths in nudge_radii)
    rde_color color;
    f32       radius;         // frame units
    b8        moved;          // pushed at all: only those are put back
    b8        pressure;       // its width varies along it (kept so)
    b8        path;           // a PATH: its points are its nodes (pushed, the curve through them bends)
    u64       bits;           // a path's first number (closed, corners, handles)
    u32       handles;        // a path's handles, from here in nudge_handles
} fude_zoom_nudged;

// A path's numbers in frame units from its nodes and handles as nudging has them.
RDE_INTERNAL u32 fude_zoom_page_nudged_path(const fude_zoom_page* _page, const fude_zoom_nudged* _item, f64* _n) {
    return fude_zoom_path_pack(_item->bits, &((const fude_zoom_v2*)_page->nudge_points.memory)[_item->first],
                               &((const fude_zoom_v2*)_page->nudge_handles.memory)[_item->handles], _item->count, _n);
}

// The pen lines within reach of _at (frame units) not yet taken: their points copied in, the originals hidden.
RDE_INTERNAL void fude_zoom_page_nudge_take(fude_zoom_page* _page, fude_zoom_v2 _at, f64 _reach) {
    fude_zoom_scene* _s = &_page->scene;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _found = rde_arr_new(sizeof(u32), _heap), _q = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
    fude_zoom_scene_query(_s, _page->nudge_frame, (fude_zoom_box){ _at.x - _reach, _at.y - _reach, _at.x + _reach, _at.y + _reach }, &_found);
    while((u32)rde_arr_length(&_page->nudge_lifted) < fude_zoom_scene_object_count(_s)) {
        const u8 _zero = 0u;
        rde_arr_add(&_page->nudge_lifted, (any)&_zero);
    }
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_found); _i++) {
        const u32               _object = ((const u32*)_found.memory)[_i];
        const fude_zoom_object* _o      = fude_zoom_scene_object(_s, _object);
        if(((const u8*)_page->nudge_lifted.memory)[_object] || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || !fude_zoom_scene_touchable(_s, _o)) {
            continue;
        }
        if(_o->kind == FUDE_ZOOM_KIND_SHAPE && _o->channels == FUDE_ZOOM_SHAPE_PATH) {
            // A curve: its nodes (and their handles) in frame units, to push.
            f64 _num[FUDE_ZOOM_SHAPE_NUMBERS];
            u64 _bits;
            fude_zoom_v2 _nodes[FUDE_ZOOM_PATH_NODES], _hs[FUDE_ZOOM_PATH_NODES];
            const u32 _k = fude_zoom_path_unpack(_num, fude_zoom_scene_shape_numbers(_s, _object, _num, FUDE_ZOOM_SHAPE_NUMBERS), &_bits, _nodes, _hs);
            if(_k < 2u) {
                continue;
            }
            const fude_zoom_sim _sim = fude_zoom_object_sim(_o);
            const fude_zoom_v2  _o0  = fude_zoom_sim_apply(_sim, (fude_zoom_v2){ 0.0, 0.0 });
            fude_zoom_nudged _n = { _object, (u32)rde_arr_length(&_page->nudge_points), _k, _o->color, (f32)((f64)_o->radius * _o->scale), false, false,
                                    true, _bits, (u32)rde_arr_length(&_page->nudge_handles) };
            for(u32 _j = 0; _j < _k; _j++) {
                const fude_zoom_v2 _p  = fude_zoom_sim_apply(_sim, _nodes[_j]);
                const fude_zoom_v2 _hp = fude_zoom_sim_apply(_sim, _hs[_j]);
                const fude_zoom_v2 _h  = { _hp.x - _o0.x, _hp.y - _o0.y };   // (a direction: turned and scaled, not moved)
                rde_arr_add(&_page->nudge_points, (any)&_p);
                rde_arr_add(&_page->nudge_radii, (any)&_n.radius);
                rde_arr_add(&_page->nudge_handles, (any)&_h);
            }
            rde_arr_add(&_page->nudge_items, &_n);
            ((u8*)_page->nudge_lifted.memory)[_object] = 1u;
            continue;
        }
        if(_o->kind != FUDE_ZOOM_KIND_STROKE || (_o->flags & FUDE_ZOOM_FLAG_MARKER) || _o->count < 2u) {
            continue;
        }
        rde_arr_clear(&_q);
        rde_arr_add_n(&_q, _o->count);
        if(!fude_zoom_scene_points(_s, _object, (fude_zoom_qpoint*)_q.memory)) {
            continue;
        }
        fude_zoom_nudged _n = { _object, (u32)rde_arr_length(&_page->nudge_points), 0u, _o->color, (f32)((f64)_o->radius * _o->scale), false,
                                (_o->flags & FUDE_ZOOM_FLAG_PRESSURE) != 0u, false, 0u, 0u };
        // Its points (and their widths), filled in where they are far apart: pushed, it bends like string, not at a few
        // points.
        const f64 _step = _reach / 6.0;
        const fude_zoom_qpoint* _qp = (const fude_zoom_qpoint*)_q.memory;
        fude_zoom_v2 _last   = fude_zoom_scene_point_at(_o, &_qp[0]);
        f32          _last_r = fude_zoom_scene_radius_at(_o, &_qp[0]);
        rde_arr_add(&_page->nudge_points, (any)&_last);
        rde_arr_add(&_page->nudge_radii, (any)&_last_r);
        _n.count = 1u;
        for(u32 _j = 1; _j < _o->count; _j++) {
            const fude_zoom_v2 _p = fude_zoom_scene_point_at(_o, &_qp[_j]);
            const f32          _r = fude_zoom_scene_radius_at(_o, &_qp[_j]);
            // (every point of its own kept; the filling stops when there would be too many)
            const u32 _fill = _n.count < 16384u ? (u32)rde_math_min_f64(rde_math_floor_f64(rde_math_hypot_f64(_p.x - _last.x, _p.y - _last.y) / _step), 256.0) : 0u;
            for(u32 _f = 1; _f <= _fill; _f++) {
                const f64 _t = (f64)_f / (f64)(_fill + 1u);
                const fude_zoom_v2 _m  = { _last.x + (_p.x - _last.x) * _t, _last.y + (_p.y - _last.y) * _t };
                const f32          _mr = _last_r + (_r - _last_r) * (f32)_t;
                rde_arr_add(&_page->nudge_points, (any)&_m);
                rde_arr_add(&_page->nudge_radii, (any)&_mr);
                _n.count++;
            }
            rde_arr_add(&_page->nudge_points, (any)&_p);
            rde_arr_add(&_page->nudge_radii, (any)&_r);
            _n.count++;
            _last   = _p;
            _last_r = _r;
        }
        rde_arr_add(&_page->nudge_items, &_n);
        ((u8*)_page->nudge_lifted.memory)[_object] = 1u;
    }
    rde_arr_free(&_found);
    rde_arr_free(&_q);
}

// See page_parts.h. Nudge works in the camera's frame; while it does, the renderer hides the lines taken (its lifted
// points at nudge_lifted instead of the selection's).
void fude_zoom_page_nudge_down(fude_zoom_page* _page, rde_vec_2F _screen) {
    fude_zoom_scene* _s = &_page->scene;
    if(!rde_arr_is_inited(&_page->nudge_items)) {
        rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
        _page->nudge_items  = rde_arr_new(sizeof(fude_zoom_nudged), _heap);
        _page->nudge_points = rde_arr_new(sizeof(fude_zoom_v2), _heap);
        _page->nudge_radii  = rde_arr_new(sizeof(f32), _heap);
        _page->nudge_handles = rde_arr_new(sizeof(fude_zoom_v2), _heap);
        _page->nudge_lifted = rde_arr_new(sizeof(u8), _heap);
    }
    rde_arr_clear(&_page->nudge_items);
    rde_arr_clear(&_page->nudge_points);
    rde_arr_clear(&_page->nudge_radii);
    rde_arr_clear(&_page->nudge_handles);
    if(rde_arr_length(&_page->nudge_lifted) > 0) {
        rde_memset(_page->nudge_lifted.memory, 0, rde_arr_length(&_page->nudge_lifted));
    }
    _page->nudging     = true;
    _page->nudge_frame = _s->camera.frame;
    _page->nudge_at    = fude_zoom_sim_apply(fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _page->nudge_frame)), (fude_zoom_v2){ _screen.x, _screen.y });
    _page->renderer.lifted = &_page->nudge_lifted;
    const f64 _k = fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, _page->nudge_frame));
    fude_zoom_page_nudge_take(_page, _page->nudge_at, FUDE_ZOOM_PAGE_NUDGE_REACH / rde_math_max_f64(_k, 1e-300));
}

// The pen moved: what is within reach pushed along with it, most at its middle, nothing at its edge.
void fude_zoom_page_nudge_moved(fude_zoom_page* _page, rde_vec_2F _screen) {
    const fude_zoom_sim _to = fude_zoom_page_frame_sim(_page, _page->nudge_frame);
    const f64           _k  = rde_math_max_f64(fude_zoom_sim_scale(_to), 1e-300);
    const f64           _r  = FUDE_ZOOM_PAGE_NUDGE_REACH / _k;
    const fude_zoom_v2  _at = fude_zoom_sim_apply(fude_zoom_sim_inverse(_to), (fude_zoom_v2){ _screen.x, _screen.y });
    // In small steps, so a quick flick pushes as a slow one would.
    const f64 _len   = rde_math_hypot_f64(_at.x - _page->nudge_at.x, _at.y - _page->nudge_at.y);
    const u32 _steps = (u32)rde_math_min_f64(rde_math_ceil_f64(_len / (_r * 0.25)), 64.0);
    for(u32 _step = 1; _step <= _steps; _step++) {
        const fude_zoom_v2 _from = _page->nudge_at;
        const fude_zoom_v2 _next = { _page->nudge_at.x + (_at.x - _page->nudge_at.x) / (f64)(_steps - _step + 1u), _page->nudge_at.y + (_at.y - _page->nudge_at.y) / (f64)(_steps - _step + 1u) };
        fude_zoom_page_nudge_take(_page, _from, _r);
        const fude_zoom_v2 _d = { _next.x - _from.x, _next.y - _from.y };
        fude_zoom_v2*      _p = (fude_zoom_v2*)_page->nudge_points.memory;
        fude_zoom_nudged*  _items = (fude_zoom_nudged*)_page->nudge_items.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->nudge_items); _i++) {
            for(u32 _j = _items[_i].first; _j < _items[_i].first + _items[_i].count; _j++) {
                const f64 _t = rde_math_hypot_f64(_p[_j].x - _from.x, _p[_j].y - _from.y) / _r;
                if(_t < 1.0) {
                    const f64 _w = (1.0 - _t * _t) * (1.0 - _t * _t);
                    _p[_j].x += _d.x * _w;
                    _p[_j].y += _d.y * _w;
                    _items[_i].moved = true;
                }
            }
        }
        _page->nudge_at = _next;
    }
}

// See page_parts.h. Each line pushed is added again where it is now (a curve through its pushed nodes, a stroke with
// its widths) and the old one let go; connectors joined to a curve follow it.
void fude_zoom_page_nudge_up(fude_zoom_page* _page) {
    if(!_page->nudging) {
        return;
    }
    _page->nudging = false;
    _page->renderer.lifted = &_page->selection.lifted;
    fude_zoom_scene*      _s    = &_page->scene;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _died = rde_arr_new(sizeof(u32), _heap), _born = rde_arr_new(sizeof(u32), _heap);
    fude_zoom_box _box = fude_zoom_box_empty();
    const fude_zoom_nudged* _items = (const fude_zoom_nudged*)_page->nudge_items.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->nudge_items); _i++) {
        if(!_items[_i].moved) {
            continue;
        }
        const u32 _was_born = (u32)rde_arr_length(&_born);
        const u16 _was = _s->layer;
        _s->layer = fude_zoom_scene_object(_s, _items[_i].object)->layer;
        if(_items[_i].path) {
            // A curve: again through its nodes where they were pushed (in its frame's own units), as it was otherwise.
            const fude_zoom_object _old = *fude_zoom_scene_object(_s, _items[_i].object);
            f64 _num[FUDE_ZOOM_SHAPE_NUMBERS];
            const u32 _count = fude_zoom_page_nudged_path(_page, &_items[_i], _num);
            const u32 _made  = fude_zoom_scene_add_shape_fill(_s, _page->nudge_frame, (fude_zoom_place){ { 0.0, 0.0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_PATH, _num, _count,
                                                              _old.color, _items[_i].radius, (u8)(_old.flags & (FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN)), _old.fill, _old.z);
            _s->layer = _was;
            rde_arr_add(&_born, (any)&_made);
            _box = fude_zoom_box_union(_box, _old.box);
            fude_zoom_scene_set_alive(_s, _items[_i].object, false);
            rde_arr_add(&_died, (any)&_items[_i].object);
            fude_zoom_page_rebind(_page, &_items[_i].object, &_made, 1u, &_died, &_born);   // (connectors joined to it follow it)
            continue;
        }
        fude_zoom_page_add_path_radii(_page, _page->nudge_frame, &((const fude_zoom_v2*)_page->nudge_points.memory)[_items[_i].first],
                                      _items[_i].pressure ? &((const f32*)_page->nudge_radii.memory)[_items[_i].first] : NULL, _items[_i].count, false,
                                      _items[_i].color, _items[_i].radius, &_born);   // (its pressure kept)
        _s->layer = _was;
        if((u32)rde_arr_length(&_born) > _was_born) {
            _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, _items[_i].object)->box);
            fude_zoom_scene_set_alive(_s, _items[_i].object, false);
            rde_arr_add(&_died, (any)&_items[_i].object);
        }
    }
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_born); _i++) {
        _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, ((const u32*)_born.memory)[_i])->box);
    }
    if(rde_arr_length(&_born) > 0) {
        fude_zoom_history_push(_s, _page->nudge_frame, _box, (const u32*)_died.memory, (u32)rde_arr_length(&_died), (const u32*)_born.memory, (u32)rde_arr_length(&_born));
    }
    rde_arr_free(&_died);
    rde_arr_free(&_born);
    rde_arr_clear(&_page->nudge_items);
    rde_arr_clear(&_page->nudge_points);
    rde_arr_clear(&_page->nudge_radii);
    rde_arr_clear(&_page->nudge_handles);
}

// While nudging: the lines pushed, where they are now, and the pen's reach.
void fude_zoom_page_render_nudge(fude_zoom_page* _page) {
    if(!_page->nudging) {
        return;
    }
    const fude_zoom_sim _to = fude_zoom_page_frame_sim(_page, _page->nudge_frame);
    const f64           _k  = fude_zoom_sim_scale(_to);
    const fude_zoom_nudged* _items = (const fude_zoom_nudged*)_page->nudge_items.memory;
    const fude_zoom_v2*     _p     = (const fude_zoom_v2*)_page->nudge_points.memory;
    const f32*              _pr    = (const f32*)_page->nudge_radii.memory;
    rde_arr _sp_arr = rde_arr_new(sizeof(rde_vec_2F), rde_memory_allocator_get_default_std()), _sr_arr = rde_arr_new(sizeof(f32), rde_memory_allocator_get_default_std());
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->nudge_items); _i++) {
        if(_items[_i].path) {
            // A curve: through its nodes where they are now.
            f64 _num[FUDE_ZOOM_SHAPE_NUMBERS];
            const u32 _count = fude_zoom_page_nudged_path(_page, &_items[_i], _num);
            rde_arr _out = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
            b8 _closed = false;
            fude_zoom_shape_outline(FUDE_ZOOM_SHAPE_PATH, _num, _count, 24u * _items[_i].count, &_out, &_closed);
            const u32 _m = (u32)rde_arr_length(&_out);
            rde_arr_resize(&_sp_arr, (usize)_m + 1u);
            rde_arr_resize(&_sr_arr, (usize)_m + 1u);
            rde_vec_2F* _cp = (rde_vec_2F*)_sp_arr.memory;
            f32*        _cr = (f32*)_sr_arr.memory;
            for(u32 _j = 0; _j <= _m && _m > 0u; _j++) {
                const fude_zoom_v2 _q = fude_zoom_sim_apply(_to, ((const fude_zoom_v2*)_out.memory)[_j % _m]);
                _cp[_j] = (rde_vec_2F){ (f32)_q.x, (f32)_q.y };
                _cr[_j] = rde_math_max_f32(_items[_i].radius * (f32)_k, 0.75f);
            }
            if(_m >= 2u) {
                rde_rendering_2d_draw_stroke(_cp, _cr, _closed ? _m + 1u : _m, fude_theme_resolve(_items[_i].color));
            }
            rde_arr_free(&_out);
            continue;
        }
        const u32 _n = _items[_i].count;
        rde_arr_resize(&_sp_arr, _n);
        rde_arr_resize(&_sr_arr, _n);
        rde_vec_2F* _sp = (rde_vec_2F*)_sp_arr.memory;
        f32*        _sr = (f32*)_sr_arr.memory;
        for(u32 _j = 0; _j < _n; _j++) {
            const fude_zoom_v2 _q = fude_zoom_sim_apply(_to, _p[_items[_i].first + _j]);
            _sp[_j] = (rde_vec_2F){ (f32)_q.x, (f32)_q.y };
            _sr[_j] = rde_math_max_f32(_pr[_items[_i].first + _j] * (f32)_k, 0.75f);
        }
        rde_rendering_2d_draw_stroke(_sp, _sr, _n, fude_theme_resolve(_items[_i].color));
    }
    rde_arr_free(&_sp_arr);
    rde_arr_free(&_sr_arr);
    const fude_zoom_v2 _c = fude_zoom_sim_apply(_to, _page->nudge_at);
    rde_color _ring = fude_theme_active()->accent;
    _ring.a = 120u;
    rde_rendering_2d_draw_circle_with_border((rde_vec_2F){ (f32)_c.x, (f32)_c.y }, (f32)FUDE_ZOOM_PAGE_NUDGE_REACH, 48u, (rde_color){ 0, 0, 0, 0 }, 1.5f, _ring, NULL);
}

// The Saw (fude_zoom_page_draw_up's, when it is on): the stroke's points in the drawing's frame (along an
// instrument's straight edge, only its two ends) made a cut (fude_zoom_page_saw_line), closed when it was drawn
// as a loop, the tail drawn over its start dropped. The capture is emptied.
RDE_INTERNAL void fude_zoom_page_draw_saw(fude_zoom_page* _page, const fude_ink_point* _p, u32 _n) {
    rde_arr _line = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    f32 _r = 0.0f;
    for(u32 _i = 0; _i < _n; _i++) {
        _r = _p[_i].radius > _r ? _p[_i].radius : _r;
        if(!(_page->ruled && _page->ruled_straight) || _i == 0 || _i + 1u == _n) {
            const fude_zoom_v2 _q = { _page->origin.x + (f64)_p[_i].position.x, _page->origin.y + (f64)_p[_i].position.y };
            rde_arr_add(&_line, (any)&_q);
        }
    }
    const fude_zoom_v2* _lp = (const fude_zoom_v2*)_line.memory;
    const u32 _m = (u32)rde_arr_length(&_line);
    const f64 _k = fude_zoom_sim_scale(fude_zoom_page_draw_sim(_page));
    const f64 _close = rde_math_max_f64(3.0 * (f64)_r, 8.0 / rde_math_max_f64(_k, 1e-300));
    const b8 _loop = _m >= 8u && rde_math_hypot_f64(_lp[0].x - _lp[_m - 1u].x, _lp[0].y - _lp[_m - 1u].y) <= _close;
    u32 _keep = _m;
    while(_loop && _keep > 6u && rde_math_hypot_f64(_lp[0].x - _lp[_keep - 1u].x, _lp[0].y - _lp[_keep - 1u].y) <= _close * 0.5) {
        _keep--;   // (a loop drawn by hand ends over its start: that tail is its start again)
    }
    fude_zoom_page_saw_line(_page, _page->draw_frame, _lp, _keep, _loop);
    rde_arr_free(&_line);
    fude_zoom_page_capture_reset(_page);
    _page->snapped = false;
}

// Drawn along an instrument's straight edge (fude_zoom_page_draw_up's): an exact line from the stroke's first point
// to its last, as thick as its thickest, one undo step and its length's chip (none when shorter than 2 points on
// the screen). The capture is emptied.
RDE_INTERNAL void fude_zoom_page_draw_ruled_line(fude_zoom_page* _page, fude_zoom_scene* _s, const fude_ink_point* _p, u32 _n, rde_color _stroke_color) {
    const fude_zoom_v2 _a = { _page->origin.x + (f64)_p[0].position.x, _page->origin.y + (f64)_p[0].position.y };
    const fude_zoom_v2 _b = { _page->origin.x + (f64)_p[_n - 1u].position.x, _page->origin.y + (f64)_p[_n - 1u].position.y };
    const f64 _k = fude_zoom_sim_scale(fude_zoom_page_draw_sim(_page));
    if(rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y) * _k >= 2.0) {
        f32 _r = 0.0f;
        for(u32 _i = 0; _i < _n; _i++) {
            _r = _p[_i].radius > _r ? _p[_i].radius : _r;
        }
        const f64 _num[2] = { _b.x - _a.x, _b.y - _a.y };
        const u32 _line = fude_zoom_scene_add_shape(_s, _page->draw_frame, (fude_zoom_place){ _a, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, _num, 2u, _stroke_color, _r, 0u, 0);
        fude_zoom_history_push(_s, _page->draw_frame, fude_zoom_scene_object(_s, _line)->box, NULL, 0, &_line, 1);
        _page->line_chip_object = _line;
        _page->line_chip_until  = rde_engine_get_time_now() + FUDE_ZOOM_PAGE_LINE_CHIP;
    }
    fude_zoom_page_capture_reset(_page);
    _page->snapped = false;
}

// Drawn along a round edge (fude_zoom_page_draw_up's): an exact arc through the stroke's points, a whole circle
// when it went all the way round, one undo step, the capture emptied. False: no arc fits them, or it is shorter
// than 2 points on the screen (the stroke goes on as drawn).
RDE_INTERNAL b8 fude_zoom_page_draw_ruled_arc(fude_zoom_page* _page, fude_zoom_scene* _s, const fude_ink_point* _p, u32 _n, rde_color _stroke_color) {
    const f64 _k = fude_zoom_sim_scale(fude_zoom_page_draw_sim(_page));
    rde_arr _q_arr = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_q_arr, _n);
    fude_zoom_v2* _q = (fude_zoom_v2*)_q_arr.memory;
    f32 _r = 0.0f;
    for(u32 _i = 0; _i < _n; _i++) {
        _q[_i] = (fude_zoom_v2){ _page->origin.x + (f64)_p[_i].position.x, _page->origin.y + (f64)_p[_i].position.y };
        _r = _p[_i].radius > _r ? _p[_i].radius : _r;
    }
    fude_zoom_v2 _c;
    f64 _radius, _from, _sweep;
    const b8 _fit = fude_zoom_shape_fit_arc(_q, _n, 0.75 / rde_math_max_f64(_k, 1e-300), &_c, &_radius, &_from, &_sweep);
    rde_arr_free(&_q_arr);
    if(_fit && rde_math_abs_f64(_sweep) * _radius * _k >= 2.0) {
        const b8  _whole = rde_math_abs_f64(_sweep) >= 2.0 * RDE_PI - 0.02;
        const f64 _num[3] = { _radius, _whole ? _radius : _from, _sweep };
        const u32 _made = fude_zoom_scene_add_shape(_s, _page->draw_frame, (fude_zoom_place){ _c, 0.0, 1.0 }, _whole ? FUDE_ZOOM_SHAPE_ELLIPSE : FUDE_ZOOM_SHAPE_ARC,
                                                    _num, _whole ? 2u : 3u, _stroke_color, _r, 0u, 0);
        fude_zoom_history_push(_s, _page->draw_frame, fude_zoom_scene_object(_s, _made)->box, NULL, 0, &_made, 1);
        fude_zoom_page_capture_reset(_page);
        _page->snapped = false;
        return true;
    }
    return false;
}

// The captured stroke put into the canvas (fude_zoom_page_draw_up's, when nothing else took it): its points as
// whole quanta from the first, in pieces of FUDE_ZOOM_STROKE_MAX (each sharing its last point with the next), one
// undo step, the capture emptied; a snapped shape then takes its place in a step of its own.
RDE_INTERNAL void fude_zoom_page_draw_stroke(fude_zoom_page* _page, fude_zoom_scene* _s, const fude_ink_stroke* _st, const fude_ink_point* _p, u32 _n, rde_color _stroke_color) {
    const i8  _q    = fude_zoom_quantum_for(_page->draw_z);
    const f64 _grid = rde_math_ldexp_f64(1.0, _q);
    f32 _base = 0.0f;
    for(u32 _i = 0; _i < _n; _i++) {
        _base = _p[_i].radius > _base ? _p[_i].radius : _base;
    }
    b8 _varies = false;
    for(u32 _i = 0; _i < _n && !_varies; _i++) {
        _varies = rde_math_abs_f32(_p[_i].radius - _base) > _base * 1e-4f;
    }
    u8 _flags = 0;
    if(_st->from_pen) { _flags |= FUDE_ZOOM_FLAG_FROM_PEN; }
    if(_st->marker)   { _flags |= FUDE_ZOOM_FLAG_MARKER; }
    if(_varies)       { _flags |= FUDE_ZOOM_FLAG_PRESSURE; }
    const u8 _channels = (u8)(FUDE_ZOOM_CHANNEL_TIME | (_varies ? FUDE_ZOOM_CHANNEL_PRESSURE : 0u));

    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _all_arr = rde_arr_new(sizeof(fude_zoom_qpoint), _heap), _pq_arr = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
    rde_arr_resize(&_all_arr, _n);
    fude_zoom_qpoint*     _all  = (fude_zoom_qpoint*)_all_arr.memory;   // (sized once: it stays put)
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _share = _base > 0.0f ? (f64)_p[_i].radius / (f64)_base : 1.0;
        _all[_i] = (fude_zoom_qpoint){
            (i32)rde_math_round_to_i64_f64(((f64)_p[_i].position.x - (f64)_p[0].position.x) / _grid),
            (i32)rde_math_round_to_i64_f64(((f64)_p[_i].position.y - (f64)_p[0].position.y) / _grid),
            (u16)rde_math_round_to_i64_f64(_share * 1023.0),
            (u32)rde_math_round_to_i64_f64((f64)_p[_i].time * 1000.0),
        };
    }
    const fude_zoom_v2 _t0 = { _page->origin.x + (f64)_p[0].position.x, _page->origin.y + (f64)_p[0].position.y };
    // Its pieces, every one (a stroke has as many as its points need: FUDE_ZOOM_STROKE_MAX points each), so all of
    // them are in its undo step, and all go when it snaps to a shape.
    rde_arr       _born_arr = rde_arr_new(sizeof(u32), _heap);
    u32           _pieces  = 0;
    fude_zoom_id  _gesture = 0;
    fude_zoom_box _box     = fude_zoom_box_empty();
    u32           _from    = 0;
    for(;;) {
        u32 _to = _from + FUDE_ZOOM_STROKE_MAX - 1u;
        _to = _to > _n - 1u ? _n - 1u : _to;
        const u32 _count = _to - _from + 1u;
        // The piece's own first point is its translation; its points from it.
        rde_arr_resize(&_pq_arr, _count);
        fude_zoom_qpoint* _pq = (fude_zoom_qpoint*)_pq_arr.memory;
        for(u32 _i = 0; _i < _count; _i++) {
            _pq[_i]       = _all[_from + _i];
            _pq[_i].x    -= _all[_from].x;
            _pq[_i].y    -= _all[_from].y;
            _pq[_i].time -= _all[_from].time;
        }
        const fude_zoom_v2 _t = { _t0.x + (f64)_all[_from].x * _grid, _t0.y + (f64)_all[_from].y * _grid };
        const u32 _o = fude_zoom_scene_add_stroke(_s, _page->draw_frame, _t, _q, _pq, _count, _channels, _st->color, _base,
                                                  (u8)(_flags | (_pieces > 0 ? FUDE_ZOOM_FLAG_CONTINUES : 0u)), _gesture, 0);
        if(_pieces == 0) {
            _gesture = fude_zoom_scene_object(_s, _o)->id;
        }
        _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, _o)->box);
        rde_arr_add(&_born_arr, (any)&_o);
        _pieces++;
        if(_to >= _n - 1u) {
            break;
        }
        _from = _to;
    }
    rde_arr_free(&_pq_arr);
    rde_arr_free(&_all_arr);
    const u32* _born = (const u32*)_born_arr.memory;
    fude_zoom_history_push(_s, _page->draw_frame, _box, NULL, 0, _born, _pieces);
    fude_zoom_page_capture_reset(_page);

    // Snapped: the shape takes the stroke's place, a step of its own — the
    // first undo brings the stroke back as it was drawn.
    if(_page->snapped) {
        _page->snapped = false;
        const fude_zoom_shape_fit _f = fude_zoom_page_fit_now(_page);
        for(u32 _i = 0; _i < _pieces; _i++) {
            fude_zoom_scene_set_alive(_s, _born[_i], false);
        }
        const u32 _shape = fude_zoom_page_add_fit(_page, _page->draw_frame, fude_zoom_page_draw_sim(_page), &_f, _stroke_color, _base, 0);
        fude_zoom_history_push(_s, _page->draw_frame, fude_zoom_scene_object(_s, _shape)->box, _born, _pieces, &_shape, 1);
    }
    rde_arr_free(&_born_arr);
}

// See page_parts.h. On the rope, the line first catches up with the pen. Then the first of these that applies: the Saw
// cuts with the line; along an instrument's straight edge, an exact line; along a round edge, an exact arc or circle;
// Sculpt redoes the line it began on. Otherwise the captured stroke goes into the canvas: its points as whole quanta
// from the first, in pieces of FUDE_ZOOM_STROKE_MAX (each sharing its last point with the next, so they join), one
// undo step; a snapped shape then takes its place in a step of its own.
// Each of those, and the stroke put in, is a step above (fude_zoom_page_draw_*).
void fude_zoom_page_draw_up(fude_zoom_page* _page) {
    if(!_page->drawing) {
        return;
    }
    _page->drawing = false;
    fude_ink* _c = &_page->capture;
    // On the rope, the line catches up with the pen (Krita's finish line).
    if(_page->stroke_smooth == FUDE_ZOOM_SMOOTH_ROPE && !_page->snapped) {
        const fude_zoom_v2 _pen  = fude_zoom_camera_to_frame(&_page->scene.camera, (fude_zoom_v2){ _page->pen_now.x, _page->pen_now.y });
        const fude_zoom_v2 _to   = { _pen.x - _page->origin.x, _pen.y - _page->origin.y };
        const fude_zoom_v2 _tip  = _page->rope_tip;
        const f64          _len  = rde_math_hypot_f64(_to.x - _tip.x, _to.y - _tip.y);
        const u32          _gaps = (u32)rde_math_min_f64(rde_math_ceil_f64(_len * _page->draw_z / FUDE_ZOOM_SMOOTH_CATCH_STEP), 4096.0);
        for(u32 _i = 1; _i <= _gaps; _i++) {
            const f64 _k = (f64)_i / (f64)_gaps;
            fude_ink_extend(_c, (rde_vec_2F){ (f32)(_tip.x + (_to.x - _tip.x) * _k), (f32)(_tip.y + (_to.y - _tip.y) * _k) });
        }
    }
    fude_zoom_page_smooth_tail_as(_page, true);   // the smoothed end, not bent to the pen
    fude_ink_end(_c);
    if(fude_ink_stroke_count(_c) == 0) {
        return;
    }
    const fude_ink_stroke* _st = fude_ink_stroke_at(_c, fude_ink_stroke_count(_c) - 1u);
    const fude_ink_point*  _p  = fude_ink_stroke_points(_c, _st);
    const u32              _n  = _st->point_count;
    const rde_color        _stroke_color = _st->color;
    if(_n == 0 || !_st->alive) {
        fude_zoom_page_capture_reset(_page);
        _page->snapped = false;
        return;
    }
    fude_zoom_scene* _s = &_page->scene;
    // The Saw: what was drawn cuts boards (along an instrument's straight edge: exactly from where it began to where it
    // ended).
    if(_page->saw && !_st->marker && _n >= 2u) {
        fude_zoom_page_draw_saw(_page, _p, _n);
        return;
    }
    // Along an instrument's straight edge: an exact line, from where it began to where it ended.
    if(_page->ruled && _page->ruled_straight && _n >= 2u && !_st->marker) {
        fude_zoom_page_draw_ruled_line(_page, _s, _p, _n, _stroke_color);
        return;
    }
    // Along a round edge (a compass, the protractor's, a corner gauge's round): an exact arc — a whole
    // circle when it went all the way round — from the points' own circle (they are on it: the edge put them there).
    if(_page->ruled && _page->ruled_round && _n >= 3u && !_st->marker) {
        if(fude_zoom_page_draw_ruled_arc(_page, _s, _p, _n, _stroke_color)) {
            return;
        }
    }
    // Sculpting: begun on a line, that line redone instead.
    if(_page->sculpt && !_st->marker && !_page->snapped && fude_zoom_page_sculpt(_page, _p, _n)) {
        fude_zoom_page_capture_reset(_page);
        return;
    }
    fude_zoom_page_draw_stroke(_page, _s, _st, _p, _n, _stroke_color);
}

// A stroke that turned out to be a gesture (a second finger): never drawn.
void fude_zoom_page_draw_cancel(fude_zoom_page* _page) {
    fude_zoom_page_nudge_cancel(_page);
    _page->drawing = false;
    _page->snapped = false;
    fude_ink_end(&_page->capture);
    fude_zoom_page_capture_reset(_page);
}

// --- the pen: erasing ------------------------------------------------------------------------------

// Rubs out along the pen's move from _from to _to (screen points) in every frame the tools reach, the eraser's radius
// taken into each frame's units.
RDE_INTERNAL void fude_zoom_page_erase_step(fude_zoom_page* _page, rde_vec_2F _from, rde_vec_2F _to) {
    rde_arr_clear(&_page->editable);
    fude_zoom_render_editable(&_page->renderer, &_page->scene, &_page->editable);
    const fude_zoom_visible* _v = (const fude_zoom_visible*)_page->editable.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->editable); _i++) {
        const fude_zoom_sim _back  = fude_zoom_sim_inverse(_v[_i].to_screen);
        const f64           _scale = fude_zoom_sim_scale(_v[_i].to_screen);
        const fude_zoom_v2  _a     = fude_zoom_sim_apply(_back, (fude_zoom_v2){ _from.x, _from.y });
        const fude_zoom_v2  _b     = fude_zoom_sim_apply(_back, (fude_zoom_v2){ _to.x, _to.y });
        fude_zoom_erase_step(&_page->scene, &_page->eraser, _v[_i].frame, _a, _b, (f64)_page->app->ink->eraser_radius / _scale, 1.0 / _scale);
    }
}

// See page_parts.h.
void fude_zoom_page_erase_down(fude_zoom_page* _page, rde_vec_2F _screen) {
    fude_zoom_erase_begin(&_page->eraser, (FUDE_ZOOM_ERASE_)_page->erase_mode);
    _page->erasing    = true;
    _page->erase_last = _screen;
    fude_zoom_page_erase_step(_page, _screen, _screen);
}

// See page_parts.h.
void fude_zoom_page_erase_moved(fude_zoom_page* _page, rde_vec_2F _screen) {
    fude_zoom_page_erase_step(_page, _page->erase_last, _screen);
    _page->erase_last = _screen;
}

// See page_parts.h: the eraser's step is pushed in the camera's frame, with the view as its box.
void fude_zoom_page_erase_up(fude_zoom_page* _page) {
    if(!_page->erasing) {
        return;
    }
    _page->erasing = false;
    fude_zoom_erase_end(&_page->scene, &_page->eraser, _page->scene.camera.frame, fude_zoom_page_view(_page));
}

// --- offsets (fill.h): kerf, a router's bit ---------------------------------------------------

// A path (frame units) as a stroke of thin, even line, in pieces of
// FUDE_ZOOM_STROKE_MAX joined as one gesture; its pieces into _born. With _radii
// (each point's half-width, frame units), its width varies so: their most its
// width, each point's share of it its pressure.
RDE_INTERNAL void fude_zoom_page_add_path_radii(fude_zoom_page* _page, u32 _frame, const fude_zoom_v2* _p, const f32* _radii, u32 _n, b8 _closed,
                                                rde_color _color, f32 _radius, rde_arr* _born) {
    f32 _most = 0.0f;
    for(u32 _i = 0; _radii != NULL && _i < _n; _i++) {
        _most = _radii[_i] > _most ? _radii[_i] : _most;
    }
    if(_radii != NULL && _most > 0.0f) {
        _radius = _most;
    } else {
        _radii = NULL;
    }
    fude_zoom_page_add_path_with(_page, _frame, _p, _radii, _n, _closed, _color, _radius, _born);
}

// See page_parts.h.
void fude_zoom_page_add_path(fude_zoom_page* _page, u32 _frame, const fude_zoom_v2* _p, u32 _n, b8 _closed, rde_color _color, f32 _radius, rde_arr* _born) {
    fude_zoom_page_add_path_with(_page, _frame, _p, NULL, _n, _closed, _color, _radius, _born);
}

// Adds the path as strokes, in pieces of FUDE_ZOOM_STROKE_MAX points, each sharing its last point with the next and
// all one gesture; closed, it ends at its first point again. Its points are quantized as a stroke drawn at four times
// the frame's zoom on the screen now would be. With _radii, each point's share of _radius is its pressure.
RDE_INTERNAL void fude_zoom_page_add_path_with(fude_zoom_page* _page, u32 _frame, const fude_zoom_v2* _p, const f32* _radii, u32 _n, b8 _closed,
                                               rde_color _color, f32 _radius, rde_arr* _born) {
    fude_zoom_scene* _s = &_page->scene;
    const u32 _m = _closed ? _n + 1u : _n;
    if(_m < 2u) {
        return;
    }
    const i8  _q    = fude_zoom_quantum_for(fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, _frame)) * 4.0);
    const f64 _grid = rde_math_ldexp_f64(1.0, _q);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _qp_arr = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
    rde_arr_resize(&_qp_arr, FUDE_ZOOM_STROKE_MAX);
    fude_zoom_qpoint* _qp = (fude_zoom_qpoint*)_qp_arr.memory;   // (sized once: it stays put)
    fude_zoom_id _gesture = 0;
    for(u32 _from = 0; _from + 1u < _m;) {
        const u32 _to = _from + FUDE_ZOOM_STROKE_MAX - 1u < _m - 1u ? _from + FUDE_ZOOM_STROKE_MAX - 1u : _m - 1u;
        const fude_zoom_v2 _t = _p[_from % _n];
        for(u32 _i = _from; _i <= _to; _i++) {
            const fude_zoom_v2 _at = _p[_i % _n];
            const u16 _pressure = _radii != NULL ? (u16)rde_math_min_f32(rde_math_max_f32(rde_math_round_f32(_radii[_i % _n] / _radius * 1023.0f), 1.0f), 1023.0f) : 1023u;
            _qp[_i - _from] = (fude_zoom_qpoint){ (i32)rde_math_round_to_i64_f64((_at.x - _t.x) / _grid), (i32)rde_math_round_to_i64_f64((_at.y - _t.y) / _grid), _pressure, 0u };
        }
        const u8  _with = _radii != NULL ? FUDE_ZOOM_FLAG_PRESSURE : 0u;
        const u32 _o = fude_zoom_scene_add_stroke(_s, _frame, _t, _q, _qp, _to - _from + 1u, _radii != NULL ? FUDE_ZOOM_CHANNEL_PRESSURE : 0u, _color, _radius,
                                                  (u8)(_with | (_gesture != 0 ? FUDE_ZOOM_FLAG_CONTINUES : 0u)), _gesture, 0);
        if(_gesture == 0) {
            _gesture = fude_zoom_scene_object(_s, _o)->id;
        }
        rde_arr_add(_born, (any)&_o);
        _from = _to;
    }
    rde_arr_free(&_qp_arr);
}
