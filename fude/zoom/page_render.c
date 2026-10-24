// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"
#include "drawing/base/redraw.h"

// ===========================================================================
// Page, drawing: the paper, the page drawn each frame (fude_zoom_page_render), the marks' arrows and their
// previews, the brush, the measuring tape and the instruments.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// The marks' arrows (fude_zoom_page_render_marks).
#define FUDE_ZOOM_PAGE_MARK_INSET 34.0f   // the marks' arrows: in from the screen's edge (screen units)
#define FUDE_ZOOM_PAGE_MARK_REST  0.2     // seconds the camera rests before they are looked for

// --- drawing ------------------------------------------------------------------------------------

#define FUDE_ZOOM_PAGE_STAFF_ENDLESS 10.0   // music paper's staves' lines apart on the endless canvas (its units)

// The page's own papers' lines (paper.h), made into _page->paper_lines (cleared first).
rde_arr* fude_zoom_page_paper_lines(fude_zoom_page* _page) {
    if(!rde_arr_is_inited(&_page->paper_lines)) {
        _page->paper_lines = rde_arr_new(sizeof(fude_zoom_paper_line), rde_memory_allocator_get_default_std());
    }
    rde_arr_clear(&_page->paper_lines);
    return &_page->paper_lines;
}

#define FUDE_ZOOM_PAGE_PAPER_RUN 512u   // a stroke's points at most: a longer run of lines goes on in another stroke

// The paper's lines _lines drawn: their units onto the screen through _to, in the paper's ink _ink as strongly as each
// says (heavier ones wider), blended over the page's colour so each is solid; those that go on from the one before (a
// ring's chords) one stroke, so their joins are not drawn twice (a ring dashed).
void fude_zoom_page_draw_paper_lines(const rde_arr* _lines, fude_zoom_sim _to, rde_color _ink) {
    const fude_zoom_paper_line* _l = (const fude_zoom_paper_line*)_lines->memory;
    const u32 _n = (u32)rde_arr_length(_lines);
    const rde_color _paper = fude_theme_active()->page;   // (under it: its ink laid on it, solid — a stroke's joins not darker)
    rde_vec_2F _run[FUDE_ZOOM_PAGE_PAPER_RUN];
    u32 _count = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        const f32 _k = (f32)_ink.a / 255.0f * _l[_i].strength;
        const rde_color _c = { (u8)((f32)_paper.r + ((f32)_ink.r - (f32)_paper.r) * _k), (u8)((f32)_paper.g + ((f32)_ink.g - (f32)_paper.g) * _k),
                               (u8)((f32)_paper.b + ((f32)_ink.b - (f32)_paper.b) * _k), (u8)(_k > 0.0f ? 255u : 0u) };
        const fude_zoom_v2 _a = fude_zoom_sim_apply(_to, _l[_i].a), _b = fude_zoom_sim_apply(_to, _l[_i].b);
        if(_count == 0u) {
            _run[_count++] = (rde_vec_2F){ (f32)_a.x, (f32)_a.y };
        }
        _run[_count++] = (rde_vec_2F){ (f32)_b.x, (f32)_b.y };
        // (the next one going on from this one's end, as strong: the same stroke)
        const b8 _goes_on = _i + 1u < _n && _count < FUDE_ZOOM_PAGE_PAPER_RUN && _l[_i + 1u].strength == _l[_i].strength && _l[_i + 1u].heavy == _l[_i].heavy &&
                            rde_math_abs_f64(_l[_i + 1u].a.x - _l[_i].b.x) + rde_math_abs_f64(_l[_i + 1u].a.y - _l[_i].b.y) <= 1e-9 * (rde_math_abs_f64(_l[_i].b.x) + rde_math_abs_f64(_l[_i].b.y) + 1.0);
        if(!_goes_on) {
            if(_c.a > 0u) {
                fude_draw_stroke_even(_run, _count, _l[_i].heavy ? 1.2f : _l[_i].strength < 0.5f ? 0.5f : 0.75f, _c);   // (faint ones finer)
            }
            _count = 0;
        }
    }
}

#define FUDE_ZOOM_PAGE_PAPER_OUT 10.0   // a notebook's paper: its lines fade out closer than these points apart...
#define FUDE_ZOOM_PAGE_PAPER_IN  40.0   // ...and finer ones come in past twice these
#define FUDE_ZOOM_PAGE_PAPER_MOST 4000.0   // a page's lines (or the endless paper's) one way at most: more are not drawn

// A notebook's pages (pages.h), under the ink: the desk round them a shade off the paper; each page on screen the
// paper's colour, a soft shadow under it, its number at its foot, and its paper — ruled (its margin line too), squared
// or dotted, or one of paper.h's — at its true spacing over the page alone, finer lines coming in between as the view
// goes in, fading out far out.
RDE_INTERNAL void fude_zoom_page_render_pages(fude_zoom_page* _page, fude_zoom_v2 _half) {
    const fude_zoom_pages* _p = &_page->pages;
    const fude_theme*      _t = fude_theme_active();
    rde_color _desk = _t->page;
    const b8  _dark = (i32)_t->page.r + (i32)_t->page.g + (i32)_t->page.b < 384;
    _desk.r = (u8)(_dark ? rde_math_min_f64(_t->page.r + 14.0, 255.0) : rde_math_max_f64(_t->page.r - 22.0, 0.0));
    _desk.g = (u8)(_dark ? rde_math_min_f64(_t->page.g + 14.0, 255.0) : rde_math_max_f64(_t->page.g - 22.0, 0.0));
    _desk.b = (u8)(_dark ? rde_math_min_f64(_t->page.b + 14.0, 255.0) : rde_math_max_f64(_t->page.b - 18.0, 0.0));
    rde_rendering_2d_draw_rectangle((rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ (f32)(_half.x * 2.0 + 4.0), (f32)(_half.y * 2.0 + 4.0) }, _desk);
    const rde_color _shadow = { (u8)(_desk.r * 0.72), (u8)(_desk.g * 0.72), (u8)(_desk.b * 0.72), 255 };   // (the desk's own, darker: opaque)
    const fude_zoom_sim _home = fude_zoom_page_frame_sim(_page, _page->scene.home);   // a millimetre's place on screen
    const f64 _z = fude_zoom_sim_scale(_home);
    if(!(_z > 0.0) || !rde_math_is_finite_f64(_z)) {
        return;
    }
    const fude_zoom_box _view = fude_zoom_sim_box(fude_zoom_sim_inverse(_home), (fude_zoom_box){ -_half.x, -_half.y, _half.x, _half.y });
    const u32 _first = fude_zoom_pages_at(_p, (fude_zoom_v2){ 0.0, _view.max_y }), _last = fude_zoom_pages_at(_p, (fude_zoom_v2){ 0.0, _view.min_y });
    const FUDE_PAPER_ _paper = (FUDE_PAPER_)_page->app->canvas->page.paper;
    const f64 _true = _paper == FUDE_PAPER_LINES || fude_zoom_paper_is_page(_paper) ? FUDE_ZOOM_PAGES_LINES : FUDE_ZOOM_PAGES_GRID;
    f64 _fade = 1.0;
    const f64 _s = fude_zoom_pages_spacing(_true, _z, FUDE_ZOOM_PAGE_PAPER_OUT, FUDE_ZOOM_PAGE_PAPER_IN, &_fade);
    for(u32 _i = _first; _i <= _last && _i < _p->count; _i++) {
        const fude_zoom_box _b  = fude_zoom_pages_box(_p, _i);
        const fude_zoom_v2  _lo = fude_zoom_sim_apply(_home, (fude_zoom_v2){ _b.min_x, _b.min_y });
        const fude_zoom_v2  _hi = fude_zoom_sim_apply(_home, (fude_zoom_v2){ _b.max_x, _b.max_y });
        // (on the screen, to a little past it: a page far bigger than the screen drawn as its part on it)
        const f64 _x0 = rde_math_max_f64(_lo.x, -_half.x - 8.0), _x1 = rde_math_min_f64(_hi.x, _half.x + 8.0), _y0 = rde_math_max_f64(_lo.y, -_half.y - 8.0), _y1 = rde_math_min_f64(_hi.y, _half.y + 8.0);
        if(_x1 <= _x0 || _y1 <= _y0) {
            continue;
        }
        const rde_vec_2F _mid  = { (f32)((_x0 + _x1) * 0.5), (f32)((_y0 + _y1) * 0.5) };
        const rde_vec_2F _size = { (f32)(_x1 - _x0), (f32)(_y1 - _y0) };
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ _mid.x + 2.0f, _mid.y - 3.0f }, _size, _shadow);
        rde_rendering_2d_draw_rectangle(_mid, _size, _t->page);
        // Its number at its foot, softly (once the page is big enough on screen to read one).
        if(_hi.x - _lo.x >= 90.0 && _lo.y >= -_half.y - 4.0 && _page->renderer.font != NULL) {
            c8 _num[16];
            rde_snprintf(_num, sizeof(_num), "%u", _i + 1u);
            const f32 _px = (f32)rde_math_min_f64(rde_math_max_f64((_hi.x - _lo.x) * 0.03, 9.0), 14.0);
            const f32 _nw = fude_draw_text_width(_page->renderer.font, _page->renderer.font_px, _num, _px);
            rde_color _soft = _t->text_soft;
            _soft.a = (u8)((u32)_soft.a * 70u / 100u);
            fude_draw_text_placed(_page->renderer.font, _num, (rde_vec_3F){ (f32)((_lo.x + _hi.x) * 0.5) - _nw * 0.5f, (f32)_lo.y + _px * 0.9f, 0.0f },
                                  (rde_vec_2F){ _px / _page->renderer.font_px, _px / _page->renderer.font_px }, 0.0f, _soft);
        }
        if(_paper == FUDE_PAPER_NONE || _fade <= 0.0 || _z * _s < 1e-9 || _z > 1e12) {
            continue;   // (blank; too far out to see; too deep for a millimetre's lines to be placed: the page alone)
        }
        if(_paper >= FUDE_PAPER_CORE) {
            // The page's own (paper.h): a page's layout at its true size; a pattern round the page's middle (music's
            // staves from under its top margin), its finer lines coming in as the view goes in, all fading far out.
            rde_arr*  _lines = fude_zoom_page_paper_lines(_page);
            rde_color _ink   = _t->page_dots;
            const b8  _finer = _s < _true;
            _ink.a = (u8)((f64)_ink.a * (_finer ? 1.0 : _fade));
            if(fude_zoom_paper_is_page(_paper)) {
                fude_zoom_paper_page(_paper, _b, _b.max_y - FUDE_ZOOM_PAGES_MARGIN, FUDE_ZOOM_PAGES_LINES, _lines);
                fude_zoom_page_draw_paper_lines(_lines, _home, _ink);
                continue;
            }
            const fude_zoom_v2 _o = _paper == FUDE_PAPER_MUSIC ? (fude_zoom_v2){ _b.min_x, _b.max_y - FUDE_ZOOM_PAGES_MARGIN }
                                                               : (fude_zoom_v2){ (_b.min_x + _b.max_x) * 0.5, (_b.min_y + _b.max_y) * 0.5 };
            const fude_zoom_box _on = { rde_math_max_f64(_view.min_x, _b.min_x) - _o.x, rde_math_max_f64(_view.min_y, _b.min_y) - _o.y,
                                        rde_math_min_f64(_view.max_x, _b.max_x) - _o.x, rde_math_min_f64(_view.max_y, _paper == FUDE_PAPER_MUSIC ? _o.y : _b.max_y) - _o.y };
            fude_zoom_paper_lines(_paper, _on, _paper == FUDE_PAPER_MUSIC ? FUDE_ZOOM_PAGE_STAFF : (_finer ? 2.0 * _s : _s), _finer ? _fade : 0.0, _z, _lines);
            fude_zoom_page_draw_paper_lines(_lines, fude_zoom_sim_compose(_home, (fude_zoom_sim){ 1.0, 0.0, _o.x, _o.y }), _ink);
            continue;
        }
        // Its lines: from its top (ruled: below the margin) and its left, those on screen — each one's strength: the
        // paper's own lines full, the finer ones between them as far as they have come in (or all faded, far out).
        const f64 _top  = _b.max_y - (_paper == FUDE_PAPER_LINES ? FUDE_ZOOM_PAGES_MARGIN : 0.0);
        const f64 _left = _b.min_x;
        rde_color _ink = _t->page_dots;
        const f64 _a0 = (f64)_ink.a;
        const f64 _vy0 = rde_math_max_f64(_view.min_y, _b.min_y), _vy1 = rde_math_min_f64(_view.max_y, _top);
        const f64 _vx0 = rde_math_max_f64(_view.min_x, _b.min_x), _vx1 = rde_math_min_f64(_view.max_x, _b.max_x);
        const f64 _k0 = rde_math_ceil_f64((_top - _vy1) / _s), _k1 = rde_math_floor_f64((_top - _vy0) / _s);
        const f64 _j0 = rde_math_ceil_f64((_vx0 - _left) / _s), _j1 = rde_math_floor_f64((_vx1 - _left) / _s);
        if(_k1 - _k0 > FUDE_ZOOM_PAGE_PAPER_MOST || _j1 - _j0 > FUDE_ZOOM_PAGE_PAPER_MOST) {
            continue;
        }
        const f64 _ratio = _true / _s;   // (how many of the finest to one of its own)
        const f64 _edge  = 1e-9 * _true;   // (a line on the page's own edge: left out)
        // Rows and columns walked by index from the first (paper.h's fude_zoom_paper_count: never by an f64 counter,
        // which would stop moving past 2^53).
        const f64 _kf = rde_math_max_f64(_k0, _paper == FUDE_PAPER_LINES ? 0.0 : 1.0), _jf = rde_math_max_f64(_j0, 1.0);
        const u32 _rows = fude_zoom_paper_count(_kf, _k1, FUDE_ZOOM_PAGE_PAPER_MOST), _cols = fude_zoom_paper_count(_jf, _j1, FUDE_ZOOM_PAGE_PAPER_MOST);
        for(u32 _r = 0; _r < _rows; _r++) {
            const f64 _k = _kf + (f64)_r;
            const f64 _y = _top - _k * _s;
            if(_y <= _b.min_y + _edge) {
                break;
            }
            const b8  _own = rde_math_fmod_f64(_k, _ratio) == 0.0;
            const f64 _row = _s < _true ? (_own ? 1.0 : _fade) : _fade;
            const fude_zoom_v2 _ya = fude_zoom_sim_apply(_home, (fude_zoom_v2){ _left, _y });
            if(_paper == FUDE_PAPER_DOTS) {
                // (a dot full where its row and its column are the paper's own; else as far as the
                // finer ones have come)
                for(u32 _c = 0; _c < _cols; _c++) {
                    const f64 _j = _jf + (f64)_c;
                    if(!(_left + _j * _s < _b.max_x - _edge)) {
                        break;
                    }
                    const f64 _col = _s < _true ? (rde_math_fmod_f64(_j, _ratio) == 0.0 ? 1.0 : _fade) : _fade;
                    _ink.a = (u8)(_a0 * rde_math_min_f64(_row, _col));
                    if(_ink.a == 0u) {
                        continue;
                    }
                    const fude_zoom_v2 _d = fude_zoom_sim_apply(_home, (fude_zoom_v2){ _left + _j * _s, _y });
                    rde_rendering_2d_draw_rectangle((rde_vec_2F){ (f32)_d.x, (f32)_d.y }, (rde_vec_2F){ FUDE_CANVAS_GRID_DOT, FUDE_CANVAS_GRID_DOT }, _ink);
                }
                continue;
            }
            _ink.a = (u8)(_a0 * _row);
            if(_ink.a > 0u) {
                fude_draw_line((rde_vec_2F){ (f32)_x0, (f32)_ya.y }, (rde_vec_2F){ (f32)_x1, (f32)_ya.y }, 0.6f, _ink);
            }
        }
        if(_paper == FUDE_PAPER_SQUARES) {
            for(u32 _c = 0; _c < _cols; _c++) {
                const f64 _j = _jf + (f64)_c;
                if(!(_left + _j * _s < _b.max_x - _edge)) {
                    break;
                }
                const b8 _own = rde_math_fmod_f64(_j, _ratio) == 0.0;
                _ink.a = (u8)(_a0 * (_s < _true ? (_own ? 1.0 : _fade) : _fade));
                const fude_zoom_v2 _xa = fude_zoom_sim_apply(_home, (fude_zoom_v2){ _left + _j * _s, _b.max_y });
                if(_ink.a > 0u) {
                    fude_draw_line((rde_vec_2F){ (f32)_xa.x, (f32)_y0 }, (rde_vec_2F){ (f32)_xa.x, (f32)_y1 }, 0.6f, _ink);
                }
            }
        }
        if(_paper == FUDE_PAPER_LINES) {
            // (its margin line, a faint red, down the page from its top)
            const fude_zoom_v2 _m = fude_zoom_sim_apply(_home, (fude_zoom_v2){ _left + FUDE_ZOOM_PAGES_MARGIN, _b.max_y });
            if(_m.x > _x0 && _m.x < _x1) {
                fude_draw_line((rde_vec_2F){ (f32)_m.x, (f32)_y0 }, (rde_vec_2F){ (f32)_m.x, (f32)_y1 }, 0.6f, (rde_color){ 214, 96, 96, (u8)(110.0 * rde_math_min_f64(_fade * 4.0, 1.0)) });
            }
        }
    }
}

// The paper, in the camera frame: dots, ruled lines or plain squares (Paper on
// the toolbar; how dense, Settings' Lines & squares size). Its lattice is 50·2^n
// frame units — the same at every depth, frames' origins sit on it (scene.h:
// FUDE_ZOOM_PAPER_ALIGN), so it runs on through a change of frame — the n that
// puts its spacing on screen between _min and twice that, and the lattice half
// as fine fading in over it as the zoom grows, so it never jumps. A notebook
// draws its pages instead (fude_zoom_page_render_pages); paper.h's own papers
// are laid round the camera frame's middle; a page's layout is ruled lines here.
RDE_INTERNAL void fude_zoom_page_render_paper(fude_zoom_page* _page, rde_window* _window) {
    fude_app*               _app   = _page->app;
    if(fude_zoom_page_paged(_page)) {
        fude_zoom_page_render_pages(_page, fude_zoom_page_half(_page));   // (a notebook: its pages' paper, not the endless lattice)
        return;
    }
    const fude_zoom_camera* _c     = &_page->scene.camera;
    FUDE_PAPER_             _paper = (FUDE_PAPER_)_app->canvas->page.paper;
    if(fude_zoom_paper_is_page(_paper)) {
        _paper = FUDE_PAPER_LINES;   // (a page's layout — Cornell's, a planner's —: ruled lines on the endless canvas)
    }
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
    if(_paper >= FUDE_PAPER_CORE) {
        // The page's own (paper.h), round the camera frame's middle: its lattice as the squares', finer coming in.
        rde_arr* _lines = fude_zoom_page_paper_lines(_page);
        fude_zoom_paper_lines(_paper, _view, _paper == FUDE_PAPER_MUSIC ? FUDE_ZOOM_PAGE_STAFF_ENDLESS : _s, rde_math_min_f64(rde_math_max_f64(_fade, 0.0), 1.0), _c->z, _lines);
        fude_zoom_page_draw_paper_lines(_lines, (fude_zoom_sim){ _c->z, 0.0, -_c->at.x * _c->z, -_c->at.y * _c->z }, _ink);
        return;
    }
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
        const f64 _x0 = rde_math_floor_f64(_view.min_x / _step), _x1 = rde_math_ceil_f64(_view.max_x / _step);
        const f64 _y0 = rde_math_floor_f64(_view.min_y / _step), _y1 = rde_math_ceil_f64(_view.max_y / _step);
        // Its lines counted, then walked by index (paper.h's fude_zoom_paper_count): a view 2^53 steps or more from 0
        // has none, so every k is a whole number under 2^53, which an i64 holds (its parity below) and an f64 counter
        // would not have moved past.
        const u32 _nx = fude_zoom_paper_count(_x0, _x1, FUDE_ZOOM_PAGE_PAPER_MOST), _ny = fude_zoom_paper_count(_y0, _y1, FUDE_ZOOM_PAGE_PAPER_MOST);
        // On screen: (k·step − camera)·z, in f64 — k·step is exact for any k a double holds.
        if(_paper == FUDE_PAPER_DOTS) {
            for(u32 _iy = 0; _iy < _ny; _iy++) {
                const f64 _ky = _y0 + (f64)_iy;
                for(u32 _ix = 0; _ix < _nx; _ix++) {
                    const f64 _kx = _x0 + (f64)_ix;
                    if(_level == 1 && (((i64)_kx) & 1) == 0 && (((i64)_ky) & 1) == 0) {
                        continue;   // the coarse lattice's own dot
                    }
                    const rde_vec_2F _p = { (f32)((_kx * _step - _c->at.x) * _c->z), (f32)((_ky * _step - _c->at.y) * _c->z) };
                    rde_rendering_2d_draw_rectangle(_p, (rde_vec_2F){ FUDE_CANVAS_GRID_DOT, FUDE_CANVAS_GRID_DOT }, _color);
                }
            }
            continue;
        }
        for(u32 _iy = 0; _iy < _ny; _iy++) {
            const f64 _ky = _y0 + (f64)_iy;
            if(_level == 1 && (((i64)_ky) & 1) == 0) {
                continue;
            }
            const f32 _y = (f32)((_ky * _step - _c->at.y) * _c->z);
            fude_draw_line((rde_vec_2F){ (f32)-_hw, _y }, (rde_vec_2F){ (f32)_hw, _y }, 0.6f, _color);
        }
        if(_paper == FUDE_PAPER_SQUARES) {
            for(u32 _ix = 0; _ix < _nx; _ix++) {
                const f64 _kx = _x0 + (f64)_ix;
                if(_level == 1 && (((i64)_kx) & 1) == 0) {
                    continue;
                }
                const f32 _x = (f32)((_kx * _step - _c->at.x) * _c->z);
                fude_draw_line((rde_vec_2F){ _x, (f32)-_hh }, (rde_vec_2F){ _x, (f32)_hh }, 0.6f, _color);
            }
        }
    }
}

// --- the arrows' previews ---------------------------------------------------------------------

// Each arrow a picture: the one already drawn of what it points at (the same frame and box, the
// canvas as it was), else one not wanted by another arrow, to draw.
RDE_INTERNAL void fude_zoom_page_previews_assign(fude_zoom_page* _page) {
    b8 _used[FUDE_ZOOM_PAGE_PREVIEWS];
    rde_memset(_used, 0, sizeof(_used));
    for(u32 _i = 0; _i < _page->mark_count; _i++) {
        const fude_zoom_nav_mark* _m = &_page->marks[_i];
        _page->mark_preview[_i] = -1;
        if(_m->ring) {
            continue;
        }
        for(u32 _k = 0; _k < FUDE_ZOOM_PAGE_PREVIEWS; _k++) {
            if(!_used[_k] && _page->preview_frame[_k] == _m->frame && rde_memcmp(&_page->preview_box[_k], &_m->box, sizeof(fude_zoom_box)) == 0) {
                _page->mark_preview[_i] = (i8)_k;
                _used[_k] = true;
                if(_page->preview_rev[_k] != _page->scene.revision) {
                    _page->preview_ready[_k] = false;   // (changed since: drawn again)
                }
                break;
            }
        }
    }
    for(u32 _i = 0; _i < _page->mark_count; _i++) {
        if(_page->marks[_i].ring || _page->mark_preview[_i] >= 0) {
            continue;
        }
        for(u32 _k = 0; _k < FUDE_ZOOM_PAGE_PREVIEWS; _k++) {
            if(!_used[_k]) {
                _used[_k] = true;
                _page->mark_preview[_i]   = (i8)_k;
                _page->preview_frame[_k]  = _page->marks[_i].frame;
                _page->preview_box[_k]    = _page->marks[_i].box;
                _page->preview_ready[_k]  = false;
                break;
            }
        }
    }
}

// Where arrow _m's preview goes: inside it, toward the middle, all of it on the screen.
b8 fude_zoom_page_preview_at(const fude_zoom_page* _page, const fude_zoom_nav_mark* _m, fude_zoom_v2 _half, rde_vec_2F* _out) {
    RDE_UNUSED(_page);
    const f32 _cx = rde_math_cos_f32(_m->angle), _cy = rde_math_sin_f32(_m->angle);
    const f32 _off = 17.0f + 10.0f + rde_math_abs_f32(_cx) * (FUDE_ZOOM_PAGE_PREVIEW_W * 0.5f + 4.0f) + rde_math_abs_f32(_cy) * (FUDE_ZOOM_PAGE_PREVIEW_H * 0.5f + 4.0f);
    rde_vec_2F _c = { _m->at.x - _cx * _off, _m->at.y - _cy * _off };
    const f32 _hx = (f32)_half.x - FUDE_ZOOM_PAGE_PREVIEW_W * 0.5f - 8.0f, _hy = (f32)_half.y - FUDE_ZOOM_PAGE_PREVIEW_H * 0.5f - 8.0f;
    if(_hx <= 0.0f || _hy <= 0.0f) {
        return false;
    }
    _c.x = rde_math_min_f32(rde_math_max_f32(_c.x, -_hx), _hx);
    _c.y = rde_math_min_f32(rde_math_max_f32(_c.y, -_hy), _hy);
    *_out = _c;
    return true;
}

// Once a frame (before the page is drawn): the next arrow's preview not drawn yet, drawn —
// what it points at, framed in a picture of its own (its frame's camera for the one draw,
// the page's put back after).
void fude_zoom_page_previews_draw(fude_zoom_page* _page, rde_window* _window, rde_camera* _camera) {
    if(!_page->open || _page->mark_count == 0) {
        return;
    }
    for(u32 _i = 0; _i < _page->mark_count; _i++) {
        const i8 _k = _page->mark_preview[_i];
        if(_k < 0 || _page->preview_ready[_k]) {
            continue;
        }
        const u32 _pw = (u32)(FUDE_ZOOM_PAGE_PREVIEW_W * FUDE_ZOOM_PAGE_PREVIEW_PX), _ph = (u32)(FUDE_ZOOM_PAGE_PREVIEW_H * FUDE_ZOOM_PAGE_PREVIEW_PX);
        if(_page->previews[_k] == NULL) {
            _page->previews[_k] = rde_render_texture_load(_pw, _ph, NULL);
            if(_page->previews[_k] == NULL) {
                return;
            }
        }
        fude_zoom_scene*       _s    = &_page->scene;
        const fude_zoom_camera _was  = _s->camera;
        const fude_zoom_v2     _half = { (f64)_pw * 0.5, (f64)_ph * 0.5 };
        const rde_color        _bg   = fude_theme_active()->page;
        _s->camera = fude_zoom_nav_framing(_half, _page->preview_frame[_k], _page->preview_box[_k], 0.84);   // (drawn only: never settled like this)
        rde_render_texture_enable(_page->previews[_k]);
        rde_rendering_2d_begin_drawing(_window, _camera);
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ (f32)_pw + 4.0f, (f32)_ph + 4.0f }, _bg);
        _page->renderer.no_guides = true;   // (a picture of the page: no guides)
        fude_zoom_render(&_page->renderer, _s, _half, _bg);
        _page->renderer.no_guides = false;
        rde_rendering_2d_end_drawing();
        rde_render_texture_disable();
        _s->camera = _was;
        _page->preview_ready[_k] = true;
        _page->preview_rev[_k]   = _s->revision;
        fude_redraw_changed();   // (render on demand: the next, if any, in the next frame)
        return;   // (one a frame)
    }
}

// The pictures let go (the canvas closing; the app in the background).
void fude_zoom_page_previews_free(fude_zoom_page* _page) {
    for(u32 _k = 0; _k < FUDE_ZOOM_PAGE_PREVIEWS; _k++) {
        if(_page->previews[_k] != NULL) {
            rde_render_texture_unload(_page->previews[_k]);
            _page->previews[_k] = NULL;
        }
        _page->preview_ready[_k] = false;
        _page->preview_frame[_k] = FUDE_ZOOM_NONE;
    }
    for(u32 _i = 0; _i < FUDE_ZOOM_NAV_MARKS; _i++) {
        _page->mark_preview[_i] = -1;
    }
}

// Nothing on screen: arrows at its edge to the nearest things drawn, rings round
// what is there but too small to see (nav.h) — found again once the camera has
// moved and rested a moment (FUDE_ZOOM_PAGE_MARK_REST), not while drawing,
// erasing, flying or holding a selection. Each arrow with its preview, once drawn.
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
    } else if(_page->mark_count == 0 && _now - _page->marks_at < FUDE_ZOOM_PAGE_MARK_REST && _page->marks_at > 0.0) {
        fude_redraw_after(FUDE_ZOOM_PAGE_MARK_REST - (_now - _page->marks_at));   // (render on demand: found once at rest)
    } else if(_page->mark_count == 0 && _now - _page->marks_at >= FUDE_ZOOM_PAGE_MARK_REST && _page->marks_at > 0.0) {
        _page->mark_count = fude_zoom_nav_marks(&_page->scene, _half, FUDE_ZOOM_PAGE_MARK_INSET, _page->marks);
        _page->marks_at   = 0.0;   // found: until the camera moves again
        fude_zoom_page_previews_assign(_page);
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
                _m->at.x -= rde_math_cos_f32(_m->angle) * 8.0f;
                _m->at.y -= rde_math_sin_f32(_m->angle) * 8.0f;
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
        // Its preview, inside it (once drawn): a card with the picture on it.
        rde_vec_2F _pc;
        const i8   _pv = _page->mark_preview[_i];
        if(_pv >= 0 && _page->preview_ready[_pv] && _page->previews[_pv] != NULL && fude_zoom_page_preview_at(_page, _m, _half, &_pc)) {
            const rde_texture* _tex = rde_render_texture_get_texture(_page->previews[_pv]);
            rde_rendering_2d_draw_rounded_rectangle_with_border(_pc, (rde_vec_2F){ FUDE_ZOOM_PAGE_PREVIEW_W + 8.0f, FUDE_ZOOM_PAGE_PREVIEW_H + 8.0f }, 0.2f, 8u,
                                                                (rde_color){ 0, 0, 0, 0 }, 1.0f, _t->outline, NULL);
            if(_tex != NULL) {
                rde_rendering_2d_draw_texture_2(_tex, (rde_vec_3F){ _pc.x, _pc.y, 0.0f }, (rde_vec_2F){ 1.0f / FUDE_ZOOM_PAGE_PREVIEW_PX, 1.0f / FUDE_ZOOM_PAGE_PREVIEW_PX },
                                                0.0f, (rde_color){ 255, 255, 255, 255 });
            }
        }
        // A round card with a chevron pointing out.
        rde_rendering_2d_draw_circle_with_border(_m->at, 17.0f, 28u, _t->surface, 1.0f, _t->outline, NULL);
        const f32 _cx = rde_math_cos_f32(_m->angle), _cy = rde_math_sin_f32(_m->angle);
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
    if(_quiet) {
        fude_redraw_after(_page->hover_quiet - rde_engine_get_time_now());   // (render on demand: the circle comes back then)
    }
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
        const f64 _home = fude_zoom_page_home_units(_page, _page->scene.camera.frame);   // (on the page: the canvas's size)
        _r    = _marking ? (f32)((f64)_ink->marker_radius * _z) : (_ink->brush_scale == FUDE_INK_BRUSH_SCALE_SCREEN ? _radius : (f32)((f64)_radius * _z / _home));
        _ring = fude_theme_resolve(_marking ? _ink->marker_color : _ink->color);
        _fill = _ring;
        _fill.a = _pressure ? 45 : 70;
        _ring.a = 190;
        if(_pressure) {
            const f32 _light = _ink->brush_scale == FUDE_INK_BRUSH_SCALE_SCREEN ? FUDE_INK_WIDTH_BASE : (f32)((f64)FUDE_INK_WIDTH_BASE * _z / _home);
            rde_color _dot = _ring;
            _dot.a = 150;
            rde_rendering_2d_draw_circle(_at, rde_math_max_f32(_light, 1.0f), 20u, _dot, NULL);
        }
    }
    _r = rde_math_max_f32(_r, 2.0f);
    rde_rendering_2d_draw_circle_with_border(_at, _r, _r > 30.0f ? 48u : 28u, _fill, 1.2f, _ring, NULL);
}

// How many millimetres a screen point is now: the canvas's scale (a unit of
// its home frame is FUDE_ZOOM_PAGE_MM_PER_UNIT) at the camera's zoom.
f64 fude_zoom_page_mm_per_point(const fude_zoom_page* _page) {
    const fude_zoom_scene* _s    = &_page->scene;
    const u32              _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    const f64 _units = fude_zoom_sim_scale(fude_zoom_scene_sim(_s, _s->camera.frame, _home));
    return FUDE_ZOOM_PAGE_MM_PER_UNIT * _units / _s->camera.z;
}

// --- dots along a line (the current's, a belt's marks) ----------------------------------------------------------

// Writes into *_t0, *_t1 the part of the screen segment _a to _b (t from 0 to 1) inside the box _half round the middle
// (Liang and Barsky's clipping). False: none of it is.
RDE_INTERNAL b8 fude_zoom_page_segment_on_screen(fude_zoom_v2 _a, fude_zoom_v2 _b, fude_zoom_v2 _half, f64* _t0, f64* _t1) {
    const f64 _d[2]  = { _b.x - _a.x, _b.y - _a.y };
    const f64 _at[2] = { _a.x, _a.y };
    const f64 _hb[2] = { _half.x, _half.y };
    f64 _lo = 0.0, _hi = 1.0;
    for(u32 _axis = 0; _axis < 2u; _axis++) {
        for(u32 _side = 0; _side < 2u; _side++) {
            const f64 _p = _side == 0u ? -_d[_axis] : _d[_axis];
            const f64 _q = _side == 0u ? _at[_axis] + _hb[_axis] : _hb[_axis] - _at[_axis];
            if(_p == 0.0) {
                if(_q < 0.0) {
                    return false;   // (along that edge, outside it)
                }
                continue;
            }
            const f64 _r = _q / _p;
            if(_p < 0.0) {
                _lo = rde_math_max_f64(_lo, _r);
            } else {
                _hi = rde_math_min_f64(_hi, _r);
            }
        }
    }
    *_t0 = _lo;
    *_t1 = _hi;
    return _lo <= _hi;
}

// See page_parts.h. Each segment is clipped to the screen first, and the points on it counted by a whole number from
// the first one there, never walked by adding the step to a length: zoomed in on a line it would take as many steps as
// the line is long on the screen, most of them off it, and past 2^53 steps an f64 length stops moving at all.
u32 fude_zoom_page_dots_along(const fude_zoom_v2* _p, u32 _n, b8 _closed, f64 _from, f64 _to, f64 _start, f64 _step, fude_zoom_sim _to_screen,
                              fude_zoom_v2 _half, u32 _most, fude_zoom_page_dot_fn _dot, any _user) {
    if(_n < 2u || !(_step > 0.0) || !(_to > _from)) {
        return 0u;
    }
    const u32 _segments = _closed ? _n : _n - 1u;
    u32 _made = 0;
    f64 _along = 0.0;   // the length where the segment starts
    for(u32 _j = 0; _j < _segments && _made < _most; _j++) {
        const fude_zoom_v2 _a = _p[_j], _b = _p[(_j + 1u) % _n];
        const f64 _len = rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y);
        const f64 _lo = rde_math_max_f64(_along, _from), _hi = rde_math_min_f64(_along + _len, _to);
        f64 _t0, _t1;
        if(_len > 0.0 && _hi >= _lo &&
           fude_zoom_page_segment_on_screen(fude_zoom_sim_apply(_to_screen, _a), fude_zoom_sim_apply(_to_screen, _b), _half, &_t0, &_t1)) {
            // Its lengths on the screen, then the first and last whole i whose _start + i·_step falls there (its end
            // left to the next segment, which begins there: a point on a corner is handed on once).
            const f64 _on_lo = rde_math_max_f64(_lo, _along + _t0 * _len), _on_hi = rde_math_min_f64(_hi, _along + _t1 * _len);
            const f64 _i0 = rde_math_ceil_f64((_on_lo - _start) / _step), _i1 = rde_math_ceil_f64((_on_hi - _start) / _step) - 1.0;
            if(_on_hi >= _on_lo && _i1 >= _i0) {
                const f64 _count = rde_math_min_f64(_i1 - _i0 + 1.0, (f64)(_most - _made));
                for(u32 _c = 0; _c < (u32)_count; _c++) {
                    const f64 _at = _start + (_i0 + (f64)_c) * _step;
                    if(!(_at < _to)) {
                        break;   // (the piece's end is the next one's start)
                    }
                    const f64 _u = rde_math_min_f64(rde_math_max_f64((_at - _along) / _len, 0.0), 1.0);
                    const fude_zoom_v2 _q = fude_zoom_sim_apply(_to_screen, (fude_zoom_v2){ _a.x + (_b.x - _a.x) * _u, _a.y + (_b.y - _a.y) * _u });
                    _dot(_user, (rde_vec_2F){ (f32)_q.x, (f32)_q.y });
                    _made++;
                }
            }
        }
        _along += _len;
    }
    return _made;
}

// A length said briefly: "125.5 mm", "1.25 m", "0.042 mm" (no trailing zeros).
RDE_INTERNAL void fude_zoom_page_say_mm(f64 _mm, c8* _out, usize _size) {
    const b8  _m   = rde_math_abs_f64(_mm) >= 1000.0;
    const f64 _v   = _m ? _mm / 1000.0 : _mm;
    const f64 _abs = rde_math_abs_f64(_v);
    const u32 _dec = _m ? 3u : _abs >= 100.0 ? 1u : _abs >= 1.0 ? 2u : _abs >= 0.01 ? 3u : 5u;
    rde_snprintf(_out, _size, "%.*f", (int)_dec, _v);
    c8* _dot = rde_strchr(_out, '.');
    if(_dot != NULL) {
        c8* _end = _out + rde_strlen(_out) - 1;
        while(_end > _dot && *_end == '0') { *_end-- = 0; }
        if(_end == _dot) { *_end = 0; }
    }
    rde_strcat(_out, _size, _m ? " m" : " mm");
}

// The measure being made (or the tape left showing): its length and angle on
// it; and the target the pen has snapped to, before it lifts.
RDE_INTERNAL void fude_zoom_page_render_measure(fude_zoom_page* _page) {
    const fude_theme* _t = fude_theme_active();
    if(_page->measuring || _page->tape_shown) {
        const fude_zoom_sim _to = fude_zoom_page_frame_sim(_page, _page->measure_frame);
        const fude_zoom_v2  _a  = fude_zoom_sim_apply(_to, _page->measure_a);
        const fude_zoom_v2  _b  = fude_zoom_sim_apply(_to, _page->measure_b);
        const f64 _len = rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y);
        if(_len >= 1.0) {
            c8 _length[FUDE_ZOOM_UNITS_TEXT], _angle[FUDE_ZOOM_UNITS_TEXT], _label[FUDE_ZOOM_UNITS_TEXT * 2u + 8u];
            fude_zoom_units_format(_len * fude_zoom_page_mm_per_point(_page), &_page->units, _length, sizeof(_length));
            f64 _deg = rde_math_atan2_f64(_b.y - _a.y, _b.x - _a.x) * 180.0 / RDE_PI;
            _deg = _deg < 0.0 ? _deg + 360.0 : _deg;
            fude_zoom_units_format_angle(_deg, 1u, _angle, sizeof(_angle));
            if(_page->measure_tool == 1u) {
                rde_snprintf(_label, sizeof(_label), "%s \xC2\xB7 %s", _length, _angle);
            } else {
                rde_snprintf(_label, sizeof(_label), "%s", _length);
            }
            fude_zoom_render_dimension(&_page->renderer, _a, _b, _page->measure_tool == 2u ? FUDE_ZOOM_PAGE_DIMENSION_OFFSET : 0.0, 1.5f, _t->accent, _label);
        }
    }
    if((_page->measuring || _page->shaping || _page->instruments.held >= 0) && _page->snap_hit.kind != FUDE_ZOOM_SNAP_NONE) {
        const fude_zoom_snap* _h = &_page->snap_hit;
        const rde_vec_2F _c = { (f32)_h->at.x, (f32)_h->at.y };
        if(_h->kind == FUDE_ZOOM_SNAP_GRID) {
            // A grid's crossing: a small cross.
            rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - 6.0f, _c.y }, (rde_vec_2F){ _c.x + 6.0f, _c.y }, _t->accent, 2.0f);
            rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x, _c.y - 6.0f }, (rde_vec_2F){ _c.x, _c.y + 6.0f }, _t->accent, 2.0f);
        } else if(_h->kind == FUDE_ZOOM_SNAP_NEAREST) {
            // On a line: a small cross on it.
            rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - 5.0f, _c.y - 5.0f }, (rde_vec_2F){ _c.x + 5.0f, _c.y + 5.0f }, _t->accent, 2.0f);
            rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - 5.0f, _c.y + 5.0f }, (rde_vec_2F){ _c.x + 5.0f, _c.y - 5.0f }, _t->accent, 2.0f);
        } else if(_h->kind == FUDE_ZOOM_SNAP_TANGENT) {
            // Touching a circle: the line from where it starts, and a ring where it touches.
            rde_color _soft = _t->accent;
            _soft.a = 140u;
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_page->snap_from.x, (f32)_page->snap_from.y }, _c, _soft, 1.5f);
            rde_rendering_2d_draw_circle_with_border((rde_vec_2F){ (f32)_h->a.x, (f32)_h->a.y }, (f32)rde_math_min_f64(_h->b.x, 4000.0), 64u, (rde_color){ 0, 0, 0, 0 }, 2.0f, _soft, NULL);
            rde_rendering_2d_draw_circle_with_border(_c, 5.0f, 16u, (rde_color){ 0, 0, 0, 0 }, 2.0f, _t->accent, NULL);
        } else if(_h->kind == FUDE_ZOOM_SNAP_EXTENSION) {
            // A straight thing carried on: dots from its nearer end to the pen.
            const f64 _da = rde_math_hypot_f64(_h->a.x - _h->at.x, _h->a.y - _h->at.y), _db = rde_math_hypot_f64(_h->b.x - _h->at.x, _h->b.y - _h->at.y);
            const fude_zoom_v2 _end = _da < _db ? _h->a : _h->b;
            const f64 _len = rde_math_min_f64(_da, _db);
            rde_color _soft = _t->accent;
            _soft.a = 170u;
            for(f64 _u = 0.0; _u < _len && _u < 2000.0; _u += 8.0) {
                const f64 _k2 = _u / rde_math_max_f64(_len, 1e-9);
                rde_rendering_2d_draw_circle((rde_vec_2F){ (f32)(_end.x + (_h->at.x - _end.x) * _k2), (f32)(_end.y + (_h->at.y - _end.y) * _k2) }, 1.3f, 6u, _soft, NULL);
            }
            rde_rendering_2d_draw_circle_with_border(_c, 5.0f, 16u, (rde_color){ 0, 0, 0, 0 }, 2.0f, _t->accent, NULL);
        } else if(_h->kind == FUDE_ZOOM_SNAP_PERP || _h->kind == FUDE_ZOOM_SNAP_PARALLEL) {
            // What it is square to or along, picked out; square: a right angle's mark at the foot.
            const b8 _piece = _h->a.x != _h->b.x || _h->a.y != _h->b.y;
            if(_piece) {
                rde_color _soft = _t->accent;
                _soft.a = 150u;
                rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_h->a.x, (f32)_h->a.y }, (rde_vec_2F){ (f32)_h->b.x, (f32)_h->b.y }, _soft, 3.0f);
            }
            const f64 _l = _piece ? rde_math_hypot_f64(_h->b.x - _h->a.x, _h->b.y - _h->a.y) : 0.0;
            if(_h->kind == FUDE_ZOOM_SNAP_PERP && _l > 0.0) {
                const fude_zoom_v2 _u = { (_h->b.x - _h->a.x) / _l, (_h->b.y - _h->a.y) / _l };
                const f64 _fx = _page->snap_from.x - _h->at.x, _fy = _page->snap_from.y - _h->at.y;
                const f64 _fl = rde_math_max_f64(rde_math_hypot_f64(_fx, _fy), 1e-9);
                const fude_zoom_v2 _v = { _fx / _fl, _fy / _fl };
                const f32 _m = 10.0f;
                const rde_vec_2F _p1 = { _c.x + (f32)_u.x * _m, _c.y + (f32)_u.y * _m }, _p2 = { _p1.x + (f32)_v.x * _m, _p1.y + (f32)_v.y * _m };
                const rde_vec_2F _p3 = { _c.x + (f32)_v.x * _m, _c.y + (f32)_v.y * _m };
                rde_rendering_2d_draw_line_1(_p1, _p2, _t->accent, 2.0f);
                rde_rendering_2d_draw_line_1(_p2, _p3, _t->accent, 2.0f);
            } else if(_h->kind == FUDE_ZOOM_SNAP_PARALLEL) {
                // The way it goes, on past the pen.
                const f64 _dx = _h->at.x - _page->snap_from.x, _dy = _h->at.y - _page->snap_from.y, _dl = rde_math_max_f64(rde_math_hypot_f64(_dx, _dy), 1e-9);
                const rde_vec_2F _far = { (f32)(_h->at.x + _dx / _dl * 60.0), (f32)(_h->at.y + _dy / _dl * 60.0) };
                rde_color _soft = _t->accent;
                _soft.a = 120u;
                rde_rendering_2d_draw_line_1(_c, _far, _soft, 1.5f);
            }
            rde_rendering_2d_draw_circle_with_border(_c, 5.0f, 16u, (rde_color){ 0, 0, 0, 0 }, 2.0f, _t->accent, NULL);
        } else {
            rde_rendering_2d_draw_circle_with_border(_c, 7.0f, 20u, (rde_color){ 0, 0, 0, 0 }, 2.0f, _t->accent, NULL);
        }
    }
}

// A length said on its line, from _a to _b, in the page's units (left unsaid when the line
// is too short to say it on).
RDE_INTERNAL void fude_zoom_page_anchor_length(const fude_zoom_page* _page, rde_vec_2F _a, rde_vec_2F _b, f64 _mm) {
    if(rde_math_abs_f64(_mm) < 1e-9 || _page->app->font == NULL) {
        return;
    }
    c8 _v[FUDE_ZOOM_UNITS_TEXT];
    fude_zoom_units_format(rde_math_abs_f64(_mm), &_page->units, _v, sizeof(_v));
    const f32 _px = 13.0f;
    const f32 _tw = fude_draw_text_width(_page->app->font, _page->app->font_px, _v, _px);
    if(rde_math_hypot_f32(_b.x - _a.x, _b.y - _a.y) < _tw + 16.0f) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    const rde_vec_2F  _m = { (_a.x + _b.x) * 0.5f, (_a.y + _b.y) * 0.5f };
    rde_rendering_2d_draw_rounded_rectangle_with_border(_m, (rde_vec_2F){ _tw + 10.0f, 18.0f }, 1.0f, 6u, _t->page, 1.0f, _t->accent, NULL);
    fude_draw_text_placed(_page->app->font, _v, (rde_vec_3F){ _m.x - _tw * 0.5f, _m.y - _px * 0.36f, 0.0f },
                          (rde_vec_2F){ _px / _page->app->font_px, _px / _page->app->font_px }, 0.0f, _t->accent);
}

// The offset pad up: the point the instrument snapped to, ringed, and the way
// from it to where its point is now (across, then up: the pad's two lengths, said on them).
RDE_INTERNAL void fude_zoom_page_render_anchor(fude_zoom_page* _page) {
    if(!_page->shown_anchor || _page->anchor_tool < 0 || _page->anchor_frame >= fude_zoom_scene_frame_count(&_page->scene)) {
        return;
    }
    const fude_theme*  _t   = fude_theme_active();
    const fude_zoom_v2 _ref = fude_zoom_sim_apply(fude_zoom_page_frame_sim(_page, _page->anchor_frame), _page->anchor_point);
    const rde_vec_2F   _c   = { (f32)_ref.x, (f32)_ref.y };
    fude_zoom_v2 _keys[FUDE_ZOOM_INSTRUMENT_KEYS];
    if(_page->anchor_key < fude_zoom_instrument_keys(&_page->instruments, (u32)_page->anchor_tool, _keys) &&
       rde_math_hypot_f64(_keys[_page->anchor_key].x - _ref.x, _keys[_page->anchor_key].y - _ref.y) > 2.0) {
        const rde_vec_2F _k = { (f32)_keys[_page->anchor_key].x, (f32)_keys[_page->anchor_key].y };
        rde_rendering_2d_draw_line_1(_c, (rde_vec_2F){ _k.x, _c.y }, _t->accent, 2.0f);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _k.x, _c.y }, _k, _t->accent, 2.0f);
        rde_rendering_2d_draw_circle_with_border(_k, 3.5f, 16u, _t->accent, 0.0f, _t->accent, NULL);
        fude_zoom_page_anchor_length(_page, _c, (rde_vec_2F){ _k.x, _c.y }, _page->anchor_mm[0]);
        fude_zoom_page_anchor_length(_page, (rde_vec_2F){ _k.x, _c.y }, _k, _page->anchor_mm[1]);
    }
    rde_rendering_2d_draw_circle_with_border(_c, 6.0f, 20u, (rde_color){ 0, 0, 0, 0 }, 2.0f, _t->accent, NULL);
    // Slid by hand: the line it slides along, through its point, dashed across the screen.
    if(_page->anchor_sliding && _page->anchor_slide_axis >= 0 && _page->instruments.held == _page->anchor_tool &&
       _page->anchor_key < fude_zoom_instrument_keys(&_page->instruments, (u32)_page->anchor_tool, _keys)) {
        const fude_zoom_v2 _k    = _keys[_page->anchor_key];
        const fude_zoom_v2 _half = fude_zoom_page_half(_page);
        const b8           _x    = _page->anchor_slide_axis == 0;
        const f64          _lo   = _x ? -_half.x : -_half.y, _hi = _x ? _half.x : _half.y;
        rde_color _guide = _t->accent;
        _guide.a = 150;
        for(f64 _u = _lo; _u < _hi; _u += 14.0) {
            const f64 _v = rde_math_min_f64(_u + 8.0, _hi);
            const rde_vec_2F _a = _x ? (rde_vec_2F){ (f32)_u, (f32)_k.y } : (rde_vec_2F){ (f32)_k.x, (f32)_u };
            const rde_vec_2F _b = _x ? (rde_vec_2F){ (f32)_v, (f32)_k.y } : (rde_vec_2F){ (f32)_k.x, (f32)_v };
            rde_rendering_2d_draw_line_1(_a, _b, _guide, 1.2f);
        }
    }
}

// The instruments over the page, and beside the pen drawing along one, how far it has gone.
RDE_INTERNAL void fude_zoom_page_render_instruments(fude_zoom_page* _page) {
    if(!fude_zoom_instruments_any(&_page->instruments)) {
        return;
    }
    fude_zoom_page_compass_follow(_page);
    const fude_theme* _t = fude_theme_active();
    rde_color _body = _t->surface;
    _body.a = 165;   // the drawing seen through it
    const fude_zoom_instruments_look _look = {
        _page->app->font, _page->app->font_px, fude_zoom_page_mm_per_point(_page),
        _body, _t->text_soft, _t->text, _t->text, _t->accent, &_page->units,
    };
    fude_zoom_instruments_render(&_page->instruments, &_look, rde_engine_get_time_now());
    f64          _along;
    b8           _degrees;
    fude_zoom_v2 _at;
    // Made longer or bigger: how long (a ruler), its leg or its radius, in real units, by its tab.
    fude_zoom_v2 _tab;
    if(_page->instruments.growing && _page->instruments.held >= 0 && _page->app->font != NULL &&
       fude_zoom_instrument_grow_at(&_page->instruments, (u32)_page->instruments.held, &_tab)) {
        c8 _say[FUDE_ZOOM_UNITS_TEXT];
        fude_zoom_units_format(fude_zoom_instrument_extent(&_page->instruments, (u32)_page->instruments.held) * _look.mm_per_point, &_page->units, _say, sizeof(_say));
        const f32 _px = 14.0f;
        const f32 _w  = fude_draw_text_width(_page->app->font, _page->app->font_px, _say, _px);
        const rde_vec_2F _c = { (f32)_tab.x, (f32)_tab.y + 30.0f };
        rde_rendering_2d_draw_rounded_rectangle_with_border(_c, (rde_vec_2F){ _w + 18.0f, 26.0f }, 1.0f, 8u, _t->surface, 1.0f, _t->accent, NULL);
        fude_draw_text_placed(_page->app->font, _say, (rde_vec_3F){ _c.x - _w * 0.5f, _c.y - _px * 0.36f, 0.0f },
                              (rde_vec_2F){ _px / _page->app->font_px, _px / _page->app->font_px }, 0.0f, _t->text);
    }
    if(_page->drawing && _page->ruled && fude_zoom_instruments_ruled(&_page->instruments, &_along, &_degrees, &_at) && _page->app->font != NULL) {
        c8 _say[40];
        if(_degrees) {
            rde_snprintf(_say, sizeof(_say), "%.1f\xC2\xB0", _along);
        } else {
            fude_zoom_page_say_mm(_along * _look.mm_per_point, _say, sizeof(_say));
        }
        const f32 _px = 14.0f;
        const f32 _w  = fude_draw_text_width(_page->app->font, _page->app->font_px, _say, _px);
        rde_rendering_2d_draw_rounded_rectangle_with_border((rde_vec_2F){ (f32)_at.x, (f32)_at.y }, (rde_vec_2F){ _w + 18.0f, 26.0f }, 1.0f, 8u, _t->surface, 1.0f, _t->accent, NULL);
        fude_draw_text_placed(_page->app->font, _say, (rde_vec_3F){ (f32)_at.x - _w * 0.5f, (f32)_at.y - _px * 0.36f, 0.0f },
                              (rde_vec_2F){ _px / _page->app->font_px, _px / _page->app->font_px }, 0.0f, _t->text);
    }
}

// The selection being turned: by how much, over its knob ("−37.5°": clockwise).
RDE_INTERNAL void fude_zoom_page_render_turn(fude_zoom_page* _page) {
    f64        _deg;
    rde_vec_2F _at;
    if(_page->app->font == NULL || !fude_zoom_select_turning(&_page->selection, &_deg, &_at)) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    c8 _say[32];
    const f64 _shown = rde_math_round_f64(_deg * 10.0) / 10.0;
    rde_snprintf(_say, sizeof(_say), "%s%.1f\xC2\xB0", _shown < 0.0 ? "\xE2\x88\x92" : "", rde_math_abs_f64(_shown));
    const f32 _px = 14.0f;
    const f32 _w  = fude_draw_text_width(_page->app->font, _page->app->font_px, _say, _px);
    rde_rendering_2d_draw_rounded_rectangle_with_border(_at, (rde_vec_2F){ _w + 18.0f, 26.0f }, 1.0f, 8u, _t->surface, 1.0f, _t->accent, NULL);
    fude_draw_text_placed(_page->app->font, _say, (rde_vec_3F){ _at.x - _w * 0.5f, _at.y - _px * 0.36f, 0.0f },
                          (rde_vec_2F){ _px / _page->app->font_px, _px / _page->app->font_px }, 0.0f, _t->text);
}

// Declared ahead: fude_zoom_page_render times it.
RDE_INTERNAL void fude_zoom_page_render_all(fude_zoom_page* _page, rde_window* _window);

// See page.h. The page's drawing is timed into perf_draw, for fude_zoom_page_perf_frame.
void fude_zoom_page_render(fude_zoom_page* _page, rde_window* _window) {
    const f64 _t0 = rde_engine_get_time_now();
    fude_zoom_page_render_all(_page, _window);
    _page->perf_draw += rde_engine_get_time_now() - _t0;
}

// See page.h. While the frame rate is not shown, the sums are kept at zero. Every 2 s the means a frame are logged:
// the app's update (Play's step within it), its render (the page's drawing within it), and the rest.
void fude_zoom_page_perf_frame(fude_zoom_page* _page, f64 _update, f64 _render) {
    const f64 _now = rde_engine_get_time_now();
    if(!_page->show_fps || _page->perf_since <= 0.0 || _now < _page->perf_since) {
        _page->perf_since  = _now;
        _page->perf_update = _page->perf_render = _page->perf_play = _page->perf_draw = 0.0;
        _page->perf_frames = 0u;
        return;
    }
    _page->perf_update += _update;
    _page->perf_render += _render;
    _page->perf_frames++;
    if(_now - _page->perf_since >= 2.0 && _page->perf_frames > 0u) {
        const f64 _n = (f64)_page->perf_frames, _span = _now - _page->perf_since;
        rde_log_level(RDE_LOG_LEVEL_INFO, "canvas perf: %.1f fps, a frame %.1f ms: update %.1f (Play %.1f), render %.1f (the page %.1f), else %.1f",
                      _n / _span, 1000.0 * _span / _n, 1000.0 * _page->perf_update / _n, 1000.0 * _page->perf_play / _n, 1000.0 * _page->perf_render / _n,
                      1000.0 * _page->perf_draw / _n, 1000.0 * (_span - _page->perf_update - _page->perf_render) / _n);
        _page->perf_since  = _now;
        _page->perf_update = _page->perf_render = _page->perf_play = _page->perf_draw = 0.0;
        _page->perf_frames = 0u;
    }
}

// A note's bold and italic letters (rich.h), loaded the first time the page is drawn: Roboto's, falling through to the
// app's fonts for the scripts Roboto lacks.
RDE_INTERNAL void fude_zoom_page_rich_fonts(fude_zoom_page* _page) {
    if(_page->rich_fonts_tried || _page->app->ui == NULL || _page->app->ui->font == NULL) {
        return;
    }
    _page->rich_fonts_tried = true;
    const rde_font_parameters _slug = RDE_DEFAULT_SLUG_FONT_PARAMETERS;
    const c8* const _paths[3] = { FUDE_DRAW_FONT_BOLD, FUDE_DRAW_FONT_ITALIC, FUDE_DRAW_FONT_BOLD_ITALIC };
    rde_font** const _to[3]   = { &_page->renderer.font_bold, &_page->renderer.font_italic, &_page->renderer.font_bold_italic };
    for(u32 _i = 0; _i < 3u; _i++) {
        *_to[_i] = rde_font_load(_paths[_i], FUDE_KIT_FONT_SIZE, NULL, &_slug, NULL);
        if(*_to[_i] != NULL && _page->app->ui->font_script != NULL) {
            rde_font_add_fallback(*_to[_i], _page->app->ui->font_script);
        }
        if(*_to[_i] != NULL && _page->app->ui->font_latin != NULL) {
            rde_font_add_fallback(*_to[_i], _page->app->ui->font_latin);
        }
    }
}

// Draws the page, bottom to top: the paper, a PDF's pages, the canvas (with what is playing: a mechanism's moving
// parts, graphs, a recording's faint ink), the mechanism as it plays, its bodies' marks and tracers, the lines a drag
// lines up along, sheets' rulers; the stroke or shape being drawn; the selection and what goes with it; the marks,
// the measure, guides, readouts, constraints, the circuit, nodes and text being written; the instruments, the offset
// pad and the brush; then the panels over everything. Presenting leaves out what only helps to draw.
RDE_INTERNAL void fude_zoom_page_render_all(fude_zoom_page* _page, rde_window* _window) {
    if(!_page->open) {
        return;
    }
    // The renderer told what it draws with: the rich fonts (once), the app's font for dimensions' lengths, the
    // canvas's units.
    fude_zoom_page_rich_fonts(_page);
    _page->renderer.font        = _page->app->font;
    _page->renderer.font_px     = _page->app->font_px;
    _page->renderer.link_color  = fude_theme_active()->accent;
    fude_zoom_export_measure(fude_zoom_render_measure, &_page->renderer);   // (a note exported as it is drawn)
    _page->renderer.mm_per_unit = FUDE_ZOOM_PAGE_MM_PER_UNIT;
    _page->renderer.units       = _page->units;
    fude_zoom_page_material_words(_page->material_words, "");   // (a board's, after its sizes)
    _page->renderer.material_words = _page->material_words;
    fude_zoom_page_render_paper(_page, _window);
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    fude_zoom_pdfview_render(&_page->pdf, fude_zoom_page_frame_sim(_page, _page->scene.home), _half);   // (a PDF's pages, under the ink)
    _page->renderer.no_guides = _page->presenting;   // (presenting: what is drawn, not what helped draw it)
    const rde_arr* _lifted_was = _page->renderer.lifted;
    if(_page->play_on && _page->play_mech && rde_arr_length(&_page->mech_lifted) > 0u) {
        _page->renderer.lifted = &_page->mech_lifted;   // (a mechanism playing: its moving parts drawn where they are, below)
    }
    _page->renderer.plot_mask = _page->play_on && _page->play_plots ? &_page->play_scope : NULL;   // (graphs playing: as their variables go)
    _page->renderer.plot_play = &_page->plot_play;
    _page->renderer.moved     = fude_zoom_page_moving_parts(_page);   // (solenoids' plungers, followers, worms' threads: as far as they have gone)
    _page->renderer.faint = rde_arr_length(&_page->audio_faint) > 0u ? &_page->audio_faint : NULL;   // (a recording played: what it has not reached, faint)
    fude_zoom_render(&_page->renderer, &_page->scene, _half, fude_theme_active()->page);
    _page->renderer.faint = NULL;
    _page->renderer.lifted    = _lifted_was;
    _page->renderer.plot_mask = NULL;
    _page->renderer.moved     = NULL;
    fude_zoom_page_render_mech(_page, _half);
    fude_zoom_page_render_bodies(_page);
    fude_zoom_page_render_trails(_page, _half);
    fude_zoom_page_render_align(_page, _half);
    _page->renderer.stuck_count = 0;
    if(!_page->presenting) {
        fude_zoom_render_sheets_stuck(&_page->renderer, &_page->scene, _half);   // (zoomed into a sheet: its rulers on the screen's edges)
    }

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
                _r = rde_math_max_f32(_r, fude_ink_stroke_points(_c, _st)[_i].radius);
            }
            _r = rde_math_max_f32(_r * (f32)fude_zoom_sim_scale(fude_zoom_page_draw_sim(_page)), FUDE_ZOOM_RENDER_MIN_RADIUS);
            fude_zoom_page_draw_fit(_page, &_f, _r, fude_theme_resolve(_st->color), false);
        }
    } else if(_page->drawing) {
        const fude_zoom_sim _to = fude_zoom_sim_compose(fude_zoom_camera_sim(&_page->scene.camera), fude_zoom_scene_sim(&_page->scene, _page->draw_frame, _page->scene.camera.frame));
        const fude_zoom_v2  _o  = fude_zoom_sim_apply(_to, _page->origin);
        fude_ink_render(&_page->capture, (rde_vec_2F){ (f32)_o.x, (f32)_o.y }, (f32)fude_zoom_sim_scale(_to),
                        (rde_vec_2F){ (f32)_half.x, (f32)_half.y }, rde_engine_get_time_now(), false);
    }
    if(_page->shaping && _page->shape_tool == FUDE_ZOOM_SHAPE_GUIDE) {
        // A guide being drawn: across the screen the way the pen goes, its point marked.
        const fude_zoom_v2 _a = { _page->shape_from.x, _page->shape_from.y };
        const f64 _dx = (f64)_page->pen_now.x - _a.x, _dy = (f64)_page->pen_now.y - _a.y, _l = rde_math_hypot_f64(_dx, _dy);
        if(_l > 0.0) {
            const f64 _far = 4.0 * rde_math_hypot_f64(_half.x, _half.y);
            fude_zoom_v2 _p = { _a.x - _dx / _l * _far, _a.y - _dy / _l * _far }, _q = { _a.x + _dx / _l * _far, _a.y + _dy / _l * _far };
            if(fude_zoom_clip_line(&_p, &_q, (fude_zoom_box){ -_half.x, -_half.y, _half.x, _half.y })) {
                rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_p.x, (f32)_p.y }, (rde_vec_2F){ (f32)_q.x, (f32)_q.y }, fude_zoom_guide_color(), 1.5f);
            }
        }
        rde_rendering_2d_draw_circle((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, 3.5f, 16u, fude_zoom_guide_color(), NULL);
    } else if(_page->shaping) {
        const fude_zoom_shape_fit _f = fude_zoom_page_drag_fit(_page);
        const f64 _k = fude_zoom_sim_scale(fude_zoom_page_draw_sim(_page));
        const f32 _r = rde_math_max_f32((f32)((f64)fude_zoom_page_brush_radius(_page, _page->draw_frame, _k) * _k), FUDE_ZOOM_RENDER_MIN_RADIUS);
        fude_zoom_page_draw_fit(_page, &_f, _r, fude_theme_resolve(_page->app->ink->color), _page->shape_filled);
    }
    fude_zoom_select_render(&_page->selection, &_page->renderer, &_page->scene, _half);
    fude_zoom_page_render_stretch(_page);
    fude_zoom_page_render_turn(_page);
    if(!_page->presenting) {
        fude_zoom_page_render_marks(_page, _half);   // (presenting: only what is presented)
    }
    fude_zoom_page_render_measure(_page);
    fude_zoom_page_render_guide_pull(_page, _half);
    fude_zoom_page_render_readout(_page, _half);
    fude_zoom_page_render_constraints(_page, _half);
    fude_zoom_page_render_circuit(_page, _half);
    fude_zoom_page_render_curve(_page);
    fude_zoom_page_render_nodes(_page);
    fude_zoom_page_render_nudge(_page);
    fude_zoom_page_render_inline(_page);   // (a text being written on the page)
    // The lasso's loop over a PDF's text, held (Copy text): its box, outlined.
    if(_page->pdf_text_on && !fude_zoom_select_any(&_page->selection)) {
        const fude_zoom_sim _home = fude_zoom_page_frame_sim(_page, _page->scene.home);
        const fude_zoom_v2  _a    = fude_zoom_sim_apply(_home, (fude_zoom_v2){ _page->pdf_text_box.min_x, _page->pdf_text_box.min_y });
        const fude_zoom_v2  _b    = fude_zoom_sim_apply(_home, (fude_zoom_v2){ _page->pdf_text_box.max_x, _page->pdf_text_box.max_y });
        rde_color _tint = fude_theme_active()->accent;
        _tint.a = 40;
        const rde_vec_2F _c = { (f32)((_a.x + _b.x) * 0.5), (f32)((_a.y + _b.y) * 0.5) };
        const rde_vec_2F _z = { (f32)rde_math_abs_f64(_b.x - _a.x), (f32)rde_math_abs_f64(_b.y - _a.y) };
        rde_rendering_2d_draw_rectangle(_c, _z, _tint);
        rde_rendering_2d_draw_rectangle_border(_c, _z, 1.5f, fude_theme_active()->accent, NULL);
    }
    if(!_page->presenting) {
        fude_zoom_page_render_instruments(_page);
        fude_zoom_page_render_anchor(_page);
        fude_zoom_page_render_brush(_page);
    }
    fude_zoom_page_render_calibration(_page);
    fude_zoom_page_render_play(_page);      // (Play's panel)
    fude_zoom_page_render_library(_page);   // (the diagram library's panel, over everything on the page)
    fude_zoom_page_render_pieces(_page);    // (My pieces' panel)
    fude_zoom_page_map_refresh(_page);
    fude_zoom_page_render_map(_page);       // (the map's panel)
    fude_zoom_page_render_strip(_page);     // (a notebook's pages' strip)
    fude_zoom_page_render_everywhere(_page);   // (Find's other canvases)
    fude_zoom_page_render_audio(_page);        // (the recordings' panel)
    fude_zoom_page_render_caption(_page);      // (Live text's last words)
    fude_zoom_page_render_present(_page);   // (presenting: the laser, the pill)
    fude_zoom_page_render_fps(_page);       // (Settings' Display: the frame rate, over everything)
}
