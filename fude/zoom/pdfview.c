// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/pdfview.h"
#include "zoom/fill.h"
#include "zoom/codec.h"
#include "drawing/base/theme.h"
#include "drawing/widgets/kit.h"
#include "drawing/widgets/draw.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See pdfview.h. As doc.c draws Kana's: one piece at a time, on a thread
// started for it (RDE's detached threads); the main thread hands it a job and
// looks each frame whether it is done. A PDF let go while a piece is being
// drawn leaves the job to the worker to finish and free (and the PDF to close).
// ===========================================================================

#define FUDE_ZOOM_PDFVIEW_MM_PER_POINT (25.4 / 72.0)

typedef enum { FUDE_ZOOM_PDFVIEW_JOB_WHOLE = 0, FUDE_ZOOM_PDFVIEW_JOB_SHARP } FUDE_ZOOM_PDFVIEW_JOB_;

typedef struct fude_zoom_pdfview_job {
    fude_pdf*  pdf;
    b8         close_pdf;   // the PDF was let go: the worker closes it too
    u32        page;
    u8         kind;        // FUDE_ZOOM_PDFVIEW_JOB_
    rde_vec_2F from, size;  // the part of the page, in its points
    u32        w, h;
    f32        zoom;        // screen points to a page point, as drawn for (a sharp piece's)
    b8         dark;
    rde_color  paper, print;
    rde_arr TYPE(u8) pixels;   // w * h * 4
    b8         ok;
    b8         done;        // the worker is through (under the mutex)
    b8         orphaned;    // the main thread let it go (under the mutex)
} fude_zoom_pdfview_job;

RDE_INTERNAL rde_mutex fude_zoom_pdfview_mutex = NULL;

RDE_INTERNAL fude_pdf_match* fude_zoom_pdfview_matches(const fude_zoom_pdfview* _v) { return (fude_pdf_match*)_v->matches.memory; }

// --- the worker -----------------------------------------------------------------------------

RDE_INTERNAL void fude_zoom_pdfview_job_free(fude_zoom_pdfview_job* _job) {
    if(_job->close_pdf) {
        fude_pdf_close(_job->pdf);
    }
    rde_arr_free(&_job->pixels);
    free(_job);
}

// A dark theme's page: white paper in the paper colour, black print in the print
// colour — by how light each pixel is, a little of its own colour kept (doc.c's).
RDE_INTERNAL void fude_zoom_pdfview_darken(fude_zoom_pdfview_job* _job) {
    const usize     _n     = (usize)_job->w * (usize)_job->h;
    u8*             _p     = _job->pixels.memory;
    const rde_color _paper = _job->paper;
    const rde_color _print = _job->print;
    for(usize _i = 0; _i < _n; _i++, _p += 4) {
        const i32 _l = ((i32)_p[0] * 77 + (i32)_p[1] * 150 + (i32)_p[2] * 29) >> 8;
        const i32 _r = (i32)_print.r + ((i32)_paper.r - (i32)_print.r) * _l / 255 + ((i32)_p[0] - _l) * 3 / 5;
        const i32 _g = (i32)_print.g + ((i32)_paper.g - (i32)_print.g) * _l / 255 + ((i32)_p[1] - _l) * 3 / 5;
        const i32 _b = (i32)_print.b + ((i32)_paper.b - (i32)_print.b) * _l / 255 + ((i32)_p[2] - _l) * 3 / 5;
        _p[0] = (u8)(_r < 0 ? 0 : _r > 255 ? 255 : _r);
        _p[1] = (u8)(_g < 0 ? 0 : _g > 255 ? 255 : _g);
        _p[2] = (u8)(_b < 0 ? 0 : _b > 255 ? 255 : _b);
        _p[3] = 255u;
    }
}

// Rows turned over: a memory texture's first row is its bottom (pdf.h's, the top).
RDE_INTERNAL void fude_zoom_pdfview_flip(fude_zoom_pdfview_job* _job) {
    const usize _row = (usize)_job->w * 4u;
    rde_arr     _tmp = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_tmp, _row);
    for(u32 _y = 0; _y < _job->h / 2u; _y++) {
        u8* _a = _job->pixels.memory + (usize)_y * _row;
        u8* _b = _job->pixels.memory + (usize)(_job->h - 1u - _y) * _row;
        memcpy(_tmp.memory, _a, _row);
        memcpy(_a, _b, _row);
        memcpy(_b, _tmp.memory, _row);
    }
    rde_arr_free(&_tmp);
}

RDE_INTERNAL any fude_zoom_pdfview_work(rde_thread* _thread, any _data) {
    RDE_UNUSED(_thread);
    fude_zoom_pdfview_job* _job = (fude_zoom_pdfview_job*)_data;
    _job->ok = fude_pdf_render(_job->pdf, _job->page, _job->from, _job->size, _job->w, _job->h, _job->pixels.memory);
    if(_job->ok) {
        fude_zoom_pdfview_flip(_job);
        if(_job->dark) {
            fude_zoom_pdfview_darken(_job);
        }
    }
    rde_mutex_lock(fude_zoom_pdfview_mutex);
    const b8 _orphaned = _job->orphaned;
    _job->done         = true;
    rde_mutex_unlock(fude_zoom_pdfview_mutex);
    if(_orphaned) {
        fude_zoom_pdfview_job_free(_job);
    }
    return NULL;
}

// --- opening and letting go -------------------------------------------------------------------

RDE_INTERNAL void fude_zoom_pdfview_tile_free(fude_doc_tile* _tile) {
    if(_tile->texture != NULL) {
        rde_memory_texture_destroy(_tile->texture);
    }
    memset(_tile, 0, sizeof(*_tile));
}

RDE_INTERNAL void fude_zoom_pdfview_drop_tiles(fude_zoom_pdfview* _v) {
    for(u32 _i = 0; _i < _v->page_count; _i++) {
        fude_zoom_pdfview_tile_free(&fude_zoom_pdfview_pages(_v)[_i].whole);
        fude_zoom_pdfview_tile_free(&fude_zoom_pdfview_pages(_v)[_i].sharp);
    }
}

// The piece being drawn let go: freed here when it is done, else by the worker
// (closing the PDF too, when _with_pdf).
RDE_INTERNAL void fude_zoom_pdfview_drop_job(fude_zoom_pdfview* _v, b8 _with_pdf) {
    fude_zoom_pdfview_job* _job = _v->job;
    _v->job                 = NULL;
    if(_job == NULL) {
        return;
    }
    rde_mutex_lock(fude_zoom_pdfview_mutex);
    const b8 _done = _job->done;
    if(!_done) {
        _job->close_pdf = _with_pdf;
        _job->orphaned  = true;
    }
    rde_mutex_unlock(fude_zoom_pdfview_mutex);
    if(_done) {
        _job->close_pdf = false;   // the caller closes it
        fude_zoom_pdfview_job_free(_job);
    } else if(_with_pdf) {
        _v->pdf = NULL;            // the worker's to close now
    }
}

void fude_zoom_pdfview_init(fude_zoom_pdfview* _v) {
    memset(_v, 0, sizeof(*_v));
    if(fude_zoom_pdfview_mutex == NULL) {
        fude_zoom_pdfview_mutex = rde_mutex_create();
    }
}

void fude_zoom_pdfview_close(fude_zoom_pdfview* _v) {
    fude_zoom_pdfview_search_stop(_v);
    fude_zoom_pdfview_drop_tiles(_v);
    fude_zoom_pdfview_drop_job(_v, true);
    if(_v->pdf != NULL) {
        fude_pdf_close(_v->pdf);
    }
    if(rde_arr_is_inited(&_v->pages)) {
        rde_arr_free(&_v->pages);
    }
    if(rde_arr_is_inited(&_v->matches)) {
        rde_arr_free(&_v->matches);
    }
    memset(_v, 0, sizeof(*_v));
}

b8 fude_zoom_pdfview_is_open(const fude_zoom_pdfview* _v) {
    return _v->pdf != NULL && _v->page_count > 0u;
}

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
    rde_arr_resize(&_v->pages, _n);
    snprintf(_v->path, sizeof(_v->path), "%s", _path);
    // One under the other, as big as they are, their middles on x 0, the first's top on y 0.
    f64 _top = 0.0;
    _v->bounds = fude_zoom_box_empty();
    for(u32 _i = 0; _i < _n; _i++) {
        fude_zoom_pdfview_page* _p = &fude_zoom_pdfview_pages(_v)[_i];
        _p->points = fude_pdf_page_size(_pdf, _i);
        const f64 _w = fmax((f64)_p->points.x, 1.0) * FUDE_ZOOM_PDFVIEW_MM_PER_POINT;
        const f64 _h = fmax((f64)_p->points.y, 1.0) * FUDE_ZOOM_PDFVIEW_MM_PER_POINT;
        _p->box    = (fude_zoom_box){ -_w * 0.5, _top - _h, _w * 0.5, _top };
        _v->bounds = fude_zoom_box_union(_v->bounds, _p->box);
        _top      -= _h + FUDE_ZOOM_PDFVIEW_GAP;
    }
    _v->match_at = 0u;
    return true;
}

fude_zoom_box fude_zoom_pdfview_bounds(const fude_zoom_pdfview* _v) {
    return _v->bounds;
}

// --- where things are -----------------------------------------------------------------------

// Canvas units (home frame) to a page point.
RDE_INTERNAL f64 fude_zoom_pdfview_k(const fude_zoom_pdfview_page* _p) {
    return (_p->box.max_x - _p->box.min_x) / fmax((f64)_p->points.x, 1e-9);
}

fude_zoom_v2 fude_zoom_pdfview_to_canvas(const fude_zoom_pdfview* _v, u32 _page, rde_vec_2F _points) {
    const fude_zoom_pdfview_page* _p = &fude_zoom_pdfview_pages(_v)[_page < _v->page_count ? _page : 0u];
    const f64 _k = fude_zoom_pdfview_k(_p);
    return (fude_zoom_v2){ _p->box.min_x + (f64)_points.x * _k, _p->box.max_y - (f64)_points.y * _k };
}

rde_vec_2F fude_zoom_pdfview_to_page(const fude_zoom_pdfview* _v, u32 _page, fude_zoom_v2 _at) {
    const fude_zoom_pdfview_page* _p = &fude_zoom_pdfview_pages(_v)[_page < _v->page_count ? _page : 0u];
    const f64 _k = fude_zoom_pdfview_k(_p);
    return (rde_vec_2F){ (f32)((_at.x - _p->box.min_x) / _k), (f32)((_p->box.max_y - _at.y) / _k) };
}

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

// What the screen shows, in home frame units (_grow: as many screens more round it).
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

RDE_INTERNAL b8 fude_zoom_pdfview_meets(fude_zoom_box _a, fude_zoom_box _b) {
    return _a.min_x <= _b.max_x && _a.max_x >= _b.min_x && _a.min_y <= _b.max_y && _a.max_y >= _b.min_y;
}

// --- drawing them -----------------------------------------------------------------------------

RDE_INTERNAL void fude_zoom_pdfview_take(fude_zoom_pdfview* _v) {
    fude_zoom_pdfview_job* _job = _v->job;
    if(_job == NULL) {
        return;
    }
    rde_mutex_lock(fude_zoom_pdfview_mutex);
    const b8 _done = _job->done;
    rde_mutex_unlock(fude_zoom_pdfview_mutex);
    if(!_done) {
        return;
    }
    _v->job = NULL;
    if(_job->ok && _job->page < _v->page_count) {
        fude_zoom_pdfview_page* _p    = &fude_zoom_pdfview_pages(_v)[_job->page];
        fude_doc_tile*      _tile = _job->kind == FUDE_ZOOM_PDFVIEW_JOB_SHARP ? &_p->sharp : &_p->whole;
        fude_zoom_pdfview_tile_free(_tile);
        _tile->texture = rde_memory_texture_create(_job->w, _job->h, 4u, NULL);
        memcpy(rde_memory_texture_get_pixels(_tile->texture), _job->pixels.memory, (usize)_job->w * (usize)_job->h * 4u);
        rde_texture_parameters _params = RDE_DEFAULT_TEXTURE_PARAMETERS;
        _params.wrap_s                 = RDE_TEXTURE_PARAMETER_TYPE_WRAP_CLAMP_TO_EDGE;
        _params.wrap_t                 = RDE_TEXTURE_PARAMETER_TYPE_WRAP_CLAMP_TO_EDGE;
        _params.generate_mipmap        = _job->kind == FUDE_ZOOM_PDFVIEW_JOB_WHOLE;   // a whole page is seen small too
        rde_memory_texture_gpu_upload(_tile->texture, &_params);
        _tile->from = _job->from;
        _tile->size = _job->size;
        _tile->zoom = _job->zoom;
    }
    _job->close_pdf = false;
    fude_zoom_pdfview_job_free(_job);
}

RDE_INTERNAL void fude_zoom_pdfview_start(fude_zoom_pdfview* _v, u32 _page, u8 _kind, rde_vec_2F _from, rde_vec_2F _size, u32 _w, u32 _h, f32 _zoom) {
    fude_zoom_pdfview_job* _job = (fude_zoom_pdfview_job*)calloc(1, sizeof(fude_zoom_pdfview_job));
    if(_job == NULL) {
        return;
    }
    _job->pdf    = _v->pdf;
    _job->page   = _page;
    _job->kind   = _kind;
    _job->from   = _from;
    _job->size   = _size;
    _job->w      = _w;
    _job->h      = _h;
    _job->zoom   = _zoom;
    _job->dark   = _v->dark;
    _job->paper  = _v->paper;
    _job->print  = _v->print;
    // (its room at once, for the worker to fill: not grown, not cleared twice)
    const usize _bytes = (usize)_w * (usize)_h * 4u;
    _job->pixels = rde_arr_new_with_capacity(sizeof(u8), _bytes + 1u, rde_memory_allocator_get_default_std());
    rde_arr_add_n(&_job->pixels, _bytes);
    _v->job = _job;
    rde_thread_run_detached(fude_zoom_pdfview_work, _job, NULL);
}

RDE_INTERNAL u32 fude_zoom_pdfview_whole_h(const fude_zoom_pdfview_page* _p) {
    return (u32)fmaxf(1.0f, (f32)FUDE_ZOOM_PDFVIEW_WHOLE_PX * _p->points.y / fmaxf(1.0f, _p->points.x));
}

// The next piece: the pages on screen whole (nearest the middle first); then,
// the view resting, their sharp pieces; then the pages a screen round.
RDE_INTERNAL void fude_zoom_pdfview_next(fude_zoom_pdfview* _v, fude_zoom_sim _home, fude_zoom_v2 _half, b8 _resting) {
    const fude_zoom_box _view = fude_zoom_pdfview_view(_home, _half, 0.0);
    const fude_zoom_v2  _mid  = { (_view.min_x + _view.max_x) * 0.5, (_view.min_y + _view.max_y) * 0.5 };
    i32 _best = -1;
    f64 _far  = 1e300;
    for(u32 _i = 0; _i < _v->page_count; _i++) {
        const fude_zoom_pdfview_page* _p = &fude_zoom_pdfview_pages(_v)[_i];
        if(_p->whole.texture == NULL && fude_zoom_pdfview_meets(_p->box, _view)) {
            const f64 _d = fabs((_p->box.min_y + _p->box.max_y) * 0.5 - _mid.y);
            if(_d < _far) {
                _far  = _d;
                _best = (i32)_i;
            }
        }
    }
    if(_best >= 0) {
        const fude_zoom_pdfview_page* _p = &fude_zoom_pdfview_pages(_v)[_best];
        fude_zoom_pdfview_start(_v, (u32)_best, FUDE_ZOOM_PDFVIEW_JOB_WHOLE, (rde_vec_2F){ 0.0f, 0.0f }, _p->points, FUDE_ZOOM_PDFVIEW_WHOLE_PX, fude_zoom_pdfview_whole_h(_p), 0.0f);
        return;
    }
    // Sharp: where the screen has more pixels for a page than its whole picture.
    const f64 _scale = fude_zoom_sim_scale(_home);   // screen points to a canvas unit
    for(u32 _i = 0; _resting && _i < _v->page_count; _i++) {
        const fude_zoom_pdfview_page* _p = &fude_zoom_pdfview_pages(_v)[_i];
        if(!fude_zoom_pdfview_meets(_p->box, _view)) {
            continue;
        }
        const f64 _k    = fude_zoom_pdfview_k(_p);
        const f64 _zoom = _scale * _k;   // screen points to a page point
        if(_zoom * (f64)_p->points.x * FUDE_ZOOM_PDFVIEW_SCALE <= (f64)FUDE_ZOOM_PDFVIEW_WHOLE_PX * 1.15) {
            continue;   // (its whole picture is as sharp as the screen)
        }
        // The page on screen, in its points.
        const f64 _x0 = fmax(_p->box.min_x, _view.min_x), _x1 = fmin(_p->box.max_x, _view.max_x);
        const f64 _y0 = fmax(_p->box.min_y, _view.min_y), _y1 = fmin(_p->box.max_y, _view.max_y);
        const rde_vec_2F _from = { (f32)((_x0 - _p->box.min_x) / _k), (f32)((_p->box.max_y - _y1) / _k) };
        const rde_vec_2F _size = { (f32)((_x1 - _x0) / _k), (f32)((_y1 - _y0) / _k) };
        if(!(_size.x > 1e-3f && _size.y > 1e-3f)) {
            continue;   // (deeper than the PDF says anything: what was last drawn, stretched)
        }
        const fude_doc_tile* _s = &_p->sharp;
        if(_s->texture != NULL && fabs((f64)_s->zoom - _zoom) <= _zoom * 0.02 && _s->from.x <= _from.x + _size.x * 0.01f && _s->from.y <= _from.y + _size.y * 0.01f &&
           _s->from.x + _s->size.x >= _from.x + _size.x * 0.99f && _s->from.y + _s->size.y >= _from.y + _size.y * 0.99f) {
            continue;   // it has this one already
        }
        f64 _w = (_x1 - _x0) * _scale * FUDE_ZOOM_PDFVIEW_SCALE;
        f64 _h = (_y1 - _y0) * _scale * FUDE_ZOOM_PDFVIEW_SCALE;
        const f64 _over = fmax(_w, _h) / (f64)FUDE_ZOOM_PDFVIEW_SHARP_MAX;
        if(_over > 1.0) {
            _w /= _over;
            _h /= _over;
        }
        if(_w < 8.0 || _h < 8.0) {
            continue;
        }
        fude_zoom_pdfview_start(_v, _i, FUDE_ZOOM_PDFVIEW_JOB_SHARP, _from, _size, (u32)_w, (u32)_h, (f32)_zoom);
        return;
    }
    // Ahead: a screen round.
    const fude_zoom_box _round = fude_zoom_pdfview_view(_home, _half, 1.0);
    for(u32 _i = 0; _i < _v->page_count; _i++) {
        const fude_zoom_pdfview_page* _p = &fude_zoom_pdfview_pages(_v)[_i];
        if(_p->whole.texture == NULL && fude_zoom_pdfview_meets(_p->box, _round)) {
            fude_zoom_pdfview_start(_v, _i, FUDE_ZOOM_PDFVIEW_JOB_WHOLE, (rde_vec_2F){ 0.0f, 0.0f }, _p->points, FUDE_ZOOM_PDFVIEW_WHOLE_PX, fude_zoom_pdfview_whole_h(_p), 0.0f);
            return;
        }
    }
}

// What is far from the view let go: sharp pieces off screen; whole pages beyond
// the FUDE_ZOOM_PDFVIEW_KEEP nearest.
RDE_INTERNAL void fude_zoom_pdfview_let_go(fude_zoom_pdfview* _v, fude_zoom_sim _home, fude_zoom_v2 _half) {
    const fude_zoom_box _view = fude_zoom_pdfview_view(_home, _half, 0.0);
    const f64           _mid  = (_view.min_y + _view.max_y) * 0.5;
    u32 _kept = 0;
    for(u32 _i = 0; _i < _v->page_count; _i++) {
        fude_zoom_pdfview_page* _p = &fude_zoom_pdfview_pages(_v)[_i];
        if(!fude_zoom_pdfview_meets(_p->box, _view)) {
            fude_zoom_pdfview_tile_free(&_p->sharp);
        }
        _kept += _p->whole.texture != NULL ? 1u : 0u;
    }
    while(_kept > FUDE_ZOOM_PDFVIEW_KEEP) {
        i32 _farthest = -1;
        f64 _far      = -1.0;
        for(u32 _i = 0; _i < _v->page_count; _i++) {
            const fude_zoom_pdfview_page* _p = &fude_zoom_pdfview_pages(_v)[_i];
            const f64 _d = fabs((_p->box.min_y + _p->box.max_y) * 0.5 - _mid);
            if(_p->whole.texture != NULL && _d > _far) {
                _far      = _d;
                _farthest = (i32)_i;
            }
        }
        if(_farthest < 0) {
            break;
        }
        fude_zoom_pdfview_tile_free(&fude_zoom_pdfview_pages(_v)[_farthest].whole);
        _kept--;
    }
}

void fude_zoom_pdfview_update(fude_zoom_pdfview* _v, fude_zoom_sim _home, fude_zoom_v2 _half) {
    if(!fude_zoom_pdfview_is_open(_v)) {
        return;
    }
    // A dark theme: the pages in its colours (drawn again when it changes).
    const fude_theme* _t     = fude_theme_active();
    const b8          _dark  = 0.299f * (f32)_t->page.r + 0.587f * (f32)_t->page.g + 0.114f * (f32)_t->page.b < 128.0f;
    const rde_color   _paper = _dark ? fude_kit_shade(_t->page, 10) : (rde_color){ 255, 255, 255, 255 };
    if(_dark != _v->dark || memcmp(&_paper, &_v->paper, sizeof(_paper)) != 0 || memcmp(&_t->text, &_v->print, sizeof(_t->text)) != 0) {
        fude_zoom_pdfview_drop_job(_v, false);
        fude_zoom_pdfview_drop_tiles(_v);
        _v->dark  = _dark;
        _v->paper = _paper;
        _v->print = _t->text;
    }
    // The view resting, or moving.
    const f64 _now = rde_engine_get_time_now();
    if(memcmp(&_home, &_v->seen, sizeof(_home)) != 0) {
        _v->seen     = _home;
        _v->moved_at = _now;
    }
    fude_zoom_pdfview_take(_v);
    if(_v->job == NULL) {
        fude_zoom_pdfview_next(_v, _home, _half, _now - _v->moved_at >= FUDE_ZOOM_PDFVIEW_REST);
    }
    fude_zoom_pdfview_let_go(_v, _home, _half);
    // A search's matches as they come.
    if(_v->searching && !_v->search_done && _v->pdf != NULL) {
        b8 _done = false;
        _v->match_count = fude_pdf_find_matches(_v->pdf, fude_zoom_pdfview_matches(_v), FUDE_PDF_MATCHES, &_done);
        _v->search_done = _done;
    }
}

// Part of a page's picture (its rectangle _from-_size, in page points) over the
// page on screen — only what of it is in _clip (canvas units): never more than
// the screen, however deep the zoom.
RDE_INTERNAL void fude_zoom_pdfview_draw_tile(const fude_zoom_pdfview_page* _p, const fude_doc_tile* _tile, fude_zoom_sim _home, fude_zoom_box _clip) {
    if(_tile->texture == NULL) {
        return;
    }
    const f64 _k = fude_zoom_pdfview_k(_p);
    const fude_zoom_box _t = { _p->box.min_x + (f64)_tile->from.x * _k, _p->box.max_y - (f64)(_tile->from.y + _tile->size.y) * _k,
                               _p->box.min_x + (f64)(_tile->from.x + _tile->size.x) * _k, _p->box.max_y - (f64)_tile->from.y * _k };
    const fude_zoom_box _c = { fmax(_t.min_x, _clip.min_x), fmax(_t.min_y, _clip.min_y), fmin(_t.max_x, _clip.max_x), fmin(_t.max_y, _clip.max_y) };
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
    const f32 _du = fmaxf(_tr.x - _bl.x, 1e-6f), _dv = fmaxf(_tr.y - _bl.y, 1e-6f);
    rde_rendering_2d_draw_texture_partial_2(_tex, (rde_vec_3F){ (f32)_mid.x, (f32)_mid.y, 0.0f },
                                            (rde_vec_2F){ (f32)((_c.max_x - _c.min_x) * _s) / _du, (f32)((_c.max_y - _c.min_y) * _s) / _dv }, 0.0f,
                                            (rde_color){ 255, 255, 255, 255 }, _bl, _tr);
}

// A canvas box on the screen as a rectangle (clipped to _clip first).
RDE_INTERNAL void fude_zoom_pdfview_rect(fude_zoom_box _b, fude_zoom_box _clip, fude_zoom_sim _home, f64 _grow, rde_color _color) {
    const f64 _s = fude_zoom_sim_scale(_home);
    const fude_zoom_box _c = { fmax(_b.min_x, _clip.min_x), fmax(_b.min_y, _clip.min_y), fmin(_b.max_x, _clip.max_x), fmin(_b.max_y, _clip.max_y) };
    if(!(_c.max_x > _c.min_x && _c.max_y > _c.min_y)) {
        return;
    }
    const fude_zoom_v2 _mid = fude_zoom_sim_apply(_home, (fude_zoom_v2){ (_c.min_x + _c.max_x) * 0.5, (_c.min_y + _c.max_y) * 0.5 });
    rde_rendering_2d_draw_rectangle((rde_vec_2F){ (f32)_mid.x, (f32)_mid.y },
                                    (rde_vec_2F){ (f32)((_c.max_x - _c.min_x) * _s + _grow * 2.0), (f32)((_c.max_y - _c.min_y) * _s + _grow * 2.0) }, _color);
}

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
        const f64 _edge = 1.5 / fmax(_s, 1e-300);
        fude_zoom_pdfview_rect((fude_zoom_box){ _p->box.min_x - _edge, _p->box.min_y - _edge, _p->box.max_x + _edge, _p->box.max_y + _edge }, _view, _home, 0.0, _t->outline);
        fude_zoom_pdfview_rect(_p->box, _view, _home, 0.0, _v->paper);
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
        const fude_zoom_box _box = { fmin(_a.x, _b.x), fmin(_a.y, _b.y), fmax(_a.x, _b.x), fmax(_a.y, _b.y) };
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
    snprintf(_v->query, sizeof(_v->query), "%s", _query);
    fude_pdf_find_start(_v->pdf, _query);
}

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

b8 fude_zoom_pdfview_match_box(fude_zoom_pdfview* _v, u32 _i, fude_zoom_box* _box) {
    if(!_v->searching || _i >= _v->match_count || fude_zoom_pdfview_matches(_v)[_i].page >= _v->page_count) {
        return false;
    }
    const fude_pdf_match* _m = &fude_zoom_pdfview_matches(_v)[_i];
    const fude_zoom_v2 _a = fude_zoom_pdfview_to_canvas(_v, _m->page, _m->from);
    const fude_zoom_v2 _b = fude_zoom_pdfview_to_canvas(_v, _m->page, (rde_vec_2F){ _m->from.x + _m->size.x, _m->from.y + _m->size.y });
    *_box        = (fude_zoom_box){ fmin(_a.x, _b.x), fmin(_a.y, _b.y), fmax(_a.x, _b.x), fmax(_a.y, _b.y) };
    _v->match_at = _i;
    return true;
}

b8 fude_zoom_pdfview_search_wanted(fude_zoom_pdfview* _v, fude_zoom_box* _box) {
    if(!_v->search_go || _v->match_count == 0u) {
        return false;
    }
    _v->search_go = false;
    return fude_zoom_pdfview_match_box(_v, 0u, _box);
}

// --- the PDF with what was drawn on it ------------------------------------------------------

typedef struct { u32 object; u64 key; } fude_zoom_pdfview_drawn;

RDE_INTERNAL int fude_zoom_pdfview_by_key(const void* _a, const void* _b) {
    const u64 _x = ((const fude_zoom_pdfview_drawn*)_a)->key, _y = ((const fude_zoom_pdfview_drawn*)_b)->key;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// On white paper: the theme's ink as the light theme's.
RDE_INTERNAL rde_color fude_zoom_pdfview_ink(rde_color _c) {
    if(_c.a == 1u && _c.r == 255u && _c.g == 254u && _c.b == 253u) {
        return (rde_color){ 255, 255, 255, 255 };   // (FUDE_THEME_PAGE_FILL: white paper)
    }
    return fude_theme_is_ink(_c) ? fude_theme_get(FUDE_THEME_PAPER)->ink : fude_theme_resolve(_c);
}

// A canvas box from frame units through _sim (home units).
RDE_INTERNAL fude_zoom_box fude_zoom_pdfview_box_through(fude_zoom_sim _sim, fude_zoom_box _b) {
    fude_zoom_box _out = fude_zoom_box_empty();
    const fude_zoom_v2 _c[4] = { { _b.min_x, _b.min_y }, { _b.max_x, _b.min_y }, { _b.max_x, _b.max_y }, { _b.min_x, _b.max_y } };
    for(u32 _i = 0; _i < 4u; _i++) {
        const fude_zoom_v2 _p = fude_zoom_sim_apply(_sim, _c[_i]);
        _out = fude_zoom_box_union(_out, (fude_zoom_box){ _p.x, _p.y, _p.x, _p.y });
    }
    return _out;
}

b8 fude_zoom_pdfview_export(const fude_zoom_pdfview* _v, const fude_zoom_scene* _s, rde_font* _font, f32 _font_px, const c8* _out) {
    if(!fude_zoom_pdfview_is_open(_v)) {
        return false;
    }
    fude_pdf_writer* _w = fude_pdf_write_begin(_out);
    if(_w == NULL) {
        return false;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    // What is drawn (alive, shown: a line, a fill, a shape), in the order it was.
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
    qsort(_drawn.memory, _nd, sizeof(fude_zoom_pdfview_drawn), fude_zoom_pdfview_by_key);
    rde_arr _q     = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
    rde_arr _pts   = rde_arr_new(sizeof(rde_vec_2F), _heap);
    rde_arr _radii = rde_arr_new(sizeof(f32), _heap);
    rde_arr _local = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr _rings = rde_arr_new(sizeof(u32), _heap);
    rde_arr _tris  = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr _tpts  = rde_arr_new(sizeof(rde_vec_2F), _heap);
    rde_arr _text  = rde_arr_new(sizeof(c8), _heap);
    for(u32 _page = 0; _page < _v->page_count; _page++) {
        const fude_zoom_pdfview_page* _p = &fude_zoom_pdfview_pages(_v)[_page];
        const f64 _k = fude_zoom_pdfview_k(_p);   // canvas units to a page point
        fude_pdf_write_page(_w, _v->pdf, _page);
        for(u32 _pass = 0; _pass < 2u; _pass++) {   // the markers' first, under the rest
            for(u32 _d = 0; _d < _nd; _d++) {
                const u32               _i = ((const fude_zoom_pdfview_drawn*)_drawn.memory)[_d].object;
                const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
                if(((_o->flags & FUDE_ZOOM_FLAG_MARKER) != 0) != (_pass == 0u)) {
                    continue;
                }
                const fude_zoom_sim _to_home = fude_zoom_scene_sim(_s, _o->frame, _s->home);
                if(!fude_zoom_pdfview_meets(fude_zoom_pdfview_box_through(_to_home, _o->box), _p->box)) {
                    continue;   // (not on this page)
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
                    fude_zoom_scene_image_corners(_s, _i, _c4);   // (bottom-left, bottom-right, top-right, top-left)
                    const rde_vec_2F _three[3] = { fude_zoom_pdfview_to_page(_v, _page, fude_zoom_sim_apply(_to_home, _c4[0])),
                                                   fude_zoom_pdfview_to_page(_v, _page, fude_zoom_sim_apply(_to_home, _c4[1])),
                                                   fude_zoom_pdfview_to_page(_v, _page, fude_zoom_sim_apply(_to_home, _c4[3])) };
                    fude_pdf_write_image(_w, _bytes, _size, _three);
                    continue;
                }
                if(_o->kind == FUDE_ZOOM_KIND_TEXT) {
                    // A text (a note's paper first): its lines as the canvas breaks them, each from its left.
                    f64 _size = 0.0, _tw = 0.0, _th = 0.0;
                    u8  _style = 0;
                    const c8* _words = NULL;
                    u32 _len = 0;
                    if(!fude_zoom_scene_text(_s, _i, &_size, &_tw, &_th, &_style, &_words, &_len) || _len == 0u || _font == NULL) {
                        continue;
                    }
                    const fude_zoom_sim _all = fude_zoom_sim_compose(_to_home, fude_zoom_object_sim(_o));
                    const f64 _kp  = fude_zoom_sim_scale(_all) / _k;   // a local unit, in page points
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
                    rde_arr_resize(&_text, (usize)_len + 1u);
                    c8* _tx = (c8*)_text.memory;
                    memcpy(_tx, _words, _len);
                    _tx[_len] = 0;
                    const f32 _px = (f32)(_size * _kp), _wide = (f32)fmax((_tw - 2.0 * _pad) * _kp, 1.0);
                    u32 _from[32], _to[32];
                    const u32 _lines = fude_draw_text_wrap_spans(_font, _font_px, _tx, _px, _wide, _from, _to, 32u);
                    for(u32 _l = 0; _l < _lines; _l++) {
                        c8 _row[512];
                        const usize _n = (usize)(_to[_l] - _from[_l]) < sizeof(_row) - 1u ? (usize)(_to[_l] - _from[_l]) : sizeof(_row) - 1u;
                        memcpy(_row, _tx + _from[_l], _n);
                        _row[_n] = 0;
                        const fude_zoom_v2 _at = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _pad, -_pad - _size * 0.95 - (f64)_l * _size * 1.3 });
                        fude_pdf_write_text(_w, _row, fude_zoom_pdfview_to_page(_v, _page, _at), _px, _color);
                    }
                    continue;
                }
                if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
                    // What it looks like: its outline, filled when it is, its line over it.
                    b8 _closed = false;
                    rde_arr_clear(&_local);
                    fude_zoom_scene_shape_outline(_s, _i, 96u, &_local, &_closed);
                    const u32 _n = (u32)rde_arr_length(&_local);
                    if(_n < 2u) {
                        continue;
                    }
                    rde_arr_clear(&_pts);
                    rde_arr_clear(&_radii);
                    const f32 _r = (f32)((f64)_o->radius * _o->scale * fude_zoom_sim_scale(_to_home) / _k);
                    for(u32 _j = 0; _j < _n + (_closed ? 1u : 0u); _j++) {
                        const fude_zoom_v2 _h = fude_zoom_sim_apply(_to_home, ((const fude_zoom_v2*)_local.memory)[_j % _n]);
                        const rde_vec_2F   _a = fude_zoom_pdfview_to_page(_v, _page, _h);
                        rde_arr_add(&_pts, (any)&_a);
                        rde_arr_add(&_radii, (any)&_r);
                    }
                    if(_closed && (_o->flags & FUDE_ZOOM_FLAG_FILLED) && _n >= 3u) {
                        rde_arr_clear(&_local);   // (the outline again, in page points, to fill)
                        for(u32 _j = 0; _j < _n; _j++) {
                            const rde_vec_2F _a = ((const rde_vec_2F*)_pts.memory)[_j];
                            const fude_zoom_v2 _b = { _a.x, _a.y };
                            rde_arr_add(&_local, (any)&_b);
                        }
                        rde_arr_clear(&_tris);
                        const u32 _t = fude_zoom_fill_triangulate((const fude_zoom_v2*)_local.memory, _n, &_tris);
                        rde_arr_clear(&_tpts);
                        for(u32 _j = 0; _j < _t * 3u; _j++) {
                            const fude_zoom_v2 _c = ((const fude_zoom_v2*)_tris.memory)[_j];
                            const rde_vec_2F   _a = { (f32)_c.x, (f32)_c.y };
                            rde_arr_add(&_tpts, (any)&_a);
                        }
                        fude_pdf_write_fill(_w, (const rde_vec_2F*)_tpts.memory, _t, fude_zoom_pdfview_ink((_o->flags & FUDE_ZOOM_FLAG_FILL_OWN) ? _o->fill : _o->color));
                    }
                    if(_r > 0.0f) {
                        fude_pdf_write_stroke(_w, (const rde_vec_2F*)_pts.memory, (const f32*)_radii.memory, (u32)rde_arr_length(&_pts), _color, false);
                    }
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
                const f64               _ks  = fude_zoom_sim_scale(_all) / _k;   // a local unit, in page points
                if(_o->kind == FUDE_ZOOM_KIND_STROKE) {
                    rde_arr_clear(&_pts);
                    rde_arr_clear(&_radii);
                    for(u32 _j = 0; _j < _o->count; _j++) {
                        const fude_zoom_v2 _h = fude_zoom_sim_apply(_all, (fude_zoom_v2){ ldexp((f64)_qp[_j].x, _o->q), ldexp((f64)_qp[_j].y, _o->q) });
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
                    const fude_zoom_v2 _h = fude_zoom_sim_apply(_all, (fude_zoom_v2){ ldexp((f64)_qp[_j].x, _o->q), ldexp((f64)_qp[_j].y, _o->q) });
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
