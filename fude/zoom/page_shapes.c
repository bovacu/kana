// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"

// ===========================================================================
// Page, shapes: the Shapes tool (a shape dragged out, the toolbar's Shapes choices), a path's or a polygon's nodes,
// stretching a lone shape by a side, containers carrying what is in them, and the Curve tool.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// --- the shapes tool ----------------------------------------------------------------------------

// A shape's line's half-width in _frame's units for one drawn at _at (screen): the Shapes' pen's on paper (× N on the
// 1:N sheet under it), else the brush's.
RDE_INTERNAL f32 fude_zoom_page_line_radius(const fude_zoom_page* _page, u32 _frame, fude_zoom_v2 _at) {
    const f64 _k = fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, _frame));
    if(!(_page->pen_mm > 0.0)) {
        return fude_zoom_page_brush_radius(_page, _frame, _k);
    }
    fude_zoom_sheet _sh;
    const f64 _n   = fude_zoom_page_sheet_under(_page, _at, FUDE_ZOOM_NONE, NULL, &_sh, NULL) != FUDE_ZOOM_NONE ? rde_math_max_f64(_sh.scale, 1.0) : 1.0;
    const f64 _per = fude_zoom_page_home_units(_page, _frame) * FUDE_ZOOM_PAGE_MM_PER_UNIT;   // mm a unit of the frame
    return (f32)(_page->pen_mm * 0.5 * _n / rde_math_max_f64(_per, 1e-300));
}

// How far a guide drawn from _at (screen) reaches each way, screen units: three times a sheet's half diagonal on one
// (across it from anywhere on it, and past), else twenty screens' — never less than three screens'.
RDE_INTERNAL f64 fude_zoom_page_guide_reach(const fude_zoom_page* _page, fude_zoom_v2 _at) {
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    const f64 _screen = rde_math_hypot_f64(_half.x, _half.y);
    fude_zoom_sim   _all;
    fude_zoom_sheet _sh;
    if(fude_zoom_page_sheet_under(_page, _at, FUDE_ZOOM_NONE, &_all, &_sh, NULL) != FUDE_ZOOM_NONE) {
        return rde_math_max_f64(3.0 * rde_math_hypot_f64(_sh.hw, _sh.hh) * fude_zoom_sim_scale(_all), 3.0 * _screen);
    }
    return 20.0 * _screen;
}

// The shape the drag has made so far, screen units: a line from where the pen
// went down to where it is (a connector's too, while dragged; a wall's with its
// thickness, a guide's with its reach); the others filling the box between those.
fude_zoom_shape_fit fude_zoom_page_drag_fit(const fude_zoom_page* _page) {
    fude_zoom_shape_fit _f;
    rde_memset(&_f, 0, sizeof(_f));
    const fude_zoom_v2 _a = { _page->shape_from.x, _page->shape_from.y }, _b = { _page->pen_now.x, _page->pen_now.y };
    const f64 _hw = rde_math_abs_f64(_b.x - _a.x) * 0.5, _hh = rde_math_abs_f64(_b.y - _a.y) * 0.5;
    _f.type = _page->shape_tool == FUDE_ZOOM_SHAPE_ARROW ? (u8)FUDE_ZOOM_SHAPE_LINE : _page->shape_tool;   // a connector shows as its line while dragged
    _f.at   = (fude_zoom_v2){ (_a.x + _b.x) * 0.5, (_a.y + _b.y) * 0.5 };
    if(_f.type == FUDE_ZOOM_SHAPE_LINE) {
        _f.at    = _a;
        _f.n[0]  = _b.x - _a.x;
        _f.n[1]  = _b.y - _a.y;
        _f.count = 2u;
    } else if(_f.type == FUDE_ZOOM_SHAPE_WALL) {
        _f.at    = _a;
        _f.n[0]  = _b.x - _a.x;
        _f.n[1]  = _b.y - _a.y;
        _f.n[2]  = (_page->wall_mm > 0.0 ? _page->wall_mm : 100.0) / rde_math_max_f64(fude_zoom_page_mm_per_point(_page), 1e-300);   // (its thickness, true size)
        _f.count = 3u;
    } else if(_f.type == FUDE_ZOOM_SHAPE_GUIDE) {
        _f.at    = _a;
        _f.n[0]  = _b.x - _a.x;
        _f.n[1]  = _b.y - _a.y;
        _f.n[2]  = fude_zoom_page_guide_reach(_page, _a);
        _f.count = 3u;
    } else if(_f.type == FUDE_ZOOM_SHAPE_RECT) {
        _f.n[0] = _hw; _f.n[1] = _hh; _f.n[2] = 0.0;
        _f.count = 3u;
    } else if(_f.type == FUDE_ZOOM_SHAPE_ELLIPSE) {
        _f.n[0] = _hw; _f.n[1] = _hh;
        _f.count = 2u;
    } else {
        // A triangle: its point at the top of the box (the screen's Y is up).
        const f64 _tri[6] = { -_hw, -_hh, _hw, -_hh, 0.0, _hh };
        rde_memcpy(_f.n, _tri, sizeof(_tri));
        _f.count = 6u;
    }
    return _f;
}

// Home-frame units a unit of _frame is (the canvas's units: what a brush "on the page" is measured in).
f64 fude_zoom_page_home_units(const fude_zoom_page* _page, u32 _frame) {
    const fude_zoom_scene* _s    = &_page->scene;
    const u32              _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    const f64              _k    = fude_zoom_sim_scale(fude_zoom_scene_sim(_s, _frame, _home));
    return _k > 0.0 ? _k : 1.0;
}

// The brush's half-width in _frame's units (_frame_to_screen: that frame's
// scale on the screen): on the screen, the same at any zoom; on the page, a
// size on the canvas itself (its home frame's units, millimetres) — what is
// drawn zoomed in or out the same width.
f32 fude_zoom_page_brush_radius(const fude_zoom_page* _page, u32 _frame, f64 _frame_to_screen) {
    const fude_ink* _brush = _page->app->ink;
    return _brush->brush_scale == FUDE_INK_BRUSH_SCALE_SCREEN ? (f32)((f64)_brush->constant_radius / _frame_to_screen)
                                                               : (f32)((f64)_brush->constant_radius / fude_zoom_page_home_units(_page, _frame));
}

// Half the line's width on the screen (the saw's: half its kerf).
RDE_INTERNAL f64 fude_zoom_page_ruled_half(const fude_zoom_page* _page) {
    const fude_ink*        _ink = _page->app->ink;
    const fude_zoom_scene* _s   = &_page->scene;
    if(_page->saw) {
        return 0.5 * fude_zoom_page_kerf(_page) / rde_math_max_f64(fude_zoom_page_mm_per_point(_page), 1e-300);
    }
    const f64 _r = _page->app->ui->bar.tool == FUDE_TOOL_MARK ? (f64)_ink->marker_radius : (f64)_ink->constant_radius;
    return _ink->brush_scale == FUDE_INK_BRUSH_SCALE_SCREEN ? _r : _r / fude_zoom_page_home_units(_page, _s->camera.frame) * _s->camera.z;
}

// A point on the edge the pen draws along, moved out of the instrument by half
// the line's width (the saw's: half its kerf), so the line lies beside the edge
// as a pencil's against a ruler — not half under it (a stencil's line: on it).
// Screen points.
rde_vec_2F fude_zoom_page_ruled_beside(const fude_zoom_page* _page, fude_zoom_v2 _on) {
    const fude_zoom_v2 _out  = fude_zoom_instruments_ruled_outward(&_page->instruments);
    const f64          _half = fude_zoom_instruments_ruled_traced(&_page->instruments) ? 0.0 : fude_zoom_page_ruled_half(_page);   // (a stencil's: on its line)
    return (rde_vec_2F){ (f32)(_on.x + _out.x * _half), (f32)(_on.y + _out.y * _half) };
}

// Turned onto a crossing edge (chained instruments): the corner — where the line beside the
// edge it left meets the line beside the one it takes (their mitre) — drawn first.
void fude_zoom_page_ruled_corner(fude_zoom_page* _page) {
    fude_zoom_v2 _x, _n1;
    if(!fude_zoom_instruments_ruled_turn(&_page->instruments, &_x, &_n1)) {
        return;
    }
    const fude_zoom_v2 _n2   = fude_zoom_instruments_ruled_outward(&_page->instruments);
    const f64          _half = fude_zoom_instruments_ruled_traced(&_page->instruments) ? 0.0 : fude_zoom_page_ruled_half(_page);
    const f64          _d    = 1.0 + _n1.x * _n2.x + _n1.y * _n2.y;
    const fude_zoom_v2 _m    = _d > 0.3 ? (fude_zoom_v2){ (_n1.x + _n2.x) * _half / _d, (_n1.y + _n2.y) * _half / _d } : (fude_zoom_v2){ _n2.x * _half, _n2.y * _half };
    _page->ruled_straight = false;   // (a line with a corner: every point kept)
    _page->ruled_round    = false;
    fude_zoom_page_pin_ruled(_page);   // (the edge it turned onto held on the drawing)
    if(_page->drawing) {
        fude_zoom_page_draw_moved(_page, (rde_vec_2F){ (f32)(_x.x + _m.x), (f32)(_x.y + _m.y) });
    }
}

// The pen down with the Shapes tool: the drag begins where it snaps to, in the frame the shape will go in.
void fude_zoom_page_shape_down(fude_zoom_page* _page, rde_vec_2F _screen) {
    _screen = fude_zoom_page_snapped(_page, _screen);
    fude_zoom_scene*   _s  = &_page->scene;
    const fude_zoom_v2 _at = fude_zoom_camera_to_frame(&_s->camera, (fude_zoom_v2){ _screen.x, _screen.y });
    const f64          _near = 4.0 / _s->camera.z;
    _page->draw_frame = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _at.x - _near, _at.y - _near, _at.x + _near, _at.y + _near });
    _page->shaping    = true;
    _page->shape_from = _screen;
    _page->pen_now    = _screen;
}

// The thing drawn under _screen in _frame (a shape, a text, a picture; not a
// connector, a line, a measurement or a guide): the one on top, or FUDE_ZOOM_NONE.
// (Something under it first, the topmost; only then a container round it — an area, a sheet, a Kanban column —
// the smallest: what it is most nearly on. A sheet goes under what is on it, an area round a sheet: either way a
// tap on the sheet takes the sheet, a tap on what is on it takes that.)
RDE_INTERNAL u32 fude_zoom_page_thing_at(fude_zoom_page* _page, u32 _frame, rde_vec_2F _screen) {
    const fude_zoom_scene* _s  = &_page->scene;
    const fude_zoom_v2     _at = fude_zoom_sim_apply(fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _frame)), (fude_zoom_v2){ _screen.x, _screen.y });
    rde_arr _found = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_scene_query(_s, _frame, (fude_zoom_box){ _at.x, _at.y, _at.x, _at.y }, &_found);
    u32 _best = FUDE_ZOOM_NONE, _round = FUDE_ZOOM_NONE;
    u64 _z    = 0;
    f64 _least = 1e300;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_found); _i++) {
        const u32               _o  = ((const u32*)_found.memory)[_i];
        const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _o);
        const b8 _kind = (_ob->kind == FUDE_ZOOM_KIND_SHAPE && _ob->channels != FUDE_ZOOM_SHAPE_ARROW && _ob->channels != FUDE_ZOOM_SHAPE_LINE &&
                          _ob->channels != FUDE_ZOOM_SHAPE_DIMENSION && _ob->channels != FUDE_ZOOM_SHAPE_RADIAL && _ob->channels != FUDE_ZOOM_SHAPE_ANGLE &&
                          _ob->channels != FUDE_ZOOM_SHAPE_GUIDE && !fude_zoom_shape_is_attribute(_ob->channels)) ||
                         _ob->kind == FUDE_ZOOM_KIND_TEXT || _ob->kind == FUDE_ZOOM_KIND_IMAGE;
        if(!_kind || !(_ob->flags & FUDE_ZOOM_FLAG_ALIVE) || !fude_zoom_scene_touchable(_s, _ob)) {
            continue;
        }
        if(fude_zoom_page_container(_s, _o)) {
            const f64 _size = (_ob->box.max_x - _ob->box.min_x) * (_ob->box.max_y - _ob->box.min_y);
            if(_size < _least) {
                _least = _size;
                _round = _o;
            }
        } else if(_best == FUDE_ZOOM_NONE || _ob->z >= _z) {
            _best = _o;
            _z    = _ob->z;
        }
    }
    rde_arr_free(&_found);
    return _best != FUDE_ZOOM_NONE ? _best : _round;
}

// The guide nearest _screen, no further than _reach (screen units), at any depth the last draw showed (_page->editable
// filled); or none.
RDE_INTERNAL u32 fude_zoom_page_guide_at(fude_zoom_page* _page, rde_vec_2F _screen, f64 _reach) {
    const fude_zoom_scene* _s = &_page->scene;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _found = rde_arr_new(sizeof(u32), _heap), _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    u32 _best = FUDE_ZOOM_NONE;
    f64 _least = _reach;
    for(u32 _v = 0; _v < (u32)rde_arr_length(&_page->editable); _v++) {
        const fude_zoom_visible _vis = ((const fude_zoom_visible*)_page->editable.memory)[_v];
        const f64 _k = fude_zoom_sim_scale(_vis.to_screen);
        if(!(_k > 0.0)) {
            continue;
        }
        const fude_zoom_v2 _at = fude_zoom_sim_apply(fude_zoom_sim_inverse(_vis.to_screen), (fude_zoom_v2){ _screen.x, _screen.y });
        const f64 _r = _reach / _k;
        rde_arr_clear(&_found);
        fude_zoom_scene_query(_s, _vis.frame, (fude_zoom_box){ _at.x - _r, _at.y - _r, _at.x + _r, _at.y + _r }, &_found);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_found); _i++) {
            const u32 _o = ((const u32*)_found.memory)[_i];
            const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _o);
            if(_ob->kind != FUDE_ZOOM_KIND_SHAPE || _ob->channels != FUDE_ZOOM_SHAPE_GUIDE || !fude_zoom_scene_touchable(_s, _ob)) {
                continue;
            }
            b8 _closed;
            rde_arr_clear(&_pts);
            fude_zoom_scene_shape_outline(_s, _o, 1u, &_pts, &_closed);
            if(rde_arr_length(&_pts) != 2u) {
                continue;
            }
            const fude_zoom_v2 _a = fude_zoom_sim_apply(_vis.to_screen, ((const fude_zoom_v2*)_pts.memory)[0]);
            const fude_zoom_v2 _b = fude_zoom_sim_apply(_vis.to_screen, ((const fude_zoom_v2*)_pts.memory)[1]);
            const f64 _dd = rde_math_point_segment_distance_2d_f64((fude_zoom_v2){ _screen.x, _screen.y }, _a, _b);
            if(_dd <= _least) {
                _least = _dd;
                _best  = _o;
            }
        }
    }
    rde_arr_free(&_found);
    rde_arr_free(&_pts);
    return _best;
}

// The thing under _screen as a tap with the Lasso takes it: a guide right under it first (a thin line: what is round
// it would be taken instead), else the topmost at any depth shown; FUDE_ZOOM_NONE: nothing.
u32 fude_zoom_page_under(fude_zoom_page* _page, rde_vec_2F _screen) {
    rde_arr_clear(&_page->editable);
    fude_zoom_render_editable(&_page->renderer, &_page->scene, &_page->editable);
    const u32 _guide = fude_zoom_page_guide_at(_page, _screen, 10.0);
    if(_guide != FUDE_ZOOM_NONE) {
        return _guide;
    }
    u32 _hit = FUDE_ZOOM_NONE, _depth = 0;
    u64 _z = 0;
    for(u32 _v = 0; _v < (u32)rde_arr_length(&_page->editable); _v++) {
        const fude_zoom_visible _vis = ((const fude_zoom_visible*)_page->editable.memory)[_v];
        const u32 _o = fude_zoom_page_thing_at(_page, _vis.frame, _screen);
        if(_o == FUDE_ZOOM_NONE) {
            continue;
        }
        const u64 _oz = fude_zoom_scene_object(&_page->scene, _o)->z;
        if(_hit == FUDE_ZOOM_NONE || _vis.depth > _depth || (_vis.depth == _depth && _oz >= _z)) {
            _hit   = _o;
            _depth = _vis.depth;
            _z     = _oz;
        }
    }
    return _hit;
}

// A tap with the Lasso: the thing under it selected whole — a board, a note or a text, a picture, a shape, a guide —;
// on nothing, nothing.
void fude_zoom_page_tap_select(fude_zoom_page* _page, rde_vec_2F _screen) {
    const u32 _hit = fude_zoom_page_under(_page, _screen);
    fude_zoom_select_clear(&_page->selection);
    if(_hit != FUDE_ZOOM_NONE) {
        rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _hit });
    }
    fude_zoom_select_update(&_page->selection, &_page->scene);
}

// A connector dragged from one thing to another: joined to both (either end
// loose when it starts or ends on nothing), from edge to edge, aimed at their middles.
RDE_INTERNAL void fude_zoom_page_connector_up(fude_zoom_page* _page) {
    fude_zoom_scene*    _s    = &_page->scene;
    const u32           _f    = _page->draw_frame;
    const fude_zoom_sim _back = fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _f));
    const u32 _from = fude_zoom_page_thing_at(_page, _f, _page->shape_from);
    u32       _to   = fude_zoom_page_thing_at(_page, _f, _page->pen_now);
    if(_to == _from) {
        _to = FUDE_ZOOM_NONE;
    }
    fude_zoom_v2 _a = fude_zoom_sim_apply(_back, (fude_zoom_v2){ _page->shape_from.x, _page->shape_from.y });
    fude_zoom_v2 _b = fude_zoom_sim_apply(_back, (fude_zoom_v2){ _page->pen_now.x, _page->pen_now.y });
    const fude_zoom_v2 _am = _from != FUDE_ZOOM_NONE ? fude_zoom_connect_middle(_s, _from) : _a;
    const fude_zoom_v2 _bm = _to != FUDE_ZOOM_NONE ? fude_zoom_connect_middle(_s, _to) : _b;
    if(_from != FUDE_ZOOM_NONE) { _a = fude_zoom_connect_edge(_s, _from, _bm); }
    if(_to != FUDE_ZOOM_NONE)   { _b = fude_zoom_connect_edge(_s, _to, _am); }
    const f64 _k = fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, _f));
    if(rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y) * _k < 4.0) {
        return;
    }
    const u32 _o = fude_zoom_connect_add(_s, _f, _a, _b, _from, _to, _page->connector_heads, _page->app->ink->color,
                                         (f32)rde_math_max_f64((f64)fude_zoom_page_brush_radius(_page, _f, _k) * 0.6, 0.6 / _k));
    fude_zoom_history_push(_s, _f, fude_zoom_scene_object(_s, _o)->box, NULL, 0, &_o, 1);
}

// The pen lifted with the Shapes tool: the shape dragged out put down (one undo step), a tap making none. A connector
// is joined to what it was dragged between; a wall is solid; with the Saw on, the shape cuts instead. A new line's
// length shows on the line chip for a while.
void fude_zoom_page_shape_up(fude_zoom_page* _page) {
    if(!_page->shaping) {
        return;
    }
    _page->shaping = false;
    _page->snap_hit.kind = FUDE_ZOOM_SNAP_NONE;
    if(rde_math_abs_f32(_page->pen_now.x - _page->shape_from.x) + rde_math_abs_f32(_page->pen_now.y - _page->shape_from.y) < 6.0f) {
        return;   // a tap: no shape
    }
    if(_page->shape_tool == FUDE_ZOOM_SHAPE_ARROW) {
        fude_zoom_page_connector_up(_page);
        return;
    }
    const fude_zoom_shape_fit _f  = fude_zoom_page_drag_fit(_page);
    const fude_zoom_sim       _to = fude_zoom_page_draw_sim(_page);
    if(_f.type == FUDE_ZOOM_SHAPE_WALL) {
        // (solid, its hairline its fill's colour: walls meeting show as one)
        const f64 _k = fude_zoom_sim_scale(_to);
        const u32 _o = fude_zoom_page_add_fit(_page, _page->draw_frame, _to, &_f, _page->app->ink->color, (f32)(0.3 / _k), FUDE_ZOOM_FLAG_FILLED);
        fude_zoom_history_push(&_page->scene, _page->draw_frame, fude_zoom_scene_object(&_page->scene, _o)->box, NULL, 0, &_o, 1);
        return;
    }
    if(_f.type == FUDE_ZOOM_SHAPE_GUIDE) {
        const f64 _k = fude_zoom_sim_scale(_to);
        const u32 _o = fude_zoom_page_add_fit(_page, _page->draw_frame, _to, &_f, _page->app->ink->color, (f32)(0.5 / _k), 0u);
        fude_zoom_history_push(&_page->scene, _page->draw_frame, fude_zoom_scene_object(&_page->scene, _o)->box, NULL, 0, &_o, 1);
        return;
    }
    if(_page->saw) {
        // The Saw: a line cuts along, a closed shape cuts out its inside (its outline in the frame's units).
        rde_arr _out = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
        b8 _closed = false;
        fude_zoom_shape_outline(_f.type, _f.n, _f.count, 96u, &_out, &_closed);
        const fude_zoom_sim _bk = fude_zoom_sim_inverse(_to);
        const f64 _co = rde_math_cos_f64(_f.rotation), _si = rde_math_sin_f64(_f.rotation);
        fude_zoom_v2* _q = (fude_zoom_v2*)_out.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_out); _i++) {
            const fude_zoom_v2 _l = _q[_i];
            _q[_i] = fude_zoom_sim_apply(_bk, (fude_zoom_v2){ _f.at.x + _l.x * _co - _l.y * _si, _f.at.y + _l.x * _si + _l.y * _co });
        }
        fude_zoom_page_saw_line(_page, _page->draw_frame, _q, (u32)rde_arr_length(&_out), _closed);
        rde_arr_free(&_out);
        return;
    }
    _page->scene.style = fude_zoom_q_of(_page->line_style, _page->hatch);   // (its line as the Shapes say)
    const u32 _o = fude_zoom_page_add_fit(_page, _page->draw_frame, _to, &_f, _page->app->ink->color,
                                          fude_zoom_page_line_radius(_page, _page->draw_frame, (fude_zoom_v2){ _page->shape_from.x, _page->shape_from.y }),
                                          _page->shape_filled ? FUDE_ZOOM_FLAG_FILLED : 0u);
    _page->scene.style = 0;
    fude_zoom_history_push(&_page->scene, _page->draw_frame, fude_zoom_scene_object(&_page->scene, _o)->box, NULL, 0, &_o, 1);
    if(_f.type == FUDE_ZOOM_SHAPE_LINE) {
        _page->line_chip_object = _o;
        _page->line_chip_until  = rde_engine_get_time_now() + FUDE_ZOOM_PAGE_LINE_CHIP;
    }
}

// Declared ahead (fude_zoom_page_restyle uses it): what the lasso holds made again as fude_zoom_page_relook_into says,
// pushed as one undo step.
RDE_INTERNAL void fude_zoom_page_relook(fude_zoom_page* _page, i32 _style, i32 _hatch, f64 _pen_mm);

// What the lasso holds given line style _style (shape.h's FUDE_ZOOM_LINE_), its hatch and pen as they are.
RDE_INTERNAL void fude_zoom_page_restyle(fude_zoom_page* _page, u8 _style) {
    fude_zoom_page_relook(_page, (i32)_style, -1, 0.0);
}

// What the lasso holds that has a line of its own (a shape: not a symbol, a connector, a measurement, a guide) made
// again where it is: its line's style _style (-1: as it is), hatched _hatch (-1: as it is), its line a pen _pen_mm
// wide on paper (0: as it is; × N on a 1:N sheet) — one undo step.
//
// Gathered: what died and what was born for it (connectors joined to them too) into _dead_all and _born_all,
// over _box (the camera's frame's) — its undo step left to whoever asked (fude_zoom_page_relook's own).
void fude_zoom_page_relook_into(fude_zoom_page* _page, i32 _style, i32 _hatch, f64 _pen_mm, rde_arr* _dead_all, rde_arr* _born_all, fude_zoom_box* _box_all) {
    fude_zoom_scene* _s = &_page->scene;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _dead = rde_arr_new(sizeof(u32), _heap), _born = rde_arr_new(sizeof(u32), _heap), _num = rde_arr_new(sizeof(f64), _heap);
    fude_zoom_box _box = fude_zoom_box_empty();
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        fude_zoom_pick* _pick = &((fude_zoom_pick*)_page->selection.picks.memory)[_i];
        const fude_zoom_object _old = *fude_zoom_scene_object(_s, _pick->object);
        const i8 _q = fude_zoom_q_of(_style >= 0 ? (u8)_style : fude_zoom_line_style_of(_old.q), _hatch >= 0 ? (u8)_hatch : fude_zoom_hatch_of(_old.q));
        f32 _radius = _old.radius;
        if(_pen_mm > 0.0) {
            const fude_zoom_sim _to = fude_zoom_page_frame_sim(_page, _old.frame);
            const fude_zoom_v2  _mid = fude_zoom_sim_apply(_to, (fude_zoom_v2){ (_old.box.min_x + _old.box.max_x) * 0.5, (_old.box.min_y + _old.box.max_y) * 0.5 });
            fude_zoom_sheet _sh;
            const f64 _n   = fude_zoom_page_sheet_under(_page, _mid, FUDE_ZOOM_NONE, NULL, &_sh, NULL) != FUDE_ZOOM_NONE ? rde_math_max_f64(_sh.scale, 1.0) : 1.0;
            const f64 _per = fude_zoom_page_home_units(_page, _old.frame) * FUDE_ZOOM_PAGE_MM_PER_UNIT;
            _radius = (f32)(_pen_mm * 0.5 * _n / rde_math_max_f64(_per * _old.scale, 1e-300));
        }
        if(!(_old.flags & FUDE_ZOOM_FLAG_ALIVE) || _old.kind != FUDE_ZOOM_KIND_SHAPE || (_old.q == _q && _old.radius == _radius) ||
           _old.channels == FUDE_ZOOM_SHAPE_SYMBOL || _old.channels == FUDE_ZOOM_SHAPE_ARROW || _old.channels == FUDE_ZOOM_SHAPE_DIMENSION ||
           _old.channels == FUDE_ZOOM_SHAPE_RADIAL || _old.channels == FUDE_ZOOM_SHAPE_ANGLE || _old.channels == FUDE_ZOOM_SHAPE_GUIDE ||
           _old.channels == FUDE_ZOOM_SHAPE_SHEET) {
            continue;   // (a sheet's edge is its own: a fine line, never hatched)
        }
        const u32 _count = fude_zoom_scene_shape_numbers_all(_s, _pick->object, &_num);
        const u16 _layer = _s->layer;
        _s->layer = _old.layer;
        _s->style = _q;
        const u32 _made = fude_zoom_scene_add_shape_fill(_s, _old.frame, fude_zoom_scene_place_of(_s, _pick->object), _old.channels, (const f64*)_num.memory, _count,
                                                         _old.color, _radius, (u8)(_old.flags & (FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN)), _old.fill, _old.z);
        _s->layer = _layer;
        _s->style = 0;
        fude_zoom_scene_set_alive(_s, _pick->object, false);
        rde_arr_add(&_dead, (any)&_pick->object);
        rde_arr_add(&_born, (any)&_made);
        _box   = fude_zoom_box_union(_box, fude_zoom_sim_box(fude_zoom_scene_sim(_s, _old.frame, _s->camera.frame), _old.box));
        _pick->object = _made;
    }
    const u32 _n = (u32)rde_arr_length(&_born);
    if(_n > 0u) {
        // (connectors and dimensions joined to them, joined again)
        rde_arr _d2 = rde_arr_new(sizeof(u32), _heap), _b2 = rde_arr_new(sizeof(u32), _heap);
        rde_memcpy(rde_arr_add_n(&_d2, _n), _dead.memory, (usize)_n * sizeof(u32));
        rde_memcpy(rde_arr_add_n(&_b2, _n), _born.memory, (usize)_n * sizeof(u32));
        fude_zoom_page_rebind(_page, (const u32*)_dead.memory, (const u32*)_born.memory, _n, &_d2, &_b2);
        rde_memcpy(rde_arr_add_n(_dead_all, rde_arr_length(&_d2)), _d2.memory, (usize)rde_arr_length(&_d2) * sizeof(u32));
        rde_memcpy(rde_arr_add_n(_born_all, rde_arr_length(&_b2)), _b2.memory, (usize)rde_arr_length(&_b2) * sizeof(u32));
        *_box_all = fude_zoom_box_union(*_box_all, _box);
        rde_arr_free(&_d2);
        rde_arr_free(&_b2);
        fude_zoom_select_update(&_page->selection, _s);
    }
    rde_arr_free(&_dead);
    rde_arr_free(&_born);
    rde_arr_free(&_num);
}

// What the lasso holds made again with line style _style, hatch _hatch and pen _pen_mm (fude_zoom_page_relook_into),
// pushed as one undo step over the camera's frame. Nothing while the lasso holds nothing or is busy.
RDE_INTERNAL void fude_zoom_page_relook(fude_zoom_page* _page, i32 _style, i32 _hatch, f64 _pen_mm) {
    fude_zoom_scene* _s = &_page->scene;
    if(!fude_zoom_select_any(&_page->selection) || fude_zoom_select_busy(&_page->selection)) {
        return;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _dead = rde_arr_new(sizeof(u32), _heap), _born = rde_arr_new(sizeof(u32), _heap);
    fude_zoom_box _box = fude_zoom_box_empty();
    fude_zoom_page_relook_into(_page, _style, _hatch, _pen_mm, &_dead, &_born, &_box);
    if(rde_arr_length(&_born) > 0u) {
        fude_zoom_history_push(_s, _s->camera.frame, _box, (const u32*)_dead.memory, (u32)rde_arr_length(&_dead), (const u32*)_born.memory, (u32)rde_arr_length(&_born));
    }
    rde_arr_free(&_dead);
    rde_arr_free(&_born);
}

// ISO pens' widths (mm on paper): the Shapes' choices 10 to 13.
RDE_INTERNAL const f64 FUDE_ZOOM_PAGE_PENS[4] = { 0.25, 0.35, 0.5, 0.7 };

// The toolbar's Shapes (extension.h): its choices, and which show chosen.
RDE_INTERNAL const u8 FUDE_ZOOM_SHAPE_CHOICES[5] = { FUDE_ZOOM_SHAPE_LINE, FUDE_ZOOM_SHAPE_RECT, FUDE_ZOOM_SHAPE_ELLIPSE, FUDE_ZOOM_SHAPE_POLYGON, FUDE_ZOOM_SHAPE_ARROW };

// The toolbar's Shapes: choice _index of FUDE_ZOOM_SHAPES_CHOICES taken. 0 to 4 a shape the pen drags out, 5 the
// Curve (chosen again: off), 6 Filled, 7 to 9 a line style, 10 to 13 a pen (chosen again: the brush's), 14 and 15 a
// hatch (chosen again: none); a style, pen or hatch is also given to what the lasso holds. Returns true to keep the
// panel open.
b8 fude_zoom_shapes_choose(fude_app* _app, u32 _index) {
    RDE_UNUSED(_app);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_index == 5u) {
        // The Curve: points tapped, a smooth line through them.
        const b8 _on = !_page->curve_tool;
        fude_zoom_page_pen_modes_off(_page);
        _page->curve_tool = _on;
        if(_on) {
            fude_zoom_page_pen_under(_page);
            fude_notice_show(fude_text(FUDE_TEXT_ZOOM_CURVE_HOW));
        }
        return false;
    }
    if(_index < 5u) {
        fude_zoom_page_pen_modes_off(_page);
        _page->shape_tool = FUDE_ZOOM_SHAPE_CHOICES[_index];
        fude_zoom_page_pen_under(_page);
        return false;
    }
    if(_index >= 7u && _index < 7u + FUDE_ZOOM_LINE_STYLES) {
        // A line's style: new shapes drawn in it — and what the lasso holds, made so (the panel stays).
        _page->line_style = (u8)(_index - 7u);
        fude_zoom_page_restyle(_page, _page->line_style);
        return true;
    }
    if(_index >= 10u && _index < 14u) {
        // A pen's width on paper (chosen again: the brush's) — and what the lasso holds, made so.
        const f64 _mm = FUDE_ZOOM_PAGE_PENS[_index - 10u];
        _page->pen_mm = _page->pen_mm == _mm ? 0.0 : _mm;
        if(_page->pen_mm > 0.0) {
            fude_zoom_page_relook(_page, -1, -1, _page->pen_mm);
        }
        return true;
    }
    if(_index == 14u || _index == 15u) {
        // Hatched, or cross-hatched (chosen again: neither) — and what the lasso holds, made so.
        const u8 _kind = _index == 14u ? (u8)FUDE_ZOOM_HATCH_DIAGONAL : (u8)FUDE_ZOOM_HATCH_CROSS;
        _page->hatch = _page->hatch == _kind ? (u8)FUDE_ZOOM_HATCH_NONE : _kind;
        fude_zoom_page_relook(_page, -1, (i32)_page->hatch, 0.0);
        return true;
    }
    _page->shape_filled = !_page->shape_filled;   // Filled: a toggle, the panel stays
    return true;
}

// The toolbar's Shapes: whether choice _index shows chosen.
b8 fude_zoom_shapes_chosen(const fude_app* _app, u32 _index) {
    RDE_UNUSED(_app);
    const fude_zoom_page* _page = fude_zoom_page_the;
    if(_index >= 10u && _index < 14u) {
        return _page->pen_mm == FUDE_ZOOM_PAGE_PENS[_index - 10u];
    }
    if(_index == 14u || _index == 15u) {
        return _page->hatch == (_index == 14u ? (u8)FUDE_ZOOM_HATCH_DIAGONAL : (u8)FUDE_ZOOM_HATCH_CROSS);
    }
    return _index < 5u ? _page->shape_tool == FUDE_ZOOM_SHAPE_CHOICES[_index] : _index == 5u ? _page->curve_tool : _index == 6u ? _page->shape_filled :
           _page->line_style == (u8)(_index - 7u);
}

// The toolbar's Shapes, whether it shows chosen: a shape tool other than the guide in hand, or the Curve.
b8 fude_zoom_shapes_selected(const fude_app* _app) {
    RDE_UNUSED(_app);
    return fude_zoom_page_the != NULL && ((fude_zoom_page_the->shape_tool != 0 && fude_zoom_page_the->shape_tool != FUDE_ZOOM_SHAPE_GUIDE) || fude_zoom_page_the->curve_tool);
}

// The toolbar's Shapes' choices, in the order fude_zoom_shapes_choose numbers them.
const fude_extension_choice FUDE_ZOOM_SHAPES_CHOICES[] = {
    { FUDE_TEXT_ZOOM_SHAPE_LINE,     FUDE_ICON_LINE,      fude_zoom_shapes_choose },
    { FUDE_TEXT_ZOOM_SHAPE_RECT,     FUDE_ICON_RECTANGLE, fude_zoom_shapes_choose },
    { FUDE_TEXT_ZOOM_SHAPE_ELLIPSE,  FUDE_ICON_CIRCLE,    fude_zoom_shapes_choose },
    { FUDE_TEXT_ZOOM_SHAPE_TRIANGLE, FUDE_ICON_TRIANGLE,  fude_zoom_shapes_choose },
    { FUDE_TEXT_ZOOM_SHAPE_ARROW,    FUDE_ICON_CONNECTOR, fude_zoom_shapes_choose },
    { FUDE_TEXT_ZOOM_SHAPE_CURVE,    FUDE_ICON_PEN_NIB,   fude_zoom_shapes_choose },
    { FUDE_TEXT_ZOOM_SHAPE_FILLED,   FUDE_ICON_FILL,      fude_zoom_shapes_choose },
    { FUDE_TEXT_ZOOM_LINE_SOLID,     FUDE_ICON_LINE,      fude_zoom_shapes_choose },   // (shape.h's FUDE_ZOOM_LINE_, in order)
    { FUDE_TEXT_ZOOM_LINE_DASHED,    FUDE_ICON_MORE,      fude_zoom_shapes_choose },
    { FUDE_TEXT_ZOOM_LINE_CENTRE,    FUDE_ICON_GRIP_H,    fude_zoom_shapes_choose },
    { FUDE_TEXT_ZOOM_PEN_025,        FUDE_ICON_SIGNAL_LOW,    fude_zoom_shapes_choose },   // (FUDE_ZOOM_PAGE_PENS, in order)
    { FUDE_TEXT_ZOOM_PEN_035,        FUDE_ICON_SIGNAL_MEDIUM, fude_zoom_shapes_choose },
    { FUDE_TEXT_ZOOM_PEN_05,         FUDE_ICON_SIGNAL_HIGH,   fude_zoom_shapes_choose },
    { FUDE_TEXT_ZOOM_PEN_07,         FUDE_ICON_PEN,           fude_zoom_shapes_choose },
    { FUDE_TEXT_ZOOM_HATCHED,        FUDE_ICON_PAPER_LINES,   fude_zoom_shapes_choose },
    { FUDE_TEXT_ZOOM_CROSS_HATCHED,  FUDE_ICON_PAPER_SQUARES, fude_zoom_shapes_choose },
};
// A compile-time check that the table has its FUDE_ZOOM_SHAPES_CHOICES_N rows (the size is -1, which does
// not compile, when not).
typedef char fude_zoom_shapes_choices_counted[(sizeof(FUDE_ZOOM_SHAPES_CHOICES) / sizeof(FUDE_ZOOM_SHAPES_CHOICES[0]) == FUDE_ZOOM_SHAPES_CHOICES_N) ? 1 : -1];

// --- a path's or a polygon's nodes (shape.h) ----------------------------------------------------

#define FUDE_ZOOM_PAGE_NODE_REACH 16.0   // screen points a node is taken from
#define FUDE_ZOOM_PAGE_NODE_LINE  10.0   // ...and its line, to add one on
#define FUDE_ZOOM_PAGE_NODE_HOLD  0.45   // seconds a node held still turns it a corner

// A path's first number's bits (shape.h: closed, a corner a node).
RDE_INTERNAL u64 fude_zoom_page_path_bits(f64 _n0) {
    return _n0 >= 0.0 && _n0 < 18446744073709551616.0 ? (u64)_n0 : 0u;
}

// A path's bits with a node put in at _at: the corner bits from there on one node along.
RDE_INTERNAL u64 fude_zoom_page_path_bits_insert(u64 _bits, u32 _at) {
    const u64 _low = (2ull << _at) - 1u;   // closed, and the nodes before it
    return (_bits & _low) | ((_bits & ~_low) << 1);
}

// A path's bits with node _at taken out: the corner bits after it one node back.
RDE_INTERNAL u64 fude_zoom_page_path_bits_remove(u64 _bits, u32 _at) {
    const u64 _low = (2ull << _at) - 1u;
    return (_bits & _low) | ((_bits >> 1) & ~_low);
}

// The lassoed path or polygon whose nodes show (FUDE_ZOOM_NONE: none): the lasso holding that one thing.
RDE_INTERNAL u32 fude_zoom_page_node_target(const fude_zoom_page* _page) {
    if(rde_arr_length(&_page->selection.picks) != 1u || rde_arr_length(&_page->selection.loop) > 0) {
        return FUDE_ZOOM_NONE;
    }
    const u32               _one = ((const fude_zoom_pick*)_page->selection.picks.memory)[0].object;
    const fude_zoom_object* _o   = fude_zoom_scene_object(&_page->scene, _one);
    return (_o->flags & FUDE_ZOOM_FLAG_ALIVE) && _o->kind == FUDE_ZOOM_KIND_SHAPE && fude_zoom_scene_touchable(&_page->scene, _o) &&
           (_o->channels == FUDE_ZOOM_SHAPE_PATH || _o->channels == FUDE_ZOOM_SHAPE_POLYGON) ? _one : FUDE_ZOOM_NONE;
}

// Its numbers, how many nodes, where they start among them, and its own units → the screen.
u32 fude_zoom_page_nodes(const fude_zoom_page* _page, u32 _object, f64* _n, u32* _count, u32* _first, fude_zoom_sim* _to_screen) {
    const fude_zoom_object* _o = fude_zoom_scene_object(&_page->scene, _object);
    *_count = fude_zoom_scene_shape_numbers(&_page->scene, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    *_first = _o->channels == FUDE_ZOOM_SHAPE_PATH ? 1u : 0u;
    *_to_screen = fude_zoom_sim_compose(fude_zoom_page_frame_sim(_page, _o->frame), fude_zoom_object_sim(_o));
    if(*_first == 1u) {
        u64 _bits;
        fude_zoom_v2 _p[FUDE_ZOOM_PATH_NODES], _h[FUDE_ZOOM_PATH_NODES];
        return fude_zoom_path_unpack(_n, *_count, &_bits, _p, _h);   // (its handles after its nodes, when it keeps them)
    }
    return *_count > *_first ? (*_count - *_first) / 2u : 0u;
}

// The shape again with these numbers, where it was, in its order and look: one undo step, the lasso on the new one.
RDE_INTERNAL void fude_zoom_page_node_replace(fude_zoom_page* _page, u32 _object, const f64* _n, u32 _count) {
    fude_zoom_scene*       _s   = &_page->scene;
    const fude_zoom_object _old = *fude_zoom_scene_object(_s, _object);
    const u16 _was = _s->layer;
    _s->layer = _old.layer;
    _s->style = _old.channels == FUDE_ZOOM_SHAPE_SYMBOL || _old.kind != FUDE_ZOOM_KIND_SHAPE ? 0 : _old.q;   // (its own line, as its layer)
    const u32 _new = fude_zoom_scene_add_shape_fill(_s, _old.frame, fude_zoom_scene_place_of(_s, _object), _old.channels, _n, _count,
                                                    _old.color, _old.radius, _old.flags, _old.fill, _old.z);
    _s->layer = _was;
    _s->style = 0;
    fude_zoom_scene_set_alive(_s, _object, false);
    rde_arr _died = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std()), _born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr_add(&_died, (any)&_object);
    rde_arr_add(&_born, (any)&_new);
    fude_zoom_page_rebind(_page, &_object, &_new, 1u, &_died, &_born);   // (connectors joined to it follow it)
    fude_zoom_history_push(_s, _old.frame, fude_zoom_box_union(_old.box, fude_zoom_scene_object(_s, _new)->box),
                           (const u32*)_died.memory, (u32)rde_arr_length(&_died), (const u32*)_born.memory, (u32)rde_arr_length(&_born));
    rde_arr_free(&_died);
    rde_arr_free(&_born);
    ((fude_zoom_pick*)_page->selection.picks.memory)[0].object = _new;
    if(_page->node_focus_object == _object) {
        _page->node_focus_object = _new;   // (its node's handle still shown)
    }
    fude_zoom_select_update(&_page->selection, _s);
}

// --- stretching: a lone rectangle, ellipse, sheet or diagram shape by one of its sides ---------------

#define FUDE_ZOOM_PAGE_STRETCH_GRAB 18.0f   // how near a side's handle the pen must land to take it

// The one thing the lasso holds, if it stretches (a rectangle, an ellipse, a diagram shape: an area), and its
// half sizes (its own units). FUDE_ZOOM_NONE: not one.
RDE_INTERNAL u32 fude_zoom_page_stretchable(const fude_zoom_page* _page, f64* _hw, f64* _hh) {
    const fude_zoom_scene* _s = &_page->scene;
    if(rde_arr_length(&_page->selection.picks) != 1u || fude_zoom_select_busy(&_page->selection)) {
        return FUDE_ZOOM_NONE;
    }
    const u32 _o = ((const fude_zoom_pick*)_page->selection.picks.memory)[0].object;
    if(_o >= fude_zoom_scene_object_count(_s)) {
        return FUDE_ZOOM_NONE;
    }
    const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _o);
    // (a sticky note: its width and height — its words wrapped to it again — as its middle's half sizes)
    u8 _style = 0;
    f64 _tw = 0.0, _th = 0.0;
    if((_ob->flags & FUDE_ZOOM_FLAG_ALIVE) && _ob->kind == FUDE_ZOOM_KIND_TEXT && fude_zoom_scene_touchable(_s, _ob) &&
       fude_zoom_scene_text(_s, _o, NULL, &_tw, &_th, &_style, NULL, NULL) && _style == FUDE_ZOOM_TEXT_STICKY) {
        *_hw = _tw * 0.5;
        *_hh = _th * 0.5;
        return *_hw > 0.0 && *_hh > 0.0 ? _o : FUDE_ZOOM_NONE;
    }
    if(!(_ob->flags & FUDE_ZOOM_FLAG_ALIVE) || _ob->kind != FUDE_ZOOM_KIND_SHAPE || !fude_zoom_scene_touchable(_s, _ob) ||
       (_ob->channels != FUDE_ZOOM_SHAPE_RECT && _ob->channels != FUDE_ZOOM_SHAPE_ELLIPSE && _ob->channels != FUDE_ZOOM_SHAPE_SYMBOL &&
        _ob->channels != FUDE_ZOOM_SHAPE_SHEET)) {
        return FUDE_ZOOM_NONE;
    }
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _c   = fude_zoom_scene_shape_numbers(_s, _o, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    const b8  _sym = _ob->channels == FUDE_ZOOM_SHAPE_SYMBOL;
    if(_c < (_sym ? 3u : 2u)) {
        return FUDE_ZOOM_NONE;
    }
    *_hw = rde_math_abs_f64(_n[_sym ? 1 : 0]);
    *_hh = rde_math_abs_f64(_n[_sym ? 2 : 1]);
    return *_hw > 0.0 && *_hh > 0.0 ? _o : FUDE_ZOOM_NONE;
}

// Its own units → the screen (as it is: not while stretched) — a note's from its middle (its own: its top left).
RDE_INTERNAL fude_zoom_sim fude_zoom_page_stretch_sim(const fude_zoom_page* _page, u32 _object) {
    const fude_zoom_object* _ob = fude_zoom_scene_object(&_page->scene, _object);
    fude_zoom_sim _to = fude_zoom_sim_compose(fude_zoom_page_frame_sim(_page, _ob->frame), fude_zoom_object_sim(_ob));
    f64 _tw = 0.0, _th = 0.0;
    if(_ob->kind == FUDE_ZOOM_KIND_TEXT && fude_zoom_scene_text(&_page->scene, _object, NULL, &_tw, &_th, NULL, NULL, NULL)) {
        _to = fude_zoom_sim_compose(_to, (fude_zoom_sim){ 1.0, 0.0, _tw * 0.5, -_th * 0.5 });
    }
    return _to;
}

// A note's least size for its words at width _w (its own units): as tall as they wrap to, its pad round them — and
// four letters wide.
RDE_INTERNAL void fude_zoom_page_note_least(fude_zoom_page* _page, u32 _object, f64 _w, f64* _min_w, f64* _min_h) {
    f64 _size = 0.0;
    const c8* _text = NULL;
    u32 _len = 0;
    *_min_w = 0.0;
    *_min_h = 0.0;
    if(!fude_zoom_scene_text(&_page->scene, _object, &_size, NULL, NULL, NULL, &_text, &_len) || _page->app->font == NULL) {
        return;
    }
    *_min_w = _size * 4.0 + 1.2 * _size;
    c8 _copy[FUDE_ZOOM_SYMBOL_TEXT * 4];
    rde_snprintf(_copy, sizeof(_copy), "%.*s", (int)(_len < sizeof(_copy) - 1u ? _len : sizeof(_copy) - 1u), _text);
    const f64 _inner = rde_math_max_f64(_w - 1.2 * _size, _size);
    const fude_zoom_rich* _rich = fude_zoom_render_rich(&_page->renderer, _text, _len, _size, _inner);   // (read as a note: as tall as it is laid out)
    if(_rich != NULL) {
        *_min_h = (f64)_rich->h + 1.2 * _size;
        return;
    }
    const u32 _lines = _len > 0u ? fude_draw_text_wrap_lines(_page->app->font, _page->app->font_px, _copy, (f32)_size, (f32)_inner) : 1u;
    *_min_h = (f64)_lines * _size * FUDE_ZOOM_PAGE_TEXT_LINE + 1.2 * _size;
}

// Its sides' handles on the screen (right, top, left, bottom). False: none (not one that stretches, or too small
// on the screen to take by its sides: its corners scale it).
RDE_INTERNAL b8 fude_zoom_page_stretch_handles(const fude_zoom_page* _page, u32* _object, rde_vec_2F _at[4]) {
    f64 _hw = 0.0, _hh = 0.0;
    const u32 _o = fude_zoom_page_stretchable(_page, &_hw, &_hh);
    if(_o == FUDE_ZOOM_NONE) {
        return false;
    }
    const fude_zoom_sim _to = fude_zoom_page_stretch_sim(_page, _o);
    const f64 _k = fude_zoom_sim_scale(_to);
    if(_hw * _k < 16.0 || _hh * _k < 16.0) {
        return false;
    }
    const fude_zoom_v2 _side[4] = { { _hw, 0.0 }, { 0.0, _hh }, { -_hw, 0.0 }, { 0.0, -_hh } };
    for(u32 _i = 0; _i < 4u; _i++) {
        const fude_zoom_v2 _p = fude_zoom_sim_apply(_to, _side[_i]);
        _at[_i] = (rde_vec_2F){ (f32)_p.x, (f32)_p.y };
    }
    *_object = _o;
    return true;
}

// A press on a side's handle: that side taken. True: taken.
b8 fude_zoom_page_stretch_down(fude_zoom_page* _page, rde_vec_2F _screen) {
    u32 _o = FUDE_ZOOM_NONE;
    rde_vec_2F _at[4];
    if(!fude_zoom_page_stretch_handles(_page, &_o, _at)) {
        return false;
    }
    for(u32 _i = 0; _i < 4u; _i++) {
        const f32 _dx = _screen.x - _at[_i].x, _dy = _screen.y - _at[_i].y;
        if(_dx * _dx + _dy * _dy <= FUDE_ZOOM_PAGE_STRETCH_GRAB * FUDE_ZOOM_PAGE_STRETCH_GRAB) {
            f64 _hw = 0.0, _hh = 0.0;
            fude_zoom_page_stretchable(_page, &_hw, &_hh);
            _page->stretching      = true;
            _page->stretch_object  = _o;
            _page->stretch_side    = (u8)_i;
            _page->stretch_half[0] = _hw;
            _page->stretch_half[1] = _hh;
            _page->stretch_to      = _i == 0u ? _hw : _i == 1u ? _hh : _i == 2u ? -_hw : -_hh;
            return true;
        }
    }
    return false;
}

// Where the side across is (its own units, along the way it stretches).
RDE_INTERNAL f64 fude_zoom_page_stretch_fixed(const fude_zoom_page* _page) {
    const f64 _half = _page->stretch_half[_page->stretch_side % 2u];
    return _page->stretch_side < 2u ? -_half : _half;
}

// Millimetres one of a thing's own units.
f64 fude_zoom_page_object_mm(const fude_zoom_page* _page, u32 _object) {
    const fude_zoom_object* _o = fude_zoom_scene_object(&_page->scene, _object);
    return _o->scale * fude_zoom_page_home_units(_page, _o->frame) * FUDE_ZOOM_PAGE_MM_PER_UNIT;
}

// The side held moved to where the pen is, along the way it stretches: never closer to the side across than 8 points
// on the screen (a note: four letters, or its words' height), a sheet's side on its rulers' ticks.
void fude_zoom_page_stretch_moved(fude_zoom_page* _page, rde_vec_2F _screen) {
    const fude_zoom_sim _to   = fude_zoom_page_stretch_sim(_page, _page->stretch_object);
    const fude_zoom_v2  _p    = fude_zoom_sim_apply(fude_zoom_sim_inverse(_to), (fude_zoom_v2){ _screen.x, _screen.y });
    const f64           _v    = _page->stretch_side % 2u == 0u ? _p.x : _p.y;
    const f64           _min  = 8.0 / rde_math_max_f64(fude_zoom_sim_scale(_to), 1e-300);   // (8 points across, at least)
    const f64           _fix  = fude_zoom_page_stretch_fixed(_page);
    f64 _least = _min;
    if(fude_zoom_scene_object(&_page->scene, _page->stretch_object)->kind == FUDE_ZOOM_KIND_TEXT) {
        // (a note: no narrower than four letters, no shorter than its words at its width)
        f64 _min_w = 0.0, _min_h = 0.0;
        fude_zoom_page_note_least(_page, _page->stretch_object, 2.0 * _page->stretch_half[0], &_min_w, &_min_h);
        _least = rde_math_max_f64(_least, _page->stretch_side % 2u == 0u ? _min_w : _min_h);
    }
    _page->stretch_to = _page->stretch_side < 2u ? rde_math_max_f64(_v, _fix + _least) : rde_math_min_f64(_v, _fix - _least);
    // A sheet's side goes from tick to tick of its rulers as they show (a whole millimetre,
    // centimetre... at this zoom).
    if(fude_zoom_scene_object(&_page->scene, _page->stretch_object)->channels == FUDE_ZOOM_SHAPE_SHEET) {
        const f64 _mm = fude_zoom_page_object_mm(_page, _page->stretch_object);
        const b8  _inch = _page->units.unit == FUDE_ZOOM_UNIT_IN || _page->units.unit == FUDE_ZOOM_UNIT_FT;
        if(_mm > 0.0) {
            const f64 _len = fude_zoom_sheet_round(rde_math_abs_f64(_page->stretch_to - _fix) * _mm, _mm / rde_math_max_f64(fude_zoom_sim_scale(_to), 1e-300), _inch) / _mm;
            _page->stretch_to = _page->stretch_side < 2u ? _fix + _len : _fix - _len;
        }
    }
}

// Let go: made so — its half size that way and its middle (the side across where it was), the rest as it was;
// one undo step, the lasso on the new one.
void fude_zoom_page_stretch_up(fude_zoom_page* _page) {
    _page->stretching = false;
    fude_zoom_scene* _s = &_page->scene;
    const u32        _o = _page->stretch_object;
    if(_o >= fude_zoom_scene_object_count(_s) || !(fude_zoom_scene_object(_s, _o)->flags & FUDE_ZOOM_FLAG_ALIVE)) {
        return;
    }
    const u32 _axis = _page->stretch_side % 2u;
    const f64 _fix  = fude_zoom_page_stretch_fixed(_page);
    const f64 _half = rde_math_abs_f64(_page->stretch_to - _fix) * 0.5;
    if(rde_math_abs_f64(_half - _page->stretch_half[_axis]) <= 1e-9 * _page->stretch_half[_axis]) {
        return;
    }
    const fude_zoom_object _old = *fude_zoom_scene_object(_s, _o);
    if(_old.kind == FUDE_ZOOM_KIND_TEXT) {
        // A note: as wide or as tall as it was let go (no shorter than its words then wrap to), its top left where
        // the side across leaves it.
        f64 _size = 0.0, _tw = 0.0, _th = 0.0;
        u8 _style = 0;
        const c8* _text = NULL;
        u32 _len = 0;
        if(!fude_zoom_scene_text(_s, _o, &_size, &_tw, &_th, &_style, &_text, &_len)) {
            return;
        }
        f64 _w = _axis == 0u ? 2.0 * _half : _tw, _h = _axis == 1u ? 2.0 * _half : _th;
        f64 _min_w = 0.0, _min_h = 0.0;
        fude_zoom_page_note_least(_page, _o, _w, &_min_w, &_min_h);
        _h = rde_math_max_f64(_h, _min_h);
        const f64 _mid = (_page->stretch_to + _fix) * 0.5;   // (its middle along that way, from the old middle)
        const fude_zoom_v2 _middle = { _tw * 0.5 + (_axis == 0u ? _mid : 0.0), -_th * 0.5 + (_axis == 1u ? _mid : 0.0) };
        fude_zoom_place _place = fude_zoom_scene_place_of(_s, _o);
        _place.t = fude_zoom_sim_apply(fude_zoom_object_sim(&_old), (fude_zoom_v2){ _middle.x - _w * 0.5, _middle.y + _h * 0.5 });
        rde_arr _copy = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
        if(_len > 0u) {
            rde_memcpy(rde_arr_add_n(&_copy, _len), _text, _len);   // (its words: the scene's own move as it adds)
        }
        const u16 _layer = _s->layer;
        _s->layer = _old.layer;
        const u32 _made = fude_zoom_scene_add_text(_s, _old.frame, _place, _size, _w, _h, _style, _old.color, _old.fill, (const c8*)_copy.memory, _len, _old.z);
        _s->layer = _layer;
        rde_arr_free(&_copy);
        fude_zoom_scene_set_alive(_s, _o, false);
        fude_zoom_history_push(_s, _old.frame, fude_zoom_box_union(_old.box, fude_zoom_scene_object(_s, _made)->box), &_o, 1u, &_made, 1u);
        if(rde_arr_length(&_page->selection.picks) == 1u) {
            ((fude_zoom_pick*)_page->selection.picks.memory)[0].object = _made;
        } else {
            fude_zoom_select_clear(&_page->selection);
            rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _made });
        }
        fude_zoom_select_update(&_page->selection, _s);
        _page->map_stale = true;
        return;
    }
    rde_arr _num = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std());
    const u32 _count = fude_zoom_scene_shape_numbers_all(_s, _o, &_num);
    f64* _n = (f64*)_num.memory;
    const u32 _at = (_old.channels == FUDE_ZOOM_SHAPE_SYMBOL ? 1u : 0u) + _axis;
    if(_at >= _count) {
        rde_arr_free(&_num);
        return;
    }
    _n[_at] = _half;
    // Its middle moved along that way (its own units → its frame's).
    const f64          _mid   = (_page->stretch_to + _fix) * 0.5;
    fude_zoom_place    _place = fude_zoom_scene_place_of(_s, _o);
    _place.t = fude_zoom_sim_apply(fude_zoom_object_sim(&_old), _axis == 0u ? (fude_zoom_v2){ _mid, 0.0 } : (fude_zoom_v2){ 0.0, _mid });
    const u16 _layer = _s->layer;
    _s->layer = _old.layer;
    _s->style = _old.channels == FUDE_ZOOM_SHAPE_SYMBOL || _old.kind != FUDE_ZOOM_KIND_SHAPE ? 0 : _old.q;   // (its own line, as its layer)
    const u32 _made = fude_zoom_scene_add_shape_fill(_s, _old.frame, _place, _old.channels, _n, _count, _old.color, _old.radius,
                                                     (u8)(_old.flags & (FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN)), _old.fill, _old.z);
    _s->layer = _layer;
    _s->style = 0;
    rde_arr_free(&_num);
    fude_zoom_scene_set_alive(_s, _o, false);
    rde_arr _dead = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std()), _born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr_add(&_dead, (any)&_o);
    rde_arr_add(&_born, (any)&_made);
    fude_zoom_page_rebind(_page, &_o, &_made, 1u, &_dead, &_born);   // (connectors and dimensions joined to it follow it)
    fude_zoom_history_push(_s, _old.frame, fude_zoom_box_union(_old.box, fude_zoom_scene_object(_s, _made)->box), (const u32*)_dead.memory, (u32)rde_arr_length(&_dead),
                           (const u32*)_born.memory, (u32)rde_arr_length(&_born));
    rde_arr_free(&_dead);
    rde_arr_free(&_born);
    // The lasso on the new one (its selection let go meanwhile — a tool chosen, an undo — taken again).
    if(rde_arr_length(&_page->selection.picks) == 1u) {
        ((fude_zoom_pick*)_page->selection.picks.memory)[0].object = _made;
    } else {
        fude_zoom_select_clear(&_page->selection);
        rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _made });
    }
    fude_zoom_select_update(&_page->selection, _s);
    _page->map_stale = true;
}

// Its sides' handles (a bar across each side's middle), or — held — its outline as it will be.
void fude_zoom_page_render_stretch(fude_zoom_page* _page) {
    const fude_theme* _t = fude_theme_active();
    if(_page->stretching) {
        if(_page->stretch_object >= fude_zoom_scene_object_count(&_page->scene)) {
            return;
        }
        const fude_zoom_sim _to  = fude_zoom_page_stretch_sim(_page, _page->stretch_object);
        const u32           _ax  = _page->stretch_side % 2u;
        const f64           _fix = fude_zoom_page_stretch_fixed(_page), _other = _page->stretch_half[1u - _ax];
        const f64           _lo  = rde_math_min_f64(_fix, _page->stretch_to), _hi = rde_math_max_f64(_fix, _page->stretch_to);
        const fude_zoom_v2  _c[4] = { _ax == 0u ? (fude_zoom_v2){ _lo, -_other } : (fude_zoom_v2){ -_other, _lo },
                                      _ax == 0u ? (fude_zoom_v2){ _hi, -_other } : (fude_zoom_v2){ _other, _lo },
                                      _ax == 0u ? (fude_zoom_v2){ _hi, _other } : (fude_zoom_v2){ _other, _hi },
                                      _ax == 0u ? (fude_zoom_v2){ _lo, _other } : (fude_zoom_v2){ -_other, _hi } };
        for(u32 _i = 0; _i < 4u; _i++) {
            const fude_zoom_v2 _a = fude_zoom_sim_apply(_to, _c[_i]), _b = fude_zoom_sim_apply(_to, _c[(_i + 1u) % 4u]);
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, _t->accent, 2.0f);
        }
        // Its length that way as it goes, by the side held, on a chip.
        rde_font* _font = _page->app->font;
        const f64 _mm = fude_zoom_page_object_mm(_page, _page->stretch_object);
        if(_font != NULL && _mm > 0.0) {
            c8 _said[FUDE_ZOOM_UNITS_TEXT];
            fude_zoom_units_format((_hi - _lo) * _mm, &_page->units, _said, sizeof(_said));
            const fude_zoom_v2 _at = fude_zoom_sim_apply(_to, _ax == 0u ? (fude_zoom_v2){ _page->stretch_to, 0.0 } : (fude_zoom_v2){ 0.0, _page->stretch_to });
            const fude_zoom_v2 _mid = fude_zoom_sim_apply(_to, (fude_zoom_v2){ 0.0, 0.0 });
            const f64 _away = rde_math_hypot_f64(_at.x - _mid.x, _at.y - _mid.y);
            const fude_zoom_v2 _out = _away > 0.0 ? (fude_zoom_v2){ (_at.x - _mid.x) / _away, (_at.y - _mid.y) / _away } : (fude_zoom_v2){ 0.0, 1.0 };
            const f32 _px = 15.0f, _tw = fude_draw_text_width(_font, _page->app->font_px, _said, _px);
            const rde_vec_2F _c2 = { (f32)(_at.x + _out.x * (36.0 + _tw * 0.5 * rde_math_abs_f64(_out.x))), (f32)(_at.y + _out.y * 36.0) };
            rde_rendering_2d_draw_rounded_rectangle_with_border(_c2, (rde_vec_2F){ _tw + 18.0f, 28.0f }, 0.5f, 6u, _t->surface, 1.5f, _t->accent, NULL);
            rde_rendering_2d_draw_text_2(_font, _said, (rde_vec_3F){ _c2.x - _tw * 0.5f, _c2.y - _px * 0.36f, 0.0f }, (rde_vec_2F){ _px / _page->app->font_px, _px / _page->app->font_px }, 0.0f, _t->text);
        }
        return;
    }
    if(_page->presenting) {
        return;
    }
    u32 _o = FUDE_ZOOM_NONE;
    rde_vec_2F _at[4];
    if(!fude_zoom_page_stretch_handles(_page, &_o, _at)) {
        return;
    }
    for(u32 _i = 0; _i < 4u; _i++) {
        const rde_vec_2F _size = _i % 2u == 0u ? (rde_vec_2F){ 9.0f, 22.0f } : (rde_vec_2F){ 22.0f, 9.0f };
        rde_rendering_2d_draw_rounded_rectangle_with_border(_at[_i], _size, 0.5f, 6u, _t->surface, 1.5f, _t->accent, NULL);
    }
}

// --- containers: an area (or a Kanban column, a lane, a group) moved carries what is in it ----------

// Is _object a container — an area, a measured sheet, or a diagram symbol things are put in (a Kanban column, a
// lane, a pool, a swimlane, a group, a boundary)?
b8 fude_zoom_page_container(const fude_zoom_scene* _s, u32 _object) {
    if(_object >= fude_zoom_scene_object_count(_s)) {
        return false;
    }
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if((_o->flags & FUDE_ZOOM_FLAG_ALIVE) && _o->kind == FUDE_ZOOM_KIND_SHAPE && _o->channels == FUDE_ZOOM_SHAPE_SHEET) {
        return true;   // (a measured sheet: what is drawn on it goes with it)
    }
    if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_SYMBOL) {
        return false;
    }
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    if(fude_zoom_scene_shape_numbers(_s, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS) < 4u) {
        return false;
    }
    static const c8* const _kinds[] = { "area", "column", "lane", "pool / lane", "swimlane", "group", "system boundary", "boundary" };
    const u32 _kind = fude_zoom_symbol_kind_of(_n, 4u);   // (FUDE_ZOOM_NONE for a damaged file's: none of them)
    for(u32 _i = 0; _i < sizeof(_kinds) / sizeof(_kinds[0]) && _kind != FUDE_ZOOM_NONE; _i++) {
        if(_kind == fude_zoom_symbol_find(_kinds[_i])) {
            return true;
        }
    }
    return false;
}

// What lies wholly inside container _c, in its frame (things drawn, notes, pictures, connectors, deeper frames — what
// is drawn deeper with them; not on a hidden or locked layer): appended to _out.
void fude_zoom_page_container_contents(const fude_zoom_scene* _s, u32 _c, rde_arr* _out) {
    const fude_zoom_object* _co = fude_zoom_scene_object(_s, _c);
    const fude_zoom_box     _b  = _co->box;
    const fude_zoom_frame*  _f  = fude_zoom_scene_frame(_s, _co->frame);
    for(u32 _k = 0; _k < (u32)rde_arr_length(&_f->order); _k++) {
        const u32               _i = ((const u32*)_f->order.memory)[_k];
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if(_i == _c || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind == FUDE_ZOOM_KIND_MARK || _o->kind == FUDE_ZOOM_KIND_LAYER ||
           !fude_zoom_scene_touchable(_s, _o) || fude_zoom_scene_hides(_s, _o)) {
            continue;
        }
        if(_o->kind == FUDE_ZOOM_KIND_FRAME && (fude_zoom_scene_frame(_s, _o->child)->removed || !fude_zoom_scene_frame_used(_s, _o->child))) {
            continue;
        }
        if(_o->box.min_x >= _b.min_x && _o->box.max_x <= _b.max_x && _o->box.min_y >= _b.min_y && _o->box.max_y <= _b.max_y) {
            rde_arr_add(_out, (any)&_i);
        }
    }
}

// The lasso's press just taken: a move of what it holds — each container in it carrying what lies wholly inside
// it, back to its own picks once the pen lifts.
void fude_zoom_page_carry(fude_zoom_page* _page) {
    fude_zoom_scene* _s = &_page->scene;
    _page->carried_keep = 0;
    if(_page->selection.grab != FUDE_ZOOM_GRAB_MOVE) {
        return;
    }
    const u32 _picks = (u32)rde_arr_length(&_page->selection.picks);
    rde_arr _in = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    for(u32 _p = 0; _p < _picks; _p++) {
        const u32 _c = ((const fude_zoom_pick*)_page->selection.picks.memory)[_p].object;
        if(fude_zoom_page_container(_s, _c)) {
            fude_zoom_page_container_contents(_s, _c, &_in);
        }
    }
    if(rde_arr_length(&_in) > 0) {
        fude_zoom_select_carry(&_page->selection, _s, (const u32*)_in.memory, (u32)rde_arr_length(&_in));
        _page->carried_keep = _picks;
    }
    rde_arr_free(&_in);
}

// The pen down with the lasso holding a path or a polygon: on the end of the shown
// node's handle, the handle held; on a node, it held (twice quickly: deleted, unless
// that would leave fewer than two, or three on a closed one); on its line, maybe a new
// node there (on the lift). False: neither (the lasso's own, then).
b8 fude_zoom_page_node_down(fude_zoom_page* _page, rde_vec_2F _screen) {
    _page->node_insert = false;
    const u32 _target = fude_zoom_page_node_target(_page);
    if(_target == FUDE_ZOOM_NONE || fude_zoom_select_busy(&_page->selection)) {
        return false;
    }
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    u32 _count, _first;
    fude_zoom_sim _to;
    const u32 _nodes = fude_zoom_page_nodes(_page, _target, _n, &_count, &_first, &_to);
    _page->node_handle = 0;
    // The node shown with its handle: an end of it taken (not where the node itself is).
    if(_first == 1u && _page->node_focus_object == _target && _page->node_focus >= 0 && _page->node_focus < (i32)_nodes) {
        u64 _bits;
        fude_zoom_v2 _p[FUDE_ZOOM_PATH_NODES], _h[FUDE_ZOOM_PATH_NODES];
        const u32 _m = fude_zoom_path_unpack(_n, _count, &_bits, _p, _h);
        const u32 _f = (u32)_page->node_focus;
        const fude_zoom_v2 _at = fude_zoom_sim_apply(_to, _p[_f]);
        for(i32 _way = 1; _way >= -1; _way -= 2) {
            const fude_zoom_v2 _t   = fude_zoom_path_tangent(_p, _h, _m, _bits, _f, _way > 0);
            const fude_zoom_v2 _end = fude_zoom_sim_apply(_to, (fude_zoom_v2){ _p[_f].x + _t.x * (f64)_way, _p[_f].y + _t.y * (f64)_way });
            if(rde_math_hypot_f64(_end.x - _screen.x, _end.y - _screen.y) <= FUDE_ZOOM_PAGE_NODE_REACH && rde_math_hypot_f64(_end.x - _at.x, _end.y - _at.y) >= 8.0) {
                _page->node_grab      = (i32)_f;
                _page->node_handle    = (i8)_way;
                _page->node_object    = _target;
                _page->node_at        = _screen;
                _page->node_down_at   = _screen;
                _page->node_down_time = rde_engine_get_time_now();
                return true;
            }
        }
    }
    i32 _near = -1;
    f64 _best = FUDE_ZOOM_PAGE_NODE_REACH;
    for(u32 _i = 0; _i < _nodes; _i++) {
        const fude_zoom_v2 _p = fude_zoom_sim_apply(_to, (fude_zoom_v2){ _n[_first + 2u * _i], _n[_first + 2u * _i + 1u] });
        const f64 _d = rde_math_hypot_f64(_p.x - _screen.x, _p.y - _screen.y);
        if(_d <= _best) {
            _best = _d;
            _near = (i32)_i;
        }
    }
    const f64 _now = rde_engine_get_time_now();
    if(_near >= 0) {
        const b8 _closed = _first == 0u || (fude_zoom_page_path_bits(_n[0]) & FUDE_ZOOM_PATH_CLOSED) != 0u;
        if(_near == _page->node_tap_index && _target == _page->node_tap_object && _now - _page->node_tap_time < 0.35 && _nodes > (_closed ? 3u : 2u)) {
            // Twice quickly: that node goes (a path's handle with it).
            if(_first == 1u) {
                u64 _bits;
                fude_zoom_v2 _p[FUDE_ZOOM_PATH_NODES], _h[FUDE_ZOOM_PATH_NODES];
                const u32 _m = fude_zoom_path_unpack(_n, _count, &_bits, _p, _h);
                rde_memmove(&_p[_near], &_p[_near + 1], (usize)(_m - (u32)_near - 1u) * sizeof(fude_zoom_v2));
                rde_memmove(&_h[_near], &_h[_near + 1], (usize)(_m - (u32)_near - 1u) * sizeof(fude_zoom_v2));
                fude_zoom_page_node_replace(_page, _target, _n, fude_zoom_path_pack(fude_zoom_page_path_bits_remove(_bits, (u32)_near), _p, _h, _m - 1u, _n));
            } else {
                rde_memmove(&_n[_first + 2u * (u32)_near], &_n[_first + 2u * (u32)_near + 2u], (usize)(_count - (_first + 2u * (u32)_near + 2u)) * sizeof(f64));
                fude_zoom_page_node_replace(_page, _target, _n, _count - 2u);
            }
            _page->node_tap_index = -1;
            _page->node_focus     = -1;
            return true;
        }
        if(_first == 1u) {
            _page->node_focus        = _near;   // (its handle shown)
            _page->node_focus_object = _target;
        }
        _page->node_grab      = _near;
        _page->node_object    = _target;
        _page->node_at        = _screen;
        _page->node_down_at   = _screen;
        _page->node_down_time = _now;
        return true;
    }
    // On its line (not its nodes): a new node there if the pen lifts without going anywhere.
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    b8 _closed;
    fude_zoom_shape_outline(fude_zoom_scene_object(&_page->scene, _target)->channels, _n, _count, 64u, &_pts, &_closed);   // (its own units)
    const u32 _k = (u32)rde_arr_length(&_pts);
    for(u32 _i = 0; _i + 1u < _k + (_closed ? 1u : 0u) && !_page->node_insert; _i++) {
        const fude_zoom_v2 _a = fude_zoom_sim_apply(_to, ((const fude_zoom_v2*)_pts.memory)[_i]);
        const fude_zoom_v2 _b = fude_zoom_sim_apply(_to, ((const fude_zoom_v2*)_pts.memory)[(_i + 1u) % _k]);
        _page->node_insert = rde_math_point_segment_distance_2d_f64((fude_zoom_v2){ _screen.x, _screen.y }, _a, _b) <= FUDE_ZOOM_PAGE_NODE_LINE && _nodes < FUDE_ZOOM_PATH_NODES;
    }
    rde_arr_free(&_pts);
    if(_page->node_insert) {
        _page->node_object  = _target;
        _page->node_down_at = _screen;
        _page->node_last_at = _screen;
    }
    return false;   // (the lasso takes it too: dragged, the whole moves instead)
}

// The pen moved holding a node or a handle: where it is now, snapped (and, unsnapped, how far it has gone, for a tap
// on the line).
void fude_zoom_page_node_moved(fude_zoom_page* _page, rde_vec_2F _screen) {
    _page->node_at      = fude_zoom_page_snapped(_page, _screen);
    _page->node_last_at = _screen;
}

// The node held let go where it is now; or a tap on the line made a node there. One undo step either way.
void fude_zoom_page_node_up(fude_zoom_page* _page) {
    const b8 _grabbed = _page->node_grab >= 0;
    const b8 _insert  = _page->node_insert && rde_math_hypot_f64(_page->node_last_at.x - _page->node_down_at.x, _page->node_last_at.y - _page->node_down_at.y) < 4.0;
    _page->node_insert = false;
    _page->snap_hit.kind = FUDE_ZOOM_SNAP_NONE;
    if(!_grabbed && !_insert) {
        return;
    }
    const u32 _target = _page->node_object;
    const i32 _which  = _page->node_grab;
    _page->node_grab = -1;
    if(_target >= fude_zoom_scene_object_count(&_page->scene) || !(fude_zoom_scene_object(&_page->scene, _target)->flags & FUDE_ZOOM_FLAG_ALIVE)) {
        return;
    }
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    u32 _count, _first;
    fude_zoom_sim _to;
    const u32 _nodes = fude_zoom_page_nodes(_page, _target, _n, &_count, &_first, &_to);
    const fude_zoom_sim _back = fude_zoom_sim_inverse(_to);
    const i8 _handle = _page->node_handle;
    _page->node_handle = 0;
    if(_grabbed && _handle != 0 && _first == 1u && _which < (i32)_nodes) {
        // A handle let go where it is: the node's way out (or in) so, smooth through it.
        if(rde_math_hypot_f64(_page->node_at.x - _page->node_down_at.x, _page->node_at.y - _page->node_down_at.y) < 0.5) {
            return;   // (taken and let go: nothing changed)
        }
        u64 _bits;
        fude_zoom_v2 _p[FUDE_ZOOM_PATH_NODES], _h[FUDE_ZOOM_PATH_NODES];
        const u32 _m = fude_zoom_path_unpack(_n, _count, &_bits, _p, _h);
        const fude_zoom_v2 _end = fude_zoom_sim_apply(_back, (fude_zoom_v2){ _page->node_at.x, _page->node_at.y });
        _h[_which] = (fude_zoom_v2){ (_end.x - _p[_which].x) * (f64)_handle, (_end.y - _p[_which].y) * (f64)_handle };
        _bits &= ~FUDE_ZOOM_PATH_CORNER((u32)_which);
        fude_zoom_page_node_replace(_page, _target, _n, fude_zoom_path_pack(_bits, _p, _h, _m, _n));
        return;
    }
    if(_grabbed) {
        if(_which >= (i32)_nodes) {
            return;
        }
        if(rde_math_hypot_f64(_page->node_at.x - _page->node_down_at.x, _page->node_at.y - _page->node_down_at.y) < 0.5) {
            // Held still a while: a path's node turned a corner (or smooth again); else a tap, maybe the first of two.
            const f64 _now = rde_engine_get_time_now();
            if(_now - _page->node_down_time >= FUDE_ZOOM_PAGE_NODE_HOLD) {
                _page->node_tap_index = -1;
                if(_first == 1u) {
                    // (a corner keeps no handle: smooth again, its own curve's)
                    u64 _bits;
                    fude_zoom_v2 _p[FUDE_ZOOM_PATH_NODES], _h[FUDE_ZOOM_PATH_NODES];
                    const u32 _m = fude_zoom_path_unpack(_n, _count, &_bits, _p, _h);
                    _h[_which] = (fude_zoom_v2){ 0.0, 0.0 };
                    fude_zoom_page_node_replace(_page, _target, _n, fude_zoom_path_pack(_bits ^ FUDE_ZOOM_PATH_CORNER((u32)_which), _p, _h, _m, _n));
                }
            } else {
                _page->node_tap_index  = _which;
                _page->node_tap_object = _target;
                _page->node_tap_time   = _now;
            }
            return;
        }
        _page->node_tap_index = -1;   // (dragged: not a tap)
        const fude_zoom_v2 _p = fude_zoom_sim_apply(_back, (fude_zoom_v2){ _page->node_at.x, _page->node_at.y });
        _n[_first + 2u * (u32)_which]      = _p.x;
        _n[_first + 2u * (u32)_which + 1u] = _p.y;
        fude_zoom_page_node_replace(_page, _target, _n, _count);
        return;
    }
    // A new node on the stretch nearest the tap: after the node that stretch starts at.
    const fude_zoom_v2 _q = fude_zoom_sim_apply(_back, (fude_zoom_v2){ _page->node_down_at.x, _page->node_down_at.y });
    const b8 _closed = _first == 0u || (fude_zoom_page_path_bits(_n[0]) & FUDE_ZOOM_PATH_CLOSED) != 0u;
    u32 _after = 0;
    f64 _best  = 1e300;
    const u32 _stretches = _closed ? _nodes : _nodes - 1u;
    for(u32 _i = 0; _i < _stretches; _i++) {
        // Each stretch as drawn (a path's is curved): its nearest point to the tap.
        const fude_zoom_v2 _b = { _n[_first + 2u * _i], _n[_first + 2u * _i + 1u] };
        const u32 _j = (_i + 1u) % _nodes;
        const fude_zoom_v2 _c = { _n[_first + 2u * _j], _n[_first + 2u * _j + 1u] };
        for(u32 _k = 0; _k <= 16u; _k++) {
            fude_zoom_v2 _m;
            if(_first == 1u) {
                const u32 _h = _i > 0 ? _i - 1u : (_closed ? _nodes - 1u : 0u), _l = _i + 2u < _nodes ? _i + 2u : (_closed ? (_i + 2u) % _nodes : _j);
                const fude_zoom_v2 _a = { _n[1u + 2u * _h], _n[2u + 2u * _h] }, _d = { _n[1u + 2u * _l], _n[2u + 2u * _l] };
                _m = fude_zoom_shape_catmull(_a, _b, _c, _d, (f64)_k / 16.0);
            } else {
                _m = (fude_zoom_v2){ _b.x + (_c.x - _b.x) * (f64)_k / 16.0, _b.y + (_c.y - _b.y) * (f64)_k / 16.0 };
            }
            const f64 _d2 = (_m.x - _q.x) * (_m.x - _q.x) + (_m.y - _q.y) * (_m.y - _q.y);
            if(_d2 < _best) {
                _best  = _d2;
                _after = _i;
            }
        }
    }
    if(_first == 1u) {
        // (a path's new node: the curve's own handle)
        u64 _bits;
        fude_zoom_v2 _p[FUDE_ZOOM_PATH_NODES], _h[FUDE_ZOOM_PATH_NODES];
        const u32 _m = fude_zoom_path_unpack(_n, _count, &_bits, _p, _h);
        if(_m >= FUDE_ZOOM_PATH_NODES) {
            return;
        }
        rde_memmove(&_p[_after + 2u], &_p[_after + 1u], (usize)(_m - _after - 1u) * sizeof(fude_zoom_v2));
        rde_memmove(&_h[_after + 2u], &_h[_after + 1u], (usize)(_m - _after - 1u) * sizeof(fude_zoom_v2));
        _p[_after + 1u] = _q;
        _h[_after + 1u] = (fude_zoom_v2){ 0.0, 0.0 };
        fude_zoom_page_node_replace(_page, _target, _n, fude_zoom_path_pack(fude_zoom_page_path_bits_insert(_bits, _after + 1u), _p, _h, _m + 1u, _n));
        return;
    }
    const u32 _at = _first + 2u * (_after + 1u);
    rde_memmove(&_n[_at + 2u], &_n[_at], (usize)(_count - _at) * sizeof(f64));
    _n[_at]      = _q.x;
    _n[_at + 1u] = _q.y;
    fude_zoom_page_node_replace(_page, _target, _n, _count + 2u);
}

// The nodes as handles over the lassoed path or polygon; the one held where the pen is, the line
// through it as it would be.
void fude_zoom_page_render_nodes(fude_zoom_page* _page) {
    const u32 _target = fude_zoom_page_node_target(_page);
    if(_target == FUDE_ZOOM_NONE || fude_zoom_select_busy(&_page->selection)) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    u32 _count, _first;
    fude_zoom_sim _to;
    const u32 _nodes = fude_zoom_page_nodes(_page, _target, _n, &_count, &_first, &_to);
    const b8 _held = _page->node_grab >= 0 && _page->node_object == _target && _page->node_grab < (i32)_nodes;
    if(_held) {
        const fude_zoom_v2 _p = fude_zoom_sim_apply(fude_zoom_sim_inverse(_to), (fude_zoom_v2){ _page->node_at.x, _page->node_at.y });
        if(_page->node_handle != 0 && _first == 1u) {
            // A handle held: the line as it would be with that handle.
            u64 _bits;
            fude_zoom_v2 _np[FUDE_ZOOM_PATH_NODES], _nh[FUDE_ZOOM_PATH_NODES];
            const u32 _m = fude_zoom_path_unpack(_n, _count, &_bits, _np, _nh);
            const u32 _g = (u32)_page->node_grab;
            _nh[_g] = (fude_zoom_v2){ (_p.x - _np[_g].x) * (f64)_page->node_handle, (_p.y - _np[_g].y) * (f64)_page->node_handle };
            _count = fude_zoom_path_pack(_bits & ~FUDE_ZOOM_PATH_CORNER(_g), _np, _nh, _m, _n);
        } else {
            _n[_first + 2u * (u32)_page->node_grab]      = _p.x;
            _n[_first + 2u * (u32)_page->node_grab + 1u] = _p.y;
        }
        rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
        b8 _closed;
        fude_zoom_shape_outline(fude_zoom_scene_object(&_page->scene, _target)->channels, _n, _count, 240u, &_pts, &_closed);
        const u32 _k = (u32)rde_arr_length(&_pts);
        for(u32 _i = 0; _i + 1u < _k + (_closed ? 1u : 0u); _i++) {
            const fude_zoom_v2 _a = fude_zoom_sim_apply(_to, ((const fude_zoom_v2*)_pts.memory)[_i]);
            const fude_zoom_v2 _b = fude_zoom_sim_apply(_to, ((const fude_zoom_v2*)_pts.memory)[(_i + 1u) % _k]);
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, _t->accent, 2.0f);
        }
        rde_arr_free(&_pts);
    }
    const u64 _bits = _first == 1u ? fude_zoom_page_path_bits(_n[0]) : ~0ull;   // (a polygon's are all corners)
    for(u32 _i = 0; _i < _nodes; _i++) {
        const fude_zoom_v2 _p = fude_zoom_sim_apply(_to, (fude_zoom_v2){ _n[_first + 2u * _i], _n[_first + 2u * _i + 1u] });
        const b8 _this = _held && (i32)_i == _page->node_grab;
        const f32 _r = _this ? 8.0f : 6.0f;
        if(_bits & FUDE_ZOOM_PATH_CORNER(_i)) {
            rde_rendering_2d_draw_rounded_rectangle_with_border((rde_vec_2F){ (f32)_p.x, (f32)_p.y }, (rde_vec_2F){ _r * 1.8f, _r * 1.8f }, 0.2f, 4u,
                                                                _this ? _t->accent : _t->page, 2.0f, _t->accent, NULL);
        } else {
            rde_rendering_2d_draw_circle_with_border((rde_vec_2F){ (f32)_p.x, (f32)_p.y }, _r, 18u, _this ? _t->accent : _t->page, 2.0f, _t->accent, NULL);
        }
    }
    // The node last touched: its handle, a line through it to its two ends (dragged out
    // either way; the curve's own when it has none, fainter).
    if(_first == 1u && _page->node_focus_object == _target && _page->node_focus >= 0 && _page->node_focus < (i32)_nodes) {
        u64 _bits;
        fude_zoom_v2 _hp[FUDE_ZOOM_PATH_NODES], _hh[FUDE_ZOOM_PATH_NODES];
        const u32 _m = fude_zoom_path_unpack(_n, _count, &_bits, _hp, _hh);
        const u32 _f = (u32)_page->node_focus;
        if(_f < _m) {
            const b8 _own = _hh[_f].x != 0.0 || _hh[_f].y != 0.0;
            rde_color _c = _t->accent;
            _c.a = _own ? 255u : 140u;
            const fude_zoom_v2 _at = fude_zoom_sim_apply(_to, _hp[_f]);
            for(i32 _way = 1; _way >= -1; _way -= 2) {
                const fude_zoom_v2 _tg  = fude_zoom_path_tangent(_hp, _hh, _m, _bits, _f, _way > 0);
                const fude_zoom_v2 _end = fude_zoom_sim_apply(_to, (fude_zoom_v2){ _hp[_f].x + _tg.x * (f64)_way, _hp[_f].y + _tg.y * (f64)_way });
                if(rde_math_hypot_f64(_end.x - _at.x, _end.y - _at.y) < 8.0) {
                    continue;   // (too short to take: the node itself is there)
                }
                rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_at.x, (f32)_at.y }, (rde_vec_2F){ (f32)_end.x, (f32)_end.y }, _c, 1.5f);
                rde_rendering_2d_draw_circle_with_border((rde_vec_2F){ (f32)_end.x, (f32)_end.y }, 5.0f, 16u, _own ? _t->accent : _t->page, 1.5f, _c, NULL);
            }
        }
    }
}

// --- the Curve tool ----------------------------------------------------------------------------

// The curve through _n points (any units) sampled into _out: _per points a segment.
RDE_INTERNAL void fude_zoom_page_curve_points(const fude_zoom_v2* _p, u32 _n, b8 _closed, u32 _per, rde_arr* _out) {
    rde_arr_clear(_out);
    if(_n < 2u) {
        return;
    }
    const u32 _segs = _closed ? _n : _n - 1u;
    for(u32 _i = 0; _i < _segs; _i++) {
        const fude_zoom_v2 _b = _p[_i], _c = _p[(_i + 1u) % _n];
        const fude_zoom_v2 _a = _closed ? _p[(_i + _n - 1u) % _n] : (_i == 0 ? _b : _p[_i - 1u]);
        const fude_zoom_v2 _d = _closed ? _p[(_i + 2u) % _n] : (_i + 2u < _n ? _p[_i + 2u] : _c);
        for(u32 _k = 0; _k < _per; _k++) {
            const fude_zoom_v2 _q = fude_zoom_shape_catmull(_a, _b, _c, _d, (f64)_k / (f64)_per);
            rde_arr_add(_out, (any)&_q);
        }
    }
    if(!_closed) {
        rde_arr_add(_out, (any)&_p[_n - 1u]);
    }
}

// The curve put down as a PATH (shape.h: its nodes kept, to move, add or
// delete later) in the brush's colour and the Shapes' line (style, hatch, pen),
// closed with _closed (three points at least). One undo step. With the Saw on,
// it cuts along instead. Either way the points are let go.
void fude_zoom_page_curve_commit(fude_zoom_page* _page, b8 _closed) {
    const u32 _nodes = _page->curve_count < FUDE_ZOOM_PATH_NODES ? _page->curve_count : FUDE_ZOOM_PATH_NODES;
    if(_nodes >= 2u && _page->saw) {
        // The Saw: the curve cuts along (or, closed, cuts out its inside), turning at the corners typed as its preview
        // does.
        f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
        u64 _bits = _closed && _nodes >= 3u ? FUDE_ZOOM_PATH_CLOSED : 0u;
        for(u32 _i = 0; _i < _nodes; _i++) {
            _n[1u + 2u * _i] = _page->curve_nodes[_i].x;
            _n[2u + 2u * _i] = _page->curve_nodes[_i].y;
            _bits |= (_page->curve_corners >> _i) & 1u ? FUDE_ZOOM_PATH_CORNER(_i) : 0u;   // (typed: a corner)
        }
        _n[0] = (f64)_bits;
        rde_arr _out = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
        b8 _loop = false;
        fude_zoom_shape_outline(FUDE_ZOOM_SHAPE_PATH, _n, 1u + 2u * _nodes, 24u * _nodes, &_out, &_loop);
        fude_zoom_page_saw_line(_page, _page->curve_frame, (const fude_zoom_v2*)_out.memory, (u32)rde_arr_length(&_out), _loop);
        rde_arr_free(&_out);
        _page->curve_count = 0;
        return;
    }
    if(_nodes >= 2u) {
        f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
        const fude_zoom_v2 _at = _page->curve_nodes[0];
        u64 _bits = _closed && _nodes >= 3u ? FUDE_ZOOM_PATH_CLOSED : 0u;
        for(u32 _i = 0; _i < _nodes; _i++) {
            _n[1u + 2u * _i] = _page->curve_nodes[_i].x - _at.x;
            _n[2u + 2u * _i] = _page->curve_nodes[_i].y - _at.y;
            _bits |= (_page->curve_corners >> _i) & 1u ? FUDE_ZOOM_PATH_CORNER(_i) : 0u;   // (typed: a corner)
        }
        _n[0] = (f64)_bits;
        _page->scene.style = fude_zoom_q_of(_page->line_style, _page->hatch);
        const u32 _o = fude_zoom_scene_add_shape(&_page->scene, _page->curve_frame, (fude_zoom_place){ _at, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_PATH, _n, 1u + 2u * _nodes,
                                                 _page->app->ink->color,
                                                 fude_zoom_page_line_radius(_page, _page->curve_frame, fude_zoom_sim_apply(fude_zoom_page_frame_sim(_page, _page->curve_frame), _at)), 0u, 0);
        _page->scene.style = 0;
        fude_zoom_history_push(&_page->scene, _page->curve_frame, fude_zoom_scene_object(&_page->scene, _o)->box, NULL, 0, &_o, 1);
    }
    _page->curve_count = 0;
}

// A point added to the Curve tool's (screen; the first chooses the frame they go in), _corner: the line turns there.
RDE_INTERNAL void fude_zoom_page_curve_put(fude_zoom_page* _page, fude_zoom_v2 _screen, b8 _corner) {
    fude_zoom_scene* _s = &_page->scene;
    if(_page->curve_count >= FUDE_ZOOM_PAGE_CURVE_NODES) {
        return;
    }
    if(_page->curve_count == 0) {
        const fude_zoom_v2 _at   = fude_zoom_camera_to_frame(&_s->camera, _screen);
        const f64          _near = 4.0 / _s->camera.z;
        _page->curve_frame   = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _at.x - _near, _at.y - _near, _at.x + _near, _at.y + _near });
        _page->curve_corners = 0;
    }
    const fude_zoom_sim _to = fude_zoom_page_frame_sim(_page, _page->curve_frame);
    if(_corner) {
        _page->curve_corners |= 1ull << _page->curve_count;
    }
    _page->curve_nodes[_page->curve_count++] = fude_zoom_sim_apply(fude_zoom_sim_inverse(_to), _screen);
}

// What a typed point counts in: its own units → the screen (X along, Y up), millimetres one of them, and where At
// counts from (its units): the sheet under the last point (none yet: under the screen's middle) and its bottom left
// corner; off a sheet, the canvas's own frame and its middle. True: on a sheet.
RDE_INTERNAL b8 fude_zoom_page_point_space(fude_zoom_page* _page, fude_zoom_sim* _to, f64* _per, fude_zoom_v2* _origin) {
    fude_zoom_v2 _at = { 0.0, 0.0 };
    if(_page->curve_count > 0u) {
        _at = fude_zoom_sim_apply(fude_zoom_page_frame_sim(_page, _page->curve_frame), _page->curve_nodes[_page->curve_count - 1u]);
    }
    fude_zoom_sheet _sh;
    f64 _mm = 0.0;
    if(fude_zoom_page_sheet_under(_page, _at, FUDE_ZOOM_NONE, _to, &_sh, &_mm) != FUDE_ZOOM_NONE && _mm > 0.0) {
        *_per    = _mm;
        *_origin = (fude_zoom_v2){ -_sh.hw, -_sh.hh };
        return true;
    }
    const fude_zoom_scene* _s = &_page->scene;
    *_to     = fude_zoom_page_frame_sim(_page, _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root);
    *_per    = FUDE_ZOOM_PAGE_MM_PER_UNIT;
    *_origin = (fude_zoom_v2){ 0.0, 0.0 };
    return false;
}

// The point card's Add point or Finish: the point where it was typed to be (a corner), then (_finish) the line ended.
RDE_INTERNAL void fude_zoom_page_point_done(any _self, const fude_zoom_point_values* _typed, b8 _finish) {
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    if(_typed != NULL) {
        _page->point_mode = _typed->mode;
        fude_zoom_sim _to;
        f64           _per;
        fude_zoom_v2  _origin;
        fude_zoom_page_point_space(_page, &_to, &_per, &_origin);
        const fude_zoom_sim _back = fude_zoom_sim_inverse(_to);
        const fude_zoom_v2  _last = _page->curve_count > 0u ?
            fude_zoom_sim_apply(_back, fude_zoom_sim_apply(fude_zoom_page_frame_sim(_page, _page->curve_frame), _page->curve_nodes[_page->curve_count - 1u])) : _origin;
        fude_zoom_v2 _p;
        if(_typed->mode == FUDE_ZOOM_POINT_AT) {
            _p = (fude_zoom_v2){ _origin.x + _typed->a / _per, _origin.y + _typed->b / _per };
        } else if(_typed->mode == FUDE_ZOOM_POINT_BY) {
            _p = (fude_zoom_v2){ _last.x + _typed->a / _per, _last.y + _typed->b / _per };
        } else {
            const f64 _r = _typed->b * (RDE_PI / 180.0);
            _p = (fude_zoom_v2){ _last.x + rde_math_cos_f64(_r) * _typed->a / _per, _last.y + rde_math_sin_f64(_r) * _typed->a / _per };
        }
        fude_zoom_page_curve_put(_page, fude_zoom_sim_apply(_to, _p), true);
        fude_zoom_point_added(&_page->point_form);
    }
    if(_finish && _page->curve_count >= 2u) {
        fude_zoom_page_curve_commit(_page, false);
    }
}

// The Curve tool's Type a point chip: the card (in the way the last was typed).
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_type_chip(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->curve_tool) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    fude_zoom_sim _to;
    f64           _per;
    fude_zoom_v2  _origin;
    const b8 _sheet = fude_zoom_page_point_space(_page, &_to, &_per, &_origin);
    fude_zoom_point_open(&_page->point_form, _page->point_mode, _page->curve_count > 0u, _sheet, &_page->units, fude_zoom_page_point_done, _page);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A tap with the Curve tool: a point (snapped to what is near); on the last
// point again, the curve finished; on the first, closed.
void fude_zoom_page_curve_tap(fude_zoom_page* _page, rde_vec_2F _screen) {
    fude_zoom_scene* _s = &_page->scene;
    if(_page->curve_count == 0) {
        const fude_zoom_v2 _at   = fude_zoom_camera_to_frame(&_s->camera, (fude_zoom_v2){ _screen.x, _screen.y });
        const f64          _near = 4.0 / _s->camera.z;
        _page->curve_frame = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _at.x - _near, _at.y - _near, _at.x + _near, _at.y + _near });
    }
    const fude_zoom_sim _to = fude_zoom_page_frame_sim(_page, _page->curve_frame);
    if(_page->curve_count >= 2u) {
        const fude_zoom_v2 _last  = fude_zoom_sim_apply(_to, _page->curve_nodes[_page->curve_count - 1u]);
        const fude_zoom_v2 _first = fude_zoom_sim_apply(_to, _page->curve_nodes[0]);
        if(rde_math_hypot_f64(_last.x - _screen.x, _last.y - _screen.y) <= 14.0) {
            fude_zoom_page_curve_commit(_page, false);
            return;
        }
        if(_page->curve_count >= 3u && rde_math_hypot_f64(_first.x - _screen.x, _first.y - _screen.y) <= 14.0) {
            fude_zoom_page_curve_commit(_page, true);
            return;
        }
    }
    if(_page->curve_count >= FUDE_ZOOM_PAGE_CURVE_NODES) {
        fude_zoom_page_curve_commit(_page, false);
        return;
    }
    _page->snap_from_on = _page->curve_count > 0u;   // (from the last point: square to things, along them)
    _page->snap_from    = _page->curve_count > 0u ? fude_zoom_sim_apply(_to, _page->curve_nodes[_page->curve_count - 1u]) : (fude_zoom_v2){ 0.0, 0.0 };
    _screen = fude_zoom_page_snapped(_page, _screen);
    _page->snap_from_on = false;
    _page->snap_hit.kind = FUDE_ZOOM_SNAP_NONE;
    fude_zoom_page_curve_put(_page, (fude_zoom_v2){ _screen.x, _screen.y }, false);
}

// The curve so far, on to where the pen hovers; its points as small rings.
void fude_zoom_page_render_curve(fude_zoom_page* _page) {
    if(!_page->curve_tool || _page->curve_count == 0) {
        return;
    }
    const fude_theme*   _t  = fude_theme_active();
    const fude_zoom_sim _to = fude_zoom_page_frame_sim(_page, _page->curve_frame);
    fude_zoom_v2 _p[FUDE_ZOOM_PAGE_CURVE_NODES + 1u];
    u32 _n = 0;
    for(u32 _i = 0; _i < _page->curve_count; _i++) {
        _p[_n++] = fude_zoom_sim_apply(_to, _page->curve_nodes[_i]);
    }
    if(_page->hover_on) {
        _p[_n++] = (fude_zoom_v2){ _page->hover_at.x, _page->hover_at.y };
    }
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    if(_page->curve_corners != 0u && _n <= FUDE_ZOOM_PATH_NODES) {
        // Typed points among them: as the path will be (its corners turning).
        f64 _num[FUDE_ZOOM_SHAPE_NUMBERS];
        u64 _bits = 0;
        for(u32 _i = 0; _i < _n; _i++) {
            _num[1u + 2u * _i] = _p[_i].x;
            _num[2u + 2u * _i] = _p[_i].y;
            _bits |= (_page->curve_corners >> _i) & 1u ? FUDE_ZOOM_PATH_CORNER(_i) : 0u;
        }
        _num[0] = (f64)_bits;
        b8 _closed;
        fude_zoom_shape_outline(FUDE_ZOOM_SHAPE_PATH, _num, 1u + 2u * _n, 16u * _n, &_pts, &_closed);
    } else {
        fude_zoom_page_curve_points(_p, _n, false, 16u, &_pts);
    }
    const u32 _m = (u32)rde_arr_length(&_pts);
    if(_m >= 2u && _m <= 4096u) {
        rde_arr _sp_arr = rde_arr_new(sizeof(rde_vec_2F), rde_memory_allocator_get_default_std()), _sr_arr = rde_arr_new(sizeof(f32), rde_memory_allocator_get_default_std());
        rde_arr_resize(&_sp_arr, _m);
        rde_arr_resize(&_sr_arr, _m);
        rde_vec_2F* _sp = (rde_vec_2F*)_sp_arr.memory;
        f32*        _sr = (f32*)_sr_arr.memory;
        const f32   _r  = rde_math_max_f32(fude_zoom_page_brush_radius(_page, _page->curve_frame, fude_zoom_sim_scale(_to)) * (f32)fude_zoom_sim_scale(_to), 1.0f);
        for(u32 _i = 0; _i < _m; _i++) {
            const fude_zoom_v2 _q = ((const fude_zoom_v2*)_pts.memory)[_i];
            _sp[_i] = (rde_vec_2F){ (f32)_q.x, (f32)_q.y };
            _sr[_i] = _r;
        }
        rde_rendering_2d_draw_stroke(_sp, _sr, _m, fude_theme_resolve(_page->app->ink->color));
        rde_arr_free(&_sp_arr);
        rde_arr_free(&_sr_arr);
    }
    rde_arr_free(&_pts);
    for(u32 _i = 0; _i < _page->curve_count; _i++) {
        rde_rendering_2d_draw_circle_with_border((rde_vec_2F){ (f32)_p[_i].x, (f32)_p[_i].y }, 5.0f, 16u, _t->surface, 1.5f, _t->accent, NULL);
    }
}
