// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"
#include "drawing/base/redraw.h"

// ===========================================================================
// Page, a notebook's pages strip (0.1.86), and the panel listing the canvases Find found the words in.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// --- a notebook's pages strip (0.1.86) ------------------------------------------------------------------

#define FUDE_ZOOM_PAGE_STRIP_GAP   14.0    // between pages
#define FUDE_ZOOM_PAGE_STRIP_LABEL 24.0    // its number, under it
#define FUDE_ZOOM_PAGE_STRIP_PX    2.0     // a picture's pixels a point

// Where the strip is (screen, centre origin, Y up): over the row at the bottom left, from its left, as wide as the
// screen lets it be (clear of the toolbar).
void fude_zoom_page_strip_lay(const fude_zoom_page* _page, fude_zoom_page_strip_layout* _l) {
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    const rde_vec_2F   _ui   = fude_kit_screen_size(_page->app->window);
    const b8  _placed = _page->places_center.x > 0.0f;
    f64 _left = _placed ? (f64)(_page->places_center.x - FUDE_ZOOM_PAGE_UI_PLACES_W * 0.5f - _ui.x * 0.5f) : -_half.x + 16.0;
    const f64 _bottom = _placed ? (f64)(_page->places_center.y + FUDE_ZOOM_PAGE_UI_CHIP_H * 0.5f - _ui.y * 0.5f) + 12.0 : -_half.y + 76.0;
    const f64 _top = _bottom + 2.0 * FUDE_ZOOM_PAGE_STRIP_PAD + FUDE_ZOOM_PAGE_STRIP_H + FUDE_ZOOM_PAGE_STRIP_LABEL;
    f64 _right = _half.x - 16.0;
    fude_zoom_page_clear_of_bar(_page, &_left, &_right, _bottom, _top, 240.0);
    _l->panel = (fude_zoom_box){ _left, _bottom, _right, _top };
    _l->close = (fude_zoom_box){ _right - FUDE_ZOOM_PAGE_STRIP_CLOSE, _top - 44.0, _right - 8.0, _top - 8.0 };
    _l->inner = (fude_zoom_box){ _left + FUDE_ZOOM_PAGE_STRIP_PAD, _bottom + FUDE_ZOOM_PAGE_STRIP_PAD, _right - FUDE_ZOOM_PAGE_STRIP_CLOSE - 4.0, _top - FUDE_ZOOM_PAGE_STRIP_PAD };
    _l->th = FUDE_ZOOM_PAGE_STRIP_H;
    _l->tw = _page->pages.h > 0.0 ? FUDE_ZOOM_PAGE_STRIP_H * _page->pages.w / _page->pages.h : FUDE_ZOOM_PAGE_STRIP_H * 0.7;
}

// Page _i's picture's box in it, as scrolled.
RDE_INTERNAL fude_zoom_box fude_zoom_page_strip_box(const fude_zoom_page* _page, const fude_zoom_page_strip_layout* _l, u32 _i) {
    const f64 _x = _l->inner.min_x + (f64)_i * (_l->tw + FUDE_ZOOM_PAGE_STRIP_GAP) - _page->strip_scroll;
    return (fude_zoom_box){ _x, _l->inner.max_y - _l->th, _x + _l->tw, _l->inner.max_y };
}

// How far it can be scrolled.
RDE_INTERNAL f64 fude_zoom_page_strip_most(const fude_zoom_page* _page, const fude_zoom_page_strip_layout* _l) {
    const f64 _all = (f64)_page->pages.count * (_l->tw + FUDE_ZOOM_PAGE_STRIP_GAP) - FUDE_ZOOM_PAGE_STRIP_GAP;
    return rde_math_max_f64(_all - (_l->inner.max_x - _l->inner.min_x), 0.0);
}

// The page the view is on (its middle's).
u32 fude_zoom_page_page_now(const fude_zoom_page* _page) {
    const fude_zoom_v2 _mid = fude_zoom_sim_apply(fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _page->scene.home)), (fude_zoom_v2){ 0.0, 0.0 });
    return fude_zoom_pages_at(&_page->pages, _mid);
}

// Opened: scrolled to have the page you are on in its middle.
void fude_zoom_page_strip_open(fude_zoom_page* _page) {
    fude_zoom_page_strip_layout _l;
    fude_zoom_page_strip_lay(_page, &_l);
    const f64 _at = (f64)fude_zoom_page_page_now(_page) * (_l.tw + FUDE_ZOOM_PAGE_STRIP_GAP) + _l.tw * 0.5;
    _page->strip_scroll = rde_math_min_f64(rde_math_max_f64(_at - (_l.inner.max_x - _l.inner.min_x) * 0.5, 0.0), fude_zoom_page_strip_most(_page, &_l));
    _page->strip_open   = true;
    _page->strip_pick   = FUDE_ZOOM_NONE;   // (no page held: none of their buttons up)
    _page->strip_lifted = false;
    _page->everywhere_shown = false;
    _page->audio_panel  = false;
    _page->map_open     = false;
    _page->places_open  = false;
}

// The pages' chip: the strip up or put away (a second press within 0.35 s is the same tap: a tablet's comes twice).
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_pages(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    fude_zoom_page* _page = fude_zoom_page_the;
    const f64       _now  = rde_engine_get_time_now();
    if(_page == NULL || !_page->open || !_page->pages.on || _now - _page->pages_pressed_at < 0.35) {
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    _page->pages_pressed_at = _now;
    if(_page->strip_open) {
        _page->strip_open = false;
    } else {
        fude_zoom_page_strip_open(_page);
    }
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// See page_parts.h. A picture is drawn into one of FUDE_ZOOM_PAGE_STRIP_PICS textures, its page's own or else the one
// shown longest ago (strip_lru), with the scene's camera pointed at the page for the while (never settled so) and
// then put back.
void fude_zoom_page_strip_draw(fude_zoom_page* _page, rde_window* _window, rde_camera* _camera) {
    if(!_page->strip_open || !_page->open || !_page->pages.on) {
        return;
    }
    fude_zoom_page_strip_layout _l;
    fude_zoom_page_strip_lay(_page, &_l);
    fude_zoom_scene* _s = &_page->scene;
    for(u32 _i = 0; _i < _page->pages.count; _i++) {
        const fude_zoom_box _b = fude_zoom_page_strip_box(_page, &_l, _i);
        if(_b.max_x < _l.inner.min_x || _b.min_x > _l.inner.max_x) {
            continue;
        }
        // Its picture: kept (and up to date: nothing to do), else drawn again into its slot, or into a free one or the
        // one shown longest ago (its texture reused).
        u32 _slot = rde_lru_find(&_page->strip_lru, (const any)&_i, sizeof(_i));
        if(_slot != RDE_LRU_NONE && _page->strip_pics[_slot] != NULL && _page->strip_pic_ready[_slot] && _page->strip_pic_rev[_slot] == _s->revision) {
            continue;
        }
        if(_slot == RDE_LRU_NONE) {
            _slot = rde_lru_add(&_page->strip_lru, (const any)&_i, sizeof(_i), NULL);
            _page->strip_pic_ready[_slot] = false;   // (another page's picture until drawn again)
        }
        const u32 _pw = (u32)(_l.tw * FUDE_ZOOM_PAGE_STRIP_PX), _ph = (u32)(_l.th * FUDE_ZOOM_PAGE_STRIP_PX);
        if(_page->strip_pics[_slot] == NULL) {
            _page->strip_pics[_slot] = rde_render_texture_load(_pw, _ph, NULL);
            if(_page->strip_pics[_slot] == NULL) {
                return;
            }
        }
        const fude_zoom_camera _was  = _s->camera;
        const fude_zoom_v2     _half = { (f64)_pw * 0.5, (f64)_ph * 0.5 };
        const rde_color        _bg   = fude_theme_active()->page;
        _s->camera = fude_zoom_nav_framing(_half, _s->home, fude_zoom_pages_box(&_page->pages, _i), 1.0);   // (drawn only: never settled like this)
        rde_render_texture_enable(_page->strip_pics[_slot]);
        rde_rendering_2d_begin_drawing(_window, _camera);
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ (f32)_pw + 4.0f, (f32)_ph + 4.0f }, _bg);
        fude_zoom_page_draw_sheet(_page, _i);   // (its paper)
        _page->renderer.no_guides = true;
        fude_zoom_render(&_page->renderer, _s, _half, _bg);
        _page->renderer.no_guides = false;
        rde_rendering_2d_end_drawing();
        rde_render_texture_disable();
        _s->camera = _was;
        _page->strip_pic_ready[_slot] = true;
        _page->strip_pic_rev[_slot]   = _s->revision;
        fude_redraw_changed();   // (render on demand: the next, if any, in the next frame)
        return;   // (one a frame)
    }
}

// See page_parts.h.
void fude_zoom_page_strip_free(fude_zoom_page* _page) {
    for(u32 _k = 0; _k < FUDE_ZOOM_PAGE_STRIP_PICS; _k++) {
        if(_page->strip_pics[_k] != NULL) {
            rde_render_texture_unload(_page->strip_pics[_k]);
            _page->strip_pics[_k] = NULL;
        }
        _page->strip_pic_ready[_k] = false;
    }
    rde_lru_clear(&_page->strip_lru);
}

#define FUDE_ZOOM_PAGE_STRIP_HOLD   0.45    // a page held this long without moving: lifted, to be dragged to its new place
#define FUDE_ZOOM_PAGE_STRIP_CHIP_H 40.0    // a held page's buttons, over the strip
#define FUDE_ZOOM_PAGE_STRIP_EDGE   44.0    // a page dragged this near an end of the strip: it scrolls on

// Page _i shown at place _slot (its own, or another while one is dragged past it): its picture's box, as scrolled.
fude_zoom_box fude_zoom_page_strip_slot(const fude_zoom_page* _page, const fude_zoom_page_strip_layout* _l, f64 _slot) {
    const f64 _x = _l->inner.min_x + _slot * (_l->tw + FUDE_ZOOM_PAGE_STRIP_GAP) - _page->strip_scroll;
    return (fude_zoom_box){ _x, _l->inner.max_y - _l->th, _x + _l->tw, _l->inner.max_y };
}

// The page whose picture is under _at (inside the strip); FUDE_ZOOM_NONE: none.
RDE_INTERNAL u32 fude_zoom_page_strip_under(const fude_zoom_page* _page, const fude_zoom_page_strip_layout* _l, rde_vec_2F _at) {
    if(_at.x < _l->inner.min_x || _at.x > _l->inner.max_x) {
        return FUDE_ZOOM_NONE;
    }
    for(u32 _i = 0; _i < _page->pages.count; _i++) {
        if(fude_zoom_page_box_holds(fude_zoom_page_strip_box(_page, _l, _i), _at)) {
            return _i;
        }
    }
    return FUDE_ZOOM_NONE;
}

// Where the page dragged would go, let go at _x: the place its picture's middle is over.
RDE_INTERNAL u32 fude_zoom_page_strip_place(const fude_zoom_page* _page, const fude_zoom_page_strip_layout* _l, f64 _x) {
    const f64 _k = rde_math_floor_f64((_x - _l->inner.min_x + _page->strip_scroll) / (_l->tw + FUDE_ZOOM_PAGE_STRIP_GAP));
    return _k <= 0.0 ? 0u : _k >= (f64)(_page->pages.count - 1u) ? _page->pages.count - 1u : (u32)_k;
}

// A held page's buttons (Add before, Add after, Delete page), over the strip above its picture: their boxes. False:
// none.
RDE_INTERNAL b8 fude_zoom_page_strip_chips(const fude_zoom_page* _page, const fude_zoom_page_strip_layout* _l, fude_zoom_box _out[3]) {
    if(_page->strip_pick == FUDE_ZOOM_NONE || _page->strip_lifted || _page->strip_pick >= _page->pages.count) {
        return false;
    }
    const c8* _words[3] = { fude_text(FUDE_TEXT_ZOOM_PAGE_BEFORE), fude_text(FUDE_TEXT_ZOOM_PAGE_AFTER), fude_text(FUDE_TEXT_ZOOM_PAGE_DELETE) };
    f64 _w[3], _all = 0.0;
    for(u32 _k = 0; _k < 3u; _k++) {
        const f64 _text = _page->renderer.font != NULL ? (f64)fude_draw_text_width(_page->renderer.font, _page->renderer.font_px, _words[_k], 15.0f)
                                                       : 8.0 * (f64)rde_strlen(_words[_k]);
        _w[_k] = _text + 32.0;
        _all  += _w[_k] + (_k > 0u ? 8.0 : 0.0);
    }
    const fude_zoom_v2  _half = fude_zoom_page_half(_page);
    const fude_zoom_box _b    = fude_zoom_page_strip_box(_page, _l, _page->strip_pick);
    f64 _x = rde_math_min_f64(rde_math_max_f64((_b.min_x + _b.max_x) * 0.5 - _all * 0.5, -_half.x + 12.0), _half.x - 12.0 - _all);
    const f64 _y0 = _l->panel.max_y + 8.0;
    for(u32 _k = 0; _k < 3u; _k++) {
        _out[_k] = (fude_zoom_box){ _x, _y0, _x + _w[_k], _y0 + FUDE_ZOOM_PAGE_STRIP_CHIP_H };
        _x += _w[_k] + 8.0;
    }
    return true;
}

// Page _i's picture in the box _b, cut at the strip's ends (_lifted: held up, whole, over the rest), its number under
// it.
RDE_INTERNAL void fude_zoom_page_strip_picture(fude_zoom_page* _page, const fude_zoom_page_strip_layout* _l, u32 _i, fude_zoom_box _b, u32 _now, b8 _lifted) {
    const fude_theme* _t = fude_theme_active();
    const f64 _x0 = _lifted ? _b.min_x : rde_math_max_f64(_b.min_x, _l->inner.min_x), _x1 = _lifted ? _b.max_x : rde_math_min_f64(_b.max_x, _l->inner.max_x);
    if(_x1 <= _x0) {
        return;
    }
    const rde_vec_2F _c = { (f32)((_x0 + _x1) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) }, _size = { (f32)(_x1 - _x0), (f32)(_b.max_y - _b.min_y) };
    if(_lifted) {
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ _c.x + 4.0f, _c.y - 6.0f }, _size, (rde_color){ 0, 0, 0, 60 });
    }
    rde_rendering_2d_draw_rectangle(_c, _size, _t->page);
    const u32 _k = rde_lru_find(&_page->strip_lru, (const any)&_i, sizeof(_i));   // (shown now: kept the longest)
    if(_k != RDE_LRU_NONE && _page->strip_pics[_k] != NULL && _page->strip_pic_ready[_k]) {
        const rde_texture* _tex = rde_render_texture_get_texture(_page->strip_pics[_k]);
        const f32 _pw = (f32)(_l->tw * FUDE_ZOOM_PAGE_STRIP_PX), _ph = (f32)(_l->th * FUDE_ZOOM_PAGE_STRIP_PX);
        const f32 _u0 = (f32)((_x0 - _b.min_x) / _l->tw) * _pw, _u1 = (f32)((_x1 - _b.min_x) / _l->tw) * _pw;
        if(_tex != NULL && _u1 > _u0) {
            rde_rendering_2d_draw_texture_partial_2(_tex, (rde_vec_3F){ _c.x, _c.y, 0.0f }, (rde_vec_2F){ 1.0f / (f32)FUDE_ZOOM_PAGE_STRIP_PX, -1.0f / (f32)FUDE_ZOOM_PAGE_STRIP_PX },
                                                    0.0f, (rde_color){ 255, 255, 255, 255 }, (rde_vec_2F){ _u0, 0.0f }, (rde_vec_2F){ _u1, _ph });
        }
    }
    const b8 _lit = _i == _now || _lifted || _i == _page->strip_pick;
    rde_rendering_2d_draw_rectangle_border(_c, _size, _lit ? 2.5f : 1.0f, _lit ? _t->accent : _t->outline, NULL);
    // Its number under it.
    c8 _num[16];
    rde_snprintf(_num, sizeof(_num), "%u", _i + 1u);
    const fude_zoom_box _nb = { _b.min_x, _b.min_y - FUDE_ZOOM_PAGE_STRIP_LABEL, _b.max_x, _b.min_y };
    if(_lifted || (_nb.min_x >= _l->inner.min_x - 1.0 && _nb.max_x <= _l->inner.max_x + 1.0)) {
        fude_zoom_render_text_in(&_page->renderer, _num, (u32)rde_strlen(_num), _nb, 14.0, false, _i == _now || _lifted ? _t->accent : _t->text_soft);
    }
}

// See page_parts.h. Drawing it also moves it on: a page held still for FUDE_ZOOM_PAGE_STRIP_HOLD is lifted, and the
// strip scrolls while a lifted page is dragged near either end. The strip closes when the canvas has no pages.
void fude_zoom_page_render_strip(fude_zoom_page* _page) {
    if(!_page->strip_open || _page->presenting || !_page->open) {
        return;
    }
    if(!_page->pages.on) {
        _page->strip_open = false;
        return;
    }
    fude_zoom_page_strip_layout _l;
    fude_zoom_page_strip_lay(_page, &_l);
    if(_page->strip_pick != FUDE_ZOOM_NONE && _page->strip_pick >= _page->pages.count) {
        _page->strip_pick   = FUDE_ZOOM_NONE;
        _page->strip_lifted = false;
    }
    // A page held still long enough: lifted, to be dragged to its place (render on demand: a hand held still sends
    // nothing, so the hold's end is a timer).
    if(_page->strip_hand != 0u && !_page->strip_dragged && !_page->strip_lifted && rde_engine_get_time_now() - _page->strip_down_at < FUDE_ZOOM_PAGE_STRIP_HOLD) {
        fude_redraw_after(FUDE_ZOOM_PAGE_STRIP_HOLD - (rde_engine_get_time_now() - _page->strip_down_at));
    }
    if(_page->strip_hand != 0u && !_page->strip_dragged && !_page->strip_lifted && rde_engine_get_time_now() - _page->strip_down_at >= FUDE_ZOOM_PAGE_STRIP_HOLD) {
        const u32 _under = fude_zoom_page_strip_under(_page, &_l, _page->strip_down);
        if(_under != FUDE_ZOOM_NONE) {
            _page->strip_lifted = true;
            _page->strip_pick   = _under;
            _page->strip_drop   = _under;
            _page->strip_at     = _page->strip_down;
        }
    }
    // (dragged near an end: the strip scrolls on under it)
    if(_page->strip_lifted) {
        const f64 _x = (f64)_page->strip_at.x;
        const f64 _push = _x < _l.inner.min_x + FUDE_ZOOM_PAGE_STRIP_EDGE ? -8.0 : _x > _l.inner.max_x - FUDE_ZOOM_PAGE_STRIP_EDGE ? 8.0 : 0.0;
        _page->strip_scroll += _push;
        _page->strip_drop    = fude_zoom_page_strip_place(_page, &_l, _x);
        if(_push != 0.0) {
            fude_redraw_changed();   // (render on demand: it scrolls on every frame, the hand still)
        }
    }
    _page->strip_scroll = rde_math_min_f64(rde_math_max_f64(_page->strip_scroll, 0.0), fude_zoom_page_strip_most(_page, &_l));
    const fude_theme* _t = fude_theme_active();
    const fude_zoom_box _p = _l.panel;
    rde_rendering_2d_draw_rounded_rectangle_with_border((rde_vec_2F){ (f32)((_p.min_x + _p.max_x) * 0.5), (f32)((_p.min_y + _p.max_y) * 0.5) },
                                                        (rde_vec_2F){ (f32)(_p.max_x - _p.min_x), (f32)(_p.max_y - _p.min_y) }, 0.05f, 8u, _t->surface, 1.0f, _t->outline, NULL);
    const u32 _now = fude_zoom_page_page_now(_page);
    for(u32 _i = 0; _i < _page->pages.count; _i++) {
        // (one dragged: the rest closed up over its place and opened where it would go)
        f64 _slot = (f64)_i;
        if(_page->strip_lifted) {
            if(_i == _page->strip_pick) {
                continue;
            }
            const u32 _d = _i < _page->strip_pick ? _i : _i - 1u;
            _slot = (f64)(_d >= _page->strip_drop ? _d + 1u : _d);
        }
        const fude_zoom_box _b = fude_zoom_page_strip_slot(_page, &_l, _slot);
        if(_b.max_x <= _l.inner.min_x || _b.min_x >= _l.inner.max_x) {
            continue;
        }
        fude_zoom_page_strip_picture(_page, &_l, _i, _b, _now, false);
    }
    {
        const fude_zoom_box _b = _l.close;
        const rde_vec_2F _c = { (f32)((_b.min_x + _b.max_x) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) };
        rde_rendering_2d_draw_circle_with_border(_c, 16.0f, 24u, _t->surface_2, 1.0f, _t->outline, NULL);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - 5.0f, _c.y - 5.0f }, (rde_vec_2F){ _c.x + 5.0f, _c.y + 5.0f }, _t->text, 1.6f);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - 5.0f, _c.y + 5.0f }, (rde_vec_2F){ _c.x + 5.0f, _c.y - 5.0f }, _t->text, 1.6f);
    }
    // The page dragged: under the hand, held up.
    if(_page->strip_lifted) {
        const f64 _mx = (f64)_page->strip_at.x, _y = 10.0;
        const fude_zoom_box _b = { _mx - _l.tw * 0.5, _l.inner.max_y - _l.th + _y, _mx + _l.tw * 0.5, _l.inner.max_y + _y };
        fude_zoom_page_strip_picture(_page, &_l, _page->strip_pick, _b, _now, true);
    }
    // A page held and let go: its buttons over it.
    fude_zoom_box _chips[3];
    if(fude_zoom_page_strip_chips(_page, &_l, _chips)) {
        const c8* _words[3] = { fude_text(FUDE_TEXT_ZOOM_PAGE_BEFORE), fude_text(FUDE_TEXT_ZOOM_PAGE_AFTER), fude_text(FUDE_TEXT_ZOOM_PAGE_DELETE) };
        for(u32 _k = 0; _k < 3u; _k++) {
            const fude_zoom_box _b = _chips[_k];
            const rde_vec_2F _c = { (f32)((_b.min_x + _b.max_x) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) }, _size = { (f32)(_b.max_x - _b.min_x), (f32)(_b.max_y - _b.min_y) };
            rde_rendering_2d_draw_rounded_rectangle_with_border(_c, _size, 0.5f, 8u, _t->surface_2, 1.0f, _k == 2u ? _t->danger : _t->outline, NULL);
            fude_zoom_render_text_in(&_page->renderer, _words[_k], (u32)rde_strlen(_words[_k]), _b, 15.0, false, _k == 2u ? _t->danger : _t->text);
        }
    }
}

// See page_parts.h. A touch off the strip and off a held page's buttons puts the buttons away and is not taken; one
// on it stops the camera's flight.
b8 fude_zoom_page_strip_down(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand) {
    if(!_page->strip_open || _page->presenting || _page->strip_hand != 0u) {
        return false;
    }
    fude_zoom_page_strip_layout _l;
    fude_zoom_page_strip_lay(_page, &_l);
    fude_zoom_box _chips[3];
    const b8 _on_chip = fude_zoom_page_strip_chips(_page, &_l, _chips) &&
                        (fude_zoom_page_box_holds(_chips[0], _at) || fude_zoom_page_box_holds(_chips[1], _at) || fude_zoom_page_box_holds(_chips[2], _at));
    if(!_on_chip && !fude_zoom_page_box_holds(_l.panel, _at)) {
        _page->strip_pick = FUDE_ZOOM_NONE;   // (a touch off it: the held page's buttons put away)
        return false;
    }
    _page->strip_hand        = _hand;
    _page->strip_down        = _at;
    _page->strip_at          = _at;
    _page->strip_down_at     = rde_engine_get_time_now();
    _page->strip_scroll_down = _page->strip_scroll;
    _page->strip_dragged     = false;
    _page->strip_lifted      = false;
    _page->flight.active     = false;
    return true;
}

// Dragged along: scrolled with the hand — or, a page held, that page carried.
b8 fude_zoom_page_strip_moved(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand) {
    if(_page->strip_hand != _hand || _hand == 0u) {
        return false;
    }
    _page->strip_at = _at;
    if(_page->strip_lifted) {
        return true;
    }
    if(!_page->strip_dragged && rde_math_abs_f32(_at.x - _page->strip_down.x) + rde_math_abs_f32(_at.y - _page->strip_down.y) < 8.0f) {
        return true;
    }
    fude_zoom_page_strip_layout _l;
    fude_zoom_page_strip_lay(_page, &_l);
    _page->strip_dragged = true;
    _page->strip_pick    = FUDE_ZOOM_NONE;
    _page->strip_scroll  = rde_math_min_f64(rde_math_max_f64(_page->strip_scroll_down - (f64)(_at.x - _page->strip_down.x), 0.0), fude_zoom_page_strip_most(_page, &_l));
    return true;
}

// Let go: a page carried put where it is over; a tap — a held page's button, the ×, or a page (the view flown to it,
// whole).
b8 fude_zoom_page_strip_up(fude_zoom_page* _page, u64 _hand) {
    if(_page->strip_hand != _hand || _hand == 0u) {
        return false;
    }
    _page->strip_hand = 0u;
    if(_page->strip_lifted) {
        _page->strip_lifted = false;
        const u32 _from = _page->strip_pick, _to = _page->strip_drop;
        if(_from != FUDE_ZOOM_NONE && _to != _from && fude_zoom_page_pages_do(_page, FUDE_ZOOM_PAGES_MOVE, _from, _to)) {
            c8 _line[160];
            FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_PAGE_MOVED, FUDE_TN(_from + 1u), FUDE_TN(_to + 1u));
            fude_notice_show(_line);
            _page->strip_pick = FUDE_ZOOM_NONE;
        }
        return true;   // (let go where it was: its buttons over it)
    }
    if(_page->strip_dragged) {
        return true;
    }
    fude_zoom_page_strip_layout _l;
    fude_zoom_page_strip_lay(_page, &_l);
    const rde_vec_2F _at = _page->strip_down;
    fude_zoom_box _chips[3];
    if(fude_zoom_page_strip_chips(_page, &_l, _chips)) {
        const u32 _pick = _page->strip_pick;
        for(u32 _k = 0; _k < 3u; _k++) {
            if(!fude_zoom_page_box_holds(_chips[_k], _at)) {
                continue;
            }
            _page->strip_pick = FUDE_ZOOM_NONE;
            if(_k == 2u) {
                if(fude_zoom_page_pages_do(_page, FUDE_ZOOM_PAGES_DELETE, _pick, 0u)) {
                    c8 _line[200];
                    FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_PAGE_DELETED, FUDE_TN(_pick + 1u));
                    fude_notice_show(_line);
                }
            } else {
                const u32 _at_page = _k == 0u ? _pick : _pick + 1u;
                if(fude_zoom_page_pages_do(_page, FUDE_ZOOM_PAGES_INSERT, _at_page, 0u)) {
                    c8 _line[160];
                    FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_PAGES_ADDED, FUDE_TN(_at_page + 1u));
                    fude_notice_show(_line);
                } else if(_page->pages.count >= FUDE_ZOOM_PAGES_MOST) {
                    c8 _line[200];   // (no page put in: the notebook has the most it can, said, not silent)
                    FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_PAGES_FULL, FUDE_TN(FUDE_ZOOM_PAGES_MOST));
                    fude_notice_show(_line);
                }
            }
            return true;
        }
    }
    _page->strip_pick = FUDE_ZOOM_NONE;
    if(fude_zoom_page_box_holds(_l.close, _at)) {
        _page->strip_open = false;
        return true;
    }
    const u32 _i = fude_zoom_page_strip_under(_page, &_l, _at);
    if(_i != FUDE_ZOOM_NONE) {
        fude_zoom_page_fly(_page, fude_zoom_nav_framing(fude_zoom_page_half(_page), _page->scene.home, fude_zoom_pages_box(&_page->pages, _i), 0.94), true);
    }
    return true;
}

// Where Find's panel of other canvases and its parts are (screen, centre origin, Y up): the panel, its ×, and the box
// its rows scroll in.
typedef struct {
    fude_zoom_box panel, close, rows;
} fude_zoom_page_everywhere_layout;

// Where its panel is: as the strip is, over the row at the bottom left, as tall as its rows.
RDE_INTERNAL void fude_zoom_page_everywhere_lay(const fude_zoom_page* _page, fude_zoom_page_everywhere_layout* _l) {
    fude_zoom_page_strip_layout _s;
    fude_zoom_page_strip_lay(_page, &_s);
    const u32 _n = (u32)rde_arr_length(&_page->everywhere_found);
    const u32 _shown = _n < FUDE_ZOOM_PAGE_EVERYWHERE_ROWS ? (_n > 0u ? _n : 1u) : FUDE_ZOOM_PAGE_EVERYWHERE_ROWS;
    const f64 _bottom = _s.panel.min_y, _top = _bottom + 2.0 * FUDE_ZOOM_PAGE_STRIP_PAD + FUDE_ZOOM_PAGE_EVERYWHERE_HEAD + (f64)_shown * FUDE_ZOOM_PAGE_EVERYWHERE_ROW;
    const f64 _right = rde_math_min_f64(_s.panel.max_x, _s.panel.min_x + 620.0);
    _l->panel = (fude_zoom_box){ _s.panel.min_x, _bottom, _right, _top };
    _l->close = (fude_zoom_box){ _right - FUDE_ZOOM_PAGE_STRIP_CLOSE, _top - 44.0, _right - 8.0, _top - 8.0 };
    _l->rows  = (fude_zoom_box){ _s.panel.min_x + FUDE_ZOOM_PAGE_STRIP_PAD, _bottom + FUDE_ZOOM_PAGE_STRIP_PAD, _right - FUDE_ZOOM_PAGE_STRIP_PAD,
                                 _top - FUDE_ZOOM_PAGE_STRIP_PAD - FUDE_ZOOM_PAGE_EVERYWHERE_HEAD };
}

// How far its rows can be scrolled.
RDE_INTERNAL f64 fude_zoom_page_everywhere_most(const fude_zoom_page* _page, const fude_zoom_page_everywhere_layout* _l) {
    return rde_math_max_f64((f64)rde_arr_length(&_page->everywhere_found) * FUDE_ZOOM_PAGE_EVERYWHERE_ROW - (_l->rows.max_y - _l->rows.min_y), 0.0);
}

// See page_parts.h. A few more canvases are looked through first (fude_zoom_page_everywhere_step), which may find
// nothing and put the panel away. A row is drawn only when it is wholly inside the panel.
void fude_zoom_page_render_everywhere(fude_zoom_page* _page) {
    if(!_page->everywhere_shown || _page->presenting || !_page->open) {
        return;
    }
    if(_page->everywhere_on) {
        fude_redraw_changed();   // (render on demand: it looks through a few more canvases each frame)
    }
    fude_zoom_page_everywhere_step(_page);
    if(!_page->everywhere_shown) {
        return;
    }
    fude_zoom_page_everywhere_layout _l;
    fude_zoom_page_everywhere_lay(_page, &_l);
    _page->everywhere_scroll = rde_math_min_f64(rde_math_max_f64(_page->everywhere_scroll, 0.0), fude_zoom_page_everywhere_most(_page, &_l));
    const fude_theme* _t = fude_theme_active();
    const fude_zoom_box _p = _l.panel;
    rde_rendering_2d_draw_rounded_rectangle_with_border((rde_vec_2F){ (f32)((_p.min_x + _p.max_x) * 0.5), (f32)((_p.min_y + _p.max_y) * 0.5) },
                                                        (rde_vec_2F){ (f32)(_p.max_x - _p.min_x), (f32)(_p.max_y - _p.min_y) }, 0.05f, 8u, _t->surface, 1.0f, _t->outline, NULL);
    // Its head: what it is, or how far it has looked.
    c8 _head[200];
    if(_page->everywhere_on) {
        FUDE_TEXTF(_head, FUDE_TEXT_ZOOM_FIND_SEARCHING_ALL, FUDE_TN(_page->everywhere_done), FUDE_TN(_page->everywhere_total));
    } else {
        rde_snprintf(_head, sizeof(_head), "%s", fude_text(FUDE_TEXT_ZOOM_FIND_ELSEWHERE));
    }
    const fude_zoom_box _hb = { _l.rows.min_x, _l.rows.max_y, _l.close.min_x - 8.0, _p.max_y - FUDE_ZOOM_PAGE_STRIP_PAD };
    fude_zoom_render_text_at(&_page->renderer, _head, (u32)rde_strlen(_head), _hb, 16.0, false, true, _t->text_soft);
    {
        const fude_zoom_box _b = _l.close;
        const rde_vec_2F _c = { (f32)((_b.min_x + _b.max_x) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) };
        rde_rendering_2d_draw_circle_with_border(_c, 16.0f, 24u, _t->surface_2, 1.0f, _t->outline, NULL);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - 5.0f, _c.y - 5.0f }, (rde_vec_2F){ _c.x + 5.0f, _c.y + 5.0f }, _t->text, 1.6f);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - 5.0f, _c.y + 5.0f }, (rde_vec_2F){ _c.x + 5.0f, _c.y - 5.0f }, _t->text, 1.6f);
    }
    // Its canvases: each one's name and matches, the line of its first under them (those wholly inside it shown).
    const fude_zoom_page_found* _f = (const fude_zoom_page_found*)_page->everywhere_found.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->everywhere_found); _i++) {
        const f64 _top = _l.rows.max_y - (f64)_i * FUDE_ZOOM_PAGE_EVERYWHERE_ROW + _page->everywhere_scroll;
        const fude_zoom_box _r = { _l.rows.min_x, _top - FUDE_ZOOM_PAGE_EVERYWHERE_ROW + 4.0, _l.rows.max_x, _top - 2.0 };
        if(_r.max_y > _l.rows.max_y + 0.5 || _r.min_y < _l.rows.min_y - 0.5) {
            continue;
        }
        rde_rendering_2d_draw_rounded_rectangle_with_border((rde_vec_2F){ (f32)((_r.min_x + _r.max_x) * 0.5), (f32)((_r.min_y + _r.max_y) * 0.5) },
                                                            (rde_vec_2F){ (f32)(_r.max_x - _r.min_x), (f32)(_r.max_y - _r.min_y) }, 0.12f, 8u, _t->surface_2, 1.0f, _t->outline, NULL);
        c8 _count[64];
        FUDE_TEXTF(_count, FUDE_TEXT_ZOOM_FIND_MATCHES, FUDE_TN(_f[_i].count));
        const f64 _cw = _page->renderer.font != NULL ? (f64)fude_draw_text_width(_page->renderer.font, _page->renderer.font_px, _count, 14.0f) + 16.0 : 110.0;
        const f64 _mid = (_r.min_y + _r.max_y) * 0.5;
        fude_zoom_render_text_at(&_page->renderer, _f[_i].name, (u32)rde_strlen(_f[_i].name), (fude_zoom_box){ _r.min_x + 12.0, _mid, _r.max_x - _cw - 8.0, _r.max_y - 2.0 },
                                 16.0, false, true, _t->text);
        fude_zoom_render_text_at(&_page->renderer, _count, (u32)rde_strlen(_count), (fude_zoom_box){ _r.max_x - _cw, _mid, _r.max_x - 8.0, _r.max_y - 2.0 }, 14.0, false, true,
                                 _t->accent);
        fude_zoom_render_text_at(&_page->renderer, _f[_i].excerpt, (u32)rde_strlen(_f[_i].excerpt), (fude_zoom_box){ _r.min_x + 12.0, _r.min_y + 2.0, _r.max_x - 8.0, _mid },
                                 13.0, false, true, _t->text_soft);
    }
    if(rde_arr_length(&_page->everywhere_found) == 0u && !_page->everywhere_on) {
        const c8* _none = fude_text(FUDE_TEXT_ZOOM_FIND_NOWHERE_ELSE);
        fude_zoom_render_text_in(&_page->renderer, _none, (u32)rde_strlen(_none), _l.rows, 15.0, false, _t->text_soft);
    }
}

// See page_parts.h.
b8 fude_zoom_page_everywhere_down(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand) {
    if(!_page->everywhere_shown || _page->presenting || _page->everywhere_hand != 0u) {
        return false;
    }
    fude_zoom_page_everywhere_layout _l;
    fude_zoom_page_everywhere_lay(_page, &_l);
    if(!fude_zoom_page_box_holds(_l.panel, _at)) {
        return false;
    }
    _page->everywhere_hand        = _hand;
    _page->everywhere_down        = _at;
    _page->everywhere_scroll_down = _page->everywhere_scroll;
    _page->everywhere_dragged     = false;
    return true;
}

// See page_parts.h.
b8 fude_zoom_page_everywhere_moved(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand) {
    if(_page->everywhere_hand != _hand || _hand == 0u) {
        return false;
    }
    if(!_page->everywhere_dragged && rde_math_abs_f32(_at.x - _page->everywhere_down.x) + rde_math_abs_f32(_at.y - _page->everywhere_down.y) < 8.0f) {
        return true;
    }
    fude_zoom_page_everywhere_layout _l;
    fude_zoom_page_everywhere_lay(_page, &_l);
    _page->everywhere_dragged = true;
    _page->everywhere_scroll  = rde_math_min_f64(rde_math_max_f64(_page->everywhere_scroll_down + (f64)(_at.y - _page->everywhere_down.y), 0.0), fude_zoom_page_everywhere_most(_page, &_l));
    return true;
}

// See page_parts.h. The words go into find_after, which Find looks for once the canvas opened is up (page_frame.c).
b8 fude_zoom_page_everywhere_up(fude_zoom_page* _page, u64 _hand) {
    if(_page->everywhere_hand != _hand || _hand == 0u) {
        return false;
    }
    _page->everywhere_hand = 0u;
    if(_page->everywhere_dragged) {
        return true;
    }
    fude_zoom_page_everywhere_layout _l;
    fude_zoom_page_everywhere_lay(_page, &_l);
    const rde_vec_2F _at = _page->everywhere_down;
    if(fude_zoom_page_box_holds(_l.close, _at)) {
        _page->everywhere_shown = false;
        return true;
    }
    if(!fude_zoom_page_box_holds(_l.rows, _at)) {
        return true;
    }
    const u32 _i = (u32)rde_math_floor_f64(((_l.rows.max_y - (f64)_at.y) + _page->everywhere_scroll) / FUDE_ZOOM_PAGE_EVERYWHERE_ROW);
    if(_i >= (u32)rde_arr_length(&_page->everywhere_found) || _page->app->notes == NULL) {
        return true;
    }
    const fude_zoom_page_found _f = ((const fude_zoom_page_found*)_page->everywhere_found.memory)[_i];
    rde_snprintf(_page->find_after, sizeof(_page->find_after), "%s", _page->everywhere_query);
    fude_notes_open(_page->app->notes, _f.note);   // (the session opens it: then the words found in it)
    return true;
}

// See page.h.
f32 fude_zoom_page_notice_above(const fude_zoom_page* _page) {
    // (the strip's or Find's panel up: over it, and over a held page's buttons)
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    f64 _top = -_half.y;
    if(_page->open && _page->strip_open && _page->pages.on) {
        fude_zoom_page_strip_layout _l;
        fude_zoom_page_strip_lay(_page, &_l);
        _top = _l.panel.max_y + (_page->strip_pick != FUDE_ZOOM_NONE ? 8.0 + FUDE_ZOOM_PAGE_STRIP_CHIP_H : 0.0);
    }
    if(_page->open && _page->everywhere_shown) {
        fude_zoom_page_everywhere_layout _l;
        fude_zoom_page_everywhere_lay(_page, &_l);
        _top = rde_math_max_f64(_top, _l.panel.max_y);
    }
    return (f32)(_top + _half.y + 12.0);
}
