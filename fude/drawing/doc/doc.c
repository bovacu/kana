// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/doc/doc.h"
#include "drawing/ink/pagefile.h"
#include "drawing/base/kfile.h"
#include "drawing/base/save.h"
#include "drawing/base/utf8.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "drawing/ink/notes.h"
#include "drawing/base/draw.h"

// ===========================================================================
// See doc.h.
//
// The worker (worker.h): one piece (a whole page, a sharp piece, or a page's picture for the reader) is drawn at a
// time, on a thread started for it (RDE's detached threads). The document starts a piece when its worker is not busy
// and takes it back each frame once drawn. When the document is let go while its piece is still being drawn, the piece
// is left to the worker to finish and free, and to close the PDF with it.
//
// Order of work (fude_doc_next): the pages on screen are drawn whole first, nearest the middle first; once the view
// has rested for FUDE_DOC_REST seconds, the part of each one on screen is drawn again at the screen's pixels; then the
// pages within a screen above and below. Which whole pages are kept, and drawn, is the worker's to choose
// (worker.h: fude_doc_wholes, from what fude_doc_wholes_plan sees of each page): the rest are let go there, and the
// sharp pieces off screen by fude_doc_let_go.
//
// Reading: a few pages each frame are asked whether they have text of their own. Those with none are drawn
// FUDE_DOC_READ_PX wide and handed to the app's picture reader, one at a time, nearest the view first (or the one a
// lasso asked for). The lines it answers with are kept in memory and in the .lines file described below.
// ===========================================================================

// The page pill ("3 / 24"): it is shown fully while the view moves and for FUDE_DOC_PILL_TIME seconds after it stops,
// then fades out over FUDE_DOC_PILL_FADE seconds.
#define FUDE_DOC_PILL_TIME 1.2    // Seconds the pill stays fully shown after the view stops.
#define FUDE_DOC_PILL_FADE 0.35

// The read lines' file, <name>.lines beside the canvas's own PDF (a kfile.h file): kind 'LINS', then a 'PAGE' chunk
// for each page read. A chunk holds u32 page, u32 count, then for each line f32 x, y, w, h (its box in points from
// the page's top-left) and its text (a u16 length, then the bytes, with no 0 at the end). A page read with no lines
// still has its chunk, so it is not read again.
#define FUDE_DOC_LINES_VERSION 1u
#define FUDE_DOC_LINES_KIND    FUDE_TAG('L', 'I', 'N', 'S')
#define FUDE_DOC_LINES_PAGE    FUDE_TAG('P', 'A', 'G', 'E')

// The document's pages and search matches as typed arrays (an rde_arr keeps its memory untyped).
RDE_INTERNAL fude_doc_page*  fude_doc_pages(const fude_doc* _doc)   { return (fude_doc_page*)_doc->pages.memory; }
RDE_INTERNAL fude_pdf_match* fude_doc_matches(const fude_doc* _doc) { return (fude_pdf_match*)_doc->matches.memory; }

// --- opening and letting go -------------------------------------------------------------------

// Frees every page's textures, so they are drawn again: the theme or a page's turn changed, or the document closes.
RDE_INTERNAL void fude_doc_drop_tiles(fude_doc* _doc) {
    for(u32 _i = 0; _i < _doc->page_count; _i++) {
        fude_doc_tile_free(&fude_doc_pages(_doc)[_i].whole);
        fude_doc_tile_free(&fude_doc_pages(_doc)[_i].sharp);
    }
}

// Lets go of the piece being drawn, as the document closes (worker.h: fude_doc_worker_drop). A finished piece is freed;
// one still being drawn is left to the worker, which closes the PDF once it has finished drawing with it, so the PDF is
// never closed under it: the document then forgets the PDF.
RDE_INTERNAL void fude_doc_drop_job(fude_doc* _doc) {
    if(fude_doc_worker_drop(&_doc->worker)) {
        _doc->pdf = NULL;   // The worker closes it.
    }
}

// Closes the open document: stops a search, drops a reading in progress, lets go of the job, the textures and the
// PDF, and empties the pages and read lines. The arrays themselves are kept for the next document.
RDE_INTERNAL void fude_doc_close(fude_doc* _doc) {
    fude_doc_search_stop(_doc);
    if(_doc->reading != UINT32_MAX) {
        _doc->reader_drain = true;   // Nobody wants its answer now; it is dropped when it comes.
    }
    _doc->reading       = UINT32_MAX;
    _doc->wanted        = UINT32_MAX;
    _doc->probe         = 0u;
    _doc->reader_off    = false;
    _doc->lines_path[0] = 0;
    if(rde_arr_is_inited(&_doc->lines)) {
        rde_arr_clear(&_doc->lines);
    }
    fude_doc_drop_job(_doc);
    fude_doc_drop_tiles(_doc);
    if(_doc->pdf != NULL) {
        fude_pdf_close(_doc->pdf);
        _doc->pdf = NULL;
    }
    if(rde_arr_is_inited(&_doc->pages)) {
        rde_arr_clear(&_doc->pages);
        rde_arr_clear(&_doc->turned);
        rde_arr_clear(&_doc->seen);
    }
    _doc->page_count  = 0;
    _doc->path[0]     = 0;
    _doc->failed      = false;
    _doc->turns_taken = false;   // The next document takes the canvas's turns afresh.
}

// --- the read lines' file ---------------------------------------------------------------------

// Writes every page read so far, with its lines, to the .lines file (its format is at the top of this file). Does
// nothing for a book of the app's, whose lines are not kept.
RDE_INTERNAL void fude_doc_lines_save(fude_doc* _doc) {
    if(_doc->lines_path[0] == 0) {
        return;
    }
    const fude_doc_line* _lines = (const fude_doc_line*)_doc->lines.memory;
    const u32            _n     = (u32)rde_arr_length(&_doc->lines);
    fude_bytes           _b     = fude_bytes_new(64u + _n * 48u);
    fude_put_header(&_b, FUDE_DOC_LINES_VERSION, FUDE_DOC_LINES_KIND);
    for(u32 _p = 0; _p < _doc->page_count; _p++) {
        if(fude_doc_pages(_doc)[_p].text != FUDE_DOC_TEXT_READ) {
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
            const u32 _len = (u32)rde_strlen(_lines[_i].text);
            fude_put_u16(&_b, (u16)_len);
            fude_put_data(&_b, _lines[_i].text, _len);
        }
        fude_chunk_end(&_b, _chunk);
    }
    if(!fude_bytes_write_and_free(&_b, _doc->lines_path, NULL)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "document: the document's read text could not be saved (%s)", _doc->lines_path);
    }
}

// fude_save_load's reader for the .lines file (its format is at the top of this file): adds each page's lines to the
// document and marks the page read. Returns false only when the file is not a .lines file at all (then nothing was
// added, and the file is set aside and its backup tried); a damaged page further in is skipped.
RDE_INTERNAL b8 fude_doc_lines_read(const u8* _data, u32 _size, any _user) {
    fude_doc*   _doc = (fude_doc*)_user;
    fude_reader _r   = fude_reader_make(_data, _size);
    if(!fude_read_header(&_r, FUDE_DOC_LINES_VERSION, FUDE_DOC_LINES_KIND)) {
        return false;
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
            rde_memcpy(_line.text, &_c.data[_c.pos], _keep);
            _line.text[_keep] = 0;
            _c.pos += _len;
            rde_arr_add(&_doc->lines, &_line);
        }
        fude_doc_pages(_doc)[_page].text = FUDE_DOC_TEXT_READ;
    }
    return true;
}

// Decides where the open document's read lines are kept, and loads those read before. They are kept beside the PDF
// (<name>.lines) only when the PDF is the learner's own, in the save folder. They are read as every saved file is
// (save.h: the file, else its backup; a damaged one set aside), so a damaged file costs at most the last page read.
RDE_INTERNAL void fude_doc_lines_load(fude_doc* _doc) {
    _doc->lines_path[0] = 0;
    const c8*   _dir = fude_save_dir();
    const usize _n   = rde_strlen(_doc->path);
    if(rde_strncmp(_doc->path, _dir, rde_strlen(_dir)) != 0 || _n < 4u || rde_strcmp(&_doc->path[_n - 4u], ".pdf") != 0) {
        return;   // A book of the app's: its lines are not kept, since its books have text of their own.
    }
    rde_snprintf(_doc->lines_path, sizeof(_doc->lines_path), "%.*s.lines", (i32)(_n - 4u), _doc->path);
    fude_save_load(_doc->lines_path, fude_doc_lines_read, _doc);   // (MISSING: nothing read yet; CORRUPT: read again)
}

// Lays the pages out one under the other, from canvas y 0 down, FUDE_DOC_PAGE_GAP apart. Each is FUDE_DOC_PAGE_W wide
// and as tall as its shape in the PDF, turned as the canvas says, makes it.
RDE_INTERNAL void fude_doc_layout(fude_doc* _doc) {
    f32 _top = 0.0f;
    for(u32 _i = 0; _i < _doc->page_count; _i++) {
        fude_doc_page* _p = &fude_doc_pages(_doc)[_i];
        _p->points        = fude_pdf_page_size(_doc->pdf, _i);
        _p->size          = (rde_vec_2F){ FUDE_DOC_PAGE_W, FUDE_DOC_PAGE_W * _p->points.y / rde_math_max_f32(1.0f, _p->points.x) };
        _p->top           = _top;
        _top             -= _p->size.y + FUDE_DOC_PAGE_GAP;
    }
}

// Opens the PDF at _path, lays out its pages and loads the lines read in its pictures before. When it cannot be
// opened, its path is still kept and failed is set, so it is not tried again each frame.
RDE_INTERNAL void fude_doc_open(fude_doc* _doc, const c8* _path) {
    rde_snprintf(_doc->path, sizeof(_doc->path), "%s", _path);
    _doc->pdf = fude_pdf_open(_path);
    // A PDF with no pages is refused like one that does not open: everything below lays out, draws and scrolls
    // pages, and counts on there being at least one (the last page's bottom, the page in the middle of the view).
    if(_doc->pdf != NULL && fude_pdf_page_count(_doc->pdf) == 0u) {
        fude_pdf_close(_doc->pdf);
        _doc->pdf = NULL;
    }
    _doc->failed = _doc->pdf == NULL;
    if(_doc->pdf == NULL) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "document: the document %s could not be opened, or has no pages", _path);
        return;
    }
    _doc->page_count = fude_pdf_page_count(_doc->pdf);
    rde_arr_resize(&_doc->pages, _doc->page_count);    // All zero: emptied when the last one was let go.
    rde_arr_resize(&_doc->turned, _doc->page_count);
    rde_arr_resize(&_doc->seen, _doc->page_count);
    fude_doc_layout(_doc);
    _doc->moved_at = rde_engine_get_time_now();
    fude_doc_lines_load(_doc);
}

// See the header. The first call also makes the mutex that every worker shares (worker.h).
void fude_doc_init(fude_doc* _doc) {
    rde_memset(_doc, 0, sizeof(*_doc));
    _doc->lines   = rde_arr_new(sizeof(fude_doc_line), rde_memory_allocator_get_default_std());
    _doc->pages   = rde_arr_new(sizeof(fude_doc_page), rde_memory_allocator_get_default_std());
    _doc->turned  = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    _doc->seen    = rde_arr_new(sizeof(fude_doc_seen), rde_memory_allocator_get_default_std());
    _doc->reading = UINT32_MAX;
    _doc->wanted  = UINT32_MAX;
    fude_doc_worker_init(&_doc->worker);
}

// See the header. The arrays of matches and of one page's read lines are made only when first needed, so they are
// freed only when they were made.
void fude_doc_destroy(fude_doc* _doc) {
    fude_doc_close(_doc);
    if(rde_arr_is_inited(&_doc->lines)) {   // These four are made together, in fude_doc_init.
        rde_arr_free(&_doc->lines);
        rde_arr_free(&_doc->pages);
        rde_arr_free(&_doc->turned);
        rde_arr_free(&_doc->seen);
    }
    if(rde_arr_is_inited(&_doc->matches)) {
        rde_arr_free(&_doc->matches);
    }
    if(rde_arr_is_inited(&_doc->read_lines)) {
        rde_arr_free(&_doc->read_lines);
    }
}

// --- each frame -------------------------------------------------------------------------------

// The part of the canvas in view _v on a window _window pixels in size, as the box _min-_max in canvas units.
RDE_INTERNAL void fude_doc_in_view(fude_view _v, rde_vec_2I _window, rde_vec_2F* _min, rde_vec_2F* _max) {
    const f32 _z = rde_math_max_f32(_v.zoom, 1e-4f);
    *_min = (rde_vec_2F){ (-(f32)_window.x * 0.5f - _v.offset.x) / _z, (-(f32)_window.y * 0.5f - _v.offset.y) / _z };
    *_max = (rde_vec_2F){ ((f32)_window.x * 0.5f - _v.offset.x) / _z, ((f32)_window.y * 0.5f - _v.offset.y) / _z };
}

// Whether page _p reaches into the band of canvas y from _y0 up to _y1.
RDE_INTERNAL b8 fude_doc_page_between(const fude_doc_page* _p, f32 _y0, f32 _y1) {
    return _p->top >= _y0 && _p->top - _p->size.y <= _y1;
}

// Takes the worker's piece once it is drawn (one drawn the old way, the theme changed meanwhile, is thrown away by the
// worker). A drawn piece goes onto the GPU as its page's whole or sharp tile (a piece that failed to draw is dropped);
// a page's picture to read is handed to the app's reader.
RDE_INTERNAL void fude_doc_take(fude_doc* _doc, const fude_doc_place* _at) {
    fude_doc_piece* _piece = fude_doc_worker_take(&_doc->worker);
    if(_piece == NULL) {
        return;
    }
    if(_piece->kind == FUDE_DOC_PIECE_READ) {
        if(_piece->ok && _piece->page < _doc->page_count && _at->picture_read != NULL && _at->picture_read(_piece->pixels.memory, _piece->w, _piece->h)) {
            fude_doc_pages(_doc)[_piece->page].text = FUDE_DOC_TEXT_READING;
            _doc->reading                = _piece->page;
            _doc->read_w                 = _piece->w;
            _doc->read_h                 = _piece->h;
        } else if(!_piece->ok && _piece->page < _doc->page_count) {
            // This page's picture could not be drawn, so it has nothing to read: the reader goes on to the other pages,
            // and the page is not saved as read (FUDE_DOC_TEXT_FAILED), so the next opening tries it again.
            fude_doc_pages(_doc)[_piece->page].text = FUDE_DOC_TEXT_FAILED;
        } else {
            // The reader cannot take it now (for example ML Kit, Kana's reader, is off or not on this device), so it
            // is not asked again while this document is open.
            _doc->reader_off = true;   // Stop asking the reader for this document.
        }
    } else if(_piece->ok && _piece->page < _doc->page_count) {
        fude_doc_page* _p = &fude_doc_pages(_doc)[_piece->page];
        fude_doc_tile_take(_piece->kind == FUDE_DOC_PIECE_SHARP ? &_p->sharp : &_p->whole, _piece);
    }
    fude_doc_piece_free(_piece);
}

// Starts the worker on a new piece: draw the part of page _page at _from of _size (points) into _w x _h pixels, as a
// whole page, a sharp piece for _zoom, or a picture to read (_kind, FUDE_DOC_PIECE_).
RDE_INTERNAL void fude_doc_start(fude_doc* _doc, u32 _page, u8 _kind, rde_vec_2F _from, rde_vec_2F _size, u32 _w, u32 _h, f32 _zoom) {
    fude_doc_worker_start(&_doc->worker, _doc->pdf, _page, _kind, _from, _size, _w, _h, _zoom);
}

// Sees each page from view _v on a window _window pixels in size (worker.h: fude_doc_seen: on screen, within a screen
// above or below, or away; how far its middle is from the screen's; how wide it is on screen, the same for every page)
// and has fude_doc_wholes choose the whole pages to keep, freeing the others, and the next to draw: the page on screen
// into *_screen, the near one into *_near (UINT32_MAX: none).
RDE_INTERNAL void fude_doc_wholes_plan(fude_doc* _doc, fude_view _v, rde_vec_2I _window, u32* _screen, u32* _near) {
    rde_vec_2F _min, _max;
    fude_doc_in_view(_v, _window, &_min, &_max);
    const f32      _mid  = (_min.y + _max.y) * 0.5f;
    const f32      _high = _max.y - _min.y;
    const f32      _px   = _v.zoom * FUDE_DOC_PAGE_W * FUDE_DOC_SCALE;   // (every page is FUDE_DOC_PAGE_W wide)
    fude_doc_seen* _seen = (fude_doc_seen*)_doc->seen.memory;
    for(u32 _i = 0; _i < _doc->page_count; _i++) {
        fude_doc_page* _p     = &fude_doc_pages(_doc)[_i];
        const u8       _where = fude_doc_page_between(_p, _min.y, _max.y) ? FUDE_DOC_ON_SCREEN :
                                fude_doc_page_between(_p, _min.y - _high, _max.y + _high) ? FUDE_DOC_NEAR : FUDE_DOC_AWAY;
        _seen[_i] = (fude_doc_seen){ &_p->whole, _p->points, rde_math_abs_f32(_p->top - _p->size.y * 0.5f - _mid), _px, _i, _where };
    }
    fude_doc_wholes(_seen, _doc->page_count, _screen, _near);
}

// Starts drawing page _page whole, at its size on screen in view _v (worker.h: fude_doc_whole_size); its tile keeps the
// width level it was drawn for.
RDE_INTERNAL void fude_doc_start_whole(fude_doc* _doc, u32 _page, fude_view _v) {
    const fude_doc_page* _p = &fude_doc_pages(_doc)[_page];
    u32                  _level, _w, _h;
    fude_doc_whole_size(_p->points, _v.zoom * FUDE_DOC_PAGE_W * FUDE_DOC_SCALE, &_level, &_w, &_h);
    fude_doc_start(_doc, _page, FUDE_DOC_PIECE_WHOLE, (rde_vec_2F){ 0.0f, 0.0f }, _p->points, _w, _h, (f32)_level);
}

// Starts drawing the next piece, by need: the page on screen to draw whole (_screen, nearest the middle first, as
// fude_doc_wholes chose it); then, once the view rests (_resting), the pages' sharp pieces; then the page within a
// screen above or below to draw whole (_near), so scrolling finds them drawn.
RDE_INTERNAL void fude_doc_next(fude_doc* _doc, fude_view _v, rde_vec_2I _window, b8 _resting, u32 _screen, u32 _near) {
    if(_screen != UINT32_MAX) {
        fude_doc_start_whole(_doc, _screen, _v);
        return;
    }
    rde_vec_2F _min, _max;
    fude_doc_in_view(_v, _window, &_min, &_max);
    const f32 _px = _v.zoom * FUDE_DOC_PAGE_W * FUDE_DOC_SCALE;
    for(u32 _i = 0; _resting && _i < _doc->page_count; _i++) {
        const fude_doc_page* _p = &fude_doc_pages(_doc)[_i];
        if(!fude_doc_page_between(_p, _min.y, _max.y)) {
            continue;
        }
        // A sharp piece is needed only where the screen gives a page more pixels than its whole picture has (over 15%
        // more, so a small zoom does not draw it again).
        u32 _level, _whole_w, _whole_h;
        fude_doc_whole_size(_p->points, _px, &_level, &_whole_w, &_whole_h);
        if(_px <= (f32)_whole_w * 1.15f) {
            continue;
        }
        // The part of the page on screen, in canvas units, and then the same in the page's points.
        const f32 _x0 = rde_math_max_f32(-_p->size.x * 0.5f, _min.x), _x1 = rde_math_min_f32(_p->size.x * 0.5f, _max.x);
        const f32 _y0 = rde_math_max_f32(_p->top - _p->size.y, _min.y), _y1 = rde_math_min_f32(_p->top, _max.y);
        if(_x1 - _x0 < 1.0f || _y1 - _y0 < 1.0f) {
            continue;
        }
        const f32        _k    = _p->points.x / _p->size.x;
        const rde_vec_2F _from = { (_x0 + _p->size.x * 0.5f) * _k, (_p->top - _y1) * _k };
        const rde_vec_2F _size = { (_x1 - _x0) * _k, (_y1 - _y0) * _k };
        const fude_doc_tile* _s = &_p->sharp;
        if(_s->texture != NULL && rde_math_abs_f32(_s->zoom - _v.zoom) <= _v.zoom * 0.02f && _s->from.x <= _from.x + 0.5f && _s->from.y <= _from.y + 0.5f &&
           _s->from.x + _s->size.x >= _from.x + _size.x - 0.5f && _s->from.y + _s->size.y >= _from.y + _size.y - 0.5f) {
            continue;   // Its sharp piece already covers this part, at this zoom (within 2%).
        }
        // Its size in pixels, shrunk to fit FUDE_DOC_SHARP_MAX; a sliver under 8 pixels is not worth drawing.
        f32 _w = (_x1 - _x0) * _v.zoom * FUDE_DOC_SCALE;
        f32 _h = (_y1 - _y0) * _v.zoom * FUDE_DOC_SCALE;
        const f32 _over = rde_math_max_f32(_w, _h) / (f32)FUDE_DOC_SHARP_MAX;
        if(_over > 1.0f) {
            _w /= _over;
            _h /= _over;
        }
        if(_w < 8.0f || _h < 8.0f) {
            continue;
        }
        fude_doc_start(_doc, _i, FUDE_DOC_PIECE_SHARP, _from, _size, (u32)_w, (u32)_h, _v.zoom);
        return;
    }
    if(_near != UINT32_MAX) {
        fude_doc_start_whole(_doc, _near, _v);
    }
}

// Frees the sharp pieces of the pages off screen. (The whole pages far from the view are let go by
// fude_doc_wholes_plan.)
RDE_INTERNAL void fude_doc_let_go(fude_doc* _doc, fude_view _v, rde_vec_2I _window) {
    rde_vec_2F _min, _max;
    fude_doc_in_view(_v, _window, &_min, &_max);
    for(u32 _i = 0; _i < _doc->page_count; _i++) {
        fude_doc_page* _p = &fude_doc_pages(_doc)[_i];
        if(!fude_doc_page_between(_p, _min.y, _max.y)) {
            fude_doc_tile_free(&_p->sharp);
        }
    }
}

// --- reading pages without text of their own ---------------------------------------------

// Asks the next four pages whether they have text of their own (pdf.h: PDFKit on Apple's, PDFBox on Android), and
// marks those with none to be read. Only a few are asked each frame, so a long document does not stall one frame, and
// none before the PDF's text is ready (fude_pdf_text_prepare: Android's PDFBox opens it on a thread of its own), so
// the first question never waits for the opening.
RDE_INTERNAL void fude_doc_probe(fude_doc* _doc) {
    if(_doc->probe >= _doc->page_count || !fude_pdf_text_prepare(_doc->pdf)) {
        return;
    }
    for(u32 _k = 0; _k < 4u && _doc->probe < _doc->page_count; _k++, _doc->probe++) {
        fude_doc_page* _p = &fude_doc_pages(_doc)[_doc->probe];
        if(_p->text == FUDE_DOC_TEXT_UNKNOWN) {
            _p->text = fude_pdf_page_has_text(_doc->pdf, _doc->probe) ? FUDE_DOC_TEXT_OWN : FUDE_DOC_TEXT_TO_READ;
        }
    }
}

// Starts drawing the next page to read: the one a lasso asked for (wanted), else the page waiting to be read that is
// nearest the middle of the view. Its picture is FUDE_DOC_READ_PX wide and at most FUDE_DOC_READ_MAX_H pixels tall.
RDE_INTERNAL void fude_doc_read_next(fude_doc* _doc, fude_view _v) {
    i32       _best = -1;
    f32       _far  = 1e30f;
    const f32 _mid  = -_v.offset.y / rde_math_max_f32(_v.zoom, 1e-4f);
    if(_doc->wanted < _doc->page_count && fude_doc_pages(_doc)[_doc->wanted].text == FUDE_DOC_TEXT_TO_READ) {
        _best = (i32)_doc->wanted;
    }
    for(u32 _i = 0; _best < 0 && _i < _doc->page_count; _i++) {
        const fude_doc_page* _p = &fude_doc_pages(_doc)[_i];
        const f32            _d = rde_math_abs_f32(_p->top - _p->size.y * 0.5f - _mid);
        if(_p->text == FUDE_DOC_TEXT_TO_READ && _d < _far) {
            _far  = _d;
            _best = (i32)_i;
        }
    }
    if(_best >= 0) {
        const fude_doc_page* _p = &fude_doc_pages(_doc)[_best];
        const u32            _h = (u32)rde_math_min_f32((f32)FUDE_DOC_READ_MAX_H, rde_math_max_f32(1.0f, (f32)FUDE_DOC_READ_PX * _p->points.y / rde_math_max_f32(1.0f, _p->points.x)));
        fude_doc_start(_doc, (u32)_best, FUDE_DOC_PIECE_READ, (rde_vec_2F){ 0.0f, 0.0f }, _p->points, FUDE_DOC_READ_PX, _h, 0.0f);
    }
}

// Takes the reader's answer for the page being read, once it has come: its lines are turned from the picture's
// pixels into the page's points and kept, the page is marked read, and the lines file is saved at once. An answer no
// longer wanted (reader_drain) is taken and dropped.
RDE_INTERNAL void fude_doc_read_take(fude_doc* _doc, const fude_doc_place* _at) {
    if(_at->picture_lines == NULL) {
        return;
    }
    if(!rde_arr_is_inited(&_doc->read_lines)) {
        _doc->read_lines = rde_arr_new(sizeof(fude_doc_line), rde_memory_allocator_get_default_std());
    }
    rde_arr_clear(&_doc->read_lines);
    if(_doc->reader_drain) {
        if(_at->picture_lines(&_doc->read_lines)) {
            _doc->reader_drain = false;   // It was for a document let go, or a page turned since: dropped.
        }
        return;
    }
    if(_doc->reading == UINT32_MAX || !_at->picture_lines(&_doc->read_lines)) {
        return;
    }
    const u32            _page  = _doc->reading;
    const fude_doc_page* _p     = &fude_doc_pages(_doc)[_page];
    // Points per pixel, across and down: a page much taller than it is wide was drawn squeezed down to
    // FUDE_DOC_READ_MAX_H, so its two scales differ.
    const f32            _kx    = _p->points.x / (f32)(_doc->read_w > 0u ? _doc->read_w : 1u);
    const f32            _ky    = _p->points.y / (f32)(_doc->read_h > 0u ? _doc->read_h : 1u);
    const fude_doc_line* _read  = (const fude_doc_line*)_doc->read_lines.memory;
    const u32            _count = (u32)rde_arr_length(&_doc->read_lines);
    for(u32 _i = 0; _i < _count; _i++) {
        fude_doc_line _line = _read[_i];
        if(_line.text[0] == 0) {
            continue;
        }
        _line.page   = _page;
        _line.from.x *= _kx;
        _line.from.y *= _ky;
        _line.size.x *= _kx;
        _line.size.y *= _ky;
        rde_arr_add(&_doc->lines, &_line);
    }
    fude_doc_pages(_doc)[_page].text = FUDE_DOC_TEXT_READ;
    _doc->reading           = UINT32_MAX;
    _doc->wanted            = _doc->wanted == _page ? UINT32_MAX : _doc->wanted;
    fude_doc_lines_save(_doc);
}

// Whether pages are still to be read (or not yet asked whether they have text), with a reader there that can read
// them. A search is not done until they are, since their text may match too.
RDE_INTERNAL b8 fude_doc_reading_left(const fude_doc* _doc, const fude_doc_place* _at) {
    if(_doc->reader_off || _at->picture_read == NULL) {
        return false;
    }
    for(u32 _i = 0; _i < _doc->page_count; _i++) {
        const u8 _t = fude_doc_pages(_doc)[_i].text;
        if(_t == FUDE_DOC_TEXT_TO_READ || _t == FUDE_DOC_TEXT_READING || _t == FUDE_DOC_TEXT_UNKNOWN) {
            return true;
        }
    }
    return false;
}

// Counts the characters (UTF-8 code points) in the first _bytes bytes of _s, stopping at its end.
RDE_INTERNAL u32 fude_doc_chars(const c8* _s, usize _bytes) {
    u32 _n = 0;
    for(usize _i = 0; _i < _bytes && _s[_i] != 0; _i++) {
        _n += ((u8)_s[_i] & 0xC0u) != 0x80u ? 1u : 0u;
    }
    return _n;
}

// Adds the search's matches in the read lines after the PDF's own, up to FUDE_PDF_MATCHES in all. They are compared as
// the PDF's own search compares (fude_utf8_find: case, accents and full-width forms alike), and each is found after the
// one before, as the PDF's are. A read line has a box but no place for each character, so a match's box is the part
// of the line's width its characters take, as if every character were the same width.
RDE_INTERNAL void fude_doc_search_lines(fude_doc* _doc) {
    const fude_doc_line* _lines = (const fude_doc_line*)_doc->lines.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_doc->lines) && _doc->match_count < FUDE_PDF_MATCHES; _i++) {
        const fude_doc_line* _l     = &_lines[_i];
        const u32            _total = fude_doc_chars(_l->text, rde_strlen(_l->text));
        usize                _end   = 0;
        for(i32 _at = fude_utf8_find(_l->text, _doc->query, 0u, &_end); _at >= 0 && _doc->match_count < FUDE_PDF_MATCHES;
            _at = fude_utf8_find(_l->text, _doc->query, _end, &_end)) {
            const u32 _before = fude_doc_chars(_l->text, (usize)_at);
            const u32 _len    = fude_doc_chars(&_l->text[_at], _end - (usize)_at);
            const f32 _each   = _l->size.x / (f32)(_total > 0u ? _total : 1u);
            fude_pdf_match* _m = &fude_doc_matches(_doc)[_doc->match_count++];
            _m->page = _l->page;
            _m->from = (rde_vec_2F){ _l->from.x + _each * (f32)_before, _l->from.y };
            _m->size = (rde_vec_2F){ _each * (f32)_len, _l->size.y };
        }
    }
}

// See the header. The steps below run in this order each frame. A step that changes how the pages look (a page
// turned, the theme changed) drops their textures and the piece being drawn, so they are drawn again.
void fude_doc_update(fude_doc* _doc, const fude_doc_place* _at) {
    if(_doc->reader_drain) {
        fude_doc_read_take(_doc, _at);   // A reading for a document let go is dropped when it comes.
    }
    // When the open canvas names another document, the one open is closed and the new one opened.
    if(rde_strcmp(_at->path, _doc->path) != 0) {
        fude_doc_close(_doc);
        if(_at->path[0] != 0) {
            fude_doc_open(_doc, _at->path);
        }
    }
    if(_doc->pdf == NULL) {
        fude_canvas_set_reader(_at->canvas, false, 0.0f, 0.0f);
        return;
    }
    // Pages without text of their own are read one at a time, when the app reads pictures.
    _doc->reader_off = _doc->reader_off || _at->picture_read == NULL;   // An app that reads no pictures.
    if(!_doc->reader_off) {
        fude_doc_probe(_doc);
        fude_doc_read_take(_doc, _at);
    }
    // A search's matches are gathered again each frame as they come in, the PDF's own first, then those in the read
    // pages; the first one is shown as soon as it comes.
    if(_doc->searching) {
        b8        _done  = false;
        const u32 _had   = _doc->match_count;
        _doc->match_count = fude_pdf_find_matches(_doc->pdf, fude_doc_matches(_doc), FUDE_PDF_MATCHES, &_done);
        fude_doc_search_lines(_doc);
        _doc->search_done = _done && !fude_doc_reading_left(_doc, _at);
        if(_had == 0u && _doc->match_count > 0u) {
            _doc->match_at = 0u;
            fude_doc_search_step(_doc, _at->canvas, 0);
        }
    }
    // When the canvas's turns (its page, canvas.h) have moved on since the document took them, or it has just
    // opened, each page's turn is looked at again. A page whose turn changed is told to the PDF, and the pages are laid
    // out and drawn again. A search is stopped, since its matches were placed on the pages as they were. A PDF must
    // not be drawn while it turns (pdf.h), so a turn waits for the piece being drawn (a frame or two), and no new piece
    // is started meanwhile: the turn cannot be put off for good by one piece after another.
    // A piece still being drawn while a turn waits shows the pages as they were: it is thrown away when taken (a page's
    // picture for the reader would be read the old way round, and its lines placed on the turned page).
    u8* _turned       = (u8*)_doc->turned.memory;
    b8  _turn_waiting = !_doc->turns_taken || _doc->turns_seen != _at->canvas->page.revision;
    if(_turn_waiting && fude_doc_worker_busy(&_doc->worker)) {
        fude_doc_worker_forget(&_doc->worker);
    }
    if(_turn_waiting && !fude_doc_worker_busy(&_doc->worker)) {
        b8 _changed = false;
        for(u32 _i = 0; _i < _doc->page_count; _i++) {
            const u8 _q = fude_page_turned(&_at->canvas->page, _i);
            if(_q != _turned[_i]) {
                _turned[_i] = _q;
                fude_pdf_set_turn(_doc->pdf, _i, _q);
                _changed = true;
            }
        }
        _doc->turns_seen  = _at->canvas->page.revision;
        _doc->turns_taken = true;
        if(_changed) {
            fude_doc_drop_tiles(_doc);
            fude_doc_layout(_doc);
            fude_doc_search_stop(_doc);
        }
        _turn_waiting = false;
    }
    // The canvas's view is a reader's (canvas.h): kept on the pages, from the first page's top to the last's bottom.
    const fude_doc_page* _last = &fude_doc_pages(_doc)[_doc->page_count - 1u];
    fude_canvas_set_reader(_at->canvas, true, FUDE_DOC_PAGE_W, _last->top - _last->size.y);
    // In a dark theme the pages are drawn in its colours (worker.h: fude_doc_worker_colours). When those change, the
    // pages are drawn again, and the piece being drawn in the old ones is thrown away when done.
    if(fude_doc_worker_colours(&_doc->worker)) {
        fude_doc_drop_tiles(_doc);
    }
    // Note when the view last moved, to tell when it has come to rest (FUDE_DOC_REST) and how long to show the pill.
    const fude_view _v   = _at->canvas->view;
    const f64       _now = rde_engine_get_time_now();
    if(_v.zoom != _doc->seen_view.zoom || _v.offset.x != _doc->seen_view.offset.x || _v.offset.y != _doc->seen_view.offset.y) {
        _doc->seen_view = _v;
        _doc->moved_at  = _now;
    }
    // The whole pages to keep and to draw are chosen each frame, so those no longer kept are let go at once.
    const rde_vec_2I _window = rde_window_get_size(_at->window);
    fude_doc_take(_doc, _at);
    u32 _screen, _near;
    fude_doc_wholes_plan(_doc, _v, _window, &_screen, &_near);
    if(!fude_doc_worker_busy(&_doc->worker) && !_turn_waiting) {
        fude_doc_next(_doc, _v, _window, _now - _doc->moved_at >= FUDE_DOC_REST, _screen, _near);
    }
    // When there is nothing to draw, a page's picture is drawn for the reader, one page at a time.
    if(!fude_doc_worker_busy(&_doc->worker) && !_turn_waiting && _doc->reading == UINT32_MAX && !_doc->reader_drain && !_doc->reader_off && _at->picture_read != NULL) {
        fude_doc_read_next(_doc, _v);
    }
    fude_doc_let_go(_doc, _v, _window);
}

// --- drawing ----------------------------------------------------------------------------------

// Draws a tile over its part of page _p, on screen in view _v (nothing when it is not drawn yet).
RDE_INTERNAL void fude_doc_draw_tile(const fude_doc_page* _p, const fude_doc_tile* _tile, fude_view _v) {
    if(_tile->texture == NULL) {
        return;
    }
    const f32        _k      = _p->size.x / rde_math_max_f32(1.0f, _p->points.x);   // Canvas units per point.
    const rde_vec_2F _c      = { -_p->size.x * 0.5f + (_tile->from.x + _tile->size.x * 0.5f) * _k, _p->top - (_tile->from.y + _tile->size.y * 0.5f) * _k };
    const rde_vec_2F _screen = { _tile->size.x * _k * _v.zoom, _tile->size.y * _k * _v.zoom };
    const rde_texture* _tex  = rde_memory_texture_get_texture(_tile->texture);
    const rde_vec_2UI  _px   = rde_texture_get_size(_tex);
    rde_rendering_2d_draw_texture_2(_tex, (rde_vec_3F){ fude_draw_x(_c.x * _v.zoom + _v.offset.x), _c.y * _v.zoom + _v.offset.y, 0.0f },
                                    (rde_vec_2F){ _screen.x / (f32)_px.x, _screen.y / (f32)_px.y }, 0.0f, (rde_color){ 255, 255, 255, 255 });
}

// See the header. A page not drawn yet shows as blank paper; its whole picture and then its sharp piece go over it.
void fude_doc_render(fude_doc* _doc, const fude_canvas* _canvas, rde_window* _window) {
    if(_doc->pdf == NULL) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    const fude_view   _v = _canvas->view;
    rde_vec_2F        _min, _max;
    fude_doc_in_view(_v, rde_window_get_size(_window), &_min, &_max);
    for(u32 _i = 0; _i < _doc->page_count; _i++) {
        const fude_doc_page* _p = &fude_doc_pages(_doc)[_i];
        if(!fude_doc_page_between(_p, _min.y, _max.y)) {
            continue;
        }
        // Its edge (a rectangle 3 units larger, in the outline colour), the sheet (blank until the page is drawn),
        // then the pictures.
        const rde_vec_2F _c    = { _v.offset.x, (_p->top - _p->size.y * 0.5f) * _v.zoom + _v.offset.y };
        const rde_vec_2F _size = { _p->size.x * _v.zoom, _p->size.y * _v.zoom };
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ fude_draw_x(_c.x), _c.y - 1.5f }, (rde_vec_2F){ _size.x + 3.0f, _size.y + 3.0f }, _t->outline);
        rde_rendering_2d_draw_rectangle(fude_draw_at(_c), _size, _doc->worker.paper);
        fude_doc_draw_tile(_p, &_p->whole, _v);
        fude_doc_draw_tile(_p, &_p->sharp, _v);
    }
    // A search's matches are marked in the accent colour; the one shown is marked more strongly and outlined.
    for(u32 _m = 0; _doc->searching && _m < _doc->match_count; _m++) {
        const fude_pdf_match* _match = &fude_doc_matches(_doc)[_m];
        if(_match->page >= _doc->page_count || !fude_doc_page_between(&fude_doc_pages(_doc)[_match->page], _min.y, _max.y)) {
            continue;
        }
        const fude_doc_page* _p = &fude_doc_pages(_doc)[_match->page];
        const f32            _k = _p->size.x / rde_math_max_f32(1.0f, _p->points.x) * _v.zoom;   // Screen units/point.
        const rde_vec_2F     _c = { (-_p->size.x * 0.5f) * _v.zoom + _v.offset.x + (_match->from.x + _match->size.x * 0.5f) * _k,
                                    _p->top * _v.zoom + _v.offset.y - (_match->from.y + _match->size.y * 0.5f) * _k };
        const rde_vec_2F     _s = { _match->size.x * _k + 4.0f, _match->size.y * _k + 4.0f };
        rde_color            _mark = _t->accent;
        _mark.a = _m == _doc->match_at ? 120u : 60u;
        rde_rendering_2d_draw_rectangle(fude_draw_at(_c), _s, _mark);
        if(_m == _doc->match_at) {
            rde_rendering_2d_draw_rectangle_border(fude_draw_at(_c), _s, 2.0f, _t->accent, NULL);
        }
    }
}

// --- reading its text, searching it, going to a page --------------------------------------------

// The number of pages, 0 when no document is open.
u32 fude_doc_page_count(const fude_doc* _doc) {
    return _doc->pdf != NULL ? _doc->page_count : 0u;
}

// Writes the text of read page _page inside the box _a-_b (points from the page's top-left) into _out, a line each,
// and returns the bytes written. A line counts when at least half its height is in the box. Of it, the characters
// whose middle is in the box are taken, as if the characters shared the line's width evenly.
RDE_INTERNAL usize fude_doc_read_text_in(const fude_doc* _doc, u32 _page, rde_vec_2F _a, rde_vec_2F _b, c8* _out, usize _size) {
    const fude_doc_line* _lines = (const fude_doc_line*)_doc->lines.memory;
    usize                _n     = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_doc->lines) && _n + 2u < _size; _i++) {
        const fude_doc_line* _l = &_lines[_i];
        if(_l->page != _page) {
            continue;
        }
        const f32 _over = rde_math_min_f32(_b.y, _l->from.y + _l->size.y) - rde_math_max_f32(_a.y, _l->from.y);
        if(_over < _l->size.y * 0.5f) {
            continue;
        }
        const u32 _total = fude_doc_chars(_l->text, rde_strlen(_l->text));
        const f32 _each  = _l->size.x / (f32)(_total > 0u ? _total : 1u);
        usize     _start = _n;
        u32       _c     = 0;
        for(const c8* _p = _l->text; *_p != 0 && _n + 5u < _size; _c++) {
            const c8* _from = _p;
            fude_utf8_next(&_p);
            const f32 _mid = _l->from.x + _each * ((f32)_c + 0.5f);
            if(_mid >= _a.x && _mid <= _b.x) {
                rde_memcpy(&_out[_n], _from, (usize)(_p - _from));
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

// See the header. Line breaks and spaces at the end are trimmed.
usize fude_doc_text_in(fude_doc* _doc, rde_vec_2F _min, rde_vec_2F _max, c8* _out, usize _size, b8* _pending) {
    usize _n = 0;
    if(_size > 0) {
        _out[0] = 0;
    }
    if(_pending != NULL) {
        *_pending = false;
    }
    for(u32 _i = 0; _doc->pdf != NULL && _i < _doc->page_count && _n + 2u < _size; _i++) {
        fude_doc_page* _p = &fude_doc_pages(_doc)[_i];
        if(!fude_doc_page_between(_p, _min.y, _max.y)) {
            continue;
        }
        // The part of the box on this page, in the page's points from its top-left.
        const f32 _x0 = rde_math_max_f32(_min.x, -_p->size.x * 0.5f), _x1 = rde_math_min_f32(_max.x, _p->size.x * 0.5f);
        const f32 _y0 = rde_math_max_f32(_min.y, _p->top - _p->size.y), _y1 = rde_math_min_f32(_max.y, _p->top);
        if(_x1 <= _x0 || _y1 <= _y0) {
            continue;
        }
        const f32        _k    = _p->points.x / rde_math_max_f32(1.0f, _p->size.x);
        const rde_vec_2F _from = { (_x0 + _p->size.x * 0.5f) * _k, (_p->top - _y1) * _k };
        const rde_vec_2F _box  = { (_x1 - _x0) * _k, (_y1 - _y0) * _k };
        if(_p->text == FUDE_DOC_TEXT_UNKNOWN && fude_pdf_text_prepare(_doc->pdf)) {
            _p->text = fude_pdf_page_has_text(_doc->pdf, _i) ? FUDE_DOC_TEXT_OWN : FUDE_DOC_TEXT_TO_READ;
        }
        if(_n > 0u && _out[_n - 1u] != '\n') {
            _out[_n++] = '\n';
            _out[_n]   = 0;
        }
        if(_p->text == FUDE_DOC_TEXT_OWN) {
            _n += fude_pdf_text_in(_doc->pdf, _i, _from, _box, &_out[_n], _size - _n);
        } else if(_p->text == FUDE_DOC_TEXT_READ || _p->text == FUDE_DOC_TEXT_FAILED) {
            // (a page whose picture failed has no lines: nothing comes, and nothing is waited for)
            _n += fude_doc_read_text_in(_doc, _i, _from, (rde_vec_2F){ _from.x + _box.x, _from.y + _box.y }, &_out[_n], _size - _n);
        } else if(!_doc->reader_off || _p->text == FUDE_DOC_TEXT_UNKNOWN) {
            // Still to be read, or the PDF's text still being opened: it is read next, and the caller asks again.
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

// Scrolls so the canvas box _min-_max is in the middle of the screen, keeping the zoom. Across, this shows only when
// zoomed in past 100%; otherwise the reader's frame (canvas.h) keeps the pages where they are.
RDE_INTERNAL void fude_doc_show_box(fude_canvas* _canvas, rde_vec_2F _min, rde_vec_2F _max) {
    fude_view* _v = &_canvas->view;
    _v->offset.y  = -(_min.y + _max.y) * 0.5f * _v->zoom;
    _v->offset.x  = -(_min.x + _max.x) * 0.5f * _v->zoom;
    _canvas->coasting = false;
}

// See the header. The page's top goes FUDE_CANVAS_READER_TOP below the top of the safe area, under the menu button.
void fude_doc_go_to_page(fude_doc* _doc, fude_canvas* _canvas, u32 _page) {
    if(_doc->pdf == NULL || _doc->page_count == 0u) {
        return;
    }
    const fude_doc_page* _p = &fude_doc_pages(_doc)[_page < _doc->page_count ? _page : _doc->page_count - 1u];
    const fude_canvas_reader* _r = &_canvas->reader;
    _canvas->coasting      = false;
    _canvas->view.offset.y = (f32)_r->window.y * 0.5f - (f32)_r->safe.y - FUDE_CANVAS_READER_TOP - _p->top * _canvas->view.zoom;
    _doc->moved_at         = rde_engine_get_time_now();   // Show the pill, so it says which page this is.
}

// See the header. The array of matches is made the first time, with room for FUDE_PDF_MATCHES.
void fude_doc_search(fude_doc* _doc, const c8* _query) {
    fude_doc_search_stop(_doc);
    if(_doc->pdf == NULL || _query == NULL || _query[0] == 0) {
        return;
    }
    if(!rde_arr_is_inited(&_doc->matches)) {
        _doc->matches = rde_arr_new(sizeof(fude_pdf_match), rde_memory_allocator_get_default_std());
        rde_arr_resize(&_doc->matches, FUDE_PDF_MATCHES);
    }
    _doc->searching   = true;
    _doc->search_done = false;
    _doc->match_count = 0u;
    _doc->match_at    = 0u;
    rde_snprintf(_doc->query, sizeof(_doc->query), "%s", _query);
    fude_pdf_find_start(_doc->pdf, _query);
}

// See the header. Scrolls to the match shown; a _step of 0 shows the current one again.
void fude_doc_search_step(fude_doc* _doc, fude_canvas* _canvas, i32 _step) {
    if(!_doc->searching || _doc->match_count == 0u) {
        return;
    }
    _doc->match_at = (u32)(((i32)_doc->match_at + _step + (i32)_doc->match_count) % (i32)_doc->match_count);
    const fude_pdf_match* _m = &fude_doc_matches(_doc)[_doc->match_at];
    if(_m->page >= _doc->page_count) {
        return;
    }
    const fude_doc_page* _p = &fude_doc_pages(_doc)[_m->page];
    const f32            _k = _p->size.x / rde_math_max_f32(1.0f, _p->points.x);
    fude_doc_show_box(_canvas, (rde_vec_2F){ -_p->size.x * 0.5f + _m->from.x * _k, _p->top - (_m->from.y + _m->size.y) * _k },
                      (rde_vec_2F){ -_p->size.x * 0.5f + (_m->from.x + _m->size.x) * _k, _p->top - _m->from.y * _k });
    _doc->moved_at = rde_engine_get_time_now();
}

// Ends the search, telling the PDF to stop too, and forgets its matches.
void fude_doc_search_stop(fude_doc* _doc) {
    if(_doc->searching && _doc->pdf != NULL) {
        fude_pdf_find_stop(_doc->pdf);
    }
    _doc->searching   = false;
    _doc->search_done = false;
    _doc->match_count = 0u;
    _doc->match_at    = 0u;
}

// See the header. *_done is also true when there is no search.
u32 fude_doc_search_count(const fude_doc* _doc, u32* _at, b8* _done) {
    *_at   = _doc->match_at;
    *_done = !_doc->searching || _doc->search_done;
    return _doc->searching ? _doc->match_count : 0u;
}

// The page in the middle of the screen in view _v, counting from 0: the first page whose bottom, less half the gap
// below it, is at or below the middle of the screen. Past the end it is the last page.
RDE_INTERNAL u32 fude_doc_page_in_middle(const fude_doc* _doc, fude_view _v) {
    const f32 _mid = -_v.offset.y / rde_math_max_f32(_v.zoom, 1e-4f);
    for(u32 _i = 0; _i < _doc->page_count; _i++) {
        if(_mid >= fude_doc_pages(_doc)[_i].top - fude_doc_pages(_doc)[_i].size.y - FUDE_DOC_PAGE_GAP * 0.5f) {
            return _i;
        }
    }
    return _doc->page_count > 0 ? _doc->page_count - 1u : 0u;
}

// See the header. The pill sits near the bottom of the screen, above the safe area, and fades out over
// FUDE_DOC_PILL_FADE seconds once FUDE_DOC_PILL_TIME has passed since the view last moved.
void fude_doc_render_pill(fude_doc* _doc, fude_view _view, rde_font* _font, f32 _font_px, rde_window* _window) {
    const f64 _age = rde_engine_get_time_now() - _doc->moved_at;
    if(_doc->pdf == NULL || _doc->page_count < 2u || _font == NULL || _age >= FUDE_DOC_PILL_TIME + FUDE_DOC_PILL_FADE) {
        return;
    }
    const f32 _fade = _age <= FUDE_DOC_PILL_TIME ? 1.0f : 1.0f - (f32)((_age - FUDE_DOC_PILL_TIME) / FUDE_DOC_PILL_FADE);
    c8        _text[512];
    FUDE_TEXTF(_text, FUDE_TEXT_OF_N, FUDE_TN(fude_doc_page_in_middle(_doc, _view) + 1u), FUDE_TN(_doc->page_count));
    const f32         _px     = 18.0f;
    const f32         _w      = fude_draw_text_width(_font, _font_px, _text, _px);
    const fude_theme* _t      = fude_theme_active();
    const rde_vec_2I  _size   = rde_window_get_size(_window);
    const rde_vec_4I  _safe   = rde_window_get_safe_area_insets(_window);   // Left, top, right, bottom.
    const rde_vec_2F  _pill   = { rde_math_max_f32(72.0f, _w + 32.0f), 40.0f };
    const rde_vec_2F  _center = { 0.0f, -(f32)_size.y * 0.5f + (f32)_safe.w + 16.0f + _pill.y * 0.5f };
    rde_color _back = _t->panel;
    _back.a         = (u8)((f32)_back.a * _fade);
    rde_rendering_2d_draw_rounded_rectangle(fude_draw_at(_center), _pill, 1.0f, 8u, _back, NULL);
    rde_color _ink = _t->button_text;
    _ink.a         = (u8)((f32)_ink.a * _fade);
    fude_draw_text(_font, _font_px, _text, _center.x - _w * 0.5f, _center.y - _px * 0.36f, _px, _ink);
}

// --- new canvases -------------------------------------------------------------------------------

// See the header.
fude_view fude_doc_first_view(rde_window* _window) {
    const rde_vec_2I _size = rde_window_get_size(_window);
    const rde_vec_4I _safe = rde_window_get_safe_area_insets(_window);   // Left, top, right, bottom.
    fude_view        _v;
    _v.zoom   = fude_canvas_reader_fit(FUDE_DOC_PAGE_W, _size, _safe);   // 100%: the pages' width across the screen.
    _v.offset = (rde_vec_2F){ (f32)(_safe.x - _safe.z) * 0.5f, (f32)_size.y * 0.5f - (f32)_safe.y - FUDE_CANVAS_READER_TOP };   // First page's top under the menu.
    return _v;
}

// See the header: the page in the middle of _view.
u32 fude_doc_page_at(const fude_doc* _doc, fude_view _view) {
    return fude_doc_page_in_middle(_doc, _view);
}

// --- written out ------------------------------------------------------------------------------

// See the header. Each page is copied from the PDF, then the ink over it: the marker's strokes first, under the rest,
// as on the canvas. A stroke is taken for a page when its bounds touch the page. Then the page's read text goes in
// unseen. Copying a page draws the PDF on this thread, so the piece the worker is drawing, if any, is waited for first
// (pdf.h: one thread at a time); no other starts before the next frame.
b8 fude_doc_export(fude_doc* _doc, const fude_ink* _ink, const c8* _out) {
    if(_doc->pdf == NULL || _ink == NULL) {
        return false;
    }
    fude_pdf_writer* _w = fude_pdf_write_begin(_out);
    if(_w == NULL) {
        return false;
    }
    fude_doc_worker_wait(&_doc->worker);
    // Scratch arrays for one stroke's points and radii, in the page's points.
    rde_arr TYPE(rde_vec_2F) _point_list  = rde_arr_new(sizeof(rde_vec_2F), rde_memory_allocator_get_default_std());
    rde_arr TYPE(f32)        _radius_list = rde_arr_new(sizeof(f32), rde_memory_allocator_get_default_std());
    const rde_color _ink_colour = fude_theme_get(FUDE_THEME_PAPER)->ink;   // White paper: the light theme's ink.
    for(u32 _i = 0; _i < _doc->page_count; _i++) {
        const fude_doc_page* _p = &fude_doc_pages(_doc)[_i];
        const f32            _k = _p->points.x / rde_math_max_f32(1.0f, _p->size.x);   // Points per canvas unit.
        fude_pdf_write_page(_w, _doc->pdf, _i);
        // Two passes: the marker's strokes first, under the rest, as they are on the canvas.
        for(u32 _pass = 0; _pass < 2u; _pass++) {
            for(u32 _s = 0; _s < fude_ink_stroke_count(_ink); _s++) {
                const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _s);
                if(!_stroke->alive || _stroke->point_count == 0u || _stroke->marker != (_pass == 0u) ||
                   _stroke->bounds_max.x < -_p->size.x * 0.5f || _stroke->bounds_min.x > _p->size.x * 0.5f ||
                   _stroke->bounds_max.y < _p->top - _p->size.y || _stroke->bounds_min.y > _p->top) {
                    continue;
                }
                rde_arr_resize(&_point_list, _stroke->point_count);
                rde_arr_resize(&_radius_list, _stroke->point_count);
                rde_vec_2F*           _points = (rde_vec_2F*)_point_list.memory;
                f32*                  _radii  = (f32*)_radius_list.memory;
                const fude_ink_point* _pts = fude_ink_stroke_points(_ink, _stroke);
                u32                   _n   = 0;
                rde_vec_2F            _sum = { 0.0f, 0.0f };
                u32                   _summed = 0;
                for(u32 _q = 0; _q < _stroke->point_count; _q++) {
                    const rde_vec_2F _at = { (_pts[_q].position.x + _p->size.x * 0.5f) * _k, (_p->top - _pts[_q].position.y) * _k };
                    const f32        _r  = _pts[_q].radius * _k;
                    // The marker keeps only points a good part of its width apart (FUDE_INK_MARKER_STEP radii), each
                    // the average of the samples since the last one kept, as fude_ink_render draws it.
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
        // The text read in the page's picture goes in unseen, so a PDF reader can find it.
        const fude_doc_line* _lines = (const fude_doc_line*)_doc->lines.memory;
        for(u32 _l = 0; _p->text == FUDE_DOC_TEXT_READ && _l < (u32)rde_arr_length(&_doc->lines); _l++) {
            if(_lines[_l].page == _i) {
                fude_pdf_write_hidden_text(_w, _lines[_l].text, _lines[_l].from, _lines[_l].size);
            }
        }
        fude_pdf_write_page_end(_w);
    }
    rde_arr_free(&_point_list);
    rde_arr_free(&_radius_list);
    return fude_pdf_write_end(_w);
}

// --- a page turned -------------------------------------------------------------------------------

// What fude_doc_turn_point needs to move the ink when a page is turned a quarter.
typedef struct {
    const fude_doc* doc;
    u32             page;      // The page turned.
    f32             top;       // Its top (canvas y); then its height before and after the turn.
    f32             h_old, h_new;
    u32*            owner;     // Each stroke's page, by where the middle of its bounds is.
} fude_doc_turning;

// Moves one point of ink (fude_ink_remap's hook). On the page turned, it turns with the page: a quarter clockwise
// about the page's top-left, then scaled so the page is as wide as the pages again. On a page after it, it moves by
// how much shorter or taller the turned page became.
RDE_INTERNAL void fude_doc_turn_point(any _user, u32 _stroke, fude_ink_point* _point) {
    const fude_doc_turning* _t = (const fude_doc_turning*)_user;
    const u32               _p = _t->owner[_stroke];
    if(_p == _t->page) {
        const f32 _f  = FUDE_DOC_PAGE_W / rde_math_max_f32(1.0f, _t->h_old);
        const f32 _px = _point->position.x + FUDE_DOC_PAGE_W * 0.5f;   // From the page's top-left, across...
        const f32 _py = _t->top - _point->position.y;                    // ...and down.
        _point->position = (rde_vec_2F){ _f * (_t->h_old - _py) - FUDE_DOC_PAGE_W * 0.5f, _t->top - _f * _px };
        _point->radius  *= _f;
    } else if(_p != UINT32_MAX && _p > _t->page) {
        _point->position.y += _t->h_old - _t->h_new;
    }
}

// See the header. The new layout itself comes on the next fude_doc_update, which follows the canvas's page.
b8 fude_doc_turn_page(fude_doc* _doc, fude_canvas* _canvas, fude_ink* _ink, u32 _page) {
    if(_doc->pdf == NULL || _page >= _doc->page_count) {
        return false;
    }
    const fude_doc_page* _p = &fude_doc_pages(_doc)[_page];
    if(!fude_page_turn_more(&_canvas->page, _page)) {
        return false;
    }
    // Which page each stroke is on: a page owns the band from its top down to halfway into the gap below it.
    const u32        _count = fude_ink_stroke_count(_ink);
    fude_doc_turning _t     = { _doc, _page, _p->top, _p->size.y, FUDE_DOC_PAGE_W * _p->points.x / rde_math_max_f32(1.0f, _p->points.y), NULL };
    rde_arr TYPE(u32) _owner = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_owner, _count);
    _t.owner = (u32*)_owner.memory;   // Safe to keep: the array does not grow while the ink is remapped.
    for(u32 _s = 0; _s < _count; _s++) {
        const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _s);
        const f32              _mid    = (_stroke->bounds_min.y + _stroke->bounds_max.y) * 0.5f;
        _t.owner[_s] = _doc->page_count - 1u;
        for(u32 _i = 0; _i < _doc->page_count; _i++) {
            if(_mid >= fude_doc_pages(_doc)[_i].top - fude_doc_pages(_doc)[_i].size.y - FUDE_DOC_PAGE_GAP * 0.5f) {
                _t.owner[_s] = _i;
                break;
            }
        }
    }
    fude_ink_remap(_ink, fude_doc_turn_point, &_t);
    rde_arr_free(&_owner);
    // Its text read from its picture is dropped and read again, since the page is upright now.
    fude_doc_line* _lines = (fude_doc_line*)_doc->lines.memory;
    u32            _kept  = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_doc->lines); _i++) {
        if(_lines[_i].page != _page) {
            _lines[_kept++] = _lines[_i];
        }
    }
    rde_arr_resize(&_doc->lines, _kept);
    if(_doc->reading == _page) {
        _doc->reader_drain = true;   // It was read the old way, so it is dropped when it comes.
        _doc->reading      = UINT32_MAX;
    }
    const u8 _was = fude_doc_pages(_doc)[_page].text;
    if(_was == FUDE_DOC_TEXT_READ || _was == FUDE_DOC_TEXT_READING || _was == FUDE_DOC_TEXT_FAILED) {
        fude_doc_pages(_doc)[_page].text = FUDE_DOC_TEXT_UNKNOWN;
        _doc->probe             = _page < _doc->probe ? _page : _doc->probe;
        fude_doc_lines_save(_doc);
    }
    fude_doc_search_stop(_doc);
    return true;   // Laid out again on the next update, which follows the canvas's page.
}
