// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/ink/pagefile.h"
#include "drawing/base/kfile.h"
#include "drawing/base/theme.h"

// ===========================================================================
// See pagefile.h.
// ===========================================================================

// The page file's kind, and its chunks' tags (their layouts are in pagefile.h).
#define FUDE_KIND_DOCUMENT  FUDE_TAG('D', 'O', 'C', ' ')

#define FUDE_CHUNK_VIEW     FUDE_TAG('V', 'I', 'E', 'W')
#define FUDE_CHUNK_STROKES  FUDE_TAG('S', 'T', 'R', 'K')
#define FUDE_CHUNK_PAGE     FUDE_TAG('P', 'A', 'G', 'E')
#define FUDE_CHUNK_TURN     FUDE_TAG('T', 'U', 'R', 'N')
#define FUDE_CHUNK_POINTS   FUDE_TAG('P', 'N', 'T', 'S')

// The sizes this build writes, in bytes: a stroke's record (point count 4, colour 4, flags 1, 3 reserved) and a
// point's (five f32). A reader takes any size at least as large.
#define FUDE_STROKE_RECORD_SIZE 12u
#define FUDE_POINT_RECORD_SIZE  20u

// The bits of a stroke record's flags byte.
#define FUDE_STROKE_FLAG_FROM_PEN 0x01u
#define FUDE_STROKE_FLAG_MARKER   0x02u   // The marker's; an older build draws it as ink of its colour.

// Writes the page file: VIEW, PAGE, TURN (only when a page is turned), then the alive strokes in STRK and their points
// in PNTS. Returns false when the file could not be written. See the header.
b8 fude_save_document(const c8* _path, const fude_ink* _ink, fude_view _view, fude_page _page, u32* _out_bytes) {
    const u32 _total   = fude_ink_stroke_count(_ink);
    u32       _strokes = 0;
    u32       _points  = 0;
    for(u32 _s = 0; _s < _total; _s++) {
        const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _s);
        if(_stroke->alive && _stroke->point_count > 0) {
            _strokes++;
            _points += _stroke->point_count;
        }
    }

    // Sized up front (+1, because rde_arr grows when an add reaches its capacity), so the file is built without a
    // reallocation. The TURN chunk is not counted, so a page with turned pages may grow the array once.
    fude_bytes _b = fude_bytes_new(FUDE_FILE_HEADER_SIZE + 20u + 9u + (16u + _strokes * FUDE_STROKE_RECORD_SIZE) + (16u + _points * FUDE_POINT_RECORD_SIZE) + 1u);
    fude_put_header(&_b, FUDE_PAGEFILE_VERSION, FUDE_KIND_DOCUMENT);

    u32 _chunk = fude_chunk_begin(&_b, FUDE_CHUNK_VIEW);
    fude_put_f32(&_b, _view.offset.x);
    fude_put_f32(&_b, _view.offset.y);
    fude_put_f32(&_b, _view.zoom);
    fude_chunk_end(&_b, _chunk);

    _chunk = fude_chunk_begin(&_b, FUDE_CHUNK_PAGE);
    fude_put_u8(&_b, (u8)_page.paper);
    fude_chunk_end(&_b, _chunk);
    const u32 _turned = fude_page_turn_count(&_page);
    if(_turned > 0u) {
        const fude_page_turn* _turns = (const fude_page_turn*)_page.turns.memory;
        _chunk = fude_chunk_begin(&_b, FUDE_CHUNK_TURN);
        fude_put_u32(&_b, _turned);
        for(u32 _i = 0; _i < _turned; _i++) {
            fude_put_u16(&_b, _turns[_i].page);
            fude_put_u8(&_b, _turns[_i].quarters);
        }
        fude_chunk_end(&_b, _chunk);
    }

    _chunk = fude_chunk_begin(&_b, FUDE_CHUNK_STROKES);
    fude_put_u32(&_b, _strokes);
    fude_put_u32(&_b, FUDE_STROKE_RECORD_SIZE);
    for(u32 _s = 0; _s < _total; _s++) {
        const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _s);
        if(!_stroke->alive || _stroke->point_count == 0) {
            continue;
        }
        fude_put_u32(&_b, _stroke->point_count);
        fude_put_color(&_b, _stroke->color);
        fude_put_u8(&_b, (u8)((_stroke->from_pen ? FUDE_STROKE_FLAG_FROM_PEN : 0u) | (_stroke->marker ? FUDE_STROKE_FLAG_MARKER : 0u)));
        fude_put_u8(&_b, 0u);
        fude_put_u8(&_b, 0u);
        fude_put_u8(&_b, 0u);
    }
    fude_chunk_end(&_b, _chunk);

    _chunk = fude_chunk_begin(&_b, FUDE_CHUNK_POINTS);
    fude_put_u32(&_b, _points);
    fude_put_u32(&_b, FUDE_POINT_RECORD_SIZE);
    for(u32 _s = 0; _s < _total; _s++) {
        const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _s);
        if(!_stroke->alive || _stroke->point_count == 0) {
            continue;
        }
        const fude_ink_point* _p = fude_ink_stroke_points(_ink, _stroke);
        for(u32 _i = 0; _i < _stroke->point_count; _i++) {
            fude_put_f32(&_b, _p[_i].position.x);
            fude_put_f32(&_b, _p[_i].position.y);
            fude_put_f32(&_b, _p[_i].pressure);
            fude_put_f32(&_b, _p[_i].radius);
            fude_put_f32(&_b, _p[_i].time);
        }
    }
    fude_chunk_end(&_b, _chunk);

    return fude_bytes_write_and_free(&_b, _path, _out_bytes);
}

// A stroke record as read from STRK, before its points are joined to it.
typedef struct {
    u32       point_count;
    rde_color color;
    b8        from_pen;
    b8        marker;
} fude_loaded_stroke;

// Parses the whole page file before touching _ink, _view and _page, so a damaged file changes nothing. Returns false
// when it is damaged: a chunk runs past the end, a stroke has no points, a position is not finite, or the strokes do
// not account for exactly the points there are. A cosmetic field (pressure, radius, time) that is out of range is
// repaired instead, and a bad view or paper is ignored.
RDE_INTERNAL b8 fude_parse_document(const u8* _data, u32 _size, fude_ink* _ink, fude_view* _view, fude_page* _page) {
    fude_reader _r = { .data = _data, .size = _size, .pos = 0, .ok = true };
    if(!fude_read_header(&_r, FUDE_PAGEFILE_VERSION, FUDE_KIND_DOCUMENT)) {
        return false;
    }

    fude_view           _v            = *_view;
    FUDE_PAPER_         _paper        = FUDE_PAPER_DOTS;   // A file from before 'PAGE' has the dots.
    // The pages turned, read into a list of their own: it replaces the page's only once the whole file has read.
    rde_arr TYPE(fude_page_turn) _turn_list = rde_arr_new(sizeof(fude_page_turn), rde_memory_allocator_get_default_std());
    // The strokes and points read, on the standard heap: a page holds as much as the user drew, more than the engine's
    // fixed pool is sized for. Running out of the pool is fatal, whereas the standard heap only fails the load.
    rde_arr TYPE(fude_loaded_stroke) _stroke_list = rde_arr_new(sizeof(fude_loaded_stroke), rde_memory_allocator_get_default_std());
    rde_arr TYPE(fude_ink_point)     _point_list  = rde_arr_new(sizeof(fude_ink_point), rde_memory_allocator_get_default_std());
    fude_loaded_stroke* _strokes      = NULL;
    u32                 _stroke_count = 0;
    fude_ink_point*     _points       = NULL;
    u32                 _point_count  = 0;
    b8                  _have_strokes = false;
    b8                  _have_points  = false;
    b8                  _ok           = true;

    while(_ok && _r.pos < _r.size) {
        const u32 _tag  = fude_get_u32(&_r);
        const u32 _len  = fude_get_u32(&_r);
        if(!_r.ok || _len > _r.size - _r.pos) {
            _ok = false;
            break;
        }

        // Each chunk is read through its own window, so a short or long payload can never run into the next chunk.
        fude_reader _c   = { .data = &_r.data[_r.pos], .size = _len, .pos = 0, .ok = true };
        _r.pos          += _len;

        if(_tag == FUDE_CHUNK_VIEW) {
            const f32 _x = fude_get_f32(&_c);
            const f32 _y = fude_get_f32(&_c);
            const f32 _z = fude_get_f32(&_c);
            if(_c.ok && rde_math_is_finite_f32(_x) && rde_math_is_finite_f32(_y) && rde_math_is_finite_f32(_z) && _z > 0.0f) {
                _v.offset = (rde_vec_2F){ _x, _y };
                _v.zoom   = rde_math_clamp_f32(_z, FUDE_CANVAS_ZOOM_MIN, FUDE_CANVAS_ZOOM_MAX);
            }
        } else if(_tag == FUDE_CHUNK_TURN) {
            // Its count is not trusted: the reader stops at the chunk's end, so a damaged count reads what is there.
            const u32 _count = fude_get_u32(&_c);
            rde_arr_clear(&_turn_list);
            for(u32 _i = 0; _i < _count && _c.ok; _i++) {
                const u16 _page     = fude_get_u16(&_c);
                const u8  _quarters = fude_get_u8(&_c);
                if(_c.ok && (_quarters & 3u) != 0u) {
                    const fude_page_turn _turn = { _page, (u8)(_quarters & 3u) };
                    rde_arr_add(&_turn_list, (any)&_turn);
                }
            }
        } else if(_tag == FUDE_CHUNK_PAGE) {
            const u8 _read = fude_get_u8(&_c);
            if(_c.ok && _read < FUDE_PAPER_COUNT) {
                _paper = (FUDE_PAPER_)_read;
            }
        } else if(_tag == FUDE_CHUNK_STROKES && !_have_strokes) {
            const u32 _count  = fude_get_u32(&_c);
            const u32 _record = fude_get_u32(&_c);
            if(!_c.ok || _record < FUDE_STROKE_RECORD_SIZE || (u64)_count * _record > (u64)(_c.size - _c.pos)) {
                _ok = false;
                break;
            }

            rde_arr_resize(&_stroke_list, _count);
            _strokes = (fude_loaded_stroke*)_stroke_list.memory;
            for(u32 _i = 0; _i < _count; _i++) {
                const u32 _start = _c.pos;
                _strokes[_i].point_count = fude_get_u32(&_c);
                _strokes[_i].color       = fude_theme_saved_color(fude_get_color(&_c));
                const u8 _flags          = fude_get_u8(&_c);
                _strokes[_i].from_pen    = (_flags & FUDE_STROKE_FLAG_FROM_PEN) != 0;
                _strokes[_i].marker      = (_flags & FUDE_STROKE_FLAG_MARKER) != 0;
                _c.pos = _start + _record;   // Skip any fields a newer build added.
                if(_strokes[_i].point_count == 0) {
                    _ok = false;
                }
            }
            _stroke_count = _count;
            _have_strokes = true;
        } else if(_tag == FUDE_CHUNK_POINTS && !_have_points) {
            const u32 _count  = fude_get_u32(&_c);
            const u32 _record = fude_get_u32(&_c);
            if(!_c.ok || _record < FUDE_POINT_RECORD_SIZE || (u64)_count * _record > (u64)(_c.size - _c.pos)) {
                _ok = false;
                break;
            }

            rde_arr_resize(&_point_list, _count);
            _points = (fude_ink_point*)_point_list.memory;
            for(u32 _i = 0; _i < _count; _i++) {
                const u32 _start = _c.pos;
                fude_ink_point* _p = &_points[_i];
                _p->position.x = fude_get_f32(&_c);
                _p->position.y = fude_get_f32(&_c);
                _p->pressure   = fude_get_f32(&_c);
                _p->radius     = fude_get_f32(&_c);
                _p->time       = fude_get_f32(&_c);
                _c.pos = _start + _record;

                if(!rde_math_is_finite_f32(_p->position.x) || !rde_math_is_finite_f32(_p->position.y)) {
                    _ok = false;
                }
                // Cosmetic fields: repaired rather than rejected.
                _p->pressure = rde_math_is_finite_f32(_p->pressure) ? rde_math_clamp_f32(_p->pressure, 0.0f, 1.0f) : 0.0f;
                _p->radius   = (rde_math_is_finite_f32(_p->radius) && _p->radius > 0.0f) ? _p->radius : FUDE_INK_RADIUS_DEFAULT;
                _p->time     = rde_math_is_finite_f32(_p->time) ? _p->time : 0.0f;
            }
            _point_count = _count;
            _have_points = true;
        }
        // Anything else is a chunk from a newer build, and is skipped.
    }

    // The strokes must account for exactly the points there are.
    if(_ok) {
        u64 _sum = 0;
        for(u32 _i = 0; _i < _stroke_count; _i++) {
            _sum += _strokes[_i].point_count;
        }
        _ok = _r.ok && _have_strokes == _have_points && _sum == _point_count;
    }

    if(_ok) {
        u32 _first = 0;
        for(u32 _i = 0; _i < _stroke_count; _i++) {
            fude_ink_add_loaded_stroke_2(_ink, &_points[_first], _strokes[_i].point_count, _strokes[_i].color, _strokes[_i].from_pen, _strokes[_i].marker);
            _first += _strokes[_i].point_count;
        }
        *_view = _v;
        fude_page_reset(_page);   // Frees the page's old list of turns; the revision moves on.
        _page->paper = _paper;
        if(rde_arr_length(&_turn_list) > 0u) {
            _page->turns = _turn_list;   // The page takes the list read.
            _turn_list   = (rde_arr){ 0 };
        }
    }

    if(rde_arr_is_inited(&_turn_list)) {
        rde_arr_free(&_turn_list);
    }
    rde_arr_free(&_stroke_list);
    rde_arr_free(&_point_list);
    return _ok;
}

// Where a page's file is read into (fude_load_document).
typedef struct {
    fude_ink*  ink;
    fude_view* view;
    fude_page* page;
} fude_pagefile_into;

// fude_save_load's reader for a page file: parses it into what _user (a fude_pagefile_into) points to.
RDE_INTERNAL b8 fude_pagefile_read(const u8* _data, u32 _size, any _user) {
    const fude_pagefile_into* _into = (const fude_pagefile_into*)_user;
    return fude_parse_document(_data, _size, _into->ink, _into->view, _into->page);
}

// Loads a page file, or its backup, through fude_save_load. See the header.
FUDE_LOAD_ fude_load_document(const c8* _path, fude_ink* _ink, fude_view* _view, fude_page* _page) {
    fude_pagefile_into _into = { _ink, _view, _page };
    return fude_save_load(_path, fude_pagefile_read, &_into);
}
