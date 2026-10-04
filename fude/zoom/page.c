// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page.h"
#include "drawing/app/app.h"
#include "drawing/app/ui.h"
#include "drawing/app/page.h"
#include "drawing/ink/notes.h"
#include "drawing/widgets/notice.h"
#include "drawing/widgets/pagemenu.h"
#include "drawing/widgets/toolbar.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "drawing/widgets/draw.h"
#include "drawing/widgets/icons.h"
#include "drawing/widgets/kit.h"
#include "drawing/doc/import.h"
#include "drawing/doc/pdf.h"
#include "zoom/smooth.h"
#include "zoom/fill.h"
#include "zoom/export.h"
#include "drawing/app/session.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See page.h.
// ===========================================================================

#define FUDE_ZOOM_PAGE_TAP_TIME   0.35    // a multi-finger tap: every finger up within this long...
#define FUDE_ZOOM_PAGE_TAP_SLOP   16.0f   // ...none moved further than this (screen units)
#define FUDE_ZOOM_PAGE_TWO_FINGER 0.25    // a second finger this soon after a finger began drawing: it was a gesture
#define FUDE_ZOOM_PAGE_WHEEL      1.12    // the zoom a wheel notch gives
#define FUDE_ZOOM_PAGE_MARK_INSET 34.0f   // the marks' arrows: in from the screen's edge (screen units)
#define FUDE_ZOOM_PAGE_MARK_REACH 26.0f   // ...and how near a press counts as on one
#define FUDE_ZOOM_PAGE_MARK_REST  0.2     // seconds the camera rests before they are looked for
#define FUDE_ZOOM_PAGE_FILL_GAP   24.0    // a line's ends this near (screen units), or a fifth of its size: closed, for Fill
#define FUDE_ZOOM_PAGE_HOVER_QUIET 0.35   // seconds the brush's circle waits after the pen lifts

// The one page (the page kind's hooks are handed the app, not the page).
RDE_INTERNAL fude_zoom_page* fude_zoom_page_the = NULL;

RDE_INTERNAL b8   fude_zoom_page_mark_press(fude_zoom_page* _page, rde_vec_2F _screen);
RDE_INTERNAL void fude_zoom_page_pen_under(fude_zoom_page* _page);

RDE_INTERNAL fude_zoom_v2 fude_zoom_page_half(const fude_zoom_page* _page) {
    const rde_vec_2I _size = rde_window_get_size(_page->app->window);
    return (fude_zoom_v2){ (f64)_size.x * 0.5, (f64)_size.y * 0.5 };
}

RDE_INTERNAL fude_zoom_box fude_zoom_page_view(const fude_zoom_page* _page) {
    const fude_zoom_camera* _c = &_page->scene.camera;
    const fude_zoom_v2      _h = fude_zoom_page_half(_page);
    return (fude_zoom_box){ _c->at.x - _h.x / _c->z, _c->at.y - _h.y / _c->z, _c->at.x + _h.x / _c->z, _c->at.y + _h.y / _c->z };
}

// --- the canvas's file ------------------------------------------------------------------------

RDE_INTERNAL void fude_zoom_page_path(u32 _canvas, c8* _out, usize _size) {
    c8 _kana[RDE_MAX_PATH];
    fude_notes_canvas_path(_canvas, _kana, sizeof(_kana));
    c8* _dot = strrchr(_kana, '.');
    if(_dot != NULL) {
        *_dot = 0;
    }
    snprintf(_out, _size, "%s.zoom", _kana);
}

RDE_INTERNAL b8 fude_zoom_page_save_now(fude_zoom_page* _page, b8 _leaving) {
    if(!_page->open) {
        return true;
    }
    b8 _ok = fude_zoom_file_flush(&_page->file, &_page->scene);
    if(_ok && _leaving && _page->file.size > 0 && _page->file.journal_bytes > 0) {
        _ok = fude_zoom_file_checkpoint(&_page->file, &_page->scene);
    }
    _page->flushed_at = rde_engine_get_time_now();
    return _ok;
}

RDE_INTERNAL void fude_zoom_page_close(fude_zoom_page* _page) {
    if(!_page->open) {
        return;
    }
    fude_zoom_page_save_now(_page, true);
    fude_zoom_render_trim(&_page->renderer);
    fude_zoom_scene_destroy(&_page->scene);
    _page->open = false;
}

RDE_INTERNAL void fude_zoom_page_open(fude_zoom_page* _page, u32 _canvas) {
    fude_zoom_page_close(_page);
    fude_zoom_scene_init(&_page->scene, _page->device);
    c8 _path[RDE_MAX_PATH];
    fude_zoom_page_path(_canvas, _path, sizeof(_path));
    const FUDE_LOAD_ _r = fude_zoom_file_open(&_page->file, _path, &_page->scene);
    u64 _points = 0;
    const u32 _strokes = fude_zoom_scene_alive_strokes(&_page->scene, &_points);
    rde_log_color(RDE_LOG_COLOR_GREEN, "fude: canvas %s (%s): %u strokes, %llu points, %u frames", _path,
                  _r == FUDE_LOAD_OK ? "loaded" : _r == FUDE_LOAD_MISSING ? "new" : _r == FUDE_LOAD_RECOVERED ? "RECOVERED from the backup" : "DAMAGED, kept as .bad",
                  _strokes, (unsigned long long)_points, fude_zoom_scene_frame_count(&_page->scene));
    fude_zoom_select_clear(&_page->selection);
    _page->canvas     = _canvas;
    _page->open       = true;
    _page->flushed_at = rde_engine_get_time_now();
    _page->drawing = _page->erasing = _page->finger_drawing = false;
}

// --- the pen: a stroke --------------------------------------------------------------------------

// The capture emptied for the next stroke (its pen state — pressure seen, the
// live pen — kept: it is the same pen).
RDE_INTERNAL void fude_zoom_page_capture_reset(fude_zoom_page* _page) {
    fude_ink* _c = &_page->capture;
    _c->strokes.count = 0;
    _c->points.count  = 0;
    _c->actions.count = 0;
    _c->targets.count = 0;
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
    const f64 _sigma = fude_zoom_smooth_sigma(_page->smooth_level) / _page->draw_z;
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
            _d += hypot(_raw[_i].x - _raw[_i - 1u].x, _raw[_i].y - _raw[_i - 1u].y);
        }
    }
}

RDE_INTERNAL void fude_zoom_page_smooth_tail(fude_zoom_page* _page) {
    fude_zoom_page_smooth_tail_as(_page, false);
}

// The pen at _at (from origin, the drawing frame's units) into the stroke: on
// the rope, where its tip is pulled to (nothing while the string is slack).
RDE_INTERNAL void fude_zoom_page_feed(fude_zoom_page* _page, fude_zoom_v2 _at) {
    if(_page->smooth_level == FUDE_ZOOM_SMOOTH_ROPE) {
        if(!fude_zoom_smooth_rope(&_page->rope_tip, _at, FUDE_ZOOM_SMOOTH_ROPE_LENGTH / _page->draw_z)) {
            return;
        }
        _at = _page->rope_tip;
    }
    fude_ink_extend(&_page->capture, (rde_vec_2F){ (f32)_at.x, (f32)_at.y });
    fude_zoom_page_smooth_tail(_page);
}

RDE_INTERNAL void fude_zoom_page_draw_down(fude_zoom_page* _page, rde_vec_2F _screen, b8 _from_pen) {
    fude_app*      _app = _page->app;
    fude_zoom_scene* _s = &_page->scene;
    const fude_zoom_v2 _at = fude_zoom_camera_to_frame(&_s->camera, (fude_zoom_v2){ _screen.x, _screen.y });
    const f64 _near = 4.0 / _s->camera.z;
    // Drawn LATER on top: a new frame when a parent's later ink covers here.
    const u32 _was_frame = _s->camera.frame; // DEBUG-VANISH
    _page->draw_frame = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _at.x - _near, _at.y - _near, _at.x + _near, _at.y + _near });
    _page->origin     = fude_zoom_camera_to_frame(&_s->camera, (fude_zoom_v2){ _screen.x, _screen.y });
    rde_log_level(RDE_LOG_LEVEL_WARNING, "VANISH down: cam was %u now %u draw %u z %g at %g,%g screen %g,%g", _was_frame, _s->camera.frame, _page->draw_frame, _s->camera.z, _page->origin.x, _page->origin.y, (f64)_screen.x, (f64)_screen.y); // DEBUG-VANISH
    _page->draw_z     = _s->camera.z;

    // The brush as the toolbar and Settings have it.
    fude_ink*       _c     = &_page->capture;
    const fude_ink* _brush = _app->ink;
    _c->width_mode      = _brush->width_mode;
    _c->brush_scale     = _brush->brush_scale;
    _c->color           = _brush->color;
    _c->constant_radius = _brush->constant_radius;
    _c->marker_color    = _brush->marker_color;
    _c->marker_radius   = _brush->marker_radius;
    _c->marking         = _app->ui->bar.tool == FUDE_TOOL_MARK;
    _c->zoom            = (f32)_page->draw_z;
    fude_zoom_page_capture_reset(_page);
    fude_ink_begin(_c, (rde_vec_2F){ 0.0f, 0.0f }, _from_pen, false);
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

RDE_INTERNAL void fude_zoom_page_draw_moved(fude_zoom_page* _page, rde_vec_2F _screen) {
    _page->pen_now = _screen;
    if(_page->snapped) {
        return;   // the shape follows the pen (drawn and placed from pen_now)
    }
    if(fabsf(_screen.x - _page->moved_from.x) + fabsf(_screen.y - _page->moved_from.y) > FUDE_ZOOM_PAGE_HOLD_SLOP) {
        _page->moved_at   = rde_engine_get_time_now();
        _page->moved_from = _screen;
    }
    const fude_zoom_v2 _at = fude_zoom_camera_to_frame(&_page->scene.camera, (fude_zoom_v2){ _screen.x, _screen.y });
    fude_zoom_page_feed(_page, (fude_zoom_v2){ _at.x - _page->origin.x, _at.y - _page->origin.y });
}

// The drawing frame's units → the screen, now.
RDE_INTERNAL fude_zoom_sim fude_zoom_page_draw_sim(const fude_zoom_page* _page) {
    return fude_zoom_sim_compose(fude_zoom_camera_sim(&_page->scene.camera), fude_zoom_scene_sim(&_page->scene, _page->draw_frame, _page->scene.camera.frame));
}

// The pen has rested: the stroke so far, if it looks like a shape, becomes it.
RDE_INTERNAL void fude_zoom_page_try_snap(fude_zoom_page* _page) {
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
RDE_INTERNAL fude_zoom_shape_fit fude_zoom_page_fit_now(const fude_zoom_page* _page) {
    fude_zoom_shape_fit _f = _page->fit;
    if(_f.type == FUDE_ZOOM_SHAPE_LINE) {
        _f.n[0] = (f64)_page->pen_now.x - _f.at.x;
        _f.n[1] = (f64)_page->pen_now.y - _f.at.y;
        return _f;
    }
    const f64 _wx = (f64)_page->snap_pen.x - _f.at.x, _wy = (f64)_page->snap_pen.y - _f.at.y;
    const f64 _nx = (f64)_page->pen_now.x - _f.at.x, _ny = (f64)_page->pen_now.y - _f.at.y;
    const f64 _was = hypot(_wx, _wy), _now = hypot(_nx, _ny);
    if(_was < 4.0 || _now < 4.0) {
        return _f;
    }
    const f64 _k = _now / _was;
    for(u32 _i = 0; _i < _f.count; _i++) {
        _f.n[_i] *= _k;
    }
    _f.rotation += atan2(_ny, _nx) - atan2(_wy, _wx);
    return _f;
}

// A shape in screen units into the frame strokes go in (_frame, seen through
// _to): its numbers in that frame's units, unscaled. Its index.
RDE_INTERNAL u32 fude_zoom_page_add_fit(fude_zoom_page* _page, u32 _frame, fude_zoom_sim _to, const fude_zoom_shape_fit* _f, rde_color _color, f32 _radius, u8 _flags) {
    const fude_zoom_sim _bk = fude_zoom_sim_inverse(_to);
    const f64           _k  = fude_zoom_sim_scale(_bk);
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    for(u32 _i = 0; _i < _f->count; _i++) {
        _n[_i] = _f->n[_i] * _k;   // screen units → the frame's
    }
    const fude_zoom_place _at = { fude_zoom_sim_apply(_bk, _f->at), _f->rotation + atan2(_bk.b, _bk.a), 1.0 };
    return fude_zoom_scene_add_shape(&_page->scene, _frame, _at, _f->type, _n, _f->count, _color, _radius, _flags, 0);
}

// A shape in screen units drawn over the page (the one being placed): its line
// _radius pt, in _color, filled too when _filled.
RDE_INTERNAL void fude_zoom_page_draw_fit(fude_zoom_page* _page, const fude_zoom_shape_fit* _f, f32 _radius, rde_color _color, b8 _filled) {
    b8 _closed;
    fude_zoom_shape_outline(_f->type, _f->n, _f->count, fude_zoom_shape_segments(400.0), &_page->snap_scratch, &_closed);
    const u32 _k = (u32)rde_arr_length(&_page->snap_scratch);
    if(_k < 2u || _k > 511u) {
        return;
    }
    const f64 _co = cos(_f->rotation), _si = sin(_f->rotation);
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
    // The fill after the line, as the canvas draws it (render.c).
    if(_closed && _filled) {
        rde_rendering_2d_draw_polygon(_pts, _k, _color, NULL);
    }
}

// The captured stroke into the canvas: its points as whole quanta from the
// first, in pieces of FUDE_ZOOM_STROKE_MAX (each sharing its last point with
// the next, so they join), one undo step.
RDE_INTERNAL void fude_zoom_page_draw_up(fude_zoom_page* _page) {
    if(!_page->drawing) {
        return;
    }
    _page->drawing = false;
    fude_ink* _c = &_page->capture;
    // On the rope, the line catches up with the pen (Krita's finish line).
    if(_page->smooth_level == FUDE_ZOOM_SMOOTH_ROPE && !_page->snapped) {
        const fude_zoom_v2 _pen  = fude_zoom_camera_to_frame(&_page->scene.camera, (fude_zoom_v2){ _page->pen_now.x, _page->pen_now.y });
        const fude_zoom_v2 _to   = { _pen.x - _page->origin.x, _pen.y - _page->origin.y };
        const fude_zoom_v2 _tip  = _page->rope_tip;
        const f64          _len  = hypot(_to.x - _tip.x, _to.y - _tip.y);
        const u32          _gaps = (u32)fmin(ceil(_len * _page->draw_z / FUDE_ZOOM_SMOOTH_CATCH_STEP), 4096.0);
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
    const i8  _q    = fude_zoom_quantum_for(_page->draw_z);
    const f64 _grid = ldexp(1.0, _q);
    f32 _base = 0.0f;
    for(u32 _i = 0; _i < _n; _i++) {
        _base = _p[_i].radius > _base ? _p[_i].radius : _base;
    }
    b8 _varies = false;
    for(u32 _i = 0; _i < _n && !_varies; _i++) {
        _varies = fabsf(_p[_i].radius - _base) > _base * 1e-4f;
    }
    u8 _flags = 0;
    if(_st->from_pen) { _flags |= FUDE_ZOOM_FLAG_FROM_PEN; }
    if(_st->marker)   { _flags |= FUDE_ZOOM_FLAG_MARKER; }
    if(_varies)       { _flags |= FUDE_ZOOM_FLAG_PRESSURE; }
    const u8 _channels = (u8)(FUDE_ZOOM_CHANNEL_TIME | (_varies ? FUDE_ZOOM_CHANNEL_PRESSURE : 0u));

    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_qpoint*     _all  = _heap->malloc(_heap->allocator, (usize)_n * sizeof(fude_zoom_qpoint));
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _share = _base > 0.0f ? (f64)_p[_i].radius / (f64)_base : 1.0;
        _all[_i] = (fude_zoom_qpoint){
            (i32)llround(((f64)_p[_i].position.x - (f64)_p[0].position.x) / _grid),
            (i32)llround(((f64)_p[_i].position.y - (f64)_p[0].position.y) / _grid),
            (u16)llround(_share * 1023.0),
            (u32)llround((f64)_p[_i].time * 1000.0),
        };
    }
    const fude_zoom_v2 _t0 = { _page->origin.x + (f64)_p[0].position.x, _page->origin.y + (f64)_p[0].position.y };
    u32           _born[256];
    u32           _pieces  = 0;
    fude_zoom_id  _gesture = 0;
    fude_zoom_box _box     = fude_zoom_box_empty();
    u32           _from    = 0;
    for(;;) {
        u32 _to = _from + FUDE_ZOOM_STROKE_MAX - 1u;
        _to = _to > _n - 1u ? _n - 1u : _to;
        const u32 _count = _to - _from + 1u;
        // The piece's own first point is its translation; its points from it.
        fude_zoom_qpoint* _pq = _heap->malloc(_heap->allocator, (usize)_count * sizeof(fude_zoom_qpoint));
        for(u32 _i = 0; _i < _count; _i++) {
            _pq[_i]       = _all[_from + _i];
            _pq[_i].x    -= _all[_from].x;
            _pq[_i].y    -= _all[_from].y;
            _pq[_i].time -= _all[_from].time;
        }
        const fude_zoom_v2 _t = { _t0.x + (f64)_all[_from].x * _grid, _t0.y + (f64)_all[_from].y * _grid };
        const u32 _o = fude_zoom_scene_add_stroke(_s, _page->draw_frame, _t, _q, _pq, _count, _channels, _st->color, _base,
                                                  (u8)(_flags | (_pieces > 0 ? FUDE_ZOOM_FLAG_CONTINUES : 0u)), _gesture, 0);
        _heap->free(_heap->allocator, _pq);
        if(_pieces == 0) {
            _gesture = fude_zoom_scene_object(_s, _o)->id;
        }
        _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, _o)->box);
        if(_pieces < 256u) {
            _born[_pieces++] = _o;
        }
        if(_to >= _n - 1u) {
            break;
        }
        _from = _to;
    }
    _heap->free(_heap->allocator, _all);
    { // DEBUG-VANISH
        const fude_zoom_sim _ds = fude_zoom_page_draw_sim(_page);
        const fude_zoom_v2 _s0 = fude_zoom_sim_apply(_ds, (fude_zoom_v2){ _box.min_x, _box.min_y });
        const fude_zoom_v2 _s1 = fude_zoom_sim_apply(_ds, (fude_zoom_v2){ _box.max_x, _box.max_y });
        rde_log_level(RDE_LOG_LEVEL_WARNING, "VANISH up: frame %u cam %u draw_z %g cam_z %g q %d n %u p0 %g,%g pN %g,%g base %g smooth %u snapped %d pieces %u screen box %g,%g..%g,%g origin %g,%g",
            _page->draw_frame, _s->camera.frame, _page->draw_z, _s->camera.z, (i32)_q, _n, (f64)_p[0].position.x, (f64)_p[0].position.y, (f64)_p[_n-1].position.x, (f64)_p[_n-1].position.y,
            (f64)_base, (u32)_page->smooth_level, (i32)_page->snapped, _pieces, _s0.x, _s0.y, _s1.x, _s1.y, _page->origin.x, _page->origin.y);
    }
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
}

// A stroke that turned out to be a gesture (a second finger): never drawn.
RDE_INTERNAL void fude_zoom_page_draw_cancel(fude_zoom_page* _page) {
    _page->drawing = false;
    _page->snapped = false;
    fude_ink_end(&_page->capture);
    fude_zoom_page_capture_reset(_page);
}

// --- the shapes tool ----------------------------------------------------------------------------

// The shape the drag has made so far, screen units: a line from where the pen
// went down to where it is; the others filling the box between those.
RDE_INTERNAL fude_zoom_shape_fit fude_zoom_page_drag_fit(const fude_zoom_page* _page) {
    fude_zoom_shape_fit _f;
    memset(&_f, 0, sizeof(_f));
    const fude_zoom_v2 _a = { _page->shape_from.x, _page->shape_from.y }, _b = { _page->pen_now.x, _page->pen_now.y };
    const f64 _hw = fabs(_b.x - _a.x) * 0.5, _hh = fabs(_b.y - _a.y) * 0.5;
    _f.type = _page->shape_tool;
    _f.at   = (fude_zoom_v2){ (_a.x + _b.x) * 0.5, (_a.y + _b.y) * 0.5 };
    if(_f.type == FUDE_ZOOM_SHAPE_LINE) {
        _f.at    = _a;
        _f.n[0]  = _b.x - _a.x;
        _f.n[1]  = _b.y - _a.y;
        _f.count = 2u;
    } else if(_f.type == FUDE_ZOOM_SHAPE_RECT) {
        _f.n[0] = _hw; _f.n[1] = _hh; _f.n[2] = 0.0;
        _f.count = 3u;
    } else if(_f.type == FUDE_ZOOM_SHAPE_ELLIPSE) {
        _f.n[0] = _hw; _f.n[1] = _hh;
        _f.count = 2u;
    } else {
        // A triangle: its point at the top of the box (the screen's Y is up).
        const f64 _tri[6] = { -_hw, -_hh, _hw, -_hh, 0.0, _hh };
        memcpy(_f.n, _tri, sizeof(_tri));
        _f.count = 6u;
    }
    return _f;
}

// The brush's half-width on screen and in the frame strokes go in.
RDE_INTERNAL f32 fude_zoom_page_brush_radius(const fude_zoom_page* _page, f64 _frame_to_screen) {
    const fude_ink* _brush = _page->app->ink;
    return _brush->brush_scale == FUDE_INK_BRUSH_SCALE_SCREEN ? (f32)((f64)_brush->constant_radius / _frame_to_screen) : _brush->constant_radius;
}

RDE_INTERNAL void fude_zoom_page_shape_down(fude_zoom_page* _page, rde_vec_2F _screen) {
    fude_zoom_scene*   _s  = &_page->scene;
    const fude_zoom_v2 _at = fude_zoom_camera_to_frame(&_s->camera, (fude_zoom_v2){ _screen.x, _screen.y });
    const f64          _near = 4.0 / _s->camera.z;
    _page->draw_frame = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _at.x - _near, _at.y - _near, _at.x + _near, _at.y + _near });
    _page->shaping    = true;
    _page->shape_from = _screen;
    _page->pen_now    = _screen;
}

RDE_INTERNAL void fude_zoom_page_shape_up(fude_zoom_page* _page) {
    if(!_page->shaping) {
        return;
    }
    _page->shaping = false;
    if(fabsf(_page->pen_now.x - _page->shape_from.x) + fabsf(_page->pen_now.y - _page->shape_from.y) < 6.0f) {
        return;   // a tap: no shape
    }
    const fude_zoom_shape_fit _f  = fude_zoom_page_drag_fit(_page);
    const fude_zoom_sim       _to = fude_zoom_page_draw_sim(_page);
    const u32 _o = fude_zoom_page_add_fit(_page, _page->draw_frame, _to, &_f, _page->app->ink->color,
                                          fude_zoom_page_brush_radius(_page, fude_zoom_sim_scale(_to)), _page->shape_filled ? FUDE_ZOOM_FLAG_FILLED : 0u);
    fude_zoom_history_push(&_page->scene, _page->draw_frame, fude_zoom_scene_object(&_page->scene, _o)->box, NULL, 0, &_o, 1);
}

// The toolbar's Shapes (extension.h): its choices, and which show chosen.
RDE_INTERNAL const u8 FUDE_ZOOM_SHAPE_CHOICES[4] = { FUDE_ZOOM_SHAPE_LINE, FUDE_ZOOM_SHAPE_RECT, FUDE_ZOOM_SHAPE_ELLIPSE, FUDE_ZOOM_SHAPE_POLYGON };

RDE_INTERNAL b8 fude_zoom_shapes_choose(fude_app* _app, u32 _index) {
    RDE_UNUSED(_app);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_index < 4u) {
        _page->shape_tool = FUDE_ZOOM_SHAPE_CHOICES[_index];
        _page->fill_tool  = false;
        fude_zoom_page_pen_under(_page);
        return false;
    }
    _page->shape_filled = !_page->shape_filled;   // Filled: a toggle, the panel stays
    return true;
}

RDE_INTERNAL b8 fude_zoom_shapes_chosen(const fude_app* _app, u32 _index) {
    RDE_UNUSED(_app);
    const fude_zoom_page* _page = fude_zoom_page_the;
    return _index < 4u ? _page->shape_tool == FUDE_ZOOM_SHAPE_CHOICES[_index] : _page->shape_filled;
}

RDE_INTERNAL b8 fude_zoom_shapes_selected(const fude_app* _app) {
    RDE_UNUSED(_app);
    return fude_zoom_page_the != NULL && fude_zoom_page_the->shape_tool != 0;
}

RDE_INTERNAL const fude_extension_choice FUDE_ZOOM_SHAPES_CHOICES[] = {
    { FUDE_TEXT_ZOOM_SHAPE_LINE,     FUDE_ICON_LINE,      fude_zoom_shapes_choose },
    { FUDE_TEXT_ZOOM_SHAPE_RECT,     FUDE_ICON_RECTANGLE, fude_zoom_shapes_choose },
    { FUDE_TEXT_ZOOM_SHAPE_ELLIPSE,  FUDE_ICON_CIRCLE,    fude_zoom_shapes_choose },
    { FUDE_TEXT_ZOOM_SHAPE_TRIANGLE, FUDE_ICON_TRIANGLE,  fude_zoom_shapes_choose },
    { FUDE_TEXT_ZOOM_SHAPE_FILLED,   FUDE_ICON_FILL,      fude_zoom_shapes_choose },
};

// --- pictures ---------------------------------------------------------------------------------

// A JPEG's or PNG's size in pixels, from its first bytes. False: neither.
RDE_INTERNAL b8 fude_zoom_picture_size(const u8* _b, u32 _n, u32* _w, u32* _h) {
    if(_n >= 24u && _b[0] == 0x89 && _b[1] == 'P' && _b[2] == 'N' && _b[3] == 'G') {
        *_w = ((u32)_b[16] << 24) | ((u32)_b[17] << 16) | ((u32)_b[18] << 8) | _b[19];
        *_h = ((u32)_b[20] << 24) | ((u32)_b[21] << 16) | ((u32)_b[22] << 8) | _b[23];
        return *_w > 0u && *_h > 0u;
    }
    if(_n < 4u || _b[0] != 0xFF || _b[1] != 0xD8) {
        return false;
    }
    // JPEG: the markers in turn, to the frame's (SOF0..15, but not DHT, JPG, DAC).
    u32 _at = 2u;
    while(_at + 9u < _n) {
        if(_b[_at] != 0xFF) {
            return false;
        }
        const u8  _m   = _b[_at + 1u];
        const u32 _len = ((u32)_b[_at + 2u] << 8) | _b[_at + 3u];
        if(_m >= 0xC0 && _m <= 0xCF && _m != 0xC4 && _m != 0xC8 && _m != 0xCC) {
            *_h = ((u32)_b[_at + 5u] << 8) | _b[_at + 6u];
            *_w = ((u32)_b[_at + 7u] << 8) | _b[_at + 8u];
            return *_w > 0u && *_h > 0u;
        }
        _at += 2u + _len;
    }
    return false;
}

// What the pickers brought (extension.h: imported): each picture in the middle
// of the screen (each next a little down and across), its longer side most of
// the screen's, one undo step for them all; then selected, the Lasso in hand.
RDE_INTERNAL void fude_zoom_kind_imported(fude_app* _app, u8 _kind, const c8* const* _paths, u32 _count) {
    RDE_UNUSED(_kind);
    fude_zoom_page*  _page = fude_zoom_page_the;
    fude_zoom_scene* _s    = &_page->scene;
    if(!_page->open || _count == 0u) {
        return;
    }
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    const fude_zoom_v2 _mid  = fude_zoom_camera_to_frame(&_s->camera, (fude_zoom_v2){ 0.0, 0.0 });
    const f64          _near = 4.0 / _s->camera.z;
    const u32          _frame = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _mid.x - _near, _mid.y - _near, _mid.x + _near, _mid.y + _near });
    const fude_zoom_sim _to   = fude_zoom_sim_compose(fude_zoom_camera_sim(&_s->camera), fude_zoom_scene_sim(_s, _frame, _s->camera.frame));
    const fude_zoom_sim _bk   = fude_zoom_sim_inverse(_to);
    rde_arr _born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    b8      _bad  = false;
    for(u32 _i = 0; _i < _count; _i++) {
        u8* _bytes = NULL;
        u32 _size  = 0, _w = 0, _h = 0;
        if(!fude_picture_bytes(_paths[_i], FUDE_ZOOM_PAGE_PICTURE_PX, &_bytes, &_size) || !fude_zoom_picture_size(_bytes, _size, &_w, &_h)) {
            free(_bytes);
            _bad = true;
            continue;
        }
        // Its size on screen: its longer side 60% of the screen's, as it fits.
        const f64 _per_px = fmin(1.2 * _half.x / (f64)_w, 1.2 * _half.y / (f64)_h);
        const f64 _step   = 24.0 * (f64)rde_arr_length(&_born);
        const fude_zoom_v2  _at = fude_zoom_sim_apply(_bk, (fude_zoom_v2){ _step, -_step });
        const f64           _k  = fude_zoom_sim_scale(_bk);
        const fude_zoom_place _place = { _at, atan2(_bk.b, _bk.a), 1.0 };
        const u32 _o = fude_zoom_scene_add_image(_s, _frame, _place, (f64)_w * _per_px * 0.5 * _k, (f64)_h * _per_px * 0.5 * _k, _bytes, _size, 0);
        free(_bytes);
        rde_arr_add(&_born, (any)&_o);
    }
    if(rde_arr_length(&_born) > 0) {
        fude_zoom_box _box = fude_zoom_box_empty();
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_born); _i++) {
            _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, ((const u32*)_born.memory)[_i])->box);
        }
        fude_zoom_history_push(_s, _frame, _box, NULL, 0, (const u32*)_born.memory, (u32)rde_arr_length(&_born));
        // Selected, to be put where it goes.
        fude_toolbar_set_tool(&_app->ui->bar, FUDE_TOOL_LASSO);
        _page->tool_seen  = (u8)FUDE_TOOL_LASSO;
        _page->shape_tool = 0;
        fude_zoom_select_clear(&_page->selection);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_born); _i++) {
            const fude_zoom_pick _p = { ((const u32*)_born.memory)[_i] };
            rde_arr_add(&_page->selection.picks, (any)&_p);
        }
        fude_zoom_select_update(&_page->selection, _s);
        fude_ui_update(_app->ui);
    }
    if(_bad) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PICTURE_UNREADABLE));
    }
    rde_arr_free(&_born);
}

// The toolbar's Picture: Photos (where there is a library; Files otherwise) or Files.
RDE_INTERNAL b8 fude_zoom_picture_choose(fude_app* _app, u32 _index) {
    if(_index == 0u && fude_import_photos_available()) {
        fude_import_photos(_app);
    } else {
        fude_import_files(_app);
    }
    return false;
}

RDE_INTERNAL const fude_extension_choice FUDE_ZOOM_PICTURE_CHOICES[] = {
    { FUDE_TEXT_ZOOM_PICTURE_PHOTOS, FUDE_ICON_IMAGE,  fude_zoom_picture_choose },
    { FUDE_TEXT_ZOOM_PICTURE_FILES,  FUDE_ICON_FOLDER, fude_zoom_picture_choose },
};

// The pen in the bar's hand under one of the app's own tools (Fill, a shape):
// what the pen does then is the app tool's — never the eraser's or the lasso's
// left underneath. Not a tap of the bar's own (that would put the app tool down).
RDE_INTERNAL void fude_zoom_page_pen_under(fude_zoom_page* _page) {
    fude_toolbar* _bar = &_page->app->ui->bar;
    if(_bar->tool == FUDE_TOOL_ERASE || _bar->tool == FUDE_TOOL_LASSO) {
        fude_toolbar_set_tool(_bar, FUDE_TOOL_DRAW);
        _page->tool_taps_seen = _bar->tool_taps;
        _page->tool_seen      = (u8)FUDE_TOOL_DRAW;
        fude_zoom_select_clear(&_page->selection);
    }
}

// --- the Fill tool (fill.h) ------------------------------------------------------------------------

// A gesture's outline: its pieces from _object's on, in order (they sit one
// after another in the table), their points in their frame's units into _out.
// Its first piece, or FUDE_ZOOM_NONE when a piece of it is gone (an erase cut
// it: then it is not one closed line any more).
RDE_INTERNAL u32 fude_zoom_page_gesture_outline(const fude_zoom_scene* _s, u32 _object, rde_arr* _out, rde_arr* _q) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    u32 _first = _object, _last = _object;
    while(_first > 0 && fude_zoom_scene_object(_s, _first)->id != _o->gesture) {
        const fude_zoom_object* _b = fude_zoom_scene_object(_s, _first - 1u);
        if(_b->gesture != _o->gesture || _b->frame != _o->frame) {
            break;
        }
        _first--;
    }
    while(_last + 1u < fude_zoom_scene_object_count(_s)) {
        const fude_zoom_object* _n = fude_zoom_scene_object(_s, _last + 1u);
        if(_n->gesture != _o->gesture || _n->frame != _o->frame || !(_n->flags & FUDE_ZOOM_FLAG_CONTINUES)) {
            break;
        }
        _last++;
    }
    rde_arr_clear(_out);
    for(u32 _i = _first; _i <= _last; _i++) {
        const fude_zoom_object* _p = fude_zoom_scene_object(_s, _i);
        if(_p->kind != FUDE_ZOOM_KIND_STROKE || !(_p->flags & FUDE_ZOOM_FLAG_ALIVE) || _p->count == 0) {
            return FUDE_ZOOM_NONE;
        }
        rde_arr_clear(_q);
        rde_arr_add_n(_q, _p->count);
        if(!fude_zoom_scene_points(_s, _i, (fude_zoom_qpoint*)_q->memory)) {
            return FUDE_ZOOM_NONE;
        }
        const fude_zoom_qpoint* _pts = (const fude_zoom_qpoint*)_q->memory;
        for(u32 _k = _i == _first ? 0u : 1u; _k < _p->count; _k++) {   // each next piece begins where the last ended
            const fude_zoom_v2 _at = fude_zoom_scene_point_at(_p, &_pts[_k]);
            rde_arr_add(_out, (any)&_at);
        }
    }
    return _first;
}

// Is a gesture's outline (_out, from fude_zoom_page_gesture_outline) a closed
// line? 1: its ends near each other (a fifth of its size, or _gap); 2: its end
// running over its beginning (fill.h's region then takes the loop round a tap,
// of the loops it draws itself); 0: neither.
RDE_INTERNAL u8 fude_zoom_page_closed(rde_arr* _out, f64 _gap) {
    fude_zoom_v2* _p = (fude_zoom_v2*)_out->memory;
    const u32     _n = (u32)rde_arr_length(_out);
    if(_n < 3u) {
        return 0;
    }
    fude_zoom_box _b = fude_zoom_box_empty();
    for(u32 _k = 0; _k < _n; _k++) {
        _b = fude_zoom_box_union(_b, (fude_zoom_box){ _p[_k].x, _p[_k].y, _p[_k].x, _p[_k].y });
    }
    const f64 _size = fmax(_b.max_x - _b.min_x, _b.max_y - _b.min_y);
    if(hypot(_p[_n - 1u].x - _p[0].x, _p[_n - 1u].y - _p[0].y) <= fmax(0.2 * _size, _gap)) {
        return 1u;
    }
    // Its last third crossing its first third: the earliest such crossing.
    const u32 _head = _n / 3u, _tail = _n - _n / 3u;
    for(u32 _i = 0; _i + 1u < _head; _i++) {
        for(u32 _j = _n - 2u; _j >= _tail && _j > _i + 1u; _j--) {
            const fude_zoom_v2 _a = _p[_i], _c = _p[_i + 1u], _d = _p[_j], _e = _p[_j + 1u];
            const f64 _den = (_c.x - _a.x) * (_e.y - _d.y) - (_c.y - _a.y) * (_e.x - _d.x);
            if(_den == 0.0) {
                continue;
            }
            const f64 _t = ((_d.x - _a.x) * (_e.y - _d.y) - (_d.y - _a.y) * (_e.x - _d.x)) / _den;
            const f64 _u = ((_d.x - _a.x) * (_c.y - _a.y) - (_d.y - _a.y) * (_c.x - _a.x)) / _den;
            if(_t >= 0.0 && _t <= 1.0 && _u >= 0.0 && _u <= 1.0) {
                return 2u;
            }
        }
    }
    return 0;
}

// A tap with the Fill tool: the topmost closed thing under it — a shape, a
// fill, a line that comes back near where it began — filled with the brush's
// colour (or let go, tapped again in it). One undo step.
RDE_INTERNAL void fude_zoom_page_fill_at(fude_zoom_page* _page, rde_vec_2F _screen) {
    fude_zoom_scene* _s     = &_page->scene;
    const rde_color  _brush = _page->app->ink->color;
    rde_arr _outline = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    rde_arr _q       = rde_arr_new(sizeof(fude_zoom_qpoint), rde_memory_allocator_get_default_std());
    rde_arr _found   = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr _region  = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    rde_arr _rings   = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    // The best so far: 1 a shape, 2 a fill, 3 a closed line (its first piece).
    u8  _what = 0;
    u32 _pick = FUDE_ZOOM_NONE, _pick_depth = 0;
    u64 _pick_z = 0;
    fude_zoom_v2 _pick_tap = { 0.0, 0.0 };   // a closed line's: the tap, its frame's units
    f64          _pick_gap = 0.0;            // ...and its ends' gap counted as closed
    rde_arr_clear(&_page->editable);
    fude_zoom_render_editable(&_page->renderer, _s, &_page->editable);
    const fude_zoom_visible* _v = (const fude_zoom_visible*)_page->editable.memory;
    for(u32 _f = 0; _f < (u32)rde_arr_length(&_page->editable); _f++) {
        const f64          _scale = fude_zoom_sim_scale(_v[_f].to_screen);
        const fude_zoom_v2 _p     = fude_zoom_sim_apply(fude_zoom_sim_inverse(_v[_f].to_screen), (fude_zoom_v2){ _screen.x, _screen.y });
        const f64          _near  = 2.0 / _scale;
        rde_arr_clear(&_found);
        fude_zoom_scene_query(_s, _v[_f].frame, (fude_zoom_box){ _p.x - _near, _p.y - _near, _p.x + _near, _p.y + _near }, &_found);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_found); _i++) {
            const u32               _object = ((const u32*)_found.memory)[_i];
            const fude_zoom_object* _o      = fude_zoom_scene_object(_s, _object);
            if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
                continue;
            }
            u8  _kind = 0;
            u32 _at   = _object;
            u64 _z    = _o->z;
            if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
                b8 _closed;
                fude_zoom_scene_shape_outline(_s, _object, 96u, &_outline, &_closed);
                if(_closed && fude_zoom_fill_inside((const fude_zoom_v2*)_outline.memory, (u32)rde_arr_length(&_outline), _p)) {
                    _kind = 1u;
                }
            } else if(_o->kind == FUDE_ZOOM_KIND_FILL && _o->count >= 3u) {
                rde_arr_clear(&_q);
                rde_arr_add_n(&_q, _o->count);
                if(fude_zoom_scene_points(_s, _object, (fude_zoom_qpoint*)_q.memory)) {
                    // What the eraser left of it (its points' rings: fill.h).
                    rde_arr_clear(&_outline);
                    rde_arr_clear(&_rings);
                    for(u32 _k = 0; _k < _o->count; _k++) {
                        const fude_zoom_qpoint* _qk = &((const fude_zoom_qpoint*)_q.memory)[_k];
                        const fude_zoom_v2      _pt = fude_zoom_scene_point_at(_o, _qk);
                        rde_arr_add(&_outline, (any)&_pt);
                        rde_arr_add(&_rings, (any)&_qk->time);
                    }
                    _kind = fude_zoom_fill_inside_rings((const fude_zoom_v2*)_outline.memory, (const u32*)_rings.memory, _o->count, _p) ? 2u : 0u;
                }
            } else if(_o->kind == FUDE_ZOOM_KIND_STROKE && !(_o->flags & FUDE_ZOOM_FLAG_MARKER)) {
                const u32 _first  = fude_zoom_page_gesture_outline(_s, _object, &_outline, &_q);
                const u8  _closed = _first != FUDE_ZOOM_NONE ? fude_zoom_page_closed(&_outline, FUDE_ZOOM_PAGE_FILL_GAP / _scale) : 0u;
                if(_closed != 0u) {
                    if(fude_zoom_fill_region((const fude_zoom_v2*)_outline.memory, (u32)rde_arr_length(&_outline), _p, _closed == 1u, &_region) >= 3u) {
                        _kind = 3u;
                        _at   = _first;
                        _z    = fude_zoom_scene_object(_s, _first)->z - 1u;   // its fill goes just under it
                    }
                }
            }
            // The deepest frame's first, then the latest drawn; a fill before the line it fills.
            const b8 _better = _kind != 0 && (_what == 0 || _v[_f].depth > _pick_depth ||
                               (_v[_f].depth == _pick_depth && (_z > _pick_z || (_z == _pick_z && _kind == 2u))));
            if(_better) {
                _what = _kind; _pick = _at; _pick_depth = _v[_f].depth; _pick_z = _z; _pick_tap = _p; _pick_gap = FUDE_ZOOM_PAGE_FILL_GAP / _scale;
            }
        }
    }
    if(_what == 0) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_FILL_NOTHING));
    } else if(_what == 1u) {
        // A shape: filled in the brush's colour, or let go when it already is.
        const fude_zoom_object _o = *fude_zoom_scene_object(_s, _pick);
        f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
        const u32 _count = fude_zoom_scene_shape_numbers(_s, _pick, _n, FUDE_ZOOM_SHAPE_NUMBERS);
        const rde_color _now  = (_o.flags & FUDE_ZOOM_FLAG_FILL_OWN) ? _o.fill : _o.color;
        const b8        _drop = (_o.flags & FUDE_ZOOM_FLAG_FILLED) && memcmp(&_now, &_brush, sizeof(rde_color)) == 0;
        const u8        _flags = _drop ? 0u : (u8)(FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN);
        const u32 _new = fude_zoom_scene_add_shape_fill(_s, _o.frame, fude_zoom_scene_place_of(_s, _pick), _o.channels, _n, _count, _o.color, _o.radius, _flags, _brush, _o.z + 1u);
        fude_zoom_scene_set_alive(_s, _pick, false);
        fude_zoom_history_push(_s, _o.frame, fude_zoom_scene_object(_s, _new)->box, &_pick, 1, &_new, 1);
    } else if(_what == 2u) {
        // A fill: in the brush's colour now, or gone when it already was.
        const fude_zoom_object _o = *fude_zoom_scene_object(_s, _pick);
        if(memcmp(&_o.color, &_brush, sizeof(rde_color)) == 0) {
            fude_zoom_scene_set_alive(_s, _pick, false);
            fude_zoom_history_push(_s, _o.frame, _o.box, &_pick, 1, NULL, 0);
        } else {
            rde_arr_clear(&_q);
            rde_arr_add_n(&_q, _o.count);
            if(fude_zoom_scene_points(_s, _pick, (fude_zoom_qpoint*)_q.memory)) {
                const u32 _new = fude_zoom_scene_add_fill(_s, _o.frame, fude_zoom_scene_place_of(_s, _pick), _o.q, (const fude_zoom_qpoint*)_q.memory, _o.count, _brush, _o.z);
                fude_zoom_scene_set_alive(_s, _pick, false);
                fude_zoom_history_push(_s, _o.frame, _o.box, &_pick, 1, &_new, 1);
            }
        }
    } else {
        // A closed line: a fill under it — the loop of it round the tap — as
        // whole quanta from its first corner.
        const fude_zoom_object _o = *fude_zoom_scene_object(_s, _pick);
        fude_zoom_page_gesture_outline(_s, _pick, &_outline, &_q);
        const b8 _ends = fude_zoom_page_closed(&_outline, _pick_gap) == 1u;
        fude_zoom_fill_region((const fude_zoom_v2*)_outline.memory, (u32)rde_arr_length(&_outline), _pick_tap, _ends, &_region);
        const u32           _n    = (u32)rde_arr_length(&_region);
        const fude_zoom_v2* _pts  = (const fude_zoom_v2*)_region.memory;
        const f64           _grid = ldexp(1.0, _o.q);
        rde_arr_clear(&_q);
        fude_zoom_qpoint* _qp = rde_arr_add_n(&_q, _n);
        for(u32 _k = 0; _k < _n; _k++) {
            _qp[_k] = (fude_zoom_qpoint){ (i32)llround((_pts[_k].x - _pts[0].x) / _grid), (i32)llround((_pts[_k].y - _pts[0].y) / _grid), 0u, 0u };
        }
        const u32 _new = fude_zoom_scene_add_fill(_s, _o.frame, (fude_zoom_place){ _pts[0], 0.0, 1.0 }, _o.q, _qp, _n, _brush, _pick_z);
        fude_zoom_history_push(_s, _o.frame, fude_zoom_scene_object(_s, _new)->box, NULL, 0, &_new, 1);
    }
    rde_arr_free(&_outline);
    rde_arr_free(&_q);
    rde_arr_free(&_found);
    rde_arr_free(&_region);
    rde_arr_free(&_rings);
}

// The toolbar's Fill: in hand or put down (the Shapes tool, and the bar's
// own tools, put it down too).
RDE_INTERNAL void fude_zoom_fill_press(fude_app* _app) {
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return;
    }
    _page->fill_tool = !_page->fill_tool;
    if(_page->fill_tool) {
        _page->shape_tool = 0;
        fude_zoom_select_clear(&_page->selection);
        fude_zoom_page_pen_under(_page);
    }
    fude_toolbar_refresh(&_app->ui->bar);
}

RDE_INTERNAL b8 fude_zoom_fill_selected(const fude_app* _app) {
    RDE_UNUSED(_app);
    return fude_zoom_page_the != NULL && fude_zoom_page_the->fill_tool;
}

// --- export (export.h) ------------------------------------------------------------------------------

#define FUDE_ZOOM_PAGE_EXPORT_PX    4096.0   // a PNG's longer side, at most
#define FUDE_ZOOM_PAGE_EXPORT_SCALE 3.0      // ...and how many pixels a screen point, at most

// The view out to _path: an SVG written now; a PNG drawn in the next frame's
// render and written in the update after (fude_zoom_page_render_offscreen).
// _share: then to the system's share sheet.
RDE_INTERNAL void fude_zoom_page_export_to(fude_zoom_page* _page, u8 _format, const c8* _path, b8 _share) {
    if(_format == 1u) {
        fude_bytes _b = fude_bytes_new(1u << 16);
        const b8   _made  = fude_zoom_export_svg(&_page->scene, fude_zoom_page_half(_page), fude_theme_active()->page, &_b);
        const b8   _wrote = _made && fude_bytes_write_and_free(&_b, _path, NULL);
        if(!_made) {
            rde_arr_free(&_b);
        }
        if(!_wrote || (_share && !rde_mobile_share_file(_path, "image/svg+xml", _page->app->info->name))) {
            fude_notice_show(fude_text(FUDE_TEXT_ZOOM_EXPORT_FAILED));
        } else if(!_share) {
            c8 _line[1200];
            FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_EXPORT_SAVED, FUDE_TS(_path));
            fude_notice_show(_line);
        }
        return;
    }
    snprintf(_page->export_path, sizeof(_page->export_path), "%s", _path);
    _page->export_share = _share;
    _page->export_stage = 1u;
}

RDE_INTERNAL u8 fude_zoom_page_export_format = 0;

#if !defined(RDE_PLATFORM_MOBILE)
RDE_INTERNAL void fude_zoom_page_on_export_path(const c8* const* _paths, u32 _count, i32 _filter, any _user_data) {
    RDE_UNUSED(_filter);
    fude_zoom_page* _page = (fude_zoom_page*)_user_data;
    if(_paths == NULL || _count == 0 || _page == NULL) {
        return;   // cancelled
    }
    const b8 _svg = fude_zoom_page_export_format == 1u;
    c8 _path[1024];
    fude_session_with_extension(_path, sizeof(_path), _paths[0], _svg ? ".svg" : ".png", _svg ? ".SVG" : ".PNG");
    fude_zoom_page_export_to(_page, fude_zoom_page_export_format, _path, false);
}
#endif

// The toolbar's Export: the view as an image (PNG) or a drawing (SVG) — shared
// on a tablet, saved where chosen on a computer.
RDE_INTERNAL b8 fude_zoom_export_choose(fude_app* _app, u32 _index) {
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->open || _page->export_stage != 0u) {
        return false;
    }
    fude_zoom_page_export_format = (u8)_index;
    const b8 _svg = _index == 1u;
#if defined(RDE_PLATFORM_MOBILE)
    c8 _path[1024];
    fude_session_outbox_path(_path, sizeof(_path), _app->info->name, _svg ? "svg" : "png");
    fude_zoom_page_export_to(_page, (u8)_index, _path, true);
#else
    static const rde_dialog_filter _png = { "PNG", "png" };
    static const rde_dialog_filter _svgf = { "SVG", "svg" };
    rde_dialog_save_file(_app->window, _svg ? &_svgf : &_png, 1, NULL, fude_zoom_page_on_export_path, _page);
#endif
    return false;
}

RDE_INTERNAL const fude_extension_choice FUDE_ZOOM_EXPORT_CHOICES[] = {
    { FUDE_TEXT_ZOOM_EXPORT_PNG, FUDE_ICON_IMAGE, fude_zoom_export_choose },
    { FUDE_TEXT_ZOOM_EXPORT_SVG, FUDE_ICON_PEN,   fude_zoom_export_choose },
};

void fude_zoom_page_render_offscreen(fude_zoom_page* _page, rde_window* _window, rde_camera* _camera) {
    if(_page->export_stage != 1u || !_page->open) {
        return;
    }
    // The screen, larger: up to three pixels a point, its longer side 4096 at most.
    const fude_zoom_v2 _half  = fude_zoom_page_half(_page);
    const f64          _scale = fmin(FUDE_ZOOM_PAGE_EXPORT_SCALE, FUDE_ZOOM_PAGE_EXPORT_PX / (2.0 * fmax(_half.x, _half.y)));
    const u32          _w     = (u32)fmax(1.0, floor(_half.x * 2.0 * _scale));
    const u32          _h     = (u32)fmax(1.0, floor(_half.y * 2.0 * _scale));
    _page->export_target = rde_render_texture_load(_w, _h, NULL);
    if(_page->export_target == NULL) {
        _page->export_stage = 0;
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_EXPORT_FAILED));
        return;
    }
    fude_zoom_scene* _s  = &_page->scene;
    const f64        _z  = _s->camera.z;
    const rde_color  _bg = fude_theme_active()->page;
    rde_render_texture_enable(_page->export_target);
    rde_rendering_2d_begin_drawing(_window, _camera);
    rde_rendering_2d_draw_rectangle((rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ (f32)_w + 4.0f, (f32)_h + 4.0f }, _bg);
    _s->camera.z = _z * _scale;   // (drawn only: never settled like this)
    fude_zoom_render(&_page->renderer, _s, (fude_zoom_v2){ (f64)_w * 0.5, (f64)_h * 0.5 }, _bg);
    _s->camera.z = _z;
    rde_rendering_2d_end_drawing();
    rde_render_texture_disable();
    _page->export_stage = 2u;
}

// The PNG's view drawn the frame before: read back, encoded, written, shared.
RDE_INTERNAL void fude_zoom_page_export_finish(fude_zoom_page* _page) {
    u32 _w = 0, _h = 0;
    u8* _px = rde_render_texture_read_pixels(_page->export_target, &_w, &_h, rde_memory_allocator_get_default_std());
    usize _size = 0;
    u8*   _png  = _px != NULL ? rde_image_encode_png(_px, _w, _h, &_size, rde_memory_allocator_get_default_std()) : NULL;
    b8    _ok   = false;
    if(_png != NULL) {
        fude_bytes _b = fude_bytes_new((u32)_size + 16u);
        fude_put_data(&_b, _png, (u32)_size);
        _ok = fude_bytes_write_and_free(&_b, _page->export_path, NULL);
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    if(_px != NULL) {
        _heap->free(_heap->allocator, _px);
    }
    if(_png != NULL) {
        _heap->free(_heap->allocator, _png);
    }
    rde_render_texture_unload(_page->export_target);
    _page->export_target = NULL;
    _page->export_stage  = 0;
    if(!_ok || (_page->export_share && !rde_mobile_share_file(_page->export_path, "image/png", _page->app->info->name))) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_EXPORT_FAILED));
    } else if(!_page->export_share) {
        c8 _line[1200];
        FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_EXPORT_SAVED, FUDE_TS(_page->export_path));
        fude_notice_show(_line);
    }
}

void fude_zoom_page_look_export(fude_zoom_page* _page, u8 _format, const c8* _path) {
    fude_zoom_page_export_to(_page, _format, _path, false);
}

// The toolbar's Smoothing: the level new strokes are drawn with (smooth.h).
RDE_INTERNAL b8 fude_zoom_smoothing_choose(fude_app* _app, u32 _index) {
    RDE_UNUSED(_app);
    if(fude_zoom_page_the != NULL && _index < FUDE_ZOOM_SMOOTH_COUNT) {
        fude_zoom_page_the->smooth_level = (u8)_index;
    }
    return false;
}

RDE_INTERNAL b8 fude_zoom_smoothing_chosen(const fude_app* _app, u32 _index) {
    RDE_UNUSED(_app);
    return fude_zoom_page_the != NULL && fude_zoom_page_the->smooth_level == _index;
}

RDE_INTERNAL b8 fude_zoom_smoothing_selected(const fude_app* _app) {
    RDE_UNUSED(_app);
    return false;   // a setting, not a tool in hand
}

// In FUDE_ZOOM_SMOOTH_ order.
RDE_INTERNAL const fude_extension_choice FUDE_ZOOM_SMOOTHING_CHOICES[] = {
    { FUDE_TEXT_ZOOM_SMOOTH_OFF,    FUDE_ICON_SCRIBBLE,      fude_zoom_smoothing_choose },
    { FUDE_TEXT_ZOOM_SMOOTH_LOW,    FUDE_ICON_SIGNAL_LOW,    fude_zoom_smoothing_choose },
    { FUDE_TEXT_ZOOM_SMOOTH_MEDIUM, FUDE_ICON_SIGNAL_MEDIUM, fude_zoom_smoothing_choose },
    { FUDE_TEXT_ZOOM_SMOOTH_HIGH,   FUDE_ICON_SIGNAL_HIGH,   fude_zoom_smoothing_choose },
    { FUDE_TEXT_ZOOM_SMOOTH_ROPE,   FUDE_ICON_GUIDED,        fude_zoom_smoothing_choose },
};

void fude_zoom_page_settings_gather(const fude_app* _app, fude_settings* _settings) {
    RDE_UNUSED(_app);
    if(fude_zoom_page_the != NULL) {
        _settings->smoothing = (u8)(fude_zoom_page_the->smooth_level + 1u);
    }
}

void fude_zoom_page_settings_apply(fude_app* _app, const fude_settings* _settings) {
    RDE_UNUSED(_app);
    if(fude_zoom_page_the != NULL && _settings->smoothing >= 1u && _settings->smoothing <= FUDE_ZOOM_SMOOTH_COUNT) {
        fude_zoom_page_the->smooth_level = (u8)(_settings->smoothing - 1u);
    }
}

const fude_extension_tool FUDE_ZOOM_TOOLS[5] = {
    {
        FUDE_TEXT_ZOOM_SHAPES, FUDE_ICON_SHAPES, NULL, NULL,
        FUDE_ZOOM_SHAPES_CHOICES, sizeof(FUDE_ZOOM_SHAPES_CHOICES) / sizeof(FUDE_ZOOM_SHAPES_CHOICES[0]),
        fude_zoom_shapes_chosen, fude_zoom_shapes_selected,
    },
    {
        FUDE_TEXT_ZOOM_PICTURE, FUDE_ICON_IMAGE, NULL, NULL,
        FUDE_ZOOM_PICTURE_CHOICES, sizeof(FUDE_ZOOM_PICTURE_CHOICES) / sizeof(FUDE_ZOOM_PICTURE_CHOICES[0]),
        NULL, NULL,
    },
    {
        FUDE_TEXT_ZOOM_SMOOTHING, FUDE_ICON_SMOOTHING, NULL, NULL,
        FUDE_ZOOM_SMOOTHING_CHOICES, sizeof(FUDE_ZOOM_SMOOTHING_CHOICES) / sizeof(FUDE_ZOOM_SMOOTHING_CHOICES[0]),
        fude_zoom_smoothing_chosen, fude_zoom_smoothing_selected,
    },
    {
        FUDE_TEXT_ZOOM_FILL, FUDE_ICON_FILL, fude_zoom_fill_press, NULL,
        NULL, 0u, NULL, fude_zoom_fill_selected,
    },
    {
        FUDE_TEXT_ZOOM_EXPORT, FUDE_ICON_EXPORT, NULL, NULL,
        FUDE_ZOOM_EXPORT_CHOICES, sizeof(FUDE_ZOOM_EXPORT_CHOICES) / sizeof(FUDE_ZOOM_EXPORT_CHOICES[0]),
        NULL, NULL,
    },
};

// --- the pen: erasing ------------------------------------------------------------------------------

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

RDE_INTERNAL void fude_zoom_page_erase_down(fude_zoom_page* _page, rde_vec_2F _screen) {
    fude_zoom_erase_begin(&_page->eraser, (FUDE_ZOOM_ERASE_)_page->erase_mode);
    _page->erasing    = true;
    _page->erase_last = _screen;
    fude_zoom_page_erase_step(_page, _screen, _screen);
}

RDE_INTERNAL void fude_zoom_page_erase_moved(fude_zoom_page* _page, rde_vec_2F _screen) {
    fude_zoom_page_erase_step(_page, _page->erase_last, _screen);
    _page->erase_last = _screen;
}

RDE_INTERNAL void fude_zoom_page_erase_up(fude_zoom_page* _page) {
    if(!_page->erasing) {
        return;
    }
    _page->erasing = false;
    fude_zoom_erase_end(&_page->scene, &_page->eraser, _page->scene.camera.frame, fude_zoom_page_view(_page));
}

// The pen (mouse, writing finger) down, moved, up: drawing or erasing, as the
// tool or the pen's end says.
RDE_INTERNAL void fude_zoom_page_down(fude_zoom_page* _page, rde_vec_2F _screen, b8 _from_pen, b8 _eraser_end) {
    if(!_page->open) {
        return;
    }
    _page->flight.active = false;   // the hand takes over where it is
    _page->crumbs_open   = false;
    _page->places_open   = false;
    fude_pagemenu_close_context(&_page->app->ui->page);   // a press anywhere else dismisses it, and still does its job
    if(fude_zoom_page_mark_press(_page, _screen)) {
        return;
    }
    if(_page->fill_tool && !_eraser_end) {
        fude_zoom_page_fill_at(_page, _screen);
        return;
    }
    if(_page->app->ui->bar.tool == FUDE_TOOL_ERASE || _eraser_end) {
        fude_zoom_page_erase_down(_page, _screen);
    } else if(_page->shape_tool != 0) {
        fude_zoom_page_shape_down(_page, _screen);
    } else if(_page->app->ui->bar.tool == FUDE_TOOL_LASSO) {
        fude_zoom_select_down(&_page->selection, &_page->scene, &_page->renderer, _screen);
    } else {
        fude_zoom_page_draw_down(_page, _screen, _from_pen);
    }
}

RDE_INTERNAL void fude_zoom_page_moved(fude_zoom_page* _page, rde_vec_2F _screen) {
    if(_page->erasing) {
        fude_zoom_page_erase_moved(_page, _screen);
    } else if(_page->drawing) {
        fude_zoom_page_draw_moved(_page, _screen);
    } else if(fude_zoom_select_busy(&_page->selection)) {
        fude_zoom_select_moved(&_page->selection, _screen);
    } else if(_page->shaping) {
        _page->pen_now = _screen;
    }
}

RDE_INTERNAL void fude_zoom_page_up(fude_zoom_page* _page) {
    fude_zoom_page_erase_up(_page);
    fude_zoom_page_draw_up(_page);
    fude_zoom_page_shape_up(_page);
    if(fude_zoom_select_busy(&_page->selection)) {
        fude_zoom_select_up(&_page->selection, &_page->scene, &_page->renderer);
    }
}

// --- getting around (nav.h) ---------------------------------------------------------------------

// The camera flown to _to; _remember: where it was goes on Back's list.
RDE_INTERNAL void fude_zoom_page_fly(fude_zoom_page* _page, fude_zoom_camera _to, b8 _remember) {
    fude_zoom_scene* _s = &_page->scene;
    if(_remember) {
        if(_page->back_count == FUDE_ZOOM_PAGE_BACK) {
            memmove(&_page->back[0], &_page->back[1], sizeof(_page->back[0]) * (FUDE_ZOOM_PAGE_BACK - 1u));
            _page->back_count--;
        }
        _page->back[_page->back_count].frame = fude_zoom_scene_frame(_s, _s->camera.frame)->id;
        _page->back[_page->back_count].at    = _s->camera.at;
        _page->back[_page->back_count].z     = _s->camera.z;
        _page->back_count++;
    }
    _page->crumbs_open = false;
    fude_zoom_fly_begin(&_page->flight, _s, _to, fude_zoom_page_half(_page), rde_engine_get_time_now());
}

// Back: to the view the last flight left. A frame dropped since (made for a
// camera that left it empty) is seen from its parent, where it was.
RDE_INTERNAL void fude_zoom_page_go_back(fude_zoom_page* _page) {
    fude_zoom_scene* _s = &_page->scene;
    while(_page->back_count > 0) {
        _page->back_count--;
        u32 _f = fude_zoom_scene_find_frame(_s, _page->back[_page->back_count].frame);
        if(_f == FUDE_ZOOM_NONE) {
            continue;
        }
        fude_zoom_v2 _at = _page->back[_page->back_count].at;
        f64          _z  = _page->back[_page->back_count].z;
        while(fude_zoom_scene_frame(_s, _f)->removed && fude_zoom_scene_frame(_s, _f)->parent != FUDE_ZOOM_NONE) {
            const fude_zoom_frame* _fr = fude_zoom_scene_frame(_s, _f);
            _at = fude_zoom_sim_apply(fude_zoom_sim_from_xform(_fr->xf), _at);
            _z /= fabs(_fr->xf.scale);
            _f  = _fr->parent;
        }
        fude_zoom_page_fly(_page, (fude_zoom_camera){ _f, _at, _z }, false);
        return;
    }
}

// A press on one of the marks (an empty screen's arrows and rings): flown to
// what it points at. True: it was on one.
RDE_INTERNAL b8 fude_zoom_page_mark_press(fude_zoom_page* _page, rde_vec_2F _screen) {
    for(u32 _i = 0; _i < _page->mark_count; _i++) {
        const fude_zoom_nav_mark* _m = &_page->marks[_i];
        const f32 _dx = _screen.x - _m->at.x, _dy = _screen.y - _m->at.y;
        if(_dx * _dx + _dy * _dy <= FUDE_ZOOM_PAGE_MARK_REACH * FUDE_ZOOM_PAGE_MARK_REACH) {
            fude_zoom_page_fly(_page, fude_zoom_nav_framing(fude_zoom_page_half(_page), _m->frame, _m->box, 0.6), true);
            _page->mark_count = 0;
            return true;
        }
    }
    return false;
}

// The depth's levels: the frames above the camera, each at its own zoom 1
// (10^3 a level), the nearest first — the top last.
RDE_INTERNAL void fude_zoom_page_find_crumbs(fude_zoom_page* _page) {
    const fude_zoom_scene* _s = &_page->scene;
    _page->crumb_count = 0;
    fude_zoom_v2 _at = _s->camera.at;
    u32          _f  = _s->camera.frame;
    while(fude_zoom_scene_frame(_s, _f)->parent != FUDE_ZOOM_NONE) {
        const fude_zoom_frame* _fr = fude_zoom_scene_frame(_s, _f);
        _at = fude_zoom_sim_apply(fude_zoom_sim_from_xform(_fr->xf), _at);
        _f  = _fr->parent;
        const b8 _top = fude_zoom_scene_frame(_s, _f)->parent == FUDE_ZOOM_NONE;
        if(_page->crumb_count < FUDE_ZOOM_PAGE_CRUMBS - 1u || _top) {
            const u32 _k = _page->crumb_count < FUDE_ZOOM_PAGE_CRUMBS ? _page->crumb_count++ : FUDE_ZOOM_PAGE_CRUMBS - 1u;
            _page->crumb_views[_k] = (fude_zoom_camera){ _f, _at, 1.0 };
        }
    }
    // At the top already, zoomed in or out: the top at zoom 1.
    if(_page->crumb_count == 0) {
        _page->crumb_views[_page->crumb_count++] = (fude_zoom_camera){ _f, _at, 1.0 };
    }
}

// --- undo, redo, and going to the change ---------------------------------------------------------

// The camera at an undone or redone change, when it is not on screen already.
RDE_INTERNAL void fude_zoom_page_show(fude_zoom_page* _page, const fude_zoom_action* _a) {
    if(_a == NULL || fude_zoom_box_is_empty(_a->box)) {
        return;
    }
    fude_zoom_scene* _s = &_page->scene;
    // Where it is, in the camera's frame: on screen there, nothing to do.
    const fude_zoom_box _in_cam = fude_zoom_sim_box(fude_zoom_scene_sim(_s, _a->frame, _s->camera.frame), _a->box);
    const f64 _size_now = fmax(_in_cam.max_x - _in_cam.min_x, _in_cam.max_y - _in_cam.min_y) * _s->camera.z;
    if(fude_zoom_box_overlaps(_in_cam, fude_zoom_page_view(_page)) && _size_now >= 4.0) {
        return;
    }
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    const f64 _w = fmax(_a->box.max_x - _a->box.min_x, 1e-300), _h = fmax(_a->box.max_y - _a->box.min_y, 1e-300);
    f64 _z = fmin(_half.x * 1.2 / _w, _half.y * 1.2 / _h);
    _z = _z > FUDE_ZOOM_Z_MAX ? FUDE_ZOOM_Z_MAX : (_z < FUDE_ZOOM_Z_MIN ? FUDE_ZOOM_Z_MIN : _z);
    fude_zoom_page_fly(_page, (fude_zoom_camera){ _a->frame, { (_a->box.min_x + _a->box.max_x) * 0.5, (_a->box.min_y + _a->box.max_y) * 0.5 }, _z }, false);
}

RDE_INTERNAL b8 fude_zoom_page_undo(fude_zoom_page* _page) {
    if(!_page->open || _page->drawing || _page->erasing || fude_zoom_select_busy(&_page->selection)) {
        return false;
    }
    const fude_zoom_action* _a = fude_zoom_history_next_undo(&_page->scene);
    const fude_zoom_action  _copy = _a != NULL ? *_a : (fude_zoom_action){ 0 };
    if(!fude_zoom_history_undo(&_page->scene)) {
        return false;
    }
    fude_zoom_page_show(_page, &_copy);
    return true;
}

RDE_INTERNAL b8 fude_zoom_page_redo(fude_zoom_page* _page) {
    if(!_page->open || _page->drawing || _page->erasing || fude_zoom_select_busy(&_page->selection)) {
        return false;
    }
    const fude_zoom_action* _a = fude_zoom_history_next_redo(&_page->scene);
    const fude_zoom_action  _copy = _a != NULL ? *_a : (fude_zoom_action){ 0 };
    if(!fude_zoom_history_redo(&_page->scene)) {
        return false;
    }
    fude_zoom_page_show(_page, &_copy);
    return true;
}

// Everything alive, erased as one undoable step.
RDE_INTERNAL void fude_zoom_page_clear(fude_zoom_page* _page) {
    if(!_page->open || _page->drawing || _page->erasing || fude_zoom_select_busy(&_page->selection)) {
        return;
    }
    fude_zoom_scene* _s = &_page->scene;
    rde_arr _died = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if(_o->kind != FUDE_ZOOM_KIND_FRAME && _o->kind != FUDE_ZOOM_KIND_MARK && (_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {   // (the places stay)
            rde_arr_add(&_died, (any)&_i);
        }
    }
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_died); _i++) {
        fude_zoom_scene_set_alive(_s, ((const u32*)_died.memory)[_i], false);
    }
    fude_zoom_history_push(_s, _s->camera.frame, fude_zoom_page_view(_page), (const u32*)_died.memory, (u32)rde_arr_length(&_died), NULL, 0);
    rde_arr_free(&_died);
}

RDE_INTERNAL void fude_zoom_page_reset_view(fude_zoom_page* _page) {
    if(!_page->open) {
        return;
    }
    fude_zoom_page_fly(_page, (fude_zoom_camera){ _page->scene.home, { 0.0, 0.0 }, 1.0 }, true);
}

// The eraser's next mode, said in a notice.
RDE_INTERNAL void fude_zoom_page_next_eraser(fude_zoom_page* _page) {
    _page->erase_mode = (u8)((_page->erase_mode + 1u) % FUDE_ZOOM_ERASE_COUNT);
    const FUDE_TEXT_ _says[FUDE_ZOOM_ERASE_COUNT] = { FUDE_TEXT_ZOOM_ERASER_PARTIAL, FUDE_TEXT_ZOOM_ERASER_STROKE, FUDE_TEXT_ZOOM_ERASER_TRIM };
    fude_notice_show(fude_text(_says[_page->erase_mode]));
}

// --- the page kind's hooks (extension.h) ----------------------------------------------------------

RDE_INTERNAL b8   fude_zoom_kind_undo(fude_app* _app)            { RDE_UNUSED(_app); return fude_zoom_page_undo(fude_zoom_page_the); }
RDE_INTERNAL b8   fude_zoom_kind_redo(fude_app* _app)            { RDE_UNUSED(_app); return fude_zoom_page_redo(fude_zoom_page_the); }
RDE_INTERNAL b8   fude_zoom_kind_can_undo(const fude_app* _app)  { RDE_UNUSED(_app); return fude_zoom_page_the->open && fude_zoom_history_can_undo(&fude_zoom_page_the->scene); }
RDE_INTERNAL b8   fude_zoom_kind_can_redo(const fude_app* _app)  { RDE_UNUSED(_app); return fude_zoom_page_the->open && fude_zoom_history_can_redo(&fude_zoom_page_the->scene); }
RDE_INTERNAL void fude_zoom_kind_clear(fude_app* _app)           { RDE_UNUSED(_app); fude_zoom_page_clear(fude_zoom_page_the); }
RDE_INTERNAL void fude_zoom_kind_reset_view(fude_app* _app)      { RDE_UNUSED(_app); fude_zoom_page_reset_view(fude_zoom_page_the); }
RDE_INTERNAL void fude_zoom_kind_open(fude_app* _app, u32 _c)    { RDE_UNUSED(_app); fude_zoom_page_open(fude_zoom_page_the, _c); }
RDE_INTERNAL u32  fude_zoom_kind_revision(const fude_app* _app)  { RDE_UNUSED(_app); return fude_zoom_page_the->open ? fude_zoom_page_the->scene.revision : 0u; }
RDE_INTERNAL b8   fude_zoom_kind_busy(const fude_app* _app)      { RDE_UNUSED(_app); return fude_zoom_page_the->drawing || fude_zoom_page_the->erasing || fude_zoom_page_the->shaping || fude_zoom_select_busy(&fude_zoom_page_the->selection); }
RDE_INTERNAL b8   fude_zoom_kind_save(fude_app* _app, b8 _leave) { RDE_UNUSED(_app); return fude_zoom_page_save_now(fude_zoom_page_the, _leave); }

RDE_INTERNAL b8 fude_zoom_kind_selection(const fude_app* _app, rde_vec_2F* _min, rde_vec_2F* _max, b8* _busy) {
    RDE_UNUSED(_app);
    const fude_zoom_selection* _sel = &fude_zoom_page_the->selection;
    fude_zoom_box _b;
    *_busy = fude_zoom_select_busy(_sel);
    if(!fude_zoom_page_the->open || !fude_zoom_select_box(_sel, &_b)) {
        return *_busy;
    }
    // The turning knob over the box is part of it, for the menu: it sits above that.
    *_min = (rde_vec_2F){ (f32)_b.min_x, (f32)_b.min_y };
    *_max = (rde_vec_2F){ (f32)_b.max_x, (f32)_b.max_y + FUDE_ZOOM_SELECT_KNOB + FUDE_ZOOM_SELECT_HANDLE };
    return true;
}

RDE_INTERNAL void fude_zoom_kind_command(fude_app* _app, u32 _cmd, rde_vec_2F _at) {
    fude_zoom_page*      _page = fude_zoom_page_the;
    fude_zoom_selection* _sel  = &_page->selection;
    if(!_page->open || _page->drawing || _page->erasing || fude_zoom_select_busy(&_page->selection)) {
        return;
    }
    switch(_cmd) {
        case FUDE_PAGE_CMD_CUT:        fude_zoom_select_copy(_sel, &_page->scene); fude_zoom_select_delete(_sel, &_page->scene); break;
        case FUDE_PAGE_CMD_COPY:       fude_zoom_select_copy(_sel, &_page->scene); break;
        case FUDE_PAGE_CMD_DUPLICATE:  fude_zoom_select_duplicate(_sel, &_page->scene); break;
        case FUDE_PAGE_CMD_DELETE:     fude_zoom_select_delete(_sel, &_page->scene); break;
        case FUDE_PAGE_CMD_PASTE:      fude_zoom_select_paste(_sel, &_page->scene, _at); break;
        case FUDE_PAGE_CMD_SELECT_ALL: fude_zoom_select_all(_sel, &_page->scene, &_page->renderer); break;
        case FUDE_PAGE_CMD_DESELECT:   fude_zoom_select_clear(_sel); break;
        default: break;
    }
    _page->tool_seen = (u8)_app->ui->bar.tool;   // a Paste or Select all chose the Lasso: not a change to let it go for
    fude_ui_update(_app->ui);
}

RDE_INTERNAL b8 fude_zoom_kind_can_paste(const fude_app* _app) {
    RDE_UNUSED(_app);
    return fude_zoom_select_can_paste(&fude_zoom_page_the->selection);
}

RDE_INTERNAL void fude_zoom_kind_imported(fude_app* _app, u8 _kind, const c8* const* _paths, u32 _count);

const fude_page_kind FUDE_ZOOM_PAGE_KIND = {
    .undo = fude_zoom_kind_undo, .redo = fude_zoom_kind_redo, .can_undo = fude_zoom_kind_can_undo, .can_redo = fude_zoom_kind_can_redo,
    .clear = fude_zoom_kind_clear, .reset_view = fude_zoom_kind_reset_view,
    .open = fude_zoom_kind_open, .revision = fude_zoom_kind_revision, .busy = fude_zoom_kind_busy, .save = fude_zoom_kind_save,
    .selection = fude_zoom_kind_selection, .command = fude_zoom_kind_command, .can_paste = fude_zoom_kind_can_paste,
    .imported = fude_zoom_kind_imported,
};

// --- the page ----------------------------------------------------------------------------------

void fude_zoom_page_init(fude_zoom_page* _page, fude_app* _app) {
    memset(_page, 0, sizeof(*_page));
    _page->app = _app;
    // Ids are (device << 32) | counter: a device number new each launch keeps
    // them unique without keeping it anywhere (the counter goes on from what
    // the file has for the same number).
    _page->device = (u32)time(NULL) ^ (u32)(uintptr_t)_page ^ 0x5A17C0DEu;
    if(_page->device == 0) {
        _page->device = 1u;
    }
    fude_zoom_render_init(&_page->renderer);
    fude_zoom_eraser_init(&_page->eraser);
    fude_ink_init(&_page->capture);
    fude_zoom_select_init(&_page->selection);
    _page->renderer.lifted = &_page->selection.lifted;
    _page->editable     = rde_arr_new(sizeof(fude_zoom_visible), rde_memory_allocator_get_default_std());
    _page->snap_scratch = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    _page->raw          = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    _page->smoothed     = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    _page->smooth_level = FUDE_ZOOM_SMOOTH_DEFAULT;
    _page->place_here   = -1;
    fude_zoom_page_the = _page;
}

void fude_zoom_page_destroy(fude_zoom_page* _page) {
    fude_zoom_page_close(_page);
    fude_zoom_render_destroy(&_page->renderer);
    fude_zoom_eraser_destroy(&_page->eraser);
    fude_ink_destroy(&_page->capture);
    fude_zoom_select_destroy(&_page->selection);
    if(rde_arr_is_inited(&_page->editable)) {
        rde_arr_free(&_page->editable);
    }
    if(rde_arr_is_inited(&_page->snap_scratch)) {
        rde_arr_free(&_page->snap_scratch);
    }
    if(rde_arr_is_inited(&_page->raw)) {
        rde_arr_free(&_page->raw);
    }
    if(rde_arr_is_inited(&_page->smoothed)) {
        rde_arr_free(&_page->smoothed);
    }
    if(fude_zoom_page_the == _page) {
        fude_zoom_page_the = NULL;
    }
}

void fude_zoom_page_trim(fude_zoom_page* _page) {
    fude_zoom_render_trim(&_page->renderer);
}

void fude_zoom_page_look_pen(fude_zoom_page* _page, u8 _phase, rde_vec_2F _screen) {
    _page->capture.sample_time = rde_engine_get_time_now();
    if(_phase == 0u) {
        fude_zoom_page_down(_page, _screen, true, false);
    } else if(_phase == 1u) {
        fude_zoom_page_moved(_page, _screen);
    } else {
        fude_zoom_page_up(_page);
    }
}

// --- fingers ---------------------------------------------------------------------------------

RDE_INTERNAL u32 fude_zoom_page_finger_count(const fude_zoom_page* _page) {
    u32 _n = 0;
    for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_FINGERS; _i++) {
        _n += _page->fingers[_i].active ? 1u : 0u;
    }
    return _n;
}

RDE_INTERNAL fude_zoom_finger* fude_zoom_page_finger(fude_zoom_page* _page, u64 _id) {
    for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_FINGERS; _i++) {
        if(_page->fingers[_i].active && _page->fingers[_i].id == _id) {
            return &_page->fingers[_i];
        }
    }
    return NULL;
}

RDE_INTERNAL void fude_zoom_page_finger_down(fude_zoom_page* _page, u64 _id, rde_vec_2F _at) {
    _page->flight.active = false;
    _page->crumbs_open   = false;
    _page->places_open   = false;
    fude_pagemenu_close_context(&_page->app->ui->page);
    if(fude_zoom_page_finger_count(_page) == 0) {
        _page->tap_start    = rde_engine_get_time_now();
        _page->tap_fingers  = 0;
        _page->tap_spoiled  = false;
        _page->long_pressed = false;
    }
    for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_FINGERS; _i++) {
        if(!_page->fingers[_i].active) {
            _page->fingers[_i] = (fude_zoom_finger){ true, _id, _at, _at };
            break;
        }
    }
    const u32 _n = fude_zoom_page_finger_count(_page);
    _page->tap_fingers = _n > _page->tap_fingers ? _n : _page->tap_fingers;
}

// The first two fingers move the camera: one pans, two pinch round their middle.
RDE_INTERNAL void fude_zoom_page_finger_moved(fude_zoom_page* _page, u64 _id, rde_vec_2F _at) {
    fude_zoom_finger* _f = fude_zoom_page_finger(_page, _id);
    if(_f == NULL) {
        return;
    }
    const f32 _dx = _at.x - _f->start.x, _dy = _at.y - _f->start.y;
    if(_dx * _dx + _dy * _dy > FUDE_ZOOM_PAGE_TAP_SLOP * FUDE_ZOOM_PAGE_TAP_SLOP) {
        _page->tap_spoiled = true;
    }
    fude_zoom_finger* _two[2] = { NULL, NULL };
    u32 _k = 0;
    for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_FINGERS && _k < 2u; _i++) {
        if(_page->fingers[_i].active) {
            _two[_k++] = &_page->fingers[_i];
        }
    }
    fude_zoom_scene* _s = &_page->scene;
    if(_k == 2u && (_f == _two[0] || _f == _two[1])) {
        fude_zoom_finger* _other = _f == _two[0] ? _two[1] : _two[0];
        const fude_zoom_v2 _old_mid = { (_f->at.x + _other->at.x) * 0.5, (_f->at.y + _other->at.y) * 0.5 };
        const fude_zoom_v2 _new_mid = { (_at.x + _other->at.x) * 0.5, (_at.y + _other->at.y) * 0.5 };
        const f64 _old_d = hypot(_f->at.x - _other->at.x, _f->at.y - _other->at.y);
        const f64 _new_d = hypot(_at.x - _other->at.x, _at.y - _other->at.y);
        fude_zoom_camera_pan(_s, (fude_zoom_v2){ _new_mid.x - _old_mid.x, _new_mid.y - _old_mid.y });
        if(_old_d > 1.0 && _new_d > 1.0) {
            fude_zoom_camera_zoom_at(_s, _new_mid, _new_d / _old_d);
        }
    } else if(_k == 1u) {
        fude_zoom_camera_pan(_s, (fude_zoom_v2){ _at.x - _f->at.x, _at.y - _f->at.y });
    }
    _f->at = _at;
    fude_zoom_camera_settle(_s, fude_zoom_page_half(_page));
}

RDE_INTERNAL void fude_zoom_page_finger_up(fude_zoom_page* _page, u64 _id) {
    fude_zoom_finger* _f = fude_zoom_page_finger(_page, _id);
    if(_f == NULL) {
        return;
    }
    _f->active = false;
    if(fude_zoom_page_finger_count(_page) > 0) {
        return;
    }
    // The last one up: a quick tap of one on a mark goes there; of two is undo, of three redo.
    if(!_page->tap_spoiled && rde_engine_get_time_now() - _page->tap_start <= FUDE_ZOOM_PAGE_TAP_TIME) {
        if(_page->tap_fingers == 1u) {
            fude_zoom_page_mark_press(_page, _f->start);
        } else if(_page->tap_fingers == 2u) {
            fude_zoom_page_undo(_page);
        } else if(_page->tap_fingers == 3u) {
            fude_zoom_page_redo(_page);
        }
        fude_ui_update(_page->app->ui);
    }
}

// --- events ----------------------------------------------------------------------------------

void fude_zoom_page_event(fude_zoom_page* _page, rde_event* _event) {
    fude_app* _app = _page->app;
    fude_ui*  _ui  = _app->ui;
    fude_ink* _cap = &_page->capture;

    switch(_event->type) {
        case RDE_EVENT_TYPE_PEN_AXIS: {
            fude_ink_pen_axis(_cap, &_event->data.pen_event_data);
        } break;

        case RDE_EVENT_TYPE_PEN_DOWN: {
            const rde_event_pen* _pen    = &_event->data.pen_event_data;
            const rde_vec_2F     _screen = fude_app_window_to_screen(_app, _pen->position);
            fude_page_pen_came(_app->page);
            _cap->pen_seen    = true;
            _cap->eraser      = _pen->eraser;
            _cap->sample_time = (f64)_event->time_stamp * 1e-9;
            if(fude_ui_hit(_ui, _screen)) {
                _page->pen_on_ui = true;
                break;
            }
            // The pen takes over from a finger drawing or moving the page.
            if(_page->finger_drawing) {
                fude_zoom_page_up(_page);
                _page->finger_drawing = false;
            }
            memset(_page->fingers, 0, sizeof(_page->fingers));
            fude_zoom_page_down(_page, _screen, true, _pen->eraser);
        } break;

        case RDE_EVENT_TYPE_PEN_MOVED: {
            _cap->pen_seen = true;
            {
                // Eased toward the pen while it hovers near where it was (a pen
                // leaving the glass reads unsteadily); a jump is followed at once.
                const rde_vec_2F _at = fude_app_window_to_screen(_app, _event->data.pen_event_data.position);
                const f32 _dx = _at.x - _page->hover_at.x, _dy = _at.y - _page->hover_at.y;
                const b8  _near = _page->hover_on && !_event->data.pen_event_data.down && _dx * _dx + _dy * _dy < 900.0f;
                _page->hover_at     = _near ? (rde_vec_2F){ _page->hover_at.x + _dx * 0.35f, _page->hover_at.y + _dy * 0.35f } : _at;
                _page->hover_eraser = _event->data.pen_event_data.eraser;
                _page->hover_on     = !_page->pen_on_ui && !fude_ui_hit(_ui, _at);
            }
            if(_page->pen_on_ui) {
                break;
            }
            _cap->sample_time = (f64)_event->time_stamp * 1e-9;
            fude_zoom_page_moved(_page, fude_app_window_to_screen(_app, _event->data.pen_event_data.position));
        } break;

        case RDE_EVENT_TYPE_PEN_UP: {
            _cap->pen_seen    = true;
            _page->pen_on_ui  = false;
            _page->hover_quiet = rde_engine_get_time_now() + FUDE_ZOOM_PAGE_HOVER_QUIET;
            if(!_page->finger_drawing) {
                fude_zoom_page_up(_page);
            }
        } break;

        case RDE_EVENT_TYPE_PEN_PROXIMITY_OUT: {
            _page->hover_on = false;
        } break;

        case RDE_EVENT_TYPE_PEN_DOUBLE_TAP: {
            fude_toolbar_pen_double_tap(&_ui->bar, _event->data.pen_event_data.tap_action);
        } break;

        // SDL echoes every pen contact as a touch (from_pen): skipped. A finger
        // landing while the pen draws is a resting palm.
        case RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(!_touch->from_pen) {
                _page->hover_on = false;   // a finger: no pen hovering (nor where it last was)
            }
            if(_touch->from_pen || ((_page->drawing || _page->erasing || fude_zoom_select_busy(&_page->selection)) && !_page->finger_drawing)) {
                break;
            }
            const rde_vec_2F _at = fude_app_touch_to_screen(_touch->init_touch_position);
            if(fude_ui_hit(_ui, _at)) {
                break;
            }
            if(_page->finger_drawing) {
                // A second finger soon after the first began: it was a pinch,
                // not a stroke — the stroke goes, the two move the page.
                if(rde_engine_get_time_now() - _page->stroke_began <= FUDE_ZOOM_PAGE_TWO_FINGER) {
                    fude_zoom_page_draw_cancel(_page);
                    _page->erasing = false;
                    _page->finger_drawing = false;
                    fude_zoom_page_finger_down(_page, _page->writer, _page->erase_last);
                    fude_zoom_page_finger_down(_page, _touch->finger_id, _at);
                }
                break;
            }
            if(_app->finger_writes && fude_zoom_page_finger_count(_page) == 0) {
                _page->finger_drawing = true;
                _page->writer         = _touch->finger_id;
                _page->erase_last     = _at;
                _cap->eraser          = false;
                _cap->sample_time     = rde_engine_get_time_now();
                fude_zoom_page_down(_page, _at, false, false);
                break;
            }
            fude_zoom_page_finger_down(_page, _touch->finger_id, _at);
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_MOVED: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(_touch->from_pen) {
                break;
            }
            const rde_vec_2F _at = fude_app_touch_to_screen(_touch->moved_touch_position);
            if(_page->finger_drawing && _touch->finger_id == _page->writer) {
                _cap->sample_time = rde_engine_get_time_now();
                fude_zoom_page_moved(_page, _at);
                if(!_page->erasing) {
                    _page->erase_last = _at;
                }
                break;
            }
            fude_zoom_page_finger_moved(_page, _touch->finger_id, _at);
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_UP: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(_touch->from_pen) {
                break;
            }
            if(_page->finger_drawing && _touch->finger_id == _page->writer) {
                fude_zoom_page_up(_page);
                _page->finger_drawing = false;
                break;
            }
            fude_zoom_page_finger_up(_page, _touch->finger_id);
        } break;

#if !defined(RDE_PLATFORM_MOBILE)
        case RDE_EVENT_TYPE_MOUSE_BUTTON_PRESSED: {
            const rde_vec_2I _m      = rde_input_mouse_get_position(_app->window);
            const rde_vec_2F _screen = { (f32)_m.x, (f32)_m.y };
            if(_event->handled || fude_ui_hit(_ui, _screen)) {
                break;
            }
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_RIGHT) {
                _page->mouse_panning = true;
                _page->mouse_last    = _screen;
                _page->mouse_down    = _screen;
                _page->mouse_dragged = false;
            } else if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT) {
                _cap->sample_time = rde_engine_get_time_now();
                fude_zoom_page_down(_page, _screen, false, false);
            }
        } break;

        case RDE_EVENT_TYPE_MOUSE_BUTTON_RELEASED: {
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_RIGHT) {
                if(_page->mouse_panning && !_page->mouse_dragged) {
                    fude_pagemenu_open_context(&_ui->page, _page->mouse_down, _page->mouse_down);   // where Paste lands: the screen
                }
                _page->mouse_panning = false;
            } else if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT) {
                fude_zoom_page_up(_page);
            }
        } break;
#endif

        default: break;
    }
}

// --- each frame ---------------------------------------------------------------------------------

void fude_zoom_page_update(fude_zoom_page* _page) {
    fude_app*   _app    = _page->app;
    rde_window* _window = _app->window;
    fude_zoom_scene* _s = &_page->scene;
    if(!_page->open) {
        return;
    }

    // What the pickers brought (Files, Photos): onto the page (fude_zoom_kind_imported).
    fude_import_update(_app);

    // A PNG export drawn last frame: read back and written now.
    if(_page->export_stage == 2u) {
        fude_zoom_page_export_finish(_page);
    }

    // A flight under way (nav.h): the camera where it has it now.
    if(_page->flight.active) {
        fude_zoom_fly_step(&_page->flight, _s, fude_zoom_page_half(_page), rde_engine_get_time_now());
    }

    // The pen resting at a stroke's end: a shape, if it looks like one.
    if(_page->drawing && !_page->snapped && rde_engine_get_time_now() - _page->moved_at >= FUDE_ZOOM_PAGE_HOLD) {
        fude_zoom_page_try_snap(_page);
        _page->moved_at = rde_engine_get_time_now() + 1e9;   // once a rest (moving again tries again)
    }

    // Erase pressed again on the toolbar: the eraser's next mode.
    // (A rebuilt toolbar — another language — counts from 0 again: not a press.)
    if(_app->ui->bar.erase_taps != _page->erase_taps_seen) {
        if(_app->ui->bar.erase_taps > _page->erase_taps_seen) {
            fude_zoom_page_next_eraser(_page);
        }
        _page->erase_taps_seen = _app->ui->bar.erase_taps;
    }
    // One of the bar's tools chosen: the Shapes tool lets go.
    if(_app->ui->bar.tool_taps != _page->tool_taps_seen) {
        if(_app->ui->bar.tool_taps > _page->tool_taps_seen && (_page->shape_tool != 0 || _page->fill_tool)) {
            _page->shape_tool = 0;
            _page->fill_tool  = false;
            fude_toolbar_refresh(&_app->ui->bar);
        }
        _page->tool_taps_seen = _app->ui->bar.tool_taps;
    }

    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_Z)) {
        fude_zoom_page_undo(_page);
    }
    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_Y)) {
        fude_zoom_page_redo(_page);
    }
    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_C)) {
        fude_zoom_page_clear(_page);
    }
    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_R)) {
        fude_zoom_page_reset_view(_page);
    }
    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_E)) {
        fude_zoom_page_next_eraser(_page);
    }
    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_BACKSPACE) || rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_DELETE)) {
        fude_zoom_select_delete(&_page->selection, _s);
    }

    // Another tool chosen: the selection is let go.
    if((u8)_app->ui->bar.tool != _page->tool_seen) {
        _page->tool_seen = (u8)_app->ui->bar.tool;
        if(_app->ui->bar.tool != FUDE_TOOL_LASSO && !fude_zoom_select_busy(&_page->selection)) {
            fude_zoom_select_clear(&_page->selection);
        }
    }

    // One finger held still (fingers moving the page, not drawing): the page's menu.
    if(!_page->long_pressed && !_page->tap_spoiled && fude_zoom_page_finger_count(_page) == 1u &&
       rde_engine_get_time_now() - _page->tap_start >= FUDE_ZOOM_PAGE_LONG_PRESS) {
        for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_FINGERS; _i++) {
            if(_page->fingers[_i].active) {
                _page->long_pressed = true;
                _page->tap_spoiled  = true;   // nor a tap
                fude_pagemenu_open_context(&_app->ui->page, _page->fingers[_i].at, _page->fingers[_i].at);
                break;
            }
        }
    }

#if !defined(RDE_PLATFORM_MOBILE)
    {
        const rde_vec_2I _m      = rde_input_mouse_get_position(_window);
        const rde_vec_2F _screen = { (f32)_m.x, (f32)_m.y };
        _page->hover_at     = _screen;
        _page->hover_eraser = false;
        _page->hover_on     = !fude_ui_hit(_app->ui, _screen);
        if(rde_input_mouse_is_button_pressed(_window, RDE_MOUSE_BUTTON_LEFT) && (_page->drawing || _page->erasing || _page->shaping || fude_zoom_select_busy(&_page->selection))) {
            _page->capture.sample_time = rde_engine_get_time_now();
            fude_zoom_page_moved(_page, _screen);
        }
        if(_page->mouse_panning) {
            if(fabsf(_screen.x - _page->mouse_down.x) + fabsf(_screen.y - _page->mouse_down.y) > 4.0f) {
                _page->mouse_dragged = true;
                _page->flight.active = false;
            }
            fude_zoom_camera_pan(_s, (fude_zoom_v2){ _screen.x - _page->mouse_last.x, _screen.y - _page->mouse_last.y });
            _page->mouse_last = _screen;
        }
        const rde_vec_2F _wheel = rde_input_mouse_get_scrolled(_window);
        if(_wheel.y != 0.0f && !_page->drawing && !_page->erasing && !fude_ui_hit(_app->ui, _screen)) {
            _page->flight.active = false;
            fude_zoom_camera_zoom_at(_s, (fude_zoom_v2){ _screen.x, _screen.y }, pow(FUDE_ZOOM_PAGE_WHEEL, (f64)_wheel.y));
        }
    }
#endif

    if(!_page->drawing && !_page->erasing && !fude_zoom_select_busy(&_page->selection) && !_page->flight.active) {
        fude_zoom_camera_settle(_s, fude_zoom_page_half(_page));
    }
    fude_zoom_select_update(&_page->selection, _s);
    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_B)) {
        fude_zoom_page_go_back(_page);
    }

    // The journal on disk about every second while there is anything new.
    const f64 _now = rde_engine_get_time_now();
    if(!_page->drawing && !_page->erasing && fude_bytes_size(&_s->journal) > 0 && _now - _page->flushed_at >= FUDE_ZOOM_PAGE_FLUSH) {
        fude_zoom_page_save_now(_page, false);
    }
}

// --- drawing ------------------------------------------------------------------------------------

// The paper, in the camera frame: dots, ruled lines or plain squares (Paper on
// the toolbar; how dense, Settings' Lines & squares size). Its lattice is 50·2^n
// frame units — the same at every depth, frames' origins sit on it (scene.h:
// FUDE_ZOOM_PAPER_ALIGN), so it runs on through a change of frame — the n that
// puts its spacing on screen between _min and twice that, and the lattice half
// as fine fading in over it as the zoom grows, so it never jumps.
RDE_INTERNAL void fude_zoom_page_render_paper(fude_zoom_page* _page, rde_window* _window) {
    fude_app*               _app   = _page->app;
    const fude_zoom_camera* _c     = &_page->scene.camera;
    const FUDE_PAPER_       _paper = (FUDE_PAPER_)_app->canvas->page.paper;
    if(_paper == FUDE_PAPER_NONE) {
        return;
    }
    // The coarse lattice's spacing on screen lies in [_min, 2·_min).
    const FUDE_PAPER_SIZE_ _size = _app->canvas->paper_size;
    const f64 _min = _paper == FUDE_PAPER_DOTS ? FUDE_ZOOM_PAGE_DOTS : (_size == FUDE_PAPER_SMALL ? FUDE_ZOOM_PAGE_PAPER_SMALL : _size == FUDE_PAPER_LARGE ? FUDE_ZOOM_PAGE_PAPER_LARGE : FUDE_ZOOM_PAGE_PAPER_MEDIUM);
    f64 _s = FUDE_CANVAS_GRID_SPACING;
    while(_s * _c->z < _min) { _s *= 2.0; }
    while(_s * _c->z >= _min * 2.0) { _s *= 0.5; }
    const f64 _fade = (_s * _c->z - _min) / _min;   // 0 .. 1: how far the finer lattice has come in

    const rde_vec_2I        _size_px = rde_window_get_size(_window);
    const f64               _hw      = (f64)_size_px.x * 0.5, _hh = (f64)_size_px.y * 0.5;
    const fude_zoom_box     _view    = { _c->at.x - _hw / _c->z, _c->at.y - _hh / _c->z, _c->at.x + _hw / _c->z, _c->at.y + _hh / _c->z };
    const rde_color         _ink     = fude_theme_active()->page_dots;
    for(u32 _level = 0; _level < 2u; _level++) {
        // The coarse lattice whole; the finer one between its lines, faded in.
        const f64 _step  = _level == 0 ? _s : _s * 0.5;
        rde_color _color = _ink;
        if(_level == 1) {
            _color.a = (u8)((f64)_ink.a * (_fade < 0.0 ? 0.0 : (_fade > 1.0 ? 1.0 : _fade)));
            if(_color.a == 0) {
                break;
            }
        }
        const f64 _x0 = floor(_view.min_x / _step), _x1 = ceil(_view.max_x / _step);
        const f64 _y0 = floor(_view.min_y / _step), _y1 = ceil(_view.max_y / _step);
        // On screen: (k·step − camera)·z, in f64 — k·step is exact for any k a double holds.
        if(_paper == FUDE_PAPER_DOTS) {
            for(f64 _ky = _y0; _ky <= _y1; _ky += 1.0) {
                for(f64 _kx = _x0; _kx <= _x1; _kx += 1.0) {
                    if(_level == 1 && (((i64)_kx) & 1) == 0 && (((i64)_ky) & 1) == 0) {
                        continue;   // the coarse lattice's own dot
                    }
                    const rde_vec_2F _p = { (f32)((_kx * _step - _c->at.x) * _c->z), (f32)((_ky * _step - _c->at.y) * _c->z) };
                    rde_rendering_2d_draw_rectangle(_p, (rde_vec_2F){ FUDE_CANVAS_GRID_DOT, FUDE_CANVAS_GRID_DOT }, _color);
                }
            }
            continue;
        }
        for(f64 _ky = _y0; _ky <= _y1; _ky += 1.0) {
            if(_level == 1 && (((i64)_ky) & 1) == 0) {
                continue;
            }
            const f32 _y = (f32)((_ky * _step - _c->at.y) * _c->z);
            fude_draw_line((rde_vec_2F){ (f32)-_hw, _y }, (rde_vec_2F){ (f32)_hw, _y }, 0.6f, _color);
        }
        if(_paper == FUDE_PAPER_SQUARES) {
            for(f64 _kx = _x0; _kx <= _x1; _kx += 1.0) {
                if(_level == 1 && (((i64)_kx) & 1) == 0) {
                    continue;
                }
                const f32 _x = (f32)((_kx * _step - _c->at.x) * _c->z);
                fude_draw_line((rde_vec_2F){ _x, (f32)-_hh }, (rde_vec_2F){ _x, (f32)_hh }, 0.6f, _color);
            }
        }
    }
}

// Nothing on screen: arrows at its edge to the nearest things drawn, rings round
// what is there but too small to see (nav.h) — found again once the camera has
// moved and rested a moment, not while drawing or flying.
RDE_INTERNAL void fude_zoom_page_render_marks(fude_zoom_page* _page, fude_zoom_v2 _half) {
    const fude_zoom_camera* _c   = &_page->scene.camera;
    const f64               _now = rde_engine_get_time_now();
    if(_page->renderer.strokes_drawn > 0 || _page->drawing || _page->erasing || _page->flight.active || fude_zoom_select_any(&_page->selection)) {
        _page->mark_count = 0;
        _page->marks_for  = (fude_zoom_camera){ FUDE_ZOOM_NONE, { 0.0, 0.0 }, 0.0 };
        return;
    }
    const b8 _moved = _c->frame != _page->marks_for.frame || _c->at.x != _page->marks_for.at.x || _c->at.y != _page->marks_for.at.y || _c->z != _page->marks_for.z;
    if(_moved) {
        _page->marks_for  = *_c;
        _page->marks_at   = _now;
        _page->mark_count = 0;
    } else if(_page->mark_count == 0 && _now - _page->marks_at >= FUDE_ZOOM_PAGE_MARK_REST && _page->marks_at > 0.0) {
        _page->mark_count = fude_zoom_nav_marks(&_page->scene, _half, FUDE_ZOOM_PAGE_MARK_INSET, _page->marks);
        _page->marks_at   = 0.0;   // found: until the camera moves again
        // An arrow under the toolbar (or anything over the page): in along its
        // way until all of it is clear.
        for(u32 _i = 0; _i < _page->mark_count; _i++) {
            fude_zoom_nav_mark* _m = &_page->marks[_i];
            for(u32 _step = 0; _step < 60u && !_m->ring; _step++) {
                const f32 _r = 19.0f;
                const rde_vec_2F _round[5] = { _m->at, { _m->at.x + _r, _m->at.y }, { _m->at.x - _r, _m->at.y }, { _m->at.x, _m->at.y + _r }, { _m->at.x, _m->at.y - _r } };
                b8 _under = false;
                for(u32 _k = 0; _k < 5u && !_under; _k++) {
                    _under = fude_ui_hit(_page->app->ui, _round[_k]);
                }
                if(!_under) {
                    break;
                }
                _m->at.x -= cosf(_m->angle) * 8.0f;
                _m->at.y -= sinf(_m->angle) * 8.0f;
            }
        }
    }
    const fude_theme* _t = fude_theme_active();
    for(u32 _i = 0; _i < _page->mark_count; _i++) {
        const fude_zoom_nav_mark* _m = &_page->marks[_i];
        if(_m->ring) {
            rde_rendering_2d_draw_circle_with_border(_m->at, 11.0f, 24u, (rde_color){ 0, 0, 0, 0 }, 2.0f, _t->accent, NULL);
            continue;
        }
        // A round card with a chevron pointing out.
        rde_rendering_2d_draw_circle_with_border(_m->at, 17.0f, 28u, _t->surface, 1.0f, _t->outline, NULL);
        const f32 _cx = cosf(_m->angle), _cy = sinf(_m->angle);
        const rde_vec_2F _tip   = { _m->at.x + _cx * 6.0f, _m->at.y + _cy * 6.0f };
        const rde_vec_2F _left  = { _m->at.x - _cx * 3.0f - _cy * 6.0f, _m->at.y - _cy * 3.0f + _cx * 6.0f };
        const rde_vec_2F _right = { _m->at.x - _cx * 3.0f + _cy * 6.0f, _m->at.y - _cy * 3.0f - _cx * 6.0f };
        rde_rendering_2d_draw_line_1(_left, _tip, _t->accent, 2.2f);
        rde_rendering_2d_draw_line_1(_right, _tip, _t->accent, 2.2f);
    }
}

// The brush's circle: how big the pen, the marker or the eraser is, where it
// would draw — while the pen hovers (or the mouse is over the page), and under
// the eraser as it erases. A soft fill and a thin ring, seen on any colour.
RDE_INTERNAL void fude_zoom_page_render_brush(fude_zoom_page* _page) {
    const fude_app*   _app  = _page->app;
    const fude_ink*   _ink  = _app->ink;
    const fude_theme* _t    = fude_theme_active();
    const FUDE_TOOL_  _tool = _app->ui->bar.tool;
    const b8 _quiet  = rde_engine_get_time_now() < _page->hover_quiet;   // the pen just lifted
    const b8 _eraser = _page->erasing || ((_tool == FUDE_TOOL_ERASE || _page->hover_eraser) && _page->hover_on && !_quiet);
    rde_vec_2F _at;
    f32        _r;
    rde_color  _fill, _ring;
    if(_eraser) {
        _at   = _page->erasing ? _page->erase_last : _page->hover_at;
        _r    = _app->ink->eraser_radius;
        _fill = _t->surface;
        _fill.a = 110;
        _ring = _t->accent;
    } else {
        if(!_page->hover_on || _quiet || _page->drawing || _page->shape_tool != 0 || _page->fill_tool || (_tool != FUDE_TOOL_DRAW && _tool != FUDE_TOOL_MARK)) {
            return;
        }
        // As the stroke will be on screen: the marker one width on the page
        // (through the zoom); the pen fixed on the screen, or on the page. With
        // the width following the pressure: its full width, a dot for its lightest.
        const b8  _marking  = _tool == FUDE_TOOL_MARK;
        const b8  _pressure = !_marking && _ink->width_mode == FUDE_INK_WIDTH_MODE_PRESSURE;
        const f64 _z        = _page->scene.camera.z;
        const f32 _radius   = _pressure ? FUDE_INK_WIDTH_BASE + FUDE_INK_WIDTH_PRESSURE : _ink->constant_radius;
        _at   = _page->hover_at;
        _r    = _marking ? (f32)((f64)_ink->marker_radius * _z) : (_ink->brush_scale == FUDE_INK_BRUSH_SCALE_SCREEN ? _radius : (f32)((f64)_radius * _z));
        _ring = fude_theme_resolve(_marking ? _ink->marker_color : _ink->color);
        _fill = _ring;
        _fill.a = _pressure ? 45 : 70;
        _ring.a = 190;
        if(_pressure) {
            const f32 _light = _ink->brush_scale == FUDE_INK_BRUSH_SCALE_SCREEN ? FUDE_INK_WIDTH_BASE : (f32)((f64)FUDE_INK_WIDTH_BASE * _z);
            rde_color _dot = _ring;
            _dot.a = 150;
            rde_rendering_2d_draw_circle(_at, fmaxf(_light, 1.0f), 20u, _dot, NULL);
        }
    }
    _r = fmaxf(_r, 2.0f);
    rde_rendering_2d_draw_circle_with_border(_at, _r, _r > 30.0f ? 48u : 28u, _fill, 1.2f, _ring, NULL);
}

void fude_zoom_page_render(fude_zoom_page* _page, rde_window* _window) {
    if(!_page->open) {
        return;
    }
    fude_zoom_page_render_paper(_page, _window);
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    fude_zoom_render(&_page->renderer, &_page->scene, _half, fude_theme_active()->page);

    // The stroke being drawn, from the capture: its points are units round
    // where it began, in the frame it is going into. Snapped: the shape instead.
    if(_page->drawing && _page->snapped) {
        const fude_zoom_shape_fit _f = fude_zoom_page_fit_now(_page);
        b8 _closed;
        fude_zoom_shape_outline(_f.type, _f.n, _f.count, fude_zoom_shape_segments(400.0), &_page->snap_scratch, &_closed);
        const u32 _k = (u32)rde_arr_length(&_page->snap_scratch);
        if(_k >= 2u) {
            const fude_ink*        _c  = &_page->capture;
            const fude_ink_stroke* _st = fude_ink_stroke_at(_c, fude_ink_stroke_count(_c) - 1u);
            f32 _r = 0.0f;
            for(u32 _i = 0; _i < _st->point_count; _i++) {
                _r = fmaxf(_r, fude_ink_stroke_points(_c, _st)[_i].radius);
            }
            _r = fmaxf(_r * (f32)fude_zoom_sim_scale(fude_zoom_page_draw_sim(_page)), FUDE_ZOOM_RENDER_MIN_RADIUS);
            fude_zoom_page_draw_fit(_page, &_f, _r, fude_theme_resolve(_st->color), false);
        }
    } else if(_page->drawing) {
        const fude_zoom_sim _to = fude_zoom_sim_compose(fude_zoom_camera_sim(&_page->scene.camera), fude_zoom_scene_sim(&_page->scene, _page->draw_frame, _page->scene.camera.frame));
        const fude_zoom_v2  _o  = fude_zoom_sim_apply(_to, _page->origin);
        fude_ink_render(&_page->capture, (rde_vec_2F){ (f32)_o.x, (f32)_o.y }, (f32)fude_zoom_sim_scale(_to),
                        (rde_vec_2F){ (f32)_half.x, (f32)_half.y }, rde_engine_get_time_now(), false);
    }
    if(_page->shaping) {
        const fude_zoom_shape_fit _f = fude_zoom_page_drag_fit(_page);
        const f64 _k = fude_zoom_sim_scale(fude_zoom_page_draw_sim(_page));
        const f32 _r = fmaxf((f32)((f64)fude_zoom_page_brush_radius(_page, _k) * _k), FUDE_ZOOM_RENDER_MIN_RADIUS);
        fude_zoom_page_draw_fit(_page, &_f, _r, fude_theme_resolve(_page->app->ink->color), _page->shape_filled);
    }
    fude_zoom_select_render(&_page->selection, &_page->renderer, &_page->scene, _half);
    fude_zoom_page_render_marks(_page, _half);
    fude_zoom_page_render_brush(_page);
}

// --- getting around: Back and the depth (the extension's widgets, ui.h) ----------------------

#define FUDE_ZOOM_PAGE_UI_MARGIN 16.0f
#define FUDE_ZOOM_PAGE_UI_BACK_W 52.0f    // as the menu button beside it (side.c)
#define FUDE_ZOOM_PAGE_UI_BACK_H 44.0f
#define FUDE_ZOOM_PAGE_UI_CHIP_W 84.0f
#define FUDE_ZOOM_PAGE_UI_CHIP_H 36.0f
#define FUDE_ZOOM_PAGE_UI_GAP    6.0f
#define FUDE_ZOOM_PAGE_UI_PLACES_W 44.0f  // Places' button
#define FUDE_ZOOM_PAGE_UI_ROW_W  230.0f   // ...and its rows
#define FUDE_ZOOM_PAGE_UI_DROP_W 40.0f    // ...and each place's ×

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_page_on_back(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    if(fude_zoom_page_the != NULL) {
        fude_zoom_page_go_back(fude_zoom_page_the);
    }
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// The depth tapped: its levels shown over it (or put away).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_page_on_depth(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    _page->crumbs_open = !_page->crumbs_open;
    _page->places_open = false;
    if(_page->crumbs_open) {
        fude_zoom_page_find_crumbs(_page);
        for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_CRUMBS; _i++) {
            if(_page->crumbs[_i] == NULL || _i >= _page->crumb_count) {
                continue;
            }
            c8 _said[32];
            fude_zoom_nav_say(fude_zoom_nav_depth_of(&_page->scene, _page->crumb_views[_i]), _said, sizeof _said);
            rde_ui_button_set_text(_page->crumbs[_i], _said);
        }
        _page->laid_out = (rde_vec_2F){ 0.0f, 0.0f };   // placed again for how many there are
    }
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_page_on_crumb(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_page* _page = fude_zoom_page_the;
    const u32       _i    = (u32)(uintptr_t)_user_data;
    if(_page != NULL && _i < _page->crumb_count) {
        fude_zoom_page_fly(_page, _page->crumb_views[_i], true);
    }
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// The bookmark (a MARK object: scene.h) the view is at — about the same place
// and zoom — or -1.
RDE_INTERNAL i32 fude_zoom_page_mark_here(const fude_zoom_page* _page) {
    const fude_zoom_scene* _s = &_page->scene;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        f64 _mz;
        if(_o->kind != FUDE_ZOOM_KIND_MARK || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || !fude_zoom_scene_mark_of(_s, _i, &_mz, NULL)) {
            continue;
        }
        const fude_zoom_sim _to = fude_zoom_scene_sim(_s, _o->frame, _s->camera.frame);
        const fude_zoom_v2  _at = fude_zoom_sim_apply(_to, _o->t);
        const f64           _z  = _mz / fude_zoom_sim_scale(_to);   // its zoom in the camera's frame's units
        if(hypot(_at.x - _s->camera.at.x, _at.y - _s->camera.at.y) * _s->camera.z < 24.0 && fabs(log2(_z / _s->camera.z)) < 0.5) {
            return (i32)_i;
        }
    }
    return -1;
}

// The places list as things are now: row 0's words, the places' (the newest first).
RDE_INTERNAL void fude_zoom_page_find_places(fude_zoom_page* _page) {
    const fude_zoom_scene* _s = &_page->scene;
    _page->place_here  = fude_zoom_page_mark_here(_page);
    _page->place_count = 0;
    for(u32 _i = fude_zoom_scene_object_count(_s); _i-- > 0 && _page->place_count < FUDE_ZOOM_PAGE_PLACES;) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if(_o->kind == FUDE_ZOOM_KIND_MARK && (_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
            _page->place_marks[_page->place_count++] = _i;
        }
    }
    if(_page->place_rows[0] == NULL) {
        return;
    }
    rde_ui_button_set_text(_page->place_rows[0], fude_text(_page->place_here >= 0 ? FUDE_TEXT_ZOOM_PLACE_UNMARK : FUDE_TEXT_ZOOM_PLACE_MARK));
    for(u32 _r = 0; _r < _page->place_count; _r++) {
        const u32               _m = _page->place_marks[_r];
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _m);
        f64 _z = 1.0;
        u32 _number = 0;
        fude_zoom_scene_mark_of(_s, _m, &_z, &_number);
        c8 _depth[32], _name[96], _row[160];
        fude_zoom_nav_say(fude_zoom_nav_depth_of(_s, (fude_zoom_camera){ _o->frame, _o->t, _z }), _depth, sizeof _depth);
        FUDE_TEXTF(_name, FUDE_TEXT_ZOOM_PLACE_N, FUDE_TN(_number));
        snprintf(_row, sizeof _row, "%s \xC2\xB7 %s", _name, _depth);
        rde_ui_button_set_text(_page->place_rows[_r + 1u], _row);
    }
    _page->laid_out = (rde_vec_2F){ 0.0f, 0.0f };   // placed again for how many there are
}

// A bookmark let go: an undo step (its box: the view it kept, to fly back to).
RDE_INTERNAL void fude_zoom_page_unmark(fude_zoom_page* _page, u32 _mark) {
    fude_zoom_scene*        _s = &_page->scene;
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _mark);
    f64 _z = 1.0;
    fude_zoom_scene_mark_of(_s, _mark, &_z, NULL);
    const fude_zoom_v2  _half = fude_zoom_page_half(_page);
    const fude_zoom_box _view = { _o->t.x - _half.x / _z, _o->t.y - _half.y / _z, _o->t.x + _half.x / _z, _o->t.y + _half.y / _z };
    const u32           _frame = _o->frame;
    fude_zoom_scene_set_alive(_s, _mark, false);
    fude_zoom_history_push(_s, _frame, _view, &_mark, 1, NULL, 0);
    fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PLACE_UNMARKED));
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_page_on_places(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page != NULL) {
        _page->places_open = !_page->places_open;
        _page->crumbs_open = false;
        if(_page->places_open) {
            fude_zoom_page_find_places(_page);
        }
    }
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// A place's ×: it let go, the list as it is now.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_page_on_place_drop(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_page* _page = fude_zoom_page_the;
    const u32       _row  = (u32)(uintptr_t)_user_data;
    if(_page == NULL || !_page->open || _row >= _page->place_count) {
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    fude_zoom_page_unmark(_page, _page->place_marks[_row]);
    fude_zoom_page_find_places(_page);
    _page->shown_place_rows = !_page->shown_place_rows;   // shown again for how many there are now
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// A row of Places: row 0 marks the view (or unmarks the place it is at); the others fly to theirs.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_page_on_place(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_page* _page = fude_zoom_page_the;
    const u32       _row  = (u32)(uintptr_t)_user_data;
    if(_page == NULL || !_page->open) {
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    fude_zoom_scene* _s = &_page->scene;
    if(_row == 0u) {
        _page->place_here = fude_zoom_page_mark_here(_page);   // as the view is now
        if(_page->place_here >= 0) {
            fude_zoom_page_unmark(_page, (u32)_page->place_here);
        } else {
            // Numbered after every one there has been (a number is never given twice).
            u32 _number = 0;
            for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
                u32 _n = 0;
                if(fude_zoom_scene_mark_of(_s, _i, NULL, &_n)) {
                    _number = _n > _number ? _n : _number;
                }
            }
            const u32 _m = fude_zoom_scene_add_mark(_s, _s->camera.frame, _s->camera.at, _s->camera.z, _number + 1u);
            fude_zoom_history_push(_s, _s->camera.frame, fude_zoom_page_view(_page), NULL, 0, &_m, 1);
            c8 _name[96], _line[160];
            FUDE_TEXTF(_name, FUDE_TEXT_ZOOM_PLACE_N, FUDE_TN(_number + 1u));
            FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_PLACE_MARKED, FUDE_TS(_name));
            fude_notice_show(_line);
        }
        _page->places_open = false;
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    if(_row - 1u < _page->place_count) {
        const u32               _m = _page->place_marks[_row - 1u];
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _m);
        f64 _z = 1.0;
        if(fude_zoom_scene_mark_of(_s, _m, &_z, NULL)) {
            fude_zoom_page_fly(_page, (fude_zoom_camera){ _o->frame, _o->t, _z }, true);
        }
    }
    _page->places_open = false;
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL void fude_zoom_page_ui_style(fude_zoom_page* _page) {
    const fude_theme* _t = fude_theme_active();
    rde_ui_button* const _all[3] = { _page->back_button, _page->depth_button, _page->places_button };
    for(u32 _i = 0; _i < 3u; _i++) {
        if(_all[_i] != NULL) {
            fude_kit_button_colors(_all[_i], _t->surface, 1.0f, _t->outline);
            fude_kit_button_round(_all[_i], 12.0f);
        }
    }
    for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_CRUMBS; _i++) {
        if(_page->crumbs[_i] != NULL) {
            fude_kit_button_colors(_page->crumbs[_i], _t->surface, 1.0f, _t->outline);
            fude_kit_button_round(_page->crumbs[_i], 10.0f);
        }
    }
    for(u32 _i = 0; _i <= FUDE_ZOOM_PAGE_PLACES; _i++) {
        if(_page->place_rows[_i] != NULL) {
            fude_kit_button_colors(_page->place_rows[_i], _i == 0 ? _t->accent : _t->surface, 1.0f, _t->outline);
            fude_kit_button_round(_page->place_rows[_i], 10.0f);
        }
        if(_i < FUDE_ZOOM_PAGE_PLACES && _page->place_drops[_i] != NULL) {
            fude_kit_button_colors(_page->place_drops[_i], _t->surface, 1.0f, _t->outline);
            fude_kit_button_round(_page->place_drops[_i], 10.0f);
        }
    }
}

void fude_zoom_page_ui_build(fude_ui* _ui, rde_ui_node* _root) {
    RDE_UNUSED(_ui);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return;
    }
    _page->back_button = fude_kit_button(_root, fude_text(FUDE_TEXT_ZOOM_GO_BACK), fude_zoom_page_on_back, NULL);
    fude_kit_icon(_page->back_button, FUDE_ICON_BACK, FUDE_KIT_ICON_ONLY, 17.0f);
    _page->depth_button = fude_kit_button(_root, "\xC3\x97" "1", fude_zoom_page_on_depth, NULL);
    for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_CRUMBS; _i++) {
        _page->crumbs[_i] = fude_kit_button(_root, "", fude_zoom_page_on_crumb, (any)(uintptr_t)_i);
        rde_ui_node_set_active(rde_ui_button_as_node(_page->crumbs[_i]), false);
    }
    _page->places_button = fude_kit_button(_root, fude_text(FUDE_TEXT_ZOOM_PLACES), fude_zoom_page_on_places, NULL);
    fude_kit_icon(_page->places_button, FUDE_ICON_BOOKMARK, FUDE_KIT_ICON_ONLY, 16.0f);
    for(u32 _i = 0; _i <= FUDE_ZOOM_PAGE_PLACES; _i++) {
        _page->place_rows[_i] = fude_kit_button(_root, "", fude_zoom_page_on_place, (any)(uintptr_t)_i);
        rde_ui_node_set_active(rde_ui_button_as_node(_page->place_rows[_i]), false);
    }
    for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_PLACES; _i++) {
        _page->place_drops[_i] = fude_kit_button(_root, fude_text(FUDE_TEXT_ZOOM_PLACE_DROP), fude_zoom_page_on_place_drop, (any)(uintptr_t)_i);
        fude_kit_icon(_page->place_drops[_i], FUDE_ICON_CLOSE, FUDE_KIT_ICON_ONLY, 14.0f);
        rde_ui_node_set_active(rde_ui_button_as_node(_page->place_drops[_i]), false);
    }
    rde_ui_node_set_active(rde_ui_button_as_node(_page->back_button), false);
    rde_ui_node_set_active(rde_ui_button_as_node(_page->depth_button), false);
    rde_ui_node_set_active(rde_ui_button_as_node(_page->places_button), false);
    _page->shown_back = _page->shown_depth = _page->shown_crumbs = _page->shown_places = _page->shown_place_rows = false;
    _page->depth_said[0] = 0;
    _page->laid_out      = (rde_vec_2F){ 0.0f, 0.0f };
    fude_zoom_page_ui_style(_page);
}

void fude_zoom_page_ui_forget(fude_ui* _ui) {
    RDE_UNUSED(_ui);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return;
    }
    _page->back_button   = NULL;
    _page->depth_button  = NULL;
    _page->places_button = NULL;
    memset(_page->crumbs, 0, sizeof(_page->crumbs));
    memset(_page->place_rows, 0, sizeof(_page->place_rows));
    memset(_page->place_drops, 0, sizeof(_page->place_drops));
}

void fude_zoom_page_ui_restyle(fude_ui* _ui) {
    RDE_UNUSED(_ui);
    if(fude_zoom_page_the != NULL) {
        fude_zoom_page_ui_style(fude_zoom_page_the);
    }
}

RDE_INTERNAL void fude_zoom_page_ui_show(rde_ui_button* _button, b8 _show, b8* _shown) {
    if(_button != NULL && _show != *_shown) {
        *_shown = _show;
        rde_ui_node_set_active(rde_ui_button_as_node(_button), _show);
    }
}

void fude_zoom_page_ui_update(fude_ui* _ui, b8 _full) {
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || _page->back_button == NULL) {
        return;
    }
    // Placed when the screen changes (or the levels shown do): Back beside the
    // menu button, the depth at the bottom left, its levels over it.
    const rde_vec_2F _screen = fude_kit_screen_size(_ui->window);
    if(_screen.x != _page->laid_out.x || _screen.y != _page->laid_out.y) {
        _page->laid_out = _screen;
        const rde_vec_4I _in = rde_window_get_safe_area_insets(_ui->window);   // left, top, right, bottom
        _page->back_center  = (rde_vec_2F){ (f32)_in.x + FUDE_ZOOM_PAGE_UI_MARGIN + FUDE_ZOOM_PAGE_UI_BACK_W * 1.5f + 8.0f,
                                            _screen.y - (f32)_in.y - FUDE_ZOOM_PAGE_UI_MARGIN * 0.5f - FUDE_ZOOM_PAGE_UI_BACK_H * 0.5f };
        _page->places_center = (rde_vec_2F){ (f32)_in.x + FUDE_ZOOM_PAGE_UI_MARGIN + FUDE_ZOOM_PAGE_UI_PLACES_W * 0.5f,
                                             (f32)_in.w + FUDE_ZOOM_PAGE_UI_MARGIN + FUDE_ZOOM_PAGE_UI_CHIP_H * 0.5f };
        _page->depth_center = (rde_vec_2F){ (f32)_in.x + FUDE_ZOOM_PAGE_UI_MARGIN + FUDE_ZOOM_PAGE_UI_PLACES_W + FUDE_ZOOM_PAGE_UI_GAP + FUDE_ZOOM_PAGE_UI_CHIP_W * 0.5f,
                                            _page->places_center.y };
        _page->crumb_size   = (rde_vec_2F){ FUDE_ZOOM_PAGE_UI_CHIP_W, FUDE_ZOOM_PAGE_UI_CHIP_H };
        _page->place_size   = (rde_vec_2F){ FUDE_ZOOM_PAGE_UI_ROW_W, FUDE_ZOOM_PAGE_UI_CHIP_H };
        fude_kit_place(rde_ui_button_as_node(_page->back_button), _page->back_center, (rde_vec_2F){ FUDE_ZOOM_PAGE_UI_BACK_W, FUDE_ZOOM_PAGE_UI_BACK_H });
        fude_kit_place(rde_ui_button_as_node(_page->depth_button), _page->depth_center, _page->crumb_size);
        fude_kit_place(rde_ui_button_as_node(_page->places_button), _page->places_center, (rde_vec_2F){ FUDE_ZOOM_PAGE_UI_PLACES_W, FUDE_ZOOM_PAGE_UI_CHIP_H });
        for(u32 _i = 0; _i <= FUDE_ZOOM_PAGE_PLACES; _i++) {
            const rde_vec_2F _at = { (f32)_in.x + FUDE_ZOOM_PAGE_UI_MARGIN + FUDE_ZOOM_PAGE_UI_ROW_W * 0.5f,
                                     _page->places_center.y + (f32)(_i + 1u) * (FUDE_ZOOM_PAGE_UI_CHIP_H + FUDE_ZOOM_PAGE_UI_GAP) };
            fude_kit_place(rde_ui_button_as_node(_page->place_rows[_i]), _at, _page->place_size);
            if(_i > 0) {   // a place's ×, just right of its row
                fude_kit_place(rde_ui_button_as_node(_page->place_drops[_i - 1u]),
                               (rde_vec_2F){ _at.x + FUDE_ZOOM_PAGE_UI_ROW_W * 0.5f + FUDE_ZOOM_PAGE_UI_GAP + FUDE_ZOOM_PAGE_UI_DROP_W * 0.5f, _at.y },
                               (rde_vec_2F){ FUDE_ZOOM_PAGE_UI_DROP_W, FUDE_ZOOM_PAGE_UI_CHIP_H });
            }
        }
        for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_CRUMBS; _i++) {
            const rde_vec_2F _at = { _page->depth_center.x, _page->depth_center.y + (f32)(_i + 1u) * (FUDE_ZOOM_PAGE_UI_CHIP_H + FUDE_ZOOM_PAGE_UI_GAP) };
            fude_kit_place(rde_ui_button_as_node(_page->crumbs[_i]), _at, _page->crumb_size);
        }
    }
    const b8 _here = !_full && _page->open;
    fude_zoom_page_ui_show(_page->back_button, _here && _page->back_count > 0, &_page->shown_back);
    // The depth, once it is not the canvas's own (×1) — with a canvas open.
    const f64 _depth = _page->open ? fude_zoom_nav_depth(&_page->scene) : 0.0;
    c8 _said[32];
    fude_zoom_nav_say(_depth, _said, sizeof _said);
    if(strcmp(_said, _page->depth_said) != 0) {
        snprintf(_page->depth_said, sizeof _page->depth_said, "%s", _said);
        rde_ui_button_set_text(_page->depth_button, _said);
    }
    fude_zoom_page_ui_show(_page->depth_button, _here && fabs(_depth) > 0.3, &_page->shown_depth);
    const b8 _crumbs = _here && _page->crumbs_open && _page->shown_depth;
    if(_crumbs != _page->shown_crumbs) {
        _page->shown_crumbs = _crumbs;
        for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_CRUMBS; _i++) {
            rde_ui_node_set_active(rde_ui_button_as_node(_page->crumbs[_i]), _crumbs && _i < _page->crumb_count);
        }
    }
    if(!_page->shown_depth) {
        _page->crumbs_open = false;
    }
    fude_zoom_page_ui_show(_page->places_button, _here, &_page->shown_places);
    const b8 _rows = _here && _page->places_open;
    if(_rows != _page->shown_place_rows) {
        _page->shown_place_rows = _rows;
        for(u32 _i = 0; _i <= FUDE_ZOOM_PAGE_PLACES; _i++) {
            rde_ui_node_set_active(rde_ui_button_as_node(_page->place_rows[_i]), _rows && _i <= _page->place_count);
            if(_i < FUDE_ZOOM_PAGE_PLACES) {
                rde_ui_node_set_active(rde_ui_button_as_node(_page->place_drops[_i]), _rows && _i < _page->place_count);
            }
        }
    }
    if(!_here) {
        _page->places_open = false;
    }
}

b8 fude_zoom_page_ui_hit(const fude_ui* _ui, rde_vec_2F _screen, rde_vec_2F _canvas) {
    RDE_UNUSED(_ui); RDE_UNUSED(_screen);
    const fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return false;
    }
    const f32 _bw = FUDE_ZOOM_PAGE_UI_BACK_W * 0.5f, _bh = FUDE_ZOOM_PAGE_UI_BACK_H * 0.5f;
    if(_page->shown_back && fabsf(_canvas.x - _page->back_center.x) <= _bw && fabsf(_canvas.y - _page->back_center.y) <= _bh) {
        return true;
    }
    const f32 _cw = _page->crumb_size.x * 0.5f, _ch = _page->crumb_size.y * 0.5f;
    if(_page->shown_depth && fabsf(_canvas.x - _page->depth_center.x) <= _cw && fabsf(_canvas.y - _page->depth_center.y) <= _ch) {
        return true;
    }
    if(_page->shown_crumbs) {
        const f32 _top = _page->depth_center.y + (f32)_page->crumb_count * (FUDE_ZOOM_PAGE_UI_CHIP_H + FUDE_ZOOM_PAGE_UI_GAP) + _ch;
        if(fabsf(_canvas.x - _page->depth_center.x) <= _cw && _canvas.y >= _page->depth_center.y && _canvas.y <= _top) {
            return true;
        }
    }
    if(_page->shown_places && fabsf(_canvas.x - _page->places_center.x) <= FUDE_ZOOM_PAGE_UI_PLACES_W * 0.5f && fabsf(_canvas.y - _page->places_center.y) <= _ch) {
        return true;
    }
    if(_page->shown_place_rows) {
        const f32 _left = _page->places_center.x - FUDE_ZOOM_PAGE_UI_PLACES_W * 0.5f;
        const f32 _top  = _page->places_center.y + (f32)(_page->place_count + 1u) * (FUDE_ZOOM_PAGE_UI_CHIP_H + FUDE_ZOOM_PAGE_UI_GAP) + _ch;
        if(_canvas.x >= _left && _canvas.x <= _left + FUDE_ZOOM_PAGE_UI_ROW_W + FUDE_ZOOM_PAGE_UI_GAP + FUDE_ZOOM_PAGE_UI_DROP_W && _canvas.y >= _page->places_center.y && _canvas.y <= _top) {
            return true;
        }
    }
    return false;
}

void fude_zoom_page_look_nav(fude_zoom_page* _page, u8 _what) {
    if(_what == 0u) {
        fude_zoom_page_reset_view(_page);
    } else if(_what == 1u) {
        fude_zoom_page_on_depth(NULL, NULL, NULL);
    } else if(_what == 3u) {
        fude_zoom_page_on_place(NULL, NULL, (any)(uintptr_t)0u);   // mark (or unmark) the view
    } else if(_what == 4u) {
        fude_zoom_page_on_places(NULL, NULL, NULL);
    } else {
        fude_zoom_page_go_back(_page);
    }
}
