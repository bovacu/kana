#include "study/models/history.h"
#include "drawing/base/kfile.h"
#include "drawing/base/save.h"

#include <stdio.h>
#include <string.h>

// ===========================================================================
// See history.h.
// ===========================================================================

#define FUDE_HISTORY_KIND          FUDE_TAG('P', 'R', 'A', 'C')
#define FUDE_HISTORY_CHUNK_SESSION FUDE_TAG('S', 'E', 'S', 'S')
#define FUDE_HISTORY_SQUARE_SIZE   20u     // the fixed part of a square record

// Sanity limits for reading: anything past them is a damaged file, not a drawing.
#define FUDE_HISTORY_MAX_SQUARES   64u
#define FUDE_HISTORY_MAX_STROKES   256u
#define FUDE_HISTORY_MAX_POINTS    100000u

RDE_INTERNAL u32 fude_history_saves = 0;   // see fude_history_revision

const c8* fude_history_dir(void) {
    static c8 _dir[RDE_MAX_PATH] = { 0 };

    if(_dir[0] == 0) {
        snprintf(_dir, sizeof(_dir), "%spractice/", fude_save_dir());
        c8 _probe[RDE_MAX_PATH];
        snprintf(_probe, sizeof(_probe), "%sx.kana", _dir);
        rde_file_create_missing_dirs(_probe);
    }

    return _dir;
}

RDE_INTERNAL void fude_history_path(u32 _codepoint, c8* _out, usize _size) {
    snprintf(_out, _size, "%s%04X.kana", fude_history_dir(), _codepoint);
}

RDE_INTERNAL u16 fude_history_fraction(f32 _v, f32 _size) {
    const f32 _f = _size > 0.0f ? rde_math_clamp_f32(_v / _size, 0.0f, 1.0f) : 0.0f;
    return (u16)(_f * 65535.0f + 0.5f);
}

RDE_INTERNAL void fude_history_put_u64(fude_bytes* _b, u64 _v) {
    fude_put_u32(_b, (u32)(_v & 0xFFFFFFFFu));
    fude_put_u32(_b, (u32)(_v >> 32));
}

RDE_INTERNAL u64 fude_history_get_u64(fude_reader* _r) {
    const u64 _lo = fude_get_u32(_r);
    const u64 _hi = fude_get_u32(_r);
    return _lo | (_hi << 32);
}

b8 fude_history_save(u32 _codepoint, u64 _time, u32 _squares, const fude_score* _scores,
                     const fude_ink* const* _drawings, f32 _square_size) {
    c8 _path[RDE_MAX_PATH];
    fude_history_path(_codepoint, _path, sizeof(_path));

    // What is there already, kept byte for byte: history is only ever added to.
    u32 _old_size = 0;
    u8* _old      = rde_file_exists(_path) ? fude_file_read(_path, &_old_size) : NULL;
    if(_old != NULL) {
        fude_reader _r = fude_reader_make(_old, _old_size);
        if(!fude_read_header(&_r, FUDE_HISTORY_VERSION, FUDE_HISTORY_KIND)) {
            // Damaged: set it aside rather than lose today's session or overwrite it.
            fude_file_free(_old);
            _old = NULL;
            fude_file_set_aside(_path);
        }
    }

    fude_bytes _b = fude_bytes_new(_old != NULL ? _old_size + 4096u : 4096u);
    if(_old != NULL) {
        fude_put_data(&_b, _old, _old_size);
        fude_file_free(_old);
    } else {
        fude_put_header(&_b, FUDE_HISTORY_VERSION, FUDE_HISTORY_KIND);
    }

    // The average over the squares that were scored.
    f32 _sum    = 0.0f;
    u32 _scored = 0;
    for(u32 _i = 0; _i < _squares; _i++) {
        if(!_scores[_i].empty) {
            _sum += _scores[_i].score;
            _scored++;
        }
    }

    const u32 _chunk = fude_chunk_begin(&_b, FUDE_HISTORY_CHUNK_SESSION);
    fude_history_put_u64(&_b, _time);
    fude_put_u32(&_b, _codepoint);
    fude_put_f32(&_b, _scored > 0 ? _sum / (f32)_scored : 0.0f);
    fude_put_u32(&_b, _squares);
    fude_put_u32(&_b, FUDE_HISTORY_SQUARE_SIZE);

    for(u32 _i = 0; _i < _squares; _i++) {
        const fude_score* _s = &_scores[_i];
        fude_put_u8(&_b, _s->empty ? 0u : 1u);
        fude_put_f32(&_b, _s->score);
        fude_put_f32(&_b, _s->shape);
        fude_put_u8(&_b, _s->drawn);
        fude_put_u8(&_b, _s->expected);
        fude_put_u8(&_b, _s->missing);
        fude_put_u8(&_b, _s->extra);
        fude_put_u8(&_b, _s->misplaced);
        fude_put_u8(&_b, _s->reversed);
        fude_put_u8(&_b, _s->first_reversed);
        fude_put_u8(&_b, _s->swap_a);
        fude_put_u8(&_b, _s->swap_b);
        fude_put_u8(&_b, _s->worst);
        fude_put_u8(&_b, 0u);   // reserved: 1 + 4 + 4 + 10 + 1 = the 20-byte record

        // The drawing, relative to its square.
        const fude_ink* _ink   = _drawings[_i];
        u32             _count = 0;
        for(u32 _k = 0; _k < fude_ink_stroke_count(_ink); _k++) {
            _count += fude_ink_stroke_at(_ink, _k)->alive ? 1u : 0u;
        }
        fude_put_u32(&_b, _count);
        for(u32 _k = 0; _k < fude_ink_stroke_count(_ink); _k++) {
            const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _k);
            if(!_stroke->alive) {
                continue;
            }
            const fude_ink_point* _p = fude_ink_stroke_points(_ink, _stroke);
            fude_put_u32(&_b, _stroke->point_count);
            for(u32 _q = 0; _q < _stroke->point_count; _q++) {
                fude_put_u16(&_b, fude_history_fraction(_p[_q].position.x, _square_size));
                fude_put_u16(&_b, fude_history_fraction(_p[_q].position.y, _square_size));
                fude_put_f32(&_b, _p[_q].time);
            }
        }
    }
    fude_chunk_end(&_b, _chunk);

    const b8 _ok = fude_bytes_write_and_free(&_b, _path, NULL);
    fude_history_saves++;
    return _ok;
}

u32 fude_history_revision(void) {
    return fude_history_saves;
}

// "<hex code point>.kana" exactly — not the .bak, .tmp or .bad beside it.
RDE_INTERNAL b8 fude_history_list_entry(const c8* _path, b8 _is_dir, any _user_data) {
    if(_is_dir) {
        return true;
    }

    const c8* _name = _path;
    for(const c8* _c = _path; *_c != 0; _c++) {
        if(*_c == '/' || *_c == '\\') {
            _name = _c + 1;
        }
    }

    u32   _cp     = 0;
    usize _digits = 0;
    for(; _name[_digits] != 0; _digits++) {
        const c8 _h = _name[_digits];
        const i32 _v = (_h >= '0' && _h <= '9') ? _h - '0' : (_h >= 'A' && _h <= 'F') ? _h - 'A' + 10 : (_h >= 'a' && _h <= 'f') ? _h - 'a' + 10 : -1;
        if(_v < 0) {
            break;
        }
        _cp = _cp * 16u + (u32)_v;
    }

    if(_digits >= 4 && _digits <= 6 && strcmp(&_name[_digits], ".kana") == 0) {
        rde_arr_add((rde_arr*)_user_data, &_cp);
    }
    return true;
}

void fude_history_list(rde_arr* _out) {
    rde_file_crawl_dir_recursively(fude_history_dir(), fude_history_list_entry, NULL, 0, _out);
}

void fude_history_init(fude_history* _history) {
    memset(_history, 0, sizeof(*_history));
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _history->sessions = rde_arr_new(sizeof(fude_history_session), _heap);
    _history->squares  = rde_arr_new(sizeof(fude_history_square),  _heap);
    _history->strokes  = rde_arr_new(sizeof(fude_history_stroke),  _heap);
    _history->points   = rde_arr_new(sizeof(fude_history_point),   _heap);
}

void fude_history_destroy(fude_history* _history) {
    rde_arr* _arrays[] = { &_history->sessions, &_history->squares, &_history->strokes, &_history->points };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    memset(_history, 0, sizeof(*_history));
}

// Reads one session's squares into _history; false if the chunk is damaged
// (the caller then drops whatever this session added).
RDE_INTERNAL b8 fude_history_read_squares(fude_history* _history, fude_reader* _c, u32 _squares, u32 _record_size) {
    for(u32 _i = 0; _i < _squares; _i++) {
        const u32           _start = _c->pos;
        fude_history_square _sq;
        memset(&_sq, 0, sizeof(_sq));

        _sq.score.empty          = fude_get_u8(_c) == 0;
        _sq.score.score          = fude_get_f32(_c);
        _sq.score.shape          = fude_get_f32(_c);
        _sq.score.drawn          = fude_get_u8(_c);
        _sq.score.expected       = fude_get_u8(_c);
        _sq.score.missing        = fude_get_u8(_c);
        _sq.score.extra          = fude_get_u8(_c);
        _sq.score.misplaced      = fude_get_u8(_c);
        _sq.score.reversed       = fude_get_u8(_c);
        _sq.score.first_reversed = fude_get_u8(_c);
        _sq.score.swap_a         = fude_get_u8(_c);
        _sq.score.swap_b         = fude_get_u8(_c);
        _sq.score.worst          = fude_get_u8(_c);
        fude_score_describe(&_sq.score);

        // A newer build's longer record: skip what this one does not know.
        if(!fude_reader_has(_c, _start + _record_size - _c->pos)) {
            return false;
        }
        _c->pos = _start + _record_size;

        const u32 _strokes = fude_get_u32(_c);
        if(!_c->ok || _strokes > FUDE_HISTORY_MAX_STROKES) {
            return false;
        }
        _sq.first_stroke = (u32)rde_arr_length(&_history->strokes);
        _sq.stroke_count = _strokes;

        for(u32 _k = 0; _k < _strokes; _k++) {
            const u32 _n = fude_get_u32(_c);
            if(!_c->ok || _n > FUDE_HISTORY_MAX_POINTS || !fude_reader_has(_c, _n * 8u)) {
                return false;
            }
            const fude_history_stroke _stroke = { (u32)rde_arr_length(&_history->points), _n };
            rde_arr_add(&_history->strokes, (any)&_stroke);
            if(_n == 0) {
                continue;
            }

            fude_history_point* _p = (fude_history_point*)rde_arr_add_n(&_history->points, _n);
            for(u32 _q = 0; _q < _n; _q++) {
                _p[_q].x    = fude_get_u16(_c);
                _p[_q].y    = fude_get_u16(_c);
                _p[_q].time = fude_get_f32(_c);
            }
        }

        rde_arr_add(&_history->squares, &_sq);
    }
    return _c->ok;
}

b8 fude_history_load(fude_history* _history, u32 _codepoint) {
    rde_arr_clear(&_history->sessions);
    rde_arr_clear(&_history->squares);
    rde_arr_clear(&_history->strokes);
    rde_arr_clear(&_history->points);
    _history->codepoint = _codepoint;

    c8 _path[RDE_MAX_PATH];
    fude_history_path(_codepoint, _path, sizeof(_path));
    if(!rde_file_exists(_path)) {
        return false;
    }

    u32 _size = 0;
    u8* _data = fude_file_read(_path, &_size);
    fude_reader _r = fude_reader_make(_data, _size);
    if(_data == NULL || !fude_read_header(&_r, FUDE_HISTORY_VERSION, FUDE_HISTORY_KIND)) {
        fude_file_free(_data);
        return false;
    }

    u32         _tag;
    fude_reader _chunk;
    while(fude_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag != FUDE_HISTORY_CHUNK_SESSION) {
            continue;
        }

        fude_history_session _session;
        _session.time    = fude_history_get_u64(&_chunk);
        const u32 _cp    = fude_get_u32(&_chunk);
        _session.average = fude_get_f32(&_chunk);
        const u32 _count = fude_get_u32(&_chunk);
        const u32 _rec   = fude_get_u32(&_chunk);
        if(!_chunk.ok || _cp != _codepoint || _count > FUDE_HISTORY_MAX_SQUARES || _rec < FUDE_HISTORY_SQUARE_SIZE) {
            continue;
        }

        // A damaged session is dropped whole: roll back what it added.
        const u32 _squares0 = _history->squares.count;
        const u32 _strokes0 = _history->strokes.count;
        const u32 _points0  = _history->points.count;
        _session.first_square = _squares0;
        _session.square_count = _count;

        if(fude_history_read_squares(_history, &_chunk, _count, _rec)) {
            rde_arr_add(&_history->sessions, &_session);
        } else {
            _history->squares.count = _squares0;
            _history->strokes.count = _strokes0;
            _history->points.count  = _points0;
        }
    }

    fude_file_free(_data);
    return rde_arr_length(&_history->sessions) > 0;
}

b8 fude_history_summarize(u32 _codepoint, fude_history_summary* _out) {
    memset(_out, 0, sizeof(*_out));

    c8 _path[RDE_MAX_PATH];
    fude_history_path(_codepoint, _path, sizeof(_path));
    if(!rde_file_exists(_path)) {
        return false;
    }

    u32 _size = 0;
    u8* _data = fude_file_read(_path, &_size);
    fude_reader _r = fude_reader_make(_data, _size);
    if(_data == NULL || !fude_read_header(&_r, FUDE_HISTORY_VERSION, FUDE_HISTORY_KIND)) {
        fude_file_free(_data);
        return false;
    }

    u32         _tag;
    fude_reader _chunk;
    while(fude_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag != FUDE_HISTORY_CHUNK_SESSION) {
            continue;
        }
        const u64 _time    = fude_history_get_u64(&_chunk);
        const u32 _cp      = fude_get_u32(&_chunk);
        const f32 _average = fude_get_f32(&_chunk);
        if(!_chunk.ok || _cp != _codepoint) {
            continue;
        }

        if(_out->sessions == 0) {
            _out->first = _average;
        }
        _out->sessions++;
        _out->last      = _average;
        _out->last_time = _time;
        _out->best      = _average > _out->best ? _average : _out->best;
    }

    fude_file_free(_data);
    return _out->sessions > 0;
}
