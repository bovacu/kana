// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"

// ===========================================================================
// Page, measuring: the measuring tape and snapping (snap.h), guides out of the rulers, the pen's readout, a
// line's exact length (numpad.h), measured sheets (sheet.h), the size card (sizeform.h) and Repeat.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
//
// In order: snapping and the measuring tape (a dimension, a circle's size, an angle), the small tags beside the pen,
// guides pulled out of a sheet's stuck rulers, the pen's readout, a line's typed length, measured sheets, the size
// card and Repeat. The size card and Repeat measure what the lasso holds in millimetres through one transform
// (fude_zoom_page_measuring): from the bottom left corner of the sheet it is on, along its sides, or else on the
// screen's own axes.
// ===========================================================================

// --- measuring and snapping (snap.h) ---------------------------------------------------------

// See page_parts.h: where _screen snaps to (an end, a corner, a middle, a centre drawn near it), within
// FUDE_ZOOM_SNAP_RADIUS; what was found is kept in snap_hit, to be shown.
rde_vec_2F fude_zoom_page_snapped(fude_zoom_page* _page, rde_vec_2F _screen) {
    return fude_zoom_page_snapped_within(_page, _screen, FUDE_ZOOM_SNAP_RADIUS);
}

// See page_parts.h: the same within _radius (screen points; a finger's is wider). A line from snap_from also snaps
// square to what it meets, along straight things and a sheet's sides, and every 15°; along such a way on a sheet with
// a grid, its length is a whole number of the grid's steps. On a measured sheet with its grid, the grid's crossings
// are snapped to when nothing else is near.
rde_vec_2F fude_zoom_page_snapped_within(fude_zoom_page* _page, rde_vec_2F _screen, f64 _radius) {
    rde_arr_clear(&_page->editable);
    fude_zoom_render_editable(&_page->renderer, &_page->scene, &_page->editable);
    const fude_zoom_v2 _p = { _screen.x, _screen.y };
    fude_zoom_sim   _all = fude_zoom_sim_identity();
    fude_zoom_sheet _sh;
    f64             _mm = 0.0;
    const u32 _sheet = fude_zoom_page_sheet_under(_page, _p, FUDE_ZOOM_NONE, &_all, &_sh, &_mm);
    const fude_zoom_visible* _frames = (const fude_zoom_visible*)_page->editable.memory;
    const u32 _nf = (u32)rde_arr_length(&_page->editable);
    if(_page->snap_from_on) {
        fude_zoom_v2 _ways[2] = { { 1.0, 0.0 }, { 0.0, 1.0 } };   // (across and up: the sheet's, else the screen's)
        const f64 _k = fude_zoom_sim_scale(_all);
        if(_sheet != FUDE_ZOOM_NONE && _k > 0.0) {
            _ways[0] = (fude_zoom_v2){ _all.a / _k, _all.b / _k };
            _ways[1] = (fude_zoom_v2){ -_all.b / _k, _all.a / _k };
        }
        _page->snap_hit = fude_zoom_snap_find_from(&_page->scene, _frames, _nf, _p, _radius, _page->snap_from, _ways, 2u, 15.0);   // (and every 15°)
    } else {
        _page->snap_hit = fude_zoom_snap_find(&_page->scene, _frames, _nf, _p, _radius);
    }
    // Along a way on a grid: its length a whole number of the grid's steps (from a crossing, along the sheet's sides:
    // onto one).
    if(_page->snap_hit.kind == FUDE_ZOOM_SNAP_PARALLEL && _sheet != FUDE_ZOOM_NONE && (_sh.flags & FUDE_ZOOM_SHEET_GRID) && _mm > 0.0) {
        const f64 _k = fude_zoom_sim_scale(_all);
        const b8  _inch = _page->units.unit == FUDE_ZOOM_UNIT_IN || _page->units.unit == FUDE_ZOOM_UNIT_FT;
        fude_zoom_sheet_step _g;
        const f64 _dx = _page->snap_hit.at.x - _page->snap_from.x, _dy = _page->snap_hit.at.y - _page->snap_from.y, _len = rde_math_hypot_f64(_dx, _dy);
        if(_k > 0.0 && _len > 0.0 && fude_zoom_sheet_step_for(_mm / _k, FUDE_ZOOM_SHEET_GRID_PT, _inch, &_g)) {
            const f64 _step = _g.step_mm / _mm * _k;   // (points a grid's step)
            const f64 _to   = rde_math_max_f64(rde_math_round_f64(_len / _step), 1.0) * _step;
            _page->snap_hit.at = (fude_zoom_v2){ _page->snap_from.x + _dx / _len * _to, _page->snap_from.y + _dy / _len * _to };
        }
    }
    if(_page->snap_hit.kind == FUDE_ZOOM_SNAP_NONE && _sheet != FUDE_ZOOM_NONE && (_sh.flags & FUDE_ZOOM_SHEET_GRID) && _mm > 0.0) {
        const f64 _k = fude_zoom_sim_scale(_all);
        const b8  _inch = _page->units.unit == FUDE_ZOOM_UNIT_IN || _page->units.unit == FUDE_ZOOM_UNIT_FT;
        fude_zoom_v2 _g;
        if(_k > 0.0 && fude_zoom_sheet_grid_snap(&_sh, _mm, _mm / _k, _inch, fude_zoom_sim_apply(fude_zoom_sim_inverse(_all), _p), &_g)) {
            const fude_zoom_v2 _q = fude_zoom_sim_apply(_all, _g);
            if(rde_math_hypot_f64(_q.x - _p.x, _q.y - _p.y) <= _radius) {
                _page->snap_hit = (fude_zoom_snap){ FUDE_ZOOM_SNAP_GRID, _q, _q, _q, FUDE_ZOOM_NONE, FUDE_ZOOM_SNAP_NO_KEY };
            }
        }
    }
    return (rde_vec_2F){ (f32)_page->snap_hit.at.x, (f32)_page->snap_hit.at.y };
}

// See page_parts.h: a frame's units to the screen, with the camera as it is now.
fude_zoom_sim fude_zoom_page_frame_sim(const fude_zoom_page* _page, u32 _frame) {
    return fude_zoom_sim_compose(fude_zoom_camera_sim(&_page->scene.camera), fude_zoom_scene_sim(&_page->scene, _frame, _page->scene.camera.frame));
}

// See page_parts.h. The tape is kept in the frame the pen pressed in (the drawing frame there), and what the start
// snapped to is kept, so a dimension can be joined to it.
void fude_zoom_page_measure_down(fude_zoom_page* _page, rde_vec_2F _screen) {
    fude_zoom_scene* _s = &_page->scene;
    _page->measure_raw = (fude_zoom_v2){ _screen.x, _screen.y };
    _screen = fude_zoom_page_snapped(_page, _screen);
    _page->measure_from_hit = _page->snap_hit;
    const fude_zoom_v2 _at   = fude_zoom_camera_to_frame(&_s->camera, (fude_zoom_v2){ _screen.x, _screen.y });
    const f64          _near = 4.0 / _s->camera.z;
    _page->measure_frame = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _at.x - _near, _at.y - _near, _at.x + _near, _at.y + _near });
    const fude_zoom_sim _back = fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _page->measure_frame));
    _page->measure_a  = fude_zoom_sim_apply(_back, (fude_zoom_v2){ _screen.x, _screen.y });
    _page->measure_b  = _page->measure_a;
    _page->measuring  = true;
    _page->tape_shown = false;
}

// See page_parts.h. The end snaps as a line from the start does (fude_zoom_page_snapped_within).
void fude_zoom_page_measure_moved(fude_zoom_page* _page, rde_vec_2F _screen) {
    _page->snap_from_on = true;
    _page->snap_from    = fude_zoom_sim_apply(fude_zoom_page_frame_sim(_page, _page->measure_frame), _page->measure_a);
    _screen = fude_zoom_page_snapped(_page, _screen);
    _page->snap_from_on = false;
    const fude_zoom_sim _back = fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _page->measure_frame));
    _page->measure_b = fude_zoom_sim_apply(_back, (fude_zoom_v2){ _screen.x, _screen.y });
}

// A tap with the Dimension in hand, on nothing measured yet: on a circle or an arc, its size — a circle's right across
// it, an arc's radius — out the way the tap is from its centre (it follows the circle: connect.h); at a corner of
// straight things (lines, sides), the angle there the tap is in. One undo step. False: neither there.
RDE_INTERNAL b8 fude_zoom_page_measure_tap(fude_zoom_page* _page) {
    fude_zoom_scene*    _s     = &_page->scene;
    const u32           _frame = _page->measure_frame;
    const fude_zoom_sim _to    = fude_zoom_page_frame_sim(_page, _frame);
    const f64           _k     = fude_zoom_sim_scale(_to);
    if(!(_k > 0.0)) {
        return false;
    }
    const fude_zoom_v2 _tap = fude_zoom_sim_apply(fude_zoom_sim_inverse(_to), _page->measure_raw);   // (frame units)
    const f64 _near = 12.0 / _k;
    rde_arr _found = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_scene_query(_s, _frame, (fude_zoom_box){ _tap.x - _near, _tap.y - _near, _tap.x + _near, _tap.y + _near }, &_found);
    u32 _round = FUDE_ZOOM_NONE;
    u64 _top   = 0;
    fude_zoom_v2 _c = { 0.0, 0.0 };
    f64 _r = 0.0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_found); _i++) {
        const u32 _f = ((const u32*)_found.memory)[_i];
        const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _f);
        fude_zoom_v2 _cc;
        f64 _rr;
        if(!fude_zoom_scene_touchable(_s, _ob) || !fude_zoom_connect_round(_s, _f, &_cc, &_rr)) {
            continue;
        }
        if(_ob->channels == FUDE_ZOOM_SHAPE_ELLIPSE) {
            f64 _n[2];
            if(fude_zoom_scene_shape_numbers(_s, _f, _n, 2u) < 2u || rde_math_abs_f64(rde_math_abs_f64(_n[0]) - rde_math_abs_f64(_n[1])) > 1e-6 * rde_math_abs_f64(_n[0])) {
                continue;   // (an ellipse: no one radius)
            }
        }
        if(rde_math_abs_f64(rde_math_hypot_f64(_tap.x - _cc.x, _tap.y - _cc.y) - _rr) <= _near && (_round == FUDE_ZOOM_NONE || _ob->z > _top)) {
            _round = _f;
            _top   = _ob->z;
            _c     = _cc;
            _r     = _rr;
        }
    }
    rde_arr_free(&_found);
    const rde_color _ink = _page->app->ink->color;
    if(_round != FUDE_ZOOM_NONE && _r > 0.0) {
        const b8 _across = fude_zoom_scene_object(_s, _round)->channels == FUDE_ZOOM_SHAPE_ELLIPSE;
        // (its circle's id in two halves, as scene.h keeps ids in a shape's numbers: shape.h's RADIAL)
        f64 _n[FUDE_ZOOM_RADIAL_NUMBERS] = { _r, rde_math_atan2_f64(_tap.y - _c.y, _tap.x - _c.x), _across ? 1.0 : 0.0, 0.0, 0.0 };
        fude_zoom_id_put(&_n[3], fude_zoom_scene_object(_s, _round)->id);
        const u32 _o = fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ _c, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RADIAL, _n, FUDE_ZOOM_RADIAL_NUMBERS, _ink, (f32)(0.5 / _k), 0u, 0);
        fude_zoom_history_push(_s, _frame, fude_zoom_scene_object(_s, _o)->box, NULL, 0, &_o, 1);
        return true;
    }
    // A corner: the ways straight things go from where the tap snapped (an end, a corner, a crossing).
    const fude_zoom_snap* _h = &_page->measure_from_hit;
    if(_h->kind != FUDE_ZOOM_SNAP_END && _h->kind != FUDE_ZOOM_SNAP_CORNER && _h->kind != FUDE_ZOOM_SNAP_CROSS) {
        return false;
    }
    rde_arr_clear(&_page->editable);
    fude_zoom_render_editable(&_page->renderer, _s, &_page->editable);
    fude_zoom_v2 _ways[16];
    const u32 _n = fude_zoom_snap_rays(_s, (const fude_zoom_visible*)_page->editable.memory, (u32)rde_arr_length(&_page->editable), _h->at, 3.0, _ways, 16u);
    if(_n < 2u) {
        return false;
    }
    // Round them in order; the gap between two neighbours the tap is in (on the corner itself: the smallest).
    f64 _ang[16];
    for(u32 _i = 0; _i < _n; _i++) {
        _ang[_i] = rde_math_atan2_f64(_ways[_i].y, _ways[_i].x);
    }
    for(u32 _i = 1; _i < _n; _i++) {
        for(u32 _j = _i; _j > 0u && _ang[_j] < _ang[_j - 1u]; _j--) {
            const f64 _t = _ang[_j]; _ang[_j] = _ang[_j - 1u]; _ang[_j - 1u] = _t;
        }
    }
    const fude_zoom_v2 _dir = { _page->measure_raw.x - _h->at.x, _page->measure_raw.y - _h->at.y };
    const b8  _aimed = rde_math_hypot_f64(_dir.x, _dir.y) >= 2.0;
    const f64 _tap_a = rde_math_atan2_f64(_dir.y, _dir.x);
    f64 _from = 0.0, _sweep = 0.0;
    b8  _got  = false;
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _a0 = _ang[_i];
        f64 _s0 = (_i + 1u < _n ? _ang[_i + 1u] : _ang[0] + 2.0 * RDE_PI) - _a0;
        if(_s0 <= 1e-9) {
            continue;
        }
        f64 _rel = _tap_a - _a0;
        while(_rel < 0.0)        { _rel += 2.0 * RDE_PI; }
        while(_rel >= 2.0 * RDE_PI) { _rel -= 2.0 * RDE_PI; }
        const b8 _in = _aimed ? _rel <= _s0 : true;
        if(_in && (!_got || (!_aimed && _s0 < _sweep))) {
            _from  = _a0;
            _sweep = _s0;
            _got   = true;
            if(_aimed) {
                break;
            }
        }
    }
    if(!_got || _sweep >= 2.0 * RDE_PI - 1e-9) {
        return false;
    }
    // In the frame's own turn (the screen's ways turned back).
    const f64 _turn = rde_math_atan2_f64(_to.b, _to.a);
    f64 _num[3] = { 36.0 / _k, _from - _turn, _sweep };
    const fude_zoom_v2 _v = fude_zoom_sim_apply(fude_zoom_sim_inverse(_to), _h->at);
    const u32 _o = fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ _v, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ANGLE, _num, 3u, _ink, (f32)(0.5 / _k), 0u, 0);
    fude_zoom_history_push(_s, _frame, fude_zoom_scene_object(_s, _o)->box, NULL, 0, &_o, 1);
    return true;
}

// See page_parts.h: the pen lifted, the tape stays showing (the measuring tape), or a dimension is drawn (one undo
// step), joined to the ends, corners or centres it was drawn between. A tap (under 4 points) with the Dimension in hand
// opens a dimension's number to type, or measures a circle or a corner (fude_zoom_page_measure_tap).
void fude_zoom_page_measure_up(fude_zoom_page* _page) {
    const fude_zoom_snap _to_hit = _page->snap_hit;   // (what it ended on)
    _page->measuring      = false;
    _page->snap_hit.kind  = FUDE_ZOOM_SNAP_NONE;
    const fude_zoom_sim _to = fude_zoom_page_frame_sim(_page, _page->measure_frame);
    const f64           _k  = fude_zoom_sim_scale(_to);
    const fude_zoom_v2  _d  = { _page->measure_b.x - _page->measure_a.x, _page->measure_b.y - _page->measure_a.y };
    if(rde_math_hypot_f64(_d.x, _d.y) * _k < 4.0 || !(_k > 0.0)) {
        // A tap: nothing measured; on a dimension (Dimension in hand), its number to type.
        if(_page->measure_tool == 2u && _k > 0.0) {
            fude_zoom_scene* _s = &_page->scene;
            const f64 _near = 8.0 / _k;
            rde_arr _found = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
            fude_zoom_scene_query(_s, _page->measure_frame, (fude_zoom_box){ _page->measure_a.x - _near, _page->measure_a.y - _near, _page->measure_a.x + _near, _page->measure_a.y + _near }, &_found);
            u32 _hit = FUDE_ZOOM_NONE;
            u64 _top = 0;
            for(u32 _i = 0; _i < (u32)rde_arr_length(&_found); _i++) {
                const u32 _f = ((const u32*)_found.memory)[_i];
                const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _f);
                if((_ob->flags & FUDE_ZOOM_FLAG_ALIVE) && _ob->kind == FUDE_ZOOM_KIND_SHAPE && _ob->channels == FUDE_ZOOM_SHAPE_DIMENSION &&
                   fude_zoom_scene_touchable(_s, _ob) && (_hit == FUDE_ZOOM_NONE || _ob->z > _top)) {
                    _hit = _f;
                    _top = _ob->z;
                }
            }
            rde_arr_free(&_found);
            if(_hit != FUDE_ZOOM_NONE) {
                _page->line_chip_object = _hit;
                fude_zoom_page_on_line_chip(NULL, NULL, NULL);
            } else {
                fude_zoom_page_measure_tap(_page);   // (on a circle: its size; at a corner: its angle)
            }
        }
        return;
    }
    if(_page->measure_tool == 1u) {
        _page->tape_shown = true;
        return;
    }
    // Joined to the points it was drawn between (an end, a corner, a centre of something): it follows them.
    const fude_zoom_snap* _fh = &_page->measure_from_hit;
    const b8 _fj = _fh->kind == FUDE_ZOOM_SNAP_END || _fh->kind == FUDE_ZOOM_SNAP_CORNER || _fh->kind == FUDE_ZOOM_SNAP_CENTRE;
    const b8 _tj = _to_hit.kind == FUDE_ZOOM_SNAP_END || _to_hit.kind == FUDE_ZOOM_SNAP_CORNER || _to_hit.kind == FUDE_ZOOM_SNAP_CENTRE;
    const u32 _o = fude_zoom_connect_dimension(&_page->scene, _page->measure_frame, _page->measure_a, _page->measure_b, FUDE_ZOOM_PAGE_DIMENSION_OFFSET / _k,
                                               _fj ? _fh->object : FUDE_ZOOM_NONE, _fh->key, _tj ? _to_hit.object : FUDE_ZOOM_NONE, _to_hit.key,
                                               _page->app->ink->color, (f32)(0.5 / _k));
    fude_zoom_history_push(&_page->scene, _page->measure_frame, fude_zoom_scene_object(&_page->scene, _o)->box, NULL, 0, &_o, 1);
    _page->line_chip_object = _o;   // its number, to type (and drive what it measures)
    _page->line_chip_until  = rde_engine_get_time_now() + FUDE_ZOOM_PAGE_LINE_CHIP;
}

// See page_parts.h: a small tag of one or two lines beside the pen (_at, screen), below it and to its right, kept on
// the screen; its letters FUDE_ZOOM_PAGE_TAG_PX high.
void fude_zoom_page_draw_tag(fude_zoom_page* _page, rde_vec_2F _at, const c8* _first, const c8* _second, fude_zoom_v2 _half) {
    fude_zoom_page_draw_tag_px(_page, _at, _first, _second, _half, FUDE_ZOOM_PAGE_TAG_PX);
}

// See page_parts.h: the tag with its letters _px high; all of it, its distance from _at too, is as much smaller or
// larger as they are. With no room on the right it goes on the left, with none below, above.
void fude_zoom_page_draw_tag_px(fude_zoom_page* _page, rde_vec_2F _at, const c8* _first, const c8* _second, fude_zoom_v2 _half, f32 _px) {
    rde_font* _font = _page->renderer.font;
    if(_font == NULL || _first == NULL) {
        return;
    }
    const f32 _u    = _px / FUDE_ZOOM_PAGE_TAG_PX;
    const f32 _line = _px + 5.0f * _u;
    const u32 _n  = _second != NULL ? 2u : 1u;
    const f32 _w  = rde_math_max_f32(fude_draw_text_width(_font, _page->renderer.font_px, _first, _px),
                          _second != NULL ? fude_draw_text_width(_font, _page->renderer.font_px, _second, _px) : 0.0f);
    const f32 _bw = _w + 14.0f * _u, _bh = (f32)_n * _line + 6.0f * _u;
    rde_vec_2F _c = { _at.x + 22.0f * _u + _bw * 0.5f, _at.y - 26.0f * _u - _bh * 0.5f };
    if(_c.x + _bw * 0.5f > (f32)_half.x - 4.0f) {
        _c.x = _at.x - 22.0f * _u - _bw * 0.5f;   // (no room on its right: on its left)
    }
    if(_c.y - _bh * 0.5f < -(f32)_half.y + 4.0f) {
        _c.y = _at.y + 26.0f * _u + _bh * 0.5f;   // (none below: above)
    }
    const fude_theme* _t = fude_theme_active();
    rde_color _back = _t->surface;
    _back.a = 235;
    rde_rendering_2d_draw_rectangle(_c, (rde_vec_2F){ _bw, _bh }, _back);
    rde_rendering_2d_draw_rectangle_border(_c, (rde_vec_2F){ _bw, _bh }, 1.0f, _t->outline, NULL);
    const c8* const _texts[2] = { _first, _second };
    for(u32 _i = 0; _i < _n; _i++) {
        const f32 _y = _c.y + _bh * 0.5f - 3.0f * _u - _line * ((f32)_i + 0.5f);
        rde_rendering_2d_draw_text_2(_font, _texts[_i], (rde_vec_3F){ _c.x - _w * 0.5f, _y - _px * 0.36f, 0.0f }, (rde_vec_2F){ _px / _page->renderer.font_px, _px / _page->renderer.font_px },
                                     0.0f, _i == 0u ? _t->text : _t->text_soft);
    }
}

// --- guides out of the rulers ----------------------------------------------------------------------
//
// A sheet's ruler stuck to the screen's edge (zoomed into it): the pen pressed on it pulls a guide out (along the
// sheet's X from its top ruler, up its Y from its left one), on the ruler's nearest small mark (or on what it snaps
// to) where it is let go, its distance from the sheet's corner said beside it as it goes. Let go on a ruler, none.
// A guide the lasso holds let go on a ruler: deleted.

// See page_parts.h: the stuck ruler under _screen (its index in the renderer's stuck bands, at most 4), or -1.
i32 fude_zoom_page_stuck_at(const fude_zoom_page* _page, rde_vec_2F _screen) {
    const fude_zoom_renderer* _r = &_page->renderer;
    for(u32 _i = 0; _i < _r->stuck_count && _i < 4u; _i++) {
        const fude_zoom_box _b = _r->stuck[_i];
        if(_screen.x >= _b.min_x && _screen.x <= _b.max_x && _screen.y >= _b.min_y && _screen.y <= _b.max_y) {
            return (i32)_i;
        }
    }
    return -1;
}

// Writes the sheet a guide is pulled out of: its own units to the screen, its numbers, millimetres one of its units.
// False when it is gone or not a sheet any more, or has no size on the screen.
RDE_INTERNAL b8 fude_zoom_page_guide_sheet(const fude_zoom_page* _page, fude_zoom_sim* _all, fude_zoom_sheet* _sheet, f64* _mm) {
    const fude_zoom_scene* _s = &_page->scene;
    if(_page->guide_sheet >= fude_zoom_scene_object_count(_s) || !fude_zoom_page_sheet_of(_page, _page->guide_sheet, NULL, NULL, _sheet)) {
        return false;
    }
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _page->guide_sheet);
    *_all = fude_zoom_sim_compose(fude_zoom_page_frame_sim(_page, _o->frame), fude_zoom_object_sim(_o));
    *_mm  = fude_zoom_page_object_mm(_page, _page->guide_sheet);
    return *_mm > 0.0 && fude_zoom_sim_scale(*_all) > 0.0;
}

// Where a pulled guide is (screen): on what the pen snaps to, else its distance from the sheet's corner on the ruler's
// nearest small mark. Its distance (millimetres) in *_mm_out.
RDE_INTERNAL rde_vec_2F fude_zoom_page_guide_pull_at(fude_zoom_page* _page, rde_vec_2F _screen, f64* _mm_out) {
    fude_zoom_sim   _all;
    fude_zoom_sheet _sh;
    f64             _mm;
    *_mm_out = 0.0;
    if(!fude_zoom_page_guide_sheet(_page, &_all, &_sh, &_mm)) {
        return _screen;
    }
    const rde_vec_2F _snapped = fude_zoom_page_snapped(_page, _screen);
    const fude_zoom_sim _back = fude_zoom_sim_inverse(_all);
    fude_zoom_v2 _l = fude_zoom_sim_apply(_back, (fude_zoom_v2){ _snapped.x, _snapped.y });
    const u32 _axis = _page->guide_top ? 1u : 0u;   // (a guide along X: where it is up the sheet)
    f64 _from = ((_axis == 1u ? _l.y + _sh.hh : _l.x + _sh.hw)) * _mm;
    if(_page->snap_hit.kind == FUDE_ZOOM_SNAP_NONE) {
        const b8 _inch = _page->units.unit == FUDE_ZOOM_UNIT_IN || _page->units.unit == FUDE_ZOOM_UNIT_FT;
        _from = fude_zoom_sheet_round(_from, _mm / fude_zoom_sim_scale(_all), _inch);
        if(_axis == 1u) {
            _l.y = _from / _mm - _sh.hh;
        } else {
            _l.x = _from / _mm - _sh.hw;
        }
    }
    *_mm_out = _from;
    const fude_zoom_v2 _p = fude_zoom_sim_apply(_all, _l);
    return (rde_vec_2F){ (f32)_p.x, (f32)_p.y };
}

// See page_parts.h. Which sheet and which ruler (top or left) are kept for the pull.
b8 fude_zoom_page_guide_pull_down(fude_zoom_page* _page, rde_vec_2F _screen) {
    const i32 _band = fude_zoom_page_stuck_at(_page, _screen);
    if(_band < 0) {
        return false;
    }
    _page->guide_pulling = true;
    _page->guide_sheet   = _page->renderer.stuck_sheet[_band];
    _page->guide_top     = _page->renderer.stuck_top[_band];
    _page->pen_now       = _screen;
    return true;
}

// See page_parts.h. The guide is a GUIDE shape in the sheet's frame, turned as the sheet is (upright for its left
// ruler's), three times the sheet's diagonal long; one undo step.
void fude_zoom_page_guide_pull_up(fude_zoom_page* _page) {
    _page->guide_pulling = false;
    _page->snap_hit.kind = FUDE_ZOOM_SNAP_NONE;
    fude_zoom_sim   _all;
    fude_zoom_sheet _sh;
    f64             _mm, _from;
    if(fude_zoom_page_stuck_at(_page, _page->pen_now) >= 0 || !fude_zoom_page_guide_sheet(_page, &_all, &_sh, &_mm)) {
        return;   // (let go on a ruler: none)
    }
    const rde_vec_2F _at = fude_zoom_page_guide_pull_at(_page, _page->pen_now, &_from);
    fude_zoom_scene*        _s = &_page->scene;
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _page->guide_sheet);
    const u32 _frame = _o->frame;
    const fude_zoom_sim _to_screen = fude_zoom_page_frame_sim(_page, _frame);
    const fude_zoom_v2  _p = fude_zoom_sim_apply(fude_zoom_sim_inverse(_to_screen), (fude_zoom_v2){ _at.x, _at.y });
    const f64 _turn = _o->rotation + (_page->guide_top ? 0.0 : RDE_PI * 0.5);
    const f64 _n[3] = { rde_math_cos_f64(_turn), rde_math_sin_f64(_turn), 3.0 * rde_math_hypot_f64(_sh.hw, _sh.hh) * _o->scale };
    const u32 _g = fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ _p, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_GUIDE, _n, 3u, _page->app->ink->color,
                                             (f32)(0.5 / fude_zoom_sim_scale(_to_screen)), 0u, 0);
    fude_zoom_history_push(_s, _frame, fude_zoom_scene_object(_s, _g)->box, NULL, 0, &_g, 1);
}

// See page_parts.h: the guide being pulled, across the screen, its distance from the sheet's corner beside the pen.
// Nothing while the pen is still on a ruler.
void fude_zoom_page_render_guide_pull(fude_zoom_page* _page, fude_zoom_v2 _half) {
    if(!_page->guide_pulling || fude_zoom_page_stuck_at(_page, _page->pen_now) >= 0) {
        return;
    }
    fude_zoom_sim   _all;
    fude_zoom_sheet _sh;
    f64             _mm, _from;
    if(!fude_zoom_page_guide_sheet(_page, &_all, &_sh, &_mm)) {
        return;
    }
    const rde_vec_2F _at = fude_zoom_page_guide_pull_at(_page, _page->pen_now, &_from);
    const f64 _k = fude_zoom_sim_scale(_all);
    const fude_zoom_v2 _way = _page->guide_top ? (fude_zoom_v2){ _all.a / _k, _all.b / _k } : (fude_zoom_v2){ -_all.b / _k, _all.a / _k };
    const f64 _far = 4.0 * rde_math_hypot_f64(_half.x, _half.y);
    fude_zoom_v2 _p = { _at.x - _way.x * _far, _at.y - _way.y * _far }, _q = { _at.x + _way.x * _far, _at.y + _way.y * _far };
    if(fude_zoom_clip_line(&_p, &_q, (fude_zoom_box){ -_half.x, -_half.y, _half.x, _half.y })) {
        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_p.x, (f32)_p.y }, (rde_vec_2F){ (f32)_q.x, (f32)_q.y }, fude_zoom_guide_color(), 1.5f);
    }
    c8 _len[48], _text[64];
    fude_zoom_units_format(_from, &_page->units, _len, sizeof(_len));
    rde_snprintf(_text, sizeof(_text), "%s %s", _page->guide_top ? "Y" : "X", _len);
    fude_zoom_page_draw_tag(_page, _at, _text, NULL, _half);
}

// --- the pen's readout: where it is on a sheet, and how long, at what angle (or how big) what it draws is -------

// Writes the point's place on the sheet under it (from its bottom left corner, X along it, Y up it: millimetres), the
// sheet's own units to the screen, and millimetres one of them. False: on no sheet.
RDE_INTERNAL b8 fude_zoom_page_on_sheet(const fude_zoom_page* _page, rde_vec_2F _screen, fude_zoom_v2* _mm_xy, fude_zoom_sim* _all, f64* _per) {
    fude_zoom_sheet _sh;
    f64 _mm = 0.0;
    if(fude_zoom_page_sheet_under(_page, (fude_zoom_v2){ _screen.x, _screen.y }, FUDE_ZOOM_NONE, _all, &_sh, &_mm) == FUDE_ZOOM_NONE || !(_mm > 0.0)) {
        return false;
    }
    const fude_zoom_v2 _l = fude_zoom_sim_apply(fude_zoom_sim_inverse(*_all), (fude_zoom_v2){ _screen.x, _screen.y });
    *_mm_xy = (fude_zoom_v2){ (_l.x + _sh.hw) * _mm, (_l.y + _sh.hh) * _mm };
    *_per   = _mm;
    return true;
}

// See page_parts.h. While a line, a guide, a wall, a rectangle, an ellipse or a polygon is drawn, or the Curve tool's
// next point is hovered: on a sheet, where the pen is from its corner; then how long and at what angle (or, for a
// box, how wide and tall) what it draws is, along the sheet's sides when on one, else the screen's. Not while
// presenting.
void fude_zoom_page_render_readout(fude_zoom_page* _page, fude_zoom_v2 _half) {
    if(_page->presenting) {
        return;
    }
    rde_vec_2F _pen = { 0.0f, 0.0f }, _start = { 0.0f, 0.0f };
    b8 _from = false, _box = false;
    const u8 _t = _page->shape_tool;
    if(_page->shaping && (_t == FUDE_ZOOM_SHAPE_LINE || _t == FUDE_ZOOM_SHAPE_GUIDE || _t == FUDE_ZOOM_SHAPE_RECT || _t == FUDE_ZOOM_SHAPE_ELLIPSE ||
                          _t == FUDE_ZOOM_SHAPE_POLYGON || _t == FUDE_ZOOM_SHAPE_WALL)) {
        _pen   = _page->pen_now;
        _start = _page->shape_from;
        _from  = true;
        _box   = _t != FUDE_ZOOM_SHAPE_LINE && _t != FUDE_ZOOM_SHAPE_GUIDE && _t != FUDE_ZOOM_SHAPE_WALL;
    } else if(_page->curve_tool && _page->hover_on && !fude_zoom_point_shown(&_page->point_form)) {
        _pen = _page->hover_at;
        if(_page->curve_count > 0u) {
            const fude_zoom_v2 _l = fude_zoom_sim_apply(fude_zoom_page_frame_sim(_page, _page->curve_frame), _page->curve_nodes[_page->curve_count - 1u]);
            _start = (rde_vec_2F){ (f32)_l.x, (f32)_l.y };
            _from  = true;
        }
    } else {
        return;
    }
    // Millimetres a screen point, and the way X goes: the sheet's under the pen; else the canvas's and the screen's.
    fude_zoom_v2  _xy;
    fude_zoom_sim _all;
    f64           _per;
    const b8 _sheet = fude_zoom_page_on_sheet(_page, _pen, &_xy, &_all, &_per);
    const fude_zoom_scene* _s = &_page->scene;
    const f64 _mpp = _sheet ? _per / fude_zoom_sim_scale(_all) :
                     FUDE_ZOOM_PAGE_MM_PER_UNIT / fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root));
    const f64 _xa = _sheet ? rde_math_atan2_f64(_all.b, _all.a) : 0.0;
    c8 _lines[2][96];
    _lines[0][0] = 0;
    _lines[1][0] = 0;
    if(_sheet) {
        c8 _x[40], _y[40];
        fude_zoom_units_format(_xy.x, &_page->units, _x, sizeof(_x));
        fude_zoom_units_format(_xy.y, &_page->units, _y, sizeof(_y));
        rde_snprintf(_lines[0], sizeof(_lines[0]), "X %s   Y %s", _x, _y);
    }
    if(_from) {
        const f64 _dx = (f64)_pen.x - _start.x, _dy = (f64)_pen.y - _start.y;
        if(_box) {
            // (across and up the sheet's way)
            const f64 _u = rde_math_abs_f64(_dx * rde_math_cos_f64(_xa) + _dy * rde_math_sin_f64(_xa)), _v = rde_math_abs_f64(-_dx * rde_math_sin_f64(_xa) + _dy * rde_math_cos_f64(_xa));
            c8 _w[40], _h[40];
            fude_zoom_units_format(_u * _mpp, &_page->units, _w, sizeof(_w));
            fude_zoom_units_format(_v * _mpp, &_page->units, _h, sizeof(_h));
            rde_snprintf(_lines[1], sizeof(_lines[1]), "%s \xC3\x97 %s", _w, _h);
        } else if(rde_math_hypot_f64(_dx, _dy) > 0.5) {
            c8 _l[40], _a[24];
            f64 _deg = (rde_math_atan2_f64(_dy, _dx) - _xa) * (180.0 / RDE_PI);
            _deg = rde_math_fmod_f64(_deg + 360.0, 360.0);
            fude_zoom_units_format(rde_math_hypot_f64(_dx, _dy) * _mpp, &_page->units, _l, sizeof(_l));
            fude_zoom_units_format_angle(_deg, 1u, _a, sizeof(_a));
            rde_snprintf(_lines[1], sizeof(_lines[1]), "%s   %s", _l, _a);
        }
    }
    if(_lines[0][0] == 0 && _lines[1][0] == 0) {
        return;
    }
    fude_zoom_page_draw_tag(_page, _pen, _lines[0][0] != 0 ? _lines[0] : _lines[1], _lines[0][0] != 0 && _lines[1][0] != 0 ? _lines[1] : NULL, _half);
}

// --- a line's exact length (numpad.h) -------------------------------------------------------

// See page_parts.h: a LINE's or a DIMENSION's length in millimetres (its numbers in its frame's units, its scale, the
// canvas's), its own x and y into *_dx and *_dy. 0, leaving them alone, for anything else.
f64 fude_zoom_page_line_mm(const fude_zoom_page* _page, u32 _object, f64* _dx, f64* _dy) {
    const fude_zoom_scene*  _s = &_page->scene;
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    if(_o->kind != FUDE_ZOOM_KIND_SHAPE || (_o->channels != FUDE_ZOOM_SHAPE_LINE && _o->channels != FUDE_ZOOM_SHAPE_DIMENSION) ||
       fude_zoom_scene_shape_numbers(_s, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS) < 2u) {
        return 0.0;
    }
    *_dx = _n[0];
    *_dy = _n[1];
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    return rde_math_hypot_f64(_n[0], _n[1]) * _o->scale * fude_zoom_sim_scale(fude_zoom_scene_sim(_s, _o->frame, _home)) * FUDE_ZOOM_PAGE_MM_PER_UNIT;
}

// See page_parts.h. The numpad's OK on a line's or a dimension's chip: it is made again exactly that long, from where
// it starts, the way it goes, and what has an end or a corner where it ended moves with that end (connect.h's driving:
// a dimension drives what it measures). Its numbers are all copied, its length changed: a dimension stays joined to
// what its ends are on (followed again to the things made again, fude_zoom_page_rebind). One undo step, the lasso
// moved onto what was made again.
void fude_zoom_page_line_typed(any _self, const c8* _text, FUDE_ZOOM_UNIT_ _unit, f64 _mm) {
    RDE_UNUSED(_text);
    fude_zoom_page*  _page = (fude_zoom_page*)_self;
    fude_zoom_scene* _s    = &_page->scene;
    _page->units.unit = _unit;   // the unit typed in is the one shown from now on
    const u32 _old = _page->line_chip_object;
    if(!_page->open || _old >= fude_zoom_scene_object_count(_s) || !(fude_zoom_scene_object(_s, _old)->flags & FUDE_ZOOM_FLAG_ALIVE)) {
        return;
    }
    f64 _dx = 0.0, _dy = 0.0;
    const f64 _now = fude_zoom_page_line_mm(_page, _old, &_dx, &_dy);
    if(!(_now > 0.0) || !(_mm > 0.0)) {
        return;
    }
    const fude_zoom_object _o = *fude_zoom_scene_object(_s, _old);
    const fude_zoom_sim    _os = fude_zoom_object_sim(&_o);
    // All its numbers, so a dimension keeps what its ends are on (FUDE_ZOOM_DIMENSION_REFS) and still follows them.
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    rde_memset(_n, 0, sizeof(_n));
    const u32 _nc = fude_zoom_scene_shape_numbers(_s, _old, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    const f64 _k = _mm / _now;
    const fude_zoom_v2 _end_was = fude_zoom_sim_apply(_os, (fude_zoom_v2){ _dx, _dy });
    _n[0] = _dx * _k;
    _n[1] = _dy * _k;
    const fude_zoom_v2 _end_now = fude_zoom_sim_apply(_os, (fude_zoom_v2){ _n[0], _n[1] });
    const u16 _layer_was = _s->layer;
    _s->layer = _o.layer;
    _s->style = _o.channels == FUDE_ZOOM_SHAPE_SYMBOL || _o.kind != FUDE_ZOOM_KIND_SHAPE ? 0 : _o.q;   // (its own line, as its layer)
    const u32 _new = fude_zoom_scene_add_shape(_s, _o.frame, fude_zoom_scene_place_of(_s, _old), _o.channels, _n, _nc,
                                               _o.color, _o.radius, (u8)(_o.flags & ~FUDE_ZOOM_FLAG_ALIVE), _o.z);
    _s->layer = _layer_was;
    _s->style = 0;
    fude_zoom_scene_set_alive(_s, _old, false);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _died = rde_arr_new(sizeof(u32), _heap), _born = rde_arr_new(sizeof(u32), _heap);
    rde_arr_add(&_died, (any)&_old);
    rde_arr_add(&_born, (any)&_new);
    // What met it at its end: half a point on the screen is "at".
    const f64 _tol = 0.5 / rde_math_max_f64(fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, _o.frame)), 1e-300);
    fude_zoom_connect_drive(_s, _o.frame, _end_was, _end_now, _tol, _new, &_died, &_born);
    {
        // Connectors joined to any of them follow them (their replacements' ids).
        const u32 _pairs = (u32)rde_arr_length(&_died);
        rde_arr _d_arr = rde_arr_new(sizeof(u32), _heap);
        rde_arr_resize(&_d_arr, (usize)_pairs * 2u);
        u32* _d = (u32*)_d_arr.memory;
        rde_memcpy(_d, _died.memory, (usize)_pairs * sizeof(u32));
        rde_memcpy(_d + _pairs, _born.memory, (usize)_pairs * sizeof(u32));
        fude_zoom_page_rebind(_page, _d, _d + _pairs, _pairs, &_died, &_born);
        rde_arr_free(&_d_arr);
    }
    fude_zoom_box _box = _o.box;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_born); _i++) {
        _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, ((const u32*)_born.memory)[_i])->box);
    }
    fude_zoom_history_push(_s, _o.frame, _box, (const u32*)_died.memory, (u32)rde_arr_length(&_died), (const u32*)_born.memory, (u32)rde_arr_length(&_born));
    // The lasso, holding what changed, holds the new ones; the chip stays on the new one. Each is followed to its last
    // replacement: a dimension made again here is made again once more when what its end is on was (rebind's later
    // pairs, after it in the arrays).
    u32 _chip = _new;
    for(u32 _k2 = 0; _k2 < (u32)rde_arr_length(&_died); _k2++) {
        _chip = _chip == ((const u32*)_died.memory)[_k2] ? ((const u32*)_born.memory)[_k2] : _chip;
    }
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        fude_zoom_pick* _pick = &((fude_zoom_pick*)_page->selection.picks.memory)[_i];
        for(u32 _k2 = 0; _k2 < (u32)rde_arr_length(&_died); _k2++) {
            if(_pick->object == ((const u32*)_died.memory)[_k2]) {
                _pick->object = ((const u32*)_born.memory)[_k2];
            }
        }
    }
    fude_zoom_select_update(&_page->selection, _s);
    rde_arr_free(&_died); rde_arr_free(&_born);
    _page->line_chip_object = _chip;
    _page->line_chip_until  = rde_engine_get_time_now() + FUDE_ZOOM_PAGE_LINE_CHIP;
}

// --- measured sheets (sheet.h; Insert's Measured sheet) -------------------------------------------

// See page_parts.h: a sheet's true size (mm) and its numbers as read; each out may be NULL. False: not a live sheet.
b8 fude_zoom_page_sheet_of(const fude_zoom_page* _page, u32 _object, f64* _w_mm, f64* _h_mm, fude_zoom_sheet* _sheet) {
    const fude_zoom_scene* _s = &_page->scene;
    if(_object >= fude_zoom_scene_object_count(_s)) {
        return false;
    }
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_SHEET) {
        return false;
    }
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _count = fude_zoom_scene_shape_numbers(_s, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    fude_zoom_sheet _sh;
    if(!fude_zoom_sheet_of(_n, _count, &_sh)) {
        return false;
    }
    const f64 _per = _o->scale * fude_zoom_page_home_units(_page, _o->frame) * FUDE_ZOOM_PAGE_MM_PER_UNIT;   // mm one of its own units
    if(_w_mm != NULL) {
        *_w_mm = 2.0 * _sh.hw * _per;
    }
    if(_h_mm != NULL) {
        *_h_mm = 2.0 * _sh.hh * _per;
    }
    if(_sheet != NULL) {
        *_sheet = _sh;
    }
    return true;
}

RDE_INTERNAL void fude_zoom_page_pair_said(const fude_zoom_page* _page, f64 _w_mm, f64 _h_mm, c8* _out, usize _size);

// See page_parts.h: what its chip says, "420 × 297 mm · 1:50"; the scale only when it is not 1:1.
void fude_zoom_page_sheet_said(const fude_zoom_page* _page, f64 _w_mm, f64 _h_mm, f64 _scale, c8* _out, usize _size) {
    c8 _pair[2u * FUDE_ZOOM_UNITS_TEXT + 8u];
    fude_zoom_page_pair_said(_page, _w_mm, _h_mm, _pair, sizeof(_pair));
    if(_scale > 1.0) {
        c8 _sc[24];
        fude_zoom_sheet_number(_scale, _sc, sizeof(_sc));
        rde_snprintf(_out, _size, "%s  \xC2\xB7  1:%s", _pair, _sc);
    } else {
        rde_snprintf(_out, _size, "%s", _pair);
    }
}

RDE_INTERNAL void fude_zoom_page_sheet_done(any _self, f64 _w_mm, f64 _h_mm, f64 _scale, u32 _flags);

// See page_parts.h: Insert's Measured sheet (FUDE_ZOOM_NONE), or that sheet changed, its card (a new one as the last
// one made was, else A4 across at 1:1 with its grid).
void fude_zoom_page_sheet_start(fude_zoom_page* _page, u32 _sheet) {
    if(!_page->open) {
        return;
    }
    f64 _w = 297.0, _h = 210.0, _scale = 1.0;
    u32 _flags = FUDE_ZOOM_SHEET_GRID;
    fude_zoom_sheet _sh;
    if(_sheet != FUDE_ZOOM_NONE && fude_zoom_page_sheet_of(_page, _sheet, &_w, &_h, &_sh)) {
        _page->sheet_editing = _sheet;
        _scale = _sh.scale;
        _flags = _sh.flags;
    } else {
        _page->sheet_editing = FUDE_ZOOM_NONE;
        if(_page->sheet_last[0] > 0.0) {
            _w = _page->sheet_last[0]; _h = _page->sheet_last[1]; _scale = _page->sheet_last[2]; _flags = (u32)_page->sheet_last[3];
        }
    }
    const b8 _new = _page->sheet_editing == FUDE_ZOOM_NONE;
    fude_zoom_sheet_open(&_page->sheet_form, fude_text(_new ? FUDE_TEXT_ZOOM_SHEET_NEW : FUDE_TEXT_ZOOM_SHEET_TITLE), _new, _w, _h, _scale, _flags, &_page->units,
                         fude_zoom_page_sheet_done, _page);
}

// The card's Insert (Done): a new sheet in the middle of the view, at its true size, under what it is put round
// (it is what they are drawn on), the view flown to it; or that sheet made so where it is, its bottom left corner
// (where its rulers count from) kept. One undo step, the lasso on it.
RDE_INTERNAL void fude_zoom_page_sheet_done(any _self, f64 _w_mm, f64 _h_mm, f64 _scale, u32 _flags) {
    fude_zoom_page*  _page = (fude_zoom_page*)_self;
    fude_zoom_scene* _s    = &_page->scene;
    if(!(_w_mm > 0.0) || !(_h_mm > 0.0)) {
        return;
    }
    _page->sheet_last[0] = _w_mm; _page->sheet_last[1] = _h_mm; _page->sheet_last[2] = _scale; _page->sheet_last[3] = (f64)_flags;
    const u32 _was = _page->sheet_editing;
    _page->sheet_editing = FUDE_ZOOM_NONE;
    fude_zoom_sheet _old_sheet;
    const b8 _editing = _was != FUDE_ZOOM_NONE && fude_zoom_page_sheet_of(_page, _was, NULL, NULL, &_old_sheet);
    fude_zoom_object _old;
    rde_memset(&_old, 0, sizeof(_old));
    u32 _frame;
    fude_zoom_place _place;
    if(_editing) {
        _old   = *fude_zoom_scene_object(_s, _was);
        _frame = _old.frame;
        _place = fude_zoom_scene_place_of(_s, _was);
    } else {
        const f64 _near = 4.0 / _s->camera.z;
        _frame = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _s->camera.at.x - _near, _s->camera.at.y - _near, _s->camera.at.x + _near, _s->camera.at.y + _near });
        _place = (fude_zoom_place){ fude_zoom_sim_apply(fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _frame)), (fude_zoom_v2){ 0.0, 0.0 }), 0.0, 1.0 };
    }
    const f64 _per = _place.scale * fude_zoom_page_home_units(_page, _frame) * FUDE_ZOOM_PAGE_MM_PER_UNIT;   // mm one of its own units
    if(!(_per > 0.0)) {
        return;
    }
    const f64 _hw = _w_mm * 0.5 / _per, _hh = _h_mm * 0.5 / _per;
    if(_editing) {
        // Its bottom left corner where it was: its middle moved by how much it grew each way (its own units, turned as
        // it is).
        const fude_zoom_sim _os = fude_zoom_object_sim(&_old);
        _place.t = fude_zoom_sim_apply(_os, (fude_zoom_v2){ _hw - _old_sheet.hw, _hh - _old_sheet.hh });
    }
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _count = fude_zoom_sheet_numbers(_n, _hw, _hh, _scale, _flags);
    // A new one under everything it is put round (in its frame): the lowest of them, less one.
    u64 _z = _editing ? _old.z : 0u;
    const f64 _k = fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, _frame));
    if(!_editing) {
        const f64 _rx = _hw * _place.scale, _ry = _hh * _place.scale;
        rde_arr _found = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        fude_zoom_scene_query(_s, _frame, (fude_zoom_box){ _place.t.x - _rx, _place.t.y - _ry, _place.t.x + _rx, _place.t.y + _ry }, &_found);
        u64 _low = 0;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_found); _i++) {
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, ((const u32*)_found.memory)[_i]);
            if((_o->flags & FUDE_ZOOM_FLAG_ALIVE) && _o->kind != FUDE_ZOOM_KIND_MARK && _o->kind != FUDE_ZOOM_KIND_LAYER && (_low == 0 || _o->z < _low)) {
                _low = _o->z;
            }
        }
        rde_arr_free(&_found);
        _z = _low > 1u ? _low - 1u : 0u;
    }
    const u16 _layer_was = _s->layer;
    if(_editing) {
        _s->layer = _old.layer;
        _s->style = _old.channels == FUDE_ZOOM_SHAPE_SYMBOL || _old.kind != FUDE_ZOOM_KIND_SHAPE ? 0 : _old.q;   // (its own line, as its layer)
    }
    const u32 _new = fude_zoom_scene_add_shape_fill(_s, _frame, _place, FUDE_ZOOM_SHAPE_SHEET, _n, _count,
                                                    _editing ? _old.color : _page->app->ink->color, _editing ? _old.radius : (f32)(0.6 / rde_math_max_f64(_k, 1e-300)),
                                                    _editing ? (u8)(_old.flags & (FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN)) : 0u,
                                                    _editing ? _old.fill : (rde_color){ 0, 0, 0, 0 }, _z);
    _s->layer = _layer_was;
    _s->style = 0;
    fude_zoom_box _box = fude_zoom_scene_object(_s, _new)->box;
    if(_editing) {
        fude_zoom_scene_set_alive(_s, _was, false);
        _box = fude_zoom_box_union(_box, _old.box);
        rde_arr _dead = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std()), _born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        rde_arr_add(&_dead, (any)&_was);
        rde_arr_add(&_born, (any)&_new);
        fude_zoom_page_rebind(_page, &_was, &_new, 1u, &_dead, &_born);   // (dimensions to its corners measure it again)
        fude_zoom_history_push(_s, _frame, _box, (const u32*)_dead.memory, (u32)rde_arr_length(&_dead), (const u32*)_born.memory, (u32)rde_arr_length(&_born));
        rde_arr_free(&_dead);
        rde_arr_free(&_born);
    } else {
        fude_zoom_history_push(_s, _frame, _box, NULL, 0, &_new, 1);
    }
    fude_zoom_select_clear(&_page->selection);
    rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _new });
    fude_zoom_select_update(&_page->selection, _s);
    _page->line_chip_object = _new;
    _page->line_chip_until  = rde_engine_get_time_now() + FUDE_ZOOM_PAGE_LINE_CHIP;
    _page->map_stale = true;
    if(!_editing) {
        // The view on it, all of it (a 21 m plan or a 20 mm part: put in the middle of a view of any size).
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _new);
        fude_zoom_page_fly(_page, fude_zoom_nav_framing(fude_zoom_page_half(_page), _o->frame, _o->box, 0.8), true);
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_SHEET_MADE));
    }
}

// See page_parts.h. Export's Sheets to scale: a PDF page each (at most 64), the size of the sheet at its scale (its
// box, if it is turned), what is on it drawn N times smaller, on white, with the printer's factors. Sheets on hidden
// layers or in frames not shown are left out. False: no sheets.
b8 fude_zoom_page_sheets_pdf(fude_zoom_page* _page, fude_bytes* _out) {
    fude_zoom_scene* _s = &_page->scene;
    enum { FUDE_ZOOM_PAGE_SHEETS_MOST = 64 };
    fude_zoom_camera _views[FUDE_ZOOM_PAGE_SHEETS_MOST];
    fude_zoom_v2     _halves[FUDE_ZOOM_PAGE_SHEETS_MOST];
    u32 _n = 0;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s) && _n < FUDE_ZOOM_PAGE_SHEETS_MOST; _i++) {
        fude_zoom_sheet _sh;
        if(!fude_zoom_page_sheet_of(_page, _i, NULL, NULL, &_sh)) {
            continue;
        }
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if(fude_zoom_scene_hides(_s, _o) || !fude_zoom_scene_frame_shown(_s, _o->frame)) {
            continue;
        }
        // Its own box in its frame (exactly its sides: not its line's width round them).
        const fude_zoom_sim _os = fude_zoom_object_sim(_o);
        fude_zoom_box _box = fude_zoom_box_empty();
        const fude_zoom_v2 _corner[4] = { { -_sh.hw, -_sh.hh }, { _sh.hw, -_sh.hh }, { _sh.hw, _sh.hh }, { -_sh.hw, _sh.hh } };
        for(u32 _c = 0; _c < 4u; _c++) {
            const fude_zoom_v2 _p = fude_zoom_sim_apply(_os, _corner[_c]);
            _box = fude_zoom_box_union(_box, (fude_zoom_box){ _p.x, _p.y, _p.x, _p.y });
        }
        const f64 _mm = fude_zoom_page_home_units(_page, _o->frame) * FUDE_ZOOM_PAGE_MM_PER_UNIT;   // mm one of its frame's units
        const f64 _pts = _mm / _sh.scale * 72.0 / 25.4;                                            // page points one of them
        if(!(_pts > 0.0)) {
            continue;
        }
        // (A page up to 200 inches, as PDF readers take: past that, smaller than its scale.)
        const f64 _w = (_box.max_x - _box.min_x) * _pts, _h = (_box.max_y - _box.min_y) * _pts;
        const f64 _fit = rde_math_min_f64(1.0, 14400.0 / rde_math_max_f64(_w, _h));
        _halves[_n] = (fude_zoom_v2){ rde_math_max_f64(_w * _fit * 0.5, 1.0), rde_math_max_f64(_h * _fit * 0.5, 1.0) };
        _views[_n]  = fude_zoom_nav_framing(_halves[_n], _o->frame, _box, 1.0);
        _n++;
    }
    if(_n == 0u) {
        return false;
    }
    return fude_zoom_export_pdf_pages(_s, _views, _halves, _n, (fude_zoom_v2){ _page->print_x, _page->print_y }, NULL, _out);
}

// --- the size card (sizeform.h): what the lasso holds, an exact size, angle and place ---------------

// See page_parts.h: the measured sheet under screen point _p (the smallest on the screen round it; not _not), its own
// units to the screen, its numbers and millimetres one of its own units (each out may be NULL). FUDE_ZOOM_NONE: none.
u32 fude_zoom_page_sheet_under(const fude_zoom_page* _page, fude_zoom_v2 _p, u32 _not, fude_zoom_sim* _all, fude_zoom_sheet* _sheet, f64* _mm) {
    const fude_zoom_scene* _s = &_page->scene;
    u32 _best = FUDE_ZOOM_NONE;
    f64 _least = 1e300;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        fude_zoom_sheet _sh;
        if(_i == _not || !fude_zoom_page_sheet_of(_page, _i, NULL, NULL, &_sh)) {
            continue;
        }
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if(fude_zoom_scene_hides(_s, _o) || !fude_zoom_scene_frame_shown(_s, _o->frame)) {
            continue;
        }
        const fude_zoom_sim _to = fude_zoom_sim_compose(fude_zoom_page_frame_sim(_page, _o->frame), fude_zoom_object_sim(_o));
        const fude_zoom_v2  _l  = fude_zoom_sim_apply(fude_zoom_sim_inverse(_to), _p);
        const f64 _k = fude_zoom_sim_scale(_to);
        if(rde_math_abs_f64(_l.x) > _sh.hw || rde_math_abs_f64(_l.y) > _sh.hh || _sh.hw * _sh.hh * _k * _k >= _least) {
            continue;
        }
        _least = _sh.hw * _sh.hh * _k * _k;
        _best  = _i;
        if(_all != NULL)   { *_all = _to; }
        if(_sheet != NULL) { *_sheet = _sh; }
        if(_mm != NULL)    { *_mm = fude_zoom_page_object_mm(_page, _i); }
    }
    return _best;
}

// Each point that bounds a thing, on the screen, into _out (fude_zoom_v2; appended): a stroke's or a fill's
// points (their middles: not its pen's width), a shape's outline, a text's or a picture's corners, a frame's anchor.
RDE_INTERNAL void fude_zoom_page_bounds_of(const fude_zoom_page* _page, u32 _object, rde_arr* _out) {
    const fude_zoom_scene*  _s  = &_page->scene;
    const fude_zoom_object* _o  = fude_zoom_scene_object(_s, _object);
    const fude_zoom_sim     _to = fude_zoom_page_frame_sim(_page, _o->frame);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
        rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap);
        b8 _closed;
        fude_zoom_scene_shape_outline(_s, _object, 360u, &_pts, &_closed);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_pts); _i++) {
            const fude_zoom_v2 _p = fude_zoom_sim_apply(_to, ((const fude_zoom_v2*)_pts.memory)[_i]);
            rde_arr_add(_out, (any)&_p);
        }
        rde_arr_free(&_pts);
    } else if((_o->kind == FUDE_ZOOM_KIND_STROKE || _o->kind == FUDE_ZOOM_KIND_FILL) && _o->count > 0u) {
        rde_arr _q_arr = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
        rde_arr_resize(&_q_arr, _o->count);
        fude_zoom_qpoint* _q = (fude_zoom_qpoint*)_q_arr.memory;
        if(fude_zoom_scene_points(_s, _object, _q)) {
            for(u32 _i = 0; _i < _o->count; _i++) {
                const fude_zoom_v2 _p = fude_zoom_sim_apply(_to, fude_zoom_scene_point_at(_o, &_q[_i]));
                rde_arr_add(_out, (any)&_p);
            }
        }
        rde_arr_free(&_q_arr);
    } else if(_o->kind == FUDE_ZOOM_KIND_TEXT || _o->kind == FUDE_ZOOM_KIND_IMAGE) {
        fude_zoom_v2 _c[4];
        if(_o->kind == FUDE_ZOOM_KIND_TEXT) {
            fude_zoom_scene_text_corners(_s, _object, _c);
        } else {
            fude_zoom_scene_image_corners(_s, _object, _c);
        }
        for(u32 _i = 0; _i < 4u; _i++) {
            const fude_zoom_v2 _p = fude_zoom_sim_apply(_to, _c[_i]);
            rde_arr_add(_out, (any)&_p);
        }
    } else {
        const fude_zoom_v2 _c[4] = { { _o->box.min_x, _o->box.min_y }, { _o->box.max_x, _o->box.min_y }, { _o->box.max_x, _o->box.max_y }, { _o->box.min_x, _o->box.max_y } };
        for(u32 _i = 0; _i < 4u; _i++) {
            const fude_zoom_v2 _p = fude_zoom_sim_apply(_to, _c[_i]);
            rde_arr_add(_out, (any)&_p);
        }
    }
}

// How what the lasso holds is measured: the screen → millimetres from the bottom left corner of the sheet it is on,
// along its sides (*_sheet: that sheet), or else from the screen's middle along its sides (FUDE_ZOOM_NONE).
RDE_INTERNAL fude_zoom_sim fude_zoom_page_measuring(const fude_zoom_page* _page, u32* _sheet) {
    const u32 _n = (u32)rde_arr_length(&_page->selection.picks);
    const u32 _lone = _n == 1u ? ((const fude_zoom_pick*)_page->selection.picks.memory)[0].object : FUDE_ZOOM_NONE;
    fude_zoom_box _b = _page->selection.box;
    *_sheet = FUDE_ZOOM_NONE;
    if(!fude_zoom_box_is_empty(_b)) {
        fude_zoom_sim _all;
        fude_zoom_sheet _sh;
        f64 _mm = 0.0;
        *_sheet = fude_zoom_page_sheet_under(_page, (fude_zoom_v2){ (_b.min_x + _b.max_x) * 0.5, (_b.min_y + _b.max_y) * 0.5 }, _lone, &_all, &_sh, &_mm);
        if(*_sheet != FUDE_ZOOM_NONE && _mm > 0.0) {
            return fude_zoom_sim_compose((fude_zoom_sim){ _mm, 0.0, _sh.hw * _mm, _sh.hh * _mm }, fude_zoom_sim_inverse(_all));
        }
        *_sheet = FUDE_ZOOM_NONE;
    }
    return (fude_zoom_sim){ fude_zoom_page_mm_per_point(_page), 0.0, 0.0, 0.0 };
}

// What the lasso holds, its bounding points in millimetres (through _to_mm), into _out (cleared first).
RDE_INTERNAL void fude_zoom_page_held_points(const fude_zoom_page* _page, fude_zoom_sim _to_mm, rde_arr* _out) {
    rde_arr_clear(_out);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        fude_zoom_page_bounds_of(_page, ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object, _out);
    }
    fude_zoom_v2* _p = (fude_zoom_v2*)_out->memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(_out); _i++) {
        _p[_i] = fude_zoom_sim_apply(_to_mm, _p[_i]);
    }
}

// Returns the box round _n points _p, each carried by _by first.
RDE_INTERNAL fude_zoom_box fude_zoom_page_points_box(const fude_zoom_v2* _p, u32 _n, fude_zoom_sim _by) {
    fude_zoom_box _b = fude_zoom_box_empty();
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_v2 _q = fude_zoom_sim_apply(_by, _p[_i]);
        _b = fude_zoom_box_union(_b, (fude_zoom_box){ _q.x, _q.y, _q.x, _q.y });
    }
    return _b;
}

// Returns, for a shape whose sides are each its own (a rectangle, an ellipse, a plain board, a diagram's symbol),
// which of its numbers are its half sizes (the first's index). FUDE_ZOOM_NONE: not one (a board once cut is not).
RDE_INTERNAL u32 fude_zoom_page_own_size(const fude_zoom_page* _page, u32 _object) {
    const fude_zoom_object* _o = fude_zoom_scene_object(&_page->scene, _object);
    if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE) {
        return FUDE_ZOOM_NONE;
    }
    if(_o->channels == FUDE_ZOOM_SHAPE_RECT || _o->channels == FUDE_ZOOM_SHAPE_ELLIPSE) {
        return 0u;
    }
    if(_o->channels == FUDE_ZOOM_SHAPE_SYMBOL) {
        return 1u;
    }
    if(_o->channels == FUDE_ZOOM_SHAPE_BOARD) {
        f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
        const u32 _c = fude_zoom_scene_shape_numbers(&_page->scene, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS);
        return _c >= 4u && !fude_zoom_board_is_cut(_n, _c) ? 0u : FUDE_ZOOM_NONE;
    }
    return FUDE_ZOOM_NONE;
}

// Returns a thing's way along (its own x) in the measuring's degrees, between -180 and 180; a hair from 0 is 0.
RDE_INTERNAL f64 fude_zoom_page_measured_angle(const fude_zoom_page* _page, u32 _object, fude_zoom_sim _to_mm) {
    const fude_zoom_object* _o = fude_zoom_scene_object(&_page->scene, _object);
    const fude_zoom_sim _all = fude_zoom_sim_compose(_to_mm, fude_zoom_sim_compose(fude_zoom_page_frame_sim(_page, _o->frame), fude_zoom_object_sim(_o)));
    f64 _deg = rde_math_atan2_f64(_all.b, _all.a) * 180.0 / RDE_PI;
    _deg = rde_math_abs_f64(_deg) < 1e-9 ? 0.0 : _deg;
    return _deg;
}

// Writes, for under its size, what it measures: a lone closed thing's area and perimeter (a circle's diameter first),
// an open one's length. "" when there is nothing to say (several things, a text, a picture, a dimension or another
// mark). An ellipse's perimeter is Ramanujan's approximation; anything else is measured along its outline or points,
// a fill's or a cut board's later rings taken off its area as holes.
RDE_INTERNAL void fude_zoom_page_measured_info(const fude_zoom_page* _page, fude_zoom_sim _to_mm, c8* _out, usize _size) {
    _out[0] = 0;
    if(rde_arr_length(&_page->selection.picks) != 1u) {
        return;
    }
    const fude_zoom_scene*  _s = &_page->scene;
    const u32               _object = ((const fude_zoom_pick*)_page->selection.picks.memory)[0].object;
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    const f64 _mm = fude_zoom_sim_scale(_to_mm) * fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, _o->frame));   // mm one of its frame's units
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap), _rings = rde_arr_new(sizeof(u32), _heap);
    b8 _closed = false;
    if(_o->kind == FUDE_ZOOM_KIND_SHAPE && _o->channels == FUDE_ZOOM_SHAPE_ELLIPSE) {
        f64 _n[2];
        if(fude_zoom_scene_shape_numbers(_s, _object, _n, 2u) == 2u) {
            const f64 _a = rde_math_abs_f64(_n[0]) * _o->scale * _mm, _b = rde_math_abs_f64(_n[1]) * _o->scale * _mm;
            const f64 _hq = (_a - _b) * (_a - _b) / ((_a + _b) * (_a + _b));
            const f64 _per = RDE_PI * (_a + _b) * (1.0 + 3.0 * _hq / (10.0 + rde_math_sqrt_f64(4.0 - 3.0 * _hq)));   // (Ramanujan's: exact for a circle)
            c8 _as[64], _ps[FUDE_ZOOM_UNITS_TEXT], _ds[FUDE_ZOOM_UNITS_TEXT];
            fude_zoom_size_area_format(RDE_PI * _a * _b, &_page->units, _as, sizeof(_as));
            fude_zoom_units_format(_per, &_page->units, _ps, sizeof(_ps));
            if(rde_math_abs_f64(_a - _b) <= 1e-9 * _a) {
                fude_zoom_units_format(2.0 * _a, &_page->units, _ds, sizeof(_ds));
                fude_text_format(_out, _size, FUDE_TEXT_ZOOM_SIZE_ROUND, (const fude_text_arg[]){ FUDE_TS(_ds), FUDE_TS(_as), FUDE_TS(_ps) }, 3u);
            } else {
                fude_text_format(_out, _size, FUDE_TEXT_ZOOM_SIZE_AREA, (const fude_text_arg[]){ FUDE_TS(_as), FUDE_TS(_ps) }, 2u);
            }
        }
        rde_arr_free(&_pts);
        rde_arr_free(&_rings);
        return;
    }
    if(_o->kind == FUDE_ZOOM_KIND_SHAPE && _o->channels == FUDE_ZOOM_SHAPE_BOARD) {
        rde_arr _num = rde_arr_new(sizeof(f64), _heap);
        const u32 _c = fude_zoom_scene_shape_numbers_all(_s, _object, &_num);
        fude_zoom_board_rings((const f64*)_num.memory, _c, &_pts, &_rings);
        rde_arr_free(&_num);
        fude_zoom_v2* _p = (fude_zoom_v2*)_pts.memory;
        const fude_zoom_sim _os = fude_zoom_object_sim(_o);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_pts); _i++) {
            _p[_i] = fude_zoom_sim_apply(_os, _p[_i]);
        }
        _closed = true;
    } else if(_o->kind == FUDE_ZOOM_KIND_SHAPE && _o->channels != FUDE_ZOOM_SHAPE_DIMENSION && _o->channels != FUDE_ZOOM_SHAPE_SYMBOL &&
              _o->channels != FUDE_ZOOM_SHAPE_SHEET && _o->channels != FUDE_ZOOM_SHAPE_RADIAL && _o->channels != FUDE_ZOOM_SHAPE_ANGLE &&
              _o->channels != FUDE_ZOOM_SHAPE_GUIDE) {
        fude_zoom_scene_shape_outline(_s, _object, 720u, &_pts, &_closed);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_pts); _i++) {
            rde_arr_add(&_rings, &(u32){ 0u });
        }
    } else if((_o->kind == FUDE_ZOOM_KIND_STROKE || _o->kind == FUDE_ZOOM_KIND_FILL) && _o->count > 1u) {
        rde_arr _q_arr = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
        rde_arr_resize(&_q_arr, _o->count);
        fude_zoom_qpoint* _q = (fude_zoom_qpoint*)_q_arr.memory;
        if(fude_zoom_scene_points(_s, _object, _q)) {
            for(u32 _i = 0; _i < _o->count; _i++) {
                const fude_zoom_v2 _p = fude_zoom_scene_point_at(_o, &_q[_i]);
                rde_arr_add(&_pts, (any)&_p);
                rde_arr_add(&_rings, &(u32){ _o->kind == FUDE_ZOOM_KIND_FILL ? _q[_i].time : 0u });
            }
        }
        rde_arr_free(&_q_arr);
        _closed = _o->kind == FUDE_ZOOM_KIND_FILL;
    }
    // Each ring's length and area (its holes taken off: a fill's later rings, a cut board's), in millimetres.
    const fude_zoom_v2* _p = (const fude_zoom_v2*)_pts.memory;
    const u32*          _r = (const u32*)_rings.memory;
    const u32           _n = (u32)rde_arr_length(&_pts);
    f64 _length = 0.0, _area = 0.0;
    for(u32 _from = 0; _from < _n;) {
        u32 _to = _from;
        while(_to < _n && _r[_to] == _r[_from]) {
            _to++;
        }
        f64 _ring_area = 0.0;
        for(u32 _i = _from; _i < _to; _i++) {
            const b8 _last = _i + 1u == _to;
            if(_last && !_closed) {
                break;
            }
            const fude_zoom_v2 _a = _p[_i], _b = _p[_last ? _from : _i + 1u];
            _length    += rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y) * _mm;
            _ring_area += (_a.x * _b.y - _b.x * _a.y) * 0.5 * _mm * _mm;
        }
        _area += _from == 0u ? rde_math_abs_f64(_ring_area) : -rde_math_abs_f64(_ring_area);
        _from = _to;
    }
    if(_n >= 2u && _length > 0.0) {
        c8 _ls[FUDE_ZOOM_UNITS_TEXT];
        fude_zoom_units_format(_length, &_page->units, _ls, sizeof(_ls));
        if(_closed && _n >= 3u) {
            c8 _as[64];
            fude_zoom_size_area_format(rde_math_max_f64(_area, 0.0), &_page->units, _as, sizeof(_as));
            fude_text_format(_out, _size, FUDE_TEXT_ZOOM_SIZE_AREA, (const fude_text_arg[]){ FUDE_TS(_as), FUDE_TS(_ls) }, 2u);
        } else {
            fude_text_format(_out, _size, FUDE_TEXT_ZOOM_SIZE_LONG, (const fude_text_arg[]){ FUDE_TS(_ls) }, 1u);
        }
    }
    rde_arr_free(&_pts);
    rde_arr_free(&_rings);
}

RDE_INTERNAL void fude_zoom_page_size_done(any _self, const fude_zoom_size_values* _typed);

// See page_parts.h. The size is the box of what the lasso holds, measured as fude_zoom_page_measuring says; a lone
// shape with sides of its own gives its own width and height and angle instead, and may be unlocked. Something straight
// along one way gives its length alone. Its place is given only on a sheet.
void fude_zoom_page_size_start(fude_zoom_page* _page) {
    if(!_page->open || !fude_zoom_select_any(&_page->selection) || fude_zoom_select_busy(&_page->selection)) {
        return;
    }
    fude_zoom_select_update(&_page->selection, &_page->scene);
    u32 _sheet = FUDE_ZOOM_NONE;
    const fude_zoom_sim _to_mm = fude_zoom_page_measuring(_page, &_sheet);
    fude_zoom_size_values _v;
    rde_memset(&_v, 0, sizeof(_v));
    _v.has_h  = true;
    _v.has_xy = _sheet != FUDE_ZOOM_NONE;
    const u32 _lone = rde_arr_length(&_page->selection.picks) == 1u ? ((const fude_zoom_pick*)_page->selection.picks.memory)[0].object : FUDE_ZOOM_NONE;
    const u32 _own  = _lone != FUDE_ZOOM_NONE ? fude_zoom_page_own_size(_page, _lone) : FUDE_ZOOM_NONE;
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    fude_zoom_page_held_points(_page, _to_mm, &_pts);
    const fude_zoom_box _b = fude_zoom_page_points_box((const fude_zoom_v2*)_pts.memory, (u32)rde_arr_length(&_pts), fude_zoom_sim_identity());
    rde_arr_free(&_pts);
    if(fude_zoom_box_is_empty(_b)) {
        return;
    }
    _v.x_mm = _b.min_x;
    _v.y_mm = _b.min_y;
    if(_own != FUDE_ZOOM_NONE) {
        // Its own sides, whichever way it is turned.
        f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
        const u32 _c = fude_zoom_scene_shape_numbers(&_page->scene, _lone, _n, FUDE_ZOOM_SHAPE_NUMBERS);
        const f64 _mm = fude_zoom_sim_scale(_to_mm) * fude_zoom_sim_scale(fude_zoom_sim_compose(fude_zoom_page_frame_sim(_page, fude_zoom_scene_object(&_page->scene, _lone)->frame),
                                                                                              fude_zoom_object_sim(fude_zoom_scene_object(&_page->scene, _lone))));
        if(_c < _own + 2u) {
            return;
        }
        _v.w_mm       = 2.0 * rde_math_abs_f64(_n[_own]) * _mm;
        _v.h_mm       = 2.0 * rde_math_abs_f64(_n[_own + 1u]) * _mm;
        _v.angle      = fude_zoom_page_measured_angle(_page, _lone, _to_mm);
        _v.can_unlock = true;
    } else {
        _v.w_mm = _b.max_x - _b.min_x;
        _v.h_mm = _b.max_y - _b.min_y;
        _v.turn = true;
        _v.locked = true;
        if(!(_v.w_mm > 0.0) || !(_v.h_mm > 0.0)) {
            // Straight along one way: its length alone, whichever way it goes (scaled by it).
            _v.has_h = false;
            _v.w_mm  = rde_math_max_f64(_b.max_x - _b.min_x, _b.max_y - _b.min_y);
            if(!(_v.w_mm > 0.0)) {
                return;
            }
        }
    }
    c8 _info[300];
    fude_zoom_page_measured_info(_page, _to_mm, _info, sizeof(_info));
    fude_zoom_size_open(&_page->size_form, fude_text(FUDE_TEXT_ZOOM_SIZE_TITLE), &_v, _info, &_page->units, fude_zoom_page_size_done, _page);
}

// The size card's Apply: a shape whose sides are its own made that size and turned to that angle (made again, one undo
// step); anything else scaled whole and turned by the angle, as the lasso's handles would (one undo step, its
// connectors following, what is in a container carried); then, on a sheet, moved so its box's bottom left corner is
// where typed.
RDE_INTERNAL void fude_zoom_page_size_done(any _self, const fude_zoom_size_values* _typed) {
    fude_zoom_page*  _page = (fude_zoom_page*)_self;
    fude_zoom_scene* _s    = &_page->scene;
    if(!fude_zoom_select_any(&_page->selection) || fude_zoom_select_busy(&_page->selection)) {
        return;
    }
    fude_zoom_select_update(&_page->selection, _s);
    u32 _sheet = FUDE_ZOOM_NONE;
    const fude_zoom_sim _to_mm   = fude_zoom_page_measuring(_page, &_sheet);
    const fude_zoom_sim _from_mm = fude_zoom_sim_inverse(_to_mm);
    const fude_zoom_size_values* _was = &_page->size_form.was;
    const u32 _lone = rde_arr_length(&_page->selection.picks) == 1u ? ((const fude_zoom_pick*)_page->selection.picks.memory)[0].object : FUDE_ZOOM_NONE;
    const u32 _own  = _lone != FUDE_ZOOM_NONE ? fude_zoom_page_own_size(_page, _lone) : FUDE_ZOOM_NONE;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    fude_zoom_page_held_points(_page, _to_mm, &_pts);
    const fude_zoom_box _b = fude_zoom_page_points_box((const fude_zoom_v2*)_pts.memory, (u32)rde_arr_length(&_pts), fude_zoom_sim_identity());
    if(fude_zoom_box_is_empty(_b)) {
        rde_arr_free(&_pts);
        return;
    }
    if(_own != FUDE_ZOOM_NONE) {
        const fude_zoom_object _old = *fude_zoom_scene_object(_s, _lone);
        rde_arr _num = rde_arr_new(sizeof(f64), _heap);
        const u32 _count = fude_zoom_scene_shape_numbers_all(_s, _lone, &_num);
        f64* _n = (f64*)_num.memory;
        const fude_zoom_sim _frame_to_mm = fude_zoom_sim_compose(_to_mm, fude_zoom_page_frame_sim(_page, _old.frame));
        const f64 _mm = fude_zoom_sim_scale(_frame_to_mm) * _old.scale;   // mm one of its own units
        if(_count < _own + 2u || !(_mm > 0.0)) {
            rde_arr_free(&_num);
            rde_arr_free(&_pts);
            return;
        }
        _n[_own]      = _typed->w_mm * 0.5 / _mm;
        _n[_own + 1u] = _typed->h_mm * 0.5 / _mm;
        fude_zoom_place _place = fude_zoom_scene_place_of(_s, _lone);
        _place.rotation += (_typed->angle - _was->angle) * RDE_PI / 180.0;
        // Where it goes: its box's bottom left corner where typed (it as it will be, measured where it is).
        if(_typed->has_xy) {
            fude_zoom_object _probe = _old;
            _probe.rotation = _place.rotation;
            rde_arr _outline = rde_arr_new(sizeof(fude_zoom_v2), _heap);
            b8 _closed;
            fude_zoom_shape_outline(_old.channels, _n, _count, 360u, &_outline, &_closed);
            const fude_zoom_box _nb = fude_zoom_page_points_box((const fude_zoom_v2*)_outline.memory, (u32)rde_arr_length(&_outline),
                                                                fude_zoom_sim_compose(_frame_to_mm, fude_zoom_object_sim(&_probe)));
            rde_arr_free(&_outline);
            const fude_zoom_v2 _d = { _typed->x_mm - _nb.min_x, _typed->y_mm - _nb.min_y };
            const fude_zoom_sim _back = fude_zoom_sim_inverse(_frame_to_mm);
            _place.t.x += _back.a * _d.x - _back.b * _d.y;
            _place.t.y += _back.b * _d.x + _back.a * _d.y;
        }
        const u16 _layer = _s->layer;
        _s->layer = _old.layer;
        _s->style = _old.channels == FUDE_ZOOM_SHAPE_SYMBOL || _old.kind != FUDE_ZOOM_KIND_SHAPE ? 0 : _old.q;   // (its own line, as its layer)
        const u32 _made = fude_zoom_scene_add_shape_fill(_s, _old.frame, _place, _old.channels, _n, _count, _old.color, _old.radius,
                                                         (u8)(_old.flags & (FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN)), _old.fill, _old.z);
        _s->layer = _layer;
        _s->style = 0;
        rde_arr_free(&_num);
        fude_zoom_scene_set_alive(_s, _lone, false);
        rde_arr _dead = rde_arr_new(sizeof(u32), _heap), _born = rde_arr_new(sizeof(u32), _heap);
        rde_arr_add(&_dead, (any)&_lone);
        rde_arr_add(&_born, (any)&_made);
        fude_zoom_page_rebind(_page, &_lone, &_made, 1u, &_dead, &_born);   // (connectors joined to it follow it)
        fude_zoom_history_push(_s, _old.frame, fude_zoom_box_union(_old.box, fude_zoom_scene_object(_s, _made)->box),
                               (const u32*)_dead.memory, (u32)rde_arr_length(&_dead), (const u32*)_born.memory, (u32)rde_arr_length(&_born));
        rde_arr_free(&_dead);
        rde_arr_free(&_born);
        ((fude_zoom_pick*)_page->selection.picks.memory)[0].object = _made;
        fude_zoom_select_update(&_page->selection, _s);
        rde_arr_free(&_pts);
        _page->map_stale = true;
        return;
    }
    // Scaled whole round its box's middle (its width typed, or its height when only that changed), turned by the angle.
    const fude_zoom_v2 _c = { (_b.min_x + _b.max_x) * 0.5, (_b.min_y + _b.max_y) * 0.5 };
    f64 _k = _typed->w_mm / rde_math_max_f64(_was->w_mm, 1e-300);
    if(_typed->has_h && rde_math_abs_f64(_typed->w_mm - _was->w_mm) <= 1e-9 * _was->w_mm && rde_math_abs_f64(_typed->h_mm - _was->h_mm) > 1e-9 * _was->h_mm) {
        _k = _typed->h_mm / rde_math_max_f64(_was->h_mm, 1e-300);
    }
    const f64 _t = _typed->angle * RDE_PI / 180.0;
    const fude_zoom_sim _turn = { _k * rde_math_cos_f64(_t), _k * rde_math_sin_f64(_t), 0.0, 0.0 };
    fude_zoom_sim _m = fude_zoom_sim_compose((fude_zoom_sim){ 1.0, 0.0, _c.x, _c.y }, fude_zoom_sim_compose(_turn, (fude_zoom_sim){ 1.0, 0.0, -_c.x, -_c.y }));
    if(_typed->has_xy) {
        const fude_zoom_box _nb = fude_zoom_page_points_box((const fude_zoom_v2*)_pts.memory, (u32)rde_arr_length(&_pts), _m);
        _m = fude_zoom_sim_compose((fude_zoom_sim){ 1.0, 0.0, _typed->x_mm - _nb.min_x, _typed->y_mm - _nb.min_y }, _m);
    }
    rde_arr_free(&_pts);
    // As a drag of it would: the screen's own way.
    fude_zoom_select_begin(&_page->selection, _s);
    fude_zoom_page_carry(_page);
    fude_zoom_select_end(&_page->selection, _s, fude_zoom_sim_compose(_from_mm, fude_zoom_sim_compose(_m, _to_mm)));
    if(_page->carried_keep > 0u) {
        fude_zoom_select_keep(&_page->selection, _s, _page->carried_keep);
        _page->carried_keep = 0;
    }
    _page->map_stale = true;
}

// --- Repeat (repeatform.h): what the lasso holds in a row, round a point, mirrored -------------------

RDE_INTERNAL void fude_zoom_page_repeat_done(any _self, const fude_zoom_repeat_values* _typed);

// See page_parts.h. The lasso's Repeat: the card, as a row of three a centimetre apart to begin with (round: six, the
// card's own start, about the sheet's middle or about a point under what is held), and for a mirror the line kept by
// its right (or bottom) edge.
void fude_zoom_page_repeat_start(fude_zoom_page* _page, u8 _mode) {
    if(!_page->open || !fude_zoom_select_any(&_page->selection) || fude_zoom_select_busy(&_page->selection)) {
        return;
    }
    fude_zoom_select_update(&_page->selection, &_page->scene);
    u32 _sheet = FUDE_ZOOM_NONE;
    const fude_zoom_sim _to_mm = fude_zoom_page_measuring(_page, &_sheet);
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    fude_zoom_page_held_points(_page, _to_mm, &_pts);
    const fude_zoom_box _b = fude_zoom_page_points_box((const fude_zoom_v2*)_pts.memory, (u32)rde_arr_length(&_pts), fude_zoom_sim_identity());
    rde_arr_free(&_pts);
    if(fude_zoom_box_is_empty(_b)) {
        return;
    }
    const f64 _w = _b.max_x - _b.min_x, _h = _b.max_y - _b.min_y;
    fude_zoom_repeat_values _v;
    rde_memset(&_v, 0, sizeof(_v));
    _v.mode   = _mode;
    _v.copies = 3u;
    _v.dx_mm  = _w + 10.0;   // (side by side, a centimetre between them)
    _v.angle  = 360.0;
    _v.turn   = true;
    _v.has_xy = _sheet != FUDE_ZOOM_NONE;
    if(_v.has_xy) {
        fude_zoom_sheet _sh;
        f64 _sw = 0.0, _shh = 0.0;
        fude_zoom_page_sheet_of(_page, _sheet, &_sw, &_shh, &_sh);
        _v.cx_mm = _sw * 0.5;
        _v.cy_mm = _shh * 0.5;
    } else {
        _v.cy_mm = -1.5 * rde_math_max_f64(_w, _h);
    }
    // The mirror's line: kept, by its right (or bottom) edge, the copy beside it; else through its middle.
    const fude_zoom_v2 _mid = { (_b.min_x + _b.max_x) * 0.5, (_b.min_y + _b.max_y) * 0.5 };
    const fude_zoom_v2 _from = _v.has_xy ? (fude_zoom_v2){ 0.0, 0.0 } : _mid;
    _v.keep       = true;
    _v.edge_mm[0] = _b.max_x - _from.x;
    _v.edge_mm[1] = _b.min_y - _from.y;
    _v.mid_mm[0]  = _mid.x - _from.x;
    _v.mid_mm[1]  = _mid.y - _from.y;
    fude_zoom_repeat_open(&_page->repeat_form, &_v, &_page->units, fude_zoom_page_repeat_done, _page);
}

// Repeat's Apply: the copies put down (one undo step), the lasso on the originals and them. A row adds the copies
// typed; round, the copies typed include the original, shared over a whole turn or spread from the first to the last
// over less; a mirror puts one copy, or moves the originals when they are not kept.
RDE_INTERNAL void fude_zoom_page_repeat_done(any _self, const fude_zoom_repeat_values* _typed) {
    fude_zoom_page*  _page = (fude_zoom_page*)_self;
    fude_zoom_scene* _s    = &_page->scene;
    if(!fude_zoom_select_any(&_page->selection) || fude_zoom_select_busy(&_page->selection)) {
        return;
    }
    fude_zoom_select_update(&_page->selection, _s);
    u32 _sheet = FUDE_ZOOM_NONE;
    const fude_zoom_sim _to_mm   = fude_zoom_page_measuring(_page, &_sheet);
    const fude_zoom_sim _from_mm = fude_zoom_sim_inverse(_to_mm);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    fude_zoom_page_held_points(_page, _to_mm, &_pts);
    const fude_zoom_box _b = fude_zoom_page_points_box((const fude_zoom_v2*)_pts.memory, (u32)rde_arr_length(&_pts), fude_zoom_sim_identity());
    rde_arr_free(&_pts);
    if(fude_zoom_box_is_empty(_b)) {
        return;
    }
    const fude_zoom_v2 _mid_mm = { (_b.min_x + _b.max_x) * 0.5, (_b.min_y + _b.max_y) * 0.5 };
    const fude_zoom_v2 _mid    = fude_zoom_sim_apply(_from_mm, _mid_mm);   // (the screen's)
    // What the lasso holds, as copies are made of it.
    const u32 _n = (u32)rde_arr_length(&_page->selection.picks);
    rde_arr _objects_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_objects_arr, _n);
    u32* _objects = (u32*)_objects_arr.memory;   // (sized once: it stays put)
    for(u32 _i = 0; _i < _n; _i++) {
        _objects[_i] = ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object;
    }
    rde_arr _clips = rde_arr_new(sizeof(fude_zoom_clip), _heap);
    fude_zoom_select_clips_of(_s, _objects, _n, &_clips);
    const fude_zoom_clip* _c = (const fude_zoom_clip*)_clips.memory;
    const u32 _nc = (u32)rde_arr_length(&_clips);
    if(_typed->mode == FUDE_ZOOM_REPEAT_MIRROR) {
        // Across an upright line of the sheet's (left ↔ right) or a level one (top ↔ bottom), where typed (on a sheet
        // from its corner, else from the middle of what is mirrored).
        const fude_zoom_v2 _way = _typed->axis == 0u ? (fude_zoom_v2){ -_from_mm.b, _from_mm.a } : (fude_zoom_v2){ _from_mm.a, _from_mm.b };
        const fude_zoom_v2 _base = _typed->has_xy ? (fude_zoom_v2){ 0.0, 0.0 } : _mid_mm;
        const fude_zoom_v2 _on_mm = _typed->axis == 0u ? (fude_zoom_v2){ _base.x + _typed->at_mm, _mid_mm.y } : (fude_zoom_v2){ _mid_mm.x, _base.y + _typed->at_mm };
        const fude_zoom_select_mirror _mirror = { fude_zoom_sim_apply(_from_mm, _on_mm), rde_math_atan2_f64(_way.y, _way.x) };
        fude_zoom_select_put(&_page->selection, _s, _c, _nc, NULL, 0u, &_mirror, _typed->keep ? NULL : _objects, _typed->keep ? 0u : _n);
    } else {
        const u32 _m = _typed->mode == FUDE_ZOOM_REPEAT_ROW ? _typed->copies : (_typed->copies > 1u ? _typed->copies - 1u : 0u);
        rde_arr _moves_arr = rde_arr_new(sizeof(fude_zoom_sim), _heap);
        rde_arr_resize(&_moves_arr, _m);
        fude_zoom_sim* _moves = (fude_zoom_sim*)_moves_arr.memory;
        if(_typed->mode == FUDE_ZOOM_REPEAT_ROW) {
            const fude_zoom_v2 _d = { _from_mm.a * _typed->dx_mm - _from_mm.b * _typed->dy_mm, _from_mm.b * _typed->dx_mm + _from_mm.a * _typed->dy_mm };
            for(u32 _k = 0; _k < _m; _k++) {
                _moves[_k] = (fude_zoom_sim){ 1.0, 0.0, _d.x * (f64)(_k + 1u), _d.y * (f64)(_k + 1u) };
            }
        } else {
            // Round its centre (on a sheet: from its corner; else from the middle of what is repeated), evenly: a whole
            // turn shared among them all, any less from the first to the last.
            const fude_zoom_v2 _cm = _typed->has_xy ? (fude_zoom_v2){ _typed->cx_mm, _typed->cy_mm } : (fude_zoom_v2){ _mid_mm.x + _typed->cx_mm, _mid_mm.y + _typed->cy_mm };
            const fude_zoom_v2 _cs = fude_zoom_sim_apply(_from_mm, _cm);
            const b8  _whole = rde_math_abs_f64(rde_math_abs_f64(_typed->angle) - 360.0) < 1e-9;
            const f64 _step  = _typed->angle * RDE_PI / 180.0 / (f64)(_whole ? _typed->copies : (_typed->copies > 1u ? _typed->copies - 1u : 1u));
            for(u32 _k = 0; _k < _m; _k++) {
                const f64 _t = _step * (f64)(_k + 1u);
                const fude_zoom_sim _turn = fude_zoom_sim_compose((fude_zoom_sim){ 1.0, 0.0, _cs.x, _cs.y },
                                                                  fude_zoom_sim_compose((fude_zoom_sim){ rde_math_cos_f64(_t), rde_math_sin_f64(_t), 0.0, 0.0 }, (fude_zoom_sim){ 1.0, 0.0, -_cs.x, -_cs.y }));
                if(_typed->turn) {
                    _moves[_k] = _turn;
                } else {
                    const fude_zoom_v2 _to = fude_zoom_sim_apply(_turn, _mid);   // (only carried round, the same way up)
                    _moves[_k] = (fude_zoom_sim){ 1.0, 0.0, _to.x - _mid.x, _to.y - _mid.y };
                }
            }
        }
        if(_m > 0u) {
            fude_zoom_select_put(&_page->selection, _s, _c, _nc, _moves, _m, NULL, NULL, 0u);
        }
        rde_arr_free(&_moves_arr);
    }
    fude_zoom_select_clips_free(&_clips);
    rde_arr_free(&_clips);
    rde_arr_free(&_objects_arr);
    _page->map_stale = true;
}

// See page_parts.h: the selection's row's Repeat, opening as a row.
void fude_zoom_page_repeat_selection(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_self); RDE_UNUSED(_arg);
    if(fude_zoom_page_the != NULL) {
        fude_zoom_page_repeat_start(fude_zoom_page_the, FUDE_ZOOM_REPEAT_ROW);
    }
}

// Writes two lengths as one size: "120 × 45 mm" (a metric unit said once, and ≈ once when either is rounded),
// "2' 6\" × 1' 3\"".
RDE_INTERNAL void fude_zoom_page_pair_said(const fude_zoom_page* _page, f64 _w_mm, f64 _h_mm, c8* _out, usize _size) {
    c8 _ws[FUDE_ZOOM_UNITS_TEXT], _hs[FUDE_ZOOM_UNITS_TEXT];
    fude_zoom_units_format(_w_mm, &_page->units, _ws, sizeof(_ws));
    fude_zoom_units_format(_h_mm, &_page->units, _hs, sizeof(_hs));
    static const c8 _about[] = "\xE2\x89\x88 ";   // (≈ and a space, as units.h writes it)
    const usize _al = sizeof(_about) - 1u;
    const b8 _wa = rde_strncmp(_ws, _about, _al) == 0, _ha = rde_strncmp(_hs, _about, _al) == 0;
    c8* _w = _wa ? _ws + _al : _ws;
    const c8* _h = _ha ? _hs + _al : _hs;
    const c8* _wu = rde_strrchr(_w, ' '), *_hu = rde_strrchr(_h, ' ');
    if(_wu != NULL && _hu != NULL && rde_strcmp(_wu, _hu) == 0 && (rde_strcmp(_wu, " mm") == 0 || rde_strcmp(_wu, " cm") == 0 || rde_strcmp(_wu, " m") == 0)) {
        _w[_wu - _w] = 0;   // (its unit said once, after the second)
    }
    rde_snprintf(_out, _size, "%s%s \xC3\x97 %s", _wa || _ha ? _about : "", _w, _h);
}

// See page_parts.h: its size as the chip says it ("120 × 45 mm"), worked out again only when the scene changes or what
// the lasso holds does (the count and the first 16 objects held). False when there is nothing to say.
b8 fude_zoom_page_size_said(fude_zoom_page* _page, c8* _out, usize _size) {
    const u32 _n = (u32)rde_arr_length(&_page->selection.picks);
    u64 _key = (u64)_page->scene.revision * 1000003ull + _n;
    for(u32 _i = 0; _i < _n && _i < 16u; _i++) {
        _key = _key * 31ull + ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object;
    }
    if(_key != _page->size_key || _page->size_said[0] == 0) {
        _page->size_key = _key;
        _page->size_said[0] = 0;
        u32 _sheet = FUDE_ZOOM_NONE;
        const fude_zoom_sim _to_mm = fude_zoom_page_measuring(_page, &_sheet);
        rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
        fude_zoom_page_held_points(_page, _to_mm, &_pts);
        const fude_zoom_box _b = fude_zoom_page_points_box((const fude_zoom_v2*)_pts.memory, (u32)rde_arr_length(&_pts), fude_zoom_sim_identity());
        rde_arr_free(&_pts);
        if(!fude_zoom_box_is_empty(_b)) {
            fude_zoom_page_pair_said(_page, _b.max_x - _b.min_x, _b.max_y - _b.min_y, _page->size_said, sizeof(_page->size_said));
        }
    }
    rde_snprintf(_out, _size, "%s", _page->size_said);
    return _page->size_said[0] != 0;
}
