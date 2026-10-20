// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"
#include "drawing/base/redraw.h"

// ===========================================================================
// Page, the map, areas and Present (map.h): the map's panel, Insert's Named area, and presenting the areas
// in turn (with its laser).
//
// In order: the map's panel (its layout, its picture drawn off screen into a render texture when stale, drawing it,
// and its presses), Insert's Named area, then Present (its steps, the pill, the laser and the keys). Positions here
// are the screen's with the origin at its centre and Y up, unless a comment says otherwise.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// --- the map, areas and Present (map.h) -----------------------------------------------------------

#define FUDE_ZOOM_PAGE_MAP_W     440.0   // The map's panel at its widest (points).
#define FUDE_ZOOM_PAGE_MAP_HEAD  50.0    // Its top row: its name, everything framed, Present, its × (points).
#define FUDE_ZOOM_PAGE_MAP_PAD   10.0    // Its margin round the map (points).
#define FUDE_ZOOM_PAGE_MAP_PX    2.0     // Its picture's pixels a point.
#define FUDE_ZOOM_PAGE_LASER_LIFE 1.1    // Seconds a laser's trail takes to fade.

// Where the map's panel and its parts are on the screen.
typedef struct {
    fude_zoom_box panel, map, title, all, present, close;
} fude_zoom_page_map_layout;

// Lays the panel out (screen, centre origin, Y up): over the row of chips at the bottom left, from its left; its map
// the panel's width, as tall as the map's shape asks (map_aspect), within bounds and under the menu's button.
RDE_INTERNAL void fude_zoom_page_map_lay(const fude_zoom_page* _page, fude_zoom_page_map_layout* _l) {
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    const rde_vec_2F   _ui   = fude_kit_screen_size(_page->app->window);
    const b8  _placed = _page->places_center.x > 0.0f;   // The row's place, in the UI's units: bottom left origin.
    const f64 _left   = _placed ? (f64)(_page->places_center.x - FUDE_ZOOM_PAGE_UI_PLACES_W * 0.5f - _ui.x * 0.5f) : -_half.x + 16.0;
    const f64 _bottom = _placed ? (f64)(_page->places_center.y + FUDE_ZOOM_PAGE_UI_CHIP_H * 0.5f - _ui.y * 0.5f) + 12.0 : -_half.y + 76.0;
    const f64 _w      = rde_math_max_f64(200.0, rde_math_min_f64(FUDE_ZOOM_PAGE_MAP_W, _half.x * 2.0 - 150.0));
    const f64 _mw     = _w - 2.0 * FUDE_ZOOM_PAGE_MAP_PAD;
    const f64 _room   = (_half.y - 84.0) - _bottom - FUDE_ZOOM_PAGE_MAP_HEAD - FUDE_ZOOM_PAGE_MAP_PAD;
    const f64 _mh     = rde_math_max_f64(60.0, rde_math_min_f64(_mw * (_page->map_aspect > 0.0 ? _page->map_aspect : 0.75), _room));
    const f64 _top    = _bottom + FUDE_ZOOM_PAGE_MAP_PAD + _mh + FUDE_ZOOM_PAGE_MAP_HEAD;
    _l->panel   = (fude_zoom_box){ _left, _bottom, _left + _w, _top };
    _l->map     = (fude_zoom_box){ _left + FUDE_ZOOM_PAGE_MAP_PAD, _bottom + FUDE_ZOOM_PAGE_MAP_PAD, _left + _w - FUDE_ZOOM_PAGE_MAP_PAD, _bottom + FUDE_ZOOM_PAGE_MAP_PAD + _mh };
    _l->close   = (fude_zoom_box){ _left + _w - FUDE_ZOOM_PAGE_MAP_PAD - 36.0, _top - 43.0, _left + _w - FUDE_ZOOM_PAGE_MAP_PAD, _top - 7.0 };
    _l->present = (fude_zoom_box){ _l->close.min_x - 10.0 - 132.0, _top - 43.0, _l->close.min_x - 10.0, _top - 7.0 };
    // Everything drawn, framed.
    _l->all     = (fude_zoom_box){ _l->present.min_x - 10.0 - 36.0, _top - 43.0, _l->present.min_x - 10.0, _top - 7.0 };
    _l->title   = (fude_zoom_box){ _left + FUDE_ZOOM_PAGE_MAP_PAD + 4.0, _top - FUDE_ZOOM_PAGE_MAP_HEAD, _l->all.min_x - 8.0, _top };
}

// Returns where a point of the root frame (its units) is on the map on the screen.
RDE_INTERNAL rde_vec_2F fude_zoom_page_map_at(const fude_zoom_page* _page, const fude_zoom_page_map_layout* _l, fude_zoom_v2 _p) {
    const fude_zoom_box _w = _page->map_world;
    const f64 _kx = (_l->map.max_x - _l->map.min_x) / rde_math_max_f64(_w.max_x - _w.min_x, 1e-300);
    const f64 _ky = (_l->map.max_y - _l->map.min_y) / rde_math_max_f64(_w.max_y - _w.min_y, 1e-300);
    return (rde_vec_2F){ (f32)(_l->map.min_x + (_p.x - _w.min_x) * _kx), (f32)(_l->map.min_y + (_p.y - _w.min_y) * _ky) };
}

// Returns the point of the root frame (its units) under _at on the map, _at kept within the map.
RDE_INTERNAL fude_zoom_v2 fude_zoom_page_map_point(const fude_zoom_page* _page, const fude_zoom_page_map_layout* _l, rde_vec_2F _at) {
    const fude_zoom_box _w = _page->map_world;
    const f64 _x = rde_math_min_f64(rde_math_max_f64((f64)_at.x, _l->map.min_x), _l->map.max_x), _y = rde_math_min_f64(rde_math_max_f64((f64)_at.y, _l->map.min_y), _l->map.max_y);
    return (fude_zoom_v2){ _w.min_x + (_x - _l->map.min_x) / rde_math_max_f64(_l->map.max_x - _l->map.min_x, 1e-9) * (_w.max_x - _w.min_x),
                           _w.min_y + (_y - _l->map.min_y) / rde_math_max_f64(_l->map.max_y - _l->map.min_y, 1e-9) * (_w.max_y - _w.min_y) };
}

// Returns what is drawn, for the map: the scene's, and over a PDF its pages (root units).
RDE_INTERNAL fude_zoom_box fude_zoom_page_map_contents(const fude_zoom_page* _page) {
    const fude_zoom_scene* _s   = &_page->scene;
    fude_zoom_box          _all = fude_zoom_map_contents(_s);
    if(fude_zoom_pdfview_is_open(&_page->pdf) && _page->pdf.page_count > 0u) {
        const fude_zoom_sim _to = fude_zoom_scene_sim(_s, _s->home, _s->root);
        for(u32 _i = 0; _i < _page->pdf.page_count; _i++) {
            _all = fude_zoom_box_union(_all, fude_zoom_sim_box(_to, fude_zoom_pdfview_pages(&_page->pdf)[_i].box));
        }
    }
    return _all;
}

// Opens the map: what is drawn worked out, its shape set from what is drawn and the view together (its height over
// its width, kept between 0.5 and 1.15, so a long row of things gives a wide map), its picture wanted, and Places and
// the depth's levels closed.
void fude_zoom_page_map_open(fude_zoom_page* _page) {
    fude_zoom_scene* _s = &_page->scene;
    _page->map_contents     = fude_zoom_page_map_contents(_page);
    _page->map_contents_rev = _s->revision;
    const fude_zoom_box _view = fude_zoom_map_view(_s, fude_zoom_page_half(_page));
    const fude_zoom_box _all  = fude_zoom_box_is_empty(_page->map_contents) ? _view : fude_zoom_box_union(_page->map_contents, _view);
    _page->map_aspect = rde_math_min_f64(rde_math_max_f64((_all.max_y - _all.min_y) / rde_math_max_f64(_all.max_x - _all.min_x, 1e-300), 0.5), 1.15);
    _page->map_open   = true;
    _page->map_world  = fude_zoom_box_empty();
    _page->map_stale  = true;
    _page->map_ready  = false;
    _page->places_open = false;
    _page->crumbs_open = false;
}

// Once a frame while the map is open: what it shows worked out again when it has to. What is drawn is looked at again
// when the scene changed and its picture was drawn 0.4 s ago or more; the root's box it shows when the view has moved
// out of it by more than 3% of its width (at most every 0.2 s once it has a picture); and the theme's paper. Any of
// them makes it stale: its picture is drawn again off screen.
void fude_zoom_page_map_refresh(fude_zoom_page* _page) {
    if(!_page->map_open || !_page->open) {
        return;
    }
    fude_zoom_scene* _s   = &_page->scene;
    const f64        _now = rde_engine_get_time_now();
    if(_page->map_contents_rev != _s->revision && _now - _page->map_drawn_at < 0.4) {
        fude_redraw_after(0.4 - (_now - _page->map_drawn_at));   // (render on demand: looked at again then)
    }
    if(_page->map_contents_rev != _s->revision && _now - _page->map_drawn_at >= 0.4) {
        _page->map_contents     = fude_zoom_page_map_contents(_page);
        _page->map_contents_rev = _s->revision;
        _page->map_stale        = true;
    }
    fude_zoom_page_map_layout _l;
    fude_zoom_page_map_lay(_page, &_l);
    const f64 _aspect = (_l.map.max_y - _l.map.min_y) / rde_math_max_f64(_l.map.max_x - _l.map.min_x, 1e-9);
    const fude_zoom_box _want = fude_zoom_map_world(_page->map_contents, fude_zoom_map_view(_s, fude_zoom_page_half(_page)), _aspect);
    const fude_zoom_box _was  = _page->map_world;
    const f64 _ww   = fude_zoom_box_is_empty(_was) ? 0.0 : _was.max_x - _was.min_x;
    const b8  _off  = _ww <= 0.0 || rde_math_abs_f64(_want.min_x - _was.min_x) > 0.03 * _ww || rde_math_abs_f64(_want.max_x - _was.max_x) > 0.03 * _ww ||
                      rde_math_abs_f64(_want.min_y - _was.min_y) > 0.03 * _ww || rde_math_abs_f64(_want.max_y - _was.max_y) > 0.03 * _ww;
    if(_off && _now - _page->map_world_at < 0.2 && _page->map_ready) {
        fude_redraw_after(0.2 - (_now - _page->map_world_at));   // (render on demand: followed then)
    }
    if(_off && (_now - _page->map_world_at >= 0.2 || !_page->map_ready)) {
        _page->map_world    = _want;
        _page->map_world_at = _now;
        _page->map_stale    = true;
    }
    const rde_color _paper = fude_theme_active()->page;
    if(rde_memcmp(&_paper, &_page->map_paper, sizeof(_paper)) != 0) {
        _page->map_stale = true;
    }
    if(_page->map_stale) {
        fude_redraw_after(0.0);   // (render on demand: its picture is drawn off screen in the next frame)
    }
}

// Off screen, once stale: the canvas from above into the map's picture, a render texture of the map's size at
// FUDE_ZOOM_PAGE_MAP_PX pixels a point, made again when that size changes. The camera is put on the root for the one
// draw and the page's put back after; guides are left out.
void fude_zoom_page_map_draw(fude_zoom_page* _page, rde_window* _window, rde_camera* _camera) {
    if(!_page->open || !_page->map_open || !_page->map_stale || fude_zoom_box_is_empty(_page->map_world)) {
        return;
    }
    fude_zoom_page_map_layout _l;
    fude_zoom_page_map_lay(_page, &_l);
    const u32 _pw = (u32)rde_math_max_f64(8.0, rde_math_round_f64((_l.map.max_x - _l.map.min_x) * FUDE_ZOOM_PAGE_MAP_PX));
    const u32 _ph = (u32)rde_math_max_f64(8.0, rde_math_round_f64((_l.map.max_y - _l.map.min_y) * FUDE_ZOOM_PAGE_MAP_PX));
    if(_page->map_picture != NULL && (_pw != _page->map_pw || _ph != _page->map_ph)) {
        rde_render_texture_unload(_page->map_picture);
        _page->map_picture = NULL;
    }
    if(_page->map_picture == NULL) {
        _page->map_picture = rde_render_texture_load(_pw, _ph, NULL);
        if(_page->map_picture == NULL) {
            return;
        }
        _page->map_pw = _pw;
        _page->map_ph = _ph;
    }
    fude_zoom_scene*       _s    = &_page->scene;
    const fude_zoom_camera _was  = _s->camera;
    const fude_zoom_box    _w    = _page->map_world;
    const fude_zoom_v2     _half = { (f64)_pw * 0.5, (f64)_ph * 0.5 };
    const rde_color        _bg   = fude_theme_active()->page;
    // Only for this draw.
    _s->camera = (fude_zoom_camera){ _s->root, { (_w.min_x + _w.max_x) * 0.5, (_w.min_y + _w.max_y) * 0.5 }, (f64)_pw / rde_math_max_f64(_w.max_x - _w.min_x, 1e-300) };
    rde_render_texture_enable(_page->map_picture);
    rde_rendering_2d_begin_drawing(_window, _camera);
    rde_rendering_2d_draw_rectangle((rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ (f32)_pw + 4.0f, (f32)_ph + 4.0f }, _bg);
    // A PDF's pages, under the ink.
    fude_zoom_pdfview_render(&_page->pdf, fude_zoom_page_frame_sim(_page, _s->home), _half);
    _page->renderer.no_guides = true;   // A picture of the page: no guides.
    fude_zoom_render(&_page->renderer, _s, _half, _bg);
    _page->renderer.no_guides = false;
    rde_rendering_2d_end_drawing();
    rde_render_texture_disable();
    _s->camera = _was;
    _page->map_stale    = false;
    _page->map_ready    = true;
    _page->map_drawn_at = rde_engine_get_time_now();
    _page->map_paper    = _bg;
}

// Draws a rectangle (its outline _width wide, and its fill unless clear) between corners _a and _b, as much of it as is
// in _clip.
RDE_INTERNAL void fude_zoom_page_map_box(rde_vec_2F _a, rde_vec_2F _b, fude_zoom_box _clip, f32 _width, rde_color _line, rde_color _fill) {
    const f32 _x0 = rde_math_max_f32(rde_math_min_f32(_a.x, _b.x), (f32)_clip.min_x), _x1 = rde_math_min_f32(rde_math_max_f32(_a.x, _b.x), (f32)_clip.max_x);
    const f32 _y0 = rde_math_max_f32(rde_math_min_f32(_a.y, _b.y), (f32)_clip.min_y), _y1 = rde_math_min_f32(rde_math_max_f32(_a.y, _b.y), (f32)_clip.max_y);
    if(_x1 <= _x0 || _y1 <= _y0) {
        return;
    }
    const rde_vec_2F _c = { (_x0 + _x1) * 0.5f, (_y0 + _y1) * 0.5f }, _z = { _x1 - _x0, _y1 - _y0 };
    if(_fill.a > 0) {
        rde_rendering_2d_draw_rectangle(_c, _z, _fill);
    }
    rde_rendering_2d_draw_rectangle_border(_c, _z, _width, _line, NULL);
}

// Draws the panel over the page: its name, everything framed, Present and its ×; then the map: its picture, the areas
// outlined and named, the places' pins, and the view (or, too small to see, a ring where it is). The areas' names and
// the pins are kept for the presses.
void fude_zoom_page_render_map(fude_zoom_page* _page) {
    _page->map_label_n = 0;
    _page->map_pin_n   = 0;
    if(!_page->map_open || _page->presenting || !_page->open) {
        return;
    }
    fude_zoom_page_map_layout _l;
    fude_zoom_page_map_lay(_page, &_l);
    const fude_theme* _t = fude_theme_active();
    const fude_zoom_box _p = _l.panel;
    rde_rendering_2d_draw_rounded_rectangle_with_border((rde_vec_2F){ (f32)((_p.min_x + _p.max_x) * 0.5), (f32)((_p.min_y + _p.max_y) * 0.5) },
                                                        (rde_vec_2F){ (f32)(_p.max_x - _p.min_x), (f32)(_p.max_y - _p.min_y) }, 0.05f, 8u, _t->surface, 1.0f, _t->outline, NULL);
    // Its name; Present; its ×.
    const c8* _name = fude_text(FUDE_TEXT_ZOOM_MAP);
    fude_zoom_render_text_at(&_page->renderer, _name, (u32)rde_strlen(_name), _l.title, 17.0, false, true, _t->text);
    {
        const fude_zoom_box _b = _l.present;
        rde_rendering_2d_draw_rounded_rectangle_with_border((rde_vec_2F){ (f32)((_b.min_x + _b.max_x) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) },
                                                            (rde_vec_2F){ (f32)(_b.max_x - _b.min_x), (f32)(_b.max_y - _b.min_y) }, 0.45f, 8u, _t->accent, 1.0f, _t->accent, NULL);
        c8 _say[96];
        rde_snprintf(_say, sizeof(_say), FUDE_ICON_PRESENT "  %s", fude_text(FUDE_TEXT_ZOOM_PRESENT));
        fude_zoom_render_text_in(&_page->renderer, _say, (u32)rde_strlen(_say), _b, 14.0, false, fude_kit_text_on(_t->accent));
    }
    {
        const fude_zoom_box _b = _l.close;
        const rde_vec_2F _c = { (f32)((_b.min_x + _b.max_x) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) };
        rde_rendering_2d_draw_circle_with_border(_c, 16.0f, 24u, _t->surface_2, 1.0f, _t->outline, NULL);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - 5.0f, _c.y - 5.0f }, (rde_vec_2F){ _c.x + 5.0f, _c.y + 5.0f }, _t->text, 1.6f);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - 5.0f, _c.y + 5.0f }, (rde_vec_2F){ _c.x + 5.0f, _c.y - 5.0f }, _t->text, 1.6f);
    }
    {
        // Everything drawn, framed: its corners' brackets.
        const fude_zoom_box _b = _l.all;
        const rde_vec_2F _c = { (f32)((_b.min_x + _b.max_x) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) };
        rde_rendering_2d_draw_circle_with_border(_c, 16.0f, 24u, _t->surface_2, 1.0f, _t->outline, NULL);
        fude_zoom_render_text_in(&_page->renderer, FUDE_ICON_FULLSCREEN, (u32)rde_strlen(FUDE_ICON_FULLSCREEN), _b, 16.0, false, _t->text);
    }
    // Its picture (as last drawn: a moment behind a change), on the page's paper until it is.
    const fude_zoom_box _m  = _l.map;
    const rde_vec_2F    _mc = { (f32)((_m.min_x + _m.max_x) * 0.5), (f32)((_m.min_y + _m.max_y) * 0.5) };
    const rde_vec_2F    _ms = { (f32)(_m.max_x - _m.min_x), (f32)(_m.max_y - _m.min_y) };
    rde_rendering_2d_draw_rectangle(_mc, _ms, _t->page);
    const rde_texture* _tex = _page->map_ready && _page->map_picture != NULL ? rde_render_texture_get_texture(_page->map_picture) : NULL;
    if(_tex != NULL) {
        const rde_vec_2F _all = { (f32)_page->map_pw, (f32)_page->map_ph };
        // Its rows bottom up.
        rde_rendering_2d_draw_texture_partial_2(_tex, (rde_vec_3F){ _mc.x, _mc.y, 0.0f }, (rde_vec_2F){ _ms.x / _all.x, -_ms.y / _all.y },
                                                0.0f, (rde_color){ 255, 255, 255, 255 }, (rde_vec_2F){ 0.0f, 0.0f }, _all);
    }
    rde_rendering_2d_draw_rectangle_border(_mc, _ms, 1.0f, _t->outline, NULL);
    if(fude_zoom_box_is_empty(_page->map_world)) {
        return;
    }
    fude_zoom_scene* _s = &_page->scene;
    // The areas: outlined, named at their top left (the name kept in the map).
    fude_zoom_map_area _areas[FUDE_ZOOM_MAP_AREAS];
    const u32 _na = fude_zoom_map_areas(_s, _areas, FUDE_ZOOM_MAP_AREAS);
    rde_color _soft = _t->accent;
    _soft.a = 28;
    for(u32 _i = 0; _i < _na; _i++) {
        const rde_vec_2F _a = fude_zoom_page_map_at(_page, &_l, (fude_zoom_v2){ _areas[_i].box.min_x, _areas[_i].box.min_y });
        const rde_vec_2F _b = fude_zoom_page_map_at(_page, &_l, (fude_zoom_v2){ _areas[_i].box.max_x, _areas[_i].box.max_y });
        fude_zoom_page_map_box(_a, _b, _m, 1.5f, _t->accent, _soft);
        if(_a.x < (f32)_m.min_x - 1.0f || _a.x > (f32)_m.max_x - 12.0f || _b.y > (f32)_m.max_y + 1.0f || _b.y < (f32)_m.min_y + 12.0f || _areas[_i].name[0] == 0) {
            continue;
        }
        const f32 _px = 12.0f;
        const f32 _tw = _page->renderer.font != NULL ? fude_draw_text_width(_page->renderer.font, _page->renderer.font_px, _areas[_i].name, _px) : 40.0f;
        const f32 _x0 = rde_math_max_f32(_a.x, (f32)_m.min_x) + 2.0f, _x1 = rde_math_min_f32(_x0 + _tw + 10.0f, (f32)_m.max_x - 2.0f);
        const f32 _y1 = rde_math_min_f32(_b.y, (f32)_m.max_y) - 2.0f, _y0 = _y1 - 18.0f;
        rde_rendering_2d_draw_rounded_rectangle_with_border((rde_vec_2F){ (_x0 + _x1) * 0.5f, (_y0 + _y1) * 0.5f }, (rde_vec_2F){ _x1 - _x0, _y1 - _y0 }, 0.3f, 6u,
                                                            _t->accent, 1.0f, _t->accent, NULL);
        fude_zoom_render_text_at(&_page->renderer, _areas[_i].name, (u32)rde_strlen(_areas[_i].name), (fude_zoom_box){ _x0 + 5.0, _y0, _x1, _y1 - 1.0 }, _px, false, true,
                                 fude_kit_text_on(_t->accent));
        if(_page->map_label_n < FUDE_ZOOM_MAP_AREAS) {
            _page->map_label_box[_page->map_label_n]    = (fude_zoom_box){ _x0 - 4.0, _y0 - 4.0, _x1 + 4.0, _y1 + 4.0 };
            _page->map_label_object[_page->map_label_n] = _areas[_i].object;
            _page->map_label_n++;
        }
    }
    // The places: a pin each, its number on it.
    fude_zoom_map_place _places[FUDE_ZOOM_MAP_PLACES];
    const u32 _np = fude_zoom_map_places(_s, _places, FUDE_ZOOM_MAP_PLACES);
    for(u32 _i = 0; _i < _np; _i++) {
        const rde_vec_2F _c = fude_zoom_page_map_at(_page, &_l, _places[_i].at);
        if(_c.x < (f32)_m.min_x || _c.x > (f32)_m.max_x || _c.y < (f32)_m.min_y || _c.y > (f32)_m.max_y) {
            continue;
        }
        rde_rendering_2d_draw_circle_with_border(_c, 9.0f, 20u, _t->surface, 1.5f, _t->accent, NULL);
        c8 _num[16];
        rde_snprintf(_num, sizeof(_num), "%u", _places[_i].number);
        fude_zoom_render_text_in(&_page->renderer, _num, (u32)rde_strlen(_num), (fude_zoom_box){ _c.x - 9.0, _c.y - 9.0, _c.x + 9.0, _c.y + 9.0 }, 10.0, false, _t->accent);
        _page->map_pin_at[_page->map_pin_n]     = _c;
        _page->map_pin_object[_page->map_pin_n] = _places[_i].object;
        _page->map_pin_n++;
    }
    // The view: its box, or — too small to see — a ring where it is.
    const fude_zoom_box _v = fude_zoom_map_view(_s, fude_zoom_page_half(_page));
    const rde_vec_2F _va = fude_zoom_page_map_at(_page, &_l, (fude_zoom_v2){ _v.min_x, _v.min_y });
    const rde_vec_2F _vb = fude_zoom_page_map_at(_page, &_l, (fude_zoom_v2){ _v.max_x, _v.max_y });
    rde_color _tint = _t->accent;
    _tint.a = 46;
    if(_vb.x - _va.x < 10.0f && _vb.y - _va.y < 10.0f) {
        const rde_vec_2F _c = { rde_math_min_f32(rde_math_max_f32((_va.x + _vb.x) * 0.5f, (f32)_m.min_x), (f32)_m.max_x), rde_math_min_f32(rde_math_max_f32((_va.y + _vb.y) * 0.5f, (f32)_m.min_y), (f32)_m.max_y) };
        rde_rendering_2d_draw_circle_with_border(_c, 9.0f, 24u, _tint, 2.0f, _t->accent, NULL);
        rde_rendering_2d_draw_circle(_c, 2.5f, 12u, _t->accent, NULL);
    } else {
        fude_zoom_page_map_box(_va, _vb, _m, 2.0f, _t->accent, _tint);
    }
}

// A hand down: on the panel, its own (a tap, or a drag of the view), and any flight stops; off it, the page's (the map
// stays). Returns true when it was taken.
b8 fude_zoom_page_map_down(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand) {
    if(!_page->map_open || _page->presenting || _page->map_hand != 0u) {
        return false;
    }
    fude_zoom_page_map_layout _l;
    fude_zoom_page_map_lay(_page, &_l);
    if(!fude_zoom_page_box_holds(_l.panel, _at)) {
        return false;
    }
    _page->map_hand    = _hand;
    _page->map_down    = _at;
    _page->map_dragged = false;
    _page->flight.active = false;
    if(fude_zoom_page_box_holds(_l.map, _at) && !fude_zoom_box_is_empty(_page->map_world)) {
        const fude_zoom_box _v  = fude_zoom_map_view(&_page->scene, fude_zoom_page_half(_page));
        const rde_vec_2F    _va = fude_zoom_page_map_at(_page, &_l, (fude_zoom_v2){ _v.min_x, _v.min_y });
        const rde_vec_2F    _vb = fude_zoom_page_map_at(_page, &_l, (fude_zoom_v2){ _v.max_x, _v.max_y });
        _page->map_on_view   = _at.x >= _va.x - 14.0f && _at.x <= _vb.x + 14.0f && _at.y >= _va.y - 14.0f && _at.y <= _vb.y + 14.0f;
        _page->map_grab      = fude_zoom_page_map_point(_page, &_l, _at);
        _page->map_grab_view = (fude_zoom_v2){ (_v.min_x + _v.max_x) * 0.5, (_v.min_y + _v.max_y) * 0.5 };
    }
    return true;
}

// The hand that pressed the map dragged, once it has gone 8 points: the view moved with it. A drag begun on the view
// (or within 14 points of it) moves it by as much as the hand moved; any other puts the view's middle under the hand.
// Returns true when it was that hand.
b8 fude_zoom_page_map_moved(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand) {
    if(_page->map_hand != _hand || _hand == 0u) {
        return false;
    }
    fude_zoom_page_map_layout _l;
    fude_zoom_page_map_lay(_page, &_l);
    if(!_page->map_dragged && rde_math_abs_f32(_at.x - _page->map_down.x) + rde_math_abs_f32(_at.y - _page->map_down.y) < 8.0f) {
        return true;
    }
    if(!fude_zoom_page_box_holds(_l.map, _page->map_down) || fude_zoom_box_is_empty(_page->map_world)) {
        return true;   // Begun on its top row: nothing to drag.
    }
    _page->map_dragged = true;
    fude_zoom_scene*   _s = &_page->scene;
    const fude_zoom_v2 _p = fude_zoom_page_map_point(_page, &_l, _at);
    const fude_zoom_v2 _to = _page->map_on_view ? (fude_zoom_v2){ _page->map_grab_view.x + _p.x - _page->map_grab.x, _page->map_grab_view.y + _p.y - _page->map_grab.y } : _p;
    _s->camera.at = fude_zoom_sim_apply(fude_zoom_scene_sim(_s, _s->root, _s->camera.frame), _to);
    fude_zoom_camera_settle(_s, fude_zoom_page_half(_page));
    return true;
}

// Let go: unless it was a drag, a tap on its ×, Present, everything framed, a place's pin (flown to), an area's name
// (framed), or anywhere on the map (flown there: map.h, as far in as makes sense). Returns true when it was that hand.
b8 fude_zoom_page_map_up(fude_zoom_page* _page, u64 _hand) {
    if(_page->map_hand != _hand || _hand == 0u) {
        return false;
    }
    _page->map_hand = 0u;
    if(_page->map_dragged) {
        return true;
    }
    fude_zoom_page_map_layout _l;
    fude_zoom_page_map_lay(_page, &_l);
    const rde_vec_2F _at = _page->map_down;
    fude_zoom_scene* _s  = &_page->scene;
    if(fude_zoom_page_box_holds(_l.close, _at)) {
        _page->map_open = false;
        return true;
    }
    if(fude_zoom_page_box_holds(_l.present, _at)) {
        fude_zoom_page_present_start(_page);
        return true;
    }
    if(fude_zoom_page_box_holds(_l.all, _at)) {
        // Everything drawn, filling most of the screen (seen from the top: whatever is deeper, where it is).
        if(!fude_zoom_box_is_empty(_page->map_contents)) {
            fude_zoom_page_fly(_page, fude_zoom_nav_framing(fude_zoom_page_half(_page), _s->root, _page->map_contents, 0.9), true);
        }
        return true;
    }
    if(!fude_zoom_page_box_holds(_l.map, _at) || fude_zoom_box_is_empty(_page->map_world)) {
        return true;
    }
    for(u32 _i = 0; _i < _page->map_pin_n; _i++) {
        const f32 _dx = _at.x - _page->map_pin_at[_i].x, _dy = _at.y - _page->map_pin_at[_i].y;
        f64 _z = 1.0;
        if(_dx * _dx + _dy * _dy <= 14.0f * 14.0f && fude_zoom_scene_mark_of(_s, _page->map_pin_object[_i], &_z, NULL)) {
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, _page->map_pin_object[_i]);
            fude_zoom_page_fly(_page, (fude_zoom_camera){ _o->frame, _o->t, _z }, true);
            return true;
        }
    }
    for(u32 _i = _page->map_label_n; _i-- > 0;) {
        if(fude_zoom_page_box_holds(_page->map_label_box[_i], _at) && fude_zoom_map_is_area(_s, _page->map_label_object[_i])) {
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, _page->map_label_object[_i]);
            fude_zoom_page_fly(_page, fude_zoom_nav_framing(fude_zoom_page_half(_page), _o->frame, _o->box, 0.9), true);
            return true;
        }
    }
    fude_zoom_page_fly(_page, fude_zoom_map_tap_view(_s, fude_zoom_page_half(_page), _page->map_world, fude_zoom_page_map_point(_page, &_l, _at)), true);
    return true;
}

// Frees the map's picture; it is made again when the map is next drawn.
void fude_zoom_page_map_free(fude_zoom_page* _page) {
    if(_page->map_picture != NULL) {
        rde_render_texture_unload(_page->map_picture);
        _page->map_picture = NULL;
    }
    _page->map_ready = false;
    _page->map_stale = true;
}

// --- areas: Insert's Named area ----------------------------------------------------------------------------------

// Insert's Named area: the text card asks for its name, "Area n" (n one more than the areas there are) to begin with,
// chosen, so typing replaces it.
void fude_zoom_page_area_card(fude_zoom_page* _page) {
    if(_page->text_field == NULL) {
        return;
    }
    fude_zoom_map_area _areas[FUDE_ZOOM_MAP_AREAS];
    const u32 _n = fude_zoom_map_areas(&_page->scene, _areas, FUDE_ZOOM_MAP_AREAS);
    c8 _name[FUDE_ZOOM_MAP_NAME];
    FUDE_TEXTF(_name, FUDE_TEXT_ZOOM_AREA_N, FUDE_TN(_n + 1u));
    const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_page->text_field, 0, _bytes);
    }
    rde_ui_text_editor_insert_at(_page->text_field, 0, _name, rde_strlen(_name));
    rde_ui_text_editor_set_placeholder(_page->text_field, fude_text(FUDE_TEXT_ZOOM_AREA_NAME));
    _page->text_object = FUDE_ZOOM_NONE;
    _page->text_style  = FUDE_ZOOM_PAGE_TEXT_AREA;
    fude_zoom_page_text_card(_page, true);
    rde_ui_text_editor_select_all(_page->text_field);
}

// The area made with the name typed (its first line, trimmed and cut to FUDE_ZOOM_MAP_NAME between characters; empty:
// "Area n"): round what the lasso holds (with room for its name over it), else the view a little in; an "area" symbol
// in the ink's colour, its name in its corner, in the frame drawn at its middle. One undo step, the lasso on it (to
// move it, to scale it), and the map drawn again.
void fude_zoom_page_area_commit(fude_zoom_page* _page, const c8* _text) {
    c8 _name[FUDE_ZOOM_MAP_NAME];
    const c8* _a = _text;
    while(*_a == ' ' || *_a == '\n') {
        _a++;
    }
    usize _len = rde_strcspn(_a, "\n");
    while(_len > 0 && _a[_len - 1u] == ' ') {
        _len--;
    }
    if(_len == 0) {
        fude_zoom_map_area _areas[FUDE_ZOOM_MAP_AREAS];
        FUDE_TEXTF(_name, FUDE_TEXT_ZOOM_AREA_N, FUDE_TN(fude_zoom_map_areas(&_page->scene, _areas, FUDE_ZOOM_MAP_AREAS) + 1u));
    } else {
        if(_len > sizeof(_name) - 1u) {
            _len = sizeof(_name) - 1u;
            while(_len > 0 && ((u8)_a[_len] & 0xC0u) == 0x80u) {
                _len--;
            }
        }
        rde_memcpy(_name, _a, _len);
        _name[_len] = 0;
    }
    fude_zoom_scene*   _s    = &_page->scene;
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    fude_zoom_box      _b;   // On the screen, centre origin.
    if(fude_zoom_select_any(&_page->selection) && fude_zoom_select_box(&_page->selection, &_b)) {
        const f64 _m = rde_math_max_f64(_b.max_x - _b.min_x, _b.max_y - _b.min_y) * 0.06 + 14.0;
        const f64 _px = rde_math_min_f64(rde_math_max_f64((_b.max_y - _b.min_y + 2.0 * _m) * 0.05, 15.0), 34.0);
        _b = (fude_zoom_box){ _b.min_x - _m, _b.min_y - _m, _b.max_x + _m, _b.max_y + _m + _px * 1.6 };
    } else {
        _b = (fude_zoom_box){ -_half.x * 0.84, -_half.y * 0.8, _half.x * 0.84, _half.y * 0.8 };
    }
    const fude_zoom_v2 _mid_screen = { (_b.min_x + _b.max_x) * 0.5, (_b.min_y + _b.max_y) * 0.5 };
    const fude_zoom_v2 _mid_cam    = fude_zoom_camera_to_frame(&_s->camera, _mid_screen);
    const f64          _near       = 4.0 / _s->camera.z;
    const u32          _frame      = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _mid_cam.x - _near, _mid_cam.y - _near, _mid_cam.x + _near, _mid_cam.y + _near });
    const fude_zoom_sim _fs = fude_zoom_page_frame_sim(_page, _frame);
    const f64           _k  = rde_math_max_f64(fude_zoom_sim_scale(_fs), 1e-300);
    const fude_zoom_v2  _c  = fude_zoom_sim_apply(fude_zoom_sim_inverse(_fs), _mid_screen);
    const f64 _px = rde_math_min_f64(rde_math_max_f64((_b.max_y - _b.min_y) * 0.05, 15.0), 34.0);
    f64 _num[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _count = fude_zoom_symbol_numbers(_num, fude_zoom_symbol_find("area"), (_b.max_x - _b.min_x) * 0.5 / _k, (_b.max_y - _b.min_y) * 0.5 / _k, _px / _k, _name);
    const u32 _made  = fude_zoom_scene_add_shape_fill(_s, _frame, (fude_zoom_place){ _c, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_SYMBOL, _num, _count, _page->app->ink->color,
                                                      (f32)(1.2 / _k), 0, (rde_color){ 0, 0, 0, 0 }, 0);
    fude_zoom_history_push(_s, _frame, fude_zoom_scene_object(_s, _made)->box, NULL, 0, &_made, 1);
    fude_zoom_select_clear(&_page->selection);
    rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _made });
    fude_zoom_select_update(&_page->selection, _s);
    c8 _say[200];
    FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_AREA_MADE, FUDE_TS(_name));
    fude_notice_show(_say);
    _page->map_stale = true;
}

// --- Present -----------------------------------------------------------------------------------------------------

// Flies to step _i: an area's box filling most of the screen, or a place as it was marked, flown 1.6 times slower than
// Back. A step whose object is gone is not flown to. The laser's trail is cleared.
void fude_zoom_page_present_go(fude_zoom_page* _page, u32 _i) {
    if(_i >= _page->present_count) {
        return;
    }
    fude_zoom_scene* _s = &_page->scene;
    const u32        _o = _page->present_objects[_i];
    if(_o >= fude_zoom_scene_object_count(_s) || !(fude_zoom_scene_object(_s, _o)->flags & FUDE_ZOOM_FLAG_ALIVE)) {
        return;
    }
    const fude_zoom_object* _obj = fude_zoom_scene_object(_s, _o);
    fude_zoom_camera _to;
    f64 _z = 1.0;
    if(_page->present_areas) {
        _to = fude_zoom_nav_framing(fude_zoom_page_half(_page), _obj->frame, _obj->box, 0.92);
    } else if(fude_zoom_scene_mark_of(_s, _o, &_z, NULL)) {
        _to = (fude_zoom_camera){ _obj->frame, _obj->t, _z };
    } else {
        return;
    }
    _page->present_step = _i;
    rde_ring_buffer_clear(&_page->laser);
    fude_zoom_page_fly(_page, _to, false);
    if(_page->flight.active) {
        _page->flight.length *= 1.6;
    }
}

// Present: the areas in turn, numbered names first (none: the places), from the area the view is in (the smallest
// round its middle), else the first; nothing but the page on the screen. With neither, a notice says so.
void fude_zoom_page_present_start(fude_zoom_page* _page) {
    fude_zoom_scene* _s = &_page->scene;
    fude_zoom_map_area _areas[FUDE_ZOOM_MAP_AREAS];
    u32 _n = fude_zoom_map_areas(_s, _areas, FUDE_ZOOM_MAP_AREAS);
    fude_zoom_map_areas_order(_areas, _n);   // Numbered names first, by their numbers.
    _page->present_areas = _n > 0u;
    for(u32 _i = 0; _i < _n; _i++) {
        _page->present_objects[_i] = _areas[_i].object;
    }
    if(_n == 0u) {
        fude_zoom_map_place _places[FUDE_ZOOM_MAP_PLACES];
        _n = fude_zoom_map_places(_s, _places, FUDE_ZOOM_MAP_PLACES);
        for(u32 _i = 0; _i < _n; _i++) {
            _page->present_objects[_i] = _places[_i].object;
        }
    }
    if(_n == 0u) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PRESENT_NONE));
        return;
    }
    // From the area the view is in (the smallest round its middle), else the first.
    u32 _from = 0;
    if(_page->present_areas) {
        const fude_zoom_box _v   = fude_zoom_map_view(_s, fude_zoom_page_half(_page));
        const fude_zoom_v2  _mid = { (_v.min_x + _v.max_x) * 0.5, (_v.min_y + _v.max_y) * 0.5 };
        f64 _least = 1e300;
        for(u32 _i = 0; _i < _n; _i++) {
            const fude_zoom_box _b = _areas[_i].box;
            const f64 _size = (_b.max_x - _b.min_x) * (_b.max_y - _b.min_y);
            if(_mid.x >= _b.min_x && _mid.x <= _b.max_x && _mid.y >= _b.min_y && _mid.y <= _b.max_y && _size < _least) {
                _least = _size;
                _from  = _i;
            }
        }
    }
    _page->present_count       = _n;
    _page->presenting          = true;
    _page->map_open            = false;
    _page->library_open        = false;
    _page->present_touched_at  = rde_engine_get_time_now();
    _page->present_pen_down    = false;
    rde_ring_buffer_clear(&_page->laser);
    fude_zoom_select_clear(&_page->selection);
    _page->app->ui->chrome_hidden = true;
    fude_ui_update(_page->app->ui);
    fude_zoom_page_present_go(_page, _from);
    fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PRESENT_HOW));
}

// Ends Present: the app's chrome back.
void fude_zoom_page_present_end(fude_zoom_page* _page) {
    if(!_page->presenting) {
        return;
    }
    _page->presenting  = false;
    rde_ring_buffer_clear(&_page->laser);
    _page->app->ui->chrome_hidden = false;
    fude_ui_update(_page->app->ui);
}

// Where the pill's parts are on the screen.
typedef struct {
    fude_zoom_box pill, prev, next, close;
} fude_zoom_page_present_layout;

// The pill at the bottom middle (screen, centre origin): ◀, where it is, ▶, ×.
RDE_INTERNAL void fude_zoom_page_present_lay(const fude_zoom_page* _page, fude_zoom_page_present_layout* _l) {
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    const f64 _w = 260.0, _h = 52.0, _y = -_half.y + 28.0 + _h * 0.5;
    _l->pill  = (fude_zoom_box){ -_w * 0.5, _y - _h * 0.5, _w * 0.5, _y + _h * 0.5 };
    _l->prev  = (fude_zoom_box){ -_w * 0.5 + 6.0, _y - 20.0, -_w * 0.5 + 46.0, _y + 20.0 };
    _l->next  = (fude_zoom_box){ _w * 0.5 - 98.0, _y - 20.0, _w * 0.5 - 58.0, _y + 20.0 };
    _l->close = (fude_zoom_box){ _w * 0.5 - 46.0, _y - 20.0, _w * 0.5 - 6.0, _y + 20.0 };
}

// Returns how much the pill shows, 0 to 1: fully for 3 s after a touch, then fading out over 0.6 s.
RDE_INTERNAL f32 fude_zoom_page_present_shown(const fude_zoom_page* _page) {
    const f64 _since = rde_engine_get_time_now() - _page->present_touched_at;
    return _since < 3.0 ? 1.0f : (f32)rde_math_max_f64(0.0, 1.0 - (_since - 3.0) / 0.6);
}

// A tap while presenting: the pill's buttons (while it shows), else the left third of the screen back and the rest on.
// Every tap shows the pill again.
void fude_zoom_page_present_tap(fude_zoom_page* _page, rde_vec_2F _at) {
    const b8 _pill = fude_zoom_page_present_shown(_page) > 0.2f;
    _page->present_touched_at = rde_engine_get_time_now();
    fude_zoom_page_present_layout _l;
    fude_zoom_page_present_lay(_page, &_l);
    if(_pill && fude_zoom_page_box_holds(_l.pill, _at)) {
        if(fude_zoom_page_box_holds(_l.close, _at)) {
            fude_zoom_page_present_end(_page);
        } else if(fude_zoom_page_box_holds(_l.prev, _at) && _page->present_step > 0u) {
            fude_zoom_page_present_go(_page, _page->present_step - 1u);
        } else if(fude_zoom_page_box_holds(_l.next, _at) && _page->present_step + 1u < _page->present_count) {
            fude_zoom_page_present_go(_page, _page->present_step + 1u);
        }
        return;
    }
    const f64 _third = -fude_zoom_page_half(_page).x / 3.0;   // The left third back, the rest on.
    if((f64)_at.x < _third) {
        if(_page->present_step > 0u) {
            fude_zoom_page_present_go(_page, _page->present_step - 1u);
        }
    } else if(_page->present_step + 1u < _page->present_count) {
        fude_zoom_page_present_go(_page, _page->present_step + 1u);
    }
}

// Adds a point to the laser's ring of FUDE_ZOOM_PAGE_LASER points (the oldest pushed out when it is full); _lift
// marks the first point of a new trail.
RDE_INTERNAL void fude_zoom_page_laser_add(fude_zoom_page* _page, rde_vec_2F _at, b8 _lift) {
    const fude_zoom_page_laser_point _p = { _at, rde_engine_get_time_now(), _lift };
    rde_ring_buffer_push(&_page->laser, (const any)&_p);
}

// The pen (or the mouse) down while presenting: a new trail begins. A lift within 0.45 s that moved no more than 10
// points is a tap, which leaves no dot.
void fude_zoom_page_present_pen_down(fude_zoom_page* _page, rde_vec_2F _at) {
    _page->present_pen_down  = true;
    _page->present_pen_moved = false;
    _page->present_pen_at    = _at;
    _page->present_pen_time  = rde_engine_get_time_now();
    fude_zoom_page_laser_add(_page, _at, true);
}

// The pen moved while presenting: the trail goes on, and a move of more than 10 points makes it no tap.
void fude_zoom_page_present_pen_moved(fude_zoom_page* _page, rde_vec_2F _at) {
    if(!_page->present_pen_down) {
        return;
    }
    if(rde_math_abs_f32(_at.x - _page->present_pen_at.x) + rde_math_abs_f32(_at.y - _page->present_pen_at.y) > 10.0f) {
        _page->present_pen_moved = true;
    }
    fude_zoom_page_laser_add(_page, _at, false);
}

// The pen lifted while presenting: a tap (see fude_zoom_page_present_pen_down) clears the trail and is taken as a tap.
void fude_zoom_page_present_pen_up(fude_zoom_page* _page) {
    if(!_page->present_pen_down) {
        return;
    }
    _page->present_pen_down = false;
    if(!_page->present_pen_moved && rde_engine_get_time_now() - _page->present_pen_time < 0.45) {
        rde_ring_buffer_clear(&_page->laser);   // A tap leaves no dot.
        fude_zoom_page_present_tap(_page, _page->present_pen_at);
    }
}

// Over the page while presenting: the laser's trail (each segment fading over FUDE_ZOOM_PAGE_LASER_LIFE), a dot where
// the pen is, and the pill (a while after a touch) with the area's name over it.
void fude_zoom_page_render_present(fude_zoom_page* _page) {
    if(!_page->presenting) {
        return;
    }
    const f64 _now = rde_engine_get_time_now();
    // The trail, oldest first: a glow under a bright line, both fading with age (each segment from the point before).
    const u32 _points = rde_ring_buffer_count(&_page->laser);
    for(u32 _k = 1; _k < _points; _k++) {
        const fude_zoom_page_laser_point* _p = (const fude_zoom_page_laser_point*)rde_ring_buffer_at(&_page->laser, _k);
        const fude_zoom_page_laser_point* _q = (const fude_zoom_page_laser_point*)rde_ring_buffer_at(&_page->laser, _k - 1u);
        if(_p->lift) {
            continue;
        }
        const f64 _age = _now - _p->time;
        if(_age >= FUDE_ZOOM_PAGE_LASER_LIFE) {
            continue;
        }
        fude_redraw_changed();   // (render on demand: the trail fades every frame)
        const f32 _alpha = (f32)(1.0 - _age / FUDE_ZOOM_PAGE_LASER_LIFE);
        rde_rendering_2d_draw_line_1(_q->at, _p->at, (rde_color){ 255, 59, 48, (u8)(70.0f * _alpha) }, 11.0f);
        rde_rendering_2d_draw_line_1(_q->at, _p->at, (rde_color){ 255, 59, 48, (u8)(255.0f * _alpha) }, 3.5f);
    }
    if(_page->present_pen_down && _points > 0u) {
        const fude_zoom_page_laser_point* _last = (const fude_zoom_page_laser_point*)rde_ring_buffer_peek_back(&_page->laser);
        rde_rendering_2d_draw_circle(_last->at, 7.0f, 16u, (rde_color){ 255, 59, 48, 90 }, NULL);
        rde_rendering_2d_draw_circle(_last->at, 3.5f, 12u, (rde_color){ 255, 59, 48, 255 }, NULL);
    }
    const f32 _shown = fude_zoom_page_present_shown(_page);
    if(_shown <= 0.0f) {
        return;
    }
    // Render on demand: the pill stays for 3 s after a touch, then fades every frame.
    const f64 _touched = _now - _page->present_touched_at;
    if(_touched < 3.0) {
        fude_redraw_after(3.0 - _touched);
    } else {
        fude_redraw_changed();
    }
    const fude_theme* _t = fude_theme_active();
    fude_zoom_page_present_layout _l;
    fude_zoom_page_present_lay(_page, &_l);
    rde_color _bg = _t->surface, _line = _t->outline, _ink = _t->text, _accent = _t->accent;
    _bg.a     = (u8)(235.0f * _shown);
    _line.a   = (u8)((f32)_line.a * _shown);
    _ink.a    = (u8)(255.0f * _shown);
    _accent.a = (u8)(255.0f * _shown);
    const fude_zoom_box _p = _l.pill;
    rde_rendering_2d_draw_rounded_rectangle_with_border((rde_vec_2F){ (f32)((_p.min_x + _p.max_x) * 0.5), (f32)((_p.min_y + _p.max_y) * 0.5) },
                                                        (rde_vec_2F){ (f32)(_p.max_x - _p.min_x), (f32)(_p.max_y - _p.min_y) }, 0.5f, 10u, _bg, 1.0f, _line, NULL);
    // ◀ and ▶ (dimmed at either end), where it is between them, the ×.
    const fude_zoom_box _arrows[2] = { _l.prev, _l.next };
    for(u32 _a = 0; _a < 2u; _a++) {
        const b8 _can = _a == 0u ? _page->present_step > 0u : _page->present_step + 1u < _page->present_count;
        rde_color _c = _can ? _accent : _ink;
        _c.a = _can ? _c.a : (u8)(80.0f * _shown);
        const rde_vec_2F _m = { (f32)((_arrows[_a].min_x + _arrows[_a].max_x) * 0.5), (f32)((_arrows[_a].min_y + _arrows[_a].max_y) * 0.5) };
        const f32 _s = _a == 0u ? -1.0f : 1.0f;
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _m.x - 4.0f * _s, _m.y + 8.0f }, (rde_vec_2F){ _m.x + 4.0f * _s, _m.y }, _c, 2.4f);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _m.x + 4.0f * _s, _m.y }, (rde_vec_2F){ _m.x - 4.0f * _s, _m.y - 8.0f }, _c, 2.4f);
    }
    c8 _where[32];
    rde_snprintf(_where, sizeof(_where), "%u / %u", _page->present_step + 1u, _page->present_count);
    fude_zoom_render_text_in(&_page->renderer, _where, (u32)rde_strlen(_where), (fude_zoom_box){ _l.prev.max_x, _p.min_y, _l.next.min_x, _p.max_y }, 16.0, false, _ink);
    // The area's name over the pill (its first line).
    if(_page->present_areas && _page->present_step < _page->present_count && fude_zoom_map_is_area(&_page->scene, _page->present_objects[_page->present_step])) {
        f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
        c8  _name[FUDE_ZOOM_SYMBOL_TEXT];
        const u32 _o = _page->present_objects[_page->present_step];
        fude_zoom_symbol_text(_n, fude_zoom_scene_shape_numbers(&_page->scene, _o, _n, FUDE_ZOOM_SHAPE_NUMBERS), _name, sizeof(_name));
        _name[rde_strcspn(_name, "\n")] = 0;
        if(_name[0] != 0) {
            const f32 _tw = _page->renderer.font != NULL ? fude_draw_text_width(_page->renderer.font, _page->renderer.font_px, _name, 14.0f) : 80.0f;
            const f32 _w  = rde_math_min_f32(_tw + 28.0f, (f32)fude_zoom_page_half(_page).x * 1.6f);
            const rde_vec_2F _c = { 0.0f, (f32)_p.max_y + 22.0f };
            rde_rendering_2d_draw_rounded_rectangle_with_border(_c, (rde_vec_2F){ _w, 30.0f }, 0.5f, 8u, _bg, 1.0f, _line, NULL);
            fude_zoom_render_text_in(&_page->renderer, _name, (u32)rde_strlen(_name), (fude_zoom_box){ _c.x - _w * 0.5 + 8.0, _c.y - 15.0, _c.x + _w * 0.5 - 8.0, _c.y + 15.0 }, 14.0, false, _ink);
        }
    }
    {
        const rde_vec_2F _c = { (f32)((_l.close.min_x + _l.close.max_x) * 0.5), (f32)((_l.close.min_y + _l.close.max_y) * 0.5) };
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - 6.0f, _c.y - 6.0f }, (rde_vec_2F){ _c.x + 6.0f, _c.y + 6.0f }, _ink, 2.0f);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - 6.0f, _c.y + 6.0f }, (rde_vec_2F){ _c.x + 6.0f, _c.y - 6.0f }, _ink, 2.0f);
    }
}

// Its keys while presenting: on (→, ↓, space, Page Down), back (←, ↑, Page Up). Escape and Back are the app's
// (fude_zoom_kind_back).
void fude_zoom_page_present_keys(fude_zoom_page* _page, rde_window* _window) {
    const b8 _on   = rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_RIGHT) || rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_DOWN) ||
                     rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_SPACE) || rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_PAGE_DOWN);
    const b8 _back = rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_LEFT) || rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_UP) ||
                     rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_PAGE_UP);
    if(_on && _page->present_step + 1u < _page->present_count) {
        fude_zoom_page_present_go(_page, _page->present_step + 1u);
    } else if(_back && _page->present_step > 0u) {
        fude_zoom_page_present_go(_page, _page->present_step - 1u);
    }
}
