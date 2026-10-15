// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"

// ===========================================================================
// Page, editing what the lasso holds: a fillet, a chamfer, extend and trim (trim.h), Combine (cut.h), constraints
// (connect.h), the lasso's row, and the page's menu (a long press, a right-click).
//
// In order: a fillet; a chamfer, extend and trim (each made through fude_zoom_page_relines, which makes the lines
// again as one undo step); Combine; constraints; To curve and the lasso's row (its buttons, and what each shows for
// what is held); a PDF's text copied; then the page's menu. Each edit is one undo step, with connectors and
// dimensions joined to what changed following it (fude_zoom_page_rebind), and the lasso put on what was made.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// --- a fillet (shape.h): the corner two lines make, rounded ----------------------------------------

// Puts the two lines the lasso holds alone into *_a and *_b. Returns false unless the lasso holds exactly two live
// LINE shapes in one frame.
RDE_INTERNAL b8 fude_zoom_page_lassoed_corner(const fude_zoom_page* _page, u32* _a, u32* _b) {
    if(rde_arr_length(&_page->selection.picks) != 2u) {
        return false;
    }
    const fude_zoom_scene* _s = &_page->scene;
    u32 _o[2];
    for(u32 _i = 0; _i < 2u; _i++) {
        _o[_i] = ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object;
        if(_o[_i] >= fude_zoom_scene_object_count(_s)) {
            return false;
        }
        const fude_zoom_object* _l = fude_zoom_scene_object(_s, _o[_i]);
        if(!(_l->flags & FUDE_ZOOM_FLAG_ALIVE) || _l->kind != FUDE_ZOOM_KIND_SHAPE || _l->channels != FUDE_ZOOM_SHAPE_LINE) {
            return false;
        }
    }
    if(fude_zoom_scene_object(_s, _o[0])->frame != fude_zoom_scene_object(_s, _o[1])->frame) {
        return false;
    }
    *_a = _o[0];
    *_b = _o[1];
    return true;
}

// The corner rounded _mm round: both lines cut back to where the arc touches them, the arc between them (the first
// line's colour and width), each on its line's layer. One undo step, the lasso on the three. A corner that cannot be
// rounded so says so in a notice.
void fude_zoom_page_fillet(fude_zoom_page* _page, f64 _mm) {
    u32 _a, _b;
    if(!fude_zoom_page_lassoed_corner(_page, &_a, &_b)) {
        return;
    }
    fude_zoom_scene* _s = &_page->scene;
    const fude_zoom_object _oa = *fude_zoom_scene_object(_s, _a), _ob = *fude_zoom_scene_object(_s, _b);
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    b8 _closed;
    fude_zoom_v2 _e[4];
    fude_zoom_scene_shape_outline(_s, _a, 1u, &_pts, &_closed);
    _e[0] = ((const fude_zoom_v2*)_pts.memory)[0]; _e[1] = ((const fude_zoom_v2*)_pts.memory)[1];
    fude_zoom_scene_shape_outline(_s, _b, 1u, &_pts, &_closed);
    _e[2] = ((const fude_zoom_v2*)_pts.memory)[0]; _e[3] = ((const fude_zoom_v2*)_pts.memory)[1];
    rde_arr_free(&_pts);
    // Millimetres a unit of their frame.
    const f64 _per = fude_zoom_page_home_units(_page, _oa.frame) * FUDE_ZOOM_PAGE_MM_PER_UNIT;
    fude_zoom_v2 _ak, _ae, _bk, _be, _c;
    f64 _from, _sweep;
    if(!(_per > 0.0) || !fude_zoom_shape_fillet(_e[0], _e[1], _e[2], _e[3], _mm / _per, &_ak, &_ae, &_bk, &_be, &_c, &_from, &_sweep)) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_FILLET_FAILED));
        return;
    }
    const u16 _was = _s->layer;
    u32 _made[3];
    _s->layer = _oa.layer;
    const f64 _na[2] = { _ae.x - _ak.x, _ae.y - _ak.y };
    _made[0] = fude_zoom_scene_add_shape_fill(_s, _oa.frame, (fude_zoom_place){ _ak, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, _na, 2u, _oa.color, _oa.radius * (f32)_oa.scale, _oa.flags, _oa.fill, _oa.z);
    _s->layer = _ob.layer;
    const f64 _nb[2] = { _be.x - _bk.x, _be.y - _bk.y };
    _made[1] = fude_zoom_scene_add_shape_fill(_s, _ob.frame, (fude_zoom_place){ _bk, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, _nb, 2u, _ob.color, _ob.radius * (f32)_ob.scale, _ob.flags, _ob.fill, _ob.z);
    _s->layer = _oa.layer;
    const f64 _nc[3] = { _mm / _per, _from, _sweep };
    _made[2] = fude_zoom_scene_add_shape(_s, _oa.frame, (fude_zoom_place){ _c, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ARC, _nc, 3u, _oa.color, _oa.radius * (f32)_oa.scale, 0u, 0);
    _s->layer = _was;
    fude_zoom_scene_set_alive(_s, _a, false);
    fude_zoom_scene_set_alive(_s, _b, false);
    rde_arr _dead = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std()), _born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    const u32 _old[2] = { _a, _b };
    rde_arr_add(&_dead, (any)&_old[0]); rde_arr_add(&_dead, (any)&_old[1]);
    for(u32 _i = 0; _i < 3u; _i++) {
        rde_arr_add(&_born, (any)&_made[_i]);
    }
    fude_zoom_page_rebind(_page, _old, _made, 2u, &_dead, &_born);   // Connectors joined to a line follow it.
    fude_zoom_box _box = fude_zoom_box_union(_oa.box, _ob.box);
    for(u32 _i = 0; _i < 3u; _i++) {
        _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, _made[_i])->box);
    }
    fude_zoom_history_push(_s, _oa.frame, _box, (const u32*)_dead.memory, (u32)rde_arr_length(&_dead), (const u32*)_born.memory, (u32)rde_arr_length(&_born));
    rde_arr_free(&_dead);
    rde_arr_free(&_born);
    fude_zoom_select_clear(&_page->selection);
    for(u32 _i = 0; _i < 3u; _i++) {
        rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _made[_i] });
    }
    fude_zoom_select_update(&_page->selection, _s);
}

// The fillet's radius typed: kept (with the unit it was typed in) for next time, and the corner rounded.
RDE_INTERNAL void fude_zoom_page_fillet_typed(any _self, const c8* _text, FUDE_ZOOM_UNIT_ _unit, f64 _mm) {
    RDE_UNUSED(_text);
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    if(!(_mm > 0.0)) {
        return;
    }
    _page->units.unit = _unit;
    _page->fillet_mm  = _mm;
    fude_zoom_page_fillet(_page, _mm);
}

// Asks for the fillet's radius on the numpad (the last one typed, else 10 mm, to type over), then rounds the corner.
RDE_INTERNAL void fude_zoom_page_fillet_selection(fude_zoom_page* _page) {
    c8 _text[48];
    fude_zoom_page_length_text(_page, _page->fillet_mm > 0.0 ? _page->fillet_mm : 10.0, _text, sizeof(_text));
    fude_zoom_numpad_open(&_page->numpad, _text, _page->units.unit, false, NULL, fude_zoom_page_fillet_typed, _page);
    fude_zoom_numpad_ask(&_page->numpad, fude_text(FUDE_TEXT_ZOOM_FILLET_RADIUS));
}

// --- a chamfer, extend, trim (trim.h): lines made to meet what they should -------------------------------

// Lines _old (_old_n, dead, in one undo step) made again as lines from _from[k] to _to[k] (their frame's units; _n of
// them, at most 8, each looking as the old one it is from, _of[k]), with _extra (already made) born in the same step;
// connectors following; the lasso on what was made and is alive.
RDE_INTERNAL void fude_zoom_page_relines(fude_zoom_page* _page, const u32* _old, u32 _old_n, const fude_zoom_v2* _from, const fude_zoom_v2* _to,
                                         const u32* _of, u32 _n, const u32* _extra, u32 _extra_n) {
    fude_zoom_scene* _s = &_page->scene;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _dead = rde_arr_new(sizeof(u32), _heap), _born = rde_arr_new(sizeof(u32), _heap);
    fude_zoom_box _box = fude_zoom_box_empty();
    u32 _frame = FUDE_ZOOM_NONE;
    const u16 _was = _s->layer;
    const i8  _style = _s->style;
    // The lines made, at most 8.
    u32 _made[8];
    for(u32 _i = 0; _i < _n && _i < 8u; _i++) {
        const fude_zoom_object _o = *fude_zoom_scene_object(_s, _of[_i]);
        _s->layer = _o.layer;
        _s->style = (i8)_o.q;
        const f64 _num[2] = { _to[_i].x - _from[_i].x, _to[_i].y - _from[_i].y };
        _made[_i] = fude_zoom_scene_add_shape_fill(_s, _o.frame, (fude_zoom_place){ _from[_i], 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, _num, 2u, _o.color,
                                                   _o.radius * (f32)_o.scale, _o.flags, _o.fill, _o.z);
        rde_arr_add(&_born, (any)&_made[_i]);
        _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, _made[_i])->box);
        _frame = _o.frame;
    }
    _s->layer = _was;
    _s->style = _style;
    for(u32 _i = 0; _i < _extra_n; _i++) {
        rde_arr_add(&_born, (any)&_extra[_i]);
        _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, _extra[_i])->box);
    }
    for(u32 _i = 0; _i < _old_n; _i++) {
        _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, _old[_i])->box);
        _frame = _frame != FUDE_ZOOM_NONE ? _frame : fude_zoom_scene_object(_s, _old[_i])->frame;
        fude_zoom_scene_set_alive(_s, _old[_i], false);
        rde_arr_add(&_dead, (any)&_old[_i]);
    }
    if(_n > 0u && _old_n > 0u) {
        // Connectors and dimensions joined to a line follow what it became: its first piece.
        u32 _to_new[8];
        for(u32 _i = 0; _i < _old_n && _i < 8u; _i++) {
            _to_new[_i] = _made[0];
            for(u32 _k = 0; _k < _n; _k++) {
                if(_of[_k] == _old[_i]) {
                    _to_new[_i] = _made[_k];
                    break;
                }
            }
        }
        fude_zoom_page_rebind(_page, _old, _to_new, _old_n < 8u ? _old_n : 8u, &_dead, &_born);
    }
    fude_zoom_history_push(_s, _frame, _box, (const u32*)_dead.memory, (u32)rde_arr_length(&_dead), (const u32*)_born.memory, (u32)rde_arr_length(&_born));
    fude_zoom_select_clear(&_page->selection);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_born); _i++) {
        if(fude_zoom_scene_object(_s, ((const u32*)_born.memory)[_i])->flags & FUDE_ZOOM_FLAG_ALIVE) {
            rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ ((const u32*)_born.memory)[_i] });
        }
    }
    fude_zoom_select_update(&_page->selection, _s);
    rde_arr_free(&_dead);
    rde_arr_free(&_born);
}

// Puts a line's two ends (its frame's units) into *_a and *_b. Returns false when its outline is not two points.
RDE_INTERNAL b8 fude_zoom_page_line_ends(const fude_zoom_page* _page, u32 _line, fude_zoom_v2* _a, fude_zoom_v2* _b) {
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    b8 _closed;
    fude_zoom_scene_shape_outline(&_page->scene, _line, 1u, &_pts, &_closed);
    const b8 _ok = rde_arr_length(&_pts) == 2u;
    if(_ok) {
        *_a = ((const fude_zoom_v2*)_pts.memory)[0];
        *_b = ((const fude_zoom_v2*)_pts.memory)[1];
    }
    rde_arr_free(&_pts);
    return _ok;
}

// The corner the two lassoed lines make cut straight _mm back along each, with a line across (0: each carried on or
// cut back to meet, with no cut).
void fude_zoom_page_chamfer(fude_zoom_page* _page, f64 _mm) {
    u32 _a, _b;
    if(!fude_zoom_page_lassoed_corner(_page, &_a, &_b)) {
        return;
    }
    fude_zoom_v2 _e[4];
    if(!fude_zoom_page_line_ends(_page, _a, &_e[0], &_e[1]) || !fude_zoom_page_line_ends(_page, _b, &_e[2], &_e[3])) {
        return;
    }
    const f64 _per = fude_zoom_page_home_units(_page, fude_zoom_scene_object(&_page->scene, _a)->frame) * FUDE_ZOOM_PAGE_MM_PER_UNIT;
    fude_zoom_v2 _ak, _ae, _bk, _be;
    if(!(_per > 0.0) || !fude_zoom_shape_chamfer(_e[0], _e[1], _e[2], _e[3], _mm / _per, &_ak, &_ae, &_bk, &_be)) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_FILLET_FAILED));
        return;
    }
    const u32 _old[2] = { _a, _b };
    fude_zoom_v2 _from[3] = { _ak, _bk, _ae }, _to[3] = { _ae, _be, _be };
    const u32 _of[3] = { _a, _b, _a };
    // 0: the corner sharp, no cut.
    fude_zoom_page_relines(_page, _old, 2u, _from, _to, _of, _mm > 0.0 ? 3u : 2u, NULL, 0u);
}

// The chamfer's length typed (0 allowed: a sharp corner): kept for next time, and the corner cut.
RDE_INTERNAL void fude_zoom_page_chamfer_typed(any _self, const c8* _text, FUDE_ZOOM_UNIT_ _unit, f64 _mm) {
    RDE_UNUSED(_text);
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    if(!(_mm >= 0.0)) {
        return;
    }
    _page->units.unit = _unit;
    _page->chamfer_mm = _mm;
    fude_zoom_page_chamfer(_page, _mm);
}

// The lasso's row's Chamfer: with a corner held, asks for its length (the last one typed, else 5 mm).
RDE_INTERNAL void fude_zoom_page_chamfer_selection(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_zoom_page* _page = fude_zoom_page_the;
    u32 _la, _lb;
    if(_page == NULL || !_page->open || fude_zoom_select_busy(&_page->selection) || !fude_zoom_page_lassoed_corner(_page, &_la, &_lb)) {
        return;
    }
    c8 _text[48];
    fude_zoom_page_length_text(_page, _page->chamfer_mm > 0.0 ? _page->chamfer_mm : 5.0, _text, sizeof(_text));
    fude_zoom_numpad_open(&_page->numpad, _text, _page->units.unit, false, NULL, fude_zoom_page_chamfer_typed, _page);
    fude_zoom_numpad_ask(&_page->numpad, fude_text(FUDE_TEXT_ZOOM_CHAMFER_ASK));
}

// Returns the live LINE shape the lasso holds alone, or FUDE_ZOOM_NONE.
RDE_INTERNAL u32 fude_zoom_page_lone_line(const fude_zoom_page* _page) {
    if(rde_arr_length(&_page->selection.picks) != 1u) {
        return FUDE_ZOOM_NONE;
    }
    const u32 _o = ((const fude_zoom_pick*)_page->selection.picks.memory)[0].object;
    const fude_zoom_object* _l = fude_zoom_scene_object(&_page->scene, _o);
    return _l->kind == FUDE_ZOOM_KIND_SHAPE && _l->channels == FUDE_ZOOM_SHAPE_LINE && (_l->flags & FUDE_ZOOM_FLAG_ALIVE) ? _o : FUDE_ZOOM_NONE;
}

#define FUDE_ZOOM_PAGE_EXTEND_REACH 200.0   // Extend looks this many of the line's lengths past each end.

// Extend: the lone line's ends carried on to the first thing each meets (an end that meets nothing stays; when neither
// meets anything, a notice says so). At most 64 crossings are looked at.
void fude_zoom_page_extend(fude_zoom_page* _page) {
    const u32 _line = fude_zoom_page_lone_line(_page);
    fude_zoom_v2 _a, _b;
    if(_line == FUDE_ZOOM_NONE || !fude_zoom_page_line_ends(_page, _line, &_a, &_b)) {
        return;
    }
    const u32 _frame = fude_zoom_scene_object(&_page->scene, _line)->frame;
    f64 _t[64];
    const u32 _n = fude_zoom_trim_crossings(&_page->scene, _frame, _line, _a, _b, -FUDE_ZOOM_PAGE_EXTEND_REACH, 1.0 + FUDE_ZOOM_PAGE_EXTEND_REACH, _t, 64u);
    f64 _lo = 0.0, _hi = 1.0;
    b8 _lo_on = false, _hi_on = false;
    for(u32 _i = 0; _i < _n; _i++) {
        if(_t[_i] < -1e-9 && (!_lo_on || _t[_i] > _lo)) {
            _lo = _t[_i];
            _lo_on = true;
        }
        if(_t[_i] > 1.0 + 1e-9 && (!_hi_on || _t[_i] < _hi)) {
            _hi = _t[_i];
            _hi_on = true;
        }
    }
    if(!_lo_on && !_hi_on) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_EXTEND_NONE));
        return;
    }
    const fude_zoom_v2 _d = { _b.x - _a.x, _b.y - _a.y };
    const fude_zoom_v2 _from = { _a.x + _d.x * _lo, _a.y + _d.y * _lo }, _to = { _a.x + _d.x * _hi, _a.y + _d.y * _hi };
    fude_zoom_page_relines(_page, &_line, 1u, &_from, &_to, &_line, 1u, NULL, 0u);
}

// Returns the line nearest _screen within _reach (screen points) at any depth shown, with the parameter along it there
// (0 to 1) in *_along; or FUDE_ZOOM_NONE.
RDE_INTERNAL u32 fude_zoom_page_line_at(fude_zoom_page* _page, rde_vec_2F _screen, f64 _reach, f64* _along) {
    rde_arr_clear(&_page->editable);
    fude_zoom_render_editable(&_page->renderer, &_page->scene, &_page->editable);
    const fude_zoom_scene* _s = &_page->scene;
    rde_arr _found = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
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
            fude_zoom_v2 _a, _b;
            if(_ob->kind != FUDE_ZOOM_KIND_SHAPE || _ob->channels != FUDE_ZOOM_SHAPE_LINE || !fude_zoom_scene_touchable(_s, _ob) ||
               !fude_zoom_page_line_ends(_page, _o, &_a, &_b)) {
                continue;
            }
            _a = fude_zoom_sim_apply(_vis.to_screen, _a);
            _b = fude_zoom_sim_apply(_vis.to_screen, _b);
            const fude_zoom_v2 _pen = { _screen.x, _screen.y };
            const f64 _t  = rde_math_segment_closest_t_2d_f64(_pen, _a, _b);
            const f64 _dd = rde_math_point_segment_distance_2d_f64(_pen, _a, _b);
            if(_dd <= _least) {
                _least = _dd;
                _best  = _o;
                *_along = _t;
            }
        }
    }
    rde_arr_free(&_found);
    return _best;
}

// Trim: the piece of the line under the pen (within 16 points) between the crossings either side of it cut away (no
// crossings: the whole line). The lasso is left empty, since the pen trims on.
void fude_zoom_page_trim_at(fude_zoom_page* _page, rde_vec_2F _screen) {
    f64 _at = 0.0;
    const u32 _line = fude_zoom_page_line_at(_page, _screen, 16.0, &_at);
    fude_zoom_v2 _a, _b;
    if(_line == FUDE_ZOOM_NONE || !fude_zoom_page_line_ends(_page, _line, &_a, &_b)) {
        return;
    }
    f64 _t[64], _keep[4];
    const u32 _n = fude_zoom_trim_crossings(&_page->scene, fude_zoom_scene_object(&_page->scene, _line)->frame, _line, _a, _b, 0.0, 1.0, _t, 64u);
    const u32 _k = fude_zoom_trim_keep(_t, _n, _at, _keep);
    const fude_zoom_v2 _d = { _b.x - _a.x, _b.y - _a.y };
    fude_zoom_v2 _from[2], _to[2];
    const u32 _of[2] = { _line, _line };
    for(u32 _i = 0; _i < _k; _i++) {
        _from[_i] = (fude_zoom_v2){ _a.x + _d.x * _keep[2u * _i], _a.y + _d.y * _keep[2u * _i] };
        _to[_i]   = (fude_zoom_v2){ _a.x + _d.x * _keep[2u * _i + 1u], _a.y + _d.y * _keep[2u * _i + 1u] };
    }
    fude_zoom_page_relines(_page, &_line, 1u, _from, _to, _of, _k, NULL, 0u);
    fude_zoom_select_clear(&_page->selection);   // The pen trims on: nothing held.
}

// --- Combine (cut.h): closed shapes and fills joined, cut out of the lowest, or kept where they overlap -------------

// Appends a thing's rings (its frame's units) to _pts and _rings (each point's ring number: *_ring on, one more for
// each ring): a closed shape's outline, or a fill's rings (only its outline, its first ring, when _outline_only).
// Returns false when it is not closed (a line, a symbol, a sheet and the like).
RDE_INTERNAL b8 fude_zoom_page_rings_of(const fude_zoom_page* _page, u32 _object, b8 _outline_only, rde_arr* _pts, rde_arr* _rings, u32* _ring) {
    const fude_zoom_scene*  _s = &_page->scene;
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
        const u8 _t = _o->channels;
        if(_t == FUDE_ZOOM_SHAPE_SHEET || _t == FUDE_ZOOM_SHAPE_GUIDE || _t == FUDE_ZOOM_SHAPE_SYMBOL || _t == FUDE_ZOOM_SHAPE_DIMENSION ||
           _t == FUDE_ZOOM_SHAPE_RADIAL || _t == FUDE_ZOOM_SHAPE_ANGLE || _t == FUDE_ZOOM_SHAPE_ARROW || _t == FUDE_ZOOM_SHAPE_BOARD) {
            return false;
        }
        rde_arr _line = rde_arr_new(sizeof(fude_zoom_v2), _heap);
        b8 _closed = false;
        fude_zoom_scene_shape_outline(_s, _object, 96u, &_line, &_closed);
        const u32 _m = (u32)rde_arr_length(&_line);
        if(_closed && _m >= 3u) {
            for(u32 _i = 0; _i < _m; _i++) {
                rde_arr_add(_pts, (any)&((const fude_zoom_v2*)_line.memory)[_i]);
                rde_arr_add(_rings, (any)_ring);
            }
            (*_ring)++;
        }
        rde_arr_free(&_line);
        return _closed && _m >= 3u;
    }
    if(_o->kind != FUDE_ZOOM_KIND_FILL || _o->count < 3u) {
        return false;
    }
    rde_arr _q = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
    rde_arr_add_n(&_q, _o->count);
    const b8 _ok = fude_zoom_scene_points(_s, _object, (fude_zoom_qpoint*)_q.memory);
    if(_ok) {
        const fude_zoom_qpoint* _qp = (const fude_zoom_qpoint*)_q.memory;
        u32 _last = _qp[0].time;
        for(u32 _i = 0; _i < _o->count; _i++) {
            if(_qp[_i].time != _last) {
                if(_outline_only) {
                    break;
                }
                _last = _qp[_i].time;
                (*_ring)++;
            }
            const fude_zoom_v2 _p = fude_zoom_scene_point_at(_o, &_qp[_i]);
            rde_arr_add(_pts, (any)&_p);
            rde_arr_add(_rings, (any)_ring);
        }
        (*_ring)++;
    }
    rde_arr_free(&_q);
    return _ok;
}

// A closed thing held and its order key, for putting them lowest first (fude_zoom_page_lassoed_closed).
typedef struct {
    u64 z;
    u32 object;
} fude_zoom_page_held_z;

// rde_arr_sort's order for fude_zoom_page_held_z: the lower order key first.
RDE_INTERNAL i32 fude_zoom_page_held_z_order(const any _a, const any _b) {
    const u64 _za = ((const fude_zoom_page_held_z*)_a)->z, _zb = ((const fude_zoom_page_held_z*)_b)->z;
    return _za < _zb ? -1 : _za > _zb ? 1 : 0;
}

// Writes what the lasso holds into _out (u32 objects, cleared first; NULL when only the count is wanted), lowest
// first, when it is closed things only (closed shapes and fills, two at least, alive, in one frame). Returns how many,
// or 0 when it is not that. However many are held: the row's face asks through fude_zoom_page_combine_offered, which
// keeps the answer until what is held or the scene changes.
RDE_INTERNAL u32 fude_zoom_page_lassoed_closed(const fude_zoom_page* _page, rde_arr* _out) {
    const fude_zoom_scene* _s = &_page->scene;
    const u32 _n = (u32)rde_arr_length(&_page->selection.picks);
    if(_out != NULL) {
        rde_arr_clear(_out);
    }
    if(_n < 2u) {
        return 0;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap), _rings = rde_arr_new(sizeof(u32), _heap);
    rde_arr _held = rde_arr_new(sizeof(fude_zoom_page_held_z), _heap);
    u32 _frame = FUDE_ZOOM_NONE, _ring = 0;
    b8 _all = true;
    for(u32 _i = 0; _i < _n && _all; _i++) {
        const u32 _o = ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object;
        const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _o);
        rde_arr_clear(&_pts);
        rde_arr_clear(&_rings);
        _all = (_ob->flags & FUDE_ZOOM_FLAG_ALIVE) && (_frame == FUDE_ZOOM_NONE || _ob->frame == _frame) &&
               ((_ob->kind == FUDE_ZOOM_KIND_SHAPE && fude_zoom_page_rings_of(_page, _o, true, &_pts, &_rings, &_ring)) || _ob->kind == FUDE_ZOOM_KIND_FILL);
        _frame = _ob->frame;
        rde_arr_add(&_held, &(fude_zoom_page_held_z){ _ob->z, _o });
    }
    if(_all && _out != NULL) {
        rde_arr_sort(&_held, fude_zoom_page_held_z_order);   // In order, lowest first.
        for(u32 _i = 0; _i < _n; _i++) {
            rde_arr_add(_out, (any)&((const fude_zoom_page_held_z*)_held.memory)[_i].object);
        }
    }
    rde_arr_free(&_pts);
    rde_arr_free(&_rings);
    rde_arr_free(&_held);
    return _all ? _n : 0u;
}

// See page_parts.h.
u64 fude_zoom_page_held_key(const fude_zoom_page* _page) {
    const u32 _n    = (u32)rde_arr_length(&_page->selection.picks);
    const u64 _seed = ((u64)_page->scene.revision << 32) | _n;
    return _n > 0u ? rde_hash_64_seeded(_page->selection.picks.memory, (usize)_n * sizeof(fude_zoom_pick), _seed) : _seed;
}

// Whether Combine is offered for what the lasso holds (two closed things or more: fude_zoom_page_lassoed_closed).
// The row's faces ask each frame and knowing means reading each thing's outline (a fill's points), so the answer is
// kept under combine_key (fude_zoom_page_held_key) and worked out again only when what is held or the scene changes.
RDE_INTERNAL b8 fude_zoom_page_combine_offered(fude_zoom_page* _page) {
    if(rde_arr_length(&_page->selection.picks) < 2u) {
        return false;
    }
    const u64 _key = fude_zoom_page_held_key(_page);
    if(_key != _page->combine_key) {
        _page->combine_key     = _key;
        _page->combine_offered = fude_zoom_page_lassoed_closed(_page, NULL) >= 2u;
    }
    return _page->combine_offered;
}

// Combine (cut.h): the lowest of the closed things held (its holes kept) with the outlines of the others, joined (_op
// FUDE_ZOOM_CUT_UNION), the others cut out of it (SUBTRACT) or kept where they overlap (INTERSECT). Each piece made
// looks as the lowest did: a fill when it was filled (its rings on a grid fine enough for its line), and its rings as
// lines when it was a shape. One undo step; a notice when nothing is left.
void fude_zoom_page_combine(fude_zoom_page* _page, u8 _op) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _things_arr = rde_arr_new(sizeof(u32), _heap);
    const u32 _n = fude_zoom_page_lassoed_closed(_page, &_things_arr);
    if(_n < 2u) {
        rde_arr_free(&_things_arr);
        return;
    }
    const u32* _things = (const u32*)_things_arr.memory;
    fude_zoom_scene* _s = &_page->scene;
    rde_arr _a = rde_arr_new(sizeof(fude_zoom_v2), _heap), _ar = rde_arr_new(sizeof(u32), _heap);
    rde_arr _b = rde_arr_new(sizeof(fude_zoom_v2), _heap), _br = rde_arr_new(sizeof(u32), _heap);
    u32 _ring = 0;
    fude_zoom_page_rings_of(_page, _things[0], false, &_a, &_ar, &_ring);   // The lowest: the region, its holes kept.
    for(u32 _i = 1; _i < _n; _i++) {
        fude_zoom_page_rings_of(_page, _things[_i], true, &_b, &_br, &_ring);   // The others: each its outline.
    }
    // The region's rings numbered from 0: ring 0 its outline.
    u32* _arv = (u32*)_ar.memory;
    const u32 _first = rde_arr_length(&_ar) > 0u ? _arv[0] : 0u;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_ar); _i++) {
        _arv[_i] -= _first;
    }
    rde_arr _out = rde_arr_new(sizeof(fude_zoom_v2), _heap), _out_rings = rde_arr_new(sizeof(u32), _heap), _out_piece = rde_arr_new(sizeof(u32), _heap);
    const u32 _pieces = fude_zoom_cut_combine((const fude_zoom_v2*)_a.memory, _arv, (u32)rde_arr_length(&_a), (const fude_zoom_v2*)_b.memory, (const u32*)_br.memory,
                                              (u32)rde_arr_length(&_b), _op, &_out, &_out_rings, &_out_piece);
    if(_pieces == 0u) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_COMBINE_NONE));
    } else {
        // Each piece: its rings as lines in the lowest one's look; filled (it was), a fill with its holes under them.
        const fude_zoom_object _look = *fude_zoom_scene_object(_s, _things[0]);
        const b8  _lines  = _look.kind == FUDE_ZOOM_KIND_SHAPE;
        const b8  _filled = _look.kind == FUDE_ZOOM_KIND_FILL || (_look.flags & FUDE_ZOOM_FLAG_FILLED);
        const rde_color _fill = _look.kind == FUDE_ZOOM_KIND_FILL ? _look.color : ((_look.flags & FUDE_ZOOM_FLAG_FILL_OWN) ? _look.fill : _look.color);
        const f64 _width = (f64)_look.radius * _look.scale;
        const fude_zoom_v2* _p  = (const fude_zoom_v2*)_out.memory;
        const u32*          _pr = (const u32*)_out_rings.memory;
        const u32*          _pp = (const u32*)_out_piece.memory;
        const u32           _m  = (u32)rde_arr_length(&_out);
        fude_zoom_box _all = fude_zoom_box_empty();
        for(u32 _i = 0; _i < _m; _i++) {
            _all = fude_zoom_box_union(_all, (fude_zoom_box){ _p[_i].x, _p[_i].y, _p[_i].x, _p[_i].y });
        }
        // The points' grid: an eighth of the line's width (or of a thousandth of the size), as a power of two.
        const f64 _size = rde_math_max_f64(_all.max_x - _all.min_x, _all.max_y - _all.min_y);
        const i8  _qe   = (i8)rde_math_max_f64(-60.0, rde_math_min_f64(60.0, rde_math_floor_f64(rde_math_log2_f64(rde_math_max_f64(rde_math_min_f64(_width > 0.0 ? _width : _size, _size * 1e-3), 1e-300) / 8.0))));
        const f64 _grid = rde_math_ldexp_f64(1.0, _qe);
        rde_arr _born = rde_arr_new(sizeof(u32), _heap), _dead = rde_arr_new(sizeof(u32), _heap);
        rde_arr _q = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
        const u16 _was = _s->layer;
        _s->layer = _look.layer;
        for(u32 _from = 0; _from < _m;) {
            u32 _to = _from;
            while(_to < _m && _pp[_to] == _pp[_from]) {
                _to++;
            }
            const fude_zoom_v2 _o = _p[_from];
            if(_filled) {
                rde_arr_clear(&_q);
                for(u32 _i = _from; _i < _to; _i++) {
                    const fude_zoom_qpoint _qp = { (i32)rde_math_round_to_i64_f64((_p[_i].x - _o.x) / _grid), (i32)rde_math_round_to_i64_f64((_p[_i].y - _o.y) / _grid), 0u, _pr[_i] };
                    rde_arr_add(&_q, (any)&_qp);
                }
                const u32 _f = fude_zoom_scene_add_fill(_s, _look.frame, (fude_zoom_place){ _o, 0.0, 1.0 }, _qe, (const fude_zoom_qpoint*)_q.memory, _to - _from, _fill, _look.z);
                rde_arr_add(&_born, (any)&_f);
            }
            for(u32 _r = _from; _lines && _r < _to;) {
                u32 _re = _r;
                while(_re < _to && _pr[_re] == _pr[_r]) {
                    _re++;
                }
                rde_arr_clear(&_q);
                const fude_zoom_v2 _ro = _p[_r];
                for(u32 _i = _r; _i <= _re; _i++) {   // Closed: back to its first point.
                    const fude_zoom_v2 _at = _p[_i < _re ? _i : _r];
                    const fude_zoom_qpoint _qp = { (i32)rde_math_round_to_i64_f64((_at.x - _ro.x) / _grid), (i32)rde_math_round_to_i64_f64((_at.y - _ro.y) / _grid), 0u, 0u };
                    rde_arr_add(&_q, (any)&_qp);
                }
                const u32 _l = fude_zoom_scene_add_stroke_at(_s, _look.frame, (fude_zoom_place){ _ro, 0.0, 1.0 }, _qe, (const fude_zoom_qpoint*)_q.memory,
                                                             (u32)rde_arr_length(&_q), 0u, _look.color, (f32)_width, 0u, 0, 0);
                rde_arr_add(&_born, (any)&_l);
                _r = _re;
            }
            _from = _to;
        }
        _s->layer = _was;
        fude_zoom_box _box = fude_zoom_box_empty();
        for(u32 _i = 0; _i < _n; _i++) {
            _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, _things[_i])->box);
            fude_zoom_scene_set_alive(_s, _things[_i], false);
            rde_arr_add(&_dead, (any)&_things[_i]);
        }
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_born); _i++) {
            _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, ((const u32*)_born.memory)[_i])->box);
        }
        fude_zoom_history_push(_s, _look.frame, _box, (const u32*)_dead.memory, (u32)rde_arr_length(&_dead), (const u32*)_born.memory, (u32)rde_arr_length(&_born));
        fude_zoom_select_clear(&_page->selection);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_born); _i++) {
            rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ ((const u32*)_born.memory)[_i] });
        }
        fude_zoom_select_update(&_page->selection, _s);
        rde_arr_free(&_born);
        rde_arr_free(&_dead);
        rde_arr_free(&_q);
    }
    rde_arr_free(&_a); rde_arr_free(&_ar); rde_arr_free(&_b); rde_arr_free(&_br);
    rde_arr_free(&_out); rde_arr_free(&_out_rings); rde_arr_free(&_out_piece);
    rde_arr_free(&_things_arr);
}

// Combine's card chosen: joined, the others cut out of the lowest, or kept where they overlap.
RDE_INTERNAL void fude_zoom_page_combine_chosen(any _self, u32 _index) {
    static const u8 _ops[3] = { FUDE_ZOOM_CUT_UNION, FUDE_ZOOM_CUT_SUBTRACT, FUDE_ZOOM_CUT_INTERSECT };
    if(_index < 3u) {
        fude_zoom_page_combine((fude_zoom_page*)_self, _ops[_index]);
    }
}

// The lasso's row's Combine: with closed things held, its card asks how.
void fude_zoom_page_combine_selection(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->open || fude_zoom_select_busy(&_page->selection) || !fude_zoom_page_combine_offered(_page)) {
        return;
    }
    const c8* const _labels[3] = { fude_text(FUDE_TEXT_ZOOM_COMBINE_UNION), fude_text(FUDE_TEXT_ZOOM_COMBINE_SUBTRACT), fude_text(FUDE_TEXT_ZOOM_COMBINE_INTERSECT) };
    const c8* const _icons[3]  = { FUDE_ICON_PLUS, FUDE_ICON_MINUS, FUDE_ICON_TARGET };
    fude_zoom_choice_open(&_page->choice_form, fude_text(FUDE_TEXT_ZOOM_COMBINE_TITLE), _labels, _icons, 3u, fude_zoom_page_combine_chosen, _page);
}

// --- constraints (connect.h): two lines kept parallel, square, the same length ---------------------------------

// The second line kept so with the first (_kind: parallel, square or equal): made again so when it is not (the one put
// down later is the one moved), and the constraint made. One undo step, the lasso on the two.
RDE_INTERNAL void fude_zoom_page_constrain(fude_zoom_page* _page, u8 _kind) {
    u32 _a, _b;
    if(!fude_zoom_page_lassoed_corner(_page, &_a, &_b)) {
        return;
    }
    fude_zoom_scene* _s = &_page->scene;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _dead = rde_arr_new(sizeof(u32), _heap), _born = rde_arr_new(sizeof(u32), _heap);
    // The one put down later follows the one put down first.
    if(fude_zoom_scene_object(_s, _a)->z > fude_zoom_scene_object(_s, _b)->z) {
        const u32 _t = _a; _a = _b; _b = _t;
    }
    fude_zoom_box _box = fude_zoom_box_union(fude_zoom_scene_object(_s, _a)->box, fude_zoom_scene_object(_s, _b)->box);
    fude_zoom_sim _m;
    if(fude_zoom_connect_constrain(_s, _kind, _a, _b, &_m)) {
        const fude_zoom_object _o = *fude_zoom_scene_object(_s, _b);
        rde_arr _ns = rde_arr_new(sizeof(f64), _heap);
        const u32 _count = fude_zoom_scene_shape_numbers_all(_s, _b, &_ns);
        const u16 _layer = _s->layer;
        const i8  _style = _s->style;
        _s->layer = _o.layer;
        _s->style = _o.q;   // (a line's own style and hatch; the scene's put back after, as fude_zoom_page_relines does)
        const u32 _made = fude_zoom_scene_add_shape_fill(_s, _o.frame, fude_zoom_place_moved(fude_zoom_scene_place_of(_s, _b), _m), _o.channels,
                                                         (const f64*)_ns.memory, _count, _o.color, _o.radius, _o.flags, _o.fill, _o.z);
        _s->layer = _layer;
        _s->style = _style;
        rde_arr_free(&_ns);
        fude_zoom_scene_set_alive(_s, _b, false);
        rde_arr_add(&_dead, (any)&_b);
        rde_arr_add(&_born, (any)&_made);
        // Its connectors, dimensions and other constraints follow it.
        fude_zoom_page_rebind(_page, &_b, &_made, 1u, &_dead, &_born);
        _b = _made;
        _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, _made)->box);
    }
    const u32 _c = fude_zoom_connect_constraint_add(_s, _kind, _a, _b);
    rde_arr_add(&_born, (any)&_c);
    fude_zoom_history_push(_s, fude_zoom_scene_object(_s, _a)->frame, _box, (const u32*)_dead.memory, (u32)rde_arr_length(&_dead), (const u32*)_born.memory,
                           (u32)rde_arr_length(&_born));
    fude_zoom_select_clear(&_page->selection);
    rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _a });
    rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _b });
    fude_zoom_select_update(&_page->selection, _s);
    rde_arr_free(&_dead);
    rde_arr_free(&_born);
}

// Lets go of the constraints between the two lassoed lines, as one undo step. Returns how many.
RDE_INTERNAL u32 fude_zoom_page_unconstrain(fude_zoom_page* _page) {
    u32 _a, _b;
    if(!fude_zoom_page_lassoed_corner(_page, &_a, &_b)) {
        return 0;
    }
    fude_zoom_scene* _s = &_page->scene;
    rde_arr _dead = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_box _box = fude_zoom_box_empty();
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        u8  _kind;
        u32 _ca, _cb;
        if(fude_zoom_connect_constraint_of(_s, _i, &_kind, &_ca, &_cb) && ((_ca == _a && _cb == _b) || (_ca == _b && _cb == _a))) {
            _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, _i)->box);
            fude_zoom_scene_set_alive(_s, _i, false);
            rde_arr_add(&_dead, (any)&_i);
        }
    }
    const u32 _n = (u32)rde_arr_length(&_dead);
    if(_n > 0u) {
        fude_zoom_history_push(_s, fude_zoom_scene_object(_s, _a)->frame, _box, (const u32*)_dead.memory, _n, NULL, 0);
    }
    rde_arr_free(&_dead);
    return _n;
}

// The constraints' card chosen: parallel, square or equal made (and a notice says it holds), or (3) let go.
RDE_INTERNAL void fude_zoom_page_constrain_chosen(any _self, u32 _index) {
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    if(_index < 3u) {
        static const u8 _kinds[3] = { FUDE_ZOOM_CONSTRAINT_PARALLEL, FUDE_ZOOM_CONSTRAINT_SQUARE, FUDE_ZOOM_CONSTRAINT_EQUAL };
        fude_zoom_page_constrain(_page, _kinds[_index]);
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_CONSTRAIN_HOLDS));
    } else if(_index == 3u) {
        fude_zoom_page_unconstrain(_page);
    }
}

// The lasso's row's Constrain: with two lines held, its card asks which.
RDE_INTERNAL void fude_zoom_page_constrain_selection(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_zoom_page* _page = fude_zoom_page_the;
    u32 _a, _b;
    if(_page == NULL || !_page->open || fude_zoom_select_busy(&_page->selection) || !fude_zoom_page_lassoed_corner(_page, &_a, &_b)) {
        return;
    }
    const c8* const _labels[4] = { fude_text(FUDE_TEXT_ZOOM_CONSTRAIN_PARALLEL), fude_text(FUDE_TEXT_ZOOM_CONSTRAIN_SQUARE), fude_text(FUDE_TEXT_ZOOM_CONSTRAIN_EQUAL),
                                   fude_text(FUDE_TEXT_ZOOM_CONSTRAIN_NONE) };
    const c8* const _icons[4]  = { FUDE_ICON_PAUSE, FUDE_ICON_PLUS, FUDE_ICON_CHECKS, FUDE_ICON_CLOSE };
    fude_zoom_choice_open(&_page->choice_form, fude_text(FUDE_TEXT_ZOOM_CONSTRAIN_TITLE), _labels, _icons, 4u, fude_zoom_page_constrain_chosen, _page);
}

// Marks the constraints of what the lasso holds: ∥, ⊥ or = by each of their lines' middles, for at most 16
// constraints. Not while presenting or while the lasso is busy.
void fude_zoom_page_render_constraints(fude_zoom_page* _page, fude_zoom_v2 _half) {
    if(_page->presenting || !fude_zoom_select_any(&_page->selection) || fude_zoom_select_busy(&_page->selection)) {
        return;
    }
    const fude_zoom_scene* _s = &_page->scene;
    u32 _shown = 0;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s) && _shown < 16u; _i++) {
        u8  _kind;
        u32 _l[2];
        if(!fude_zoom_connect_constraint_of(_s, _i, &_kind, &_l[0], &_l[1]) || _l[0] == FUDE_ZOOM_NONE || _l[1] == FUDE_ZOOM_NONE) {
            continue;
        }
        b8 _held = false;
        for(u32 _k = 0; _k < (u32)rde_arr_length(&_page->selection.picks); _k++) {
            const u32 _p = ((const fude_zoom_pick*)_page->selection.picks.memory)[_k].object;
            _held = _held || _p == _l[0] || _p == _l[1];
        }
        if(!_held) {
            continue;
        }
        static const c8* const _marks[FUDE_ZOOM_CONSTRAINT_KINDS] = { "", "\xE2\x88\xA5", "\xE2\x8A\xA5", "=" };
        const c8* _mark = _kind < FUDE_ZOOM_CONSTRAINT_KINDS ? _marks[_kind] : "";
        for(u32 _k = 0; _k < 2u; _k++) {
            fude_zoom_v2 _a, _b;
            if(!fude_zoom_page_line_ends(_page, _l[_k], &_a, &_b)) {
                continue;
            }
            const fude_zoom_sim _to = fude_zoom_page_frame_sim(_page, fude_zoom_scene_object(_s, _l[_k])->frame);
            const fude_zoom_v2 _m = fude_zoom_sim_apply(_to, (fude_zoom_v2){ (_a.x + _b.x) * 0.5, (_a.y + _b.y) * 0.5 });
            fude_zoom_page_draw_tag(_page, (rde_vec_2F){ (f32)_m.x - 24.0f, (f32)_m.y + 30.0f }, _mark, NULL, _half);
        }
        _shown++;
    }
}

// One run of a gesture's pieces made a path (fude_zoom_page_curve_selection's; _runs[_r] the run): its points
// smoothed of a hand's tremor, fitted with nodes, its sharp turns kept as corners, closed when its ends meet, on
// its first piece's layer, line and colour. A dot or a dab (fewer than 8 points) is left as it is. The path is
// added to _born and the pieces, let go, to _died; _box grows round both, and _frame_any is the run's frame. _pts
// and _t_arr are the caller's, used again for each run.
RDE_INTERNAL void fude_zoom_page_curve_run(fude_zoom_page* _page, fude_zoom_scene* _s, u32* _pieces, const fude_zoom_run* _runs, u32 _r, rde_arr* _pts,
                                           rde_arr* _t_arr, rde_arr* _died, rde_arr* _born, fude_zoom_box* _box, u32* _frame_any) {
    const fude_zoom_object _look = *fude_zoom_scene_object(_s, _pieces[_runs[_r].first]);
    const f64 _radius = (f64)_look.radius * _look.scale;
    rde_arr_clear(_pts);
    fude_zoom_page_run_points(_s, _pieces, _runs[_r], _pts);
    const u32 _count = (u32)rde_arr_length(_pts);
    if(_count < 8u) {
        return;   // A dot or a dab: left as it is.
    }
    // Its nodes: within two and a half points on the screen (a hand's tremor smoothed away), its ends kept; closed
    // when they meet.
    const f64 _k     = fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, _look.frame));
    const f64 _tol   = 2.5 / rde_math_max_f64(_k, 1e-300);
    fude_zoom_v2 _nodes[FUDE_ZOOM_PATH_NODES];
    {
        // A hand's tremor smoothed out first: a moving average 7 points wide, twice; its ends where they were.
        fude_zoom_v2* _w = (fude_zoom_v2*)_pts->memory;
        rde_arr_resize(_t_arr, _count);
        fude_zoom_v2* _t = (fude_zoom_v2*)_t_arr->memory;
        for(u32 _pass = 0; _pass < 2u; _pass++) {
            for(u32 _j = 0; _j < _count; _j++) {
                const u32 _lo = _j >= 3u ? _j - 3u : 0u, _hi = _j + 3u < _count ? _j + 3u : _count - 1u;
                fude_zoom_v2 _sum = { 0.0, 0.0 };
                for(u32 _k2 = _lo; _k2 <= _hi; _k2++) { _sum.x += _w[_k2].x; _sum.y += _w[_k2].y; }
                _t[_j] = (fude_zoom_v2){ _sum.x / (f64)(_hi - _lo + 1u), _sum.y / (f64)(_hi - _lo + 1u) };
            }
            _t[0] = _w[0];
            _t[_count - 1u] = _w[_count - 1u];
            rde_memcpy(_w, _t, (usize)_count * sizeof(fude_zoom_v2));
        }
    }
    const fude_zoom_v2* _p = (const fude_zoom_v2*)_pts->memory;
    const b8 _closed = _count >= 8u && rde_math_hypot_f64(_p[0].x - _p[_count - 1u].x, _p[0].y - _p[_count - 1u].y) <= rde_math_max_f64(3.0 * _radius, 6.0 / rde_math_max_f64(_k, 1e-300));
    u32 _m = fude_zoom_shape_fit_path(_p, _count, _tol, FUDE_ZOOM_PATH_NODES, _nodes);
    if(_closed && _m >= 4u) {
        _m--;   // Its last node is its first.
    }
    f64 _num[FUDE_ZOOM_SHAPE_NUMBERS];
    u64 _bits = _closed && _m >= 3u ? FUDE_ZOOM_PATH_CLOSED : 0u;
    // A sharp turn (more than about 65 degrees) a corner: a box drawn by hand keeps its corners.
    for(u32 _j = 0; _j < _m; _j++) {
        if(!(_bits & FUDE_ZOOM_PATH_CLOSED) && (_j == 0 || _j + 1u == _m)) {
            continue;
        }
        const fude_zoom_v2 _a = _nodes[(_j + _m - 1u) % _m], _b = _nodes[_j], _c = _nodes[(_j + 1u) % _m];
        const f64 _in  = rde_math_atan2_f64(_b.y - _a.y, _b.x - _a.x), _out = rde_math_atan2_f64(_c.y - _b.y, _c.x - _b.x);
        f64 _turn = rde_math_abs_f64(_out - _in);
        _turn = _turn > RDE_PI ? 2.0 * RDE_PI - _turn : _turn;
        // Between two long stretches only: a tremor's short zigzag is not a corner.
        const b8 _long = rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y) > 8.0 * _tol && rde_math_hypot_f64(_c.x - _b.x, _c.y - _b.y) > 8.0 * _tol;
        if(_turn > 1.13 && _long) {
            _bits |= FUDE_ZOOM_PATH_CORNER(_j);
        }
    }
    _num[0] = (f64)_bits;
    for(u32 _j = 0; _j < _m; _j++) {
        _num[1u + 2u * _j] = _nodes[_j].x - _nodes[0].x;
        _num[2u + 2u * _j] = _nodes[_j].y - _nodes[0].y;
    }
    const u16 _was = _s->layer;
    _s->layer = _look.layer;
    const u32 _path = fude_zoom_scene_add_shape(_s, _look.frame, (fude_zoom_place){ _nodes[0], 0.0, 1.0 }, FUDE_ZOOM_SHAPE_PATH, _num, 1u + 2u * _m,
                                                _look.color, (f32)_radius, 0u, _look.z);
    _s->layer = _was;
    rde_arr_add(_born, (any)&_path);
    *_box = fude_zoom_box_union(*_box, fude_zoom_scene_object(_s, _path)->box);
    for(u32 _j = _runs[_r].first; _j < _runs[_r].first + _runs[_r].count; _j++) {
        *_box = fude_zoom_box_union(*_box, fude_zoom_scene_object(_s, _pieces[_j])->box);
        fude_zoom_scene_set_alive(_s, _pieces[_j], false);
        rde_arr_add(_died, (any)&_pieces[_j]);
    }
    *_frame_any = _look.frame;
}

// The lasso's row's To curve, whose face says what it does for what is held: two lines, Fillet; a line alone, Extend;
// a diagram's symbol alone, Edit text; boards, Fit parts; formulas, Graph; typed texts, Handwriting. Otherwise each
// gesture's strokes held become a path (fude_zoom_page_curve_run, above, for each run of its pieces): their points
// smoothed of a hand's tremor, fitted with nodes, its sharp turns kept as corners, closed when its ends meet. One undo
// step; a notice when nothing was made.
void fude_zoom_page_curve_selection(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->open || rde_arr_length(&_page->selection.picks) == 0 || fude_zoom_select_busy(&_page->selection)) {
        return;
    }
    u32 _la, _lb;
    if(fude_zoom_page_lassoed_corner(_page, &_la, &_lb)) {
        fude_zoom_page_fillet_selection(_page);   // Two lines held: this button is Fillet (its face says so).
        return;
    }
    if(fude_zoom_page_lone_line(_page) != FUDE_ZOOM_NONE) {
        fude_zoom_page_extend(_page);   // A line alone: this button is Extend.
        return;
    }
    const u32 _symbol = fude_zoom_page_lone_symbol(_page);
    if(_symbol != FUDE_ZOOM_NONE) {
        fude_zoom_page_symbol_edit(_page, _symbol);   // A diagram's symbol held: it is Edit text.
        return;
    }
    u32 _stock = FUDE_ZOOM_NONE;
    if(fude_zoom_page_lassoed_boards(_page, &_stock) > 0u) {
        fude_zoom_page_fit_selection(_page);   // Boards held: this button is Fit parts (its face says so).
        return;
    }
    if(fude_zoom_formula_present() && fude_zoom_page_lassoed_formulas(_page) > 0u && fude_zoom_page_lassoed_texts(_page) == 0u) {
        fude_zoom_page_graph_formulas(_page);   // Formulas held: it is Graph.
        return;
    }
    if(fude_zoom_page_lassoed_texts(_page) > 0u) {
        fude_zoom_page_handwrite_selection(_page);   // Typed texts held: it is Handwriting.
        return;
    }
    fude_zoom_scene*      _s    = &_page->scene;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _died = rde_arr_new(sizeof(u32), _heap), _born = rde_arr_new(sizeof(u32), _heap);
    rde_arr _pts  = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr _strokes = rde_arr_new(sizeof(u32), _heap);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        fude_zoom_page_take_strokes(_s, ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object, &_strokes, 0);
    }
    const u32  _n    = (u32)rde_arr_length(&_strokes);
    const u32* _took = (const u32*)_strokes.memory;
    fude_zoom_box _box = fude_zoom_box_empty();
    u32 _frame_any = _s->camera.frame;
    // A line at a time: the pieces of one gesture in one frame, in runs (see fude_zoom_page_line_runs).
    rde_arr _done_arr = rde_arr_new(sizeof(u8), _heap), _pieces_arr = rde_arr_new(sizeof(u32), _heap), _runs_arr = rde_arr_new(sizeof(fude_zoom_run), _heap);
    rde_arr_resize(&_done_arr, _n);
    rde_arr_resize(&_pieces_arr, _n);
    rde_arr_resize(&_runs_arr, _n);
    u8*  _done   = (u8*)_done_arr.memory;   // Sized once: they stay put.
    u32* _pieces = (u32*)_pieces_arr.memory;
    fude_zoom_run* _runs = (fude_zoom_run*)_runs_arr.memory;
    rde_arr _t_arr = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _took[_i]);
        if(_done[_i] || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_STROKE || (_o->flags & FUDE_ZOOM_FLAG_MARKER)) {
            continue;
        }
        const fude_zoom_id _gesture = _o->gesture != 0 ? _o->gesture : _o->id;
        const u32          _frame   = _o->frame;
        u32 _count_pieces = 0;
        for(u32 _j = _i; _j < _n; _j++) {
            const fude_zoom_object* _p = fude_zoom_scene_object(_s, _took[_j]);
            if(!_done[_j] && _p->frame == _frame && (_p->gesture != 0 ? _p->gesture : _p->id) == _gesture && (_p->flags & FUDE_ZOOM_FLAG_ALIVE) &&
               _p->kind == FUDE_ZOOM_KIND_STROKE && !(_p->flags & FUDE_ZOOM_FLAG_MARKER)) {
                _done[_j] = 1u;
                _pieces[_count_pieces++] = _took[_j];
            }
        }
        const u32 _run_count = fude_zoom_page_line_runs(_s, _pieces, _count_pieces, _runs);
        for(u32 _r = 0; _r < _run_count; _r++) {
            fude_zoom_page_curve_run(_page, _s, _pieces, _runs, _r, &_pts, &_t_arr, &_died, &_born, &_box, &_frame_any);
        }
    }
    rde_arr_free(&_done_arr);
    rde_arr_free(&_pieces_arr);
    rde_arr_free(&_runs_arr);
    rde_arr_free(&_t_arr);
    if(rde_arr_length(&_born) > 0) {
        fude_zoom_history_push(_s, _frame_any, _box, (const u32*)_died.memory, (u32)rde_arr_length(&_died), (const u32*)_born.memory, (u32)rde_arr_length(&_born));
        fude_zoom_select_clear(&_page->selection);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_born); _i++) {
            rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ ((const u32*)_born.memory)[_i] });
        }
        fude_zoom_select_update(&_page->selection, _s);
    } else {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_TO_CURVE_NONE));   // Nothing made, nothing let go.
    }
    rde_arr_free(&_died); rde_arr_free(&_born); rde_arr_free(&_pts); rde_arr_free(&_strokes);
}

// Writes a PDF's own text inside box _box (home units) into _out (_size bytes, UTF-8): each page's under it, a line
// between pages, trailing spaces and line breaks trimmed. A page's text that does not fit in what is left is left out.
// Returns its length (0: none there).
usize fude_zoom_page_pdf_text(fude_zoom_page* _page, fude_zoom_box _box, c8* _out, usize _size) {
    const fude_zoom_pdfview* _v = &_page->pdf;
    usize _len = 0;
    if(_size > 0) {
        _out[0] = 0;
    }
    for(u32 _i = 0; fude_zoom_pdfview_is_open(_v) && _i < _v->page_count && _len + 2u < _size; _i++) {
        const fude_zoom_box _p = fude_zoom_pdfview_pages(_v)[_i].box;
        const fude_zoom_box _c = { rde_math_max_f64(_p.min_x, _box.min_x), rde_math_max_f64(_p.min_y, _box.min_y), rde_math_min_f64(_p.max_x, _box.max_x), rde_math_min_f64(_p.max_y, _box.max_y) };
        if(!(_c.max_x > _c.min_x && _c.max_y > _c.min_y)) {
            continue;
        }
        // Its top-left.
        const rde_vec_2F _a = fude_zoom_pdfview_to_page(_v, _i, (fude_zoom_v2){ _c.min_x, _c.max_y });
        const rde_vec_2F _b = fude_zoom_pdfview_to_page(_v, _i, (fude_zoom_v2){ _c.max_x, _c.min_y });
        if(_len > 0) {
            _out[_len++] = '\n';
            _out[_len]   = 0;
        }
        const usize _got = fude_pdf_text_in(_v->pdf, _i, _a, (rde_vec_2F){ _b.x - _a.x, _b.y - _a.y }, _out + _len, _size - _len);
        _len += _got < _size - _len ? _got : 0u;
        _out[_len < _size ? _len : _size - 1u] = 0;
    }
    while(_len > 0 && (_out[_len - 1u] == '\n' || _out[_len - 1u] == ' ')) {
        _out[--_len] = 0;
    }
    return _len;
}

// The selection's row: Copy text (the PDF's under the loop, at most 256 KB, to the clipboard).
void fude_zoom_page_copy_pdf_text(fude_zoom_page* _page) {
    if(_page == NULL || !_page->pdf_text_on) {
        return;
    }
    const usize _cap  = 1u << 18;
    rde_arr     _text_arr = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_text_arr, _cap);
    c8*         _text = (c8*)_text_arr.memory;
    if(fude_zoom_page_pdf_text(_page, _page->pdf_text_box, _text, _cap) > 0u) {
        rde_engine_set_clipboard(_text);   // The row says "Copied" on the button (pagemenu.h).
    }
    rde_arr_free(&_text_arr);
}

// Each of the row's buttons hidden unless it applies to what the lasso holds, as fude_zoom_page_selection_faces
// says where it calls this (_faces: the row's, one a button of _row).
RDE_INTERNAL void fude_zoom_page_faces_hidden(fude_zoom_page* _page, const fude_row_def* _row, fude_row_face* _faces) {
    const fude_zoom_scene* _s = &_page->scene;
    b8 _ink = false, _outline = false, _boards = false, _texts = false;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object);
        const u8 _t = _o->channels;
        _ink     = _ink || (_o->kind == FUDE_ZOOM_KIND_STROKE && !(_o->flags & FUDE_ZOOM_FLAG_MARKER)) || _o->kind == FUDE_ZOOM_KIND_FRAME;
        _outline = _outline || _o->kind == FUDE_ZOOM_KIND_STROKE || _o->kind == FUDE_ZOOM_KIND_FILL ||
                   (_o->kind == FUDE_ZOOM_KIND_SHAPE && _t != FUDE_ZOOM_SHAPE_ARROW && _t != FUDE_ZOOM_SHAPE_DIMENSION && _t != FUDE_ZOOM_SHAPE_RADIAL &&
                    _t != FUDE_ZOOM_SHAPE_ANGLE && _t != FUDE_ZOOM_SHAPE_SYMBOL && _t != FUDE_ZOOM_SHAPE_SHEET && _t != FUDE_ZOOM_SHAPE_GUIDE);
        _boards  = _boards || (_o->kind == FUDE_ZOOM_KIND_SHAPE && _t == FUDE_ZOOM_SHAPE_BOARD);
        _texts   = _texts || _o->kind == FUDE_ZOOM_KIND_TEXT;
    }
    u32 _ca, _cb;
    const u32 _read   = fude_row_def_find(_row, fude_zoom_page_read_selection);
    const u32 _offset = fude_row_def_find(_row, fude_zoom_page_offset_selection);
    const u32 _curve  = fude_row_def_find(_row, fude_zoom_page_curve_selection);
    if(_read != FUDE_ROW_NONE) {
        _faces[_read].hidden = !_ink;
    }
    const u32 _maths = fude_row_def_find(_row, fude_zoom_page_formula_read_selection);
    if(_maths != FUDE_ROW_NONE) {
        _faces[_maths].hidden = !_ink;
    }
    const u32 _edit = fude_row_def_find(_row, fude_zoom_page_text_edit_selection);
    if(_edit != FUDE_ROW_NONE) {
        _faces[_edit].hidden = fude_zoom_page_lone_text(_page) == FUDE_ZOOM_NONE;
    }
    const u32 _hand = fude_row_def_find(_row, fude_zoom_page_formula_hand_selection);
    if(_hand != FUDE_ROW_NONE) {
        _faces[_hand].hidden = !fude_zoom_formula_present() || fude_zoom_page_lassoed_formulas(_page) == 0u;
    }
    if(_offset != FUDE_ROW_NONE) {
        _faces[_offset].hidden = !_outline;
    }
    if(_curve != FUDE_ROW_NONE) {
        // Texts: only typed ones (Handwriting), or formulas (Graph); a note alone has its Edit text.
        _texts = fude_zoom_page_lassoed_texts(_page) > 0u || (fude_zoom_formula_present() && fude_zoom_page_lassoed_formulas(_page) > 0u);
        _faces[_curve].hidden = !(_ink || _boards || _texts || fude_zoom_page_lone_symbol(_page) != FUDE_ZOOM_NONE || fude_zoom_page_lassoed_corner(_page, &_ca, &_cb) ||
                                  fude_zoom_page_lone_line(_page) != FUDE_ZOOM_NONE);
    }
    const u32 _chamfer = fude_row_def_find(_row, fude_zoom_page_chamfer_selection);
    if(_chamfer != FUDE_ROW_NONE) {
        _faces[_chamfer].hidden = !fude_zoom_page_lassoed_corner(_page, &_ca, &_cb);
    }
    const u32 _constrain = fude_row_def_find(_row, fude_zoom_page_constrain_selection);
    if(_constrain != FUDE_ROW_NONE) {
        _faces[_constrain].hidden = !fude_zoom_page_lassoed_corner(_page, &_ca, &_cb);
    }
    const u32 _combine = fude_row_def_find(_row, fude_zoom_page_combine_selection);
    if(_combine != FUDE_ROW_NONE) {
        _faces[_combine].hidden = !fude_zoom_page_combine_offered(_page);
    }
    const u32 _play = fude_row_def_find(_row, fude_zoom_page_play_selection);
    if(_play != FUDE_ROW_NONE) {
        // A circuit's, a mechanism's parts, a graph with variables.
        _faces[_play].hidden = (fude_zoom_page_play_what(_page) & 7u) == 0u;
    }
    const u32 _value = fude_row_def_find(_row, fude_zoom_page_value_selection);
    if(_value != FUDE_ROW_NONE) {
        _faces[_value].hidden = fude_zoom_page_value_lone(_page) == FUDE_ZOOM_NONE;
    }
    const u32 _limits = fude_row_def_find(_row, fude_zoom_page_limits_selection);
    if(_limits != FUDE_ROW_NONE) {
        _faces[_limits].hidden = !fude_zoom_product_get()->hobby || (_page->topic != FUDE_ZOOM_TOPIC_ELECTRONICS && _page->topic != FUDE_ZOOM_TOPIC_GENERAL) ||
                                 fude_zoom_page_limits_lone(_page) == FUDE_ZOOM_NONE;
    }
    const u32 _partb = fude_row_def_find(_row, fude_zoom_page_part_selection);
    if(_partb != FUDE_ROW_NONE) {
        _faces[_partb].hidden = !fude_zoom_product_get()->hobby || (_page->topic != FUDE_ZOOM_TOPIC_ELECTRONICS && _page->topic != FUDE_ZOOM_TOPIC_GENERAL) ||
                                !fude_zoom_page_lasso_logic(_page);
    }
    const u32 _inside = fude_row_def_find(_row, fude_zoom_page_inside_selection);
    if(_inside != FUDE_ROW_NONE) {
        // A custom part of a person's own, alone.
        _faces[_inside].hidden = fude_zoom_page_lone_custom(_page) == NULL;
    }
    const u32 _body = fude_row_def_find(_row, fude_zoom_page_body_selection);
    if(_body != FUDE_ROW_NONE) {
        u32 _bt[1];
        // Mechanisms', and General's (a project's app's).
        _faces[_body].hidden = !fude_zoom_product_get()->hobby || (_page->topic != FUDE_ZOOM_TOPIC_MECHANISMS && _page->topic != FUDE_ZOOM_TOPIC_GENERAL) ||
                               fude_zoom_page_body_targets(_page, _bt, 1u) == 0u;
    }
}

// What To curve and Offset say for what the lasso holds (fude_zoom_page_selection_faces's): Extend or Edit text
// for a line or a symbol alone; a board's grain, or a seam's, on Offset; then Fillet for a corner, Graph for
// formulas, Handwriting for typed texts, or Fit parts (into a board, named) for boards.
RDE_INTERNAL void fude_zoom_page_faces_said(fude_zoom_page* _page, const fude_row_def* _row, fude_row_face* _faces) {
    // A diagram's symbol alone: To curve is Edit text; a line alone, Extend.
    const u32 _curve_at = fude_row_def_find(_row, fude_zoom_page_curve_selection);
    if(_curve_at != FUDE_ROW_NONE && fude_zoom_page_lone_line(_page) != FUDE_ZOOM_NONE) {
        rde_snprintf(_faces[_curve_at].label, FUDE_ROW_LABEL, "%s", fude_text(FUDE_TEXT_ZOOM_EXTEND));
        _faces[_curve_at].icon = FUDE_ICON_ARROW_RIGHT;
    }
    if(_curve_at != FUDE_ROW_NONE && fude_zoom_page_lone_symbol(_page) != FUDE_ZOOM_NONE) {
        rde_snprintf(_faces[_curve_at].label, FUDE_ROW_LABEL, "%s", fude_text(FUDE_TEXT_ZOOM_EDIT_TEXT));
        _faces[_curve_at].icon = FUDE_ICON_TEXT;
    }
    // A board alone: Offset says its grain, and turns it.
    const u32 _offset = fude_row_def_find(_row, fude_zoom_page_offset_selection);
    const u32 _lone   = fude_zoom_page_lone_board(_page);
    if(_offset != FUDE_ROW_NONE && _lone != FUDE_ZOOM_NONE) {
        f64 _gn[FUDE_ZOOM_SHAPE_NUMBERS];
        const u32 _gc = fude_zoom_scene_shape_numbers(&_page->scene, _lone, _gn, FUDE_ZOOM_SHAPE_NUMBERS);
        const b8  _across = fude_zoom_board_grain(_gn, _gc) != 0u;
        rde_snprintf(_faces[_offset].label, FUDE_ROW_LABEL, "%s", fude_text(_across ? FUDE_TEXT_ZOOM_GRAIN_ACROSS : FUDE_TEXT_ZOOM_GRAIN_ALONG));
        _faces[_offset].icon = FUDE_ICON_TURN_PAGE;
    } else if(_offset != FUDE_ROW_NONE && fude_zoom_topic_now == FUDE_ZOOM_TOPIC_SEWING) {
        rde_snprintf(_faces[_offset].label, FUDE_ROW_LABEL, "%s", fude_text(FUDE_TEXT_ZOOM_SEAM));
    }
    const u32 _at = fude_row_def_find(_row, fude_zoom_page_curve_selection);
    u32 _la, _lb;
    if(_at != FUDE_ROW_NONE && fude_zoom_page_lassoed_corner(_page, &_la, &_lb)) {
        rde_snprintf(_faces[_at].label, FUDE_ROW_LABEL, "%s", fude_text(FUDE_TEXT_ZOOM_FILLET));
        _faces[_at].icon = FUDE_ICON_PROTRACTOR;
        return;
    }
    u32 _stock = FUDE_ZOOM_NONE;
    const u32 _boards = _at != FUDE_ROW_NONE ? fude_zoom_page_lassoed_boards(_page, &_stock) : 0u;
    if(_at != FUDE_ROW_NONE && _boards == 0u && fude_zoom_formula_present() && fude_zoom_page_lassoed_formulas(_page) > 0u &&
       fude_zoom_page_lassoed_texts(_page) == 0u) {
        rde_snprintf(_faces[_at].label, FUDE_ROW_LABEL, "%s", fude_text(FUDE_TEXT_ZOOM_SYM_X_GRAPH));
        _faces[_at].icon = FUDE_ICON_CHART_LINE;
        return;
    }
    if(_at != FUDE_ROW_NONE && _boards == 0u && fude_zoom_page_lassoed_texts(_page) > 0u) {
        rde_snprintf(_faces[_at].label, FUDE_ROW_LABEL, "%s", fude_text(FUDE_TEXT_ZOOM_HANDWRITE));
        _faces[_at].icon = FUDE_ICON_PEN;
        return;
    }
    if(_at == FUDE_ROW_NONE || _boards == 0u || _stock == FUDE_ZOOM_NONE) {
        return;
    }
    if(_boards == 1u) {
        rde_snprintf(_faces[_at].label, FUDE_ROW_LABEL, "%s", fude_text(FUDE_TEXT_ZOOM_FIT_PARTS));
    } else {
        f64 _mm[3];
        c8 _name[FUDE_ZOOM_BOARD_NAME];
        fude_zoom_page_board_of(_page, _stock, _mm, NULL, _name, sizeof(_name));
        FUDE_TEXTF(_faces[_at].label, FUDE_TEXT_ZOOM_FIT_INTO, FUDE_TS(_name));
    }
    _faces[_at].icon = FUDE_ICON_COLUMNS;
}

// The lasso's row as it shows now (extension.h's selection_faces): which buttons apply to what is held, and what To
// curve and Offset say (fude_zoom_page_faces_hidden, then fude_zoom_page_faces_said, above).
void fude_zoom_page_selection_faces(fude_app* _app, const fude_row_def* _row, fude_row_face* _faces) {
    RDE_UNUSED(_app);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->open) {
        return;
    }
    // Only a PDF's text in the loop (no ink): Copy, as Copy text — and nothing else.
    const u32 _copy = fude_row_def_find(_row, fude_pagemenu_on_copy);
    if(_copy != FUDE_ROW_NONE && _page->pdf_text_on && !fude_zoom_select_any(&_page->selection)) {
        for(u32 _i = 0; _i < _row->count; _i++) {
            _faces[_i].disabled = _i != _copy;
        }
        if(_faces[_copy].label[0] == 0) {   // Not "Copied" just now.
            rde_snprintf(_faces[_copy].label, FUDE_ROW_LABEL, "%s", fude_text(FUDE_TEXT_ZOOM_COPY_TEXT));
        }
        return;
    }
    // Only what applies to what it holds (no To text or To curve on a sheet, say): To text for handwriting, Offset for
    // outlines, lines and fills (a board alone: its grain), To curve for strokes, or, as its face says below, a
    // corner's fillet, a symbol's text, boards' parts, texts' handwriting. The rest apply to anything.
    fude_zoom_page_faces_hidden(_page, _row, _faces);
    fude_zoom_page_faces_said(_page, _row, _faces);
}

// The lasso's row (extension.h's selection row): the core's Cut, Copy and Duplicate, the canvas's own, then Delete.
// Each shows only where it applies (fude_zoom_page_selection_faces).
static const fude_row_button FUDE_ZOOM_SELECTION_BUTTONS[] = {
    FUDE_PAGEMENU_BUTTON_CUT, FUDE_PAGEMENU_BUTTON_COPY, FUDE_PAGEMENU_BUTTON_DUPLICATE,
    // Play: what is held simulated (its circuit, its mechanism, its graphs).
    { FUDE_TEXT_ZOOM_PLAY, FUDE_ICON_PLAY, fude_zoom_page_play_selection, 0, FUDE_ROW_QUIET, false, NULL },
    // Value: a part alone's value (valueform.h).
    { FUDE_TEXT_ZOOM_VALUE, FUDE_ICON_SLIDERS, fude_zoom_page_value_selection, 0, FUDE_ROW_QUIET, false, NULL },
    // Limits: a part alone's limits (limitsform.h).
    { FUDE_TEXT_ZOOM_LIMITS, FUDE_ICON_FLAG, fude_zoom_page_limits_selection, 0, FUDE_ROW_QUIET, false, NULL },
    // Make body: drawings made bodies (props.h).
    { FUDE_TEXT_ZOOM_MAKE_BODY, FUDE_ICON_TARGET, fude_zoom_page_body_selection, 0, FUDE_ROW_QUIET, false, NULL },
    // Make part: a circuit made a part (logic.h).
    { FUDE_TEXT_ZOOM_MAKE_PART, FUDE_ICON_STACK, fude_zoom_page_part_selection, 0, FUDE_ROW_QUIET, false, NULL },
    // Inside: a custom part's template (custom.h).
    { FUDE_TEXT_ZOOM_CUSTOM_INSIDE, FUDE_ICON_FOLDER_OPEN, fude_zoom_page_inside_selection, 0, FUDE_ROW_QUIET, false, NULL },
    // Repeat: in a row, round, mirrored (repeatform.h).
    { FUDE_TEXT_ZOOM_REPEAT, FUDE_ICON_STACK, fude_zoom_page_repeat_selection, 0, FUDE_ROW_QUIET, false, NULL },
    { FUDE_TEXT_ZOOM_READ, FUDE_ICON_TEXT, fude_zoom_page_read_selection, 0, FUDE_ROW_QUIET, false, fude_zoom_read_present },
    // Formula: handwriting read as maths (mathread.h).
    { FUDE_TEXT_ZOOM_INSERT_FORMULA, FUDE_ICON_CALCULATOR, fude_zoom_page_formula_read_selection, 0, FUDE_ROW_QUIET, false, fude_zoom_formula_present },
    // Handwriting: formulas written by hand (handmath.h).
    { FUDE_TEXT_ZOOM_HANDWRITE, FUDE_ICON_PEN, fude_zoom_page_formula_hand_selection, 0, FUDE_ROW_QUIET, false, NULL },
    // Edit text: a text or a note alone, its card.
    { FUDE_TEXT_ZOOM_EDIT_TEXT, FUDE_ICON_TEXT, fude_zoom_page_text_edit_selection, 0, FUDE_ROW_QUIET, false, NULL },
    { FUDE_TEXT_ZOOM_OFFSET, FUDE_ICON_RECTANGLE, fude_zoom_page_offset_selection, 0, FUDE_ROW_QUIET, false, NULL },
    { FUDE_TEXT_ZOOM_TO_CURVE, FUDE_ICON_GUIDED, fude_zoom_page_curve_selection, 0, FUDE_ROW_QUIET, false, NULL },
    // Chamfer: a corner of two lines held.
    { FUDE_TEXT_ZOOM_CHAMFER, FUDE_ICON_TRIANGLE, fude_zoom_page_chamfer_selection, 0, FUDE_ROW_QUIET, false, NULL },
    // Combine: closed shapes held (cut.h).
    { FUDE_TEXT_ZOOM_COMBINE, FUDE_ICON_SHAPES, fude_zoom_page_combine_selection, 0, FUDE_ROW_QUIET, false, NULL },
    // Constrain: two lines held (connect.h).
    { FUDE_TEXT_ZOOM_CONSTRAIN, FUDE_ICON_LOCK, fude_zoom_page_constrain_selection, 0, FUDE_ROW_QUIET, false, NULL },
    { FUDE_TEXT_ZOOM_TO_LAYER, FUDE_ICON_LAYERS, fude_zoom_page_to_layer_selection, 0, FUDE_ROW_QUIET, false, NULL },
    // Keep: kept in My pieces (piece.h).
    { FUDE_TEXT_ZOOM_PIECE_KEEP, FUDE_ICON_FOLDER_ADD, fude_zoom_page_keep_selection, 0, FUDE_ROW_QUIET, false, NULL },
    FUDE_PAGEMENU_BUTTON_DELETE,
};
const fude_row_def FUDE_ZOOM_SELECTION_ROW = FUDE_ROW_DEF(FUDE_ZOOM_SELECTION_BUTTONS);

// --- the page's menu (a long press, a right-click: extension.h's context_row) --------------------------------------
//
// Paste and Select all; Edit and Open link where it opened on a text or a note's link; and Delete where it opened on
// something: what the lasso holds (opened inside its box), else the thing under it as a tap with the Lasso takes it
// (a part, a mechanism's piece, a drawing, a text) with what goes with it (a part's wires: fude_zoom_select_delete).
// One undo step.

// Finds the link of the note under _screen (the one under the press, else the note's first) and writes its address
// into _page->context_link; empty: none.
RDE_INTERNAL void fude_zoom_page_context_find_link(fude_zoom_page* _page, rde_vec_2F _screen) {
    _page->context_link[0] = 0;
    u32 _o = FUDE_ZOOM_NONE;
    const fude_zoom_rich* _rich = NULL;
    f32 _x = 0.0f, _y = 0.0f;
    f64 _k = 1.0;
    c8  _words[FUDE_ZOOM_SYMBOL_TEXT * 8];
    if(!fude_zoom_page_rich_under(_page, _screen, &_o, &_rich, &_x, &_y, &_k, _words, sizeof(_words))) {
        return;
    }
    const fude_zoom_rich_hit* _h = (const fude_zoom_rich_hit*)_rich->hits.memory;
    const u32 _under = fude_zoom_rich_hit_at(_rich, _x, _y, (f32)(FUDE_ZOOM_PAGE_RICH_REACH / _k));
    u32 _link = _under != FUDE_ZOOM_RICH_NONE && _h[_under].kind == FUDE_ZOOM_RICH_HIT_LINK ? _under : FUDE_ZOOM_RICH_NONE;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_rich->hits) && _link == FUDE_ZOOM_RICH_NONE; _i++) {
        _link = _h[_i].kind == FUDE_ZOOM_RICH_HIT_LINK ? _i : FUDE_ZOOM_RICH_NONE;
    }
    if(_link != FUDE_ZOOM_RICH_NONE) {
        fude_zoom_rich_address(_words, _h[_link].at, _h[_link].len, _page->context_link, sizeof(_page->context_link));
    }
}

// Opens the page's menu at _screen: notes whether it opened on what the lasso holds, else what is under it, and the
// link under it.
void fude_zoom_page_context_open(fude_zoom_page* _page, rde_vec_2F _screen) {
    _page->context_held   = fude_zoom_select_any(&_page->selection) && fude_zoom_select_handle_at(&_page->selection, _screen) >= 0;
    _page->context_target = _page->context_held ? FUDE_ZOOM_NONE : fude_zoom_page_under(_page, _screen);
    fude_zoom_page_context_find_link(_page, _screen);
    fude_pagemenu_open_context(&_page->app->ui->page, _screen, _screen);
}

// Open link: the note's link the menu opened on, in the browser.
RDE_INTERNAL void fude_zoom_page_context_link(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    fude_pagemenu_close_context((fude_pagemenu*)_self);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page != NULL && _page->context_link[0] != 0) {
        rde_engine_open_url(_page->context_link);
        _page->context_link[0] = 0;
    }
}

// The page's menu's Delete: what the lasso holds, or the thing it opened on with what goes with it. Not while
// playing or while the lasso is busy.
void fude_zoom_page_context_delete(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    fude_pagemenu_close_context((fude_pagemenu*)_self);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->open || _page->play_on || fude_zoom_select_busy(&_page->selection)) {
        return;
    }
    fude_zoom_scene* _s = &_page->scene;
    const u32 _target = _page->context_target;
    if(_page->context_held) {
        fude_zoom_select_delete(&_page->selection, _s);
    } else if(_target < fude_zoom_scene_object_count(_s) && (fude_zoom_scene_object(_s, _target)->flags & FUDE_ZOOM_FLAG_ALIVE)) {
        fude_zoom_selection _one;
        fude_zoom_select_init(&_one);
        rde_arr_add(&_one.picks, &(fude_zoom_pick){ _target });
        fude_zoom_select_update(&_one, _s);
        fude_zoom_select_delete(&_one, _s);
        fude_zoom_select_destroy(&_one);
        // What the lasso holds that went with it (a wire of the part's) is let go.
        fude_zoom_pick* _p = (fude_zoom_pick*)_page->selection.picks.memory;
        u32 _kept = 0;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
            if(_p[_i].object < fude_zoom_scene_object_count(_s) && (fude_zoom_scene_object(_s, _p[_i].object)->flags & FUDE_ZOOM_FLAG_ALIVE)) {
                _p[_kept++] = _p[_i];
            }
        }
        rde_arr_resize(&_page->selection.picks, _kept);
        fude_zoom_select_update(&_page->selection, _s);
    }
    _page->context_target = FUDE_ZOOM_NONE;
    _page->context_held   = false;
}

// Returns what the page's menu's Edit changes: the thing it opened on (or the one thing the lasso holds, when it opened
// on that) when it is a text (a note, a formula) or a diagram's shape; FUDE_ZOOM_NONE: none.
RDE_INTERNAL u32 fude_zoom_page_context_editable(const fude_zoom_page* _page) {
    const fude_zoom_scene* _s = &_page->scene;
    u32 _o = _page->context_target;
    if(_page->context_held) {
        _o = rde_arr_length(&_page->selection.picks) == 1u ? ((const fude_zoom_pick*)_page->selection.picks.memory)[0].object : FUDE_ZOOM_NONE;
    }
    if(_o >= fude_zoom_scene_object_count(_s)) {
        return FUDE_ZOOM_NONE;
    }
    const fude_zoom_object* _b = fude_zoom_scene_object(_s, _o);
    const b8 _text = _b->kind == FUDE_ZOOM_KIND_TEXT, _symbol = _b->kind == FUDE_ZOOM_KIND_SHAPE && _b->channels == FUDE_ZOOM_SHAPE_SYMBOL;
    return (_b->flags & FUDE_ZOOM_FLAG_ALIVE) && (_text || _symbol) ? _o : FUDE_ZOOM_NONE;
}

// The page's menu's Edit: the text's card or the symbol's text.
RDE_INTERNAL void fude_zoom_page_context_edit(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    fude_pagemenu_close_context((fude_pagemenu*)_self);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->open || _page->play_on) {
        return;
    }
    const u32 _o = fude_zoom_page_context_editable(_page);
    if(_o != FUDE_ZOOM_NONE) {
        if(fude_zoom_scene_object(&_page->scene, _o)->kind == FUDE_ZOOM_KIND_TEXT) {
            fude_zoom_page_text_edit(_page, _o);
        } else {
            fude_zoom_page_symbol_edit(_page, _o);
        }
    }
    _page->context_target = FUDE_ZOOM_NONE;
    _page->context_held   = false;
}

// The page's menu (extension.h's context_row): Paste, Select all, Edit, Open link, Inside, Delete.
static const fude_row_button FUDE_ZOOM_CONTEXT_BUTTONS[] = {
    FUDE_PAGEMENU_BUTTON_PASTE, FUDE_PAGEMENU_BUTTON_SELECT_ALL,
    // Edit: a text, a note, a diagram's shape.
    { FUDE_TEXT_ZOOM_CUSTOM_EDIT, FUDE_ICON_TEXT, fude_zoom_page_context_edit, 0, FUDE_ROW_QUIET, false, NULL },
    // Open link: a note's link, in the browser.
    { FUDE_TEXT_ZOOM_OPEN_LINK, FUDE_ICON_ARROW_RIGHT, fude_zoom_page_context_link, 0, FUDE_ROW_QUIET, false, NULL },
    // Inside: a custom part's template.
    { FUDE_TEXT_ZOOM_CUSTOM_INSIDE, FUDE_ICON_FOLDER_OPEN, fude_zoom_page_context_inside, 0, FUDE_ROW_QUIET, false, NULL },
    { FUDE_TEXT_DELETE, FUDE_ICON_TRASH, fude_zoom_page_context_delete, 0, FUDE_ROW_DANGER, false, NULL },
};
const fude_row_def FUDE_ZOOM_CONTEXT_ROW = FUDE_ROW_DEF(FUDE_ZOOM_CONTEXT_BUTTONS);

// The page's menu as it shows now: each button only where it applies.
void fude_zoom_page_context_faces(fude_app* _app, const fude_row_def* _row, fude_row_face* _faces) {
    RDE_UNUSED(_app);
    const fude_zoom_page* _page = fude_zoom_page_the;
    const u32 _delete = fude_row_def_find(_row, fude_zoom_page_context_delete);
    if(_delete != FUDE_ROW_NONE) {
        _faces[_delete].hidden = _page == NULL || (!_page->context_held && _page->context_target == FUDE_ZOOM_NONE);
    }
    const u32 _inside = fude_row_def_find(_row, fude_zoom_page_context_inside);
    if(_inside != FUDE_ROW_NONE) {
        _faces[_inside].hidden = _page == NULL || !_page->open || fude_zoom_page_context_custom(_page) == NULL;
    }
    const u32 _edit = fude_row_def_find(_row, fude_zoom_page_context_edit);
    if(_edit != FUDE_ROW_NONE) {
        _faces[_edit].hidden = _page == NULL || !_page->open || fude_zoom_page_context_editable(_page) == FUDE_ZOOM_NONE;
    }
    const u32 _link = fude_row_def_find(_row, fude_zoom_page_context_link);
    if(_link != FUDE_ROW_NONE) {
        _faces[_link].hidden = _page == NULL || !_page->open || _page->context_link[0] == 0;
    }
}
