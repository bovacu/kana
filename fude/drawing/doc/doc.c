#include "drawing/doc/doc.h"
#include "drawing/app/app.h"
#include "drawing/base/save.h"
#include "drawing/base/kfile.h"
#include "drawing/base/utf8.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "drawing/ink/notes.h"
#include "drawing/widgets/draw.h"
#include "drawing/widgets/kit.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See doc.h. One piece is drawn at a time, on a thread started for it (RDE's
// detached threads): the main thread hands it a job and looks each frame
// whether it is done. A document let go while its piece is being drawn leaves
// the job to the worker to finish and free (and the PDF with it, to close).
// ===========================================================================

#define FUDE_DOC_PILL_TIME 1.2    // the page pill: seconds shown after the view stops
#define FUDE_DOC_PILL_FADE 0.35

// What a job draws: a page whole, the sharp piece, or a page to read its text.
typedef enum { FUDE_DOC_JOB_WHOLE = 0, FUDE_DOC_JOB_SHARP, FUDE_DOC_JOB_READ } FUDE_DOC_JOB_;

// The read lines' file (kfile.h): kind 'LINS'; a 'PAGE' chunk for each page read —
// u32 page, u32 count, then per line f32 x, y, w, h (points from the page's
// top-left), its text (u16 length, the bytes). A page read with no line has its
// chunk too: it is not read again.
#define FUDE_DOC_LINES_VERSION 1u
#define FUDE_DOC_LINES_KIND    FUDE_TAG('L', 'I', 'N', 'S')
#define FUDE_DOC_LINES_PAGE    FUDE_TAG('P', 'A', 'G', 'E')

typedef struct fude_doc_job {
    fude_pdf*  pdf;
    b8         close_pdf;   // the document was let go: the worker closes the PDF too
    u32        page;
    u8         kind;        // FUDE_DOC_JOB_
    rde_vec_2F from, size;  // the part of the page, in its points
    u32        w, h;
    f32        zoom;
    b8         dark;
    rde_color  paper, print;
    u8*        pixels;
    b8         ok;
    b8         done;        // the worker is through (under the mutex)
    b8         orphaned;    // the main thread let it go (under the mutex)
} fude_doc_job;

RDE_INTERNAL rde_mutex fude_doc_mutex = NULL;

// --- the worker -----------------------------------------------------------------------------

RDE_INTERNAL void fude_doc_job_free(fude_doc_job* _job) {
    if(_job->close_pdf) {
        fude_pdf_close(_job->pdf);
    }
    free(_job->pixels);
    free(_job);
}

// A dark theme's page: white paper in the paper colour, black print in the
// print colour — by how light each pixel is, a little of its own colour kept.
RDE_INTERNAL void fude_doc_darken(fude_doc_job* _job) {
    const usize     _n     = (usize)_job->w * (usize)_job->h;
    u8*             _p     = _job->pixels;
    const rde_color _paper = _job->paper;
    const rde_color _print = _job->print;
    for(usize _i = 0; _i < _n; _i++, _p += 4) {
        const i32 _l    = ((i32)_p[0] * 77 + (i32)_p[1] * 150 + (i32)_p[2] * 29) >> 8;
        const i32 _r    = (i32)_print.r + ((i32)_paper.r - (i32)_print.r) * _l / 255 + ((i32)_p[0] - _l) * 3 / 5;
        const i32 _g    = (i32)_print.g + ((i32)_paper.g - (i32)_print.g) * _l / 255 + ((i32)_p[1] - _l) * 3 / 5;
        const i32 _b    = (i32)_print.b + ((i32)_paper.b - (i32)_print.b) * _l / 255 + ((i32)_p[2] - _l) * 3 / 5;
        _p[0]           = (u8)(_r < 0 ? 0 : _r > 255 ? 255 : _r);
        _p[1]           = (u8)(_g < 0 ? 0 : _g > 255 ? 255 : _g);
        _p[2]           = (u8)(_b < 0 ? 0 : _b > 255 ? 255 : _b);
        _p[3]           = 255u;
    }
}

// Rows turned over: a memory texture's first row is its bottom (pdf.h's, the top).
RDE_INTERNAL void fude_doc_flip(fude_doc_job* _job) {
    const usize _row = (usize)_job->w * 4u;
    u8*         _tmp = (u8*)malloc(_row);
    if(_tmp == NULL) {
        return;
    }
    for(u32 _y = 0; _y < _job->h / 2u; _y++) {
        u8* _a = _job->pixels + (usize)_y * _row;
        u8* _b = _job->pixels + (usize)(_job->h - 1u - _y) * _row;
        memcpy(_tmp, _a, _row);
        memcpy(_a, _b, _row);
        memcpy(_b, _tmp, _row);
    }
    free(_tmp);
}

RDE_INTERNAL any fude_doc_work(rde_thread* _thread, any _data) {
    RDE_UNUSED(_thread);
    fude_doc_job* _job = (fude_doc_job*)_data;
    _job->ok = fude_pdf_render(_job->pdf, _job->page, _job->from, _job->size, _job->w, _job->h, _job->pixels);
    if(_job->ok && _job->kind != FUDE_DOC_JOB_READ) {   // a picture to read stays as drawn: the top row first, its own colours
        fude_doc_flip(_job);
        if(_job->dark) {
            fude_doc_darken(_job);
        }
    }
    rde_mutex_lock(fude_doc_mutex);
    const b8 _orphaned = _job->orphaned;
    _job->done         = true;
    rde_mutex_unlock(fude_doc_mutex);
    if(_orphaned) {
        fude_doc_job_free(_job);
    }
    return NULL;
}

// --- opening and letting go -------------------------------------------------------------------

RDE_INTERNAL void fude_doc_tile_free(fude_doc_tile* _tile) {
    if(_tile->texture != NULL) {
        rde_memory_texture_destroy(_tile->texture);
    }
    memset(_tile, 0, sizeof(*_tile));
}

RDE_INTERNAL void fude_doc_drop_tiles(fude_doc* _doc) {
    for(u32 _i = 0; _i < _doc->page_count; _i++) {
        fude_doc_tile_free(&_doc->pages[_i].whole);
        fude_doc_tile_free(&_doc->pages[_i].sharp);
    }
}

// The piece being drawn let go: freed here when it is done, else by the worker
// (closing the PDF too, when _with_pdf).
RDE_INTERNAL void fude_doc_drop_job(fude_doc* _doc, b8 _with_pdf) {
    fude_doc_job* _job = _doc->job;
    _doc->job          = NULL;
    if(_job == NULL) {
        return;
    }
    rde_mutex_lock(fude_doc_mutex);
    const b8 _done = _job->done;
    if(!_done) {
        _job->close_pdf = _with_pdf;
        _job->orphaned  = true;
    }
    rde_mutex_unlock(fude_doc_mutex);
    if(_done) {
        _job->close_pdf = false;   // the caller closes it
        fude_doc_job_free(_job);
    } else if(_with_pdf) {
        _doc->pdf = NULL;          // the worker's to close now
    }
}

RDE_INTERNAL void fude_doc_close(fude_doc* _doc) {
    fude_doc_search_stop(_doc);
    if(_doc->reading != UINT32_MAX) {
        _doc->reader_drain = true;   // its answer, when it comes, is no one's
    }
    _doc->reading       = UINT32_MAX;
    _doc->wanted        = UINT32_MAX;
    _doc->probe         = 0u;
    _doc->reader_off    = false;
    _doc->lines_path[0] = 0;
    if(rde_arr_is_inited(&_doc->lines)) {
        rde_arr_clear(&_doc->lines);
    }
    fude_doc_drop_job(_doc, true);
    fude_doc_drop_tiles(_doc);
    if(_doc->pdf != NULL) {
        fude_pdf_close(_doc->pdf);
        _doc->pdf = NULL;
    }
    free(_doc->pages);
    free(_doc->turned);
    _doc->pages      = NULL;
    _doc->turned     = NULL;
    _doc->page_count = 0;
    _doc->path[0]    = 0;
    _doc->failed     = false;
}

// --- the read lines' file ---------------------------------------------------------------------

RDE_INTERNAL void fude_doc_lines_save(fude_doc* _doc) {
    if(_doc->lines_path[0] == 0) {
        return;
    }
    const fude_doc_line* _lines = (const fude_doc_line*)_doc->lines.memory;
    const u32            _n     = (u32)rde_arr_length(&_doc->lines);
    fude_bytes           _b     = fude_bytes_new(64u + _n * 48u);
    fude_put_header(&_b, FUDE_DOC_LINES_VERSION, FUDE_DOC_LINES_KIND);
    for(u32 _p = 0; _p < _doc->page_count; _p++) {
        if(_doc->pages[_p].text != FUDE_DOC_TEXT_READ) {
            continue;
        }
        u32 _count = 0;
        for(u32 _i = 0; _i < _n; _i++) {
            _count += _lines[_i].page == _p ? 1u : 0u;
        }
        const u32 _chunk = fude_chunk_begin(&_b, FUDE_DOC_LINES_PAGE);
        fude_put_u32(&_b, _p);
        fude_put_u32(&_b, _count);
        for(u32 _i = 0; _i < _n; _i++) {
            if(_lines[_i].page != _p) {
                continue;
            }
            fude_put_f32(&_b, _lines[_i].from.x);
            fude_put_f32(&_b, _lines[_i].from.y);
            fude_put_f32(&_b, _lines[_i].size.x);
            fude_put_f32(&_b, _lines[_i].size.y);
            const u32 _len = (u32)strlen(_lines[_i].text);
            fude_put_u16(&_b, (u16)_len);
            fude_put_data(&_b, _lines[_i].text, _len);
        }
        fude_chunk_end(&_b, _chunk);
    }
    if(!fude_bytes_write_and_free(&_b, _doc->lines_path, NULL)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "fude: the document's read text could not be saved (%s)", _doc->lines_path);
    }
}

// Where its read lines are kept — beside its PDF, when that is the learner's own
// (in the save folder) — and those read before.
RDE_INTERNAL void fude_doc_lines_load(fude_doc* _doc) {
    _doc->lines_path[0] = 0;
    const c8*   _dir = fude_save_dir();
    const usize _n   = strlen(_doc->path);
    if(strncmp(_doc->path, _dir, strlen(_dir)) != 0 || _n < 4u || strcmp(&_doc->path[_n - 4u], ".pdf") != 0) {
        return;   // a book of the app's: not kept (they have text of their own)
    }
    snprintf(_doc->lines_path, sizeof(_doc->lines_path), "%.*s.lines", (i32)(_n - 4u), _doc->path);
    if(!rde_file_exists(_doc->lines_path)) {
        return;
    }
    u32         _size = 0;
    u8*         _data = fude_file_read(_doc->lines_path, &_size);
    fude_reader _r    = fude_reader_make(_data, _size);
    if(_data == NULL || !fude_read_header(&_r, FUDE_DOC_LINES_VERSION, FUDE_DOC_LINES_KIND)) {
        fude_file_free(_data);
        return;
    }
    u32         _tag;
    fude_reader _c;
    while(fude_next_chunk(&_r, &_tag, &_c)) {
        if(_tag != FUDE_DOC_LINES_PAGE) {
            continue;
        }
        const u32 _page  = fude_get_u32(&_c);
        const u32 _count = fude_get_u32(&_c);
        if(!_c.ok || _page >= _doc->page_count) {
            continue;
        }
        for(u32 _i = 0; _i < _count && _c.ok; _i++) {
            fude_doc_line _line = { .page = _page };
            _line.from.x = fude_get_f32(&_c);
            _line.from.y = fude_get_f32(&_c);
            _line.size.x = fude_get_f32(&_c);
            _line.size.y = fude_get_f32(&_c);
            const u32 _len = fude_get_u16(&_c);
            if(!_c.ok || _len > _c.size - _c.pos) {
                break;
            }
            const u32 _keep = _len < FUDE_DOC_LINE_TEXT - 1u ? _len : FUDE_DOC_LINE_TEXT - 1u;
            memcpy(_line.text, &_c.data[_c.pos], _keep);
            _line.text[_keep] = 0;
            _c.pos += _len;
            rde_arr_add(&_doc->lines, &_line);
        }
        _doc->pages[_page].text = FUDE_DOC_TEXT_READ;
    }
    fude_file_free(_data);
}

// The pages one under the other, each as the PDF has it, turned as the canvas says.
RDE_INTERNAL void fude_doc_layout(fude_doc* _doc) {
    f32 _top = 0.0f;
    for(u32 _i = 0; _i < _doc->page_count; _i++) {
        fude_doc_page* _p = &_doc->pages[_i];
        _p->points        = fude_pdf_page_size(_doc->pdf, _i);
        _p->size          = (rde_vec_2F){ FUDE_DOC_PAGE_W, FUDE_DOC_PAGE_W * _p->points.y / fmaxf(1.0f, _p->points.x) };
        _p->top           = _top;
        _top             -= _p->size.y + FUDE_DOC_PAGE_GAP;
    }
}

RDE_INTERNAL void fude_doc_open(fude_doc* _doc, const c8* _path) {
    snprintf(_doc->path, sizeof(_doc->path), "%s", _path);
    _doc->pdf    = fude_pdf_open(_path);
    _doc->failed = _doc->pdf == NULL;
    if(_doc->pdf == NULL) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "fude: the document %s could not be opened", _path);
        return;
    }
    _doc->page_count = fude_pdf_page_count(_doc->pdf);
    _doc->pages      = (fude_doc_page*)calloc(_doc->page_count, sizeof(fude_doc_page));
    _doc->turned     = (u8*)calloc(_doc->page_count, sizeof(u8));
    fude_doc_layout(_doc);
    _doc->moved_at = rde_engine_get_time_now();
    fude_doc_lines_load(_doc);
}

void fude_doc_init(fude_doc* _doc) {
    memset(_doc, 0, sizeof(*_doc));
    _doc->lines   = rde_arr_new(sizeof(fude_doc_line), rde_memory_allocator_get_default_std());
    _doc->reading = UINT32_MAX;
    _doc->wanted  = UINT32_MAX;
    if(fude_doc_mutex == NULL) {
        fude_doc_mutex = rde_mutex_create();
    }
}

void fude_doc_destroy(fude_doc* _doc) {
    fude_doc_close(_doc);
    free(_doc->matches);
    free(_doc->read_lines);
    _doc->matches    = NULL;
    _doc->read_lines = NULL;
    if(rde_arr_is_inited(&_doc->lines)) {
        rde_arr_free(&_doc->lines);
    }
}

b8 fude_doc_path(const fude_app* _app, u32 _canvas, u8 _document, c8* _out, usize _size) {
    _out[0] = 0;
    if(_document == 0u) {
        return false;
    }
    if(_document == FUDE_NOTE_DOCUMENT_OWN) {
        fude_notes_document_path(_canvas, _out, _size);
        return true;
    }
    const fude_extension* _ext = fude_app_ext(_app);
    for(u32 _i = 0; _i < _ext->library_count; _i++) {
        if(_ext->library[_i].id == _document) {
            snprintf(_out, _size, "%s", _ext->library[_i].file);
            return true;
        }
    }
    return false;
}

// --- each frame -------------------------------------------------------------------------------

// The canvas in view: [_min, _max] (canvas units).
RDE_INTERNAL void fude_doc_in_view(fude_view _v, rde_vec_2I _window, rde_vec_2F* _min, rde_vec_2F* _max) {
    const f32 _z = fmaxf(_v.zoom, 1e-4f);
    *_min = (rde_vec_2F){ (-(f32)_window.x * 0.5f - _v.offset.x) / _z, (-(f32)_window.y * 0.5f - _v.offset.y) / _z };
    *_max = (rde_vec_2F){ ((f32)_window.x * 0.5f - _v.offset.x) / _z, ((f32)_window.y * 0.5f - _v.offset.y) / _z };
}

RDE_INTERNAL b8 fude_doc_page_between(const fude_doc_page* _p, f32 _y0, f32 _y1) {
    return _p->top >= _y0 && _p->top - _p->size.y <= _y1;
}

// A finished piece onto the GPU (or dropped: it failed); a page to read handed
// to the app's reader.
RDE_INTERNAL void fude_doc_take(fude_doc* _doc, fude_app* _app) {
    fude_doc_job* _job = _doc->job;
    if(_job == NULL) {
        return;
    }
    rde_mutex_lock(fude_doc_mutex);
    const b8 _done = _job->done;
    rde_mutex_unlock(fude_doc_mutex);
    if(!_done) {
        return;
    }
    _doc->job = NULL;
    if(_job->kind == FUDE_DOC_JOB_READ) {
        const fude_extension* _ext = fude_app_ext(_app);
        if(_job->ok && _job->page < _doc->page_count && _ext->picture_read != NULL && _ext->picture_read(_job->pixels, _job->w, _job->h)) {
            _doc->pages[_job->page].text = FUDE_DOC_TEXT_READING;
            _doc->reading                = _job->page;
            _doc->read_w                 = _job->w;
        } else {
            _doc->reader_off = true;   // it cannot now (ML Kit off, not here): not asked again while this one is open
        }
    } else if(_job->ok && _job->page < _doc->page_count) {
        fude_doc_page* _p    = &_doc->pages[_job->page];
        fude_doc_tile* _tile = _job->kind == FUDE_DOC_JOB_SHARP ? &_p->sharp : &_p->whole;
        fude_doc_tile_free(_tile);
        _tile->texture = rde_memory_texture_create(_job->w, _job->h, 4u, NULL);
        memcpy(rde_memory_texture_get_pixels(_tile->texture), _job->pixels, (usize)_job->w * (usize)_job->h * 4u);
        rde_texture_parameters _params = RDE_DEFAULT_TEXTURE_PARAMETERS;
        _params.wrap_s                 = RDE_TEXTURE_PARAMETER_TYPE_WRAP_CLAMP_TO_EDGE;
        _params.wrap_t                 = RDE_TEXTURE_PARAMETER_TYPE_WRAP_CLAMP_TO_EDGE;
        _params.generate_mipmap        = _job->kind == FUDE_DOC_JOB_WHOLE;   // a whole page is seen small too
        rde_memory_texture_gpu_upload(_tile->texture, &_params);
        _tile->from = _job->from;
        _tile->size = _job->size;
        _tile->zoom = _job->zoom;
    }
    _job->close_pdf = false;
    fude_doc_job_free(_job);
}

RDE_INTERNAL void fude_doc_start(fude_doc* _doc, u32 _page, u8 _kind, rde_vec_2F _from, rde_vec_2F _size, u32 _w, u32 _h, f32 _zoom) {
    fude_doc_job* _job = (fude_doc_job*)calloc(1, sizeof(fude_doc_job));
    _job->pdf          = _doc->pdf;
    _job->page         = _page;
    _job->kind         = _kind;
    _job->from         = _from;
    _job->size         = _size;
    _job->w            = _w;
    _job->h            = _h;
    _job->zoom         = _zoom;
    _job->dark         = _doc->dark;
    _job->paper        = _doc->paper;
    _job->print        = _doc->print;
    _job->pixels       = (u8*)malloc((usize)_w * (usize)_h * 4u);
    if(_job->pixels == NULL) {
        free(_job);
        return;
    }
    _doc->job = _job;
    rde_thread_run_detached(fude_doc_work, _job, NULL);
}

// The next piece: the pages on screen whole (nearest the middle first); then,
// the view resting, their sharp pieces; then the pages a screen above and below.
RDE_INTERNAL void fude_doc_next(fude_doc* _doc, fude_view _v, rde_vec_2I _window, b8 _resting) {
    rde_vec_2F _min, _max;
    fude_doc_in_view(_v, _window, &_min, &_max);
    const f32 _mid  = (_min.y + _max.y) * 0.5f;
    const f32 _high = _max.y - _min.y;

    i32 _best = -1;
    f32 _far  = 1e30f;
    for(u32 _i = 0; _i < _doc->page_count; _i++) {
        const fude_doc_page* _p = &_doc->pages[_i];
        if(_p->whole.texture == NULL && fude_doc_page_between(_p, _min.y, _max.y)) {
            const f32 _d = fabsf(_p->top - _p->size.y * 0.5f - _mid);
            if(_d < _far) {
                _far  = _d;
                _best = (i32)_i;
            }
        }
    }
    if(_best >= 0) {
        const fude_doc_page* _p = &_doc->pages[_best];
        const u32            _h = (u32)fmaxf(1.0f, (f32)FUDE_DOC_WHOLE_PX * _p->points.y / fmaxf(1.0f, _p->points.x));
        fude_doc_start(_doc, (u32)_best, FUDE_DOC_JOB_WHOLE, (rde_vec_2F){ 0.0f, 0.0f }, _p->points, FUDE_DOC_WHOLE_PX, _h, 0.0f);
        return;
    }

    // Sharp: where the screen has more pixels for a page than its whole picture.
    if(_resting && _v.zoom * FUDE_DOC_PAGE_W * FUDE_DOC_SCALE > (f32)FUDE_DOC_WHOLE_PX * 1.15f) {
        for(u32 _i = 0; _i < _doc->page_count; _i++) {
            const fude_doc_page* _p = &_doc->pages[_i];
            if(!fude_doc_page_between(_p, _min.y, _max.y)) {
                continue;
            }
            // The page on screen (canvas units), and that in its points.
            const f32 _x0 = fmaxf(-_p->size.x * 0.5f, _min.x), _x1 = fminf(_p->size.x * 0.5f, _max.x);
            const f32 _y0 = fmaxf(_p->top - _p->size.y, _min.y), _y1 = fminf(_p->top, _max.y);
            if(_x1 - _x0 < 1.0f || _y1 - _y0 < 1.0f) {
                continue;
            }
            const f32        _k    = _p->points.x / _p->size.x;
            const rde_vec_2F _from = { (_x0 + _p->size.x * 0.5f) * _k, (_p->top - _y1) * _k };
            const rde_vec_2F _size = { (_x1 - _x0) * _k, (_y1 - _y0) * _k };
            const fude_doc_tile* _s = &_p->sharp;
            if(_s->texture != NULL && fabsf(_s->zoom - _v.zoom) <= _v.zoom * 0.02f && _s->from.x <= _from.x + 0.5f && _s->from.y <= _from.y + 0.5f &&
               _s->from.x + _s->size.x >= _from.x + _size.x - 0.5f && _s->from.y + _s->size.y >= _from.y + _size.y - 0.5f) {
                continue;   // it has this one already
            }
            f32 _w = (_x1 - _x0) * _v.zoom * FUDE_DOC_SCALE;
            f32 _h = (_y1 - _y0) * _v.zoom * FUDE_DOC_SCALE;
            const f32 _over = fmaxf(_w, _h) / (f32)FUDE_DOC_SHARP_MAX;
            if(_over > 1.0f) {
                _w /= _over;
                _h /= _over;
            }
            if(_w < 8.0f || _h < 8.0f) {
                continue;
            }
            fude_doc_start(_doc, _i, FUDE_DOC_JOB_SHARP, _from, _size, (u32)_w, (u32)_h, _v.zoom);
            return;
        }
    }

    // Ahead: a screen above and below.
    for(u32 _i = 0; _i < _doc->page_count; _i++) {
        const fude_doc_page* _p = &_doc->pages[_i];
        if(_p->whole.texture == NULL && fude_doc_page_between(_p, _min.y - _high, _max.y + _high)) {
            const u32 _h = (u32)fmaxf(1.0f, (f32)FUDE_DOC_WHOLE_PX * _p->points.y / fmaxf(1.0f, _p->points.x));
            fude_doc_start(_doc, _i, FUDE_DOC_JOB_WHOLE, (rde_vec_2F){ 0.0f, 0.0f }, _p->points, FUDE_DOC_WHOLE_PX, _h, 0.0f);
            return;
        }
    }
}

// What is far from the view let go: sharp pieces off screen; whole pages beyond
// the FUDE_DOC_KEEP nearest.
RDE_INTERNAL void fude_doc_let_go(fude_doc* _doc, fude_view _v, rde_vec_2I _window) {
    rde_vec_2F _min, _max;
    fude_doc_in_view(_v, _window, &_min, &_max);
    const f32 _mid  = (_min.y + _max.y) * 0.5f;
    u32       _kept = 0;
    for(u32 _i = 0; _i < _doc->page_count; _i++) {
        fude_doc_page* _p = &_doc->pages[_i];
        if(!fude_doc_page_between(_p, _min.y, _max.y)) {
            fude_doc_tile_free(&_p->sharp);
        }
        _kept += _p->whole.texture != NULL ? 1u : 0u;
    }
    while(_kept > FUDE_DOC_KEEP) {
        i32 _farthest = -1;
        f32 _far      = -1.0f;
        for(u32 _i = 0; _i < _doc->page_count; _i++) {
            const fude_doc_page* _p = &_doc->pages[_i];
            const f32            _d = fabsf(_p->top - _p->size.y * 0.5f - _mid);
            if(_p->whole.texture != NULL && _d > _far) {
                _far      = _d;
                _farthest = (i32)_i;
            }
        }
        if(_farthest < 0) {
            break;
        }
        fude_doc_tile_free(&_doc->pages[_farthest].whole);
        _kept--;
    }
}

// --- reading pages without text of their own ---------------------------------------------

// A few pages asked whether they have text of their own (PDFKit): those with
// none are to be read.
RDE_INTERNAL void fude_doc_probe(fude_doc* _doc) {
    for(u32 _k = 0; _k < 4u && _doc->probe < _doc->page_count; _k++, _doc->probe++) {
        fude_doc_page* _p = &_doc->pages[_doc->probe];
        if(_p->text == FUDE_DOC_TEXT_UNKNOWN) {
            _p->text = fude_pdf_page_has_text(_doc->pdf, _doc->probe) ? FUDE_DOC_TEXT_OWN : FUDE_DOC_TEXT_TO_READ;
        }
    }
}

// The next page to read: the one wanted (a lasso's), else the nearest the view.
RDE_INTERNAL void fude_doc_read_next(fude_doc* _doc, fude_view _v) {
    i32       _best = -1;
    f32       _far  = 1e30f;
    const f32 _mid  = -_v.offset.y / fmaxf(_v.zoom, 1e-4f);
    if(_doc->wanted < _doc->page_count && _doc->pages[_doc->wanted].text == FUDE_DOC_TEXT_TO_READ) {
        _best = (i32)_doc->wanted;
    }
    for(u32 _i = 0; _best < 0 && _i < _doc->page_count; _i++) {
        const fude_doc_page* _p = &_doc->pages[_i];
        const f32            _d = fabsf(_p->top - _p->size.y * 0.5f - _mid);
        if(_p->text == FUDE_DOC_TEXT_TO_READ && _d < _far) {
            _far  = _d;
            _best = (i32)_i;
        }
    }
    for(u32 _i = 0; _best >= 0 && _i < 1u; _i++) {
        const fude_doc_page* _p = &_doc->pages[_best];
        const u32            _h = (u32)fminf(2400.0f, fmaxf(1.0f, (f32)FUDE_DOC_READ_PX * _p->points.y / fmaxf(1.0f, _p->points.x)));
        fude_doc_start(_doc, (u32)_best, FUDE_DOC_JOB_READ, (rde_vec_2F){ 0.0f, 0.0f }, _p->points, FUDE_DOC_READ_PX, _h, 0.0f);
    }
}

// The reader's answer for the page being read: its lines kept (in the page's
// points), the page read; kept on disk at once.
RDE_INTERNAL void fude_doc_read_take(fude_doc* _doc, fude_app* _app) {
    const fude_extension* _ext = fude_app_ext(_app);
    if(_ext->picture_lines == NULL) {
        return;
    }
    if(_doc->read_lines == NULL) {
        _doc->read_lines = (fude_doc_line*)calloc(FUDE_DOC_READ_MAX, sizeof(fude_doc_line));
    }
    u32 _count = 0;
    if(_doc->reader_drain) {
        if(_ext->picture_lines(_doc->read_lines, FUDE_DOC_READ_MAX, &_count)) {
            _doc->reader_drain = false;   // a document let go: dropped
        }
        return;
    }
    if(_doc->reading == UINT32_MAX || !_ext->picture_lines(_doc->read_lines, FUDE_DOC_READ_MAX, &_count)) {
        return;
    }
    const u32            _page = _doc->reading;
    const fude_doc_page* _p    = &_doc->pages[_page];
    const f32            _k    = _p->points.x / (f32)(_doc->read_w > 0u ? _doc->read_w : 1u);   // pixels to points
    for(u32 _i = 0; _i < _count && _i < FUDE_DOC_READ_MAX; _i++) {
        fude_doc_line _line = _doc->read_lines[_i];
        if(_line.text[0] == 0) {
            continue;
        }
        _line.page   = _page;
        _line.from.x *= _k;
        _line.from.y *= _k;
        _line.size.x *= _k;
        _line.size.y *= _k;
        rde_arr_add(&_doc->lines, &_line);
    }
    _doc->pages[_page].text = FUDE_DOC_TEXT_READ;
    _doc->reading           = UINT32_MAX;
    _doc->wanted            = _doc->wanted == _page ? UINT32_MAX : _doc->wanted;
    fude_doc_lines_save(_doc);
}

// Are pages still to be read (by a reader that can)?
RDE_INTERNAL b8 fude_doc_reading_left(const fude_doc* _doc, const fude_app* _app) {
    if(_doc->reader_off || fude_app_ext(_app)->picture_read == NULL) {
        return false;
    }
    for(u32 _i = 0; _i < _doc->page_count; _i++) {
        const u8 _t = _doc->pages[_i].text;
        if(_t == FUDE_DOC_TEXT_TO_READ || _t == FUDE_DOC_TEXT_READING || _t == FUDE_DOC_TEXT_UNKNOWN) {
            return true;
        }
    }
    return false;
}

// _hay's code points, as a lower-case ASCII search sees them: _needle's next
// place from byte _from (-1: none).
RDE_INTERNAL i32 fude_doc_find_in(const c8* _hay, const c8* _needle, usize _from) {
    const usize _n = strlen(_needle);
    if(_n == 0u) {
        return -1;
    }
    for(usize _i = _from; _hay[_i] != 0; _i++) {
        usize _k = 0;
        while(_k < _n && _hay[_i + _k] != 0) {
            c8 _a = _hay[_i + _k], _b = _needle[_k];
            _a = (_a >= 'A' && _a <= 'Z') ? (c8)(_a - 'A' + 'a') : _a;
            _b = (_b >= 'A' && _b <= 'Z') ? (c8)(_b - 'A' + 'a') : _b;
            if(_a != _b) {
                break;
            }
            _k++;
        }
        if(_k == _n) {
            return (i32)_i;
        }
    }
    return -1;
}

// Code points in _s's first _bytes bytes.
RDE_INTERNAL u32 fude_doc_chars(const c8* _s, usize _bytes) {
    u32 _n = 0;
    for(usize _i = 0; _i < _bytes && _s[_i] != 0; _i++) {
        _n += ((u8)_s[_i] & 0xC0u) != 0x80u ? 1u : 0u;
    }
    return _n;
}

// The search's matches in the read lines, after the PDF's own: each as the part
// of its line its characters take (the line's width shared out evenly).
RDE_INTERNAL void fude_doc_search_lines(fude_doc* _doc) {
    const fude_doc_line* _lines = (const fude_doc_line*)_doc->lines.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_doc->lines) && _doc->match_count < FUDE_PDF_MATCHES; _i++) {
        const fude_doc_line* _l     = &_lines[_i];
        const u32            _total = fude_doc_chars(_l->text, strlen(_l->text));
        for(i32 _at = fude_doc_find_in(_l->text, _doc->query, 0u); _at >= 0 && _doc->match_count < FUDE_PDF_MATCHES;
            _at = fude_doc_find_in(_l->text, _doc->query, (usize)_at + 1u)) {
            const u32 _before = fude_doc_chars(_l->text, (usize)_at);
            const u32 _len    = fude_doc_chars(&_l->text[_at], strlen(_doc->query));
            const f32 _each   = _l->size.x / (f32)(_total > 0u ? _total : 1u);
            fude_pdf_match* _m = &_doc->matches[_doc->match_count++];
            _m->page = _l->page;
            _m->from = (rde_vec_2F){ _l->from.x + _each * (f32)_before, _l->from.y };
            _m->size = (rde_vec_2F){ _each * (f32)_len, _l->size.y };
        }
    }
}

void fude_doc_update(fude_doc* _doc, fude_app* _app) {
    _doc->frame++;
    if(_doc->reader_drain) {
        fude_doc_read_take(_doc, _app);   // a reading of a document let go: dropped when it comes
    }
    // The open canvas's document: another one, this one opened.
    const fude_note* _note = fude_notes_find(_app->notes, _app->notes->open);
    c8               _want[RDE_MAX_PATH];
    if(_note == NULL || _note->kind != FUDE_NOTE_CANVAS || !fude_doc_path(_app, _note->id, _note->document, _want, sizeof(_want))) {
        _want[0] = 0;
    }
    if(strcmp(_want, _doc->path) != 0) {
        fude_doc_close(_doc);
        if(_want[0] != 0) {
            fude_doc_open(_doc, _want);
        }
    }
    if(_doc->pdf == NULL) {
        fude_canvas_set_reader(_app->canvas, false, 0.0f, 0.0f);
        return;
    }
    // Pages without text of their own read, one at a time (when the app reads pictures).
    _doc->reader_off = _doc->reader_off || fude_app_ext(_app)->picture_read == NULL;   // an app that reads no pictures
    if(!_doc->reader_off) {
        fude_doc_probe(_doc);
        fude_doc_read_take(_doc, _app);
    }
    // A search's matches, as they come — the PDF's own, then the read pages' —
    // the first one shown.
    if(_doc->searching) {
        b8        _done  = false;
        const u32 _had   = _doc->match_count;
        _doc->match_count = fude_pdf_find_matches(_doc->pdf, _doc->matches, FUDE_PDF_MATCHES, &_done);
        fude_doc_search_lines(_doc);
        _doc->search_done = _done && !fude_doc_reading_left(_doc, _app);
        if(_had == 0u && _doc->match_count > 0u) {
            _doc->match_at = 0u;
            fude_doc_search_step(_doc, _app->canvas, 0);
        }
    }
    // The pages turned as the canvas says (its page: canvas.h): laid out again,
    // drawn again.
    b8 _turns_changed = false;
    for(u32 _i = 0; _i < _doc->page_count; _i++) {
        const u8 _q = fude_page_turned(&_app->canvas->page, _i);
        if(_q != _doc->turned[_i]) {
            _doc->turned[_i] = _q;
            fude_pdf_set_turn(_doc->pdf, _i, _q);
            _turns_changed = true;
        }
    }
    if(_turns_changed) {
        fude_doc_drop_job(_doc, false);   // nothing of a page being drawn as it was (the worker keeps the PDF)
        fude_doc_drop_tiles(_doc);
        fude_doc_layout(_doc);
        fude_doc_search_stop(_doc);
    }
    // The view a reader's (canvas.h): kept on the pages.
    const fude_doc_page* _last = &_doc->pages[_doc->page_count - 1u];
    fude_canvas_set_reader(_app->canvas, true, FUDE_DOC_PAGE_W, _last->top - _last->size.y);
    // A dark theme: the pages in its colours (drawn again when it changes).
    const fude_theme* _t     = fude_theme_active();
    const b8          _dark  = 0.299f * (f32)_t->page.r + 0.587f * (f32)_t->page.g + 0.114f * (f32)_t->page.b < 128.0f;
    const rde_color   _paper = _dark ? fude_kit_shade(_t->page, 10) : (rde_color){ 255, 255, 255, 255 };
    if(_dark != _doc->dark || memcmp(&_paper, &_doc->paper, sizeof(_paper)) != 0 || memcmp(&_t->text, &_doc->print, sizeof(_t->text)) != 0) {
        fude_doc_drop_job(_doc, false);
        fude_doc_drop_tiles(_doc);
        _doc->dark  = _dark;
        _doc->paper = _paper;
        _doc->print = _t->text;
    }
    // The view resting, or moving.
    const fude_view _v   = _app->canvas->view;
    const f64       _now = rde_engine_get_time_now();
    if(_v.zoom != _doc->seen_view.zoom || _v.offset.x != _doc->seen_view.offset.x || _v.offset.y != _doc->seen_view.offset.y) {
        _doc->seen_view = _v;
        _doc->moved_at  = _now;
    }
    const rde_vec_2I _window = rde_window_get_size(_app->window);
    fude_doc_take(_doc, _app);
    if(_doc->job == NULL) {
        fude_doc_next(_doc, _v, _window, _now - _doc->moved_at >= FUDE_DOC_REST);
    }
    // Nothing to draw: a page to read (one at a time).
    if(_doc->job == NULL && _doc->reading == UINT32_MAX && !_doc->reader_drain && !_doc->reader_off && fude_app_ext(_app)->picture_read != NULL) {
        fude_doc_read_next(_doc, _v);
    }
    fude_doc_let_go(_doc, _v, _window);
}

// --- drawing ----------------------------------------------------------------------------------

// A tile over its part of page _p, on screen.
RDE_INTERNAL void fude_doc_draw_tile(const fude_doc_page* _p, const fude_doc_tile* _tile, fude_view _v) {
    if(_tile->texture == NULL) {
        return;
    }
    const f32        _k      = _p->size.x / fmaxf(1.0f, _p->points.x);   // canvas units to a point
    const rde_vec_2F _c      = { -_p->size.x * 0.5f + (_tile->from.x + _tile->size.x * 0.5f) * _k, _p->top - (_tile->from.y + _tile->size.y * 0.5f) * _k };
    const rde_vec_2F _screen = { _tile->size.x * _k * _v.zoom, _tile->size.y * _k * _v.zoom };
    const rde_texture* _tex  = rde_memory_texture_get_texture(_tile->texture);
    const rde_vec_2UI  _px   = rde_texture_get_size(_tex);
    rde_rendering_2d_draw_texture_2(_tex, (rde_vec_3F){ _c.x * _v.zoom + _v.offset.x, _c.y * _v.zoom + _v.offset.y, 0.0f },
                                    (rde_vec_2F){ _screen.x / (f32)_px.x, _screen.y / (f32)_px.y }, 0.0f, (rde_color){ 255, 255, 255, 255 });
}

void fude_doc_render(fude_doc* _doc, const fude_canvas* _canvas, rde_window* _window) {
    if(_doc->pdf == NULL) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    const fude_view   _v = _canvas->view;
    rde_vec_2F        _min, _max;
    fude_doc_in_view(_v, rde_window_get_size(_window), &_min, &_max);
    for(u32 _i = 0; _i < _doc->page_count; _i++) {
        const fude_doc_page* _p = &_doc->pages[_i];
        if(!fude_doc_page_between(_p, _min.y, _max.y)) {
            continue;
        }
        // The sheet (blank until it is drawn), its edge, then the pictures.
        const rde_vec_2F _c    = { _v.offset.x, (_p->top - _p->size.y * 0.5f) * _v.zoom + _v.offset.y };
        const rde_vec_2F _size = { _p->size.x * _v.zoom, _p->size.y * _v.zoom };
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ _c.x, _c.y - 1.5f }, (rde_vec_2F){ _size.x + 3.0f, _size.y + 3.0f }, _t->outline);
        rde_rendering_2d_draw_rectangle(_c, _size, _doc->paper);
        fude_doc_draw_tile(_p, &_p->whole, _v);
        fude_doc_draw_tile(_p, &_p->sharp, _v);
    }
    // A search's matches, marked; the one shown more strongly, outlined.
    for(u32 _m = 0; _doc->searching && _m < _doc->match_count; _m++) {
        const fude_pdf_match* _match = &_doc->matches[_m];
        if(_match->page >= _doc->page_count || !fude_doc_page_between(&_doc->pages[_match->page], _min.y, _max.y)) {
            continue;
        }
        const fude_doc_page* _p = &_doc->pages[_match->page];
        const f32            _k = _p->size.x / fmaxf(1.0f, _p->points.x) * _v.zoom;   // points to screen units
        const rde_vec_2F     _c = { (-_p->size.x * 0.5f) * _v.zoom + _v.offset.x + (_match->from.x + _match->size.x * 0.5f) * _k,
                                    _p->top * _v.zoom + _v.offset.y - (_match->from.y + _match->size.y * 0.5f) * _k };
        const rde_vec_2F     _s = { _match->size.x * _k + 4.0f, _match->size.y * _k + 4.0f };
        rde_color            _mark = _t->accent;
        _mark.a = _m == _doc->match_at ? 120u : 60u;
        rde_rendering_2d_draw_rectangle(_c, _s, _mark);
        if(_m == _doc->match_at) {
            rde_rendering_2d_draw_rectangle_border(_c, _s, 2.0f, _t->accent, NULL);
        }
    }
}

// --- reading its text, searching it, going to a page --------------------------------------------

u32 fude_doc_page_count(const fude_doc* _doc) {
    return _doc->pdf != NULL ? _doc->page_count : 0u;
}

// A read page's text inside the box (points from its top-left: _a-_b): each
// line it crosses (half its height in it, at least), the characters of it the
// box takes (the line's width shared out evenly), a line each.
RDE_INTERNAL usize fude_doc_read_text_in(const fude_doc* _doc, u32 _page, rde_vec_2F _a, rde_vec_2F _b, c8* _out, usize _size) {
    const fude_doc_line* _lines = (const fude_doc_line*)_doc->lines.memory;
    usize                _n     = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_doc->lines) && _n + 2u < _size; _i++) {
        const fude_doc_line* _l = &_lines[_i];
        if(_l->page != _page) {
            continue;
        }
        const f32 _over = fminf(_b.y, _l->from.y + _l->size.y) - fmaxf(_a.y, _l->from.y);
        if(_over < _l->size.y * 0.5f) {
            continue;
        }
        const u32 _total = fude_doc_chars(_l->text, strlen(_l->text));
        const f32 _each  = _l->size.x / (f32)(_total > 0u ? _total : 1u);
        usize     _start = _n;
        u32       _c     = 0;
        for(const c8* _p = _l->text; *_p != 0 && _n + 5u < _size; _c++) {
            const c8* _from = _p;
            fude_utf8_next(&_p);
            const f32 _mid = _l->from.x + _each * ((f32)_c + 0.5f);
            if(_mid >= _a.x && _mid <= _b.x) {
                memcpy(&_out[_n], _from, (usize)(_p - _from));
                _n += (usize)(_p - _from);
            }
        }
        if(_n > _start) {
            _out[_n++] = '\n';
        }
        _out[_n] = 0;
    }
    return _n;
}

usize fude_doc_text_in(fude_doc* _doc, rde_vec_2F _min, rde_vec_2F _max, c8* _out, usize _size, b8* _pending) {
    usize _n = 0;
    if(_size > 0) {
        _out[0] = 0;
    }
    if(_pending != NULL) {
        *_pending = false;
    }
    for(u32 _i = 0; _doc->pdf != NULL && _i < _doc->page_count && _n + 2u < _size; _i++) {
        fude_doc_page* _p = &_doc->pages[_i];
        if(!fude_doc_page_between(_p, _min.y, _max.y)) {
            continue;
        }
        // The box on this page, in its points from its top-left.
        const f32 _x0 = fmaxf(_min.x, -_p->size.x * 0.5f), _x1 = fminf(_max.x, _p->size.x * 0.5f);
        const f32 _y0 = fmaxf(_min.y, _p->top - _p->size.y), _y1 = fminf(_max.y, _p->top);
        if(_x1 <= _x0 || _y1 <= _y0) {
            continue;
        }
        const f32        _k    = _p->points.x / fmaxf(1.0f, _p->size.x);
        const rde_vec_2F _from = { (_x0 + _p->size.x * 0.5f) * _k, (_p->top - _y1) * _k };
        const rde_vec_2F _box  = { (_x1 - _x0) * _k, (_y1 - _y0) * _k };
        if(_p->text == FUDE_DOC_TEXT_UNKNOWN) {
            _p->text = fude_pdf_page_has_text(_doc->pdf, _i) ? FUDE_DOC_TEXT_OWN : FUDE_DOC_TEXT_TO_READ;
        }
        if(_n > 0u && _out[_n - 1u] != '\n') {
            _out[_n++] = '\n';
            _out[_n]   = 0;
        }
        if(_p->text == FUDE_DOC_TEXT_OWN) {
            _n += fude_pdf_text_in(_doc->pdf, _i, _from, _box, &_out[_n], _size - _n);
        } else if(_p->text == FUDE_DOC_TEXT_READ) {
            _n += fude_doc_read_text_in(_doc, _i, _from, (rde_vec_2F){ _from.x + _box.x, _from.y + _box.y }, &_out[_n], _size - _n);
        } else if(!_doc->reader_off) {
            // Still to be read: next (then ask again).
            if(_pending != NULL) {
                *_pending = true;
            }
            if(_p->text == FUDE_DOC_TEXT_TO_READ) {
                _doc->wanted = _i;
            }
        }
    }
    while(_n > 0u && (_out[_n - 1u] == '\n' || _out[_n - 1u] == ' ')) {
        _out[--_n] = 0;
    }
    return _n;
}

// The canvas box _min-_max in the middle of the screen (across only when zoomed
// in past 100%: the reader's frame keeps it otherwise), at the zoom there is.
RDE_INTERNAL void fude_doc_show_box(fude_canvas* _canvas, rde_vec_2F _min, rde_vec_2F _max) {
    fude_view* _v = &_canvas->view;
    _v->offset.y  = -(_min.y + _max.y) * 0.5f * _v->zoom;
    _v->offset.x  = -(_min.x + _max.x) * 0.5f * _v->zoom;
    _canvas->coasting = false;
}

void fude_doc_go_to_page(fude_doc* _doc, fude_canvas* _canvas, u32 _page) {
    if(_doc->pdf == NULL || _doc->page_count == 0u) {
        return;
    }
    const fude_doc_page* _p = &_doc->pages[_page < _doc->page_count ? _page : _doc->page_count - 1u];
    const fude_canvas_reader* _r = &_canvas->reader;
    _canvas->coasting      = false;
    _canvas->view.offset.y = (f32)_r->window.y * 0.5f - (f32)_r->safe.y - FUDE_CANVAS_READER_TOP - _p->top * _canvas->view.zoom;
    _doc->moved_at         = rde_engine_get_time_now();   // the pill says where
}

void fude_doc_search(fude_doc* _doc, const c8* _query) {
    fude_doc_search_stop(_doc);
    if(_doc->pdf == NULL || _query == NULL || _query[0] == 0) {
        return;
    }
    if(_doc->matches == NULL) {
        _doc->matches = (fude_pdf_match*)calloc(FUDE_PDF_MATCHES, sizeof(fude_pdf_match));
    }
    _doc->searching   = true;
    _doc->search_done = false;
    _doc->match_count = 0u;
    _doc->match_at    = 0u;
    snprintf(_doc->query, sizeof(_doc->query), "%s", _query);
    fude_pdf_find_start(_doc->pdf, _query);
}

void fude_doc_search_step(fude_doc* _doc, fude_canvas* _canvas, i32 _step) {
    if(!_doc->searching || _doc->match_count == 0u) {
        return;
    }
    _doc->match_at = (u32)(((i32)_doc->match_at + _step + (i32)_doc->match_count) % (i32)_doc->match_count);
    const fude_pdf_match* _m = &_doc->matches[_doc->match_at];
    if(_m->page >= _doc->page_count) {
        return;
    }
    const fude_doc_page* _p = &_doc->pages[_m->page];
    const f32            _k = _p->size.x / fmaxf(1.0f, _p->points.x);
    fude_doc_show_box(_canvas, (rde_vec_2F){ -_p->size.x * 0.5f + _m->from.x * _k, _p->top - (_m->from.y + _m->size.y) * _k },
                      (rde_vec_2F){ -_p->size.x * 0.5f + (_m->from.x + _m->size.x) * _k, _p->top - _m->from.y * _k });
    _doc->moved_at = rde_engine_get_time_now();
}

void fude_doc_search_stop(fude_doc* _doc) {
    if(_doc->searching && _doc->pdf != NULL) {
        fude_pdf_find_stop(_doc->pdf);
    }
    _doc->searching   = false;
    _doc->search_done = false;
    _doc->match_count = 0u;
    _doc->match_at    = 0u;
}

u32 fude_doc_search_count(const fude_doc* _doc, u32* _at, b8* _done) {
    *_at   = _doc->match_at;
    *_done = !_doc->searching || _doc->search_done;
    return _doc->searching ? _doc->match_count : 0u;
}

u32 fude_doc_page_at(const fude_doc* _doc, fude_view _view);

// The page in the middle of the screen (from 0).
RDE_INTERNAL u32 fude_doc_page_in_middle(const fude_doc* _doc, fude_view _v) {
    const f32 _mid = -_v.offset.y / fmaxf(_v.zoom, 1e-4f);
    for(u32 _i = 0; _i < _doc->page_count; _i++) {
        if(_mid >= _doc->pages[_i].top - _doc->pages[_i].size.y - FUDE_DOC_PAGE_GAP * 0.5f) {
            return _i;
        }
    }
    return _doc->page_count > 0 ? _doc->page_count - 1u : 0u;
}

void fude_doc_render_pill(fude_doc* _doc, const fude_app* _app, rde_window* _window) {
    const f64 _age = rde_engine_get_time_now() - _doc->moved_at;
    if(_doc->pdf == NULL || _doc->page_count < 2u || _app->font == NULL || _age >= FUDE_DOC_PILL_TIME + FUDE_DOC_PILL_FADE) {
        return;
    }
    const f32 _fade = _age <= FUDE_DOC_PILL_TIME ? 1.0f : 1.0f - (f32)((_age - FUDE_DOC_PILL_TIME) / FUDE_DOC_PILL_FADE);
    c8        _text[32];
    snprintf(_text, sizeof(_text), "%u / %u", fude_doc_page_in_middle(_doc, _app->canvas->view) + 1u, _doc->page_count);
    const f32         _px     = 18.0f;
    const f32         _w      = fude_draw_text_width(_app->font, _app->font_px, _text, _px);
    const fude_theme* _t      = fude_theme_active();
    const rde_vec_2I  _size   = rde_window_get_size(_window);
    const rde_vec_4I  _safe   = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const rde_vec_2F  _pill   = { fmaxf(72.0f, _w + 32.0f), 40.0f };
    const rde_vec_2F  _center = { 0.0f, -(f32)_size.y * 0.5f + (f32)_safe.w + 16.0f + _pill.y * 0.5f };
    rde_color _back = _t->panel;
    _back.a         = (u8)((f32)_back.a * _fade);
    rde_rendering_2d_draw_rounded_rectangle(_center, _pill, 1.0f, 8u, _back, NULL);
    rde_color _ink = _t->button_text;
    _ink.a         = (u8)((f32)_ink.a * _fade);
    fude_draw_text(_app->font, _app->font_px, _text, _center.x - _w * 0.5f, _center.y - _px * 0.36f, _px, _ink);
}

// --- new canvases -------------------------------------------------------------------------------

fude_view fude_doc_first_view(rde_window* _window) {
    const rde_vec_2I _size = rde_window_get_size(_window);
    const rde_vec_4I _safe = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    fude_view        _v;
    _v.zoom   = fude_canvas_reader_fit(FUDE_DOC_PAGE_W, _size, _safe);   // 100%: the pages' width across the screen
    _v.offset = (rde_vec_2F){ (f32)(_safe.x - _safe.z) * 0.5f, (f32)_size.y * 0.5f - (f32)_safe.y - FUDE_CANVAS_READER_TOP };   // the first page's top under the menu button
    return _v;
}

u32 fude_doc_new_canvas(fude_app* _app, u8 _document, const c8* _own, const c8* _name, u32 _parent) {
    const u32 _id = fude_notes_add(_app->notes, FUDE_NOTE_CANVAS, _parent, _name);
    if(_id == 0u) {
        return 0u;
    }
    if(_document == FUDE_NOTE_DOCUMENT_OWN) {
        c8 _to[RDE_MAX_PATH];
        fude_notes_document_path(_id, _to, sizeof(_to));
        if(_own == NULL || !rde_file_copy(_own, _to) || !rde_file_exists(_to)) {
            fude_notes_remove(_app->notes, _id);
            return 0u;
        }
    }
    fude_notes_set_document(_app->notes, _id, _document);
    // Its page: nothing written yet, the first page's width on screen, no paper.
    fude_ink* _ink = (fude_ink*)calloc(1, sizeof(fude_ink));
    fude_ink_init(_ink);
    c8 _path[RDE_MAX_PATH];
    fude_notes_canvas_path(_id, _path, sizeof(_path));
    const fude_page _page = { .paper = FUDE_PAPER_NONE };
    fude_save_document(_path, _ink, fude_doc_first_view(_app->window), _page, NULL);
    fude_ink_destroy(_ink);
    free(_ink);
    fude_notes_open(_app->notes, _id);
    return _id;
}

u32 fude_doc_open_book(fude_app* _app, const fude_doc_book* _book, const c8* _folder_name) {
    // The canvas there is; else the folder the library's others are in.
    b8  _found  = false;
    u32 _folder = 0u;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_app->notes->notes); _i++) {
        const fude_note* _n = &((const fude_note*)_app->notes->notes.memory)[_i];
        if(_n->kind != FUDE_NOTE_CANVAS || _n->document < 2u) {
            continue;
        }
        if(_n->document == _book->id) {
            fude_notes_open(_app->notes, _n->id);
            return _n->id;
        }
        if(!_found) {
            _found  = true;
            _folder = _n->parent;
        }
    }
    // None: a folder at the top named so (the one made before, emptied since), else a new one.
    for(u32 _i = 0; !_found && _folder_name != NULL && _i < (u32)rde_arr_length(&_app->notes->notes); _i++) {
        const fude_note* _n = &((const fude_note*)_app->notes->notes.memory)[_i];
        if(_n->kind == FUDE_NOTE_FOLDER && _n->parent == 0u && strcmp(_n->name, _folder_name) == 0) {
            _found  = true;
            _folder = _n->id;
        }
    }
    if(!_found && _folder_name != NULL) {
        _folder = fude_notes_add(_app->notes, FUDE_NOTE_FOLDER, 0u, _folder_name);
    }
    return fude_doc_new_canvas(_app, _book->id, NULL, fude_text(_book->title), _folder);
}

u32 fude_doc_page_at(const fude_doc* _doc, fude_view _view) {
    return fude_doc_page_in_middle(_doc, _view);
}

// --- written out ------------------------------------------------------------------------------

b8 fude_doc_export(fude_doc* _doc, const fude_ink* _ink, const c8* _out) {
    if(_doc->pdf == NULL || _ink == NULL) {
        return false;
    }
    fude_pdf_writer* _w = fude_pdf_write_begin(_out);
    if(_w == NULL) {
        return false;
    }
    // Scratch: a stroke's points, in a page's points.
    u32         _cap    = 256u;
    rde_vec_2F* _points = (rde_vec_2F*)malloc(sizeof(rde_vec_2F) * _cap);
    f32*        _radii  = (f32*)malloc(sizeof(f32) * _cap);
    const rde_color _ink_colour = fude_theme_get(FUDE_THEME_PAPER)->ink;   // white paper: the light theme's ink
    for(u32 _i = 0; _i < _doc->page_count; _i++) {
        const fude_doc_page* _p = &_doc->pages[_i];
        const f32            _k = _p->points.x / fmaxf(1.0f, _p->size.x);   // canvas units to points
        fude_pdf_write_page(_w, _doc->pdf, _i);
        // The marker's strokes first, under the rest — as on the page.
        for(u32 _pass = 0; _pass < 2u; _pass++) {
            for(u32 _s = 0; _s < fude_ink_stroke_count(_ink); _s++) {
                const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _s);
                if(!_stroke->alive || _stroke->eraser || _stroke->point_count == 0u || _stroke->marker != (_pass == 0u) ||
                   _stroke->bounds_max.x < -_p->size.x * 0.5f || _stroke->bounds_min.x > _p->size.x * 0.5f ||
                   _stroke->bounds_max.y < _p->top - _p->size.y || _stroke->bounds_min.y > _p->top) {
                    continue;
                }
                if(_stroke->point_count > _cap) {
                    _cap    = _stroke->point_count;
                    _points = (rde_vec_2F*)realloc(_points, sizeof(rde_vec_2F) * _cap);
                    _radii  = (f32*)realloc(_radii, sizeof(f32) * _cap);
                }
                const fude_ink_point* _pts = fude_ink_stroke_points(_ink, _stroke);
                u32                   _n   = 0;
                rde_vec_2F            _sum = { 0.0f, 0.0f };
                u32                   _summed = 0;
                for(u32 _q = 0; _q < _stroke->point_count; _q++) {
                    const rde_vec_2F _at = { (_pts[_q].position.x + _p->size.x * 0.5f) * _k, (_p->top - _pts[_q].position.y) * _k };
                    const f32        _r  = _pts[_q].radius * _k;
                    // The marker: its points a good part of its width apart, each the
                    // middle of the samples between (as fude_ink_render draws it).
                    if(_stroke->marker && _n > 0u) {
                        const b8  _end = _q + 1u == _stroke->point_count;
                        const f32 _dx  = _at.x - _points[_n - 1u].x, _dy = _at.y - _points[_n - 1u].y;
                        _sum.x += _at.x;
                        _sum.y += _at.y;
                        _summed++;
                        if(_dx * _dx + _dy * _dy < (_r * FUDE_INK_MARKER_STEP) * (_r * FUDE_INK_MARKER_STEP)) {
                            if(_end && _n > 1u) {
                                _points[_n - 1u] = _at;
                            }
                            continue;
                        }
                        _points[_n] = _end ? _at : (rde_vec_2F){ _sum.x / (f32)_summed, _sum.y / (f32)_summed };
                        _radii[_n++] = _r;
                        _sum    = (rde_vec_2F){ 0.0f, 0.0f };
                        _summed = 0;
                        continue;
                    }
                    _points[_n] = _at;
                    _radii[_n++] = _r;
                }
                const rde_color _colour = fude_theme_is_ink(_stroke->color) ? _ink_colour : _stroke->color;
                fude_pdf_write_stroke(_w, _points, _radii, _n, _colour, _stroke->marker);
            }
        }
        // What was read in its picture, unseen.
        const fude_doc_line* _lines = (const fude_doc_line*)_doc->lines.memory;
        for(u32 _l = 0; _p->text == FUDE_DOC_TEXT_READ && _l < (u32)rde_arr_length(&_doc->lines); _l++) {
            if(_lines[_l].page == _i) {
                fude_pdf_write_hidden_text(_w, _lines[_l].text, _lines[_l].from, _lines[_l].size);
            }
        }
        fude_pdf_write_page_end(_w);
    }
    free(_points);
    free(_radii);
    return fude_pdf_write_end(_w);
}

// --- a page turned -------------------------------------------------------------------------------

typedef struct {
    const fude_doc* doc;
    u32             page;      // the page turned
    f32             top;       // its top (canvas y), its height before and after
    f32             h_old, h_new;
    u32*            owner;     // each stroke's page (by where its middle is)
} fude_doc_turning;

// A point of ink: on the page turned, turned with it (a quarter clockwise about
// its top-left, then as wide as the pages again); on a page after it, moved by
// how much shorter or taller it became.
RDE_INTERNAL void fude_doc_turn_point(any _user, u32 _stroke, fude_ink_point* _point) {
    const fude_doc_turning* _t = (const fude_doc_turning*)_user;
    const u32               _p = _t->owner[_stroke];
    if(_p == _t->page) {
        const f32 _f  = FUDE_DOC_PAGE_W / fmaxf(1.0f, _t->h_old);
        const f32 _px = _point->position.x + FUDE_DOC_PAGE_W * 0.5f;   // from its top-left, across...
        const f32 _py = _t->top - _point->position.y;                    // ...and down
        _point->position = (rde_vec_2F){ _f * (_t->h_old - _py) - FUDE_DOC_PAGE_W * 0.5f, _t->top - _f * _px };
        _point->radius  *= _f;
    } else if(_p != UINT32_MAX && _p > _t->page) {
        _point->position.y += _t->h_old - _t->h_new;
    }
}

b8 fude_doc_turn_page(fude_doc* _doc, fude_app* _app, u32 _page) {
    if(_doc->pdf == NULL || _page >= _doc->page_count) {
        return false;
    }
    const fude_doc_page* _p = &_doc->pages[_page];
    if(!fude_page_turn_more(&_app->canvas->page, _page)) {
        return false;
    }
    // Which page each stroke is on: the band round each page down to the next one's.
    fude_ink*        _ink   = _app->ink;
    const u32        _count = fude_ink_stroke_count(_ink);
    fude_doc_turning _t     = { _doc, _page, _p->top, _p->size.y, FUDE_DOC_PAGE_W * _p->points.x / fmaxf(1.0f, _p->points.y), NULL };
    _t.owner = (u32*)malloc(sizeof(u32) * (_count > 0u ? _count : 1u));
    for(u32 _s = 0; _s < _count; _s++) {
        const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _s);
        const f32              _mid    = (_stroke->bounds_min.y + _stroke->bounds_max.y) * 0.5f;
        _t.owner[_s] = _doc->page_count - 1u;
        for(u32 _i = 0; _i < _doc->page_count; _i++) {
            if(_mid >= _doc->pages[_i].top - _doc->pages[_i].size.y - FUDE_DOC_PAGE_GAP * 0.5f) {
                _t.owner[_s] = _i;
                break;
            }
        }
    }
    fude_ink_remap(_ink, fude_doc_turn_point, &_t);
    free(_t.owner);
    // Its text read from its picture: read again, upright now.
    fude_doc_line* _lines = (fude_doc_line*)_doc->lines.memory;
    u32            _kept  = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_doc->lines); _i++) {
        if(_lines[_i].page != _page) {
            _lines[_kept++] = _lines[_i];
        }
    }
    _doc->lines.count = _kept;
    if(_doc->reading == _page) {
        _doc->reader_drain = true;   // read the way it was: dropped when it comes
        _doc->reading      = UINT32_MAX;
    }
    if(_doc->pages[_page].text == FUDE_DOC_TEXT_READ || _doc->pages[_page].text == FUDE_DOC_TEXT_READING) {
        _doc->pages[_page].text = FUDE_DOC_TEXT_UNKNOWN;
        _doc->probe             = _page < _doc->probe ? _page : _doc->probe;
        fude_doc_lines_save(_doc);
    }
    fude_doc_search_stop(_doc);
    return true;   // laid out again on the next update (it follows the canvas's page)
}
