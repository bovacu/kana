#include "study/history.h"
#include "base/kfile.h"
#include "base/save.h"

#include <stdio.h>
#include <string.h>

// ===========================================================================
// See history.h.
// ===========================================================================

#define KANA_HISTORY_KIND          KANA_TAG('P', 'R', 'A', 'C')
#define KANA_HISTORY_CHUNK_SESSION KANA_TAG('S', 'E', 'S', 'S')
#define KANA_HISTORY_SQUARE_SIZE   20u     // the fixed part of a square record

// Sanity limits for reading: anything past them is a damaged file, not a drawing.
#define KANA_HISTORY_MAX_SQUARES   64u
#define KANA_HISTORY_MAX_STROKES   256u
#define KANA_HISTORY_MAX_POINTS    100000u

RDE_INTERNAL u32 kana_history_saves = 0;   // see kana_history_revision

const c8* kana_history_dir(void) {
    static c8 _dir[RDE_MAX_PATH] = { 0 };

    if(_dir[0] == 0) {
        snprintf(_dir, sizeof(_dir), "%spractice/", kana_save_dir());
        c8 _probe[RDE_MAX_PATH];
        snprintf(_probe, sizeof(_probe), "%sx.kana", _dir);
        rde_file_create_missing_dirs(_probe);
    }

    return _dir;
}

RDE_INTERNAL void kana_history_path(u32 _codepoint, c8* _out, usize _size) {
    snprintf(_out, _size, "%s%04X.kana", kana_history_dir(), _codepoint);
}

RDE_INTERNAL u16 kana_history_fraction(f32 _v, f32 _size) {
    const f32 _f = _size > 0.0f ? rde_math_clamp_f32(_v / _size, 0.0f, 1.0f) : 0.0f;
    return (u16)(_f * 65535.0f + 0.5f);
}

RDE_INTERNAL void kana_history_put_u64(kana_bytes* _b, u64 _v) {
    kana_put_u32(_b, (u32)(_v & 0xFFFFFFFFu));
    kana_put_u32(_b, (u32)(_v >> 32));
}

RDE_INTERNAL u64 kana_history_get_u64(kana_reader* _r) {
    const u64 _lo = kana_get_u32(_r);
    const u64 _hi = kana_get_u32(_r);
    return _lo | (_hi << 32);
}

b8 kana_history_save(u32 _codepoint, u64 _time, u32 _squares, const kana_score* _scores,
                     const kana_ink* const* _drawings, f32 _square_size) {
    c8 _path[RDE_MAX_PATH];
    kana_history_path(_codepoint, _path, sizeof(_path));

    // What is there already, kept byte for byte: history is only ever added to.
    u32 _old_size = 0;
    u8* _old      = rde_file_exists(_path) ? kana_file_read(_path, &_old_size) : NULL;
    if(_old != NULL) {
        kana_reader _r = kana_reader_make(_old, _old_size);
        if(!kana_read_header(&_r, KANA_HISTORY_VERSION, KANA_HISTORY_KIND)) {
            // Damaged: set it aside rather than lose today's session or overwrite it.
            kana_file_free(_old);
            _old = NULL;
            kana_file_set_aside(_path);
        }
    }

    kana_bytes _b = kana_bytes_new(_old != NULL ? _old_size + 4096u : 4096u);
    if(_old != NULL) {
        kana_put_data(&_b, _old, _old_size);
        kana_file_free(_old);
    } else {
        kana_put_header(&_b, KANA_HISTORY_VERSION, KANA_HISTORY_KIND);
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

    const u32 _chunk = kana_chunk_begin(&_b, KANA_HISTORY_CHUNK_SESSION);
    kana_history_put_u64(&_b, _time);
    kana_put_u32(&_b, _codepoint);
    kana_put_f32(&_b, _scored > 0 ? _sum / (f32)_scored : 0.0f);
    kana_put_u32(&_b, _squares);
    kana_put_u32(&_b, KANA_HISTORY_SQUARE_SIZE);

    for(u32 _i = 0; _i < _squares; _i++) {
        const kana_score* _s = &_scores[_i];
        kana_put_u8(&_b, _s->empty ? 0u : 1u);
        kana_put_f32(&_b, _s->score);
        kana_put_f32(&_b, _s->shape);
        kana_put_u8(&_b, _s->drawn);
        kana_put_u8(&_b, _s->expected);
        kana_put_u8(&_b, _s->missing);
        kana_put_u8(&_b, _s->extra);
        kana_put_u8(&_b, _s->misplaced);
        kana_put_u8(&_b, _s->reversed);
        kana_put_u8(&_b, _s->first_reversed);
        kana_put_u8(&_b, _s->swap_a);
        kana_put_u8(&_b, _s->swap_b);
        kana_put_u8(&_b, _s->worst);
        kana_put_u8(&_b, 0u);   // reserved: 1 + 4 + 4 + 10 + 1 = the 20-byte record

        // The drawing, relative to its square.
        const kana_ink* _ink   = _drawings[_i];
        u32             _count = 0;
        for(u32 _k = 0; _k < kana_ink_stroke_count(_ink); _k++) {
            _count += kana_ink_stroke_at(_ink, _k)->alive ? 1u : 0u;
        }
        kana_put_u32(&_b, _count);
        for(u32 _k = 0; _k < kana_ink_stroke_count(_ink); _k++) {
            const kana_ink_stroke* _stroke = kana_ink_stroke_at(_ink, _k);
            if(!_stroke->alive) {
                continue;
            }
            const kana_ink_point* _p = kana_ink_stroke_points(_ink, _stroke);
            kana_put_u32(&_b, _stroke->point_count);
            for(u32 _q = 0; _q < _stroke->point_count; _q++) {
                kana_put_u16(&_b, kana_history_fraction(_p[_q].position.x, _square_size));
                kana_put_u16(&_b, kana_history_fraction(_p[_q].position.y, _square_size));
                kana_put_f32(&_b, _p[_q].time);
            }
        }
    }
    kana_chunk_end(&_b, _chunk);

    const b8 _ok = kana_bytes_write_and_free(&_b, _path, NULL);
    kana_history_saves++;
    return _ok;
}

u32 kana_history_revision(void) {
    return kana_history_saves;
}

// "<hex code point>.kana" exactly — not the .bak, .tmp or .bad beside it.
RDE_INTERNAL b8 kana_history_list_entry(const c8* _path, b8 _is_dir, any _user_data) {
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

void kana_history_list(rde_arr* _out) {
    rde_file_crawl_dir_recursively(kana_history_dir(), kana_history_list_entry, NULL, 0, _out);
}

void kana_history_init(kana_history* _history) {
    memset(_history, 0, sizeof(*_history));
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _history->sessions = rde_arr_new(sizeof(kana_history_session), _heap);
    _history->squares  = rde_arr_new(sizeof(kana_history_square),  _heap);
    _history->strokes  = rde_arr_new(sizeof(kana_history_stroke),  _heap);
    _history->points   = rde_arr_new(sizeof(kana_history_point),   _heap);
}

void kana_history_destroy(kana_history* _history) {
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
RDE_INTERNAL b8 kana_history_read_squares(kana_history* _history, kana_reader* _c, u32 _squares, u32 _record_size) {
    for(u32 _i = 0; _i < _squares; _i++) {
        const u32           _start = _c->pos;
        kana_history_square _sq;
        memset(&_sq, 0, sizeof(_sq));

        _sq.score.empty          = kana_get_u8(_c) == 0;
        _sq.score.score          = kana_get_f32(_c);
        _sq.score.shape          = kana_get_f32(_c);
        _sq.score.drawn          = kana_get_u8(_c);
        _sq.score.expected       = kana_get_u8(_c);
        _sq.score.missing        = kana_get_u8(_c);
        _sq.score.extra          = kana_get_u8(_c);
        _sq.score.misplaced      = kana_get_u8(_c);
        _sq.score.reversed       = kana_get_u8(_c);
        _sq.score.first_reversed = kana_get_u8(_c);
        _sq.score.swap_a         = kana_get_u8(_c);
        _sq.score.swap_b         = kana_get_u8(_c);
        _sq.score.worst          = kana_get_u8(_c);
        kana_score_describe(&_sq.score);

        // A newer build's longer record: skip what this one does not know.
        if(!kana_reader_has(_c, _start + _record_size - _c->pos)) {
            return false;
        }
        _c->pos = _start + _record_size;

        const u32 _strokes = kana_get_u32(_c);
        if(!_c->ok || _strokes > KANA_HISTORY_MAX_STROKES) {
            return false;
        }
        _sq.first_stroke = (u32)rde_arr_length(&_history->strokes);
        _sq.stroke_count = _strokes;

        for(u32 _k = 0; _k < _strokes; _k++) {
            const u32 _n = kana_get_u32(_c);
            if(!_c->ok || _n > KANA_HISTORY_MAX_POINTS || !kana_reader_has(_c, _n * 8u)) {
                return false;
            }
            const kana_history_stroke _stroke = { (u32)rde_arr_length(&_history->points), _n };
            rde_arr_add(&_history->strokes, (any)&_stroke);
            if(_n == 0) {
                continue;
            }

            kana_history_point* _p = (kana_history_point*)rde_arr_add_n(&_history->points, _n);
            for(u32 _q = 0; _q < _n; _q++) {
                _p[_q].x    = kana_get_u16(_c);
                _p[_q].y    = kana_get_u16(_c);
                _p[_q].time = kana_get_f32(_c);
            }
        }

        rde_arr_add(&_history->squares, &_sq);
    }
    return _c->ok;
}

b8 kana_history_load(kana_history* _history, u32 _codepoint) {
    rde_arr_clear(&_history->sessions);
    rde_arr_clear(&_history->squares);
    rde_arr_clear(&_history->strokes);
    rde_arr_clear(&_history->points);
    _history->codepoint = _codepoint;

    c8 _path[RDE_MAX_PATH];
    kana_history_path(_codepoint, _path, sizeof(_path));
    if(!rde_file_exists(_path)) {
        return false;
    }

    u32 _size = 0;
    u8* _data = kana_file_read(_path, &_size);
    kana_reader _r = kana_reader_make(_data, _size);
    if(_data == NULL || !kana_read_header(&_r, KANA_HISTORY_VERSION, KANA_HISTORY_KIND)) {
        kana_file_free(_data);
        return false;
    }

    u32         _tag;
    kana_reader _chunk;
    while(kana_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag != KANA_HISTORY_CHUNK_SESSION) {
            continue;
        }

        kana_history_session _session;
        _session.time    = kana_history_get_u64(&_chunk);
        const u32 _cp    = kana_get_u32(&_chunk);
        _session.average = kana_get_f32(&_chunk);
        const u32 _count = kana_get_u32(&_chunk);
        const u32 _rec   = kana_get_u32(&_chunk);
        if(!_chunk.ok || _cp != _codepoint || _count > KANA_HISTORY_MAX_SQUARES || _rec < KANA_HISTORY_SQUARE_SIZE) {
            continue;
        }

        // A damaged session is dropped whole: roll back what it added.
        const u32 _squares0 = _history->squares.count;
        const u32 _strokes0 = _history->strokes.count;
        const u32 _points0  = _history->points.count;
        _session.first_square = _squares0;
        _session.square_count = _count;

        if(kana_history_read_squares(_history, &_chunk, _count, _rec)) {
            rde_arr_add(&_history->sessions, &_session);
        } else {
            _history->squares.count = _squares0;
            _history->strokes.count = _strokes0;
            _history->points.count  = _points0;
        }
    }

    kana_file_free(_data);
    return rde_arr_length(&_history->sessions) > 0;
}

b8 kana_history_summarize(u32 _codepoint, kana_history_summary* _out) {
    memset(_out, 0, sizeof(*_out));

    c8 _path[RDE_MAX_PATH];
    kana_history_path(_codepoint, _path, sizeof(_path));
    if(!rde_file_exists(_path)) {
        return false;
    }

    u32 _size = 0;
    u8* _data = kana_file_read(_path, &_size);
    kana_reader _r = kana_reader_make(_data, _size);
    if(_data == NULL || !kana_read_header(&_r, KANA_HISTORY_VERSION, KANA_HISTORY_KIND)) {
        kana_file_free(_data);
        return false;
    }

    u32         _tag;
    kana_reader _chunk;
    while(kana_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag != KANA_HISTORY_CHUNK_SESSION) {
            continue;
        }
        const u64 _time    = kana_history_get_u64(&_chunk);
        const u32 _cp      = kana_get_u32(&_chunk);
        const f32 _average = kana_get_f32(&_chunk);
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

    kana_file_free(_data);
    return _out->sessions > 0;
}
