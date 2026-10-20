// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/pdfview.h"
#include "drawing/base/redraw.h"
#include "zoom/rich.h"
#include "zoom/formula.h"
#include "zoom/fill.h"
#include "zoom/codec.h"
#include "drawing/base/theme.h"
#include "drawing/widgets/kit.h"
#include "drawing/base/draw.h"

// ===========================================================================
// See pdfview.h. Drawn like doc.c's documents, by a PDF worker of its own (drawing/doc/worker.h): one piece at a time,
// on a thread started for it; the view looks each frame whether it is done. Which whole pages are kept and drawn is the
// worker's to choose (fude_doc_wholes), from what fude_zoom_pdfview_wholes sees of each page. A PDF let go while a
// piece is being drawn leaves the piece to the worker to finish and free (and the PDF to close). A sharp piece's zoom
// is screen points to a page point.
//
// In order: opening and letting go, where things are (canvas units and page points), drawing the pages (which piece
// next, what to let go, the frame's update and render), the search, and the export.
// ===========================================================================

// A PDF point in millimetres (a canvas unit over a PDF is a millimetre).
#define FUDE_ZOOM_PDFVIEW_MM_PER_POINT (25.4 / 72.0)

// Returns the search's matches as an array (room for FUDE_PDF_MATCHES).
RDE_INTERNAL fude_pdf_match* fude_zoom_pdfview_matches(const fude_zoom_pdfview* _v) { return (fude_pdf_match*)_v->matches.memory; }

// --- opening and letting go -------------------------------------------------------------------

// Frees every page's pictures.
RDE_INTERNAL void fude_zoom_pdfview_drop_tiles(fude_zoom_pdfview* _v) {
    for(u32 _i = 0; _i < _v->page_count; _i++) {
        fude_doc_tile_free(&fude_zoom_pdfview_pages(_v)[_i].whole);
        fude_doc_tile_free(&fude_zoom_pdfview_pages(_v)[_i].sharp);
    }
}

// See the header. The worker's mutex is shared by every worker (worker.h).
void fude_zoom_pdfview_init(fude_zoom_pdfview* _v) {
    rde_memset(_v, 0, sizeof(*_v));
    fude_doc_worker_init(&_v->worker);
}

// See the header. The search is stopped and the pictures freed first, then the piece being drawn let go with the PDF
// (worker.h: fude_doc_worker_drop, this its only caller here, as doc.c's fude_doc_drop_job is there): one still being
// drawn is left to the worker, which closes the PDF once done with it, so the view forgets the PDF and never closes it
// twice. While the PDF is open a piece is never orphaned: it stays the view's until taken (a theme change marks it
// stale instead), so only one is ever drawing from the PDF.
void fude_zoom_pdfview_close(fude_zoom_pdfview* _v) {
    fude_zoom_pdfview_search_stop(_v);
    fude_zoom_pdfview_drop_tiles(_v);
    if(fude_doc_worker_drop(&_v->worker)) {
        _v->pdf = NULL;   // The worker's to close now.
    }
    if(_v->pdf != NULL) {
        fude_pdf_close(_v->pdf);
    }
    if(rde_arr_is_inited(&_v->pages)) {
        rde_arr_free(&_v->pages);
        rde_arr_free(&_v->pages_seen);   // (made with the pages)
    }
    if(rde_arr_is_inited(&_v->matches)) {
        rde_arr_free(&_v->matches);
    }
    rde_memset(_v, 0, sizeof(*_v));
}

// See the header.
b8 fude_zoom_pdfview_is_open(const fude_zoom_pdfview* _v) {
    return _v->pdf != NULL && _v->page_count > 0u;
}

// See the header. Pages smaller than a point are taken as a point.
b8 fude_zoom_pdfview_open(fude_zoom_pdfview* _v, const c8* _path) {
    fude_zoom_pdfview_close(_v);
    fude_pdf* _pdf = fude_pdf_open(_path);
    if(_pdf == NULL) {
        return false;
    }
    const u32 _n = fude_pdf_page_count(_pdf);
    if(_n == 0u) {
        fude_pdf_close(_pdf);
        return false;
    }
    _v->pdf        = _pdf;
    _v->page_count = _n;
    _v->pages      = rde_arr_new(sizeof(fude_zoom_pdfview_page), rde_memory_allocator_get_default_std());
    _v->pages_seen = rde_arr_new(sizeof(fude_doc_seen), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_v->pages, _n);
    rde_arr_resize(&_v->pages_seen, _n);
    rde_snprintf(_v->path, sizeof(_v->path), "%s", _path);
    // One under the other, as big as they are, their middles on x 0, the first's top on y 0.
    f64 _top = 0.0;
    _v->bounds = fude_zoom_box_empty();
    for(u32 _i = 0; _i < _n; _i++) {
        fude_zoom_pdfview_page* _p = &fude_zoom_pdfview_pages(_v)[_i];
        _p->points = fude_pdf_page_size(_pdf, _i);
        const f64 _w = rde_math_max_f64((f64)_p->points.x, 1.0) * FUDE_ZOOM_PDFVIEW_MM_PER_POINT;
        const f64 _h = rde_math_max_f64((f64)_p->points.y, 1.0) * FUDE_ZOOM_PDFVIEW_MM_PER_POINT;
        _p->box    = (fude_zoom_box){ -_w * 0.5, _top - _h, _w * 0.5, _top };
        _v->bounds = fude_zoom_box_union(_v->bounds, _p->box);
        _top      -= _h + FUDE_ZOOM_PDFVIEW_GAP;
    }
    _v->match_at = 0u;
    fude_pdf_text_prepare(_pdf);   // (its text begun, on Android off the main thread, before Find or a lasso asks)
    return true;
}

// See the header.
fude_zoom_box fude_zoom_pdfview_bounds(const fude_zoom_pdfview* _v) {
    return _v->bounds;
}

// --- where things are -----------------------------------------------------------------------

// Returns how many canvas units (home frame) a page point is on page _p.
RDE_INTERNAL f64 fude_zoom_pdfview_k(const fude_zoom_pdfview_page* _p) {
    return (_p->box.max_x - _p->box.min_x) / rde_math_max_f64((f64)_p->points.x, 1e-9);
}

// See the header. Page points run down from the page's top, canvas units up.
fude_zoom_v2 fude_zoom_pdfview_to_canvas(const fude_zoom_pdfview* _v, u32 _page, rde_vec_2F _points) {
    const fude_zoom_pdfview_page* _p = &fude_zoom_pdfview_pages(_v)[_page < _v->page_count ? _page : 0u];
    const f64 _k = fude_zoom_pdfview_k(_p);
    return (fude_zoom_v2){ _p->box.min_x + (f64)_points.x * _k, _p->box.max_y - (f64)_points.y * _k };
}

// See the header.
rde_vec_2F fude_zoom_pdfview_to_page(const fude_zoom_pdfview* _v, u32 _page, fude_zoom_v2 _at) {
    const fude_zoom_pdfview_page* _p = &fude_zoom_pdfview_pages(_v)[_page < _v->page_count ? _page : 0u];
    const f64 _k = fude_zoom_pdfview_k(_p);
    return (rde_vec_2F){ (f32)((_at.x - _p->box.min_x) / _k), (f32)((_p->box.max_y - _at.y) / _k) };
}

// See the header. Distances are squared, from the box's nearest point.
u32 fude_zoom_pdfview_page_at(const fude_zoom_pdfview* _v, fude_zoom_v2 _at) {
    u32 _best = 0;
    f64 _far  = 1e300;
    for(u32 _i = 0; _i < _v->page_count; _i++) {
        const fude_zoom_box* _b = &fude_zoom_pdfview_pages(_v)[_i].box;
        const f64 _dx = _at.x < _b->min_x ? _b->min_x - _at.x : (_at.x > _b->max_x ? _at.x - _b->max_x : 0.0);
        const f64 _dy = _at.y < _b->min_y ? _b->min_y - _at.y : (_at.y > _b->max_y ? _at.y - _b->max_y : 0.0);
        const f64 _d  = _dx * _dx + _dy * _dy;
        if(_d < _far) {
            _far  = _d;
            _best = _i;
        }
    }
    return _best;
}

// Returns what the screen shows, in home frame units (_grow: as many screens more on every side).
RDE_INTERNAL fude_zoom_box fude_zoom_pdfview_view(fude_zoom_sim _home, fude_zoom_v2 _half, f64 _grow) {
    const fude_zoom_sim _back = fude_zoom_sim_inverse(_home);
    const f64 _hx = _half.x * (1.0 + 2.0 * _grow), _hy = _half.y * (1.0 + 2.0 * _grow);
    fude_zoom_box _b = fude_zoom_box_empty();
    const fude_zoom_v2 _c[4] = { { -_hx, -_hy }, { _hx, -_hy }, { _hx, _hy }, { -_hx, _hy } };
    for(u32 _i = 0; _i < 4u; _i++) {
        const fude_zoom_v2 _p = fude_zoom_sim_apply(_back, _c[_i]);
        _b = fude_zoom_box_union(_b, (fude_zoom_box){ _p.x, _p.y, _p.x, _p.y });
    }
    return _b;
}

// True when the boxes overlap or touch (neither checked for being empty).
RDE_INTERNAL b8 fude_zoom_pdfview_meets(fude_zoom_box _a, fude_zoom_box _b) {
    return _a.min_x <= _b.max_x && _a.max_x >= _b.min_x && _a.min_y <= _b.max_y && _a.max_y >= _b.min_y;
}

// --- drawing them -----------------------------------------------------------------------------

// Takes the piece when the worker is through: its picture onto the GPU as the page's whole picture or sharp piece
// (worker.h: fude_doc_tile_take), and the piece freed. One drawn in the colours before a theme change (stale) the
// worker throws away itself; one that failed to draw is dropped.
RDE_INTERNAL void fude_zoom_pdfview_take(fude_zoom_pdfview* _v) {
    fude_doc_piece* _piece = fude_doc_worker_take(&_v->worker);
    if(_piece == NULL) {
        return;
    }
    if(_piece->ok && _piece->page < _v->page_count) {
        fude_zoom_pdfview_page* _p = &fude_zoom_pdfview_pages(_v)[_piece->page];
        fude_doc_tile_take(_piece->kind == FUDE_DOC_PIECE_SHARP ? &_p->sharp : &_p->whole, _piece);
    }
    fude_doc_piece_free(_piece);
}

// Starts a piece on the worker: page _page's part _from-_size (page points) drawn at _w by _h pixels, in the view's
// colours, for _zoom (a sharp piece's zoom, a whole page's width level: kept with its tile).
RDE_INTERNAL void fude_zoom_pdfview_start(fude_zoom_pdfview* _v, u32 _page, u8 _kind, rde_vec_2F _from, rde_vec_2F _size, u32 _w, u32 _h, f32 _zoom) {
    fude_doc_worker_start(&_v->worker, _v->pdf, _page, _kind, _from, _size, _w, _h, _zoom);
}

// Returns page _p's width on screen in pixels at _scale screen points a canvas unit, at most a billion (as deep as the
// canvas zooms, the f32 it is handed in never overflows).
RDE_INTERNAL f32 fude_zoom_pdfview_px(const fude_zoom_pdfview_page* _p, f64 _scale) {
    return (f32)rde_math_min_f64((_p->box.max_x - _p->box.min_x) * _scale * FUDE_DOC_SCALE, 1e9);
}

// Sees each page from the view (worker.h: fude_doc_seen: on screen, within a screen round it, or away; how far its
// middle is from the screen's, up and down, as the pages hang one under the other; how wide it is on screen) and has
// fude_doc_wholes choose the whole pages to keep, freeing the others, and the next to draw: the page on screen into
// *_screen, the near one into *_near (UINT32_MAX: none).
RDE_INTERNAL void fude_zoom_pdfview_wholes(fude_zoom_pdfview* _v, fude_zoom_sim _home, fude_zoom_v2 _half, u32* _screen, u32* _near) {
    const fude_zoom_box _view  = fude_zoom_pdfview_view(_home, _half, 0.0);
    const fude_zoom_box _round = fude_zoom_pdfview_view(_home, _half, 1.0);
    const f64           _mid   = (_view.min_y + _view.max_y) * 0.5;
    const f64           _scale = fude_zoom_sim_scale(_home);   // Screen points to a canvas unit.
    fude_doc_seen*      _seen  = (fude_doc_seen*)_v->pages_seen.memory;
    for(u32 _i = 0; _i < _v->page_count; _i++) {
        fude_zoom_pdfview_page* _p     = &fude_zoom_pdfview_pages(_v)[_i];
        const u8                _where = fude_zoom_pdfview_meets(_p->box, _view) ? FUDE_DOC_ON_SCREEN : fude_zoom_pdfview_meets(_p->box, _round) ? FUDE_DOC_NEAR : FUDE_DOC_AWAY;
        _seen[_i] = (fude_doc_seen){ &_p->whole, _p->points, rde_math_abs_f64((_p->box.min_y + _p->box.max_y) * 0.5 - _mid), fude_zoom_pdfview_px(_p, _scale), _i, _where };
    }
    fude_doc_wholes(_seen, _v->page_count, _screen, _near);
}

// Starts drawing page _page whole, at its size on screen through _home (worker.h: fude_doc_whole_size); its tile keeps
// the width level it was drawn for.
RDE_INTERNAL void fude_zoom_pdfview_start_whole(fude_zoom_pdfview* _v, u32 _page, fude_zoom_sim _home) {
    const fude_zoom_pdfview_page* _p = &fude_zoom_pdfview_pages(_v)[_page];
    u32 _level, _w, _h;
    fude_doc_whole_size(_p->points, fude_zoom_pdfview_px(_p, fude_zoom_sim_scale(_home)), &_level, &_w, &_h);
    fude_zoom_pdfview_start(_v, _page, FUDE_DOC_PIECE_WHOLE, (rde_vec_2F){ 0.0f, 0.0f }, _p->points, _w, _h, (f32)_level);
}

// Starts the next piece, if any: the page on screen to draw whole (_screen, nearest the middle first, as
// fude_doc_wholes chose it); then, the view resting, the pages' sharp pieces; then the page a screen round to draw
// whole (_near).
RDE_INTERNAL void fude_zoom_pdfview_next(fude_zoom_pdfview* _v, fude_zoom_sim _home, fude_zoom_v2 _half, b8 _resting, u32 _screen, u32 _near) {
    if(_screen != UINT32_MAX) {
        fude_zoom_pdfview_start_whole(_v, _screen, _home);
        return;
    }
    const fude_zoom_box _view = fude_zoom_pdfview_view(_home, _half, 0.0);
    // Sharp: where the screen has more pixels for a page than its whole picture.
    const f64 _scale = fude_zoom_sim_scale(_home);   // Screen points to a canvas unit.
    for(u32 _i = 0; _resting && _i < _v->page_count; _i++) {
        const fude_zoom_pdfview_page* _p = &fude_zoom_pdfview_pages(_v)[_i];
        if(!fude_zoom_pdfview_meets(_p->box, _view)) {
            continue;
        }
        const f64 _k    = fude_zoom_pdfview_k(_p);
        const f64 _zoom = _scale * _k;   // Screen points to a page point.
        u32 _level, _whole_w, _whole_h;
        fude_doc_whole_size(_p->points, fude_zoom_pdfview_px(_p, _scale), &_level, &_whole_w, &_whole_h);
        if(_zoom * (f64)_p->points.x * FUDE_DOC_SCALE <= (f64)_whole_w * 1.15) {
            continue;   // Its whole picture is as sharp as the screen.
        }
        // The page on screen, in its points.
        const f64 _x0 = rde_math_max_f64(_p->box.min_x, _view.min_x), _x1 = rde_math_min_f64(_p->box.max_x, _view.max_x);
        const f64 _y0 = rde_math_max_f64(_p->box.min_y, _view.min_y), _y1 = rde_math_min_f64(_p->box.max_y, _view.max_y);
        const rde_vec_2F _from = { (f32)((_x0 - _p->box.min_x) / _k), (f32)((_p->box.max_y - _y1) / _k) };
        const rde_vec_2F _size = { (f32)((_x1 - _x0) / _k), (f32)((_y1 - _y0) / _k) };
        if(!(_size.x > 1e-3f && _size.y > 1e-3f)) {
            continue;   // Deeper than the PDF says anything: what was last drawn, stretched.
        }
        const fude_doc_tile* _s = &_p->sharp;
        // A sharp piece drawn for this zoom (within 2%) that covers what is on screen (within 1%) is kept.
        if(_s->texture != NULL && rde_math_abs_f64((f64)_s->zoom - _zoom) <= _zoom * 0.02 && _s->from.x <= _from.x + _size.x * 0.01f && _s->from.y <= _from.y + _size.y * 0.01f &&
           _s->from.x + _s->size.x >= _from.x + _size.x * 0.99f && _s->from.y + _s->size.y >= _from.y + _size.y * 0.99f) {
            continue;   // It has this one already.
        }
        // Its pixels: the screen's (FUDE_DOC_SCALE a point), its longer side at most
        // FUDE_DOC_SHARP_MAX.
        f64 _w = (_x1 - _x0) * _scale * FUDE_DOC_SCALE;
        f64 _h = (_y1 - _y0) * _scale * FUDE_DOC_SCALE;
        const f64 _over = rde_math_max_f64(_w, _h) / (f64)FUDE_DOC_SHARP_MAX;
        if(_over > 1.0) {
            _w /= _over;
            _h /= _over;
        }
        if(_w < 8.0 || _h < 8.0) {
            continue;
        }
        fude_zoom_pdfview_start(_v, _i, FUDE_DOC_PIECE_SHARP, _from, _size, (u32)_w, (u32)_h, (f32)_zoom);
        return;
    }
    if(_near != UINT32_MAX) {
        fude_zoom_pdfview_start_whole(_v, _near, _home);
    }
}

// Lets go of the sharp pieces off screen. (The whole pages far from the view are let go by fude_zoom_pdfview_wholes.)
RDE_INTERNAL void fude_zoom_pdfview_let_go(fude_zoom_pdfview* _v, fude_zoom_sim _home, fude_zoom_v2 _half) {
    const fude_zoom_box _view = fude_zoom_pdfview_view(_home, _half, 0.0);
    for(u32 _i = 0; _i < _v->page_count; _i++) {
        fude_zoom_pdfview_page* _p = &fude_zoom_pdfview_pages(_v)[_i];
        if(!fude_zoom_pdfview_meets(_p->box, _view)) {
            fude_doc_tile_free(&_p->sharp);
        }
    }
}

// See the header. A theme change drops every picture (drawn again in the new colours) and marks the piece being drawn
// stale (worker.h): it stays the worker's until it is through, then is thrown away, so no second piece is started on
// the PDF meanwhile and closing it never closes it under the first. The view counts as resting FUDE_DOC_REST seconds
// after its transform last changed.
void fude_zoom_pdfview_update(fude_zoom_pdfview* _v, fude_zoom_sim _home, fude_zoom_v2 _half) {
    if(!fude_zoom_pdfview_is_open(_v)) {
        return;
    }
    // A dark theme: the pages in its colours (drawn again when it changes).
    if(fude_doc_worker_colours(&_v->worker)) {
        fude_zoom_pdfview_drop_tiles(_v);   // (the piece being drawn, in the old colours, is thrown away when done)
    }
    // The view resting, or moving.
    const f64 _now = rde_engine_get_time_now();
    if(rde_memcmp(&_home, &_v->seen, sizeof(_home)) != 0) {
        _v->seen     = _home;
        _v->moved_at = _now;
    }
    fude_zoom_pdfview_take(_v);
    u32 _screen, _near;
    fude_zoom_pdfview_wholes(_v, _home, _half, &_screen, &_near);   // (the whole pages not kept let go at once)
    if(!fude_doc_worker_busy(&_v->worker)) {
        fude_zoom_pdfview_next(_v, _home, _half, _now - _v->moved_at >= FUDE_DOC_REST, _screen, _near);
    }
    fude_zoom_pdfview_let_go(_v, _home, _half);
    // Render on demand: the sharp pieces start once the view has rested; the worker's pieces wake the app themselves.
    if(_now - _v->moved_at < FUDE_DOC_REST) {
        fude_redraw_after(FUDE_DOC_REST - (_now - _v->moved_at));
    }
    // A search's matches as they come (render on demand: gathered each frame).
    if(_v->searching && !_v->search_done && _v->pdf != NULL) {
        fude_redraw_changed();
        b8 _done = false;
        _v->match_count = fude_pdf_find_matches(_v->pdf, fude_zoom_pdfview_matches(_v), FUDE_PDF_MATCHES, &_done);
        _v->search_done = _done;
    }
}

// Draws part of a page's picture (its rectangle _from-_size, in page points) over the page on screen: only what of it
// is in _clip (canvas units), so never more than the screen, however deep the zoom.
RDE_INTERNAL void fude_zoom_pdfview_draw_tile(const fude_zoom_pdfview_page* _p, const fude_doc_tile* _tile, fude_zoom_sim _home, fude_zoom_box _clip) {
    if(_tile->texture == NULL) {
        return;
    }
    const f64 _k = fude_zoom_pdfview_k(_p);
    const fude_zoom_box _t = { _p->box.min_x + (f64)_tile->from.x * _k, _p->box.max_y - (f64)(_tile->from.y + _tile->size.y) * _k,
                               _p->box.min_x + (f64)(_tile->from.x + _tile->size.x) * _k, _p->box.max_y - (f64)_tile->from.y * _k };
    const fude_zoom_box _c = { rde_math_max_f64(_t.min_x, _clip.min_x), rde_math_max_f64(_t.min_y, _clip.min_y), rde_math_min_f64(_t.max_x, _clip.max_x), rde_math_min_f64(_t.max_y, _clip.max_y) };
    if(!(_c.max_x > _c.min_x && _c.max_y > _c.min_y)) {
        return;
    }
    const rde_texture* _tex = rde_memory_texture_get_texture(_tile->texture);
    const rde_vec_2UI  _px  = rde_texture_get_size(_tex);
    const f64 _tw = _t.max_x - _t.min_x, _th = _t.max_y - _t.min_y;
    // Its pixels there (the bottom row first), and that on the screen.
    const rde_vec_2F _bl = { (f32)((_c.min_x - _t.min_x) / _tw * (f64)_px.x), (f32)((_c.min_y - _t.min_y) / _th * (f64)_px.y) };
    const rde_vec_2F _tr = { (f32)((_c.max_x - _t.min_x) / _tw * (f64)_px.x), (f32)((_c.max_y - _t.min_y) / _th * (f64)_px.y) };
    const f64          _s   = fude_zoom_sim_scale(_home);
    const fude_zoom_v2 _mid = fude_zoom_sim_apply(_home, (fude_zoom_v2){ (_c.min_x + _c.max_x) * 0.5, (_c.min_y + _c.max_y) * 0.5 });
    const f32 _du = rde_math_max_f32(_tr.x - _bl.x, 1e-6f), _dv = rde_math_max_f32(_tr.y - _bl.y, 1e-6f);
    rde_rendering_2d_draw_texture_partial_2(_tex, (rde_vec_3F){ (f32)_mid.x, (f32)_mid.y, 0.0f },
                                            (rde_vec_2F){ (f32)((_c.max_x - _c.min_x) * _s) / _du, (f32)((_c.max_y - _c.min_y) * _s) / _dv }, 0.0f,
                                            (rde_color){ 255, 255, 255, 255 }, _bl, _tr);
}

// Draws a canvas box on the screen as a filled rectangle, clipped to _clip first and grown by _grow points each side.
RDE_INTERNAL void fude_zoom_pdfview_rect(fude_zoom_box _b, fude_zoom_box _clip, fude_zoom_sim _home, f64 _grow, rde_color _color) {
    const f64 _s = fude_zoom_sim_scale(_home);
    const fude_zoom_box _c = { rde_math_max_f64(_b.min_x, _clip.min_x), rde_math_max_f64(_b.min_y, _clip.min_y), rde_math_min_f64(_b.max_x, _clip.max_x), rde_math_min_f64(_b.max_y, _clip.max_y) };
    if(!(_c.max_x > _c.min_x && _c.max_y > _c.min_y)) {
        return;
    }
    const fude_zoom_v2 _mid = fude_zoom_sim_apply(_home, (fude_zoom_v2){ (_c.min_x + _c.max_x) * 0.5, (_c.min_y + _c.max_y) * 0.5 });
    rde_rendering_2d_draw_rectangle((rde_vec_2F){ (f32)_mid.x, (f32)_mid.y },
                                    (rde_vec_2F){ (f32)((_c.max_x - _c.min_x) * _s + _grow * 2.0), (f32)((_c.max_y - _c.min_y) * _s + _grow * 2.0) }, _color);
}

// See the header. Each page on screen: its edge, its paper, its whole picture, then its sharp piece over it.
void fude_zoom_pdfview_render(const fude_zoom_pdfview* _v, fude_zoom_sim _home, fude_zoom_v2 _half) {
    if(!fude_zoom_pdfview_is_open(_v)) {
        return;
    }
    const fude_theme*   _t    = fude_theme_active();
    const fude_zoom_box _view = fude_zoom_pdfview_view(_home, _half, 0.02);
    const f64           _s    = fude_zoom_sim_scale(_home);
    for(u32 _i = 0; _i < _v->page_count; _i++) {
        const fude_zoom_pdfview_page* _p = &fude_zoom_pdfview_pages(_v)[_i];
        if(!fude_zoom_pdfview_meets(_p->box, _view)) {
            continue;
        }
        // The sheet (blank until it is drawn), its edge a point round it, then the pictures.
        const f64 _edge = 1.5 / rde_math_max_f64(_s, 1e-300);
        fude_zoom_pdfview_rect((fude_zoom_box){ _p->box.min_x - _edge, _p->box.min_y - _edge, _p->box.max_x + _edge, _p->box.max_y + _edge }, _view, _home, 0.0, _t->outline);
        fude_zoom_pdfview_rect(_p->box, _view, _home, 0.0, _v->worker.paper);
        fude_zoom_pdfview_draw_tile(_p, &_p->whole, _home, _view);
        fude_zoom_pdfview_draw_tile(_p, &_p->sharp, _home, _view);
    }
    // A search's matches, marked; the one gone to more strongly, outlined.
    for(u32 _m = 0; _v->searching && _m < _v->match_count; _m++) {
        const fude_pdf_match* _match = &fude_zoom_pdfview_matches(_v)[_m];
        if(_match->page >= _v->page_count) {
            continue;
        }
        const fude_zoom_v2 _a = fude_zoom_pdfview_to_canvas(_v, _match->page, _match->from);
        const fude_zoom_v2 _b = fude_zoom_pdfview_to_canvas(_v, _match->page, (rde_vec_2F){ _match->from.x + _match->size.x, _match->from.y + _match->size.y });
        const fude_zoom_box _box = { rde_math_min_f64(_a.x, _b.x), rde_math_min_f64(_a.y, _b.y), rde_math_max_f64(_a.x, _b.x), rde_math_max_f64(_a.y, _b.y) };
        if(!fude_zoom_pdfview_meets(_box, _view)) {
            continue;
        }
        rde_color _mark = _t->accent;
        _mark.a = _m == _v->match_at ? 120u : 60u;
        fude_zoom_pdfview_rect(_box, _view, _home, 2.0, _mark);
        if(_m == _v->match_at) {
            const fude_zoom_v2 _c = fude_zoom_sim_apply(_home, (fude_zoom_v2){ (_box.min_x + _box.max_x) * 0.5, (_box.min_y + _box.max_y) * 0.5 });
            const rde_vec_2F   _z = { (f32)((_box.max_x - _box.min_x) * _s + 6.0), (f32)((_box.max_y - _box.min_y) * _s + 6.0) };
            if(_z.x < 1e5f && _z.y < 1e5f) {
                rde_rendering_2d_draw_rectangle_border((rde_vec_2F){ (f32)_c.x, (f32)_c.y }, _z, 2.0f, _t->accent, NULL);
            }
        }
    }
}

// --- its text, searched ---------------------------------------------------------------------

// See the header. The matches' room is made the first time, FUDE_PDF_MATCHES of them.
void fude_zoom_pdfview_search(fude_zoom_pdfview* _v, const c8* _query) {
    fude_zoom_pdfview_search_stop(_v);
    if(_v->pdf == NULL || _query == NULL || _query[0] == 0 || !fude_pdf_text_available()) {
        return;
    }
    if(!rde_arr_is_inited(&_v->matches)) {
        _v->matches = rde_arr_new(sizeof(fude_pdf_match), rde_memory_allocator_get_default_std());
        rde_arr_resize(&_v->matches, FUDE_PDF_MATCHES);
    }
    _v->searching   = true;
    _v->search_done = false;
    _v->search_go   = true;
    _v->match_count = 0u;
    _v->match_at    = 0u;
    rde_snprintf(_v->query, sizeof(_v->query), "%s", _query);
    fude_pdf_find_start(_v->pdf, _query);
}

// See the header.
void fude_zoom_pdfview_search_stop(fude_zoom_pdfview* _v) {
    if(_v->searching && _v->pdf != NULL) {
        fude_pdf_find_stop(_v->pdf);
    }
    _v->searching   = false;
    _v->search_done = false;
    _v->search_go   = false;
    _v->match_count = 0u;
    _v->match_at    = 0u;
    _v->query[0]    = 0;
}

// See the header.
b8 fude_zoom_pdfview_match_box(fude_zoom_pdfview* _v, u32 _i, fude_zoom_box* _box) {
    if(!_v->searching || _i >= _v->match_count || fude_zoom_pdfview_matches(_v)[_i].page >= _v->page_count) {
        return false;
    }
    const fude_pdf_match* _m = &fude_zoom_pdfview_matches(_v)[_i];
    const fude_zoom_v2 _a = fude_zoom_pdfview_to_canvas(_v, _m->page, _m->from);
    const fude_zoom_v2 _b = fude_zoom_pdfview_to_canvas(_v, _m->page, (rde_vec_2F){ _m->from.x + _m->size.x, _m->from.y + _m->size.y });
    *_box        = (fude_zoom_box){ rde_math_min_f64(_a.x, _b.x), rde_math_min_f64(_a.y, _b.y), rde_math_max_f64(_a.x, _b.x), rde_math_max_f64(_a.y, _b.y) };
    _v->match_at = _i;
    return true;
}

// See the header.
b8 fude_zoom_pdfview_search_wanted(fude_zoom_pdfview* _v, fude_zoom_box* _box) {
    if(!_v->search_go || _v->match_count == 0u) {
        return false;
    }
    _v->search_go = false;
    return fude_zoom_pdfview_match_box(_v, 0u, _box);
}

// --- the PDF with what was drawn on it ------------------------------------------------------

// One object to export and its place in the drawing order (fude_zoom_scene_draw_key).
typedef struct { u32 object; u64 key; } fude_zoom_pdfview_drawn;

// rde_sort's comparison: the smaller key first, as the canvas draws them.
RDE_INTERNAL int fude_zoom_pdfview_by_key(const any _a, const any _b) {
    const u64 _x = ((const fude_zoom_pdfview_drawn*)_a)->key, _y = ((const fude_zoom_pdfview_drawn*)_b)->key;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// Returns the colour to write on white paper: the theme's ink as the light theme's, and the page's own fill as white.
RDE_INTERNAL rde_color fude_zoom_pdfview_ink(rde_color _c) {
    if(_c.a == 1u && _c.r == 255u && _c.g == 254u && _c.b == 253u) {
        return (rde_color){ 255, 255, 255, 255 };   // FUDE_THEME_PAGE_FILL: white paper.
    }
    return fude_theme_is_ink(_c) ? fude_theme_get(FUDE_THEME_PAPER)->ink : fude_theme_resolve(_c);
}

// Returns the box round box _b's corners through _sim (from frame units to home units).
RDE_INTERNAL fude_zoom_box fude_zoom_pdfview_box_through(fude_zoom_sim _sim, fude_zoom_box _b) {
    fude_zoom_box _out = fude_zoom_box_empty();
    const fude_zoom_v2 _c[4] = { { _b.min_x, _b.min_y }, { _b.max_x, _b.min_y }, { _b.max_x, _b.max_y }, { _b.min_x, _b.max_y } };
    for(u32 _i = 0; _i < 4u; _i++) {
        const fude_zoom_v2 _p = fude_zoom_sim_apply(_sim, _c[_i]);
        _out = fude_zoom_box_union(_out, (fude_zoom_box){ _p.x, _p.y, _p.x, _p.y });
    }
    return _out;
}

// The app's font in its four faces (pdf.h's FUDE_PDF_FACE_ order), for a note's words measured as the canvas measures
// them (rich.h).
typedef struct {
    rde_font* const* faces;
    f32              font_px;
} fude_zoom_pdfview_fonts;

// The face a note's run is set in (pdf.h's FUDE_PDF_FACE_), from its style's bold and italic (rich.h).
RDE_INTERNAL u8 fude_zoom_pdfview_face_of(u8 _style) {
    return (u8)(((_style & FUDE_ZOOM_RICH_BOLD) ? FUDE_PDF_FACE_BOLD : FUDE_PDF_FACE_REGULAR) | ((_style & FUDE_ZOOM_RICH_ITALIC) ? FUDE_PDF_FACE_ITALIC : 0));
}

// rich.h's measure: the width of _len bytes of _text at _px in the app's font, in the face its style says (the regular
// one when the app has not that face), as the canvas measures it. The bytes are copied to be NUL-ended: on the stack,
// or for a run of 512 bytes or more into a scratch array on the std allocator, so a long run is measured whole.
RDE_INTERNAL f32 fude_zoom_pdfview_measure(any _self, const c8* _text, u32 _len, f32 _px, u8 _style) {
    const fude_zoom_pdfview_fonts* _f    = (const fude_zoom_pdfview_fonts*)_self;
    rde_font*                      _font = _f->faces[fude_zoom_pdfview_face_of(_style)];
    _font = _font != NULL ? _font : _f->faces[FUDE_PDF_FACE_REGULAR];
    c8 _small[512];
    if(_len < sizeof(_small)) {
        rde_memcpy(_small, _text, _len);
        _small[_len] = 0;
        return fude_draw_text_width(_font, _f->font_px, _small, _px);
    }
    rde_arr _big = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_big, (usize)_len + 1u);
    rde_memcpy(_big.memory, _text, _len);
    ((c8*)_big.memory)[_len] = 0;
    const f32 _width = fude_draw_text_width(_font, _f->font_px, (const c8*)_big.memory, _px);
    rde_arr_free(&_big);
    return _width;
}

// A plain text's lines are laid out (draw.h's fude_draw_text_wrap_spans) through a window of the rest of the text at a
// time, at most this many bytes: the wrapper gives 32 lines at most and none for a text of 64 KB or more.
#define FUDE_ZOOM_PDFVIEW_WRAP_WINDOW 16384u

// Writes a plain text's _len bytes (_tx, NUL-ended) onto page _page wrapped to _wide page points at _px, every line of
// it, each whole (never cut inside a character): its lines laid out a window at a time and each written from where
// the one before ended. _all: its own units onto the home frame; _size its letters' height in its own units; _pad its
// margin.
RDE_INTERNAL void fude_zoom_pdfview_text_lines(fude_pdf_writer* _w, const fude_zoom_pdfview* _v, u32 _page, fude_zoom_sim _all, rde_font* _font, f32 _font_px,
                                               const c8* _tx, u32 _len, f32 _px, f32 _wide, f64 _size, f64 _pad, rde_color _color) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _window = rde_arr_new(sizeof(c8), _heap), _row = rde_arr_new(sizeof(c8), _heap);
    u32 _at = 0, _line = 0;   // where the rest starts; the lines written so far
    while(_at < _len) {
        // The window: the rest, or as much of it as fits, cut before a whole character.
        u32 _n = _len - _at;
        const b8 _cut = _n > FUDE_ZOOM_PDFVIEW_WRAP_WINDOW;
        if(_cut) {
            _n = FUDE_ZOOM_PDFVIEW_WRAP_WINDOW;
            while(_n > 0u && ((u8)_tx[_at + _n] & 0xC0u) == 0x80u) {
                _n--;
            }
        }
        rde_arr_resize(&_window, (usize)_n + 1u);
        c8* _win = (c8*)_window.memory;
        rde_memcpy(_win, _tx + _at, _n);
        _win[_n] = 0;
        u32 _from[32], _to[32];
        const u32 _lines = fude_draw_text_wrap_spans(_font, _font_px, _win, _px, _wide, _from, _to, 32u);
        // A cut window's last line may end only because the window does: it is laid out again with what follows.
        const u32 _use = _cut && _lines > 1u && _to[_lines - 1u] >= _n ? _lines - 1u : _lines;
        for(u32 _l = 0; _l < _use; _l++, _line++) {
            const u32 _bytes = _to[_l] - _from[_l];
            rde_arr_resize(&_row, (usize)_bytes + 1u);
            rde_memcpy(_row.memory, _win + _from[_l], _bytes);
            ((c8*)_row.memory)[_bytes] = 0;
            const fude_zoom_v2 _p = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _pad, -_pad - _size * 0.95 - (f64)_line * _size * 1.3 });
            fude_pdf_write_text(_w, (const c8*)_row.memory, fude_zoom_pdfview_to_page(_v, _page, _p), _px, _color);
        }
        if(_use == 0u || (_use == _lines && !_cut && _lines < 32u)) {
            break;   // (all of it laid out, or nothing more would be)
        }
        // On from the end of the last line written: past its spaces and the line break that ended it, if one did.
        u32 _next = _at + _to[_use - 1u];
        while(_next < _len && _tx[_next] == ' ') {
            _next++;
        }
        if(_next < _len && _tx[_next] == '\n') {
            _next++;
        }
        if(_next <= _at) {
            break;   // (nothing taken: stop rather than go round for ever)
        }
        _at = _next;
    }
    rde_arr_free(&_window);
    rde_arr_free(&_row);
}

// Writes a text read as a note (rich.h) onto page _page: its marks as lines and fills, each run of its words a text in
// its own face (bold, italic: _faces, as fude_zoom_pdfview_export has them), the writer left in the regular face after.
// _all: its own units onto the home frame; _kp: a unit of it in page points; _width: the width it wraps at (0: none);
// _pad: its margin. Each run is written whole, however long.
RDE_INTERNAL void fude_zoom_pdfview_rich(fude_pdf_writer* _w, const fude_zoom_pdfview* _v, u32 _page, fude_zoom_sim _all, f64 _kp, rde_font* const* _faces,
                                         f32 _font_px, const c8* _words, u32 _len, f64 _size, f64 _width, f64 _pad, rde_color _color) {
    fude_zoom_pdfview_fonts _fonts = { _faces, _font_px };
    fude_zoom_rich _rich;
    fude_zoom_rich_init(&_rich);
    fude_zoom_rich_lay(&_rich, _words, _len, (f32)_width, (f32)_size, 1.3f, fude_zoom_pdfview_measure, &_fonts);
    // A point of the note's layout (x right, y down from its top left, inside the pad) on the page, in page points.
    #define FZPV_AT(_x, _y) fude_zoom_pdfview_to_page(_v, _page, fude_zoom_sim_apply(_all, (fude_zoom_v2){ _pad + (f64)(_x), -_pad - (f64)(_y) }))
    const f32 _r = (f32)rde_math_max_f64(0.2, _size * _kp * 0.035);
    rde_color _soft = _color, _ground = _color;
    _soft.a   = (u8)((u32)_color.a * 35u / 100u);
    _ground.a = (u8)((u32)_color.a * 10u / 100u);
    const fude_zoom_rich_mark* _m = (const fude_zoom_rich_mark*)_rich.marks.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_rich.marks); _i++) {
        const fude_zoom_rich_mark _q = _m[_i];
        if(_q.kind == FUDE_ZOOM_RICH_MARK_LINE) {
            const rde_vec_2F _l[2] = { FZPV_AT(_q.x0, _q.y0), FZPV_AT(_q.x1, _q.y1) };
            const f32 _rr[2] = { _r, _r };
            fude_pdf_write_stroke(_w, _l, _rr, 2u, _q.soft ? _soft : _color, false);
            continue;
        }
        const rde_vec_2F _c[5] = { FZPV_AT(_q.x0, _q.y0), FZPV_AT(_q.x1, _q.y0), FZPV_AT(_q.x1, _q.y1), FZPV_AT(_q.x0, _q.y1), FZPV_AT(_q.x0, _q.y0) };
        if(_q.kind == FUDE_ZOOM_RICH_MARK_GROUND) {
            const rde_vec_2F _two[6] = { _c[0], _c[1], _c[2], _c[0], _c[2], _c[3] };
            fude_pdf_write_fill(_w, _two, 2u, _ground);
            continue;
        }
        const f32 _rr[5] = { _r, _r, _r, _r, _r };
        fude_pdf_write_stroke(_w, _c, _rr, 5u, _color, false);
        if(_q.kind == FUDE_ZOOM_RICH_MARK_TICKED) {
            const f32 _bw = _q.x1 - _q.x0, _bh = _q.y1 - _q.y0;
            const rde_vec_2F _t[3] = { FZPV_AT(_q.x0 + 0.2f * _bw, _q.y0 + 0.52f * _bh), FZPV_AT(_q.x0 + 0.42f * _bw, _q.y0 + 0.78f * _bh),
                                       FZPV_AT(_q.x0 + 0.82f * _bw, _q.y0 + 0.22f * _bh) };
            const f32 _tr[3] = { _r * 1.4f, _r * 1.4f, _r * 1.4f };
            fude_pdf_write_stroke(_w, _t, _tr, 3u, _color, false);
        }
    }
    const fude_zoom_rich_run* _run = (const fude_zoom_rich_run*)_rich.runs.memory;
    rde_arr _row = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());   // (a run NUL-ended, whole)
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_rich.runs); _i++) {
        const fude_zoom_rich_run _q = _run[_i];
        rde_arr_resize(&_row, (usize)_q.len + 1u);
        rde_memcpy(_row.memory, (_q.style & FUDE_ZOOM_RICH_OWN) ? FUDE_ZOOM_RICH_BULLET : &_words[_q.from], _q.len);
        ((c8*)_row.memory)[_q.len] = 0;
        rde_color _c = (_q.style & FUDE_ZOOM_RICH_LINK) ? (rde_color){ 70, 130, 230, 255 } : _color;
        if(_q.style & FUDE_ZOOM_RICH_SOFT) {
            _c.a = (u8)((u32)_c.a * 55u / 100u);
        }
        fude_pdf_write_face(_w, fude_zoom_pdfview_face_of(_q.style));
        fude_pdf_write_text(_w, (const c8*)_row.memory, FZPV_AT(_q.x, _q.y), (f32)(_q.px * _kp), _c);
    }
    fude_pdf_write_face(_w, FUDE_PDF_FACE_REGULAR);   // (the next text's, a plain one's, in the regular face)
    rde_arr_free(&_row);
    #undef FZPV_AT
    fude_zoom_rich_free(&_rich);
}

// What fude_zoom_pdfview_export's steps for an object share (fude_zoom_pdfview_export_text, _shape): the writer, the
// view and the page being written (k: a canvas unit in its points), the canvas, the font and the formulas, and the
// scratch arrays the objects share (the function's). A step takes what it uses under the names the function had.
typedef struct {
    fude_pdf_writer*         w;
    const fude_zoom_pdfview* v;
    u32                      page;
    f64                      k;
    const fude_zoom_scene*   s;
    rde_font* const*         fonts;     // the app's font's four faces (fude_zoom_pdfview_export's _fonts)
    f32                      font_px;
    fude_zoom_formulas*      formulas;
    rde_arr*                 local, *pts, *radii, *tris, *tpts, *text;
} fude_zoom_pdfview_exporting;

// Writes text object _i (_o; _to_home its frame onto the home frame, _color its ink) onto the page being written, for
// fude_zoom_pdfview_export: a formula as its glyphs' triangles and its rules, else a sticky note's paper and its words
// (a note's through rich.h, a plain text's lines broken as the canvas breaks them). Nothing without a font.
RDE_INTERNAL void fude_zoom_pdfview_export_text(const fude_zoom_pdfview_exporting* _ex, u32 _i, const fude_zoom_object* _o, fude_zoom_sim _to_home, rde_color _color) {
    fude_pdf_writer* _w = _ex->w;
    const fude_zoom_pdfview* _v = _ex->v;
    const u32 _page = _ex->page;
    const f64 _k = _ex->k;
    const fude_zoom_scene* _s = _ex->s;
    rde_font* _font = _ex->fonts[FUDE_PDF_FACE_REGULAR];
    const f32 _font_px = _ex->font_px;
    fude_zoom_formulas* _formulas = _ex->formulas;
    rde_arr* _tpts = _ex->tpts;
    rde_arr* _text = _ex->text;
    // A text (a note's paper first): its lines as the canvas breaks them, each from its left.
    f64 _size = 0.0, _tw = 0.0, _th = 0.0;
    u8  _style = 0;
    const c8* _words = NULL;
    u32 _len = 0;
    if(!fude_zoom_scene_text(_s, _i, &_size, &_tw, &_th, &_style, &_words, &_len) || _len == 0u || _font == NULL) {
        return;
    }
    const fude_zoom_sim _all = fude_zoom_sim_compose(_to_home, fude_zoom_object_sim(_o));
    const f64 _kp  = fude_zoom_sim_scale(_all) / _k;   // A local unit, in page points.
    const fude_zoom_tex_box* _box = _style == FUDE_ZOOM_TEXT_FORMULA && _formulas != NULL ? fude_zoom_formulas_set(_formulas, _words, _len) : NULL;
    if(_box != NULL) {
        // A formula: its glyphs' triangles as the canvas draws them (cut finer: a PDF is zoomed into),
        // and its rules.
        const fude_zoom_sim _em = fude_zoom_sim_compose(_all, (fude_zoom_sim){ _size, 0.0, FUDE_ZOOM_FORMULA_PAD * _size,
                                                                              -(FUDE_ZOOM_FORMULA_PAD + _box->height) * _size });
        const f64 _upem = (f64)_formulas->font.units_per_em;
        const fude_zoom_tex_glyph* _g = (const fude_zoom_tex_glyph*)_box->glyphs.memory;
        for(u32 _j = 0; _j < (u32)rde_arr_length(&_box->glyphs); _j++) {
            const fude_zoom_v2* _t = NULL;
            const u32 _nt = fude_zoom_formulas_mesh(_formulas, _g[_j].glyph, 8.0 * _size * _kp * _g[_j].size, &_t);
            const fude_zoom_sim _gs = fude_zoom_sim_compose(_em, (fude_zoom_sim){ _g[_j].size / _upem, 0.0, _g[_j].x, _g[_j].y });
            rde_arr_clear(_tpts);
            for(u32 _m = 0; _m < 3u * _nt; _m++) {
                const rde_vec_2F _a = fude_zoom_pdfview_to_page(_v, _page, fude_zoom_sim_apply(_gs, _t[_m]));
                rde_arr_add(_tpts, (any)&_a);
            }
            if(_nt > 0u) {
                fude_pdf_write_fill(_w, (const rde_vec_2F*)_tpts->memory, _nt, _color);
            }
        }
        const fude_zoom_tex_rule* _r = (const fude_zoom_tex_rule*)_box->rules.memory;
        for(u32 _j = 0; _j < (u32)rde_arr_length(&_box->rules); _j++) {
            const fude_zoom_v2 _c[4] = { { _r[_j].x, _r[_j].y }, { _r[_j].x + _r[_j].w, _r[_j].y },
                                         { _r[_j].x + _r[_j].w, _r[_j].y + _r[_j].h }, { _r[_j].x, _r[_j].y + _r[_j].h } };
            rde_vec_2F _q[4];
            for(u32 _m = 0; _m < 4u; _m++) {
                _q[_m] = fude_zoom_pdfview_to_page(_v, _page, fude_zoom_sim_apply(_em, _c[_m]));
            }
            const rde_vec_2F _two[6] = { _q[0], _q[1], _q[2], _q[0], _q[2], _q[3] };
            fude_pdf_write_fill(_w, _two, 2u, _color);
        }
        return;
    }
    // A plain text or a note: a sticky note's paper first, then its words, a note's through rich.h.
    const f64 _pad = _style == FUDE_ZOOM_TEXT_STICKY ? _size * 0.6 : 0.0;
    if(_style == FUDE_ZOOM_TEXT_STICKY) {
        const fude_zoom_v2 _c[4] = { { 0.0, 0.0 }, { _tw, 0.0 }, { _tw, -_th }, { 0.0, -_th } };
        rde_vec_2F _q[4];
        for(u32 _j = 0; _j < 4u; _j++) {
            _q[_j] = fude_zoom_pdfview_to_page(_v, _page, fude_zoom_sim_apply(_all, _c[_j]));
        }
        const rde_vec_2F _two[6] = { _q[0], _q[1], _q[2], _q[0], _q[2], _q[3] };
        fude_pdf_write_fill(_w, _two, 2u, _o->fill);
    }
    if(fude_zoom_rich_marked(_words, _len)) {
        fude_zoom_pdfview_rich(_w, _v, _page, _all, _kp, _ex->fonts, _font_px, _words, _len, _size,
                               _style == FUDE_ZOOM_TEXT_STICKY ? rde_math_max_f64(_tw - 2.0 * _pad, _size) : 0.0, _pad, _color);
        return;
    }
    rde_arr_resize(_text, (usize)_len + 1u);
    c8* _tx = (c8*)_text->memory;
    rde_memcpy(_tx, _words, _len);
    _tx[_len] = 0;
    const f32 _px = (f32)(_size * _kp), _wide = (f32)rde_math_max_f64((_tw - 2.0 * _pad) * _kp, 1.0);
    fude_zoom_pdfview_text_lines(_w, _v, _page, _all, _font, _font_px, _tx, _len, _px, _wide, _size, _pad, _color);
}

// Writes shape object _i (_o; _to_home its frame onto the home frame, _color its ink) onto the page being written, for
// fude_zoom_pdfview_export: its outline, filled when it is, its line over it.
RDE_INTERNAL void fude_zoom_pdfview_export_shape(const fude_zoom_pdfview_exporting* _ex, u32 _i, const fude_zoom_object* _o, fude_zoom_sim _to_home, rde_color _color) {
    fude_pdf_writer* _w = _ex->w;
    const fude_zoom_pdfview* _v = _ex->v;
    const u32 _page = _ex->page;
    const f64 _k = _ex->k;
    const fude_zoom_scene* _s = _ex->s;
    rde_arr* _local = _ex->local;
    rde_arr* _pts = _ex->pts;
    rde_arr* _radii = _ex->radii;
    rde_arr* _tris = _ex->tris;
    rde_arr* _tpts = _ex->tpts;
    // What it looks like: its outline, filled when it is, its line over it.
    b8 _closed = false;
    rde_arr_clear(_local);
    fude_zoom_scene_shape_outline(_s, _i, 96u, _local, &_closed);
    const u32 _n = (u32)rde_arr_length(_local);
    if(_n < 2u) {
        return;
    }
    rde_arr_clear(_pts);
    rde_arr_clear(_radii);
    const f32 _r = (f32)((f64)_o->radius * _o->scale * fude_zoom_sim_scale(_to_home) / _k);
    for(u32 _j = 0; _j < _n + (_closed ? 1u : 0u); _j++) {
        const fude_zoom_v2 _h = fude_zoom_sim_apply(_to_home, ((const fude_zoom_v2*)_local->memory)[_j % _n]);
        const rde_vec_2F   _a = fude_zoom_pdfview_to_page(_v, _page, _h);
        rde_arr_add(_pts, (any)&_a);
        rde_arr_add(_radii, (any)&_r);
    }
    if(_closed && (_o->flags & FUDE_ZOOM_FLAG_FILLED) && _n >= 3u) {
        rde_arr_clear(_local);   // The outline again, in page points, to fill.
        for(u32 _j = 0; _j < _n; _j++) {
            const rde_vec_2F _a = ((const rde_vec_2F*)_pts->memory)[_j];
            const fude_zoom_v2 _b = { _a.x, _a.y };
            rde_arr_add(_local, (any)&_b);
        }
        rde_arr_clear(_tris);
        const u32 _t = fude_zoom_fill_triangulate((const fude_zoom_v2*)_local->memory, _n, _tris);
        rde_arr_clear(_tpts);
        for(u32 _j = 0; _j < _t * 3u; _j++) {
            const fude_zoom_v2 _c = ((const fude_zoom_v2*)_tris->memory)[_j];
            const rde_vec_2F   _a = { (f32)_c.x, (f32)_c.y };
            rde_arr_add(_tpts, (any)&_a);
        }
        fude_pdf_write_fill(_w, (const rde_vec_2F*)_tpts->memory, _t, fude_zoom_pdfview_ink((_o->flags & FUDE_ZOOM_FLAG_FILL_OWN) ? _o->fill : _o->color));
    }
    if(_r > 0.0f) {
        fude_pdf_write_stroke(_w, (const rde_vec_2F*)_pts->memory, (const f32*)_radii->memory, (u32)rde_arr_length(_pts), _color, false);
    }
}

// See the header. The objects that are drawn (alive, shown, with points) are sorted into the order the canvas draws
// them; then each page is copied and the objects on it written over it, in two passes, the markers first. Copying a
// page draws the PDF on this thread, so the piece the worker is drawing, if any, is waited for first (pdf.h: one thread
// at a time); no other starts before the next frame. A plain
// text's lines are broken as the canvas breaks them, every one of them written whole (fude_zoom_pdfview_text_lines).
// Each kind of object is written in its own way, one after the other (a text and a shape by steps of their own); the
// scratch arrays are shared by them.
b8 fude_zoom_pdfview_export(const fude_zoom_pdfview* _v, const fude_zoom_scene* _s, rde_font* const _fonts[FUDE_PDF_FACES], f32 _font_px,
                            fude_zoom_formulas* _formulas, const c8* _out) {
    if(!fude_zoom_pdfview_is_open(_v)) {
        return false;
    }
    fude_pdf_writer* _w = fude_pdf_write_begin(_out);
    if(_w == NULL) {
        return false;
    }
    fude_doc_worker_wait(&_v->worker);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    // What is drawn (alive, shown: a line, a fill, a shape, a text, a picture), in the order it was.
    rde_arr _drawn = rde_arr_new(sizeof(fude_zoom_pdfview_drawn), _heap);
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) ||
           (_o->kind != FUDE_ZOOM_KIND_STROKE && _o->kind != FUDE_ZOOM_KIND_FILL && _o->kind != FUDE_ZOOM_KIND_SHAPE && _o->kind != FUDE_ZOOM_KIND_TEXT &&
            _o->kind != FUDE_ZOOM_KIND_IMAGE) ||
           _o->count == 0u || fude_zoom_scene_hides(_s, _o) || !fude_zoom_scene_frame_shown(_s, _o->frame)) {
            continue;
        }
        const fude_zoom_pdfview_drawn _d = { _i, fude_zoom_scene_draw_key(_s, _o) };
        rde_arr_add(&_drawn, (any)&_d);
    }
    const u32 _nd = (u32)rde_arr_length(&_drawn);
    rde_sort(_drawn.memory, _nd, sizeof(fude_zoom_pdfview_drawn), fude_zoom_pdfview_by_key);
    rde_arr _q     = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
    rde_arr _pts   = rde_arr_new(sizeof(rde_vec_2F), _heap);
    rde_arr _radii = rde_arr_new(sizeof(f32), _heap);
    rde_arr _local = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr _rings = rde_arr_new(sizeof(u32), _heap);
    rde_arr _tris  = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr _tpts  = rde_arr_new(sizeof(rde_vec_2F), _heap);
    rde_arr _text  = rde_arr_new(sizeof(c8), _heap);
    fude_zoom_pdfview_exporting _ex = { _w, _v, 0u, 0.0, _s, _fonts, _font_px, _formulas, &_local, &_pts, &_radii, &_tris, &_tpts, &_text };
    for(u32 _page = 0; _page < _v->page_count; _page++) {
        const fude_zoom_pdfview_page* _p = &fude_zoom_pdfview_pages(_v)[_page];
        const f64 _k = fude_zoom_pdfview_k(_p);   // Canvas units to a page point.
        _ex.page = _page;
        _ex.k    = _k;
        fude_pdf_write_page(_w, _v->pdf, _page);
        for(u32 _pass = 0; _pass < 2u; _pass++) {   // The markers' first, under the rest.
            for(u32 _d = 0; _d < _nd; _d++) {
                const u32               _i = ((const fude_zoom_pdfview_drawn*)_drawn.memory)[_d].object;
                const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
                if(((_o->flags & FUDE_ZOOM_FLAG_MARKER) != 0) != (_pass == 0u)) {
                    continue;
                }
                const fude_zoom_sim _to_home = fude_zoom_scene_sim(_s, _o->frame, _s->home);
                if(!fude_zoom_pdfview_meets(fude_zoom_pdfview_box_through(_to_home, _o->box), _p->box)) {
                    continue;   // Not on this page.
                }
                const rde_color _color = fude_zoom_pdfview_ink(_o->color);
                if(_o->kind == FUDE_ZOOM_KIND_IMAGE) {
                    // A picture: its file's bytes, onto its corners.
                    const u8* _bytes = NULL;
                    u32       _size  = 0;
                    if(!fude_zoom_scene_image(_s, _i, NULL, NULL, &_bytes, &_size) || _bytes == NULL || _size == 0u) {
                        continue;
                    }
                    fude_zoom_v2 _c4[4];
                    fude_zoom_scene_image_corners(_s, _i, _c4);   // Bottom-left, bottom-right, top-right, top-left.
                    const rde_vec_2F _three[3] = { fude_zoom_pdfview_to_page(_v, _page, fude_zoom_sim_apply(_to_home, _c4[0])),
                                                   fude_zoom_pdfview_to_page(_v, _page, fude_zoom_sim_apply(_to_home, _c4[1])),
                                                   fude_zoom_pdfview_to_page(_v, _page, fude_zoom_sim_apply(_to_home, _c4[3])) };
                    fude_pdf_write_image(_w, _bytes, _size, _three);
                    continue;
                }
                if(_o->kind == FUDE_ZOOM_KIND_TEXT) {
                    fude_zoom_pdfview_export_text(&_ex, _i, _o, _to_home, _color);
                    continue;
                }
                if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
                    fude_zoom_pdfview_export_shape(&_ex, _i, _o, _to_home, _color);
                    continue;
                }
                // A line, or a fill: its points as the renderer has them, through its place.
                rde_arr_clear(&_q);
                rde_arr_add_n(&_q, _o->count);
                if(!fude_zoom_scene_points(_s, _i, (fude_zoom_qpoint*)_q.memory)) {
                    continue;
                }
                const fude_zoom_qpoint* _qp  = (const fude_zoom_qpoint*)_q.memory;
                const fude_zoom_sim     _all = fude_zoom_sim_compose(_to_home, fude_zoom_object_sim(_o));
                const f64               _ks  = fude_zoom_sim_scale(_all) / _k;   // A local unit, in page points.
                if(_o->kind == FUDE_ZOOM_KIND_STROKE) {
                    rde_arr_clear(&_pts);
                    rde_arr_clear(&_radii);
                    for(u32 _j = 0; _j < _o->count; _j++) {
                        const fude_zoom_v2 _h = fude_zoom_sim_apply(_all, (fude_zoom_v2){ rde_math_ldexp_f64((f64)_qp[_j].x, _o->q), rde_math_ldexp_f64((f64)_qp[_j].y, _o->q) });
                        const rde_vec_2F   _a = fude_zoom_pdfview_to_page(_v, _page, _h);
                        const f32          _r = (f32)((f64)fude_zoom_scene_local_radius_at(_o, &_qp[_j]) * _ks);
                        rde_arr_add(&_pts, (any)&_a);
                        rde_arr_add(&_radii, (any)&_r);
                    }
                    fude_pdf_write_stroke(_w, (const rde_vec_2F*)_pts.memory, (const f32*)_radii.memory, _o->count, _color, (_o->flags & FUDE_ZOOM_FLAG_MARKER) != 0);
                    continue;
                }
                // A fill: its rings (the eraser's cuts out of it) as triangles, painted as one.
                rde_arr_clear(&_local);
                rde_arr_clear(&_rings);
                for(u32 _j = 0; _j < _o->count; _j++) {
                    const fude_zoom_v2 _h = fude_zoom_sim_apply(_all, (fude_zoom_v2){ rde_math_ldexp_f64((f64)_qp[_j].x, _o->q), rde_math_ldexp_f64((f64)_qp[_j].y, _o->q) });
                    const rde_vec_2F   _a = fude_zoom_pdfview_to_page(_v, _page, _h);
                    const fude_zoom_v2 _b = { _a.x, _a.y };
                    const u32          _ring = (_o->channels & FUDE_ZOOM_CHANNEL_TIME) ? _qp[_j].time : 0u;
                    rde_arr_add(&_local, (any)&_b);
                    rde_arr_add(&_rings, (any)&_ring);
                }
                rde_arr_clear(&_tris);
                const u32 _t = fude_zoom_fill_triangulate_rings((const fude_zoom_v2*)_local.memory, (const u32*)_rings.memory, _o->count, &_tris);
                rde_arr_clear(&_tpts);
                for(u32 _j = 0; _j < _t * 3u; _j++) {
                    const fude_zoom_v2 _c = ((const fude_zoom_v2*)_tris.memory)[_j];
                    const rde_vec_2F   _a = { (f32)_c.x, (f32)_c.y };
                    rde_arr_add(&_tpts, (any)&_a);
                }
                fude_pdf_write_fill(_w, (const rde_vec_2F*)_tpts.memory, _t, _color);
            }
        }
        fude_pdf_write_page_end(_w);
    }
    rde_arr_free(&_drawn);
    rde_arr_free(&_q);
    rde_arr_free(&_pts);
    rde_arr_free(&_radii);
    rde_arr_free(&_local);
    rde_arr_free(&_rings);
    rde_arr_free(&_tris);
    rde_arr_free(&_tpts);
    rde_arr_free(&_text);
    return fude_pdf_write_end(_w);
}
