// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/doc/worker.h"
#include "drawing/base/redraw.h"
#include "drawing/base/theme.h"

// ===========================================================================
// See worker.h.
//
// In order: the piece's life (freeing it), what the thread does to its pixels (darken) and the thread's body,
// then the main thread's calls (init, colours, forget, start, take, wait, drop), the tiles, and the whole pages' plan.
// A piece is on the engine's pool (rde_calloc), its pixels on the std allocator; its done and orphaned are read and
// written under fude_doc_worker_mutex, everything else in it belongs to one thread at a time (the main thread until it
// is started, the thread until done, then whoever frees it).
// ===========================================================================

// Guards each piece's done and orphaned, which the main thread and the piece's thread share. The first
// fude_doc_worker_init makes it; it is never freed.
RDE_INTERNAL rde_mutex fude_doc_worker_mutex = NULL;
// Broadcast, under the mutex, each time a piece is done, for fude_doc_worker_wait. Made with the mutex; never freed.
RDE_INTERNAL rde_cond  fude_doc_worker_done  = NULL;

// --- the piece -------------------------------------------------------------------------------

// Frees a piece and its pixels, and closes its PDF too when the PDF was let go while the piece was being drawn.
RDE_INTERNAL void fude_doc_piece_release(fude_doc_piece* _piece) {
    if(_piece->close_pdf) {
        fude_pdf_close(_piece->pdf);
    }
    rde_arr_free(&_piece->pixels);
    rde_free(_piece);
}

// See the header. A piece handed back is never orphaned, so its PDF is the caller's still.
void fude_doc_piece_free(fude_doc_piece* _piece) {
    _piece->close_pdf = false;
    fude_doc_piece_release(_piece);
}

// --- the thread ------------------------------------------------------------------------------

// Recolours a drawn page for a dark theme: white paper becomes the paper colour and black print the print colour.
// Each pixel is placed between the two by how light it is (its luma, weights 77, 150 and 29 out of 256), and 3/5 of
// how far its own colour is from that grey is kept, so coloured print stays a little coloured.
RDE_INTERNAL void fude_doc_worker_darken(fude_doc_piece* _piece) {
    const usize     _n     = (usize)_piece->w * (usize)_piece->h;
    u8*             _p     = _piece->pixels.memory;
    const rde_color _paper = _piece->paper;
    const rde_color _print = _piece->print;
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

// The thread's body: draws the piece (the top row first, as pdf.h draws it and as RDE's textures hold their rows); a
// piece to show is, in a dark theme, recoloured (a reader's picture stays in its own colours); then it is marked done.
// When the main thread has let it go meanwhile, the thread frees it here, and closes its PDF with it.
RDE_INTERNAL any fude_doc_worker_work(rde_thread* _thread, any _data) {
    RDE_UNUSED(_thread);
    fude_doc_piece* _piece = (fude_doc_piece*)_data;
    _piece->ok = fude_pdf_render(_piece->pdf, _piece->page, _piece->from, _piece->size, _piece->w, _piece->h, _piece->pixels.memory);
    if(_piece->ok && _piece->kind != FUDE_DOC_PIECE_READ && _piece->dark) {
        fude_doc_worker_darken(_piece);
    }
    rde_mutex_lock(fude_doc_worker_mutex);
    const b8 _orphaned = _piece->orphaned;
    _piece->done       = true;
    rde_cond_broadcast(fude_doc_worker_done);   // (a PDF being written out may wait for it: fude_doc_worker_wait)
    rde_mutex_unlock(fude_doc_worker_mutex);
    if(_orphaned) {
        fude_doc_piece_release(_piece);
    } else {
        fude_redraw_request();   // (render on demand: wakes the main thread, which takes the piece and draws it)
    }
    return NULL;
}

// --- the main thread -------------------------------------------------------------------------

// See the header.
void fude_doc_worker_init(fude_doc_worker* _worker) {
    rde_memset(_worker, 0, sizeof(*_worker));
    if(fude_doc_worker_mutex == NULL) {
        fude_doc_worker_mutex = rde_mutex_create();
        fude_doc_worker_done  = rde_cond_create();
    }
}

// See the header.
b8 fude_doc_worker_busy(const fude_doc_worker* _worker) {
    return _worker->job != NULL;
}

// See the header. The paper is a shade off a dark theme's page colour, so the page stands out from the canvas round it.
b8 fude_doc_worker_colours(fude_doc_worker* _worker) {
    const fude_theme* _t     = fude_theme_active();
    const b8          _dark  = 0.299f * (f32)_t->page.r + 0.587f * (f32)_t->page.g + 0.114f * (f32)_t->page.b < 128.0f;
    const rde_color   _paper = _dark ? fude_theme_shade(_t->page, 10) : (rde_color){ 255, 255, 255, 255 };
    if(_dark == _worker->dark && rde_memcmp(&_paper, &_worker->paper, sizeof(_paper)) == 0 && rde_memcmp(&_t->text, &_worker->print, sizeof(_t->text)) == 0) {
        return false;
    }
    _worker->stale = _worker->job != NULL;   // The piece being drawn is in the old colours: thrown away when done.
    _worker->dark  = _dark;
    _worker->paper = _paper;
    _worker->print = _t->text;
    return true;
}

// See the header. Like a theme's change, it leaves the piece the worker's until it is through.
void fude_doc_worker_forget(fude_doc_worker* _worker) {
    _worker->stale = _worker->stale || _worker->job != NULL;
}

// See the header. The piece comes from the engine's allocator, which is thread-safe, because its thread may be the one
// to free it; its room for the pixels is made at once, for the thread to fill (not grown, not cleared: the drawing
// covers every pixel).
void fude_doc_worker_start(fude_doc_worker* _worker, fude_pdf* _pdf, u32 _page, u8 _kind, rde_vec_2F _from, rde_vec_2F _size, u32 _w, u32 _h, f32 _zoom) {
    fude_doc_piece* _piece = (fude_doc_piece*)rde_calloc(1, sizeof(fude_doc_piece));
    _piece->pdf   = _pdf;
    _piece->page  = _page;
    _piece->kind  = _kind;
    _piece->from  = _from;
    _piece->size  = _size;
    _piece->w     = _w;
    _piece->h     = _h;
    _piece->zoom  = _zoom;
    _piece->dark  = _worker->dark;
    _piece->paper = _worker->paper;
    _piece->print = _worker->print;
    const usize _bytes = (usize)_w * (usize)_h * 4u;
    // One byte more than needed, so adding the pixels below never has to grow the array.
    _piece->pixels = rde_arr_new_with_capacity(sizeof(u8), _bytes + 1u, rde_memory_allocator_get_default_std());
    rde_arr_add_n(&_piece->pixels, _bytes);
    _worker->job = _piece;
    rde_thread_run_detached(fude_doc_worker_work, _piece, NULL);
}

// See the header.
fude_doc_piece* fude_doc_worker_take(fude_doc_worker* _worker) {
    fude_doc_piece* _piece = _worker->job;
    if(_piece == NULL) {
        return NULL;
    }
    rde_mutex_lock(fude_doc_worker_mutex);
    const b8 _done = _piece->done;
    rde_mutex_unlock(fude_doc_worker_mutex);
    if(!_done) {
        return NULL;
    }
    _worker->job = NULL;
    if(_worker->stale) {
        _worker->stale = false;   // Drawn the old way (the colours or the pages changed meanwhile): thrown away.
        fude_doc_piece_free(_piece);
        return NULL;
    }
    return _piece;
}

// See the header. The piece is the worker's until it is taken, so its thread never frees it (it is not orphaned) and
// it is still there to look at once done.
void fude_doc_worker_wait(const fude_doc_worker* _worker) {
    const fude_doc_piece* _piece = _worker->job;
    if(_piece == NULL) {
        return;
    }
    rde_mutex_lock(fude_doc_worker_mutex);
    while(!_piece->done) {
        rde_cond_wait(fude_doc_worker_done, fude_doc_worker_mutex);
    }
    rde_mutex_unlock(fude_doc_worker_mutex);
}

// See the header. A piece only ever outlives its PDF this way: while the PDF is open, its worker has one piece at a
// time, and that piece stays its own until taken (a colour change marks it stale instead).
b8 fude_doc_worker_drop(fude_doc_worker* _worker) {
    fude_doc_piece* _piece = _worker->job;
    _worker->job           = NULL;
    _worker->stale         = false;
    if(_piece == NULL) {
        return false;
    }
    rde_mutex_lock(fude_doc_worker_mutex);
    const b8 _done = _piece->done;
    if(!_done) {
        _piece->close_pdf = true;
        _piece->orphaned  = true;
    }
    rde_mutex_unlock(fude_doc_worker_mutex);
    if(_done) {
        fude_doc_piece_free(_piece);   // (the caller closes the PDF itself)
        return false;
    }
    return true;   // (its thread closes the PDF, once it has finished drawing with it)
}

// --- tiles -----------------------------------------------------------------------------------

// See the header.
void fude_doc_tile_free(fude_doc_tile* _tile) {
    if(_tile->texture != NULL) {
        rde_memory_texture_destroy(_tile->texture);
    }
    rde_memset(_tile, 0, sizeof(*_tile));
}

// See the header. The memory texture is made on the std allocator, as anything as big as a page is: the engine's pool
// is a fixed block (the app's config.rdef), which a few pages and a sharp piece (up to 64 MB) would fill. Its copy of
// the pixels stays as long as the texture, since RDE's upload keeps it (it does not look at the parameters' free_data).
void fude_doc_tile_take(fude_doc_tile* _tile, const fude_doc_piece* _piece) {
    fude_doc_tile_free(_tile);
    _tile->texture = rde_memory_texture_create(_piece->w, _piece->h, 4u, rde_memory_allocator_get_default_std());
    rde_memcpy(rde_memory_texture_get_pixels(_tile->texture), _piece->pixels.memory, (usize)_piece->w * (usize)_piece->h * 4u);
    rde_texture_parameters _params = RDE_DEFAULT_TEXTURE_PARAMETERS;
    _params.wrap_s                 = RDE_TEXTURE_PARAMETER_TYPE_WRAP_CLAMP_TO_EDGE;
    _params.wrap_t                 = RDE_TEXTURE_PARAMETER_TYPE_WRAP_CLAMP_TO_EDGE;
    _params.generate_mipmap        = _piece->kind == FUDE_DOC_PIECE_WHOLE;   // It is seen small too.
    rde_memory_texture_gpu_upload(_tile->texture, &_params);
    _tile->from = _piece->from;
    _tile->size = _piece->size;
    _tile->zoom = _piece->zoom;
    _tile->w    = _piece->w;
    _tile->h    = _piece->h;
}

// --- whole pages: which to keep, and which to draw ------------------------------------------

// A whole page's picture is drawn again once the page is this much wider on screen than its level (it would show
// blurred), or its level this many times the page's width on screen (it would hold pixels nobody sees). A picture just
// drawn is from once to twice the width, well inside both, so a still view never draws a page again.
#define FUDE_DOC_WHOLE_FINER   1.15f
#define FUDE_DOC_WHOLE_COARSER 2.3f

// See the header. The sizes are worked out in f32, as the pieces' always were: a page at FUDE_DOC_WHOLE_PX gets the
// height it always had.
void fude_doc_whole_size(rde_vec_2F _points, f32 _px, u32* _level, u32* _w, u32* _h) {
    u32 _l = FUDE_DOC_WHOLE_PX;
    while(_l / 2u >= FUDE_DOC_WHOLE_LEAST && (f32)(_l / 2u) >= _px) {
        _l /= 2u;
    }
    const f32 _tall = (f32)_l * _points.y / rde_math_max_f32(1.0f, _points.x);
    *_level = _l;
    if(_tall > (f32)FUDE_DOC_SHARP_MAX) {
        *_w = (u32)rde_math_max_f32(1.0f, (f32)_l * (f32)FUDE_DOC_SHARP_MAX / _tall);   // (a long receipt, narrower)
        *_h = FUDE_DOC_SHARP_MAX;
    } else {
        *_w = _l;
        *_h = (u32)rde_math_max_f32(1.0f, _tall);
    }
}

// Whether a whole picture drawn for level _level suits a page _px pixels wide on screen: sharp enough (at most
// FUDE_DOC_WHOLE_FINER narrower than the page, or as wide as a whole picture is drawn), and not wastefully big (at most
// FUDE_DOC_WHOLE_COARSER times its width, or as small as one is drawn).
RDE_INTERNAL b8 fude_doc_whole_fits(u32 _level, f32 _px) {
    const b8 _sharp = _px <= (f32)_level * FUDE_DOC_WHOLE_FINER || _level >= FUDE_DOC_WHOLE_PX;
    const b8 _small = (f32)_level <= _px * FUDE_DOC_WHOLE_COARSER || _level <= FUDE_DOC_WHOLE_LEAST;
    return _sharp && _small;
}

// rde_sort's order for fude_doc_wholes: on screen, then near, then away; within each, nearest the middle first, then
// by page, so the order is the same however the sort goes.
RDE_INTERNAL int fude_doc_seen_order(const any _a, const any _b) {
    const fude_doc_seen* _x = (const fude_doc_seen*)_a;
    const fude_doc_seen* _y = (const fude_doc_seen*)_b;
    if(_x->where != _y->where) {
        return _x->where > _y->where ? -1 : 1;
    }
    if(_x->far != _y->far) {
        return _x->far < _y->far ? -1 : 1;
    }
    return _x->page < _y->page ? -1 : (_x->page > _y->page ? 1 : 0);
}

// See the header. The pages that count (on screen, near, or holding a picture) are moved to the front and sorted; the
// rest of a long document is left where it is. They are then gone through in order, adding up what each kept picture
// costs: once one off screen does not fit, it and every one after it are let go. A page too small on screen to draw
// counts as away (its picture kept while there is room, never drawn again).
void fude_doc_wholes(fude_doc_seen* _seen, u32 _n, u32* _screen, u32* _near) {
    *_screen = UINT32_MAX;
    *_near   = UINT32_MAX;
    u32 _count = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        if(_seen[_i].px < FUDE_DOC_SEEN_LEAST) {
            _seen[_i].where = FUDE_DOC_AWAY;
        }
        if(_seen[_i].where != FUDE_DOC_AWAY || _seen[_i].whole->texture != NULL) {
            const fude_doc_seen _swap = _seen[_count];
            _seen[_count++]           = _seen[_i];
            _seen[_i]                 = _swap;
        }
    }
    rde_sort(_seen, _count, sizeof(fude_doc_seen), fude_doc_seen_order);
    u64 _used = 0;
    b8  _full = false;
    for(u32 _i = 0; _i < _count; _i++) {
        const fude_doc_seen* _s    = &_seen[_i];
        const fude_doc_tile* _t    = _s->whole;
        u32                  _level, _w, _h;
        fude_doc_whole_size(_s->points, _s->px, &_level, &_w, &_h);
        // Drawn again only when on screen or near; away, a picture is kept as it is.
        const b8  _draw = _s->where != FUDE_DOC_AWAY && (_t->texture == NULL || !fude_doc_whole_fits((u32)_t->zoom, _s->px));
        const u64 _cost = _draw ? (u64)_w * (u64)_h * 4u : (u64)_t->w * (u64)_t->h * 4u;
        if(_s->where != FUDE_DOC_ON_SCREEN) {
            _full = _full || _used + _cost > (u64)FUDE_DOC_KEEP_BYTES;
            if(_full) {
                fude_doc_tile_free(_s->whole);   // (not kept: let go, and not drawn)
                continue;
            }
        }
        _used += _cost;
        if(_draw && _s->where == FUDE_DOC_ON_SCREEN && *_screen == UINT32_MAX) {
            *_screen = _s->page;
        }
        if(_draw && _s->where == FUDE_DOC_NEAR && *_near == UINT32_MAX) {
            *_near = _s->page;
        }
    }
}
